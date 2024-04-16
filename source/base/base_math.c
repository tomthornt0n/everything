
// NOTE(tbt): use libm for powf and fmod
// TODO(tbt): get off libm
#include <math.h>

//~NOTE(tbt): single floats

Function B32
IsNaN1F(F32 a)
{
    union { F32 f; U32 i; } i_from_f = { .f = a, };
    B32 result = (i_from_f.i & 0x7fffffff) > 0x7f800000;
    return result;
}

Function F32
Smoothstep1F(F32 t)
{
    return t*t*(3 - 2*t);
}

Function F32
InterpolateLinear1F(F32 a, F32 b,
                    F32 t) // NOTE(tbt): (0.0 <= t <= 1.0)
{
    return a + t*(b - a);
}

Function F32
InterpolateExponential1F(F32 a, F32 b,
                         F32 t) // NOTE(tbt): (0.0 <= t <= 1.0)
{
    return InterpolateLinear1F(a, b, t*t);
}

Function F32
InterpolateSmooth1F(F32 a, F32 b,
                    F32 t) // NOTE(tbt): (0.0 <= t <= 1.0)
{
    return InterpolateLinear1F(a, b, Smoothstep1F(t));
}

Function F32
Min1F(F32 a, F32 b)
{
    if(a < b)
    {
        return a;
    }
    else
    {
        return b;
    }
}

Function F32
Max1F(F32 a, F32 b)
{
    if(a > b)
    {
        return a;
    }
    else
    {
        return b;
    }
}

Function F32
Clamp1F(F32 a,
        F32 min,
        F32 max)
{
    return (Max1F(min, Min1F(max, a)));
}

Function F32
Abs1F(F32 a)
{
    union { F32 f; U32 u; } u_from_f;
    u_from_f.f = a;
    u_from_f.u &= ~(1 << 31);
    return u_from_f.f;
}

Function F32
Pow1F(F32 a,
      F32 b)
{
    F32 result = powf(a, b);
    return result;
}

Function F32
Round1F(F32 a)
{
    return Floor1F(a + 0.5f);
}

Function F32
Floor1F(F32 a)
{
    union
    {
        F32 f;
        U32 u;
    } u32_from_f32 =
    {
        .f = a,
    };
    
    I32 exponent = ((u32_from_f32.u >> 23) & 255) - 127;
    I32 fractional_bits = 23 - exponent;
    
    F32 result = 0.0f;
    
    if (exponent < 0)
    {
        result = (a >= 0.0f) ? 0.0f : -1.0f;
    }
    else if(fractional_bits <= 0)
    {
        result = a;
    }
    else
    {
        unsigned integral_mask = 0xffffffff << fractional_bits;
        
        union
        {
            F32 f;
            U32 u;
        } f32_from_u32 =
        {
            .u = (u32_from_f32.u & integral_mask),
        };
        
        if(f32_from_u32.f < 0.0f && f32_from_u32.u != u32_from_f32.u)
        {
            result = f32_from_u32.f - 1.0f;
        }
        else
        {
            result = f32_from_u32.f;
        }
    }
    
    return result;
}

Function F32
Ceil1F(F32 a)
{
    union
    {
        F32 f;
        U32 u;
    } u32_from_f32 =
    {
        .f = a,
    };
    
    I32 exponent = ((u32_from_f32.u >> 23) & 255) - 127;
    I32 fractional_bits = 23 - exponent;
    
    F32 result = 0.0f;
    
    if (exponent < 0)
    {
        result = (a > 0.0f) ? 1.0f : 0.0f;
    }
    else if(fractional_bits <= 0)
    {
        result = a;
    }
    else
    {
        unsigned integral_mask = 0xffffffff << fractional_bits;
        
        union
        {
            F32 f;
            U32 u;
        } f32_from_u32 =
        {
            .u = (u32_from_f32.u & integral_mask),
        };
        
        if(f32_from_u32.f > 0.0f && f32_from_u32.u != u32_from_f32.u)
        {
            result = f32_from_u32.f + 1.0f;
        }
        else
        {
            result = f32_from_u32.f;
        }
    }
    
    return result;
}

Function F32
Fract1F(F32 a)
{
    I32 i = (I32)a;
    F32 result = a - (F32)i;
    return result;
}

Function F32
Sqrt1F(F32 a)
{
    F32 result = sqrtf(a);
    return result;
}

// NOTE(tbt): not very fast or accurate
Function F32
Sin1F(F32 a)
{
    // NOTE(tbt): range reduction
    I32 k = (I32)(a*(1.0f / (2.0f*Pi)));
    a = a - k*(2*Pi);
    a = Min1F(a, Pi - a);
    a = Max1F(a, -Pi - a);
    a = Min1F(a, Pi - a);
    
    F32 result = 0.0f;
    
    F32  a1 = a;
    F32  a2 = a1*a1;
    F32  a4 = a2*a2;
    F32  a5 = a1*a4;
    F32  a9 = a4*a5;
    F32 a13 = a9*a4;
    
    F32 term_1 =  a1*(1.0f - a2 /  6.0f);
    F32 term_2 =  a5*(1.0f - a2 /  42.0f) / 120.0f;
    F32 term_3 =  a9*(1.0f - a2 / 110.0f) / 362880.0f;
    F32 term_4 = a13*(1.0f - a2 / 225.0f) / 6227020800.0f;
    
    result += term_4;
    result += term_3;
    result += term_2;
    result += term_1;
    
    return result;
}

