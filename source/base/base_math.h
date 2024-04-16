
//~NOTE(tbt): axis

typedef enum Axis2 Axis2;
enum Axis2
{
    Axis2_X,
    Axis2_Y,
    Axis2_MAX,
};

typedef enum Axis3 Axis3;
enum Axis3
{
    Axis3_X,
    Axis3_Y,
    Axis3_Z,
    Axis3_MAX,
};

typedef enum Axis4 Axis4;
enum Axis4
{
    Axis4_X,
    Axis4_Y,
    Axis4_Z,
    Axis4_W,
    Axis4_MAX,
};

//~NOTE(tbt): single floats

Function B32 IsNaN1F (F32 a);
Function F32 Smoothstep1F (F32 t);
Function F32 InterpolateLinear1F (F32 a, F32 b, F32 t);
Function F32 InterpolateExponential1F (F32 a, F32 b, F32 t);
Function F32 InterpolateSmooth1F (F32 a, F32 b, F32 t);
Function F32 Min1F (F32 a, F32 b);
Function F32 Max1F (F32 a, F32 b);
Function F32 Clamp1F (F32 a, F32 min, F32 max);
Function F32 Abs1F (F32 a);
Function F32 Pow1F (F32 a, F32 b);
Function F32 Round1F (F32 a);
Function F32 Floor1F (F32 a);
Function F32 Ceil1F (F32 a);
Function F32 Fract1F (F32 a);
Function F32 Sqrt1F (F32 a);
Function F32 Sin1F (F32 a);
Function F32 Cos1F (F32 a);
Function F32 Tan1F (F32 a);
Function F32 ACos1F (F32 a);
Function F32 Mod1F (F32 a, F32 b);

//~NOTE(tbt): floating point vectors

typedef union V2F V2F;
union V2F
{
    struct
    {
        F32 x;
        F32 y;
    };
    F32 elements[2];
};
#define V2F(...) ((V2F){ __VA_ARGS__ })

typedef union V3F V3F;
union V3F
{
    struct
    {
        F32 x;
        F32 y;
        F32 z;
    };
    struct
    {
        F32 r;
        F32 g;
        F32 b;
    };
    F32 elements[3];
};
#define V3F(...) ((V3F){ __VA_ARGS__ })

typedef union V4F V4F;
union V4F
{
    struct
    {
        F32 x;
        F32 y;
        F32 z;
        F32 w;
    };
    struct
    {
        union
        {
            struct
            {
                F32 r;
                F32 g;
                F32 b;
            };
            struct
            {
                F32 h;
                F32 s;
                F32 v;
            };
        };
        F32 a;
    };
    F32 elements[4];
};
#define V4F(...) ((V4F){ __VA_ARGS__ })

#define Col(R, G, B, A) ((V4F)ColInitialiser(R, G, B, A))
#define ColInitialiser(R, G, B, A) {(R)*(A), (G)*(A), (B)*(A), (A)}

#define ColFromHex(U) Col(((U >> 24) & 0xFF) / 255.0f, \
((U >> 16) & 0xFF) / 255.0f, \
((U >> 8) & 0xFF) / 255.0f, \
((U >> 0) & 0xFF) / 255.0f)
#define ColFromHexInitialiser(U) ColInitialiser(((U >> 24) & 0xFF) / 255.0f, \
((U >> 16) & 0xFF) / 255.0f, \
((U >> 8) & 0xFF) / 255.0f, \
((U >> 0) & 0xFF) / 255.0f)

Function V4F U4F (F32 a);
Function V4F Add4F (V4F a, V4F b);
Function V4F Sub4F (V4F a, V4F b);
Function V4F Mul4F (V4F a, V4F b);
Function V4F Div4F (V4F a, V4F b);
Function F32 Dot4F (V4F a, V4F b);
Function V4F Scale4F (V4F a, F32 b);
Function F32 LengthSquared4F (V4F a);
Function F32 Length4F (V4F a);
Function V4F Normalise4F (V4F a);
Function V4F Mins4F (V4F a, V4F b);
Function V4F Maxs4F (V4F a, V4F b);
Function V4F Clamp4F (V4F a, V4F min, V4F max);
Function V4F InterpolateLinear4F (V4F a, V4F b, V4F t);
Function V4F Round4F (V4F a);

