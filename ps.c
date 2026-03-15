#include "types.h"
#include "user.h"
#include "uproc.h"

int main(int argc, char *argv[]) {
    struct uproc up;
    for(int pid = 1; pid < 64; pid++) {
        if(getprocinfo(pid, &up) == 0)
            printf(1, "PID:%d Name:%s PPID:%d Size:%d\n",
                up.pid, up.name, up.ppid, up.sz);
    }
    exit();
}
