#pragma once

#include "engine/models/glass/glassmodel.h"

#include <cstddef>
#include <cstdint>

class idHashIndex;
class idStaticModel;

struct glassSourceSurface_t {
  idTriangles* triangles{};
  const idMaterial* material{};
  int declIndex{};
};

const idMaterial* GetSurfaceTypeMaterial(const idMaterial* material);

class idGlassModelGenerator {
public:
  idGlassModelGenerator();
  ~idGlassModelGenerator();

  void AddModel(const idStaticModel* model, const char* declName);
  int AddDecl(const char* declName);
  bool Generate();
  bool WriteBinary(const char* filename);

private:
  static int FindVertexId(const idHashIndex& hash, const idVec3& vertex,
                          idDrawVert* vertices, int numVertices);
  void FreeSurfaces();
  void FreeSourceSurfaces();
  void GenerateIslands(const glassSourceSurface_t& sourceSurface);

public:
  idList<idStr, 5> decls;
  idList<glassSourceSurface_t, 5> sourceSurfaces;
  idList<idGlassModelSurface, 5> glassSurfaces;
  int sourceTimestamp{-1};
  std::uint32_t recoveredPadding{};

  friend class idGlassModel;
  friend class GlassModelValidationAccess;
};

static_assert(sizeof(glassSourceSurface_t) == 24,
              "glass source surface ABI drift");
static_assert(offsetof(glassSourceSurface_t, triangles) == 0,
              "glass source triangles offset drift");
static_assert(offsetof(glassSourceSurface_t, material) == 8,
              "glass source material offset drift");
static_assert(offsetof(glassSourceSurface_t, declIndex) == 16,
              "glass source decl offset drift");
static_assert(sizeof(idGlassModelGenerator) == 80,
              "glass model generator ABI drift");
static_assert(offsetof(idGlassModelGenerator, sourceSurfaces) == 24,
              "glass generator source-surfaces offset drift");
static_assert(offsetof(idGlassModelGenerator, glassSurfaces) == 48,
              "glass generator glass-surfaces offset drift");
static_assert(offsetof(idGlassModelGenerator, sourceTimestamp) == 72,
              "glass generator timestamp offset drift");
