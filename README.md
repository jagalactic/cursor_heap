
# cursor_heap (aka cheap)

The cursor_heap library is adapted from the cursor_heap component in the
Heterogeneous Storage Engine. In HSE we needed a compact allocator for
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

# Alignment
## cursor_heap alignment
When a cursor_heap is created, and alignment parameter is passed in.  Valid
values are powers of 2 in the range 0..64.  General purpose allocation calls
(e.g. cheap_malloc(), cheap_calloc(), cheap_xmalloc() will always return
an aligned pointer.

## allocation alignment
We also support cheap_memalign(), which is modeled after the posix_memalign()
allocator.  Alignment must be a power of 2, but there is no upper bound -
so page alignment, or even huge page-alignment are possible.

# Usage with regular system memory

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

# Usage with DAX memory

cursor_heap currently supports devdax mode only.
```c:
    struct cheap *h;

    /* Create cursor heap with 8 byte default allocation alignment */
    h = cheap_create_dax("/dev/dax0.0", 8);
    

```

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


