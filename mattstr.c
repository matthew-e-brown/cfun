#include <stdlib.h>  // NULL, etc.
#include <stdio.h>   // vsnprintf
#include <stdarg.h>  // va_list stuff
#include <string.h>  // memcpy, memset, strlen, etc

#include "mattlib.h" // mattalloc, mattrealloc
#include "mattstr.h" // self

str_t strempty()
{
    return (str_t){ .str = NULL, .len = 0, .cap = 0 };
}

str_t strnew(size_t cap)
{
    char *buf = mattalloc(cap + 1);
    return (str_t){ .str = buf, .len = 0, .cap = cap };
}

void strreserve(str_t *str, size_t n)
{
    // Do we need to grow?
    if (n > 0 && str->cap < str->len + n)
    {
        // Yes we do; should we just add `n`, or double
        // the capacity?
        size_t newcap = (n > str->cap)
            ? str->len + n
            : str->cap * 2;
        str->str = mattrealloc(str->str, newcap + 1);
        str->cap = newcap;
    }
}

void strclear(str_t *str)
{
    if (str->str) {
        memset(str->str, 0, str->cap + 1);
        str->len = 0;
    }
}

str_t strfstr(const char *restrict fmt, ...)
{
    va_list args;
    char *buf = NULL;
    size_t len = 0;

    // Start our variable list of arguments after fmt.
    va_start(args, fmt);
    
    // From printf(3): "(v)snprintf does not write more than `n` bytes,"
    //   including the NUL terminator; but, if the output doesn't fit,
    //   then it returns the number of characters that would have been
    //   written, *excluding* the NUL byte.
    //
    // We take advantage of this to figure out how large of a buffer to
    // allocate for our formatted string. It means printf's parsing and
    // formatting logic is running twice, but... that's what you get
    // with libc, I guess. Blech.
    len = vsnprintf(NULL, 0, fmt, args);
    va_end(args);
    
    // Is this an empty string?
    if (len == 0)
    {
        return strempty();
    }
    
    // Now do the whole thing again after allocating a buffer. NB: it is
    // totally safe to re-use the same `va_list` as long as you va_end
    // it first: https://stackoverflow.com/a/41411702/10549827
    buf = mattalloc(len + 1);
    va_start(args, fmt);
    vsnprintf(buf, len + 1, fmt, args);
    va_end(args);

    return (str_t){ .str = buf, .len = len, .cap = len };
}


void strpush(str_t *str, char const *s)
{
    size_t slen = strlen(s);
    strreserve(str, slen);
    memcpy(str->str + str->len, s, slen);
    str->len += slen;
}

void strpushstrf(str_t *str, const char *restrict fmt, ...)
{
    va_list args;
    size_t needed_len = 0;
    
    // Check length required for format string.
    va_start(args, fmt);
    needed_len = vsnprintf(NULL, 0, fmt, args);
    va_end(args);
    
    if (needed_len == 0)
        return;
    
    // Grow if necessary:
    strreserve(str, needed_len);
    
    // Now we can format into the end of the buffer:
    va_start(args, fmt);
    vsnprintf(str->str + str->len, needed_len + 1, fmt, args);
    va_end(args);

    str->len += needed_len;
}

void strfree(str_t *str)
{
    free(str->str);
    str->str = NULL;
    str->len = 0;
    str->cap = 0;
}

/* str_t strconcat(size_t count, ...)
{
    size_t i;
    size_t total;
    size_t offset;
    va_list args;
    
    total = 0;
    va_start(args, count);
    for (i = 0; i < count; i++)
    {
        str_t *s = va_arg(args, str_t*);
        total += s->len;
    }
    va_end(args);
    
    // `total` does not include NUL byte
    char *buf = mattalloc(total + 1);
    
    offset = 0;
    va_start(args, count);
    for (i = 0; i < count; i++)
    {
        str_t *s = va_arg(args, str_t*);
        memcpy(buf + offset, s->str, s->len);
        offset += s->len;
        strfree(s);
    } 
    va_end(args);
    
    return (str_t){ .str = buf, .len = total };
} */
