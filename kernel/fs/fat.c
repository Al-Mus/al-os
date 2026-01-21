#include "fat.h"
#include "../drivers/ata.h"
#include "../drivers/vga.h"
#include "../utils/string.h"
#include "../utils/ports.h"

typedef struct __attribute__((packed)) {
    uint8_t     jmp[3];
    char        oem[8];
    uint16_t    bytes_per_sector;
    uint8_t     sectors_per_cluster;
    uint16_t    reserved_sectors;
    uint8_t     num_fats;
    uint16_t    root_entry_count;
    uint16_t    total_sectors_16;
    uint8_t     media_type;
    uint16_t    fat_size_16;
    uint16_t    sectors_per_track;
    uint16_t    num_heads;
    uint32_t    hidden_sectors;
    uint32_t    total_sectors_32;
} bpb_t;

typedef struct __attribute__((packed)) {
    bpb_t       bpb;
    uint8_t     drive_number;
    uint8_t     reserved1;
    uint8_t     boot_sig;
    uint32_t    volume_id;
    char        volume_label[11];
    char        fs_type[8];
} fat16_ebpb_t;

typedef struct __attribute__((packed)) {
    bpb_t       bpb;
    uint32_t    fat_size_32;
    uint16_t    ext_flags;
    uint16_t    fs_version;
    uint32_t    root_cluster;
    uint16_t    fs_info;
    uint16_t    backup_boot_sector;
    uint8_t     reserved[12];
    uint8_t     drive_number;
    uint8_t     reserved1;
    uint8_t     boot_sig;
    uint32_t    volume_id;
    char        volume_label[11];
    char        fs_type[8];
} fat32_ebpb_t;

typedef struct __attribute__((packed)) {
    char        name[8];
    char        ext[3];
    uint8_t     attr;
    uint8_t     nt_reserved;
    uint8_t     create_time_tenths;
    uint16_t    create_time;
    uint16_t    create_date;
    uint16_t    access_date;
    uint16_t    cluster_hi;
    uint16_t    modify_time;
    uint16_t    modify_date;
    uint16_t    cluster_lo;
    uint32_t    file_size;
} fat_dir_entry_t;

typedef struct __attribute__((packed)) {
    uint8_t     order;
    uint16_t    name1[5];
    uint8_t     attr;
    uint8_t     type;
    uint8_t     checksum;
    uint16_t    name2[6];
    uint16_t    cluster;
    uint16_t    name3[2];
} fat_lfn_entry_t;

#define MAX_SECTOR_SIZE     4096
#define DIR_ENTRY_SIZE      32

static struct {
    uint8_t     mounted;
    uint8_t     drive;
    fat_type_t  type;
    
    uint16_t    bytes_per_sector;
    uint8_t     sectors_per_cluster;
    uint16_t    entries_per_sector;
    uint8_t     ata_sectors_per_fs_sector;
    
    uint32_t    fat_start_sector;
    uint32_t    fat_size_sectors;
    uint32_t    root_dir_sector;
    uint32_t    root_dir_sectors;
    uint32_t    data_start_sector;
    uint32_t    total_clusters;
    
    uint32_t    root_cluster;
    
    uint32_t    current_cluster;
    char        current_path[FAT_MAX_PATH];
    
    char        volume_label[12];
    
    uint8_t     sector_buf[MAX_SECTOR_SIZE];
    
    uint32_t    fat_cache_sector;
    uint8_t     fat_cache[MAX_SECTOR_SIZE];
    uint8_t     fat_cache_dirty;
    
} fat_state;

static int read_sector(uint32_t sector, void* buffer) {
    if (fat_state.bytes_per_sector == 512) {
        return ata_read_sectors(fat_state.drive, sector, 1, buffer);
    }
    
    uint32_t ata_sector = sector * fat_state.ata_sectors_per_fs_sector;
    uint8_t* buf = (uint8_t*)buffer;
    
    for (int i = 0; i < fat_state.ata_sectors_per_fs_sector; i++) {
        if (ata_read_sectors(fat_state.drive, ata_sector + i, 1, buf + (i * 512)) < 0) {
            return -1;
        }
    }
    return 0;
}

static int write_sector(uint32_t sector, const void* buffer) {
    if (fat_state.bytes_per_sector == 512) {
        return ata_write_sectors(fat_state.drive, sector, 1, buffer);
    }
    
    uint32_t ata_sector = sector * fat_state.ata_sectors_per_fs_sector;
    const uint8_t* buf = (const uint8_t*)buffer;
    
    for (int i = 0; i < fat_state.ata_sectors_per_fs_sector; i++) {
        if (ata_write_sectors(fat_state.drive, ata_sector + i, 1, buf + (i * 512)) < 0) {
            return -1;
        }
    }
    return 0;
}

static void to_upper(char* str) {
    while (*str) {
        if (*str >= 'a' && *str <= 'z') *str -= 32;
        str++;
    }
}

static uint32_t cluster_to_sector(uint32_t cluster) {
    return fat_state.data_start_sector + 
           (cluster - 2) * fat_state.sectors_per_cluster;
}

static int fat_cache_load(uint32_t sector) {
    if (fat_state.fat_cache_sector == sector) return 0;
    
    if (fat_state.fat_cache_dirty) {
        write_sector(fat_state.fat_cache_sector, fat_state.fat_cache);
        fat_state.fat_cache_dirty = 0;
    }
    
    if (read_sector(sector, fat_state.fat_cache) < 0) return -1;
    fat_state.fat_cache_sector = sector;
    return 0;
}

static uint32_t fat_get_entry(uint32_t cluster) {
    uint32_t fat_offset;
    uint32_t fat_sector;
    uint32_t ent_offset;
    uint32_t value = 0;
    uint16_t bps = fat_state.bytes_per_sector;
    
    switch (fat_state.type) {
        case FAT_TYPE_12:
            fat_offset = cluster + (cluster / 2);
            fat_sector = fat_state.fat_start_sector + (fat_offset / bps);
            ent_offset = fat_offset % bps;
            
            if (fat_cache_load(fat_sector) < 0) return 0xFFFFFFFF;
            
            value = fat_state.fat_cache[ent_offset];
            
            if (ent_offset == (uint32_t)(bps - 1)) {
                if (fat_cache_load(fat_sector + 1) < 0) return 0xFFFFFFFF;
                value |= ((uint32_t)fat_state.fat_cache[0]) << 8;
            } else {
                value |= ((uint32_t)fat_state.fat_cache[ent_offset + 1]) << 8;
            }
            
            if (cluster & 1) value >>= 4;
            else value &= 0x0FFF;
            
            if (value >= 0x0FF8) value = 0x0FFFFFFF;
            break;
            
        case FAT_TYPE_16:
            fat_offset = cluster * 2;
            fat_sector = fat_state.fat_start_sector + (fat_offset / bps);
            ent_offset = fat_offset % bps;
            
            if (fat_cache_load(fat_sector) < 0) return 0xFFFFFFFF;
            
            value = *(uint16_t*)&fat_state.fat_cache[ent_offset];
            if (value >= 0xFFF8) value = 0x0FFFFFFF;
            break;
            
        case FAT_TYPE_32:
            fat_offset = cluster * 4;
            fat_sector = fat_state.fat_start_sector + (fat_offset / bps);
            ent_offset = fat_offset % bps;
            
            if (fat_cache_load(fat_sector) < 0) return 0xFFFFFFFF;
            
            value = *(uint32_t*)&fat_state.fat_cache[ent_offset] & 0x0FFFFFFF;
            if (value >= 0x0FFFFFF8) value = 0x0FFFFFFF;
            break;
            
        default:
            return 0xFFFFFFFF;
    }
    
    return value;
}

