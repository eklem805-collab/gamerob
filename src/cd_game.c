// Cyber Drift — gameplay, procedural world streaming, rendering.
#include "cd_game.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

extern const cd_palette CD_PAL_HULL_MAIN, CD_PAL_MAGENTA, CD_PAL_VIOLET, CD_PAL_TEAL, CD_PAL_SUNSET;

// Visual constants
#define CAM_BACK 8.2f
#define CAM_HEIGHT 3.1f
#define CAM_LOOK_AHEAD 16.f
#define CRAFT_MAX_SPEED 78.f
#define CRAFT_BOOST_SPEED 128.f
#define DESPAWN_Z (-60.f)

static vec4 emissive(vec4 c, float e) { return v4(c.x, c.y, c.z, e); }

// ---------------------------------------------------------------------------
// Setup
// ---------------------------------------------------------------------------
static void build_static_meshes(cd_game* g) {
    cd_mesh m;
    cd_palette pal;

    // Track deck tile (tile spans one chunk segment of length CD_CHUNK_LEN/CD_CHUNK_SEGS).
    pal = CD_PAL_VIOLET;
    pal.body = v4(0.10f, 0.07f, 0.22f, 0.f);
    pal.dark = v4(0.05f, 0.04f, 0.12f, 0.f);
    m.v = NULL; cd_mesh_init(&m);
    cd_box(&m, v3(0, -0.55f, -CD_CHUNK_LEN / CD_CHUNK_SEGS * 0.5f),
           v3(1.0f, 0.55f, CD_CHUNK_LEN / CD_CHUNK_SEGS * 0.5f), pal.body);
    // neon lane rails are generated per-instance via scaling of this tile's rails
    cd_gpumesh_upload(&g->mesh_deck, &m);
    cd_mesh_free(&m);

    // Hovercraft
    cd_mesh_init(&m);
    cd_kit_hovercraft(&m, &CD_PAL_HULL_MAIN);
    cd_gpumesh_upload(&g->mesh_craft, &m);
    cd_mesh_free(&m);

    // Pickups
    cd_mesh_init(&m);
    cd_kit_energy_orb(&m, 1.0f, v4(0.20f, 1.00f, 0.90f, 1.f));
    cd_gpumesh_upload(&g->mesh_orb, &m);
    cd_mesh_free(&m);

    cd_mesh_init(&m);
    {   // boost cell: stacked prisms, orange/magenta
        cd_prism(&m, v3(0, -1.f, 0), 0.85f, 1.0f, -1.0f, 6, emissive(v4(1.0f, 0.55f, 0.15f, 1), 1.f), 0.f);
        cd_ring(&m, v3(0, 0, 0), v3(0, 1, 0), 1.15f, 0.09f, 8, 3, emissive(v4(1.f, 0.9f, 0.3f, 1), 1.f), 4);
        cd_ring(&m, v3(0, 0, 0), v3(1, 0, 0.4f), 0.95f, 0.07f, 7, 3, emissive(v4(1.f, 0.35f, 0.6f, 1), 1.f), 4);
    }
    cd_gpumesh_upload(&g->mesh_boost, &m);
    cd_mesh_free(&m);

    cd_mesh_init(&m);
    {   // shield pickup: blue shell
        cd_icosphere(&m, v3(0, 0, 0), 0.95f, 1, emissive(v4(0.35f, 0.65f, 1.0f, 0.85f), 0.85f));
        cd_ring(&m, v3(0, 0, 0), v3(0, 1, 0), 1.25f, 0.08f, 10, 3, emissive(v4(0.6f, 0.9f, 1.f, 1), 1.f), 4);
    }
    cd_gpumesh_upload(&g->mesh_shield, &m);
    cd_mesh_free(&m);

    // Crystal hazard
    cd_mesh_init(&m);
    {
        rng_t r = {991u};
        cd_kit_crystal(&m, &r, 5.5f, v4(0.95f, 0.15f, 0.45f, 0.f), emissive(v4(1.0f, 0.35f, 0.75f, 1), 1.f));
    }
    cd_gpumesh_upload(&g->mesh_crystal, &m);
    cd_mesh_free(&m);

    // Hazard block (floating industrial barrier)
    cd_mesh_init(&m);
    {
        vec4 body = v4(0.22f, 0.10f, 0.30f, 0.f);
        cd_box_emit_edges(&m, v3(0, 0, 0), v3(2.2f, 1.1f, 0.5f), body, emissive(v4(1.0f, 0.25f, 0.65f, 1), 1.f), 0.16f);
        cd_box_emit_edges(&m, v3(0, 1.2f, 0), v3(0.55f, 0.25f, 0.30f), body, emissive(v4(1.f, 0.85f, 0.2f, 1), 1.f), 0.10f);
    }
    cd_gpumesh_upload(&g->mesh_block, &m);
    cd_mesh_free(&m);

    // Pillar obstacle
    cd_mesh_init(&m);
    {
        cd_prism(&m, v3(0, 0, 0), 1.5f, 7.5f, 0.f, 6, v4(0.16f, 0.10f, 0.34f, 0.f), 0.3f);
        cd_beam(&m, v3(0, 1.2f, 0), v3(0, 7.4f, 0), 0.22f, emissive(v4(0.25f, 1.0f, 0.9f, 1), 1.f));
        cd_ring(&m, v3(0, 6.4f, 0), v3(0, 1, 0), 1.75f, 0.10f, 8, 3, emissive(v4(1.f, 0.3f, 0.8f, 1), 1.f), 4);
    }
    cd_gpumesh_upload(&g->mesh_pillar, &m);
    cd_mesh_free(&m);

    // Arch gate (cleared for bonus)
    cd_mesh_init(&m);
    cd_kit_arch(&m, v3(0, 0, 0), 13.f, 21.f, &CD_PAL_TEAL);
    cd_gpumesh_upload(&g->mesh_arch, &m);
    cd_mesh_free(&m);

    // Pylon
    cd_mesh_init(&m);
    cd_kit_pylon(&m, 5.0f, &CD_PAL_MAGENTA);
    cd_gpumesh_upload(&g->mesh_pylon, &m);
    cd_mesh_free(&m);

    // Canyon walls (left / right)
    // Canyon walls are generated as a ribbon every frame (draw_canyons).

    // Rock
    cd_mesh_init(&m);
    { rng_t r = {51u}; cd_kit_rock(&m, &r, 2.2f, v4(0.16f, 0.11f, 0.26f, 0.f)); }
    cd_gpumesh_upload(&g->mesh_rock, &m);
    cd_mesh_free(&m);

    // Three building archetypes for the skyline
    const cd_palette* pals[3] = {&CD_PAL_VIOLET, &CD_PAL_MAGENTA, &CD_PAL_TEAL};
    cd_gpumesh* dst[3] = {&g->mesh_building_a, &g->mesh_building_b, &g->mesh_building_c};
    for (int i = 0; i < 3; i++) {
        cd_mesh_init(&m);
        rng_t r = {(uint32_t)(100u + i * 37u)};
        cd_kit_building(&m, &r, v3(0, 0, 0), 14.f, 62.f, 14.f, pals[i]);
        cd_gpumesh_upload(dst[i], &m);
        cd_mesh_free(&m);
    }

    // Distant static skyline ring
    cd_mesh_init(&m);
    {
        rng_t r = {777u};
        for (int i = 0; i < 64; i++) {
            float a = (float)i / 64.f * CD_TAU;
            float rad = 260.f + rng_range(&r, -60.f, 160.f);
            float h = rng_range(&r, 30.f, 130.f);
            float w = rng_range(&r, 8.f, 22.f);
            vec3 c = v3(cosf(a) * rad, h * 0.5f - 10.f, sinf(a) * rad);
            cd_box(&m, c, v3(w, h * 0.5f, w), v4(0.10f, 0.05f, 0.20f, 0.f));
            cd_beam(&m, v3(c.x - w, h - 10.f, c.z + w), v3(c.x + w, h - 10.f, c.z + w), 0.5f,
                    emissive(v4(1.f, 0.3f, 0.8f, 1), 0.8f));
        }
    }
    cd_gpumesh_upload(&g->mesh_skyline, &m);
    cd_mesh_free(&m);
}

