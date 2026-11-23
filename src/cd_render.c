#include "cd_render.h"
#ifndef CD_GL_DIRECT
#include "cd_android.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ATLAS_SIZE 512
#define GL_ATLAS_UNIT 0
// Glyph cells are 32x32 texels (8x8 font at 4x) laid out 16 per row, 6 rows.
// cd_font glyphs only occupy the top 7 of their 12 declared rows, so the cell
// is cropped to 8 rows; that keeps text metrics tight and legible.
#define GLYPH_W 32
#define GLYPH_H 32
#define GLYPH_COLS 16
#define GLYPH_SCALE (GLYPH_W / 8)
#define ATLAS_GLOW_X 8
#define ATLAS_GLOW_Y 300
#define ATLAS_GREY_X 80
#define ATLAS_GREY_Y 300

// ---------------------------------------------------------------- shaders
static const char* VS_NEON =
    "attribute vec3 aPos;\n"
    "attribute vec3 aNrm;\n"
    "attribute vec4 aCol;\n"
    "uniform mat4 uMVP;\n"
    "uniform mat4 uModel;\n"
    "varying vec3 vN;\n"
    "varying vec4 vC;\n"
    "varying vec3 vW;\n"
    "void main(){\n"
    "  vec4 w = uModel * vec4(aPos,1.0);\n"
    "  vW = w.xyz;\n"
    "  vN = normalize((uModel * vec4(aNrm,0.0)).xyz);\n"
    "  vC = aCol;\n"
    "  gl_Position = uMVP * vec4(aPos,1.0);\n"
    "}\n";

static const char* FS_NEON =
    "precision mediump float;\n"
    "varying vec3 vN;\n"
    "varying vec4 vC;\n"
    "varying vec3 vW;\n"
    "uniform vec3 uCam;\n"
    "uniform float uTime;\n"
    "uniform vec2 uFog;\n"     // x = fog start, y = fog end
    "uniform vec4 uTint;\n"
    "void main(){\n"
    "  vec3 n = normalize(vN);\n"
    "  // double sided: face the normal toward the viewer\n"
    "  if (dot(n, uCam - vW) < 0.0) n = -n;\n"
    "  vec3 L1 = normalize(vec3(0.35, 0.90, 0.25));\n"
    "  vec3 L2 = normalize(vec3(-0.70, 0.35, -0.55));\n"
    "  vec3 C1 = vec3(1.00, 0.45, 0.85);\n"   // magenta key light
    "  vec3 C2 = vec3(0.15, 0.90, 1.00);\n"   // teal rim light
    "  float d1 = max(dot(n, L1), 0.0);\n"
    "  float d2 = max(dot(n, L2), 0.0);\n"
    "  vec3 amb = vec3(0.42, 0.34, 0.62);\n"
    "  vec3 base = vC.rgb;\n"
    "  float e = clamp(vC.a, 0.0, 1.0);\n"
    "  vec3 lit = base * (amb + C1 * (d1 * 1.35) + C2 * (d2 * 0.85));\n"
    "  float fres = pow(1.0 - max(dot(n, normalize(uCam - vW)), 0.0), 2.5);\n"
    "  vec3 col = mix(lit, base * 2.30, e);\n"
    "  col += base * e * (0.30 + 0.10 * sin(uTime * 3.0));\n"
    "  col += vec3(0.16, 0.60, 0.75) * fres * 0.85;\n"
    "  col += base * 0.16;\n"
    "  col *= uTint.rgb;\n"
    "  float dist = length(vW - uCam);\n"
    "  float fog = smoothstep(uFog.x, uFog.y, dist);\n"
    "  vec3 fogc = mix(vec3(0.10, 0.03, 0.22), vec3(0.30, 0.05, 0.35),\n"
    "                  clamp(vW.y * 0.02, 0.0, 1.0));\n"
    "  col = mix(col, fogc, fog * 0.92);\n"
    "  gl_FragColor = vec4(col, 1.0);\n"
    "}\n";

static const char* VS_SKY =
    "attribute vec3 aPos;\n"
    "attribute vec3 aNrm;\n"
    "attribute vec4 aCol;\n"
    "uniform mat4 uMVP;\n"
    "uniform mat4 uModel;\n"
    "varying vec3 vN;\n"
    "varying vec4 vC;\n"
    "varying vec3 vW;\n"
    "void main(){\n"
    "  vec4 w = uModel * vec4(aPos,1.0);\n"
    "  vW = w.xyz;\n"
    "  vN = normalize((uModel * vec4(aNrm,0.0)).xyz);\n"
    "  vC = aCol;\n"
    "  gl_Position = uMVP * vec4(aPos,1.0);\n"
    "}\n";

