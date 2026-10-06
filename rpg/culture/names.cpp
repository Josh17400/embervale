// Names in a culture's own phonology (VISION_PLAN 5.2 Phonology, M3; CULTURE lane): places, people, kingdoms,
// landmarks and dungeons. Deterministic in (culture phonology, seed); upper case ASCII A-Z plus space, apostrophe and
// hyphen; short enough for the HUD (places and kingdoms <= 12 letters, people <= 10).
//
// Pronounceable by construction: syllables are onset + nucleus (+ coda) drawn by weight (earlier = commoner), with at
// most three consonants or three vowels in a row, no letter tripled and the culture's banned pairs avoided. Every
// name is also screened against real-world fantasy names and profanity (rpg_test --cultures checks the same lists).
#include <cstdint>
#include <string>
#include <vector>
#include "rpg/culture/culture.h"

namespace cult {
namespace detail {
uint64_t smix(uint64_t z);   // priors.cpp

namespace {
struct Pk {
  uint64_t s;
  explicit Pk(uint64_t seed) : s(seed ? seed : 0x51ull) {}
  uint32_t next() { s = smix(s); return (uint32_t)(s >> 32); }
  int pick(int n) { return n <= 1 ? 0 : (int)(next() % (uint32_t)n); }
  int tri(int n) {   // favours early entries (the list is weighted by order)
    const int a = pick(n), b = pick(n);
    return a < b ? a : b;
  }
  bool chance(int p256) { return (int)(next() & 255) < p256; }
};

bool vowel(char c) { return c == 'A' || c == 'E' || c == 'I' || c == 'O' || c == 'U' || c == 'Y'; }

// real-world fantasy and game names, and profanity: a name containing one is drawn again
const char* const kBannedIP[] = {"SKYRIM", "TAMRIEL", "WESTEROS", "GONDOR", "MORDOR", "ROHAN", "NARNIA", "HOGWARTS", "AZEROTH",
                                 "HYRULE", "RIVENDELL", "ISENGARD", "LOTHLORIEN", "WINTERFELL", "ESSOS", "MORROWIND",
                                 "OBLIVION", "ERIADOR", "NUMENOR", "VALINOR", "FAERUN", "KRYNN", "MIDGARD", "ASGARD",
                                 "PANDORA", "ANDOR", "THEDAS", "ZELDA", "SAURON", "GANDALF", "DUMBLEDORE"};
const char* const kBannedRude[] = {"FUCK", "SHIT", "CUNT", "COCK", "DICK", "PISS", "TWAT", "SLUT", "WHORE", "FAG", "NIGG",
                                   "NIGA", "RAPE", "PENIS", "VAGIN", "TITS", "BITCH", "ANUS", "PORN", "SEX", "POOP", "CRAP",
                                   "KKK", "NAZI", "HITLER", "JIZZ", "WANK", "BOLLOCK", "ARSE", "TURD", "SPERM", "COON",
                                   "SPIC", "KIKE", "GOOK", "CHINK", "DAMN", "HELL", "PUSS", "CUM", "BUTT", "BOOB", "DILDO",
                                   "HOMO", "LESBO", "RETARD", "SHAG", "TOSS", "PRICK", "SCROT", "KILL", "DIE", "GAY", "NEGR",
                                   "SATAN"};
}  // namespace

namespace {
// the first two letters of every banned word: a name with none of these bigrams needs no substring search
struct BanBigrams {
  bool hit[26][26] = {};
  BanBigrams() {
    for (const char* b : kBannedIP) hit[b[0] - 'A'][b[1] - 'A'] = true;
    for (const char* b : kBannedRude) hit[b[0] - 'A'][b[1] - 'A'] = true;
  }
};
const BanBigrams kBan;
}  // namespace

bool nameBanned(const std::string& s) {
  bool maybe = false;
  for (size_t i = 0; i + 1 < s.size() && !maybe; i++) {
    const char a = s[i], b = s[i + 1];
    if (a >= 'A' && a <= 'Z' && b >= 'A' && b <= 'Z') maybe = kBan.hit[a - 'A'][b - 'A'];
  }
  if (maybe) {
    for (const char* b : kBannedIP)
      if (s.find(b) != std::string::npos) return true;
    for (const char* b : kBannedRude)
      if (s.find(b) != std::string::npos) return true;
  }
  // whole words only for the short ones
  std::string w;
  for (size_t i = 0; i <= s.size(); i++) {
    if (i == s.size() || s[i] == ' ' || s[i] == '-' || s[i] == '\'') {
      // whole words: the short rude ones, and everyday English words that read as a joke on a map ("KINGDOM OF TUBE")
      static const char* const kWords[] = {"ASS", "TIT", "FAT", "POO", "WEE", "PEE", "TUBE", "TAPE", "CUBE", "BABE", "BABY",
                                           "MOOD", "NOOB", "FOOD", "GOOD", "BAD", "SAD", "MAD", "POT", "POOL", "LOO", "MOM",
                                           "DAD", "BUM", "PUKE", "BARF", "SNOT", "BOOB", "DUMB", "DUMMY", "NOOSE", "MOO",
                                           "BOO", "LOL", "OMG", "WTF", "TOILET", "TOOT", "FART", "BURP", "MUMMY", "DADDY",
                                           "SOAP", "SOUP", "TACO", "PIZZA", "COLA", "BANANA", "MANGO", "TOFU", "SUSHI",
                                           "ROBOT", "MAMA", "PAPA", "HOBO", "BIMBO", "BOZO", "LOSER", "WIMP", "SISSY"};
      for (const char* k : kWords)
        if (w == k) return true;
      w.clear();
    } else {
      w += s[i];
    }
  }
  return false;
}

namespace {
// is s pronounceable and legal: A-Z (and the joiners) only, <= 3 consonants / vowels in a row, nothing tripled, no
// banned pair, no banned name
bool legal(const std::string& s, const Phonology& ph) {
  if (s.empty()) return false;
  int cons = 0, vow = 0;
  for (size_t i = 0; i < s.size(); i++) {
    const char ch = s[i];
    if (ch == ' ' || ch == '-' || ch == '\'') { cons = vow = 0; continue; }
    if (ch < 'A' || ch > 'Z') return false;
    if (vowel(ch)) { vow++; cons = 0; } else { cons++; vow = 0; }
    if (cons > 3 || vow > 2) return false;
    if (i >= 2 && s[i] == s[i - 1] && s[i] == s[i - 2]) return false;
  }
  for (size_t k = 0; k + 1 < ph.forbid.size(); k += 2) {
    const char pair[3] = {ph.forbid[k], ph.forbid[k + 1], 0};
    if (s.find(pair) != std::string::npos) return false;
  }
  return !nameBanned(s);
}

// one word of `syl` syllables
std::string word(const Phonology& ph, Pk& p, int syl) {
  std::string s;
  const int no = (int)ph.onsets.size(), nn = (int)ph.nuclei.size(), nc = (int)ph.codas.size();
  if (!nn) return "A";
  for (int i = 0; i < syl; i++) {
    const bool afterVowel = !s.empty() && vowel(s.back());
    // syllable type by the pattern bits: 1 CV, 2 CVC, 4 V, 8 VC
    int wt[4] = {(ph.pattern & 1) ? 5 : 0, (ph.pattern & 2) ? 3 : 0, (ph.pattern & 4) && !afterVowel ? 1 : 0,
                 (ph.pattern & 8) && !afterVowel ? 1 : 0};
    if (i == syl - 1 && (ph.pattern & 2)) wt[1] += 1;   // closed last syllables read as names
    int sum = wt[0] + wt[1] + wt[2] + wt[3];
    if (sum == 0) { wt[0] = 1; sum = 1; }
    int r = p.pick(sum), type = 0;
    while (type < 3 && r >= wt[type]) { r -= wt[type]; type++; }
    const bool onset = type <= 1, coda = type == 1 || type == 3;
    if (onset && no) {
      // an empty onset after a vowel would run two nuclei together ("CAAURA"): take a real consonant instead
      const std::string* o = &ph.onsets[(size_t)p.tri(no)];
      for (int t = 0; t < 4 && o->empty() && afterVowel; t++) o = &ph.onsets[(size_t)p.pick(no)];
      s += *o;
    }
    s += ph.nuclei[(size_t)p.tri(nn)];
    if (coda && nc) s += ph.codas[(size_t)p.tri(nc)];
  }
  return s;
}

// letters only (spaces, hyphens and apostrophes do not count toward the HUD budget much, but we cap the whole)
std::string fit(std::string s, size_t cap) {
  if (s.size() <= cap) return s;
  s.resize(cap);
  while (!s.empty() && (s.back() == ' ' || s.back() == '-' || s.back() == '\'')) s.pop_back();
  return s;
}

std::string withSuffix(const Phonology& ph, Pk& p, std::string base, size_t cap) {
  if (ph.placeSuffix.empty()) return base;
  const std::string& sf = ph.placeSuffix[(size_t)p.tri((int)ph.placeSuffix.size())];
  if (sf.empty()) return base;
  if (sf[0] == '^') {
    std::string pre = sf.substr(1);
    if (pre.size() + base.size() > cap) return base;
    return pre + base;
  }
  // avoid a doubled vowel or a clumsy consonant pile at the seam
  if (!base.empty() && vowel(base.back()) && vowel(sf[0])) base.pop_back();
  if (base.size() + sf.size() > cap) return base;
  return base + sf;
}

template <class F>
std::string screened(const Phonology& ph, uint64_t seed, size_t cap, F make) {
  for (int tries = 0; tries < 16; tries++) {
    Pk p(seed + (uint64_t)tries * 0x9E3779B97F4A7C15ull);
    std::string s = fit(make(p), cap);
    if (s.size() >= 3 && legal(s, ph)) return s;
  }
  // a plain safe fallback (never reached in practice; the tests count names, not fallbacks)
  return "ALDER";
}

uint64_t key(const Culture& c, uint32_t seed, uint32_t salt) { return smix(((uint64_t)c.seed << 32) ^ seed ^ ((uint64_t)salt << 20)); }
}  // namespace

// The distance metric's name sample (VISION_PLAN 5.4: bigrams over 200 sample names): 100 place-like and 100
// person-like words straight from the phonology (the same syllable machinery as the real names, without the
// screening, which changes too few names to move a distribution), counted into a bigram table without allocating.
// counts: kSym x kSym (28: boundary, A..Z, other), bigram (a, b) at a * 28 + b.
void sampleBigrams(const Culture& c, uint32_t* counts) {
  const Phonology& ph = c.phon;
  const int no = (int)ph.onsets.size(), nn = (int)ph.nuclei.size(), nc = (int)ph.codas.size();
  if (!nn) return;
  Pk p(smix(((uint64_t)c.seed << 20) ^ 0x8A3E5ull ^ (uint64_t)no << 50));
  auto sym = [](char ch) { return ch >= 'A' && ch <= 'Z' ? ch - 'A' + 1 : 27; };
  for (int n = 0; n < 200; n++) {
    char buf[64];
    int len = 0;
    auto put = [&](const std::string& s) { for (char ch : s) if (len < 60) buf[len++] = ch; };
    const bool person = n >= 100;
    const int syl = person ? ph.sylMin + p.pick(ph.sylMax - ph.sylMin + 1 > 2 ? 2 : ph.sylMax - ph.sylMin + 1)
                           : (ph.sylMin <= 1 ? 1 + p.pick(2) : 1 + p.pick(ph.sylMax > 2 ? 2 : ph.sylMax));
    for (int i = 0; i < (syl < 1 ? 1 : syl); i++) {
      const bool afterVowel = len > 0 && vowel(buf[len - 1]);
      int wt[4] = {(ph.pattern & 1) ? 5 : 0, (ph.pattern & 2) ? 3 : 0, (ph.pattern & 4) && !afterVowel ? 1 : 0,
                   (ph.pattern & 8) && !afterVowel ? 1 : 0};
      int sum = wt[0] + wt[1] + wt[2] + wt[3];
      if (sum == 0) { wt[0] = 1; sum = 1; }
      int r = p.pick(sum), type = 0;
      while (type < 3 && r >= wt[type]) { r -= wt[type]; type++; }
      if (type <= 1 && no) put(ph.onsets[(size_t)p.tri(no)]);
      put(ph.nuclei[(size_t)p.tri(nn)]);
      if ((type == 1 || type == 3) && nc) put(ph.codas[(size_t)p.tri(nc)]);
    }
    const std::vector<std::string>& suf = person ? ((n & 1) ? ph.personSuffixF : ph.personSuffixM) : ph.placeSuffix;
    if (!suf.empty() && (person || p.chance(150))) {
      const std::string& x = suf[(size_t)p.tri((int)suf.size())];
      if (!x.empty() && x[0] == '^') {
        char tmp[64];
        int tl = 0;
        for (size_t k = 1; k < x.size() && tl < 30; k++) tmp[tl++] = x[k];
        for (int k = 0; k < len && tl < 60; k++) tmp[tl++] = buf[k];
        len = tl;
        for (int k = 0; k < len; k++) buf[k] = tmp[k];
      } else {
        put(x);
      }
    }
    int prev = 0;
    for (int k = 0; k < len; k++) {
      const int s2 = sym(buf[k]);
      counts[prev * 28 + s2]++;
      prev = s2;
    }
    counts[prev * 28]++;
  }
}

std::string rootWord(const Culture& c, uint32_t seed, int maxLen) {
  const size_t cap = (size_t)(maxLen < 3 ? 3 : maxLen > 6 ? 6 : maxLen);
  return screened(c.phon, key(c, seed, 0x2007), cap, [&](Pk& p) {
    std::string s = word(c.phon, p, 1);
    if (s.size() < 3) s += word(c.phon, p, 1);
    bool cons = false;
    for (char ch : s) cons |= !vowel(ch);
    if (!cons)   // a root needs a consonant to stand on ("IIA" is no word)
      for (const std::string& o : c.phon.onsets)
        if (!o.empty()) { s = o + s; break; }
    return s;
  });
}

}  // namespace detail

using detail::Pk;

std::string placeName(const Culture& c, uint32_t seed) {
  const Phonology& ph = c.phon;
  return detail::screened(ph, detail::smix((uint64_t)seed * 0x2545F4914F6CDD1Dull ^ 0x51ACEull ^ ((uint64_t)c.phon.onsets.size() << 50) ^
                                           (uint64_t)detail::smix(c.seed)),
                          12, [&](Pk& p) {
    const int syl = ph.sylMin <= 1 ? 1 + p.pick(2) : 1 + p.pick(ph.sylMax > 2 ? 2 : ph.sylMax);
    std::string base = detail::word(ph, p, syl);
    if (base.size() < 3 || p.chance(150)) base = detail::withSuffix(ph, p, base, 12);
    return base;
  });
}

std::string personName(const Culture& c, uint32_t seed, bool female) {
  const Phonology& ph = c.phon;
  return detail::screened(ph, detail::key(c, seed, female ? 0xFE3A : 0x3A1E), 10, [&](Pk& p) {
    const int syl = ph.sylMin + p.pick(ph.sylMax - ph.sylMin + 1 > 2 ? 2 : ph.sylMax - ph.sylMin + 1);
    std::string s = detail::word(ph, p, syl < 1 ? 1 : syl);
    const std::vector<std::string>& suf = female ? ph.personSuffixF : ph.personSuffixM;
    if (!suf.empty()) {
      const std::string& x = suf[(size_t)p.tri((int)suf.size())];
      if (!x.empty()) {
        if (!s.empty() && detail::vowel(s.back()) && detail::vowel(x[0])) s.pop_back();
        if (s.size() + x.size() <= 10) s += x;
      } else if (female && !s.empty() && !detail::vowel(s.back()) && s.size() < 10) {
        s += 'A';
      }
    }
    return s;
  });
}

std::string kingdomName(const Culture& c, uint32_t seed) {
  const Phonology& ph = c.phon;
  return detail::screened(ph, detail::key(c, seed, 0x4B1D), 12, [&](Pk& p) {
    std::string s = detail::word(ph, p, 2 + (ph.sylMax >= 3 && p.chance(70) ? 1 : 0));
    if (p.chance(90)) s = detail::withSuffix(ph, p, s, 12);
    return s;
  });
}

std::string landmarkName(const Culture& c, uint32_t seed, int kind) {
  const Phonology& ph = c.phon;
  return detail::screened(ph, detail::key(c, seed, 0x1A4D + (uint32_t)kind * 977u), 10, [&](Pk& p) {
    return detail::word(ph, p, 2);
  });
}

// SiteType order (rpg/sim/world.h): City, Town, Village, Cave, Ruin, BanditCamp, Shrine, DragonLair, Vignette, Wonder
std::string dungeonName(const Culture& c, uint32_t seed, int siteType) {
  Pk p(detail::key(c, seed, 0xD0D + (uint32_t)siteType * 131u));
  const std::string root = detail::rootWord(c, p.next(), 7);
  switch (siteType) {
    case 3: {
      static const char* const n[] = {"CAVERN", "DEEP", "HOLLOW", "GROTTO", "DELVE", "WARREN"};
      return root + " " + n[p.pick(6)];
    }
    case 4: {
      static const char* const n[] = {"BARROW", "HALLS", "TOMB", "VAULT", "CRYPT", "SPIRE"};
      if (p.chance(80)) return "TOMB OF " + personName(c, p.next(), p.chance(100));
      return root + " " + n[p.pick(6)];
    }
    case 5: {
      static const char* const n[] = {"CAMP", "HIDEOUT", "LOOKOUT", "REDOUBT"};
      std::string who = personName(c, p.next(), p.chance(60));
      if (who.size() > 8) who.resize(8);
      return who + "'S " + n[p.pick(4)];
    }
    case 6: {
      if (!c.faith.names.empty()) return "SHRINE OF " + c.faith.names[(size_t)p.pick((int)c.faith.names.size())];
      return root + " SHRINE";
    }
    case 7: return root + " PEAK";
    default: return root + " STONES";
  }
}

}  // namespace cult
