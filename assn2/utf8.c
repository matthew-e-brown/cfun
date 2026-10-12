#include <stdint.h>

#include "utf8.h"

/**
 * Max codepoint is U+10FFFF (0b00010000'11111111'11111111).
 *
 * In UTF-8, codepoints are either 1, 2, 3, or 4 "units" (bytes) long.
 * The length can be interpreted
 *
 *  - 1-unit codepoints are 0xxxxxxx
 *  - 2-unit codepoints are 110xxxxx 10xxxxxx
 *  - 3-unit codepoints are 1110xxxx 10xxxxxx 10xxxxxx
 *  - 4-unit codepoints are 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
 *
 * Bit-masks converted to hex (C doesn't have binary literals till C23):
 *
 *  - c & 0b1'0000000 (0x80) == 0b0'0000000 (0x00) ==> 1-byte codepoint
 *  - c & 0b111'00000 (0xE0) == 0b110'00000 (0xC0) ==> 2-byte codepoint
 *  - c & 0b1111'0000 (0xF0) == 0b1110'0000 (0xD0) ==> 3-byte codepoint
 *  - c & 0b11111'000 (0xF8) == 0b11110'000 (0xF0) ==> 4-byte codepoint
 *  - c & 0b11'000000 (0xC0) == 0b10'000000 (0x80) ==> Continuation byte
 *
 * Inverted bit-masks to extract the components:
 *
 *  - c & 0b0'1111111 (0x7F) for leading byte of 1-unit codepoints
 *  - c & 0b000'11111 (0x1F) for leading byte of 2-unit codepoints
 *  - c & 0b0000'1111 (0x0F) for leading byte of 3-unit codepoints
 *  - c & 0b00000'111 (0x30) for leading byte of 4-unit codepoints
 *  - c & 0b00'111111 (0x3F) for all cont. bytes in all 2-, 3-, and 4-unit codepoints
**/


/// Checks if `b` is a continuation byte.
#define IS_CONT(b) (((b) & 0xC0) == 0x80)

/// `byte` is a lot easier to type than `uint8_t`.
typedef uint8_t byte;


utf8_decoder_t utf8dec_new()
{
    utf8_decoder_t dec =
    {
        .state = UTF8_STATE_RESET,
        .history = {0x00, 0x00, 0x00}
    };
    
    return dec;
}


utf8dec_result_t utf8dec_decode_next(utf8_decoder *self, char c, uint32_t *cp)
{
    byte b = (byte)c;
    
    // Comma-separated expressions evaluate to their rightmost value.
    // Allows for making an assignment and specifying a return value at the same time.
    #define SUCCESS()     (self->state = UTF8_STATE_RESET, UTF8_DEC_CODEPOINT)
    #define INVALID()     (self->state = UTF8_STATE_RESET, UTF8_DEC_INVALID)
    #define MBFAIL()      (self->state = UTF8_STATE_RESET, UTF8_DEC_MBFAIL)
    #define PENDING(next) (self->state = UTF8_STATE_##next, UTF8_DEC_PENDING)
    
    switch (self->state)
    {
        case UTF8_STATE_RESET:
            if ((b & 0x80) == 0x00)         // 1-unit codepoint (0xxxxxxx).
            {
                *cp = b & 0x7F;
                return SUCCESS();
            }
            else if ((b & 0xE0) == 0xC0)    // 2-unit codepoint (110xxxxx).
            {
                self->history[0] = b;
                return PENDING(TWO_1);
            }
            else if ((b & 0xF0) == 0xD0)    // 3-unit codepoint (1110xxxx).
            {
                self->history[0] = b;
                return PENDING(THREE_1);
            }
            else if ((b & 0xF8) == 0xF0)    // 4-unit codepoint (11110xxx).
            {
                self->history[0] = b;
                return PENDING(FOUR_1);
            }
            else                            // Invalid leading byte.
            {
                *cp = UTF8_REPLACEMENT_CHAR;
                return INVALID();
            }
            
        case UTF8_STATE_TWO_1:
            if (IS_CONT(b))
            {
                byte b1 = self->history[0];
                byte b2 = b;
                
                *cp = ((b1 & 0x1F) << 6) |
                      ((b2 & 0x3F));
                
                return SUCCESS();
            }
            else
            {
                return MBFAIL();
            }
            
        case UTF8_STATE_THREE_1:
            if (IS_CONT(b))
            {
                self->history[1] = b;
                return PENDING(THREE_2);
            }
            else
            {
                return MBFAIL();
            }
            
        case UTF8_STATE_THREE_2:
            if (IS_CONT(b))
            {
                byte b1 = self->history[0];
                byte b2 = self->history[1];
                byte b3 = b;
                
                *cp = ((b1 & 0x0F) << 12) |
                      ((b2 & 0x3F) <<  6) |
                      ((b3 & 0x3F));
                
                return SUCCESS();
            }
            else
            {        
                return MBFAIL();
            }
            
        case UTF8_STATE_FOUR_1:
            if (IS_CONT(b))
            {
                self->history[1] = b;
                return PENDING(FOUR_2);
            }
            else
            {
                return MBFAIL();
            }
            
        case UTF8_STATE_FOUR_2:
            if (IS_CONT(b))
            {
                self->history[2] = b;
                return PENDING(FOUR_3);
            }
            else
            {
                return MBFAIL();
            }
            
        case UTF8_STATE_FOUR_3:
            if (IS_CONT(b))
            {
                byte b1 = self->history[0];
                byte b2 = self->history[1];
                byte b3 = self->history[2];
                byte b4 = b;
                
                *cp = ((b1 & 0x07) << 18) |
                      ((b2 & 0x3F) << 12) |
                      ((b3 & 0x3F) <<  6) |
                      ((b4 & 0x3F));
                
                return SUCCESS();
            }
            else
            {
                return MBFAIL();
            }
            
        default:
            /*
             * This branch should be unreachable... unless the caller has gone
             * mucking around with the decoder's state without permission >:(
             */
            return (utf8dec_result_t)(0xFF);
    }
}
