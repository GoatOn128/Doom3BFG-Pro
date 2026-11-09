#pragma once

#include "shared/idlib/containers/list.h"
#include "tungsten/game/game_runtime.h"

#include <cstdint>
#include <string>
#include <vector>

class idPlayer : public idEntity {
public:
  void* GetPlayerHud();
  void SetPlayerHud(void* value) { playerHud = value; }
private:
  void* playerHud{nullptr};
};

struct automapFloor_t {
  float coordinateScale{1.0F};
  int fogGridSize{32};
  int linkSecretId{-1};
};

struct automapBlock_t {
  idVec3 worldMin{};
  idVec3 worldMax{};
  idVec2 screenMin{};
  idVec2 screenMax{};
  float priorityScale{1.0F};
  int floorIndex{0};
  bool ignore{false};
};

struct idDeclAutomap {
  std::string name;
  std::vector<automapFloor_t> floors;
  std::vector<automapBlock_t> blocks;
};

struct automapObject_t {
  const automapBlock_t* Construct(const idDeclAutomap* decl,
                                  const idVec3& origin, const idMat3& axis,
                                  const std::string& iconName, int objectId,
                                  int playerFloorIndex);
  idVec2 position{};
  float rotation{0.0F};
  std::string icon;
  int id{0};
  bool operator==(const automapObject_t& other) const { return id == other.id; }
};

struct automapFogMap_t {
  std::vector<std::uint8_t> bits;
  int bitArrayWidth{0};
  int bitArrayHeight{0};
  bool operator==(const automapFogMap_t& other) const {
    return bits == other.bits && bitArrayWidth == other.bitArrayWidth &&
           bitArrayHeight == other.bitArrayHeight;
  }
};

struct automapSprite_t {
  idVec2 vertices[4]{};
  std::string material;
  std::uint8_t alpha{255};
};

class idAutomap {
public:
  idAutomap();
  ~idAutomap();
  void Activate();
  void DrawSprite(float x, float y, float width, float height,
                  const std::string& sprite, std::uint8_t alpha);
  void DrawRotatedSprite(float x, float y, float width, float height,
                         float angle, const std::string& sprite,
                         std::uint8_t alpha);
  void ForceLogUpdate(bool force);
  const automapFloor_t* GetCurrentFloorDecl() const;
  void ResetProgress();
  void Deactivate();
  std::vector<std::uint8_t> Save() const;
  bool Load(const std::vector<std::uint8_t>& data);
  void Render();
  bool CanActivate() const;
  void Update(const idVec3& playerOrigin, const idMat3& playerAxis);
  void Clear();
  void Initialize(const idDeclAutomap* declaration);

  void SetCurrentFloor(int value) { currentFloorIndex = value; }
  void SetUnlocked(bool value) { unlocked = value; }
  bool IsUnlocked() const { return unlocked; }
  bool IsActive() const { return active; }
  bool IsForceLogUpdate() const { return forceLogUpdate; }
  int NumFogMaps() const { return floorFogMaps.Num(); }
  int NumObjects() const { return objects.Num(); }
  int NumSprites() const { return static_cast<int>(sprites.size()); }
  int GetRenderCount() const { return renderCount; }
  const automapObject_t& GetObject(int index) const { return objects[index]; }
  const automapSprite_t& GetSprite(int index) const { return sprites.at(index); }
  automapFogMap_t& GetFogMap(int index) { return floorFogMaps[index]; }

private:
  const idDeclAutomap* decl{nullptr};
  idList<automapObject_t, 5> objects;
  idList<automapFogMap_t, 5> floorFogMaps;
  std::vector<automapSprite_t> sprites;
  int currentFloorIndex{0};
  int lastLogUpdate{-1};
  int renderCount{0};
  bool active{false};
  bool unlocked{false};
  bool forceLogUpdate{false};
};

struct idAutomapRenderParmResource { std::string name; };
extern idAutomapRenderParmResource rpAutomapFogMapSize;
extern idCVar g_automapAllowUpdateTime;
