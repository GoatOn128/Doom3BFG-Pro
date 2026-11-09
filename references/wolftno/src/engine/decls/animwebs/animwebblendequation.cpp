#include "animwebblendequation.h"
#include "engine/animation/animationpreload.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>

int idDeclAnimWebNode::idModelInfo::NumTagGroups() const { return tagGroups?tagGroups->Num():0; }

namespace {
const idMD6Anim* LeafAnim(const idMD6Node*node){if(!node||(node->type!=idMD6Node::NODE_LEAF_PLAY&&node->type!=idMD6Node::NODE_LEAF_PAUSE))return nullptr;return static_cast<const idMD6Leaf*>(node)->anim;}
int FindScalar(const idAnimWebBlendEquation::scalarList_t&scalars,const char*name){for(int i=0;i<scalars.Num();++i)if(idStr::Icmp(scalars[i].name.str,name)==0)return i;return-1;}
int FindExisting(const idAnimWebBlendEquation::existingNodeList_t&nodes,const char*name){for(int i=0;i<nodes.Num();++i)if(idStr::Icmp(nodes[i].name.str,name)==0)return i;return-1;}
idMD6Node* AllocateNode(idMD6Allocator&allocator,idMD6Node::nodeType_t type,idStaticList<idMD6Node*,64>&allocated){idMD6Node*node=allocator.Alloc(type);if(node){node->type=static_cast<std::uint8_t>(type);allocated.Append(node);}return node;}
}

idAnimWebBlendEquation::animList_t::animList_t():animPropIndex(-1){}
idAnimWebBlendEquation::animList_t::~animList_t()=default;
idAnimWebBlendEquation::idAnimWebBlendEquation():startTime(0),ticksPerSec(0),expressionLevel(0){}

bool idAnimWebBlendEquation::ParseScalarName(idLexer&source,idToken&token){if(token=="#"||token=="onplay")return source.ExpectTokenType(TT_NAME,0,token);if(token.type!=TT_NAME){source.Error("Expected a scalar variable name or a number, got '%s'",token.data);return false;}return true;}
void idAnimWebBlendEquation::AllocateUndeclaredScalars(scalarList_t&scalars,idScalarMemBlock&block){block.Free(scalars);int count=0;for(int i=0;i<scalars.Num();++i)if(scalars[i].scalarIndex<0&&!scalars[i].scalarPtr)++count;if(!count)return;block.Alloc(count);int index=0;for(int i=0;i<scalars.Num();++i)if(scalars[i].scalarIndex<0&&!scalars[i].scalarPtr){scalars[i].SetIndex(&block,static_cast<std::int16_t>(index++));scalars[i].SetScalar(&block,(scalars[i].flags&idAnimWebScalar::DECLARED)?1.0f:0.0f);}}
void idAnimWebBlendEquation::ParseImmediate(idLexer&source,float&value){idToken token;if(!source.PeekTokenType(TT_NUMBER,0,token)){source.Error("Expected constant (non-variable) value");value=0.0f;return;}value=source.ParseFloat();}
unsigned idAnimWebBlendEquation::ParseWeightGroup(idLexer&source){if(!source.CheckTokenString("filtered")&&!source.CheckTokenString("masked")&&!source.CheckTokenString("weighted"))return 0;source.CheckTokenString("by");idToken token;if(source.PeekTokenType(TT_NUMBER,TT_INTEGER,token)){unsigned value=static_cast<unsigned>(source.ParseInt());if(value>=8)source.Error("Weight group index %u is out of range",value);return std::min(value,7u);}source.ExpectAnyToken(token);static const char*const names[]={"all","legs","torso","head","face","mouth","eyelids","user0"};for(unsigned i=0;i<8;++i)if(idStr::Icmp(names[i],token.data)==0)return i;source.Error("Unknown weight group name '%s'",token.data);return 0;}
void idAnimWebBlendEquation::AddScalar(const char*name,scalarList_t&scalars,scalarPairList_t&pairs,int&scalarIndex,int&pairIndex){scalarIndex=FindScalar(scalars,name);if(scalarIndex<0){idAnimWebScalar scalar;scalar.Init(name,0);scalarIndex=scalars.Append(scalar);}pairIndex=pairs.Num();idAnimWebScalarPair pair;pair.scalarIndex=scalarIndex;pairs.Append(pair);}

