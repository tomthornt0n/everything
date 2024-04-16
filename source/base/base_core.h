
//~NOTE(tbt): handles

typedef union OpaqueHandle OpaqueHandle;
union OpaqueHandle
{
    U64 a[2];
    struct { U64 p, g; };
};

Function B32 OpaqueHandleMatch (OpaqueHandle a, OpaqueHandle b);

//~NOTE(tbt): asserts

#if BuildOSWindows
# define AssertBreak() __debugbreak()
#elif BuildCompilerClang || BuildCompilerGCC
# define AssertBreak() __builtin_trap()
#else
# define AssertBreak() I32 volatile i = *((I32 volatile *)0)
#endif

typedef void AssertLogHook(char *title, char *msg);
Global AssertLogHook *assert_log_hook = 0;

Function void AssertWithMessage (B32 c, char *msg);

#if BuildEnableAsserts
# define Assert(C) Statement(AssertWithMessage((C), __FILE__ "(" Stringify(__LINE__) "): assertion failure: " #C);)
#else
# define Assert(C) Statement((void)0;)
#endif

//~NOTE(tbt): data access flags

typedef U64 DataAccessFlags;
enum DataAccessFlags
{
    DataAccessFlags_Read = Bit(0),
    DataAccessFlags_Write = Bit(1),
    DataAccessFlags_Execute = Bit(2),
};

//~NOTE(tbt): error reporting

typedef struct ErrorMsg ErrorMsg;
struct ErrorMsg
{
    ErrorMsg *next;
    ErrorMsg *prev;
    U64 timestamp;
    S8 string;
};

typedef struct ErrorScope ErrorScope;
struct ErrorScope
{
    ArenaTemp checkpoint;
    ErrorScope *next;
    ErrorMsg *first;
    ErrorMsg *last;
    U64 count;
};

Function void ErrorPush (ErrorScope *errors, Arena *arena, U64 timestamp, S8 string);
Function void ErrorPushCond (ErrorScope *errors, Arena *arena, B32 success, U64 timestamp, S8 string);
Function void ErrorPushFmtV (ErrorScope *errors, Arena *arena, U64 timestamp, const char *fmt, va_list args);
Function void ErrorPushFmtCondV (ErrorScope *errors, Arena *arena, B32 success, U64 timestamp, const char *fmt, va_list args);
Function void ErrorPushFmt (ErrorScope *errors, Arena *arena, U64 timestamp, const char *fmt, ...);
Function void ErrorPushFmtCond (ErrorScope *errors, Arena *arena, B32 success, U64 timestamp, const char *fmt, ...);

//~NOTE(tbt): random number generation

// NOTE(tbt): 'noise' via hashing a position
Function U32 Noise1U (U32 a);
Function I32 Noise2I (V2I a);
Function F32 Noise2F (V2F a);

// NOTE(tbt): pseudo-random sequence
Function I32 RandNext1I (I32 *seed);
Function I32 RandNextBounded1I (I32 *seed, I32 min, I32 max);

// NOTE(tbt): perlin noise
Function F32 Perlin2F (V2F a, F32 freq, I32 depth);

//~NOTE(tbt): time

typedef struct DateTime DateTime;
struct DateTime
{
    U16 msec; // NOTE(tbt): milliseconds after the seconds, [0, 999]
    U8 sec; // NOTE(tbt): seconds after the minute, [0, 60] (60 allows for leap seconds)
    U8 min; // NOTE(tbt): minutes after the hour, [0, 59]
    U8 hour; // NOTE(tbt): hours into the day, [0, 23]
    U8 day; // NOTE(tbt): days into the month, [0, 30]
    U8 mon; // NOTE(tbt): months into the year, [0, 11]
    I16 year; // NOTE(tbt): ... -1 == -2 BCE; 0 == -1 BCE; 1 == 1 CE, 2 == 2 CE; ...
};

typedef U64 DenseTime;

typedef enum DayOfWeek DayOfWeek;
enum DayOfWeek
{
    DayOfWeek_Monday,
    DayOfWeek_Tuesday,
    DayOfWeek_Wednesday,
    DayOfWeek_Thursday,
    DayOfWeek_Friday,
    DayOfWeek_Saturday,
    DayOfWeek_Sunday,
};

Function DenseTime DenseTimeFromDateTime (DateTime date_time);
Function DateTime DateTimeFromDenseTime (DenseTime dense_time);

Function S8 S8FromDateTime (Arena *arena, DateTime date_time);
Function S8 S8FromDenseTime (Arena *arena, DenseTime dense_time);

Function DateTime DateTimeFromDDMMYYYYString (S8 string, I16 current_year);

Function B32 IsLeapYear (I16 year);
Function DateTime TommorowFromDateTime (DateTime today);
Function DateTime YesterdayFromDateTime (DateTime today);
Function DayOfWeek DayOfWeekFromDateTime (DateTime date);

//~NOTE(tbt): sorting

typedef I32 SortComparator(const void *a, const void *b, void *user_data);
Function void Sort (void *base, U64 n_items, U64 bytes_per_item, SortComparator *comparator, void *user_data);

Function I32 SortCompare1U64 (U64 a, U64 b);
Function I32 SortCompareLexicographic (S8 a, S8 b, MatchFlags flags);
Function I32 SortCompareChronological (DateTime a, DateTime b);

//~NOTE(tbt): random stuff

Function U32 NextPowerOfTwo1U (U32 a);

Function U64 EncodeU64FromI64 (I64 a);
Function I64 DecodeI64FromU64 (U64 a);

Function I32 PopulationCount1U (U32 a);

Function U64 RingWrite (U8 *buffer, U64 cap, U64 write_pos, const void *src, U64 bytes);
Function U64 RingRead (U8 *buffer, U64 cap, U64 read_pos, void *dest, U64 bytes);