static const char* FS_SKY =
    "precision mediump float;\n"
    "varying vec3 vN;\n"
    "varying vec4 vC;\n"
    "varying vec3 vW;\n"
    "uniform vec3 uCam;\n"
    "uniform float uTime;\n"
    "uniform vec2 uFog;\n"
    "uniform vec4 uTint;\n"
    "void main(){\n"
    "  vec3 dir = normalize(vW - uCam);\n"
    "  float h = clamp(dir.y, -1.0, 1.0);\n"
    "  vec3 top = vec3(0.04, 0.01, 0.13);\n"
    "  vec3 mid = vec3(0.36, 0.05, 0.38);\n"
    "  vec3 hor = vec3(1.00, 0.34, 0.42);\n"
    "  vec3 col = mix(hor, mid, smoothstep(-0.02, 0.26, h));\n"
    "  col = mix(col, top, smoothstep(0.22, 0.95, h));\n"
    "  // synthwave sun: a disc with horizontal scanline bands\n"
    "  vec2 sp = dir.xz / max(0.18, abs(dir.y) + 0.62);\n"
    "  float r = length(sp);\n"
    "  float sun = smoothstep(0.50, 0.44, r);\n"
    "  float band = step(0.0, sin(dir.y * 70.0 + 1.2));\n"
    "  float stripes = mix(1.0, band, smoothstep(-0.10, -0.30, dir.y));\n"
    "  vec3 suncol = mix(vec3(1.00, 0.88, 0.30), vec3(1.00, 0.18, 0.55),\n"
    "                    clamp(0.55 - dir.y * 1.6, 0.0, 1.0));\n"
    "  col = mix(col, suncol, sun * stripes * 0.95);\n"
    "  col += suncol * 0.10 * sun;\n"
    "  // horizon glow band\n"
    "  col += vec3(0.55, 0.10, 0.35) * exp(-abs(h) * 9.0) * 0.55;\n"
    "  // distant city haze\n"
    "  col += vec3(0.10, 0.25, 0.45) * smoothstep(0.05, -0.25, h) * 0.25;\n"
    "  col += vC.rgb * vC.a * 0.9;\n"
    "  gl_FragColor = vec4(col, 1.0);\n"
    "}\n";

static const char* VS_UI =
    "attribute vec2 aPos;\n"
    "attribute vec2 aUV;\n"
    "attribute vec4 aCol;\n"
    "uniform mat4 uMVP;\n"
    "varying vec2 vUV;\n"
    "varying vec4 vC;\n"
    "void main(){\n"
    "  vUV = aUV;\n"
    "  vC = aCol;\n"
    "  gl_Position = uMVP * vec4(aPos, 0.0, 1.0);\n"
    "}\n";

static const char* FS_UI =
    "precision mediump float;\n"
    "varying vec2 vUV;\n"
    "varying vec4 vC;\n"
    "uniform sampler2D uAtlas;\n"
    "uniform float uTime;\n"
    "void main(){\n"
    "  vec4 t = texture2D(uAtlas, vUV);\n"
    "  // uUV.x >= 1 means: solid shape, texture gives coverage in .a only\n"
    "  float a = vC.a * t.a;\n"
    "  if (a < 0.04) discard;\n"
    "  vec3 c = vC.rgb * (0.85 + 0.55 * t.r);\n"
    "  gl_FragColor = vec4(c, a);\n"
    "}\n";

typedef struct { float x, y, u, v, r, g, b, a; } ui_vertex;

static GLuint make_program(const char* vs_src, const char* fs_src, const char* tag) {
    GLuint vs = CD_GL(glCreateShader)(GL_VERTEX_SHADER);
    CD_GL(glShaderSource)(vs, 1, &vs_src, NULL);
    CD_GL(glCompileShader)(vs);
    GLint ok = 0;
    CD_GL(glGetShaderiv)(vs, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];
        CD_GL(glGetShaderInfoLog)(vs, sizeof(log), NULL, log);
        LOGI("shader %s VS failed: %s", tag, log);
        return 0;
    }
    GLuint fs = CD_GL(glCreateShader)(GL_FRAGMENT_SHADER);
    CD_GL(glShaderSource)(fs, 1, &fs_src, NULL);
    CD_GL(glCompileShader)(fs);
    CD_GL(glGetShaderiv)(fs, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];
        CD_GL(glGetShaderInfoLog)(fs, sizeof(log), NULL, log);
        LOGI("shader %s FS failed: %s", tag, log);
        return 0;
    }
    GLuint p = CD_GL(glCreateProgram)();
    CD_GL(glAttachShader)(p, vs);
    CD_GL(glAttachShader)(p, fs);
    CD_GL(glLinkProgram)(p);
    CD_GL(glGetProgramiv)(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024];
        CD_GL(glGetProgramInfoLog)(p, sizeof(log), NULL, log);
        LOGI("program %s link failed: %s", tag, log);
        return 0;
    }
    CD_GL(glDeleteShader)(vs);
    CD_GL(glDeleteShader)(fs);
    LOGI("shader %s ready (id %u)", tag, p);
    return p;
}

