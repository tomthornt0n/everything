
#undef STB_SPRINTF_IMPLEMENTATION
#include "external/stb_sprintf.h"

#if BuildZLib
# include "external/zlib/zlib.h"
#endif

//~NOTE(tbt): string types

//-NOTE(tbt): utf8 ecnoded strings
typedef struct S8 S8;
struct S8
{
    const char *buffer;
    U64 len;
};
#define S8(S) ((S8){ S, sizeof(S) - 1 })
#define S8Initialiser(S) { .buffer = S, .len = sizeof(S) - 1, }

//-NOTE(tbt): utf16 ecnoded strings
typedef struct S16 S16;
struct S16
{
    const wchar_t *buffer;
    U64 len;
};
#define S16(S) ((S16){ WidenString(S), (sizeof(WidenString(S)) - 2) / 2 })

//-NOTE(tbt): utf32 ecnoded strings
typedef struct S32 S32;
struct S32
{
    const U32 *buffer;
    U64 len;
};

//~NOTE(tbt): fmt string helpers

#define FmtS8(S) (int)(S).len, (S).buffer // NOTE(tbt): S8 --> "%.*s"

#define FmtB32(B) ((!!(B)) ? "True" : "False") // NOTE(tbt): B32 --> "%s"

#define FmtV4F(V) (V).x, (V).y, (V).z, (V).w // NOTE(tbt): V4F --> "%f%f%f%f"
#define FmtV3F(V) (V).x, (V).y, (V).z // NOTE(tbt): V3F --> "%f%f%f"
#define FmtV2F(V) (V).x, (V).y // NOTE(tbt): V2F --> "%f%f"

#define FmtV4I(V) (V).x, (V).y, (V).z, (V).w // NOTE(tbt): V4I --> "%d%d%d%d"
#define FmtV3I(V) (V).x, (V).y, (V).z // NOTE(tbt): V3I --> "%d%d%d"
#define FmtV2I(V) (V).x, (V).y // NOTE(tbt): V2I --> "%d%d"

//~NOTE(tbt): character utilites

Function B32 CharIsSymbol (char c);
Function B32 CharIsSpace (char c);
Function B32 CharIsNumber (char c);
Function B32 CharIsUppercase (char c);
Function B32 CharIsLowercase (char c);
Function B32 CharIsLetter (char c);
Function B32 CharIsAlphanumeric (char c);
Function B32 CharIsPrintable (char c);

Function char CharLowerFromUpper (char c);
Function char CharUpperFromLower (char c);

//~NOTE(tbt): c-string helpers

Function U64 CStringCalculateUTF8Length (const char *cstring);
Function S8 CStringAsS8 (const char *cstring);
Function U64 CStringCalculateUTF16Length (const wchar_t *cstring);
Function S16 CStringAsS16 (const wchar_t *cstring);

//~NOTE(tbt): unicode conversions

typedef struct UTFConsume UTFConsume;
struct UTFConsume
{
    U32 codepoint;
    I32 advance;
};

Function B32 UTF8IsContinuationByte (S8 string, U64 index);

// NOTE(tbt): from utf8
Function UTFConsume CodepointFromUTF8 (S8 string, U64 index);
Function S16 S16FromS8 (Arena *arena, S8 string);
Function S32 S32FromS8 (Arena *arena, S8 string);

// NOTE(tbt): from utf16
Function UTFConsume CodepointFromUTF16 (S16 string, U64 index);
Function S8 S8FromS16 (Arena *arena, S16 string);
Function S32 S32FromS16 (Arena *arena, S16 string);

// NOTE(tbt): from utf32
Function S8 UTF8FromCodepoint (Arena *arena, U32 codepoint);
Function S16 UTF16FromCodepoint (Arena *arena, U32 codepoint);
Function S8 S8FromS32 (Arena *arena, S32 string);
Function S16 S16FromS32 (Arena *arena, S32 string);

