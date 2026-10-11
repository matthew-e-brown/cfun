/* ====================================================================
 * General purpose constants and helpers.
 * ================================================================= */

#ifndef MATTLIB
#define MATTLIB

#include <stdlib.h>     // malloc, abort
#include <string.h>     // memset
#include <unistd.h>     // write

/* ----- Type definitions ----- */

/// Return code for functions that can fail.
///
/// `ERR` is defined to be 1 so that erroneous conditions can be checked
/// in an if-statement.
typedef enum result {
    OK = 0,
    ERR = 1,
} result_t;

/* ----- ANSI Colour Escape Codes ----- */

#define ANSI_BLK(s) "\x1b[30m" s "\x1b[0m"
#define ANSI_RED(s) "\x1b[31m" s "\x1b[0m"
#define ANSI_GRN(s) "\x1b[32m" s "\x1b[0m"
#define ANSI_YLW(s) "\x1b[33m" s "\x1b[0m"
#define ANSI_BLU(s) "\x1b[34m" s "\x1b[0m"
#define ANSI_MAG(s) "\x1b[35m" s "\x1b[0m"
#define ANSI_CYN(s) "\x1b[36m" s "\x1b[0m"
#define ANSI_WHT(s) "\x1b[37m" s "\x1b[0m"

#define ANSI_BRBLK(s) "\x1b[90m" s "\x1b[0m"
#define ANSI_BRRED(s) "\x1b[91m" s "\x1b[0m"
#define ANSI_BRGRN(s) "\x1b[92m" s "\x1b[0m"
#define ANSI_BRYLW(s) "\x1b[93m" s "\x1b[0m"
#define ANSI_BRBLU(s) "\x1b[94m" s "\x1b[0m"
#define ANSI_BRMAG(s) "\x1b[95m" s "\x1b[0m"
#define ANSI_BRCYN(s) "\x1b[96m" s "\x1b[0m"
#define ANSI_BRWHT(s) "\x1b[97m" s "\x1b[0m"

/* ----- Helper functions ----- */

/// Error message for `mattalloc()`.
static const char _allocfail = ANSI_RED("Internal error")": memory allocation failed\n";

/// Wrapper for memory allocation that handles failure.
///
/// Also, sets memory to zero after allocation.
///
/// Failure is handled in the most brutish way possible, but it is handled!
/// That is... this simply prints (or attempts to print) a message to `stderr`
/// and then aborts the process.
static void *mattalloc(size_t amt);
{
    void *p = malloc(amt);

    if (p == NULL)
    {
        write(STDERR_FILENO, _allocfail, sizeof(_allocfail));
        abort();
    }
    
    memset(p, 0, amt);
    return p;
}

static void *mattrealloc(void *ptr, size_t newamt)
{
    ptr = realloc(ptr, newamt);
    
    if (ptr == NULL)
    {
        write(STDERR_FILENO, _allocfail, sizeof(_allocfail));
        abort();
    }
    
    return ptr;
}

#endif
