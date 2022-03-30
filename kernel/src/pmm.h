#include <common.h>

//DEBUG ralted
#ifdef TEST
  #include <assert.h>
  #include <stdio.h>
  #define DEBUG(...) __VA_ARGS__
  #define Assert(cond,format,...)\
    ((void) sizeof ((cond) ? 1 : 0),__extension__({if(!(cond)){\
        fprintf(stderr,format,__VA_ARGS__);\
        assert(cond);\
    }}))
  #define MAGIC_UNUSED (0x7f)
  #define MAGIC_BIG (0x5d)
  #define MAGIC_SMALL (0x3b)
#else
  #define DEBUG(...) 
  #define Assert(...) ((void)0)
#endif

//macros that may be used
#define LOWBIT(x) ((x)&((x)^((x)-1)))
#define Max(a,b) ((a)>(b)?(a):(b))
#define __contact(x,y) x##y
#define contact(x,y) __contact(x,y)

//heap_size related
extern uintptr_t kernel_max;
void * kernel_alloc(size_t size);
#define MAX_alloc (16*1024*1024)
#define Unit_size (4096)
#define HEAP_START ((uintptr_t)heap.start)
#define HEAP_END ((uintptr_t)heap.end)
#define HEAP_OFFSET_START ROUNDDOWN(HEAP_START,MAX_alloc)
#define NR_NUM(size) (size==4096?1:(Unit_size-sizeof(free_list))/size)

//lock_realated
#define MAGIC_UNLOCKED (0)
#define MAGIC_LOCKED (1)
typedef int spinlock_t;
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

//buddy system related
typedef union{
  unsigned int size;
  unsigned int longest[1];
}buddy;
buddy * buddy_init(size_t size);
void * buddy_alloc(buddy * self,size_t size);
void buddy_free(buddy * self,void * ptr);
unsigned char is_block(buddy * self,size_t offset);

//slub? related
typedef struct free_list_t{
  struct free_list_t *nxt;
}free_list;
typedef struct block_t{
  struct block_t * nxt;
}block;
typedef struct {
  int size;
}block_info;


//top
typedef struct {
  spinlock_t lock;
  free_list * head;
  int nr_num;
}start_info;
typedef struct {
  spinlock_t lock;
  block * start;
}start_info_all;
typedef struct rubbish_block_t{
  free_list * start;
  free_list * tail;
  struct rubbish_block_t * nxt;
}rubbish_block;
typedef struct{
  spinlock_t lock;
  rubbish_block * first;
}start_info_rubbish;
