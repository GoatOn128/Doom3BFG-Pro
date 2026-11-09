#include "tungsten/game/clientgame/presentable/presentableweaponstatic.h"

#include <algorithm>
#include <cmath>

// DIA: idPresentableWeaponStatic::GetFiredWeapon
// DIA: idPresentableWeaponStatic::ReleaseTrigger
// DIA: idPresentableWeaponStatic::PostSerializeRead
// DIA: idPresentableWeaponStatic::Draw
// DIA: idPresentableWeaponStatic::Serialize
// DIA: idPresentableWeaponStatic::~idPresentableWeaponStatic
// DIA: `idPresentableWeaponStatic::~idPresentableWeaponStatic'::`1'::dtor$0
// DIA: `idPresentableWeaponStatic::~idPresentableWeaponStatic'::`1'::dtor$1
// DIA: idPresentableWeaponStatic::GetType
// DIA: idPresentableWeaponStatic::SetClientWeapon
// DIA: idList<barrelProps_t,5>::Resize
// DIA: idPresentableWeaponStatic::`scalar deleting destructor'
// DIA: idPresentableWeaponStatic::ClientThink
// DIA: idList<barrelProps_t,5>::Alloc
// DIA: idPresentableWeaponStatic::SetDefaults
// DIA: idPresentableWeaponStatic::idPresentableWeaponStatic
// DIA: `idPresentableWeaponStatic::idPresentableWeaponStatic'::`1'::dtor$0
// DIA: `idPresentableWeaponStatic::idPresentableWeaponStatic'::`1'::dtor$1

idPresentableWeaponStatic::idPresentableWeaponStatic()
    : idPresentableProp({}, false) {
  SetDefaults();
}

idPresentableWeaponStatic::~idPresentableWeaponStatic() {
  if (ownsClientWeapon) {
    delete clientWeapon;
  }
  clientWeapon = nullptr;
}

idWeapon* idPresentableWeaponStatic::GetFiredWeapon(
    const idDeclWeapon* weaponDefinition) const {
  return weaponDecl != nullptr && weaponDefinition == weaponDecl ? clientWeapon
                                                                 : nullptr;
}

void idPresentableWeaponStatic::ReleaseTrigger() {
  if (!shooting) {
    return;
  }
  shooting = false;
  StopSound(1);
  if (!sndFireStop.empty()) {
    StartSoundShader(sndFireStop, 2);
  }
  windupTimer = windupTime;
}

void idPresentableWeaponStatic::PostSerializeRead(bool firstClientFrame) {
  (void)firstClientFrame;
  if (weaponDecl != nullptr && clientWeapon == nullptr) {
    clientWeapon = new idWeapon(weaponDecl->index);
    ownsClientWeapon = true;
  }
}

bool idPresentableWeaponStatic::Draw(idPresentablePlayer* player) {
  if (player == nullptr) {
    return false;
  }
  ++drawCount;
  return true;
}

void idPresentableWeaponStatic::Serialize(idSerializer* serializer) {
  if (serializer == nullptr) {
    return;
  }
  serializer->SerializeFloat(minPitch);
  serializer->SerializeFloat(maxPitch);
  serializer->SerializeFloat(minYaw);
  serializer->SerializeFloat(maxYaw);
  serializer->SerializeFloat(windupTime);
  serializer->SerializeFloat(windupTimer);
  serializer->SerializeFloat(currentPitch);
  serializer->SerializeFloat(currentYaw);
  serializer->SerializeInt(lastThinkTime);
  serializer->SerializeInt(shotCount);
  serializer->SerializeBool(shooting);
  serializer->SerializeBool(triggerDown);
}

int idPresentableWeaponStatic::GetType() const { return 17; }

void idPresentableWeaponStatic::SetClientWeapon(idWeapon* weapon) {
  if (ownsClientWeapon) {
    delete clientWeapon;
  }
  clientWeapon = weapon;
  ownsClientWeapon = false;
}

void idPresentableWeaponStatic::ClientThink(int currentTime, float fraction,
                                            bool predict) {
  const int elapsedMs = lastThinkTime > 0 ? std::max(0, currentTime - lastThinkTime)
                                         : 0;
  const float elapsedSeconds = static_cast<float>(elapsedMs) * 0.001F;
  const float blend = std::clamp(fraction * turnScale, 0.0F, 1.0F);
  const float targetPitch = std::clamp(desiredPitch, minPitch, maxPitch);
  const float targetYaw = std::clamp(desiredYaw, minYaw, maxYaw);
  const float priorPitch = currentPitch;
  const float priorYaw = currentYaw;
  currentPitch += (targetPitch - currentPitch) * blend;
  currentYaw += (targetYaw - currentYaw) * blend;
  const bool moved = std::fabs(currentPitch - priorPitch) > 0.001F ||
                     std::fabs(currentYaw - priorYaw) > 0.001F;

  if (moved) {
    if (turning == 0 && !sndTurning.empty()) {
      StartSoundShader(sndTurning, 3);
    }
    turning = std::min(turning + 1, 4);
  } else {
    turning = 0;
    StopSound(3);
  }

  if (controller != nullptr && triggerDown) {
    if (!shooting) {
      shooting = true;
      windupTimer = windupTime;
      if (!sndFireLoop.empty()) {
        StartSoundShader(sndFireLoop, 1);
      }
    }
    windupTimer = std::max(0.0F, windupTimer - elapsedSeconds);
    if (windupTimer == 0.0F) {
      ++shotCount;
      windupTimer = windupTime;
    }
  } else {
    ReleaseTrigger();
  }

  zoomTime = std::clamp(zoomTime + (triggerDown ? -elapsedMs : elapsedMs),
                        0.0F, 180.0F);
  lastThinkTime = currentTime;
  ClientPredict(currentTime, fraction);
  (void)predict;
}

void idPresentableWeaponStatic::SetDefaults() {
  turnScale = 0.4F;
  fireInOrder = false;
  lookAt = {};
  lookAt[0] = {1.0F, 0.0F, 0.0F};
  lookAt[1] = {0.0F, 1.0F, 0.0F};
  lookAt[2] = {0.0F, 0.0F, 1.0F};
  turning = 0;
  weaponDecl = nullptr;
  controller = nullptr;
  sndTurning.clear();
  sndFireLoop.clear();
  sndFireStop.clear();
  zoomTime = 180.0F;
  windupTime = 0.5F;
  windupTimer = 0.0F;
  shooting = false;
  triggerDown = false;
  clipForwardAmount = 22.0F;
  lastThinkTime = 0;
  shotCount = 0;
  drawCount = 0;
  currentPitch = 0.0F;
  currentYaw = 0.0F;
  desiredPitch = 0.0F;
  desiredYaw = 0.0F;
  if (barrels.Num() == 0) {
    barrelProps_t* barrel = barrels.Alloc();
    *barrel = barrelProps_t{};
  }
}

template bool idList<barrelProps_t, 5>::Resize(int);
template barrelProps_t* idList<barrelProps_t, 5>::Alloc();

