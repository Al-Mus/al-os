#include "nano.h"
#include "vga.h"
#include "keyboard.h"
#include "fs.h"
#include "ports.h"
#include <string.h>

#define MAX_LINES       512
#define EROW_MAX_LEN    78

struct editor_row {
    char chars[EROW_MAX_LEN + 1];
    int size;
};

struct editor_config {
    int cx, cy;
    int rowoff;
    int screenrows;
    int screencols;
    struct editor_row row[MAX_LINES];
    int numrows;
    char filename[64];
    int dirty;
    char statusmsg[80];
};

static struct editor_config E;

static void editor_set_status(const char* msg) {
    strncpy(E.statusmsg, msg, sizeof(E.statusmsg) - 1);
    E.statusmsg[sizeof(E.statusmsg) - 1] = '\0';
}

static void editor_append_row(const char* s, int len) {
    if (E.numrows >= MAX_LINES) return;
    if (len > EROW_MAX_LEN) len = EROW_MAX_LEN;
    strncpy(E.row[E.numrows].chars, s, len);
    E.row[E.numrows].chars[len] = '\0';
    E.row[E.numrows].size = len;
    E.numrows++;
}

static void editor_load_file(const char* path) {
    fs_node* node = resolve_path(path, fs_current);
    if (!node || node->type != FS_FILE) {
        fs_touch(path);
        node = resolve_path(path, fs_current);
        if (!node) {
            editor_set_status("Failed to create/open file");
            return;
        }
    }

    E.numrows = 0;
    char line[EROW_MAX_LEN + 1];
    int line_len = 0;

    const char* content = node->content;
    for (int i = 0; content[i]; ++i) {
        if (content[i] == '\n') {
            editor_append_row(line, line_len);
            line_len = 0;
        } else {
            if (line_len < EROW_MAX_LEN) {
                line[line_len++] = content[i];
            }
        }
    }
    if (line_len > 0) {
        editor_append_row(line, line_len);
    }

    E.dirty = 0;
}

static void editor_save(void) {
    if (!E.filename[0]) return;

    char buf[MAX_FILE_SIZE];
    buf[0] = '\0';
    int pos = 0;

    for (int i = 0; i < E.numrows; ++i) {
        int len = E.row[i].size;
        if (pos + len + 1 >= MAX_FILE_SIZE) break;
        strncpy(buf + pos, E.row[i].chars, len);
        pos += len;
        if (pos + 1 < MAX_FILE_SIZE) {
            buf[pos++] = '\n';
        }
    }
    buf[pos] = '\0';

    fs_write(E.filename, buf);
    E.dirty = 0;
    editor_set_status("Saved");
}

static void editor_insert_char(char c) {
    if (E.cy == E.numrows) {
        editor_append_row("", 0);
    }

    struct editor_row* row = &E.row[E.cy];
    if (E.cx > row->size) E.cx = row->size;

    if (row->size >= EROW_MAX_LEN) return;

    memmove(&row->chars[E.cx + 1], &row->chars[E.cx], row->size - E.cx + 1);
    row->chars[E.cx] = c;
    row->size++;
    E.cx++;
    E.dirty = 1;
}

static void editor_insert_newline(void) {
    if (E.numrows >= MAX_LINES) return;

    struct editor_row* row = &E.row[E.cy];

    memmove(&E.row[E.cy + 1], &E.row[E.cy], sizeof(struct editor_row) * (E.numrows - E.cy));

    E.row[E.cy + 1].size = row->size - E.cx;
    if (E.row[E.cy + 1].size > 0) {
        strncpy(E.row[E.cy + 1].chars, row->chars + E.cx, E.row[E.cy + 1].size);
    }
    E.row[E.cy + 1].chars[E.row[E.cy + 1].size] = '\0';

    row->size = E.cx;
    row->chars[row->size] = '\0';

    E.cy++;
    E.cx = 0;
    E.numrows++;
    E.dirty = 1;
}

static void editor_del_char(void) {
    if (E.cy >= E.numrows) return;

    struct editor_row* row = &E.row[E.cy];

    if (E.cx > 0) {
        memmove(&row->chars[E.cx - 1], &row->chars[E.cx], row->size - E.cx + 1);
        row->size--;
        E.cx--;
        E.dirty = 1;
    } else if (E.cy > 0) {
        struct editor_row* prev = &E.row[E.cy - 1];
        int prev_len = prev->size;

        if (prev_len + row->size <= EROW_MAX_LEN) {
            strncpy(prev->chars + prev_len, row->chars, row->size);
            prev->size += row->size;
            prev->chars[prev->size] = '\0';

            memmove(&E.row[E.cy], &E.row[E.cy + 1], sizeof(struct editor_row) * (E.numrows - E.cy - 1));
            E.numrows--;
            E.cy--;
            E.cx = prev_len;
            E.dirty = 1;
        }
    }
}

