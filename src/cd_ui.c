// Cyber Drift — HUD, menus and touch-safe control overlay.
// Layout is resolution independent: everything scales from the short edge so
// 16:10 and 4:3 tablets both get comfortable targets (>= 96 dp circles).
#include "cd_game.h"
#include <stdio.h>
#include <string.h>

static const vec4 C_TEAL = {0.20f, 1.00f, 0.88f, 1.f};
static const vec4 C_MAGENTA = {1.00f, 0.28f, 0.78f, 1.f};
static const vec4 C_VIOLET = {0.55f, 0.34f, 1.00f, 1.f};
static const vec4 C_AMBER = {1.00f, 0.72f, 0.20f, 1.f};
static const vec4 C_WHITE = {0.92f, 0.96f, 1.00f, 1.f};
static const vec4 C_DIM = {0.78f, 0.82f, 1.00f, 0.95f};
static const vec4 C_PANEL = {0.04f, 0.02f, 0.10f, 0.80f};

static void fmt_int(char* buf, long v) {
    char tmp[32];
    int n = 0;
    int neg = v < 0;
    if (neg) v = -v;
    if (v == 0) tmp[n++] = '0';
    while (v > 0 && n < 30) { tmp[n++] = (char)('0' + (v % 10)); v /= 10; }
    int k = 0;
    if (neg) buf[k++] = '-';
    while (n > 0) buf[k++] = tmp[--n];
    buf[k] = 0;
}

static void fmt_clock(char* buf, float seconds) {
    int s = (int)seconds;
    int m = s / 60;
    int r = s % 60;
    char a[16], b[8];
    fmt_int(a, m);
    fmt_int(b, r);
    int k = 0;
    for (char* p = a; *p; p++) buf[k++] = *p;
    buf[k++] = ':';
    if (r < 10) buf[k++] = '0';
    for (char* p = b; *p; p++) buf[k++] = *p;
    buf[k] = 0;
}

// ---------------------------------------------------------------------------
// Touch control geometry (shared by drawing and hit-testing)
// ---------------------------------------------------------------------------
static float s(float v, const cd_game* g) { return v * g->ui_scale; }

static void draw_joystick(cd_game* g, float cx, float cy, float radius, float knob_x, float knob_y) {
    cd_renderer* r = &g->renderer;
    cd_ui_circle(r, cx, cy, radius, 32, v4(0.06f, 0.03f, 0.15f, 0.55f));
    cd_ui_ring(r, cx, cy, radius, s(3.5f, g), 40, v4(0.25f, 1.f, 0.9f, 0.65f));
    cd_ui_ring(r, cx, cy, radius * 0.62f, s(2.f, g), 32, v4(0.6f, 0.4f, 1.f, 0.35f));
    // directional chevrons
    float ca = s(16.f, g);
    cd_ui_tri(r, cx - radius * 0.82f, cy, cx - radius * 0.82f + ca, cy - ca * 0.8f,
              cx - radius * 0.82f + ca, cy + ca * 0.8f, v4(0.3f, 1.f, 0.9f, 0.5f));
    cd_ui_tri(r, cx + radius * 0.82f, cy, cx + radius * 0.82f - ca, cy - ca * 0.8f,
              cx + radius * 0.82f - ca, cy + ca * 0.8f, v4(0.3f, 1.f, 0.9f, 0.5f));
    // knob
    cd_ui_circle(r, knob_x, knob_y, radius * 0.34f, 24, v4(0.15f, 0.95f, 0.95f, 0.85f));
    cd_ui_circle(r, knob_x, knob_y, radius * 0.20f, 20, v4(0.85f, 1.f, 1.f, 0.95f));
}

