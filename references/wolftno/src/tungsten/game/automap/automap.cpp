#include "tungsten/game/automap/automap.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace {
std::uint32_t NameHash(const std::string& value) {
  std::uint32_t hash = 2166136261U;
  for (unsigned char character : value) hash = (hash ^ character) * 16777619U;
  return hash;
}
template <typename T>
void AppendBytes(std::vector<std::uint8_t>& output, const T& value) {
  const auto* bytes = reinterpret_cast<const std::uint8_t*>(&value);
  output.insert(output.end(), bytes, bytes + sizeof(T));
}
template <typename T>
bool ReadBytes(const std::vector<std::uint8_t>& input, std::size_t& offset,
               T& value) {
  if (offset + sizeof(T) > input.size()) return false;
  std::memcpy(&value, input.data() + offset, sizeof(T));
  offset += sizeof(T);
  return true;
}
}

// DIA: idPlayer::GetPlayerHud
void* idPlayer::GetPlayerHud() { return playerHud; }

// DIA: automapObject_t::Construct
const automapBlock_t* automapObject_t::Construct(
    const idDeclAutomap* decl, const idVec3& origin, const idMat3& axis,
    const std::string& iconName, int objectId, int playerFloorIndex) {
  id = objectId;
  const automapBlock_t* selected = nullptr;
  float smallestVolume = std::numeric_limits<float>::max();
  if (decl != nullptr) {
    for (const automapBlock_t& block : decl->blocks) {
      const bool contains = origin.x >= block.worldMin.x && origin.x <= block.worldMax.x &&
                            origin.y >= block.worldMin.y && origin.y <= block.worldMax.y &&
                            origin.z >= block.worldMin.z && origin.z <= block.worldMax.z;
      if (!contains || (playerFloorIndex >= 0 && block.floorIndex != playerFloorIndex)) continue;
      const idVec3 extent = block.worldMax - block.worldMin;
      const float volume = std::max(0.0F, extent.x * extent.y * extent.z) * block.priorityScale;
      if (volume < smallestVolume) { smallestVolume = volume; selected = &block; }
    }
  }
  if (selected == nullptr || selected->ignore || decl == nullptr ||
      selected->floorIndex < 0 || selected->floorIndex >= static_cast<int>(decl->floors.size())) {
    position = {}; rotation = 0.0F; icon.clear(); return nullptr;
  }
  const float fx = (origin.x - selected->worldMin.x) /
      std::max(0.0001F, selected->worldMax.x - selected->worldMin.x);
  const float fy = (origin.y - selected->worldMin.y) /
      std::max(0.0001F, selected->worldMax.y - selected->worldMin.y);
  const float scale = decl->floors[static_cast<std::size_t>(selected->floorIndex)].coordinateScale;
  const idVec2 raw{selected->screenMin.x + fx * (selected->screenMax.x - selected->screenMin.x),
                   selected->screenMin.y + (1.0F - fy) * (selected->screenMax.y - selected->screenMin.y)};
  position = {640.0F + (raw.x - 640.0F) * scale,
              360.0F + (raw.y - 360.0F) * scale};
  idVec3 forward = axis[0];
  rotation = std::atan2(forward.x, forward.y);
  icon = iconName;
  return selected;
}

