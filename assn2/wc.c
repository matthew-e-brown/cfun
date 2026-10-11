#include <stdio.h>      // printf/fprintf
#include <stdlib.h>     // EXIT_* codes; malloc + free
#include <stdint.h>     // integer typedefs
#include <stdbool.h>    // ...bools, duh
#include <sysexits.h>   // standardized exit codes (from BSD!) see manpage for list
#include <string.h>     // memset, memcpy, strcmp; strerror*
#include <fcntl.h>      // open()
#include <unistd.h>     // read(), write(), close()
#include <assert.h>     // static_assert() macro

#include "../mattlib.h" // result_t; ANSI colour-codes
#include "../mattstr.h" // str_t and methods; mostly used for error reporting


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

/// Opens the file given by `path` before forwarding to the main counting
/// process. This function is mostly responsible for handling any errors.
///
/// If `path` is NULL, input is read from stdin.
result_t process_file(const char *path, stats_t *stats, str_t *err);

/// Runs the main counting process on the opened file descriptor.
///
/// If an error occurrs, `*err` is set to an error message and `ERR` is
/// returned; `*stats` will not be modified in this case. Otherwise, `OK` is
/// returned and `*err` is not modified.
result_t count(int fd, stats_t *stats, str_t *err);


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

    if (ERR == parse_args(argc, (const char**)argv, &args, &err))
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
    
    // Onto the main program!
    // ------------------------------------------
    
    printf(
        "Flags: %s-c %s-w %s-m %s-l\x1b[0m\n",
        args.config.print_bytes ? "\x1b[97m" : "\x1b[90m",
        args.config.print_words ? "\x1b[97m" : "\x1b[90m",
        args.config.print_chars ? "\x1b[97m" : "\x1b[90m",
        args.config.print_lines ? "\x1b[97m" : "\x1b[90m"
    );
    
    printf("List of files:\n");
    
    if (args.num_files == 0)
    {
        printf("* "ANSI_BRCYN("(stdin)")"\n");
    }
    else
    {
        for (size_t i = 0; i < args.num_files; i++)
        {
            char *file = args.files[i];
            if (0 == strcmp(file, "-"))
                file = "(stdin)";
            printf("* "ANSI_BRCYN("%s")"\n", file);
        }
    }

done:
    // Both `free` and `strfree` are totally happy to accept not-yet-allocated
    // pointers/strings.
    free(args.files);
    strfree(&err);
    return exit_code;
}


result_t parse_args(int argc, char **argv, args_t *args, str_t *err)
{
    bool dbl_dash = false;        // Seen '--' at some point?
    
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
            // Macro for setting a flag if it hasn't been set already,
            // returning an error if it has.
            #define SET_ONCE(lvalue, strname) \
                /* https://share.google/aimode/N0zgBCfzmZN5BiUL0 */ \
                do { \
                    if (!(lvalue)) { \
                        (lvalue) = true; \
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
                else if (0 == strcmp(arg, "--bytes")) SET_ONCE(args->config.print_bytes, "-c/--bytes");
                else if (0 == strcmp(arg, "--words")) SET_ONCE(args->config.print_words, "-w/--words");
                else if (0 == strcmp(arg, "--chars")) SET_ONCE(args->config.print_chars, "-m/--chars");
                else if (0 == strcmp(arg, "--lines")) SET_ONCE(args->config.print_lines, "-l/--lines");
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
                        case 'c': SET_ONCE(args->config.print_bytes, "-c/--bytes"); break;
                        case 'w': SET_ONCE(args->config.print_words, "-w/--words"); break;
                        case 'm': SET_ONCE(args->config.print_chars, "-m/--chars"); break;
                        case 'l': SET_ONCE(args->config.print_lines, "-l/--lines"); break;
                        case 'h':
                            args->help = true;
                            return OK;
                        default:
                            *err = strfstr("unknown option -%c", *c);
                            return ERR;
                    }
                }
            }
            
            #undef SET_ONCE
        }
        else
        {
            // Plain filepath.
            args->files[args->num_files++] = arg;
        }
    }
    
    return OK;
}


result_t process_file(const char *path, stats_t *o_stats, str_t *err)
{
    int fd;
    result_t status;
    
    // First, open the file. If the path is NULL, we want stdin.
    if (path == NULL)
    {
        // stdin doesn't need to be opened; we can just read from it directly.
        fd = STDIN_FILENO; // constant from unistd.h
    }
    else
    {
        fd = open(path, O_RDONLY);
        if (fd < 0)
        {
            int open_errno = errno;
            // These are from string.h: see strerror(3).
            const char *errname = strerrorname_np(open_errno);
            const char *errdesc = strerrordesc_np(open_errno);
            *err = strfstr("failed to open %s: "ANSI_RED("%s")": %s", path, errname, errdesc);
            return ERR;
        }
    }
    
    // Now actually run the main counting algorithm.
    if (ERR == count(fd, o_stats, err))
    {
        // According to the errors listed in `read(2)`, these should be the only
        // two errors possible for our current situation, aside from EINTR
        // (which is handled by `fill_buff`).
        if      (errno == EISDIR) *err = strfstr("%s is a directory");
        else if (errno == EIO)    *err = strfstr("failed to read %s: an I/O error occurred");
        else                      *err = strfstr("failed to read %s: an unknown error occurred");
    }
    
    // Cleanup the FD before leaving.
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
        int close_errno = errno;
        const char *errname = strerrorname_np(close_errno);
        const char *errdesc = strerrordesc_np(close_errno);
        // Check if we already have an error from a failed read and push onto
        // that. If not, this is fresh error.
        if (status == ERR) {
            strpushstrf(err, ". failed to close: "ANSI_RED("%s")": %s", errname, errdesc);
        } else {
            *err = strfstr("failed to close %s: "ANSI_RED("%s")": %s", path, errname, errdesc);
        }
    }
    
    return status;
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

