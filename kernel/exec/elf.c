#include "elf.h"
#include "../fs/fat.h"
#include "../drivers/vga.h"
#include "../drivers/keyboard.h"
#include "../utils/string.h"

#define ELF_MAX_FILE_SIZE   (512 * 1024)

static uint8_t elf_buffer[ELF_MAX_FILE_SIZE];

#define KERNEL_RESERVED_END 0x110000
#define SAFE_LOAD_MAX       0xA00000

#define HEAP_SIZE           0x100000
static uint8_t program_heap[HEAP_SIZE];
static uint32_t heap_offset = 0;

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

static int sys_file_exists(const char* path) {
    return fat_exists(path);
}

static int sys_file_read(const char* path, void* buf, uint32_t max_size) {
    return fat_read(path, buf, max_size);
}

static int sys_file_write(const char* path, const void* data, uint32_t size) {
    return fat_write(path, data, size);
}

static int sys_file_remove(const char* path) {
    return fat_rm(path);
}

static int sys_file_mkdir(const char* path) {
    return fat_mkdir(path);
}

static int sys_is_dir(const char* path) {
    return fat_is_dir(path);
}

static int sys_list_dir(const char* path, void (*callback)(const char* name, uint32_t size, uint8_t is_dir)) {
    (void)callback;
    if (!fat_is_mounted()) return -1;
    fat_ls(path);
    return 0;
}

static void sys_set_cursor(int x, int y) {
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x >= VGA_WIDTH) x = VGA_WIDTH - 1;
    if (y >= VGA_HEIGHT) y = VGA_HEIGHT - 1;
    uint16_t pos = y * VGA_WIDTH + x;
    vga_set_cursor(pos);
}

static void sys_get_cursor(int* x, int* y) {
    uint16_t pos = vga_get_cursor();
    if (x) *x = pos % VGA_WIDTH;
    if (y) *y = pos / VGA_WIDTH;
}

static int sys_get_screen_width(void) {
    return VGA_WIDTH;
}

static int sys_get_screen_height(void) {
    return VGA_HEIGHT;
}

static int sys_key_pressed(void) {
    return keyboard_has_key();
}

static int sys_get_key_nonblock(void) {
    if (!keyboard_has_key()) return 0;
    return keyboard_read_char();
}

static void* sys_malloc(uint32_t size) {
    size = (size + 3) & ~3;
    if (heap_offset + size > HEAP_SIZE) return (void*)0;
    void* ptr = &program_heap[heap_offset];
    heap_offset += size;
    return ptr;
}

static void sys_free(void* ptr) {
    (void)ptr;
}

static void setup_syscall_table(void) {
    syscall_table_t* table = (syscall_table_t*)SYSCALL_TABLE_ADDR;
    
    table->magic = SYSCALL_MAGIC_VALUE;
    table->version = 3;
    
    table->print = sys_print;
    table->print_color = sys_print_color;
    table->putchar = sys_putchar;
    table->clear = sys_clear;
    table->getchar = sys_getchar;
    table->read_line = sys_read_line;
    table->sleep = sys_sleep;
    table->get_ticks = sys_get_ticks;
    
    table->file_exists = sys_file_exists;
    table->file_read = sys_file_read;
    table->file_write = sys_file_write;
    table->file_remove = sys_file_remove;
    table->file_mkdir = sys_file_mkdir;
    table->is_dir = sys_is_dir;
    table->list_dir = sys_list_dir;
    
    table->set_cursor = sys_set_cursor;
    table->get_cursor = sys_get_cursor;
    table->get_screen_width = sys_get_screen_width;
    table->get_screen_height = sys_get_screen_height;
    
    table->key_pressed = sys_key_pressed;
    table->get_key_nonblock = sys_get_key_nonblock;
    
    table->malloc = sys_malloc;
    table->free = sys_free;
}

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

elf_error_t elf_load(const void* data, uint32_t size, uint32_t* entry) {
    elf_error_t err = elf_validate(data, size);
    if (err != ELF_OK) return err;
    
    const Elf32_Ehdr* ehdr = (const Elf32_Ehdr*)data;
    const Elf32_Phdr* phdr = (const Elf32_Phdr*)((uint8_t*)data + ehdr->e_phoff);
    
    for (uint16_t i = 0; i < ehdr->e_phnum; i++) {
        if (phdr[i].p_type != PT_LOAD) continue;
        if (phdr[i].p_memsz == 0) continue;
        
        uint32_t vaddr = phdr[i].p_vaddr;
        uint32_t memsz = phdr[i].p_memsz;
        uint32_t end_addr = vaddr + memsz;
        
        if (vaddr < KERNEL_RESERVED_END || end_addr > SAFE_LOAD_MAX) {
            return ELF_ERR_LOAD_FAILED;
        }
    }
    
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
    
    heap_offset = 0;
    
    setup_syscall_table();
    
    uint32_t entry;
    err = elf_load(elf_buffer, bytes_read, &entry);
    if (err != ELF_OK) {
        vga_print_color("Load error: ", 0x0C);
        vga_print_color(elf_strerror(err), 0x0C);
        vga_putc('\n');
        
        if (err == ELF_ERR_LOAD_FAILED) {
            elf_info_t info;
            if (elf_get_info(elf_buffer, bytes_read, &info) == ELF_OK) {
                vga_print_color("Program address: 0x", 0x0E);
                char buf[16];
                itoa(info.load_addr, buf, 16);
                vga_print(buf);
                vga_print_color(" - 0x", 0x0E);
                itoa(info.bss_end, buf, 16);
                vga_print(buf);
                vga_print_color("\nAllowed: 0x110000 - 0xA00000\n", 0x0E);
                vga_print_color("Recompile with linker script\n", 0x0E);
            }
        }
        return -1;
    }
    
    elf_entry_fn program = (elf_entry_fn)entry;
    int result = program();
    
    return result;
}

const char* elf_strerror(elf_error_t err) {
    switch (err) {
        case ELF_OK:                    return "Success";
        case ELF_ERR_NOT_ELF:           return "Not an ELF file";
        case ELF_ERR_NOT_32BIT:         return "Not 32-bit ELF";
        case ELF_ERR_NOT_LITTLE_ENDIAN: return "Not little-endian";
        case ELF_ERR_NOT_EXECUTABLE:    return "Not executable";
        case ELF_ERR_WRONG_ARCH:        return "Wrong architecture (need i386)";
        case ELF_ERR_NO_SEGMENTS:       return "No loadable segments";
        case ELF_ERR_LOAD_FAILED:       return "Load failed (bad address)";
        case ELF_ERR_FILE_NOT_FOUND:    return "File not found";
        case ELF_ERR_FILE_READ:         return "Read error";
        case ELF_ERR_NO_MEMORY:         return "Out of memory";
        default:                        return "Unknown error";
    }
}