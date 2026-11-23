// Cyber Drift — portable math layer (no dependencies, GLES-friendly).
#ifndef CD_MATH_H
#define CD_MATH_H

#include <math.h>
#include <stdint.h>

#define CD_PI 3.14159265358979323846f
#define CD_TAU 6.28318530717958647692f

static inline float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
static inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }
static inline float signf(float v) { return v < 0.f ? -1.f : 1.f; }
static inline float smoothstepf(float t) { t = clampf(t, 0.f, 1.f); return t * t * (3.f - 2.f * t); }
static inline float move_to(float cur, float target, float max_delta) {
    float d = target - cur;
    if (d > max_delta) d = max_delta;
    if (d < -max_delta) d = -max_delta;
    return cur + d;
}
static inline float expo_lerp(float cur, float target, float rate, float dt) {
    return target + (cur - target) * expf(-rate * dt);
}

typedef struct { float x, y; } vec2;
typedef struct { float x, y, z; } vec3;
typedef struct { float x, y, z, w; } vec4;
typedef struct { float m[16]; } mat4;  // column-major, like OpenGL

static inline vec2 v2(float x, float y) { vec2 r = {x, y}; return r; }
static inline vec3 v3(float x, float y, float z) { vec3 r = {x, y, z}; return r; }
static inline vec4 v4(float x, float y, float z, float w) { vec4 r = {x, y, z, w}; return r; }

