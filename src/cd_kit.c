// Procedural asset kit — every model is built from triangles at load time.
#include "cd_mesh.h"

// Palette used everywhere (Neon Teal / Magenta / Dark Violet).
const cd_palette CD_PAL_HULL_MAIN = {{0.09f, 0.13f, 0.28f, 0.0f},
                                     {0.28f, 0.95f, 0.90f, 0.25f},
                                     {0.15f, 1.00f, 0.85f, 1.00f},
                                     {0.05f, 0.06f, 0.15f, 0.0f}};
const cd_palette CD_PAL_MAGENTA = {{0.24f, 0.07f, 0.36f, 0.0f},
                                   {1.00f, 0.25f, 0.85f, 0.85f},
                                   {1.00f, 0.20f, 0.75f, 1.00f},
                                   {0.07f, 0.03f, 0.14f, 0.0f}};
const cd_palette CD_PAL_VIOLET = {{0.15f, 0.10f, 0.32f, 0.0f},
                                  {0.55f, 0.35f, 1.00f, 0.55f},
                                  {0.62f, 0.35f, 1.00f, 1.00f},
                                  {0.06f, 0.04f, 0.13f, 0.0f}};
const cd_palette CD_PAL_TEAL = {{0.06f, 0.20f, 0.26f, 0.0f},
                                {0.15f, 1.00f, 0.85f, 0.75f},
                                {0.20f, 1.00f, 0.90f, 1.00f},
                                {0.03f, 0.09f, 0.13f, 0.0f}};
const cd_palette CD_PAL_SUNSET = {{0.32f, 0.10f, 0.18f, 0.0f},
                                  {1.00f, 0.45f, 0.30f, 0.45f},
                                  {1.00f, 0.55f, 0.20f, 1.00f},
                                  {0.10f, 0.03f, 0.08f, 0.0f}};

static vec4 emis(vec4 c, float e) { return v4(c.x, c.y, c.z, e); }

// Canyon corridor geometry: inner lips sit just outside the widest track.
#define WALL_INNER_X 17.5f
#define WALL_OUTER_X 34.0f

