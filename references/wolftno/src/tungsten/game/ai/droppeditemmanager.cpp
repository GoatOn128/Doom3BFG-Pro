#include "tungsten/game/ai/droppeditemmanager.h"

#include <algorithm>

idCVar g_manageDroppedItems(
    "g_manageDroppedItems", "0", 1,
    "If true, manage dropped items separately from corpses.");
idCVar g_maxNumDroppedItems("g_maxNumDroppedItems", "12", 2,
                            "All dropped item above this will be removed.");

// DIA: idDroppedItemManager::droppedItemSave_t::droppedItemSave_t
idDroppedItemManager::droppedItemSave_t::droppedItemSave_t() = default;

// DIA: idDroppedItemManager::IsEnabled
bool idDroppedItemManager::IsEnabled() const {
  return g_manageDroppedItems.GetInteger() != 0;
}

// DIA: idDroppedItemManager::idDroppedItemManager
idDroppedItemManager::idDroppedItemManager() = default;

// DIA: idDroppedItemManager::CheckDroppedItemRemoval
void idDroppedItemManager::CheckDroppedItemRemoval() {
  const int maximum = g_maxNumDroppedItems.GetInteger();
  if (!IsEnabled() || maximum < 0) return;
  for (int attempts = 0; droppedItems.Num() > maximum && attempts < 5;
       ++attempts) {
    int selected = -1;
    for (int index = 0; index < droppedItems.Num(); ++index) {
      if (droppedItems[index].entity == nullptr ||
          !droppedItems[index].visible) {
        selected = index;
        break;
      }
    }
    if (selected < 0 && droppedItems.Num() > 0) selected = 0;
    if (selected < 0) break;
    if (droppedItems[selected].entity != nullptr) {
      droppedItems[selected].entity->Remove();
    }
    droppedItems.RemoveIndexFast(selected);
  }
}

// DIA: idList<idDroppedItemManager::droppedItemSave_t,5>::Resize
template bool idList<idDroppedItemManager::droppedItemSave_t, 5>::Resize(
    int newSize);

// DIA: idDroppedItemManager::RemoveDroppedItem
void idDroppedItemManager::RemoveDroppedItem(idEntity* droppedItem) {
  if (!IsEnabled()) return;
  for (int index = 0; index < droppedItems.Num(); ++index) {
    if (droppedItems[index].entity == droppedItem) {
      droppedItems.RemoveIndex(index);
      return;
    }
  }
}

// DIA: idDroppedItemManager::Write
void idDroppedItemManager::Write(idCorpseFile* file, int savegameVersionMajor,
                                 std::int64_t savegameVersionMinor) {
  (void)savegameVersionMajor;
  (void)savegameVersionMinor;
  if (file == nullptr) return;
  int count = 0;
  if (IsEnabled()) {
    for (int index = 0; index < droppedItems.Num() && count < 32; ++index) {
      if (droppedItems[index].entity != nullptr &&
          !droppedItems[index].hasDropOwner) {
        ++count;
      }
    }
  }
  file->WriteValue(count);
  int written = 0;
  for (int index = 0; index < droppedItems.Num() && written < count; ++index) {
    const trackedItem_t& item = droppedItems[index];
    if (item.entity == nullptr || item.hasDropOwner) continue;
    file->WriteValue(item.save.position);
    file->WriteValue(item.save.orientation);
    file->WriteString(item.save.def);
    file->WriteValue(item.save.ammoCount);
    ++written;
  }
}

// DIA: idDroppedItemManager::AddDroppedItem
void idDroppedItemManager::AddDroppedItem(idEntity* droppedItem) {
  AddDroppedItem(droppedItem, droppedItemSave_t{}, false, false);
}

void idDroppedItemManager::AddDroppedItem(idEntity* droppedItem,
                                          const droppedItemSave_t& save,
                                          bool visible,
                                          bool hasDropOwner) {
  if (!IsEnabled()) return;
  droppedItems.Append({droppedItem, save, visible, hasDropOwner});
}

// DIA: idDroppedItemManager::SpawnDroppedItems
void idDroppedItemManager::SpawnDroppedItems() {
  if (!IsEnabled() || spawner == nullptr) return;
  for (int index = 0; index < checkpointDroppedItems.Num(); ++index) {
    idEntity* entity = spawner->SpawnDroppedItem(checkpointDroppedItems[index]);
    if (entity != nullptr) {
      droppedItems.Append({entity, checkpointDroppedItems[index], false, false});
    }
  }
}

// DIA: idList<idDroppedItemManager::droppedItemSave_t,5>::Alloc
template idDroppedItemManager::droppedItemSave_t*
idList<idDroppedItemManager::droppedItemSave_t, 5>::Alloc();

// DIA: idDroppedItemManager::Read
void idDroppedItemManager::Read(idCorpseFile* file, int savegameVersionMajor,
                                std::int64_t savegameVersionMinor) {
  (void)savegameVersionMajor;
  (void)savegameVersionMinor;
  checkpointDroppedItems.ClearFree();
  if (file == nullptr) return;
  int count = 0;
  if (!file->ReadValue(count) || count < 0) return;
  for (int index = 0; index < count; ++index) {
    droppedItemSave_t saved;
    if (!file->ReadValue(saved.position) ||
        !file->ReadValue(saved.orientation) || !file->ReadString(saved.def) ||
        !file->ReadValue(saved.ammoCount)) {
      checkpointDroppedItems.Clear();
      return;
    }
    checkpointDroppedItems.Append(saved);
  }
  if (!IsEnabled()) checkpointDroppedItems.ClearFree();
}

// DIA: `dynamic initializer for 'g_manageDroppedItems''
// DIA: `dynamic initializer for 'g_maxNumDroppedItems''
// DIA: `dynamic atexit destructor for 'g_manageDroppedItems''
// DIA: `dynamic atexit destructor for 'g_maxNumDroppedItems''
