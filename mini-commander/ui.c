#include "includes.h"
#include "types.h"
#include "globals.h"

// The program handles 8-bit text only (also for a plain VGA text console):
// one byte is one display cell.  Bytes outside 32..126 are shown as '.'.
int printable_byte(int c)
{
    return c >= 32 && c < 127;
}

int text_cell(const char *text, int length, char *chars, int *width)
{
    if (length <= 0)
    {
        chars[0]='\0';
        *width=0;
        return 0;
    }
    chars[0]=text[0];
    chars[1]='\0';
    *width=1;
    return 1;
}

// Advance by one byte (one cell).
int text_next(const char *text, int length, int position)
{
    (void)text; (void)length;
    return position+1;
}

// Find the preceding display cell.
int text_previous(const char *text, int position)
{
    (void)text;
    return position > 0 ? position-1 : 0;
}

// Convert a byte length to terminal columns; a tab spans tab_width columns.
int text_column(const char *text, int length, int tab_width)
{
    int column=0;
    for (int offset=0; offset < length; offset++)
        column+=text[offset] == '\t' ? tab_width : 1;
    return column;
}

// Snap a screen column to the beginning of a cell.
int text_offset(const char *text, int length, int column, int tab_width)
{
    int offset=0, current=0;
    while (offset < length)
    {
        int width=text[offset] == '\t' ? tab_width : 1;
        if (current+width > column) break;
        offset++;
        current+=width;
    }
    return offset;
}

// Result buffers for SHORTEN and SPRINTF: a ring of eight, see globals.h.
static char ring_buffers[8][CMD_MAX];
static int ring_index=0;

char *ring_next(void)
{
    ring_index=(ring_index+1) % 8;
    return ring_buffers[ring_index];
}

char *ring_result(int length)
{
    (void)length;
    return ring_buffers[ring_index];
}

char *shorten_ring(const char *name, int width)
{
    char *buffer=ring_next();
    shorten((char *)name, width, buffer);
    return buffer;
}

// Shorten to width columns, keeping the start and the end and putting '~' in the middle.
void shorten(char *name, int width, char *result) {
    result[0]='\0';
    if (width <= 0) return;
    if (width > CMD_MAX - 2) width=CMD_MAX - 2;
    char text[CMD_MAX];
    int count=0;
    for (const char *p=name; *p && count < CMD_MAX - 1; p++)
        text[count++]=printable_byte((unsigned char)*p) ? *p : '?';
    text[count]='\0';
    if (count <= width)
    {
        memcpy(result, text, count + 1);
        return;
    }
    int half=(width - 1) / 2, right=width - 1 - half;
    memcpy(result, text, half);
    result[half]='~';
    memcpy(result + half + 1, text + count - right, right);
    result[width]='\0';
}

// Draw bytes in a column viewport without wrapping or emitting controls.
void draw_text(WINDOW *win, int row, int x, const char *text, int start, int width)
{
    attr_t attributes;
    short color;
    wattr_get(win, &attributes, &color, NULL);
    int length=strlen(text), column=0;
    for (int offset=0; offset < length && column < start+width; offset++, column++)
    {
        char c=printable_byte((unsigned char)text[offset]) ? text[offset] : '.';
        if (column >= start)
            mvwaddch(win, row, x+column-start, (chtype)(unsigned char)c | attributes);
    }
}

// Text keys are 7-bit characters; ncurses key codes (>= 256) are returned as they are.
int read_text_key(WINDOW *win, char *text, MEVENT *mouse)
{
    text[0]='\0';
    // One KEY_MOUSE can announce several queued events; drain them before waiting.
    if (getmouse(mouse) == OK) return KEY_MOUSE;
    int input=wgetch(win);
    if (input == ERR) return ERR;
    if (input == KEY_MOUSE) return getmouse(mouse) == OK ? KEY_MOUSE : ERR;
    if (input >= 256) return noesc(input);
    if (input == 127 || input == 8) return KEY_BACKSPACE;
    if (input < 32) return input;
    if (!printable_byte(input)) return ERR;
    text[0]=(char)input;
    text[1]='\0';
    return 0;
}

// Hide the cursor during changed output, then show it only at its final position.
void refresh_screen(int cursor_visibility)
{
    if (is_wintouched(newscr)) curs_set(0);
    doupdate();
    curs_set(cursor_visibility);
}

