
//~NOTE(tbt): handles

Function B32 OpaqueHandleMatch (OpaqueHandle a, OpaqueHandle b)
{
    B32 result = (a.p == b.p &&
                  a.g == b.g);
    return result;
}

//~NOTE(tbt): asserts

Function void
AssertWithMessage(B32 c, char *msg)
{
    if(!c)
    {
        if(0 != assert_log_hook)
        {
            assert_log_hook("Assertion Failure", msg);
        }
        
        AssertBreak();
    }
}

//~NOTE(tbt): error reporting

Function void
ErrorPush(ErrorScope *scope, Arena *arena, U64 timestamp, S8 string)
{
    ErrorMsg *msg = PushArray(arena, ErrorMsg, 1);
    msg->timestamp = timestamp;
    msg->string = S8Clone(arena, string);
    msg->next = scope->first;
    if(0 != msg->next)
    {
        msg->next->prev = msg;
    }
    else
    {
        scope->last = msg;
    }
    scope->first = msg;
    scope->count += 1;
}

Function void
ErrorPushCond(ErrorScope *scope, Arena *arena, B32 success, U64 timestamp, S8 string)
{
    if(!success)
    {
        ErrorPush(scope, arena, timestamp, string);
    }
}

Function void
ErrorPushFmtV(ErrorScope *scope, Arena *arena, U64 timestamp, const char *fmt, va_list args)
{
    S8 string = S8FromFmtV(arena, fmt, args);
    ErrorPush(scope, arena, timestamp, string);
}

Function void
ErrorPushFmtCondV(ErrorScope *scope, Arena *arena, B32 success, U64 timestamp, const char *fmt, va_list args)
{
    S8 string = S8FromFmtV(arena, fmt, args);
    ErrorPushCond(scope, arena, success, timestamp, string);
}

Function void
ErrorPushFmt(ErrorScope *scope, Arena *arena, U64 timestamp, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    ErrorPushFmtV(scope, arena, timestamp, fmt, args);
    va_end(args);
}

Function void ErrorPushFmtCond(ErrorScope *scope, Arena *arena, B32 success, U64 timestamp, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    ErrorPushFmtCondV(scope, arena, success, timestamp, fmt, args);
    va_end(args);
}

//~NOTE(tbt): random number generation

Function U32
Noise1U(U32 a)
{
    U32 result = a;
    result ^= 2747636419;
    result *= 2654435769;
    result ^= result >> 16;
    result *= 2654435769;
    result ^= result >> 16;
    result *= 2654435769;
    return result;
}

Function I32
Noise2I(V2I a)
{
    I32 result;
    result = Noise1U(a.y);
    result = Noise1U(result + a.x);
    return result;
}

Function F32
Noise2F(V2F a)
{
    F32 result;
    
    I32 x_int = (I32)a.x;
    I32 y_int = (I32)a.y;
    F32 x_frac = a.x - x_int;
    F32 y_frac = a.y - y_int;
    I32 s = Noise2I(V2I(x_int, y_int));
    I32 t = Noise2I(V2I(x_int + 1, y_int));
    I32 u = Noise2I(V2I(x_int, y_int + 1));
    I32 v = Noise2I(V2I(x_int + 1, y_int + 1));
    F32 low = InterpolateSmooth1F((F32)s, (F32)t, x_frac);
    F32 high = InterpolateSmooth1F((F32)u, (F32)v, x_frac);
    result = InterpolateSmooth1F(low, high, y_frac);
    
    return result;
}

Function I32
RandNext1I(I32 *seed)
{
    I32 result = Noise1U(*seed);
    *seed += 1;
    return result;
}

Function I32
RandNextBounded1I(I32 *seed, I32 min, I32 max)
{
    I32 n = Abs1I(RandNext1I(seed));
    I32 range = max - min;
    I32 result = min + (Abs1I(n) % range);
    return result;
}

Function F32
Perlin2F(V2F a,
         F32 freq,
         I32 depth)
{
    F32 result;
    
    F32 xa = a.x*freq;
    F32 ya = a.y*freq;
    F32 amp = 1.0f;
    F32 fin = 0.0f;
    F32 div = 0.0f;
    for (I32 i = 0;
         i < depth;
         i += 1)
    {
        div += 256.0f*amp;
        fin += Noise2F(V2F(xa, ya))*amp;
        amp /= 2.0f;
        xa *= 2.0f;
        ya *= 2.0f;
    }
    result = fin / div;
    
    return result;
}

//~NOTE(tbt): time

