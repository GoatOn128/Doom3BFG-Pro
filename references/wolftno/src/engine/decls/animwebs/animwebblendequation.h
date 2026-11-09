#pragma once

#include "declanimweb.h"
#include "shared/idlib/containers/staticlist.h"

class idAnimWebBlendEquation {
public:
  struct animList_t {
    animList_t();
    ~animList_t();
    idStaticList<idMD6Filter*,10> filterList;
    idStaticList<idMD6Node*,256> animList;
    idStaticList<unsigned,256> tagList;
    int animPropIndex;
  };
  struct existingNodeName_t {
    char str[8];
    operator char*() { return str; }
    operator const char*() const { return str; }
  };
  struct existingNode_t {
    existingNodeName_t name;
    idMD6Node* node = nullptr;
  };
  using scalarList_t = idList<idAnimWebScalar,33>;
  using scalarPairList_t = idList<idAnimWebScalarPair,33>;
  using existingNodeList_t = idList<existingNode_t,5>;

  idAnimWebBlendEquation();
  static bool ParseScalarName(idLexer& source,idToken& token);
  static void AllocateUndeclaredScalars(scalarList_t& scalars,idScalarMemBlock& block);
  void ParseImmediate(idLexer& source,float& value);
  unsigned ParseWeightGroup(idLexer& source);
  void AddScalar(const char* name,scalarList_t& scalars,scalarPairList_t& pairs,int& scalarIndex,int& pairIndex);
  idMD6Node* AddLeafPlayFromDefaultLeaf(idMD6Allocator& allocator,const existingNodeList_t& existingNodes,float animIndex,int start,int ticks,int startFrame,float rate,int wrapMode,md6WeightGroup_t weightGroup);
  idMD6Node* AddLeafPauseFromDefaultLeaf(idMD6Allocator& allocator,const existingNodeList_t& existingNodes,float animIndex,float frame,int wrapMode,md6WeightGroup_t weightGroup);
  const idDeclAnimWebTagGroup* ProcessTagGroup(bool copyOnPlay,const char* groupName,const idDeclAnimWebNode::idModelInfo& modelInfo,idMD6Node& node,scalarList_t& scalars,scalarPairList_t& pairs,unsigned& tagBias,unsigned& tagCount,unsigned& defaultTag,unsigned& groupIndex,unsigned& groupMask);
  void ParseScalar(idLexer& source,scalarList_t& scalars,scalarPairList_t& pairs,int& scalarIndex,int& pairIndex,float& value,bool& copyOnPlay);
  void ParseNode(idLexer& source,const char* name,const idMD6Model* model,idMD6Allocator& allocator,scalarList_t& scalars,existingNodeList_t& existingNodes,idMD6Node*& root,scalarPairList_t& pairs,animList_t* animList);
  void ParseAnimList(idLexer& source,const idDeclAnimWebNode::idModelInfo& modelInfo,const idMD6Model* model,idMD6Allocator& allocator,existingNodeList_t& existingNodes,const idDeclAnimWebTagGroup* tagGroup,animList_t& result);
  void ParseBlend(bool useUserChannels,int dimensions,idLexer& source,const idDeclAnimWebNode::idModelInfo& modelInfo,const idMD6Model* model,idMD6Allocator* allocator,existingNodeList_t& existingNodes,scalarList_t& scalars,scalarPairList_t& pairs,idList<idUserChannelIndex>& userChannelIndices,idList<float>& coordinateList,idList<idMD6Node*>& animationList,idList<float>& currentCoordinate,idList<int>& currentCoordinateScalarIndex,idList<int>& currentCoordinateScalarPairIndex,idList<bool>& isCopyOnPlay,md6WeightGroup_t& weightGroup,idMD6Node** baseAnimation);
  void Parse_r(idLexer& source,const idDeclAnimWebNode::idModelInfo& modelInfo,const idMD6Model* model,idMD6Allocator& allocator,scalarList_t& scalars,existingNodeList_t& existingNodes,idMD6Node*& root,scalarPairList_t& pairs,animList_t* animList=nullptr);
  bool Parse(const idDeclAnimWebNode::idModelInfo& modelInfo,const idMD6Model* model,idMD6Allocator* allocator,int time,int ticksPerSecond,const char* equation,scalarList_t& scalars,existingNodeList_t& existingNodes,idMD6Node*& root,scalarPairList_t& pairs);

  int startTime;
  int ticksPerSec;
  idStaticList<idMD6Node*,64> allocatedNodes;
  int expressionLevel;
};
static_assert(sizeof(idAnimWebBlendEquation::existingNode_t)==16,"AnimWeb existing-node ABI drift");
static_assert(sizeof(idAnimWebBlendEquation::animList_t)==3232,"AnimWeb animation-list ABI drift");
static_assert(sizeof(idAnimWebBlendEquation)==552,"AnimWeb equation ABI drift");
