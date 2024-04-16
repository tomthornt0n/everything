
#define STB_SPRINTF_IMPLEMENTATION
#include "external/stb_sprintf.h"

//~NOTE(tbt): character utilities

// TODO(tbt): make these work more generally and not just on ASCII

Function B32
CharIsSymbol(char c)
{
    return (('!' <= c && c <= '/') ||
            (':' <= c && c <= '@') ||
            ('[' <= c && c <= '`') ||
            ('{' <= c && c <= '~'));
}

Function B32
CharIsSpace(char c)
{
    return (' '  == c ||
            '\t' == c ||
            '\v' == c ||
            '\n' == c ||
            '\r' == c ||
            '\f' == c);
}

Function B32
CharIsNumber(char c)
{
    return '0' <= c && c <= '9';
}

Function B32
CharIsUppercase(char c)
{
    return 'A' <= c && c <= 'Z';
}

Function B32
CharIsLowercase(char c)
{
    return 'a' <= c && c <= 'z';
}

Function B32
CharIsLetter(char c)
{
    return CharIsUppercase(c) || CharIsLowercase(c);
}

Function B32
CharIsAlphanumeric(char c)
{
    return CharIsLetter(c) || CharIsNumber(c);
}

Function B32
CharIsPrintable(char c)
{
    return CharIsAlphanumeric(c) || CharIsSpace(c) || CharIsSymbol(c);
}

Function char
CharLowerFromUpper(char c)
{
    char result = c;
    if(CharIsUppercase(c))
    {
        result ^= 1 << 5;
    }
    return result;
}

Function char
CharUpperFromLower(char c)
{
    char result = c;
    if(CharIsLowercase(c))
    {
        result ^= 1 << 5;
    }
    return result;
}

//~NOTE(tbt): c-string helpers

Function U64
CStringCalculateUTF8Length(const char *cstring)
{
    U64 len = 0;
    S8 str = { .buffer = cstring, .len = ~((U64)0) };
    
    UTFConsume consume = CodepointFromUTF8(str, len);
    while(True)
    {
        if('\0' == consume.codepoint)
        {
            break;
        }
        else
        {
            len += consume.advance;
            consume = CodepointFromUTF8(str, len);
        }
    }
    return len;
}

Function U64
CStringCalculateUTF16Length(const wchar_t *cstring)
{
    U64 len = 0;
    S16 str = { .buffer = cstring, .len = ~((U64)0) };
    
    UTFConsume consume = CodepointFromUTF16(str, len);
    while(True)
    {
        if(0 == consume.codepoint)
        {
            break;
        }
        else
        {
            len += consume.advance;
            consume = CodepointFromUTF16(str, len);
        }
    }
    return len;
}

Function S8
CStringAsS8(const char *cstring)
{
    S8 result =
    {
        .buffer = cstring,
        .len = (0 == cstring) ? 0 : CStringCalculateUTF8Length(cstring),
    };
    return result;
}

Function S16
CStringAsS16(const wchar_t *cstring)
{
    S16 result =
    {
        .buffer = cstring,
        .len = (0 == cstring) ? 0 : CStringCalculateUTF16Length(cstring),
    };
    return result;
}

//~NOTE(tbt): unicode conversions

Function B32
UTF8IsContinuationByte(S8 string, U64 index)
{
    unsigned char bit_7 = !!(string.buffer[index] & Bit(7));
    unsigned char bit_6 = !!(string.buffer[index] & Bit(6));
    B32 result = (1 == bit_7 && 0 == bit_6);
    return result;
}

Function UTFConsume
CodepointFromUTF8(S8 string, U64 index)
{
    UTFConsume result = { ~((U32)0), 1 };
    
    if(index < string.len && !UTF8IsContinuationByte(string, index))
    {
        U64 max = string.len - index;
        
        unsigned char utf8_class[32] = 
        {
            1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,2,2,2,2,3,3,4,5,
        };
        
        U32 bitmask_3 = 0x07;
        U32 bitmask_4 = 0x0F;
        U32 bitmask_5 = 0x1F;
        U32 bitmask_6 = 0x3F;
        
        U8 byte = string.buffer[index];
        U8 byte_class = utf8_class[byte >> 3];
        
        switch(byte_class)
        {
            case(1):
            {
                result.codepoint = byte;
            } break;
            
            case(2):
            {
                if(2 <= max)
                {
                    U8 cont_byte = string.buffer[index + 1];
                    if(0 == utf8_class[cont_byte >> 3])
                    {
                        result.codepoint = (byte & bitmask_5) << 6;
                        result.codepoint |= (cont_byte & bitmask_6);
                        result.advance = 2;
                    }
                }
            } break;
            
            case(3):
            {
                if (3 <= max)
                {
                    U8 cont_byte[2] = { string.buffer[index + 1], string.buffer[index + 2] };
                    if (0 == utf8_class[cont_byte[0] >> 3] &&
                        0 == utf8_class[cont_byte[1] >> 3])
                    {
                        result.codepoint = (byte & bitmask_4) << 12;
                        result.codepoint |= ((cont_byte[0] & bitmask_6) << 6);
                        result.codepoint |= (cont_byte[1] & bitmask_6);
                        result.advance = 3;
                    }
                }
            } break;
            
            case(4):
            {
                if(4 <= max)
                {
                    U8 cont_byte[3] =
                    {
                        string.buffer[index + 1],
                        string.buffer[index + 2],
                        string.buffer[index + 3]
                    };
                    if(0 == utf8_class[cont_byte[0] >> 3] &&
                       0 == utf8_class[cont_byte[1] >> 3] &&
                       0 == utf8_class[cont_byte[2] >> 3])
                    {
                        result.codepoint = (byte & bitmask_3) << 18;
                        result.codepoint |= (cont_byte[0] & bitmask_6) << 12;
                        result.codepoint |= (cont_byte[1] & bitmask_6) << 6;
                        result.codepoint |= (cont_byte[2] & bitmask_6);
                        result.advance = 4;
                    }
                }
            } break;
        }
    }
    return result;
}

