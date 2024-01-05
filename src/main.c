
#include "gc.h"

int main(void)
{
    gc_init();

    char *p = gc_alloc(10);
    gc_free(p);

    gc_cleanup();
}