// DIA: idAutomap::Activate
void idAutomap::Activate() { if (CanActivate()) active = true; }
// DIA: idAutomap::DrawSprite
void idAutomap::DrawSprite(float x, float y, float width, float height,
                           const std::string& sprite, std::uint8_t alpha) {
  automapSprite_t item; item.vertices[0] = {x,y}; item.vertices[1] = {x+width,y};
  item.vertices[2] = {x+width,y+height}; item.vertices[3] = {x,y+height};
  item.material = sprite; item.alpha = alpha; sprites.push_back(std::move(item));
}
// DIA: idAutomap::DrawRotatedSprite
void idAutomap::DrawRotatedSprite(float x, float y, float width, float height,
                                  float angle, const std::string& sprite,
                                  std::uint8_t alpha) {
  DrawSprite(x, y, width, height, sprite, alpha);
  automapSprite_t& item = sprites.back(); const idVec2 center{x+width*0.5F,y+height*0.5F};
  const float sine=std::sin(angle), cosine=std::cos(angle);
  for (idVec2& vertex : item.vertices) { const idVec2 delta=vertex-center;
    vertex={center.x+delta.x*cosine-delta.y*sine,center.y+delta.x*sine+delta.y*cosine}; }
}
// DIA: idAutomap::ForceLogUpdate
void idAutomap::ForceLogUpdate(bool force) { forceLogUpdate = force; }
// DIA: idAutomap::GetCurrentFloorDecl
const automapFloor_t* idAutomap::GetCurrentFloorDecl() const { return decl != nullptr && currentFloorIndex >= 0 && currentFloorIndex < static_cast<int>(decl->floors.size()) ? &decl->floors[static_cast<std::size_t>(currentFloorIndex)] : nullptr; }
// DIA: idAutomap::ResetProgress
void idAutomap::ResetProgress() { for (int i=0;i<floorFogMaps.Num();++i) std::fill(floorFogMaps[i].bits.begin(),floorFogMaps[i].bits.end(),std::uint8_t{0}); unlocked=false; }
// DIA: idAutomap::Deactivate
void idAutomap::Deactivate() { active=false; sprites.clear(); }
// DIA: idAutomap::Save
std::vector<std::uint8_t> idAutomap::Save() const { std::vector<std::uint8_t> out; const std::uint32_t hash=decl?NameHash(decl->name):0; AppendBytes(out,hash); const std::uint32_t count=static_cast<std::uint32_t>(floorFogMaps.Num()); AppendBytes(out,count); for(int i=0;i<floorFogMaps.Num();++i){const std::uint64_t size=floorFogMaps[i].bits.size();AppendBytes(out,size);out.insert(out.end(),floorFogMaps[i].bits.begin(),floorFogMaps[i].bits.end());} return out; }
// DIA: idAutomap::Load
bool idAutomap::Load(const std::vector<std::uint8_t>& data) { std::size_t offset=0;std::uint32_t hash=0,count=0;if(!ReadBytes(data,offset,hash)||!ReadBytes(data,offset,count))return false;if(decl==nullptr||hash!=NameHash(decl->name)){ResetProgress();return false;} for(std::uint32_t i=0;i<count;++i){std::uint64_t size=0;if(!ReadBytes(data,offset,size)||offset+size>data.size())return false;if(i<static_cast<std::uint32_t>(floorFogMaps.Num()))floorFogMaps[static_cast<int>(i)].bits.assign(data.begin()+static_cast<std::ptrdiff_t>(offset),data.begin()+static_cast<std::ptrdiff_t>(offset+size));offset+=static_cast<std::size_t>(size);}return true; }
// DIA: idAutomap::Render
void idAutomap::Render() { if(active){++renderCount;if(forceLogUpdate){lastLogUpdate=g_automapAllowUpdateTime.GetInteger();forceLogUpdate=false;}} }
// DIA: idList<automapObject_t,5>::Resize
// DIA: idAutomap::CanActivate
bool idAutomap::CanActivate() const { return decl != nullptr && floorFogMaps.Num() > 0; }
// DIA: idList<automapObject_t,5>::Append
// DIA: idAutomap::Update
void idAutomap::Update(const idVec3& playerOrigin, const idMat3& playerAxis) { if(!active||decl==nullptr)return;objects.Clear();automapObject_t player;if(player.Construct(decl,playerOrigin,playerAxis,"player",1,currentFloorIndex)!=nullptr)objects.Append(player); }
// DIA: idList<automapFogMap_t,5>::Resize
// DIA: idAutomap::Clear
void idAutomap::Clear() { decl=nullptr;objects.ClearFree();floorFogMaps.ClearFree();sprites.clear();currentFloorIndex=0;lastLogUpdate=-1;active=false;unlocked=false;forceLogUpdate=false; }
// DIA: idAutomap::idAutomap
idAutomap::idAutomap() = default;
// DIA: `idAutomap::idAutomap'::`1'::dtor$0
// DIA: `idAutomap::idAutomap'::`1'::dtor$1
// DIA: idAutomap::~idAutomap
idAutomap::~idAutomap() = default;
// DIA: idAutomap::Initialize
void idAutomap::Initialize(const idDeclAutomap* declaration) { Clear();decl=declaration;if(decl==nullptr)return;floorFogMaps.SetNum(static_cast<int>(decl->floors.size()));for(std::size_t i=0;i<decl->floors.size();++i){const int grid=std::max(1,decl->floors[i].fogGridSize);auto& fog=floorFogMaps[static_cast<int>(i)];fog.bitArrayWidth=1280/grid;fog.bitArrayHeight=720/grid;fog.bits.assign(static_cast<std::size_t>((fog.bitArrayWidth*fog.bitArrayHeight+7)/8),0);} }
// DIA: `dynamic initializer for 'rpAutomapFogMapSize''
idAutomapRenderParmResource rpAutomapFogMapSize{"rpAutomapFogMapSize"};
// DIA: `dynamic initializer for 'g_automapAllowUpdateTime''
// DIA: `dynamic atexit destructor for 'g_automapAllowUpdateTime''
idCVar g_automapAllowUpdateTime("g_automapAllowUpdateTime","800",2,"minimum automap update-log interval");

template class idList<automapObject_t, 5>;
template class idList<automapFogMap_t, 5>;
