// M6b "Sagas": the voice (rpg/story/saga.h phrase / voiceKeys). VOICE lane.
//
// Phrase grammars per culture voice family (0 plain, 1 fjord/highland, 2 heartland/imperial, 3 dune/sun-temple, 4 steppe,
// 5 marsh/river, 6 jade, 7 sylvan/starspire: rumours.cpp voiceOf) x speaker motive (fear .. hope) x key. The lines live
// in voice_social.cpp and voice_wisdom.cpp as small sub-grammars, one block per key:
//
//   = <key>                      a key (the 24 of saga.h, plus the VOICE lane's additions)
//   F<v> <line>                  a whole line in voice v (0..7)
//   A<v> <opener>                a short opener in voice v ("HAIL, ROAD-WALKER.", "O GUEST, HEAR ME.")
//   B<v> <line>                  a line in voice v that stands alone OR follows one of the voice's openers
//   M<motive> <line>             a line coloured by the speaker's motive; it always follows one of the voice's openers,
//                                so the people's idiom frames what the motive says
//
// The phrases of (key, voice, motive) are: every F line, every B line, every opener + B line, every opener + M line of
// that motive (combinations over 60 characters are dropped). The seed picks one. A phrase's FAMILY (the repetition
// guard counts them) is its template: an F line, a B line (alone or after any opener), or an M line (after any opener).
// Families are hashes of the key, the voice and the line's place in the table: stable while the table keeps its order.
//
// Rules for the lines (rpg_test --voice and --sagalib check them): upper case, at most 60 characters, only the
// placeholders {PLAYER} {GIVER} {GIVER.GOD} {GIVER.PEOPLE} {HOME}; no quotes or '#'; every line fits its key's meaning in
// ANY story (no claim about who died, what was stolen, where the road goes). Curses and insults are aimed at "THEM" or at
// the listener; threats at the listener.
#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include <vector>
#include "rpg/story/saga.h"

