
#ifndef GCLIB_COLLECTOR_H
#define GCLIB_COLLECTOR_H


#include "gclib-table.h"

typedef uintptr_t generic_ptr; // `void *` also works instead of `uintptr_t` but this is more useful for memory alignment and pointer arithmetic

extern const generic_ptr *g_stack_start_ptr;
extern const generic_ptr *g_stack_end_ptr;
extern const generic_ptr *g_data_start_ptr;
extern const generic_ptr *g_data_end_ptr;

/* Run the garbage collector with the option to sweep through all generations. */
void collector_run(bool all_gens);

/* Mark all `chunk_nodes` within the generations to collect as reachable that have references between `*start` and `*end`. */
void collector_mark(bool to_collect[GENERATIONS], const generic_ptr *start, const generic_ptr *end);

/* Sweep through the given generations and remove any `chunk_nodes` determined as unreachable. */
void collector_sweep(bool to_collect[GENERATIONS]);


#endif // GCLIB_COLLECTOR_H
