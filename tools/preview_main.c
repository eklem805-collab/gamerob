// Cyber Drift — desktop preview harness.
//
// Renders the *exact same* engine code that ships in the APK (cd_game /
// cd_render / cd_ui) into an offscreen GLES context, then writes PNG frames.
// This is how the visuals are validated without an Android device.
//
//   preview out_dir [frames] [state]
//
// state: play | menu | gameover | pause
#include "preview_gl.h"
#include "cd_gl.h"
#include "cd_game.h"
#include <unistd.h>

extern int preview_egl_init(int w, int h);

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern void cd_ui_menu(cd_game* g);

// ----------------------------------------------------------------- PNG writer
static unsigned char* g_pixels;

static void png_write(const char* path, int w, int h, const unsigned char* rgba) {
    // flip vertically (GL origin is bottom-left)
    size_t row = (size_t)w * 4;
    unsigned char* flipped = (unsigned char*)malloc(row * h);
    for (int y = 0; y < h; y++) memcpy(flipped + row * y, rgba + row * (h - 1 - y), row);

    size_t raw_size = (row + 1) * h;
    unsigned char* raw = (unsigned char*)malloc(raw_size);
    for (int y = 0; y < h; y++) {
        raw[y * (row + 1)] = 0;
        memcpy(raw + y * (row + 1) + 1, flipped + row * y, row);
    }
    free(flipped);

    // simple zlib store-only deflate
    size_t max_out = raw_size + raw_size / 65535 * 5 + 64;
    unsigned char* z = (unsigned char*)malloc(max_out);
    size_t zlen = 0;
    // zlib header
    z[zlen++] = 0x78;
    z[zlen++] = 0x01;
    size_t pos = 0;
    while (pos < raw_size) {
        size_t block = raw_size - pos;
        if (block > 65535) block = 65535;
        int final = (pos + block >= raw_size);
        z[zlen++] = (unsigned char)(final ? 1 : 0);
        z[zlen++] = (unsigned char)(block & 0xFF);
        z[zlen++] = (unsigned char)(block >> 8);
        z[zlen++] = (unsigned char)(~block & 0xFF);
        z[zlen++] = (unsigned char)((~block >> 8) & 0xFF);
        memcpy(z + zlen, raw + pos, block);
        zlen += block;
        pos += block;
    }
    // adler32
    unsigned long a = 1, b = 0;
    for (size_t i = 0; i < raw_size; i++) {
        a = (a + raw[i]) % 65521;
        b = (b + a) % 65521;
    }
    unsigned long adler = (b << 16) | a;
    z[zlen++] = (unsigned char)(adler >> 24);
    z[zlen++] = (unsigned char)(adler >> 16);
    z[zlen++] = (unsigned char)(adler >> 8);
    z[zlen++] = (unsigned char)(adler);
    free(raw);

    FILE* f = fopen(path, "wb");
    if (!f) { printf("cannot write %s\n", path); return; }
    static const unsigned char sig[8] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
    fwrite(sig, 1, 8, f);

    unsigned char buf[32];
    // IHDR
    buf[0] = 0; buf[1] = 0; buf[2] = 0; buf[3] = 13;
    memcpy(buf + 4, "IHDR", 4);
    unsigned int W = (unsigned int)w, H = (unsigned int)h;
    buf[8] = (unsigned char)(W >> 24); buf[9] = (unsigned char)(W >> 16);
    buf[10] = (unsigned char)(W >> 8); buf[11] = (unsigned char)W;
    buf[12] = (unsigned char)(H >> 24); buf[13] = (unsigned char)(H >> 16);
    buf[14] = (unsigned char)(H >> 8); buf[15] = (unsigned char)H;
    buf[16] = 8; buf[17] = 6; buf[18] = 0; buf[19] = 0; buf[20] = 0;
    unsigned int crc = 0xFFFFFFFFu;
    for (int i = 4; i < 21; i++) {
        crc ^= buf[i];
        for (int k = 0; k < 8; k++) crc = (crc >> 1) ^ (0xEDB88320u & (-(int)(crc & 1)));
    }
    crc ^= 0xFFFFFFFFu;
    buf[21] = (unsigned char)(crc >> 24); buf[22] = (unsigned char)(crc >> 16);
    buf[23] = (unsigned char)(crc >> 8); buf[24] = (unsigned char)crc;
    fwrite(buf, 1, 25, f);

    // IDAT
    size_t idat_len = zlen;
    unsigned char* idat = (unsigned char*)malloc(idat_len + 12);
    idat[0] = (unsigned char)(idat_len >> 24); idat[1] = (unsigned char)(idat_len >> 16);
    idat[2] = (unsigned char)(idat_len >> 8); idat[3] = (unsigned char)idat_len;
    memcpy(idat + 4, "IDAT", 4);
    memcpy(idat + 8, z, idat_len);
    crc = 0xFFFFFFFFu;
    for (size_t i = 4; i < idat_len + 8; i++) {
        crc ^= idat[i];
        for (int k = 0; k < 8; k++) crc = (crc >> 1) ^ (0xEDB88320u & (-(int)(crc & 1)));
    }
    crc ^= 0xFFFFFFFFu;
    idat[8 + idat_len] = (unsigned char)(crc >> 24);
    idat[9 + idat_len] = (unsigned char)(crc >> 16);
    idat[10 + idat_len] = (unsigned char)(crc >> 8);
    idat[11 + idat_len] = (unsigned char)crc;
    fwrite(idat, 1, idat_len + 12, f);
    free(idat);
    free(z);

    buf[0] = 0; buf[1] = 0; buf[2] = 0; buf[3] = 0;
    memcpy(buf + 4, "IEND", 4);
    crc = 0xAE426082u;
    buf[8] = (unsigned char)(crc >> 24); buf[9] = (unsigned char)(crc >> 16);
    buf[10] = (unsigned char)(crc >> 8); buf[11] = (unsigned char)crc;
    fwrite(buf, 1, 12, f);
    fclose(f);
    printf("wrote %s (%dx%d)\n", path, w, h);
}

