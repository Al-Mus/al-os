#ifndef FS_H
#define FS_H

#define MAX_NAME_LEN   32
#define MAX_CHILDREN   64
#define MAX_FILE_SIZE  16384
#define MAX_NODES      256

typedef enum { FS_FILE, FS_DIR } fs_type;

typedef struct fs_node {
    char            name[MAX_NAME_LEN];
    fs_type         type;
    struct fs_node* parent;
    struct fs_node* children[MAX_CHILDREN];
    int             child_count;
    char            content[MAX_FILE_SIZE];
} fs_node;


extern fs_node* fs_root;
extern fs_node* fs_current;
extern char current_path[128];

fs_node* resolve_path(const char* path, fs_node* base);
void fs_init(void);
void fs_list(const char* path);
void fs_pwd(void);
int  fs_mkdir(const char* path);
int  fs_cd(const char* path);
int  fs_rm(const char* path);
int  fs_touch(const char* path);
int  fs_write(const char* path, const char* text);
int  fs_cat(const char* path);

fs_node* resolve_path(const char* path, fs_node* base);

#endif