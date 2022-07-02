#include <os.h>
#include <syscall.h>

#include "uproc.h"

extern int create_all(task_t * task, const char * name, Context * ctx);
extern Area make_stack(task_t * task);
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

int vme_lock=0;
static inline void map_safe(AddrSpace * as,void * va,void * pa,int prot){
    int i=ienabled();iset(0);
    map(as,va,pa,prot);
    if(i) iset(1);
}
static inline void protect_safe(AddrSpace * as){
    int i=ienabled();iset(0);
    protect(as);
    if(i) iset(1);
}
static inline Context * ucontext_safe(AddrSpace *as, Area kstack, void *entry){
    int i=ienabled();iset(0);
    Context * ctx=ucontext(as,kstack,entry);
    if(i) iset(1);
    return ctx;
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
        return cnt;
    }pmm->free(cnt);
    return NULL;
}

void add_pg(pgs ** all,void * va,void * pa,int prot,int shared,counter * cnt){ 
    assert(cnt==NULL||pa!=NULL);
    assert(cnt==NULL||!shared);
    pgs * now=pmm->alloc(sizeof(pgs));
    now->pa=pa;
    if(pa==NULL) now->va=(void *)((uintptr_t)va | prot | (shared << 3));
    else now->va=(void *)((uintptr_t)va | prot | (shared << 3) | 16);
    if(cnt) now->cnt=add_cnt(cnt);
    else now->cnt=NULL;
    Log("va %p -> pa %p",now->va,pa);
    now->nxt=*all;*all=now;
}
void del_pg(pgs ** all,AddrSpace * as){
    assert(all&&*all);
    pgs * now=*all;*all = now->nxt;
    if(real(now->va)){
        map_safe(as,get_vaddr(now->va),NULL,MMAP_NONE);
        if(now->cnt) Log("%d",now->cnt->cnt);
        if(now->cnt) now->cnt=dec_cnt(now->cnt);
    }
    if(!now->cnt) pmm->free(now->pa);
    pmm->free(now);
}

vpage_len * add_page_2(vpage_len * nxt,void * addr,int len){
    vpage_len * unit=pmm->alloc(sizeof(vpage_len));
    unit->addr=addr;unit->len=len;
    unit->nxt=nxt;return unit;
}
void add_page_len(len_list * list,void * addr,int len){
    if(!list->start||(uintptr_t)addr>(uintptr_t)list->start->addr){
        list->start=add_page_2(list->start,addr,len);
        return;
    }
    vpage_len * pre=list->start;
    for(vpage_len * now=pre->nxt;now&&(uintptr_t)addr<(uintptr_t)now->addr;now=now->nxt) pre=now;
    Assert(pre&&(uintptr_t)pre->addr>=(uintptr_t)addr+len&&(pre->nxt==NULL||(uintptr_t)pre->nxt->addr+pre->nxt->len<=(uintptr_t)addr),"Unexpected lines! %p",addr);
    pre->nxt=add_page_2(pre->nxt,addr,len);
    return;
}
void * find_check_add(len_list * list,void * addr,int len){
    Assert(list->start&&list->start->nxt,"init not completed! %p",addr);
    void * ret=NULL;vpage_len * to_be_added=NULL;
    for(vpage_len * pre=list->start, * now=list->start->nxt;now&&(uintptr_t)now->addr+now->len>=(uintptr_t)addr;pre=now,now=now->nxt){
        Assert(pre->nxt==now,"invalid status %d",0);
        uintptr_t ll=(uintptr_t)now->addr+now->len,rr=(uintptr_t)pre->addr;
        if((uintptr_t)addr>=ll && (uintptr_t)addr+len<=rr){
            pre->nxt=add_page_2(now,addr,len);
            return addr; 
        } 
        if(rr-ll>=len) ret=(void *)(rr-len),to_be_added=pre;
    }
    Assert((uintptr_t)ret>=(uintptr_t)addr,"unexpected ret %p addr %p",ret,addr);
    to_be_added->nxt=add_page_2(to_be_added->nxt,ret,len);
    return ret;
}
void split_page(len_list * list,void * addr,int len_all){
    vpage_len * now=list->start;vpage_len ** pre=&list->start;
    uintptr_t st_all=(uintptr_t)addr,ed_all=(uintptr_t)addr+len_all;
    int left=len_all;
    for(;now&&left;pre=&now->nxt,now=now->nxt){
        uintptr_t ll=(uintptr_t)now->addr,rr=(uintptr_t)now->addr+now->len;
        if(rr>st_all&&ll<ed_all){
            #define Max(a,b) ((a)>(b)?(a):(b))
            #define Min(a,b) ((a)<(b)?(a):(b))
            uintptr_t st=Max(st_all,ll),ed=Min(ed_all,rr);
            int len=ed-st;len_all-=len;
            if(ll==st&&rr==ed){
                *pre=now->nxt;
                pmm->free(now);
                continue;
            }
            if(ll==st||rr==ed){
                now->len-=len;
                if(ll==st) now->addr=(void *)ed;
                continue;
            }
            now->addr=(void *)ed;now->len=rr-ed;
            vpage_len * new=pmm->alloc(sizeof(vpage_len));
            new->addr=(void *)ll;new->len=ll-st;
            new->nxt=now->nxt;now->nxt=new;
        }
    }
    return;
}

