#include "tungsten/game/clientgame/presentable/presentable.h"

#include <algorithm>

// Complete recovered function inventory; C++ RAII owns retail cleanup funclets.
// DIA: idPresentableVehicle::ShouldEnableSphereCollision
// DIA: idPresentable::Delete
// DIA: idPresentable::PostSerializeRead
// DIA: idPresentable::SetClipModelContents
// DIA: idPresentable::GetCurrentSoundShader
// DIA: idPresentable::FreeSoundEmitter
// DIA: idPresentable::Show
// DIA: idPresentable::ShutdownFXMgr
// DIA: idPresentable::SetLocalClipModelContents
// DIA: idPresentable::SetParm
// DIA: idPresentable::SetParm
// DIA: idPresentable::SetParm
// DIA: idPresentable::Shutdown
// DIA: idPresentable::GetRenderModel_TreeAnimator
// DIA: idPresentable::GetRenderModel_DiscreteAnimation
// DIA: idPresentable::GetRenderModel_Particle
// DIA: idPresentable::UpdateClientCollision
// DIA: idPresentable::SetCustomMaterial
// DIA: idPresentable::InheritPropertiesFromModel
// DIA: idPresentable::UpdateModelTransform
// DIA: idPresentable::ClientOverrideClipContents
// DIA: idPresentable::GetSoundEmitter
// DIA: idPresentable::UpdateSound
// DIA: idPresentable::IsPlayingSound
// DIA: idPresentable::FadeSound
// DIA: idPresentable::FadePitch
// DIA: idPresentable::UpdateSound
// DIA: idPresentable::StartSoundShader
// DIA: idPresentable::SetSoundVolume
// DIA: idPresentable::SetSoundPitch
// DIA: idPresentable::ModifySound
// DIA: idPresentable::SetVolumeAdjustment
// DIA: idPresentable::SetClearVolumeAdjustment
// DIA: idPresentable::StopSound
// DIA: idPresentable::Hide
// DIA: idPresentable::ResetFXMgr
// DIA: idPresentable::StartFX
// DIA: idPresentable::LocalStartFX
// DIA: idPresentable::StopFX
// DIA: idPresentable::StopAllFX
// DIA: idPresentable::LocalStopAllFX
// DIA: idPresentable::UpdateFX
// DIA: idPresentable::UpdateFX
// DIA: idPresentable::IdentifyFriendFoeNeutral
// DIA: idPresentable::InitFXMgr
// DIA: idPresentable::ShutdownFXMgr
// DIA: idPresentable::SetTeam
// DIA: idPresentable::SetOwnerPlayerIndex
// DIA: idPresentable::CreateMoveableClipModel
// DIA: idPresentable::BecomeReplicated
// DIA: idPresentable::AddToEntityLUT
// DIA: idPresentable::RemoveFromEntityLUT
// DIA: idPresentable::GetBounds
// DIA: idPresentable::GetAbsBounds
// DIA: idPresentable::GetPlayerEntity
// DIA: idPresentable::IsTargetLockable
// DIA: idClipModelInfo::Clear
// DIA: idPresentable::GetOnlineUsableState
// DIA: idPresentable::GetOnlineModifiedCrosshairInfo
// DIA: idPresentable::GetShouldPresent
// DIA: idPresentable::GetParmBlock
// DIA: idBitMsg::ReadQuantizedVector<idVec3,32768,17>
// DIA: idPresentable::UpdateFX
// DIA: idPresentable::GetOnlineUsableText
// DIA: idPresentable::UpdateFX
// DIA: idPresentable::FreeRenderModel
// DIA: idPresentable::SetParent
// DIA: idPresentable::SerializeFX
// DIA: idPresentable::Interpolate
// DIA: idPresentable::ClientThink
// DIA: idPresentable::SetRenderModel
// DIA: idPresentable::SetClipModelInfo
// DIA: idPresentable::StartSoundShader
// DIA: idPresentable::InitFXMgr
// DIA: idPresentable::SetEntityNumber
// DIA: idBitMsg::WriteQuantizedVector<idVec3,32768,17>
// DIA: idPresentable::idPresentable
// DIA: `idPresentable::idPresentable'::`1'::dtor$0
// DIA: idPresentable::~idPresentable
// DIA: `idPresentable::~idPresentable'::`1'::dtor$0
// DIA: idPresentable::GetParent
// DIA: idPresentable::UpdateModelGroupMaster
// DIA: idPresentable::GetWorldTransformInternal
// DIA: idPresentable::ShouldSaveForTimeTrial
// DIA: idPresentable::`scalar deleting destructor'
// DIA: idPresentable::Present
// DIA: idPresentable::Serialize
// DIA: `idPresentable::Serialize'::`1'::dtor$0
// DIA: `idPresentable::Serialize'::`1'::dtor$1
// DIA: `idPresentable::Serialize'::`1'::dtor$2
// DIA: `idPresentable::Serialize'::`1'::dtor$3
// DIA: `idPresentable::Serialize'::`1'::dtor$4
// DIA: idPresentable::SetGroupMaster
// DIA: idPresentable::GetWorldTransform
// DIA: `dynamic initializer for 'cg_optimizePresents''
// DIA: `dynamic initializer for 'cg_optimizePresentsCountdown''
// DIA: `dynamic atexit destructor for 'cg_optimizePresents''
// DIA: `dynamic atexit destructor for 'cg_optimizePresentsCountdown''
#include <cstring>

