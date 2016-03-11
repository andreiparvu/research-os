// paging.c -- Defines the interface for and structures relating to paging.
//             Written for JamesM's kernel development tutorials.

#include "paging.h"
#include "kheap.h"

// The kernel's page directory
page_directory_t *kernel_directory=0;

// The current page directory;
page_directory_t *current_directory=0;

// A bitset of frames - used or free.
u32int *frames;
u32int nframes;

// Defined in kheap.c
extern u32int placement_address;
extern heap_t *kheap;

// Macros used in the bitset algorithms.
#define INDEX_FROM_BIT(a) (a/(8*4))
#define OFFSET_FROM_BIT(a) (a%(8*4))

// Static function to set a bit in the frames bitset
static void set_frame(u32int frame_addr)
{
    u32int frame = frame_addr/0x1000;
    u32int idx = INDEX_FROM_BIT(frame);
    u32int off = OFFSET_FROM_BIT(frame);
    frames[idx] |= (0x1 << off);
}

// Static function to clear a bit in the frames bitset
static void clear_frame(u32int frame_addr)
{
    u32int frame = frame_addr/0x1000;
    u32int idx = INDEX_FROM_BIT(frame);
    u32int off = OFFSET_FROM_BIT(frame);
    frames[idx] &= ~(0x1 << off);
}

// Static function to test if a bit is set.
static u32int test_frame(u32int frame_addr)
{
    u32int frame = frame_addr/0x1000;
    u32int idx = INDEX_FROM_BIT(frame);
    u32int off = OFFSET_FROM_BIT(frame);
    return (frames[idx] & (0x1 << off));
}

// Static function to find the first free frame.
static u32int first_frame()
{
    u32int i, j;
    for (i = 0; i < INDEX_FROM_BIT(nframes); i++)
    {
        if (frames[i] != 0xFFFFFFFF) // nothing free, exit early.
        {
            // at least one bit is free here.
            for (j = 0; j < 32; j++)
            {
                u32int toTest = 0x1 << j;
                if ( !(frames[i]&toTest) )
                {
                    return i*4*8+j;
                }
            }
        }
    }
}

// Function to allocate a frame.
void alloc_frame(page_t *page, int is_kernel, int is_writeable) {
    if (page->frame != 0) {
        return;
    }
    u32int idx = first_frame();
    if (idx == (u32int)-1) {
      // PANIC! no free frames!!
    }
    set_frame(idx * 0x1000);
    page->present = 1;
    page->rw = 1;//is_writeable;
    page->user = !is_kernel;
    page->frame = idx;
}

// Function to deallocate a frame.
void free_frame(page_t *page)
{
    u32int frame;
    if (!(frame=page->frame))
    {
        return;
    }
    else
    {
        clear_frame(frame);
        page->frame = 0x0;
    }
}

extern u32int end;

void initialise_paging() {
  // The size of physical memory. For the moment we 
  // assume it is 16MB big.
  u32int mem_end_page = 0x1000000;

  nframes = mem_end_page / 0x1000;
  frames = (u32int*)kmalloc(INDEX_FROM_BIT(nframes));
  memset(frames, 0, INDEX_FROM_BIT(nframes));

  // Let's make a page directory.
  kernel_directory = (page_directory_t*)kmalloc_align(sizeof(page_directory_t));
  memset(kernel_directory, 0, sizeof(page_directory_t));
  //current_directory = kernel_directory;
  kernel_directory->physicalAddr = (u32int)kernel_directory->tablesPhysical;

  int i = 0;
  for (i = KHEAP_START; i < KHEAP_START + KHEAP_INITIAL_SIZE; i += 0x1000) {
    get_page(i, 1, kernel_directory);
  }

  i = 0;
  while (i < placement_address + 0x1000) {
    // Kernel code is readable but not writeable from userspace.
    alloc_frame(get_page(i, 1, kernel_directory), 0, 0);
    i += 0x1000;
  }

  for (i = KHEAP_START; i < KHEAP_START + KHEAP_INITIAL_SIZE; i += 0x1000) {
    alloc_frame( get_page(i, 1, kernel_directory), 0, 0);
  }

  // Before we enable paging, we must register our page fault handler.
  register_interrupt_handler(14, page_fault);

  // Now, enable paging!
  switch_page_directory(kernel_directory);

  kheap = create_heap(KHEAP_START, KHEAP_START + KHEAP_INITIAL_SIZE, 0xcffff000, 0, 0);

  current_directory = clone_directory(kernel_directory);
  //current_directory = kernel_directory;
  switch_page_directory(current_directory);
}

