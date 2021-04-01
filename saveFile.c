#include "types.h"
#include "stat.h"
#include "user.h"
#include "fs.h"
#include "fcntl.h"

#define N 100

struct test {
    char name;
    int number;
};

void
save(void)
{
    int fd;
    // struct test t;
    // t.name = 's';
    // t.number = 1;
    
    fd = open("logs.txt", O_RDWR);
    if(fd >= 0) {
        printf(1, "ok: create backup file succeed\n");
    } else {
        printf(1, "error: create backup file failed\n");
        exit();
    }

    // int size = sizeof(t);
    if(write(fd, "abc", 3) != 3){
        printf(1, "error: write to backup file failed\n");
        exit();
    }
    printf(1, "write ok\n");
    close(fd);
}

int
main(){
    save();
    exit();
}