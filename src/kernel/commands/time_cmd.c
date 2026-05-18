#include "../drivers/vga/vga.h"
#include "../drivers/time/time.h"
#include "../utils/string.h"
#include "../drivers/vga/colors.h"
#include "../fs/fat/fat.h" // Нужен для проверки монтирования часового пояса

#include "all_commands.h"

void time_cmd()
{
    rtc_time now;
    rtc_read(&now);

    // Читаем таймзону из файла и применяем её к текущему времени
    int tz = read_timezone_offset();
    apply_timezone(&now, tz);

    char buf[16];

    // Вывод часов с ведущим нулем
    if (now.hour < 10) vga_print_color("0", YELLOW);
    itoa(now.hour, buf, 10);
    vga_print_color(buf, YELLOW);
    vga_print_color(":", 0x0F);

    // Вывод минут с ведущим нулем
    if (now.min < 10) vga_print_color("0", YELLOW);
    itoa(now.min, buf, 10);
    vga_print_color(buf, YELLOW);
    vga_print_color(":", 0x0F);

    // Вывод секунд с ведущим нулем
    if (now.sec < 10) vga_print_color("0", YELLOW);
    itoa(now.sec, buf, 10);
    vga_print_color(buf, YELLOW);

    // Бонус: если ФС смонтирована, красиво допишем часовой пояс в конце, например: (UTC+3)
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