void cd_game_init(cd_game* g, int sw, int sh) {
    memset(g, 0, sizeof(*g));
    g->rng.s = 20261123u;
    g->state = CD_STATE_MENU;
    g->width = 19.f;
    g->far_z = 420.f;
    g->screen_w = sw;
    g->screen_h = sh;
    g->ui_scale = clampf((float)sh / 800.f, 0.8f, 2.2f);
    g->craft.pos = v3(0, 0, 0);
    g->craft.boost = 1.f;
    g->craft.hover_phase = 0.f;
    g->menu_time = 0.f;
    build_static_meshes(g);
    cd_game_reset(g);
}

void cd_game_reset(cd_game* g) {
    g->score = 0;
    g->energy = 0;
    g->combo = 0;
    g->combo_timer = 0.f;
    g->distance = 0.f;
    g->play_time = 0.f;
    g->shields = 0;
    g->shield_timer = 0.f;
    g->hit_flash = 0.f;
    g->boost_flash = 0.f;
    g->shake = 0.f;
    g->craft.pos = v3(0, 0, 0);
    g->craft.angle = 0.f;
    g->craft.vel_speed = 26.f;
    g->craft.vel_yaw = 0.f;
    g->craft.drift = 0.f;
    g->craft.boost = 1.f;
    g->craft.boosting = 0.f;
    g->craft.pitch = g->craft.roll = 0.f;
    g->next_obstacle_s = 150.f;
    g->next_pickup_s = 60.f;
    g->next_arch_s = 380.f;
    for (int i = 0; i < CD_MAX_OBSTACLES; i++) g->obstacles[i].active = 0;
    for (int i = 0; i < CD_MAX_PICKUPS; i++) g->pickups[i].active = 0;
    for (int i = 0; i < CD_MAX_PARTICLES; i++) g->particles[i].active = 0;
    for (int i = 0; i < CD_MAX_BOLTS; i++) g->bolts[i].active = 0;
}

void cd_game_resize(cd_game* g, int sw, int sh) {
    g->screen_w = sw;
    g->screen_h = sh;
    g->ui_scale = clampf((float)sh / 800.f, 0.8f, 2.2f);
    cd_render_resize(&g->renderer, sw, sh);
}

// ---------------------------------------------------------------------------
// World streaming: spawn hazards/pickups relative to the craft's Z.
// ---------------------------------------------------------------------------
static cd_obstacle* alloc_obstacle(cd_game* g) {
    for (int i = 0; i < CD_MAX_OBSTACLES; i++)
        if (!g->obstacles[i].active) return &g->obstacles[i];
    return NULL;
}

static cd_pickup* alloc_pickup(cd_game* g) {
    for (int i = 0; i < CD_MAX_PICKUPS; i++)
        if (!g->pickups[i].active) return &g->pickups[i];
    return NULL;
}

static void spawn_obstacle(cd_game* g, float z, float lane_t) {
    cd_obstacle* o = alloc_obstacle(g);
    if (!o) return;
    vec3 c; float yaw;
    cd_track_frame(z, &c, NULL, &yaw);
    float half = cd_track_width_at(z) - 2.5f;
    vec3 right = v3(cosf(yaw), 0.f, -sinf(yaw));
    o->active = 1;
    o->spin = rng_range(&g->rng, 0.f, CD_TAU);
    o->bob = rng_range(&g->rng, 0.f, CD_TAU);
    int roll = rng_int(&g->rng, 0, 9);
    if (roll < 4) {
        o->kind = 0;  // barrier block
        o->radius = 2.4f;
        o->height = 1.2f;
        o->pos = v3add(c, v3add(v3scale(right, lane_t * half), v3(0, 1.4f, 0)));
    } else if (roll < 7) {
        o->kind = 2;  // pillar
        o->radius = 1.7f;
        o->height = 7.5f;
        o->pos = v3add(c, v3add(v3scale(right, lane_t * half), v3(0, 0.f, 0)));
    } else {
        o->kind = 1;  // crystal cluster
        o->radius = 2.1f;
        o->height = 5.5f;
        o->pos = v3add(c, v3add(v3scale(right, lane_t * half * 1.1f), v3(0, 0.f, 0)));
    }
}

