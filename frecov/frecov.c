#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;

// Copied from the manual
struct fat32hdr {
  u8  BS_jmpBoot[3];
  u8  BS_OEMName[8];
  u16 BPB_BytsPerSec;
  u8  BPB_SecPerClus;
  u16 BPB_RsvdSecCnt;
  u8  BPB_NumFATs;
  u16 BPB_RootEntCnt;
  u16 BPB_TotSec16;
  u8  BPB_Media;
  u16 BPB_FATSz16;
  u16 BPB_SecPerTrk;
  u16 BPB_NumHeads;
  u32 BPB_HiddSec;
  u32 BPB_TotSec32;
  u32 BPB_FATSz32;
  u16 BPB_ExtFlags;
  u16 BPB_FSVer;
  u32 BPB_RootClus;
  u16 BPB_FSInfo;
  u16 BPB_BkBootSec;
  u8  BPB_Reserved[12];
  u8  BS_DrvNum;
  u8  BS_Reserved1;
  u8  BS_BootSig;
  u32 BS_VolID;
  u8  BS_VolLab[11];
  u8  BS_FilSysType[8];
  u8  __padding_1[420];
  u16 Signature_word;
} __attribute__((packed));

struct _dirStrct{
  u8 DIR_name[11];
  u8 DIR_Attr;
  u8 DIR_NTRes;
  u8 DIR_CrtTimeTenth;
  u16 DIR_CrtTime;
  u16 DIR_CrtDate;
  u16 DIR_LstAccDate;
  u16 DIR_FstClusHI;
  u16 DIR_WrtTime;
  u16 DIR_WrtDate;
  u16 DIR_FstClusLO;
  u32 DIR_FileSize;
}__attribute__((packed));
typedef struct _dirStrct dirStrct;

struct _lnameStrct{
  u8 LDIR_Ord;
  u8 LDIR_Name1[10];
  u8 LDIR_Attr;
  u8 LDIR_Type;
  u8 LDIR_Chksum;
  u8 LDIR_Name2[12];
  u16 LDIR_FstClusLO;
  u8 LDIR_Name3[4]; 
}__attribute__((packed));
typedef struct _lnameStrct lnameStrct;

struct _bmpHdr{
  u16 bfType;
  u32 bfSize;
  u32 bfReserved;
  u32 boffBits;
}__attribute__((packed));;
typedef struct _bmpHdr bmpHdr;

struct _bmpInfo{
  u32 bisize;
  u32 biWidth;
  u32 biHeight;
  u16 biPlanes;
  u16 biBitCount;
  u32 biCompression;
  u32 biSizeImages;
  int biXPelsPerMeter;
  int biYPelsPerMeter;
  u32 biClrUsed;
  u32 biClrImportant;
}__attribute__((packed));
typedef struct _bmpInfo bmpInfo;

void * start_of_file=NULL, * start_of_FAT=NULL,* start_of_data=NULL, *end_of_file =NULL;
#define OFFSET_BASIC(byte,start) ((void *)(((u8 *)start)+byte))
#define OFFSET_BASIC_TYPE(byte,start,type) ((type)OFFSET_BASIC(byte,start))
#define OFFSET_BASIC_NUM(num,bytepernum,start) OFFSET_BASIC((num)*(bytepernum),start)
#define OFFSET_BASIC_NUM_TYPE(num,bytepernum,start,type) OFFSET_BASIC_TYPE((num)*(bytepernum),start,type)

#define OFFSET_FILE(byte) OFFSET_BASIC(byte,start_of_file)
#define OFFSET_FILE_TYPE(byte,type) OFFSET_BASIC_TYPE(byte,start_of_file,type)
#define OFFSET_FILE_NUM(num,bytepernum) OFFSET_BASIC_NUM(num,bytepernum,start_of_file)
#define OFFSET_FILE_NUM_TYPE(num,bytepernum,type) OFFSET_BASIC_NUM_TYPE(num,bytepernum,start_of_file,type)

#define OFFSET_DATA(byte) OFFSET_BASIC(byte,start_of_data)
#define OFFSET_DATA_TYPE(byte,type) OFFSET_BASIC_TYPE(byte,start_of_data,type)
#define OFFSET_DATA_NUM(num,bytepernum) OFFSET_BASIC_NUM(num,bytepernum,start_of_data)
#define OFFSET_DATA_NUM_TYPE(num,bytepernum,type) OFFSET_BASIC_NUM_TYPE(num,bytepernum,start_of_data,type)

#ifdef LOCAL
  #define DEBUG(...) __VA_ARGS__
#else
  #define DEBUG(...) ((void)0);
