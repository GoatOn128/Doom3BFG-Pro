#include "engine/models/particles/declparticle.h"

#include "engine/decls/decltable.h"
#include "engine/framework/coderesource.h"
#include "engine/models/particles/particleparmmisc.h"
#include "engine/models/particles/jobs/staticparticlemodeldata.h"
#include "engine/renderer/declmaterial.h"
#include "shared/idlib/filesystem/file.h"
#include "shared/idlib/filesystem/filesystemlocal.h"
#include "shared/idlib/containers/array.h"
#include "shared/idlib/lib.h"
#include "shared/idlib/csystems/cvarsystemlocal.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <new>

idCVar g_debugDuplicateStages(
    "g_debugDuplicateStages", "1", CVAR_INTEGER,
    "If 1 parse duplicates on save, if 2 print info about the duplicates it also");
idCVar g_disableParticleGeneratedBinaries(
    "g_disableParticleGeneratedBinaries", "0", CVAR_BOOL,
    "If true; will not generate and read binary data for particles");

class idDeclRenderParm;
static idCodeResource<idDeclRenderParm> rp_ParticleFade("particleFade");
static idCodeResource<idDeclRenderParm> rp_transAtlasScaleBias(
    "transAtlasScaleBias");
static idCodeResource<idDeclRenderParm> rp_TransMap("transMap");
static idCodeResource<idDeclRenderParm> rp_ViewDepthMap("viewDepthMap");

const particleParmDesc_t particleDistributionDesc[7] = {
    {"rect", 3, "sizeX sizeY sizeZ"},
    {"cylinder", 4, "sizeX sizeY sizeZ ringFraction"},
    {"sphere", 4, "sizeX sizeY sizeZ ringFraction"},
    {"rect_surface", 3, "sizeX sizeY sizeZ"},
    {"cylinder_surface", 4, "sizeX sizeY sizeZ ringFraction"},
    {"sphere_surface", 4, "sizeX sizeY sizeZ ringFraction"},
    {"mesh_surface", 0, ""},
};

const particleParmDesc_t particleDirectionDesc[3] = {
    {"cone", 4, "cone angle, rotation angles xyz"},
    {"outward", 1, "upward bias"},
    {"speed", 0, ""},
};

const particleParmDesc_t particleOrientationDesc[7] = {
    {"view", 0, ""},
    {"trail", 2, "numtrails, trailtime"},
    {"aimed", 3, "length, orientVelOnly, fade angle bias"},
    {"x", 0, ""},
    {"y", 0, ""},
    {"z", 0, ""},
    {"xyz", 0, ""},
};

const particleParmDesc_t particleCustomDesc[5] = {
    {"standard", 0, "Standard"},
    {"helix", 5, "sizeX sizeY sizeZ radialSpeed axialSpeed"},
    {"flies", 3, "radialSpeed axialSpeed size"},
    {"orbit", 2, "radius speed"},
    {"drip", 1, "speed"},
};

const particleParmDesc_t particleTexFlipTypeDesc[3] = {
    {"none", 0, "don't flip the texture coords"},
    {"random", 0, "randomly flip the texture coords"},
    {"always", 0, "always flip the texture coords"},
};

const particleParmDesc_t particleAnimationTypeDesc[3] = {
    {"cycle_rate", 0, "cycle animation at specific number of fps"},
    {"single_cycle_rate", 0,
     "single animation cycle at specific number of fps"},
    {"single_cycle", 0, "single cycle over the life of the particle"},
};

const particleParmDesc_t particleSortTypeDesc[3] = {
    {"none", 0, "no particle stage sorting"},
    {"newest_to_oldest", 0, "sort on particle life, newest to oldest"},
    {"oldest_to_newest", 0, "sort on particle life, oldest to newest"},
};

const particleParmDesc_t particleLightingDesc[3] = {
    {"off", 0, "no lighting"},
    {"average", 0, "average ambient light from all directions"},
    {"full", 0, "ambient and prime lighting using a normal map"},
};

namespace {

template <typename T>
bool WriteParticleString(idFile* file, const char* name, const char* specifier,
                         const T& value, const T& defaultValue) {
  if (value == defaultValue)
    return false;
  idStr paddedName(name);
  paddedName.PadLength(25);
  idStr format;
  format.Format("\t\t%s\t%s\n", paddedName.data, specifier);
  file->WriteFloatString(format.data, value);
  return true;
}

void ParseStageParm(idParser& source, idParticleParm* parm,
                    idList<const idDeclTable*, 79>& tableDecls,
                    idList<idLookupTable, 79>& tables) {
  ParseParticleParm(
      source, parm,
      reinterpret_cast<idList<const idDeclTable*, 5>&>(tableDecls),
      reinterpret_cast<idList<idLookupTable, 5>&>(tables));
}

template <std::size_t Count>
int ParticleNameToType(const idStr& text,
                       const particleParmDesc_t (&descriptions)[Count]) {
  for (std::size_t index = 0; index < Count; ++index) {
    if (text.Icmp(descriptions[index].name) == 0)
      return static_cast<int>(index);
  }
  return -1;
}

std::uint16_t ByteSwap16(std::uint16_t value) {
  return static_cast<std::uint16_t>((value << 8) | (value >> 8));
}

std::uint32_t ByteSwap32(std::uint32_t value) {
  return (value << 24) | ((value << 8) & 0x00ff0000u) |
         ((value >> 8) & 0x0000ff00u) | (value >> 24);
}

void ReadBig32(idFile* file, void* destination) {
  std::uint32_t value = 0;
  file->Read(&value, sizeof(value));
  value = ByteSwap32(value);
  std::memcpy(destination, &value, sizeof(value));
}

void ReadBig16(idFile* file, void* destination) {
  std::uint16_t value = 0;
  file->Read(&value, sizeof(value));
  value = ByteSwap16(value);
  std::memcpy(destination, &value, sizeof(value));
}

void WriteBig32(idFile* file, const void* source) {
  std::uint32_t value;
  std::memcpy(&value, source, sizeof(value));
  value = ByteSwap32(value);
  file->Write(&value, sizeof(value));
}

void WriteBig16(idFile* file, const void* source) {
  std::uint16_t value;
  std::memcpy(&value, source, sizeof(value));
  value = ByteSwap16(value);
  file->Write(&value, sizeof(value));
}

void ReadBinParticleParm(idFile* file, idParticleParm& parm) {
  ReadBig32(file, &parm.val0);
  ReadBig32(file, &parm.val1);
  ReadBig32(file, &parm.variance);
  ReadBig16(file, &parm.tableIdx);
  ReadBig16(file, &parm.table2Idx);
  ReadBig32(file, &parm.calcType);
}

void ReadBinParticleParmArray(idFile* file, idParticleParm* parms, int count) {
  for (int index = 0; index < count; ++index)
    ReadBinParticleParm(file, parms[index]);
}

void WriteBinParticleParm(idFile* file, const idParticleParm& parm) {
  WriteBig32(file, &parm.val0);
  WriteBig32(file, &parm.val1);
  WriteBig32(file, &parm.variance);
  WriteBig16(file, &parm.tableIdx);
  WriteBig16(file, &parm.table2Idx);
  WriteBig32(file, &parm.calcType);
}

void WriteBinParticleParmArray(idFile* file, const idParticleParm* parms,
                               int count) {
  for (int index = 0; index < count; ++index)
    WriteBinParticleParm(file, parms[index]);
}

template <typename T>
void ReadBigValue(idFile* file, T& value) {
  static_assert(sizeof(T) == 4);
  ReadBig32(file, &value);
}

template <typename T>
void WriteBigValue(idFile* file, const T& value) {
  static_assert(sizeof(T) == 4);
  WriteBig32(file, &value);
}

template <typename T>
void ReadBigShort(idFile* file, T& value) {
  static_assert(sizeof(T) == 2);
  ReadBig16(file, &value);
}

template <typename T>
void WriteBigShort(idFile* file, const T& value) {
  static_assert(sizeof(T) == 2);
  WriteBig16(file, &value);
}

void ReadBigFloats(idFile* file, float* values, int count) {
  for (int index = 0; index < count; ++index)
    ReadBigValue(file, values[index]);
}

void WriteBigFloats(idFile* file, const float* values, int count) {
  for (int index = 0; index < count; ++index)
    WriteBigValue(file, values[index]);
}

void ReadBigBounds(idFile* file, idBounds& value) {
  ReadBigFloats(file, &value[0].x, 6);
}

void WriteBigBounds(idFile* file, const idBounds& value) {
  WriteBigFloats(file, &value[0].x, 6);
}

void ReadBigVec4(idFile* file, idVec4& value) {
  ReadBigFloats(file, &value.x, 4);
}

void WriteBigVec4(idFile* file, const idVec4& value) {
  WriteBigFloats(file, &value.x, 4);
}

void ReadRawString(idFile* file, idStr& value) {
  file->ReadString(value);
}

void ReadRawString(idFile* file, idAtomicString& value) {
  idStr text;
  file->ReadString(text);
  value.Set(text.data);
}

idStr ParticleGeneratedFileName(const char* name) {
  idStr result;
  result.Format("generated/decls/particle/%s.bin", name);
  return result;
}

} // namespace

int ParticleDistributionNameToType(const idStr& text) {
  return ParticleNameToType(text, particleDistributionDesc);
}

int ParticleDirectionNameToType(const idStr& text) {
  return ParticleNameToType(text, particleDirectionDesc);
}

int ParticleOrientationNameToType(const idStr& text) {
  return ParticleNameToType(text, particleOrientationDesc);
}

int ParticleCustomNameToType(const idStr& text) {
  return ParticleNameToType(text, particleCustomDesc);
}

int ParticleTextureFlipNameToType(const idStr& text) {
  return ParticleNameToType(text, particleTexFlipTypeDesc);
}

int ParticleAnimationNameToType(const idStr& text) {
  return ParticleNameToType(text, particleAnimationTypeDesc);
}

int ParticleSortNameToType(const idStr& text) {
  return ParticleNameToType(text, particleSortTypeDesc);
}

int ParticleLightingToType(const idStr& text) {
  return ParticleNameToType(text, particleLightingDesc);
}

idParticleStageImposter* idDeclParticle::CreateNewDuplicateSpawn() {
  void* storage = idMem::Singleton()->AllocWithLocation(
      "w:\\build\\tech5\\engine\\models\\particles\\DeclParticle.cpp(2175) : TAG_PARTICLE",
      sizeof(idParticleStageImposter), static_cast<memTag_t>(79), false, ALIGN_16,
      HEAP_DEFAULTHEAP);
  return storage != nullptr ? new (storage) idParticleStageImposter : nullptr;
}

idParticleStage* idDeclParticle::CreateNewStage() {
  void* storage = idMem::Singleton()->AllocWithLocation(
      "w:\\build\\tech5\\engine\\models\\particles\\DeclParticle.cpp(2243) : TAG_PARTICLE",
      sizeof(idParticleStage), static_cast<memTag_t>(79), false, ALIGN_16,
      HEAP_DEFAULTHEAP);
  return storage != nullptr ? new (storage) idParticleStage : nullptr;
}

