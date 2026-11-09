#include "engine/renderer/jobs/render/parmstate.h"

#include "engine/renderer/approximatelighting.h"
#include "engine/renderer/parmblock.h"
#include "shared/idlib/lookuptable.h"
#include "shared/idlib/math/vector.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <immintrin.h>

int c_opsBlocks = 0;
int c_ops = 0;

namespace {

constexpr int kInvalidParmIndex = 2175;

void TouchExplicitParm(idParmState& state, int parmIndex) {
  const unsigned char usage = state.parmUsageMask[parmIndex];
  ++state.renderParmVersion;
  state.vertexParmVersion += usage & 1u;
  state.fragmentParmVersion += usage & 2u;
  state.textureParmVersion += usage & 4u;
  state.samplerParmVersion += usage & 8u;
}

void CopyVector(parmValue_t& destination, const idVec4& source) {
  std::memcpy(&destination, &source, sizeof(destination));
}

}  // namespace

idParmState::idParmState() {
  numGlobalTables = 0;
  parmIndexPositionToViewTexture = kInvalidParmIndex;
  parmIndexRenderPositionToViewTexture = kInvalidParmIndex;
  parmIndexPositionToFeedback = kInvalidParmIndex;
  parmIndexFragmentPositionToWindow = kInvalidParmIndex;
  parmIndexModelMatrixX = kInvalidParmIndex;
  parmIndexModelMatrixY = kInvalidParmIndex;
  parmIndexModelMatrixZ = kInvalidParmIndex;
  parmIndexModelMatrixW = kInvalidParmIndex;
  parmIndexInverseModelMatrixX = kInvalidParmIndex;
  parmIndexInverseModelMatrixY = kInvalidParmIndex;
  parmIndexInverseModelMatrixZ = kInvalidParmIndex;
  parmIndexInverseModelMatrixW = kInvalidParmIndex;
  parmIndexMVPMatrixX = kInvalidParmIndex;
  parmIndexMVPMatrixY = kInvalidParmIndex;
  parmIndexMVPMatrixZ = kInvalidParmIndex;
  parmIndexMVPMatrixW = kInvalidParmIndex;
  parmIndexPrimeLightDir = kInvalidParmIndex;
  parmIndexPrimeLightColor = kInvalidParmIndex;
  std::fill(std::begin(parmIndexGlobalLightOrg),
            std::end(parmIndexGlobalLightOrg), kInvalidParmIndex);
  std::fill(std::begin(parmIndexLightColor), std::end(parmIndexLightColor),
            kInvalidParmIndex);
  std::fill(std::begin(parmIndexChannelLight),
            std::end(parmIndexChannelLight), kInvalidParmIndex);
  parmIndexLightingModel = kInvalidParmIndex;
  parmIndexHighlightColor = kInvalidParmIndex;
  parmIndexFacingCorrection = kInvalidParmIndex;
  parmIndexSrcTexClamp = kInvalidParmIndex;
  parmIndexVertexStScaleBias = kInvalidParmIndex;
  parmIndexVertexXYZScale = kInvalidParmIndex;
  parmIndexVertexXYZBias = kInvalidParmIndex;
  parmIndexVertexMorphScale = kInvalidParmIndex;
  parmIndexIf = kInvalidParmIndex;
  parmIndexPolygonOffset = kInvalidParmIndex;
  std::memset(parmDecl, 0, sizeof(parmDecl));
  std::memset(parmUsageMask, 0, sizeof(parmUsageMask));
  std::memset(globalTables, 0, sizeof(globalTables));
  std::memset(globalLookupTables, 0, sizeof(globalLookupTables));
}

void idParmState::CopyParmUnaligned(parmValue_t& destination,
                                    const parmValue_t& source) {
  std::memcpy(&destination, &source, sizeof(destination));
}

void idParmState::VirtualRollbackParmBlock(
    const idParmBlock&, const idParmBlock& parmBlock) {
  if (parmStack.virtualParmBlockLevel == 0)
    std::abort();
  const unsigned int level = --parmStack.virtualParmBlockLevel;
  parmStack.virtualParmBlockStackMask |= 1u << level;
  if (parmStack.parmBlockStack[level].parmBlock != &parmBlock)
    std::abort();
}

const idVec4& idParmState::GetParmVec4(
    const idDeclRenderParm* parameter) const {
  return reinterpret_cast<const idVec4&>(values[parameter->parmIndex]);
}

float idParmState::GetParmFloat(
    const idDeclRenderParm* parameter) const {
  return values[parameter->parmIndex].value[0];
}

