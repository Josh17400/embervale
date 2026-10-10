// Software synth + mixer. No audio files: every sound and every piece of music is generated. Thread-safe play().
#pragma once
#include <atomic>
#include <cstdint>
#include "engine/mathx.h"
#include "engine/music_style.h"

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

enum class Sfx : uint8_t {
  Swing, Hit, HitHeavy, Block, Hurt, PlayerHurt, EnemyDie, Death,
  Arrow, ArrowHit, Fireball, Explode, Frost, Heal, Roll, Step,
  Pickup, Coin, Chest, Door, Buy, Talk,
  LevelUp, QuestStart, QuestDone, Discover,
  MenuMove, MenuSelect, MenuBack,
  Roar, Splash,
  Bell,      // town alarm bell (M0 town defence): two quick strikes of a bronze bell
  // M5 Hearth and Hall (AMBIENCE lane; phase A stand-ins): village animals and the tavern's crowd
  Bark,      // a dog's bark (a short double woof)
  Cluck,     // a hen's clucking
  Meow,      // a cat
  Cheer,     // a tavern / festival crowd's cheer and clapping (a toast, the bard's last chord)
  // M6 Steel (BEASTS lane): the new families and the ranked foes. The view plays them from what it sees (the harpy's
  // swoop wind-up, the golem's heavy slam, an elite or champion first coming into view, a world boss's aggro and every
  // boss's phase change); the sim may also play them with Game::sfx.
  HarpyShriek,   // a wavering, raking bird-woman's scream (the swoop)
  GolemSlam,     // stone fists into the ground: a deep thump, grinding crunch, rubble pattering down
  EliteSting,    // an ominous sting (a dissonant swell under a cold bell): something stronger is here
  BossRoar,      // a world boss's roar: deeper and longer than the dragon's, a wall of breath and a sub drop
  // M7 Home (VIEW lane): the ridden horse's hooves (the view plays them at the walk's four-beat and the gallop's
  // three-beat cadence, pitched by the ground)
  Hoof,          // one hoof on packed earth: a hollow, woody clop with a little grit
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
  // M5 Hearth and Hall: the bard's piece in the tavern, in the culture's style (setMusic(Tavern, style, festive)): its
  // scale and lead instrument, its meter a little quicker, the bass and drums up front, a clap on the off-beats; on a
  // festival the dance meter (a 4/4 culture turns to a 6/8 jig) and quicker still. The view picks it inside a gathering
  // place while the bard performs, faintly outside its door at night (setMusicLevel: quieter and muffled by distance)
  // and on the plaza on festival days.
  Tavern,
  COUNT
};

class Audio {
 public:
  bool init();
  void shutdown();
  void play(Sfx s, float pitch = 1.0f, float vol = 1.0f);
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
  void add(const Note& n);
  void trigger(Sfx s, float pitch, float vol);
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