const char* idDeclParticle::DefaultDefinition() const {
  return "{\n"
         "\t{\n"
         "\t\tmaterial\ttextures/effects/particles/default\n"
         "\t\tcount\t\t5\n"
         "\t\ttime\t\tconstant 1.0\n"
         "\t\tspeedZ\t\tparametricIntegrate \"150.000\"\n"
         "\t}\n"
         "}\n";
}

float idParticleParm::GetMaxParmVal(const idLookupTable* lookupTables) const {
  const auto maximum = [lookupTables](int index) {
    return index >= 0 && lookupTables != nullptr
               ? lookupTables[index].GetMaximum()
               : 0.0f;
  };
  switch (calcType) {
  case PARTICLE_CALC_CURVE_GENERIC: {
    const float tableMaximum = maximum(tableIdx);
    return ((tableMaximum * variance) + tableMaximum) * val0 + val1;
  }
  case PARTICLE_CALC_GENERIC: {
    const float valueMaximum = std::max(val0, val1);
    return valueMaximum * variance + valueMaximum;
  }
  case PARTICLE_CALC_CURVE_MOD_CURVE:
    return maximum(tableIdx) * maximum(table2Idx);
  case PARTICLE_CALC_CURVE_ADD_CURVE:
    return maximum(tableIdx) + maximum(table2Idx);
  case PARTICLE_CALC_PARAMETRIC_EVAL:
    return val1 * variance + val1;
  case PARTICLE_CALC_PARAMETRIC_INTEGRATE:
    return ((val1 * variance + val1) + val0 + val0 * variance) * 0.5f;
  case PARTICLE_CALC_PARAMETRIC_INTEGRATE_MINMAX: {
    const float valueMaximum = std::max(val0, val1);
    return ((valueMaximum * variance + valueMaximum) + valueMaximum +
            valueMaximum * variance) *
           0.5f;
  }
  default:
    return 0.0f;
  }
}

void WriteBounds(idFile* file, const idStr& variableName,
                 const idBounds& value) {
  file->WriteFloatString("\t\t%s\t ", variableName.data);
  file->WriteFloatString("\"%.3f\" \"%.3f\" \"%.3f\" ", value[0].x,
                         value[0].y, value[0].z);
  file->WriteFloatString("\"%.3f\" \"%.3f\" \"%.3f\" ", value[1].x,
                         value[1].y, value[1].z);
  file->WriteFloatString("\n");
}

void ParseBounds(idParser& source, idBounds& value) {
  value = {};
  float points[6]{};
  int count = 0;
  idToken token;
  if (source.ReadTokenOnLine(token)) {
    while (count != 6) {
      token.StripQuotes();
      points[count++] = std::strtof(token.data, nullptr);
      if (!source.ReadTokenOnLine(token))
        break;
    }
    if (count == 6 && source.ReadTokenOnLine(token))
      source.Error("ParseBounds: too many parms on line");
  }
  value.AddPoint({points[0], points[1], points[2]});
  value.AddPoint({points[3], points[4], points[5]});
}

void idDeclParticle::CalcBatchingGroups() {
  if (stages.Num() <= 1) {
    mergeSurfaces = true;
    return;
  }
  const idParticleStage* first = stages[0];
  const idMaterial* material = first->systemProperties.material;
  bool canMerge = material != nullptr && first->staticData == nullptr;
  for (int index = 1; canMerge && index < stages.Num(); ++index) {
    const idParticleStage* stage = stages[index];
    canMerge = stage->staticData == nullptr &&
               stage->systemProperties.material != nullptr &&
               material->IsEqualForRender(stage->systemProperties.material);
  }
  mergeSurfaces = canMerge;
}

idParticleStage* idDeclParticle::FindStageByName(const char* stageName) {
  if (stageName == nullptr || *stageName == '\0')
    return nullptr;
  for (int index = 0; index < stageNames.Num(); ++index) {
    if (idStr::Icmp(stageNames[index].str, stageName) == 0)
      return stages[index];
  }
  return nullptr;
}

int idDeclParticle::CalcLodForDistance(float distanceSquared) const {
  if (lodRadius <= 0)
    return 0;
  int lod = static_cast<int>(distanceSquared /
                             static_cast<float>(lodRadius * lodRadius));
  if (lod < 0)
    return 3;
  if (lod > 3)
    lod = 3;
  return 3 - lod;
}

const idLookupTable* idDeclParticle::GetTables() const {
  idDeclParticle* mutableThis = const_cast<idDeclParticle*>(this);
  for (int index = 0; index < tableDecls.Num(); ++index) {
    if (tableDecls[index]->SourceFileExists() &&
        tableDecls[index]->EverReloaded()) {
      std::memmove(&mutableThis->tables[index],
                   &tableDecls[index]->LookupTable(), sizeof(idLookupTable));
    }
  }
  return tables.list;
}

void idDeclParticle::ParseParms(idParser& source, float* parameters,
                                int maxParameters) {
  std::memset(parameters, 0, sizeof(*parameters) * maxParameters);
  idToken token;
  int count = 0;
  if (!source.ReadTokenOnLine(token))
    return;
  while (count != maxParameters) {
    if (token.type == TT_PUNCTUATION && token == "-") {
      source.ExpectTokenType(TT_NUMBER, 0, token);
      parameters[count] = -std::strtof(token.data, nullptr);
    } else {
      token.StripQuotes();
      parameters[count] = std::strtof(token.data, nullptr);
    }
    ++count;
    if (!source.ReadTokenOnLine(token))
      return;
  }
  source.Error("too many parms on line");
}

idParticleStageImposter* idDeclParticle::ParseDuplicateImposter(
    idParser& source, const idStr& duplicateName) {
  idParticleStageImposter* duplicate = CreateNewDuplicateSpawn();
  duplicate->name.Set(duplicateName.data);
  idArray<idStr, 3> locationParms{{idStr("locDataX"), idStr("locDataY"),
                                  idStr("locDataZ")}};
  int blockType = 0;
  prtLocation_t* location = nullptr;
  idParticleStageImposter::prtStageInfo_t* stageInfo = nullptr;
  idToken token;

  while (!source.HadError() && source.ReadToken(token)) {
    idStr blockName;
    if (token.type == TT_NAME) {
      blockName = token;
      if (blockName.Find("LOC_", true, 0, blockName.Length()) == 0) {
        blockName.Strip("LOC_");
        blockType = 1;
        location = duplicate->AddNewSpawnlocation();
      } else if (blockName.Find("DUPLSTAGE_", true, 0,
                                blockName.Length()) == 0) {
        blockName.Strip("DUPLSTAGE_");
        blockType = 2;
        stageInfo = duplicate->AddNewStage(blockName.data, nullptr);
      }
      source.ReadToken(token);
    }

    if (token == "{") {
      if (blockType == 1) {
        if (location == nullptr) {
          idLib::Printf("location not created aborting");
          break;
        }
        duplicate->locationNames[duplicate->locations.Num() - 1].Set(
            blockName.data);
        while (source.ReadToken(token)) {
          for (int index = 0; index < 3; ++index) {
            if (token.Icmp(locationParms.ptr[index].data) == 0)
              ParseStageParm(source, &location->loc[index], tableDecls, tables);
          }
          if (token.Icmp("diversity") == 0)
            location->diversity = source.ParseInt() & 0x7fff;
          if (token == "}")
            break;
        }
      } else if (blockType == 2) {
        if (stageInfo == nullptr) {
          idLib::Printf("stageInfo not created aborting");
          break;
        }
        while (source.ReadToken(token)) {
          if (token.Icmp("stageID") == 0) {
            float stageID = -1.0f;
            ParseParms(source, &stageID, 1);
            stageInfo->connection =
                FindStageByName(stageNames[static_cast<int>(stageID)].str);
          }
          if (token == "}")
            break;
        }
      }
    }

    if (token == "}") {
      if (blockType == 0)
        break;
      blockType = 0;
    }
  }
  return duplicate;
}

