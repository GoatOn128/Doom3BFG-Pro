#include "engine/renderer/declrenderprog.h"

#include "engine/framework/coderesource.h"
#include "engine/framework/resourcemanager.h"
#include "engine/renderer/declrenderprogattribs.h"
#include "engine/renderer/declrenderproggenerator.h"
#include "engine/renderer/jobs/render/graphicsapiwrapper.h"
#include "engine/renderer/jobs/render/parmstate.h"
#include "shared/idlib/csystems/cvarsystemlocal.h"
#include "shared/idlib/filesystem/filesystemlocal.h"
#include "shared/idlib/lib.h"
#include "shared/idlib/text/parser.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace {

constexpr std::uint32_t GENERATED_RENDERPROG_VERSION = 0xB6880012u;
constexpr int RENDERPROG_PARSER_FLAGS = 156216;

std::uint16_t ByteSwap16(std::uint16_t value) {
  return static_cast<std::uint16_t>((value << 8) | (value >> 8));
}

std::uint32_t ByteSwap32(std::uint32_t value) {
  return ((value & 0x000000FFu) << 24) |
         ((value & 0x0000FF00u) << 8) |
         ((value & 0x00FF0000u) >> 8) |
         ((value & 0xFF000000u) >> 24);
}

std::uint64_t ByteSwap64(std::uint64_t value) {
  return (static_cast<std::uint64_t>(ByteSwap32(
              static_cast<std::uint32_t>(value)))
          << 32) |
         ByteSwap32(static_cast<std::uint32_t>(value >> 32));
}

bool ReadExact(idFile* file, void* destination, std::size_t size) {
  return file != nullptr && file->Read(destination, size) == size;
}

void WriteBig16(idFile* file, std::uint16_t value) {
  value = ByteSwap16(value);
  file->Write(&value, sizeof(value));
}

void WriteBig32(idFile* file, std::uint32_t value) {
  value = ByteSwap32(value);
  file->Write(&value, sizeof(value));
}

void WriteBig64(idFile* file, std::uint64_t value) {
  value = ByteSwap64(value);
  file->Write(&value, sizeof(value));
}

bool ReadBig16(idFile* file, std::uint16_t& value) {
  if (!ReadExact(file, &value, sizeof(value)))
    return false;
  value = ByteSwap16(value);
  return true;
}

bool ReadBig32(idFile* file, std::uint32_t& value) {
  if (!ReadExact(file, &value, sizeof(value)))
    return false;
  value = ByteSwap32(value);
  return true;
}

bool ReadBig64(idFile* file, std::uint64_t& value) {
  if (!ReadExact(file, &value, sizeof(value)))
    return false;
  value = ByteSwap64(value);
  return true;
}

bool ReadStoredString(idFile* file, idStr& text) {
  int length = 0;
  if (!ReadExact(file, &length, sizeof(length)) || length < 0)
    return false;
  if (length == 0) {
    text.Empty();
    return true;
  }
  text.Fill(' ', length);
  return ReadExact(file, &text[0], static_cast<std::size_t>(length));
}

const idDeclRenderProg* FindRenderProgNoLoad(const char* name) {
  char remapped[512]{};
  const char* mapped = idDeclRenderProg::resourceList.RemapResourceName(
      remapped, sizeof(remapped), name);
  return static_cast<const idDeclRenderProg*>(
      idDeclRenderProg::resourceList.Load(mapped, false, false));
}

class ResourcePlatformScope {
public:
  ResourcePlatformScope() : previous(0), active(resourceManager != nullptr) {
    if (active) {
      previous = resourceManager->GetResourceFilePlatform();
      resourceManager->SetResourceFilePlatform(31);
    }
  }
  ~ResourcePlatformScope() {
    if (active)
      resourceManager->SetResourceFilePlatform(previous);
  }

private:
  unsigned int previous;
  bool active;
};

}  // namespace

