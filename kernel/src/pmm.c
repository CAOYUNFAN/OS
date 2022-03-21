#include <common.h>

#define MAX_malloc (16*1024*1024)
#define Unit_size (MAX_malloc<<3)
#define Unit_mask (-Unit_size)

#define MAGIC_UNUSED (0x7c)
#define MAGIC_UNLOCKED (0)
#define MAGIC_LOCKED (1)
#define MAGIC_MHD (0x19810114)

#define LOCK_ADDR(x) ((x)+Unit_size-sizeof(int))

typedef int spinlock_t;

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

static int lock;

#ifndef TEST
static void pmm_init() {
  uintptr_t pmsize = ((uintptr_t)heap.end - (uintptr_t)heap.start);
  printf("Got %d MiB heap: [%p, %p)\n", pmsize >> 20, heap.start, heap.end);

//  init_mm(begin=(((uintptr_t)heap.end-Unit_size+1)&Unit_mask)+Unit_size,end=(uintptr_t)heap.end&Unit_mask);
  return;
}
#else
#define HEAP_SIZE 0x40000000
static void pmm_init() {
  char *ptr  = malloc(HEAP_SIZE);
  heap.start = ptr;
  heap.end   = ptr + HEAP_SIZE;
  printf("Got %d MiB heap: [%p, %p)\n", HEAP_SIZE >> 20, heap.start, heap.end);

//  init_mm(begin=(((uintptr_t)heap.end-Unit_size+1)&Unit_mask)+Unit_size,end=(uintptr_t)heap.end&Unit_mask);
  return;
}
#endif

static void * kalloc(size_t size){
  if(size>MAX_malloc) return NULL;
  spin_lock(&lock);
  spin_unlock(&lock);
  return NULL;
}

static void kfree(void * ptr){
  spin_lock(&lock);

  spin_unlock(&lock);
}

MODULE_DEF(pmm) = {
  .init  = pmm_init,
  .alloc = kalloc,
  .free  = kfree,
};