static int fat_set_entry(uint32_t cluster, uint32_t value) {
    uint32_t fat_offset;
    uint32_t fat_sector;
    uint32_t ent_offset;
    uint16_t bps = fat_state.bytes_per_sector;
    
    switch (fat_state.type) {
        case FAT_TYPE_12:
            fat_offset = cluster + (cluster / 2);
            fat_sector = fat_state.fat_start_sector + (fat_offset / bps);
            ent_offset = fat_offset % bps;
            
            if (fat_cache_load(fat_sector) < 0) return -1;
            
            if (cluster & 1) {
                fat_state.fat_cache[ent_offset] = 
                    (fat_state.fat_cache[ent_offset] & 0x0F) | ((value & 0x0F) << 4);
                    
                if (ent_offset == (uint32_t)(bps - 1)) {
                    fat_state.fat_cache_dirty = 1;
                    write_sector(fat_sector, fat_state.fat_cache);
                    if (fat_cache_load(fat_sector + 1) < 0) return -1;
                    fat_state.fat_cache[0] = (value >> 4) & 0xFF;
                } else {
                    fat_state.fat_cache[ent_offset + 1] = (value >> 4) & 0xFF;
                }
            } else {
                fat_state.fat_cache[ent_offset] = value & 0xFF;
                
                if (ent_offset == (uint32_t)(bps - 1)) {
                    fat_state.fat_cache_dirty = 1;
                    write_sector(fat_sector, fat_state.fat_cache);
                    if (fat_cache_load(fat_sector + 1) < 0) return -1;
                    fat_state.fat_cache[0] = 
                        (fat_state.fat_cache[0] & 0xF0) | ((value >> 8) & 0x0F);
                } else {
                    fat_state.fat_cache[ent_offset + 1] = 
                        (fat_state.fat_cache[ent_offset + 1] & 0xF0) | ((value >> 8) & 0x0F);
                }
            }
            fat_state.fat_cache_dirty = 1;
            break;
            
        case FAT_TYPE_16:
            fat_offset = cluster * 2;
            fat_sector = fat_state.fat_start_sector + (fat_offset / bps);
            ent_offset = fat_offset % bps;
            
            if (fat_cache_load(fat_sector) < 0) return -1;
            *(uint16_t*)&fat_state.fat_cache[ent_offset] = (uint16_t)value;
            fat_state.fat_cache_dirty = 1;
            break;
            
        case FAT_TYPE_32:
            fat_offset = cluster * 4;
            fat_sector = fat_state.fat_start_sector + (fat_offset / bps);
            ent_offset = fat_offset % bps;
            
            if (fat_cache_load(fat_sector) < 0) return -1;
            *(uint32_t*)&fat_state.fat_cache[ent_offset] = 
                (*(uint32_t*)&fat_state.fat_cache[ent_offset] & 0xF0000000) | (value & 0x0FFFFFFF);
            fat_state.fat_cache_dirty = 1;
            break;
            
        default:
            return -1;
    }
    
    return 0;
}

static uint32_t fat_alloc_cluster(void) {
    for (uint32_t i = 2; i < fat_state.total_clusters + 2; i++) {
        if (fat_get_entry(i) == 0) {
            uint32_t eoc;
            switch (fat_state.type) {
                case FAT_TYPE_12: eoc = 0x0FFF; break;
                case FAT_TYPE_16: eoc = 0xFFFF; break;
                case FAT_TYPE_32: eoc = 0x0FFFFFFF; break;
                default: return 0;
            }
            fat_set_entry(i, eoc);
            
            if (fat_state.fat_cache_dirty) {
                write_sector(fat_state.fat_cache_sector, fat_state.fat_cache);
                fat_state.fat_cache_dirty = 0;
            }
            
            memset(fat_state.sector_buf, 0, fat_state.bytes_per_sector);
            uint32_t sector = cluster_to_sector(i);
            for (int s = 0; s < fat_state.sectors_per_cluster; s++) {
                write_sector(sector + s, fat_state.sector_buf);
            }
            
            return i;
        }
    }
    return 0;
}

static uint8_t lfn_checksum(const char* short_name) {
    uint8_t sum = 0;
    for (int i = 0; i < 11; i++) {
        sum = ((sum & 1) ? 0x80 : 0) + (sum >> 1) + (uint8_t)short_name[i];
    }
    return sum;
}

static int needs_lfn(const char* name) {
    int len = 0;
    int dot_pos = -1;
    
    for (int i = 0; name[i]; i++) {
        if (name[i] == '.') dot_pos = i;
        if (name[i] >= 'a' && name[i] <= 'z') return 1;
        len++;
    }
    
    if (dot_pos == -1) {
        if (len > 8) return 1;
    } else {
        if (dot_pos > 8) return 1;
        if (len - dot_pos - 1 > 3) return 1;
    }
    
    return 0;
}

static void fat_name_to_str(const fat_dir_entry_t* entry, char* out) {
    int i, j = 0;
    
    for (i = 0; i < 8 && entry->name[i] != ' '; i++) {
        out[j++] = entry->name[i];
    }
    
    if (entry->ext[0] != ' ') {
        out[j++] = '.';
        for (i = 0; i < 3 && entry->ext[i] != ' '; i++) {
            out[j++] = entry->ext[i];
        }
    }
    
    out[j] = '\0';
}

static void str_to_fat_name(const char* name, char* out) {
    memset(out, ' ', 11);
    
    int i = 0, j = 0;
    
    const char* dot = NULL;
    for (i = 0; name[i]; i++) {
        if (name[i] == '.') dot = &name[i];
    }
    
    i = 0;
    while (name[i] && &name[i] != dot && j < 8) {
        char c = name[i];
        if (c >= 'a' && c <= 'z') c -= 32;
        out[j++] = c;
        i++;
    }
    
    if (dot) {
        dot++;
        j = 8;
        while (*dot && j < 11) {
            char c = *dot;
            if (c >= 'a' && c <= 'z') c -= 32;
            out[j++] = c;
            dot++;
        }
    }
}

