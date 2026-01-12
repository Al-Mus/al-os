#include "screensaver.h"
#include "../drivers/vga.h"
#include "../drivers/keyboard.h"
#include "string.h"
#include "ports.h"

/* ============== Константы ============== */
#define SCR_WIDTH  80
#define SCR_HEIGHT 25

/* ============== Случайные числа ============== */
static unsigned int rand_seed = 12345;

static unsigned int rand(void) {
    rand_seed = rand_seed * 1103515245 + 12345;
    return (rand_seed >> 16) & 0x7FFF;
}

static int rand_range(int min, int max) {
    return min + (rand() % (max - min + 1));
}

static void rand_init(void) {
    outb(0x43, 0x00);
    unsigned char lo = inb(0x40);
    unsigned char hi = inb(0x40);
    rand_seed = (hi << 8) | lo;
    rand_seed ^= rand_seed << 13;
    if (rand_seed == 0) rand_seed = 12345;
}

/* ============== VGA функции ============== */
static void scr_put(int x, int y, char c, uint8_t color) {
    if (x < 0 || x >= SCR_WIDTH || y < 0 || y >= SCR_HEIGHT) return;
    vga_put_at(c, color, y * SCR_WIDTH + x);
}

static void scr_clear(uint8_t color) {
    for (int y = 0; y < SCR_HEIGHT; y++) {
        for (int x = 0; x < SCR_WIDTH; x++) {
            scr_put(x, y, ' ', color);
        }
    }
}

/* ============== КЛАВИАТУРА (исправленная) ============== */

// Проверка есть ли данные в буфере клавиатуры
static int kbd_has_data(void) {
    return inb(0x64) & 1;
}

// Читаем и выбрасываем все данные из буфера
static void kbd_flush(void) {
    while (kbd_has_data()) {
        inb(0x60);
        for (volatile int i = 0; i < 500; i++);
    }
}

// Проверка нажатия БЕЗ блокировки - возвращает 1 если была нажата клавиша
static int kbd_check_any_key(void) {
    if (kbd_has_data()) {
        uint8_t sc = inb(0x60);
        // Только нажатия (не отпускания)
        if (!(sc & 0x80)) {
            kbd_flush(); // Очищаем остальное
            return 1;
        }
    }
    return 0;
}

// Ждём нажатия клавиши (блокирующее) - возвращает scancode
static uint8_t kbd_wait_key(void) {
    uint8_t sc;
    
    // Сначала очищаем буфер
    kbd_flush();
    
    // Ждём нажатия (не отпускания)
    while (1) {
        while (!kbd_has_data());
        sc = inb(0x60);
        if (!(sc & 0x80)) {
            // Ждём отпускания этой клавиши
            while (1) {
                if (kbd_has_data()) {
                    uint8_t rel = inb(0x60);
                    if (rel == (sc | 0x80)) break;
                }
            }
            kbd_flush();
            return sc;
        }
    }
}

// Задержка с проверкой клавиши
static int delay_or_key(unsigned int ms) {
    for (unsigned int i = 0; i < ms; i++) {
        for (volatile int j = 0; j < 2000; j++);
        if (kbd_check_any_key()) {
            return 1; // Была нажата клавиша
        }
    }
    return 0; // Таймаут
}

/* ============== MATRIX ============== */

#define MATRIX_CHARS "abcdefghijklmnopqrstuvwxyz0123456789@#$%^&*"

typedef struct {
    int y;
    int length;
    int speed;
    int timer;
    int active;
} matrix_column;

