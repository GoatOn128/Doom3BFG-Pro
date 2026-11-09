#include "engine/renderer/jobs/render/graphicsapiwrapper.h"

#include "engine/models/rendermodel.h"
#include "engine/renderer/declrenderprog.h"
#include "engine/renderer/gl_api.h"
#include "engine/renderer/glconfig.h"
#include "engine/renderer/image.h"
#include "engine/renderer/jobs/render/parmstate.h"
#include "engine/renderer/renderdestination.h"
#include "engine/renderer/renderlog.h"
#include "engine/renderer/texturesampler.h"
#include "engine/renderer/triangles.h"
#include "shared/idlib/containers/hashindex.h"
#include "shared/idlib/containers/list.h"
#include "shared/idlib/lib.h"
#include "shared/idlib/sys/sys_alloc.h"
#include "shared/idlib/sys/sys_time.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>

constexpr unsigned int GL_ZERO_VALUE = 0;
constexpr unsigned int GL_ONE_VALUE = 1;
constexpr unsigned int GL_FRONT_VALUE = 0x0404;
constexpr unsigned int GL_BACK_VALUE = 0x0405;
constexpr unsigned int GL_FRONT_AND_BACK_VALUE = 0x0408;
constexpr unsigned int GL_CW_VALUE = 0x0900;
constexpr unsigned int GL_CCW_VALUE = 0x0901;
constexpr unsigned int GL_CULL_FACE_VALUE = 0x0B44;
constexpr unsigned int GL_LIGHTING_VALUE = 0x0B50;
constexpr unsigned int GL_DEPTH_TEST_VALUE = 0x0B71;
constexpr unsigned int GL_STENCIL_TEST_VALUE = 0x0B90;
constexpr unsigned int GL_BLEND_VALUE = 0x0BE2;
constexpr unsigned int GL_SCISSOR_TEST_VALUE = 0x0C11;
constexpr unsigned int GL_TEXTURE_2D_VALUE = 0x0DE1;
constexpr unsigned int GL_BYTE_VALUE = 0x1400;
constexpr unsigned int GL_UNSIGNED_BYTE_VALUE = 0x1401;
constexpr unsigned int GL_SHORT_VALUE = 0x1402;
constexpr unsigned int GL_UNSIGNED_SHORT_VALUE = 0x1403;
constexpr unsigned int GL_UNSIGNED_INT_VALUE = 0x1405;
constexpr unsigned int GL_FLOAT_VALUE = 0x1406;
constexpr unsigned int GL_NEVER_VALUE = 0x0200;
constexpr unsigned int GL_LESS_VALUE = 0x0201;
constexpr unsigned int GL_EQUAL_VALUE = 0x0202;
constexpr unsigned int GL_LEQUAL_VALUE = 0x0203;
constexpr unsigned int GL_GREATER_VALUE = 0x0204;
constexpr unsigned int GL_NOTEQUAL_VALUE = 0x0205;
constexpr unsigned int GL_GEQUAL_VALUE = 0x0206;
constexpr unsigned int GL_ALWAYS_VALUE = 0x0207;
constexpr unsigned int GL_SRC_COLOR_VALUE = 0x0300;
constexpr unsigned int GL_ONE_MINUS_SRC_COLOR_VALUE = 0x0301;
constexpr unsigned int GL_SRC_ALPHA_VALUE = 0x0302;
constexpr unsigned int GL_ONE_MINUS_SRC_ALPHA_VALUE = 0x0303;
constexpr unsigned int GL_DST_ALPHA_VALUE = 0x0304;
constexpr unsigned int GL_ONE_MINUS_DST_ALPHA_VALUE = 0x0305;
constexpr unsigned int GL_DST_COLOR_VALUE = 0x0306;
constexpr unsigned int GL_ONE_MINUS_DST_COLOR_VALUE = 0x0307;
constexpr unsigned int GL_KEEP_VALUE = 0x1E00;
constexpr unsigned int GL_REPLACE_VALUE = 0x1E01;
constexpr unsigned int GL_INCR_VALUE = 0x1E02;
constexpr unsigned int GL_DECR_VALUE = 0x1E03;
constexpr unsigned int GL_INVERT_VALUE = 0x150A;
constexpr unsigned int GL_INCR_WRAP_VALUE = 0x8507;
constexpr unsigned int GL_DECR_WRAP_VALUE = 0x8508;
constexpr unsigned int GL_LINE_VALUE = 0x1B01;
constexpr unsigned int GL_FILL_VALUE = 0x1B02;
constexpr unsigned int GL_SMOOTH_VALUE = 0x1D01;
constexpr unsigned int GL_TRIANGLES_VALUE = 0x0004;
constexpr unsigned int GL_TRIANGLE_STRIP_VALUE = 0x0005;
constexpr unsigned int GL_COLOR_BUFFER_BIT_VALUE = 0x4000;
constexpr unsigned int GL_DEPTH_BUFFER_BIT_VALUE = 0x0100;
constexpr unsigned int GL_STENCIL_BUFFER_BIT_VALUE = 0x0400;
constexpr unsigned int GL_POLYGON_OFFSET_LINE_VALUE = 0x2A02;
constexpr unsigned int GL_POLYGON_OFFSET_FILL_VALUE = 0x8037;
constexpr unsigned int GL_SAMPLE_COVERAGE_VALUE = 0x80A0;
constexpr unsigned int GL_SAMPLE_ALPHA_TO_COVERAGE_VALUE = 0x809E;
constexpr unsigned int GL_ARRAY_BUFFER_VALUE = 0x8892;
constexpr unsigned int GL_ELEMENT_ARRAY_BUFFER_VALUE = 0x8893;
constexpr unsigned int GL_DEPTH_BOUNDS_TEST_EXT_VALUE = 0x8890;
constexpr unsigned int GL_CLAMP_VERTEX_COLOR_ARB_VALUE = 0x891A;
constexpr unsigned int GL_CLAMP_FRAGMENT_COLOR_ARB_VALUE = 0x891B;
constexpr unsigned int GL_CLAMP_READ_COLOR_ARB_VALUE = 0x891C;
constexpr unsigned int GL_SAMPLES_PASSED_VALUE = 0x8914;
constexpr unsigned int GL_QUERY_RESULT_VALUE = 0x8866;
constexpr unsigned int GL_QUERY_RESULT_AVAILABLE_VALUE = 0x8867;
constexpr unsigned int GL_TIME_ELAPSED_VALUE = 0x88BF;
constexpr unsigned int GL_UNIFORM_BUFFER_VALUE = 0x8A11;
constexpr unsigned int GL_FRAMEBUFFER_VALUE = 0x8D40;
constexpr unsigned int GL_COLOR_ATTACHMENT0_VALUE = 0x8CE0;
constexpr unsigned int GL_DEPTH_ATTACHMENT_VALUE = 0x8D00;
constexpr unsigned int GL_STENCIL_ATTACHMENT_VALUE = 0x8D20;
constexpr unsigned int GL_TEXTURE_CUBE_MAP_POSITIVE_X_VALUE = 0x8515;
constexpr unsigned int GL_TEXTURE_2D_MULTISAMPLE_VALUE = 0x9100;
constexpr unsigned int GL_SAMPLE_MASK_VALUE = 0x8E51;
constexpr unsigned int GL_TEXTURE_CUBE_MAP_SEAMLESS_VALUE = 0x884F;
constexpr unsigned int GL_PRIMITIVE_RESTART_FIXED_INDEX_VALUE = 0x8D69;
constexpr unsigned int GL_PRIMITIVE_RESTART_VALUE = 0x8F9D;
constexpr unsigned int GL_FUNC_ADD_VALUE = 0x8006;
constexpr unsigned int GL_FUNC_REVERSE_SUBTRACT_VALUE = 0x800B;
constexpr unsigned int GL_MIN_VALUE = 0x8007;
constexpr unsigned int GL_MAX_VALUE = 0x8008;
constexpr unsigned int GL_SYNC_GPU_COMMANDS_COMPLETE_VALUE = 0x9117;
constexpr unsigned int GL_SYNC_FLUSH_COMMANDS_BIT_VALUE = 1;
constexpr unsigned int GL_ALREADY_SIGNALED_VALUE = 0x911A;
constexpr unsigned int GL_TIMEOUT_EXPIRED_VALUE = 0x911B;
constexpr unsigned int GL_CONDITION_SATISFIED_VALUE = 0x911C;
constexpr unsigned int GL_WAIT_FAILED_VALUE = 0x911D;
constexpr unsigned int GL_VBO_FREE_MEMORY_ATI_VALUE = 0x87FB;
constexpr unsigned int GL_TEXTURE_FREE_MEMORY_ATI_VALUE = 0x87FC;
constexpr unsigned int GL_RENDERBUFFER_FREE_MEMORY_ATI_VALUE = 0x87FD;
constexpr unsigned int GL_GPU_MEMORY_INFO_DEDICATED_VIDMEM_NVX_VALUE = 0x9047;
constexpr unsigned int GL_GPU_MEMORY_INFO_TOTAL_AVAILABLE_MEMORY_NVX_VALUE =
    0x9048;
constexpr unsigned int GL_GPU_MEMORY_INFO_CURRENT_AVAILABLE_VIDMEM_NVX_VALUE =
    0x9049;
constexpr unsigned int GL_GPU_MEMORY_INFO_EVICTION_COUNT_NVX_VALUE = 0x904A;
constexpr unsigned int GL_GPU_MEMORY_INFO_EVICTED_MEMORY_NVX_VALUE = 0x904B;

constexpr std::uint64_t GLS_MIRROR_VIEW = 0x0004000000000000ULL;

unsigned int dummyVAOMain = 0;
unsigned int dummyVAORender = 0;
unsigned int dummyVAO = 0;

constexpr unsigned int glRenderTarget[4] = {
    GL_COLOR_ATTACHMENT0_VALUE, GL_COLOR_ATTACHMENT0_VALUE + 1,
    GL_COLOR_ATTACHMENT0_VALUE + 2, GL_COLOR_ATTACHMENT0_VALUE + 3};

const idVertexScaleAndBias defaultVSB = {
    {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f},
    {1.0f, 1.0f},       {0.0f, 0.0f}};