idCVar r_useUniformArrays(
    "r_useUniformArrays", "1", 16386,
    "0: use individual uniforms, 1: use uniform array", 0.0f, 2.0f);
idCVar r_validateRenderProgs(
    "r_validateRenderProgs", "1", 2,
    "if > 0 validates 360 shaders and points out potential shader patch issues");
idCVar r_useGeneratedRenderprogs(
    "r_useGeneratedRenderprogs", "1", 1,
    "Use the pre-translated renderprog files for faster loads");
idCVar r_renderProgProdOverridePath(
    "r_renderProgProdOverridePath", "", 0,
    "load renderprogs from this path instead of from resource file in production");

extern idCVar r_feedbackBGRA;
idCodeResource<idDeclRenderParm> rpStageSort("stageSort");

const char* programTargetNameString[PT_NUM_TARGETS] = {
    "PC", "PC_D3D", "360", "DURANGO", "PS3", "ORBIS"};

idDeclInfoTemplate<idDeclRenderProg> idDeclRenderProg::resourceList(
    "renderProg", "idDeclRenderProg", "renderprog", false);

renderProgData_t::renderProgData_t()
    : inheritsFrom(), compoundCheckSum(0), parentCheckSum(0), dependencies(),
      dependencyTimestamps(), glState(0), parmBlock(), vertexParms(),
      fragmentParms(), textureParms(), samplerParms(), flags(0), stageSort(0.0f),
      hasInteractions(false), hasAlphaToCoverage(false),
      has16BitScaleBias(false), hasHardwareSkinning(false),
      hasVertexTexture(false), hasClip(false), hasDepthOutput(false),
      hasBranchStripping(false), vertexMask(0), fragmentOutputs(0),
      registerCountPS3(0), registerCount360(0), vertexCode(), fragmentCode() {
  Clear();
}

renderProgData_t::~renderProgData_t() = default;

renderProgData_t& renderProgData_t::operator=(const renderProgData_t& other) {
  if (this == &other)
    return *this;
  inheritsFrom = other.inheritsFrom;
  compoundCheckSum = other.compoundCheckSum;
  parentCheckSum = other.parentCheckSum;
  dependencies = other.dependencies;
  dependencyTimestamps = other.dependencyTimestamps;
  glState = other.glState;
  parmBlock.CopyFrom(other.parmBlock);
  vertexParms = other.vertexParms;
  fragmentParms = other.fragmentParms;
  textureParms = other.textureParms;
  samplerParms = other.samplerParms;
  flags = other.flags;
  stageSort = other.stageSort;
  hasInteractions = other.hasInteractions;
  hasAlphaToCoverage = other.hasAlphaToCoverage;
  has16BitScaleBias = other.has16BitScaleBias;
  hasHardwareSkinning = other.hasHardwareSkinning;
  hasVertexTexture = other.hasVertexTexture;
  hasClip = other.hasClip;
  hasDepthOutput = other.hasDepthOutput;
  hasBranchStripping = other.hasBranchStripping;
  vertexMask = other.vertexMask;
  fragmentOutputs = other.fragmentOutputs;
  registerCountPS3 = other.registerCountPS3;
  registerCount360 = other.registerCount360;
  vertexCode = other.vertexCode;
  fragmentCode = other.fragmentCode;
  return *this;
}

void renderProgData_t::Clear() {
  compoundCheckSum = 0;
  parentCheckSum = 0;
  glState = 0;
  parmBlock.Clear();
  flags = 0;
  stageSort = 0.0f;
  hasInteractions = false;
  hasAlphaToCoverage = false;
  has16BitScaleBias = false;
  hasHardwareSkinning = false;
  hasVertexTexture = false;
  hasClip = false;
  hasDepthOutput = false;
  hasBranchStripping = false;
  vertexMask = 0;
  fragmentOutputs = 0;
  registerCountPS3 = 0;
  registerCount360 = 0;
  dependencies.ClearFree();
  std::memset(vertexParms.list, 0,
              static_cast<std::size_t>(vertexParms.size) * sizeof(short));
  std::memset(fragmentParms.list, 0,
              static_cast<std::size_t>(fragmentParms.size) * sizeof(short));
  std::memset(textureParms.list, 0,
              static_cast<std::size_t>(textureParms.size) * sizeof(short));
  std::memset(samplerParms.list, 0,
              static_cast<std::size_t>(samplerParms.size) * sizeof(short));
}