static int read_dir_entries(uint32_t start_cluster, 
                           int (*callback)(fat_dir_entry_t*, char*, void*),
                           void* ctx) {
    uint32_t cluster = start_cluster;
    char lfn_buf[FAT_MAX_NAME];
    int has_lfn = 0;
    uint16_t entries_per_sec = fat_state.entries_per_sector;
    
    lfn_buf[0] = '\0';
    
    if (cluster == 0 && fat_state.type != FAT_TYPE_32) {
        for (uint32_t s = 0; s < fat_state.root_dir_sectors; s++) {
            if (read_sector(fat_state.root_dir_sector + s, fat_state.sector_buf) < 0)
                return -1;
            
            fat_dir_entry_t* entries = (fat_dir_entry_t*)fat_state.sector_buf;
            
            for (uint16_t i = 0; i < entries_per_sec; i++) {
                if (entries[i].name[0] == 0x00) return 0;
                if ((uint8_t)entries[i].name[0] == 0xE5) continue;
                
                if (entries[i].attr == FAT_ATTR_LFN) {
                    fat_lfn_entry_t* lfn = (fat_lfn_entry_t*)&entries[i];
                    int ord = lfn->order & 0x3F;
                    int pos = (ord - 1) * 13;
                    
                    if (lfn->order & 0x40) {
                        has_lfn = 1;
                        memset(lfn_buf, 0, sizeof(lfn_buf));
                    }
                    
                    for (int k = 0; k < 5 && pos < FAT_MAX_NAME - 1; k++, pos++)
                        lfn_buf[pos] = (char)lfn->name1[k];
                    for (int k = 0; k < 6 && pos < FAT_MAX_NAME - 1; k++, pos++)
                        lfn_buf[pos] = (char)lfn->name2[k];
                    for (int k = 0; k < 2 && pos < FAT_MAX_NAME - 1; k++, pos++)
                        lfn_buf[pos] = (char)lfn->name3[k];
                    continue;
                }
                
                if (entries[i].attr & FAT_ATTR_VOLUME_ID) continue;
                
                char name[FAT_MAX_NAME];
                if (has_lfn) {
                    strcpy(name, lfn_buf);
                    has_lfn = 0;
                } else {
                    fat_name_to_str(&entries[i], name);
                }
                
                if (callback(&entries[i], name, ctx) != 0) return 1;
            }
        }
        return 0;
    }
    
    while (cluster < 0x0FFFFFF8) {
        uint32_t sector = cluster_to_sector(cluster);
        
        for (int s = 0; s < fat_state.sectors_per_cluster; s++) {
            if (read_sector(sector + s, fat_state.sector_buf) < 0) return -1;
            
            fat_dir_entry_t* entries = (fat_dir_entry_t*)fat_state.sector_buf;
            
            for (uint16_t i = 0; i < entries_per_sec; i++) {
                if (entries[i].name[0] == 0x00) return 0;
                if ((uint8_t)entries[i].name[0] == 0xE5) continue;
                
                if (entries[i].attr == FAT_ATTR_LFN) {
                    fat_lfn_entry_t* lfn = (fat_lfn_entry_t*)&entries[i];
                    int ord = lfn->order & 0x3F;
                    int pos = (ord - 1) * 13;
                    
                    if (lfn->order & 0x40) {
                        has_lfn = 1;
                        memset(lfn_buf, 0, sizeof(lfn_buf));
                    }
                    
                    for (int k = 0; k < 5 && pos < FAT_MAX_NAME - 1; k++, pos++)
                        lfn_buf[pos] = (char)lfn->name1[k];
                    for (int k = 0; k < 6 && pos < FAT_MAX_NAME - 1; k++, pos++)
                        lfn_buf[pos] = (char)lfn->name2[k];
                    for (int k = 0; k < 2 && pos < FAT_MAX_NAME - 1; k++, pos++)
                        lfn_buf[pos] = (char)lfn->name3[k];
                    continue;
                }
                
                if (entries[i].attr & FAT_ATTR_VOLUME_ID) continue;
                
                char name[FAT_MAX_NAME];
                if (has_lfn) {
                    strcpy(name, lfn_buf);
                    has_lfn = 0;
                } else {
                    fat_name_to_str(&entries[i], name);
                }
                
                if (callback(&entries[i], name, ctx) != 0) return 1;
            }
        }
        
        cluster = fat_get_entry(cluster);
    }
    
    return 0;
}

typedef struct {
    const char* target;
    fat_dir_entry_t* result;
    int found;
} find_ctx_t;

static int find_callback(fat_dir_entry_t* entry, char* name, void* ctx) {
    find_ctx_t* fctx = (find_ctx_t*)ctx;
    
    char upper_name[FAT_MAX_NAME];
    char upper_target[FAT_MAX_NAME];
    
    strncpy(upper_name, name, FAT_MAX_NAME - 1);
    strncpy(upper_target, fctx->target, FAT_MAX_NAME - 1);
    upper_name[FAT_MAX_NAME - 1] = '\0';
    upper_target[FAT_MAX_NAME - 1] = '\0';
    
    to_upper(upper_name);
    to_upper(upper_target);
    
    if (strcmp(upper_name, upper_target) == 0) {
        memcpy(fctx->result, entry, sizeof(fat_dir_entry_t));
        fctx->found = 1;
        return 1;
    }
    return 0;
}

static int fat_find_in_dir(uint32_t dir_cluster, const char* name, fat_dir_entry_t* out) {
    find_ctx_t ctx = { name, out, 0 };
    read_dir_entries(dir_cluster, find_callback, &ctx);
    return ctx.found ? 0 : -1;
}

static uint32_t get_entry_cluster(fat_dir_entry_t* entry) {
    uint32_t cluster = entry->cluster_lo;
    if (fat_state.type == FAT_TYPE_32) {
        cluster |= ((uint32_t)entry->cluster_hi) << 16;
    }
    return cluster;
}

static int fat_resolve_path(const char* path, uint32_t* out_cluster, fat_dir_entry_t* out_entry) {
    uint32_t cluster;
    
    if (path[0] == '/') {
        cluster = (fat_state.type == FAT_TYPE_32) ? fat_state.root_cluster : 0;
        path++;
    } else {
        cluster = fat_state.current_cluster;
    }
    
    char component[FAT_MAX_NAME];
    fat_dir_entry_t entry;
    
    while (*path) {
        while (*path == '/') path++;
        if (!*path) break;
        
        int i = 0;
        while (*path && *path != '/' && i < FAT_MAX_NAME - 1) {
            component[i++] = *path++;
        }
        component[i] = '\0';
        
        if (strcmp(component, ".") == 0) continue;
        
        if (strcmp(component, "..") == 0) {
            if (fat_find_in_dir(cluster, "..", &entry) < 0) {
                cluster = (fat_state.type == FAT_TYPE_32) ? fat_state.root_cluster : 0;
            } else {
                cluster = get_entry_cluster(&entry);
                if (cluster == 0 && fat_state.type == FAT_TYPE_32) {
                    cluster = fat_state.root_cluster;
                }
            }
            continue;
        }
        
        if (fat_find_in_dir(cluster, component, &entry) < 0) {
            return -1;
        }
        
        cluster = get_entry_cluster(&entry);
        
        if (cluster == 0 && fat_state.type == FAT_TYPE_32) {
            cluster = fat_state.root_cluster;
        }
    }
    
    if (out_cluster) *out_cluster = cluster;
    if (out_entry) memcpy(out_entry, &entry, sizeof(fat_dir_entry_t));
    
    return 0;
}