Function V3F U3F (F32 a);
Function V3F Add3F (V3F a, V3F b);
Function V3F Sub3F (V3F a, V3F b);
Function V3F Mul3F (V3F a, V3F b);
Function V3F Div3F (V3F a, V3F b);
Function F32 Dot3F (V3F a, V3F b);
Function V3F Scale3F (V3F a, F32 b);
Function F32 LengthSquared3F (V3F a);
Function F32 Length3F (V3F a);
Function V3F Normalise3F (V3F a);
Function V3F Mins3F (V3F a, V3F b);
Function V3F Maxs3F (V3F a, V3F b);
Function V3F Clamp3F (V3F a, V3F min, V3F max);
Function V3F Cross3F (V3F a, V3F b);
Function V3F InterpolateLinear3F (V3F a, V3F b, V3F t);
Function V3F Round3F (V3F a);

Function V2F U2F (F32 a);
Function V2F Add2F (V2F a, V2F b);
Function V2F Sub2F (V2F a, V2F b);
Function V2F Mul2F (V2F a, V2F b);
Function V2F Div2F (V2F a, V2F b);
Function F32 Dot2F (V2F a, V2F b);
Function V2F Scale2F (V2F a, F32 b);
Function F32 LengthSquared2F (V2F a);
Function F32 Length2F (V2F a);
Function V2F Normalise2F (V2F a);
Function V2F Mins2F (V2F a, V2F b);
Function V2F Maxs2F (V2F a, V2F b);
Function V2F Clamp2F (V2F a, V2F min, V2F max);
Function V2F Tangent2F (V2F a);
Function V2F InterpolateLinear2F (V2F a, V2F b, V2F t);
Function V2F Round2F (V2F a);
Function V2F QuadraticBezier2F (V2F p0, V2F p1, V2F p2, F32 t);
Function V2F CubicBezier2F (V2F p0, V2F p1, V2F p2, V2F p3, F32 t);
Function V2F QuadraticBezierTangent2F(V2F p0, V2F p1, V2F p2, F32 t);
Function V2F CubicBezierTangent2F (V2F p0, V2F p1, V2F p2, V2F p3, F32 t);

//~NOTE(tbt): matrices

typedef union M4x4F M4x4F;
union M4x4F
{
    struct
    {
        V4F r_0;
        V4F r_1;
        V4F r_2;
        V4F r_3;
    };
    F32 elements[4][4];
};

Function M4x4F InitialiseDiagonal4x4F (F32 diag);
Function M4x4F Mul4x4F (M4x4F a, M4x4F b);
Function M4x4F PerspectiveMake4x4f (F32 fov, F32 aspect_ratio, F32 near, F32 far);
Function M4x4F OrthoMake4x4F (F32 left, F32 right, F32 top, F32 bottom, F32 near, F32 far);
Function M4x4F LookAtMake4x4F (V3F eye, V3F centre, V3F up);
Function M4x4F TranslationMake4x4F (V3F translation);
Function M4x4F ScaleMake4x4F (V3F scale);
Function V4F Transform4F (V4F a, M4x4F b);

//~NOTE(tbt): single integers

Function I32 InterpolateLinear1I (I32 a, I32 b, U8 t);
Function I32 Min1I (I32 a, I32 b);
Function I32 Max1I (I32 a, I32 b);
Function I32 Clamp1I (I32 a, I32 min, I32 max);
Function I32 Abs1I (I32 a);
Function I32 RotL1I (I32 a, I32 b);
Function I32 RotR1I (I32 a, I32 b);
Function I32 Normalise1I (I32 a);

//~NOTE(tbt): integer vectors

typedef union V2I V2I;
union V2I
{
    struct
    {
        I32 x;
        I32 y;
    };
    I32 elements[2];
};
#define V2I(...) ((V2I){ __VA_ARGS__ })

typedef union V3I V3I;
union V3I
{
    struct
    {
        I32 x;
        I32 y;
        I32 z;
    };
    I32 elements[3];
};
#define V3I(...) ((V3I){ __VA_ARGS__ })

typedef union V4I V4I;
union V4I
{
    struct
    {
        I32 x;
        I32 y;
        I32 z;
        I32 w;
    };
    I32 elements[4];
};
#define V4I(...) ((V4I){ __VA_ARGS__ })

Function V4I U4I (I32 a);
Function V4I Add4I (V4I a, V4I b);
Function V4I Sub4I (V4I a, V4I b);
Function V4I Mul4I (V4I a, V4I b);
Function V4I Div4I (V4I a, V4I b);
Function I32 Dot4I (V4I a, V4I b);
Function V4I Scale4I (V4I a, I32 b);
Function I32 LengthSquared4I (V4I a);
Function F32 Length4I (V4I a);

