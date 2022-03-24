#include <common.h>

#ifdef TEST
  #include <assert.h>
  #include <stdio.h>
  #define DEBUG(...) (__VA_ARGS__)
#else
  #define DEBUG()
#endif


#define MAX_malloc (16*1024*1024)
#define Unit_size (MAX_malloc)
#define Unit_mask (-Unit_size)

#define HEAP_START ROUNDUP((uintptr_t)heap.start,Unit_size)
#define HEAP_END ROUNDDOWN((uintptr_t)heap.end,Unit_size)
#define total_num ((HEAP_END-HEAP_START)/Unit_size)

#define LOWBIT(x) ((x)&((x)^((x)-1)))

#ifdef TEST
#define MAGIC_UNUSED (0x7f)
#define MAGIC_USED (0x69)
#endif
#define MAGIC_UNLOCKED (0)
#define MAGIC_LOCKED (1)
#define MAGIC_MTG (0x11451419)

#define MTG_addr(pos,len) ((mem_tag *)((uintptr_t)pos+len-sizeof(mem_tag)))

typedef int spinlock_t;
typedef unsigned long uintptr_t;

typedef struct __free_list{
  uintptr_t size;
  struct __free_list *nxt;
}free_list;

typedef struct{
  uintptr_t size;
  uintptr_t magic;
}mem_tag;

static inline void spin_lock(spinlock_t *lk) {
  while (1) {
    intptr_t value = atomic_xchg(lk, MAGIC_LOCKED);
    if (value == MAGIC_UNLOCKED) {
      break;
    }
  }
}

static inline void spin_unlock(spinlock_t *lk) {
  atomic_xchg(lk, MAGIC_UNLOCKED);
}

static uintptr_t sbrk_now=0;

static inline void * kernel_alloc(size_t len){
  sbrk_now-=len;
  return (void *)sbrk_now;
}

static free_list * start_of_128;//,start_of_64,start_of_1024;
static uintptr_t heap_128_start,heap_128_end;
static int lock_128;
static inline void init_128(){
  heap_128_start=HEAP_START;heap_128_end=ROUNDUP(HEAP_START,Unit_size)+total_num/4*Unit_size;
  start_of_128=(free_list *)heap_128_start;
  ((free_list *)heap_128_start)->size=128;
  for(uintptr_t ptr=heap_128_start+128;ptr<heap_128_end;ptr+=128){
    ((free_list *)ptr)->size=128;
    ((free_list *)(ptr-128))->nxt=(free_list *)ptr;
  }
  ((free_list *)(heap_128_end-128))->nxt=NULL;
  return;
}
static inline void * kalloc_128(){
  if(start_of_128==NULL) return NULL;
  spin_lock(&lock_128);
  void * ret=(void *) start_of_128;
  if(ret){
    start_of_128=start_of_128->nxt;
    #ifdef TEST
    unsigned long j=0;
    for(unsigned char * ptr=((unsigned char *)ret)+sizeof(free_list);j<128-sizeof(free_list);++ptr,++j) assert(*ptr==MAGIC_UNUSED);
    #endif
  }
  spin_unlock(&lock_128);
  #ifdef TEST
  memset(ret,MAGIC_USED,128);
  #endif
  return ret;
}
static inline void kfree_128(void * ptr){
  free_list * hdr=ptr;
  #ifdef TEST
  memset(ptr,MAGIC_UNUSED,128);
  #endif
  hdr->size=128;
  spin_lock(&lock_128);
  hdr->nxt=start_of_128->nxt;
  start_of_128=hdr;
  spin_unlock(&lock_128);
}