Function S16
S16FromS8(Arena *arena, S8 string)
{
    S16 result = {0};
    
    UTFConsume consume;
    for(U64 i = 0;
        i < string.len;
        i += consume.advance)
    {
        consume = CodepointFromUTF8(string, i);
        // NOTE(tbt): UTF16FromCodepoint allocates unaligned so characters guaranteed to be
        //            contiguous in arena - no need to copy anything :)
        S16 s16 = UTF16FromCodepoint(arena, consume.codepoint);
        if (!result.buffer)
        {
            result.buffer = s16.buffer;
        }
        result.len += s16.len;
    }
    // NOTE(tbt): null terminate to make easy to use with system APIs
    PushArray(arena, wchar_t, 1);
    
    return result;
}

Function S32
S32FromS8(Arena *arena, S8 string)
{
    S32 result = {0};
    
    UTFConsume consume;
    for(U64 i = 0;
        i < string.len;
        i += consume.advance)
    {
        consume = CodepointFromUTF8(string, i);
        
        U32 *character = PushArray(arena, U32, 1);
        if (!result.buffer)
        {
            result.buffer = character;
        }
        *character = consume.codepoint;
        result.len += 1;
    }
    
    return result;
}

Function UTFConsume
CodepointFromUTF16(S16 string, U64 index)
{
    U64 max = string.len - index;
    
    UTFConsume result = { string.buffer[index + 0], 1 };
    if(1 < max &&
       0xD800 <= string.buffer[index + 0] && string.buffer[index + 0] < 0xDC00 &&
       0xDC00 <= string.buffer[index + 1] && string.buffer[index + 1] < 0xE000)
    {
        result.codepoint = ((string.buffer[index + 0] - 0xD800) << 10) | (string.buffer[index + 1] - 0xDC00);
        result.advance = 2;
    }
    return result;
}

Function S8
S8FromS16(Arena *arena, S16 string)
{
    S8 result = {0};
    
    UTFConsume consume;
    for(U64 i = 0;
        i < string.len;
        i += consume.advance)
    {
        consume = CodepointFromUTF16(string, i);
        S8 s8 = UTF8FromCodepoint(arena, consume.codepoint);
        if(0 == result.buffer)
        {
            result.buffer = s8.buffer;
        }
        result.len += s8.len;
    }
    // NOTE(tbt): null terminate to make easy to use with system APIs
    PushArray(arena, char, 1);
    
    return result;
}

Function S32
S32FromS16(Arena *arena, S16 string)
{
    S32 result = {0};
    
    UTFConsume consume;
    for(U64 i = 0;
        i < string.len;
        i += consume.advance)
    {
        consume = CodepointFromUTF16(string, i);
        
        U32 *character = PushArray(arena, U32, 1);
        if (!result.buffer)
        {
            result.buffer = character;
        }
        *character = consume.codepoint;
        result.len += 1;
    }
    
    return result;
}

// NOTE(tbt): unlike most other string Functions, does not allocate and write a null
//            terminator after the buffer
Function S8
UTF8FromCodepoint(Arena *arena, U32 codepoint)
{
    S8 result;
    
    char *buffer = 0;
    
    if(codepoint == ~((U32)0))
    {
        result.len = 1;
        buffer = PushArray(arena, char, result.len);
        buffer[0] = '?';
    }
    if(codepoint <= 0x7f)
    {
        result.len = 1;
        buffer = PushArray(arena, char, result.len);
        buffer[0] = codepoint;
    }
    else if(codepoint <= 0x7ff)
    {
        result.len = 2;
        buffer = PushArray(arena, char, result.len);
        buffer[0] = 0xc0 | (codepoint >> 6);
        buffer[1] = 0x80 | (codepoint & 0x3f);
    }
    else if(codepoint <= 0xffff)
    {
        result.len = 3;
        buffer = PushArray(arena, char, result.len);
        buffer[0] = 0xe0 | ((codepoint >> 12));
        buffer[1] = 0x80 | ((codepoint >>  6) & 0x3f);
        buffer[2] = 0x80 | ((codepoint & 0x3f));
    }
    else if(codepoint <= 0xffff)
    {
        result.len = 4;
        buffer = PushArray(arena, char, result.len);
        buffer[0] = 0xf0 | ((codepoint >> 18));
        buffer[1] = 0x80 | ((codepoint >> 12) & 0x3f);
        buffer[2] = 0x80 | ((codepoint >>  6) & 0x3f);
        buffer[3] = 0x80 | ((codepoint & 0x3f));
    }
    
    result.buffer = buffer;
    
    return result;
}