// Draw the inset frame shared by confirmation and progress dialogs.
void draw_dialog_frame(WINDOW *win, int separator)
{
    int width=getmaxx(win), height=getmaxy(win);
    mvwvline(win, 2, 1, '|', height-4);
    mvwvline(win, 2, width-2, '|', height-4);
    int rows[]={1, separator, height-2};
    for (int i=0; i < 3; i++)
    {
        mvwhline(win, rows[i], 2, '-', width-4);
        mvwaddch(win, rows[i], 1, '+');
        mvwaddch(win, rows[i], width-2, '+');
    }
}

// Draw newline-separated dialog text, shortening each line to the available columns.
void draw_dialog_text(WINDOW *win, int row, const char *text)
{
    char *copy=strdup(text), *line=strtok(copy, "\n");
    while (line)
    {
        mvwaddstr(win, row++, 3, SHORTEN(line, getmaxx(win)-6));
        line=strtok(NULL, "\n");
    }
    free(copy);
}

// Keep function keys in their usual slots and clip labels on narrow terminals.
const char *const panel_buttons[9]={"Sort", "View", "Edit", "Copy", "Move", "Mkdir", "Del", "Refresh", "Quit"};

void draw_buttons(int maxY, int maxX, const char *const labels[9]) {
    attrset(A_NORMAL);
    move(maxY - 1, 0);
    clrtoeol();
    for (int i=0; i < 9; i++) {
        int x=i*maxX/9, width=(i+1)*maxX/9-x-1;
        if (width <= 0) continue;
        char key[4];
        int length=snprintf(key, sizeof(key), "F%d", i+2);
        attrset(A_NORMAL);
        mvaddnstr(maxY-1, x, key, width);
        if (width <= length) continue;
        attrset(COLOR_PAIR(COLOR_BLACK_ON_CYAN));
        mvhline(maxY-1, x+length, ' ', width-length);
        mvaddnstr(maxY-1, x+length, labels[i], width-length);
    }
    attrset(A_NORMAL);
}

// Translate clicks on visible, active footer slots into their keyboard actions.
int button_key(MEVENT *event, const char *const labels[9], int *pressed)
{
    int hit=0, columns=getmaxx(stdscr), footer=event->y == getmaxy(stdscr)-1;
    for (int i=0; footer && i < 9; i++)
        if (labels[i][0] && event->x >= i*columns/9 && event->x < (i+1)*columns/9-1) hit=KEY_F(i+2);
    if ((event->bstate & BUTTON1_PRESSED) && !(event->bstate & REPORT_MOUSE_POSITION)) *pressed=hit ? hit : -1;
    int clicked=event->bstate & (BUTTON1_CLICKED|BUTTON1_DOUBLE_CLICKED|BUTTON1_TRIPLE_CLICKED);
    if (event->bstate & BUTTON1_RELEASED)
    {
        // With mouseinterval(0), ncurses can report a quick click as a release alone.
        clicked=*pressed == 0 || *pressed == hit;
        *pressed=0;
    }
    if (clicked && hit) { *pressed=0; return hit; }
    return footer ? ERR : KEY_MOUSE;
}

// Standard xterm profiles report clicks only; request motion while a button is held.
void mouse_tracking(int enabled)
{
    const char *prefix=tigetstr("kmous");
    if (prefix && (!strcmp(prefix, "\033[M") || !strcmp(prefix, "\033[<")))
    {
        fputs(enabled ? "\033[?1002h" : "\033[?1002l", stdout);
        fflush(stdout);
    }
}

void draw_windows(int maxY, int maxX) {
    // Stage the background before the panel windows.
    wnoutrefresh(stdscr);

    WINDOW **windows[]={&win1, &win2};
    for (int i=0; i < 2; i++) {
        int x=i*maxX/2, width=(i+1)*maxX/2-x;
        delwin(*windows[i]);
        *windows[i]=newwin(maxY-2, width, 0, x);
        wbkgd(*windows[i], COLOR_PAIR(COLOR_WHITE_ON_BLUE));
        wborder(*windows[i], '|', '|', '-', '-', '+', '+', '+', '+');
        wnoutrefresh(*windows[i]);
    }
}
