// Cyber Drift — GL trace shim.
//
// This file implements the entire GLES2 subset the renderer uses, but instead
// of rasterising it records every draw call (model matrix, packed vertex data,
// tint, uniforms) to a binary trace. tools/rasterize.py then renders that trace
// in software. Result: the real engine draw path is exercised and previewed.
#include "cd_gl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Define the GL entry-point globals declared by cd_gl.h (they are plain
// function pointers in preview builds; here we point them at our mocks).
#define X(ret, fname, args) ret trace_##fname args;
CD_GL_FUNCS(X)
#undef X

void* egl_preview_get_proc(const char* name) {
#define X(ret, fname, args) if (strcmp(name, #fname) == 0) return (void*)trace_##fname;
    CD_GL_FUNCS(X)
#undef X
    return NULL;
}

static void trace_link_all(void) {
#define X(ret, fname, args) cd_##fname = (cd_pfn_##fname)trace_##fname;
    CD_GL_FUNCS(X)
#undef X
}

typedef struct {
    GLuint id;
    unsigned char* data;   // copy of the vertex buffer contents
    size_t size;
} trace_buf;

static trace_buf g_bufs[64];
static int g_nbufs;
static GLuint g_bound_array;
static GLuint g_cur_program;

static GLuint g_prog_neon, g_prog_sky, g_prog_ui;
static GLuint g_next_id = 1;
static int g_prog_bound_kind = -1;

// current state
static float g_u_mvp[16], g_u_model[16], g_u_tint[4] = {1, 1, 1, 1}, g_u_fog[2];
static float g_u_cam[3], g_u_time;
static GLint g_mvp_loc = -1, g_model_loc = -1, g_tint_loc = -1, g_cam_loc = -1, g_time_loc = -1, g_fog_loc = -1;

static int g_blend_enabled;
static int g_blend_additive;

typedef struct {
    int kind;            // 0 = neon, 1 = sky, 2 = ui
    int blend;           // 0 = opaque, 1 = alpha, 2 = additive
    float mvp[16];
    float model[16];
    float tint[4];
    float cam[3];
    float time;
    float fog[2];
    int count;
    unsigned char* verts;   // count * stride bytes (neon: 40, ui: 32)
    int stride;
    int texture;            // 1 = atlas sampled
} trace_draw;

static trace_draw* g_draws;
static int g_ndraws, g_capdraws;
static unsigned char* g_atlas;
static int g_atlas_w, g_atlas_h;

// ---------------------------------------------------------------- helpers
static trace_buf* find_buf(GLuint id) {
    for (int i = 0; i < g_nbufs; i++)
        if (g_bufs[i].id == id) return &g_bufs[i];
    return NULL;
}

static void record(int kind, int count, const void* data, int stride, int textured) {
    if (count <= 0 || !data) return;
    if (g_ndraws == g_capdraws) {
        g_capdraws = g_capdraws ? g_capdraws * 2 : 256;
        g_draws = (trace_draw*)realloc(g_draws, sizeof(trace_draw) * (size_t)g_capdraws);
    }
    trace_draw* d = &g_draws[g_ndraws++];
    memset(d, 0, sizeof(*d));
    d->kind = kind;
    d->blend = g_blend_enabled ? (g_blend_additive ? 2 : 1) : 0;
    memcpy(d->mvp, g_u_mvp, sizeof(g_u_mvp));
    memcpy(d->model, g_u_model, sizeof(g_u_model));
    memcpy(d->tint, g_u_tint, sizeof(g_u_tint));
    memcpy(d->cam, g_u_cam, sizeof(g_u_cam));
    memcpy(d->fog, g_u_fog, sizeof(g_u_fog));
    d->time = g_u_time;
    d->count = count;
    d->stride = stride;
    d->texture = textured;
    d->verts = (unsigned char*)malloc((size_t)count * stride);
    memcpy(d->verts, data, (size_t)count * stride);
}

