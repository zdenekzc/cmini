/* curses8.c - 8-bit ncurses subset for ToaruOS, written for cmini (-L /lib).
 *
 * Covers the functions tiny-ide uses (see the undefined symbols of its objects).
 * One screen (stdscr); cells hold a byte plus attributes (A_BOLD, colour pair in
 * bits 8-15, as in ownheaders/ncurses.h).  Output: ANSI/xterm sequences, which the
 * ToaruOS terminal understands; refresh() sends only the cells that changed.
 * Input: bytes from fd 0; ESC sequences are decoded with a table (define_key adds
 * entries), ESCDELAY ms are waited for the rest of a sequence.
 * No wide characters, no locale, no varargs. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <poll.h>
#include <ncurses.h>

#define OBUF_MAX 65536
#define TABLE_MAX 256
#define PAIRS 256
#define PENDING_MAX 64

struct _win_st { int unused; };
static struct _win_st stdscr_object;

WINDOW *stdscr = NULL;          /* set by initscr */
int LINES = 24;
int COLS = 80;
int ESCDELAY = 50;

static int rows, cols;
static chtype *cells;          /* what the program wants on the screen */
static chtype *shown;          /* what the terminal shows now (refresh compares) */
static int cur_y, cur_x;
static chtype cur_attr;
static chtype bkgd;
static int initialised;
static int raw_active;
static struct termios saved_tio;
static int saved_ok;
static int timeout_ms = -1;

static char obuf[OBUF_MAX];
static int olen;

static short pair_fg[PAIRS];
static short pair_bg[PAIRS];

/* key table: sequence => code */
static char *seq_text[TABLE_MAX];
static int seq_code[TABLE_MAX];
static int seq_count;

static unsigned char pending[PENDING_MAX];
static int pend_head, pend_count;

/* ------------------------------------------------------------ output */

static void out_flush(void)
{
    int done = 0;
    while (done < olen) {
        int n = (int) write(1, obuf + done, (unsigned long) (olen - done));
        if (n <= 0) break;
        done += n;
    }
    olen = 0;
}

static void out_byte(char c)
{
    if (olen >= OBUF_MAX) out_flush();
    obuf[olen++] = c;
}

static void out_str(const char *s)
{
    while (*s) out_byte(*s++);
}

static void out_num(int v)
{
    char b[16];
    snprintf(b, sizeof(b), "%d", v);
    out_str(b);
}

static void out_cup(int y, int x)
{
    out_str("\033[");
    out_num(y + 1);
    out_byte(';');
    out_num(x + 1);
    out_byte('H');
}

/* SGR for one attribute word: reset, bold, then the colour pair */
static void out_sgr(chtype a)
{
    int pair = (int) ((a >> 8) & 0xFF);
    out_str("\033[0");
    if (a & A_BOLD) out_str(";1");
    if (pair != 0) {
        out_str(";3");
        out_num(pair_fg[pair] & 7);
        out_str(";4");
        out_num(pair_bg[pair] & 7);
    }
    out_byte('m');
}

/* ------------------------------------------------------------ terminal size and mode */

static void screen_size(void)
{
    winsize_t ws;
    rows = 24;
    cols = 80;
    if (ioctl(1, TIOCGWINSZ, &ws) == 0 && ws.ws_row > 1 && ws.ws_col > 1) {
        rows = ws.ws_row;
        cols = ws.ws_col;
    }
    LINES = rows;
    COLS = cols;
}

static void term_raw(void)
{
    termios_t t;
    int i;
    if (!saved_ok) {
        if (tcgetattr(0, &saved_tio) != 0) return;
        saved_ok = 1;
    }
    t = saved_tio;
    t.c_iflag &= ~(IXON | IXOFF | IXANY | INLCR | ICRNL | ISTRIP | BRKINT | IGNCR);
    t.c_oflag &= ~OPOST;
    t.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
    t.c_cflag &= ~(CSIZE | PARENB);
    t.c_cflag |= CS8;
    for (i = 0; i < NCCS; i++) t.c_cc[i] = 0;
    t.c_cc[VMIN] = 1;
    t.c_cc[VTIME] = 0;
    tcsetattr(0, TCSANOW, &t);
    raw_active = 1;
}

static void term_restore(void)
{
    if (saved_ok) tcsetattr(0, TCSANOW, &saved_tio);
    raw_active = 0;
}

/* ------------------------------------------------------------ key table */

static void table_add(const char *text, int code)
{
    int i;
    for (i = 0; i < seq_count; i++) {
        if (strcmp(seq_text[i], text) == 0) { seq_code[i] = code; return; }
    }
    if (seq_count >= TABLE_MAX) return;
    seq_text[seq_count] = strdup(text);
    seq_code[seq_count] = code;
    seq_count++;
}

