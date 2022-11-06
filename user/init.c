#include "ulib.h"

void puts(const char * s){
  for(;*s;s++) kputc(*s);
  return;
}

void putnum(int x){
  if(x){
    putnum(x/10);
    kputc(x%10+'0');
  }
}

void puts2(const char * s,int x){
  for(;*s;s++){
    if(*s!='%') kputc(*s);
    else{
      s++;
      if(x<0) kputc('-'),x=-x;
      if(x==0) kputc('0');
      putnum(x);
    }
  }
}

void hello_test();
void dfs_test();
void fork_wait_test();

int main() {
//  puts("Hello from user\n");
//  hello_test();
  dfs_test();
//  fork_wait_test();
  while (1);
}

void hello_test() {
  int pid = fork(), x = 0;

  const char *fmt;
  if (pid) {
    fmt = "Parent #%d\n";
  } else {
    sleep(1);
    fmt = "Child #%d\n";
  }

  while (1) {
    puts2(fmt, ++x);
    sleep(2);
  }
}

#define DEST  '+'
#define EMPTY '.'

static struct move {
  int x, y, ch;
} moves[] = {
  { 0, 1, '>' },
  { 1, 0, 'v' },
  { 0, -1, '<' },
  { -1, 0, '^' },
};

static char map[][16] = {
  "######",
  "#....#",
  "##..+#",
  "######",
  "",
};

void display();

void dfs(int x, int y) {
  if (map[x][y] == DEST) {
    display();
  } else {
    sleep(1);
    int nfork = 0;

    for (struct move *m = moves; m < moves + 4; m++) {
      int x1 = x + m->x, y1 = y + m->y;
      if (map[x1][y1] == DEST || map[x1][y1] == EMPTY) {
        int pid = fork();
        if (pid == 0) { // map[][] copied
          map[x][y] = m->ch;
          dfs(x1, y1);
        } else {
          nfork++;
        }
      }
    }
  }
  while (1) sleep(1);
}

void dfs_test() {
  dfs(1, 1);
  while (1) sleep(1);
}

int strlen(const char *s){
  int ret=0;
  for(;*s;s++) ret++;
  return ret;
}

void display() {
  for (int i = 0; ; i++) {
    for (const char *s = map[i]; *s; s++) {
      switch (*s) {
        case EMPTY: puts("   "); break;
        case DEST : puts(" * "); break;
        case '>'  : puts(" > "); break;
        case '<'  : puts(" < "); break;
        case '^'  : puts(" ^ "); break;
        case 'v'  : puts(" v "); break;
        default   : puts("000"); break;
      }
    }
    puts("\n");
    if (strlen(map[i]) == 0) break;
  }
}

#define N 1000
void fork_wait_test(){
  int n, pid;
  int * addr=mmap(NULL,4096,PROT_READ|PROT_WRITE,MAP_SHARED);
  addr[1]=1919810;
//  puts("fork test\n");

  for(n=0; n<N; n++){
    pid = fork();
    if(pid < 0)
      break;
    if(pid == 0){
      int x=getpid();
      addr[x]=x;
      puts2("%d",x);
      if(addr[1]!=1919810) kputc('E');
      kputc('\n');
      exit(114514);
    }  
  }
  
  /*if(n == N){
    puts("fork claimed to work N times!\n");
    exit(255);
  }*/
  int ret=0;
  for(; n > 0; n--){
    if(wait(&ret)<0||ret!=114514){
      puts("wait stopped early\n");
      exit(0);
    }
  }
  
  if(wait(&ret) != -1){
    puts( "wait got too many\n");
    exit(0);
  }
  
  kputc('\n');
  for(int i=2;i<N+2;i++) if(addr[i]!=i){
    puts2("do not change shared map! %d\n",i);
  }

  puts("fork test OK\n");
  while (1) ;
}
