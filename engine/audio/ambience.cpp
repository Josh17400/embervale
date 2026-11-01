// The ambient beds, each on its own quiet bus: the land's bed, the sea, the falls' roar and the crowd.
#include "engine/audio/audio_internal.h"

// ------------------------------------------------------------------------------------------ (M3c) ambient beds
// The 15 beds (rpg/world/biomes.h Ambience order). Each is a mix of a few continuous noise layers (wind with gusts, a
// high hiss of sand, a resonant howl, surf swells, a deep rumble, a glassy hum, a low drone, an insect chorus, rustling
// leaves) and sparse event voices (birdsong, skylarks, owls, crickets, frogs, gulls, drips, creaks, bamboo knocks, wind
// chimes, bubbles, crackles, vent hisses, exotic calls, bees, distant calls, flies). The layers ease toward the bed's
// profile (about two seconds: the crossfade), the events follow the bed and the hour. All on the audio thread, no
// allocation: a handful of one-pole filters and one state-variable filter per sample.
namespace {
using AmbProfile = float[11];
// wind, windCut, hiss, hissCut, howl, surf, rumble, hum, drone, insects, rustle
const float kBed[15][11] = {
  {0.26f, 500, 0, 2000, 0, 0, 0, 0, 0, 0, 0.05f},          // Breeze
  {0.15f, 450, 0, 2000, 0, 0, 0, 0, 0, 0.04f, 0.05f},      // Meadow
  {0.12f, 400, 0, 2000, 0, 0, 0, 0, 0, 0, 0.12f},          // Woods
  {0.09f, 260, 0, 2000, 0.03f, 0, 0, 0, 0, 0, 0.06f},      // DeepWoods
  {0.05f, 400, 0, 2000, 0, 0, 0, 0, 0, 0.22f, 0.06f},      // Jungle
  {0.08f, 350, 0, 2000, 0, 0, 0, 0, 0, 0.08f, 0.03f},      // Marsh
  {0.10f, 600, 0, 2000, 0, 0.40f, 0, 0, 0, 0, 0},          // Surf
  {0.28f, 700, 0, 2000, 0.06f, 0.25f, 0.06f, 0, 0, 0, 0},  // Cliffs
  {0.30f, 700, 0.22f, 2600, 0, 0, 0, 0, 0, 0, 0},          // DesertWind
  {0.30f, 520, 0.04f, 3000, 0.34f, 0, 0, 0, 0, 0, 0},      // ColdWind
  {0.08f, 300, 0.05f, 2200, 0, 0, 0.40f, 0, 0, 0, 0},      // Volcanic
  {0.08f, 500, 0, 2000, 0, 0, 0, 0.25f, 0, 0, 0},          // Crystal
  {0.06f, 300, 0, 2000, 0, 0, 0, 0, 0.42f, 0, 0},          // Blight
  {0.10f, 500, 0, 2000, 0, 0, 0, 0, 0, 0, 0.20f},          // Bamboo
  {0.10f, 500, 0, 2000, 0, 0, 0, 0.22f, 0, 0, 0.03f},      // Mystic
};
// event voices: 0 chirp, 1 skylark, 2 owl, 3 cricket, 4 frog, 5 gull, 6 drip, 7 creak, 8 knock, 9 chime, 10 bubble,
// 11 crackle, 12 vent, 13 exotic call, 14 bee, 15 distant call, 16 fly, 17 water trickle; rates per second by day / night
struct EvRate { int8_t e; float day, night; };
const EvRate kBedEv[15][5] = {
  {{3, 0, 1.2f}, {0, 0.10f, 0}, {-1, 0, 0}, {-1, 0, 0}, {-1, 0, 0}},                            // Breeze
  {{1, 0.25f, 0}, {0, 0.35f, 0}, {14, 0.10f, 0}, {3, 0, 1.5f}, {2, 0, 0.03f}},                 // Meadow
  {{0, 0.60f, 0}, {2, 0, 0.08f}, {3, 0, 0.6f}, {7, 0.02f, 0.02f}, {-1, 0, 0}},                 // Woods
  {{7, 0.12f, 0.12f}, {6, 0.25f, 0.25f}, {15, 0.06f, 0.06f}, {2, 0, 0.08f}, {0, 0.10f, 0}},     // DeepWoods
  {{13, 0.35f, 0.10f}, {4, 0.40f, 0.80f}, {0, 0.30f, 0}, {6, 0.20f, 0.20f}, {-1, 0, 0}},       // Jungle
  {{4, 0.50f, 1.20f}, {10, 0.30f, 0.30f}, {0, 0.25f, 0}, {3, 0, 0.50f}, {-1, 0, 0}},           // Marsh
  {{5, 0.12f, 0}, {-1, 0, 0}, {-1, 0, 0}, {-1, 0, 0}, {-1, 0, 0}},                              // Surf
  {{5, 0.20f, 0.02f}, {-1, 0, 0}, {-1, 0, 0}, {-1, 0, 0}, {-1, 0, 0}},                          // Cliffs
  {{-1, 0, 0}, {-1, 0, 0}, {-1, 0, 0}, {-1, 0, 0}, {-1, 0, 0}},                                 // DesertWind
  {{7, 0.05f, 0.05f}, {-1, 0, 0}, {-1, 0, 0}, {-1, 0, 0}, {-1, 0, 0}},                          // ColdWind
  {{11, 2.0f, 2.0f}, {12, 0.08f, 0.08f}, {-1, 0, 0}, {-1, 0, 0}, {-1, 0, 0}},                   // Volcanic
  {{9, 0.35f, 0.35f}, {-1, 0, 0}, {-1, 0, 0}, {-1, 0, 0}, {-1, 0, 0}},                          // Crystal
  {{16, 0.25f, 0.10f}, {7, 0.03f, 0.03f}, {-1, 0, 0}, {-1, 0, 0}, {-1, 0, 0}},                  // Blight
  {{8, 0.50f, 0.30f}, {0, 0.15f, 0}, {17, 0.15f, 0.15f}, {3, 0, 0.4f}, {-1, 0, 0}},            // Bamboo
  {{9, 0.20f, 0.20f}, {3, 0, 0.3f}, {-1, 0, 0}, {-1, 0, 0}, {-1, 0, 0}},                        // Mystic
};
constexpr float kAmbGain = 0.5f;   // the bed's overall level (calibrated with tools/audio_preview: peaks ~0.25)
}  // namespace

