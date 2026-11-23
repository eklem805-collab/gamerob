// Procedural canyon "Cyber Drift" corridor.
// The world extends in +Z; the corridor curves and breathes in width.
// Pure sine sums keep chunk generation stateless and seamless.
#include "cd_game.h"

float cd_track_width_at(float z) {
    float w = 19.0f + 5.0f * sinf(z * 0.0068f) + 2.5f * sinf(z * 0.021f + 1.7f);
    return clampf(w, 12.0f, 27.0f);
}

vec3 cd_track_center(float z) {
    float x = 20.0f * sinf(z * 0.0052f) + 8.0f * sinf(z * 0.0135f + 0.8f) +
              3.0f * sinf(z * 0.031f + 2.2f);
    float y = 6.0f * sinf(z * 0.0075f + 0.4f) + 2.2f * sinf(z * 0.018f + 1.1f);
    return v3(x, y, z);
}

void cd_track_frame(float z, vec3* center, vec3* tangent, float* yaw) {
    vec3 c = cd_track_center(z);
    vec3 c2 = cd_track_center(z + 0.5f);
    vec3 t = v3norm(v3sub(c2, c));
    if (center) *center = c;
    if (tangent) *tangent = t;
    if (yaw) *yaw = atan2f(t.x, t.z);
}

void cd_track_build_layout(cd_track_layout* L, float start_z, int segs, float spacing) {
    L->count = segs + 1;
    for (int i = 0; i <= segs; i++) {
        float z = start_z + (float)i * spacing;
        cd_track_sample* s = &L->samples[i];
        s->s = (float)i * spacing;
        s->half_w = cd_track_width_at(z);
        cd_track_frame(z, &s->center, &s->tangent, NULL);
    }
    L->arc_total = (float)segs * spacing;
}
