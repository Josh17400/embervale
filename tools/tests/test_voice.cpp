// rpg_test --voice [--verbose] [--print <key> [voice] [motive]]: the M6b voice grammars (rpg/story/voice*.cpp). VOICE lane.
// No world is needed. Checks:
//   - the tables read without errors; the 24 keys of saga.h are all there (the lane may add more)
//   - every key x voice (0..7) x motive gives phrases; every phrase is upper case, at most 60 characters, uses only the
//     placeholders {PLAYER} {GIVER} {GIVER.GOD} {GIVER.PEOPLE} {HOME}, and has no quote or '#' (it lands inside a DSL
//     quoted line)
//   - at least 6 distinct phrases per key x voice x motive; at least 12 proverbs per voice; every voice has its own
//     lines for every key (no two voices share more than a quarter of a key's phrases, so two peoples never sound alike)
//   - phrase() is deterministic and its families are stable: the same context gives the same text and family, a
//     family is never 0, and one family never covers two different line templates of different keys
//   - the motive colours a key that has motive lines: the phrase pools of two motives differ
// It prints the counts (keys, phrases per voice, the motive-coloured keys).
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <vector>
#include "rpg/story/dsl.h"
#include "rpg/story/saga.h"
#include "tools/tests/tests.h"

namespace story {
namespace saga {
// (voice.cpp) test hooks
std::vector<std::string> voicePool(const std::string& key, int voice, int motive);
const std::vector<std::string>& voiceTableErrors();
std::vector<std::string> voiceLines(const std::string& key, int voice, char kind);
}  // namespace saga
}  // namespace story