static free_list * start_of_4096;
static uintptr_t heap_4096_start,heap_4096_end;
static int lock_4096;
static inline void init_4096(){
  heap_4096_start=heap_128_end;
  heap_4096_end=heap_4096_start+total_num/4*Unit_size;
  start_of_4096=(free_list *)heap_4096_start;
  ((free_list *)heap_4096_start)->size=4096;
  for(uintptr_t ptr=heap_4096_start+4096;ptr<heap_4096_end;ptr+=4096){
    ((free_list *)ptr)->size=4096;
    ((free_list *)(ptr-4096))->nxt=(free_list *)ptr;
  }
  ((free_list *)(heap_4096_end-4096))->nxt=NULL;
  return;
}
static inline void * kalloc_4096(){
  if(start_of_4096==NULL) return NULL;
  spin_lock(&lock_4096);
  void * ret=(void *) start_of_4096;
  if(ret){
    start_of_4096=start_of_4096->nxt;
    #ifdef TEST
    unsigned long j=0;
    for(unsigned char * ptr=((unsigned char *)ret)+sizeof(free_list);j<4096-sizeof(free_list);++ptr,++j) assert(*ptr==MAGIC_UNUSED);
    #endif
  }
  spin_unlock(&lock_4096);
  #ifdef TEST
  memset(ret,MAGIC_USED,4096);
  #endif
  return ret;
}
static inline void kfree_4096(void * ptr){
  free_list * hdr=ptr;
  #ifdef TEST
  memset(ptr,MAGIC_UNUSED,4096);
  #endif
  hdr->size=4096;
  spin_lock(&lock_4096);
  hdr->nxt=start_of_4096->nxt;
  start_of_4096=hdr;
  spin_unlock(&lock_4096);
}