Function F32
Cos1F(F32 a)
{
    return Sin1F(a + 0.5f*Pi);
}

Function F32
Tan1F(F32 a)
{
    return Sin1F(a) / Cos1F(a);
}

Function F32
ACos1F(F32 a)
{
    F32 x = -0.939115566365855f;
    F32 y =  0.9217841528914573f;
    F32 z = -1.2845906244690837f;
    F32 w =  0.295624144969963174f;
    F32 result = Pi / 2.0f + (x*a + y*a*a*a) / (1.0f + z*a*a + w*a*a*a*a);
    return result;
}

Function F32
Mod1F(F32 a, F32 b)
{
    F64 _a = a;
    F64 _b = b;
    F64 result = _a - (I64)(_a / _b)*_b;
    
    return (F32)result;
}

//~NOTE(tbt): F32 vectors

Function V4F
U4F(F32 a)
{
    V4F result =
    {
        .x = a,
        .y = a,
        .z = a,
        .w = a,
    };
    return result;
}

Function V4F
Add4F(V4F a, V4F b)
{
    V4F result =
    {
        .x = a.x + b.x,
        .y = a.y + b.y,
        .z = a.z + b.z,
        .w = a.w + b.w,
    };
    return result;
}

Function V4F
Sub4F(V4F a, V4F b)
{
    V4F result =
    {
        .x = a.x - b.x,
        .y = a.y - b.y,
        .z = a.z - b.z,
        .w = a.w - b.w,
    };
    return result;
}

Function V4F
Mul4F(V4F a, V4F b)
{
    V4F result =
    {
        .x = a.x*b.x,
        .y = a.y*b.y,
        .z = a.z*b.z,
        .w = a.w*b.w,
    };
    return result;
}

Function V4F
Div4F(V4F a, V4F b)
{
    V4F result =
    {
        .x = a.x / b.x,
        .y = a.y / b.y,
        .z = a.z / b.z,
        .w = a.w / b.w,
    };
    return result;
}

Function F32
Dot4F(V4F a, V4F b)
{
    F32 result = (a.x*b.x) + (a.y*b.y) + (a.z*b.z) + (a.w*b.w);
    return result;
}

Function V4F
Scale4F(V4F a, F32 b)
{
    V4F _b = { b, b, b, b };
    return Mul4F(a, _b);
}

Function F32
LengthSquared4F(V4F a)
{
    return a.x*a.x + a.y*a.y + a.z*a.z + a.w*a.w;
}

Function F32
Length4F(V4F a)
{
    return Sqrt1F(LengthSquared4F(a));
}

Function V4F
Normalise4F(V4F a)
{
    F32 one_over_length = 1.0f / Sqrt1F(LengthSquared4F(a));
    return Scale4F(a, one_over_length);
}

Function V4F
Mins4F(V4F a, V4F b)
{
    V4F result =
    {
        .x = Min1F(a.x, b.x),
        .y = Min1F(a.y, b.y),
        .z = Min1F(a.z, b.z),
        .w = Min1F(a.w, b.w),
    };
    return result;
}

Function V4F
Maxs4F(V4F a, V4F b)
{
    V4F result =
    {
        .x = Max1F(a.x, b.x),
        .y = Max1F(a.y, b.y),
        .z = Max1F(a.z, b.z),
        .w = Max1F(a.w, b.w),
    };
    return result;
}

Function V4F
Clamp4F(V4F a, V4F min, V4F max)
{
    V4F result =
    {
        .x = Clamp1F(a.x, min.x, max.x),
        .y = Clamp1F(a.y, min.y, max.y),
        .z = Clamp1F(a.z, min.z, max.z),
        .w = Clamp1F(a.w, min.w, max.w),
    };
    return result;
}

Function V4F
InterpolateLinear4F(V4F a, V4F b, V4F t)
{
    V4F result =
    {
        .x = InterpolateLinear1F(a.x, b.x, t.x),
        .y = InterpolateLinear1F(a.y, b.y, t.y),
        .z = InterpolateLinear1F(a.z, b.z, t.z),
        .w = InterpolateLinear1F(a.w, b.w, t.w),
    };
    return result;
}

Function V4F
Round4F(V4F a)
{
    V4F result =
    {
        .x = Round1F(a.x),
        .y = Round1F(a.y),
        .z = Round1F(a.z),
        .w = Round1F(a.w),
    };
    return result;
}

//-

Function V3F
U3F(F32 a)
{
    V3F result =
    {
        .x = a,
        .y = a,
        .z = a,
    };
    return result;
}

Function V3F
Add3F(V3F a, V3F b)
{
    V3F result =
    {
        .x = a.x + b.x,
        .y = a.y + b.y,
        .z = a.z + b.z,
    };
    return result;
}

Function V3F
Sub3F(V3F a, V3F b)
{
    V3F result =
    {
        .x = a.x - b.x,
        .y = a.y - b.y,
        .z = a.z - b.z,
    };
    return result;
}

Function V3F
Mul3F(V3F a, V3F b)
{
    V3F result =
    {
        .x = a.x*b.x,
        .y = a.y*b.y,
        .z = a.z*b.z,
    };
    return result;
}