static inline vec3 v3add(vec3 a, vec3 b) { return v3(a.x + b.x, a.y + b.y, a.z + b.z); }
static inline vec3 v3sub(vec3 a, vec3 b) { return v3(a.x - b.x, a.y - b.y, a.z - b.z); }
static inline vec3 v3scale(vec3 a, float s) { return v3(a.x * s, a.y * s, a.z * s); }
static inline vec3 v3mul(vec3 a, vec3 b) { return v3(a.x * b.x, a.y * b.y, a.z * b.z); }
static inline float v3dot(vec3 a, vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static inline vec3 v3cross(vec3 a, vec3 b) {
    return v3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
}
static inline float v3len(vec3 a) { return sqrtf(v3dot(a, a)); }
static inline vec3 v3norm(vec3 a) {
    float l = v3len(a);
    return l > 1e-6f ? v3scale(a, 1.f / l) : v3(0, 0, 1);
}
static inline vec3 v3lerp(vec3 a, vec3 b, float t) { return v3add(a, v3scale(v3sub(b, a), t)); }
static inline vec3 v3neg(vec3 a) { return v3(-a.x, -a.y, -a.z); }

static inline float dot2(vec2 a, vec2 b) { return a.x * b.x + a.y * b.y; }
static inline float len2(vec2 a) { return sqrtf(dot2(a, a)); }
static inline vec2 norm2(vec2 a) {
    float l = len2(a);
    return l > 1e-6f ? v2(a.x / l, a.y / l) : v2(0, 1);
}

// Column-major helpers ------------------------------------------------------
static inline mat4 mat4_identity(void) {
    mat4 r = {{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}};
    return r;
}
static inline mat4 mat4_mul(mat4 a, mat4 b) {
    mat4 r;
    for (int c = 0; c < 4; c++)
        for (int row = 0; row < 4; row++) {
            float s = 0.f;
            for (int k = 0; k < 4; k++) s += a.m[k * 4 + row] * b.m[c * 4 + k];
            r.m[c * 4 + row] = s;
        }
    return r;
}
static inline vec4 mat4_mulv4(mat4 a, vec4 v) {
    vec4 r;
    r.x = a.m[0] * v.x + a.m[4] * v.y + a.m[8] * v.z + a.m[12] * v.w;
    r.y = a.m[1] * v.x + a.m[5] * v.y + a.m[9] * v.z + a.m[13] * v.w;
    r.z = a.m[2] * v.x + a.m[6] * v.y + a.m[10] * v.z + a.m[14] * v.w;
    r.w = a.m[3] * v.x + a.m[7] * v.y + a.m[11] * v.z + a.m[15] * v.w;
    return r;
}
static inline mat4 mat4_translate(vec3 t) {
    mat4 r = mat4_identity();
    r.m[12] = t.x; r.m[13] = t.y; r.m[14] = t.z;
    return r;
}
static inline mat4 mat4_scale(vec3 s) {
    mat4 r = mat4_identity();
    r.m[0] = s.x; r.m[5] = s.y; r.m[10] = s.z;
    return r;
}
static inline mat4 mat4_rotX(float a) {
    mat4 r = mat4_identity();
    float c = cosf(a), s = sinf(a);
    r.m[5] = c; r.m[6] = s; r.m[9] = -s; r.m[10] = c;
    return r;
}
static inline mat4 mat4_rotY(float a) {
    mat4 r = mat4_identity();
    float c = cosf(a), s = sinf(a);
    r.m[0] = c; r.m[2] = -s; r.m[8] = s; r.m[10] = c;
    return r;
}
static inline mat4 mat4_rotZ(float a) {
    mat4 r = mat4_identity();
    float c = cosf(a), s = sinf(a);
    r.m[0] = c; r.m[1] = s; r.m[4] = -s; r.m[5] = c;
    return r;
}
static inline mat4 mat4_perspective(float fovy_rad, float aspect, float znear, float zfar) {
    float f = 1.f / tanf(fovy_rad * 0.5f);
    mat4 r = {{0}};
    r.m[0] = f / aspect;
    r.m[5] = f;
    r.m[10] = (zfar + znear) / (znear - zfar);
    r.m[11] = -1.f;
    r.m[14] = (2.f * zfar * znear) / (znear - zfar);
    return r;
}
// Right-handed look-at with a glTF-style basis [-right, up, -forward]:
// +X right, +Y up, +Z towards the viewer. Front faces stay counter-clockwise.
static inline mat4 mat4_lookAt(vec3 eye, vec3 target, vec3 up) {
    vec3 f = v3norm(v3sub(target, eye));
    vec3 s = v3norm(v3cross(f, up));
    vec3 u = v3cross(s, f);
    mat4 r = mat4_identity();
    r.m[0] = -s.x; r.m[4] = -s.y; r.m[8] = -s.z;
    r.m[1] = u.x;  r.m[5] = u.y;  r.m[9] = u.z;
    r.m[2] = -f.x; r.m[6] = -f.y; r.m[10] = -f.z;
    r.m[12] = v3dot(s, eye);
    r.m[13] = -v3dot(u, eye);
    r.m[14] = v3dot(f, eye);
    return r;
}
static inline mat4 mat4_ortho(float l, float r_, float b, float t, float n, float f) {
    mat4 r = mat4_identity();
    r.m[0] = 2.f / (r_ - l);
    r.m[5] = 2.f / (t - b);
    r.m[10] = -2.f / (f - n);
    r.m[12] = -(r_ + l) / (r_ - l);
    r.m[13] = -(t + b) / (t - b);
    r.m[14] = -(f + n) / (f - n);
    return r;
}

// Deterministic RNG (tiny xorshift) so the procedural world is reproducible.
typedef struct { uint32_t s; } rng_t;
static inline uint32_t rng_u32(rng_t* r) {
    r->s ^= r->s << 13;
    r->s ^= r->s >> 17;
    r->s ^= r->s << 5;
    return r->s;
}
static inline float rng_f(rng_t* r) { return (float)(rng_u32(r) & 0xFFFFFF) / 16777215.f; }
static inline float rng_range(rng_t* r, float lo, float hi) { return lo + (hi - lo) * rng_f(r); }
static inline int rng_int(rng_t* r, int lo, int hi) { return lo + (int)(rng_u32(r) % (uint32_t)(hi - lo + 1)); }

#endif
