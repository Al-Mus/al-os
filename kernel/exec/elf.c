#include "elf.h"
#include "../fs/fat.h"
#include "../drivers/vga.h"
#include "../drivers/keyboard.h"
#include "../utils/string.h"

/* ==================== Configuration ==================== */

#define ELF_MAX_FILE_SIZE   (512 * 1024)

static uint8_t elf_buffer[ELF_MAX_FILE_SIZE];

/* ==================== Syscall Table at Fixed Address ==================== */

#define SYSCALL_TABLE_ADDR  0x100000
#define SYSCALL_MAGIC_VALUE 0xA105C411

/* Syscall implementations */
static void sys_print(const char* str) {
    vga_print(str);
}

static void sys_print_color(const char* str, uint8_t color) {
    vga_print_color(str, color);
}

static void sys_putchar(char c) {
    vga_putc(c);
}

static void sys_clear(void) {
    vga_clear();
}

static char sys_getchar(void) {
    char c = 0;
    while (c == 0) {
        c = keyboard_read_char();
    }
    return c;
}

static void sys_read_line(char* buf, int max) {
    keyboard_read_line(buf, max);
}

static void sys_sleep(uint32_t ms) {
    for (volatile uint32_t i = 0; i < ms * 5000; i++);
}

static uint32_t sys_get_ticks(void) {
    static uint32_t t = 0;
    return t++;
}

/* Setup syscall table at fixed memory address */
static void setup_syscall_table(void) {
    syscall_table_t* table = (syscall_table_t*)SYSCALL_TABLE_ADDR;
    
    table->magic = SYSCALL_MAGIC_VALUE;
    table->version = 1;
    table->print = sys_print;
    table->print_color = sys_print_color;
    table->putchar = sys_putchar;
    table->clear = sys_clear;
    table->getchar = sys_getchar;
    table->read_line = sys_read_line;
    table->sleep = sys_sleep;
    table->get_ticks = sys_get_ticks;
}

/* ==================== Validation ==================== */

elf_error_t elf_validate(const void* data, uint32_t size) {
    if (size < sizeof(Elf32_Ehdr)) {
        return ELF_ERR_NOT_ELF;
    }
    
    const Elf32_Ehdr* ehdr = (const Elf32_Ehdr*)data;
    
    if (ehdr->e_ident[EI_MAG0] != 0x7F ||
        ehdr->e_ident[EI_MAG1] != 'E' ||
        ehdr->e_ident[EI_MAG2] != 'L' ||
        ehdr->e_ident[EI_MAG3] != 'F') {
        return ELF_ERR_NOT_ELF;
    }
    
    if (ehdr->e_ident[EI_CLASS] != ELFCLASS32) {
        return ELF_ERR_NOT_32BIT;
    }
    
    if (ehdr->e_ident[EI_DATA] != ELFDATA2LSB) {
        return ELF_ERR_NOT_LITTLE_ENDIAN;
    }
    
    if (ehdr->e_type != ET_EXEC) {
        return ELF_ERR_NOT_EXECUTABLE;
    }
    
    if (ehdr->e_machine != EM_386) {
        return ELF_ERR_WRONG_ARCH;
    }
    
    if (ehdr->e_phnum == 0) {
        return ELF_ERR_NO_SEGMENTS;
    }
    
    return ELF_OK;
}

/* ==================== Info ==================== */

elf_error_t elf_get_info(const void* data, uint32_t size, elf_info_t* info) {
    elf_error_t err = elf_validate(data, size);
    if (err != ELF_OK) return err;
    
    const Elf32_Ehdr* ehdr = (const Elf32_Ehdr*)data;
    const Elf32_Phdr* phdr = (const Elf32_Phdr*)((uint8_t*)data + ehdr->e_phoff);
    
    info->entry_point = ehdr->e_entry;
    info->load_addr = 0xFFFFFFFF;
    info->load_end = 0;
    info->bss_end = 0;
    
    for (uint16_t i = 0; i < ehdr->e_phnum; i++) {
        if (phdr[i].p_type == PT_LOAD) {
            if (phdr[i].p_vaddr < info->load_addr) {
                info->load_addr = phdr[i].p_vaddr;
            }
            uint32_t seg_end = phdr[i].p_vaddr + phdr[i].p_filesz;
            if (seg_end > info->load_end) {
                info->load_end = seg_end;
            }
            uint32_t mem_end = phdr[i].p_vaddr + phdr[i].p_memsz;
            if (mem_end > info->bss_end) {
                info->bss_end = mem_end;
            }
        }
    }
    
    return ELF_OK;
}

