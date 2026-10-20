#ifndef MATTSTR
#define MATTSTR

#include <stddef.h> // size_t

/// A C-style string packaged alongside its length for convenience.
///
/// That is:
///
/// * `len` is the length of the string WITHOUT a NUL byte;
/// * `str` is a pointer to a buffer which IS terminated by a NUL byte.
/// * `cap` is the size of its buffer WITHOUT a NUL byte;
typedef struct str {
    char *str;
    size_t len;
    size_t cap;
} str_t;

/// Returns an empty `str_t` with no allocation.
///
/// NB: this `str_t` is still safe to free, since `free(3)` does not
/// care if it gets passed NULL.
str_t strempty();

/// Returns an empty `str_t` with enough space to hold `cap` characters.
str_t strnew(size_t cap);

/// Reserves capacity for at least `n` additional characters.
void strreserve(str_t *str, size_t n);

/// Clears the contents of a string, keeping the rest of its buffer.
void strclear(str_t *str);

/// Returns an allocated `str_t` (that the user must free) containing
/// the formatted output specified by `fmt` and `...`.
str_t strfstr(const char *restrict fmt, ...);

/// Pushes `s` onto `str`'s buffer.
///
/// `s` MUST be NUL-terminated.
void strpush(str_t *str, char const *s);

/// Pushes the result of formatting `fmt` and `...` onto `str`.
void strpushstrf(str_t *str, const char *restrict fmt, ...);

/// Frees a `str_t`.
///
/// This is the same as just calling `free(3)` on the string's `str`
/// member and setting its `len`, but looks a little more "proper".
void strfree(str_t *str);

/*
/// Accepts `count` instances of `str_t*` and concatenates them together
/// into a new one, freeing all old strings automatically.
str_t strconcat(size_t count, ...);
*/

#endif
