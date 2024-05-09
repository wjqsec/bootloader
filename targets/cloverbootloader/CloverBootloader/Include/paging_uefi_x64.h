#pragma once

// #define DBG
// struct linear_addr
// {
//     unsigned long long offset : 12;
//     unsigned long long table : 9;
//     unsigned long long dir : 9;
//     unsigned long long dirptr : 9;
//     unsigned long long pml4 : 9;
//     unsigned long long unused : 16;
// }__attribute__((packed));


// struct pml4_entry
// {
//     unsigned long long p: 1;
//     unsigned long long rw: 1;
//     unsigned long long us: 1;
//     unsigned long long page_write_through: 1;
//     unsigned long long page_cache: 1;
//     unsigned long long accessed: 1;
//     unsigned long long ignored1: 1;
//     unsigned long long page_size: 1;
//     unsigned long long ignored2: 4;
//     unsigned long long pfn: 36;
//     unsigned long long reserved: 4;
//     unsigned long long ignored3: 11;
//     unsigned long long nx: 1;
// }__attribute__((packed));

// struct dirptr_entry
// {
//     unsigned long long p: 1;
//     unsigned long long rw: 1;
//     unsigned long long us: 1;
//     unsigned long long page_write: 1;
//     unsigned long long page_cache: 1;
//     unsigned long long accessed: 1;
//     unsigned long long ignored1: 1;
//     unsigned long long page_size: 1;
//     unsigned long long ignored2: 4;
//     unsigned long long pfn: 36;
//     unsigned long long reserved: 4;
//     unsigned long long ignored3: 11;
//     unsigned long long nx: 1;
// }__attribute__((packed));


// struct dir_entry
// {
//     unsigned long long p: 1;
//     unsigned long long rw: 1;
//     unsigned long long us: 1;
//     unsigned long long page_write: 1;
//     unsigned long long page_cache: 1;
//     unsigned long long accessed: 1;
//     unsigned long long ignored1: 1;
//     unsigned long long page_size: 1;
//     unsigned long long ignored2: 4;
//     unsigned long long pfn: 36;
//     unsigned long long reserved: 4;
//     unsigned long long ignored3: 11;
//     unsigned long long nx: 1;
// }__attribute__((packed));

// struct pte_entry
// {
//     unsigned long long p: 1;
//     unsigned long long rw: 1;
//     unsigned long long us: 1;
//     unsigned long long page_write: 1;
//     unsigned long long page_cache: 1;
//     unsigned long long accessed: 1;
//     unsigned long long dirty: 1;
//     unsigned long long page_access_type: 1;
//     unsigned long long global: 4;
//     unsigned long long pfn: 36;
//     unsigned long long reserved: 4;
//     unsigned long long ignored3: 7;
//     unsigned long long protect_key: 4;
//     unsigned long long nx: 1;
// }__attribute__((packed));

// struct pml4
// {
//     struct pml4_entry entries[512];
// }__attribute__((packed));

// struct dirptr
// {
//     struct dirptr_entry entries[512];
// }__attribute__((packed));

// struct dir
// {
//     struct dir_entry entries[512];
// }__attribute__((packed));

// struct table
// {
//     struct pte_entry entries[262144];
// }__attribute__((packed));

// static struct pml4 *pml4_ptr;
// static struct dirptr *dirptr_ptr;
// static struct dir *dir_ptr;
// static struct table *table_ptr;

// static int page_inited = 0;


// #define MAKE_ADDR(pml4,dirptr,dir,table,offset) (offset | ((unsigned long long)table << 12) | ((unsigned long long)dir << 21) | ((unsigned long long)dirptr << 30) | ((unsigned long long)pml4 << 39))
// #define PML4(addr) (((unsigned long long)addr >> 39) & 0x1ff)
// #define DIRPTR(addr) (((unsigned long long)addr >> 30) & 0x1ff)
// #define DIR(addr) (((unsigned long long)addr >> 21) & 0x1ff)
// #define TABLE(addr) (((unsigned long long)addr >> 12) & 0x1ff)