int fat_mount(uint8_t drive) {
    if (fat_state.mounted) {
        fat_unmount();
    }
    
    ata_init();
    
    if (!ata_drive_exists(drive)) {
        vga_print_color("Drive not found\n", 0x0C);
        return -1;
    }
    
    uint8_t boot_sector[512];
    if (ata_read_sectors(drive, 0, 1, boot_sector) < 0) {
        vga_print_color("Failed to read boot sector\n", 0x0C);
        return -1;
    }
    
    bpb_t* bpb = (bpb_t*)boot_sector;
    
    uint16_t bps = bpb->bytes_per_sector;
    if (bps != 512 && bps != 1024 && bps != 2048 && bps != 4096) {
        vga_print_color("Unsupported sector size: ", 0x0C);
        char buf[16];
        itoa(bps, buf, 10);
        vga_print(buf);
        vga_print_color("\nSupported: 512, 1024, 2048, 4096\n", 0x0C);
        return -1;
    }
    
    if (bpb->num_fats == 0 || bpb->sectors_per_cluster == 0) {
        vga_print_color("Invalid BPB\n", 0x0C);
        return -1;
    }
    
    fat_state.drive = drive;
    fat_state.bytes_per_sector = bps;
    fat_state.sectors_per_cluster = bpb->sectors_per_cluster;
    fat_state.entries_per_sector = bps / DIR_ENTRY_SIZE;
    fat_state.ata_sectors_per_fs_sector = bps / 512;
    
    if (bps > 512) {
        memcpy(fat_state.sector_buf, boot_sector, 512);
        for (int i = 1; i < fat_state.ata_sectors_per_fs_sector; i++) {
            if (ata_read_sectors(drive, i, 1, fat_state.sector_buf + (i * 512)) < 0) {
                vga_print_color("Failed to read full boot sector\n", 0x0C);
                return -1;
            }
        }
        bpb = (bpb_t*)fat_state.sector_buf;
    }
    
    fat_state.fat_start_sector = bpb->reserved_sectors;
    
    uint32_t fat_size;
    if (bpb->fat_size_16 != 0) {
        fat_size = bpb->fat_size_16;
    } else {
        fat32_ebpb_t* fat32 = (fat32_ebpb_t*)(bps > 512 ? fat_state.sector_buf : boot_sector);
        fat_size = fat32->fat_size_32;
    }
    fat_state.fat_size_sectors = fat_size;
    
    fat_state.root_dir_sector = fat_state.fat_start_sector + (bpb->num_fats * fat_size);
    fat_state.root_dir_sectors = ((bpb->root_entry_count * 32) + (bps - 1)) / bps;
    
    fat_state.data_start_sector = fat_state.root_dir_sector + fat_state.root_dir_sectors;
    
    uint32_t total_sectors = (bpb->total_sectors_16 != 0) ? 
                             bpb->total_sectors_16 : bpb->total_sectors_32;
    
    uint32_t data_sectors = total_sectors - fat_state.data_start_sector;
    fat_state.total_clusters = data_sectors / bpb->sectors_per_cluster;
    
    if (fat_state.total_clusters < 4085) {
        fat_state.type = FAT_TYPE_12;
    } else if (fat_state.total_clusters < 65525) {
        fat_state.type = FAT_TYPE_16;
    } else {
        fat_state.type = FAT_TYPE_32;
        fat32_ebpb_t* fat32 = (fat32_ebpb_t*)(bps > 512 ? fat_state.sector_buf : boot_sector);
        fat_state.root_cluster = fat32->root_cluster;
        fat_state.root_dir_sectors = 0;
        fat_state.data_start_sector = fat_state.root_dir_sector;
    }
    
    if (fat_state.type == FAT_TYPE_32) {
        fat32_ebpb_t* fat32 = (fat32_ebpb_t*)(bps > 512 ? fat_state.sector_buf : boot_sector);
        memcpy(fat_state.volume_label, fat32->volume_label, 11);
    } else {
        fat16_ebpb_t* fat16 = (fat16_ebpb_t*)(bps > 512 ? fat_state.sector_buf : boot_sector);
        memcpy(fat_state.volume_label, fat16->volume_label, 11);
    }
    fat_state.volume_label[11] = '\0';
    
    for (int i = 10; i >= 0 && fat_state.volume_label[i] == ' '; i--) {
        fat_state.volume_label[i] = '\0';
    }
    
    fat_state.current_cluster = (fat_state.type == FAT_TYPE_32) ? 
                                fat_state.root_cluster : 0;
    strcpy(fat_state.current_path, "/");
    
    fat_state.fat_cache_sector = 0xFFFFFFFF;
    fat_state.fat_cache_dirty = 0;
    
    fat_state.mounted = 1;
    
    return 0;
}

void fat_unmount(void) {
    if (!fat_state.mounted) return;
    
    if (fat_state.fat_cache_dirty) {
        write_sector(fat_state.fat_cache_sector, fat_state.fat_cache);
    }
    
    memset(&fat_state, 0, sizeof(fat_state));
}

int fat_is_mounted(void) {
    return fat_state.mounted;
}

fat_type_t fat_get_type(void) {
    return fat_state.type;
}

const char* fat_get_type_str(void) {
    switch (fat_state.type) {
        case FAT_TYPE_12: return "FAT12";
        case FAT_TYPE_16: return "FAT16";
        case FAT_TYPE_32: return "FAT32";
        default: return "Unknown";
    }
}

const char* fat_get_current_path(void) {
    if (!fat_state.mounted) return "";
    return fat_state.current_path;
}

int fat_cd(const char* path) {
    if (!fat_state.mounted) {
        vga_print_color("No filesystem mounted\n", 0x0C);
        return -1;
    }
    
    if (!path || !path[0]) return 0;
    
    uint32_t cluster;
    fat_dir_entry_t entry;
    
    if (strcmp(path, "/") == 0) {
        fat_state.current_cluster = (fat_state.type == FAT_TYPE_32) ? 
                                    fat_state.root_cluster : 0;
        strcpy(fat_state.current_path, "/");
        return 0;
    }
    
    if (fat_resolve_path(path, &cluster, &entry) < 0) {
        vga_print_color("Directory not found\n", 0x0C);
        return -1;
    }
    
    if (!(entry.attr & FAT_ATTR_DIRECTORY)) {
        vga_print_color("Not a directory\n", 0x0C);
        return -1;
    }
    
    fat_state.current_cluster = cluster;
    
    if (path[0] == '/') {
        strncpy(fat_state.current_path, path, FAT_MAX_PATH - 1);
    } else if (strcmp(path, "..") == 0) {
        char* last_slash = strrchr(fat_state.current_path, '/');
        if (last_slash && last_slash != fat_state.current_path) {
            *last_slash = '\0';
        } else {
            strcpy(fat_state.current_path, "/");
        }
    } else if (strcmp(path, ".") != 0) {
        if (strlen(fat_state.current_path) > 1) {
            strcat(fat_state.current_path, "/");
        }
        strcat(fat_state.current_path, path);
    }
    
    return 0;
}

void fat_pwd(void) {
    if (!fat_state.mounted) {
        vga_print_color("No filesystem mounted\n", 0x0C);
        return;
    }
    vga_print_color(fat_state.current_path, 0x0F);
    vga_putc('\n');
}

static int ls_callback(fat_dir_entry_t* entry, char* name, void* ctx) {
    (void)ctx;
    
    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) return 0;
    
    if (entry->attr & FAT_ATTR_DIRECTORY) {
        vga_print_color(name, 0x09);
        vga_print_color("/", 0x09);
    } else {
        vga_print_color(name, 0x0F);
    }
    
    if (!(entry->attr & FAT_ATTR_DIRECTORY)) {
        vga_print_color("  ", 0x08);
        char size_buf[16];
        itoa(entry->file_size, size_buf, 10);
        vga_print_color(size_buf, 0x08);
    }
    
    vga_putc('\n');
    return 0;
}

void fat_ls(const char* path) {
    if (!fat_state.mounted) {
        vga_print_color("No filesystem mounted\n", 0x0C);
        return;
    }
    
    uint32_t cluster;
    
    if (!path || !path[0] || strcmp(path, ".") == 0) {
        cluster = fat_state.current_cluster;
    } else {
        fat_dir_entry_t entry;
        if (fat_resolve_path(path, &cluster, &entry) < 0) {
            vga_print_color("Directory not found\n", 0x0C);
            return;
        }
        if (!(entry.attr & FAT_ATTR_DIRECTORY)) {
            vga_print_color("Not a directory\n", 0x0C);
            return;
        }
    }
    
    read_dir_entries(cluster, ls_callback, NULL);
}