constexpr idVec4 vec4Origin = {0.0f, 0.0f, 0.0f, 0.0f};

enum queryState_t : int {
  QUERY_INVALID = 0,
  QUERY_STARTED = 1,
  QUERY_STOPPED = 2,
};

struct renderStateGL_t {
  int frameNumber{};
  std::uint64_t currentState{};
  std::int64_t currentOcclusionQueryBatch{};
  std::int64_t currentOcclusionQuery{};
  unsigned int totalQueriesInBatch[16]{};
  unsigned int cachedOcclusionResult[16][256]{};
  std::int64_t lastCachedOcclusionBatch{};
  unsigned int timeQueryId{};
  queryState_t timeQueryPending{};
  std::uint64_t startGPUTimeMicroSec{};
  std::uint64_t endGPUTimeMicroSec{};
  void* renderSync{};
  int currentNumTargets{};
  bool defaultViewPort{};
  unsigned char alignmentPadding[3]{};
  float polyOfsScale{};
  float polyOfsBias{};
  bool polyOfsFill{};
  unsigned char alignmentPadding2[7]{};
  const idDeclRenderProg* boundProgram{};
  std::uint64_t boundVertexProgramChecksum{};
  std::uint64_t boundFragmentProgramChecksum{};
  std::uint64_t boundVertexParmVersion{};
  std::uint64_t boundFragmentParmVersion{};
  std::uint64_t boundTextureParmVersion{};
  std::uint64_t boundSamplerParmVersion{};
  int boundTextureUnits{};
  unsigned char alignmentPadding3[4]{};
  const idImage* boundTextures[16]{};
  const idTextureSampler* boundSamplers[16]{};
  bool boundTexturesChanged{};
  unsigned char alignmentPadding4[7]{};
  void* boundVertexBuffer{};
  void* boundIndexBuffer{};
  unsigned int boundVertexAttribArrays{};
  unsigned int boundVertexAttribPointers{};
  void* boundVAO{};
  idVec4 hardwareSkinning{};
};

extern renderStateGL_t renderStateGL;

struct alignas(8) vaoInfo_t {
  const void* vao{};
  const void* vb{};
  const void* ib{};
  const void* mb{};
  const void* stb{};
  unsigned int vertexMask{};
  unsigned int alignmentPadding{};
};

std::uint32_t PointerKey(const void* pointer) {
  return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(pointer));
}

int VaoHashKey(const void* vb, const void* ib, const void* mb,
               const void* stb, unsigned int vertexMask) {
  std::uint32_t key = vertexMask;
  key = PointerKey(vb) + 31U * key;
  key = PointerKey(ib) + 31U * key;
  key = PointerKey(mb) + 31U * key;
  key = PointerKey(stb) + 31U * key;
  return static_cast<int>(key);
}

unsigned int GlName(const void* apiObject) {
  return static_cast<unsigned int>(
      reinterpret_cast<std::uintptr_t>(apiObject));
}

void* ApiObject(unsigned int name) {
  return reinterpret_cast<void*>(static_cast<std::uintptr_t>(name));
}

unsigned int DecodeStencilOp(std::uint64_t code) {
  switch (code & 7U) {
    case 0:
      return GL_KEEP_VALUE;
    case 1:
      return GL_ZERO_VALUE;
    case 2:
      return GL_REPLACE_VALUE;
    case 3:
      return GL_INCR_VALUE;
    case 4:
      return GL_DECR_VALUE;
    case 5:
      return GL_INVERT_VALUE;
    case 6:
      return GL_INCR_WRAP_VALUE;
    default:
      return GL_DECR_WRAP_VALUE;
  }
}

unsigned int FloatBits(float value) {
  unsigned int bits;
  std::memcpy(&bits, &value, sizeof(bits));
  return bits;
}

int UniformLocation(void* encodedLocation) {
  return static_cast<int>(reinterpret_cast<std::uintptr_t>(encodedLocation)) -
         1;
}

int VertexStride(unsigned int vertexMask) {
  int stride = 0;
  stride += (vertexMask & 1U) != 0U ? 12 : 0;
  stride += static_cast<int>((vertexMask >> 1U) & 4U);
  stride += static_cast<int>((vertexMask >> 2U) & 4U);
  stride += static_cast<int>((vertexMask >> 9U) & 4U);
  stride += (vertexMask & 0x1000U) != 0U ? 12 : 0;
  stride += 4 * static_cast<int>(vertexMask & 2U);
  stride += static_cast<int>(vertexMask & 4U);
  stride += (vertexMask & 0x20U) != 0U ? -4 : 0;
  stride += (vertexMask & 0x40U) != 0U ? -4 : 0;
  return stride;
}

void GetMultiQueryResults(unsigned int* ids, int count,
                          unsigned int parameter, unsigned int* values) {
  if (wrapperConfig.useMultiQuerry) {
    glGetMultiQueryObjectuivAMD(ids, count, parameter, values);
  } else {
    qglGetMultiQueryObjectuivAMD(ids, count, parameter, values);
  }
}

void WaitForRenderSync(bool reportErrors) {
  if (glConfig.syncAvailable && _glewIsSync(renderStateGL.renderSync) != 0 &&
      wrapperConfig.useGLArbSync != 0) {
    unsigned int waitResult;
    do {
      const std::uint64_t timeout =
          wrapperConfig.useGLArbSync == 2 ? 1000000000ULL : 1000000ULL;
      waitResult = _glewClientWaitSync(
          renderStateGL.renderSync, GL_SYNC_FLUSH_COMMANDS_BIT_VALUE, timeout);
    } while (waitResult == GL_TIMEOUT_EXPIRED_VALUE);

    if (reportErrors && waitResult == GL_WAIT_FAILED_VALUE) {
      idLib::Warning(
          "GL_WaitForEndFrame: glClientWaitSync returned GL_WAIT_FAILED, "
          "glerror: 0x%x",
          GL_GetError());
    } else if (reportErrors && waitResult != GL_CONDITION_SATISFIED_VALUE &&
               waitResult != GL_ALREADY_SIGNALED_VALUE) {
      idLib::Warning(
          "GL_WaitForEndFrame: glClientWaitSync returned unknown value: "
          "0x%x",
          waitResult);
    }
  } else {
    glFinish();
  }
}

void WaitAndPrintRenderSync(bool reportErrors) {
  const int start = static_cast<int>(Sys_Microseconds());
  WaitForRenderSync(reportErrors);
  const int elapsed = static_cast<int>(Sys_Microseconds()) - start;
  if (wrapperConfig.printSyncGpuStatsThreshold != -1.0f &&
      static_cast<double>(elapsed) >
          static_cast<double>(wrapperConfig.printSyncGpuStatsThreshold) *
              1000000.0) {
    idLib::Printf("sync waited %7.3fms\n", static_cast<double>(elapsed) * 0.001);
  }
}

class idVaoContainer {
public:
  idVaoContainer();
  ~idVaoContainer();

  void* FindVaoInfo(const void* vb, const void* ib, const void* mb,
                    const void* stb, unsigned int vertexMask) const;
  void AddVaoInfo(const void* vao, const void* vb, const void* ib,
                  const void* mb, const void* stb, unsigned int vertexMask);
  void RemoveVaoInfo(int index);
  void RemoveVertexBuffer(const void* apiObject);

  idList<vaoInfo_t, 5> vaoInfo;
  idList<unsigned int, 5> freeList;
  idHashIndex vaoHash;
  idHashIndex vbVaoHash;
  idHashIndex ibVaoHash;
  idHashIndex mbVaoHash;
  idHashIndex stbVaoHash;
};

renderStateGL_t renderStateGL;
idVaoContainer vaoContainer;

wrapperContext_t wrapperContext{};
wrapperConfig_t wrapperConfig{};
wrapperStats_t wrapperStats{};
glConfig_t glConfig{};
std::int64_t mem_GPUMem[6]{};
const char* mem_GPUNames[6]{};

static_assert(offsetof(renderStateGL_t, currentState) == 0x08,
              "GL render-state state-vector offset drift");
static_assert(offsetof(renderStateGL_t, cachedOcclusionResult) == 0x60,
              "GL render-state query-cache offset drift");
static_assert(offsetof(renderStateGL_t, lastCachedOcclusionBatch) == 0x4060,
              "GL render-state cached-batch offset drift");
static_assert(offsetof(renderStateGL_t, boundProgram) == 0x40A0,
              "GL render-state bound-program offset drift");
static_assert(offsetof(renderStateGL_t, boundTextures) == 0x40E0,
              "GL render-state bound-texture offset drift");
static_assert(offsetof(renderStateGL_t, boundVAO) == 0x4200,
              "GL render-state VAO offset drift");
static_assert(sizeof(renderStateGL_t) == 0x4218,
              "GL render-state ABI drift");
static_assert(sizeof(vaoInfo_t) == 48, "VAO-info ABI drift");
static_assert(sizeof(idVaoContainer) == 0xF8,
              "VAO-container ABI drift");

void GL_UnbindVAO() {
  unsigned int& threadDummy =
      idLib::IsMainThread() ? dummyVAOMain : dummyVAORender;
  if (threadDummy == 0) {
    _glewGenVertexArrays(1, &threadDummy);
  }
  dummyVAO = threadDummy;
  _glewBindVertexArray(threadDummy);
}

void GL_SetWrapperContext(const wrapperContext_t& context) {
  std::memmove(&wrapperContext, &context, 0x4004);
  renderStateGL.lastCachedOcclusionBatch = 16;
  renderStateGL.currentOcclusionQueryBatch = -1;
  renderStateGL.currentOcclusionQuery = 1;
}

void GL_SetWrapperConfig(const wrapperConfig_t& config) {
  wrapperConfig = config;
}

void GL_StartFrame(int frame) {
  renderStateGL.frameNumber = frame;
  if (!wrapperContext.useTimerQuery) {
    return;
  }
  if (renderStateGL.timeQueryId == 0) {
    _glewGenQueries(1, &renderStateGL.timeQueryId);
  }
  if (renderStateGL.timeQueryPending == QUERY_INVALID) {
    _glewBeginQuery(GL_TIME_ELAPSED_VALUE, renderStateGL.timeQueryId);
    renderStateGL.timeQueryPending = QUERY_STARTED;
  }
}

