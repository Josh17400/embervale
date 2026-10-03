// Software synth + mixer. No audio files: every sound is generated. Thread-safe play().
#pragma once
#include <atomic>
#include <cstdint>
#include "engine/mathx.h"

struct SDL_AudioStream;
struct SDL_Mutex;

enum class Sfx : uint8_t { Level, Hurt, Select, Dead, Start, Summon, Merge, Shot, Hit, Coin, WaveStart, Lucky, WaveClear, WallHit };

class Audio {
 public:
  bool init();
  void shutdown();
  void play(Sfx s, float pitch = 1.0f, float vol = 1.0f);
  void setIntensity(float v) { intensity_.store(v); }   // 0..1 drives music layers
  void setMusic(bool on) { music_.store(on); }
  float beat() const { return beat_.load(); }            // 0..1 phase within the current beat (1 = on the beat)
  void setMaster(float v) { master_.store(v); }

  // called from the audio thread
  void render(float* out, int frames);

 private:
  enum Wave : uint8_t { Sine, Square, Saw, Noise };
  struct Voice {
    bool on = false;
    Wave wave = Sine;
    float phase = 0, freq = 440, slide = 0;   // slide: octaves per second
    float vol = 0.3f, attack = 0.003f, dur = 0.2f, t = 0, delay = 0;
    uint32_t rng = 1;
    float lp = 0;                              // one-pole lowpass state
    float cutoff = 1.0f;                       // 1 = open
  };
  static constexpr int MAXV = 40;
  Voice v_[MAXV];
  SDL_AudioStream* stream_ = nullptr;
  SDL_Mutex* mu_ = nullptr;
  std::atomic<float> intensity_{0.0f}, beat_{0.0f}, master_{0.55f};
  std::atomic<bool> music_{true};
  int sampleRate_ = 48000;
  // music sequencer state (audio thread only)
  double seqT_ = 0;
  int lastStep_ = -1;
  void add(Wave w, float f, float slide, float vol, float dur, float delay = 0, float attack = 0.003f, float cutoff = 1.0f);
  void musicStep(int step);
};
