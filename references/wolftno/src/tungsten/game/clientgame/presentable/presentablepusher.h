#pragma once

#include "tungsten/game/clientgame/presentable/presentableanimatedphysics.h"

class idPresentablePusher : public idPresentable {
public:
  explicit idPresentablePusher(int ownerEntityNumber=-1,const std::string& renderModel={});
  ~idPresentablePusher()override;
  void DisableClip(bool disable);
  int GetType()const;
  void InitPhysics(bool isObstacle);
  void Serialize(idSerializer* serializer);
  void UpdateClientCollision(const idVec3& oldOrigin,const idMat3& oldAxis,const idVec3& newOrigin,const idMat3& newAxis);
  void SetPushEnabled(bool value){pushEnabled=value;}
  bool IsPushEnabled()const{return pushEnabled;}
  bool IsPhysicsInitialized()const{return physicsInit;}
  bool IsClipEnabled()const{return clipEnabled;}
  int GetPushCount()const{return pushCount;}
  const idVec3& GetPhysicsOrigin()const{return physicsOrigin;}
private:
  idVec3 physicsOrigin{};idMat3 physicsAxis{};
  bool pushEnabled{false};bool physicsInit{false};bool clipEnabled{true};bool obstacle{false};int pushCount{0};
};

extern idCVar pp_disablePush;