// ------------------------------------------------------------------- atlas
static unsigned char* g_atlas_pixels;

static void atlas_draw_rect(int x, int y, int w, int h) {
    for (int j = 0; j < h; j++)
        for (int i = 0; i < w; i++) {
            int px = x + i, py = y + j;
            unsigned char* p = &g_atlas_pixels[(py * ATLAS_SIZE + px) * 4];
            p[0] = 255; p[1] = 255; p[2] = 255; p[3] = 255;
        }
}

static void atlas_build(void) {
    g_atlas_pixels = (unsigned char*)calloc((size_t)ATLAS_SIZE * ATLAS_SIZE * 4, 1);
    // Slot 0: solid texel (white, full alpha)
    atlas_draw_rect(0, 0, 2, 2);
    // Slots 1..: ASCII glyphs, 32x48 texels each (4x supersampled 8x12 font)
    for (int c = 0; c < 96; c++) {
        int gx = (c % GLYPH_COLS) * GLYPH_W;
        int gy = (c / GLYPH_COLS) * GLYPH_H;
        for (int row = 0; row < 8; row++) {
            unsigned char bits = cd_font_shift[c][row];
            if (!bits) continue;
            for (int col = 0; col < 8; col++) {
                if (bits & (0x80 >> col))
                    atlas_draw_rect(gx + col * GLYPH_SCALE, gy + row * GLYPH_SCALE, GLYPH_SCALE, GLYPH_SCALE);
            }
        }
    }
    // Soft radial dot for particles/glows (free area below the glyph rows)
    for (int j = 0; j < 64; j++) {
        for (int i = 0; i < 64; i++) {
            float dx = (i - 31.5f) / 32.f, dy = (j - 31.5f) / 32.f;
            float d = sqrtf(dx * dx + dy * dy);
            float a = clampf(1.f - d, 0.f, 1.f);
            a = a * a;
            unsigned char* p = &g_atlas_pixels[((ATLAS_GLOW_Y + j) * ATLAS_SIZE + ATLAS_GLOW_X + i) * 4];
            p[0] = (unsigned char)(255 * a);
            p[1] = (unsigned char)(255 * a);
            p[2] = (unsigned char)(255 * a);
            p[3] = (unsigned char)(255 * a);
        }
    }
    // Neutral grey fill for UI quads
    for (int j = 0; j < 8; j++)
        for (int i = 0; i < 8; i++) {
            unsigned char* p = &g_atlas_pixels[((ATLAS_GREY_Y + j) * ATLAS_SIZE + ATLAS_GREY_X + i) * 4];
            p[0] = 128; p[1] = 128; p[2] = 128; p[3] = 255;
        }
}

// UV helpers ---------------------------------------------------------------
void cd_ui_glyph_uv(char c, float* u0, float* v0, float* u1, float* v1) {
    int idx = (unsigned char)c - 32;
    if (idx < 0 || idx > 95) idx = 0;
    int gx = (idx % GLYPH_COLS) * GLYPH_W;
    int gy = (idx / GLYPH_COLS) * GLYPH_H;
    const float s = 1.f / (float)ATLAS_SIZE;
    *u0 = (float)gx * s;
    *v0 = (float)gy * s;
    *u1 = (float)(gx + GLYPH_W) * s;
    *v1 = (float)(gy + GLYPH_H) * s;
}

static void uv_solid(float* u0, float* v0, float* u1, float* v1) {
    const float s = 1.f / (float)ATLAS_SIZE;
    *u0 = 0.5f * s; *v0 = 0.5f * s;
    *u1 = 1.5f * s; *v1 = 1.5f * s;
}

static void uv_glow(float* u0, float* v0, float* u1, float* v1) {
    const float s = 1.f / (float)ATLAS_SIZE;
    *u0 = (float)(ATLAS_GLOW_X + 0.5f) * s; *v0 = (float)(ATLAS_GLOW_Y + 0.5f) * s;
    *u1 = (float)(ATLAS_GLOW_X + 63.5f) * s; *v1 = (float)(ATLAS_GLOW_Y + 63.5f) * s;
}

