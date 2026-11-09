#include "engine/models/rendermodelgui.h"

#include "engine/framework/coderesource.h"
#include "engine/renderer/declmaterial.h"
#include "engine/renderer/declrenderprog.h"
#include "engine/renderer/renderersystem.h"
#include "shared/idlib/csystems/cvarsystemlocal.h"
#include "shared/idlib/lib.h"
#include "shared/idlib/sys/sys_time.h"
#include "shared/idlib/text/str.h"

#include <cstring>

idArray<idVertexBuffer, idRenderModelGui::NUM_BUFFERS>
    idRenderModelGui::vertexBuffer;
idArray<idIndexBuffer, idRenderModelGui::NUM_BUFFERS>
    idRenderModelGui::indexBuffer;
int idRenderModelGui::mappedBufferIndex = -1;
idDrawVert* idRenderModelGui::mappedVertices = nullptr;
std::uint16_t* idRenderModelGui::mappedIndices = nullptr;
int idRenderModelGui::numMappedVertices = 0;
int idRenderModelGui::numMappedIndices = 0;
int idRenderModelGui::frameCount = 0;
bool idRenderModelGui::initialized = false;

// The initialized retail data was read back from WolfNewOrder_x64.exe.  The
// small-character dimensions live in zero-initialized storage and are filled by
// SetupMonospaceFont; the remaining font metrics begin at one.
float SMALLCHAR_HEIGHT = 0.0f;
float SMALLCHAR_WIDTH = 0.0f;
float useScale = 1.0f;
float oneOverWidth = 1.0f;
float oneOverHeight = 1.0f;
int fontWidth = 1;
int fontHeight = 1;

idCodeResource<idMaterial> whiteShader("_white");
idCodeResource<idFont> monospaceFont(idFont::DEFAULT_FONT);
idCodeResource<idDeclRenderProg> progGuiBlend("guiBlend");

idCVar r_guiDebug("r_guiDebug", "0", CVAR_BOOL,
                  "Provides debug output in idRenderModelGui code");

namespace {

idRenderModelGuiRuntimeCallbacks runtimeCallbacks{};
constexpr idVec4 colorWhite{1.0f, 1.0f, 1.0f, 1.0f};

int RenderWidth() {
  if (runtimeCallbacks.renderWidth != nullptr)
    return runtimeCallbacks.renderWidth();
  return renderSystem != nullptr ? renderSystem->GetWidth() : 0;
}

int RenderHeight() {
  if (runtimeCallbacks.renderHeight != nullptr)
    return runtimeCallbacks.renderHeight();
  return renderSystem != nullptr ? renderSystem->GetHeight() : 0;
}

} // namespace

void SetRenderModelGuiRuntimeCallbacks(
    const idRenderModelGuiRuntimeCallbacks& callbacks) {
  runtimeCallbacks = callbacks;
}

void SetRenderModelGuiResources(const idMaterial* whiteMaterial,
                                idFont* font) {
  whiteShader.r = const_cast<idMaterial*>(whiteMaterial);
  monospaceFont.r = font;
}

bool idRenderModelGui::IsInitialized() { return initialized; }

void idRenderModelGui::Init() {
  for (int index = 0; index < NUM_BUFFERS; ++index) {
    vertexBuffer.ptr[index].AllocBufferObject(
        nullptr, 0x200000, 31, BU_DYNAMIC);
    indexBuffer.ptr[index].AllocBufferObject(nullptr, 0x40000, BU_DYNAMIC);
  }
  mappedBufferIndex = -1;
  mappedVertices = nullptr;
  mappedIndices = nullptr;
  numMappedVertices = 0;
  numMappedIndices = 0;
  frameCount = 0;
  initialized = true;
}

void idRenderModelGui::Shutdown() {
  if (mappedBufferIndex >= 0) {
    vertexBuffer.ptr[mappedBufferIndex].UnmapBuffer();
    indexBuffer.ptr[mappedBufferIndex].UnmapBuffer();
  }
  for (int index = 0; index < NUM_BUFFERS; ++index) {
    vertexBuffer.ptr[index].FreeBufferObject();
    indexBuffer.ptr[index].FreeBufferObject();
  }
  mappedVertices = nullptr;
  mappedIndices = nullptr;
  numMappedVertices = 0;
  numMappedIndices = 0;
  frameCount = 0;
  initialized = false;
  mappedBufferIndex = -1;
}

