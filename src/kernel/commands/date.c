#include "all_commands.h"
#include "../drivers/time/time.h"
#include "../drivers/vga/vga.h"
#include "../drivers/vga/colors.h"
#include "../utils/string.h"
#include "../fs/fat/fat.h"
#include <stddef.h>


void cmd_date(const char* args) {

    if (args && (args[0] == '+' || args[0] == '-')) {
        if (!fat_is_mounted()) {
            vga_print_color("Error: FAT not mounted. Cannot save timezone.\n", LIGHT_RED);
            return;
        }

        char tz_str[16];
        strcpy(tz_str, args);

        fat_touch("/etc/localtime");

        if (fat_write("/etc/localtime", tz_str, strlen(tz_str)) >= 0) {
            vga_print_color("Timezone updated to ", LIGHT_GREEN);
            vga_print_color(tz_str, YELLOW);
            vga_print_color("\n", LIGHT_GREEN);
        } else {
            vga_print_color("Failed to write /etc/localtime\n", LIGHT_RED);
        }
        return;
    }

    rtc_time now;
    rtc_read(&now);

    int tz = read_timezone_offset();
    apply_timezone(&now, tz);

    char buf[64];

    if (now.day < 10) vga_print("0");
    itoa(now.day, buf, 10);
    vga_print(buf);
    vga_print(".");

    if (now.month < 10) vga_print("0");
    itoa(now.month, buf, 10);
    vga_print(buf);
    vga_print(".");

    itoa(now.year, buf, 10);
    vga_print(buf);
    vga_print(" ");

    if (now.hour < 10) vga_print("0");
    itoa(now.hour, buf, 10);
    vga_print(buf);
    vga_print(":");

    if (now.min < 10) vga_print("0");
    itoa(now.min, buf, 10);
    vga_print(buf);
    vga_print(":");

    if (now.sec < 10) vga_print("0");
    itoa(now.sec, buf, 10);
    vga_print(buf);

    if (fat_is_mounted() && fat_exists("/etc/localtime")) {
        char tzbuf[16];
        int bytes = fat_read("/etc/localtime", tzbuf, 15);

        if (bytes > 0) {
            tzbuf[bytes] = '\0';
            vga_print_color(" [UTC", LIGHT_CYAN);
            vga_print_color(tzbuf, LIGHT_CYAN);
            vga_print_color("]", LIGHT_CYAN);
        }
    }

    vga_putc('\n');
}