#endif

struct fat32hdr * hdr;
int bytsperclus,bytspersec;
u16 type[32768];

void *map_disk(const char *fname) {
  int fd = open(fname, O_RDWR);

  if (fd < 0) {
    perror(fname);
    goto release;
  }

  off_t size = lseek(fd, 0, SEEK_END);
  if (size == -1) {
    perror(fname);
    goto release;
  }

  struct fat32hdr *hdr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
  if (hdr == (void *)-1) {
    goto release;
  }

  close(fd);

  if (hdr->Signature_word != 0xaa55 ||
      hdr->BPB_TotSec32 * hdr->BPB_BytsPerSec != size) {
    fprintf(stderr, "%s: Not a FAT file image\n", fname);
    goto release;
  }
  return hdr;

release:
  if (fd > 0) {
    close(fd);
  }
  exit(1);
}

inline static void parse_args(int argc,char * argv[]){
  if (argc < 2) {
    fprintf(stderr, "Usage: %s fs-image\n", argv[0]);
    exit(1);
  }
  setbuf(stdout, NULL);
  assert(sizeof(struct fat32hdr) == 512);
  assert(sizeof(dirStrct)==32);
  assert(sizeof(lnameStrct)==32);

  start_of_file = map_disk(argv[1]);
  hdr = OFFSET_FILE_TYPE(0, struct fat32hdr *);

  bytspersec=hdr->BPB_BytsPerSec;
  bytsperclus=hdr->BPB_SecPerClus*bytspersec;

  start_of_FAT = OFFSET_FILE_NUM(hdr->BPB_RsvdSecCnt,bytspersec);
  start_of_data =OFFSET_FILE_NUM(hdr->BPB_RsvdSecCnt+hdr->BPB_FATSz32*hdr->BPB_NumFATs,bytspersec);
  end_of_file = OFFSET_FILE_NUM(hdr->BPB_TotSec32,bytspersec);
  DEBUG(printf("file:%p data:%p\n",start_of_file,start_of_data);)
  DEBUG(printf("BytesPerSec:%d,BytesPerCluster:%d\n",bytspersec,bytsperclus);)
}