//~NOTE(tbt): string operations

typedef enum MatchFlags MatchFlags;
enum MatchFlags
{
    MatchFlags_Exact = 0,
    MatchFlags_RightSideSloppy = Bit(0), // NOTE(tbt): if the lengths are not equal, compare up to the length of the shortest string
    MatchFlags_CaseInsensitive = Bit(1),
};

typedef struct S8Split S8Split;
struct S8Split
{
    S8 delimiter;
    MatchFlags flags;
    
    S8 string;
    S8 current;
    U64 index;
    B32 is_done;
};

Function S8 S8Clone (Arena *arena, S8 string);
Function S8 S8Truncate (S8 string, U64 len);
Function S8 S8LFFromCRLF (Arena *arena, S8 string);
Function S8 S8FromFmtV (Arena *arena, const char *fmt, va_list args);
Function S8 S8FromFmt (Arena *arena, const char *fmt, ...);
Function S8 S8Replace (Arena *arena, S8 string, S8 a, S8 b, MatchFlags flags);
Function S8 S8PrefixGet (S8 string, U64 len);
Function S8 S8SuffixGet (S8 string, U64 len);
Function S8 S8Advance (S8 *string, U64 n);
#define TFromS8Advance(S, T) ((S)->len >= sizeof(T) ? *((T *)S8Advance((S), sizeof(T)).buffer) : (T){0}) // NOTE(tbt): beware of multiple evaluation of S and T
Function B32 S8Consume (S8 *string, S8 consume, MatchFlags flags);
Function U64 S8ByteIndexFromCharIndex (S8 string, U64 char_index);
Function U64 S8CharIndexFromByteIndex (S8 string, U64 byte_index);
Function U64 S8Hash (S8 string);
Function U64 S8Match (S8 a, S8 b, MatchFlags flags);
Function S8 S8Substring (S8 h, S8 n, MatchFlags flags);
Function B32 S8HasSubstring (S8 h, S8 n, MatchFlags flags);
Function F64 S8Parse1F64 (S8 string);
Function U64 S8Parse1U64 (S8 string);
Function S8Split S8SplitMake (S8 string, S8 delimiter, MatchFlags flags);
Function B32 S8SplitNext (S8Split *state);
Function I32 S8LevenshteinDistance (Arena *scratch_arena, S8 a, S8 b, MatchFlags flags);

Function S16 S16Clone (Arena *arena, S16 string);
Function U64 S16Hash (S16 string);
Function B32 S16Match (S16 a, S16 b, MatchFlags flags);


//~NOTE(tbt): filename helpers

#if BuildOSWindows
# define DirectorySeparatorStr "\\"
# define DirectorySeparatorChr '\\'
#else
# define DirectorySeparatorStr "/"
# define DirectorySeparatorChr '/'
#endif
Function B32 FilenameHasTrailingSlash (S8 filename);
Function void FilenameSplitExtension (S8 filename, S8 *without_extension, S8 *extension);
Function S8 ExtensionFromFilename (S8 filename);
Function S8 FilenameWithoutExtension (S8 filename);
Function S8 FilenamePush (Arena *arena, S8 filename, S8 string);
Function S8 FilenamePop (S8 filename);
Function S8 FilenameLast (S8 filename);
Function B32 FilenameIsChild (S8 parent, S8 filename);

//~NOTE(tbt): misc.

Function B32 S8IsWordBoundary (S8 string, U64 index);

//~NOTE(tbt): string lists

typedef struct S8Node S8Node;
struct S8Node
{
    S8 string;
    S8Node *next;
};

typedef struct S8List S8List;
struct S8List
{
    S8Node *first;
    S8Node *last;
    U64 count;
};

typedef struct S8ListFind S8ListFind;
struct S8ListFind
{
    B32 has;
    U64 i;
};
Function S8ListFind S8ListIndexFromS8 (S8List list, S8 string, MatchFlags flags);

