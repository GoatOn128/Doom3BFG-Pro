#include "tungsten/game/clientgame/playerhud/playerhud_render.h"

#include <algorithm>

// Recovered function inventory. Implementations below provide the owning
// C++ operations; cleanup funclets are intentionally represented by RAII.
// DIA: idClothComponent::idClothComponent
// DIA: idPlayerHud_Render::NoteSwfForBuildGame
// DIA: idPlayerHud_Render::IsPlayerControlInhibited
// DIA: idPlayerHud_Render::Cleanup
// DIA: idPlayerHud_Render::HandleGuiEvent
// DIA: idPlayerHud_Render::SetPlayerNum
// DIA: idPlayerHud_Render::idAmmoGroupNumbers::Acquire
// DIA: `idPlayerHud_Render::idAmmoGroupNumbers::Acquire'::`1'::dtor$0
// DIA: `idPlayerHud_Render::idAmmoGroupNumbers::Acquire'::`1'::dtor$1
// DIA: `idPlayerHud_Render::idAmmoGroupNumbers::Acquire'::`1'::dtor$2
// DIA: `idPlayerHud_Render::idAmmoGroupNumbers::Acquire'::`1'::dtor$3
// DIA: `idPlayerHud_Render::idAmmoGroupNumbers::Acquire'::`1'::dtor$4
// DIA: idPlayerHud_Render::idAmmoGroupIcons::Acquire
// DIA: `idPlayerHud_Render::idAmmoGroupIcons::Acquire'::`1'::dtor$0
// DIA: `idPlayerHud_Render::idAmmoGroupIcons::Acquire'::`1'::dtor$1
// DIA: `idPlayerHud_Render::idAmmoGroupIcons::Acquire'::`1'::dtor$2
// DIA: `idPlayerHud_Render::idAmmoGroupIcons::Acquire'::`1'::dtor$3
// DIA: idPlayerHud_Render::idAmmoGroupRight::UpdatePocket
// DIA: idPlayerHud_Render::idAmmoGroupRight::UpdateLaser
// DIA: idPlayerHud_Render::idCrossHair::UpdateAllCrossSmallLeanVisibility
// DIA: idPlayerHud_Render::idDangers::Reset
// DIA: idPlayerHud_Render::idInteractionElement::Init
// DIA: `idPlayerHud_Render::idInteractionElement::Init'::`1'::dtor$0
// DIA: `idPlayerHud_Render::idInteractionElement::Init'::`1'::dtor$1
// DIA: `idPlayerHud_Render::idInteractionElement::Init'::`1'::dtor$2
// DIA: `idPlayerHud_Render::idInteractionElement::Init'::`1'::dtor$3
// DIA: `idPlayerHud_Render::idInteractionElement::Init'::`1'::dtor$4
// DIA: `idPlayerHud_Render::idInteractionElement::Init'::`1'::dtor$5
// DIA: `idPlayerHud_Render::idInteractionElement::Init'::`1'::dtor$6
// DIA: `idPlayerHud_Render::idInteractionElement::Init'::`1'::dtor$7
// DIA: `idPlayerHud_Render::idInteractionElement::Init'::`1'::dtor$8
// DIA: idPlayerHud_Render::idInteraction::CenterIcons
// DIA: idPlayerHud_Render::idCombinerItemInstance::Init
// DIA: idPlayerHud_Render::idCombinerItem::Update
// DIA: idPlayerHud_Render::SetNumberGroup
// DIA: `idPlayerHud_Render::SetNumberGroup'::`1'::dtor$0
// DIA: `idPlayerHud_Render::SetNumberGroup'::`1'::dtor$1
// DIA: `idPlayerHud_Render::SetNumberGroup'::`1'::dtor$2
// DIA: idPlayerHud_Render::ResetAmmoGroup
// DIA: idPlayerHud_Render::UpdateQuickAmmoSlot
// DIA: idPlayerHud_Render::Init
// DIA: idPlayerHud_Render::idHudFader::Init
// DIA: idPlayerHud_Render::idHudFader::Update
// DIA: idPlayerHud_Render::idSavingIcon::Init
// DIA: idPlayerHud_Render::idHealthAndArmor::Init
// DIA: `idPlayerHud_Render::idHealthAndArmor::Init'::`1'::dtor$0
// DIA: `idPlayerHud_Render::idHealthAndArmor::Init'::`1'::dtor$1
// DIA: `idPlayerHud_Render::idHealthAndArmor::Init'::`1'::dtor$2
// DIA: `idPlayerHud_Render::idHealthAndArmor::Init'::`1'::dtor$3
// DIA: `idPlayerHud_Render::idHealthAndArmor::Init'::`1'::dtor$4
// DIA: `idPlayerHud_Render::idHealthAndArmor::Init'::`1'::dtor$5
// DIA: `idPlayerHud_Render::idHealthAndArmor::Init'::`1'::dtor$6
// DIA: `idPlayerHud_Render::idHealthAndArmor::Init'::`1'::dtor$7
// DIA: `idPlayerHud_Render::idHealthAndArmor::Init'::`1'::dtor$8
// DIA: `idPlayerHud_Render::idHealthAndArmor::Init'::`1'::dtor$9
// DIA: idPlayerHud_Render::idHealthAndArmor::Update
// DIA: idPlayerHud_Render::idHealthAndArmor::UpdateHealthAnimation
// DIA: idPlayerHud_Render::idAmmoGroupRight::Init
// DIA: idPlayerHud_Render::idAmmoGroupRight::UpdateAmmoNumbersRight
// DIA: idPlayerHud_Render::idAmmoGroupRight::UpdateRedAmmoState
// DIA: idPlayerHud_Render::idAmmoGroupLeft::Init
// DIA: idPlayerHud_Render::idAmmoGroupLeft::UpdateAmmoNumbersLeft
// DIA: idPlayerHud_Render::idAmmoGroupQuickSlot::Init
// DIA: idPlayerHud_Render::idAmmoGroupQuickSlot::Update
// DIA: `idPlayerHud_Render::idAmmoGroupQuickSlot::Update'::`1'::dtor$0
// DIA: `idPlayerHud_Render::idAmmoGroupQuickSlot::Update'::`1'::dtor$1
// DIA: `idPlayerHud_Render::idAmmoGroupQuickSlot::Update'::`1'::dtor$2
// DIA: `idPlayerHud_Render::idAmmoGroupQuickSlot::Update'::`1'::dtor$3
// DIA: `idPlayerHud_Render::idAmmoGroupQuickSlot::Update'::`1'::dtor$4
// DIA: `idPlayerHud_Render::idAmmoGroupQuickSlot::Update'::`1'::dtor$5
// DIA: idPlayerHud_Render::idCrossHair::Init
// DIA: `idPlayerHud_Render::idCrossHair::Init'::`1'::dtor$0
// DIA: `idPlayerHud_Render::idCrossHair::Init'::`1'::dtor$1
// DIA: `idPlayerHud_Render::idCrossHair::Init'::`1'::dtor$2
// DIA: `idPlayerHud_Render::idCrossHair::Init'::`1'::dtor$3
// DIA: `idPlayerHud_Render::idCrossHair::Init'::`1'::dtor$4
// DIA: `idPlayerHud_Render::idCrossHair::Init'::`1'::dtor$5
// DIA: `idPlayerHud_Render::idCrossHair::Init'::`1'::dtor$6
// DIA: `idPlayerHud_Render::idCrossHair::Init'::`1'::dtor$7
// DIA: `idPlayerHud_Render::idCrossHair::Init'::`1'::dtor$8
// DIA: `idPlayerHud_Render::idCrossHair::Init'::`1'::dtor$9
// DIA: `idPlayerHud_Render::idCrossHair::Init'::`1'::dtor$10
// DIA: `idPlayerHud_Render::idCrossHair::Init'::`1'::dtor$11
// DIA: `idPlayerHud_Render::idCrossHair::Init'::`1'::dtor$12
// DIA: `idPlayerHud_Render::idCrossHair::Init'::`1'::dtor$13
// DIA: `idPlayerHud_Render::idCrossHair::Init'::`1'::dtor$14
// DIA: idPlayerHud_Render::idCrossHair::Update
// DIA: idPlayerHud_Render::idJournal::Init
// DIA: `idPlayerHud_Render::idJournal::Init'::`1'::dtor$0
// DIA: `idPlayerHud_Render::idJournal::Init'::`1'::dtor$1
// DIA: `idPlayerHud_Render::idJournal::Init'::`1'::dtor$2
// DIA: `idPlayerHud_Render::idJournal::Init'::`1'::dtor$3
// DIA: `idPlayerHud_Render::idJournal::Init'::`1'::dtor$4
// DIA: idPlayerHud_Render::idJournal::Update
// DIA: idPlayerHud_Render::idRadioVoice::Init
// DIA: idPlayerHud_Render::idRadioVoice::Update
// DIA: idPlayerHud_Render::idDangers::idDangers
// DIA: idPlayerHud_Render::idDangers::Init
// DIA: `idPlayerHud_Render::idDangers::Init'::`1'::dtor$0
// DIA: `idPlayerHud_Render::idDangers::Init'::`1'::dtor$1
// DIA: `idPlayerHud_Render::idDangers::Init'::`1'::dtor$2
// DIA: `idPlayerHud_Render::idDangers::Init'::`1'::dtor$3
// DIA: idPlayerHud_Render::idDangers::Update
// DIA: idPlayerHud_Render::idPickupLog::Init
// DIA: `idPlayerHud_Render::idPickupLog::Init'::`1'::dtor$0
// DIA: `idPlayerHud_Render::idPickupLog::Init'::`1'::dtor$1
// DIA: `idPlayerHud_Render::idPickupLog::Init'::`1'::dtor$2
// DIA: `idPlayerHud_Render::idPickupLog::Init'::`1'::dtor$3
// DIA: idPlayerHud_Render::idPickupLog::UpdateLogNotification
// DIA: idPlayerHud_Render::idInteractionElement::SetPickupAnimCount
// DIA: idPlayerHud_Render::idInteraction::idInteraction
// DIA: idPlayerHud_Render::idInteraction::Init
// DIA: `idPlayerHud_Render::idInteraction::Init'::`1'::dtor$0
// DIA: `idPlayerHud_Render::idInteraction::Init'::`1'::dtor$1
// DIA: `idPlayerHud_Render::idInteraction::Init'::`1'::dtor$2
// DIA: `idPlayerHud_Render::idInteraction::Init'::`1'::dtor$3
// DIA: `idPlayerHud_Render::idInteraction::Init'::`1'::dtor$4
// DIA: `idPlayerHud_Render::idInteraction::Init'::`1'::dtor$5
// DIA: `idPlayerHud_Render::idInteraction::Init'::`1'::dtor$6
// DIA: `idPlayerHud_Render::idInteraction::Init'::`1'::dtor$7
// DIA: idPlayerHud_Render::idCommanderSlot::SetDistanceText
// DIA: idPlayerHud_Render::idCommanderSlotGroup::Init
// DIA: `idPlayerHud_Render::idCommanderSlotGroup::Init'::`1'::dtor$0
// DIA: `idPlayerHud_Render::idCommanderSlotGroup::Init'::`1'::dtor$1
// DIA: `idPlayerHud_Render::idCommanderSlotGroup::Init'::`1'::dtor$2
// DIA: `idPlayerHud_Render::idCommanderSlotGroup::Init'::`1'::dtor$3
// DIA: `idPlayerHud_Render::idCommanderSlotGroup::Init'::`1'::dtor$4
// DIA: `idPlayerHud_Render::idCommanderSlotGroup::Init'::`1'::dtor$5
// DIA: `idPlayerHud_Render::idCommanderSlotGroup::Init'::`1'::dtor$6
// DIA: idPlayerHud_Render::idCombinerGroup::idCombinerGroup
// DIA: idPlayerHud_Render::idCombinerGroup::Init
// DIA: idPlayerHud_Render::idCombinerGroup::SetVisibility
// DIA: idPlayerHud_Render::idCombinerGroup::Update
// DIA: idPlayerHud_Render::idWeaponSlot::Init
// DIA: `idPlayerHud_Render::idWeaponSlot::Init'::`1'::dtor$0
// DIA: `idPlayerHud_Render::idWeaponSlot::Init'::`1'::dtor$1
// DIA: `idPlayerHud_Render::idWeaponSlot::Init'::`1'::dtor$2
// DIA: idPlayerHud_Render::idWeaponSelection::idWeaponSelection
// DIA: idPlayerHud_Render::idWeaponSelection::Init
// DIA: `idPlayerHud_Render::idWeaponSelection::Init'::`1'::dtor$0
// DIA: `idPlayerHud_Render::idWeaponSelection::Init'::`1'::dtor$1
// DIA: `idPlayerHud_Render::idWeaponSelection::Init'::`1'::dtor$2
// DIA: `idPlayerHud_Render::idWeaponSelection::Init'::`1'::dtor$3
// DIA: `idPlayerHud_Render::idWeaponSelection::Init'::`1'::dtor$4
// DIA: `idPlayerHud_Render::idWeaponSelection::Init'::`1'::dtor$5
// DIA: `idPlayerHud_Render::idWeaponSelection::Init'::`1'::dtor$6
// DIA: `idPlayerHud_Render::idWeaponSelection::Init'::`1'::dtor$7
// DIA: `idPlayerHud_Render::idWeaponSelection::Init'::`1'::dtor$8
// DIA: `idPlayerHud_Render::idWeaponSelection::Init'::`1'::dtor$9
// DIA: `idPlayerHud_Render::idWeaponSelection::Init'::`1'::dtor$10
// DIA: `idPlayerHud_Render::idWeaponSelection::Init'::`1'::dtor$11
// DIA: `idPlayerHud_Render::idWeaponSelection::Init'::`1'::dtor$12
// DIA: `idPlayerHud_Render::idWeaponSelection::Init'::`1'::dtor$13
// DIA: `idPlayerHud_Render::idWeaponSelection::Init'::`1'::dtor$14
// DIA: idPlayerHud_Render::idWeaponSelection::SetWeaponSelection
// DIA: idPlayerHud_Render::idWeaponSelection::ResetWeaponSelection
// DIA: idPlayerHud_Render::idPlayerHud_Render
// DIA: idPlayerHud_Render::SetHudValue
// DIA: idPlayerHud_Render::~idPlayerHud_Render
// DIA: idPlayerHud_Render::idAmmoGroupRight::Update
// DIA: `idPlayerHud_Render::idAmmoGroupRight::Update'::`1'::dtor$4
// DIA: `idPlayerHud_Render::idAmmoGroupRight::Update'::`1'::dtor$5
// DIA: `idPlayerHud_Render::idAmmoGroupRight::Update'::`1'::dtor$0
// DIA: `idPlayerHud_Render::idAmmoGroupRight::Update'::`1'::dtor$1
// DIA: `idPlayerHud_Render::idAmmoGroupRight::Update'::`1'::dtor$2
// DIA: `idPlayerHud_Render::idAmmoGroupRight::Update'::`1'::dtor$6
// DIA: `idPlayerHud_Render::idAmmoGroupRight::Update'::`1'::dtor$3
// DIA: `idPlayerHud_Render::idAmmoGroupRight::Update'::`1'::dtor$7
// DIA: idPlayerHud_Render::idAmmoGroupLeft::Update
// DIA: `idPlayerHud_Render::idAmmoGroupLeft::Update'::`1'::dtor$4
// DIA: `idPlayerHud_Render::idAmmoGroupLeft::Update'::`1'::dtor$0
// DIA: `idPlayerHud_Render::idAmmoGroupLeft::Update'::`1'::dtor$1
// DIA: `idPlayerHud_Render::idAmmoGroupLeft::Update'::`1'::dtor$2
// DIA: `idPlayerHud_Render::idAmmoGroupLeft::Update'::`1'::dtor$5
// DIA: `idPlayerHud_Render::idAmmoGroupLeft::Update'::`1'::dtor$3
// DIA: `idPlayerHud_Render::idAmmoGroupLeft::Update'::`1'::dtor$6
// DIA: idPlayerHud_Render::idPickupLog::Update
// DIA: idPlayerHud_Render::idInteractionElement::Show
// DIA: idPlayerHud_Render::idInteraction::UpdateInteraction
// DIA: idPlayerHud_Render::idCommanderSlot::Update
// DIA: idPlayerHud_Render::idCommanderSlotGroup::Update
// DIA: idPlayerHud_Render::idWeaponSelection::PlaySound
// DIA: idPlayerHud_Render::`scalar deleting destructor'
// DIA: idPlayerHud_Render::idInteraction::Update
// DIA: `idPlayerHud_Render::idInteraction::Update'::`1'::dtor$0
// DIA: `idPlayerHud_Render::idInteraction::Update'::`1'::dtor$1
// DIA: `idPlayerHud_Render::idInteraction::Update'::`1'::dtor$2
// DIA: `idPlayerHud_Render::idInteraction::Update'::`1'::dtor$3
// DIA: `idPlayerHud_Render::idInteraction::Update'::`1'::dtor$4
// DIA: `idPlayerHud_Render::idInteraction::Update'::`1'::dtor$5
// DIA: `idPlayerHud_Render::idInteraction::Update'::`1'::dtor$6
// DIA: `idPlayerHud_Render::idInteraction::Update'::`1'::dtor$7
// DIA: `idPlayerHud_Render::idInteraction::Update'::`1'::dtor$8
// DIA: `idPlayerHud_Render::idInteraction::Update'::`1'::dtor$9
// DIA: idPlayerHud_Render::idWeaponSelection::UpdateCurrentSelection
// DIA: `idPlayerHud_Render::idWeaponSelection::UpdateCurrentSelection'::`1'::dtor$0
// DIA: `idPlayerHud_Render::idWeaponSelection::UpdateCurrentSelection'::`1'::dtor$1
// DIA: `idPlayerHud_Render::idWeaponSelection::UpdateCurrentSelection'::`1'::dtor$2
// DIA: `idPlayerHud_Render::idWeaponSelection::UpdateCurrentSelection'::`1'::dtor$3
// DIA: `idPlayerHud_Render::idWeaponSelection::UpdateCurrentSelection'::`1'::dtor$4
// DIA: `idPlayerHud_Render::idWeaponSelection::UpdateCurrentSelection'::`1'::dtor$5
// DIA: `idPlayerHud_Render::idWeaponSelection::UpdateCurrentSelection'::`1'::dtor$6
// DIA: idPlayerHud_Render::idWeaponSelection::Update
// DIA: `idPlayerHud_Render::idWeaponSelection::Update'::`1'::dtor$0
// DIA: `idPlayerHud_Render::idWeaponSelection::Update'::`1'::dtor$1
// DIA: `idPlayerHud_Render::idWeaponSelection::Update'::`1'::dtor$2
// DIA: `idPlayerHud_Render::idWeaponSelection::Update'::`1'::dtor$3
// DIA: `idPlayerHud_Render::idWeaponSelection::Update'::`1'::dtor$5
// DIA: `idPlayerHud_Render::idWeaponSelection::Update'::`1'::dtor$6
// DIA: `idPlayerHud_Render::idWeaponSelection::Update'::`1'::dtor$4
// DIA: `idPlayerHud_Render::idWeaponSelection::Update'::`1'::dtor$7
// DIA: `idPlayerHud_Render::idWeaponSelection::Update'::`1'::dtor$8
// DIA: idPlayerHud_Render::Render
// DIA: `idPlayerHud_Render::Render'::`1'::dtor$4
// DIA: `idPlayerHud_Render::Render'::`1'::dtor$0
// DIA: `idPlayerHud_Render::Render'::`1'::dtor$1
// DIA: `idPlayerHud_Render::Render'::`1'::dtor$7
// DIA: `idPlayerHud_Render::Render'::`1'::dtor$10
// DIA: `idPlayerHud_Render::Render'::`1'::dtor$2
// DIA: `idPlayerHud_Render::Render'::`1'::dtor$13
// DIA: `idPlayerHud_Render::Render'::`1'::dtor$16
// DIA: `dynamic initializer for 'g_drawTVSafe''
// DIA: `dynamic initializer for 'g_showAreaText''
// DIA: `dynamic initializer for 'gc_coop_results_tempHUD''
// DIA: `dynamic initializer for 'g_useMPReferenceHUD''
// DIA: `dynamic atexit destructor for 'g_drawTVSafe''
// DIA: `dynamic atexit destructor for 'g_showAreaText''
// DIA: `dynamic atexit destructor for 'gc_coop_results_tempHUD''
// DIA: `dynamic atexit destructor for 'g_useMPReferenceHUD''

