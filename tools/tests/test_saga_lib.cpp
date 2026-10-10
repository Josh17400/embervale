// rpg_test --sagalib [--print <archetype> [twist ...]] [--verbose]: the M6b saga library lint (rpg/story/saga.h). No world
// is needed: every check is on the templates, the twists, the voice and the campaign plans themselves. ARCHETYPES lane
// (phase A: lead).
//   - archetype and twist ids are unique and lower case; names, sources, tiers and bodies are filled in
//   - every tier-2 archetype composes into a VALID script (the M4 validator) in every voice family 0..7, for every motive
//     it allows, for several seeds, and with every twist that fits each of its slots (one slot at a time)
//   - every twist fits at least one archetype; every <<key>> a template uses is a voice key (compose reports it)
//   - no template names a modern work's people, places or invented terms (the owner's rule, 15.20)
//   - every voice key gives an upper-case phrase in every voice and motive, at most 60 characters, using only the
//     placeholders {PLAYER} {GIVER} {GIVER.GOD} {GIVER.PEOPLE} {HOME}
//   - every campaign plan composes with each arc's first choice
//   - report: archetypes by source against the 70-at-ship target, distinct texts per archetype over 24 seeds
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <vector>
#include "rpg/story/dsl.h"
#include "rpg/story/saga.h"
#include "tools/tests/tests.h"

