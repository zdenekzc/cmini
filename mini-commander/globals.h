#ifndef GLOBALS_H
#define GLOBALS_H

void initialize_ncurses(void);
void refresh_screen(int cursor_visibility);
void draw_buttons(int maxY, int maxX, const char *const labels[9]);
extern const char *const panel_buttons[9];
int button_key(MEVENT *event, const char *const labels[9], int *pressed);
void mouse_tracking(int enabled);
void select_file(PanelProp *panel, FileNode *file, int selected);
int panel_mouse(MEVENT *event, PanelMouse *mouse);
void draw_windows(int maxY, int maxX);
int printable_byte(int c);
int text_cell(const char *text, int length, char *chars, int *width);
int text_previous(const char *text, int position);
int text_next(const char *text, int length, int position);
int text_column(const char *text, int length, int tab_width);
int text_offset(const char *text, int length, int column, int tab_width);
void draw_text(WINDOW *win, int row, int x, const char *text, int start, int width);
int read_text_key(WINDOW *win, char *text, MEVENT *mouse);
void shorten(char *name, int width, char *result);
void update_panel(WINDOW *win, PanelProp *panel);
void update_panel_cursor(void);
void restore_panel_position(PanelProp *panel);
void init_screen(void);
void cleanup(void);
void redraw_ui(void);
int compare_nodes(FileNode *a, FileNode *b, SortOrders sort_order);
void sort_file_nodes(FileNode **head_ref, SortOrders sort_order);
int update_panel_files(PanelProp *panel);
void update_files_in_both_panels(void);
void free_file_nodes(FileNode *head);
int lines(char * title);
WINDOW *create_dialog(char *title, char *buttons[], int prompt_is_present, int is_danger, int vertical_buttons);
void update_dialog_buttons(WINDOW *win, char * title, char *buttons[], int selected, int prompt_present, int editing_prompt, int is_danger, int vertical_buttons);
int show_dialog(char *title, char *buttons[], int selected, char *prompt, int is_danger, int vertical_buttons, int edit_prompt);
int show_chmod_dialog(const char *name, const stat_t *info, mode_t *mode, mode_t *marked, int multiple);
void change_file_permissions(FileNode *current);
int show_symlink_dialog(char *target, char *link_name);
int show_edit_symlink_dialog(const char *name, char *target);
void create_symbolic_link(FileNode *current);
void edit_symbolic_link(FileNode *current);
void show_errormsg(char * msg);
void update_cmd(void);
int execute_file(const char *filename);
int insert_command_path(const char *path, int directory);
void command_history_add(const char *command);
void command_history_move(int direction);
void command_history_free(void);
int command_cd(const char *command);
void display_line(WINDOW *win, file_lines *line, int max_x, int current_col, int editor_mode, int show_tabs, int syntax, off_t mark_start, off_t mark_end);
int view_edit_file(char *filename, int editor_mode);
int file_has_extension(const char *filename, const char *extensions[]);
void dive_into_directory(FileNode *current);
int change_panel_directory(PanelProp *panel, const char *path);
int filename_matches(const char *pattern, const char *name);
void select_pattern(PanelProp *panel, const char *pattern, int action);
void swap_panels(void);
int noesc(int ch);
void format_size_with_units(off_t size, char *size_str, size_t len, int maxlen);
void show_shadow(WINDOW *win);
void draw_dialog_frame(WINDOW *win, int separator);
void draw_dialog_text(WINDOW *win, int row, const char *text);
void dialog_save_screen();
void dialog_restore_screen();
void create_progress_dialog(int title_lines);
int file_exists(const char *path);
int update_progress_dialog(char *title, int current_progress, int total_progress, char *infotext);
int update_progress_dialog_delta(char *title, int current_progress, int total_progress, char *infotext);
int panel_mass_action(OperationFunc func, char *tgt, operationContext *context);
void run_file_operation(OperationFunc operation, char *target);
int recursive_operation(const char *src, const char *tgt, operationContext *context, OperationFunc func);
int copy_operation(const char *src, const char *tgt, operationContext *context);
int move_operation(const char *src, const char *tgt, operationContext *context);
int delete_operation(const char *src, const char *tgt, operationContext *context);
int countstats_operation(const char *src, const char *tgt, operationContext *context);
int mkdir_recursive(const char *path, mode_t mode);
int format_number(off_t num, char *str);

// Results of SHORTEN and SPRINTF live in a ring of static buffers (no varargs or
// statement expressions in cparser); eight live results per statement are enough.
char *ring_next(void);
char *ring_result(int length);
char *shorten_ring(const char *name, int width);
#define SHORTEN(name, width) shorten_ring((name), (width))

#define SPRINTF(...) ring_result(snprintf(ring_next(), CMD_MAX, __VA_ARGS__))
#define format_text(buffer, ...) (snprintf((buffer), CMD_MAX, __VA_ARGS__), (buffer))

#endif // GLOBALS_H