static void spawn_pickup(cd_game* g, float z, float lane_t) {
    cd_pickup* p = alloc_pickup(g);
    if (!p) return;
    vec3 c; float yaw;
    cd_track_frame(z, &c, NULL, &yaw);
    float half = cd_track_width_at(z) - 3.0f;
    vec3 right = v3(cosf(yaw), 0.f, -sinf(yaw));
    int roll = rng_int(&g->rng, 0, 9);
    p->active = 1;
    p->kind = roll < 7 ? 0 : (roll < 9 ? 1 : 2);
    p->phase = rng_range(&g->rng, 0.f, CD_TAU);
    p->radius = 1.45f;
    p->lane = (int)(lane_t * 3.f);
    p->pos = v3add(c, v3add(v3scale(right, lane_t * half), v3(0, 2.0f, 0)));
}

// ---------------------------------------------------------------------------
// Particles
// ---------------------------------------------------------------------------
static void emit_particle(cd_game* g, vec3 pos, vec3 vel, vec4 col, float life, float size, int kind) {
    for (int i = 0; i < CD_MAX_PARTICLES; i++) {
        cd_particle* p = &g->particles[i];
        if (p->active) continue;
        p->active = 1;
        p->pos = pos;
        p->vel = vel;
        p->color = col;
        p->life = life;
        p->max_life = life;
        p->size = size;
        p->kind = kind;
        return;
    }
}

static void burst(cd_game* g, vec3 pos, vec4 col, int n, float speed) {
    for (int i = 0; i < n; i++) {
        vec3 dir = v3(rng_range(&g->rng, -1.f, 1.f), rng_range(&g->rng, -0.3f, 1.f),
                      rng_range(&g->rng, -1.f, 1.f));
        dir = v3norm(dir);
        emit_particle(g, pos, v3scale(dir, rng_range(&g->rng, speed * 0.3f, speed)), col,
                      rng_range(&g->rng, 0.35f, 0.9f), rng_range(&g->rng, 0.25f, 0.7f), 0);
    }
}

// ---------------------------------------------------------------------------
// Update
// ---------------------------------------------------------------------------
static void craft_world_frame(cd_game* g, vec3* fwd, vec3* right) {
    float a = g->craft.angle;
    *fwd = v3(sinf(a), 0.f, cosf(a));
    *right = v3(cosf(a), 0.f, -sinf(a));
}

static void update_craft(cd_game* g, float dt) {
    cd_craft* c = &g->craft;

    // --- steering + drift physics
    float steer_target = clampf(g->in_steer, -1.f, 1.f);
    c->steer_input = expo_lerp(c->steer_input, steer_target, 12.f, dt);
    c->tilt_input = expo_lerp(c->tilt_input, g->in_throttle, 8.f, dt);

    float speed_norm = clampf(c->vel_speed / CRAFT_BOOST_SPEED, 0.f, 1.f);
    // More responsive at low speed, heavier at high speed (arcade feel).
    float yaw_per_sec = lerpf(2.45f, 1.35f, speed_norm) * c->steer_input;
    float yaw_accel = lerpf(22.f, 13.f, speed_norm);
    c->vel_yaw = expo_lerp(c->vel_yaw, yaw_per_sec, yaw_accel, dt);
    c->angle += c->vel_yaw * dt;

    // --- longitudinal speed
    float target_speed = 62.f;
    if (c->tilt_input > 0.15f && c->boost > 0.02f) {
        target_speed = CRAFT_BOOST_SPEED;
        c->boost = clampf(c->boost - dt * 0.42f, 0.f, 1.f);
        g->boost_flash = fmaxf(g->boost_flash, 0.55f);
    } else {
        c->boost = clampf(c->boost + dt * 0.10f, 0.f, 1.f);
    }
    // Drift scrub: hard turns bleed speed, straightening up regains it.
    float scrub = fabsf(c->vel_yaw) * 3.1f;
    target_speed -= scrub;
    if (g->in_brake) target_speed *= 0.55f;
    c->vel_speed = expo_lerp(c->vel_speed, target_speed, g->in_brake ? 4.5f : 1.9f, dt);
    c->vel_speed = clampf(c->vel_speed, 12.f, CRAFT_BOOST_SPEED);
    c->boosting = expo_lerp(c->boosting, (c->tilt_input > 0.15f && c->boost > 0.02f) ? 1.f : 0.f, 6.f, dt);

    // Slip angle for trail intensity (drift feel).
    c->drift = expo_lerp(c->drift, fabsf(c->vel_yaw) * 0.55f, 6.f, dt);

    // --- integrate position (world scrolls in +Z)
    vec3 fwd, right;
    craft_world_frame(g, &fwd, &right);
    vec3 vel = v3(fwd.x * c->vel_speed, 0.f, fwd.z * c->vel_speed);
    c->pos = v3add(c->pos, v3scale(vel, dt));
    g->distance = c->pos.z;

    // --- keep inside the corridor
    vec3 center; float yaw;
    cd_track_frame(c->pos.z, &center, NULL, &yaw);
    float half = cd_track_width_at(c->pos.z) - 0.9f;
    vec3 right_t = v3(cosf(yaw), 0.f, -sinf(yaw));
    float lateral = v3dot(v3sub(c->pos, center), right_t);
    if (fabsf(lateral) > half) {
        float overshoot = fabsf(lateral) - half;
        c->pos = v3add(c->pos, v3scale(right_t, -signf(lateral) * overshoot));
        c->vel_speed = fmaxf(c->vel_speed * 0.94f, 12.f);
        g->shake = fmaxf(g->shake, 0.35f + overshoot * 0.02f);
        // drift sparks along the wall
        for (int i = 0; i < 3; i++) {
            vec3 sp = v3add(c->pos, v3scale(right_t, signf(lateral) * 1.6f));
            emit_particle(g, sp, v3(rng_range(&g->rng, -6.f, 6.f), rng_range(&g->rng, 0.f, 5.f),
                                   rng_range(&g->rng, -8.f, 2.f)),
                          v4(1.0f, 0.75f, 0.25f, 1.f), 0.4f, 0.35f, 0);
        }
    }
    // Hover bob + banking
    c->hover_phase += dt * 3.4f;
    float bank = -c->vel_yaw * 0.42f - c->steer_input * 0.18f;
    c->roll = expo_lerp(c->roll, bank, 7.f, dt);
    float pitch = -c->boosting * 0.12f + sinf(c->hover_phase) * 0.02f;
    c->pitch = expo_lerp(c->pitch, pitch, 6.f, dt);
    c->pos.y = center.y + 1.0f + sinf(c->hover_phase) * 0.30f;
    c->engine_glow = expo_lerp(c->engine_glow, 0.5f + c->boosting * 0.9f + c->drift * 0.4f, 9.f, dt);

    // engine trail + drift sparks
    int trail_rate = c->boosting > 0.5f ? 4 : 2;
    for (int i = 0; i < trail_rate; i++) {
        for (int s = -1; s <= 1; s += 2) {
            vec3 off = v3add(v3scale(right, 0.52f * s), v3(0.f, -0.05f, -1.4f));
            vec3 ep = v3add(c->pos, v3add(v3scale(fwd, -1.3f), v3scale(right, 0.52f * s)));
            vec4 col = c->boosting > 0.5f ? v4(1.0f, 0.55f, 0.15f, 1.f) : v4(0.35f, 1.0f, 0.95f, 1.f);
            emit_particle(g, ep, v3(rng_range(&g->rng, -1.5f, 1.5f), rng_range(&g->rng, -0.5f, 1.2f),
                                    -c->vel_speed * 0.25f - rng_range(&g->rng, 0.f, 6.f)),
                          col, rng_range(&g->rng, 0.25f, 0.55f), rng_range(&g->rng, 0.30f, 0.65f), 0);
            (void)off;
        }
    }
    if (c->drift > 0.35f) {
        vec3 bp = v3add(c->pos, v3add(v3scale(fwd, -1.0f), v3scale(right, -signf(c->vel_yaw) * 1.3f)));
        emit_particle(g, bp, v3scale(right, -signf(c->vel_yaw) * 5.f), v4(0.9f, 0.35f, 1.0f, 1.f), 0.45f, 0.5f, 0);
    }
}

