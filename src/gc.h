
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

/* Allocate `size` bytes of memory and return a pointer to the beginning of the chunk. */
void *gc_alloc(size_t size);

/* Explicitly free `ptr`, which should have been obtained through `gc_alloc()`. */
void gc_free(void *ptr);

/* Force the garbage collector to run a cycle and free unreachable memory chunks obtained through `gc_alloc()`. */
void gc_force_collect();


#endif // GC_H
