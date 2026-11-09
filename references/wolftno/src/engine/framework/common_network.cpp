#include "engine/framework/common_local.h"

#include "engine/framework/session.h"
#include "engine/framework/console.h"
#include "engine/framework/debuggraph.h"
#include "engine/models/rendermodelgui.h"
#include "engine/renderer/renderersystem.h"
#include "shared/idlib/color.h"
#include "engine/models/serializer.h"
#include "shared/idlib/containers/staticlist.h"
#include "shared/idlib/csystems/cvarsystemlocal.h"
#include "shared/idlib/networking/bitmsg.h"
#include "shared/idlib/lib.h"
#include "shared/idlib/sys/sys_time.h"

#include <algorithm>
#include <cstring>

namespace {
int FloatBits(float value) { int bits;std::memcpy(&bits,&value,sizeof(bits));return bits; }
float BitsFloat(int bits) { float value;std::memcpy(&value,&bits,sizeof(value));return value; }
constexpr int SESSION_INGAME=7;
}

extern idCVar net_verbose;
extern idCVar com_drawSnapshots;

#if !defined(WOLF_COMMON_NETWORK_CODEC_ONLY)
idCVar net_debug_NetSendUsercmds("net_debug_NetSendUsercmds","0",CVAR_BOOL,"");
idCVar net_debug_snapShotTime("net_debug_snapShotTime","0",CVAR_BOOL|CVAR_ARCHIVE,"");
idCVar net_userCmdRate("net_userCmdRate","4",CVAR_INTEGER,"");
idCVar net_print_ucmds_outgoing("net_print_ucmds_outgoing","0",CVAR_BOOL,"");
idCVar net_print_ucmds_incoming("net_print_ucmds_incoming","0",CVAR_BOOL,"");
idCVar net_ucmd_smoothing_enable("net_ucmd_smoothing_enable","1",CVAR_BOOL,"");

bool idCommonLocal::IsLosingConnectionToHost() {
  return !common->IsServer()&&session->IsLosingConnectionToHost();
}

void idCommonLocal::NetReceiveReliable(int type,idBitMsg& msg,int peer) {
  if(Game()!=nullptr) Game()->NetReceiveReliable(type,msg,peer);
  else if(net_verbose.valueInteger!=0)
    idLib::Printf("Received Game reliable msg before game has been allocated.\n");
}
#endif

void idCommonLocal::WriteUserCmdDelta(idBitMsg& msg,const usercmd_t& cmd,
                                      const usercmd_t* base) {
  if(base!=nullptr) {
    msg.WriteDeltaLongCounter(base->clientGameFrame,cmd.clientGameFrame);
    msg.WriteDelta(base->buttons,cmd.buttons,32);
    msg.WriteDelta(base->forwardmove,cmd.forwardmove,-8);
    msg.WriteDelta(base->rightmove,cmd.rightmove,-8);
    msg.WriteDelta(base->upmove,cmd.upmove,-8);
    msg.WriteDelta(base->forwardmoveSec,cmd.forwardmoveSec,-8);
    msg.WriteDelta(base->rightmoveSec,cmd.rightmoveSec,-8);
    for(int i=0;i<3;++i) msg.WriteDelta(base->angles[i],cmd.angles[i],-16);
    for(int i=0;i<3;++i) msg.WriteDelta(FloatBits((&base->pos.x)[i]),FloatBits((&cmd.pos.x)[i]),32);
    msg.WriteDelta(base->pitch,cmd.pitch,-16);
    msg.WriteDelta(base->yaw,cmd.yaw,-16);
    msg.WriteDelta(base->roll,cmd.roll,-16);
    msg.WriteDelta(base->predictionStateBits,cmd.predictionStateBits,8);
    msg.WriteDelta(base->vehicleThrottle,cmd.vehicleThrottle,8);
    msg.WriteDeltaLongCounter(base->serverGameTime,cmd.serverGameTime);
    msg.WriteDeltaShortCounter(base->fireCount,cmd.fireCount);
    for(int i=0;i<2;++i) msg.WriteDelta(base->fireAngles[i],cmd.fireAngles[i],-16);
    for(int i=0;i<3;++i) msg.WriteDelta(base->firePos[i],cmd.firePos[i],-16);
    msg.WriteDelta(base->speed,cmd.speed,8);
  } else {
    msg.WriteBits(cmd.clientGameFrame,32);
    msg.WriteBits(cmd.buttons,32);
    msg.WriteBits(cmd.forwardmove,-8);msg.WriteBits(cmd.rightmove,-8);
    msg.WriteBits(cmd.upmove,-8);msg.WriteBits(cmd.forwardmoveSec,-8);
    msg.WriteBits(cmd.rightmoveSec,-8);
    for(int i=0;i<3;++i) msg.WriteBits(cmd.angles[i],-16);
    for(int i=0;i<3;++i) msg.WriteBits(FloatBits((&cmd.pos.x)[i]),32);
    msg.WriteBits(cmd.pitch,-16);msg.WriteBits(cmd.yaw,-16);msg.WriteBits(cmd.roll,-16);
    msg.WriteBits(cmd.predictionStateBits,8);msg.WriteBits(cmd.vehicleThrottle,8);
    msg.WriteBits(cmd.serverGameTime,32);msg.WriteBits(cmd.fireCount,-16);
    for(int i=0;i<2;++i) msg.WriteBits(cmd.fireAngles[i],-16);
    for(int i=0;i<3;++i) msg.WriteBits(cmd.firePos[i],-16);
    msg.WriteBits(cmd.speed,8);
  }
  msg.WriteBits(cmd.inhibited,1);
}

