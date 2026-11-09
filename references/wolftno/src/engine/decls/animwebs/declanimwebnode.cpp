#include "declanimwebnode.h"
#include "engine/animation/animationpreload.h"
#include "shared/idlib/lib.h"

#include <algorithm>

idDeclAnimWebNode::idModelInfo::idModelInfo():modelIndex(),coordinates(nullptr),tagGroups(nullptr){}
idDeclAnimWebNode::idModelInfo::idModelInfo(const idModelInfo&o):idModelInfo(){*this=o;}
idDeclAnimWebNode::idModelInfo&idDeclAnimWebNode::idModelInfo::operator=(const idModelInfo&o){
  if(this!=&o){
    modelIndex=o.modelIndex;blendEq=o.blendEq;blendEqFixed=o.blendEqFixed;animProps=o.animProps;
    if(o.coordinates){if(!coordinates)coordinates=new idList<float,32>;*coordinates=*o.coordinates;}else{delete coordinates;coordinates=nullptr;}
    CopyTagGroups(o);
  }
  return*this;
}
idDeclAnimWebNode::idModelInfo::~idModelInfo(){delete coordinates;delete tagGroups;}
void idDeclAnimWebNode::idModelInfo::SetNumCoordinateDimensions(int count){
  if(count>0){if(!coordinates)coordinates=new idList<float,32>;coordinates->SetNum(count*animProps.Num(),0.0f);}
  else{delete coordinates;coordinates=nullptr;}
}
void idDeclAnimWebNode::idModelInfo::SetCoordinate(int propIndex,const idList<float,5>&value){
  if(!coordinates||coordinates->Num()<=0||animProps.Num()<=0)return;
  const int offset=propIndex*value.Num();
  for(int i=0;i<value.Num()&&offset+i<coordinates->Num();++i)(*coordinates)[offset+i]=value[i];
}
void idDeclAnimWebNode::idModelInfo::AddTagGroup(){
  if(!tagGroups)tagGroups=new idList<idDeclAnimWebTagGroup,32>;
  idDeclAnimWebTagGroup group;group.name.Set("TagGroup");tagGroups->Append(group);
}
void idDeclAnimWebNode::idModelInfo::UpdateTagGroupsList(int count){
  if(count>0){if(!tagGroups)tagGroups=new idList<idDeclAnimWebTagGroup,32>;}
  else{delete tagGroups;tagGroups=nullptr;}
}
void idDeclAnimWebNode::idModelInfo::CopyTagGroups(const idModelInfo&o){
  const int count=o.tagGroups?o.tagGroups->Num():0;UpdateTagGroupsList(count);
  if(count>0){*tagGroups=*o.tagGroups;const int n=std::min(animProps.Num(),o.animProps.Num());for(int i=0;i<n;++i)animProps[i].tags=o.animProps[i].tags;}
}
void idDeclAnimWebNode::idModelInfo::FixUpAnimVars(){
  idStr temp(blendEq.str);
  for(int i=animProps.Num()-1;i>=0;--i){
    idStr animVarName;animVarName.Format("anim%d",i);
    if(temp.Find(animVarName.data,true,0,-1)<0)continue;
    const idMD6AnimProps&props=animProps[i];const char*name=nullptr;
    if(props.aliasHandle==0xffff){if(props.anim)name=props.anim->name.str;}
    else if(idAnimationPreload*service=AnimationPreloadService())name=service->FindAliasName(props.aliasHandle);
    if(name)temp.Replace(animVarName.data,name);
  }
  blendEqFixed.Set(temp.data);
}
namespace {
template<class ResolveModel>
void ParseModelInfoRetail(idDeclAnimWebNode::idModelInfo& info,idParser&source,ResolveModel resolveModel){
  idToken token;
  source.ExpectTokenString("tree");
  source.ExpectTokenString("{");
  if(source.CheckTokenString("modelIndex"))info.modelIndex=animWebModelIndex_t(source.ParseInt());
  else if(source.CheckTokenString("modelName")){source.ExpectTokenType(TT_STRING,0,token);info.modelIndex=animWebModelIndex_t(resolveModel(token.data));if(info.modelIndex.value<0){idLib::Warning("Failed to resolve modelName '%s'",token.data);info.modelIndex=animWebModelIndex_t(0);}}
  else info.modelIndex=animWebModelIndex_t(0);
  source.ExpectTokenString("blendEq");source.ExpectTokenType(TT_STRING,0,token);info.blendEq.Set(*token.data?token.data:"anim0");
  while(source.CheckTokenString("tagGroup")){
    info.AddTagGroup();idDeclAnimWebTagGroup&group=(*info.tagGroups)[info.tagGroups->Num()-1];source.ExpectTokenType(TT_STRING,0,token);group.name.Set(token.data);source.ExpectTokenString("{");
    while(source.CheckTokenString("tag")){idDeclAnimWebTag tag;source.ExpectTokenType(TT_STRING,0,token);tag.name.Set(token.data);tag.isDefault=source.ParseBool();group.tags.Append(tag);}
    source.ExpectTokenString("}");
  }
  source.ExpectTokenString("anims");source.ExpectTokenString("{");
  while(source.ReadToken(token)&&token!="}"){
    if(token!="alias"){source.Error("Unknown token '%s' in anim list",token.data);break;}
    source.ExpectTokenString("{");source.ExpectTokenString("name");source.ExpectTokenType(TT_STRING,0,token);
    idMD6AnimProps props;idStr animName(token.data),extension;animName.BackSlashesToSlashes();animName.ExtractFileExtension(extension);
    if(idStr::Icmp(extension.data,"md6anim")==0){props.SetAnim(AnimationPreloadService()?AnimationPreloadService()->ResolveAnim(animName.data):nullptr);}
    else if(extension.IsEmpty())props.SetAliasHandle(AnimationPreloadService()?AnimationPreloadService()->ResolveAliasHandle(nullptr,animName.data):static_cast<std::uint16_t>(idStr::IHash(animName.data)));
    else source.Error("Alias names must not have extensions: '%s'",animName.data);
    idList<float,5> coordinate;
    while(source.ReadToken(token)&&token!="}"){
      if(token=="wrap"){source.ExpectTokenType(TT_NAME,0,token);if(token=="WRAP_CLAMP"||token=="clamp")props.wrap=idMD6AnimProps::WRAP_CLAMP;else if(token=="WRAP_REPEAT"||token=="repeat")props.wrap=idMD6AnimProps::WRAP_LOOP;else source.Error("Unknown wrap mode '%s'",token.data);}
      else if(token=="rate")props.rate=source.ParseFloat();
      else if(token=="tags"){
        while(source.ReadTokenOnLine(token)){bool found=false;unsigned tagBit=1;for(int g=0;info.tagGroups&&g<info.tagGroups->Num()&&!found;++g)for(int t=0;t<(*info.tagGroups)[g].tags.Num();++t){if(idStr::Cmp((*info.tagGroups)[g].tags[t].name.str,token.data)==0){props.tags|=tagBit;found=true;break;}tagBit=(tagBit<<1)|(tagBit>>31);}if(!found)source.Warning("Unknown tag '%s'",token.data);}
      }else if(token=="coordinate"){
        source.ExpectTokenString("(");float sign=1.0f;while(source.ReadToken(token)&&token!=")"){if(token=="-"){sign=-1.0f;continue;}if(token==",")continue;if(token.type!=TT_NUMBER){source.Error("Expected coordinate value, got '%s'",token.data);continue;}token.NumberValue();coordinate.Append(sign*token.floatvalue);sign=1.0f;}
      }else source.Error("Unknown token '%s' in alias block",token.data);
    }
    const int animIndex=info.animProps.Append(props);
    info.SetNumCoordinateDimensions(coordinate.Num());
    info.SetCoordinate(animIndex,coordinate);
  }
  source.ExpectTokenString("}");
  info.FixUpAnimVars();
}
int FindAtomicName(const idList<idAtomicString,32>&names,const char*name){for(int i=0;i<names.Num();++i)if(idStr::Icmp(names[i].str,name)==0)return i;return-1;}
int AnimDeltaForName(const char*name){static const char*const names[]={"DELTA_DEFAULT","DELTA_FULL","DELTA_FULL_NOCLIP","DELTA_FULL_GRAVITY","DELTA_TURN","DELTA_VELOCITY","DELTA_DRIVE_AI","DELTA_IGNORE"};for(int i=0;i<8;++i)if(idStr::Icmp(names[i],name)==0)return i;return-1;}
std::uint16_t NodeCustomFlag(const idToken&token){static const struct{const char*name;std::uint16_t value;}flags[]={{"AW_CF_MELEE",1},{"AW_CF_RECOVER",2},{"AW_CF_RECOVER_ATTACK",4},{"AW_CF_PUPPETMODE",8},{"AW_CF_FORCETRANSITION",0x10},{"AW_CF_UNINTERRUPTIBLE",0x20},{"AW_CF_SYNCGROUP_LOCO",0x100}};for(const auto&flag:flags)if(idStr::Icmp(flag.name,token.data)==0)return flag.value;if(token.type==TT_NUMBER){idToken value(token);value.NumberValue();return static_cast<std::uint16_t>(value.intvalue);}return 0;}
template<class Decl>
void ParseNodeProps(idDeclAnimWebNode&node,Decl&decl,idParser&source){
  idToken token;source.ExpectTokenString("props");source.ExpectTokenString("{");
  while(source.ReadToken(token)&&token!="}"){
    if(token=="delta"){source.ExpectAnyToken(token);int value=AnimDeltaForName(token.data);if(value<0&&token.type==TT_NUMBER){token.NumberValue();value=static_cast<int>(token.intvalue);}if(value<0||value>7)source.Error("Unknown animDelta value '%s'",token.data);else node.animDelta=static_cast<std::uint8_t>(value);}
    else if(token=="startTransition"||token=="transition")node.flags|=1;
    else if(token=="stopTransition")node.flags|=2;
    else if(token=="stepTransition")node.flags|=4;
    else if(token=="turnTransition")node.flags|=8;
    else if(token=="blendOutNode")node.flags|=0x10;
    else if(token=="additiveCamera")node.flags|=0x20;
    else if(token=="genericTransition")node.flags|=0x40;
    else if(token=="hide")node.flags|=0x80;
    else if(token=="customFlags"){while(source.ReadTokenOnLine(token)){const std::uint16_t flag=NodeCustomFlag(token);if(flag)node.customFlags|=flag;else source.Warning("unrecognized custom flag '%s'",token.data);}}
    else if(token=="pos")source.Parse1DMatrix(3,&node.settings.pos.x);
    else source.Error("Unknown token '%s' in anim web node props",token.data);
  }
  ++decl.changeId;
}
}

