
# cursor_heap (aka cheap)

The cursor_heap library is adapted from the cursor_heap component in the
[Heterogeneous Storage Engine](https://github.com/hse-project/hse). 
In HSE we needed a compact allocator for
Bonsai Tree elements, and we knew that our Bonsai Trees would be freed
all at once - so 1) we didn't need the ability to free individual elements,
and 2) that allows us to avoid all garbage collection overhead and almost
all metadata overhead.

When we initially deployed the cursor_heap for this purpose, we observed a
30x improvement in maximum HSE insert performance, which (during a sync
interval) had been dominated by malloc overhead.

This adaptation is intended to provide the cheapest and most memory-efficient
allocator possible for use cases that do not need to free individual elements.
Memory benchmarks (e.g. multichase, STREAM, etc.) generally fit this well:
memory is allocated and then tested, with no cycle of free/reallocate that
would require garbage collection.

If an allocation would exceed the size of the cursor_heap, it fails.

# Build Instructions

```
$ mkdir build
$ cd build
$ cmake ..
$ make
```

The build will produce build/libcheap.a.

# Usage
## Regular system memory

When you create a cursor_heap from regular memory via cheap_create(),
the memory is allocated via anonymous mmap.  Since anonymous mmap is
lazy, it means that pages will only be allocated when they are used
(actually when they are touched).  This means it is even possible to
make a cursor_heap larger than real memory (although you almost certainly
don't want to actually do this - it will result in swap-thrashing).

For examples, refer to the unit tests in:
```
testlib/cheap_testlib.c
```

## DAX memory

cursor_heap currently supports devdax mode. The entire dax device will be mapped
and used when you call cheap_create_dax().
```c:
    struct cheap *h;

    /* Create cursor heap using the entirety of /dev/dax0.0
     * with 8 byte default allocation alignment */
    h = cheap_create_dax("/dev/dax0.0", 8);

    ptr = cheap_malloc(h, size);
```
dax-fs files are not currently supported, but such support could be added. The
issue is that determining the size of a devdax device works completely different
from determining the size of a file.

Currently the only way to create a cursor_heap from a portion of a dax
device is to partition the device into multiple sub-devices via
daxctl (disable-device, destroy-device, create-device, etc.).

## Releasing cursor_heap memory

Memory is freed when you destroy a cursor heap.  

```c:
    cheap_destroy(h);
```
You can reuse the same memory by destroying and re-creating cursor_heaps
that use the same memory (e.g. dax device), but otherwise there is no way
to keep allocating after a cursor_heap becomes full.

# Alignment
## Default Alignment
When a cursor_heap is created, an alignment parameter is passed in.  Valid
values are 0 and powers of 2 in the range 1..64.  General purpose allocation calls
(e.g. cheap_malloc(), cheap_calloc(), cheap_xmalloc() will always return
an aligned pointer.

```c:
    /* Create a cursor_heap in regular memory, where all allocations that
     * do not specify alignment return an 8-byte aligned address */
    struct cheap *h = cheap_create(8, size);
```

## Allocation-time Alignment
We also support cheap_memalign(), which is modeled after the posix_memalign()
allocator.  Alignment must be a power of 2, but there is no upper bound -
so page alignment, or even huge page-alignment are possible.
```c:
    /* Allocate 'size' bytes at a 2MiB-aligned address
    addr = cheap_memalign(h, 2 * 1024 * 1024, size);
```
Use of cheap_memalign(), or any future allocation function that is specific
as to alignment, overrides the default alignment of the cursor_heap.
Subsequent allocations with non-specified alignment will revert back to
the default alignment of the cursor_heap.

# Unit tests

This project has a good collection of unit tests, which make use of the
googletest framework. If you make any changes (or submit any patches),
you should (of course) run the unit tests.

To run the unit tests:
```
# after building the repo, from the build directory:
ctest
```
or
```
ctest -V
```


