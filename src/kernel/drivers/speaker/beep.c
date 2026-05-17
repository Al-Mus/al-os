#include "../../utils/ports.h"
#include "beep.h"


void beep_pit(unsigned int frequency, unsigned int duration_ms) {
    if (frequency == 0) return;
    unsigned int div = 1193180 / frequency;
    outb(0x43, 0xB6);
    outb(0x42, div & 0xFF);
    outb(0x42, (div >> 8) & 0xFF);
    unsigned char tmp = inb(0x61);
    outb(0x61, tmp | 3);
    for (volatile unsigned int i = 0; i < duration_ms * 10000; i++);
    outb(0x61, tmp & 0xFC);
}
