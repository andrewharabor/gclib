
#include "gclib.h"

int main(void)
{
    int *p;

    gclib_init();

    p = gclib_alloc(sizeof(int), false);
    p = gclib_realloc(p, sizeof(int) * 2);

    gclib_print_leaks(stdout);

    gclib_free(p);

    gclib_cleanup();
}
