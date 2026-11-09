#include "engine/sys/win32/win_steamleaderboards.h"

#include <algorithm>

namespace { idSteamLeaderboardPlatform* leaderboardPlatform=nullptr; }
void Win_SetSteamLeaderboardPlatform(idSteamLeaderboardPlatform* platform){leaderboardPlatform=platform;}
idSteamLeaderboardPlatform* Win_GetSteamLeaderboardPlatform(){return leaderboardPlatform;}
idSteamLeaderboards::idSteamLeaderboards()=default;
idSteamLeaderboards::~idSteamLeaderboards()=default;
void idSteamLeaderboards::PumpStateSteamUnavailable(){if(leaderboardPlatform!=nullptr&&leaderboardPlatform->IsAvailable())state=STATE_IDLE;}
void idSteamLeaderboards::BeginDownload(int op,const leaderboardDefinition_t* def,int start,int rows,const idLeaderboardCallback* cb){
  if(download!=nullptr)return; download.reset(idAsyncLeaderboardDownload::CreateWithCopiedDefAndCallbackAndRange(op,def,start,rows,cb));
  if(download!=nullptr){download->Begin();state=STATE_WAIT_FOR_DOWNLOAD;}
}
void idSteamLeaderboards::OnAttachmentDownloaded(bool,const std::vector<std::uint8_t>&){}
bool idSteamLeaderboards::DownloadAttachment(std::uint64_t handle,std::vector<std::uint8_t>& output){return leaderboardPlatform!=nullptr&&leaderboardPlatform->DownloadAttachment(handle,output);}
void idSteamLeaderboards::PumpStateWaitForDownload(){if(download==nullptr){state=STATE_IDLE;return;}download->Pump();if(download->IsFinished()){download.reset();state=STATE_IDLE;}}
bool idSteamLeaderboards::QueueUpload(int op,const leaderboardDefinition_t* def,const column_t* columns,const void* attachment,int attachmentSize){
  if(asyncUploads.size()>=8)return false; std::unique_ptr<idAsyncLeaderboardUpload> upload(idAsyncLeaderboardUpload::CreateWithCopiedDefAndColumns(op,def,columns));
  if(upload==nullptr)return false;if(attachment!=nullptr&&attachmentSize>0)upload->AddAttachement(attachment,attachmentSize);upload->Begin();asyncUploads.push_back(std::move(upload));state=STATE_WAIT_FOR_UPLOAD;return true;
}
void idSteamLeaderboards::PumpStateWaitForUpload(){for(auto& upload:asyncUploads)upload->Pump();asyncUploads.erase(std::remove_if(asyncUploads.begin(),asyncUploads.end(),[](const auto& p){return p->IsFinished();}),asyncUploads.end());if(asyncUploads.empty())state=STATE_IDLE;}
void idSteamLeaderboards::Pump(){if(leaderboardPlatform==nullptr||!leaderboardPlatform->IsAvailable()){state=STATE_STEAM_UNAVAILABLE;}switch(state){case STATE_STEAM_UNAVAILABLE:PumpStateSteamUnavailable();break;case STATE_WAIT_FOR_DOWNLOAD:PumpStateWaitForDownload();break;case STATE_WAIT_FOR_UPLOAD:PumpStateWaitForUpload();break;default:break;}}

