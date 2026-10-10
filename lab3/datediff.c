/*
 * Build with:
 *
 * gcc -std=c11 -pedantic -Wall -Wextra ./datediff.c ../mattstr.c -o ./datediff
 */

#define _POSIX_C_SOURCE 200112L // Needed for setenv to exist inside stdlib.h
#include <stdio.h>   // printf, etc.
#include <stdlib.h>  // malloc, calloc, free; setenv (for timezone); strtol; EXIT_*
#include <errno.h>   // errno (checking strtol)
#include <stdint.h>  // int32_t, uint32_t, etc.
#include <assert.h>  // assert() macro
#include <time.h>    // struct tm, time, mktime, localtime, etc.
#include <stdbool.h> // ...bools, duh
#include <libgen.h>  // basename()
#include <string.h>  // strcmp

#include "../mattlib.h"
#include "../mattstr.h"

// Sometimes, I wanna treat my bools like ints and do bit-shifts on them...
#pragma GCC diagnostic ignored "-Wint-in-bool-context"

#define SINGPLUR(x) (x == 1 ? "" : "s")

typedef struct arg_error {
    str_t msg;  /// The actual error message.
    bool usage; /// The message should get a small "usage" string printed alongside it.
    bool help;  /// The user asked for help (TODO).
} arg_error_t;

/// Parses command line input from the user into a `tm` struct.
///
/// Returns `ERR` if arguments are invalid.
///
/// If `ERR` is returned, `err->msg` is a `str_t` with an error message.
/// This `str_t` must be freed by the caller later. If `OK` is returned,
/// `err` is unchanged.
result_t parse_args(int argc, char const **argv, struct tm *tm, arg_error_t *err);

/// Prints an argument error.
void print_arg_error(char *argv0, arg_error_t const *err);

/// Formats a time_t into text.
str_t format_timespan(time_t ts);


int main(int argc, char **argv)
{
    // Set the current timezone if it is not already set.
    //
    // The colon makes it so `localtime` skips directly to interpreting TZ as
    // a filepath to a timezone file, rather than attempting to parse it as a
    // specification directly first (see tzset(3)).
    setenv("TZ", ":America/Toronto", 0);

    struct tm tm = {0};
    arg_error_t arg_err = {0};

    if (ERR == parse_args(argc, (char const**)argv, &tm, &arg_err))
    {
        if (arg_err.help) {
            fprintf(stdout, "TODO: help\n");
            strfree(&arg_err.msg);
            return EXIT_SUCCESS;
        } else {
            print_arg_error(argv[0], &arg_err);
            strfree(&arg_err.msg);
            return EXIT_FAILURE;
        }
    }
    
    errno = 0;
    tm.tm_wday = -1;  // Recommended way to error-check mktime (mktime(3))
    tm.tm_isdst = -1; // Have mktime(3) figure out if we're in DST or not

    time_t cur_timestamp = time(NULL);
    time_t req_timestamp = mktime(&tm);
    if (errno != 0 || tm.tm_wday == -1)
    {
        fprintf(stderr, ANSI_RED("Error (mktime)")": time is out of range.");
        return EXIT_FAILURE;
    }

    time_t difference = labs(req_timestamp - cur_timestamp);
    str_t time_fmtted = format_timespan(difference);
    bool in_the_past = req_timestamp <= cur_timestamp;

    printf(
        "Current timestamp: "ANSI_BRCYN("%ld")"\n"
        "   Your timestamp: "ANSI_BRCYN("%ld")"\n"
        "Your date (midnight) %s "ANSI_BRCYN("%s")" "ANSI_BRBLK("(%ld sec%s)")" %s.\n",
        cur_timestamp,
        req_timestamp,
        in_the_past ? "was" : "is",
        time_fmtted.str,
        difference,
        SINGPLUR(difference),
        in_the_past ? "ago" : "from now"
    );
    
    return EXIT_SUCCESS;
}

str_t format_timespan(time_t ts)
{
    static const char *items[] = {"year", "day", "hour", "minute", "second"};
    time_t values[] = {
        (ts / 60 / 60 / 24 / 365),
        (ts / 60 / 60 / 24) % 365,
        (ts / 60 / 60) % 24,
        (ts / 60) % 60,
        ts % 60
    };
    
    str_t fmt = strnew(60);
    bool first_item = true;
    int n_items = 5; // TODO: allow omitting h:m:s at some point, for shorter output?

    for (int i = 0; i < n_items; i++)
    {
        time_t v = values[i];
        
        // Skip all leading-zero items
        if (first_item && v == 0)
            continue;

        if (first_item) {
            first_item = false;
        } else {
            // Only allow negative sign to appear on the first item.
            v = labs(v);
            if (i < n_items - 1) strpush(&fmt, ", ");
            else                 strpush(&fmt, ", and ");
        }

        strpushstrf(&fmt, "%ld %s%s", v, items[i], SINGPLUR(v));
    }
    
    return fmt;
}

