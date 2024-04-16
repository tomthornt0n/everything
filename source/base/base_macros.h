
//~NOTE(tbt): context cracking

//-NOTE(tbt): msvc
#if defined(_MSC_VER)
# define BuildCompilerMSVC 1

// NOTE(tbt): os detection
# if defined(_WIN32)
# define BuildOSWindows 1
# else
# error Could not detect OS
# endif

// NOTE(tbt): architecture detection
# if defined(_M_AMD64)
# define BuildArchX64 1
# elif defined(_M_I86)
# define BuildArchX86 1
# elif defined(_M_ARM)
# define BuildArchARM 1
// TODO(tbt): ARM64 ???
# else
# error Could not detect architecture
# endif

// NOTE(tbt): clang
#elif defined(__clang__)
# define BuildCompilerClang 1

// NOTE(tbt): os detection
# if defined(_WIN32)
# define BuildOSWindows 1
# elif defined(__gnu_linux__)
# define BuildOSLinux 1
# elif defined(__APPLE__) && defined(__MACH__)
# define BuildOSMac 1
# else
# error Could not detect OS
# endif

// NOTE(tbt): architecture detection
# if defined(__amd64__)
# define BuildArchX64 1
# elif defined(__i386__)
# define BuildArchX86 1
# elif defined(__arm__)
# define BuildArchARM 1
# elif defined(__aarch64__)
# define BuildArchARM64 1
# else
# error Could not detect architecture
# endif

//-NOTE(tbt): GCC
#elif defined(__GNUC__)
# define BuildCompilerGCC 1

// NOTE(tbt): os detection
# if defined(_WIN32)
# define BuildOSWindows 1
# elif defined(__gnu_linux__)
# define BuildOSLinux 1
# elif defined(__APPLE__) && defined(__MACH__)
# define BuildOSMac 1
# else
# error Could not detect OS
# endif

// NOTE(tbt): architecture detection
# if defined(__amd64__)
# define BuildArchX64 1
# elif defined(__i386__)
# define BuildArchX86 1
# elif defined(__arm__)
# define BuildArchARM 1
# elif defined(__aarch64__)
# define BuildArchARM64 1
# else
# error Could not detect architecture
# endif

#else
# error Could not detect compiler

#endif

//-NOTE(tbt): zero fill non set macros

// NOTE(tbt): compiler
#if !defined(BuildCompilerMSVC)
# define BuildCompilerMSVC 0
#endif
#if !defined(BuildCompilerClang)
# define BuildCompilerClang 0
#endif
#if !defined(BuildCompilerGCC)
# define BuildCompilerGCC 0
#endif

// NOTE(tbt): OS
#if !defined(BuildOSWindows)
# define BuildOSWindows 0
#endif
#if !defined(BuildOSLinux)
# define BuildOSLinux 0
#endif
#if !defined(BuildOSMac)
# define BuildOSMac 0
#endif

// NOTE(tbt): architecture
#if defined(BuildArchX64)
# define BuildUseSSE2 1
# include <emmintrin.h>
# include <immintrin.h>
#else
# define BuildArchX64 0
#endif
#if !defined(BuildArchX86)
# define BuildArchX86 0
#endif
#if !defined(BuildArchARM)
# define BuildArchARM 0
#endif
#if !defined(BuildArchARM64)
# define BuildArchARM64 0
#endif

#if !defined(BuildUseSSE2)
# define BuildUseSSE2 0
#endif

// NOTE(tbt): build options
#if !defined(BuildModeDebug)
# define BuildModeDebug 0
#endif
#if !defined(BuildModeRelease)
# define BuildModeRelease 0
#endif
#if !defined(BuildEnableAsserts)
# define BuildEnableAsserts 0
#endif
#if !defined(BuildZLib)
# define BuildZLib 0
#endif

//~NOTE(tbt): definitions

// NOTE(tbt): WARING - some of these are #undef'ed and then re-#define'd when
// including <windows.h> to avoid conflicts. Only changing
// them here might not do what you think

#if BuildOSMac
// NOTE(tbt): functions aren't static on Mac, need to link with objective C TU
# define Function
# define Global static
# define Persist static
#else
# define Function static
# define Global static
# define Persist static
#endif

//~NOTE(tbt): sized types

