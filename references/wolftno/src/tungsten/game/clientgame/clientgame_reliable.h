#pragma once

#include "tungsten/game/clientgame/clientgame_local.h"

#include <string>

class idClientGameMsg {
public:
  virtual ~idClientGameMsg();
  virtual int GetMsgType() const = 0;
  virtual void Serialize(idSerializer* serializer) = 0;
  virtual void Execute(idClientGame& game) = 0;
  virtual std::string PrintDebug() const = 0;
  virtual bool ShouldExecuteOnInstigator() const { return true; }
};

class idClientGameMsg_HandsEquip : public idClientGameMsg {
public:
  int GetMsgType()const override;void PrintDebugLine()const;std::string PrintDebug()const override;
  void Serialize(idSerializer*)override;void Execute(idClientGame&)override;
  int playerNumber{0};int itemId{-1};int ammoId{-1};
};
class idClientGameMsg_HandsAmmoChange : public idClientGameMsg {
public:
  int GetMsgType()const override;void PrintDebugLine()const;std::string PrintDebug()const override;
  void Serialize(idSerializer*)override;void Execute(idClientGame&)override;
  int playerNumber{0};int ammoId{-1};int amount{0};
};
class idClientGameMsg_DropItem : public idClientGameMsg {
public:
  int GetMsgType()const override;bool ShouldExecuteOnInstigator()const override;
  std::string PrintDebug()const override;void Serialize(idSerializer*)override;void Execute(idClientGame&)override;
  int playerNumber{0};int itemId{-1};int amount{1};
};
class idClientGameMsg_HandsThrow : public idClientGameMsg {
public:
  idClientGameMsg_HandsThrow();~idClientGameMsg_HandsThrow()override;
  int GetMsgType()const override;bool ShouldExecuteOnInstigator()const override;
  std::string PrintDebug()const override;void Serialize(idSerializer*)override;void Execute(idClientGame&)override;
  int playerNumber{0};idVec3 velocity{};
};
class idClientGameMsg_HitScanHit : public idClientGameMsg {
public:
  idClientGameMsg_HitScanHit();explicit idClientGameMsg_HitScanHit(int player);
  int GetMsgType()const override;std::string PrintDebug()const override;
  void Serialize(idSerializer*)override;void Execute(idClientGame&)override;
  void AddHit(const clientHitScan_t& hit);int playerNumber{0};idList<clientHitScan_t,5> hits;
};
class idClientGameMsg_QuickUse : public idClientGameMsg {
public:int GetMsgType()const override;std::string PrintDebug()const override;void Serialize(idSerializer*)override;void Execute(idClientGame&)override;int playerNumber{0};int itemId{-1};};
class idClientGameMsg_UseFocus : public idClientGameMsg {
public:int GetMsgType()const override;std::string PrintDebug()const override;void Serialize(idSerializer*)override;void Execute(idClientGame&)override;int playerNumber{0};int focusEntity{-1};};
class idClientGameMsg_MeleeHit : public idClientGameMsg {
public:int GetMsgType()const override;std::string PrintDebug()const override;void Serialize(idSerializer*)override;void Execute(idClientGame&)override;int attacker{-1};int victim{-1};float damage{0};};
class idClientGameMsg_ActorModifier : public idClientGameMsg {
public:int GetMsgType()const override;std::string PrintDebug()const override;void Serialize(idSerializer*)override;void Execute(idClientGame&)override;int actor{-1};float speed{1};float damage{1};bool disableOverdrive{false};};
class idClientGameMsg_VehicleCrosshairChangeTarget : public idClientGameMsg {
public:int GetMsgType()const override;std::string PrintDebug()const override;void Serialize(idSerializer*)override;void Execute(idClientGame&)override;int vehicle{-1};int target{-1};};
class idClientGameMsg_VehicleWeaponChange : public idClientGameMsg {
public:int GetMsgType()const override{return 44;}std::string PrintDebug()const override;void Serialize(idSerializer*)override;void Execute(idClientGame&)override;int vehicle{-1};int weapon{-1};};
class idClientGameMsg_UpdateStat : public idClientGameMsg {
public:int GetMsgType()const override;std::string PrintDebug()const override;void Serialize(idSerializer*)override;void Execute(idClientGame&)override;int player{-1};int stat{-1};int value{0};};

struct killAssistData_t { int player{-1};float damage{0};bool operator==(const killAssistData_t&o)const{return player==o.player&&damage==o.damage;} };
class idClientGameMsg_PostGameScoreRecap : public idClientGameMsg {
public:
  struct scoreItem_t { int player{-1};int score{0};bool operator==(const scoreItem_t&o)const{return player==o.player&&score==o.score;} };
  idClientGameMsg_PostGameScoreRecap();~idClientGameMsg_PostGameScoreRecap()override;
  int GetMsgType()const override;std::string PrintDebug()const override;void Serialize(idSerializer*)override;void Execute(idClientGame&)override;
  idList<scoreItem_t,5> scores;
};
class idClientGameMsg_PlayerKilled : public idClientGameMsg {
public:
  idClientGameMsg_PlayerKilled();~idClientGameMsg_PlayerKilled()override;
  int GetMsgType()const override;std::string PrintDebug()const override;void Serialize(idSerializer*)override;void Execute(idClientGame&)override;
  int killer{-1};int victim{-1};idList<killAssistData_t,5> assists;
};

extern idCVar cg_reliable_debug;
extern idCVar cg_projectile_ignoreGrenades;
extern idCVar cg_projectile_grenadeDeathDropVelocity;

