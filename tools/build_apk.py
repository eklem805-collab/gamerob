#!/usr/bin/env python3
"""Cyber Drift — end-to-end APK build.

 1. regenerate NDK import stubs            (tools/gen_stubs.py)
 2. cross-compile libcyberdrift.so         (zig cc -> aarch64-linux-android)
 3. link resources + manifest              (aapt2)
 4. assemble aligned APK                   (tools/mkapk.py)
 5. sign with v1 + v2 schemes              (tools/mkapk.py)
 6. verify                                 (aapt2 dump badging + signature parse)
"""
import os
import shutil
import struct
import subprocess
import sys
import time
import zipfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BUILD = os.path.join(ROOT, "build")
APP = os.path.join(ROOT, "app")
sys.path.insert(0, os.path.join(ROOT, "tools"))

import gen_stubs  # noqa: E402
import mkapk      # noqa: E402

ZIG = os.environ.get("ZIG", "zig")
AAPT2 = os.environ.get("AAPT2", os.path.join(ROOT, "tools", "bin", "aapt2"))
ANDROID_JAR = os.environ.get("ANDROID_JAR", os.path.join(ROOT, "third_party", "android.jar"))

APP_ID = "com.cyberdrift.hover"
APP_NAME = "Cyber Drift"
VERSION_CODE = 1
VERSION_NAME = "1.0"
MIN_SDK = 24
TARGET_SDK = 34

SOURCES = [
    "src/cd_gl.c", "src/cd_mesh.c", "src/cd_kit.c", "src/cd_track.c",
    "src/cd_font.c", "src/cd_render.c", "src/cd_game.c", "src/cd_ui.c",
    "src/platform_android.c",
]
LIBS = ["libEGL.so", "libGLESv2.so", "libandroid.so", "liblog.so", "libm.so", "libc.so"]


def run(cmd, **kw):
    print("  $", " ".join(os.path.basename(c) if i == 0 else c for i, c in enumerate(cmd)))
    r = subprocess.run(cmd, capture_output=True, text=True, **kw)
    if r.returncode != 0:
        print(r.stdout[-3000:])
        print(r.stderr[-3000:])
        raise SystemExit(f"FAILED: {' '.join(cmd[:3])} ... (exit {r.returncode})")
    return r


def step_stubs():
    print("[1/6] NDK import stubs")
    gen_stubs.ZIG = ZIG
    if gen_stubs.main() != 0:
        raise SystemExit("stub generation failed")


def step_native():
    print("[2/6] native library (arm64-v8a)")
    out_dir = os.path.join(BUILD, "jni", "arm64-v8a")
    os.makedirs(out_dir, exist_ok=True)
    so = os.path.join(out_dir, "libcyberdrift.so")
    stub = os.path.join(BUILD, "stubs")
    cmd = [ZIG, "cc", "-target", "aarch64-linux-android", "-std=c11", "-O2",
           "-fPIC", "-shared", "-nostdlib", "-fno-stack-protector",
           "-ffunction-sections", "-fdata-sections",
           "-isystem", os.path.join(ROOT, "third_party", "freestanding"),
           "-I", os.path.join(ROOT, "src"),
           "-I", os.path.join(ROOT, "third_party"),
           "-DCD_GL_DIRECT"]
    cmd += [os.path.join(ROOT, s) for s in SOURCES]
    cmd += ["-o", so,
            "-Wl,-soname,libcyberdrift.so",
            "-Wl,--no-undefined",
            "-Wl,-z,max-page-size=16384",
            "-Wl,--gc-sections"]
    cmd += [os.path.join(stub, l) for l in LIBS]
    run(cmd)
    verify_elf(so)
    return so