Function DenseTime
DenseTimeFromDateTime(DateTime date_time)
{
    U64 encoded_year = EncodeU64FromI64(date_time.year);
    
    DenseTime result = 0;
    result = (result + encoded_year)   * 12;
    result = (result + date_time.mon)  * 31;
    result = (result + date_time.day)  * 24;
    result = (result + date_time.hour) * 60;
    result = (result + date_time.min)  * 61;
    result = (result + date_time.sec)  * 1000;
    result = (result + date_time.msec) * 1;
    return result;
}

Function DateTime
DateTimeFromDenseTime(DenseTime dense_time)
{
    DateTime result = {0};
    result.msec = dense_time % 1000;
    dense_time /= 1000;
    result.sec = dense_time % 61;
    dense_time /= 61;
    result.min = dense_time % 60;
    dense_time /= 60;
    result.hour = dense_time % 24;
    dense_time /= 24;
    result.day = dense_time % 31;
    dense_time /= 31;
    result.mon = dense_time % 12;
    dense_time /= 12;
    result.year = (I16)DecodeI64FromU64(dense_time);
    return result;
}

Function S8
S8FromDateTime(Arena *arena, DateTime date_time)
{
    S8 result = S8FromFmt(arena, "%02d:%02d %02d/%02d/%d",
                          date_time.hour,
                          date_time.min,
                          date_time.day + 1,
                          date_time.mon + 1,
                          date_time.year);
    return result;
}

Function S8
S8FromDenseTime(Arena *arena, DenseTime dense_time)
{
    DateTime date_time = DateTimeFromDenseTime(dense_time);
    S8 result = S8FromDateTime(arena, date_time);
    return result;
}

Function DateTime
DateTimeFromDDMMYYYYString(S8 string, I16 current_year)
{
    DateTime result = {0};
    
    S8Split split = S8SplitMake(string, S8("/"), 0);
    I16 day = 0, mon = 0, year = 0;
    if(S8SplitNext(&split)) { day = (I16)S8Parse1U64(split.current); }
    if(S8SplitNext(&split)) { mon = (I16)S8Parse1U64(split.current); }
    if(S8SplitNext(&split)) { year = (I16)S8Parse1U64(split.current); }
    if(1 <= year && year <= 999)
    {
        year += (current_year / 1000)*1000;
    }
    I32 days_in_month[] =
    {
        31, 28 + IsLeapYear(year), 31, 30, 31, 30, 31, 31, 30, 31, 30, 31,
    };
    B32 is_valid = ((1 <= mon && mon <= 12) &&
                    (1 <= day && day <= days_in_month[mon - 1]));
    
    if(is_valid)
    {
        result.year = year;
        result.mon = mon - 1;
        result.day = day - 1;
    }
    else
    {
        result = DateTimeFromDenseTime(0);
    }
    
    return result;
}

Function B32
IsLeapYear(I16 year)
{
    B32 is_divisible_by_4 = ( 0 == (year % 4));
    B32 is_divisible_by_100 = ( 0 == (year % 100));
    B32 is_divisible_by_400 = ( 0 == (year % 400));
    B32 result = (is_divisible_by_4 && (!is_divisible_by_100 || is_divisible_by_400));
    return result;
}

Function DateTime
TommorowFromDateTime(DateTime today)
{
    I32 days_per_month[] = 
    {
        31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31,
    };
    if(IsLeapYear(today.year))
    {
        days_per_month[1] = 29;
    }
    
    DateTime result = today;
    result.day += 1;
    if(result.day >= days_per_month[result.mon])
    {
        result.day = 0;
        result.mon += 1;
        if(result.mon >= 12)
        {
            result.mon = 0;
            result.year += 1;
        }
    }
    
    return result;
}

Function DateTime
YesterdayFromDateTime(DateTime today)
{
    DateTime result = today;
    if(result.day > 0)
    {
        result.day -= 1;
    }
    else
    {
        I32 days_per_month[] = 
        {
            31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31,
        };
        if(IsLeapYear(today.year))
        {
            days_per_month[1] = 29;
        }
        
        if(result.mon > 0)
        {
            result.mon -= 1;
        }
        else
        {
            result.mon = 11;
            result.year -= 1;
        }
        
        result.day = days_per_month[result.mon] - 1;
    }
    
    return result;
}

Function DayOfWeek
DayOfWeekFromDateTime(DateTime date)
{
    // NOTE(tbt): only works for positive years
    // TODO(tbt): floor correctly!
    
    I32 d = date.day + 1;
    I32 m = date.mon + 1;
    if(m < 3)
    {
        m += 12;
    }
    I32 c = date.year / 100;
    I32 y = date.year - 100*c;
    I32 w = (((13*(m + 1)) / 5) + (y / 4) + (c / 4) + d + y - 2*c + 5) % 7;
    
    return w;
}

