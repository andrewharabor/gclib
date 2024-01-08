
#include "gclib-collector.h"

const void **g_stack_start_ptr; // Address of the stack frame after the last user-called function (the stack is iterated through in a top-down manner)
const void **g_stack_end_ptr;   // Address of the stack frame of `main()` (the stack is iterated through in a top-down manner)
const void **g_data_start_ptr;  // Address of the start of the initialized data segment
const void **g_data_end_ptr;    // Address of the end of the BSS segment

void collector_run(bool all_gens)
{
    g_stack_start_ptr = (const void **) __builtin_frame_address(1); // address of stack frame of caller of `collecter_run()` (should be a `gclib` function called by the user); https://gcc.gnu.org/onlinedocs/gcc/Return-Address.html#index-_005f_005fbuiltin_005fframe_005faddress

    bool to_collect[GENERATIONS];
    uint8_t gen;

    // See which generations need to be collected
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

    collector_mark(to_collect, g_stack_start_ptr, g_stack_end_ptr);
    collector_mark(to_collect, g_data_start_ptr, g_data_end_ptr);
    collector_sweep(to_collect);

    return;
}

void collector_mark(bool to_collect[GENERATIONS], const void **start, const void **end)
{
    const void **ptr;
    uint16_t idx;
    uint8_t gen;
    chunk_node *p_current;

    // Treat each block of 8-bytes as a pointer (that could potentially point to a user-allocated chunk)
    for (ptr = start; ptr < end; ptr++)
    {
        idx = table_hash_ptr(*ptr);
        for (gen = 0; gen < GENERATIONS; gen++)
        {
            if (to_collect[gen])
            {
                // Iterate through linked list and mark reachable chunks
                for (p_current = g_hash_table[gen][idx]; p_current != NULL; p_current = p_current->next)
                {
                    // Incrementing `p_current->ptr` (which is `void *`) below only works because with GCC, `sizeof(void)` is 1
                    // Casting to `char *` and then `void *` is technically more correct (and portable) but makes the code harder to understand
                    if (p_current->ptr <= *ptr && *ptr < p_current->ptr + p_current->size) // `ptr` is the address of a reference to `p_current`
                    {
                        p_current->reachable = true;

                        // same issue with incrementing a `void *` as above
                        collector_mark(to_collect, p_current->ptr, p_current->ptr + p_current->size); // since this chunk is reachable, any chunk it references is also reachable
                    }
                }
            }
        }
    }

    return;
}

void collector_sweep(bool to_collect[GENERATIONS])
{
    uint16_t idx;
    uint8_t gen;
    chunk_node *p_current, *p_previous;

    for (gen = GENERATIONS - 1; 0 <= gen && gen < GENERATIONS; gen--) // generations MUST be collected in reverse order to avoid any issues with promotions to higher generations
    {
        if (to_collect[gen])
        {
            for (idx = 0; idx < HASH_TABLE_SIZE; idx++)
            {
                p_previous = NULL;
                p_current = g_hash_table[gen][idx];
                while (p_current != NULL)
                {
                    if (p_current->reachable) // promote to next generation
                    {
                        p_current->reachable = false; // set up for next mark-cycle

                        if (gen < GENERATIONS - 1)
                        {
                            list_unlink(gen, idx, p_current, p_previous);
                            list_link(gen + 1, idx, p_current);

                            if (p_previous == NULL) // unlinked the head of the list
                            {
                                p_current = g_hash_table[gen][idx];
                            }
                            else
                            {
                                p_current = p_previous->next;
                            }
                        }
                        else // already in highest generation so can't promote
                        {
                            p_previous = p_current;
                            p_current = p_current->next;
                        }
                    }
                    else // unreachable chunk; free it
                    {
                        list_unlink(gen, idx, p_current, p_previous);
                        free(p_current->ptr);
                        free(p_current);

                        if (p_previous == NULL) // freed the head of the list
                        {
                            p_current = g_hash_table[gen][idx];
                        }
                        else
                        {
                            p_current = p_previous->next;
                        }
                    }
                }
            }
        }
    }

    return;
}