void idCommonLocal::ReadUserCmdDelta(const idBitMsg& input,usercmd_t& cmd,const usercmd_t* base) {
  idBitMsg& msg=const_cast<idBitMsg&>(input);
  std::memset(&cmd,0,sizeof(cmd));
  if(base!=nullptr) {
    cmd.clientGameFrame=msg.ReadDeltaLongCounter(base->clientGameFrame);
    cmd.buttons=msg.ReadDelta(base->buttons,32);
    cmd.forwardmove=static_cast<std::int8_t>(msg.ReadDelta(base->forwardmove,-8));
    cmd.rightmove=static_cast<std::int8_t>(msg.ReadDelta(base->rightmove,-8));
    cmd.upmove=static_cast<std::int8_t>(msg.ReadDelta(base->upmove,-8));
    cmd.forwardmoveSec=static_cast<std::int8_t>(msg.ReadDelta(base->forwardmoveSec,-8));
    cmd.rightmoveSec=static_cast<std::int8_t>(msg.ReadDelta(base->rightmoveSec,-8));
    for(int i=0;i<3;++i) cmd.angles[i]=static_cast<std::int16_t>(msg.ReadDelta(base->angles[i],-16));
    for(int i=0;i<3;++i) (&cmd.pos.x)[i]=BitsFloat(msg.ReadDelta(FloatBits((&base->pos.x)[i]),32));
    cmd.pitch=static_cast<std::int16_t>(msg.ReadDelta(base->pitch,-16));
    cmd.yaw=static_cast<std::int16_t>(msg.ReadDelta(base->yaw,-16));
    cmd.roll=static_cast<std::int16_t>(msg.ReadDelta(base->roll,-16));
    cmd.predictionStateBits=static_cast<std::uint8_t>(msg.ReadDelta(base->predictionStateBits,8));
    cmd.vehicleThrottle=static_cast<std::uint8_t>(msg.ReadDelta(base->vehicleThrottle,8));
    cmd.serverGameTime=msg.ReadDeltaLongCounter(base->serverGameTime);
    cmd.fireCount=static_cast<std::uint16_t>(msg.ReadDeltaShortCounter(base->fireCount));
    for(int i=0;i<2;++i) cmd.fireAngles[i]=static_cast<std::int16_t>(msg.ReadDelta(base->fireAngles[i],-16));
    for(int i=0;i<3;++i) cmd.firePos[i]=static_cast<std::int16_t>(msg.ReadDelta(base->firePos[i],-16));
    cmd.speed=static_cast<std::uint8_t>(msg.ReadDelta(base->speed,8));
  } else {
    cmd.clientGameFrame=msg.ReadBits(32);cmd.buttons=msg.ReadBits(32);
    cmd.forwardmove=static_cast<std::int8_t>(msg.ReadBits(-8));
    cmd.rightmove=static_cast<std::int8_t>(msg.ReadBits(-8));
    cmd.upmove=static_cast<std::int8_t>(msg.ReadBits(-8));
    cmd.forwardmoveSec=static_cast<std::int8_t>(msg.ReadBits(-8));
    cmd.rightmoveSec=static_cast<std::int8_t>(msg.ReadBits(-8));
    for(int i=0;i<3;++i) cmd.angles[i]=static_cast<std::int16_t>(msg.ReadBits(-16));
    for(int i=0;i<3;++i) (&cmd.pos.x)[i]=BitsFloat(msg.ReadBits(32));
    cmd.pitch=static_cast<std::int16_t>(msg.ReadBits(-16));
    cmd.yaw=static_cast<std::int16_t>(msg.ReadBits(-16));
    cmd.roll=static_cast<std::int16_t>(msg.ReadBits(-16));
    cmd.predictionStateBits=static_cast<std::uint8_t>(msg.ReadBits(8));
    cmd.vehicleThrottle=static_cast<std::uint8_t>(msg.ReadBits(8));
    cmd.serverGameTime=msg.ReadBits(32);
    cmd.fireCount=static_cast<std::uint16_t>(msg.ReadBits(-16));
    for(int i=0;i<2;++i) cmd.fireAngles[i]=static_cast<std::int16_t>(msg.ReadBits(-16));
    for(int i=0;i<3;++i) cmd.firePos[i]=static_cast<std::int16_t>(msg.ReadBits(-16));
    cmd.speed=static_cast<std::uint8_t>(msg.ReadBits(8));
  }
  cmd.inhibited=msg.ReadBits(1)==1;
}