static uintptr_t heap_rest_start,heap_rest_end;
static free_list ** start_of_rest;
static int lock_rest;
void init_rest(){
  heap_rest_start=heap_4096_end;heap_rest_end=HEAP_END-Unit_size;
  uintptr_t num=0;
  for(uintptr_t i=8192;i<=(MAX_malloc);i<<=1) num++;
  num++;
  start_of_rest=kernel_alloc(sizeof(free_list *)*num);
  uintptr_t j=0;
  for(uintptr_t i=8192;i<MAX_malloc;i<<=1,++j) start_of_rest[j]=NULL;
  start_of_rest[j]=NULL;
  if(heap_rest_end%(MAX_malloc<<1)!=0){
    start_of_rest[j]=(free_list *)(heap_rest_end-MAX_malloc);
    start_of_rest[j]->size=MAX_malloc;
    start_of_rest[j]->nxt=NULL;
  }
  if(heap_rest_start%(MAX_malloc<<1)!=0){
    free_list * temp=(free_list *)heap_rest_start;
    temp->size=MAX_malloc;temp->nxt=start_of_rest[j];
    start_of_rest[j]=temp;
  }
  ++j;
  if(ROUNDUP(heap_rest_start,MAX_malloc<<1)==ROUNDDOWN(heap_rest_end,MAX_malloc<<1)) start_of_rest[j]=NULL;
  else{
    start_of_rest[j]=(free_list *)ROUNDUP(heap_rest_start,MAX_malloc<<1);
    ((free_list *)heap_rest_start)->size=MAX_malloc<<1;
    for(uintptr_t ptr=ROUNDUP(heap_rest_start,MAX_malloc<<1)+(MAX_malloc<<1);ptr<heap_rest_end;ptr+=MAX_malloc<<1){
      ((free_list *)ptr)->size=MAX_malloc<<1;
      ((free_list *)(ptr-(MAX_malloc<<1)))->nxt=(free_list *)ptr;
    }
    ((free_list *)(ROUNDDOWN(heap_rest_end,MAX_malloc<<1)-(MAX_malloc<<1)))->nxt=NULL;
  }  
}
static inline void insert(free_list * insert,free_list ** head){
  if(*head==NULL||(uintptr_t)*head>(uintptr_t)insert){
    insert->nxt=*head;
    *head=insert;
    return;
  }
  for(free_list * now=*head;now!=NULL;now=now->nxt)
  if((uintptr_t)now<(uintptr_t)insert&&(now->nxt==NULL||(uintptr_t)now->nxt>(uintptr_t)insert)){
    insert->nxt=now->nxt;
    now->nxt=insert;
    return;
  }
}
static inline void * kalloc_rest(size_t size){
  size+=sizeof(mem_tag);
  free_list * ret=NULL;
  spin_lock(&lock_rest);
  for(uintptr_t i=8192,j=0;i<=MAX_malloc<<1;i<<=1,++j){
    if(i<size||!start_of_rest[j]) continue;
    ret=start_of_rest[j];
    start_of_rest[j]=start_of_rest[j]->nxt;
    #ifdef TEST
    unsigned long jj=0;
    for(unsigned char * ptr=((unsigned char *)ret)+sizeof(free_list);jj<i-sizeof(free_list);++ptr,++jj) {
      if(*ptr!=MAGIC_UNUSED) printf("%p-%lx,size=%ld\n",ptr,jj,size);
      assert(*ptr==MAGIC_UNUSED);
    }
    #endif
    for(;(i>>1)>=size;i>>=1){
      ret->size>>=1;
      free_list * divide=(free_list *)((uintptr_t)ret+ret->size);
      divide->size=(i>>1);
      insert(divide,&start_of_rest[--j]);
    }
    #ifdef TEST
    memset(ret,MAGIC_USED,i);
    #endif
    MTG_addr(ret,i)->size=i;MTG_addr(ret,i)->magic=MAGIC_MTG;
    break;
  }
  spin_unlock(&lock_rest);
  return (void *)ret;
}
static inline free_list * update(free_list ** head){
  if(*head==NULL) return NULL;
  uintptr_t len=(*head)->size;
  if(len==MAX_malloc) return NULL;
  if(LOWBIT((uintptr_t)*head)>len&&(uintptr_t)*head+len==(uintptr_t)((*head)->nxt)){
    free_list * ret=*head;
    *head=(*head)->nxt->nxt;
    ret->size=(len<<1);
    #ifdef TEST
    memset((void *)ret->nxt,MAGIC_UNUSED,sizeof(free_list));
    int jj=0;
    for(unsigned char *x=((unsigned char *)ret+sizeof(free_list));jj<(len<<1)-sizeof(free_list);++jj,++x) {
      if(*x!=MAGIC_UNUSED) printf("%p-%lx\n",x,jj+sizeof(free_list));
      assert(*x==MAGIC_UNUSED);
    }
    #endif
    return ret;
  }
  for(free_list * now=*head;now->nxt!=NULL;now=now->nxt){
    free_list * next=now->nxt;
    if(LOWBIT((uintptr_t)next)>len&&(uintptr_t)next+len==(uintptr_t)(next->nxt)){
      now->nxt=next->nxt->nxt;
      next->size=len<<1;
      #ifdef TEST
      memset(next->nxt,MAGIC_UNUSED,sizeof(free_list));
      #endif
      return next;
    }
  }
  return NULL;
}
static inline void kfree_rest(void * ptr){
  uintptr_t len=LOWBIT((uintptr_t)ptr);
  for(;len;len>>=1){
    if((uintptr_t)ptr+len<=heap_rest_end&&MTG_addr(ptr,len)->magic==MAGIC_MTG&&MTG_addr(ptr,len)->size==len) break;
  }
  #ifdef TEST
  memset((void *)ptr,MAGIC_UNUSED,len);
  #endif
  int pos=0;
  for(uintptr_t i=8192;i<len;i<<=1) ++pos;
  ((free_list *)ptr)->size=len;
  spin_lock(&lock_rest);
  insert(ptr,&start_of_rest[pos]);
  while((ptr=update(&start_of_rest[pos]))!=NULL) insert(ptr,&start_of_rest[++pos]);
  spin_unlock(&lock_rest);
}

void init_mm(){
  sbrk_now=HEAP_END;
  #ifdef TEST
  memset((void *)HEAP_START,MAGIC_UNUSED,HEAP_END-HEAP_START);
  #endif
  init_128();init_4096();init_rest();
}