int fat_cat(const char* path) {
    if (!fat_state.mounted) {
        vga_print_color("No filesystem mounted\n", 0x0C);
        return -1;
    }
    
    fat_dir_entry_t entry;
    uint32_t cluster;
    
    if (fat_resolve_path(path, &cluster, &entry) < 0) {
        vga_print_color("File not found\n", 0x0C);
        return -1;
    }
    
    if (entry.attr & FAT_ATTR_DIRECTORY) {
        vga_print_color("Is a directory\n", 0x0C);
        return -1;
    }
    
    uint32_t remaining = entry.file_size;
    cluster = get_entry_cluster(&entry);
    uint16_t bps = fat_state.bytes_per_sector;
    
    while (cluster < 0x0FFFFFF8 && remaining > 0) {
        uint32_t sector = cluster_to_sector(cluster);
        
        for (int s = 0; s < fat_state.sectors_per_cluster && remaining > 0; s++) {
            if (read_sector(sector + s, fat_state.sector_buf) < 0) {
                vga_print_color("\nRead error\n", 0x0C);
                return -1;
            }
            
            uint32_t to_print = (remaining < bps) ? remaining : bps;
            for (uint32_t i = 0; i < to_print; i++) {
                char c = fat_state.sector_buf[i];
                if (c == '\0') break;
                vga_putc(c);
            }
            remaining -= to_print;
        }
        
        cluster = fat_get_entry(cluster);
    }
    
    vga_putc('\n');
    return 0;
}

int fat_read(const char* path, void* buffer, uint32_t max_size) {
    if (!fat_state.mounted) return -1;
    
    fat_dir_entry_t entry;
    uint32_t cluster;
    
    if (fat_resolve_path(path, &cluster, &entry) < 0) return -1;
    if (entry.attr & FAT_ATTR_DIRECTORY) return -1;
    
    uint32_t to_read = entry.file_size;
    if (to_read > max_size) to_read = max_size;
    
    uint32_t read_total = 0;
    uint8_t* buf = (uint8_t*)buffer;
    cluster = get_entry_cluster(&entry);
    uint16_t bps = fat_state.bytes_per_sector;
    
    while (cluster < 0x0FFFFFF8 && read_total < to_read) {
        uint32_t sector = cluster_to_sector(cluster);
        
        for (int s = 0; s < fat_state.sectors_per_cluster && read_total < to_read; s++) {
            if (read_sector(sector + s, fat_state.sector_buf) < 0) return -1;
            
            uint32_t chunk = to_read - read_total;
            if (chunk > bps) chunk = bps;
            
            memcpy(buf + read_total, fat_state.sector_buf, chunk);
            read_total += chunk;
        }
        
        cluster = fat_get_entry(cluster);
    }
    
    return (int)read_total;
}

static int find_empty_entries(uint32_t dir_cluster, int count, uint32_t* out_sector, int* out_index) {
    int consecutive = 0;
    uint32_t first_sector = 0;
    int first_index = 0;
    uint16_t entries_per_sec = fat_state.entries_per_sector;
    
    if (dir_cluster == 0 && fat_state.type != FAT_TYPE_32) {
        for (uint32_t s = 0; s < fat_state.root_dir_sectors; s++) {
            if (read_sector(fat_state.root_dir_sector + s, fat_state.sector_buf) < 0)
                return -1;
            
            fat_dir_entry_t* entries = (fat_dir_entry_t*)fat_state.sector_buf;
            for (uint16_t i = 0; i < entries_per_sec; i++) {
                if (entries[i].name[0] == 0x00 || (uint8_t)entries[i].name[0] == 0xE5) {
                    if (consecutive == 0) {
                        first_sector = fat_state.root_dir_sector + s;
                        first_index = i;
                    }
                    consecutive++;
                    if (consecutive >= count) {
                        *out_sector = first_sector;
                        *out_index = first_index;
                        return 0;
                    }
                } else {
                    consecutive = 0;
                }
            }
        }
        return -1;
    }
    
    uint32_t cluster = dir_cluster;
    while (cluster < 0x0FFFFFF8) {
        uint32_t sector = cluster_to_sector(cluster);
        
        for (int s = 0; s < fat_state.sectors_per_cluster; s++) {
            if (read_sector(sector + s, fat_state.sector_buf) < 0) return -1;
            
            fat_dir_entry_t* entries = (fat_dir_entry_t*)fat_state.sector_buf;
            for (uint16_t i = 0; i < entries_per_sec; i++) {
                if (entries[i].name[0] == 0x00 || (uint8_t)entries[i].name[0] == 0xE5) {
                    if (consecutive == 0) {
                        first_sector = sector + s;
                        first_index = i;
                    }
                    consecutive++;
                    if (consecutive >= count) {
                        *out_sector = first_sector;
                        *out_index = first_index;
                        return 0;
                    }
                } else {
                    consecutive = 0;
                }
            }
        }
        
        cluster = fat_get_entry(cluster);
    }
    
    uint32_t new_cluster = fat_alloc_cluster();
    if (new_cluster == 0) return -1;
    
    fat_set_entry(cluster, new_cluster);
    if (fat_state.fat_cache_dirty) {
        write_sector(fat_state.fat_cache_sector, fat_state.fat_cache);
        fat_state.fat_cache_dirty = 0;
    }
    
    *out_sector = cluster_to_sector(new_cluster);
    *out_index = 0;
    return 0;
}

static int create_lfn_entries(uint32_t dir_cluster, const char* name, 
                              const char* short_name, uint32_t* entry_sector, int* entry_index) {
    int name_len = strlen(name);
    int lfn_entries = (name_len + 12) / 13;
    
    if (find_empty_entries(dir_cluster, lfn_entries + 1, entry_sector, entry_index) < 0) {
        return -1;
    }
    
    uint8_t checksum = lfn_checksum(short_name);
    
    uint32_t current_sector = *entry_sector;
    int current_index = *entry_index;
    uint16_t entries_per_sec = fat_state.entries_per_sector;
    
    for (int ord = lfn_entries; ord >= 1; ord--) {
        if (read_sector(current_sector, fat_state.sector_buf) < 0) return -1;
        
        fat_lfn_entry_t* lfn = (fat_lfn_entry_t*)&((fat_dir_entry_t*)fat_state.sector_buf)[current_index];
        
        memset(lfn, 0xFF, sizeof(fat_lfn_entry_t));
        lfn->order = ord | ((ord == lfn_entries) ? 0x40 : 0);
        lfn->attr = FAT_ATTR_LFN;
        lfn->type = 0;
        lfn->checksum = checksum;
        lfn->cluster = 0;
        
        int pos = (ord - 1) * 13;
        for (int k = 0; k < 5; k++) {
            lfn->name1[k] = (pos < name_len) ? (uint16_t)(uint8_t)name[pos++] : 0x0000;
        }
        for (int k = 0; k < 6; k++) {
            lfn->name2[k] = (pos < name_len) ? (uint16_t)(uint8_t)name[pos++] : 0x0000;
        }
        for (int k = 0; k < 2; k++) {
            lfn->name3[k] = (pos < name_len) ? (uint16_t)(uint8_t)name[pos++] : 0x0000;
        }
        
        if (write_sector(current_sector, fat_state.sector_buf) < 0) return -1;
        
        current_index++;
        if (current_index >= (int)entries_per_sec) {
            current_index = 0;
            current_sector++;
        }
    }
    
    *entry_sector = current_sector;
    *entry_index = current_index;
    
    return 0;
}

