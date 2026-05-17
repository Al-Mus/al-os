#ifndef TIME_H
#define TIME_H
#include "../../kernel.h"
#include <stdint.h>

typedef struct {
    int sec, min, hour, day, month, year;
} rtc_time;

long time_to_seconds(const rtc_time* t);
void wait_cycles(uint32_t cycles);
void rtrim_spaces(char* s);
void rtc_read(rtc_time* t);
int read_timezone_offset(void);
void apply_timezone(rtc_time* t, int offset);

extern long boot_seconds;

#endif
