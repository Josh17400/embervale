// audio_preview: renders every Sfx and 40 s of every Music mode offline (no audio device) to 16-bit WAVs and
// prints level / spectral / musical-structure statistics so the synth can be checked without listening.
// M3: then the culture pieces: the Town piece of each of the 12 archetypes (their MusicStyles as the culture engine's
// priors draw them, mid-range), a Wild, a Night and a Combat in culture styles, and a border crossing (Town in one
// style crossfading into Town in another), each checked for clipping, gaps, loudness against the classic Town, and
// clicks at the crossing.
//
// M3c (LIFE lane): the 15 ambient beds (by day, then by night) and the Wild piece in each of the 11 land moods, each
// checked for clipping, clicks (sample jumps far above the bed's own) and level, and a bed-to-bed and a mood-to-mood
// crossfade checked for clicks.
//
// M5 (AMBIENCE lane): the bard's tavern piece in every archetype's style and the festival dance, the crowd's murmur,
// the bard heard from the street (--tavern; also part of the full run), and the animals' and the crowd's sfx.
//
//   audio_preview [outDir] [--cultures] [--wild] [--tavern]   default outDir: %LOCALAPPDATA%\Temp\claude\embervale_audio;
//                                                  --cultures: only the culture pieces; --wild: only the beds and moods;
//                                                  --tavern: only the tavern
#include "engine/audio.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <complex>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace {
constexpr int SR = 48000;
namespace fs = std::filesystem;

const char* kSfxName[(int)Sfx::COUNT] = {
  "Swing", "Hit", "HitHeavy", "Block", "Hurt", "PlayerHurt", "EnemyDie", "Death",
  "Arrow", "ArrowHit", "Fireball", "Explode", "Frost", "Heal", "Roll", "Step",
  "Pickup", "Coin", "Chest", "Door", "Buy", "Talk",
  "LevelUp", "QuestStart", "QuestDone", "Discover",
  "MenuMove", "MenuSelect", "MenuBack",
  "Roar", "Splash",
  "Bell",
  "Bark", "Cluck", "Meow", "Cheer",
  "HarpyShriek", "GolemSlam", "EliteSting", "BossRoar",
  "Hoof",
};
static_assert(sizeof(kSfxName) / sizeof(kSfxName[0]) == (size_t)Sfx::COUNT, "a name for every Sfx");
const char* kMusicName[(int)Music::COUNT] = {"Silence", "Title", "Wild", "Night", "Town", "Cave", "Combat", "Boss", "Tavern"};

bool writeWav(const fs::path& path, const std::vector<float>& x) {
  FILE* f = std::fopen(path.string().c_str(), "wb");
  if (!f) return false;
  auto u32 = [&](uint32_t v) { std::fwrite(&v, 4, 1, f); };
  auto u16 = [&](uint16_t v) { std::fwrite(&v, 2, 1, f); };
  const uint32_t bytes = (uint32_t)x.size() * 2;
  std::fwrite("RIFF", 1, 4, f); u32(36 + bytes); std::fwrite("WAVEfmt ", 1, 8, f);
  u32(16); u16(1); u16(1); u32(SR); u32(SR * 2); u16(2); u16(16);
  std::fwrite("data", 1, 4, f); u32(bytes);
  std::vector<int16_t> pcm(x.size());
  for (size_t i = 0; i < x.size(); i++) {
    float v = x[i] < -1.0f ? -1.0f : (x[i] > 1.0f ? 1.0f : x[i]);
    pcm[i] = (int16_t)std::lround(v * 32767.0f);
  }
  bool ok = std::fwrite(pcm.data(), 2, pcm.size(), f) == pcm.size();
  return std::fclose(f) == 0 && ok;
}

void fft(std::vector<std::complex<float>>& a) {   // in-place radix-2
  const size_t n = a.size();
  for (size_t i = 1, j = 0; i < n; i++) {
    size_t bit = n >> 1;
    for (; j & bit; bit >>= 1) j ^= bit;
    j ^= bit;
    if (i < j) std::swap(a[i], a[j]);
  }
  for (size_t len = 2; len <= n; len <<= 1) {
    const float ang = -6.28318530718f / (float)len;
    const std::complex<float> wl(std::cos(ang), std::sin(ang));
    for (size_t i = 0; i < n; i += len) {
      std::complex<float> w(1);
      for (size_t k = 0; k < len / 2; k++, w *= wl) {
        auto u = a[i + k], v = a[i + k + len / 2] * w;
        a[i + k] = u + v;
        a[i + k + len / 2] = u - v;
      }
    }
  }
}

// magnitude spectrum of a Hann-windowed frame starting at `at`
std::vector<float> spectrum(const std::vector<float>& x, size_t at, size_t n) {
  std::vector<std::complex<float>> a(n);
  for (size_t i = 0; i < n; i++) {
    float w = 0.5f - 0.5f * std::cos(6.28318530718f * (float)i / (float)(n - 1));
    a[i] = at + i < x.size() ? x[at + i] * w : 0.0f;
  }
  fft(a);
  std::vector<float> m(n / 2);
  for (size_t i = 0; i < n / 2; i++) m[i] = std::abs(a[i]);
  return m;
}