static int is_valid_name(const char* name) {
    if (!name || !name[0]) return 0;
    if (strcmp(name, "/") == 0) return 0;
    if (strcmp(name, ".") == 0) return 0;
    if (strcmp(name, "..") == 0) return 0;
    
    for (int i = 0; name[i]; i++) {
        char c = name[i];
        if (c == '/' || c == '\\' || c == ':' || c == '*' || 
            c == '?' || c == '"' || c == '<' || c == '>' || c == '|') {
            return 0;
        }
    }
    return 1;
}

int fat_touch(const char* path) {
    if (!fat_state.mounted) {
        vga_print_color("No filesystem mounted\n", 0x0C);
        return -1;
    }
    
    char parent_path[FAT_MAX_PATH];
    char filename[FAT_MAX_NAME];
    
    strncpy(parent_path, path, FAT_MAX_PATH - 1);
    parent_path[FAT_MAX_PATH - 1] = '\0';
    
    char* last_slash = strrchr(parent_path, '/');
    if (last_slash) {
        strcpy(filename, last_slash + 1);
        if (last_slash == parent_path) {
            parent_path[1] = '\0';
        } else {
            *last_slash = '\0';
        }
    } else {
        strcpy(filename, path);
        strcpy(parent_path, ".");
    }
    
    if (!is_valid_name(filename)) {
        vga_print_color("Invalid filename\n", 0x0C);
        return -1;
    }
    
    uint32_t parent_cluster;
    fat_dir_entry_t parent_entry;
    
    if (strcmp(parent_path, ".") == 0) {
        parent_cluster = fat_state.current_cluster;
    } else if (strcmp(parent_path, "/") == 0) {
        parent_cluster = (fat_state.type == FAT_TYPE_32) ? fat_state.root_cluster : 0;
    } else {
        if (fat_resolve_path(parent_path, &parent_cluster, &parent_entry) < 0) {
            vga_print_color("Parent directory not found\n", 0x0C);
            return -1;
        }
    }
    
    fat_dir_entry_t existing;
    if (fat_find_in_dir(parent_cluster, filename, &existing) == 0) {
        if (existing.attr & FAT_ATTR_DIRECTORY) {
            vga_print_color("A directory with this name exists\n", 0x0C);
            return -1;
        }
        return 0;
    }
    
    char short_name[11];
    str_to_fat_name(filename, short_name);
    
    uint32_t entry_sector;
    int entry_index;
    
    if (needs_lfn(filename)) {
        if (create_lfn_entries(parent_cluster, filename, short_name, &entry_sector, &entry_index) < 0) {
            vga_print_color("Directory full or disk full\n", 0x0C);
            return -1;
        }
    } else {
        if (find_empty_entries(parent_cluster, 1, &entry_sector, &entry_index) < 0) {
            vga_print_color("Directory full or disk full\n", 0x0C);
            return -1;
        }
    }
    
    if (read_sector(entry_sector, fat_state.sector_buf) < 0) return -1;
    
    fat_dir_entry_t* entries = (fat_dir_entry_t*)fat_state.sector_buf;
    fat_dir_entry_t* new_entry = &entries[entry_index];
    
    memset(new_entry, 0, sizeof(fat_dir_entry_t));
    memcpy(new_entry->name, short_name, 11);
    new_entry->attr = FAT_ATTR_ARCHIVE;
    new_entry->file_size = 0;
    new_entry->cluster_lo = 0;
    new_entry->cluster_hi = 0;
    
    if (write_sector(entry_sector, fat_state.sector_buf) < 0) return -1;
    
    return 0;
}