// Texture mode encoding shared with the UI helpers: 0 = solid, 1 = glow disc,
// anything >= 1000 is a glyph code (1000 + ASCII).
static void uv_for_mode(float mode, float* u0, float* v0, float* u1, float* v1) {
    if (mode >= 1000.f) cd_ui_glyph_uv((char)(int)(mode - 1000.f + 0.5f), u0, v0, u1, v1);
    else if (mode > 0.5f) uv_glow(u0, v0, u1, v1);
    else uv_solid(u0, v0, u1, v1);
}

// ------------------------------------------------------------------ gpu mesh
void cd_gpumesh_upload(cd_gpumesh* g, const cd_mesh* m) {
    if (!g->vbo) {
        CD_GL(glGenBuffers)(1, &g->vbo);
        g->cap = 0;
    }
    if (m->count > g->cap) {
        CD_GL(glBindBuffer)(GL_ARRAY_BUFFER, g->vbo);
        CD_GL(glBufferData)(GL_ARRAY_BUFFER, (GLsizeiptr)(m->count * sizeof(cd_vertex)), m->v, GL_STATIC_DRAW);
        g->cap = m->count;
    } else if (m->count > 0) {
        CD_GL(glBindBuffer)(GL_ARRAY_BUFFER, g->vbo);
        CD_GL(glBufferSubData)(GL_ARRAY_BUFFER, 0, (GLsizeiptr)(m->count * sizeof(cd_vertex)), m->v);
    }
    g->count = m->count;
}

void cd_gpumesh_free(cd_gpumesh* g) {
    if (g->vbo) CD_GL(glDeleteBuffers)(1, &g->vbo);
    g->vbo = 0;
    g->count = g->cap = 0;
}

