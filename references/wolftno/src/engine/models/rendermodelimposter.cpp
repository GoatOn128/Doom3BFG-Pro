#include "engine/models/rendermodelimposter.h"

namespace {

void CopyReferencedSurface(idRenderModelSurface& destination,
                           const idRenderModelSurface& source,
                           bool copyBinaryModelId, bool copyParms) {
  destination.material = source.material;
  destination.materialNum = source.materialNum;
  destination.extraGLState = source.extraGLState;
  destination.geometry = source.geometry;
  destination.geometryIsReference = true;
  destination.referenceMask = 0x3f;
  if (copyBinaryModelId)
    destination.binaryModelId = source.binaryModelId;
  destination.joints = source.joints;
  destination.surfaceConstants = source.surfaceConstants;
  destination.morphMap = source.morphMap;
  destination.morphMultiplier = source.morphMap != nullptr ? 1.0f : 0.0f;
  destination.stMap = source.stMap;
  destination.parms = copyParms ? source.parms : nullptr;
  destination.skinOffsets = source.skinOffsets;
}

} // namespace

idRenderModelImposter::idRenderModelImposter() {
  parent = nullptr;
  renderModelType = RENDERMODEL_IMPOSTER;
}

idRenderModelImposter::~idRenderModelImposter() = default;

void UnlinkFromImposterChain(idRenderModel* imposter) {
  idRenderModel* imposterParent = imposter->recoveredParmLinks.imposterParent;
  idRenderModel* imposterNext = imposterParent->recoveredParmLinks.imposterNext;
  if (imposterNext == imposter) {
    imposterParent->recoveredParmLinks.imposterNext =
        imposter->recoveredParmLinks.imposterNext;
  } else if (imposterNext != nullptr) {
    while (imposterNext->recoveredParmLinks.imposterNext != imposter) {
      imposterNext = imposterNext->recoveredParmLinks.imposterNext;
      if (imposterNext == nullptr)
        break;
    }
    if (imposterNext != nullptr) {
      imposterNext->recoveredParmLinks.imposterNext =
          imposter->recoveredParmLinks.imposterNext;
    }
  }

  imposter->recoveredParmLinks.imposterParent = nullptr;
  imposter->recoveredParmLinks.imposterNext = nullptr;
  if (imposterParent->recoveredParmLinks.imposterNext == nullptr)
    imposterParent->recoveredParmLinks.imposterParent = nullptr;
}

bool idRenderModelImposter::UpdateInView(const idRenderView*,
                                         const idRenderView*,
                                         idRenderModelUpdateTools*) {
  if (parent == nullptr) {
    surfaces.ClearFree();
    return false;
  }

  for (int index = 0; index < surfaces.Num(); ++index)
    CopyReferencedSurface(surfaces[index], parent->surfaces[index], false,
                          true);
  referenceBounds = parent->referenceBounds;
  return true;
}

bool idRenderModelImposter::MimicRenderModel(idRenderModel* parentModel) {
  if (parentModel == nullptr) {
    if (recoveredParmLinks.imposterParent != nullptr)
      UnlinkFromImposterChain(this);
    FreeSurfaces();
    parent = nullptr;
    return false;
  }

  if (parent == parentModel && parentModel->surfaces.Num() == surfaces.Num())
    return false;

  FreeSurfaces();
  if (parent != parentModel) {
    if (recoveredParmLinks.imposterParent != nullptr)
      UnlinkFromImposterChain(this);
    recoveredParmLinks.imposterParent = parentModel;
    recoveredParmLinks.imposterNext =
        parentModel->recoveredParmLinks.imposterNext;
    parentModel->recoveredParmLinks.imposterNext = this;
    if (parentModel->recoveredParmLinks.imposterParent == nullptr)
      parentModel->recoveredParmLinks.imposterParent = parentModel;
  }

  parent = parentModel;
  for (int index = 0; index < parentModel->surfaces.Num(); ++index) {
    idRenderModelSurface surface{};
    CopyReferencedSurface(surface, parentModel->surfaces[index], true, false);
    AddSurface(surface);
  }
  FinishSurfaces();
  referenceBounds = parentModel->referenceBounds;
  return true;
}