void idCommonLocal::SerializePlayer(idSerializer& serializer,gameReturnPlayer_t& player) {
  idBitMsg& msg=*serializer.msg;
  if(serializer.writing) msg.WriteBits(player.view.viewID,32);
  else player.view.viewID=msg.ReadBits(32);
  if(serializer.writing) msg.WriteBits(FloatBits(player.view.fov_x),32);
  else player.view.fov_x=BitsFloat(msg.ReadBits(32));
  if(serializer.writing) msg.WriteBits(FloatBits(player.view.fov_y),32);
  else player.view.fov_y=BitsFloat(msg.ReadBits(32));
  const bool absoluteAngles=player.inputSettings.absoluteAngles;
  if(serializer.writing) {
    msg.WriteBits(absoluteAngles,1);
    player.inputSettings.absoluteAngles=absoluteAngles;
  } else {
    player.inputSettings.absoluteAngles=msg.ReadBits(1)==1;
  }
}

#if !defined(WOLF_COMMON_NETWORK_CODEC_ONLY)
void idCommonLocal::NetSendUsercmds() {
  if(session->GetState()!=SESSION_INGAME||!session->GetActingGameStateLobbyBase().IsPeer()) return;
  int& userCmdFrame=Field<int>(0x34C44);
  ++userCmdFrame;
  if(userCmdFrame%session->GetTitleStorageInt("net_userCmdRate",net_userCmdRate.valueInteger)!=0) return;

  std::uint8_t buffer[1024]{};
  idBitMsg msg;
  msg.InitWrite(buffer,sizeof(buffer));
  idUserCmdMgr& manager=UserCmdMgr();
  for(int device=0;device<4;++device) {
    const int player=Game()->GetPlayerIndexFromInputDevice(device);
    if(player==-1) continue;
    usercmd_t fakeCmd;
    const usercmd_t* base=&fakeCmd;
    msg.WriteBits(1,1);
    msg.WriteBits(player,5);
    const int redundancy=std::min(session->GetTitleStorageInt("MAX_REDUNDANT_CMDS",6),6);
    const int writeFrame=manager.writeFrame[player];
    const int first=std::max(writeFrame-redundancy,0);
    int count=writeFrame-first;
    usercmd_t* commands[6]{};
    for(int i=0;i<count;++i) commands[i]=&manager.Command(first+i,player);
    bool smoothing=false;
    if(count>0) Game()->GetPlayerPredictionData(player,commands,count,smoothing);
    msg.WriteBits(smoothing,1);
    msg.WriteBits(count,8);
    if(common->IsClient()&&net_debug_NetSendUsercmds.valueInteger!=0)
      idLib::Printf("Sending commands: %d\n",count);
    int previousFrame=-1;
    for(int i=0;i<count;++i) {
      usercmd_t* command=commands[i];
      if(common->IsClient()&&command->clientGameFrame>10&&previousFrame>=0&&
         command->clientGameFrame!=previousFrame+1)
        idLib::Printf("CLIENT: Sent input out of sequence for user %i ( %i/%i)\n",
                      player,command->clientGameFrame,previousFrame);
      if(common->IsClient()&&net_debug_NetSendUsercmds.valueInteger!=0)
        idLib::Printf("  Sent command for frame: %d \n",command->clientGameFrame);
      if(net_print_ucmds_outgoing.valueInteger!=0) {
        idLib::Printf("%s-------\n",base==&fakeCmd?"*":"-");
        command->Print();
      }
      WriteUserCmdDelta(msg,*command,base);
      base=command;
      previousFrame=command->clientGameFrame;
    }
  }
  msg.WriteBits(0,1);
  session->SendUsercmds(msg);
}

