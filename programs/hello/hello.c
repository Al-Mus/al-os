/*
 * Hello world for AL-OS
 * Compile: i686-elf-gcc -ffreestanding -nostdlib -fno-pie -Ttext=0x200000 -o hello.elf hello.c
 */

#include <stdint.h>

/* Syscall table at fixed address */
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

int _start(void) {
    syscall_table_t* sys = SYSCALL_TABLE;
    
    /* Check magic */
    if (sys->magic != SYSCALL_MAGIC) {
        return -1;
    }
    
    sys->print_color("================================\n", 0x0B);
    sys->print_color("  Hello from ELF program!\n", 0x0A);
    sys->print_color("================================\n", 0x0B);
    sys->print("\n");
    sys->print("Loaded from FAT filesystem.\n");
    sys->print("Using kernel syscalls.\n");
    sys->print("\n");
    sys->print_color("Press any key...", 0x0E);
    sys->getchar();
    sys->print("\n");
    
    return 0;
}