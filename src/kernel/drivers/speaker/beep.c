#include "../../utils/ports.h"
#include "beep.h"
#include "../../arch/i686/timer/timer.h" // Подключаем твой системный таймер

void beep_pit(unsigned int frequency, unsigned int duration_ms) {
    if (frequency == 0) return;

    // Защита от переполнения 16-битного делителя PIT (минимальная частота ~19 Гц)
    if (frequency < 19) {
        frequency = 19;
    }

    unsigned int div = 1193180 / frequency;

    outb(0x43, 0xB6);
    outb(0x42, div & 0xFF);
    outb(0x42, (div >> 8) & 0xFF);

    unsigned char tmp = inb(0x61);
    outb(0x61, tmp | 3);

    // Вместо пустого цикла "for" используем надежный sleep из таймера
    sleep(duration_ms);

    outb(0x61, tmp & 0xFC);
}