idMD6Node*idAnimWebBlendEquation::AddLeafPlayFromDefaultLeaf(idMD6Allocator&allocator,const existingNodeList_t&existing,float animIndex,int start,int ticks,int startFrame,float rate,int wrapMode,md6WeightGroup_t weightGroup){idMD6LeafPlay*leaf=static_cast<idMD6LeafPlay*>(AllocateNode(allocator,idMD6Node::NODE_LEAF_PLAY,allocatedNodes));if(!leaf)return nullptr;const int index=static_cast<int>(animIndex);leaf->anim=index>=0&&index<existing.Num()?LeafAnim(existing[index].node):nullptr;leaf->rateScale=rate;leaf->wrapMode=static_cast<std::uint8_t>(wrapMode);leaf->weightGroup=static_cast<std::uint8_t>(weightGroup);leaf->initCounter=1;const int frameRate=leaf->anim&&leaf->anim->animData?leaf->anim->animData->frameRate:30;leaf->startTime=frameRate>0&&rate!=0.0f?static_cast<int>(static_cast<float>(start)-static_cast<float>(ticks*startFrame/frameRate)/rate):start;return leaf;}
idMD6Node*idAnimWebBlendEquation::AddLeafPauseFromDefaultLeaf(idMD6Allocator&allocator,const existingNodeList_t&existing,float animIndex,float frame,int wrapMode,md6WeightGroup_t weightGroup){idMD6LeafPause*leaf=static_cast<idMD6LeafPause*>(AllocateNode(allocator,idMD6Node::NODE_LEAF_PAUSE,allocatedNodes));if(!leaf)return nullptr;const int index=static_cast<int>(animIndex);leaf->anim=index>=0&&index<existing.Num()?LeafAnim(existing[index].node):nullptr;leaf->frame=frame;leaf->flags=0;leaf->wrapMode=static_cast<std::uint8_t>(wrapMode);leaf->weightGroup=static_cast<std::uint8_t>(weightGroup);return leaf;}

const idDeclAnimWebTagGroup*idAnimWebBlendEquation::ProcessTagGroup(bool copyOnPlay,const char*groupName,const idDeclAnimWebNode::idModelInfo&modelInfo,idMD6Node&node,scalarList_t&scalars,scalarPairList_t&pairs,unsigned&tagBias,unsigned&tagCount,unsigned&defaultTag,unsigned&groupIndex,unsigned&groupMask){tagBias=tagCount=defaultTag=groupIndex=groupMask=0;unsigned globalTag=0;const idDeclAnimWebTagGroup*selected=nullptr;if(!modelInfo.tagGroups)return nullptr;for(int g=0;g<modelInfo.tagGroups->Num();++g){const auto&group=(*modelInfo.tagGroups)[g];if(!selected&&(!groupName||idStr::Icmp(group.name.str,groupName)==0)){selected=&group;tagBias=globalTag;tagCount=group.tags.Num();groupIndex=g;for(int t=0;t<group.tags.Num()&&globalTag+t<32;++t)groupMask|=1u<<(globalTag+t);}for(int t=0;t<group.tags.Num()&&globalTag<32;++t,++globalTag){if(group.tags[t].isDefault)defaultTag|=1u<<globalTag;if(node.type!=idMD6Node::NODE_TAG_FILTER){int scalarIndex,pairIndex;AddScalar(group.tags[t].name.str,scalars,pairs,scalarIndex,pairIndex);auto&pair=pairs[pairIndex];pair.node=&node;pair.fieldFlags=static_cast<std::uint8_t>(0x10|(copyOnPlay?2:1)|scalars[scalarIndex].flags);pair.data1=static_cast<std::uint8_t>(globalTag);}}}return selected;}
void idAnimWebBlendEquation::ParseScalar(idLexer&source,scalarList_t&scalars,scalarPairList_t&pairs,int&scalarIndex,int&pairIndex,float&value,bool&copyOnPlay){scalarIndex=pairIndex=-1;value=0.0f;copyOnPlay=false;idToken token;source.ReadToken(token);if(token.type==TT_NUMBER){token.NumberValue();value=token.floatvalue;return;}if(token=="-"){value=-source.ParseFloat();return;}if(token=="#"||token=="onplay"){copyOnPlay=true;source.ExpectTokenType(TT_NAME,0,token);}else if(token.type!=TT_NAME){source.Error("Expected a scalar variable name or a number, got '%s'",token.data);return;}AddScalar(token.data,scalars,pairs,scalarIndex,pairIndex);if(scalarIndex>=0&&scalars[scalarIndex].scalarPtr)value=*scalars[scalarIndex].scalarPtr;}

