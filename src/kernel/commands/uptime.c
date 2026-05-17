#include "all_commands.h"
#include "../drivers/time/time.h"
#include "../utils/string.h"
#include "../drivers/vga/vga.h"
#include "../drivers/vga/colors.h"


void uptime_cmd() {
    rtc_time now; rtc_read(&now);
    long now_s = time_to_seconds(&now);
    long diff = now_s - boot_seconds;
    if (diff < 0) diff = 0;
    int days = diff / 86400; diff %= 86400;
    int hours = diff / 3600; diff %= 3600;
    int mins = diff / 60; int secs = diff % 60;
    char buf[64];
    itoa(days, buf, 10); vga_print_color(buf, YELLOW); vga_print_color(" days ", 0x0F);
    itoa(hours, buf, 10); vga_print_color(buf, YELLOW); vga_print_color(":", 0x0F);
    itoa(mins, buf, 10); vga_print_color(buf, YELLOW); vga_print_color(":", 0x0F);
    itoa(secs, buf, 10); vga_print_color(buf, YELLOW); vga_putc('\n');
}
