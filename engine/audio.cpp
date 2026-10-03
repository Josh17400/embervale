#include "engine/audio.h"
#include <SDL3/SDL.h>
#include <cmath>

namespace {
constexpr double BPM = 124.0;
constexpr double STEP_LEN = 60.0 / BPM / 4.0;     // 16th note
// A minor pentatonic over two octaves (Hz)
const float kScale[] = {220.00f, 261.63f, 293.66f, 329.63f, 392.00f, 440.00f, 523.25f, 587.33f, 659.25f, 784.00f};

void SDLCALL streamCb(void* ud, SDL_AudioStream* s, int additional, int) {
  Audio* a = (Audio*)ud;
  int frames = additional / (int)sizeof(float);
  float buf[1024];
  while (frames > 0) {
    int n = frames > 1024 ? 1024 : frames;
    a->render(buf, n);
    SDL_PutAudioStreamData(s, buf, n * (int)sizeof(float));
    frames -= n;
  }
}
}  // namespace

bool Audio::init() {
  mu_ = SDL_CreateMutex();
  SDL_AudioSpec spec{SDL_AUDIO_F32, 1, sampleRate_};
  stream_ = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, streamCb, this);
  if (!stream_) {
    SDL_Log("audio unavailable: %s", SDL_GetError());
    return false;
  }
  SDL_ResumeAudioStreamDevice(stream_);
  return true;
}

void Audio::shutdown() {
  if (stream_) { SDL_DestroyAudioStream(stream_); stream_ = nullptr; }
  if (mu_) { SDL_DestroyMutex(mu_); mu_ = nullptr; }
}

void Audio::add(Wave w, float f, float slide, float vol, float dur, float delay, float attack, float cutoff) {
  int slot = -1;
  for (int i = 0; i < MAXV; i++) if (!v_[i].on) { slot = i; break; }
  if (slot < 0) {   // steal the oldest
    float best = -1;
    for (int i = 0; i < MAXV; i++) if (v_[i].t > best) { best = v_[i].t; slot = i; }
  }
  Voice& v = v_[slot];
  v = Voice{};
  v.on = true; v.wave = w; v.freq = f; v.slide = slide; v.vol = vol; v.dur = dur; v.delay = delay;
  v.attack = attack; v.cutoff = cutoff; v.rng = 0x9E3779B1u * (uint32_t)(slot + 7) + (uint32_t)(f * 13);
}

void Audio::play(Sfx s, float pitch, float vol) {
  if (!mu_) return;
  SDL_LockMutex(mu_);
  switch (s) {
    case Sfx::Level:
      for (int i = 0; i < 4; i++) add(Square, kScale[i * 2] * 2, 0, 0.09f * vol, 0.16f, 0.07f * i, 0.003f, 0.6f);
      break;
    case Sfx::Hurt:
      add(Saw, 240, -1.4f, 0.28f * vol, 0.28f, 0, 0.002f, 0.5f);
      add(Noise, 0, 0, 0.20f * vol, 0.15f, 0, 0.001f, 0.7f);
      break;
    case Sfx::Select:
      add(Sine, 660, 0.3f, 0.2f * vol, 0.08f);
      add(Sine, 990, 0.0f, 0.15f * vol, 0.12f, 0.05f);
      break;
    case Sfx::Dead:
      add(Saw, 300, -1.0f, 0.35f * vol, 0.9f, 0, 0.002f, 0.4f);
      add(Sine, 90, -0.8f, 0.5f * vol, 0.9f);
      break;
    case Sfx::Start:
      for (int i = 0; i < 3; i++) add(Square, kScale[i * 2 + 1], 0, 0.1f * vol, 0.15f, 0.06f * i, 0.003f, 0.6f);
      break;
    case Sfx::Summon:
      add(Sine, 300 * pitch, 1.6f, 0.22f * vol, 0.14f);
      add(Noise, 0, 0, 0.08f * vol, 0.06f, 0, 0.001f, 0.8f);
      break;
    case Sfx::Merge:   // pitch climbs with level/combo; two-note chime + thump
      add(Sine, 110, -1.2f, 0.30f * vol, 0.22f);
      add(Square, 392 * pitch, 0, 0.09f * vol, 0.16f, 0.0f, 0.002f, 0.55f);
      add(Square, 588 * pitch, 0, 0.09f * vol, 0.22f, 0.07f, 0.002f, 0.55f);
      add(Sine, 1176 * pitch, 0, 0.06f * vol, 0.25f, 0.12f);
      break;
    case Sfx::Shot:
      add(Noise, 0, 0, 0.05f * vol, 0.03f, 0, 0.001f, 0.9f);
      add(Square, 900 * pitch, -2.5f, 0.035f * vol, 0.05f, 0, 0.001f, 0.7f);
      break;
    case Sfx::Hit:
      add(Noise, 0, 0, 0.07f * vol, 0.04f, 0, 0.001f, 0.6f);
      add(Sine, 240 * pitch, -2.0f, 0.08f * vol, 0.06f);
      break;
    case Sfx::Coin:
      add(Sine, 1320 * pitch, 0, 0.06f * vol, 0.05f);
      add(Sine, 1760 * pitch, 0, 0.06f * vol, 0.08f, 0.04f);
      break;
    case Sfx::WaveStart:
      add(Saw, 98, 0.2f, 0.25f * vol, 0.5f, 0, 0.05f, 0.25f);
      add(Square, 196, 0, 0.08f * vol, 0.3f, 0.1f, 0.003f, 0.5f);
      break;
    case Sfx::Lucky:
      for (int i = 0; i < 7; i++) add(Sine, kScale[i + 2] * 2 * pitch, 0, 0.11f * vol, 0.28f, 0.05f * i);
      break;
    case Sfx::WaveClear:
      for (int i = 0; i < 5; i++) add(Square, kScale[i * 2] * 2, 0, 0.08f * vol, 0.2f, 0.07f * i, 0.003f, 0.6f);
      break;
    case Sfx::WallHit:
      add(Noise, 0, 0, 0.3f * vol, 0.25f, 0, 0.001f, 0.25f);
      add(Sine, 70, -0.8f, 0.5f * vol, 0.3f);
      break;
  }
  SDL_UnlockMutex(mu_);
}

