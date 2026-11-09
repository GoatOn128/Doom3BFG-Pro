#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

class idProtoMessage {
public:
  virtual ~idProtoMessage() = default;
  virtual std::string GetTypeName() const = 0;
  virtual void Clear() = 0;
  virtual bool SerializeToString(std::string* output) const = 0;
  virtual bool ParseFromString(const std::string& input) = 0;
  virtual bool IsInitialized() const { return true; }
  int ByteSize() const {
    std::string data;
    SerializeToString(&data);
    return static_cast<int>(data.size());
  }
};

namespace idproto {
void WriteVarint(std::string&, std::uint64_t);
void WriteString(std::string&, unsigned, const std::string&);
void WriteUInt(std::string&, unsigned, std::uint64_t);
void WriteFloat(std::string&, unsigned, float);
bool Next(const std::string&, std::size_t&, unsigned&, unsigned&, std::uint64_t&, std::string&);
} // namespace idproto

namespace idmetrics {
struct SessionHeader final : idProtoMessage {
  std::string runtimeSessionId, matchId, sessionState;
  std::string GetTypeName() const override { return "idmetrics.SessionHeader"; }
  void Clear() override;
  bool SerializeToString(std::string*) const override;
  bool ParseFromString(const std::string&) override;
};
struct Vec3 final : idProtoMessage {
  float x{}, y{}, z{};
  std::string GetTypeName() const override { return "idmetrics.Vec3"; }
  void Clear() override { x = y = z = 0; }
  bool SerializeToString(std::string*) const override;
  bool ParseFromString(const std::string&) override;
};
struct ReliableMessageMetric final : idProtoMessage {
  SessionHeader header;
  std::uint32_t peerCount{}, peerNumber{};
  std::vector<std::uint32_t> reliables;
  std::string GetTypeName() const override { return "idmetrics.ReliableMessageMetric"; }
  void Clear() override;
  bool SerializeToString(std::string*) const override;
  bool ParseFromString(const std::string&) override;
};
struct SnapshotMetric final : idProtoMessage {
  SessionHeader header;
  std::string report;
  std::string GetTypeName() const override { return "idmetrics.SnapshotMetric"; }
  void Clear() override;
  bool SerializeToString(std::string*) const override;
  bool ParseFromString(const std::string&) override;
};
struct SessionStateRecord final : idProtoMessage {
  SessionHeader header;
  std::string state;
  std::string GetTypeName() const override { return "idmetrics.SessionStateRecord"; }
  void Clear() override;
  bool SerializeToString(std::string*) const override;
  bool ParseFromString(const std::string&) override;
};
struct EquippedWeapon {
  std::string name;
  float currentSpread{};
  bool isZoomed{};
};
struct AmmoDef {
  std::string name;
  std::uint32_t count{};
};
struct WeaponDef {
  std::string name;
};
struct GrenadeDef {
  std::string name;
  std::uint32_t count{};
};
struct PlayerDeath {
  std::string cause;
  Vec3 position;
};
} // namespace idmetrics

namespace idreports {
class DynamicReport : public idProtoMessage {
public:
  std::map<std::string, std::string> fields;
  std::vector<std::string> tags;
  void Clear() override {
    fields.clear();
    tags.clear();
  }
  bool SerializeToString(std::string*) const override;
  bool ParseFromString(const std::string&) override;
  void Set(const std::string& key, const std::string& value) { fields[key] = value; }
  const std::string& Get(const std::string& key) const;
};
#define ID_REPORT(Name)                                                                            \
  struct Name final : DynamicReport {                                                              \
    std::string GetTypeName() const override { return "idreports." #Name; }                        \
  };
ID_REPORT(BigUInt)
ID_REPORT(Attachment) ID_REPORT(LogEvent) ID_REPORT(MemoryInfo) ID_REPORT(ExceptionInfo)
    ID_REPORT(CallstackLine) ID_REPORT(EngineVersionInfo) ID_REPORT(AssertReport)
        ID_REPORT(MapWarning) ID_REPORT(MapReport) ID_REPORT(MapLoadPerformanceReport)
            ID_REPORT(ViewNoteReport) ID_REPORT(StringIDReferenceReport) ID_REPORT(CPUInfo)
                ID_REPORT(RegisterInfo) ID_REPORT(HeapCategoryUsage) ID_REPORT(HeapUsage)
                    ID_REPORT(Xbox360VersionInfo) ID_REPORT(Xbox360KitInfo)
                        ID_REPORT(XenonRegisterInfo) ID_REPORT(XenonMemoryInfo)
                            ID_REPORT(PPURegisterInfo) ID_REPORT(SPURegisterInfo)
                                ID_REPORT(PS3ExceptionInfo) ID_REPORT(PS3VMRegionStats)
                                    ID_REPORT(PS3MemoryInfo) ID_REPORT(CrashReport)
                                        ID_REPORT(Xbox360CrashReport) ID_REPORT(PS3CrashReport)
                                            ID_REPORT(MapCompileStats) ID_REPORT(MapLoadStats)
                                                ID_REPORT(MapSaveStats)
#undef ID_REPORT
} // namespace idreports
