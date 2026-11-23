#!/usr/bin/env python3
"""Generate import stub shared libraries for the Android system libs we use.

We build without the NDK, so there are no libEGL.so/libGLESv2.so/libandroid.so
import libraries to link against. This script derives every symbol name the
engine references (from the shim headers + Khronos headers) and emits tiny
aarch64 shared objects that define exactly those symbols. Linking against them
with --no-undefined therefore validates every name we call, and the resulting
DT_NEEDED entries make Android's loader bind them to the real system libs.
"""
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
WORK = os.path.join(ROOT, "build", "stubs")
ZIG = os.environ.get("ZIG", "zig")
TARGET = "aarch64-linux-android"


def read(p):
    with open(os.path.join(ROOT, p), "r", encoding="utf-8") as f:
        return f.read()


KEYWORD_BLOCK = {
    "if", "while", "for", "switch", "return", "sizeof", "defined", "do", "else",
    "__attribute__", "static", "inline", "const", "volatile", "unsigned", "signed",
    "struct", "union", "enum", "typedef", "void", "char", "int", "long", "short",
    "float", "double", "int8_t", "uint8_t", "int16_t", "uint16_t", "int32_t",
    "uint32_t", "int64_t", "uint64_t", "size_t", "ssize_t", "time_t", "jint",
    "jlong", "jfloat", "jdouble", "jboolean", "jobject", "jclass", "jstring",
    "jarray", "jvalue", "JNIEnv", "JavaVM", "FILE", "alignas", "noreturn",
}


def decl_names_loose(text):
    """Loose scan: every identifier used as a call target."""
    text = re.sub(r"//[^\n]*", "", text)
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    names = set()
    for m in re.finditer(r"\b([A-Za-z_]\w*)\s*\(", text):
        n = m.group(1)
        if n in KEYWORD_BLOCK or n.startswith("CD_") or n.startswith("__builtin"):
            continue
        names.add(n)
    return names


def decl_names(text):
    """Extract function names from C declarations."""
    names = set()
    # strip comments
    text = re.sub(r"//[^\n]*", "", text)
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    for m in re.finditer(r"\b([A-Za-z_]\w*)\s*\([^;{)]*(?:\([^;{)]*\)[^;{)]*)*\)\s*;", text):
        name = m.group(1)
        if name in ("if", "while", "for", "switch", "return", "sizeof", "defined"):
            continue
        names.add(name)
    return names


def main():
    os.makedirs(WORK, exist_ok=True)
    groups = {}

    # EGL + GLES2 come from the Khronos headers (real prototypes).
    egl = read("third_party/EGL/egl.h")
    gles = read("third_party/GLES2/gl2.h")
    groups["EGL"] = sorted(n for n in decl_names(egl) if n.startswith("egl"))
    groups["GLESv2"] = sorted(n for n in decl_names(gles) if n.startswith("gl"))

    an = read("third_party/android_compat.h")
    an_names = decl_names(an)
    android_prefixes = (
        "ANativeWindow_", "ANativeActivity_", "AInputQueue_", "AInputEvent_",
        "AKeyEvent_", "AMotionEvent_", "ALooper_", "AConfiguration_",
    )
    groups["android"] = sorted(
        n for n in an_names
        if n.startswith(android_prefixes) or n == "app_dummy"
    )
    groups["log"] = sorted(set(n for n in an_names if n.startswith("__android_log")) |
                          {"__android_log_print", "__android_log_write", "__android_log_vprint",
                           "__android_log_assert"})
    math_names = decl_names(read("third_party/freestanding/math.h")) | \
        decl_names_loose(read("third_party/freestanding/math.h"))
    groups["m"] = sorted(n for n in math_names
                         if n not in ('isnan', 'isinf') and not n.startswith('M_'))

    libc_syms = ["malloc", "calloc", "realloc", "free", "memset", "memcpy", "memmove",
                 "memcmp", "strlen", "strcmp", "strcpy", "strncpy", "clock_gettime",
                 "nanosleep", "abort", "exit", "dlopen", "dlsym", "dlclose", "dlerror"]
    groups["c"] = sorted(set(libc_syms))

    extra = {
        # AInputQueue/ALooper helpers we call from platform_android.c
        "android": ["AInputQueue_getEvent", "AInputQueue_preDispatchEvent",
                    "AInputQueue_finishEvent", "AInputQueue_attachLooper",
                    "AInputQueue_detachLooper", "ANativeActivity_setWindowFlags",
                    "ANativeWindow_fromSurface", "AAssetManager_open"],
    }
    extra = {
        # AInputQueue/ALooper helpers we call from platform_android.c
        "android": ["AInputQueue_getEvent", "AInputQueue_preDispatchEvent",
                    "AInputQueue_finishEvent", "AInputQueue_attachLooper",
                    "AInputQueue_detachLooper", "ANativeActivity_setWindowFlags",
                    "ANativeWindow_fromSurface", "AAssetManager_open"],
        # LLVM contracts sinf/cosf pairs into sincosf
        "m": ["sincosf", "sincos", "sincosf_", "__sincosf", "ldexpf", "ldexp",
              "fmodf", "roundf", "truncf", "nearbyintf", "copysignf", "fminf", "fmaxf",
              "erff", "log2f", "expm1f", "atan2f", "tanf", "asinf", "acosf", "powf",
              "fabsf", "floorf", "ceilf", "sqrtf", "sinf", "cosf", "expf", "logf"],
    }
    for k, v in extra.items():
        groups[k] = sorted(set(groups.get(k, []) + v))

    rc = 0
    for lib, syms in groups.items():
        tag = {"c": "c", "log": "log", "m": "m"}.get(lib, lib)
        cfile = os.path.join(WORK, f"stub_{tag}.c")
        lines = ['typedef void (*cd_fn)(void);']
        # Non-JNI, no-arg function pointers are enough: we only need the names.
        for s in syms:
            lines.append(f'__attribute__((visibility("default"), used)) static void* {s}_addr = (void*)0;')
            lines.append(f'__attribute__((visibility("default"))) void {s}(void) {{ }}')
        with open(cfile, "w", encoding="utf-8") as f:
            f.write("\n".join(lines) + "\n")
        so = os.path.join(WORK, f"lib{tag}.so")
        cmd = [ZIG, "cc", "-target", TARGET, "-shared", "-nostdlib", "-fPIC",
               cfile, "-o", so, f"-Wl,-soname,lib{tag}.so"]
        r = subprocess.run(cmd, capture_output=True, text=True)
        if r.returncode != 0:
            print(f"[stub] {lib} FAILED\n{r.stderr[:800]}")
            rc = 1
        else:
            print(f"[stub] lib{tag}.so  ({len(syms)} symbols)")
    return rc


if __name__ == "__main__":
    sys.exit(main())
