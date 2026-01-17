#include "fm_fat.h"
#include "../drivers/vga.h"
#include "../drivers/keyboard.h"
#include "../fs/fat.h"
#include "string.h"
#include "ports.h"

// ============== Константы ==============
#define FM_PANEL_WIDTH    39
#define FM_PANEL_HEIGHT   19
#define FM_MAX_FILES      64
#define FM_PATH_MAX       128

// ============== Цвета ==============
#define COL_BORDER        0x0B    // Cyan - рамка
#define COL_BORDER_ACT    0x0E    // Yellow - активная рамка
#define COL_HEADER        0x1F    // White on blue - заголовок
#define COL_FILE          0x07    // Light gray - файл
#define COL_DIR           0x0A    // Light green - папка
#define COL_SELECTED      0x70    // Black on white - выделенный
#define COL_SEL_DIR       0x72    // Green on white - выделенная папка
#define COL_PARENT        0x0E    // Yellow - родительская папка
#define COL_FKEYS         0x30    // Black on cyan - F-клавиши
#define COL_FKEYS_NUM     0x0F    // White - номера F-клавиш
#define COL_STATUS        0x1E    // Yellow on blue - статус
#define COL_DIALOG_BG     0x1F    // White on blue - диалог
#define COL_DIALOG_TITLE  0x1E    // Yellow on blue - заголовок диалога
#define COL_INPUT         0x0F    // White on black - поле ввода

// ============== Структура панели ==============
typedef struct {
    fs_node*  current_dir;
    char      path[FM_PATH_MAX];
    int       selected;
    int       scroll;
    int       file_count;
    fs_node*  files[FM_MAX_FILES];
    int       has_parent;
} fm_panel;

// ============== Глобальные переменные ==============
static fm_panel left_panel;
static fm_panel right_panel;
static int active_panel;
static int fm_running;
static char fm_status[80];
static int fm_need_redraw;

// ============== Скан-коды клавиш ==============
#define KEY_UP      0x48
#define KEY_DOWN    0x50
#define KEY_LEFT    0x4B
#define KEY_RIGHT   0x4D
#define KEY_ENTER   0x1C
#define KEY_TAB     0x0F
#define KEY_ESC     0x01
#define KEY_HOME    0x47
#define KEY_END     0x4F
#define KEY_PGUP    0x49
#define KEY_PGDN    0x51
#define KEY_F1      0x3B
#define KEY_F2      0x3C
#define KEY_F3      0x3D
#define KEY_F4      0x3E
#define KEY_F5      0x3F
#define KEY_F6      0x40
#define KEY_F7      0x41
#define KEY_F8      0x42
#define KEY_F9      0x43
#define KEY_F10     0x44
#define KEY_F11     0x57
#define KEY_F12     0x58

// ============== Клавиатурные функции ==============

static int fm_kbd_has_data(void) {
    return inb(0x64) & 1;
}

static uint8_t fm_read_scancode_raw(void) {
    return inb(0x60);
}

static void fm_kbd_flush(void) {
    while (fm_kbd_has_data()) {
        fm_read_scancode_raw();
        for (volatile int i = 0; i < 1000; i++);
    }
}

static void fm_wait_key_release(void) {
    for (volatile int i = 0; i < 50000; i++);
    fm_kbd_flush();
}

static uint8_t fm_read_scancode(void) {
    while (!fm_kbd_has_data());
    return fm_read_scancode_raw();
}

static uint8_t fm_wait_key(void) {
    uint8_t sc;
    do {
        sc = fm_read_scancode();
    } while (sc & 0x80);
    return sc;
}

// ============== Вспомогательные функции VGA ==============

static void fm_put_char(int x, int y, char c, uint8_t color) {
    if (x < 0 || x >= VGA_WIDTH || y < 0 || y >= VGA_HEIGHT) return;
    uint16_t pos = y * VGA_WIDTH + x;
    vga_put_at(c, color, pos);
}

static void fm_put_string(int x, int y, const char* str, uint8_t color) {
    while (*str && x < VGA_WIDTH) {
        fm_put_char(x++, y, *str++, color);
    }
}