// NOTE(tbt): unlike most other string Functions, does not allocate and write a null
//            terminator after the buffer
Function S16
UTF16FromCodepoint(Arena *arena, U32 codepoint)
{
    S16 result;
    
    wchar_t *buffer = 0;
    
    if(codepoint == ~((U32)0))
    {
        result.len = 1;
        buffer = PushArray(arena, wchar_t, result.len);
        buffer[0] = '?';
    }
    else if(codepoint < 0x10000)
    {
        result.len = 1;
        buffer = PushArray(arena, wchar_t, result.len);
        buffer[0] = codepoint;
    }
    else
    {
        result.len = 2;
        buffer = PushArray(arena, wchar_t, result.len);
        U64 v = codepoint - 0x10000;
        buffer[0] = '?';
        buffer[0] = 0xD800 + (v >> 10);
        buffer[1] = 0xDC00 + (v & 0x03FF);
    }
    
    result.buffer = buffer;
    
    return result;
}

Function S8
S8FromS32(Arena *arena, S32 string)
{
    S8 result = {0};
    
    for(U64 i = 0;
        i < string.len;
        i += 1)
    {
        S8 s8 = UTF8FromCodepoint(arena, string.buffer[i]);
        if(0 == result.buffer)
        {
            result.buffer = s8.buffer;
        }
        result.len += s8.len;
    }
    // NOTE(tbt): null terminate to make easy to use with system APIs
    PushArray(arena, char, 1);
    
    return result;
}

Function S16
S16FromS32(Arena *arena, S32 string)
{
    S16 result = {0};
    
    for(U64 i = 0;
        i < string.len;
        i += 1)
    {
        S16 s16 = UTF16FromCodepoint(arena, string.buffer[i]);
        if(0 == result.buffer)
        {
            result.buffer = s16.buffer;
        }
        result.len += s16.len;
    }
    // NOTE(tbt): null terminate to make easy to use with system APIs
    PushArray(arena, S16, 1);
    
    return result;
}

//~NOTE(tbt): string operations

Function S8
S8Clone(Arena *arena, S8 string)
{
    S8 result = string;
    if(0 != string.len && !ArenaHas(arena, string.buffer))
    {
        char *buffer = PushArray(arena, char, string.len + 1);
        MemoryCopy(buffer, string.buffer, string.len);
        result.buffer = buffer;
    }
    return result;
}

Function S16
S16Clone(Arena *arena, S16 string)
{
    S16 result = string;
    if(!ArenaHas(arena, string.buffer))
    {
        wchar_t *buffer = PushArray(arena, wchar_t, string.len + 1);
        MemoryCopy(buffer, string.buffer, string.len*sizeof(wchar_t));
        result.buffer = buffer;
    }
    return result;
}

Function S8
S8PrefixGet(S8 string, U64 len)
{
    S8 result =
    {
        .len = Min1U64(len, string.len),
        .buffer = string.buffer,
    };
    return result;
}

Function S8
S8SuffixGet(S8 string, U64 len)
{
    S8 result =
    {
        .len = Min1U64(len, string.len),
        .buffer = string.buffer,
    };
    result.buffer += string.len - result.len;
    return result;
}

Function U64
S8ByteIndexFromCharIndex(S8 string, U64 char_index)
{
    U64 result = 0;
    for(UTFConsume consume = CodepointFromUTF8(string, result);
        result < string.len && char_index > 0;
        char_index -= 1)
    {
        result += consume.advance;
        consume = CodepointFromUTF8(string, result);
    }
    if(char_index > 0)
    {
        result = string.len + 1;
    }
    return result;
}

Function U64
S8CharIndexFromByteIndex(S8 string, U64 byte_index)
{
    U64 char_index = 0;
    
    U64 i = 0;
    for(UTFConsume consume = CodepointFromUTF8(string, i);
        i < byte_index && i < string.len;
        i += consume.advance)
    {
        char_index += 1;
        consume = CodepointFromUTF8(string, i);
    }
    
    if(byte_index > string.len)
    {
        char_index += byte_index - string.len;
    }
    
    return char_index;
}

Function S8
S8FromFmtV(Arena *arena, const char *fmt, va_list args)
{
    S8 result = {0};
    if(0 == fmt)
    {
        result = S8("NULL");
    }
    else if(0 != fmt[0])
    {
        va_list _args;
        va_copy(_args, args);
        result.len = stbsp_vsnprintf(0, 0, fmt, args);
        char *buffer = ArenaPushNoZero(arena, result.len + 1, _Alignof(char));
        stbsp_vsnprintf(buffer, result.len + 1, fmt, _args);
        buffer[result.len] = 0;
        result.buffer = buffer;
        va_end(_args);
    }
    return result;
}

Function S8
S8FromFmt(Arena *arena, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    S8 result = S8FromFmtV(arena, fmt, args);
    va_end(args);
    return result;
}

Function U64
S8Match(S8 a, S8 b, MatchFlags flags)
{
    U64 matched_len = 0;
    
    if((flags & MatchFlags_RightSideSloppy) || a.len == b.len)
    {
        U64 length_to_compare = Min(a.len, b.len);
        
        // TODO(tbt): work with unicode
        for(U64 character_index = 0;
            character_index < length_to_compare;
            character_index += 1)
        {
            B32 char_is_match = False;
            if(flags & MatchFlags_CaseInsensitive)
            {
                char_is_match = (CharLowerFromUpper(a.buffer[character_index]) ==
                                 CharLowerFromUpper(b.buffer[character_index]));
            }
            else
            {
                char_is_match = (a.buffer[character_index] == b.buffer[character_index]);
            }
            
            if(char_is_match)
            {
                matched_len += 1;
            }
            else
            {
                if(!(flags & MatchFlags_RightSideSloppy))
                {
                    matched_len = 0;
                }
                break;
            }
        }
    }
    
    return matched_len;
}

