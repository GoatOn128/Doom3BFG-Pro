#include "engine/sound/declsoundmixing.h"

#include <cstdint>

namespace {

void InitializeCompressor(soundCompressorSettings_t& compressor) {
  compressor.enabled = false;
  compressor.sideChain = false;
  compressor.sideChainSourceBus = SSB_NONE;
  compressor.parms.ratio = 0.25F;
  compressor.parms.threshold = -5.0F;
  compressor.parms.attackTime = 0.1F;
  compressor.parms.releaseTime = 2.0F;
  compressor.parms.makeupGain = 1.0F;
  compressor.parms.rmsWindow = 0.005F;
  compressor.parms.useRMS = false;
}

void InitializeLimiter(soundLimiterSettings_t& limiter) {
  limiter.enabled = false;
  limiter.parms.threshold = 0.0F;
  limiter.parms.releaseTime = 1.0F;
}

}  // namespace

soundSubBusSettings_t::soundSubBusSettings_t() {
  InitializeCompressor(compressor);
  InitializeLimiter(limiter);
  filter.type = SND_FILTERTYPE_NONE;
  filter.frequency = 1000;
  output = SSB_MASTER;
  gain = 0.0F;
  wetGain = 0.0F;
}

idDeclSoundMixing::groups_t::groups_t() {
  auto* entries = reinterpret_cast<soundGroupSettings_t*>(this);
  for (int index = 0; index < 31; ++index) {
    entries[index].bus = SSB_MASTER;
  }
}

idDeclSoundMixing::idDeclSoundMixing() {
  InitializeCompressor(masterBus.compressor);
  InitializeLimiter(masterBus.limiter);
}

idDeclInfo* idDeclSoundMixing::GetDeclInfo() const {
  return &resourceList;
}

bool idDeclSoundMixing::SetImplicitText() {
  if (defaultSoundMixing != nullptr) {
    SetText(defaultSoundMixing->GetText(), defaultSoundMixing->textLength);
  }
  return true;
}

soundShaderBus_t idDeclSoundMixing::GetGroupRouting(soundShaderGroups_t group) const {
  if (group == SSG_MASTER) {
    return groups.master.bus;
  }
  const std::uint32_t bits = static_cast<std::uint32_t>(group);
  if (bits == 0 || (bits & (bits - 1)) != 0) {
    return SSB_NONE;
  }
  unsigned int bit = 0;
  for (std::uint32_t value = bits; value > 1; value >>= 1) {
    ++bit;
  }
  const auto* entries = reinterpret_cast<const soundGroupSettings_t*>(&groups);
  return entries[bit + 1].bus;
}

idDeclInfoTemplate<idDeclSoundMixing> idDeclSoundMixing::resourceList(
    "soundmixing", "idDeclSoundMixing", "def", false);
idDeclSoundMixing* idDeclSoundMixing::defaultSoundMixing = nullptr;
