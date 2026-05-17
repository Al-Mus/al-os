#ifndef PIC_H
#define PIC_H

#include <stdint.h>

// Порты ввода-вывода для PIC
#define PIC1_COMMAND 0x20
#define PIC1_DATA    0x21
#define PIC2_COMMAND 0xA0
#define PIC2_DATA    0xA1

// Команда End of Interrupt (EOI)
#define PIC_EOI      0x20

// --- КАРТА АППАРАТНЫХ ПРЕРЫВАНИЙ (IRQ) ---
#define IRQ_TIMER    0  // Системный таймер (PIT)
#define IRQ_KEYBOARD 1  // Клавиатура (PS/2)
#define IRQ_CASCADE  2  // Каскадный сигнал для связи Master и Slave PIC
#define IRQ_COM2     3  // Последовательный порт COM2
#define IRQ_COM1     4  // Последовательный порт COM1
#define IRQ_LPT2     5  // Параллельный порт LPT2
#define IRQ_FLOPPY   6  // Дисковод гибких дисков
#define IRQ_LPT1     7  // Параллельный порт LPT1

#define IRQ_CMOS     8  // Часы реального времени (RTC)
#define IRQ_PCI1     9  // Перенаправленные PCI-прерывания / Сетевая карта
#define IRQ_PCI2     10
#define IRQ_PCI3     11
#define IRQ_MOUSE    12 // Мышь PS/2
#define IRQ_FPU      13 // Сопроцессор (FPU)
#define IRQ_ATA1     14 // Контроллер жесткого диска (Primary IDE)
#define IRQ_ATA2     15 // Контроллер жесткого диска (Secondary IDE)

void pic_remap(int offset1, int offset2);
void pic_send_eoi(uint8_t irq);
void pic_set_mask(unsigned char irq_line);
void pic_clear_mask(unsigned char irq_line);


#endif