Function S8
S8Substring(S8 h, S8 n, MatchFlags flags)
{
    S8 result = {0};
    
    // NOTE(tbt): doesn't make any sense
    Assert(0 == (flags & MatchFlags_RightSideSloppy));
    
    for(U64 character_index = 0;
        n.len > 0 && n.len <= h.len - character_index;
        character_index += 1)
    {
        S8 comp = { &h.buffer[character_index], n.len };
        if(S8Match(comp, n, flags))
        {
            result = comp;
            break;
        }
    }
    
    return result;
}

Function B32
S8HasSubstring(S8 h, S8 n, MatchFlags flags)
{
    B32 result = (S8Substring(h, n, flags).len > 0);
    return result;
}

Function S8
S8Advance(S8 *string, U64 n)
{
    U64 to_advance = Min1U64(n, string->len);
    S8 result =
    {
        .buffer = string->buffer,
        .len = n,
    };
    string->buffer += n;
    string->len = (string->len >= n) ? string->len - n : 0;
    return result;
}

Function B32
S8Consume(S8 *string, S8 consume, MatchFlags flags)
{
    B32 result = False;
    U64 matched_chars = S8Match(*string, consume, MatchFlags_RightSideSloppy | flags);
    if(((flags & MatchFlags_RightSideSloppy) && matched_chars) || (matched_chars == consume.len))
    {
        S8Advance(string, matched_chars);
        result = True;
    }
    return result;
}

Function S8
S8Replace(Arena *arena, S8 string, S8 a, S8 b, MatchFlags flags)
{
    S8 result = {0};
    
    S8Split split;
    
    split = S8SplitMake(string, a, flags);
    while(S8SplitNext(&split))
    {
        result.len += split.current.len + b.len;
    }
    char *buffer = ArenaPushNoZero(arena, result.len + 1, _Alignof(char));
    
    U64 i = 0;
    split = S8SplitMake(string, a, flags);
    while(S8SplitNext(&split))
    {
        MemoryCopy(&buffer[i], split.current.buffer, split.current.len);
        i += split.current.len;
        MemoryCopy(&buffer[i], b.buffer, b.len);
        i += b.len;
    }
    
    buffer[result.len] = 0;
    
    result.buffer = buffer;
    
    return result;
};

Function B32
S16Match(S16 a, S16 b, MatchFlags flags)
{
    B32 is_match;
    
    if(!(flags & MatchFlags_RightSideSloppy) && a.len != b.len)
    {
        is_match = False;
    }
    else
    {
        U64 length_to_compare = Min(a.len, b.len);
        
        // TODO(tbt): work with unicode
        if(flags & MatchFlags_CaseInsensitive)
        {
            is_match = True;
            for(U64 character_index = 0;
                character_index < length_to_compare;
                character_index += 1)
            {
                if(CharLowerFromUpper(a.buffer[character_index]) !=
                   CharLowerFromUpper(b.buffer[character_index]))
                {
                    is_match = False;
                    break;
                }
            }
        }
        else
        {
            is_match = MemoryMatch(a.buffer, b.buffer, length_to_compare);
        }
    }
    
    return is_match;
}

#include "external/MurmurHash3.cpp"

Function U64
S8Hash(S8 string)
{
    U64 result[2];
    MurmurHash3_x64_128(string.buffer, string.len*sizeof(string.buffer[0]), 0, &result);
    return result[0];
}

Function U64
S16Hash(S16 string)
{
    U64 result[2];
    MurmurHash3_x64_128(string.buffer, string.len*sizeof(string.buffer[0]), 0, &result);
    return result[0];
}

Function S8
S8LFFromCRLF(Arena *arena, S8 string)
{
    S8 result = {0};
    
    UTFConsume consume;
    for(U64 i = 0;
        i < string.len;
        i += consume.advance)
    {
        consume = CodepointFromUTF8(string, i);
        UTFConsume next_ch = CodepointFromUTF8(string, i + consume.advance);
        if('\r' == consume.codepoint &&
           '\n' == next_ch.codepoint)
        {
            char *c = PushArray(arena, char, 1);
            *c = '\n';
            if(0 == result.buffer)
            {
                result.buffer = c;
            }
            result.len += 1;
            i += next_ch.advance;
        }
        else
        {
            char *c = PushArray(arena, char, consume.advance);
            MemoryCopy(c, &string.buffer[i], consume.advance);
            if(0 == result.buffer)
            {
                result.buffer = c;
            }
            result.len += consume.advance;
        }
    }
    
    return result;
}

Function B32
FilenameHasTrailingSlash(S8 filename)
{
    B32 result = (filename.len > 0 && DirectorySeparatorChr == filename.buffer[filename.len - 1]);
    return result;
}

