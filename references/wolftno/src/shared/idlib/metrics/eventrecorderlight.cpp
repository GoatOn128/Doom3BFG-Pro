#include "shared/idlib/metrics/recoverymetrics.h"
#include "shared/idlib/filesystem/file_buffered.h"
#include "shared/idlib/filesystem/filesystemlocal.h"
#include "shared/idlib/lib.h"
#include "shared/idlib/sys/sys_time.h"

#include <algorithm>
#include <cstdint>

void idEventRecorderLight::StartCapture() {
  if (list.size < 0)
    list.ClearFree();
  list.num = std::min(list.size, 0);
  if (list.size < 32768)
    list.ResizeForPort(32768);
  for (int i = list.num; i < 32768; ++i)
    list.list[i] = {-1, nullptr};

  if (strBuffer.size < 0)
    strBuffer.ClearFree();
  strBuffer.num = std::min(strBuffer.size, 0);
  if (strBuffer.size < STRING_BUFFER_SIZE)
    strBuffer.ResizeForPort(STRING_BUFFER_SIZE);
  for (int i = strBuffer.num; i < STRING_BUFFER_SIZE; ++i)
    strBuffer.list[i] = '\0';

  enabled = true;
  startStamp = Sys_GetClockTicks();
}

void idEventRecorderLight::StopCapture() {
  enabled = false;
}

void idEventRecorderLight::Clear() {
  list.ClearFree();
  enabled = false;
}

void idEventRecorderLight::StartEventCopyStringInternal(const char* tag) {
  const int length = idStr::Length(tag);
  const int offset = strBuffer.num;
  const char* storedTag = "eventlog out of stringbuffer";
  if (offset + length + 1 < STRING_BUFFER_SIZE) {
    const int required = offset + length + 1;
    if (required > strBuffer.size)
      strBuffer.ResizeForPort(required);
    strBuffer.num = std::min(required, strBuffer.size);
    idStr::Copynz(strBuffer.list + offset, tag, length + 1);
    storedTag = strBuffer.list + offset;
  }

  const event ev = {Sys_GetClockTicks(), storedTag};
  list.Append(ev);
}

void idEventRecorderLight::WriteReport(const char* filename, bool numbered,
                                       fsPath_t basePath) {
  idFile_Buffered file(32768);

  idStr outputName;
  if (numbered) {
    const idStr originalName(filename);
    const idStr base = originalName.GetWithoutExtension();
    idStr extension;
    originalName.ExtractFileExtension(extension);
    for (int index = 0; index < 9999; ++index) {
      outputName.Format("%s%04d.%s", base.data, index, extension.data);
      if (fileSystem == nullptr || !fileSystem->FileExists(outputName.data, true))
        break;
    }
  } else {
    outputName = filename;
  }

  file.OpenFileWrite(outputName.data, basePath);

  idList<event, 5> openEvents;
  file.Printf("//times in ms\n");
  file.Printf("//timestamp duration, tag\n");

  static const char indentation[] = "                                ";
  const float ticksPerSecond = static_cast<float>(GetTicksPerSec());
  for (int eventIndex = 0; eventIndex < list.num; ++eventIndex) {
    const event& current = list.list[eventIndex];
    const float time = 1000.0f * static_cast<float>(current.stamp - startStamp) /
                       ticksPerSecond;
    if (current.tag != nullptr) {
      const event& next = list.list[eventIndex + 1];
      if (next.tag != nullptr) {
        const int depth = std::clamp(openEvents.num, 0, 32);
        file.Printf("%8.3f:   %s>> %s\n", time, indentation + 32 - depth,
                    current.tag);
        openEvents.Append(current);
      } else {
        const float duration =
            1000.0f * static_cast<float>(next.stamp - current.stamp) /
            ticksPerSecond;
        const int depth = std::clamp(openEvents.num, 0, 32);
        file.Printf("%8.3f:   %s<>%8.3f %s\n", time,
                    indentation + 32 - depth, duration, current.tag);
        ++eventIndex;
      }
    } else {
      const int openIndex = openEvents.num - 1;
      const event& opening = openEvents.list[openIndex];
      const float duration =
          1000.0f * static_cast<float>(current.stamp - opening.stamp) /
          ticksPerSecond;
      const int depth = std::clamp(openIndex, 0, 32);
      file.Printf("%8.3f:   %s<<%8.3f %s\n", time,
                  indentation + 32 - depth, duration, opening.tag);
      if (openEvents.num > 0 && openIndex < openEvents.num)
        --openEvents.num;
    }
  }

  if (file.backFile != nullptr)
    idLib::Printf("Wrote event record log to \"%s\"\n",
                  file.backFile->GetFullPath());

  const std::int64_t benchmarkStart = Sys_GetClockTicks();
  std::int64_t dummy = 0;
  for (int index = 0; index < 1000; ++index)
    dummy += Sys_GetClockTicks();
  const float benchmarkMicroseconds =
      1000000.0f * static_cast<float>(Sys_GetClockTicks() - benchmarkStart) /
      static_cast<float>(Sys_ClockTicksPerSecond());
  idLib::Printf("1000 gettime: %2.3fus (make sure dummy is used: %d)\n",
                benchmarkMicroseconds, static_cast<int>(dummy));
}
