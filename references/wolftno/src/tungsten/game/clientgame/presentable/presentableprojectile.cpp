#include "tungsten/game/clientgame/presentable/presentableprojectile.h"

#include <algorithm>

// DIA: idProjectile_Rocket::GetInitialDir
// DIA: idPresentableProjectile::~idPresentableProjectile
// DIA: idPresentableProjectile_Rocket::GetType
// DIA: idPresentableProjectile_Grenade::GetType
// DIA: idPresentableProjectile::`scalar deleting destructor'
// DIA: idPresentableProjectile::SetDefaults
// DIA: idPresentableProjectile::Init
// DIA: idPresentableProjectile::UpdateFX
// DIA: idPresentableProjectile::GetWorldTransform
// DIA: idPresentableProjectile::InternalClientLaunch
// DIA: idPresentableProjectile::StartFlySound
// DIA: idPresentableProjectile::SetupClientProjectilePhysics
// DIA: idPresentableProjectile::IsDead
// DIA: idPresentableProjectile::IsActive
// DIA: idPresentableProjectile::DecayOriginAndAxisSmoothing
// DIA: idPresentableProjectile::SmoothFromOriginAndAxis
// DIA: idPresentableProjectile_Rocket::`scalar deleting destructor'
// DIA: `idPresentableProjectile_Rocket::`scalar deleting destructor''::`1'::dtor$0
// DIA: idPresentableProjectile_Homing::GetInitialDir
// DIA: idPresentableProjectile_Grenade::`scalar deleting destructor'
// DIA: idPresentableProjectile_Grenade::GetSpeedForFireAxis
// DIA: idPresentableProjectile::idPresentableProjectile
// DIA: `idPresentableProjectile::idPresentableProjectile'::`1'::dtor$0
// DIA: idPresentableProjectile::SmoothFromLaunchOriginAxis
// DIA: idPresentableProjectile::ClientThink
// DIA: idPresentableProjectile::AdjustStartToCollision
// DIA: idPresentableProjectile_Rocket::ClientThink
// DIA: idPresentableProjectile_Homing::ClientThink
// DIA: idPresentableProjectile_Grenade::idPresentableProjectile_Grenade
// DIA: idPresentableProjectile_Grenade::InternalClientLaunch
// DIA: idProjectileTarget::operator idPresentable const * __ptr64
// DIA: idPresentableProjectile::ClientLaunch
// DIA: idPresentableProjectile_Rocket::idPresentableProjectile_Rocket
// DIA: idPresentableProjectile_Rocket::InternalClientLaunch
// DIA: idPresentableProjectile_Homing::GetType
// DIA: idPresentableProjectile_Homing::idPresentableProjectile_Homing
// DIA: idPresentableProjectile_Homing::InternalClientLaunch
// DIA: idPresentableProjectile::Serialize
// DIA: `dynamic initializer for 'g_projectileDebug''
// DIA: `dynamic initializer for 'g_projectileClientDontMove''
// DIA: `dynamic initializer for 'fakeCB''
// DIA: `dynamic initializer for 'fakbCBStopWhenCollide''
// DIA: `dynamic atexit destructor for 'g_projectileDebug''
// DIA: `dynamic atexit destructor for 'g_projectileClientDontMove''
// DIA: `dynamic atexit destructor for 'fakeCB''
// DIA: `dynamic atexit destructor for 'fakbCBStopWhenCollide''