void idCommonLocal::DrawSnapshotMetrics(bool draw) {
  if(session->GetState()!=SESSION_INGAME) return;
  if(common->IsServer()) {
    idLobbyBase& lobby=session->GetActingGameStateLobbyBase();
    if(lobby.HasActivePeers())
      lobby.DrawDebugNetworkHUD_ServerSnapshotMetrics(console->Gui(),draw);
    return;
  }

  idDebugGraph*& bufferedGraph=Field<idDebugGraph*>(0x34C48);
  idDebugGraph*& rateGraph=Field<idDebugGraph*>(0x34C50);
  constexpr float graphStart=600.0f;
  constexpr float graphEnd=-200.0f;
  constexpr float snapRateStart=0.0f;
  constexpr float snapRateEnd=100.0f;
  auto clamp01=[](float value){return std::max(0.0f,std::min(1.0f,value));};
  if(bufferedGraph==nullptr) {
    bufferedGraph=console->CreateGraph(500);
    bufferedGraph->mode=idDebugGraph::GRAPH_LINE;
    bufferedGraph->AddGridLine(clamp01((-100.0f-graphStart)/(graphEnd-graphStart)),
                               idColor::colorYellow);
    bufferedGraph->AddGridLine(clamp01((0.0f-graphStart)/(graphEnd-graphStart)),
                               idColor::colorRed);
    const float width=static_cast<float>(renderSystem->GetWidth());
    bufferedGraph->position={width*0.25f,50.0f,width*0.5f,200.0f};
    rateGraph=console->CreateGraph(500);
    rateGraph->mode=idDebugGraph::GRAPH_LINE;
    rateGraph->AddGridLine(clamp01((16.0f-snapRateStart)/(snapRateEnd-snapRateStart)),
                           idColor::colorYellow);
    rateGraph->position={width*0.25f,300.0f,width*0.5f,200.0f};
  }
  if(!draw) {
    bufferedGraph->enable=false;
    rateGraph->enable=false;
    return;
  }
  bufferedGraph->enable=true;
  rateGraph->enable=false;

  idRenderModelGui* gui=console->Gui();
  const idVec4 background{0.0f,0.0f,0.0f,0.7f};
  const idVec4 white{1.0f,1.0f,1.0f,1.0f};
  gui->DrawFilled(background,10.0f,10.0f,1550.0f,80.0f);
  const int readIndex=Field<int>(0x34730);
  const int writeIndex=Field<int>(0x34734);
  va line("ss: %d/%d.  (%d)",readIndex,writeIndex,readIndex-writeIndex);
  gui->DrawString(20.0f,20.0f,line.buffer,white,false,1.0f);
  line=va("optimalTimeBuffered: %d effectiveSnapRate:  %.2f  totalBufferedTime: %d totalRecvTime: %d optimalPCTBuffer: %.2f",
          static_cast<int>(Field<float>(0x34C3C)),Field<float>(0x34668),
          Field<int>(0x3466C),Field<int>(0x34670),Field<float>(0x34C38));
  gui->DrawString(20.0f,35.0f,line.buffer,white,false,1.0f);
  const int serverTime=ReceivedSnaps()[(writeIndex-1)%16].GetTime();
  const int bufferedTime=static_cast<int>(Field<float>(0x34664));
  line=va("Interpolation time: %d.  Server time: %d.  delta: %d",
          Game()->GetServerGameTimeMs(),serverTime,bufferedTime);
  gui->DrawString(20.0f,50.0f,line.buffer,white,false,1.0f);
  const int currentTime=static_cast<int>(Field<std::uint64_t>(0x750));
  const int snapRate=static_cast<int>(Field<std::uint64_t>(0x728));
  line=va("snapCurrentTime: %d.  snapRate: %d.  delta: %d",
          currentTime,snapRate,currentTime-snapRate);
  gui->DrawString(20.0f,65.0f,line.buffer,white,false,1.0f);

  idColor color=idColor::colorGreen;
  const float optimal=Field<float>(0x34C3C);
  if(optimal>bufferedTime) color=idColor::colorRed;
  else if(bufferedTime>optimal+Field<float>(0x34C40)) color=idColor::colorYellow;
  bufferedGraph->SetValue(-1,clamp01((bufferedTime-graphStart)/(graphEnd-graphStart)),color);
}