shaderConstructInfo_t::shaderConstructInfo_t()
    : parsed(false), headerString(), sourceString(),
      outputAttribInitString(), inAttribs(), outAttribs() {}

shaderConstructInfo_t::~shaderConstructInfo_t() = default;

const char* idDeclRenderProg::DefaultDefinition() const {
  return "{\tstate { depthFunc GL_LEQUAL }\n"
         "\thlsl_vp {\n"
         "\t\tresult.position.x = dot4( vertex.position, $mvpMatrixX );\n"
         "\t\tresult.position.y = dot4( vertex.position, $mvpMatrixY );\n"
         "\t\tresult.position.z = dot4( vertex.position, $mvpMatrixZ );\n"
         "\t\tresult.position.w = dot4( vertex.position, $mvpMatrixW );\n"
         "\t}\n"
         "\thlsl_fp {\n"
         "\t\tresult.color = float4( 0.2, 1.0, 0.4, 1.0 );\n"
         "\t}\n"
         "}\n";
}

void idDeclRenderProg::BindTexture(int textureUnit, const idImage* image,
                                   const idTextureSampler* sampler) {
  GL_BindTexture(textureUnit, image, sampler);
  GL_ResetTextureState();
}

idStr RenderProgGeneratedFileName(const char* progName,
                                  const progTarget_t target) {
  idStr result("generated/renderprogs/");
  result.Append(progName != nullptr ? progName : "");
  result.Append('_');
  result.Append(target >= PT_PC && target < PT_NUM_TARGETS
                    ? programTargetNameString[target]
                    : "unknown");
  result.Append(".bin");
  return result;
}

idFile* idDeclRenderProg::OpenFileReadWithOverride(const char* filename) {
  if (fileSystem == nullptr)
    return nullptr;
  if (r_renderProgProdOverridePath.valueString.IsEmpty())
    return fileSystem->OpenFileRead(filename);
  idStr name(r_renderProgProdOverridePath.valueString);
  name.AppendPath(filename);
  return fileSystem->OpenFileRead(name.data);
}

bool idDeclRenderProg::ReloadIfStale() {
  if (!loadedResource || !partiallyLoaded)
    return false;
  idLib::Printf("Reloading partially loaded renderprog %s\n", name.str);
  return true;
}

void idDeclRenderProg::Print() {
  const struct {
    const char* heading;
    const idList<short>* parms;
  } groups[] = {{"--- vertex uniforms ---\n", &data.vertexParms},
                {"--- fragment uniforms ---\n", &data.fragmentParms},
                {"--- textures ---\n", &data.textureParms},
                {"--- samplers ---\n", &data.samplerParms}};
  for (const auto& group : groups) {
    idLib::Printf("%s", group.heading);
    for (int index = 0; index < group.parms->num; ++index) {
      const idDeclRenderParm* parm =
          renderThreadParmState->GetRenderParmForIndex((*group.parms)[index]);
      const_cast<idDeclRenderParm*>(parm)->List();
    }
  }
}

void idDeclRenderProg::List() const {
  idLib::Printf("%2i %2i %2i %s\n", data.vertexParms.num,
                data.fragmentParms.num, data.textureParms.num, name.str);
}

bool idDeclRenderProg::ProgUsesParm(const idDeclRenderParm* parm) const {
  if (parm == nullptr)
    return false;
  const idList<short>* lists[] = {&data.vertexParms, &data.fragmentParms,
                                  &data.textureParms, &data.samplerParms};
  for (const idList<short>* list : lists) {
    for (int index = 0; index < list->num; ++index) {
      if ((*list)[index] == parm->parmIndex)
        return true;
    }
  }
  return false;
}

