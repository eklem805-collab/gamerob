#include "cd_mesh.h"
#include <stdlib.h>
#include <string.h>

void cd_mesh_init(cd_mesh* m) { m->v = NULL; m->count = 0; m->cap = 0; }

void cd_mesh_free(cd_mesh* m) {
    free(m->v);
    m->v = NULL;
    m->count = m->cap = 0;
}

void cd_mesh_clear(cd_mesh* m) { m->count = 0; }

void cd_mesh_reserve(cd_mesh* m, int extra) {
    if (m->count + extra <= m->cap) return;
    int cap = m->cap ? m->cap : 256;
    while (cap < m->count + extra) cap *= 2;
    m->v = (cd_vertex*)realloc(m->v, (size_t)cap * sizeof(cd_vertex));
    m->cap = cap;
}

static void push(cd_mesh* m, vec3 p, vec3 n, vec4 c) {
    cd_mesh_reserve(m, 1);
    cd_vertex* v = &m->v[m->count++];
    v->px = p.x; v->py = p.y; v->pz = p.z;
    v->nx = n.x; v->ny = n.y; v->nz = n.z;
    v->r = c.x; v->g = c.y; v->b = c.z; v->a = c.w;
}

// Flat-shaded triangle: normal derived from winding so low-poly facets stay crisp.
void cd_tri(cd_mesh* m, vec3 a, vec3 b, vec3 c, vec4 col) {
    vec3 n = v3norm(v3cross(v3sub(b, a), v3sub(c, a)));
    push(m, a, n, col);
    push(m, b, n, col);
    push(m, c, n, col);
}

// a-b-c-d counter-clockwise when viewed from the front face.
void cd_quad(cd_mesh* m, vec3 a, vec3 b, vec3 c, vec3 d, vec4 col) {
    cd_tri(m, a, b, c, col);
    cd_tri(m, a, c, d, col);
}

void cd_box_rot(cd_mesh* m, vec3 c, vec3 h, float yaw, vec4 col);

void cd_box(cd_mesh* m, vec3 c, vec3 h, vec4 col) {
    cd_box_rot(m, c, h, 0.f, col);
}

// Axis-aligned box, optionally yawed (used for grooved floors and facades).
// a-b-c-d are counter-clockwise seen from +Z (glTF front-face convention).
void cd_box_rot(cd_mesh* m, vec3 c, vec3 h, float yaw, vec4 col) {
    float ca = cosf(yaw), sa = sinf(yaw);
    vec3 p[8];
    for (int i = 0; i < 8; i++) {
        float x = (i & 1) ? h.x : -h.x;
        float y = (i & 2) ? h.y : -h.y;
        float z = (i & 4) ? h.z : -h.z;
        p[i] = v3(c.x + x * ca + z * sa, c.y + y, c.z - x * sa + z * ca);
    }
    int f[6][4] = {
        {4, 5, 7, 6},   // +Z
        {1, 0, 2, 3},   // -Z
        {5, 1, 3, 7},   // +X
        {0, 4, 6, 2},   // -X
        {2, 6, 7, 3},   // +Y
        {0, 1, 5, 4},   // -Y
    };
    for (int i = 0; i < 6; i++)
        cd_quad(m, p[f[i][0]], p[f[i][1]], p[f[i][2]], p[f[i][3]], col);
}

// Box with glowing edge tubes — the signature synthwave look.
void cd_box_emit_edges(cd_mesh* m, vec3 c, vec3 h, vec4 col, vec4 emit, float t) {
    cd_box(m, c, h, col);
    float x0 = c.x - h.x, x1 = c.x + h.x;
    float y0 = c.y - h.y, y1 = c.y + h.y;
    float z0 = c.z - h.z, z1 = c.z + h.z;
    vec3 corners[8] = {
        v3(x0, y0, z0), v3(x1, y0, z0), v3(x1, y1, z0), v3(x0, y1, z0),
        v3(x0, y0, z1), v3(x1, y0, z1), v3(x1, y1, z1), v3(x0, y1, z1)};
    for (int i = 0; i < 4; i++) {
        cd_beam(m, corners[i], corners[(i + 1) & 3], t, emit);
        cd_beam(m, corners[4 + i], corners[4 + ((i + 1) & 3)], t, emit);
        cd_beam(m, corners[i], corners[4 + i], t, emit);
    }
}

