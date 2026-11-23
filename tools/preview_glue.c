// Desktop preview: Mesa-backed implementations of the cd_* GL entry points.
// The engine resolves CD_GL(foo) -> cd_foo in preview builds; these forwarders
// call the pointers loaded from libGLESv2.
#include "cd_gl.h"
#include "preview_gl.h"
#include <stdlib.h>

// EGL function pointers (the preview context is created by preview_egl.c).
cd_pfn_eglGetDisplay cd_eglGetDisplay = 0;
cd_pfn_eglInitialize cd_eglInitialize = 0;
cd_pfn_eglGetError cd_eglGetError = 0;
cd_pfn_eglTerminate cd_eglTerminate = 0;
cd_pfn_eglChooseConfig cd_eglChooseConfig = 0;
cd_pfn_eglGetConfigAttrib cd_eglGetConfigAttrib = 0;
cd_pfn_eglCreateWindowSurface cd_eglCreateWindowSurface = 0;
cd_pfn_eglCreatePbufferSurface cd_eglCreatePbufferSurface = 0;
cd_pfn_eglCreateContext cd_eglCreateContext = 0;
cd_pfn_eglMakeCurrent cd_eglMakeCurrent = 0;
cd_pfn_eglDestroySurface cd_eglDestroySurface = 0;
cd_pfn_eglDestroyContext cd_eglDestroyContext = 0;
cd_pfn_eglSwapBuffers cd_eglSwapBuffers = 0;
cd_pfn_eglSwapInterval cd_eglSwapInterval = 0;
cd_pfn_eglBindAPI cd_eglBindAPI = 0;
cd_pfn_eglGetConfigs cd_eglGetConfigs = 0;
cd_pfn_eglGetProcAddress cd_eglGetProcAddress = 0;

#include "preview_glue_gen.inc"

// glReadPixels is preview-only (not in the engine's GL subset).
extern void pgl_read(int x, int y, int w, int h, unsigned int fmt, unsigned int type, void* px);
void pgl_glReadPixels(int x, int y, int w, int h, unsigned int fmt, unsigned int type, void* px) {
    pgl_read(x, y, w, h, fmt, type, px);
}