void WriteParmList(idFile* file, const idList<short>* parms) {
  WriteBig32(file, static_cast<std::uint32_t>(parms->num));
  for (int index = 0; index < parms->num; ++index) {
    const char* name =
        renderThreadParmState->GetParmNameForIndex((*parms)[index]);
    const int length = name != nullptr ? idStr::Length(name) : 0;
    file->Write(&length, sizeof(length));
    if (length > 0)
      file->Write(name, static_cast<std::size_t>(length));
  }
}

void renderProgData_t::Write(idFile* file) const {
  file->WriteString(inheritsFrom.data);
  WriteBig64(file, glState);
  parmBlock.Save(file);
  WriteParmList(file, &vertexParms);
  WriteParmList(file, &fragmentParms);
  WriteParmList(file, &textureParms);
  WriteParmList(file, &samplerParms);
  WriteBig32(file, compoundCheckSum);
  WriteBig32(file, parentCheckSum);
  WriteBig32(file, static_cast<std::uint32_t>(dependencies.num));
  for (int index = 0; index < dependencies.num; ++index) {
    file->WriteString(dependencies[index].data);
    WriteBig32(file, dependencyTimestamps[index]);
  }
  std::uint32_t stageBits = 0;
  std::memcpy(&stageBits, &stageSort, sizeof(stageBits));
  WriteBig32(file, stageBits);
  WriteBig32(file, static_cast<std::uint32_t>(flags));
  const bool values[] = {hasInteractions,    hasAlphaToCoverage,
                         has16BitScaleBias,  hasHardwareSkinning,
                         hasVertexTexture,   hasClip,
                         hasDepthOutput,     hasBranchStripping};
  for (const bool value : values)
    file->Write(&value, sizeof(value));
  WriteBig32(file, vertexMask);
  WriteBig16(file, fragmentOutputs);
  file->Write(&registerCountPS3, sizeof(registerCountPS3));
  file->Write(&registerCount360, sizeof(registerCount360));
  file->WriteString(vertexCode.data);
  file->WriteString(fragmentCode.data);
}

void SaveGeneratedFile(const char* filename, renderProgData_t* data,
                       const unsigned int checksum) {
  if (fileSystem == nullptr)
    return;
  idFileLocal file(fileSystem->OpenFileWrite(filename));
  if (file.file == nullptr)
    return;
  WriteBig32(file.file, GENERATED_RENDERPROG_VERSION);
  WriteBig32(file.file, checksum);
  data->Write(file.file);
}

bool ReadParmList(idFile* file, idList<short>* parms) {
  std::uint32_t storedCount = 0;
  if (!ReadBig32(file, storedCount) ||
      storedCount > static_cast<std::uint32_t>(parms->size))
    return false;
  parms->num = static_cast<int>(storedCount);
  for (int index = 0; index < parms->num; ++index) {
    idStr renderParmName;
    if (!ReadStoredString(file, renderParmName))
      return false;
    const idDeclRenderParm* parm = static_cast<const idDeclRenderParm*>(
        idDeclRenderParm::resourceList.FindWithInheritance(
            renderParmName.data, false, true));
    if (parm == nullptr) {
      idLib::Warning("render parm '%s' not found for program",
                     renderParmName.data);
      return false;
    }
    (*parms)[index] = static_cast<short>(parm->parmIndex);
  }
  return true;
}

