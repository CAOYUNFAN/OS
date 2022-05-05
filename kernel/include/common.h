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

struct task {
  int status,lock;
  struct task * nxt;
  Context * ctx;
  void * stack;
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
