/*
 * Build with:
 *
 * gcc -std=c11 -pedantic -Wall -Wextra ./wc.c ./{utf8,breaks}.c ../{mattstr,mattrdr}.c -o wc
 */

#include <stdio.h>      // printf/fprintf
#include <stdlib.h>     // EXIT_* codes; malloc + free
#include <stdint.h>     // integer typedefs
#include <stdbool.h>    // ...bools, duh
#include <sysexits.h>   // standardized exit codes (from BSD!) see manpage for list
#include <string.h>     // memset, memcpy, strcmp; strerror*
#include <fcntl.h>      // open()
#include <unistd.h>     // read(), write(), close()
#include <assert.h>     // static_assert() macro
#include <errno.h>      // errno

#include "../mattlib.h" // result_t; ANSI colour-codes
#include "../mattstr.h" // str_t and methods; mostly used for error reporting
#include "../mattrdr.h" // rdr_t and methods for bufferred file-reading

#include "utf8.h"       // UTF-8 decoder state machine
#include "breaks.h"     // Unicode word- and line-breaking algorithms


/// Statistics gathered while reading a file.
typedef struct stats {
    // stdint.h: `uintmax_t` is the widest unsigned integer on the platform.
    uintmax_t num_bytes;
    uintmax_t num_chars;
    uintmax_t num_words;
    uintmax_t num_lines;
} stats_t;

/// Program configuration. Holds which statistics the user has requested on the
/// command-line.
typedef struct config {
    bool print_bytes;   /// -c/--bytes flag
    bool print_chars;   /// -m/--chars flag
    bool print_words;   /// -w/--words flag
    bool print_lines;   /// -l/--lines flag
} config_t;

/// Results of command-line argument parsing.
typedef struct args {
    /// Was the help flag passed?
    bool help;
    
    /// The actual program configuration.
    config_t config;
    
    /// The number of positional parameters (files) provided on the command
    /// line. This is the length of the `files` array/buffer.
    size_t num_files;
    
    /// Array of filenames. 
    ///
    /// Strings are borrowed from the program's `argv` (and are therefore
    /// not const).
    char **files;
} args_t;


/// Attempts to parse command-line arguments into an `args_t`.
///
/// The `files` member of `args` **must** be initialized to a buffer of size
/// at least `argc - 1` before this function is called.
///
/// Sets an error message in `*err` and returns `ERR` if parsing fails.
/// `*err` must be freed in this case. Otherwise, `*err` is unchanged.
/// On success, `OK` is returned.
result_t parse_args(int argc, char **argv, args_t *args, str_t *err);

/// Small wrapper around `open(2)`.
///
/// Handles mapping a NULL path to stdin, and reports errors according to the
/// same convention as the rest of the program.
result_t try_open(const char *path, int *fd, str_t *err);

/// Small wrapper around `close(2)`.
///
/// Avoids inadvertently closing stdin and handles error-message formatting the
/// same way as the rest of the program.
result_t try_close(int fd, str_t *err);

/// Runs the main counting process on the opened file descriptor.
///
/// In the event of an error, `*stats` is not modified.
result_t try_count(int fd, stats_t *out_stats, str_t *err);


/// Prints counted statistics to stdout.
void print_stats(stats_t stats, config_t config, const char *path)
{
    unsigned int n = 0; // Count how many prints we've done to know if we need a space.
    if (config.print_bytes) fprintf(stdout, "%ju", stats.num_bytes), n++;
    if (config.print_chars) fprintf(stdout, "%s%ju", n++ ? " " : "", stats.num_chars);
    if (config.print_words) fprintf(stdout, "%s%ju", n++ ? " " : "", stats.num_words);
    if (config.print_lines) fprintf(stdout, "%s%ju", n++ ? " " : "", stats.num_lines);
    
    if (n > 0 && path != NULL)
        fprintf(stdout, " "ANSI_CYN("%s"), path);
        
    if (n > 0)
        fprintf(stdout, "\n");
    
    fflush(stdout);
}