Function void
FilenameSplitExtension(S8 filename, S8 *without_extension, S8 *extension)
{
    const char *last_dot = 0;
    
    UTFConsume consume = CodepointFromUTF8(filename, 0);
    for(U64 character_index = 0;
        character_index < filename.len;
        character_index += consume.advance)
    {
        if('.' == consume.codepoint)
        {
            last_dot = &filename.buffer[character_index - consume.advance];
        }
        
        consume = CodepointFromUTF8(filename, character_index);
    }
    
    U64 len_without_extension = last_dot + 1 - filename.buffer;
    
    if(0 != without_extension)
    {
        without_extension->buffer = filename.buffer;
        without_extension->len = len_without_extension;
    }
    
    if(0 != extension)
    {
        extension->buffer = last_dot + 1;
        extension->len = filename.len - len_without_extension;
        if(0 == last_dot)
        {
            extension->buffer = &filename.buffer[filename.len];
            extension->len = 0;
        }
    }
}

Function S8
ExtensionFromFilename(S8 filename)
{
    S8 result = {0};
    FilenameSplitExtension(filename, 0, &result);
    return result;
}

Function S8
FilenameWithoutExtension(S8 filename)
{
    S8 result = {0};
    FilenameSplitExtension(filename, &result, 0);
    return result;
}

Function S8
FilenamePush(Arena *arena, S8 filename, S8 string)
{
    S8 result;
    
    if(FilenameHasTrailingSlash(filename) || 0 == filename.len)
    {
        result = S8FromFmt(arena, "%.*s%.*s",
                           FmtS8(filename),
                           FmtS8(string));
    }
    else
        
    {
        result = S8FromFmt(arena, "%.*s" DirectorySeparatorStr "%.*s",
                           FmtS8(filename),
                           FmtS8(string));
    }
    
    return result;
}

Function S8
FilenamePop(S8 filename)
{
    const char *last_slash = 0;
    
    UTFConsume consume = CodepointFromUTF8(filename, 0);
    for(U64 character_index = 0;
        character_index < filename.len;
        character_index += consume.advance)
    {
        if(DirectorySeparatorChr == consume.codepoint)
        {
            last_slash = &filename.buffer[character_index];
        }
        
        consume = CodepointFromUTF8(filename, character_index);
    }
    
    S8 result = filename;
    if(0 != last_slash)
    {
        result.buffer = filename.buffer;
        result.len = last_slash - filename.buffer - 1;
    }
    
    return result;
}

Function S8
FilenameLast(S8 filename)
{
    const char *last_slash = 0;
    
    UTFConsume consume = CodepointFromUTF8(filename, 0);
    for(U64 character_index = 0;
        character_index < filename.len;
        character_index += consume.advance)
    {
        if(DirectorySeparatorChr == consume.codepoint)
        {
            last_slash = &filename.buffer[character_index];
        }
        
        consume = CodepointFromUTF8(filename, character_index);
    }
    
    S8 result = filename;
    if(0 != last_slash)
    {
        result.buffer = last_slash;
        result.len = &filename.buffer[filename.len] - last_slash;
    }
    
    return result;
}

Function B32
FilenameIsChild(S8 parent, S8 filename)
{
    B32 result = False;
    
    while(!result && parent.len > 0)
    {
        if(S8Match(parent, filename, 0))
        {
            result  = True;
        }
        parent = FilenamePop(parent);
    }
    
    return result;
}

Function B32
S8IsWordBoundary(S8 string, U64 index)
{
    B32 result;
    
    if (0 < index && index < string.len)
    {
        result = ((CharIsSpace(string.buffer[index - 1]) || CharIsSymbol(string.buffer[index - 1])) &&
                  (!CharIsSpace(string.buffer[index]) &&
                   !CharIsSymbol(string.buffer[index])));
    }
    else
    {
        result = True;
    }
    
    return result;
}

Function B32
S8IsLineBoundary(S8 string, U64 index)
{
    B32 result;
    
    if (0 < index && index < string.len)
    {
        result = ('\n' == string.buffer[index - 1] ||
                  '\n' == string.buffer[index]);
    }
    else
    {
        result = True;
    }
    
    return result;
}

Function F64
S8Parse1F64(S8 string)
{
    // TODO(tbt): something less dumb
    
    F64 result = 0.0f;
    F64 sign = 1.0f;
    
    if(0 == string.len)
    {
        result = NaN;
    }
    else
    {
        if('-' == string.buffer[0])
        {
            sign = -1.0f;
            S8Advance(&string, 1);
        }
        
        B32 past_radix = False;
        F32 radix = 1.0f;
        
        while(string.len > 0)
        {
            if(CharIsNumber(string.buffer[0]))
            {
                if(past_radix)
                {
                    radix *= 10.0f;
                }
                result *= 10.0f;
                result += string.buffer[0] - '0';
            }
            else if(!past_radix && '.' == string.buffer[0])
            {
                past_radix = True;
            }
            else
            {
                result = NaN;
                break;
            }
            
            S8Advance(&string, 1);
        }
        
        result = sign*(result / radix);
    }
    
    return result;
}

Function U64
S8Parse1U64(S8 string)
{
    U64 result = 0;
    
    while(string.len > 0)
    {
        if(CharIsNumber(string.buffer[0]))
        {
            result *= 10;
            result += string.buffer[0] - '0';
        }
        else
        {
            result = 0;
            break;
        }
        
        S8Advance(&string, 1);
    }
    
    return result;
}

Function S8Split
S8SplitMake(S8 string, S8 delimiter, MatchFlags flags)
{
    S8Split result =
    {
        .delimiter = delimiter,
        .flags = flags,
        .string = string,
        .index = ~(U64)0,
        .is_done = (0 == string.len),
    };
    return result;
}

