#include "types.h"
#include "mmu.h"
#include "param.h"
#include "proc.h"
#include "user.h"
#include "fcntl.h"

int main(int argc, char *argv[]) {
    int new_priority, pid; 
    
    new_priority = atoi(argv[1]);
    pid = atoi(argv[2]);

    printf(1, "%d %d\n", pid, new_priority);

    int ret = set_priority(new_priority, pid);
    printf(1, "%d\n", ret);
    exit();
}