static void fm_put_string_n(int x, int y, const char* str, int max_width, uint8_t color) {
    int i = 0;
    while (str[i] && i < max_width && x + i < VGA_WIDTH) {
        fm_put_char(x + i, y, str[i], color);
        i++;
    }
    while (i < max_width && x + i < VGA_WIDTH) {
        fm_put_char(x + i, y, ' ', color);
        i++;
    }
}

static void fm_draw_dialog_box(int x, int y, int w, int h, const char* title, uint8_t color) {
    fm_put_char(x, y, '+', color);
    for (int i = 1; i < w - 1; i++) {
        fm_put_char(x + i, y, '-', color);
    }
    fm_put_char(x + w - 1, y, '+', color);
    
    if (title) {
        int title_x = x + (w - (int)strlen(title)) / 2;
        fm_put_string(title_x, y, title, COL_DIALOG_TITLE);
    }
    
    for (int row = 1; row < h - 1; row++) {
        fm_put_char(x, y + row, '|', color);
        for (int col = 1; col < w - 1; col++) {
            fm_put_char(x + col, y + row, ' ', color);
        }
        fm_put_char(x + w - 1, y + row, '|', color);
    }
    
    fm_put_char(x, y + h - 1, '+', color);
    for (int i = 1; i < w - 1; i++) {
        fm_put_char(x + i, y + h - 1, '-', color);
    }
    fm_put_char(x + w - 1, y + h - 1, '+', color);
}

// ============== Диалог ввода текста ==============

// Возвращает 1 если OK, 0 если отмена
static int fm_input_dialog(const char* title, const char* prompt, char* buffer, int max_len) {
    int dialog_w = 50;
    int dialog_h = 7;
    int dialog_x = (VGA_WIDTH - dialog_w) / 2;
    int dialog_y = (VGA_HEIGHT - dialog_h) / 2;
    
    fm_draw_dialog_box(dialog_x, dialog_y, dialog_w, dialog_h, title, COL_DIALOG_BG);
    
    fm_put_string(dialog_x + 2, dialog_y + 2, prompt, COL_DIALOG_BG);
    
    int input_x = dialog_x + 2;
    int input_y = dialog_y + 3;
    int input_w = dialog_w - 4;
    
    for (int i = 0; i < input_w; i++) {
        fm_put_char(input_x + i, input_y, ' ', COL_INPUT);
    }
    
    fm_put_string(dialog_x + 2, dialog_y + 5, "Enter=OK  ESC=Cancel", 0x17);
    
    int pos = 0;
    buffer[0] = '\0';
    
    vga_set_cursor(input_y * VGA_WIDTH + input_x);
    
    fm_wait_key_release();
    
    while (1) {
        for (int i = 0; i < input_w; i++) {
            char c = (i < pos) ? buffer[i] : ' ';
            fm_put_char(input_x + i, input_y, c, COL_INPUT);
        }
        
        vga_set_cursor(input_y * VGA_WIDTH + input_x + pos);
        
        uint8_t sc = fm_wait_key();
        
        if (sc == KEY_ESC) {
            fm_wait_key_release();
            return 0;
        }
        
        if (sc == KEY_ENTER) {
            fm_wait_key_release();
            return 1;
        }
        
        if (sc == 0x0E && pos > 0) {
            pos--;
            buffer[pos] = '\0';
            continue;
        }
        
        char c = 0;
        
        static const char scancode_to_char[] = {
            0, 0, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', 0, 0,
            'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', 0, 0,
            'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0, '\\',
            'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' '
        };
        
        if (sc < sizeof(scancode_to_char)) {
            c = scancode_to_char[sc];
        }
        
        if (c && pos < max_len - 1) {
            buffer[pos++] = c;
            buffer[pos] = '\0';
        }
    }
}

// ============== Работа с панелями ==============

static int fm_get_file_size(fs_node* node) {
    if (!node || node->type != FS_FILE) return 0;
    return (int)strlen(node->content);
}