/// Since we're using this bffer to decode UTF-8, we need enough to be able to
/// read at least one code point at once.
static_assert(BUFSIZE >= 4, "UTF-8 buffer must be >= 4 bytes in size.");


/// Fills a `buffer` of `BUFSIZE` and handles errors.
static inline ssize_t fill_buffer(int fd, char *buffer)
{
retry:
    ssize_t n = read(fd, buffer, BUFSIZE);
    if (n < 0 && errno == EINTR)
        goto retry; // Interrupted by signal. Try again.
    // Otherwise, return `n` and let the caller deal with errno.
    return n;
}

static result_t get_codepoint(const char *buffer, ssize_t *i, uint32_t *codepoint)
{
    uint8_t c1 = (unsigned)buffer[*i];
    uint8_t c2 = (unsigned)buffer[*i + 1];
    uint8_t c3 = (unsigned)buffer[*i + 2];
    uint8_t c4 = (unsigned)buffer[*i + 3];
    
    /// Checks if the given byte is a UTF-8 continuation byte.
    /// (starts with 0b10______).
    #define IS_CONT(u) (((u) & 0xC0 ) == 0x80)

    // Max codepoint: 0x10FFFF = 0b00010000'11111111'11111111.
    //
    // ```
    // if      ((c & 0x80 /* 0b1'0000000 */) == 0x00 /* 0b0'0000000 */) ; // 1-byte
    // else if ((c & 0xE0 /* 0b111'00000 */) == 0xC0 /* 0b110'00000 */) ; // 2-byte
    // else if ((c & 0xF0 /* 0b1111'0000 */) == 0xD0 /* 0b1110'0000 */) ; // 3-byte
    // else if ((c & 0xF8 /* 0b11111'000 */) == 0xF0 /* 0b11110'000 */) ; // 4-byte
    // ```
    
    // 1-unit codepoint (0xxxxxxx).
    if ((c1 & 0x80) == 0x00) 
        *i += 1;
        *codepoint = c1 & 0x7F;
        return OK;
    }
    
    // 2-unit codepoint (110xxxxx 10xxxxxx).
    else if ((c & 0xE0) == 0xC0) {
        *i += 2;
        
        // 2nd byte must start with 0b10______.
        if (!IS_CONT(c2))
            goto err;
        
        *codepoint =
            ((c1 & 0x1F) << 6) |
            (c2 & 0x3F);
        
        return OK;
    }
    
    // 3-unit codepoint (1110xxxx 10xxxxxx 10xxxxxx).
    else if ((c & 0xF0) == 0xD0) {
        *i += 3;
        
        // Next 2 bytes must start with 0b10______.
        if (!IS_CONT(c2) || !IS_CONT(c3))
            goto err;
        
        *codepoint =
            ((c1 & 0x0F) << 12) |
            ((c2 & 0x3F) << 6) |
            (c3 & 0x3F);
        
        return OK;
    }
    
    // 4-unit codepoint (11110xxx 10xxxxxx 10xxxxxx 10xxxxxx).
    else if ((c & 0xF8) == 0XF0) {
        *i += 4;
        
        // Only valid if next 3 bytes start with 0b10______.
        if (!IS_CONT(c2) || !IS_CONT(c3) || !IS_CONT(c4))
            goto ERR;
            
        *codepoint =
            ((c1 & 0x07) << 18) |
            ((c2 & 0x3F) << 12) |
            ((c3 & 0x3F) << 6) |
            (c4 & 0x3F);
        
        return OK;
    }
    
    #undef IS_CONT
    
    // Else, we have an invalid leading byte.
err:
    *codepoint = 0x00FFFD; // U+FFFD Replacement Character
    return ERR;
}


result_t count(int fd, stats_t *o_stats, str_t *err)
{
    stats_t stats = {0}; // Local copy to reduce pointer indirection.
    ssize_t nread = 0;   // Number of bytes read by each syscall.
    ssize_t bufpos = 0;  // How many bytes we have read from our buffer.
    
    while (true)
    {
        // If we're within 4 bytes of the end of the buffer, refill it. By
        // looking 4 bytes ahead, we ensure that we never have a situation
        // where a codepoint is split across the boundary of two buffer reads.
        // Makes the code simpler.
        if (bufpos + 4 > BUFSIZE)
        {
            if ((nread = fill_buffer(fd, buffer)) <= 0)
                break;
            bufpos = 0;
            stats.num_bytes += (uintmax_t)(nread);
        }
        
        // Now we can read the next 4 bytes and extract a codepoint.
        uint32_t codepoint = 0x000000;
        if (ERR = get_codepoint(buffer, &bufpos, &codepoint)) {
            // Simply ignore invalid codepoints.
            continue;
        }
        
        // Handle the codepoint!
        // [TODO]
    }
    
    // Did we break out of the loop successfully, or because of an error?
    if (nread == 0) {
        *o_stats = stats;
        return OK;
    } else {
        return ERR;
    }
}
