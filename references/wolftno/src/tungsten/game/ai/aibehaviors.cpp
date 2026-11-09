#include "tungsten/game/ai/aibehaviors.h"

// DIA: idCoverCapabilities::~idCoverCapabilities
idCoverCapabilities::~idCoverCapabilities() = default;

// DIA: idCoverCapabilities::Clear
void idCoverCapabilities::Clear() {
  actionMask = 0;
  capabilityMask = 0;
}

// DIA: idCoverCapabilities::HasCapability
bool idCoverCapabilities::HasCapability(
    const idCoverCapabilities* other) const {
  if (other == nullptr) return false;
  return (actionMask & other->actionMask) != 0 ||
         (capabilityMask & other->capabilityMask) != 0;
}

// DIA: idCoverCapabilities::`scalar deleting destructor'

// DIA: idAIBehaviors::idAIBehaviors
idAIBehaviors::idAIBehaviors() = default;

// DIA: idCoverCapabilities::HasCapability
bool idCoverCapabilities::HasCapability(const idCoverActions* other) const {
  if (other == nullptr) return false;
  idCoverCapabilities converted;
  converted.SetActionMask(other->GetActionMask());
  converted.SetCapabilityMask(static_cast<std::uint8_t>(
      ((other->GetActionMask() & 0x01u) << 1u) |
      ((other->GetActionMask() & 0x02u) << 2u) |
      ((other->GetActionMask() & 0x04u) << 3u) |
      ((other->GetActionMask() & 0x08u) >> 1u) |
      ((other->GetActionMask() & 0x10u) >> 2u)));
  return HasCapability(&converted);
}

// DIA: `idCoverCapabilities::HasCapability'::`1'::dtor$0