static void fm_refresh_panel(fm_panel* panel) {
    panel->file_count = 0;
    panel->has_parent = 0;
    
    if (!panel->current_dir) return;
    
    if (panel->current_dir->parent != NULL) {
        panel->has_parent = 1;
    }
    
    for (int i = 0; i < panel->current_dir->child_count && panel->file_count < FM_MAX_FILES; i++) {
        panel->files[panel->file_count++] = panel->current_dir->children[i];
    }
    
    int total = panel->file_count + (panel->has_parent ? 1 : 0);
    if (panel->selected >= total) {
        panel->selected = total > 0 ? total - 1 : 0;
    }
    if (panel->selected < 0) {
        panel->selected = 0;
    }
}

static void fm_build_path(fm_panel* panel) {
    if (!panel->current_dir) {
        strcpy(panel->path, "/");
        return;
    }
    
    char parts[8][MAX_NAME_LEN];
    int depth = 0;
    fs_node* node = panel->current_dir;
    
    while (node && node->parent && depth < 8) {
        strcpy(parts[depth++], node->name);
        node = node->parent;
    }
    
    panel->path[0] = '/';
    panel->path[1] = '\0';
    
    for (int i = depth - 1; i >= 0; i--) {
        if (strlen(panel->path) > 1) {
            strcat(panel->path, "/");
        }
        strcat(panel->path, parts[i]);
    }
}

static void fm_init_panel(fm_panel* panel, fs_node* dir) {
    panel->current_dir = dir ? dir : fs_root;
    panel->selected = 0;
    panel->scroll = 0;
    fm_build_path(panel);
    fm_refresh_panel(panel);
}

static fm_panel* fm_get_active(void) {
    return active_panel == 0 ? &left_panel : &right_panel;
}

static fm_panel* fm_get_inactive(void) {
    return active_panel == 0 ? &right_panel : &left_panel;
}

static fs_node* fm_get_selected_node(fm_panel* panel) {
    if (panel->has_parent && panel->selected == 0) {
        return NULL;
    }
    int idx = panel->selected - (panel->has_parent ? 1 : 0);
    if (idx >= 0 && idx < panel->file_count) {
        return panel->files[idx];
    }
    return NULL;
}

// ============== Отрисовка ==============