struct Levels { float peak = 0, rms = 0, activeRms = 0, dur = 0, activeDur = 0; int clipped = 0; };
Levels levels(const std::vector<float>& x, size_t from = 0) {
  Levels L;
  double ss = 0;
  size_t last = from;
  for (size_t i = from; i < x.size(); i++) {
    float a = std::fabs(x[i]);
    L.peak = std::fmax(L.peak, a);
    if (a >= 0.999f) L.clipped++;
    ss += (double)x[i] * x[i];
    if (a > 1e-3f) last = i;
  }
  const size_t n = x.size() > from ? x.size() - from : 1;
  L.rms = (float)std::sqrt(ss / (double)n);
  double sa = 0;
  for (size_t i = from; i <= last && i < x.size(); i++) sa += (double)x[i] * x[i];
  L.activeRms = (float)std::sqrt(sa / (double)(last - from + 1));
  L.dur = (float)x.size() / SR;
  L.activeDur = (float)(last - from + 1) / SR;
  return L;
}
float db(float v) { return 20.0f * std::log10(std::fmax(v, 1e-9f)); }

// What a phone speaker roughly reproduces: 2nd-order high-pass at 200 Hz (RBJ biquad).
std::vector<float> phoneSpeaker(const std::vector<float>& x) {
  const double w = 6.283185307 * 200.0 / SR, al = std::sin(w) / (2 * 0.7071), c = std::cos(w), a0 = 1 + al;
  const double b0 = (1 + c) / 2 / a0, b1 = -(1 + c) / a0, b2 = b0, a1 = -2 * c / a0, a2 = (1 - al) / a0;
  std::vector<float> y(x.size());
  double x1 = 0, x2 = 0, y1 = 0, y2 = 0;
  for (size_t i = 0; i < x.size(); i++) {
    double o = b0 * x[i] + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
    x2 = x1; x1 = x[i]; y2 = y1; y1 = o;
    y[i] = (float)o;
  }
  return y;
}
// RMS of the loudest 200 ms window (short-term loudness), and the mean RMS from `from` on
struct Loud { float maxWin = 0, mean = 0; };
Loud loudness(const std::vector<float>& x, size_t from = 0) {
  Loud L;
  const size_t w = SR / 5;
  double total = 0;
  for (size_t at = from; at < x.size(); at += w / 4) {
    double ss = 0;
    size_t end = std::min(x.size(), at + w);
    for (size_t i = at; i < end; i++) ss += (double)x[i] * x[i];
    L.maxWin = std::fmax(L.maxWin, (float)std::sqrt(ss / (double)w));
  }
  for (size_t i = from; i < x.size(); i++) total += (double)x[i] * x[i];
  L.mean = x.size() > from ? (float)std::sqrt(total / (double)(x.size() - from)) : 0.0f;
  return L;
}

// power-weighted spectral centroid of the loudest 85 ms window, and the share of its power above 4 kHz
struct Tone { float centroid = 0, hf = 0; };
Tone tone(const std::vector<float>& x) {
  const size_t n = 4096;
  size_t best = 0;
  double bestE = -1;
  for (size_t at = 0; at + n <= x.size() || at == 0; at += 1024) {
    double e = 0;
    for (size_t i = at; i < at + n && i < x.size(); i++) e += (double)x[i] * x[i];
    if (e > bestE) { bestE = e; best = at; }
    if (at + n > x.size()) break;
  }
  auto m = spectrum(x, best, n);
  double num = 0, den = 0, hf = 0;
  for (size_t i = 1; i < m.size(); i++) {
    double f = (double)i * SR / n, pw = (double)m[i] * m[i];
    num += f * pw;
    den += pw;
    if (f > 4000) hf += pw;
  }
  return den > 0 ? Tone{(float)(num / den), (float)(hf / den)} : Tone{};
}

// Longest stretch (seconds) where 100 ms windows sit below -60 dBFS, ignoring the first `skip` seconds.
float longestSilence(const std::vector<float>& x, float skip) {
  const size_t w = SR / 10;
  float run = 0, best = 0;
  for (size_t at = (size_t)(skip * SR); at + w <= x.size(); at += w) {
    double ss = 0;
    for (size_t i = at; i < at + w; i++) ss += (double)x[i] * x[i];
    if (db((float)std::sqrt(ss / (double)w)) < -60.0f) best = std::fmax(best, run += 0.1f);
    else run = 0;
  }
  return best;
}

struct Structure {
  float onsetsPerMin = 0;    // spectral-flux note onsets
  float pitchChangesPerMin = 0;  // changes of the dominant melody-band pitch
  float bestLoopMatch = 0;   // best fraction of identical dominant pitches at any lag 4..20 s (1.0 = exact loop)
  float bestLoopLag = 0;
  float chromaSim = 0;       // best mean chroma cosine similarity at any lag 4..20 s (harmony repeats are expected)
};

