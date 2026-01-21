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

int _start(void) {
    syscall_table_t* sys = SYSCALL_TABLE;
    
    if (sys->magic != SYSCALL_MAGIC) {
        return -1;
    }
    
    sys->print_color("\n=== Hello from AL-OS! ===\n\n", 0x0A);
    
    sys->print("Syscall API version: ");
    char v = '0' + sys->version;
    sys->putchar(v);
    sys->putchar('\n');
    
    sys->print("Screen: ");
    int w = sys->get_screen_width();
    int h = sys->get_screen_height();
    sys->putchar('0' + w / 10);
    sys->putchar('0' + w % 10);
    sys->putchar('x');
    sys->putchar('0' + h / 10);
    sys->putchar('0' + h % 10);
    sys->putchar('\n');
    
    sys->print_color("\nPress any key to exit...\n", 0x0E);
    sys->getchar();
    
    sys->print_color("Goodbye!\n", 0x0B);
    
    return 0;
}