int is_unused(void * ptr){
  u8 * begin=ptr;
  for(int num=0;num<bytsperclus;++num,++begin) if(*begin) return 0;
  return 1;
}
int is_bmp_hdr(void * ptr,int filesize){
  assert(sizeof(bmpHdr)==14);assert(sizeof(bmpInfo)==40);
  bmpHdr * bmphdr=ptr;
  if(bmphdr->bfType!=0x4d42||bmphdr->bfSize!=filesize||bmphdr->bfReserved!=0||bmphdr->boffBits!=54) return 0;
  bmpInfo * bmpinfo=OFFSET_BASIC_TYPE(14,ptr,bmpInfo *);
  if(bmpinfo->bisize!=40||bmpinfo->biPlanes!=1||bmpinfo->biBitCount!=24||bmpinfo->biCompression!=0||bmpinfo->biSizeImages!=filesize-54) return 0;
  if(bmpinfo->biXPelsPerMeter!=0xec4||bmpinfo->biYPelsPerMeter!=0xec4||bmpinfo->biClrUsed!=0||bmpinfo->biClrImportant!=0) return 0;
  return 1;
}
inline static int check_char(u8 ch){
  return 
    (ch>='0'&&ch<='9')||
    (ch>='A'&&ch<='Z')||
    (ch>='a'&&ch<='z')||
    (ch=='-')||(ch=='_')||(ch=='.')||
    (ch=='~')||(ch==' ')||(ch==0)||(ch==0xff);
}
inline static int check_char2(u8 ch){
  if(ch<0x20) return 0;
  if(ch>='a'&&ch<='z') return 0;
  if(ch==0x22||(ch>=0x2a&&ch<=0x2f)||(ch>=0x3a&&ch<=0x3f)||(ch>=0x5b&&ch<=0x5d)||ch==0x7c||ch==0xff) return 0;
  return check_char(ch);
}
int check_lname(lnameStrct * lname){
  if(lname->LDIR_Type!=0) return 0;
  if(lname->LDIR_FstClusLO!=0) return 0;
  u8 ch[26];
  for(int i=0;i<10;i++) ch[i]=lname->LDIR_Name1[i];
  for(int i=0;i<12;i++) ch[i+10]=lname->LDIR_Name2[i];
  for(int i=0;i<4;++i) ch[i+22]=lname->LDIR_Name3[i];
  int i=0;
  for(;i<26;i+=2){
    if(ch[i+1]!=0||!check_char(ch[i])) return 0;
    if(ch[i]==0) break;
  }

  if(i<26){
    if(!(lname->LDIR_Ord&0x40)) return 0;
    for(int j=i+2;j<26;++j) if(ch[j]!=0xff) return 0;
    i-=2;
    int flag=(ch[i]=='p'?'a'-'A':0);

    for(int k=3;i>=0&&k>=0;i-=2,k--){
      switch(k){
        case 3:if(ch[i]!='P'+flag) return 0; break;
        case 2:if(ch[i]!='M'+flag) return 0; break;
        case 1:if(ch[i]!='B'+flag) return 0; break;
        case 0:if(ch[i]!='.') return 0; break;
        default: break;
      }
    }
  }
  return 1;
}
int check_dir(dirStrct * dir,int tag){
  if(dir->DIR_Attr==((u8)0xf)) return check_lname((lnameStrct *)dir);
  if(dir->DIR_NTRes!=0) return 0;
  if(dir->DIR_name[0]==0xe5) return 2;
  if(dir->DIR_name[0]==0x00) return 2;
  if(dir->DIR_CrtTimeTenth>199) return 0;
  if(dir->DIR_Attr>=0x40) return 0;
  u32 addr=(u32)dir->DIR_FstClusHI<<16|dir->DIR_FstClusLO;
  if(dir->DIR_Attr&0x08){
    if(addr!=0) return 0;
  }
  else if(addr>=2&&OFFSET_DATA_NUM(addr-2,bytsperclus)>=end_of_file) return 0;

  if(dir->DIR_Attr&0x10){
    if(dir->DIR_FileSize!=0) return 0;
    if(dir->DIR_name[0]=='.'){
      for(int i=1;i<11;++i) if(dir->DIR_name[i]!=' '&& !(i==2||dir->DIR_name[i]=='.') ) return 0;
      return 1;
    }
    if(dir->DIR_name[0]=='D'&&dir->DIR_name[1]=='C'&&dir->DIR_name[2]=='I'&&dir->DIR_name[3]=='M'){
      for(int i=4;i<11;++i) if(dir->DIR_name[i]!=' ') return 0;
      return 1;
    }
  }else{
    if(dir->DIR_name[0]==0x20) return 0;
    for(int i=0;i<11;++i) if(!check_char2(dir->DIR_name[i])) return 0;
    if(dir->DIR_name[8]=='B'&&dir->DIR_name[9]=='M'&&dir->DIR_name[10]=='P') {
      type[addr]=tag;
      return 1;
    }
  }
  return 0;
}
int is_dir(void * ptr,int tag){
  assert(tag>=2&&tag<32768);
  dirStrct * now=ptr;
  int temp=0;
  for(int tot_idents=bytsperclus/sizeof(dirStrct);tot_idents;tot_idents--,now++){
    int res=check_dir(now,tag);
    if(!res) return 0;
    if(res==1) temp=1;
  }
  return temp;
}
char * get_short_name(dirStrct * ptr,int * chk){
  static char temp[16];int len=0;*chk=0;
  for(int i=0;i<11;i++) if(ptr->DIR_name[i]!=' '){
    if(ptr->DIR_name[i]=='~') *chk=1;
    if(i==8) temp[len++]='.';
    temp[len++]=ptr->DIR_name[i];
  }
  temp[len]=0;
  return temp;
}


static char buf[1024];
#ifdef LOCAL
const char * filepath="bmp/%s";
#else
const char * filepath="/tmp/%s";
#endif
int file_recovery(void * ptr,u32 filesize,FILE * file);
int get_file(void * ptr,u32 filesize,char * filename){
  static char name[500],cmd[512];
  sprintf(name,filepath,filename);
  sprintf(cmd,"sha1sum %s",name);
  FILE * fd=fopen(name,"wb");
  int ret=file_recovery(ptr,filesize,fd);
  fclose(fd);
  if(ret){
    FILE * fp=popen(cmd,"r");
    assert(fp>=0);
    fscanf(fp, "%s", buf); // Get it!
    pclose(fp);
  }
  return ret;
}