void GL_EndFrame() {
  if (renderStateGL.timeQueryPending == QUERY_STARTED) {
    _glewEndQuery(GL_TIME_ELAPSED_VALUE);
    renderStateGL.timeQueryPending = QUERY_STOPPED;
  }

  if (glConfig.syncAvailable && wrapperConfig.useGLArbSync != 0) {
    if (_glewIsSync(renderStateGL.renderSync) != 0) {
      _glewDeleteSync(renderStateGL.renderSync);
    }
    renderStateGL.renderSync =
        _glewFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE_VALUE, 0);
    if (renderStateGL.renderSync == nullptr) {
      idLib::Warning("glFenceSync: return zero fence");
    }
  }

  if (_GLEW_ATI_meminfo != 0) {
    std::int64_t vbo[4]{};
    std::int64_t texture[4]{};
    std::int64_t renderbuffer[4]{};
    _glewGetInteger64v(GL_VBO_FREE_MEMORY_ATI_VALUE, vbo);
    _glewGetInteger64v(GL_TEXTURE_FREE_MEMORY_ATI_VALUE, texture);
    _glewGetInteger64v(GL_RENDERBUFFER_FREE_MEMORY_ATI_VALUE, renderbuffer);
    mem_GPUMem[0] = vbo[0];
    mem_GPUMem[1] = vbo[1];
    mem_GPUMem[2] = texture[0];
    mem_GPUMem[3] = texture[1];
    mem_GPUMem[4] = renderbuffer[0];
    mem_GPUMem[5] = renderbuffer[1];
    mem_GPUNames[0] = "VBOFree";
    mem_GPUNames[1] = "VBOLargest";
    mem_GPUNames[2] = "TexFree";
    mem_GPUNames[3] = "TexLargest";
    mem_GPUNames[4] = "RenderFree";
    mem_GPUNames[5] = "RenderLargest";
  } else if (_GLEW_NVX_gpu_memory_info != 0) {
    _glewGetInteger64v(GL_GPU_MEMORY_INFO_DEDICATED_VIDMEM_NVX_VALUE,
                       &mem_GPUMem[0]);
    _glewGetInteger64v(GL_GPU_MEMORY_INFO_TOTAL_AVAILABLE_MEMORY_NVX_VALUE,
                       &mem_GPUMem[1]);
    _glewGetInteger64v(
        GL_GPU_MEMORY_INFO_CURRENT_AVAILABLE_VIDMEM_NVX_VALUE,
        &mem_GPUMem[2]);
    _glewGetInteger64v(GL_GPU_MEMORY_INFO_EVICTION_COUNT_NVX_VALUE,
                       &mem_GPUMem[3]);
    _glewGetInteger64v(GL_GPU_MEMORY_INFO_EVICTED_MEMORY_NVX_VALUE,
                       &mem_GPUMem[4]);
    mem_GPUNames[0] = "MemSize";
    mem_GPUNames[1] = "TotalMem";
    mem_GPUNames[2] = "CurrentMem";
    mem_GPUNames[3] = "EvictCount";
    mem_GPUNames[4] = "EvictSize";
  }

  if (wrapperConfig.syncAtEndFrame &&
      !wrapperConfig.skipGpuSyncAtEndOfFrame) {
    WaitAndPrintRenderSync(true);
  }
}

void GL_WaitForEndFrame() {
  if (!wrapperConfig.skipGpuSyncAtEndOfFrame &&
      !wrapperConfig.syncAtEndFrame) {
    WaitAndPrintRenderSync(false);
  }

  if (wrapperContext.useTimerQuery &&
      renderStateGL.timeQueryPending == QUERY_STOPPED) {
    const std::uint64_t endTime = Sys_Microseconds();
    std::uint64_t gpuFrameNanoseconds = 0;
    _glewGetQueryObjectui64vEXT(renderStateGL.timeQueryId,
                                GL_QUERY_RESULT_VALUE,
                                &gpuFrameNanoseconds);
    renderStateGL.endGPUTimeMicroSec = endTime;
    renderStateGL.timeQueryPending = QUERY_INVALID;
    renderStateGL.startGPUTimeMicroSec =
        endTime - gpuFrameNanoseconds / 1000ULL;
  }
}

void GL_GetLastFrameTime(std::uint64_t& startGPUTimeMicroSec,
                         std::uint64_t& endGPUTimeMicroSec) {
  if (wrapperContext.useTimerQuery) {
    startGPUTimeMicroSec = renderStateGL.startGPUTimeMicroSec;
    endGPUTimeMicroSec = renderStateGL.endGPUTimeMicroSec;
  } else {
    startGPUTimeMicroSec = 0;
    endGPUTimeMicroSec = 0;
  }
}

void GL_Flush() { glFlush(); }

void GL_Finish() { glFinish(); }

void GL_State(std::uint64_t stateBits, bool forceGlState) {
  std::uint64_t newState = stateBits;
  if (wrapperConfig.forceTwoSided) {
    newState |= 0x8000ULL;
  }

  const std::uint64_t difference =
      (wrapperConfig.disableStateCaching || forceGlState)
          ? ~std::uint64_t{0}
          : newState ^ renderStateGL.currentState;
  if (difference == 0) {
    return;
  }

  if ((difference & GLS_MIRROR_VIEW) != 0) {
    glFrontFace((newState & GLS_MIRROR_VIEW) != 0 ? GL_CW_VALUE
                                                  : GL_CCW_VALUE);
  }
  if ((difference & 0x18000ULL) != 0) {
    if ((difference & 0x8000ULL) != 0) {
      if ((newState & 0x8000ULL) != 0) {
        glDisable(GL_CULL_FACE_VALUE);
      } else {
        glEnable(GL_CULL_FACE_VALUE);
      }
    }
    if ((difference & 0x10000ULL) != 0) {
      glCullFace((newState & 0x10000ULL) != 0 ? GL_FRONT_VALUE
                                              : GL_BACK_VALUE);
    }
  }
  if ((difference & 0x6000ULL) != 0) {
    unsigned int function = GL_LEQUAL_VALUE;
    switch (newState & 0x6000ULL) {
      case 0x2000ULL:
        function = GL_GEQUAL_VALUE;
        break;
      case 0x4000ULL:
        function = GL_ALWAYS_VALUE;
        break;
      case 0x6000ULL:
        function = GL_EQUAL_VALUE;
        break;
      default:
        break;
    }
    glDepthFunc(function);
  }
  if ((difference & 0x3FULL) != 0) {
    static constexpr unsigned int sourceFactors[8] = {
        GL_ONE_VALUE,
        GL_ZERO_VALUE,
        GL_DST_COLOR_VALUE,
        GL_ONE_MINUS_DST_COLOR_VALUE,
        GL_SRC_ALPHA_VALUE,
        GL_ONE_MINUS_SRC_ALPHA_VALUE,
        GL_DST_ALPHA_VALUE,
        GL_ONE_MINUS_DST_ALPHA_VALUE,
    };
    static constexpr unsigned int destinationFactors[8] = {
        GL_ZERO_VALUE,
        GL_ONE_VALUE,
        GL_SRC_COLOR_VALUE,
        GL_ONE_MINUS_SRC_COLOR_VALUE,
        GL_SRC_ALPHA_VALUE,
        GL_ONE_MINUS_SRC_ALPHA_VALUE,
        GL_DST_ALPHA_VALUE,
        GL_ONE_MINUS_DST_ALPHA_VALUE,
    };
    const unsigned int source = sourceFactors[newState & 7ULL];
    const unsigned int destination =
        destinationFactors[(newState >> 3ULL) & 7ULL];
    if (source == GL_ONE_VALUE && destination == GL_ZERO_VALUE) {
      glDisable(GL_BLEND_VALUE);
    } else {
      glEnable(GL_BLEND_VALUE);
      glBlendFunc(source, destination);
    }
  }
  if ((difference & 0xC0000ULL) != 0) {
    static constexpr unsigned int equations[4] = {
        GL_FUNC_ADD_VALUE, GL_FUNC_REVERSE_SUBTRACT_VALUE,
        GL_MIN_VALUE, GL_MAX_VALUE};
    _glewBlendEquation(equations[(newState >> 18ULL) & 3ULL]);
  }
  if ((difference & 0x40ULL) != 0) {
    glDepthMask((newState & 0x40ULL) == 0);
  }
  if ((difference & 0x780ULL) != 0) {
    glColorMask((newState & 0x80ULL) == 0,
                (newState & 0x100ULL) == 0,
                (newState & 0x200ULL) == 0,
                (newState & 0x400ULL) == 0);
  }
  if ((difference & 0x800ULL) != 0) {
    glPolygonMode(GL_FRONT_AND_BACK_VALUE,
                  (newState & 0x800ULL) != 0 ? GL_LINE_VALUE
                                             : GL_FILL_VALUE);
  }
  if ((difference & 0x20000ULL) != 0) {
    if ((newState & 0x20000ULL) != 0) {
      glEnable(GL_SAMPLE_ALPHA_TO_COVERAGE_VALUE);
    } else {
      glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE_VALUE);
    }
  }
  if ((difference & 0x1000ULL) != 0) {
    if ((newState & 0x1000ULL) != 0) {
      float scale = renderStateGL.polyOfsScale;
      float bias = renderStateGL.polyOfsBias;
      bool fill = renderStateGL.polyOfsFill;
      if (scale == 0.0f && bias == 0.0f) {
        scale = wrapperConfig.defaultPolyOfsFactor;
        bias = wrapperConfig.defaultPolyOfsUnits;
        fill = true;
      }
      if (fill) {
        glEnable(GL_POLYGON_OFFSET_FILL_VALUE);
        glDisable(GL_POLYGON_OFFSET_LINE_VALUE);
      } else {
        glDisable(GL_POLYGON_OFFSET_FILL_VALUE);
        glEnable(GL_POLYGON_OFFSET_LINE_VALUE);
      }
      glPolygonOffset(scale, bias);
    } else {
      glDisable(GL_POLYGON_OFFSET_FILL_VALUE);
      glDisable(GL_POLYGON_OFFSET_LINE_VALUE);
    }
  }
  if ((difference & 0xFFF000000000ULL) != 0) {
    if ((newState & 0xFFF000000000ULL) != 0) {
      glEnable(GL_STENCIL_TEST_VALUE);
    } else {
      glDisable(GL_STENCIL_TEST_VALUE);
    }
  }
  if ((difference & 0x7FFFF00000ULL) != 0) {
    static constexpr unsigned int functions[8] = {
        GL_ALWAYS_VALUE,  GL_LESS_VALUE,   GL_LEQUAL_VALUE,
        GL_GREATER_VALUE, GL_GEQUAL_VALUE, GL_EQUAL_VALUE,
        GL_NOTEQUAL_VALUE, GL_NEVER_VALUE};
    glStencilFunc(functions[(newState >> 36ULL) & 7ULL],
                  static_cast<unsigned char>(newState >> 20ULL),
                  static_cast<unsigned char>(newState >> 28ULL));
  }
  if ((difference & 0xFF8000000000ULL) != 0) {
    glStencilOp(DecodeStencilOp(newState >> 39ULL),
                DecodeStencilOp(newState >> 42ULL),
                DecodeStencilOp(newState >> 45ULL));
  }
  renderStateGL.currentState = newState;
}