static void fm_draw_panel(fm_panel* panel, int start_x, int is_active) {
    uint8_t border_col = is_active ? COL_BORDER_ACT : COL_BORDER;
    
    fm_put_char(start_x, 0, '+', border_col);
    for (int i = 1; i < FM_PANEL_WIDTH - 1; i++) {
        fm_put_char(start_x + i, 0, '-', border_col);
    }
    fm_put_char(start_x + FM_PANEL_WIDTH - 1, 0, '+', border_col);
    
    char title[FM_PANEL_WIDTH - 4];
    int path_len = strlen(panel->path);
    if (path_len > FM_PANEL_WIDTH - 6) {
        strcpy(title, "...");
        strcat(title, panel->path + path_len - (FM_PANEL_WIDTH - 9));
    } else {
        strcpy(title, panel->path);
    }
    
    int title_x = start_x + (FM_PANEL_WIDTH - (int)strlen(title)) / 2;
    fm_put_string(title_x, 0, title, COL_HEADER);
    
    fm_put_char(start_x, 1, '|', border_col);
    fm_put_string_n(start_x + 1, 1, " Name", 24, COL_HEADER);
    fm_put_string_n(start_x + 25, 1, "Size", 12, COL_HEADER);
    fm_put_char(start_x + FM_PANEL_WIDTH - 1, 1, '|', border_col);
    
    fm_put_char(start_x, 2, '+', border_col);
    for (int i = 1; i < FM_PANEL_WIDTH - 1; i++) {
        fm_put_char(start_x + i, 2, '-', border_col);
    }
    fm_put_char(start_x + FM_PANEL_WIDTH - 1, 2, '+', border_col);
    
    int visible_lines = FM_PANEL_HEIGHT - 1;
    int total_items = panel->file_count + (panel->has_parent ? 1 : 0);
    
    if (panel->selected < panel->scroll) {
        panel->scroll = panel->selected;
    }
    if (panel->selected >= panel->scroll + visible_lines) {
        panel->scroll = panel->selected - visible_lines + 1;
    }
    
    for (int line = 0; line < visible_lines; line++) {
        int y = 3 + line;
        int idx = panel->scroll + line;
        
        fm_put_char(start_x, y, '|', border_col);
        
        if (idx < total_items) {
            int is_selected = (idx == panel->selected);
            uint8_t color;
            char name[32];
            char size_str[16];
            
            if (panel->has_parent && idx == 0) {
                strcpy(name, "[..]");
                strcpy(size_str, "<UP>");
                color = is_selected ? COL_SELECTED : COL_PARENT;
            } else {
                int file_idx = idx - (panel->has_parent ? 1 : 0);
                fs_node* node = panel->files[file_idx];
                
                if (node->type == FS_DIR) {
                    name[0] = '[';
                    strncpy(name + 1, node->name, 28);
                    name[29] = '\0';
                    strcat(name, "]");
                    strcpy(size_str, "<DIR>");
                    color = is_selected ? COL_SEL_DIR : COL_DIR;
                } else {
                    strncpy(name, node->name, 30);
                    name[30] = '\0';
                    int size = fm_get_file_size(node);
                    itoa(size, size_str, 10);
                    strcat(size_str, " B");
                    color = is_selected ? COL_SELECTED : COL_FILE;
                }
            }
            
            if (is_selected) {
                for (int i = 1; i < FM_PANEL_WIDTH - 1; i++) {
                    fm_put_char(start_x + i, y, ' ', color);
                }
            }
            
            fm_put_string_n(start_x + 1, y, name, 24, color);
            fm_put_string_n(start_x + 25, y, size_str, 12, color);
        } else {
            fm_put_string_n(start_x + 1, y, "", FM_PANEL_WIDTH - 2, COL_FILE);
        }
        
        fm_put_char(start_x + FM_PANEL_WIDTH - 1, y, '|', border_col);
    }
    
    int bottom_y = 3 + visible_lines;
    fm_put_char(start_x, bottom_y, '+', border_col);
    for (int i = 1; i < FM_PANEL_WIDTH - 1; i++) {
        fm_put_char(start_x + i, bottom_y, '-', border_col);
    }
    fm_put_char(start_x + FM_PANEL_WIDTH - 1, bottom_y, '+', border_col);
    
    char info[20];
    itoa(panel->file_count, info, 10);
    strcat(info, " items");
    fm_put_string(start_x + 2, bottom_y, info, border_col);
}

static void fm_draw_fkeys(void) {
    int y = VGA_HEIGHT - 1;
    
    for (int x = 0; x < VGA_WIDTH; x++) {
        fm_put_char(x, y, ' ', COL_FKEYS);
    }
    
    // F1-Help F2-Rename F3-View F4-Edit F5-Copy F6-Move F7-MkDir F8-Del F9-Touch F10-Quit
    const struct { int num; const char* name; } keys[] = {
        {1, "Help"}, {2, "Ren"}, {3, "View"}, {4, "Edit"}, {5, "Copy"},
        {6, "Move"}, {7, "MkDir"}, {8, "Del"}, {9, "Touch"}, {10, "Quit"}
    };
    
    int x = 0;
    for (int i = 0; i < 10; i++) {
        char num_str[4];
        itoa(keys[i].num, num_str, 10);
        
        for (int j = 0; num_str[j]; j++) {
            fm_put_char(x++, y, num_str[j], COL_FKEYS_NUM);
        }
        
        const char* name = keys[i].name;
        while (*name && x < (i + 1) * 8) {
            fm_put_char(x++, y, *name++, COL_FKEYS);
        }
        
        while (x < (i + 1) * 8 && x < VGA_WIDTH) {
            fm_put_char(x++, y, ' ', COL_FKEYS);
        }
    }
}

static void fm_draw_status(void) {
    int y = VGA_HEIGHT - 2;
    for (int x = 0; x < VGA_WIDTH; x++) {
        fm_put_char(x, y, ' ', COL_STATUS);
    }
    fm_put_string(1, y, fm_status, COL_STATUS);
}

