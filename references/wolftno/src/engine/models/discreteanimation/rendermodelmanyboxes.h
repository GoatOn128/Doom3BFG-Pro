#pragma once

#include "engine/models/rendermodel.h"
#include "engine/renderer/bufferobject.h"

#include <cstddef>

class idMaterial;

class alignas(16) idRenderModelManyBoxes : public idRenderModel {
public:
  struct box_t {
    idVec3 position;
    idMat3 orientation;
    float radius;
  };

  idRenderModelManyBoxes();

  int gameTime;
  const idMaterial* material;
  idList<box_t, 5> boxes;
  idVertexBuffer vertexBuffers[2];
  idTriangles triangles;
  idIndexBuffer indexBuffer;

protected:
  ~idRenderModelManyBoxes() override;
  bool CommitSubclass() override;

  friend class RenderModelManyBoxesValidationAccess;
};

static_assert(sizeof(idRenderModelManyBoxes::box_t) == 52,
              "many-box element ABI drift");
static_assert(offsetof(idRenderModelManyBoxes, gameTime) == 0x880,
              "many-box game-time offset drift");
static_assert(offsetof(idRenderModelManyBoxes, material) == 0x888,
              "many-box material offset drift");
static_assert(offsetof(idRenderModelManyBoxes, boxes) == 0x890,
              "many-box list offset drift");
static_assert(offsetof(idRenderModelManyBoxes, vertexBuffers) == 0x8a8,
              "many-box vertex-buffer offset drift");
static_assert(offsetof(idRenderModelManyBoxes, triangles) == 0x8d8,
              "many-box triangles offset drift");
static_assert(offsetof(idRenderModelManyBoxes, indexBuffer) == 0x978,
              "many-box index-buffer offset drift");
static_assert(sizeof(idRenderModelManyBoxes) == 0x990,
              "idRenderModelManyBoxes ABI drift");