void idAnimWebBlendEquation::ParseNode(
    idLexer& source, const char* name, const idMD6Model*, idMD6Allocator& allocator,
    scalarList_t& scalars, existingNodeList_t& existing, idMD6Node*& root,
    scalarPairList_t& pairs, animList_t* animList) {
  root = nullptr;
  const bool animArrayName = idStr::Icmp(name, "anim") == 0;
  const bool animNumberName = idStr::Icmpn(name, "anim", 4) == 0;
  if (animList && animNumberName)
    animList->animPropIndex = std::atoi(name + 4);

  int indexScalar = -1, indexPair = -1;
  float firstIndex = animNumberName ? static_cast<float>(std::atoi(name + 4)) : 0.0f;
  float lastIndex = firstIndex;
  bool indexCopyOnPlay = false;
  bool span = false;
  int modulo = 0;
  int offset = 0;
  if (source.CheckTokenString("[")) {
    ParseScalar(source, scalars, pairs, indexScalar, indexPair, firstIndex,
                indexCopyOnPlay);
    if (!animArrayName)
      source.Error("Unrecognized anim array name '%s' - must use syntax 'anim[ ... ]'", name);
    if (indexScalar < 0) {
      if (firstIndex < 0.0f || firstIndex >= static_cast<float>(existing.Num()))
        source.Error("Blend equation uses 'anim' array, but array index is out of bounds!");
      if (static_cast<float>(static_cast<int>(firstIndex)) != firstIndex)
        source.Error("non-integer anim index used (%f)!", firstIndex);
      if (animList && source.CheckTokenString(",")) {
        int spanScalar = -1, spanPair = -1;
        bool spanCopy = false;
        ParseScalar(source, scalars, pairs, spanScalar, spanPair, lastIndex, spanCopy);
        if (spanScalar >= 0)
          source.Error("anim array span endpoints must be constant");
        if (lastIndex < 0.0f || lastIndex >= static_cast<float>(existing.Num()))
          source.Error("Blend equation uses 'anim' array, but span index is out of bounds!");
        if (static_cast<float>(static_cast<int>(lastIndex)) != lastIndex)
          source.Error("non-integer span index used (%f)!", lastIndex);
        if (firstIndex > lastIndex)
          std::swap(firstIndex, lastIndex);
        span = true;
        animList->animPropIndex = static_cast<int>(firstIndex);
      } else {
        if (source.CheckTokenString("%")) {
          if (source.CheckTokenString("_numanims"))
            modulo = existing.Num();
          else {
            float immediate = 0.0f;
            ParseImmediate(source, immediate);
            modulo = static_cast<int>(immediate);
            if (static_cast<float>(modulo) != immediate)
              source.Error("non-integer anim index modulo used (%f)!", immediate);
          }
        }
        if (source.CheckTokenString("+")) {
          float immediate = 0.0f;
          ParseImmediate(source, immediate);
          offset = static_cast<int>(immediate);
          if (static_cast<float>(offset) != immediate)
            source.Error("non-integer anim index offset used (%f)!", immediate);
        }
      }
    }
    source.ExpectTokenString("]");
  }

  const auto weightGroup = static_cast<md6WeightGroup_t>(ParseWeightGroup(source));
  float rate = 1.0f;
  int rateScalar = -1, ratePair = -1;
  bool rateCopyOnPlay = false;
  if (source.CheckTokenString("/") || source.CheckTokenString("scaled")) {
    source.CheckTokenString("by");
    ParseScalar(source, scalars, pairs, rateScalar, ratePair, rate, rateCopyOnPlay);
    if (rate <= 0.0f || rate >= 9.0f)
      source.Error("Specified rate scale is out of range. Must be between 0.0f and 9.0f");
  }

  bool paused = false;
  float frame = 0.0f;
  int frameScalar = -1, framePair = -1;
  bool frameCopyOnPlay = false;
  if (source.CheckTokenString("paused")) {
    paused = true;
    source.CheckTokenString("at");
    ParseScalar(source, scalars, pairs, frameScalar, framePair, frame, frameCopyOnPlay);
    if (frame <= 0.0f || frame >= 1.0f)
      source.Error("Specified normalized pause frame is out of range. Must be between 0.0f and 1.0f");
  }

  auto bindScalar = [&](int scalarIndex, int& pairIndex, std::uint8_t fieldType,
                        bool copyOnPlay, idMD6Node* node, bool duplicate) {
    if (scalarIndex < 0 || pairIndex < 0 || node == nullptr)
      return;
    if (duplicate) {
      idAnimWebScalarPair pair;
      pair.scalarIndex = scalarIndex;
      pairIndex = pairs.Append(pair);
    }
    idAnimWebScalarPair& pair = pairs[pairIndex];
    pair.node = node;
    pair.scalarIndex = scalarIndex;
    pair.coordinateIndex = -1;
    pair.fieldType = fieldType;
    pair.fieldFlags = static_cast<std::uint8_t>((copyOnPlay ? 2 : 1) |
                                                scalars[scalarIndex].flags);
    if (fieldType == 1)
      scalars[scalarIndex].flags = idAnimWebScalar::DECLARED;
  };

  if (indexScalar >= 0) {
    for (int index = 0; index < existing.Num(); ++index) {
      idMD6Node* node = existing[index].node;
      if (!node || (node->type != idMD6Node::NODE_LEAF_PLAY &&
                    node->type != idMD6Node::NODE_LEAF_PAUSE))
        source.Error("'anim' arrays can only index leaves that are idMD6LeafPlay or idMD6LeafPause!");
    }
    root = paused
               ? AddLeafPauseFromDefaultLeaf(allocator, existing, firstIndex, frame, 1,
                                             weightGroup)
               : AddLeafPlayFromDefaultLeaf(allocator, existing, firstIndex, startTime,
                                            ticksPerSec, 0, rate, 1, weightGroup);
    if (indexPair >= 0) {
      idAnimWebScalarPair& pair = pairs[indexPair];
      pair.node = root;
      pair.scalarIndex = indexScalar;
      pair.coordinateIndex = -1;
      pair.fieldType = 2;
      pair.fieldFlags = static_cast<std::uint8_t>((indexCopyOnPlay ? 2 : 1) |
                                                  scalars[indexScalar].flags);
      if (modulo > 0) { pair.fieldFlags |= 4; pair.data1 = static_cast<std::uint8_t>(modulo); }
      if (offset > 0) { pair.fieldFlags |= 8; pair.data2 = static_cast<std::uint8_t>(offset); }
    }
    bindScalar(rateScalar, ratePair, 1, rateCopyOnPlay, root, false);
    bindScalar(frameScalar, framePair, 3, frameCopyOnPlay, root, false);
    return;
  }

  const int count = span ? static_cast<int>(lastIndex - firstIndex) + 1 : 1;
  for (int item = 0; item < count; ++item) {
    const int animIndex = static_cast<int>(firstIndex) + item;
    idStr leafName(name);
    if (animArrayName)
      leafName.Format("anim%d", animIndex);
    const int existingIndex = FindExisting(existing, leafName.data);
    idMD6Node* node = existingIndex >= 0 ? existing[existingIndex].node : nullptr;
    if (node && (node->type == idMD6Node::NODE_BRANCH ||
                 node->type == idMD6Node::NODE_BLEND_BRANCH ||
                 node->type == idMD6Node::NODE_BLENDA_BRANCH ||
                 node->type == idMD6Node::NODE_FUSION_BRANCH)) {
      static_cast<idMD6Branch*>(node)->filterGroup = static_cast<std::uint8_t>(weightGroup);
    } else if (node && paused && node->type == idMD6Node::NODE_LEAF_PAUSE) {
      auto* leaf = static_cast<idMD6LeafPause*>(node);
      leaf->weightGroup = static_cast<std::uint8_t>(weightGroup);
      leaf->frame = frame;
    } else if (node && !paused && node->type == idMD6Node::NODE_LEAF_PLAY) {
      auto* leaf = static_cast<idMD6LeafPlay*>(node);
      leaf->weightGroup = static_cast<std::uint8_t>(weightGroup);
      leaf->startTime = startTime;
    } else if (existingIndex >= 0) {
      node = paused
                 ? AddLeafPauseFromDefaultLeaf(allocator, existing,
                                               static_cast<float>(existingIndex), frame, 1,
                                               weightGroup)
                 : AddLeafPlayFromDefaultLeaf(allocator, existing,
                                              static_cast<float>(existingIndex), startTime,
                                              ticksPerSec, 0, rate, 1, weightGroup);
    } else {
      const idMD6Anim* directAnim = nullptr;
      bool implicitAnim = animNumberName;
      if (implicitAnim) {
        for (const char* digit = leafName.data + 4; *digit; ++digit)
          if (!std::isdigit(static_cast<unsigned char>(*digit))) { implicitAnim = false; break; }
      }
      if (!implicitAnim && AnimationPreloadService())
        directAnim = AnimationPreloadService()->ResolveAnim(leafName.data);
      if (!implicitAnim && !directAnim)
        source.Error("'%s' is not a valid animation or leaf node name.", leafName.data);
      if (implicitAnim && animIndex >= 0 && animIndex < existing.Num())
        directAnim = LeafAnim(existing[animIndex].node);
      if (paused) {
        auto* leaf = static_cast<idMD6LeafPause*>(
            AllocateNode(allocator, idMD6Node::NODE_LEAF_PAUSE, allocatedNodes));
        if (leaf) { leaf->anim=directAnim; leaf->frame=frame; leaf->flags=0; leaf->wrapMode=1; leaf->weightGroup=static_cast<std::uint8_t>(weightGroup); }
        node = leaf;
      } else {
        auto* leaf = static_cast<idMD6LeafPlay*>(
            AllocateNode(allocator, idMD6Node::NODE_LEAF_PLAY, allocatedNodes));
        if (leaf) { leaf->anim=directAnim; leaf->rateScale=rate; leaf->wrapMode=1; leaf->weightGroup=static_cast<std::uint8_t>(weightGroup); leaf->initCounter=1; leaf->startTime=startTime; }
        node = leaf;
      }
    }
    root = node;
    if (span && animList && node)
      animList->animList.Append(node);
    bindScalar(rateScalar, ratePair, 1, rateCopyOnPlay, node, item > 0);
    bindScalar(frameScalar, framePair, 3, frameCopyOnPlay, node, item > 0);
  }
}

