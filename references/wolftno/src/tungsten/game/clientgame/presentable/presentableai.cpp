#include "tungsten/game/clientgame/presentable/presentableai.h"
#include "tungsten/game/ai/ai2_core_support.h"

#include <algorithm>

idInfluenceTrailManager* influenceTrailManager = nullptr;

// DIA: idPresentableActor::GetActorModifierManager
// DIA: idPresentableAI::`scalar deleting destructor'
// DIA: `idPresentableAI::`scalar deleting destructor''::`1'::dtor$0
// DIA: `idPresentableAI::`scalar deleting destructor''::`1'::dtor$1
// DIA: `idPresentableAI::`scalar deleting destructor''::`1'::dtor$2
// DIA: idPresentableAI::InternalSplatterBlood
// DIA: idPresentableAI::InternalPlayAdditivePain
// DIA: idPresentableAI::InternalLootBody
// DIA: idPresentableAI::Interpolate
// DIA: idPresentableAI::UpdateInfluenceTrail
// DIA: idPresentableAI::ShouldSerializeRenderModelParms
// DIA: idPresentableAI::SetDefaults
// DIA: idPresentableAI::Serialize
// DIA: idPresentableAI::SplatterBlood
// DIA: idPresentableAI::PlayAdditivePain
// DIA: idPresentableAI::LootBody
// DIA: idPresentableAI::ClientThink
// DIA: idPresentableAI::PredictHitScanHit
// DIA: idPresentableAI::idPresentableAI
// DIA: `idPresentableAI::idPresentableAI'::`1'::dtor$0
// DIA: `idPresentableAI::idPresentableAI'::`1'::dtor$1
// DIA: `idPresentableAI::idPresentableAI'::`1'::dtor$2
// DIA: `dynamic initializer for 'presentable_ai_serialize_render_parms''
// DIA: `dynamic initializer for 'ai_influenceForce''
// DIA: `dynamic atexit destructor for 'presentable_ai_serialize_render_parms''
// DIA: `dynamic atexit destructor for 'ai_influenceForce''

void* idPresentableActor::GetActorModifierManager(){return modifierManager;}
idPresentableAI::~idPresentableAI()=default;
void idPresentableAI::InternalSplatterBlood(const idVec3& position,float amount){if(amount>0){++bloodSplatterCount;UpdateFX(position);}}
void idPresentableAI::InternalPlayAdditivePain(const std::string& animation,float weight){painAnimation=animation;painWeight=std::clamp(weight,0.0F,1.0F);}
bool idPresentableAI::InternalLootBody(int playerIndex){if(looted||playerIndex<0)return false;looted=true;lootedBy=playerIndex;return true;}
void idPresentableAI::Interpolate(float fraction){idPresentable::Interpolate(fraction);}
void idPresentableAI::UpdateInfluenceTrail(const idVec3& position){if(influenceTrail.empty()||(position-influenceTrail.back()).Length()>1.0F)influenceTrail.push_back(position);if(influenceTrail.size()>16)influenceTrail.erase(influenceTrail.begin());}
bool idPresentableAI::ShouldSerializeRenderModelParms()const{return presentable_ai_serialize_render_parms.GetInteger()!=0;}
void idPresentableAI::SetDefaults(){influenceTrail.clear();painAnimation.clear();painWeight=0;bloodSplatterCount=0;lootedBy=-1;looted=false;SetTeam(0);}
std::vector<std::uint8_t> idPresentableAI::Serialize()const{auto data=idPresentable::Serialize();data.push_back(static_cast<std::uint8_t>(looted));data.push_back(static_cast<std::uint8_t>(bloodSplatterCount));return data;}
void idPresentableAI::SplatterBlood(const idVec3& position,float amount){InternalSplatterBlood(position,amount);}
void idPresentableAI::PlayAdditivePain(const std::string& animation,float weight){InternalPlayAdditivePain(animation,weight);}
bool idPresentableAI::LootBody(int playerIndex){return InternalLootBody(playerIndex);}
void idPresentableAI::ClientThink(){idPresentable::ClientThink();if(!influenceTrail.empty())SetParm("influence",ai_influenceForce.GetFloat());}
bool idPresentableAI::PredictHitScanHit(const idVec3& start,const idVec3& direction,float range)const{const idBounds bounds=GetAbsBounds();const idVec3 center=(bounds.b[0]+bounds.b[1])*0.5F;idVec3 ray=direction;if(ray.Normalize()==0)return false;const float along=(center-start)*ray;if(along<0||along>range)return false;const idVec3 closest=start+ray*along;const idVec3 extent=(bounds.b[1]-bounds.b[0])*0.5F;return (center-closest).Length()<=extent.Length();}
idPresentableAI::idPresentableAI(){SetDefaults();}

// DIA: idPresentableAI::idPresentableAI
idPresentableAI::idPresentableAI(
    idAI2* entity, idRenderModel* renderModel, idAnimStack* animStack,
    idAnimator_Pain* painAnimator, idEntityInfluenceTrail* influenceSettings,
    int entityNumber, const idDeclFX* fxDeclarationValue,
    bool useSphereModelValue)
    : aiEntity(entity),
      sourceRenderModel(renderModel),
      sourceAnimStack(animStack),
      sourcePainAnimator(painAnimator),
      influence(influenceSettings != nullptr ? *influenceSettings
                                             : idEntityInfluenceTrail{}),
      inventory(entity != nullptr ? entity->GetInventory() : nullptr),
      fxDeclaration(fxDeclarationValue),
      useSphereModel(useSphereModelValue) {
  SetDefaults();
  SetSourceRenderModelForAI(renderModel);
  SetEntityNumber(entityNumber);
}

idCVar presentable_ai_serialize_render_parms("presentable_ai_serialize_render_parms","1",4,"serialize AI render parameters");
idCVar ai_influenceForce("ai_influenceForce","1",4,"AI influence trail force");