// ---------------------------------------------------------------------------
// Hovercraft: arrow-head hull with canards, twin glow engines, cockpit dome.
// Local space: +Z forward, +Y up, origin at hull centre.
// Roughly 3.2 long x 3.6 wide x 1.1 tall.
// ---------------------------------------------------------------------------
void cd_kit_hovercraft(cd_mesh* m, const cd_palette* p) {
    vec4 hull = p->body;
    vec4 accent = p->accent;
    vec4 glow = p->glow;
    vec4 dark = p->dark;

    // Nose spike + main fuselage (faceted arrow).
    vec3 nose = v3(0.f, 0.02f, 1.75f);
    vec3 tail = v3(0.f, 0.05f, -1.35f);
    vec3 lw = v3(-0.62f, 0.f, 0.15f);
    vec3 rw = v3(0.62f, 0.f, 0.15f);
    vec3 lw2 = v3(-0.30f, -0.10f, -1.05f);
    vec3 rw2 = v3(0.30f, -0.10f, -1.05f);
    vec3 topC = v3(0.f, 0.36f, -0.25f);
    vec3 botC = v3(0.f, -0.30f, -0.15f);

    // Upper shell
    cd_tri(m, nose, lw, topC, hull);
    cd_tri(m, nose, topC, rw, hull);
    cd_tri(m, topC, lw, tail, accent);
    cd_tri(m, topC, tail, rw, accent);
    // Lower shell
    cd_tri(m, lw, nose, botC, dark);
    cd_tri(m, nose, rw, botC, dark);
    cd_tri(m, botC, lw2, tail, dark);
    cd_tri(m, botC, tail, rw2, dark);
    // Side flanks
    cd_tri(m, lw, lw2, tail, hull);
    cd_tri(m, rw, tail, rw2, hull);
    cd_tri(m, lw, botC, lw2, hull);
    cd_tri(m, rw, rw2, botC, hull);

    // Canards / wings with neon leading edge.
    for (int s = -1; s <= 1; s += 2) {
        float x = (float)s;
        vec3 a = v3(0.45f * x, 0.02f, 0.10f);
        vec3 b = v3(1.55f * x, 0.10f, -0.55f);
        vec3 c = v3(0.42f * x, 0.02f, -0.62f);
        vec3 d = v3(1.30f * x, 0.06f, -0.90f);
        cd_tri(m, a, b, c, accent);
        cd_tri(m, b, d, c, hull);
        // wingtip light
        cd_beam(m, b, v3(b.x + 0.05f * x, b.y, b.z + 0.02f), 0.20f, glow);
        cd_beam(m, a, b, 0.055f, emis(glow, 1.0f));
        // rim accent strip along the hull side
        cd_beam(m, v3(0.58f * x, 0.10f, 0.55f), v3(0.60f * x, 0.06f, -0.95f), 0.05f, emis(accent, 0.9f));
    }

    // Cockpit canopy (dark glass + rim glow).
    vec3 cbase = v3(0.f, 0.30f, 0.30f);
    cd_tri(m, cbase, v3(-0.26f, 0.34f, 0.05f), v3(0.f, 0.46f, 0.02f), emis(v4(0.05f, 0.10f, 0.18f, 0.6f), 0.6f));
    cd_tri(m, cbase, v3(0.f, 0.46f, 0.02f), v3(0.26f, 0.34f, 0.05f), emis(v4(0.05f, 0.10f, 0.18f, 0.6f), 0.6f));
    cd_tri(m, cbase, v3(0.26f, 0.34f, 0.05f), v3(-0.26f, 0.34f, 0.05f), emis(v4(0.04f, 0.08f, 0.16f, 0.5f), 0.5f));
    cd_beam(m, v3(-0.27f, 0.345f, 0.06f), v3(0.27f, 0.345f, 0.06f), 0.045f, emis(glow, 1.0f));

    // Dorsal fin.
    cd_tri(m, v3(0.f, 0.34f, 0.05f), v3(0.f, 0.78f, -0.62f), v3(0.f, 0.30f, -0.78f), accent);
    cd_tri(m, v3(0.f, 0.34f, 0.05f), v3(0.f, 0.30f, -0.78f), v3(0.f, 0.78f, -0.62f), accent);
    cd_beam(m, v3(0.f, 0.76f, -0.60f), v3(0.f, 0.76f, -0.36f), 0.05f, emis(glow, 1.0f));

    // Twin engine nacelles + exhaust glow rings.
    for (int s = -1; s <= 1; s += 2) {
        float x = 0.52f * (float)s;
        vec3 c = v3(x, -0.02f, -0.92f);
        cd_cylinder(m, v3(x, -0.02f, -0.62f), v3(x, -0.02f, -1.28f), 0.24f, 0.30f, 7, accent);
        cd_disc(m, v3(x, -0.02f, -1.285f), v3(0, 0, -1), 0.29f, 7, dark);
        cd_ring(m, v3(x, -0.02f, -1.30f), v3(0, 0, 1), 0.31f, 0.055f, 8, 4, emis(glow, 1.0f), 4);
        cd_icosphere(m, v3(x, -0.02f, -1.34f), 0.20f, 1, emis(glow, 1.0f));
        (void)c;
    }

    // Skids (hover pads) under the hull.
    for (int s = -1; s <= 1; s += 2) {
        float x = 0.42f * (float)s;
        cd_box(m, v3(x, -0.34f, 0.10f), v3(0.10f, 0.05f, 0.70f), dark);
        cd_beam(m, v3(x, -0.40f, 0.78f), v3(x, -0.40f, -0.58f), 0.05f, emis(glow, 0.85f));
    }

    // Underside hover-field emitter.
    cd_disc(m, v3(0.f, -0.31f, -0.05f), v3(0, -1, 0), 0.42f, 8, emis(v4(0.35f, 0.95f, 1.0f, 0.75f), 0.75f));
}

