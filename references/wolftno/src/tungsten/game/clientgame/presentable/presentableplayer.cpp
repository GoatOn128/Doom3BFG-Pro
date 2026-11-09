#include "tungsten/game/clientgame/presentable/presentableplayer_shared.h"

#include <cstring>

// DIA: idPresentableWeapon::Present
// DIA: idPresentableActor::idPresentableActor
// DIA: idPresentableActor::~idPresentableActor
// DIA: `idPresentableActor::~idPresentableActor'::`1'::dtor$0
// DIA: idPresentableActor::PostSerializeRead
// DIA: idPresentableActor::ClientThink
// DIA: idPresentableActor::ServerThink
// DIA: idPresentableActor::HandleActorModifierMessage
// DIA: idPresentableActor::GetSpeedScale
// DIA: idPresentableActor::GetDamageScale
// DIA: idPresentableActor::IsOverdriveDisabled
// DIA: localView_t::~localView_t
// DIA: `localView_t::~localView_t'::`1'::dtor$0
// DIA: idFocusTracker_Player::~idFocusTracker_Player
// DIA: idPresentablePlayer::InitAutomap
// DIA: idPresentablePlayer::GetMoveBufferItemForClientGameFrame
// DIA: idPresentablePlayer::WriteFireDataToUserCmd
// DIA: idUCmdTracker::SetLastPlayerInputAngles
// DIA: idUCmdTracker::GetAnglesMatchLastPlayerInputAngles
// DIA: idPresentablePlayer::PlayLootSound
// DIA: idPresentablePlayer::GetLocalView
// DIA: idPresentablePlayer::ChallengeExit
// DIA: idPresentablePlayer::NoteSwfForBuildGame
// DIA: idPresentablePlayer::SetPlayerGuiFile
// DIA: idPresentablePlayer::IsTutorialGuiActive
// DIA: idPresentablePlayer::IsInGameGuiPaused
// DIA: idPresentablePlayer::IsInGameGuiActive
// DIA: idPresentablePlayer::IsInGameGuiRendering
// DIA: idPresentablePlayer::HandleInGameGuiEvent
// DIA: idPresentablePlayer::HandleTutorialEvent
// DIA: idPresentablePlayer::HandleGuiEvent
// DIA: idPresentablePlayer::HandleGuiFocus
// DIA: idList<idPresentablePlayer::perkNotificationInfo_t,5>::RemoveIndex
// DIA: idAngles::Clamp
// DIA: idHudInfo::weaponSelection_t::weaponSelection_t
// DIA: idUCmdInhibit::DebugPrintInhibitFlagNames
// DIA: idPresentableActor::GetType
// DIA: idPresentableActor::`scalar deleting destructor'
// DIA: idPresentableActor::GetActorEntity
// DIA: idPresentableActor::GetEquippedWeapon
// DIA: idPresentableActor::GetEquippedWeapon
// DIA: idPresentablePlayer::idSWFScriptFunction_resumeMap::Call
// DIA: idPresentablePlayer::UpdateSerializedView
// DIA: idPresentablePlayer::HudThink
// DIA: idPresentablePlayer::ClientJobSync
// DIA: idPresentablePlayer::ClientUpdateFocusEntity
// DIA: idPresentablePlayer::GetBestWeapon
// DIA: idPresentablePlayer::SelectBestWeapon
// DIA: idPresentablePlayer::SelectLaserCutter
// DIA: idPresentablePlayer::GetViewTransform
// DIA: idPresentablePlayer::StartScreenParticle
// DIA: idPresentablePlayer::StopScreenParticle
// DIA: idPresentablePlayer::StartFxScreenParticle
// DIA: idPresentablePlayer::StopFxScreenParticle
// DIA: idPresentablePlayer::UpdateScreenParticles
// DIA: idPresentablePlayer::UpdateInfluenceTrail
// DIA: idPresentablePlayer::EnableInfluenceTrail
// DIA: idUCmdTracker::ClearAngles
// DIA: idUCmdTracker::SetViewAngles
// DIA: idUCmdTracker::ConstrainViewAngles
// DIA: idUCmdTracker::SetViewAnglesConstraintExplicit
// DIA: idPresentablePlayer::SetViewAnglesConstraintExplicit
// DIA: idUCmdTracker::Clamp180
// DIA: idPresentablePlayer::GetUserCmdsForNetSend
// DIA: idPresentablePlayer::ScopeZoomBlur
// DIA: idPresentablePlayer::GetScopeGuiNum
// DIA: idPresentablePlayer::RespondToUseItem
// DIA: idPresentablePlayer::InventoryAdded
// DIA: idPresentablePlayer::InventoryRemoved
// DIA: idPresentablePlayer::SetServerOverridePos
// DIA: idPresentablePlayer::GetInhibitFlags
// DIA: idPresentablePlayer::SetInhibitFlags
// DIA: idPresentablePlayer::AddInhibitFlags
// DIA: idPresentablePlayer::ClearInhibitFlags
// DIA: idPresentablePlayer::SetRestrictedGamePlay
// DIA: idPresentablePlayer::SetInhibitRaiseWeapon
// DIA: idPresentablePlayer::PlayerBehavior_SprintSlide_IsSliding
// DIA: idPresentablePlayer::PlayerBehavior_JumpOverCover_IsJumping
// DIA: idPresentablePlayer::PlayerBehavior_Takedown_IsInTakedown
// DIA: idPresentablePlayer::PlayerBehavior_Shared_ShouldHideWeapon
// DIA: idPresentablePlayer::PlayerBehavior_Lean_GetHandsOffsetScale
// DIA: idPresentablePlayer::ExitedVehicle
// DIA: idPresentablePlayer::ChallengeRestart
// DIA: idPresentablePlayer::HideTutorialGui
// DIA: idPresentablePlayer::DrawGuis
// DIA: idPresentablePlayer::GetPlayerGui
// DIA: `idPresentablePlayer::GetPlayerGui'::`1'::dtor$0
// DIA: idPresentablePlayer::GuiIsActive
// DIA: idPresentablePlayer::CloseInGameGui
// DIA: idPresentablePlayer::DrawSubtitles
// DIA: idPresentablePlayer::SetSubtitle
// DIA: idPresentablePlayer::GetLobbyUserID
// DIA: idPresentablePlayer::SetLobbyUserID
// DIA: `idPresentablePlayer::InvokeModelBrowser'::`8'::idSWFScriptFunction_returnToMainMenu::Call
// DIA: `idPresentablePlayer::InvokeModelBrowser'::`9'::idSWFScriptFunction_spawnEntity::Call
// DIA: `idPresentablePlayer::InvokeModelBrowser'::`10'::idSWFScriptFunction_removeEntity::Call
// DIA: idPlayerViewCallbacks::GetName
// DIA: idPlayerViewCallbacks::GetViewID
// DIA: idPlayerViewCallbacks::GetPosition
// DIA: idPlayerViewCallbacks::GetViewPosition
// DIA: idPlayerViewCallbacks::ApplyScreenShake
// DIA: idPlayerViewCallbacks::ApplyControllerShake
// DIA: idPlayerViewCallbacks::ApplyDynEnvOverride
// DIA: idPlayerViewCallbacks::ApplyDynEnvParmOverride
// DIA: idPlayerViewCallbacks::ApplyRadialBlur
// DIA: idPlayerViewCallbacks::AddScreenParticle
// DIA: idPlayerViewCallbacks::RemoveScreenParticle
// DIA: idInterpolate<float>::GetCurrentValueEaseOut
// DIA: idPresentablePlayer::idPlayerExplicitMotion_t::idPlayerExplicitMotion_t
// DIA: idWeaponKick::GetOffset
// DIA: idWeaponKick::Init
// DIA: idPresentableActor::Serialize
// DIA: idPresentablePlayer::InitForClient
// DIA: idPresentablePlayer::SerializeHud
// DIA: idPresentablePlayer::PostSerializeRead
// DIA: idHudInfo::weaponSelection_t::operator=
// DIA: idPresentablePlayer::GetPlayerGameTime
// DIA: idPresentablePlayer::IsLocallyControlled
// DIA: idUCmdTracker::idUCmdTracker
// DIA: idPresentablePlayer::SetViewAnglesConstraint
// DIA: idUCmdTracker::IsPressedForPlayer
// DIA: idUCmdTracker::WasReleasedForPlayer
// DIA: idUCmdTracker::WasPressedForPlayer
// DIA: idUCmdTracker::IsPressedForPlayerRaw
// DIA: idUCmdTracker::WasReleasedForPlayerRaw
// DIA: idUCmdTracker::WasPressedForPlayerRaw
// DIA: idUCmdTracker::IsPressedForMenu
// DIA: idPresentablePlayer::ApplyDamageToPresentable
// DIA: idPresentablePlayer::DeactivateGuis
// DIA: idPresentablePlayer::UpdateSubtitles
// DIA: idPresentablePlayer::EndSubtitles
// DIA: `idPresentablePlayer::InvokeModelBrowser'::`11'::idSWFScriptFunction_isStreamingDone::Call
// DIA: idList<idHudDangerInfo::dangerInfo_t,5>::operator=
// DIA: idList<idHudDangerInfo::commanderInfo_t,5>::operator=
// DIA: idList<idCrosshairCombinerInfo,5>::operator=
// DIA: idList<idHudInteractionInfo::icon_t,5>::operator=
// DIA: idWeaponKick::idWeaponKick
// DIA: idMenuManager_InGame::SetActivator
// DIA: idPresentableActor::PostAlloc
// DIA: idStaticList<idCrosshairCombinerInfo,3>::operator=
// DIA: idHudDangerInfo::operator=
// DIA: idStaticList<idHudInteractionInfo::icon_t,3>::operator=
// DIA: idPresentablePlayer::GetControl
// DIA: idPresentablePlayer::GetVehicle
// DIA: idUCmdTracker::Update
// DIA: idPresentablePlayer::UseItem
// DIA: `idPresentablePlayer::UseItem'::`1'::dtor$0
// DIA: idPresentablePlayer::ChallengeRestartRequest
// DIA: idPresentablePlayer::InvokeModelBrowser
// DIA: `idPresentablePlayer::InvokeModelBrowser'::`1'::dtor$0
// DIA: `idPresentablePlayer::InvokeModelBrowser'::`1'::dtor$1
// DIA: `idPresentablePlayer::InvokeModelBrowser'::`1'::dtor$2
// DIA: `idPresentablePlayer::InvokeModelBrowser'::`1'::dtor$3
// DIA: `idPresentablePlayer::InvokeModelBrowser'::`1'::dtor$4
// DIA: `idPresentablePlayer::InvokeModelBrowser'::`1'::dtor$5
// DIA: `idPresentablePlayer::InvokeModelBrowser'::`1'::dtor$6
// DIA: `idPresentablePlayer::InvokeModelBrowser'::`1'::dtor$7
// DIA: idStaticList<idPresentablePlayer::screenParticleState_t,9>::idStaticList<idPresentablePlayer::screenParticleState_t,9>
// DIA: idList<idPresentablePlayer::perkNotificationInfo_t,5>::Resize
// DIA: idHudDangerInfo::~idHudDangerInfo
// DIA: idStaticList<idHudInteractionInfo::icon_t,3>::idStaticList<idHudInteractionInfo::icon_t,3>
// DIA: idStaticList<idCrosshairCombinerInfo,3>::idStaticList<idCrosshairCombinerInfo,3>
// DIA: idPresentableActor::GetFiredWeapon
// DIA: idHudInfo::~idHudInfo
// DIA: `idHudInfo::~idHudInfo'::`1'::dtor$0
// DIA: `idHudInfo::~idHudInfo'::`1'::dtor$1
// DIA: idPresentablePlayer::idSWFScriptFunction_goToMission::Call
// DIA: `idPresentablePlayer::idSWFScriptFunction_goToMission::Call'::`1'::dtor$0
// DIA: `idPresentablePlayer::idSWFScriptFunction_goToMission::Call'::`1'::dtor$1
// DIA: `idPresentablePlayer::idSWFScriptFunction_goToMission::Call'::`1'::dtor$2
// DIA: `idPresentablePlayer::idSWFScriptFunction_goToMission::Call'::`1'::dtor$3
// DIA: idPresentablePlayer::Draw
// DIA: idPresentablePlayer::ClientPredict
// DIA: idPresentablePlayer::ShouldLinkPresentableCollision
// DIA: idHudInteractionInfo::operator=
// DIA: idPresentablePlayer::SerializePredictionState
// DIA: idPresentablePlayer::SerializeNonPredictionState
// DIA: idPresentablePlayer::RecordMove
// DIA: idPresentablePlayer::HandleUserCmds
// DIA: idPresentablePlayer::DetachController
// DIA: idList<idSubtitles::idPart,5>::operator=
// DIA: idList<idPresentablePlayer::perkNotificationInfo_t,5>::Append
// DIA: idHudInfo::idHudInfo
// DIA: `idHudInfo::idHudInfo'::`1'::dtor$3
// DIA: `idHudInfo::idHudInfo'::`1'::dtor$0
// DIA: `idHudInfo::idHudInfo'::`1'::dtor$1
// DIA: idPlayerHud::~idPlayerHud
// DIA: idPresentablePlayer::Serialize
// DIA: idHudInfo::operator=
// DIA: idPresentablePlayer::RemoveControl
// DIA: idPresentablePlayer::ResetControl
// DIA: idPresentablePlayer::SetPerkNotification
// DIA: idPresentablePlayer::ShowTutorialGui
// DIA: idPresentablePlayer::HideSpecificTutorialGui
// DIA: idPresentablePlayer::~idPresentablePlayer
// DIA: `idPresentablePlayer::~idPresentablePlayer'::`1'::dtor$0
// DIA: `idPresentablePlayer::~idPresentablePlayer'::`1'::dtor$1
// DIA: `idPresentablePlayer::~idPresentablePlayer'::`1'::dtor$2
// DIA: `idPresentablePlayer::~idPresentablePlayer'::`1'::dtor$3
// DIA: `idPresentablePlayer::~idPresentablePlayer'::`1'::dtor$4
// DIA: `idPresentablePlayer::~idPresentablePlayer'::`1'::dtor$5
// DIA: `idPresentablePlayer::~idPresentablePlayer'::`1'::dtor$6
// DIA: `idPresentablePlayer::~idPresentablePlayer'::`1'::dtor$7
// DIA: `idPresentablePlayer::~idPresentablePlayer'::`1'::dtor$8
// DIA: `idPresentablePlayer::~idPresentablePlayer'::`1'::dtor$9
// DIA: `idPresentablePlayer::~idPresentablePlayer'::`1'::dtor$10
// DIA: `idPresentablePlayer::~idPresentablePlayer'::`1'::dtor$11
// DIA: `idPresentablePlayer::~idPresentablePlayer'::`1'::dtor$12
// DIA: `idPresentablePlayer::~idPresentablePlayer'::`1'::dtor$13
// DIA: `idPresentablePlayer::~idPresentablePlayer'::`1'::dtor$14
// DIA: `idPresentablePlayer::~idPresentablePlayer'::`1'::dtor$15
// DIA: `idPresentablePlayer::~idPresentablePlayer'::`1'::dtor$16
// DIA: `idPresentablePlayer::~idPresentablePlayer'::`1'::dtor$17
// DIA: `idPresentablePlayer::~idPresentablePlayer'::`1'::dtor$18
// DIA: `idPresentablePlayer::~idPresentablePlayer'::`1'::dtor$19
// DIA: `idPresentablePlayer::~idPresentablePlayer'::`1'::dtor$20
// DIA: `idPresentablePlayer::~idPresentablePlayer'::`1'::dtor$27
// DIA: `idPresentablePlayer::~idPresentablePlayer'::`1'::dtor$31
// DIA: `idPresentablePlayer::~idPresentablePlayer'::`1'::dtor$34
// DIA: idPresentablePlayer::GetOnlineUsableText
// DIA: idPresentablePlayer::ResetLocalViews
// DIA: `idPresentablePlayer::ResetLocalViews'::`1'::dtor$0
// DIA: idPresentablePlayer::AddControl
// DIA: idPresentablePlayer::UpdatePerks
// DIA: idPresentablePlayer::subtitle_t::operator=
// DIA: idPresentablePlayer::subtitle_t::subtitle_t
// DIA: idPresentablePlayer::`scalar deleting destructor'
// DIA: idPresentablePlayer::idPresentablePlayer
// DIA: `idPresentablePlayer::idPresentablePlayer'::`1'::dtor$23
// DIA: `idPresentablePlayer::idPresentablePlayer'::`1'::dtor$0
// DIA: `idPresentablePlayer::idPresentablePlayer'::`1'::dtor$1
// DIA: `idPresentablePlayer::idPresentablePlayer'::`1'::dtor$2
// DIA: `idPresentablePlayer::idPresentablePlayer'::`1'::dtor$25
// DIA: `idPresentablePlayer::idPresentablePlayer'::`1'::dtor$3
// DIA: `idPresentablePlayer::idPresentablePlayer'::`1'::dtor$4
// DIA: `idPresentablePlayer::idPresentablePlayer'::`1'::dtor$5
// DIA: `idPresentablePlayer::idPresentablePlayer'::`1'::dtor$6
// DIA: `idPresentablePlayer::idPresentablePlayer'::`1'::dtor$7
// DIA: `idPresentablePlayer::idPresentablePlayer'::`1'::dtor$8
// DIA: `idPresentablePlayer::idPresentablePlayer'::`1'::dtor$9
// DIA: `idPresentablePlayer::idPresentablePlayer'::`1'::dtor$10
// DIA: `idPresentablePlayer::idPresentablePlayer'::`1'::dtor$11
// DIA: `idPresentablePlayer::idPresentablePlayer'::`1'::dtor$28
// DIA: `idPresentablePlayer::idPresentablePlayer'::`1'::dtor$12
// DIA: `idPresentablePlayer::idPresentablePlayer'::`1'::dtor$13
// DIA: `idPresentablePlayer::idPresentablePlayer'::`1'::dtor$14
// DIA: `idPresentablePlayer::idPresentablePlayer'::`1'::dtor$15
// DIA: `idPresentablePlayer::idPresentablePlayer'::`1'::dtor$16
// DIA: `idPresentablePlayer::idPresentablePlayer'::`1'::dtor$17
// DIA: `idPresentablePlayer::idPresentablePlayer'::`1'::dtor$18
// DIA: `idPresentablePlayer::idPresentablePlayer'::`1'::dtor$19
// DIA: `idPresentablePlayer::idPresentablePlayer'::`1'::dtor$20
// DIA: `idPresentablePlayer::idPresentablePlayer'::`1'::dtor$21
// DIA: `idPresentablePlayer::idPresentablePlayer'::`1'::dtor$22
// DIA: idPresentablePlayer::ClientThink
// DIA: `idPresentablePlayer::ClientThink'::`1'::dtor$0
// DIA: idPresentablePlayer::SetupSubtitle
// DIA: `idPresentablePlayer::SetupSubtitle'::`1'::dtor$0
// DIA: idPresentablePlayer::ClearSubtitle
// DIA: `idPresentablePlayer::ClearSubtitle'::`1'::dtor$0
// DIA: `dynamic initializer for 'fakeCB''
// DIA: `dynamic initializer for 'sub_debug''
// DIA: `dynamic initializer for 'pm_clientAuthoritative''
// DIA: `dynamic initializer for 'pm_clientAuthoritative_debug''
// DIA: `dynamic initializer for 'pm_influenceForce''
// DIA: `dynamic initializer for 'pm_predict''
// DIA: `dynamic initializer for 'pm_predict_db''
// DIA: `dynamic initializer for 'pm_debugInhibit''
// DIA: `dynamic initializer for 'pm_screenParticleOffset''
// DIA: `dynamic atexit destructor for 'sub_debug''
// DIA: `dynamic atexit destructor for 'pm_clientAuthoritative''
// DIA: `dynamic atexit destructor for 'pm_clientAuthoritative_debug''
// DIA: `dynamic atexit destructor for 'pm_influenceForce''
// DIA: `dynamic atexit destructor for 'pm_predict''
// DIA: `dynamic atexit destructor for 'pm_predict_db''
// DIA: `dynamic atexit destructor for 'pm_debugInhibit''
// DIA: `dynamic atexit destructor for 'pm_screenParticleOffset''
// DIA: `dynamic atexit destructor for 'fakeCB''