/// Attempts to parse an integer from a string.
static result_t parse_int_arg(char const* str, int32_t *x, str_t *err)
{
    char *endptr;
    long value;
    
    if (*str == '\0')
    {
        *err = strfstr("input is empty");
        return ERR;
    }

    errno = 0;
    value = strtol(str, &endptr, 10);
    
    // strtol sets endptr to the next character after its last-found
    // valid integer character. So, if the whole string was valid, it
    // should be at the NUL at the end of the string. If there were no
    // valid characters, it'll still be at the start.
    if (endptr == str || *endptr != '\0') {
        // String does not start with an integer, or contains non-integer
        // chars before end.
        *err = strfstr("input is not an integer");
        return ERR;
    } else if (errno == ERANGE) {
        // Number overflowed. Could also check against LONG_MIN/LONG_MAX to
        // check for underflow, but that's prolly not needed.
        *err = strfstr("input too large (overflow occurred)");
        return ERR;
    } else {
        *x = (int32_t)value;
        return OK;
    }
}

/// Checks if the given date is in bounds for the given year and month.
static inline result_t validate_day(int32_t year, int32_t month, int32_t day)
{
    int32_t len = 0;
    
    if (month == 2)
    {
        // https://stackoverflow.com/a/11595914/10549827
        // This dude is very passionate about the correct way to do this; may as well listen to him lol.
        bool is_leap = (year & 3) == 0 && ((year % 25) != 0 || (year & 15) == 0);
        len = is_leap ? 28 : 29;
    }
    else
    {
        static int32_t lengths[] = { 31, 0, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
        len = lengths[month - 1]; // month is 1-12, want 0-11
    }
    
    return (day <= len) ? OK : ERR;
}

result_t parse_args(int argc, char const **argv, struct tm *tm, arg_error_t *err)
{
    // [TODO] Also allow an optional hours, minutes, and seconds.
    //        e.g. so I can do 2000-09-17 2:15 AM (my birthday! 😄)

    // Ok, first things first: check for help flag.
    for (int i = 1; i < argc; i++) {
        if (strncmp(argv[i], "-h", 2) == 0 || strncmp(argv[i], "--help", 6) == 0)
        {
            err->msg = strempty();
            err->help = true;
            err->usage = false;
            return ERR;
        }
    }

    // Next, argument count.
    if (argc != 4)
    {
        // Print no error for missing <year>; just print usage.
        if (argc <= 1) err->msg = strempty();
        else if (argc == 2) err->msg = strfstr("Value missing for <month>");
        else if (argc == 3) err->msg = strfstr("Value missing for <day>");
        else /* argc >= 5 */ err->msg = strfstr("Unexpected argument '%s'", argv[4]);
        err->usage = true;
        err->help = false;
        return ERR;
    }

    // Now we know there are exactly four (really 3) arguments.
    // Try to parse them to integers.
    int32_t yyyy;
    int32_t mm;
    int32_t dd;

    str_t parse_err = {0};
    // Any non-zero value is truthy; we know the `ERR` enum is defined
    // to always be 1, so we can detect which one failed by OR-ing on
    // their 0/1 values shifted by some known amounts.
    int parse_rc = ((int)parse_int_arg(argv[1], &yyyy, &parse_err) << 0)
        || ((int)parse_int_arg(argv[2], &mm, &parse_err) << 1)
        || ((int)parse_int_arg(argv[3], &dd, &parse_err) << 2);

    if (parse_rc) {
        char const *arg_name = "argument";
        switch (parse_rc) {
            case /*001*/ 1: arg_name = "<year>"; break;
            case /*010*/ 2: arg_name = "<month>"; break;
            case /*100*/ 4: arg_name = "<day>"; break;
            default: /* unreachable */;
        }
        
        err->msg = strfstr("Invalid value for %s: %s", arg_name, parse_err.str);
        strfree(&parse_err);
        return ERR;
    }

    if (yyyy < 0 || yyyy > 9999) {
        err->msg = strfstr("Year %d is out of range (0-9999)", yyyy);
        return ERR;
    }
    
    if (mm < 1 || mm > 12) {
        err->msg = strfstr("Month %d is out of range (1-12)", mm);
        return ERR;
    }
    
    if (ERR == validate_day(yyyy, mm, dd)) {
        err->msg = strfstr("Day %d is not valid for month %d-%d", dd, yyyy, mm);
        return ERR;
    }
    
    tm->tm_year = yyyy - 1900;  // tm's year is offset from 1900
    tm->tm_mon = mm - 1;        // tm's month is 1-indexed
    tm->tm_mday = dd;           // day is unchanged
    return OK;
}

void print_arg_error(char *argv0, arg_error_t const *err)
{
    if (err->usage)
    {
        char *pname = basename(argv0);
        fprintf(stderr, "Usage: %s <year> <month> <day> [-h] [--help]\n", pname);
    }
    
    if (err->msg.str)
    {
        fprintf(stderr, ANSI_RED("Error") ": %s\n", err->msg.str);
    }
}
