
#include <stdarg.h>
#include <stdio.h>

#include "gclib.h"
#include "gclib-collector.h"

extern char etext, edata, end; // end of text segment, initialized data segment, and BSS; all provided by the linker; https://linux.die.net/man/3/etext

static bool g_init = false;
static bool g_cleanup = false;

void gclib_init(void)
{
    if (g_init || g_cleanup)
    {
        return;
    }

    // Getting pointers to the stack and data segments like this is hackish but there seems to be no better way.
    // Not to mention that this likely only works on x86-64 Linux AND when complied with GCC.
    g_stack_end_ptr = (const void **) __builtin_frame_address(1); // address of stack frame of caller of `gclib_init()` (should be `main()`); https://gcc.gnu.org/onlinedocs/gcc/Return-Address.html#index-_005f_005fbuiltin_005fframe_005faddress
    g_data_start_ptr = (const void **) &etext;
    g_data_end_ptr = (const void **) &end;

    g_init = true;

    return;
}

void gclib_cleanup(void)
{
    if (!gclib_ready())
    {
        return;
    }

    table_free();

    g_cleanup = true;

    return;
}

bool gclib_ready(void)
{
    return g_init && !g_cleanup;
}

void *gclib_alloc(size_t size, bool zeroed)
{
    void *ptr;

    if (!gclib_ready())
    {
        return NULL;
    }

    collector_run(false);

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
        collector_run(true); // likely not to improve the situation but not much else we can do

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
            return NULL;
        }
    }

    if (size == 0)
    {
        free(ptr); // `malloc()`/`calloc()` may not always return `NULL` in this case

        return NULL;
    }

    table_insert(ptr, size);

    return ptr;
}

void *gclib_realloc(void *ptr, size_t new_size)
{
    void *new_ptr;

    if (!gclib_ready())
    {
        return NULL;
    }

    collector_run(false);

    new_ptr = realloc(ptr, new_size);

    // Handle any errors from `realloc()`
    if (new_ptr == NULL && new_size != 0) // if `new_size` is zero, `realloc()` returning `NULL` is actually the intended effect
    {
        collector_run(true); // likely not to improve the situation but not much else we can do

        new_ptr = realloc(ptr, new_size);
        if (new_ptr == NULL)
        {
            return NULL;
        }
    }

    if (ptr == NULL) // `realloc(NULL, new_size)` acts as `malloc(new_size)`
    {
        if (new_size == 0)
        {
            free(new_ptr); // `realloc()` may not always return `NULL` in this case

            return NULL;
        }

        table_insert(new_ptr, new_size);

        return new_ptr;
    }

    if (new_size == 0) // `realloc(ptr, 0)` acts as `free(ptr)` and returns `NULL`
    {
        table_remove(ptr);

        return NULL;
    }

    table_remove(ptr);               // `realloc()` returns the same pointer passed to it after resizing if possible,
    table_insert(new_ptr, new_size); // which means the removal and insertion is inefficient

    return new_ptr;
}

void gclib_free(void *ptr)
{
    if (!gclib_ready())
    {
        return;
    }

    free(ptr);

    if (ptr != NULL) // `gclib_alloc()` and `gclib_realloc()` don't add null-pointers to the hash table
    {
        table_remove(ptr);
    }

    return;
}

void gclib_collect(void)
{
    if (!gclib_ready())
    {
        return;
    }

    collector_run(false);

    return;
}

void gclib_force_collect(void)
{
    if (!gclib_ready())
    {
        return;
    }

    collector_run(true);

    return;
}

void gclib_print_leaks(FILE *stream)
{
    if (!gclib_ready())
    {
        return;
    }

    table_print(stream);

    return;
}