idParticleStage* idDeclParticle::ParseParticleStage(
    idParser& source, const idStr& stageName, idStr& inheritedStageName) {
  idParticleStage* stage = CreateNewStage();
  inheritedStageName.Empty();
  stage->Default(GetTables());
  int parsedParms = 0;
  idToken token;

  auto parm = [&](idParticleParm* value) {
    ParseStageParm(source, value, tableDecls, tables);
  };
  auto readMappedType = [&](int (*mapper)(const idStr&), int& destination,
                            const char* errorText) {
    source.ReadToken(token);
    const int type = mapper(token);
    if (type >= 0)
      destination = type;
    else
      source.Error(errorText, token.data);
  };
  auto updateConeAxis = [&]() {
    idAngles angles{stage->direction.parms[1], stage->direction.parms[2],
                    stage->direction.parms[3]};
    stage->direction.coneAxis = angles.ToMat3();
  };

  while (!source.HadError() && source.ReadToken(token) && token != "}") {
    if (token.Icmp("inherit") == 0) {
      if (parsedParms > 0)
        idLib::Error("Particle: %s: Stage inherited after other parms were parsed!. 'inherit' must come first.",
                     name.str);
      if (!source.ReadToken(token))
        break;
      idParticleStage* inherited = FindStageByName(token.data);
      if (inherited == nullptr)
        idLib::Error("Particle: %s: Stage inherited from invalid stage '%s'.",
                     name.str, token.data);
      *stage = *inherited;
      inheritedStageName = token;
      continue;
    }
    ++parsedParms;

    if (token.Icmp("material") == 0) {
      source.ReadToken(token);
      stage->SetMaterial(static_cast<const idMaterial*>(
          idMaterial::resourceList.FindWithInheritance(token.data, true, true)));
    } else if (token.Icmp("staticMesh") == 0) {
      source.ReadToken(token);
      stage->SetStaticMesh(static_cast<const idStaticParticleModelData*>(
          idStaticParticleModelData::resourceList.Load(token.data, true, false)));
    } else if (token.Icmp("bounds") == 0) {
      ParseBounds(source, stage->bounds);
    } else if (token.Icmp("count") == 0) {
      stage->systemProperties.totalParticles =
          static_cast<std::int16_t>(source.ParseInt());
    } else if (token.Icmp("time") == 0) {
      parm(&stage->systemProperties.particleLife);
    } else if (token.Icmp("cycles") == 0) {
      stage->systemProperties.cycles = static_cast<std::int16_t>(source.ParseInt());
    } else if (token.Icmp("timeOffset") == 0) {
      stage->systemProperties.timeOffset = source.ParseFloat();
    } else if (token.Icmp("diversity") == 0) {
      stage->systemProperties.diversity = source.ParseInt() & 0x7fff;
    } else if (token.Icmp("useSysTime") == 0) {
      stage->systemProperties.useSysTime = source.ParseFloat();
    } else if (token.Icmp("emissionTime") == 0) {
      stage->systemProperties.emissionTime = source.ParseFloat();
    } else if (token.Icmp("deadTime") == 0) {
      parm(&stage->systemProperties.deadTime);
    } else if (token.Icmp("randomDistribution") == 0) {
      stage->distribution.random = source.ParseBool();
    } else if (token.Icmp("bunching") == 0) {
      stage->systemProperties.spawnBunching = source.ParseFloat();
    } else if (token.Icmp("distributionType") == 0) {
      int type = stage->distribution.type;
      readMappedType(ParticleDistributionNameToType, type,
                     "bad distribution type: %s\n");
      stage->distribution.type = static_cast<prtDistributionType_t>(type);
    } else if (token.Icmp("distribSizeX") == 0) {
      parm(&stage->distribution.size[0]);
    } else if (token.Icmp("distribSizeY") == 0) {
      parm(&stage->distribution.size[1]);
    } else if (token.Icmp("distribSizeZ") == 0) {
      parm(&stage->distribution.size[2]);
    } else if (token.Icmp("direction") == 0) {
      if (!source.PeekTokenType(TT_PUNCTUATION, 46, token)) {
        source.ReadToken(token);
        const int type = ParticleDirectionNameToType(token);
        if (type >= 0)
          stage->direction.type = static_cast<prtDirectionType_t>(type);
        else
          source.Error("bad direction type: %s\n", token.data);
        ParseParms(source, stage->direction.parms, 4);
        updateConeAxis();
      } else {
        source.ExpectTokenType(TT_PUNCTUATION, 46, token);
        while (source.ReadToken(token) && token != "}") {
          if (token.Icmp("type") == 0) {
            int type = stage->direction.type;
            readMappedType(ParticleDirectionNameToType, type,
                           "bad direction type: %s\n");
            stage->direction.type = static_cast<prtDirectionType_t>(type);
          } else if (token.Icmp("parms") == 0) {
            ParseParms(source, stage->direction.parms, 4);
            updateConeAxis();
          } else if (token.Icmp("angleOffsetStart") == 0) {
            ParseParms(source, stage->direction.angleOffsetStart, 2);
          } else if (token.Icmp("angleOffsetRange") == 0) {
            ParseParms(source, stage->direction.angleOffsetRange, 2);
          } else {
            source.Error("Unknown token '%s' in 'direction' block in stage '%s' in particle '%s'",
                         token.data, stageName.data, name.str);
          }
        }
      }
    } else if (token.Icmp("orientation") == 0) {
      if (!source.PeekTokenType(TT_PUNCTUATION, 46, token)) {
        source.ReadToken(token);
        if (token.Icmp("world") == 0) {
          stage->orientation.world = true;
          source.ReadToken(token);
        }
        const int type = ParticleOrientationNameToType(token);
        if (type >= 0)
          stage->orientation.type = static_cast<prtOrientationType_t>(type);
        else
          source.Error("bad orientation type: %s\n", token.data);
        if (stage->orientation.type == POR_TRAIL) {
          stage->orientation.numTrails =
              static_cast<std::int16_t>(source.ParseInt());
          stage->orientation.segmentLength = source.ParseFloat();
        } else if (stage->orientation.type == POR_AIMED) {
          stage->orientation.segmentLength = source.ParseFloat();
          if (source.ReadTokenOnLine(token) && std::atol(token.data) == 1)
            stage->orientation.orientToVelOnly = true;
          if (source.ReadTokenOnLine(token))
            stage->orientation.viewFade = std::strtof(token.data, nullptr);
        }
      } else {
        source.ExpectTokenType(TT_PUNCTUATION, 46, token);
        while (source.ReadToken(token) && token != "}") {
          if (token.Icmp("type") == 0) {
            source.ReadToken(token);
            const int type = ParticleOrientationNameToType(token);
            if (type >= 0)
              stage->orientation.type = static_cast<prtOrientationType_t>(type);
            else
              source.Error("Bad orientation type: %s\n", token.data);
          } else if (token.Icmp("world") == 0) {
            stage->orientation.world = source.ParseBool();
          } else if (token.Icmp("viewFade") == 0) {
            stage->orientation.viewFade = source.ParseFloat();
          } else if (token.Icmp("segmentLength") == 0) {
            stage->orientation.segmentLength = source.ParseFloat();
          } else if (token.Icmp("aimedSafeQuad") == 0) {
            stage->orientation.aimedSafeQuad = source.ParseBool();
          } else if (token.Icmp("aimedSafeQuadAlign") == 0) {
            stage->orientation.aimedSafeQuadAlign = source.ParseFloat();
          } else if (token.Icmp("numTrails") == 0) {
            stage->orientation.numTrails =
                static_cast<std::int16_t>(source.ParseInt());
          } else if (token.Icmp("orientToVelOnly") == 0) {
            stage->orientation.orientToVelOnly = source.ParseBool();
          } else if (token.Icmp("depthOffset") == 0) {
            if (source.PeekTokenType(TT_STRING, 0, token))
              parm(&stage->orientation.depthOffset);
            else
              SetParticleParmAsConstant(&stage->orientation.depthOffset,
                                        source.ParseFloat(), 0.0f);
          } else {
            source.Error("Unknown token '%s' in 'orientation' block in stage '%s' in particle '%s'",
                         token.data, stageName.data, name.str);
          }
        }
      }
    } else if (token.Icmp("customPathType") == 0) {
      int type = stage->customPath.type;
      readMappedType(ParticleCustomNameToType, type,
                     "bad custom path type: %s\n");
      stage->customPath.type = static_cast<prtCustomPthType_t>(type);
    } else if (token.Icmp("customPathUseRandomSpawn") == 0) {
      stage->customPath.useRandomSpawn = source.ParseBool();
    } else if (token.Icmp("customPathParm0") == 0) parm(&stage->customPath.parms[0]);
    else if (token.Icmp("customPathParm1") == 0) parm(&stage->customPath.parms[1]);
    else if (token.Icmp("customPathParm2") == 0) parm(&stage->customPath.parms[2]);
    else if (token.Icmp("customPathParm3") == 0) parm(&stage->customPath.parms[3]);
    else if (token.Icmp("customPathParm4") == 0) parm(&stage->customPath.parms[4]);
    else if (token.Icmp("speedX") == 0) parm(&stage->speed.speed[0]);
    else if (token.Icmp("speedY") == 0) parm(&stage->speed.speed[1]);
    else if (token.Icmp("speedZ") == 0) parm(&stage->speed.speed[2]);
    else if (token.Icmp("worldAcceleration") == 0) stage->acceleration.world = source.ParseBool();
    else if (token.Icmp("accelerationX") == 0) parm(&stage->acceleration.acceleration[0]);
    else if (token.Icmp("accelerationY") == 0) parm(&stage->acceleration.acceleration[1]);
    else if (token.Icmp("accelerationZ") == 0) parm(&stage->acceleration.acceleration[2]);
    else if (token.Icmp("frictionX") == 0) parm(&stage->friction.friction[0]);
    else if (token.Icmp("frictionY") == 0) parm(&stage->friction.friction[1]);
    else if (token.Icmp("frictionZ") == 0) parm(&stage->friction.friction[2]);
    else if (token.Icmp("rotationSpeedX") == 0) parm(&stage->rotation.rotation[0]);
    else if (token.Icmp("rotationSpeedY") == 0) parm(&stage->rotation.rotation[1]);
    else if (token.Icmp("rotationSpeedZ") == 0) parm(&stage->rotation.rotation[2]);
    else if (token.Icmp("angleX") == 0) parm(&stage->initialRotation.initialAngle[0]);
    else if (token.Icmp("angleY") == 0) parm(&stage->initialRotation.initialAngle[1]);
    else if (token.Icmp("angleZ") == 0) parm(&stage->initialRotation.initialAngle[2]);
    else if (token.Icmp("spawnLocX") == 0) parm(&stage->spawnLocation.spawnLocation[0]);
    else if (token.Icmp("spawnLocY") == 0) parm(&stage->spawnLocation.spawnLocation[1]);
    else if (token.Icmp("spawnLocZ") == 0) parm(&stage->spawnLocation.spawnLocation[2]);
    else if (token.Icmp("pivotX") == 0) stage->pivot.pivotOffset.x = source.ParseFloat();
    else if (token.Icmp("pivotY") == 0) stage->pivot.pivotOffset.y = source.ParseFloat();
    else if (token.Icmp("sizeX") == 0) parm(&stage->size.size[0]);
    else if (token.Icmp("sizeY") == 0) parm(&stage->size.size[1]);
    else if (token.Icmp("sizeZ") == 0) parm(&stage->size.size[2]);
    else if (token.Icmp("aspect") == 0) parm(&stage->size.aspectRatio);
    else if (token.Icmp("fadeIn") == 0) stage->colorAttributes.fadeInFraction = source.ParseFloat();
    else if (token.Icmp("fadeOut") == 0) stage->colorAttributes.fadeOutFraction = source.ParseFloat();
    else if (token.Icmp("fadeIndex") == 0) stage->colorAttributes.fadeIndexFraction = source.ParseFloat();
    else if (token.Icmp("softParticleAlphaScale") == 0)
      stage->colorAttributes.softParticleAlphaScale = 1.0f / std::max(source.ParseFloat(), 1.0f);
    else if (token.Icmp("minShadowVal") == 0) stage->colorAttributes.minShadowVal = source.ParseFloat();
    else if (token.Icmp("entityColorBlendVal") == 0) stage->colorAttributes.entityColorBlendVal = source.ParseFloat();
    else if (token.Icmp("brightness") == 0) parm(&stage->colorAttributes.brightness);
    else if (token.Icmp("genericParmR") == 0) parm(&stage->genericParm.genericParm[0]);
    else if (token.Icmp("genericParmG") == 0) parm(&stage->genericParm.genericParm[1]);
    else if (token.Icmp("genericParmB") == 0) parm(&stage->genericParm.genericParm[2]);
    else if (token.Icmp("genericParmA") == 0) parm(&stage->genericParm.genericParm[3]);
    else if (token.Icmp("useGlobalShadows") == 0) stage->colorAttributes.useGlobalShadows = source.ParseBool();
    else if (token.Icmp("colorR") == 0) parm(&stage->colorAttributes.baseColor[0]);
    else if (token.Icmp("colorG") == 0) parm(&stage->colorAttributes.baseColor[1]);
    else if (token.Icmp("colorB") == 0) parm(&stage->colorAttributes.baseColor[2]);
    else if (token.Icmp("colorA") == 0) parm(&stage->colorAttributes.baseColor[3]);
    else if (token.Icmp("fadeColor") == 0) {
      stage->colorAttributes.fadeColor.x = source.ParseFloat();
      stage->colorAttributes.fadeColor.y = source.ParseFloat();
      stage->colorAttributes.fadeColor.z = source.ParseFloat();
      stage->colorAttributes.fadeColor.w = source.ParseFloat();
    } else if (token.Icmp("offsetX") == 0) parm(&stage->offset.offset[0]);
    else if (token.Icmp("offsetY") == 0) parm(&stage->offset.offset[1]);
    else if (token.Icmp("offsetZ") == 0) parm(&stage->offset.offset[2]);
    else if (token.Icmp("animationFrames") == 0) {
      const int frames = source.ParseInt();
      stage->texAnimation.numColumns = static_cast<std::uint16_t>(frames);
      if (stage->texAnimation.numColumns > 64) {
        idLib::Printf("%s: particle filmstrip has exceeded max column frames %d with %d frames, capping frames\n",
                      name.str, frames, 64);
        stage->texAnimation.numColumns = 64;
      }
    } else if (token.Icmp("animationRows") == 0) {
      const int rows = source.ParseInt();
      stage->texAnimation.numRows = static_cast<std::uint16_t>(rows);
      if (stage->texAnimation.numRows > 4) {
        idLib::Printf("%s: particle filmstrip has exceeded max row frames %d with %d frames, capping frames\n",
                      name.str, rows, 4);
        stage->texAnimation.numRows = 4;
      }
    } else if (token.Icmp("animationRate") == 0) parm(&stage->texAnimation.rate);
    else if (token.Icmp("useRndStartFrame") == 0) {
      if (source.ParseBool()) stage->texAnimation.startFrame = -1;
    } else if (token.Icmp("animationStartFrame") == 0) stage->texAnimation.startFrame = static_cast<std::int16_t>(source.ParseInt());
    else if (token.Icmp("useRndAnimRow") == 0) stage->texAnimation.useRandomRow = source.ParseBool();
    else if (token.Icmp("animationType") == 0) {
      int type = stage->texAnimation.type;
      readMappedType(ParticleAnimationNameToType, type, "bad animation type: %s\n");
      stage->texAnimation.type = static_cast<prtAnimationType_t>(type);
    } else if (token.Icmp("skipAnimCrossFade") == 0) stage->texAnimation.useFrameBlending = !source.ParseBool();
    else if (token.Icmp("rndFlipTexS") == 0) {
      int type = stage->systemProperties.textureFlipS;
      readMappedType(ParticleTextureFlipNameToType, type, "bad texture flip mode type: %s\n");
      stage->systemProperties.textureFlipS = static_cast<prtTextureFlipMode_t>(type);
    } else if (token.Icmp("rndFlipTexT") == 0) {
      int type = stage->systemProperties.textureFlipT;
      readMappedType(ParticleTextureFlipNameToType, type, "bad texture flip mode type: %s\n");
      stage->systemProperties.textureFlipT = static_cast<prtTextureFlipMode_t>(type);
    } else if (token.Icmp("allowRotDirOverride") == 0) stage->rotation.allowRotDirOverride = source.ParseBool();
    else if (token.Icmp("boundsExpansion") == 0) stage->systemProperties.boundsExpansion = source.ParseFloat();
    else if (token.Icmp("randomOnCycle") == 0) stage->systemProperties.randomOnCycle = source.ParseBool();
    else if (token.Icmp("lightingType") == 0) {
      int type = stage->systemProperties.lightingType;
      readMappedType(ParticleLightingToType, type, "bad lighting mode: %s\n");
      stage->systemProperties.lightingType = static_cast<prtLightingType_t>(type);
    } else if (token.Icmp("gravity") == 0) {
      source.ReadToken(token);
      if (token.Icmp("world") == 0) stage->gravity.world = true;
      else source.UnreadToken(token);
      parm(&stage->gravity.gravity);
    } else if (token.Icmp("windBias") == 0) parm(&stage->systemProperties.windBias);
    else if (token.Icmp("sortType") == 0) {
      int type = stage->systemProperties.sortType;
      readMappedType(ParticleSortNameToType, type, "bad sort type: %s\n");
      stage->systemProperties.sortType = static_cast<prtSortType_t>(type);
    } else if (token.Icmp("lodCount") == 0) stage->lodParms.totalParticles = static_cast<std::int16_t>(std::max(source.ParseInt(), 1));
    else if (token.Icmp("lodScale") == 0) stage->lodParms.sizeScale = source.ParseFloat();
    else if (token.Icmp("lodLerpAmount") == 0) stage->lodParms.lerpAmount = source.ParseFloat();
    else if (token.Icmp("lodRadius") == 0) stage->lodParms.radius = static_cast<std::int16_t>(source.ParseInt());
    else source.Warning("unknown token %s\n", token.data);
  }

  stage->maxParticleLife = stage->systemProperties.particleLife.GetMaxParmVal(GetTables());
  stage->maxDeadTime = stage->systemProperties.deadTime.GetMaxParmVal(GetTables());
  stage->cycleMsec = static_cast<int>(stage->maxDeadTime * 1000.0f +
                                      stage->maxParticleLife * 1000.0f);
  stage->bunchTime = stage->systemProperties.emissionTime <= 0.0f
                         ? stage->maxParticleLife
                         : stage->systemProperties.emissionTime;
  const int vertsPerParticle = stage->NumVertsPerParticle();
  const int maximumParticles = 0x4000 / (32 * vertsPerParticle);
  if (stage->systemProperties.totalParticles * vertsPerParticle > 512) {
    idLib::Warning("Particle '%s' exceeds max vertex count in stage '%s', reducing number of particles from %d to %d!",
                   name.str, stageName.data,
                   stage->systemProperties.totalParticles, maximumParticles);
    stage->systemProperties.totalParticles =
        static_cast<std::int16_t>(maximumParticles);
  }
  if (stage->lodParms.totalParticles * vertsPerParticle > 512) {
    idLib::Warning("Particle '%s' exceeds max vertex count in stage '%s', reducing number of LOD particles from %d to %d!",
                   name.str, stageName.data, stage->lodParms.totalParticles,
                   maximumParticles);
    stage->lodParms.totalParticles =
        static_cast<std::int16_t>(maximumParticles);
  }
  if (stage->orientation.numTrails > 6) {
    idLib::Warning("Particle '%s' has too many trails in stage '%s', reducing number of trails from %d to %d!",
                   name.str, stageName.data, stage->orientation.numTrails, 6);
    stage->orientation.numTrails = 6;
  }
  if (stage->systemProperties.material == nullptr)
    idLib::Warning("Particle '%s' is missing material in stage '%s'",
                   name.str, stageName.data);
  return stage;
}