const idImage* idParmState::GetParmImage(
    const idDeclRenderParm* parameter) const {
  return values[parameter->parmIndex].image;
}

void idParmState::ApplyOp(const expOp_t& operation,
                          const parmValue_t& constant,
                          const parmValue_t& parmA,
                          const parmValue_t& parmB,
                          parmValue_t& result) {
  const int mask = (operation.type >> 9) & 0x0F;
  const auto maskedWrite = [&](const float* source) {
    for (int component = 0; component < 4; ++component) {
      if ((mask & (1 << component)) != 0)
        result.value[component] = source[component];
    }
  };
  float temporary[4]{};
  switch (operation.type & 0x1F) {
    case OP_TYPE_UNMASKED_MOVE_CONSTANT:
      CopyParmUnaligned(result, constant);
      return;
    case OP_TYPE_UNMASKED_MOVE:
      CopyParmUnaligned(result, parmA);
      return;
    case OP_TYPE_MOVE_CONSTANT:
      maskedWrite(constant.value);
      return;
    case OP_TYPE_MOVE:
      maskedWrite(parmA.value);
      return;
    case OP_TYPE_SWIZZLE:
      for (int component = 0; component < 4; ++component) {
        const int source =
            (constant.swizzle[component] + 4 - component) & 3;
        temporary[component] = parmA.value[source];
      }
      maskedWrite(temporary);
      return;
    case OP_TYPE_ADD:
      for (int i = 0; i < 4; ++i)
        temporary[i] = parmA.value[i] + parmB.value[i];
      break;
    case OP_TYPE_SUBTRACT:
      for (int i = 0; i < 4; ++i)
        temporary[i] = parmA.value[i] - parmB.value[i];
      break;
    case OP_TYPE_MULTIPLY:
      for (int i = 0; i < 4; ++i)
        temporary[i] = parmA.value[i] * parmB.value[i];
      break;
    case OP_TYPE_DOT3: {
      const float firstPair = parmA.value[0] * parmB.value[0] +
                              parmA.value[1] * parmB.value[1];
      const float secondPair = parmA.value[2] * parmB.value[2] + 0.0f;
      const float dot = firstPair + secondPair;
      std::fill(std::begin(temporary), std::end(temporary), dot);
      break;
    }
    case OP_TYPE_DOT4: {
      const float firstPair = parmA.value[0] * parmB.value[0] +
                              parmA.value[1] * parmB.value[1];
      const float secondPair = parmA.value[2] * parmB.value[2] +
                               parmA.value[3] * parmB.value[3];
      const float dot = firstPair + secondPair;
      std::fill(std::begin(temporary), std::end(temporary), dot);
      break;
    }
    case OP_TYPE_DIVIDE:
      for (int i = 0; i < 4; ++i)
        temporary[i] = parmA.value[i] / parmB.value[i];
      break;
    case OP_TYPE_MOD: {
      const __m128 quotient =
          _mm_div_ps(_mm_loadu_ps(parmA.value),
                     _mm_loadu_ps(parmB.value));
      _mm_storeu_ps(
          temporary,
          _mm_sub_ps(quotient,
                     _mm_cvtepi32_ps(_mm_cvttps_epi32(quotient))));
      break;
    }
    case OP_TYPE_GT:
      for (int i = 0; i < 4; ++i)
        temporary[i] = parmA.value[i] > parmB.value[i] ? 1.0f : 0.0f;
      break;
    case OP_TYPE_GE:
      for (int i = 0; i < 4; ++i)
        temporary[i] = parmA.value[i] >= parmB.value[i] ? 1.0f : 0.0f;
      break;
    case OP_TYPE_LT:
    case OP_TYPE_LE:
      for (int i = 0; i < 4; ++i)
        temporary[i] = parmA.value[i] < parmB.value[i] ? 1.0f : 0.0f;
      break;
    case OP_TYPE_EQ:
      for (int i = 0; i < 4; ++i)
        temporary[i] = parmA.value[i] == parmB.value[i] ? 1.0f : 0.0f;
      break;
    case OP_TYPE_NE:
      for (int i = 0; i < 4; ++i)
        temporary[i] = parmA.value[i] != parmB.value[i] ? 1.0f : 0.0f;
      break;
    case OP_TYPE_AND:
      for (int i = 0; i < 4; ++i) {
        temporary[i] =
            parmA.value[i] != 0.0f && parmB.value[i] != 0.0f ? 1.0f
                                                               : 0.0f;
      }
      break;
    case OP_TYPE_OR:
      for (int i = 0; i < 4; ++i) {
        temporary[i] =
            parmA.value[i] != 0.0f || parmB.value[i] != 0.0f ? 1.0f
                                                               : 0.0f;
      }
      break;
    case OP_TYPE_TABLE: {
      const float value =
          renderThreadParmState
              ->globalLookupTables[operation.parmIndexA]
              ->TableLookup(parmB.value[0], true);
      std::fill(std::begin(temporary), std::end(temporary), value);
      break;
    }
    default:
      std::abort();
  }
  maskedWrite(temporary);
}

