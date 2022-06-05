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


void * start_of_file=NULL, * start_of_FAT=NULL,* start_of_data=NULL;
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
  DEBUG(printf("%p %p\n",start_of_file,start_of_data);)
}

int main(int argc, char *argv[]) {
  parse_args(argc,argv);
  // TODO: frecov

  // file system traversal
  munmap(start_of_file, hdr->BPB_TotSec32 * hdr->BPB_BytsPerSec);
}
