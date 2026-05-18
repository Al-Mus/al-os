#ifndef NET_H
#define NET_H

#include <stdint.h>

// Заголовок Ethernet кадра (14 байт)
struct eth_hdr {
    uint8_t  dest_mac[6];
    uint8_t  src_mac[6];
    uint16_t type;
} __attribute__((packed));

// Заголовок ARP пакета (28 байт)
struct arp_hdr {
    uint16_t hw_type;      // Тип оборудования (1 для Ethernet)
    uint16_t proto_type;   // Тип протокола (0x0800 для IPv4)
    uint8_t  hw_len;        // Длина MAC (6)
    uint8_t  proto_len;     // Длина IP (4)
    uint16_t opcode;        // Операция: 1 - запрос (Request), 2 - ответ (Reply)
    uint8_t  src_mac[6];    // MAC отправителя
    uint32_t src_ip;        // IP отправителя
    uint8_t  dest_mac[6];   // MAC получателя (при запросе заполняется нулями)
    uint32_t dest_ip;       // IP получателя
} __attribute__((packed));

struct ip_hdr {
    uint8_t  ver_ihl;        // Версия (4) и длина заголовка (5) -> 0x45
    uint8_t  tos;            // Type of Service (0)
    uint16_t len;            // Общая длина пакета (IP заголовок + данные)
    uint16_t id;             // Идентификатор пакета (любое число, например 0x1C)
    uint16_t flags_frag;     // Флаги фрагментации (0x4000 - Don't Fragment)
    uint8_t  ttl;            // Time to Live (обычно 64)
    uint8_t  proto;          // Протокол верхнего уровня (1 = ICMP)
    uint16_t checksum;       // Контрольная сумма IP-заголовка
    uint32_t src_ip;         // IP отправителя
    uint32_t dest_ip;        // IP получателя
} __attribute__((packed));

// Заголовок ICMP (8 байт для Echo)
struct icmp_hdr {
    uint8_t  type;           // Тип: 8 = Echo Request, 0 = Echo Reply
    uint8_t  code;           // Код (0)
    uint16_t checksum;       // Контрольная сумма ICMP
    uint16_t id;             // Идентификатор (например, 0x1234)
    uint16_t seq;            // Номер последовательности (например, 1)
} __attribute__((packed));

#endif