void Audio::ambEvent(int e, float V) {
  const float r = rnd(0.9f, 1.1f);
  switch (e) {
    case 0: {   // a songbird's phrase: 2-5 quick whistled notes sliding
      const int n = 2 + (int)(rnd() * 4);
      const float base = rnd(2600, 4200);
      for (int i = 0; i < n; i++)
        add(NB(Sine, base * rnd(0.85f, 1.2f), 0.10f * V).env(0.004f, 0.05f, 0, 0.06f, 0.02f).slide(rnd(-3, 3)).send(0.25f).at(i * rnd(0.07f, 0.12f)));
      break;
    }
    case 1: {   // a skylark's warble: a fast fluttering trill
      add(NB(Fm, rnd(3000, 3800), 0.07f * V).env(0.05f, 1, 1, rnd(0.6f, 1.2f), 0.1f).fm(0.5f, 1.5f, 1).vib(1.5f, rnd(14, 22)).send(0.3f));
      break;
    }
    case 2: {   // an owl: hoo... hoo-hoo
      const float f = rnd(360, 420);
      add(NB(Sine, f, 0.14f * V).env(0.04f, 1, 1, 0.22f, 0.12f).slide(-0.2f).breath(0.05f).send(0.45f));
      add(NB(Sine, f * 0.97f, 0.11f * V).env(0.04f, 1, 1, 0.15f, 0.1f).slide(-0.2f).send(0.45f).at(0.55f));
      add(NB(Sine, f * 0.95f, 0.11f * V).env(0.04f, 1, 1, 0.3f, 0.15f).slide(-0.25f).send(0.45f).at(0.8f));
      break;
    }
    case 3: {   // a cricket: a pulse train
      const float f = rnd(4200, 4900);
      for (int i = 0; i < 4; i++) add(NB(Sine, f, 0.035f * V).env(0.003f, 0.02f, 0, 0.02f, 0.01f).send(0.15f).at(i * 0.045f));
      break;
    }
    case 4: {   // a frog: a buzzing croak (or two)
      const float f = rnd(90, 170);
      const int n = 1 + (int)(rnd() * 2.5f);
      for (int i = 0; i < n; i++)
        add(NB(Saw, f * r, 0.10f * V).env(0.01f, 1, 1, rnd(0.12f, 0.22f), 0.05f).vox(rnd(500, 900), 2.5f).am(rnd(22, 34), 0.8f).send(0.25f).at(i * 0.3f));
      break;
    }
    case 5: {   // a gull's cry: two falling calls
      for (int i = 0; i < 2; i++)
        add(NB(Saw, rnd(1100, 1400), 0.06f * V).env(0.02f, 1, 1, 0.22f, 0.08f).slide(-0.8f).bp(1500, 2.5f).vib(0.5f, 9).send(0.4f).at(i * 0.32f));
      break;
    }
    case 6: add(NB(Sine, rnd(1400, 2600), 0.10f * V).perc(0.06f).slide(-2.5f).send(0.5f)); break;            // a drip
    case 7: add(NB(Fm, rnd(140, 220), 0.06f * V).env(0.15f, 1, 1, rnd(0.4f, 0.8f), 0.2f).fm(2.1f, 2.5f, 1).slide(rnd(-0.4f, 0.4f)).lp(900, 2).send(0.35f)); break;   // a creak
    case 8: {   // bamboo knocks: two hollow tocks
      const float f = rnd(480, 780);
      add(NB(Tri, f, 0.18f * V).perc(0.09f).pitch(0.5f, 0.01f).send(0.3f));
      add(NB(Tri, f * rnd(1.1f, 1.35f), 0.13f * V).perc(0.08f).pitch(0.5f, 0.01f).send(0.3f).at(rnd(0.12f, 0.3f)));
      break;
    }
    case 9: {   // wind chimes: a few bell tones of a pentatonic set
      static const float pent[5] = {1.0f, 1.125f, 1.25f, 1.5f, 1.667f};
      const float base = rnd() < 0.5f ? 1320.0f : 1760.0f;
      const int n = 1 + (int)(rnd() * 3);
      for (int i = 0; i < n; i++)
        add(NB(Fm, base * pent[(int)(rnd() * 5) % 5], 0.06f * V).perc(rnd(1.2f, 2.0f)).fm(3.5f, 1.2f, 0.4f).send(0.55f).at(i * rnd(0.1f, 0.3f)));
      break;
    }
    case 10: add(NB(Sine, rnd(260, 420), 0.07f * V).perc(0.07f).slide(rnd(2.5f, 4)).send(0.2f)); break;        // a bubble
    case 11: add(NB(Noise, 0, 0.10f * V).perc(rnd(0.004f, 0.012f)).hp(rnd(1500, 4000))); break;                // a crackle
    case 12: add(NB(Noise, 0, 0.06f * V).env(0.3f, 1, 1, rnd(0.8f, 1.6f), 0.5f).hp(1800).bump(0.5f).send(0.2f)); break;   // a vent's hiss
    case 13: {  // an exotic bird's whoop
      const float f = rnd(800, 1100);
      add(NB(Sine, f, 0.09f * V).env(0.02f, 1, 1, 0.25f, 0.06f).slide(1.6f).send(0.35f));
      add(NB(Sine, f * 1.6f, 0.08f * V).env(0.01f, 1, 1, 0.18f, 0.06f).slide(-2.2f).send(0.35f).at(0.27f));
      break;
    }
    case 14: add(NB(Saw, rnd(200, 240), 0.025f * V).env(0.4f, 1, 1, rnd(0.8f, 1.6f), 0.4f).lp(900).vib(0.4f, 7).am(110, 0.3f)); break;   // a bee
    case 15: add(NB(Sine, rnd(420, 560), 0.06f * V).env(0.1f, 1, 1, 0.5f, 0.4f).slide(-0.5f).vib(0.3f, 5).send(0.7f)); break;   // a distant call
    case 16: add(NB(Saw, rnd(190, 260), 0.03f * V).env(0.1f, 1, 1, rnd(0.4f, 0.9f), 0.2f).lp(1400).vib(0.8f, 9).am(40, 0.4f)); break;   // a fly
    case 17: for (int i = 0; i < 3; i++) add(NB(Sine, rnd(900, 1600), 0.04f * V).perc(0.04f).slide(rnd(1, 3)).at(i * rnd(0.05f, 0.12f))); break;   // trickling water
    default: break;
  }
}

