// The generative score: the pieces (classic and in a culture's MusicStyle), composition, the sequencer, the
// instruments and the crossfade between pieces (updateMusic).
#include "engine/audio/audio_internal.h"

enum class Audio::Inst : uint8_t {
  Choir, Vox, Strings, StringsDark, Horn, Flute, Fiddle, Bell, Star, Lute, Harp, PluckBass, BowBass, Cello,
  Drone, DriveBass, Stab, Taiko, Frame, Snare, Shaker, Tom, Timpani, Cymbal, Drip, Rumble,
  // M3 culture voices
  Oud, Pipes, Reed, Marimba, Brass, Organ, Shimmer, HandBass, Gong, Block, Doum, Tek, TablaLo, TablaHi, Bodhran, BodhranRim,
  BellPerc, Throat,
};

namespace {
// ------------------------------------------------------------------------------------------ music theory
// The 12 scales (engine/music_style.h order). Harmony always moves in a 7-note parent mode (chords stacked in thirds);
// the melody keeps to the scale's own notes (kMelMask, pitch-class bits from the tonic): a pentatonic melody over its
// parent mode's chords, the way folk musicians play.
const int8_t kHarm[(int)Scale::COUNT][7] = {
  {0, 2, 4, 5, 7, 9, 11},   // Major
  {0, 2, 3, 5, 7, 8, 10},   // Minor (natural)
  {0, 2, 3, 5, 7, 9, 10},   // Dorian
  {0, 1, 3, 5, 7, 8, 10},   // Phrygian (dark, b2)
  {0, 2, 4, 5, 7, 9, 10},   // Mixolydian
  {0, 2, 4, 6, 7, 9, 11},   // Lydian
  {0, 2, 3, 5, 7, 8, 11},   // Harmonic minor
  {0, 1, 4, 5, 7, 8, 10},   // Hijaz (Phrygian dominant)
  {0, 2, 4, 5, 7, 9, 11},   // Penta major (parent: major)
  {0, 2, 3, 5, 7, 8, 10},   // Penta minor (parent: minor)
  {0, 2, 3, 5, 7, 8, 10},   // Hirajoshi (parent: minor)
  {0, 1, 3, 5, 7, 8, 10},   // In-sen (parent: phrygian)
  {0, 2, 4, 6, 7, 9, 10},   // (M10) Whole-tone (parent: lydian dominant)
  {0, 1, 3, 5, 7, 8, 10},   // (M10) Pelog (parent: phrygian)
  {0, 1, 4, 5, 7, 8, 11},   // (M10) Double harmonic
};
// chord progressions that suit each scale (roots as degrees of the parent mode)
const int8_t kProgs[(int)Scale::COUNT][4][4] = {
  {{0, 3, 4, 0}, {0, 5, 3, 4}, {0, 4, 5, 3}, {3, 0, 4, 0}},   // Major: I IV V I, I vi IV V...
  {{0, 5, 2, 6}, {0, 3, 6, 0}, {0, 6, 5, 6}, {5, 3, 0, 4}},   // Minor
  {{0, 3, 0, 6}, {0, 6, 3, 0}, {0, 2, 3, 0}, {0, 3, 6, 4}},   // Dorian: i IV i bVII
  {{0, 1, 0, 6}, {0, 1, 6, 0}, {0, 5, 1, 0}, {0, 6, 5, 1}},   // Phrygian: i bII
  {{0, 6, 3, 0}, {0, 3, 6, 0}, {0, 4, 6, 3}, {0, 6, 0, 3}},   // Mixolydian: I bVII IV
  {{0, 1, 0, 4}, {0, 1, 6, 0}, {0, 4, 1, 0}, {0, 5, 1, 0}},   // Lydian: I II
  {{0, 3, 4, 0}, {0, 5, 4, 0}, {0, 3, 6, 4}, {5, 3, 4, 0}},   // Harmonic minor: i iv V
  {{0, 1, 0, 6}, {0, 6, 1, 0}, {0, 3, 1, 0}, {0, 1, 6, 0}},   // Hijaz: I bII
  {{0, 4, 0, 5}, {0, 3, 0, 4}, {5, 4, 0, 0}, {0, 5, 3, 0}},   // Penta major
  {{0, 6, 0, 3}, {0, 3, 6, 0}, {0, 4, 6, 0}, {0, 6, 3, 6}},   // Penta minor
  {{0, 5, 0, 6}, {0, 0, 5, 6}, {0, 3, 0, 5}, {0, 6, 5, 0}},   // Hirajoshi
  {{0, 1, 0, 6}, {0, 6, 0, 1}, {0, 5, 1, 0}, {0, 0, 1, 0}},   // In-sen
  {{0, 1, 0, 6}, {0, 6, 1, 0}, {0, 1, 6, 1}, {6, 0, 1, 0}},   // (M10) Whole-tone: drifting between I, II and bVII
  {{0, 1, 0, 5}, {0, 5, 1, 0}, {0, 0, 1, 5}, {5, 1, 0, 0}},   // (M10) Pelog
  {{0, 1, 0, 4}, {0, 4, 1, 0}, {0, 3, 1, 0}, {0, 1, 4, 0}},   // (M10) Double harmonic: I bII, the V
};

using Style = audio_detail::Piece;
constexpr uint8_t Ionian = (uint8_t)Scale::Major, Aeolian = (uint8_t)Scale::Minor, Phrygian = (uint8_t)Scale::Phrygian;

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
  // Tavern (M5 phase A stand-in: the Town jig a little quicker; the AMBIENCE lane gives the bard his own pieces)
  {84, 2, 6, 1, 41, Ionian, {{0, 3, 4, 0}, {0, 5, 3, 4}, {0, 4, 5, 3}, {3, 0, 4, 0}}, 77, -4, 6, 0.6f, 0.9f, 0.08f, false, 1.15f},
};

bool styledMode(Music m) { return m == Music::Town || m == Music::Wild || m == Music::Night || m == Music::Tavern; }
// what a style changes in a mode (a different key crossfades to a new piece): the whole style in Town / Wild / Night,
// only the drums in Combat / Boss, nothing elsewhere
uint64_t styleKey(Music m, uint64_t style) {
  if (!style) return 0;
  if (styledMode(m)) return style;
  if (m == Music::Combat || m == Music::Boss) return (1ull << 63) | (style & (15ull << 28));
  return 0;
}
// loudness of each lead relative to the classic flute (calibrated with tools/audio_preview)
const float kLeadNorm[(int)LeadInst::COUNT] = {1.06f, 1.0f, 0.9f, 0.95f, 0.95f, 1.08f, 1.0f, 0.95f, 0.72f, 1.0f, 0.82f, 0.95f};