void idDeclParticle::UpdateBounds() {
  bounds.Clear();
  for (int index = 0; index < stages.Num(); ++index) {
    idPartImposterData* duplicateData =
        index < imposterData.Num() ? imposterData[index] : nullptr;
    stages[index]->CalculateBounds(
        GetTables(), duplicateData != nullptr ? duplicateData->locData : nullptr,
        duplicateData != nullptr ? duplicateData->sumLocations : 0);
    bounds.AddPoint(stages[index]->bounds[0]);
    bounds.AddPoint(stages[index]->bounds[1]);
  }
  const float width = bounds[1].x - bounds[0].x;
  const float height = bounds[1].y - bounds[0].y;
  const float depth = bounds[1].z - bounds[0].z;
  if (bounds[0].x >= bounds[1].x || bounds[0].y >= bounds[1].y ||
      bounds[0].z >= bounds[1].z || width * height * depth <= 0.1f) {
    bounds[0] = {-8.0f, -8.0f, -8.0f};
    bounds[1] = {8.0f, 8.0f, 8.0f};
  }
}

void idDeclParticle::UpdateMaxSystemDuration() {
  maxSystemDuration = 0;
  for (int index = 0; index < stages.Num(); ++index) {
    idParticleStage* stage = stages[index];
    stage->maxParticleLife =
        stage->systemProperties.particleLife.GetMaxParmVal(GetTables());
    stage->maxDeadTime =
        stage->systemProperties.deadTime.GetMaxParmVal(GetTables());
    const float cycleTime =
        stage->maxParticleLife * 1000.0f + stage->maxDeadTime * 1000.0f;
    stage->cycleMsec = static_cast<int>(cycleTime);
    stage->bunchTime = stage->systemProperties.emissionTime > 0.0f
                           ? stage->systemProperties.emissionTime
                           : stage->maxParticleLife;
    const int duration = static_cast<int>(cycleTime) -
                         static_cast<int>(
                             stage->systemProperties.timeOffset * -1000.0f);
    maxSystemDuration = std::max(maxSystemDuration, duration);
  }
}

void idDeclParticle::UpdateWindUsage() {
  usesWind = false;
  for (int index = 0; index < stages.Num(); ++index) {
    usesWind |= stages[index]->systemProperties.windBias.GetMaxParmVal(
                    GetTables()) >
                0.00000011920929f;
  }
}

void idDeclParticle::CreateDuplicateData() {
  if (stages.Num() == 0)
    return;
  imposterData.SetNum(stages.Num());
  for (int stageIndex = 0; stageIndex < stages.Num(); ++stageIndex) {
    idPartImposterData* data = static_cast<idPartImposterData*>(
        idMem::Singleton()->AllocWithLocation(
            "w:\\build\\tech5\\engine\\models\\particles\\DeclParticle.cpp(1782) : TAG_PARTICLE",
            sizeof(idPartImposterData), static_cast<memTag_t>(79), false,
            ALIGN_16, HEAP_DEFAULTHEAP));
    data->sumLocations = 0;
    data->locData = nullptr;
    imposterData[stageIndex] = data;
  }

  for (int stageIndex = 0; stageIndex < stages.Num(); ++stageIndex) {
    for (int imposterIndex = 0; imposterIndex < imposters.Num();
         ++imposterIndex) {
      idParticleStageImposter* imposter = imposters[imposterIndex];
      for (int infoIndex = 0; infoIndex < imposter->stageData.Num();
           ++infoIndex) {
        if (imposter->stageData[infoIndex]->connection == stages[stageIndex])
          imposterData[stageIndex]->sumLocations += imposter->locations.Num();
      }
    }
  }

  for (int stageIndex = 0; stageIndex < stages.Num(); ++stageIndex) {
    idPartImposterData* data = imposterData[stageIndex];
    if (data->sumLocations == 0)
      continue;
    idLib::Printf("Allocated mem %d \n",
                  data->sumLocations * static_cast<int>(sizeof(prtLocation_t)));
    data->locData = static_cast<prtLocation_t*>(
        idMem::Singleton()->AllocWithLocation(
            "w:\\build\\tech5\\engine\\models\\particles\\DeclParticle.cpp(1812) : TAG_PARTICLE",
            sizeof(prtLocation_t) *
                static_cast<std::size_t>(data->sumLocations),
            static_cast<memTag_t>(79), true, ALIGN_16, HEAP_DEFAULTHEAP));
    int destination = 0;
    for (int imposterIndex = 0; imposterIndex < imposters.Num();
         ++imposterIndex) {
      idParticleStageImposter* imposter = imposters[imposterIndex];
      for (int infoIndex = 0; infoIndex < imposter->stageData.Num();
           ++infoIndex) {
        if (imposter->stageData[infoIndex]->connection != stages[stageIndex])
          continue;
        for (int locationIndex = 0; locationIndex < imposter->locations.Num();
             ++locationIndex)
          data->locData[destination++] = *imposter->locations[locationIndex];
      }
    }
  }

  if (g_debugDuplicateStages.GetInteger() > 1) {
    for (int stageIndex = 0; stageIndex < imposterData.Num(); ++stageIndex) {
      const idPartImposterData* data = imposterData[stageIndex];
      idLib::Printf("On stage %d we have %d locations \n", stageIndex,
                    data->sumLocations);
      for (int locationIndex = 0; locationIndex < data->sumLocations;
           ++locationIndex) {
        const prtLocation_t& location = data->locData[locationIndex];
        idLib::Printf("L%d location(%f %f %f) div %d \n", locationIndex,
                      location.loc[0].val0, location.loc[1].val0,
                      location.loc[2].val0, location.diversity);
      }
    }
  }
}