static void update_world(cd_game* g, float dt) {
    cd_craft* c = &g->craft;
    float craft_z = c->pos.z;

    // Spawn ahead of the player, in arcs so density stays even.
    while (g->next_obstacle_s < craft_z + 340.f) {
        float lane = rng_range(&g->rng, -1.f, 1.f);
        spawn_obstacle(g, g->next_obstacle_s, lane);
        // occasional twin barrier gate that still leaves a gap
        if (rng_f(&g->rng) < 0.45f) {
            float other = lane > 0.f ? rng_range(&g->rng, -1.f, 0.05f) : rng_range(&g->rng, -0.05f, 1.f);
            spawn_obstacle(g, g->next_obstacle_s + rng_range(&g->rng, 4.f, 12.f), other);
        }
        g->next_obstacle_s += rng_range(&g->rng, 34.f, 62.f);
    }
    while (g->next_pickup_s < craft_z + 340.f) {
        // energy pearls often come in short trails of 3-5
        int n = rng_int(&g->rng, 2, 5);
        float base = rng_range(&g->rng, -0.85f, 0.85f);
        for (int i = 0; i < n; i++) {
            float t = clampf(base + sinf((float)i * 0.9f) * 0.35f, -0.95f, 0.95f);
            spawn_pickup(g, g->next_pickup_s + (float)i * 11.f, t);
        }
        g->next_pickup_s += rng_range(&g->rng, 90.f, 150.f);
    }

    // Despawn behind
    for (int i = 0; i < CD_MAX_OBSTACLES; i++) {
        cd_obstacle* o = &g->obstacles[i];
        if (!o->active) continue;
        if (o->pos.z < craft_z + DESPAWN_Z) o->active = 0;
        o->spin += dt * 0.8f;
    }
    for (int i = 0; i < CD_MAX_PICKUPS; i++) {
        cd_pickup* p = &g->pickups[i];
        if (!p->active) continue;
        if (p->pos.z < craft_z + DESPAWN_Z) p->active = 0;
        p->phase += dt * 2.2f;
    }

    // --- collisions with obstacles
    vec3 fwd, right;
    craft_world_frame(g, &fwd, &right);
    for (int i = 0; i < CD_MAX_OBSTACLES; i++) {
        cd_obstacle* o = &g->obstacles[i];
        if (!o->active) continue;
        float dz = o->pos.z - c->pos.z;
        if (fabsf(dz) > 6.f) continue;
        vec3 d = v3sub(o->pos, c->pos);
        d.y = 0.f;
        float dist = v3len(d);
        float hit_r = o->radius + 1.5f;
        if (dist < hit_r) {
            o->active = 0;
            if (g->shields > 0) {
                g->shields--;
                g->shake = fmaxf(g->shake, 0.6f);
                burst(g, o->pos, v4(0.4f, 0.8f, 1.f, 1.f), 26, 16.f);
                g->score += 120;
            } else {
                g->energy = (int)fmaxf(0.f, (float)g->energy - 25.f);
                g->combo = 0;
                g->combo_timer = 0.f;
                g->hit_flash = 1.f;
                g->shake = 1.f;
                c->vel_speed = fmaxf(c->vel_speed * 0.55f, 14.f);
                burst(g, o->pos, v4(1.f, 0.3f, 0.6f, 1.f), 34, 18.f);
                // knock the craft away from the impact
                vec3 away = v3norm(v3sub(c->pos, o->pos));
                c->pos = v3add(c->pos, v3scale(away, 1.6f));
                if (g->energy <= 0) {
                    g->state = CD_STATE_GAMEOVER;
                    if (g->score > g->best) g->best = g->score;
                    burst(g, c->pos, v4(1.f, 0.6f, 0.2f, 1.f), 60, 26.f);
                }
            }
        }
    }

    // --- pickups
    for (int i = 0; i < CD_MAX_PICKUPS; i++) {
        cd_pickup* p = &g->pickups[i];
        if (!p->active) continue;
        float dz = p->pos.z - c->pos.z;
        if (fabsf(dz) > 5.f) continue;
        vec3 d = v3sub(p->pos, c->pos);
        if (v3len(d) < p->radius + 1.7f) {
            p->active = 0;
            g->combo++;
            g->combo_timer = 2.2f;
            float mult = 1.f + (float)(g->combo / 5) * 0.5f;
            if (p->kind == 0) {
                g->energy = (int)fminf(100.f, (float)g->energy + 6.f);
                g->score += (long)(50.f * mult);
                burst(g, p->pos, v4(0.25f, 1.f, 0.9f, 1.f), 12, 9.f);
            } else if (p->kind == 1) {
                c->boost = clampf(c->boost + 0.55f, 0.f, 1.f);
                g->score += (long)(30.f * mult);
                burst(g, p->pos, v4(1.f, 0.7f, 0.2f, 1.f), 14, 10.f);
            } else {
                g->shields = (int)fminf(2, g->shields + 1);
                g->score += (long)(80.f * mult);
                burst(g, p->pos, v4(0.5f, 0.8f, 1.f, 1.f), 16, 11.f);
            }
        }
    }

    if (g->combo_timer > 0.f) {
        g->combo_timer -= dt;
        if (g->combo_timer <= 0.f) g->combo = 0;
    }

    // --- particles
    for (int i = 0; i < CD_MAX_PARTICLES; i++) {
        cd_particle* p = &g->particles[i];
        if (!p->active) continue;
        p->life -= dt;
        if (p->life <= 0.f) { p->active = 0; continue; }
        p->pos = v3add(p->pos, v3scale(p->vel, dt));
        p->vel = v3scale(p->vel, 1.f - 2.2f * dt);
    }

    // --- scoring over time
    g->score += (long)(c->vel_speed * dt * 1.2f);
}