static void draw_nitro(cd_game* g, float cx, float cy, float radius) {
    cd_renderer* r = &g->renderer;
    cd_craft* c = &g->craft;
    int active = g->in_throttle > 0.15f;
    vec4 base = active ? v4(1.0f, 0.55f, 0.12f, 0.55f) : v4(0.10f, 0.06f, 0.20f, 0.55f);
    cd_ui_circle(r, cx, cy, radius, 32, base);
    cd_ui_ring(r, cx, cy, radius, s(4.f, g), 36, active ? C_AMBER : v4(0.75f, 0.45f, 1.f, 0.55f));
    // fuel arc
    float a0 = -CD_PI * 0.5f;
    float amt = c->boost;
    int segs = 28;
    for (int i = 0; i < segs; i++) {
        float t0 = (float)i / segs, t1 = (float)(i + 1) / segs;
        if (t1 > amt) break;
        float A0 = a0 + t0 * CD_TAU, A1 = a0 + t1 * CD_TAU;
        float rr = radius * 1.16f;
        cd_ui_tri(r, cx, cy, cx + cosf(A0) * rr, cy + sinf(A0) * rr,
                  cx + cosf(A1) * rr, cy + sinf(A1) * rr,
                  amt > 0.3f ? v4(1.f, 0.7f, 0.2f, 0.55f) : v4(1.f, 0.3f, 0.3f, 0.55f));
    }
    cd_ui_text_center(r, "NITRO", cx, cy - s(11.f, g), s(22.f, g), active ? C_WHITE : C_DIM);
    // lightning glyph
    float k = s(30.f, g);
    cd_ui_tri(r, cx + k * 0.15f, cy - k * 0.55f, cx - k * 0.45f, cy + k * 0.15f, cx + k * 0.05f, cy + k * 0.15f,
              C_AMBER);
    cd_ui_tri(r, cx + k * 0.05f, cy + k * 0.15f, cx - k * 0.15f, cy + k * 0.15f, cx + k * 0.15f, cy + k * 0.55f,
              C_AMBER);
}

static void draw_brake(cd_game* g, float cx, float cy, float radius) {
    cd_renderer* r = &g->renderer;
    int active = g->in_brake;
    cd_ui_circle(r, cx, cy, radius, 28, active ? v4(1.f, 0.2f, 0.35f, 0.55f) : v4(0.10f, 0.06f, 0.20f, 0.5f));
    cd_ui_ring(r, cx, cy, radius, s(3.5f, g), 32, active ? v4(1.f, 0.5f, 0.6f, 0.9f) : v4(0.7f, 0.5f, 1.f, 0.5f));
    float k = s(18.f, g);
    cd_ui_quad(r, cx - k * 0.55f, cy - k * 0.35f, k * 1.1f, k * 0.7f, active ? C_WHITE : C_DIM, 0);
}

static void draw_pause_button(cd_game* g, float x, float y, float radius) {
    cd_renderer* r = &g->renderer;
    cd_ui_circle(r, x, y, radius, 26, v4(0.07f, 0.04f, 0.16f, 0.7f));
    cd_ui_ring(r, x, y, radius, s(3.f, g), 30, v4(0.35f, 1.f, 0.9f, 0.7f));
    float k = radius * 0.34f;
    cd_ui_quad(r, x - k * 0.9f, y - k, k * 0.62f, k * 2.f, C_TEAL, 0);
    cd_ui_quad(r, x + k * 0.28f, y - k, k * 0.62f, k * 2.f, C_TEAL, 0);
}