Structure structure(const std::vector<float>& x, float skip) {
  Structure S;
  const size_t n = 8192, hop = 2048;
  std::vector<std::vector<float>> mags;
  std::vector<std::array<float, 12>> chroma;
  std::vector<int> domPitch;
  for (size_t at = (size_t)(skip * SR); at + n <= x.size(); at += hop) {
    auto m = spectrum(x, at, n);
    std::array<float, 12> c{};
    std::vector<float> pitchE(128, 0.0f);
    for (size_t i = 2; i < m.size(); i++) {
      float f = (float)i * SR / (float)n;
      if (f < 55 || f > 2500) continue;
      int midi = (int)std::lround(69 + 12 * std::log2(f / 440.0f));
      c[(size_t)((midi % 12 + 12) % 12)] += m[i] * m[i];
      if (f > 300 && f < 2200) pitchE[(size_t)midi] += m[i] * m[i];
    }
    float norm = 0;
    for (float v : c) norm += v * v;
    norm = std::sqrt(norm) + 1e-12f;
    for (float& v : c) v /= norm;
    chroma.push_back(c);
    int arg = -1;
    float best = 0, total = 0;
    for (int k = 0; k < 128; k++) { total += pitchE[(size_t)k]; if (pitchE[(size_t)k] > best) { best = pitchE[(size_t)k]; arg = k; } }
    domPitch.push_back(best > 0.25f * total && best > 1e-3f ? arg : -1);   // -1 = no clear melody pitch
    mags.push_back(std::move(m));
  }
  const float frameSec = (float)hop / SR, minutes = (float)mags.size() * frameSec / 60.0f;
  if (mags.size() < 4 || minutes <= 0) return S;

  // onsets: half-wave rectified log-spectral flux, adaptive threshold, 60 ms refractory
  std::vector<float> flux(mags.size(), 0.0f);
  for (size_t f = 1; f < mags.size(); f++)
    for (size_t i = 2; i < 600; i++) {
      float d = std::log1p(10 * mags[f][i]) - std::log1p(10 * mags[f - 1][i]);
      if (d > 0) flux[f] += d;
    }
  int onsets = 0;
  for (size_t f = 1; f + 1 < flux.size(); f++) {
    float mean = 0;
    int cnt = 0;
    for (size_t g = f > 8 ? f - 8 : 0; g < f + 8 && g < flux.size(); g++, cnt++) mean += flux[g];
    mean /= (float)cnt;
    if (flux[f] > flux[f - 1] && flux[f] >= flux[f + 1] && flux[f] > 1.4f * mean + 1.0f) onsets++;
  }
  S.onsetsPerMin = (float)onsets / minutes;

  int changes = 0;
  for (size_t f = 1; f < domPitch.size(); f++)
    if (domPitch[f] >= 0 && domPitch[f - 1] >= 0 && domPitch[f] != domPitch[f - 1]) changes++;
  S.pitchChangesPerMin = (float)changes / minutes;

  for (size_t lag = (size_t)(4.0f / frameSec); lag <= (size_t)(20.0f / frameSec) && lag + 20 < chroma.size(); lag++) {
    int same = 0, both = 0;
    float sim = 0;
    for (size_t f = 0; f + lag < chroma.size(); f++) {
      float d = 0;
      for (int k = 0; k < 12; k++) d += chroma[f][(size_t)k] * chroma[f + lag][(size_t)k];
      sim += d;
      if (domPitch[f] >= 0 && domPitch[f + lag] >= 0) { both++; if (domPitch[f] == domPitch[f + lag]) same++; }
    }
    sim /= (float)(chroma.size() - lag);
    S.chromaSim = std::fmax(S.chromaSim, sim);
    float match = both > 20 ? (float)same / (float)both : 0.0f;
    if (match > S.bestLoopMatch) { S.bestLoopMatch = match; S.bestLoopLag = (float)lag * frameSec; }
  }
  return S;
}

std::vector<float> renderFor(Audio& a, float seconds) {
  std::vector<float> out((size_t)(seconds * SR));
  for (size_t at = 0; at < out.size(); at += 512) a.render(out.data() + at, (int)std::min<size_t>(512, out.size() - at));
  return out;
}

// render until 0.5 s of near-silence follows the sound (max 10 s)
std::vector<float> renderSfx(Audio& a) {
  std::vector<float> out;
  float buf[480];
  int quiet = 0;
  while (out.size() < (size_t)SR * 10) {
    a.render(buf, 480);
    float pk = 0;
    for (float v : buf) pk = std::fmax(pk, std::fabs(v));
    out.insert(out.end(), buf, buf + 480);
    quiet = pk < 1e-4f ? quiet + 1 : 0;
    if (quiet >= 50 && out.size() > (size_t)SR / 10) break;
  }
  out.resize(out.size() - (size_t)quiet * 480 + std::min<size_t>((size_t)quiet * 480, SR / 20));   // keep 50 ms tail
  return out;
}

float maxStep(const std::vector<float>& x) {   // largest sample-to-sample jump: clicks show up here
  float m = 0;
  for (size_t i = 1; i < x.size(); i++) m = std::fmax(m, std::fabs(x[i] - x[i - 1]));
  return m;
}

// ---------------------------------------------------------------- M3 culture pieces
struct CultureStyle { const char* name; MusicStyle ms; };
MusicStyle mk(Scale sc, int bpm, int meter, LeadInst l, PadInst pad, BassInst bass, PercKind perc, int swing, int orn, int drone,
              int seed) {
  MusicStyle m;
  m.scale = sc; m.bpm = (uint8_t)bpm; m.meter = (uint8_t)meter; m.lead = l; m.pad = pad; m.bass = bass; m.perc = perc;
  m.swing = (uint8_t)swing; m.ornament = (uint8_t)orn; m.drone = (uint8_t)drone; m.seed = (uint16_t)seed;
  return m;
}
// each archetype's style as rpg/culture/priors.cpp draws it (mid-range dials; the tool links only the engine)
const CultureStyle kCultures[12] = {
    {"fjordfolk", mk(Scale::Dorian, 76, 3, LeadInst::Horn, PadInst::Drone, BassInst::Drone, PercKind::Bodhran, 0, 3, 11, 101)},
    {"highland", mk(Scale::Mixolydian, 100, 6, LeadInst::Pipes, PadInst::Drone, BassInst::Drone, PercKind::Bodhran, 0, 11, 13, 202)},
    {"heartland", mk(Scale::Major, 92, 6, LeadInst::Lute, PadInst::Strings, BassInst::Plucked, PercKind::Frame, 0, 4, 1, 303)},
    {"imperial", mk(Scale::Lydian, 104, 4, LeadInst::Brass, PadInst::Organ, BassInst::Horn, PercKind::Kettle, 0, 2, 0, 404)},
    {"dune", mk(Scale::Hijaz, 106, 7, LeadInst::Oud, PadInst::Drone, BassInst::Plucked, PercKind::Hand, 0, 12, 7, 505)},
    {"steppe", mk(Scale::PentaMinor, 114, 4, LeadInst::Voice, PadInst::Drone, BassInst::Drone, PercKind::Frame, 3, 7, 13, 606)},
    {"marsh", mk(Scale::PentaMajor, 88, 4, LeadInst::Reed, PadInst::Bowed, BassInst::Hand, PercKind::Frame, 7, 6, 3, 707)},
    {"jade", mk(Scale::InSen, 72, 4, LeadInst::Bells, PadInst::Shimmer, BassInst::Plucked, PercKind::Gong, 0, 8, 3, 808)},
    {"river", mk(Scale::Major, 114, 4, LeadInst::Fiddle, PadInst::Organ, BassInst::Plucked, PercKind::Wood, 9, 4, 0, 909)},
    {"suntemple", mk(Scale::Phrygian, 106, 5, LeadInst::Marimba, PadInst::None, BassInst::Hand, PercKind::Hand, 3, 3, 1, 1010)},
    {"sylvan", mk(Scale::Minor, 80, 6, LeadInst::Flute, PadInst::Shimmer, BassInst::Plucked, PercKind::Bells, 0, 7, 2, 1111)},
    {"starspire", mk(Scale::Lydian, 66, 3, LeadInst::Harp, PadInst::Choir, BassInst::Bowed, PercKind::Bells, 0, 4, 4, 1212)},
};

