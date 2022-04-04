#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>

char * my_getenv(char * envp[]){
  const char * ch="PATH";
  int len=strlen(ch);
  for(char ** temp=envp;*temp;temp++){
    char * now=*temp;
    if(!strncmp(now,ch,len))
      if(*(now+len)=='=') return now+len+1;
  }
  return NULL;
}

void copy(char * dest,char * src){
  for(;*src&&*src!=':';++src,++dest) *dest=*src;
  *dest=0;
  return;
}

void my_execvp(char * filename,char * argv[],char * envp[]){
  char * path=my_getenv(envp);
  if(!path||strchr(filename,'/')) {
    execve(filename,argv,envp);
    assert(0);
  }
  char buf[128];
  while (*path){
    copy(buf,path);
    if(buf[strlen(buf)-1]!='/') strcat(buf,"/");
    strcat(buf,filename);
    if(execve(buf,argv,envp)==-1){
      while(*path&&*path!=':') ++path;
      if(*path==':') ++path;
    }else assert(0);
  }
  assert(0);
}

char ** parse_args(char * argv[]){
  int num=0;
  for(char ** temp=argv;*temp;temp++) num++;
  char ** work_argv=malloc((num+2)*sizeof(char *));
  work_argv[0]="strace";
  work_argv[1]="-T";
  for(int i=1;i<num;i++) work_argv[i+1]=argv[i];
  work_argv[num+1]=NULL;
  return work_argv;
}

int main(int argc, char *argv[],char * envp[]) {
  char ** work_argv=parse_args(argv);
  my_execvp("strace",work_argv,envp);
}
