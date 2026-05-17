#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>

// Порты PIT
#define PIT_CHANNEL_0 0x40
#define PIT_COMMAND   0x43

void init_timer(uint32_t frequency);
void sleep(uint32_t ms);
uint32_t get_ticks(void);

#endif