idDeclParticle::idDeclParticle() {
  resourceListPtr = &resourceList;
  stages.granularity = 0;
  stageNames.granularity = 0;
  stageInheritNames.granularity = 0;
  tableDecls.granularity = 0;
  tables.granularity = 0;
  imposters.granularity = 0;
  imposterData.granularity = 0;
  bounds = {};
  maxSystemDuration = 0;
  lodRadius = 0;
  usesWind = false;
  mergeSurfaces = false;
}

idDeclParticle::~idDeclParticle() { FreeData(); }

void idDeclParticle::FreeData() {
  for (int index = 0; index < imposters.Num(); ++index) {
    if (imposters[index] != nullptr) {
      imposters[index]->~idParticleStageImposter();
      idMem::Singleton()->Free(imposters[index], ALIGN_16);
    }
  }
  for (int index = 0; index < stages.Num(); ++index)
    idMem::Singleton()->Free(stages[index], ALIGN_16);
  for (int index = 0; index < imposterData.Num(); ++index) {
    if (imposterData[index] != nullptr) {
      idMem::Singleton()->Free(imposterData[index]->locData, ALIGN_16);
      idMem::Singleton()->Free(imposterData[index], ALIGN_16);
    }
  }
  imposterData.ClearFree();
  imposters.ClearFree();
  stages.ClearFree();
  stageNames.ClearFree();
  stageInheritNames.ClearFree();
  tableDecls.ClearFree();
  tables.ClearFree();
}

idDeclInfo* idDeclParticle::GetDeclInfo() const { return &resourceList; }

void idDeclParticle::WriteStageImposter(
    idFile* file, const idParticleStageImposter* imposter) {
  if (imposter == nullptr)
    return;
  idStr fullTag("DUPLSPAWN_");
  fullTag += imposter->name.str;
  file->WriteFloatString("\t\"%s\" {\n\n", fullTag.data);

  const int defaultStageIndex = -1;
  for (int index = 0; index < imposter->stageData.Num(); ++index) {
    const idParticleStageImposter::prtStageInfo_t* info =
        imposter->stageData[index];
    fullTag = "DUPLSTAGE_";
    fullTag += info->name.str;
    file->WriteFloatString("\t\t\"%s\" {\n\n", fullTag.data);
    int stageIndex = -1;
    for (int candidate = 0; candidate < stages.Num(); ++candidate) {
      if (stages[candidate] == info->connection) {
        stageIndex = candidate;
        break;
      }
    }
    WriteParticleString<int>(file, "stageID", "%i", stageIndex,
                             defaultStageIndex);
    file->WriteFloatString("\n\t\t}\n");
  }

  const int defaultDiversity = -1;
  idParticleParm defaultParm{};
  defaultParm.tableIdx = -1;
  defaultParm.table2Idx = -1;
  defaultParm.calcType = PARTICLE_CALC_NONE;
  idStr empty;
  auto& declTables =
      reinterpret_cast<idList<const idDeclTable*, 5>&>(tableDecls);
  for (int index = 0; index < imposter->locations.Num(); ++index) {
    const prtLocation_t* location = imposter->locations[index];
    fullTag = "LOC_";
    fullTag += imposter->locationNames[index].str;
    file->WriteFloatString("\t\t\"%s\" {\n\n", fullTag.data);
    WriteParticleParm(file, "locDataX", &location->loc[0], &defaultParm,
                      declTables, empty, empty);
    WriteParticleParm(file, "locDataY", &location->loc[1], &defaultParm,
                      declTables, empty, empty);
    WriteParticleParm(file, "locDataZ", &location->loc[2], &defaultParm,
                      declTables, empty, empty);
    WriteParticleString<int>(file, "diversity", "%i", location->diversity,
                             defaultDiversity);
    file->WriteFloatString("\n\t\t}\n");
  }
  file->WriteFloatString("\n\t}\n");
}

