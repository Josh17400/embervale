// EMBERVALE software synth: SFX voices + generative nordic-folk score + reverb/echo + limiter.
//
// Threading: play() / setMusic() only touch a lock-free queue and an atomic. Everything else (voice
// allocation, sequencing, DSP) happens on the audio thread inside render(), so render() also works
// standalone (no init(), no device) for offline previews. Nothing on the audio thread allocates or
// uses std::function: fixed voice pool, fixed string and delay buffers.
//
// Signal flow, per 32-sample block:
//   voices -> sfx bus -------------------------------------------------------\
//          -> music bus x layer crossfade x musicVol x ducking under sfx ----+-> + reverb/echo -> master -> limiter
//          -> reverb / echo sends -------------------------------------------/
#include "engine/audio.h"
#include <SDL3/SDL.h>
#include <cmath>

namespace {
constexpr float FS = 48000.0f;
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

// ------------------------------------------------------------------------------------------ music theory
enum ScaleId : uint8_t { Ionian, Aeolian, Phrygian };
const int8_t kScale[3][7] = {
  {0, 2, 4, 5, 7, 9, 11},   // major
  {0, 2, 3, 5, 7, 8, 10},   // natural minor
  {0, 1, 3, 5, 7, 8, 10},   // phrygian (dark, b2)
};

struct Style {
  float bpm;                // beats per minute (beat = spb sixteenths)
  uint8_t beats, spb;       // beats per bar, sixteenth-steps per beat (4 simple, 6 compound)
  uint8_t chordBars;        // bars per chord
  int8_t root;              // MIDI tonic for the bass register
  ScaleId scale;
  int8_t progs[4][4];       // chord roots as scale degrees
  int8_t melRoot;           // MIDI tonic for the melody (same pitch class as root)
  int8_t lo, hi;            // melody range in scale degrees around melRoot
  float busy;               // rhythmic density of motifs, 0..1
  float density;            // chance a phrase is sung (0 = no melody)
  float echo;               // echo send for this piece
  bool intro;               // first section: half of it without melody
  float level;              // overall gain, evens out loudness between pieces
};

const Style kStyle[(int)Music::COUNT] = {
  // Silence
  {60, 4, 4, 1, 48, Aeolian, {{0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}}, 60, 0, 7, 0, 0, 0.10f, false, 1.0f},
  // Title: D minor, slow 4/4, two bars per chord, horn + choir over taiko
  {64, 4, 4, 2, 38, Aeolian, {{0, 5, 2, 6}, {0, 6, 5, 6}, {0, 3, 5, 6}, {5, 6, 0, 0}}, 62, -3, 8, 0.15f, 0.9f, 0.20f, true, 0.62f},
  // Wild: G major 3/4 pastoral waltz, flute + harp
  {88, 3, 4, 1, 43, Ionian, {{0, 4, 5, 3}, {0, 3, 0, 4}, {5, 3, 0, 4}, {0, 2, 3, 4}}, 67, -2, 8, 0.4f, 0.8f, 0.14f, true, 1.25f},
  // Night: A minor, very slow, bells and distant flute
  {54, 4, 4, 2, 45, Aeolian, {{0, 5, 3, 4}, {0, 4, 5, 2}, {0, 3, 0, 6}, {5, 3, 6, 0}}, 69, -3, 6, 0.2f, 0.6f, 0.32f, false, 1.1f},
  // Town: F major 6/8 jig-lilt, lute arpeggios, flute / fiddle
  {72, 2, 6, 1, 41, Ionian, {{0, 3, 4, 0}, {0, 5, 3, 4}, {0, 4, 5, 3}, {3, 0, 4, 0}}, 77, -4, 5, 0.5f, 0.85f, 0.10f, true, 1.15f},
  // Cave: E phrygian drones, no melody
  {48, 4, 4, 2, 40, Phrygian, {{0, 1, 0, 6}, {0, 0, 1, 0}, {0, 5, 1, 0}, {0, 6, 5, 1}}, 64, -2, 5, 0.1f, 0.0f, 0.50f, false, 1.1f},
  // Combat: E minor 140 bpm, taiko + string ostinato + fiddle
  {140, 4, 4, 1, 40, Aeolian, {{0, 5, 6, 0}, {0, 6, 5, 6}, {0, 3, 6, 2}, {0, 5, 2, 6}}, 64, 0, 9, 0.75f, 0.9f, 0.08f, false, 0.56f},
  // Boss: C minor, half-time heavy taiko, choir + low brass, galloping bass
  {120, 4, 4, 1, 36, Aeolian, {{0, 5, 6, 0}, {0, 3, 5, 6}, {0, 5, 2, 6}, {0, 6, 5, 4}}, 60, -2, 8, 0.45f, 0.9f, 0.12f, false, 0.52f},
};

inline int degMidi(const Style& st, int base, int deg) {
  int oct = deg >= 0 ? deg / 7 : -((6 - deg) / 7);
  return base + 12 * oct + kScale[st.scale][deg - 7 * oct];
}
inline bool isChordTone(int deg, int chord) { int r = posmod(deg - chord, 7); return r == 0 || r == 2 || r == 4; }
int nearestChordTone(int deg, int chord, uint32_t& rng, bool rootOnly = false) {
  int sign = (xs32(rng) & 1) ? 1 : -1;
  for (int off = 0; off <= 3; off++)
    for (int k = 0; k < 2; k++) {
      int x = deg + (k ? -sign : sign) * off;
      if (rootOnly ? posmod(x - chord, 7) == 0 : isChordTone(x, chord)) return x;
    }
  return deg;
}
// MIDI note of chord tone j (0 root, 1 third, 2 fifth) placed inside [lo, lo + 12)
inline int chordWin(const Style& st, int chord, int j, int lo) {
  int m = degMidi(st, st.root, chord + 2 * j);
  while (m < lo) m += 12;
  while (m >= lo + 12) m -= 12;
  return m;
}
inline int bassNote(const Style& st, int deg) {  // keeps the bass within a fifth below .. a fifth above root
  int m = degMidi(st, st.root, deg);
  while (m > st.root + 7) m -= 12;
  while (m < st.root - 5) m += 12;
  return m;
}

// Rhythm cells for one beat (onsets / lengths in sixteenths). span 2 = a two-beat note.
struct Cell { uint8_t n, span; uint8_t on[4], len[4]; };
const Cell kCells4[] = {
  {1, 1, {0}, {4}},                    // quarter
  {2, 1, {0, 2}, {2, 2}},              // two eighths
  {2, 1, {0, 3}, {3, 1}},              // dotted eighth + sixteenth
  {3, 1, {0, 2, 3}, {2, 1, 1}},        // eighth + two sixteenths
  {3, 1, {0, 1, 2}, {1, 1, 2}},        // two sixteenths + eighth
  {4, 1, {0, 1, 2, 3}, {1, 1, 1, 1}},  // four sixteenths
  {1, 1, {2}, {2}},                    // off-beat eighth
  {0, 1, {}, {}},                      // rest
  {1, 2, {0}, {8}},                    // half note
};
const Cell kCells6[] = {
  {1, 1, {0}, {6}},               // dotted quarter
  {2, 1, {0, 4}, {4, 2}},         // quarter + eighth (the jig lilt)
  {3, 1, {0, 2, 4}, {2, 2, 2}},   // three eighths
  {2, 1, {0, 2}, {2, 4}},         // eighth + quarter
  {3, 1, {0, 3, 4}, {3, 1, 2}},   // dotted eighth, sixteenth, eighth
  {0, 1, {}, {}},                 // rest
  {1, 2, {0}, {12}},              // dotted half
};

int pickWeighted(const float* w, int n, uint32_t& rng) {
  float sum = 0;
  for (int i = 0; i < n; i++) sum += w[i];
  float r = unit(rng) * sum;
  for (int i = 0; i < n; i++) {
    if (r < w[i]) return i;
    r -= w[i];
  }
  return n - 1;
}
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

enum class Audio::Inst : uint8_t {
  Choir, Vox, Strings, StringsDark, Horn, Flute, Fiddle, Bell, Star, Lute, Harp, PluckBass, BowBass, Cello,
  Drone, DriveBass, Stab, Taiko, Frame, Snare, Shaker, Tom, Timpani, Cymbal, Drip, Rumble,
};

// ------------------------------------------------------------------------------------------ device
namespace {
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
  if (stream_) return true;
  if (!SDL_WasInit(SDL_INIT_AUDIO) && !SDL_InitSubSystem(SDL_INIT_AUDIO)) {
    SDL_Log("audio unavailable: %s", SDL_GetError());
    return false;
  }
  SDL_AudioSpec spec{SDL_AUDIO_F32, 1, (int)FS};
  stream_ = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, streamCb, this);
  if (!stream_) {
    SDL_Log("audio unavailable: %s", SDL_GetError());
    return false;
  }
  SDL_ResumeAudioStreamDevice(stream_);
  return true;
}

void Audio::shutdown() {
  if (stream_) { SDL_DestroyAudioStream(stream_); stream_ = nullptr; }  // stops the callback before returning
}

void Audio::play(Sfx s, float pitch, float vol) {
  if ((int)s >= (int)Sfx::COUNT || !(vol > 0.0f)) return;
  if (!(pitch > 0.0f)) pitch = 1.0f;
  Event e{s, clampf(pitch, 0.25f, 4.0f), clampf(vol, 0.0f, 2.0f)};
  while (qLock_.test_and_set(std::memory_order_acquire)) {}
  uint32_t head = qHead_.load(std::memory_order_relaxed);
  if (head - qTail_.load(std::memory_order_acquire) < (uint32_t)QN) {   // full queue: drop (never block)
    q_[head % QN] = e;
    qHead_.store(head + 1, std::memory_order_release);
  }
  qLock_.clear(std::memory_order_release);
}

