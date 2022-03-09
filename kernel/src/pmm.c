#include <common.h>

#define MAX_malloc (16*1024*1024)

typedef struct{
  int is_cover,max_len,left_len;
}tree_unit;
tree_unit tree[4*MAX_malloc];

static void build_tree(uintptr_t l,uintptr_t r,int now){
  tree[now].is_cover=0;tree[now].max_len=tree[now].left_len=(int)(r-l);
  if(r-l>1){
    uintptr_t mid=(l+r)>>1;
    build_tree(l,mid,now<<1);
    build_tree(mid,r,now<<1|1);
  }
  return;
}

#define Max(a,b) ((a)>(b)?(a):(b))

static inline void update(int now,uintptr_t l,uintptr_t r){
  int ls=now<<1,rs=now<<1|1;
  uintptr_t mid=(l+r)>>1;
  if(tree[ls].max_len==mid-l){
    tree[now].max_len=tree[now].left_len=tree[rs].left_len+mid-l;
  }else{
    tree[now].left_len=tree[ls].left_len;
    tree[now].max_len=Max(tree[ls].max_len,tree[rs].max_len);
  }
  return;
}

static inline void cover(int now,int tag,uintptr_t x){
  tree[now].is_cover=tag;
  tree[now].left_len=tree[now].max_len=x;
  return;
}

static inline void push_down(int now){
  if(tree[now].is_cover){
    tree[now].is_cover=0;
    cover(now<<1,1,0);cover(now<<1|1,1,0);
  }
  return;
}

static void add_tag(int tag,uintptr_t left, uintptr_t right,uintptr_t l,uintptr_t r,int now){
  push_down(now);
  if(left<=l&&right>=r){
    cover(now,tag,tag?0:r-l);
    return;
  }
  uintptr_t mid=(l+r)>>1;
  if(left<mid) add_tag(tag,left,right,l,mid,now<<1);
  if(right>mid) add_tag(tag,left,right,mid,r,now<<1|1); 
  update(now,l,r);
}

#define no_ans ((uintptr_t)-1)

static uintptr_t detect(size_t len,int l,int r,int now){
  if(tree[now].max_len<len) return no_ans;
  push_down(now);
  if(r-l==1) return l;
  uintptr_t mid=(l+r)>>1;
  uintptr_t ans=detect(len,mid,r,now<<1);
  if(ans==no_ans) ans=detect(len,l,mid,now<<1|1);
  return ans==no_ans?l:ans;
}

static uintptr_t tree_l,tree_r;

static int pair[MAX_malloc];

static void *kalloc(size_t size) {
  if(size>MAX_malloc) return NULL;
  uintptr_t l=detect(size,tree_l,tree_r,1);
  if(l==no_ans) return NULL;
  pair[l-tree_l]=size;
  add_tag(1,l,l+size,tree_l,tree_r,1);
  return (void *)l;
}

static void kfree(void *ptr) {
  add_tag(0,(uintptr_t)ptr,(uintptr_t)ptr+pair[(uintptr_t)ptr-tree_l],tree_l,tree_r,1);
  return;
}

static void pmm_init() {
  uintptr_t pmsize = ((uintptr_t)heap.end - (uintptr_t)heap.start);
  printf("Got %d MiB heap: [%p, %p)\n", pmsize >> 20, heap.start, heap.end);
  tree_l=((uintptr_t)heap.start+(uintptr_t)MAX_malloc-1)&-(uintptr_t)MAX_malloc;
  tree_r=tree_l+MAX_malloc;
  build_tree(tree_l,tree_r,1);
}

MODULE_DEF(pmm) = {
  .init  = pmm_init,
  .alloc = kalloc,
  .free  = kfree,
};