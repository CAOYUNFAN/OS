#include <os.h>
#include <syscall.h>

#include "uproc.h"

extern int create_all(task_t * task, const char * name, void (*entry)(void * arg), void * arg, Context * ctx);
static inline void lock_inside_ker(int * addr){
    while (1) {
        if(atomic_xchg(addr,1)==0) {
            iset(false);
            return;
        }
    }
}
static inline void unlock_inside_ker(int * addr){
    atomic_xchg(addr,0);
}
static inline void lock_inside(int * addr,int * status){
    *status=ienabled();
    lock_inside_ker(addr);
}
static inline void unlock_inside(int * addr,int status){
    unlock_inside_ker(addr);
    if(status) iset(true);
}

counter * add_cnt(counter * cnt){
    if(cnt==NULL){
        cnt=pmm->alloc(sizeof(counter));
        cnt->lock=cnt->cnt=0;
    }
    int i=0; lock_inside(&cnt->lock,&i);
    cnt->cnt++;
    unlock_inside(&cnt->lock,i);
    return cnt;
}
counter * dec_cnt(counter * cnt){
    int i=0; lock_inside(&cnt->lock,&i);
    cnt->cnt--;
    if(cnt->cnt){
        unlock_inside(&cnt->lock,i);
    }else pmm->free(cnt);
    return NULL;
}

void add_pg(pgs ** all,void * va,void * pa,int prot,int shared,counter * cnt){ Log("va %p -> pa %p",va,pa);
    assert(cnt==NULL||pa!=NULL);
    assert(cnt==NULL||!shared);
    pgs * now=pmm->alloc(sizeof(pgs));
    now->pa=pa;
    if(pa==NULL) now->va=(void *)((uintptr_t)va | prot | (shared << 8));
    else now->va=(void *)((uintptr_t)va | prot | (shared << 8) | 16);
    if(cnt) now->cnt=add_cnt(cnt);
    else now->cnt=NULL;
    now->nxt=*all;*all=now;
}
void del_pg(pgs ** all,AddrSpace * as){
    assert(all&&*all);
    pgs * now=*all;*all = now->nxt;
    if(real(now->va)){
        map(as,get_vaddr(now->va),NULL,MMAP_NONE);
        if(now->cnt) now->cnt=dec_cnt(now->cnt);
    }
    if(!now->cnt) pmm->free(now->pa);
    pmm->free(now);
}

void uproc_clear_space(utaskk * ut){
    while (ut->start) del_pg(&ut->start,&ut->as);    
    unprotect(&ut->as);
    return;
}

static inline uint64_t get_time_us(){
    return io_read(AM_TIMER_UPTIME).us;
}

static int uproc_kputc(task_t * task,char ch){
    putch(ch); // safe for qemu even if not lock-protected
    return 0;
}

static int uproc_fork(task_t *task){
    Assert(task->nc==1,"%s multicall of fork",task->name);
    task_t * task_new=(task_t *)pmm->alloc(sizeof(task_t));
    AddrSpace * as=&task_new->utask.as;pgs ** all=&task_new->utask.start;
    protect(as);all=NULL;
    for(pgs * now=task->utask.start;now;now=now->nxt){
        if(!real(now->va)) now->va=(void *)((uintptr_t)now->va | 16), now->pa=pmm->alloc(4096);
        if(!now->cnt) now->cnt=add_cnt(NULL);
        void * va=get_vaddr(now->va), *pa=now->pa;
        int prot=get_prot(now->va);
        if(is_shared(now->va)){
            add_pg(all,va,pa,prot,1 ,now->cnt);
            map(as,va,pa,prot);
        }else{
            add_pg(all,va,pa,prot,0 ,now->cnt);
            if(prot & PROT_WRITE) prot-=PROT_WRITE;
            map(as,va,pa,prot);
            map(&task->utask.as,va,pa,prot);
        }
    }

    task_new->stack=pmm->alloc(16*4096);
    Area temp;
    temp.start=task_new->stack;temp.end=(void *)((uintptr_t)task_new->stack+16*4096);
    Context * ctx2=ucontext(&task_new->utask.as,temp,task_new->utask.as.area.start);
    uintptr_t rsp0=ctx2->rsp0;
    void * cr3=ctx2->cr3;
    memcpy(ctx2,task->ctx[0],sizeof(Context));
    ctx2->rsp0=rsp0;
    ctx2->cr3=cr3;
    ctx2->GPRx=0;

    #ifdef LOCAL
    char * ch="FORK!";
    #else
    char * ch=NULL;
    #endif
    return create_all(task_new,ch,NULL,NULL,ctx2);
}

