#include "rtl8139.h"
#include "../../utils/ports.h"
#include "../vga/vga.h"
#include "../vga/colors.h"
#include "../../utils/string.h"

static uint32_t rtl_io_base = 0;
static uint32_t rx_offset = 0;
static uint8_t  rtl_irq = 0;
static uint8_t  mac_address[6] = {0};

static uint8_t rx_buffer[8192 + 16] __attribute__((aligned(4)));
static uint8_t tx_buffer[1514] __attribute__((aligned(4)));

extern uint16_t pci_config_read_word(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);
static inline void pci_config_write_word(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint16_t value) {
    uint32_t address = (uint32_t)((bus << 16) | (slot << 11) | (func << 8) | (offset & 0xFC) | 0x80000000);

    __asm__ volatile ("outl %0, %%dx" : : "a"(address), "d"((uint16_t)0xCF8));

    uint16_t port = (uint16_t)(0xCFC + (offset & 2));

    __asm__ volatile ("outw %%ax, %%dx" : : "a"(value), "d"(port));
}

void rtl8139_init(uint32_t io_base, uint8_t irq) {
    rtl_io_base = io_base;
    rtl_irq = irq;

    vga_print_color("[RTL8139] Initializing network card...\n", LIGHT_CYAN);

    uint8_t bus = 0, slot = 3, func = 0;
    uint16_t pci_cmd = pci_config_read_word(bus, slot, func, 0x04);
    pci_cmd |= (1 << 0);
    pci_cmd |= (1 << 2);
    pci_config_write_word(bus, slot, func, 0x04, pci_cmd);

    outb(rtl_io_base + RTL_REG_CONFIG1, 0x00);

    outb(rtl_io_base + RTL_REG_CR, 0x10);
    vga_print("[RTL8139] Resetting card... ");

    while((inb(rtl_io_base + RTL_REG_CR) & 0x10) != 0) {

    }
    vga_print_color("OK\n", LIGHT_GREEN);

    uint32_t rx_buffer_ptr = (uint32_t)&rx_buffer;
    outl(rtl_io_base + RTL_REG_RBSTART, rx_buffer_ptr);

    outw(rtl_io_base + RTL_REG_IMR, RTL_INT_ROK | RTL_INT_TOK);

    outl(rtl_io_base + RTL_REG_RCR, 0x0F | 0x80);

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
        if(mac_address[i] < 16) vga_print("0");
        vga_print_color(hex, YELLOW);
        if(i < 5) vga_print(":");
    }
    vga_putc('\n');

    outb(rtl_io_base + RTL_REG_CR, 0x0C);

    vga_print_color("[RTL8139] Card is UP and RUNNING!\n", LIGHT_GREEN);
}

