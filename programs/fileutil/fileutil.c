#include <stdint.h>

#define SYSCALL_TABLE ((syscall_table_t*)0x100000)
#define SYSCALL_MAGIC 0xA105C411

typedef struct {
    uint32_t    magic;
    uint32_t    version;
    
    void        (*print)(const char* str);
    void        (*print_color)(const char* str, uint8_t color);
    void        (*putchar)(char c);
    void        (*clear)(void);
    char        (*getchar)(void);
    void        (*read_line)(char* buf, int max);
    void        (*sleep)(uint32_t ms);
    uint32_t    (*get_ticks)(void);
    
    int         (*file_exists)(const char* path);
    int         (*file_read)(const char* path, void* buf, uint32_t max_size);
    int         (*file_write)(const char* path, const void* data, uint32_t size);
    int         (*file_remove)(const char* path);
    int         (*file_mkdir)(const char* path);
    int         (*is_dir)(const char* path);
    int         (*list_dir)(const char* path, void (*callback)(const char* name, uint32_t size, uint8_t is_dir));
    
    void        (*set_cursor)(int x, int y);
    void        (*get_cursor)(int* x, int* y);
    int         (*get_screen_width)(void);
    int         (*get_screen_height)(void);
    
    int         (*key_pressed)(void);
    int         (*get_key_nonblock)(void);
    
    void*       (*malloc)(uint32_t size);
    void        (*free)(void* ptr);
} syscall_table_t;

static syscall_table_t* sys;

static int str_len(const char* s) {
    int len = 0;
    while (s[len]) len++;
    return len;
}

static int str_cmp(const char* a, const char* b) {
    while (*a && *a == *b) { a++; b++; }
    return *a - *b;
}

static void str_cpy(char* dst, const char* src) {
    while (*src) *dst++ = *src++;
    *dst = 0;
}

static char* str_chr(const char* s, char c) {
    while (*s) {
        if (*s == c) return (char*)s;
        s++;
    }
    return 0;
}

static void print_num(int n) {
    if (n < 0) {
        sys->putchar('-');
        n = -n;
    }
    if (n >= 10) print_num(n / 10);
    sys->putchar('0' + (n % 10));
}

static void show_help(void) {
    sys->print_color("\n=== FileUtil v1.0 ===\n", 0x0E);
    sys->print_color("File management utility for AL-OS\n\n", 0x07);
    sys->print_color("Commands:\n", 0x0F);
    sys->print("  ls [path]        - List directory\n");
    sys->print("  cat <file>       - Display file contents\n");
    sys->print("  write <f> <txt>  - Write text to file\n");
    sys->print("  touch <file>     - Create empty file\n");
    sys->print("  rm <file>        - Remove file\n");
    sys->print("  mkdir <dir>      - Create directory\n");
    sys->print("  exists <path>    - Check if path exists\n");
    sys->print("  isdir <path>     - Check if path is directory\n");
    sys->print("  copy <src> <dst> - Copy file\n");
    sys->print("  hex <file>       - Hexdump file\n");
    sys->print("  info             - Show system info\n");
    sys->print("  clear            - Clear screen\n");
    sys->print("  help             - Show this help\n");
    sys->print("  exit             - Exit program\n\n");
}

static void cmd_ls(const char* path) {
    if (path && *path) {
        if (!sys->file_exists(path)) {
            sys->print_color("Path not found\n", 0x0C);
            return;
        }
        if (!sys->is_dir(path)) {
            sys->print_color("Not a directory\n", 0x0C);
            return;
        }
    }
    
    sys->list_dir(path, 0);
}

static void cmd_cat(const char* path) {
    if (!path || !*path) {
        sys->print_color("Usage: cat <file>\n", 0x0C);
        return;
    }
    
    if (!sys->file_exists(path)) {
        sys->print_color("File not found: ", 0x0C);
        sys->print(path);
        sys->putchar('\n');
        return;
    }
    
    if (sys->is_dir(path)) {
        sys->print_color("Cannot cat a directory\n", 0x0C);
        return;
    }
    
    char* buf = (char*)sys->malloc(4096);
    if (!buf) {
        sys->print_color("Out of memory\n", 0x0C);
        return;
    }
    
    int size = sys->file_read(path, buf, 4095);
    if (size < 0) {
        sys->print_color("Read error\n", 0x0C);
        sys->free(buf);
        return;
    }
    
    buf[size] = 0;
    sys->print(buf);
    if (size > 0 && buf[size-1] != '\n') {
        sys->putchar('\n');
    }
    
    sys->free(buf);
}