void idAnimWebBlendEquation::ParseAnimList(
    idLexer& source, const idDeclAnimWebNode::idModelInfo& modelInfo,
    const idMD6Model* model, idMD6Allocator& allocator,
    existingNodeList_t& existing, const idDeclAnimWebTagGroup* tagGroup,
    animList_t& result) {
  bool hadExplicitEntries = false;
  while (source.CheckTokenString(",")) {
    hadExplicitEntries = true;
    scalarList_t localScalars;
    scalarPairList_t localPairs;
    animList_t nested;
    idMD6Node* node = nullptr;
    Parse_r(source, modelInfo, model, allocator, localScalars, existing, node, localPairs, &nested);

    for (int filter = 0; filter < nested.filterList.Num(); ++filter)
      result.filterList.Append(nested.filterList[filter]);

    if (nested.animPropIndex < 0) {
      if (node != nullptr && node->type != idMD6Node::NODE_TAG_FILTER) {
        result.animList.Append(node);
        result.tagList.Append(nested.tagList.Num() ? nested.tagList[0] : 0u);
      }
      for (int anim = 0; anim < nested.animList.Num(); ++anim) {
        result.animList.Append(nested.animList[anim]);
        result.tagList.Append(anim < nested.tagList.Num() ? nested.tagList[anim] : 0u);
      }
      continue;
    }

    const int count = std::max(1, nested.animList.Num());
    for (int anim = 0; anim < count; ++anim) {
      result.animList.Append(nested.animList.Num() ? nested.animList[anim] : node);
      const int propIndex = nested.animPropIndex + anim;
      const unsigned tags = tagGroup != nullptr && propIndex >= 0 &&
                                    propIndex < modelInfo.animProps.Num()
                                ? modelInfo.animProps[propIndex].tags
                                : 0u;
      result.tagList.Append(tags);
    }
  }

  if (hadExplicitEntries)
    return;

  // Omitting the comma-separated list means all model animations.
  for (int anim = 0; anim < modelInfo.animProps.Num(); ++anim) {
    idMD6Node* node = anim < existing.Num() ? existing[anim].node : nullptr;
    if (node == nullptr) {
      const idMD6AnimProps& props = modelInfo.animProps[anim];
      idMD6LeafPlay* leaf = static_cast<idMD6LeafPlay*>(
          AllocateNode(allocator, idMD6Node::NODE_LEAF_PLAY, allocatedNodes));
      if (leaf != nullptr) {
        leaf->anim = props.anim;
        leaf->startTime = startTime;
        leaf->rateScale = props.rate;
        leaf->wrapMode = static_cast<std::uint8_t>(props.wrap);
        leaf->weightGroup = 0;
      }
      node = leaf;
    }
    if (node != nullptr) {
      result.animList.Append(node);
      result.tagList.Append(tagGroup != nullptr ? modelInfo.animProps[anim].tags : 0u);
    }
  }
}

