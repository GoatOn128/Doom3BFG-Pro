#include "tungsten/game/components/timelinecomponent.h"
#include <algorithm>
// DIA: idComponentTimeLine::IsEventValidForEntity
// DIA: idComponentTimeLine::GetRelativeTime
// DIA: idComponentTimeLine::GetComponentTime
// DIA: idAIAction::GetActionEventName
// DIA: idSort<idComponentTimeLine::idTimeLineEvent>::`scalar deleting destructor'
// DIA: idComponentTimeLine::IsTimelineComplete
// DIA: idComponentActionScript::IsEventValidForEntity
// DIA: idComponentTimeLine::idTimeLineEvent::idTimeLineEvent
// DIA: idSort_TimeLineEvent::~idSort_TimeLineEvent
// DIA: idComponentTimeLine::Update
// DIA: idComponentTimeLine::ProcessTimelineEvent
// DIA: `idComponentTimeLine::ProcessTimelineEvent'::`1'::dtor$0
// DIA: `idComponentTimeLine::ProcessTimelineEvent'::`1'::dtor$1
// DIA: `idComponentTimeLine::ProcessTimelineEvent'::`1'::dtor$3
// DIA: `idComponentTimeLine::ProcessTimelineEvent'::`1'::dtor$4
// DIA: idComponentActionScript::GetRelativeTime
// DIA: idComponentActionScript::ResetTimeline
// DIA: idComponentActionScript::ProcessTimelineEvent
// DIA: idComponentActionScript::IsTimelineComplete
// DIA: idComponentActionScript::OnTimelineCompletion
// DIA: SwapValues<idComponentTimeLine::idTimeLineEvent>
// DIA: `SwapValues<idComponentTimeLine::idTimeLineEvent>'::`1'::dtor$0
// DIA: idComponentTimeLine::Start
// DIA: idComponentActionScript::GetTimelineIndex
// DIA: idSort_Quick<idComponentTimeLine::idTimeLineEvent,idSort_TimeLineEvent>::Sort
// DIA: idList<idComponentTimeLine::idTimeLineEvent,5>::Resize
// DIA: idComponentActionScript::RunningActionScript
// DIA: idComponentActionScript::ClearActionScript
// DIA: idComponentActionScript::NumScriptActions
// DIA: idComponentTimeLine::idTimeLineEntity::idTimeLineEntity
// DIA: idComponentTimeLine::Init
// DIA: `idComponentTimeLine::Init'::`1'::dtor$0
// DIA: idList<idComponentTimeLine::idTimeLineEvent,5>::Alloc
// DIA: idList<idComponentTimeLine::idTimeLineEntity,5>::Resize
// DIA: idList<idComponentTimeLine::idTimeLineEntity,5>::~idList<idComponentTimeLine::idTimeLineEntity,5>
// DIA: idComponentTimeLine::idComponentTimeLine
// DIA: `idComponentTimeLine::idComponentTimeLine'::`1'::dtor$0
// DIA: `idComponentTimeLine::idComponentTimeLine'::`1'::dtor$1
// DIA: idComponentTimeLine::~idComponentTimeLine
// DIA: idComponentActionScript::idComponentActionScript
// DIA: idComponentActionScript::~idComponentActionScript
// DIA: idList<idComponentTimeLine::idTimeLineEntity,5>::Alloc
// DIA: idComponentTimeLine::`scalar deleting destructor'
// DIA: idComponentActionScript::`scalar deleting destructor'
// DIA: idComponentActionScript::SetActionScript
// DIA: `idComponentActionScript::SetActionScript'::`1'::dtor$0
// DIA: `idComponentActionScript::SetActionScript'::`1'::dtor$1
// DIA: `idComponentActionScript::SetActionScript'::`1'::dtor$2
// DIA: `idComponentActionScript::SetActionScript'::`1'::dtor$3
// DIA: `idComponentActionScript::SetActionScript'::`1'::dtor$4
// DIA: `idComponentActionScript::SetActionScript'::`1'::dtor$5
// DIA: `idComponentActionScript::SetActionScript'::`1'::dtor$6
// DIA: `idComponentActionScript::SetActionScript'::`1'::dtor$7
// DIA: `idComponentActionScript::SetActionScript'::`1'::dtor$8
// DIA: `idComponentActionScript::SetActionScript'::`1'::dtor$9
// DIA: `idComponentActionScript::SetActionScript'::`1'::dtor$10
// DIA: `idComponentActionScript::SetActionScript'::`1'::dtor$11
// DIA: `idComponentActionScript::SetActionScript'::`1'::dtor$12
// DIA: `idComponentActionScript::SetActionScript'::`1'::dtor$13
// DIA: `idComponentActionScript::SetActionScript'::`1'::dtor$14
// DIA: `idComponentActionScript::SetActionScript'::`1'::dtor$15
// DIA: `idComponentActionScript::SetActionScript'::`1'::dtor$16
// DIA: `idComponentActionScript::SetActionScript'::`1'::dtor$17
// DIA: `idComponentActionScript::SetActionScript'::`1'::dtor$18
// DIA: `idComponentActionScript::SetActionScript'::`1'::dtor$19
// DIA: `idComponentActionScript::SetActionScript'::`1'::dtor$20
// DIA: `idComponentActionScript::SetActionScript'::`1'::dtor$21
// DIA: `idComponentActionScript::SetActionScript'::`1'::dtor$22
// DIA: `idComponentActionScript::SetActionScript'::`1'::dtor$23
// DIA: `idComponentActionScript::SetActionScript'::`1'::dtor$24
// DIA: `idComponentActionScript::SetActionScript'::`1'::dtor$25
// DIA: `idComponentActionScript::SetActionScript'::`1'::dtor$26
// DIA: `idComponentActionScript::SetActionScript'::`1'::dtor$27
// DIA: `idComponentActionScript::SetActionScript'::`1'::dtor$28
// DIA: `idComponentActionScript::SetActionScript'::`1'::dtor$29
// DIA: `idComponentActionScript::SetActionScript'::`1'::dtor$30
// DIA: `idComponentActionScript::SetActionScript'::`1'::dtor$31
// DIA: `idComponentActionScript::SetActionScript'::`1'::dtor$32
// DIA: `idComponentActionScript::SetActionScript'::`1'::dtor$33
// DIA: `idComponentActionScript::SetActionScript'::`1'::dtor$34
// DIA: `idComponentActionScript::SetActionScript'::`1'::dtor$35
// DIA: `idComponentActionScript::SetActionScript'::`1'::dtor$36
// DIA: `idComponentActionScript::SetActionScript'::`1'::dtor$37
// DIA: `idComponentActionScript::SetActionScript'::`1'::dtor$38
// DIA: `idComponentActionScript::SetActionScript'::`1'::dtor$42
// DIA: `idComponentActionScript::SetActionScript'::`1'::dtor$39
// DIA: `idComponentActionScript::SetActionScript'::`1'::dtor$46
class idAIAction{public:static std::string GetActionEventName(const std::string&name);};std::string idAIAction::GetActionEventName(const std::string&name){return name+"Enter";}
idComponentTimeLine::idTimeLineEvent::idTimeLineEvent()=default;idComponentTimeLine::idTimeLineEntity::idTimeLineEntity()=default;idSort_TimeLineEvent::~idSort_TimeLineEvent()=default;idComponentTimeLine::idComponentTimeLine()=default;idComponentTimeLine::~idComponentTimeLine()=default;
bool idComponentTimeLine::IsEventValidForEntity(idEntity*entity,const idTimeLineEvent&event)const{return entity!=nullptr&&event.enabled&&!event.name.empty();}int idComponentTimeLine::GetRelativeTime(int timelineIndex)const{(void)timelineIndex;return startTime<0?-1:currentTime-startTime;}int idComponentTimeLine::GetComponentTime()const{return startTime<0?-1:currentTime-startTime;}bool idComponentTimeLine::IsTimelineComplete(int index)const{return index<0||index>=entityEvents.Num()||nextEventIndices[index]>=entityEvents[index].events.Num();}
void idComponentTimeLine::Update(int now){if(startTime<0)return;currentTime=std::max(now,startTime);bool all=true;for(int i=0;i<entityEvents.Num();++i){auto&line=entityEvents[i];while(nextEventIndices[i]<line.events.Num()){const auto&event=line.events[nextEventIndices[i]];if(event.eventTime>GetRelativeTime(i))break;if(IsEventValidForEntity(line.entity,event))ProcessTimelineEvent(line.entity,event);++nextEventIndices[i];}if(IsTimelineComplete(i))OnTimelineCompletion(i);else all=false;}if(all)startTime=-1;}
void idComponentTimeLine::ProcessTimelineEvent(idEntity*entity,const idTimeLineEvent&event){if(event.callback)event.callback(entity);}void idComponentTimeLine::ResetTimeline(int index){if(index>=0&&index<nextEventIndices.Num())nextEventIndices[index]=0;}void idComponentTimeLine::OnTimelineCompletion(int timelineIndex){(void)timelineIndex;++completionCount;}void idComponentTimeLine::Start(idEntity*activator,int now){activatingEntity=activator;startTime=now;currentTime=now;completionCount=0;nextEventIndices.SetNum(entityEvents.Num(),0);for(int i=0;i<entityEvents.Num();++i)ResetTimeline(i);Update(now);}void idComponentTimeLine::Init(){startTime=-1;nextEventIndices.SetNum(entityEvents.Num(),0);idSort_Quick<idTimeLineEvent,idSort_TimeLineEvent>sorter;for(int i=0;i<entityEvents.Num();++i)sorter.Sort(entityEvents[i].events);}
int idComponentTimeLine::AddEvent(idEntity*entity,int time,std::string name,std::function<void(idEntity*)>callback){int index=-1;for(int i=0;i<entityEvents.Num();++i)if(entityEvents[i].entity==entity)index=i;if(index<0){auto*line=entityEvents.Alloc();line->entity=entity;index=entityEvents.Num()-1;}entityEvents[index].events.Append({std::max(0,time),std::move(name),std::move(callback)});return index;}
idComponentActionScript::idComponentActionScript()=default;idComponentActionScript::~idComponentActionScript()=default;bool idComponentActionScript::IsEventValidForEntity(idEntity*entity,const idTimeLineEvent&event)const{return idComponentTimeLine::IsEventValidForEntity(entity,event)&&event.name.rfind("action:",0)==0;}int idComponentActionScript::GetRelativeTime(int index)const{return IsTimelineComplete(index)?-1:idComponentTimeLine::GetRelativeTime(index);}void idComponentActionScript::ResetTimeline(int index){idComponentTimeLine::ResetTimeline(index);actionComplete=false;}void idComponentActionScript::ProcessTimelineEvent(idEntity*entity,const idTimeLineEvent&event){idComponentTimeLine::ProcessTimelineEvent(entity,event);}bool idComponentActionScript::IsTimelineComplete(int index)const{return idComponentTimeLine::IsTimelineComplete(index);}void idComponentActionScript::OnTimelineCompletion(int index){if(!actionComplete){actionComplete=true;idComponentTimeLine::OnTimelineCompletion(index);}}int idComponentActionScript::GetTimelineIndex(idEntity*entity)const{for(int i=0;i<entityEvents.Num();++i)if(entityEvents[i].entity==entity)return i;return -1;}bool idComponentActionScript::RunningActionScript()const{return IsRunning()&&!actionComplete;}void idComponentActionScript::ClearActionScript(){entityEvents.Clear();nextEventIndices.Clear();startTime=-1;actionComplete=false;scriptExecutor=nullptr;}int idComponentActionScript::NumScriptActions()const{int count=0;for(int i=0;i<entityEvents.Num();++i)count+=entityEvents[i].events.Num();return count;}void idComponentActionScript::SetActionScript(idEntity*entity,const std::vector<std::pair<int,std::string>>&script,idEntity*executor,idEntity*activator,bool tool){ClearActionScript();scriptExecutor=executor;toolMode=tool;for(const auto&entry:script)AddEvent(entity,entry.first,"action:"+idAIAction::GetActionEventName(entry.second));Init();Start(activator,0);}
template class idSort<idComponentTimeLine::idTimeLineEvent>;
template void SwapValues<idComponentTimeLine::idTimeLineEvent>(idComponentTimeLine::idTimeLineEvent&,idComponentTimeLine::idTimeLineEvent&);
template class idSort_Quick<idComponentTimeLine::idTimeLineEvent,idSort_TimeLineEvent>;
template class idList<idComponentTimeLine::idTimeLineEvent,5>;
template class idList<idComponentTimeLine::idTimeLineEntity,5>;