void GL_Scissor(int x, int y, int width, int height) {
  glScissor(x, y, width, height);
}

void GL_Viewport(int x, int y, int width, int height) {
  glViewport(x, y, width, height);
  renderStateGL.defaultViewPort = false;
}

void GL_Clear(bool color, bool depth, bool stencil,
              unsigned char stencilValue, float red, float green, float blue,
              float alpha) {
  unsigned int mask = 0;
  if (color) {
    glClearColor(red, green, blue, alpha);
    mask = GL_COLOR_BUFFER_BIT_VALUE;
  }
  if (depth) {
    mask |= GL_DEPTH_BUFFER_BIT_VALUE;
  }
  if (stencil) {
    glClearStencil(stencilValue);
    mask |= GL_STENCIL_BUFFER_BIT_VALUE;
  }
  glClear(mask);
}

void GL_PolygonOffset(float scale, float bias, bool fill) {
  renderStateGL.polyOfsScale = scale;
  renderStateGL.polyOfsBias = bias;
  renderStateGL.polyOfsFill = fill;
}

void GL_DepthBoundsTest(float minimum, float maximum) {
  if (wrapperContext.disableDepthBoundsTest || minimum > maximum) {
    return;
  }
  if (minimum == 0.0f && maximum == 0.0f) {
    glDisable(GL_DEPTH_BOUNDS_TEST_EXT_VALUE);
  } else {
    glEnable(GL_DEPTH_BOUNDS_TEST_EXT_VALUE);
    _glewDepthBoundsEXT(minimum, maximum);
  }
}

std::int64_t GL_GetCurrentQueryNumber() {
  return renderStateGL.currentOcclusionQuery |
         (renderStateGL.currentOcclusionQueryBatch << 8);
}

void GL_CacheOcclusionQueryBatches(std::int64_t upToIncludingBatchNum) {
  renderStateGL.lastCachedOcclusionBatch =
      std::max(renderStateGL.lastCachedOcclusionBatch,
               upToIncludingBatchNum - 16);
  while (renderStateGL.lastCachedOcclusionBatch < upToIncludingBatchNum) {
    const std::int64_t batch = ++renderStateGL.lastCachedOcclusionBatch;
    const int slot = static_cast<int>(batch) & 15;
    const unsigned int count = renderStateGL.totalQueriesInBatch[slot];
    bool resultsAvailable = wrapperConfig.waitForOcclusionQuery;
    if (!resultsAvailable) {
      unsigned int available[256]{};
      GetMultiQueryResults(wrapperContext.queries[slot],
                           static_cast<int>(count),
                           GL_QUERY_RESULT_AVAILABLE_VALUE, available);
      resultsAvailable = true;
      for (unsigned int index = 0; index < count; ++index) {
        resultsAvailable &= available[index] != 0;
      }
    }
    if (resultsAvailable) {
      GetMultiQueryResults(wrapperContext.queries[slot],
                           static_cast<int>(count), GL_QUERY_RESULT_VALUE,
                           renderStateGL.cachedOcclusionResult[slot]);
    } else {
      // The retail instruction sequence multiplies the count by 1024 here.
      std::memset(renderStateGL.cachedOcclusionResult[slot], 0,
                  static_cast<std::size_t>(count) << 10U);
    }
  }
}

void GL_BeginQueryBatch() {
  if (glGetMultiQueryObjectuivAMD != nullptr) {
    renderStateGL.currentOcclusionQuery = 0;
    ++renderStateGL.currentOcclusionQueryBatch;
    renderStateGL.totalQueriesInBatch
        [renderStateGL.currentOcclusionQueryBatch & 15] = 0;
  }
}

void GL_BeginQuery(std::int64_t* queryNumber) {
  if (glGetMultiQueryObjectuivAMD != nullptr) {
    if (renderStateGL.currentOcclusionQuery >= 256) {
      renderStateGL.currentOcclusionQuery = 0;
      ++renderStateGL.currentOcclusionQueryBatch;
      renderStateGL.totalQueriesInBatch
          [renderStateGL.currentOcclusionQueryBatch & 15] = 0;
    }
    _glewBeginQuery(
        GL_SAMPLES_PASSED_VALUE,
        wrapperContext.queries[renderStateGL.currentOcclusionQueryBatch & 15]
                              [renderStateGL.currentOcclusionQuery]);
    *queryNumber = GL_GetCurrentQueryNumber();
  } else {
    *queryNumber = renderStateGL.currentOcclusionQuery;
    const unsigned int masked =
        static_cast<unsigned int>(renderStateGL.currentOcclusionQuery) &
        0xFFFU;
    _glewBeginQuery(GL_SAMPLES_PASSED_VALUE,
                    wrapperContext.queries[masked >> 4U]
                                          [static_cast<unsigned char>(masked)]);
  }
  ++wrapperStats.c_queriesIssued;
}

void GL_EndQuery() {
  _glewEndQuery(GL_SAMPLES_PASSED_VALUE);
  ++renderStateGL.currentOcclusionQuery;
  renderStateGL.totalQueriesInBatch
      [renderStateGL.currentOcclusionQueryBatch & 15] =
      static_cast<unsigned int>(renderStateGL.currentOcclusionQuery);
}

int GL_GetDeferredQueryResult(std::int64_t queryNumber) {
  if (queryNumber == 0) {
    return 1;
  }
  unsigned int result = 1;
  if (glGetMultiQueryObjectuivAMD == nullptr) {
    if (queryNumber < renderStateGL.currentOcclusionQuery - 4096) {
      ++wrapperStats.c_queriesTooOld;
      return OCCLUSION_QUERY_TOO_OLD;
    }
    const unsigned int masked = static_cast<unsigned int>(queryNumber) & 0xFFFU;
    const unsigned int query =
        wrapperContext.queries[masked >> 4U]
                              [static_cast<unsigned char>(masked)];
    if (wrapperConfig.waitForOcclusionQuery) {
      _glewGetQueryObjectuivARB(query, GL_QUERY_RESULT_AVAILABLE_VALUE,
                                &result);
      while (result == 0) {
        ++wrapperStats.c_queriesWaitTime;
        _glewGetQueryObjectuivARB(query, GL_QUERY_RESULT_AVAILABLE_VALUE,
                                  &result);
      }
    }
    _glewGetQueryObjectuivARB(query, GL_QUERY_RESULT_VALUE, &result);
  } else {
    const std::int64_t batch =
        (queryNumber >> 8) & 0x00FFFFFFFFFFFFFFLL;
    if (renderStateGL.currentOcclusionQueryBatch - batch >= 16) {
      ++wrapperStats.c_queriesTooOld;
      return OCCLUSION_QUERY_TOO_OLD;
    }
    GL_CacheOcclusionQueryBatches(batch);
    result = renderStateGL.cachedOcclusionResult[batch & 15]
                                                  [static_cast<unsigned char>(
                                                      queryNumber)];
  }
  wrapperStats.c_queriesPassed += result != 0;
  return static_cast<int>(result);
}

void GL_BindTexture(int textureUnit, const idImage* image,
                    const idTextureSampler* sampler) {
  idImage* mutableImage = const_cast<idImage*>(image);
  ++mutableImage->bindCount;
  mutableImage->frameUsed = renderStateGL.frameNumber;
  if (sampler == nullptr) {
    sampler = image->textureSampler;
  }
  _glewBindSampler(static_cast<unsigned int>(textureUnit),
                   sampler->oglSamplerState);
  _glewBindMultiTextureEXT(
      static_cast<unsigned int>(textureUnit) + 0x84C0U, image->bindTarget,
      image->oglTexture);
  ++wrapperStats.c_texturesBound;
}

void GL_ResetProgramState() {
  renderStateGL.boundVertexProgramChecksum = ~std::uint64_t{0};
  renderStateGL.boundFragmentProgramChecksum = ~std::uint64_t{0};
  renderStateGL.boundTextureUnits = 16;
  renderStateGL.boundProgram = nullptr;
  renderStateGL.boundVertexParmVersion = 0;
  renderStateGL.boundFragmentParmVersion = 0;
  renderStateGL.boundTextureParmVersion = 0;
  renderStateGL.boundSamplerParmVersion = 0;
  for (unsigned int unit = 0; unit < 16; ++unit) {
    if (renderStateGL.boundTextures[unit] != nullptr ||
        renderStateGL.boundSamplers[unit] != nullptr) {
      renderStateGL.boundTextures[unit] = nullptr;
      renderStateGL.boundSamplers[unit] = nullptr;
      _glewBindMultiTextureEXT(0x84C0U + unit, GL_TEXTURE_2D_VALUE, 0);
    }
  }
  renderStateGL.boundVertexBuffer = nullptr;
  renderStateGL.boundIndexBuffer = nullptr;
  renderStateGL.boundVAO = nullptr;
  renderStateGL.boundTexturesChanged = true;
  renderStateGL.boundVertexAttribArrays = ~0U;
  renderStateGL.boundVertexAttribPointers = ~0U;
}

void GL_ResetTextureState() { renderStateGL.boundTexturesChanged = true; }

wrapperStats_t GL_GetCurrentStats() { return wrapperStats; }

void GL_ClearStats() { std::memset(&wrapperStats, 0, sizeof(wrapperStats)); }