// ---------------------------------------------------------------------------
// In-game HUD
// ---------------------------------------------------------------------------
void cd_ui_hud(cd_game* g) {
    cd_renderer* r = &g->renderer;
    cd_craft* c = &g->craft;
    float W = (float)r->w, H = (float)r->h;

    // ---------- top bar
    float pad = s(22.f, g);
    float bar_h = s(74.f, g);
    cd_ui_bevel_box(r, pad, pad, W * 0.62f, bar_h, s(16.f, g), C_PANEL, v4(0, 0, 0, 0));

    char buf[64];
    // SCORE
    cd_ui_text(r, "SCORE", pad + s(18.f, g), pad + s(8.f, g), s(20.f, g), C_DIM);
    fmt_int(buf, g->score);
    cd_ui_text(r, buf, pad + s(18.f, g), pad + s(26.f, g), s(40.f, g), C_TEAL);
    cd_ui_text(r, buf, pad + s(19.f, g), pad + s(27.f, g), s(40.f, g), C_WHITE);

    // SPEED
    float sx = pad + W * 0.155f;
    cd_ui_text(r, "SPEED", sx, pad + s(8.f, g), s(20.f, g), C_DIM);
    fmt_int(buf, (long)g->speed_display);
    int n = (int)strlen(buf);
    char sbuf[32];
    memcpy(sbuf, buf, (size_t)n);
    sbuf[n] = ' '; sbuf[n + 1] = 'K'; sbuf[n + 2] = 'M'; sbuf[n + 3] = 'H'; sbuf[n + 4] = 0;
    cd_ui_text(r, sbuf, sx, pad + s(28.f, g), s(32.f, g), c->boosting > 0.4f ? C_AMBER : C_TEAL);

    // ENERGY meter
    float ex = pad + W * 0.345f;
    float ew = W * 0.235f;
    cd_ui_text(r, "ENERGY", ex, pad + s(8.f, g), s(20.f, g), C_DIM);
    float eh = s(16.f, g);
    cd_ui_quad(r, ex, pad + s(38.f, g), ew, eh, v4(0.12f, 0.08f, 0.24f, 0.9f), 0);
    float frac = clampf((float)g->energy / 100.f, 0.f, 1.f);
    vec4 ecol = frac > 0.5f ? C_TEAL : (frac > 0.22f ? C_AMBER : v4(1.f, 0.3f, 0.35f, 1.f));
    cd_ui_quad(r, ex, pad + s(38.f, g), ew * frac, eh, ecol, 0);
    cd_ui_frame(r, ex, pad + s(38.f, g), ew, eh, s(2.f, g), v4(0.5f, 0.7f, 1.f, 0.8f));
    // shields
    for (int i = 0; i < g->shields; i++) {
        cd_ui_circle(r, ex + ew - s(12.f, g) - (float)i * s(26.f, g), pad + s(66.f, g), s(9.f, g), 12,
                     v4(0.45f, 0.8f, 1.f, 0.95f));
    }

    // ---------- right side: distance + pause
    float px = W - pad - s(46.f, g);
    float py = pad + s(46.f, g);
    draw_pause_button(g, px, py, s(46.f, g));
    g->pause_btn_x = px; g->pause_btn_y = py; g->pause_btn_r = s(58.f, g);

    fmt_int(buf, (long)g->distance);
    char dbuf[40];
    int dn = (int)strlen(buf);
    memcpy(dbuf, buf, (size_t)dn);
    dbuf[dn] = ' '; dbuf[dn + 1] = 'M'; dbuf[dn + 2] = 0;
    cd_ui_text(r, dbuf, px - s(18.f, g) - cd_ui_text_width(dbuf, s(26.f, g)), py - s(13.f, g), s(26.f, g), C_VIOLET);

    // combo
    if (g->combo >= 5) {
        fmt_int(buf, g->combo);
        char cbuf[40];
        int cn = (int)strlen(buf);
        memcpy(cbuf, buf, (size_t)cn);
        cbuf[cn] = 'X'; cbuf[cn + 1] = ' '; cbuf[cn + 2] = 'C'; cbuf[cn + 3] = 'O'; cbuf[cn + 4] = 'M';
        cbuf[cn + 5] = 'B'; cbuf[cn + 6] = 'O'; cbuf[cn + 7] = 0;
        float pulse = 0.7f + 0.3f * sinf(g->menu_time * 10.f);
        cd_ui_text_center(r, cbuf, W * 0.5f, pad * 1.1f, s(30.f, g), v4(1.f, 0.5f * pulse, 0.85f, pulse));
    }

    // ---------- bottom control zones (geometry published for touch handling)
    {
        float jx = H * 0.20f + pad * 0.6f;
        float jy = H - H * 0.20f - pad * 0.2f;
        float jr = H * 0.155f;
        if (jr < s(92.f, g)) jr = s(92.f, g);
        if (jr > H * 0.20f) jr = H * 0.20f;
        float nx = W - H * 0.20f - pad * 0.6f;
        float ny = jy;
        float nr = jr * 0.82f;
        g->joy_cx = jx; g->joy_cy = jy; g->joy_r = jr;
        g->nitro_cx = nx; g->nitro_cy = ny; g->nitro_r = nr;
        g->brake_cx = nx - nr * 1.55f; g->brake_cy = ny + nr * 0.35f; g->brake_r = jr * 0.46f;
    }
    if (g->state == CD_STATE_PLAY) {
        float jx = g->joy_cx, jy = g->joy_cy, jr = g->joy_r;
        float knob_x = jx + clampf(g->joy_dx, -1.f, 1.f) * jr * 0.58f;
        float knob_y = jy + clampf(g->joy_dy, -1.f, 1.f) * jr * 0.58f;
        draw_joystick(g, jx, jy, jr, knob_x, knob_y);

        draw_nitro(g, g->nitro_cx, g->nitro_cy, g->nitro_r);
        draw_brake(g, g->brake_cx, g->brake_cy, g->brake_r);
    }

    // ---------- center messages
    if (g->state == CD_STATE_PAUSE) {
        cd_ui_quad(r, 0, 0, W, H, v4(0.02f, 0.01f, 0.06f, 0.72f), 0);
        cd_ui_text_center(r, "PAUSED", W * 0.5f, H * 0.32f, s(84.f, g), C_TEAL);
        cd_ui_text_center(r, "TAP ANYWHERE TO RESUME", W * 0.5f, H * 0.32f + s(110.f, g), s(26.f, g), C_DIM);
        fmt_int(buf, g->score);
        cd_ui_text_center(r, buf, W * 0.5f, H * 0.32f + s(170.f, g), s(46.f, g), C_WHITE);
    }

    if (g->state == CD_STATE_GAMEOVER) {
        cd_ui_quad(r, 0, 0, W, H, v4(0.02f, 0.01f, 0.06f, 0.78f), 0);
        cd_ui_bevel_box(r, W * 0.5f - s(330.f, g), H * 0.5f - s(210.f, g), s(660.f, g), s(420.f, g),
                        s(26.f, g), v4(0.07f, 0.03f, 0.16f, 0.95f), C_MAGENTA);
        cd_ui_text_center(r, "RUN TERMINATED", W * 0.5f, H * 0.5f - s(170.f, g), s(46.f, g), C_MAGENTA);
        cd_ui_text_center(r, "SCORE", W * 0.5f, H * 0.5f - s(80.f, g), s(22.f, g), C_DIM);
        fmt_int(buf, g->score);
        cd_ui_text_center(r, buf, W * 0.5f, H * 0.5f - s(52.f, g), s(64.f, g), C_WHITE);
        char t[40];
        fmt_clock(t, g->play_time);
        cd_ui_text_center(r, t, W * 0.5f, H * 0.5f + s(30.f, g), s(28.f, g), C_VIOLET);
        fmt_int(buf, g->best);
        char b[64];
        int bn = 0;
        for (const char* p = "BEST "; *p; p++) b[bn++] = *p;
        for (const char* p = buf; *p; p++) b[bn++] = *p;
        b[bn] = 0;
        cd_ui_text_center(r, b, W * 0.5f, H * 0.5f + s(74.f, g), s(26.f, g), C_TEAL);

        // RESTART button: wide enough for its label on every aspect ratio.
        float bw = fminf(W * 0.46f, s(520.f, g)), bh = s(100.f, g);
        float bx = W * 0.5f - bw * 0.5f, by = H * 0.5f + s(112.f, g);
        cd_ui_bevel_box(r, bx, by, bw, bh, s(18.f, g), v4(0.16f, 0.55f, 0.5f, 0.9f), C_TEAL);
        cd_ui_text_fit_center(r, "TAP TO FLY AGAIN", W * 0.5f, by + bh * 0.33f, s(32.f, g),
                              bw - s(40.f, g), C_WHITE);
        g->restart_btn_x = bx; g->restart_btn_y = by;
        g->restart_btn_w = bw; g->restart_btn_h = bh;
    }

    // full screen flashes
    if (g->hit_flash > 0.01f) {
        cd_ui_quad(r, 0, 0, W, H, v4(1.f, 0.15f, 0.35f, g->hit_flash * 0.35f), 0);
    }
    if (g->boost_flash > 0.01f) {
        // speed lines at the screen edges
        float a = g->boost_flash * 0.45f;
        for (int i = 0; i < 22; i++) {
            float t = (float)i / 22.f;
            float yy = H * (0.15f + 0.7f * ((float)((i * 37) % 100) / 100.f));
            float len = s(90.f, g) + s(120.f, g) * sinf(g->menu_time * 9.f + t * 12.f);
            cd_ui_quad(r, -len * 0.2f, yy, len, s(4.f, g), v4(0.6f, 0.95f, 1.f, a), 0);
            cd_ui_quad(r, W - len * 0.8f, yy, len, s(4.f, g), v4(0.6f, 0.95f, 1.f, a), 0);
        }
    }
}