void idPresentableWeapon::Present(){idPresentable::Present();}
idPresentableActor::idPresentableActor()=default;idPresentableActor::~idPresentableActor()=default;
void idPresentableActor::PostSerializeRead(){idPresentable::PostSerializeRead();}void idPresentableActor::ClientThink(){idPresentable::ClientThink();++clientTicks;}void idPresentableActor::ServerThink(){++serverTicks;}
void idPresentableActor::HandleActorModifierMessage(float speed,float damage,bool disable){speedScale=std::max(0.0F,speed);damageScale=std::max(0.0F,damage);overdriveDisabled=disable;}float idPresentableActor::GetSpeedScale()const{return speedScale;}float idPresentableActor::GetDamageScale()const{return damageScale;}bool idPresentableActor::IsOverdriveDisabled()const{return overdriveDisabled;}int idPresentableActor::GetType()const{return 2;}idEntity* idPresentableActor::GetActorEntity()const{return actorEntity;}idWeapon* idPresentableActor::GetEquippedWeapon()const{return actorWeapons[0];}idWeapon* idPresentableActor::GetEquippedWeapon(int slot)const{return slot>=0&&slot<2?actorWeapons[static_cast<std::size_t>(slot)]:nullptr;}std::vector<std::uint8_t> idPresentableActor::Serialize()const{auto data=idPresentable::Serialize();data.push_back(static_cast<std::uint8_t>(overdriveDisabled));return data;}void idPresentableActor::PostAlloc(){Show();}idWeapon* idPresentableActor::GetFiredWeapon()const{return firedWeapon;}
localView_t::~localView_t()=default;idFocusTracker_Player::~idFocusTracker_Player()=default;
void idAngles::Clamp(const idAngles& lo,const idAngles& hi){pitch=std::clamp(pitch,lo.pitch,hi.pitch);yaw=std::clamp(yaw,lo.yaw,hi.yaw);roll=std::clamp(roll,lo.roll,hi.roll);}
idHudInfo::weaponSelection_t::weaponSelection_t()=default;idHudInfo::weaponSelection_t& idHudInfo::weaponSelection_t::operator=(const weaponSelection_t&o){selected=o.selected;visible=o.visible;return*this;}std::string idUCmdInhibit::DebugPrintInhibitFlagNames(unsigned int flags){std::string out;if(flags&1)out+="move ";if(flags&2)out+="look ";if(flags&4)out+="fire ";if(flags&8)out+="use ";return out;}
void idUCmdTracker::SetLastPlayerInputAngles(const idAngles& value){lastInput=value;}bool idUCmdTracker::GetAnglesMatchLastPlayerInputAngles(const idAngles& v)const{return v.pitch==lastInput.pitch&&v.yaw==lastInput.yaw&&v.roll==lastInput.roll;}void idUCmdTracker::ClearAngles(){lastInput={};viewAngles={};}void idUCmdTracker::SetViewAngles(const idAngles& v){viewAngles=v;ConstrainViewAngles();}void idUCmdTracker::ConstrainViewAngles(){viewAngles.Clamp(minAngles,maxAngles);}void idUCmdTracker::SetViewAnglesConstraintExplicit(const idAngles& lo,const idAngles& hi){minAngles=lo;maxAngles=hi;ConstrainViewAngles();}float idUCmdTracker::Clamp180(float v)const{while(v>180)v-=360;while(v<-180)v+=360;return v;}bool idUCmdTracker::IsPressedForPlayer(unsigned int b)const{return(buttons&b)!=0;}bool idUCmdTracker::WasReleasedForPlayer(unsigned int b)const{return(previousButtons&b)!=0&&(buttons&b)==0;}bool idUCmdTracker::WasPressedForPlayer(unsigned int b)const{return(previousButtons&b)==0&&(buttons&b)!=0;}bool idUCmdTracker::IsPressedForPlayerRaw(unsigned int b)const{return IsPressedForPlayer(b);}bool idUCmdTracker::WasReleasedForPlayerRaw(unsigned int b)const{return WasReleasedForPlayer(b);}bool idUCmdTracker::WasPressedForPlayerRaw(unsigned int b)const{return WasPressedForPlayer(b);}bool idUCmdTracker::IsPressedForMenu(unsigned int b)const{return IsPressedForPlayer(b);}idUCmdTracker::idUCmdTracker()=default;void idUCmdTracker::Update(const idUserCmd& cmd){previousButtons=buttons;buttons=cmd.buttons;SetLastPlayerInputAngles(cmd.angles);SetViewAngles(cmd.angles);}
const char* idPlayerViewCallbacks::GetName()const{return"playerView";}int idPlayerViewCallbacks::GetViewID()const{return 0;}idVec3 idPlayerViewCallbacks::GetPosition()const{return{};}idVec3 idPlayerViewCallbacks::GetViewPosition()const{return GetPosition();}void idPlayerViewCallbacks::ApplyScreenShake(float v){screenShake=std::max(0.0F,v);}void idPlayerViewCallbacks::ApplyControllerShake(float v){controllerShake=std::max(0.0F,v);}void idPlayerViewCallbacks::ApplyDynEnvOverride(float v){dynEnv=v;}void idPlayerViewCallbacks::ApplyDynEnvParmOverride(const std::string& name,float v){if(!name.empty())dynEnv=v;}void idPlayerViewCallbacks::ApplyRadialBlur(float v){radialBlur=std::clamp(v,0.0F,1.0F);}int idPlayerViewCallbacks::AddScreenParticle(const std::string& p){particles.push_back(p);return static_cast<int>(particles.size()-1);}void idPlayerViewCallbacks::RemoveScreenParticle(int h){if(h>=0&&h<static_cast<int>(particles.size()))particles.erase(particles.begin()+h);}
template<> float idInterpolate<float>::GetCurrentValueEaseOut()const{const float f=std::clamp(fraction,0.0F,1.0F);const float eased=1.0F-(1.0F-f)*(1.0F-f);return start+(end-start)*eased;}
idPresentablePlayer::idPlayerExplicitMotion_t::idPlayerExplicitMotion_t()=default;idWeaponKick::idWeaponKick()=default;idVec3 idWeaponKick::GetOffset(float time)const{if(duration<=0||time>=duration)return{};return{0,0,magnitude*(1.0F-time/duration)};}void idWeaponKick::Init(float m,float d){magnitude=std::max(0.0F,m);duration=std::max(0.0F,d);}idHudDangerInfo::~idHudDangerInfo()=default;idHudDangerInfo& idHudDangerInfo::operator=(const idHudDangerInfo&o){danger=o.danger;commanders=o.commanders;return*this;}idHudInteractionInfo& idHudInteractionInfo::operator=(const idHudInteractionInfo&o){icons=o.icons;return*this;}idHudInfo::idHudInfo()=default;idHudInfo::~idHudInfo()=default;idHudInfo& idHudInfo::operator=(const idHudInfo&o){weaponSelection=o.weaponSelection;danger=o.danger;interaction=o.interaction;return*this;}idPlayerHud::~idPlayerHud()=default;void idMenuManager_InGame::SetActivator(idEntity* v){activator=v;}