void uproc_clear_space(utaskk * ut){
    while (ut->start) del_pg(&ut->start,&ut->as);
    int i=0;lock_inside(&vme_lock,&i);    
    unprotect(&ut->as);
    unlock_inside(&vme_lock,i);
    vpage_len * now=ut->list.start;
    while (now){
        vpage_len * temp=now;
        now=now->nxt;
        free(temp);
    }
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
    iset(0);
    AddrSpace * as=&task_new->utask.as;pgs ** all=&task_new->utask.start;
    protect_safe(as);*all=NULL;
    for(pgs * now=task->utask.start;now;now=now->nxt){
        Assert(real(now->va)||is_shared(now->va),"%s unexpected status %p",task->name,now->va);
        Log("%p",now->va);
        if(!real(now->va)) {
            now->va=(void *)((uintptr_t)now->va | 16);
            now->pa=pmm->alloc(4096);
            Log("mmap unmaped page %p->%p",now->va,now->pa);
        }
        if(!now->cnt) now->cnt=add_cnt(NULL);
        void * va=get_vaddr(now->va), *pa=now->pa;
        int prot=get_prot(now->va);
        if(is_shared(now->va)){
            add_pg(all,va,pa,prot,1 ,now->cnt);
            map_safe(as,va,pa,prot);
        }else{
            add_pg(all,va,pa,prot,0 ,now->cnt);
            map_safe(as,va,pa,MMAP_READ);
            map_safe(&task->utask.as,va,NULL,MMAP_NONE);
            map_safe(&task->utask.as,va,pa,MMAP_READ);
        }
    }
    for(vpage_len * src=task->utask.list.start, ** dst=&task_new->utask.list.start;src;src=src->nxt){
        *dst=pmm->alloc(sizeof(vpage_len));
        (*dst)->addr=src->addr;(*dst)->len=src->len;
        (*dst)->nxt=NULL;dst=&(*dst)->nxt;
    }
    Context * ctx2=ucontext_safe(as,make_stack(task_new),as->area.start);
    uintptr_t rsp0=ctx2->rsp0;
    void * cr3=ctx2->cr3;
    memcpy(ctx2,task->ctx[0],sizeof(Context));
    ctx2->rsp0=rsp0;
    ctx2->cr3=cr3;
    ctx2->GPRx=0;

    #ifdef LOCAL
    char * ch=pmm->alloc(128);sprintf(ch,"\"fork ch of %d\"",task->pid);
    #else
    char * ch=NULL;
    #endif
    int ret=create_all(task_new,ch,ctx2);
    iset(1);
    return ret;
}

