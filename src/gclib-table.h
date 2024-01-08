
#ifndef GCLIB_TABLE_H_
#define GCLIB_TABLE_H_


#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/* A node in a linked list containing information about an allocated memory chunk. */
typedef struct chunk_node
{
    void *ptr;
    size_t size;
    bool reachable;
    struct chunk_node *next;
} chunk_node;

#define HASH_TABLE_SIZE 1024
#define GENERATIONS 3
#define MAX_ALLOCED_BYTES (1e+9) // maximum size of all allocations (per generation) before running the collector; 1GB may not be optimal for actual use
extern chunk_node *g_hash_table[GENERATIONS][HASH_TABLE_SIZE];
extern size_t g_alloced_bytes[GENERATIONS];

/* Insert a `chunk_node` containing `ptr` and `size` into `g_hash_table`. */
void table_insert(void *ptr, size_t size);

/* Remove all `chunk_nodes` containing `ptr` across all generations from `g_hash_table`. */
void table_remove(void *ptr);

/* Print all entries in `g_hash_table` to `stream`. */
void table_print(FILE *stream);

/* Free all `chunk_node` linked lists residing in `g_hash_table`. */
void table_free(void);

/* Hash `ptr` and return an number suitable for indexing into `g_hash_table`. */
uint16_t table_hash_ptr(const void *ptr);

/* Link the `chunk_node` `*p_node` to the beginning of the linked list starting at `g_hash_table[gen][idx]`. */
void list_link(uint8_t gen, uint16_t idx, chunk_node *p_node);

/* Unlink (but do not free) the `chunk_node` `*p_node`from the linked list starting at `g_hash_table[gen][idx]`. */
void list_unlink(uint8_t gen, uint16_t idx, chunk_node *p_node, chunk_node *p_previous);


#endif  // GCLIB_TABLE_H_