void idPresentablePlayer::idSWFScriptFunction_resumeMap::Call(idPresentablePlayer& p){p.CloseInGameGui();}void idPresentablePlayer::idSWFScriptFunction_goToMission::Call(idPresentablePlayer& p,const std::string& mission){p.ShowInGameGuiScreen("mission:"+mission);}
void idPresentablePlayer::InitAutomap(){automapInitialized=true;}int idPresentablePlayer::GetMoveBufferItemForClientGameFrame(int frame)const{return moveBuffer.empty()?-1:((frame%static_cast<int>(moveBuffer.size()))+static_cast<int>(moveBuffer.size()))%static_cast<int>(moveBuffer.size());}void idPresentablePlayer::WriteFireDataToUserCmd(idUserCmd& cmd)const{cmd.fireCount=clientFireCount;cmd.angles=viewAngles;}void idPresentablePlayer::PlayLootSound(){PlayLocalSound("player/loot");}localView_t* idPresentablePlayer::GetLocalView(){return &localViews[0];}void idPresentablePlayer::ChallengeExit(){challengeRestartPending=false;activeGui.clear();}void idPresentablePlayer::NoteSwfForBuildGame(const std::string& swf){notedSwf=swf;}void idPresentablePlayer::SetPlayerGuiFile(const std::string& file){playerGui=file;}bool idPresentablePlayer::IsTutorialGuiActive()const{return tutorialActive;}bool idPresentablePlayer::IsInGameGuiPaused()const{return guiPaused;}bool idPresentablePlayer::IsInGameGuiActive()const{return!activeGui.empty();}bool idPresentablePlayer::IsInGameGuiRendering()const{return guiRendering;}bool idPresentablePlayer::HandleInGameGuiEvent(const std::string& e){if(e=="close"){CloseInGameGui();return true;}return IsInGameGuiActive();}bool idPresentablePlayer::HandleTutorialEvent(const std::string& e){if(e=="dismiss"){HideTutorialGui();return true;}return tutorialActive;}bool idPresentablePlayer::HandleGuiEvent(const std::string& e){return HandleTutorialEvent(e)||HandleInGameGuiEvent(e);}void idPresentablePlayer::HandleGuiFocus(bool f){guiFocused=f;}
void idPresentablePlayer::UpdateSerializedView(){localViews[0].angles=viewAngles;localViews[0].position=serverOverridePos;}void idPresentablePlayer::HudThink(){guiRendering=IsInGameGuiActive()||tutorialActive;}void idPresentablePlayer::ClientJobSync(){++gameTime;}void idPresentablePlayer::ClientUpdateFocusEntity(idEntity* v){focusEntity=v;}idWeapon* idPresentablePlayer::GetBestWeapon()const{for(int i=7;i>=0;--i)if(auto*w=GetEquippedWeapon(i);w&&w->ammo>0)return w;return GetEquippedWeapon();}void idPresentablePlayer::SelectBestWeapon(){if(auto*w=GetBestWeapon())SelectWeapon(w->id,false);}void idPresentablePlayer::SelectLaserCutter(){SelectWeapon(7,false);}void idPresentablePlayer::GetViewTransform(idVec3& o,idMat3& a)const{o=serverOverridePos+eyeOffset;a=viewAngles.ToMat3();}
int idPresentablePlayer::StartScreenParticle(const std::string& n){if(!screenParticles.Append({n,true}))return-1;return screenParticles.Num()-1;}void idPresentablePlayer::StopScreenParticle(int h){if(h>=0&&h<screenParticles.Num())screenParticles.values[static_cast<std::size_t>(h)].active=false;}int idPresentablePlayer::StartFxScreenParticle(const std::string& n){return StartScreenParticle("fx:"+n);}void idPresentablePlayer::StopFxScreenParticle(int h){StopScreenParticle(h);}void idPresentablePlayer::UpdateScreenParticles(){for(auto& p:screenParticles.values)if(p.name.empty())p.active=false;}void idPresentablePlayer::UpdateInfluenceTrail(){if(influenceTrail)StartFX("player/influenceTrail");}void idPresentablePlayer::EnableInfluenceTrail(bool e){influenceTrail=e;if(!e)StopAllFX();}
void idPresentablePlayer::SetViewAnglesConstraintExplicit(const idAngles& lo,const idAngles& hi){cmdTracker.SetViewAnglesConstraintExplicit(lo,hi);SetLocalViewLimits(lo,hi);}std::vector<idUserCmd> idPresentablePlayer::GetUserCmdsForNetSend()const{return moveBuffer;}float idPresentablePlayer::ScopeZoomBlur()const{return zoomBlend*zoomBlend;}int idPresentablePlayer::GetScopeGuiNum()const{return IsZoomedIn()?currentWeaponSlot:-1;}bool idPresentablePlayer::RespondToUseItem(idInventoryItem* item){return UseItem(item);}void idPresentablePlayer::InventoryAdded(idInventoryItem* item){AddInventoryItem(item);SetQuickslotStatus();}void idPresentablePlayer::InventoryRemoved(idInventoryItem* item){auto&v=inventory.items;v.erase(std::remove(v.begin(),v.end(),item),v.end());for(auto& e:equipped)if(e==item)e=nullptr;}void idPresentablePlayer::SetServerOverridePos(const idVec3& v){serverOverridePos=v;SetOrigin(v);}unsigned int idPresentablePlayer::GetInhibitFlags()const{return inhibitFlags;}void idPresentablePlayer::SetInhibitFlags(unsigned int f){inhibitFlags=f;}void idPresentablePlayer::AddInhibitFlags(unsigned int f){inhibitFlags|=f;}void idPresentablePlayer::ClearInhibitFlags(unsigned int f){inhibitFlags&=~f;}void idPresentablePlayer::SetRestrictedGamePlay(bool v){restrictedGameplay=v;}void idPresentablePlayer::SetInhibitRaiseWeapon(bool v){inhibitRaiseWeapon=v;}bool idPresentablePlayer::PlayerBehavior_SprintSlide_IsSliding()const{return sliding;}bool idPresentablePlayer::PlayerBehavior_JumpOverCover_IsJumping()const{return jumpingCover;}bool idPresentablePlayer::PlayerBehavior_Takedown_IsInTakedown()const{return takedown;}bool idPresentablePlayer::PlayerBehavior_Shared_ShouldHideWeapon()const{return inhibitRaiseWeapon||takedown||restrictedGameplay;}float idPresentablePlayer::PlayerBehavior_Lean_GetHandsOffsetScale()const{return crouching?0.5F:1.0F;}void idPresentablePlayer::ExitedVehicle(){vehicle=nullptr;}void idPresentablePlayer::ChallengeRestart(){challengeRestartPending=false;PresentableRespawn(false);}void idPresentablePlayer::HideTutorialGui(){tutorialActive=false;tutorialGui.clear();}void idPresentablePlayer::DrawGuis(){guiRendering=IsInGameGuiActive()||tutorialActive;}const std::string& idPresentablePlayer::GetPlayerGui()const{return playerGui;}bool idPresentablePlayer::GuiIsActive(const std::string& gui)const{return activeGui==gui||tutorialGui==gui;}void idPresentablePlayer::CloseInGameGui(){activeGui.clear();guiPaused=false;guiRendering=false;}void idPresentablePlayer::DrawSubtitles(){if(!subtitle.text.empty())SetParm("subtitleActive",1);}void idPresentablePlayer::SetSubtitle(const std::string& text,int end){subtitle.text=text;subtitle.endTime=end;}std::uint64_t idPresentablePlayer::GetLobbyUserID()const{return lobbyUserId;}void idPresentablePlayer::SetLobbyUserID(std::uint64_t id){lobbyUserId=id;}
void idPresentablePlayer::InitForClient(int number){Init(number);InitAutomap();}std::vector<std::uint8_t> idPresentablePlayer::SerializeHud()const{return{static_cast<std::uint8_t>(weaponSelectionVisible),static_cast<std::uint8_t>(tutorialActive)};}void idPresentablePlayer::PostSerializeRead(){idPresentableActor::PostSerializeRead();UpdateSerializedView();}int idPresentablePlayer::GetPlayerGameTime()const{return gameTime;}bool idPresentablePlayer::IsLocallyControlled()const{return control==nullptr||control==this;}void idPresentablePlayer::SetViewAnglesConstraint(const idAngles& lo,const idAngles& hi){SetViewAnglesConstraintExplicit(lo,hi);}void idPresentablePlayer::ApplyDamageToPresentable(float d){PresentableDamaged(d*GetDamageScale());}void idPresentablePlayer::DeactivateGuis(){CloseInGameGui();HideTutorialGui();}void idPresentablePlayer::UpdateSubtitles(int now){if(subtitle.endTime>0&&now>=subtitle.endTime)ClearSubtitle();}void idPresentablePlayer::EndSubtitles(){ClearSubtitle();}idPresentablePlayer* idPresentablePlayer::GetControl()const{return control;}idEntity* idPresentablePlayer::GetVehicle()const{return vehicle;}bool idPresentablePlayer::UseItem(idInventoryItem* item){std::string useName=item?std::to_string(item->id):std::string{};if(!item||restrictedGameplay)return false;return EquipItem(item)&&!useName.empty();}void idPresentablePlayer::ChallengeRestartRequest(){challengeRestartPending=true;}void idPresentablePlayer::InvokeModelBrowser(){std::vector<std::string> models{"player","weapon"};activeGui=models.empty()?std::string{}:"modelBrowser";}idWeapon* idPresentablePlayer::GetFiredWeapon()const{return GetEquippedWeapon();}
void idPresentablePlayer::Draw(){Draw_Shared();DrawGuis();DrawSubtitles();}void idPresentablePlayer::ClientPredict(){if(pm_predict.GetInteger()!=0)HandleUserCmds();}bool idPresentablePlayer::ShouldLinkPresentableCollision()const{return!IsDead()&&!IsHidden();}std::vector<std::uint8_t> idPresentablePlayer::SerializePredictionState()const{std::vector<std::uint8_t>d(sizeof(serverOverridePos)+sizeof(viewAngles));std::memcpy(d.data(),&serverOverridePos,sizeof(serverOverridePos));std::memcpy(d.data()+sizeof(serverOverridePos),&viewAngles,sizeof(viewAngles));return d;}std::vector<std::uint8_t> idPresentablePlayer::SerializeNonPredictionState()const{auto d=SerializeHud();d.push_back(static_cast<std::uint8_t>(health));return d;}void idPresentablePlayer::RecordMove(const idUserCmd& cmd){moveBuffer.push_back(cmd);if(moveBuffer.size()>64)moveBuffer.erase(moveBuffer.begin());}void idPresentablePlayer::HandleUserCmds(){if(moveBuffer.empty())return;cmdTracker.Update(moveBuffer.back());SetViewAngles(cmdTracker.GetViewAngles(),false);EvaluateClientFire(moveBuffer.back().fireCount,gameTime,gameTime+1);}void idPresentablePlayer::DetachController(){control=nullptr;}std::vector<std::uint8_t> idPresentablePlayer::Serialize()const{auto d=idPresentableActor::Serialize();auto p=SerializePredictionState();auto n=SerializeNonPredictionState();d.insert(d.end(),p.begin(),p.end());d.insert(d.end(),n.begin(),n.end());return d;}void idPresentablePlayer::RemoveControl(){control=nullptr;}void idPresentablePlayer::ResetControl(){control=this;}void idPresentablePlayer::SetPerkNotification(const std::string& n){perkNotifications.Append({n});}void idPresentablePlayer::ShowTutorialGui(const std::string& gui){tutorialGui=gui;tutorialActive=!gui.empty();}void idPresentablePlayer::HideSpecificTutorialGui(const std::string& gui){if(tutorialGui==gui)HideTutorialGui();}
idPresentablePlayer::~idPresentablePlayer()=default;std::string idPresentablePlayer::GetOnlineUsableText()const{return restrictedGameplay?"":"use";}void idPresentablePlayer::ResetLocalViews(){localViews={};}void idPresentablePlayer::AddControl(idPresentablePlayer* p){control=p?p:this;}void idPresentablePlayer::UpdatePerks(){for(int i=perkNotifications.Num()-1;i>=0;--i)if(perkNotifications[i].name.empty())perkNotifications.RemoveIndex(i);}idPresentablePlayer::subtitle_t::subtitle_t()=default;idPresentablePlayer::subtitle_t& idPresentablePlayer::subtitle_t::operator=(const subtitle_t&o){text=o.text;endTime=o.endTime;return*this;}idPresentablePlayer::idPresentablePlayer(){SetDefaults();ResetControl();}void idPresentablePlayer::ClientThink(int time,float fraction,bool predict){std::string phase=predict?"predict":"interpolate";gameTime=time;idPresentableActor::ClientThink();Interpolate(fraction);if(phase=="predict")ClientPredict();HudThink();UpdateSubtitles(time);UpdateScreenParticles();}void idPresentablePlayer::SetupSubtitle(const std::string& text,int end){std::string normalized=text;SetSubtitle(normalized,end);}void idPresentablePlayer::ClearSubtitle(){std::string old=subtitle.text;subtitle={};(void)old;}