void idParmState::EvaluateParmBlock(const idParmBlock& localParmBlock,
                                    const idParmBlock& parmBlock) {
  const unsigned int level = parmStack.currentParmBlockLevel;
  if (level >= 32 ||
      parmStack.currentOpLevel + localParmBlock.ops.num > 1024) {
    std::abort();
  }
  ++c_opsBlocks;
  c_ops += localParmBlock.ops.num;
  parmStack.parmBlockStack[level].parmBlock = &parmBlock;
  parmStack.parmBlockStack[level].lastOpLevel = parmStack.currentOpLevel;
  const unsigned int levelBit = 1u << level;
  unsigned char usage = 0;
  for (int index = 0; index < localParmBlock.ops.num; ++index) {
    const expOp_t& operation = localParmBlock.ops.list[index];
    const int destination = operation.parmIndexDest;
    usage |= parmUsageMask[destination];
    CopyParmUnaligned(parmStack.opStack[parmStack.currentOpLevel + index],
                      values[destination]);
    ApplyOp(operation, localParmBlock.constants.list[index],
            values[operation.parmIndexA], values[operation.parmIndexB],
            values[destination]);
    if (operation.parmIndexA < kInvalidParmIndex)
      onParmBlockStack[operation.parmIndexA] |= levelBit;
    if (operation.parmIndexB < kInvalidParmIndex)
      onParmBlockStack[operation.parmIndexB] |= levelBit;
    onParmBlockStack[destination] |= levelBit;
  }
  vertexParmVersion += usage & 1u;
  fragmentParmVersion += usage & 2u;
  textureParmVersion += usage & 4u;
  samplerParmVersion += usage & 8u;
  ++parmStack.currentParmBlockLevel;
  parmStack.currentOpLevel += localParmBlock.ops.num;
}

void idParmState::RollbackParmBlock(const idParmBlock& localParmBlock,
                                    const idParmBlock& parmBlock) {
  if (parmStack.currentParmBlockLevel == 0)
    std::abort();
  const unsigned int level = --parmStack.currentParmBlockLevel;
  if (parmStack.parmBlockStack[level].parmBlock != &parmBlock)
    std::abort();
  const unsigned int lastOpLevel =
      parmStack.parmBlockStack[level].lastOpLevel;
  const unsigned int clearMask = ~(1u << level);
  unsigned char usage = 0;
  for (int index = localParmBlock.ops.num - 1; index >= 0; --index) {
    const expOp_t& operation = localParmBlock.ops.list[index];
    usage |= parmUsageMask[operation.parmIndexDest];
    CopyParmUnaligned(values[operation.parmIndexDest],
                      parmStack.opStack[lastOpLevel + index]);
    if (operation.parmIndexA < kInvalidParmIndex)
      onParmBlockStack[operation.parmIndexA] &= clearMask;
    if (operation.parmIndexB < kInvalidParmIndex)
      onParmBlockStack[operation.parmIndexB] &= clearMask;
    onParmBlockStack[operation.parmIndexDest] &= clearMask;
  }
  vertexParmVersion += usage & 1u;
  fragmentParmVersion += usage & 2u;
  textureParmVersion += usage & 4u;
  samplerParmVersion += usage & 8u;
  parmStack.currentOpLevel = lastOpLevel;
}

void idParmState::VirtualEvaluateParmBlock(
    const idParmBlock& localParmBlock, const idParmBlock& parmBlock) {
  if (parmStack.currentParmBlockLevel > parmStack.virtualParmBlockLevel) {
    const unsigned int virtualLevel = parmStack.virtualParmBlockLevel;
    if (parmStack.parmBlockStack[virtualLevel].parmBlock == &parmBlock) {
      parmStack.virtualParmBlockStackMask &= ~(1u << virtualLevel);
      ++parmStack.virtualParmBlockLevel;
      return;
    }
    while (parmStack.currentParmBlockLevel >
           parmStack.virtualParmBlockLevel) {
      const idParmBlock* block =
          parmStack
              .parmBlockStack[parmStack.currentParmBlockLevel - 1]
              .parmBlock;
      RollbackParmBlock(*block, *block);
    }
    parmStack.virtualParmBlockStackMask = 0;
  }
  EvaluateParmBlock(localParmBlock, parmBlock);
  ++parmStack.virtualParmBlockLevel;
}