void Audio::renderAmbient(float* out, int n, float blockSec) {
  const int kind = wantAmb_.load(std::memory_order_relaxed);
  const float lvlWant = clampf(wantAmbLevel_.load(std::memory_order_relaxed), 0.0f, 1.0f);
  const float day = clampf(wantDay_.load(std::memory_order_relaxed), 0.0f, 1.0f);
  const int k = kind < 15 ? kind : 0;
  // ease toward the bed (about two seconds) and the level
  const float ke = 1.0f - std::exp(-blockSec / 0.7f);
  float* cur = &ambCur_.wind;
  const float night = 1.0f - day;
  for (int i = 0; i < 11; i++) {
    float t = kBed[k][i];
    if (i == 9 && (k == 1 || k == 4 || k == 5)) t *= 1.0f + 1.2f * night;   // insects louder at night
    cur[i] += (t - cur[i]) * ke;
  }
  ambLvl_ += (lvlWant - ambLvl_) * ke;
  ambDay_ += (day - ambDay_) * ke;
  ambT_ += blockSec;
  if (ambLvl_ < 1e-4f) return;
  // events
  for (int j = 0; j < 5; j++) {
    const EvRate& er = kBedEv[k][j];
    if (er.e < 0) continue;
    const float rate = er.day * ambDay_ + er.night * (1.0f - ambDay_);
    float& T = ambEvT_[er.e];
    T -= blockSec;
    if (T <= 0) {
      if (rate > 0.001f && ambLvl_ > 0.05f && T > -5.0f) ambEvent(er.e, ambLvl_ * kAmbGain * 2.0f);
      T = rate > 0.001f ? -std::log(std::fmax(1e-4f, rnd())) / rate : 1.0f;
    }
  }
  // gusts: a slow random target the wind envelope chases
  if (rnd() < blockSec * 0.25f) ambGustTarget_ = rnd(0.25f, 1.0f);
  ambGustEnv_ += (ambGustTarget_ - ambGustEnv_) * (1.0f - std::exp(-blockSec / 1.5f));
  const AmbP& A = ambCur_;
  const float g = ambLvl_ * kAmbGain;
  const float cw = 1.0f - std::exp(-TAU * A.windCut * (0.6f + 0.6f * ambGustEnv_) / FS);
  const float ch = 1.0f - std::exp(-TAU * A.hissCut / FS);
  const float cs = 1.0f - std::exp(-TAU * 900.0f / FS), cr = 1.0f - std::exp(-TAU * 70.0f / FS);
  const float cin = 1.0f - std::exp(-TAU * 3600.0f / FS), cru = 1.0f - std::exp(-TAU * 2200.0f / FS);
  // the howl's resonant band slides slowly (a wind whistling round rocks)
  const float howlF = 420.0f + 260.0f * (0.5f + 0.5f * sinc(ambT_ * 0.07f)) + 120.0f * ambGustEnv_;
  const float hg = std::tan(PI * howlF / FS), hk = 1.0f / 9.0f;
  const float ha1 = 1.0f / (1.0f + hg * (hg + hk)), ha2 = hg * ha1, ha3 = hg * ha2;
  // the surf's swell: a slow rise and a quicker fall, every ~8 s
  const float sp = std::fmod(ambT_ / 8.0f, 1.0f);
  const float swell = 0.25f + 0.75f * (sp < 0.7f ? sp / 0.7f * sp / 0.7f : 1.0f - (sp - 0.7f) / 0.3f);
  const float rumbleAm = 0.7f + 0.3f * sinc(ambT_ * 0.31f) * sinc(ambT_ * 0.13f);
  const float insectAm = 0.6f + 0.4f * sinc(ambT_ * 0.5f);
  const float rustleAm = 0.3f + 0.7f * ambGustEnv_ * ambGustEnv_;
  for (int i = 0; i < n; i++) {
    const float t = ambT_ - blockSec + (float)i * DT;
    const float w = white(ambRng_);
    float y = 0;
    // wind (two-pole low-passed noise with gusts)
    ambLp_[0] += cw * (w - ambLp_[0]);
    ambLp_[1] += cw * (ambLp_[0] - ambLp_[1]);
    y += ambLp_[1] * A.wind * (0.4f + ambGustEnv_) * 3.2f;
    // hiss (sand): what the low-pass leaves above the cut
    ambLp_[2] += ch * (w - ambLp_[2]);
    y += (w - ambLp_[2]) * A.hiss * (0.3f + ambGustEnv_) * 0.45f;
    // howl
    if (A.howl > 0.001f) {
      const float v3 = w - ambSv2_, v1 = ha1 * ambSv1_ + ha2 * v3, v2 = ambSv2_ + ha2 * ambSv1_ + ha3 * v3;
      ambSv1_ = 2.0f * v1 - ambSv1_;
      ambSv2_ = 2.0f * v2 - ambSv2_;
      y += hk * v1 * A.howl * (0.3f + ambGustEnv_) * 4.0f;
    }
    // surf
    if (A.surf > 0.001f) {
      ambLp_[3] += cs * (w - ambLp_[3]);
      y += ambLp_[3] * A.surf * swell * 2.2f;
    }
    // rumble
    if (A.rumble > 0.001f) {
      ambLp_[4] += cr * (w - ambLp_[4]);
      ambLp_[5] += cr * (ambLp_[4] - ambLp_[5]);
      y += ambLp_[5] * A.rumble * rumbleAm * 14.0f;
    }
    // hum (crystal, mystic): beating glassy partials
    if (A.hum > 0.001f) y += (sinc(t * 440.0f) * 0.5f + sinc(t * 441.3f) * 0.5f + sinc(t * 660.7f) * 0.35f + sinc(t * 1318.5f) * 0.12f) * A.hum * 0.11f * (0.7f + 0.3f * sinc(t * 0.21f));
    // drone (blight): a low, sick beating
    if (A.drone > 0.001f) y += (sinc(t * 55.0f) + 0.6f * sinc(t * 55.6f) + 0.35f * sinc(t * 82.9f) + 0.2f * sinc(t * 110.4f)) * A.drone * 0.12f;
    // insects: the band above ~3.6 kHz, pulsing fast
    if (A.insects > 0.001f) {
      ambLp_[6] += cin * (w - ambLp_[6]);
      y += (w - ambLp_[6]) * A.insects * insectAm * (0.55f + 0.45f * sinc(t * 31.0f)) * 0.5f;
    }
    // rustle: leaves in the gusts
    if (A.rustle > 0.001f) {
      ambLp_[7] += cru * (w - ambLp_[7]);
      y += (w - ambLp_[7]) * A.rustle * rustleAm * (0.6f + 0.4f * sinc(t * 3.7f) * sinc(t * 1.3f)) * 0.5f;
    }
    out[i] = y * g;
  }
}