// static void memset_paging(unsigned char *buf, unsigned char val, int size)
// {
//     for(int i = 0; i < size; i++)
//     {
//         buf[i] = val;
//     }
// }
// void init_paging(struct pml4 *pml4_ptr_,struct dirptr *dirptr_ptr_,struct dir *dir_ptr_,struct table *table_ptr_)
// {
//     if(((unsigned long long)pml4_ptr_ & 0xfff) != 0 
//     || ((unsigned long long)dirptr_ptr_ & 0xfff) != 0 
//     || ((unsigned long long)dir_ptr_ & 0xfff) != 0 
//     || ((unsigned long long)table_ptr_ & 0xfff) != 0 )
//     {
//         return;
//     }
//     pml4_ptr = pml4_ptr_;
//     dirptr_ptr = dirptr_ptr_;
//     dir_ptr = dir_ptr_;
//     table_ptr = table_ptr_;
//     memset_paging((unsigned char*)pml4_ptr,0,sizeof(struct pml4));
//     memset_paging((unsigned char*)dirptr_ptr,0,sizeof(struct dirptr));
//     memset_paging((unsigned char*)dir_ptr,0,sizeof(struct dir));
//     memset_paging((unsigned char*)table_ptr,0,sizeof(struct table));
    
//     pml4_ptr->entries[0].p = 1;
//     pml4_ptr->entries[0].rw = 1;
//     pml4_ptr->entries[0].us = 1;
//     pml4_ptr->entries[0].page_write_through = 1;
//     pml4_ptr->entries[0].page_cache = 1;
//     pml4_ptr->entries[0].accessed = 0;
//     pml4_ptr->entries[0].page_size = 0;
//     pml4_ptr->entries[0].pfn = ((unsigned long long)dirptr_ptr) >> 12;
//     pml4_ptr->entries[0].nx = 0;


//     dirptr_ptr->entries[0].p = 1;
//     dirptr_ptr->entries[0].rw = 1;
//     dirptr_ptr->entries[0].us = 1;
//     dirptr_ptr->entries[0].page_write = 1;
//     dirptr_ptr->entries[0].page_cache = 1;
//     dirptr_ptr->entries[0].accessed = 0;
//     dirptr_ptr->entries[0].page_size = 0;
//     dirptr_ptr->entries[0].pfn = ((unsigned long long)dir_ptr) >> 12;
//     dirptr_ptr->entries[0].nx = 0;


//     for (int i = 0 ; i < 512 ; i++)
//     {
//         dir_ptr->entries[i].p = 1;
//         dir_ptr->entries[i].rw = 1;
//         dir_ptr->entries[i].us = 1;
//         dir_ptr->entries[i].page_write = 1;
//         dir_ptr->entries[i].page_cache = 1;
//         dir_ptr->entries[i].accessed = 0;
//         dir_ptr->entries[i].page_size = 0;
//         dir_ptr->entries[i].pfn = ((unsigned long long)&table_ptr->entries[i * 512]) >> 12;
//         dir_ptr->entries[i].nx = 0;
//         for (int j = 0 ; j < 512 ; j++)
//         {
//             table_ptr->entries[i * 512 + j].p = 1;
//             table_ptr->entries[i * 512 + j].rw = 1;
//             table_ptr->entries[i * 512 + j].us = 1;
//             table_ptr->entries[i * 512 + j].page_write = 1;
//             table_ptr->entries[i * 512 + j].page_cache = 1;
//             table_ptr->entries[i * 512 + j].accessed = 0;
//             table_ptr->entries[i * 512 + j].dirty = 0;
//             table_ptr->entries[i * 512 + j].page_access_type = 1;
//             table_ptr->entries[i * 512 + j].global = 0;
//             table_ptr->entries[i * 512 + j].pfn = ((unsigned long long)MAKE_ADDR(0,0,i,j,0)) >> 12;
//             table_ptr->entries[i * 512 + j].nx = 0;
//         }
//     }
//     page_inited = 1;
// }
// void enable_paging(void)
// {
//     if(!page_inited)
//         return;

//     asm volatile ( 
//         // "mov %cr0, %rdx;"
//         // "and $0x7fffffff, %edx;"
//         // "mov %rdx, %cr0;"

//         "mov %cr4, %rdx;"
//         "or  $0x20, %rdx;"
//         "mov %rdx, %cr4;" );

