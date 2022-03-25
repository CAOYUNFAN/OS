#include <common.h>

static uintptr_t top;
static int lock;

typedef int spinlock_t;

#define MAGIC_UNLOCKED (0)
#define MAGIC_LOCKED (1)
#define LOWBIT(x) ((x)&((x)^((x)-1)))

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

void init_mm(){
  top=(uintptr_t)heap.start;
  lock=MAGIC_UNLOCKED;
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
#define HEAP_SIZE (512*1024*1024+Unit_size)
static void pmm_init() {
  char *ptr  = malloc(HEAP_SIZE);
  heap.start = ptr;
  heap.end   = ptr + HEAP_SIZE;
  printf("Got %d MiB heap: [%p, %p)\n", HEAP_SIZE >> 20, heap.start, heap.end);
  printf("free_list size=%ld,mem_tag size=%d\n",sizeof(free_list),sizeof(mem_tag));
  fprintf(fd,"Got %d Byte heap: [%p, %p)\n", HEAP_SIZE , heap.start, heap.end);
  init_mm();
//  printf("Initialize memory Completed!\n");
  return;
}
#endif


static void * kalloc(size_t size){
  uintptr_t ret=top;
  spin_lock(&lock);
  while (ret<(uintptr_t)heap.end&&LOWBIT(ret)<size) ret+=LOWBIT(ret);
  if(ret+size>(uintptr_t)heap.end) ret=0;
  else top=ret+size;
  spin_unlock(&lock);
  return (void *) ret;
}

static void kfree(void * ptr){
}

MODULE_DEF(pmm) = {
  .init  = pmm_init,
  .alloc = kalloc,
  .free  = kfree,
};
