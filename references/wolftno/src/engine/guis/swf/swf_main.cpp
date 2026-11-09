#include "engine/guis/swf/swf.h"

#include "engine/decls/declswfresource.h"
#include "engine/framework/resourcemanager.h"
#include "engine/renderer/declmaterial.h"
#include "engine/renderer/declrenderparm.h"
#include "engine/renderer/image.h"
#include "engine/sound/soundsystem.h"
#include "engine/sys/sys_public.h"
#include "engine/guis/swf/swf_spriteinstance.h"
#include "shared/idlib/color.h"
#include "shared/idlib/csystems/cvarsystemlocal.h"
#include "shared/idlib/filesystem/filesystemlocal.h"
#include "shared/idlib/langdict.h"
#include "shared/idlib/lib.h"
#include "shared/idlib/sys/sys_time.h"

#include <algorithm>
#include <cmath>
#include <cfloat>
#include <cstdint>
#include <cstring>

namespace {

idCVar swf_platformOverride(
    "swf_platformOverride", "-1", CVAR_INTEGER,
    "used to force which buttons are displayed for testing UI 0 - X360, 1 - PS3, "
    "2 - PC, 3 - XONE, 4 PS4");
idCVar swf_loadBinary("swf_loadBinary", "1", CVAR_INTEGER,
                      "used to set whether to load binary swf from generated");
idCVar swf_purgeUnusedImages("swf_purgeUnusedImages", "1", CVAR_BOOL,
                            "purge atlas images when a swf is not being rendered");
idCVar swf_playMenuSounds("swf_playMenuSounds", "1", CVAR_INTEGER,
                         "Do we want to play menu sounds 1 Yes 0 No");
idCVar swf_debugInvoke("swf_debugInvoke", "0", CVAR_BOOL,
                      "prints calls made through idSWF::Invoke");

template <typename T>
T* FindDecl(idDeclInfo& info, const char* name, bool makeDefault = true) {
  return name != nullptr ? static_cast<T*>(info.FindWithInheritance(name, makeDefault, true))
                         : nullptr;
}

void SetVar(idSWFScriptObject* object, const char* name, const idSWFScriptVar& value) {
  if (object != nullptr) object->Set(name, value);
}

void SetString(idSWFScriptObject* object, const char* name, const char* value) {
  SetVar(object, name, idSWFScriptVar(value != nullptr ? value : ""));
}

void SetFunction(idSWFScriptObject* object, const char* name,
                 idSWFScriptFunction_NestedSWF& function, idSWF* swf) {
  function.pThis = swf;
  SetVar(object, name, idSWFScriptVar(&function));
}

bool JapaneseLanguage() {
  return cvarSystem != nullptr && cvarSystem->GetCVarBool("sys_langJapanese", false);
}

int CVarInteger(const char* name, int fallback) {
  return cvarSystem != nullptr ? cvarSystem->GetCVarInteger(name, fallback) : fallback;
}

bool CVarBool(const char* name, bool fallback) {
  return cvarSystem != nullptr ? cvarSystem->GetCVarBool(name, fallback) : fallback;
}

const char* const useStringsPC[7] = {
    "", "_use", "_crouch", "_melee", "<LOCKED>", "<INVALID>", "_zoom"};
const char* const useStringsConsole[7] = {
    "", "_useandreload", "_crouch", "_melee", "<LOCKED>", "<INVALID>", "_zoom"};

struct TooltipDef {
  const char* key;
  const char* x360;
  const char* xone;
  const char* ps3;
  const char* ps4;
  int width;
  int height;
  int baseline;
};

constexpr TooltipDef tooltipDefs[] = {
    {"<DPAD>", "textures/guis/controller/xb360/360_dpad", "textures/guis/controller/xbone/xbone_dpad", "textures/guis/controller/ps3/ps3_dpad", "textures/guis/controller/ps4/ps4_dpad", 64, 64, 0},
    {"<JOY_DPAD_UP>", "textures/guis/controller/xb360/360_dpad_up", "textures/guis/controller/xbone/xbone_dpad_up", "textures/guis/controller/ps3/ps3_dpad_up", "textures/guis/controller/ps4/ps4_dpad_up", 64, 64, 0},
    {"<JOY_DPAD_RIGHT>", "textures/guis/controller/xb360/360_dpad_right", "textures/guis/controller/xbone/xbone_dpad_right", "textures/guis/controller/ps3/ps3_dpad_right", "textures/guis/controller/ps4/ps4_dpad_right", 64, 64, 0},
    {"<JOY_DPAD_DOWN>", "textures/guis/controller/xb360/360_dpad_down", "textures/guis/controller/xbone/xbone_dpad_down", "textures/guis/controller/ps3/ps3_dpad_down", "textures/guis/controller/ps4/ps4_dpad_down", 64, 64, 0},
    {"<JOY_DPAD_LEFT>", "textures/guis/controller/xb360/360_dpad_left", "textures/guis/controller/xbone/xbone_dpad_left", "textures/guis/controller/ps3/ps3_dpad_left", "textures/guis/controller/ps4/ps4_dpad_left", 64, 64, 0},
    {"<JOY1>", "textures/guis/controller/xb360/360_a", "textures/guis/controller/xbone/xbone_a", "textures/guis/controller/ps3/ps3_x", "textures/guis/controller/ps4/ps4_x", 64, 64, 0},
    {"<JOY2>", "textures/guis/controller/xb360/360_b", "textures/guis/controller/xbone/xbone_b", "textures/guis/controller/ps3/ps3_circle", "textures/guis/controller/ps4/ps4_circle", 64, 64, 0},
    {"<JOY3>", "textures/guis/controller/xb360/360_x", "textures/guis/controller/xbone/xbone_x", "textures/guis/controller/ps3/ps3_square", "textures/guis/controller/ps4/ps4_square", 64, 64, 0},
    {"<JOY4>", "textures/guis/controller/xb360/360_y", "textures/guis/controller/xbone/xbone_y", "textures/guis/controller/ps3/ps3_triangle", "textures/guis/controller/ps4/ps4_triangle", 64, 64, 0},
    {"<JOY5>", "textures/guis/controller/xb360/360_lb", "textures/guis/controller/xbone/xbone_lb", "textures/guis/controller/ps3/ps3_l1", "textures/guis/controller/ps4/ps4_l1", 64, 64, 0},
    {"<JOY6>", "textures/guis/controller/xb360/360_rb", "textures/guis/controller/xbone/xbone_rb", "textures/guis/controller/ps3/ps3_r1", "textures/guis/controller/ps4/ps4_r1", 64, 64, 0},
    {"<JOY7>", "textures/guis/controller/xb360/360_ls_push", "textures/guis/controller/xbone/xbone_ls_push", "textures/guis/controller/ps3/ps3_l3", "textures/guis/controller/ps4/ps4_l3", 64, 64, 0},
    {"<JOY8>", "textures/guis/controller/xb360/360_rs_push", "textures/guis/controller/xbone/xbone_rs_push", "textures/guis/controller/ps3/ps3_r3", "textures/guis/controller/ps4/ps4_r3", 64, 64, 0},
    {"<JOY9>", "textures/guis/controller/xb360/360_start", "textures/guis/controller/xbone/xbone_start", "textures/guis/controller/ps3/ps3_start", "textures/guis/controller/ps4/ps4_options", 64, 64, 0},
    {"<JOY10>", "textures/guis/controller/xb360/360_back", "textures/guis/controller/xbone/xbone_back", "textures/guis/controller/ps3/ps3_select", "textures/guis/controller/ps4/ps4_trackpad", 64, 64, 0},
    {"<LOCKED>", "hud/icons/interact/door_locked", "hud/icons/interact/door_locked", "hud/icons/interact/door_locked", "hud/icons/interact/door_locked", 48, 48, 0},
    {"<INVALID>", "hud/icons/interact/door_invalid", "hud/icons/interact/door_invalid", "hud/icons/interact/door_invalid", "hud/icons/interact/door_invalid", 48, 48, 0},
    {"<LEFT_STICK>", "textures/guis/controller/xb360/360_ls", "textures/guis/controller/xbone/xbone_ls", "textures/guis/controller/ps3/ps3_ls", "textures/guis/controller/ps4/ps4_ls", 64, 64, 0},
    {"<JOY_STICK1_LEFT>", "textures/guis/controller/xb360/360_ls", "textures/guis/controller/xbone/xbone_ls", "textures/guis/controller/ps3/ps3_ls", "textures/guis/controller/ps4/ps4_ls", 64, 64, 0},
    {"<JOY_STICK1_RIGHT>", "textures/guis/controller/xb360/360_ls", "textures/guis/controller/xbone/xbone_ls", "textures/guis/controller/ps3/ps3_ls", "textures/guis/controller/ps4/ps4_ls", 64, 64, 0},
    {"<JOY_STICK1_UP>", "textures/guis/controller/xb360/360_ls", "textures/guis/controller/xbone/xbone_ls", "textures/guis/controller/ps3/ps3_ls", "textures/guis/controller/ps4/ps4_ls", 64, 64, 0},
    {"<JOY_STICK1_DOWN>", "textures/guis/controller/xb360/360_ls", "textures/guis/controller/xbone/xbone_ls", "textures/guis/controller/ps3/ps3_ls", "textures/guis/controller/ps4/ps4_ls", 64, 64, 0},
    {"<RIGHT_STICK>", "textures/guis/controller/xb360/360_rs", "textures/guis/controller/xbone/xbone_rs", "textures/guis/controller/ps3/ps3_rs", "textures/guis/controller/ps4/ps4_rs", 64, 64, 0},
    {"<JOY_STICK2_LEFT>", "textures/guis/controller/xb360/360_rs", "textures/guis/controller/xbone/xbone_rs", "textures/guis/controller/ps3/ps3_rs", "textures/guis/controller/ps4/ps4_rs", 64, 64, 0},
    {"<JOY_STICK2_RIGHT>", "textures/guis/controller/xb360/360_rs", "textures/guis/controller/xbone/xbone_rs", "textures/guis/controller/ps3/ps3_rs", "textures/guis/controller/ps4/ps4_rs", 64, 64, 0},
    {"<JOY_STICK2_UP>", "textures/guis/controller/xb360/360_rs", "textures/guis/controller/xbone/xbone_rs", "textures/guis/controller/ps3/ps3_rs", "textures/guis/controller/ps4/ps4_rs", 64, 64, 0},
    {"<JOY_STICK2_DOWN>", "textures/guis/controller/xb360/360_rs", "textures/guis/controller/xbone/xbone_rs", "textures/guis/controller/ps3/ps3_rs", "textures/guis/controller/ps4/ps4_rs", 64, 64, 0},
    {"<JOY_TRIGGER2>", "textures/guis/controller/xb360/360_rt", "textures/guis/controller/xbone/xbone_rt", "textures/guis/controller/ps3/ps3_r2", "textures/guis/controller/ps4/ps4_r2", 64, 64, 0},
    {"<JOY_TRIGGER1>", "textures/guis/controller/xb360/360_lt", "textures/guis/controller/xbone/xbone_lt", "textures/guis/controller/ps3/ps3_l2", "textures/guis/controller/ps4/ps4_l2", 64, 64, 0},
};

} // namespace