namespace story {
namespace saga {

// (voice_social.cpp, voice_wisdom.cpp) the key blocks, as raw text
namespace voicedata {
void socialBlocks(std::vector<const char*>& out);
void wisdomBlocks(std::vector<const char*>& out);
}  // namespace voicedata

namespace {

constexpr int VOICES = 8;

uint32_t fnv(const std::string& s) {
  uint32_t h = 2166136261u;
  for (char c : s) { h ^= (uint8_t)c; h *= 16777619u; }
  return h;
}

uint64_t mix(uint64_t z) {
  z += 0x9E3779B97F4A7C15ull;
  z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
  z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
  return z ^ (z >> 31);
}

struct Line { std::string text; uint32_t family = 0; };
struct KeyTable {
  std::string key;
  std::vector<Line> F[VOICES], A[VOICES], B[VOICES];
  std::vector<Line> M[(int)Motive::COUNT];
  // built: the phrases of each (voice, motive)
  std::vector<Line> pool[VOICES][(int)Motive::COUNT];
};

struct Tables {
  std::vector<KeyTable> keys;
  std::map<std::string, size_t> index;
  std::vector<std::string> names;
  std::vector<std::string> errors;
};

std::string trim(const std::string& s) {
  size_t a = 0, b = s.size();
  while (a < b && (s[a] == ' ' || s[a] == '\t' || s[a] == '\r')) a++;
  while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r')) b--;
  return s.substr(a, b - a);
}

void parseBlock(const char* text, Tables& T) {
  KeyTable* cur = nullptr;
  const std::string s = text ? text : "";
  size_t p = 0;
  while (p <= s.size()) {
    size_t e = s.find('\n', p);
    if (e == std::string::npos) e = s.size();
    const std::string l = trim(s.substr(p, e - p));
    p = e + 1;
    if (l.empty() || l[0] == '/') continue;
    if (l[0] == '=') {
      const std::string k = trim(l.substr(1));
      auto it = T.index.find(k);
      if (it == T.index.end()) {
        T.index[k] = T.keys.size();
        T.keys.push_back(KeyTable());
        T.keys.back().key = k;
        T.names.push_back(k);
        cur = &T.keys.back();
      } else {
        cur = &T.keys[it->second];
      }
      continue;
    }
    if (!cur) { T.errors.push_back("a line before any key: " + l); continue; }
    const size_t sp = l.find(' ');
    if (sp == std::string::npos) { T.errors.push_back("no text: " + l); continue; }
    const std::string tag = l.substr(0, sp);
    Line ln;
    ln.text = trim(l.substr(sp + 1));
    const char kind = tag[0];
    if (kind == 'M') {
      const int m = motiveWord(tag.substr(1));
      if (m < 0) { T.errors.push_back("unknown motive: " + l); continue; }
      std::vector<Line>& v = cur->M[m];
      ln.family = fnv(cur->key + "|M|" + tag.substr(1) + "|" + std::to_string(v.size())) | 1u;
      v.push_back(ln);
      continue;
    }
    if (tag.size() != 2 || tag[1] < '0' || tag[1] > '7' || (kind != 'F' && kind != 'A' && kind != 'B')) {
      T.errors.push_back("bad tag: " + l);
      continue;
    }
    const int v = tag[1] - '0';
    std::vector<Line>& dst = kind == 'F' ? cur->F[v] : kind == 'A' ? cur->A[v] : cur->B[v];
    ln.family = fnv(cur->key + "|" + kind + std::to_string(v) + "|" + std::to_string(dst.size())) | 1u;
    dst.push_back(ln);
  }
}

// the opener and the line, joined with one space; "" when too long
std::string join(const std::string& a, const std::string& b) {
  std::string s = a + " " + b;
  return s.size() <= 60 ? s : std::string();
}

const Tables& tables() {
  static const Tables T = [] {
    Tables t;
    std::vector<const char*> blocks;
    voicedata::socialBlocks(blocks);
    voicedata::wisdomBlocks(blocks);
    for (const char* b : blocks) parseBlock(b, t);
    for (KeyTable& k : t.keys)
      for (int v = 0; v < VOICES; v++)
        for (int m = 0; m < (int)Motive::COUNT; m++) {
          std::vector<Line>& P = k.pool[v][m];
          for (const Line& f : k.F[v]) P.push_back(f);
          for (const Line& b : k.B[v]) P.push_back(b);
          for (const Line& a : k.A[v])
            for (const Line& b : k.B[v]) {
              Line c;
              c.text = join(a.text, b.text);
              c.family = b.family;
              if (!c.text.empty()) P.push_back(c);
            }
          for (const Line& a : k.A[v])
            for (const Line& mm : k.M[m]) {
              Line c;
              c.text = join(a.text, mm.text);
              c.family = mm.family;
              if (!c.text.empty()) P.push_back(c);
            }
        }
    return t;
  }();
  return T;
}

}  // namespace

Phrase phrase(const std::string& key, const VoiceCtx& v) {
  Phrase p;
  const Tables& T = tables();
  auto it = T.index.find(key);
  if (it == T.index.end()) return p;
  const KeyTable& k = T.keys[it->second];
  const int vo = v.voice < 0 || v.voice >= VOICES ? 2 : v.voice;
  const int mo = v.motive < Motive::COUNT ? (int)v.motive : (int)Motive::Duty;
  const std::vector<Line>& P = k.pool[vo][mo];
  if (P.empty()) return p;
  const uint64_t r = mix(v.seed ^ 0x5EEDF00Dull);
  const Line& l = P[(size_t)(r % P.size())];
  p.text = l.text;
  p.family = l.family;
  return p;
}

const std::vector<std::string>& voiceKeys() { return tables().names; }

// (rpg_test --voice) the phrases a (key, voice, motive) can give, and problems found while reading the tables
std::vector<std::string> voicePool(const std::string& key, int voice, int motive) {
  std::vector<std::string> out;
  const Tables& T = tables();
  auto it = T.index.find(key);
  if (it == T.index.end() || voice < 0 || voice >= VOICES || motive < 0 || motive >= (int)Motive::COUNT) return out;
  for (const Line& l : T.keys[it->second].pool[voice][motive]) out.push_back(l.text);
  return out;
}
const std::vector<std::string>& voiceTableErrors() { return tables().errors; }
// (rpg_test --voice) the table's own lines of one kind ('F', 'A', 'B': voice `voice`; 'M': motive `voice`)
std::vector<std::string> voiceLines(const std::string& key, int voice, char kind) {
  std::vector<std::string> out;
  const Tables& T = tables();
  auto it = T.index.find(key);
  if (it == T.index.end()) return out;
  const KeyTable& k = T.keys[it->second];
  const std::vector<Line>* v = nullptr;
  if (kind == 'M') { if (voice >= 0 && voice < (int)Motive::COUNT) v = &k.M[voice]; }
  else if (voice >= 0 && voice < VOICES) v = kind == 'F' ? &k.F[voice] : kind == 'A' ? &k.A[voice] : &k.B[voice];
  if (v) for (const Line& l : *v) out.push_back(l.text);
  return out;
}

}  // namespace saga
}  // namespace story
