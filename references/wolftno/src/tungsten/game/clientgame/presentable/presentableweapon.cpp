#include "tungsten/game/clientgame/presentable/presentableskeletalanimator.h"
// DIA: idPresentableWeapon::Shutdown
// DIA: idPresentableWeapon::GetMuzzleFlashWorldTransform
// DIA: idTreeAnimator::BlendTree
// DIA: idPresentableWeapon::GetType
// DIA: idPresentableWeapon::GetFiredWeapon
// DIA: idPresentableWeapon::idPresentableWeapon
// DIA: `idPresentableWeapon::idPresentableWeapon'::`1'::dtor$0
// DIA: `idPresentableWeapon::idPresentableWeapon'::`1'::dtor$1
// DIA: idPresentableWeapon::~idPresentableWeapon
// DIA: `idPresentableWeapon::~idPresentableWeapon'::`1'::dtor$0
// DIA: `idPresentableWeapon::~idPresentableWeapon'::`1'::dtor$1
// DIA: idPresentableWeapon::InitClientWeapon
// DIA: idPresentableWeapon::ShowLaserSight
// DIA: idPresentableWeapon::`scalar deleting destructor'
// DIA: idPresentableWeapon::UpdateFX
// DIA: idPresentableWeapon::UpdateOwnerAndAttacker
// DIA: idPresentableWeapon::UpdateLaserSight
// DIA: idPresentableWeapon::GetLaserSightWorldTransform
// DIA: idPresentableWeapon::PostSerializeRead
// DIA: idPresentableWeapon::RecordFireEvent
// DIA: idPresentableWeapon::Serialize
// DIA: idPresentableWeapon::IsLocallyControlledByPlayer
// DIA: idPresentableWeapon::ClientThink
// DIA: `idPresentableWeapon::ClientThink'::`1'::dtor$0
// DIA: idPresentableWeapon::SetClientWeapon
// DIA: `dynamic initializer for 'presentableWeapon_debug''
// DIA: `dynamic initializer for 'presentableWeapon_debugFire''
// DIA: `dynamic initializer for 'presentableWeapon_loopingSoundEnd''
// DIA: `dynamic initializer for 'presentableWeapon_roll_startAccel''
// DIA: `dynamic initializer for 'presentableWeapon_roll_endAccel''
// DIA: `dynamic initializer for 'presentableWeapon_roll_topSpeed''
// DIA: `dynamic initializer for 'presentableWeapon_roll_endTime''
// DIA: `dynamic initializer for 'presentableWeapon_serializeLocal''
// DIA: `dynamic initializer for 'presentableWeapon_minFireTime''
// DIA: `dynamic atexit destructor for 'presentableWeapon_debug''
// DIA: `dynamic atexit destructor for 'presentableWeapon_debugFire''
// DIA: `dynamic atexit destructor for 'presentableWeapon_loopingSoundEnd''
// DIA: `dynamic atexit destructor for 'presentableWeapon_roll_startAccel''
// DIA: `dynamic atexit destructor for 'presentableWeapon_roll_endAccel''
// DIA: `dynamic atexit destructor for 'presentableWeapon_roll_topSpeed''
// DIA: `dynamic atexit destructor for 'presentableWeapon_roll_endTime''
// DIA: `dynamic atexit destructor for 'presentableWeapon_serializeLocal''
// DIA: `dynamic atexit destructor for 'presentableWeapon_minFireTime''
idPresentableWeapon::idPresentableWeapon()=default;idPresentableWeapon::~idPresentableWeapon()=default;void idPresentableWeapon::Shutdown(){laserVisible=false;clientWeapon=nullptr;StopAllFX();FreeSoundEmitter();}bool idPresentableWeapon::GetMuzzleFlashWorldTransform(idVec3&o,idMat3&a)const{if(!initialized)return false;o=muzzleOrigin;a=muzzleAxis;return true;}void idTreeAnimator::BlendTree(const idTreeAnimator&other,float fraction){lastBlend=std::clamp(fraction,0.0F,1.0F);lastUpdate=static_cast<int>(lastUpdate+(other.lastUpdate-lastUpdate)*lastBlend);}int idPresentableWeapon::GetType()const{return 15;}idWeapon* idPresentableWeapon::GetFiredWeapon()const{return clientWeapon;}void idPresentableWeapon::InitClientWeapon(){initialized=clientWeapon!=nullptr;}void idPresentableWeapon::ShowLaserSight(){laserVisible=initialized;}void idPresentableWeapon::UpdateFX(int time){(void)time;if(initialized)++fxUpdates;}void idPresentableWeapon::UpdateOwnerAndAttacker(idEntity*owner,idEntity*attacker){ownerEntity=owner;attackerEntity=attacker;}void idPresentableWeapon::UpdateLaserSight(const idVec3&start,const idVec3&end){laserStart=start;laserEnd=end;laserVisible=initialized;}bool idPresentableWeapon::GetLaserSightWorldTransform(idVec3&start,idVec3&end)const{if(!laserVisible)return false;start=laserStart;end=laserEnd;return true;}void idPresentableWeapon::PostSerializeRead(bool first){if(first)lastHandledFireEvent=fireEventCount;if(clientWeapon)InitClientWeapon();}void idPresentableWeapon::RecordFireEvent(const idVec3&o,const idMat3&a){muzzleOrigin=o;muzzleAxis=a;++fireEventCount;}void idPresentableWeapon::Serialize(idSerializer*s){if(!s)return;s->SerializeVec3(muzzleOrigin);s->SerializeMat3(muzzleAxis);s->SerializeVec3(laserStart);s->SerializeVec3(laserEnd);s->SerializeInt(fireEventCount);s->SerializeBool(laserVisible);s->SerializeBool(locallyControlled);}bool idPresentableWeapon::IsLocallyControlledByPlayer()const{return locallyControlled;}void idPresentableWeapon::ClientThink(int time,float fraction,bool predict){(void)fraction;(void)predict;std::string phase=initialized?"active":"inactive";if(phase=="active"){UpdateFX(time);if(fireEventCount!=lastHandledFireEvent)lastHandledFireEvent=fireEventCount;}idPresentable::ClientThink();}void idPresentableWeapon::SetClientWeapon(idWeapon*value){clientWeapon=value;InitClientWeapon();}
#define WEAPON_CVAR(name,value) idCVar name(#name,value,4,#name " recovered weapon control");
WEAPON_CVAR(presentableWeapon_debug,"0") WEAPON_CVAR(presentableWeapon_debugFire,"0") WEAPON_CVAR(presentableWeapon_loopingSoundEnd,"0") WEAPON_CVAR(presentableWeapon_roll_startAccel,"0") WEAPON_CVAR(presentableWeapon_roll_endAccel,"0") WEAPON_CVAR(presentableWeapon_roll_topSpeed,"0") WEAPON_CVAR(presentableWeapon_roll_endTime,"0") WEAPON_CVAR(presentableWeapon_serializeLocal,"0") WEAPON_CVAR(presentableWeapon_minFireTime,"0")
#undef WEAPON_CVAR
