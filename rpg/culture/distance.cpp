// The culture distance (VISION_PLAN 5.4; M3, CULTURE lane): d(A, B) in Q16 (0 .. 65536 = 1.0), INTEGER maths only,
// because it drives structural choices (the four-phase maximin decides which culture lives where; the same answer must
// come out on MSVC, clang and Emscripten). No libm anywhere in here.
//
//   palette        0.20  mean OKLab dE of the 5 key colours after optimal matching (120 permutations) x 2 (= dE*100/50), capped
//   roof form      0.15  0/1          names      0.10  Jensen-Shannon divergence of character bigrams, 200 sample names
//   wall material  0.08  0/1          layout     0.06  0/1          roof material 0.05  0/1      clothing cut 0.05 0/1
//   helm form      0.05  0/1          headwear   0.04  0/1          blade 0.04 0/1     music scale 0.04 0/1
//   ornament       0.04  Jaccard      shield     0.03  0/1          lead instrument 0.03 0/1    meter 0.02 0/1   religion 0.02 0/1
//
// The 5 key colours: the roof, the wall, the accent (doors, bands, awnings), and the two commonest cloths.
// Headline components (5.4): palette (>= 0.25), roof form, wall material, names (JS >= 0.25), music scale.
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>
#include "rpg/culture/culture.h"