Function V3F
Div3F(V3F a, V3F b)
{
    V3F result =
    {
        .x = a.x / b.x,
        .y = a.y / b.y,
        .z = a.z / b.z,
    };
    return result;
}

Function F32
Dot3F(V3F a, V3F b)
{
    F32 result = a.x*b.x + a.y*b.y + a.z*b.z;
    return result;
}

Function V3F
Cross3F(V3F a, V3F b)
{
    V3F result =
    {
        .x = a.y*b.z - a.z*b.y,
        .y = a.z*b.x - a.x*b.z,
        .z = a.x*b.y - a.y*b.x,
    };
    return result;
}

Function V3F
Scale3F(V3F a, F32 b)
{
    V3F _b = { b, b, b };
    return Mul3F(a, _b);
}

Function F32
LengthSquared3F(V3F a)
{
    return a.x*a.x + a.y*a.y + a.z*a.z;
}

Function F32
Length3F(V3F a)
{
    return Sqrt1F(LengthSquared3F(a));
}

Function V3F
Normalise3F(V3F a)
{
    F32 one_over_length = 1.0f / Sqrt1F(LengthSquared3F(a));
    return Scale3F(a, one_over_length);
}

Function V3F
Mins3F(V3F a, V3F b)
{
    V3F result =
    {
        .x = Min1F(a.x, b.x),
        .y = Min1F(a.y, b.y),
        .z = Min1F(a.z, b.z),
    };
    return result;
}

Function V3F
Maxs3F(V3F a, V3F b)
{
    V3F result =
    {
        .x = Max1F(a.x, b.x),
        .y = Max1F(a.y, b.y),
        .z = Max1F(a.z, b.z),
    };
    return result;
}

Function V3F
Clamp3F(V3F a, V3F min, V3F max)
{
    V3F result =
    {
        .x = Clamp1F(a.x, min.x, max.x),
        .y = Clamp1F(a.y, min.y, max.y),
        .z = Clamp1F(a.z, min.z, max.z),
    };
    return result;
}

Function V3F
InterpolateLinear3F(V3F a, V3F b, V3F t)
{
    V3F result =
    {
        .x = InterpolateLinear1F(a.x, b.x, t.x),
        .y = InterpolateLinear1F(a.y, b.y, t.y),
        .z = InterpolateLinear1F(a.z, b.z, t.z),
    };
    return result;
}

Function V3F
Round3F(V3F a)
{
    V3F result =
    {
        .x = Round1F(a.x),
        .y = Round1F(a.y),
        .z = Round1F(a.z),
    };
    return result;
}

//-

Function V2F
U2F(F32 a)
{
    V2F result =
    {
        .x = a,
        .y = a,
    };
    return result;
}

Function V2F
Add2F(V2F a, V2F b)
{
    V2F result =
    {
        .x = a.x + b.x,
        .y = a.y + b.y,
    };
    return result;
}

Function V2F
Sub2F(V2F a, V2F b)
{
    V2F result =
    {
        .x = a.x - b.x,
        .y = a.y - b.y,
    };
    return result;
}

Function V2F
Mul2F(V2F a, V2F b)
{
    V2F result =
    {
        .x = a.x*b.x,
        .y = a.y*b.y,
    };
    return result;
}

Function V2F
Div2F(V2F a, V2F b)
{
    V2F result =
    {
        .x = a.x / b.x,
        .y = a.y / b.y,
    };
    return result;
}

Function F32
Dot2F(V2F a, V2F b)
{
    F32 result = a.x*b.x + a.y*b.y;
    return result;
}

Function V2F
Scale2F(V2F a, F32 b)
{
    V2F _b = { b, b };
    return Mul2F(a, _b);
}

Function F32
LengthSquared2F(V2F a)
{
    return a.x*a.x + a.y*a.y;
}

Function F32
Length2F(V2F a)
{
    return Sqrt1F(LengthSquared2F(a));
}

Function V2F
Normalise2F(V2F a)
{
    F32 one_over_length = 1.0f / Sqrt1F(LengthSquared2F(a));
    return Scale2F(a, one_over_length);
}


Function V2F
Mins2F(V2F a, V2F b)
{
    V2F result =
    {
        .x = Min1F(a.x, b.x),
        .y = Min1F(a.y, b.y),
    };
    return result;
}

Function V2F
Maxs2F(V2F a, V2F b)
{
    V2F result =
    {
        .x = Max1F(a.x, b.x),
        .y = Max1F(a.y, b.y),
    };
    return result;
}

Function V2F
Clamp2F(V2F a, V2F min, V2F max)
{
    V2F result =
    {
        .x = Clamp1F(a.x, min.x, max.x),
        .y = Clamp1F(a.y, min.y, max.y),
    };
    return result;
}

Function V2F
InterpolateLinear2F(V2F a, V2F b, V2F t)
{
    V2F result =
    {
        .x = InterpolateLinear1F(a.x, b.x, t.x),
        .y = InterpolateLinear1F(a.y, b.y, t.y),
    };
    return result;
}

Function V2F
Round2F(V2F a)
{
    V2F result =
    {
        .x = Round1F(a.x),
        .y = Round1F(a.y),
    };
    return result;
}

Function V2F
Tangent2F(V2F a)
{
    V2F result =
    {
        .x = +a.y,
        .y = -a.x,
    };
    return result;
}

