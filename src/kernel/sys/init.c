#include "init.h"
#include "../drivers/vga/vga.h"
#include "../fs/memory_fs/fs.h"
#include "../drivers/time/time.h"


void init_system_base() {
    vga_clear();
    vga_print_color("Welcome to AL-OS!\n", 0x0A);
    vga_print_color("Type 'help' to see available commands\n\n", 0x0F);

    fs_init();
    fs_cd("/home");

    rtc_time boot_time;
    rtc_read(&boot_time);
    boot_seconds = time_to_seconds(&boot_time);
}