Function B32
S8SplitNext(S8Split *state)
{
    B32 result = !state->is_done;
    
    if(state->is_done)
    {
        state->current = S8("");
    }
    else
    {
        S8 substring = S8Substring(state->string, state->delimiter, state->flags);
        if(0 == substring.buffer)
        {
            state->current = state->string;
            state->string = (S8){0};
            state->is_done = True;
        }
        else
        {
            S8 next =
            {
                .buffer = state->string.buffer,
                .len = substring.buffer - state->string.buffer,
            };
            state->current = next;
            S8Advance(&state->string, next.len + state->delimiter.len);
        }
        state->index += 1;
    }
    
    return result;
}

Function I32
S8LevenshteinDistance(Arena *scratch_arena, S8 a, S8 b, MatchFlags flags)
{
    ArenaTemp scratch = ArenaTempBegin(scratch_arena);
    
    I32 *m = PushArray(scratch.arena, I32, (a.len + 1)*(b.len + 1));
    
    for(U64 character_index = 1; character_index <= a.len; character_index += 1)
    {
        m[character_index] = character_index;
    }
    
    for(U64 character_index = 1; character_index <= b.len; character_index += 1)
    {
        m[character_index*(a.len + 1)] = character_index;
    }
    
    for(U64 y = 1; y < b.len + 1; y += 1)
    {
        for(U64 x = 1; x < a.len + 1; x += 1)
        {
            I32 subst_cost = 0;
            if(flags & MatchFlags_CaseInsensitive)
            {
                subst_cost =
                (CharLowerFromUpper(a.buffer[x - 1]) !=
                 CharLowerFromUpper(b.buffer[y - 1]));
            }
            else
            {
                subst_cost = (a.buffer[x - 1] != b.buffer[y - 1]);
            }
            I32 s = m[(x - 1) + (y - 1)*(a.len + 1)] + subst_cost;
            I32 i = m[ x      + (y - 1)*(a.len + 1)] + 1;
            I32 d = m[(x - 1) +  y     *(a.len + 1)] + 1;
            m[x + y*(a.len + 1)] = Min1I(Min1I(s, i), d);
        }
    }
    
    I32 result = m[a.len + b.len*(a.len + 1)];
    
    ArenaTempEnd(scratch);
    
    return result;
}

//~NOTE(tbt): string lists

Function void
S8ListClear(S8List *list)
{
    list->first = 0;
    list->last = 0;
    list->count = 0;
}

Function void
S8ListPushExplicit(S8List *list, S8Node *string)
{
    if(0 == list->first)
    {
        Assert(0 == list->last);
        string->next = 0;
        list->first = string;
        list->last = string;
    }
    else
    {
        string->next = list->first;
        list->first = string;
    }
    list->count += 1;
}

Function void
S8ListAppendExplicit(S8List *list, S8Node *string)
{
    if(0 == list->last)
    {
        Assert(0 == list->first);
        list->first = string;
        list->last = string;
    }
    else
    {
        list->last->next = string;
        list->last = string;
    }
    string->next = 0;
    list->count += 1;
}

Function S8Node *
S8ListPush(Arena *arena, S8List *list, S8 string)
{
    S8Node *node = ArenaPushNoZero(arena, sizeof(*node), _Alignof(S8Node));
    node->string = S8Clone(arena, string);
    S8ListPushExplicit(list, node);
    return node;
}

Function S8Node *
S8ListPushFmtV(Arena *arena, S8List *list, const char *fmt, va_list args)
{
    S8Node *node = ArenaPushNoZero(arena, sizeof(*node), _Alignof(S8Node));
    node->string = S8FromFmtV(arena, fmt, args);
    S8ListPushExplicit(list, node);
    return node;
}

Function S8Node *
S8ListPushFmt(Arena *arena, S8List *list, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    S8Node *result = S8ListPushFmtV(arena, list, fmt, args);
    va_end(args);
    return result;
}

Function S8Node *
S8ListAppend(Arena *arena, S8List *list, S8 string)
{
    S8Node *node = ArenaPushNoZero(arena, sizeof(*node), _Alignof(S8Node));
    node->string = S8Clone(arena, string);
    S8ListAppendExplicit(list, node);
    return node;
}

Function S8Node *
S8ListAppendFmtV(Arena *arena, S8List *list, const char *fmt, va_list args)
{
    S8Node *node = ArenaPushNoZero(arena, sizeof(*node), _Alignof(S8Node));
    node->string = S8FromFmtV(arena, fmt, args);
    S8ListAppendExplicit(list, node);
    return node;
}

Function S8Node *
S8ListAppendFmt(Arena *arena, S8List *list, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    S8Node *result = S8ListAppendFmtV(arena, list, fmt, args);
    va_end(args);
    return result;
}

Function void
S8ListConcatenate(S8List *a, S8List b)
{
    if(0 == a->count)
    {
        *a = b;
    }
    else if(b.count > 0)
    {
        a->last->next = b.first;
        a->last = b.last;
    }
}

Function S8List
S8ListClone(Arena *arena, S8List list)
{
    S8List result = {0};
    for(S8ListForEach(list, node))
    {
        S8ListAppend(arena, &result, node->string);
    }
    return result;
}

