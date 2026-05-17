#include <stddef.h>
#include "time.h"
#include "../../fs/fat/fat.h"
#include "../../utils/ports.h"
#include "../../utils/string.h"


int bcd2bin(int v) { return (v & 0x0F) + ((v >> 4) * 10); }

void rtc_read(rtc_time* t) {
    unsigned char status;
    do { outb(0x70, 0x0A); status = inb(0x71); } while (status & 0x80);
    outb(0x70, 0x00); unsigned char sec = inb(0x71);
    outb(0x70, 0x02); unsigned char min = inb(0x71);
    outb(0x70, 0x04); unsigned char hour = inb(0x71);
    outb(0x70, 0x07); unsigned char day = inb(0x71);
    outb(0x70, 0x08); unsigned char month = inb(0x71);
    outb(0x70, 0x09); unsigned char year = inb(0x71);
    outb(0x70, 0x0B); unsigned char statusB = inb(0x71);

    if (!(statusB & 0x04)) {
        t->sec = bcd2bin(sec);
        t->min = bcd2bin(min);
        t->hour = bcd2bin(hour);
        t->day = bcd2bin(day);
        t->month = bcd2bin(month);
        t->year = bcd2bin(year) + 2000;
    } else {
        t->sec = sec; t->min = min; t->hour = hour; t->day = day; t->month = month; t->year = 2000 + year;
    }
}

long days_from_civil(int y, unsigned m, unsigned d) {
    y -= m <= 2;
    long era = (y >= 0 ? y : y-399) / 400;
    unsigned yoe = (unsigned)(y - era * 400);
    unsigned doy = (153*(m + (m > 2 ? -3 : 9)) + 2)/5 + d - 1;
    unsigned doe = yoe * 365 + yoe/4 - yoe/100 + doy;
    return era * 146097 + (long)doe - 719468;
}

long time_to_seconds(const rtc_time* t) {
    long days = days_from_civil(t->year, t->month, t->day);
    return days * 86400 + t->hour * 3600 + t->min * 60 + t->sec;
}

void wait_cycles(uint32_t cycles) {
    for (volatile uint32_t i = 0; i < cycles; i++);
}

void rtrim_spaces(char* s) {
    int len = strlen(s);
    while (len > 0 && (s[len - 1] == ' ' || s[len - 1] == '\t')) {
        s[--len] = '\0';
    }
}

int read_timezone_offset(void) {
    if (!fat_is_mounted() || !fat_exists("/etc/localtime"))
        return 0;

    char buf[16];
    int bytes = fat_read("/etc/localtime", buf, 15);
    if (bytes <= 0) return 0;

    buf[bytes] = '\0';

    return atoi(buf);
}

void apply_timezone(rtc_time* t, int offset) {
    t->hour += offset;

    while (t->hour >= 24) {
        t->hour -= 24;
        t->day += 1;
    }

    while (t->hour < 0) {
        t->hour += 24;
        t->day -= 1;
    }
}
