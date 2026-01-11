#include "fs.h"
#include "../utils/string.h"
#include "../drivers/vga.h"
#include "../utils/panic.h"

static fs_node fs_nodes[MAX_NODES];
static int node_count = 0;

fs_node* fs_root = NULL;
fs_node* fs_current = NULL;
char current_path[128] = "";

static fs_node* create_node(const char* name, fs_type type, fs_node* parent) {
    if (node_count >= MAX_NODES) {
        panic("Filesystem", "too many nodes (node pool exhausted)", __func__);
    }

    fs_node* n = &fs_nodes[node_count++];
    
    int i = 0;
    while (name[i] && i < MAX_NAME_LEN - 1) {
        n->name[i] = name[i];
        i++;
    }
    n->name[i] = '\0';

    n->type = type;
    n->parent = parent;
    n->child_count = 0;
    for (i = 0; i < MAX_CHILDREN; i++) n->children[i] = NULL;
    n->content[0] = '\0';
    return n;
}

static fs_node* find_child(fs_node* dir, const char* name) {
    for (int i = 0; i < dir->child_count; i++)
        if (strcmp(dir->children[i]->name, name) == 0)
            return dir->children[i];
    return NULL;
}

static int split_path(const char* path, char segs[16][MAX_NAME_LEN]) {
    int seg = 0, i = 0, j = 0;
    if (!path || !path[0]) return 0;

    while (path[i] && seg < 16) {
        if (path[i] == '/') {
            if (j > 0) {
                segs[seg][j] = '\0';
                seg++;
                j = 0;
            }
        } else {
            if (j < MAX_NAME_LEN - 1) segs[seg][j++] = path[i];
        }
        i++;
    }
    if (j > 0) {
        segs[seg][j] = '\0';
        seg++;
    }
    return seg;
}

fs_node* resolve_path(const char* path, fs_node* base) {
    if (!path || !path[0]) return base;
    fs_node* cur = (path[0] == '/') ? fs_root : base;
    char segs[16][MAX_NAME_LEN];
    int n = split_path(path, segs);

    for (int i = 0; i < n; i++) {
        if (strcmp(segs[i], ".") == 0) continue;
        if (strcmp(segs[i], "..") == 0) {
            if (cur->parent) cur = cur->parent;
            continue;
        }
        fs_node* child = find_child(cur, segs[i]);
        if (!child) return NULL;
        cur = child;
    }
    return cur;
}

static void update_current_path(void) {
    char temp[128];
    temp[0] = '\0';
    fs_node* cur = fs_current;

    while (cur && cur != fs_root) {
        char part[MAX_NAME_LEN + 2];
        strcpy(part, "/");
        strcat(part, cur->name);
        strcat(part, temp);
        strcpy(temp, part);
        cur = cur->parent;
    }
    if (temp[0] == '\0') strcpy(temp, "/");
    strcpy(current_path, temp);
}

static void rtrim(char* str) {
    if (!str || !*str) return;
    char* end = str + strlen(str) - 1;
    while (end >= str && (*end == ' ' || *end == '\t')) {
        *end = '\0';
        end--;
    }
}

/* ======================= Public Functions ======================= */

void fs_init(void) {
    node_count = 0;
    fs_root = create_node("", FS_DIR, NULL);
    strcpy(fs_root->name, "/");
    fs_current = fs_root;
    update_current_path();

    fs_mkdir("bin");
    fs_mkdir("dev");
    fs_mkdir("home");
    fs_mkdir("mnt");
}

void fs_list(const char* path) {
    fs_node* dir = path ? resolve_path(path, fs_current) : fs_current;
    if (!dir || dir->type != FS_DIR) {
        vga_print_color("Not a directory\n", 0x0C);
        return;
    }

    fs_node* dirs[MAX_CHILDREN];
    fs_node* files[MAX_CHILDREN];
    int dcount = 0, fcount = 0;
    for (int i = 0; i < dir->child_count; i++) {
        if (dir->children[i]->type == FS_DIR) dirs[dcount++] = dir->children[i];
        else files[fcount++] = dir->children[i];
    }

    for (int i = 0; i < dcount - 1; i++) {
        int min = i;
        for (int j = i + 1; j < dcount; j++) if (strcmp(dirs[j]->name, dirs[min]->name) < 0) min = j;
        if (min != i) { fs_node* t = dirs[i]; dirs[i] = dirs[min]; dirs[min] = t; }
    }
    for (int i = 0; i < fcount - 1; i++) {
        int min = i;
        for (int j = i + 1; j < fcount; j++) if (strcmp(files[j]->name, files[min]->name) < 0) min = j;
        if (min != i) { fs_node* t = files[i]; files[i] = files[min]; files[min] = t; }
    }

    int maxlen = 4;
    for (int i = 0; i < dcount; i++) { int l = 0; while (dirs[i]->name[l]) l++; if (l + 1 > maxlen) maxlen = l + 1; }
    for (int i = 0; i < fcount; i++) { int l = 0; while (files[i]->name[l]) l++; if (l > maxlen) maxlen = l; }

    int colw = maxlen + 2;
    int linew = 0;
    int screenw = 80;

    for (int i = 0; i < dcount; i++) {
        char buf[64]; int p = 0;
        int j = 0; while (dirs[i]->name[j] && j < (int)sizeof(buf)-2) buf[p++] = dirs[i]->name[j++];
        buf[p++] = '/';
        while (p < colw && p < (int)sizeof(buf)-1) buf[p++] = ' ';
        buf[p] = '\0';
        if (linew + colw > screenw) { vga_putc('\n'); linew = 0; }
        vga_print_color(buf, 0x09);
        linew += colw;
    }

    for (int i = 0; i < fcount; i++) {
        char buf[64]; int p = 0;
        int j = 0; while (files[i]->name[j] && j < (int)sizeof(buf)-1) buf[p++] = files[i]->name[j++];
        while (p < colw && p < (int)sizeof(buf)-1) buf[p++] = ' ';
        buf[p] = '\0';
        if (linew + colw > screenw) { vga_putc('\n'); linew = 0; }
        vga_print_color(buf, 0x0F);
        linew += colw;
    }

    if (linew) vga_putc('\n');
}

