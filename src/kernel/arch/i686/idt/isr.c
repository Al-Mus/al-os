#include "isr.h"
#include "../pic/pic.h"
#include "../../../sys/panic.h"
#include "../../../drivers/keyboard/keyboard.h"


extern void timer_handler(void);

// Текстовые описания первых 32 исключений x86
const char *exception_messages[] = {
    "Division By Zero",
    "Debug",
    "Non Maskable Interrupt",
    "Breakpoint",
    "Into Detected Overflow",
    "Out of Bounds",
    "Invalid Opcode",
    "No Coprocessor",
    "Double Fault",
    "Coprocessor Segment Overrun",
    "Bad TSS",
    "Segment Not Present",
    "Stack Fault",
    "General Protection Fault",
    "Page Fault",
    "Unknown Interrupt",
    "Coprocessor Fault",
    "Alignment Check",
    "Machine Check",
    "SIMD Floating-Point",
    "Virtualization Exception",
    "Control Protection Exception",
    "Reserved", "Reserved", "Reserved", "Reserved",
    "Reserved", "Reserved", "Reserved", "Reserved",
    "Reserved", "Reserved"
};

// Сюда прыгает ассемблерный stub
void isr_handler(registers_t regs) {
    if (regs.int_no < 32) {
        panic("ISR", exception_messages[regs.int_no], "isr_handler");
    }
}

void irq_handler(registers_t regs) {
    uint8_t irq_no = regs.int_no - 32;

    switch (irq_no) {
        case IRQ_TIMER:
            timer_handler();
            break;
        case IRQ_KEYBOARD:
            keyboard_handler();
            break;
        default:
            break;
    }

    pic_send_eoi(irq_no);
}
