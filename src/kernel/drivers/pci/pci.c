#include "pci.h"
#include "../vga/vga.h"
#include "../vga/colors.h"
#include "../../utils/ports.h"
#include "../../utils/string.h"
#include "../net/rtl8139.h"

// Функция чтения 16-битного слова из конфигурационного пространства PCI
uint16_t pci_config_read_word(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t address;
    uint32_t lbus  = (uint32_t)bus;
    uint32_t lslot = (uint32_t)slot;
    uint32_t lfunc = (uint32_t)func;

    // Формируем адрес:
    // Бит 31: Enable bit (1)
    // Биты 23-16: Номер шины (Bus)
    // Биты 15-11: Номер устройства (Slot)
    // Биты 10-8: Номер функции (Func)
    // Биты 7-2: Смещение регистра (Offset)
    address = (uint32_t)((lbus << 16) | (lslot << 11) |
    (lfunc << 8) | (offset & 0xFC) | ((uint32_t)0x80000000));

    // Пишем адрес в порт 0xCF8
    outl(0xCF8, address);

    // Читаем данные из порта 0xCFC, сдвигаем и маскируем нужные 16 бит
    return (uint16_t)((inl(0xCFC) >> ((offset & 2) * 8)) & 0xFFFF);
}

// Простой сканер PCI, который выведет все найденные устройства
void pci_scan_bus() {
    vga_print_color("Scanning PCI bus...\n", LIGHT_CYAN);

    int devices_found = 0;
    char buf[16];

    for (uint16_t bus = 0; bus < 256; bus++) {
        for (uint8_t slot = 0; slot < 32; slot++) {
            uint16_t vendor_id = pci_config_read_word(bus, slot, 0, 0);
            if (vendor_id == 0xFFFF) continue;

            uint16_t device_id = pci_config_read_word(bus, slot, 0, 2);
            devices_found++;

            vga_print("Bus ");
            itoa(bus, buf, 10); vga_print_color(buf, YELLOW);
            vga_print(" Slot ");
            itoa(slot, buf, 10); vga_print_color(buf, YELLOW);

            vga_print(" | Vendor: 0x");
            itoa(vendor_id, buf, 16); vga_print_color(buf, LIGHT_GREEN);
            vga_print(" Device: 0x");
            itoa(device_id, buf, 16); vga_print_color(buf, LIGHT_GREEN);

            // Если нашли нашу сетевую карту
            if (vendor_id == 0x10EC && device_id == 0x8139) {
                vga_print_color("  <-- RTL8139 FOUND!\n", LIGHT_RED);

                // Читаем BAR0 (Базовый адрес ввода-вывода). Он занимает 32 бита (два слова по 16 бит)
                uint32_t bar0_low = pci_config_read_word(bus, slot, 0, 0x10);
                uint32_t bar0_high = pci_config_read_word(bus, slot, 0, 0x12);
                uint32_t bar0 = (bar0_high << 16) | bar0_low;

                // Бит 0 в BAR указывает тип: 1 - I/O порты, 0 - Memory Mapping.
                // Нам нужен именно Memory/IO порт назначения. Маскируем нижние биты.
                uint32_t io_base = bar0 & ~0x3;

                // Читаем Interrupt Line (смещение 0x3C, нижний байт указывает IRQ)
                uint16_t irq_data = pci_config_read_word(bus, slot, 0, 0x3C);
                uint8_t irq = irq_data & 0xFF;

                vga_print_color("    [NET] IO Base (BAR0): 0x", LIGHT_CYAN);
                itoa(io_base, buf, 16); vga_print_color(buf, WHITE);

                vga_print_color(" | IRQ: ", LIGHT_CYAN);
                itoa(irq, buf, 10); vga_print_color(buf, WHITE);

                rtl8139_init(io_base, irq);
            }

            vga_putc('\n');
        }
    }

    if (devices_found == 0) {
        vga_print_color("No PCI devices found.\n", LIGHT_RED);
    } else {
        vga_print_color("PCI scan complete.\n", LIGHT_CYAN);
    }
}
