#ifndef PCI_H
#define PCI_H

#include <stdint.h>

// Базовые функции PCI
uint16_t pci_config_read_word(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);
void pci_scan_bus();

#endif
