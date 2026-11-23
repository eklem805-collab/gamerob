// Cyber Drift — drives the engine headlessly and dumps a GL trace.
//   trace_main out.trace <state> [frames_to_simulate] [width] [height]
#include "cd_game.h"
#include "cd_gl.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

extern int cd_trace_write(const char* path);
extern void trace_gl_load_required(void);
extern void cd_ui_menu(cd_game* g);

int main(int argc, char** argv) {
    const char* out = argc > 1 ? argv[1] : "build/trace.bin";
    const char* state = argc > 2 ? argv[2] : "play";
    int sim = argc > 3 ? atoi(argv[3]) : 240;
    int W = argc > 4 ? atoi(argv[4]) : 1280;
    int H = argc > 5 ? atoi(argv[5]) : 800;

    static cd_game g;
    cd_game_init(&g, W, H);          // builds every mesh procedurally
    if (!cd_render_init(&g.renderer, W, H)) {
        printf("render init failed\n");
        return 1;
    }

    if (!strcmp(state, "play")) {
        g.state = CD_STATE_PLAY;
        g.in_steer = 0.0f;
        for (int i = 0; i < sim; i++) cd_game_update(&g, 1.f / 60.f);
        g.in_throttle = 1.f;
        for (int i = 0; i < 40; i++) cd_game_update(&g, 1.f / 60.f);
        g.in_throttle = 0.f;
    } else if (!strcmp(state, "menu")) {
        g.state = CD_STATE_MENU;
        for (int i = 0; i < sim; i++) cd_game_update(&g, 1.f / 60.f);
    } else if (!strcmp(state, "gameover")) {
        g.state = CD_STATE_PLAY;
        for (int i = 0; i < sim; i++) cd_game_update(&g, 1.f / 60.f);
        g.score = 18450;
        g.play_time = 96.4f;
        g.best = 22910;
        g.distance = 2140.f;
        g.state = CD_STATE_GAMEOVER;
    } else if (!strcmp(state, "showcase")) {
        g.state = CD_STATE_PLAY;
        for (int i = 0; i < 300; i++) cd_game_update(&g, 1.f / 60.f);
        // Deterministic line-up of every gameplay element in view.
        for (int i = 0; i < CD_MAX_PICKUPS; i++) g.pickups[i].active = 0;
        for (int i = 0; i < CD_MAX_OBSTACLES; i++) g.obstacles[i].active = 0;
        for (int i = 0; i < 5; i++) {
            float z = g.craft.pos.z + 26.f + (float)i * 13.f;
            vec3 c; float yaw;
            cd_track_frame(z, &c, NULL, &yaw);
            vec3 right = v3(cosf(yaw), 0.f, -sinf(yaw));
            cd_pickup* p = &g.pickups[i];
            p->active = 1;
            p->kind = (i == 4) ? 1 : 0;
            p->phase = (float)i * 0.7f;
            p->radius = 1.45f;
            p->pos = v3add(c, v3add(v3scale(right, sinf((float)i * 1.1f) * 5.f), v3(0, 2.2f, 0)));
        }
        // pillar to the left, barrier to the right
        {
            vec3 c; float yaw;
            cd_track_frame(g.craft.pos.z + 62.f, &c, NULL, &yaw);
            vec3 right = v3(cosf(yaw), 0.f, -sinf(yaw));
            cd_obstacle* o = &g.obstacles[0];
            o->active = 1; o->kind = 2; o->radius = 1.7f; o->height = 7.5f; o->spin = 0.3f;
            o->pos = v3add(c, v3scale(right, -7.5f));
            o = &g.obstacles[1];
            o->active = 1; o->kind = 0; o->radius = 2.4f; o->height = 1.2f; o->spin = 0.1f; o->bob = 0.f;
            o->pos = v3add(c, v3add(v3scale(right, 7.f), v3(0, 1.4f, 0)));
            // arch gate further out
            cd_track_frame(g.craft.pos.z + 120.f, &c, NULL, &yaw);
            o = &g.obstacles[2];
            o->active = 1; o->kind = 3; o->pos = c; o->radius = 0.f; o->spin = 0.f; o->bob = 0.f;
        }
        g.in_throttle = 0.0f;
    } else if (!strcmp(state, "pause")) {
        g.state = CD_STATE_PLAY;
        for (int i = 0; i < sim; i++) cd_game_update(&g, 1.f / 60.f);
        g.score = 9420;
        g.state = CD_STATE_PAUSE;
    }

    if (!strcmp(state, "assets")) {
        // Fixed-camera line-up of every procedurally generated asset.
        vec3 cam = v3(0.f, 12.f, -26.f);
        vec3 target = v3(0.f, 3.0f, 4.f);
        mat4 view = mat4_lookAt(cam, target, v3(0, 1, 0));
        mat4 proj = mat4_perspective(50.f * CD_PI / 180.f, (float)W / (float)H, 0.3f, 300.f);
        cd_render_begin_world(&g.renderer, cam, view, proj, 1.0f);
        cd_render_sky(&g.renderer);
        // row 1: craft, orbs, boost, shield
        cd_render_draw(&g.renderer, CD_SHADER_NEON, mat4_translate(v3(-9, 2.2f, 6)), &g.mesh_craft, v4(1,1,1,1), 60.f);
        cd_render_draw(&g.renderer, CD_SHADER_NEON, mat4_translate(v3(-2.5f, 2.2f, 6)), &g.mesh_orb, v4(1,1,1,1), 60.f);
        cd_render_draw(&g.renderer, CD_SHADER_NEON, mat4_translate(v3(2.0f, 2.2f, 6)), &g.mesh_boost, v4(1,1,1,1), 60.f);
        cd_render_draw(&g.renderer, CD_SHADER_NEON, mat4_translate(v3(6.5f, 2.2f, 6)), &g.mesh_shield, v4(1,1,1,1), 60.f);
        // row 2: hazards + pylon + rock
        cd_render_draw(&g.renderer, CD_SHADER_NEON, mat4_translate(v3(-10, 0.0f, 16)), &g.mesh_crystal, v4(1,1,1,1), 60.f);
        cd_render_draw(&g.renderer, CD_SHADER_NEON, mat4_translate(v3(-3, 1.4f, 16)), &g.mesh_block, v4(1,1,1,1), 60.f);
        cd_render_draw(&g.renderer, CD_SHADER_NEON, mat4_translate(v3(2.5f, 0.0f, 16)), &g.mesh_pillar, v4(1,1,1,1), 60.f);
        cd_render_draw(&g.renderer, CD_SHADER_NEON, mat4_translate(v3(7.5f, 0.0f, 16)), &g.mesh_pylon, v4(1,1,1,1), 60.f);
        cd_render_draw(&g.renderer, CD_SHADER_NEON, mat4_translate(v3(11.f, 1.0f, 14)), &g.mesh_rock, v4(1,1,1,1), 60.f);
        // row 3: arch gate, buildings
        cd_render_draw(&g.renderer, CD_SHADER_NEON, mat4_translate(v3(-9, 0.f, 30)), &g.mesh_arch, v4(1,1,1,1), 60.f);
        cd_render_draw(&g.renderer, CD_SHADER_NEON, mat4_translate(v3(2, 0.f, 34)), &g.mesh_building_a, v4(1,1,1,1), 60.f);
        cd_render_draw(&g.renderer, CD_SHADER_NEON, mat4_translate(v3(18, 0.f, 34)), &g.mesh_building_b, v4(1,1,1,1), 60.f);
        cd_render_draw(&g.renderer, CD_SHADER_NEON, mat4_translate(v3(-26, 0.f, 34)), &g.mesh_building_c, v4(1,1,1,1), 60.f);
        cd_render_begin_ui(&g.renderer, W, H);
        cd_ui_text(&g.renderer, "PROCEDURAL ASSET SHEET", 24, 20, 26, v4(0.4f,1.f,0.9f,1.f));
        cd_render_end_ui(&g.renderer);
        return cd_trace_write(out) ? 0 : 1;
    }
    cd_game_render(&g);
    if (g.state == CD_STATE_MENU) {
        g.best = 22910;
        cd_render_begin_ui(&g.renderer, W, H);
        cd_ui_menu(&g);
        cd_render_end_ui(&g.renderer);
    }
    printf("camera pos=(%.2f,%.2f,%.2f) look=(%.2f,%.2f,%.2f) craft=(%.2f,%.2f,%.2f) score=%ld\n",
           g.renderer.cam_pos.x, g.renderer.cam_pos.y, g.renderer.cam_pos.z,
           g.craft.pos.x + sinf(g.craft.angle) * 16.f, g.craft.pos.y + 1.8f,
           g.craft.pos.z + cosf(g.craft.angle) * 16.f,
           g.craft.pos.x, g.craft.pos.y, g.craft.pos.z, g.score);
    return cd_trace_write(out) ? 0 : 1;
}