void idRenderModelGui::StartFrame() {
  if (!initialized)
    return;
  if (mappedBufferIndex >= 0) {
    vertexBuffer.ptr[mappedBufferIndex].UnmapBuffer();
    indexBuffer.ptr[mappedBufferIndex].UnmapBuffer();
  }
  mappedBufferIndex = (frameCount + 1) % NUM_BUFFERS;
  const bool isNvidia = runtimeCallbacks.isNvidia != nullptr &&
                        runtimeCallbacks.isNvidia();
  const int mapType = isNvidia ? 2 : 3;
  mappedVertices = static_cast<idDrawVert*>(
      vertexBuffer.ptr[mappedBufferIndex].MapBuffer(mapType));
  mappedIndices = static_cast<std::uint16_t*>(
      indexBuffer.ptr[mappedBufferIndex].MapBuffer(mapType));
  ++frameCount;
  numMappedVertices = 0;
  numMappedIndices = 0;
}

idRenderModelGui::idRenderModelGui()
    : idRenderModel(), surf(nullptr), currentVertexColor(0),
      numGuiSurfaces(0), guiSurfaces{}, triangles{}, useVirtualSize(false),
      recoveredVirtualPadding{}, frameNum(0), waitForFrame(0),
      recoveredTailPadding(0) {
  renderModelType = RENDERMODEL_GUI;
  SetName("_guiModel");

  int width = RenderWidth();
  if (width <= 0)
    width = 1280;
  int height = RenderHeight();
  if (height <= 0)
    height = 720;

  g.flags2 |= 0x01U;
  useDeferredPosition &= static_cast<std::uint8_t>(~1U);
  g.flags0 |= 0x80U;
  g.flags1 |= 0x01U;
  g.axis = mat3_identity;
  deferredAxis = mat3_identity;
  g.origin = {-1.0f, 1.0f, 0.0f};
  deferredOrigin = {-1.0f, 1.0f, 0.0f};
  g.scale = {2.0f / static_cast<float>(width),
             -2.0f / static_cast<float>(height), 1.0f};
  SetViewport(0, 0, width, height);
  SetMaxSurfaces(MAX_GUI_SURFACES);
}

idRenderModelGui::~idRenderModelGui() = default;

void idRenderModelGui::SetVirtualSize(int width, int height) {
  g.scale.z = 1.0f;
  g.scale.x = 2.0f / static_cast<float>(width);
  g.scale.y = -2.0f / static_cast<float>(height);
  useVirtualSize = true;
}

void idRenderModelGui::SetViewport(int x, int y, int width, int height) {
  idRenderModel::SetViewport(x, y, width, height);
  if (!useVirtualSize) {
    g.scale.z = 1.0f;
    g.scale.x = 2.0f / static_cast<float>(width);
    g.scale.y = -2.0f / static_cast<float>(height);
  }
}

float idRenderModelGui::GetVirtualWidth() {
  if (useVirtualSize)
    return 2.0f / g.scale.x;
  return static_cast<float>(g.viewport.x2 - g.viewport.x1) + 1.0f;
}

float idRenderModelGui::GetVirtualHeight() {
  if (useVirtualSize)
    return -2.0f / g.scale.y;
  return static_cast<float>(g.viewport.y2 - g.viewport.y1) + 1.0f;
}

void idRenderModelGui::AdvanceSurf() {
  if (numGuiSurfaces == MAX_GUI_SURFACES) {
    idLib::Warning("MAX_GUI_SURFACES exceeded!");
    return;
  }
  int currentFrame = frameCount;
  if (numGuiSurfaces > 0 && frameCount != frameNum) {
    idLib::Warning("idRenderModelGui::AdvanceSurf: framenu missmatch %d, %d",
                   frameCount, frameNum);
    currentFrame = frameCount;
  }
  frameNum = currentFrame;
  guiModelSurface_t* next = &guiSurfaces.ptr[numGuiSurfaces];
  if (numGuiSurfaces <= 0) {
    next->material = nullptr;
    next->extraGLState = 0;
  } else {
    next->material = surf->material;
    next->extraGLState = surf->extraGLState;
  }
  next->numIndexes = 0;
  next->numVerts = 0;
  next->firstIndex = numMappedIndices;
  ++numGuiSurfaces;
  surf = next;
}

