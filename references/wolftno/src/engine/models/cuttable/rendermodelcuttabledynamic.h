#pragma once

#include "engine/models/rendermodel.h"

#include <cstdint>

class idMaterial;

class idRenderModelCuttableDynamic final : public idRenderModel {
public:
  enum : int {
    MAX_VERTICES = 512,
    MAX_INDICES = 6144,
  };

  idRenderModelCuttableDynamic();
  ~idRenderModelCuttableDynamic() override;

  bool IsValid() const;
  void Upload(const idMaterial* renderMaterial,
              const idMaterial* depthMaterial,
              const idList<idDrawVert>& vertices, const idList<idVec2>& st,
              const idList<std::uint16_t>& indices);

private:
  void UploadRenderGeometry(const idMaterial* material,
                            const idList<idDrawVert>& vertices,
                            const idList<std::uint16_t>& indices);
  void UploadDepthGeometry(const idMaterial* material,
                           const idList<idVec2>& st);
};

static_assert(sizeof(idRenderModelCuttableDynamic) == 2176,
              "idRenderModelCuttableDynamic ABI drift");
