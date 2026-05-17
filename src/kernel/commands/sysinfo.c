#include "../drivers/vga/vga.h"
#include "all_commands.h"
#include "../sys/metadata.h"
#include "../drivers/vga/colors.h"
#include "../utils/string.h"

void cmd_sysinfo(void) {
    vga_print_color("=== AL-OS ===\n", LIGHT_MAGENTA);
    vga_print_color("Arch: i686\nBuild: v", WHITE);
    vga_print_color(VERSION, WHITE);
    if ((strcmp(RELEASE_NOTES, "")) != 0)
    {
        vga_print_color(" - ", WHITE);
        vga_print_color(RELEASE_NOTES, WHITE);
    }

    vga_print_color("\n", WHITE);
}
