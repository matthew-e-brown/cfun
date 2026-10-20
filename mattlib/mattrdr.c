#define _POSIX_C_SOURCE 200809L // Needed for SSIZE_MAX to exist in limits.h
#include <stdlib.h>     // NULL
#include <stddef.h>     // size_t
#include <sys/types.h>  // ssize_t
#include <string.h>     // memcpy()
#include <stdbool.h>    // bools
#include <errno.h>      // errno
#include <limits.h>     // SSIZE_MAX
#include <unistd.h>     // read

#include "mattlib.h"    // mattalloc
#include "mattrdr.h"    // this


rdr_t rdr_new(int fd, void *buffer, size_t bufsize)
{
    rdr_t r = {0};

    r.fd = fd;
    r.buffer = (unsigned char*)buffer;

    if (r.buffer == NULL) {
        r.buffer = mattalloc(bufsize);
        r.flags |= RDR_OWNED;
    }
    
    r.cur = r.end = r.buffer;
    r.bufsize = bufsize;
    
    return r;
}

void rdr_free(rdr_t *rdr)
{
    // Did we allocate this buffer, or was it given to us?
    if (rdr->flags & RDR_OWNED) {
        free(rdr->buffer);
    }
    
    rdr->buffer = rdr->cur = rdr->end = NULL;
    rdr->bufsize = 0;
    rdr->flags = 0;
    rdr->prev_errno = 0;
}

bool rdr_eof(const rdr_t *rdr)
{
    return (rdr->flags & RDR_EOF) > 0;
}

/// Wraps a call to `read(2)` with a loop to handle the case when
/// `errno == EINTR` ("interrupted by signal").
static inline ssize_t read_noeintr(int fd, void *buf, size_t n)
{
retry:;
    ssize_t nread = read(fd, buf, n);
    if (nread < 0 && errno == EINTR)
        goto retry;
    return nread;
}

ssize_t rdr_read(rdr_t *rdr, void *dst, size_t n)
{
    /*
     * Check cached error from previous read.
     */
    if (rdr->prev_errno != 0)
    {
        errno = rdr->prev_errno;
        rdr->prev_errno = 0; // Reset for next read.
        return -1;
    }
    
    /*
     * Limit `s` to be no larger than ssize_max to avoid overflow.
     */
    if (n > (size_t)SSIZE_MAX)
        n = (size_t)SSIZE_MAX;
    
    ssize_t read_total = 0;
    while (read_total < (ssize_t)n)
    {
        /*
         * `cur` <= `end`. How much space is left in the current buffer?
         * If there's nothing left, we need to refill.
         */
        size_t remaining = (size_t)(rdr->end - rdr->cur);
        if (remaining == 0)
        {
            /*
             * If we've already hit EOF before, just return.
             */
            if (rdr->flags & RDR_EOF)
                return read_total;
            
            ssize_t nread = read_noeintr(rdr->fd, rdr->buffer, rdr->bufsize);
            
            /*
             * Read has failed; did we manage to read anything so far?
             * If so, don't return the error yet; save it for next read.
             */
            if (nread < 0) {
                if (read_total > 0) {
                    rdr->prev_errno = errno;
                    return read_total;
                } else {
                    return -1;
                }
            }
            
            /*
             * EOF. There's no more to read.
             */
            if (nread == 0) {
                rdr->flags |= RDR_EOF;
                return read_total;
            }
            
            /*
             * Read successful! Update our state.
             */
            rdr->cur = rdr->buffer;
            rdr->end = rdr->buffer + nread;
            remaining = (size_t)nread;
        }
        
        /*
         * Copy all we can (min(cur_len, n)) from our buffer into the
         * destination. Then let the loop repeat, refilling the buffer if
         * needed.
         */
        size_t copy_amt = (remaining <= n) ? remaining : n;
        memcpy(dst, rdr->cur, copy_amt);
        read_total += (ssize_t)copy_amt;
        rdr->cur += copy_amt;
    }
    
    return read_total;
}