def verify_elf(path):
    d = open(path, "rb").read()
    assert d[:4] == b"\x7fELF", "not an ELF"
    assert d[4] == 2, "not 64-bit"
    machine = int.from_bytes(d[18:20], "little")
    assert machine == 183, f"unexpected machine {machine} (want 183 = aarch64)"
    # check DT_NEEDED came through
    e_shoff = struct.unpack_from("<Q", d, 0x28)[0]
    e_shentsize = struct.unpack_from("<H", d, 0x3A)[0]
    e_shnum = struct.unpack_from("<H", d, 0x3C)[0]
    e_shstrndx = struct.unpack_from("<H", d, 0x3E)[0]
    secs = []
    for i in range(e_shnum):
        o = e_shoff + i * e_shentsize
        secs.append(dict(zip(("name", "typ", "flags", "addr", "off", "size", "link", "info", "align", "entsize"),
                             struct.unpack_from("<IIQQQQIIQQ", d, o))))
    shstr = secs[e_shstrndx]

    def nm(n):
        s = shstr["off"] + n
        return d[s:d.index(b"\0", s)].decode()

    for s in secs:
        s["n"] = nm(s["name"])
    dyn = [s for s in secs if s["n"] == ".dynamic"][0]
    dynstr = [s for s in secs if s["n"] == ".dynstr"][0]
    st = d[dynstr["off"]:dynstr["off"] + dynstr["size"]]
    needed, pagesize = [], None
    o = dyn["off"]
    while True:
        tag, val = struct.unpack_from("<qQ", d, o)
        o += 16
        if tag == 0:
            break
        if tag == 1:
            needed.append(st[val:st.index(b"\0", val)].decode())
        if tag == 0x6000000F:   # DT_ANDROID_REL? not used; keep pagesize detection simple
            pass
    print(f"        ELF ok: aarch64, {len(d)//1024} KiB, NEEDED={needed}")
    missing = [l for l in ("libEGL.so", "libGLESv2.so", "libandroid.so") if l not in needed]
    assert not missing, f"missing DT_NEEDED: {missing}"


def step_resources():
    print("[3/6] aapt2 link (manifest + resources)")
    res_dir = os.path.join(APP, "res")
    manifest = os.path.join(APP, "AndroidManifest.xml")
    res_zip = os.path.join(BUILD, "res.zip")
    base_apk = os.path.join(BUILD, "base.apk")
    if not os.path.isfile(AAPT2):
        raise SystemExit(f"aapt2 not found at {AAPT2}")
    if not os.path.isfile(ANDROID_JAR):
        raise SystemExit(f"android.jar not found at {ANDROID_JAR}")
    run([AAPT2, "compile", "--dir", res_dir, "-o", res_zip])
    run([AAPT2, "link", "-o", base_apk,
         "--manifest", manifest,
         "-I", ANDROID_JAR,
         "-R", res_zip,
         "--java", os.path.join(BUILD, "gen"),
         "--min-sdk-version", str(MIN_SDK),
         "--target-sdk-version", str(TARGET_SDK),
         "--version-code", str(VERSION_CODE),
         "--version-name", VERSION_NAME,
         "--auto-add-overlay"])
    return base_apk


def step_package(base_apk, so_path):
    print("[4/6] assembling aligned APK")
    unsigned = os.path.join(BUILD, "cyber-drift-unsigned.apk")
    entries = []

    # AndroidManifest.xml and resources.arsc must come first for fast install.
    with zipfile.ZipFile(base_apk) as z:
        names = z.namelist()
        priority = ["AndroidManifest.xml", "resources.arsc"]
        ordered = [n for n in priority if n in names] + [n for n in names if n not in priority]
        for n in ordered:
            if n.endswith("/"):
                continue
            data = z.read(n)
            if n == "resources.arsc":
                # Android 11+ rejects a compressed resources.arsc
                entries.append((n, data, "store", 4096))
            else:
                entries.append((n, data, "deflate", 4))

    # native lib: stored + 16 KB aligned (Android 15 16 KB page-size devices)
    so_bytes = open(so_path, "rb").read()
    entries.append(("lib/arm64-v8a/libcyberdrift.so", so_bytes, "store", 16384))

    # optional assets
    assets_dir = os.path.join(APP, "assets")
    if os.path.isdir(assets_dir):
        for dirpath, _, files in os.walk(assets_dir):
            for f in sorted(files):
                full = os.path.join(dirpath, f)
                rel = os.path.relpath(full, assets_dir).replace(os.sep, "/")
                entries.append((f"assets/{rel}", open(full, "rb").read(), "deflate", 4))

    zb = mkapk.ZipBuilder()
    for name, data, method, align in entries:
        zb.add(name, data, method, align)
    apk, cd_off, cd_size = zb.build()
    with open(unsigned, "wb") as f:
        f.write(apk)
    print(f"        {len(entries)} entries, {len(apk)//1024} KiB")
    return unsigned


