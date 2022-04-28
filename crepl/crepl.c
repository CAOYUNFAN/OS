#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>

char * file_templelate ="int __expr_wrapper_%d () { return %s ;}\n";
char * gcc_arg[]={"gcc","-fPIC","-shared","-o",NULL,NULL,NULL};

void make_link(char * data){
  char name[]="/tmp/filename-XXXXXX";
  int tt=mkstemp(name);
  assert(tt>=0);
  FILE * fd=fopen(name,"w");assert(fd);
  #ifdef LOCAL
  printf("NAME=\n%s\n",name);
  #endif
  fputs(data,fd);
  fclose(fd);
  pid_t pid=fork();
  char name2[25];
  sprintf(name2,"%s.so",name);  
  if(!pid){
    gcc_arg[4]=name2;gcc_arg[5]=name;
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