Function V2F
QuadraticBezier2F(V2F p0, V2F p1, V2F p2, F32 t)
{
    F32 nt = 1.0f - t;
    F32 x = p1.x + nt*nt*(p0.x - p1.x) + t*t*(p2.x - p1.x);
    F32 y = p1.y + nt*nt*(p0.y - p1.y) + t*t*(p2.y - p1.y);
    V2F result = V2F(x, y);
    return result;
}

Function V2F
CubicBezier2F(V2F p0, V2F p1, V2F p2, V2F p3, F32 t)
{
    F32 nt = 1.0f - t;
    F32 x = nt*nt*nt*p0.x + 3.0f*t*nt*nt*p1.x + 3.0f*t*t*nt*p2.x + t*t*t*p3.x;
    F32 y = nt*nt*nt*p0.y + 3.0f*t*nt*nt*p1.y + 3.0f*t*t*nt*p2.y + t*t*t*p3.y;
    V2F result = V2F(x, y);
    return result;
}

Function V2F
QuadraticBezierTangent2F(V2F p0, V2F p1, V2F p2, F32 t)
{
    F32 nt = 1.0f - t;
    F32 x = 2.0f*nt*(p1.x - p0.x) + 2.0f*t*(p2.x - p1.x);
    F32 y = 2.0f*nt*(p1.y - p0.y) + 2.0f*t*(p2.y - p1.y);
    V2F result = V2F(x, y);
    return result;
}

Function V2F
CubicBezierTangent2F(V2F p0, V2F p1, V2F p2, V2F p3, F32 t)
{
    F32 nt = 1.0f - t;
    F32 x = -3.0f*nt*nt*p0.x + 3.0f*(1.0f - 4.0f*t + 3.0f*t*t)*p1.x + 3.0f*(2.0f*t - 3.0f*t*t)*p2.x + 3.0f*t*t*p3.x;
    F32 y = -3.0f*nt*nt*p0.y + 3.0f*(1.0f - 4.0f*t + 3.0f*t*t)*p1.y + 3.0f*(2.0f*t - 3.0f*t*t)*p2.y + 3.0f*t*t*p3.y;
    V2F result = V2F(x, y);
    return result;
}

//~NOTE(tbt): matrices

Function M4x4F
InitialiseDiagonal4x4F(F32 diag)
{
    M4x4F result = 
    {
        {
            { diag, 0.0f, 0.0f, 0.0f },
            { 0.0f, diag, 0.0f, 0.0f },
            { 0.0f, 0.0f, diag, 0.0f },
            { 0.0f, 0.0f, 0.0f, diag },
        },
    };
    return result;
}

#if BuildUseSSE2
typedef struct SSE_M4x4F SSE_M4x4F;
struct SSE_M4x4F
{
    __m128 rows[4];
};

Function SSE_M4x4F
SSEM4x4FFromM4x4F_(M4x4F a)
{
    SSE_M4x4F result;
    result.rows[0] = _mm_load_ps(&a.elements[0][0]);
    result.rows[1] = _mm_load_ps(&a.elements[1][0]);
    result.rows[2] = _mm_load_ps(&a.elements[2][0]);
    result.rows[3] = _mm_load_ps(&a.elements[3][0]);
    return result;
}

Function M4x4F
M4x4FFromSSEM4x4F_(SSE_M4x4F a)
{
    M4x4F result;
    _mm_store_ps(&result.elements[0][0], a.rows[0]);
    _mm_store_ps(&result.elements[1][0], a.rows[1]);
    _mm_store_ps(&result.elements[2][0], a.rows[2]);
    _mm_store_ps(&result.elements[3][0], a.rows[3]);
    return result;
}

Function __m128
LinearCombine4x4F_(const __m128 *a,
                   const SSE_M4x4F *b)
{
    __m128 result;
    result = _mm_mul_ps(_mm_shuffle_ps(*a, *a, 0x00), b->rows[0]);
    result = _mm_add_ps(result, _mm_mul_ps(_mm_shuffle_ps(*a, *a, 0x55), b->rows[1]));
    result = _mm_add_ps(result, _mm_mul_ps(_mm_shuffle_ps(*a, *a, 0x55), b->rows[2]));
    result = _mm_add_ps(result, _mm_mul_ps(_mm_shuffle_ps(*a, *a, 0x55), b->rows[3]));
    return result;
}

#endif

Function M4x4F
Mul4x4F(M4x4F a, M4x4F b)
{
    M4x4F result = {0};
#if BuildUseSSE2
    SSE_M4x4F _result;
    SSE_M4x4F _a = SSEM4x4FFromM4x4F_(a);
    SSE_M4x4F _b = SSEM4x4FFromM4x4F_(b);
    _result.rows[0] = LinearCombine4x4F_(&_a.rows[0], &_b);
    _result.rows[1] = LinearCombine4x4F_(&_a.rows[1], &_b);
    _result.rows[2] = LinearCombine4x4F_(&_a.rows[2], &_b);
    _result.rows[3] = LinearCombine4x4F_(&_a.rows[3], &_b);
    result = M4x4FFromSSEM4x4F_(_result);
#else
    for(I32 j = 0; j < 4; ++j)
    {
        for(I32 i = 0; i < 4; ++i)
        {
            result.elements[i][j] = (a.elements[0][j]*b.elements[i][0] +
                                     a.elements[1][j]*b.elements[i][1] +
                                     a.elements[2][j]*b.elements[i][2] +
                                     a.elements[3][j]*b.elements[i][3]);
        }
    }
#endif
    return result;
}

