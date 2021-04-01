
#include "types.h"
#include "user.h"

int number_of_processes = 5;

void one(){
  int j;
  for (j = 0; j < number_of_processes; j++)
  {
    int pid = fork();
    if (pid < 0)
    {
      printf(1, "Fork failed\n");
      continue;
    }
    if (pid == 0)
    {
      volatile int i;
      for (volatile int k = 0; k < number_of_processes; k++)
      {
        if (k <= j)
        {
          sleep(100); //io time
        }
        else
        {
          for (i = 0; i < 500000000; i++)
          {
            ; //cpu time
          }
        }
      }
    //   printf(1, "Process: %d Finished\n", j);
      exit();
    }
    else{
      set_priority(100-(20+j),pid); // will only matter for PBS, comment it out if not implemented yet (better priorty for more IO intensive jobs)
    }
  }
  for (j = 0; j < number_of_processes+5; j++)
  {
    wait();
  }
}

void two(){
  // Only 2 processess.
  // one process tries to abuse MLFQ to stay in queue 0
  // other works properly
  // both do a total of num_iter iterations (CPU time)
  // and 2000 ticks of sleep (io time)
  int num_iter = 100000000;  

  int pid = fork();
  if(pid==0){
    int x=1;
      for(int i=1; i<=100; i++){
        sleep(2000/100);
        for(int j=0; j<=num_iter/100; j++){
          x = x*i;
        }
      }
    sleep(2000);
    printf(2, "%d", x);
  }
  else {
    int pid2 = fork();
    if(pid2==0){
      int x=1;
      for(int i=1; i<=2000; i++){
        sleep(1);
        for(int j=0; j<=num_iter/2000; j++){
          x = x*i;
        }
      }
      printf(2, "%d", x);
    }
    else{
      int y=1;
      for(int i=2; i<num_iter*10; i++){
        y = y*i;
      }
      printf(2, "%d", y);

      wait();
      wait();
      wait();
      wait();
    }
  }
}

int main(int argc, char *argv[])
{
  if(strcmp("1", argv[1])==0){
    one();
  }
  else if(strcmp("2", argv[1])==0){
    two();
  }
  exit();
}