static void table_defaults(void)
{
    table_add("\033[A", KEY_UP);
    table_add("\033[B", KEY_DOWN);
    table_add("\033[C", KEY_RIGHT);
    table_add("\033[D", KEY_LEFT);
    table_add("\033OA", KEY_UP);
    table_add("\033OB", KEY_DOWN);
    table_add("\033OC", KEY_RIGHT);
    table_add("\033OD", KEY_LEFT);
    table_add("\033OH", KEY_HOME);
    table_add("\033OF", KEY_END);
    table_add("\033[H", KEY_HOME);
    table_add("\033[F", KEY_END);
    table_add("\033[1~", KEY_HOME);
    table_add("\033[4~", KEY_END);
    table_add("\033[2~", KEY_IC);
    table_add("\033[3~", KEY_DC);
    table_add("\033[5~", KEY_PPAGE);
    table_add("\033[6~", KEY_NPAGE);
    table_add("\033[Z", KEY_BTAB);
    table_add("\033OP", KEY_F(1));
    table_add("\033OQ", KEY_F(2));
    table_add("\033OR", KEY_F(3));
    table_add("\033OS", KEY_F(4));
    table_add("\033[11~", KEY_F(1));
    table_add("\033[12~", KEY_F(2));
    table_add("\033[13~", KEY_F(3));
    table_add("\033[14~", KEY_F(4));
    table_add("\033[15~", KEY_F(5));
    table_add("\033[17~", KEY_F(6));
    table_add("\033[18~", KEY_F(7));
    table_add("\033[19~", KEY_F(8));
    table_add("\033[20~", KEY_F(9));
    table_add("\033[21~", KEY_F(10));
    table_add("\033[23~", KEY_F(11));
    table_add("\033[24~", KEY_F(12));
}

/* ------------------------------------------------------------ input */

/* next byte: from the pushed-back queue, else from fd 0 within ms (-1 = wait; 0 = no wait) */
static int next_byte(int ms)
{
    pollfd_t p;
    unsigned char c;
    if (pend_count > 0) {
        c = pending[pend_head];
        pend_head = (pend_head + 1) % PENDING_MAX;
        pend_count--;
        return c;
    }
    p.fd = 0;
    p.events = POLLIN;
    p.revents = 0;
    if (poll(&p, 1, ms) <= 0) return -1;
    if (read(0, &c, 1) != 1) return -1;
    return c;
}

static void unread_byte(int c)
{
    int tail;
    if (pend_count >= PENDING_MAX) return;
    tail = (pend_head + pend_count) % PENDING_MAX;
    pending[tail] = (unsigned char) c;
    pend_count++;
}

static int is_prefix_of_table(const char *buf, int len)
{
    int i;
    for (i = 0; i < seq_count; i++) {
        if ((int) strlen(seq_text[i]) > len && strncmp(seq_text[i], buf, len) == 0) return 1;
    }
    return 0;
}

static int getch_impl(void)
{
    char buf[16];
    int len = 0;
    int c, i;

    c = next_byte(timeout_ms);
    if (c < 0) return ERR;
    if (c != 27) return c;

    buf[len++] = 27;
    while (len < 15) {
        int exact = -1;
        int more;
        for (i = 0; i < seq_count; i++) {
            if ((int) strlen(seq_text[i]) == len && strncmp(seq_text[i], buf, len) == 0) { exact = i; break; }
        }
        more = is_prefix_of_table(buf, len);
        if (exact >= 0 && !more) return seq_code[exact];
        c = next_byte(more ? ESCDELAY : ESCDELAY);
        if (c < 0) break;
        buf[len++] = (char) c;
    }
    /* no table entry: ESC on its own, the other bytes are read again as keys */
    for (i = len - 1; i >= 1; i--) unread_byte((unsigned char) buf[i]);
    return 27;
}

/* ------------------------------------------------------------ screen */

static void put_cell(int y, int x, chtype ch)
{
    if (y < 0 || x < 0 || y >= rows || x >= cols) return;
    cells[y * cols + x] = ch;
}

static chtype make_cell(chtype ch)
{
    chtype attr = (ch & ~(chtype) 0xFF) | (cur_attr & ~(chtype) 0xFF);
    return attr | (ch & 0xFF);
}

int initscr(void)
{
    int n;
    stdscr = (WINDOW *) &stdscr_object;
    if (!initialised) {
        screen_size();
        n = rows * cols;
        cells = (chtype *) calloc((unsigned long) n, sizeof(chtype));
        shown = (chtype *) calloc((unsigned long) n, sizeof(chtype));
        if (cells == NULL || shown == NULL) { fprintf(stderr, "initscr: out of memory\n"); exit(1); }
        table_defaults();
        for (n = 0; n < PAIRS; n++) { pair_fg[n] = 7; pair_bg[n] = 0; }
        initialised = 1;
    }
    for (n = 0; n < rows * cols; n++) { cells[n] = bkgd | ' '; shown[n] = (chtype) -1; }
    cur_y = 0;
    cur_x = 0;
    term_raw();
    out_str("\033[?1049h\033[0m\033[2J\033[H");
    out_flush();
    return 0;
}

