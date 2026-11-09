#include "declvoiceover.h"

#include <algorithm>

idDeclInfoTemplate<idDeclVoiceOver> idDeclVoiceOver::resourceList(
    "voiceOver", "idDeclVoiceOver", "tdef", false);
idDeclVoiceOver::idDeclVoiceOver() : nextSampleToPlay(-1) { resourceListPtr = &resourceList; }
idDeclVoiceOver::~idDeclVoiceOver() = default;
void idDeclVoiceOver::FreeData() { tracks.ClearFree(); }
idDeclInfo* idDeclVoiceOver::GetDeclInfo() const { return &resourceList; }
int idDeclVoiceOver::GetMaxLength() const { int length=0;for(int i=0;i<tracks.Num();++i)if(tracks[i])length=std::max(length,tracks[i]->approximateDuration);return length; }
std::uint64_t idDeclVoiceOver::Size() const { std::uint64_t size=128ULL+8ULL*tracks.size;for(int i=0;i<tracks.Num();++i)if(tracks[i])size+=tracks[i]->Memory()>440?tracks[i]->Memory()-440:0;return size; }
