#include <game.h>

// Operating system is a C program!
#define FPS 250

inline uint64_t uptime(){
  AM_TIMER_UPTIME_T ret;
  ioe_read(AM_TIMER_UPTIME,&ret);
  return ret.us;
}

int main(const char *args) {
  ioe_init();

//  puts("mainargs = \"");
//  puts(args); // make run mainargs=xxx
//  puts("\"\n");
//  printf("HERE!\n");
  splash();

  puts("Press any key to see its key code...\n");
  while (1) {
    func_key();
  }
//  uint64_t next_frame=0;
/*  while (1) {
//    printf("A!\n");
    while(uptime()<next_frame);
    func_key();
    splash();
    next_frame+=1000/FPS;
  }*/
  return 0;
}
/*
  splash();

  puts("Press any key to see its key code...\n");
  while (1) {
    print_key();
  }
*/