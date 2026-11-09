#pragma once
#include "tungsten/game/game_runtime.h"
#include <memory>
#include <string>
struct idDeclCloth{std::string name;float detachThreshold{100};};
struct idClothSim{ idVec3 origin{};idMat3 axis{};idVec3 windDirection{};float stress{0};int updates{0};bool detached{false}; };
class idClothComponent{public:explicit idClothComponent(const idDeclCloth*decl=nullptr):clothDecl(decl){}~idClothComponent();bool Init(idEntity*animated);void Update(float deltaSeconds=1.0F/60.0F);void AddStress(float value){if(clothSim)clothSim->stress+=value;}bool IsInitialized()const{return clothSim!=nullptr;}bool IsDetached()const{return isDetached;}const idClothSim* GetSimulation()const{return clothSim.get();}private:const idDeclCloth*clothDecl{nullptr};idEntity*animatedEntity{nullptr};std::unique_ptr<idClothSim>clothSim;bool isDetached{false};};
extern idCVar g_debugClothComponent;extern idCVar g_TestClothComponentWind;