void idAnimWebBlendEquation::ParseBlend(bool useUserChannels,int dimensions,idLexer&source,const idDeclAnimWebNode::idModelInfo&modelInfo,const idMD6Model*model,idMD6Allocator*allocator,existingNodeList_t&existing,scalarList_t&scalars,scalarPairList_t&pairs,idList<idUserChannelIndex>&userChannelIndices,idList<float>&coordinateList,idList<idMD6Node*>&animationList,idList<float>&currentCoordinate,idList<int>&currentCoordinateScalarIndex,idList<int>&currentCoordinateScalarPairIndex,idList<bool>&isCopyOnPlay,md6WeightGroup_t&weightGroup,idMD6Node**baseAnimation){
  dimensions=std::clamp(dimensions,1,6);currentCoordinate.SetNum(dimensions);currentCoordinateScalarIndex.SetNum(dimensions);currentCoordinateScalarPairIndex.SetNum(dimensions);isCopyOnPlay.SetNum(dimensions);source.ExpectTokenString("(");
  for(int dimension=0;dimension<dimensions;++dimension){if(dimension)source.ExpectTokenString(",");ParseScalar(source,scalars,pairs,currentCoordinateScalarIndex[dimension],currentCoordinateScalarPairIndex[dimension],currentCoordinate[dimension],isCopyOnPlay[dimension]);}
  if(useUserChannels){for(int dimension=0;dimension<dimensions;++dimension){source.ExpectTokenString(",");idToken channel;source.ExpectAnyToken(channel);const std::int16_t index=AnimationPreloadService()?AnimationPreloadService()->ResolveUserChannelIndex(model,channel.data):-1;userChannelIndices.Append(idUserChannelIndex(index));}}
  if(baseAnimation){source.ExpectTokenString(",");Parse_r(source,modelInfo,model,*allocator,scalars,existing,*baseAnimation,pairs);}
  bool separatorConsumed=false;
  while(!source.PeekTokenString(")")){
    if(!separatorConsumed)source.ExpectTokenString(",");separatorConsumed=false;
    animList_t expanded;idMD6Node*node=nullptr;Parse_r(source,modelInfo,model,*allocator,scalars,existing,node,pairs,baseAnimation?nullptr:&expanded);
    const int count=std::max(1,expanded.animList.Num());idList<float,5>overrideCoordinates;
    if(!baseAnimation&&source.CheckTokenString(",")){separatorConsumed=true;source.CheckTokenString("=");idToken number;if(source.PeekTokenType(TT_NUMBER,0,number)){separatorConsumed=false;for(int dimension=0;dimension<dimensions;++dimension){if(dimension)source.ExpectTokenString(",");overrideCoordinates.Append(source.ParseFloat());}}}
    for(int item=0;item<count;++item){idMD6Node*animation=expanded.animList.Num()?expanded.animList[item]:node;animationList.Append(animation);const int propIndex=expanded.animPropIndex>=0?expanded.animPropIndex+item:-1;for(int dimension=0;dimension<dimensions;++dimension){float coordinate=0.0f;if(dimension<overrideCoordinates.Num())coordinate=overrideCoordinates[dimension];else if(modelInfo.coordinates&&modelInfo.animProps.Num()>0&&propIndex>=0){const int storedDimensions=modelInfo.coordinates->Num()/modelInfo.animProps.Num();const int offset=propIndex*storedDimensions+dimension;if(dimension<storedDimensions&&offset<modelInfo.coordinates->Num())coordinate=(*modelInfo.coordinates)[offset];}coordinateList.Append(coordinate);}}
  }
  source.ExpectTokenString(")");weightGroup=static_cast<md6WeightGroup_t>(ParseWeightGroup(source));
}

