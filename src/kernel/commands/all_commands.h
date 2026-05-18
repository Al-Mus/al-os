#ifndef ALL_COMMANDS_H
#define ALL_COMMANDS_H

#include "../fs/memory_fs/fs.h"

void cmd_echo(const char* args);
void cmd_cp(const char* args);
void cmd_mv(const char* args);
void cmd_help(const char* args);
void cmd_tree(fs_node* node, int depth);
void cmd_calc(const char* expr);
void cmd_sysinfo();
void cmd_slowfetch();
void cmd_whoami();
void cmd_date(const char* args);
void cmd_colorbar(void);
void cmd_memtest();
void cmd_aarch();
void time_cmd();
void uptime_cmd();
void meminfo_cmd();
void cmd_history();
void cmd_disks();
void cmd_fatwrite();
void cmd_crash();
void cmd_mkrootfs(const char* args);
void pci_scan_bus();
void net_test();
void ping_cmd(char* args);

#endif