void Audio::setMusic(Music m) {
  if ((int)m < (int)Music::COUNT) wantMusic_.store((int)m, std::memory_order_relaxed);
}

float Audio::rnd() { return unit(rng_); }

// ------------------------------------------------------------------------------------------ voices
void Audio::add(const Note& src) {
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

void Audio::renderVoice(Voice& v, int n, const float* layerGain, float* sfx, float* mus, float* sndS, float* sndM) {
  Note& p = v.n;
  int s0 = 0;
  if (p.delay > 0) {
    int ds = (int)(p.delay * FS);
    if (ds >= n) { p.delay -= (float)n * DT; return; }
    s0 = ds;
    p.delay = 0;
  }
  const int cnt = n - s0;
  const float t1 = v.t + (float)cnt * DT, tm = v.t + 0.5f * (float)cnt * DT;

  // control rate: pitch
  float f = p.freq;
  if (p.slide != 0) f *= std::exp2(p.slide * tm);
  if (p.pEnv != 0) f *= 1.0f + p.pEnv * std::exp(-tm / p.pDec);
  if (p.vibDepth != 0) {
    float fade = p.vibDelay > 0 ? clampf((tm - p.vibDelay) / p.vibDelay, 0.0f, 1.0f) : 1.0f;
    f *= std::exp2(p.vibDepth * (1.0f / 12.0f) * fade * sinc(p.vibRate * tm));
  }
  f = clampf(f, 1.0f, 20000.0f);
  const float inc = f * DT;

  // control rate: amplitude (ramped linearly across the block)
  const float env = NB::envAt(p, t1);
  float amp = p.vol * env;
  const bool done = t1 > p.atk && (env < 1e-3f || amp < 2e-4f) && (p.sus <= 0 || t1 > p.hold);  // -60 dB or inaudible
  if (p.amDepth > 0) amp *= 1.0f - p.amDepth * (0.5f + 0.5f * sinc(p.amRate * t1));
  if (p.bus == 1 || p.bus == 2) amp *= layerGain[p.bus - 1];
  const float g1 = done ? 0.0f : amp;
  float g = v.gain;
  const float dg = (g1 - g) / (float)cnt;

  // oscillator
  float osc[BLOCK];
  switch (p.wave) {
    case Sine:
      for (int i = 0; i < cnt; i++) { osc[i] = sinc(v.ph[0]); v.ph[0] += inc; }
      v.ph[0] -= std::floor(v.ph[0]);
      break;
    case Tri:
      for (int i = 0; i < cnt; i++) {
        osc[i] = 4.0f * std::fabs(v.ph[0] - 0.5f) - 1.0f;
        v.ph[0] += inc;
        if (v.ph[0] >= 1.0f) v.ph[0] -= 1.0f;
      }
      break;
    case Saw:
      for (int i = 0; i < cnt; i++) {
        osc[i] = 2.0f * v.ph[0] - 1.0f - blep(v.ph[0], inc);
        v.ph[0] += inc;
        if (v.ph[0] >= 1.0f) v.ph[0] -= 1.0f;
      }
      break;
    case Square: {
      const float pw = clampf(p.pw, 0.05f, 0.95f);
      for (int i = 0; i < cnt; i++) {
        float ph = v.ph[0], ph2 = ph + 1.0f - pw;
        if (ph2 >= 1.0f) ph2 -= 1.0f;
        osc[i] = (ph < pw ? 1.0f : -1.0f) + blep(ph, inc) - blep(ph2, inc);
        v.ph[0] += inc;
        if (v.ph[0] >= 1.0f) v.ph[0] -= 1.0f;
      }
      break;
    }
    case Noise:
      if (p.color > 0) {   // one-pole lowpass before the main filter: steeper skirts, less hiss
        const float a = 1.0f - clampf(p.color, 0.0f, 0.98f);
        for (int i = 0; i < cnt; i++) { v.nlp += a * (white(v.rng) - v.nlp); osc[i] = v.nlp; }
      } else {
        for (int i = 0; i < cnt; i++) osc[i] = white(v.rng);
      }
      break;
    case Fm: {
      const float idx = p.fmIndex * std::exp(-tm / p.fmDec) * (1.0f / TAU), minc = inc * p.fmRatio;
      for (int i = 0; i < cnt; i++) {
        osc[i] = sinc(v.ph[0] + idx * sinc(v.mph));
        v.ph[0] += inc;
        v.mph += minc;
      }
      v.ph[0] -= std::floor(v.ph[0]);
      v.mph -= std::floor(v.mph);
      break;
    }
    case Pad: {
      const float incs[3] = {inc * (1.0f - p.detune), inc, inc * (1.0f + p.detune)};
      for (int i = 0; i < cnt; i++) {
        float s = 0;
        for (int k = 0; k < 3; k++) {
          s += 2.0f * v.ph[k] - 1.0f - blep(v.ph[k], incs[k]);
          v.ph[k] += incs[k];
          if (v.ph[k] >= 1.0f) v.ph[k] -= 1.0f;
        }
        osc[i] = s * 0.45f;
      }
      break;
    }
    case Pluck: {
      float* buf = str_[v.str];
      const float h0 = 1.0f - v.ksH1;
      for (int i = 0; i < cnt; i++) {
        float cur = buf[v.ksPos];
        float lpf = h0 * cur + v.ksH1 * v.ksPrev;
        v.ksPrev = cur;
        float y = v.ksC * lpf + v.apX - v.ksC * v.apY;   // first-order allpass: fractional tuning
        v.apX = lpf;
        v.apY = y;
        buf[v.ksPos] = y * v.ksRho;
        if (++v.ksPos >= v.ksLen) v.ksPos = 0;
        osc[i] = cur;
      }
      break;
    }
  }
  if (p.breath > 0)
    for (int i = 0; i < cnt; i++) osc[i] += p.breath * white(v.rng);

  // filter (Simper/Cytomic trapezoidal SVF; stable under fast modulation)
  if (p.filt != Off) {
    float oct = p.fSlide * tm;
    if (p.fEnv != 0) oct += p.fEnv * std::exp(-tm / p.fDec);
    if (p.fBump != 0) oct += p.fBump * sinc(0.5f * std::fmin(tm / p.hold, 1.0f));
    const float fc = clampf(p.cutoff * std::exp2(oct), 20.0f, 20000.0f);
    const float gg = std::tan(PI * fc / FS), k = 1.0f / std::fmax(p.q, 0.1f);
    const float a1 = 1.0f / (1.0f + gg * (gg + k)), a2 = gg * a1, a3 = gg * a2;
    for (int i = 0; i < cnt; i++) {
      float x = osc[i];
      float v3 = x - v.ic2;
      float v1 = a1 * v.ic1 + a2 * v3;
      float v2 = v.ic2 + a2 * v.ic1 + a3 * v3;
      v.ic1 = 2.0f * v1 - v.ic1;
      v.ic2 = 2.0f * v2 - v.ic2;
      switch (p.filt) {
        case LP: osc[i] = v2; break;
        case BP: osc[i] = k * v1; break;
        case HP: osc[i] = x - k * v1 - v2; break;
        default: osc[i] = v2 + 0.8f * v1; break;   // Vox
      }
    }
  }
  if (p.drive > 0) {
    const float pre = 1.0f + p.drive;
    for (int i = 0; i < cnt; i++) osc[i] = softSat(osc[i] * pre);
  }

  float* bus = p.bus == 0 ? sfx : mus;
  float* snd = p.bus == 0 ? sndS : sndM;
  for (int i = 0; i < cnt; i++) {
    g += dg;
    float y = osc[i] * g;
    bus[s0 + i] += y;
    snd[s0 + i] += y * p.send;
  }
  v.t = t1;
  v.gain = g1;
  if (done) {
    v.on = false;
    if (v.str >= 0) { strUsed_[v.str] = false; v.str = -1; }
  }
}

// ------------------------------------------------------------------------------------------ sound effects
// Per-effect trim, calibrated with tools/audio_preview so single effects peak below the limiter (0.8 at
// master 0.6): impacts ~0.7, spells/fanfares ~0.45-0.65, pickups ~0.35, UI and footsteps deliberately soft.
const float kSfxTrim[(int)Sfx::COUNT] = {
  1.3f,  0.87f, 0.68f, 0.73f, 0.88f, 0.87f, 0.82f, 0.76f,   // Swing Hit HitHeavy Block Hurt PlayerHurt EnemyDie Death
  1.08f, 1.0f,  0.72f, 0.65f, 0.95f, 1.0f,  1.35f, 1.0f,    // Arrow ArrowHit Fireball Explode Frost Heal Roll Step
  1.6f,  2.0f,  1.0f,  0.95f, 1.7f,  1.0f,                  // Pickup Coin Chest Door Buy Talk
  0.88f, 1.15f, 0.9f,  0.8f,                                // LevelUp QuestStart QuestDone Discover
  1.0f,  1.0f,  1.0f,                                       // MenuMove MenuSelect MenuBack
  0.67f, 1.1f,                                              // Roar Splash
};

void Audio::trigger(Sfx s, float k, float V) {
  V *= kSfxTrim[(int)s];
  switch (s) {
    case Sfx::Swing: {   // band-passed noise whose centre sweeps up and back down: a blade cutting air
      float r = k * rnd(0.9f, 1.12f);
      add(NB(Noise, 0, 2.0f * V).env(0.03f, 1, 1, 0.14f, 0.035f).bp(520 * r, 1.6f).bump(1.8f).color(0.8f));
      add(NB(Noise, 0, 0.05f * V).env(0.05f, 1, 1, 0.11f, 0.03f).hp(3800 * r).bump(0.6f));
      add(NB(Sine, 190 * r, 0.12f * V).env(0.02f, 1, 1, 0.1f, 0.03f).slide(-1.5f));
      break;
    }
    case Sfx::Hit: {     // thump + crunch + crack
      float r = k * rnd(0.92f, 1.08f);
      add(NB(Sine, 118 * r, 0.85f * V).perc(0.085f).pitch(2.5f, 0.018f));
      add(NB(Noise, 0, 0.75f * V).perc(0.03f).lp(3000 * r, 0.9f));
      add(NB(Square, 82 * r, 0.30f * V).perc(0.06f).lp(900 * r, 1.2f).fenv(1.5f, 0.02f).drive(1.5f));
      add(NB(Noise, 0, 0.30f * V).perc(0.008f).hp(5000));
      break;
    }
    case Sfx::HitHeavy: {
      float r = k * rnd(0.94f, 1.06f);
      add(NB(Sine, 74 * r, 0.95f * V).perc(0.2f).pitch(3, 0.03f).drive(0.5f));
      add(NB(Noise, 0, 0.85f * V).perc(0.07f).lp(2200 * r, 0.9f).fenv(1, 0.03f));
      add(NB(Saw, 52 * r, 0.45f * V).perc(0.13f).lp(600 * r).fenv(2, 0.03f).drive(2));
      add(NB(Noise, 0, 0.40f * V).perc(0.012f).hp(4000));
      add(NB(Noise, 0, 0.30f * V).env(0.01f, 0.22f).lp(380).send(0.3f).at(0.01f));
      break;
    }
    case Sfx::Block: {   // inharmonic FM partials: steel on steel
      float r = k * rnd(0.97f, 1.03f);
      add(NB(Fm, 640 * r, 0.42f * V).perc(0.35f).fm(1.414f, 3.5f, 0.06f).send(0.3f));
      add(NB(Fm, 1490 * r, 0.28f * V).perc(0.22f).fm(2.76f, 2.0f, 0.05f).send(0.3f));
      add(NB(Sine, 2650 * r, 0.12f * V).perc(0.12f));
      add(NB(Noise, 0, 0.55f * V).perc(0.012f).hp(3000, 0.9f));
      add(NB(Sine, 180 * r, 0.35f * V).perc(0.04f).pitch(1, 0.01f));
      break;
    }
    case Sfx::Hurt: {    // enemy: short formant grunt on a meaty thud
      float r = k * rnd(0.9f, 1.12f);
      add(NB(Saw, 230 * r, 0.42f * V).env(0.004f, 0.11f).slide(-1.6f).vox(900 * r, 2.5f).drive(1));
      add(NB(Noise, 0, 0.55f * V).perc(0.04f).lp(2000 * r));
      add(NB(Sine, 130 * r, 0.65f * V).perc(0.07f).pitch(1.5f, 0.015f));
      break;
    }
    case Sfx::PlayerHurt: {   // heavier thud + "oof" + a dissonant sting so the player always notices
      add(NB(Sine, 150 * k, 0.85f * V).perc(0.1f).pitch(2, 0.02f));
      add(NB(Noise, 0, 0.6f * V).perc(0.05f).lp(2600 * k));
      add(NB(Square, 300 * k, 0.30f * V).env(0.003f, 0.15f).slide(-1.2f).pw(0.3f).lp(1800 * k, 1.5f).drive(0.5f));
      add(NB(Tri, 932 * k, 0.17f * V).perc(0.14f).vib(0.3f, 14));
      add(NB(Tri, 988 * k, 0.17f * V).perc(0.14f).vib(0.3f, 14));
      break;
    }
    case Sfx::EnemyDie: {   // rattling groan falls away, then the body hits the ground
      add(NB(Saw, 200 * k, 0.42f * V).env(0.005f, 0.32f).slide(-1.1f).vox(800 * k, 2).vib(0.6f, 28).drive(1).send(0.2f));
      add(NB(Noise, 0, 0.40f * V).env(0.02f, 0.2f).lp(700 * k).send(0.2f));
      add(NB(Sine, 70 * k, 0.85f * V).perc(0.16f).pitch(1.5f, 0.03f).at(0.14f));
      add(NB(Noise, 0, 0.45f * V).perc(0.05f).lp(1500).at(0.14f));
      break;
    }
    case Sfx::Death: {    // boom, a falling D-minor choir and a low bell toll
      add(NB(Sine, 55 * k, 1.0f * V).perc(0.9f).pitch(2, 0.05f));
      add(NB(Noise, 0, 0.5f * V).env(0.005f, 0.5f).lp(500 * k).fenv(2, 0.1f).send(0.3f));
      const float chord[4] = {146.8f, 174.6f, 220.0f, 293.7f};
      for (float c : chord)
        add(NB(Pad, c * k, 0.13f * V).detune(0.006f).env(0.06f, 1, 1, 1.3f, 0.9f).slide(-0.12f).vox(1200, 1.2f)
                .fslide(-0.8f).vib(0.15f, 4.5f).send(0.55f).at(0.05f));
      add(NB(Fm, 73.4f * k, 0.5f * V).perc(1.4f).fm(1.4f, 2.5f, 0.5f).send(0.5f).at(0.05f));
      break;
    }
    case Sfx::Arrow: {    // bowstring twang, then the shaft whistling away
      add(NB(Pluck, 196 * k, 0.45f * V).bright(0.75f).ring(0.3f).env(0.001f, 1, 1, 0.25f, 0.06f));
      add(NB(Saw, 310 * k, 0.22f * V).perc(0.07f).slide(-1.5f).lp(2400 * k, 2));
      add(NB(Noise, 0, 0.80f * V).env(0.012f, 1, 1, 0.17f, 0.04f).bp(1800 * k, 1.5f).bump(1.2f).at(0.015f));
      break;
    }
    case Sfx::ArrowHit: { // thunk into wood / flesh, with a short shaft quiver
      float r = k * rnd(0.93f, 1.07f);
      add(NB(Sine, 210 * r, 0.75f * V).perc(0.045f).pitch(1.2f, 0.01f));
      add(NB(Noise, 0, 0.55f * V).perc(0.025f).lp(1600 * r));
      add(NB(Noise, 0, 1.6f * V).perc(0.07f).bp(760 * r, 6));
      add(NB(Tri, 420 * r, 0.10f * V).perc(0.12f).vib(1.5f, 32));
      break;
    }
    case Sfx::Fireball: { // roaring whoosh with a fiery rumble and crackle
      add(NB(Noise, 0, 1.0f * V).env(0.06f, 1, 1, 0.42f, 0.22f).lp(450 * k, 1.1f).bump(2.2f).color(0.6f).drive(0.6f).send(0.3f));
      add(NB(Saw, 65 * k, 0.35f * V).env(0.08f, 1, 1, 0.4f, 0.2f).slide(0.7f).lp(500, 0.9f).am(24, 0.5f).drive(1));
      add(NB(Noise, 0, 0.28f * V).env(0.03f, 1, 1, 0.35f, 0.15f).hp(2500).am(37, 0.8f));
      break;
    }
    case Sfx::Explode: {
      add(NB(Sine, 58 * k, 1.0f * V).perc(0.4f).pitch(2.5f, 0.04f).drive(0.5f));
      add(NB(Noise, 0, 1.0f * V).env(0.002f, 0.45f).lp(350 * k, 0.8f).fenv(3.5f, 0.15f).drive(1).send(0.35f));
      add(NB(Noise, 0, 0.5f * V).perc(0.015f).hp(3000));
      add(NB(Noise, 0, 0.45f * V).env(0.05f, 0.8f).lp(250).send(0.5f).at(0.03f));
      break;
    }
    case Sfx::Frost: {    // cascade of glassy FM bells over an icy, shimmering hiss
      const float fr[6] = {2093, 2637, 3136, 3520, 2794, 3951};
      for (int i = 0; i < 6; i++)
        add(NB(Fm, fr[i] * k, 0.15f * V).perc(0.28f).fm(2.0f, 1.2f, 0.1f).send(0.55f).at(0.035f * (float)i));
      add(NB(Noise, 0, 0.16f * V).env(0.04f, 1, 1, 0.25f, 0.12f).hp(6000).am(19, 0.6f).send(0.5f));
      add(NB(Sine, 1046 * k, 0.15f * V).env(0.01f, 0.4f).vib(0.3f, 9));
      break;
    }
    case Sfx::Heal: {     // rising major-pentatonic sparkle over a soft swell
      const float fr[6] = {523.3f, 659.3f, 784.0f, 1046.5f, 1318.5f, 1568.0f};
      for (int i = 0; i < 6; i++) {
        add(NB(Sine, fr[i] * k, 0.17f * V).env(0.004f, 0.35f).vib(0.15f, 6).send(0.55f).at(0.055f * (float)i));
        add(NB(Sine, fr[i] * 2 * k, 0.05f * V).perc(0.2f).send(0.55f).at(0.055f * (float)i));
      }
      add(NB(Tri, 523.3f * k, 0.15f * V).env(0.08f, 1, 1, 0.35f, 0.25f).slide(0.6f).send(0.5f));
      break;
    }
    case Sfx::Roll: {     // cloth-and-dirt tumble with two soft bumps
      add(NB(Noise, 0, 0.75f * V).env(0.02f, 1, 1, 0.22f, 0.08f).lp(1300 * k, 0.8f).bump(0.9f).color(0.5f));
      add(NB(Sine, 95 * k, 0.40f * V).perc(0.05f).pitch(1, 0.01f).at(0.04f));
      add(NB(Sine, 85 * k, 0.32f * V).perc(0.05f).pitch(1, 0.01f).at(0.2f));
      break;
    }
    case Sfx::Step: {     // very soft scuff + thud; randomised so repeats never sound mechanical
      float r = k * rnd(0.85f, 1.15f);
      add(NB(Noise, 0, 0.30f * V).perc(0.02f).bp(1500 * r, 0.8f).send(0.02f));            // grit: what phones hear
      add(NB(Noise, 0, 0.22f * V).perc(0.03f).lp(700 * r, 0.8f).send(0.02f));             // soft scuff
      add(NB(Sine, 105 * r, 0.16f * V).perc(0.025f).pitch(0.8f, 0.008f).send(0.02f));    // weight
      break;
    }
    case Sfx::Pickup: {
      add(NB(Sine, 620 * k, 0.35f * V).perc(0.07f).slide(2.5f));
      add(NB(Tri, 1318.5f * k, 0.22f * V).perc(0.12f).at(0.05f));
      add(NB(Tri, 1760 * k, 0.20f * V).perc(0.18f).at(0.1f).send(0.3f));
      break;
    }
    case Sfx::Coin: {     // classic two-note ding (B5 -> E6) with a metallic shimmer
      add(NB(Square, 987.8f * k, 0.15f * V).env(0.002f, 0.06f).lp(5000));
      add(NB(Square, 1318.5f * k, 0.15f * V).perc(0.3f).lp(6000).at(0.07f).send(0.25f));
      add(NB(Fm, 2637 * k, 0.10f * V).perc(0.15f).fm(3.5f, 1.5f, 0.05f).at(0.07f));
      break;
    }
    case Sfx::Chest: {    // hinge creak, lid thunk, then the treasure jingles
      add(NB(Saw, 120 * k, 0.32f * V).env(0.04f, 1, 1, 0.32f, 0.08f).slide(0.5f).vox(900 * k, 4).am(26, 0.75f));
      add(NB(Sine, 90 * k, 0.6f * V).perc(0.12f).pitch(1, 0.02f).at(0.34f));
      add(NB(Noise, 0, 0.4f * V).perc(0.05f).lp(700).at(0.34f));
      const float jf[5] = {1568, 2093, 2349, 2637, 3136};
      for (int i = 0; i < 5; i++)
        add(NB(Fm, jf[i] * k, 0.12f * V).perc(0.2f).fm(3.5f, 1.2f, 0.06f).send(0.4f).at(0.4f + 0.05f * (float)i));
      const float cf[3] = {1046.5f, 1318.5f, 1568.0f};
      for (float c : cf) add(NB(Sine, c * k, 0.08f * V).env(0.01f, 0.6f).send(0.6f).at(0.58f));
      break;
    }
    case Sfx::Door: {     // slow creak, then the heavy wooden thud
      add(NB(Saw, 90 * k, 0.32f * V).env(0.06f, 1, 1, 0.42f, 0.1f).slide(0.8f).vox(700 * k, 5).am(19, 0.7f).drive(0.5f));
      add(NB(Sine, 68 * k, 0.9f * V).perc(0.2f).pitch(1.5f, 0.02f).at(0.42f).send(0.3f));
      add(NB(Noise, 0, 0.5f * V).perc(0.08f).lp(500).at(0.42f).send(0.3f));
      break;
    }
    case Sfx::Buy: {      // coins tumbling into a purse + a bright "sold" ping
      for (int i = 0; i < 3; i++)
        add(NB(Fm, (2400 + 260 * (float)i) * k * rnd(0.97f, 1.03f), 0.13f * V).perc(0.09f).fm(3.5f, 1.6f, 0.04f)
                .at(0.045f * (float)i));
      add(NB(Square, 1568 * k, 0.12f * V).perc(0.25f).lp(5000).at(0.14f));
      add(NB(Fm, 2093 * k, 0.12f * V).perc(0.4f).fm(2, 1, 0.1f).at(0.14f).send(0.35f));
      break;
    }
    case Sfx::Talk: {     // soft vowel-ish blip; caller's pitch = speaker voice, small random inflection
      float f = 320 * k * rnd(0.94f, 1.08f);
      add(NB(Square, f, 0.15f * V).env(0.004f, 0.045f).pw(0.3f).vox(900 * rnd(0.85f, 1.2f), 1.8f).send(0.05f));
      break;
    }
    case Sfx::LevelUp: {  // brass fanfare G-C-E-G up to a held C over a strings chord, timpani and sparkle
      const float run[4] = {392.0f, 523.3f, 659.3f, 784.0f};
      for (int i = 0; i < 4; i++) {
        float t = 0.11f * (float)i;
        add(NB(Saw, run[i] * k, 0.22f * V).env(0.01f, 0.3f, 0.6f, 0.09f, 0.06f).lp(1300, 1.1f).fenv(1.2f, 0.08f).at(t).send(0.3f));
      }
      add(NB(Saw, 1046.5f * k, 0.22f * V).env(0.02f, 0.4f, 0.7f, 0.85f, 0.35f).lp(1500, 1.1f).fenv(1, 0.15f)
              .vib(0.2f, 5.5f, 0.25f).at(0.44f).send(0.45f));
      add(NB(Square, 523.3f * k, 0.08f * V).env(0.02f, 0.4f, 0.7f, 0.85f, 0.35f).pw(0.3f).lp(2200).at(0.44f).send(0.4f));
      const float ch[3] = {261.6f, 329.6f, 392.0f};
      for (float c : ch) add(NB(Pad, c * k, 0.11f * V).detune(0.007f).lp(2000).env(0.05f, 1, 1, 0.9f, 0.5f).at(0.44f).send(0.5f));
      add(NB(Sine, 65.4f * k, 0.7f * V).perc(0.35f).pitch(0.3f, 0.04f));
      add(NB(Sine, 65.4f * k, 0.8f * V).perc(0.6f).pitch(0.3f, 0.04f).at(0.44f).send(0.3f));
      for (int i = 0; i < 4; i++)
        add(NB(Sine, 2093.0f * k * std::exp2((float)i * 4.0f / 12.0f), 0.06f * V).perc(0.3f).at(0.5f + 0.06f * (float)i).send(0.6f));
      break;
    }
    case Sfx::QuestStart: {  // nordic horn call: D up to A, low horn doubling, frame drum
      add(NB(Saw, 293.7f * k, 0.25f * V).env(0.04f, 0.4f, 0.75f, 0.32f, 0.12f).lp(700, 1.1f).fenv(1.2f, 0.12f).send(0.5f));
      add(NB(Saw, 440.0f * k, 0.25f * V).env(0.05f, 0.5f, 0.75f, 0.75f, 0.35f).lp(800, 1.1f).fenv(1.2f, 0.15f)
              .vib(0.15f, 5, 0.3f).at(0.36f).send(0.55f));
      add(NB(Saw, 146.8f * k, 0.18f * V).env(0.06f, 0.5f, 0.8f, 1.05f, 0.4f).lp(450).send(0.5f));
      add(NB(Sine, 98 * k, 0.6f * V).perc(0.15f).pitch(0.9f, 0.02f));
      add(NB(Sine, 98 * k, 0.6f * V).perc(0.15f).pitch(0.9f, 0.02f).at(0.36f));
      break;
    }
    case Sfx::QuestDone: {   // "da-da-da DAAA": G-G-G then a full C-major brass chord, bells, timpani roll
      for (int i = 0; i < 3; i++)
        add(NB(Saw, 392.0f * k, 0.2f * V).env(0.008f, 0.2f, 0.6f, 0.08f, 0.05f).lp(1400, 1.1f).fenv(1, 0.06f)
                .at(0.12f * (float)i).send(0.3f));
      const float ch[3] = {523.3f, 659.3f, 784.0f};
      for (float c : ch)
        add(NB(Saw, c * k, 0.15f * V).env(0.03f, 0.5f, 0.75f, 1.0f, 0.5f).lp(1500, 1.0f).fenv(1, 0.15f)
                .vib(0.15f, 5.2f, 0.3f).at(0.36f).send(0.5f));
      add(NB(Pad, 130.8f * k, 0.15f * V).detune(0.006f).lp(900).env(0.05f, 1, 1, 1.0f, 0.6f).at(0.36f).send(0.4f));
      for (int i = 0; i < 6; i++)
        add(NB(Sine, 65.4f * k, (0.18f + 0.06f * (float)i) * V).perc(0.12f).pitch(0.3f, 0.03f).at(0.04f * (float)i));
      add(NB(Sine, 65.4f * k, 0.8f * V).perc(0.7f).pitch(0.3f, 0.04f).at(0.36f).send(0.3f));
      const float bells[4] = {1568, 2093, 2637, 3136};
      for (int i = 0; i < 4; i++) add(NB(Fm, bells[i] * k, 0.08f * V).perc(0.5f).fm(3, 1, 0.2f).at(0.45f + 0.08f * (float)i).send(0.6f));
      break;
    }
    case Sfx::Discover: {    // majestic two-chord swell (Bb -> D major) with timpani and a high bell
      const float c1[4] = {116.5f, 174.6f, 233.1f, 293.7f};
      const float c2[4] = {146.8f, 220.0f, 293.7f, 370.0f};
      for (float c : c1) add(NB(Pad, c * k, 0.11f * V).detune(0.006f).vox(900, 1.2f).env(0.12f, 1, 1, 0.45f, 0.25f).send(0.6f));
      for (float c : c2)
        add(NB(Pad, c * k, 0.12f * V).detune(0.006f).vox(1000, 1.2f).env(0.15f, 1, 1, 1.5f, 0.9f).vib(0.1f, 4.8f, 0.5f)
                .at(0.55f).send(0.65f));
      add(NB(Saw, 293.7f * k, 0.14f * V).env(0.1f, 0.5f, 0.8f, 1.4f, 0.7f).lp(800, 1.1f).fenv(1, 0.2f).at(0.55f).send(0.55f));
      add(NB(Sine, 58.3f * k, 0.75f * V).perc(0.5f).pitch(0.3f, 0.05f).send(0.3f));
      add(NB(Sine, 73.4f * k, 0.9f * V).perc(0.8f).pitch(0.3f, 0.05f).at(0.55f).send(0.4f));
      add(NB(Fm, 587.3f * k, 0.12f * V).perc(1.2f).fm(3.5f, 1.4f, 0.4f).at(0.6f).send(0.7f));
      add(NB(Fm, 880.0f * k, 0.08f * V).perc(1.0f).fm(3.5f, 1.2f, 0.4f).at(0.72f).send(0.7f));
      break;
    }
    case Sfx::MenuMove:
      add(NB(Sine, 1100 * k, 0.16f * V).perc(0.03f));
      add(NB(Noise, 0, 0.06f * V).perc(0.004f).hp(4000));
      break;
    case Sfx::MenuSelect:
      add(NB(Tri, 660 * k, 0.24f * V).perc(0.07f));
      add(NB(Tri, 990 * k, 0.24f * V).perc(0.12f).at(0.05f).send(0.15f));
      add(NB(Sine, 1980 * k, 0.06f * V).perc(0.1f).at(0.05f));
      break;
    case Sfx::MenuBack:
      add(NB(Tri, 620 * k, 0.22f * V).perc(0.07f));
      add(NB(Tri, 415 * k, 0.22f * V).perc(0.1f).at(0.05f));
      break;
    case Sfx::Roar: {     // dragon: growling saws with a rise-and-fall contour, rasping breath, sub
      add(NB(Saw, 52 * k, 0.5f * V).env(0.18f, 1, 1, 1.4f, 0.5f).vib(3, 0.35f).lp(420, 1.4f).bump(1.6f).am(27, 0.55f).drive(1.4f).send(0.45f));
      add(NB(Saw, 78.5f * k, 0.36f * V).env(0.2f, 1, 1, 1.35f, 0.5f).vib(3, 0.35f).lp(620, 1.2f).bump(1.4f).am(31, 0.5f).drive(1.2f).send(0.45f));
      add(NB(Noise, 0, 0.65f * V).env(0.15f, 1, 1, 1.3f, 0.5f).bp(480 * k, 1.1f).bump(1.5f).am(23, 0.4f).color(0.7f).send(0.45f));
      add(NB(Saw, 104 * k, 0.3f * V).env(0.22f, 1, 1, 1.3f, 0.45f).vib(3, 0.35f).vox(720 * k, 3).bump(0.8f).am(27, 0.6f).drive(1.5f).send(0.45f));  // throat
      add(NB(Sine, 38 * k, 0.45f * V).env(0.2f, 1, 1, 1.3f, 0.6f).vib(3, 0.35f));
      break;
    }
    case Sfx::Splash: {   // body of water, spray, then a few bubbles
      add(NB(Noise, 0, 0.9f * V).env(0.003f, 0.18f).lp(3500 * k, 0.7f).send(0.3f));
      add(NB(Noise, 0, 0.7f * V).env(0.02f, 0.35f).bp(1100 * k, 0.8f).at(0.02f).send(0.3f));
      for (int i = 0; i < 4; i++)
        add(NB(Sine, rnd(500, 1100) * k, 0.18f * V).perc(0.05f).slide(2.5f).at(0.08f + 0.06f * (float)i + rnd(0, 0.03f)));
      break;
    }
    case Sfx::COUNT: break;
  }
}

// ------------------------------------------------------------------------------------------ music: instruments
void Audio::inst(int layer, Inst which, int midi, float dur, float vel, float dly) {
  const float f = mtof(midi);
  vel *= kStyle[(int)seq_[layer].mode].level;
  const int bus = layer + 1;
  switch (which) {
    case Inst::Choir:
      add(NB(Pad, f, 0.06f * vel).detune(0.006f).vox(820, 1.1f).env(0.6f, 1, 1, dur, 0.75f).vib(0.1f, 4.8f, 0.4f).send(0.65f).at(dly).bus(bus));
      break;
    case Inst::Vox:
      add(NB(Pad, f, 0.08f * vel).detune(0.005f).vox(950, 1.6f).env(0.12f, 0.5f, 0.85f, dur, 0.4f).vib(0.18f, 5, 0.25f).send(0.55f).at(dly).bus(bus));
      break;
    case Inst::Strings:
      add(NB(Pad, f, 0.05f * vel).detune(0.007f).lp(1700).env(0.45f, 1, 1, dur, 0.6f).vib(0.08f, 5.5f, 0.3f).send(0.5f).at(dly).bus(bus));
      break;
    case Inst::StringsDark:
      add(NB(Pad, f, 0.06f * vel).detune(0.006f).lp(850).env(0.9f, 1, 1, dur, 1.0f).vib(0.08f, 5, 0.5f).send(0.6f).at(dly).bus(bus));
      break;
    case Inst::Horn:
      add(NB(Saw, f, 0.12f * vel).lp(520, 1.1f).fenv(1.4f, 0.18f).env(0.05f, 0.5f, 0.75f, dur, 0.22f).vib(0.12f, 5, 0.35f).send(0.45f).at(dly).bus(bus));
      break;
    case Inst::Flute:
      add(NB(Tri, f, 0.15f * vel).breath(0.1f).lp(3800).env(0.05f, 0.4f, 0.85f, dur, 0.12f).vib(0.2f, 5.3f, 0.2f).send(0.45f).at(dly).bus(bus));
      break;
    case Inst::Fiddle:
      add(NB(Saw, f, 0.075f * vel).lp(2700, 1.4f).env(0.03f, 0.25f, 0.8f, dur, 0.09f).vib(0.22f, 6, 0.12f).send(0.35f).at(dly).bus(bus));
      break;
    case Inst::Bell:
      add(NB(Fm, f, 0.13f * vel).fm(3.0f, 1.8f, 0.5f).env(0.002f, 0.9f).send(0.7f).at(dly).bus(bus));
      break;
    case Inst::Star:
      add(NB(Sine, f, 0.06f * vel).env(0.003f, 1.1f).send(0.85f).at(dly).bus(bus));
      break;
    case Inst::Lute:
      add(NB(Pluck, f, 0.22f * vel).bright(0.45f).ring(1.0f).lp(3200).env(0.001f, 1, 1, std::fmax(dur, 0.25f), 0.18f).send(0.3f).at(dly).bus(bus));
      break;
    case Inst::Harp:
      add(NB(Pluck, f, 0.17f * vel).bright(0.62f).ring(2.4f).env(0.001f, 1, 1, std::fmax(dur, 0.6f), 0.35f).send(0.6f).at(dly).bus(bus));
      break;
    case Inst::PluckBass:
      add(NB(Pluck, f, 0.32f * vel).bright(0.3f).ring(1.4f).lp(1100).env(0.001f, 1, 1, std::fmax(dur, 0.4f), 0.15f).send(0.12f).at(dly).bus(bus));
      break;
    case Inst::BowBass:
      add(NB(Saw, f, 0.15f * vel).lp(360, 0.9f).env(0.18f, 1, 1, dur, 0.6f).vib(0.05f, 4.5f, 0.5f).send(0.3f).at(dly).bus(bus));
      break;
    case Inst::Cello:
      add(NB(Saw, f, 0.11f * vel).lp(650, 1.1f).env(0.35f, 1, 1, dur, 0.9f).vib(0.15f, 4.8f, 0.4f).send(0.7f).at(dly).bus(bus));
      break;
    case Inst::Drone:
      add(NB(Pad, f, 0.12f * vel).detune(0.004f).lp(300, 1.3f).env(2.5f, 1, 1, dur, 3).am(0.13f, 0.35f).send(0.6f).at(dly).bus(bus));
      break;
    case Inst::DriveBass:
      add(NB(Saw, f, 0.2f * vel).lp(480, 1.2f).fenv(1.6f, 0.05f).env(0.003f, 0.15f, 0.5f, dur, 0.06f).drive(0.8f).send(0.1f).at(dly).bus(bus));
      break;
    case Inst::Stab:
      add(NB(Saw, f, 0.075f * vel).lp(1500).fenv(1, 0.04f).env(0.002f, 0.08f).send(0.25f).at(dly).bus(bus));
      break;
    case Inst::Taiko:
      add(NB(Sine, 62, 0.5f * vel).pitch(1.3f, 0.03f).env(0.001f, 0.3f).send(0.35f).at(dly).bus(bus));
      add(NB(Noise, 0, 0.28f * vel).lp(450, 0.9f).env(0.001f, 0.06f).send(0.3f).at(dly).bus(bus));
      break;
    case Inst::Frame:
      add(NB(Sine, 110, 0.32f * vel).pitch(0.9f, 0.02f).env(0.001f, 0.12f).send(0.25f).at(dly).bus(bus));
      add(NB(Noise, 0, 0.4f * vel).bp(380, 1.1f).env(0.001f, 0.045f).send(0.25f).at(dly).bus(bus));
      break;
    case Inst::Snare:
      add(NB(Noise, 0, 0.45f * vel).bp(1900, 0.8f).env(0.001f, 0.08f).send(0.35f).at(dly).bus(bus));
      add(NB(Tri, 230, 0.2f * vel).pitch(0.6f, 0.02f).env(0.001f, 0.04f).at(dly).bus(bus));
      break;
    case Inst::Shaker:
      add(NB(Noise, 0, 0.14f * vel).hp(6500).env(0.006f, 0.03f).send(0.15f).at(dly).bus(bus));
      break;
    case Inst::Tom:
      add(NB(Sine, f, 0.42f * vel).pitch(0.7f, 0.03f).env(0.001f, 0.18f).send(0.3f).at(dly).bus(bus));
      add(NB(Noise, 0, 0.15f * vel).lp(600).env(0.001f, 0.04f).at(dly).bus(bus));
      break;
    case Inst::Timpani:
      add(NB(Sine, f, 0.38f * vel).pitch(0.12f, 0.06f).env(0.002f, 0.65f).send(0.4f).at(dly).bus(bus));
      add(NB(Fm, f, 0.08f * vel).fm(1.5f, 1.0f, 0.2f).env(0.002f, 0.35f).send(0.4f).at(dly).bus(bus));
      break;
    case Inst::Cymbal:   // reverse-swell into the next section
      add(NB(Noise, 0, 0.08f * vel).hp(5000).env(dur, 1, 1, dur, 0.08f).send(0.4f).at(dly).bus(bus));
      break;
    case Inst::Drip:     // a water drop: tiny sine "plink" whose pitch flicks upward, drowned in echo
      add(NB(Sine, f, 0.14f * vel).slide(2.2f).env(0.001f, 0.045f).send(0.95f).at(dly).bus(bus));
      break;
    case Inst::Rumble:
      add(NB(Noise, 0, 0.5f * vel).lp(110, 0.9f).env(2, 1, 1, 2.5f, 2.5f).send(0.4f).at(dly).bus(bus));
      break;
  }
}

// ------------------------------------------------------------------------------------------ music: composition
void Audio::startSeq(Seq& s, Music m) {
  s = Seq{};
  s.mode = m;
  starts_++;
  s.rng = 0x9E3779B9u * (starts_ + 1u) ^ (0x85EBCA6Bu * ((uint32_t)m + 7u)) ^ rng_;
  if (s.rng == 0) s.rng = 1;
  for (int i = 0; i < 4; i++) xs32(s.rng);
  s.nextStep = 0.05;
}

void Audio::genMotif(Seq& s, Motif& m) {
  const Style& st = kStyle[(int)s.mode];
  const bool six = st.spb == 6;
  const Cell* cells = six ? kCells6 : kCells4;
  const float b = st.busy;
  float w4[9] = {3 - 1.5f * b, 1 + 2 * b, 1.2f, 2 * b, 1.6f * b, 2.2f * b * b, 0.6f, 0.5f, 2.4f * (1 - b)};
  float w6[7] = {2 - b, 2, 1 + 2 * b, 1.2f, 1.2f * b, 0.4f, 1.5f * (1 - b)};
  const float* w = six ? w6 : w4;
  const int nCells = six ? 7 : 9, restCell = six ? 5 : 7, halfCell = six ? 6 : 8;
  const int barSteps = st.beats * st.spb;
  m.n = 0;
  auto push = [&](int on, int len) {
    if (m.n < 32) { m.on[m.n] = (uint8_t)on; m.len[m.n] = (uint8_t)len; m.n++; }
  };
  for (int bar = 0; bar < 2; bar++) {
    int beat = 0;
    while (beat < st.beats) {
      const int base = bar * barSteps + beat * st.spb;
      const bool lastBeat = beat == st.beats - 1;
      if (bar == 1 && beat == st.beats - 2 && unit(s.rng) < 0.6f) { push(base, 2 * st.spb); beat += 2; continue; }  // long close
      if (bar == 1 && lastBeat) { push(base, st.spb); beat++; continue; }
      if (bar == 0 && lastBeat && unit(s.rng) < 0.4f) { push(base, st.spb); beat++; continue; }  // breath mid-phrase
      int c = pickWeighted(w, nCells, s.rng);
      if (c == halfCell && beat > st.beats - 2) c = 0;
      if (bar == 0 && beat == 0 && (c == restCell || cells[c].on[0] != 0)) c = 0;   // phrases start on the beat
      const Cell& cell = cells[c];
      for (int i = 0; i < cell.n; i++) push(base + cell.on[i], cell.len[i]);
      beat += cell.span;
    }
  }
  static const float wIv[9] = {0.25f, 0.5f, 1.3f, 3.0f, 0.0f, 3.0f, 1.3f, 0.6f, 0.35f};  // -4..+4 scale steps
  float wi[9];
  for (int i = 0; i < 9; i++) wi[i] = wIv[i];
  wi[4] = 0.6f + 1.2f * b;   // repeated notes suit the busier dance tunes
  for (int i = 0; i < m.n; i++) m.iv[i] = (int8_t)(pickWeighted(wi, 9, s.rng) - 4);
}

void Audio::compose(Seq& s) {
  const Style& st = kStyle[(int)s.mode];
  const int sec = s.section++;
  const int barSteps = st.beats * st.spb;
  const int idx = sec - (st.intro ? 1 : 0);
  s.variant = posmod(idx < 0 ? 0 : idx, 4);
  s.role = idx < 0 ? 0 : (s.variant == 2 ? 2 : 1);   // 0 intro, 1 A (A, A', A''), 2 B (contrast)

  // harmony: A sections keep (and occasionally change) their progression, B always contrasts it
  auto otherProg = [&](int not_) { int p = (int)(xs32(s.rng) % 3); return p >= not_ ? p + 1 : p; };
  if (sec == 0) s.prog = 0;
  else if (s.role != 2 && unit(s.rng) < 0.3f) s.prog = otherProg(s.prog);
  const int progIdx = s.role == 2 ? otherProg(s.prog) : s.prog;
  const int8_t* pr = st.progs[progIdx];
  const int8_t* alt = st.progs[otherProg(progIdx)];
  const bool altHalf = st.chordBars == 1 && unit(s.rng) < 0.25f;   // fresh second half now and then
  for (int b = 0; b < 8; b++) s.chords[b] = st.chordBars == 2 ? pr[b / 2] : (altHalf && b >= 4 ? alt[b % 4] : pr[b % 4]);

  // melody
  s.melN = s.melI = 0;
  if (st.density <= 0) return;
  Motif& m = s.role == 2 ? s.b : s.a;
  if (s.role == 2) genMotif(s, s.b);
  else if (!s.haveA || (s.variant == 0 && unit(s.rng) < 0.5f)) { genMotif(s, s.a); s.haveA = true; }

  int lo = st.lo, hi = st.hi;
  if (s.role == 2) { lo += 2; hi += 2; }
  const bool epic = s.mode == Music::Title || s.mode == Music::Boss;
  if (s.variant == 3 && epic && unit(s.rng) < 0.6f) { lo += 3; hi += 3; }   // climax: lift the register
  const bool invert = s.variant == 3 && unit(s.rng) < 0.35f;

  const int mid = (lo + hi) / 2;
  int deg = s.cur;
  for (int p = 0; p < 4; p++) {
    bool on = p == 0 ? unit(s.rng) < std::fmax(st.density, 0.75f) : unit(s.rng) < st.density;
    if (s.role == 0 && p < 2) on = false;        // intro: let the bed breathe first
    if (!on) continue;
    const int base = p * 2 * barSteps;
    bool first = true;
    for (int k = 0; k < m.n; k++) {
      const int onStep = m.on[k];
      if (p == 3 && onStep >= barSteps) break;   // last bar becomes the cadence
      const int bar = 2 * p + onStep / barSteps;
      const int chord = s.chords[bar];
      if (first) {   // each phrase re-anchors the motif on a chord tone (natural transposition), drifting home
        const int from = p == 0 ? (deg + 2 * mid) / 3 : (2 * deg + mid) / 3;
        deg = nearestChordTone(from < lo ? lo : (from > hi ? hi : from), chord, s.rng);
        first = false;
      } else {
        int iv = m.iv[k];
        if (invert && p >= 2) iv = -iv;
        if (p == 1 && k >= m.n - 2 && unit(s.rng) < 0.5f) iv = (int)(xs32(s.rng) % 5) - 2;   // vary the answer
        if (unit(s.rng) < 0.12f) iv += (xs32(s.rng) & 1) ? 1 : -1;
        if (((deg > mid + 2 && iv > 0) || (deg < mid - 2 && iv < 0)) && unit(s.rng) < 0.5f) iv = -iv;   // melodic gravity
        deg += iv;
      }
      if (deg > hi) deg = 2 * hi - deg;   // reflect off the range edges instead of sticking to them
      if (deg < lo) deg = 2 * lo - deg;
      deg = deg < lo ? lo : (deg > hi ? hi : deg);
      const int inBar = onStep % barSteps;
      const bool strong = inBar == 0 || (st.beats == 4 && inBar == barSteps / 2);
      if (strong && unit(s.rng) < 0.75f) deg = nearestChordTone(deg, chord, s.rng);
      int len = m.len[k];
      if (s.melN >= 158) break;
      if (len >= 2 * st.spb && st.busy > 0.3f && unit(s.rng) < 0.18f) {   // ornament: split with a passing note
        int half = len / 2;
        s.mel[s.melN++] = {(uint8_t)(base + onStep), (uint8_t)half, (int8_t)deg, (uint8_t)strong};
        int pass = deg + ((xs32(s.rng) & 1) ? 1 : -1);
        if (pass > hi || pass < lo) pass = 2 * deg - pass;
        s.mel[s.melN++] = {(uint8_t)(base + onStep + half), (uint8_t)(len - half), (int8_t)pass, 0};
        deg = pass;
      } else {
        s.mel[s.melN++] = {(uint8_t)(base + onStep), (uint8_t)len, (int8_t)deg, (uint8_t)strong};
      }
    }
    if (p == 3) {   // cadence: approach note, then a long chord root
      const int chord = s.chords[7];
      const int target = nearestChordTone(deg < lo ? lo : (deg > hi ? hi : deg), chord, s.rng, true);
      const int start = 7 * barSteps;
      if (s.melN < 158) {
        int approach = target + ((xs32(s.rng) & 1) ? 1 : -1);
        if (unit(s.rng) < 0.5f) approach = target + 2 * (deg > target ? 1 : -1);
        s.mel[s.melN++] = {(uint8_t)start, (uint8_t)st.spb, (int8_t)approach, 1};
        s.mel[s.melN++] = {(uint8_t)(start + st.spb), (uint8_t)(barSteps - st.spb), (int8_t)target, 1};
        deg = target;
      }
    }
  }
  s.cur = deg;
}

void Audio::melody(Seq& s, int layer, const MelEv& e, float stepSec, float dly) {
  const Style& st = kStyle[(int)s.mode];
  const int m = degMidi(st, st.melRoot, e.deg);
  const float dur = (float)e.len * stepSec;
  const float vel = (e.accent ? 1.0f : 0.84f) * (0.92f + 0.12f * unit(s.rng));
  const float d = dly + 0.005f * unit(s.rng);   // a touch of human timing
  const bool longNote = e.len >= st.spb;          // choir doubles only sustained notes (voice budget)
  switch (s.mode) {
    case Music::Title:
      if (s.role == 2) inst(layer, Inst::Vox, m, dur * 0.95f, vel, d);
      else {
        inst(layer, Inst::Horn, m, dur * 0.92f, vel, d);
        if (s.variant == 3 && longNote) inst(layer, Inst::Vox, m, dur * 0.95f, vel * 0.7f, d);
      }
      break;
    case Music::Wild: inst(layer, Inst::Flute, m, dur * 0.95f, vel, d); break;
    case Music::Night:
      if (s.role == 2) inst(layer, Inst::Flute, m, dur * 0.95f, vel * 0.7f, d);
      else inst(layer, Inst::Bell, m, dur, vel, d);
      break;
    case Music::Town:
      if (s.role == 2) inst(layer, Inst::Fiddle, m - 12, dur * 0.85f, vel * 1.1f, d);
      else inst(layer, Inst::Flute, m, dur * 0.92f, vel, d);
      break;
    case Music::Combat:
      if (s.role == 2) inst(layer, Inst::Horn, m - 12, dur * 0.9f, vel, d);
      else inst(layer, Inst::Fiddle, m, dur * 0.85f, vel, d);
      break;
    case Music::Boss:
      inst(layer, Inst::Horn, m, dur * 0.92f, vel, d);
      if (longNote) inst(layer, Inst::Vox, m, dur * 0.95f, vel * 0.8f, d);
      if (s.role == 2) inst(layer, Inst::Fiddle, m + 12, dur * 0.85f, vel * 0.55f, d);
      break;
    default: break;
  }
}

void Audio::seqStep(Seq& s, int L, float dly) {
  const Style& st = kStyle[(int)s.mode];
  const int barSteps = st.beats * st.spb;
  if (s.step == 0) compose(s);
  const int i = s.step, bar = i / barSteps, sb = i % barSteps, sub = sb % st.spb;
  const int c = s.chords[bar];
  const bool chordStart = sb == 0 && (st.chordBars == 1 || (bar & 1) == 0);
  const bool lastBar = bar == 7, intense = s.role == 2 || s.variant == 3;
  const float stepSec = 60.0f / st.bpm / (float)st.spb;
  const float chordSec = stepSec * (float)(barSteps * st.chordBars);
  const int root = bassNote(st, c);
  auto r = [&]() { return unit(s.rng); };

  while (s.melI < s.melN && s.mel[s.melI].step <= i) melody(s, L, s.mel[s.melI++], stepSec, dly);

  switch (s.mode) {
    case Music::Title: {
      if (chordStart) {
        for (int j = 0; j < 3; j++) inst(L, Inst::Choir, chordWin(st, c, j, 50), chordSec, 1.0f, dly);
        inst(L, Inst::Choir, chordWin(st, c, 0, 38), chordSec, 0.9f, dly);
        inst(L, Inst::BowBass, root, chordSec, 1.0f, dly);
        inst(L, Inst::Timpani, root + 12, 0, 0.75f, dly);
        if (intense)
          for (int j = 0; j < 3; j++) inst(L, Inst::Strings, chordWin(st, c, j, 62), chordSec, 0.7f, dly);
      }
      if (sb == 0) inst(L, Inst::Taiko, 0, 0, 1.0f, dly);
      if (sb == 8) inst(L, Inst::Taiko, 0, 0, 0.55f, dly);
      if (s.role != 0 && (sb == 4 || sb == 12)) inst(L, Inst::Frame, 0, 0, 0.4f, dly);
      if ((bar & 1) && (sb == 14 || sb == 15)) inst(L, Inst::Taiko, 0, 0, 0.35f + 0.2f * (float)(sb - 14), dly);  // pickup
      if (lastBar && sb >= 8) inst(L, Inst::Timpani, root + 12, 0, 0.2f + 0.07f * (float)(sb - 8), dly);        // roll
      break;
    }
    case Music::Wild: {
      if (chordStart)
        for (int j = 0; j < 3; j++) inst(L, Inst::Strings, chordWin(st, c, j, 55), chordSec, 0.7f, dly);
      if (sb == 0) inst(L, Inst::PluckBass, root, stepSec * 6, 1.0f, dly);
      if (sb == 8 && r() < 0.6f) inst(L, Inst::PluckBass, bassNote(st, c + 4), stepSec * 4, 0.7f, dly);
      if ((sub & 1) == 0) {   // harp arpeggio in eighths; two shapes alternate by section
        static const int8_t upDown[6] = {0, 2, 4, 7, 4, 2}, broken[6] = {0, 4, 2, 7, 4, 9};
        const int8_t* pat = (s.section & 1) ? upDown : broken;
        const int k = sb / 2;
        inst(L, Inst::Harp, degMidi(st, st.root + 12, c + pat[k]), stepSec * 6, k == 0 ? 0.9f : 0.6f, dly + 0.004f * r());
      }
      if (intense) {
        if (sb == 0) inst(L, Inst::Frame, 0, 0, 0.45f, dly);
        if (sub == 2) inst(L, Inst::Shaker, 0, 0, sb == 10 ? 0.4f : 0.25f, dly);
      }
      break;
    }
    case Music::Night: {
      if (chordStart) {
        for (int j = 0; j < 3; j++) inst(L, Inst::StringsDark, chordWin(st, c, j, 57), chordSec, 0.8f, dly);
        inst(L, Inst::BowBass, root, chordSec, 0.7f, dly);
      }
      if (sub == 0 && r() < 0.3f) inst(L, Inst::Harp, chordWin(st, c, (int)(xs32(s.rng) % 3), 69), 2.0f, 0.35f, dly);
      if (r() < 0.035f) {     // stars: minor-pentatonic glints far away
        static const int8_t pent[5] = {0, 2, 3, 4, 6};
        inst(L, Inst::Star, degMidi(st, st.melRoot + 12, pent[xs32(s.rng) % 5]), 0, 0.5f + 0.4f * r(), dly);
      }
      break;
    }
    case Music::Town: {
      if (chordStart)
        for (int j = 0; j < 3; j++) inst(L, Inst::Strings, chordWin(st, c, j, 60), chordSec, 0.45f, dly);
      if (sb == 0) inst(L, Inst::PluckBass, root, stepSec * 5, 1.0f, dly);
      if (sb == 6) inst(L, Inst::PluckBass, bassNote(st, c + 4), stepSec * 5, 0.75f, dly);
      if ((sub & 1) == 0) {   // lute: root-fifth-octave-tenth rolling arpeggio
        static const int8_t pat[6] = {0, 4, 7, 9, 7, 4};
        const int k = sb / 2;
        inst(L, Inst::Lute, degMidi(st, st.root + 12, c + pat[k]), stepSec * 2.5f, k == 0 ? 1.0f : 0.72f, dly + 0.004f * r());
      }
      if (lastBar) {
        if ((sub & 1) == 0) inst(L, Inst::Frame, 0, 0, 0.3f + 0.06f * (float)sb, dly);
      } else {
        if (sb == 0) inst(L, Inst::Frame, 0, 0, 0.75f, dly);
        if (sb == 6) inst(L, Inst::Frame, 0, 0, 0.5f, dly);
        if (sb == 10 && r() < 0.4f) inst(L, Inst::Frame, 0, 0, 0.3f, dly);
      }
      if ((sub & 1) == 0 && sub != 0) inst(L, Inst::Shaker, 0, 0, 0.35f, dly);
      else if (sub == 0) inst(L, Inst::Shaker, 0, 0, 0.5f, dly);
      break;
    }
    case Music::Cave: {
      if (chordStart) {
        inst(L, Inst::Drone, root, chordSec + 1.0f, 1.0f, dly);
        inst(L, Inst::Drone, root + 7, chordSec + 1.0f, 0.65f, dly);
      }
      if (sub == 0 && r() < 0.16f) {   // a lone low phrase in the dark, often leaning on the b2
        static const int8_t pick[4] = {0, 2, 4, 1};
        inst(L, Inst::Cello, degMidi(st, st.root + 12, c + pick[xs32(s.rng) % 4]), stepSec * (float)(4 + xs32(s.rng) % 5), 0.75f, dly);
      }
      if (r() < 0.06f) inst(L, Inst::Drip, 84 + (int)(xs32(s.rng) % 13), 0, 0.6f + 0.4f * r(), dly + stepSec * r());
      if (sb == 0 && (bar == 0 || bar == 4) && r() < 0.5f) inst(L, Inst::Rumble, 0, 0, 0.8f, dly);
      if (sb == 0 && bar == 3 && r() < 0.4f) inst(L, Inst::Bell, degMidi(st, st.melRoot, c), 0, 0.45f, dly);
      break;
    }
    case Music::Combat: {
      static const float taiko[16] = {1, 0, 0, 0.55f, 0, 0, 0.5f, 0, 0.9f, 0, 0.45f, 0, 0, 0, 0.5f, 0};
      if (lastBar && sb >= 8) {
        inst(L, Inst::Tom, 52 - (sb - 8), 0, 0.5f + 0.06f * (float)(sb - 8), dly);
        if (sb == 8) inst(L, Inst::Cymbal, 0, stepSec * 8, 1.0f, dly);
      } else {
        if (taiko[sb] > 0) inst(L, Inst::Taiko, 0, 0, taiko[sb], dly);
        else if (intense && (sb & 1) == 0) inst(L, Inst::Taiko, 0, 0, 0.3f, dly);
      }
      if (sb == 4 || sb == 12) inst(L, Inst::Snare, 0, 0, 0.9f, dly);
      if (sb == 15 && r() < 0.5f) inst(L, Inst::Snare, 0, 0, 0.3f, dly);
      inst(L, Inst::Shaker, 0, 0, (sb & 1) ? 0.35f : 0.2f, dly);
      if ((sb & 1) == 0) inst(L, Inst::DriveBass, root + ((sb == 6 || sb == 14) ? 12 : 0), stepSec * 1.6f, (sb & 3) == 0 ? 1.0f : 0.75f, dly);
      static const int8_t ost[16] = {0, 4, 2, 4, 0, 4, 2, 4, 0, 4, 2, 4, 0, 4, 7, 4};
      inst(L, Inst::Stab, degMidi(st, st.root + 12, c + ost[sb]), 0, ((sb & 3) == 0 ? 0.9f : 0.6f) * (intense ? 1.0f : 0.75f), dly);
      if (chordStart && intense)
        for (int j = 0; j < 3; j++) inst(L, Inst::Horn, chordWin(st, c, j, 52), stepSec * 3, 0.6f, dly);
      break;
    }
    case Music::Boss: {
      if (chordStart) {
        for (int j = 0; j < 3; j++) inst(L, Inst::Choir, chordWin(st, c, j, 48), chordSec, 1.1f, dly);
        inst(L, Inst::Choir, chordWin(st, c, 0, 36), chordSec, 1.0f, dly);
        inst(L, Inst::Timpani, root + 12, 0, 1.0f, dly);
        if (intense)
          for (int j = 0; j < 3; j++) inst(L, Inst::Strings, chordWin(st, c, j, 60), chordSec, 0.7f, dly);
      }
      if (sb == 0) inst(L, Inst::Taiko, 0, 0, 1.0f, dly);
      if (sb == 10) inst(L, Inst::Taiko, 0, 0, 0.8f, dly);
      if (sb == 6 && r() < 0.5f) inst(L, Inst::Taiko, 0, 0, 0.5f, dly);
      if (sb == 8) inst(L, Inst::Snare, 0, 0, 1.0f, dly);
      if (intense && (sb == 14 || sb == 15)) inst(L, Inst::Tom, 45 - (sb - 14) * 3, 0, 0.55f, dly);
      if ((sb & 3) == 2) inst(L, Inst::Shaker, 0, 0, 0.3f, dly);
      if (lastBar && sb >= 12) inst(L, Inst::Timpani, root + 12, 0, 0.3f + 0.12f * (float)(sb - 12), dly);
      if (lastBar && sb == 8) inst(L, Inst::Cymbal, 0, stepSec * 8, 1.0f, dly);
      if (sub < 3) inst(L, Inst::DriveBass, root, stepSec * (sub == 2 ? 1.8f : 0.8f), sub == 0 ? 1.0f : 0.7f, dly);  // gallop
      break;
    }
    default: break;
  }
  s.step = (i + 1) % (8 * barSteps);
}

// ------------------------------------------------------------------------------------------ mixing
void Audio::updateMusic(float blockSec) {
  int want = wantMusic_.load(std::memory_order_relaxed);
  Seq& cur = seq_[fg_];
  if (want != (int)cur.mode) {
    const int o = fg_ ^ 1;
    Seq& other = seq_[o];
    if ((int)other.mode != want) {
      // Reusing the layer: its leftover voices (possibly still audible if a third piece is requested mid-fade)
      // get the layer gain baked in and a quick release, so the new piece never inherits them.
      const float g = std::sin(other.x * PI * 0.5f);
      for (Voice& v : v_)
        if (v.on && v.n.bus == o + 1) {
          v.n.vol *= g;
          v.n.bus = 3;
          if (v.t < v.n.hold) {   // still gated: release now (already-releasing voices just finish)
            v.n.hold = v.t;
            v.n.rel = std::fmin(v.n.rel, 0.15f);
          }
          if (v.n.delay > 0 || g <= 0) {
            v.on = false;
            if (v.str >= 0) { strUsed_[v.str] = false; v.str = -1; }
          }
        }
      startSeq(other, (Music)want);
    }
    other.target = 1;
    cur.target = 0;
    fg_ = o;
  }
  for (int L = 0; L < 2; L++) {
    Seq& s = seq_[L];
    const float stepX = blockSec / CROSSFADE_SEC;
    s.x = s.target > s.x ? std::fmin(s.target, s.x + stepX) : std::fmax(s.target, s.x - stepX);
    if (s.mode == Music::Silence || (s.target <= 0 && s.x <= 0)) continue;
    const Style& st = kStyle[(int)s.mode];
    const double stepSec = 60.0 / (double)st.bpm / (double)st.spb;
    while (s.nextStep < s.t + (double)blockSec) {
      seqStep(s, L, (float)(s.nextStep - s.t));
      s.nextStep += stepSec;
    }
    s.t += (double)blockSec;
  }
  echoAmt_ += (kStyle[(int)seq_[fg_].mode].echo - echoAmt_) * 0.002f;
}

void Audio::renderBlock(float* out, int n) {
  updateMusic((float)n * DT);
  const float lg[2] = {std::sin(seq_[0].x * PI * 0.5f), std::sin(seq_[1].x * PI * 0.5f)};
  float sfx[BLOCK] = {}, mus[BLOCK] = {}, sndS[BLOCK] = {}, sndM[BLOCK] = {};
  for (Voice& v : v_)
    if (v.on) renderVoice(v, n, lg, sfx, mus, sndS, sndM);

  static constexpr int combLen[4] = {1215, 1293, 1390, 1476}, apLen[2] = {605, 480};
  const float master = clampf(master_.load(std::memory_order_relaxed), 0.0f, 1.5f);
  const float mv = clampf(musicVol_.load(std::memory_order_relaxed), 0.0f, 1.5f);
  for (int i = 0; i < n; i++) {
    // music ducks gently under loud effects
    const float a = std::fabs(sfx[i]);
    duckEnv_ = a > duckEnv_ ? duckEnv_ + (a - duckEnv_) * 0.02f : duckEnv_ * 0.99995f;
    const float duck = 1.0f - 0.35f * clampf((duckEnv_ - 0.35f) * 2.0f, 0.0f, 1.0f);   // footsteps/UI never duck
    const float m = mus[i] * mv * duck;
    const float send = sndS[i] + sndM[i] * mv * duck;

    // reverb: 4 damped feedback combs + 2 allpasses (mono Freeverb)
    const float in = send * 0.22f + 1e-18f;   // tiny offset keeps the tails out of denormals
    float rv = 0;
    for (int c = 0; c < 4; c++) {
      float y = comb_[c][combPos_[c]];
      combLp_[c] = y * 0.7f + combLp_[c] * 0.3f;
      comb_[c][combPos_[c]] = in + combLp_[c] * 0.83f;
      if (++combPos_[c] >= combLen[c]) combPos_[c] = 0;
      rv += y;
    }
    for (int k = 0; k < 2; k++) {
      float b = ap_[k][apPos_[k]];
      ap_[k][apPos_[k]] = rv + b * 0.5f;
      rv = b - rv;
      if (++apPos_[k] >= apLen[k]) apPos_[k] = 0;
    }
    // echo: one damped feedback delay (~300 ms) for open-air / cavern space
    const float e = echo_[(echoPos_ - ECHO_TAP) & (ECHO - 1)];
    echoLp_ += (e - echoLp_) * 0.35f;
    echo_[echoPos_] = send * echoAmt_ + echoLp_ * 0.42f + 1e-18f;
    echoPos_ = (echoPos_ + 1) & (ECHO - 1);

    float x = (sfx[i] + m + rv * 0.3f + e * 0.6f) * master;
    // peak limiter: instant attack, 150 ms release; output can never exceed 0.8
    const float ax = std::fabs(x);
    limEnv_ = ax > limEnv_ ? ax : limEnv_ * 0.99986f;
    if (limEnv_ > 0.8f) x *= 0.8f / limEnv_;
    out[i] = x;
  }
}

void Audio::render(float* out, int frames) {
  if (!out || frames <= 0) return;
  uint32_t tail = qTail_.load(std::memory_order_relaxed);
  const uint32_t head = qHead_.load(std::memory_order_acquire);
  for (; tail != head; tail++) {
    const Event& e = q_[tail % QN];
    trigger(e.s, e.pitch, e.vol);
  }
  qTail_.store(tail, std::memory_order_release);
  for (int done = 0; done < frames;) {
    const int n = frames - done < BLOCK ? frames - done : BLOCK;
    renderBlock(out + done, n);
    done += n;
  }
}
