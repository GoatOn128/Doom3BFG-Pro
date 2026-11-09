#include "tungsten/game/components/ammoupgrades.h"
#include <algorithm>
// DIA: idAmmoUpgrades::ApplyUpgradeToList
// DIA: idAmmoUpgrades::SetDwAmmo
// DIA: idAmmoUpgrades::GetExtraAmmo
// DIA: `idAmmoUpgrades::GiveUpgradeAmmoAmount'::`5'::local_t::GiveAmmo
// DIA: idAmmoUpgrades::GiveUpgradeAmmoAmount
// DIA: idAmmoUpgrades::Init
// DIA: `dynamic initializer for 'g_debugAmmoUpgrades''
// DIA: `dynamic atexit destructor for 'g_debugAmmoUpgrades''
idCVar g_debugAmmoUpgrades("g_debugAmmoUpgrades","0",0,"ammo upgrade diagnostics");
void idAmmoUpgrades::ApplyUpgradeToList(const idDeclPlayerUpgradeAmmo*u){if(!u)return;for(int i=0;i<AMMO_TYPE_COUNT;++i)slots[static_cast<std::size_t>(i)].amount+=u->slots[static_cast<std::size_t>(i)].amount;}
void idAmmoUpgrades::SetDwAmmo(AmmoTypes_t type,int amount){if(type>=0&&type<AMMO_TYPE_COUNT)dwSlots[static_cast<std::size_t>(type)].amount=std::max(0,amount);}
int idAmmoUpgrades::GetExtraAmmo(AmmoTypes_t type)const{return type>=0&&type<AMMO_TYPE_COUNT?slots[static_cast<std::size_t>(type)].amount+dwSlots[static_cast<std::size_t>(type)].amount:0;}
void idAmmoUpgrades::GiveUpgradeAmmoAmount(idInventoryCollection&inventory,const idDeclPlayerUpgradeAmmo*u){if(!u)return;for(auto*item:inventory.items){auto*ammo=dynamic_cast<idAmmoItem*>(item);if(!ammo||ammo->id<0||ammo->id>=AMMO_TYPE_COUNT)continue;ammo->ammo=std::max(0,ammo->ammo+u->slots[static_cast<std::size_t>(ammo->id)].amount);}}
void idAmmoUpgrades::Init(idPlayer*player){(void)player;slots={};dwSlots={};}
