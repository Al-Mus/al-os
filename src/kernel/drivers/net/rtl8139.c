#include "rtl8139.h"
#include "../../utils/ports.h"
#include "../vga/vga.h"
#include "../vga/colors.h"
#include "../../utils/string.h"

// Переменные для хранения состояния карты
static uint32_t rtl_io_base = 0;
static uint8_t  rtl_irq = 0;
static uint8_t  mac_address[6] = {0};

// Буфер для приема данных (8КБ + 16 байт запаса на оверфлоу по спецификации Realtek)
// В идеале память должна быть выровнена по границе 4 байт
static uint8_t rx_buffer[8192 + 16] __attribute__((aligned(4)));

// Функции чтения/записи в PCI конфигурацию (допишем внешние или используем обертки)
extern uint16_t pci_config_read_word(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);
// Нам понадобится запись в PCI, добавим локальный прототип
static inline void pci_config_write_word(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint16_t value) {
    uint32_t address = (uint32_t)((bus << 16) | (slot << 11) | (func << 8) | (offset & 0xFC) | 0x80000000);

    // Пишем 32-битный адрес в CONFIG_ADDRESS (0xCF8)
    __asm__ volatile ("outl %0, %%dx" : : "a"(address), "d"((uint16_t)0xCF8));

    // Вычисляем точный 16-битный порт внутри CONFIG_DATA (0xCFC)
    uint16_t port = (uint16_t)(0xCFC + (offset & 2));

    // Пишем 16-битное значение в вычисленный порт
    // Используем явное указание регистра %%dx для порта
    __asm__ volatile ("outw %%ax, %%dx" : : "a"(value), "d"(port));
}

void rtl8139_init(uint32_t io_base, uint8_t irq) {
    rtl_io_base = io_base;
    rtl_irq = irq;

    vga_print_color("[RTL8139] Initializing network card...\n", LIGHT_CYAN);

    // 1. ВКЛЮЧАЕМ BUS MASTERING И IO SPACE НА ШИНЕ PCI
    // Твоя карта сидит на Bus 0, Slot 3, Func 0 (мы увидели это на прошлом шаге)
    uint8_t bus = 0, slot = 3, func = 0;
    uint16_t pci_cmd = pci_config_read_word(bus, slot, func, 0x04); // Смещение 0x04 - Command Register в PCI
    pci_cmd |= (1 << 0); // IO Space Enable
    pci_cmd |= (1 << 2); // Bus Master Enable
    pci_config_write_word(bus, slot, func, 0x04, pci_cmd);

    // 2. РАЗБУДИТЬ КАРТУ (Включаем питание / Снимаем режим сна)
    outb(rtl_io_base + RTL_REG_CONFIG1, 0x00);

    // 3. ПРОГРАММНЫЙ СБРОС (Software Reset)
    outb(rtl_io_base + RTL_REG_CR, 0x10); // Отправляем бит RST (0x10)
    vga_print("[RTL8139] Resetting card... ");

    // Ждем, пока бит RST не сбросится обратно в 0
    while((inb(rtl_io_base + RTL_REG_CR) & 0x10) != 0) {
        // В будущем тут можно поставить таймаут, но в эмуляторах сброс мгновенный
    }
    vga_print_color("OK\n", LIGHT_GREEN);

    // 4. ПЕРЕДАЕМ АДРЕС ПРИЕМНОГО БУФЕРА КАРТЕ
    // Карте нужно передать ФИЗИЧЕСКИЙ адрес памяти. Поскольку у нас пока flat-модель
    // без сложного пейджинга процессов, адрес массива rx_buffer и есть физический адрес.
    uint32_t rx_buffer_ptr = (uint32_t)&rx_buffer;
    outl(rtl_io_base + RTL_REG_RBSTART, rx_buffer_ptr);

    // 5. НАСТРОЙКА МАСКИ ПРЕРЫВАНИЙ (IMR)
    // Разрешаем прерывания ROK (Пакет принят) и TOK (Пакет отправлен)
    outw(rtl_io_base + RTL_REG_IMR, RTL_INT_ROK | RTL_INT_TOK);

    // 6. НАСТРОЙКА КОНФИГУРАЦИИ ПРИЕМА (RCR)
    // Устанавливаем фильтры:
    // AB  (0x08) — Принимать Broadcast пакеты (крики ко всем в сети, например ARP)
    // AM  (0x04) — Принимать Multicast
    // APM (0x02) — Принимать пакеты, отправленные лично на наш MAC-адрес
    // AAP (0x01) — Промискуитетный режим (принимать вообще всё, полезно для снифферов, пока включим для тестов)
    // WRAP(0x80) — Разрешить кольцевой сдвиг буфера
    outl(rtl_io_base + RTL_REG_RCR, 0x0F | 0x80);

    // 7. ЧИТАЕМ АППАРАТНЫЙ MAC-АДРЕС ИЗ КАРТЫ (Она берет его из своей EEPROM)
    uint32_t mac_low = inl(rtl_io_base + RTL_REG_MAC0);
    uint32_t mac_high = inl(rtl_io_base + RTL_REG_MAC4);
    mac_address[0] = (uint8_t)(mac_low & 0xFF);
    mac_address[1] = (uint8_t)((mac_low >> 8) & 0xFF);
    mac_address[2] = (uint8_t)((mac_low >> 16) & 0xFF);
    mac_address[3] = (uint8_t)((mac_low >> 24) & 0xFF);
    mac_address[4] = (uint8_t)(mac_high & 0xFF);
    mac_address[5] = (uint8_t)((mac_high >> 8) & 0xFF);

    vga_print("[RTL8139] MAC Address: ");
    char hex[4];
    for(int i = 0; i < 6; i++) {
        itoa(mac_address[i], hex, 16);
        if(mac_address[i] < 16) vga_print("0"); // Ведущий ноль для красоты
        vga_print_color(hex, YELLOW);
        if(i < 5) vga_print(":");
    }
    vga_putc('\n');

    // 8. ВКЛЮЧАЕМ ПРИЕМ (RE) И ПЕРЕДАЧУ (TE) В РЕГИСТРЕ КОМАНД
    outb(rtl_io_base + RTL_REG_CR, 0x0C); // Бит RE (0x08) + Бит TE (0x04)

    vga_print_color("[RTL8139] Card is UP and RUNNING!\n", LIGHT_GREEN);
}

// Заглушка для обработчика прерываний
void rtl8139_handler() {
    // Читаем, что вызвало прерывание
    uint16_t status = inw(rtl_io_base + RTL_REG_ISR);

    // Сбрасываем прерывание (в RTL8139 для сброса нужно записать единицу в сработавший бит)
    outw(rtl_io_base + RTL_REG_ISR, status);

    if (status & RTL_INT_ROK) {
        vga_print_color("[RTL8139] Packet received!\n", LIGHT_GREEN);
        // Сюда мы вернемся, когда будем парсить Ethernet-кадры
    }
    if (status & RTL_INT_TOK) {
        vga_print_color("[RTL8139] Packet transmitted successfully.\n", LIGHT_CYAN);
    }
}