namespace{ idMat3 Identity(){idMat3 a{};a[0]={1,0,0};a[1]={0,1,0};a[2]={0,0,1};return a;} struct FakeProjectileCB{FakeProjectileCB(const char*n):name(n){}std::string name;};static FakeProjectileCB fakeCB("fakeCB");static FakeProjectileCB fakbCBStopWhenCollide("fakbCBStopWhenCollide"); }
idVec3 idProjectile_Rocket::GetInitialDir(const idMat3& axis){return axis[0];}
idPresentableProjectile::~idPresentableProjectile()=default;int idPresentableProjectile_Rocket::GetType()const{return 12;}int idPresentableProjectile_Grenade::GetType()const{return 10;}
void idPresentableProjectile::SetDefaults(){projectileOrigin={};projectileAxis=Identity();velocity={};launchOrigin={};launchAxis=Identity();smoothingOrigin={};projectileSpeed=0;launchTime=0;lastThinkTime=0;fxUpdates=0;active=false;dead=false;physicsReady=false;}
void idPresentableProjectile::Init(const std::string& model,float speed){SetRenderModel(model);projectileSpeed=std::max(0.0F,speed);SetupClientProjectilePhysics();}
void idPresentableProjectile::UpdateFX(int time){(void)time;if(active)++fxUpdates;idPresentable::UpdateFX(projectileOrigin);}
void idPresentableProjectile::GetWorldTransform(idVec3& o,idMat3& a)const{o=projectileOrigin+smoothingOrigin;a=projectileAxis;}
void idPresentableProjectile::InternalClientLaunch(const idVec3& o,const idMat3& a,const idProjectileTarget& target,float speed,int time){projectileOrigin=o;projectileAxis=a;projectileTarget=target;projectileSpeed=std::max(0.0F,speed);velocity=a[0]*projectileSpeed;launchOrigin=o;launchAxis=a;launchTime=time;lastThinkTime=time;active=true;dead=false;}
void idPresentableProjectile::StartFlySound(){if(active)StartSoundShader("projectile/fly",1);}void idPresentableProjectile::SetupClientProjectilePhysics(){physicsReady=true;}bool idPresentableProjectile::IsDead()const{return dead;}bool idPresentableProjectile::IsActive()const{return active&&!dead;}
void idPresentableProjectile::DecayOriginAndAxisSmoothing(){smoothingOrigin*=0.8F;if(smoothingOrigin.Length()<0.001F)smoothingOrigin={};}void idPresentableProjectile::SmoothFromOriginAndAxis(const idVec3& o,const idMat3& a){smoothingOrigin=projectileOrigin-o;projectileOrigin=o;projectileAxis=a;}idPresentableProjectile::idPresentableProjectile(){SetDefaults();}
void idPresentableProjectile::SmoothFromLaunchOriginAxis(){SmoothFromOriginAndAxis(launchOrigin,launchAxis);}void idPresentableProjectile::ClientThink(int time,float fraction,bool predict){(void)fraction;(void)predict;if(!IsActive())return;const float dt=std::max(0,time-lastThinkTime)*0.001F;if(g_projectileClientDontMove.GetInteger()==0)projectileOrigin+=velocity*dt;lastThinkTime=time;DecayOriginAndAxisSmoothing();UpdateFX(time);}
bool idPresentableProjectile::AdjustStartToCollision(const idBounds& b){if(projectileOrigin.x>=b.b[0].x&&projectileOrigin.x<=b.b[1].x&&projectileOrigin.y>=b.b[0].y&&projectileOrigin.y<=b.b[1].y&&projectileOrigin.z>=b.b[0].z&&projectileOrigin.z<=b.b[1].z){projectileOrigin=b.b[1]+idVec3{0.01F,0.01F,0.01F};return true;}return false;}
idPresentableProjectile_Rocket::~idPresentableProjectile_Rocket()=default;idVec3 idPresentableProjectile_Homing::GetInitialDir(const idVec3& target)const{idVec3 d=target-projectileOrigin;if(d.Normalize()==0)d=projectileAxis[0];return d;}idPresentableProjectile_Grenade::~idPresentableProjectile_Grenade()=default;float idPresentableProjectile_Grenade::GetSpeedForFireAxis(const idMat3& fireAxis)const{return projectileSpeed*std::max(0.1F,fireAxis[0].Length());}
idPresentableProjectile_Grenade::idPresentableProjectile_Grenade()=default;void idPresentableProjectile_Grenade::InternalClientLaunch(const idVec3&o,const idMat3&a,const idProjectileTarget&t,float s,int time){idPresentableProjectile::InternalClientLaunch(o,a,t,GetSpeedForFireAxis(a)>0?GetSpeedForFireAxis(a):s,time);velocity=a[0]*s+idVec3{0,0,s*0.25F};}void idPresentableProjectile_Grenade::ClientThink(int time,float f,bool p){const float dt=std::max(0,time-lastThinkTime)*0.001F;velocity.z-=9.81F*dt;idPresentableProjectile::ClientThink(time,f,p);}
idProjectileTarget::operator const idPresentable*()const{return target;}void idPresentableProjectile::ClientLaunch(const idVec3&o,const idMat3&a,const idProjectileTarget&t,float s,int time){SetupClientProjectilePhysics();InternalClientLaunch(o,a,t,s,time);StartFlySound();}
idPresentableProjectile_Rocket::idPresentableProjectile_Rocket()=default;void idPresentableProjectile_Rocket::InternalClientLaunch(const idVec3&o,const idMat3&a,const idProjectileTarget&t,float s,int time){idPresentableProjectile::InternalClientLaunch(o,a,t,s,time);velocity=idProjectile_Rocket::GetInitialDir(a)*s;}void idPresentableProjectile_Rocket::ClientThink(int time,float f,bool p){idPresentableProjectile::ClientThink(time,f,p);}
int idPresentableProjectile_Homing::GetType()const{return 13;}idPresentableProjectile_Homing::idPresentableProjectile_Homing()=default;void idPresentableProjectile_Homing::InternalClientLaunch(const idVec3&o,const idMat3&a,const idProjectileTarget&t,float s,int time){idPresentableProjectile::InternalClientLaunch(o,a,t,s,time);velocity=GetInitialDir(targetPosition)*s;}void idPresentableProjectile_Homing::ClientThink(int time,float f,bool p){idVec3 desired=GetInitialDir(targetPosition)*projectileSpeed;velocity=velocity*0.8F+desired*0.2F;idPresentableProjectile::ClientThink(time,f,p);}
void idPresentableProjectile::Serialize(idSerializer* ser){if(!ser)return;ser->SerializeVec3(projectileOrigin);ser->SerializeMat3(projectileAxis);ser->SerializeVec3(velocity);ser->SerializeFloat(projectileSpeed);ser->SerializeInt(launchTime);ser->SerializeBool(active);ser->SerializeBool(dead);}
idCVar g_projectileDebug("g_projectileDebug","0",1,"projectile debug");idCVar g_projectileClientDontMove("g_projectileClientDontMove","0",1,"freeze client projectile movement");
