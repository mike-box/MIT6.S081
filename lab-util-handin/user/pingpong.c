#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
    int p1[2];
    int p2[2];
    
    
    if(argc > 1){
       fprintf(2,"Only 1 argument is needed!\n");
       exit(1);
    }

    // creat a pipe
    pipe(p1);
    pipe(p2);
    // child
    if(fork() == 0) {
        char buffer[32];
        //read from the parent
        close(p1[1]);
        close(p2[0]);
        read(p1[0],buffer,4);
        close(p1[0]);
        // write a byte to the parent
        printf("%d: received ping\n",getpid());
        write(p2[1],"pong", 4);
        close(p2[1]);
    } else { 
        char buffer[32];
        // send a byte
        close(p1[0]);
        close(p2[1]);
        write(p1[1],"ping",4);
        close(p1[1]);
        // receive from child
        read(p2[0],buffer,4);
        printf("%d: received pong\n",getpid());
        close(p2[0]);
    }
    exit(0);
}

/*
int main(int argc,char* argv[])
{
   if(argc>1)
   {
      fprintf(2,"Only 1 argument is needed!\n");
      exit(1);
   }	   
   int p1[2];
   int p2[2];
   // char* s="a";
   pipe(p1);
   pipe(p2);
   // write(p[1],s,1);
   int pid;
   if((pid=fork())<0)
   {
      fprintf(2,"fork error\n");
      exit(1);
   }
   else if(pid==0)
   {
      close(p1[1]);
      close(p2[0]);      
      char buf[10];	   
      read(p1[0],buf,4);  
      close(p1[0]);
     // printf("aa:%s\n",buf);
      printf("%d: received %s\n",getpid(),buf);
      write(p2[1],"pong",strlen("pong"));
      close(p2[1]);
   }
   else
   {   
      close(p1[0]);
      close(p2[1]);      
      write(p1[1],"ping",strlen("ping")); 
      close(p1[1]);     	
      // wait(0);
      char buf[10];   
      read(p2[0],buf,4);
      printf("%d: received %s\n",getpid(),buf);
      close(p2[0]);
   }  
   exit(0);
}*/