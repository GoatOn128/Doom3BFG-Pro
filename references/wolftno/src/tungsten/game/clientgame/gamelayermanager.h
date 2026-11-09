#pragma once

#include "tungsten/game/clientgame/presentable/presentableplayer_shared.h"

#include <string>
#include <utility>
#include <vector>

enum layerState_t { LAYERSTATE_DEACTIVE=0,LAYERSTATE_ACTIVE=1 };
struct idLayer {
  std::string layerDeclName;
  layerState_t layerState{LAYERSTATE_DEACTIVE};
  bool activeNextMap{false};
  bool deactiveNextMap{false};
  bool operator==(const idLayer&o)const{return layerDeclName==o.layerDeclName;}
  bool operator<(const idLayer&o)const{return layerDeclName<o.layerDeclName;}
};
struct idEntityRetainedState { std::string entityName;std::vector<std::uint8_t> state;bool operator==(const idEntityRetainedState&o)const{return entityName==o.entityName;} };
struct idRetainedItemMemoryForMap { std::string mapName;idList<std::string,5> itemsList;bool operator==(const idRetainedItemMemoryForMap&o)const{return mapName==o.mapName;} };

template<typename T>class idSort{public:virtual ~idSort()=default;virtual bool Less(const T&a,const T&b)const{return a<b;}};
class idSort_GameLayer:public idSort<idLayer>{public:~idSort_GameLayer()override;bool Less(const idLayer&a,const idLayer&b)const override{return a.layerDeclName<b.layerDeclName;}};
template<typename T>void SwapValues(T&a,T&b){T temp=std::move(a);a=std::move(b);b=std::move(temp);}
template<typename T,typename Sorter>class idSort_Quick{public:void Sort(idList<T,5>&list,const Sorter&sorter=Sorter{}){for(int i=0;i<list.Num();++i)for(int j=i+1;j<list.Num();++j)if(sorter.Less(list[j],list[i]))SwapValues(list[i],list[j]);}};

class idGameLayerManager {
public:
  idGameLayerManager();~idGameLayerManager();
  idEntityRetainedState* FindRetainedEntityState(const std::string& entityName);
  bool ContainsItemMemory(const std::string& itemName)const;
  std::string WriteToFile()const;
  bool ReadFromFile(const std::string& data);
  void RemoveLayerFromTransientLists(const std::string& layerName);
  void Think();void ClearLayers();
  bool ActivateDeactiveLayer(const std::string& layerName);
  bool DeactivateActiveLayer(const std::string& layerName);
  bool RemoveActiveLayer(const std::string& layerName);
  bool RemoveActiveLayerFromMap(const std::string& layerName);
  bool RemoveLayerMarkActiveNextMapLoad(const std::string& layerName);
  bool RemoveLayerMarkDeactiveNextMapLoad(const std::string& layerName);
  void ActivateDeactiveLayers(const std::vector<std::string>& layers);
  void DeactiveateActiveLayers(const std::vector<std::string>& layers);
  void RemoveLayers(const std::vector<std::string>& layers);
  void RemoveLayersMarkActiveNextMapLoad(const std::vector<std::string>& layers);
  void RemoveLayersMarkDeactiveNextMapLoad(const std::vector<std::string>& layers);
  void RemoveLayersFromGame(const std::vector<std::string>& layers);
  void AddActiveLayer(const std::string& layerName);
  void AddDeactiveLayer(const std::string& layerName);
  void GetLayerStrings(std::string& active,std::string& deactive,bool propagateNextMap)const;
  void Init(idPlayer* player,bool firstLoad);
  void AddActiveLayers(const std::vector<std::string>& layers);
  void AddDeactiveLayers(const std::vector<std::string>& layers);
  void AddItemMemory(const std::string& itemName);
  void SetCurrentMap(std::string map){currentMap=std::move(map);}
  void AddRetainedEntityState(std::string name,std::vector<std::uint8_t> state);
  const idList<idLayer,5>& GetLayers()const{return playerLayers;}
  int GetProcessedTransitionCount()const{return processedTransitions;}
private:
  idLayer* FindLayer(const std::string& name);const idLayer* FindLayer(const std::string& name)const;
  static std::string Normalize(std::string value);
  static bool AddUnique(std::vector<std::string>& list,const std::string& value);
  idPlayer* owner{nullptr};std::string currentMap{"default"};idList<idLayer,5> playerLayers;
  idList<idEntityRetainedState,5> entityRetainedStates;idList<idRetainedItemMemoryForMap,5> retainedItemMemoryForMap;
  std::vector<std::string> layers_DeactiveToActive,layers_ActiveToDeactivate,layers_ToRemove,layers_ToRemoveFromGameLocalLists;
  int processedTransitions{0};
};
