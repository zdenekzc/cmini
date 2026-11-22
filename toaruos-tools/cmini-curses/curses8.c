/* curses8.c - 8-bit ncurses subset for ToaruOS, compiled by cmini (-L /lib).
 *
 * Windows with their own cells; virtual screen (newscr) filled by wnoutrefresh,
 * changes to the terminal (curscr) by doupdate.  Output: ANSI/xterm sequences
 * (the ToaruOS terminal understands them).  Input: bytes from fd 0, ESC sequences
 * decoded with a table (define_key adds entries), ESCDELAY ms for the rest.
 * No wide characters, no locale, no varargs (mvwprintw is a macro in ncurses.h). */

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
#define FORMAT_MAX 2048

struct _win_st {
    int begy, begx;          /* position on the screen */
    int maxy, maxx;          /* size */
    int cury, curx;          /* cursor inside the window */
    chtype attr;             /* current attributes (waddch uses them) */
    chtype bkg;              /* background character and attributes */
    int touched;             /* changed since the last wnoutrefresh */
    chtype *cells;
};

typedef struct _win_st win_t;

struct _screen_st {
    int dummy;
};

static struct _screen_st screen_object;

WINDOW *stdscr = NULL;
WINDOW *newscr = NULL;
WINDOW *curscr = NULL;
int LINES = 24;
int COLS = 80;
int ESCDELAY = 50;

static int rows, cols;
static int cursor_y, cursor_x;     /* absolute position of the terminal cursor */
static int cursor_visible = 1;
static int initialised;
static int raw_active;
static int timeout_ms = -1;
/* nl() mode (the ncurses default): Enter = CR from the raw terminal is returned as NL;
 * mc compares with '\n'. nonl() (tiny-ide) returns CR unchanged. */
static int nl_mode = 1;
static int saved_ok;
static struct termios saved_tio;

static char obuf[OBUF_MAX];
static int olen;
static char format_buf[FORMAT_MAX];

static short pair_fg[PAIRS];
static short pair_bg[PAIRS];

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

