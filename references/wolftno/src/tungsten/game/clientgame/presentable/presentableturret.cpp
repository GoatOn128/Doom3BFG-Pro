#include "tungsten/game/clientgame/presentable/presentableturret.h"
// DIA: idPresentableTurret::GetType
// DIA: idPresentableTurret::`scalar deleting destructor'
// DIA: idPresentableTurret::idPresentableTurret
// DIA: idPresentableTurret::ClientThink
// DIA: idPresentableTurret::ApplyJointMod
// DIA: idPresentableTurret::GetFiredWeapon
// DIA: idPresentableTurret::Serialize
idPresentableTurret::idPresentableTurret(int rotationJoint,int pitchJointValue,int spinJoint):jointRotation(rotationJoint),jointPitch(pitchJointValue),jointSpinwheel(spinJoint){}idPresentableTurret::~idPresentableTurret()=default;int idPresentableTurret::GetType()const{return 23;}
void idPresentableTurret::ClientThink(int time,float fraction,bool predict){(void)time;(void)predict;const float f=std::clamp(fraction,0.0F,1.0F);appliedRotation.pitch=(previousRotationPitch+(rotationPitch-previousRotationPitch)*f)/182.04445F;appliedRotation.yaw=(previousYaw+(rotationYaw-previousYaw)*f)/182.04445F;appliedRotation.roll=0;appliedPitch=(previousPitch+(pitchPitch-previousPitch)*f)/182.04445F;idPresentable::ClientThink();}
void idPresentableTurret::ApplyJointMod(int joint,const idMat3& jointAxis){const idAngles angles=jointAxis[0].ToAngles();if(joint==jointRotation){rotationYaw=static_cast<int>(angles.yaw*182.04445F);rotationPitch=static_cast<int>(angles.pitch*182.04445F);}if(joint==jointPitch)pitchPitch=static_cast<int>(angles.pitch*182.04445F);}
idWeapon* idPresentableTurret::GetFiredWeapon()const{return firedWeapon;}
void idPresentableTurret::Serialize(idSerializer*s){if(!s)return;if(!s->writing){previousYaw=rotationYaw;previousRotationPitch=rotationPitch;previousPitch=pitchPitch;}s->SerializeInt(jointRotation);s->SerializeInt(rotationYaw);s->SerializeInt(rotationPitch);s->SerializeInt(jointPitch);s->SerializeInt(pitchPitch);s->SerializeInt(jointSpinwheel);}
