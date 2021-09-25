
CFLAGS=-O2 -march=native



libcheap_dax.a: cursor_heap.o cheap_dax.o
	ar rcs libcheap_dax.a cursor_heap.o cheap_dax.o

all:	libcheap_dax.a

clean:
	-rm *.o *.a