//~NOTE(tbt): sorting

#if BuildCompilerMSVC
# pragma warning(push, 1)
#endif
#include "external/qsort.c"
#if BuildCompilerMSVC
# pragma warning(pop)
#endif

Function void
Sort(void *base,
     U64 n_items, U64 bytes_per_item,
     SortComparator *comparator,
     void *arg)
{
    MUSL__qsort_r(base, n_items, bytes_per_item, comparator, arg);
}

Function I32
SortCompare1U64(U64 a, U64 b)
{
    I32 result = 0;
    if(a > b)
    {
        result = 1;
    }
    else if(a < b)
    {
        result = -1;
    }
    return result;
}

Function I32
SortCompareLexicographic(S8 a, S8 b, MatchFlags flags)
{
    I32 result = 0;
    
    U64 len_to_compare = Min1U64(a.len, b.len);
    
    U64 i_a = 0;
    U64 i_b = 0;
    while(0 == result && i_a < a.len && i_b < b.len)
    {
        if(CharIsNumber(a.buffer[i_a]) &&
           CharIsNumber(b.buffer[i_b]))
        {
            I32 num_a = 0;
            I32 num_b = 0;
            while(CharIsNumber(a.buffer[i_a]))
            {
                num_a *= 10;
                num_a += a.buffer[i_a] - '0';
                i_a += 1;
            }
            while(CharIsNumber(b.buffer[i_b]))
            {
                num_b *= 10;
                num_b += b.buffer[i_b] - '0';
                i_b += 1;
            }
            result = num_a - num_b;
        }
        else
        {
            char char_a = a.buffer[i_a];
            char char_b = b.buffer[i_b];
            if(flags & MatchFlags_CaseInsensitive)
            {
                char_a = CharLowerFromUpper(char_a);
                char_b = CharLowerFromUpper(char_b);
            }
            
            result = char_a - char_b;
            i_a += 1;
            i_b += 1;
        }
    }
    
    return result;
}

Function I32
SortCompareChronological(DateTime a, DateTime b)
{
    DenseTime dense_a = DenseTimeFromDateTime(a);
    DenseTime dense_b = DenseTimeFromDateTime(b);
    I32 result = SortCompare1U64(dense_a, dense_b);
    return result;
}

//~NOTE(tbt): random stuff

Function U32
NextPowerOfTwo1U(U32 x)
{
    U32 result = 0;
    if(0 != x)
    {
        result = x;
        result -= 1;
        result |= result >> 1;
        result |= result >> 2;
        result |= result >> 4;
        result |= result >> 8;
        result |= result >> 16;
        result += 1;
    }
    return result;
}

Function U64
EncodeU64FromI64(I64 a)
{
    U64 result = (((U64)a) << 1) | (((U64)a) >> 63);
    return result;
}

Function I64
DecodeI64FromU64(U64 a)
{
    I64 result = (I64)((a >> 1) | (a << 63));
    return result;
}

Function I32
PopulationCount1U(U32 a)
{
    U32 count = a - ((a >> 1) & 033333333333) - ((a >> 2) & 011111111111);
    I32 result = ((count + (count >> 3)) & 030707070707) % 63;
    return result;
}

Function U64
RingWrite(U8 *buffer, U64 cap, U64 write_pos, const void *src, U64 bytes)
{
    U64 part_1_offset = (write_pos % cap);
    U64 part_2_offset = 0;
    
    S8 part_1 = { .buffer = src, .len = bytes, };
    S8 part_2 = {0};
    
    if(part_1_offset + bytes > cap)
    {
        part_2 = part_1;
        part_1 = S8Advance(&part_2, cap - part_1_offset);
    }
    
    MemoryCopy(&buffer[part_1_offset], part_1.buffer, part_1.len);
    MemoryCopy(&buffer[part_2_offset], part_2.buffer, part_2.len);
    
    return bytes;
}

Function U64
RingRead(U8 *buffer, U64 cap, U64 read_pos, void *dest, U64 bytes)
{
    U64 read_offset = (read_pos % cap);
    
    S8 part_1 = { .buffer = &buffer[read_offset], .len = bytes,  };
    S8 part_2 = { .buffer = &buffer[0], .len = 0, };
    
    if(read_offset + bytes > cap)
    {
        part_2.len = read_offset + bytes - cap;
        part_1.len -= part_2.len;
    }
    
    void *part_1_out = dest;
    void *part_2_out = PtrFromInt(IntFromPtr(dest) + part_1.len);
    
    MemoryCopy(part_1_out, part_1.buffer, part_1.len);
    MemoryCopy(part_2_out, part_2.buffer, part_2.len);
    
    return bytes;
}