void screensaver_matrix(void) {
    rand_init();
    vga_clear();
    vga_set_cursor(SCR_WIDTH * SCR_HEIGHT);
    
    matrix_column cols[SCR_WIDTH];
    char screen[SCR_HEIGHT][SCR_WIDTH];
    uint8_t brightness[SCR_HEIGHT][SCR_WIDTH];
    
    for (int x = 0; x < SCR_WIDTH; x++) {
        cols[x].active = 0;
        cols[x].y = -rand_range(5, 25);
        cols[x].length = rand_range(5, 20);
        cols[x].speed = rand_range(1, 4);
        cols[x].timer = 0;
    }
    
    for (int y = 0; y < SCR_HEIGHT; y++) {
        for (int x = 0; x < SCR_WIDTH; x++) {
            screen[y][x] = ' ';
            brightness[y][x] = 0;
        }
    }
    
    const char* chars = MATRIX_CHARS;
    int chars_len = strlen(chars);
    
    kbd_flush();
    
    while (1) {
        // Обновляем колонки
        for (int x = 0; x < SCR_WIDTH; x++) {
            cols[x].timer++;
            
            if (cols[x].timer >= cols[x].speed) {
                cols[x].timer = 0;
                
                if (!cols[x].active && rand() % 50 == 0) {
                    cols[x].active = 1;
                    cols[x].y = -rand_range(1, 10);
                    cols[x].length = rand_range(8, 20);
                }
                
                if (cols[x].active) {
                    cols[x].y++;
                    
                    if (cols[x].y >= 0 && cols[x].y < SCR_HEIGHT) {
                        screen[cols[x].y][x] = chars[rand() % chars_len];
                        brightness[cols[x].y][x] = 15;
                    }
                    
                    for (int i = 1; i <= cols[x].length + 10; i++) {
                        int ty = cols[x].y - i;
                        if (ty >= 0 && ty < SCR_HEIGHT) {
                            if (brightness[ty][x] > 0) brightness[ty][x]--;
                            if (rand() % 20 == 0) {
                                screen[ty][x] = chars[rand() % chars_len];
                            }
                        }
                    }
                    
                    if (cols[x].y - cols[x].length > SCR_HEIGHT + 5) {
                        cols[x].active = 0;
                        cols[x].y = -rand_range(5, 20);
                    }
                }
            }
        }
        
        // Отрисовка
        for (int y = 0; y < SCR_HEIGHT; y++) {
            for (int x = 0; x < SCR_WIDTH; x++) {
                uint8_t b = brightness[y][x];
                uint8_t color;
                
                if (b == 0) color = 0x00;
                else if (b >= 12) color = 0x0F;
                else if (b >= 8) color = 0x0A;
                else color = 0x02;
                
                scr_put(x, y, b > 0 ? screen[y][x] : ' ', color);
            }
        }
        
        if (delay_or_key(30)) break;
    }
}

/* ============== STARS ============== */

#define MAX_STARS 100

typedef struct {
    int x, y;
    int speed;
    char symbol;
    uint8_t color;
} star;

void screensaver_stars(void) {
    rand_init();
    scr_clear(0x00);
    vga_set_cursor(SCR_WIDTH * SCR_HEIGHT);
    
    star stars[MAX_STARS];
    
    for (int i = 0; i < MAX_STARS; i++) {
        stars[i].x = rand_range(0, SCR_WIDTH - 1);
        stars[i].y = rand_range(0, SCR_HEIGHT - 1);
        stars[i].speed = rand_range(1, 4);
        
        switch (stars[i].speed) {
            case 1: stars[i].symbol = '.'; stars[i].color = 0x08; break;
            case 2: stars[i].symbol = '.'; stars[i].color = 0x07; break;
            case 3: stars[i].symbol = '*'; stars[i].color = 0x0F; break;
            case 4: stars[i].symbol = '*'; stars[i].color = 0x0F; break;
        }
    }
    
    // Твоё лого
    const char* logo[] = {
        "     _    _         ___  ____  ",
        "    / \\  | |       / _ \\/ ___| ",
        "   / _ \\ | |      | | | \\___ \\ ",
        "  / ___ \\| |___   | |_| |___) |",
        " /_/   \\_\\_____|   \\___/|____/ "
    };
    int logo_h = 5;
    int logo_w = 32;
    int logo_x = (SCR_WIDTH - logo_w) / 2;
    int logo_y = (SCR_HEIGHT - logo_h) / 2;
    
    kbd_flush();
    
    while (1) {
        scr_clear(0x00);
        
        for (int i = 0; i < MAX_STARS; i++) {
            stars[i].x -= stars[i].speed;
            if (stars[i].x < 0) {
                stars[i].x = SCR_WIDTH - 1;
                stars[i].y = rand_range(0, SCR_HEIGHT - 1);
            }
            scr_put(stars[i].x, stars[i].y, stars[i].symbol, stars[i].color);
        }
        
        // Рисуем лого
        for (int row = 0; row < logo_h; row++) {
            for (int col = 0; logo[row][col]; col++) {
                if (logo[row][col] != ' ') {
                    scr_put(logo_x + col, logo_y + row, logo[row][col], 0x0B);
                }
            }
        }
        
        if (delay_or_key(50)) break;
    }
}

/* ============== BOUNCE ============== */

