#pragma once

#include "tungsten/game/ai/cover/aicover.h"

struct idClipModel {
  idBounds bounds{};
  const char* name{nullptr};
};

class idInventoryCollection {
public:
  int GetInventoryCount() const;
  void Add(const void* item) { inventory.Append(item); }

private:
  idList<const void*, 5> inventory;
};

class idDynamicCover : public idEntity, public idDynamicCoverProvider {
public:
  struct coverInfo_t {
    aas2Cover_t cover;
    std::uint64_t contentsQuery{0};
    std::uint64_t translationQuery{0};
    const idEntity* reserver{nullptr};
  };

  explicit idDynamicCover(int coverCount = 0);
  ~idDynamicCover() override;

  static const idClipModel* GetClipForAAS(const idAAS2* aas);
  static const char* GetPropNameForAAS(const idAAS2* aas);
  coverInfo_t* GetCoverPoint(int index);
  const coverInfo_t* GetCoverPoint(int index) const;
  aas2Cover_t* GetDynamicCoverPoint(const idAAS2* aas,
                                    std::int16_t index) const override;
  void MarkAllUsable();
  const idEntity* GetReserver(int index) const;
  void Reserve(int index, const idEntity* entity);
  void Unreserve(int index);
  int GetUsableTime(int index) const;
  void SetUsableTime(int index, int time);
  void MarkAllUnusable();
  int InternalAppendUsableCover(const idEntity* owner, const idAAS2* aas,
                                idList<idAICover, 5>* output) const;
  virtual void UpdateUsable(const idEntity* owner, const idAAS2* aas,
                            unsigned int currentTime);
  bool AppendUsableCover(int coverIndex);

  const idList<int, 5>& GetUsableCover() const { return usableCover; }
  int GetNumCover() const { return coverInfo.Num(); }
  bool IsUpdated() const { return updated; }
  void SetUpdated(bool value) { updated = value; }

private:
  idList<coverInfo_t, 5> coverInfo;
  idList<int, 5> usableCover;
  bool updated{false};
};

extern idCVar ai_debugDynamicCover;