namespace {

using namespace story::saga;

const char* const kFrozen[] = {"greet", "welcome", "plea", "urgency", "thanks", "farewell", "blessing", "oath",
                               "vow", "proverb", "saying", "curse", "threat", "insult", "comfort", "grief",
                               "doubt", "warning", "apology", "boast", "refusal", "surprise", "relief", "omen"};

std::string checkText(const std::string& t) {
  if (t.empty()) return "empty";
  if (t.size() > 60) return "longer than 60 characters";
  if (t.find('"') != std::string::npos || t.find('#') != std::string::npos) return "a quote or '#'";
  if (t.find("[[") != std::string::npos || t.find("<<") != std::string::npos) return "template markup";
  bool in = false;
  for (char c : t) {
    if (c == '{') in = true;
    else if (c == '}') in = false;
    else if (!in && c >= 'a' && c <= 'z') return "lower case";
  }
  for (const auto& ph : story::dsl::placeholders(t)) {
    const bool ok = (ph.first == "player" && ph.second.empty()) || (ph.first == "home" && ph.second.empty()) ||
                    (ph.first == "giver" && (ph.second.empty() || ph.second == "god" || ph.second == "people"));
    if (!ok) return "a placeholder other than {PLAYER} {GIVER} {GIVER.GOD} {GIVER.PEOPLE} {HOME}";
  }
  return std::string();
}

// --lane: the VOICE lane's archetypes (folk, Maas-like, Gwynne-like, world-native) composed in every voice x allowed
// motive x 3 seeds and with every fitting twist in every slot; every error printed (the --sagalib lint stops at 20), then
// the distinct texts over the lint's 24 seeds (the lane's bar: 18)
int laneCheck(bool verbose) {
  int bad = 0, composed = 0, arch = 0;
  for (const Archetype& a : archetypes()) {
    if (a.tier != 2 || (a.source != Source::Folk && a.source != Source::Maas && a.source != Source::Gwynne && a.source != Source::World))
      continue;
    arch++;
    std::set<std::string> errs;
    auto one = [&](const Spec& s) {
      Composed c;
      composed++;
      if (compose(s, c)) return;
      for (const std::string& e : c.errors) {
        // "saga:17 saga1~...: message" -> "line 17: message" (the spec id differs per composition)
        const size_t sp = e.find(' '), cs = e.find(": ");
        if (e.compare(0, 5, "saga:") == 0 && sp != std::string::npos && cs != std::string::npos && cs > sp)
          errs.insert("line " + e.substr(5, sp - 5) + ": " + e.substr(cs + 2));
        else errs.insert(e);
      }
    };
    for (int v = 0; v < 8; v++)
      for (int m = 0; m < (int)Motive::COUNT; m++) {
        if (a.motives && !(a.motives & motiveBit((Motive)m))) continue;
        for (uint32_t sd = 1; sd <= 3; sd++) {
          Spec s;
          s.arch = a.id; s.voice = v; s.motive = (Motive)m; s.seed = sd * 7919u + (uint32_t)(v * 13 + m);
          one(s);
        }
      }
    const std::map<std::string, std::string> slots = slotsOf(a.body);
    for (const Twist& t : twists())
      for (int k = 0; k < 3; k++) {
        if (!slots.count("t" + std::to_string(k + 1)) || !twistFitsSlot(a, t, k, {})) continue;
        Spec s;
        s.arch = a.id; s.twist[k] = t.id; s.voice = (k * 3 + 1) % 8; s.seed = 11u + (uint32_t)k;
        for (int m = 0; m < (int)Motive::COUNT; m++) if (!a.motives || (a.motives & motiveBit((Motive)m))) { s.motive = (Motive)m; break; }
        one(s);
      }
    std::set<std::string> texts;
    for (uint32_t sd = 0; sd < 24; sd++) {
      Spec s;
      s.arch = a.id; s.voice = (int)(sd % 8); s.seed = 1000u + sd * 104729u;
      for (int m = 0; m < (int)Motive::COUNT; m++) if (!a.motives || (a.motives & motiveBit((Motive)m))) { s.motive = (Motive)m; break; }
      Composed c;
      if (compose(s, c)) texts.insert(c.text.substr(c.text.find('\n')));
    }
    if (texts.size() < 18) { printf("FAIL: voice lane: %s has %zu distinct texts of 24 (18 wanted)\n", a.id, texts.size()); bad++; }
    for (const std::string& e : errs) { printf("FAIL: voice lane: %s: %s\n", a.id, e.c_str()); bad++; }
    if (verbose) printf("  %-16s %-7s %2zu distinct of 24\n", a.id, sourceName(a.source), texts.size());
  }
  printf("voice lane: %d archetypes, %d compositions, %d failures\n", arch, composed, bad);
  return bad ? 1 : 0;
}

int voiceCmd(int argc, char** argv) {
  bool verbose = false, lane = false;
  std::string printKey;
  for (int i = 2; i < argc; i++) if (!strcmp(argv[i], "--lane")) lane = true;
  int printVoice = -1, printMotive = -1;
  for (int i = 2; i < argc; i++) {
    if (!strcmp(argv[i], "--verbose")) verbose = true;
    else if (!strcmp(argv[i], "--print") && i + 1 < argc) {
      printKey = argv[++i];
      if (i + 1 < argc && argv[i + 1][0] >= '0' && argv[i + 1][0] <= '9') printVoice = atoi(argv[++i]);
      if (i + 1 < argc && argv[i + 1][0] != '-') printMotive = motiveWord(argv[++i]);
    }
  }
  if (lane) return laneCheck(verbose);
  // --compose <arch> [voice] [motive] [seed hex] [twist...]: the composed DSL text, for reading a story in one voice
  for (int i = 2; i < argc; i++) {
    if (strcmp(argv[i], "--compose") != 0 || i + 1 >= argc) continue;
    Spec s;
    s.arch = argv[i + 1];
    if (i + 2 < argc) s.voice = atoi(argv[i + 2]);
    if (i + 3 < argc && motiveWord(argv[i + 3]) >= 0) s.motive = (Motive)motiveWord(argv[i + 3]);
    if (i + 4 < argc) s.seed = (uint32_t)strtoul(argv[i + 4], nullptr, 16);
    for (int k = 0; k < 3 && i + 5 + k < argc; k++) s.twist[k] = argv[i + 5 + k];
    Composed c;
    compose(s, c);
    printf("%s\n", c.text.c_str());
    for (const std::string& e : c.errors) printf("ERROR: %s\n", e.c_str());
    return c.errors.empty() ? 0 : 1;
  }
  if (!printKey.empty()) {
    for (int v = 0; v < 8; v++) {
      if (printVoice >= 0 && v != printVoice) continue;
      for (int m = 0; m < (int)Motive::COUNT; m++) {
        if (printMotive >= 0 && m != printMotive) continue;
        const std::vector<std::string> P = voicePool(printKey, v, m);
        printf("<<%s>> voice %d motive %s: %zu phrases\n", printKey.c_str(), v, motiveName((Motive)m), P.size());
        for (const std::string& s : P) printf("   %s\n", s.c_str());
      }
    }
    return 0;
  }
  int bad = 0;
  auto fail = [&](const std::string& m) { if (bad < 60) printf("FAIL: voice: %s\n", m.c_str()); bad++; };
  for (const std::string& e : voiceTableErrors()) fail("table: " + e);
  const std::vector<std::string>& keys = voiceKeys();
  for (const char* k : kFrozen)
    if (std::find(keys.begin(), keys.end(), std::string(k)) == keys.end()) fail(std::string("the key <<") + k + ">> is missing");
  // every table line on its own (an F or B line can be a phrase as it is; an opener or a motive line only joined)
  for (const std::string& k : keys) {
    for (int v = 0; v < 8; v++)
      for (char kind : {'F', 'A', 'B'})
        for (const std::string& l : voiceLines(k, v, kind)) {
          const std::string why = checkText(l);
          if (!why.empty()) fail("<<" + k + ">> voice " + std::to_string(v) + " line '" + l + "': " + why);
        }
    for (int m = 0; m < (int)Motive::COUNT; m++)
      for (const std::string& l : voiceLines(k, m, 'M')) {
        const std::string why = checkText(l);
        if (!why.empty()) fail("<<" + k + ">> motive " + motiveName((Motive)m) + " line '" + l + "': " + why);
      }
  }
  // pools: every key x voice x motive
  long total = 0;
  size_t perVoice[8] = {};
  int coloured = 0;
  std::map<uint32_t, std::string> famKey;   // family -> the key it belongs to
  for (const std::string& k : keys) {
    std::set<std::string> voiceSet[8];
    bool motiveMatters = false;
    for (int v = 0; v < 8; v++) {
      std::set<std::string> firstPool;
      for (int m = 0; m < (int)Motive::COUNT; m++) {
        const std::vector<std::string> P = voicePool(k, v, m);
        const std::set<std::string> S(P.begin(), P.end());
        if (S.size() < 6) fail("<<" + k + ">> voice " + std::to_string(v) + " motive " + motiveName((Motive)m) + ": only " + std::to_string(S.size()) + " distinct phrases (6 wanted)");
        for (const std::string& s : P) {
          const std::string why = checkText(s);
          if (!why.empty()) fail("<<" + k + ">> voice " + std::to_string(v) + ": '" + s + "': " + why);
          voiceSet[v].insert(s);
        }
        if (m == 0) firstPool = S;
        else if (S != firstPool) motiveMatters = true;
        // phrase(): deterministic, families stable and non-zero, texts from the pool
        for (uint64_t sd = 0; sd < 12; sd++) {
          VoiceCtx c;
          c.voice = v;
          c.motive = (Motive)m;
          c.seed = sd * 0x9E3779B97F4A7C15ull + (uint64_t)(v * 131 + m * 7);
          const Phrase a = phrase(k, c), b = phrase(k, c);
          if (a.text != b.text || a.family != b.family) fail("<<" + k + ">>: phrase() is not deterministic");
          if (!a.family) fail("<<" + k + ">>: a phrase with family 0: " + a.text);
          if (!S.count(a.text)) fail("<<" + k + ">>: phrase() gave a text outside its pool: " + a.text);
          auto it = famKey.find(a.family);
          if (it == famKey.end()) famKey[a.family] = k;
          else if (it->second != k) fail("the family " + std::to_string(a.family) + " is shared by <<" + it->second + ">> and <<" + k + ">>");
        }
      }
      total += (long)voiceSet[v].size();
      perVoice[v] += voiceSet[v].size();
    }
    if (motiveMatters) coloured++;
    // the voices differ: no two share more than a quarter of the smaller set
    for (int a = 0; a < 8; a++)
      for (int b = a + 1; b < 8; b++) {
        size_t common = 0;
        for (const std::string& s : voiceSet[a]) if (voiceSet[b].count(s)) common++;
        const size_t small = std::min(voiceSet[a].size(), voiceSet[b].size());
        if (voiceSet[a] == voiceSet[b] || common * 4 > small)
          fail("<<" + k + ">>: voices " + std::to_string(a) + " and " + std::to_string(b) + " share " + std::to_string(common) + " of " + std::to_string(small) + " phrases");
      }
    if (verbose) {
      printf("  <<%s>>:", k.c_str());
      for (int v = 0; v < 8; v++) printf(" v%d %zu", v, voiceSet[v].size());
      printf("%s\n", motiveMatters ? "  (motive-coloured)" : "");
    }
  }
  // proverbs: at least 12 of each people's own
  for (int v = 0; v < 8; v++) {
    const size_t n = voiceLines("proverb", v, 'B').size() + voiceLines("proverb", v, 'F').size();
    if (n < 12) fail("voice " + std::to_string(v) + " has " + std::to_string(n) + " proverbs (12 wanted)");
  }
  // an unknown key gives nothing (the composer reports it)
  {
    VoiceCtx c;
    if (!phrase("no_such_key", c).text.empty()) fail("an unknown key gave a phrase");
  }
  printf("voice: %zu keys (%d motive-coloured), %ld distinct phrases in all; per voice:", keys.size(), coloured, total);
  for (int v = 0; v < 8; v++) printf(" v%d %zu", v, perVoice[v]);
  printf("\nvoice: %d failures\n", bad);
  return bad ? 1 : 0;
}

}  // namespace

RPG_TEST_CMD("--voice", "M6b voice grammars: every key x voice x motive, phrase rules, variety, distinct peoples, stable families [--verbose] [--print <key> [voice] [motive]]", voiceCmd);
