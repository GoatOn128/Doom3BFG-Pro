#pragma once

#include "decl.h"
#include "engine/sound/soundchannel.h"
#include "shared/idlib/langdict.h"

class idMaterial;
class idSoundShader;

enum swfFontEffects_t : int {
  SWF_FONT_FX_INVALID = -1,
  SWF_FONT_FX_OUTLINE_THIN_BLACK = 0,
  SWF_FONT_FX_OUTLINE_NORMAL_BLACK = 1,
  SWF_FONT_FX_OUTLINE_THICK_BLACK = 2,
  SWF_MAX_FONT_FX = 3
};

enum swfUsedFonts_t : int {
  SWF_FONT_INVALID = -1,
  SWF_FONT_FUTURA_COM_MEDIUM = 0,
  SWF_FONT_FUTURA_COM_BOLD = 1,
  SWF_FONT_ARIAL = 2,
  SWF_FONT_JAPANESE = 3,
  SWF_FONT_CYRILLIC = 4,
  SWF_MAX_FONTS = 5
};

class idDeclSwfResource final : public idDeclTypeInfo {
public:
  struct soundEntry_t {
    idStr name;
    const idSoundShader* sound;
  };

  struct musicEntry_t {
    idStr name;
    const idSoundShader* music;
    idStr channelName;
    soundChannel_t channel;
  };

  struct fontEntry_t {
    swfUsedFonts_t font;
    swfFontEffects_t effect;
    idMaterial* material;
  };

  struct loadMissionEntry_t {
    idAtomicString mapName;
    idMaterial* materialTo;
    idMaterial* materialFrom;
  };

  struct secretCategory_t {
    idStrId title1;
    idStrId locked;
    idStrId title2;
    idMaterial* material;
  };

  idDeclSwfResource();
  ~idDeclSwfResource() override;
  int FindSoundIndex(const idStr& soundName) const;
  idDeclInfo* GetDeclInfo() const override;

  idList<soundEntry_t> soundEffects;
  musicEntry_t music;
  idList<fontEntry_t> fontEffects;
  idList<loadMissionEntry_t> loadMissionEntries;
  idList<secretCategory_t> categoryDecls;
  secretCategory_t emptyCategory;
  bool snapToPixel;
  bool useBlackbars;

  static idDeclInfoTemplate<idDeclSwfResource> resourceList;
};

static_assert(sizeof(idDeclSwfResource::soundEntry_t) == 48,
              "idDeclSwfResource::soundEntry_t ABI drift");
static_assert(sizeof(idDeclSwfResource::musicEntry_t) == 96,
              "idDeclSwfResource::musicEntry_t ABI drift");
static_assert(sizeof(idDeclSwfResource::fontEntry_t) == 16,
              "idDeclSwfResource::fontEntry_t ABI drift");
static_assert(sizeof(idDeclSwfResource::loadMissionEntry_t) == 24,
              "idDeclSwfResource::loadMissionEntry_t ABI drift");
static_assert(sizeof(idDeclSwfResource::secretCategory_t) == 24,
              "idDeclSwfResource::secretCategory_t ABI drift");
static_assert(sizeof(idDeclSwfResource) == 320, "idDeclSwfResource ABI drift");