void fs_pwd(void) {
    vga_print_color(current_path, 0x0F);
    vga_putc('\n');
}

int fs_mkdir(const char* path) {
    if (!path || !path[0]) return -1;

    char segs[16][MAX_NAME_LEN];
    int n = split_path(path, segs);

    for (int i = 0; i < n; i++) {
        rtrim(segs[i]);
    }

    if (n == 0 || segs[n-1][0] == '\0') {
        vga_print_color("Invalid directory name\n", 0x0C);
        return -1;
    }

    fs_node* parent = (path[0] == '/') ? fs_root : fs_current;

    for (int i = 0; i < n - 1; i++) {
        fs_node* child = find_child(parent, segs[i]);
        if (!child || child->type != FS_DIR) return -1;
        parent = child;
    }

    const char* last = segs[n-1];

    if (find_child(parent, last)) return -1;

    if (parent->child_count >= MAX_CHILDREN) {
        vga_print_color("Directory full\n", 0x0C);
        return -1;
    }

    fs_node* d = create_node(last, FS_DIR, parent);
    if (!d) panic("Filesystem", "failed to create directory node", __func__);

    parent->children[parent->child_count++] = d;
    return 0;
}

int fs_cd(const char* path) {
    if (!path || !path[0]) return -1;
    fs_node* node = resolve_path(path, fs_current);
    if (!node || node->type != FS_DIR) {
        vga_print_color("No such directory\n", 0x0C);
        return -1;
    }
    fs_current = node;
    update_current_path();
    return 0;
}

int fs_rm(const char* path) {
    if (!path || !path[0]) return -1;
    fs_node* node = resolve_path(path, fs_current);
    if (!node || node == fs_root) return -1;
    fs_node* parent = node->parent;
    int idx = -1;
    for (int i = 0; i < parent->child_count; i++)
        if (parent->children[i] == node) { idx = i; break; }
    if (idx == -1) return -1;
    for (int i = idx; i < parent->child_count - 1; i++)
        parent->children[i] = parent->children[i + 1];
    parent->child_count--;
    return 0;
}

int fs_touch(const char* path) {
    if (!path || !path[0]) return -1;
    char segs[16][MAX_NAME_LEN];
    int n = split_path(path, segs);
    fs_node* parent = (path[0] == '/') ? fs_root : fs_current;

    for (int i = 0; i < n - 1; i++) {
        fs_node* child = find_child(parent, segs[i]);
        if (!child || child->type != FS_DIR) return -1;
        parent = child;
    }

    if (find_child(parent, segs[n-1])) return -1;
    if (parent->child_count >= MAX_CHILDREN) {
        vga_print_color("Directory full\n", 0x0C);
        return -1;
    }

    fs_node* f = create_node(segs[n-1], FS_FILE, parent);
    if (!f) {
        panic("Filesystem", "failed to create file node", __func__);
    }

    parent->children[parent->child_count++] = f;
    return 0;
}

int fs_write(const char* path, const char* text) {
    if (!path || !path[0]) return -1;
    fs_node* node = resolve_path(path, fs_current);
    if (!node || node->type != FS_FILE) return -1;
    int i = 0;
    while (text[i] && i < MAX_FILE_SIZE - 1) {
        node->content[i] = text[i];
        i++;
    }
    node->content[i] = '\0';
    return 0;
}

int fs_cat(const char* path) {
    if (!path || !path[0]) return -1;
    fs_node* node = resolve_path(path, fs_current);
    if (!node || node->type != FS_FILE) {
        vga_print_color("Not a file\n", 0x0C);
        return -1;
    }
    if (node->content[0]) vga_print_color(node->content, 0x0F);
    else vga_print_color("(empty)", 0x08);
    vga_putc('\n');
    return 0;
}