idSWF::purgableSwfImage_t idSWF::purgableSWFImages[8]{};
idList<idSWF*, 72> idSWF::allSWFObjects;
std::uint32_t idSWF::swfFrameNum = 0;

void idSWF::SetListener(int id) {
  if (soundEmitter != nullptr)
    soundEmitter->UpdateEmitter(vec3_origin, mat3_identity, vec3_origin, id, false);
}

void idSWF::ClearEmitter(bool instant) {
  if (soundSystem != nullptr && soundSystem->GetGlobalSoundWorld() != nullptr &&
      soundEmitter != nullptr) {
    soundEmitter->Free(instant);
  }
  soundEmitter = nullptr;
}

void idSWF::StopSounds(int channel) {
  if (soundEmitter != nullptr)
    soundEmitter->StopSound(static_cast<soundChannel_t>(channel));
  else
    idLib::Warning("idSWF::StopSounds: No sound emitter");
}

void idSWF::Activate(int currentTime) {
  if (isActive) return;
  isActive = true;
  inhibitControl = false;
  if (sys != nullptr && sys->Joystick() != nullptr) {
    for (int i = 0; i < 4; ++i) {
      sys->Joystick()->ResetColor(i);
      sys->Joystick()->SetRumble(i, 0, 0, 0, 0);
    }
  }
  lastRenderTime = currentTime;
  mainspriteInstance->FreeDisplayList();
  PurgeOrphanage();
  mainspriteInstance->Play();
  mainspriteInstance->Run();
  mainspriteInstance->RunActions();
}

