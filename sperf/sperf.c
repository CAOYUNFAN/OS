#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <assert.h>

#ifdef LOCAL
#define DEBUG(fmt,...) printf(fmt,__VA_ARGS__)
#else
#define DEBUG(fmt,...) ((void)0)
#endif

#ifdef TEST
#define DEBUG2(fmt,...) DEBUG(fmt,__VA_ARGS__)
#else
#define DEBUG2(fmt,...) ((void)0)
#endif

void copy(char * dest,char * src){
  for(;*src&&*src!=':';++src,++dest) *dest=*src;
  *dest=0;
  return;
}

void my_execvp(char * filename,char * argv[],char * envp[]){
  char * path=getenv("PATH");
  if(!path||strchr(filename,'/')) {
    execve(filename,argv,envp);
    exit(EXIT_FAILURE);
  }
  char buf[128];
  while (*path){
    copy(buf,path);
    if(buf[strlen(buf)-1]!='/') strcat(buf,"/");
    #ifndef LOCAL
    if(strcmp(buf,"/usr/bin/")==0) continue;
    #endif
    strcat(buf,filename);
    if(execve(buf,argv,envp)==-1){
      while(*path&&*path!=':') ++path;
      if(*path==':') ++path;
    }else {
      exit(EXIT_FAILURE);
    }
  }
  exit(EXIT_FAILURE);
}

char ** parse_args(int argc,char * argv[]){
  int num=argc+1;
  char ** work_argv=malloc((num+1)*sizeof(char *));
  work_argv[0]="strace";
  work_argv[1]="-T";
  for(int i=1;i<num;i++) work_argv[i+1]=argv[i];
  work_argv[num]=NULL;
  return work_argv;
}

#define N 16382
char s[N];

inline int is_w(char x){
  return (x>='a'&&x<='z')||(x>='A'&&x<='Z')||(x>='0'&&x<='9')||x=='_';
}

char * get_name(char * s){
  while (*s&&!is_w(*s)) s++;
  if(!*s) return NULL;
  char * temp=s;
  int num=0;
  while(is_w(*s)) ++s,++num;
  char * ret=malloc((num+1)*sizeof(char));
  strncpy(ret,temp,num);
  ret[num]=0;
  return ret;
}

int get_time(double *x,char * s){
  char * temp=s+strlen(s)-1;
  while(temp>=s&&*temp!='<') --temp;
  if(*temp!='<') return 0;
  sscanf(temp+1,"%lf",x);
  return 1;
}

typedef struct unit_t{
  char * name;
  double time;
  struct unit_t * nxt;
}unit;

unit * head=NULL;
double time_all=0;

void work(char * name,double time){
  time_all+=time;
  for(unit * now=head;now;now=now->nxt)
  if(strcmp(name,now->name)==0){
    now->time+=time;
    free(name);
    DEBUG2("add %s ,time=%lf,time_all=%lf\n",now->name,now->time,time_all);
    return;
  }
  unit * temp=(unit *)malloc(sizeof(unit));
  temp->name=name;temp->time=time;temp->nxt=head;head=temp;
  DEBUG2("new %s ,time=%lf,time_all=%lf\n",temp->name,temp->time,time_all);
  return;
}

int is_fail(char * s){
  while(*s&&!(is_w(*s)||*s=='+')) ++s;
  return *s=='+';
}

time_t get_time2(){
  time_t ti=time(NULL);
  return ti;
}

void output(){
  DEBUG("TIME:%ld\n",get_time2());
  unit * all[5];
  for(int i=0;i<5;++i) all[i]=NULL;
  for(unit * now=head;now;now=now->nxt) 
  for(int i=0;i<5;++i) if(!all[i]||all[i]->time<now->time){
    for(int j=4;j>i;--j) all[j]=all[j-1];
    all[i]=now;
    break;
  }
  for(int i=0;i<5&&all[i];i++){
    int percent=(int)(all[i]->time*100.0/time_all);
    printf("%s (%d%%)\n",all[i]->name,percent);
  }

  if(all[0])for(int i=0;i<80;++i) putchar('\0');
  DEBUG("%s\n","=================");
  fflush(stdout);
}

int main(int argc, char *argv[],char * envp[]) {
  char ** work_argv=parse_args(argc,argv);
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
    #ifdef LOCAL
    int fdd=open("/dev/null",O_WRONLY);
    dup2(fdd,STDOUT_FILENO);
    #endif
    my_execvp("strace",work_argv,envp);
    exit(EXIT_FAILURE);
  }
  
  close(pipe_fd[1]);
  dup2(pipe_fd[0],STDIN_FILENO);
  time_t now=get_time2();
  while (fgets(s,N,stdin)){
    DEBUG2("%s",s);
    assert(strlen(s)>0);
    if(is_fail(s)) break;
    char * name=get_name(s);
    if(name==NULL) assert(0);
    double time_used;
    if(!get_time(&time_used,s)) break;
    DEBUG2("%s %lf\n",name,time_used);
    work(name,time_used);
    time_t later=get_time2();
    if(later>now){
      output();
      now=later;
    }
  }
  output();
  return 0;
}
