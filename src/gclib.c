
#include <stdarg.h>
#include <stdio.h>

#include "gclib.h"
#include "gclib-collector.h"

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

extern char etext, edata, end; // end of text segment, initialized data segment, and BSS; all provided by the linker; https://linux.die.net/man/3/etext

static bool g_init = false;
static bool g_cleanup = false;

void gclib_init(void)
{
    if (g_init || g_cleanup)
    {
        debug_log("%s\n", "GC is not ready");

        return;
    }

    // Getting pointers to the stack and data segments like this is hackish but there seems to be no better way.
    // Not to mention that this likely only works on x86-64 Linux AND when complied with GCC.
    g_stack_end_ptr = ((const generic_ptr *) __builtin_frame_address(1)) + 1; // address of stack frame of caller of `gclib_init()` (should be `main()`); https://gcc.gnu.org/onlinedocs/gcc/Return-Address.html#index-_005f_005fbuiltin_005fframe_005faddress
    g_data_start_ptr = (const generic_ptr *) &etext;
    g_data_end_ptr = (const generic_ptr *) &end;
    debug_log("stack base: %p\n", g_stack_end_ptr);
    debug_log("initialized data start: %p\n", g_data_start_ptr);
    debug_log("BSS end: %p\n", g_data_end_ptr);

    g_init = true;
    debug_log("%s\n", "GC initialized");

    return;
}

void gclib_cleanup(void)
{
    if (!gclib_ready())
    {
        debug_log("%s\n", "GC is not ready");

        return;
    }

    table_free();

    g_cleanup = true;
    debug_log("%s\n", "GC cleaned up");

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
        debug_log("%s\n", "GC is not ready");

        return NULL;
    }

    // TODO: run collector

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

        // TODO: run collector for all generations // likely not to improve the situation but not much else we can do

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

        free(ptr); // `malloc()`/`calloc()` may not always return `NULL` in this case

        return NULL;
    }

    table_insert(ptr, size);
    debug_log("alloced chunk - ptr: %p, size: %zu\n", ptr, size);

    return ptr;
}

void *gclib_realloc(void *ptr, size_t new_size)
{
    void *new_ptr;

    if (!gclib_ready())
    {
        debug_log("%s\n", "GC is not ready");

        return NULL;
    }

    // TODO: run collector

    new_ptr = realloc(ptr, new_size);

    // Handle any errors from `realloc()`
    if (new_ptr == NULL && new_size != 0) // if `new_size` is zero, `realloc()` returning `NULL` is actually the intended effect
    {
        debug_log("%s\n", "`realloc()` failed");

        // TODO: run collector for all generations // likely not to improve the situation but not much else we can do

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

            free(new_ptr); // `malloc()`/`calloc()` may not always return `NULL` in this case

            return NULL;
        }

        table_insert(new_ptr, new_size);
        debug_log("alloced chunk - ptr: %p, size: %zu\n", new_ptr, new_size);

        return new_ptr;
    }

    if (new_size == 0) // `realloc(ptr, 0)` acts as `free(ptr)` and returns `NULL`
    {
        table_remove(ptr);
        debug_log("freed chunk - ptr: %p\n", ptr);

        return NULL;
    }

    table_remove(ptr);
    table_insert(new_ptr, new_size);
    debug_log("realloced chunk - ptr: %p, new ptr: %p, new size: %zu\n", ptr, new_ptr, new_size);

    return new_ptr;
}

void gclib_free(void *ptr)
{
    if (!gclib_ready())
    {
        debug_log("%s\n", "GC is not ready");

        return;
    }

    table_remove(ptr);
    debug_log("freed chunk - ptr: %p\n", ptr);

    return;
}

void gclib_collect(void)
{
    if (!gclib_ready())
    {
        debug_log("%s\n", "GC is not ready");

        return;
    }

    // TODO: run the collector

    return;
}

void gclib_force_collect(void)
{
    if (!gclib_ready())
    {
        debug_log("%s\n", "GC is not ready");

        return;
    }

    // TODO: run the collector for all generations

    return;
}

void gclib_print_leaks(FILE *stream)
{
    if (!gclib_ready())
    {
        debug_log("%s\n", "GC is not ready");

        return;
    }

    table_print(stream);

    return;
}