static void fm_draw(void) {
    vga_clear();
    fm_draw_panel(&left_panel, 0, active_panel == 0);
    fm_draw_panel(&right_panel, 40, active_panel == 1);
    fm_draw_status();
    fm_draw_fkeys();
    vga_set_cursor(VGA_WIDTH * VGA_HEIGHT);
}

// ============== Действия ==============

static void fm_enter_dir(fm_panel* panel) {
    int total = panel->file_count + (panel->has_parent ? 1 : 0);
    if (panel->selected >= total) return;
    
    fs_node* target = NULL;
    
    if (panel->has_parent && panel->selected == 0) {
        target = panel->current_dir->parent;
    } else {
        int idx = panel->selected - (panel->has_parent ? 1 : 0);
        if (idx >= 0 && idx < panel->file_count) {
            target = panel->files[idx];
        }
    }
    
    if (target && target->type == FS_DIR) {
        panel->current_dir = target;
        panel->selected = 0;
        panel->scroll = 0;
        fm_build_path(panel);
        fm_refresh_panel(panel);
        strcpy(fm_status, "Changed to: ");
        strcat(fm_status, panel->path);
    }
}

// F1 - Help
static void fm_show_help(void) {
    vga_clear();
    vga_print_color("=============== AL-OS File Manager Help ===============\n\n", 0x0E);
    
    vga_print_color("Navigation:\n", 0x0B);
    vga_print_color("  Up/Down      - Move selection\n", 0x07);
    vga_print_color("  Enter        - Enter directory / View file\n", 0x07);
    vga_print_color("  Tab          - Switch between panels\n", 0x07);
    vga_print_color("  Home/End     - Go to first/last item\n", 0x07);
    vga_print_color("  PgUp/PgDn    - Page up/down\n", 0x07);
    
    vga_print_color("\nFunction Keys:\n", 0x0B);
    vga_print_color("  F1  - This help\n", 0x07);
    vga_print_color("  F2  - Rename file/directory\n", 0x07);
    vga_print_color("  F3  - View file content\n", 0x07);
    vga_print_color("  F4  - Edit file (opens nano)\n", 0x07);
    vga_print_color("  F5  - Copy to other panel\n", 0x07);
    vga_print_color("  F6  - Move to other panel\n", 0x07);
    vga_print_color("  F7  - Create directory\n", 0x07);
    vga_print_color("  F8  - Delete file/directory\n", 0x07);
    vga_print_color("  F9  - Create new file (touch)\n", 0x07);
    vga_print_color("  F10 - Exit (or ESC)\n", 0x07);
    
    vga_print_color("\n\nPress any key to return...", 0x0A);
    
    fm_wait_key_release();
    fm_wait_key();
    fm_wait_key_release();
    
    strcpy(fm_status, "Ready");
}

// F3 - View
static void fm_view_file(void) {
    fm_panel* panel = fm_get_active();
    fs_node* node = fm_get_selected_node(panel);
    
    if (!node || node->type != FS_FILE) {
        strcpy(fm_status, "Cannot view: not a file");
        return;
    }
    
    vga_clear();
    vga_print_color("=== Viewing: ", 0x0E);
    vga_print_color(node->name, 0x0F);
    vga_print_color(" ===\n\n", 0x0E);
    
    const char* content = node->content;
    int line = 0;
    int lines_per_page = VGA_HEIGHT - 4;
    
    while (*content) {
        vga_putc(*content);
        if (*content == '\n') {
            line++;
            if (line >= lines_per_page) {
                vga_print_color("\n-- Press any key for more, ESC to exit --", 0x0B);
                fm_wait_key_release();
                uint8_t sc = fm_wait_key();
                if (sc == KEY_ESC) break;
                vga_clear();
                line = 0;
            }
        }
        content++;
    }
    
    vga_print_color("\n\n-- Press any key to return --", 0x0B);
    fm_wait_key_release();
    fm_wait_key();
    fm_wait_key_release();
    
    strcpy(fm_status, "Viewed: ");
    strcat(fm_status, node->name);
}

