// Software synth + mixer. No audio files: every sound and every piece of music is generated. Thread-safe play().
#pragma once
#include <atomic>
#include <cstdint>
#include "engine/mathx.h"
#include "engine/music_style.h"
#include "engine/sound_ids.h"   // Sfx, Music, Sting (the sound vocabulary)

struct SDL_AudioStream;
struct SDL_Mutex;

namespace audio_detail {
// One piece's parameters: the classic pieces' table (audio.cpp kStyle) or one built from a culture's MusicStyle
// (M3). Plain data, copied into the sequencer layer that plays it.
struct Piece {
  float bpm;                // beats per minute (beat = spb sixteenths)
  uint8_t beats, spb;       // beats per bar, steps per beat (4 simple, 6 compound, 2 for 7/8 eighths)
  uint8_t chordBars;        // bars per chord
  int8_t root;              // MIDI tonic for the bass register
  uint8_t scale;            // Scale (engine/music_style.h): the harmony's 7-note parent and the melody's note set
  int8_t progs[4][4];       // chord roots as scale degrees
  int8_t melRoot;           // MIDI tonic for the melody (same pitch class as root)
  int8_t lo, hi;            // melody range in scale degrees around melRoot
  float busy;               // rhythmic density of motifs, 0..1
  float density;            // chance a phrase is sung (0 = no melody)
  float echo;               // echo send for this piece
  bool intro;               // first section: half of it without melody
  float level;              // overall gain, evens out loudness between pieces
};
}  // namespace audio_detail

