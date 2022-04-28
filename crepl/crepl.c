#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <dlfcn.h>

char * file_templelate = "int __expr_wrapper_%d () { return %s ;}\n";
char * func_templelate = "__expr_wrapper_%d";
char * gcc_arg[]={"gcc","-fPIC","-shared","-o",NULL,NULL,NULL};

void make_tmp_file(char * name,char * name_so){
  int tt=mkstemps(name,2);
  assert(tt>=0);
  int i=0;
  for(;name[i]&&name[i]!='c';i++) name_so[i]=name[i];
  name_so[i]='s';name_so[i+1]='o';name_so[i+2]=0;
  return;
}

void * make_link(char * data){
  char name[]="filename-XXXXXX.c";
  char name_so[25];
  make_tmp_file(name,name_so);
  FILE * fd=fopen(name,"w");assert(fd);
  #ifdef LOCAL
  printf("NAME=%s,NAME2=%s\n",name,name_so);
  #endif
  fputs(data,fd);
  fclose(fd);
  pid_t pid=fork();
  if(!pid){
    gcc_arg[4]=name_so;gcc_arg[5]=name;
    int fdd=open("/dev/null",O_WRONLY);
    dup2(fdd,STDOUT_FILENO);
    dup2(fdd,STDERR_FILENO);
    execvp("gcc",gcc_arg);
    assert(0);
  }
  int wstauts;
  waitpid(pid,&wstauts,0);
  if(!WIFEXITED(wstauts)){
    printf("Compile Error!\n");
    return NULL;
  }
  #ifdef LOCAL
  printf("Compile Completed!\n");
  #endif
  return dlopen(name_so,RTLD_NOW|RTLD_DEEPBIND);
}

char func_main[4096],func_name[4096];

typedef int (*)() my_func;

int main(int argc, char *argv[]) {
  static char line[4096];
  int x=0;
  while (1) {
    
    printf("crepl> ");
    fflush(stdout);
    if (!fgets(line, sizeof(line), stdin)) {
      break;
    }
    #ifdef LOCAL
    printf("%s\nGot %zu chars.\n", line,strlen(line)); // ??
    #endif
    if(strncmp(line,"int ",4)==0){
      if(make_link(line)) puts("OK.");
    }else{
      sprintf(func_main,file_templelate,x,line);
      void * handle=make_link(func_main);
      if(!handle) continue;
      sprintf(func_name,func_templelate,x);
      my_func ans_func=dlsym(handle,func_name);
      printf("%d\n"ans_func());
    }
    x++;
  }
  #ifdef LOCAL
  putchar('\n');
  #endif
}