void idParmState::RollbackVirtualStack(bool) {
  while (parmStack.currentParmBlockLevel > parmStack.virtualParmBlockLevel) {
    const idParmBlock* block =
        parmStack.parmBlockStack[parmStack.currentParmBlockLevel - 1]
            .parmBlock;
    RollbackParmBlock(*block, *block);
  }
  parmStack.virtualParmBlockStackMask = 0;
}

void idParmState::RollbackOpsToZero() {
  RollbackVirtualStack(false);
  if (parmStack.currentParmBlockLevel != 0) {
    ++renderParmVersion;
    ++vertexParmVersion;
    fragmentParmVersion += 2;
    textureParmVersion += 4;
    samplerParmVersion += 8;
    parmStack.virtualParmBlockLevel = 0;
    parmStack.currentParmBlockLevel = 0;
    parmStack.currentOpLevel = 0;
    currentMVPHasNegativeDeterminant = false;
  }
}

void idParmState::SetParmValue(const idDeclRenderParm* parameter,
                               const parmValue_t& value) {
  const int index = parameter->parmIndex;
  if ((onParmBlockStack[index] & parmStack.virtualParmBlockStackMask) != 0)
    RollbackVirtualStack(true);
  CopyParmUnaligned(values[index], value);
  TouchExplicitParm(*this, index);
}

void idParmState::SetParmFloat(const idDeclRenderParm* parameter,
                               float value) {
  const int index = parameter->parmIndex;
  if ((onParmBlockStack[index] & parmStack.virtualParmBlockStackMask) != 0)
    RollbackVirtualStack(true);
  std::fill(std::begin(values[index].value), std::end(values[index].value),
            value);
  TouchExplicitParm(*this, index);
}

void idParmState::SetParmVector(const idDeclRenderParm* parameter, float x,
                                float y, float z, float w) {
  const int index = parameter->parmIndex;
  if ((onParmBlockStack[index] & parmStack.virtualParmBlockStackMask) != 0)
    RollbackVirtualStack(true);
  values[index].value[0] = x;
  values[index].value[1] = y;
  values[index].value[2] = z;
  values[index].value[3] = w;
  TouchExplicitParm(*this, index);
}

void idParmState::SetParmVector(const idDeclRenderParm* parameter,
                                const float* value) {
  parmValue_t parmValue{};
  std::memcpy(parmValue.value, value, sizeof(parmValue.value));
  SetParmValue(parameter, parmValue);
}

void idParmState::SetParmVec3(const idDeclRenderParm* parameter,
                              const idVec3& value) {
  SetParmVector(parameter, value.x, value.y, value.z, 1.0f);
}

void idParmState::SetParmSampler(const idDeclRenderParm* parameter,
                                 const idTextureSampler* sampler) {
  const int index = parameter->parmIndex;
  if ((onParmBlockStack[index] & parmStack.virtualParmBlockStackMask) != 0)
    RollbackVirtualStack(true);
  values[index].sampler = sampler;
  TouchExplicitParm(*this, index);
}

void idParmState::SetSrcTexClamp(int validWidth, int validHeight,
                                 int textureWidth, int textureHeight) {
  const int index = parmIndexSrcTexClamp;
  if ((onParmBlockStack[index] & parmStack.virtualParmBlockStackMask) != 0)
    RollbackVirtualStack(true);
  values[index].value[0] = 0.0f;
  values[index].value[1] = 0.0f;
  values[index].value[2] =
      static_cast<float>(validWidth) / static_cast<float>(textureWidth) -
      0.5f / static_cast<float>(textureWidth);
  values[index].value[3] =
      static_cast<float>(validHeight) / static_cast<float>(textureHeight) -
      0.5f / static_cast<float>(textureHeight);
  TouchExplicitParm(*this, index);
}

void idParmState::ResetSrcTexClamp() {
  const int index = parmIndexSrcTexClamp;
  if ((onParmBlockStack[index] & parmStack.virtualParmBlockStackMask) != 0)
    RollbackVirtualStack(true);
  values[index].value[0] = 0.0f;
  values[index].value[1] = 0.0f;
  values[index].value[2] = 1.0f;
  values[index].value[3] = 1.0f;
  TouchExplicitParm(*this, index);
}