Function M4x4F
PerspectiveMake4x4f(F32 fov,
                    F32 aspect_ratio,
                    F32 near_plane, F32 far_plane)
{
    M4x4F result = {0};
    F32 tan_half_theta = Tan1F(fov*(Pi/360.0f));
    result.elements[0][0] = 1.0f / tan_half_theta;
    result.elements[1][1] = aspect_ratio / tan_half_theta;
    result.elements[3][2] = -1.0f;
    result.elements[2][2] = (near_plane + far_plane) / (near_plane - far_plane);
    result.elements[2][3] = (2.0f*near_plane*far_plane) / (near_plane - far_plane);
    result.elements[3][3] = 0.0f;
    return result;
}

Function M4x4F
OrthoMake4x4F(F32 left, F32 right,
              F32 top, F32 bottom,
              F32 near_plane, F32 far_plane)
{
    M4x4F result = {0};
    result.elements[0][0] = +2.0f / (right - left);
    result.elements[1][1] = +2.0f / (top - bottom);
    result.elements[2][2] = -2.0f / (far_plane - near_plane);
    result.elements[3][3] = 1.0f;
    result.elements[3][0] = -((right + left) / (right - left));
    result.elements[3][1] = -((top + bottom) / (top - bottom));
    result.elements[3][2] = -((far_plane + near_plane) / (far_plane - near_plane));
    return result;
}

Function M4x4F
LookAtMake4x4F(V3F eye, V3F centre, V3F up)
{
    M4x4F result;
    
    V3F f = Normalise3F(Sub3F(centre, eye));
    V3F s = Normalise3F(Cross3F(f, up));
    V3F u = Cross3F(s, f);
    
    result.elements[0][0] = s.x;
    result.elements[0][1] = u.x;
    result.elements[0][2] = -f.x;
    result.elements[0][3] = 0.0f;
    
    result.elements[1][0] = s.y;
    result.elements[1][1] = u.y;
    result.elements[1][2] = -f.y;
    result.elements[1][3] = 0.0f;
    
    result.elements[2][0] = s.z;
    result.elements[2][1] = u.z;
    result.elements[2][2] = -f.z;
    result.elements[2][3] = 0.0f;
    
    result.elements[3][0] = -Dot3F(s, eye);
    result.elements[3][1] = -Dot3F(u, eye);
    result.elements[3][2] = Dot3F(f, eye);
    result.elements[3][3] = 1.0f;
    
    return result;
}

Function M4x4F
TranslationMake4x4F(V3F translation)
{
    M4x4F result = InitialiseDiagonal4x4F(1.0f);
    result.elements[3][0] = translation.x;
    result.elements[3][1] = translation.y;
    result.elements[3][2] = translation.z;
    return result;
}

Function M4x4F
ScaleMake4x4F(V3F scale)
{
    M4x4F result = InitialiseDiagonal4x4F(1.0f);
    result.elements[0][0] = scale.x;
    result.elements[1][1] = scale.y;
    result.elements[2][2] = scale.z;
    return result;
}

Function V4F
Transform4F(V4F a, M4x4F b)
{
    V4F result =
    {
        .x = b.elements[0][0]*a.x + b.elements[0][1]*a.y + b.elements[0][2]*a.z + b.elements[0][3]*a.w,
        .y = b.elements[1][0]*a.x + b.elements[1][1]*a.y + b.elements[1][2]*a.z + b.elements[1][3]*a.w,
        .z = b.elements[2][0]*a.x + b.elements[2][1]*a.y + b.elements[2][2]*a.z + b.elements[2][3]*a.w,
        .w = b.elements[3][0]*a.x + b.elements[3][1]*a.y + b.elements[3][2]*a.z + b.elements[3][3]*a.w,
    };
    return result;
}

//~NOTE(tbt): single integers

Function I32
InterpolateLinear1I(I32 a, I32 b, U8 t) // NOTE(tbt): (0 <= t <= 255)
{
    return ((t*(b - a)) >> 8) + a;
}

Function I32
Min1I(I32 a, I32 b)
{
    if(a < b)
    {
        return a;
    }
    else
    {
        return b;
    }
}

Function I32
Max1I(I32 a, I32 b)
{
    if(a > b)
    {
        return a;
    }
    else
    {
        return b;
    }
}

Function I32
Clamp1I(I32 a, I32 min, I32 max)
{
    return Max1I(min, Min1I(max, a));
}

Function I32
Abs1I(I32 a)
{
    return (a < 0) ? -a : a;
}

Function I32
RotL1I(I32 a, I32 b)
{
    return (a << b) | (a >> (32 - b));
}

Function I32
RotR1I(I32 a, I32 b)
{
    return (a >> b) | (a << (32 - b));
}

Function I32
Normalise1I(I32 a)
{
    I32 result = 0;
    if(a < 0)
    {
        result = -1;
    }
    else if(a > 0)
    {
        result = +1;
    }
    return result;
}

//~NOTE(tbt): integer vectors

Function V4I
U4I(I32  a)
{
    V4I result =
    {
        .x = a,
        .y = a,
        .z = a,
        .w = a,
    };
    return result;
}