void work(void * ptr){
  dirStrct * now=ptr;
  static char longname[256];
  for(int tot_idents=bytsperclus/sizeof(dirStrct);tot_idents;tot_idents--,now++){
    if(now->DIR_name[0]==0x00||now->DIR_name[0]==0xe5||now->DIR_Attr&0x10||now->DIR_Attr==0xf) continue;
    int chk=0; char * name=get_short_name(now,&chk);
    if(chk){
      name=longname;int len=0;
      lnameStrct * pre=(lnameStrct *)now;

      #define fpp(ch) if((ch)!=0&&(ch)!=0xff) longname[len++]=ch
      
      for(--pre;(u8 *)pre>=(u8 *)ptr;--pre){
        for(int i=0;i<10;i+=2) fpp(pre->LDIR_Name1[i]);
        for(int i=0;i<12;i+=2) fpp(pre->LDIR_Name2[i]);
        for(int i=0;i<4;i+=2) fpp(pre->LDIR_Name3[i]);
        if(pre->LDIR_Ord&0x40) break;
      }
      longname[len]=0;
      if(!(pre->LDIR_Ord&0x40)) continue;
    }
    DEBUG(static int tot=1;)
    u32 addr=(u32)now->DIR_FstClusHI<<16|now->DIR_FstClusLO;
    
    DEBUG(printf("Name %d:\n",tot++);)
    if(get_file(OFFSET_DATA_NUM(addr-2,bytsperclus),now->DIR_FileSize,name)) printf("%s  %s\n",buf,name);
    else printf("  %s\n",name);
    DEBUG("start at %x\n",addr);
    fflush(stdout);
  }
  return;
}

int main(int argc, char *argv[]) {
  parse_args(argc,argv);
  // TODO: frecov
  int tot=0;
  for(int i=0;OFFSET_DATA_NUM(i,bytsperclus)<end_of_file;i++){
    if(type[i+2]!=0) continue;
    void * page=OFFSET_DATA_NUM(i,bytsperclus);
    if(is_unused(page)) type[i+2]=-1;
    else if(is_dir(page,i+2)) type[i+2]=1;
    else type[i+2]=0;
    tot=i+2;
  }
  for(int i=2;i<=tot;i++) if(type[i]!=1&&type[i]!=-1&&type[i]!=0){
    if(type[type[i]]==1) type[i]=2;
    else type[i]=0;
  }
  for(int i=0;i<=tot-2;i++) if(type[i+2]==1){
    void * page=OFFSET_DATA_NUM(i,bytsperclus);
    DEBUG(printf("dir:%x\n",i+2);)
    work(page);
  }
  // file system traversal
  munmap(start_of_file, hdr->BPB_TotSec32 * hdr->BPB_BytsPerSec);
}


typedef long long LL;
int chk(u8 * x,u8 * y,int len){
  assert(x+len<=(u8 *)end_of_file);
  assert(y+bytsperclus<=(u8 *)end_of_file);
  LL sum=0;
  double sigma=15.0;
  for(int i=0;i<len;i++) sum+=((LL)x[i]-(LL)y[i]);
//  if(abs(sum)>3.0*sigma*sqrt((double)len)) return 0;
  return 1;
}
void * next_cluster(void * ptr,u32 rowsize){
  void * nxtptr=OFFSET_BASIC(bytsperclus,ptr);
  int num=((u8 *)nxtptr-(u8 *)start_of_data)/bytsperclus+2;
  if(nxtptr<end_of_file && type[num]==0 && chk((u8 *)nxtptr-rowsize,nxtptr,rowsize)) return nxtptr;
//  DEBUG(printf("FAIL for nxtptr,");)
  for(int i=0;OFFSET_DATA_NUM(i,bytsperclus)<end_of_file;i++) if(type[i+2]!=1&&type[i+2]!=(u16)(-1)&&type[type[i+2]]!=1){
    void * page=OFFSET_DATA_NUM(i,bytsperclus);
    if(chk((u8 *)nxtptr-rowsize,page,rowsize)) {
//      printf( "To %x %p",i,page);
      return page;
    }
  }
  if(nxtptr<end_of_file) return nxtptr;
  else return NULL;
}

int file_recovery(void * ptr,u32 filesize,FILE * file){
  if(!is_bmp_hdr(ptr,filesize)) return 0;
  bmpInfo * bmpinfo=OFFSET_BASIC_TYPE(14,ptr,bmpInfo *);
  u32 rowsize=4*((3*bmpinfo->biWidth+3)/4);
  while(filesize) {
    #define Min(x,y) ((x)<(y)?(x):(y))
    filesize-=fwrite(ptr,1,Min(filesize,bytsperclus),file);
    DEBUG(printf("#%x ",(u32)((u8 *)ptr-(u8 *)start_of_data)/bytsperclus+2);)
    if(filesize) ptr=next_cluster(ptr,rowsize);
    if(!ptr) return 0;
  }
  DEBUG(printf("\n");)//printf("OUT!\n");
  return 1;
}
/*void dummy(){
  printf("DUMMY!\n");
  return;
}*/