void idSWF::AllocateEmitter(idSoundWorld* soundWorld) {
  if (soundEmitter == nullptr && soundWorld != nullptr)
    soundEmitter = soundWorld->AllocSoundEmitter();
}

void idSWF::Pause() { mainspriteInstance->Stop(); }
void idSWF::Resume() { mainspriteInstance->Play(); }
void idSWF::SetGlobal(const char* name, const idSWFScriptVar& value) { globals->Set(name, value); }
idSWFScriptVar idSWF::GetGlobal(const char* name) const { return globals->Get(name); }
int idSWF::GetFontEffect(int font, int effect) const { return fontEffects[3 * font + effect]; }

bool idSWF::LoadFile(const char* sourceFilename) {
  timestamp = idLib::production != PROD_DEVELOPMENT
                  ? 1u
                  : (fileSystem != nullptr ? fileSystem->GetTimestamp(sourceFilename) : 0u);
  idStr binaryFileName("generated/");
  binaryFileName.Append(sourceFilename != nullptr ? sourceFilename : "");
  binaryFileName.BackSlashesToSlashes();
  binaryFileName.SetFileExtension(".bswf");

  idStr atlasName("generated/");
  atlasName.Append(sourceFilename != nullptr ? sourceFilename : "");
  atlasName.BackSlashesToSlashes();
  atlasName.SetFileExtension(".bimage");
  atlasTextureName.Set(atlasName.data);

  if (idLib::production >= PROD_PRODUCTION && resourceManager != nullptr)
    resourceManager->SetFileHook();

  bool loaded = false;
  if (swf_loadBinary.GetBool()) {
    if (idLib::production == PROD_PRODUCTION) {
      loaded = LoadBinary(binaryFileName.data);
    } else if (fileSystem != nullptr && fileSystem->FileExists(binaryFileName.data) &&
               LoadBinary(binaryFileName.data)) {
      loaded = true;
    } else if (LoadSWF(sourceFilename)) {
      WriteBinary(binaryFileName.data);
      loaded = true;
    }
  } else {
    loaded = LoadSWF(sourceFilename);
  }
  if (loaded) CreateAltasImageAndMaterial();
  return loaded;
}

bool idSWF::IsPlayerControlInhibited() const {
  return frameRate != 0 && isActive && inhibitControl && useInhibitControl;
}

bool idSWF::IsSoundChannelPlaying(int channel) {
  if (!isActive || soundEmitter == nullptr) return false;
  if (soundSystem == nullptr || soundSystem->GetGlobalSoundWorld() == nullptr) {
    soundEmitter = nullptr;
    return false;
  }
  return soundEmitter->IsCurrentlyPlaying(static_cast<soundChannel_t>(channel));
}

bool idSWF::IsPaused() const { return mainspriteInstance == nullptr || !mainspriteInstance->isPlaying; }

bool idSWF::Invoke(const char* functionName, const idSWFParmList& parms) {
  if (swf_debugInvoke.GetBool())
    idLib::Printf("SWF: Invoke %s with %d parms (%s)\n", functionName, parms.Num(), filename.str);
  idSWFScriptVar value = mainspriteInstance->Get(functionName);
  if (!value.IsFunction()) return false;
  idSWFParmList copy = parms;
  value.GetFunction()->Call(nullptr, copy);
  return true;
}

int idSWF::GetPlatform() {
  const int overrideValue = swf_platformOverride.GetInteger();
  if (static_cast<unsigned>(overrideValue) <= 4u) return overrideValue;
  return CVarBool("in_joystick", false) ? CVarInteger("in_joystickButtons", 2) : 2;
}

const char* idSWF::CrosshairIconMapper(int mapper) {
  if (mapper < 0 || mapper >= 7) return "";
  return CVarInteger("in_joystickButtons", 2) == 2 ? useStringsPC[mapper]
                                                   : useStringsConsole[mapper];
}

void idSWF::MarkImageForPurging(idImage* image) {
  if (image == nullptr) return;
  int index = 0;
  for (; index < 8; ++index) {
    if (purgableSWFImages[index].imageName.str == image->name.str) break;
  }
  if (index == 8) {
    for (index = 0; index < 8; ++index) {
      const char* current = purgableSWFImages[index].imageName.str;
      if (current == nullptr || current[0] == '\0') {
        purgableSWFImages[index].imageName.Set(image->name.str);
        break;
      }
    }
  }
  if (index != 8) purgableSWFImages[index].swfFrameNum = swfFrameNum;
}