class Audio {
 public:
  static constexpr int kSampleRate = 48000;   // mono f32 (render: a test harness mixes into its own buffer)
  bool init();
  void shutdown();
  void play(Sfx s, float pitch = 1.0f, float vol = 1.0f);
  // (M9 SPELLFX) a spell's motif (Sfx::SpellMotif): motif picks the contour and rhythm (FxStyle::motif), scale the
  // culture's (engine/music_style.h Scale), timbre the school's voice (0 brass, 1 bells, 2 organ, 3 glass, 4 wood,
  // 5 choir, 6 harp), root the tonic's MIDI note (48..84)
  void playMotif(uint8_t motif, uint8_t scale, uint8_t timbre, int root = 72, float vol = 1.0f);
  // (M12 VIEW lane) the herald's stings of the player's rise: the enum lives in engine/sound_ids.h
  using Sting = ::Sting;
  void playSting(Sting s, float pitch = 1.0f, float vol = 1.0f);
  // (Witness 0.6 sound events) a listener told every sound started through play / playMotif / playSting, by name
  // (sfxName: "swing", "hit_heavy", "quest_done"...; a motif "spell_motif", a sting "sting_fanfare"...) and volume, on the
  // caller's thread before the voice is queued. rpg/main.cpp sets it in witness mode only (witness_sound_ex); null: none.
  // One for the process (a Witness reset rebuilds Audio in place and the listener stays). Changes nothing that sounds.
  using PlayListener = void (*)(const char* name, float vol);
  static void setPlayListener(PlayListener f);
  static const char* sfxName(Sfx s);
  static const char* stingName(Sting s);
  void setMusic(Music m);
  // M3: the piece in a culture's style (engine/music_style.h; nullptr = the classic piece). Town, Wild and Night take
  // its scale, tempo, meter, instruments and ornament; Combat and Boss keep their structure but take its percussion.
  // A new style with the same mode crossfades like a new piece (the border-crossing moment, VISION_PLAN 5.6).
  void setMusic(Music m, const MusicStyle* style);
  // M5: the same, with the tavern's festive variant (only Music::Tavern reads it; a change crossfades like a new piece)
  void setMusic(Music m, const MusicStyle* style, bool festive);
  // M5: the score's level (0..1, eased) and how muffled it is (0 open .. 1 heard through a wall: a low-pass): the bard's
  // piece heard from the street outside the tavern's door. 1 / 0 everywhere else. The crowd bed goes through it too.
  void setMusicLevel(float gain, float muffle) { wantMusGain_.store(gain); wantMuffle_.store(muffle); }
  // M5: the murmur of a crowd indoors (0 none .. 1 a packed hall: overlapping voices, laughter, mugs set down), on its
  // own quiet bus under the music; `lively` 0..1 (a festival night: more laughter and cheering)
  void setCrowd(float level, float lively = 0) { wantCrowd_.store(level); wantLively_.store(lively); }
  // M3c Wildlands: the ambient sound bed of the land the player stands in (kind: rpg/world/biomes.h Ambience value;
  // level 0..1, 0 = silent: inside buildings, caves, menus) and the wilderness music's mood (Mood value) that Wild and
  // Night take on top of the culture's style. Main thread; a change crossfades over about two seconds.
  // (LIFE lane) 15 synthesised beds (wind, birds, insects, frogs, surf, gulls, jungle, desert wind, cold wind, volcanic
  // rumble, crystal hum and chimes, blight drone, bamboo knocks, mystic shimmer) on their own quiet bus; the mood bends
  // the Wild and Night pieces (tempo, density, register, drone, scale for the ominous and wondrous lands) and a new mood
  // crossfades like a new piece.
  void setAmbient(uint8_t kind, float level) { wantAmb_.store(kind); wantAmbLevel_.store(level); }
  void setMood(uint8_t mood) { wantMood_.store(mood); }
  // 0 night .. 1 day: the beds' day voices (birdsong, bees) give way to the night's (crickets, owls, more frogs)
  void setDaylight(float d) { wantDay_.store(d); }
  // M10 Beyond the Mist (ART lane): the sea's bed, eased on the audio thread. open: out on the water (the swell's
  // wash under the hull); surf: a shore near (the breakers); gulls: a harbour or a coast near; wind: wind in the sails
  // (0 none .. 1 a gale); creak: aboard a ship (its timbers and rigging; more for a big hull); oars: strokes a second at
  // the oars (0 none); storm: the storm's weight (heavy seas, rain, the gale's howl); mist: the Mist's weight (a cold
  // drone and whispers); turn: the Mist turning the boat (the drone turns dissonant). All 0..1 unless said.
  struct SeaSound { float open = 0, surf = 0, gulls = 0, wind = 0, creak = 0, oars = 0, storm = 0, mist = 0, turn = 0; };
  void setSea(const SeaSound& s) {
    wantSea_[0].store(s.open); wantSea_[1].store(s.surf); wantSea_[2].store(s.gulls); wantSea_[3].store(s.wind);
    wantSea_[4].store(s.creak); wantSea_[5].store(s.oars); wantSea_[6].store(s.storm); wantSea_[7].store(s.mist);
    wantSea_[8].store(s.turn);
  }
  // M11 High Ground (VISION_PLAN 11.3, 15.32): the roar of the nearest waterfall, a looping bed (the FALLS lane renders
  // it in audio.cpp: a deep filtered rush with a spatter on top). level 0 silent .. 1 standing at its foot; pan -1 left
  // .. 1 right; size 0 a little cascade .. 1 a great fall (the pitch and the weight of the roar).
  void setFalls(float level, float pan, float size) { wantFalls_[0].store(level); wantFalls_[1].store(pan); wantFalls_[2].store(size); }
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
    uint64_t style = 0;              // M3: the culture style it plays (MusicStyle::pack(); 0 = the classic piece)
    MusicStyle ms;                   // ... unpacked
    audio_detail::Piece st{};        // the piece's parameters (the classic table's row, or built from the style)
    uint32_t motifRng = 1;           // M3: the culture's own motif stream (its tunes are recognisably its own)
    uint8_t mood = 0;                // (M3c) the land's Mood it plays in (Wild / Night only; 255 none)
    int percVar = 0;                 // M3: which of the meter's percussion patterns this culture plays
    bool festive = false;            // (M5) Music::Tavern on a festival: the dance
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
  struct Event { Sfx s; float pitch, vol; uint32_t arg = 0; };   // arg: (M9) SpellMotif's packed motif

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
  std::atomic<uint8_t> wantAmb_{0}, wantMood_{0};   // (M3c) setAmbient / setMood
  std::atomic<float> wantAmbLevel_{0.0f}, wantDay_{1.0f};
  // ---- (M3c) the ambient bed (audio thread): continuous layers eased toward the bed's profile, and sparse event voices
  struct AmbP { float wind, windCut, hiss, hissCut, howl, surf, rumble, hum, drone, insects, rustle; };
  AmbP ambCur_{0, 400, 0, 2000, 0, 0, 0, 0, 0, 0, 0};
  float ambLvl_ = 0, ambDay_ = 1, ambT_ = 0, ambGustEnv_ = 0, ambGustTarget_ = 0.5f;
  float ambLp_[8] = {}, ambSv1_ = 0, ambSv2_ = 0;
  uint32_t ambRng_ = 0x6A09E667u;
  static constexpr int AMB_EV = 18;
  float ambEvT_[AMB_EV] = {};
  void renderAmbient(float* out, int n, float blockSec);
  // ---- (M5) the score's level and muffle, and the crowd bed (audio thread)
  std::atomic<float> wantMusGain_{1.0f}, wantMuffle_{0.0f}, wantCrowd_{0.0f}, wantLively_{0.0f};
  float musGain_ = 1, muffle_ = 0, muffLp_[2] = {}, crowdLvl_ = 0, crowdLively_ = 0, crowdT_ = 0;
  struct Talker {                    // one voice of the murmur: a buzz through two formants, in syllables and phrases
    float ph = 0, f0 = 150, f0t = 150, f1 = 500, f2 = 1500, env = 0, sylT = 0, phraseT = 0;
    bool talking = false, on = false;
    float s1a = 0, s1b = 0, s2a = 0, s2b = 0;
  };
  static constexpr int TALKERS = 5;
  Talker talk_[TALKERS];
  uint32_t crowdRng_ = 0x3C6EF372u;
  float crowdEvT_[3] = {1, 2, 3};    // laughter, a mug set down, a chair scraped
  void renderCrowd(float* out, int n, float blockSec);
  // ---- (M10) the sea bed (audio thread)
  std::atomic<float> wantSea_[9] = {};
  std::atomic<float> wantFalls_[3] = {};   // M11 setFalls: level, pan, size (renderFalls)
  // (M11 FALLS) the falls' roar (audio thread): eased level / size, the filters' states, the spatter's gate
  float fallsCur_[2] = {}, fallsLp_[8] = {}, fallsT_ = 0, fallsGate_ = 0, fallsGateT_ = 0;
  uint32_t fallsRng_ = 0x1F83D9ABu;
  void renderFalls(float* out, int n, float blockSec);
  float seaCur_[9] = {}, seaT_ = 0, seaLp_[6] = {}, seaEvT_[6] = {0.5f, 1.0f, 1.5f, 2.0f, 0.3f, 3.0f}, seaSwellT_ = 0;
  uint32_t seaRng_ = 0x510E527Fu;
  void renderSea(float* out, int n, float blockSec);
  void ambEvent(int e, float vol);
  std::atomic<int> wantMusic_{0};
  std::atomic<uint64_t> wantStyle_{0};
  // (M3 fixer) a sequence lock over the pair (wantMusic_, wantStyle_): setMusic bumps it to odd, writes both, bumps it
  // to even; updateMusic reads the pair only between two equal even counts, so it never starts a new mode in the old
  // culture's style (or the reverse) at a border crossing
  std::atomic<uint32_t> wantSeq_{0};   // M3: MusicStyle::pack() of the wanted style (0 none); the audio thread reads it