void screensaver_bounce(void) {
    rand_init();
    scr_clear(0x00);
    vga_set_cursor(SCR_WIDTH * SCR_HEIGHT);
    
    // Твоё лого AL-OS
    const char* logo[] = {
        "     _    _         ___  ____  ",
        "    / \\  | |       / _ \\/ ___| ",
        "   / _ \\ | |      | | | \\___ \\ ",
        "  / ___ \\| |___   | |_| |___) |",
        " /_/   \\_\\_____|   \\___/|____/ "
    };
    int logo_h = 5;
    int logo_w = 32;
    
    int x = rand_range(0, SCR_WIDTH - logo_w);
    int y = rand_range(0, SCR_HEIGHT - logo_h);
    int dx = (rand() % 2) ? 1 : -1;
    int dy = (rand() % 2) ? 1 : -1;
    
    uint8_t colors[] = {0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F};
    int num_colors = 7;
    int color_idx = 0;
    
    kbd_flush();
    
    while (1) {
        scr_clear(0x00);
        
        for (int row = 0; row < logo_h; row++) {
            for (int col = 0; logo[row][col]; col++) {
                if (logo[row][col] != ' ') {
                    scr_put(x + col, y + row, logo[row][col], colors[color_idx]);
                }
            }
        }
        
        x += dx;
        y += dy;
        
        int bounced = 0;
        
        if (x <= 0) { x = 0; dx = 1; bounced = 1; }
        if (x >= SCR_WIDTH - logo_w) { x = SCR_WIDTH - logo_w; dx = -1; bounced = 1; }
        if (y <= 0) { y = 0; dy = 1; bounced = 1; }
        if (y >= SCR_HEIGHT - logo_h) { y = SCR_HEIGHT - logo_h; dy = -1; bounced = 1; }
        
        if (bounced) {
            color_idx = (color_idx + 1) % num_colors;
        }
        
        if (delay_or_key(70)) break;
    }
}

/* ============== PIPES ============== */

void screensaver_pipes(void) {
    rand_init();
    scr_clear(0x00);
    vga_set_cursor(SCR_WIDTH * SCR_HEIGHT);
    
    int x = rand_range(10, SCR_WIDTH - 10);
    int y = rand_range(5, SCR_HEIGHT - 5);
    int dir = rand_range(0, 3);
    
    uint8_t colors[] = {0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E};
    int num_colors = 6;
    int color_idx = 0;
    
    int steps = 0;
    int max_steps = rand_range(10, 50);
    
    kbd_flush();
    
    while (1) {
        char pipe_char = (dir == 0 || dir == 2) ? '|' : '-';
        scr_put(x, y, pipe_char, colors[color_idx]);
        
        switch (dir) {
            case 0: y--; break;
            case 1: x++; break;
            case 2: y++; break;
            case 3: x--; break;
        }
        
        steps++;
        
        int must_turn = (x <= 1 || x >= SCR_WIDTH - 2 || y <= 1 || y >= SCR_HEIGHT - 2);
        
        if (steps >= max_steps || must_turn) {
            int new_dir;
            
            if (must_turn) {
                if (x <= 1) new_dir = 1;
                else if (x >= SCR_WIDTH - 2) new_dir = 3;
                else if (y <= 1) new_dir = 2;
                else new_dir = 0;
            } else {
                new_dir = (rand() % 2) ? (dir + 1) % 4 : (dir + 3) % 4;
            }
            
            scr_put(x, y, '+', colors[color_idx]);
            dir = new_dir;
            steps = 0;
            max_steps = rand_range(5, 30);
            
            if (rand() % 3 == 0) {
                color_idx = (color_idx + 1) % num_colors;
            }
        }
        
        if (rand() % 300 == 0) {
            x = rand_range(10, SCR_WIDTH - 10);
            y = rand_range(5, SCR_HEIGHT - 5);
            color_idx = (color_idx + 1) % num_colors;
        }
        
        if (delay_or_key(25)) break;
    }
}

/* ============== FIRE ============== */

void screensaver_fire(void) {
    rand_init();
    vga_set_cursor(SCR_WIDTH * SCR_HEIGHT);
    
    static unsigned char fire[SCR_HEIGHT + 1][SCR_WIDTH];
    
    const char fire_chars[] = " .:-=+*#%@";
    int num_chars = 10;
    
    uint8_t fire_colors[] = {0x00, 0x04, 0x04, 0x0C, 0x06, 0x0E, 0x0E, 0x0F};
    int num_fire_colors = 8;
    
    for (int y = 0; y <= SCR_HEIGHT; y++) {
        for (int x = 0; x < SCR_WIDTH; x++) {
            fire[y][x] = 0;
        }
    }
    
    kbd_flush();
    
    while (1) {
        for (int x = 0; x < SCR_WIDTH; x++) {
            fire[SCR_HEIGHT][x] = rand() % 2 ? 70 : 0;
        }
        
        for (int y = 0; y < SCR_HEIGHT; y++) {
            for (int x = 0; x < SCR_WIDTH; x++) {
                int sum = 0;
                int count = 0;
                
                for (int dx = -1; dx <= 1; dx++) {
                    int nx = x + dx;
                    if (nx >= 0 && nx < SCR_WIDTH) {
                        sum += fire[y + 1][nx];
                        count++;
                    }
                }
                
                int avg = sum / count;
                fire[y][x] = (avg > 3) ? avg - rand() % 4 : 0;
            }
        }
        
        for (int y = 0; y < SCR_HEIGHT; y++) {
            for (int x = 0; x < SCR_WIDTH; x++) {
                int intensity = fire[y][x];
                
                int char_idx = (intensity * num_chars) / 80;
                if (char_idx >= num_chars) char_idx = num_chars - 1;
                if (char_idx < 0) char_idx = 0;
                
                int color_idx = (intensity * num_fire_colors) / 80;
                if (color_idx >= num_fire_colors) color_idx = num_fire_colors - 1;
                if (color_idx < 0) color_idx = 0;
                
                scr_put(x, y, fire_chars[char_idx], fire_colors[color_idx]);
            }
        }
        
        if (delay_or_key(35)) break;
    }
}

