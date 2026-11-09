#pragma once

#include "engine/guis/swf/swf_enums.h"
#include "engine/guis/swf/swf_resources.h"
#include "engine/guis/swf/swf_scriptobject.h"
#include "engine/guis/swf/swf_scriptfunction.h"
#include "shared/idlib/text/atomicstring.h"

#include <cstdint>

class idMaterial;
class idDeclSwfResource;
class idDeclRenderParm;
class idSoundWorld;
class idSoundEmitter;
class idSoundShader;
class idImage;
class idStr;
class idSWFSpriteInstance;
class idSWFTextInstance;
class idSWFScriptObject;
struct sysEvent_t;
struct millisecondUnique_t;

class idSWFDictionaryEntry {
public:
  idSWFDictionaryEntry();
  ~idSWFDictionaryEntry();
  idSWFDictionaryEntry& operator=(idSWFDictionaryEntry& other);
  idSWFDictionaryEntry& operator=(idSWFDictionaryEntry&& other) { return operator=(other); }

  swfDictType_t type;
  idMaterial* material;
  idSWFShape* shape;
  idSWFSprite* sprite;
  idSWFFont* font;
  idSWFText* text;
  idSWFEditText* edittext;
  idVec2i imageSize;
  idVec2i imageAtlasOffset;
  idVec4 channelScale;
};

class idSWF {
public:
  struct purgableSwfImage_t {
    idAtomicString imageName;
    std::uint32_t swfFrameNum{0};
  };

  static int mouseX;
  static int mouseY;
  struct imageToPack_t {
    int characterID;
    idVec2i trueSize;
    std::uint8_t* imageData;
    idVec2i allocSize;
  };
  class idDecompressJPEG {
  public:
    idDecompressJPEG();
    ~idDecompressJPEG();
    std::uint8_t* Load(const std::uint8_t* input, int inputSize, int& width, int& height);
    void* vinfo;
  };

  struct keyButtonImages_t {
    const char* key;
    const char* xb360Image;
    const char* xbOneImage;
    const char* ps3Image;
    const char* ps4Image;
    const idMaterial* xb360Material;
    const idMaterial* xbOneMaterial;
    const idMaterial* ps3Material;
    const idMaterial* ps4Material;
    int width;
    int height;
    int baseline;
  };

#define SWF_MAIN_FUNCTION(name)                                                       \
  class idSWFScriptFunction_##name : public idSWFScriptFunction_NestedSWF {           \
  public:                                                                              \
    idSWFScriptVar Call(idSWFScriptObject* object, idSWFParmList& parms) override;     \
  }
  class idSWFScriptFunction_Object : public idSWFScriptFunction_NestedSWF {
  public:
    idSWFScriptVar Call(idSWFScriptObject* object, idSWFParmList& parms) override;
    idSWFScriptObject object;
  };
  SWF_MAIN_FUNCTION(shortcutKeys_clear);
  SWF_MAIN_FUNCTION(soundObject_clear);
  SWF_MAIN_FUNCTION(deactivate);
  SWF_MAIN_FUNCTION(inhibitControl);
  SWF_MAIN_FUNCTION(useInhibit);
  SWF_MAIN_FUNCTION(precacheSound);
  SWF_MAIN_FUNCTION(precacheFontFXMaterial);
  SWF_MAIN_FUNCTION(playSound);
  SWF_MAIN_FUNCTION(stopSounds);
  SWF_MAIN_FUNCTION(getPlatform);
  SWF_MAIN_FUNCTION(getTruePlatform);
  SWF_MAIN_FUNCTION(getLocalString);
  SWF_MAIN_FUNCTION(swapPS3Buttons);
  SWF_MAIN_FUNCTION(getCVarInteger);
  SWF_MAIN_FUNCTION(setCVarInteger);
  SWF_MAIN_FUNCTION(getCVarString);
  SWF_MAIN_FUNCTION(setCVarString);
  SWF_MAIN_FUNCTION(strReplace);
  SWF_MAIN_FUNCTION(isJapanese);
  SWF_MAIN_FUNCTION(acos);
  SWF_MAIN_FUNCTION(cos);
  SWF_MAIN_FUNCTION(sin);
  SWF_MAIN_FUNCTION(round);
  SWF_MAIN_FUNCTION(pow);
  SWF_MAIN_FUNCTION(sqrt);
  SWF_MAIN_FUNCTION(abs);
  SWF_MAIN_FUNCTION(rand);
  SWF_MAIN_FUNCTION(floor);
  SWF_MAIN_FUNCTION(ceil);
  SWF_MAIN_FUNCTION(setColor);
  SWF_MAIN_FUNCTION(toUpper);