static void cmd_write(const char* args) {
    if (!args || !*args) {
        sys->print_color("Usage: write <file> <text>\n", 0x0C);
        return;
    }
    
    char* space = str_chr(args, ' ');
    if (!space) {
        sys->print_color("Usage: write <file> <text>\n", 0x0C);
        return;
    }
    
    int path_len = space - args;
    char path[256];
    for (int i = 0; i < path_len && i < 255; i++) {
        path[i] = args[i];
    }
    path[path_len] = 0;
    
    const char* text = space + 1;
    while (*text == ' ') text++;
    
    if (sys->file_write(path, text, str_len(text)) == 0) {
        sys->print_color("Written to: ", 0x0A);
        sys->print(path);
        sys->putchar('\n');
    } else {
        sys->print_color("Write failed\n", 0x0C);
    }
}

static void cmd_touch(const char* path) {
    if (!path || !*path) {
        sys->print_color("Usage: touch <file>\n", 0x0C);
        return;
    }
    
    if (sys->file_write(path, "", 0) == 0) {
        sys->print_color("Created: ", 0x0A);
        sys->print(path);
        sys->putchar('\n');
    } else {
        sys->print_color("Failed to create file\n", 0x0C);
    }
}

static void cmd_rm(const char* path) {
    if (!path || !*path) {
        sys->print_color("Usage: rm <file>\n", 0x0C);
        return;
    }
    
    if (!sys->file_exists(path)) {
        sys->print_color("Not found: ", 0x0C);
        sys->print(path);
        sys->putchar('\n');
        return;
    }
    
    if (sys->file_remove(path) == 0) {
        sys->print_color("Removed: ", 0x0A);
        sys->print(path);
        sys->putchar('\n');
    } else {
        sys->print_color("Remove failed\n", 0x0C);
    }
}

static void cmd_mkdir(const char* path) {
    if (!path || !*path) {
        sys->print_color("Usage: mkdir <dir>\n", 0x0C);
        return;
    }
    
    if (sys->file_mkdir(path) == 0) {
        sys->print_color("Directory created: ", 0x0A);
        sys->print(path);
        sys->putchar('\n');
    } else {
        sys->print_color("Failed to create directory\n", 0x0C);
    }
}

static void cmd_exists(const char* path) {
    if (!path || !*path) {
        sys->print_color("Usage: exists <path>\n", 0x0C);
        return;
    }
    
    if (sys->file_exists(path)) {
        sys->print_color("EXISTS: ", 0x0A);
    } else {
        sys->print_color("NOT FOUND: ", 0x0C);
    }
    sys->print(path);
    sys->putchar('\n');
}

static void cmd_isdir(const char* path) {
    if (!path || !*path) {
        sys->print_color("Usage: isdir <path>\n", 0x0C);
        return;
    }
    
    if (!sys->file_exists(path)) {
        sys->print_color("Path not found\n", 0x0C);
        return;
    }
    
    if (sys->is_dir(path)) {
        sys->print_color("YES, is directory: ", 0x0A);
    } else {
        sys->print_color("NO, is file: ", 0x0E);
    }
    sys->print(path);
    sys->putchar('\n');
}

static void cmd_copy(const char* args) {
    if (!args || !*args) {
        sys->print_color("Usage: copy <src> <dst>\n", 0x0C);
        return;
    }
    
    char* space = str_chr(args, ' ');
    if (!space) {
        sys->print_color("Usage: copy <src> <dst>\n", 0x0C);
        return;
    }
    
    int src_len = space - args;
    char src[256], dst[256];
    for (int i = 0; i < src_len && i < 255; i++) src[i] = args[i];
    src[src_len] = 0;
    
    const char* d = space + 1;
    while (*d == ' ') d++;
    str_cpy(dst, d);
    
    if (!sys->file_exists(src)) {
        sys->print_color("Source not found\n", 0x0C);
        return;
    }
    
    char* buf = (char*)sys->malloc(4096);
    if (!buf) {
        sys->print_color("Out of memory\n", 0x0C);
        return;
    }
    
    int size = sys->file_read(src, buf, 4096);
    if (size < 0) {
        sys->print_color("Read error\n", 0x0C);
        sys->free(buf);
        return;
    }
    
    if (sys->file_write(dst, buf, size) == 0) {
        sys->print_color("Copied ", 0x0A);
        print_num(size);
        sys->print(" bytes\n");
    } else {
        sys->print_color("Write error\n", 0x0C);
    }
    
    sys->free(buf);
}

