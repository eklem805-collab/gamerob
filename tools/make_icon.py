#!/usr/bin/env python3
"""Generate the launcher icon + Play-style art procedurally (no binary assets in git).

Produces app/res/mipmap-*/ic_launcher.png at the five standard densities.
"""
import math
import os
import struct
import sys
import zlib

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SIZES = {"mdpi": 48, "hdpi": 72, "xhdpi": 96, "xxhdpi": 144, "xxxhdpi": 192}


def lerp(a, b, t):
    return a + (b - a) * t


def clamp(v, lo=0.0, hi=1.0):
    return lo if v < lo else (hi if v > hi else v)


def render(size):
    """Supersampled synthwave icon: sun, grid horizon, hovercraft silhouette."""
    ss = 3
    n = size * ss
    buf = [[(0.0, 0.0, 0.0, 0.0) for _ in range(n)] for _ in range(n)]

    for y in range(n):
        for x in range(n):
            u = (x + 0.5) / n          # 0..1
            v = (y + 0.5) / n
            cx, cy = u - 0.5, v - 0.5
            r = math.hypot(cx, cy)

            # rounded-square background
            rad = 0.14
            bx = max(abs(cx) - (0.5 - rad), 0.0)
            by = max(abs(cy) - (0.5 - rad), 0.0)
            inside = math.hypot(bx, by) <= rad
            if not inside:
                continue

            # sky gradient (deep violet -> magenta horizon)
            t = clamp((v - 0.05) / 0.72)
            col = (lerp(0.05, 0.42, t ** 1.4), lerp(0.01, 0.06, t), lerp(0.16, 0.42, t ** 1.2))

            # sun disc with scanline bands
            sy = v - 0.34
            if sy > 0:
                sr = 0.255
                if r < sr:
                    sun = (lerp(1.0, 0.55, clamp(sy / sr)), lerp(0.72, 0.10, clamp(sy / sr)),
                           lerp(0.20, 0.35, clamp(sy / sr)))
                    band = 1.0
                    if sy > sr * 0.15:
                        band = 1.0 if math.sin(sy * 150.0) > -0.2 else 0.0
                    k = 0.95 * band
                    col = tuple(lerp(col[i], sun[i], k) for i in range(3))
                    col = tuple(clamp(c + (0.35 if r < sr * 1.12 and r > sr * 0.98 else 0.0)) for c in col)

            # perspective grid floor
            if v > 0.60:
                horizon = 0.60
                dz = (v - horizon) / (0.5 - 0.05)
                # horizontal lines
                hl = abs(math.sin(dz * 26.0))
                if hl > 0.93:
                    col = tuple(lerp(c, 0.15, 0.55) for c in col)
                    col = (0.15, 1.0, 0.85) if hl > 0.96 else col
                # radial lines converging to vanishing point
                ang = math.atan2(cx, max(1e-5, v - horizon))
                if abs(math.sin(ang * 9.0)) > 0.965:
                    col = (1.0, 0.25, 0.80)
                col = tuple(clamp(c * (0.55 + 0.55 * dz)) for c in col)
            else:
                # ground haze
                col = tuple(clamp(c * 0.85) for c in col)

            # hovercraft silhouette: arrowhead seen from behind
            cxp = cx
            cyp = v - 0.46
            body = False
            if -0.13 < cyp < 0.13:
                # nose at the top (small v), wings widest at the bottom
                f = (cyp + 0.13) / 0.26
                half_w = 0.035 + 0.30 * (f ** 0.75)
                # tail notch: cut a V out of the rear centre
                notch = 0.115 * clamp((f - 0.55) / 0.45)
                if abs(cxp) < half_w and not (abs(cxp) < notch and f > 0.55):
                    body = True
            if body:
                col = (0.07, 0.10, 0.22)
                # neon wing edges
                f = (cyp + 0.13) / 0.26
                half_w = 0.035 + 0.30 * (f ** 0.75)
                if abs(abs(cxp) - half_w) < 0.028:
                    col = (1.0, 0.30, 0.82)
                if abs(cyp + 0.13) < 0.022:
                    col = (0.20, 1.0, 0.90)
            # cockpit canopy
            if math.hypot(cxp * 2.6, (cyp + 0.055) * 3.4) < 0.20:
                col = (0.60, 0.98, 1.0)
            # twin engine glow at the tail
            for sx in (-1, 1):
                if math.hypot(cxp - sx * 0.175, cyp - 0.10) < 0.045:
                    col = (1.0, 0.72, 0.25)
            buf[y][x] = (col[0], col[1], col[2], 1.0)

    # downsample
    out = bytearray()
    for y in range(size):
        out.append(0)  # filter type 0
        for x in range(size):
            r = g = b = a = 0.0
            for j in range(ss):
                for i in range(ss):
                    px = buf[y * ss + j][x * ss + i]
                    r += px[0]; g += px[1]; b += px[2]; a += px[3]
            k = 1.0 / (ss * ss)
            # premultiplied-ish edge smoothing
            af = a * k
            if af > 0.001:
                out += bytes((int(clamp(r * k / af) * 255), int(clamp(g * k / af) * 255),
                              int(clamp(b * k / af) * 255), int(clamp(af) * 255)))
            else:
                out += bytes((int(clamp(r * k) * 255), int(clamp(g * k) * 255),
                              int(clamp(b * k) * 255), 0))
    return bytes(out), size


def png(path, raw, size):
    def chunk(tag, data):
        c = struct.pack(">I", len(data)) + tag + data
        return c + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)

    ihdr = struct.pack(">IIBBBBB", size, size, 8, 6, 0, 0, 0)
    data = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", ihdr) + \
        chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b"")
    with open(path, "wb") as f:
        f.write(data)


def main():
    for dens, size in SIZES.items():
        d = os.path.join(ROOT, "app", "res", f"mipmap-{dens}")
        os.makedirs(d, exist_ok=True)
        raw, sz = render(size)
        png(os.path.join(d, "ic_launcher.png"), raw, sz)
        print(f"wrote mipmap-{dens}/ic_launcher.png ({size}x{size})")

    # store a large copy for documentation / store listing
    art = os.path.join(ROOT, "docs")
    os.makedirs(art, exist_ok=True)
    raw, sz = render(512)
    png(os.path.join(art, "icon-512.png"), raw, sz)
    print("wrote docs/icon-512.png")
    return 0


if __name__ == "__main__":
    sys.exit(main())
