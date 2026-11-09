#pragma once

#include <cstdint>

class idDeclRenderProg;
class idImage;
class idRenderModelSurface;
class idTextureSampler;
class idRenderDestination;
class idTriangles;
class idVertexBuffer;
class idIndexBuffer;

constexpr int MAX_OCCLUSION_QUERIES = 4096;
constexpr int OCCLUSION_QUERY_TOO_OLD = -1;

template <typename T> class idODSObject {
public:
  const T* objectPtr;
};

struct wrapperContext_t {
  bool disableDepthBoundsTest;
  bool useTimerQuery;
  bool useUniformArrays;
  unsigned char alignmentPadding;
  unsigned int queries[16][256];
};

struct wrapperConfig_t {
  bool waitForOcclusionQuery;
  bool forceTwoSided;
  bool disableStateCaching;
  bool lazyBindPrograms;
  bool lazyBindParms;
  bool lazyBindTextures;
  bool stripFragmentBranches;
  bool skipDetailTris;
  bool singleTriangle;
  bool finishEveryDraw;
  bool syncAtEndFrame;
  unsigned char useGLArbSync;
  bool useGLVAO;
  bool skipGpuSyncAtEndOfFrame;
  unsigned char alignmentPadding[2];
  float printSyncGpuStatsThreshold;
  bool useMultiQuerry;
  unsigned char alignmentPadding2[3];
  float defaultPolyOfsFactor;
  float defaultPolyOfsUnits;
};

struct wrapperStats_t {
  int c_queriesIssued;
  int c_queriesPassed;
  int c_queriesWaitTime;
  int c_queriesTooOld;
  int c_programsBound;
  int c_texturesBound;
  int c_drawElements;
  int c_drawIndices;
  int c_drawVertices;
};

extern wrapperContext_t wrapperContext;
extern wrapperConfig_t wrapperConfig;
extern wrapperStats_t wrapperStats;

void GL_UnbindVAO();
void GL_SetWrapperContext(const wrapperContext_t& context);
void GL_SetWrapperConfig(const wrapperConfig_t& config);
void GL_StartFrame(int frame);
void GL_EndFrame();
void GL_WaitForEndFrame();
void GL_GetLastFrameTime(std::uint64_t& startGPUTimeMicroSec,
                         std::uint64_t& endGPUTimeMicroSec);

void GL_SetDefaultState();
void GL_SetRenderDestination(
    const idODSObject<idRenderDestination>& destination, int level, int side);
void GL_Viewport(int x, int y, int width, int height);
void GL_ViewportAndScissor(int x, int y, int width, int height);
void GL_Scissor(int x, int y, int width, int height);
void GL_Clear(bool color, bool depth, bool stencil, unsigned char stencilValue,
              float red, float green, float blue, float alpha);
int GL_GetDeferredQueryResult(std::int64_t queryNumber);
void GL_Flush();
void GL_Finish();
void GL_BeginQueryBatch();
std::int64_t GL_GetCurrentQueryNumber();
void GL_CacheOcclusionQueryBatches(std::int64_t upToIncludingBatchNum);
void GL_DepthBoundsTest(float minimum, float maximum);
void GL_State(std::uint64_t stateBits, bool forceState);
void GL_PolygonOffset(float scale, float bias, bool fill);
void GL_BeginQuery(std::int64_t* queryNumber);
void GL_EndQuery();
void GL_BindTexture(int textureUnit, const idImage* image,
                    const idTextureSampler* sampler);
void GL_ResetProgramState();
void GL_ResetTextureState();
wrapperStats_t GL_GetCurrentStats();
void GL_ClearStats();
void GL_BindProgram(const idODSObject<idDeclRenderProg>& program,
                    std::uint64_t extraState, unsigned int triangleVertexMask);
void GL_ClearSurfaceParms(const idDeclRenderProg* program,
                          const idRenderModelSurface* surface);
void* GL_BuildVAO(const idVertexBuffer& vertexBuffer,
                  const idIndexBuffer& indexBuffer,
                  const idVertexBuffer* morphMap,
                  const idVertexBuffer* stMap, int vertexMask);
void GL_DeleteVertexBufferVAO(const void* apiObject);
void GL_DeleteIndexBufferVAO(const void* apiObject);
void GL_DrawElements(const idODSObject<idDeclRenderProg>& program,
                     const idODSObject<idTriangles>& triangles,
                     std::uint64_t extraState, bool skipDetailTriangles);
void GL_DrawElements(const idODSObject<idDeclRenderProg>& program,
                     const idODSObject<idRenderModelSurface>& surface,
                     std::uint64_t extraState, bool skipDetailTriangles);

static_assert(sizeof(idODSObject<void>) == sizeof(void*),
              "ODS object ABI drift");
static_assert(sizeof(wrapperContext_t) == 0x4004,
              "wrapper context ABI drift");
static_assert(sizeof(wrapperConfig_t) == 0x20,
              "wrapper config ABI drift");
static_assert(sizeof(wrapperStats_t) == 0x24,
              "wrapper stats ABI drift");
