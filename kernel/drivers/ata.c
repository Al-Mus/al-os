#include "ata.h"
#include "../utils/ports.h"
#include "../utils/string.h"
#include "../drivers/vga.h"

/* Up to 4 drives: primary master/slave, secondary master/slave */
static ata_device_t ata_devices[4];
static int ata_initialized = 0;

/* I/O delay */
static void ata_io_wait(uint16_t ctrl_port) {
    inb(ctrl_port);
    inb(ctrl_port);
    inb(ctrl_port);
    inb(ctrl_port);
}

/* Wait for BSY to clear */
static int ata_wait_bsy(uint16_t status_port) {
    int timeout = 100000;
    while ((inb(status_port) & ATA_SR_BSY) && timeout > 0) {
        timeout--;
    }
    return timeout > 0 ? 0 : -1;
}

/* Wait for DRQ */
static int ata_wait_drq(uint16_t status_port) {
    int timeout = 100000;
    uint8_t status;
    while (timeout > 0) {
        status = inb(status_port);
        if (status & ATA_SR_ERR) return -1;
        if (status & ATA_SR_DF) return -1;
        if (status & ATA_SR_DRQ) return 0;
        timeout--;
    }
    return -1;
}

/* Poll status after command */
static int ata_poll(uint16_t status_port) {
    /* Wait 400ns */
    for (int i = 0; i < 4; i++) inb(status_port);
    
    if (ata_wait_bsy(status_port) < 0) return -1;
    
    uint8_t status = inb(status_port);
    if (status & ATA_SR_ERR) return -1;
    if (status & ATA_SR_DF) return -1;
    
    return 0;
}

/* Software reset */
static void ata_soft_reset(uint16_t ctrl_port) {
    outb(ctrl_port, 0x04);  /* Set SRST */
    ata_io_wait(ctrl_port);
    outb(ctrl_port, 0x00);  /* Clear SRST */
    ata_io_wait(ctrl_port);
}

/* Identify drive */
static int ata_identify(uint8_t channel, uint8_t drive, ata_device_t* dev) {
    uint16_t io_base = (channel == 0) ? ATA_PRIMARY_DATA : ATA_SECONDARY_DATA;
    uint16_t ctrl_base = (channel == 0) ? ATA_PRIMARY_CTRL : ATA_SECONDARY_CTRL;
    
    dev->present = 0;
    dev->channel = channel;
    dev->drive = drive;
    
    /* Select drive */
    outb(io_base + 6, 0xA0 | (drive << 4));
    ata_io_wait(ctrl_base);
    
    /* Send IDENTIFY command */
    outb(io_base + 2, 0);   /* Sector count */
    outb(io_base + 3, 0);   /* LBA lo */
    outb(io_base + 4, 0);   /* LBA mid */
    outb(io_base + 5, 0);   /* LBA hi */
    outb(io_base + 7, ATA_CMD_IDENTIFY);
    
    ata_io_wait(ctrl_base);
    
    /* Check if drive exists */
    uint8_t status = inb(io_base + 7);
    if (status == 0) return -1;  /* No drive */
    
    /* Wait for BSY to clear */
    if (ata_wait_bsy(io_base + 7) < 0) return -1;
    
    /* Check for ATAPI */
    uint8_t lba_mid = inb(io_base + 4);
    uint8_t lba_hi = inb(io_base + 5);
    if (lba_mid != 0 || lba_hi != 0) {
        /* This is ATAPI or SATA, skip for now */
        return -1;
    }
    
    /* Wait for DRQ or ERR */
    if (ata_wait_drq(io_base + 7) < 0) return -1;
    
    /* Read identify data */
    uint16_t identify[256];
    for (int i = 0; i < 256; i++) {
        identify[i] = inw(io_base);
    }
    
    dev->present = 1;
    dev->signature = identify[0];
    dev->capabilities = identify[49];
    dev->command_sets = ((uint32_t)identify[83] << 16) | identify[82];
    
    /* Get size */
    if (dev->command_sets & (1 << 26)) {
        /* 48-bit LBA */
        dev->size = ((uint32_t)identify[103] << 16) | identify[102];
    } else {
        /* 28-bit LBA */
        dev->size = ((uint32_t)identify[61] << 16) | identify[60];
    }
    
    /* Get model string (swap bytes) */
    for (int i = 0; i < 40; i += 2) {
        dev->model[i] = (char)(identify[27 + i/2] >> 8);
        dev->model[i + 1] = (char)(identify[27 + i/2] & 0xFF);
    }
    dev->model[40] = '\0';
    
    /* Trim trailing spaces */
    for (int i = 39; i >= 0; i--) {
        if (dev->model[i] == ' ') dev->model[i] = '\0';
        else break;
    }
    
    return 0;
}