//     asm volatile (
//         "movq %0, %%cr3"
//         : :"r"(pml4_ptr) :
//         );
//     asm volatile (
//         "mov %cr0, %rdx;"
//         "or $0x80000000, %edx;"
//         "mov %rdx, %cr0;"
//         );
//     while(1)
//     {
//         ;
//     }
// }

// unsigned long long get_cr3() 
// {
//     unsigned long long cr3;
//     asm volatile (
//         "mov %%cr3,%0" : "=r"(cr3));
//     return cr3;
// }
// void disable_paging(void)
// {
//     if(!page_inited)
//         return;
//     asm volatile (
//         "mov %cr0, %rdx;"
//         "and $0x7fffffff, %edx;"
//         "mov %rdx, %cr0;"
//         );
// }

// void map_unmap_page(void *addr,int val)
// {
//     if(!page_inited)
//         return;
//     int pm4l4_idx = PML4(addr);
//     int dirptr_idx = DIRPTR(addr);
//     int dir_idx = DIR(addr);
//     int table_idx = TABLE(addr);
//     if (pm4l4_idx != 0 || dirptr_idx != 0)
//         return;
//      table_ptr->entries[dir_idx * 512 + table_idx].p = val;
// }

struct idt_descriptor_x64
{
    unsigned short length;
    void*    base;
} __attribute__((packed));

struct idt_gate_x64 {
    unsigned long long offset1: 16;
    unsigned long long segment : 16;
    unsigned long long ist : 3;
    unsigned long long reserved : 5;
    unsigned long long gate_type :4;
    unsigned long long zero :1;
    unsigned long long dpl : 2;
    unsigned long long p :1;
    unsigned long long offset2 : 16;
    unsigned long long offset3 : 32;
    unsigned long long reserved2 : 32;

}__attribute__((packed));


struct idt { 
    struct idt_gate_x64 gates[256]; 
} __attribute__((packed));

struct interrupt_frame
{
    unsigned long long ip;
    unsigned long long cs;
    unsigned long long flags;
    unsigned long long sp;
    unsigned long long ss;
};

static struct idt *IDT;
// static struct idt_descriptor_x64 IDTR;

void kafl_panic_interrupt_handler(struct interrupt_frame *p);
void get_idtr(struct idt_descriptor_x64 *idtr);
void init_fuzz_intc(void);

void kafl_panic_interrupt_handler(struct interrupt_frame *p)
{
    (void)p;

    #ifdef DBG
    printf("kafl_panic_interrupt_handler\n");
    #else
    kAFL_hypercall(HYPERCALL_KAFL_PANIC, 0);
    #endif
    while (1)
    {
        ;
    }
}


void get_idtr(struct idt_descriptor_x64 *idtr) {
    asm volatile (
        "sidt %[idtr]" // Read IDTR into the memory pointed by 'idtr'
        : [idtr] "=m" (*idtr) // Output constraint
        : // No input constraint
        : "memory" // Clobbered memory
    );
}

void init_fuzz_intc(void)
{
    asm volatile ("cli");
    struct idt_descriptor_x64 idt_desc;
    get_idtr(&idt_desc);


    IDT = (idt*)idt_desc.base;
    
    for (int i = 0; i < 0x14; i++)
    {
        IDT->gates[i].offset1 = (unsigned long long)kafl_panic_interrupt_handler & 0xffff;
        // IDT->gates[i].segment = 0x38;
        // IDT->gates[i].ist = 0; 
        // IDT->gates[i].reserved = 0;
        // IDT->gates[i].gate_type = 0xe;
        // IDT->gates[i].zero = 0;
        // IDT->gates[i].dpl = 0;
        IDT->gates[i].p = 1;
        IDT->gates[i].offset2 = ((unsigned long long)kafl_panic_interrupt_handler >> 16) & 0xffff;
        IDT->gates[i].offset3 = ((unsigned long long)kafl_panic_interrupt_handler >> 32) & 0xffffffff;
        // IDT->gates[i].reserved2 = 0;
    }


    // // set IDTR

    // IDTR.base = IDT;
    // IDTR.length = 256*16 -1;

    // asm volatile (
    //     "lidt %0;"
    //     : : "m"(IDTR) :
    //     );
    asm volatile ("sti");
    
}

// static void test_crash(void)
// {
//     volatile int*a = (int*)0xf234567887654321;
//     volatile int b = *a;
//     (void)b;
// }