namespace cult {
namespace detail {

// ------------------------------------------------------------------ OKLab in Q16, integer only
namespace {
// sRGB 0..255 -> linear light, Q16 (precomputed offline from the sRGB transfer function)
const int32_t kLin[256] = {
    0, 20, 40, 60, 80, 99, 119, 139, 159, 179, 199, 219, 241, 264, 288, 313, 340, 367, 396, 427, 458, 491, 526, 562, 599, 637,
    677, 718, 761, 805, 851, 898, 947, 997, 1048, 1101, 1156, 1212, 1270, 1330, 1391, 1453, 1517, 1583, 1651, 1720, 1791, 1863,
    1937, 2013, 2090, 2170, 2250, 2333, 2418, 2504, 2592, 2681, 2773, 2866, 2961, 3058, 3157, 3258, 3360, 3464, 3570, 3678, 3788,
    3900, 4014, 4129, 4247, 4366, 4488, 4611, 4736, 4864, 4993, 5124, 5257, 5392, 5530, 5669, 5810, 5953, 6099, 6246, 6395, 6547,
    6701, 6856, 7014, 7174, 7336, 7500, 7666, 7834, 8004, 8177, 8352, 8529, 8708, 8889, 9072, 9258, 9446, 9636, 9828, 10022,
    10219, 10418, 10619, 10822, 11028, 11236, 11446, 11658, 11873, 12090, 12309, 12531, 12754, 12981, 13209, 13440, 13673, 13909,
    14147, 14387, 14629, 14874, 15122, 15372, 15624, 15878, 16135, 16394, 16656, 16920, 17187, 17456, 17727, 18001, 18278, 18556,
    18838, 19121, 19408, 19696, 19988, 20281, 20578, 20876, 21178, 21481, 21788, 22096, 22408, 22722, 23038, 23357, 23679, 24003,
    24329, 24659, 24991, 25325, 25662, 26002, 26344, 26689, 27036, 27387, 27739, 28095, 28453, 28813, 29177, 29543, 29911, 30283,
    30657, 31033, 31413, 31795, 32180, 32567, 32957, 33350, 33746, 34144, 34545, 34949, 35355, 35765, 36177, 36591, 37009, 37429,
    37852, 38278, 38707, 39138, 39572, 40009, 40449, 40892, 41337, 41786, 42237, 42691, 43147, 43607, 44069, 44534, 45003, 45474,
    45947, 46424, 46904, 47386, 47871, 48360, 48851, 49345, 49842, 50342, 50844, 51350, 51859, 52370, 52884, 53402, 53922, 54445,
    54972, 55501, 56033, 56568, 57106, 57647, 58191, 58738, 59288, 59841, 60397, 60956, 61518, 62083, 62651, 63222, 63796, 64373,
    64953, 65536};

// cube root of a Q16 value (0 .. ~65536), Q16 result: floor(cbrt(v * 2^32))
int32_t cbrtQ16(int64_t v) {
  if (v <= 0) return 0;
  const uint64_t x = (uint64_t)v << 32;
  uint64_t lo = 0, hi = 1u << 17;
  while (lo < hi) {
    const uint64_t mid = (lo + hi + 1) >> 1;
    if (mid * mid * mid <= x) lo = mid; else hi = mid - 1;
  }
  return (int32_t)lo;
}
uint64_t isqrt64(uint64_t x) {
  uint64_t r = 0, bit = 1ull << 62;
  while (bit > x) bit >>= 2;
  while (bit) {
    if (x >= r + bit) { x -= r + bit; r = (r >> 1) + bit; } else { r >>= 1; }
    bit >>= 2;
  }
  return r;
}
struct Lab { int32_t L, a, b; };
Lab oklab(uint32_t rgba) {
  const int64_t r = kLin[rgba & 255], g = kLin[(rgba >> 8) & 255], b = kLin[(rgba >> 16) & 255];
  const int64_t l = (27015 * r + 35149 * g + 3372 * b) >> 16;
  const int64_t m = (13887 * r + 44610 * g + 7038 * b) >> 16;
  const int64_t s = (5787 * r + 18463 * g + 41286 * b) >> 16;
  const int64_t l_ = cbrtQ16(l), m_ = cbrtQ16(m), s_ = cbrtQ16(s);
  Lab o;
  o.L = (int32_t)((13792 * l_ + 52011 * m_ - 267 * s_) >> 16);
  o.a = (int32_t)((129630 * l_ - 159160 * m_ + 29530 * s_) >> 16);
  o.b = (int32_t)((1698 * l_ + 51300 * m_ - 52997 * s_) >> 16);
  return o;
}
int32_t dE(const Lab& x, const Lab& y) {
  const int64_t dl = x.L - y.L, da = x.a - y.a, db = x.b - y.b;
  return (int32_t)isqrt64((uint64_t)(dl * dl + da * da + db * db));
}

// the 120 permutations of 5, built once by an integer procedure (constant data: no state that can differ)
struct Perms {
  uint8_t p[120][5];
  Perms() {
    int n = 0;
    uint8_t a[5] = {0, 1, 2, 3, 4};
    // Heap's algorithm, iterative
    int cnt[5] = {};
    for (int k = 0; k < 5; k++) p[n][k] = a[k];
    n++;
    int i = 0;
    while (i < 5) {
      if (cnt[i] < i) {
        const int j = (i & 1) ? cnt[i] : 0;
        const uint8_t t = a[j]; a[j] = a[i]; a[i] = t;
        for (int k = 0; k < 5; k++) p[n][k] = a[k];
        n++;
        cnt[i]++;
        i = 0;
      } else {
        cnt[i] = 0;
        i++;
      }
    }
  }
};
const Perms kPerms;

void keyColours(const Culture& c, uint32_t out[5]) {
  out[0] = c.arch.roofTint ? c.arch.roofTint : 0xFF3A506Eu;
  out[1] = c.arch.wallTint ? c.arch.wallTint : 0xFFB8D2DEu;
  out[2] = c.arch.accentTint ? c.arch.accentTint : 0xFF28288Cu;
  out[3] = c.dress.cloth[0] ? c.dress.cloth[0] : 0xFF8C6E46u;
  out[4] = c.dress.cloth[2] ? c.dress.cloth[2] : 0xFF466E8Cu;
}

// ------------------------------------------------------------------ fixed-point log2 and the bigram divergence
// floor(log2(x) * 65536) for x >= 1 (exact bit-by-bit squaring)
int64_t log2Q16(uint64_t x) {
  if (x <= 1) return 0;
  int n = 63;
  while (!(x >> n)) n--;
  // y in [1, 2) as Q30
  uint64_t y = n >= 30 ? x >> (n - 30) : x << (30 - n);
  int64_t r = (int64_t)n << 16;
  for (int bit = 15; bit >= 0; bit--) {
    y = (y * y) >> 30;
    if (y >= (2ull << 30)) { y >>= 1; r |= (int64_t)1 << bit; }
  }
  return r;
}
// sum over bins of c * log2(c) (Q16 bits)
int64_t sumClogC(const std::vector<std::pair<uint16_t, uint32_t>>& bins, uint64_t scale) {
  int64_t s = 0;
  for (const auto& b : bins) {
    const uint64_t c = (uint64_t)b.second * scale;
    s += (int64_t)c * log2Q16(c);
  }
  return s;
}
}  // namespace

constexpr int kSym = 28;   // boundary, A..Z, other
struct NameHist {
  std::vector<std::pair<uint16_t, uint32_t>> bins;   // (bigram index, count), sorted by index
  uint32_t total = 0;
};

void sampleBigrams(const Culture& c, uint32_t* counts);   // names.cpp

NameHist nameHist(const Culture& c) {
  uint32_t cnt[kSym * kSym] = {};
  sampleBigrams(c, cnt);
  NameHist h;
  h.bins.reserve(160);
  for (int i = 0; i < kSym * kSym; i++)
    if (cnt[i]) { h.bins.push_back({(uint16_t)i, cnt[i]}); h.total += cnt[i]; }
  return h;
}

// Jensen-Shannon divergence (base 2, so 0..1) in Q16
int32_t namesQ(const NameHist& P, const NameHist& Q) {
  if (!P.total || !Q.total) return 0;
  const uint64_t Np = P.total, Nq = Q.total, T = 2 * Np * Nq;
  // mixture counts m_i = p_i * Nq + q_i * Np (total T)
  std::vector<std::pair<uint16_t, uint32_t>> M;
  int64_t sumM = 0;
  size_t i = 0, j = 0;
  while (i < P.bins.size() || j < Q.bins.size()) {
    uint64_t m;
    if (j >= Q.bins.size() || (i < P.bins.size() && P.bins[i].first < Q.bins[j].first)) { m = P.bins[i].second * Nq; i++; }
    else if (i >= P.bins.size() || Q.bins[j].first < P.bins[i].first) { m = Q.bins[j].second * Np; j++; }
    else { m = P.bins[i].second * Nq + Q.bins[j].second * Np; i++; j++; }
    sumM += (int64_t)m * log2Q16(m);
  }
  // H(X) = log2 N - (1/N) sum c log2 c  (Q16)
  const int64_t HM = log2Q16(T) - sumM / (int64_t)T;
  const int64_t HP = log2Q16(Np) - sumClogC(P.bins, 1) / (int64_t)Np;
  const int64_t HQ = log2Q16(Nq) - sumClogC(Q.bins, 1) / (int64_t)Nq;
  int64_t js = HM - (HP + HQ) / 2;
  if (js < 0) js = 0;
  if (js > 65536) js = 65536;
  return (int32_t)js;
}

// a culture's 5 key colours in OKLab (computed once per culture by the maximin; on the fly by the public metric)
struct PalKey { int32_t L[5], a[5], b[5]; };
PalKey palKey(const Culture& c) {
  uint32_t col[5];
  keyColours(c, col);
  PalKey k;
  for (int i = 0; i < 5; i++) { const Lab l = oklab(col[i]); k.L[i] = l.L; k.a[i] = l.a; k.b[i] = l.b; }
  return k;
}
int32_t paletteQ(const PalKey& x, const PalKey& y) {
  int32_t d[5][5];
  for (int i = 0; i < 5; i++)
    for (int j = 0; j < 5; j++) {
      const int64_t dl = x.L[i] - y.L[j], da = x.a[i] - y.a[j], db = x.b[i] - y.b[j];
      d[i][j] = (int32_t)isqrt64((uint64_t)(dl * dl + da * da + db * db));
    }
  int64_t best = INT64_MAX;
  for (int k = 0; k < 120; k++) {
    int64_t s = 0;
    for (int i = 0; i < 5; i++) s += d[i][kPerms.p[k][i]];
    if (s < best) best = s;
  }
  // mean dE (OKLab, Q16) * 2  ==  (dE * 100) / 50
  const int64_t q = best * 2 / 5;
  return (int32_t)(q > 65536 ? 65536 : q);
}
int32_t paletteQ(const Culture& a, const Culture& b) { return paletteQ(palKey(a), palKey(b)); }

namespace {
int32_t jaccardQ(uint32_t x, uint32_t y) {
  auto pop = [](uint32_t v) { int n = 0; while (v) { v &= v - 1; n++; } return n; };
  const int u = pop(x | y);
  if (!u) return 0;
  return (int32_t)(65536 - (int64_t)pop(x & y) * 65536 / u);
}
}  // namespace

// everything but the names (the maximin bounds its candidates with it), and the cheap headline count (palette, roof,
// wall, scale), from one palette comparison
void compareCheap(const Culture& a, const PalKey& ka, const Culture& b, const PalKey& kb, int32_t& dOut, int& headOut) {
  const int32_t pal = paletteQ(ka, kb);
  int64_t d = 0;
  d += (int64_t)pal * 13107;
  d += (int64_t)(a.arch.roof != b.arch.roof) * 65536 * 9830;
  d += (int64_t)(a.arch.wall != b.arch.wall) * 65536 * 5243;
  d += (int64_t)(a.town.layout != b.town.layout) * 65536 * 3932;
  d += (int64_t)(a.arch.roofMat != b.arch.roofMat) * 65536 * 3277;
  d += (int64_t)(a.dress.cutM != b.dress.cutM || a.dress.cutF != b.dress.cutF) * 65536 * 3277;
  d += (int64_t)(a.arms.helm[0] != b.arms.helm[0]) * 65536 * 3277;
  d += (int64_t)(a.dress.head[0] != b.dress.head[0]) * 65536 * 2621;
  d += (int64_t)(a.arms.blade != b.arms.blade) * 65536 * 2621;
  d += (int64_t)(a.music.scale != b.music.scale) * 65536 * 2621;
  d += (int64_t)jaccardQ(a.arch.ornament, b.arch.ornament) * 2622;
  d += (int64_t)(a.arms.shield != b.arms.shield) * 65536 * 1966;
  d += (int64_t)(a.music.lead != b.music.lead) * 65536 * 1966;
  d += (int64_t)(a.music.meter != b.music.meter) * 65536 * 1311;
  d += (int64_t)(a.faith.kind != b.faith.kind) * 65536 * 1311;
  dOut = (int32_t)(d >> 16);
  headOut = (int)(pal >= 16384) + (int)(a.arch.roof != b.arch.roof) + (int)(a.arch.wall != b.arch.wall) + (int)(a.music.scale != b.music.scale);
}
// the full distance and headline count: the cheap part plus the names (so cheap + 0.10 and + 1 bound them)
void compareFull(const Culture& a, const PalKey& ka, const NameHist& ha, const Culture& b, const PalKey& kb, const NameHist& hb,
                 int32_t& dOut, int& headOut) {
  compareCheap(a, ka, b, kb, dOut, headOut);
  const int32_t nm = namesQ(ha, hb);
  const int64_t q = (int64_t)dOut + (((int64_t)nm * 6554) >> 16);
  dOut = (int32_t)(q > 65536 ? 65536 : q);
  headOut += (int)(nm >= 16384);
}

// The public metric has no atlas to hold the name histograms: a small per-thread memo keyed by the phonology and
// seed (a pure cache: the answer is the same with or without it).
namespace {
uint64_t phonKey(const Culture& c) {
  uint64_t k = 0xCBF29CE484222325ull ^ c.seed;
  auto mx = [&](const std::string& s) { for (char ch : s) { k ^= (uint8_t)ch; k *= 0x100000001B3ull; } k ^= 0xFF; k *= 0x100000001B3ull; };
  for (const auto& s : c.phon.onsets) mx(s);
  for (const auto& s : c.phon.nuclei) mx(s);
  for (const auto& s : c.phon.codas) mx(s);
  for (const auto& s : c.phon.placeSuffix) mx(s);
  for (const auto& s : c.phon.personSuffixF) mx(s);
  for (const auto& s : c.phon.personSuffixM) mx(s);
  mx(c.phon.forbid);
  k ^= (uint64_t)c.phon.sylMin << 8 | (uint64_t)c.phon.sylMax << 16 | (uint64_t)c.phon.pattern << 24;
  return k;
}
const NameHist& histOf(const Culture& c) {
  thread_local std::unordered_map<uint64_t, NameHist> memo;
  const uint64_t k = phonKey(c);
  auto it = memo.find(k);
  if (it != memo.end()) return it->second;
  if (memo.size() > 8192) memo.clear();
  return memo.emplace(k, nameHist(c)).first->second;
}
}  // namespace

}  // namespace detail

int32_t distanceQ(const Culture& a, const Culture& b) {
  const detail::NameHist ha = detail::histOf(a);   // a copy: the second lookup may rehash the memo
  const detail::NameHist& hb = detail::histOf(b);
  int32_t d;
  int h;
  detail::compareFull(a, detail::palKey(a), ha, b, detail::palKey(b), hb, d, h);
  return d;
}
int headlineDiffs(const Culture& a, const Culture& b) {
  const detail::NameHist ha = detail::histOf(a);
  const detail::NameHist& hb = detail::histOf(b);
  int32_t d;
  int h;
  detail::compareFull(a, detail::palKey(a), ha, b, detail::palKey(b), hb, d, h);
  return h;
}

}  // namespace cult