void idSWF::PurgeUnusedImages() {
  if (!swf_purgeUnusedImages.GetBool()) return;
  for (purgableSwfImage_t& entry : purgableSWFImages) {
    const char* imageName = entry.imageName.str;
    if (imageName == nullptr || imageName[0] == '\0') continue;
    const unsigned inactiveFrames = swfFrameNum - entry.swfFrameNum;
    if (inactiveFrames <= 2) continue;
    if (CVarInteger("swf_debug", 0) > 0)
      idLib::Printf("Purging SWF image '%s' ... inactive for %d frames\n", imageName,
                    inactiveFrames);
    for (int i = 0; i < idImage::resourceList.resourceList.Num(); ++i) {
      idResource* resource = idImage::resourceList.resourceList[i];
      if (resource != nullptr && idStr::Cmp(resource->name.str, imageName) == 0) {
        static_cast<idImage*>(resource)->PurgeImage();
        break;
      }
    }
    entry.imageName.Set(nullptr);
  }
  ++swfFrameNum;
}

millisecondUnique_t idSWF::PlaySound(const idSoundShader* shader, int channel, bool blocking) {
  if (!isActive || !swf_playMenuSounds.GetBool()) return {};
  if (soundEmitter == nullptr) {
    idLib::Warning("idSWF::PlaySound: No sound emitter");
    return {};
  }
  soundShaderParms_t soundParms;
  soundParms.soundChannel = static_cast<soundChannel_t>(channel);
  soundParms.groups = channel == SND_CHANNEL_MUSIC1 || channel == SND_CHANNEL_MUSIC2 ||
                              channel == SND_CHANNEL_MUSIC3
                          ? SSG_MUSIC_MENU
                          : SSG_UI;
  soundParms.soundShaderFlags = blocking ? SSF_BLOCK_ON_LOAD : SSF_GLOBAL;
  return soundEmitter->StartSound(static_cast<soundChannel_t>(channel), shader, &soundParms);
}

int idSWF::FindFontFXMaterial(const char* materialName) {
  for (int i = 0; i < fontFXMaterials.Num(); ++i) {
    if (idStr::Icmp(fontFXMaterials[i]->name.str, materialName) == 0) return i;
  }
  const idMaterial* material = FindDecl<idMaterial>(idMaterial::resourceList, materialName, false);
  if (material == nullptr) return -1;
  return fontFXMaterials.Append(material);
}

void idSWF::Deactivate() {
  isActive = false;
  idSWFScriptVar callback = GetGlobal("OnDeactivate");
  if (callback.IsFunction()) {
    idSWFParmList parms;
    callback.GetFunction()->Call(nullptr, parms);
  }
}

void idSWF::RegisterSWFResources(const char* resourceName) {
  const int start = static_cast<int>(Sys_Milliseconds());
  if (resourceManager != nullptr && resourceManager->GetBuildPlatforms() != 0) {
    idSWF* swf = new idSWF(resourceName, nullptr, true);
    delete swf;
  }
  idLib::Printf("idSWF::RegisterSWFResources %s took %d ms\n", resourceName,
                static_cast<int>(Sys_Milliseconds()) - start);
}

