#ifndef CD_RENDER_H
#define CD_RENDER_H

#include "cd_gl.h"
#include "cd_math.h"
#include "cd_mesh.h"

#define CD_SHADER_NEON 0
#define CD_SHADER_SKY 1
#define CD_SHADER_UI 2

typedef struct {
    GLuint vbo;      // vertex buffer
    int count;       // vertices
    int cap;         // capacity (verts)
    int mesh_owned;
} cd_gpumesh;

typedef struct {
    int w, h;
    float aspect;
    mat4 view, proj;
    vec3 cam_pos;
    float time;
    // shader handles
    GLuint prog[3];
    struct {
        GLint u_mvp, u_model, u_normal, u_time, u_fog, u_cam, u_tint, u_focus;
    } uni[3];
    // dynamic vertex buffer for particles / sprites
    cd_gpumesh dyn;
    // ui buffer
    cd_gpumesh ui;
    // sprite texture atlas (font + effects), 512x512 RGBA
    GLuint atlas;
    int ui_bound;   // current texture bound for UI pass
} cd_renderer;

int cd_render_init(cd_renderer* r, int w, int h);
void cd_render_resize(cd_renderer* r, int w, int h);
void cd_render_shutdown(cd_renderer* r);

void cd_gpumesh_upload(cd_gpumesh* g, const cd_mesh* m);
void cd_gpumesh_free(cd_gpumesh* g);

void cd_render_begin_world(cd_renderer* r, vec3 cam_pos, mat4 view, mat4 proj, float time);
void cd_render_draw(cd_renderer* r, int shader, mat4 model, const cd_gpumesh* g, vec4 tint, float focus);
void cd_render_sky(cd_renderer* r);
void cd_render_begin_ui(cd_renderer* r, int screen_w, int screen_h);
void cd_render_end_ui(cd_renderer* r);

// ---------------------------------------------------------------------------
// UI drawing (2D, top-left origin, in pixels).
// ---------------------------------------------------------------------------
void cd_ui_quad(cd_renderer* r, float x, float y, float w, float h, vec4 color, float tex_mode);
void cd_ui_quad_rot(cd_renderer* r, float cx, float cy, float w, float h, float angle, vec4 color, float tex_mode);
void cd_ui_tri(cd_renderer* r, float x0, float y0, float x1, float y1, float x2, float y2, vec4 color);
void cd_ui_line(cd_renderer* r, float x0, float y0, float x1, float y1, float thickness, vec4 color);
void cd_ui_frame(cd_renderer* r, float x, float y, float w, float h, float t, vec4 color);
void cd_ui_bevel_box(cd_renderer* r, float x, float y, float w, float h, float cut, vec4 fill, vec4 edge);
void cd_ui_circle(cd_renderer* r, float x, float y, float radius, int segs, vec4 color);
void cd_ui_ring(cd_renderer* r, float x, float y, float radius, float thickness, int segs, vec4 color);
void cd_ui_text(cd_renderer* r, const char* str, float x, float y, float size, vec4 color);
float cd_ui_text_width(const char* str, float size);
void cd_ui_text_center(cd_renderer* r, const char* str, float cx, float y, float size, vec4 color);
float cd_ui_text_fit(const char* str, float size, float max_w);
void cd_ui_text_fit_center(cd_renderer* r, const char* str, float cx, float y, float size, float max_w, vec4 color);
void cd_ui_flush(cd_renderer* r);

// Procedural font atlas: 0..127 ASCII in 8x12 cells scaled 2x -> 16x24 texels.
void cd_ui_glyph_uv(char c, float* u0, float* v0, float* u1, float* v1);

extern const unsigned char cd_font_shift[96][12];
#endif
