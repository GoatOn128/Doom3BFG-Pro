#pragma once

#include "tungsten/game/clientgame/presentable/presentableplayer_shared.h"
#include "tungsten/game/clientgame/presentable/presentableprop.h"

#include <string>

struct idDeclWeapon {
  explicit idDeclWeapon(std::string weaponName = {}, int weaponIndex = 0)
      : name(std::move(weaponName)), index(weaponIndex) {}

  std::string name;
  int index{0};
};

struct barrelProps_t {
  std::string muzzleTag{"muzzle"};
  std::string ejectTag;
  bool primary{true};
  int muzzleTagIndex{-1};
  int ejectTagIndex{-1};

  bool operator==(const barrelProps_t& other) const {
    return muzzleTag == other.muzzleTag && ejectTag == other.ejectTag &&
           primary == other.primary;
  }
};

class idPresentableWeaponStatic : public idPresentableProp {
public:
  idPresentableWeaponStatic();
  ~idPresentableWeaponStatic() override;

  idWeapon* GetFiredWeapon(const idDeclWeapon* weaponDefinition) const;
  void ReleaseTrigger();
  void PostSerializeRead(bool firstClientFrame);
  bool Draw(idPresentablePlayer* player);
  void Serialize(idSerializer* serializer);
  int GetType() const;
  void SetClientWeapon(idWeapon* weapon);
  void ClientThink(int currentTime, float fraction, bool predict);
  void SetDefaults();

  void SetWeaponDecl(const idDeclWeapon* value) { weaponDecl = value; }
  void SetController(idPresentablePlayer* value) { controller = value; }
  void SetTriggerDown(bool value) { triggerDown = value; }
  void SetAimAngles(float pitch, float yaw) {
    desiredPitch = pitch;
    desiredYaw = yaw;
  }
  void SetAimLimits(float pitch, float yaw) {
    minPitch = maxPitch = pitch;
    minPitch = -pitch;
    minYaw = -yaw;
    maxYaw = yaw;
  }
  void SetWindupTime(float value) { windupTime = std::max(0.0F, value); }
  void SetSounds(std::string turningSound, std::string fireLoop,
                 std::string fireStop) {
    sndTurning = std::move(turningSound);
    sndFireLoop = std::move(fireLoop);
    sndFireStop = std::move(fireStop);
  }

  idWeapon* GetClientWeapon() const { return clientWeapon; }
  const idList<barrelProps_t, 5>& GetBarrels() const { return barrels; }
  float GetPitch() const { return currentPitch; }
  float GetYaw() const { return currentYaw; }
  bool IsShooting() const { return shooting; }
  int GetShotCount() const { return shotCount; }
  int GetDrawCount() const { return drawCount; }
  float GetWindupTimer() const { return windupTimer; }

private:
  idList<barrelProps_t, 5> barrels;
  const idDeclWeapon* weaponDecl{nullptr};
  idPresentablePlayer* controller{nullptr};
  idWeapon* clientWeapon{nullptr};
  std::string sndTurning;
  std::string sndFireLoop;
  std::string sndFireStop;
  idMat3 lookAt{};
  float minPitch{-90.0F};
  float maxPitch{90.0F};
  float minYaw{-180.0F};
  float maxYaw{180.0F};
  float desiredPitch{0.0F};
  float desiredYaw{0.0F};
  float currentPitch{0.0F};
  float currentYaw{0.0F};
  float turnScale{0.4F};
  float zoomTime{180.0F};
  float windupTime{0.5F};
  float windupTimer{0.0F};
  float clipForwardAmount{22.0F};
  int lastThinkTime{0};
  int turning{0};
  int shotCount{0};
  int drawCount{0};
  bool fireInOrder{false};
  bool shooting{false};
  bool triggerDown{false};
  bool ownsClientWeapon{false};
};