// ------------------------------------------------------------ GL surface
#define X(ret, fname, args) cd_pfn_##fname cd_##fname = 0;
CD_GL_FUNCS(X)
#undef X

static int g_inited;
static void trace_init_once(void) {
    if (g_inited) return;
    g_inited = 1;
    trace_link_all();
}

__attribute__((constructor)) static void trace_ctor(void) { trace_link_all(); }

GLuint trace_glCreateShader(GLenum t) { (void)t; return g_next_id++; }
void trace_glShaderSource(GLuint s, GLsizei n, const GLchar* const* src, const GLint* len) { (void)s; (void)n; (void)src; (void)len; }
void trace_glCompileShader(GLuint s) { (void)s; }
void trace_glGetShaderiv(GLuint s, GLenum p, GLint* v) { (void)s; (void)p; *v = 1; }
void trace_glGetShaderInfoLog(GLuint s, GLsizei n, GLsizei* l, GLchar* b) { (void)s; (void)n; if (l) *l = 0; if (b) b[0] = 0; }

GLuint trace_glCreateProgram(void) { return g_next_id++; }
void trace_glAttachShader(GLuint p, GLuint s) { (void)p; (void)s; }
void trace_glDeleteShader(GLuint s) { (void)s; }
void trace_glDeleteProgram(GLuint p) { (void)p; }
void trace_glLinkProgram(GLuint p) {
    if (g_prog_neon == 0) { g_prog_neon = p; }
    else if (g_prog_sky == 0) { g_prog_sky = p; }
    else { g_prog_ui = p; }
}
void trace_glGetProgramiv(GLuint p, GLenum e, GLint* v) { (void)p; (void)e; *v = 1; }
void trace_glGetProgramInfoLog(GLuint p, GLsizei n, GLsizei* l, GLchar* b) { (void)p; (void)n; if (l) *l = 0; if (b) b[0] = 0; }
void trace_glUseProgram(GLuint p) {
    g_cur_program = p;
    g_prog_bound_kind = (p == g_prog_neon) ? 0 : (p == g_prog_sky ? 1 : (p == g_prog_ui ? 2 : -1));
    g_mvp_loc = -1; g_model_loc = -1; g_tint_loc = -1; g_cam_loc = -1; g_time_loc = -1; g_fog_loc = -1;
}
// Name-aware uniform locations so the trace records the right matrices.
static struct { GLuint prog; char name[48]; int loc; } g_ulocs[64];
static int g_nulocs;

GLint trace_glGetUniformLocation(GLuint p, const GLchar* name) {
    for (int i = 0; i < g_nulocs; i++)
        if (g_ulocs[i].prog == p && strcmp(g_ulocs[i].name, name) == 0) return g_ulocs[i].loc;
    int loc = (int)g_next_id++;
    g_ulocs[g_nulocs].prog = p;
    strncpy(g_ulocs[g_nulocs].name, name, 47);
    g_ulocs[g_nulocs].name[47] = 0;
    g_ulocs[g_nulocs].loc = loc;
    g_nulocs++;
    return loc;
}

static const char* uloc_name(GLint loc) {
    for (int i = 0; i < g_nulocs; i++)
        if (g_ulocs[i].loc == loc) return g_ulocs[i].name;
    return "";
}
GLint trace_glGetAttribLocation(GLuint p, const GLchar* name) { (void)p; (void)name; return 0; }

