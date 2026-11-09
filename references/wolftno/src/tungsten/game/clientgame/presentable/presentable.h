#pragma once

#include "tungsten/game/game_runtime.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

class idDeclRenderParm;

class idBitMsg {
public:
  template <typename T, int Range, int Bits> T ReadQuantizedVector();
  template <typename T, int Range, int Bits> void WriteQuantizedVector(const T& value);
  void ResetRead() { readOffset=0; }
private:
  std::vector<std::uint8_t> data;
  std::size_t readOffset{0};
};

struct idClipModelInfo {
  void Clear();
  idBounds bounds{};
  std::uint32_t contents{0};
  bool valid{false};
};

class idPresentable {
public:
  idPresentable();
  virtual ~idPresentable();
  virtual idPresentable* GetAnimatedEntityInterface() { return nullptr; }
  virtual void* GetAnimatedTreeAnimator() { return nullptr; }
  void SetAIDeathState(bool lootable) {
    aiLootable = lootable;
    aiDeathInitialized = true;
  }
  bool IsAILootableForTest() const { return aiLootable; }
  bool IsAIDeathInitializedForTest() const { return aiDeathInitialized; }
  void SetModelFade(float value);
  void Delete(); void PostSerializeRead(); void SetClipModelContents(std::uint32_t value);
  const std::string& GetCurrentSoundShader(int channel) const; void FreeSoundEmitter();
  void Show(); void ShutdownFXMgr(); void SetLocalClipModelContents(std::uint32_t value);
  void SetParm(const std::string& name,float value); void SetParm(const std::string& name,int value); void SetParm(const std::string& name,const idVec3& value);
  void SetParm(const idDeclRenderParm* parm, float value);
  void SetSourceRenderModelForAI(idRenderModel* value) {
    sourceRenderModel = value;
  }
  idRenderModel* GetSourceRenderModelForAI() const {
    return sourceRenderModel;
  }
  void Shutdown(); const std::string& GetRenderModel_TreeAnimator()const; const std::string& GetRenderModel_DiscreteAnimation()const; const std::string& GetRenderModel_Particle()const;
  void UpdateClientCollision(); void SetCustomMaterial(const std::string& material); void InheritPropertiesFromModel(const idPresentable& model); void UpdateModelTransform();
  void ClientOverrideClipContents(std::uint32_t value); int GetSoundEmitter(); void UpdateSound(); void UpdateSound(int time); bool IsPlayingSound(int channel)const;
  void FadeSound(int channel,float target,float seconds); void FadePitch(int channel,float target,float seconds);
  int StartSoundShader(const std::string& shader,int channel); int StartSoundShader(const std::string& shader,int channel,float volume);
  void SetSoundVolume(int channel,float volume); void SetSoundPitch(int channel,float pitch); void ModifySound(int channel,float volume,float pitch);
  void SetVolumeAdjustment(float value); void SetClearVolumeAdjustment(bool value); void StopSound(int channel); void Hide(); void ResetFXMgr();
  int StartFX(const std::string& effect); int LocalStartFX(const std::string& effect); void StopFX(int handle); void StopAllFX(); void LocalStopAllFX();
  void UpdateFX(); void UpdateFX(int time); void UpdateFX(const idVec3& origin); void UpdateFX(bool paused);
  int IdentifyFriendFoeNeutral()const; void InitFXMgr(); void InitFXMgr(bool local); void ShutdownFXMgr(bool immediate); void SetTeam(int value); void SetOwnerPlayerIndex(int value);
  void CreateMoveableClipModel(const idBounds& value); void BecomeReplicated(); void AddToEntityLUT(); void RemoveFromEntityLUT();
  const idBounds& GetBounds()const; idBounds GetAbsBounds()const; idEntity* GetPlayerEntity()const; bool IsTargetLockable()const;
  int GetOnlineUsableState()const; std::string GetOnlineModifiedCrosshairInfo()const; bool GetShouldPresent()const; const std::unordered_map<std::string,float>& GetParmBlock()const;
  std::string GetOnlineUsableText()const; void FreeRenderModel(); void SetParent(idPresentable* value); std::vector<std::uint8_t> SerializeFX()const;
  void Interpolate(float fraction); void ClientThink(); void SetRenderModel(const std::string& model); void SetClipModelInfo(const idClipModelInfo& info);
  void SetEntityNumber(int value); idPresentable* GetParent()const; void UpdateModelGroupMaster(); void GetWorldTransformInternal(idVec3& origin,idMat3& axis)const;
  bool ShouldSaveForTimeTrial()const; void Present(); std::vector<std::uint8_t> Serialize()const; void SetGroupMaster(idPresentable* value); void GetWorldTransform(idVec3& origin,idMat3& axis)const;