bool renderProgData_t::Read(idFile* file) {
  if (!ReadStoredString(file, inheritsFrom) || !ReadBig64(file, glState) ||
      !parmBlock.Load(file) || !ReadParmList(file, &vertexParms) ||
      !ReadParmList(file, &fragmentParms) ||
      !ReadParmList(file, &textureParms) ||
      !ReadParmList(file, &samplerParms) ||
      !ReadBig32(file, compoundCheckSum) ||
      !ReadBig32(file, parentCheckSum))
    return false;

  std::uint32_t dependencyCount = 0;
  if (!ReadBig32(file, dependencyCount))
    return false;
  dependencies.SetNum(static_cast<int>(dependencyCount));
  dependencyTimestamps.SetNum(static_cast<int>(dependencyCount));
  for (int index = 0; index < static_cast<int>(dependencyCount); ++index) {
    if (!ReadStoredString(file, dependencies[index]) ||
        !ReadBig32(file, dependencyTimestamps[index]))
      return false;
  }

  std::uint32_t stageBits = 0;
  std::uint32_t storedFlags = 0;
  if (!ReadBig32(file, stageBits) || !ReadBig32(file, storedFlags))
    return false;
  std::memcpy(&stageSort, &stageBits, sizeof(stageSort));
  flags = static_cast<int>(storedFlags);
  bool* values[] = {&hasInteractions,   &hasAlphaToCoverage,
                    &has16BitScaleBias, &hasHardwareSkinning,
                    &hasVertexTexture,  &hasClip,
                    &hasDepthOutput,    &hasBranchStripping};
  for (bool* value : values) {
    if (!ReadExact(file, value, sizeof(*value)))
      return false;
  }
  return ReadBig32(file, vertexMask) &&
         ReadBig16(file, fragmentOutputs) &&
         ReadExact(file, &registerCountPS3, sizeof(registerCountPS3)) &&
         ReadExact(file, &registerCount360, sizeof(registerCount360)) &&
         ReadStoredString(file, vertexCode) &&
         ReadStoredString(file, fragmentCode);
}

bool LoadGeneratedFile(const char* filename, renderProgData_t* data,
                       const unsigned int checksum) {
  idFileLocal file(idDeclRenderProg::OpenFileReadWithOverride(filename));
  if (file.file == nullptr)
    return false;
  std::uint32_t version = 0;
  std::uint32_t fileChecksum = 0;
  if (!ReadBig32(file.file, version) || version != GENERATED_RENDERPROG_VERSION ||
      !ReadBig32(file.file, fileChecksum))
    return false;
  if (fileChecksum != checksum &&
      (r_renderProgProdOverridePath.valueString.IsEmpty() ||
       idLib::production < PROD_PRODUCTION))
    return false;
  return data->Read(file.file);
}

idDeclInfo* idDeclRenderProg::GetDeclInfo() const { return &resourceList; }

void idDeclRenderProg::Clear() {
  renamed = this;
  data.Clear();
  inheritsFromDecl = nullptr;
  vertexProgramChecksum = 0;
  fragmentProgramChecksum = 0;
  partiallyLoaded = false;
  versions.ptr[0] = this;
  versions.ptr[1] = this;
  versions.ptr[2] = this;
  vertexProgram = nullptr;
  fragmentProgram = nullptr;
}

renderProgData_t idDeclRenderProg::GetRenderProgData(
    const renderProgParms_t* parms, shaderConstructInfo_t* vertInfo,
    shaderConstructInfo_t* fragInfo) {
  idParser source;
  source.SetFlags(RENDERPROG_PARSER_FLAGS);
  char* text = GetText(false);
  source.LoadMemory(text != nullptr ? text : "", textLength, GetFileName());
  source.SetSourceLine(GetLineNum());
  source.SkipUntilString("{");
  return TranslateRenderProg(parms, &source, md5Checksum, vertInfo, fragInfo);
}

idDeclRenderProg::idDeclRenderProg()
    : renamed(this), data(), inheritsFromDecl(nullptr),
      vertexProgramChecksum(0), fragmentProgramChecksum(0), hasDerived(false),
      partiallyLoaded(false), versions{}, vertexProgram(nullptr),
      fragmentProgram(nullptr) {
  Clear();
}

idDeclRenderProg::~idDeclRenderProg() noexcept { FreeData(); }