  // ---- space + master ----
  float comb_[4][1536] = {}, combLp_[4] = {}, ap_[2][640] = {};
  int combPos_[4] = {}, apPos_[2] = {};
  float echo_[ECHO] = {}, echoLp_ = 0, echoAmt_ = 0.1f;
  int echoPos_ = 0;
  float duckEnv_ = 0, limEnv_ = 0;

  float rnd();                       // audio-thread RNG, 0..1
  float rnd(float lo, float hi) { return lo + (hi - lo) * rnd(); }
  float rndCall(float lo, float hi);   // rnd(lo, hi) kept out of line (engine/audio/sfx.cpp: bit identity)
  void add(const Note& n);
  void trigger(Sfx s, float pitch, float vol, uint32_t arg = 0);
  // (engine/audio/sfx.cpp) one voice per sound effect, generated from SFX_LIST: trigger() plays Audio::fx<name>
#define EMB_SFX_ARM(name, trim) void fx##name(float k, float V, uint32_t arg);
  SFX_LIST(EMB_SFX_ARM)
#undef EMB_SFX_ARM
  void spellMotif(uint32_t arg, float V);   // (M9) Sfx::SpellMotif's notes
  void riseSting(int which, float k, float V);   // (M12) playSting's notes (arg bit 30 on the SpellMotif slot)
  void push(const Event& e);                // the play queue (main thread -> audio thread)
  void renderBlock(float* out, int n);
  void renderVoice(Voice& v, int n, const float* layerGain, float* sfx, float* mus, float* sndS, float* sndM);
  void updateMusic(float blockSec);
  void startSeq(Seq& s, Music m, uint64_t style, uint8_t mood = 255);
  void compose(Seq& s);
  void genMotif(Seq& s, Motif& m, uint32_t& rng);
  void seqStep(Seq& s, int layer, float dly);
  void seqStepStyled(Seq& s, int layer, float dly);      // M3: Town / Wild / Night in a culture's style
  void melody(Seq& s, int layer, const MelEv& e, float stepSec, float dly);
  void inst(int layer, Inst which, int midi, float dur, float vel, float dly);
  // M3 culture voices: a lead instrument note (with the style's ornaments), a percussion stroke of a family
  // (0 low / open, 1 high / rim, 2 ghost), a pad chord and a bass note
  void lead(Seq& s, int layer, LeadInst li, int midi, float dur, float vel, float dly, bool strong);
  void perc(int layer, PercKind k, int which, float vel, float dly, int root);
  void padChord(Seq& s, int layer, PadInst p, int chord, float dur, float vel, float dly);
  void bassNoteStyled(int layer, BassInst b, int midi, float dur, float vel, float dly);
  void clap(int layer, float vel, float dly);   // (M5) a handclap in the tavern's crowd (a few hands, not quite together)
};