void cd_game_update(cd_game* g, float dt) {
    if (dt > 0.1f) dt = 0.1f;   // clamp after hitches
    g->menu_time += dt;

    if (g->tap_pause) {
        g->tap_pause = 0;
        if (g->state == CD_STATE_PLAY) g->state = CD_STATE_PAUSE;
        else if (g->state == CD_STATE_PAUSE) g->state = CD_STATE_PLAY;
    }

    if (g->state == CD_STATE_PLAY) {
        g->play_time += dt;
        update_craft(g, dt);
        update_world(g, dt);
        g->speed_display = expo_lerp(g->speed_display, g->craft.vel_speed * 3.6f, 8.f, dt);
        if (g->shield_timer > 0.f) g->shield_timer -= dt;
    } else if (g->state == CD_STATE_MENU) {
        // slow cinematic drift for the title screen
        // The deck follows cd_track_center(), so the attract-mode craft has to
        // ride the same height curve or the camera ends up under the floor.
        float mz = g->menu_time * 26.f;
        vec3 mc = cd_track_center(mz);
        g->craft.pos = v3(mc.x + sinf(g->menu_time * 0.25f) * 4.f, mc.y + 1.1f, mz);
        g->craft.angle = sinf(g->menu_time * 0.4f) * 0.25f;
        g->craft.roll = sinf(g->menu_time * 0.6f) * 0.1f;
        g->craft.hover_phase += dt * 2.5f;
        g->craft.pos.y = mc.y + 1.1f + sinf(g->craft.hover_phase) * 0.3f;
        g->craft.vel_speed = 26.f;
        update_world(g, dt * 0.6f);
    } else if (g->state == CD_STATE_GAMEOVER) {
        // keep the camera alive; particles settle
        for (int i = 0; i < CD_MAX_PARTICLES; i++) {
            cd_particle* p = &g->particles[i];
            if (!p->active) continue;
            p->life -= dt;
            if (p->life <= 0.f) p->active = 0;
            p->pos = v3add(p->pos, v3scale(p->vel, dt));
        }
    }

    g->shake = fmaxf(0.f, g->shake - dt * 2.4f);
    g->hit_flash = fmaxf(0.f, g->hit_flash - dt * 2.6f);
    g->boost_flash = fmaxf(0.f, g->boost_flash - dt * 2.2f);
    if (g->state == CD_STATE_MENU && g->score > g->best) g->best = g->score;
}

// ---------------------------------------------------------------------------
// Rendering
// ---------------------------------------------------------------------------
// Canyon walls: one continuous ribbon per side, rebuilt every frame from the
// track curve so it can never crack open at a segment seam or cut a corner.
static float wall_noise(float z, float seed) {
    return 0.55f * sinf(z * 0.021f + seed) + 0.30f * sinf(z * 0.047f + seed * 2.3f) +
           0.15f * sinf(z * 0.093f + seed * 4.1f);
}

static void wall_quad(cd_mesh* m, vec3 a, vec3 b, vec3 c, vec3 d, vec4 col) {
    cd_quad(m, a, b, c, d, col);
}