void idDeclParticle::WriteStage(idFile* file, const idParticleStage* stage,
                                const char* stageName,
                                const idParticleStage* parent,
                                const char* parentName) {
  file->WriteFloatString("\t\"%s\" {\n\n", stageName);
  if (idStr::Cmp(parentName, "") != 0) {
    idStr paddedInherit("inherit");
    paddedInherit.PadLength(25);
    file->WriteFloatString("\t\t%s\t\"%s\"\n", paddedInherit.data,
                           parentName);
  }
  WriteBounds(file, idStr("bounds"), stage->bounds);

  auto& declTables =
      reinterpret_cast<idList<const idDeclTable*, 5>&>(tableDecls);
  const idStr empty;
  auto writeParm = [&](const char* parmName, const idParticleParm& value,
                       const idParticleParm& defaultValue,
                       const char* info = "", const char* parentInfo = "") {
    WriteParticleParm(file, parmName, &value, &defaultValue, declTables,
                      idStr(info), idStr(parentInfo));
  };
  auto changedFloats = [](const float* lhs, const float* rhs, int count) {
    for (int index = 0; index < count; ++index)
      if (lhs[index] != rhs[index]) return true;
    return false;
  };

  WriteParticleString<short>(file, "count", "%i",
      stage->systemProperties.totalParticles,
      parent->systemProperties.totalParticles);
  const char* stageMaterial = stage->systemProperties.material->name.str;
  const char* parentMaterial = parent->systemProperties.material->name.str;
  WriteParticleString<const char*>(file, "material", "%s", stageMaterial,
                                    parentMaterial);
  if (stage->staticData != nullptr) {
    const char* staticName = stage->staticData->name.str;
    const char* parentStaticName = parent->staticData != nullptr
                                       ? parent->staticData->name.str
                                       : "";
    WriteParticleString<const char*>(file, "staticMesh", "%s", staticName,
                                      parentStaticName);
  }
  WriteParticleString<short>(file, "cycles", "%i",
      stage->systemProperties.cycles, parent->systemProperties.cycles);
  WriteParticleString<float>(file, "bunching", "%.3f",
      stage->systemProperties.spawnBunching,
      parent->systemProperties.spawnBunching);
  writeParm("offsetX", stage->offset.offset[0], parent->offset.offset[0]);
  writeParm("offsetY", stage->offset.offset[1], parent->offset.offset[1]);
  writeParm("offsetZ", stage->offset.offset[2], parent->offset.offset[2]);
  WriteParticleString<float>(file, "boundsExpansion", "%.3f",
      stage->systemProperties.boundsExpansion,
      parent->systemProperties.boundsExpansion);
  WriteParticleBool(file, "randomOnCycle", stage->systemProperties.randomOnCycle,
                    parent->systemProperties.randomOnCycle);
  const char* lighting = particleLightingDesc[stage->systemProperties.lightingType].name;
  const char* parentLighting = particleLightingDesc[parent->systemProperties.lightingType].name;
  WriteParticleString<const char*>(file, "lightingType", "%s", lighting,
                                    parentLighting);
  writeParm("time", stage->systemProperties.particleLife,
            parent->systemProperties.particleLife);
  WriteParticleString<float>(file, "timeOffset", "%.3f",
      stage->systemProperties.timeOffset, parent->systemProperties.timeOffset);
  WriteParticleString<int>(file, "diversity", "%i",
      stage->systemProperties.diversity, parent->systemProperties.diversity);
  WriteParticleString<float>(file, "useSysTime", "%.3f",
      stage->systemProperties.useSysTime, parent->systemProperties.useSysTime);
  WriteParticleString<float>(file, "emissionTime", "%.3f",
      stage->systemProperties.emissionTime, parent->systemProperties.emissionTime);
  writeParm("deadTime", stage->systemProperties.deadTime,
            parent->systemProperties.deadTime);

  const char* custom = particleCustomDesc[stage->customPath.type].name;
  const char* parentCustom = particleCustomDesc[parent->customPath.type].name;
  WriteParticleString<const char*>(file, "customPathType", "%s", custom,
                                    parentCustom);
  WriteParticleBool(file, "customPathUseRandomSpawn",
                    stage->customPath.useRandomSpawn,
                    parent->customPath.useRandomSpawn);
  for (int index = 0; index < particleCustomDesc[stage->customPath.type].count;
       ++index) {
    idStr parmName;
    parmName.Format("customPathParm%d", index);
    writeParm(parmName.data, stage->customPath.parms[index],
              parent->customPath.parms[index]);
  }

  const char* distribution = particleDistributionDesc[stage->distribution.type].name;
  const char* parentDistribution = particleDistributionDesc[parent->distribution.type].name;
  WriteParticleString<const char*>(file, "distributionType", "%s",
                                    distribution, parentDistribution);
  WriteParticleBool(file, "randomDistribution", stage->distribution.random,
                    parent->distribution.random);
  writeParm("distribSizeX", stage->distribution.size[0], parent->distribution.size[0]);
  writeParm("distribSizeY", stage->distribution.size[1], parent->distribution.size[1]);
  writeParm("distribSizeZ", stage->distribution.size[2], parent->distribution.size[2]);
  writeParm("spawnLocX", stage->spawnLocation.spawnLocation[0], parent->spawnLocation.spawnLocation[0]);
  writeParm("spawnLocY", stage->spawnLocation.spawnLocation[1], parent->spawnLocation.spawnLocation[1]);
  writeParm("spawnLocZ", stage->spawnLocation.spawnLocation[2], parent->spawnLocation.spawnLocation[2]);

  const bool directionChanged = stage->direction.type != parent->direction.type ||
      changedFloats(stage->direction.parms, parent->direction.parms, 4) ||
      changedFloats(stage->direction.angleOffsetStart,
                    parent->direction.angleOffsetStart, 2) ||
      changedFloats(stage->direction.angleOffsetRange,
                    parent->direction.angleOffsetRange, 2);
  if (directionChanged) {
    file->WriteFloatString("\t\tdirection {\n");
    if (stage->direction.type != parent->direction.type)
      file->WriteFloatString("\t\t\ttype %s\n",
          particleDirectionDesc[stage->direction.type].name);
    if (changedFloats(stage->direction.parms, parent->direction.parms, 4)) {
      file->WriteFloatString("\t\t\tparms");
      for (float value : stage->direction.parms)
        file->WriteFloatString(" %.3f", value);
      file->WriteFloatString("\n");
    }
    if (changedFloats(stage->direction.angleOffsetStart,
                      parent->direction.angleOffsetStart, 2))
      file->WriteFloatString("\t\t\tangleOffsetStart %.3f %.3f\n",
          stage->direction.angleOffsetStart[0], stage->direction.angleOffsetStart[1]);
    if (changedFloats(stage->direction.angleOffsetRange,
                      parent->direction.angleOffsetRange, 2))
      file->WriteFloatString("\t\t\tangleOffsetRange %.3f %.3f\n",
          stage->direction.angleOffsetRange[0], stage->direction.angleOffsetRange[1]);
    file->WriteFloatString("\t\t}\n");
  }

  if (std::memcmp(&stage->orientation, &parent->orientation,
                  sizeof(stage->orientation)) != 0) {
    file->WriteFloatString("\t\torientation {\n");
    if (stage->orientation.type != parent->orientation.type)
      file->WriteFloatString("\t\t\ttype %s\n",
          particleOrientationDesc[stage->orientation.type].name);
    writeParm("depthOffset", stage->orientation.depthOffset,
              parent->orientation.depthOffset);
    if (stage->orientation.world != parent->orientation.world)
      file->WriteFloatString("\t\t\tworld %s\n",
                             stage->orientation.world ? "true" : "false");
    if (stage->orientation.type != POR_VIEW &&
        stage->orientation.viewFade != parent->orientation.viewFade)
      file->WriteFloatString("\t\t\tviewFade %.3f\n", stage->orientation.viewFade);
    if ((stage->orientation.type == POR_TRAIL ||
         stage->orientation.type == POR_AIMED) &&
        stage->orientation.segmentLength != parent->orientation.segmentLength)
      file->WriteFloatString("\t\t\tsegmentLength %.3f\n",
                             stage->orientation.segmentLength);
    if ((stage->orientation.type == POR_TRAIL ||
         stage->orientation.type == POR_AIMED) &&
        stage->orientation.aimedSafeQuad != parent->orientation.aimedSafeQuad)
      file->WriteFloatString("\t\t\taimedSafeQuad %s\n",
          stage->orientation.aimedSafeQuad ? "true" : "false");
    if (stage->orientation.aimedSafeQuad &&
        stage->orientation.aimedSafeQuadAlign !=
            parent->orientation.aimedSafeQuadAlign)
      file->WriteFloatString("\t\t\taimedSafeQuadAlign %.3f\n",
                             stage->orientation.aimedSafeQuadAlign);
    if (stage->orientation.type == POR_TRAIL &&
        stage->orientation.numTrails != parent->orientation.numTrails)
      file->WriteFloatString("\t\t\tnumTrails %d\n",
                             stage->orientation.numTrails);
    if (stage->orientation.type == POR_AIMED &&
        stage->orientation.orientToVelOnly !=
            parent->orientation.orientToVelOnly)
      file->WriteFloatString("\t\t\torientToVelOnly %s\n",
          stage->orientation.orientToVelOnly ? "true" : "false");
    file->WriteFloatString("\t\t}\n");
  }

  WriteParticleBool(file, "allowRotDirOverride",
      stage->rotation.allowRotDirOverride, parent->rotation.allowRotDirOverride);
  writeParm("angleX", stage->initialRotation.initialAngle[0], parent->initialRotation.initialAngle[0]);
  writeParm("angleY", stage->initialRotation.initialAngle[1], parent->initialRotation.initialAngle[1]);
  writeParm("angleZ", stage->initialRotation.initialAngle[2], parent->initialRotation.initialAngle[2]);
  WriteParticleString<float>(file, "pivotX", "%.3f", stage->pivot.pivotOffset.x, parent->pivot.pivotOffset.x);
  WriteParticleString<float>(file, "pivotY", "%.3f", stage->pivot.pivotOffset.y, parent->pivot.pivotOffset.y);
  writeParm("speedX", stage->speed.speed[0], parent->speed.speed[0]);
  writeParm("speedY", stage->speed.speed[1], parent->speed.speed[1]);
  writeParm("speedZ", stage->speed.speed[2], parent->speed.speed[2]);
  writeParm("rotationSpeedX", stage->rotation.rotation[0], parent->rotation.rotation[0]);
  writeParm("rotationSpeedY", stage->rotation.rotation[1], parent->rotation.rotation[1]);
  writeParm("rotationSpeedZ", stage->rotation.rotation[2], parent->rotation.rotation[2]);
  WriteParticleBool(file, "worldAcceleration", stage->acceleration.world,
                    parent->acceleration.world);
  writeParm("accelerationX", stage->acceleration.acceleration[0], parent->acceleration.acceleration[0]);
  writeParm("accelerationY", stage->acceleration.acceleration[1], parent->acceleration.acceleration[1]);
  writeParm("accelerationZ", stage->acceleration.acceleration[2], parent->acceleration.acceleration[2]);
  writeParm("gravity", stage->gravity.gravity, parent->gravity.gravity,
            stage->gravity.world ? "world" : "",
            parent->gravity.world ? "world" : "");
  writeParm("windBias", stage->systemProperties.windBias, parent->systemProperties.windBias);
  writeParm("frictionX", stage->friction.friction[0], parent->friction.friction[0]);
  writeParm("frictionY", stage->friction.friction[1], parent->friction.friction[1]);
  writeParm("frictionZ", stage->friction.friction[2], parent->friction.friction[2]);
  writeParm("sizeX", stage->size.size[0], parent->size.size[0]);
  writeParm("sizeY", stage->size.size[1], parent->size.size[1]);
  writeParm("sizeZ", stage->size.size[2], parent->size.size[2]);
  writeParm("aspect", stage->size.aspectRatio, parent->size.aspectRatio);

  const char* flipS = particleTexFlipTypeDesc[stage->systemProperties.textureFlipS].name;
  const char* parentFlipS = particleTexFlipTypeDesc[parent->systemProperties.textureFlipS].name;
  WriteParticleString<const char*>(file, "rndFlipTexS", "%s", flipS, parentFlipS);
  const char* flipT = particleTexFlipTypeDesc[stage->systemProperties.textureFlipT].name;
  const char* parentFlipT = particleTexFlipTypeDesc[parent->systemProperties.textureFlipT].name;
  WriteParticleString<const char*>(file, "rndFlipTexT", "%s", flipT, parentFlipT);
  const char* animation = particleAnimationTypeDesc[stage->texAnimation.type].name;
  const char* parentAnimation = particleAnimationTypeDesc[parent->texAnimation.type].name;
  WriteParticleString<const char*>(file, "animationType", "%s", animation, parentAnimation);
  WriteParticleString<unsigned short>(file, "animationFrames", "%i", stage->texAnimation.numColumns, parent->texAnimation.numColumns);
  WriteParticleString<unsigned short>(file, "animationRows", "%i", stage->texAnimation.numRows, parent->texAnimation.numRows);
  writeParm("animationRate", stage->texAnimation.rate, parent->texAnimation.rate);
  WriteParticleString<short>(file, "animationStartFrame", "%i", stage->texAnimation.startFrame, parent->texAnimation.startFrame);
  WriteParticleBool(file, "useRndAnimRow", stage->texAnimation.useRandomRow, parent->texAnimation.useRandomRow);
  const bool skipBlend = !stage->texAnimation.useFrameBlending;
  const bool parentSkipBlend = !parent->texAnimation.useFrameBlending;
  WriteParticleBool(file, "skipAnimCrossFade", skipBlend, parentSkipBlend);

  WriteParticleString<float>(file, "fadeIn", "%.3f", stage->colorAttributes.fadeInFraction, parent->colorAttributes.fadeInFraction);
  WriteParticleString<float>(file, "fadeOut", "%.3f", stage->colorAttributes.fadeOutFraction, parent->colorAttributes.fadeOutFraction);
  WriteParticleString<float>(file, "fadeIndex", "%.3f", stage->colorAttributes.fadeIndexFraction, parent->colorAttributes.fadeIndexFraction);
  WriteParticleVec4(file, "fadeColor", stage->colorAttributes.fadeColor, parent->colorAttributes.fadeColor);
  writeParm("colorR", stage->colorAttributes.baseColor[0], parent->colorAttributes.baseColor[0]);
  writeParm("colorG", stage->colorAttributes.baseColor[1], parent->colorAttributes.baseColor[1]);
  writeParm("colorB", stage->colorAttributes.baseColor[2], parent->colorAttributes.baseColor[2]);
  writeParm("colorA", stage->colorAttributes.baseColor[3], parent->colorAttributes.baseColor[3]);
  writeParm("brightness", stage->colorAttributes.brightness, parent->colorAttributes.brightness);
  writeParm("genericParmR", stage->genericParm.genericParm[0], parent->genericParm.genericParm[0]);
  writeParm("genericParmG", stage->genericParm.genericParm[1], parent->genericParm.genericParm[1]);
  writeParm("genericParmB", stage->genericParm.genericParm[2], parent->genericParm.genericParm[2]);
  writeParm("genericParmA", stage->genericParm.genericParm[3], parent->genericParm.genericParm[3]);
  const float softScale = 1.0f / stage->colorAttributes.softParticleAlphaScale;
  WriteParticleString<float>(file, "softParticleAlphaScale", "%.3f", softScale,
                             parent->colorAttributes.softParticleAlphaScale);
  WriteParticleBool(file, "useGlobalShadows", stage->colorAttributes.useGlobalShadows,
                    parent->colorAttributes.useGlobalShadows);
  WriteParticleString<float>(file, "minShadowVal", "%.3f", stage->colorAttributes.minShadowVal, parent->colorAttributes.minShadowVal);
  WriteParticleString<float>(file, "entityColorBlendVal", "%.3f", stage->colorAttributes.entityColorBlendVal, parent->colorAttributes.entityColorBlendVal);
  WriteParticleString<short>(file, "lodCount", "%i", stage->lodParms.totalParticles, parent->lodParms.totalParticles);
  WriteParticleString<float>(file, "lodScale", "%.3f", stage->lodParms.sizeScale, parent->lodParms.sizeScale);
  WriteParticleString<float>(file, "lodLerpAmount", "%.3f", stage->lodParms.lerpAmount, parent->lodParms.lerpAmount);
  WriteParticleString<short>(file, "lodRadius", "%i", stage->lodParms.radius, parent->lodParms.radius);
  const char* sort = particleSortTypeDesc[stage->systemProperties.sortType].name;
  const char* parentSort = particleSortTypeDesc[parent->systemProperties.sortType].name;
  WriteParticleString<const char*>(file, "sortType", "%s", sort, parentSort);
  file->WriteFloatString("\n\t}\n");
}

bool idDeclParticle::RebuildTextSource() {
  idFile_Memory file;
  file.WriteFloatString("{\n");
  idParticleStage defaultStage;
  defaultStage.Default(GetTables());
  if (bounds.IsZero()) {
    idLib::Warning("Invalid bounds for %s!", name.str);
  } else {
    WriteBounds(&file, idStr("bounds"), bounds);
  }

  for (int index = 0; index < stages.Num(); ++index) {
    if (stageNames[index].str[0] == '\0') {
      va generatedName{};
      idStr::snPrintf(generatedName.buffer, sizeof(generatedName.buffer),
                      "stage%d", index);
      stageNames[index].Set(generatedName.buffer);
    }
    const idParticleStage* parent = &defaultStage;
    const char* inheritedName = stageInheritNames[index].str;
    if (inheritedName != nullptr && inheritedName[0] != '\0') {
      idParticleStage* inherited = FindStageByName(inheritedName);
      if (inherited != nullptr)
        parent = inherited;
    }
    WriteStage(&file, stages[index], stageNames[index].str, parent,
               inheritedName);
  }
  if (g_debugDuplicateStages.GetInteger() > 0) {
    for (int index = 0; index < imposters.Num(); ++index)
      WriteStageImposter(&file, imposters[index]);
  }
  file.WriteFloatString("}");
  SetText(file.GetDataPtr(), static_cast<int>(file.Length()));
  return true;
}

