#include <common.h>

#ifdef TEST
  #include <assert.h>
  #define DEBUG(...) (__VA_ARGS__)
#else
  #define DEBUG()
#endif


#define MAX_malloc (32*1024*1024)
#define Unit_size (MAX_malloc)
#define Unit_mask (-Unit_size)

#define HEAP_START ROUNDUP((uintptr_t)heap.start,Unit_size)
#define HEAP_END ROUNDDOWN((uintptr_t)heap.end,Unit_size)
#define total_num ((HEAP_END-HEAP_START)/Unit_size)

#define LOWBIT(x) ((x)&((x)^((x)-1)))

#define MAGIC_UNUSED (0x7c)
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

static free_list * start_of_128;
static uintptr_t heap_128_start,heap_128_end;
static int lock_128;
static inline void init_128(){
  heap_128_start=HEAP_START;heap_128_end=HEAP_START+total_num/2*Unit_size;
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
  }
  spin_unlock(&lock_128);
  return ret;
}
static inline void kfree_128(void * ptr){
  free_list * hdr=ptr;
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
  }
  spin_unlock(&lock_4096);
  return ret;
}
static inline void kfree_4096(void * ptr){
  free_list * hdr=ptr;
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
  start_of_rest=kernel_alloc(sizeof(free_list *)*num);
  uintptr_t j=0;
  for(uintptr_t i=8192;i<MAX_malloc;i<<=1,++j) start_of_rest[j]=NULL;
  if(j!=num-1) printf("FAIL!\n");
  start_of_rest[j]=(free_list *)heap_rest_start;
  ((free_list *)heap_rest_start)->size=MAX_malloc;
  for(uintptr_t ptr=heap_rest_start+MAX_malloc;ptr<heap_rest_end;ptr+=MAX_malloc){
    ((free_list *)ptr)->size=MAX_malloc;
    ((free_list *)(ptr-MAX_malloc))->nxt=(free_list *)ptr;
  }
  ((free_list *)(heap_rest_end-MAX_malloc))->nxt=NULL;
}
static inline void insert(free_list * insert,free_list ** head){
  if(*head==NULL||(uintptr_t)*head>(uintptr_t)insert){
    *head=insert;
    insert->nxt=*head;
    return;
  }
  for(free_list * now=*head;now!=NULL;now=now->nxt)
  if((uintptr_t)now<(uintptr_t)insert&&(now->nxt==NULL||(uintptr_t)now->nxt>(uintptr_t)insert)){
    insert->nxt=now->nxt;
    now->nxt=insert;
  }
  return;
}
static inline void * kalloc_rest(size_t size){
  size+=sizeof(mem_tag);
  free_list * ret=NULL;
  spin_lock(&lock_rest);
  for(uintptr_t i=8192,j=0;i<=MAX_malloc;i<<=1,++j){
    if(i<size||!start_of_rest[j]) continue;
    ret=start_of_rest[j];
    start_of_rest[j]=start_of_rest[j]->nxt;
    for(;i>=size;i>>=1){
      ret->size>>=1;
      free_list * divide=(free_list *)((uintptr_t)ret+ret->size);
      divide->size=i;
      insert(divide,&start_of_rest[--j]);
    }
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
  printf("%ld\n",len);
  if(LOWBIT((uintptr_t)*head)>len&&(uintptr_t)*head+len==(uintptr_t)((*head)->nxt)){
    free_list * ret=*head;
    *head=(*head)->nxt->nxt;
    ret->size=(len<<1);memset((void *)ret->nxt,MAGIC_UNUSED,sizeof(free_list));
    return ret;
  }
  for(free_list * now=*head;now->nxt!=NULL;now=now->nxt){
    free_list * next=now->nxt;
    if(LOWBIT((uintptr_t)next)>len&&(uintptr_t)next+len==(uintptr_t)(next->nxt)){
      now->nxt=next->nxt->nxt;
      next->size=len<<1;memset(next->nxt,MAGIC_UNUSED,sizeof(free_list));
      return next;
    }
  }
  return NULL;
}
static inline void kfree_rest(void * ptr){printf("ENTERING free!\n");
  uintptr_t len=LOWBIT((uintptr_t)ptr);
  for(;len;len>>=1){printf("%p+%lx=%lx\n",ptr,len,(uintptr_t)ptr+len);
    if(MTG_addr(ptr,len)->magic==MAGIC_MTG&&MTG_addr(ptr,len)->size==len) break;
  }
  memset((void *)ptr,len,MAGIC_UNUSED);
  int pos=0;
  for(uintptr_t i=8192;i<len;i<<=1) ++pos;
  spin_lock(&lock_rest);
  insert(ptr,&start_of_rest[pos]);
  while((ptr=update(&start_of_rest[pos]))!=NULL) insert(ptr,&start_of_rest[++pos]),printf("pos=%d\n",pos);
  spin_unlock(&lock_rest);
}

void init_mm(){
  sbrk_now=HEAP_END;
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
#define HEAP_SIZE 0x40000000
static void pmm_init() {
  char *ptr  = malloc(HEAP_SIZE);
  heap.start = ptr;
  heap.end   = ptr + HEAP_SIZE;
  printf("Got %d MiB heap: [%p, %p)\n", HEAP_SIZE >> 20, heap.start, heap.end);
  init_mm();
//  printf("Initialize memory Completed!\n");
  return;
}
#endif

static void * kalloc(size_t size){
  if(size>(MAX_malloc)>>1) return NULL;
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
