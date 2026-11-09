#pragma once

#include "engine/sys/sys_lobby_types.h"
#include "shared/idlib/text/str.h"

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

enum aggregationMethod_t { AGGREGATE_MIN, AGGREGATE_MAX, AGGREGATE_SUM, AGGREGATE_LAST };
enum statsColumnDisplayType_t {
  STATS_COLUMN_DISPLAY_NUMBER, STATS_COLUMN_DISPLAY_TIME_MILLISECONDS,
  STATS_COLUMN_DISPLAY_CASH, STATS_COLUMN_NEVER_DISPLAY
};
enum rankOrder_t { RANK_GREATEST_FIRST, RANK_LEAST_FIRST };
enum leaderboardError_t {
  LEADERBOARD_ERROR_NONE, LEADERBOARD_ERROR_FAILED, LEADERBOARD_ERROR_NOT_ONLINE,
  LEADERBOARD_ERROR_BUSY, LEADERBOARD_ERROR_INVALID_USER, LEADERBOARD_ERROR_INVALID_REQUEST,
  LEADERBOARD_ERROR_DOWNLOAD, LEADERBOARD_ERROR_NOT_FOUND
};

struct columnDef_t {
  const char* locDisplayName{};
  int bits{};
  aggregationMethod_t aggregationMethod{AGGREGATE_LAST};
  statsColumnDisplayType_t displayType{STATS_COLUMN_DISPLAY_NUMBER};
};
struct column_t { std::int64_t value{}; };
struct leaderboardDefinition_t {
  int id{};
  int numColumns{};
  const columnDef_t* columnDefs{};
  rankOrder_t rankOrder{RANK_GREATEST_FIRST};
  bool supportsAttachments{};
  bool checkAgainstCurrent{};
};

class idLeaderboardCallback : public idCallback {
public:
  struct row_t {
    row_t() = default;
    row_t& operator=(const row_t& other);
    bool hasAttachment{};
    std::int64_t attachmentID{};
    idStr name;
    std::int64_t rank{};
    std::array<std::int64_t, 16> columns{};
    std::uint64_t userId{};
  };
  const leaderboardDefinition_t* def{};
  int startIndex{};
  int localIndex{-1};
  std::vector<row_t> rows;
  int numRowsInLeaderboard{};
  leaderboardError_t errorCode{LEADERBOARD_ERROR_NONE};
};

struct steamLeaderboardEntry_t {
  std::uint64_t userId{};
  idStr name;
  int globalRank{};
  int score{};
  std::array<int, 16> details{};
  int detailCount{};
  std::uint64_t attachment{};
};

class idSteamLeaderboardPlatform {
public:
  virtual ~idSteamLeaderboardPlatform() = default;
  virtual bool IsAvailable() const = 0;
  virtual std::uint64_t FindLeaderboard(const char* name, bool create,
                                        rankOrder_t order) = 0;
  virtual bool Upload(std::uint64_t leaderboard, int score, const int* details,
                      int detailCount, bool keepBest, std::uint64_t attachment) = 0;
  virtual std::vector<steamLeaderboardEntry_t> Download(std::uint64_t leaderboard,
                                                        int requestType, int start, int count) = 0;
  virtual std::uint64_t ShareAttachment(const void* data, int size) = 0;
  virtual bool DownloadAttachment(std::uint64_t handle, std::vector<std::uint8_t>& data) = 0;
};

void Win_SetSteamLeaderboardPlatform(idSteamLeaderboardPlatform* platform);