/* ============== PLASMA ============== */

void screensaver_plasma(void) {
    rand_init();
    vga_set_cursor(SCR_WIDTH * SCR_HEIGHT);
    
    int frame = 0;
    const char plasma_chars[] = " .-:=+*#%@";
    int num_chars = 10;
    
    uint8_t plasma_colors[] = {0x01, 0x09, 0x03, 0x0B, 0x02, 0x0A, 0x06, 0x0E};
    int num_colors = 8;
    
    kbd_flush();
    
    while (1) {
        for (int y = 0; y < SCR_HEIGHT; y++) {
            for (int x = 0; x < SCR_WIDTH; x++) {
                // Простая плазма через сумму синусоид (аппроксимация)
                int v1 = (x + frame) % 20;
                int v2 = (y + frame / 2) % 15;
                int v3 = (x + y + frame) % 25;
                int v4 = ((x - y) + frame * 2) % 30;
                
                int value = (v1 + v2 + v3 + v4) % (num_chars * 2);
                if (value >= num_chars) value = num_chars * 2 - 1 - value;
                
                int color_val = (v1 + v3 + frame / 3) % (num_colors * 2);
                if (color_val >= num_colors) color_val = num_colors * 2 - 1 - color_val;
                
                scr_put(x, y, plasma_chars[value], plasma_colors[color_val]);
            }
        }
        
        frame++;
        
        if (delay_or_key(40)) break;
    }
}

/* ============== ГЛАВНОЕ МЕНЮ ============== */

void screensaver_run(void) {
    int mode = 0;
    int num_modes = 6;
    const char* mode_names[] = {
        "Matrix    - Falling green code",
        "Starfield - Flying through space",
        "Bounce    - Bouncing logo",
        "Pipes     - Colorful pipes",
        "Fire      - Burning flames",
        "Plasma    - Psychedelic waves"
    };
    
    kbd_flush();
    
    while (1) {
        vga_clear();
        
        vga_print_color("\n", 0x00);
        vga_print_color("      _    _         ___  ____  \n", 0x0B);
        vga_print_color("     / \\  | |       / _ \\/ ___| \n", 0x0B);
        vga_print_color("    / _ \\ | |      | | | \\___ \\ \n", 0x0B);
        vga_print_color("   / ___ \\| |___   | |_| |___) |\n", 0x0B);
        vga_print_color("  /_/   \\_\\_____|   \\___/|____/ \n", 0x0B);
        vga_print_color("\n", 0x00);
        vga_print_color("         === SCREENSAVER ===\n\n", 0x0E);
        
        for (int i = 0; i < num_modes; i++) {
            if (i == mode) {
                vga_print_color("    > ", 0x0A);
                vga_print_color(mode_names[i], 0x0F);
                vga_print_color("\n", 0x0A);
            } else {
                vga_print_color("      ", 0x07);
                vga_print_color(mode_names[i], 0x08);
                vga_print_color("\n", 0x07);
            }
        }
        
        vga_print_color("\n    [Up/Down] Select   [Enter] Start   [ESC] Exit\n", 0x07);
        vga_print_color("\n    Press any key to stop screensaver\n", 0x08);
        
        // Ждём нажатия клавиши
        uint8_t sc = kbd_wait_key();
        
        switch (sc) {
            case 0x48: // Up
                mode = (mode - 1 + num_modes) % num_modes;
                break;
            case 0x50: // Down
                mode = (mode + 1) % num_modes;
                break;
            case 0x1C: // Enter
                kbd_flush();
                switch (mode) {
                    case 0: screensaver_matrix(); break;
                    case 1: screensaver_stars(); break;
                    case 2: screensaver_bounce(); break;
                    case 3: screensaver_pipes(); break;
                    case 4: screensaver_fire(); break;
                    case 5: screensaver_plasma(); break;
                }
                kbd_flush();
                break;
            case 0x01: // ESC
                vga_clear();
                vga_print_color("Screensaver closed.\n", 0x0A);
                return;
        }
    }
}