void idParmState::Set16BitVertexScaleBias(
    const idVertexScaleAndBias& scaleAndBias) {
  if (((onParmBlockStack[parmIndexVertexStScaleBias] |
        onParmBlockStack[parmIndexVertexXYZBias] |
        onParmBlockStack[parmIndexVertexXYZScale]) &
       parmStack.virtualParmBlockStackMask) != 0) {
    RollbackVirtualStack(true);
  }
  parmValue_t& st = values[parmIndexVertexStScaleBias];
  st.value[0] = scaleAndBias.stScale.x;
  st.value[1] = scaleAndBias.stScale.y;
  st.value[2] = scaleAndBias.stBias.x;
  st.value[3] = scaleAndBias.stBias.y;
  parmValue_t& xyzScale = values[parmIndexVertexXYZScale];
  xyzScale.value[0] = scaleAndBias.xyzScale.x;
  xyzScale.value[1] = scaleAndBias.xyzScale.y;
  xyzScale.value[2] = scaleAndBias.xyzScale.z;
  xyzScale.value[3] = 0.0f;
  parmValue_t& xyzBias = values[parmIndexVertexXYZBias];
  xyzBias.value[0] = scaleAndBias.xyzBias.x;
  xyzBias.value[1] = scaleAndBias.xyzBias.y;
  xyzBias.value[2] = scaleAndBias.xyzBias.z;
  xyzBias.value[3] = 1.0f;
  const unsigned char usage =
      parmUsageMask[parmIndexVertexXYZScale] |
      parmUsageMask[parmIndexVertexStScaleBias] |
      parmUsageMask[parmIndexVertexXYZBias];
  ++renderParmVersion;
  vertexParmVersion += usage & 1U;
  fragmentParmVersion += usage & 2U;
  textureParmVersion += usage & 4U;
  samplerParmVersion += usage & 8U;
}

void GL_SetDefaultState() {
  glClearDepth(1.0);
  glEnable(GL_DEPTH_TEST_VALUE);
  glEnable(GL_BLEND_VALUE);
  glEnable(GL_SCISSOR_TEST_VALUE);
  glEnable(GL_CULL_FACE_VALUE);
  glEnable(GL_SAMPLE_COVERAGE_VALUE);
  glEnable(GL_SAMPLE_MASK_VALUE);
  glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS_VALUE);
  glDisable(GL_STENCIL_TEST_VALUE);
  glPolygonMode(GL_FRONT_AND_BACK_VALUE, GL_FILL_VALUE);
  glDepthMask(1);
  glDepthFunc(GL_ALWAYS_VALUE);
  glShadeModel(GL_SMOOTH_VALUE);
  glDisable(GL_POLYGON_OFFSET_FILL_VALUE);
  glDisable(GL_POLYGON_OFFSET_LINE_VALUE);
  glPolygonOffset(0.0f, 0.0f);
  glFrontFace(GL_CW_VALUE);
  glBindTexture(GL_TEXTURE_2D_VALUE, 0);
  _glewClampColorARB(GL_CLAMP_READ_COLOR_ARB_VALUE, 0);
  renderStateGL.polyOfsScale = 0.0f;
  renderStateGL.polyOfsBias = 0.0f;
  renderStateGL.polyOfsFill = true;
  renderStateGL.currentState = 0;
  GL_State(0x4000U, true);
  GL_ResetProgramState();
  _glewSampleMaski(0, ~0U);
  if (_glewPrimitiveRestartIndex != nullptr) {
    glEnable(GL_PRIMITIVE_RESTART_VALUE);
    _glewPrimitiveRestartIndex(0xFFFFU);
  }
  glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
  glDisable(GL_LIGHTING_VALUE);
  glAlphaFunc(GL_GREATER_VALUE, 0.40000001f);
  glEnable(GL_TEXTURE_2D_VALUE);
  _glewClampColorARB(GL_CLAMP_FRAGMENT_COLOR_ARB_VALUE, 0);
}

void GL_SetRenderDestination(
    const idODSObject<idRenderDestination>& destination, int level, int side) {
  if (destination.objectPtr == nullptr) {
    _glewBindFramebufferEXT(GL_FRAMEBUFFER_VALUE, 0);
    renderStateGL.currentNumTargets = 1;
    return;
  }

  idRenderDestination* mutableDestination =
      const_cast<idRenderDestination*>(destination.objectPtr);
  _glewBindFramebufferEXT(GL_FRAMEBUFFER_VALUE,
                          destination.objectPtr->FBOhandle);
  if ((destination.objectPtr->targetImage[0] != nullptr ||
       destination.objectPtr->depthImage != nullptr ||
       destination.objectPtr->stencilImage != nullptr) &&
      (destination.objectPtr->boundLevel != level ||
       destination.objectPtr->boundSide != side)) {
    for (int targetIndex = 0; targetIndex < 4; ++targetIndex) {
      const idImage* image = destination.objectPtr->targetImage[targetIndex];
      if (image == nullptr) {
        continue;
      }
      unsigned int target = GL_TEXTURE_2D_VALUE;
      if (image->opts.textureType == TT_CUBIC) {
        target = GL_TEXTURE_CUBE_MAP_POSITIVE_X_VALUE + side;
      } else if (image->opts.numSamples > 1) {
        target = GL_TEXTURE_2D_MULTISAMPLE_VALUE;
      }
      _glewFramebufferTexture2DEXT(GL_FRAMEBUFFER_VALUE,
                                   glRenderTarget[targetIndex], target,
                                   image->oglTexture, level);
    }
    if (destination.objectPtr->depthImage != nullptr) {
      const idImage* image = destination.objectPtr->depthImage;
      const unsigned int target = image->opts.numSamples > 1
                                      ? GL_TEXTURE_2D_MULTISAMPLE_VALUE
                                      : GL_TEXTURE_2D_VALUE;
      _glewFramebufferTexture2DEXT(GL_FRAMEBUFFER_VALUE,
                                   GL_DEPTH_ATTACHMENT_VALUE, target,
                                   image->oglTexture, level);
    }
    if (destination.objectPtr->stencilImage != nullptr) {
      const idImage* image = destination.objectPtr->stencilImage;
      const unsigned int target = image->opts.numSamples > 1
                                      ? GL_TEXTURE_2D_MULTISAMPLE_VALUE
                                      : GL_TEXTURE_2D_VALUE;
      _glewFramebufferTexture2DEXT(GL_FRAMEBUFFER_VALUE,
                                   GL_STENCIL_ATTACHMENT_VALUE, target,
                                   image->oglTexture, level);
    }
    mutableDestination->boundSide = side;
    mutableDestination->boundLevel = level;
  }
  renderStateGL.currentNumTargets = 0;
  for (int targetIndex = 0; targetIndex < 4; ++targetIndex) {
    if (destination.objectPtr->targetImage[targetIndex] != nullptr) {
      renderStateGL.currentNumTargets = targetIndex + 1;
    }
  }
  renderStateGL.defaultViewPort = true;
}

