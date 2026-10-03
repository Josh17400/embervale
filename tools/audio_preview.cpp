// audio_preview: renders every Sfx and 40 s of every Music mode offline (no audio device) to 16-bit WAVs and
// prints level / spectral / musical-structure statistics so the synth can be checked without listening.
//
//   audio_preview [outDir]      default outDir: %LOCALAPPDATA%\Temp\claude\embervale_audio
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
};
const char* kMusicName[(int)Music::COUNT] = {"Silence", "Title", "Wild", "Night", "Town", "Cave", "Combat", "Boss"};

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
}  // namespace

int main(int argc, char** argv) {
  fs::path dir;
  if (argc > 1) dir = argv[1];
  else {
    const char* la = std::getenv("LOCALAPPDATA");
    dir = fs::path(la ? la : ".") / "Temp" / "claude" / "embervale_audio";
  }
  std::error_code ec;
  fs::create_directories(dir, ec);
  if (ec) { std::fprintf(stderr, "cannot create %s: %s\n", dir.string().c_str(), ec.message().c_str()); return 1; }
  std::printf("output: %s\n(levels at default master 0.6, music volume 0.7)\n\n", dir.string().c_str());
  int problems = 0;

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
        (Sfx)s != Sfx::MenuBack) {
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
  std::printf("\n%s (%d problem%s)\n", problems ? "CHECK FAILED" : "all checks passed", problems, problems == 1 ? "" : "s");
  return problems ? 2 : 0;
}
