#include "all_commands.h"
#include "../kernel.h"
#include "../drivers/vga/vga.h"

void cmd_whoami(void) {
    vga_print_color(user, 0x0A);
    vga_print_color("\n", 0x0F);
}