Function S8
S8ListJoin(Arena *arena, S8List list)
{
    S8 result = {0};
    for(S8ListForEach(list, node))
    {
        char *buffer = ArenaPushNoZero(arena, node->string.len, _Alignof(char));
        MemoryCopy(buffer, node->string.buffer, node->string.len);
        if(0 == result.buffer)
        {
            result.buffer = buffer;
        }
        result.len += node->string.len;
    }
    // NOTE(tbt): null terminate to make easy to use with system APIs
    PushArray(arena, char, 1);
    
    return result;
}

Function S8
S8ListJoinFormatted(Arena *arena, S8List list, S8ListJoinFormat join)
{
    S8 result = {0};
    
    U64 prefix_and_suffix_len = join.prefix.len + join.suffix.len;
    
    if(join.before.len > 0)
    {
        char *buffer = ArenaPushNoZero(arena, join.before.len, _Alignof(char));
        result.len += join.before.len;
        MemoryCopy(buffer, join.before.buffer, join.before.len);
        if(0 == result.buffer)
        {
            result.buffer = buffer;
        }
    }
    
    for(S8ListForEach(list, node))
    {
        char *buffer = ArenaPushNoZero(arena, node->string.len + prefix_and_suffix_len, _Alignof(char));
        result.len += node->string.len + prefix_and_suffix_len;
        MemoryCopy(buffer, join.prefix.buffer, join.prefix.len);
        MemoryCopy(buffer + join.prefix.len, node->string.buffer, node->string.len);
        MemoryCopy(buffer + join.prefix.len + node->string.len, join.suffix.buffer, join.suffix.len);
        if(0 != node->next)
        {
            char *delimiter_copy = ArenaPushNoZero(arena, join.delimiter.len, _Alignof(char));
            result.len += join.delimiter.len;
            MemoryCopy(delimiter_copy, join.delimiter.buffer, join.delimiter.len);
        }
        if(0 == result.buffer)
        {
            result.buffer = buffer;
        }
    }
    
    if(join.after.len > 0)
    {
        char *buffer = ArenaPushNoZero(arena, join.after.len, _Alignof(char));
        result.len += join.after.len;
        MemoryCopy(buffer, join.after.buffer, join.after.len);
        if(0 == result.buffer)
        {
            result.buffer = buffer;
        }
    }
    
    // NOTE(tbt): null terminate to make easy to use with system APIs
    PushArray(arena, char, 1);
    
    return result;
}

Function S8
S8ListS8FromIndex(S8List list,
                  U64 index)
{
    S8 result = {0};
    
    S8Node *node = list.first;
    for(U64 i = 0;
        i < index && 0 != node;
        node = node->next)
    {
        i += 1;
    }
    if(0 != node)
    {
        result = node->string;
    }
    return result;
}

Function S8ListFind
S8ListIndexFromS8(S8List list,
                  S8 string,
                  MatchFlags flags)
{
    S8ListFind result = {0};
    for(S8ListForEach(list, i))
    {
        if(S8Match(string, i->string, flags))
        {
            result.has = True;
            break;
        }
        result.i += 1;
    }
    return result;
}

Function B32
S8ListHas(S8List list,
          S8 string,
          MatchFlags flags)
{
    B32 result = False;
    for(S8ListForEach(list, s))
    {
        if(S8Match(string, s->string, flags))
        {
            result = True;
            break;
        }
    }
    return result;
}

Function void
S8ListRefresh(S8List *list)
{
    // TODO(tbt): this is kind of dumb
    list->count = 0;
    list->last = 0;
    for(S8ListForEach(*list, s))
    {
        list->last = s;
        list->count += 1;
    }
}

Function void
S8ListRemoveExplicit(S8List *list, S8Node *string)
{
    for(S8Node **s = &list->first;
        0 != *s;
        s = &(*s)->next)
    {
        if(*s == string)
        {
            *s = string->next;
            break;
        }
    }
    S8ListRefresh(list);
}

Function S8
S8ListPop(S8List *list)
{
    S8 result = {0};
    if(0 != list->first)
    {
        result = list->first->string;
        S8ListRemoveExplicit(list, list->first);
    }
    return result;
}

Function S8Node *
S8ListRemoveFirstOccurenceOf(S8List *list,
                             S8 string,
                             MatchFlags flags)
{
    S8Node *result = 0;
    for(S8Node **s = &list->first;
        0 == result && 0 != (*s);
        s = &(*s)->next)
    {
        if(S8Match(string, (*s)->string, flags))
        {
            result = (*s);
            *s = (*s)->next;
            list->count -= 1;
            result->next = 0;
        }
    }
    S8ListRefresh(list);
    return result;
}

Function void
S8ListRemoveAllOccurencesOf(S8List *list,
                            S8 string,
                            MatchFlags flags)
{
    for(S8Node **s = &list->first;
        0 != *s;
        s = &(*s)->next)
    {
        if(S8Match(string, (*s)->string, flags))
        {
            *s = (*s)->next;
            list->count -= 1;
            if(0 == (*s))
            {
                break;
            }
        }
    }
    S8ListRefresh(list);
}

Function S8List
S8ListFromSplit(Arena *arena, S8 string, S8 delimiter, MatchFlags flags)
{
    S8List result = {0};
    
    S8Split split = S8SplitMake(string, delimiter, flags);
    while(S8SplitNext(&split))
    {
        S8ListAppend(arena, &result, split.current);
    }
    
    S8ListRefresh(&result);
    
    return result;
}

//~NOTE(tbt): compression

#if 0 && BuildZLib