// F2 - Rename
static void fm_rename(void) {
    fm_panel* panel = fm_get_active();
    fs_node* node = fm_get_selected_node(panel);
    
    if (!node) {
        strcpy(fm_status, "Nothing to rename");
        return;
    }
    
    char new_name[MAX_NAME_LEN];
    
    if (fm_input_dialog(" Rename ", "Enter new name:", new_name, MAX_NAME_LEN)) {
        if (new_name[0]) {
            for (int i = 0; i < panel->current_dir->child_count; i++) {
                if (panel->current_dir->children[i] != node &&
                    strcmp(panel->current_dir->children[i]->name, new_name) == 0) {
                    strcpy(fm_status, "Error: name already exists");
                    return;
                }
            }
            
            char old_name[MAX_NAME_LEN];
            strcpy(old_name, node->name);
            strcpy(node->name, new_name);
            
            strcpy(fm_status, "Renamed: ");
            strcat(fm_status, old_name);
            strcat(fm_status, " -> ");
            strcat(fm_status, new_name);
        } else {
            strcpy(fm_status, "Rename cancelled: empty name");
        }
    } else {
        strcpy(fm_status, "Rename cancelled");
    }
    
    fm_refresh_panel(panel);
}

// F9 - Touch (create file)
static void fm_touch(void) {
    fm_panel* panel = fm_get_active();
    
    char filename[MAX_NAME_LEN];
    
    if (fm_input_dialog(" Create File ", "Enter filename:", filename, MAX_NAME_LEN)) {
        if (filename[0]) {
            for (int i = 0; i < panel->current_dir->child_count; i++) {
                if (strcmp(panel->current_dir->children[i]->name, filename) == 0) {
                    strcpy(fm_status, "Error: file already exists");
                    return;
                }
            }
            
            fs_node* old_current = fs_current;
            fs_current = panel->current_dir;
            
            if (fs_touch(filename) == 0) {
                strcpy(fm_status, "Created file: ");
                strcat(fm_status, filename);
            } else {
                strcpy(fm_status, "Failed to create file");
            }
            
            fs_current = old_current;
            fm_refresh_panel(panel);
        } else {
            strcpy(fm_status, "Cancelled: empty name");
        }
    } else {
        strcpy(fm_status, "Cancelled");
    }
}

// F5 - Copy
static void fm_copy_file(void) {
    fm_panel* src_panel = fm_get_active();
    fm_panel* dst_panel = fm_get_inactive();
    
    fs_node* node = fm_get_selected_node(src_panel);
    if (!node) {
        strcpy(fm_status, "Nothing to copy");
        return;
    }
    
    if (node->type != FS_FILE) {
        strcpy(fm_status, "Cannot copy directories (yet)");
        return;
    }
    
    char new_name[MAX_NAME_LEN];
    strcpy(new_name, node->name);
    
    char prompt[64];
    strcpy(prompt, "Copy to: ");
    strcat(prompt, dst_panel->path);

    for (int i = 0; i < dst_panel->current_dir->child_count; i++) {
        if (strcmp(dst_panel->current_dir->children[i]->name, node->name) == 0) {
            if (!fm_input_dialog(" Copy ", "File exists! New name:", new_name, MAX_NAME_LEN)) {
                strcpy(fm_status, "Copy cancelled");
                return;
            }
            if (!new_name[0] || strcmp(new_name, node->name) == 0) {
                strcpy(fm_status, "Copy cancelled: same name");
                return;
            }
            break;
        }
    }
    
    fs_node* old_current = fs_current;
    fs_current = dst_panel->current_dir;
    fs_touch(new_name);
    
    fs_node* new_file = resolve_path(new_name, dst_panel->current_dir);
    if (new_file && new_file->type == FS_FILE) {
        strcpy(new_file->content, node->content);
        strcpy(fm_status, "Copied: ");
        strcat(fm_status, node->name);
        if (strcmp(new_name, node->name) != 0) {
            strcat(fm_status, " -> ");
            strcat(fm_status, new_name);
        }
    } else {
        strcpy(fm_status, "Copy failed");
    }
    
    fs_current = old_current;
    fm_refresh_panel(dst_panel);
}

