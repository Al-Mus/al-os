#include "timer.h"
#include "../../../utils/ports.h"

// Глобальный счетчик системных тиков
static volatile uint32_t timer_ticks = 0;
static uint32_t system_frequency = 0;

// Эта функция будет вызываться из нашего irq_handler при каждом IRQ0
void timer_handler(void) {
    timer_ticks++;
}

void init_timer(uint32_t frequency) {
    system_frequency = frequency;

    // Вычисляем делитель
    uint32_t divisor = 1193182 / frequency;

    // Отправляем команду настройки: Канал 0, доступ к LSB/MSB, Режим 3 (квадратная волна), 16-битный бинарный счетчик
    outb(PIT_COMMAND, 0x36);

    // Разделяем 16-битный делитель на два байта и отправляем в порт данных Канала 0
    uint8_t l = (uint8_t)(divisor & 0xFF);
    uint8_t h = (uint8_t)((divisor >> 8) & 0xFF);

    outb(PIT_CHANNEL_0, l);
    outb(PIT_CHANNEL_0, h);
}

uint32_t get_ticks(void) {
    return timer_ticks;
}

// Функция задержки в миллисекундах
void sleep(uint32_t ms) {
    uint32_t ticks_to_wait = (ms * system_frequency) / 1000;
    uint32_t eticks = get_ticks() + ticks_to_wait;

    while(get_ticks() < eticks) {
        __asm__ __volatile__("hlt"); // Теперь это будет работать идеально!
    }
}