int main(int argc, char **argv)
{
    // Skip first arg. We don't need it here.
    // (when we implement --help, we may decide we want progname, tho)
    // (argc *should* never be zero, but it is technically valid per ISO C)
    if (argc > 0) {
        ++argv;
        --argc;
    }

    int exit_code = EXIT_SUCCESS;
    args_t args = {0};
    str_t err = strempty();

    if (ERR == parse_args(argc, argv, &args, &err))
    {
        fprintf(stderr, ANSI_RED("Invalid arguments")": %s\n", err.str);
        exit_code = EX_USAGE;
        goto done;
    }
    
    if (args.help)
    {
        fprintf(stdout, "TODO: help message\n");
        goto done;
    }
    
    stats_t stats = {0};
    stats_t totals = {0};
    
    /*
     * If no files were provided on the command-line, we just read from stdin.
     *
     * The only difference between specifying no args and specifying "-" on the
     * command line is that, in the latter case, we want to print the filename
     * (stdin) at the end of the line. This matches wc's behaviour with '-'.
     */
    
    if (args.num_files == 0)
    {
        // stdin does not need to be opened or closed. This simplifies error-
        // handling for this branch.
        if (ERR == try_count(STDIN_FILENO, &stats, &err))
        {
            fprintf(stderr, "wc: %s\n", err.str);
        }
        else
        {
            print_stats(stats, args.config, NULL);
        }
    }
    else
    {
        for (size_t i = 0; i < args.num_files; i++)
        {
            int fd;
            char *path = args.files[i];
            char *pfmt = path == NULL ? "(stdin)" : path;
            
            if (ERR == try_open(path, &fd, &err)) {
                fprintf(stderr, "wc: %s: %s\n", pfmt, err.str);
                continue;
            }
            
            if (ERR == try_count(fd, &stats, &err)) {
                fprintf(stderr, "wc: %s: %s\n", pfmt, err.str);
                continue;
            }
            
            print_stats(stats, args.config, pfmt);
            totals.num_bytes += stats.num_bytes;
            totals.num_chars += stats.num_chars;
            totals.num_words += stats.num_words;
            totals.num_lines += stats.num_lines;
            
            if (ERR == try_close(fd, &err)) {
                fprintf(stderr, "wc: %s: %s\n", pfmt, err.str);
                continue;
            }
        }
        
        if (args.num_files > 1)
        {
            print_stats(totals, args.config, "Total");
        }
    }

done:
    // Both `free` and `strfree` are totally happy to accept not-yet-allocated
    // pointers/strings.
    free(args.files);
    strfree(&err);
    return exit_code;
}


result_t try_open(const char *path, int *fd, str_t *err)
{
    result_t status = OK;
    
    if (path == NULL)
    {
        *fd = STDIN_FILENO;
    }
    else
    {
        if ((*fd = open(path, O_RDONLY)) < 0)
        {
            char *errdesc = strerror(errno);
            *err = strfstr("failed to open: %s", errdesc);
            status = ERR;
        }
    }
    
    return status;
}

result_t try_close(int fd, str_t *err)
{
    result_t status = OK;
    
    if (fd == STDIN_FILENO)
    {
        return OK;
    }

    if (close(fd) < 0 && errno != EINTR)
    {
        /*
         * What do you even do when you fail to close a file? The man page for
         * `close()` specifically says not to try closing it again, since Linux
         * frees the descriptor very early in the function.
         *
         * Well, from `close(2)`:
         *
         *      Note, however, that a failure return should be used only for
         *      diagnostic purposes (i.e., a warning to the application that
         *      there may still be I/O pending or there may have been failed
         *      I/O) or remedial purposes (e.g., writing the file once more or
         *      creating a backup).
         *
         * So, we don't actually have any remedial action here. We'll just push
         * the error message onto the end of the last one (if there is one).
         */
        char *errdesc = strerror(errno);
        *err = strfstr("failed to close: %s", errdesc);
        status = ERR;
    }
    
    return status;
}


