#include "cd_gl.h"

#ifdef CD_GL_DYNAMIC
#define X(ret, name, args) cd_pfn_##name cd_##name = 0;
CD_EGL_FUNCS(X)
CD_GL_FUNCS(X)
#undef X

int cd_gl_load(void* (*get_proc)(const char* name)) {
    int missing = 0;
#define X(ret, name, args)                                        \
    cd_##name = (cd_pfn_##name)get_proc(#name);                    \
    if (!cd_##name) { missing++; }
    CD_EGL_FUNCS(X)
    CD_GL_FUNCS(X)
#undef X
    return missing;
}
#else
int cd_gl_load(void* (*get_proc)(const char* name)) {
    (void)get_proc;
    return 0;
}
#endif
