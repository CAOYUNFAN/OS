#include <stdio.h>
#include <assert.h>

#include <stdlib.h>
#include <getopt.h>
#include <sys/types.h>
#include <dirent.h>
#include <string.h>

const struct option table[]={
  {"show-pids"    ,no_argument  ,NULL ,'p'},
  {"numeric-sort" ,no_argument  ,NULL ,'n'},
  {"version"      ,no_argument  ,NULL ,'V'},
  {0              ,0            ,0    ,0}
};

int flag_n,flag_p;
void prase_args(int argc,char * argv[]){
  int o;
  while ((o=getopt_long(argc,argv,"-pnV",table,NULL))!=-1){
    switch (o) {
    case 'V':
      fprintf(stderr,"pstree (for OS) 0.0\n");
      exit(0);
    case 'n':
      flag_n=1;printf("-n!\n");
      break;
    case 'p':
      flag_p=1;printf("-p!\n");
      break;
    default:
      fprintf(stderr,"Usage : pstree [OPTION]\n");
      fprintf(stderr,"\t-V,--version\n");
      fprintf(stderr,"\t-n,--numeric-sort\n");
      fprintf(stderr,"\t-p,--show-pids\n");
      exit(1);
    }
  }
  return;
}

#define N 10000
#define M 256
int n;
pid_t pid[N],fa[N];
char name[N][M];

int check(const char * st){
  for(;*st;st++) if(*st<'0'||*st>'9') return 0;
  return 1;
}
char st[M]="/proc/";
void work(){
  DIR *dir=opendir("/proc/");assert(dir);
  struct dirent *entry;
  FILE * fp;
  n=0;
  while(entry=readdir(dir))
  if(check(entry->d_name)&&entry->d_type==DT_DIR){
    int i=strlen("/proc/");
    for(char * ch=entry->d_name;*ch;++ch,++i) st[i]=*ch;
    st[i]=0;strcat(st,"/stat");
    fp=fopen(st,"r");assert(fp);
    char ch;
    fscanf(fp,"%d%s%c%d",pid[n],name[n],&ch,fa[n]);
    printf("%d %s %d\n",pid[n],name[n],fa[n]);
    fclose(fp);
    n++;
  }
  return;
}

int main(int argc, char *argv[]) {
  prase_args(argc,argv);
  work();

  return 0;
}

/*
  for (int i = 0; i < argc; i++) {
    assert(argv[i]);
    printf("argv[%d] = %s\n", i, argv[i]);
  }
  assert(!argv[argc]);
*/