// ------------------------------------------------------------------------------------------ (M10) the sea bed
// The sea round the boat: the swell washing under the hull (low-passed noise breathing on two overlapping wave
// periods), the shore's breakers (a slow rise and a crash), the wind in the sails (a resonant band that thrums with
// the gusts), a storm (the gale, heavy rain, the seas heaving), the Mist (a cold beating drone that turns dissonant as
// it turns the boat back). Sparse events as voices: gulls' cries, a timber's creak, a halyard's tap, oar strokes, a
// wave slapping the hull, whispers in the fog. A handful of one-pole filters per sample: cheap on a phone.
void Audio::renderSea(float* out, int n, float blockSec) {
  float want[9];
  bool any = false;
  for (int i = 0; i < 9; i++) {
    want[i] = clampf(wantSea_[i].load(std::memory_order_relaxed), 0.0f, 4.0f);
    any = any || want[i] > 0.001f || seaCur_[i] > 0.001f;
  }
  if (!any) return;
  const float ke = 1.0f - std::exp(-blockSec / 0.8f);
  for (int i = 0; i < 9; i++) seaCur_[i] += (want[i] - seaCur_[i]) * ke;
  const float open = seaCur_[0], surf = seaCur_[1], gulls = seaCur_[2], wind = seaCur_[3], creak = seaCur_[4];
  const float oars = seaCur_[5], storm = seaCur_[6], mist = seaCur_[7], turn = seaCur_[8];
  seaT_ += blockSec;
  // ---- events (voices)
  auto due = [&](int e, float rate) {
    float& T = seaEvT_[e];
    T -= blockSec;
    if (T > 0) return false;
    T = rate > 0.001f ? -std::log(std::fmax(1e-4f, rnd())) / rate : 1.0f;
    return rate > 0.001f;
  };
  if (due(0, gulls * 0.35f)) {   // a gull: a falling "keee-ow", sometimes two or three
    const int nc = 1 + (int)(rnd() * 2.5f);
    const float f = rnd(1500, 2100), v = gulls * 0.10f;
    for (int i = 0; i < nc; i++)
      add(NB(Saw, f * rnd(0.95f, 1.05f), v).env(0.02f, 0.3f, 0, 0.18f, 0.08f).slide(-0.9f).vox(f * 1.2f, 3.0f).vib(0.5f, 18).send(0.4f).at(0.28f * (float)i));
  }
  if (due(1, creak * 0.45f)) {   // a timber's creak / the rigging's groan
    const float f = rnd(90, 210), v = creak * 0.07f;
    add(NB(Saw, f, v).env(0.08f, 1, 1, rnd(0.25f, 0.6f), 0.15f).slide(rnd(-0.4f, 0.4f)).vox(rnd(400, 900), 3.5f).am(rnd(18, 34), 0.7f).send(0.2f));
  }
  if (due(2, creak * 0.25f + storm * 0.3f)) {   // a halyard tapping the mast, a block knocking
    const float f = rnd(500, 900), v = (creak * 0.5f + storm * 0.5f) * 0.08f;
    add(NB(Sine, f, v).perc(0.05f).pitch(0.5f, 0.01f));
    add(NB(Noise, 0, v * 0.6f).perc(0.01f).bp(f * 2.0f, 2.0f));
  }
  if (oars > 0.05f && due(3, oars)) trigger(Sfx::OarStroke, rnd(0.95f, 1.05f), 0.35f);
  if (due(4, (open * 0.25f + storm * 0.6f) * (creak > 0.01f ? 1.0f : 0.0f))) trigger(Sfx::WaveCrash, rnd(0.9f, 1.25f), 0.10f + 0.10f * storm);
  if (due(5, mist * 0.12f)) trigger(Sfx::MistWhisper, turn > 0.5f ? 0.94f : 1.0f, 0.18f * mist);
  // ---- the continuous layers
  const float swellA = 0.5f + 0.5f * sinc(seaT_ / 7.3f), swellB = 0.5f + 0.5f * sinc(seaT_ / 4.1f + 0.3f);
  const float swell = 0.35f + 0.65f * (0.6f * swellA + 0.4f * swellB) * (1.0f + storm * 0.3f);
  const float sp = std::fmod(seaT_ / (8.5f - storm * 2.5f), 1.0f);
  const float breaker = sp < 0.75f ? (sp / 0.75f) * (sp / 0.75f) : 1.0f - (sp - 0.75f) / 0.25f;
  const float gust = 0.55f + 0.45f * sinc(seaT_ * 0.13f) * sinc(seaT_ * 0.37f);
  const float cWash = 1.0f - std::exp(-TAU * (260.0f + 300.0f * storm) / FS), cSurf = 1.0f - std::exp(-TAU * 1100.0f / FS);
  const float cWind = 1.0f - std::exp(-TAU * (300.0f + 500.0f * wind * gust) / FS), cRain = 1.0f - std::exp(-TAU * 5000.0f / FS);
  const float droneHz = 73.4f, beat = turn > 0.5f ? 1.0f + 0.059f * turn : 1.006f;   // a tritone creeping in as it turns
  for (int i = 0; i < n; i++) {
    const float t = seaT_ - blockSec + (float)i * DT;
    const float w = white(seaRng_);
    float y = 0;
    // the swell's wash
    seaLp_[0] += cWash * (w - seaLp_[0]);
    seaLp_[1] += cWash * (seaLp_[0] - seaLp_[1]);
    y += seaLp_[1] * (open * 1.5f + storm * 0.5f) * swell * 2.4f;
    // the shore's breakers
    if (surf > 0.001f) {
      seaLp_[2] += cSurf * (w - seaLp_[2]);
      y += seaLp_[2] * surf * (0.25f + 0.75f * breaker) * 1.6f + (w - seaLp_[2]) * surf * breaker * breaker * 0.25f;
    }
    // the wind in the sails / the gale
    if (wind > 0.001f || storm > 0.001f) {
      seaLp_[3] += cWind * (w - seaLp_[3]);
      seaLp_[4] += cWind * (seaLp_[3] - seaLp_[4]);
      const float thrum = 1.0f + 0.25f * sinc(t * (9.0f + 4.0f * gust)) * wind;
      y += seaLp_[4] * (wind * 1.1f + storm * 0.6f) * gust * thrum * 2.2f;
    }
    // the storm's rain
    if (storm > 0.001f) {
      seaLp_[5] += cRain * (w - seaLp_[5]);
      y += (w - seaLp_[5]) * storm * 0.14f;
    }
    // the Mist's drone
    if (mist > 0.001f)
      y += (sinc(t * droneHz) + 0.8f * sinc(t * droneHz * beat) + 0.35f * sinc(t * droneHz * 1.5f * beat) + 0.2f * sinc(t * droneHz * 4.02f)) * mist * 0.05f *
           (0.7f + 0.3f * sinc(t * 0.17f));
    out[i] += y * kAmbGain * 0.6f;
  }
}

