#include "engine/renderer/jobs/render/parmstate.h"

#include "engine/decls/decltable.h"
#include "engine/renderer/autorenderparms.h"
#include "engine/renderer/declrenderprog.h"
#include "shared/idlib/lib.h"

#include <algorithm>
#include <cstring>

idCodeResource<idDeclRenderParm> rp_vertexStScaleBias(
    "vertexStScaleBias");
idCodeResource<idDeclRenderParm> rp_vertexXYZScale("vertexXYZScale");
idCodeResource<idDeclRenderParm> rp_vertexXYZBias("vertexXYZBias");
idCodeResource<idDeclRenderParm> rp_vertexMorphScale("vertexMorphScale");
idCodeResource<idDeclRenderParm> rp_lightingModel("lightingModel");
idCodeResource<idDeclRenderParm> rp_fragmentPositionToWindow(
    "fragmentPositionToWindow");

idParmState renderThreadParmStateObject;
idParmState* renderThreadParmState = &renderThreadParmStateObject;

namespace {

constexpr int kInvalidParmIndex = 2175;

int CodeResourceIndex(const idCodeResource<idDeclRenderParm>& resource) {
  return resource.r->resourceID;
}

}  // namespace

int idParmState::AllocParmState(const idDeclRenderParm* parm) {
  int first = 0;
  int end = 0;
  const char* typeName = "unknown";
  switch (static_cast<int>(parm->parmType)) {
    case PT_VECTOR:
      end = 1024;
      typeName = "vector";
      break;
    case PT_TEXTURE_2D:
    case PT_TEXTURE_3D:
    case PT_TEXTURE_CUBE:
    case PT_TEXTURE_SHADOW_2D:
    case PT_TEXTURE_SHADOW_3D:
    case PT_TEXTURE_SHADOW_CUBE:
    case PT_TEXTURE_DIM_SHADOW_2D:
    case PT_TEXTURE_MULTISAMPLE_2D:
    case PT_IMAGE_2D_READ:
    case PT_IMAGE_2D_WRITE:
      first = 1024;
      end = 1536;
      typeName = "texture";
      break;
    case PT_SAMPLER:
      first = 1536;
      end = 1664;
      typeName = "sampler";
      break;
    case PT_PROGRAM:
      first = 1664;
      end = 2048;
      typeName = "program";
      break;
    case PT_STRING:
      first = 2048;
      end = 2175;
      typeName = "string";
      break;
    default:
      break;
  }

  int parmIndex = kInvalidParmIndex;
  for (int index = first; index < end; ++index) {
    if (parmDecl[index] != nullptr &&
        parmDecl[index]->name.str == parm->name.str) {
      parmIndex = index;
    }
  }
  if (parmIndex == kInvalidParmIndex) {
    for (int index = first; index < end; ++index) {
      if (parmDecl[index] == nullptr) {
        parmIndex = index;
        break;
      }
    }
  }
  if (parmIndex == kInvalidParmIndex) {
    idLib::Error("idParmState::AllocParmState: failed to allocate %s parm state",
                 typeName);
  }

  std::memset(&renderThreadParmState->values[parmIndex], 0,
              sizeof(parmValue_t));
  renderThreadParmState->onParmBlockStack[parmIndex] = 0;
  parmDecl[parmIndex] = parm;
  parmUsageMask[parmIndex] = 0;
  return parmIndex;
}

void idParmState::FreeParmState(const idDeclRenderParm* parm) {
  const int parmIndex = parm->parmIndex;
  if (parmIndex == kInvalidParmIndex)
    return;
  if ((onParmBlockStack[parmIndex] &
       parmStack.virtualParmBlockStackMask) != 0) {
    RollbackVirtualStack(true);
  }
  if (parm->parmType == PT_STRING)
    values[parmIndex].string = nullptr;
  parmDecl[parmIndex] = nullptr;
}

int idParmState::GetTableIndex(const idDeclTable* table) {
  if (table == nullptr)
    return kInvalidParmIndex;
  for (int index = 0; index < numGlobalTables; ++index) {
    if (globalTables[index] == nullptr || globalTables[index] == table) {
      globalTables[index] = table;
      globalLookupTables[index] = table->table;
      return index;
    }
  }
  if (numGlobalTables >= 1024)
    idLib::Error("more than %d parm block tables", 1024);
  const int index = numGlobalTables++;
  globalTables[index] = table;
  globalLookupTables[index] = table->table;
  return index;
}

const idDeclTable* idParmState::GetTable(int tableIndex) const {
  return tableIndex >= 1024 ? nullptr : globalTables[tableIndex];
}

void idParmState::RemoveTable(const idDeclTable* table) {
  for (int index = 0; index < numGlobalTables; ++index) {
    if (globalTables[index] == table) {
      globalTables[index] = nullptr;
      globalLookupTables[index] = nullptr;
      return;
    }
  }
}