// ---------------------------------------------------------------------------
// Tower block: stacked prisms, neon bands, antenna. Height in metres.
// ---------------------------------------------------------------------------
void cd_kit_building(cd_mesh* m, rng_t* rng, vec3 base, float w, float h, float d, const cd_palette* p) {
    int tiers = rng_int(rng, 2, 4);
    float y = base.y;
    float cw = w, cd_ = d;
    vec4 neon = (rng_int(rng, 0, 2) == 0) ? p->glow : emis(v4(1.0f, 0.20f, 0.75f, 1.f), 1.f);
    for (int t = 0; t < tiers; t++) {
        float th = h / tiers * rng_range(rng, 0.7f, 1.3f);
        float shrink = 1.f - 0.16f * (float)t;
        vec3 c = v3(base.x, y + th * 0.5f, base.z);
        vec3 half = v3(cw * shrink * 0.5f, th * 0.5f, cd_ * shrink * 0.5f);
        cd_box(m, c, half, p->body);
        // vertical neon corners
        cd_beam(m, v3(c.x - half.x, y, c.z - half.z), v3(c.x - half.x, y + th, c.z - half.z), 0.09f, emis(neon, 0.9f));
        cd_beam(m, v3(c.x + half.x, y, c.z + half.z), v3(c.x + half.x, y + th, c.z + half.z), 0.09f, emis(neon, 0.9f));
        // horizontal band
        cd_beam(m, v3(c.x - half.x, y + th, c.z + half.z), v3(c.x + half.x, y + th, c.z + half.z), 0.07f, emis(neon, 0.85f));
        cd_beam(m, v3(c.x - half.x, y + th, c.z - half.z), v3(c.x + half.x, y + th, c.z - half.z), 0.07f, emis(neon, 0.85f));
        // window strips on the two faces that face the track
        int rows = (int)(th / 4.5f);
        for (int r = 0; r < rows; r++) {
            float wy = y + 2.0f + (float)r * 4.2f;
            if (wy > y + th - 1.0f) break;
            vec4 wc = (r % 3 == 0) ? emis(v4(0.55f, 1.0f, 0.95f, 0.7f), 0.7f)
                                   : emis(v4(0.35f, 0.55f, 1.0f, 0.45f), 0.45f);
            cd_beam(m, v3(c.x - half.x, wy, base.z + cd_ * shrink * 0.5f + 0.02f),
                    v3(c.x + half.x, wy, base.z + cd_ * shrink * 0.5f + 0.02f), 0.10f, wc);
            cd_beam(m, v3(base.x + cw * shrink * 0.5f + 0.02f, wy, c.z - half.z),
                    v3(base.x + cw * shrink * 0.5f + 0.02f, wy, c.z + half.z), 0.10f, wc);
        }
        y += th;
        cw *= 0.86f;
        cd_ *= 0.86f;
    }
    // Antenna / beacon
    if (rng_f(rng) > 0.45f) {
        float ah = rng_range(rng, 3.f, 9.f);
        cd_beam(m, v3(base.x, y, base.z), v3(base.x, y + ah, base.z), 0.14f, p->dark);
        cd_icosphere(m, v3(base.x, y + ah, base.z), 0.34f, 0, emis(v4(1.f, 0.25f, 0.35f, 1.f), 1.f));
    }
}

// Gateway arch the player flies through.
void cd_kit_arch(cd_mesh* m, vec3 base, float height, float width, const cd_palette* p) {
    float half = width * 0.5f;
    float leg = height * 0.72f;
    for (int s = -1; s <= 1; s += 2) {
        float x = half * (float)s;
        cd_box(m, v3(base.x + x, base.y + leg * 0.5f, base.z), v3(0.55f, leg * 0.5f, 0.55f), p->body);
        cd_beam(m, v3(base.x + x, base.y, 0.55f + base.z), v3(base.x + x, base.y + leg, 0.55f + base.z), 0.10f, emis(p->glow, 1.f));
    }
    // Curved top: 5 faceted segments
    int seg = 6;
    for (int i = 0; i < seg; i++) {
        float t0 = (float)i / seg, t1 = (float)(i + 1) / seg;
        float x0 = lerpf(-half, half, t0), x1 = lerpf(-half, half, t1);
        float y0 = base.y + leg + sinf(t0 * CD_PI) * (height - leg);
        float y1 = base.y + leg + sinf(t1 * CD_PI) * (height - leg);
        cd_beam(m, v3(base.x + x0, y0, base.z), v3(base.x + x1, y1, base.z), 0.62f, p->accent);
        cd_beam(m, v3(base.x + x0, y0 - 0.40f, base.z), v3(base.x + x1, y1 - 0.40f, base.z), 0.13f,
                emis(p->glow, 1.f));
    }
}

