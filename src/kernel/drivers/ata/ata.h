#ifndef ATA_H
#define ATA_H

#include <stdint.h>

/* ATA Primary Channel Ports */
#define ATA_PRIMARY_DATA        0x1F0
#define ATA_PRIMARY_ERROR       0x1F1
#define ATA_PRIMARY_FEATURES    0x1F1
#define ATA_PRIMARY_SECCOUNT    0x1F2
#define ATA_PRIMARY_LBA_LO      0x1F3
#define ATA_PRIMARY_LBA_MID     0x1F4
#define ATA_PRIMARY_LBA_HI      0x1F5
#define ATA_PRIMARY_DRIVE       0x1F6
#define ATA_PRIMARY_STATUS      0x1F7
#define ATA_PRIMARY_COMMAND     0x1F7
#define ATA_PRIMARY_CTRL        0x3F6

/* ATA Secondary Channel Ports */
#define ATA_SECONDARY_DATA      0x170
#define ATA_SECONDARY_ERROR     0x171
#define ATA_SECONDARY_SECCOUNT  0x172
#define ATA_SECONDARY_LBA_LO    0x173
#define ATA_SECONDARY_LBA_MID   0x174
#define ATA_SECONDARY_LBA_HI    0x175
#define ATA_SECONDARY_DRIVE     0x176
#define ATA_SECONDARY_STATUS    0x177
#define ATA_SECONDARY_COMMAND   0x177
#define ATA_SECONDARY_CTRL      0x376

/* Status Register Bits */
#define ATA_SR_BSY      0x80    /* Busy */
#define ATA_SR_DRDY     0x40    /* Drive Ready */
#define ATA_SR_DF       0x20    /* Drive Fault */
#define ATA_SR_DSC      0x10    /* Drive Seek Complete */
#define ATA_SR_DRQ      0x08    /* Data Request */
#define ATA_SR_CORR     0x04    /* Corrected Data */
#define ATA_SR_IDX      0x02    /* Index */
#define ATA_SR_ERR      0x01    /* Error */

/* Commands */
#define ATA_CMD_READ_PIO        0x20
#define ATA_CMD_READ_PIO_EXT    0x24
#define ATA_CMD_WRITE_PIO       0x30
#define ATA_CMD_WRITE_PIO_EXT   0x34
#define ATA_CMD_CACHE_FLUSH     0xE7
#define ATA_CMD_CACHE_FLUSH_EXT 0xEA
#define ATA_CMD_IDENTIFY        0xEC

/* Drive selection */
#define ATA_MASTER      0x00
#define ATA_SLAVE       0x01

/* Sector size */
#define ATA_SECTOR_SIZE 512

typedef struct {
    uint8_t     present;
    uint8_t     channel;        /* 0 = primary, 1 = secondary */
    uint8_t     drive;          /* 0 = master, 1 = slave */
    uint16_t    signature;
    uint16_t    capabilities;
    uint32_t    command_sets;
    uint32_t    size;           /* Size in sectors */
    char        model[41];
} ata_device_t;

/* Initialize ATA subsystem, detect drives */
int ata_init(void);

/* Read sectors from drive */
int ata_read_sectors(uint8_t drive, uint32_t lba, uint8_t count, void* buffer);

/* Write sectors to drive */
int ata_write_sectors(uint8_t drive, uint32_t lba, uint8_t count, const void* buffer);

/* Get drive info */
ata_device_t* ata_get_device(uint8_t drive);

/* Check if drive exists */
int ata_drive_exists(uint8_t drive);

#endif