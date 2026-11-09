#pragma once

#include "tungsten/game/clientgame/presentable/presentable.h"

#include <string>
#include <vector>

struct idAtomicString { std::string value; };
class idSerializer {
public:
  explicit idSerializer(bool writeMode):writing(writeMode){}
  void SerializeString(idAtomicString* value);
  void SerializeInt(int& value){if(writing)integers.push_back(value);else if(integerReadIndex<integers.size())value=integers[integerReadIndex++];}
  void SerializeBool(bool& value){if(writing)bits.push_back(value);else if(bitReadIndex<bits.size())value=bits[bitReadIndex++];}
  void SerializeFloat(float& value){if(writing)floats.push_back(value);else if(floatReadIndex<floats.size())value=floats[floatReadIndex++];}
  void SerializeVec3(idVec3& value){if(writing)vectors3.push_back(value);else if(vector3ReadIndex<vectors3.size())value=vectors3[vector3ReadIndex++];}
  void SerializeVec4(idVec4& value){if(writing)vectors4.push_back(value);else if(vector4ReadIndex<vectors4.size())value=vectors4[vector4ReadIndex++];}
  void SerializeMat3(idMat3& value){if(writing)matrices.push_back(value);else if(matrixReadIndex<matrices.size())value=matrices[matrixReadIndex++];}
  template<int Range,int Bits> void SerializeQ(float& value){(void)Range;(void)Bits;SerializeFloat(value);}
  bool writing{true}; std::vector<std::string> strings; std::vector<int> integers; std::vector<bool> bits; std::vector<float> floats; std::vector<idVec3> vectors3; std::vector<idVec4> vectors4; std::vector<idMat3> matrices; std::size_t readIndex{0}; std::size_t integerReadIndex{0}; std::size_t bitReadIndex{0}; std::size_t floatReadIndex{0}; std::size_t vector3ReadIndex{0}; std::size_t vector4ReadIndex{0}; std::size_t matrixReadIndex{0};
};

class idDummyPhysicsCallback { public: idDummyPhysicsCallback(); ~idDummyPhysicsCallback(); const char* GetName()const; private: std::string name; };

class idPresentableAnimatedPhysics : public idPresentable {
public:
  idPresentableAnimatedPhysics(int entityOwner,const std::string& clipName,bool obstacle);
  ~idPresentableAnimatedPhysics()override=default;
  int GetType()const; void Serialize(idSerializer* serializer);
  void LinkCollision(); void UnlinkCollision(); void Present();
  bool IsLinked()const{return linked;} const std::string& GetClip()const{return clip;} int GetOwnerIndex()const{return entityOwnerIndex;}
protected: std::string clip; int entityOwnerIndex{-1}; bool obstacle{false}; bool linked{false}; bool physicsInit{false};
};

class idFakeCBStopWhenCollide {
public:
  idFakeCBStopWhenCollide():name("fakeCB"){}
  ~idFakeCBStopWhenCollide()=default;
  const char* GetName()const;
private:
  std::string name;
};

class idPresentableDoorAnimated : public idPresentableAnimatedPhysics {
public:
  idPresentableDoorAnimated(int entityOwner,const std::string& clipName,bool obstacle);
  ~idPresentableDoorAnimated()override;
  void LinkCollision(); void UnlinkCollision(); void Present();
  int GetType()const;
  void InitPhysics(const std::string& clipName,bool isObstacle);
  void Serialize(idSerializer* serializer);
  bool IsPhysicsInitialized()const{return physicsInit;}
};

extern idDummyPhysicsCallback dummyCB;
extern idFakeCBStopWhenCollide fakeCB;