// the piece a culture plays in a mode
Style buildPiece(Music m, const MusicStyle& ms, bool festive = false) {
  Style st = kStyle[(int)m];
  int meter = ms.meter ? ms.meter : 4;
  // (M5) the festival's dance: a common-time culture strikes up a 6/8 jig (3/4, 5/8 and 7/8 are dances already)
  if (m == Music::Tavern && festive && meter == 4) meter = 6;
  switch (meter) {
    case 3: st.beats = 3; st.spb = 4; break;
    case 5: st.beats = 5; st.spb = 4; break;
    case 6: st.beats = 2; st.spb = 6; break;
    case 7: st.beats = 7; st.spb = 2; break;
    default: st.beats = 4; st.spb = 4; break;
  }
  float bpm = ms.bpm ? (float)ms.bpm : st.bpm;
  if (meter == 6) bpm *= 0.72f;          // a 6/8 beat is a dotted quarter
  if (meter == 7) bpm *= 2.0f;           // a 7/8 beat is an eighth
  const float modeK = m == Music::Town ? 1.0f : m == Music::Tavern ? (festive ? 1.24f : 1.1f) : m == Music::Wild ? 0.84f : 0.62f;
  st.bpm = bpm * modeK;
  if (st.bpm < 40) st.bpm = 40;
  st.scale = (uint8_t)((int)ms.scale < (int)Scale::COUNT ? (int)ms.scale : 0);
  for (int i = 0; i < 4; i++)
    for (int j = 0; j < 4; j++) st.progs[i][j] = kProgs[st.scale][i][j];
  st.root = (int8_t)(36 + (int)(ms.seed % 10u));
  const bool high = ms.lead == LeadInst::Flute || ms.lead == LeadInst::Pipes || ms.lead == LeadInst::Bells ||
                    ms.lead == LeadInst::Marimba || ms.lead == LeadInst::Reed;
  int mel = st.root + 24 + (high ? 12 : 0);
  while (mel > 79) mel -= 12;
  while (mel < 57) mel += 12;
  st.melRoot = (int8_t)mel;
  st.lo = -3; st.hi = 7;
  if (ms.lead == LeadInst::Voice || ms.lead == LeadInst::Horn) { st.lo = -2; st.hi = 5; }
  const bool townish = m == Music::Town || m == Music::Tavern;
  st.chordBars = (uint8_t)((!townish || ms.drone >= 11 || ms.scale == Scale::InSen || ms.scale == Scale::Hirajoshi) ? 2 : 1);
  float busy = m == Music::Town ? 0.5f : m == Music::Tavern ? (festive ? 0.7f : 0.6f) : m == Music::Wild ? 0.38f : 0.2f;
  busy += (float)ms.ornament / 60.0f;
  if (ms.lead == LeadInst::Marimba || ms.lead == LeadInst::Oud || ms.lead == LeadInst::Fiddle || ms.lead == LeadInst::Pipes) busy += 0.12f;
  if (ms.lead == LeadInst::Horn || ms.lead == LeadInst::Voice || ms.lead == LeadInst::Brass) busy -= 0.1f;
  st.busy = busy < 0.1f ? 0.1f : busy > 0.9f ? 0.9f : busy;
  st.density = townish ? (m == Music::Tavern ? 0.92f : 0.85f) : m == Music::Wild ? 0.75f : 0.55f;
  if (ms.pad == PadInst::Shimmer || ms.pad == PadInst::Choir || ms.lead == LeadInst::Bells || ms.lead == LeadInst::Harp) st.echo += 0.06f;
  st.intro = m != Music::Tavern;   // (M5) the bard starts on the tune (the room is already listening)
  st.level = (townish ? (m == Music::Tavern ? 1.1f : 1.15f) : m == Music::Wild ? 1.2f : 1.1f) * kLeadNorm[(int)ms.lead < (int)LeadInst::COUNT ? (int)ms.lead : 0];
  if (m == Music::Tavern) st.echo = std::max(0.04f, st.echo - 0.04f);   // a low-ceilinged room: drier than the street
  return st;
}

// the nearest note of the scale's own set (the melody of a pentatonic culture keeps off its parent's avoid notes)
int snapToScale(int midi, int tonic, uint8_t scale) {
  const uint16_t mask = kMelMask[scale < (uint8_t)Scale::COUNT ? scale : 0];
  for (int d = 0; d < 6; d++) {
    if (mask & (1u << posmodTop(midi - d - tonic))) return midi - d;
    if (mask & (1u << posmodTop(midi + d - tonic))) return midi + d;
  }
  return midi;
}

