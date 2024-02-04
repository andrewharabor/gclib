# gclib

A garbage collection "library" written in and for C

## Description

First of all, note that while this project is designed similar to a library, it is not actually intended to be used in such a way. In fact, it is not intended to be used at all as it was more an exercise in learning about garbage collection and memory management in C.

The "library" functions included in this project work as wrappers around the standard-library functions `malloc()`, `calloc()`, `realloc()`, and `free()`, which can be found in `stdlib.h`. That is to say that this project does not implement its own memory allocator.

The garbage collector itself works as follows, implementing a mark-and-sweep algorithm:

Mark phase: It establishes the root set, which are segments in the program's memory layout that can contain references to allocated chunks. This specific collector scans the initialized data segment and the BSS segment where global variables are stored as well as the active stack which contains local variables and arguments from function calls. Note that it does not explicitly scan the entirety of the heap itself, where allocated chunks are actually located. It then iterates through each aligned block of 8 bytes (`sizeof(void *)`) within the root set, interprets them as a pointer, and checks to see if it points to or falls within an allocated memory chunk. If so, that chunk is marked as reachable and it is recursively scanned (if the chunk is reachable, anything it points to is also reachable). Note that by pure chance, a set of aligned 8 bytes may just happen to look like a valid pointer, in which case the collector still counts it as a valid reference. This is why the collector is conservative.

Sweep phase: After all the necessary memory has been checked for references, the collector iterates through the allocated chunks and frees those that are unreachable.

This collector however, also implements generational garbage collecion. The premise is that the longer a chunk has been allocated, the longer it will continue to remain reachable in the program. Here is how this garbage collector implements this idea: When chunks are first allocated, they are placed into generation 0. Once a certain number of bytes has been allocated within this generation, a garbage collection cycle is performed. Unreachable chunks are freed while reachable chunks get promoted to the next generation. This process continues, with each generation only being collected once a certain quota of allocated bytes is filled. Chunks in the last generation are collected the least often and also cannot be promoted.

## Limitations

There are quite a few things that hold back this garbage collector. As a non-exhaustive list:

- No support for multiple threads; it is a stop-the-world collector where the rest of the program must halt completely while the collector runs.
- Probably only works on x86-64 Linux and when compiled with GCC because of how the locations of the data segment and active stack are obtained.
- The root set may not encompass everywhere that may contain refernces to allocated memory in the program.
- Not even sure that it would work as a library unless compiled and linked with the source file(s) that use it.

## Documentation

### `gclib_init()`

#### Prototype

``` c
void gclib_init(void);
```

#### Synopsis

Initialize `gclib`. **Must be called from `main()` and before the program uses any `gclib` functions.**

#### Description

`gclib_init()` defines important addresses from the program's memory structure for the garbage collection step. If it is used incorrectly, the garbage collector will function as normal though will not be fully effective in freeing unreachable memory chunks. Note that if `gclib_init()` isn't called before a `gclib` function is used, the function will return immediately and return a null-value.

#### Parameters

None.

#### Return Value

None.

### `gclib_cleanup()`

#### Prototype

``` c
void gclib_cleanup(void);
```

#### Synopsis

Clean up all resources used by `gclib`. **Should only be called after the program no longer requires `gclib` functions.**

#### Description

`gclib_cleanup()` frees any and all memory that was dynamically allocated by `gclib` (which is separate from user-allocated memory and is not garbage-collected). Any call to a `gclib` function after after `gclib_cleanup()` will cause that function to return immediately and return a null-value.

#### Parameters

None.

#### Return Value

None.

### `gclib_ready()`

#### Prototype

``` c
bool gclib_ready(void);
```

#### Synopsis

Indicate if `gclib` functions are ready to be called.

#### Description

None.

#### Parameters

None.

#### Return Value

The return value is `true` if `gclib_init()` has been called but `gclib_cleanup()` has not, which means `gclib` functions can be called. Otherwise, `gclib_ready()` returns `false`. Note that the `bool` types are defined as `#define true 1` and `#define false 0` so the numerical literals can be interpreted instead.

### `gclib_alloc()`

#### Prototype

``` c
void *gclib_alloc(size_t size, bool zeroed);
```

#### Synopsis

Dynamically allocate a chunk of memory subject to garbage collection.

#### Description

`gclib_alloc()` acts as a wrapper around `malloc()` or `calloc()`, using the `zeroed` parameter to determine which one to call and then passing the `size` argument. As such, it mirrors the behavior of these standard-library functions. The result is stored internally for `gclib` to use later on during the garbage collection process.

#### Parameters

`size` - The size in bytes of the memory chunk to be allocated.

`zeroed` - The option to initialize all bytes in the allocated chunk to zero. Enabling this option may result in slower runtime. Note that `bool` types are defined as `#define true 1` and `#define false 0` so the respective numerical literals work as well.

#### Return Value

If the return value is a not `NULL`, it is a valid pointer to a memory chunk allocated by `malloc()` or `calloc()` that can be freed through `gclib_free()`. Note that while this pointer can also be freed through the standard-library function `free()`, `gclib` will be oblivious to this and the collector will later try to free the pointer when the chunk is determined unreachable, resulting in undefined and erroneous behavior. If the `size` argument is equal to zero, `gclib_alloc()` will return `NULL`. Otherwise, a return value of `NULL` indicates that `malloc()` or `calloc()` failed, even after running the garbage collector and trying again.

### `gclib_realloc()`