result_t parse_args(int argc, char **argv, args_t *args, str_t *err)
{
    bool dbl_dash = false;      // Seen '--' at some point?
    unsigned int num_flags = 0; // Number of '-' flags passed.
    
    args->num_files = 0;
    args->files = mattalloc(argc * sizeof(char*));

    for (int optind = 0; optind < argc; optind++)
    {
        char *arg = argv[optind];
        
        // Should never be NULL when coming from argv, but this function is
        // technically callable from anywhere. I suppose we should also check
        // for the outer `argv` being null... but maybe we just assume that the
        // *outermost* pointers are never NULL, since that means the caller
        // passed a NULL directly, and so that's on them.
        if (arg == NULL)
            continue;

        if (arg[0] == '-' && !dbl_dash)
        {
            // Sets a flag's the given flag's value to `true` in `args->config`
            // (but only if it hasn't already been set) and increments
            // `num_flags`.
            #define SET_FLAG(field, strname) \
                /* https://share.google/aimode/N0zgBCfzmZN5BiUL0 */ \
                do { \
                    if (!args->config.field) { \
                        args->config.field = true; \
                        num_flags += 1; \
                    } else { \
                        *err = strfstr("option " strname " specified more than once"); \
                        return ERR; \
                    } \
                } while (0)
            
            // Is this argument literally just '-'?
            // (NB: reading arg[1] is safe since arg[0] is '-'; there must at
            // least be a NUL there. Same goes for subsequent reads down below).
            if (arg[1] == '\0')
            {
                // If so, this represents reading from stdin. To differentiate
                // it from a file actually called '-', we use a NULL string.
                args->files[args->num_files++] = NULL;
                continue;
            }
            
            // Is this a long option? (starts with '--')?
            else if (arg[1] == '-')
            {
                // If this option is just '--', no more processing needed.
                if (arg[2] == '\0') {
                    dbl_dash = true;
                    continue;
                }
                
                if (0 == strcmp(arg, "--help"))
                {
                    args->help = true;
                    return OK;
                }
                else if (0 == strcmp(arg, "--bytes")) SET_FLAG(print_bytes, "-c/--bytes");
                else if (0 == strcmp(arg, "--words")) SET_FLAG(print_words, "-w/--words");
                else if (0 == strcmp(arg, "--chars")) SET_FLAG(print_chars, "-m/--chars");
                else if (0 == strcmp(arg, "--lines")) SET_FLAG(print_lines, "-l/--lines");
                else
                {
                    *err = strfstr("unknown option %s", arg);
                    return ERR;
                }
            }
            
            // If this is a short option, loop through all its characters
            // to see which flags it sets (e.g., -wcl sets three in one go).
            else
            {
                for (char *c = &arg[1]; *c != '\0'; ++c)
                {
                    switch (*c) {
                        case 'c': SET_FLAG(print_bytes, "-c/--bytes"); break;
                        case 'w': SET_FLAG(print_words, "-w/--words"); break;
                        case 'm': SET_FLAG(print_chars, "-m/--chars"); break;
                        case 'l': SET_FLAG(print_lines, "-l/--lines"); break;
                        case 'h':
                            args->help = true;
                            return OK;
                        default:
                            *err = strfstr("unknown option -%c", *c);
                            return ERR;
                    }
                }
            }
            
            #undef SET_FLAG
        }
        else
        {
            // Plain filepath.
            args->files[args->num_files++] = arg;
        }
    }
    
    // If no flags were passed, print all four fields.
    if (num_flags == 0)
    {
        args->config = (config_t){true, true, true, true};
    }
    
    return OK;
}


/// Size of the stack-local buffer for reading files in `count`.
///
/// According to some brief googling, 16K seems to be about as large as you can
/// make a buffer before you start getting diminishing returns. Some other
/// sources suggested only 8K.
///
/// Although I've never done this with a buffer quite this large (I think I
/// would usually go for more like 256, 512, 1024, or 4096), this number feels
/// like the correct "modern" buffer size to use. My only concern was that
/// allocating 16K on the stack in a loop (since main calls `count` in a loop)
/// would be a bit much; but Google's AI Overview assures me that 16K should be
/// fine (https://share.google/aimode/09uATU7wfL0MJicpl).
static const size_t BUFSIZE = 16 * 1024;

