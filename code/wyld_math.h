/* date = February 2nd 2024 11:23 pm */

#ifndef WYLD_MATH_H
#define WYLD_MATH_H

typedef union {
  struct {
    f32 x, y;
  };
  struct {
    f32 width, height;
  };
  f32 v[2];
} v2f;

typedef union {
  struct {
    s32 x, y;
  };
  struct {
    s32 width, height;
  };
  s32 v[2];
} v2i;

typedef union {
  struct {
    f32 x, y, z;
  };
  struct {
    f32 r, g, b;
  };
  f32 v[3];
} v3f;

typedef union {
  struct {
    f32 x, y, z, w;
  };
  struct {
    f32 r, g, b, a;
  };
  struct {
    v3f xyz;
    f32 __unused_a;
  };
  struct {
    v3f rgb;
    f32 __unused_b;
  };
  f32 v[4];
} v4f;

typedef union {
  struct {
    v4f r0, r1;
  };
  v4f rows[2];
  f32 m[2][4];
} m24;

typedef union {
  struct {
    v4f r0, r1, r2, r3;
  };
  v4f rows[4];
  f32 m[4][4];
} m44;

typedef struct {
  m24 mat;
} Hermite_Curve;

typedef struct {
  m24 mat;
} Bezier_Curve;

typedef struct {
  v2f p;
  v2f dims;
} RectF32;

typedef struct {
  v2i p;
  v2i dims;
} RectS32;

#define pi_f32 3.141592653589f
#define two_pi_f32 6.28318530718f

#define pi_f64 3.141592653589
#define two_pi_f64 6.28318530718

//~ NOTE(christian): math tool
inl f32 to_radians(f32 degrees);
inl f32 absolute_valuef32(f32 a);
inl b32 close_enough_zerof32(f32 a);
inl b32 close_enough_equalf32(f32 a, f32 b);

//~ NOTE(christian): v2is
inl v2i v2i_make(s32 x, s32 y);
inl v2i v2i_negate(v2i a);
inl v2i v2i_add(v2i a, v2i b);
inl v2i v2i_sub(v2i a, v2i b);

//~ NOTE(christian): v2fs
#define v2f_zero() v2f_make(0,0)
#define v2f_make_uniform(x) v2f_make(x,x)
inl v2f v2f_make(f32 x, f32 y);
inl v2f v2f_negate(v2f a);
inl v2f v2f_add(v2f a, v2f b);
inl v2f v2f_sub(v2f a, v2f b);
inl v2f v2f_scale(v2f a, f32 s);
inl v2f v2f_inv_scale(v2f a, f32 s);
inl void v2f_normalize(v2f *a);
inl f32 v2f_dot(v2f a, v2f b);
inl f32 v2f_cross(v2f a, v2f b);
inl f32 v2f_length(v2f a);

inl void v2f_plus_equals(v2f *a, v2f b);
inl void v2f_minus_equals(v2f *a, v2f b);
inl void v2f_mul_equals(v2f *a, f32 s);
inl void v2f_div_equals(v2f *a, f32 s);

//~ NOTE(christian): convs / shtuff
inl v2i v2f_to_v2i(v2f from);
inl v2f v2i_to_v2f(v2i from);
inl v2i v2i_min(v2i a, v2i b);
inl v2i v2i_max(v2i a, v2i b);
inl v2f v2f_min(v2f a, v2f b);
inl v2f v2f_max(v2f a, v2f b);
inl v2f v2f_lerp(v2f origin, v2f dest, f32 t);
inl void v2f_add_eq(v2f *a, v2f b);

inl RectF32 rectf32_make(v2f p, v2f dims);
fun b32 point_in_rect(v2f *p, RectF32 *rect);

//~ NOTE(christian): rects32
inl RectS32 r32_make(v2i p, v2i dims);

inl v3f v3f_add(v3f a, v3f b);
inl v3f v3f_scale(v3f v, f32 s);
inl v3f v3f_lerp(v3f origin, v3f dest, f32 t);

//~ NOTE(christian): v4fs
#define rgba_make(r,g,b,a) v4f_make(r,g,b,a)
#define rgba_normalize(r,g,b,a) v4f_make((r)*0.00392156862f,(g)*0.00392156862f,(b)*0.00392156862f,(a)*0.00392156862f)
inl v4f v4f_make(f32 x, f32 y, f32 z, f32 w);
// NOTE(christian):
// hue => [0, 360] degrees
// saturation => [0, 100] percent
// value => [0, 100] percent
// alpha => [0, 1] norm percent
inl v4f rgba_from_hsva(f32 hue, f32 saturation, f32 value, f32 alpha);

//~ NOTE(Christian): m24s
inl m24 m24_mul_m44(m24 *a, m44 *b);
inl v2f m24_mul_v4(m24 *a, v4f b);

//~ NOTE(christian): m44s
inl m44 m44_make_ortho_lh_z01(f32 left, f32 right, f32 top, f32 bottom, f32 near_plane, f32 far_plane);
inl v4f m44_mul_vec4(m44 *m, v4f v);

//~ NOTE(christian): ease
inl f32 ease_out_quart(f32 t);
inl f32 ease_in_quart(f32 t);
inl f32 ease_in_quad(f32 t);

//~ NOTE(Christian): Hermite
inl Hermite_Curve herm_make(v2f start, v2f end, v2f start_tangent, v2f end_tangent);
inl v2f herm_point(Hermite_Curve *curve, f32 t);

//~ NOTE(Christian): bezier
inl Bezier_Curve bezier_make(v2f P_1, v2f P_2, v2f P_3, v2f P_4);
inl v2f bezier_point(Bezier_Curve *curve, f32 t);
#endif //WYLD_MATH_H
