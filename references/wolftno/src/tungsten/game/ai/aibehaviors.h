#pragma once

#include "tungsten/game/ai/ai2_actions.h"

#include <cstdint>

class idCoverActions {
public:
  virtual ~idCoverActions() = default;
  void Clear() { actionMask &= 0xE0u; }
  void Init(const struct aas2Cover_t* cover);

  void SetActionMask(std::uint8_t value) {
    actionMask = static_cast<std::uint8_t>((actionMask & 0xE0u) |
                                           (value & 0x1Fu));
  }
  std::uint8_t GetActionMask() const { return actionMask & 0x1Fu; }
  std::uint8_t GetRawActionFlags() const { return actionMask; }
  void SetRawActionFlags(std::uint8_t value) { actionMask = value; }

protected:
  std::uint8_t actionMask{0};
};

class idCoverCapabilities : public idCoverActions {
public:
  ~idCoverCapabilities() override;

  void Clear();
  bool HasCapability(const idCoverCapabilities* other) const;
  bool HasCapability(const idCoverActions* other) const;
  void SetCapabilityMask(std::uint8_t value) { capabilityMask = value & 0x7Fu; }
  std::uint8_t GetCapabilityMask() const { return capabilityMask; }

private:
  std::uint8_t capabilityMask{0};
};

class idAIBehaviors {
public:
  idAIBehaviors();

  const void* decl{nullptr};
  const void* declBehaviorVoiceOver{nullptr};
  const void* declBehaviorVoiceOverChains{nullptr};
  const void* declBehaviorEvents{nullptr};
  const void* aimTrackingParameters{nullptr};
  const void* lookTrackingParameters{nullptr};
  float avoidRadius{0.0F};
};
