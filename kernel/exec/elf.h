#ifndef ELF_H
#define ELF_H

#include <stdint.h>

/* ELF constants */
#define EI_NIDENT       16
#define EI_MAG0         0
#define EI_MAG1         1
#define EI_MAG2         2
#define EI_MAG3         3
#define EI_CLASS        4
#define EI_DATA         5

#define ELFCLASS32      1
#define ELFDATA2LSB     1
#define ET_EXEC         2
#define EM_386          3
#define PT_LOAD         1

/* Syscall table - at fixed address 0x100000 */
#define SYSCALL_TABLE_ADDR  0x100000
#define SYSCALL_MAGIC_VALUE 0xA105C411

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

/* ELF structures */
typedef struct {
    uint8_t     e_ident[EI_NIDENT];
    uint16_t    e_type;
    uint16_t    e_machine;
    uint32_t    e_version;
    uint32_t    e_entry;
    uint32_t    e_phoff;
    uint32_t    e_shoff;
    uint32_t    e_flags;
    uint16_t    e_ehsize;
    uint16_t    e_phentsize;
    uint16_t    e_phnum;
    uint16_t    e_shentsize;
    uint16_t    e_shnum;
    uint16_t    e_shstrndx;
} __attribute__((packed)) Elf32_Ehdr;

typedef struct {
    uint32_t    p_type;
    uint32_t    p_offset;
    uint32_t    p_vaddr;
    uint32_t    p_paddr;
    uint32_t    p_filesz;
    uint32_t    p_memsz;
    uint32_t    p_flags;
    uint32_t    p_align;
} __attribute__((packed)) Elf32_Phdr;

typedef enum {
    ELF_OK = 0,
    ELF_ERR_NOT_ELF,
    ELF_ERR_NOT_32BIT,
    ELF_ERR_NOT_LITTLE_ENDIAN,
    ELF_ERR_NOT_EXECUTABLE,
    ELF_ERR_WRONG_ARCH,
    ELF_ERR_NO_SEGMENTS,
    ELF_ERR_LOAD_FAILED,
    ELF_ERR_FILE_NOT_FOUND,
    ELF_ERR_FILE_READ,
    ELF_ERR_NO_MEMORY,
} elf_error_t;

typedef struct {
    uint32_t    entry_point;
    uint32_t    load_addr;
    uint32_t    load_end;
    uint32_t    bss_end;
} elf_info_t;

elf_error_t elf_validate(const void* data, uint32_t size);
elf_error_t elf_get_info(const void* data, uint32_t size, elf_info_t* info);
elf_error_t elf_load(const void* data, uint32_t size, uint32_t* entry);
int elf_exec(const char* path);
const char* elf_strerror(elf_error_t err);

#endif