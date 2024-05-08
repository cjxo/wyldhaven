
//~ NOTE(christian): math tool
inl f32
to_radians(f32 degrees) {
    f32 result = degrees * (pi_f32 / 180.0f);
    return(result);
}

inl f32
absolute_valuef32(f32 a) {
    union { f32 f; u32 n; } v;
    v.f = a;
    v.n &= ~0x80000000;
    return(v.f);
}

inl b32
close_enough_zerof32(f32 a) {
    b32 result = absolute_valuef32(a) < 1e-8f;
    return(result);
}

inl b32
close_enough_equalf32(f32 a, f32 b) {
    b32 result = close_enough_zerof32(a - b);
    return(result);
}

//~ NOTE(christian): v2i
inl v2i
v2i_make(s32 x, s32 y) {
    v2i result;
    result.x = x;
    result.y = y;
    return(result);
}

inl v2i
v2i_negate(v2i a) {
    v2i result;
    result.x = -a.x;
    result.y = -a.y;
    return(result);
}

inl v2i
v2i_add(v2i a, v2i b) {
    v2i result;
    result.x = a.x + b.x;
    result.y = a.y + b.y;
    return(result);
}

inl v2i
v2i_sub(v2i a, v2i b) {
    v2i result;
    result.x = a.x - b.x;
    result.y = a.y - b.y;
    return(result);
}

//~ NOTE(christian): v2fs
inl v2f
v2f_make(f32 x, f32 y) {
    v2f result;
    result.x = x;
    result.y = y;
    return(result);
}

inl v2f
v2f_negate(v2f a) {
    return v2f_make(-a.x, -a.y);
}

inl v2f
v2f_add(v2f a, v2f b) {
    return v2f_make(a.x + b.x, a.y + b.y);
}

inl v2f
v2f_sub(v2f a, v2f b) {
    return v2f_make(a.x - b.x, a.y - b.y);
}

inl v2f
v2f_scale(v2f a, f32 s) {
    return v2f_make(a.x * s, a.y * s);
}

inl v2f
v2f_inv_scale(v2f a, f32 s) {
    return v2f_scale(a, 1.0f / s);
}

inl void
v2f_normalize(v2f *a) {
    f32 ilen = 1.0f / v2f_length(*a);
    a->x *= ilen;
    a->y *= ilen;
}

inl f32
v2f_dot(v2f a, v2f b) {
    f32 result = a.x * b.x + a.y * b.y;
    return(result);
}

inl f32
v2f_cross(v2f a, v2f b) {
    f32 result = a.x * b.y - a.y * b.x;
    return(result);
}

inl f32
v2f_length(v2f a) {
    return sqrtf(v2f_dot(a, a));
}

inl void
v2f_plus_equals(v2f *a, v2f b) {
    a->x += b.x;
    a->y += b.y;
}

inl void
v2f_minus_equals(v2f *a, v2f b) {
    a->x -= b.x;
    a->y -= b.y;
}

inl void
v2f_mul_equals(v2f *a, f32 s) {
    a->x *= s;
    a->y *= s;
}

inl void
v2f_div_equals(v2f *a, f32 s) {
    a->x /= s;
    a->y /= s;
}

inl v2f
v2f_lerp(v2f origin, v2f dest, f32 t) {
    v2f result = v2f_add(v2f_scale(origin, (1.0f - t)), v2f_scale(dest, t));
    return(result);
}

fun b32
point_in_rect(v2f *p, RectF32 *rect) {
    f32 rect_max_x = rect->p.x + rect->dims.width;
    f32 rect_max_y = rect->p.y + rect->dims.height;
    
    if ((p->x < rect->p.x) || (p->x > rect_max_x)) {
        return false;
    }
    
    if ((p->y < rect->p.y) || (p->y > rect_max_y)) {
        return false;
    }
    
    return true;
}

inl RectS32
r32_make(v2i p, v2i dims) {
    RectS32 result;
    result.p = p;
    result.dims = dims;
    return(result);
}

//~ NOTE(christian): convs/shtuff
inl v2i
v2f_to_v2i(v2f from) {
    v2i result;
    result.x = (s32)from.x;
    result.y = (s32)from.y;
    return(result);
}

inl v2f
v2i_to_v2f(v2i from) {
    v2f result;
    result.x = (f32)from.x;
    result.y = (f32)from.y;
    return(result);
}

inl v2i
v2i_min(v2i a, v2i b) {
    v2i result;
    result.x = minimum(a.x, b.x);
    result.y = minimum(a.y, b.y);
    return(result);
}

