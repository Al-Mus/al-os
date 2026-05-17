#include "all_commands.h"
#include "../drivers/vga/vga.h"
#include "../utils/string.h"

void cmd_colorbar(void) {
    const char *colors[] = {
        "Black   ", "Blue    ", "Green   ", "Cyan    ",
        "Red     ", "Magenta ", "Brown   ", "Light Gray",
        "Dark Gray", "Light Blue", "Light Green", "Light Cyan",
        "Light Red", "Light Magenta", "Yellow  ", "White   "
    };

    vga_print("\n");

    for (int bg = 0; bg < 8; bg++) {
        for (int fg = 0; fg < 16; fg++) {
            uint8_t color = (bg << 4) | fg;
            uint8_t old_color = vga_color;
            vga_color = color;

            vga_print("  ");
            char num[4];
            itoa(fg + bg*16, num, 10);
            if (strlen(num) == 1) vga_print("0");
            if (strlen(num) == 2) vga_print(" ");
            vga_print(num);

            vga_color = old_color;
            vga_print(" ");
        }
        vga_putc('\n');
    }

    vga_print("\nStandard VGA text colors (0-15 foreground, 0-7 background):\n");
    for (int i = 0; i < 16; i++) {
        char buf[8];
        itoa(i, buf, 10);
        vga_print_color(buf, i);
        vga_print(" ");
        vga_print_color(colors[i], i);
        vga_putc('\n');
    }
    vga_putc('\n');
}