// Energy orb pickup: faceted core + orbit rings.
void cd_kit_energy_orb(cd_mesh* m, float radius, vec4 col) {
    cd_icosphere(m, v3(0, 0, 0), radius, 1, emis(v4(col.x, col.y, col.z, 1.f), 1.f));
    cd_ring(m, v3(0, 0, 0), v3(0, 1, 0), radius * 1.45f, radius * 0.10f, 10, 3, emis(v4(1, 1, 1, 0.9f), 0.9f), 4);
    cd_ring(m, v3(0, 0, 0), v3(1, 0.35f, 0), radius * 1.25f, radius * 0.08f, 9, 3, emis(v4(col.x, 1.f, col.z, 0.85f), 0.85f), 4);
}

// Hazard crystal cluster (obstacle).
void cd_kit_crystal(cd_mesh* m, rng_t* rng, float height, vec4 col, vec4 tip) {
    int shards = rng_int(rng, 3, 5);
    for (int i = 0; i < shards; i++) {
        float a = (float)i / shards * CD_TAU + rng_range(rng, -0.3f, 0.3f);
        float r = (i == 0) ? 0.f : rng_range(rng, 0.45f, 1.15f);
        float hh = height * (i == 0 ? 1.f : rng_range(rng, 0.45f, 0.8f));
        vec3 c = v3(cosf(a) * r, 0.f, sinf(a) * r);
        float rad = rng_range(rng, 0.28f, 0.5f) * (i == 0 ? 1.3f : 1.f);
        // tapered shard = prism with nearly-zero top radius
        cd_cylinder(m, v3(c.x, c.y, c.z), v3(c.x + rng_range(rng, -0.2f, 0.2f), hh, c.z), rad, 0.02f, 5, col);
        cd_cylinder(m, v3(c.x, 0.f, c.z), v3(c.x, hh * 0.25f, c.z), rad, rad * 0.85f, 5, tip);
    }
}

// Pylon: roadside light post with a neon blade.
void cd_kit_pylon(cd_mesh* m, float height, const cd_palette* p) {
    cd_cylinder(m, v3(0, 0, 0), v3(0, height, 0), 0.30f, 0.16f, 6, p->dark);
    cd_beam(m, v3(0, height, 0), v3(0, height + 1.6f, 0), 0.22f, emis(p->glow, 1.f));
    cd_box(m, v3(0, height * 0.55f, 0), v3(0.34f, 0.10f, 0.34f), emis(p->accent, 0.7f));
}

// Canyon wall segment: a sealed low-poly ridge running along -Z from z0.
// Height and stance come from smooth functions of world Z so neighbouring
// segments tile without cliffs or zig-zag seams.
static float wall_noise(float z, float seed) {
    return 0.55f * sinf(z * 0.021f + seed) + 0.30f * sinf(z * 0.047f + seed * 2.3f) +
           0.15f * sinf(z * 0.093f + seed * 4.1f);
}

