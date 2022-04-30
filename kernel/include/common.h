#include <kernel.h>
#include <klib.h>
#include <klib-macros.h>

#ifdef LOCAL
#define _Log(...) \
  do { \
    printf(__VA_ARGS__); \
    log_write(__VA_ARGS__); \
  } while (0)

#define Log(fmt,...) \
do{\
    _Log("[%s:%s,%d]" fmt "\n",__FILE__, __LINE__, __func__, ## __VA_ARGS__);
}while (0)

#define Assert(cond,fmt,...)\
do{\
    if(!(cond)) Log(fmt,__VA_ARGS__);\
    assert(cond);\
} while (0)

#else
#define Log(fmt,...) ((void)(0))
#define Assert(cond,fmt,...) ((void)(0))
#endif
    

enum task_status{
  TASK_RUNNING,TASK_RUNABLE,TASK_WAITING,TASK_DEAD
}

typedef struct{
    task_t * head;
    int lock;
}list_head;

struct task {
  int status;
  task * nxt;
  Context ctx;
};

struct spinlock {
  int used;
  list_head head;
};

struct semaphore {
  // TODO
};
