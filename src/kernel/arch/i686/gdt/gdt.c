#include "gdt.h"

// Нам нужно всего 3 записи
gdt_entry_t gdt[3];
gdt_ptr_t   gdt_ptr;

// Эту функцию мы напишем в ассемблере, она применит таблицу
extern void gdt_flush(uint32_t gdt_ptr_addr);

// Утилита для красивого заполнения битовых полей дескриптора
static void gdt_set_gate(int32_t num, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran) {
    gdt[num].base_low    = (base & 0xFFFF);
    gdt[num].base_middle = (base >> 16) & 0xFF;
    gdt[num].base_high   = (base >> 24) & 0xFF;

    gdt[num].limit_low   = (limit & 0xFFFF);
    gdt[num].granularity = (limit >> 16) & 0x0F;

    gdt[num].granularity |= gran & 0xF0;
    gdt[num].access      = access;
}

void init_gdt(void) {
    // Вычисляем размер таблицы
    gdt_ptr.limit = (sizeof(gdt_entry_t) * 3) - 1;
    gdt_ptr.base  = (uint32_t)(uintptr_t)&gdt;

    // 1. Нулевой дескриптор (обязательно должен быть пустым)
    gdt_set_gate(0, 0, 0, 0, 0);

    // 2. Сегмент кода ядра.
    // База = 0, Лимит = 4 ГБ (0xFFFFFFFF).
    // 0x9A = Присутствует в памяти, Кольцо 0, Исполняемый.
    // 0xCF = Гранулярность 4 КБ (размер лимита умножается на 4096), 32-битный режим.
    gdt_set_gate(1, 0, 0xFFFFFFFF, 0x9A, 0xCF);

    // 3. Сегмент данных ядра.
    // Всё то же самое, но флаг 0x92 означает "Данные: разрешено чтение и запись".
    gdt_set_gate(2, 0, 0xFFFFFFFF, 0x92, 0xCF);

    // Передаем адрес структуры в ассемблер для загрузки
    gdt_flush((uint32_t)(uintptr_t)&gdt_ptr);
}
