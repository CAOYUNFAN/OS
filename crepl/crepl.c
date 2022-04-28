#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>

char * file_templelate ="int __expr_wrapper_%d () { return %s ;}\n";

void make_link(char * data){
  char name[]="filename-XXXXXX";
  int tt=mkstemp(name);
  assert(tt>=0);
  FILE * fd=fopen(name,"r");assert(fd);
  #ifdef LOCAL
  printf("NAME=\n%s\n",name);
  #endif
  fputs(data,fd);
  fclose(fd);
  pid_t pid=fork();
  if(!pid){
    char name2[20];
    sprintf(name2,"%s.so",name);  
    execlp("gcc","-fPIC","-shared","-o",name2,name,NULL);
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
