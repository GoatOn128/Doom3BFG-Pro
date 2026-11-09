#include "declenv.h"

#include "engine/framework/coderesource.h"
#include "engine/renderer/parmblockparser.h"
#include "shared/idlib/lib.h"

extern idCodeResource<idDeclRenderParm> rp_BlendDuration;
extern idCodeResource<idDeclRenderParm> rp_userEnvMap;

const char* idDeclEnv::DefaultDefinition() const {
  return "{\n"
         "\trenderParms {\n"
         "\t\tenvBlendTime\t\t\t\t1.2\n"
         "\t\tuserEnvMap\t\t\t\t\tcubefilter env/default_px\n"
         "\t\tambientEnvMap\t\t\t\tcubefilter env/ambiTest_px\n"
         "\t\tenvMapRotation\t\t\t\t{ 0, 0, 0 }\n"
         "\t\tenvTint\t\t\t\t\t\t{ 1, 1, 1, 1 }\n"
         "\t\tbakedvieworigin\t\t\t\t{ 0, 0, 0 }\n"
         "\t\tuserColorCorrection\t\t\tuncompressed colorcorrection/defaultcc.tga\n"
         "\t\tcbShadow\t\t\t\t\t{ 0, 0, 0 }\n"
         "\t\tcbMidtone\t\t\t\t\t{ 0, 0, 0 }\n"
         "\t\tcbHighlight\t\t\t\t\t{ 0, 0, 0 }\n"
         "\t\tcbDesaturate\t\t\t\t0\n"
         "\t\tcbBrightness\t\t\t\t{ 1, 1, 1, 1 }\n"
         "\t\tcbColorDodge\t\t\t\t{ 0, 0, 0, 0 }\n"
         "\t\tcbColorBurn\t\t\t\t\t{ 1, 1, 1, 1 }\n"
         "\t\tcbColorMultiply\t\t\t\t{ 1, 1, 1, 1 }\n"
         "\t\tcbColorScreen\t\t\t\t{ 0, 0, 0, 0 }\n"
         "\t\tdepthBasedColorGrading\t\t{ 0, 0, 0, 0 }\n"
         "\t\tfogColor\t\t\t\t\t{ 0.2, 0.43, 0.5, 0.0 }\n"
         "\t\tfogScale\t\t\t\t\t0.00005\n"
         "\t\tfogStart\t\t\t\t\t0.0\n"
         "\t\tfogEnd\t\t\t\t\t\t32000.0\n"
         "\t\tfogSkyAmount\t\t\t\t0.0\n"
         "\t\tnewAmbientScale\t\t\t\t{ 0.3, 0.3, 0.3 }\n"
         "\t\tsharpbakedambient\t\t\t0\n"
         "\t\texposure\t\t\t\t\t1.0\n"
         "\t\tglareprescale\t\t\t\t1.0\n"
         "\t\tskyglareprescale\t\t\t1.0\n"
         "\t\tglareScale\t\t\t\t\t0.3\n"
         "\t\tlandwetrippleintensity\t\t0.0\n"
         "\t\tlandwetripplescale\t\t\t1.0\n"
         "\t\tlandwetrippleframespeed\t\t1.0\n"
         "\t\tlandwetripplemotionspeed\t1.0\n"
         "\t\tbloomminthreshold\t\t\t0.2\n"
         "\t\tbloommaxthreshold\t\t\t0.8\n"
         "\t\tbloombrightness\t\t\t\t0.5\n"
         "\t\tbloomdecay\t\t\t\t\t0.1\n"
         "\t\tbloomcolor\t\t\t\t\t{ 0, 0, 0, 0 }\n"
         "\t\tdofNearAmount\t\t\t\t0.0\n"
         "\t\tdofNearEnd\t\t\t\t\t0.0\n"
         "\t\tdofFocusBegin\t\t\t\t1.0\n"
         "\t\tdofFocusEnd\t\t\t\t\t1.0\n"
         "\t\tdofFarBegin\t\t\t\t\t5500.0\n"
         "\t\tdofFarAmount\t\t\t\t0.0\n"
         "\t\tdofSkyAmount\t\t\t\t0.0\n"
         "\t\tscreenOverlay\t\t\t\t_white\n"
         "\t\toverlayOpacity\t\t\t\t0\n"
         "\t\tgrainMap\t\t\t\t\t\tuncompressed textures/common/postgrain\n"
         "\t\tgrainAmount\t\t\t\t\t0.03\n"
         "\t\tgrainHz\t\t\t\t\t\t20\n"
         "\t\tgrainLevelsMin\t\t\t\t0\n"
         "\t\tgrainLevelsMax\t\t\t\t1\n"
         "\t\tviewSpecularScale\t\t\t1.0\n"
         "\t\thazeFlareSpillage\t\t\t0.0\n"
         "\t\thazeFlareSoftness\t\t\t0.50\n"
         "\t\thazeFlareReach\t\t\t\t1.0\n"
         "\t\thazeFlareExp\t\t\t\t1.0\n"
         "\t\thazeFlareReflExp\t\t\t0.50\n"
         "\t\thazeFlareReflection\t\t\t1\n"
         "\t\thazeFlareMatteBoxSize\t\t0.50\n"
         "\t\thazeFlareMatteBoxShade\t\t0.50\n"
         "\t\thazeFlareTintColor\t\t\t{ 1.0, 1.0, 1.0 }\n"
         "\t\tshadowPrimeIncidence\t\t0.0\n"
         "\t\tshadowIncidenceInfluence\t0.0\n"
         "\t\tspecularTint\t\t\t\t{ 0, 0, 0, 1 }\n"
         "\t\tspecularPowerAndScale\t\t{ 2, 64, 0.5, 2 }\n"
         "\t\tspecularUseOrigin\t\t\t1\n"
         "\t\tspecularFadeOutDist\t\t\t10000\n"
         "\t\tspecularOrigin\t\t\t\t{ 0, 0, 0, 1 }\n"
         "\t}\n"
         "}\n";
}