void cd_cylinder(cd_mesh* m, vec3 a, vec3 b, float r0, float r1, int sides, vec4 col) {
    vec3 axis = v3sub(b, a);
    vec3 dir = v3norm(axis);
    vec3 tmp = fabsf(dir.y) < 0.9f ? v3(0, 1, 0) : v3(1, 0, 0);
    vec3 u = v3norm(v3cross(tmp, dir));
    vec3 v = v3cross(dir, u);
    for (int i = 0; i < sides; i++) {
        float a0 = (float)i / sides * CD_TAU;
        float a1 = (float)(i + 1) / sides * CD_TAU;
        vec3 d0 = v3add(v3scale(u, cosf(a0)), v3scale(v, sinf(a0)));
        vec3 d1 = v3add(v3scale(u, cosf(a1)), v3scale(v, sinf(a1)));
        vec3 a0p = v3add(a, v3scale(d0, r0)), a1p = v3add(a, v3scale(d1, r0));
        vec3 b0p = v3add(b, v3scale(d0, r1)), b1p = v3add(b, v3scale(d1, r1));
        cd_quad(m, a0p, a1p, b1p, b0p, col);   // outward facing
    }
}

void cd_disc(cd_mesh* m, vec3 c, vec3 n, float radius, int sides, vec4 col) {
    vec3 normal = v3norm(n);
    vec3 tmp = fabsf(normal.y) < 0.9f ? v3(0, 1, 0) : v3(1, 0, 0);
    vec3 u = v3norm(v3cross(tmp, normal));
    vec3 v = v3cross(normal, u);
    for (int i = 0; i < sides; i++) {
        float a0 = (float)i / sides * CD_TAU;
        float a1 = (float)(i + 1) / sides * CD_TAU;
        vec3 p0 = v3add(c, v3scale(v3add(v3scale(u, cosf(a0)), v3scale(v, sinf(a0))), radius));
        vec3 p1 = v3add(c, v3scale(v3add(v3scale(u, cosf(a1)), v3scale(v, sinf(a1))), radius));
        cd_tri(m, c, p0, p1, col);
    }
}

// Frustum of `sides` corners: the workhorse for neons and buildings.
void cd_prism(cd_mesh* m, vec3 c, float radius, float h_top, float h_bot, int sides, vec4 col, float twist) {
    for (int i = 0; i < sides; i++) {
        float a0 = (float)i / sides * CD_TAU;
        float a1 = (float)(i + 1) / sides * CD_TAU;
        vec3 t0 = v3(c.x + cosf(a0) * radius, c.y + h_top, c.z + sinf(a0) * radius);
        vec3 t1 = v3(c.x + cosf(a1) * radius, c.y + h_top, c.z + sinf(a1) * radius);
        float b0a = a0 + twist, b1a = a1 + twist;
        vec3 b0 = v3(c.x + cosf(b0a) * radius, c.y + h_bot, c.z + sinf(b0a) * radius);
        vec3 b1 = v3(c.x + cosf(b1a) * radius, c.y + h_bot, c.z + sinf(b1a) * radius);
        cd_quad(m, t0, t1, b1, b0, col);
        cd_tri(m, v3(c.x, c.y + h_bot, c.z), b0, b1, col);
        cd_tri(m, v3(c.x, c.y + h_top, c.z), t1, t0, col);
    }
}

void cd_pyramid(cd_mesh* m, vec3 c, float radius, float height, int sides, vec4 col, float twist) {
    vec3 apex = v3(c.x, c.y + height, c.z);
    for (int i = 0; i < sides; i++) {
        float a0 = (float)i / sides * CD_TAU + twist;
        float a1 = (float)(i + 1) / sides * CD_TAU + twist;
        vec3 p0 = v3(c.x + cosf(a0) * radius, c.y, c.z + sinf(a0) * radius);
        vec3 p1 = v3(c.x + cosf(a1) * radius, c.y, c.z + sinf(a1) * radius);
        cd_tri(m, p1, p0, apex, col);
        cd_tri(m, c, p0, p1, col);
    }
}

void cd_ring(cd_mesh* m, vec3 c, vec3 normal, float radius, float tube, int seg_major, int seg_minor, vec4 col, int glow_sides) {
    vec3 n = v3norm(normal);
    vec3 tmp = fabsf(n.y) < 0.9f ? v3(0, 1, 0) : v3(1, 0, 0);
    vec3 u = v3norm(v3cross(tmp, n));
    vec3 v = v3cross(n, u);
    for (int i = 0; i < seg_major; i++) {
        // Only the outer band gets the neon emission to save fill rate.
        vec4 cc = (i % 4 < glow_sides) ? col : col;
        float a0 = (float)i / seg_major * CD_TAU;
        float a1 = (float)(i + 1) / seg_major * CD_TAU;
        for (int j = 0; j < seg_minor; j++) {
            float b0 = (float)j / seg_minor * CD_TAU;
            float b1 = (float)(j + 1) / seg_minor * CD_TAU;
            vec3 p[4];
            float aa[2] = {a0, a1};
            float bb[2] = {b0, b1};
            for (int k = 0; k < 4; k++) {
                float a = aa[k & 1];
                float b = bb[k >> 1];
                vec3 dir = v3add(v3scale(u, cosf(a)), v3scale(v, sinf(a)));
                vec3 pos = v3add(c, v3scale(dir, radius));
                vec3 side = v3norm(v3cross(v3sub(pos, c), n));
                vec3 up = v3norm(v3sub(pos, c));
                vec3 off = v3add(v3scale(side, cosf(b) * tube), v3scale(up, sinf(b) * tube));
                p[k] = v3add(pos, off);
            }
            // p[0]=a0b0 p[1]=a1b0 p[2]=a0b1 p[3]=a1b1
            cd_quad(m, p[0], p[1], p[3], p[2], cc);
        }
    }
}

