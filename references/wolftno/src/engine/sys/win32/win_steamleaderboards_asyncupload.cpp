#include "engine/sys/win32/win_steamleaderboards.h"

#include <cstring>

idAsyncLeaderboardUpload::idAsyncLeaderboardUpload() = default;
void idAsyncLeaderboardUpload::AddAttachement(const void* data, int size) {
  attachment.resize(size > 0 ? size : 0); if (data != nullptr && size > 0) std::memcpy(attachment.data(), data, size);
}
bool idAsyncLeaderboardUpload::IsFinished() const { return state == STATE_DONE || state == STATE_FAILED; }
void idAsyncLeaderboardUpload::Pump() {
  switch (state) {
    case STATE_IDLE: Begin(); break; case STATE_SHARING: UploadLeaderboardAttachment(); break;
    case STATE_FINDING: BeginFindLeaderboard(); break; case STATE_DOWNLOADING: BeginDownload(); break;
    case STATE_UPLOADING: BeginUpload(); break; default: break;
  }
}
std::int64_t idAsyncLeaderboardUpload::AggregateColumn(aggregationMethod_t method,
    std::int64_t oldValue, std::int64_t newValue) {
  switch (method) { case AGGREGATE_MIN: return (std::min)(oldValue,newValue);
    case AGGREGATE_MAX: return (std::max)(oldValue,newValue); case AGGREGATE_SUM: return oldValue+newValue;
    case AGGREGATE_LAST: default: return newValue; }
}
void idAsyncLeaderboardUpload::OnAttachmentSharedFail() { state = STATE_FAILED; }
void idAsyncLeaderboardUpload::OnScoresDownloadedFail() { state = STATE_FAILED; }
void idAsyncLeaderboardUpload::AggregateDownloadResult(const std::vector<steamLeaderboardEntry_t>& entries) {
  if (entries.empty()) return; const auto& current = entries.front();
  columns[0].value = AggregateColumn(definition.columnDefs != nullptr ? definition.columnDefs[0].aggregationMethod : AGGREGATE_LAST,
                                     current.score, columns[0].value);
  for (int i=1;i<definition.numColumns && i<16 && i-1<current.detailCount;++i)
    columns[i].value = AggregateColumn(definition.columnDefs[i].aggregationMethod, current.details[i-1], columns[i].value);
}
void idAsyncLeaderboardUpload::OnAttachmentUploaded(bool success) { state = success ? STATE_FINDING : STATE_FAILED; }
void idAsyncLeaderboardUpload::OnAttachmentSharedSuccess(std::uint64_t handle) { attachmentHandle = handle; OnAttachmentUploaded(handle != 0); }
bool idAsyncLeaderboardUpload::VerifyAndSetDefAndColumns(const leaderboardDefinition_t* def, const column_t* values) {
  if (def == nullptr || values == nullptr || def->numColumns <= 0 || def->numColumns > 16) return false;
  definition = *def; for (int i=0;i<def->numColumns;++i) columns[i]=values[i]; leaderboardName.Format("%i", def->id+1); return true;
}
void idAsyncLeaderboardUpload::OnAttachmentShared(std::uint64_t handle) { handle != 0 ? OnAttachmentSharedSuccess(handle) : OnAttachmentSharedFail(); }
void idAsyncLeaderboardUpload::UploadLeaderboardAttachment() {
  auto* platform=Win_GetSteamLeaderboardPlatform(); OnAttachmentShared(platform!=nullptr ? platform->ShareAttachment(attachment.data(),static_cast<int>(attachment.size())):0);
}
idAsyncLeaderboardUpload* idAsyncLeaderboardUpload::CreateWithCopiedDefAndColumns(
    int op,const leaderboardDefinition_t* def,const column_t* values) {
  auto* result=new idAsyncLeaderboardUpload(); result->operationId=op;
  if(!result->VerifyAndSetDefAndColumns(def,values)){delete result;return nullptr;} return result;
}
void idAsyncLeaderboardUpload::OnScoreUploaded(bool success) { state=success?STATE_DONE:STATE_FAILED; }
void idAsyncLeaderboardUpload::BeginUpload() {
  auto* platform=Win_GetSteamLeaderboardPlatform(); int details[15]{};
  for(int i=1;i<definition.numColumns&&i<16;++i) details[i-1]=static_cast<int>(columns[i].value);
  OnScoreUploaded(platform!=nullptr && platform->Upload(leaderboardId,static_cast<int>(columns[0].value),details,
      (std::max)(0,definition.numColumns-1),definition.rankOrder==RANK_GREATEST_FIRST,attachmentHandle));
}
void idAsyncLeaderboardUpload::OnScoresDownloaded(const std::vector<steamLeaderboardEntry_t>& entries) { AggregateDownloadResult(entries); state=STATE_UPLOADING; }
void idAsyncLeaderboardUpload::BeginDownload() {
  auto* platform=Win_GetSteamLeaderboardPlatform(); if(platform==nullptr){OnScoresDownloadedFail();return;}
  OnScoresDownloaded(platform->Download(leaderboardId,1,0,1));
}
void idAsyncLeaderboardUpload::OnFindResultSuccess(std::uint64_t handle) { leaderboardId=handle; state=definition.checkAgainstCurrent?STATE_DOWNLOADING:STATE_UPLOADING; }
void idAsyncLeaderboardUpload::OnFindResult(std::uint64_t handle) { handle!=0?OnFindResultSuccess(handle):state=STATE_FAILED; }
void idAsyncLeaderboardUpload::BeginFindLeaderboard() {
  auto* platform=Win_GetSteamLeaderboardPlatform(); OnFindResult(platform!=nullptr?platform->FindLeaderboard(leaderboardName.data,true,definition.rankOrder):0);
}
void idAsyncLeaderboardUpload::Begin() { state=!attachment.empty()&&definition.supportsAttachments?STATE_SHARING:STATE_FINDING; }

