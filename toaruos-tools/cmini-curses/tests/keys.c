/* keys.c - checks the 8-bit curses shim: reads 6 keys and prints their codes (after endwin) */
#include <stdio.h>
#include <ncurses.h>

int main(void)
{
    int codes[6];
    int i;
    initscr();
    raw();
    noecho();
    keypad(stdscr, 1);
    for (i = 0; i < 6; i++) {
        codes[i] = getch();
    }
    endwin();
    for (i = 0; i < 6; i++) {
        printf("KEY%d=%d\n", i, codes[i]);
    }
    return 0;
}
