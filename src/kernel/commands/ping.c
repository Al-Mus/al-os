#include "all_commands.h"
#include "../drivers/net/rtl8139.h"
#include "../drivers/net/net.h"
#include "../drivers/vga/vga.h"
#include "../drivers/vga/colors.h"
#include "../utils/string.h"

extern void rtl8139_send_packet(void* data, uint32_t len);

static inline uint16_t htons(uint16_t v) { return (v >> 8) | (v << 8); }
static inline uint32_t htonl(uint32_t v) {
    return ((v & 0xFF000000) >> 24) | ((v & 0x00FF0000) >> 8) |
    ((v & 0x0000FF00) << 8)  | ((v & 0x000000FF) << 24);
}

// Контрольная сумма Internet Checksum
static uint16_t net_checksum(void *addr, int count) {
    uint32_t sum = 0;
    uint16_t *ptr = (uint16_t *)addr;
    while (count > 1) { sum += *ptr++; count -= 2; }
    if (count > 0) { sum += *(uint8_t *)ptr; }
    while (sum >> 16) { sum = (sum & 0xFFFF) + (sum >> 16); }
    return (uint16_t)(~sum);
}

// Парсер строки "A.B.C.D" в 32-битное число (Network Byte Order)
static uint32_t parse_ip(char* ip_str) {
    uint32_t res = 0;
    uint32_t byte = 0;
    char* c = ip_str;

    for (int i = 0; i < 4; i++) {
        byte = 0;
        while (*c >= '0' && *c <= '9') {
            byte = byte * 10 + (*c - '0');
            c++;
        }
        res = (res << 8) | (byte & 0xFF);
        if (*c == '.') c++;
    }
    return htonl(res);
}

volatile int icmp_reply_received = 0;

void ping_cmd(char* args) {
    char target_str[64];
    if (args == 0 || args[0] == '\0') {
        vga_print_color("Usage: ping <IP_ADDRESS>\n", LIGHT_RED);
        return;
    } else {
        strcpy(target_str, args);
    }

    // ВАЖНО: Пока пингуем только сырые IP (например: 10.0.2.2 или 8.8.8.8)
    uint32_t dest_ip = parse_ip(target_str);

    vga_print("PING "); vga_print_color(target_str, YELLOW);
    vga_print(" (56(84) bytes of data).\n");

    uint8_t packet[74];
    for(int i = 0; i < 74; i++) packet[i] = 0;

    struct eth_hdr *eth  = (struct eth_hdr *)packet;
    struct ip_hdr  *ip   = (struct ip_hdr  *)(packet + sizeof(struct eth_hdr));
    struct icmp_hdr *icmp = (struct icmp_hdr *)(packet + sizeof(struct eth_hdr) + sizeof(struct ip_hdr));
    uint8_t *payload     = (uint8_t *)(packet + sizeof(struct eth_hdr) + sizeof(struct ip_hdr) + sizeof(struct icmp_hdr));

    // Направляем роутеру QEMU
    eth->dest_mac[0] = 0x52; eth->dest_mac[1] = 0x55; eth->dest_mac[2] = 0x0A;
    eth->dest_mac[3] = 0x00; eth->dest_mac[4] = 0x02; eth->dest_mac[5] = 0x02;

    eth->src_mac[0] = 0x52; eth->src_mac[1] = 0x54; eth->src_mac[2] = 0x00;
    eth->src_mac[3] = 0x12; eth->src_mac[4] = 0x34; eth->src_mac[5] = 0x56;
    eth->type = htons(0x0800); // IPv4

    // IP
    ip->ver_ihl = 0x45;
    ip->tos = 0;
    ip->len = htons(20 + 8 + 32);
    ip->id = htons(0x5566);
    ip->flags_frag = htons(0x4000);
    ip->ttl = 64;
    ip->proto = 1; // ICMP
    ip->src_ip = htonl(0x0A00020F); // 10.0.2.15
    ip->dest_ip = dest_ip;
    ip->checksum = net_checksum(ip, 20);

    // Payload
    for (int i = 0; i < 32; i++) payload[i] = 'A' + i;

    // ICMP
    icmp->type = 8; // Echo Request
    icmp->code = 0;
    icmp->id = htons(0x1122);
    icmp->seq = htons(1);
    icmp->checksum = net_checksum(icmp, 8 + 32);

    // Сбрасываем флаг перед отправкой
    icmp_reply_received = 0;

    // Отправляем пакет на карту
    rtl8139_send_packet(packet, sizeof(packet));

    // Начинаем активно опрашивать карту на предмет входящих пакетов (таймаут около 3 секунд)
    vga_print("Waiting for reply...\n");

    volatile uint32_t listen_timeout = 15000000;
    while (listen_timeout > 0 && icmp_reply_received == 0) {
        // Постоянно проверяем кольцевой буфер приемника
        rtl8139_receive();
        listen_timeout--;
        __asm__ volatile("nop");
    }

    // Если цикл завершился, а флаг так и не взведен — значит, ответа нет!
    if (icmp_reply_received == 0) {
        vga_print_color("Request timeout. No reply from ", LIGHT_RED);
        vga_print_color(target_str, LIGHT_RED);
        vga_print("\n");
    }
}
