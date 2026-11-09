#include "tungsten/game/components/healthcomponent.h"
#include <algorithm>
#include <cstring>
// DIA: idTungstenHealth::Heal_Impl
// DIA: idTungstenHealth::Kill_Impl
// DIA: idTungstenHealth::Reset_Impl
// DIA: idTungstenHealth::DamageComponent_Impl
// DIA: idTungstenHealth::HealComponent_Impl
// DIA: idTungstenHealth::GetTotalCurHealth_Impl
// DIA: idTungstenHealth::GetTotalMaxHealth_Impl
// DIA: idTungstenHealth::idTungstenHealth
// DIA: `idTungstenHealth::idTungstenHealth'::`1'::dtor$0
// DIA: idTungstenHealth::GetComponentCur_Impl
// DIA: idTungstenHealth::GetComponentMax_Impl
// DIA: idTungstenHealth::SetComponentCur_Impl
// DIA: idTungstenHealth::SetComponentMax_Impl
// DIA: idTungstenHealth::GetBaseHealthRatio_Impl
// DIA: idTungstenHealth::SetCurBaseHealth_Impl
// DIA: idTungstenHealth::GetTotalHealthRatio_Impl
// DIA: idTungstenHealth::IsDead_Impl
// DIA: idTungstenHealth::ClearDeadFlag
// DIA: idTungstenHealth::Update_Impl
// DIA: idTungstenHealth::Damage_Impl
// DIA: idTungstenHealth::WriteComponents_Impl
// DIA: idTungstenHealth::ReadComponents_Impl
idTungstenHealth::idTungstenHealth(){components[0].max=100;components[1].max=100;Reset_Impl();}
void idTungstenHealth::Heal_Impl(float h){HealComponent_Impl(0,h,false);}void idTungstenHealth::Kill_Impl(){isDead=true;components[0].cur=0;}void idTungstenHealth::Reset_Impl(){regenSpeedMultiplier=1;timeSinceDamage=0;isDead=false;components[0].cur=components[0].max;components[1].cur=0;}
bool idTungstenHealth::DamageComponent_Impl(int t,float d,bool allowDeath,bool overflow,float*r){if(r)*r=0;if(t<0||t>=2||d<=0)return false;auto&c=components[static_cast<std::size_t>(t)];float applied=std::min(d,allowDeath?c.cur:std::max(0.0F,c.cur-1));c.cur-=applied;if(r)*r=overflow?d-applied:0;if(t==0&&c.cur<=0&&allowDeath){Kill_Impl();return true;}return false;}
void idTungstenHealth::HealComponent_Impl(int t,float h,bool overflow){if(t<0||t>=2||h<=0)return;auto&c=components[static_cast<std::size_t>(t)];c.cur=overflow?c.cur+h:std::min(c.max,c.cur+h);}
float idTungstenHealth::GetTotalCurHealth_Impl()const{return components[0].cur+components[1].cur;}float idTungstenHealth::GetTotalMaxHealth_Impl()const{return components[0].max+components[1].max;}float idTungstenHealth::GetComponentCur_Impl(int t)const{return t>=0&&t<2?components[static_cast<std::size_t>(t)].cur:0;}float idTungstenHealth::GetComponentMax_Impl(int t)const{return t>=0&&t<2?components[static_cast<std::size_t>(t)].max:0;}
void idTungstenHealth::SetComponentCur_Impl(int t,float h){if(t>=0&&t<2)components[static_cast<std::size_t>(t)].cur=std::max(0.0F,h);}void idTungstenHealth::SetComponentMax_Impl(int t,float h){if(t>=0&&t<2){auto&c=components[static_cast<std::size_t>(t)];c.max=std::max(0.0F,h);c.cur=std::min(c.cur,c.max);}}float idTungstenHealth::GetBaseHealthRatio_Impl()const{return components[0].max>0?components[0].cur/components[0].max:0;}void idTungstenHealth::SetCurBaseHealth_Impl(float h){SetComponentCur_Impl(0,h);}float idTungstenHealth::GetTotalHealthRatio_Impl()const{const float m=GetTotalMaxHealth_Impl();return m>0?GetTotalCurHealth_Impl()/m:0;}bool idTungstenHealth::IsDead_Impl()const{return isDead;}void idTungstenHealth::ClearDeadFlag(){isDead=false;}
void idTungstenHealth::Update_Impl(float boost,float dt){timeSinceDamage+=std::max(0.0F,dt);if(!isDead&&timeSinceDamage>=regenDelay)HealComponent_Impl(0,regenSpeed*regenSpeedMultiplier*std::max(0.0F,boost)*dt,false);for(auto&c:components)if(c.cur>c.max)c.cur=std::max(c.max,c.cur-regenSpeed*dt);}
bool idTungstenHealth::Damage_Impl(float d,bool allowDeath,float*r,bool ignoreArmor){timeSinceDamage=0;float remaining=std::max(0.0F,d);if(!ignoreArmor&&components[1].cur>0){const float absorbed=std::min(components[1].cur,d*0.5F);components[1].cur-=absorbed;remaining=std::max(0.0F,d-absorbed*1.6F);}const bool killed=DamageComponent_Impl(0,remaining,allowDeath,true,r);return killed;}
std::vector<std::uint8_t> idTungstenHealth::WriteComponents_Impl()const{std::vector<std::uint8_t>d(sizeof(components)+1);std::memcpy(d.data(),components.data(),sizeof(components));d.back()=isDead?1:0;return d;}bool idTungstenHealth::ReadComponents_Impl(const std::vector<std::uint8_t>&d){if(d.size()!=sizeof(components)+1)return false;std::memcpy(components.data(),d.data(),sizeof(components));isDead=d.back()!=0;return true;}
