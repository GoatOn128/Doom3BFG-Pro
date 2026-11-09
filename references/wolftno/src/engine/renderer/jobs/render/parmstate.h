#pragma once

#include "engine/renderer/declrenderparm.h"

#include <cstddef>
#include <cstdint>

class idDeclRenderParm;
class idDeclRenderProg;
class idDeclTable;
class idImage;
class idLookupTable;
class idTextureSampler;
class idParmBlock;
struct approximateLighting_t;
class idVec3;
class idBounds;
struct idVec4;
struct idRenderMatrix;
struct expOp_t;
class idVertexScaleAndBias;

class alignas(16) idParmState {
public:
  idParmState();

  struct parmBlockStack_t {
    unsigned int lastOpLevel;
    const idParmBlock* parmBlock;
  };

  struct parmStack_t {
    unsigned int virtualParmBlockLevel;
    unsigned int currentParmBlockLevel;
    unsigned int currentOpLevel;
    unsigned int virtualParmBlockStackMask;
    parmBlockStack_t parmBlockStack[32];
    parmValue_t opStack[1024];
  };

  int AllocParmState(const idDeclRenderParm* parm);
  void FreeParmState(const idDeclRenderParm* parm);
  int GetTableIndex(const idDeclTable* table);
  const idDeclTable* GetTable(int tableIndex) const;
  void RemoveTable(const idDeclTable* table);
  const char* GetParmNameForIndex(int parmIndex) const;
  const idDeclRenderParm* GetRenderParmForIndex(int parmIndex) const;
  void DefaultRenderParms();
  void Init();
  void UpdateParmUsageMasks(const idDeclRenderProg* program);

  const idVec4& GetParmVec4(const idDeclRenderParm* parameter) const;
  float GetParmFloat(const idDeclRenderParm* parameter) const;
  const idImage* GetParmImage(const idDeclRenderParm* parameter) const;
  void EvaluateParmBlock(const idParmBlock& localParmBlock,
                         const idParmBlock& parmBlock);
  void RollbackParmBlock(const idParmBlock& localParmBlock,
                         const idParmBlock& parmBlock);
  void RollbackOpsToZero();
  void SetParmValue(const idDeclRenderParm* parameter,
                    const parmValue_t& value);
  void SetModelMatrix(const idRenderMatrix& matrix);
  void SetInverseModelMatrix(const idRenderMatrix& matrix);
  void SetMVPMatrix(const idRenderMatrix& matrix);
  void SetMVPMatrixFlipped(const idRenderMatrix& matrix);
  void SetMVPMatrixForBounds(const idRenderMatrix& matrix,
                             const idBounds& bounds);
  void SetMVPMatrixForInverseProject(
      const idRenderMatrix& matrix,
      const idRenderMatrix& inverseProject);
  void SetParmFloat(const idDeclRenderParm* parameter, float value);
  void SetParmSampler(const idDeclRenderParm* parameter,
                      const idTextureSampler* sampler);
  void SetParmVector(const idDeclRenderParm* parameter,
                     const float* value);
  void SetParmVector(const idDeclRenderParm* parameter, float x, float y,
                     float z, float w);
  void SetParmVec3(const idDeclRenderParm* parameter, const idVec3& value);
  void SetApproximateLighting(const approximateLighting_t& lighting);
  void SetHighlightColor(const idVec4& color);
  void SetFragmentPositionToFeedback(int renderWidth, int renderHeight,
                                     int feedbackWidth, int feedbackHeight);
  void SetFragmentPositionToWindow(int renderWidth, int renderHeight,
                                   int windowWidth, int windowHeight);
  void SetFragmentPositionToViewTexture(int renderWidth, int renderHeight,
                                        int windowWidth, int windowHeight);
  void SetSrcTexClamp(int validWidth, int validHeight, int textureWidth,
                      int textureHeight);
  void ResetSrcTexClamp();
  void Set16BitVertexScaleBias(const idVertexScaleAndBias& scaleAndBias);
  void VirtualEvaluateParmBlock(const idParmBlock& localParmBlock,
                                const idParmBlock& parmBlock);
  void VirtualRollbackParmBlock(const idParmBlock& localParmBlock,
                                const idParmBlock& parmBlock);
  void RollbackVirtualStack(bool renderParmChange);

  unsigned long long renderParmVersion;
  unsigned long long vertexParmVersion;
  unsigned long long fragmentParmVersion;
  unsigned long long textureParmVersion;
  unsigned long long samplerParmVersion;
  bool currentMVPHasNegativeDeterminant;
  parmValue_t values[2176];
  unsigned int onParmBlockStack[2176];
  parmStack_t parmStack;
  const idDeclRenderParm* parmDecl[2176];
  unsigned char parmUsageMask[2176];
  const idDeclTable* globalTables[1024];
  const idLookupTable* globalLookupTables[1024];
  int numGlobalTables;
  int parmIndexPositionToViewTexture;
  int parmIndexRenderPositionToViewTexture;
  int parmIndexPositionToFeedback;
  int parmIndexFragmentPositionToWindow;
  int parmIndexModelMatrixX;
  int parmIndexModelMatrixY;
  int parmIndexModelMatrixZ;
  int parmIndexModelMatrixW;
  int parmIndexInverseModelMatrixX;
  int parmIndexInverseModelMatrixY;
  int parmIndexInverseModelMatrixZ;
  int parmIndexInverseModelMatrixW;
  int parmIndexMVPMatrixX;
  int parmIndexMVPMatrixY;
  int parmIndexMVPMatrixZ;
  int parmIndexMVPMatrixW;
  int parmIndexPrimeLightDir;
  int parmIndexPrimeLightColor;
  int parmIndexGlobalLightOrg[3];
  int parmIndexLightColor[3];
  int parmIndexChannelLight[6];
  int parmIndexLightingModel;
  int parmIndexHighlightColor;
  int parmIndexFacingCorrection;
  int parmIndexSrcTexClamp;
  int parmIndexVertexStScaleBias;
  int parmIndexVertexXYZScale;
  int parmIndexVertexXYZBias;
  int parmIndexVertexMorphScale;
  int parmIndexIf;
  int parmIndexPolygonOffset;

private:
  friend class idParmBlock;

  static void CopyParmUnaligned(parmValue_t& destination,
                                const parmValue_t& source);
  static void ApplyOp(const expOp_t& operation,
                      const parmValue_t& constant,
                      const parmValue_t& parmA,
                      const parmValue_t& parmB,
                      parmValue_t& result);
};

extern idParmState* renderThreadParmState;
extern idParmState renderThreadParmStateObject;

static_assert(sizeof(idParmState::parmBlockStack_t) == 0x10,
              "parm-state block-stack entry ABI drift");
static_assert(sizeof(idParmState::parmStack_t) == 0x4210,
              "parm-state stack ABI drift");
static_assert(offsetof(idParmState, values) == 0x30,
              "parm-state values offset drift");
static_assert(offsetof(idParmState, onParmBlockStack) == 0x8830,
              "parm-state block mask offset drift");
static_assert(offsetof(idParmState, parmStack) == 0xAA30,
              "parm-state stack offset drift");
static_assert(offsetof(idParmState, parmUsageMask) == 0x13040,
              "parm-state usage-mask offset drift");
static_assert(offsetof(idParmState, parmIndexMVPMatrixX) == 0x178F4,
              "parm-state MVP index offset drift");
static_assert(sizeof(idParmState) == 0x17970,
              "parm-state ABI drift");
