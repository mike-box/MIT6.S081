#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"


int process(const int * p){
    int prime = 1;
    int cnt = 0;
    int num = 0;
    int pip[2];
    int status;

    /*close p1*/
    pipe(pip);
    while(read(p[0],&num,4)){
        if(num == 0) break;
        if(prime == 1){
            prime = num;
            printf("prime %d\n",prime);
        }else{
            if(num%prime == 0) continue;
            write(pip[1],&num,4);
            cnt++;
        }
    }
    close(p[0]);
    close(pip[1]);
    if(prime > 1){
        if(fork() == 0){
            process(pip);
            exit(1);
        }
    }
    wait(&status);
    close(pip[0]);
    return 0;
}

int
main(int argc, char *argv[])
{
    int p[2];

    pipe(p);
    for(int i = 2; i <= 35; ++i){
        write(p[1],&i,4);
    }
    close(p[1]);
    process(p);
    close(p[0]);
    exit(0);
}