// ------------------------------------------------------------------------------------------ (M11) the falls' roar
// A waterfall heard from nearby (Audio::setFalls, the FALLS lane): a deep rush (noise through two low-pass poles,
// lower and heavier for a great fall), a body of mid-band hiss (the sheet tearing on the rock), and spatter on top
// (bright clicks of droplets in short random bursts). A slow swell breathes through it so it never reads as static
// noise. Level and size ease on the audio thread (a window shift or a new nearest fall never pops). The mixer is mono:
// the side (pan) only tilts the tone (the far side duller), it cannot place the sound.
void Audio::renderFalls(float* out, int n, float blockSec) {
  const float wantL = clampf(wantFalls_[0].load(std::memory_order_relaxed), 0.0f, 1.0f);
  const float wantS = clampf(wantFalls_[2].load(std::memory_order_relaxed), 0.0f, 1.0f);
  const float pan = clampf(wantFalls_[1].load(std::memory_order_relaxed), -1.0f, 1.0f);
  if (wantL < 0.0005f && fallsCur_[0] < 0.0005f) { fallsCur_[0] = 0; return; }
  const float ke = 1.0f - std::exp(-blockSec / 0.35f), ks = 1.0f - std::exp(-blockSec / 1.5f);
  const float l0 = fallsCur_[0];
  fallsCur_[0] += (wantL - fallsCur_[0]) * ke;
  fallsCur_[1] += (wantS - fallsCur_[1]) * ks;
  const float l1 = fallsCur_[0], size = fallsCur_[1];
  fallsT_ += blockSec;
  // the filters by size: a little cascade hisses high, a great fall thunders low
  const float cDeep = 1.0f - std::exp(-TAU * (420.0f - 260.0f * size) / FS);
  const float cBody = 1.0f - std::exp(-TAU * (2600.0f - 1200.0f * size - 500.0f * std::fabs(pan)) / FS);
  const float cHi = 1.0f - std::exp(-TAU * 5200.0f / FS);
  const float swell = 0.82f + 0.12f * sinc(fallsT_ * 0.21f) + 0.06f * sinc(fallsT_ * 0.53f + 1.3f);
  const float wDeep = 2.6f + 2.2f * size, wBody = 0.9f - 0.3f * size, wSpat = 0.22f - 0.08f * size;
  for (int i = 0; i < n; i++) {
    const float lv = l0 + (l1 - l0) * ((float)i / (float)n);   // per-sample ramp: no zipper steps
    const float w = white(fallsRng_);
    // the deep rush
    fallsLp_[0] += cDeep * (w - fallsLp_[0]);
    fallsLp_[1] += cDeep * (fallsLp_[0] - fallsLp_[1]);
    float y = fallsLp_[1] * wDeep;
    // the body: band-limited hiss (low-pass minus a lower low-pass)
    fallsLp_[2] += cBody * (w - fallsLp_[2]);
    fallsLp_[3] += cDeep * (fallsLp_[2] - fallsLp_[3]);
    y += (fallsLp_[2] - fallsLp_[3]) * wBody;
    // spatter: bright clicks gated in short random bursts
    fallsGateT_ -= DT;
    if (fallsGateT_ <= 0) {
      fallsGateT_ = 0.004f + 0.03f * (float)(xs32(fallsRng_) & 1023) / 1023.0f;
      fallsGate_ = (float)(xs32(fallsRng_) & 1023) / 1023.0f;
      fallsGate_ *= fallsGate_;
    }
    fallsLp_[4] += cHi * (w - fallsLp_[4]);
    y += (w - fallsLp_[4]) * fallsGate_ * wSpat;
    out[i] += y * swell * lv * kAmbGain * 0.48f * (0.8f + 0.6f * size);   // a great fall is louder as well as lower
  }
}

