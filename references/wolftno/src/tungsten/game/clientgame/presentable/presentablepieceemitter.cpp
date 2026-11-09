#include "tungsten/game/clientgame/presentable/presentablepieceemitter.h"

#include <algorithm>

// DIA: idWorldDestructibleEntity::GetModelTransform
// DIA: idPresentablePieceEmitter::SetInitProperties
// DIA: idPresentablePieceEmitter::emitPieceItem_t::emitPieceItem_t
// DIA: idPresentablePieceEmitter::idPresentablePieceEmitter
// DIA: `idPresentablePieceEmitter::idPresentablePieceEmitter'::`1'::dtor$0
// DIA: idPresentablePieceEmitter::Present
// DIA: idPresentablePieceEmitter::Interpolate
// DIA: idPresentablePieceEmitter::EmitPiece
// DIA: idPresentablePieceEmitter::`scalar deleting destructor'
// DIA: `idPresentablePieceEmitter::`scalar deleting destructor''::`1'::dtor$0
// DIA: `idPresentablePieceEmitter::`scalar deleting destructor''::`1'::dtor$1
// DIA: idPresentablePieceEmitter::Serialize

namespace {
idMat3 IdentityAxis(){idMat3 axis{};axis[0]={1,0,0};axis[1]={0,1,0};axis[2]={0,0,1};return axis;}
}

void idWorldDestructibleEntity::GetModelTransform(idVec3& origin,idMat3& axis)const{origin={};axis=IdentityAxis();}

void idPresentablePieceEmitter::SetInitProperties(const std::string& impactTableName,float minBounceVelocity,int lifeSpan,float friction,float fovScale,float modelDepthHack,float collisionAgeDecay,float collisionVolumeDecay){
  impactTable=impactTableName;
  pieceMinBounceVelocity=std::max(0.0F,minBounceVelocity);
  pieceLifeSpan=std::max(0,lifeSpan);
  pieceFriction=std::max(0.0F,friction);
  emitFovScale=fovScale;
  emitModelDepthHack=modelDepthHack;
  pieceCollisionAgeDecay=std::max(0.0F,collisionAgeDecay);
  pieceCollisionVolumeDecay=std::max(0.0F,collisionVolumeDecay);
}

idPresentablePieceEmitter::emitPieceItem_t::emitPieceItem_t():axis(IdentityAxis()){}

idPresentablePieceEmitter::idPresentablePieceEmitter(int number,const std::string& model){SetEntityNumber(number);SetRenderModel(model);}
idPresentablePieceEmitter::~idPresentablePieceEmitter()=default;

void idPresentablePieceEmitter::Present(){idPresentable::Present();}

void idPresentablePieceEmitter::Interpolate(int currentTime,float fraction){
  (void)currentTime;
  idPresentable::Interpolate(fraction);
  if(activePieces>0)++simulationCount;
  else replayCount=0;
}

void idPresentablePieceEmitter::EmitPiece(int emitTime,const idVec3& position,const idMat3& axes,const idVec3& point,const idVec3& impulseValue){
  (void)emitTime;
  emitPieceBuffer[static_cast<std::size_t>(emitPieceBufferPos)]={};
  auto& piece=emitPieceBuffer[static_cast<std::size_t>(emitPieceBufferPos)];
  piece.pos=position;piece.axis=axes;piece.impulsePoint=point;piece.impulse=impulseValue;
  emitPieceBufferPos=(emitPieceBufferPos+1)%static_cast<int>(emitPieceBuffer.size());
  activePieces=std::min(activePieces+1,static_cast<int>(emitPieceBuffer.size()));
}

void idPresentablePieceEmitter::Serialize(idSerializer* serializer){
  if(serializer==nullptr)return;
  idAtomicString table{impactTable};serializer->SerializeString(&table);impactTable=table.value;
  serializer->SerializeFloat(pieceMinBounceVelocity);
  serializer->SerializeFloat(pieceCollisionAgeDecay);
  serializer->SerializeFloat(pieceCollisionVolumeDecay);
  serializer->SerializeInt(pieceLifeSpan);
  serializer->SerializeFloat(pieceFriction);
  serializer->SerializeFloat(emitFovScale);
  serializer->SerializeFloat(emitModelDepthHack);
  const int previousPosition=emitPieceBufferPos;
  serializer->SerializeInt(emitPieceBufferPos);
  emitPieceBufferPos=((emitPieceBufferPos%10)+10)%10;
  for(auto& piece:emitPieceBuffer){
    serializer->SerializeVec3(piece.pos);
    serializer->SerializeMat3(piece.axis);
    serializer->SerializeVec3(piece.impulsePoint);
    serializer->SerializeVec3(piece.impulse);
  }
  if(!serializer->writing){
    replayCount=(emitPieceBufferPos-previousPosition+10)%10;
    activePieces=std::min(10,activePieces+replayCount);
  }
}
