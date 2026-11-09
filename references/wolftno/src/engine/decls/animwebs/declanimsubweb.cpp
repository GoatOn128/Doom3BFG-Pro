#include "declanimsubweb.h"

namespace {
int FindAtomicName(const idList<idAtomicString,32>& names,const char* name){for(int i=0;i<names.Num();++i)if(idStr::Icmp(names[i].str,name)==0)return i;return-1;}
}

idDeclInfoTemplate<idDeclAnimSubWeb> idDeclAnimSubWeb::resourceList("subWeb","idDeclAnimSubWeb","subweb",false);
idDeclAnimSubWeb::idDeclAnimSubWeb():changeId(0),color{-1,-1,-1,-1},isHub(false),isVisible(true){resourceListPtr=&resourceList;}
idDeclAnimSubWeb::~idDeclAnimSubWeb(){FreeData();}
const char*idDeclAnimSubWeb::DefaultDefinition()const{return"{\n\tsubwebname \"defaultSubWeb\"\n\tmodels {\n\t}\n\tprops {\n\t}\n}\n";}idDeclInfo*idDeclAnimSubWeb::GetDeclInfo() const{return&resourceList;}
void idDeclAnimSubWeb::FreeData(){for(int i=0;i<nodes.Num();++i)delete nodes[i];nodes.ClearFree();subWebs.ClearFree();states.ClearFree();models.ClearFree();edges.ClearFree();edgeCache.ClearFree();++changeId;}
animWebSubWebIndex_t idDeclAnimSubWeb::AddSubWeb(const char*n){for(int index=0;index<subWebs.Num();++index)if(idStr::Icmp(subWebs[index].name.str,n)==0)return animWebSubWebIndex_t(index);idSubWebInfo_t i;i.name.Set(n);return animWebSubWebIndex_t(subWebs.Append(i));}
animWebNodeIndex_t idDeclAnimSubWeb::AddNode(animWebSubWebIndex_t s,const idDeclAnimWeb::stateList_t&list){if(s.value>=0&&s.value<subWebs.Num())for(int i=0;i<subWebs[s.value].nodes.Num();++i){idDeclAnimWebNode*n=nodes[subWebs[s.value].nodes[i].value];if(n->MatchStates(list)==list.numStates)return n->nodeIndex;}const animWebNodeIndex_t index(nodes.Num());nodes.Append(new idDeclAnimWebNode(index,s,list));if(s.value>=0&&s.value<subWebs.Num())subWebs[s.value].nodes.Append(index);++changeId;return index;}
void idDeclAnimSubWeb::AddNode(const char*subWebName,const idStaticList<idStr,4>&stateNames,animWebNodeIndex_t&nodeIndex,animWebSubWebIndex_t&subWebIndex,idDeclAnimWeb::stateList_t&stateIndices){subWebIndex=AddSubWeb(subWebName);for(int i=0;i<stateNames.Num()&&stateIndices.numStates<4;++i){int state=FindAtomicName(states,stateNames[i].data);if(state<0)state=states.Append(idAtomicString(stateNames[i].data));stateIndices.stateIndex[stateIndices.numStates++]=animWebStateIndex_t(state);}nodeIndex=AddNode(subWebIndex,stateIndices);++changeId;}
animWebEdgeIndex_t idDeclAnimSubWeb::AllocEdge(){const animWebEdgeIndex_t index(edges.Num());edges.Alloc();edgeCache.Alloc();return index;}
void idDeclAnimSubWeb::Parse(idParser&source){
  idToken token;if(source.PeekTokenString("{"))source.ExpectTokenString("{");if(source.PeekTokenString("}")){source.ExpectTokenString("}");return;}FreeData();
  source.ExpectTokenString("subwebname");source.ExpectTokenType(TT_STRING,0,token);const animWebSubWebIndex_t active=AddSubWeb(token.data);
  source.ExpectTokenString("models");source.ExpectTokenString("{");
  while(source.ReadToken(token)&&token!="}"){
    if(token=="(")continue;
    if(token.type!=TT_STRING){source.Error("Unexpected token '%s' in subWeb 'models' block.",token.data);break;}
    if(FindAtomicName(models,token.data)<0)models.Append(idAtomicString(token.data));
  }
  source.ExpectTokenString("props");source.ExpectTokenString("{");
  while(source.ReadToken(token)&&token!="}"){
    if(token=="color")source.Parse1DMatrix(4,&color.x);
    else if(token=="hub")isHub=source.ParseBool();
    else if(token=="visible")isVisible=source.ParseBool();
    else source.Error("Unexpected token '%s' in subWeb 'props' block.",token.data);
  }
  while(source.ReadToken(token)&&token!="}"){
    if(token!="node"){source.Error("Unexpected token '%s' in subWeb 'node' block.",token.data);break;}
    idStaticList<idStr,4> stateNames;
    while(source.ReadToken(token)&&token!="{"){
      if(stateNames.Num()>=4){source.Error("Node contains too many states.");continue;}
      stateNames.Append(idStr(token.data));
    }
    animWebNodeIndex_t nodeIndex;animWebSubWebIndex_t subWebIndex;idDeclAnimWeb::stateList_t stateIndices;
    AddNode(subWebs[active.value].name.str,stateNames,nodeIndex,subWebIndex,stateIndices);
    nodes[nodeIndex.value]->Parse(*this,source);
  }
}
