#include "engine/sys/win32/win_steamleaderboards.h"

#include <cstdio>

idLeaderboardCallback::row_t& idLeaderboardCallback::row_t::operator=(const row_t& other) = default;

idLeaderboardCallback::row_t ConvertEntryToRow(const steamLeaderboardEntry_t& entry) {
  idLeaderboardCallback::row_t row;
  row.hasAttachment = entry.attachment != 0;
  row.attachmentID = static_cast<std::int64_t>(entry.attachment);
  row.name = entry.name;
  row.rank = entry.globalRank;
  row.userId = entry.userId;
  row.columns[0] = entry.score;
  for (int index = 0; index < entry.detailCount && index + 1 < 16; ++index)
    row.columns[index + 1] = entry.details[index];
  return row;
}

idAsyncLeaderboardDownload::idAsyncLeaderboardDownload() = default;
idAsyncLeaderboardDownload::~idAsyncLeaderboardDownload() = default;
void idAsyncLeaderboardDownload::Pump() {
  if (state == STATE_IDLE) Begin();
  else if (state == STATE_FINDING) BeginFindLeaderboard();
  else if (state == STATE_DOWNLOADING) BeginDownload();
}
bool idAsyncLeaderboardDownload::IsFinished() const { return state == STATE_DONE || state == STATE_FAILED; }
void idAsyncLeaderboardDownload::SetCallback(const idLeaderboardCallback* value) {
  callback.reset(value != nullptr ? static_cast<idLeaderboardCallback*>(value->Clone()) : nullptr);
}
idAsyncLeaderboardDownload* idAsyncLeaderboardDownload::CreateWithCopiedDefAndCallbackAndRange(
    int op, const leaderboardDefinition_t* def, int start, int rows,
    const idLeaderboardCallback* callbackValue) {
  if (def == nullptr || callbackValue == nullptr || rows <= 0) return nullptr;
  auto* result = new idAsyncLeaderboardDownload(); result->operationId = op;
  result->definition = *def; result->startingRank = start; result->numRows = rows;
  result->SetCallback(callbackValue); result->leaderboardName.Format("%i", def->id + 1); return result;
}
void idAsyncLeaderboardDownload::RunFailCallbackAndChangeStateToFailed(leaderboardError_t error) {
  state = STATE_FAILED; if (callback != nullptr) { callback->errorCode = error; callback->Call(); }
}
void idAsyncLeaderboardDownload::RunCallbackWithResults(
    const std::vector<steamLeaderboardEntry_t>& entries) {
  if (callback == nullptr) { state = STATE_DONE; return; }
  callback->def = &definition; callback->startIndex = startingRank; callback->rows.clear();
  callback->numRowsInLeaderboard = static_cast<int>(entries.size()); callback->localIndex = -1;
  for (const auto& entry : entries) {
    callback->rows.push_back(ConvertEntryToRow(entry));
  }
  callback->errorCode = LEADERBOARD_ERROR_NONE; callback->Call(); state = STATE_DONE;
}
void idAsyncLeaderboardDownload::OnScoresDownloadedFail() { RunFailCallbackAndChangeStateToFailed(LEADERBOARD_ERROR_DOWNLOAD); }
void idAsyncLeaderboardDownload::OnScoresDownloaded(const std::vector<steamLeaderboardEntry_t>& entries) { RunCallbackWithResults(entries); }
void idAsyncLeaderboardDownload::BeginDownload() {
  auto* platform = Win_GetSteamLeaderboardPlatform();
  if (platform == nullptr) { OnScoresDownloadedFail(); return; }
  OnScoresDownloaded(platform->Download(leaderboardId, 0, startingRank, numRows));
}
void idAsyncLeaderboardDownload::OnFindResult(std::uint64_t handle) {
  if (handle == 0) { RunFailCallbackAndChangeStateToFailed(LEADERBOARD_ERROR_NOT_FOUND); return; }
  leaderboardId = handle; state = STATE_DOWNLOADING;
}
void idAsyncLeaderboardDownload::BeginFindLeaderboard() {
  auto* platform = Win_GetSteamLeaderboardPlatform();
  OnFindResult(platform != nullptr ? platform->FindLeaderboard(leaderboardName.data, false, definition.rankOrder) : 0);
}
void idAsyncLeaderboardDownload::Begin() { state = STATE_FINDING; }