def step_sign(unsigned):
    print("[5/6] signing (v1 JAR + v2 APK Signature Scheme)")
    key, cert_der = load_or_make_key()
    apk = open(unsigned, "rb").read()

    entries, cd_offset, cd_size, eocd_off = mkapk.read_zip_structure(apk)
    # v1 digest covers entry contents
    entry_data = {}
    for e in entries:
        entry_data[e["name"]] = zlib_crc_bytes(apk, e)

    manifest, sf, sig = mkapk.v1_sign(entry_data, key, cert_der)

    # Rebuild the archive including the signature files, then apply v2.
    zb = mkapk.ZipBuilder()
    for e in entries:
        zb.add(e["name"], entry_data[e["name"]], "store" if e["method"] == 0 else "deflate",
               16384 if e["name"].endswith(".so") else 4)
    zb.add("META-INF/MANIFEST.MF", manifest, "deflate", 4)
    zb.add("META-INF/CERT.SF", sf, "deflate", 4)
    zb.add("META-INF/CERT.RSA", sig, "deflate", 4)
    v1_apk, _, _ = zb.build()
    v2_apk = mkapk.v2_sign(v1_apk, key, cert_der)

    out = os.path.join(ROOT, "game.apk")
    with open(out, "wb") as f:
        f.write(v2_apk)
    print(f"        wrote {os.path.relpath(out, ROOT)} ({len(v2_apk)//1024} KiB)")
    return out


def load_or_make_key():
    """Reuse the release key so updates install over an existing copy."""
    from cryptography import x509
    from cryptography.hazmat.primitives import hashes, serialization
    from cryptography.hazmat.primitives.asymmetric import rsa
    from datetime import datetime, timedelta, timezone
    from cryptography.x509.oid import NameOID
    key_path = os.path.join(APP, "cyberdrift-release.der")
    cert_path = os.path.join(APP, "cyberdrift-release.cer")
    if os.path.isfile(key_path) and os.path.isfile(cert_path):
        key = serialization.load_der_private_key(open(key_path, "rb").read(), None)
        return key, open(cert_path, "rb").read()
    key, cert_der = mkapk.make_key_and_cert("Cyber Drift", "Arena Interactive")
    os.makedirs(APP, exist_ok=True)
    with open(key_path, "wb") as f:
        f.write(key.private_bytes(serialization.Encoding.DER,
                                  serialization.PrivateFormat.PKCS8,
                                  serialization.NoEncryption()))
    with open(cert_path, "wb") as f:
        f.write(cert_der)
    return key, cert_der


def zlib_crc_bytes(apk, entry):
    """Return the uncompressed bytes of a zip entry using stored sizes."""
    off = entry["local_offset"]
    (_sig, _ver, _flags, method, _t, _d, _crc, csize, usize, nlen, elen) = \
        struct.unpack_from("<IHHHHHIIIHH", apk, off)
    start = off + 30 + nlen + elen
    payload = apk[start:start + csize]
    if method == 0:
        return payload
    import zlib
    return zlib.decompress(payload, -15, usize)


def step_verify(apk_path):
    print("[6/6] verification")
    r = run([AAPT2, "dump", "badging", apk_path])
    info = r.stdout
    checks = {
        "package line": "package: name='" in info,
        "launchable activity": "launchable-activity" in info,
        "native code": "native-code: 'arm64-v8a'" in info,
        "landscape": "landscape" in info,
        "sdk": f"sdkVersion:'{MIN_SDK}'" in info,
    }
    for name, ok in checks.items():
        print(f"        {'OK  ' if ok else 'FAIL'} {name}")
    print("        ---- badging ----")
    for line in info.splitlines()[:12]:
        print("        " + line)

    # inside the apk
    with zipfile.ZipFile(apk_path) as z:
        names = z.namelist()
        bad = z.testzip()
        print(f"        zip integrity: {'ok' if bad is None else 'corrupt ' + bad}")
        so = [n for n in names if n.endswith(".so")]
        print(f"        native libs: {so}")
        # verify 16 KB alignment of the stored .so
        with open(apk_path, "rb") as f:
            raw = f.read()
        for e in mkapk.read_zip_structure(raw)[0]:
            if e["name"].endswith(".so"):
                off = e["local_offset"]
                (_s, _v, _f, method, _t, _d, _c, _cs, _us, nlen, elen) = \
                    struct.unpack_from("<IHHHHHIIIHH", raw, off)
                data_off = off + 30 + nlen + elen
                print(f"        {e['name']}: stored={method == 0}, data offset {data_off} "
                      f"aligned(16384)={'yes' if data_off % 16384 == 0 else 'NO'}")
        # v2 block present?
        idx = raw.find(b"APK Sig Block 42")
        print(f"        v2 signing block: {'present' if idx > 0 else 'MISSING'}")
    return all(checks.values())


def main():
    t0 = time.time()
    os.makedirs(BUILD, exist_ok=True)
    step_stubs()
    so = step_native()
    base = step_resources()
    unsigned = step_package(base, so)
    apk = step_sign(unsigned)
    ok = step_verify(apk)
    print(f"\n{'SUCCESS' if ok else 'COMPLETED WITH WARNINGS'} in {time.time() - t0:.1f}s -> {apk}")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
