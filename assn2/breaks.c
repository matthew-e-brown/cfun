#include <stdint.h> // uint32_t

#include "breaks.h" // this

/// Returns `true` if the given codepoint is a whitespace character, according
/// to the `White_Space` property in [PropList.txt].
///
/// [PropList.txt]: https://www.unicode.org/Public/UCD/latest/ucd/PropList.txt
///
/// The full list is relatively short, at only 25 code-points:
///
/// ```
/// 0009..000D    ; White_Space # Cc   [5] <control-0009>..<control-000D>
/// 0020          ; White_Space # Zs       SPACE
/// 0085          ; White_Space # Cc       <control-0085>
/// 00A0          ; White_Space # Zs       NO-BREAK SPACE
/// 1680          ; White_Space # Zs       OGHAM SPACE MARK
/// 2000..200A    ; White_Space # Zs  [11] EN QUAD..HAIR SPACE
/// 2028          ; White_Space # Zl       LINE SEPARATOR
/// 2029          ; White_Space # Zp       PARAGRAPH SEPARATOR
/// 202F          ; White_Space # Zs       NARROW NO-BREAK SPACE
/// 205F          ; White_Space # Zs       MEDIUM MATHEMATICAL SPACE
/// 3000          ; White_Space # Zs       IDEOGRAPHIC SPACE
///
/// # Total code points: 25
/// ```
static bool is_whitespace(uint32_t cp)
{
    // There's only 25 of them... may as well just write a big switch-case and
    // let the compiler optimize it. It'll probably be way better than whatever
    // I can come up with!
    switch (cp)
    {
        case 0x0009: // CHARACTER TABULATION / HORIZONTAL TABULATION (HT)
        case 0x000A: // LINE FEED (LF)
        case 0x000B: // LINE TABULATION / VERTICAL TABULATION (VT)
        case 0x000C: // FORM FEED (FF)
        case 0x000D: // CARRIAGE RETURN (CR)
        // --------
        case 0x0020: // SPACE
        case 0x0085: // NEXT LINE (NEL)
        case 0x00A0: // NO-BREAK SPACE
        case 0x1680: // OGHAM SPACE MARK
        // --------
        case 0x2000: // EN QUAD
        case 0x2001: // EM QUAD
        case 0x2002: // EN SPACE
        case 0x2003: // EM SPACE
        case 0x2004: // THREE-PER-EM SPACE
        case 0x2005: // FOUR-PER-EM SPACE
        case 0x2006: // SIX-PER-EM SPACE
        case 0x2007: // FIGURE SPACE
        case 0x2008: // PUNCTUATION SPACE
        case 0x2009: // THIN SPACE
        case 0x200A: // HAIR SPACE
        // --------
        case 0x2028: // LINE SEPARATOR
        case 0x2029: // PARAGRAPH SEPARATOR
        case 0x202F: // NARROW NO-BREAK SPACE
        case 0x205F: // MEDIUM MATHEMATICAL SPACE
        case 0x3000: // IDEOGRAPHIC SPACE
            return true;

        default:
            return false;
    }
}

wb_state_t wb_new()
{
    wb_state_t wb = { .inside_word = false };
    return wb;
}

bool wb_check_next(wb_state_t *self, uint32_t cp)
{
    bool prev = self->inside_word;
    bool curr = self->inside_word = !is_whitespace(cp);

    // // We can check both bools at once by packing them into two bits:
    // uint8_t bits = ((uint8_t)prev << 1) | ((uint8_t)curr);
    // switch (bits)
    // {
    //     case /* 11 */ 3: return true;  // Was inside word, now in whitespace.
    //     case /* 10 */ 2: return false; // Was inside word, not in whitespace.
    //     case /* 01 */ 1: return false; // Wasn't inside word, still in whitespace.
    //     case /* 00 */ 0: return false; // Wasn't inside word, now inside a word.
    //                                    // (we don't count the word until it ends).
    // }
    
    // ...or, now that I've written that out, it's clear we can just do this:
    return prev && curr;
}

bool wb_finish(wb_state_t *self)
{
    // If we are currently inside a word, then we have one more break.
    return self->inside_word;
}


lb_state_t lb_new()
{
    lb_state_t lb = { .cr = false };
    return lb;
}

bool lb_check_next(lb_state_t *self, uint32_t cp)
{
    bool lb = false;
    
    // Since we are only concerned with counting, and not actually breaking
    // at any lines, we will handle CR+LF sequences as follows:
    // - *Always* count CR;
    // - Only count LF if we didn't just count a CR.
    switch (cp)
    {
        case 0x000B: // LINE TABULATION (VT)
        case 0x000C: // FORM FEED (FF)
        case 0x2028: // LINE SEPARATOR (LS)
        case 0x2029: // PARAGRAPH SEPARATOR (PS)
        case 0x0085: // NEXT LINE (NEL)
        case 0x000D: // CARRIAGE RETURN (CR)
            lb = true;
            break;

        case 0x000A: // LINE FEED (LF)
            if (!self->cr)
                lb = true;
            break;
    }
    
    self->cr = (cp == 0x000D);
    return lb;
}

/*
 * We use a `self` parameter for consistency with other methods, even though we
 * aren't actually doing anything with it.
 */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"

bool lb_finish(lb_state_t *self)
{
    /*
     * Per UAX #14, rule [LB3] (Section 6), a line break is always present at end of text.
     *
     * [LB3]: https://www.unicode.org/reports/tr14/#LB3
     *
     * This makes sense, since it means that encountering zero explicit
     * linebreak characters still gives us a single line.
     */
    return true;
}

#pragma GCC diagnostic pop
