#pragma once

#include "engine/sound/mediaplayer/mediaplayer.h"

class idNullMediaPlayer final : public idMediaPlayer {
public:
  ~idNullMediaPlayer() override = default;
  void Start() override {}
  void Stop() override {}
  void Pause() override {}
  void Resume() override {}
  void NextSong() override {}
  void PreviousSong() override {}
  void Volume(float volume) override { (void)volume; }
  int GetCurrentPlaylist() override { return 0; }
  void SetCurrentPlaylist(int playlist) override { (void)playlist; }
};

extern idNullMediaPlayer nullMediaPlayer;
extern idMediaPlayer* mediaPlayer;

static_assert(sizeof(idNullMediaPlayer) == sizeof(void*));
