/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Copyright (C) 2015-2020 Micron Technology, Inc.  All rights reserved.
 */

#ifndef HSE_PLATFORM_CURSOR_HEAP_H
#define HSE_PLATFORM_CURSOR_HEAP_H

#include <sys/types.h>
#include <string.h>
#include <time.h>
#include <assert.h>
#include <sys/user.h>

#define MIN(a, b) ((a) > (b)) ? (b) : (a)
#define MAX(a, b) ((a) < (b)) ? (b) : (a)

#define CL_SIZE 64
#define CL_SHIFT 6

static inline u_int64_t
get_cycles(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);

    return (u_int64_t)ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}

/*
 * This is an allocator for use by the components of the HSE storage stack
 * stack.
 *
 * A Cursor Heap (cheap) is a memory range from which items can be allocated
 * via a cursor.  The cursor starts at offset 0, and moves forward as items
 * are allocated.
 */

/* Align @x upward to @mask. Value of @mask should be one less
 * than a power of two (e.g., 0x0001ffff).
 */
#define ALIGN_MASK(x, mask) (((x) + (mask)) & ~(mask))

/* Align @x upward to boundary @a (@a expected to be a power of 2).
 */
#define ALIGN(x, a) ALIGN_MASK(x, (typeof(x))(a)-1)

/* Align 'addr' to the next page boundary.
 */
#define PAGE_ALIGN(addr) ALIGN(addr, PAGE_SIZE)

/* Test if 'x' is aligned to 'a' boundary ('a' must be a power 2).
 */
#define IS_ALIGNED(x, a) (((x) & ((typeof(x))(a)-1)) == 0)

/* Everything in this structure is opaque to callers (but not really,
 * because the cheap unit tests need access to the implementation).
 */
struct cheap {
    size_t    alignment;
    u_int64_t cursorp;
    size_t    size;
    u_int64_t lastp;
    u_int64_t base;
    u_int64_t brk;
    void *    mem;
    u_int64_t magic;
    int       mfd;
    int       mapped;
};

/**
 * cheap_create() - Create a cursor heap from which to cheaply allocate memory
 *
 * @mem:        Memory to allocate items from
 * @alignment:  Alignment for cheap_alloc() (must be a power of 2 from 0 to 64)
 * @size:       Size of the memory at @mem
 *
 * Return: Returns a ptr to a struct cheap if successful, otherwise NULL.
 */
struct cheap *
cheap_create(int alignment, size_t size);

/**
 * cheap_create_dax() - Create a cursor heap from an entire DAX device
 *
 * @mem:        Memory to allocate items from
 * @devpath:    dax device to map (e.g. /dev/dax0.0)
 * @alignment:  Alignment for cheap_alloc() (must be a power of 2 from 0 to 64)
 *
 * Return: Returns a ptr to a struct cheap if successful, otherwise NULL.
 */
struct cheap *
cheap_create_dax(const char *devpath, int alignment);

/**
 * cheap_destroy() - destroy a cheap
 * @h:  the cheap to destroy
 *
 * Free and destroy a cheap entirely.
 * It's a bad bug to touch anything from a cheap after calling this.
 */
void
cheap_destroy(struct cheap *h);

/**
 * cheap_malloc() - allocate space from a cheap
 * @h:      the cheap from which to allocate
 * @size:   size in bytes of the desired allocation
 *
 * This function has the same general calling convention and semantics
 * as malloc().
 *
 * Return: Returns a pointer to the allocated memory if succussful,
 * otherwise returns NULL.
 */
void *
cheap_malloc(struct cheap *h, size_t size);

/**
 * Same as cheap_malloc, but exits if an allocation fails
 */
void *
cheap_xmalloc(struct cheap *h, size_t size);

/**
 * cheap_calloc() - allocate zeroed space from a cheap
 * @h:      the cheap from which to allocaet
 * @size:   size in bytes of the desired allocation
 *
 * This function has the same general calling convention and semantics
 * as calloc().
 *
 * Return: Returns a pointer to the allocated memory if succussful,
 * otherwise returns NULL.
 */
static inline void *
cheap_calloc(struct cheap *h, size_t size)
{
    void *mem;

    mem = cheap_malloc(h, size);
    if (mem)
        memset(mem, 0, size);

    return mem;
}

void
cheap_free(struct cheap *h, void *addr);

/**
 * cheap_memalign() - allocate aligned storage from a cheap
 * @h:          the cheap from which to allocate
 * @alignment:  the desired alignement
 * @size:       size in bytes of the desired allocation
 *
 * This function has the same general calling convention and semantics
 * as aligned_alloc() and memalign().  The alignment parameter of this
 * function overrides the default alignment of the cursor_heap (even
 * if it is smaller)
 *
 * Return: Returns a pointer to the allocated memory, or NULL on failure
 */
void *
cheap_memalign(struct cheap *h, int alignment, size_t size);
void *
cheap_memalign_zero(struct cheap *h, int alignment, size_t size);

/**
 * cheap_used() - return number of bytes used
 * @h:  ptr to a cheap
 *
 * Return number of bytes used, including all padding incurred
 * by aligned allocations.
 */
size_t
cheap_used(struct cheap *h);

/**
 * cheap_avail() - return remaining free space
 * @h:  ptr to a cheap
 *
 * Calculate remaining free space in a cursor_heap (cheap).
 */
size_t
cheap_avail(struct cheap *h);

#endif /* HSE_PLATFORM_CURSOR_HEAP_H */