/* SGR for a cell attribute word: reset, bold/reverse/underline, colour pair */
static void out_sgr(chtype a)
{
    int pair = PAIR_NUMBER(a);
    out_str("\033[0");
    if (a & A_BOLD) out_str(";1");
    if (a & A_UNDERLINE) out_str(";4");
    if (a & A_REVERSE) out_str(";7");
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

/* ------------------------------------------------------------ windows */

static WINDOW *win_alloc(int lines, int cols_, int begy, int begx)
{
    WINDOW *w = (WINDOW *) calloc(1, sizeof(win_t));
    int n;
    if (w == NULL) return NULL;
    w->maxy = lines;
    w->maxx = cols_;
    w->begy = begy;
    w->begx = begx;
    w->bkg = ' ';
    n = lines * cols_;
    w->cells = (chtype *) calloc((unsigned long) (n > 0 ? n : 1), sizeof(chtype));
    if (w->cells == NULL) { free(w); return NULL; }
    for (n = 0; n < lines * cols_; n++) w->cells[n] = ' ';
    return w;
}

static void cell_set(WINDOW *w, int y, int x, chtype ch)
{
    if (y < 0 || x < 0 || y >= w->maxy || x >= w->maxx) return;
    w->cells[y * w->maxx + x] = ch;
    w->touched = 1;
}

/* a character with the window's current attributes (and the caller's attributes) */
/* cell = character + attributes; the colour pair of the character wins, then the pair
 * of the window attributes, then the pair of the window background (wbkgd) */
static chtype cell_value(WINDOW *w, chtype ch)
{
    chtype attr = (ch & ~(chtype) (A_CHARTEXT | A_COLOR)) | (w->attr & ~(chtype) (A_CHARTEXT | A_COLOR));
    chtype color = ch & A_COLOR;
    if (color == 0) color = w->attr & A_COLOR;
    if (color == 0) color = w->bkg & A_COLOR;
    return attr | color | (ch & A_CHARTEXT);
}

static void apply_key_table(void);

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

/* next byte from the pushed-back queue, else from fd 0 (ms: -1 = wait, 0 = no wait) */
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
    if (ms < 0) {
        /* wait without a limit: poll in short steps (ToaruOS poll does not wait for -1) */
        while (poll(&p, 1, 100) <= 0) {
            p.revents = 0;
        }
    } else if (poll(&p, 1, ms) <= 0) {
        return -1;
    }
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

static int table_prefix(const char *buf, int len)
{
    int i;
    for (i = 0; i < seq_count; i++) {
        if ((int) strlen(seq_text[i]) > len && strncmp(seq_text[i], buf, len) == 0) return 1;
    }
    return 0;
}

static int getch_impl(int ms)
{
    char buf[16];
    int len = 0;
    int c, i;

    c = next_byte(ms);
    if (c < 0) return ERR;
    if (c == '\r' && nl_mode) return '\n';
    if (c != 27) return c;

    buf[len++] = 27;
    while (len < 15) {
        int exact = -1;
        int more;
        for (i = 0; i < seq_count; i++) {
            if ((int) strlen(seq_text[i]) == len && strncmp(seq_text[i], buf, len) == 0) { exact = i; break; }
        }
        more = table_prefix(buf, len);
        if (exact >= 0 && !more) return seq_code[exact];
        c = next_byte(ESCDELAY);
        if (c < 0) break;
        buf[len++] = (char) c;
    }
    /* no table entry: ESC on its own; the other bytes are read again as keys */
    for (i = len - 1; i >= 1; i--) unread_byte((unsigned char) buf[i]);
    return 27;
}

/* ------------------------------------------------------------ screen */

int initscr(void)
{
    int n;
    if (!initialised) {
        screen_size();
        stdscr = win_alloc(rows, cols, 0, 0);
        newscr = win_alloc(rows, cols, 0, 0);
        curscr = win_alloc(rows, cols, 0, 0);
        if (stdscr == NULL || newscr == NULL || curscr == NULL) {
            fprintf(stderr, "initscr: out of memory\n");
            exit(1);
        }
        table_defaults();
        for (n = 0; n < PAIRS; n++) { pair_fg[n] = 7; pair_bg[n] = 0; }
        initialised = 1;
    }
    for (n = 0; n < rows * cols; n++) curscr->cells[n] = (chtype) -1;   /* the terminal shows nothing known */
    for (n = 0; n < rows * cols; n++) newscr->cells[n] = ' ';
    werase(stdscr);
    term_raw();
    out_str("\033[?1049h\033[0m\033[2J\033[H");
    out_flush();
    cursor_y = 0;
    cursor_x = 0;
    return stdscr != NULL ? 0 : ERR;
}

SCREEN *newterm(const char *type, void *outfd, void *infd)
{
    (void) type;
    (void) outfd;
    (void) infd;
    if (initscr() == ERR) return NULL;
    return (SCREEN *) &screen_object;
}

int endwin(void)
{
    out_str("\033[0m\033[2J\033[H\033[?25h");
    out_flush();
    term_restore();
    return OK;
}

int isendwin(void)
{
    return !raw_active;
}

/* copy a window into the virtual screen at its position */
int wnoutrefresh(WINDOW *win)
{
    int y, x, sy, sx;
    if (win == NULL) return ERR;
    for (y = 0; y < win->maxy; y++) {
        sy = win->begy + y;
        if (sy < 0 || sy >= rows) continue;
        for (x = 0; x < win->maxx; x++) {
            sx = win->begx + x;
            if (sx < 0 || sx >= cols) continue;
            newscr->cells[sy * cols + sx] = win->cells[y * win->maxx + x];
        }
    }
    win->touched = 0;
    return OK;
}

/* send the differences between the virtual screen and the terminal */
int doupdate(void)
{
    int y, x, n;
    chtype last = (chtype) -1;
    int ly = -1, lx = -1;
    for (y = 0; y < rows; y++) {
        for (x = 0; x < cols; x++) {
            n = y * cols + x;
            if (newscr->cells[n] == curscr->cells[n]) continue;
            if (ly != y || lx != x) out_cup(y, x);
            if ((newscr->cells[n] & ~(chtype) A_CHARTEXT) != last) {
                out_sgr(newscr->cells[n]);
                last = newscr->cells[n] & ~(chtype) A_CHARTEXT;
            }
            {
                unsigned char c = (unsigned char) (newscr->cells[n] & A_CHARTEXT);
                out_byte(c >= 32 && c < 127 ? (char) c : ' ');
            }
            curscr->cells[n] = newscr->cells[n];
            ly = y;
            lx = x + 1;
            if (lx >= cols) ly = -1;
        }
    }
    out_str("\033[0m");
    out_cup(cursor_y, cursor_x);
    out_str(cursor_visible ? "\033[?25h" : "\033[?25l");
    out_flush();
    return OK;
}

int wrefresh(WINDOW *win)
{
    wnoutrefresh(win);
    return doupdate();
}

int refresh(void)
{
    return wrefresh(stdscr);
}

int redrawwin(WINDOW *win)
{
    int n;
    if (win != NULL) win->touched = 1;
    out_str("\033[0m\033[2J");
    for (n = 0; n < rows * cols; n++) curscr->cells[n] = (chtype) -1;
    return refresh();
}

int is_wintouched(WINDOW *win)
{
    return win != NULL ? win->touched : 0;
}

int start_color(void)
{
    return OK;
}

int init_pair(short pair, short fg, short bg)
{
    if (pair < 0 || pair >= PAIRS) return ERR;
    pair_fg[pair] = fg;
    pair_bg[pair] = bg;
    return OK;
}

int raw(void)
{
    if (!raw_active) term_raw();
    return OK;
}

int cbreak(void)
{
    return raw();
}

int noecho(void)
{
    return OK;
}

int echo(void)
{
    return OK;
}

int nl(void)
{
    nl_mode = 1;
    return OK;
}

int nonl(void)
{
    nl_mode = 0;
    return OK;
}

int keypad(WINDOW *win, int on)
{
    (void) win;
    (void) on;
    return OK;
}

int curs_set(int visibility)
{
    int previous = cursor_visible;
    cursor_visible = visibility != 0;
    out_str(cursor_visible ? "\033[?25h" : "\033[?25l");
    out_flush();
    return previous ? 1 : 0;
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
    return getch_impl(timeout_ms);
}

int wgetch(WINDOW *win)
{
    (void) win;
    return getch_impl(timeout_ms);
}

int define_key(const char *seq, int code)
{
    if (seq == NULL) return ERR;
    table_add(seq, code);
    return OK;
}

/* no mouse on this terminal path */
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

char *tigetstr(const char *capname)
{
    (void) capname;
    return NULL;
}

char *curses_format_buffer(void)
{
    return format_buf;
}

/* ------------------------------------------------------------ windows: creation and geometry */

WINDOW *newwin(int lines, int cols_, int begy, int begx)
{
    if (lines <= 0) lines = rows - begy;
    if (cols_ <= 0) cols_ = cols - begx;
    return win_alloc(lines, cols_, begy, begx);
}

WINDOW *dupwin(WINDOW *win)
{
    WINDOW *w = win_alloc(win->maxy, win->maxx, win->begy, win->begx);
    int n;
    if (w == NULL) return NULL;
    for (n = 0; n < win->maxy * win->maxx; n++) w->cells[n] = win->cells[n];
    w->attr = win->attr;
    w->bkg = win->bkg;
    return w;
}

int delwin(WINDOW *win)
{
    if (win == NULL || win == stdscr || win == newscr || win == curscr) return ERR;
    free(win->cells);
    free(win);
    return OK;
}

int wresize(WINDOW *win, int lines, int cols_)
{
    chtype *fresh;
    int y, x;
    if (win == NULL || lines <= 0 || cols_ <= 0) return ERR;
    fresh = (chtype *) calloc((unsigned long) (lines * cols_), sizeof(chtype));
    if (fresh == NULL) return ERR;
    for (y = 0; y < lines; y++)
        for (x = 0; x < cols_; x++)
            fresh[y * cols_ + x] = (y < win->maxy && x < win->maxx) ? win->cells[y * win->maxx + x] : win->bkg;
    free(win->cells);
    win->cells = fresh;
    win->maxy = lines;
    win->maxx = cols_;
    win->touched = 1;
    return OK;
}

int overwrite(const WINDOW *src, WINDOW *dst)
{
    int y, x;
    for (y = 0; y < src->maxy; y++) {
        for (x = 0; x < src->maxx; x++) {
            int dy = src->begy + y - dst->begy, dx = src->begx + x - dst->begx;
            if (dy >= 0 && dx >= 0 && dy < dst->maxy && dx < dst->maxx)
                dst->cells[dy * dst->maxx + dx] = src->cells[y * src->maxx + x];
        }
    }
    dst->touched = 1;
    return OK;
}

int wenclose(const WINDOW *win, int y, int x)
{
    return y >= win->begy && y < win->begy + win->maxy && x >= win->begx && x < win->begx + win->maxx;
}

int getbegx(WINDOW *win) { return win->begx; }
int getbegy(WINDOW *win) { return win->begy; }
int getcurx(WINDOW *win) { return win->curx; }
int getcury(WINDOW *win) { return win->cury; }
int getmaxx(WINDOW *win) { return win->maxx; }
int getmaxy(WINDOW *win) { return win->maxy; }

/* ------------------------------------------------------------ windows: drawing */

int wmove(WINDOW *win, int y, int x)
{
    if (win == NULL || y < 0 || x < 0 || y >= win->maxy || x >= win->maxx) return ERR;
    win->cury = y;
    win->curx = x;
    cursor_y = win->begy + y;
    cursor_x = win->begx + x;
    return OK;
}

int move(int y, int x)
{
    return wmove(stdscr, y, x);
}

int waddch(WINDOW *win, chtype ch)
{
    if (win == NULL) return ERR;
    if (ch == '\n') {
        win->curx = 0;
        win->cury++;
        if (win->cury >= win->maxy) win->cury = win->maxy - 1;
        return OK;
    }
    cell_set(win, win->cury, win->curx, cell_value(win, ch));
    win->curx++;
    if (win->curx >= win->maxx) {
        win->curx = 0;
        if (win->cury < win->maxy - 1) win->cury++;
    }
    return OK;
}

int mvwaddch(WINDOW *win, int y, int x, chtype ch)
{
    if (wmove(win, y, x) == ERR) return ERR;
    cell_set(win, y, x, cell_value(win, ch));
    return OK;
}

int mvaddch(int y, int x, chtype ch)
{
    return mvwaddch(stdscr, y, x, ch);
}

int mvwaddnstr(WINDOW *win, int y, int x, const char *s, int n)
{
    int i = 0;
    if (wmove(win, y, x) == ERR) return ERR;
    while (s[i] != 0 && (n < 0 || i < n)) {
        cell_set(win, y, x + i, cell_value(win, (chtype) (unsigned char) s[i]));
        i++;
    }
    return OK;
}

int mvwaddstr(WINDOW *win, int y, int x, const char *s)
{
    return mvwaddnstr(win, y, x, s, -1);
}

int addstr(const char *s)
{
    return waddnstr(stdscr, s, -1);
}

int waddstr(WINDOW *win, const char *s)
{
    return waddnstr(win, s, -1);
}

int mvaddstr(int y, int x, const char *s)
{
    return mvwaddnstr(stdscr, y, x, s, -1);
}

int mvaddnstr(int y, int x, const char *s, int n)
{
    return mvwaddnstr(stdscr, y, x, s, n);
}

int waddnstr(WINDOW *win, const char *s, int n)
{
    return mvwaddnstr(win, win->cury, win->curx, s, n);
}

int mvwhline(WINDOW *win, int y, int x, chtype ch, int n)
{
    int i;
    if (wmove(win, y, x) == ERR) return ERR;
    for (i = 0; i < n; i++) cell_set(win, y, x + i, cell_value(win, ch));
    return OK;
}

int mvhline(int y, int x, chtype ch, int n)
{
    return mvwhline(stdscr, y, x, ch, n);
}

int whline(WINDOW *win, chtype ch, int n)
{
    return mvwhline(win, win->cury, win->curx, ch, n);
}

int mvwvline(WINDOW *win, int y, int x, chtype ch, int n)
{
    int i;
    if (wmove(win, y, x) == ERR) return ERR;
    for (i = 0; i < n; i++) cell_set(win, y + i, x, cell_value(win, ch));
    return OK;
}

int mvvline(int y, int x, chtype ch, int n)
{
    return mvwvline(stdscr, y, x, ch, n);
}

int wvline(WINDOW *win, chtype ch, int n)
{
    return mvwvline(win, win->cury, win->curx, ch, n);
}

int wclrtoeol(WINDOW *win)
{
    int x;
    for (x = win->curx; x < win->maxx; x++) cell_set(win, win->cury, x, win->bkg);
    return OK;
}

int clrtoeol(void)
{
    return wclrtoeol(stdscr);
}

int werase(WINDOW *win)
{
    int n;
    for (n = 0; n < win->maxy * win->maxx; n++) win->cells[n] = win->bkg;
    win->touched = 1;
    return OK;
}

int erase(void)
{
    return werase(stdscr);
}

int wclear(WINDOW *win)
{
    werase(win);
    redrawwin(win);
    return OK;
}

int clear(void)
{
    return wclear(stdscr);
}

chtype mvwinch(WINDOW *win, int y, int x)
{
    if (y < 0 || x < 0 || y >= win->maxy || x >= win->maxx) return (chtype) ERR;
    return win->cells[y * win->maxx + x];
}

chtype winch(WINDOW *win)
{
    return mvwinch(win, win->cury, win->curx);
}

int wborder(WINDOW *win, chtype ls, chtype rs, chtype ts, chtype bs, chtype tl, chtype tr, chtype bl, chtype br)
{
    int y, x;
    if (ls == 0) ls = '|';
    if (rs == 0) rs = '|';
    if (ts == 0) ts = '-';
    if (bs == 0) bs = '-';
    if (tl == 0) tl = '+';
    if (tr == 0) tr = '+';
    if (bl == 0) bl = '+';
    if (br == 0) br = '+';
    for (x = 1; x < win->maxx - 1; x++) {
        cell_set(win, 0, x, cell_value(win, ts));
        cell_set(win, win->maxy - 1, x, cell_value(win, bs));
    }
    for (y = 1; y < win->maxy - 1; y++) {
        cell_set(win, y, 0, cell_value(win, ls));
        cell_set(win, y, win->maxx - 1, cell_value(win, rs));
    }
    cell_set(win, 0, 0, cell_value(win, tl));
    cell_set(win, 0, win->maxx - 1, cell_value(win, tr));
    cell_set(win, win->maxy - 1, 0, cell_value(win, bl));
    cell_set(win, win->maxy - 1, win->maxx - 1, cell_value(win, br));
    return OK;
}

int wbkgd(WINDOW *win, chtype ch)
{
    int n;
    chtype old = win->bkg;
    /* ncurses: character 0 (mc: wbkgd(win, COLOR_PAIR(n))) means a space */
    if ((ch & A_CHARTEXT) == 0) ch |= ' ';
    win->bkg = ch;
    /* as in ncurses: cells with the old background character get the new one, cells
     * with the old background colour (or none) get the new colour */
    for (n = 0; n < win->maxy * win->maxx; n++) {
        chtype c = win->cells[n];
        if ((c & A_CHARTEXT) == (old & A_CHARTEXT))
            c = (c & ~(chtype) A_CHARTEXT) | (ch & A_CHARTEXT);
        if ((c & A_COLOR) == 0 || (c & A_COLOR) == (old & A_COLOR))
            c = (c & ~(chtype) A_COLOR) | (ch & A_COLOR);
        win->cells[n] = c;
    }
    win->touched = 1;
    return OK;
}

int wattrset(WINDOW *win, int attrs)
{
    win->attr = (chtype) attrs;
    return OK;
}

int attrset(int attrs)
{
    return wattrset(stdscr, attrs);
}

/* ncurses semantics: a colour pair REPLACES the current pair (only one at a time), the
 * other attribute bits are added. OR-ing the pair numbers mixed the pairs of mc
 * (4|8|1|2 = 15 = COLOR_BLUE_ON_WHITE: white background everywhere). */
int wattron(WINDOW *win, int attrs)
{
    chtype a = (chtype) attrs;
    if (a & A_COLOR)
        win->attr = (win->attr & ~(chtype) A_COLOR) | (a & A_COLOR);
    win->attr |= a & ~(chtype) A_COLOR;
    return OK;
}

/* a colour pair in attrs turns the colour off completely (back to pair 0) */
int wattroff(WINDOW *win, int attrs)
{
    chtype a = (chtype) attrs;
    if (a & A_COLOR)
        win->attr &= ~(chtype) A_COLOR;
    win->attr &= ~(a & ~(chtype) A_COLOR);
    return OK;
}

int attron(int attrs)
{
    return wattron(stdscr, attrs);
}

int attroff(int attrs)
{
    return wattroff(stdscr, attrs);
}

int wattr_get(WINDOW *win, attr_t *attrs, short *pair, void *opts)
{
    (void) opts;
    if (attrs != NULL) *attrs = win->attr & ~(chtype) A_COLOR;
    if (pair != NULL) *pair = (short) PAIR_NUMBER(win->attr);
    return OK;
}

int wattr_set(WINDOW *win, attr_t attrs, short pair, void *opts)
{
    (void) opts;
    win->attr = (attrs & ~(chtype) A_COLOR) | COLOR_PAIR(pair);
    return OK;
}

int wattr_on(WINDOW *win, attr_t attrs, void *opts)
{
    (void) opts;
    return wattron(win, (int) attrs);
}

int wattr_off(WINDOW *win, attr_t attrs, void *opts)
{
    (void) opts;
    return wattroff(win, (int) attrs);
}

int invert(WINDOW *win)
{
    (void) win;
    return OK;
}
