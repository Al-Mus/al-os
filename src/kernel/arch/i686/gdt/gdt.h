#ifndef GDT_H
#define GDT_H

#include <stdint.h>

// Структура одного дескриптора сегмента (8 байт)
struct gdt_entry_struct {
    uint16_t limit_low;           // Младшие 16 бит лимита
    uint16_t base_low;            // Младшие 16 бит базового адреса
    uint8_t  base_middle;         // Следующие 8 бит базового адреса
    uint8_t  access;              // Байт доступа (права кольца, тип сегмента)
    uint8_t  granularity;         // Гранулярность и старшие 4 бита лимита
    uint8_t  base_high;           // Старшие 8 бит базового адреса
} __attribute__((packed));

typedef struct gdt_entry_struct gdt_entry_t;

// Структура, которую мы скормим инструкции lgdt (6 байт)
struct gdt_ptr_struct {
    uint16_t limit;               // Размер всей таблицы GDT минус 1
    uint32_t base;                // Линейный адрес самой таблицы
} __attribute__((packed));

typedef struct gdt_ptr_struct gdt_ptr_t;

// Функция инициализации GDT
void init_gdt(void);

#endif
