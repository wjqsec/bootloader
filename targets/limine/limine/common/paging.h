/*
 * Paging support for GRUB 2
 * Nils Langius
 * 
 */

#pragma once


struct page_dir_entry {
    unsigned int present    : 1;
    unsigned int rw         : 1;
    unsigned int user       : 1;
    unsigned int pwt        : 1;
    unsigned int pcd        : 1;
    unsigned int accessed   : 1;
    unsigned int ignored    : 1;
    unsigned int page_size  : 1;
    unsigned int available  : 4;
    unsigned int page_table : 20;
} __attribute__((packed));

struct page_table_entry {
    unsigned int present    : 1;
    unsigned int rw         : 1;
    unsigned int user       : 1;
    unsigned int pwt        : 1;
    unsigned int pcd        : 1;
    unsigned int accessed   : 1;
    unsigned int dirty      : 1;
    unsigned int pat        : 1;
    unsigned int global     : 1;
    unsigned int available  : 3;
    unsigned int page_frame : 20;
} __attribute__((packed));

struct page_table { 
    struct page_table_entry pages[1024]; 
} __attribute__((packed));

struct page_dir { 
    struct page_dir_entry tables[1024]; 
} __attribute__((packed));

struct page_table_list { 
    struct page_table tables[1024];
} __attribute__((packed));

static struct page_dir *PAGE_DIRECTORY;
static struct page_table_list *page_tables;

//  interrupt
struct idt_descriptor {
    unsigned int size   : 16;
    unsigned int offset : 32;
} __attribute__((packed));

enum GATE_TYPE {
    TASK_GATE      = 0x5,
    INTERRUPT_GATE = 0xE,
    TRAP_GATE      = 0xf};

struct idt_gate {
    unsigned int offset_low  : 16;
    unsigned int segment     : 16;
    unsigned int reserved    : 8;
    unsigned int gate_type   : 4;
    unsigned int ignored     : 1;
    unsigned int dpl         : 2;
    unsigned int present     : 1;
    unsigned int offset_high : 16;
} __attribute__((packed));


struct idt { 
    struct idt_gate gates[256]; 
} __attribute__((packed));

struct interrupe_frame {
    unsigned int ip;
    unsigned int cs;
    unsigned int eflags;
} __attribute__ ((packed));

static struct idt *IDT;
static struct idt_descriptor IDTR;

//  interrupt end
#define REGIONS 3
#define PAGE_SIZE 4096

#define ADDR_TO_PAGE(addr) (((unsigned long long)(addr)) >> 12)
#define ADDR_TO_DIR_ENTRY(addr) (((unsigned long long)(addr)) >> 22)
#define ADDR_TO_TABLE_ENTRY(addr) ((((unsigned long long)(addr)) >> 12) & 0x3ff)

#define ROUNDUP_TO_PAGE_ALIGNED(addr) (((unsigned long long)addr + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1))




static bool page_inited = false;
__attribute__ ((interrupt)) void kafl_panic_interrupt_handler(struct interrupe_frame* frame)
{
    kAFL_hypercall(HYPERCALL_KAFL_PANIC, 0);
    while (1)
    {
        ;
    }
}

__attribute__ ((interrupt)) void kafl_panic_interrupt_handler_dummy(struct interrupe_frame* frame)
{
    
}

void enable_paging(void) 
{
    if(!page_inited)
        return;

    // load address of page directory into cr3
    asm volatile (
        "movl %0, %%cr3;"
        : :"r"(PAGE_DIRECTORY) :
        );

    // enable paging by setting bit 31 in cr0
    asm volatile (
        "movl %%cr0, %%eax;"
        "or $0x80000000, %%eax;"
        "movl %%eax, %%cr0;"
        : : : "eax"
        );
}
void disable_paging(void) 
{
    if(!page_inited)
        return;
    // disable paging by clearing bit 31 in cr0
    asm volatile (
        "movl %%cr0, %%eax;"
        "and $0x7fffffff, %%eax;"
        "movl %%eax, %%cr0;"
        : : : "eax"
        );
}

static void memset_paging(unsigned char *buf, unsigned char val, int size)
{
    for(int i = 0; i < size; i++)
    {
        buf[i] = val;
    }
}
void init_paging(struct page_dir *PAGE_DIRECTORY_, struct page_table_list *page_tables_) 
{ 
    // make sure to allocate page-aligned
    if ((((unsigned long long)PAGE_DIRECTORY_ & 0xfff) != 0) || (((unsigned long long)page_tables_ & 0xfff) != 0))
        return;
    PAGE_DIRECTORY = PAGE_DIRECTORY_;
    page_tables = page_tables_;
    
    
    memset_paging((unsigned char *)PAGE_DIRECTORY, 0, sizeof(struct page_dir));
    memset_paging((unsigned char *)page_tables, 0, sizeof(struct page_table_list));
    

    // allocate all page tables to avoid complexity
    for (int i = 0; i < 1024; i++)
    {
        for (int j = 0; j < 1024; j++)
        {
            page_tables->tables[i].pages[j].present = 0;
            page_tables->tables[i].pages[j].rw = 1;
            page_tables->tables[i].pages[j].page_frame = (i << 10) + j;
        }
        // add table to directory
        PAGE_DIRECTORY->tables[i].present = 1;
        PAGE_DIRECTORY->tables[i].rw = 1;
        PAGE_DIRECTORY->tables[i].page_table = ADDR_TO_PAGE(&page_tables->tables[i]);

    }
    page_inited = true;
}
void init_fuzz_intc(struct idt *IDT_)
{
    IDT = IDT_;
    memset_paging((unsigned char *)IDT, 0, sizeof(struct idt));
    for (int i = 0; i < 256; i++)
    {
        IDT->gates[i].offset_low = (unsigned long)kafl_panic_interrupt_handler_dummy & 0xffff;
        IDT->gates[i].offset_high = (unsigned long)kafl_panic_interrupt_handler_dummy >> 16;
        IDT->gates[i].segment = 16; // CS segment offset in GDT
        IDT->gates[i].present = 1;
        IDT->gates[i].gate_type = INTERRUPT_GATE; // does not really matter, interrupts are off anyways
    }


    for (int i = 0; i <= 18; i++)
    {
        IDT->gates[i].offset_low = (unsigned long)kafl_panic_interrupt_handler & 0xffff;
        IDT->gates[i].offset_high = (unsigned long)kafl_panic_interrupt_handler >> 16;
        IDT->gates[i].segment = 16; // CS segment offset in GDT
        IDT->gates[i].present = 1;
        IDT->gates[i].gate_type = INTERRUPT_GATE; // does not really matter, interrupts are off anyways
    }

    // // set IDTR

    IDTR.offset = (unsigned long)IDT;
    IDTR.size = 256*8-1;

    asm volatile (
        "cs lidt %0;"
        : : "m"(IDTR) :
        );
}

void map_unmap_page(unsigned long addr, int map)
{
    if(!page_inited)
        return;
    int dir_entry_index = ADDR_TO_DIR_ENTRY(addr);
    int page_table_index = ADDR_TO_TABLE_ENTRY(addr);

    page_tables->tables[dir_entry_index].pages[page_table_index].present = map;
}