void idDeclAnimWebNode::idModelInfo::Parse(idDeclAnimWeb&web,idParser&source){idDeclAnimWebNode dummy;Parse(web,dummy,source);}
void idDeclAnimWebNode::idModelInfo::Parse(idDeclAnimWeb&web,idDeclAnimWebNode&,idParser&source){ParseModelInfoRetail(*this,source,[&web](const char*name){for(int i=0;i<web.models.Num();++i)if(web.models[i]&&idStr::Icmp(web.models[i]->name.str,name)==0)return i;return-1;});}
void idDeclAnimWebNode::idModelInfo::Parse(idDeclAnimSubWeb&web,idDeclAnimWebNode&,idParser&source){ParseModelInfoRetail(*this,source,[&web](const char*name){return FindAtomicName(web.models,name);});}

idDeclAnimWebNode::idDeclAnimWebNode():nodeIndex(),subWebIndex(),animDelta(0),flags(0),customFlags(0){}
idDeclAnimWebNode::idDeclAnimWebNode(animWebNodeIndex_t nodeIndex_,animWebSubWebIndex_t subWebIndex_,const animWebStateList_t&states_):nodeIndex(nodeIndex_),subWebIndex(subWebIndex_),states(states_),animDelta(0),flags(0),customFlags(0){}
idDeclAnimWebNode::~idDeclAnimWebNode()=default;
int idDeclAnimWebNode::MatchStates(const animWebStateList_t&other)const{int matched=0;for(int i=0;i<states.numStates;++i)for(int j=0;j<other.numStates;++j)if(states.stateIndex[i].value==other.stateIndex[j].value){++matched;break;}return matched;}
void idDeclAnimWebNode::GetName(const idDeclAnimWeb&web,idStr&name)const{name=web.GetSubWebName(subWebIndex);for(int i=0;i<states.numStates;++i){name.Append('/');name.Append(web.GetStateName(states.stateIndex[i]));}}
void idDeclAnimWebNode::GetName(const idDeclAnimSubWeb&web,idStr&name)const{name=subWebIndex.value>=0&&subWebIndex.value<web.subWebs.Num()?web.subWebs[subWebIndex.value].name.str:"";for(int i=0;i<states.numStates;++i){name.Append('/');const int state=states.stateIndex[i].value;name.Append(state>=0&&state<web.states.Num()?web.states[state].str:"");}}
void idDeclAnimWebNode::GetPath(const idDeclAnimWeb&web,idAnimWebPath&path)const{path.SetPath(web.name.str,web.GetSubWebName(subWebIndex),web.GetStateName(states.stateIndex[0]));}
void idDeclAnimWebNode::GetStateName(const idDeclAnimWeb&web,idStr&name)const{name=web.GetStateName(states.stateIndex[0]);}
void idDeclAnimWebNode::GetStateName(const idDeclAnimWeb&web,int index,idStr&name)const{name=web.GetStateName(states.stateIndex[index]);}
idDeclAnimWebNode::idModelInfo*idDeclAnimWebNode::FindModelInfo(animWebModelIndex_t model){for(int i=0;i<modelInfo.Num();++i)if(modelInfo[i].modelIndex.value==model.value)return&modelInfo[i];return nullptr;}
const idDeclAnimWebNode::idModelInfo*idDeclAnimWebNode::FindModelInfo(animWebModelIndex_t model)const{return const_cast<idDeclAnimWebNode*>(this)->FindModelInfo(model);}
void idDeclAnimWebNode::AddModelInfo(const idModelInfo&info,animWebModelIndex_t model){idModelInfo*destination=FindModelInfo(model);if(destination)*destination=info;else{modelInfo.Append(info);destination=&modelInfo[modelInfo.Num()-1];}destination->modelIndex=model;}
void idDeclAnimWebNode::CreateSubWebEdges(idDeclAnimWeb&web){idList<int>subWebEdgeIndices;for(int i=0;i<edges.Num();++i){const int edgeIndex=edges[i];if(edgeIndex>=0&&edgeIndex<web.edgeCache.Num()&&web.edgeCache[edgeIndex].destNodeIndex.value<0)subWebEdgeIndices.Append(i);}for(int i=0;i<subWebEdgeIndices.Num();++i){const int sourceEdgeIndex=edges[subWebEdgeIndices[i]];idDeclAnimWebEdge edgeCopy=web.edges[sourceEdgeIndex];idAnimWebEdgeCache cacheCopy=web.edgeCache[sourceEdgeIndex];const int destinationSubWeb=edgeCopy.destSubWebIndex.value;if(destinationSubWeb<0||destinationSubWeb>=web.subWebs.Num()||web.subWebs[destinationSubWeb].nodes.Num()==0)continue;web.edgeCache[sourceEdgeIndex].destNodeIndex=web.subWebs[destinationSubWeb].nodes[0];for(int n=1;n<web.subWebs[destinationSubWeb].nodes.Num();++n){const animWebNodeIndex_t destination=web.subWebs[destinationSubWeb].nodes[n];bool duplicate=false;for(int e=0;e<edges.Num();++e){const int existingEdge=edges[e];if(existingEdge>=0&&existingEdge<web.edgeCache.Num()&&web.edgeCache[existingEdge].destNodeIndex.value==destination.value){duplicate=true;break;}}if(duplicate)continue;const animWebEdgeIndex_t newEdge=web.AllocEdge();edges.Append(newEdge.value);web.edges[newEdge.value]=edgeCopy;web.edgeCache[newEdge.value]=cacheCopy;web.edgeCache[newEdge.value].destNodeIndex=destination;}}}
int idDeclAnimWebNode::NumAnims(animWebModelIndex_t model)const{const idModelInfo*info=FindModelInfo(model);return info?info->animProps.Num():0;}
std::size_t idDeclAnimWebNode::Size()const{return 2u*(static_cast<std::size_t>(edges.size)+32u*static_cast<std::size_t>(modelInfo.size))+88u;}
bool idDeclAnimWebNode::Parse(idDeclAnimWeb&web,idParser&source){
  edges.ClearFree();ParseNodeProps(*this,web,source);source.ExpectTokenString("blendTrees");source.ExpectTokenString("{");
  while(source.PeekTokenString("tree")){idModelInfo&info=*modelInfo.Alloc();info.Parse(web,*this,source);}modelInfo.ResizeForPort(modelInfo.Num());source.ExpectTokenString("}");
  source.ExpectTokenString("edges");source.ExpectTokenString("{");idToken token;while(source.ReadToken(token)&&token!="}"){if(token!="edge"){source.Error("Unknown token '%s' in edges block",token.data);break;}const animWebEdgeIndex_t edgeIndex=web.AllocEdge();edges.Append(edgeIndex.value);web.edges[edgeIndex.value].Parse(web,*this,web.edgeCache[edgeIndex.value],source);}edges.ResizeForPort(edges.Num());source.ExpectTokenString("}");return!source.HadError();
}
bool idDeclAnimWebNode::Parse(idDeclAnimSubWeb&web,idParser&source){
  edges.ClearFree();ParseNodeProps(*this,web,source);source.ExpectTokenString("blendTrees");source.ExpectTokenString("{");
  while(source.PeekTokenString("tree")){idModelInfo&info=*modelInfo.Alloc();info.Parse(web,*this,source);}modelInfo.ResizeForPort(modelInfo.Num());source.ExpectTokenString("}");
  source.ExpectTokenString("edges");source.ExpectTokenString("{");idToken token;while(source.ReadToken(token)&&token!="}"){if(token!="edge"){source.Error("Unknown token '%s' in edges block",token.data);break;}const animWebEdgeIndex_t edgeIndex=web.AllocEdge();edges.Append(edgeIndex.value);web.edges[edgeIndex.value].Parse(web,*this,web.edgeCache[edgeIndex.value],source);}edges.ResizeForPort(edges.Num());source.ExpectTokenString("}");return!source.HadError();
}
