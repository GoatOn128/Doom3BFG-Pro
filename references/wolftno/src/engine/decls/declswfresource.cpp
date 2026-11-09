#include "declswfresource.h"

idDeclSwfResource::idDeclSwfResource()
    : music{}, emptyCategory{}, snapToPixel(false), useBlackbars(false) {
  resourceListPtr = &resourceList;
  music.name = "music";
  music.music = nullptr;
  music.channelName = "channel";
  music.channel = SND_CHANNEL_ANY;
  emptyCategory.title1.Set("#str_empty");
  emptyCategory.title2.Set("#str_empty");
  emptyCategory.locked.Set("#str_empty");
  emptyCategory.material = nullptr;
}

idDeclSwfResource::~idDeclSwfResource() = default;

int idDeclSwfResource::FindSoundIndex(const idStr& soundName) const {
  for (int index = 0; index < soundEffects.Num(); ++index) {
    if (soundEffects[index].name == soundName)
      return index;
  }
  return -1;
}

idDeclInfo* idDeclSwfResource::GetDeclInfo() const {
  return &resourceList;
}
