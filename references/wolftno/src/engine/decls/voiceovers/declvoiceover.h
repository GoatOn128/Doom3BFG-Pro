#pragma once

#include "engine/decls/decl.h"
#include "voicetrack.h"

class idDeclVoiceOver final : public idDeclTypeInfo {
public:
  idDeclVoiceOver();
  ~idDeclVoiceOver() override;
  void FreeData() override;
  idDeclInfo* GetDeclInfo() const override;
  std::uint64_t Size() const override;
  int GetMaxLength() const;

  std::int8_t nextSampleToPlay;
  idList<const idVoiceTrack*, 65> tracks;
  static idDeclInfoTemplate<idDeclVoiceOver> resourceList;
};

static_assert(sizeof(idDeclVoiceOver)==128,"voice-over decl ABI drift");
