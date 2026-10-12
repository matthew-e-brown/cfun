#ifndef MATT_UTF8
#define MATT_UTF8

#include <stdint.h>

#define UTF8_REPLACEMENT_CHAR ((uint32_t)0xFFFD)

/// Return-codes for a single step of a UTF-8 decoder.
typedef enum utf8dec_result {
    /// The most recently pushed byte:
    UTF8_DEC_CODEPOINT = 0, /// Completed a codepoint.
    UTF8_DEC_PENDING,       /// Started or continued a multibyte sequence.
    UTF8_DEC_INVALID,       /// Was not a valid leading UTF-8 code-unit.
    UTF8_DEC_MBFAIL,        /// Invalidated a pending multibyte sequence (but was not itself invalid).
} utf8dec_result_t;

/// The UTF-8 decoder state machine.
typedef struct utf8_decoder {
    /// Decoder's current state.
    uint8_t state;
    
    /// History of bytes.
    uint8_t history[3];
} utf8_decoder_t;


/// Creates a new UTF-8 decoder.
///
/// The decoder's stack footprint is very small (4 bytes total) and requires no
/// cleanup when finished.
utf8_decoder_t utf8dec_new();


/// Steps a decoder's state machine by pushing the next byte, `c`.
///
/// Return values for this function are as follows:
///
/// UTF8_DEC_CODEPOINT
///
///     `c` was the last byte needed to complete a codepoint.
///     If `*cp` is non-NULL, it is set to the UTF-32 value of the codepoint
///     (i.e., `U+ABCDEF` = 0xABCDEF).
///
/// UTF8_DEC_PENDING
///
///     `c` is part of a multibyte sequence. Keep pushin' more bytes!
///     `*cp` is not modified.
///
/// UTF8_DEC_INVALID
///
///     `c` is not a valid UTF-8 lading code-unit. This includes code-units
///     which are valid continuation bytes but do not appear inside of a valid
///     multi-unit sequence.
///     If `*cp` is non-NULL, it is set to `U+FFFD` "Replacement Character".
///
/// UTF8_DEC_MBFAIL
///
///     The decoder was expecting a continuation byte, but `c` was not one.
///     This invalidates the currently pending multibyte sequence, but does
///     **not** mean that `c` itself is invalid. The call should be repeated
///     with `c` again to properly process it.
///
///     In this case, `*cp` is set to `U+FFFD` "Replacement Character".
utf8dec_result_t utf8dec_decode_next(utf8_decoder_t *self, uint8_t c, uint32_t *cp);


/// Returns the number of bytes read for a decoder's currently-pending
/// multi-byte sequence.
///
/// If the decoder does not have a pending multi-byte sequence (i.e., it is
/// between codepoints), 0 is returned.
///
/// Since a UTF-8 code-unit sequence will never exceed four bytes in size, this
/// function can only ever return values between 0 and 3 inclusive.
///
/// Subtracting this function's return value from the input stream's current
/// position will give the start of the currently in-progress codepoint. This
/// can be followed by `utf8dec_mb_endsize()` to determine what size of
/// multi-unit codepoint was expected.
///
/// This function can be used to detect errors at the end of a UTF-8 stream.
/// If it returns 0 at the end of a stream, then the last byte in the sequence
/// successfully yielded a codepoint. Otherwise, the last `n` bytes did not
/// yet participate in a well-formed code-unit sequence.
size_t utf8dec_mb_cursize(utf8_decoder_t *self);


/// Returns the expected size of a decoder's currently-pending multi-byte
/// sequence.
///
/// That is, if the decoder is currently partway through decoding a 2-byte
/// codepoint, this function returns 2; if it is partway through a 3-byte
/// sequence, 3 is returned; if it is partway through a 4-byte sequence, 4 is
/// returned.
///
/// If the decoder does not have a pending multi-byte sequence (i.e., it is
/// between codepoints), 0 is returned.
///
/// Since there is no such thing as a multi-byte sequence of length 1 (the
/// decoder can never be waiting for the "next" byte of a single-byte sequence),
/// this function will only ever return the values 0, 2, 3, or 4.
size_t utf8dec_mb_endsize(utf8_decoder_t *self);


#endif