void GL_BindProgram(const idODSObject<idDeclRenderProg>& program,
                    std::uint64_t extraState, unsigned int) {
  const idDeclRenderProg* prog = program.objectPtr;
  if (prog->vertexProgram == nullptr || prog->fragmentProgram == nullptr) {
    return;
  }

  renderThreadParmState->RollbackVirtualStack(false);
  if (renderThreadParmState->currentMVPHasNegativeDeterminant) {
    extraState |= GLS_MIRROR_VIEW;
  }
  const std::uint64_t inheritedStateMask =
      (extraState & (std::uint64_t{1} << 63U)) == 0
          ? ~std::uint64_t{0}
          : std::uint64_t{0};
  GL_State((extraState & 0x7FFFFFFFFFFFFFFFULL) |
               (prog->data.glState & inheritedStateMask),
           false);

  const bool programChanged =
      prog->vertexProgramChecksum !=
          renderStateGL.boundVertexProgramChecksum ||
      prog->fragmentProgramChecksum !=
          renderStateGL.boundFragmentProgramChecksum;
  const bool bindProgram = programChanged || !wrapperConfig.lazyBindPrograms;
  if (bindProgram) {
    _glewUseProgram(GlName(prog->vertexProgram));
    renderLog.OpenBlock("Binding Program %s:\n", prog->GetName());
  }

  float blockParms[168];
  const bool bindVertexParms =
      bindProgram || !wrapperConfig.lazyBindParms ||
      renderStateGL.boundVertexParmVersion !=
          renderThreadParmState->vertexParmVersion;
  if (bindVertexParms) {
    if (wrapperContext.useUniformArrays) {
      for (int index = 0; index < prog->data.vertexParms.num; ++index) {
        std::memcpy(&blockParms[index * 4],
                    renderThreadParmState
                        ->values[prog->data.vertexParms.list[index]]
                        .value,
                    sizeof(float) * 4);
      }
      if (prog->data.vertexParms.num > 0) {
        _glewUniform4fv(UniformLocation(prog->vertexUniforms.ptr[0]),
                        prog->data.vertexParms.num, blockParms);
      }
    } else {
      for (int index = 0; index < prog->data.vertexParms.num; ++index) {
        _glewUniform4fv(
            UniformLocation(prog->vertexUniforms.ptr[index]), 1,
            renderThreadParmState
                ->values[prog->data.vertexParms.list[index]]
                .value);
      }
    }
  }

  const bool bindFragmentParms =
      bindProgram || !wrapperConfig.lazyBindParms ||
      renderStateGL.boundFragmentParmVersion !=
          renderThreadParmState->fragmentParmVersion;
  if (bindFragmentParms) {
    if (wrapperContext.useUniformArrays) {
      for (int index = 0; index < prog->data.fragmentParms.num; ++index) {
        std::memcpy(&blockParms[index * 4],
                    renderThreadParmState
                        ->values[prog->data.fragmentParms.list[index]]
                        .value,
                    sizeof(float) * 4);
      }
      if (prog->data.fragmentParms.num > 0) {
        _glewUniform4fv(UniformLocation(prog->fragmentUniforms.ptr[0]),
                        prog->data.fragmentParms.num, blockParms);
      }
    } else {
      for (int index = 0; index < prog->data.fragmentParms.num; ++index) {
        _glewUniform4fv(
            UniformLocation(prog->fragmentUniforms.ptr[index]), 1,
            renderThreadParmState
                ->values[prog->data.fragmentParms.list[index]]
                .value);
      }
    }
  }

  const bool bindTextures =
      bindProgram || !wrapperConfig.lazyBindTextures ||
      renderStateGL.boundTextureParmVersion !=
          renderThreadParmState->textureParmVersion ||
      renderStateGL.boundSamplerParmVersion !=
          renderThreadParmState->samplerParmVersion ||
      renderStateGL.boundTexturesChanged;
  if (bindTextures) {
    const int textureCount = prog->data.textureParms.num;
    for (int unit = 0; unit < textureCount; ++unit) {
      const idImage* desiredImage =
          renderThreadParmState
              ->values[prog->data.textureParms.list[unit]]
              .image;
      const idTextureSampler* desiredSampler =
          renderThreadParmState
              ->values[prog->data.samplerParms.list[unit]]
              .sampler;
      if ((((renderStateGL.boundTextures[unit] != desiredImage) ||
            (renderStateGL.boundSamplers[unit] != desiredSampler)) &&
           renderStateGL.boundTextures[unit] != nullptr) ||
          renderStateGL.boundTexturesChanged) {
        _glewBindMultiTextureEXT(0x84C0U + unit, GL_TEXTURE_2D_VALUE, 0);
        _glewBindSampler(unit, 0);
      }
    }
    for (int unit = textureCount; unit < 16; ++unit) {
      if (renderStateGL.boundTextures[unit] != nullptr) {
        renderStateGL.boundTextures[unit] = nullptr;
        _glewBindMultiTextureEXT(0x84C0U + unit, GL_TEXTURE_2D_VALUE, 0);
        _glewBindSampler(unit, 0);
      }
    }
    for (int unit = 0; unit < textureCount; ++unit) {
      const idImage* image =
          renderThreadParmState
              ->values[prog->data.textureParms.list[unit]]
              .image;
      const idTextureSampler* sampler =
          renderThreadParmState
              ->values[prog->data.samplerParms.list[unit]]
              .sampler;
      if ((renderStateGL.boundTextures[unit] != image ||
           renderStateGL.boundSamplers[unit] != sampler ||
           renderStateGL.boundTexturesChanged) &&
          image != nullptr) {
        GL_BindTexture(unit, image,
                       sampler != nullptr ? sampler : image->textureSampler);
        renderStateGL.boundTextures[unit] = image;
        renderStateGL.boundSamplers[unit] = sampler;
      }
    }
  }

  renderStateGL.boundProgram = prog;
  renderStateGL.boundVertexProgramChecksum = prog->vertexProgramChecksum;
  renderStateGL.boundFragmentProgramChecksum = prog->fragmentProgramChecksum;
  renderStateGL.boundVertexParmVersion =
      renderThreadParmState->vertexParmVersion;
  renderStateGL.boundFragmentParmVersion =
      renderThreadParmState->fragmentParmVersion;
  renderStateGL.boundTextureParmVersion =
      renderThreadParmState->textureParmVersion;
  renderStateGL.boundSamplerParmVersion =
      renderThreadParmState->samplerParmVersion;
  ++wrapperStats.c_programsBound;
  renderStateGL.boundTexturesChanged = false;
  renderStateGL.boundTextureUnits = prog->data.textureParms.num;
}

void GL_ClearSurfaceParms(const idDeclRenderProg* program,
                          const idRenderModelSurface*) {
  const int numVertexParms = program->data.vertexParms.num;
  if (program->vertexUniforms.ptr[numVertexParms] != nullptr) {
    _glewUniform4fv(
        UniformLocation(program->vertexUniforms.ptr[numVertexParms + 1]), 1,
        &vec4Origin.x);
  }
  const unsigned int hardwareBits =
      FloatBits(renderStateGL.hardwareSkinning.x) |
      FloatBits(renderStateGL.hardwareSkinning.y) |
      FloatBits(renderStateGL.hardwareSkinning.z) |
      FloatBits(renderStateGL.hardwareSkinning.w);
  if ((hardwareBits & 0x7FFFFFFFU) != 0) {
    renderStateGL.hardwareSkinning = vec4Origin;
    _glewUniform4fv(
        UniformLocation(program->vertexUniforms.ptr[numVertexParms + 1]), 1,
        &renderStateGL.hardwareSkinning.x);
  }
}

template <typename T>
T* idListArrayResize(T* oldPointer, int oldNum, int newNum, unsigned int tag,
                     bool zeroBuffer);

template <>
vaoInfo_t* idListArrayResize<vaoInfo_t>(vaoInfo_t* oldPointer, int oldNum,
                                        int newNum, unsigned int tag,
                                        bool zeroBuffer) {
  if (newNum <= 0) {
    idMem::Singleton()->Free(oldPointer, ALIGN_16);
    return nullptr;
  }
  vaoInfo_t* result = static_cast<vaoInfo_t*>(
      idMem::Singleton()->AllocWithLocation(
          zeroBuffer ? "w:\\build\\tech5\\shared\\idlib\\containers/List.h(27) : tag"
                     : "w:\\build\\tech5\\shared\\idlib\\containers/List.h(29) : tag",
          sizeof(vaoInfo_t) * static_cast<std::size_t>(newNum),
          static_cast<memTag_t>(tag), zeroBuffer, ALIGN_16,
          HEAP_DEFAULTHEAP));
  if (result != nullptr) {
    const int copyCount = std::min(oldNum, newNum);
    if (copyCount > 0) {
      std::memcpy(result, oldPointer,
                  sizeof(vaoInfo_t) * static_cast<std::size_t>(copyCount));
    }
    idMem::Singleton()->Free(oldPointer, ALIGN_16);
  }
  return result;
}

void* idVaoContainer::FindVaoInfo(const void* vb, const void* ib,
                                  const void* mb, const void* stb,
                                  unsigned int vertexMask) const {
  for (int index = vaoHash.First(VaoHashKey(vb, ib, mb, stb, vertexMask));
       index != -1; index = vaoHash.Next(index)) {
    const vaoInfo_t& info = vaoInfo[index];
    if (info.vb == vb && info.ib == ib && info.mb == mb && info.stb == stb &&
        info.vertexMask == vertexMask) {
      return const_cast<void*>(info.vao);
    }
  }
  return nullptr;
}

idVaoContainer::idVaoContainer()
    : vaoHash(4096, 0x4000),
      vbVaoHash(4096, 0x4000),
      ibVaoHash(4096, 0x4000),
      mbVaoHash(4096, 0x4000),
      stbVaoHash(4096, 0x4000) {
  vaoHash.memTag = TAG_HASHINDEX;
  vbVaoHash.memTag = TAG_HASHINDEX;
  ibVaoHash.memTag = TAG_HASHINDEX;
  mbVaoHash.memTag = TAG_HASHINDEX;
  stbVaoHash.memTag = TAG_HASHINDEX;
  idMem::Singleton()->PushHeap(HEAP_SYSTEMHEAP);
  vaoInfo.granularity = 0x2000;
  freeList.granularity = 0x2000;
  idMem::Singleton()->PopHeap();
}

idVaoContainer::~idVaoContainer() = default;

void idVaoContainer::AddVaoInfo(const void* vao, const void* vb,
                                const void* ib, const void* mb,
                                const void* stb, unsigned int vertexMask) {
  idMem::Singleton()->PushHeap(HEAP_SYSTEMHEAP);
  int index;
  if (freeList.num != 0) {
    index = static_cast<int>(freeList.list[freeList.num - 1]);
    --freeList.num;
  } else {
    index = vaoInfo.num;
    vaoInfo.Alloc();
  }
  vaoInfo_t& info = vaoInfo[index];
  info.vao = vao;
  info.vb = vb;
  info.ib = ib;
  info.mb = mb;
  info.stb = stb;
  info.vertexMask = vertexMask;
  vaoHash.Add(VaoHashKey(vb, ib, mb, stb, vertexMask), index);
  if (vb != nullptr) {
    vbVaoHash.Add(static_cast<int>(PointerKey(vb)), index);
  }
  if (ib != nullptr) {
    ibVaoHash.Add(static_cast<int>(PointerKey(ib)), index);
  }
  if (mb != nullptr) {
    mbVaoHash.Add(static_cast<int>(PointerKey(mb)), index);
  }
  if (stb != nullptr) {
    stbVaoHash.Add(static_cast<int>(PointerKey(stb)), index);
  }
  idMem::Singleton()->PopHeap();
}

void idVaoContainer::RemoveVaoInfo(int index) {
  idMem::Singleton()->PushHeap(HEAP_SYSTEMHEAP);
  vaoInfo_t& info = vaoInfo[index];
  const unsigned int vao = GlName(info.vao);
  _glewDeleteVertexArrays(1, &vao);
  if (info.vb != nullptr) {
    vbVaoHash.Remove(static_cast<int>(PointerKey(info.vb)), index);
  }
  if (info.ib != nullptr) {
    ibVaoHash.Remove(static_cast<int>(PointerKey(info.ib)), index);
  }
  if (info.mb != nullptr) {
    mbVaoHash.Remove(static_cast<int>(PointerKey(info.mb)), index);
  }
  if (info.stb != nullptr) {
    stbVaoHash.Remove(static_cast<int>(PointerKey(info.stb)), index);
  }
  vaoHash.Remove(
      VaoHashKey(info.vb, info.ib, info.mb, info.stb, info.vertexMask), index);
  freeList.Append(static_cast<unsigned int>(index));
  idMem::Singleton()->PopHeap();
}

void idVaoContainer::RemoveVertexBuffer(const void* apiObject) {
  const int key = static_cast<int>(PointerKey(apiObject));
  for (int index = vbVaoHash.First(key); index != -1;) {
    if (vaoInfo[index].vb == apiObject) {
      RemoveVaoInfo(index);
      index = vbVaoHash.First(key);
    } else {
      index = vbVaoHash.Next(index);
    }
  }
  for (int index = mbVaoHash.First(key); index != -1;) {
    if (vaoInfo[index].mb == apiObject) {
      RemoveVaoInfo(index);
      index = mbVaoHash.First(key);
    } else {
      index = mbVaoHash.Next(index);
    }
  }
  for (int index = stbVaoHash.First(key); index != -1;) {
    if (vaoInfo[index].stb == apiObject) {
      RemoveVaoInfo(index);
      index = stbVaoHash.First(key);
    } else {
      index = stbVaoHash.Next(index);
    }
  }
}

