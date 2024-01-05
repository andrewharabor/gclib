
#include "gc.h"

int main(void)
{
    int *p;

    gc_init();

    p = gc_alloc(sizeof(int), false);
    p = gc_realloc(p, sizeof(int) * 2);

    gc_free(p);

    gc_cleanup();
}