static void editor_move_cursor(int key) {
    struct editor_row* row = (E.cy < E.numrows) ? &E.row[E.cy] : NULL;

    switch (key) {
        case -1: // left
            if (E.cx > 0) {
                E.cx--;
            } else if (E.cy > 0) {
                E.cy--;
                E.cx = E.row[E.cy].size;
            }
            break;

        case -3: // right
            if (row && E.cx < row->size) {
                E.cx++;
            } else if (row) {
                E.cy++;
                E.cx = 0;
            }
            break;

        case -2: // up
            if (E.cy > 0) E.cy--;
            break;

        case -4: // down
            if (E.cy < E.numrows) E.cy++;
            break;
    }

    row = (E.cy < E.numrows) ? &E.row[E.cy] : NULL;
    int rowlen = row ? row->size : 0;
    if (E.cx > rowlen) E.cx = rowlen;
}

static void editor_scroll(void) {
    if (E.cy < E.rowoff) {
        E.rowoff = E.cy;
    }
    if (E.cy >= E.rowoff + E.screenrows) {
        E.rowoff = E.cy - E.screenrows + 1;
    }
}

static void editor_refresh_screen(void)
{
    editor_scroll();
    vga_clear();

    for (int y = 0; y < E.screenrows; y++)
    {
        int filerow = y + E.rowoff;
        uint16_t pos = y * VGA_WIDTH;

        if (filerow >= E.numrows)
        {
            vga_put_at('~', 0x08, pos);
        }
        else
        {
            int len = E.row[filerow].size;
            if (len > E.screencols) len = E.screencols;
            for (int j = 0; j < len; j++)
            {
                vga_put_at(E.row[filerow].chars[j], 0x07, pos + j);
            }
        }
    }

    uint16_t status_pos = E.screenrows * VGA_WIDTH;
    char status[80] = {0};
    char rstatus[80] = {0};

    const char* fname = E.filename[0] ? E.filename : "[No Name]";
    int fname_len = strlen(fname);
    if (fname_len > VGA_WIDTH / 2) fname_len = VGA_WIDTH / 2;

    strncpy(status, fname, fname_len);

    if (E.dirty)
        strcat(status, " (modified)");

    char numbuf[16];

    itoa(E.cy + 1, numbuf, 10);
    strcpy(rstatus, numbuf);
    strcat(rstatus, "/");
    itoa(E.numrows, numbuf, 10);
    strcat(rstatus, numbuf);

    size_t rlen = strlen(rstatus);

    for (int j = 0; j < VGA_WIDTH; j++)
        vga_put_at(' ', 0x70, status_pos + j);

    for (size_t j = 0; j < strlen(status); j++)
        vga_put_at(status[j], 0x70, status_pos + j);

    for (size_t j = 0; j < rlen; j++)
        vga_put_at(rstatus[j], 0x70, status_pos + VGA_WIDTH - rlen + j);

    if (E.statusmsg[0])
    {
        size_t msglen = strlen(E.statusmsg);
        if (msglen > VGA_WIDTH - 2) msglen = VGA_WIDTH - 2;
        for (size_t j = 0; j < msglen; j++)
            vga_put_at(E.statusmsg[j], 0x70, status_pos + j);
        E.statusmsg[0] = '\0';
    }

    int screeny = E.cy - E.rowoff;
    if (screeny >= 0 && screeny < E.screenrows)
        vga_set_cursor(screeny * VGA_WIDTH + E.cx);
}

void nano_edit(const char* filename) {
    memset(&E, 0, sizeof(E));

    E.screenrows = VGA_HEIGHT - 1;
    E.screencols = VGA_WIDTH;

    if (filename && filename[0]) {
        strncpy(E.filename, filename, sizeof(E.filename) - 1);
        E.filename[sizeof(E.filename) - 1] = '\0';
    }

    editor_load_file(filename ? filename : "");

    editor_set_status("Ctrl+S = Save | Ctrl+Q = Quit | Ctrl+C = Force Quit");

    while (1) {
        editor_refresh_screen();

        char c = keyboard_read_char();
        if (c == 0) continue;

        if (c == '\x03') break;

        if (c == 17) {  // Ctrl+Q
            if (E.dirty) {
                editor_save();
            }
            break;
        }

        if (c == 19) {  // Ctrl+S
            editor_save();
            continue;
        }

        switch (c) {
            case '\b':              editor_del_char(); break;
            case '\n':              editor_insert_newline(); break;
            case -5:                // Delete
                editor_move_cursor(-3);
                editor_del_char();
                break;

            case -1: case -3: case -2: case -4: // ← → ↑ ↓
                editor_move_cursor(c);
                break;

            default:
                if (c >= 32 && c <= 126) {
                    editor_insert_char(c);
                }
                break;
        }
    }

    vga_clear();
}