void idRenderModelGui::SetGLState(std::uint64_t bits) {
  if (surf != nullptr && bits != surf->extraGLState) {
    if (surf->numVerts != 0)
      AdvanceSurf();
    surf->firstIndex = numMappedIndices;
    surf->extraGLState = bits;
  }
}

idDrawVert* idRenderModelGui::AllocTris(
    int numVerts, const std::uint16_t* indexes, int numIndexes,
    const idMaterial* material) {
  if (material == nullptr || numIndexes == 0 || surf == nullptr)
    return nullptr;
  if (material != surf->material) {
    if (surf->numVerts != 0)
      AdvanceSurf();
    surf->firstIndex = numMappedIndices;
    surf->material = material;
  }

  const int vertexBase = numMappedVertices;
  if (numMappedVertices + numVerts > MAX_GUI_VERTICES ||
      numMappedIndices + numIndexes + 3 > MAX_GUI_INDEXES) {
    return nullptr;
  }

  int outputCount = numIndexes;
  const int pairCount = numIndexes / 2;
  for (int pair = 0; pair < pairCount; ++pair) {
    mappedIndices[numMappedIndices + pair * 2] = static_cast<std::uint16_t>(
        vertexBase + indexes[pair * 2]);
    mappedIndices[numMappedIndices + pair * 2 + 1] =
        static_cast<std::uint16_t>(vertexBase + indexes[pair * 2 + 1]);
  }
  if ((numIndexes & 1) != 0) {
    const std::uint16_t last = static_cast<std::uint16_t>(
        vertexBase + indexes[numIndexes - 1]);
    const int output = numMappedIndices + pairCount * 2;
    mappedIndices[output] = last;
    mappedIndices[output + 1] = last;
    mappedIndices[output + 2] = last;
    mappedIndices[output + 3] = last;
    outputCount += 3;
  }

  surf->numVerts += numVerts;
  surf->numIndexes += outputCount;
  numMappedVertices += numVerts;
  numMappedIndices += outputCount;
  return &mappedVertices[vertexBase];
}

void idRenderModelGui::DrawTris(const idDrawVert* verts, int numVerts,
                                const std::uint16_t* indexes, int numIndexes,
                                const idMaterial* material) {
  idDrawVert* destination =
      AllocTris(numVerts, indexes, numIndexes, material);
  if (destination != nullptr && numVerts > 0) {
    std::memcpy(destination, verts,
                static_cast<std::size_t>(numVerts) * sizeof(idDrawVert));
  }
}

void idRenderModelGui::DrawStretchPic(
    const idVec4& topLeft, const idVec4& topRight,
    const idVec4& bottomRight, const idVec4& bottomLeft,
    const idMaterial* material, float z) {
  if (material == nullptr) {
    if (r_guiDebug.valueInteger != 0) {
      idLib::Printf(
          "idRenderModelGui::DrawStretchPic() - Material is NULL. NOT DRAWING.\n");
    }
    return;
  }
  if (surf == nullptr) {
    if (r_guiDebug.valueInteger != 0)
      idLib::Printf("idRenderModelGui::DrawStretchPic() - surf is NULL.\n");
    return;
  }
  if (material != surf->material) {
    if (surf->numVerts != 0)
      AdvanceSurf();
    surf->firstIndex = numMappedIndices;
    surf->material = material;
  }

  const int vertexBase = numMappedVertices;
  const int indexBase = numMappedIndices;
  if (numMappedVertices + 4 > MAX_GUI_VERTICES ||
      numMappedIndices + 6 > MAX_GUI_INDEXES) {
    if (r_guiDebug.valueInteger != 0)
      idLib::Printf("idRenderModelGui::DrawStretchPic() - buffer full.\n");
    return;
  }

  numMappedVertices += 4;
  numMappedIndices += 6;
  surf->numVerts += 4;
  surf->numIndexes += 6;

  const idVec4 corners[4] = {topLeft, topRight, bottomRight, bottomLeft};
  for (int index = 0; index < 4; ++index) {
    idDrawVert& vertex = mappedVertices[vertexBase + index];
    std::memset(&vertex, 0, sizeof(vertex));
    vertex.xyz = {corners[index].x, corners[index].y, z};
    vertex.st = {corners[index].z, corners[index].w};
    std::memset(vertex.tangent, 0x7f, sizeof(vertex.tangent));
    std::memcpy(vertex.color, &currentVertexColor,
                sizeof(currentVertexColor));
  }

  mappedIndices[indexBase + 0] = static_cast<std::uint16_t>(vertexBase + 3);
  mappedIndices[indexBase + 1] = static_cast<std::uint16_t>(vertexBase + 0);
  mappedIndices[indexBase + 2] = static_cast<std::uint16_t>(vertexBase + 2);
  mappedIndices[indexBase + 3] = static_cast<std::uint16_t>(vertexBase + 2);
  mappedIndices[indexBase + 4] = static_cast<std::uint16_t>(vertexBase + 0);
  mappedIndices[indexBase + 5] = static_cast<std::uint16_t>(vertexBase + 1);
}