Function S8
S8Deflate(Arena *arena, S8 string)
{
    S8 result = {0};
    
    if(string.len > 0)
    {
        unsigned char out[Kilobytes(256)];
        
        z_stream stream = {0};
        if(Z_OK == deflateInit(&stream, Z_DEFAULT_COMPRESSION))
        {
            stream.avail_in = string.len;
            stream.next_in = (char *)string.buffer;
            
            do
            {
                stream.avail_out = sizeof(out);
                stream.next_out = out;
                deflate(&stream, Z_FINISH);
                
                U64 have = sizeof(out) - stream.avail_out;
                char *buffer = ArenaPushNoZero(arena, have, _Alignof(char));
                if(0 == result.buffer)
                {
                    result.buffer = buffer;
                }
                MemoryCopy(buffer, out, have);
                result.len += have;
            } while (stream.avail_out == 0);
            
            deflateEnd(&stream);
        }
    }
    
    return result;
}

Function S8
S8Inflate(Arena *arena, S8 string)
{
    S8 result = {0};
    
    if(string.len > 0)
    {
        unsigned char out[Kilobytes(256)];
        
        z_stream stream = {0};
        if(Z_OK == inflateInit(&stream))
        {
            I32 rc;
            do
            {
                stream.avail_in = string.len;
                stream.next_in = (char *)string.buffer;
                
                do
                {
                    stream.avail_out = sizeof(out);
                    stream.next_out = out;
                    rc = inflate(&stream, Z_NO_FLUSH);
                    switch(rc)
                    {
                        case Z_NEED_DICT:
                        case Z_DATA_ERROR:
                        case Z_MEM_ERROR:
                        goto end;
                    }
                    
                    U64 have = sizeof(out) - stream.avail_out;
                    char *buffer = ArenaPushNoZero(arena, have, _Alignof(char));
                    if(0 == result.buffer)
                    {
                        result.buffer = buffer;
                    }
                    MemoryCopy(buffer, out, have);
                    result.len += have;
                } while (stream.avail_out == 0);
            } while (rc != Z_STREAM_END);
            
            end:;
            inflateEnd(&stream);
        }
    }
    
    // NOTE(tbt): null terminate to make easy to use with system APIs
    PushArray(arena, char, 1);
    
    return result;
}

#endif

//~NOTE(tbt): string builders

Function S8Builder
PushS8Builder(Arena *arena, U64 cap)
{
    S8Builder result = {0};
    result.cap = cap;
    result.buffer = PushArray(arena, char, cap);
    return result;
}

Function S8
S8FromBuilder(S8Builder builder)
{
    S8 result = 
    {
        .buffer = builder.buffer,
        .len = builder.len,
    };
    return result;
}

Function S8
S8FromBuilderAndRange(S8Builder builder, Range1U64 range)
{
    S8 result = 
    {
        .buffer = &builder.buffer[range.min],
        .len = range.max - range.min,
    };
    return result;
}

Function B32
S8BuilderReplaceRange(S8Builder *builder, Range1U64 range, S8 string)
{
    B32 result = False;
    
    if(range.max < range.min)
    {
        U64 temp = range.max;
        range.max = range.min;
        range.min = temp;
    }
    Assert(range.max <= builder->len);
    
    //-NOTE(tbt): shift section of builder after range.max to make room to copy in the string
    {
        U64 index = range.min + string.len;
        U64 n_bytes_to_copy = Min1U64(builder->len - range.max, (builder->cap > index) ? builder->cap - index : 0);
        MemoryMove(&builder->buffer[index], &builder->buffer[range.max], n_bytes_to_copy);
        
        result = result || (n_bytes_to_copy > 0 && index != range.max);
    }
    
    //-NOTE(tbt): update length
    U64 range_len = Measure1U64(range);
    if(string.len != range_len)
    {
        result = True;
        if(string.len > range_len)
        {
            builder->len += string.len - range_len;
        }
        else
        {
            builder->len -= range_len - string.len;
        }
        builder->len = Min1U64(builder->len, builder->cap);
    }
    
    //-NOTE(tbt): actually copy
    {
        U64 index = range.min;
        U64 n_bytes_to_copy = Min1U64(string.len, (builder->cap > index) ? builder->cap - index : 0);
        MemoryCopy(&builder->buffer[index], string.buffer, n_bytes_to_copy);
        
        result = result || (n_bytes_to_copy > 0);
    }
    
    return result;
}

Function void
S8BuilderAppend(S8Builder *builder, S8 string)
{
    U64 n_bytes_to_copy = Min1U64(string.len, builder->cap - builder->len);
    MemoryCopy(&builder->buffer[builder->len], string.buffer, n_bytes_to_copy);
    builder->len += n_bytes_to_copy;
}

Function void
S8BuilderAppendFmtV(S8Builder *builder, const char *fmt, va_list args)
{
    U64 n_remaining_bytes = builder->cap - builder->len;
    U64 string_len = stbsp_vsnprintf(&builder->buffer[builder->len], n_remaining_bytes , fmt, args);
    if(string_len > 0)
    {
        builder->len += Min1U64(string_len, n_remaining_bytes);
    }
}

Function void
S8BuilderAppendFmt(S8Builder *builder, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    S8BuilderAppendFmtV(builder, fmt, args);
    va_end(args);
}

Function void
S8BuilderAppendBytes(S8Builder *builder, void *data, U64 len)
{
    S8 string = { .buffer = data, .len = len, };
    S8BuilderAppend(builder, string);
}