void idDeclEnv::FreeData() {
  parmBlock.Clear();
  parentDeclName.Set("");
  blendDuration = 0.0f;
}

void idDeclEnv::Parse(idParser& source) {
  FreeData();
  idParmBlock localParms;
  idToken token;

  while (source.ReadToken(token)) {
    if (token == "}") {
      parmBlock.Append(localParms);
      if (parmBlock.ops.Num() >= 80)
        idLib::Error("env decl '%s' has too many renderparms %d but max is %d",
                     name.str, parmBlock.ops.Num(), 80);
      // NEEDS_WORK(engine/renderer): rp_BlendDuration is populated by the code
      // resource registry when renderer is linked.
      const idDeclRenderParm* blendParm = rp_BlendDuration.Get();
      blendDuration = blendParm != nullptr ? parmBlock.GetFloat(blendParm, nullptr) : 0.0f;
      return;
    }

    if (token.Icmp("inherit") == 0) {
      if (!source.ExpectTokenString("{")) {
        source.Error("idDeclEnv::Parse Unexpected token, missing '{'");
        return;
      }
      if (!source.ExpectTokenType(TT_NAME, 0, token) || !source.ExpectTokenString("}")) {
        source.Error("idDeclEnv::Parse Failed to parse inherit data");
        return;
      }
      if (!token.IsEmpty()) {
        idDecl* parent = resourceList.FindWithInheritance(token.data, false, true);
        if (parent != nullptr) {
          auto* parentEnv = static_cast<idDeclEnv*>(parent);
          parmBlock.Append(parentEnv->parmBlock);
          if (declManager != nullptr) declManager->AddDependency(*this, parent);
          parentDeclName.Set(token.data);
        } else {
          source.Warning("idDeclEnv::Parse - Invalid base decl '%s'", token.data);
          parentDeclName.Set("");
        }
      }
      continue;
    }

    if (token.Icmp("renderParms") == 0) {
      if (!source.ExpectTokenString("{")) {
        source.Error("idDeclEnv::Parse Unexpected token, missing '{'");
        return;
      }
      // The constructor consumes the entire renderParms block and emits its
      // operations into localParms.
      idParmBlockParser parser(source, localParms);
      if (source.HadWarning()) return;
      continue;
    }

    source.Error("idDeclEnv::Parse Unexpected token '%s'", token.data);
    return;
  }
  source.Error("idDeclEnv::Parse Unexpected End of File");
}

const idDeclEnv* BuildEnvDecl(const char* entityName, char* parentDeclName,
                              char* worldName, const char* userEnvMap) {
  if (entityName == nullptr || worldName == nullptr || declManager == nullptr)
    return nullptr;

  idStr normalizedWorld(worldName);
  normalizedWorld.StripTrailingOnce("_lq");
  const char* parent = parentDeclName != nullptr && parentDeclName[0] != '\0'
                           ? parentDeclName
                           : "default";
  const char* envMap = userEnvMap != nullptr ? userEnvMap : "";

  idStr generatedName;
  generatedName.Format("%s/%s", normalizedWorld.data, entityName);
  auto* existing = static_cast<idDeclEnv*>(
      idDeclEnv::resourceList.FindWithInheritance(generatedName.data, false, false));

  bool needsUpdate = existing == nullptr ||
                     idStr::Icmp(existing->parentDeclName.str, parent) != 0;
  // NEEDS_WORK(engine/renderer): the retail fast path also compares the
  // resolved rp_userEnvMap image after stripping image options.
  if (envMap[0] != '\0') needsUpdate = true;
  if (!needsUpdate) return existing;

  idStr envMapLine;
  if (envMap[0] != '\0') envMapLine.Format("\t\tuserEnvMap %s\n", envMap);
  idStr declText;
  declText.Format("{\n\tinherit {\n\t\t%s\n\t}\n\trenderParms {\n%s\t}\n}\n",
                  parent, envMapLine.data);

  if (existing != nullptr) {
    existing->SetText(declText.data, declText.Length());
    existing->ReplaceSourceFileText(false);
    existing->ReParse(true);
    return existing;
  }

  idStr fileName;
  fileName.Format("generated/decls/env/%s.env", generatedName.data);
  idDecl* created = declManager->CreateNewDecl(&idDeclEnv::resourceList, generatedName.data,
                                               fileName.data, declText.data);
  if (created == nullptr) return nullptr;
  created->ReplaceSourceFileText(false);
  return static_cast<const idDeclEnv*>(created);
}
