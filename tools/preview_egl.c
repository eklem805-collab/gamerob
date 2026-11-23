// Desktop preview: Mesa EGL setup for an offscreen pbuffer + GLES2 context.
#include "cd_gl.h"
#include <dlfcn.h>
#include <stdio.h>
#include <stdint.h>

typedef void* PDisplay;
typedef void* PSurface;
typedef void* PContext;
typedef void* PConfig;

static void* g_libegl;
static void* g_libgles;

#define F(ret, name, args) typedef ret (*t_##name) args; static t_##name f_##name;
F(PDisplay, eglGetDisplay, (void*))
F(int, eglInitialize, (PDisplay, int*, int*))
F(int, eglChooseConfig, (PDisplay, const int*, PConfig*, int, int*))
F(int, eglBindAPI, (unsigned int))
F(PContext, eglCreateContext, (PDisplay, PConfig, PContext, const int*))
F(PSurface, eglCreatePbufferSurface, (PDisplay, PConfig, const int*))
F(int, eglMakeCurrent, (PDisplay, PSurface, PSurface, PContext))
F(void*, eglGetProcAddress, (const char*))
F(int, eglSwapInterval, (PDisplay, int))
#undef F

static void* sym(const char* name) {
    void* p = NULL;
    if (g_libgles) p = dlsym(g_libgles, name);
    if (!p && g_libegl) p = dlsym(g_libegl, name);
    if (!p && f_eglGetProcAddress) p = (void*)f_eglGetProcAddress(name);
    return p;
}

void* egl_preview_get_proc(const char* name) { return sym(name); }

int preview_egl_init(int w, int h) {
    const char* egl_names[] = {"libEGL.so.1", "libEGL.so", "/usr/lib/x86_64-linux-gnu/libEGL.so.1",
                               "/lib/x86_64-linux-gnu/libEGL.so.1", NULL};
    for (int i = 0; egl_names[i]; i++) {
        g_libegl = dlopen(egl_names[i], RTLD_NOW | RTLD_GLOBAL);
        if (g_libegl) { printf("loaded %s\n", egl_names[i]); break; }
    }
    if (!g_libegl) { printf("cannot load libEGL\n"); return 0; }
    const char* gl_names[] = {"libGLESv2.so.2", "libGLESv2.so", "/usr/lib/x86_64-linux-gnu/libGLESv2.so.2",
                              NULL};
    for (int i = 0; gl_names[i]; i++) {
        g_libgles = dlopen(gl_names[i], RTLD_NOW | RTLD_GLOBAL);
        if (g_libgles) { printf("loaded %s\n", gl_names[i]); break; }
    }

    f_eglGetDisplay = (t_eglGetDisplay)dlsym(g_libegl, "eglGetDisplay");
    f_eglInitialize = (t_eglInitialize)dlsym(g_libegl, "eglInitialize");
    f_eglChooseConfig = (t_eglChooseConfig)dlsym(g_libegl, "eglChooseConfig");
    f_eglBindAPI = (t_eglBindAPI)dlsym(g_libegl, "eglBindAPI");
    f_eglCreateContext = (t_eglCreateContext)dlsym(g_libegl, "eglCreateContext");
    f_eglCreatePbufferSurface = (t_eglCreatePbufferSurface)dlsym(g_libegl, "eglCreatePbufferSurface");
    f_eglMakeCurrent = (t_eglMakeCurrent)dlsym(g_libegl, "eglMakeCurrent");
    f_eglGetProcAddress = (void*)dlsym(g_libegl, "eglGetProcAddress");
    if (!f_eglGetDisplay || !f_eglInitialize) { printf("EGL entry points missing\n"); return 0; }

    PDisplay dpy = f_eglGetDisplay(NULL);
    int major = 0, minor = 0;
    if (!f_eglInitialize(dpy, &major, &minor)) { printf("eglInitialize failed\n"); return 0; }
    printf("EGL %d.%d\n", major, minor);

    const int config_attribs[] = {
        0x3033, 0x0001,   // EGL_SURFACE_TYPE, EGL_PBUFFER_BIT
        0x3040, 0x0004,   // EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT
        0x3024, 8, 0x3023, 8, 0x3022, 8, 0x3021, 8,
        0x3025, 24,
        0x3038};
    PConfig cfg = NULL;
    int n = 0;
    if (!f_eglChooseConfig(dpy, config_attribs, &cfg, 1, &n) || n < 1) {
        printf("no EGL config (pbuffer)\n");
        return 0;
    }
    const int pb[] = {0x3057, w, 0x3056, h, 0x3038};
    PSurface surf = f_eglCreatePbufferSurface(dpy, cfg, pb);
    if (!surf) { printf("pbuffer failed\n"); return 0; }
    f_eglBindAPI(0x30A0);   // EGL_OPENGL_ES_API
    const int ctx_attribs[] = {0x3098, 2, 0x3038};
    PContext ctx = f_eglCreateContext(dpy, cfg, NULL, ctx_attribs);
    if (!ctx) { printf("context failed\n"); return 0; }
    if (!f_eglMakeCurrent(dpy, surf, surf, ctx)) { printf("makeCurrent failed\n"); return 0; }
    return 1;
}