void cd_kit_canyon_wall(cd_mesh* m, rng_t* rng, float length, float height, float side,
                        const cd_palette* p, float z0) {
    (void)rng;
    const int steps = 4;
    const float seglen = length / (float)steps;
    float in_x[steps + 1], in_h[steps + 1], out_x[steps + 1], out_h[steps + 1];
    for (int i = 0; i <= steps; i++) {
        float z = z0 - (float)i * seglen;
        float n = wall_noise(z, side > 0.f ? 2.7f : 8.1f);
        in_x[i] = side * (WALL_INNER_X + 1.2f + 1.0f * n);
        in_h[i] = height * (0.72f + 0.20f * n);
        out_x[i] = side * (WALL_OUTER_X + 3.0f * n);
        out_h[i] = in_h[i] * (0.44f + 0.10f * n);
    }
    for (int i = 0; i < steps; i++) {
        float za = z0 - (float)i * seglen;
        float zb = z0 - (float)(i + 1) * seglen;
        vec3 a = v3(in_x[i], 0.f, zb);          // foot, far end
        vec3 b = v3(in_x[i], 0.f, za);          // foot, near end
        vec3 c = v3(in_x[i + 1], in_h[i + 1], zb);
        vec3 d = v3(in_x[i], in_h[i], za);
        vec3 e = v3(out_x[i], out_h[i], za);
        vec3 f = v3(out_x[i + 1], out_h[i + 1], zb);
        // inner face, facing the track
        cd_quad(m, a, b, d, c, p->body);
        // top ledge from the crest back to the outer shoulder
        cd_quad(m, d, e, f, c, p->dark);
        // outer flank down to the ground
        cd_quad(m, e, b, a, f, p->dark);
        cd_tri(m, f, a, c, p->dark);
        // two glowing strata running along the inner face
        for (int k = 1; k <= 2; k++) {
            float t = 0.18f + 0.30f * (float)k;
            vec4 col = (k == 1) ? emis(v4(0.10f, 1.0f, 0.90f, 0.90f), 0.90f)
                                : emis(v4(1.0f, 0.20f, 0.80f, 0.75f), 0.75f);
            cd_beam(m, v3(a.x - side * 0.06f, in_h[i + 1] * t, zb),
                    v3(b.x - side * 0.06f, in_h[i] * t, za), 0.22f, col);
        }
        // glowing crest line on the ledge
        cd_beam(m, v3(d.x, d.y + 0.06f, za), v3(c.x, c.y + 0.06f, zb), 0.30f,
                emis(p->glow, 0.95f));
    }
}

// Rock: irregular icosahedron deformed by rng.
void cd_kit_rock(cd_mesh* m, rng_t* rng, float size, vec4 col) {
    cd_mesh tmp;
    cd_mesh_init(&tmp);
    cd_icosphere(&tmp, v3(0, 0, 0), size, 0, col);
    for (int i = 0; i < tmp.count; i++) {
        cd_vertex* v = &tmp.v[i];
        float k = 0.72f + rng_range(rng, 0.f, 0.55f);
        v->px *= k;
        v->py *= k * 0.8f;
        v->pz *= k;
        m->v ? (void)0 : (void)0;
        cd_mesh_reserve(m, 1);
        m->v[m->count++] = *v;
    }
    cd_mesh_free(&tmp);
}

// Flat ground tile with glowing grid seams (track floor).
void cd_kit_ground_plate(cd_mesh* m, float width, float length, const cd_palette* p) {
    cd_quad(m, v3(-width * 0.5f, 0, 0), v3(-width * 0.5f, 0, -length),
            v3(width * 0.5f, 0, -length), v3(width * 0.5f, 0, 0), p->body);
    // edge neon rails
    cd_beam(m, v3(-width * 0.5f + 0.4f, 0.05f, 0), v3(-width * 0.5f + 0.4f, 0.05f, -length), 0.20f,
            emis(p->glow, 0.9f));
    cd_beam(m, v3(width * 0.5f - 0.4f, 0.05f, 0), v3(width * 0.5f - 0.4f, 0.05f, -length), 0.20f,
            emis(p->glow, 0.9f));
    // centre dashes
    for (float z = -2.f; z > -length; z -= 9.f) {
        cd_beam(m, v3(0, 0.04f, z), v3(0, 0.04f, z - 3.2f), 0.16f, emis(v4(0.55f, 0.35f, 1.f, 0.55f), 0.55f));
    }
}

// Wide low-poly terrain slab used for the canyon floor / distant mesas.
void cd_kit_world_plate(cd_mesh* m, float size, int res, rng_t* rng, vec4 low, vec4 high) {
    float step = size / (float)res;
    for (int i = 0; i < res; i++) {
        for (int j = 0; j < res; j++) {
            float x0 = -size * 0.5f + (float)i * step;
            float x1 = x0 + step;
            float z0 = -size * 0.5f + (float)j * step;
            float z1 = z0 + step;
            float y00 = rng_range(rng, -0.6f, 0.6f);
            float y10 = rng_range(rng, -0.6f, 0.6f);
            float y01 = rng_range(rng, -0.6f, 0.6f);
            float y11 = rng_range(rng, -0.6f, 0.6f);
            vec4 c = ((i + j) & 1) ? low : high;
            cd_quad(m, v3(x0, y00, z0), v3(x1, y10, z0), v3(x1, y11, z1), v3(x0, y01, z1), c);
        }
    }
}