void idAnimWebBlendEquation::Parse_r(
    idLexer& source, const idDeclAnimWebNode::idModelInfo& modelInfo,
    const idMD6Model* model, idMD6Allocator& allocator,
    scalarList_t& scalars, existingNodeList_t& existing,
    idMD6Node*& root, scalarPairList_t& pairs, animList_t* animList) {
  idToken token;
  if (!source.ReadToken(token)) {
    source.Error("Empty token");
    root = nullptr;
    return;
  }
  token.ToLower();
  ++expressionLevel;

  if (token == "(") {
    Parse_r(source, modelInfo, model, allocator, scalars, existing, root, pairs, nullptr);
    source.ExpectTokenString(")");
    --expressionLevel;
    return;
  }

  const bool basicBranch = token == "lerp" || token == "addr" || token == "addl" ||
                           token == "subr" || token == "subl";
  if (basicBranch) {
    source.ExpectTokenString("(");
    idMD6Node* left = nullptr;
    idMD6Node* right = nullptr;
    Parse_r(source, modelInfo, model, allocator, scalars, existing, left, pairs);
    source.ExpectTokenString(",");
    Parse_r(source, modelInfo, model, allocator, scalars, existing, right, pairs);
    source.ExpectTokenString(",");

    int scalarIndex = -1;
    int pairIndex = -1;
    float alpha = 0.0f;
    bool copyOnPlay = false;
    ParseScalar(source, scalars, pairs, scalarIndex, pairIndex, alpha, copyOnPlay);
    float targetAlpha = alpha;
    float alphaRate = 0.0f;
    if (scalarIndex >= 0) {
      if (source.CheckTokenString("<")) {
        targetAlpha = 0.0f;
        alphaRate = source.ParseFloat();
      } else if (source.CheckTokenString(">")) {
        targetAlpha = 1.0f;
        alphaRate = source.ParseFloat();
      } else if (source.CheckTokenString(",")) {
        targetAlpha = source.ParseFloat();
        source.ExpectTokenString(",");
        alphaRate = source.ParseFloat();
      }
    }
    source.ExpectTokenString(")");
    const unsigned weightGroup = ParseWeightGroup(source);

    idMD6Branch* branch = static_cast<idMD6Branch*>(
        AllocateNode(allocator, idMD6Node::NODE_BRANCH, allocatedNodes));
    if (branch != nullptr) {
      branch->left = left;
      branch->right = right;
      branch->op = token == "addl" ? 3 : token == "addr" ? 4 :
                   token == "subl" ? 5 : token == "subr" ? 6 : 1;
      branch->originBlend = 0;
      branch->currentAlpha = alpha;
      branch->targetAlpha = targetAlpha;
      branch->alphaRate = alphaRate;
      branch->filterGroup = static_cast<std::uint8_t>(weightGroup);
    }
    root = branch;
    if (scalarIndex >= 0 && pairIndex >= 0) {
      pairs[pairIndex].node = root;
      pairs[pairIndex].fieldType = 0;
      pairs[pairIndex].fieldFlags = static_cast<std::uint8_t>(
          (copyOnPlay ? 2 : 1) | scalars[scalarIndex].flags);
    }
  } else if (token == "choose" || token == "select") {
    source.ExpectTokenString("(");
    idMD6Node* left = nullptr;
    idMD6Node* right = nullptr;
    Parse_r(source, modelInfo, model, allocator, scalars, existing, left, pairs);
    source.ExpectTokenString(",");
    Parse_r(source, modelInfo, model, allocator, scalars, existing, right, pairs);
    source.ExpectTokenString(",");
    int scalarIndex = -1, pairIndex = -1;
    float selector = 0.0f;
    bool copyOnPlay = false;
    ParseScalar(source, scalars, pairs, scalarIndex, pairIndex, selector, copyOnPlay);
    if (scalarIndex < 0)
      source.Warning("Blend equation uses 'Select' function, but selector is a constant. Is this intended?");
    source.ExpectTokenString(")");
    auto* branch = static_cast<idMD6Branch*>(
        AllocateNode(allocator, idMD6Node::NODE_BRANCH, allocatedNodes));
    if (branch) {
      branch->left = left;
      branch->right = right;
      branch->op = 1;
      branch->originBlend = 0;
      branch->currentAlpha = selector;
      branch->targetAlpha = selector;
      branch->alphaRate = 0.0f;
      branch->filterGroup = 0;
      branch->blendType = 0;
    }
    root = branch;
    if (scalarIndex >= 0 && pairIndex >= 0) {
      pairs[pairIndex].node = root;
      pairs[pairIndex].scalarIndex = scalarIndex;
      pairs[pairIndex].coordinateIndex = -1;
      pairs[pairIndex].fieldType = 0;
      pairs[pairIndex].fieldFlags = static_cast<std::uint8_t>(
          (copyOnPlay ? 2 : 1) | scalars[scalarIndex].flags);
    }
  } else if (token == "blenda") {
    idList<idUserChannelIndex>channels;idList<float>coordinates,current;idList<idMD6Node*>animations;idList<int>scalarIndices,pairIndices;idList<bool>copyFlags;md6WeightGroup_t weightGroup=MD6_WEIGHTGROUP_ALL;idMD6Node*base=nullptr;
    ParseBlend(false,1,source,modelInfo,model,&allocator,existing,scalars,pairs,channels,coordinates,animations,current,scalarIndices,pairIndices,copyFlags,weightGroup,&base);
    idMD6BlendAdditiveBranch* branch = static_cast<idMD6BlendAdditiveBranch*>(
        AllocateNode(allocator, idMD6Node::NODE_BLENDA_BRANCH, allocatedNodes));
    if (branch != nullptr) {
      branch->baseAnimation = base;
      for(int i=0;i<animations.Num();++i)branch->animationList.Append(animations[i]);
      for(int i=0;i<coordinates.Num();++i)branch->coordinateList.Append(coordinates[i]);
      branch->currentCoordinate = current.Num()?current[0]:0.0f;
      branch->filterGroup=static_cast<std::uint8_t>(weightGroup);
    }
    root = branch;
    if (scalarIndices.Num()&&scalarIndices[0]>=0&&pairIndices[0]>=0) {
      pairs[pairIndices[0]].node=root;pairs[pairIndices[0]].scalarIndex=scalarIndices[0];pairs[pairIndices[0]].coordinateIndex=0;pairs[pairIndices[0]].fieldType=4;pairs[pairIndices[0]].fieldFlags=static_cast<std::uint8_t>((copyFlags[0]?2:1)|scalars[scalarIndices[0]].flags);
    }
  } else if (token.Icmpn("blend", 5) == 0) {
    const char* suffix = token.data + 5;
    bool userChannels = *suffix == 'u';
    if (userChannels)
      ++suffix;
    bool angleWrapped = *suffix == 'y';
    if (angleWrapped)
      ++suffix;
    const int dimensions=std::max(1,std::atoi(suffix));idList<idUserChannelIndex>channels;idList<float>coordinates,current;idList<idMD6Node*>animations;idList<int>scalarIndices,pairIndices;idList<bool>copyFlags;md6WeightGroup_t weightGroup=MD6_WEIGHTGROUP_ALL;
    ParseBlend(userChannels,dimensions,source,modelInfo,model,&allocator,existing,scalars,pairs,channels,coordinates,animations,current,scalarIndices,pairIndices,copyFlags,weightGroup,nullptr);
    auto*branch=static_cast<idMD6BlendBranch*>(AllocateNode(allocator,idMD6Node::NODE_BLEND_BRANCH,allocatedNodes));if(branch){branch->numDimensions=static_cast<std::uint8_t>(dimensions);branch->isAngleWrapped=angleWrapped;branch->filterGroup=static_cast<std::uint8_t>(weightGroup);for(int i=0;i<channels.Num();++i)branch->userChannelIndices.Append(channels[i]);for(int i=0;i<coordinates.Num();++i)branch->coordinateList.Append(coordinates[i]);for(int i=0;i<animations.Num();++i)branch->animationList.Append(animations[i]);for(int i=0;i<current.Num();++i)branch->currentCoordinate.Append(current[i]);}root=branch;for(int i=0;i<scalarIndices.Num();++i)if(scalarIndices[i]>=0&&pairIndices[i]>=0){auto&pair=pairs[pairIndices[i]];pair.node=root;pair.scalarIndex=scalarIndices[i];pair.coordinateIndex=static_cast<std::int16_t>(i);pair.fieldType=4;pair.fieldFlags=static_cast<std::uint8_t>((copyFlags[i]?2:1)|scalars[scalarIndices[i]].flags);}
  } else if (token.Icmpn("fusion", 6) == 0) {
    const char* suffix = token.data + 6;
    bool userChannels = *suffix == 'u';
    if (userChannels)
      ++suffix;
    bool angleWrapped = *suffix == 'y';
    if (angleWrapped)
      ++suffix;
    if (userChannels && angleWrapped)
      source.Warning("FusionUY is invalid; angle wrapping disabled");
    const int dimensions=std::max(1,std::atoi(suffix));idList<idUserChannelIndex>channels;idList<float>coordinates,current;idList<idMD6Node*>animations;idList<int>scalarIndices,pairIndices;idList<bool>copyFlags;md6WeightGroup_t weightGroup=MD6_WEIGHTGROUP_ALL;
    ParseBlend(userChannels,dimensions,source,modelInfo,model,&allocator,existing,scalars,pairs,channels,coordinates,animations,current,scalarIndices,pairIndices,copyFlags,weightGroup,nullptr);
    auto*branch=static_cast<idMD6FusionBranch*>(AllocateNode(allocator,idMD6Node::NODE_FUSION_BRANCH,allocatedNodes));if(branch){branch->numDimensions=static_cast<std::uint8_t>(dimensions);branch->isAngleWrapped=angleWrapped&&!userChannels;branch->filterGroup=static_cast<std::uint8_t>(weightGroup);for(int i=0;i<channels.Num();++i)branch->userChannelIndices.Append(channels[i]);for(int i=0;i<coordinates.Num();++i)branch->coordinateList.Append(coordinates[i]);for(int i=0;i<animations.Num();++i)branch->animationList.Append(animations[i]);for(int i=0;i<current.Num();++i)branch->currentCoordinate.Append(current[i]);}root=branch;for(int i=0;i<scalarIndices.Num();++i)if(scalarIndices[i]>=0&&pairIndices[i]>=0){auto&pair=pairs[pairIndices[i]];pair.node=root;pair.scalarIndex=scalarIndices[i];pair.coordinateIndex=static_cast<std::int16_t>(i);pair.fieldType=4;pair.fieldFlags=static_cast<std::uint8_t>((copyFlags[i]?2:1)|scalars[scalarIndices[i]].flags);}
  } else if (token == "best" || token == "filter") {
    source.ExpectTokenString("(");
    idToken groupToken;
    source.ExpectAnyToken(groupToken);
    const bool copyOnPlay = ParseScalarName(source, groupToken);

    const idMD6Node::nodeType_t nodeType = token == "best" ? idMD6Node::NODE_BEST_LEAF
                                                           : idMD6Node::NODE_TAG_FILTER;
    root = AllocateNode(allocator, nodeType, allocatedNodes);
    unsigned tagBias = 0;
    unsigned tagCount = 0;
    unsigned defaultTag = 0;
    unsigned groupIndex = 0;
    unsigned groupMask = 0;
    const idDeclAnimWebTagGroup* tagGroup = ProcessTagGroup(
        copyOnPlay, groupToken.data, modelInfo, *root, scalars, pairs, tagBias,
        tagCount, defaultTag, groupIndex, groupMask);
    if (tagGroup == nullptr)
      source.Error("Unknown animation tag group '%s'", groupToken.data);

    if (token == "best") {
      animList_t choices;
      ParseAnimList(source, modelInfo, model, allocator, existing, tagGroup, choices);
      source.ExpectTokenString(")");
      idMD6BestLeaf* best = static_cast<idMD6BestLeaf*>(root);
      for (int index = 0; index < choices.animList.Num(); ++index)
        best->leafList.Append(choices.animList[index]);
      for (int index = 0; index < choices.tagList.Num(); ++index)
        best->tagList.Append(choices.tagList[index]);
      for (int index = 0; index < choices.filterList.Num(); ++index)
        best->filterList.Append(choices.filterList[index]);
      best->tagGroupFilter.tagGroup = tagGroup;
      best->tagGroupFilter.tagMask = groupMask;
      best->tagGroupFilter.tagGroupIndex = static_cast<std::uint8_t>(groupIndex);
      best->tagGroupFilter.tagBias = static_cast<std::uint8_t>(tagBias);
      best->tagGroup = tagGroup;
      best->desiredTag = defaultTag;
      best->activeTag = defaultTag;
      best->defaultTag = defaultTag;
      best->tagBias = static_cast<std::uint8_t>(tagBias);
      best->tagGroupIndex = static_cast<std::uint8_t>(groupIndex);
    } else {
      animList_t ignored;
      animList_t& filteredAnimations = animList != nullptr ? *animList : ignored;
      ParseAnimList(source, modelInfo, model, allocator, existing, tagGroup,
                    filteredAnimations);
      source.ExpectTokenString(")");
      idMD6TagFilter* filter = static_cast<idMD6TagFilter*>(root);
      filter->tagGroup = tagGroup;
      filter->tagMask = groupMask;
      filter->tagGroupIndex = static_cast<std::uint8_t>(groupIndex);
      filter->tagBias = static_cast<std::uint8_t>(tagBias);
      if (animList != nullptr)
        animList->filterList.Append(filter);
    }
  } else {
    ParseNode(source, token.data, model, allocator, scalars, existing, root, pairs,
              animList);
  }
  --expressionLevel;
}

