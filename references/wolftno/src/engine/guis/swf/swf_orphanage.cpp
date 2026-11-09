#include "engine/guis/swf/swf.h"

#include "engine/guis/swf/swf_spriteinstance.h"
#include "engine/guis/swf/swf_textinstance.h"
#include "shared/idlib/text/str.h"

idSWFSpriteInstance* idSWF::ExtractSpriteFromOrphanage(
    idStr& objectName, const idSWFSprite* sprite, int depth,
    const idSWFSpriteInstance* parent) {
  for (int i = 0; i < orphanedSprites.Num(); ++i) {
    idSWFSpriteInstance* instance = orphanedSprites[i];
    if (instance->parent != parent || instance->sprite != sprite)
      continue;
    const bool matches = instance->name.str != nullptr && instance->name.str[0] != '\0'
                             ? idStr::Cmp(instance->name.str, objectName.data) == 0
                             : instance->depth == depth;
    if (!matches)
      continue;
    --orphanedSprites.num;
    if (i != orphanedSprites.num)
      orphanedSprites[i] = orphanedSprites[orphanedSprites.num];
    if (instance->isPlaying)
      instance->Play();
    return instance;
  }
  return nullptr;
}

idSWFTextInstance* idSWF::ExtractTextFromOrphanage(
    idStr& objectName, const idSWFEditText* editText, int depth,
    const idSWFSpriteInstance* parent) {
  for (int i = 0; i < orphanedTexts.Num(); ++i) {
    idSWFTextInstance* instance = orphanedTexts[i];
    if (instance->parent != parent)
      continue;
    const bool matches = instance->name.str != nullptr && instance->name.str[0] != '\0'
                             ? idStr::Cmp(objectName.data, instance->name.str) == 0
                             : instance->depth == depth;
    if (!matches)
      continue;
    --orphanedTexts.num;
    if (i != orphanedTexts.num)
      orphanedTexts[i] = orphanedTexts[orphanedTexts.num];
    instance->editText = const_cast<idSWFEditText*>(editText);
    instance->ReInit();
    return instance;
  }
  return nullptr;
}

void idSWF::PurgeOrphanage() {
  for (int i = 0; i < orphanedSprites.Num(); ++i)
    delete orphanedSprites[i];
  orphanedSprites.ClearFree();
  for (int i = 0; i < orphanedTexts.Num(); ++i)
    delete orphanedTexts[i];
  orphanedTexts.ClearFree();
}

void idSWF::AddSpriteOrphan(const idSWFSpriteInstance* spriteInstance) {
  orphanedSprites.Append(const_cast<idSWFSpriteInstance*>(spriteInstance));
}

void idSWF::AddTextOrphan(const idSWFTextInstance* textInstance) {
  orphanedTexts.Append(const_cast<idSWFTextInstance*>(textInstance));
}