idSWFScriptVar idSWF::idSWFScriptFunction_inhibitControl::Call(idSWFScriptObject*, idSWFParmList& parms) {
  pThis->inhibitControl = parms[0].ToBool();
  return {};
}
idSWFScriptVar idSWF::idSWFScriptFunction_useInhibit::Call(idSWFScriptObject*, idSWFParmList& parms) {
  pThis->useInhibitControl = parms[0].ToBool();
  return {};
}
idSWFScriptVar idSWF::idSWFScriptFunction_stopSounds::Call(idSWFScriptObject*, idSWFParmList& parms) {
  pThis->StopSounds(parms.Num() > 0 ? parms[0].ToInt32() : 0);
  return {};
}
idSWFScriptVar idSWF::idSWFScriptFunction_getPlatform::Call(idSWFScriptObject*, idSWFParmList&) {
  return idSWFScriptVar(GetPlatform());
}
idSWFScriptVar idSWF::idSWFScriptFunction_getTruePlatform::Call(idSWFScriptObject*, idSWFParmList&) {
  return idSWFScriptVar(2);
}
idSWFScriptVar idSWF::idSWFScriptFunction_setCVarInteger::Call(idSWFScriptObject*, idSWFParmList& parms) {
  if (cvarSystem != nullptr)
    cvarSystem->SetCVarInteger(parms[0].ToString().data, parms[1].ToInt32());
  return {};
}
idSWFScriptVar idSWF::idSWFScriptFunction_setCVarString::Call(idSWFScriptObject*, idSWFParmList& parms) {
  if (cvarSystem != nullptr) {
    const idStr name = parms[0].ToString();
    const idStr value = parms[1].ToString();
    cvarSystem->SetCVarString(name.data, value.data);
  }
  return {};
}
idSWFScriptVar idSWF::idSWFScriptFunction_setColor::Call(idSWFScriptObject*, idSWFParmList& parms) {
  if (parms.Num() < 5) {
    idLib::Warning("Invalid parameters specified for setColor");
    return {};
  }
  idSWFSpriteInstance* sprite = parms[0].GetSprite();
  if (sprite != nullptr) {
    idColor color;
    color.r = static_cast<float>(parms[1].ToNumber());
    color.g = static_cast<float>(parms[2].ToNumber());
    color.b = static_cast<float>(parms[3].ToNumber());
    color.a = static_cast<float>(parms[4].ToNumber());
    sprite->SetColor(color);
  }
  return {};
}
idSWFScriptVar idSWF::idSWFScriptFunction_Object::Call(idSWFScriptObject*, idSWFParmList& parms) {
  return parms.Num() > 0 ? parms[0] : idSWFScriptVar();
}
idSWFScriptVar idSWF::idSWFScriptFunction_precacheSound::Call(idSWFScriptObject*, idSWFParmList& parms) {
  const idStr name = parms[0].ToString();
  const idSoundShader* shader = FindDecl<idSoundShader>(idSoundShader::resourceList, name.data, true);
  return idSWFScriptVar(shader != nullptr ? shader->name.str : "");
}
idSWFScriptVar idSWF::idSWFScriptFunction_playSound::Call(idSWFScriptObject*, idSWFParmList& parms) {
  if (pThis->soundEmitter == nullptr) {
    idLib::Warning("idSWF::playSound: No sound emitter");
    return {};
  }
  const idStr name = parms[0].ToString();
  if (name.IsEmpty() || !swf_playMenuSounds.GetBool()) return {};
  const int channel = parms.Num() > 1 ? parms[1].ToInt32() : SND_CHANNEL_ANY;
  const idSoundShader* shader = FindDecl<idSoundShader>(idSoundShader::resourceList, name.data, true);
  return idSWFScriptVar(pThis->PlaySound(shader, channel, false).value);
}
idSWFScriptVar idSWF::idSWFScriptFunction_strReplace::Call(idSWFScriptObject*, idSWFParmList& parms) {
  if (parms.Num() != 3) return idSWFScriptVar("");
  idStr value = parms[0].ToString();
  const idStr oldValue = parms[1].ToString();
  const idStr newValue = parms[2].ToString();
  value.Replace(oldValue.data, newValue.data);
  return idSWFScriptVar(value);
}
idSWFScriptVar idSWF::idSWFScriptFunction_getLocalString::Call(idSWFScriptObject*, idSWFParmList& parms) {
  if (parms.Num() == 0) return {};
  const idStr value = parms[0].ToString();
  return idSWFScriptVar(idLocalization::languageDict.GetString(value.data));
}
idSWFScriptVar idSWF::idSWFScriptFunction_swapPS3Buttons::Call(idSWFScriptObject*, idSWFParmList&) {
  return idSWFScriptVar(false);
}
idSWFScriptVar idSWF::idSWFScriptFunction_getCVarInteger::Call(idSWFScriptObject*, idSWFParmList& parms) {
  const idStr name = parms[0].ToString();
  return idSWFScriptVar(CVarInteger(name.data, 0));
}
idSWFScriptVar idSWF::idSWFScriptFunction_getCVarString::Call(idSWFScriptObject*, idSWFParmList& parms) {
  const idStr name = parms[0].ToString();
  return idSWFScriptVar(cvarSystem != nullptr ? cvarSystem->GetCVarString(name.data, "") : "");
}
idSWFScriptVar idSWF::idSWFScriptFunction_isJapanese::Call(idSWFScriptObject*, idSWFParmList&) {
  return idSWFScriptVar(JapaneseLanguage());
}
idSWFScriptVar idSWF::idSWFScriptFunction_acos::Call(idSWFScriptObject*, idSWFParmList& parms) {
  if (parms.Num() != 1) return {};
  const float x = static_cast<float>(parms[0].ToNumber());
  return idSWFScriptVar(x <= -1.0f ? 3.1415927f : (x >= 1.0f ? 0.0f : std::acos(x)));
}
idSWFScriptVar idSWF::idSWFScriptFunction_cos::Call(idSWFScriptObject*, idSWFParmList& parms) {
  if (parms.Num() != 1) return {};
  float x = static_cast<float>(parms[0].ToNumber());
  if (x < 0.0f || x >= 6.2831855f) x -= std::floor(x * 0.15915494f) * 6.2831855f;
  return idSWFScriptVar(std::cos(x));
}
idSWFScriptVar idSWF::idSWFScriptFunction_sin::Call(idSWFScriptObject*, idSWFParmList& parms) {
  if (parms.Num() != 1) return {};
  float x = static_cast<float>(parms[0].ToNumber());
  if (x < 0.0f || x >= 6.2831855f) x -= std::floor(x * 0.15915494f) * 6.2831855f;
  return idSWFScriptVar(std::sin(x));
}
idSWFScriptVar idSWF::idSWFScriptFunction_round::Call(idSWFScriptObject*, idSWFParmList& parms) {
  return parms.Num() == 1 ? idSWFScriptVar(static_cast<int>(static_cast<float>(parms[0].ToNumber()) + 0.5f))
                          : idSWFScriptVar();
}
idSWFScriptVar idSWF::idSWFScriptFunction_pow::Call(idSWFScriptObject*, idSWFParmList& parms) {
  return parms.Num() == 2 ? idSWFScriptVar(std::pow(static_cast<float>(parms[0].ToNumber()),
                                                   static_cast<float>(parms[1].ToNumber())))
                          : idSWFScriptVar();
}
idSWFScriptVar idSWF::idSWFScriptFunction_sqrt::Call(idSWFScriptObject*, idSWFParmList& parms) {
  return parms.Num() == 1 ? idSWFScriptVar(std::sqrt(std::max(static_cast<float>(parms[0].ToNumber()), FLT_MIN)))
                          : idSWFScriptVar();
}
idSWFScriptVar idSWF::idSWFScriptFunction_abs::Call(idSWFScriptObject*, idSWFParmList& parms) {
  return parms.Num() == 1 ? idSWFScriptVar(std::fabs(static_cast<float>(parms[0].ToNumber())))
                          : idSWFScriptVar();
}
idSWFScriptVar idSWF::idSWFScriptFunction_rand::Call(idSWFScriptObject*, idSWFParmList& parms) {
  if (parms.Num() > 2) return {};
  static std::uint32_t seed = static_cast<std::uint32_t>(Sys_Milliseconds());
  seed = 1664525u * seed + 1013904223u;
  const float random = static_cast<float>((seed >> 10) & 0x7fff) * 0.000030518509f;
  if (parms.Num() == 0) return idSWFScriptVar(random);
  if (parms.Num() == 1) return idSWFScriptVar(random * static_cast<float>(parms[0].ToNumber()));
  const float minimum = static_cast<float>(parms[0].ToNumber());
  const float maximum = static_cast<float>(parms[1].ToNumber());
  return idSWFScriptVar(minimum + random * (maximum - minimum));
}
idSWFScriptVar idSWF::idSWFScriptFunction_floor::Call(idSWFScriptObject*, idSWFParmList& parms) {
  if (parms.Num() == 1 && (parms[0].GetType() == idSWFScriptVar::SWF_VAR_DOUBLE ||
                           parms[0].GetType() == idSWFScriptVar::SWF_VAR_INTEGER))
    return idSWFScriptVar(std::floor(static_cast<float>(parms[0].ToNumber())));
  idLib::Warning("SWF %s: Invalid parameters specified for floor", pThis->filename.str);
  return {};
}
idSWFScriptVar idSWF::idSWFScriptFunction_ceil::Call(idSWFScriptObject*, idSWFParmList& parms) {
  if (parms.Num() == 1 && (parms[0].GetType() == idSWFScriptVar::SWF_VAR_DOUBLE ||
                           parms[0].GetType() == idSWFScriptVar::SWF_VAR_INTEGER))
    return idSWFScriptVar(std::ceil(static_cast<float>(parms[0].ToNumber())));
  idLib::Warning("SWF %s: Invalid parameters specified for ceil", pThis->filename.str);
  return {};
}
idSWFScriptVar idSWF::idSWFScriptFunction_toUpper::Call(idSWFScriptObject*, idSWFParmList& parms) {
  if (parms.Num() != 1 || !parms[0].IsString()) {
    idLib::Warning("SWF %s: Invalid parameters specified for toUpper", pThis->filename.str);
    return {};
  }
  const idStr key = parms[0].ToString();
  idStr value(idLocalization::languageDict.GetString(key.data));
  value.ToUpper();
  return idSWFScriptVar(value);
}

