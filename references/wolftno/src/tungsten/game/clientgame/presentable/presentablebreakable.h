#pragma once

#include "shared/idlib/containers/list.h"
#include "tungsten/game/clientgame/presentable/presentable.h"

#include <array>
#include <string>

struct breakableJoint_t {
  int joint{-1}; idVec3 localOrigin{}; bool active{true};
  bool operator==(const breakableJoint_t& other)const{return joint==other.joint;}
};

template <typename T> T* idListArrayNew(int count);
template <typename T> T* idListArrayResize(T* oldList,int oldCount,int newCount);

class idPresentableDamageable : public idPresentable {
public:
  struct rayBufferItem_t { idVec3 start{}; idVec3 end{}; };

  idPresentableDamageable();
  ~idPresentableDamageable() override;
  void Present();
  void SetInitProperties(const std::string& impactTableName,float minBounceVelocity);
  void StartExplosions();
  void ActivateAllNow();
  int GetType()const;
  void Interpolate(int currentTime,float fraction);
  void GetWorldTransform(idVec3& origin,idMat3& axis)const;
  void AddRayImpact(const idVec3& start,const idVec3& end);
  std::vector<std::uint8_t> Serialize()const;

  int GetRayImpactBufferPosition()const{return rayImpactBufferPos;}
  int GetExplosionFrame()const{return explosionFrame;}
  int GetActivateAllNowFrame()const{return activateAllNowFrame;}
  int GetSimulationCount()const{return simulationCount;}
  float GetPieceMinBounceVelocity()const{return pieceMinBounceVelocity;}
  const std::string& GetImpactTable()const{return impactTable;}
  const rayBufferItem_t& GetRayImpact(int index)const{return rayImpactBuffer[static_cast<std::size_t>(index)];}

private:
  std::array<rayBufferItem_t,10> rayImpactBuffer{};
  int rayImpactBufferPos{0};
  int explosionFrame{0};
  int currentExplosionFrame{0};
  int activateAllNowFrame{0};
  int currentActivateAllNowFrame{0};
  int simulationCount{0};
  float pieceMinBounceVelocity{1.0F};
  std::string impactTable;
};

class idPresentableBreakable : public idPresentableDamageable {
public:
  idPresentableBreakable(); ~idPresentableBreakable()override=default;
  void SetInitProperties(int healthValue,int fadeTime,int fadeDelay);
  void SetRenderModel(const std::string& model);
  void GibDefault(int currentTime); void GetWorldTransform(idVec3& origin,idMat3& axis)const;
  bool ShouldLinkPresentableCollision()const; void Break(const idPresentable* activator);
  bool PredictHitScanHit(const idVec3& start,const idVec3& direction,float range)const;
  void StopAllFX(); void GibWithActivator(int currentTime,const idPresentable* activator);
  void PostSerializeRead(bool firstClientFrame); std::vector<std::uint8_t> Serialize()const;
  void GibOrientedBodies(int currentTime,const idVec3& direction); void Interpolate(int currentTime,float fraction);
  breakableJoint_t* AddJoint(); bool IsBroken()const{return broken;} int NumJoints()const{return joints.Num();} const idVec3& GetGibImpulse()const{return gibImpulse;} int GetExplodeFrame()const{return explodeFrame;}
private:
  idList<breakableJoint_t,5> joints; idVec3 gibImpulse{}; int health{100}; int fadeTimeMs{0}; int fadeDelayMs{0}; int explodeFrame{0}; int currentExplodeFrame{0}; int brokenStartTime{0}; bool broken{false};
};

extern idCVar g_gibImpulse; extern idCVar g_defaultImpulse; extern idCVar presentable_breakable_debug; extern idCVar presentable_breakable_disablePredict;
