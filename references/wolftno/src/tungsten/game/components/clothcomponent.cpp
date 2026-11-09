#include "tungsten/game/components/clothcomponent.h"
#include <cmath>
// DIA: idClothComponent::~idClothComponent
// DIA: idClothComponent::Init
// DIA: idClothComponent::Update
// DIA: `dynamic initializer for 'g_debugClothComponent''
// DIA: `dynamic initializer for 'g_TestClothComponentWind''
// DIA: `dynamic atexit destructor for 'g_debugClothComponent''
// DIA: `dynamic atexit destructor for 'g_TestClothComponentWind''
idCVar g_debugClothComponent("g_debugClothComponent","0",0,"cloth diagnostics");idCVar g_TestClothComponentWind("g_TestClothComponentWind","0",0,"cloth wind strength");
idClothComponent::~idClothComponent()=default;
bool idClothComponent::Init(idEntity*animated){if(!clothDecl||!animated)return false;animatedEntity=animated;clothSim=std::make_unique<idClothSim>();if(auto*p=animated->GetPhysics()){clothSim->origin=p->GetOrigin();clothSim->axis=p->GetAxis();}return true;}
void idClothComponent::Update(float dt){if(!clothSim)return;const float wind=g_TestClothComponentWind.GetFloat();clothSim->windDirection={std::sin(static_cast<float>(clothSim->updates)*dt)*wind,-wind,0};clothSim->stress+=std::fabs(wind)*dt;if(!isDetached&&clothDecl&&clothSim->stress>=clothDecl->detachThreshold){isDetached=true;clothSim->detached=true;}++clothSim->updates;(void)animatedEntity;}
