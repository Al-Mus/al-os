#include "all_commands.h"
#include "../drivers/ata/ata.h"
#include "../drivers/vga/vga.h"
#include "../utils/string.h"
#include "../drivers/vga/colors.h"

void cmd_disks() {
    ata_init();
    vga_print_color("Detected drives:\n", YELLOW);
    for (int i = 0; i < 4; i++) {
        if (ata_drive_exists(i)) {
            ata_device_t* dev = ata_get_device(i);
            char buf[16];
            itoa(i, buf, 10);
            vga_print_color("  Drive ", 0x0F);
            vga_print(buf);
            vga_print_color(": ", 0x0F);
            vga_print_color(dev->model, 0x0A);
            vga_print_color(" (", 0x08);
            itoa(dev->size / 2048, buf, 10);
            vga_print(buf);
            vga_print_color(" MB)\n", 0x08);
        }
    }
}
