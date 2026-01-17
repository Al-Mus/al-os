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
} syscall_table_t;

static syscall_table_t* sys;

static int streq(const char* a, const char* b) {
    while (*a && *b && *a == *b) { a++; b++; }
    return (*a == *b);
}

int _start(void) {
    sys = SYSCALL_TABLE;
    if (sys->magic != SYSCALL_MAGIC) return -1;
    
    char cmd[64];
    
    sys->print_color("Mini-Shell v1.0\n", 0x0E);
    sys->print("Type 'exit' to quit\n\n");
    
    while (1) {
        sys->print_color("$ ", 0x0B);
        sys->read_line(cmd, 64);
        
        if (streq(cmd, "exit")) {
            sys->print("Bye!\n");
            break;
        }
        else if (streq(cmd, "help")) {
            sys->print("Commands: help, hello, clear, exit\n");
        }
        else if (streq(cmd, "hello")) {
            sys->print_color("Hello World!\n", 0x0A);
        }
        else if (streq(cmd, "clear")) {
            sys->clear();
        }
        else if (cmd[0]) {
            sys->print("Unknown: ");
            sys->print(cmd);
            sys->print("\n");
        }
    }
    
    return 0;
}