int ata_init(void) {
    if (ata_initialized) return 0;
    
    memset(ata_devices, 0, sizeof(ata_devices));
    
    /* Reset both channels */
    ata_soft_reset(ATA_PRIMARY_CTRL);
    ata_soft_reset(ATA_SECONDARY_CTRL);
    
    /* Identify all drives */
    int found = 0;
    
    /* Primary Master (drive 0) */
    if (ata_identify(0, 0, &ata_devices[0]) == 0) found++;
    
    /* Primary Slave (drive 1) */
    if (ata_identify(0, 1, &ata_devices[1]) == 0) found++;
    
    /* Secondary Master (drive 2) */
    if (ata_identify(1, 0, &ata_devices[2]) == 0) found++;
    
    /* Secondary Slave (drive 3) */
    if (ata_identify(1, 1, &ata_devices[3]) == 0) found++;
    
    ata_initialized = 1;
    return found;
}

int ata_drive_exists(uint8_t drive) {
    if (drive >= 4) return 0;
    return ata_devices[drive].present;
}

ata_device_t* ata_get_device(uint8_t drive) {
    if (drive >= 4) return NULL;
    if (!ata_devices[drive].present) return NULL;
    return &ata_devices[drive];
}

int ata_read_sectors(uint8_t drive, uint32_t lba, uint8_t count, void* buffer) {
    if (drive >= 4 || !ata_devices[drive].present) return -1;
    if (count == 0) return -1;
    
    ata_device_t* dev = &ata_devices[drive];
    uint16_t io_base = (dev->channel == 0) ? ATA_PRIMARY_DATA : ATA_SECONDARY_DATA;
    uint16_t ctrl_base = (dev->channel == 0) ? ATA_PRIMARY_CTRL : ATA_SECONDARY_CTRL;
    
    /* Wait for drive to be ready */
    if (ata_wait_bsy(io_base + 7) < 0) return -1;
    
    /* Select drive with LBA mode */
    outb(io_base + 6, 0xE0 | (dev->drive << 4) | ((lba >> 24) & 0x0F));
    ata_io_wait(ctrl_base);
    
    /* Send read command */
    outb(io_base + 1, 0x00);                    /* Features */
    outb(io_base + 2, count);                   /* Sector count */
    outb(io_base + 3, (uint8_t)(lba & 0xFF));           /* LBA lo */
    outb(io_base + 4, (uint8_t)((lba >> 8) & 0xFF));    /* LBA mid */
    outb(io_base + 5, (uint8_t)((lba >> 16) & 0xFF));   /* LBA hi */
    outb(io_base + 7, ATA_CMD_READ_PIO);        /* Command */
    
    /* Read sectors */
    uint16_t* buf = (uint16_t*)buffer;
    for (int s = 0; s < count; s++) {
        if (ata_poll(io_base + 7) < 0) return -1;
        if (ata_wait_drq(io_base + 7) < 0) return -1;
        
        for (int i = 0; i < 256; i++) {
            buf[s * 256 + i] = inw(io_base);
        }
    }
    
    return 0;
}

int ata_write_sectors(uint8_t drive, uint32_t lba, uint8_t count, const void* buffer) {
    if (drive >= 4 || !ata_devices[drive].present) return -1;
    if (count == 0) return -1;
    
    ata_device_t* dev = &ata_devices[drive];
    uint16_t io_base = (dev->channel == 0) ? ATA_PRIMARY_DATA : ATA_SECONDARY_DATA;
    uint16_t ctrl_base = (dev->channel == 0) ? ATA_PRIMARY_CTRL : ATA_SECONDARY_CTRL;
    
    /* Wait for drive to be ready */
    if (ata_wait_bsy(io_base + 7) < 0) return -1;
    
    /* Select drive with LBA mode */
    outb(io_base + 6, 0xE0 | (dev->drive << 4) | ((lba >> 24) & 0x0F));
    ata_io_wait(ctrl_base);
    
    /* Send write command */
    outb(io_base + 1, 0x00);                    /* Features */
    outb(io_base + 2, count);                   /* Sector count */
    outb(io_base + 3, (uint8_t)(lba & 0xFF));           /* LBA lo */
    outb(io_base + 4, (uint8_t)((lba >> 8) & 0xFF));    /* LBA mid */
    outb(io_base + 5, (uint8_t)((lba >> 16) & 0xFF));   /* LBA hi */
    outb(io_base + 7, ATA_CMD_WRITE_PIO);       /* Command */
    
    /* Write sectors */
    const uint16_t* buf = (const uint16_t*)buffer;
    for (int s = 0; s < count; s++) {
        if (ata_poll(io_base + 7) < 0) return -1;
        if (ata_wait_drq(io_base + 7) < 0) return -1;
        
        for (int i = 0; i < 256; i++) {
            outw(io_base, buf[s * 256 + i]);
        }
        
        /* Flush cache */
        outb(io_base + 7, ATA_CMD_CACHE_FLUSH);
        if (ata_poll(io_base + 7) < 0) return -1;
    }
    
    return 0;
}