// ------------------------------------------------------------------------------------------ (M5) the crowd
// A tavern's murmur: five talkers, each a glottal buzz (a saw near 100..240 Hz with a drifting intonation) through two
// formant band-passes whose vowel changes every syllable, in phrases with pauses; overlapping, low-passed by the room.
// Events on top: laughter (a voice's "ha-ha-ha" falling), a mug set down on wood, a chair scraped back. No allocation;
// two state-variable filters per talker per sample (about 10 per sample in all: the phone does not notice).
void Audio::renderCrowd(float* out, int n, float blockSec) {
  const float want = clampf(wantCrowd_.load(std::memory_order_relaxed), 0.0f, 1.0f);
  const float lively = clampf(wantLively_.load(std::memory_order_relaxed), 0.0f, 1.0f);
  const float ke = 1.0f - std::exp(-blockSec / 0.8f);
  crowdLvl_ += (want - crowdLvl_) * ke;
  crowdLively_ += (lively - crowdLively_) * ke;
  crowdT_ += blockSec;
  if (crowdLvl_ < 1e-4f) return;
  // how many are talking: a quiet room two, a packed one all five
  const int active = 1 + (int)std::lround(crowdLvl_ * (float)(TALKERS - 1));
  auto cr = [&]() { return unit(crowdRng_); };
  static const float kVowel[6][2] = {{730, 1090}, {530, 1840}, {390, 2300}, {570, 840}, {440, 1020}, {660, 1700}};
  for (int t = 0; t < TALKERS; t++) {
    Talker& T = talk_[t];
    T.on = t < active;
    T.phraseT -= blockSec;
    if (T.phraseT <= 0) {   // a phrase begins (a new speaker's voice now and then) or a pause
      T.talking = T.on && !T.talking;
      T.phraseT = T.talking ? 0.8f + cr() * 2.4f : 0.3f + cr() * (1.8f - crowdLvl_);
      if (T.talking && cr() < 0.3f) T.f0t = (cr() < 0.45f ? 190.0f : 110.0f) * (0.85f + 0.35f * cr());
      if (T.f0 <= 0) T.f0 = T.f0t;
    }
    T.sylT -= blockSec;
    if (T.sylT <= 0) {   // the next syllable: a vowel, a pitch inflection
      T.sylT = 0.09f + cr() * 0.16f;
      const float* v = kVowel[(int)(cr() * 5.99f)];
      T.f1 = v[0] * (0.9f + 0.2f * cr());
      T.f2 = v[1] * (0.9f + 0.2f * cr());
      T.f0 = T.f0t * (0.9f + 0.25f * cr());
    }
  }
  // events: laughter, a mug, a chair (busier on a lively night)
  const float evK = crowdLvl_ * (0.6f + crowdLively_);
  const float rates[3] = {0.14f * evK, 0.35f * evK, 0.05f * evK};
  for (int e = 0; e < 3; e++) {
    crowdEvT_[e] -= blockSec;
    if (crowdEvT_[e] > 0) continue;
    crowdEvT_[e] = rates[e] > 0.001f ? -std::log(std::fmax(1e-4f, cr())) / rates[e] : 2.0f;
    if (rates[e] <= 0.001f) continue;
    const float V = crowdLvl_ * 0.5f;
    if (e == 0) {   // laughter: 3-5 falling "ha"s
      const float f = (cr() < 0.5f ? 230.0f : 140.0f) * (0.9f + 0.2f * cr());
      const int nh = 3 + (int)(cr() * 3);
      for (int h = 0; h < nh; h++)
        add(NB(Saw, f * (1.0f - 0.04f * (float)h), 0.10f * V * (1.0f - 0.12f * (float)h)).env(0.01f, 0.08f, 0.0f, 0.07f, 0.04f)
                .vox(780, 2.0f).pitch(0.3f, 0.02f).at(0.13f * (float)h).send(0.4f));
    } else if (e == 1) {   // a mug set down: a dull knock on the board and the pewter's short ring
      add(NB(Sine, 180 * (0.9f + 0.2f * cr()), 0.18f * V).perc(0.05f).pitch(0.8f, 0.01f).send(0.3f));
      add(NB(Fm, 1800 * (0.9f + 0.2f * cr()), 0.05f * V).perc(0.12f).fm(1.41f, 1.5f, 0.05f).send(0.3f));
    } else {   // a chair scraped back over the boards
      add(NB(Noise, 0, 0.10f * V).env(0.04f, 0.25f, 0.6f, 0.22f, 0.06f).bp(900 * (0.8f + 0.4f * cr()), 3.0f).am(38, 0.6f).send(0.3f));
    }
  }
  const float g = crowdLvl_ * 0.11f;
  const float ks = 1.0f - std::exp(-DT / 0.025f);   // a syllable's onset and fall (per sample)
  for (int t = 0; t < TALKERS; t++) {
    Talker& T = talk_[t];
    const float target = T.on && T.talking ? (T.sylT > 0.035f ? 1.0f : 0.25f) : 0.0f;
    if (T.env < 1e-4f && target <= 0) { T.env = 0; continue; }
    // per-sample SVF coefficients (formants change per syllable: computed once per block)
    const float g1 = std::tan(PI * std::fmin(T.f1, 4000.0f) / FS), k1 = 1.0f / 4.0f;
    const float a11 = 1.0f / (1.0f + g1 * (g1 + k1)), a12 = g1 * a11, a13 = g1 * a12;
    const float g2 = std::tan(PI * std::fmin(T.f2, 6000.0f) / FS), k2 = 1.0f / 6.0f;
    const float a21 = 1.0f / (1.0f + g2 * (g2 + k2)), a22 = g2 * a21, a23 = g2 * a22;
    const float inc = T.f0 * DT;
    for (int i = 0; i < n; i++) {
      T.env += (target - T.env) * ks;
      T.ph += inc;
      if (T.ph >= 1.0f) T.ph -= 1.0f;
      const float src = (2.0f * T.ph - 1.0f) * 0.7f + white(crowdRng_) * 0.25f;   // buzz plus breath
      // formant 1
      float v3 = src - T.s1b, v1 = a11 * T.s1a + a12 * v3, v2 = T.s1b + a12 * T.s1a + a13 * v3;
      T.s1a = 2.0f * v1 - T.s1a; T.s1b = 2.0f * v2 - T.s1b;
      const float y1 = k1 * v1;
      v3 = src - T.s2b; v1 = a21 * T.s2a + a22 * v3; v2 = T.s2b + a22 * T.s2a + a23 * v3;
      T.s2a = 2.0f * v1 - T.s2a; T.s2b = 2.0f * v2 - T.s2b;
      const float y2 = k2 * v1;
      out[i] += (y1 * 1.4f + y2 * 0.8f) * T.env * g;
    }
  }
}