void idPresentable::SetParm(const idDeclRenderParm* parm, float value) {
  floatParms["aiRobotGlow"] = value;
  aiRobotGlow = value;
  lastRenderParm = parm;
  lastRenderParmValue = value;
  ++renderParmSetCount;
}

void idClipModelInfo::Clear(){bounds={};contents=0;valid=false;}
void idPresentable::Delete(){deleted=true;Shutdown();}void idPresentable::PostSerializeRead(){UpdateModelTransform();}void idPresentable::SetClipModelContents(std::uint32_t value){clipInfo.contents=value;clipInfo.valid=true;}const std::string& idPresentable::GetCurrentSoundShader(int channel)const{static const std::string empty;const auto found=sounds.find(channel);return found!=sounds.end()?found->second.shader:empty;}void idPresentable::FreeSoundEmitter(){sounds.clear();}void idPresentable::Show(){hidden=false;}void idPresentable::ShutdownFXMgr(){effects.clear();fxInitialized=false;}void idPresentable::SetLocalClipModelContents(std::uint32_t value){SetClipModelContents(value);}void idPresentable::SetParm(const std::string& name,float value){floatParms[name]=value;}void idPresentable::SetParm(const std::string& name,int value){SetParm(name,static_cast<float>(value));}void idPresentable::SetParm(const std::string& name,const idVec3& value){vectorParms[name]=value;}void idPresentable::Shutdown(){StopAllFX();FreeSoundEmitter();FreeRenderModel();}
const std::string& idPresentable::GetRenderModel_TreeAnimator()const{return renderModel;}const std::string& idPresentable::GetRenderModel_DiscreteAnimation()const{return renderModel;}const std::string& idPresentable::GetRenderModel_Particle()const{return renderModel;}void idPresentable::UpdateClientCollision(){clipInfo.valid=!hidden;}void idPresentable::SetCustomMaterial(const std::string& value){customMaterial=value;}void idPresentable::InheritPropertiesFromModel(const idPresentable& model){renderModel=model.renderModel;customMaterial=model.customMaterial;clipInfo=model.clipInfo;}void idPresentable::UpdateModelTransform(){++updateModelTransformCount;}void idPresentable::ClientOverrideClipContents(std::uint32_t value){clipInfo.contents=value;}int idPresentable::GetSoundEmitter(){return entityNumber;}void idPresentable::UpdateSound(){}void idPresentable::UpdateSound(int time){(void)time;}bool idPresentable::IsPlayingSound(int channel)const{const auto found=sounds.find(channel);return found!=sounds.end()&&found->second.playing;}void idPresentable::FadeSound(int channel,float target,float seconds){(void)seconds;if(sounds.count(channel))sounds[channel].volume=target;}void idPresentable::FadePitch(int channel,float target,float seconds){(void)seconds;if(sounds.count(channel))sounds[channel].pitch=target;}int idPresentable::StartSoundShader(const std::string& shader,int channel){return StartSoundShader(shader,channel,1);}int idPresentable::StartSoundShader(const std::string& shader,int channel,float volume){sounds[channel]={shader,volume,1,true};return channel;}void idPresentable::SetSoundVolume(int channel,float volume){if(sounds.count(channel))sounds[channel].volume=volume;}void idPresentable::SetSoundPitch(int channel,float pitch){if(sounds.count(channel))sounds[channel].pitch=pitch;}void idPresentable::ModifySound(int channel,float volume,float pitch){SetSoundVolume(channel,volume);SetSoundPitch(channel,pitch);}void idPresentable::SetVolumeAdjustment(float value){volumeAdjustment=value;}void idPresentable::SetClearVolumeAdjustment(bool value){clearVolumeAdjustment=value;}void idPresentable::StopSound(int channel){if(sounds.count(channel))sounds[channel].playing=false;}void idPresentable::Hide(){hidden=true;}void idPresentable::ResetFXMgr(){effects.clear();fxInitialized=true;}int idPresentable::StartFX(const std::string& effect){effects.push_back(effect);return static_cast<int>(effects.size()-1);}int idPresentable::LocalStartFX(const std::string& effect){return StartFX(effect);}void idPresentable::StopFX(int handle){if(handle>=0&&handle<static_cast<int>(effects.size()))effects.erase(effects.begin()+handle);}void idPresentable::StopAllFX(){effects.clear();}void idPresentable::LocalStopAllFX(){StopAllFX();}void idPresentable::UpdateFX(){}void idPresentable::UpdateFX(int time){(void)time;}void idPresentable::UpdateFX(const idVec3& value){origin=value;}void idPresentable::UpdateFX(bool paused){(void)paused;}int idPresentable::IdentifyFriendFoeNeutral()const{return team;}void idPresentable::InitFXMgr(){fxInitialized=true;}void idPresentable::InitFXMgr(bool local){(void)local;InitFXMgr();}void idPresentable::ShutdownFXMgr(bool immediate){(void)immediate;ShutdownFXMgr();}void idPresentable::SetTeam(int value){team=value;}void idPresentable::SetOwnerPlayerIndex(int value){ownerPlayerIndex=value;}void idPresentable::CreateMoveableClipModel(const idBounds& value){clipInfo.bounds=value;clipInfo.valid=true;}void idPresentable::BecomeReplicated(){replicated=true;}void idPresentable::AddToEntityLUT(){inEntityLUT=true;}void idPresentable::RemoveFromEntityLUT(){inEntityLUT=false;}const idBounds& idPresentable::GetBounds()const{return clipInfo.bounds;}idBounds idPresentable::GetAbsBounds()const{idBounds result=clipInfo.bounds;result.b[0]+=origin;result.b[1]+=origin;return result;}idEntity* idPresentable::GetPlayerEntity()const{return playerEntity;}bool idPresentable::IsTargetLockable()const{return targetLockable;}int idPresentable::GetOnlineUsableState()const{return hidden?0:1;}std::string idPresentable::GetOnlineModifiedCrosshairInfo()const{return targetLockable?"lockable":"default";}bool idPresentable::GetShouldPresent()const{return !hidden&&!deleted;}const std::unordered_map<std::string,float>& idPresentable::GetParmBlock()const{return floatParms;}std::string idPresentable::GetOnlineUsableText()const{return GetOnlineUsableState()?"use":"";}void idPresentable::FreeRenderModel(){renderModel.clear();}void idPresentable::SetParent(idPresentable* value){parent=value;}std::vector<std::uint8_t> idPresentable::SerializeFX()const{return {static_cast<std::uint8_t>(effects.size())};}void idPresentable::Interpolate(float fraction){floatParms["interpolation"]=std::clamp(fraction,0.0F,1.0F);}void idPresentable::ClientThink(){UpdateSound();UpdateFX();}void idPresentable::SetRenderModel(const std::string& model){renderModel=model;}void idPresentable::SetClipModelInfo(const idClipModelInfo& info){clipInfo=info;}void idPresentable::SetEntityNumber(int value){entityNumber=value;}
idPresentable::idPresentable()=default;idPresentable::~idPresentable(){Shutdown();}idPresentable* idPresentable::GetParent()const{return parent;}void idPresentable::UpdateModelGroupMaster(){if(groupMaster!=nullptr){origin=groupMaster->origin;axis=groupMaster->axis;}}void idPresentable::GetWorldTransformInternal(idVec3& outOrigin,idMat3& outAxis)const{outOrigin=origin;outAxis=axis;}bool idPresentable::ShouldSaveForTimeTrial()const{return replicated&&!deleted;}void idPresentable::Present(){if(GetShouldPresent())++presentCount;}std::vector<std::uint8_t> idPresentable::Serialize()const{std::vector<std::uint8_t> data(sizeof(entityNumber)+sizeof(origin));std::memcpy(data.data(),&entityNumber,sizeof(entityNumber));std::memcpy(data.data()+sizeof(entityNumber),&origin,sizeof(origin));return data;}void idPresentable::SetGroupMaster(idPresentable* value){groupMaster=value;}void idPresentable::GetWorldTransform(idVec3& outOrigin,idMat3& outAxis)const{if(parent!=nullptr)parent->GetWorldTransform(outOrigin,outAxis);else GetWorldTransformInternal(outOrigin,outAxis);}

template<> idVec3 idBitMsg::ReadQuantizedVector<idVec3,32768,17>(){idVec3 result{};if(readOffset+sizeof(result)<=data.size()){std::memcpy(&result,data.data()+readOffset,sizeof(result));readOffset+=sizeof(result);}return result;}
template<> void idBitMsg::WriteQuantizedVector<idVec3,32768,17>(const idVec3& value){const auto* bytes=reinterpret_cast<const std::uint8_t*>(&value);data.insert(data.end(),bytes,bytes+sizeof(value));}

idCVar cg_optimizePresents("cg_optimizePresents","1",4,"optimize present calls");idCVar cg_optimizePresentsCountdown("cg_optimizePresentsCountdown","3",4,"optimized present countdown");
