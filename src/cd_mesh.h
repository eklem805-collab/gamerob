// Cyber Drift — procedural mesh construction kit.
// Every 3D asset in the game is generated here at runtime (no imported models).
#ifndef CD_MESH_H
#define CD_MESH_H

#include "cd_math.h"
#include <stddef.h>

typedef struct {
    float px, py, pz;
    float nx, ny, nz;
    float r, g, b, a;   // a = emission strength (0 diffuse .. 1 full neon)
} cd_vertex;

typedef struct {
    cd_vertex* v;
    int count;
    int cap;
} cd_mesh;

void cd_mesh_init(cd_mesh* m);
void cd_mesh_free(cd_mesh* m);
void cd_mesh_clear(cd_mesh* m);
void cd_mesh_reserve(cd_mesh* m, int extra);

// Core primitives -----------------------------------------------------------
void cd_tri(cd_mesh* m, vec3 a, vec3 b, vec3 c, vec4 col);
void cd_quad(cd_mesh* m, vec3 a, vec3 b, vec3 c, vec3 d, vec4 col);
void cd_box(cd_mesh* m, vec3 center, vec3 half, vec4 col);
void cd_box_rot(cd_mesh* m, vec3 center, vec3 half, float yaw, vec4 col);
void cd_box_emit_edges(cd_mesh* m, vec3 center, vec3 half, vec4 col, vec4 emit, float t);
void cd_prism(cd_mesh* m, vec3 center, float radius, float h_top, float h_bot, int sides, vec4 col, float twist);
void cd_pyramid(cd_mesh* m, vec3 center, float radius, float height, int sides, vec4 col, float twist);
void cd_ring(cd_mesh* m, vec3 center, vec3 normal, float radius, float tube, int seg_major, int seg_minor, vec4 col, int glow_sides);
void cd_icosphere(cd_mesh* m, vec3 center, float radius, int subdiv, vec4 col);
void cd_cylinder(cd_mesh* m, vec3 a, vec3 b, float r0, float r1, int sides, vec4 col);
void cd_disc(cd_mesh* m, vec3 center, vec3 normal, float radius, int sides, vec4 col);
void cd_beam(cd_mesh* m, vec3 a, vec3 b, float width, vec4 col);

// Asset kit ---------------------------------------------------------------
typedef struct {
    vec4 body;      // hull
    vec4 accent;    // trim
    vec4 glow;      // emissive parts
    vec4 dark;      // underbelly / panels
} cd_palette;

void cd_kit_hovercraft(cd_mesh* m, const cd_palette* p);
void cd_kit_building(cd_mesh* m, rng_t* rng, vec3 base, float w, float h, float d, const cd_palette* p);
void cd_kit_arch(cd_mesh* m, vec3 base, float height, float width, const cd_palette* p);
void cd_kit_energy_orb(cd_mesh* m, float radius, vec4 col);
void cd_kit_crystal(cd_mesh* m, rng_t* rng, float height, vec4 col, vec4 tip);
void cd_kit_pylon(cd_mesh* m, float height, const cd_palette* p);
void cd_kit_canyon_wall(cd_mesh* m, rng_t* rng, float length, float height, float side, const cd_palette* p, float z0);
void cd_kit_ground_plate(cd_mesh* m, float width, float length, const cd_palette* p);
void cd_kit_rock(cd_mesh* m, rng_t* rng, float size, vec4 col);
void cd_kit_world_plate(cd_mesh* m, float size, int res, rng_t* rng, vec4 low, vec4 high);

// Helpers -----------------------------------------------------------------
void cd_mesh_bounds(const cd_mesh* m, vec3* out_min, vec3* out_max);

#endif
