#include "../../drivers/vga/vga.h"
#include "../../utils/ports.h"
#include "../../drivers/vga/colors.h"

#include "power.h"


void do_reboot(void) {
    vga_print_color("Rebooting...\n", LIGHT_RED);
    for (volatile int i = 0; i < 50000000; i++);
    asm volatile("cli");
    outb(0x64, 0xFE);
    outb(0x92, 0x01);
    outw(0xCF9, 0x06);
    while (1) asm("hlt");
}
