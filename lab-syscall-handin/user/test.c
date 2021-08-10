// Shell.

#include "kernel/types.h"
#include "user/user.h"
#include "kernel/fcntl.h"

int main(int argc,char *argv[]){
    char buf[64];
    int nbuf;
    if(argc < 2){
        fprintf(2, "we need 2 parameter");
        exit(1);
    }

    if(fork() == 0){
        close(0);
        int fd = open(argv[1], O_RDWR);
        nbuf = 32;
        while(gets(buf,nbuf)){
            fprintf(2,buf);
        }
    }
    wait(0);
    return 0;
}