inl v2i
v2i_max(v2i a, v2i b) {
    v2i result;
    result.x = maximum(a.x, b.x);
    result.y = maximum(a.y, b.y);
    return(result);
}

inl v2f
v2f_min(v2f a, v2f b) {
    v2f result;
    result.x = minimum(a.x, b.x);
    result.y = minimum(a.y, b.y);
    return(result);
}

inl v2f
v2f_max(v2f a, v2f b) {
    v2f result;
    result.x = maximum(a.x, b.x);
    result.y = maximum(a.y, b.y);
    return(result);
}

inl v3f
v3f_add(v3f a, v3f b) {
    v3f result;
    result.x = a.x + b.x;
    result.y = a.y + b.y;
    result.z = a.z + b.z;
    return(result);
}

inl v3f
v3f_scale(v3f v, f32 s) {
	v3f result;
	result.x = v.x * s;
	result.y = v.y * s;
	result.z = v.z * s;
	return(result);
}

inl v3f
v3f_lerp(v3f origin, v3f dest, f32 t) {
    v3f result;
    result = v3f_add(v3f_scale(origin, 1.0f - t), v3f_scale(dest, t));
    return(result);
}

//~ NOTE(christian): v4fs
inl v4f
v4f_make(f32 x, f32 y, f32 z, f32 w) {
    v4f result;
    result.x = x;
    result.y = y;
    result.z = z;
    result.w = w;
    
    return(result);
}

inl f32
v4f_dot(v4f a, v4f b) {
	f32 result = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
	return(result);
}
inl v4f
rgba_from_hsva(f32 hue, f32 saturation, f32 value, f32 alpha) {
    v4f result;
    result.a = alpha;
    
    f32 v_norm = value / 100.0f;
    f32 s_norm = saturation / 100.0f;
    
    f32 xx = absolute_valuef32(fmodf(hue / 60.0f, 2.0f) - 1.0f);
    f32 C = v_norm * s_norm;
    f32 X = C * (1.0f - xx);
    f32 m = v_norm - C;
    
    f32 r_p, g_p, b_p;
    
    if (hue < 60.0f) {
        r_p = C;
        g_p = X;
        b_p = 0.0f;
    } else if (hue < 120.0f) {
        r_p = X;
        g_p = C;
        b_p = 0.0f;
    } else if (hue < 180.0f) {
        r_p = 0.0f;
        g_p = C;
        b_p = X;
    } else if (hue < 240.0f) {
        r_p = 0.0f;
        g_p = X;
        b_p = C;
    } else if (hue < 300.0f) {
        r_p = X;
        g_p = 0.0f;
        b_p = C;
    } else {
        r_p = C;
        g_p = 0.0f;
        b_p = X;
    }
    
    result.r = r_p + m;
    result.g = g_p + m;
    result.b = b_p + m;
    
    return(result);
}

//~ NOTE(christian): m44s
inl m44
m44_make_ortho_lh_z01(f32 left, f32 right, f32 top, f32 bottom, f32 near, f32 far) {
    m44 result;
    result.r0 = v4f_make(2.0f / (right - left), 0.0f, 0.0f, 0.0f);
    result.r1 = v4f_make(0.0f, 2.0f / (top - bottom), 0.0f, 0.0f);
    result.r2 = v4f_make(0.0f, 0.0f, 1.0f / (far - near), 0.0f);
    result.r3 = v4f_make(-(right + left) / (right - left), -(top + bottom) / (top - bottom), -near / (far - near), 1.0f);
    
    return(result);
}

inl v4f
m44_mul_vec4(m44 *m, v4f v) {
	v4f result;
	result.x = v4f_dot(m->rows[0], v);
	result.y = v4f_dot(m->rows[1], v);
	result.z = v4f_dot(m->rows[2], v);
	result.w = v4f_dot(m->rows[3], v);
	return(result);
}

inl f32
ease_out_quart(f32 t) {
    f32 f = 1.0f - t;
    f32 x = f * f * f * f;
    f32 result = 1.0f - x;
    return(result);
}

inl f32
ease_in_quart(f32 t) {
    f32 result = t * t * t * t;
    return(result);
}

inl f32
ease_in_quad(f32 t) {
    f32 result = t * t;
    return(result);
}

inl f32
ease_out_quad(f32 t) {
    f32 result = 1.0f - (1.0f - t) * (1.0f - t);
    return(result);
}
