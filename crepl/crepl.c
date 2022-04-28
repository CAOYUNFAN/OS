#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>

char * file_templelate ="int __expr_wrapper_%d () { return %s ;}\n";
char * gcc_arg[]={"gcc","-fPIC","-shared","-o",NULL,NULL,NULL};

void make_tmp_file(char * name,char * name_so){
  int tt=mkstemps(name,2);
  assert(tt>=0);
  int i=0;
  for(;name[i]&&name[i]!='c';i++) name_so[i]=name[i];
  name_so[i]='s';name_so[i+1]='o';name_so[i+2]=0;
  return;
}

void make_link(char * data){
  char name[]="/tmp/filename-XXXXXX.c";
  char name_so[25];
  make_tmp_file(name,name_so);
  FILE * fd=fopen(name,"w");assert(fd);
  #ifdef LOCAL
  printf("NAME=\n%s\n",name);
  #endif
  fputs(data,fd);
  fclose(fd);
  pid_t pid=fork();
  if(!pid){
    gcc_arg[4]=name_so;gcc_arg[5]=name;
    execvp("gcc",gcc_arg);
  }
  
}

int main(int argc, char *argv[]) {
  static char line[4096];
  while (1) {
    printf("crepl> ");
    fflush(stdout);
    if (!fgets(line, sizeof(line), stdin)) {
      break;
    }
    make_link(line);
    #ifdef LOCAL
    printf("Got %zu chars.\n", strlen(line)); // ??
    #endif
  }
}
