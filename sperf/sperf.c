#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>

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

inline int is_w(char x){
  return (x>='a'&&x<='z')||(x>='A'&&x<='Z')||(x>='A'&&x<='Z')||x=='_';
}

char * get_name(char * s){
  while (*s&&!is_w(*s)) s++;
  if(!*s) return NULL;
  char * temp=s;
  int num=0;
  while(is_w(*s)) ++s,++num;
  char * ret=malloc((num+1)*sizeof(char));
  strncpy(ret,temp,num);
  return ret;
}

int get_time(double *x,char * s){
  char * temp=s+strlen(s)-1;
  while(temp>=s&&*temp!='<') --temp;
  if(*temp!='<') return 0;
  printf("%s ",temp);
  sscanf(temp+1,"%lf",x);
  return 1;
}

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
  time_t now=time(NULL);
  while (fgets(s,10000,stdin)){
    printf("%s",s);
    char * name=get_name(s);
    if(name==NULL) continue;
    double time_used;
    if(!get_time(&time_used,s)) continue;
    printf("%s %lf\n",name,time_used);
    if(*s=='+') {
      printf("HERE!\n");
      return 0;
    }
  }
  exit(EXIT_FAILURE);
}
