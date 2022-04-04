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
    exit(EXIT_FAILURE);
  }
  char buf[128];
  while (*path){
    copy(buf,path);
    if(buf[strlen(buf)-1]!='/') strcat(buf,"/");
    strcat(buf,filename);
    if(execve(buf,argv,envp)==-1){
      while(*path&&*path!=':') ++path;
      if(*path==':') ++path;
    }else exit(EXIT_FAILURE);
  }
  exit(EXIT_FAILURE);
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

char s[10000];

int main(int argc, char *argv[],char * envp[]) {
  char ** work_argv=parse_args(argv);
  int pipe_fd[2];
  
  if(pipe(pipe_fd)==-1){
    perror("pipe");
    exit(EXIT_FAILURE);
  }
  
  pid_t cpid=fork();
  if(cpid==-1){
    perror("fork");
    exit(EXIT_FAILURE);
  }

  if(cpid==0){
    close(pipe_fd[0]);
    dup2(pipe_fd[1],STDERR_FILENO);
    my_execvp("strace",work_argv,envp);
    exit(EXIT_FAILURE);
  }
  close(pipe_fd[1]);
  dup2(pipe_fd[0],STDIN_FILENO);
  while (scanf("%s",s)){
    if(*s=='+') return 0;
  //  printf("%s",s);
  }
  exit(EXIT_FAILURE);
}