namespace {

using namespace story::saga;

bool lowerId(const char* s) {
  if (!s || !*s) return false;
  for (const char* p = s; *p; p++) if (!((*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9') || *p == '_')) return false;
  return true;
}

std::string upperS(std::string s) { for (char& c : s) if (c >= 'a' && c <= 'z') c = (char)(c - 32); return s; }

// modern works' names and invented terms (themes are borrowed; these never are)
const char* const kBanned[] = {
    // Lewis
    "NARNIA", "ASLAN", "PEVENSIE", "JADIS", "CAIR PARAVEL", "TUMNUS", "CALORMEN", "ARCHENLAND", "TELMAR",
    // Tolkien
    "MORDOR", "GONDOR", "ROHAN", "SAURON", "GANDALF", "FRODO", "BILBO", "HOBBIT", "ARAGORN", "ISILDUR", "MIDDLE-EARTH",
    "RIVENDELL", "MORIA", "BALROG", "NAZGUL", "MITHRIL", "SILMARIL", "NUMENOR", "LOTHLORIEN", "SMAUG", "SHELOB",
    "GOLLUM", "PALANTIR", "ISENGARD", "SARUMAN", "ELROND", "GALADRIEL",
    "ERECH", "DUNHARROW", "TREEBEARD", "FANGORN", "ANDURIL", "NARSIL", "PELARGIR", "MINAS TIRITH", "BEORN", "HOOM",
    "GREY HOST", "DUNEDAIN", "MIDDLE EARTH", "THE SHIRE", "BAGGINS", "SAMWISE", "BARROW-WIGHT", "ENTWIFE", "ENTMOOT",
    // Lewis (more)
    "CASPIAN", "REEPICHEEP", "PUDDLEGLUM", "STONE TABLE", "TURKISH DELIGHT", "LAMP-POST", "WHITE WITCH", "EMETH",
    "DAWN TREADER", "FURTHER UP AND FURTHER IN",
    // Maas
    "PRYTHIAN", "VELARIS", "RHYSAND", "FEYRE", "TAMLIN", "ILLYRIAN", "NIGHT COURT", "SPRING COURT", "AMARANTHA",
    "CELAENA", "AELIN", "ADARLAN", "TERRASEN", "VALG", "WYRDKEY", "CRESCENT CITY",
    // Gwynne
    "CORBAN", "NATHAIR", "ASROTH", "ELYON", "KADOSHIM", "BEN-ELIM", "BANISHED LANDS", "STORM THE WOLVEN", "CYWEN",
    "GAR ", "DRAIG", "RIV ", "DREM", "BLEAK MERE", "OF THE FAITHFUL AND THE FALLEN",
};

int checkVoice(bool verbose) {
  int bad = 0;
  static const std::set<std::string> allowed = {"player", "giver", "home"};
  for (const std::string& k : voiceKeys()) {
    std::set<std::string> seen;
    for (int v = 0; v < 8; v++)
      for (int m = 0; m < (int)Motive::COUNT; m++)
        for (uint64_t sd = 0; sd < 6; sd++) {
          VoiceCtx c;
          c.voice = v;
          c.motive = (Motive)m;
          c.seed = sd * 0x9E3779B97F4A7C15ull + (uint64_t)(v * 31 + m);
          const Phrase p = phrase(k, c);
          if (p.text.empty()) { printf("FAIL: voice: <<%s>> is empty in voice %d, motive %s\n", k.c_str(), v, motiveName((Motive)m)); bad++; continue; }
          seen.insert(p.text);
          if (p.text.size() > 60) { printf("FAIL: voice: <<%s>> is longer than 60 characters: %s\n", k.c_str(), p.text.c_str()); bad++; }
          bool lowerCase = false;
          for (const auto& ph : story::dsl::placeholders(p.text)) {
            const bool ok = (ph.first == "player" && ph.second.empty()) || (ph.first == "home" && ph.second.empty()) ||
                            (ph.first == "giver" && (ph.second.empty() || ph.second == "god" || ph.second == "people"));
            if (!ok) { printf("FAIL: voice: <<%s>> uses {%s%s%s}: %s\n", k.c_str(), ph.first.c_str(), ph.second.empty() ? "" : ".", ph.second.c_str(), p.text.c_str()); bad++; }
          }
          bool inPh = false;
          for (char ch : p.text) { if (ch == '{') inPh = true; else if (ch == '}') inPh = false; else if (!inPh && ch >= 'a' && ch <= 'z') lowerCase = true; }
          if (lowerCase) { printf("FAIL: voice: <<%s>> has lower case: %s\n", k.c_str(), p.text.c_str()); bad++; }
        }
    if (verbose) printf("  voice <<%s>>: %zu distinct phrases\n", k.c_str(), seen.size());
  }
  return bad;
}

int sagaLibCmd(int argc, char** argv) {
  bool verbose = false;
  std::vector<std::string> printSpec;
  for (int i = 2; i < argc; i++) {
    if (!strcmp(argv[i], "--verbose")) verbose = true;
    else if (!strcmp(argv[i], "--print")) { while (i + 1 < argc && argv[i + 1][0] != '-') printSpec.push_back(argv[++i]); }
  }
  if (!printSpec.empty()) {
    Spec s;
    s.arch = printSpec[0];
    for (size_t k = 1; k < printSpec.size() && k <= 3; k++) s.twist[k - 1] = printSpec[k];
    s.seed = 1;
    Composed c;
    compose(s, c);
    printf("%s\n", c.text.c_str());
    for (const std::string& e : c.errors) printf("ERROR: %s\n", e.c_str());
    return c.errors.empty() ? 0 : 1;
  }
  int bad = 0;
  auto fail = [&](const std::string& m) { printf("FAIL: sagalib: %s\n", m.c_str()); bad++; };
  const std::vector<Archetype>& A = archetypes();
  const std::vector<Twist>& T = twists();
  // ---- ids, names, fields
  std::set<std::string> ids;
  for (const Archetype& a : A) {
    if (!lowerId(a.id)) fail(std::string("archetype id '") + a.id + "' is not lower case [a-z0-9_]");
    if (!ids.insert(a.id).second) fail(std::string("two archetypes are called ") + a.id);
    if (!a.name || !*a.name || upperS(a.name) != a.name) fail(std::string("archetype ") + a.id + ": its name must be upper case");
    if (a.tier != 2 && a.tier != 3) fail(std::string("archetype ") + a.id + ": tier 2 or 3");
    if (!a.body || std::strlen(a.body) < 200) fail(std::string("archetype ") + a.id + ": no body");
    if (a.source >= Source::COUNT) fail(std::string("archetype ") + a.id + ": no source");
    if (a.twists && slotsOf(a.body).empty()) fail(std::string("archetype ") + a.id + ": twist families but no slot");
    if (!a.twists && !slotsOf(a.body).empty()) fail(std::string("archetype ") + a.id + ": slots but no twist families");
    for (int k = 0; k < 3; k++) {
      const uint32_t m = slotFamilies(a.body, k);
      if (m & ~a.twists) fail(std::string("archetype ") + a.id + ": slot t" + std::to_string(k + 1) + " names a family the archetype does not list");
      if (slotsOf(a.body).count("t" + std::to_string(k + 1))) {
        bool any = false;
        for (const Twist& t : T) if (twistFitsSlot(a, t, k, {})) any = true;
        if (!any && !T.empty()) printf("  (note: no twist fits slot t%d of %s yet)\n", k + 1, a.id);
      }
    }
  }
  std::set<std::string> tids;
  for (const Twist& t : T) {
    if (!lowerId(t.id)) fail(std::string("twist id '") + t.id + "' is not lower case [a-z0-9_]");
    if (!tids.insert(t.id).second) fail(std::string("two twists are called ") + t.id);
    if (!t.family || (t.family & (t.family - 1))) fail(std::string("twist ") + t.id + ": exactly one family bit");
    if (!t.body || !*t.body) fail(std::string("twist ") + t.id + ": no body");
  }
  // ---- banned names
  // (a banned word must begin a word: "GAR " is a name, "BEGGAR " is not)
  auto scan = [&](const std::string& what, const char* body) {
    const std::string B = upperS(body ? body : "");
    for (const char* w : kBanned)
      for (size_t at = B.find(w); at != std::string::npos; at = B.find(w, at + 1)) {
        const char before = at ? B[at - 1] : ' ';
        if ((before >= 'A' && before <= 'Z') || (before >= '0' && before <= '9')) continue;
        fail(what + " names '" + w + "' (a modern work's own word)");
        break;
      }
  };
  for (const Archetype& a : A) scan(std::string("archetype ") + a.id, a.body);
  for (const Twist& t : T) scan(std::string("twist ") + t.id, t.body);
  for (const CampaignPlan& c : campaignPlans()) {
    scan(std::string("campaign ") + c.id, c.head);
    scan(std::string("campaign ") + c.id, c.finale);
  }
  // ---- every tier-2 archetype composes, in every voice and allowed motive, with every fitting twist
  int composed = 0, failedCompose = 0, shownErr = 0;
  std::set<std::string> seenErr;
  std::map<std::string, int> twistUses;
  for (const Archetype& a : A) {
    if (a.tier != 2) continue;
    auto tryOne = [&](const Spec& s) {
      Composed c;
      composed++;
      if (!compose(s, c)) {
        failedCompose++;
        // (every distinct problem is printed once, without the spec id and the measured length that differ per seed,
        // so one long line in a template reads as one failure line, not two hundred)
        for (size_t i = 0; i < c.errors.size() && i < 6; i++) {
          std::string e = c.errors[i];
          const size_t sid = e.find("saga1~");
          if (sid != std::string::npos) {
            const size_t sp = e.find(' ', sid);
            e.erase(sid, sp == std::string::npos ? std::string::npos : sp - sid);
          }
          for (size_t k = 0; k + 1 < e.size(); k++)
            if (e[k] == '(' && e[k + 1] >= '0' && e[k + 1] <= '9') {
              const size_t g = e.find(" > ", k);
              if (g != std::string::npos) e.erase(k + 1, g - k);
            }
          if (seenErr.insert(std::string(a.id) + "|" + e).second && shownErr++ < 200)
            printf("FAIL: sagalib: %s (e.g. %s): %s\n", a.id, specId(s).c_str(), e.c_str());
        }
        bad++;
      }
    };
    for (int v = 0; v < 8; v++)
      for (int m = 0; m < (int)Motive::COUNT; m++) {
        if (a.motives && !(a.motives & motiveBit((Motive)m))) continue;
        for (uint32_t sd = 1; sd <= 3; sd++) {
          Spec s;
          s.arch = a.id; s.voice = v; s.motive = (Motive)m; s.seed = sd * 7919u + (uint32_t)v;
          tryOne(s);
        }
      }
    const std::map<std::string, std::string> slots = slotsOf(a.body);
    for (const Twist& t : T) {
      if (!twistFits(a, t, {})) continue;
      for (int k = 0; k < 3; k++) {
        if (!slots.count("t" + std::to_string(k + 1)) || !twistFitsSlot(a, t, k, {})) continue;
        twistUses[t.id]++;
        Spec s;
        s.arch = a.id; s.twist[k] = t.id; s.voice = (k * 3) % 8; s.seed = 11u + (uint32_t)k;
        if (a.motives) for (int m = 0; m < (int)Motive::COUNT; m++) if (a.motives & motiveBit((Motive)m)) { s.motive = (Motive)m; break; }
        tryOne(s);
      }
    }
  }
  for (const Twist& t : T) if (!twistUses.count(t.id)) fail(std::string("twist ") + t.id + " fits no archetype");
  // ---- the voice
  bad += checkVoice(verbose);
  // ---- campaign plans compose with each arc's first choice
  for (const CampaignPlan& c : campaignPlans()) {
    Spec s;
    s.campaign = true;
    s.arch = c.id;
    for (const ArcSlot& sl : c.arcs) s.arcs.push_back(sl.choices.empty() ? std::string() : sl.choices[0]);
    s.seed = 5;
    Composed cc;
    if (!composeCampaign(s, cc)) {
      printf("FAIL: sagalib: campaign %s does not compose:\n", c.id);
      for (size_t i = 0; i < cc.errors.size() && i < 8; i++) printf("    %s\n", cc.errors[i].c_str());
      bad++;
    }
  }
  // ---- report: sources, variety
  const LibraryReport r = report();
  printf("saga library: %d archetypes (%d stories, %d campaign arcs), %d twists, %d voice keys, %d campaign plans\n", r.archetypes,
         r.tier2, r.arcs, r.twists, r.voiceKeys, r.campaigns);
  printf("  by source:");
  for (int s = 0; s < (int)Source::COUNT; s++) printf(" %s %d", sourceName((Source)s), r.bySource[s]);
  printf("\n  target at ship: 70 story archetypes (%d now), 6 campaigns incl. emptythrone (%d plans + 1)\n", r.tier2, r.campaigns);
  for (const Archetype& a : A) {
    if (a.tier != 2) continue;
    std::set<std::string> texts;
    int words = 0;
    for (uint32_t sd = 0; sd < 24; sd++) {
      Spec s;
      s.arch = a.id; s.voice = (int)(sd % 8); s.seed = 1000u + sd * 104729u;
      if (a.motives) for (int m = 0; m < (int)Motive::COUNT; m++) if (a.motives & motiveBit((Motive)m)) { s.motive = (Motive)m; break; }
      Composed c;
      if (compose(s, c)) {
        // the text without the script line (its id names the seed)
        texts.insert(c.text.substr(c.text.find('\n')));
        words = c.words;
      }
    }
    printf("  %-14s %-9s %-34s %2zu distinct of 24, %4d words\n", a.id, sourceName(a.source), a.name, texts.size(), words);
  }
  printf("sagalib: %d compositions, %d failed; %d failures\n", composed, failedCompose, bad);
  return bad ? 1 : 0;
}

}  // namespace

RPG_TEST_CMD("--sagalib", "M6b saga library lint: archetypes, twists, voice, campaign plans compose and validate; banned names [--print <arch> [twists]] [--verbose]", sagaLibCmd);
