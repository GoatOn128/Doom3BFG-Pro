#include "tungsten/game/clientgame/playerhud/playerhud_info.h"

#include <algorithm>

// DIA: idHudDangerInfo::CommanderExist
bool idHudDangerInfo::CommanderExist(int spawnIndex) const { for(int i=0;i<commanders.Num();++i)if(commanders[i].entityIndex==spawnIndex)return true;return false; }
// DIA: idHudInfo::weaponSelectionElement_t::Clear
void idHudInfo::weaponSelectionElement_t::Clear(){slotRef=-1;slotIndex=-1;icon.clear();weaponName.clear();weaponNameMulti.clear();hidden=false;dualWield=false;}
// DIA: idHudInfo::weaponSelection_t::ClearElements
void idHudInfo::weaponSelection_t::ClearElements(){for(auto& element:weaponElements)element.Clear();numWeapons=0;currentSelection=currentSelectArray=selectionAngleIndex=prevSelectionAngle=prevWeaponSelection=-1;}
// DIA: idHudInfo::weaponSelection_t::SetCurrentWeapon
bool idHudInfo::weaponSelection_t::SetCurrentWeapon(const void* player,int weapon,int angleIndex){(void)player;if(angleIndex!=selectionAngleIndex)prevSelectionAngle=selectionAngleIndex;if(weapon==-1){currentSelection=-1;selectionAngleIndex=angleIndex;return true;}for(int i=0;i<numWeapons;++i){const auto& element=weaponElements[static_cast<std::size_t>(i)];if(!element.hidden&&(element.slotRef==weapon||(element.dualWield&&element.slotRef+1==weapon))){prevWeaponSelection=currentSelection;currentSelection=weapon;currentSelectArray=i;if(angleIndex>0)selectionAngleIndex=angleIndex;return true;}}return false;}
// DIA: idHudDangerInfo::Clear
void idHudDangerInfo::Clear(){commanders.Clear();slotsTaken.fill(8);}
// DIA: idHudDangerInfo::UpdateCommanderSlots
void idHudDangerInfo::UpdateCommanderSlots(){for(int i=commanders.Num()-1;i>=0;--i)if((commanders[i].commanderFlags&0x80)!=0)commanders.RemoveIndex(i);}
// DIA: idHudDangerInfo::UpdateCommander
void idHudDangerInfo::UpdateCommander(int index,int currentTime){(void)currentTime;if(index<0||index>=commanders.Num())return;auto& commander=commanders[index];if(commander.arrayIndex<0||commander.arrayIndex>=3)return;const auto& danger=dangerEntries[static_cast<std::size_t>(commander.arrayIndex)];commander.entityIndex=danger.spawnIndex;commander.distanceToCommander=static_cast<int>(danger.playerRange*0.01905F);commander.dangerIndex=(danger.flags&2)!=0?2:1;if((danger.flags&1)!=0){commander.commanderFlags|=0x2000;commander.currentText=signalLost;commander.timer=800;}if((danger.flags&4)!=0){commander.commanderFlags|=0x30000;commander.currentText=terminated;commander.timer=danger.terminatedTimer;}if(commander.timer>0)commander.timer=std::max(0,commander.timer-16);}
// DIA: idList<idHudDangerInfo::commanderInfo_t,5>::Resize
// DIA: idList<idHudDangerInfo::commanderInfo_t,5>::Append
// DIA: idHudDangerInfo::idHudDangerInfo
idHudDangerInfo::idHudDangerInfo(){commanders.ResizeForPort(3);Clear();}
// DIA: `idHudDangerInfo::idHudDangerInfo'::`1'::dtor$0
// DIA: `idHudDangerInfo::idHudDangerInfo'::`1'::dtor$1
// DIA: `idHudDangerInfo::idHudDangerInfo'::`1'::dtor$2
// DIA: idHudDangerInfo::AddCommander
bool idHudDangerInfo::AddCommander(int arrayIndex){if(commanders.Num()>=3||arrayIndex<0||arrayIndex>=3)return false;commanderInfo_t item;item.commanderFlags=1361;item.arrayIndex=arrayIndex;item.timer=200+arrayIndex*100;commanders.Append(item);return true;}

template class idList<idHudDangerInfo::commanderInfo_t, 5>;
