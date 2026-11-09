#include "tungsten/game/clientgame/presentable/presentableplayer_shared.h"
#include "tungsten/game/clientgame/presentable/presentablevehiclecar.h"

#include <cmath>

// Complete recovered inventory for presentableplayer_shared.cpp.
// DIA: idPresentableVehicle::StartSoundShader
// DIA: idPresentablePlayer::IsZoomedIn
// DIA: idPresentablePlayer::GetPendingWeapon
// DIA: idPresentablePlayer::Reload
// DIA: idPresentablePlayer::ForceWeaponSelectionUpdate
// DIA: idPresentablePlayer::ClearPendingAmmo
// DIA: idPresentablePlayer::GetEquippedWeapon
// DIA: idPresentablePlayer::StartSoundShader
// DIA: idPresentablePlayer::StartSoundShader_Predicted
// DIA: idPresentablePlayer::StopSound_Predicted
// DIA: idPresentablePlayer::SetSoundVolume
// DIA: idPresentablePlayer::PlayLocalSound
// DIA: idPresentablePlayer::GetDecayParams
// DIA: idPresentablePlayer::GetInventory
// DIA: idPresentablePlayer::GetPlayerSounds
// DIA: idPresentablePlayer::DisplayDeathMessage
// DIA: idPresentablePlayer::WeaponSelectionBlocksInput
// DIA: idPresentablePlayer::GetPeerIndex
// DIA: idPresentablePlayer::GetPeerMaskForPrediction
// DIA: idSpring<idVec1>::Update
// DIA: idViewCallbacks::`scalar deleting destructor'
// DIA: idWeapon::UsesBatteryAmmo
// DIA: idHands::BlockZoom
// DIA: viewDof_t::Clamp
// DIA: idPresentablePlayer::Init
// DIA: idPresentablePlayer::PresentableRespawn
// DIA: idPresentablePlayer::StartPerfectOrigin
// DIA: idPresentablePlayer::DecayOriginError
// DIA: idPresentablePlayer::GetViewAngles
// DIA: idPresentablePlayer::GetRecoilAngles
// DIA: idPresentablePlayer::SetLocalViewLimits
// DIA: idPresentablePlayer::ResetLocalViewLimits
// DIA: idPresentablePlayer::DefaultFov
// DIA: idPresentablePlayer::CalcFov
// DIA: idPresentablePlayer::CheckFovModified
// DIA: idPresentablePlayer::GetNextAmmoDecl
// DIA: idPresentablePlayer::ClampAdditionalPitchOffset
// DIA: idPresentablePlayer::ReleaseWeaponKickRecoveryDelay
// DIA: idPresentablePlayer::SetQuickslotStatus
// DIA: idPresentablePlayer::UpdateEnemyDanger
// DIA: idPresentablePlayer::UpdateAmmoGroup
// DIA: idPresentablePlayer::ClearAmmoStatus
// DIA: idPresentablePlayer::SetupWeaponSelectAmmo
// DIA: idPresentablePlayer::SelectQuickAmmo
// DIA: idPresentablePlayer::SelectQuickWeapon
// DIA: idPresentablePlayer::UpdateSound
// DIA: idPresentablePlayer::ClientPredictTriggers
// DIA: idPresentablePlayer::TransferDWAmmo
// DIA: idPresentablePlayer::ToggleDualWieldItem
// DIA: idPresentablePlayer::EquipItem
// DIA: idPresentablePlayer::GetEquipped
// DIA: idPresentablePlayer::IsDead
// DIA: idPresentablePlayer::IsCrouching
// DIA: idPresentablePlayer::GetFocusFriendly
// DIA: idPresentablePlayer::DeployableAllowed
// DIA: idPresentablePlayer::GetFireTrajectory
// DIA: idPresentablePlayer::ClearEquipped
// DIA: idPresentablePlayer::BeforeUse
// DIA: idPresentablePlayer::BeforeUseIsRunning
// DIA: idPresentablePlayer::SetAngles
// DIA: idPresentablePlayer::IsPlayerControlInhibited
// DIA: idPresentablePlayer::InhibitFire
// DIA: idPresentablePlayer::CheckInhibitFire
// DIA: idPresentablePlayer::GetControlWeapon
// DIA: idPresentablePlayer::SetControllerShake
// DIA: idPresentablePlayer::ShowInGameGuiScreen
// DIA: idPresentablePlayer::SetWalkState
// DIA: idPresentablePlayer::PlayFootStepEffect
// DIA: idPresentablePlayer::PresentableDamaged
// DIA: idPresentablePlayer::UpdateEditEntityMode
// DIA: idPresentablePlayer::IsFullyZoomedIn
// DIA: idPresentablePlayer::IsFullyZoomedOut
// DIA: idPresentablePlayer::GetZoomBlend
// DIA: idPresentablePlayer::ApplyBlendedAdditiveCamera
// DIA: idPresentablePlayer::StartCameraShake
// DIA: idPresentablePlayer::HandleCameraShake
// DIA: idPresentablePlayer::SetExplicitMove
// DIA: idSpring<idVec1>::SetConstants
// DIA: idWeapon::UsesBatteryAmmo
// DIA: viewDof_t::Mix
// DIA: idPresentablePlayer::ChangeCurrentZoomLevel
// DIA: idPresentablePlayer::SetCurrentZoomLevel
// DIA: idPresentablePlayer::ToggleZoom
// DIA: idPresentablePlayer::SetupZoom
// DIA: idPresentablePlayer::UpdateStepUpSprings
// DIA: idPresentablePlayer::GetEyeOffset
// DIA: idPresentablePlayer::SelectWeapon
// DIA: idPresentablePlayer::SelectWeaponBySelectIndex
// DIA: idPresentablePlayer::PrevEquipWeapon
// DIA: idPresentablePlayer::ClearWeaponKick
// DIA: idPresentablePlayer::ClearSavedPitchForKick
// DIA: idPresentablePlayer::GetCurWeaponSpread
// DIA: idPresentablePlayer::CalcCurWeaponSpread
// DIA: idPresentablePlayer::SetAmmoStatusControlled
// DIA: idPresentablePlayer::SetAmmoStatus
// DIA: idPresentablePlayer::UpdateWeaponSelectionCurrent
// DIA: idPresentablePlayer::SetupWeaponSelectionElement
// DIA: idPresentablePlayer::WeaponFireFeedback
// DIA: idPresentablePlayer::GetEquippedWeapon
// DIA: idPresentablePlayer::ShowInGameNewsPaperGuiScreen
// DIA: idPresentablePlayer::DamageDealtFeedback
// DIA: idPresentablePlayer::UpdateDecal
// DIA: `idPresentablePlayer::UpdateDecal'::`1'::dtor$0
// DIA: idPresentablePlayer::GetMovementScale
// DIA: idPresentablePlayer::GetPlayerInputSettings
// DIA: idFocusTracker::GetFocusEntity
// DIA: idPlayer::GetFocusEntity
// DIA: idPresentablePlayer::PlayerUpdateZoomState
// DIA: idPresentablePlayer::OffsetThirdPersonView
// DIA: idPresentablePlayer::UpdateWeaponKick
// DIA: idPresentablePlayer::UpdateWeaponSelection
// DIA: idPresentablePlayer::WeaponFireFeedback
// DIA: idPresentablePlayer::GetFocusEntity
// DIA: idPresentablePlayer::UniqueProjectileAllowed
// DIA: idPresentablePlayer::Use
// DIA: idPresentablePlayer::AddAttacker
// DIA: idPresentablePlayer::UpdateDamageDealt
// DIA: idPresentablePlayer::UpdateDamageFeedback
// DIA: idPlayer::GetControlCamera
// DIA: idPresentablePlayer::CalculateView
// DIA: idPresentablePlayer::SetViewAngles
// DIA: idPresentablePlayer::UpdateViewAngles
// DIA: idPresentablePlayer::Draw_Shared
// DIA: idList<idHudDangerInfo::dangerInfo_t,5>::Append
// DIA: idPresentablePlayer::SetDefaults
// DIA: idPresentablePlayer::UpdateGrenades
// DIA: idPresentablePlayer::UpdateWeaponAmmoInfo
// DIA: idPresentablePlayer::UpdateWeapon
// DIA: `dynamic initializer for 'pm_lookUpTweak''
// DIA: `dynamic initializer for 'coop_dmgRumbleScalarHigh''
// DIA: `dynamic initializer for 'coop_dmgRumbleScalarLow''
// DIA: `dynamic initializer for 'coop_dmgRumbleDurHigh''
// DIA: `dynamic initializer for 'coop_dmgRumbleDurLow''
// DIA: `dynamic initializer for 'pm_debugView''
// DIA: `dynamic initializer for 'p_useStepUpSprings''
// DIA: `dynamic initializer for 'p_stepUpViewSpringK''
// DIA: `dynamic initializer for 'p_stepUpHandsSpringK''
// DIA: `dynamic initializer for 'p_skipAdditiveAnimCamera''
// DIA: `dynamic initializer for 'g_debugPlayerZoom''
// DIA: `dynamic initializer for 'g_debugZoomBlur''
// DIA: `dynamic initializer for 'g_blendedZoom_nonLinear''
// DIA: `dynamic initializer for 'g_showDangerIndicators''
// DIA: `dynamic initializer for 'g_dangerRobotTeslaRadius''
// DIA: `dynamic initializer for 'g_wpnSelectThreshold''
// DIA: `dynamic initializer for 'g_wpnSelectBlockInput''
// DIA: `dynamic initializer for 'g_wpnSelectShowDelay''
// DIA: `dynamic initializer for 'g_hideBetweenWeaponSelectTime''
// DIA: `dynamic initializer for 'g_wpnSelectLingeringTime''
// DIA: `dynamic initializer for 'dof_debug''
// DIA: `dynamic initializer for 'dof_dofNearAmount''
// DIA: `dynamic initializer for 'dof_dofNearEnd''
// DIA: `dynamic initializer for 'dof_dofFocusBegin''
// DIA: `dynamic initializer for 'dof_dofFocusEnd''
// DIA: `dynamic initializer for 'dof_dofFarBegin''
// DIA: `dynamic initializer for 'dof_dofFarAmount''
// DIA: `dynamic initializer for 'dof_dofSkyAmount''
// DIA: `dynamic initializer for 'hmd_use''
// DIA: `dynamic initializer for 'hmd_pixelAspect''
// DIA: `dynamic initializer for 'hmdQuat''
// DIA: `dynamic initializer for 'net_errorSmoothingMaxDecay''
// DIA: `dynamic initializer for 'net_errorSmoothingDecay''
// DIA: `dynamic initializer for 'pm_clientZoomInDelay''
// DIA: `dynamic initializer for 'pm_clientUseThrottleTime''
// DIA: `dynamic initializer for 'player_damageFeedbackScale''
// DIA: `dynamic initializer for 'player_spikeDamageFeedbackScale''
// DIA: `dynamic initializer for 'rp_DecalPos''
// DIA: `dynamic atexit destructor for 'pm_lookUpTweak''
// DIA: `dynamic atexit destructor for 'coop_dmgRumbleScalarHigh''
// DIA: `dynamic atexit destructor for 'coop_dmgRumbleScalarLow''
// DIA: `dynamic atexit destructor for 'coop_dmgRumbleDurHigh''
// DIA: `dynamic atexit destructor for 'coop_dmgRumbleDurLow''
// DIA: `dynamic atexit destructor for 'pm_debugView''
// DIA: `dynamic atexit destructor for 'p_useStepUpSprings''
// DIA: `dynamic atexit destructor for 'p_stepUpViewSpringK''
// DIA: `dynamic atexit destructor for 'p_stepUpHandsSpringK''
// DIA: `dynamic atexit destructor for 'p_skipAdditiveAnimCamera''
// DIA: `dynamic atexit destructor for 'g_debugPlayerZoom''
// DIA: `dynamic atexit destructor for 'g_debugZoomBlur''
// DIA: `dynamic atexit destructor for 'g_blendedZoom_nonLinear''
// DIA: `dynamic atexit destructor for 'g_showDangerIndicators''
// DIA: `dynamic atexit destructor for 'g_dangerRobotTeslaRadius''
// DIA: `dynamic atexit destructor for 'g_wpnSelectThreshold''
// DIA: `dynamic atexit destructor for 'g_wpnSelectBlockInput''
// DIA: `dynamic atexit destructor for 'g_wpnSelectShowDelay''
// DIA: `dynamic atexit destructor for 'g_hideBetweenWeaponSelectTime''
// DIA: `dynamic atexit destructor for 'g_wpnSelectLingeringTime''
// DIA: `dynamic atexit destructor for 'dof_debug''
// DIA: `dynamic atexit destructor for 'dof_dofNearAmount''
// DIA: `dynamic atexit destructor for 'dof_dofNearEnd''
// DIA: `dynamic atexit destructor for 'dof_dofFocusBegin''
// DIA: `dynamic atexit destructor for 'dof_dofFocusEnd''
// DIA: `dynamic atexit destructor for 'dof_dofFarBegin''
// DIA: `dynamic atexit destructor for 'dof_dofFarAmount''
// DIA: `dynamic atexit destructor for 'dof_dofSkyAmount''
// DIA: `dynamic atexit destructor for 'hmd_use''
// DIA: `dynamic atexit destructor for 'hmd_pixelAspect''
// DIA: `dynamic atexit destructor for 'net_errorSmoothingMaxDecay''
// DIA: `dynamic atexit destructor for 'net_errorSmoothingDecay''
// DIA: `dynamic atexit destructor for 'pm_clientZoomInDelay''
// DIA: `dynamic atexit destructor for 'pm_clientUseThrottleTime''
// DIA: `dynamic atexit destructor for 'player_damageFeedbackScale''
// DIA: `dynamic atexit destructor for 'player_spikeDamageFeedbackScale''