#undef SWF_MAIN_FUNCTION

#define SWF_MAIN_NATIVE(name)                                                         \
  class idSWFScriptNativeVar_##name : public idSWFScriptNativeVariable_NestedSWF {    \
  public:                                                                              \
    idSWFScriptVar Get(idSWFScriptObject* object) override;                           \
    void Set(idSWFScriptObject* object, const idSWFScriptVar& value) override;         \
  }
  class idSWFScriptNativeVar_platform : public idSWFScriptNativeVariable_NestedSWF {
  public:
    bool IsReadOnly() override { return true; }
    idSWFScriptVar Get(idSWFScriptObject* object) override;
    void Set(idSWFScriptObject* object, const idSWFScriptVar& value) override;
  };
  SWF_MAIN_NATIVE(blackbars);
  SWF_MAIN_NATIVE(crop);
  SWF_MAIN_NATIVE(automaticRescale);
#undef SWF_MAIN_NATIVE

  idSWF(const char* filename_, idSoundWorld* soundWorld = nullptr, bool atlasPurgable_ = false);
  ~idSWF();

  static const char* GetTagName(swfTag_t tag);
  static const char* GetActionName(swfAction_t action);
  bool Inflate(const std::uint8_t* input, unsigned int inputSize, std::uint8_t* output,
               unsigned int outputSize);
  bool LoadSWF(const char* fullpath, bool preload = true);
  bool LoadBinary(const char* bfilename);
  void WriteBinary(const char* bfilename);
  idSWFDictionaryEntry* AddDictionaryEntry(int characterID, swfDictType_t type);
  idSWFDictionaryEntry* FindDictionaryEntry(int characterID, swfDictType_t type);
  idSWFDictionaryEntry* FindDictionaryEntry(int characterID);
  void DefineShape(idSWFBitStream& bitstream);
  void DefineShape2(idSWFBitStream& bitstream);
  void DefineShape3(idSWFBitStream& bitstream);
  void DefineShape4(idSWFBitStream& bitstream);
  void DefineEditText(idSWFBitStream& bitstream);
  void DefineTextX(idSWFBitStream& bitstream, bool rgba);
  void DefineText(idSWFBitStream& bitstream);
  void DefineText2(idSWFBitStream& bitstream);
  void DefineFont2(idSWFBitStream& bitstream);
  void DefineFont3(idSWFBitStream& bitstream);
  void DefineSound(idSWFBitStream& bitstream);
  void JPEGTables(idSWFBitStream& bitstream);
  void DefineBits(idSWFBitStream& bitstream);
  void DefineBitsJPEG2(idSWFBitStream& bitstream);
  void DefineBitsJPEG3(idSWFBitStream& bitstream);
  void DefineBitsLossless(idSWFBitStream& bitstream);
  void DefineBitsLossless2(idSWFBitStream& bitstream);
  void LoadImageA(int characterID, const std::uint8_t* imageData, int width, int height);
  void WriteSwfImageAtlas(const char* filename_);
  void AddSpriteOrphan(const idSWFSpriteInstance* spriteInstance);
  void AddTextOrphan(const idSWFTextInstance* textInstance);
  idSWFSpriteInstance* ExtractSpriteFromOrphanage(idStr& name, const idSWFSprite* sprite,
                                                   int depth,
                                                   const idSWFSpriteInstance* parent);
  idSWFTextInstance* ExtractTextFromOrphanage(idStr& name, const idSWFEditText* editText,
                                               int depth,
                                               const idSWFSpriteInstance* parent);
  void PurgeOrphanage();
  idSWFScriptObject* HitTest(idSWFSpriteInstance* spriteInstance,
                             const swfRenderState_t& renderState, bool* overObject,
                             int x, int y, idSWFScriptObject* parentObject);
  bool HandleEvent(const sysEvent_t* event);
  idSWFScriptVar GetGlobal(const char* name) const;
  bool Invoke(const char* functionName, const idSWFParmList& parms);
  void SetListener(int id);
  void ClearEmitter(bool instant);
  void StopSounds(int channel);
  void Activate(int currentTime);
  void Deactivate();
  void AllocateEmitter(idSoundWorld* soundWorld);
  void Pause();
  void Resume();
  void SetGlobal(const char* name, const idSWFScriptVar& value);
  int GetFontEffect(int font, int effect) const;
  bool LoadFile(const char* filename_);
  bool IsPlayerControlInhibited() const;
  bool IsSoundChannelPlaying(int channel);
  bool IsPaused() const;
  static int GetPlatform();
  static const char* CrosshairIconMapper(int mapper);
  static void MarkImageForPurging(idImage* image);
  static void PurgeUnusedImages();
  millisecondUnique_t PlaySound(const idSoundShader* soundShader, int channel,
                                bool blocking = false);
  int FindFontFXMaterial(const char* materialName);
  static void RegisterSWFResources(const char* filename);
  void CreateAltasImageAndMaterial();
  std::uint64_t GLStateForRenderState(const swfRenderState_t& renderState) const;
  bool LoadAtlasImage(bool force);
  void RenderShape(class idRenderModelGui* gui, const idSWFShape* shape,
                   const swfRenderState_t& renderState);
  void RenderRadial(class idRenderModelGui* gui, idSWFSpriteInstance* spriteInstance,
                    const swfRenderState_t& renderState);
  void DrawEditCursor(class idRenderModelGui* gui, float x, float y, float z,
                      float width, float height, const swfMatrix_t& matrix);
  void RenderMorphShape(class idRenderModelGui* gui, const idSWFShape* shape,
                        float ratio, const swfRenderState_t& renderState);
  void DrawEditTextImages(const idList<struct swfGlyphIconInfo_t, 5>& imageList,
                          class idRenderModelGui* gui);
  void DrawEditTextSelections(
      const idList<struct swfGlyphSelectionInfo_t, 5>& selectionList,
      class idRenderModelGui* gui);
  void DrawEditTextGlyphs(const idList<struct swfGlyphDrawInfo_t, 5>& glyphList,
                          class idRenderModelGui* gui);
  void RenderMask(class idRenderModelGui* gui, const swfDisplayEntry_t* mask,
                  const swfRenderState_t& renderState, const idMaterial* material);
  void ReplaceTooltipIcons(idList<struct tooltipIcon_t, 72>& tooltipIconList,
                           idStr& text, const idList<int, 5>& bindsets,
                           int deviceNum);
  void RenderEditText(class idRenderModelGui* gui, idSWFTextInstance* textInstance,
                      const swfRenderState_t& renderState, int time,
                      bool isSplitscreen);
  void RenderSprite(class idRenderModelGui* gui, idSWFSpriteInstance* spriteInstance,
                    const swfRenderState_t& renderState, int time,
                    bool isSplitscreen);
  void Render(class idRenderModelGui* gui, int currentTime, bool isSplitscreen);

  static purgableSwfImage_t purgableSWFImages[8];
  static idList<idSWF*, 72> allSWFObjects;
  static std::uint32_t swfFrameNum;

  const idDeclSwfResource* swfResourceMenu;
  idSWFScriptObject* soundObject;
  idAtomicString filename;
  std::uint32_t timestamp;
  std::uint32_t recoveredTimestampPadding;
  idSWFScriptFunction_Object scriptFunction_Object;
  idSWFScriptObject* globals;
  idSWFSprite* mainsprite;
  idSWFSpriteInstance* mainspriteInstance;
  const idDeclRenderParm* parm;
  idSWFTextInstance* focusWindow;
  bool isActive;
  bool inhibitControl;
  bool useInhibitControl;
  bool arGui;
  bool pausedRender;
  std::uint8_t recoveredFramePadding[3];
  float frameWidth;
  float frameHeight;
  std::uint16_t frameRate;
  std::uint16_t recoveredRatePadding;
  float renderBorder;
  float swfScale;
  int lastRenderTime;
  bool cursorOverObject;
  bool showMouseCursor;
  bool mouseEnabled;
  bool useMouse;
  std::uint32_t recoveredMousePadding;
  idSWFScriptObject* mouseObject;
  idSWFScriptObject* hoverObject;
  bool blackbars;
  bool crop;
  bool paused;
  bool hasHitObject;
  bool automaticRescale;
  std::uint8_t recoveredFontEffectsPadding[3];
  int fontEffects[15];
  std::uint32_t recoveredShortcutPadding;
  idSWFScriptObject* shortcutKeys;
  idSoundEmitter* soundEmitter;
  idAtomicString atlasTextureName;
  const idMaterial* atlasMaterial;
  idImage* atlasImage;
  bool atlasPurgable;
  std::uint8_t recoveredAtlasPadding[3];
  int atlasWidth;
  int atlasHeight;
  std::uint32_t recoveredAtlasFilePadding;
  idFile* atlasFile;
  idSWFScriptFunction_shortcutKeys_clear scriptFunction_shortcutKeys_clear;
  idSWFScriptFunction_soundObject_clear scriptFunction_soundObject_clear;
  idSWFScriptFunction_deactivate scriptFunction_deactivate;
  idSWFScriptFunction_inhibitControl scriptFunction_inhibitControl;
  idSWFScriptFunction_useInhibit scriptFunction_useInhibit;
  idSWFScriptFunction_precacheSound scriptFunction_precacheSound;
  idSWFScriptFunction_precacheFontFXMaterial scriptFunction_precacheFontFXMaterial;
  idSWFScriptFunction_playSound scriptFunction_playSound;
  idSWFScriptFunction_stopSounds scriptFunction_stopSounds;
  idSWFScriptFunction_getPlatform scriptFunction_getPlatform;
  idSWFScriptFunction_getTruePlatform scriptFunction_getTruePlatform;
  idSWFScriptFunction_getLocalString scriptFunction_getLocalString;
  idSWFScriptFunction_swapPS3Buttons scriptFunction_swapPS3Buttons;
  idSWFScriptFunction_getCVarInteger scriptFunction_getCVarInteger;
  idSWFScriptFunction_setCVarInteger scriptFunction_setCVarInteger;
  idSWFScriptFunction_getCVarString scriptFunction_getCVarString;
  idSWFScriptFunction_setCVarString scriptFunction_setCVarString;
  idSWFScriptFunction_strReplace scriptFunction_strReplace;
  idSWFScriptFunction_isJapanese scriptFunction_isJapanese;
  idSWFScriptFunction_acos scriptFunction_acos;
  idSWFScriptFunction_cos scriptFunction_cos;
  idSWFScriptFunction_sin scriptFunction_sin;
  idSWFScriptFunction_round scriptFunction_round;
  idSWFScriptFunction_pow scriptFunction_pow;
  idSWFScriptFunction_sqrt scriptFunction_sqrt;
  idSWFScriptFunction_abs scriptFunction_abs;
  idSWFScriptFunction_rand scriptFunction_rand;
  idSWFScriptFunction_floor scriptFunction_floor;
  idSWFScriptFunction_ceil scriptFunction_ceil;
  idSWFScriptFunction_setColor scriptFunction_setColor;
  idSWFScriptFunction_toUpper scriptFunction_toUpper;
  idSWFScriptNativeVar_platform swfScriptVar_platform;
  idSWFScriptNativeVar_blackbars swfScriptVar_blackbars;
  idSWFScriptNativeVar_crop swfScriptVar_crop;
  idSWFScriptNativeVar_automaticRescale swfScriptVar_automaticRescale;
  idList<idSWFDictionaryEntry, 72> dictionary;
  idList<keyButtonImages_t, 72> tooltipButtonImage;
  const idMaterial* guiSolid;
  const idMaterial* guiStencilIncr;
  const idMaterial* guiStencilDecr;
  const idMaterial* guiCursor_arrow;
  const idMaterial* guiCursor_hand;
  const idMaterial* white;
  idList<const idMaterial*, 72> fontFXMaterials;
  idDecompressJPEG jpeg;
  idList<imageToPack_t, 72> packImages;
  idList<idSWFSpriteInstance*, 72> orphanedSprites;
  idList<idSWFTextInstance*, 72> orphanedTexts;
};

static_assert(sizeof(idSWFDictionaryEntry) == 88, "idSWFDictionaryEntry ABI drift");
static_assert(sizeof(idSWF::imageToPack_t) == 32, "SWF packed image ABI drift");
static_assert(sizeof(idSWF::idDecompressJPEG) == 8, "SWF JPEG decoder ABI drift");
static_assert(sizeof(idSWF) == 4480, "idSWF ABI drift");