int fat_write(const char* path, const void* data, uint32_t size) {
    if (!fat_state.mounted) {
        vga_print_color("No filesystem mounted\n", 0x0C);
        return -1;
    }
    
    fat_dir_entry_t entry;
    uint32_t dummy;
    int file_exists = (fat_resolve_path(path, &dummy, &entry) == 0);
    
    if (file_exists && (entry.attr & FAT_ATTR_DIRECTORY)) {
        vga_print_color("Cannot write to directory\n", 0x0C);
        return -1;
    }
    
    if (!file_exists) {
        if (fat_touch(path) < 0) {
            return -1;
        }
        if (fat_resolve_path(path, &dummy, &entry) < 0) {
            vga_print_color("Failed to create file\n", 0x0C);
            return -1;
        }
    }
    
    uint32_t old_cluster = get_entry_cluster(&entry);
    while (old_cluster >= 2 && old_cluster < 0x0FFFFFF8) {
        uint32_t next = fat_get_entry(old_cluster);
        fat_set_entry(old_cluster, 0);
        old_cluster = next;
    }
    
    if (fat_state.fat_cache_dirty) {
        write_sector(fat_state.fat_cache_sector, fat_state.fat_cache);
        fat_state.fat_cache_dirty = 0;
    }
    
    if (size == 0) {
        char parent_path[FAT_MAX_PATH];
        char filename[FAT_MAX_NAME];
        
        strncpy(parent_path, path, FAT_MAX_PATH - 1);
        char* last_slash = strrchr(parent_path, '/');
        if (last_slash) {
            strcpy(filename, last_slash + 1);
            if (last_slash == parent_path) parent_path[1] = '\0';
            else *last_slash = '\0';
        } else {
            strcpy(filename, path);
            strcpy(parent_path, ".");
        }
        
        uint32_t parent_cluster;
        if (strcmp(parent_path, ".") == 0) {
            parent_cluster = fat_state.current_cluster;
        } else if (strcmp(parent_path, "/") == 0) {
            parent_cluster = (fat_state.type == FAT_TYPE_32) ? fat_state.root_cluster : 0;
        } else {
            fat_dir_entry_t pentry;
            fat_resolve_path(parent_path, &parent_cluster, &pentry);
        }
        
        char fat_name[11];
        str_to_fat_name(filename, fat_name);
        uint16_t entries_per_sec = fat_state.entries_per_sector;
        
        if (parent_cluster == 0 && fat_state.type != FAT_TYPE_32) {
            for (uint32_t s = 0; s < fat_state.root_dir_sectors; s++) {
                read_sector(fat_state.root_dir_sector + s, fat_state.sector_buf);
                fat_dir_entry_t* entries = (fat_dir_entry_t*)fat_state.sector_buf;
                
                for (uint16_t i = 0; i < entries_per_sec; i++) {
                    if (memcmp(entries[i].name, fat_name, 11) == 0) {
                        entries[i].cluster_lo = 0;
                        entries[i].cluster_hi = 0;
                        entries[i].file_size = 0;
                        write_sector(fat_state.root_dir_sector + s, fat_state.sector_buf);
                        return 0;
                    }
                }
            }
        } else {
            uint32_t cluster = parent_cluster;
            while (cluster < 0x0FFFFFF8) {
                uint32_t sector = cluster_to_sector(cluster);
                
                for (int s = 0; s < fat_state.sectors_per_cluster; s++) {
                    read_sector(sector + s, fat_state.sector_buf);
                    fat_dir_entry_t* entries = (fat_dir_entry_t*)fat_state.sector_buf;
                    
                    for (uint16_t i = 0; i < entries_per_sec; i++) {
                        if (memcmp(entries[i].name, fat_name, 11) == 0) {
                            entries[i].cluster_lo = 0;
                            entries[i].cluster_hi = 0;
                            entries[i].file_size = 0;
                            write_sector(sector + s, fat_state.sector_buf);
                            return 0;
                        }
                    }
                }
                cluster = fat_get_entry(cluster);
            }
        }
        return 0;
    }
    
    uint32_t first_cluster = 0;
    uint32_t prev_cluster = 0;
    uint32_t bytes_written = 0;
    const uint8_t* src = (const uint8_t*)data;
    uint16_t bps = fat_state.bytes_per_sector;
    
    while (bytes_written < size) {
        uint32_t cluster = fat_alloc_cluster();
        if (cluster == 0) {
            if (first_cluster != 0) {
                uint32_t c = first_cluster;
                while (c >= 2 && c < 0x0FFFFFF8) {
                    uint32_t next = fat_get_entry(c);
                    fat_set_entry(c, 0);
                    c = next;
                }
                if (fat_state.fat_cache_dirty) {
                    write_sector(fat_state.fat_cache_sector, fat_state.fat_cache);
                    fat_state.fat_cache_dirty = 0;
                }
            }
            if (!file_exists) {
                fat_rm(path);
            }
            vga_print_color("Disk full\n", 0x0C);
            return -1;
        }
        
        if (first_cluster == 0) first_cluster = cluster;
        if (prev_cluster != 0) {
            fat_set_entry(prev_cluster, cluster);
        }
        prev_cluster = cluster;
        
        uint32_t sector = cluster_to_sector(cluster);
        for (int s = 0; s < fat_state.sectors_per_cluster && bytes_written < size; s++) {
            uint32_t chunk = size - bytes_written;
            if (chunk > bps) chunk = bps;
            
            memset(fat_state.sector_buf, 0, bps);
            memcpy(fat_state.sector_buf, src + bytes_written, chunk);
            
            if (write_sector(sector + s, fat_state.sector_buf) < 0) return -1;
            bytes_written += bps;
        }
    }
    
    if (fat_state.fat_cache_dirty) {
        write_sector(fat_state.fat_cache_sector, fat_state.fat_cache);
        fat_state.fat_cache_dirty = 0;
    }
    
    char parent_path[FAT_MAX_PATH];
    char filename[FAT_MAX_NAME];
    
    strncpy(parent_path, path, FAT_MAX_PATH - 1);
    char* last_slash = strrchr(parent_path, '/');
    if (last_slash) {
        strcpy(filename, last_slash + 1);
        if (last_slash == parent_path) parent_path[1] = '\0';
        else *last_slash = '\0';
    } else {
        strcpy(filename, path);
        strcpy(parent_path, ".");
    }
    
    uint32_t parent_cluster;
    if (strcmp(parent_path, ".") == 0) {
        parent_cluster = fat_state.current_cluster;
    } else if (strcmp(parent_path, "/") == 0) {
        parent_cluster = (fat_state.type == FAT_TYPE_32) ? fat_state.root_cluster : 0;
    } else {
        fat_dir_entry_t pentry;
        fat_resolve_path(parent_path, &parent_cluster, &pentry);
    }
    
    char fat_name[11];
    str_to_fat_name(filename, fat_name);
    uint16_t entries_per_sec = fat_state.entries_per_sector;
    
    if (parent_cluster == 0 && fat_state.type != FAT_TYPE_32) {
        for (uint32_t s = 0; s < fat_state.root_dir_sectors; s++) {
            read_sector(fat_state.root_dir_sector + s, fat_state.sector_buf);
            fat_dir_entry_t* entries = (fat_dir_entry_t*)fat_state.sector_buf;
            
            for (uint16_t i = 0; i < entries_per_sec; i++) {
                if (memcmp(entries[i].name, fat_name, 11) == 0) {
                    entries[i].cluster_lo = first_cluster & 0xFFFF;
                    entries[i].cluster_hi = (first_cluster >> 16) & 0xFFFF;
                    entries[i].file_size = size;
                    write_sector(fat_state.root_dir_sector + s, fat_state.sector_buf);
                    return 0;
                }
            }
        }
    } else {
        uint32_t cluster = parent_cluster;
        while (cluster < 0x0FFFFFF8) {
            uint32_t sector = cluster_to_sector(cluster);
            
            for (int s = 0; s < fat_state.sectors_per_cluster; s++) {
                read_sector(sector + s, fat_state.sector_buf);
                fat_dir_entry_t* entries = (fat_dir_entry_t*)fat_state.sector_buf;
                
                for (uint16_t i = 0; i < entries_per_sec; i++) {
                    if (memcmp(entries[i].name, fat_name, 11) == 0) {
                        entries[i].cluster_lo = first_cluster & 0xFFFF;
                        entries[i].cluster_hi = (first_cluster >> 16) & 0xFFFF;
                        entries[i].file_size = size;
                        write_sector(sector + s, fat_state.sector_buf);
                        return 0;
                    }
                }
            }
            cluster = fat_get_entry(cluster);
        }
    }
    
    return 0;
}

int fat_mkdir(const char* path) {
    if (!fat_state.mounted) {
        vga_print_color("No filesystem mounted\n", 0x0C);
        return -1;
    }
    
    char parent_path[FAT_MAX_PATH];
    char dirname[FAT_MAX_NAME];
    
    strncpy(parent_path, path, FAT_MAX_PATH - 1);
    parent_path[FAT_MAX_PATH - 1] = '\0';
    
    char* last_slash = strrchr(parent_path, '/');
    if (last_slash) {
        strcpy(dirname, last_slash + 1);
        if (last_slash == parent_path) parent_path[1] = '\0';
        else *last_slash = '\0';
    } else {
        strcpy(dirname, path);
        strcpy(parent_path, ".");
    }
    
    if (!is_valid_name(dirname)) {
        vga_print_color("Invalid directory name\n", 0x0C);
        return -1;
    }
    
    uint32_t parent_cluster;
    if (strcmp(parent_path, ".") == 0) {
        parent_cluster = fat_state.current_cluster;
    } else if (strcmp(parent_path, "/") == 0) {
        parent_cluster = (fat_state.type == FAT_TYPE_32) ? fat_state.root_cluster : 0;
    } else {
        fat_dir_entry_t pentry;
        if (fat_resolve_path(parent_path, &parent_cluster, &pentry) < 0) {
            vga_print_color("Parent not found\n", 0x0C);
            return -1;
        }
    }
    
    fat_dir_entry_t existing;
    if (fat_find_in_dir(parent_cluster, dirname, &existing) == 0) {
        vga_print_color("Already exists\n", 0x0C);
        return -1;
    }
    
    uint32_t new_cluster = fat_alloc_cluster();
    if (new_cluster == 0) {
        vga_print_color("Disk full\n", 0x0C);
        return -1;
    }
    
    memset(fat_state.sector_buf, 0, fat_state.bytes_per_sector);
    fat_dir_entry_t* entries = (fat_dir_entry_t*)fat_state.sector_buf;
    
    memset(entries[0].name, ' ', 11);
    entries[0].name[0] = '.';
    entries[0].attr = FAT_ATTR_DIRECTORY;
    entries[0].cluster_lo = new_cluster & 0xFFFF;
    entries[0].cluster_hi = (new_cluster >> 16) & 0xFFFF;
    
    memset(entries[1].name, ' ', 11);
    entries[1].name[0] = '.';
    entries[1].name[1] = '.';
    entries[1].attr = FAT_ATTR_DIRECTORY;
    entries[1].cluster_lo = parent_cluster & 0xFFFF;
    entries[1].cluster_hi = (parent_cluster >> 16) & 0xFFFF;
    
    write_sector(cluster_to_sector(new_cluster), fat_state.sector_buf);
    
    memset(fat_state.sector_buf, 0, fat_state.bytes_per_sector);
    for (int s = 1; s < fat_state.sectors_per_cluster; s++) {
        write_sector(cluster_to_sector(new_cluster) + s, fat_state.sector_buf);
    }
    
    char short_name[11];
    str_to_fat_name(dirname, short_name);
    
    uint32_t entry_sector;
    int entry_index;
    
    if (needs_lfn(dirname)) {
        if (create_lfn_entries(parent_cluster, dirname, short_name, &entry_sector, &entry_index) < 0) {
            fat_set_entry(new_cluster, 0);
            if (fat_state.fat_cache_dirty) {
                write_sector(fat_state.fat_cache_sector, fat_state.fat_cache);
                fat_state.fat_cache_dirty = 0;
            }
            vga_print_color("Directory full or disk full\n", 0x0C);
            return -1;
        }
    } else {
        if (find_empty_entries(parent_cluster, 1, &entry_sector, &entry_index) < 0) {
            fat_set_entry(new_cluster, 0);
            if (fat_state.fat_cache_dirty) {
                write_sector(fat_state.fat_cache_sector, fat_state.fat_cache);
                fat_state.fat_cache_dirty = 0;
            }
            vga_print_color("Directory full or disk full\n", 0x0C);
            return -1;
        }
    }
    
    read_sector(entry_sector, fat_state.sector_buf);
    entries = (fat_dir_entry_t*)fat_state.sector_buf;
    
    memset(&entries[entry_index], 0, sizeof(fat_dir_entry_t));
    memcpy(entries[entry_index].name, short_name, 11);
    entries[entry_index].attr = FAT_ATTR_DIRECTORY;
    entries[entry_index].cluster_lo = new_cluster & 0xFFFF;
    entries[entry_index].cluster_hi = (new_cluster >> 16) & 0xFFFF;
    
    write_sector(entry_sector, fat_state.sector_buf);
    
    return 0;
}

