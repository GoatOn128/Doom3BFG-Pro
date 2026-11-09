#pragma once

#include "tungsten/game/ai/corpsemanager.h"

class idDroppedItemManager {
public:
  struct droppedItemSave_t {
    droppedItemSave_t();
    idStr def;
    idVec3 position{};
    idQuat orientation{0.0F, 0.0F, 0.0F, 1.0F};
    int ammoCount{0};
  };

  struct trackedItem_t {
    idEntity* entity{nullptr};
    droppedItemSave_t save;
    bool visible{false};
    bool hasDropOwner{false};
  };

  class spawner_t {
  public:
    virtual ~spawner_t() = default;
    virtual idEntity* SpawnDroppedItem(const droppedItemSave_t& saved) = 0;
  };

  idDroppedItemManager();
  bool IsEnabled() const;
  void CheckDroppedItemRemoval();
  void RemoveDroppedItem(idEntity* droppedItem);
  void Write(idCorpseFile* file, int savegameVersionMajor,
             std::int64_t savegameVersionMinor);
  void AddDroppedItem(idEntity* droppedItem);
  void AddDroppedItem(idEntity* droppedItem, const droppedItemSave_t& save,
                      bool visible, bool hasDropOwner);
  void SpawnDroppedItems();
  void Read(idCorpseFile* file, int savegameVersionMajor,
            std::int64_t savegameVersionMinor);

  void SetSpawner(spawner_t* value) { spawner = value; }
  int GetTrackedCount() const { return droppedItems.Num(); }
  int GetCheckpointCount() const { return checkpointDroppedItems.Num(); }
  const droppedItemSave_t& GetCheckpointItem(int index) const {
    return checkpointDroppedItems[index];
  }

private:
  idList<droppedItemSave_t, 5> checkpointDroppedItems;
  idList<trackedItem_t, 5> droppedItems;
  spawner_t* spawner{nullptr};
};

extern idCVar g_manageDroppedItems;
extern idCVar g_maxNumDroppedItems;