static int uproc_wait(task_t * task,int * status){
    Assert(task==current_all[cpu_current()],"unexpected task %s",task->name);
    Assert(task->status==TASK_RUNNING&&task->lock,"Unexpected current %s,status %d, lock %d",task->name,task->status,task->lock);
    if(!task->ch)return -1;
    while (1) {
        task_t ** pre=&task->ch;
        for(task_t * temp=task->ch;temp;pre=&temp->bro,temp=temp->bro) if(temp->status==TASK_DEAD){
            if(status) *status=temp->ret;
            *pre=temp->bro;
            return 0;
        }
        yield();
    }
    Assert(0,"%s should not reach here",task->name);
    return 0;
}

static int uproc_exit(task_t *task, int status){
    Assert(task==current_all[cpu_current()],"unexpected task %s",task->name);
    Assert(task->status==TASK_RUNNING&&task->lock,"Unexpected current %s,status %d, lock %d",task->name,task->status,task->lock);
    task->ret=status;
    kmt->teardown(task);
    yield();
    Assert(0,"SHOULD NOT REACH HERE! %s",task->name);
    return 0;
}

static int uproc_kill(task_t *task, int pid){
    if(pid<0) pid =-pid;
    Assert(task==current_all[cpu_current()],"unexpected task %s",task->name);
    Assert(task->status==TASK_RUNNING&&task->lock,"Unexpected current %s,status %d, lock %d",task->name,task->status,task->lock);
    Assert(task_all_pid[pid],"pid %d should not be NULL!",pid);
    kmt->teardown(task_all_pid[pid]);
    return 0;
}

static void * uproc_mmap(task_t *task, void *addr, int length, int prot, int flags){
    AddrSpace * as=&task->utask.as;int pgsize=as->pgsize;
    if(flags==MAP_UNMAP){
        uintptr_t vaddr=(uintptr_t)addr;
        pgs ** pre=&task->utask.start; pgs * now=*pre;
        while (now){
            uintptr_t temp=(uintptr_t)get_vaddr(now->va);
            if(temp>=vaddr&&temp<vaddr+length){
                del_pg(pre,as);
                now=*pre;
            }else pre=&now->nxt,now=now->nxt;
        }
        return NULL;
    }else{
        char * vaddr=(char *)((uintptr_t)addr & (-4096L));
        int flag=0;uintptr_t maxn=0;
        for(pgs * now=task->utask.start;now;now=now->nxt){
            if((uintptr_t)now->va>=(uintptr_t)vaddr&&(uintptr_t)now->va<(uintptr_t)vaddr+length) flag=1;
            if((uintptr_t)now->va > maxn) maxn= (uintptr_t) now->va + 4096;
        }
        if(flag || !vaddr) vaddr=(char *)maxn;
        addr=vaddr;
        for(;length>0;length-=pgsize,vaddr+=pgsize) add_pg(&task->utask.start,vaddr,NULL,prot,flags==MAP_SHARED,NULL);
    } 
    return addr;
}

static int uproc_getpid(task_t *task){
    return task->pid;
}

static int uproc_sleep(task_t *task, int seconds){
    Assert(task->lock==1,"Name %s do not hlod the lock!",task->name);
    uint64_t awake=get_time_us()+(uint64_t)seconds*1000000uLL;
    while(get_time_us()<awake) yield();
    return 0;
}
  
