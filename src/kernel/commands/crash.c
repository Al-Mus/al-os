#include "all_commands.h"
#include "../sys/crash.h"

void cmd_crash(void) {
    crash_system();
}
