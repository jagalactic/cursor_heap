/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Copyright (C) 2015-2020 Micron Technology, Inc.  All rights reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "cursor_heap.h"
#include "cheap_dax.h"
#include "minmax.h"
#include "assert.h"

static struct cheap *
__cheap_create(void *mem, int alignment, size_t size)
{
	struct cheap *h = NULL;

	if (alignment < 2) {
		alignment = 1;
	}
	else if (alignment > 64) {
		return NULL; /* This is item alignment, not heap alignment */
	}
	else if (alignment & (alignment - 1)) {
		return NULL; /* Alignment must be a power of 2 */
	}

	/* We do not use the managed memory for our metadata */
	h = calloc(sizeof(*h), 1);

        /* Offset the base of the cheap by a pseudo-random number of
         * cache lines in effort to ameliorate cache conflict misses.
         */
        h->mem       = mem;
        h->magic     = (u_int64_t)h;
        h->alignment = (size_t)alignment;
        h->size      = size;
        h->base      = ALIGN((u_int64_t)h->mem, CL_SIZE);
        h->cursorp   = h->base;
        h->brk       = PAGE_ALIGN(h->cursorp);
        h->lastp     = 0;

	return h;
}

struct cheap *

cheap_create(int alignment, size_t size)
{
	struct cheap *h;
	void *addr;

	if (size < 0)
		return NULL;

	/* Align the size of all cheaps to an integral multiple
	 * of 2MB in hopes of making life easier on the VMM.
	 */
	size = ALIGN(size, 2u << 20);

	/* get memory via anonymous mmap */
	addr = mmap(NULL, size, PROT_READ | PROT_WRITE,
		    MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	if (addr == MAP_FAILED) {
		fprintf(stderr, "Anonymous mmap failed\n");
		exit(-1);
	}
	h = __cheap_create(addr, alignment, size);
	if (h)
		h->mapped = 1;
	return h;
}

struct cheap *
cheap_create_dax(const char *devpath, int alignment)
{
	int mfd;
	void *addr;
	size_t size = cheap_devdax_get_file_size(devpath);
	struct cheap *h;

	printf("%s: %s size is %ld\n", __func__, devpath, size);
	if (size <= 0)
		return NULL;

	/* Map the DAX memory */
	mfd = open(devpath, O_RDWR);
	if (mfd < 0) {
		fprintf(stderr, "Failed to open memory device %s\n",
			devpath);
		exit(-1);
	}

	addr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED,
		    mfd, 0);
	if (addr == MAP_FAILED) {
		fprintf(stderr, "mmap failed for device %s\n", devpath);
		exit(-1);
	}
	h =  __cheap_create(addr, alignment, size);
	h->mfd = mfd;
	h->mapped = 1;
	return h;
}

void
cheap_destroy(struct cheap *h)
{
    if (!h)
        return;

    assert(h->magic == (u_int64_t)h);

    if (h->mapped)
	    munmap((void *)h->mem, h->size);

    if (h->mfd)
	    close(h->mfd);

    h->magic = ~h->magic;
}

static inline void *
cheap_memalign_impl(struct cheap *h, int alignment, size_t size)
{
    u_int64_t allocp;

    assert(h->magic == (u_int64_t)h);

    assert(1 == __builtin_popcount(alignment));

    allocp = ALIGN(h->cursorp, alignment);

    if (size > h->size)
        return NULL;

    if ((allocp - h->base + size) > h->size)
        return NULL;

    h->cursorp = allocp + size;
    h->lastp = allocp;

    return (void *)allocp;
}

void *
cheap_memalign(struct cheap *h, int alignment, size_t size)
{
	if (alignment & (alignment - 1))
		return NULL;

	return cheap_memalign_impl(h, alignment, size);
}

void *
cheap_memalign_zero(struct cheap *h, int alignment, size_t size)
{
	void *p;

	if (alignment & (alignment - 1))
		return NULL;

	p = cheap_memalign_impl(h, alignment, size);
	memset(p, 0, size);
	return p;
}

void *
cheap_malloc(struct cheap *h, size_t size)
{
	return cheap_memalign_impl(h, h->alignment, size);
}

void *
cheap_xmalloc(struct cheap *h, size_t size)
{
	void *mem = cheap_malloc(h, size);

	if (mem == NULL)
		exit(-1);

	return mem;
}

void
cheap_free(struct cheap *h, void *addr)
{
    /* Freeing within a cheap can only occur if the user of the cheap only
     * ever frees something that was just allocated. Once another thing has
     * been allocated, we can't free the previously allocated thing. The
     * use case for the free is to handle the case where the owner of the
     * cheap needs to allocate space to ensure that it can make progress
     * after it does something that may fail. If the failure occurs, we want
     * to free the just-allocated space.
     *
     * [HSE_REVISIT] - this should be replaced by a reservation mechanism
     */
    if (h->lastp && (u_int64_t)addr == h->lastp) {
        if (h->brk < h->cursorp)
            h->brk = PAGE_ALIGN(h->cursorp);
        h->cursorp = h->lastp;
        h->lastp = 0;
    }
}

size_t
cheap_used(struct cheap *h)
{
    assert(h->magic == (u_int64_t)h);

    return min_t(size_t, h->size, (h->cursorp - h->base));
}

size_t
cheap_avail(struct cheap *h)
{
    assert(h->magic == (u_int64_t)h);

    return h->size - cheap_used(h);
}