// ------------------------------------------------------------------- main
int main(int argc, char** argv) {
    const char* outdir = argc > 1 ? argv[1] : "preview_frames";
    int frames = argc > 2 ? atoi(argv[2]) : 1;
    const char* state = argc > 3 ? argv[3] : "play";
    int W = 1280, H = 800;   // 16:10 tablet
    if (argc > 5) { W = atoi(argv[4]); H = atoi(argv[5]); }

    if (!preview_egl_init(W, H)) {
        printf("EGL init failed\n");
        return 1;
    }
    if (preview_gl_load() > 0) {
        printf("GL functions missing\n");
        return 1;
    }

    static cd_game g;
    cd_game_init(&g, W, H);
    if (!cd_render_init(&g.renderer, W, H)) return 1;

    if (!strcmp(state, "play")) {
        g.state = CD_STATE_PLAY;
        // simulate a few seconds of driving so the scene is populated
        g.in_steer = 0.25f;
        for (int i = 0; i < 240; i++) cd_game_update(&g, 1.f / 60.f);
        g.in_throttle = 1.f;
        for (int i = 0; i < 60; i++) cd_game_update(&g, 1.f / 60.f);
    } else if (!strcmp(state, "menu")) {
        g.state = CD_STATE_MENU;
        for (int i = 0; i < 90; i++) cd_game_update(&g, 1.f / 60.f);
    } else if (!strcmp(state, "pause")) {
        g.state = CD_STATE_PLAY;
        for (int i = 0; i < 300; i++) cd_game_update(&g, 1.f / 60.f);
        g.state = CD_STATE_PAUSE;
    } else if (!strcmp(state, "gameover")) {
        g.state = CD_STATE_PLAY;
        for (int i = 0; i < 300; i++) cd_game_update(&g, 1.f / 60.f);
        g.score = 18450;
        g.play_time = 96.4f;
        g.best = 22910;
        g.state = CD_STATE_GAMEOVER;
    }

    g_pixels = (unsigned char*)malloc((size_t)W * H * 4);
    char path[512];
    for (int f = 0; f < frames; f++) {
        if (f > 0) {
            cd_game_update(&g, 1.f / 60.f);
            if (g.state == CD_STATE_MENU) cd_game_update(&g, 1.f / 60.f);
        }
        cd_game_render(&g);
        if (g.state == CD_STATE_MENU) {
            cd_render_begin_ui(&g.renderer, W, H);
            cd_ui_menu(&g);
            cd_render_end_ui(&g.renderer);
        }
        pgl_glReadPixels(0, 0, W, H, 0x1908, 0x1401, g_pixels);
        snprintf(path, sizeof(path), "%s/%s_%02d.png", outdir, state, f);
        png_write(path, W, H, g_pixels);
    }
    printf("done\n");
    return 0;
}