int culturePieces(const fs::path& dir) {
  int problems = 0;
  const float secs = 32.0f;
  auto classic = std::make_unique<Audio>();
  classic->setMusic(Music::Town);
  const auto ref = renderFor(*classic, secs);
  const Loud RL = loudness(ref, 3 * SR);
  const Levels RLv = levels(ref, 3 * SR);
  std::printf("\nCULTURE PIECES (%.0f s each; classic Town: peak %.3f, rms %.1f dB, loudest 200ms %.1f dB)\n", secs, RLv.peak,
              db(RLv.rms), db(RL.maxWin));
  std::printf("%-22s %7s %7s %6s %6s %8s %8s %9s %9s %8s\n", "piece", "peak", "rms dB", "vs ref", "silent", "clip", "onset/m",
              "200ms dB", "phone dB", "pchg/m");
  auto judge = [&](const char* name, std::vector<float> x, bool town) {
    const Levels L = levels(x, 3 * SR);
    const Loud ML = loudness(x, 3 * SR), MP = loudness(phoneSpeaker(x), 3 * SR);
    const Structure S = structure(x, 3.0f);
    const float gap = longestSilence(x, 3.0f);
    const float vs = db(L.rms) - db(RLv.rms);
    std::printf("%-22s %7.3f %7.1f %+6.1f %5.1fs %6d %8.0f %9.1f %9.1f %8.0f\n", name, L.peak, db(L.rms), vs, gap, L.clipped,
                S.onsetsPerMin, db(ML.maxWin), db(MP.mean), S.pitchChangesPerMin);
    if (L.clipped || L.peak > 0.5f || gap > 4.0f || (town && (vs > 4.0f || vs < -6.0f))) { std::printf("  ^ PROBLEM\n"); problems++; }
    writeWav(dir / (std::string("culture_") + name + ".wav"), x);
  };
  for (const CultureStyle& c : kCultures) {
    auto a = std::make_unique<Audio>();
    a->setMusic(Music::Town, &c.ms);
    judge((std::string("town_") + c.name).c_str(), renderFor(*a, secs), true);
  }
  {
    auto a = std::make_unique<Audio>();
    a->setMusic(Music::Wild, &kCultures[10].ms);
    judge("wild_sylvan", renderFor(*a, secs), false);
  }
  {
    auto a = std::make_unique<Audio>();
    a->setMusic(Music::Night, &kCultures[4].ms);
    judge("night_dune", renderFor(*a, secs), false);
  }
  {
    auto a = std::make_unique<Audio>();
    a->setMusic(Music::Combat, &kCultures[4].ms);
    judge("combat_dune", renderFor(*a, 16.0f), false);
  }
  {   // the border crossing: Town in one style, then Town in another (a same-mode call must crossfade like a new piece);
      // a same-style call must change nothing; leaving the style returns to the classic piece
    auto a = std::make_unique<Audio>();
    std::vector<float> all;
    std::vector<size_t> sw;
    struct Leg { const MusicStyle* st; float sec; };
    const Leg legs[] = {{&kCultures[0].ms, 10}, {&kCultures[0].ms, 2}, {&kCultures[4].ms, 10}, {nullptr, 8}};
    uint64_t prev = 0;
    for (const Leg& l : legs) {
      a->setMusic(Music::Town, l.st);
      const uint64_t k = l.st ? l.st->pack() : 1;
      if (k != prev) sw.push_back(all.size());
      prev = k;
      auto part = renderFor(*a, l.sec);
      all.insert(all.end(), part.begin(), part.end());
    }
    float nearSwitch = 0, elsewhere = 0;
    for (size_t i = 1; i < all.size(); i++) {
      const float d = std::fabs(all[i] - all[i - 1]);
      bool near = false;
      for (size_t s0 : sw) near |= i >= s0 && i < s0 + SR / 4;
      (near ? nearSwitch : elsewhere) = std::fmax(near ? nearSwitch : elsewhere, d);
    }
    std::printf("border crossing (fjordfolk town -> same again -> dune town -> classic): peak %.3f, largest jump within 250 ms "
                "of a switch %.4f vs elsewhere %.4f, longest gap %.1fs\n", levels(all).peak, nearSwitch, elsewhere,
                longestSilence(all, 3.0f));
    if (nearSwitch > 1.5f * elsewhere || longestSilence(all, 3.0f) > 3.0f) { std::printf("PROBLEM: crossing artefact\n"); problems++; }
    writeWav(dir / "culture_border_fjordfolk_to_dune.wav", all);
  }
  std::printf("culture pieces: %s (%d problem%s)\n", problems ? "CHECK FAILED" : "all checks passed", problems, problems == 1 ? "" : "s");
  return problems;
}
const char* kBedName[15] = {"Breeze", "Meadow", "Woods", "DeepWoods", "Jungle", "Marsh", "Surf", "Cliffs", "DesertWind",
                            "ColdWind", "Volcanic", "Crystal", "Blight", "Bamboo", "Mystic"};
