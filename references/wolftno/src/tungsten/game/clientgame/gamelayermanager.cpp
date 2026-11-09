#include "tungsten/game/clientgame/gamelayermanager.h"

#include <algorithm>
#include <cctype>
#include <sstream>

// DIA: idSort<idLayer>::`scalar deleting destructor'
// DIA: idGameLayerManager::FindRetainedEntityState
// DIA: idGameLayerManager::ContainsItemMemory
// DIA: idGameLayerManager::WriteToFile
// DIA: idGameLayerManager::RemoveLayerFromTransientLists
// DIA: `idGameLayerManager::RemoveLayerFromTransientLists'::`1'::dtor$0
// DIA: `idGameLayerManager::RemoveLayerFromTransientLists'::`1'::dtor$1
// DIA: `idGameLayerManager::RemoveLayerFromTransientLists'::`1'::dtor$2
// DIA: `idGameLayerManager::RemoveLayerFromTransientLists'::`1'::dtor$3
// DIA: `idGameLayerManager::RemoveLayerFromTransientLists'::`1'::dtor$4
// DIA: `idGameLayerManager::RemoveLayerFromTransientLists'::`1'::dtor$5
// DIA: idSort_GameLayer::~idSort_GameLayer
// DIA: idList<idRetainedItemMemoryForMap,5>::~idList<idRetainedItemMemoryForMap,5>
// DIA: idList<idLayer,5>::~idList<idLayer,5>
// DIA: idList<idLayer,5>::RemoveIndex
// DIA: SwapValues<idLayer>
// DIA: `SwapValues<idLayer>'::`1'::dtor$1
// DIA: `SwapValues<idLayer>'::`1'::dtor$0
// DIA: `SwapValues<idLayer>'::`1'::dtor$3
// DIA: idGameLayerManager::Think
// DIA: idGameLayerManager::ClearLayers
// DIA: idList<idEntityRetainedState,5>::Resize
// DIA: idList<idLayer,5>::Resize
// DIA: idSort_Quick<idLayer,idSort_GameLayer>::Sort
// DIA: `idSort_Quick<idLayer,idSort_GameLayer>::Sort'::`1'::dtor$0
// DIA: `idSort_Quick<idLayer,idSort_GameLayer>::Sort'::`1'::dtor$1
// DIA: `idSort_Quick<idLayer,idSort_GameLayer>::Sort'::`1'::dtor$2
// DIA: `idSort_Quick<idLayer,idSort_GameLayer>::Sort'::`1'::dtor$3
// DIA: `idSort_Quick<idLayer,idSort_GameLayer>::Sort'::`1'::dtor$4
// DIA: `idSort_Quick<idLayer,idSort_GameLayer>::Sort'::`1'::dtor$5
// DIA: `idSort_Quick<idLayer,idSort_GameLayer>::Sort'::`1'::dtor$6
// DIA: `idSort_Quick<idLayer,idSort_GameLayer>::Sort'::`1'::dtor$7
// DIA: `idSort_Quick<idLayer,idSort_GameLayer>::Sort'::`1'::dtor$8
// DIA: `idSort_Quick<idLayer,idSort_GameLayer>::Sort'::`1'::dtor$9
// DIA: idGameLayerManager::ActivateDeactiveLayer
// DIA: idGameLayerManager::DeactivateActiveLayer
// DIA: idGameLayerManager::RemoveActiveLayer
// DIA: idGameLayerManager::RemoveActiveLayerFromMap
// DIA: idGameLayerManager::RemoveLayerMarkActiveNextMapLoad
// DIA: idGameLayerManager::RemoveLayerMarkDeactiveNextMapLoad
// DIA: idList<idEntityRetainedState,5>::Append
// DIA: idList<idLayer,5>::Append
// DIA: idGameLayerManager::idGameLayerManager
// DIA: `idGameLayerManager::idGameLayerManager'::`1'::dtor$0
// DIA: `idGameLayerManager::idGameLayerManager'::`1'::dtor$1
// DIA: `idGameLayerManager::idGameLayerManager'::`1'::dtor$2
// DIA: `idGameLayerManager::idGameLayerManager'::`1'::dtor$3
// DIA: `idGameLayerManager::idGameLayerManager'::`1'::dtor$4
// DIA: `idGameLayerManager::idGameLayerManager'::`1'::dtor$5
// DIA: `idGameLayerManager::idGameLayerManager'::`1'::dtor$6
// DIA: `idGameLayerManager::idGameLayerManager'::`1'::dtor$7
// DIA: idGameLayerManager::~idGameLayerManager
// DIA: `idGameLayerManager::~idGameLayerManager'::`1'::dtor$0
// DIA: `idGameLayerManager::~idGameLayerManager'::`1'::dtor$1
// DIA: `idGameLayerManager::~idGameLayerManager'::`1'::dtor$2
// DIA: `idGameLayerManager::~idGameLayerManager'::`1'::dtor$3
// DIA: `idGameLayerManager::~idGameLayerManager'::`1'::dtor$4
// DIA: `idGameLayerManager::~idGameLayerManager'::`1'::dtor$5
// DIA: `idGameLayerManager::~idGameLayerManager'::`1'::dtor$6
// DIA: `idGameLayerManager::~idGameLayerManager'::`1'::dtor$7
// DIA: `idGameLayerManager::~idGameLayerManager'::`1'::dtor$8
// DIA: idGameLayerManager::ActivateDeactiveLayers
// DIA: idGameLayerManager::DeactiveateActiveLayers
// DIA: idGameLayerManager::RemoveLayers
// DIA: idGameLayerManager::RemoveLayersMarkActiveNextMapLoad
// DIA: idGameLayerManager::RemoveLayersMarkDeactiveNextMapLoad
// DIA: idGameLayerManager::RemoveLayersFromGame
// DIA: idGameLayerManager::AddActiveLayer
// DIA: `idGameLayerManager::AddActiveLayer'::`1'::dtor$0
// DIA: `idGameLayerManager::AddActiveLayer'::`1'::dtor$5
// DIA: `idGameLayerManager::AddActiveLayer'::`1'::dtor$6
// DIA: `idGameLayerManager::AddActiveLayer'::`1'::dtor$2
// DIA: `idGameLayerManager::AddActiveLayer'::`1'::dtor$8
// DIA: `idGameLayerManager::AddActiveLayer'::`1'::dtor$3
// DIA: `idGameLayerManager::AddActiveLayer'::`1'::dtor$10
// DIA: `idGameLayerManager::AddActiveLayer'::`1'::dtor$12
// DIA: `idGameLayerManager::AddActiveLayer'::`1'::dtor$4
// DIA: idGameLayerManager::AddDeactiveLayer
// DIA: `idGameLayerManager::AddDeactiveLayer'::`1'::dtor$0
// DIA: `idGameLayerManager::AddDeactiveLayer'::`1'::dtor$4
// DIA: `idGameLayerManager::AddDeactiveLayer'::`1'::dtor$5
// DIA: `idGameLayerManager::AddDeactiveLayer'::`1'::dtor$2
// DIA: `idGameLayerManager::AddDeactiveLayer'::`1'::dtor$7
// DIA: `idGameLayerManager::AddDeactiveLayer'::`1'::dtor$3
// DIA: `idGameLayerManager::AddDeactiveLayer'::`1'::dtor$9
// DIA: `idGameLayerManager::AddDeactiveLayer'::`1'::dtor$11
// DIA: idGameLayerManager::GetLayerStrings
// DIA: `idGameLayerManager::GetLayerStrings'::`1'::dtor$9
// DIA: `idGameLayerManager::GetLayerStrings'::`1'::dtor$10
// DIA: `idGameLayerManager::GetLayerStrings'::`1'::dtor$1
// DIA: `idGameLayerManager::GetLayerStrings'::`1'::dtor$12
// DIA: `idGameLayerManager::GetLayerStrings'::`1'::dtor$2
// DIA: `idGameLayerManager::GetLayerStrings'::`1'::dtor$14
// DIA: `idGameLayerManager::GetLayerStrings'::`1'::dtor$16
// DIA: `idGameLayerManager::GetLayerStrings'::`1'::dtor$18
// DIA: `idGameLayerManager::GetLayerStrings'::`1'::dtor$19
// DIA: `idGameLayerManager::GetLayerStrings'::`1'::dtor$4
// DIA: `idGameLayerManager::GetLayerStrings'::`1'::dtor$21
// DIA: `idGameLayerManager::GetLayerStrings'::`1'::dtor$5
// DIA: `idGameLayerManager::GetLayerStrings'::`1'::dtor$23
// DIA: `idGameLayerManager::GetLayerStrings'::`1'::dtor$25
// DIA: `idGameLayerManager::GetLayerStrings'::`1'::dtor$6
// DIA: `idGameLayerManager::GetLayerStrings'::`1'::dtor$7
// DIA: `idGameLayerManager::GetLayerStrings'::`1'::dtor$8
// DIA: idGameLayerManager::Init
// DIA: idGameLayerManager::AddActiveLayers
// DIA: idGameLayerManager::AddDeactiveLayers
// DIA: idList<idRetainedItemMemoryForMap,5>::Resize
// DIA: idList<idRetainedItemMemoryForMap,5>::Append
// DIA: idGameLayerManager::ReadFromFile
// DIA: `idGameLayerManager::ReadFromFile'::`1'::dtor$0
// DIA: `idGameLayerManager::ReadFromFile'::`1'::dtor$5
// DIA: `idGameLayerManager::ReadFromFile'::`1'::dtor$6
// DIA: `idGameLayerManager::ReadFromFile'::`1'::dtor$1
// DIA: `idGameLayerManager::ReadFromFile'::`1'::dtor$2
// DIA: `idGameLayerManager::ReadFromFile'::`1'::dtor$8
// DIA: `idGameLayerManager::ReadFromFile'::`1'::dtor$9
// DIA: `idGameLayerManager::ReadFromFile'::`1'::dtor$10
// DIA: `idGameLayerManager::ReadFromFile'::`1'::dtor$11
// DIA: `idGameLayerManager::ReadFromFile'::`1'::dtor$12
// DIA: `idGameLayerManager::ReadFromFile'::`1'::dtor$13
// DIA: idGameLayerManager::AddItemMemory
// DIA: `idGameLayerManager::AddItemMemory'::`1'::dtor$0
// DIA: `idGameLayerManager::AddItemMemory'::`1'::dtor$1
// DIA: `idGameLayerManager::AddItemMemory'::`1'::dtor$3
// DIA: `idGameLayerManager::AddItemMemory'::`1'::dtor$4
// DIA: `idGameLayerManager::AddItemMemory'::`1'::dtor$2
// DIA: `idGameLayerManager::AddItemMemory'::`1'::dtor$6
// DIA: `idGameLayerManager::AddItemMemory'::`1'::dtor$7