inline int degMidi(const Style& st, int base, int deg) {
  int oct = deg >= 0 ? deg / 7 : -((6 - deg) / 7);
  return base + 12 * oct + kHarm[st.scale][deg - 7 * oct];
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

const Cell kCells2[] = {
  {1, 1, {0}, {2}},          // eighth
  {2, 1, {0, 1}, {1, 1}},    // two sixteenths
  {0, 1, {}, {}},            // rest
  {1, 2, {0}, {4}},          // quarter (two beats)
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

// ------------------------------------------------------------------------------------------ music: instruments
void Audio::inst(int layer, Inst which, int midi, float dur, float vel, float dly) {
  const float f = mtof(midi);
  vel *= seq_[layer].st.level;
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
    // ---- M3 culture voices
    case Inst::Oud:        // a short-necked lute: bright, woody pluck, a touch of fret-buzz brightness, quick decay
      add(NB(Pluck, f, 0.24f * vel).bright(0.72f).ring(0.75f).lp(3400).env(0.001f, 1, 1, std::fmax(dur, 0.2f), 0.12f).send(0.3f).at(dly).bus(bus));
      add(NB(Pluck, f * 2.0f, 0.05f * vel).bright(0.5f).ring(0.3f).env(0.001f, 1, 1, 0.15f, 0.08f).at(dly).bus(bus));
      break;
    case Inst::Pipes:      // a chanter: reedy, nasal, steady (no vibrato), strong attack
      add(NB(Saw, f, 0.06f * vel).vox(1500, 2.2f).breath(0.03f).env(0.012f, 0.2f, 0.9f, dur, 0.05f).send(0.35f).at(dly).bus(bus));
      add(NB(Square, f, 0.025f * vel).pw(0.3f).lp(2600).env(0.012f, 0.2f, 0.9f, dur, 0.05f).at(dly).bus(bus));
      break;
    case Inst::Reed:       // a soft shawm / clarinet: hollow odd harmonics, gentle vibrato
      add(NB(Square, f, 0.065f * vel).pw(0.5f).lp(1700, 1.2f).breath(0.04f).env(0.045f, 0.3f, 0.85f, dur, 0.1f).vib(0.15f, 5.2f, 0.3f).send(0.4f).at(dly).bus(bus));
      break;
    case Inst::Marimba:    // wooden bars over resonators: a round fundamental and the fourth partial, short
      add(NB(Sine, f, 0.24f * vel).env(0.001f, 0.42f).send(0.3f).at(dly).bus(bus));
      add(NB(Sine, f * 4.0f, 0.06f * vel).env(0.001f, 0.07f).at(dly).bus(bus));
      add(NB(Noise, 0, 0.05f * vel).bp(f * 2.0f, 2.0f).env(0.001f, 0.015f).at(dly).bus(bus));
      break;
    case Inst::Brass:      // a bright natural trumpet: brassier and more forward than the horn
      add(NB(Saw, f, 0.085f * vel).lp(950, 1.2f).fenv(1.6f, 0.07f).env(0.025f, 0.4f, 0.8f, dur, 0.15f).vib(0.12f, 5.5f, 0.3f).send(0.4f).at(dly).bus(bus));
      add(NB(Square, f * 0.5f, 0.025f * vel).pw(0.4f).lp(700).env(0.03f, 0.4f, 0.8f, dur, 0.15f).at(dly).bus(bus));
      break;
    case Inst::Organ:      // a small positive organ: stopped flue pipes (fundamental + octave + twelfth)
      add(NB(Tri, f, 0.04f * vel).lp(2200).env(0.06f, 1, 1, dur, 0.25f).send(0.5f).at(dly).bus(bus));
      add(NB(Sine, f * 2.0f, 0.018f * vel).env(0.06f, 1, 1, dur, 0.25f).send(0.5f).at(dly).bus(bus));
      add(NB(Sine, f * 3.0f, 0.008f * vel).env(0.06f, 1, 1, dur, 0.25f).send(0.5f).at(dly).bus(bus));
      break;
    case Inst::Shimmer:    // a glassy, slow pad an octave up, drowned in reverb (elven halls)
      add(NB(Sine, f * 2.0f, 0.03f * vel).env(0.9f, 1, 1, dur, 1.2f).vib(0.08f, 3.8f, 0.6f).send(0.85f).at(dly).bus(bus));
      add(NB(Tri, f, 0.022f * vel).lp(1600).env(0.7f, 1, 1, dur, 1.0f).send(0.7f).at(dly).bus(bus));
      break;
    case Inst::HandBass:   // a plucked, muted bass string
      add(NB(Pluck, f, 0.3f * vel).bright(0.22f).ring(0.45f).lp(900).env(0.001f, 1, 1, std::fmax(dur, 0.2f), 0.08f).send(0.1f).at(dly).bus(bus));
      break;
    case Inst::Gong:       // a temple gong: inharmonic, slow bloom
      add(NB(Fm, f, 0.11f * vel).fm(1.41f, 2.4f, 1.2f).env(0.02f, 2.8f).send(0.6f).at(dly).bus(bus));
      add(NB(Sine, f * 2.76f, 0.03f * vel).env(0.05f, 1.6f).vib(0.3f, 4.0f).send(0.6f).at(dly).bus(bus));
      break;
    case Inst::Block:      // a wood block / clog
      add(NB(Sine, 980, 0.16f * vel).pitch(0.2f, 0.005f).env(0.001f, 0.045f).send(0.15f).at(dly).bus(bus));
      add(NB(Noise, 0, 0.12f * vel).bp(2400, 1.5f).env(0.001f, 0.012f).at(dly).bus(bus));
      break;
    case Inst::Doum:       // a goblet drum's deep centre stroke
      add(NB(Sine, 96, 0.34f * vel).pitch(0.8f, 0.018f).env(0.001f, 0.2f).send(0.2f).at(dly).bus(bus));
      add(NB(Noise, 0, 0.12f * vel).lp(700).env(0.001f, 0.03f).at(dly).bus(bus));
      break;
    case Inst::Tek:        // ... and its bright rim stroke
      add(NB(Noise, 0, 0.22f * vel).bp(3200, 1.8f).env(0.001f, 0.03f).send(0.15f).at(dly).bus(bus));
      add(NB(Sine, 620, 0.1f * vel).env(0.001f, 0.025f).at(dly).bus(bus));
      break;
    case Inst::TablaLo:    // the bass drum's bending "ge"
      add(NB(Sine, 72, 0.3f * vel).slide(0.9f).env(0.001f, 0.35f).send(0.2f).at(dly).bus(bus));
      break;
    case Inst::TablaHi:    // the treble drum's ringing "na"
      add(NB(Fm, 520, 0.11f * vel).fm(1.0f, 1.2f, 0.06f).env(0.001f, 0.22f).send(0.25f).at(dly).bus(bus));
      add(NB(Noise, 0, 0.08f * vel).bp(4000, 2).env(0.001f, 0.01f).at(dly).bus(bus));
      break;
    case Inst::Bodhran:    // a frame drum struck with a tipper: a warm, dry boom
      add(NB(Sine, 82, 0.34f * vel).pitch(0.6f, 0.025f).env(0.001f, 0.16f).send(0.2f).at(dly).bus(bus));
      add(NB(Noise, 0, 0.18f * vel).lp(420).env(0.001f, 0.05f).at(dly).bus(bus));
      break;
    case Inst::BodhranRim:
      add(NB(Noise, 0, 0.16f * vel).bp(900, 1.2f).env(0.001f, 0.035f).send(0.15f).at(dly).bus(bus));
      add(NB(Sine, 160, 0.1f * vel).env(0.001f, 0.04f).at(dly).bus(bus));
      break;
    case Inst::BellPerc:   // small hand bells / finger cymbals
      add(NB(Fm, 2600, 0.045f * vel).fm(2.7f, 1.4f, 0.12f).env(0.001f, 0.35f).send(0.5f).at(dly).bus(bus));
      break;
    case Inst::Throat:     // an overtone singer's drone: a low voice with a whistling formant
      add(NB(Pad, f, 0.05f * vel).detune(0.003f).vox(1200, 5.0f).env(1.2f, 1, 1, dur, 1.5f).send(0.5f).at(dly).bus(bus));
      break;
  }
}

// ------------------------------------------------------------------------------------------ music: composition
// (M3c) the land's Mood (rpg/world/biomes.h order: Pastoral, Woodland, Deep, Exotic, Wetland, Coastal, Arid, Frozen,
// Highland, Wondrous, Ominous) bends a Wild or Night piece: tempo, phrase density and busyness, the melody's register,
// the echo, the style's drone, and for the ominous and wondrous lands the harmony's mode
static bool moodMode(Music m) { return m == Music::Wild || m == Music::Night; }
static void applyMood(audio_detail::Piece& st, MusicStyle& ms, bool styled, uint8_t mood) {
  auto scaleTo = [&](Scale sc) {
    st.scale = (uint8_t)sc;
    for (int a = 0; a < 4; a++) for (int b = 0; b < 4; b++) st.progs[a][b] = kProgs[(int)sc][a][b];
  };
  switch (mood) {
    case 1: st.busy = clampf(st.busy - 0.05f, 0, 1); st.echo += 0.04f; break;                                     // Woodland
    case 2: st.bpm *= 0.86f; st.density *= 0.75f; if (st.melRoot >= 62) st.melRoot -= 12; st.echo += 0.08f; break; // Deep
    case 3: st.bpm *= 1.08f; st.busy = clampf(st.busy + 0.15f, 0, 1); break;                                     // Exotic
    case 4: st.bpm *= 0.9f; st.density *= 0.85f; st.echo += 0.08f; break;                                       // Wetland
    case 5: st.busy = clampf(st.busy - 0.05f, 0, 1); st.echo += 0.06f; break;                                    // Coastal
    case 6: st.bpm *= 0.92f; st.density *= 0.8f; st.busy = clampf(st.busy + 0.05f, 0, 1); if (styled) ms.drone = (uint8_t)std::max<int>(ms.drone, 8); break;   // Arid
    case 7: st.bpm *= 0.8f; st.density *= 0.7f; if (st.melRoot < 70) st.melRoot += 12; st.echo += 0.10f; break;  // Frozen
    case 8: if (styled) ms.drone = (uint8_t)std::max<int>(ms.drone, 10); st.echo += 0.04f; break;                 // Highland
    case 9: st.density *= 0.85f; st.busy = clampf(st.busy - 0.1f, 0, 1); st.echo += 0.12f;                       // Wondrous
      if (st.scale == (uint8_t)Scale::Major || st.scale == (uint8_t)Scale::PentaMajor) scaleTo(Scale::Lydian);
      break;
    case 10: st.bpm *= 0.85f; st.density *= 0.7f; if (st.melRoot >= 62) st.melRoot -= 12; st.echo += 0.06f;     // Ominous
      if (st.scale != (uint8_t)Scale::Phrygian && st.scale != (uint8_t)Scale::InSen && st.scale != (uint8_t)Scale::HarmonicMinor) scaleTo(Scale::Phrygian);
      break;
    default: break;
  }
  st.echo = clampf(st.echo, 0, 0.5f);
}

void Audio::startSeq(Seq& s, Music m, uint64_t style, uint8_t mood) {
  s = Seq{};
  s.mode = m;
  s.style = styleKey(m, style);
  s.ms = MusicStyle::unpack(style);
  s.st = s.style && styledMode(m) ? buildPiece(m, s.ms, ((style >> 62) & 1) != 0) : kStyle[(int)m];
  s.festive = m == Music::Tavern && ((style >> 62) & 1) != 0;
  s.mood = moodMode(m) ? mood : (uint8_t)255;
  if (moodMode(m) && mood != 255) applyMood(s.st, s.ms, s.style != 0, mood);
  starts_++;
  s.rng = 0x9E3779B9u * (starts_ + 1u) ^ (0x85EBCA6Bu * ((uint32_t)m + 7u)) ^ rng_;
  if (s.rng == 0) s.rng = 1;
  for (int i = 0; i < 4; i++) xs32(s.rng);
  // the culture's own motifs: its first theme comes from its seed (so its tunes are recognisably its own); the
  // variations, answers and the rest still change from visit to visit
  s.motifRng = 0x2545F491u ^ ((uint32_t)s.ms.seed * 0x9E3779B1u) ^ ((uint32_t)m * 0x85EBCA6Bu);
  if (!s.motifRng) s.motifRng = 1;
  for (int i = 0; i < 3; i++) xs32(s.motifRng);
  s.percVar = (int)(s.ms.seed >> 3) & 1;
  s.nextStep = 0.05;
}

void Audio::genMotif(Seq& s, Motif& m, uint32_t& rng) {
  const Style& st = s.st;
  const bool six = st.spb == 6, two = st.spb == 2;
  const Cell* cells = six ? kCells6 : two ? kCells2 : kCells4;
  const float b = st.busy;
  float w4[9] = {3 - 1.5f * b, 1 + 2 * b, 1.2f, 2 * b, 1.6f * b, 2.2f * b * b, 0.6f, 0.5f, 2.4f * (1 - b)};
  float w6[7] = {2 - b, 2, 1 + 2 * b, 1.2f, 1.2f * b, 0.4f, 1.5f * (1 - b)};
  float w2[4] = {3, 1 + 2 * b, 0.4f, 1.5f * (1 - b)};
  const float* w = six ? w6 : two ? w2 : w4;
  const int nCells = six ? 7 : two ? 4 : 9, restCell = six ? 5 : two ? 2 : 7, halfCell = six ? 6 : two ? 3 : 8;
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
      if (bar == 1 && beat == st.beats - 2 && unit(rng) < 0.6f) { push(base, 2 * st.spb); beat += 2; continue; }  // long close
      if (bar == 1 && lastBeat) { push(base, st.spb); beat++; continue; }
      if (bar == 0 && lastBeat && unit(rng) < 0.4f) { push(base, st.spb); beat++; continue; }  // breath mid-phrase
      int c = pickWeighted(w, nCells, rng);
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
  for (int i = 0; i < m.n; i++) m.iv[i] = (int8_t)(pickWeighted(wi, 9, rng) - 4);
}

void Audio::compose(Seq& s) {
  const Style& st = s.st;
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
  if (s.role == 2) genMotif(s, s.b, s.rng);
  else if (!s.haveA) { genMotif(s, s.a, s.style ? s.motifRng : s.rng); s.haveA = true; }   // a culture's theme: its own
  else if (s.variant == 0 && unit(s.rng) < 0.5f) genMotif(s, s.a, s.rng);

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
  const Style& st = s.st;
  int m = degMidi(st, st.melRoot, e.deg);
  if (s.style && styledMode(s.mode)) {   // M3: the culture's lead, on its own scale's notes
    m = snapToScale(m, st.melRoot, st.scale);
    const float dur = (float)e.len * stepSec;
    const float vel = (e.accent ? 1.0f : 0.84f) * (0.92f + 0.12f * unit(s.rng));
    const float d = dly + 0.005f * unit(s.rng);
    LeadInst li = s.ms.lead;
    // B sections answer on a partner instrument now and then (a fiddle answering pipes, a flute answering a harp)
    static const LeadInst partner[(int)LeadInst::COUNT] = {LeadInst::Flute, LeadInst::Harp, LeadInst::Fiddle, LeadInst::Pipes,
                                                          LeadInst::Reed, LeadInst::Flute, LeadInst::Lute, LeadInst::Flute,
                                                          LeadInst::Flute, LeadInst::Flute, LeadInst::Horn, LeadInst::Fiddle};
    if (s.role == 2 && (s.section & 1) && (int)li < (int)LeadInst::COUNT) li = partner[(int)li];
    float v = vel;
    if (s.mode == Music::Night) v *= 0.8f;
    if (s.mode == Music::Wild) v *= 0.92f;
    lead(s, layer, li, m, dur, v, d, e.accent != 0);
    return;
  }
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
    case Music::Town: case Music::Tavern:
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
  if (s.style && styledMode(s.mode)) { seqStepStyled(s, L, dly); return; }
  const Style& st = s.st;
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
  // M3: Combat and Boss in a culture's style keep their structure but play its drums (taiko -> its low drum, snare ->
  // its rim / high stroke); the classic pieces call inst() exactly as before
  const PercKind pk = s.style ? s.ms.perc : PercKind::None;
  const bool ownDrums = pk != PercKind::None && pk != PercKind::Taiko;
  auto drum = [&](Inst which, float vel, float d) {
    if (!ownDrums) { inst(L, which, 0, 0, vel, d); return; }
    perc(L, pk, which == Inst::Taiko ? 0 : 1, vel * 1.1f, d, root);
  };

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
    case Music::Town: case Music::Tavern: {   // (M5: the classic tavern plays the town jig at its own pace)
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
        if (taiko[sb] > 0) drum(Inst::Taiko, taiko[sb], dly);
        else if (intense && (sb & 1) == 0) drum(Inst::Taiko, 0.3f, dly);
      }
      if (sb == 4 || sb == 12) drum(Inst::Snare, 0.9f, dly);
      if (sb == 15 && r() < 0.5f) drum(Inst::Snare, 0.3f, dly);
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
      if (sb == 0) drum(Inst::Taiko, 1.0f, dly);
      if (sb == 10) drum(Inst::Taiko, 0.8f, dly);
      if (sb == 6 && r() < 0.5f) drum(Inst::Taiko, 0.5f, dly);
      if (sb == 8) drum(Inst::Snare, 1.0f, dly);
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

// ------------------------------------------------------------------------------------------ music: culture styles (M3)
// a lead note with the style's ornaments: grace notes before strong notes, slides into notes on the bending
// instruments, the odd trill on a long note (ornament 0..15 scales how often)
void Audio::lead(Seq& s, int L, LeadInst li, int midi, float dur, float vel, float dly, bool strong) {
  const MusicStyle& ms = s.ms;
  const float orn = (float)ms.ornament / 15.0f;
  const bool bends = li == LeadInst::Oud || li == LeadInst::Fiddle || li == LeadInst::Voice || li == LeadInst::Reed ||
                     li == LeadInst::Pipes || li == LeadInst::Flute;
  Inst in = Inst::Flute;
  float vk = 1.0f, durK = 0.92f;
  int oct = 0;
  switch (li) {
    case LeadInst::Lute: in = Inst::Lute; vk = 0.95f; break;
    case LeadInst::Flute: in = Inst::Flute; break;
    case LeadInst::Horn: in = Inst::Horn; oct = -12; vk = 1.05f; break;
    case LeadInst::Pipes: in = Inst::Pipes; durK = 0.97f; break;
    case LeadInst::Oud: in = Inst::Oud; oct = -12; break;
    case LeadInst::Reed: in = Inst::Reed; break;
    case LeadInst::Fiddle: in = Inst::Fiddle; oct = -12; vk = 1.1f; durK = 0.85f; break;
    case LeadInst::Bells: in = Inst::Bell; vk = 0.95f; durK = 1.0f; break;
    case LeadInst::Marimba: in = Inst::Marimba; durK = 1.0f; break;
    case LeadInst::Harp: in = Inst::Harp; oct = -12; vk = 1.05f; durK = 1.0f; break;
    case LeadInst::Brass: in = Inst::Brass; oct = -12; break;
    case LeadInst::Voice: in = Inst::Vox; oct = -12; vk = 0.95f; durK = 0.95f; break;
    default: break;
  }
  midi += oct;
  const Style& st = s.st;
  auto upper = [&](int m) { return snapToScale(m + 2, st.melRoot, st.scale) > m ? snapToScale(m + 2, st.melRoot, st.scale) : m + 2; };
  // a trill on a long note: main, upper, main, upper, then the note held
  if (dur > 0.55f && orn > 0.55f && unit(s.rng) < 0.25f * orn) {
    const float t = 0.065f;
    const int up = upper(midi);
    for (int k = 0; k < 4; k++) inst(L, in, (k & 1) ? up : midi, t * 0.9f, vel * vk * 0.8f, dly + t * (float)k);
    inst(L, in, midi, dur * durK - 4 * t, vel * vk, dly + 4 * t);
    return;
  }
  // a grace note just before a strong note (the pipes' cuts, the oud's flicks)
  if (strong && dur > 0.18f && unit(s.rng) < 0.7f * orn) {
    const float g = 0.045f;
    inst(L, in, upper(midi), g, vel * vk * 0.55f, dly > g ? dly - g : dly);
  }
  // a slide up into the note on the bending instruments: an extra short note a step below, then the note
  if (bends && dur > 0.25f && orn > 0.3f && unit(s.rng) < 0.3f * orn) {
    const int below = snapToScale(midi - 1, st.melRoot, st.scale) < midi ? snapToScale(midi - 1, st.melRoot, st.scale) : midi - 1;
    inst(L, in, below, 0.06f, vel * vk * 0.6f, dly);
    inst(L, in, midi, dur * durK - 0.05f, vel * vk, dly + 0.05f);
    return;
  }
  inst(L, in, midi, dur * durK, vel * vk, dly);
}

// one stroke of a percussion family: which 0 the low / open stroke, 1 the high / rim stroke, 2 a ghost
void Audio::perc(int L, PercKind k, int which, float vel, float dly, int root) {
  switch (k) {
    case PercKind::None: break;
    case PercKind::Frame:
      if (which == 2) inst(L, Inst::Shaker, 0, 0, vel * 0.8f, dly);
      else inst(L, Inst::Frame, 0, 0, which ? vel * 0.6f : vel, dly);
      break;
    case PercKind::Bodhran:
      if (which == 0) inst(L, Inst::Bodhran, 0, 0, vel, dly);
      else inst(L, Inst::BodhranRim, 0, 0, which == 2 ? vel * 0.5f : vel, dly);
      break;
    case PercKind::Taiko:
      if (which == 0) inst(L, Inst::Taiko, 0, 0, vel, dly);
      else inst(L, Inst::Frame, 0, 0, which == 2 ? vel * 0.4f : vel * 0.7f, dly);
      break;
    case PercKind::Hand:
      if (which == 0) inst(L, Inst::Doum, 0, 0, vel, dly);
      else inst(L, Inst::Tek, 0, 0, which == 2 ? vel * 0.45f : vel, dly);
      break;
    case PercKind::Tabla:
      if (which == 0) inst(L, Inst::TablaLo, 0, 0, vel, dly);
      else inst(L, Inst::TablaHi, 0, 0, which == 2 ? vel * 0.5f : vel, dly);
      break;
    case PercKind::Gong:
      if (which == 0) inst(L, Inst::Gong, root - 12 < 30 ? root : root - 12, 0, vel * 0.9f, dly);
      else inst(L, Inst::Block, 0, 0, which == 2 ? vel * 0.35f : vel * 0.6f, dly);
      break;
    case PercKind::Wood:
      if (which == 0) inst(L, Inst::Tom, 52, 0, vel * 0.7f, dly);
      else inst(L, Inst::Block, 0, 0, which == 2 ? vel * 0.45f : vel * 0.85f, dly);
      break;
    case PercKind::Bells:
      if (which == 0) inst(L, Inst::Frame, 0, 0, vel * 0.5f, dly);
      else inst(L, Inst::BellPerc, 0, 0, which == 2 ? vel * 0.5f : vel, dly);
      break;
    case PercKind::Kettle:
      if (which == 0) inst(L, Inst::Timpani, root + 12, 0, vel * 0.8f, dly);
      else inst(L, Inst::Snare, 0, 0, which == 2 ? vel * 0.25f : vel * 0.5f, dly);
      break;
    default: break;
  }
}

// (M5) a handclap from the tavern's crowd: three hands a few milliseconds apart, a bright noise slap with a body
void Audio::clap(int L, float vel, float dly) {
  vel *= seq_[L].st.level;
  const int bus = L + 1;
  for (int h = 0; h < 3; h++) {
    const float d = dly + 0.004f * (float)h + 0.006f * rnd();
    add(NB(Noise, 0, 0.30f * vel * (h == 0 ? 1.0f : 0.7f)).perc(0.022f).bp(rnd(1050, 1500), 1.4f).send(0.3f).at(d).bus(bus));
  }
  add(NB(Noise, 0, 0.10f * vel).perc(0.06f).bp(800, 0.8f).send(0.35f).at(dly + 0.01f).bus(bus));
}

void Audio::padChord(Seq& s, int L, PadInst p, int c, float dur, float vel, float dly) {
  const Style& st = s.st;
  switch (p) {
    case PadInst::Strings:
      for (int j = 0; j < 3; j++) inst(L, Inst::Strings, chordWin(st, c, j, 58), dur, vel * 0.55f, dly);
      break;
    case PadInst::Drone:
      inst(L, Inst::Drone, chordWin(st, c, 0, 45), dur + 0.5f, vel * 0.7f, dly);
      inst(L, Inst::StringsDark, chordWin(st, c, 2, 52), dur, vel * 0.5f, dly);
      break;
    case PadInst::Organ:
      for (int j = 0; j < 3; j++) inst(L, Inst::Organ, chordWin(st, c, j, 55), dur, vel * 0.9f, dly);
      break;
    case PadInst::Choir:
      for (int j = 0; j < 3; j++) inst(L, Inst::Choir, chordWin(st, c, j, 52), dur, vel * 0.8f, dly);
      break;
    case PadInst::Bowed:
      for (int j = 0; j < 3; j++) inst(L, Inst::StringsDark, chordWin(st, c, j, 55), dur, vel * 0.7f, dly);
      break;
    case PadInst::Shimmer:
      for (int j = 0; j < 3; j++) inst(L, Inst::Shimmer, chordWin(st, c, j, 60), dur, vel, dly);
      break;
    default: break;
  }
}

void Audio::bassNoteStyled(int L, BassInst b, int midi, float dur, float vel, float dly) {
  switch (b) {
    case BassInst::Plucked: inst(L, Inst::PluckBass, midi, dur, vel, dly); break;
    case BassInst::Bowed: inst(L, Inst::BowBass, midi, dur, vel * 0.85f, dly); break;
    case BassInst::Drone: inst(L, Inst::Drone, midi, dur, vel * 0.8f, dly); break;
    case BassInst::Horn: inst(L, Inst::Horn, midi + 12, dur, vel * 0.55f, dly); break;
    case BassInst::Hand: inst(L, Inst::HandBass, midi, dur, vel, dly); break;
    default: break;
  }
}

namespace {
// percussion patterns per meter: strengths of the low stroke and the high stroke on each step of a bar, two variants
// (a culture plays one); 0 = nothing. Steps: 3/4, 4/4 and 5/4 in sixteenths; 6/8 in sixteenths (12); 7/8 in
// sixteenths grouped 2 + 2 + 3 eighths (14).
struct PercPat { int steps; float lo[20], hi[20]; };
const PercPat kPerc[5][2] = {
  {{12, {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, .7f, 0, 0, 0, .7f, 0, 0, 0}},                                     // 3/4
   {12, {1, 0, 0, 0, 0, 0, 0, 0, .5f, 0, 0, 0}, {0, 0, .3f, 0, .7f, 0, .3f, 0, 0, 0, .6f, 0}}},
  {{16, {1, 0, 0, 0, 0, 0, 0, .5f, .8f, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, .8f, 0, 0, 0, 0, 0, .5f, 0, .8f, 0, 0, .3f}},     // 4/4
   {16, {1, 0, 0, .4f, 0, 0, .6f, 0, 1, 0, 0, 0, 0, 0, 0, 0}, {0, 0, .4f, 0, .8f, 0, 0, 0, 0, 0, .4f, 0, .8f, 0, .4f, 0}}},
  {{20, {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, .8f, 0, 0, 0, 0, 0, 0, 0},                                                         // 5/4 (3 + 2)
         {0, 0, 0, 0, .7f, 0, .3f, 0, .7f, 0, 0, 0, 0, 0, 0, 0, .7f, 0, 0, .3f}},
   {20, {1, 0, 0, 0, 0, 0, .5f, 0, 0, 0, 0, 0, .9f, 0, 0, 0, 0, 0, 0, 0},
         {0, 0, .4f, 0, .7f, 0, 0, 0, .7f, 0, .3f, 0, 0, 0, .5f, 0, .7f, 0, 0, 0}}},
  {{12, {1, 0, 0, 0, 0, 0, .7f, 0, 0, 0, 0, 0}, {0, 0, 0, .5f, 0, .4f, 0, 0, 0, .6f, .3f, 0}},                                   // 6/8
   {12, {1, 0, 0, 0, 0, .4f, .8f, 0, 0, 0, 0, 0}, {0, 0, .5f, 0, .5f, 0, 0, 0, .6f, 0, .5f, 0}}},
  {{14, {1, 0, 0, 0, 0, 0, 0, 0, .7f, 0, 0, 0, 0, 0}, {0, 0, .3f, 0, .8f, 0, .3f, 0, 0, 0, .6f, 0, .5f, 0}},                    // 7/8
   {14, {1, 0, .4f, 0, 0, 0, 0, 0, .8f, 0, 0, 0, 0, 0}, {0, 0, 0, 0, .8f, 0, .4f, 0, 0, 0, .7f, 0, .4f, .3f}}},
};
}  // namespace

void Audio::seqStepStyled(Seq& s, int L, float dly) {
  const Style& st = s.st;
  const MusicStyle& ms = s.ms;
  const int barSteps = st.beats * st.spb;
  if (s.step == 0) compose(s);
  const int i = s.step, bar = i / barSteps, sb = i % barSteps, sub = sb % st.spb;
  const int c = s.chords[bar];
  const bool chordStart = sb == 0 && (st.chordBars == 1 || (bar & 1) == 0);
  const bool lastBar = bar == 7, intense = s.role == 2 || s.variant == 3;
  const float stepSec = 60.0f / st.bpm / (float)st.spb;
  const float chordSec = stepSec * (float)(barSteps * st.chordBars);
  const int root = bassNote(st, c);
  const bool tavern = s.mode == Music::Tavern;
  const bool town = s.mode == Music::Town || tavern, wild = s.mode == Music::Wild, night = s.mode == Music::Night;
  auto r = [&]() { return unit(s.rng); };
  // swing: the off-beat eighth of a simple meter is late (0..15 -> up to a third of a step... of two steps)
  auto swingAt = [&](int step) {
    if (!ms.swing || st.spb != 4) return 0.0f;
    const int ss = step % st.spb;
    return ss == 2 ? (float)ms.swing / 15.0f * stepSec * 0.66f : (ss & 1) ? (float)ms.swing / 15.0f * stepSec * 0.3f : 0.0f;
  };
  const float sw = swingAt(sb);

  while (s.melI < s.melN && s.mel[s.melI].step <= i) {
    const MelEv& e = s.mel[s.melI++];
    melody(s, L, e, stepSec, dly + swingAt(e.step % barSteps));
  }

  // the bed: the culture's pad (softer in the wild, dark and low at night)
  if (chordStart) {
    const float pv = town ? 0.8f : wild ? 0.7f : 0.75f;
    PadInst pad = ms.pad;
    if (night && pad == PadInst::Strings) pad = PadInst::Bowed;
    padChord(s, L, pad, c, chordSec, pv, dly);
  }
  // the drone (pipes, steppe, fjord): tonic and fifth held under everything, as present as the style says
  if (ms.drone > 0 && sb == 0 && (bar & 3) == 0) {
    const float dv = (float)ms.drone / 15.0f * (night ? 0.6f : 0.85f);
    const int tonic = bassNote(st, 0);
    const float len = stepSec * (float)(barSteps * 4) + 0.4f;
    if (ms.lead == LeadInst::Voice && ms.drone >= 9) inst(L, Inst::Throat, tonic + 12, len, dv, dly);
    else inst(L, Inst::Drone, tonic, len, dv, dly);
    if (ms.drone >= 8) inst(L, Inst::Drone, tonic + 7, len, dv * 0.55f, dly);
  }
  // the bass
  {
    const float bv = town ? 1.0f : wild ? 0.8f : 0.6f;
    const int half = st.spb == 6 ? 6 : st.beats == 5 ? 12 : st.beats == 7 ? 8 : st.beats == 3 ? 8 : 8;
    switch (ms.bass) {
      case BassInst::Plucked: case BassInst::Hand:
        if (sb == 0) bassNoteStyled(L, ms.bass, root, stepSec * (float)half * 0.8f, bv, dly);
        if (sb == half && !night && r() < 0.8f) bassNoteStyled(L, ms.bass, bassNote(st, c + 4), stepSec * 4, bv * 0.7f, dly + sw);
        if (ms.bass == BassInst::Hand && town && sub == st.spb - 1 && r() < 0.25f)
          bassNoteStyled(L, ms.bass, root, stepSec, bv * 0.45f, dly + sw);
        break;
      case BassInst::Bowed: case BassInst::Drone: case BassInst::Horn:
        if (chordStart) bassNoteStyled(L, ms.bass, root, chordSec, bv, dly);
        break;
      default: break;
    }
  }
  // the accompaniment: plucked and struck cultures roll their chords in eighths; the wind and voice cultures leave the
  // drone and drums to carry them
  {
    Inst acc = Inst::Harp;
    bool has = true;
    switch (ms.lead) {
      case LeadInst::Lute: acc = Inst::Lute; break;
      case LeadInst::Oud: acc = Inst::Oud; break;
      case LeadInst::Harp: acc = Inst::Harp; break;
      case LeadInst::Marimba: acc = Inst::Marimba; break;
      case LeadInst::Bells: acc = Inst::Harp; break;
      case LeadInst::Flute: acc = Inst::Harp; break;
      case LeadInst::Fiddle: acc = Inst::Lute; break;
      case LeadInst::Reed: acc = Inst::Lute; break;
      case LeadInst::Brass: acc = Inst::Harp; break;
      default: has = false; break;   // Horn, Pipes, Voice
    }
    const int every = st.spb == 6 ? 2 : 2;
    if (has && (sub % every) == 0 && !(night && (bar & 1)) && !(wild && !intense && (sb / every) % 2 == 1)) {
      static const int8_t up[8] = {0, 2, 4, 7, 4, 2, 4, 7}, broken[8] = {0, 4, 2, 7, 4, 9, 7, 4};
      const int8_t* pat = ((ms.seed >> 5) & 1) ? up : broken;
      const int k = (sb / every) % 8;
      const float av = (k == 0 ? 0.85f : 0.6f) * (town ? 1.0f : wild ? 0.75f : 0.55f) * (acc == Inst::Marimba ? 0.6f : 1.0f);
      const int m = degMidi(st, st.root + 12, c + pat[k]);
      inst(L, acc, m, stepSec * (float)every * 1.6f, av, dly + 0.004f * r() + sw);
    }
  }
  // percussion: the meter's pattern in the culture's family (towns; the wild only when the piece lifts; never at night,
  // bar a soft low stroke now and then for the drum cultures)
  if (ms.perc != PercKind::None) {
    const int mi = st.spb == 6 ? 3 : st.beats == 3 ? 0 : st.beats == 5 ? 2 : st.beats == 7 ? 4 : 1;
    const PercPat& P = kPerc[mi][s.percVar & 1];
    const int ps = sb < P.steps ? sb : sb % P.steps;
    const float pv = town ? 1.0f : wild ? (intense ? 0.55f : 0.0f) : 0.0f;
    if (pv > 0) {
      if (lastBar && sb >= barSteps / 2) {   // a fill into the next section
        if ((sb & 1) == 0) perc(L, ms.perc, (sb / 2) & 1, pv * (0.4f + 0.5f * (float)(sb - barSteps / 2) / (float)barSteps), dly + sw, root);
      } else {
        if (P.lo[ps] > 0) perc(L, ms.perc, 0, P.lo[ps] * pv * 0.95f, dly, root);
        if (P.hi[ps] > 0) perc(L, ms.perc, 1, P.hi[ps] * pv * 0.8f, dly + sw, root);
        else if (intense && (sb & 1) == 1 && r() < 0.25f) perc(L, ms.perc, 2, pv * 0.5f, dly + sw, root);
      }
      if (ms.perc == PercKind::Gong && i == 0) perc(L, PercKind::Gong, 0, 0.9f, dly, root);
    } else if (night && sb == 0 && (bar & 3) == 0 && r() < 0.5f &&
               (ms.perc == PercKind::Taiko || ms.perc == PercKind::Gong || ms.perc == PercKind::Hand || ms.perc == PercKind::Tabla)) {
      perc(L, ms.perc, 0, 0.35f, dly, root);
    }
  }
  // (M5) the tavern's room joins in: a clap on every other beat (on a festival on the off-beats too, and a stamp on
  // the boards at each bar)
  if (tavern) {
    const int beat = st.spb;
    const bool off = st.spb == 6 ? (sb % beat) == 3 : (sb % beat) == beat / 2;
    const float cv = s.festive ? 0.55f : 0.32f;
    if (!lastBar && (sb % beat) == 0 && (sb / beat) % 2 == 1) clap(L, cv * (0.85f + 0.3f * r()), dly + sw);
    if (s.festive && !lastBar && off && r() < 0.7f) clap(L, cv * 0.6f * (0.8f + 0.4f * r()), dly + sw);
    if (s.festive && sb == 0) perc(L, PercKind::Bodhran, 0, 0.55f, dly, root);   // a stamp on the boards
  }
  // night: far-off glints in the culture's own scale
  if ((night && r() < 0.03f) || (wild && (s.mood == 9 || s.mood == 7) && r() < 0.015f)) {
    const int m = snapToScale(degMidi(st, st.melRoot + 12, (int)(xs32(s.rng) % 7)), st.melRoot, st.scale);
    inst(L, Inst::Star, m, 0, 0.5f + 0.4f * r(), dly);
  }
  s.step = (i + 1) % (8 * barSteps);
}

// ------------------------------------------------------------------------------------------ mixing
void Audio::updateMusic(float blockSec) {
  // (M3 fixer) the wanted mode and style as one consistent pair (wantSeq_); while setMusic is writing them, the piece
  // playing now goes on for this block
  int want = (int)seq_[fg_].mode;
  uint64_t wantStyle = 0;
  bool got = false;
  for (int tries = 0; tries < 4 && !got; tries++) {
    const uint32_t s1 = wantSeq_.load(std::memory_order_acquire);
    if (s1 & 1u) continue;
    const int m = wantMusic_.load(std::memory_order_relaxed);
    const uint64_t st = wantStyle_.load(std::memory_order_relaxed);
    std::atomic_thread_fence(std::memory_order_acquire);
    if (wantSeq_.load(std::memory_order_relaxed) == s1) { want = m; wantStyle = st; got = true; }
  }
  const uint64_t wantKey = got ? styleKey((Music)want, wantStyle) : seq_[fg_].style;
  Seq& cur = seq_[fg_];
  const uint8_t wm = wantMood_.load(std::memory_order_relaxed);
  const uint8_t wantMood = moodMode((Music)want) ? (wm < 11 ? wm : (uint8_t)0) : (uint8_t)255;
  if (want != (int)cur.mode || wantKey != cur.style || wantMood != cur.mood) {   // a new piece, another culture's style or another land's mood
    const int o = fg_ ^ 1;
    Seq& other = seq_[o];
    if ((int)other.mode != want || other.style != wantKey || other.mood != wantMood) {
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
      startSeq(other, (Music)want, wantStyle, wantMood);
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
    const Style& st = s.st;
    const double stepSec = 60.0 / (double)st.bpm / (double)st.spb;
    while (s.nextStep < s.t + (double)blockSec) {
      seqStep(s, L, (float)(s.nextStep - s.t));
      s.nextStep += stepSec;
    }
    s.t += (double)blockSec;
  }
  echoAmt_ += (seq_[fg_].st.echo - echoAmt_) * 0.002f;
}