void GL_DeleteVertexBufferVAO(const void* apiObject) {
  vaoContainer.RemoveVertexBuffer(apiObject);
}

void GL_DeleteIndexBufferVAO(const void* apiObject) {
  const int key = static_cast<int>(PointerKey(apiObject));
  for (int index = vaoContainer.ibVaoHash.First(key); index != -1;) {
    if (vaoContainer.vaoInfo[index].ib == apiObject) {
      vaoContainer.RemoveVaoInfo(index);
      index = vaoContainer.ibVaoHash.First(key);
    } else {
      index = vaoContainer.ibVaoHash.Next(index);
    }
  }
}

void* GL_BuildVAO(const idVertexBuffer& vertexBuffer,
                  const idIndexBuffer& indexBuffer,
                  const idVertexBuffer* morphMap,
                  const idVertexBuffer* stMap, int vertexMaskValue) {
  unsigned int vao = 0;
  renderLog.OpenBlock("Setting up VAO:\n");
  _glewGenVertexArrays(1, &vao);
  _glewBindVertexArray(vao);
  _glewBindBufferARB(GL_ARRAY_BUFFER_VALUE, GlName(vertexBuffer.apiObject));

  const unsigned int vertexMask =
      static_cast<unsigned int>(vertexMaskValue);
  const int stride = VertexStride(vertexMask);
  std::uintptr_t offset = 0;
  if ((vertexMask & 1U) != 0) {
    if ((vertexMask & 0x20U) != 0) {
      _glewVertexAttribPointerARB(0, 3, GL_SHORT_VALUE, 0, stride, nullptr);
      offset = 8;
    } else {
      _glewVertexAttribPointerARB(0, 3, GL_FLOAT_VALUE, 0, stride, nullptr);
      offset = 12;
    }
    _glewEnableVertexAttribArrayARB(0);
  } else {
    _glewDisableVertexAttribArrayARB(0);
  }
  if ((vertexMask & 2U) != 0) {
    if ((vertexMask & 0x40U) != 0) {
      _glewVertexAttribPointerARB(1, 2, GL_SHORT_VALUE, 0, stride,
                                  reinterpret_cast<const void*>(offset));
      offset += 4;
    } else {
      _glewVertexAttribPointerARB(1, 2, GL_FLOAT_VALUE, 0, stride,
                                  reinterpret_cast<const void*>(offset));
      offset += 8;
    }
    _glewEnableVertexAttribArrayARB(1);
  } else {
    _glewDisableVertexAttribArrayARB(1);
  }
  if ((vertexMask & 0x2000U) != 0 || (vertexMask & 0x400U) == 0) {
    _glewDisableVertexAttribArrayARB(4);
  } else {
    if ((vertexMask & 0x800U) != 0) {
      _glewVertexAttribPointerARB(4, 2, GL_SHORT_VALUE, 0, stride,
                                  reinterpret_cast<const void*>(offset));
      offset += 4;
    } else {
      _glewVertexAttribPointerARB(4, 2, GL_FLOAT_VALUE, 0, stride,
                                  reinterpret_cast<const void*>(offset));
      offset += 8;
    }
    _glewEnableVertexAttribArrayARB(4);
  }
  if ((vertexMask & 4U) != 0) {
    _glewVertexAttribPointerARB(2, 4, GL_UNSIGNED_BYTE_VALUE, 1, stride,
                                reinterpret_cast<const void*>(offset));
    offset += 4;
    _glewEnableVertexAttribArrayARB(2);
  } else {
    _glewDisableVertexAttribArrayARB(2);
  }
  if ((vertexMask & 0x10U) != 0) {
    _glewVertexAttribPointerARB(9, 4, GL_UNSIGNED_BYTE_VALUE, 1, stride,
                                reinterpret_cast<const void*>(offset));
    offset += 4;
    _glewEnableVertexAttribArrayARB(9);
  } else {
    _glewDisableVertexAttribArrayARB(9);
  }
  if ((vertexMask & 8U) != 0) {
    _glewVertexAttribPointerARB(3, 4, GL_UNSIGNED_BYTE_VALUE, 1, stride,
                                reinterpret_cast<const void*>(offset));
    _glewEnableVertexAttribArrayARB(3);
  } else {
    _glewDisableVertexAttribArrayARB(3);
  }
  if (morphMap != nullptr) {
    _glewBindBufferARB(GL_ARRAY_BUFFER_VALUE, GlName(morphMap->apiObject));
    const std::uintptr_t morphOffset =
        static_cast<unsigned int>(morphMap->offsetInOtherBuffer) & 0x7FFFFFFFU;
    _glewVertexAttribPointerARB(
        10, 4, GL_UNSIGNED_BYTE_VALUE, 1, 4,
        reinterpret_cast<const void*>(morphOffset));
    _glewEnableVertexAttribArrayARB(10);
  } else {
    _glewDisableVertexAttribArrayARB(10);
  }
  if (stMap != nullptr) {
    _glewBindBufferARB(GL_ARRAY_BUFFER_VALUE, GlName(stMap->apiObject));
    const std::uintptr_t stOffset =
        static_cast<unsigned int>(stMap->offsetInOtherBuffer) & 0x7FFFFFFFU;
    _glewVertexAttribPointerARB(4, 2, GL_FLOAT_VALUE, 0, 8,
                                reinterpret_cast<const void*>(stOffset));
    _glewEnableVertexAttribArrayARB(4);
  }
  _glewBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_VALUE,
                     GlName(indexBuffer.apiObject));
  vaoContainer.AddVaoInfo(ApiObject(vao), vertexBuffer.apiObject,
                          indexBuffer.apiObject,
                          morphMap != nullptr ? morphMap->apiObject : nullptr,
                          stMap != nullptr ? stMap->apiObject : nullptr,
                          vertexMask);
  return ApiObject(vao);
}

void* GL_SetupSurfaceParms(const idDeclRenderProg* program,
                           const idRenderModelSurface* surface) {
  const int numVertexParms = program->data.vertexParms.num;
  if (program->vertexUniforms.ptr[numVertexParms] != nullptr) {
    _glewUniform4fv(
        UniformLocation(program->vertexUniforms.ptr[numVertexParms]), 1,
        &surface->skinOffsets.x);
  }

  bool hardwareJoints =
      program->vertexUniforms.ptr[numVertexParms + 2] != nullptr &&
      surface->joints != nullptr;
  bool morphing = (program->data.vertexMask & 0x80U) != 0 &&
                  surface->morphMap != nullptr;
  const bool secondaryPosition =
      (program->data.vertexMask & surface->geometry->vertexMask & 0x1000U) !=
      0;
  const bool dualQuaternion =
      hardwareJoints && (surface->joints->flags & 1) != 0;
  const bool separateTexcoords =
      (program->data.vertexMask & 0x400U) != 0 && surface->stMap != nullptr;

  if (program->vertexUniforms.ptr[numVertexParms + 1] == nullptr) {
    hardwareJoints = false;
    morphing = false;
  }
  if (!secondaryPosition && !dualQuaternion && !hardwareJoints && !morphing) {
    renderStateGL.hardwareSkinning = vec4Origin;
  } else {
    renderStateGL.hardwareSkinning.x = hardwareJoints ? 1.0f : 0.0f;
    renderStateGL.hardwareSkinning.y = surface->morphMultiplier;
    renderStateGL.hardwareSkinning.z = secondaryPosition ? 1.0f : 0.0f;
    renderStateGL.hardwareSkinning.w = dualQuaternion ? 1.0f : 0.0f;
    _glewUniform4fv(
        UniformLocation(program->vertexUniforms.ptr[numVertexParms + 1]), 1,
        &renderStateGL.hardwareSkinning.x);
    if (hardwareJoints) {
      const std::intptr_t offset =
          static_cast<unsigned int>(surface->joints->offsetInOtherBuffer) &
          0x7FFFFFFFU;
      _glewBindBufferRange(
          GL_UNIFORM_BUFFER_VALUE, 0, GlName(surface->joints->apiObject), offset,
          48LL * (surface->joints->numJoints & 0x7FFFFFFF));
      renderLog.OpenBlock("%i joint parms\n",
                          surface->joints->numJoints & 0x7FFFFFFF);
    }
  }

  if ((!wrapperConfig.useGLVAO || !morphing) && !separateTexcoords) {
    return nullptr;
  }
  void* vao = vaoContainer.FindVaoInfo(
      surface->geometry->vertexBuffer.apiObject,
      surface->geometry->indexBuffer.apiObject,
      surface->morphMap != nullptr ? surface->morphMap->apiObject : nullptr,
      surface->stMap != nullptr ? surface->stMap->apiObject : nullptr,
      surface->geometry->vertexMask);
  if (vao == nullptr) {
    vao = GL_BuildVAO(surface->geometry->vertexBuffer,
                      surface->geometry->indexBuffer, surface->morphMap,
                      surface->stMap,
                      static_cast<int>(surface->geometry->vertexMask));
  }
  return vao;
}

