#pragma once

class idMediaPlayer {
public:
  virtual ~idMediaPlayer() = default;
  virtual void Start() = 0;
  virtual void Stop() = 0;
  virtual void Pause() = 0;
  virtual void Resume() = 0;
  virtual void NextSong() = 0;
  virtual void PreviousSong() = 0;
  virtual void Volume(float volume) = 0;
  virtual int GetCurrentPlaylist() = 0;
  virtual void SetCurrentPlaylist(int playlist) = 0;
};

static_assert(sizeof(idMediaPlayer) == sizeof(void*));
