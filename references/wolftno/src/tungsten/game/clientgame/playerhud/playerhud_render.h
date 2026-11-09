#pragma once

#include "tungsten/game/game_runtime.h"

#include <array>
#include <string>
#include <unordered_map>
#include <vector>

class idClothComponent {
public:
  idClothComponent();
  bool IsEnabled() const { return enabled; }
private:
  bool enabled{true};
};

class idPlayerHud_Render {
public:
  struct idAmmoGroupNumbers { bool Acquire(const std::string& path); bool acquired{false}; std::string binding; };
  struct idAmmoGroupIcons { bool Acquire(const std::string& path); bool acquired{false}; std::string binding; };
  struct idAmmoGroupRight {
    void Init(); void UpdatePocket(int count); void UpdateLaser(int charge);
    void UpdateAmmoNumbersRight(int loaded, int reserve); void UpdateRedAmmoState();
    void Update(); int pocket{0}; int laser{0}; int loaded{0}; int reserve{0}; bool red{false};
  };
  struct idAmmoGroupLeft {
    void Init(); void UpdateAmmoNumbersLeft(int loaded, int reserve); void Update();
    int loaded{0}; int reserve{0};
  };
  struct idAmmoGroupQuickSlot { void Init(); void Update(int slot, int count); int slot{-1}; int count{0}; };
  struct idCrossHair {
    void Init(); void UpdateAllCrossSmallLeanVisibility(bool value); void Update(float spread);
    bool leanVisible{false}; float spread{0}; bool initialized{false};
  };
  struct idDangers { idDangers(); void Reset(); void Init(); void Update(int count); int count{0}; bool initialized{false}; };
  struct idInteractionElement {
    void Init(const std::string& iconName); void SetPickupAnimCount(int value); void Show(bool value);
    std::string icon; int pickupAnimCount{0}; bool visible{false};
  };
  struct idInteraction {
    idInteraction(); void CenterIcons(); void Init(); void UpdateInteraction(const std::string& prompt); void Update();
    std::array<idInteractionElement,3> elements{}; std::string prompt; int updateCount{0};
  };
  struct idCombinerItemInstance { void Init(int value); int value{-1}; };
  struct idCombinerItem { void Update(int value); idCombinerItemInstance instance; };
  struct idHudFader { void Init(); void Update(float target, float dt); float alpha{0}; };
  struct idSavingIcon { void Init(); bool visible{false}; };
  struct idHealthAndArmor {
    void Init(); void Update(int healthValue,int armorValue); void UpdateHealthAnimation();
    int health{100}; int armor{0}; int previousHealth{100}; bool damagePulse{false};
  };
  struct idJournal { void Init(); void Update(int entries); int entries{0}; };
  struct idRadioVoice { void Init(); void Update(bool speaking); bool speaking{false}; };
  struct idPickupLog { void Init(); void UpdateLogNotification(const std::string& text); void Update(); std::vector<std::string> entries; };
  struct idCommanderSlot { void SetDistanceText(int distance); void Update(bool active); std::string distanceText; bool active{false}; };
  struct idCommanderSlotGroup { void Init(); void Update(int activeCount); std::array<idCommanderSlot,3> slots{}; };
  struct idCombinerGroup { idCombinerGroup(); void Init(); void SetVisibility(bool value); void Update(int value); bool visible{false}; int value{0}; };
  struct idWeaponSlot { void Init(int slotNumber); int slot{-1}; bool initialized{false}; };
  struct idWeaponSelection {
    idWeaponSelection(); void Init(); void SetWeaponSelection(int weapon); void ResetWeaponSelection();
    void PlaySound(const std::string& sound); void UpdateCurrentSelection(int weapon); void Update();
    int selection{-1}; int previous{-1}; int updateCount{0}; std::string lastSound;
  };

  idPlayerHud_Render();
  virtual ~idPlayerHud_Render();
  void NoteSwfForBuildGame(const std::string& swf);
  bool IsPlayerControlInhibited() const;
  void Cleanup();
  bool HandleGuiEvent(int key, bool pressed);
  void SetPlayerNum(int player);
  void SetNumberGroup(const std::string& group, int value);
  void ResetAmmoGroup();
  void UpdateQuickAmmoSlot(int slot, int count);
  void Init();
  void SetHudValue(const std::string& name, float value);
  void Render(int currentTime);

  void SetMenuOpen(bool value) { menuOpen=value; }
  bool IsInitialized() const { return initialized; }
  int GetRenderCount() const { return renderCount; }
  int GetPlayerNum() const { return playerNum; }
  float GetHudValue(const std::string& name) const;
  const std::vector<std::string>& GetBuildSwfs() const { return buildSwfs; }
  idHealthAndArmor& Health() { return healthAndArmor; }
  idWeaponSelection& Weapons() { return weaponSelection; }
  idAmmoGroupQuickSlot& QuickSlot() { return quickSlot; }

private:
  idAmmoGroupRight ammoRight;
  idAmmoGroupLeft ammoLeft;
  idAmmoGroupQuickSlot quickSlot;
  idCrossHair crosshair;
  idDangers dangers;
  idInteraction interaction;
  idHealthAndArmor healthAndArmor;
  idWeaponSelection weaponSelection;
  std::vector<std::string> buildSwfs;
  std::unordered_map<std::string,float> hudValues;
  int playerNum{-1}; int renderCount{0}; bool initialized{false}; bool menuOpen{false};
};

extern idCVar g_drawTVSafe;
extern idCVar g_showAreaText;
extern idCVar gc_coop_results_tempHUD;
extern idCVar g_useMPReferenceHUD;
