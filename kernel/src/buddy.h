void * kernel_alloc(size_t size);

typedef struct{
    int size;
    int longest[1];
}buddy;

#define lch(index) ((index)*2+1)
#define rch(index) ((index)*2+2)
#define fa(index) (((index)+1)/2-1)

buddy * buddy_init(size_t size);

int buddy_alloc(buddy * self,int size);

void buddy_free(buddy * self,int offset);