// F6 - Move
static void fm_move_file(void) {
    fm_panel* src_panel = fm_get_active();
    fm_panel* dst_panel = fm_get_inactive();
    
    fs_node* node = fm_get_selected_node(src_panel);
    if (!node) {
        strcpy(fm_status, "Nothing to move");
        return;
    }
    
    for (int i = 0; i < dst_panel->current_dir->child_count; i++) {
        if (strcmp(dst_panel->current_dir->children[i]->name, node->name) == 0) {
            strcpy(fm_status, "Error: file exists in destination");
            return;
        }
    }
    
    if (node->type == FS_DIR) {
        fs_node* check = dst_panel->current_dir;
        while (check) {
            if (check == node) {
                strcpy(fm_status, "Cannot move directory into itself");
                return;
            }
            check = check->parent;
        }
    }

    fs_node* old_parent = node->parent;
    for (int i = 0; i < old_parent->child_count; i++) {
        if (old_parent->children[i] == node) {
            for (int j = i; j < old_parent->child_count - 1; j++) {
                old_parent->children[j] = old_parent->children[j + 1];
            }
            old_parent->child_count--;
            break;
        }
    }
    
    dst_panel->current_dir->children[dst_panel->current_dir->child_count++] = node;
    node->parent = dst_panel->current_dir;
    
    fm_refresh_panel(src_panel);
    fm_refresh_panel(dst_panel);
    
    strcpy(fm_status, "Moved: ");
    strcat(fm_status, node->name);
}

// F7 - MkDir
static void fm_make_dir(void) {
    fm_panel* panel = fm_get_active();
    
    char dirname[MAX_NAME_LEN];
    
    if (fm_input_dialog(" Create Directory ", "Enter directory name:", dirname, MAX_NAME_LEN)) {
        if (dirname[0]) {
            for (int i = 0; i < panel->current_dir->child_count; i++) {
                if (strcmp(panel->current_dir->children[i]->name, dirname) == 0) {
                    strcpy(fm_status, "Error: already exists");
                    return;
                }
            }
            
            fs_node* old_current = fs_current;
            fs_current = panel->current_dir;
            
            if (fs_mkdir(dirname) == 0) {
                strcpy(fm_status, "Created directory: ");
                strcat(fm_status, dirname);
            } else {
                strcpy(fm_status, "Failed to create directory");
            }
            
            fs_current = old_current;
            fm_refresh_panel(panel);
        } else {
            strcpy(fm_status, "Cancelled: empty name");
        }
    } else {
        strcpy(fm_status, "Cancelled");
    }
}

// F8 - Delete
static void fm_delete(void) {
    fm_panel* panel = fm_get_active();
    fs_node* node = fm_get_selected_node(panel);
    
    if (!node) {
        strcpy(fm_status, "Nothing to delete");
        return;
    }
    
    int dialog_w = 50;
    int dialog_h = 6;
    int dialog_x = (VGA_WIDTH - dialog_w) / 2;
    int dialog_y = (VGA_HEIGHT - dialog_h) / 2;
    
    fm_draw_dialog_box(dialog_x, dialog_y, dialog_w, dialog_h, " Delete ", 0x4F);
    
    fm_put_string(dialog_x + 2, dialog_y + 2, "Delete: ", 0x4F);
    fm_put_string(dialog_x + 10, dialog_y + 2, node->name, 0x4E);
    fm_put_string(dialog_x + 2, dialog_y + 4, "Press Y to confirm, any other to cancel", 0x4F);
    
    fm_wait_key_release();
    uint8_t sc = fm_wait_key();
    fm_wait_key_release();
    
    if (sc != 0x15) {
        strcpy(fm_status, "Deletion cancelled");
        return;
    }
    
    if (node->type == FS_DIR && node->child_count > 0) {
        strcpy(fm_status, "Cannot delete: directory not empty");
        return;
    }
    
    fs_node* parent = node->parent;
    for (int i = 0; i < parent->child_count; i++) {
        if (parent->children[i] == node) {
            for (int j = i; j < parent->child_count - 1; j++) {
                parent->children[j] = parent->children[j + 1];
            }
            parent->child_count--;
            break;
        }
    }
    
    fm_refresh_panel(panel);
    
    strcpy(fm_status, "Deleted: ");
    strcat(fm_status, node->name);
}

