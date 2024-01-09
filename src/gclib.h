
#ifndef GCLIB_H
#define GCLIB_H


#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

/*
#### Synopsis
Initialize `gclib`.
MUST BE CALLED FROM `main()` AND BEFORE THE PROGRAM USES ANY `gclib` FUNCTIONS.

#### Description
`gclib_init()` defines important addresses from the program's memory structure for the garbage collection step.
If it is used incorrectly, the garbage collector will function as normal though will not be fully effective in freeing
unreachable memory chunks. Note that if `gclib_init()` isn't called before a `gclib` function is used, the
function will return immediately and return a null-value.

#### Parameters
None.

#### Return Value
None.
*/
void gclib_init(void);

/*
#### Synopsis
Clean up all resources used by `gclib`.
SHOULD ONLY BE CALLED AFTER THE PROGRAM NO LONGER REQUIRES `gclib` FUNCTIONS.

#### Description
`gclib_cleanup()` frees any and all memory that was dynamically allocated by `gclib` (which is separate from
user-allocated memory and is not garbage-collected). Any call to a `gclib` function after after `gclib_cleanup()`
will cause that function to return immediately and return a null-value.

#### Parameters
None.

#### Return Value
None.
*/
void gclib_cleanup(void);

/*
#### Synopsis
Indicate if `gclib` functions are ready to be called.

#### Description
None.

#### Parameters
None.

#### Return Value
The return value is `true` if `gclib_init()` has been called but `gclib_cleanup()` has not, which means
`gclib` functions can be called. Otherwise, `gclib_ready()` returns `false`. Note that the `bool` types are
defined as `#define true 1` and `#define false 0` so the numerical literals can be interpreted instead.
*/
bool gclib_ready(void);

/*
#### Synopsis
Dynamically allocate a chunk of memory subject to garbage collection.

#### Description
`gclib_alloc()` acts as a wrapper around `malloc()` or `calloc()`, using the `zeroed` parameter to
determine which one to call and then passing the `size` argument. As such, it mirrors the behavior of these
standard-library functions. The result is stored internally for `gclib` to use later on during the garbage collection
process.

#### Parameters
`size` - The size in bytes of the memory chunk to be allocated.
`zeroed` - The option to initialize all bytes in the allocated chunk to zero. Enabling this option may result in
slower runtime. Note that `bool` types are defined as `#define true 1` and `#define false 0` so the
respective numerical literals work as well.

#### Return Value
If the return value is a not `NULL`, it is a valid pointer to a memory chunk allocated by `malloc()` or `calloc()`
that can be freed through `gclib_free()`. Note that while this pointer can also be freed through the
standard-library function `free()`, `gclib` will be oblivious to this and the collector will later try to free the pointer
when the chunk is determined unreachable, resulting in undefined and erroneous behavior. If the `size` argument is
equal to zero, `gclib_alloc()` will return `NULL`. Otherwise, a return value of `NULL` indicates that `malloc()` or
`calloc()` failed, even after running the garbage collector and trying again.
*/
void *gclib_alloc(size_t size, bool zeroed);

/*
#### Synopsis
Resize a chunk of dynamically allocated memory subject to garbage collection.

#### Description
`gclib_realloc()` acts as a wrapper around `realloc()` by passing the `ptr` and `new_size` arguments to it.
As such, it mirrors the behavior of this standard-library function. The result is stored internally for `gclib` to
use later on during the garbage collection process.

#### Parameters
`ptr` - The pointer to the memory chunk that is to be resized. Note that `ptr` must have been returned by either
`gclib_alloc()` or `gclib_realloc()`; any other pointer results in undefined and erroneous behavior.
`new_size` - The size in bytes that the resized memory chunk should have.

#### Return Value
If the return value is a not `NULL`, it is a valid pointer to a memory chunk reallocated by `realloc()` that can be
freed through `gclib_free()`. Note that while this pointer can also be freed through the standard-library function
`free()`, `gclib` will be oblivious to this and the collector will later try to free the pointer when the chunk is
determined unreachable, resulting in undefined and erroneous behavior. If the argument `ptr` is equal to `NULL`,
the function will act as `gclib_alloc(new_size)` for all values of `new_size`. If `new_size` is equal to zero, the
function will act as `gclib_free(ptr)`. In the case that `ptr` is `NULL` and `new_size` is zero, the function will
return `NULL` (as specified when `gclib_alloc()` is called with zero). Otherwise, a return value of `NULL` indicates
that `realloc()` failed, even after running the garbage collector and trying again.
*/
void *gclib_realloc(void *ptr, size_t new_size);

/*
#### Synopsis
Explicitly free a chunk of dynamically allocated memory subject to garbage collection.

#### Description
`gclib_free()` acts as a warapper around `free()` by passing the argument `ptr` to it. As such, it mirrors the
behavior of this standard-library function.

#### Parameters
`ptr` - The pointer to the memory chunk that is to be freed. Note that `ptr` must have been returned by either
`gclib_alloc()` or `gclib_realloc()`; any other pointer results in undefined and erroneous behavior.

#### Return Value
None.
*/
void gclib_free(void *ptr);

/*
#### Synopsis
Explicitly run the garbage collector in order to free unreachable dynamically allocated memory chunks.

#### Description
`gclib_collect()` runs a standard cycle of the garbage collector in an effort to collect memory chunks allocated
through `gclib_alloc()` and `gclib_realloc()`. However, note that because each generation of chunks is only
swept through after a certain number of bytes have been allocated, not much will be collected if only a small
amount of memory has been allocated through `gclib` functions.

#### Parameters
None.

#### Return Value
None.
*/
void gclib_collect(void);

/*
#### Synopsis
Forcibly run the garbage collector in order to free unreachable dynamically allocated memory chunks from all
generations.
USE OF THIS FUNCTION IS NOT RECOMMENDED AS IT IS TIME CONSUMING AND MAY INTERFERE WITH THE
COLLECTOR'S NATURAL CYCLES.

#### Description
`gclib_force_collect()` runs a cycle of the garbage collector but with the condition that all generations are
swept through. The result is that any chunks allocated through `gclib_alloc()` and `gclib_realloc()` that are
determined to be unreachable are freed.

#### Parameters
None.

#### Return Value
None.
*/
void gclib_force_collect(void);

/*
#### Synopsis
Print out any unfreed memory chunks allocated through `gclib_alloc()` and `gclib_realloc()`.

#### Description
`gclib_print_leaks()` is intended to utilize `gclib`'s tracking of memory for garbage collection as part of
memory-leak detection capability. Note that throughout the duration of the program, as allocations are made, the
collector may activate and free unreachable chunks. This means that the diagnosis of memory leaks may not be
completely accurate. Nevertheless, it may give the caller a decent idea of where the program leaks memory. In a
program that uses only the standard-library allocation functions, the following macros can be used in conjunction with
`gclib_print_leaks()` to identify unfreed memory chunks (of course `gclib_init()` and `gclib_cleanup()` should be
included as well).

``` c
#define malloc(size) gclib_alloc(size, false)
#define calloc(count, size_each) gclib_alloc((count) * (size_each), true)
#define realloc(ptr, new_size) gclib_realloc(ptr, new_size)
#define free(ptr) gclib_free(ptr)
```

#### Parameters
`stream` - The file or output stream in which to print the found memory leaks.

#### Return Value
None.
*/
void gclib_print_leaks(FILE *stream);


#endif // GCLIB_H
