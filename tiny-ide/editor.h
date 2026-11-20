#ifndef TINY_IDE_EDITOR_H
#define TINY_IDE_EDITOR_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define EDITOR_NO_SELECTION ((size_t)-1)
#define EDITOR_MAX_FILE ((size_t)4 * 1024 * 1024)
#define EDITOR_CELL_CHARS 8

typedef struct editor_history editor_history_t;

/*
 * The text is always NUL-terminated when text is non-NULL.  Cursor and
 * selection offsets are byte offsets.  lines contains the start of every
 * line, including the one empty line of an empty buffer.
 */
typedef struct editor editor_t;
struct editor {
    char *text;
    size_t length, capacity;
    size_t *lines;
    int line_count;
    size_t cursor, anchor;
    unsigned long long revision, saved_revision;
    size_t top_line;
    int left_column, preferred_column;
    int preferred_valid;

    /* Private implementation details. */
    int line_capacity;
    editor_history_t *undo, *redo;
};

/* editor_init and editor_set_text establish an initially saved, empty undo
 * history.  set_text resets revision and saved_revision to zero on success,
 * preserves the old buffer on failure, and returns zero on success. */
void editor_init(editor_t *e);
int editor_set_text(editor_t *e, const char *data, size_t length);
void editor_free(editor_t *e);

/* Line numbers are clamped to the available range.  line_end excludes LF. */
size_t editor_line_start(const editor_t *e, int line);
size_t editor_line_end(const editor_t *e, int line);
/* Return zero on success; an out-of-range offset is clamped. */
int editor_position(const editor_t *e, size_t offset,
                    int *line, int *column);
size_t editor_offset_at(const editor_t *e, int line, int column);
/* Return one if the viewport changed, zero otherwise. */
int editor_scroll_visible(editor_t *e, int rows, int columns);

void editor_move_to(editor_t *e, size_t offset, int keep_selection);
void editor_move_left(editor_t *e, int keep_selection);
void editor_move_right(editor_t *e, int keep_selection);
void editor_move_up(editor_t *e, int keep_selection);
void editor_move_down(editor_t *e, int keep_selection);
void editor_move_home(editor_t *e, int keep_selection);
void editor_move_end(editor_t *e, int keep_selection);
void editor_move_page(editor_t *e, int lines, int keep_selection);
void editor_move_document(editor_t *e, int end, int keep_selection);
void editor_move_word(editor_t *e, int direction, int keep_selection);
void editor_goto_line(editor_t *e, int one_based_line, int keep_selection);
void editor_select_all(editor_t *e);

int editor_selection(const editor_t *e, size_t *start, size_t *end);
char *editor_copy_selection(const editor_t *e, size_t *length);

/* Editing functions return one for a change, zero for a no-op, and -1 on
 * error.  Inserting into a nonempty selection replaces it as one undo step. */
int editor_insert(editor_t *e, const char *text, size_t length);
int editor_backspace(editor_t *e);
int editor_delete(editor_t *e);
int editor_newline(editor_t *e);
int editor_undo(editor_t *e);
int editor_redo(editor_t *e);
void editor_mark_saved(editor_t *e);

/* Search is bytewise and case-sensitive.  A successful match is selected and
 * the cursor is placed at its end. */
long editor_find(editor_t *e, const char *needle);

/*
 * Display-cell helpers work on 1-byte ASCII.  ed_text_cell returns the bytes
 * consumed (0 or 1), writes the cell into chars (NUL-terminated, at most
 * EDITOR_CELL_CHARS-1 characters) and sets width.  Tabs have width one here;
 * line-oriented column helpers expand them to tab stops.
 */
int ed_text_cell(const char *text, int length,
                 char chars[EDITOR_CELL_CHARS], int *width);
size_t ed_text_next(const char *text, size_t length, size_t position);
size_t ed_text_previous(const char *text, size_t position);
int ed_text_column(const char *text, size_t length, int tab_width);
size_t ed_text_offset(const char *text, size_t length, int column,
                      int tab_width);

#ifdef __cplusplus
}
#endif

#endif