void idDeclParticle::SaveGeneratedFile(const idStr& generatedName,
                                       int sourceChecksum) {
  idFile* file = fileSystem->OpenFileWrite(generatedName.data);
  if (file == nullptr)
    return;
  idFileLocal closeFile(file);
  const std::uint32_t magic = 0x50525406u;
  file->Write(&magic, sizeof(magic));
  WriteBigValue(file, sourceChecksum);

  WriteBigValue(file, tableDecls.num);
  for (int index = 0; index < tableDecls.Num(); ++index)
    file->WriteString(tableDecls[index] != nullptr ? tableDecls[index]->name.str
                                                   : "");

  WriteBigValue(file, stages.num);
  for (int index = 0; index < stages.Num(); ++index) {
    file->WriteString(stageNames[index].str);
    file->WriteString(stageInheritNames[index].str);
    const idParticleStage& stage = *stages[index];
    const char* materialName = stage.systemProperties.material != nullptr
                                   ? stage.systemProperties.material->name.str
                                   : "";
    const char* meshName = stage.staticData != nullptr
                               ? stage.staticData->name.str
                               : "";
    file->WriteString(materialName);
    file->WriteString(meshName);
    WriteBigValue(file, stage.cycleMsec);
    WriteBigValue(file, stage.maxParticleLife);
    WriteBigValue(file, stage.maxDeadTime);
    WriteBigValue(file, stage.bunchTime);
    WriteBigBounds(file, stage.bounds);
    const bool hidden = stage.hidden;
    file->Write(&hidden, 1);
    WriteBigShort(file, stage.systemProperties.totalParticles);
    WriteBigShort(file, stage.systemProperties.cycles);
    WriteBigValue(file, stage.systemProperties.diversity);
    WriteBinParticleParm(file, stage.systemProperties.particleLife);
    WriteBigValue(file, stage.systemProperties.timeOffset);
    WriteBinParticleParm(file, stage.systemProperties.deadTime);
    WriteBigValue(file, stage.systemProperties.useSysTime);
    WriteBigValue(file, stage.systemProperties.spawnBunching);
    WriteBigValue(file, stage.systemProperties.emissionTime);
    WriteBigValue(file, stage.systemProperties.textureFlipS);
    WriteBigValue(file, stage.systemProperties.textureFlipT);
    WriteBinParticleParm(file, stage.systemProperties.windBias);
    WriteBigValue(file, stage.systemProperties.sortType);
    WriteBigValue(file, stage.systemProperties.boundsExpansion);
    file->Write(&stage.systemProperties.randomOnCycle, 1);
    WriteBigValue(file, stage.systemProperties.lightingType);
    WriteBigValue(file, stage.distribution.type);
    WriteBinParticleParmArray(file, stage.distribution.size, 3);
    file->Write(&stage.distribution.random, 1);
    WriteBigValue(file, stage.orientation.type);
    WriteBigShort(file, stage.orientation.numTrails);
    WriteBigValue(file, stage.orientation.segmentLength);
    WriteBigValue(file, stage.orientation.viewFade);
    file->Write(&stage.orientation.orientToVelOnly, 1);
    file->Write(&stage.orientation.world, 1);
    file->Write(&stage.orientation.aimedSafeQuad, 1);
    WriteBigValue(file, stage.orientation.aimedSafeQuadAlign);
    WriteBinParticleParm(file, stage.orientation.depthOffset);
    WriteBigValue(file, stage.direction.type);
    WriteBigFloats(file, stage.direction.parms, 4);
    WriteBigFloats(file, stage.direction.angleOffsetStart, 2);
    WriteBigFloats(file, stage.direction.angleOffsetRange, 2);
    WriteBigFloats(file, &stage.direction.coneAxis.mat[0].x, 9);
    WriteBinParticleParmArray(file, stage.speed.speed, 3);
    WriteBinParticleParmArray(file, stage.acceleration.acceleration, 3);
    file->Write(&stage.acceleration.world, 1);
    WriteBinParticleParm(file, stage.gravity.gravity);
    file->Write(&stage.gravity.world, 1);
    WriteBinParticleParmArray(file, stage.friction.friction, 3);
    WriteBinParticleParmArray(file, stage.offset.offset, 3);
    WriteBinParticleParmArray(file, stage.spawnLocation.spawnLocation, 3);
    WriteBinParticleParmArray(file, stage.colorAttributes.baseColor, 4);
    WriteBigVec4(file, stage.colorAttributes.fadeColor);
    WriteBigValue(file, stage.colorAttributes.fadeInFraction);
    WriteBigValue(file, stage.colorAttributes.fadeOutFraction);
    WriteBigValue(file, stage.colorAttributes.fadeIndexFraction);
    WriteBigValue(file, stage.colorAttributes.softParticleAlphaScale);
    WriteBinParticleParm(file, stage.colorAttributes.brightness);
    file->Write(&stage.colorAttributes.useGlobalShadows, 1);
    WriteBigValue(file, stage.colorAttributes.minShadowVal);
    WriteBigValue(file, stage.colorAttributes.entityColorBlendVal);
    WriteBinParticleParmArray(file, stage.rotation.rotation, 3);
    file->Write(&stage.rotation.allowRotDirOverride, 1);
    WriteBinParticleParmArray(file, stage.initialRotation.initialAngle, 3);
    WriteBigFloats(file, &stage.pivot.pivotOffset.x, 2);
    WriteBinParticleParmArray(file, stage.size.size, 3);
    WriteBinParticleParm(file, stage.size.aspectRatio);
    WriteBigValue(file, stage.texAnimation.type);
    WriteBigShort(file, stage.texAnimation.numColumns);
    WriteBigShort(file, stage.texAnimation.numRows);
    WriteBinParticleParm(file, stage.texAnimation.rate);
    WriteBigShort(file, stage.texAnimation.startFrame);
    file->Write(&stage.texAnimation.useRandomRow, 1);
    file->Write(&stage.texAnimation.useFrameBlending, 1);
    WriteBigValue(file, stage.customPath.type);
    file->Write(&stage.customPath.useRandomSpawn, 1);
    WriteBinParticleParmArray(file, stage.customPath.parms, 5);
    WriteBinParticleParmArray(file, stage.genericParm.genericParm, 4);
    WriteBigValue(file, stage.lodParms.sizeScale);
    WriteBigValue(file, stage.lodParms.lerpAmount);
    WriteBigShort(file, stage.lodParms.radius);
    WriteBigShort(file, stage.lodParms.totalParticles);
  }

  WriteBigValue(file, imposters.num);
  for (int index = 0; index < imposters.Num(); ++index) {
    const idParticleStageImposter& imposter = *imposters[index];
    file->WriteString(imposter.name.str);
    WriteBigValue(file, imposter.stageData.num);
    for (int stageInfoIndex = 0; stageInfoIndex < imposter.stageData.Num();
         ++stageInfoIndex) {
      int stageIndex = -1;
      for (int candidate = 0; candidate < stages.Num(); ++candidate) {
        if (stages[candidate] ==
            imposter.stageData[stageInfoIndex]->connection) {
          stageIndex = candidate;
          break;
        }
      }
      file->WriteString(stageNames[stageIndex].str);
      WriteBigValue(file, stageIndex);
    }
    WriteBigValue(file, imposter.locations.num);
    for (int locationIndex = 0; locationIndex < imposter.locations.Num();
         ++locationIndex) {
      file->WriteString(imposter.locationNames[locationIndex].str);
      WriteBinParticleParmArray(file, imposter.locations[locationIndex]->loc,
                                3);
      WriteBigValue(file, imposter.locations[locationIndex]->diversity);
    }
  }

  WriteBigValue(file, imposterData.num);
  for (int index = 0; index < imposterData.Num(); ++index) {
    const idPartImposterData& data = *imposterData[index];
    WriteBigValue(file, data.sumLocations);
    for (int locationIndex = 0; locationIndex < data.sumLocations;
         ++locationIndex) {
      WriteBinParticleParmArray(file, data.locData[locationIndex].loc, 3);
      WriteBigValue(file, data.locData[locationIndex].diversity);
    }
  }
  WriteBigBounds(file, bounds);
  WriteBigValue(file, maxSystemDuration);
  WriteBigValue(file, lodRadius);
}

