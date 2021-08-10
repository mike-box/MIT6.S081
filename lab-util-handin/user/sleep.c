#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  int wait = 0;
  if(argc <= 1){
    printf("sleep: need parameter error\n");
    exit(0);
  }
  
   wait = atoi(argv[1]);
   sleep(wait);
   exit(0);
}