void idParmState::SetFragmentPositionToWindow(int renderWidth,
                                               int renderHeight, int, int) {
  const int index = parmIndexFragmentPositionToWindow;
  if ((onParmBlockStack[index] & parmStack.virtualParmBlockStackMask) != 0)
    RollbackVirtualStack(true);
  values[index].value[0] = 0.0f;
  values[index].value[1] = 0.0f;
  values[index].value[2] = 1.0f / static_cast<float>(renderWidth);
  values[index].value[3] = 1.0f / static_cast<float>(renderHeight);
  TouchExplicitParm(*this, index);
}

void idParmState::SetFragmentPositionToViewTexture(
    int, int, int windowWidth, int windowHeight) {
  for (int index : {parmIndexPositionToViewTexture,
                    parmIndexRenderPositionToViewTexture}) {
    if ((onParmBlockStack[index] & parmStack.virtualParmBlockStackMask) != 0)
      RollbackVirtualStack(true);
    values[index].value[0] = 0.0f;
    values[index].value[1] = 0.0f;
    values[index].value[2] = 1.0f / static_cast<float>(windowWidth);
    values[index].value[3] = 1.0f / static_cast<float>(windowHeight);
    TouchExplicitParm(*this, index);
  }
}

void idParmState::SetFragmentPositionToFeedback(
    int renderWidth, int renderHeight, int feedbackWidth,
    int feedbackHeight) {
  const int index = parmIndexPositionToFeedback;
  if ((onParmBlockStack[index] & parmStack.virtualParmBlockStackMask) != 0)
    RollbackVirtualStack(true);
  values[index].value[0] = 0.0f;
  values[index].value[1] = 0.0f;
  values[index].value[2] =
      static_cast<float>(feedbackWidth) * 0.5f /
      static_cast<float>(renderWidth);
  values[index].value[3] =
      static_cast<float>(feedbackHeight) * 0.5f /
      static_cast<float>(renderHeight);
  TouchExplicitParm(*this, index);
}

void idParmState::SetHighlightColor(const idVec4& color) {
  const int index = parmIndexHighlightColor;
  if ((onParmBlockStack[index] & parmStack.virtualParmBlockStackMask) != 0)
    RollbackVirtualStack(true);
  values[index].value[0] = color.x;
  values[index].value[1] = color.y;
  values[index].value[2] = color.z;
  values[index].value[3] = 1.0f;
  TouchExplicitParm(*this, index);
}

void idParmState::SetApproximateLighting(
    const approximateLighting_t& lighting) {
  const int indices[] = {
      parmIndexLightingModel,
      parmIndexPrimeLightDir,
      parmIndexPrimeLightColor,
      parmIndexGlobalLightOrg[0],
      parmIndexGlobalLightOrg[1],
      parmIndexGlobalLightOrg[2],
      parmIndexLightColor[0],
      parmIndexLightColor[1],
      parmIndexLightColor[2],
      parmIndexChannelLight[0],
      parmIndexChannelLight[1],
      parmIndexChannelLight[2],
      parmIndexChannelLight[3],
      parmIndexChannelLight[4],
      parmIndexChannelLight[5],
  };
  unsigned int stackBits = 0;
  for (int index : indices)
    stackBits |= onParmBlockStack[index];
  if ((stackBits & parmStack.virtualParmBlockStackMask) != 0)
    RollbackVirtualStack(true);

  CopyVector(values[parmIndexLightingModel], lighting.lightingModel);
  CopyVector(values[parmIndexPrimeLightDir], lighting.primeLightDir);
  CopyVector(values[parmIndexPrimeLightColor], lighting.lightColor[0]);
  for (int index = 0; index < 3; ++index) {
    CopyVector(values[parmIndexGlobalLightOrg[index]],
               lighting.lightOrigin[index]);
    CopyVector(values[parmIndexLightColor[index]],
               lighting.lightColor[index]);
  }
  for (int index = 0; index < 6; ++index) {
    CopyVector(values[parmIndexChannelLight[index]],
               lighting.channelLighting[index]);
  }
  unsigned char usage = 0;
  for (int index : indices)
    usage |= parmUsageMask[index];
  ++renderParmVersion;
  vertexParmVersion += usage & 1u;
  fragmentParmVersion += usage & 2u;
  textureParmVersion += usage & 4u;
  samplerParmVersion += usage & 8u;
}