static void draw_canyons(cd_game* g, float cam_z) {
    cd_craft* c = &g->craft;
    static cd_mesh wm;
    static int built = 0;
    if (!built) { cd_mesh_init(&wm); built = 1; }
    wm.count = 0;

    const float seg_len = 10.f;
    const float span = 520.f;
    vec4 body_l = v4(0.13f, 0.09f, 0.28f, 0.f);
    vec4 body_r = v4(0.20f, 0.06f, 0.32f, 0.f);
    vec4 dark = v4(0.05f, 0.03f, 0.11f, 0.f);
    int base = (int)floorf(cam_z / seg_len) - 2;

    for (int side = -1; side <= 1; side += 2) {
        vec4 body = side < 0 ? body_l : body_r;
        float seed = side < 0 ? 2.7f : 8.1f;
        vec4 strata_a = (side < 0) ? v4(0.10f, 1.0f, 0.90f, 0.90f) : v4(1.0f, 0.20f, 0.80f, 0.80f);
        vec4 strata_b = (side < 0) ? v4(1.0f, 0.20f, 0.80f, 0.75f) : v4(0.35f, 1.0f, 0.95f, 0.85f);
        vec4 crest = (side < 0) ? v4(1.0f, 0.30f, 0.85f, 1.0f) : v4(0.20f, 1.0f, 0.92f, 1.0f);

        for (int i = 0; i < (int)(span / seg_len); i++) {
            float z0 = (float)(base + i) * seg_len;
            float z1 = z0 + seg_len;
            if (z1 < cam_z - 30.f) continue;
            if (z0 > cam_z + span) break;
            vec3 c0, c1; float yaw0, yaw1;
            cd_track_frame(z0, &c0, NULL, &yaw0);
            cd_track_frame(z1, &c1, NULL, &yaw1);
            vec3 r0 = v3(cosf(yaw0), 0.f, -sinf(yaw0));
            vec3 r1 = v3(cosf(yaw1), 0.f, -sinf(yaw1));
            float n0 = wall_noise(z0, seed), n1 = wall_noise(z1, seed);
            float hw0 = cd_track_width_at(z0) + 3.6f + 1.1f * n0;
            float hw1 = cd_track_width_at(z1) + 3.6f + 1.1f * n1;
            float h0 = 21.f * (0.78f + 0.18f * n0);
            float h1 = 21.f * (0.78f + 0.18f * n1);
            float ow0 = hw0 + 13.f + 2.5f * n0, ow1 = hw1 + 13.f + 2.5f * n1;
            float oh0 = h0 * 0.46f, oh1 = h1 * 0.46f;
            vec3 o0 = v3add(v3(c0.x, 0.f, z0), v3scale(r0, (float)side * ow0));
            vec3 o1 = v3add(v3(c1.x, 0.f, z1), v3scale(r1, (float)side * ow1));
            // inner lip at the base, crest pushed outwards for a battered face
            vec3 f0 = v3add(v3(c0.x, 0.f, z0), v3scale(r0, (float)side * hw0));
            vec3 f1 = v3add(v3(c1.x, 0.f, z1), v3scale(r1, (float)side * hw1));
            vec3 t0 = v3(f0.x + (o0.x - f0.x) * 0.45f, h0, z0);
            vec3 t1 = v3(f1.x + (o1.x - f1.x) * 0.45f, h1, z1);
            vec3 s0 = v3(o0.x, oh0, z0), s1 = v3(o1.x, oh1, z1);
            wall_quad(&wm, f0, f1, t1, t0, body);   // face the track
            wall_quad(&wm, t0, t1, s1, s0, dark);   // top ledge
            wall_quad(&wm, s0, s1, o1, o0, dark);   // outer flank
            // glowing strata on the inner face (parallel to the lip)
            cd_beam(&wm, v3(f0.x, h0 * 0.30f, z0), v3(f1.x, h1 * 0.30f, z1), 0.26f,
                    emissive(strata_a, 0.92f));
            cd_beam(&wm, v3((f0.x + t0.x) * 0.5f, h0 * 0.66f, z0),
                    v3((f1.x + t1.x) * 0.5f, h1 * 0.66f, z1), 0.24f, emissive(strata_b, 0.85f));
            cd_beam(&wm, v3(t0.x, h0 + 0.10f, z0), v3(t1.x, h1 + 0.10f, z1), 0.34f,
                    emissive(crest, 0.95f));
        }
    }
    (void)c;
    if (wm.count) {
        cd_gpumesh_upload(&g->mesh_wall_l, &wm);
        cd_render_draw(&g->renderer, CD_SHADER_NEON, mat4_identity(), &g->mesh_wall_l,
                       v4(1, 1, 1, 1), 260.f);
    }
}

static void draw_deck(cd_game* g) {
    cd_craft* c = &g->craft;
    float seg_len = 4.f;
    static cd_mesh deck;
    static int built = 0;
    if (!built) { cd_mesh_init(&deck); built = 1; }
    deck.count = 0;

    int base = (int)floorf(c->pos.z / seg_len);
    for (int i = -6; i < 150; i++) {
        float z0 = (float)(base + i) * seg_len;
        float z1 = z0 + seg_len;
        if (z0 < c->pos.z - 40.f) continue;
        vec3 c0, c1; float yaw0, yaw1;
        cd_track_frame(z0, &c0, NULL, &yaw0);
        cd_track_frame(z1, &c1, NULL, &yaw1);
        float hw0 = cd_track_width_at(z0);
        float hw1 = cd_track_width_at(z1);
        vec3 r0 = v3(cosf(yaw0), 0.f, -sinf(yaw0));
        vec3 r1 = v3(cosf(yaw1), 0.f, -sinf(yaw1));
        vec3 l0 = v3sub(c0, v3scale(r0, hw0));
        vec3 R0 = v3add(c0, v3scale(r0, hw0));
        vec3 l1 = v3sub(c1, v3scale(r1, hw1));
        vec3 R1 = v3add(c1, v3scale(r1, hw1));
        float yoff = -0.35f;
        l0.y += yoff; R0.y += yoff; l1.y += yoff; R1.y += yoff;

        // walking checker tint for speed readability
        int band = (int)floorf(z0 / 24.f);
        float shade = (band & 1) ? 1.0f : 0.82f;
        vec4 floor_col = v4(0.085f * shade, 0.055f * shade, 0.20f * shade, 0.f);
        cd_quad(&deck, l0, R0, R1, l1, floor_col);

        // neon rails
        vec4 railc = (band & 1) ? v4(0.20f, 1.0f, 0.90f, 0.9f) : v4(1.0f, 0.25f, 0.80f, 0.9f);
        float rw = 0.34f;
        cd_beam(&deck, v3(l0.x, l0.y + 0.05f, l0.z), v3(l1.x, l1.y + 0.05f, l1.z), rw, railc);
        cd_beam(&deck, v3(R0.x, R0.y + 0.05f, R0.z), v3(R1.x, R1.y + 0.05f, R1.z), rw, railc);
        // centre dashes
        if (i % 3 == 0) {
            cd_beam(&deck, v3(c0.x, c0.y + yoff + 0.06f, c0.z), v3(c1.x, c1.y + yoff + 0.06f, c1.z),
                    0.20f, v4(0.45f, 0.30f, 1.0f, 0.55f));
        }
    }
    cd_gpumesh_upload(&g->mesh_deck, &deck);
    cd_render_draw(&g->renderer, CD_SHADER_NEON, mat4_identity(), &g->mesh_deck, v4(1, 1, 1, 1), 190.f);
}

