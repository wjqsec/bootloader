
#define MALLOC_MAGIC 0xdeadbeefdeadbeef
#define MAXNUM_RECORDINGS 500
struct malloc_recording
{
    void *p;
    unsigned int size;
};
static struct malloc_recording recordings[MAXNUM_RECORDINGS];

static void check_malloc()
{
    int i;
    for(i = 0 ; i < MAXNUM_RECORDINGS; i++)
    {
        if(recordings[i].p != 0)
        {
            if(*(uint64_t*)((char *)recordings[i].p + recordings[i].size) != MALLOC_MAGIC)
                kAFL_hypercall(HYPERCALL_KAFL_PANIC, 0);
        }
    }
}

static void insert_malloc_recording(void *ptr, unsigned int size)
{
    int i;
    for(i = 0 ; i < MAXNUM_RECORDINGS; i++)
    {
        if(recordings[i].p == 0)
        {
            recordings[i].p = ptr;
            recordings[i].size = size - 8;
            *(uint64_t*)((char*)ptr + size - 8) = MALLOC_MAGIC;
            break;
        }
    }
}



static void delete_malloc_recording(void *ptr)
{
    int i;
    boot found = false;
    for(i = 0 ; i < MAXNUM_RECORDINGS; i++)
    {
        if(recordings[i].p == ptr)
        {
            if(*(uint64_t*)((char *)recordings[i].p + recordings[i].size) != MALLOC_MAGIC)
                kAFL_hypercall(HYPERCALL_KAFL_PANIC, 0);
            recordings[i].p = 0;
            recordings[i].size = 0;
            found = true;
            break;
        }
    }
    if(!found)
        kAFL_hypercall(HYPERCALL_KAFL_PANIC, 0);
}

