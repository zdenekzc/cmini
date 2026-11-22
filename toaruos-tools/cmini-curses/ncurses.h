/* ncurses.h - 8-bit ncurses API of the ToaruOS shim for cmini (curses8.c).
 * Values of the constants are those of ncursesw (KEY_*, COLOR_*, A_*, BUTTON*).
 * No wide characters.  Used by tiny-ide and mini-commander. */
#ifndef CMINI_CURSES_NCURSES_H
#define CMINI_CURSES_NCURSES_H

typedef unsigned long chtype;
typedef chtype attr_t;
typedef unsigned long mmask_t;
typedef struct _win_st WINDOW;
typedef struct _screen_st SCREEN;

typedef struct {
    short id;
    int x, y, z;
    mmask_t bstate;
} MEVENT;

extern WINDOW *stdscr;
extern WINDOW *newscr;
extern WINDOW *curscr;
extern int LINES, COLS, ESCDELAY;

#define ERR (-1)
#define OK 0
#define TRUE 1
#define FALSE 0

#define A_NORMAL     0UL
#define A_CHARTEXT   0xFFUL
#define A_COLOR      0xFF00UL
#define A_REVERSE    262144UL
#define A_UNDERLINE  131072UL
#define A_BOLD       2097152UL

#define COLOR_BLACK 0
#define COLOR_RED 1
#define COLOR_GREEN 2
#define COLOR_YELLOW 3
#define COLOR_BLUE 4
#define COLOR_MAGENTA 5
#define COLOR_CYAN 6
#define COLOR_WHITE 7

#define COLOR_PAIR(n) (((chtype)(n) << 8) & A_COLOR)
#define PAIR_NUMBER(a) ((int)(((chtype)(a) & A_COLOR) >> 8))

#define KEY_MIN 257
#define KEY_BREAK 257
#define KEY_DOWN 258
#define KEY_UP 259
#define KEY_LEFT 260
#define KEY_RIGHT 261
#define KEY_HOME 262
#define KEY_BACKSPACE 263
#define KEY_F0 264
#define KEY_F(n) (KEY_F0 + (n))
#define KEY_DL 328
#define KEY_IL 329
#define KEY_DC 330
#define KEY_IC 331
#define KEY_EIC 332
#define KEY_CLEAR 333
#define KEY_EOS 334
#define KEY_EOL 335
#define KEY_SF 336
#define KEY_SR 337
#define KEY_NPAGE 338
#define KEY_PPAGE 339
#define KEY_STAB 340
#define KEY_CTAB 341
#define KEY_CATAB 342
#define KEY_ENTER 343
#define KEY_END 360
#define KEY_BTAB 353
#define KEY_SLEFT 393
#define KEY_SRIGHT 402
#define KEY_RESIZE 410
#define KEY_MOUSE 409
#define KEY_MAX 511

#define BUTTON1_PRESSED 2UL
#define BUTTON1_RELEASED 1UL
#define BUTTON1_CLICKED 4UL
#define BUTTON1_DOUBLE_CLICKED 8UL
#define BUTTON1_TRIPLE_CLICKED 16UL
#define BUTTON3_PRESSED 512UL
#define BUTTON3_RELEASED 256UL
#define BUTTON3_CLICKED 1024UL
#define BUTTON3_DOUBLE_CLICKED 2048UL
#define BUTTON3_TRIPLE_CLICKED 4096UL
#define BUTTON4_PRESSED 65536UL
#define BUTTON5_PRESSED 2097152UL
#define REPORT_MOUSE_POSITION 268435456UL
#define ALL_MOUSE_EVENTS (REPORT_MOUSE_POSITION - 1)

/* a stdio-style call without varargs: format into a buffer, then the string is drawn */
char *curses_format_buffer(void);
#define mvwprintw(win, y, x, ...) \
    mvwaddstr((win), (y), (x), (snprintf(curses_format_buffer(), 2048, __VA_ARGS__), curses_format_buffer()))