// F4 - Edit
static void fm_edit_file(void) {
    fm_panel* panel = fm_get_active();
    fs_node* node = fm_get_selected_node(panel);
    
    if (!node || node->type != FS_FILE) {
        strcpy(fm_status, "Cannot edit: not a file");
        return;
    }
    
    char full_path[256];
    strcpy(full_path, panel->path);
    if (full_path[strlen(full_path) - 1] != '/') {
        strcat(full_path, "/");
    }
    strcat(full_path, node->name);
    
    extern void nano_edit(const char* filename);
    nano_edit(full_path);
    
    fm_refresh_panel(panel);
    fm_wait_key_release();
    
    strcpy(fm_status, "Edited: ");
    strcat(fm_status, node->name);
}

static void fm_handle_input(void) {
    uint8_t scancode;
    do {
        if (!fm_kbd_has_data()) continue;
        scancode = fm_read_scancode_raw();
    } while (scancode & 0x80);
    
    fm_panel* panel = fm_get_active();
    int total_items = panel->file_count + (panel->has_parent ? 1 : 0);
    
    switch (scancode) {
        case KEY_UP:
            if (panel->selected > 0) {
                panel->selected--;
            }
            break;
            
        case KEY_DOWN:
            if (panel->selected < total_items - 1) {
                panel->selected++;
            }
            break;
            
        case KEY_HOME:
            panel->selected = 0;
            panel->scroll = 0;
            break;
            
        case KEY_END:
            panel->selected = total_items > 0 ? total_items - 1 : 0;
            break;
            
        case KEY_PGUP:
            panel->selected -= FM_PANEL_HEIGHT - 2;
            if (panel->selected < 0) panel->selected = 0;
            break;
            
        case KEY_PGDN:
            panel->selected += FM_PANEL_HEIGHT - 2;
            if (panel->selected >= total_items) 
                panel->selected = total_items > 0 ? total_items - 1 : 0;
            break;
            
        case KEY_TAB:
            active_panel = 1 - active_panel;
            strcpy(fm_status, active_panel == 0 ? "Left panel" : "Right panel");
            break;
            
        case KEY_ENTER:
            {
                fs_node* node = fm_get_selected_node(panel);
                if (node && node->type == FS_FILE) {
                    fm_view_file();
                } else {
                    fm_enter_dir(panel);
                }
            }
            break;
            
        case KEY_ESC:
        case KEY_F10:
            fm_running = 0;
            break;
            
        case KEY_F1:
            fm_show_help();
            break;
            
        case KEY_F2:
            fm_rename();
            break;
            
        case KEY_F3:
            fm_view_file();
            break;
            
        case KEY_F4:
            fm_edit_file();
            break;
            
        case KEY_F5:
            fm_copy_file();
            break;
            
        case KEY_F6:
            fm_move_file();
            break;
            
        case KEY_F7:
            fm_make_dir();
            break;
            
        case KEY_F8:
            fm_delete();
            break;
            
        case KEY_F9:
            fm_touch();
            break;
    }
    
    if (scancode >= KEY_F1 && scancode <= KEY_F12) {
        fm_wait_key_release();
    }
}


void fm_run(void) {
    fs_node* saved_current = fs_current;
    uint8_t saved_color = vga_color;
    
    active_panel = 0;
    fm_running = 1;
    fm_need_redraw = 1;
    strcpy(fm_status, "F1=Help | Tab=Switch | F10/ESC=Exit");
    
    fm_init_panel(&left_panel, fs_current);
    fm_init_panel(&right_panel, fs_root);
    
    fm_kbd_flush();
    
    while (fm_running) {
        fm_draw();
        fm_handle_input();
    }
    
    fs_current = saved_current;
    vga_color = saved_color;
    vga_clear();
    
    vga_print_color("File Manager closed.\n", 0x0A);
}