#ifndef TEST
static void pmm_init() {
  uintptr_t pmsize = ((uintptr_t)heap.end - (uintptr_t)heap.start);
  printf("Got %d MiB heap: [%p, %p)\n", pmsize >> 20, heap.start, heap.end);
  init_mm();
  return;
}
#else
extern FILE * fd;
#define HEAP_SIZE 128*1024*1024
static void pmm_init() {
  char *ptr  = malloc(HEAP_SIZE);
  heap.start = ptr;
  heap.end   = ptr + HEAP_SIZE;
  printf("Got %d MiB heap: [%p, %p)\n", HEAP_SIZE >> 20, heap.start, heap.end);
  fprintf(fd,"Got %d MiB heap: [%p, %p)\n", HEAP_SIZE >> 20, heap.start, heap.end);
  init_mm();
  printf("num:%lx\n",total_num);
  printf("128:[%lx,%lx) 4096:[%lx,%lx) rest: [%lx,%lx)\n",heap_128_start,heap_128_end,heap_4096_start,heap_4096_end,heap_rest_start,heap_rest_end);
//  printf("Initialize memory Completed!\n");
  return;
}
#endif

static void * kalloc(size_t size){
  if(size>MAX_malloc) return NULL;
  if(size<=128) return kalloc_128();
  if(size<=4096) return kalloc_4096();
  return kalloc_rest(size);
}

static void kfree(void * ptr){
  if((uintptr_t)ptr>=heap_128_start&&(uintptr_t)ptr<heap_128_end) kfree_128(ptr);
  if((uintptr_t)ptr>=heap_4096_start&&(uintptr_t)ptr<heap_4096_end) kfree_4096(ptr);
  if((uintptr_t)ptr>=heap_rest_start&&(uintptr_t)ptr<heap_rest_end) kfree_rest(ptr);
}

MODULE_DEF(pmm) = {
  .init  = pmm_init,
  .alloc = kalloc,
  .free  = kfree,
};


/*  num_of_block=(HEAP_END-HEAP_START)/Unit_size;
  for(uintptr_t i=HEAP_START,j=0;i<HEAP_END;i+=Unit_size,j++){
//    printf("%lx,%lx,i=%lx,j=%d,num=%d\n",HEAP_USE_START,HEAP_END,i,j,num_of_block);
    *lock_addr(j)=MAGIC_LOCKED;
    start_of_free_list(j)=(free_list *)i;
    memset((void *)i,MAGIC_UNUSED,Unit_size);
    ((free_list *)i)->size=Unit_size;
    ((free_list *)i)->nxt=NULL;
    *lock_addr(j)=MAGIC_UNLOCKED;
  }*/

/*  size=up_bound(size+sizeof(mem_tag));
  uintptr_t try_num=0;
  for(uintptr_t i=0,j=HEAP_START;try_num<num_of_block;i=(i+1)%num_of_block,j=(j+Unit_size==HEAP_END?HEAP_START:j+HEAP_END))
  if(atomic_xchg(lock_addr(i),MAGIC_LOCKED)==MAGIC_UNLOCKED){
    ++try_num;
    void * ret=find(size);
    if(ret) work(ret,size);
    spin_unlock(lock_addr(i));
    if(ret) return ret;
  }
  return NULL;*/

/*  uintptr_t pos=(ROUNDDOWN((uintptr_t)ptr,Unit_size)-HEAP_START)/Unit_size;
  spin_lock(lock_addr(pos));
  real_free((uintptr_t)ptr);
  spin_unlock(lock_addr(pos));*/

  /*uintptr_t len=LOWBIT(ptr);
  for(;len;len>>=1){
    uintptr_t pos=ptr+len-sizeof(mem_tag);
    if(((mem_tag *)pos)->size+ptr==pos+sizeof(mem_tag)&&((mem_tag *)pos)->magic==MAGIC_MHD) break;
  }
  memset((void *)ptr,len,MAGIC_UNUSED);*/
