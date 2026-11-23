// Desktop-only GL loader for the preview harness (no GL headers on the box).
#ifndef PREVIEW_GL_H
#define PREVIEW_GL_H

#include <dlfcn.h>
#include <stdint.h>
#include <stddef.h>

typedef float GLfloat;
typedef int GLint;
typedef unsigned int GLuint;
typedef int GLsizei;
typedef unsigned int GLenum;
typedef unsigned char GLboolean;
typedef unsigned char GLubyte;
typedef char GLchar;
typedef long GLsizeiptr;
typedef long GLintptr;
typedef unsigned int GLbitfield;

#define GL_FALSE 0
#define GL_TRUE 1
#define GL_TRIANGLES 0x0004
#define GL_DEPTH_TEST 0x0B71
#define GL_BLEND 0x0BE2
#define GL_LEQUAL 0x0203
#define GL_CULL_FACE 0x0B44
#define GL_BACK 0x0405
#define GL_SRC_ALPHA 0x0302
#define GL_ONE_MINUS_SRC_ALPHA 0x0303
#define GL_ONE 1
#define GL_ARRAY_BUFFER 0x8892
#define GL_STATIC_DRAW 0x88E4
#define GL_DYNAMIC_DRAW 0x88E8
#define GL_VERTEX_SHADER 0x8B31
#define GL_FRAGMENT_SHADER 0x8B30
#define GL_COMPILE_STATUS 0x8B81
#define GL_LINK_STATUS 0x8B82
#define GL_COLOR_BUFFER_BIT 0x00004000
#define GL_DEPTH_BUFFER_BIT 0x00000100
#define GL_TEXTURE_2D 0x0DE1
#define GL_RGBA 0x1908
#define GL_UNSIGNED_BYTE 0x1401
#define GL_FLOAT 0x1406
#define GL_TEXTURE0 0x84C0
#define GL_TEXTURE_MIN_FILTER 0x2801
#define GL_TEXTURE_MAG_FILTER 0x2800
#define GL_LINEAR 0x2601
#define GL_TEXTURE_WRAP_S 0x2802
#define GL_TEXTURE_WRAP_T 0x2803
#define GL_CLAMP_TO_EDGE 0x812F

#define GL_FUNCS(X)                                                            \
    X(void, glClear, (GLbitfield))                                             \
    X(void, glClearColor, (GLfloat, GLfloat, GLfloat, GLfloat))                 \
    X(void, glEnable, (GLenum))                                                \
    X(void, glDisable, (GLenum))                                               \
    X(void, glBlendFunc, (GLenum, GLenum))                                     \
    X(void, glViewport, (GLint, GLint, GLsizei, GLsizei))                      \
    X(void, glCullFace, (GLenum))                                              \
    X(void, glDepthMask, (GLboolean))                                          \
    X(void, glDepthFunc, (GLenum))                                             \
    X(GLenum, glGetError, (void))                                              \
    X(const GLubyte*, glGetString, (GLenum))                                   \
    X(void, glGetIntegerv, (GLenum, GLint*))                                   \
    X(GLuint, glCreateShader, (GLenum))                                        \
    X(void, glShaderSource, (GLuint, GLsizei, const GLchar* const*, const GLint*)) \
    X(void, glCompileShader, (GLuint))                                         \
    X(void, glGetShaderiv, (GLuint, GLenum, GLint*))                           \
    X(void, glGetShaderInfoLog, (GLuint, GLsizei, GLsizei*, GLchar*))          \
    X(GLuint, glCreateProgram, (void))                                         \
    X(void, glAttachShader, (GLuint, GLuint))                                  \
    X(void, glLinkProgram, (GLuint))                                           \
    X(void, glGetProgramiv, (GLuint, GLenum, GLint*))                          \
    X(void, glGetProgramInfoLog, (GLuint, GLsizei, GLsizei*, GLchar*))         \
    X(void, glUseProgram, (GLuint))                                            \
    X(void, glDeleteShader, (GLuint))                                          \
    X(GLint, glGetUniformLocation, (GLuint, const GLchar*))                    \
    X(GLint, glGetAttribLocation, (GLuint, const GLchar*))                     \
    X(void, glUniform1i, (GLint, GLint))                                       \
    X(void, glUniform1f, (GLint, GLfloat))                                     \
    X(void, glUniform2f, (GLint, GLfloat, GLfloat))                            \
    X(void, glUniform3f, (GLint, GLfloat, GLfloat, GLfloat))                   \
    X(void, glUniform4f, (GLint, GLfloat, GLfloat, GLfloat, GLfloat))          \
    X(void, glUniformMatrix4fv, (GLint, GLsizei, GLboolean, const GLfloat*))    \
    X(void, glVertexAttribPointer, (GLuint, GLint, GLenum, GLboolean, GLsizei, const void*)) \
    X(void, glEnableVertexAttribArray, (GLuint))                               \
    X(void, glGenBuffers, (GLsizei, GLuint*))                                  \
    X(void, glBindBuffer, (GLenum, GLuint))                                    \
    X(void, glBufferData, (GLenum, GLsizeiptr, const void*, GLenum))           \
    X(void, glBufferSubData, (GLenum, GLintptr, GLsizeiptr, const void*))      \
    X(void, glDeleteBuffers, (GLsizei, const GLuint*))                         \
    X(void, glDrawArrays, (GLenum, GLint, GLsizei))                            \
    X(void, glGenTextures, (GLsizei, GLuint*))                                 \
    X(void, glBindTexture, (GLenum, GLuint))                                   \
    X(void, glTexImage2D, (GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void*)) \
    X(void, glTexParameteri, (GLenum, GLenum, GLint))                          \
    X(void, glActiveTexture, (GLenum))                                         \
    X(void, glDeleteProgram, (GLuint))                                         \
    X(void, glDeleteTextures, (GLsizei, const GLuint*))

int preview_gl_load(void);
void pgl_glReadPixels(int x, int y, int w, int h, unsigned int fmt, unsigned int type, void* px);

#endif