template class idList<idPresentablePlayer::perkNotificationInfo_t,5>;template class idList<idHudDangerInfo::dangerInfo_t,5>;template class idList<idHudDangerInfo::commanderInfo_t,5>;template class idList<idCrosshairCombinerInfo,5>;template class idList<idHudInteractionInfo::icon_t,5>;template class idList<idSubtitles::idPart,5>;template class idStaticList<idPresentablePlayer::screenParticleState_t,9>;template class idStaticList<idCrosshairCombinerInfo,3>;template class idStaticList<idHudInteractionInfo::icon_t,3>;

struct idPlayerFakeCB{ idPlayerFakeCB():name("fakeCB"){}std::string name;};static idPlayerFakeCB fakeCB;
idCVar sub_debug("sub_debug","0",1,"subtitle debug");idCVar pm_clientAuthoritative("pm_clientAuthoritative","0",1,"client authoritative movement");idCVar pm_clientAuthoritative_debug("pm_clientAuthoritative_debug","0",1,"debug client authoritative movement");idCVar pm_influenceForce("pm_influenceForce","1",4,"influence trail force");idCVar pm_predict("pm_predict","1",1,"predict player movement");idCVar pm_predict_db("pm_predict_db","0",1,"prediction double buffer");idCVar pm_debugInhibit("pm_debugInhibit","0",1,"debug input inhibition");idCVar pm_screenParticleOffset("pm_screenParticleOffset","0",4,"screen particle offset");