Function V4I
Add4I(V4I a, V4I b)
{
    a.x += b.x;
    a.y += b.y;
    a.z += b.z;
    a.w += b.w;
    return a;
}

Function V4I
Sub4I(V4I a, V4I b)
{
    a.x -= b.x;
    a.y -= b.y;
    a.z -= b.z;
    a.w -= b.w;
    return a;
}

Function V4I
Mul4I(V4I a, V4I b)
{
    a.x *= b.x;
    a.y *= b.y;
    a.z *= b.z;
    a.w *= b.w;
    return a;
}

Function V4I
Div4I(V4I a, V4I b)
{
    a.x /= b.x;
    a.y /= b.y;
    a.z /= b.z;
    a.w /= b.w;
    return a;
}

Function I32
Dot4I(V4I a, V4I b)
{
    I32 result = 0;
    result += a.x*b.x;
    result += a.y*b.y;
    result += a.z*b.z;
    result += a.w*b.w;
    return result;
}

Function V4I
Scale4I(V4I a, I32 b)
{
    V4I result =
    {
        .x = a.x*b,
        .y = a.y*b,
        .z = a.z*b,
        .w = a.w*b,
    };
    return result;
}

Function I32
LengthSquared4I(V4I a)
{
    I32 result = a.x*a.x + a.y*a.y + a.z*a.z + a.w*a.w;
    return result;
}

Function F32
Length4I(V4I a)
{
    F32 result = Sqrt1F(LengthSquared4I(a));
    return result;
}

//-

Function V3I
U3I(I32 a)
{
    V3I result =
    {
        .x = a,
        .y = a,
        .z = a,
    };
    return result;
}

Function V3I
Add3I(V3I a, V3I b)
{
    a.x += b.x;
    a.y += b.y;
    a.z += b.z;
    return a;
}

Function V3I
Sub3I(V3I a, V3I b)
{
    a.x -= b.x;
    a.y -= b.y;
    a.z -= b.z;
    return a;
}

Function V3I
Mul3I(V3I a, V3I b)
{
    a.x *= b.x;
    a.y *= b.y;
    a.z *= b.z;
    return a;
}

Function V3I
Div3I(V3I a, V3I b)
{
    a.x /= b.x;
    a.y /= b.y;
    a.z /= b.z;
    return a;
}

Function I32
Dot3I(V3I a, V3I b)
{
    I32 result = 0;
    result += a.x*b.x;
    result += a.y*b.y;
    result += a.z*b.z;
    return result;
}


Function V3I
Scale3I(V3I a, I32 b)
{
    V3I result =
    {
        .x = a.x*b,
        .y = a.y*b,
        .z = a.z*b,
    };
    return result;
}

Function V3I
Cross3i(V3I a, V3I b)
{
    V3I result =
    {
        .x = a.y*b.z - a.z*b.y,
        .y = a.z*b.x - a.x*b.z,
        .z = a.x*b.y - a.y*b.x,
    };
    return result;
}

Function I32
LengthSquared3I(V3I a)
{
    I32 result = a.x*a.x + a.y*a.y + a.z*a.z;
    return result;
}

Function F32
Length3I(V3I a)
{
    F32 result = Sqrt1F(LengthSquared3I(a));
    return result;
}

//-

Function V2I
U2I(I32 a)
{
    V2I result =
    {
        .x = a,
        .y = a,
    };
    return result;
}

Function V2I
Add2I(V2I a, V2I b)
{
    a.x += b.x;
    a.y += b.y;
    return a;
}

Function V2I
Sub2I(V2I a, V2I b)
{
    a.x -= b.x;
    a.y -= b.y;
    return a;
}

Function V2I
Mul2I(V2I a, V2I b)
{
    a.x *= b.x;
    a.y *= b.y;
    return a;
}

Function V2I
Div2I(V2I a, V2I b)
{
    a.x /= b.x;
    a.y /= b.y;
    return a;
}

Function I32
Dot2I(V2I a, V2I b)
{
    I32 result = 0;
    result += a.x*b.x;
    result += a.y*b.y;
    return result;
}


Function V2I
Scale2I(V2I a, I32 b)
{
    V2I result =
    {
        .x = a.x*b,
        .y = a.y*b,
    };
    return result;
}


Function I32
LengthSquared2I(V2I a)
{
    I32 result = a.x*a.x + a.y*a.y;
    return result;
}

Function F32
Length2I(V2I a)
{
    F32 result = Sqrt1F(LengthSquared2I(a));
    return result;
}

//~NOTE(tbt): single unsigned integers

Function U64
Min1U64(U64 a, U64 b)
{
    if(a < b)
    {
        return a;
    }
    else
    {
        return b;
    }
}

Function U64
Max1U64(U64 a, U64 b)
{
    if(a > b)
    {
        return a;
    }
    else
    {
        return b;
    }
}

Function U64
Clamp1U64(U64 a, U64 min, U64 max)
{
    return Max1U64(min, Min1U64(max, a));
}

//~NOTE(tbt): ranges

Function B32
IsOverlapping1U64(Range1U64 a, Range1U64 b)
{
    B32 result = True;
    if(a.max < b.min || a.min >= b.max)
    {
        result = False;
    }
    return result;
}

