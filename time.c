#include "types.h"
#include "mmu.h"
#include "param.h"
#include "proc.h"
#include "user.h"
#include "fcntl.h"

int main(int argc, char *argv[]) {
    // printf(1, "%s %s %s\n", argv[0], argv[1], argv[2]); 
    int pid = fork();
    if(pid == 0) {
        exec(argv[1], argv + 1);
        exit();
    }
    else{
        int wtime, rtime;
        waitx(&wtime, &rtime);
        printf(1, "Ran for: %d ticks\n", rtime);
        printf(1, "Waited for: %d ticks\n", wtime);
        exit();
    }
}