static page_table_t* clone_table(page_table_t *src, uint32_t *physAddr) {
  page_table_t *table = (page_table_t*)kmalloc_ap(sizeof(page_table_t), physAddr);
  memset(table, 0, sizeof(page_directory_t));

  int i;
  for (i = 0; i < 1024; i++) {
    if (!src->pages[i].frame) {
      continue;
    }

    alloc_frame(&table->pages[i], 0, 0);

    if (src->pages[i].present) table->pages[i].present = 1;
    if (src->pages[i].rw) table->pages[i].rw = 1;
    if (src->pages[i].user) table->pages[i].user = 1;
    if (src->pages[i].accessed) table->pages[i].accessed = 1;
    if (src->pages[i].dirty) table->pages[i].dirty = 1;

    copy_page_physical(src->pages[i].frame * 0x1000, table->pages[i].frame * 0x1000);
  }

  return table;
}

page_directory_t* clone_directory(page_directory_t *src) {
  uint32_t phys;

  page_directory_t *dir = (page_directory_t*)kmalloc_ap(sizeof(page_directory_t),
      &phys);
  memset(dir, 0, sizeof(page_directory_t));

  uint32_t offset = (uint32_t)dir->tablesPhysical - (uint32_t)dir;

  dir->physicalAddr = phys + offset;

  int i;
  for (i = 0; i < 1024; i++) {
    if (!src->tables[i]) {
      continue;
    }

    if (kernel_directory->tables[i] == src->tables[i]) {
      dir->tables[i] = src->tables[i];
      dir->tablesPhysical[i] = src->tablesPhysical[i];
    } else {
      uint32_t phys;
      dir->tables[i] = clone_table(src->tables[i], &phys);
      dir->tablesPhysical[i] = phys | 0x07;
    }
  }

  return dir;
}

void switch_page_directory(page_directory_t *dir)
{
    current_directory = dir;
    asm volatile("mov %0, %%cr3":: "r"(dir->physicalAddr));
    u32int cr0;
    asm volatile("mov %%cr0, %0": "=r"(cr0));
    cr0 |= 0x80000000; // Enable paging!
    asm volatile("mov %0, %%cr0":: "r"(cr0));
}

page_t *get_page(u32int address, int make, page_directory_t *dir) {
    // Turn the address into an index.
    address /= 0x1000;
    // Find the page table containing this address.
    u32int table_idx = address / 1024;
    if (dir->tables[table_idx]) {
        return &dir->tables[table_idx]->pages[address % 1024];
    }
    if (make) {
        uint32_t tmp;
        monitor_write("going to do this here\n");
        dir->tables[table_idx] = (page_table_t*)kmalloc_ap(sizeof(page_table_t), &tmp);
        memset(dir->tables[table_idx], 0, 0x1000);
        dir->tablesPhysical[table_idx] = tmp | 0x7; // PRESENT, RW, US.
        return &dir->tables[table_idx]->pages[address % 1024];
    }

    return NULL;
}


void page_fault(registers_t regs)
{
    // A page fault has occurred.
    // The faulting address is stored in the CR2 register.
    u32int faulting_address;
    asm volatile("mov %%cr2, %0" : "=r" (faulting_address));
    
    // The error code gives us details of what happened.
    int present   = !(regs.err_code & 0x1); // Page not present
    int rw = regs.err_code & 0x2;           // Write operation?
    int us = regs.err_code & 0x4;           // Processor was in user-mode?
    int reserved = regs.err_code & 0x8;     // Overwritten CPU-reserved bits of page entry?
    int id = regs.err_code & 0x10;          // Caused by an instruction fetch?

    // Output an error message.
    monitor_write("Page fault! ( ");
    if (present) {monitor_write("present ");}
    if (rw) {monitor_write("read-only ");}
    if (us) {monitor_write("user-mode ");}
    if (reserved) {monitor_write("reserved ");}
    monitor_write(") at ");
    monitor_write_hex(faulting_address);
    monitor_write("\n");
    PANIC("Page fault");
}
