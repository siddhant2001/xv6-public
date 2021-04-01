#include "types.h"
#include "user.h"

int
main(int argc, char *argv[])
{
  if(argc > 1){
    printf(1, "ps: too many arguments\n");
    exit();
  }

  ps();

  exit();
}