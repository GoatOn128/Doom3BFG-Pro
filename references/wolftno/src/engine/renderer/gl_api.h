#pragma once

#include <cstdint>

using glBindBufferProc_t = void (*)(unsigned int target, unsigned int buffer);
using glGenBuffersProc_t = void (*)(int count, unsigned int* buffers);
using glDeleteBuffersProc_t = void (*)(int count,
                                       const unsigned int* buffers);
using glBufferDataProc_t = void (*)(unsigned int target, std::intptr_t size,
                                    const void* data, unsigned int usage);
using glBufferSubDataProc_t = void (*)(unsigned int target,
                                       std::intptr_t offset,
                                       std::intptr_t size,
                                       const void* data);
using glNamedBufferSubDataProc_t = void (*)(unsigned int buffer,
                                            std::intptr_t offset,
                                            std::intptr_t size,
                                            const void* data);
using glMapBufferProc_t = void* (*)(unsigned int target,
                                    unsigned int access);
using glMapBufferRangeProc_t = void* (*)(unsigned int target,
                                         std::intptr_t offset,
                                         std::intptr_t length,
                                         unsigned int access);
using glMapNamedBufferProc_t = void* (*)(unsigned int buffer,
                                         unsigned int access);
using glMapNamedBufferRangeProc_t = void* (*)(unsigned int buffer,
                                              std::intptr_t offset,
                                              std::intptr_t length,
                                              unsigned int access);
using glUnmapBufferProc_t = unsigned char (*)(unsigned int target);
using glUnmapNamedBufferProc_t = unsigned char (*)(unsigned int buffer);
using glBindFramebufferProc_t = void (*)(unsigned int target,
                                         unsigned int framebuffer);
using glGenFramebuffersProc_t = void (*)(int count,
                                         unsigned int* framebuffers);
using glDeleteFramebuffersProc_t = void (*)(
    int count, const unsigned int* framebuffers);
using glCheckFramebufferStatusProc_t = unsigned int (*)(
    unsigned int target);
using glDrawBuffersProc_t = void (*)(int count,
                                     const unsigned int* buffers);
using glBlitFramebufferProc_t = void (*)(int sourceX0, int sourceY0,
                                         int sourceX1, int sourceY1,
                                         int destinationX0,
                                         int destinationY0,
                                         int destinationX1,
                                         int destinationY1,
                                         unsigned int mask,
                                         unsigned int filter);
using glGenerateMipmapProc_t = void (*)(unsigned int target);
using glBlendEquationProc_t = void (*)(unsigned int mode);
using glGenVertexArraysProc_t = void (*)(int count, unsigned int* arrays);
using glDeleteVertexArraysProc_t = void (*)(int count,
                                            const unsigned int* arrays);
using glBindVertexArrayProc_t = void (*)(unsigned int array);
using glGenQueriesProc_t = void (*)(int count, unsigned int* queries);
using glBeginQueryProc_t = void (*)(unsigned int target, unsigned int query);
using glEndQueryProc_t = void (*)(unsigned int target);
using glGetQueryObjectuivProc_t = void (*)(unsigned int query,
                                           unsigned int parameter,
                                           unsigned int* value);
using glGetQueryObjectui64vProc_t = void (*)(unsigned int query,
                                             unsigned int parameter,
                                             std::uint64_t* value);
using glIsSyncProc_t = unsigned char (*)(void* sync);
using glDeleteSyncProc_t = void (*)(void* sync);
using glFenceSyncProc_t = void* (*)(unsigned int condition,
                                    unsigned int flags);
using glClientWaitSyncProc_t = unsigned int (*)(void* sync,
                                                unsigned int flags,
                                                std::uint64_t timeout);
using glGetInteger64vProc_t = void (*)(unsigned int parameter,
                                       std::int64_t* values);
using glDepthBoundsProc_t = void (*)(double minimum, double maximum);
using glBindSamplerProc_t = void (*)(unsigned int unit,
                                     unsigned int sampler);
using glBindMultiTextureProc_t = void (*)(unsigned int textureUnit,
                                          unsigned int target,
                                          unsigned int texture);
using glClampColorProc_t = void (*)(unsigned int target,
                                    unsigned int clamp);
using glSampleMaskProc_t = void (*)(unsigned int maskNumber,
                                    unsigned int mask);