const char* kMoodName[11] = {"Pastoral", "Woodland", "Deep", "Exotic", "Wetland", "Coastal", "Arid", "Frozen", "Highland",
                             "Wondrous", "Ominous"};
// the largest jump between neighbouring samples (a click is a jump far above what the bed itself does)
float maxJump(const std::vector<float>& x, size_t from = 0) {
  float m = 0;
  for (size_t i = std::max<size_t>(1, from); i < x.size(); i++) m = std::fmax(m, std::fabs(x[i] - x[i - 1]));
  return m;
}
int wildlands(const fs::path& dir) {
  int problems = 0;
  std::printf("\n%-11s %7s %8s %7s %7s %6s %9s %8s\n", "BED", "peak", "peak dB", "rms dB", "night", "clip", "jump", "200ms dB");
  float bedMax = 0;
  for (int b = 0; b < 15; b++) {
    auto a = std::make_unique<Audio>();
    a->setAmbient((uint8_t)b, 1.0f);
    a->setDaylight(1.0f);
    auto x = renderFor(*a, 14.0f);
    a->setDaylight(0.0f);
    auto y = renderFor(*a, 10.0f);
    const Levels L = levels(x, 3 * SR), N = levels(y, 3 * SR);
    std::vector<float> all = x;
    all.insert(all.end(), y.begin(), y.end());
    const Loud LL = loudness(all, 3 * SR);
    const float jump = maxJump(all, 3 * SR);
    std::printf("%-11s %7.3f %8.1f %7.1f %7.1f %6d %9.4f %8.1f\n", kBedName[b], std::fmax(L.peak, N.peak), db(std::fmax(L.peak, N.peak)), db(L.rms),
                db(N.rms), L.clipped + N.clipped, jump, db(LL.maxWin));
    bedMax = std::fmax(bedMax, LL.mean);
    // a bed must be audible but sit well under the music and the sfx; no clipping; nothing jumps like a click
    if (L.clipped || N.clipped || std::fmax(L.peak, N.peak) > 0.45f || L.rms < 1e-3f || jump > 0.35f) { std::printf("  ^ PROBLEM\n"); problems++; }
    writeWav(dir / (std::string("bed_") + kBedName[b] + ".wav"), all);
  }
  {   // walking from the shore into the cold: Surf crossfades into ColdWind, then the player goes indoors (level 0)
    auto a = std::make_unique<Audio>();
    a->setAmbient(6, 1.0f);
    auto p1 = renderFor(*a, 6.0f);
    a->setAmbient(9, 1.0f);
    auto p2 = renderFor(*a, 5.0f);
    a->setAmbient(9, 0.0f);
    auto p3 = renderFor(*a, 4.0f);
    std::vector<float> all = p1;
    all.insert(all.end(), p2.begin(), p2.end());
    all.insert(all.end(), p3.begin(), p3.end());
    float tail = 0;
    for (size_t i = all.size() - SR; i < all.size(); i++) tail = std::fmax(tail, std::fabs(all[i]));
    const float jSwitch = maxJump(std::vector<float>(all.begin() + 6 * SR - 1, all.begin() + 6 * SR + SR / 2)), jElse = maxJump(p1, 3 * SR);
    std::printf("bed crossfade Surf -> ColdWind -> indoors: jump at the switch %.4f vs steady %.4f, last second peak %.5f\n", jSwitch, jElse, tail);
    if (jSwitch > 2.0f * jElse + 0.01f || tail > 0.02f) { std::printf("PROBLEM: bed crossfade artefact\n"); problems++; }
    writeWav(dir / "bed_crossfade.wav", all);
  }
  std::printf("\n%-9s %7s %8s %7s %7s %6s %8s\n", "MOOD", "peak", "peak dB", "rms dB", "silent", "clip", "onset/m");
  for (int m = 0; m < 11; m++) {
    auto a = std::make_unique<Audio>();
    a->setMood((uint8_t)m);
    a->setMusic(Music::Wild);
    auto x = renderFor(*a, 24.0f);
    const Levels L = levels(x, 3 * SR);
    const Structure S = structure(x, 3.0f);
    const float gap = longestSilence(x, 3.0f);
    std::printf("%-9s %7.3f %8.1f %7.1f %6.1fs %6d %8.0f\n", kMoodName[m], L.peak, db(L.peak), db(L.rms), gap, L.clipped, S.onsetsPerMin);
    if (L.clipped || gap > 6.0f || L.peak > 0.5f) { std::printf("  ^ PROBLEM\n"); problems++; }
    writeWav(dir / (std::string("mood_wild_") + kMoodName[m] + ".wav"), x);
  }
  {   // a mood change mid-piece crossfades (the Pastoral meadow into the Ominous blight), with the beds under it
    auto a = std::make_unique<Audio>();
    a->setMood(0); a->setAmbient(1, 1.0f); a->setMusic(Music::Wild);
    auto p1 = renderFor(*a, 8.0f);
    a->setMood(10); a->setAmbient(12, 1.0f);
    auto p2 = renderFor(*a, 8.0f);
    std::vector<float> all = p1;
    all.insert(all.end(), p2.begin(), p2.end());
    const Levels L = levels(all, 3 * SR);
    const float jSwitch = maxJump(std::vector<float>(all.begin() + 8 * SR - 1, all.begin() + 8 * SR + SR / 4)), jElse = maxJump(p1, 3 * SR);
    std::printf("mood crossfade Pastoral -> Ominous (with beds): peak %.3f, jump at the switch %.4f vs steady %.4f, longest gap %.1fs\n",
                L.peak, jSwitch, jElse, longestSilence(all, 3.0f));
    if (jSwitch > 1.5f * jElse + 0.01f || L.clipped) { std::printf("PROBLEM: mood crossfade artefact\n"); problems++; }
    writeWav(dir / "mood_crossfade.wav", all);
  }
  std::printf("wildlands audio: %s (%d problem%s); loudest bed mean %.1f dB\n", problems ? "CHECK FAILED" : "all checks passed", problems,
              problems == 1 ? "" : "s", db(bedMax));
  return problems;
}
}  // namespace