static void draw_obstacles(cd_game* g) {
    for (int i = 0; i < CD_MAX_OBSTACLES; i++) {
        cd_obstacle* o = &g->obstacles[i];
        if (!o->active) continue;
        float dist = o->pos.z - g->craft.pos.z;
        // Never draw geometry that would intersect the chase camera.
        if (dist > 520.f || dist < -40.f) continue;
        if (dist < 9.f && dist > -14.f && fabsf(v3len(v3sub(o->pos, g->renderer.cam_pos))) < o->radius + 6.f) continue;
        mat4 m = mat4_translate(o->pos);
        if (o->kind == 3) {
            mat4 am = mat4_mul(mat4_translate(o->pos), mat4_rotY(atan2f(0.f, 1.f)));
            vec3 ctr; float yaw;
            cd_track_frame(o->pos.z, &ctr, NULL, &yaw);
            am = mat4_mul(mat4_translate(o->pos), mat4_rotY(yaw));
            cd_render_draw(&g->renderer, CD_SHADER_NEON, am, &g->mesh_arch, v4(1, 1, 1, 1), 150.f);
            continue;
        }
        if (o->kind == 0) {
            m = mat4_mul(m, mat4_rotY(o->spin * 0.3f));
            m = mat4_mul(m, mat4_translate(v3(0, sinf(o->bob + o->spin) * 0.4f, 0)));
            cd_render_draw(&g->renderer, CD_SHADER_NEON, m, &g->mesh_block, v4(1, 1, 1, 1), 120.f);
        } else if (o->kind == 2) {
            m = mat4_mul(m, mat4_rotY(o->spin * 0.5f));
            cd_render_draw(&g->renderer, CD_SHADER_NEON, m, &g->mesh_pillar, v4(1, 1, 1, 1), 120.f);
        } else {
            m = mat4_mul(m, mat4_rotY(o->spin * 0.25f));
            cd_render_draw(&g->renderer, CD_SHADER_NEON, m, &g->mesh_crystal, v4(1, 1, 1, 1), 120.f);
        }
    }
    // roadside pylons every 36 m for speed sense
    for (int i = -1; i < 40; i++) {
        float z = floorf(g->craft.pos.z / 36.f) * 36.f + (float)i * 36.f;
        vec3 ctr; float yaw;
        cd_track_frame(z, &ctr, NULL, &yaw);
        float w = cd_track_width_at(z);
        vec3 right = v3(cosf(yaw), 0.f, -sinf(yaw));
        for (int s = -1; s <= 1; s += 2) {
            vec3 p = v3add(ctr, v3scale(right, (w + 1.4f) * (float)s));
            mat4 m = mat4_mul(mat4_translate(p), mat4_rotY(yaw));
            int parity = ((int)floorf(z / 36.f) + (s > 0 ? 1 : 0)) & 1;
            cd_render_draw(&g->renderer, CD_SHADER_NEON, m, &g->mesh_pylon,
                           parity ? v4(1, 1, 1, 1) : v4(1.4f, 0.8f, 1.6f, 1), 140.f);
        }
    }
}

static void draw_pickups(cd_game* g) {
    for (int i = 0; i < CD_MAX_PICKUPS; i++) {
        cd_pickup* p = &g->pickups[i];
        if (!p->active) continue;
        float dist = p->pos.z - g->craft.pos.z;
        if (dist > 520.f || dist < -40.f) continue;
        // Hide pickups that are already at/behind the craft's nose: at 12 m
        // from the chase camera they fill the screen for a couple of frames
        // before being collected.
        if (v3len(v3sub(p->pos, g->renderer.cam_pos)) < 13.f) continue;
        vec3 pos = p->pos;
        pos.y += sinf(p->phase) * 0.45f;
        mat4 m = mat4_mul(mat4_translate(pos), mat4_rotY(p->phase * 0.9f));
        float pscale = (p->kind == 0) ? 0.78f : 0.85f;
        m = mat4_mul(m, mat4_scale(v3(pscale, pscale, pscale)));
        if (p->kind == 0) cd_render_draw(&g->renderer, CD_SHADER_NEON, m, &g->mesh_orb, v4(1, 1, 1, 1), 120.f);
        else if (p->kind == 1) cd_render_draw(&g->renderer, CD_SHADER_NEON, m, &g->mesh_boost, v4(1, 1, 1, 1), 120.f);
        else cd_render_draw(&g->renderer, CD_SHADER_NEON, m, &g->mesh_shield, v4(1, 1, 1, 1), 120.f);
    }
}

static void draw_particles(cd_game* g) {
    // Billboarded quads rebuilt every frame in world space.
    static cd_mesh pm;
    static int built = 0;
    if (!built) { cd_mesh_init(&pm); built = 1; }
    pm.count = 0;
    cd_craft* c = &g->craft;
    for (int i = 0; i < CD_MAX_PARTICLES; i++) {
        cd_particle* p = &g->particles[i];
        if (!p->active) continue;
        float dist = p->pos.z - c->pos.z;
        if (dist < -45.f || dist > 420.f) continue;
        // Fade puffs out near the lens: the chase camera sits ~14 m behind the
        // tail, so fresh exhaust would otherwise be a screen-filling white
        // sheet. This also keeps the additive pass from washing out the frame.
        float near_cam = v3len(v3sub(p->pos, g->renderer.cam_pos));
        float cam_fade = clampf((near_cam - 4.f) / 14.f, 0.f, 1.f);
        if (cam_fade <= 0.01f) continue;
        float t = clampf(p->life / p->max_life, 0.f, 1.f) * cam_fade;
        float s = p->size * (0.35f + t * 0.75f) * cam_fade;
        // Additive pass: brightness carries the fade (blend factor is ONE).
        vec4 col = v4(p->color.x, p->color.y, p->color.z, 1.f);
        col.x *= t * 0.75f; col.y *= t * 0.75f; col.z *= t * 0.75f;
        // camera-facing approximation: plane perpendicular to view Z
        vec3 right = v3(1, 0, 0), up = v3(0, 1, 0);
        vec3 a = v3add(p->pos, v3add(v3scale(right, -s), v3scale(up, -s)));
        vec3 b = v3add(p->pos, v3add(v3scale(right, s), v3scale(up, -s)));
        vec3 cc = v3add(p->pos, v3add(v3scale(right, s), v3scale(up, s)));
        vec3 d = v3add(p->pos, v3add(v3scale(right, -s), v3scale(up, s)));
        cd_quad(&pm, a, b, cc, d, col);
    }
    if (pm.count) {
        cd_gpumesh_upload(&g->mesh_particles, &pm);
        cd_renderer* r = &g->renderer;
        CD_GL(glDisable)(GL_CULL_FACE);
        CD_GL(glEnable)(GL_BLEND);
        CD_GL(glBlendFunc)(GL_SRC_ALPHA, GL_ONE);
        CD_GL(glDepthMask)(GL_FALSE);
        cd_render_draw(r, CD_SHADER_NEON, mat4_identity(), &g->mesh_particles, v4(1, 1, 1, 1), 120.f);
        CD_GL(glDepthMask)(GL_TRUE);
        CD_GL(glDisable)(GL_BLEND);
        CD_GL(glEnable)(GL_CULL_FACE);
    }
}

