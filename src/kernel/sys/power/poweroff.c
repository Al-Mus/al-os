#include "../../drivers/vga/vga.h"
#include "../../utils/ports.h"
#include "../../drivers/vga/colors.h"
#include "power.h"


void do_poweroff(void) {
    vga_print_color("Shutting down...\n", LIGHT_RED);
    for (volatile int i = 0; i < 50000000; i++);

    asm volatile("cli");

    outw(0x604, 0x2000);

    outw(0xB004, 0x2000);

    outw(0x4004, 0x3400);

    outb(0x64, 0xFE);

    outb(0x92, 0x01);

    vga_print_color("Shutdown failed. Please power off manually.\n", LIGHT_RED);
    while (1) {
        asm volatile("hlt");
    }
}
