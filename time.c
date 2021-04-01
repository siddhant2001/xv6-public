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
        int start_ticks = current_ticks();
        waitx(&wtime, &rtime);
        printf(1, "Rtime: %d ticks\n", rtime);
        printf(1, "wtime: %d ticks\n", wtime);
        printf(1, "Total time: %d ticks\n", current_ticks()-start_ticks);
        exit();
    }
}