int fat_rm(const char* path) {
    if (!fat_state.mounted) {
        vga_print_color("No filesystem mounted\n", 0x0C);
        return -1;
    }
    
    char parent_path[FAT_MAX_PATH];
    char name[FAT_MAX_NAME];
    
    strncpy(parent_path, path, FAT_MAX_PATH - 1);
    char* last_slash = strrchr(parent_path, '/');
    if (last_slash) {
        strcpy(name, last_slash + 1);
        if (last_slash == parent_path) parent_path[1] = '\0';
        else *last_slash = '\0';
    } else {
        strcpy(name, path);
        strcpy(parent_path, ".");
    }
    
    uint32_t parent_cluster;
    if (strcmp(parent_path, ".") == 0) {
        parent_cluster = fat_state.current_cluster;
    } else if (strcmp(parent_path, "/") == 0) {
        parent_cluster = (fat_state.type == FAT_TYPE_32) ? fat_state.root_cluster : 0;
    } else {
        fat_dir_entry_t pentry;
        if (fat_resolve_path(parent_path, &parent_cluster, &pentry) < 0) {
            vga_print_color("Parent not found\n", 0x0C);
            return -1;
        }
    }
    
    fat_dir_entry_t entry;
    if (fat_find_in_dir(parent_cluster, name, &entry) < 0) {
        vga_print_color("Not found\n", 0x0C);
        return -1;
    }
    
    uint32_t cluster = get_entry_cluster(&entry);
    while (cluster >= 2 && cluster < 0x0FFFFFF8) {
        uint32_t next = fat_get_entry(cluster);
        fat_set_entry(cluster, 0);
        cluster = next;
    }
    
    if (fat_state.fat_cache_dirty) {
        write_sector(fat_state.fat_cache_sector, fat_state.fat_cache);
        fat_state.fat_cache_dirty = 0;
    }
    
    char fat_name[11];
    str_to_fat_name(name, fat_name);
    uint16_t entries_per_sec = fat_state.entries_per_sector;
    
    if (parent_cluster == 0 && fat_state.type != FAT_TYPE_32) {
        for (uint32_t s = 0; s < fat_state.root_dir_sectors; s++) {
            read_sector(fat_state.root_dir_sector + s, fat_state.sector_buf);
            fat_dir_entry_t* entries = (fat_dir_entry_t*)fat_state.sector_buf;
            
            for (uint16_t i = 0; i < entries_per_sec; i++) {
                if (memcmp(entries[i].name, fat_name, 11) == 0) {
                    entries[i].name[0] = 0xE5;
                    write_sector(fat_state.root_dir_sector + s, fat_state.sector_buf);
                    return 0;
                }
            }
        }
    } else {
        cluster = parent_cluster;
        while (cluster < 0x0FFFFFF8) {
            uint32_t sector = cluster_to_sector(cluster);
            
            for (int s = 0; s < fat_state.sectors_per_cluster; s++) {
                read_sector(sector + s, fat_state.sector_buf);
                fat_dir_entry_t* entries = (fat_dir_entry_t*)fat_state.sector_buf;
                
                for (uint16_t i = 0; i < entries_per_sec; i++) {
                    if (memcmp(entries[i].name, fat_name, 11) == 0) {
                        entries[i].name[0] = 0xE5;
                        write_sector(sector + s, fat_state.sector_buf);
                        return 0;
                    }
                }
            }
            cluster = fat_get_entry(cluster);
        }
    }
    
    return 0;
}

void fat_info(void) {
    if (!fat_state.mounted) {
        vga_print_color("No filesystem mounted\n", 0x0C);
        return;
    }
    
    char buf[32];
    
    vga_print_color("=== FAT Filesystem Info ===\n", 0x0E);
    
    vga_print_color("Type: ", 0x0F);
    vga_print_color(fat_get_type_str(), 0x0A);
    vga_putc('\n');
    
    vga_print_color("Volume: ", 0x0F);
    vga_print_color(fat_state.volume_label, 0x0A);
    vga_putc('\n');
    
    vga_print_color("Bytes/Sector: ", 0x0F);
    itoa(fat_state.bytes_per_sector, buf, 10);
    vga_print_color(buf, 0x0A);
    vga_putc('\n');
    
    vga_print_color("Sectors/Cluster: ", 0x0F);
    itoa(fat_state.sectors_per_cluster, buf, 10);
    vga_print(buf);
    vga_putc('\n');
    
    vga_print_color("Total Clusters: ", 0x0F);
    itoa(fat_state.total_clusters, buf, 10);
    vga_print(buf);
    vga_putc('\n');
    
    uint32_t cluster_bytes = fat_state.sectors_per_cluster * fat_state.bytes_per_sector;
    uint32_t total_mb = (fat_state.total_clusters * cluster_bytes) / (1024 * 1024);
    vga_print_color("Total Size: ", 0x0F);
    itoa(total_mb, buf, 10);
    vga_print(buf);
    vga_print_color(" MB\n", 0x0F);
}

int fat_exists(const char* path) {
    if (!fat_state.mounted) return 0;
    
    uint32_t cluster;
    fat_dir_entry_t entry;
    return (fat_resolve_path(path, &cluster, &entry) == 0) ? 1 : 0;
}

int fat_is_dir(const char* path) {
    if (!fat_state.mounted) return 0;
    
    uint32_t cluster;
    fat_dir_entry_t entry;
    
    if (fat_resolve_path(path, &cluster, &entry) < 0) return 0;
    return (entry.attr & FAT_ATTR_DIRECTORY) ? 1 : 0;
}