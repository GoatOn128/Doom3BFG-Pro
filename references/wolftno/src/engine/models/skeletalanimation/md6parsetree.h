#pragma once

#include "engine/animation/md6.h"

#include <cstddef>
#include <cstdint>

struct frameSetData_t {
  std::uint16_t firstROffset;
  std::uint16_t firstSOffset;
  std::uint16_t firstTOffset;
  std::uint16_t firstUOffset;
  std::uint16_t rangeROffset;
  std::uint16_t rangeSOffset;
  std::uint16_t rangeTOffset;
  std::uint16_t rangeUOffset;
  std::uint16_t RBitsOffset;
  std::uint16_t SBitsOffset;
  std::uint16_t TBitsOffset;
  std::uint16_t UBitsOffset;
  std::uint16_t nextROffset;
  std::uint16_t nextSOffset;
  std::uint16_t nextTOffset;
  std::uint16_t nextUOffset;
  std::uint16_t totalSize;
  std::uint16_t frameStart;
  std::uint16_t frameRange;
  std::uint8_t pad[10];
};

struct md6FrameInfo_t {
  const idMD6AnimData* animData;
  const frameSetData_t* frameSetData;
  const idMD6Blend::jointMod_t* animMods;
  std::uint16_t flags;
  std::uint16_t weightGroup;
  std::uint16_t animInfoSize;
  std::uint16_t frameSetSize;
  std::uint16_t animModsSize;
  std::int16_t frame;
  float frameFraction;
  std::int16_t frameSetFrame;
  std::int16_t originFrame;
  float originFrameFraction;
  std::int16_t originFrameSetFrame;
  std::uint16_t originFrameSetSize;
  const frameSetData_t* originFrameSetData;
};

struct md6BlendInfo_t {
  std::uint8_t op;
  std::uint8_t filterGroup;
  std::uint8_t originBlend;
  std::uint8_t pad;
  float currentAlpha;
};

struct md6AnimCommand_t {
  enum op_t : std::int32_t {
    OP_NO = 0,
    OP_END = 1,
    OP_DECODE_AND_PUSH = 2,
    OP_DECODE_FACECOMPOSITE_AND_BLEND = 3,
    OP_POP_AND_BLEND = 4,
  } op;
  md6FrameInfo_t frameInfo;
  md6BlendInfo_t blendInfo;
  int debugCommand;
  const char* animName;
};

struct md6BlendOutputs_t {
  md6BlendOutputs_t()
      : deltaRotation{0.0f, 0.0f, 0.0f, 1.0f},
        deltaScale{1.0f, 1.0f, 1.0f, 1.0f}, deltaTranslation{},
        animRotation{0.0f, 0.0f, 0.0f, 1.0f},
        animScale{1.0f, 1.0f, 1.0f, 1.0f}, animTranslation{}, done(true),
        fudged(false), pad{}, time(-1),
        jointBounds{1.0e30f, 1.0e30f, 1.0e30f, -1.0e30f, -1.0e30f,
                    -1.0e30f} {}

  float deltaRotation[4];
  float deltaScale[4];
  float deltaTranslation[4];
  float animRotation[4];
  float animScale[4];
  float animTranslation[4];
  bool done;
  bool fudged;
  std::uint8_t pad[2];
  int time;
  float jointBounds[6];
};

using idMD6ParseTreeModelCompatible = bool (*)(const idMD6Anim*,
                                                const idMD6Model*);
using idMD6ParseTreeBranchUpdate = void (*)(idMD6Branch*, bool,
                                             int timeMSec, int ticksPerSec,
                                             const idMD6Skel* skeleton);
void SetMD6ParseTreeModelCompatible(idMD6ParseTreeModelCompatible compatible);
void SetMD6ParseTreeBranchUpdate(idMD6ParseTreeBranchUpdate update);
bool DispatchMD6ParseTreeBranchUpdate(idMD6Branch* branch, bool forceUpdate,
                                      int timeMSec, int ticksPerSec,
                                      const idMD6Skel* skeleton);

int ParseTree(const idMD6Model* model, int lastTimeMSec, int timeMSec,
              int ticksPerSec, idMD6Node* tree, md6AnimCommand_t* commands,
              int maxCommands, idBounds& frameBounds,
              idBounds& translatedBounds, idBounds& normalizedBounds,
              md6BlendOutputs_t* blendOutputs, std::uint16_t animatorFlags);

static_assert(sizeof(frameSetData_t) == 48);
static_assert(sizeof(md6FrameInfo_t) == 64);
static_assert(offsetof(md6FrameInfo_t, flags) == 24);
static_assert(offsetof(md6FrameInfo_t, frame) == 34);
static_assert(offsetof(md6FrameInfo_t, originFrameSetData) == 56);
static_assert(sizeof(md6BlendInfo_t) == 8);
static_assert(sizeof(md6AnimCommand_t) == 96);
static_assert(offsetof(md6AnimCommand_t, frameInfo) == 8);
static_assert(offsetof(md6AnimCommand_t, blendInfo) == 72);
static_assert(offsetof(md6AnimCommand_t, animName) == 88);
static_assert(sizeof(md6BlendOutputs_t) == 128);
