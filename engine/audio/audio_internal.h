// engine/audio's private header: the shared DSP helpers, the melody note sets and the note builder (Audio::NB).
// Who may include it: engine/audio.cpp and engine/audio/*.cpp only (everyone else uses engine/audio.h).
//   synth.cpp     the voice pool: Audio::renderVoice (Audio::add is inline below)
//   sfx.cpp       every sound effect (Audio::fx<name>, one per SFX_LIST entry), its trim and name, the motifs, the stings
//   music.cpp     the generative score: pieces, composition, sequencing, the instruments, updateMusic
//   ambience.cpp  the beds: renderAmbient, renderSea, renderFalls, renderCrowd
//   mix.cpp       renderBlock (buses, reverb, echo, limiter), render, the device stream
#pragma once
#include "engine/audio.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>
#include <initializer_list>

namespace {
constexpr float FS = (float)Audio::kSampleRate;
constexpr float DT = 1.0f / FS;
constexpr float CROSSFADE_SEC = 2.0f;

// ------------------------------------------------------------------------------------------ DSP helpers
struct SineTable {
  float v[2049];
  SineTable() { for (int i = 0; i <= 2048; i++) v[i] = std::sin(TAU * (float)i / 2048.0f); }
};
const SineTable kSine;

inline float sinc(float ph) {  // sine of a phase given in cycles (any range)
  ph -= std::floor(ph);
  float x = ph * 2048.0f;
  int i = (int)x;
  if (i > 2047) i = 2047;
  float fr = x - (float)i;
  return kSine.v[i] + (kSine.v[i + 1] - kSine.v[i]) * fr;
}
inline float blep(float t, float dt) {  // polyBLEP residual: band-limits saw / square edges
  if (t < dt) { t /= dt; return t + t - t * t - 1.0f; }
  if (t > 1.0f - dt) { t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
  return 0.0f;
}
inline float softSat(float x) {  // tanh-like, cheap
  if (x > 3.0f) return 1.0f;
  if (x < -3.0f) return -1.0f;
  float x2 = x * x;
  return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}
inline uint32_t xs32(uint32_t& s) { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
inline float white(uint32_t& s) { return (float)(int32_t)xs32(s) * (1.0f / 2147483648.0f); }
inline float unit(uint32_t& s) { return (float)(xs32(s) >> 8) * (1.0f / 16777216.0f); }
inline float mtof(int midi) { return 440.0f * std::exp2((float)(midi - 69) / 12.0f); }
inline int posmod(int a, int m) { int r = a % m; return r < 0 ? r + m : r; }
inline int posmodTop(int a) { return posmod(a, 12); }
}  // namespace

// inline here (it was one TU with its callers): under /fp:fast an out-of-line call rounds rnd(lo, hi) differently
inline float Audio::rnd() { return unit(rng_); }
namespace {

constexpr uint16_t pcs(std::initializer_list<int> l) { uint16_t m = 0; for (int x : l) m = (uint16_t)(m | 1u << x); return m; }
const uint16_t kMelMask[(int)Scale::COUNT] = {
  pcs({0, 2, 4, 5, 7, 9, 11}), pcs({0, 2, 3, 5, 7, 8, 10}), pcs({0, 2, 3, 5, 7, 9, 10}), pcs({0, 1, 3, 5, 7, 8, 10}),
  pcs({0, 2, 4, 5, 7, 9, 10}), pcs({0, 2, 4, 6, 7, 9, 11}), pcs({0, 2, 3, 5, 7, 8, 11}), pcs({0, 1, 4, 5, 7, 8, 10}),
  pcs({0, 2, 4, 7, 9}),        pcs({0, 3, 5, 7, 10}),       pcs({0, 2, 3, 7, 8}),       pcs({0, 1, 5, 7, 10}),
  pcs({0, 2, 4, 6, 8, 10}),    pcs({0, 1, 3, 7, 8}),        pcs({0, 1, 4, 5, 7, 8, 11}),
};
}  // namespace

// ------------------------------------------------------------------------------------------ note builder
struct Audio::NB {
  Note n;
  NB(Wave w, float f, float vol) { n.wave = w; n.freq = f; n.vol = vol; }
  NB& env(float a, float d, float s = 0, float hold = 30, float r = 0.05f) {
    n.atk = a; n.dec = d; n.sus = s; n.hold = hold; n.rel = r; return *this;
  }
  NB& perc(float d) { return env(0.001f, d); }
  NB& lp(float c, float q = 0.707f) { n.filt = LP; n.cutoff = c; n.q = q; return *this; }
  NB& bp(float c, float q = 1.0f) { n.filt = BP; n.cutoff = c; n.q = q; return *this; }
  NB& hp(float c, float q = 0.707f) { n.filt = HP; n.cutoff = c; n.q = q; return *this; }
  NB& vox(float c, float q = 1.5f) { n.filt = Vox; n.cutoff = c; n.q = q; return *this; }
  NB& fenv(float oct, float dec) { n.fEnv = oct; n.fDec = dec; return *this; }
  NB& bump(float oct) { n.fBump = oct; return *this; }   // cutoff rises and falls over the gate (whooshes)
  NB& fslide(float oct) { n.fSlide = oct; return *this; }
  NB& pitch(float amount, float dec) { n.pEnv = amount; n.pDec = dec; return *this; }
  NB& slide(float oct) { n.slide = oct; return *this; }
  NB& vib(float depth, float rate, float delay = 0) { n.vibDepth = depth; n.vibRate = rate; n.vibDelay = delay; return *this; }
  NB& fm(float ratio, float index, float dec) { n.fmRatio = ratio; n.fmIndex = index; n.fmDec = dec; return *this; }
  NB& am(float rate, float depth) { n.amRate = rate; n.amDepth = depth; return *this; }
  NB& drive(float d) { n.drive = d; return *this; }
  NB& send(float s) { n.send = s; return *this; }
  NB& at(float d) { n.delay = d; return *this; }
  NB& pw(float w) { n.pw = w; return *this; }
  NB& detune(float d) { n.detune = d; return *this; }
  NB& bright(float b) { n.bright = b; return *this; }
  NB& ring(float t60) { n.ring = t60; return *this; }
  NB& breath(float b) { n.breath = b; return *this; }
  NB& color(float c) { n.color = c; return *this; }
  NB& bus(int b) { n.bus = (uint8_t)b; return *this; }
  operator const Note&() const { return n; }

  static float envAt(const Note& p, float t) {   // closed-form envelope: attack, exp decay to sustain, release
    auto gate = [&](float tt) {
      if (tt < p.atk) return tt / p.atk;
      return p.sus + (1.0f - p.sus) * std::exp(-(tt - p.atk) / p.dec);
    };
    if (t < p.hold) return gate(t);
    return gate(p.hold) * std::exp(-(t - p.hold) / p.rel);
  }
};

// ------------------------------------------------------------------------------------------ voices
// Audio::add is inline here, not in synth.cpp: its callers (sfx, music, ambience) were one TU with it, and under /fp:fast
// MSVC schedules (and so rounds) their float code differently when add is out of reach. Bit identity needs it visible.
inline void Audio::add(const Note& src) {
  int slot = -1;
  float best = 1e30f;
  for (int i = 0; i < MAXV; i++) {
    const Voice& v = v_[i];
    if (!v.on) { slot = i; break; }
    float score = v.t > 0 ? v.gain : v.n.vol;     // pending voices count at full volume
    if (v.n.bus == 0) score *= 2.0f;              // sfx are worth more than a music voice
    if (score < best) { best = score; slot = i; }
  }
  Voice& v = v_[slot];
  if (v.on && v.str >= 0) strUsed_[v.str] = false;
  v = Voice{};
  Note& p = v.n;
  p = src;
  p.atk = std::fmax(p.atk, 2e-4f);
  p.dec = std::fmax(p.dec, 1e-3f);
  p.rel = std::fmax(p.rel, 1e-3f);
  p.pDec = std::fmax(p.pDec, 1e-3f);
  p.fDec = std::fmax(p.fDec, 1e-3f);
  p.fmDec = std::fmax(p.fmDec, 1e-3f);
  p.hold = std::fmax(p.hold, 1e-3f);
  p.freq = clampf(p.freq, 1.0f, 20000.0f);
  p.delay = std::fmax(p.delay, 0.0f);
  v.on = true;
  v.rng = xs32(rng_) | 1u;
  if (p.wave == Pad)
    for (float& ph : v.ph) ph = rnd();

  if (p.wave == Pluck) {   // Karplus-Strong: noise burst in a tuned, damped delay loop
    int s = -1;
    for (int i = 0; i < NSTR; i++) if (!strUsed_[i]) { s = i; break; }
    if (s < 0) {           // all strings busy: a filtered triangle pluck is a fine stand-in
      p.wave = Tri;
      p.sus = 0;
      p.dec = p.ring * 0.18f;
      if (p.filt == Off) { p.filt = LP; p.cutoff = p.freq * 4.0f; }
      return;
    }
    strUsed_[s] = true;
    v.str = s;
    float period = std::fmin(FS / p.freq, (float)(STRLEN - 2));
    v.ksH1 = 0.5f * (1.0f - clampf(p.bright, 0.0f, 1.0f));
    float d = period - v.ksH1;
    int len = (int)(d - 0.1f);
    if (len < 2) len = 2;
    float frac = d - (float)len;
    v.ksC = (1.0f - frac) / (1.0f + frac);
    v.ksLen = len;
    v.ksRho = std::pow(0.001f, 1.0f / (p.freq * std::fmax(p.ring, 0.05f)));
    float* buf = str_[s];
    float a = 0.25f + 0.7f * clampf(p.bright, 0.0f, 1.0f), y = 0, mean = 0, peak = 1e-6f;
    for (int i = 0; i < len; i++) { y += a * (white(v.rng) - y); buf[i] = y; mean += y; }
    mean /= (float)len;
    for (int i = 0; i < len; i++) { buf[i] -= mean; peak = std::fmax(peak, std::fabs(buf[i])); }
    for (int i = 0; i < len; i++) buf[i] *= 0.9f / peak;
  }
}
