#include "all_commands.h"
#include "../drivers/net/rtl8139.h"
#include "../drivers/net/net.h"
#include "../drivers/vga/vga.h"
#include "../drivers/vga/colors.h"
#include "../utils/string.h"

extern void rtl8139_send_packet(void* data, uint32_t len);

// Переворот 16-битных значений (Short)
static inline uint16_t htons(uint16_t v) {
    return (v >> 8) | (v << 8);
}

// Переворот 32-битных значений (Long) для правильного сетевого порядка IP
static inline uint32_t htonl(uint32_t v) {
    return ((v & 0xFF000000) >> 24) |
    ((v & 0x00FF0000) >> 8)  |
    ((v & 0x0000FF00) << 8)  |
    ((v & 0x000000FF) << 24);
}

void net_test() {
    uint8_t packet[sizeof(struct eth_hdr) + sizeof(struct arp_hdr)];

    struct eth_hdr *eth = (struct eth_hdr *)packet;
    struct arp_hdr *arp = (struct arp_hdr *)(packet + sizeof(struct eth_hdr));

    // 1. ЗАПОЛНЯЕМ ETHERNET ЗАГОЛОВОК
    // Broadcast: отправляем на FF:FF:FF:FF:FF:FF
    for(int i = 0; i < 6; i++) eth->dest_mac[i] = 0xFF;

    // Наш MAC
    eth->src_mac[0] = 0x52; eth->src_mac[1] = 0x54; eth->src_mac[2] = 0x00;
    eth->src_mac[3] = 0x12; eth->src_mac[4] = 0x34; eth->src_mac[5] = 0x56;

    eth->type = htons(0x0806); // Тип: ARP

    // 2. ЗАПОЛНЯЕМ ARP ЗАГОЛОВОК
    arp->hw_type = htons(1);          // Ethernet
    arp->proto_type = htons(0x0800);   // IPv4
    arp->hw_len = 6;
    arp->proto_len = 4;
    arp->opcode = htons(1);           // ARP Request

    // Копируем MAC
    for(int i = 0; i < 6; i++) arp->src_mac[i] = eth->src_mac[i];

    // Переводим IP в правильный Network Byte Order (Big Endian)
    // 10.0.2.15 -> 0x0A00020F
    arp->src_ip = htonl(0x0A00020F);

    // Целевой MAC обнуляем
    for(int i = 0; i < 6; i++) arp->dest_mac[i] = 0x00;

    // Искомый IP роутера QEMU: 10.0.2.2 -> 0x0A000202
    arp->dest_ip = htonl(0x0A000202);

    vga_print_color("[NET] Blasting REAL ARP Request to QEMU gateway (10.0.2.2)...\n", LIGHT_CYAN);

    // Отправляем пакет
    rtl8139_send_packet(packet, sizeof(packet));

    // Важно: даем эмулятору чуть больше времени обработать пакет и вернуть ответ в буфер
    for(volatile uint32_t i = 0; i < 40000000; i++) {
        __asm__ volatile("nop");
    }

    vga_print_color("[NET] Checking if QEMU router replied to us:\n", LIGHT_CYAN);

    // Смотрим в кольцевой буфер
    rtl8139_receive();
}
