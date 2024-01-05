
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "gc.h"

static uint16_t hash_ptr(void *ptr);
static void table_remove(void *ptr);
static void table_insert(void *ptr, size_t size);

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

/* A node in a linked list containing information about a memory chunk allocated by `malloc()` through `gc_alloc()`. */
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
chunk_node *g_hash_table[GENERATIONS][HASH_TABLE_SIZE + 1];
size_t g_alloced_bytes[GENERATIONS]; // total size of all allocations for each generation

typedef uintptr_t generic_ptr; // `void *` also works instead of `uintptr_t` but this is more useful for memory alignment
#define WORD sizeof(generic_ptr)

extern char etext, edata, end; // end of text segment, initialized data segment, and BSS; all provided by the linker; https://linux.die.net/man/3/etext

bool g_init = false;
bool g_cleanup = false;

generic_ptr g_stack_start_ptr; // Address of the stack frame of `gc_alloc()` (the stack is iterated through in a top-down manner)
generic_ptr g_stack_end_ptr;   // Address of the stack frame of `main()` (the stack is iterated through in a top-down manner)
generic_ptr g_data_start_ptr;  // Address of the start of the initialized data segment
generic_ptr g_data_end_ptr;    // Address of the end of the BSS segment

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

    // TODO: free linked lists in hash table

    g_cleanup = true;
    debug_log("%s\n", "GC cleaned up");

    return;
}

bool gc_ready(void)
{
    return g_init && !g_cleanup;
}

void *gc_alloc(size_t size)
{
    void *ptr;
    uint8_t gen;

    if (!gc_ready())
    {
        debug_log("%s\n", "GC is not ready");

        return NULL;
    }

    for (gen = GENERATIONS - 1; 0 <= gen && gen < GENERATIONS; gen--)
    {
        if (g_alloced_bytes[gen] > MAX_ALLOCED_BYTES)
        {
            ; // TODO: run collector with `gen`
        }
    }

    ptr = malloc(size);
    if (ptr == NULL)
    {
        debug_log("%s\n", "`malloc()` failed");

        for (gen = GENERATIONS - 1; 0 <= gen && gen < GENERATIONS; gen--)
        {
            ; // TODO: run collector with `gen`
        }

        ptr = malloc(size);
        if (ptr == NULL)
        {
            debug_log("%s\n", "`malloc()` failed again");

            return NULL; // there doesn't seem to be any reason to try `malloc()` again
        }
    }

    table_insert(ptr, size);
    debug_log("alloced chunk - ptr: %p, size: %zu\n", ptr, size);

    return ptr;
}

void gc_free(void *ptr)
{
    if (!gc_ready())
    {
        debug_log("%s\n", "GC is not ready");

        return;
    }

    table_remove(ptr);
    debug_log("freed chunk - ptr: %p\n", ptr);

    return;
}

void gc_force_collect()
{
    uint8_t gen;

    if (!gc_ready())
    {
        debug_log("%s\n", "GC is not ready");

        return;
    }

    for (gen = GENERATIONS - 1; 0 <= gen && gen < GENERATIONS; gen--)
    {
        ; // TODO: run collector with `gen`
    }

    return;
}

/* Insert a `chunk_node` containing `ptr` and `size` into `g_hash_table`. */
static void table_insert(void *ptr, size_t size)
{
    uint16_t idx;
    chunk_node *p_node;

    p_node = malloc(sizeof(chunk_node)); // using `malloc()` for internal memory needs shouldn't interfere with the collector
    if (p_node == NULL)
    {
        debug_log("%s\n", "`malloc()` could not allocate new node");

        return;
    }

    p_node->ptr = ptr;
    p_node->size = size;
    p_node->reachable = false; // set to `true` during the mark phase of the collector

    idx = hash_ptr(ptr);
    p_node->next = g_hash_table[0][idx];
    g_hash_table[0][idx] = p_node; // insert into generation 0 since it's a new allocation
    g_alloced_bytes[0] += size;

    debug_log("inserted chunk (gen 0) - ptr: %p, size: %zu\n", ptr, size);

    return;
}

/* Remove all `chunk_nodes` containing `ptr`. Note that this combs through all generations to look for the corresponding node. */
static void table_remove(void *ptr)
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
            if (p_current->ptr == ptr)
            {
                size = p_current->size;

                if (p_previous == NULL) // `p_current` is the head of the list
                {
                    g_hash_table[gen][idx] = p_current->next;
                    free(p_current);
                    p_current = g_hash_table[gen][idx]; // the new head is now the next node in the list
                }
                else
                {
                    p_previous->next = p_current->next;
                    free(p_current);
                    p_current = p_previous->next;
                }

                g_alloced_bytes[gen] -= size;

                debug_log("removed chunk (gen %d) - ptr: %p, size: %zu\n", gen, ptr, size);
            }
            else
            {
                p_previous = p_current;
                p_current = p_current->next;
            }
        }
        while (p_current != NULL);
    }

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