static void draw_craft(cd_game* g) {
    cd_craft* c = &g->craft;
    mat4 m = mat4_translate(c->pos);
    m = mat4_mul(m, mat4_rotY(c->angle));
    m = mat4_mul(m, mat4_rotZ(c->roll));
    m = mat4_mul(m, mat4_rotX(c->pitch));
    float pulse = 0.75f + 0.25f * sinf(g->menu_time * 6.0f) * (0.4f + c->boosting);
    vec4 tint = v4(pulse + c->boosting * 0.30f, pulse, pulse + c->drift * 0.25f, 1.f);
    cd_render_draw(&g->renderer, CD_SHADER_NEON, m, &g->mesh_craft, tint, 120.f);
    // shield bubble
    if (g->shields > 0) {
        cd_renderer* r = &g->renderer;
        static cd_mesh bubble;
        static int built = 0;
        if (!built) {
            cd_mesh_init(&bubble);
            cd_icosphere(&bubble, v3(0, 0, 0), 2.6f, 2, v4(0.02f, 0.05f, 0.09f, 0.f));
            cd_gpumesh_upload(&g->mesh_trail, &bubble);
            cd_mesh_free(&bubble);
            built = 1;
        }
        CD_GL(glEnable)(GL_BLEND);
        CD_GL(glBlendFunc)(GL_SRC_ALPHA, GL_ONE);
        CD_GL(glDepthMask)(GL_FALSE);
        CD_GL(glDisable)(GL_CULL_FACE);
        mat4 bm = mat4_translate(c->pos);
        bm = mat4_mul(bm, mat4_rotY(c->angle));
        bm = mat4_mul(bm, mat4_scale(v3(1.1f, 0.8f, 1.4f)));
        // Rim-only energy bubble: the base colour is nearly black so the
        // additive pass reads as a fresnel halo instead of a solid dome.
        float sh = 0.75f + 0.25f * sinf(g->menu_time * 5.0f);
        cd_render_draw(r, CD_SHADER_NEON, bm, &g->mesh_trail,
                       v4(sh * 0.09f, sh * 0.16f, sh * 0.30f, 1.f), 500.f);
        CD_GL(glDepthMask)(GL_TRUE);
        CD_GL(glDisable)(GL_BLEND);
        CD_GL(glEnable)(GL_CULL_FACE);
    }
}

static void draw_skyline(cd_game* g) {
    mat4 m = mat4_translate(v3(g->craft.pos.x * 0.35f, 0.f, g->craft.pos.z * 0.35f));
    cd_render_draw(&g->renderer, CD_SHADER_NEON, m, &g->mesh_skyline, v4(0.9f, 0.7f, 1.1f, 1), 600.f);
}

void cd_game_render(cd_game* g) {
    cd_renderer* r = &g->renderer;
    cd_craft* c = &g->craft;

    // ---- camera: chase cam with look-ahead and shake
    vec3 ctr; float yaw;
    cd_track_frame(c->pos.z + 8.f, &ctr, NULL, &yaw);
    // blend track direction with craft heading for a smooth chase cam
    static float cam_yaw_state = 0.f;
    float target_yaw = yaw;
    if (g->state != CD_STATE_MENU) target_yaw = lerpf(yaw, c->angle, 0.35f);
    cam_yaw_state = expo_lerp(cam_yaw_state, target_yaw, 3.2f, 1.f / 60.f);
    float cam_yaw = cam_yaw_state;
    vec3 back = v3(-sinf(cam_yaw), 0.f, -cosf(cam_yaw));
    float speed_zoom = clampf(c->vel_speed / CRAFT_BOOST_SPEED, 0.f, 1.f);
    vec3 cam_pos = v3add(c->pos, v3scale(back, CAM_BACK + speed_zoom * 3.0f));
    cam_pos.y = c->pos.y + CAM_HEIGHT + speed_zoom * 0.6f;
    float shake = g->shake;
    if (shake > 0.f) {
        cam_pos.x += sinf(g->menu_time * 91.f) * shake * 0.55f;
        cam_pos.y += sinf(g->menu_time * 77.f) * shake * 0.35f;
        cam_pos.z += cosf(g->menu_time * 83.f) * shake * 0.35f;
    }
    // Aim slightly above the nose so the craft sits low in frame and the
    // corridor gets the bulk of the screen.
    vec3 look = v3add(c->pos, v3add(v3(sinf(c->angle) * CAM_LOOK_AHEAD, 0.4f, cosf(c->angle) * CAM_LOOK_AHEAD),
                                  v3(0, 2.2f, 0)));
    // glTF-style camera basis: +X right, +Y up, +Z towards the viewer, so
    // front faces stay counter-clockwise on screen (see cd_math.h).
    mat4 view = mat4_lookAt(cam_pos, look, v3(0, 1, 0));
    float fov = (52.f + speed_zoom * 10.f + c->boosting * 6.f) * CD_PI / 180.f;
    mat4 proj = mat4_perspective(fov, r->aspect, 0.35f, 900.f);

    cd_render_begin_world(r, cam_pos, view, proj, g->menu_time);
    cd_render_sky(r);
    draw_skyline(g);
    draw_canyons(g, cam_pos.z);
    draw_deck(g);
    draw_obstacles(g);
    draw_pickups(g);
    draw_craft(g);
    draw_particles(g);

    // ---- HUD
    cd_render_begin_ui(r, r->w, r->h);
    extern void cd_ui_hud(cd_game* g);
    cd_ui_hud(g);
    cd_render_end_ui(r);
}
