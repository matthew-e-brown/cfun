#include "mattlib.h"

#include <stddef.h>     // size_t
#include <stdlib.h>     // malloc, abort
#include <string.h>     // memset
#include <unistd.h>     // write

/// Error message for `mattalloc()` and `mattrealloc()`.
static const char allocfail[] = ANSI_RED("Internal error")": memory allocation failed\n";

void *mattalloc(size_t amt)
{
    void *p = malloc(amt);

    if (p == NULL)
    {
        write(STDERR_FILENO, allocfail, sizeof(allocfail));
        abort();
    }
    
    memset(p, 0, amt);
    return p;
}

void *mattrealloc(void *ptr, size_t newamt)
{
    ptr = realloc(ptr, newamt);
    
    if (ptr == NULL)
    {
        write(STDERR_FILENO, allocfail, sizeof(allocfail));
        abort();
    }
    
    return ptr;
}
