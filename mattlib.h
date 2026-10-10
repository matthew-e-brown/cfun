/* ====================================================================
 * General purpose helpers and constants.
 * ================================================================= */

#ifndef MATTLIB
#define MATTLIB

#include <stdint.h>

typedef int8_t  i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

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

#endif