void idDeclRenderProg::Parse(idParser& source) {
  const renderProgParms_t parms{PT_PC,
                                r_useUniformArrays.valueInteger != 0,
                                false,
                                r_feedbackBGRA.valueInteger != 0,
                                true,
                                true};
  const unsigned int checksum = md5Checksum;
  const idStr generatedFileName =
      RenderProgGeneratedFileName(name.str, PT_PC);
  const idDeclRenderProg* inherited = nullptr;
  bool haveData = false;

  if (!(idLib::production < PROD_PRODUCTION &&
        r_useGeneratedRenderprogs.valueInteger == 0)) {
    haveData = LoadGeneratedFile(generatedFileName.data, &data, checksum);
    if (haveData && idLib::production < PROD_PRODUCTION) {
      for (int index = 0; index < data.dependencies.num; ++index) {
        if (fileSystem == nullptr ||
            data.dependencyTimestamps[index] !=
                fileSystem->GetTimestamp(data.dependencies[index].data)) {
          haveData = false;
          break;
        }
      }
    }
    if (haveData && !data.inheritsFrom.IsEmpty() &&
        !( !r_renderProgProdOverridePath.valueString.IsEmpty() &&
           idLib::production >= PROD_PRODUCTION)) {
      inherited = FindRenderProgNoLoad(data.inheritsFrom.data);
      if (inherited == nullptr ||
          inherited->data.compoundCheckSum != data.parentCheckSum) {
        inherited = nullptr;
        haveData = false;
      }
    }
  }

  if (!haveData) {
    if (idLib::production >= PROD_PRODUCTION) {
      idLib::Warning("Binary render prog '%s' failed.", name.str);
      return;
    }
    shaderConstructInfo_t vertInfo;
    shaderConstructInfo_t fragInfo;
    renderProgData_t translated =
        TranslateRenderProg(&parms, &source, checksum, &vertInfo, &fragInfo);
    data = translated;
    if (r_useGeneratedRenderprogs.valueInteger != 0)
      SaveGeneratedFile(generatedFileName.data, &data, checksum);
  }

  data.stageSort = std::floor(data.parmBlock.GetFloat(rpStageSort.Get()));
  if (declManager != nullptr) {
    for (int index = 0; index < data.dependencies.num; ++index)
      declManager->AddDependency(*this, data.dependencies[index].data);
  }
  renderThreadParmState->UpdateParmUsageMasks(this);
  if (!data.fragmentCode.IsEmpty() || !data.vertexCode.IsEmpty()) {
    if (idLib::IsMainThread())
      UploadTargetCode();
    else
      partiallyLoaded = true;
  }

  ResourcePlatformScope platformScope;
  if (data.hasInteractions) {
    idStr variant(name.str);
    variant.Append("_point");
    versions.ptr[0] = static_cast<const idDeclRenderProg*>(
        resourceList.FindWithInheritance(variant.data, true, true));
    variant = name.str;
    variant.Append("_spot");
    versions.ptr[1] = static_cast<const idDeclRenderProg*>(
        resourceList.FindWithInheritance(variant.data, true, true));
    variant = name.str;
    variant.Append("_parallel");
    versions.ptr[2] = static_cast<const idDeclRenderProg*>(
        resourceList.FindWithInheritance(variant.data, true, true));
  }

  if (!data.inheritsFrom.IsEmpty()) {
    if (inherited == nullptr)
      inherited = FindRenderProgNoLoad(data.inheritsFrom.data);
    inheritsFromDecl = inherited;
    if (inherited != nullptr) {
      const_cast<idDeclRenderProg*>(inherited)->hasDerived = true;
    } else {
      source.Warning("Unknown inherit RenderProg %s in %s",
                     data.inheritsFrom.data, name.str);
    }
  }

  if (!idLib::headless && idLib::IsMainThread() &&
      (vertexProgram == nullptr || fragmentProgram == nullptr))
    source.Warning("renderprog error");
}