int endwin(void)
{
    out_str("\033[0m\033[2J\033[H\033[?25h");
    out_flush();
    term_restore();
    return 0;
}

int refresh(void)
{
    int y, x, n;
    chtype last = (chtype) -1;
    int ly = -1, lx = -1;
    for (y = 0; y < rows; y++) {
        for (x = 0; x < cols; x++) {
            n = y * cols + x;
            if (cells[n] == shown[n]) continue;
            if (ly != y || lx != x) out_cup(y, x);
            if ((cells[n] & ~(chtype) 0xFF) != last) {
                out_sgr(cells[n]);
                last = cells[n] & ~(chtype) 0xFF;
            }
            {
                unsigned char c = (unsigned char) (cells[n] & 0xFF);
                out_byte(c >= 32 && c < 127 ? (char) c : ' ');
            }
            shown[n] = cells[n];
            ly = y;
            lx = x + 1;
            if (lx >= cols) { ly = -1; }
        }
    }
    out_str("\033[0m");
    out_cup(cur_y, cur_x);
    out_flush();
    return 0;
}

int redrawwin(WINDOW *win)
{
    int n;
    (void) win;
    out_str("\033[0m\033[2J");
    for (n = 0; n < rows * cols; n++) shown[n] = (chtype) -1;
    return refresh();
}

int move(int y, int x)
{
    if (y < 0 || x < 0 || y >= rows || x >= cols) return ERR;
    cur_y = y;
    cur_x = x;
    return OK;
}

int attrset(int attrs)
{
    cur_attr = (chtype) attrs;
    return OK;
}

int wbkgd(WINDOW *win, chtype ch)
{
    int n;
    (void) win;
    bkgd = ch & ~(chtype) 0xFF;
    for (n = 0; n < rows * cols; n++) {
        if ((cells[n] & 0xFF) == ' ') cells[n] = bkgd | ' ';
    }
    return OK;
}

int erase(void)
{
    int n;
    for (n = 0; n < rows * cols; n++) cells[n] = bkgd | ' ';
    return OK;
}

int clrtoeol(void)
{
    int x;
    for (x = cur_x; x < cols; x++) put_cell(cur_y, x, bkgd | ' ');
    return OK;
}

int mvaddch(int y, int x, chtype ch)
{
    move(y, x);
    put_cell(y, x, make_cell(ch));
    return OK;
}

int mvwaddch(WINDOW *win, int y, int x, chtype ch)
{
    (void) win;
    return mvaddch(y, x, ch);
}

int mvaddstr(int y, int x, const char *s)
{
    while (*s) {
        put_cell(y, x, make_cell((chtype) (unsigned char) *s));
        x++;
        s++;
    }
    return OK;
}

int mvhline(int y, int x, chtype ch, int n)
{
    int i;
    for (i = 0; i < n; i++) put_cell(y, x + i, make_cell(ch));
    return OK;
}

int mvwhline(WINDOW *win, int y, int x, chtype ch, int n)
{
    (void) win;
    return mvhline(y, x, ch, n);
}

int init_pair(short pair, short fg, short bg)
{
    if (pair < 0 || pair >= PAIRS) return ERR;
    pair_fg[pair] = fg;
    pair_bg[pair] = bg;
    return OK;
}

int start_color(void)
{
    return OK;
}

int raw(void)
{
    if (!raw_active) term_raw();
    return OK;
}

int noecho(void)
{
    return OK;
}

int nonl(void)
{
    return OK;
}

int keypad(WINDOW *win, bool on)
{
    (void) win;
    (void) on;
    return OK;
}

int curs_set(int visibility)
{
    out_str(visibility ? "\033[?25h" : "\033[?25l");
    out_flush();
    return visibility ? 1 : 0;
}

int beep(void)
{
    out_byte('\a');
    out_flush();
    return OK;
}

int timeout(int ms)
{
    timeout_ms = ms;
    return OK;
}

int wtimeout(WINDOW *win, int ms)
{
    (void) win;
    timeout_ms = ms;
    return OK;
}

int getch(void)
{
    return getch_impl();
}

int define_key(const char *seq, int code)
{
    if (seq == NULL) return ERR;
    table_add(seq, code);
    return OK;
}

/* no mouse in the ToaruOS terminal path used here */
int getmouse(MEVENT *event)
{
    (void) event;
    return ERR;
}

mmask_t mousemask(mmask_t newmask, mmask_t *oldmask)
{
    (void) newmask;
    if (oldmask != NULL) *oldmask = 0;
    return 0;
}

int mouseinterval(int ms)
{
    (void) ms;
    return 0;
}
