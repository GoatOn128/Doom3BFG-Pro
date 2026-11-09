#pragma once

#include "engine/decls/decl.h"
#include "engine/renderer/parmblock.h"
#include "shared/idlib/containers/staticlist.h"

#include <cstddef>
#include <cstdint>

class idImage;
class idTextureSampler;
class idFile;
struct shaderConstructInfo_t;

enum progTarget_t : int {
  PT_PC = 0,
  PT_PC_D3D = 1,
  PT_360 = 2,
  PT_DURANGO = 3,
  PT_PS3 = 4,
  PT_ORBIS = 5,
  PT_NUM_TARGETS = 6,
};

struct renderProgParms_t {
  progTarget_t target;
  bool useUniformArrays;
  bool useDeferredSlowmapLighting;
  bool feedbackBGRA;
  bool useNormalGBuffer;
  bool validateInOutParms360;
};

struct renderProgData_t {
  renderProgData_t();
  ~renderProgData_t();
  renderProgData_t& operator=(const renderProgData_t& other);
  void Clear();
  void Write(idFile* file) const;
  bool Read(idFile* file);

  idStr inheritsFrom;
  unsigned int compoundCheckSum;
  unsigned int parentCheckSum;
  idList<idStr> dependencies;
  idList<unsigned int> dependencyTimestamps;
  std::uint64_t glState;
  idParmBlock parmBlock;
  idStaticList<short, 42> vertexParms;
  idStaticList<short, 42> fragmentParms;
  idStaticList<short, 16> textureParms;
  idStaticList<short, 16> samplerParms;
  int flags;
  float stageSort;
  bool hasInteractions;
  bool hasAlphaToCoverage;
  bool has16BitScaleBias;
  bool hasHardwareSkinning;
  bool hasVertexTexture;
  bool hasClip;
  bool hasDepthOutput;
  bool hasBranchStripping;
  unsigned int vertexMask;
  std::uint16_t fragmentOutputs;
  std::uint8_t registerCountPS3;
  std::uint8_t registerCount360;
  idStr vertexCode;
  idStr fragmentCode;
};

template <typename T, std::size_t Count> struct idDeclFixedArray {
  T ptr[Count];
};

class idDeclRenderProg : public idDecl {
public:
  idDeclRenderProg();
  ~idDeclRenderProg() noexcept override;
  const char* DefaultDefinition() const override;
  bool ReloadIfStale() override;
  void Print() override;
  void List() const override;
  idDeclInfo* GetDeclInfo() const override;
  void Parse(idParser& source) override;
  void FreeData() override;

  void Clear();
  void UploadTargetCode();
  renderProgData_t GetRenderProgData(const renderProgParms_t* parms,
                                     shaderConstructInfo_t* vertInfo,
                                     shaderConstructInfo_t* fragInfo);
  static idFile* OpenFileReadWithOverride(const char* filename);

  static void BindTexture(int textureUnit, const idImage* image,
                          const idTextureSampler* sampler);
  bool ProgUsesParm(const idDeclRenderParm* parm) const;

  const idDeclRenderProg* renamed;
  renderProgData_t data;
  const idDeclRenderProg* inheritsFromDecl;
  unsigned int vertexProgramChecksum;
  unsigned int fragmentProgramChecksum;
  bool hasDerived;
  bool partiallyLoaded;
  std::uint8_t alignmentBeforeVersions[6];
  idDeclFixedArray<const idDeclRenderProg*, 3> versions;
  void* vertexProgram;
  void* fragmentProgram;
  idDeclFixedArray<void*, 45> vertexUniforms;
  idDeclFixedArray<void*, 42> fragmentUniforms;

  static idDeclInfoTemplate<idDeclRenderProg> resourceList;
};

unsigned int CompileGLSL(idStr* programGLSL, unsigned int* checksum,
                         bool isVertexProgram, const char* name,
                         const char* fileName);

idStr RenderProgGeneratedFileName(const char* progName, progTarget_t target);
void WriteParmList(idFile* file, const idList<short>* parms);
bool ReadParmList(idFile* file, idList<short>* parms);
void SaveGeneratedFile(const char* filename, renderProgData_t* data,
                       unsigned int checksum);
bool LoadGeneratedFile(const char* filename, renderProgData_t* data,
                       unsigned int checksum);

static_assert(sizeof(renderProgParms_t) == 12,
              "render-program translation parms ABI drift");

static_assert(offsetof(renderProgData_t, glState) == 0x60,
              "render-program GL-state offset drift");
static_assert(offsetof(renderProgData_t, parmBlock) == 0x68,
              "render-program parm-block offset drift");
static_assert(offsetof(renderProgData_t, vertexParms) == 0x98,
              "render-program vertex-parm offset drift");
static_assert(offsetof(renderProgData_t, samplerParms) == 0x1B0,
              "render-program sampler-parm offset drift");
static_assert(offsetof(renderProgData_t, flags) == 0x1E8,
              "render-program flags offset drift");
static_assert(offsetof(renderProgData_t, stageSort) == 0x1EC,
              "render-program stage-sort offset drift");
static_assert(sizeof(renderProgData_t) == 0x250,
              "render-program data ABI drift");
static_assert(offsetof(idDeclRenderProg, renamed) == 0x50,
              "render-program rename offset drift");
static_assert(offsetof(idDeclRenderProg, data) == 0x58,
              "render-program data offset drift");
static_assert(offsetof(idDeclRenderProg, versions) == 0x2C0,
              "render-program versions offset drift");
static_assert(sizeof(idDeclRenderProg) == 0x5A0,
              "render-program ABI drift");
