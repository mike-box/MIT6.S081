#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

char* getname(char *path)
{
  char *p;
  // Find first character after last slash.
  for(p=path+strlen(path); p >= path && *p != '/'; p--){}
  // skip '/'
  p++;
  // Return blank-padded name.
  return p;
}

void find(char *path,const char * filename)
{
  char buf[512], *p;
  char *curr;
  char *fname;
  int fd;
  struct dirent de;
  struct stat st;

  // open the dir
  if((fd = open(path, 0)) < 0){
    fprintf(2, "ls: cannot open %s\n", path);
    return;
  }
  // open stat
  if(fstat(fd, &st) < 0){
    fprintf(2, "ls: cannot stat %s\n", path);
    close(fd);
    return;
  }
  // check the current the file is 
  if(st.type != T_DIR){
    fprintf(2, "the current the file is not dictionary\n", path);
    close(fd);
    return;
  }

  //read all the files under the dir
  strcpy(buf, path);
  p = buf + strlen(buf);
  *p++ = '/';
  while(read(fd, &de, sizeof(de)) == sizeof(de)){
      if(de.inum == 0)
          continue;
      // skip "." and ".."
      if(strcmp(de.name,".") == 0) continue;
      if(strcmp(de.name,"..") == 0) continue;

      curr = p;
      memmove(curr, de.name, DIRSIZ);
      curr[DIRSIZ] = 0;
      if(stat(buf, &st) < 0){
          printf("ls: cannot stat %s\n", buf);
          continue;
      }
      // printf("current file name is：%s \n",buf);
      // record the current file
      switch(st.type){
        // we check the current file
        case T_FILE:
            fname = getname(buf);
            if(strcmp(fname,filename) == 0){
                printf("%s\n",buf);
            }
            break;
        // we check the current dir
        case T_DIR:
            if(strlen(path) + 1 + DIRSIZ + 1 > sizeof(buf)){
                printf("ls: path too long\n");
                break;
            }
            find(buf,filename);
            break;
      }
  }

  close(fd);
}

int
main(int argc, char *argv[])
{
  if(argc != 3){
     printf("we need 3 paramters!\n");
     exit(0);
  }
  find(argv[1],argv[2]);
  exit(0);
}
