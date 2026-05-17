#include "../drivers/vga/vga.h"
#include "../drivers/time/time.h"
#include "../utils/string.h"
#include "../drivers/vga/colors.h"

#include "all_commands.h"

void time_cmd()
{
    rtc_time now; rtc_read(&now);
    char buf[32];
    itoa(now.hour, buf, 10); vga_print_color(buf, YELLOW); vga_print_color(":", 0x0F);
    itoa(now.min, buf, 10); vga_print_color(buf, YELLOW); vga_print_color(":", 0x0F);
    itoa(now.sec, buf, 10); vga_print_color(buf, YELLOW); vga_putc('\n');
}
