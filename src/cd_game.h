#ifndef CD_GAME_H
#define CD_GAME_H

#include "cd_render.h"
#include "cd_math.h"

#define CD_MAX_OBSTACLES 220
#define CD_MAX_PICKUPS 220
#define CD_MAX_PARTICLES 900
#define CD_MAX_BOLTS 48
#define CD_TRACK_SEGS 128
#define CD_TRACK_SPACING 36.f
#define CD_CHUNK_LEN 360.f
// Canyon wall segment length (short, so walls follow the track curve).
#define WALL_SEG 18.f
#define CD_CHUNK_SEGS 10

typedef enum {
    CD_STATE_MENU = 0,
    CD_STATE_PLAY = 1,
    CD_STATE_PAUSE = 2,
    CD_STATE_GAMEOVER = 3,
} cd_game_state;

typedef struct {
    vec3 pos;        // world position
    float angle;     // heading radians (0 = +Z)
    float vel_speed; // forward speed m/s
    float vel_yaw;   // yaw rate
    float drift;     // |slip| for trail intensity
    float pitch, roll;
    float hover_phase;
    float boost;     // 0..1 nitro tank
    float boosting;  // 0..1 smoothed boost amount
    float tilt_input, steer_input;
    float engine_glow;
} cd_craft;

typedef struct {
    int active;
    int kind;        // 0 = hazard block, 1 = crystal, 2 = pillar
    vec3 pos;
    float radius;
    float height;
    float spin;
    float bob;
} cd_obstacle;

typedef struct {
    int active;
    int kind;        // 0 = energy, 1 = boost cell, 2 = shield
    vec3 pos;
    float phase;
    float radius;
    int lane;
} cd_pickup;

typedef struct {
    int active;
    vec3 pos;
    vec3 vel;
    vec4 color;
    float life, max_life, size;
    int kind;        // 0 = flat glow sprite, 1 = spark streak
} cd_particle;

typedef struct {
    int active;
    vec3 pos, vel;
    float life;
} cd_bolt;

typedef struct {
    float s;      // arc length along the track
    float half_w; // corridor half width
    vec3 center;
    vec3 tangent;
} cd_track_sample;

typedef struct {
    cd_track_sample samples[CD_TRACK_SEGS + 1];
    int count;
    float arc_total;
} cd_track_layout;

typedef struct {
    cd_game_state state;
    cd_craft craft;
    cd_renderer renderer;

    // world
    cd_gpumesh mesh_deck;        // track tile
    cd_gpumesh mesh_craft;
    cd_gpumesh mesh_orb;
    cd_gpumesh mesh_boost;
    cd_gpumesh mesh_shield;
    cd_gpumesh mesh_crystal;
    cd_gpumesh mesh_block;
    cd_gpumesh mesh_pillar;
    cd_gpumesh mesh_arch;
    cd_gpumesh mesh_pylon;
    cd_gpumesh mesh_wall_l, mesh_wall_r;
    cd_gpumesh mesh_rock;
    cd_gpumesh mesh_building_a, mesh_building_b, mesh_building_c;
    cd_gpumesh mesh_skyline;     // distant static towers
    cd_gpumesh mesh_particles;
    cd_gpumesh mesh_trail;

    cd_obstacle obstacles[CD_MAX_OBSTACLES];
    cd_pickup pickups[CD_MAX_PICKUPS];
    cd_particle particles[CD_MAX_PARTICLES];
    cd_bolt bolts[CD_MAX_BOLTS];

    // spawning cursors (world arc-length based)
    float next_obstacle_s;
    float next_pickup_s;
    float next_arch_s;

    // stats
    long score;
    long best;
    int energy;
    int combo;
    float combo_timer;
    float distance;
    float speed_display;
    float play_time;
    float shake;
    float hit_flash;
    float boost_flash;
    int shields;
    float shield_timer;

    // track geometry (infinite, procedural by chunk)
    float curve_amp;
    float curve_freq;
    float curve_phase;
    float width;
    float far_z;
    rng_t rng;

    // input state (filled by platform layer)
    float in_steer;     // -1..1
    float in_throttle;  // 0..1 (nitro)
    int in_brake;
    int tap_pause;

    // ui scale computed from screen size
    float ui_scale;
    int screen_w, screen_h;
    float menu_time;

    // fps
    float fps;

    // touch state produced by the platform layer
    float joy_dx, joy_dy;        // joystick vector (-1..1)
    float joy_cx, joy_cy, joy_r; // joystick centre / radius in pixels
    float nitro_cx, nitro_cy, nitro_r;
    float brake_cx, brake_cy, brake_r;
    float pause_btn_x, pause_btn_y, pause_btn_r;
    float start_btn_x, start_btn_y, start_btn_w, start_btn_h;
    float restart_btn_x, restart_btn_y, restart_btn_w, restart_btn_h;
} cd_game;

void cd_game_init(cd_game* g, int screen_w, int screen_h);
void cd_game_resize(cd_game* g, int screen_w, int screen_h);
void cd_game_reset(cd_game* g);
void cd_game_update(cd_game* g, float dt);
void cd_game_render(cd_game* g);

// Track sampling used by both gameplay + rendering.
vec3 cd_track_center(float z);
float cd_track_width_at(float z);
void cd_track_frame(float z, vec3* center, vec3* tangent, float* yaw);

// Touch handling produced by the platform layer.
typedef struct {
    float steer_axis;   // -1..1 from joystick / tilt buttons
    int nitro;          // nitro held
    int brake;
    int pause_pressed;
    int start_pressed;
    int resume_pressed;
    int restart_pressed;
    float pause_x, pause_y, pause_r;  // hit region in pixels (for drawing)
} cd_input;

#endif