using glPrimitiveRestartIndexProc_t = void (*)(unsigned int index);
using glFramebufferTexture2DProc_t = void (*)(unsigned int target,
                                              unsigned int attachment,
                                              unsigned int textureTarget,
                                              unsigned int texture,
                                              int level);
using glUseProgramProc_t = void (*)(unsigned int program);
using glUniform4fvProc_t = void (*)(int location, int count,
                                    const float* values);
using glBindBufferRangeProc_t = void (*)(unsigned int target,
                                         unsigned int index,
                                         unsigned int buffer,
                                         std::intptr_t offset,
                                         std::intptr_t size);
using glVertexAttribPointerProc_t = void (*)(unsigned int index, int size,
                                             unsigned int type,
                                             unsigned char normalized,
                                             int stride,
                                             const void* pointer);
using glVertexAttribArrayProc_t = void (*)(unsigned int index);
using glDrawElementsBaseVertexProc_t = void (*)(unsigned int mode, int count,
                                                unsigned int type,
                                                const void* indices,
                                                int baseVertex);
using glGetMultiQueryObjectuivAMDProc_t = void (*)(unsigned int* ids,
                                                   unsigned int count,
                                                   unsigned int parameter,
                                                   unsigned int* values);
using glCreateShaderProc_t = unsigned int (*)(unsigned int type);
using glShaderSourceProc_t = void (*)(unsigned int shader, int count,
                                      const char* const* source,
                                      const int* lengths);
using glCompileShaderProc_t = void (*)(unsigned int shader);
using glGetShaderivProc_t = void (*)(unsigned int shader,
                                     unsigned int parameter, int* value);
using glGetShaderInfoLogProc_t = void (*)(unsigned int shader, int maxLength,
                                          int* length, char* infoLog);
using glDeleteShaderProc_t = void (*)(unsigned int shader);
using glCreateProgramProc_t = unsigned int (*)();
using glDeleteProgramProc_t = void (*)(unsigned int program);
using glProgramBinaryProc_t = void (*)(unsigned int program,
                                       unsigned int binaryFormat,
                                       const void* binary, int length);
using glGetProgramivProc_t = void (*)(unsigned int program,
                                      unsigned int parameter, int* value);
using glAttachShaderProc_t = void (*)(unsigned int program,
                                      unsigned int shader);
using glBindAttribLocationProc_t = void (*)(unsigned int program,
                                            unsigned int index,
                                            const char* name);
using glBindFragDataLocationProc_t = void (*)(unsigned int program,
                                              unsigned int color,
                                              const char* name);
using glProgramParameteriProc_t = void (*)(unsigned int program,
                                           unsigned int parameter,
                                           int value);
using glLinkProgramProc_t = void (*)(unsigned int program);
using glDetachShaderProc_t = void (*)(unsigned int program,
                                      unsigned int shader);
using glGetProgramInfoLogProc_t = void (*)(unsigned int program,
                                           int maxLength, int* length,
                                           char* infoLog);
using glGetProgramBinaryProc_t = void (*)(unsigned int program,
                                          int bufferSize, int* length,
                                          unsigned int* binaryFormat,
                                          void* binary);
using glGetUniformLocationProc_t = int (*)(unsigned int program,
                                           const char* name);
using glGetUniformBlockIndexProc_t = unsigned int (*)(unsigned int program,
                                                       const char* name);
using glUniformBlockBindingProc_t = void (*)(unsigned int program,
                                             unsigned int blockIndex,
                                             unsigned int blockBinding);
using glUniform1iProc_t = void (*)(int location, int value);
using glMapTexture2DIntelProc_t = void* (*)(unsigned int texture, int level,
                                            unsigned int access, int* stride,
                                            unsigned int* layout);
using glUnmapTexture2DIntelProc_t = void (*)(unsigned int texture, int level);
using glCompressedTextureSubImage3DProc_t = void (*)(
    unsigned int texture, unsigned int target, int level, int x, int y, int z,
    int width, int height, int depth, unsigned int format, int imageSize,
    const void* data);
using glTextureSubImage3DProc_t = void (*)(
    unsigned int texture, unsigned int target, int level, int x, int y, int z,
    int width, int height, int depth, unsigned int format, unsigned int type,
    const void* data);
