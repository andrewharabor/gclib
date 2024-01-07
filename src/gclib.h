
#ifndef GCLIB_H
#define GCLIB_H


#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

/*
### Synopsis
Initialize `gclib`.
MUST BE CALLED FROM `main()` AND BEFORE THE PROGRAM USES ANY `gclib` FUNCTIONS.

### Description
The main job of `gclib_init()` is to define pointers to the initialized data segment and BSS along with a pointer to
the stack. It relies on being called from `main()` as it defines the stack pointer to be the address of the beginning
of the stack frame of the caller function. This means that if `gclib_init()` is used incorrectly, the garbage collector
will function with an offset stack pointer and will not be fully effective.

If `gclib_init()` isn't called before a `gclib` function is used, the function will return immediately and return a
null-value.

### Parameters
None.

### Return Value
None.
*/
void gclib_init(void);

/* Clean up any resources used by the garbage collector.
SHOULD ONLY BE CALLED AFTER ALL `gclib` FUNCTIONS ARE USED. */

/*
### Synopsis
Clean up all resources used by `gclib`.
SHOULD ONLY BE CALLED AFTER THE PROGRAM NO LONGER REQUIRES `gclib` FUNCTIONS.

### Description
`gclib_cleanup()` frees any and all memory that was dynamically allocated by `gclib` (which is separate from
user-allocated memory and is not garbage-collected). Any call to a `gclib` function after after `gclib_cleanup()`
will cause that function to return immediately and return a null-value.

### Parameters
None.

### Return Value
None.
*/
void gclib_cleanup(void);

/* Check if `gclib` functions are ready to run (`gclib_init()` has been called but `gclib_cleanup()` has not). */
bool gclib_ready(void);

/* Allocate `size` bytes of memory with the option to initialize the chunk to zero-bytes through the `zeroed` flag.
Return a pointer to the beginning of the chunk. */
void *gclib_alloc(size_t size, bool zeroed);

/* Resize the chunk pointed to by `ptr` to `new_size` bytes.
`ptr` should have been obtained through `gclib_alloc()` or `gclib_realloc()`. */
void *gclib_realloc(void *ptr, size_t new_size);

/* Explicitly free the chunk pointed to by `ptr`, which should have been obtained through `gclib_alloc()` or `gclib_realloc()`. */
void gclib_free(void *ptr);

/* Explicitly run the garbage collector to free unreachable memory obtained through `gclib_alloc()` or `gclib_realloc()`; will have a limited effect if insufficient allocations have been accumulated. */
void gclib_collect(void);

/* Force the garbage collector to free unreachable memory obtained through `gclib_alloc()` or `gclib_realloc()`, running a cycle through all allocations. */
void gclib_force_collect(void);

void gclib_print_leaks(FILE *stream);


#endif // GCLIB_H