// Icosphere: even low-poly ball used for orbs, missiles and boulders.
void cd_icosphere(cd_mesh* m, vec3 c, float radius, int subdiv, vec4 col) {
    static const float t = 1.618034f;
    vec3 base[12] = {
        v3(-1, t, 0), v3(1, t, 0), v3(-1, -t, 0), v3(1, -t, 0),
        v3(0, -1, t), v3(0, 1, t), v3(0, -1, -t), v3(0, 1, -t),
        v3(t, 0, -1), v3(t, 0, 1), v3(-t, 0, -1), v3(-t, 0, 1)};
    static const int faces[20][3] = {
        {0, 11, 5}, {0, 5, 1}, {0, 1, 7}, {0, 7, 10}, {0, 10, 11},
        {1, 5, 9}, {5, 11, 4}, {11, 10, 2}, {10, 7, 6}, {7, 1, 8},
        {3, 9, 4}, {3, 4, 2}, {3, 2, 6}, {3, 6, 8}, {3, 8, 9},
        {4, 0, 2}, {2, 1, 6}, {6, 8, 7}, {9, 5, 4}, {8, 1, 3}};
    for (int i = 0; i < 20; i++) {
        vec3 a = v3norm(base[faces[i][0]]);
        vec3 b = v3norm(base[faces[i][1]]);
        vec3 cc = v3norm(base[faces[i][2]]);
        if (subdiv <= 0) {
            cd_tri(m, v3add(c, v3scale(a, radius)), v3add(c, v3scale(b, radius)),
                   v3add(c, v3scale(cc, radius)), col);
            continue;
        }
        vec3 ab = v3norm(v3add(a, b)), bc = v3norm(v3add(b, cc)), ca = v3norm(v3add(cc, a));
        vec3 A = v3add(c, v3scale(a, radius)), B = v3add(c, v3scale(b, radius)), C = v3add(c, v3scale(cc, radius));
        vec3 AB = v3add(c, v3scale(ab, radius)), BC = v3add(c, v3scale(bc, radius)), CA = v3add(c, v3scale(ca, radius));
        cd_tri(m, A, AB, CA, col);
        cd_tri(m, B, BC, AB, col);
        cd_tri(m, C, CA, BC, col);
        cd_tri(m, AB, BC, CA, col);
    }
}

// Square-section beam between two points (neon tube / strut).
void cd_beam(cd_mesh* m, vec3 a, vec3 b, float w, vec4 col) {
    vec3 axis = v3sub(b, a);
    float l = v3len(axis);
    if (l < 1e-4f) return;
    vec3 dir = v3scale(axis, 1.f / l);
    vec3 tmp = fabsf(dir.y) < 0.9f ? v3(0, 1, 0) : v3(1, 0, 0);
    vec3 u = v3norm(v3cross(tmp, dir));
    vec3 v = v3cross(dir, u);
    vec3 o0 = v3scale(v3add(u, v), w * 0.5f);
    vec3 o1 = v3scale(v3add(u, v3neg(v)), w * 0.5f);
    vec3 o2 = v3scale(v3add(v3neg(u), v3neg(v)), w * 0.5f);
    vec3 o3 = v3scale(v3add(v3neg(u), v), w * 0.5f);
    vec3 A[4] = {v3add(a, o0), v3add(a, o1), v3add(a, o2), v3add(a, o3)};
    vec3 B[4] = {v3add(b, o0), v3add(b, o1), v3add(b, o2), v3add(b, o3)};
    for (int i = 0; i < 4; i++) {
        int j = (i + 1) & 3;
        cd_quad(m, A[j], A[i], B[i], B[j], col);
    }
}

void cd_mesh_bounds(const cd_mesh* m, vec3* out_min, vec3* out_max) {
    vec3 mn = v3(1e30f, 1e30f, 1e30f), mx = v3(-1e30f, -1e30f, -1e30f);
    for (int i = 0; i < m->count; i++) {
        vec3 p = v3(m->v[i].px, m->v[i].py, m->v[i].pz);
        mn = v3(fminf(mn.x, p.x), fminf(mn.y, p.y), fminf(mn.z, p.z));
        mx = v3(fmaxf(mx.x, p.x), fmaxf(mx.y, p.y), fmaxf(mx.z, p.z));
    }
    *out_min = mn;
    *out_max = mx;
}