int idRenderModelGui::GetStringWidth(const char* string, float scale) {
  int count = 0;
  while (*string != '\0') {
    if (idStr::IsColor(string))
      string += 2;
    else {
      ++string;
      ++count;
    }
  }
  return static_cast<int>(static_cast<float>(count) * SMALLCHAR_WIDTH *
                          scale);
}

int idRenderModelGui::GetStringHeight(float scale) {
  return static_cast<int>(SMALLCHAR_HEIGHT * scale);
}

void idRenderModelGui::SetupMonospaceFont(float drawWidth) {
  SMALLCHAR_WIDTH = drawWidth;
  const idFont* font = static_cast<const idFont*>(monospaceFont.r);
  const glyphInfo_t& glyph = font->GetGlyph('A');
  SMALLCHAR_HEIGHT = static_cast<float>(font->GetPointSize()) *
                     SMALLCHAR_WIDTH / static_cast<float>(glyph.xSkip);
  useScale = SMALLCHAR_WIDTH / static_cast<float>(glyph.xSkip);
  fontWidth = static_cast<int>(font->GetMaterialWidth());
  fontHeight = static_cast<int>(font->GetMaterialHeight());
  oneOverWidth = 1.0f / static_cast<float>(fontWidth);
  oneOverHeight = 1.0f / static_cast<float>(fontHeight);
}

void idRenderModelGui::Clear(std::uint64_t bits) {
  numGuiSurfaces = 0;
  if (!idLib::headless) {
    const std::uint64_t start = Sys_Microseconds();
    if (frameCount < waitForFrame) {
      do {
        Sys_Sleep(1);
      } while (Sys_Microseconds() - start <= 16000 &&
               frameCount < waitForFrame);
    }
    const std::uint64_t elapsed = Sys_Microseconds() - start;
    if (elapsed > 5000) {
      idLib::Warning("idRenderModelGui::Clear: waited for buffers for %.3fms",
                     static_cast<float>(static_cast<int>(elapsed)) / 1000.0f);
    }
  }
  AdvanceSurf();
  SetGLState(bits);
}

bool idRenderModelGui::UpdateInView(const idRenderView*, const idRenderView*,
                                    idRenderModelUpdateTools*) {
  for (int index = 0; index < surfaces.Num(); ++index) {
    const idRenderModelSurface& surface = surfaces[index];
    if (surface.material != nullptr &&
        runtimeCallbacks.loadMaterialPagesInBackground != nullptr) {
      runtimeCallbacks.loadMaterialPagesInBackground(surface.material);
    }
  }
  return true;
}

void idRenderModelGui::DrawFilled(const idVec4& color, float x, float y,
                                  float width, float height) {
  const std::uint32_t savedColor = currentVertexColor;
  currentVertexColor = idColor::PackColor(color);
  DrawStretchPic(x, y, 0.0f, width, height, 0.0f, 0.0f, 1.0f, 1.0f,
                 static_cast<const idMaterial*>(whiteShader.r));
  currentVertexColor = savedColor;
}

void idRenderModelGui::DrawPic(float x, float y,
                               const idMaterial* material) {
  const float width = static_cast<float>(material->GetWidth());
  const float height = static_cast<float>(material->GetHeight());
  DrawStretchPic(x, y, 0.0f, width, height, 0.0f, 0.0f, 1.0f, 1.0f,
                 material);
}

