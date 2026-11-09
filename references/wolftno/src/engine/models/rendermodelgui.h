#pragma once

#include "engine/guis/font.h"
#include "engine/models/rendermodel.h"
#include "engine/renderer/bufferobject.h"
#include "engine/renderer/triangles.h"
#include "shared/idlib/color.h"
#include "shared/idlib/containers/array.h"

#include <cstddef>
#include <cstdint>

class idDeclRenderProg;
class idMaterial;

struct guiModelSurface_t {
  std::uint64_t extraGLState = 0;
  const idMaterial* material = nullptr;
  int numVerts = 0;
  int firstIndex = 0;
  int numIndexes = 0;
  std::uint32_t recoveredPadding = 0;
};

struct idRenderModelGuiRuntimeCallbacks {
  int (*renderWidth)() = nullptr;
  int (*renderHeight)() = nullptr;
  bool (*isNvidia)() = nullptr;
  void (*loadMaterialPagesInBackground)(const idMaterial*) = nullptr;
};

void SetRenderModelGuiRuntimeCallbacks(
    const idRenderModelGuiRuntimeCallbacks& callbacks);
void SetRenderModelGuiResources(const idMaterial* whiteMaterial,
                                idFont* font);

class idRenderModelGui : public idRenderModel {
public:
  static constexpr int MAX_GUI_SURFACES = 256;
  static constexpr int NUM_BUFFERS = 10;
  static constexpr int MAX_GUI_VERTICES = 0x10000;
  static constexpr int MAX_GUI_INDEXES = 0x20000;

  idRenderModelGui();
  ~idRenderModelGui() override;

  static bool IsInitialized();
  static void Init();
  static void Shutdown();
  static void StartFrame();

  void SetVirtualSize(int width, int height);
  void SetViewport(int x, int y, int width, int height);
  float GetVirtualWidth();
  float GetVirtualHeight();
  float GetVirtualWidth() const {
    return const_cast<idRenderModelGui*>(this)->GetVirtualWidth();
  }
  float GetVirtualHeight() const {
    return const_cast<idRenderModelGui*>(this)->GetVirtualHeight();
  }

private:
  // Retail decorates this helper as a private member (AEAAXXZ).
  void AdvanceSurf();

public:
  void SetGLState(std::uint64_t bits);
  idDrawVert* AllocTris(int numVerts, const std::uint16_t* indexes,
                        int numIndexes, const idMaterial* material);
  void DrawTris(const idDrawVert* verts, int numVerts,
                const std::uint16_t* indexes, int numIndexes,
                const idMaterial* material);
  void DrawStretchPic(const idVec4& topLeft, const idVec4& topRight,
                      const idVec4& bottomRight, const idVec4& bottomLeft,
                      const idMaterial* material, float z);
  void DrawStretchPic(float x, float y, float z, float width, float height,
                      float s1, float t1, float s2, float t2,
                      const idMaterial* material) {
    const idVec4 topLeft{x, y, s1, t1};
    const idVec4 topRight{x + width, y, s2, t1};
    const idVec4 bottomRight{x + width, y + height, s2, t2};
    const idVec4 bottomLeft{x, y + height, s1, t2};
    DrawStretchPic(topLeft, topRight, bottomRight, bottomLeft, material, z);
  }

  int GetStringWidth(const char* string, float scale);
  int GetStringHeight(float scale);
  void SetupMonospaceFont(float drawWidth);
  void Clear(std::uint64_t bits);
  bool UpdateInView(const idRenderView* currentView,
                    const idRenderView* nextView,
                    idRenderModelUpdateTools* updateTools) override;
  void DrawFilled(const idVec4& color, float x, float y, float width,
                  float height);
  void DrawPic(float x, float y, const idMaterial* material);
  void DrawChar(float x, float y, unsigned character, float scale);
  void DrawString(float x, float y, const char* string,
                  const idVec4& defaultColor, bool forceColor, float scale);
  bool CommitSubclass() override;

  static idArray<idVertexBuffer, NUM_BUFFERS> vertexBuffer;
  static idArray<idIndexBuffer, NUM_BUFFERS> indexBuffer;
  static int mappedBufferIndex;
  static idDrawVert* mappedVertices;
  static std::uint16_t* mappedIndices;
  static int numMappedVertices;
  static int numMappedIndices;
  static int frameCount;
  static bool initialized;

  guiModelSurface_t* surf;
  std::uint32_t currentVertexColor;
  int numGuiSurfaces;
  idArray<guiModelSurface_t, MAX_GUI_SURFACES> guiSurfaces;
  idArray<idTriangles, MAX_GUI_SURFACES> triangles;
  bool useVirtualSize;
  std::uint8_t recoveredVirtualPadding[3];
  int frameNum;
  int waitForFrame;
  std::uint32_t recoveredTailPadding;
};

static_assert(sizeof(idArray<idVertexBuffer, 10>) == 240,
              "GUI vertex buffer array ABI drift");
static_assert(sizeof(idArray<idIndexBuffer, 10>) == 240,
              "GUI index buffer array ABI drift");
static_assert(sizeof(guiModelSurface_t) == 32,
              "guiModelSurface_t ABI drift");
static_assert(offsetof(guiModelSurface_t, material) == 8,
              "GUI surface material offset drift");
static_assert(offsetof(guiModelSurface_t, numVerts) == 16,
              "GUI surface vertex count offset drift");
static_assert(offsetof(guiModelSurface_t, firstIndex) == 20,
              "GUI surface first-index offset drift");
static_assert(offsetof(guiModelSurface_t, numIndexes) == 24,
              "GUI surface index count offset drift");
static_assert(offsetof(idRenderModelGui, surf) == 0x880,
              "GUI current surface offset drift");
static_assert(offsetof(idRenderModelGui, currentVertexColor) == 0x888,
              "GUI vertex color offset drift");
static_assert(offsetof(idRenderModelGui, numGuiSurfaces) == 0x88C,
              "GUI surface count offset drift");
static_assert(offsetof(idRenderModelGui, guiSurfaces) == 0x890,
              "GUI surface array offset drift");
static_assert(offsetof(idRenderModelGui, triangles) == 0x2890,
              "GUI triangles offset drift");
static_assert(offsetof(idRenderModelGui, useVirtualSize) == 0xC890,
              "GUI virtual-size flag offset drift");
static_assert(offsetof(idRenderModelGui, frameNum) == 0xC894,
              "GUI frame number offset drift");
static_assert(offsetof(idRenderModelGui, waitForFrame) == 0xC898,
              "GUI wait frame offset drift");
static_assert(sizeof(idRenderModelGui) == 0xC8A0,
              "idRenderModelGui ABI drift");