void rtl8139_receive() {
    if (inb(rtl_io_base + RTL_REG_CR) & 0x01) {
        return;
    }

    vga_print_color("[RTL8139] Multi-packet chunk detected!\n", LIGHT_CYAN);

    while ((inb(rtl_io_base + RTL_REG_CR) & 0x01) == 0) {

        uint32_t buf_index = rx_offset;

        uint16_t packet_status = *(uint16_t*)(rx_buffer + buf_index);
        uint16_t packet_length = *(uint16_t*)(rx_buffer + buf_index + 2);

        if (!(packet_status & 0x01)) {
            vga_print_color("[RTL8139] Rx error! Resetting buffer pointers.\n", LIGHT_RED);
            outw(rtl_io_base + RTL_REG_CAPR, 0);
            rx_offset = 0;
            return;
        }

        uint8_t* packet_data = rx_buffer + buf_index + 4;
        uint32_t data_len = packet_length - 4;

        vga_print("[NET] Frame received! Size: ");
        char lbuf[16];
        itoa(data_len, lbuf, 10); vga_print_color(lbuf, YELLOW);
        vga_print(" bytes\n");

        vga_print("      From: ");
        char hbuf[4];
        for (int i = 6; i < 12; i++) {
            itoa(packet_data[i], hbuf, 16);
            if (packet_data[i] < 16) vga_print("0");
            vga_print_color(hbuf, LIGHT_GREEN);
            if (i < 11) vga_print(":");
        }

        uint16_t ethertype = (packet_data[12] << 8) | packet_data[13];
        vga_print(" | Type: 0x");
        itoa(ethertype, lbuf, 16); vga_print_color(lbuf, LIGHT_BLUE);
        vga_putc('\n');

        if (ethertype == 0x0806) {
            vga_print_color("      --> [Protocol] It's an ARP request/reply!\n", LIGHT_GREEN);
        } else if (ethertype == 0x0800) {
            uint8_t ip_proto = packet_data[23];

            if (ip_proto == 1) { // ICMP
                uint8_t* icmp_data = packet_data + 20;
                uint8_t icmp_type = icmp_data[0];

                if (icmp_type == 0) { // Echo Reply!
                    extern volatile int icmp_reply_received;
                    icmp_reply_received = 1; // Уведомляем команду ping, что пакет пришел!

                    vga_print("64 bytes from ");
                    char ip_buf[4];
                    for (int buf_i = 12; buf_i < 16; buf_i++) {
                        itoa(packet_data[buf_i], ip_buf, 10);
                        vga_print_color(ip_buf, LIGHT_GREEN);
                        if (buf_i < 15) vga_print(".");
                    }
                    vga_print(": icmp_seq=1 ttl=");
                    itoa(packet_data[22], ip_buf, 10);
                    vga_print_color(ip_buf, LIGHT_BLUE);
                    vga_print("\n");
                }
            }
        }

        rx_offset = (rx_offset + packet_length + 4 + 3) & ~3;

        rx_offset %= 8192;

        outw(rtl_io_base + RTL_REG_CAPR, (uint16_t)(rx_offset - 16));
    }
}

void rtl8139_send_packet(void* data, uint32_t len) {
    extern void* memcpy(void* dest, const void* src, size_t n);

    // Защита от слишком маленьких пакетов (минимальный размер Ethernet-кадра — 60 байт без CRC)
    // Если пакет меньше 60 байт, QEMU может его проигнорировать. Добьем его нулями.
    uint32_t send_len = len;
    if (send_len < 60) {
        send_len = 60;
    }

    // Обнуляем буфер перед копированием, чтобы "хвост" маленького пакета был чистым
    for (uint32_t i = 0; i < send_len; i++) {
        tx_buffer[i] = 0;
    }
    memcpy(tx_buffer, data, len);

    // Смещения регистров по спецификации Realtek RTL8139:
    // TSD0  (Transmit Status Descriptor 0) = rtl_io_base + 0x10
    // TSAD0 (Transmit Start Address 0)     = rtl_io_base + 0x20  <-- ВОТ ТУТ БЫЛА ОШИБКА!

    // 1. Сначала ПЕРВЫМ шагом даем карте физический адрес буфера
    outl(rtl_io_base + 0x20, (uint32_t)&tx_buffer);

    // 2. ВТОРЫМ шагом пишем статус и размер, что заставляет карту начать чтение памяти (DMA)
    // Очищаем бит OWN (записывая 0 в верхние биты) и передаем размер
    outl(rtl_io_base + 0x10, send_len & 0xFFF);

    // Безопасное ожидание завершения передачи (смотрим в регистр статуса TSD0 - 0x10)
    volatile uint32_t timeout = 500000;
    while (((inl(rtl_io_base + 0x10) & 0x8000) == 0) && timeout > 0) {
        __asm__ volatile("nop");
        timeout--;
    }

    if (timeout == 0) {
        vga_print_color("[RTL8139] Send TX timeout! TSD0 status: 0x", LIGHT_RED);
        char sbuf[16];
        itoa(inl(rtl_io_base + 0x10), sbuf, 16);
        vga_print_color(sbuf, LIGHT_RED);
        vga_putc('\n');
    } else {
        vga_print_color("[RTL8139] Hardware TX finished successfully!\n", LIGHT_GREEN);
    }
}

void rtl8139_handler() {
    uint16_t status = inw(rtl_io_base + RTL_REG_ISR);

    outw(rtl_io_base + RTL_REG_ISR, status);

    if (status & RTL_INT_ROK) {
        vga_print_color("[RTL8139] Packet received!\n", LIGHT_GREEN);
    }
    if (status & RTL_INT_TOK) {
        vga_print_color("[RTL8139] Packet transmitted successfully.\n", LIGHT_CYAN);
    }
}