void Audio::musicStep(int step) {
  float in = intensity_.load();
  int s = step & 15;
  int bar = (step >> 4) & 3;
  static const float bassRoot[4] = {55.0f, 55.0f, 43.65f, 49.0f};  // A A F G
  float root = bassRoot[bar];
  if (s % 4 == 0) {                                                  // kick on the beat
    add(Sine, 110, -3.0f, 0.5f, 0.16f, 0, 0.001f);
  }
  if (in > 0.12f && (s % 4 == 2)) add(Saw, root, 0, 0.14f, 0.16f, 0, 0.003f, 0.12f);   // offbeat bass
  if (in > 0.30f && (s % 2 == 1)) add(Noise, 0, 0, 0.05f, 0.03f, 0, 0.001f, 0.95f);    // hats
  if (in > 0.5f && (s == 4 || s == 12)) add(Noise, 0, 0, 0.12f, 0.12f, 0, 0.001f, 0.6f); // snare
  if (in > 0.65f) {                                                  // arpeggio
    static const int pat[16] = {0, 2, 4, 2, 5, 4, 2, 4, 0, 2, 4, 7, 5, 4, 2, 0};
    int note = pat[s] + (bar == 2 ? -1 : (bar == 3 ? 1 : 0));
    note = note < 0 ? 0 : (note > 9 ? 9 : note);
    add(Square, kScale[note], 0, 0.045f, 0.09f, 0, 0.002f, 0.5f);
  }
}

void Audio::render(float* out, int frames) {
  SDL_LockMutex(mu_);
  const float dt = 1.0f / sampleRate_;
  float master = master_.load();
  bool music = music_.load();
  for (int n = 0; n < frames; n++) {
    if (music) {
      seqT_ += dt;
      int step = (int)(seqT_ / STEP_LEN);
      if (step != lastStep_) { lastStep_ = step; musicStep(step); }
      double beatLen = 60.0 / BPM;
      beat_.store(1.0f - (float)std::fmod(seqT_, beatLen) / (float)beatLen);
    }
    float mix = 0;
    for (int i = 0; i < MAXV; i++) {
      Voice& v = v_[i];
      if (!v.on) continue;
      if (v.delay > 0) { v.delay -= dt; continue; }
      if (v.t >= v.dur) { v.on = false; continue; }
      float env = v.t < v.attack ? v.t / v.attack : 1.0f;
      float rem = (v.dur - v.t) / v.dur;
      env *= rem * rem;
      float s = 0;
      switch (v.wave) {
        case Sine: s = std::sin(v.phase * TAU); break;
        case Square: s = std::sin(v.phase * TAU) > 0 ? 1.0f : -1.0f; break;
        case Saw: s = 2.0f * (v.phase - std::floor(v.phase + 0.5f)); break;
        case Noise:
          v.rng = v.rng * 1664525u + 1013904223u;
          s = ((v.rng >> 9) * (1.0f / 4194304.0f)) - 1.0f;
          break;
      }
      v.lp += (s - v.lp) * v.cutoff;
      mix += v.lp * v.vol * env;
      float f = v.freq * std::exp2(v.slide * v.t);
      v.phase += f * dt;
      v.phase -= std::floor(v.phase);
      v.t += dt;
    }
    mix = std::tanh(mix * 1.2f) * master;   // soft clip
    out[n] = mix;
  }
  SDL_UnlockMutex(mu_);
}