bool idDeclParticle::LoadGeneratedFile(const idStr& generatedName,
                                       int sourceChecksum) {
  if (g_disableParticleGeneratedBinaries.GetInteger() != 0)
    return false;
  idFile* file = fileSystem->OpenFileRead(generatedName.data);
  if (file == nullptr)
    return false;
  idFileLocal closeFile(file);
  std::uint32_t rawMagic = 0;
  if (file->Read(&rawMagic, sizeof(rawMagic)) != sizeof(rawMagic))
    return false;
  const std::uint32_t magic = ByteSwap32(rawMagic);
  const bool oldVersion = magic == 0x05545250u;
  if (magic != 0x06545250u && !oldVersion)
    return false;
  int fileChecksum = 0;
  ReadBigValue(file, fileChecksum);
  if (fileChecksum != sourceChecksum)
    return false;

  int count = 0;
  ReadBigValue(file, count);
  tableDecls.SetNum(count);
  tables.SetNum(count);
  for (int index = 0; index < count; ++index) {
    idStr tableName;
    ReadRawString(file, tableName);
    tableDecls[index] = static_cast<const idDeclTable*>(
        idDeclTable::resourceList.FindWithInheritance(tableName.data, false,
                                                       true));
    if (tableDecls[index] == nullptr) {
      idLib::Warning("particle '%s' missing table '%s'", name.str,
                     tableName.data);
      return false;
    }
    std::memmove(&tables[index], &tableDecls[index]->LookupTable(),
                 sizeof(idLookupTable));
  }

  ReadBigValue(file, count);
  stages.SetNum(count);
  stageNames.SetNum(count);
  stageInheritNames.SetNum(count);
  for (int index = 0; index < count; ++index) {
    stages[index] = CreateNewStage();
    ReadRawString(file, stageNames[index]);
    ReadRawString(file, stageInheritNames[index]);
    idParticleStage& stage = *stages[index];
    idStr materialName;
    idStr meshName;
    ReadRawString(file, materialName);
    ReadRawString(file, meshName);
    const idMaterial* material = nullptr;
    if (!materialName.IsEmpty()) {
      material = static_cast<const idMaterial*>(
          idMaterial::resourceList.FindWithInheritance(materialName.data,
                                                        false, true));
      if (material == nullptr)
        idLib::Warning("stage %d has invalid material specified", index);
    }
    const idStaticParticleModelData* mesh = nullptr;
    if (!meshName.IsEmpty())
      mesh = static_cast<const idStaticParticleModelData*>(
          idStaticParticleModelData::resourceList.Load(meshName.data, true,
                                                        false));
    stage.SetMaterial(material);
    stage.SetStaticMesh(mesh);
    ReadBigValue(file, stage.cycleMsec);
    ReadBigValue(file, stage.maxParticleLife);
    ReadBigValue(file, stage.maxDeadTime);
    ReadBigValue(file, stage.bunchTime);
    ReadBigBounds(file, stage.bounds);
    bool hidden = false;
    file->Read(&hidden, 1);
    stage.hidden = hidden;
    ReadBigShort(file, stage.systemProperties.totalParticles);
    ReadBigShort(file, stage.systemProperties.cycles);
    ReadBigValue(file, stage.systemProperties.diversity);
    ReadBinParticleParm(file, stage.systemProperties.particleLife);
    ReadBigValue(file, stage.systemProperties.timeOffset);
    ReadBinParticleParm(file, stage.systemProperties.deadTime);
    ReadBigValue(file, stage.systemProperties.useSysTime);
    ReadBigValue(file, stage.systemProperties.spawnBunching);
    ReadBigValue(file, stage.systemProperties.emissionTime);
    ReadBigValue(file, stage.systemProperties.textureFlipS);
    ReadBigValue(file, stage.systemProperties.textureFlipT);
    ReadBinParticleParm(file, stage.systemProperties.windBias);
    ReadBigValue(file, stage.systemProperties.sortType);
    ReadBigValue(file, stage.systemProperties.boundsExpansion);
    file->Read(&stage.systemProperties.randomOnCycle, 1);
    ReadBigValue(file, stage.systemProperties.lightingType);
    ReadBigValue(file, stage.distribution.type);
    ReadBinParticleParmArray(file, stage.distribution.size, 3);
    file->Read(&stage.distribution.random, 1);
    ReadBigValue(file, stage.orientation.type);
    ReadBigShort(file, stage.orientation.numTrails);
    ReadBigValue(file, stage.orientation.segmentLength);
    ReadBigValue(file, stage.orientation.viewFade);
    file->Read(&stage.orientation.orientToVelOnly, 1);
    file->Read(&stage.orientation.world, 1);
    file->Read(&stage.orientation.aimedSafeQuad, 1);
    ReadBigValue(file, stage.orientation.aimedSafeQuadAlign);
    ReadBinParticleParm(file, stage.orientation.depthOffset);
    ReadBigValue(file, stage.direction.type);
    ReadBigFloats(file, stage.direction.parms, 4);
    ReadBigFloats(file, stage.direction.angleOffsetStart, 2);
    ReadBigFloats(file, stage.direction.angleOffsetRange, 2);
    ReadBigFloats(file, &stage.direction.coneAxis.mat[0].x, 9);
    ReadBinParticleParmArray(file, stage.speed.speed, 3);
    ReadBinParticleParmArray(file, stage.acceleration.acceleration, 3);
    file->Read(&stage.acceleration.world, 1);
    ReadBinParticleParm(file, stage.gravity.gravity);
    file->Read(&stage.gravity.world, 1);
    ReadBinParticleParmArray(file, stage.friction.friction, 3);
    ReadBinParticleParmArray(file, stage.offset.offset, 3);
    ReadBinParticleParmArray(file, stage.spawnLocation.spawnLocation, 3);
    ReadBinParticleParmArray(file, stage.colorAttributes.baseColor, 4);
    ReadBigVec4(file, stage.colorAttributes.fadeColor);
    ReadBigValue(file, stage.colorAttributes.fadeInFraction);
    ReadBigValue(file, stage.colorAttributes.fadeOutFraction);
    ReadBigValue(file, stage.colorAttributes.fadeIndexFraction);
    ReadBigValue(file, stage.colorAttributes.softParticleAlphaScale);
    ReadBinParticleParm(file, stage.colorAttributes.brightness);
    file->Read(&stage.colorAttributes.useGlobalShadows, 1);
    ReadBigValue(file, stage.colorAttributes.minShadowVal);
    ReadBigValue(file, stage.colorAttributes.entityColorBlendVal);
    ReadBinParticleParmArray(file, stage.rotation.rotation, 3);
    file->Read(&stage.rotation.allowRotDirOverride, 1);
    ReadBinParticleParmArray(file, stage.initialRotation.initialAngle, 3);
    ReadBigFloats(file, &stage.pivot.pivotOffset.x, 2);
    ReadBinParticleParmArray(file, stage.size.size, 3);
    ReadBinParticleParm(file, stage.size.aspectRatio);
    ReadBigValue(file, stage.texAnimation.type);
    ReadBigShort(file, stage.texAnimation.numColumns);
    ReadBigShort(file, stage.texAnimation.numRows);
    ReadBinParticleParm(file, stage.texAnimation.rate);
    ReadBigShort(file, stage.texAnimation.startFrame);
    file->Read(&stage.texAnimation.useRandomRow, 1);
    file->Read(&stage.texAnimation.useFrameBlending, 1);
    ReadBigValue(file, stage.customPath.type);
    if (!oldVersion)
      file->Read(&stage.customPath.useRandomSpawn, 1);
    ReadBinParticleParmArray(file, stage.customPath.parms, 5);
    ReadBinParticleParmArray(file, stage.genericParm.genericParm, 4);
    ReadBigValue(file, stage.lodParms.sizeScale);
    ReadBigValue(file, stage.lodParms.lerpAmount);
    ReadBigShort(file, stage.lodParms.radius);
    ReadBigShort(file, stage.lodParms.totalParticles);
  }

  ReadBigValue(file, count);
  imposters.SetNum(count);
  for (int index = 0; index < count; ++index) {
    idParticleStageImposter* imposter = CreateNewDuplicateSpawn();
    imposters[index] = imposter;
    ReadRawString(file, imposter->name);
    int subCount = 0;
    ReadBigValue(file, subCount);
    imposter->stageData.SetNum(subCount);
    for (int stageInfoIndex = 0; stageInfoIndex < subCount;
         ++stageInfoIndex) {
      idParticleStageImposter::prtStageInfo_t* info =
          imposter->AllocStageInfo();
      imposter->stageData[stageInfoIndex] = info;
      ReadRawString(file, info->name);
      int stageIndex = -1;
      ReadBigValue(file, stageIndex);
      if (stageIndex >= 0) {
        idParticleStage* connection = FindStageByName(stageNames[stageIndex].str);
        if (connection != nullptr)
          info->connection = connection;
      }
    }
    ReadBigValue(file, subCount);
    imposter->locations.SetNum(subCount);
    imposter->locationNames.SetNum(subCount);
    for (int locationIndex = 0; locationIndex < subCount; ++locationIndex) {
      prtLocation_t* location = imposter->AllocLocation();
      imposter->locations[locationIndex] = location;
      ReadRawString(file, imposter->locationNames[locationIndex]);
      ReadBinParticleParmArray(file, location->loc, 3);
      ReadBigValue(file, location->diversity);
    }
  }

  ReadBigValue(file, count);
  imposterData.SetNum(count);
  for (int index = 0; index < count; ++index) {
    idPartImposterData* data = static_cast<idPartImposterData*>(
        idMem::Singleton()->AllocWithLocation(
            "w:\\build\\tech5\\engine\\models\\particles\\DeclParticle.cpp(1522) : TAG_PARTICLE",
            sizeof(idPartImposterData), static_cast<memTag_t>(79), false,
            ALIGN_16, HEAP_DEFAULTHEAP));
    imposterData[index] = data;
    ReadBigValue(file, data->sumLocations);
    data->locData = static_cast<prtLocation_t*>(
        idMem::Singleton()->AllocWithLocation(
            "w:\\build\\tech5\\engine\\models\\particles\\DeclParticle.cpp(1524) : TAG_PARTICLE",
            sizeof(prtLocation_t) *
                static_cast<std::size_t>(data->sumLocations),
            static_cast<memTag_t>(79), true, ALIGN_16, HEAP_DEFAULTHEAP));
    for (int locationIndex = 0; locationIndex < data->sumLocations;
         ++locationIndex) {
      ReadBinParticleParmArray(file, data->locData[locationIndex].loc, 3);
      ReadBigValue(file, data->locData[locationIndex].diversity);
    }
  }
  ReadBigBounds(file, bounds);
  ReadBigValue(file, maxSystemDuration);
  ReadBigValue(file, lodRadius);
  return true;
}

void idDeclParticle::Parse(idParser& source) {
  const idStr generatedFileName = ParticleGeneratedFileName(name.str);
  const std::uint32_t sourceChecksum = md5Checksum;
  int stageCount = 0;
  if (!LoadGeneratedFile(generatedFileName,
                         static_cast<int>(sourceChecksum))) {
    idToken token;
    int parseType = 0;
    while (source.ReadToken(token) && token.Icmp("}") != 0) {
      if (token.Icmp("bounds") == 0) {
        ParseBounds(source, bounds);
        continue;
      }
      idStr blockName;
      if (token.type == TT_NAME) {
        blockName = token;
        if (blockName.Find("DUPLSPAWN_", true, 0,
                           blockName.Length()) != 0) {
          parseType = 1;
        } else {
          blockName.Strip("DUPLSPAWN_");
          parseType = 2;
        }
        source.ReadToken(token);
      }
      if (token.Icmp("{") != 0)
        continue;
      if (parseType == 1) {
        idStr inheritedName;
        idParticleStage* stage =
            ParseParticleStage(source, blockName, inheritedName);
        if (stage == nullptr) {
          source.Warning("Particle stage parse failed");
          break;
        }
        stages.Append(stage);
        idAtomicString atomicName;
        atomicName.Set(blockName.data);
        stageNames.Append(atomicName);
        idAtomicString atomicInheritedName;
        atomicInheritedName.Set(inheritedName.data);
        stageInheritNames.Append(atomicInheritedName);
      } else if (parseType == 2) {
        idParticleStageImposter* duplicate =
            ParseDuplicateImposter(source, blockName);
        if (duplicate == nullptr) {
          source.Warning("Imposter parse failed %s", blockName.data);
          break;
        }
        imposters.Append(duplicate);
      }
    }
    CreateDuplicateData();
    UpdateBounds();
    UpdateMaxSystemDuration();
    lodRadius = 0;
    for (stageCount = 0; stageCount < stages.Num(); ++stageCount)
      lodRadius = std::max(lodRadius,
                           static_cast<int>(stages[stageCount]->lodParms.radius));
    SaveGeneratedFile(generatedFileName, static_cast<int>(sourceChecksum));
  }
  UpdateWindUsage();
  CalcBatchingGroups();
}

void idDeclParticle::LogMissingDecl() {
  static_cast<const idDeclParticle*>(this)->LogMissingDecl();
}

void idDeclParticle::LogMissingDecl() const {
  idStr filename("decls/particles/missing_decls.prt");
  char osPath[256]{};
  fileSystem->RelativePathToOSPath(filename.data, osPath,
                                   static_cast<int>(sizeof(osPath)),
                                   FSPATH_BASE);
  if (idLib::sourceControl == nullptr)
    idLib::Error("Source control not active when attempting to check out %s\n",
                 filename.data);
  if (idLib::sourceControl->CheckOut(idStr(osPath)) == 0)
    idLib::Error("Failed to check out %s\n", osPath);
  idFile* file = fileSystem->OpenFileAppend(filename.data, FSPATH_BASE);
  if (file == nullptr)
    idLib::Error("Failed to open %s\n", filename.data);
  file->Printf("particle %s\n%s", name.str, DefaultDefinition());
  delete file;
}
