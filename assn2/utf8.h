#ifndef MATT_UTF8
#define MATT_UTF8

#include <stdint.h>

#define UTF8_REPLACEMENT_CHAR ((uint32_t)0xFFFD)

/// Return-codes for a single step of a UTF-8 decoder.
typedef enum utf8dec_result {
    /// The most recently pushed byte...
    UTF8_DEC_CODEPOINT = 0, /// ...yielded a complete codepoint.
    UTF8_DEC_PENDING,       /// ...did not complete a codepoint.
    UTF8_DEC_INVALID,       /// ...was not a valid UTF-8 code-unit.
    UTF8_DEC_MBFAIL,        /// ...invalidated a pending multibyte sequence.
} utf8dec_result_t;

/// UTF-8 decoder's internal state.
typedef enum utf8dec_state {
    UTF8_STATE_RESET = 0,   // Waiting for first code-unit.
    UTF8_STATE_TWO_1,       // Seen first unit of a 2-unit codepoint.
    UTF8_STATE_THREE_1,     // Seen first unit of a 3-unit codepoint.
    UTF8_STATE_THREE_2,     // Seen first two units of a 3-unit codepoint.
    UTF8_STATE_FOUR_1,      // Seen first unit of a 4-unit codepoint.
    UTF8_STATE_FOUR_2,      // Seen first two units of a 4-unit codepoint.
    UTF8_STATE_FOUR_3,      // Seen first three units of a 4-unit codepoint.
} utf8dec_state_t;

typedef struct utf8_decoder {
    /// Decoder's internal state.
    utf8dec_state_t state;
    
    /// History of bytes.
    uint8_t history[3];
} utf8_decoder_t;


/// Creates a new UTF-8 decoder.
utf8_decoder_t utf8dec_new();

/// Steps a decoder's state machine by pushing the next byte, `c`.
///
/// Return values for this function are as follows:
///
/// UTF8_DEC_CODEPOINT
///
///     `c` was the last byte needed to complete a codepoint.
///     If non-NULL, `*cp` is set to the UTF-32 value of the codepoint
///     (`U+ABCD` = 0xABCD).
///
/// UTF8_DEC_PENDING
///
///     `c` is part of a multibyte sequence. Keep pushin' more bytes!
///     `*cp` is not modified.
///
/// UTF8_DEC_INVALID
///
///     `c` is not a valid UTF-8 code unit.
///     If non-NULL, `*cp` is set to `U+FFFD` "Replacement Character".
///
/// UTF8_DEC_MBFAIL
///
///     The decoder was expecting a continuation byte, but `c` was not one.
///     This invalidates the currently pending multibyte sequence, but does
///     **not** mean that `c` itself is invalid. The call should be repeated
///     with `c` again to properly process it.
///
///     In this case, `*cp` is set to `U+FFFD` "Replacement Character".
utf8dec_result_t utf8dec_decode_next(utf8_decoder *self, char c, uint32_t *cp);


#endif