static int uproc_wait(task_t * task,int * status){
    Assert(task==current_all[cpu_current()],"unexpected task %s",task->name);
    Assert(task->status==TASK_RUNNING&&task->lock,"Unexpected current %s,status %d, lock %d",task->name,task->status,task->lock);
    if(!task->ch)return -1;
    while (1) {
        assert(task->lock);
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
    iset(0);
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
        split_page(&task->utask.list,addr,length);
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
        addr=find_check_add(&task->utask.list,(void *)ROUNDUP(addr,4096),ROUNDUP(length,4096));
        if(flags==MAP_SHARED){
            char * vaddr=addr;
            for(;length>0;length-=pgsize,vaddr+=pgsize) add_pg(&task->utask.start,vaddr,NULL,prot,1,NULL);
        }
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
    AddrSpace * as=&task->utask.as;
    protect_safe(as);task->utask.start=NULL;
    assert(as->pgsize==4096);
    char * pa=pmm->alloc(_init_len>4096?_init_len:4096);
    add_page_len(&task->utask.list,as->area.start,ROUNDUP(_init_len,4096));
    add_page_len(&task->utask.list,(void *)((uintptr_t)as->area.end-1024*4096),1024*4096);
    memcpy(pa,_init,_init_len);
    char * va=as->area.start;
    for(int len=0;len<_init_len;len+=4096,pa+=4096,va+=4096){
        add_pg(&task->utask.start,va,pa,PROT_READ|PROT_WRITE,0,NULL);
        map(as,va,pa,MMAP_ALL);
    }
    create_all(task,"first_uproc",ucontext_safe(as,make_stack(task),as->area.start));
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

#define NAME_RELATION(func,...) \
    case SYS_ ## func : Log("%s syscall %s",task->name,#func); ctx->GPRx = (uintptr_t) uproc -> func (task , ## __VA_ARGS__); break;

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
    Log("%s (pid %d) pagefault %p",current_all[cpu_current()]->name,current_all[cpu_current()]->pid,va);
    va=(void *)ROUNDDOWN(va,4096);
    AddrSpace * as=&task->utask.as;pgs * now=task->utask.start;
    Assert((uintptr_t)va>=(uintptr_t)as->area.start&&(uintptr_t)va<(uintptr_t)as->area.end,"Unexpected virtual address %p",va);
    while(now&&get_vaddr(now->va)!=va) now=now->nxt;
    if(!now){
        void * pa=pmm->alloc(4096);
        add_pg(&task->utask.start,va,pa,PROT_READ|PROT_WRITE,0,NULL);
        map_safe(as,va,pa,MMAP_ALL);
    }else if(!real(now->va)){
        Assert(now->pa==NULL&&now->cnt==NULL&&is_shared(now->va),"%s unexpected page states %p!",task->name,now->va);
        Log("add dummy page %s->%s",now->va,now->pa);
        now->pa=pmm->alloc(4096);
        map_safe(as,va,now->pa,MMAP_ALL);
        now->va = (void *)((uintptr_t) now->va | 16L);
    }else{
        Assert(now->pa && now->cnt,"%s unexpected page states!",task->name);
        void * pa_old=now->pa;
        int i=0;lock_inside(&now->cnt->lock,&i);
        now->cnt->cnt--;
        Log("more access %p->%p,%d",now->va,now->pa,now->cnt);
        if(now->cnt->cnt){
            now->pa=pmm->alloc(4096);
            memcpy(now->pa,pa_old,4096);
            unlock_inside(&now->cnt->lock,i);
        }else{
            now->pa=pa_old;
            free(now->cnt);
        }
        now->cnt=NULL;
        map_safe(as,va,NULL,MMAP_NONE);
        map_safe(as,va,now->pa,MMAP_ALL);
    }
    Log("%s pagefault ended!",current_all[cpu_current()]->name);
    return;
}