using glCompressedTextureSubImage2DProc_t = void (*)(
    unsigned int texture, unsigned int target, int level, int x, int y,
    int width, int height, unsigned int format, int imageSize,
    const void* data);
using glTextureSubImage2DProc_t = void (*)(
    unsigned int texture, unsigned int target, int level, int x, int y,
    int width, int height, unsigned int format, unsigned int type,
    const void* data);
using glCompressedTexSubImage3DProc_t = void (*)(
    unsigned int target, int level, int x, int y, int z, int width,
    int height, int depth, unsigned int format, int imageSize,
    const void* data);
using glTexSubImage3DProc_t = void (*)(unsigned int target, int level, int x,
                                      int y, int z, int width, int height,
                                      int depth, unsigned int format,
                                      unsigned int type, const void* data);
using glCompressedTexSubImage2DProc_t = void (*)(
    unsigned int target, int level, int x, int y, int width, int height,
    unsigned int format, int imageSize, const void* data);
using glTexStorageSparseAMDProc_t = void (*)(
    unsigned int target, unsigned int internalFormat, int width, int height,
    int depth, int layers, unsigned int flags);
using glCompressedTexImage3DProc_t = void (*)(
    unsigned int target, int level, unsigned int internalFormat, int width,
    int height, int depth, int border, int imageSize, const void* data);
using glTexImage3DProc_t = void (*)(unsigned int target, int level,
                                    int internalFormat, int width, int height,
                                    int depth, int border,
                                    unsigned int format, unsigned int type,
                                    const void* data);
using glCompressedTexImage2DProc_t = void (*)(
    unsigned int target, int level, unsigned int internalFormat, int width,
    int height, int border, int imageSize, const void* data);
using glTexImage2DMultisampleProc_t = void (*)(
    unsigned int target, int samples, int internalFormat, int width,
    int height, unsigned char fixedSampleLocations);
using glActiveTextureProc_t = void (*)(unsigned int texture);
using glGenSamplersProc_t = void (*)(int count, unsigned int* samplers);
using glSamplerParameteriProc_t = void (*)(unsigned int sampler,
                                           unsigned int name, int value);
using glSamplerParameterfProc_t = void (*)(unsigned int sampler,
                                          unsigned int name, float value);
using glSamplerParameterfvProc_t = void (*)(unsigned int sampler,
                                           unsigned int name,
                                           const float* value);
using glGetStringiProc_t = const unsigned char* (*)(unsigned int name,
                                                    unsigned int index);
using glDebugMessageCallbackProc_t = void (*)(
    void (*callback)(unsigned int, unsigned int, unsigned int, unsigned int,
                     int, const char*, const void*),
    const void* userParam);
using glDebugMessageControlProc_t = void (*)(
    unsigned int source, unsigned int type, unsigned int severity, int count,
    const unsigned int* ids, unsigned char enabled);