void trace_glUniformMatrix4fv(GLint loc, GLsizei n, GLboolean tr, const GLfloat* v) {
    (void)n; (void)tr;
    const char* nm = uloc_name(loc);
    if (strcmp(nm, "uModel") == 0) memcpy(g_u_model, v, 64);
    else memcpy(g_u_mvp, v, 64);
}
void trace_glUniform1f(GLint l, GLfloat v) {
    const char* nm = uloc_name(l);
    if (strcmp(nm, "uTime") == 0) g_u_time = v;
}
void trace_glUniform2f(GLint l, GLfloat a, GLfloat b) {
    const char* nm = uloc_name(l);
    if (strcmp(nm, "uFog") == 0) { g_u_fog[0] = a; g_u_fog[1] = b; }
}
void trace_glUniform3f(GLint l, GLfloat a, GLfloat b, GLfloat c) {
    const char* nm = uloc_name(l);
    if (strcmp(nm, "uCam") == 0) { g_u_cam[0] = a; g_u_cam[1] = b; g_u_cam[2] = c; }
}
void trace_glUniform4f(GLint l, GLfloat a, GLfloat b, GLfloat c, GLfloat d) {
    const char* nm = uloc_name(l);
    if (strcmp(nm, "uTint") == 0) { g_u_tint[0] = a; g_u_tint[1] = b; g_u_tint[2] = c; g_u_tint[3] = d; }
}
void trace_glUniform1i(GLint l, GLint v) { (void)l; (void)v; }

// ---- buffers
void trace_glGenBuffers(GLsizei n, GLuint* out) {
    for (GLsizei i = 0; i < n && g_nbufs < 64; i++) {
        out[i] = g_next_id++;
        g_bufs[g_nbufs].id = out[i];
        g_bufs[g_nbufs].data = NULL;
        g_bufs[g_nbufs].size = 0;
        g_nbufs++;
    }
}
void trace_glBindBuffer(GLenum target, GLuint id) { (void)target; g_bound_array = id; }
void trace_glBufferData(GLenum target, GLsizeiptr size, const void* data, GLenum usage) {
    (void)target; (void)usage;
    trace_buf* b = find_buf(g_bound_array);
    if (!b) return;
    free(b->data);
    b->data = (unsigned char*)malloc((size_t)size);
    b->size = (size_t)size;
    if (data) memcpy(b->data, data, (size_t)size);
    else memset(b->data, 0, (size_t)size);
}
void trace_glBufferSubData(GLenum target, GLintptr off, GLsizeiptr size, const void* data) {
    (void)target;
    trace_buf* b = find_buf(g_bound_array);
    if (!b || !b->data) return;
    if ((size_t)off + (size_t)size > b->size) return;
    memcpy(b->data + off, data, (size_t)size);
}
void trace_glDeleteBuffers(GLsizei n, const GLuint* ids) { (void)n; (void)ids; }
void trace_glVertexAttribPointer(GLuint i, GLint sz, GLenum t, GLboolean n, GLsizei stride, const void* ptr) {
    (void)i; (void)sz; (void)t; (void)n; (void)stride; (void)ptr;
}
void trace_glEnableVertexAttribArray(GLuint i) { (void)i; }

void trace_glDrawArrays(GLenum mode, GLint first, GLsizei count) {
    (void)mode;
    trace_buf* b = find_buf(g_bound_array);
    if (!b || !b->data) return;
    int stride = (g_prog_bound_kind == 2) ? (int)sizeof(float) * 8 : (int)sizeof(float) * 10;
    size_t off = (size_t)first * (size_t)stride;
    if (off + (size_t)count * (size_t)stride > b->size) {
        count = (GLsizei)((b->size - off) / (size_t)stride);
    }
    record(g_prog_bound_kind < 0 ? 0 : g_prog_bound_kind, count, b->data + off, stride, g_prog_bound_kind == 2);
}