bool idAnimWebBlendEquation::Parse(const idDeclAnimWebNode::idModelInfo&modelInfo,const idMD6Model*model,idMD6Allocator*allocator,int time,int ticksPerSecond,const char*equation,scalarList_t&scalars,existingNodeList_t&existing,idMD6Node*&root,scalarPairList_t&pairs){if(!allocator)return false;idLexer source(LEXFL_ALLOWPATHNAMES|LEXFL_ALLOWNUMBERNAMES);if(!source.LoadMemory(equation,idStr::Length(equation),"AnimWebBlendEquation"))return false;allocatedNodes.Clear();startTime=time;ticksPerSec=ticksPerSecond;expressionLevel=0;root=nullptr;Parse_r(source,modelInfo,model,*allocator,scalars,existing,root,pairs);if(!source.EndOfFile()){idToken trailing;if(source.ReadToken(trailing))source.Error("Unexpected token '%s' after blend equation",trailing.data);}if(source.HadError()){for(int i=0;i<allocatedNodes.Num();++i)allocator->FreeMem(allocatedNodes[i]);allocatedNodes.Clear();root=nullptr;return false;}for(int i=pairs.Num()-1;i>0;--i)if(pairs[i-1].fieldType==2&&pairs[i].fieldType==1)pairs[i-1].fieldFlags|=0x20;return root!=nullptr;}