static void cmd_hex(const char* path) {
    if (!path || !*path) {
        sys->print_color("Usage: hex <file>\n", 0x0C);
        return;
    }
    
    if (!sys->file_exists(path)) {
        sys->print_color("File not found\n", 0x0C);
        return;
    }
    
    uint8_t* buf = (uint8_t*)sys->malloc(256);
    if (!buf) {
        sys->print_color("Out of memory\n", 0x0C);
        return;
    }
    
    int size = sys->file_read(path, buf, 256);
    if (size < 0) {
        sys->print_color("Read error\n", 0x0C);
        sys->free(buf);
        return;
    }
    
    const char* hex = "0123456789ABCDEF";
    
    for (int i = 0; i < size; i++) {
        if (i % 16 == 0) {
            if (i > 0) sys->putchar('\n');
        }
        sys->putchar(hex[buf[i] >> 4]);
        sys->putchar(hex[buf[i] & 0xF]);
        sys->putchar(' ');
    }
    sys->putchar('\n');
    
    sys->print_color("Size: ", 0x0E);
    print_num(size);
    sys->print(" bytes\n");
    
    sys->free(buf);
}

static void cmd_info(void) {
    sys->print_color("\n=== System Info ===\n", 0x0E);
    
    sys->print("API Version: ");
    print_num(sys->version);
    sys->putchar('\n');
    
    sys->print("Screen: ");
    print_num(sys->get_screen_width());
    sys->putchar('x');
    print_num(sys->get_screen_height());
    sys->putchar('\n');
    
    int x, y;
    sys->get_cursor(&x, &y);
    sys->print("Cursor pos: ");
    print_num(x);
    sys->print(", ");
    print_num(y);
    sys->putchar('\n');
    
    sys->putchar('\n');
}

int _start(void) {
    sys = SYSCALL_TABLE;
    
    if (sys->magic != SYSCALL_MAGIC) {
        return -1;
    }
    
    sys->print_color("FileUtil v1.0 - Type 'help' for commands, 'exit' to quit\n\n", 0x0B);
    
    char line[256];
    
    while (1) {
        sys->print_color("fileutil> ", 0x0A);
        sys->read_line(line, sizeof(line));
        
        if (line[0] == 0) continue;
        
        char* space = str_chr(line, ' ');
        char* args = 0;
        if (space) {
            *space = 0;
            args = space + 1;
            while (*args == ' ') args++;
        }
        
        if (str_cmp(line, "exit") == 0 || str_cmp(line, "quit") == 0) {
            sys->print_color("Goodbye!\n", 0x0E);
            return 0;
        }
        else if (str_cmp(line, "help") == 0) show_help();
        else if (str_cmp(line, "ls") == 0) cmd_ls(args);
        else if (str_cmp(line, "cat") == 0) cmd_cat(args);
        else if (str_cmp(line, "write") == 0) cmd_write(args);
        else if (str_cmp(line, "touch") == 0) cmd_touch(args);
        else if (str_cmp(line, "rm") == 0) cmd_rm(args);
        else if (str_cmp(line, "mkdir") == 0) cmd_mkdir(args);
        else if (str_cmp(line, "exists") == 0) cmd_exists(args);
        else if (str_cmp(line, "isdir") == 0) cmd_isdir(args);
        else if (str_cmp(line, "copy") == 0) cmd_copy(args);
        else if (str_cmp(line, "hex") == 0) cmd_hex(args);
        else if (str_cmp(line, "info") == 0) cmd_info();
        else if (str_cmp(line, "clear") == 0) sys->clear();
        else {
            sys->print_color("Unknown: ", 0x0C);
            sys->print(line);
            sys->print(" - type 'help'\n");
        }
    }
    
    return 0;
}