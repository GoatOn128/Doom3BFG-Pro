#pragma once

#include "engine/models/static/staticmodel.h"
#include "engine/renderer/triangles.h"
#include "shared/idlib/text/str.h"

class idParticleMeshDistributionGenerator {
public:
  static idStr GetBinaryFileName(const char* name);
  static bool GenerateFromTriangles(const idStr& name,
                                    const idTriangles* tris);
  static bool GenerateFromModel(const idStr& name,
                                const idStaticModel* model);
  static bool Generate(const idStr& name);

private:
  static bool GenerateInternal(const idStr& name, const idTriangles* tris);
};