typedef struct S8ListJoinFormat S8ListJoinFormat;
struct S8ListJoinFormat
{
    S8 before; // NOTE(tbt): added to the start of the list
    S8 prefix; // NOTE(tbt): concatenated to the start of each node
    S8 delimiter; // NOTE(tbt): added between every node
    S8 suffix; // NOTE(tbt): concatenated to the end of each node
    S8 after; // NOTE(tbt): added to the end of the list
};
#define S8ListJoinFormat(...) ((S8ListJoinFormat){ __VA_ARGS__ })

// NOTE(tbt): clones the new string(s) to the arena and allocates a new node in the arena
Function S8Node *S8ListPush (Arena *arena, S8List *list, S8 string);
Function S8Node *S8ListPushFmtV (Arena *arena, S8List *list, const char *fmt, va_list args);
Function S8Node *S8ListPushFmt (Arena *arena, S8List *list, const char *fmt, ...);
Function S8Node *S8ListAppend (Arena *arena, S8List *list, S8 string);
Function S8Node *S8ListAppendFmtV (Arena *arena, S8List *list, const char *fmt, va_list args);
Function S8Node *S8ListAppendFmt (Arena *arena, S8List *list, const char *fmt, ...);
Function void S8ListConcatenate (S8List *a, S8List b);

Function void S8ListClear (S8List *list);
Function void S8ListPushExplicit (S8List *list, S8Node *string);
Function void S8ListAppendExplicit (S8List *list, S8Node *string);
Function void S8ListRemoveExplicit (S8List *list, S8Node *string);
Function void S8ListReresh (S8List *list);

Function S8List S8ListClone (Arena *arena, S8List list);
Function S8 S8ListS8FromIndex (S8List list, U64 index);
Function B32 S8ListHas (S8List list, S8 string, MatchFlags flags);
Function S8 S8ListJoin (Arena *arena, S8List list);
Function S8 S8ListJoinFormatted (Arena *arena, S8List list, S8ListJoinFormat join);
Function S8 S8ListPop (S8List *list);
Function S8Node *S8ListRemoveFirstOccurenceOf (S8List *list, S8 string, MatchFlags flags);
Function void S8ListRemoveAllOccurencesOf (S8List *list, S8 string, MatchFlags flags);
Function S8List S8ListFromSplit (Arena *arena, S8 string, S8 delimiter, MatchFlags flags);

#define S8ListForEach(L, V) S8Node *(V) = (L).first; 0 != (V); (V) = (V)->next


//~NOTE(tbt): compression

#if BuildZLib
Function S8 S8Deflate (Arena *arena, S8 string);
Function S8 S8Inflate (Arena *arena, S8 string);
#endif

//~NOTE(tbt): string builders

typedef struct S8Builder S8Builder;
struct S8Builder
{
    char *buffer;
    U64 len;
    U64 cap;
};

#define S8BuilderFromArray(A) ((S8Builder){ .buffer = (A), .cap = sizeof(A), .len = 0, })
#define S8BuilderFromArrayInitialiser(A) { .buffer = (A), .cap = sizeof(A), .len = 0, }

Function S8Builder PushS8Builder (Arena *arena, U64 cap);
Function S8 S8FromBuilder (S8Builder builder);
Function S8 S8FromBuilderAndRange (S8Builder builder, Range1U64 range);
Function B32 S8BuilderReplaceRange (S8Builder *builder, Range1U64 range, S8 string);
Function void S8BuilderAppend (S8Builder *builder, S8 string);
Function void S8BuilderAppendFmtV (S8Builder *builder, const char *fmt, va_list args);
Function void S8BuilderAppendFmt (S8Builder *builder, const char *fmt, ...);
Function void S8BuilderAppendBytes (S8Builder *builder, void *data, U64 len);
#define S8BuilderAppendBytesFromLValue(B, D) S8BuilderAppendBytes(B, &(D), sizeof(D))