// ---------------------------------------------------------------- M5 the tavern
// The bard's piece (Music::Tavern) in every archetype's style, the festival dance in four, the crowd's murmur at three
// fills (a quiet room, a busy one, a packed festival night) and the bard heard from the street (setMusicLevel: quieter
// and muffled): each checked for clipping, gaps and its level against the classic Town piece (the tavern may stand a
// little above the street's music: it is the room's focus; the street's bleed well below it).
int tavernPieces(const fs::path& dir) {
  int problems = 0;
  const float secs = 24.0f;
  auto classic = std::make_unique<Audio>();
  classic->setMusic(Music::Town);
  const Levels RLv = levels(renderFor(*classic, secs), 3 * SR);
  std::printf("\nTAVERN (%.0f s each; classic Town rms %.1f dB)\n", secs, db(RLv.rms));
  std::printf("%-26s %7s %7s %6s %6s %6s %8s %9s\n", "piece", "peak", "rms dB", "vs ref", "silent", "clip", "onset/m", "phone dB");
  auto judge = [&](const std::string& name, const std::vector<float>& x, float lo, float hi, bool music) {
    const Levels L = levels(x, 3 * SR);
    const Loud MP = loudness(phoneSpeaker(x), 3 * SR);
    const Structure S = structure(x, 3.0f);
    const float gap = longestSilence(x, 3.0f);
    const float vs = db(L.rms) - db(RLv.rms);
    std::printf("%-26s %7.3f %7.1f %+6.1f %5.1fs %6d %8.0f %9.1f\n", name.c_str(), L.peak, db(L.rms), vs, gap, L.clipped, S.onsetsPerMin,
                db(MP.mean));
    if (L.clipped || L.peak > 0.5f || (music && gap > 4.0f) || vs > hi || vs < lo) { std::printf("  ^ PROBLEM\n"); problems++; }
    writeWav(dir / ("tavern_" + name + ".wav"), x);
  };
  for (const CultureStyle& c : kCultures) {
    auto a = std::make_unique<Audio>();
    a->setMusic(Music::Tavern, &c.ms, false);
    judge(std::string("bard_") + c.name, renderFor(*a, secs), -6.0f, 5.0f, true);
  }
  for (int ci : {0, 3, 4, 6}) {
    auto a = std::make_unique<Audio>();
    a->setMusic(Music::Tavern, &kCultures[ci].ms, true);
    judge(std::string("festival_") + kCultures[ci].name, renderFor(*a, secs), -6.0f, 6.0f, true);
  }
  {
    auto a = std::make_unique<Audio>();
    a->setMusic(Music::Tavern);
    judge("bard_classic", renderFor(*a, secs), -6.0f, 5.0f, true);
  }
  const float fills[3][2] = {{0.25f, 0}, {0.6f, 0.2f}, {1.0f, 1.0f}};
  const char* fillName[3] = {"crowd_quiet", "crowd_busy", "crowd_festival"};
  for (int k = 0; k < 3; k++) {   // the murmur alone: a bed well under the music (-26..-4 dB vs the Town piece)
    auto a = std::make_unique<Audio>();
    a->setCrowd(fills[k][0], fills[k][1]);
    judge(fillName[k], renderFor(*a, secs), -26.0f, -4.0f, false);
  }
  {   // the bard and a busy room together
    auto a = std::make_unique<Audio>();
    a->setMusic(Music::Tavern, &kCultures[2].ms, false);
    a->setCrowd(0.7f, 0.3f);
    judge("room_heartland", renderFor(*a, secs), -6.0f, 6.0f, true);
  }
  {   // from the street at night, ten tiles from the door: well below the town's own music, muffled
    auto a = std::make_unique<Audio>();
    a->setMusic(Music::Tavern, &kCultures[0].ms, false);
    a->setCrowd(0.5f, 0);
    a->setMusicLevel(0.35f, 0.75f);
    judge("street_fjordfolk", renderFor(*a, secs), -20.0f, -5.0f, false);
  }
  return problems;
}