// ------------------------------------------------------------------- setup
int cd_render_init(cd_renderer* r, int w, int h) {
    memset(r, 0, sizeof(*r));
    r->prog[CD_SHADER_NEON] = make_program(VS_NEON, FS_NEON, "neon");
    r->prog[CD_SHADER_SKY] = make_program(VS_SKY, FS_SKY, "sky");
    r->prog[CD_SHADER_UI] = make_program(VS_UI, FS_UI, "ui");
    if (!r->prog[0] || !r->prog[1] || !r->prog[2]) {
        LOGI("RENDER: shader compilation failed");
        return 0;
    }
    for (int i = 0; i < 3; i++) {
        GLuint p = r->prog[i];
        r->uni[i].u_mvp = CD_GL(glGetUniformLocation)(p, "uMVP");
        r->uni[i].u_model = CD_GL(glGetUniformLocation)(p, "uModel");
        r->uni[i].u_normal = CD_GL(glGetUniformLocation)(p, "uNormal");
        r->uni[i].u_time = CD_GL(glGetUniformLocation)(p, "uTime");
        r->uni[i].u_fog = CD_GL(glGetUniformLocation)(p, "uFog");
        r->uni[i].u_cam = CD_GL(glGetUniformLocation)(p, "uCam");
        r->uni[i].u_tint = CD_GL(glGetUniformLocation)(p, "uTint");
        r->uni[i].u_focus = CD_GL(glGetUniformLocation)(p, "uFocus");
    }
    // atlas texture
    atlas_build();
    CD_GL(glGenTextures)(1, &r->atlas);
    CD_GL(glBindTexture)(GL_TEXTURE_2D, r->atlas);
    CD_GL(glTexImage2D)(GL_TEXTURE_2D, 0, GL_RGBA, ATLAS_SIZE, ATLAS_SIZE, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                        g_atlas_pixels);
    CD_GL(glTexParameteri)(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    CD_GL(glTexParameteri)(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    CD_GL(glTexParameteri)(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    CD_GL(glTexParameteri)(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    free(g_atlas_pixels);
    g_atlas_pixels = NULL;

    CD_GL(glEnable)(GL_DEPTH_TEST);
    CD_GL(glDepthFunc)(GL_LEQUAL);
    CD_GL(glEnable)(GL_CULL_FACE);
    CD_GL(glCullFace)(GL_BACK);
    CD_GL(glDisable)(GL_BLEND);
    CD_GL(glClearColor)(0.05f, 0.02f, 0.12f, 1.f);
    cd_render_resize(r, w, h);
    LOGI("RENDER: init %dx%d ok", w, h);
    return 1;
}

void cd_render_resize(cd_renderer* r, int w, int h) {
    r->w = w > 0 ? w : 1;
    r->h = h > 0 ? h : 1;
    r->aspect = (float)r->w / (float)r->h;
    CD_GL(glViewport)(0, 0, r->w, r->h);
}

void cd_render_shutdown(cd_renderer* r) {
    for (int i = 0; i < 3; i++)
        if (r->prog[i]) CD_GL(glDeleteProgram)(r->prog[i]);
    cd_gpumesh_free(&r->dyn);
    cd_gpumesh_free(&r->ui);
}

void cd_render_begin_world(cd_renderer* r, vec3 cam_pos, mat4 view, mat4 proj, float time) {
    r->view = view;
    r->proj = proj;
    r->cam_pos = cam_pos;
    r->time = time;
    CD_GL(glEnable)(GL_DEPTH_TEST);
    CD_GL(glDepthMask)(GL_TRUE);
    // Procedural low-poly meshes are rendered double sided: trivial extra cost
    // at these poly counts and it keeps every generated asset watertight.
    CD_GL(glDisable)(GL_CULL_FACE);
    CD_GL(glDisable)(GL_BLEND);
    CD_GL(glClear)(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

static void bind_attribs(void) {
    // layout: pos(3) nrm(3) col(4)
    CD_GL(glEnableVertexAttribArray)(0);
    CD_GL(glVertexAttribPointer)(0, 3, GL_FLOAT, GL_FALSE, sizeof(cd_vertex), (const void*)0);
    CD_GL(glEnableVertexAttribArray)(1);
    CD_GL(glVertexAttribPointer)(1, 3, GL_FLOAT, GL_FALSE, sizeof(cd_vertex), (const void*)(3 * sizeof(float)));
    CD_GL(glEnableVertexAttribArray)(2);
    CD_GL(glVertexAttribPointer)(2, 4, GL_FLOAT, GL_FALSE, sizeof(cd_vertex), (const void*)(6 * sizeof(float)));
}

void cd_render_draw(cd_renderer* r, int shader, mat4 model, const cd_gpumesh* g, vec4 tint, float focus) {
    if (!g || !g->count) return;
    GLuint p = r->prog[shader];
    CD_GL(glUseProgram)(p);
    mat4 mvp = mat4_mul(r->proj, mat4_mul(r->view, model));
    CD_GL(glUniformMatrix4fv)(r->uni[shader].u_mvp, 1, GL_FALSE, mvp.m);
    if (r->uni[shader].u_model >= 0) CD_GL(glUniformMatrix4fv)(r->uni[shader].u_model, 1, GL_FALSE, model.m);
    if (r->uni[shader].u_time >= 0) CD_GL(glUniform1f)(r->uni[shader].u_time, r->time);
    if (r->uni[shader].u_cam >= 0) CD_GL(glUniform3f)(r->uni[shader].u_cam, r->cam_pos.x, r->cam_pos.y, r->cam_pos.z);
    if (r->uni[shader].u_fog >= 0) CD_GL(glUniform2f)(r->uni[shader].u_fog, focus, 150.f + focus);
    if (r->uni[shader].u_tint >= 0) CD_GL(glUniform4f)(r->uni[shader].u_tint, tint.x, tint.y, tint.z, tint.w);
    CD_GL(glBindBuffer)(GL_ARRAY_BUFFER, g->vbo);
    bind_attribs();
    CD_GL(glDrawArrays)(GL_TRIANGLES, 0, g->count);
}

void cd_render_sky(cd_renderer* r) {
    // Full-screen sky dome drawn with depth writes off, using the dyn buffer.
    // Reuse neon vertex layout with a huge inverted box centred on the camera.
    static cd_mesh dome;
    static int built = 0;
    if (!built) {
        cd_mesh_init(&dome);
        float e = 400.f;
        vec4 c = v4(0, 0, 0, 0);
        vec3 v[8] = {v3(-e, -e, -e), v3(e, -e, -e), v3(e, e, -e), v3(-e, e, -e),
                     v3(-e, -e, e), v3(e, -e, e), v3(e, e, e), v3(-e, e, e)};
        int f[6][4] = {{4, 5, 6, 7}, {1, 0, 3, 2}, {5, 1, 2, 6}, {0, 4, 7, 3}, {3, 7, 6, 2}, {0, 1, 5, 4}};
        for (int i = 0; i < 6; i++) cd_quad(&dome, v[f[i][0]], v[f[i][1]], v[f[i][2]], v[f[i][3]], c);
        built = 1;
    }
    mat4 model = mat4_translate(v3(r->cam_pos.x, 0.f, r->cam_pos.z));
    mat4 mvp = mat4_mul(r->proj, mat4_mul(r->view, model));
    GLuint p = r->prog[CD_SHADER_SKY];
    CD_GL(glUseProgram)(p);
    CD_GL(glUniformMatrix4fv)(r->uni[CD_SHADER_SKY].u_mvp, 1, GL_FALSE, mvp.m);
    CD_GL(glUniformMatrix4fv)(r->uni[CD_SHADER_SKY].u_model, 1, GL_FALSE, model.m);
    CD_GL(glUniform1f)(r->uni[CD_SHADER_SKY].u_time, r->time);
    CD_GL(glUniform3f)(r->uni[CD_SHADER_SKY].u_cam, r->cam_pos.x, r->cam_pos.y, r->cam_pos.z);
    CD_GL(glDepthMask)(GL_FALSE);
    CD_GL(glDisable)(GL_CULL_FACE);
    static GLuint dome_vbo = 0;
    static int dome_cap = 0;
    if (!dome_vbo) CD_GL(glGenBuffers)(1, &dome_vbo);
    if (dome.count > dome_cap) {
        CD_GL(glBindBuffer)(GL_ARRAY_BUFFER, dome_vbo);
        CD_GL(glBufferData)(GL_ARRAY_BUFFER, dome.count * sizeof(cd_vertex), dome.v, GL_STATIC_DRAW);
        dome_cap = dome.count;
    }
    CD_GL(glBindBuffer)(GL_ARRAY_BUFFER, dome_vbo);
    bind_attribs();
    CD_GL(glDrawArrays)(GL_TRIANGLES, 0, dome.count);
    CD_GL(glEnable)(GL_CULL_FACE);
    CD_GL(glDepthMask)(GL_TRUE);
}

// ---------------------------------------------------------------------- UI
void cd_render_begin_ui(cd_renderer* r, int sw, int sh) {
    CD_GL(glDisable)(GL_DEPTH_TEST);
    CD_GL(glDisable)(GL_CULL_FACE);
    CD_GL(glEnable)(GL_BLEND);
    CD_GL(glBlendFunc)(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    CD_GL(glDepthMask)(GL_FALSE);
    GLuint p = r->prog[CD_SHADER_UI];
    CD_GL(glUseProgram)(p);
    mat4 proj = mat4_ortho(0.f, (float)sw, (float)sh, 0.f, -1.f, 1.f);
    CD_GL(glUniformMatrix4fv)(r->uni[CD_SHADER_UI].u_mvp, 1, GL_FALSE, proj.m);
    CD_GL(glUniform1f)(r->uni[CD_SHADER_UI].u_time, r->time);
    CD_GL(glActiveTexture)(GL_TEXTURE0);
    CD_GL(glBindTexture)(GL_TEXTURE_2D, r->atlas);
    CD_GL(glUniform1i)(r->uni[CD_SHADER_UI].u_focus, GL_ATLAS_UNIT);
    if (!r->ui.vbo) CD_GL(glGenBuffers)(1, &r->ui.vbo);
    if (r->ui.cap < 4096 * 4) {
        CD_GL(glBindBuffer)(GL_ARRAY_BUFFER, r->ui.vbo);
        CD_GL(glBufferData)(GL_ARRAY_BUFFER, (GLsizeiptr)(4096 * 6 * sizeof(ui_vertex)), NULL, GL_DYNAMIC_DRAW);
        r->ui.cap = 4096 * 6;
    }
    r->ui.count = 0;
}

// We accumulate directly into a static staging buffer, then upload on flush.
static ui_vertex g_ui[4096 * 6];
static int g_ui_count = 0;

static void ui_vert(cd_renderer* r, float x, float y, float u, float v, vec4 c) {
    (void)r;
    if (g_ui_count >= 4096 * 6) return;
    ui_vertex* o = &g_ui[g_ui_count++];
    o->x = x; o->y = y; o->u = u; o->v = v;
    o->r = c.x; o->g = c.y; o->b = c.z; o->a = c.w;
}

void cd_ui_flush(cd_renderer* r) {
    if (!g_ui_count) return;
    CD_GL(glBindBuffer)(GL_ARRAY_BUFFER, r->ui.vbo);
    CD_GL(glBufferSubData)(GL_ARRAY_BUFFER, 0, (GLsizeiptr)(g_ui_count * sizeof(ui_vertex)), g_ui);
    CD_GL(glEnableVertexAttribArray)(0);
    CD_GL(glVertexAttribPointer)(0, 2, GL_FLOAT, GL_FALSE, sizeof(ui_vertex), (const void*)0);
    CD_GL(glEnableVertexAttribArray)(1);
    CD_GL(glVertexAttribPointer)(1, 2, GL_FLOAT, GL_FALSE, sizeof(ui_vertex), (const void*)(2 * sizeof(float)));
    CD_GL(glEnableVertexAttribArray)(2);
    CD_GL(glVertexAttribPointer)(2, 4, GL_FLOAT, GL_FALSE, sizeof(ui_vertex), (const void*)(4 * sizeof(float)));
    CD_GL(glDrawArrays)(GL_TRIANGLES, 0, g_ui_count);
    g_ui_count = 0;
}

void cd_render_end_ui(cd_renderer* r) {
    cd_ui_flush(r);
    CD_GL(glDisable)(GL_BLEND);
    CD_GL(glDepthMask)(GL_TRUE);
    CD_GL(glEnable)(GL_DEPTH_TEST);
    CD_GL(glEnable)(GL_CULL_FACE);
}

void cd_ui_quad(cd_renderer* r, float x, float y, float w, float h, vec4 color, float tex_mode) {
    (void)r;
    float u0, v0, u1, v1;
    uv_for_mode(tex_mode, &u0, &v0, &u1, &v1);
    ui_vert(r, x, y, u0, v0, color);
    ui_vert(r, x + w, y, u1, v0, color);
    ui_vert(r, x + w, y + h, u1, v1, color);
    ui_vert(r, x, y, u0, v0, color);
    ui_vert(r, x + w, y + h, u1, v1, color);
    ui_vert(r, x, y + h, u0, v1, color);
}

void cd_ui_quad_rot(cd_renderer* r, float cx, float cy, float w, float h, float angle, vec4 color, float tex_mode) {
    float u0, v0, u1, v1;
    uv_for_mode(tex_mode, &u0, &v0, &u1, &v1);
    float ca = cosf(angle), sa = sinf(angle);
    float hw = w * 0.5f, hh = h * 0.5f;
    float px[4] = {-hw, hw, hw, -hw};
    float py[4] = {-hh, -hh, hh, hh};
    float uu[4] = {u0, u1, u1, u0};
    float vv[4] = {v0, v0, v1, v1};
    float X[4], Y[4];
    for (int i = 0; i < 4; i++) {
        X[i] = cx + px[i] * ca - py[i] * sa;
        Y[i] = cy + px[i] * sa + py[i] * ca;
    }
    int tri[6] = {0, 1, 2, 0, 2, 3};
    for (int i = 0; i < 6; i++) {
        int k = tri[i];
        ui_vert(r, X[k], Y[k], uu[k], vv[k], color);
    }
}

void cd_ui_tri(cd_renderer* r, float x0, float y0, float x1, float y1, float x2, float y2, vec4 color) {
    float u0, v0, u1, v1;
    uv_solid(&u0, &v0, &u1, &v1);
    ui_vert(r, x0, y0, u0, v0, color);
    ui_vert(r, x1, y1, u0, v0, color);
    ui_vert(r, x2, y2, u0, v0, color);
}

void cd_ui_line(cd_renderer* r, float x0, float y0, float x1, float y1, float t, vec4 color) {
    float dx = x1 - x0, dy = y1 - y0;
    float len = sqrtf(dx * dx + dy * dy);
    if (len < 1e-4f) return;
    float nx = -dy / len * t * 0.5f, ny = dx / len * t * 0.5f;
    float u0, v0, u1, v1;
    uv_solid(&u0, &v0, &u1, &v1);
    ui_vert(r, x0 + nx, y0 + ny, u0, v0, color);
    ui_vert(r, x1 + nx, y1 + ny, u0, v0, color);
    ui_vert(r, x1 - nx, y1 - ny, u0, v0, color);
    ui_vert(r, x0 + nx, y0 + ny, u0, v0, color);
    ui_vert(r, x1 - nx, y1 - ny, u0, v0, color);
    ui_vert(r, x0 - nx, y0 - ny, u0, v0, color);
}

void cd_ui_frame(cd_renderer* r, float x, float y, float w, float h, float t, vec4 color) {
    cd_ui_quad(r, x, y, w, t, color, 0);
    cd_ui_quad(r, x, y + h - t, w, t, color, 0);
    cd_ui_quad(r, x, y, t, h, color, 0);
    cd_ui_quad(r, x + w - t, y, t, h, color, 0);
}

void cd_ui_bevel_box(cd_renderer* r, float x, float y, float w, float h, float cut, vec4 fill, vec4 edge) {
    // octagon-ish panel with cut corners
    float x1 = x + w, y1 = y + h;
    float u0, v0, u1, v1;
    uv_solid(&u0, &v0, &u1, &v1);
    float pts[8][2] = {{x + cut, y}, {x1 - cut, y}, {x1, y + cut}, {x1, y1 - cut},
                       {x1 - cut, y1}, {x + cut, y1}, {x, y1 - cut}, {x, y + cut}};
    for (int i = 0; i < 8; i++) {
        int j = (i + 1) & 7;
        ui_vert(r, x + w * 0.5f, y + h * 0.5f, u0, v0, fill);
        ui_vert(r, pts[i][0], pts[i][1], u0, v0, fill);
        ui_vert(r, pts[j][0], pts[j][1], u0, v0, fill);
    }
    if (edge.w > 0.f) {
        for (int i = 0; i < 8; i++) {
            int j = (i + 1) & 7;
            cd_ui_line(r, pts[i][0], pts[i][1], pts[j][0], pts[j][1], 3.f, edge);
        }
    }
}

void cd_ui_circle(cd_renderer* r, float x, float y, float radius, int segs, vec4 color) {
    float u0, v0, u1, v1;
    uv_solid(&u0, &v0, &u1, &v1);
    for (int i = 0; i < segs; i++) {
        float a0 = (float)i / segs * CD_TAU, a1 = (float)(i + 1) / segs * CD_TAU;
        ui_vert(r, x, y, u0, v0, color);
        ui_vert(r, x + cosf(a0) * radius, y + sinf(a0) * radius, u0, v0, color);
        ui_vert(r, x + cosf(a1) * radius, y + sinf(a1) * radius, u0, v0, color);
    }
}

void cd_ui_ring(cd_renderer* r, float x, float y, float radius, float thickness, int segs, vec4 color) {
    float u0, v0, u1, v1;
    uv_solid(&u0, &v0, &u1, &v1);
    for (int i = 0; i < segs; i++) {
        float a0 = (float)i / segs * CD_TAU, a1 = (float)(i + 1) / segs * CD_TAU;
        float c0 = cosf(a0), s0 = sinf(a0), c1 = cosf(a1), s1 = sinf(a1);
        float r0 = radius - thickness * 0.5f, r1 = radius + thickness * 0.5f;
        ui_vert(r, x + c0 * r0, y + s0 * r0, u0, v0, color);
        ui_vert(r, x + c1 * r0, y + s1 * r0, u0, v0, color);
        ui_vert(r, x + c1 * r1, y + s1 * r1, u0, v0, color);
        ui_vert(r, x + c0 * r0, y + s0 * r0, u0, v0, color);
        ui_vert(r, x + c1 * r1, y + s1 * r1, u0, v0, color);
        ui_vert(r, x + c0 * r1, y + s0 * r1, u0, v0, color);
    }
}

// Text: size = glyph cell height in pixels. The 8x8 font leaves roughly a
// column of padding on each side, hence the 0.86 advance.
static float glyph_advance(float size) { return size * 0.86f; }

float cd_ui_text_width(const char* str, float size) {
    float w = 0;
    for (const char* p = str; *p; p++) w += glyph_advance(size);
    return w;
}

void cd_ui_text(cd_renderer* r, const char* str, float x, float y, float size, vec4 color) {
    float adv = glyph_advance(size);
    float gw = size * 0.80f;
    for (const char* p = str; *p; p++) {
        char c = *p;
        if (c == ' ') { x += adv; continue; }
        cd_ui_quad(r, x, y, gw, size, color, 1000.f + (float)(unsigned char)c);
        x += adv;
    }
}

// Shrink-to-fit helper: keeps tablet text inside its panel whatever the
// aspect ratio (16:10 tablets vs 4:3 tablets).
float cd_ui_text_fit(const char* str, float size, float max_w) {
    float w = cd_ui_text_width(str, size);
    if (max_w > 1.f && w > max_w) size *= max_w / w;
    return size;
}

void cd_ui_text_fit_center(cd_renderer* r, const char* str, float cx, float y, float size,
                           float max_w, vec4 color) {
    cd_ui_text_center(r, str, cx, y, cd_ui_text_fit(str, size, max_w), color);
}

void cd_ui_text_center(cd_renderer* r, const char* str, float cx, float y, float size, vec4 color) {
    cd_ui_text(r, str, cx - cd_ui_text_width(str, size) * 0.5f, y, size, color);
}