// ---------------------------------------------------------------------------
// Title screen
// ---------------------------------------------------------------------------
void cd_ui_menu(cd_game* g) {
    cd_renderer* r = &g->renderer;
    float W = (float)r->w, H = (float)r->h;
    float t = g->menu_time;

    // vignette
    cd_ui_quad(r, 0, 0, W, H, v4(0.03f, 0.01f, 0.09f, 0.35f), 0);

    float title_size = fminf(H * 0.16f, W * 0.105f);
    title_size = cd_ui_text_fit("CYBER", title_size, W * 0.80f);
    float ty = H * 0.13f;
    // chromatic-aberration title
    cd_ui_text(r, "CYBER", W * 0.5f - cd_ui_text_width("CYBER", title_size) * 0.5f - s(4.f, g), ty,
               title_size, v4(1.f, 0.2f, 0.6f, 0.55f));
    cd_ui_text(r, "CYBER", W * 0.5f - cd_ui_text_width("CYBER", title_size) * 0.5f + s(4.f, g), ty,
               title_size, v4(0.2f, 0.9f, 1.f, 0.55f));
    cd_ui_text_center(r, "CYBER", W * 0.5f, ty, title_size, C_WHITE);
    float dy = ty + title_size * 1.02f;
    float dsize = cd_ui_text_fit("DRIFT", title_size * 0.94f, W * 0.72f);
    cd_ui_text(r, "DRIFT", W * 0.5f - cd_ui_text_width("DRIFT", dsize) * 0.5f - s(4.f, g), dy,
               dsize, v4(1.f, 0.25f, 0.7f, 0.5f));
    cd_ui_text_center(r, "DRIFT", W * 0.5f, dy, dsize, C_MAGENTA);

    // orbit ring behind the title for that synthwave feel
    float ring_r = fminf(W, H) * 0.26f;
    for (int i = 0; i < 40; i++) {
        float a = (float)i / 40.f * CD_TAU + t * 0.4f;
        float rr = ring_r * (1.0f + 0.04f * sinf(a * 3.f + t * 2.f));
        cd_ui_quad_rot(r, W * 0.5f + cosf(a) * rr, ty + title_size * 0.45f + sinf(a) * rr * 0.35f,
                       s(9.f, g), s(9.f, g), a, v4(0.4f, 1.f, 0.9f, 0.55f), 1.f);
    }

    float tag = cd_ui_text_fit("NEON CANYON HOVER RACER", s(26.f, g), W * 0.7f);
    cd_ui_text_center(r, "NEON CANYON HOVER RACER", W * 0.5f, dy + dsize * 1.05f + s(10.f, g), tag, C_TEAL);

    // how to play
    float panel_w = fminf(W * 0.78f, s(900.f, g));
    float hy = dy + dsize * 1.05f + s(78.f, g);
    float inner = panel_w - s(48.f, g);
    cd_ui_bevel_box(r, W * 0.5f - panel_w * 0.5f, hy, panel_w, s(146.f, g), s(20.f, g), C_PANEL,
                    v4(0.5f, 0.35f, 1.f, 0.6f));
    cd_ui_text_fit_center(r, "LEFT STICK STEER     NITRO BOOST     BRAKE SLOW", W * 0.5f,
                          hy + s(26.f, g), s(27.f, g), inner, C_WHITE);
    cd_ui_text_fit_center(r, "COLLECT ENERGY ORBS - AVOID CRYSTALS AND BARRIERS", W * 0.5f,
                          hy + s(66.f, g), s(23.f, g), inner, C_DIM);
    cd_ui_text_fit_center(r, "LANDSCAPE TABLET - TOUCH CONTROLS", W * 0.5f,
                          hy + s(102.f, g), s(21.f, g), inner, C_VIOLET);

    // launch button
    float bw = fminf(W * 0.40f, s(500.f, g)), bh = s(104.f, g);
    float bx = W * 0.5f - bw * 0.5f, by = H - bh - s(46.f, g);
    float pulse = 0.75f + 0.25f * sinf(t * 4.f);
    cd_ui_bevel_box(r, bx, by, bw, bh, s(22.f, g), v4(0.10f, 0.42f * pulse, 0.40f * pulse, 0.92f), C_TEAL);
    cd_ui_text_fit_center(r, "LAUNCH", W * 0.5f, by + bh * 0.30f, s(44.f, g), bw - s(40.f, g), C_WHITE);
    g->start_btn_x = bx; g->start_btn_y = by;
    g->start_btn_w = bw; g->start_btn_h = bh;

    if (g->best > 0) {
        char buf[64], b[80];
        fmt_int(buf, g->best);
        int bn = 0;
        for (const char* p = "BEST "; *p; p++) b[bn++] = *p;
        for (const char* p = buf; *p; p++) b[bn++] = *p;
        b[bn] = 0;
        cd_ui_text_fit_center(r, b, W * 0.5f, by - s(34.f, g), s(24.f, g), bw, C_AMBER);
    }
}
