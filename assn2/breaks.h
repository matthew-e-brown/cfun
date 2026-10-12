#ifndef MATT_BREAKS
#define MATT_BREAKS

#include <stdint.h>     // uint32_t
#include <stdbool.h>    // bools

/// State for determining whether a codepoint in a stream of Unicode text is a
/// word-break.
///
/// For now, this simply looks for transitions from a non-whitespace character
/// to a whitespace character, where a "whitespace character" is defined is a
/// codepoint with the `White_Space` Unicode property. Since there are only 25
/// such characters, this keeps things simple.
///
/// Eventually, the goal is to apply the full Unicode Standard Annex (UAX) #29
/// Word Segmentatin algorithm. This will require using a subset of the UCD.
///
/// [UAX #29]: https://www.unicode.org/reports/tr29/tr29-49.html
typedef struct wb_state {
    bool inside_word;
} wb_state_t;

/// Initializes fresh word-break detection state.
wb_state_t wb_new();

/// Check whether the codepoint `cp` results in a word-break in the current text
/// stream. Returns `true` if it does, and `false` otherwise.
bool wb_check_next(wb_state_t *self, uint32_t cp);

/// Check whether a word-break occurs at end-of-text for the current text
/// stream.
bool wb_finish(wb_state_t *self);


/// State for determining whether a codepoint in a stream of Unicode text is a
/// line-break.
///
/// Specifically, this structure detects sequences of two codepoints which fall
/// under rules LB4 and LB5 in Section 6.1 of Unicode Standard Annex (UAX) #14,
/// "Unicode Line Breaking Algorithm."
///
/// [UAX #14]:     https://www.unicode.org/reports/tr14/
/// [Section 6.1]: https://www.unicode.org/reports/tr14/#BreakingRules
///
/// The rules are listed in order of priority. Rules LB4 and LB5 are the ones
/// concerned with explicit line breaks.
///
/// Rule LB4 is just, "Always break after hard line breaks"; a "hard line break"
/// is defined to be anything in the `BK` character class, which is, surprise,
/// the "Mandatory Break" class. The following characters are in the `BK` class:
///
/// * U+000B LINE TABULATION (VT)
/// * U+000C FORM FEED (FF)
/// * U+2028 LINE SEPARATOR
/// * U+2029 PARAGRAPH SEPARATOR
///
/// Rule LB5 is, "Treat `CR` followed by `LF`, as well as `CR`, `LF`, and `NL`
/// as hard line breaks." `CR`, `LF`, and `NL` are *technically* defined as
/// separate character classes for the purposes of the formal specificaition of
/// the algorithm, but they are each just composed of single characters:
///
/// * `CR` = U+000D CARRIAGE RETURN
/// * `LF` = U+000A LINE FEED
/// * `NL` = U+0085 NEXT LINE (NEL)
///
/// The remainder of the UAX #14 algorithm is concerned with determining where
/// lines *may* be broken in the event of over-long lines. Since these two rules
/// (appear to) cover all explicit cases, it should suffice for our purposes.
typedef struct lb_state {
    // All we need to know is whether or not the previous codepoint was a CR.
    // Otherwise, it's just an explicit match on the provided codepoint.
    bool cr;
} lb_state_t;

/// Initializes fresh word-break detection state.
lb_state_t lb_new();

/// Check whether the codepoint `cp` results in a line-break in the current text
/// stream. Returns `true` if it does, and `false` otherwise.
bool lb_check_next(lb_state_t *self, uint32_t cp);

/// Check whether a line-break occurs at end-of-text for the current text
/// stream.
///
/// NOTE: This function ignores its argument and always returns `true`. Per UAX
/// #14, [Rule LB3] (Section 6), a line break is always present at end of text.
/// 
/// [Rule LB3]: https://www.unicode.org/reports/tr14/#LB3
///
/// This function is provided for consistency and future API compatibility.
bool lb_finish(lb_state_t *self);

#endif
