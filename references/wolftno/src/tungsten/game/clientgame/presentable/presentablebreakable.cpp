#include "tungsten/game/clientgame/presentable/presentablebreakable.h"

// DIA: idPresentableDamageable::Present
// DIA: idPresentableBreakable::SetInitProperties
// DIA: idPresentableBreakable::SetRenderModel
// DIA: idPresentableBreakable::GibDefault
// DIA: idPresentableBreakable::GetWorldTransform
// DIA: idPresentableBreakable::ShouldLinkPresentableCollision
// DIA: idPresentableBreakable::Break
// DIA: idPresentableBreakable::PredictHitScanHit
// DIA: idPresentableBreakable::StopAllFX
// DIA: idBitMsg::ReadQuantizedVector<idVec3,512,8>
// DIA: idListArrayNew<breakableJoint_t>
// DIA: idPresentableBreakable::`scalar deleting destructor'
// DIA: `idPresentableBreakable::`scalar deleting destructor''::`1'::dtor$0
// DIA: `idPresentableBreakable::`scalar deleting destructor''::`1'::dtor$1
// DIA: idPresentableBreakable::idPresentableBreakable
// DIA: idPresentableBreakable::GibWithActivator
// DIA: idBitMsg::WriteQuantizedVector<idVec3,512,8>
// DIA: idListArrayResize<breakableJoint_t>
// DIA: idPresentableBreakable::PostSerializeRead
// DIA: idList<breakableJoint_t,5>::Resize
// DIA: idPresentableBreakable::Serialize
// DIA: idList<breakableJoint_t,5>::Alloc
// DIA: idPresentableBreakable::GibOrientedBodies
// DIA: `idPresentableBreakable::GibOrientedBodies'::`1'::dtor$0
// DIA: `idPresentableBreakable::GibOrientedBodies'::`1'::dtor$1
// DIA: idPresentableBreakable::Interpolate
// DIA: `dynamic initializer for 'g_gibImpulse''
// DIA: `dynamic initializer for 'g_defaultImpulse''
// DIA: `dynamic initializer for 'presentable_breakable_debug''
// DIA: `dynamic initializer for 'presentable_breakable_disablePredict''
// DIA: `dynamic atexit destructor for 'g_gibImpulse''
// DIA: `dynamic atexit destructor for 'g_defaultImpulse''
// DIA: `dynamic atexit destructor for 'presentable_breakable_debug''
// DIA: `dynamic atexit destructor for 'presentable_breakable_disablePredict''

#include <algorithm>

void idPresentableDamageable::Present(){idPresentable::Present();}
void idPresentableBreakable::SetInitProperties(int healthValue,int fadeTime,int fadeDelay){health=std::max(0,healthValue);fadeTimeMs=std::max(0,fadeTime);fadeDelayMs=std::max(0,fadeDelay);}
void idPresentableBreakable::SetRenderModel(const std::string& model){idPresentable::SetRenderModel(model);}
void idPresentableBreakable::GibDefault(int currentTime){(void)currentTime;gibImpulse={0,0,g_defaultImpulse.GetFloat()};broken=true;}
void idPresentableBreakable::GetWorldTransform(idVec3& outOrigin,idMat3& outAxis)const{idPresentable::GetWorldTransform(outOrigin,outAxis);}
bool idPresentableBreakable::ShouldLinkPresentableCollision()const{return !broken;}
void idPresentableBreakable::Break(const idPresentable* activator){++explodeFrame;if(activator)GibWithActivator(0,activator);else GibDefault(0);}
bool idPresentableBreakable::PredictHitScanHit(const idVec3& start,const idVec3& direction,float range)const{if(presentable_breakable_disablePredict.GetInteger()!=0||broken)return false;idVec3 ray=direction;if(ray.Normalize()==0)return false;const idBounds bounds=GetAbsBounds();const idVec3 center=(bounds.b[0]+bounds.b[1])*0.5F;const float along=(center-start)*ray;if(along<0||along>range)return false;return (center-(start+ray*along)).Length()<=(bounds.b[1]-bounds.b[0]).Length()*0.5F;}
void idPresentableBreakable::StopAllFX(){idPresentable::StopAllFX();}
void idPresentableBreakable::GibWithActivator(int currentTime,const idPresentable* activator){(void)currentTime;idVec3 activatorOrigin{};idMat3 activatorAxis{};activator->GetWorldTransform(activatorOrigin,activatorAxis);idVec3 selfOrigin{};idMat3 selfAxis{};GetWorldTransform(selfOrigin,selfAxis);idVec3 direction=selfOrigin-activatorOrigin;if(direction.Normalize()==0)direction={0,0,1};gibImpulse=direction*g_gibImpulse.GetFloat();broken=true;}
void idPresentableBreakable::PostSerializeRead(bool firstClientFrame){if(firstClientFrame)currentExplodeFrame=explodeFrame;}
std::vector<std::uint8_t> idPresentableBreakable::Serialize()const{auto data=idPresentable::Serialize();data.push_back(static_cast<std::uint8_t>(broken));data.push_back(static_cast<std::uint8_t>(explodeFrame));return data;}
breakableJoint_t* idPresentableBreakable::AddJoint(){return joints.Alloc();}
void idPresentableBreakable::GibOrientedBodies(int currentTime,const idVec3& direction){(void)currentTime;gibImpulse=direction;gibImpulse.Normalize();gibImpulse*=g_gibImpulse.GetFloat();broken=true;for(int i=0;i<joints.Num();++i)joints[i].active=true;}
void idPresentableBreakable::Interpolate(int currentTime,float fraction){idPresentable::Interpolate(fraction);if(!broken&&currentExplodeFrame!=explodeFrame){GibDefault(currentTime);brokenStartTime=currentTime;}currentExplodeFrame=explodeFrame;if(broken&&fadeTimeMs>0&&currentTime>=brokenStartTime+fadeDelayMs+fadeTimeMs)Hide();Present();}
idPresentableBreakable::idPresentableBreakable()=default;

template<typename T>T* idListArrayNew(int count){return count>0?new T[static_cast<std::size_t>(count)]:nullptr;}
template<typename T>T* idListArrayResize(T* oldList,int oldCount,int newCount){T* replacement=idListArrayNew<T>(newCount);for(int i=0;i<std::min(oldCount,newCount);++i)replacement[i]=oldList[i];delete[] oldList;return replacement;}
template breakableJoint_t* idListArrayNew<breakableJoint_t>(int);
template breakableJoint_t* idListArrayResize<breakableJoint_t>(breakableJoint_t*,int,int);
template class idList<breakableJoint_t,5>;

template<> idVec3 idBitMsg::ReadQuantizedVector<idVec3,512,8>(){return ReadQuantizedVector<idVec3,32768,17>();}
template<> void idBitMsg::WriteQuantizedVector<idVec3,512,8>(const idVec3& value){WriteQuantizedVector<idVec3,32768,17>(value);}

idCVar g_gibImpulse("g_gibImpulse","50000",4,"gib impulse");idCVar g_defaultImpulse("g_defaultImpulse","5000",4,"default gib impulse");idCVar presentable_breakable_debug("presentable_breakable_debug","0",1,"breakable debug");idCVar presentable_breakable_disablePredict("presentable_breakable_disablePredict","0",1,"disable breakable prediction");
