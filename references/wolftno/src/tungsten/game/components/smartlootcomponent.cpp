#include "tungsten/game/components/smartlootcomponent.h"
#include <algorithm>
#include <random>
// DIA: idSmartLootComponent::idLootContainer::DeleteItem
// DIA: idBaseHealth::HasMaxCurBaseHealth
// DIA: `idSmartLootComponent::Activate'::`9'::local_t::CreateFromWeapon
// DIA: `idSmartLootComponent::Activate'::`9'::local_t::CreateFromLootableItem
// DIA: PrepareItemGroupWeights
// DIA: GetTotalAmmoAndDisableGroups
// DIA: GetWeightedRandom<`anonymous namespace'::tempItemGroup_t,20>
// DIA: GetWeightedRandom<`anonymous namespace'::tempItem_t,0>
// DIA: idSmartLootComponent::Activate
// DIA: idSmartLootComponent::Looted
// DIA: IsCompatibleAmmo
// DIA: idActor::GetDroppedItem1
// DIA: idList<idPickupHudInfo::item_t,5>::Resize
// DIA: idSmartLootComponent::~idSmartLootComponent
// DIA: `idSmartLootComponent::~idSmartLootComponent'::`1'::dtor$0
// DIA: idSmartLootComponent::idSmartLootComponent
// DIA: idSmartLootComponent::`scalar deleting destructor'
// DIA: idSmartLootComponent::GetLootItems
// DIA: CreateRandomItem
// DIA: idSmartLootComponent::CreateRandomItem
// DIA: idSmartLootComponent::CreateRandomAmmo
// DIA: idList<idPickupHudInfo::item_t,5>::Alloc
// DIA: idSmartLootComponent::Update
// DIA: idSmartLootComponent::GetPickupHudInfo
// DIA: `idSmartLootComponent::GetPickupHudInfo'::`1'::dtor$0
namespace{struct lootGroup_t{float weight{1},minimum{0},maximum{1};};struct tempItemGroup_t{lootGroup_t*group{nullptr};bool disabled{false};float val{0},need{0},weight{0};};struct tempItem_t{idInventoryItem*item{nullptr};float weight{0};};template<typename T,int Tag>T* GetWeightedRandom(T*items,int count,float pick){float total=0;for(int i=0;i<count;++i)total+=std::max(0.0F,items[i].weight);if(total<=0)return nullptr;float cursor=std::clamp(pick,0.0F,0.999999F)*total;for(int i=0;i<count;++i){cursor-=std::max(0.0F,items[i].weight);if(cursor<=0)return &items[i];}return &items[count-1];}}
void PrepareItemGroupWeights(tempItemGroup_t*groups,int count){bool anyNeed=false;for(int i=0;i<count;++i){auto&g=groups[i];g.disabled=!g.group||g.group->weight<=0;if(!g.disabled){const float range=g.group->maximum-g.group->minimum;g.need=range>0?std::clamp(1-(g.val-g.group->minimum)/range,0.0F,1.0F):0;anyNeed|=g.need>0;}}for(int i=0;i<count;++i)groups[i].weight=groups[i].disabled?0:groups[i].group->weight*(anyNeed?groups[i].need:1);}
int GetTotalAmmoAndDisableGroups(const idInventoryCollection*inventory,int ammoId,bool*disable){int total=0;if(inventory)for(auto*item:inventory->items)if(item&&item->id==ammoId)total+=item->ammo;if(disable)*disable=total>100;return total;}
idInventoryItem* CreateRandomItem(tempItemGroup_t*groups,int count){auto*group=GetWeightedRandom<tempItemGroup_t,20>(groups,count,0.5F);return group&&!group->disabled?new idInventoryItem(static_cast<int>(group-groups)):nullptr;}
class idActor{public:idInventoryItem*GetDroppedItem1()const;void SetDropped(idInventoryItem*v){dropped=v;}private:idInventoryItem*dropped{nullptr};};
idInventoryItem* idActor::GetDroppedItem1()const{return dropped;}
void idSmartLootComponent::idLootContainer::DeleteItem(){if(owned)delete item;item=nullptr;owned=false;}
bool idBaseHealth::HasMaxCurBaseHealth()const{return cur>=max;}
idSmartLootComponent::idSmartLootComponent(){lootableItems.SetNum(4);}
idSmartLootComponent::~idSmartLootComponent(){for(int i=0;i<lootableItems.Num();++i)lootableItems[i].DeleteItem();}
void idSmartLootComponent::Activate(idEntity*owner,idInventoryCollection*inventory){(void)owner;if(!inventory)return;for(auto*item:inventory->items){if(!item)continue;auto*copy=dynamic_cast<idWeapon*>(item)?static_cast<idInventoryItem*>(new idWeapon(item->id)):static_cast<idInventoryItem*>(new idInventoryItem(item->id));copy->ammo=item->ammo;const unsigned category=dynamic_cast<idWeapon*>(item)?LOOT_WEAPON:LOOT_CUSTOM;lootableItems.Append({copy,category,true});}}
void idSmartLootComponent::Looted(const idLootContainer*loot){if(!loot)return;for(int i=0;i<lootableItems.Num();++i)if(lootableItems[i].item==loot->item&&lootableItems[i].category==loot->category){lootableItems[i].item=nullptr;lootableItems[i].owned=false;latestLoot|=loot->category;if(loot->category&(LOOT_HEALTH|LOOT_ARMOR))hasLootedHealth=true;return;}}
void idSmartLootComponent::GetLootItems(std::vector<idLootContainer>&out,unsigned category)const{out.clear();unsigned remaining=category;for(int i=0;i<lootableItems.Num();++i){const auto&loot=lootableItems[i];if(loot.item&&(loot.category&remaining)){out.push_back(loot);remaining&=~loot.category;}}}
bool IsCompatibleAmmo(const idInventoryItem*ammo,const idWeapon*weapon){return ammo&&weapon&&ammo->id==weapon->id;}
bool idSmartLootComponent::CreateRandomItem(float healthRatio,float armorRatio){lootGroup_t defs[2]{{1,0,1},{1,0,1}};tempItemGroup_t groups[2]{{&defs[0],false,healthRatio},{&defs[1],false,armorRatio}};PrepareItemGroupWeights(groups,2);std::mt19937 rng(seed++);std::uniform_real_distribution<float>d(0,1);auto*g=GetWeightedRandom<tempItemGroup_t,20>(groups,2,d(rng));if(!g)return false;const int index=static_cast<int>(g-groups);auto&slot=lootableItems[index];slot.DeleteItem();slot={new idInventoryItem(index),index==0?LOOT_HEALTH:LOOT_ARMOR,true};return true;}
bool idSmartLootComponent::CreateRandomAmmo(const idInventoryCollection*inventory){bool disabled=false;const int total=GetTotalAmmoAndDisableGroups(inventory,1,&disabled);if(disabled)return false;auto&slot=lootableItems[3];slot.DeleteItem();auto*ammo=new idAmmoItem(1);ammo->ammo=std::max(1,20-total/10);slot={ammo,LOOT_AMMO,true};return true;}
void idSmartLootComponent::Update(const idEntity*owner,const idInventoryCollection*inventory){(void)owner;if(!lootableItems[0].item&&!lootableItems[1].item)CreateRandomItem(0.5F,0.5F);if(!lootableItems[3].item)CreateRandomAmmo(inventory);}
void idSmartLootComponent::GetPickupHudInfo(const idEntity*user,idPickupHudInfo&info,unsigned category)const{(void)user;info.items.Clear();for(int i=0;i<lootableItems.Num();++i){const auto&loot=lootableItems[i];if(loot.item&&(loot.category&category)){auto*entry=info.items.Alloc();entry->id=loot.item->id;entry->category=static_cast<int>(loot.category);entry->amount=loot.item->ammo;}}}
namespace {
template tempItemGroup_t* GetWeightedRandom<tempItemGroup_t,20>(tempItemGroup_t*,int,float);
template tempItem_t* GetWeightedRandom<tempItem_t,0>(tempItem_t*,int,float);
}
template bool idList<idPickupHudInfo::item_t,5>::Resize(int);
template idPickupHudInfo::item_t* idList<idPickupHudInfo::item_t,5>::Alloc();