extern glBindBufferProc_t _glewBindBufferARB;
extern glGenBuffersProc_t _glewGenBuffersARB;
extern glDeleteBuffersProc_t _glewDeleteBuffersARB;
extern glBufferDataProc_t _glewBufferDataARB;
extern glBufferSubDataProc_t _glewBufferSubDataARB;
extern glNamedBufferSubDataProc_t _glewNamedBufferSubDataEXT;
extern glMapBufferProc_t _glewMapBufferARB;
extern glMapBufferRangeProc_t _glewMapBufferRange;
extern glMapNamedBufferProc_t _glewMapNamedBufferEXT;
extern glMapNamedBufferRangeProc_t _glewMapNamedBufferRangeEXT;
extern glUnmapBufferProc_t _glewUnmapBufferARB;
extern glUnmapNamedBufferProc_t _glewUnmapNamedBufferEXT;
extern glBindFramebufferProc_t _glewBindFramebufferEXT;
extern glGenFramebuffersProc_t _glewGenFramebuffersEXT;
extern glDeleteFramebuffersProc_t _glewDeleteFramebuffersEXT;
extern glCheckFramebufferStatusProc_t _glewCheckFramebufferStatusEXT;
extern glDrawBuffersProc_t _glewDrawBuffers;
extern glBlitFramebufferProc_t _glewBlitFramebufferEXT;
extern glGenerateMipmapProc_t _glewGenerateMipmapEXT;
extern glBlendEquationProc_t _glewBlendEquation;
extern glGenVertexArraysProc_t _glewGenVertexArrays;
extern glDeleteVertexArraysProc_t _glewDeleteVertexArrays;
extern glBindVertexArrayProc_t _glewBindVertexArray;
extern glGenQueriesProc_t _glewGenQueries;
extern glBeginQueryProc_t _glewBeginQuery;
extern glEndQueryProc_t _glewEndQuery;
extern glGetQueryObjectuivProc_t _glewGetQueryObjectuivARB;
extern glGetQueryObjectui64vProc_t _glewGetQueryObjectui64vEXT;
extern glIsSyncProc_t _glewIsSync;
extern glDeleteSyncProc_t _glewDeleteSync;
extern glFenceSyncProc_t _glewFenceSync;
extern glClientWaitSyncProc_t _glewClientWaitSync;
extern glGetInteger64vProc_t _glewGetInteger64v;
extern glDepthBoundsProc_t _glewDepthBoundsEXT;
extern glBindSamplerProc_t _glewBindSampler;
extern glGenSamplersProc_t _glewGenSamplers;
extern glSamplerParameteriProc_t _glewSamplerParameteri;
extern glSamplerParameterfProc_t _glewSamplerParameterf;
extern glSamplerParameterfvProc_t _glewSamplerParameterfv;
extern glBindMultiTextureProc_t _glewBindMultiTextureEXT;
extern glClampColorProc_t _glewClampColorARB;
extern glSampleMaskProc_t _glewSampleMaski;
extern glPrimitiveRestartIndexProc_t _glewPrimitiveRestartIndex;
extern glFramebufferTexture2DProc_t _glewFramebufferTexture2DEXT;
extern glUseProgramProc_t _glewUseProgram;
extern glUniform4fvProc_t _glewUniform4fv;
extern glBindBufferRangeProc_t _glewBindBufferRange;
extern glVertexAttribPointerProc_t _glewVertexAttribPointerARB;
extern glVertexAttribArrayProc_t _glewEnableVertexAttribArrayARB;
extern glVertexAttribArrayProc_t _glewDisableVertexAttribArrayARB;
extern glDrawElementsBaseVertexProc_t _glewDrawElementsBaseVertex;
extern glGetMultiQueryObjectuivAMDProc_t glGetMultiQueryObjectuivAMD;
extern glCreateShaderProc_t _glewCreateShader;
extern glShaderSourceProc_t _glewShaderSource;
extern glCompileShaderProc_t _glewCompileShader;
extern glGetShaderivProc_t _glewGetShaderiv;
extern glGetShaderInfoLogProc_t _glewGetShaderInfoLog;
extern glDeleteShaderProc_t _glewDeleteShader;
extern glCreateProgramProc_t _glewCreateProgram;
extern glDeleteProgramProc_t _glewDeleteProgram;
extern glProgramBinaryProc_t _glewProgramBinary;
extern glGetProgramivProc_t _glewGetProgramiv;
extern glAttachShaderProc_t _glewAttachShader;
extern glBindAttribLocationProc_t _glewBindAttribLocation;
extern glBindFragDataLocationProc_t _glewBindFragDataLocation;
extern glProgramParameteriProc_t _glewProgramParameteri;
extern glLinkProgramProc_t _glewLinkProgram;
extern glDetachShaderProc_t _glewDetachShader;
extern glGetProgramInfoLogProc_t _glewGetProgramInfoLog;
extern glGetProgramBinaryProc_t _glewGetProgramBinary;
extern glGetUniformLocationProc_t _glewGetUniformLocation;
extern glGetUniformBlockIndexProc_t _glewGetUniformBlockIndex;
extern glUniformBlockBindingProc_t _glewUniformBlockBinding;
extern glUniform1iProc_t _glewUniform1i;
extern glMapTexture2DIntelProc_t _glewMapTexture2DINTEL;
extern glUnmapTexture2DIntelProc_t _glewUnmapTexture2DINTEL;
extern glCompressedTextureSubImage3DProc_t
    _glewCompressedTextureSubImage3DEXT;
extern glTextureSubImage3DProc_t _glewTextureSubImage3DEXT;
extern glCompressedTextureSubImage2DProc_t
    _glewCompressedTextureSubImage2DEXT;
