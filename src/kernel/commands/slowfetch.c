#include "all_commands.h"
#include "../drivers/vga/vga.h"
#include "../drivers/keyboard/keyboard.h"
#include "../drivers/vga/colors.h"
#include "../arch/i686/timer/timer.h"
#include "../sys/metadata.h"


static void slowprint_line(const char* str, uint8_t color, uint32_t delay_ms) {
    uint8_t old = vga_color;
    vga_color = color;

    for (int i = 0; str[i]; i++) {

        keyboard_poll();
        if (keyboard_sigint_check()) {
            vga_print_color("\nOperation cancelled\n", LIGHT_RED);
            vga_color = old;
            return;
        }

        vga_putc(str[i]);

        if (delay_ms) {
            sleep(delay_ms);
        }
    }

    vga_putc('\n');
    vga_color = old;
}

static void slowprint_logo() {
    for (int i = 0; AL_OS_LOGO[i]; i++) {
        slowprint_line(AL_OS_LOGO[i], 0x0B, 20);
    }
}

static void slowprint_release_note() {
        slowprint_line(RELEASE_NOTES, 0x0B, 20);
}

void cmd_slowfetch(void) {
    slowprint_line("", 0x0B, 20);
    slowprint_logo();
    slowprint_line("", 0x07, 20);
    slowprint_release_note();
    slowprint_line("", 0x07, 20);
}
