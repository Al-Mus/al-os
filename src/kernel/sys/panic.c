#include <stdint.h>
#include "../drivers/vga/vga.h"
#include "../arch/i686/timer/timer.h"
#include "../utils/ports.h" // Подключаем outb/inb для работы с портами

#define BLUE_BG_WHITE  0x1F
#define BLUE_BG_RED    0x1C
#define BLUE_BG_YELLOW 0x1E
#define BLUE_BG_GRAY   0x17 // Менее заметный цвет: светло-серый текст на синем фоне

__attribute__((noreturn))
void panic(const char *module, const char *reason, const char *function) {
    // 1. Выключаем прерывания на время отрисовки интерфейса
    asm volatile("cli");

    fill_screen_with_color(BLUE_BG_WHITE);

    vga_print_centered("KERNEL PANIC", 6, BLUE_BG_RED);

    char msg[96] = {0};
    int pos = 0;
    if (module && *module) {
        while (*module && pos < 70) msg[pos++] = *module++;
        if (pos < 70) msg[pos++] = ':';
        if (pos < 70) msg[pos++] = ' ';
    }
    if (reason && *reason) {
        while (*reason && pos < 70) msg[pos++] = *reason++;
    }
    vga_print_centered(msg[0] ? msg : "Unknown fatal error", 10, BLUE_BG_WHITE);

    if (function && *function) {
        vga_print_centered("in function:", 14, BLUE_BG_YELLOW);
        vga_print_centered(function, 16, BLUE_BG_YELLOW);
    }

    // Добавляем неброскую надпись в самый низ экрана (например, на 24 строку)
    vga_print_centered("(Press Enter to restart)", 24, BLUE_BG_GRAY);

    // ВАЖНО: Маскируем IRQ1 (клавиатуру) в мастере PIC (порт 0x21), чтобы
    // прерывание от клавиатуры не улетало в драйвер, и мы могли опросить её вручную.
    // IRQ0 (таймер) остается включенным.
    outb(0x21, inb(0x21) | 0x02);

    // 2. Включаем прерывания обратно для работы sleep()
    asm volatile("sti");

    int forced_reboot = 0;
    int seconds = 10;
    while (seconds >= 0) {
        char buf[32] = "Reboot in ";
        int i = 10;
        int t = seconds;

        // Фикс бага смещения: выравниваем длину строки пробелом
        if (t < 10) {
            buf[i++] = ' '; // Добавляем пробел перед однозначным числом
            buf[i++] = '0' + t;
        } else {
            char tmp[4];
            int j = 0;
            while (t > 0) {
                tmp[j++] = '0' + (t % 10);
                t /= 10;
            }
            while (j > 0) buf[i++] = tmp[--j];
        }
        buf[i++] = ' ';
        buf[i++] = 's';
        buf[i++] = 'e';
        buf[i++] = 'c';
        buf[i] = 0;

        vga_print_centered(buf, 22, BLUE_BG_WHITE);

        // Вместо sleep(1000) делаем 100 проверок по 10 мс для мгновенного отклика
        for (int m = 0; m < 100; m++) {
            sleep(10);

            // Проверяем статус контроллера клавиатуры (порт 0x64, бит 0 — буфер полон)
            if (inb(0x64) & 0x01) {
                uint8_t scancode = inb(0x60);
                if (scancode == 0x1C) { // 0x1C — скан-код нажатия клавиши Enter
                    forced_reboot = 1;
                    break;
                }
            }
        }

        if (forced_reboot) {
            break; // Выходим из цикла отсчета и летим в перезагрузку
        }

        seconds--;
    }

    // 3. Железно гасим прерывания перед отправкой команд перезагрузки
    asm volatile("cli");

    // Метод 1: Перезагрузка через контроллер клавиатуры 8042
    asm volatile(
        "mov $0xFE, %%al;"
        "out %%al, $0x64;"
        ::: "eax"
    );

    // Метод 2: Fast A20 reset (на случай, если первый проигнорирован)
    asm volatile(
        "mov $0x02, %%al;"
        "out %%al, $0x92;"
        ::: "eax"
    );

    while (1) asm("hlt");
}
