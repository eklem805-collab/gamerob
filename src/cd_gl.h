// Dynamic GLES2/EGL entry points.
// Android build links libEGL/libGLESv2 directly (CD_GL_DIRECT).
// Desktop preview build uses dlopen (CD_GL_DYNAMIC) for the same renderer code.
#ifndef CD_GL_H
#define CD_GL_H

#if !defined(CD_GL_DIRECT)
#define CD_GL_DYNAMIC 1
#endif

#ifdef CD_GL_DIRECT
#include "cd_android.h"
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#define CD_GL(f) f
#else
#define EGL_EGL_PROTOTYPES 0
#define EGL_EGLEXT_PROTOTYPES 0
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#define CD_GL(f) cd_##f
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifndef CD_GL_DIRECT
typedef void* cd_EGLDisplay;
typedef void* cd_EGLConfig;
typedef void* cd_EGLSurface;
typedef void* cd_EGLContext;
typedef void* cd_EGLNativeDisplayType;
typedef void* cd_EGLNativeWindowType;
typedef int32_t cd_EGLint;
typedef uint32_t cd_EGLBoolean;

#define CD_EGL_FUNCS(X)                                                        \
    X(EGLDisplay, eglGetDisplay, (cd_EGLNativeDisplayType))                     \
    X(cd_EGLint, eglInitialize, (cd_EGLDisplay, cd_EGLint*, cd_EGLint*))        \
    X(cd_EGLint, eglTerminate, (cd_EGLDisplay))                                 \
    X(cd_EGLint, eglGetError, (void))                                           \
    X(cd_EGLint, eglChooseConfig, (cd_EGLDisplay, const cd_EGLint*, cd_EGLConfig*, cd_EGLint, cd_EGLint*)) \
    X(cd_EGLint, eglGetConfigAttrib, (cd_EGLDisplay, cd_EGLConfig, cd_EGLint, cd_EGLint*)) \
    X(cd_EGLSurface, eglCreateWindowSurface, (cd_EGLDisplay, cd_EGLConfig, cd_EGLNativeWindowType, const cd_EGLint*)) \
    X(cd_EGLSurface, eglCreatePbufferSurface, (cd_EGLDisplay, cd_EGLConfig, const cd_EGLint*)) \
    X(cd_EGLContext, eglCreateContext, (cd_EGLDisplay, cd_EGLConfig, cd_EGLContext, const cd_EGLint*)) \
    X(cd_EGLint, eglMakeCurrent, (cd_EGLDisplay, cd_EGLSurface, cd_EGLSurface, cd_EGLContext)) \
    X(cd_EGLint, eglDestroySurface, (cd_EGLDisplay, cd_EGLSurface))              \
    X(cd_EGLint, eglDestroyContext, (cd_EGLDisplay, cd_EGLContext))             \
    X(cd_EGLint, eglSwapBuffers, (cd_EGLDisplay, cd_EGLSurface))                \
    X(cd_EGLint, eglSwapInterval, (cd_EGLDisplay, cd_EGLint))                   \
    X(cd_EGLBoolean, eglBindAPI, (cd_EGLint))                                   \
    X(cd_EGLint, eglGetConfigs, (cd_EGLDisplay, cd_EGLConfig*, cd_EGLint, cd_EGLint*)) \
    X(__eglMustCastToProperFunctionPointerType, eglGetProcAddress, (const char*))
#define X(ret, name, args) typedef ret (*cd_pfn_##name) args; extern cd_pfn_##name cd_##name;
CD_EGL_FUNCS(X)
#undef X

#define CD_GL_FUNCS(X)                                                          \
    X(void, glClear, (GLbitfield))                                              \
    X(void, glClearColor, (GLfloat, GLfloat, GLfloat, GLfloat))                  \
    X(void, glEnable, (GLenum))                                                 \
    X(void, glDisable, (GLenum))                                                \
    X(void, glBlendFunc, (GLenum, GLenum))                                      \
    X(void, glViewport, (GLint, GLint, GLsizei, GLsizei))                       \
    X(void, glCullFace, (GLenum))                                               \
    X(void, glDepthMask, (GLboolean))                                           \
    X(void, glDepthFunc, (GLenum))                                              \
    X(GLenum, glGetError, (void))                                               \
    X(const GLubyte*, glGetString, (GLenum))                                    \
    X(void, glGetIntegerv, (GLenum, GLint*))                                    \
    X(GLuint, glCreateShader, (GLenum))                                         \
    X(void, glShaderSource, (GLuint, GLsizei, const GLchar* const*, const GLint*)) \
    X(void, glCompileShader, (GLuint))                                          \
    X(void, glGetShaderiv, (GLuint, GLenum, GLint*))                            \
    X(void, glGetShaderInfoLog, (GLuint, GLsizei, GLsizei*, GLchar*))           \
    X(GLuint, glCreateProgram, (void))                                          \
    X(void, glAttachShader, (GLuint, GLuint))                                   \
    X(void, glLinkProgram, (GLuint))                                            \
    X(void, glGetProgramiv, (GLuint, GLenum, GLint*))                           \
    X(void, glGetProgramInfoLog, (GLuint, GLsizei, GLsizei*, GLchar*))          \
    X(void, glUseProgram, (GLuint))                                             \
    X(void, glDeleteShader, (GLuint))                                           \
    X(GLint, glGetUniformLocation, (GLuint, const GLchar*))                     \
    X(GLint, glGetAttribLocation, (GLuint, const GLchar*))                      \
    X(void, glUniform1i, (GLint, GLint))                                        \
    X(void, glUniform1f, (GLint, GLfloat))                                      \
    X(void, glUniform2f, (GLint, GLfloat, GLfloat))                             \
    X(void, glUniform3f, (GLint, GLfloat, GLfloat, GLfloat))                    \
    X(void, glUniform4f, (GLint, GLfloat, GLfloat, GLfloat, GLfloat))           \
    X(void, glUniformMatrix4fv, (GLint, GLsizei, GLboolean, const GLfloat*))     \
    X(void, glVertexAttribPointer, (GLuint, GLint, GLenum, GLboolean, GLsizei, const void*)) \
    X(void, glEnableVertexAttribArray, (GLuint))                                \
    X(void, glGenBuffers, (GLsizei, GLuint*))                                   \
    X(void, glBindBuffer, (GLenum, GLuint))                                     \
    X(void, glBufferData, (GLenum, GLsizeiptr, const void*, GLenum))            \
    X(void, glDeleteBuffers, (GLsizei, const GLuint*))                           \
    X(void, glBufferSubData, (GLenum, GLintptr, GLsizeiptr, const void*))        \
    X(void, glDrawArrays, (GLenum, GLint, GLsizei))                             \
    X(void, glGenTextures, (GLsizei, GLuint*))                                  \
    X(void, glBindTexture, (GLenum, GLuint))                                    \
    X(void, glTexImage2D, (GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void*)) \
    X(void, glTexParameteri, (GLenum, GLenum, GLint))                           \
    X(void, glActiveTexture, (GLenum))                                          \
    X(void, glDeleteProgram, (GLuint))                                          \
    X(void, glDeleteTextures, (GLsizei, const GLuint*))
#define X(ret, name, args) typedef ret (*cd_pfn_##name) args; extern cd_pfn_##name cd_##name;
CD_GL_FUNCS(X)
#undef X
#endif

// get_proc: resolves a symbol name. Returns 1 on success.
int cd_gl_load(void* (*get_proc)(const char* name));

#ifdef __cplusplus
}
#endif
#endif