int idPresentableVehicle::StartSoundShader(const std::string& shader,int channel){return shader.empty()?-1:channel;}
bool idPresentablePlayer::IsZoomedIn()const{return zoomBlend>0.0F||zoomRequested;}
idWeapon* idPresentablePlayer::GetPendingWeapon()const{return pendingWeaponSlot>=0?dynamic_cast<idWeapon*>(equipped[static_cast<std::size_t>(pendingWeaponSlot)]):nullptr;}
void idPresentablePlayer::Reload(){if(auto* weapon=GetEquippedWeapon()){++reloadCount;weapon->ammo=std::max(weapon->ammo,1);}}
void idPresentablePlayer::ForceWeaponSelectionUpdate(){++selectionRevision;weaponSelectionVisible=true;}
void idPresentablePlayer::ClearPendingAmmo(){pendingAmmoGroup=-1;}
idWeapon* idPresentablePlayer::GetEquippedWeapon()const{return GetEquippedWeapon(currentWeaponSlot);}
int idPresentablePlayer::StartSoundShader(const std::string& shader,int channel){return idPresentable::StartSoundShader(shader,channel);}
int idPresentablePlayer::StartSoundShader_Predicted(const std::string& shader,int channel){return StartSoundShader(shader,channel);}
void idPresentablePlayer::StopSound_Predicted(int channel){StopSound(channel);}
void idPresentablePlayer::SetSoundVolume(int channel,float volume){idPresentable::SetSoundVolume(channel,volume);}
void idPresentablePlayer::PlayLocalSound(const std::string& shader){localSound=shader;StartSoundShader(shader,0);}
const decayParams_s& idPresentablePlayer::GetDecayParams(bool crouched)const{return crouched?crouchingDecay:standingDecay;}
idInventoryCollection* idPresentablePlayer::GetInventory(){return &inventory;}
const playerSounds_t& idPresentablePlayer::GetPlayerSounds()const{return sounds;}
void idPresentablePlayer::DisplayDeathMessage(){deathMessageDisplayed=true;activeGui="death";}
bool idPresentablePlayer::WeaponSelectionBlocksInput()const{return weaponSelectionVisible&&g_wpnSelectBlockInput.GetInteger()!=0;}
int idPresentablePlayer::GetPeerIndex()const{return playerNumber;}
unsigned char idPresentablePlayer::GetPeerMaskForPrediction()const{return playerNumber>=0&&playerNumber<8?static_cast<unsigned char>(1U<<playerNumber):0;}

