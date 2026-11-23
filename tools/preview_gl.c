// Desktop preview: resolve the GLES2/EGL entry points at runtime.
#include "preview_gl.h"
#include "cd_gl.h"
#include <dlfcn.h>
#include <stdio.h>


extern void* egl_preview_get_proc(const char* name);

typedef void (*t_ReadPixels)(int, int, int, int, unsigned int, unsigned int, void*);
static t_ReadPixels s_readpixels;

void pgl_read(int x, int y, int w, int h, unsigned int fmt, unsigned int type, void* px) {
    if (s_readpixels) s_readpixels(x, y, w, h, fmt, type, px);
}

int preview_gl_load(void) {
    int missing = 0;
    s_readpixels = (t_ReadPixels)egl_preview_get_proc("glReadPixels");
#define X(ret, name, args)                                        \
    cd_##name = (cd_pfn_##name)egl_preview_get_proc(#name);           \
    if (!cd_##name) { printf("missing GL: %s\n", #name); missing++; }
    GL_FUNCS(X)
#undef X
    return missing;
}
