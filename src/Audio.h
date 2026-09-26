#pragma once
#include "Common.h"

enum class PcmFormat { ADPCM=0, P16=1, P8=2 };
struct WaveData {
  std::vector<float> mono;
  uint32_t sampleRate=0;
  uint16_t bits=0;
  uint16_t channels=0;
  std::wstring formatName;
};

bool LoadWave(const std::wstring& path, WaveData& out, std::wstring& err);
std::vector<float> ResampleLinear(const std::vector<float>& in, double ratio);
std::vector<float> TimeStretchOLA(const std::vector<float>& in, double factor);
std::vector<float> ProcessPcm(const std::vector<float>& in, uint32_t inRate, float volume, int transpose, bool pitchShift);
std::vector<uint8_t> EncodeTarget(const std::vector<float>& pcm15625, PcmFormat fmt);
std::vector<float> DecodeTarget(const uint8_t* data, size_t bytes, PcmFormat fmt);
bool WriteMono16Wave(const std::wstring& path, const std::vector<float>& pcm, uint32_t rate, std::wstring& err);

class WaveOutPlayer {
public:
  ~WaveOutPlayer(){ Stop(); }
  void Stop();
  bool Play(const std::vector<float>& pcm, uint32_t rate);
  bool IsPlaying() const { return h_ && !(hdr_.dwFlags & WHDR_DONE); }
  size_t PositionSamples() const;
private:
  HWAVEOUT h_=nullptr; WAVEHDR hdr_{}; std::vector<int16_t> buf_;
};