class idAsyncLeaderboardDownload {
public:
  enum state_t { STATE_IDLE, STATE_FINDING, STATE_DOWNLOADING, STATE_DONE, STATE_FAILED };
  idAsyncLeaderboardDownload();
  ~idAsyncLeaderboardDownload();
  void Pump();
  bool IsFinished() const;
  void SetCallback(const idLeaderboardCallback* callbackValue);
  static idAsyncLeaderboardDownload* CreateWithCopiedDefAndCallbackAndRange(
      int operationId, const leaderboardDefinition_t* definition, int start, int rows,
      const idLeaderboardCallback* callbackValue);
  void RunFailCallbackAndChangeStateToFailed(leaderboardError_t error);
  void RunCallbackWithResults(const std::vector<steamLeaderboardEntry_t>& entries);
  void OnScoresDownloadedFail();
  void OnScoresDownloaded(const std::vector<steamLeaderboardEntry_t>& entries);
  void BeginDownload();
  void OnFindResult(std::uint64_t handle);
  void BeginFindLeaderboard();
  void Begin();

private:
  state_t state{STATE_IDLE};
  idStr leaderboardName;
  leaderboardDefinition_t definition{};
  std::uint64_t leaderboardId{};
  int operationId{};
  int startingRank{};
  int numRows{};
  std::unique_ptr<idLeaderboardCallback> callback;
};

class idAsyncLeaderboardUpload {
public:
  enum state_t { STATE_IDLE, STATE_SHARING, STATE_FINDING, STATE_DOWNLOADING,
                 STATE_UPLOADING, STATE_DONE, STATE_FAILED };
  idAsyncLeaderboardUpload();
  ~idAsyncLeaderboardUpload() = default;
  void AddAttachement(const void* data, int size);
  bool IsFinished() const;
  void Pump();
  static std::int64_t AggregateColumn(aggregationMethod_t method, std::int64_t oldValue,
                                      std::int64_t newValue);
  void OnAttachmentSharedFail();
  void OnScoresDownloadedFail();
  void AggregateDownloadResult(const std::vector<steamLeaderboardEntry_t>& entries);
  void OnAttachmentUploaded(bool success);
  void OnAttachmentSharedSuccess(std::uint64_t handle);
  bool VerifyAndSetDefAndColumns(const leaderboardDefinition_t* definition,
                                const column_t* columns);
  void OnAttachmentShared(std::uint64_t handle);
  void UploadLeaderboardAttachment();
  static idAsyncLeaderboardUpload* CreateWithCopiedDefAndColumns(
      int operationId, const leaderboardDefinition_t* definition, const column_t* columns);
  void OnScoreUploaded(bool success);
  void BeginUpload();
  void OnScoresDownloaded(const std::vector<steamLeaderboardEntry_t>& entries);
  void BeginDownload();
  void OnFindResultSuccess(std::uint64_t handle);
  void OnFindResult(std::uint64_t handle);
  void BeginFindLeaderboard();
  void Begin();

private:
  state_t state{STATE_IDLE};
  leaderboardDefinition_t definition{};
  std::array<column_t, 16> columns{};
  idStr leaderboardName;
  std::uint64_t leaderboardId{};
  int operationId{};
  std::vector<std::uint8_t> attachment;
  std::uint64_t attachmentHandle{};
};

class idSteamLeaderboards {
public:
  enum state_t { STATE_IDLE, STATE_WAIT_FOR_UPLOAD, STATE_WAIT_FOR_DOWNLOAD,
                 STATE_STEAM_UNAVAILABLE };
  idSteamLeaderboards();
  ~idSteamLeaderboards();
  void PumpStateSteamUnavailable();
  void BeginDownload(int operationId, const leaderboardDefinition_t* definition,
                     int start, int rows, const idLeaderboardCallback* callback);
  void OnAttachmentDownloaded(bool success, const std::vector<std::uint8_t>& data);
  bool DownloadAttachment(std::uint64_t handle, std::vector<std::uint8_t>& output);
  void PumpStateWaitForDownload();
  bool QueueUpload(int operationId, const leaderboardDefinition_t* definition,
                   const column_t* columns, const void* attachment, int attachmentSize);
  void PumpStateWaitForUpload();
  void Pump();

private:
  state_t state{STATE_IDLE};
  std::vector<std::unique_ptr<idAsyncLeaderboardUpload>> asyncUploads;
  std::unique_ptr<idAsyncLeaderboardDownload> download;
};

idSteamLeaderboardPlatform* Win_GetSteamLeaderboardPlatform();
idLeaderboardCallback::row_t ConvertEntryToRow(const steamLeaderboardEntry_t& entry);
