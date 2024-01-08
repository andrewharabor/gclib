
#include "gclib-table.h"

chunk_node *g_hash_table[GENERATIONS][HASH_TABLE_SIZE]; // hash table containing linked lists of `chunk_node`s represented user-allocated blocks
size_t g_alloced_bytes[GENERATIONS];                    // total size (in bytes) of all allocations for each generation

void table_insert(void *ptr, size_t size)
{
    uint16_t idx;
    chunk_node *p_node;

    // Allocate and initialze new `chunk_node`
    p_node = malloc(sizeof(chunk_node)); // using `malloc()` for internal memory needs shouldn't interfere with the collector
    if (p_node == NULL)
    {
        return;
    }

    p_node->ptr = ptr;
    p_node->size = size;
    p_node->reachable = false; // handled during the mark phase of the collector

    idx = table_hash_ptr(ptr);
    list_link(0, idx, p_node); // insert into generation 0 since it's a new allocation

    return;
}

void table_remove(void *ptr)
{
    uint16_t idx;
    uint8_t gen;
    chunk_node *p_current, *p_previous;

    idx = table_hash_ptr(ptr);
    for (gen = 0; gen < GENERATIONS; gen++)
    {
        p_previous = NULL;
        p_current = g_hash_table[gen][idx];
        while (p_current != NULL)
        {
            if (p_current->ptr == ptr) // matches the pointer to remove
            {
                list_unlink(gen, idx, p_current, p_previous);
                free(p_current);

                if (p_previous == NULL) // removed head of list so now it's empty
                {
                    continue;
                }

                p_current = p_previous->next;
            }
            else
            {
                p_previous = p_current;
                p_current = p_current->next;
            }
        }
    }

    return;
}

void table_print(FILE *stream)
{
    uint16_t idx;
    uint8_t gen;
    uint32_t count;
    size_t bytes;
    chunk_node *p_node;

    count = bytes = 0;
    for (gen = 0; gen < GENERATIONS; gen++)
    {
        fprintf(stream, "Generation %d:\n\n", gen);

        for (idx = 0; idx < HASH_TABLE_SIZE; idx++)
        {
            for (p_node = g_hash_table[gen][idx]; p_node != NULL; p_node = p_node->next)
            {
                count++;
                bytes += p_node->size;

                fprintf(stream, "\tUnfreed block:\n\t\tAddress: %p\n\t\tSize: %zu (bytes)\n\n", p_node->ptr, p_node->size);
            }
        }

        fprintf(stream, "\n");
    }

    fprintf(stream, "TOTAL:\n\tUnfreed chunks: %d\n\tUnfreed bytes: %zu\n", count, bytes);

    return;
}

void table_free(void)
{
    uint16_t idx;
    uint8_t gen;
    chunk_node *p_current, *p_tmp;

    for (gen = 0; gen < GENERATIONS; gen++)
    {
        for (idx = 0; idx < HASH_TABLE_SIZE; idx++)
        {
            // Iterate through the linked list and free each node
            p_current = g_hash_table[gen][idx];
            while (p_current != NULL)
            {
                p_tmp = p_current;
                p_current = p_current->next;
                free(p_tmp->ptr);
                free(p_tmp);
            }

            g_hash_table[gen][idx] = NULL;
        }
    }

    return;
}

uint16_t table_hash_ptr(void *ptr)
{
    // From https://stackoverflow.com/a/12996028, which is based on https://xorshift.di.unimi.it/splitmix64.c

    uint64_t val = (uint64_t) ptr;

    val = (val ^ (val >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
    val = (val ^ (val >> 27)) * UINT64_C(0x94d049bb133111eb);
    val = val ^ (val >> 31);

    return val % HASH_TABLE_SIZE;
}

void list_link(uint8_t gen, uint16_t idx, chunk_node *p_node)
{
    p_node->next = g_hash_table[gen][idx];
    g_hash_table[gen][idx] = p_node;

    g_alloced_bytes[gen] += p_node->size;

    return;
}

void list_unlink(uint8_t gen, uint16_t idx, chunk_node *p_node, chunk_node *p_previous)
{
    size_t size;

    size = p_node->size;

    if (p_previous == NULL) // `p_node` is the head of the list
    {
        g_hash_table[gen][idx] = p_node->next;
    }
    else
    {
        p_previous->next = p_node->next;
    }

    g_alloced_bytes[gen] -= size;

    return;
}