// Expressions inside static asserts must be "constant." Sadly, since a
// `static const` is not a *true* constant, we can't actually check it with
// this. C23 fixes this by adding real `constexpr`; but in C11, I guess
// `static_assert` is kinda JUST for macros? Lame. :/
/* static_assert(BUFSIZE >= 1, "buffer size cannot be zero"); */


result_t try_count(int fd, stats_t *out_stats, str_t *err)
{
    /*
     * Reading from the file is buffered by a `rdr_t`. We use a stack-local
     * buffer for its internal storage-space, so we don't have to heap allocate
     * anything.
     *
     * Using a buffer lets us read byte-by-byte to decode UTF-8 without taking
     * a hit on performance by making a whole `read(2)` syscall for every byte.
     */
    uint8_t read_buf[BUFSIZE];
    rdr_t reader = rdr_new(fd, read_buf, BUFSIZE);
    
    /*
     * `utf8_decoder_t` is a state machine that handles incoming single- and
     * multi-byte sequences of UTF-8 code units.
     */
    utf8_decoder_t decoder = utf8dec_new();
    
    /*
     * Finally, we have yet two more state machines (albiet, *very* simple ones)
     * for determining whether or not a given codepoint results in a word- or
     * line-break.
     */
    wb_state_t wb = wb_new();
    lb_state_t lb = lb_new();
    
    uint8_t c;              // Most recently read byte.
    uint32_t cp;            // Latest decoded codepoint.
    stats_t stats = {0};    // Local copy of output, reduce indirection
    ssize_t nread = 0;      // Return value from calls to `read`
    
    while ((nread = rdr_read(&reader, &c, 1)) > 0)
    {    
        stats.num_bytes += 1;

retry_byte:
        switch (utf8dec_decode_next(&decoder, c, &cp))
        {
            /*
             * Character decoded! Which character is it?
             */
            case UTF8_DEC_CODEPOINT:
                // [TODO] Count extended grapheme clusters instead of (or,
                // in addition to?) counting codepoints.
                stats.num_chars += 1;
                if (wb_check_next(&wb, cp)) stats.num_words += 1;
                if (lb_check_next(&lb, cp)) stats.num_lines += 1;
                break;
            
            /*
             * Still waiting for the codepoint to finish. Skip to the next byte.
             */
            case UTF8_DEC_PENDING:
                continue;
            
            /*
             * This byte is not necessarily invalid... but the codepoint we
             * were decoding did not complete properly.
             */
            case UTF8_DEC_MBFAIL:
                goto retry_byte;
            
            /*
             * This byte is either:
             * (a) not a valid leading code unit, or
             * (b) a continuation byte not part of a sequence.
             */
            case UTF8_DEC_INVALID:
                // [TODO] ...what does this mean in terms of "counting" a
                // file? Does this count as a "character"? Do we treat it like
                // a word separator? UAX #29 and #14 may have notes about this.
                //
                // For now, we'll just skip it as if nothing happened.
                break;
        }
    }
    
    // We want to count the end of the final word...
    if (wb_finish(&wb)) stats.num_words += 1;
    // But we *don't* want to count the implicit newline at the end of the file.
    /* if (lb_finish(&lb)) stats.num_lines += 1; */
    
    rdr_free(&reader);
    
    // Did we break out of the loop successfully, or because of an error?
    if (nread < 0)
    {
        if      (errno == EIO)    *err = strfstr("failed to read: an I/O error occurred");
        else if (errno == EISDIR) *err = strfstr("failed to read: file is a directory");
        else                      *err = strfstr("failed to read: an unknown error occurred");
        return ERR;
    }
    else
    {
        *out_stats = stats;
        return OK;
    }
}
