#include "engine/cm/collisionmodelbuilder.h"

#include <algorithm>
#include <cctype>
#include <string>

std::shared_ptr<idCollisionModelLocal>
idCollisionModelBuilder::BuildSphere(float radius, const char* modelName,
                                     std::uint32_t contents) const {
  auto model = std::make_shared<idCollisionModelLocal>();
  model->FreeData();
  model->name = modelName ? modelName : "sphere";
  model->type = CM_TYPE_SPHERE;
  model->sphereRadius = std::max(radius, 0.0f);
  model->bounds[0] = {-model->sphereRadius, -model->sphereRadius, -model->sphereRadius};
  model->bounds[1] = {model->sphereRadius, model->sphereRadius, model->sphereRadius};
  cmPolytope_t volume;
  volume.bounds = model->bounds;
  volume.contents = contents;
  model->polytopes.push_back(std::move(volume));
  return model;
}

bool idCollisionModelBuilder::IsAnimatedRenderModel(const char* name) const {
  if (!name) {
    return false;
  }
  std::string lower(name);
  std::transform(lower.begin(), lower.end(), lower.begin(),
                 [](unsigned char value) { return static_cast<char>(std::tolower(value)); });
  return lower.find(".md5mesh") != std::string::npos || lower.find(".md6") != std::string::npos;
}

std::shared_ptr<idCollisionModelLocal>
idCollisionModelBuilder::BuildForDeclMD6(const char* name, const idBounds& animatedBounds,
                                         std::uint32_t contents) const {
  // MD6 collision in retail is generated from the declaration's conservative
  // animated bounds.  Keeping that volume conservative prevents missed hits as
  // joints move between poses.
  return BuildForBounds(animatedBounds, name ? name : "md6", contents);
}
