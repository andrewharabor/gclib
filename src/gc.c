
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "gc.h"

static void hash_table_insert(void *ptr, size_t size);
static void hash_table_remove(void *ptr);
static void hash_table_free(void);
static uint16_t hash_ptr(void *ptr);

// TODO: REMOVE DEBUG LOGGING (EVERYWHERE)
#define DEBUG 1
#define debug_log(format, ...)                                                                     \
        do                                                                                         \
        {                                                                                          \
            if (DEBUG)                                                                             \
            {                                                                                      \
                fprintf(stderr, "%s:%d:%s(): " format, __FILE__, __LINE__, __func__, __VA_ARGS__); \
            }                                                                                      \
        } while (0)

/* A node in a linked list containing information about a memory chunk allocated by `malloc()`/`calloc()` through `gc_alloc()`. */
typedef struct chunk_node
{
    void *ptr;
    size_t size;
    bool reachable;
    struct chunk_node *next;
} chunk_node;

#define HASH_TABLE_SIZE 1024
#define GENERATIONS 3
#define MAX_ALLOCED_BYTES (1e+9) // maximum size of all allocations (per generation) before running the collector; 1GB may be too conservative for actual use
static chunk_node *g_hash_table[GENERATIONS][HASH_TABLE_SIZE + 1];
static size_t g_alloced_bytes[GENERATIONS]; // total size of all allocations for each generation

typedef uintptr_t generic_ptr; // `void *` also works instead of `uintptr_t` but this is more useful for memory alignment
#define WORD sizeof(generic_ptr)

extern char etext, edata, end; // end of text segment, initialized data segment, and BSS; all provided by the linker; https://linux.die.net/man/3/etext

static bool g_init = false;
static bool g_cleanup = false;

static generic_ptr g_stack_start_ptr; // Address of the stack frame of `gc_alloc()` (the stack is iterated through in a top-down manner)
static generic_ptr g_stack_end_ptr;   // Address of the stack frame of `main()` (the stack is iterated through in a top-down manner)
static generic_ptr g_data_start_ptr;  // Address of the start of the initialized data segment
static generic_ptr g_data_end_ptr;    // Address of the end of the BSS segment

// TODO: Implement collector: mark reachable chunks in hash table and then sweep to free unreachable chunks
// FIXME: generations MUST be collected in reverse order to avoid any issues with promotions (to higher generations)
// FIXME: `gc_alloc()` calls the collector so call `__builtin_frame_address(1)` from the collector to get `g_stack_end_ptr`

void gc_init(void)
{
    if (g_init || g_cleanup)
    {
        debug_log("%s\n", "GC is not ready");

        return;
    }

    // Getting pointers to the stack and data segments like this is hackish but there seems to be no better way.
    // Not to mention that this likely only works on x86-64 Linux AND when complied with GCC.
    g_stack_end_ptr = ((generic_ptr) __builtin_frame_address(1)) + 1; // address of stack frame of caller of `gc_init()` (should be `main()`); https://gcc.gnu.org/onlinedocs/gcc/Return-Address.html#index-_005f_005fbuiltin_005fframe_005faddress
    g_data_start_ptr = (generic_ptr) &etext;
    g_data_end_ptr = (generic_ptr) &end;
    debug_log("stack base: %p\n", g_stack_end_ptr);
    debug_log("initialized data start: %p\n", g_data_start_ptr);
    debug_log("BSS end: %p\n", g_data_end_ptr);

    g_init = true;
    debug_log("%s\n", "GC initialized");

    return;
}

void gc_cleanup(void)
{
    if (!gc_ready())
    {
        debug_log("%s\n", "GC is not ready");

        return;
    }

    hash_table_free();

    g_cleanup = true;
    debug_log("%s\n", "GC cleaned up");

    return;
}

bool gc_ready(void)
{
    return g_init && !g_cleanup;
}

void *gc_alloc(size_t size, bool zeroed)
{
    void *ptr;
    uint8_t gen;

    if (!gc_ready())
    {
        debug_log("%s\n", "GC is not ready");

        return NULL;
    }

    gc_collect();

    if (zeroed)
    {
        ptr = calloc(1, size);
    }
    else
    {
        ptr = malloc(size);
    }

    // Handle any errors from `malloc()`/`calloc()`
    if (ptr == NULL && size != 0)
    {
        debug_log("%s\n", "`malloc()`/`calloc()` failed");

        gc_force_collect(); // likely not to improve the situation but not much else we can do

        if (zeroed)
        {
            ptr = calloc(1, size);
        }
        else
        {
            ptr = malloc(size);
        }

        if (ptr == NULL)
        {
            debug_log("%s\n", "`malloc()`/`calloc()` failed again");

            return NULL;
        }
    }

    if (size == 0)
    {
        debug_log("alloced nothing - size: %zu\n", size);

        return NULL;
    }

    hash_table_insert(ptr, size);
    debug_log("alloced chunk - ptr: %p, size: %zu\n", ptr, size);

    return ptr;
}

