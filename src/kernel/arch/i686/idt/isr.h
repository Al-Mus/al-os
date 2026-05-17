#ifndef ISR_H
#define ISR_H

#include <stdint.h>

// Структура, описывающая состояние процессора в момент прерывания
struct registers_struct {
    uint32_t ds;                                     // Сегмент данных, сохраненный нами вручную
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax; // Сохранено командой pusha
    uint32_t int_no, err_code;                       // Номер прерывания и код ошибки, сохраненные макросом
    uint32_t eip, cs, eflags, useresp, ss;           // Автоматически сохранено процессором
} __attribute__((packed));

typedef struct registers_struct registers_t;

// Прототип главного обработчика, который вызывается из ассемблера
void isr_handler(registers_t regs);

// Объявляем внешние ассемблерные заглушки для IDT
extern void isr0(void);
extern void isr1(void);
extern void isr2(void);
extern void isr3(void);
extern void isr4(void);
extern void isr5(void);
extern void isr6(void);
extern void isr7(void);
extern void isr8(void);
extern void isr9(void);
extern void isr10(void);
extern void isr11(void);
extern void isr12(void);
extern void isr13(void);
extern void isr14(void);
extern void isr15(void);
extern void isr16(void);
extern void isr17(void);
extern void isr18(void);
extern void isr19(void);
extern void isr20(void);
extern void isr21(void);
extern void isr22(void);
extern void isr23(void);
extern void isr24(void);
extern void isr25(void);
extern void isr26(void);
extern void isr27(void);
extern void isr28(void);
extern void isr29(void);
extern void isr30(void);
extern void isr31(void);

#endif