Function V3I U3I (I32 a);
Function V3I Add3I (V3I a, V3I b);
Function V3I Sub3I (V3I a, V3I b);
Function V3I Mul3I (V3I a, V3I b);
Function V3I Div3I (V3I a, V3I b);
Function I32 Dot3I (V3I a, V3I b);
Function V3I Scale3I (V3I a, I32 b);
Function V3I Cross3I (V3I a, V3I b);
Function I32 LengthSquared3I (V3I a);
Function F32 Length3I (V3I a);

Function V2I U2I (I32 a);
Function V2I Add2I (V2I a, V2I b);
Function V2I Sub2I (V2I a, V2I b);
Function V2I Mul2I (V2I a, V2I b);
Function V2I Div2I (V2I a, V2I b);
Function I32 Dot2I (V2I a, V2I b);
Function V2I Scale2I (V2I a, I32 b);
Function I32 LengthSquared2I (V2I a);
Function F32 Length2I (V2I a);

//~NOTE(tbt): single unsigned integers

Function U64 Min1U64 (U64 a, U64 b);
Function U64 Max1U64 (U64 a, U64 b);
Function U64 Clamp1U64 (U64 a, U64 min, U64 max);

//~NOTE(tbt): ranges

typedef union Range1U64 Range1U64;
union Range1U64
{
    struct
    {
        U64 min;
        U64 max;
    };
    struct
    {
        U64 cursor;
        U64 mark;
    };
    U64 elements[2];
};
#define Range1U64(...) ((Range1U64){ __VA_ARGS__ })

typedef union Range1F Range1F;
union Range1F
{
    struct
    {
        F32 min;
        F32 max;
    };
    F32 elements[2];
    V2F v;
};
#define Range1F(...) ((Range1F){ __VA_ARGS__ })

typedef union Range2F Range2F;
union Range2F
{
    struct
    {
        V2F min;
        V2F max;
    };
    struct
    {
        F32 x0;
        F32 y0;
        F32 x1;
        F32 y1;
    };
    V2F points[2];
    F32 elements[4];
    V4F v;
};
#define Range2F(...) ((Range2F){ __VA_ARGS__ })

Function B32 IsOverlapping1U64 (Range1U64 a, Range1U64 b);
Function Range1U64 Intersection1U64 (Range1U64 a, Range1U64 b);
Function Range1U64 Bounds1U64 (Range1U64 a, Range1U64 b);
Function Range1U64 Grow1U64 (Range1U64 a, U64 b);
Function B32 HasPoint1U64 (Range1U64 a, U64 b);
Function U64 Measure1U64 (Range1U64 a);

Function B32 IsOverlapping1F (Range1F a, Range1F b);
Function Range1F Intersection1F (Range1F a, Range1F b);
Function Range1F Bounds1F (Range1F a, Range1F b);
Function Range1F Grow1F (Range1F a, F32 b);
Function B32 HasPoint1F (Range1F a, F32 b);
Function F32 Centre1F (Range1F a);
Function F32 Measure1F (Range1F a);
Function F32 RangeMap1F (Range1F a, Range1F b, float point);

Function B32 IsOverlapping2F (Range2F a, Range2F b);
Function B32 IsWithin2F (Range2F a, Range2F b);
Function Range2F Intersection2F (Range2F a, Range2F b);
Function Range2F Bounds2F (Range2F a, Range2F b);
Function Range2F Grow2F (Range2F a, V2F b);
Function B32 HasPoint2F (Range2F a, V2F b);
Function V2F Centre2F (Range2F a);
Function V2F Measure2F (Range2F a);
Function V2F RangeMap2F (Range2F a, Range2F b, V2F point);

Function Range2F RectMake2F (V2F pos, V2F dimensions);

//~NOTE(tbt): colours

typedef union Pixel Pixel;
union Pixel
{
    U32 val;
    struct
    {
        U8 r;
        U8 g;
        U8 b;
        U8 a;
    };
};
#define Pixel(...) ((Pixel){ __VA_ARGS__ })

Function F32 LinearFromSRGB (F32 srgb);
Function F32 SRGBFromLinear (F32 linear);
Function Pixel InterpolateLinearPixel(Pixel a, Pixel b, unsigned char t);

Function V4F HSVFromRGB (V4F col);
Function V4F RGBFromHSV (V4F col);
Function V4F ColMix (V4F a, V4F b, F32 t);
