#pragma once
#include "tungsten/game/clientgame/presentable/presentableplayer_shared.h"
#include <array>
enum AmmoTypes_t{AMMO_NONE=0,AMMO_BULLET,AMMO_SHELL,AMMO_ROCKET,AMMO_GRENADE,AMMO_LASER,AMMO_DIESEL,AMMO_ELECTRIC,AMMO_SNIPER,AMMO_HEAVY,AMMO_SPECIAL,AMMO_TYPE_COUNT};
struct ammoUpgradeSlot_t{int amount{0};};
struct idDeclPlayerUpgradeAmmo{std::array<ammoUpgradeSlot_t,AMMO_TYPE_COUNT> slots{};};
class idAmmoUpgrades{public:void ApplyUpgradeToList(const idDeclPlayerUpgradeAmmo*upgrade);void SetDwAmmo(AmmoTypes_t type,int amount);int GetExtraAmmo(AmmoTypes_t type)const;void GiveUpgradeAmmoAmount(idInventoryCollection&inventory,const idDeclPlayerUpgradeAmmo*upgrade);void Init(idPlayer*player);private:std::array<ammoUpgradeSlot_t,AMMO_TYPE_COUNT>slots{};std::array<ammoUpgradeSlot_t,AMMO_TYPE_COUNT>dwSlots{};};
extern idCVar g_debugAmmoUpgrades;