void idCommonLocal::NetReceiveUsercmds(idBitMsg& msg) {
  idUserCmdMgr& manager=UserCmdMgr();
  while(msg.ReadBits(1)!=0) {
    const int player=msg.ReadBits(5);
    const bool senderSmoothing=msg.ReadBits(1)==1;
    const int count=static_cast<std::uint8_t>(msg.ReadBits(8));
    if(net_debug_NetSendUsercmds.valueInteger!=0)
      idLib::Printf("Received commands: %d\n",count);
    usercmd_t fakeCmd;
    usercmd_t lastCmd;
    const usercmd_t* deltaBase=&fakeCmd;
    idStaticList<usercmd_t,6> newCommands;
    const int newestFrame=std::max(manager.writeFrame[player]-1,0);
    usercmd_t baseCmd=manager.Command(newestFrame,player);
    int currentFrame=baseCmd.clientGameFrame;
    bool gotNewCommand=false;
    for(int index=0;index<count;++index) {
      usercmd_t command;
      ReadUserCmdDelta(msg,command,deltaBase);
      if(net_print_ucmds_incoming.valueInteger!=0) {
        idLib::Printf("%s--------\n",deltaBase==&fakeCmd?"*":"-");
        command.Print();
      }
      lastCmd=command;
      deltaBase=&lastCmd;
      if(net_debug_NetSendUsercmds.valueInteger!=0)
        idLib::Printf("  Received command for frame: %d \n",command.clientGameFrame);
      if(command.clientGameFrame>currentFrame) {
        if(command.clientGameFrame!=currentFrame+1&&command.clientGameFrame>10&&currentFrame>10)
          idLib::Printf("^6SERVER: Client input frame out of sequence (GAP)  for user: %i, cur: %i, new: %i\n",
                        player,currentFrame,command.clientGameFrame);
        if(net_print_ucmds_incoming.valueInteger!=0)
          idLib::Printf("* %d\n",command.clientGameFrame);
        if(newCommands.Num()<6) {
          newCommands.Append(command);
          gotNewCommand=true;
          currentFrame=command.clientGameFrame;
        }
      }
    }
    if(net_ucmd_smoothing_enable.valueInteger!=0&&senderSmoothing&&newCommands.Num()>1)
      Game()->SmoothIncomingUserCmds(baseCmd,newCommands.list,newCommands.Num());
    for(int index=0;index<newCommands.Num();++index) {
      manager.Command(manager.writeFrame[player],player)=newCommands[index];
      if(manager.writeFrame[player]-manager.readFrame[player]+1>128) {
        manager.readFrame[player]=manager.writeFrame[player]-64;
        idLib::Printf("PutUserCmdForPlayer: buffer overflow.\n");
      }
      ++manager.writeFrame[player];
    }
    if(count>0&&!gotNewCommand&&manager.writeFrame[player]>1) {
      idLib::Printf("\nSERVER: No commands were newer.\n");
      idLib::Printf("usercmdMgr readFrame: %d writeFrame: %d \n",
                    manager.readFrame[player],manager.writeFrame[player]);
      idLib::Printf("HasUserCmdForPlayer: %d \n",
                    manager.readFrame[player]<manager.writeFrame[player]-1);
    }
  }
}