static int64_t uproc_uptime(task_t *task){
    return (int64_t)(get_time_us()/1000uLL);
}

static void * pgalloc(int len){return pmm->alloc((size_t)len);}
static void uproc_init(){
    vme_init(pgalloc, pmm->free);
    task_t * task=pmm->alloc(sizeof(task_t));
    protect(&task->utask.as);task->utask.start=NULL;
    assert(task->utask.as.pgsize==4096);
    Log("%lx %lx %lx",&(task->utask.as),task->utask.as.area.start,task->utask.as.area.end);
    void * vaddr=uproc_mmap(task,task->utask.as.area.start,_init_len, PROT_READ | PROT_WRITE,MAP_PRIVATE);
    for(pgs * now=task->utask.start;now;now=now->nxt) if((uintptr_t)now->va >= (uintptr_t) vaddr && (uintptr_t) now->va < (uintptr_t) vaddr + _init_len){
        now->va =(void *)((uintptr_t)now->va | 16L);
        uintptr_t offset = (uintptr_t) now->va - (uintptr_t) vaddr, len= 4096;
        if(offset+len>_init_len) len=_init_len-offset;
        now->pa = pmm->alloc(4096);
        memcpy(now->pa,_init+offset,len);
    }
    task->stack=pmm->alloc(16*4096);
    Area temp;
    temp.start=task->stack;temp.end=(void *)((uintptr_t)task->stack+16*4096);    
    Context * ctx=ucontext(&task->utask.as,temp,task->utask.as.area.start);
    create_all(task,"first_uproc",NULL,NULL,ctx);
    return;
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
    UPROC_NAME(sleep)
    UPROC_NAME(uptime)
};

#define NAME_RELATION(name,...) \
    case SYS_ ## name : ctx->GPRx = (uintptr_t) uproc -> name (task , ## __VA_ARGS__); break;

Context * syscall(task_t * task,Context * ctx){
    iset(true);
    switch (ctx->GPRx) {
        NAME_RELATION(kputc,ctx->GPR1)
        NAME_RELATION(fork)
        NAME_RELATION(wait,(int *)ctx->GPR1)
        NAME_RELATION(exit,ctx->GPR1)
        NAME_RELATION(kill,ctx->GPR1)
        NAME_RELATION(mmap,(void *)ctx->GPR1,ctx->GPR2,ctx->GPR3,ctx->GPR4)
        NAME_RELATION(sleep,ctx->GPR1)
        NAME_RELATION(getpid)
        NAME_RELATION(uptime)   
        default: ctx->GPRx=-1; break;
    }
    iset(false);
    return ctx;
}

void pagefault_handler(void * va,int prot,task_t * task){
    AddrSpace * as=&task->utask.as;pgs * now=task->utask.start;
    while(now&&get_vaddr(now->va)!=va) now=now->nxt;
    Assert(now&& (get_prot(now->va) & prot)==prot,"%s addr %p do not exist!",task->name,va);
    if(!real(now->va)){
        Assert(now->pa==NULL&&now->cnt==NULL,"%s unexpected page states!",task->name);
        now->pa=pmm->alloc(4096);
        map(as,now->va,now->pa,get_prot(now->va));
        now->va = (void *)((uintptr_t) now->va | 16L);
    }else{
        Assert(now->pa && now->cnt,"%s unexpected page states!",task->name);
        void * pa_old=now->pa;
        int i=0;lock_inside(&now->cnt->lock,&i);
        now->cnt->cnt--;
        if(now->cnt->cnt){
            now->pa=pmm->alloc(4096);
            memcpy(now->pa,pa_old,4096);
            unlock_inside(&now->cnt->lock,i);
        }else{
            now->pa=pa_old;
            free(now->cnt);
        }
        now->cnt=NULL;
        map(as,get_vaddr(now->va),now->pa,get_prot(now->va));
    }
    return;
}