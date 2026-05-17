#ifndef FAT_H
#define FAT_H

#include <stdint.h>

typedef enum {
    FAT_TYPE_NONE = 0,
    FAT_TYPE_12,
    FAT_TYPE_16,
    FAT_TYPE_32
} fat_type_t;

#define FAT_ATTR_READ_ONLY  0x01
#define FAT_ATTR_HIDDEN     0x02
#define FAT_ATTR_SYSTEM     0x04
#define FAT_ATTR_VOLUME_ID  0x08
#define FAT_ATTR_DIRECTORY  0x10
#define FAT_ATTR_ARCHIVE    0x20
#define FAT_ATTR_LFN        0x0F
#define FAT12_EOC   0x0FF8
#define FAT16_EOC   0xFFF8
#define FAT32_EOC   0x0FFFFFF8
#define FAT_MAX_PATH    256
#define FAT_MAX_NAME    256

typedef struct {
    char        name[FAT_MAX_NAME];
    uint8_t     attr;
    uint32_t    size;
    uint32_t    cluster;
    uint16_t    date;
    uint16_t    time;
} fat_file_info_t;

int fat_mount(uint8_t drive);
void fat_unmount(void);
int fat_is_mounted(void);

fat_type_t fat_get_type(void);
const char* fat_get_type_str(void);
const char* fat_get_current_path(void);

int fat_cd(const char* path);
void fat_pwd(void);
void fat_ls(const char* path);

int fat_cat(const char* path);
int fat_read(const char* path, void* buffer, uint32_t max_size);
int fat_touch(const char* path);
int fat_write(const char* path, const void* data, uint32_t size);
int fat_append(const char* path, const void* data, uint32_t size);
int fat_mkdir(const char* path);
int fat_rm(const char* path);
int fat_stat(const char* path, fat_file_info_t* info);
int fat_exists(const char* path);
int fat_is_dir(const char* path);

uint32_t fat_free_space(void);
uint32_t fat_total_space(void);

void fat_info(void);

#endif