void *gc_realloc(void *ptr, size_t new_size)
{
    void *new_ptr;

    if (!gc_ready())
    {
        debug_log("%s\n", "GC is not ready");

        return NULL;
    }

    gc_collect();

    new_ptr = realloc(ptr, new_size);

    // Handle any errors from `realloc()`
    if (new_ptr == NULL && new_size != 0) // if `new_size` is zero, `realloc()` returning `NULL` is actually the intended effect
    {
        debug_log("%s\n", "`realloc()` failed");

        gc_force_collect(); // likely not to improve the situation but not much else we can do

        new_ptr = realloc(ptr, new_size);
        if (new_ptr == NULL)
        {
            debug_log("%s\n", "`realloc()` failed again");

            return NULL;
        }
    }

    if (ptr == NULL) // `realloc(NULL, new_size)` acts as `malloc(new_size)`
    {
        if (new_size == 0)
        {
            debug_log("alloced nothing - ptr: %p, size: %zu\n", ptr, new_size);

            return NULL;
        }

        hash_table_insert(new_ptr, new_size);
        debug_log("alloced chunk - ptr: %p, size: %zu\n", new_ptr, new_size);

        return new_ptr;
    }

    if (new_size == 0) // `realloc(ptr, 0)` acts as `free(ptr)` and returns `NULL`
    {
        hash_table_remove(ptr);
        debug_log("freed chunk - ptr: %p\n", ptr);

        return NULL;
    }

    hash_table_remove(ptr);
    hash_table_insert(new_ptr, new_size);
    debug_log("realloced chunk - ptr: %p, new ptr: %p, new size: %zu\n", ptr, new_ptr, new_size);

    return new_ptr;
}

void gc_free(void *ptr)
{
    if (!gc_ready())
    {
        debug_log("%s\n", "GC is not ready");

        return;
    }

    hash_table_remove(ptr);
    debug_log("freed chunk - ptr: %p\n", ptr);

    return;
}

void gc_collect(void)
{
    uint8_t gen;

    if (!gc_ready())
    {
        debug_log("%s\n", "GC is not ready");

        return;
    }

    // Note that generations MUST be collected in reverse order to avoid any issues with promoting chunks to higher generations
    for (gen = GENERATIONS - 1; 0 <= gen && gen < GENERATIONS; gen--)
    {
        if (g_alloced_bytes[gen] > MAX_ALLOCED_BYTES)
        {
            ; // TODO: run collector with `gen`
        }
    }

    return;
}

void gc_force_collect(void)
{
    uint8_t gen;

    if (!gc_ready())
    {
        debug_log("%s\n", "GC is not ready");

        return;
    }

    // Run the collector, regardless of how many bytes are allocated for each generation
    for (gen = GENERATIONS - 1; 0 <= gen && gen < GENERATIONS; gen--)
    {
        ; // TODO: run collector with `gen`
    }

    return;
}

/* Insert a `chunk_node` containing `ptr` and `size` into `g_hash_table`. */
static void hash_table_insert(void *ptr, size_t size)
{
    uint16_t idx;
    chunk_node *p_node;

    // Allocate and initialze new `chunk_node`
    p_node = malloc(sizeof(chunk_node)); // using `malloc()` for internal memory needs shouldn't interfere with the collector
    if (p_node == NULL)
    {
        debug_log("%s\n", "`malloc()` could not allocate new node");

        return;
    }

    p_node->ptr = ptr;
    p_node->size = size;
    p_node->reachable = false; // set to `true` during the mark phase of the collector

    // Insert as new head of linked list in the hash table
    idx = hash_ptr(ptr);
    p_node->next = g_hash_table[0][idx];
    g_hash_table[0][idx] = p_node; // insert into generation 0 since it's a new allocation
    g_alloced_bytes[0] += size;

    debug_log("inserted chunk (gen 0) - ptr: %p, size: %zu\n", ptr, size);

    return;
}

/* Remove all `chunk_nodes` containing `ptr`. Note that this combs through all generations to look for the corresponding node. */
static void hash_table_remove(void *ptr)
{
    uint16_t idx;
    uint8_t gen;
    size_t size;
    chunk_node *p_current, *p_previous;

    idx = hash_ptr(ptr);
    for (gen = 0; gen < GENERATIONS; gen++)
    {
        p_previous = NULL;
        p_current = g_hash_table[gen][idx];
        if (p_current == NULL) // no list in this generation for this hash value
        {
            continue;
        }

        do
        {
            if (p_current->ptr == ptr) // matches the pointer to remove
            {
                size = p_current->size;

                if (p_previous == NULL) // `p_current` is the head of the list
                {
                    // Set the next node as the head and free the current one
                    g_hash_table[gen][idx] = p_current->next;
                    free(p_current);
                    p_current = g_hash_table[gen][idx];
                }
                else
                {
                    // Skip over the current node and free it
                    p_previous->next = p_current->next;
                    free(p_current);
                    p_current = p_previous->next;
                }

                g_alloced_bytes[gen] -= size;

                debug_log("removed chunk (gen %d) - ptr: %p, size: %zu\n", gen, ptr, size);
            }
            else
            {
                // Increment the previous and current nodes
                p_previous = p_current;
                p_current = p_current->next;
            }
        }
        while (p_current != NULL);
    }

    return;
}

/* Free all `chunk_node` linked lists residing in `g_hash_table`. */
static void hash_table_free(void)
{
    uint16_t idx;
    uint8_t gen;
    chunk_node *p_current, *p_tmp;

    for (gen = 0; gen < GENERATIONS; gen++)
    {
        for (idx = 0; idx < HASH_TABLE_SIZE; idx++)
        {
            // Iterate through linked list and free each node
            p_current = g_hash_table[gen][idx];
            while (p_current != NULL)
            {
                p_tmp = p_current;
                p_current = p_current->next;
                p_tmp->next = NULL;
                free(p_tmp);
            }

            g_hash_table[gen][idx] = NULL;
        }
    }

    debug_log("%s\n", "cleaned up hash table");

    return;
}

/* Hash `ptr` and return an number suitable for indexing into `g_hash_table`. */
static uint16_t hash_ptr(void *ptr)
{
    // From https://stackoverflow.com/a/12996028, which is based on https://xorshift.di.unimi.it/splitmix64.c

    uint64_t val = (uint64_t) ptr;

    val = (val ^ (val >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
    val = (val ^ (val >> 27)) * UINT64_C(0x94d049bb133111eb);
    val = val ^ (val >> 31);

    return val % HASH_TABLE_SIZE;
}
