#include "idt.h"
#include "isr.h"

idt_entry_t idt[256];
idt_ptr_t   idt_ptr;

// Наша функция из kernel_entry.asm
extern void idt_flush(uint32_t idt_ptr_addr);

extern void irq0(void);
extern void irq1(void);

void idt_set_gate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags) {
    idt[num].base_lo = base & 0xFFFF;
    idt[num].base_hi = (base >> 16) & 0xFFFF;

    idt[num].sel     = sel;
    idt[num].always0 = 0;
    idt[num].flags   = flags;
}

void init_idt(void) {
    idt_ptr.limit = (sizeof(idt_entry_t) * 256) - 1;
    idt_ptr.base  = (uint32_t)(uintptr_t)&idt;

    for (int i = 0; i < 256; i++) {
        idt_set_gate(i, 0, 0, 0);
    }

    // Регистрируем первые 32 исключения процессора
    // Флаг 0x8E означает: прерывание присутствует, кольцо 0 (ядро), 32-битные ворота
    idt_set_gate(0, (uint32_t)(uintptr_t)isr0, 0x08, 0x8E);
    idt_set_gate(1, (uint32_t)(uintptr_t)isr1, 0x08, 0x8E);
    idt_set_gate(2, (uint32_t)(uintptr_t)isr2, 0x08, 0x8E);
    idt_set_gate(3, (uint32_t)(uintptr_t)isr3, 0x08, 0x8E);
    idt_set_gate(4, (uint32_t)(uintptr_t)isr4, 0x08, 0x8E);
    idt_set_gate(5, (uint32_t)(uintptr_t)isr5, 0x08, 0x8E);
    idt_set_gate(6, (uint32_t)(uintptr_t)isr6, 0x08, 0x8E);
    idt_set_gate(7, (uint32_t)(uintptr_t)isr7, 0x08, 0x8E);
    idt_set_gate(8, (uint32_t)(uintptr_t)isr8, 0x08, 0x8E);
    idt_set_gate(9, (uint32_t)(uintptr_t)isr9, 0x08, 0x8E);
    idt_set_gate(10, (uint32_t)(uintptr_t)isr10, 0x08, 0x8E);
    idt_set_gate(11, (uint32_t)(uintptr_t)isr11, 0x08, 0x8E);
    idt_set_gate(12, (uint32_t)(uintptr_t)isr12, 0x08, 0x8E);
    idt_set_gate(13, (uint32_t)(uintptr_t)isr13, 0x08, 0x8E);
    idt_set_gate(14, (uint32_t)(uintptr_t)isr14, 0x08, 0x8E);
    idt_set_gate(15, (uint32_t)(uintptr_t)isr15, 0x08, 0x8E);
    idt_set_gate(16, (uint32_t)(uintptr_t)isr16, 0x08, 0x8E);
    idt_set_gate(17, (uint32_t)(uintptr_t)isr17, 0x08, 0x8E);
    idt_set_gate(18, (uint32_t)(uintptr_t)isr18, 0x08, 0x8E);
    idt_set_gate(19, (uint32_t)(uintptr_t)isr19, 0x08, 0x8E);
    idt_set_gate(20, (uint32_t)(uintptr_t)isr20, 0x08, 0x8E);
    idt_set_gate(21, (uint32_t)(uintptr_t)isr21, 0x08, 0x8E);
    idt_set_gate(22, (uint32_t)(uintptr_t)isr22, 0x08, 0x8E);
    idt_set_gate(23, (uint32_t)(uintptr_t)isr23, 0x08, 0x8E);
    idt_set_gate(24, (uint32_t)(uintptr_t)isr24, 0x08, 0x8E);
    idt_set_gate(25, (uint32_t)(uintptr_t)isr25, 0x08, 0x8E);
    idt_set_gate(26, (uint32_t)(uintptr_t)isr26, 0x08, 0x8E);
    idt_set_gate(27, (uint32_t)(uintptr_t)isr27, 0x08, 0x8E);
    idt_set_gate(28, (uint32_t)(uintptr_t)isr28, 0x08, 0x8E);
    idt_set_gate(29, (uint32_t)(uintptr_t)isr29, 0x08, 0x8E);
    idt_set_gate(30, (uint32_t)(uintptr_t)isr30, 0x08, 0x8E);
    idt_set_gate(31, (uint32_t)(uintptr_t)isr31, 0x08, 0x8E);

    idt_set_gate(32, (uint32_t)(uintptr_t)irq0, 0x08, 0x8E);
    idt_set_gate(33, (uint32_t)(uintptr_t)irq1, 0x08, 0x8E);

    idt_flush((uint32_t)(uintptr_t)&idt_ptr);
}
