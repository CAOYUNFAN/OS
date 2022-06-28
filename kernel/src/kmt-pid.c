#include <os.h>
#include "kmt.h"

static int pid_lock,pid_lock2,pid_max;
task_t * task_all_pid[32768];
typedef struct pid_unit__{
    int data;
    struct pid_unit__ * nxt;
}pid_unit;
pid_unit * pid_start, * pid_end;

int new_pid(){
    int i=0,ret;
    pid_unit * temp=NULL;
    lock_inside(&pid_lock,&i);
    if(pid_max<32768) ret=++pid_max;
    else{
        lock_inside_ker(&pid_lock2);
        panic_on(pid_start==NULL,"Too many procedures!");
        ret=pid_start->data;
        temp=pid_start;
        pid_start=pid_start->nxt;
        if(!pid_start) pid_end=NULL;
        unlock_inside_ker(&pid_lock2);
    }
    unlock_inside(&pid_lock,i);
    if(temp) pmm->free(temp);
    return ret;
}

void pid_free(int pid){
    pid_unit * temp=(pid_unit *)pmm->alloc(sizeof(pid_unit));
    temp->data=pid;temp->nxt=NULL;
    int i=0;
    lock_inside(&pid_lock2,&i);
    if(pid_start) pid_start=temp;
    else{
        Assert(pid_end,"%s SHOULD NOT BE NULL!","queue of pid");
        pid_end->nxt=temp;
    }
    pid_end=temp;
    unlock_inside(&pid_lock2,i);
    return;
}
