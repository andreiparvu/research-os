// kheap.h -- Interface for kernel heap functions, also provides
//            a placement malloc() for use before the heap is 
//            initialised.
//            Written for JamesM's kernel development tutorials.

#ifndef KHEAP_H
#define KHEAP_H

#include "common.h"
#include "ordered_array.h"

#define KHEAP_START         0xC0000000
#define KHEAP_INITIAL_SIZE  0x100000
#define HEAP_INDEX_SIZE   0x20000
#define HEAP_MAGIC        0x123890AB
#define HEAP_MIN_SIZE     0x70000

typedef struct {
  uint32_t magic;
  uint8_t is_hole;
  uint32_t size;
} header_t;

typedef struct {
  uint32_t magic;
  header_t *header;
} footer_t;

typedef struct {
  ordered_array_t index;
  uint32_t start_address;
  uint32_t end_address;
  uint32_t max_address;

  uint8_t supervisor;
  uint8_t readonly;
} heap_t;


heap_t *create_heap(uint32_t start, uint32_t end, uint32_t max,
    uint8_t supervisor, uint8_t readonly);

void *alloc(uint32_t size, uint8_t page_align, heap_t *heap);

void free(void *p, heap_t *heap);

/**
   Allocate a chunk of memory, sz in size. If align == 1,
   the chunk must be page-aligned. If phys != 0, the physical
   location of the allocated chunk will be stored into phys.

   This is the internal version of kmalloc. More user-friendly
   parameter representations are available in kmalloc, kmalloc_a,
   kmalloc_ap, kmalloc_p.
**/
uint32_t kmalloc_int(uint32_t sz, int align, uint32_t *phys);

/**
   Allocate a chunk of memory, sz in size. The chunk must be
   page aligned.
**/
uint32_t kmalloc_align(uint32_t sz);

/**
   Allocate a chunk of memory, sz in size. The physical address
   is returned in phys. Phys MUST be a valid pointer to uint32_t!
**/
uint32_t kmalloc_phys(uint32_t sz, uint32_t *phys);

/**
   Allocate a chunk of memory, sz in size. The physical address 
   is returned in phys. It must be page-aligned.
**/
uint32_t kmalloc_ap(uint32_t sz, uint32_t *phys);

/**
   General allocation function.
**/
uint32_t kmalloc(uint32_t sz);

void kfree(uint32_t p);
#endif // KHEAP_H
