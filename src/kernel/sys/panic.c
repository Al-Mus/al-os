#include <stdint.h>
#include "../drivers/vga/vga.h"


#define BLUE_BG_WHITE 0x1F
#define BLUE_BG_RED 0x1C
#define BLUE_BG_YELLOW 0x1E


__attribute__((noreturn))
void panic(const char *module, const char *reason, const char *function) {
    asm volatile("cli");

    fill_screen_with_color(BLUE_BG_WHITE);

    vga_print_centered("KERNEL PANIC", 6, BLUE_BG_RED);

    char msg[96] = {0};
    int pos = 0;
    if (module && *module) {
        while (*module && pos < 70) msg[pos++] = *module++;
        if (pos < 70) msg[pos++] = ':';
        if (pos < 70) msg[pos++] = ' ';
    }
    if (reason && *reason) {
        while (*reason && pos < 70) msg[pos++] = *reason++;
    }
    vga_print_centered(msg[0] ? msg : "Unknown fatal error", 10, BLUE_BG_WHITE);

    if (function && *function) {
        vga_print_centered("in function:", 14, BLUE_BG_YELLOW);
        vga_print_centered(function, 16, BLUE_BG_YELLOW);
    }

    int seconds = 10;
    while (seconds >= 0) {
        char buf[32] = "Reboot in ";
        int i = 10;
        int t = seconds;
        if (t == 0) {
            buf[i++] = '0';
        } else {
            char tmp[4];
            int j = 0;
            while (t > 0) {
                tmp[j++] = '0' + (t % 10);
                t /= 10;
            }
            while (j > 0) buf[i++] = tmp[--j];
        }
        buf[i++] = 's';
        buf[i] = 0;
        vga_print_centered(buf, 22, BLUE_BG_WHITE);

        for (volatile int d = 0; d < 90000000; d++) asm("pause");

        seconds--;
    }

    asm volatile(
        "mov $0xFE, %%al;"
        "out %%al, $0x64;"
        ::: "eax"
    );

    asm volatile(
        "mov $0x02, %%al;"
        "out %%al, $0x92;"
        ::: "eax"
    );

    while (1) asm("hlt");
}