void idRenderModelGui::DrawChar(float x, float y, unsigned character,
                                float scale) {
  const idFont* font = static_cast<const idFont*>(monospaceFont.r);
  if (static_cast<int>(font->GetMaterialWidth()) != fontWidth ||
      static_cast<int>(font->GetMaterialHeight()) != fontHeight) {
    SetupMonospaceFont(SMALLCHAR_WIDTH);
  }
  const float bottom = y + SMALLCHAR_HEIGHT;
  const glyphInfo_t& glyph = font->GetGlyph(character);
  const float glyphScale = useScale * scale;
  const float drawWidth = static_cast<float>(glyph.width) + 1.0f;
  const float s1 = (static_cast<float>(glyph.s) - 0.5f) * oneOverWidth;
  const float t1 = (static_cast<float>(glyph.t) - 0.5f) * oneOverHeight;
  const float drawHeight = static_cast<float>(glyph.height) + 1.0f;
  const float s2 =
      (static_cast<float>(glyph.s + glyph.width) + 0.5f) * oneOverWidth;
  const float t2 =
      (static_cast<float>(glyph.t + glyph.height) + 0.5f) * oneOverHeight;
  const float top = static_cast<float>(glyph.top) * glyphScale;
  const float left =
      ((SMALLCHAR_WIDTH / useScale - static_cast<float>(glyph.xSkip)) *
           0.5f +
       static_cast<float>(glyph.left)) *
      glyphScale;
  DrawStretchPic(left + x, bottom - top, 0.0f, glyphScale * drawWidth,
                 glyphScale * drawHeight, s1, t1, s2, t2,
                 font->GetMaterial());
}

void idRenderModelGui::DrawString(float x, float y, const char* string,
                                  const idVec4& defaultColor,
                                  bool forceColor, float scale) {
  currentVertexColor = idColor::PackColor(defaultColor);
  while (*string != '\0') {
    if (!idStr::IsColor(string)) {
      DrawChar(x, y, static_cast<unsigned char>(*string), scale);
      ++string;
      x += SMALLCHAR_WIDTH * scale;
      continue;
    }
    if (!forceColor) {
      if (string[1] == '0') {
        currentVertexColor = idColor::PackColor(defaultColor);
      } else {
        idVec4 color = idStr::ColorForIndex(string[1]);
        color.w = defaultColor.w;
        currentVertexColor = idColor::PackColor(color);
      }
    }
    string += 2;
  }
  currentVertexColor = idColor::PackColor(colorWhite);
}

bool idRenderModelGui::CommitSubclass() {
  surfaces.Clear();
  waitForFrame = frameNum + 1;
  if (numGuiSurfaces <= 0)
    return false;
  if (guiSurfaces.ptr[0].numVerts != 0) {
    if (frameCount != frameNum) {
      idLib::Warning(
          "idRenderModelGui::CommitSubclass: framenu missmatch %d, %d",
          frameCount, frameNum);
    }
    if (numGuiSurfaces > surfaces.size)
      surfaces.ResizeForPort(numGuiSurfaces);
    for (int index = 0; index < numGuiSurfaces; ++index) {
      const guiModelSurface_t& guiSurface = guiSurfaces.ptr[index];
      if (guiSurface.numVerts == 0 || guiSurface.material == nullptr)
        continue;
      idRenderModelSurface* surface = &surfaces.Alloc();
      surface->material = guiSurface.material;
      surface->geometryIsReference = true;
      surface->extraGLState = guiSurface.extraGLState;
      surface->geometry = &triangles.ptr[index];
      surface->geometry->numVerts = guiSurface.numVerts;
      surface->geometry->numIndexes = guiSurface.numIndexes;
      surface->geometry->vertexBuffer.Reference(
          &vertexBuffer.ptr[mappedBufferIndex]);
      surface->geometry->indexBuffer.Reference(
          &indexBuffer.ptr[mappedBufferIndex], 2 * guiSurface.firstIndex,
          2 * guiSurface.numIndexes);
    }
    numGuiSurfaces = 0;
  }
  return true;
}

// The recovered export contains a DIA spillover record for
// idClientGameMsg_VehicleWeaponChange::GetMsgType.  Its own metadata names
// ClientGame_reliable.obj, not RenderModelGui.obj, so that game compiland record
// is intentionally accounted here but is not duplicated into engine/models.
