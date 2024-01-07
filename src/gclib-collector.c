
#include "gclib-collector.h"

const generic_ptr *g_stack_start_ptr; // Address of the stack frame after the last user-called function (the stack is iterated through in a top-down manner)
const generic_ptr *g_stack_end_ptr;   // Address of the stack frame of `main()` (the stack is iterated through in a top-down manner)
const generic_ptr *g_data_start_ptr;  // Address of the start of the initialized data segment
const generic_ptr *g_data_end_ptr;    // Address of the end of the BSS segment

// TODO: Implement collector: mark reachable chunks in hash table and then sweep to free unreachable chunks
// TODO: generations MUST be collected in reverse order to avoid any issues with promotions (to higher generations)
// TODO: store the generations to collect in an array so they can be marked and swept with one function call
// TODO: `gclib_alloc()` calls the collector so call `__builtin_frame_address(1)` from the collector to get `g_stack_end_ptr`

void collector_run(bool all_gens)
{
    bool to_collect[GENERATIONS];
    uint8_t gen;

    for (gen = 0; gen < GENERATIONS; gen++)
    {
        if (all_gens || g_alloced_bytes[gen] > MAX_ALLOCED_BYTES)
        {
            to_collect[gen] = true;
        }
        else
        {
            to_collect[gen] = false;
        }
    }

    // TODO: mark-and-sweep

    return;
}

void collector_mark(bool to_collect[GENERATIONS], const generic_ptr *start, const generic_ptr *end)
{
    const generic_ptr *ptr;

    for (ptr = start; ptr < end; ptr++)
    {
        ; // TODO
    }

    return;
}

void collector_sweep(bool to_collect[GENERATIONS])
{
    // TODO

    return;
}