void GL_DrawElementsInternal(const idTriangles* triangles, void* vao,
                             bool skipDetailTriangles) {
  int numIndexes;
  if ((!wrapperConfig.skipDetailTris && !skipDetailTriangles) ||
      triangles->detailOffset == 0) {
    numIndexes = triangles->numIndexes;
  } else {
    numIndexes = triangles->detailOffset;
  }
  if (numIndexes < 3) {
    return;
  }
  if (wrapperConfig.singleTriangle) {
    numIndexes = 3;
  }

  const unsigned int vertexMask = triangles->vertexMask;
  const int stride = VertexStride(vertexMask);
  if (stride == 0) {
    return;
  }

  if (wrapperConfig.useGLVAO) {
    if (vao == nullptr) {
      vao = vaoContainer.FindVaoInfo(
          triangles->vertexBuffer.apiObject,
          triangles->indexBuffer.apiObject, nullptr, nullptr, vertexMask);
      if (vao == nullptr) {
        vao = GL_BuildVAO(triangles->vertexBuffer, triangles->indexBuffer,
                          nullptr, nullptr, static_cast<int>(vertexMask));
      }
    }
    if (renderStateGL.boundVAO != vao) {
      renderLog.OpenBlock("Binding VAO\n");
      _glewBindVertexArray(GlName(vao));
      renderStateGL.boundVAO = vao;
    }
  } else {
    if (renderStateGL.boundIndexBuffer != triangles->indexBuffer.apiObject) {
      renderLog.OpenBlock("Binding Index Buffer %d\n",
                          static_cast<int>(GlName(
                              triangles->indexBuffer.apiObject)));
      _glewBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_VALUE,
                         GlName(triangles->indexBuffer.apiObject));
    }
    if (renderStateGL.boundVertexBuffer != triangles->vertexBuffer.apiObject) {
      renderLog.OpenBlock("Binding Vertex Buffer %d\n",
                          static_cast<int>(GlName(
                              triangles->vertexBuffer.apiObject)));
      _glewBindBufferARB(GL_ARRAY_BUFFER_VALUE,
                         GlName(triangles->vertexBuffer.apiObject));
    }
    if (renderStateGL.boundVertexAttribArrays != (vertexMask & 0x141FU)) {
      renderLog.OpenBlock(
          "changing vertex attribute array state to (%d)\n",
          vertexMask & 0x141FU);
      ((vertexMask & 1U) != 0 ? _glewEnableVertexAttribArrayARB
                              : _glewDisableVertexAttribArrayARB)(0);
      ((vertexMask & 0x1000U) != 0 ? _glewEnableVertexAttribArrayARB
                                   : _glewDisableVertexAttribArrayARB)(5);
      ((vertexMask & 2U) != 0 ? _glewEnableVertexAttribArrayARB
                              : _glewDisableVertexAttribArrayARB)(1);
      if ((vertexMask & 0x2000U) == 0) {
        ((vertexMask & 0x400U) != 0 ? _glewEnableVertexAttribArrayARB
                                    : _glewDisableVertexAttribArrayARB)(4);
      }
      ((vertexMask & 4U) != 0 ? _glewEnableVertexAttribArrayARB
                              : _glewDisableVertexAttribArrayARB)(2);
      ((vertexMask & 8U) != 0 ? _glewEnableVertexAttribArrayARB
                              : _glewDisableVertexAttribArrayARB)(3);
      ((vertexMask & 0x10U) != 0 ? _glewEnableVertexAttribArrayARB
                                 : _glewDisableVertexAttribArrayARB)(9);
    }
    if (renderStateGL.boundVertexBuffer != triangles->vertexBuffer.apiObject ||
        renderStateGL.boundVertexAttribPointers != (vertexMask & 0x1C7FU)) {
      std::uintptr_t offset = 0;
      if ((vertexMask & 1U) != 0) {
        if ((vertexMask & 0x20U) != 0) {
          renderLog.OpenBlock(
              "vertex attribute: short xyz[3] (offset=%d, stride=%d)\n",
              static_cast<int>(offset), stride);
          _glewVertexAttribPointerARB(0, 3, GL_SHORT_VALUE, 0, stride,
                                      reinterpret_cast<void*>(offset));
          offset = 8;
        } else {
          renderLog.OpenBlock(
              "vertex attribute: float xyz[3] (offset=%d, stride=%d)\n",
              static_cast<int>(offset), stride);
          _glewVertexAttribPointerARB(0, 3, GL_FLOAT_VALUE, 0, stride,
                                      reinterpret_cast<void*>(offset));
          offset = 12;
        }
      }
      if ((vertexMask & 0x1000U) != 0) {
        renderLog.OpenBlock(
            "vertex attribute: float xyz1[3] (offset=%d, stride=%d)\n",
            static_cast<int>(offset), stride);
        _glewVertexAttribPointerARB(5, 3, GL_FLOAT_VALUE, 0, stride,
                                    reinterpret_cast<void*>(offset));
        offset += 12;
      }
      if ((vertexMask & 2U) != 0) {
        if ((vertexMask & 0x40U) != 0) {
          renderLog.OpenBlock(
              "vertex attribute: short st[2] (offset=%d, stride=%d)\n",
              static_cast<int>(offset), stride);
          _glewVertexAttribPointerARB(1, 2, GL_SHORT_VALUE, 0, stride,
                                      reinterpret_cast<void*>(offset));
          offset += 4;
        } else {
          renderLog.OpenBlock(
              "vertex attribute: float st[2] (offset=%d, stride=%d)\n",
              static_cast<int>(offset), stride);
          _glewVertexAttribPointerARB(1, 2, GL_FLOAT_VALUE, 0, stride,
                                      reinterpret_cast<void*>(offset));
          offset += 8;
        }
      }
      if ((vertexMask & 0x400U) != 0 && (vertexMask & 0x2000U) == 0) {
        if ((vertexMask & 0x800U) != 0) {
          renderLog.OpenBlock(
              "vertex attribute: short st1[2] (offset=%d, stride=%d)\n",
              static_cast<int>(offset), stride);
          _glewVertexAttribPointerARB(4, 2, GL_SHORT_VALUE, 0, stride,
                                      reinterpret_cast<void*>(offset));
          offset += 4;
        } else {
          const std::uintptr_t separateOffset =
              32U * static_cast<unsigned int>(triangles->numVerts / 2) + 12U;
          renderLog.OpenBlock(
              "vertex attribute: float st1[2] (offset=%d, stride=%d)\n",
              static_cast<int>(separateOffset), stride);
          _glewVertexAttribPointerARB(
              4, 2, GL_FLOAT_VALUE, 0, stride,
              reinterpret_cast<void*>(separateOffset));
        }
      }
      if ((vertexMask & 4U) != 0) {
        renderLog.OpenBlock(
            "vertex attribute: byte normal[4] (offset=%d, stride=%d)\n",
            static_cast<int>(offset), stride);
        _glewVertexAttribPointerARB(2, 4, GL_UNSIGNED_BYTE_VALUE, 1, stride,
                                    reinterpret_cast<void*>(offset));
        offset += 4;
      }
      if ((vertexMask & 0x10U) != 0) {
        renderLog.OpenBlock(
            "vertex attribute: byte tangent[4] (offset=%d, stride=%d)\n",
            static_cast<int>(offset), stride);
        _glewVertexAttribPointerARB(9, 4, GL_UNSIGNED_BYTE_VALUE, 1, stride,
                                    reinterpret_cast<void*>(offset));
        offset += 4;
      }
      if ((vertexMask & 8U) != 0) {
        renderLog.OpenBlock(
            "vertex attribute: byte color[4] (offset=%d, stride=%d)\n",
            static_cast<int>(offset), stride);
        _glewVertexAttribPointerARB(3, 4, GL_UNSIGNED_BYTE_VALUE, 1, stride,
                                    reinterpret_cast<void*>(offset));
      }
    }
    renderStateGL.boundIndexBuffer = triangles->indexBuffer.apiObject;
    renderStateGL.boundVertexBuffer = triangles->vertexBuffer.apiObject;
    renderStateGL.boundVertexAttribArrays = vertexMask & 0x141FU;
    renderStateGL.boundVertexAttribPointers = vertexMask & 0x1C7FU;
  }

  const std::uintptr_t indexOffset =
      static_cast<unsigned int>(triangles->indexBuffer.offsetInOtherBuffer) &
      0x7FFFFFFFU;
  const unsigned int mode = (vertexMask & 0x200U) != 0
                                ? GL_TRIANGLE_STRIP_VALUE
                                : GL_TRIANGLES_VALUE;
  const unsigned int indexType = (vertexMask & 0x100U) != 0
                                     ? GL_UNSIGNED_INT_VALUE
                                     : GL_UNSIGNED_SHORT_VALUE;
  const int baseVertex =
      (triangles->vertexBuffer.offsetInOtherBuffer & 0x7FFFFFFF) / stride;
  _glewDrawElementsBaseVertex(mode, numIndexes, indexType,
                              reinterpret_cast<const void*>(indexOffset),
                              baseVertex);
  if (wrapperConfig.finishEveryDraw) {
    glFinish();
  }
  ++wrapperStats.c_drawElements;
  wrapperStats.c_drawIndices += numIndexes;
  wrapperStats.c_drawVertices += triangles->numVerts;
}

void GL_DrawElements(const idODSObject<idDeclRenderProg>& program,
                     const idODSObject<idRenderModelSurface>& surface,
                     std::uint64_t extraState, bool skipDetailTriangles) {
  idTriangles* triangles = surface.objectPtr->geometry;
  if ((triangles->vertexMask & 0x60U) != 0) {
    renderThreadParmState->Set16BitVertexScaleBias(triangles->vsb);
  }
  GL_BindProgram(program, extraState, triangles->vertexMask);
  void* vao = GL_SetupSurfaceParms(program.objectPtr, surface.objectPtr);
  GL_DrawElementsInternal(triangles, vao, skipDetailTriangles);
  GL_ClearSurfaceParms(program.objectPtr, surface.objectPtr);
  if ((triangles->vertexMask & 0x60U) != 0) {
    renderThreadParmState->Set16BitVertexScaleBias(defaultVSB);
  }
  renderLog.OpenBlock("%4i triangles with %s\n", triangles->numIndexes / 3,
                      program.objectPtr->GetName());
}

void GL_DrawElements(const idODSObject<idDeclRenderProg>& program,
                     const idODSObject<idTriangles>& triangles,
                     std::uint64_t extraState, bool skipDetailTriangles) {
  if ((triangles.objectPtr->vertexMask & 0x60U) != 0) {
    renderThreadParmState->Set16BitVertexScaleBias(triangles.objectPtr->vsb);
  }
  GL_BindProgram(program, extraState, triangles.objectPtr->vertexMask);
  GL_DrawElementsInternal(triangles.objectPtr, nullptr, skipDetailTriangles);
  if ((triangles.objectPtr->vertexMask & 0x60U) != 0) {
    renderThreadParmState->Set16BitVertexScaleBias(defaultVSB);
  }
  renderLog.OpenBlock("%4i triangles with %s\n",
                      triangles.objectPtr->numIndexes / 3,
                      program.objectPtr->GetName());
}