int main(int argc, char** argv) {
  fs::path dir;
  bool culturesOnly = false, wildOnly = false, tavernOnly = false;
  for (int i = 1; i < argc; i++) {
    if (std::string(argv[i]) == "--cultures") culturesOnly = true;
    else if (std::string(argv[i]) == "--tavern") tavernOnly = true;
    else if (std::string(argv[i]) == "--wild") wildOnly = true;
    else if (argv[i][0] == '-') {
      // An unknown flag (e.g. --help) must never become the output directory: that once dumped 200 MB of WAVs
      // into a folder named "--help" in the repo root.
      std::fprintf(stderr, "usage: audio_preview [outDir] [--cultures] [--wild] [--tavern]\n");
      return std::string(argv[i]) == "--help" || std::string(argv[i]) == "-h" ? 0 : 2;
    } else dir = argv[i];
  }
  if (dir.empty()) {
    const char* la = std::getenv("LOCALAPPDATA");
    dir = fs::path(la ? la : ".") / "Temp" / "claude" / "embervale_audio";
  }
  std::error_code ec;
  fs::create_directories(dir, ec);
  if (ec) { std::fprintf(stderr, "cannot create %s: %s\n", dir.string().c_str(), ec.message().c_str()); return 1; }
  std::printf("output: %s\n(levels at default master 0.6, music volume 0.7)\n\n", dir.string().c_str());
  int problems = 0;
  if (wildOnly) {
    problems = wildlands(dir);
    return problems ? 2 : 0;
  }
  if (culturesOnly) {
    problems = culturePieces(dir);
    return problems ? 2 : 0;
  }
  if (tavernOnly) {
    problems = tavernPieces(dir);
    return problems ? 2 : 0;
  }

  // "raw" = peak the sound would reach without the limiter (rendered at half master, doubled); > 0.8 means limited
  // "200ms" = loudest 200 ms window; "phone" = the same through a 200 Hz high-pass (what a phone speaker plays)
  std::printf("%-11s %6s %7s %7s %8s %7s %7s %6s %5s %9s %9s\n", "SFX", "dur s", "raw", "peak", "peak dB", "rms dB", "cent Hz",
              " >4k%", "clip", "200ms dB", "phone dB");
  float sfxPeakMin = 1, sfxLoudMin = 1, sfxPhoneMin = 1;
  for (int s = 0; s < (int)Sfx::COUNT; s++) {
    auto a = std::make_unique<Audio>();
    a->play((Sfx)s);
    auto x = renderSfx(*a);
    Levels L = levels(x);
    const Tone T = tone(x);
    auto half = std::make_unique<Audio>();
    half->setMaster(0.3f);
    half->play((Sfx)s);
    const float raw = 2.0f * levels(renderSfx(*half)).peak;
    const float loud = loudness(x).maxWin, phone = loudness(phoneSpeaker(x)).maxWin;
    std::printf("%-11s %6.2f %7.3f %7.3f %8.1f %7.1f %7.0f %5.0f%% %5d %9.1f %9.1f\n", kSfxName[s], L.activeDur, raw, L.peak, db(L.peak),
                db(L.activeRms), T.centroid, 100.0f * T.hf, L.clipped, db(loud), db(phone));
    if (L.clipped || L.peak < 0.02f || L.activeDur < 0.02f) { std::printf("  ^ PROBLEM\n"); problems++; }
    if ((Sfx)s != Sfx::Step && (Sfx)s != Sfx::Talk && (Sfx)s != Sfx::MenuMove && (Sfx)s != Sfx::MenuSelect &&
        (Sfx)s != Sfx::MenuBack && (Sfx)s != Sfx::Bark && (Sfx)s != Sfx::Cluck && (Sfx)s != Sfx::Meow && (Sfx)s != Sfx::Cheer &&
        (Sfx)s != Sfx::Hoof) {
      // (M5) the animals and the crowd's cheer are the town's ambience, heard at a distance under the music by design
      sfxPeakMin = std::fmin(sfxPeakMin, L.peak);
      sfxLoudMin = std::fmin(sfxLoudMin, loud);
      sfxPhoneMin = std::fmin(sfxPhoneMin, phone);
    }
    writeWav(dir / (std::string("sfx_") + kSfxName[s] + ".wav"), x);
  }

  // rms = the bed level; 200ms = its loudest moment (usually a drum hit); phone = bed level through the phone filter
  std::printf("\n%-8s %7s %8s %7s %7s %6s %8s %8s %9s %8s %6s %9s %9s\n", "MUSIC", "peak", "peak dB", "rms dB", "silent", "clip",
              "onset/m", "pchg/m", "loopMatch", "chroma", "xRT", "200ms dB", "phone dB");
  float musPeakMax = 0, musBedMax = 0, musLoudMax = 0, musPhoneMax = 0;
  for (int m = 0; m < (int)Music::COUNT; m++) {
    auto a = std::make_unique<Audio>();
    a->setMusic((Music)m);
    auto t0 = std::chrono::steady_clock::now();
    auto x = renderFor(*a, 40.0f);
    double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    Levels L = levels(x, 3 * SR);   // skip the 2 s fade-in
    if ((Music)m == Music::Silence) {
      std::printf("%-8s %7.4f  (must be silent: below -120 dBFS)\n", kMusicName[m], L.peak);
      if (L.peak > 1e-6f) { std::printf("  ^ PROBLEM\n"); problems++; }
      continue;
    }
    Structure S = structure(x, 3.0f);
    const Loud ML = loudness(x, 3 * SR), MP = loudness(phoneSpeaker(x), 3 * SR);
    float gap = longestSilence(x, 3.0f);
    std::printf("%-8s %7.3f %8.1f %7.1f %6.1fs %6d %8.0f %8.0f %5.2f@%4.1fs %8.2f %5.0fx %9.1f %9.1f\n", kMusicName[m], L.peak, db(L.peak),
                db(L.rms), gap, L.clipped, S.onsetsPerMin, S.pitchChangesPerMin, S.bestLoopMatch, S.bestLoopLag, S.chromaSim,
                40.0 / secs, db(ML.maxWin), db(MP.mean));
    musPeakMax = std::fmax(musPeakMax, L.peak);
    musBedMax = std::fmax(musBedMax, ML.mean);
    musLoudMax = std::fmax(musLoudMax, ML.maxWin);
    musPhoneMax = std::fmax(musPhoneMax, MP.mean);
    if (L.clipped || gap > 4.0f || L.peak > 0.5f) { std::printf("  ^ PROBLEM\n"); problems++; }
    writeWav(dir / (std::string("music_") + kMusicName[m] + ".wav"), x);
  }

  // Every gameplay sfx (UI blips, talk and footsteps excluded: soft by design) must stand >= 6 dB above the loudest
  // music bed, full-range and on a phone speaker, and peak above any music peak.
  std::printf("\nloudest music: peak %.3f, bed %.1f dB, loudest 200ms %.1f dB, phone bed %.1f dB\n"
              "quietest gameplay sfx: peak %.3f, loudest 200ms %.1f dB, phone %.1f dB  ->  margin %.1f dB, phone margin %.1f dB\n",
              musPeakMax, db(musBedMax), db(musLoudMax), db(musPhoneMax), sfxPeakMin, db(sfxLoudMin), db(sfxPhoneMin),
              db(sfxLoudMin) - db(musBedMax), db(sfxPhoneMin) - db(musPhoneMax));
  if (sfxPeakMin < musPeakMax || db(sfxLoudMin) - db(musBedMax) < 6 || db(sfxPhoneMin) - db(musPhoneMax) < 6) {
    std::printf("PROBLEM: an sfx does not stand clearly above the music\n");
    problems++;
  }

  {   // music-only transitions, including a third piece requested mid-fade; clicks would show up as sample jumps
      // right after a switch that are far larger than anything the steady music produces
    auto a = std::make_unique<Audio>();
    std::vector<float> all;
    std::vector<size_t> switches;
    struct Leg { Music m; float sec; };
    const Leg legs[] = {{Music::Wild, 8}, {Music::Wild, 2}, {Music::Combat, 1}, {Music::Town, 0.5f}, {Music::Boss, 6},
                        {Music::Silence, 4}};
    Music prev = Music::Silence;
    for (const Leg& l : legs) {
      a->setMusic(l.m);   // the second Wild is a same-mode call and must change nothing
      if (l.m != prev) switches.push_back(all.size());
      prev = l.m;
      auto part = renderFor(*a, l.sec);
      all.insert(all.end(), part.begin(), part.end());
    }
    float nearSwitch = 0, elsewhere = 0;
    for (size_t i = 1; i < all.size(); i++) {
      float d = std::fabs(all[i] - all[i - 1]);
      bool near = false;
      for (size_t sw : switches) near |= i >= sw && i < sw + SR / 4;
      (near ? nearSwitch : elsewhere) = std::fmax(near ? nearSwitch : elsewhere, d);
    }
    float tailPeak = 0;
    for (size_t i = all.size() - SR; i < all.size(); i++) tailPeak = std::fmax(tailPeak, std::fabs(all[i]));
    std::printf("\ntransitions (Wild, Wild again, Combat, Town mid-fade, Boss, Silence): peak %.3f, largest sample jump within "
                "250 ms after a switch %.4f vs elsewhere %.4f, longest gap before Silence %.1fs, last second peak %.5f\n",
                levels(all).peak, nearSwitch, elsewhere,
                longestSilence(std::vector<float>(all.begin(), all.end() - 4 * SR), 0.5f), tailPeak);
    if (nearSwitch > 1.5f * elsewhere || tailPeak > 1e-3f) { std::printf("PROBLEM: transition artefact\n"); problems++; }
    writeWav(dir / "test_transitions.wav", all);
  }
  {   // two visits to the same mode must differ (fresh seed per start)
    auto a = std::make_unique<Audio>();
    a->setMusic(Music::Town);
    auto p = renderFor(*a, 14.0f);
    a->setMusic(Music::Silence);
    renderFor(*a, 4.0f);
    a->setMusic(Music::Town);
    auto q = renderFor(*a, 14.0f);
    double dot = 0, ep = 0, eq = 0;
    for (size_t i = 4 * SR; i < p.size(); i++) { dot += (double)p[i] * q[i]; ep += (double)p[i] * p[i]; eq += (double)q[i] * q[i]; }
    std::printf("re-entering Town: waveform correlation with the first visit %.3f (1.0 would be a replay)\n",
                dot / std::sqrt(ep * eq + 1e-20));
  }
  {   // music + sfx together: the hit must stand clearly above the score
    auto a = std::make_unique<Audio>();
    a->setMusic(Music::Combat);
    auto bed = renderFor(*a, 6.0f);
    a->play(Sfx::Hit);
    auto hit = renderFor(*a, 0.15f);
    Levels B = levels(bed, 3 * SR), H = levels(hit);
    std::printf("Hit over Combat: hit-window peak %.3f vs bed peak %.3f, rms %.1f dB vs %.1f dB\n", H.peak, B.peak, db(H.rms), db(B.rms));
  }
  problems += culturePieces(dir);
  problems += tavernPieces(dir);
  problems += wildlands(dir);
  std::printf("\n%s (%d problem%s)\n", problems ? "CHECK FAILED" : "all checks passed", problems, problems == 1 ? "" : "s");
  return problems ? 2 : 0;
}
