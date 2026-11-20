/* termios.h - layout of the ToaruOS kernel (cmini declarations; libc.a is the ToaruOS one) */
#ifndef TOARU_CMINI_TERMIOS_H
#define TOARU_CMINI_TERMIOS_H

struct winsize {
    unsigned short ws_row;
    unsigned short ws_col;
    unsigned short ws_xpixel;
    unsigned short ws_ypixel;
};

typedef unsigned int tcflag_t;
typedef unsigned char cc_t;

#define VMIN   6
#define VTIME  11
#define NCCS   32

#define IGNBRK 0000004
#define BRKINT 0000001
#define ICRNL  0000002
#define IGNCR  0000010
#define INLCR  0000040
#define ISTRIP 0000200
#define IXON   0004000
#define IXOFF  0002000
#define IXANY  0001000
#define OPOST  0000001
#define ECHO   0000010
#define ECHONL 0000100
#define ICANON 0000002
#define ISIG   0000001
#define IEXTEN 0001000
#define CSIZE  0000060
#define CS8    0000060
#define PARENB 0000400
#define TCSANOW 0x0001
#define TIOCGWINSZ 0x400E

struct termios {
    tcflag_t c_iflag;
    tcflag_t c_oflag;
    tcflag_t c_cflag;
    tcflag_t c_lflag;
    cc_t     c_cc[NCCS];
};

typedef struct winsize winsize_t;
typedef struct termios termios_t;

int tcgetattr(int fd, struct termios *t);
int tcsetattr(int fd, int when, const struct termios *t);

#endif