template<> void idSpring<idVec1>::Update(float dt){if(dt<=0)return;const float acceleration=(target.x-value.x)*k-velocity.x*c;velocity.x+=acceleration*dt;value.x+=velocity.x*dt;}
template<> void idSpring<idVec1>::SetConstants(float stiffness,float damping){k=std::max(0.0F,stiffness);c=std::max(0.0F,damping);}
idViewCallbacks::~idViewCallbacks()=default;
bool idWeapon::UsesBatteryAmmo(bool secondary)const{return secondary?batterySecondary:batteryPrimary;}
bool idWeapon::UsesBatteryAmmo()const{return UsesBatteryAmmo(false);}
bool idHands::BlockZoom()const{return blockZoom;}
void viewDof_t::Clamp(){nearAmount=std::clamp(nearAmount,0.0F,1.0F);farAmount=std::clamp(farAmount,0.0F,1.0F);skyAmount=std::clamp(skyAmount,0.0F,1.0F);nearEnd=std::max(0.0F,nearEnd);focusBegin=std::max(nearEnd,focusBegin);focusEnd=std::max(focusBegin,focusEnd);farBegin=std::max(focusEnd,farBegin);}
void viewDof_t::Mix(const viewDof_t& o){nearAmount=(nearAmount+o.nearAmount)*0.5F;nearEnd=(nearEnd+o.nearEnd)*0.5F;focusBegin=(focusBegin+o.focusBegin)*0.5F;focusEnd=(focusEnd+o.focusEnd)*0.5F;farBegin=(farBegin+o.farBegin)*0.5F;farAmount=(farAmount+o.farAmount)*0.5F;skyAmount=(skyAmount+o.skyAmount)*0.5F;Clamp();}

