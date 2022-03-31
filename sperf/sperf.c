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
  char * path=my_getenv(envp);printf("%s\n",path);
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

int main(int argc, char *argv[],char * envp[]) {
//  for(char ** temp=argv;*temp;temp++) puts(*temp);
//  puts("END OF ARGC!");
//  for(char ** temp=envp;*temp;temp++) puts(*temp);
//  puts("END OF ENVP!");
  char *exec_argv[] = { "strace", "ls", NULL, };
  char *exec_envp[] = { "PATH=/bin", NULL, };
  my_execvp("strace",exec_argv,envp);
/*  execve("strace",          exec_argv, exec_envp);
  execve("/bin/strace",     exec_argv, exec_envp);
  execve("/usr/bin/strace", exec_argv, exec_envp);
  perror(argv[0]);
  exit(EXIT_FAILURE);*/
}
