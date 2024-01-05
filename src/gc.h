
#ifndef GC_H
#define GC_H

#include <stdbool.h>
#include <stddef.h>

/* Initialize the garbage collector.
MUST BE CALLED FROM `main()` AND BEFORE ANY OTHER `gc_` FUNCTION IS USED. */
void gc_init(void);

/* Clean up any resources the garbage collector used.
SHOULD ONLY BE CALLED AFTER ALL `gc_` FUNCTIONS ARE USED. */
void gc_cleanup(void);

/* Check if `gc_` functions are ready to run (`gc_init()` has been called but `gc_cleanup()` has not). */
bool gc_ready(void);

/* Allocate `size` bytes of memory with the option to initialize the chunk to zero-bytes through `zeroed`; return a pointer to the beginning of the chunk. */
void *gc_alloc(size_t size, bool zeroed);

/* Resize the chunk pointed to by `ptr` to `new_size` bytes; `ptr` should have been obtained through `gc_alloc()` or `gc_realloc()`. */
void *gc_realloc(void *ptr, size_t new_size);

/* Explicitly free the chunk pointed to by `ptr`, which should have been obtained through `gc_alloc()` or `gc_realloc()`. */
void gc_free(void *ptr);

/* Explicitly run the garbage collector to free unreachable memory obtained through `gc_alloc()` or `gc_realloc()`; will have a limited effect if insufficient allocations have been accumulated. */
void gc_collect(void);

/* Force the garbage collector to free unreachable memory obtained through `gc_alloc()` or `gc_realloc()`, running a cycle through all allocations. */
void gc_force_collect(void);


#endif // GC_H
