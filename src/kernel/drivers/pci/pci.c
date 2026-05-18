#include "pci.h"
#include "../vga/vga.h"
#include "../vga/colors.h"
#include "../../utils/ports.h"
#include "../../utils/string.h"

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

    for (uint16_t bus = 0; bus < 256; bus++) {
        for (uint8_t slot = 0; slot < 32; slot++) {
            // Читаем Vendor ID (смещение 0)
            uint16_t vendor_id = pci_config_read_word(bus, slot, 0, 0);

            // Если Vendor ID = 0xFFFF, значит устройства в этом слоте нет
            if (vendor_id == 0xFFFF) continue;

            // Читаем Device ID (смещение 2)
            uint16_t device_id = pci_config_read_word(bus, slot, 0, 2);

            devices_found++;

            vga_print("Bus ");
            char buf[16];
            itoa(bus, buf, 10); vga_print_color(buf, YELLOW);
            vga_print(" Slot ");
            itoa(slot, buf, 10); vga_print_color(buf, YELLOW);

            vga_print(" | Vendor: 0x");
            itoa(vendor_id, buf, 16); vga_print_color(buf, LIGHT_GREEN);
            vga_print(" Device: 0x");
            itoa(device_id, buf, 16); vga_print_color(buf, LIGHT_GREEN);

            // Если это наша сетевая карта RTL8139 (Vendor: 0x10EC, Device: 0x8139)
            if (vendor_id == 0x10EC && device_id == 0x8139) {
                vga_print_color("  <-- RTL8139 NETWORK CARD FOUND!", LIGHT_RED);
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