// ---- misc state: recorded implicitly, nothing to do
void trace_glClear(GLbitfield m) { (void)m; }
void trace_glClearColor(GLfloat r, GLfloat g, GLfloat b, GLfloat a) { (void)r; (void)g; (void)b; (void)a; }
void trace_glEnable(GLenum c) { if (c == GL_BLEND) g_blend_enabled = 1; }
void trace_glDisable(GLenum c) { if (c == GL_BLEND) g_blend_enabled = 0; }
void trace_glBlendFunc(GLenum a, GLenum b) {
    // additive when the destination factor is ONE (engine trails/sparks)
    g_blend_additive = (b == GL_ONE);
}
void trace_glViewport(GLint x, GLint y, GLsizei w, GLsizei h) { (void)x; (void)y; (void)w; (void)h; }
void trace_glCullFace(GLenum m) { (void)m; }
void trace_glDepthMask(GLboolean f) { (void)f; }
void trace_glDepthFunc(GLenum f) { (void)f; }
GLenum trace_glGetError(void) { return 0; }
const GLubyte* trace_glGetString(GLenum n) { (void)n; return (const GLubyte*)"trace"; }
void trace_glGetIntegerv(GLenum p, GLint* v) { (void)p; if (v) v[0] = 0; }

// ---- textures: keep the atlas for UI texture lookups
void trace_glGenTextures(GLsizei n, GLuint* out) { for (GLsizei i = 0; i < n; i++) out[i] = g_next_id++; }
void trace_glBindTexture(GLenum t, GLuint id) { (void)t; (void)id; }
void trace_glActiveTexture(GLenum t) { (void)t; }
void trace_glTexParameteri(GLenum a, GLenum b, GLint c) { (void)a; (void)b; (void)c; }
void trace_glDeleteTextures(GLsizei n, const GLuint* ids) { (void)n; (void)ids; }
void trace_glTexImage2D(GLenum target, GLint level, GLint ifmt, GLsizei w, GLsizei h, GLint border,
                     GLenum fmt, GLenum type, const void* pixels) {
    (void)target; (void)level; (void)ifmt; (void)border; (void)fmt; (void)type;
    if (!pixels) return;
    g_atlas_w = w;
    g_atlas_h = h;
    size_t n = (size_t)w * (size_t)h * 4;
    if (g_atlas) free(g_atlas);
    g_atlas = (unsigned char*)malloc(n);
    memcpy(g_atlas, pixels, n);
}

// ------------------------------------------------------------- trace output
// Binary layout (little endian):
//   "CDTRACE1", int atlas_w, atlas_h, int atlas_bytes, atlas data,
//   int ndraws, then per draw:
//     int kind, int count, int stride, int texture,
//     float mvp[16], model[16], tint[4], cam[3], time, fog[2], verts[count*stride]
int cd_trace_write(const char* path) {
    FILE* f = fopen(path, "wb");
    if (!f) return 0;
    fwrite("CDTRACE1", 1, 8, f);
    fwrite(&g_atlas_w, 4, 1, f);
    fwrite(&g_atlas_h, 4, 1, f);
    int abytes = g_atlas ? g_atlas_w * g_atlas_h * 4 : 0;
    fwrite(&abytes, 4, 1, f);
    if (abytes) fwrite(g_atlas, 1, (size_t)abytes, f);
    fwrite(&g_ndraws, 4, 1, f);
    for (int i = 0; i < g_ndraws; i++) {
        trace_draw* d = &g_draws[i];
        fwrite(&d->kind, 4, 1, f);
        fwrite(&d->count, 4, 1, f);
        fwrite(&d->stride, 4, 1, f);
        fwrite(&d->texture, 4, 1, f);
        fwrite(&d->blend, 4, 1, f);
        fwrite(d->mvp, 4, 16, f);
        fwrite(d->model, 4, 16, f);
        fwrite(d->tint, 4, 4, f);
        fwrite(d->cam, 4, 3, f);
        fwrite(&d->time, 4, 1, f);
        fwrite(d->fog, 4, 2, f);
        fwrite(d->verts, 1, (size_t)d->count * (size_t)d->stride, f);
    }
    fclose(f);
    int k[3] = {0, 0, 0};
    for (int i = 0; i < g_ndraws; i++) if (g_draws[i].kind >= 0 && g_draws[i].kind < 3) k[g_draws[i].kind]++;
    printf("[gltrace] %d draw calls (neon=%d sky=%d ui=%d) -> %s\n", g_ndraws, k[0], k[1], k[2], path);
    return 1;
}
