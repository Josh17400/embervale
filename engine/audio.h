// Software synth + mixer. No audio files: every sound and every piece of music is generated. Thread-safe play().
#pragma once
#include <atomic>
#include <cstdint>
#include "engine/mathx.h"

struct SDL_AudioStream;
struct SDL_Mutex;

enum class Sfx : uint8_t {
  Swing, Hit, HitHeavy, Block, Hurt, PlayerHurt, EnemyDie, Death,
  Arrow, ArrowHit, Fireball, Explode, Frost, Heal, Roll, Step,
  Pickup, Coin, Chest, Door, Buy, Talk,
  LevelUp, QuestStart, QuestDone, Discover,
  MenuMove, MenuSelect, MenuBack,
  Roar, Splash,
  COUNT
};

// Background score. Changing mode crossfades (~2 s) to the new piece.
enum class Music : uint8_t {
  Silence,
  Title,    // slow, majestic, nordic-folk theme
  Wild,     // calm exploration, day
  Night,    // sparse, mysterious exploration at night
  Town,     // warm, lute/flute-ish, cosy
  Cave,     // dark drones, drips
  Combat,   // driving drums, minor
  Boss,     // epic, heavy
  COUNT
};

class Audio {
 public:
  bool init();
  void shutdown();
  void play(Sfx s, float pitch = 1.0f, float vol = 1.0f);
  void setMusic(Music m);
  void setMaster(float v) { master_.store(v); }
  void setMusicVolume(float v) { musicVol_.store(v); }

  // called from the audio thread
  void render(float* out, int frames);

 private:
  // ---- synth voice ----
  enum Wave : uint8_t { Sine, Tri, Saw, Square, Noise, Fm, Pad, Pluck };  // Pad = 3 detuned saws, Pluck = Karplus-Strong
  enum Filt : uint8_t { Off, LP, BP, HP, Vox };                             // Vox = lowpass + resonant band (formant)
  enum class Inst : uint8_t;                                               // music instruments (audio.cpp)
  struct Note {                      // everything one voice needs; built with Audio::NB in audio.cpp
    Wave wave = Sine;
    Filt filt = Off;
    uint8_t bus = 0;                 // 0 sfx, 1..2 music layers (crossfaded), 3 orphaned music (gain baked in)
    float freq = 440, vol = 0.3f, delay = 0;
    float atk = 0.002f, dec = 0.1f, sus = 0, hold = 30, rel = 0.05f;      // envelope; hold = gate length
    float slide = 0, pEnv = 0, pDec = 0.03f;                              // octaves/s; transient pitch boost
    float vibDepth = 0, vibRate = 5, vibDelay = 0;                        // semitones, Hz, s
    float cutoff = 20000, q = 0.707f, fEnv = 0, fDec = 0.1f, fBump = 0, fSlide = 0;  // filter (octaves)
    float fmRatio = 1, fmIndex = 0, fmDec = 1;
    float pw = 0.5f, detune = 0, bright = 0.5f, ring = 1, breath = 0, color = 0;  // color: 0 white .. ~0.95 dark noise
    float amRate = 0, amDepth = 0, drive = 0, send = 0.1f;
  };
  struct NB;                         // fluent Note builder
  struct Voice {
    Note n;
    bool on = false;
    float t = 0, gain = 0;           // gain: last block's output gain (ramped per block, no zipper/clicks)
    float ph[3] = {}, mph = 0;       // oscillator / FM-modulator phases (cycles)
    float ic1 = 0, ic2 = 0, nlp = 0; // state-variable filter, noise colouring
    uint32_t rng = 1;
    int str = -1, ksLen = 0, ksPos = 0;                                   // Karplus-Strong string
    float ksRho = 0, ksH1 = 0, ksC = 0, ksPrev = 0, apX = 0, apY = 0;
  };

  // ---- generative music ----
  struct MelEv { uint8_t step, len; int8_t deg; uint8_t accent; };
  struct Motif { int n = 0; uint8_t on[32] = {}, len[32] = {}; int8_t iv[32] = {}; };
  struct Seq {                       // one music layer; two exist so pieces can crossfade
    Music mode = Music::Silence;
    float x = 0, target = 0;         // crossfade position (equal-power), moves at 0.5/s
    double t = 0, nextStep = 0;
    int step = 0, section = 0, role = 0, variant = 0, prog = 0;
    uint32_t rng = 1;
    int8_t chords[8] = {};
    Motif a, b;
    bool haveA = false;
    MelEv mel[160] = {};
    int melN = 0, melI = 0, cur = 2;
  };
  struct Event { Sfx s; float pitch, vol; };

  static constexpr int MAXV = 64, NSTR = 16, STRLEN = 1024, QN = 64, BLOCK = 32;
  static constexpr int ECHO = 16384, ECHO_TAP = 14400;
  Voice v_[MAXV];
  float str_[NSTR][STRLEN] = {};
  bool strUsed_[NSTR] = {};
  Seq seq_[2];
  int fg_ = 0;                       // layer currently playing / fading in
  uint32_t starts_ = 0, rng_ = 0x2545F491u;

  Event q_[QN] = {};                 // lock-free SPSC: play() -> audio thread
  std::atomic<uint32_t> qHead_{0}, qTail_{0};
  std::atomic_flag qLock_;           // serialises producers if play() is ever called off the main thread

  SDL_AudioStream* stream_ = nullptr;
  std::atomic<float> master_{0.6f}, musicVol_{0.7f};
  std::atomic<int> wantMusic_{0};

  // ---- space + master ----
  float comb_[4][1536] = {}, combLp_[4] = {}, ap_[2][640] = {};
  int combPos_[4] = {}, apPos_[2] = {};
  float echo_[ECHO] = {}, echoLp_ = 0, echoAmt_ = 0.1f;
  int echoPos_ = 0;
  float duckEnv_ = 0, limEnv_ = 0;

  float rnd();                       // audio-thread RNG, 0..1
  float rnd(float lo, float hi) { return lo + (hi - lo) * rnd(); }
  void add(const Note& n);
  void trigger(Sfx s, float pitch, float vol);
  void renderBlock(float* out, int n);
  void renderVoice(Voice& v, int n, const float* layerGain, float* sfx, float* mus, float* sndS, float* sndM);
  void updateMusic(float blockSec);
  void startSeq(Seq& s, Music m);
  void compose(Seq& s);
  void genMotif(Seq& s, Motif& m);
  void seqStep(Seq& s, int layer, float dly);
  void melody(Seq& s, int layer, const MelEv& e, float stepSec, float dly);
  void inst(int layer, Inst which, int midi, float dur, float vel, float dly);
};
