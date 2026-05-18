#ifndef RTL8139_H
#define RTL8139_H

#include <stdint.h>

// Основные регистры RTL8139 относительно IO Base
#define RTL_REG_MAC0         0x00    // MAC адрес (байты 0-3)
#define RTL_REG_MAC4         0x04    // MAC адрес (байты 4-5)
#define RTL_REG_RBSTART      0x30    // Начало Rx буфера (Receive Buffer Start)
#define RTL_REG_CR           0x37    // Регистр команд (Command Register)
#define RTL_REG_CAPR         0x38    // Текущий адрес чтения Rx (Current Address of Packet Read)
#define RTL_REG_IMR          0x3C    // Маска прерываний (Interrupt Mask Register)
#define RTL_REG_ISR          0x3E    // Статус прерываний (Interrupt Status Register)
#define RTL_REG_TCR          0x40    // Конфигурация передачи (Transmit Configuration Register)
#define RTL_REG_RCR          0x44    // Конфигурация приема (Receive Configuration Register)
#define RTL_REG_CONFIG1      0x52    // Регистр конфигурации 1

// Биты прерываний (IMR / ISR)
#define RTL_INT_ROK          0x01    // Receive OK (Пакет успешно принят)
#define RTL_INT_TOK          0x02    // Transmit OK (Пакет успешно отправлен)


void rtl8139_init(uint32_t io_base, uint8_t irq);
void rtl8139_receive();
void rtl8139_send_packet(void* data, uint32_t len);
void rtl8139_handler();

#endif