idSWFScriptVar idSWF::idSWFScriptFunction_shortcutKeys_clear::Call(idSWFScriptObject*, idSWFParmList&) {
  static const char* const pairs[][2] = {
      {"JOY1", "ENTER"}, {"JOY2", "BACKSPACE"}, {"JOY3", "START"}, {"JOY5", "LB"},
      {"JOY6", "RB"}, {"JOY7", "LSTICK"}, {"JOY8", "RSTICK"}, {"JOY9", "START"},
      {"JOY10", "BACKSPACE"}, {"JOY_DPAD_UP", "UP"}, {"JOY_DPAD_DOWN", "DOWN"},
      {"JOY_DPAD_LEFT", "LEFT"}, {"JOY_DPAD_RIGHT", "RIGHT"},
      {"JOY_STICK1_UP", "STICK1_UP"}, {"JOY_STICK1_DOWN", "STICK1_DOWN"},
      {"JOY_STICK1_LEFT", "STICK1_LEFT"}, {"JOY_STICK1_RIGHT", "STICK1_RIGHT"},
      {"JOY_STICK2_UP", "STICK2_UP"}, {"JOY_STICK2_DOWN", "STICK2_DOWN"},
      {"JOY_STICK2_LEFT", "STICK2_LEFT"}, {"JOY_STICK2_RIGHT", "STICK2_RIGHT"},
      {"KP_ENTER", "ENTER"}, {"MWHEELDOWN", "MWHEEL_DOWN"}, {"MWHEELUP", "MWHEEL_UP"},
      {"LEFTARROW", "LEFT"}, {"RIGHTARROW", "RIGHT"}, {"UPARROW", "UP"},
      {"DOWNARROW", "DOWN"}, {"PGUP", nullptr}, {"PGDN", nullptr}, {"HOME", nullptr},
      {"END", nullptr}, {"SPACE", nullptr}};
  idSWFScriptObject* keys = pThis->shortcutKeys;
  keys->Clear();
  SetVar(keys, "clear", idSWFScriptVar(this));
  for (const auto& pair : pairs) {
    SetString(keys, pair[0], pair[1]);
    if (pair[1] != nullptr) SetString(keys, pair[1], "");
  }
  return {};
}

idSWFScriptVar idSWF::idSWFScriptFunction_soundObject_clear::Call(idSWFScriptObject*, idSWFParmList&) {
  idSWFScriptObject* object = pThis->soundObject;
  object->Clear();
  SetVar(object, "clear", idSWFScriptVar(this));
  if (pThis->swfResourceMenu == nullptr) return {};
  for (int i = 0; i < pThis->swfResourceMenu->soundEffects.Num(); ++i) {
    const idDeclSwfResource::soundEntry_t& entry = pThis->swfResourceMenu->soundEffects[i];
    if (entry.sound != nullptr) SetString(object, entry.name.data, entry.sound->name.str);
  }
  const idDeclSwfResource::musicEntry_t& music = pThis->swfResourceMenu->music;
  if (music.music != nullptr) {
    SetString(object, music.name.data, music.music->name.str);
    SetVar(object, music.channelName.data, idSWFScriptVar(static_cast<int>(music.channel)));
  }
  return {};
}

idSWFScriptVar idSWF::idSWFScriptFunction_precacheFontFXMaterial::Call(idSWFScriptObject*, idSWFParmList& parms) {
  const idStr name = parms[0].ToString();
  return idSWFScriptVar(pThis->FindFontFXMaterial(name.data));
}
idSWFScriptVar idSWF::idSWFScriptFunction_deactivate::Call(idSWFScriptObject*, idSWFParmList&) {
  pThis->Deactivate();
  return {};
}