Function Range1U64
Intersection1U64(Range1U64 a, Range1U64 b)
{
    Range1U64 result = 
    {
        .min = Max1U64(a.min, b.min),
        .max = Min1U64(a.max, b.max),
    };
    return result;
}

Function Range1U64
Bounds1U64(Range1U64 a, Range1U64 b)
{
    Range1U64 result = 
    {
        .min = Min1U64(a.min, b.min),
        .max = Max1U64(a.max, b.max),
    };
    return result;
}

Function Range1U64
Grow1U64(Range1U64 a, U64 b)
{
    Range1U64 result =
    {
        .min = a.min - b,
        .max = a.max + b,
    };
    return result;
}

Function B32
HasPoint1U64(Range1U64 a, U64 b)
{
    B32 result = True;
    if(b < a.min || b >= a.max)
    {
        result = False;
    }
    return result;
}

Function U64
Measure1U64(Range1U64 a)
{
    U64 result = (a.max >= a.min) ? (a.max - a.min) : 0;
    return result;
}

Function B32
IsOverlapping1F(Range1F a, Range1F b)
{
    B32 result = (a.max > b.min && a.min <= b.max);
    return result;
}

Function Range1F
Intersection1F(Range1F a, Range1F b)
{
    Range1F result =
    {
        .min = Max1F(a.min, b.min),
        .max = Min1F(a.max, b.max),
    };
    return result;
}

Function Range1F
Bounds1F(Range1F a, Range1F b)
{
    Range1F result =
    {
        .min = Min1F(a.min, b.min),
        .max = Max1F(a.max, b.max),
    };
    return result;
}

Function Range1F
Grow1F(Range1F a, F32 b)
{
    Range1F result =
    {
        .min = a.min - b,
        .max = a.max + b,
    };
    return result;
}

Function B32
HasPoint1F(Range1F a, F32 b)
{
    B32 result = (a.min <= b && b <= a.max);
    return result;
}

Function F32
Centre1F(Range1F a)
{
    return (a.min + a.max) / 2.0f;
}

Function F32
Measure1F(Range1F a)
{
    F32 result = a.max - a.min;
    return result;
}

Function float
RangeMap1F(Range1F a, Range1F b, float point)
{
    float result = ((point - a.min) / (a.max - a.min))*(b.max - b.min) + b.min;
    return result;
}

Function B32
IsOverlapping2F(Range2F a, Range2F b)
{
    B32 result = ((a.max.x >= b.min.x && a.min.x < b.max.x) &&
                  (a.max.y >= b.min.y && a.min.y < b.max.y));
    return result;
}

Function B32
IsWithin2F(Range2F a, Range2F b)
{
    B32 result = ((a.max.x <= b.max.x && a.min.x >= b.min.x) &&
                  (a.max.y <= b.max.y && a.min.y >= b.min.y));
    return result;
}

Function Range2F
Intersection2F(Range2F a, Range2F b)
{
    Range2F result =
    {
        .min.x = Max1F(a.min.x, b.min.x),
        .max.x = Min1F(a.max.x, b.max.x),
        .min.y = Max1F(a.min.y, b.min.y),
        .max.y = Min1F(a.max.y, b.max.y),
    };
    if(result.min.x > result.max.x ||
       result.min.y > result.max.y)
    {
        result.min.x = 0.0f;
        result.min.y = 0.0f;
        result.max.x = 0.0f;
        result.max.y = 0.0f;
    }
    return result;
}

Function Range2F
Bounds2F(Range2F a, Range2F b)
{
    Range2F result =
    {
        .min.x = Min1F(a.min.x, b.min.x),
        .max.x = Max1F(a.max.x, b.max.x),
        .min.y = Min1F(a.min.y, b.min.y),
        .max.y = Max1F(a.max.y, b.max.y),
    };
    return result;
}

Function Range2F
Grow2F(Range2F a, V2F b)
{
    Range2F result =
    {
        .min = Sub2F(a.min, b),
        .max = Add2F(a.max, b),
    };
    return result;
}

Function B32
HasPoint2F(Range2F a, V2F b)
{
    if(b.x < a.min.x || b.x >= a.max.x) { return False; }
    if(b.y < a.min.y || b.y >= a.max.y) { return False; }
    return True;
}

Function V2F
Centre2F(Range2F a)
{
    V2F result =
    {
        .x = Centre1F(Range1F(a.min.x, a.max.x)),
        .y = Centre1F(Range1F(a.min.y, a.max.y)),
    };
    return result;
}

Function V2F
Measure2F(Range2F a)
{
    V2F result =
    {
        .x = a.max.x - a.min.x,
        .y = a.max.y - a.min.y,
    };
    return result;
}

Function V2F
RangeMap2F(Range2F a, Range2F b, V2F point)
{
    V2F result =
    {
        .x = ((point.x - a.min.x) / (a.max.x - a.min.x))*(b.max.x - b.min.x) + b.min.x,
        .y = ((point.y - a.min.y) / (a.max.y - a.min.y))*(b.max.y - b.min.y) + b.min.y,
    };
    return result;
}

Function Range2F
RectMake2F(V2F pos, V2F dimensions)
{
    V2F max = Add2F(pos, dimensions);
    Range2F result =
    {
        .min = Mins2F(pos, max),
        .max = Maxs2F(pos, max),
    };
    return result;
}

//~NOTE(tbt): colours