typedef int8_t I8;
typedef int16_t I16;
typedef int32_t I32;
typedef int64_t I64;

typedef uint8_t U8;
typedef uint16_t U16;
typedef uint32_t U32;
typedef uint64_t U64;

typedef float F32;
typedef double F64;

typedef int8_t B8;
typedef int32_t B32;

enum Boolean
{
    False = 0,
    True = 1,
};

//~NOTE(tbt): preprocessor helpers

#define Glue_(A, B) A ## B
#define Glue(A, B) Glue_(A, B)


#define Stringify_(A) #A
#define Stringify(A) Stringify_(A)
#define WideStringify(A) Glue(L, Stringify(A))

#define Statement(S) do { S } while(False)

//~NOTE(tbt): size helpers

#define Bytes(N) Glue_(N, LLU)
#define Kilobytes(N) (1000LLU * (U64)Bytes(N))
#define Megabytes(N) (1000LLU * (U64)Kilobytes(N))
#define Gigabytes(N) (1000LLU * (U64)Megabytes(N))
#define Terabytes(N) (1000LLU * (U64)Gigabytes(N))

//~NOTE(tbt): useful constants

#define Pi (3.1415926535897)

Global const union
{
    U32 u;
    F32 f;
} Infinity_ =
{
    .u = 0x7F800000,
};
#define Infinity (Infinity_.f)

Global const union
{
    U32 u;
    F32 f;
} NegativeInfinity_ =
{
    .u = 0xFF800000,
};
#define NegativeInfinity (NegativeInfinity_.f)

Global const union
{
    U8 u[sizeof(F32)];
	F32 f;
} NaN_ =
{
	.u = { 0, 0, 0xc0, 0x7f, },
};
#define NaN (NaN_.f)

//~NOTE(tbt): random helper macros

#define ArrayCount(A) (sizeof(A) / sizeof((A)[0]))

#define Bit(N) (((U64)1ULL) << ((U64)(N)))
#define SetMask(V, M, T) ((V) = ((T) ? ((V) | (M)) : ((V) & ~(M))))

#define ApplicationNameString Stringify(ApplicationName)
#define ApplicationNameWString WideStringify(ApplicationName)

#define DeferLoop(BEGIN, END) for(B32 _i = ((BEGIN), True); True == _i; ((END), _i = False))

#if BuildCompilerMSVC
# pragma section(".roglob", read)
# define ReadOnlyVar __declspec(allocate(".roglob"))
#else
// TODO(tbt): read only globals section on other compilers
# define ReadOnlyVar const
#endif

#if BuildCompilerMSVC
# define ThreadLocalVar __declspec(thread)
#else
#define ThreadLocalVar __thread
#endif

#define CopyAndIncrement(D, S, N, I) Statement( MemoryCopy((D), (S), (N)); (I) += (N); )

#define Initialiser(...) { __VA_ARGS__, }

#define Lit0(T) ((T){0})

//~NOTE(tbt): type generic math helpers

#define Min(A, B) ((A) < (B) ? (A) : (B))
#define Max(A, B) ((A) > (B) ? (A) : (B))
#define Clamp(X, A, B) (Max(A, Min(B, X)))
#define Abs(A) ((A) < 0 ? -(A) : (A))

//~NOTE(tbt): linked list helpers

// TODO(tbt): ^

//~NOTE(tbt): pointer arithmetic

#define IntFromPtr(P) ((U64)((unsigned char *)(P) - (unsigned char *)0))
#define PtrFromInt(N) ((void *)((unsigned char *)0 + (N)))

#define Member(T, M) (&((T *)0)->M)
#define OffsetOf(T, M) IntFromPtr(Member(T, M))
#define MemberAtOffset(B, O, T) ((T *)(PtrFromInt(IntFromPtr(B) + O)))

//~NOTE(tbt): entry point

#if BuildOSWindows
# define EntryPoint int WINAPI wWinMain(HINSTANCE instance_handle, HINSTANCE prev_instance_handle, PWSTR command_line, int show_mode)
#else
# define EntryPoint int main(int argc, char **argv)
#endif

//~NOTE(tbt): MSVC warnings

#if BuildCompilerMSVC
# pragma warning(disable: 4244)
#endif