#### Prototype

``` c
void *gclib_realloc(void *ptr, size_t new_size);
```

#### Synopsis

Resize a chunk of dynamically allocated memory subject to garbage collection.

#### Description

`gclib_realloc()` acts as a wrapper around `realloc()` by passing the `ptr` and `new_size` arguments to it. As such, it mirrors the behavior of this standard-library function. The result is stored internally for `gclib` to use later on during the garbage collection process.

#### Parameters

`ptr` - The pointer to the memory chunk that is to be resized. Note that `ptr` must have been returned by either `gclib_alloc()` or `gclib_realloc()`; any other pointer results in undefined and erroneous behavior.

`new_size` - The size in bytes that the resized memory chunk should have.

#### Return Value

If the return value is a not `NULL`, it is a valid pointer to a memory chunk reallocated by `realloc()` that can be freed through `gclib_free()`. Note that while this pointer can also be freed through the standard-library function `free()`, `gclib` will be oblivious to this and the collector will later try to free the pointer when the chunk is determined unreachable, resulting in undefined and erroneous behavior. If the argument `ptr` is equal to `NULL`, the function will act as `gclib_alloc(new_size)` for all values of `new_size`. If `new_size` is equal to zero, the function will act as `gclib_free(ptr)`. In the case that `ptr` is `NULL` and `new_size` is zero, the function will return `NULL` (as specified when `gclib_alloc()` is called with zero). Otherwise, a return value of `NULL` indicates that `realloc()` failed, even after running the garbage collector and trying again.

### `gclib_free()`

#### Prototype

``` c
void gclib_free(void *ptr);
```

#### Synopsis

Explicitly free a chunk of dynamically allocated memory subject to garbage collection.

#### Description

`gclib_free()` acts as a warapper around `free()` by passing the argument `ptr` to it. As such, it mirrors the behavior of this standard-library function.

#### Parameters

`ptr` - The pointer to the memory chunk that is to be freed. Note that `ptr` must have been returned by either `gclib_alloc()` or `gclib_realloc()`; any other pointer results in undefined and erroneous behavior.

#### Return Value

None.

### `gclib_collect()`

#### Prototype

``` c
void gclib_collect(void);
```

#### Synopsis

Explicitly run the garbage collector in order to free unreachable dynamically allocated memory chunks.

#### Description

`gclib_collect()` runs a standard cycle of the garbage collector in an effort to collect memory chunks allocated through `gclib_alloc()` and `gclib_realloc()`. However, note that because each generation of chunks is only swept through after a certain number of bytes have been allocated, not much will be collected if only a small amount of memory has been allocated through `gclib` functions.

#### Parameters

None.

#### Return Value

None.

### `gclib_force_collect()`

#### Prototype

``` c
void gclib_force_collect(void);
```

#### Synopsis

Forcibly run the garbage collector in order to free unreachable dynamically allocated memory chunks from all generations. **Use of this function is not recommended as it is time consuming and may interfere with the collector's natural cycles.**

#### Description

`gclib_force_collect()` runs a cycle of the garbage collector but with the condition that all generations are swept through. The result is that any chunks allocated through `gclib_alloc()` and `gclib_realloc()` that are determined to be unreachable are freed.

#### Parameters

None.

#### Return Value

None.

### `gclib_print_leaks()`

#### Prototype

``` c
void gclib_print_leaks(FILE *stream);
```

#### Synopsis

Print out any unfreed memory chunks allocated through `gclib_alloc()` and `gclib_realloc()`.

#### Description

`gclib_print_leaks()` is intended to utilize `gclib`'s tracking of memory for garbage collection as part of memory-leak detection capability. Note that throughout the duration of the program, as allocations are made, the collector may activate and free unreachable chunks. This means that the diagnosis of memory leaks may not be completely accurate. Nevertheless, it may give the caller a decent idea of where the program leaks memory. In a program that uses only the standard-library allocation functions, the following macros can be used in conjunction with `gclib_print_leaks()` to identify unfreed memory chunks (of course `gclib_init()` and `gclib_cleanup()` should be included as well).

``` c
#define malloc(size)             gclib_alloc(size, false)
#define calloc(count, size_each) gclib_alloc((count) * (size_each), true)
#define realloc(ptr, new_size)   gclib_realloc(ptr, new_size)
#define free(ptr)                gclib_free(ptr)
```

#### Parameters

`stream` - The file or output stream in which to print the found memory leaks.

#### Return Value

None.

## A Brief Note from the Author

While this project is technically considered complete, there are still a few more things that I would like to implement. Currently, sufficient time has been devoted to this project and it is in a (hopefully) functional state. In the future, should I have some time to return to this project, I will focus on:

- Refactoring the hash table and linked list that the collector uses in a more modular manner; implementing them in a more generic state and so that they are not as intertwined with the inner-workings and needs of the collector.
- Writing thorough tests for each "module" of the project (linked list, hash table, collector, etc.) to ensure they work correctly and behave as intended.

## Inspiration

Many thanks to the following resources for being the inspiration behind the core ideas of this project:

[Writing Garbage Collector in C](https://www.youtube.com/watch?v=2JgEKEd3tw8)

[Boehm-Demers-Weiser Garbage Collector](https://github.com/ivmai/bdwgc/tree/master)

[Writing a Simple Garbage Collector in C](https://maplant.com/gc.html)

[SimpleGC](https://github.com/gtoubassi/SimpleGC)