Function F32
LinearFromSRGB(F32 srgb)
{
    F32 result = 0.0f;
    if(0.0f <= srgb && srgb <= 0.04045f)
    {
        result = srgb / 12.92f;
    }
    else if(0.04045f < srgb && srgb <= 1.0f)
    {
        result = Pow1F((srgb + 0.055f) / 1.055f, 2.4f);
    }
    return result;
}

Function F32
SRGBFromLinear(F32 linear)
{
    F32 result = 0.0f;
    if(0.0f <= linear && linear <= 0.0031308f)
    {
        result = linear*12.92f;
    }
    else if(0.0031308 < linear && linear <= 1.0f)
    {
        result = 1.055f*Pow1F(linear, 1.0f / 2.4f) - 0.055f;
    }
    return result;
}

Function Pixel
InterpolateLinearPixel(Pixel a, Pixel b, unsigned char t)
{
    Pixel result;
    
    // NOTE(tbt): gamma naive but intended to just be fast and simple
    
    if(0 == t)
    {
        result = a;
    }
    else if(255 == t)
    {
        result = b;
    }
    else
    {
        result.b = ((t*(b.b - a.b)) >> 8) + a.b;
        result.g = ((t*(b.g - a.g)) >> 8) + a.g;
        result.r = ((t*(b.r - a.r)) >> 8) + a.r;
    }
    return result;
}

Function V4F
HSVFromRGB(V4F col)
{
    V4F result = {0};
    
    V4F _col =
    {
        .r = LinearFromSRGB(col.r / col.a),
        .g = LinearFromSRGB(col.g / col.a),
        .b = LinearFromSRGB(col.b / col.a),
        .a = 1.0f,
    };
    
    F32 max = Max1F(Max1F(_col.r, _col.g), _col.b);
    F32 min = Min1F(Min1F(_col.r, _col.g), _col.b);
    F32 delta = max - min;
    
    if(delta > 0.0f)
    {
        if(max == _col.r)
        {
            result.h = 60.0f*(Mod1F(((_col.g - _col.b) / delta), 6.0f));
        }
        else if(max == _col.g)
        {
            result.h = 60.0f*(((_col.b - _col.r) / delta) + 2.0f);
        }
        else if(max == _col.b)
        {
            result.h = 60.0f*(((_col.r - _col.g) / delta) + 4.0f);
        }
        
        if(max > 0)
        {
            result.s = delta / max;
        }
        else
        {
            result.s = 0.0f;
        }
        
        result.v = max;
    }
    else
    {
        result.h = 0.0f;
        result.s = 0.0f;
        result.v = max;
    }
    
    if(result.h < 0.0f)
    {
        result.h = 360.0f + result.h;
    }
    
    result.a = col.a;
    
    return result;
}

Function V4F
RGBFromHSV(V4F col)
{
    V4F result = {0};
    
    F64 chroma = col.v*col.s;
    F64 h = Mod1F(col.h, 360.0f) / 60.0f;
    I64 i = h;
    F64 f = h - i;
    F64 p = col.v * (1.0 - col.s);
    F64 q = col.v * (1.0 - (col.s * f));
    F64 t = col.v * (1.0 - (col.s * (1.0 - f)));
    
    V3F linear = {0};
    if(0 == i)
    {
        linear = V3F(col.v, t, p);
    }
    else if(1 == i)
    {
        linear = V3F(q, col.v, p);
    }
    else if(2 == i)
    {
        linear = V3F(p, col.v, t);
    }
    else if(3 == i)
    {
        linear = V3F(p, q, col.v);
    }
    else if(4 == i)
    {
        linear = V3F(t, p, col.v);
    }
    else if(5 == i)
    {
        linear = V3F(col.v, p, q);
    }
    
    result = V4F(SRGBFromLinear(linear.r)*col.a,
                 SRGBFromLinear(linear.g)*col.a,
                 SRGBFromLinear(linear.b)*col.a,
                 col.a);
    
    return result;
}

Function V4F
ColMix(V4F a, V4F b, F32 t)
{
    V4F result = {0};
    
    if(t < 0.001f)
    {
        result = a;
    }
    else if(t > 0.999f)
    {
        result = b;
    }
    else
    {
        if(0.0f == a.a)
        {
            result.r = b.r;
            result.g = b.g;
            result.b = b.b;
            result.a = b.a*t;
        }
        else if(0.0f == b.a)
        {
            result.r = a.r;
            result.g = a.g;
            result.b = a.b;
            result.a = a.a*(1.0f - t);
        }
        else
        {
            a.r = LinearFromSRGB(a.r) / a.a;
            a.g = LinearFromSRGB(a.g) / a.a;
            a.b = LinearFromSRGB(a.b) / a.a;
            
            b.r = LinearFromSRGB(b.r) / b.a;
            b.g = LinearFromSRGB(b.g) / b.a;
            b.b = LinearFromSRGB(b.b) / b.a;
            
            result.r = InterpolateLinear1F(a.r, b.r, t);
            result.g = InterpolateLinear1F(a.g, b.g, t);
            result.b = InterpolateLinear1F(a.b, b.b, t);
            result.a = InterpolateLinear1F(a.a, b.a, t);
            
            result.r = SRGBFromLinear(result.r*result.a);
            result.g = SRGBFromLinear(result.g*result.a);
            result.b = SRGBFromLinear(result.b*result.a);
        }
    }
    
    return result;
}
