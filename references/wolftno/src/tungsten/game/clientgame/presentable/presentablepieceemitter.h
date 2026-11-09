#pragma once

#include "tungsten/game/clientgame/presentable/presentableanimatedphysics.h"

#include <array>
#include <string>

class idWorldDestructibleEntity {
public:
  void GetModelTransform(idVec3& origin,idMat3& axis)const;
};

class idPresentablePieceEmitter : public idPresentable {
public:
  struct emitPieceItem_t {
    emitPieceItem_t();
    idVec3 pos{};
    idMat3 axis{};
    idVec3 impulsePoint{};
    idVec3 impulse{};
  };

  explicit idPresentablePieceEmitter(int entityNumber=-1,const std::string& renderModel={});
  ~idPresentablePieceEmitter()override;
  void SetInitProperties(const std::string& impactTableName,float minBounceVelocity,int lifeSpan,float friction,float fovScale,float modelDepthHack,float collisionAgeDecay,float collisionVolumeDecay);
  void Present();
  void Interpolate(int currentTime,float fraction);
  void EmitPiece(int emitTime,const idVec3& position,const idMat3& axes,const idVec3& impulsePoint,const idVec3& impulse);
  void Serialize(idSerializer* serializer);

  int GetBufferPosition()const{return emitPieceBufferPos;}
  const emitPieceItem_t& GetPiece(int index)const{return emitPieceBuffer[static_cast<std::size_t>(index)];}
  int GetActivePieceCount()const{return activePieces;}
  int GetSimulationCount()const{return simulationCount;}
  int GetReplayCount()const{return replayCount;}
  float GetPieceFriction()const{return pieceFriction;}
  int GetPieceLifeSpan()const{return pieceLifeSpan;}
  const std::string& GetImpactTable()const{return impactTable;}

private:
  std::array<emitPieceItem_t,10> emitPieceBuffer{};
  std::string impactTable;
  float pieceMinBounceVelocity{1.0F};
  int pieceLifeSpan{1};
  float pieceFriction{15.0F};
  float emitFovScale{1.0F};
  float emitModelDepthHack{1.0F};
  float pieceCollisionAgeDecay{0.0F};
  float pieceCollisionVolumeDecay{0.0F};
  int emitPieceBufferPos{0};
  int activePieces{0};
  int simulationCount{0};
  int replayCount{0};
};
