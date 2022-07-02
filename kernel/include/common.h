#include <kernel.h>
#include <klib.h>
#include <klib-macros.h>

#ifdef LOCAL
#define _Log(...) \
  do { \
    printf(__VA_ARGS__); \
  } while (0)

#define Log(fmt,...) \
do{\
    _Log("[%s:%d,%s]CPU%d:" fmt "\n",__FILE__, __LINE__, __func__, cpu_current(),## __VA_ARGS__);\
}while (0)

#define Assert(cond,fmt,...)\
do{\
    if(!(cond)) Log(fmt,__VA_ARGS__);\
    assert(cond);\
} while (0)
#define NAME const char * name;
#else
#define Log(fmt,...) ((void)(0))
#define Assert(cond,fmt,...) ((void)(0))
#define NAME
#endif

enum task_status{
  TASK_RUNNING,TASK_RUNABLE,TASK_WAITING,TASK_DEAD
};

typedef struct{
  int lock,cnt;
}counter;

#define get_vaddr(va) ((void *)((uintptr_t)va & (-4096L)))
#define get_prot(va) ((uintptr_t)va & 0x7L)
#define is_shared(va) (((uintptr_t)va & 0x8L) >> 3)
#define real(va) ((uintptr_t)va & 0x10L)

typedef struct pgstruct{
  void * va, * pa;
  struct pgstruct * nxt;counter * cnt;
}pgs;

typedef struct _vpage_len{
  void * addr;int len;
  struct _vpage_len * nxt;  
}vpage_len;

typedef struct{
  vpage_len * start;
}len_list;

typedef struct{
  AddrSpace as;
  pgs * start;
  len_list list;
}utaskk;

struct task {
  int status,lock;
  int pid,ret;
//  uint64_t awake_time;
  struct task * nxt, * ch, * bro;
  Context * ctx[2];
  unsigned char nc; 
  void * stack;
  utaskk utask;
  NAME
};

typedef struct{
  struct task * head;
  struct task * tail;
  int lock;
}task_queue;

struct spinlock {
  int lock,used;
  task_queue head;
  int status;
  NAME
};

struct semaphore {
  int lock,num;
  task_queue head;
  NAME
};