void idCommonLocal::InterpolateSnapshot(gameReturn_t& result,
                                         gameReturn_t& previous,
                                         gameReturn_t& next,float fraction,bool) {
  result.localTime=Game()->GetGameMs(1);
  result.serverTime=static_cast<int>((next.serverTime-previous.serverTime)*fraction+
                                     previous.serverTime);
  Game()->SetServerGameTimeMs(result.serverTime);
  Game()->SetSSTimes(previous.serverTime,next.serverTime);
  const int demoMode=Field<int>(0x6D0);
  void* snapshotFile=Field<void*>(0x6E8);
  if(demoMode!=4||snapshotFile==nullptr) {
    for(int index=0;index<16;++index) {
      gameReturnPlayer_t& output=result.players[index];
      const gameReturnPlayer_t& from=previous.players[index];
      const gameReturnPlayer_t& to=next.players[index];
      output.valid=from.valid&&to.valid;
      if(!output.valid) continue;
      output.gui=nullptr;
      output.inputSettings=to.inputSettings;
      output.view=to.view;
      output.view.fov_x=(to.view.fov_x-from.view.fov_x)*fraction+from.view.fov_x;
      output.view.fov_y=(to.view.fov_y-from.view.fov_y)*fraction+from.view.fov_y;
      output.view.vieworg.x=(to.view.vieworg.x-from.view.vieworg.x)*fraction+from.view.vieworg.x;
      output.view.vieworg.y=(to.view.vieworg.y-from.view.vieworg.y)*fraction+from.view.vieworg.y;
      output.view.vieworg.z=(to.view.vieworg.z-from.view.vieworg.z)*fraction+from.view.vieworg.z;
      idQuat rotation;
      rotation.Slerp(from.view.viewaxis.ToQuat(),to.view.viewaxis.ToQuat(),fraction);
      output.view.viewaxis=rotation.ToMat3();
    }
  }
  Game()->SetInterpolation(fraction,result.serverTime,previous.serverTime,next.serverTime);
  const gameFrameParms_t parms=common->GameFrameParms(1);
  Game()->RunFrame(parms,&result);
}

void idCommonLocal::ProcessSnapshot(idSnapShot& snapshot) {
  static int lastReceivedLocalTime=0;
  const int localTime=static_cast<int>(Sys_Milliseconds());
  gameReturn_t& current=SnapCurrent();
  gameReturn_t& previous=SnapPrevious();
  previous=current;
  const int priorServerTime=previous.serverTime;
  current.serverTime=snapshot.GetTime();
  Field<std::uint64_t>(0x728)=static_cast<std::uint64_t>(current.serverTime-priorServerTime);
  if(net_debug_snapShotTime.valueInteger!=0)
    idLib::Printf("^2ProcessSnapshot. delta serverTime: %d  delta localTime: %d \n",
                  current.serverTime-priorServerTime,localTime-lastReceivedLocalTime);
  lastReceivedLocalTime=localTime;
  console->ClearSnapshots();
  for(int index=0;index<16;++index) {
    idBitMsg msg;
    const int objectIndex=snapshot.FindObjectIndexByID(index);
    if(static_cast<unsigned int>(snapshot.GetObjectMsgByIndex(objectIndex,msg,false))==
       static_cast<unsigned int>(index)) {
      if(msg.GetSize()!=0) {
        idSerializer serializer;
        serializer.writing=false;
        serializer.msg=&msg;
        SerializePlayer(serializer,current.players[index]);
        current.players[index].valid=true;
        if(com_drawSnapshots.valueInteger==3)
          console->AddSnapObject("players",msg.GetSize(),
                                 snapshot.CompareObject(&OldSnapshot(),index,0,0,0),0.0f);
      } else {
        current.players[index].valid=false;
      }
    }
  }
  const int serverTime=Game()->GetServerGameTimeMs();
  Game()->SetServerGameTimeMs(current.serverTime);
  Game()->ReadFromSnapshot(snapshot,&OldSnapshot());
  Game()->GetRenderWorld()->ReadFromSnapshot(snapshot,&OldSnapshot());
  Game()->GetSoundWorld()->ReadFromSnapshot(snapshot,&OldSnapshot());
  Game()->SetServerGameTimeMs(serverTime);
  Field<std::uint64_t>(0x740)=static_cast<std::uint64_t>(snapshot.GetRecvTime()-OldSnapshot().GetRecvTime());
  OldSnapshot()=snapshot;
}

void idCommonLocal::ProcessNextSnapshot() {
  int& readIndex=Field<int>(0x34730);
  const int writeIndex=Field<int>(0x34734);
  if(readIndex==writeIndex) {
    idLib::Printf("No snapshots to process.\n");
    return;
  }
  ProcessSnapshot(ReceivedSnaps()[readIndex%16]);
  ++readIndex;
}

void idCommonLocal::NetReceiveSnapshot(idSnapShot& snapshot) {
  snapshot.SetRecvTime(static_cast<int>(Sys_Milliseconds()));
  int& readIndex=Field<int>(0x34730);
  int& writeIndex=Field<int>(0x34734);
  if(writeIndex-readIndex>=16) {
    idLib::Printf("Overwritting oldest snapshot %d with new snapshot %d\n",readIndex,writeIndex);
    ProcessNextSnapshot();
  }
  ReceivedSnaps()[writeIndex%16]=snapshot;
  const int oldReadIndex=readIndex;
  ++writeIndex;
  if(oldReadIndex<2) ProcessNextSnapshot();
}
#endif
