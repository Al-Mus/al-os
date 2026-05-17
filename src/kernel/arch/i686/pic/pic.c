#include "pic.h"
#include "../../../utils/ports.h"

static inline void io_wait(void) {
    __asm__ __volatile__("outb %%al, $0x80" : : "a"(0));
}

void pic_remap(int offset1, int offset2) {
    uint8_t a1, a2;

    // Считываем текущие маски, чтобы ничего не сломать
    a1 = inb(PIC1_DATA);
    a2 = inb(PIC2_DATA);

    // Начинаем инициализацию (ICW1)
    outb(PIC1_COMMAND, 0x11);
    io_wait();
    outb(PIC2_COMMAND, 0x11);
    io_wait();

    // Задаем базовые векторы (ICW2)
    outb(PIC1_DATA, offset1); // Теперь IRQ0-7 станут векторами 32-39
    io_wait();
    outb(PIC2_DATA, offset2); // Теперь IRQ8-15 станут векторами 40-47
    io_wait();

    // Связываем Master и Slave PIC (ICW3)
    outb(PIC1_DATA, 0x04);
    io_wait();
    outb(PIC2_DATA, 0x02);
    io_wait();

    // Режим 8086 (ICW4)
    outb(PIC1_DATA, 0x01);
    io_wait();
    outb(PIC2_DATA, 0x01);
    io_wait();

    // Возвращаем маски на место
    // outb(PIC1_DATA, a1);
    // outb(PIC2_DATA, a2);


    // Жестко задаем маски безопасности:
    // Master PIC: 0xfc (11111100b) -> Разрешены только IRQ0 (таймер) и IRQ1 (клавиатура)
    // Если в будущем понадобятся прерывания диска (IRQ14), надо будет записать 0xf8 (открыть еще и IRQ2 каскад)
    outb(PIC1_DATA, 0xFC);
    io_wait();

    // Slave PIC: 0xff (11111111b) -> Глушим вообще все прерывания на ведомом контроллере
    outb(PIC2_DATA, 0xFF);
    io_wait();
}

void pic_send_eoi(uint8_t irq) {
    if (irq >= 8) {
        outb(PIC2_COMMAND, PIC_EOI);
    }
    outb(PIC1_COMMAND, PIC_EOI);
}

void pic_set_mask(unsigned char irq_line) {
    uint16_t port;
    uint8_t value;

    if (irq_line < 8) {
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        irq_line -= 8;
    }
    value = inb(port) | (1 << irq_line);
    outb(port, value);
}

void pic_clear_mask(unsigned char irq_line) {
    uint16_t port;
    uint8_t value;

    if (irq_line < 8) {
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        irq_line -= 8;
    }
    value = inb(port) & ~(1 << irq_line);
    outb(port, value);
}
