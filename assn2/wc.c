#include <stdio.h>
#include <stdlib.h> // EXIT_* codes
#include <stdbool.h>
#include <string.h> // memset, memcpy, strcmp

#include "../mattlib.h"
#include "../mattstr.h"

typedef enum arg_type {
    ARG_FLAG,
    ARG_PATH,
} arg_type_t;

typedef struct args {
    bool help;          /// --help flag
    bool count_bytes;   /// -c/--bytes flag
    bool count_chars;   /// -m/--chars flag
    bool count_words;   /// -w/--words flag
    bool count_lines;   /// -l/--lines flag
    
    /// `true` if at least 1 positional argument appeared. Lets us determine
    /// if we need to read from `stdin` or not.
    bool files_specified;

    /// VLA with the length of `argc - 1` (initialized in main) that, after
    /// parsing, denotes what each of the arguments in `argv` are. Used to
    /// filter CLI flags out of the list of
    arg_type_t *arg_lut;
} args_t;

/// Attempt to parse command-line arguments into an `args_t`.
///
/// Sets an error message in `*err` and returns `ERR` if parsing fails.
/// `*err` must be freed in this case. Otherwise, `*err` is unchanged.
/// On success, `OK` is returned.
result_t parse_args(int argc, const char **argv, args_t *args, str_t *err);


/// The "main" function that actually runs the guts of the program.
///
/// Returns `ERR` if running fails for some reason, and sets `err`.
result_t run(int argc, const char **argv, const args_t args, str_t *err);


int main(int argc, char **argv)
{
    // Skip first arg. We don't need it here.
    // (when we implement --help, we may decide we want progname, tho)
    argv++; argc--;
    
    str_t err = {0};
    args_t args = {0};
    arg_type_t arg_lut[argc];
    
    args.arg_lut = (arg_type_t*)arg_lut;
    
    if (ERR == parse_args(argc, (const char**)argv, &args, &err))
    {
        printf(ANSI_RED("Error")": %s\n", err.str);
        strfree(&err);
        return EXIT_FAILURE;
    }
    
    if (args.help)
    {
        printf("TODO: help message\n");
        return EXIT_SUCCESS;
    }

    if (ERR == run(argc, (const char**)argv, args, &err))
    {
        printf(ANSI_RED("Error")": %s\n", err.str);
        strfree(&err);
        return EXIT_FAILURE;
    }    

    return EXIT_SUCCESS;
}


result_t parse_args(int argc, const char **argv, args_t *args, str_t *err)
{
    bool dbl_dash = false;        // Seen '--' at some point?

    for (int optind = 0; optind < argc; optind++)
    {
        const char *arg = argv[optind];
        size_t arg_len = strlen(arg);
        
        if (arg[0] == '-' && !dbl_dash)
        {
            // Command line option.
            args->arg_lut[optind] = ARG_FLAG;
            
            // Macro for setting a flag if it hasn't been set already,
            // returning an error if it has.
            #define SET_ONCE(arg, name) \
                /* https://share.google/aimode/N0zgBCfzmZN5BiUL0 */ \
                do { \
                    if (!(arg)) { \
                        (arg) = true; \
                    } else { \
                        *err = strfstr("duplicate option " name); \
                        return ERR; \
                    } \
                } while (0)
            
            // Is this argument literally just '-'?
            if (arg_len == 1)
            {
                // If so, this represents reading from stdin.
                args->arg_lut[optind] = ARG_PATH;
                continue;
            }
            
            // Is this a long option? (starts with '--')?
            else if (arg_len > 1 && arg[1] == '-')
            {
                // If this option is just '--', no more processing needed.
                if (arg_len == 2) {
                    dbl_dash = true;
                    continue;
                }
                
                if (0 == strcmp(arg, "--help")) {
                    args->help = true;
                    return OK;
                }
                else if (0 == strcmp(arg, "--bytes")) SET_ONCE(args->count_bytes, "-c/--bytes");
                else if (0 == strcmp(arg, "--words")) SET_ONCE(args->count_words, "-w/--words");
                else if (0 == strcmp(arg, "--chars")) SET_ONCE(args->count_chars, "-m/--chars");
                else if (0 == strcmp(arg, "--lines")) SET_ONCE(args->count_lines, "-l/--lines");
                else {
                    *err = strfstr("unknown option %s", arg);
                    return ERR;
                }
            }
            
            // If this is a short option, loop through all its characters
            // to see which flags it sets (e.g., -wcl sets three in one go).
            else
            {
                for (size_t i = 1; i < arg_len; i++)
                {
                    switch (arg[i]) {
                        case 'c': SET_ONCE(args->count_bytes, "-c/--bytes"); break;
                        case 'w': SET_ONCE(args->count_words, "-w/--words"); break;
                        case 'm': SET_ONCE(args->count_chars, "-m/--chars"); break;
                        case 'l': SET_ONCE(args->count_lines, "-l/--lines"); break;
                        case 'h':
                            args->help = true;
                            return OK;
                        default:
                            *err = strfstr("unknown option -%c", arg[i]);
                            return ERR;
                    }
                }
            }
            
            #undef SET_ONCE
        }
        else
        {
            // Plain filepath.
            args->arg_lut[optind] = ARG_PATH;
            args->files_specified = true;
        }
    }
    
    return OK;
}


result_t run(int argc, const char **argv, const args_t args, str_t *err)
{
    printf(
        "Flags: %s-c %s-w %s-m %s-l\x1b[0m\n",
        args.count_bytes ? "\x1b[97m" : "\x1b[90m",
        args.count_words ? "\x1b[97m" : "\x1b[90m",
        args.count_chars ? "\x1b[97m" : "\x1b[90m",
        args.count_lines ? "\x1b[97m" : "\x1b[90m"
    );
    
    printf("List of files:\n");
    
    if (!args.files_specified)
    {
        printf("* "ANSI_BRCYN("(stdin)")"\n");
    }
    else
    {
        for (int i = 0; i < argc; i++) {
            if (args.arg_lut[i] == ARG_PATH) {
                printf(
                    "* "ANSI_BRCYN("%s")"\n",
                    (0 == strcmp(argv[i], "-")) ? "(stdin)" : argv[i]
                );
            }
        }
    }

    return OK;
}