  bool IsHidden()const{return hidden;} bool IsDeleted()const{return deleted;} int NumFX()const{return static_cast<int>(effects.size());} int GetPresentCount()const{return presentCount;}
  std::uint32_t GetClipModelContents()const{return clipInfo.contents;}
  int GetEntityNumber()const{return entityNumber;}
  const idMat3& GetAxis() const { return axis; }
  float GetFloatParmForTest(const std::string& name) const {
    const auto found = floatParms.find(name);
    return found != floatParms.end() ? found->second : 0.0F;
  }
  float GetModelFadeForTest() const { return modelFade; }
  int GetModelFadeSetCountForTest() const { return modelFadeSetCount; }
  int GetUpdateFXCountForTest() const { return updateFXCount; }
  int GetUpdateModelTransformCountForTest() const {
    return updateModelTransformCount;
  }
  int GetRenderParmSetCountForTest() const { return renderParmSetCount; }
  const idDeclRenderParm* GetLastRenderParmForTest() const {
    return lastRenderParm;
  }
  float GetLastRenderParmValueForTest() const {
    return lastRenderParmValue;
  }
  void SetAISphereModelAvailableForTest(bool value) {
    aiSphereModelAvailable = value;
  }
  bool HasAISphereModelForTest() const { return aiSphereModelAvailable; }
  void SetAISphereModelContents(std::uint32_t value) {
    if (aiSphereModelAvailable) aiSphereModelContents = value;
  }
  std::uint32_t GetAISphereModelContentsForTest() const {
    return aiSphereModelContents;
  }
  void SetOrigin(const idVec3& value){origin=value;} void SetAxis(const idMat3& value){axis=value;} void SetPlayerEntity(idEntity* value){playerEntity=value;} void SetTargetLockable(bool value){targetLockable=value;}

private:
  friend class idAI2;
  struct Sound { std::string shader; float volume{1}; float pitch{1}; bool playing{true}; };
  std::unordered_map<int,Sound> sounds; std::vector<std::string> effects; std::unordered_map<std::string,float> floatParms; std::unordered_map<std::string,idVec3> vectorParms;
  idClipModelInfo clipInfo; idPresentable* parent{nullptr}; idPresentable* groupMaster{nullptr}; idEntity* playerEntity{nullptr}; idRenderModel* sourceRenderModel{nullptr}; std::string renderModel; std::string customMaterial;
  idVec3 origin{}; idMat3 axis{}; int entityNumber{-1}; int team{0}; int ownerPlayerIndex{-1}; int presentCount{0}; float volumeAdjustment{1}; float aiRobotGlow{0.0F}; float modelFade{1.0F}; int modelFadeSetCount{0}; int updateFXCount{0}; int updateModelTransformCount{0}; int renderParmSetCount{0}; const idDeclRenderParm* lastRenderParm{nullptr}; float lastRenderParmValue{0.0F}; std::uint32_t aiSphereModelContents{0}; bool aiSphereModelAvailable{true}; bool clearVolumeAdjustment{false}; bool hidden{false}; bool deleted{false}; bool replicated{false}; bool inEntityLUT{false}; bool fxInitialized{false}; bool targetLockable{false}; bool aiLootable{false}; bool aiDeathInitialized{false};
};

class idPresentableVehicle;

extern idCVar cg_optimizePresents;
extern idCVar cg_optimizePresentsCountdown;
