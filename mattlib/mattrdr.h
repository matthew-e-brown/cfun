#ifndef MATTRDR
#define MATTRDR

#include <stddef.h>     // size_t
#include <stdint.h>     // uint_least8_t
#include <stdbool.h>    // bools
#include <sys/types.h>  // ssize_t

/// A reader that buffers reading from a file descriptor.
typedef struct rdr {
    int fd;
    
    /// Saved `errno` from a previously failed read operation.
    ///
    /// When a read operation fails, it's possible that some of the output
    /// buffer has already been written. In this case, the operation is
    /// reported as a successful read, returning the number of bytes that have
    /// already been copied to the output buffer. Then, the next read will
    /// report this error just like a call to `read`. 
    int prev_errno;
    
    /// Flags.
    uint_least8_t flags;
    
    /// The user-supplied buffer pointer and its length.
    unsigned char *buffer;
    size_t bufsize;
    
    /// We define `cur` to always be `<=` end.
    unsigned char *cur; /// Where the next byte should be read from.
    unsigned char *end; /// Pointer to just past the last byte in the buffer.
} rdr_t;


/// Has the underlying file descriptor already reached EOF?
#define RDR_EOF     ((uint_least8_t)(1 << 0))

/// Did *we* allocate this reader's buffer, or was it given to us?
#define RDR_OWNED   ((uint_least8_t)(1 << 1))


/// Construct a new reader.
///
/// If `buffer` is NULL, a new buffer is allocated with `mattalloc`.
/// It should be freed with `rdr_free()`.
///
/// If `buffer` is *not* NULL, the provided buffer will be used. In this case,
/// `rdr_free()` will *not* free the buffer.
rdr_t rdr_new(int fd, void *buffer, size_t bufsize);

/// Frees a reader that was created by `rdr_new`.
///
/// `rdr_free()` will only call `free` on its internal buffer if the buffer was
/// allocated inside of `rdr_new()`; otherwise, it is interpreted to be owned by
/// the caller. Either way, `rdr->buffer` will be NULL after the call.
///
/// The provided file descriptor is not closed.
void rdr_free(rdr_t *rdr);

/// Checks if this reader has already reached EOF.
bool rdr_eof(const rdr_t *rdr);

/// Read `n` bytes from `rdr` into `dst`.
///
/// If `dst` is NULL`, the reader's internal cursor is advanced without writing
/// the data anywhere.
///
/// If there are fewer than `n` bytes available in the reader's buffer, a
/// `read(2)` syscall is performed.
///
/// On success, the total number of bytes read is returned. On failure, -1 is
/// returned and the underlying error from `read(2)` is left in `errno` for
/// inspection.
ssize_t rdr_read(rdr_t *rdr, void *dst, size_t n);

#endif