int initscr(void);
int endwin(void);
int isendwin(void);
SCREEN *newterm(const char *type, void *outfd, void *infd);
int refresh(void);
int wrefresh(WINDOW *win);
int wnoutrefresh(WINDOW *win);
int doupdate(void);
int redrawwin(WINDOW *win);
int is_wintouched(WINDOW *win);
int start_color(void);
int init_pair(short pair, short fg, short bg);
int raw(void);
int cbreak(void);
int noecho(void);
int echo(void);
int nl(void);
int nonl(void);
int keypad(WINDOW *win, int on);
int curs_set(int visibility);
int beep(void);
int timeout(int ms);
int wtimeout(WINDOW *win, int ms);
int getch(void);
int wgetch(WINDOW *win);
int define_key(const char *seq, int code);
int getmouse(MEVENT *event);
mmask_t mousemask(mmask_t newmask, mmask_t *oldmask);
int mouseinterval(int ms);
char *tigetstr(const char *capname);

WINDOW *newwin(int lines, int cols, int begy, int begx);
WINDOW *dupwin(WINDOW *win);
int delwin(WINDOW *win);
int wresize(WINDOW *win, int lines, int cols);
int overwrite(const WINDOW *src, WINDOW *dst);
int wenclose(const WINDOW *win, int y, int x);
int getbegx(WINDOW *win);
int getbegy(WINDOW *win);
int getcurx(WINDOW *win);
int getcury(WINDOW *win);
int getmaxx(WINDOW *win);
int getmaxy(WINDOW *win);

/* macros of ncurses: store into y and x */
#define getyx(win, y, x) ((y) = getcury(win), (x) = getcurx(win))
#define getbegyx(win, y, x) ((y) = getbegy(win), (x) = getbegx(win))
#define getmaxyx(win, y, x) ((y) = getmaxy(win), (x) = getmaxx(win))

int move(int y, int x);
int wmove(WINDOW *win, int y, int x);
int mvaddch(int y, int x, chtype ch);
int mvwaddch(WINDOW *win, int y, int x, chtype ch);
int waddch(WINDOW *win, chtype ch);
int mvaddstr(int y, int x, const char *s);
int mvwaddstr(WINDOW *win, int y, int x, const char *s);
int waddnstr(WINDOW *win, const char *s, int n);
int addstr(const char *s);
int waddstr(WINDOW *win, const char *s);
int mvaddnstr(int y, int x, const char *s, int n);
int mvwaddnstr(WINDOW *win, int y, int x, const char *s, int n);
int mvhline(int y, int x, chtype ch, int n);
int mvwhline(WINDOW *win, int y, int x, chtype ch, int n);
int mvvline(int y, int x, chtype ch, int n);
int mvwvline(WINDOW *win, int y, int x, chtype ch, int n);
int whline(WINDOW *win, chtype ch, int n);
int wvline(WINDOW *win, chtype ch, int n);
int wclrtoeol(WINDOW *win);
int clrtoeol(void);
int werase(WINDOW *win);
int erase(void);
int wclear(WINDOW *win);
int clear(void);
chtype mvwinch(WINDOW *win, int y, int x);
chtype winch(WINDOW *win);
int wborder(WINDOW *win, chtype ls, chtype rs, chtype ts, chtype bs, chtype tl, chtype tr, chtype bl, chtype br);
int wbkgd(WINDOW *win, chtype ch);
int attrset(int attrs);
int wattrset(WINDOW *win, int attrs);
int attron(int attrs);
int attroff(int attrs);
int wattron(WINDOW *win, int attrs);
int wattroff(WINDOW *win, int attrs);
int wattr_get(WINDOW *win, attr_t *attrs, short *pair, void *opts);
int wattr_set(WINDOW *win, attr_t attrs, short pair, void *opts);
int wattr_on(WINDOW *win, attr_t attrs, void *opts);
int wattr_off(WINDOW *win, attr_t attrs, void *opts);
int invert(WINDOW *win);

#endif