idSWFScriptVar idSWF::idSWFScriptNativeVar_platform::Get(idSWFScriptObject*) {
  return idSWFScriptVar(GetPlatform());
}
void idSWF::idSWFScriptNativeVar_platform::Set(idSWFScriptObject*, const idSWFScriptVar&) {}
idSWFScriptVar idSWF::idSWFScriptNativeVar_blackbars::Get(idSWFScriptObject*) { return idSWFScriptVar(pThis->blackbars); }
void idSWF::idSWFScriptNativeVar_blackbars::Set(idSWFScriptObject*, const idSWFScriptVar& value) { pThis->blackbars = value.ToBool(); }
idSWFScriptVar idSWF::idSWFScriptNativeVar_crop::Get(idSWFScriptObject*) { return idSWFScriptVar(pThis->crop); }
void idSWF::idSWFScriptNativeVar_crop::Set(idSWFScriptObject*, const idSWFScriptVar& value) { pThis->crop = value.ToBool(); }
idSWFScriptVar idSWF::idSWFScriptNativeVar_automaticRescale::Get(idSWFScriptObject*) { return idSWFScriptVar(pThis->automaticRescale); }
void idSWF::idSWFScriptNativeVar_automaticRescale::Set(idSWFScriptObject*, const idSWFScriptVar& value) { pThis->automaticRescale = value.ToBool(); }