void idPresentablePlayer::Init(int number){playerNumber=number;SetDefaults();}
void idPresentablePlayer::PresentableRespawn(bool initial){(void)initial;health=100;respawned=true;fireInhibited=false;damageTaken=0;Show();}
void idPresentablePlayer::StartPerfectOrigin(const idVec3& value){originError=perfectOrigin-value;perfectOrigin=value;SetOrigin(value);}
void idPresentablePlayer::DecayOriginError(){originError*=std::clamp(1.0F-net_errorSmoothingDecay.GetFloat(),0.0F,1.0F);if(originError.Length()<0.001F)originError={};}
idAngles idPresentablePlayer::GetViewAngles()const{return viewAngles;}
idAngles idPresentablePlayer::GetRecoilAngles()const{return recoilAngles;}
void idPresentablePlayer::SetLocalViewLimits(const idAngles& minimum,const idAngles& maximum){minView=minimum;maxView=maximum;}
void idPresentablePlayer::ResetLocalViewLimits(){minView={-89,-360,-45};maxView={89,360,45};}
float idPresentablePlayer::DefaultFov()const{return baseFov;}
float idPresentablePlayer::CalcFov(bool honorZoom)const{return honorZoom?baseFov+(zoomFov-baseFov)*zoomBlend:baseFov;}
void idPresentablePlayer::CheckFovModified(){baseFov=std::clamp(baseFov,1.0F,179.0F);zoomFov=std::clamp(zoomFov,1.0F,baseFov);}
idAmmoItem* idPresentablePlayer::GetNextAmmoDecl(){for(auto* item:inventory.items)if(auto* ammo=dynamic_cast<idAmmoItem*>(item);ammo&&ammo->ammo>0)return ammo;return nullptr;}
void idPresentablePlayer::ClampAdditionalPitchOffset(float limit){additionalPitchOffset=std::clamp(additionalPitchOffset,-std::abs(limit),std::abs(limit));}
void idPresentablePlayer::ReleaseWeaponKickRecoveryDelay(){kickRecoveryDelay=0;}
void idPresentablePlayer::SetQuickslotStatus(){for(std::size_t i=0;i<quickslots.size();++i)quickslots[i]=i<inventory.items.size()?static_cast<int>(i):-1;}
void idPresentablePlayer::UpdateEnemyDanger(){dangerLevel=focusEntity&&!focusFriendly?1.0F:std::max(0.0F,dangerLevel-0.1F);}
void idPresentablePlayer::UpdateAmmoGroup(int group,int amount){if(group>=0&&group<static_cast<int>(ammoGroups.size()))ammoGroups[static_cast<std::size_t>(group)]=std::max(0,amount);}
void idPresentablePlayer::ClearAmmoStatus(){ammoGroups.fill(0);pendingAmmoGroup=-1;}
void idPresentablePlayer::SetupWeaponSelectAmmo(int group){pendingAmmoGroup=group;weaponSelectionVisible=true;}
void idPresentablePlayer::SelectQuickAmmo(int slot){if(slot>=0&&slot<static_cast<int>(quickslots.size()))pendingAmmoGroup=quickslots[static_cast<std::size_t>(slot)];}
void idPresentablePlayer::SelectQuickWeapon(int slot){if(slot>=0&&slot<static_cast<int>(quickslots.size()))SelectWeapon(quickslots[static_cast<std::size_t>(slot)],false);}
void idPresentablePlayer::UpdateSound(){idPresentable::UpdateSound();}
void idPresentablePlayer::ClientPredictTriggers(){++predictionTriggerCount;}
void idPresentablePlayer::TransferDWAmmo(int fromSlot,int toSlot){auto* from=GetEquippedWeapon(fromSlot);auto* to=GetEquippedWeapon(toSlot);if(from&&to){to->ammo+=from->ammo;from->ammo=0;}}
bool idPresentablePlayer::ToggleDualWieldItem(idInventoryItem* item){if(!item)return false;item->dualWield=!item->dualWield;return item->dualWield;}
bool idPresentablePlayer::EquipItem(idInventoryItem* item){if(!item)return false;int slot=std::clamp(item->id,0,static_cast<int>(equipped.size()-1));equipped[static_cast<std::size_t>(slot)]=item;currentWeaponSlot=slot;return true;}
idInventoryItem* idPresentablePlayer::GetEquipped(int slot)const{return slot>=0&&slot<static_cast<int>(equipped.size())?equipped[static_cast<std::size_t>(slot)]:nullptr;}
bool idPresentablePlayer::IsDead()const{return health<=0;}
bool idPresentablePlayer::IsCrouching()const{return crouching;}
bool idPresentablePlayer::GetFocusFriendly()const{return focusEntity&&focusFriendly;}
bool idPresentablePlayer::DeployableAllowed()const{return !IsDead()&&!controlInhibited;}
void idPresentablePlayer::GetFireTrajectory(idVec3& position,idMat3& outAxis)const{position=perfectOrigin+eyeOffset;outAxis=viewAngles.ToMat3();}
void idPresentablePlayer::ClearEquipped(int slot){if(slot>=0&&slot<static_cast<int>(equipped.size()))equipped[static_cast<std::size_t>(slot)]=nullptr;if(currentWeaponSlot==slot)currentWeaponSlot=-1;}
bool idPresentablePlayer::BeforeUse(){beforeUseRunning=focusEntity!=nullptr&&!controlInhibited;return beforeUseRunning;}
bool idPresentablePlayer::BeforeUseIsRunning()const{return beforeUseRunning;}
void idPresentablePlayer::SetAngles(const idAngles& angles){SetViewAngles(angles,true);}
bool idPresentablePlayer::IsPlayerControlInhibited()const{return controlInhibited;}
void idPresentablePlayer::InhibitFire(bool inhibit){fireInhibited=inhibit;}
bool idPresentablePlayer::CheckInhibitFire()const{return fireInhibited||controlInhibited||IsDead();}
idWeapon* idPresentablePlayer::GetControlWeapon()const{return GetEquippedWeapon();}
void idPresentablePlayer::SetControllerShake(float scale,float duration){controllerShakeScale=std::max(0.0F,scale);controllerShakeDuration=std::max(0.0F,duration);}
void idPresentablePlayer::ShowInGameGuiScreen(const std::string& screen){activeGui=screen;}
void idPresentablePlayer::SetWalkState(int state){walkState=state;}
bool idPresentablePlayer::PlayFootStepEffect(const std::string& effect){if(effect.empty()||IsDead())return false;StartFX(effect);return true;}
void idPresentablePlayer::PresentableDamaged(float damage){damage=std::max(0.0F,damage);damageTaken+=damage;health=std::max(0.0F,health-damage);if(IsDead())DisplayDeathMessage();}
void idPresentablePlayer::UpdateEditEntityMode(){editEntityMode=!editEntityMode;}
bool idPresentablePlayer::IsFullyZoomedIn()const{return zoomBlend>=1.0F;}
bool idPresentablePlayer::IsFullyZoomedOut()const{return zoomBlend<=0.0F;}
float idPresentablePlayer::GetZoomBlend()const{return zoomBlend;}
void idPresentablePlayer::ApplyBlendedAdditiveCamera(idAngles& angles)const{angles.pitch+=recoilAngles.pitch*zoomBlend;angles.yaw+=recoilAngles.yaw*zoomBlend;angles.roll+=recoilAngles.roll*zoomBlend;}
void idPresentablePlayer::StartCameraShake(float scale,float decay){cameraShakeScale=std::max(0.0F,scale);cameraShakeDecay=std::max(0.0F,decay);}
void idPresentablePlayer::HandleCameraShake(){cameraShakeScale=std::max(0.0F,cameraShakeScale-cameraShakeDecay);}
void idPresentablePlayer::SetExplicitMove(const idVec3& move){explicitMove=move;}
void idPresentablePlayer::ChangeCurrentZoomLevel(){SetCurrentZoomLevel((currentZoomLevel+1)%(maxZoomLevel+1));}
void idPresentablePlayer::SetCurrentZoomLevel(int level){currentZoomLevel=std::clamp(level,0,maxZoomLevel);zoomRequested=currentZoomLevel>0;}
void idPresentablePlayer::ToggleZoom(bool enabled){zoomRequested=enabled;if(!enabled)currentZoomLevel=0;else if(currentZoomLevel==0)currentZoomLevel=1;}
void idPresentablePlayer::SetupZoom(bool handsZoomIn,idWeapon* weapon){zoomRequested=handsZoomIn&&weapon!=nullptr;currentZoomLevel=zoomRequested?std::max(1,currentZoomLevel):0;}
void idPresentablePlayer::UpdateStepUpSprings(){if(p_useStepUpSprings.GetInteger()!=0)eyeOffset.z+=(64.0F-eyeOffset.z)*0.25F;else eyeOffset.z=64.0F;}
idVec3 idPresentablePlayer::GetEyeOffset()const{return eyeOffset;}
void idPresentablePlayer::SelectWeapon(int number,bool dualWieldIfSame){if(number<0||number>=static_cast<int>(equipped.size())||!equipped[static_cast<std::size_t>(number)])return;if(number==currentWeaponSlot&&dualWieldIfSame)ToggleDualWieldItem(equipped[static_cast<std::size_t>(number)]);pendingWeaponSlot=number;}
void idPresentablePlayer::SelectWeaponBySelectIndex(int number,bool dualWieldIfSame){SelectWeapon(number,dualWieldIfSame);}
void idPresentablePlayer::PrevEquipWeapon(){for(int offset=1;offset<=static_cast<int>(equipped.size());++offset){int slot=(currentWeaponSlot-offset+static_cast<int>(equipped.size()))%static_cast<int>(equipped.size());if(equipped[static_cast<std::size_t>(slot)]){pendingWeaponSlot=slot;return;}}}
void idPresentablePlayer::ClearWeaponKick(){recoilAngles={};additionalPitchOffset=0;currentSpread=0;}
void idPresentablePlayer::ClearSavedPitchForKick(float& pitch){pitch=0;}
float idPresentablePlayer::GetCurWeaponSpread(bool secondary)const{return currentSpread*(secondary?1.1F:1.0F);}
void idPresentablePlayer::CalcCurWeaponSpread(float baseSpread){currentSpread=std::max(0.0F,baseSpread)*(crouching?0.75F:1.0F)*(IsZoomedIn()?0.5F:1.0F);}
void idPresentablePlayer::SetAmmoStatusControlled(idEntity* controllingEntity){ammoController=controllingEntity;}
void idPresentablePlayer::SetAmmoStatus(bool leftHand){ammoStatusLeft=leftHand;}
void idPresentablePlayer::UpdateWeaponSelectionCurrent(){if(pendingWeaponSlot>=0){currentWeaponSlot=pendingWeaponSlot;pendingWeaponSlot=-1;}weaponSelectionVisible=false;}
void idPresentablePlayer::SetupWeaponSelectionElement(int slot){weaponSelectionVisible=slot>=0&&slot<static_cast<int>(equipped.size());}
void idPresentablePlayer::WeaponFireFeedback(float recoil,bool secondary){recoilAngles.pitch-=std::abs(recoil);recoilAngles.yaw+=(secondary?-1.0F:1.0F)*recoil*0.25F;currentSpread+=std::abs(recoil);kickRecoveryDelay=1;}
idWeapon* idPresentablePlayer::GetEquippedWeapon(int slot)const{return dynamic_cast<idWeapon*>(GetEquipped(slot));}
void idPresentablePlayer::ShowInGameNewsPaperGuiScreen(const std::string& screen){activeGui="newspaper:"+screen;}
void idPresentablePlayer::DamageDealtFeedback(float damage){damageDealt+=std::max(0.0F,damage);++damageDealtCount;}
void idPresentablePlayer::UpdateDecal(const idVec3& position){std::string decalTag=rp_DecalPos.name;decalPosition=position;if(!decalTag.empty())SetParm(decalTag,position);}
float idPresentablePlayer::GetMovementScale()const{return movementScale*(crouching?0.5F:1.0F);}
inputSettings_t* idPresentablePlayer::GetPlayerInputSettings(){return &inputSettings;}
idEntity* idFocusTracker::GetFocusEntity()const{return focus;}
idEntity* idPlayer::GetFocusEntity()const{return focus;}
void idPresentablePlayer::PlayerUpdateZoomState(){const float target=zoomRequested?1.0F:0.0F;zoomBlend+=std::clamp(target-zoomBlend,-0.2F,0.2F);}
void idPresentablePlayer::OffsetThirdPersonView(idVec3& viewOrigin)const{viewOrigin-=viewAngles.ToForward()*96.0F;viewOrigin.z+=24.0F;}
void idPresentablePlayer::UpdateWeaponKick(idAngles& angles){angles.pitch+=recoilAngles.pitch;angles.yaw+=recoilAngles.yaw;recoilAngles.pitch*=0.8F;recoilAngles.yaw*=0.8F;}
void idPresentablePlayer::UpdateWeaponSelection(){if(pendingWeaponSlot>=0)UpdateWeaponSelectionCurrent();}
void idPresentablePlayer::WeaponFireFeedback(){WeaponFireFeedback(1.0F,false);}
idEntity* idPresentablePlayer::GetFocusEntity()const{return focusEntity;}
bool idPresentablePlayer::UniqueProjectileAllowed(int projectileId)const{return projectileId!=lastUniqueProjectile;}
bool idPresentablePlayer::Use(){const bool usable=BeforeUse();beforeUseRunning=false;return usable;}
void idPresentablePlayer::AddAttacker(idEntity* attacker,float damage){lastAttacker=attacker;PresentableDamaged(damage);}
void idPresentablePlayer::UpdateDamageDealt(){if(damageDealt>0)damageDealt=std::max(0.0F,damageDealt-1.0F);}
void idPresentablePlayer::UpdateDamageFeedback(){if(damageTaken>0){++damageFeedbackCount;damageTaken=std::max(0.0F,damageTaken-1.0F);}}
void* idPlayer::GetControlCamera()const{return camera;}
void idPresentablePlayer::CalculateView(){viewAngles.pitch=std::clamp(viewAngles.pitch,minView.pitch,maxView.pitch);viewAngles.yaw=std::clamp(viewAngles.yaw,minView.yaw,maxView.yaw);viewAngles.roll=std::clamp(viewAngles.roll,minView.roll,maxView.roll);}
void idPresentablePlayer::SetViewAngles(const idAngles& angles,bool force){if(force||!controlInhibited)viewAngles=angles;CalculateView();}
void idPresentablePlayer::UpdateViewAngles(){viewAngles.pitch+=additionalPitchOffset;CalculateView();}
void idPresentablePlayer::Draw_Shared(){if(!IsHidden()){Present();++drawCount;}}
template int idList<idHudDangerInfo::dangerInfo_t,5>::Append(const idHudDangerInfo::dangerInfo_t&);
void idPresentablePlayer::SetDefaults(){health=100;baseFov=90;zoomFov=45;zoomBlend=0;zoomRequested=false;currentZoomLevel=0;currentWeaponSlot=-1;pendingWeaponSlot=-1;pendingAmmoGroup=-1;fireInhibited=false;controlInhibited=false;crouching=false;movementScale=1;viewAngles={};recoilAngles={};explicitMove={};activeGui.clear();}
void idPresentablePlayer::UpdateGrenades(){++grenadeCount;}
void idPresentablePlayer::UpdateWeaponAmmoInfo(){if(auto* weapon=GetEquippedWeapon())UpdateAmmoGroup(currentWeaponSlot,weapon->ammo);}
void idPresentablePlayer::UpdateWeapon(){UpdateWeaponSelection();UpdateWeaponAmmoInfo();++weaponUpdateCount;}
void idPresentablePlayer::AddInventoryItem(idInventoryItem* item){if(item)inventory.items.push_back(item);}

#define WOLF_DEFINE_PLAYER_CVAR(name) idCVar name(#name,"0",4,#name " recovered gameplay control");
WOLF_PLAYER_SHARED_CVARS(WOLF_DEFINE_PLAYER_CVAR)
#undef WOLF_DEFINE_PLAYER_CVAR

idRuntimeQuat::idRuntimeQuat(){x=0;y=0;z=0;w=1;}
idRuntimeQuat hmdQuat;
idCodeResourcePlayerParm rp_DecalPos("rp_DecalPos");