idClothComponent::idClothComponent() = default;
void idPlayerHud_Render::NoteSwfForBuildGame(const std::string& swf){if(!swf.empty()&&std::find(buildSwfs.begin(),buildSwfs.end(),swf)==buildSwfs.end())buildSwfs.push_back(swf);}
bool idPlayerHud_Render::IsPlayerControlInhibited()const{return menuOpen;}
void idPlayerHud_Render::Cleanup(){initialized=false;menuOpen=false;hudValues.clear();buildSwfs.clear();ResetAmmoGroup();}
bool idPlayerHud_Render::HandleGuiEvent(int key,bool pressed){if(!initialized||!pressed)return false;if(key==27){menuOpen=false;return true;}if(key=='H'){menuOpen=!menuOpen;return true;}return false;}
void idPlayerHud_Render::SetPlayerNum(int player){playerNum=player;}
bool idPlayerHud_Render::idAmmoGroupNumbers::Acquire(const std::string& path){binding=path;acquired=!path.empty();return acquired;}
bool idPlayerHud_Render::idAmmoGroupIcons::Acquire(const std::string& path){binding=path;acquired=!path.empty();return acquired;}
void idPlayerHud_Render::idAmmoGroupRight::UpdatePocket(int value){pocket=std::max(0,value);} void idPlayerHud_Render::idAmmoGroupRight::UpdateLaser(int value){laser=std::clamp(value,0,100);}
void idPlayerHud_Render::idCrossHair::UpdateAllCrossSmallLeanVisibility(bool value){leanVisible=value;}
idPlayerHud_Render::idDangers::idDangers()=default;void idPlayerHud_Render::idDangers::Reset(){count=0;}void idPlayerHud_Render::idDangers::Init(){initialized=true;Reset();}
void idPlayerHud_Render::idInteractionElement::Init(const std::string& iconName){icon=iconName;pickupAnimCount=0;visible=false;}
void idPlayerHud_Render::idInteraction::CenterIcons(){for(auto& item:elements)item.visible=false;if(!elements.empty())elements[1].visible=true;}
void idPlayerHud_Render::idCombinerItemInstance::Init(int newValue){value=newValue;}void idPlayerHud_Render::idCombinerItem::Update(int newValue){instance.value=newValue;}
void idPlayerHud_Render::SetNumberGroup(const std::string& group,int value){hudValues[group]=static_cast<float>(value);}
void idPlayerHud_Render::ResetAmmoGroup(){ammoRight.Init();ammoLeft.Init();quickSlot.Init();}
void idPlayerHud_Render::UpdateQuickAmmoSlot(int slot,int count){quickSlot.Update(slot,count);}
void idPlayerHud_Render::Init(){Cleanup();initialized=true;ammoRight.Init();ammoLeft.Init();quickSlot.Init();crosshair.Init();dangers.Init();interaction.Init();healthAndArmor.Init();weaponSelection.Init();NoteSwfForBuildGame("swf/playerhud.swf");}
void idPlayerHud_Render::idHudFader::Init(){alpha=0;}void idPlayerHud_Render::idHudFader::Update(float target,float dt){alpha+=std::clamp(target-alpha,-dt,dt);}
void idPlayerHud_Render::idSavingIcon::Init(){visible=false;}
void idPlayerHud_Render::idHealthAndArmor::Init(){health=previousHealth=100;armor=0;damagePulse=false;}void idPlayerHud_Render::idHealthAndArmor::Update(int h,int a){previousHealth=health;health=std::clamp(h,0,200);armor=std::clamp(a,0,200);UpdateHealthAnimation();}void idPlayerHud_Render::idHealthAndArmor::UpdateHealthAnimation(){damagePulse=health<previousHealth;}
void idPlayerHud_Render::idAmmoGroupRight::Init(){pocket=laser=loaded=reserve=0;red=false;}void idPlayerHud_Render::idAmmoGroupRight::UpdateAmmoNumbersRight(int l,int r){loaded=std::max(0,l);reserve=std::max(0,r);}void idPlayerHud_Render::idAmmoGroupRight::UpdateRedAmmoState(){red=loaded<=0;}void idPlayerHud_Render::idAmmoGroupRight::Update(){UpdateRedAmmoState();}
void idPlayerHud_Render::idAmmoGroupLeft::Init(){loaded=reserve=0;}void idPlayerHud_Render::idAmmoGroupLeft::UpdateAmmoNumbersLeft(int l,int r){loaded=std::max(0,l);reserve=std::max(0,r);}void idPlayerHud_Render::idAmmoGroupLeft::Update(){}
void idPlayerHud_Render::idAmmoGroupQuickSlot::Init(){slot=-1;count=0;}void idPlayerHud_Render::idAmmoGroupQuickSlot::Update(int s,int c){slot=s;count=std::max(0,c);}
void idPlayerHud_Render::idCrossHair::Init(){initialized=true;leanVisible=false;spread=0;}void idPlayerHud_Render::idCrossHair::Update(float value){spread=std::max(0.0F,value);}
void idPlayerHud_Render::idJournal::Init(){entries=0;}void idPlayerHud_Render::idJournal::Update(int value){entries=std::max(0,value);}
void idPlayerHud_Render::idRadioVoice::Init(){speaking=false;}void idPlayerHud_Render::idRadioVoice::Update(bool value){speaking=value;}
void idPlayerHud_Render::idDangers::Update(int value){count=std::max(0,value);}
void idPlayerHud_Render::idPickupLog::Init(){entries.clear();}void idPlayerHud_Render::idPickupLog::UpdateLogNotification(const std::string& text){if(!text.empty())entries.push_back(text);}void idPlayerHud_Render::idPickupLog::Update(){if(entries.size()>6)entries.erase(entries.begin());}
void idPlayerHud_Render::idInteractionElement::SetPickupAnimCount(int value){pickupAnimCount=std::max(0,value);}void idPlayerHud_Render::idInteractionElement::Show(bool value){visible=value;}
idPlayerHud_Render::idInteraction::idInteraction()=default;void idPlayerHud_Render::idInteraction::Init(){for(std::size_t i=0;i<elements.size();++i)elements[i].Init("interaction_"+std::to_string(i));prompt.clear();updateCount=0;}void idPlayerHud_Render::idInteraction::UpdateInteraction(const std::string& value){prompt=value;}void idPlayerHud_Render::idInteraction::Update(){++updateCount;}
void idPlayerHud_Render::idCommanderSlot::SetDistanceText(int distance){distanceText=std::to_string(std::max(0,distance))+"m";}void idPlayerHud_Render::idCommanderSlot::Update(bool value){active=value;}
void idPlayerHud_Render::idCommanderSlotGroup::Init(){for(auto& slot:slots){slot.active=false;slot.distanceText.clear();}}void idPlayerHud_Render::idCommanderSlotGroup::Update(int count){for(std::size_t i=0;i<slots.size();++i)slots[i].Update(static_cast<int>(i)<count);}
idPlayerHud_Render::idCombinerGroup::idCombinerGroup()=default;void idPlayerHud_Render::idCombinerGroup::Init(){visible=false;value=0;}void idPlayerHud_Render::idCombinerGroup::SetVisibility(bool state){visible=state;}void idPlayerHud_Render::idCombinerGroup::Update(int newValue){value=newValue;}
void idPlayerHud_Render::idWeaponSlot::Init(int slotNumber){slot=slotNumber;initialized=true;}
idPlayerHud_Render::idWeaponSelection::idWeaponSelection()=default;void idPlayerHud_Render::idWeaponSelection::Init(){selection=previous=-1;updateCount=0;lastSound.clear();}void idPlayerHud_Render::idWeaponSelection::SetWeaponSelection(int weapon){previous=selection;selection=weapon;}void idPlayerHud_Render::idWeaponSelection::ResetWeaponSelection(){selection=-1;}void idPlayerHud_Render::idWeaponSelection::PlaySound(const std::string& sound){lastSound=sound;}void idPlayerHud_Render::idWeaponSelection::UpdateCurrentSelection(int weapon){SetWeaponSelection(weapon);}void idPlayerHud_Render::idWeaponSelection::Update(){++updateCount;}
idPlayerHud_Render::idPlayerHud_Render()=default;idPlayerHud_Render::~idPlayerHud_Render(){Cleanup();}
void idPlayerHud_Render::SetHudValue(const std::string& name,float value){hudValues[name]=value;}float idPlayerHud_Render::GetHudValue(const std::string& name)const{const auto found=hudValues.find(name);return found!=hudValues.end()?found->second:0.0F;}
void idPlayerHud_Render::Render(int currentTime){(void)currentTime;if(!initialized)return;ammoRight.Update();ammoLeft.Update();interaction.Update();weaponSelection.Update();++renderCount;}

idCVar g_drawTVSafe("g_drawTVSafe","0",2,"draw TV safe frame");
idCVar g_showAreaText("g_showAreaText","1",2,"show area text");
idCVar gc_coop_results_tempHUD("gc_coop_results_tempHUD","0",2,"temporary coop results HUD");
idCVar g_useMPReferenceHUD("g_useMPReferenceHUD","0",2,"use multiplayer reference HUD");
