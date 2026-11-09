#include "engine/guis/swf/swf_spriteinstance.h"

#include "engine/guis/swf/swf.h"
#include "engine/guis/swf/swf_bitstream.h"
#include "engine/guis/swf/swf_textinstance.h"

namespace {
int c_PlaceObject2 = 0;
int c_PlaceObject3 = 0;
constexpr std::uint64_t PlaceMove = 1;
constexpr std::uint64_t PlaceCharacter = 2;
constexpr std::uint64_t PlaceMatrix = 4;
constexpr std::uint64_t PlaceColor = 8;
constexpr std::uint64_t PlaceRatio = 0x10;
constexpr std::uint64_t PlaceName = 0x20;
constexpr std::uint64_t PlaceClipDepth = 0x40;
} // namespace

swfDisplayEntry_t* idSWFSpriteInstance::ExecutePlaceObject2(const placeObject2Data_t& po2) {
  swfDisplayEntry_t* display = nullptr;
  if ((po2.flags & PlaceMove) == 0 && (po2.flags & PlaceCharacter) != 0) {
    if (po2.characterID < 0)
      return nullptr;
    display = AddDisplayEntry(po2.depth, po2.characterID,
                              const_cast<idStr&>(po2.name));
    if (display == nullptr)
      return nullptr;
  } else if ((po2.flags & PlaceMove) != 0) {
    display = FindDisplayEntry(po2.depth);
    if (display == nullptr)
      return nullptr;
    if (po2.characterID >= 0) {
      if (display->spriteInstance != nullptr || display->textInstance != nullptr)
        return nullptr;
      idSWFDictionaryEntry* dictionaryEntry = swf->FindDictionaryEntry(po2.characterID);
      if (dictionaryEntry != nullptr &&
          (dictionaryEntry->type == SWF_DICT_SPRITE ||
           dictionaryEntry->type == SWF_DICT_EDITTEXT))
        return nullptr;
      display->characterID = static_cast<std::uint16_t>(po2.characterID);
    }
  }

  if (display == nullptr)
    return nullptr;
  if ((po2.flags & PlaceMatrix) != 0)
    display->matrix = po2.matrix;
  if ((po2.flags & PlaceColor) != 0)
    display->cxf = po2.cxf;
  if ((po2.flags & PlaceRatio) != 0)
    display->ratio = po2.ratio;
  if ((po2.flags & PlaceName) != 0) {
    if (po2.characterID < 0)
      return nullptr;
    if (display->spriteInstance != nullptr)
      display->spriteInstance->name.Set(po2.name.data);
    else if (display->textInstance != nullptr)
      display->textInstance->name.Set(po2.name.data);
  }
  if ((po2.flags & PlaceClipDepth) != 0)
    display->clipDepth = po2.clipDepth;
  return display;
}

void idSWFSpriteInstance::PlaceObject2(idSWFBitStream& bitstream) {
  ++c_PlaceObject2;
  placeObject2Data_t po2;
  po2.flags = bitstream.ReadU8();
  po2.depth = bitstream.ReadU16();
  if ((po2.flags & PlaceCharacter) != 0)
    po2.characterID = bitstream.ReadU16();
  if ((po2.flags & PlaceMatrix) != 0)
    bitstream.ReadMatrix(po2.matrix);
  if ((po2.flags & PlaceColor) != 0)
    bitstream.ReadColorXFormRGBA(po2.cxf);
  if ((po2.flags & PlaceRatio) != 0)
    po2.ratio = bitstream.ReadU16() * (1.0f / 65535.0f);
  if ((po2.flags & PlaceName) != 0)
    po2.name = bitstream.ReadString();
  if ((po2.flags & PlaceClipDepth) != 0)
    po2.clipDepth = bitstream.ReadU16();
  ExecutePlaceObject2(po2);
}

void idSWFSpriteInstance::PlaceObject3(idSWFBitStream& bitstream) {
  ++c_PlaceObject3;
  placeObject2Data_t po2;
  const std::uint8_t flags1 = bitstream.ReadU8();
  const std::uint8_t flags2 = bitstream.ReadU8();
  po2.flags = flags1;
  po2.depth = bitstream.ReadU16();
  if ((flags2 & 8) != 0 || ((flags2 & 0x10) != 0 && (flags1 & PlaceCharacter) != 0))
    bitstream.ReadString();
  if ((flags1 & PlaceCharacter) != 0)
    po2.characterID = bitstream.ReadU16();
  if ((flags1 & PlaceMatrix) != 0)
    bitstream.ReadMatrix(po2.matrix);
  if ((flags1 & PlaceColor) != 0)
    bitstream.ReadColorXFormRGBA(po2.cxf);
  if ((flags1 & PlaceRatio) != 0)
    po2.ratio = bitstream.ReadU16() * (1.0f / 65535.0f);
  if ((flags1 & PlaceName) != 0)
    po2.name = bitstream.ReadString();
  if ((flags1 & PlaceClipDepth) != 0)
    po2.clipDepth = bitstream.ReadU16();
  if ((flags2 & 1) != 0)
    return;
  const bool hasBlendMode = (flags2 & 2) != 0;
  const std::uint8_t blendMode = hasBlendMode ? bitstream.ReadU8() : 0;
  swfDisplayEntry_t* display = ExecutePlaceObject2(po2);
  if (display != nullptr && hasBlendMode)
    display->blendMode = blendMode;
}

void idSWFSpriteInstance::RemoveObject2(idSWFBitStream& bitstream) {
  RemoveDisplayEntry(bitstream.ReadU16());
}