/* ==================== Loading ==================== */

elf_error_t elf_load(const void* data, uint32_t size, uint32_t* entry) {
    elf_error_t err = elf_validate(data, size);
    if (err != ELF_OK) return err;
    
    const Elf32_Ehdr* ehdr = (const Elf32_Ehdr*)data;
    const Elf32_Phdr* phdr = (const Elf32_Phdr*)((uint8_t*)data + ehdr->e_phoff);
    
    for (uint16_t i = 0; i < ehdr->e_phnum; i++) {
        if (phdr[i].p_type != PT_LOAD) continue;
        
        uint32_t vaddr = phdr[i].p_vaddr;
        uint32_t filesz = phdr[i].p_filesz;
        uint32_t memsz = phdr[i].p_memsz;
        uint32_t offset = phdr[i].p_offset;
        
        if (offset + filesz > size) {
            return ELF_ERR_LOAD_FAILED;
        }
        
        uint8_t* dest = (uint8_t*)vaddr;
        const uint8_t* src = (const uint8_t*)data + offset;
        
        for (uint32_t j = 0; j < filesz; j++) {
            dest[j] = src[j];
        }
        
        for (uint32_t j = filesz; j < memsz; j++) {
            dest[j] = 0;
        }
    }
    
    *entry = ehdr->e_entry;
    return ELF_OK;
}

/* ==================== Execute ==================== */

typedef int (*elf_entry_fn)(void);

int elf_exec(const char* path) {
    if (!fat_is_mounted()) {
        vga_print_color("Error: No filesystem mounted\n", 0x0C);
        return -1;
    }
    
    int bytes_read = fat_read(path, elf_buffer, ELF_MAX_FILE_SIZE);
    if (bytes_read < 0) {
        vga_print_color("Error: File not found: ", 0x0C);
        vga_print_color(path, 0x0C);
        vga_putc('\n');
        return -1;
    }
    
    if (bytes_read < (int)sizeof(Elf32_Ehdr)) {
        vga_print_color("Error: File too small\n", 0x0C);
        return -1;
    }
    
    elf_error_t err = elf_validate(elf_buffer, bytes_read);
    if (err != ELF_OK) {
        vga_print_color("Error: ", 0x0C);
        vga_print_color(elf_strerror(err), 0x0C);
        vga_putc('\n');
        return -1;
    }
    
    vga_print_color("Running: ", 0x0A);
    vga_print_color(path, 0x0E);
    vga_putc('\n');
    
    /* Setup syscalls BEFORE loading program */
    setup_syscall_table();
    
    /* Load ELF */
    uint32_t entry;
    err = elf_load(elf_buffer, bytes_read, &entry);
    if (err != ELF_OK) {
        vga_print_color("Load error: ", 0x0C);
        vga_print_color(elf_strerror(err), 0x0C);
        vga_putc('\n');
        return -1;
    }
    
    /* Call program - no arguments */
    elf_entry_fn program = (elf_entry_fn)entry;
    int result = program();
    
    vga_print_color("Exit code: ", 0x08);
    char buf[16];
    itoa(result, buf, 10);
    vga_print_color(buf, 0x0F);
    vga_putc('\n');
    
    return result;
}

/* ==================== Error Strings ==================== */

const char* elf_strerror(elf_error_t err) {
    switch (err) {
        case ELF_OK:                    return "Success";
        case ELF_ERR_NOT_ELF:           return "Not an ELF file";
        case ELF_ERR_NOT_32BIT:         return "Not 32-bit";
        case ELF_ERR_NOT_LITTLE_ENDIAN: return "Wrong endianness";
        case ELF_ERR_NOT_EXECUTABLE:    return "Not executable";
        case ELF_ERR_WRONG_ARCH:        return "Wrong arch (need i386)";
        case ELF_ERR_NO_SEGMENTS:       return "No segments";
        case ELF_ERR_LOAD_FAILED:       return "Load failed";
        case ELF_ERR_FILE_NOT_FOUND:    return "File not found";
        case ELF_ERR_FILE_READ:         return "Read error";
        case ELF_ERR_NO_MEMORY:         return "No memory";
        default:                        return "Unknown error";
    }
}