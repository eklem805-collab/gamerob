#!/usr/bin/env python3
"""Software rasterizer for Cyber Drift GL traces (tools/gltrace.c output).

Implements the same shading math as the GLSL ES shaders in src/cd_render.c
(neon lighting + fresnel + fog, sky gradient + sun, UI atlas quads with the
alpha-blend the engine requests), so the preview reflects the real engine
draw path: geometry, matrices, vertex colours and draw order all come from the
engine, not from this file.

usage: rasterize.py <trace> <out.png> [width] [height]
"""
import math
import struct
import sys
import zlib

import numpy as np

# ------------------------------------------------------------------ trace io
def read_trace(path):
    d = open(path, "rb").read()
    assert d[:8] == b"CDTRACE1", "bad trace magic"
    p = 8
    aw, ah, abytes = struct.unpack_from("<iii", d, p)
    p += 12
    atlas = None
    if abytes:
        atlas = np.frombuffer(d[p:p + abytes], dtype=np.uint8).reshape(ah, aw, 4).copy()
        p += abytes
    (ndraws,) = struct.unpack_from("<i", d, p)
    p += 4
    draws = []
    for _ in range(ndraws):
        kind, count, stride, textured = struct.unpack_from("<iiii", d, p)
        p += 16
        (blend,) = struct.unpack_from("<i", d, p)
        p += 4
        mvp = struct.unpack_from("<16f", d, p); p += 64
        model = struct.unpack_from("<16f", d, p); p += 64
        tint = struct.unpack_from("<4f", d, p); p += 16
        cam = struct.unpack_from("<3f", d, p); p += 12
        (tm,) = struct.unpack_from("<f", d, p); p += 4
        fog = struct.unpack_from("<2f", d, p); p += 8
        vbytes = count * stride
        verts = np.frombuffer(d[p:p + vbytes], dtype=np.float32).reshape(count, stride // 4).copy()
        p += vbytes
        draws.append(dict(kind=kind, blend=blend, count=count, mvp=np.array(mvp, dtype=np.float32),
                          model=np.array(model, dtype=np.float32), tint=tint, cam=cam,
                          time=tm, fog=fog, verts=verts, textured=textured))
    return atlas, draws


# ------------------------------------------------------------------- math
def look_at_gltf(eye, target, up=(0, 1, 0)):
    """Camera basis [-right, up, -fwd] (see src/cd_math.h)."""
    f = np.array(target, float) - np.array(eye, float)
    f /= np.linalg.norm(f)
    s = np.cross(f, np.array(up, float))
    s /= np.linalg.norm(s)
    u = np.cross(s, f)
    m = np.eye(4)
    m[0, :3] = -s
    m[1, :3] = u
    m[2, :3] = -f
    m[0, 3] = np.dot(s, eye)
    m[1, 3] = -np.dot(u, eye)
    m[2, 3] = np.dot(f, eye)
    return m


def perspective(fovy, aspect, znear, zfar):
    f = 1.0 / math.tan(fovy * 0.5)
    m = np.zeros((4, 4))
    m[0, 0] = f / aspect
    m[1, 1] = f
    m[2, 2] = (zfar + znear) / (znear - zfar)
    m[2, 3] = (2 * zfar * znear) / (znear - zfar)
    m[3, 2] = -1.0
    return m


def ortho(l, r, b, t, n, f):
    m = np.eye(4)
    m[0, 0] = 2 / (r - l)
    m[1, 1] = 2 / (t - b)
    m[2, 2] = -2 / (f - n)
    m[0, 3] = -(r + l) / (r - l)
    m[1, 3] = -(t + b) / (t - b)
    m[2, 3] = -(f + n) / (f - n)
    return m


# --------------------------------------------------------------- shading
def sky_shade(dirv, t):
    """Port of FS_SKY."""
    h = np.clip(dirv[..., 1], -1, 1)
    top = np.array([0.04, 0.01, 0.13])
    mid = np.array([0.36, 0.05, 0.38])
    hor = np.array([1.00, 0.34, 0.42])
    s1 = np.clip((h + 0.02) / 0.28, 0, 1); s1 = s1 * s1 * (3 - 2 * s1)
    col = hor[None, :] + (mid - hor)[None, :] * s1[..., None]
    s2 = np.clip((h - 0.22) / 0.73, 0, 1); s2 = s2 * s2 * (3 - 2 * s2)
    col = col + (top - col) * s2[..., None]
    with np.errstate(divide="ignore", invalid="ignore"):
        denom = np.maximum(0.18, np.abs(dirv[..., 1]) + 0.62)
        sp = np.stack([dirv[..., 0] / denom, dirv[..., 2] / denom], -1)
    r = np.sqrt((sp ** 2).sum(-1))
    sun = np.clip((r - 0.50) / (0.44 - 0.50), 0, 1)
    sun = sun * sun * (3 - 2 * sun)
    band = (np.sin(dirv[..., 1] * 70.0 + 1.2) >= 0).astype(float)
    sy = np.clip((-0.10 - dirv[..., 1]) / 0.20, 0, 1)
    stripes = 1.0 + (band - 1.0) * sy
    k = np.clip(0.55 - dirv[..., 1] * 1.6, 0, 1)
    suncol = np.array([1.0, 0.88, 0.30])[None, :] * (1 - k)[..., None] + \
             np.array([1.0, 0.18, 0.55])[None, :] * k[..., None]
    m = (sun * stripes * 0.95)[..., None]
    col = col * (1 - m) + suncol * m
    col = col + suncol * (0.10 * sun)[..., None]
    col = col + np.array([0.55, 0.10, 0.35])[None, :] * (np.exp(-np.abs(h) * 9.0) * 0.55)[..., None]
    low = np.clip((0.05 - h) / 0.30, 0, 1); low = low * low * (3 - 2 * low)
    col = col + np.array([0.10, 0.25, 0.45])[None, :] * (low * 0.25)[..., None]
    return col


def neon_shade(world, nrm, col, emit, cam, time, fogrange, tint):
    """Port of FS_NEON."""
    L1 = np.array([0.35, 0.90, 0.25]); L1 = L1 / np.linalg.norm(L1)
    L2 = np.array([-0.70, 0.35, -0.55]); L2 = L2 / np.linalg.norm(L2)
    C1 = np.array([1.00, 0.45, 0.85])
    C2 = np.array([0.15, 0.90, 1.00])
    # double sided lighting: face the normal toward the camera
    tocam = cam[None, :] - world
    flip = ((nrm * tocam).sum(1) < 0)[:, None]
    nrm = np.where(flip, -nrm, nrm)
    d1 = np.maximum(nrm @ L1, 0)[:, None]
    d2 = np.maximum(nrm @ L2, 0)[:, None]
    amb = np.array([0.42, 0.34, 0.62])
    e = np.clip(emit, 0, 1)[:, None]
    base = col
    lit = base * (amb[None, :] + d1 * C1[None, :] * 1.35 + d2 * C2[None, :] * 0.85)
    vdir = cam[None, :] - world
    vdir /= np.maximum(np.linalg.norm(vdir, axis=1, keepdims=True), 1e-6)
    fres = np.power(np.clip(1.0 - np.abs((nrm * vdir).sum(1)), 0, 1), 2.5)[:, None]
    out = lit * (1 - e) + base * 2.30 * e
    out = out + base * e * (0.30 + 0.10 * math.sin(time * 3.0))
    out = out + np.array([0.16, 0.60, 0.75])[None, :] * fres * 0.85
    out = out + base * 0.16
    out = out * np.array(tint[:3])[None, :]
    dist = np.linalg.norm(world - cam[None, :], axis=1)
    f0, f1 = fogrange
    fog = np.clip((dist - f0) / max(1e-3, (f1 - f0)), 0, 1)
    fog = fog * fog * (3 - 2 * fog)
    fogc = np.array([0.10, 0.03, 0.22])[None, :] * (1 - np.clip(world[:, 1:2] * 0.02, 0, 1)) + \
           np.array([0.30, 0.05, 0.35])[None, :] * np.clip(world[:, 1:2] * 0.02, 0, 1)
    out = out * (1 - fog[:, None] * 0.92) + fogc * (fog[:, None] * 0.92)
    return np.clip(out, 0, 1)


# ----------------------------------------------------------- raster core
def rasterize_tris(tris, W, H, cull_double_sided=True):
    """tris: list of (v0,v1,v2 screen coords, depth, attr arrays)."""
    pass


DEBUG_BBOX = False


def _clip_half(poly, axis, bound, keep_greater):
    """Sutherland-Hodgman against one half-plane. `poly` is a list of tuples
    whose first two entries are x and y; every channel is interpolated."""
    if not poly:
        return []
    def dist(p):
        d = p[axis] - bound
        return d if keep_greater else -d

    out = []
    n = len(poly)
    for i in range(n):
        a, b = poly[i], poly[(i + 1) % n]
        da, db = dist(a), dist(b)
        if da >= 0.0:
            out.append(a)
            if db < 0.0:
                t = da / (da - db)
                out.append(tuple(a[k] + (b[k] - a[k]) * t for k in range(len(a))))
        elif db >= 0.0:
            t = da / (da - db)
            out.append(tuple(a[k] + (b[k] - a[k]) * t for k in range(len(a))))
    return out


def clip_verts(poly, W, H):
    """Clip a polygon to the viewport; returns a fan of triangles."""
    pts = poly
    pts = _clip_half(pts, 0, 0.0, True)
    pts = _clip_half(pts, 0, float(W - 1), False)
    pts = _clip_half(pts, 1, 0.0, True)
    pts = _clip_half(pts, 1, float(H - 1), False)
    if len(pts) < 3:
        return []
    return [[pts[0], pts[i], pts[i + 1]] for i in range(1, len(pts) - 1)]


def draw_mesh(draw, W, H, atlas, depth, color, is_ui, ss=1, ualpha=None):
    blend = draw.get("blend", 0)
    kind = draw["kind"]
    V = draw["verts"].astype(np.float64)
    # Per-vertex channel layout after screen mapping:
    #   neon: x, y, z, world(3), normal(3), colour(4)
    #   ui:   x, y, z, uv(2), colour(4)
    if kind == 2:
        poss = V[:, 0:2]
        uvs = V[:, 2:4]
        cols = V[:, 4:8]
        mvp = draw["mvp"].reshape(4, 4).T
        ndc = np.concatenate([poss, np.zeros((len(V), 1)) - 1.0, np.ones((len(V), 1))], 1) @ mvp.T
        screen = np.stack([(ndc[:, 0] * 0.5 + 0.5) * W, (1.0 - (ndc[:, 1] * 0.5 + 0.5)) * H], 1) * ss
        zbuf = np.zeros(len(V))
        chan = np.concatenate([screen, zbuf[:, None], uvs, cols], 1)
    else:
        pos = V[:, 0:3]
        nrm = V[:, 3:6]
        cols = V[:, 6:10]
        model = draw["model"].reshape(4, 4).T
        mvp = draw["mvp"].reshape(4, 4).T
        wpos4 = np.concatenate([pos, np.ones((len(V), 1))], 1) @ model.T
        world = wpos4[:, :3]
        nm = model[:3, :3]
        nworld = nrm @ np.linalg.inv(nm)
        nworld /= np.maximum(np.linalg.norm(nworld, axis=1, keepdims=True), 1e-9)
        clip = np.concatenate([pos, np.ones((len(V), 1))], 1) @ mvp.T
        # near-plane clip in clip space (w > eps), interpolating attributes
        w = clip[:, 3]
        keep = w > 1e-4
        chan = np.concatenate([clip, world, nworld, cols], 1)
        tri = []
        for t in range(len(V) // 3):
            cv = chan[3 * t:3 * t + 3]
            inside = keep[3 * t:3 * t + 3]
            n = int(inside.sum())
            if n == 0:
                continue
            if n == 3:
                tri.append(cv)
                continue
            if n == 1:
                i = int(np.argmax(inside)); j, k = (i + 1) % 3, (i + 2) % 3
                tj = (1e-4 - cv[i, 3]) / (cv[j, 3] - cv[i, 3])
                tk = (1e-4 - cv[i, 3]) / (cv[k, 3] - cv[i, 3])
                tri.append(np.stack([cv[i], cv[i] + (cv[j] - cv[i]) * tj, cv[i] + (cv[k] - cv[i]) * tk]))
            else:
                i = int(np.argmin(inside)); j, k = (i + 1) % 3, (i + 2) % 3
                tj = (1e-4 - cv[j, 3]) / (cv[i, 3] - cv[j, 3])
                tk = (1e-4 - cv[j, 3]) / (cv[k, 3] - cv[j, 3])
                a = cv[j] + (cv[i] - cv[j]) * tj
                b = cv[j] + (cv[k] - cv[j]) * tk
                tri.append(np.stack([cv[j], b, a]))
                tri.append(np.stack([a, b, cv[k]]))
        if not tri:
            return
        chan = np.concatenate(tri)
        cw = np.maximum(chan[:, 3], 1e-6)
        ndc = chan[:, 0:3] / cw[:, None]
        screen = np.stack([(ndc[:, 0] * 0.5 + 0.5) * W, (1.0 - (ndc[:, 1] * 0.5 + 0.5)) * H], 1)
        chan = np.concatenate([screen, ndc[:, 2:3], chan[:, 4:]], 1)

    # Clip every triangle to the viewport rect: near-plane clipping can leave
    # vertices millions of pixels away, and scan conversion of those loses all
    # precision in the barycentrics (which produced screen-filling garbage).
    tris = []
    for t in range(len(chan) // 3):
        poly = [tuple(row) for row in chan[3 * t:3 * t + 3]]
        if not all(all(math.isfinite(v) for v in row) for row in poly):
            continue
        tris.extend(clip_verts(poly, W * ss, H * ss))

    for tri in tris:
        s = np.asarray([(v[0], v[1]) for v in tri], dtype=np.float64)
        at = np.asarray(tri, dtype=np.float64)
        z = at[:, 2]
        x0, y0 = s[0]; x1, y1 = s[1]; x2, y2 = s[2]
        area = (x1 - x0) * (y2 - y0) - (x2 - x0) * (y1 - y0)
        if abs(area) < 1e-9:
            continue
        minx = max(0, int(math.floor(min(x0, x1, x2))))
        maxx = min(W * ss - 1, int(math.ceil(max(x0, x1, x2))))
        miny = max(0, int(math.floor(min(y0, y1, y2))))
        maxy = min(H * ss - 1, int(math.ceil(max(y0, y1, y2))))
        if maxx < minx or maxy < miny:
            continue
        # Scan convert in horizontal bands: a full-screen quad at 3x UI
        # supersampling would otherwise need a gigabyte of temporaries.
        for by0 in range(miny, maxy + 1, TILE_ROWS):
            by1 = min(maxy, by0 + TILE_ROWS - 1)
            raster_span(tri=s, attrib=at, area=area, W=W, H=H, ss=ss, kind=kind,
                        blend=blend, x0=x0, y0=y0, x1=x1, y1=y1, x2=x2, y2=y2,
                        z=z, minx=minx, maxx=maxx, miny=by0, maxy=by1,
                        depth=depth, color=color, atlas=atlas, ualpha=ualpha,
                        draw=draw)


TILE_ROWS = 64


def raster_span(tri, attrib, area, W, H, ss, kind, blend, x0, y0, x1, y1, x2, y2,
                z, minx, maxx, miny, maxy, depth, color, atlas, ualpha, draw):
    ys, xs = np.mgrid[miny:maxy + 1, minx:maxx + 1]
    px = xs + 0.5
    py = ys + 0.5
    w0 = ((x1 - px) * (y2 - py) - (x2 - px) * (y1 - py)) / area
    w1 = ((x2 - px) * (y0 - py) - (x0 - px) * (y2 - py)) / area
    w2 = 1.0 - w0 - w1
    inside = (w0 >= 0) & (w1 >= 0) & (w2 >= 0)
    if not inside.any():
        return
    zz = w0 * z[0] + w1 * z[1] + w2 * z[2]
    sub_depth = depth[miny:maxy + 1, minx:maxx + 1]
    mask = inside if kind == 2 else (inside & (zz < sub_depth))
    if not mask.any():
        return
    sub_color = color[miny:maxy + 1, minx:maxx + 1]
    attr = (w0[..., None] * attrib[0] + w1[..., None] * attrib[1] + w2[..., None] * attrib[2])
    if kind == 2:
        uu = attr[..., 3]
        vv = attr[..., 4]
        cc = attr[..., 5:9]
        # The UI pass is already supersampled (SS=3), so plain point sampling of
        # the 32x32 glyph cell is enough; jittering the UVs would pull in
        # neighbouring glyphs and produce ghosting that the device never shows.
        tx = np.clip((uu * (atlas.shape[1] - 1)).astype(int), 0, atlas.shape[1] - 1)
        ty = np.clip((vv * (atlas.shape[0] - 1)).astype(int), 0, atlas.shape[0] - 1)
        tex = atlas[ty, tx].astype(np.float32) / 255.0
        a = cc[..., 3] * tex[..., 3]
        a = np.where(a < 0.04, 0.0, a)
        rgb = cc[..., :3] * (0.85 + 0.55 * tex[..., 0])[..., None]
        m3 = mask[..., None]
        blended = sub_color * (1 - a[..., None]) + np.clip(rgb, 0, 1) * a[..., None]
        sub_color[:] = np.where(m3, blended, sub_color)
        if ualpha is not None:
            sub_a = ualpha[miny:maxy + 1, minx:maxx + 1]
            sub_a[:] = np.where(mask, sub_a + a * (1 - sub_a), sub_a)
        return
    world = attr[..., 3:6]
    nworld = attr[..., 6:9]
    ccol = attr[..., 9:13]
    wn = nworld / np.maximum(np.linalg.norm(nworld, axis=-1, keepdims=True), 1e-9)
    sh = np.zeros(world.shape[:2] + (3,))
    sel = mask
    if sel.any():
        sh[sel] = neon_shade(world[sel], wn[sel], ccol[sel][:, :3], ccol[sel][:, 3],
                             np.array(draw["cam"]), draw["time"], draw["fog"], draw["tint"])
    m3 = mask[..., None]
    if blend == 2:            # additive glow: no depth write
        sub_color[:] = np.clip(sub_color + sh * m3, 0, 1)
    elif blend == 1:          # alpha blended with the vertex alpha
        a = np.clip(ccol[:, 3], 0, 1)[..., None]
        sub_color[:] = np.where(m3, sub_color * (1 - a) + sh * a, sub_color)
    else:
        sub_color[:] = np.where(m3, sh, sub_color)
        sub_depth[mask] = zz[mask]


def cull_ok(area, kind):
    """The engine renders every pass with culling disabled and the shader faces
    normals toward the camera, so nothing is rejected here."""
    return True


def text_mode_from_uv(u0, v0):
    return None


def render(trace_path, out_path, W=1280, H=800, fov_deg=58.0, cam_pos=None, look=None, report=print, let_view=None, show_ui=True):
    atlas, draws = read_trace(trace_path)
    depth = np.full((H, W), 1e30, dtype=np.float64)
    color = np.zeros((H, W, 3), dtype=np.float64)

    # --- sky (kind 1): the engine draws a camera-centred box; shade per pixel
    sky = [d for d in draws if d["kind"] == 1]
    neon = [d for d in draws if d["kind"] == 0]
    ref = (sky[0] if sky else (neon[0] if neon else draws[0]))
    cam = np.array(cam_pos if cam_pos is not None else ref["cam"], dtype=float)
    rows = np.linspace(1.0, -1.0, H)[:, None]
    cols = np.linspace(-1.0, 1.0, W)[None, :]
    fov = math.radians(fov_deg)
    th = math.tan(fov * 0.5)
    aspect = W / H
    dx = np.broadcast_to(cols * th * aspect, (H, W)).copy()
    dy = np.broadcast_to(rows * th, (H, W)).copy()
    dz = np.full((H, W), -1.0)
    d = np.stack([dx, dy, dz], -1)
    d /= np.linalg.norm(d, axis=-1, keepdims=True)
    # Camera basis, glTF convention (src/cd_math.h). Either supplied by the
    # caller or recovered from the engine camera position + first neon draw.
    fwdv = np.array(look, float) - cam if look is not None else np.array([0.0, 0.0, 1.0])
    fwdv /= np.linalg.norm(fwdv)
    right = np.cross(fwdv, np.array([0.0, 1.0, 0.0])); right /= np.linalg.norm(right)
    up = np.cross(right, fwdv)
    # eye position from the inverse view matrix
    world_dir = (d[..., 0:1] * right[None, None, :] + d[..., 1:2] * up[None, None, :] +
                 (-d[..., 2:3]) * fwdv[None, None, :])
    world_dir /= np.maximum(np.linalg.norm(world_dir, axis=-1, keepdims=True), 1e-9)
    color[:] = sky_shade(world_dir, draws[0]["time"] if draws else 0.0)

    # --- depth-sorted opaque, then blend
    stats = {}
    # kind 1 is the sky dome: its pixels come from sky_shade above, not from
    # rasterising the box (which would only cover the frame with fresnel).
    world = [d for d in draws if d["kind"] == 0]
    ordered = [d for d in world if d.get("blend", 0) == 0] + \
              [d for d in world if d.get("blend", 0) == 1] + \
              [d for d in world if d.get("blend", 0) == 2]
    for d in ordered:
        stats[d["kind"]] = stats.get(d["kind"], 0) + 1
        draw_mesh(d, W, H, atlas, depth, color, False)

    # UI pass at 3x resolution, then resolve: matches the GPU's linear
    # filtering well enough to judge glyph legibility in the preview.
    ui = [d for d in draws if d["kind"] == 2]
    if ui:
        stats[2] = len(ui)
        SS = 3
        uw, uh = W * SS, H * SS
        udepth = np.full((uh, uw), 1e30)
        ucolor = np.zeros((uh, uw, 3))
        ualpha = np.zeros((uh, uw))
        for d in ui:
            draw_mesh(d, W, H, atlas, udepth, ucolor, True, ss=SS, ualpha=ualpha)
        # Resolve the supersampled UI buffer over the world pass.
        ucov = ualpha.reshape(H, SS, W, SS).mean(axis=(1, 3))[..., None]
        urgb = ucolor.reshape(H, SS, W, SS, 3).mean(axis=(1, 3))
        color = color * (1 - ucov) + urgb
    report(f"rasterised {len(draws)} draws (neon={stats.get(0,0)} sky={stats.get(1,0)} ui={stats.get(2,0)})")
    write_png(out_path, (np.clip(color, 0, 1) * 255).astype(np.uint8), W, H)
    report(f"wrote {out_path}")


# ---------------------------------------------------------------- png out
def write_png(path, rgb, W, H):
    raw = b"".join(b"\x00" + rgb[y].tobytes() for y in range(H))

    def chunk(tag, data):
        c = struct.pack(">I", len(data)) + tag + data
        return c + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)

    ihdr = struct.pack(">IIBBBBB", W, H, 8, 2, 0, 0, 0)
    open(path, "wb").write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", ihdr) +
                           chunk(b"IDAT", zlib.compress(raw, 6)) + chunk(b"IEND", b""))


if __name__ == "__main__":
    trace = sys.argv[1]
    out = sys.argv[2]
    w = int(sys.argv[3]) if len(sys.argv) > 3 else 1280
    h = int(sys.argv[4]) if len(sys.argv) > 4 else 800
    cam = None
    look = None
    if len(sys.argv) > 5:
        cam = [float(v) for v in sys.argv[5].split(",")]
        look = [float(v) for v in sys.argv[6].split(",")]
    render(trace, out, w, h, cam_pos=cam, look=look)
