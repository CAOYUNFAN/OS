#include <os.h>
#include <syscall.h>

#include "initcode.inc"
#include "uproc.h"

static void uproc_init(){
    return;
}

static int uproc_kputc(task_t * task,char ch){
    putch(ch); // safe for qemu even if not lock-protected
    return 0;
}

static int uproc_fork(task_t *task){
    return -1;
}

static int uproc_wait(task_t * task,int * status){
    return -1;
}

static int uproc_exit(task_t *task, int status){
    return -1;
}

static int uproc_kill(task_t *task, int pid){
    return -1;
}

static void * uproc_mmap(task_t *task, void *addr, int length, int prot, int flags){
    return NULL;
}

static int uproc_getpid(task_t *task){
    return -1;
}

static int uproc_sleep(task_t *task, int seconds){
    return -1;
}
  
static int64_t uproc_uptime(task_t *task){
    return 0;
}

MODULE_DEF(uproc) = {
    UPROC_NAME(init)
    UPROC_NAME(kputc)
    UPROC_NAME(fork)
    UPROC_NAME(wait)
    UPROC_NAME(exit)
    UPROC_NAME(kill)
    UPROC_NAME(mmap)
    UPROC_NAME(getpid)
    UPROC_NAME(uptime)
};