idSWF::idSWF(const char* sourceName, idSoundWorld* soundWorld, bool atlasPurgable_)
    : swfResourceMenu(nullptr), soundObject(nullptr), filename(), timestamp(0),
      recoveredTimestampPadding(0), scriptFunction_Object(), globals(nullptr), mainsprite(nullptr),
      mainspriteInstance(nullptr), parm(nullptr), focusWindow(nullptr), isActive(false),
      inhibitControl(false), useInhibitControl(true), arGui(false), pausedRender(false),
      recoveredFramePadding{}, frameWidth(0.0f), frameHeight(0.0f), frameRate(0),
      recoveredRatePadding(0), renderBorder(0.0f), swfScale(1.0f), lastRenderTime(0),
      cursorOverObject(false), showMouseCursor(false), mouseEnabled(false), useMouse(true),
      recoveredMousePadding(0), mouseObject(nullptr), hoverObject(nullptr), blackbars(false),
      crop(false), paused(false), hasHitObject(false), automaticRescale(true),
      recoveredFontEffectsPadding{}, fontEffects{}, recoveredShortcutPadding(0),
      shortcutKeys(nullptr), soundEmitter(nullptr), atlasTextureName(), atlasMaterial(nullptr),
      atlasImage(nullptr), atlasPurgable(atlasPurgable_), recoveredAtlasPadding{}, atlasWidth(0),
      atlasHeight(0), recoveredAtlasFilePadding(0), atlasFile(nullptr) {
  const bool warningPushed = idLib::PushWarningInfo("swf", sourceName != nullptr ? sourceName : "");
  scriptFunction_Object.SetPrototype(&scriptFunction_Object.object);
  mainsprite = new idSWFSprite();
  mainspriteInstance = new idSWFSpriteInstance(this, mainsprite);
  globals = new idSWFScriptObject();
  shortcutKeys = new idSWFScriptObject();
  soundObject = new idSWFScriptObject();

  guiSolid = FindDecl<idMaterial>(idMaterial::resourceList, "guiSolid");
  guiStencilIncr = FindDecl<idMaterial>(idMaterial::resourceList, "guiStencilIncr");
  guiStencilDecr = FindDecl<idMaterial>(idMaterial::resourceList, "guiStencilDecr");
  parm = FindDecl<idDeclRenderParm>(idDeclRenderParm::resourceList, "swf_snaptopixel", false);
  guiCursor_arrow = FindDecl<idMaterial>(idMaterial::resourceList, "textures/guis/default/guicursor_arrow");
  guiCursor_hand = FindDecl<idMaterial>(idMaterial::resourceList, "textures/guis/default/guicursor_hand");
  white = FindDecl<idMaterial>(idMaterial::resourceList, "_white");

  for (const TooltipDef& def : tooltipDefs) {
    keyButtonImages_t entry{def.key, def.x360, def.xone, def.ps3, def.ps4,
                            nullptr, nullptr, nullptr, nullptr,
                            def.width, def.height, def.baseline};
    entry.xb360Material = FindDecl<idMaterial>(idMaterial::resourceList, def.x360);
    entry.xbOneMaterial = FindDecl<idMaterial>(idMaterial::resourceList, def.xone);
    entry.ps3Material = FindDecl<idMaterial>(idMaterial::resourceList, def.ps3);
    entry.ps4Material = FindDecl<idMaterial>(idMaterial::resourceList, def.ps4);
    tooltipButtonImage.Append(entry);
  }
  FindDecl<idMaterial>(idMaterial::resourceList, "textures/gui/load_image");

  if (JapaneseLanguage()) {
    idStr japaneseName;
    japaneseName.Format("%s_japanese", sourceName != nullptr ? sourceName : "");
    swfResourceMenu = FindDecl<idDeclSwfResource>(idDeclSwfResource::resourceList,
                                                 japaneseName.data, false);
  }
  if (swfResourceMenu == nullptr)
    swfResourceMenu = FindDecl<idDeclSwfResource>(idDeclSwfResource::resourceList, sourceName, true);

  idStr canonical(sourceName != nullptr ? sourceName : "");
  if (idStr::Cmpn(canonical.data, "swf/", 4) != 0) {
    idStr prefixed("swf/");
    prefixed.Append(canonical);
    canonical = prefixed;
  }
  canonical.MakeNameCanonical();
  canonical.StripFileExtension();
  idStr overrideName;
  if (JapaneseLanguage()) {
    overrideName = canonical;
    overrideName.Append("_japanese.swf");
  }
  canonical.Append(".swf");
  filename.Set(canonical.data);
  if (!overrideName.IsEmpty() && LoadFile(overrideName.data))
    filename.Set(overrideName.data);
  else
    LoadFile(filename.str);

  SetVar(globals, "_global", idSWFScriptVar(globals));
  SetVar(globals, "Object", idSWFScriptVar(&scriptFunction_Object));
  scriptFunction_shortcutKeys_clear.pThis = this;
  { idSWFParmList empty; scriptFunction_shortcutKeys_clear.Call(shortcutKeys, empty); }
  SetVar(globals, "shortcutKeys", idSWFScriptVar(shortcutKeys));
  scriptFunction_soundObject_clear.pThis = this;
  { idSWFParmList empty; scriptFunction_soundObject_clear.Call(soundObject, empty); }
  SetVar(globals, "soundResource", idSWFScriptVar(soundObject));

#define BIND_SWF(name, text) SetFunction(globals, text, scriptFunction_##name, this)
  BIND_SWF(deactivate, "deactivate"); BIND_SWF(inhibitControl, "inhibitControl");
  BIND_SWF(useInhibit, "useInhibit"); BIND_SWF(precacheSound, "precacheSound");
  BIND_SWF(precacheFontFXMaterial, "precacheFontFXMaterial"); BIND_SWF(playSound, "playSound");
  BIND_SWF(stopSounds, "stopSounds"); BIND_SWF(getPlatform, "getPlatform");
  BIND_SWF(getTruePlatform, "getTruePlatform"); BIND_SWF(getLocalString, "getLocalString");
  BIND_SWF(swapPS3Buttons, "swapPS3Buttons"); BIND_SWF(strReplace, "strReplace");
  BIND_SWF(getCVarInteger, "getCVarInteger"); BIND_SWF(setCVarInteger, "setCVarInteger");
  BIND_SWF(getCVarString, "getCVarString"); BIND_SWF(setCVarString, "setCVarString");
  BIND_SWF(isJapanese, "isJapanese"); BIND_SWF(acos, "acos"); BIND_SWF(cos, "cos");
  BIND_SWF(sin, "sin"); BIND_SWF(round, "round"); BIND_SWF(pow, "pow");
  BIND_SWF(sqrt, "sqrt"); BIND_SWF(abs, "abs"); BIND_SWF(rand, "rand");
  BIND_SWF(floor, "floor"); BIND_SWF(ceil, "ceil"); BIND_SWF(toUpper, "toUpper");
  BIND_SWF(setColor, "setColor");
#undef BIND_SWF

  swfScriptVar_platform.pThis = this; globals->SetNative("platform", &swfScriptVar_platform);
  swfScriptVar_blackbars.pThis = this; globals->SetNative("blackbars", &swfScriptVar_blackbars);
  swfScriptVar_automaticRescale.pThis = this;
  globals->SetNative("automaticRescale", &swfScriptVar_automaticRescale);
  swfScriptVar_crop.pThis = this;
  globals->SetNative("cropToHeight", &swfScriptVar_crop);
  globals->SetNative("cropToFit", &swfScriptVar_crop);
  globals->SetNative("crop", &swfScriptVar_crop);

  static const char* const followNames[] = {
      "FOLLOW_NONE", "FOLLOW_TOPLEFT", "FOLLOW_TOPCENTER", "FOLLOW_TOPRIGHT",
      "FOLLOW_CENTERLEFT", "FOLLOW_CENTERCENTER", "FOLLOW_CENTERRIGHT",
      "FOLLOW_BOTTOMLEFT", "FOLLOW_BOTTOMCENTER", "FOLLOW_BOTTOMRIGHT"};
  for (int i = 0; i < 10; ++i) SetVar(globals, followNames[i], idSWFScriptVar(i - 1));

  mainspriteInstance->Init(nullptr, "_root", 0);
  SetVar(globals, "_root", idSWFScriptVar(mainspriteInstance->scriptObject));
  AllocateEmitter(soundWorld);
  mainspriteInstance->Run();
  mainspriteInstance->RunActions();
  mainspriteInstance->RunTo(0);
  if (mouseX == -1) mouseX = static_cast<int>(frameWidth * 0.5f);
  if (mouseY == -1) mouseY = static_cast<int>(frameHeight * 0.5f);
  if (idLib::production >= PROD_PRODUCTION && resourceManager != nullptr)
    resourceManager->ReleaseFileHook();
  allSWFObjects.Append(this);

  std::fill(std::begin(fontEffects), std::end(fontEffects), -1);
  if (swfResourceMenu != nullptr) {
    blackbars = swfResourceMenu->useBlackbars;
    for (int i = 0; i < swfResourceMenu->fontEffects.Num(); ++i) {
      const idDeclSwfResource::fontEntry_t& entry = swfResourceMenu->fontEffects[i];
      if (entry.material != nullptr)
        fontEffects[3 * entry.font + entry.effect] = FindFontFXMaterial(entry.material->name.str);
    }
  }
  if (warningPushed) idLib::PopWarningInfo();
}

idSWF::~idSWF() {
  if (atlasImage != nullptr && atlasImage->oglTexture != ~0u && atlasPurgable)
    MarkImageForPurging(atlasImage);
  for (int i = 0; i < allSWFObjects.Num(); ++i) {
    if (allSWFObjects[i] == this) { allSWFObjects.RemoveIndex(i); break; }
  }
  if (mouseObject != nullptr) { mouseObject->Release(); mouseObject = nullptr; }
  if (hoverObject != nullptr) { hoverObject->Release(); hoverObject = nullptr; }
  if (globals != nullptr) { globals->Clear(); globals->Release(); globals = nullptr; }
  if (shortcutKeys != nullptr) { shortcutKeys->Clear(); shortcutKeys->Release(); shortcutKeys = nullptr; }
  if (soundObject != nullptr) { soundObject->Clear(); soundObject->Release(); soundObject = nullptr; }
  delete mainspriteInstance;
  mainspriteInstance = nullptr;
  delete mainsprite;
  mainsprite = nullptr;
  PurgeOrphanage();
  dictionary.ClearFree();
  tooltipButtonImage.ClearFree();
  ClearEmitter(true);
}
