#include "engine/sound/sound_runtime.h"

#include "shared/idlib/filesystem/file.h"

void SaveParms(idFile* file, const soundShaderParms_t* parms) {
  if (file == nullptr || parms == nullptr) return;
  file->Write(parms, sizeof(*parms));
}

bool LoadParms(idFile* file, soundShaderParms_t* parms) {
  return file != nullptr && parms != nullptr &&
         file->Read(parms, sizeof(*parms)) == sizeof(*parms);
}

void SaveFade(idFile* file, const idSoundFade* fade) {
  if (file == nullptr || fade == nullptr) return;
  file->Write(fade, sizeof(*fade));
}

bool LoadFade(idFile* file, idSoundFade* fade) {
  return file != nullptr && fade != nullptr &&
         file->Read(fade, sizeof(*fade)) == sizeof(*fade);
}

void idSoundWorldLocal::SaveGame(idFile* file) {
  if (file != nullptr) file->Write(&soundTime, sizeof(soundTime));
}

bool idSoundWorldLocal::LoadGame(idFile* file) {
  return file != nullptr && file->Read(&soundTime, sizeof(soundTime)) == sizeof(soundTime);
}