idSort_GameLayer::~idSort_GameLayer()=default;
idGameLayerManager::idGameLayerManager()=default;
idGameLayerManager::~idGameLayerManager()=default;
std::string idGameLayerManager::Normalize(std::string v){std::transform(v.begin(),v.end(),v.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});return v;}
bool idGameLayerManager::AddUnique(std::vector<std::string>&list,const std::string&value){if(std::find(list.begin(),list.end(),value)!=list.end())return false;list.push_back(value);return true;}
idLayer* idGameLayerManager::FindLayer(const std::string&name){const auto key=Normalize(name);for(int i=0;i<playerLayers.Num();++i)if(playerLayers[i].layerDeclName==key)return &playerLayers[i];return nullptr;}
const idLayer* idGameLayerManager::FindLayer(const std::string&name)const{const auto key=Normalize(name);for(int i=0;i<playerLayers.Num();++i)if(playerLayers[i].layerDeclName==key)return &playerLayers[i];return nullptr;}
idEntityRetainedState* idGameLayerManager::FindRetainedEntityState(const std::string&name){for(int i=0;i<entityRetainedStates.Num();++i)if(entityRetainedStates[i].entityName==name)return &entityRetainedStates[i];return nullptr;}
bool idGameLayerManager::ContainsItemMemory(const std::string&item)const{for(int i=0;i<retainedItemMemoryForMap.Num();++i)if(retainedItemMemoryForMap[i].mapName==currentMap)for(int j=0;j<retainedItemMemoryForMap[i].itemsList.Num();++j)if(retainedItemMemoryForMap[i].itemsList[j]==item)return true;return false;}
std::string idGameLayerManager::WriteToFile()const{std::ostringstream out;out<<currentMap<<'\n'<<playerLayers.Num()<<'\n';for(int i=0;i<playerLayers.Num();++i)out<<playerLayers[i].layerDeclName<<'\t'<<static_cast<int>(playerLayers[i].layerState)<<'\t'<<playerLayers[i].activeNextMap<<'\t'<<playerLayers[i].deactiveNextMap<<'\n';out<<retainedItemMemoryForMap.Num()<<'\n';for(int i=0;i<retainedItemMemoryForMap.Num();++i){const auto&m=retainedItemMemoryForMap[i];out<<m.mapName<<'\t'<<m.itemsList.Num();for(int j=0;j<m.itemsList.Num();++j)out<<'\t'<<m.itemsList[j];out<<'\n';}return out.str();}
bool idGameLayerManager::ReadFromFile(const std::string&data){std::istringstream in(data);ClearLayers();int layers=0;if(!std::getline(in,currentMap)||!(in>>layers))return false;std::string line;std::getline(in,line);for(int i=0;i<layers;++i){idLayer l;int state=0;if(!(in>>l.layerDeclName>>state>>l.activeNextMap>>l.deactiveNextMap))return false;l.layerState=state?LAYERSTATE_ACTIVE:LAYERSTATE_DEACTIVE;playerLayers.Append(l);}int maps=0;if(!(in>>maps))return false;std::getline(in,line);for(int i=0;i<maps;++i){if(!std::getline(in,line))return false;std::istringstream row(line);idRetainedItemMemoryForMap m;int count=0;row>>m.mapName>>count;for(int j=0;j<count;++j){std::string item;row>>item;m.itemsList.Append(item);}retainedItemMemoryForMap.Append(m);}return true;}
void idGameLayerManager::RemoveLayerFromTransientLists(const std::string&name){const auto key=Normalize(name);auto erase=[&](auto&v){v.erase(std::remove(v.begin(),v.end(),key),v.end());};erase(layers_DeactiveToActive);erase(layers_ActiveToDeactivate);erase(layers_ToRemove);erase(layers_ToRemoveFromGameLocalLists);}
void idGameLayerManager::Think(){processedTransitions+=static_cast<int>(layers_DeactiveToActive.size()+layers_ActiveToDeactivate.size()+layers_ToRemove.size()+layers_ToRemoveFromGameLocalLists.size());for(const auto&n:layers_ToRemove){if(auto*l=FindLayer(n)){for(int i=0;i<playerLayers.Num();++i)if(&playerLayers[i]==l){playerLayers.RemoveIndex(i);break;}}}for(const auto&n:layers_ToRemoveFromGameLocalLists){for(int i=0;i<playerLayers.Num();++i)if(playerLayers[i].layerDeclName==n){playerLayers.RemoveIndex(i);break;}}layers_DeactiveToActive.clear();layers_ActiveToDeactivate.clear();layers_ToRemove.clear();layers_ToRemoveFromGameLocalLists.clear();}
void idGameLayerManager::ClearLayers(){playerLayers.Clear();layers_DeactiveToActive.clear();layers_ActiveToDeactivate.clear();layers_ToRemove.clear();layers_ToRemoveFromGameLocalLists.clear();}
bool idGameLayerManager::ActivateDeactiveLayer(const std::string&name){auto*l=FindLayer(name);if(!l||l->layerState==LAYERSTATE_ACTIVE)return false;l->layerState=LAYERSTATE_ACTIVE;RemoveLayerFromTransientLists(name);return AddUnique(layers_DeactiveToActive,l->layerDeclName);}
bool idGameLayerManager::DeactivateActiveLayer(const std::string&name){auto*l=FindLayer(name);if(!l||l->layerState==LAYERSTATE_DEACTIVE)return false;l->layerState=LAYERSTATE_DEACTIVE;RemoveLayerFromTransientLists(name);return AddUnique(layers_ActiveToDeactivate,l->layerDeclName);}
bool idGameLayerManager::RemoveActiveLayer(const std::string&name){auto*l=FindLayer(name);if(!l)return false;RemoveLayerFromTransientLists(name);return AddUnique(layers_ToRemove,l->layerDeclName);}
bool idGameLayerManager::RemoveActiveLayerFromMap(const std::string&name){auto*l=FindLayer(name);if(!l)return false;RemoveLayerFromTransientLists(name);return AddUnique(layers_ToRemoveFromGameLocalLists,l->layerDeclName);}
bool idGameLayerManager::RemoveLayerMarkActiveNextMapLoad(const std::string&name){auto*l=FindLayer(name);if(!l)return false;l->activeNextMap=true;l->deactiveNextMap=false;return RemoveActiveLayer(name);}
bool idGameLayerManager::RemoveLayerMarkDeactiveNextMapLoad(const std::string&name){auto*l=FindLayer(name);if(!l)return false;l->deactiveNextMap=true;l->activeNextMap=false;return RemoveActiveLayer(name);}
#define WOLF_LAYER_BATCH(name,single) void idGameLayerManager::name(const std::vector<std::string>&layers){for(const auto&layer:layers)single(layer);}
WOLF_LAYER_BATCH(ActivateDeactiveLayers,ActivateDeactiveLayer)
WOLF_LAYER_BATCH(DeactiveateActiveLayers,DeactivateActiveLayer)
WOLF_LAYER_BATCH(RemoveLayers,RemoveActiveLayer)
WOLF_LAYER_BATCH(RemoveLayersMarkActiveNextMapLoad,RemoveLayerMarkActiveNextMapLoad)
WOLF_LAYER_BATCH(RemoveLayersMarkDeactiveNextMapLoad,RemoveLayerMarkDeactiveNextMapLoad)
WOLF_LAYER_BATCH(RemoveLayersFromGame,RemoveActiveLayerFromMap)
#undef WOLF_LAYER_BATCH
void idGameLayerManager::AddActiveLayer(const std::string&name){const auto key=Normalize(name);if(auto*l=FindLayer(key)){l->layerState=LAYERSTATE_ACTIVE;return;}playerLayers.Append({key,LAYERSTATE_ACTIVE});}
void idGameLayerManager::AddDeactiveLayer(const std::string&name){const auto key=Normalize(name);if(auto*l=FindLayer(key)){l->layerState=LAYERSTATE_DEACTIVE;return;}playerLayers.Append({key,LAYERSTATE_DEACTIVE});}
void idGameLayerManager::GetLayerStrings(std::string&active,std::string&deactive,bool propagate)const{active.clear();deactive.clear();idList<idLayer,5> sorted=playerLayers;idSort_Quick<idLayer,idSort_GameLayer>{}.Sort(sorted);for(int i=0;i<sorted.Num();++i){const auto&l=sorted[i];const bool isActive=propagate?(l.activeNextMap||(!l.deactiveNextMap&&l.layerState==LAYERSTATE_ACTIVE)):l.layerState==LAYERSTATE_ACTIVE;auto&dest=isActive?active:deactive;if(!dest.empty())dest+=",";dest+=l.layerDeclName;}}
void idGameLayerManager::Init(idPlayer*player,bool firstLoad){owner=player;if(firstLoad)Think();}
void idGameLayerManager::AddActiveLayers(const std::vector<std::string>&layers){for(const auto&l:layers)AddActiveLayer(l);}
void idGameLayerManager::AddDeactiveLayers(const std::vector<std::string>&layers){for(const auto&l:layers)AddDeactiveLayer(l);}
void idGameLayerManager::AddItemMemory(const std::string&item){if(ContainsItemMemory(item))return;idRetainedItemMemoryForMap*map=nullptr;for(int i=0;i<retainedItemMemoryForMap.Num();++i)if(retainedItemMemoryForMap[i].mapName==currentMap)map=&retainedItemMemoryForMap[i];if(!map){map=retainedItemMemoryForMap.Alloc();map->mapName=currentMap;}map->itemsList.Append(item);}
void idGameLayerManager::AddRetainedEntityState(std::string name,std::vector<std::uint8_t>state){if(auto*found=FindRetainedEntityState(name)){found->state=std::move(state);return;}entityRetainedStates.Append({std::move(name),std::move(state)});}

template class idSort<idLayer>;
template void SwapValues<idLayer>(idLayer&,idLayer&);
template class idSort_Quick<idLayer,idSort_GameLayer>;
template class idList<idRetainedItemMemoryForMap,5>;
template class idList<idLayer,5>;
template bool idList<idEntityRetainedState,5>::Resize(int);
template int idList<idEntityRetainedState,5>::Append(const idEntityRetainedState&);