const char* idParmState::GetParmNameForIndex(int parmIndex) const {
  return parmDecl[parmIndex]->name.str;
}

const idDeclRenderParm* idParmState::GetRenderParmForIndex(
    int parmIndex) const {
  return parmDecl[parmIndex];
}

void idParmState::DefaultRenderParms() {
  for (const idDeclRenderParm* parm : parmDecl) {
    if (parm != nullptr)
      values[parm->parmIndex] = parm->declaredValue;
  }
}

void idParmState::Init() {
  parmIndexPositionToViewTexture =
      CodeResourceIndex(rp_positionToViewTexture);
  parmIndexRenderPositionToViewTexture =
      CodeResourceIndex(rp_renderPositionToViewTexture);
  parmIndexPositionToFeedback = CodeResourceIndex(rp_positionToFeedback);
  parmIndexFragmentPositionToWindow =
      CodeResourceIndex(rp_fragmentPositionToWindow);
  parmIndexModelMatrixX = CodeResourceIndex(rp_modelMatrixX);
  parmIndexModelMatrixY = CodeResourceIndex(rp_modelMatrixY);
  parmIndexModelMatrixZ = CodeResourceIndex(rp_modelMatrixZ);
  parmIndexModelMatrixW = CodeResourceIndex(rp_modelMatrixW);
  parmIndexInverseModelMatrixX = CodeResourceIndex(rp_inverseModelMatrixX);
  parmIndexInverseModelMatrixY = CodeResourceIndex(rp_inverseModelMatrixY);
  parmIndexInverseModelMatrixZ = CodeResourceIndex(rp_inverseModelMatrixZ);
  parmIndexInverseModelMatrixW = CodeResourceIndex(rp_inverseModelMatrixW);
  parmIndexMVPMatrixX = CodeResourceIndex(rp_mvpMatrixX);
  parmIndexMVPMatrixY = CodeResourceIndex(rp_mvpMatrixY);
  parmIndexMVPMatrixZ = CodeResourceIndex(rp_mvpMatrixZ);
  parmIndexMVPMatrixW = CodeResourceIndex(rp_mvpMatrixW);
  parmIndexPrimeLightDir = CodeResourceIndex(rp_primeLightDir);
  parmIndexPrimeLightColor = CodeResourceIndex(rp_primeLightColor);
  parmIndexLightingModel = CodeResourceIndex(rp_lightingModel);
  parmIndexGlobalLightOrg[0] = CodeResourceIndex(rp_globalLightOrg0);
  parmIndexGlobalLightOrg[1] = CodeResourceIndex(rp_globalLightOrg1);
  parmIndexGlobalLightOrg[2] = CodeResourceIndex(rp_globalLightOrg2);
  parmIndexLightColor[0] = CodeResourceIndex(rp_lightColor0);
  parmIndexLightColor[1] = CodeResourceIndex(rp_lightColor1);
  parmIndexLightColor[2] = CodeResourceIndex(rp_lightColor2);
  parmIndexChannelLight[0] = CodeResourceIndex(rp_channelLight0);
  parmIndexChannelLight[1] = CodeResourceIndex(rp_channelLight1);
  parmIndexChannelLight[2] = CodeResourceIndex(rp_channelLight2);
  parmIndexChannelLight[3] = CodeResourceIndex(rp_channelLight3);
  parmIndexChannelLight[4] = CodeResourceIndex(rp_channelLight4);
  parmIndexChannelLight[5] = CodeResourceIndex(rp_channelLight5);
  parmIndexHighlightColor = CodeResourceIndex(rp_highlightColor);
  parmIndexFacingCorrection = CodeResourceIndex(rp_facingCorrection);
  parmIndexSrcTexClamp = CodeResourceIndex(rp_srcTexClamp);
  parmIndexVertexStScaleBias = CodeResourceIndex(rp_vertexStScaleBias);
  parmIndexVertexXYZScale = CodeResourceIndex(rp_vertexXYZScale);
  parmIndexVertexXYZBias = CodeResourceIndex(rp_vertexXYZBias);
  parmIndexIf = CodeResourceIndex(rp_If);
  parmIndexPolygonOffset = CodeResourceIndex(rp_polygonOffset);
  parmIndexVertexMorphScale = CodeResourceIndex(rp_vertexMorphScale);
}

void idParmState::UpdateParmUsageMasks(const idDeclRenderProg* program) {
  for (int index = 0; index < program->data.vertexParms.num; ++index)
    parmUsageMask[program->data.vertexParms.list[index]] |= 1u;
  for (int index = 0; index < program->data.fragmentParms.num; ++index)
    parmUsageMask[program->data.fragmentParms.list[index]] |= 2u;
  for (int index = 0; index < program->data.textureParms.num; ++index)
    parmUsageMask[program->data.textureParms.list[index]] |= 4u;
  for (int index = 0; index < program->data.samplerParms.num; ++index)
    parmUsageMask[program->data.samplerParms.list[index]] |= 8u;
}