extern glTextureSubImage2DProc_t _glewTextureSubImage2DEXT;
extern glCompressedTexSubImage3DProc_t _glewCompressedTexSubImage3DARB;
extern glTexSubImage3DProc_t _glewTexSubImage3D;
extern glCompressedTexSubImage2DProc_t _glewCompressedTexSubImage2DARB;
extern glTexStorageSparseAMDProc_t _glewTexStorageSparseAMD;
extern glCompressedTexImage3DProc_t _glewCompressedTexImage3DARB;
extern glTexImage3DProc_t _glewTexImage3D;
extern glCompressedTexImage2DProc_t _glewCompressedTexImage2DARB;
extern glTexImage2DMultisampleProc_t _glewTexImage2DMultisample;
extern glActiveTextureProc_t _glewActiveTextureARB;
extern glGetStringiProc_t _glewGetStringi;
extern glDebugMessageCallbackProc_t _glewDebugMessageCallbackARB;
extern glDebugMessageControlProc_t _glewDebugMessageControlARB;

extern unsigned char _GLEW_ATI_meminfo;
extern unsigned char _GLEW_NVX_gpu_memory_info;

void qglGetMultiQueryObjectuivAMD(unsigned int* ids, unsigned int count,
                                  unsigned int parameter,
                                  unsigned int* values);
unsigned int GL_GetError();
void GL_CheckErrors(bool force);

extern "C" void glBindTexture(unsigned int target, unsigned int texture);
extern "C" unsigned int glGetError();
extern "C" const unsigned char* glGetString(unsigned int name);
extern "C" void glGetIntegerv(unsigned int parameter, int* values);
extern "C" void glFlush();
extern "C" void glFinish();
extern "C" void glFrontFace(unsigned int mode);
extern "C" void glDisable(unsigned int capability);
extern "C" void glEnable(unsigned int capability);
extern "C" void glCullFace(unsigned int mode);
extern "C" void glDepthFunc(unsigned int function);
extern "C" void glBlendFunc(unsigned int source, unsigned int destination);
extern "C" void glDepthMask(unsigned char enabled);
extern "C" void glColorMask(unsigned char red, unsigned char green,
                             unsigned char blue, unsigned char alpha);
extern "C" void glPolygonMode(unsigned int face, unsigned int mode);
extern "C" void glPolygonOffset(float factor, float units);
extern "C" void glStencilFunc(unsigned int function, int reference,
                               unsigned int mask);
extern "C" void glStencilOp(unsigned int stencilFail,
                             unsigned int depthFail,
                             unsigned int depthPass);
extern "C" void glScissor(int x, int y, int width, int height);
extern "C" void glViewport(int x, int y, int width, int height);
extern "C" void glClearColor(float red, float green, float blue,
                              float alpha);
extern "C" void glClearStencil(int value);
extern "C" void glClear(unsigned int mask);
extern "C" void glClearDepth(double depth);
extern "C" void glShadeModel(unsigned int mode);
extern "C" void glColor4f(float red, float green, float blue, float alpha);
extern "C" void glAlphaFunc(unsigned int function, float reference);
extern "C" void glTexSubImage2D(unsigned int target, int level, int xOffset,
                                int yOffset, int width, int height,
                                unsigned int format, unsigned int type,
                                const void* pixels);
extern "C" void glPixelStorei(unsigned int parameter, int value);
extern "C" void glDeleteTextures(int count, const unsigned int* textures);
extern "C" void glGenTextures(int count, unsigned int* textures);
extern "C" void glTexParameteri(unsigned int target, unsigned int parameter,
                                 int value);
extern "C" void glTexImage2D(unsigned int target, int level,
                              int internalFormat, int width, int height,
                              int border, unsigned int format,
                              unsigned int type, const void* pixels);
extern "C" void glCopyTexSubImage2D(unsigned int target, int level,
                                     int xOffset, int yOffset, int x, int y,
                                     int width, int height);
extern "C" void glReadPixels(int x, int y, int width, int height,
                              unsigned int format, unsigned int type,
                              void* pixels);
extern "C" void glReadBuffer(unsigned int mode);
extern "C" void glDrawBuffer(unsigned int mode);
extern "C" void glGetTexImage(unsigned int target, int level,
                               unsigned int format, unsigned int type,
                               void* pixels);
