// rpg_test --sagas [N] [--seeds A..B] [--verbose] [--sample K] [--no-play]: M6b generated stories on real worlds
// (rpg/story/saga.h). COMPOSER lane. Per seed, N stories (default 40; CI: 2000 x 5 seeds = 10,000) are composed from the
// archetype library (every archetype in turn, twists drawn per slot, every voice and allowed motive), cast at a hook
// place of the world that HAS what the archetype needs (wars, famines and news are forced at the hook like rpg_test
// --story does; a grieving household and a hungry neighbour are made in the start town), and EVERY path of each is
// walked headlessly to an ending. Checks (15.20 "Quality bar and tests"):
//   - every composition is a valid script (parser + M4 validator), composes the same twice, and casts on the world
//   - every bound entity exists and is reachable: places load, positions lie on land, census residents live, named
//     beasts and world bosses are the foes generator's own, givers are real people
//   - every path ends (the journal quest closes), every ending of the script is reached, no "{" "}" "[[" "<<" "%" "@"
//     and no lower case in any text the player reads
//   - rewards stay inside the M6 bands: no `do loot`, no `do gold` above 40; every stage pays at most its rewards'
//     gold and any gear is uncommon / rare at the place's own item level
//   - uniqueness metrics: distinct (archetype, twists) pairs, (archetype, twists, cast shape) triples and story texts per
//     1,000 stories; phrase-family reuse. GATED once the library is full (>= 60 story archetypes): >= 900 distinct texts
//     and >= 300 distinct pairs per 1,000
//   - (the first seed) a play walk: offers drawn from the people and boards of 30 settlements over two months, accepting
//     some; no two offers within GUARD_RADIUS share archetype + twists (the repetition guard), offers stay rare
// The walker visits each (stage, variables) state once (a script's graph is a DAG under forced conditions; a cycle is
// reported), snapshots the realm only for scripts that touch it, and composes each spec once (saga::script's cache).
//
// rpg_test --saga-read N [--seeds S]: N stories printed in full for the WRITING-QUALITY REVIEW LENS: the cast, then
// every stage of the script (what is said, the journal, the options, the memories and facts it leaves) filled in with the
// real names of this world.
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <vector>
#include "rpg/sim/foes.h"
#include "rpg/sim/gear.h"
#include "rpg/sim/life.h"
#include "rpg/sim/party.h"
#include "rpg/sim/party_voice.h"
#include "rpg/sim/talk.h"
#include "rpg/story/dsl.h"
#include "rpg/story/saga.h"
#include "rpg/story/story.h"
#include "rpg/story/story_internal.h"
#include "rpg/world/ids.h"
#include "rpg/world/source.h"
#include "tools/tests/tests.h"

namespace {

using namespace story::saga;
namespace dsl = story::dsl;
bool g_sverbose = false;
bool g_trace = false;

void steps(Game& g, int n) {
  for (int i = 0; i < n; i++) { g.update(SIM_DT, Input()); g.events.clear(); }
}

// ---------------------------------------------------------------- snapshots (the walker restores between branches)
struct Snap {
  story::Engine st;
  realm::Realm rm;
  bool hasRealm = false;
  int gold = 0, plXp = 0, plLevel = 1, nextQuestId = 1, trackedQuest = -1, day = 1;
  std::vector<Item> inv;
  std::map<uint64_t, int32_t> marks;
  std::vector<Quest> quests;
  party::Party party;   // (M8: companion stories change the party: recruits, fates, approval)
};
Snap snap(const Game& g, bool realmToo) {
  Snap s;
  s.st = g.story; s.gold = g.gold; s.plXp = g.plXp; s.plLevel = g.plLevel; s.nextQuestId = g.nextQuestId;
  s.trackedQuest = g.trackedQuest; s.inv = g.inv; s.marks = g.marks; s.quests = g.quests; s.day = g.day;
  s.party = g.party;
  if (realmToo) { s.rm = g.realm; s.hasRealm = true; }
  return s;
}
void restore(Game& g, const Snap& s) {
  g.story = s.st; g.gold = s.gold; g.plXp = s.plXp; g.plLevel = s.plLevel; g.nextQuestId = s.nextQuestId;
  g.trackedQuest = s.trackedQuest; g.inv = s.inv; g.marks = s.marks; g.quests = s.quests; g.day = s.day;
  // (census people who joined on a path the snapshot did not take are free again: RF_COMPANION would otherwise leak
  // across thousands of walks until no adult of the town is left to stage)
  for (const party::Member& m : g.party.members) {
    bool had = false;
    for (const party::Member& o : s.party.members) if (o.id == m.id) { had = true; break; }
    if (had || !m.site || m.res < 0) continue;
    if (life::Census* c = g.life.findMut(m.site))
      if (m.res < (int)c->res.size()) c->res[(size_t)m.res].flags = (uint16_t)(c->res[(size_t)m.res].flags & ~(life::RF_AWAY | life::RF_COMPANION));
  }
  g.party.members = s.party.members;
  g.party.fallen = s.party.fallen;
  g.party.nextId = s.party.nextId;
  g.party.pending.clear();
  // (actors embodying members the snapshot did not have stop being companions)
  for (Actor& a : g.actors) if (a.comp > g.party.members.size()) a.comp = 0;
  if (s.hasRealm) g.realm = s.rm;
  g.events.clear();
  g.mode = Mode::Play;
}

// does the script change the realm (effects, or goals the walker completes by forcing the realm)?
bool touchesRealm(const dsl::Script& s) {
  for (const dsl::StageDef& G : s.stages) {
    if (G.kind == dsl::StageKind::Goal && (G.goal.type == dsl::ObjType::War || G.goal.type == dsl::ObjType::Famine || G.goal.type == dsl::ObjType::Rep))
      return true;
    for (const dsl::EffDef& e : G.effs)
      if (e.type == dsl::EffType::Rep || e.type == dsl::EffType::Fame || e.type == dsl::EffType::Rumour ||
          (e.type >= dsl::EffType::RealmWar && e.type <= dsl::EffType::RealmEvent))
        return true;
  }
  return false;
}

// text the player reads: upper case, every placeholder filled, no composer markup left behind
const char* unreadable(const std::string& t) {
  for (char c : t) if (c >= 'a' && c <= 'z') return "lower case";
  if (t.find('{') != std::string::npos || t.find('}') != std::string::npos) return "an unfilled placeholder";
  if (t.find("[[") != std::string::npos || t.find("]]") != std::string::npos) return "a [[...]] left";
  if (t.find("<<") != std::string::npos || t.find(">>") != std::string::npos) return "a <<voice>> slot left";
  if (t.find('%') != std::string::npos || t.find('@') != std::string::npos) return "a %local or @target left";
  return nullptr;
}

std::string varsKey(const std::map<std::string, int32_t>& v) {
  std::string k;
  for (const auto& kv : v) { k += kv.first; k += '='; k += std::to_string(kv.second); k += ';'; }
  return k;
}

// ---------------------------------------------------------------- (M7b) the conversations as the player hears them
// The walker rebuilds every conversation of every path the way the dialogue shows it (the hook's line and pitch, each
// talk stage's line and options, the answer to the option chosen: the same person talking on, a closing line, or the
// dialogue closing) and lints it (speech::lint with the cast's names, plus the answer check below). --review FILE
// writes a stratified sample (per archetype x hook x motive) in the phone panel's wrap for the dialogue-quality review.
struct TalkPath {
  speech::Conversation conv;      // the open conversation (no turns: none open)
  std::string role;               // the role in front of the player
  std::set<std::string> met;      // roles who have spoken to the player on this path (introduced themselves)
  std::set<std::string> known;    // names the player heard in an earlier conversation of this path
  bool forced = false;            // the path took an option the dialogue would have hidden (not linted from there on)
  std::set<std::string> placesKnown;   // places an earlier conversation of this path named (and placed, or the player went)
};
constexpr uint32_t C_ANSWER = 1u << 20;   // (this test's own check) the answer does not take up the option chosen
struct TalkStats {
  long convs = 0, turns = 0, answers = 0, answerMiss = 0, placeConvs = 0, placeIssues = 0, rawName = 0, rawDouble = 0, forcedConvs = 0, talkAnswer = 0;
  std::map<uint32_t, long> byCheck;               // conversations with at least one issue of the check
  std::map<std::string, long> sites;              // archetype | check | the offending text: how often (--talk-report)
  std::map<uint32_t, std::vector<std::string>> examples;
  std::vector<std::string> placeExamples;
  std::set<uint32_t> seenConv;                    // conversations already linted (the walker meets a prefix many times)
  std::string reviewKey;                          // archetype|hook|motive of the story being walked
  std::map<std::string, int> reviewTaken;
  std::string review;
  int reviewed = 0, reviewPer = 0;
};
TalkStats g_talk;
std::vector<std::string> g_castNames, g_placeNames;   // the walked story's people and places (bound names)
std::string g_pitch;                                  // the walked story's pitch (the hook's invitation)
std::vector<std::string> g_allPlaces;                 // every place of the cast, the hook's home too
std::set<std::string> g_ambiguous;                    // names that are also words or other names of the story (not judged)
std::vector<std::pair<std::string, std::string>> g_nameRole;   // bound name -> {role} (the report folds seeds together)

std::string asTemplate(std::string t) {
  for (const auto& nr : g_nameRole)
    for (size_t p = t.find(nr.first); p != std::string::npos; p = t.find(nr.first, p + nr.second.size())) t.replace(p, nr.first.size(), nr.second);
  return t;
}

bool talkStop(const std::string& w) {
  static const char* const k[] = {"THE", "A", "AN", "AND", "OF", "TO", "IN", "IS", "IT", "I", "YOU", "YOUR", "MY", "ME", "WE", "OUR", "THAT", "THIS", "ON",
                                  "AT", "FOR", "WITH", "BE", "ARE", "WAS", "IT'S", "I'M", "BUT", "OR", "SO", "NOT", "NO", "DO", "HAVE", "HAS", "HE",
                                  "SHE", "THEY", "THEM", "HIS", "HER", "AS", "IF", "BY", "I'LL", "WILL", "WHAT", "WHY", "WHO", "HOW", "WHERE",
                                  "THEN", "THERE", "HERE", "YES", "AYE", "NOW", "CAN", "COULD", "WOULD", "SHOULD", "LET", "ALL", "ONE", "ANY",
                                  "SOME", "ABOUT", "FROM", "INTO", "WHEN", "JUST", "ONLY", "TELL", "KNOW", "SEE", "GO", "COME", "GET", "TAKE",
                                  "GIVE", "MAKE", "WANT", "NEED", "THINK", "WELL", "VERY", "MORE", "MUCH", "BACK", "OUT", "UP", "OFF", "OVER",
                                  "DON'T", "WON'T", "CAN'T", "YOU'RE", "WE'LL", "THAT'S", "LEAVE", "ASK", "SAY", "TRY", "LOOK", "FIND", "HELP",
                                  "SORRY", "GOOD", "MUST", "SHALL", "MAY", "THEIR", "HIM", "US", "BEEN", "BEING", "DID", "DOES", "DONE", "SURE",
                                  "STILL", "EVEN", "AGAIN", "FIRST", "TOO", "YET", "OWN", "SUCH", "THAN", "THOSE", "THESE", "WHICH", "FAREWELL"};
  for (const char* x : k) if (w == x) return true;
  return false;
}
std::vector<std::string> talkWords(const std::string& s) {
  std::vector<std::string> w;
  std::string cur;
  for (char ch : s) {
    const bool letter = (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '\'';
    if (letter) cur += ch;
    else if (!cur.empty()) { w.push_back(cur); cur.clear(); }
  }
  if (!cur.empty()) w.push_back(cur);
  return w;
}
// two words of one stem (the first five letters; a shorter word whole, or its first four letters)
bool sameStem(const std::string& a, const std::string& b) {
  const size_t m = std::min(a.size(), b.size());
  if (m < 4) return a == b;
  return a.compare(0, std::min<size_t>(5, m), b, 0, std::min<size_t>(5, m)) == 0;
}
// does the answer take up what the option ASKED? A question ("WHY DID ATHANAS RUN?") must be answered about its
// subject: the reply shares a content word with it, or, when it asks after someone or something by pronoun ("WHY DID
// HE RUN?"), the reply speaks of a person or thing of the line it asked about. A statement or a promise ("I'LL FIND
// HIM.") is answered by a reaction, which need share nothing (the lint's C_ECHO catches a reply that ignores it). The
// hook's own pitch is the invitation to tell the story, answered by the whole opening.
bool answers(const std::string& opt, const std::string& reply, const std::string& prev, const std::vector<std::string>& names) {
  if (reply == prev) return false;
  if (opt.empty() || opt.back() != '?') return true;
  {
    const std::vector<std::string> ow0 = talkWords(opt);
    bool wh = false;
    for (const std::string& w : ow0)
      if (w == "WHAT" || w == "WHAT'S" || w == "WHERE" || w == "WHERE'S" || w == "WHY" || w == "HOW" || w == "WHICH" || w == "WHEN" || w == "WHO" ||
          w == "WHO'S" || w == "WHOSE")
        wh = true;
    if (!wh) return true;   // (a yes-or-no question is answered by a stance: "DEAD THESE TWO YEARS.")
    const std::vector<std::string> rw0 = talkWords(reply);
    bool replyPronoun = false;
    for (const std::string& r : rw0)
      if (r == "HE" || r == "SHE" || r == "HIM" || r == "HER" || r == "HIS" || r == "THEY" || r == "THEM" || r == "HE'S" || r == "SHE'S") replyPronoun = true;
    // a question after someone by name ("WHY DID ATHANAS RUN?") answered about them ("HE SAYS HE WAS NEVER PAID.")
    for (const std::string& n : names) if (opt.find(n) != std::string::npos && (replyPronoun || reply.find(n) != std::string::npos)) return true;
    // what the engine says when the story moves on is no answer to a question (it must say who can answer it)
    {
      static const char* const canned[] = {"THANK YOU. ", "GOOD. ", "THEN GO WELL. ", "THEN YOU NEED ", "GO AND SEE ", "SPEAK WITH ", "YOU SHOULD SPEAK WITH ",
                                           "GO AND FIND "};
      for (const char* k : canned) if (reply.compare(0, strlen(k), k) == 0) return false;
    }
    // a WHERE answered with a place or a way ("CERNESES'S REDOUBT IS A SHORT WALK TO THE NORTH.")
    for (const std::string& w : ow0)
      if (w == "WHERE" || w == "WHERE'S") {
        for (const std::string& r : rw0)
          if (r == "NORTH" || r == "SOUTH" || r == "EAST" || r == "WEST" || r == "NORTH-EAST" || r == "NORTH-WEST" || r == "SOUTH-EAST" ||
              r == "SOUTH-WEST" || r == "HERE" || r == "THERE" || r == "IN" || r == "AT" || r == "BY" || r == "UNDER" || r == "BEHIND")
            return true;
        for (const std::string& n : g_placeNames) if (reply.find(n) != std::string::npos) return true;
      }
    for (const std::string& w : ow0) {
      // (a WHY is answered by a reason, which seldom repeats the question's words: "DEBTS. A TEMPER. A NIGHT I'M NOT
      // PROUD OF." The echo and repeat checks still catch a reply that only restates)
      if (w == "WHY" || w == "WHAT" || w == "WHAT'S" || w == "HOW" || w == "WHICH" || w == "WHEN") return true;
      if (w == "WHO" || w == "WHO'S" || w == "WHOSE") {
        for (const std::string& n : names) if (reply.find(n) != std::string::npos) return true;
        for (const std::string& r : rw0) if (r == "I" || r == "ME" || r == "MY" || r == "HE" || r == "SHE" || r == "THEY" || r == "NOBODY" || r == "NO-ONE") return true;
      }
    }
  }
  std::vector<std::string> ow;
  bool pronoun = false;
  for (const std::string& w : talkWords(opt)) {
    if (w == "HE" || w == "SHE" || w == "HIM" || w == "HER" || w == "HIS" || w == "THEY" || w == "THEM" || w == "IT" || w == "THAT" || w == "THIS" ||
        w == "THERE" || w == "THEIR")
      pronoun = true;
    if (w.size() >= 3 && !talkStop(w)) ow.push_back(w);
  }
  const std::vector<std::string> rw = talkWords(reply);
  for (const std::string& a : ow)
    for (const std::string& b : rw) if (sameStem(a, b)) return true;
  if (pronoun || ow.empty()) {
    for (const std::string& b : rw)
      if (b == "HE" || b == "SHE" || b == "HIM" || b == "HER" || b == "HIS" || b == "THEY" || b == "THEM" || b == "IT" || b == "IT'S" || b == "THEIR")
        return true;
    for (const std::string& n : names) if (reply.find(n) != std::string::npos && prev.find(n) != std::string::npos) return true;
  }
  return ow.empty();
}

// (the lint's introduction rule, and the forms it does not know yet, all of which say who someone is with their name:
// a name in apposition after a noun phrase, "MY YOUNGEST, ELION," / "A TINKER FROM OVER THE BORDER, ARN,"; a title or
// trade before it, "REEVE ARN" / "THE TINKER ARN"; a role after it, "ARN, CAPTAIN OF THE CAMP," / "ARN, CHANCELLOR TO")
bool titleWord(const std::string& p) {
  static const char* const k[] = {"REEVE", "STEWARD", "CHANCELLOR", "SERGEANT", "HUNTSMAN", "WARDEN", "TINKER", "PEDDLER", "HERBALIST", "HEALER",
                                  "MILLER", "BAKER", "WEAVER", "SHEPHERD", "LADY", "LORD", "EMIR", "KHAN", "JARL", "SPEAKER", "YOUNGEST",
                                  "ELDEST", "NEPHEW", "NIECE", "GRANDSON", "GRANDDAUGHTER", "GRANDMOTHER", "GRANDFATHER", "APPRENTICE",
                                  "JOURNEYMAN", "MASTER", "MISTRESS", "CAPTAIN", "PRIESTESS", "PRIEST", "SMITH", "HUNTER", "FARMER", "FISHER",
                                  "INNKEEPER", "MERCHANT", "GUARD", "SOLDIER", "WIDOW", "WIDOWER", "ELDER", "KING", "QUEEN", "PRINCE",
                                  "PRINCESS", "REGENT", "BAILIFF", "TITHE-TAKER", "SEER", "HERMIT", "STRANGER", "NOVICE", "SQUIRE", "CHAMPION",
                                  "NEIGHBOUR", "FRIEND", "BROTHER", "SISTER", "SON", "DAUGHTER", "MOTHER", "FATHER", "WIFE", "HUSBAND",
                                  "UNCLE", "AUNT", "COUSIN", "OLD", "YOUNG", "LITTLE", "CALLED", "NAMED", "BOY", "GIRL", "LAD", "LASS",
                                  "SWEETHEART", "BELOVED", "BETROTHED", "RANGER", "HUNTSMAN", "QUARRY-MASTER", "KINSMAN", "KINSWOMAN"};
  for (const char* x : k) if (p == x) return true;
  return false;
}
// 1 introduced at its first whole-word mention in `text`, 0 mentioned there without who they are, -1 not mentioned
int introducedHere(const std::string& text, const std::string& name) {
  // (the first whole-word mention: judged there)
  size_t at = std::string::npos;
  for (size_t p = text.find(name); p != std::string::npos; p = text.find(name, p + 1)) {
    const bool l = p == 0 || !((text[p - 1] >= 'A' && text[p - 1] <= 'Z') || text[p - 1] == '\'');
    const size_t q = p + name.size();
    const bool r = q >= text.size() || !((text[q] >= 'A' && text[q] <= 'Z') || (text[q] == '\'' && q + 1 < text.size() && text[q + 1] != 'S'));
    if (l && r) { at = p; break; }
  }
  if (at == std::string::npos) return -1;
  const std::vector<std::string> before = talkWords(text.substr(0, at)), after = talkWords(text.substr(at + name.size()));
  if (!before.empty() && titleWord(before.back())) return 1;
  // apposition: ", NAME" after a noun phrase of this sentence that began with an article or a possessive
  if (at >= 2 && text.compare(at - 2, 2, ", ") == 0) {
    size_t cl = text.find_last_of(".!?", at);
    const std::string clause = text.substr(cl == std::string::npos ? 0 : cl + 1, at - (cl == std::string::npos ? 0 : cl + 1));
    for (const std::string& w : talkWords(clause))
      if (w == "A" || w == "AN" || w == "MY" || w == "OUR" || w == "YOUR" || w == "HIS" || w == "HER" || w == "THEIR" || w == "THE") return 1;
  }
  // "NAME, CAPTAIN OF ..." / "NAME, CHANCELLOR TO ..." / "NAME, A WIDOW ..." / "NAME, WHO ..."
  const size_t e = at + name.size();
  if (e + 1 < text.size() && text[e] == ',' && after.size() >= 2 &&
      (after[1] == "OF" || after[1] == "TO" || after[0] == "A" || after[0] == "AN" || after[0] == "THE" || after[0] == "MY" || after[0] == "WHO" ||
       titleWord(after[0])))
    return 1;
  // (the lint's own rule, read at the same whole-word mention)
  static const char* const lintBefore[] = {"MY", "OUR", "YOUR", "CALLED", "NAMED", "I'M", "AM", "IS"};
  if (!before.empty()) for (const char* b : lintBefore) if (before.back() == b) return 1;
  static const char* const lintAfter[] = {"THE", "MY", "OUR", "WHO", "A", "AN", "OF", "IS", "WAS"};
  if (!after.empty()) for (const char* a : lintAfter) if (after[0] == a) return 1;
  return 0;
}

void closeConv(TalkPath& tp) {
  if (tp.conv.turns.empty()) return;
  if (tp.forced) { tp.conv = speech::Conversation(); tp.role.clear(); g_talk.forcedConvs++; return; }
  const speech::Conversation c = tp.conv;
  tp.conv = speech::Conversation();
  tp.role.clear();
  const std::string fmt = speech::formatPhone(c);
  std::string knownKey;
  for (const std::string& n : tp.known) knownKey += n + ",";
  // the names heard in it are known from now on (judged at their first mention: here), and the one who spoke
  tp.known.insert(c.who);
  std::set<std::string> placesBefore = tp.placesKnown;
  for (const std::string& n : g_placeNames)
    for (const speech::Turn& t : c.turns) if (t.text.find(n) != std::string::npos) { tp.placesKnown.insert(n); break; }
  for (const std::string& n : g_castNames)
    for (const speech::Turn& t : c.turns) if (t.text.find(n) != std::string::npos) { tp.known.insert(n); break; }
  if (!g_talk.seenConv.insert(speech::lineHash(fmt + "|" + knownKey)).second) return;
  g_talk.convs++;
  g_talk.turns += (long)c.turns.size();
  speech::LintCtx ctx;
  ctx.names = g_castNames;
  for (const std::string& n : g_castNames) if (knownKey.find(n + ",") != std::string::npos) ctx.known.push_back(n);
  std::vector<speech::Issue> raw = speech::lint(c, ctx), is;
  {   // (the TALK lane's own answer relation, reported beside this test's: speech::C_ANSWER is not in C_ALL)
    speech::LintCtx ac;
    ac.checks = speech::C_ANSWER;
    if (!speech::lint(c, ac).empty()) g_talk.talkAnswer++;
  }
  // (two lint findings this test reads more closely: a name introduced in apposition after a possessive noun; a
  // sentence opening on a hesitation, " ...VOND?", is not broken punctuation. Both are counted raw too.)
  for (const speech::Issue& x : raw) {
    if (x.check == speech::C_NAME) {
      g_talk.rawName++;
      bool ok = false;
      for (const speech::Turn& t : c.turns) {
        const int r = introducedHere(t.text, x.what);
        if (r >= 0) { ok = r == 1; break; }
      }
      if (ok) continue;
    }
    if (x.check == speech::C_SELF && g_ambiguous.count(c.who)) continue;   // (a speaker called BOLD, and a lord "THE BOLD")
    if (x.check == speech::C_FRAGMENT) {
      // (one-word sentences that are whole speech: an imperative, a name said alone; the lint does not know them yet)
      static const char* const whole[] = {"SIT.", "REST.", "DRINK.", "EAT.", "HURRY.", "RUN.", "PRAY.", "LISTEN.", "WATCH.", "SPEAK.", "BREATHE.",
                                          "KNEEL.", "SLEEP.", "SWEAR.", "STAY.", "FIGHT.", "RIDE.", "CHOOSE.", "FORGIVE.", "REMEMBER.", "LEAVE."};
      bool ok = false;
      for (const char* w : whole) if (x.what == w) ok = true;
      for (const std::string& n : g_castNames) if (x.what == n + ".") ok = true;
      for (const std::string& n : g_allPlaces) if (x.what == n + ".") ok = true;
      if (ok) continue;
    }
    if (x.check == speech::C_DOUBLE) {
      g_talk.rawDouble++;
      if (x.turn >= 0 && x.turn < (int)c.turns.size()) {
        speech::Conversation one;
        speech::Turn t = c.turns[(size_t)x.turn];
        for (size_t p = t.text.find(" ..."); p != std::string::npos; p = t.text.find(" ...", p + 1)) t.text.replace(p, 4, " ");
        if (t.text.compare(0, 3, "...") == 0) t.text = t.text.substr(3);
        t.opts.clear();
        one.turns.push_back(t);
        speech::LintCtx dc;
        dc.checks = speech::C_DOUBLE;
        if (speech::lint(one, dc).empty()) continue;
      }
    }
    is.push_back(x);
  }
  for (size_t t = 0; t + 1 < c.turns.size(); t++) {
    const speech::Turn& T = c.turns[t];
    if (T.chosen < 0 || T.chosen >= (int)T.opts.size()) continue;
    if (T.opts[(size_t)T.chosen] == g_pitch) continue;
    g_talk.answers++;
    if (!answers(T.opts[(size_t)T.chosen], c.turns[t + 1].text, T.text, g_castNames)) {
      g_talk.answerMiss++;
      speech::Issue x;
      x.check = C_ANSWER;
      x.turn = (int)t + 1;
      x.what = "\"" + T.opts[(size_t)T.chosen] + "\" -> " + c.turns[t + 1].text.substr(0, 100);
      is.push_back(x);
    }
  }
  {   // a place the conversation names but never says where it is (a soft measure: the journal and the map marker
      // carry it too). Placed: a direction or "HERE" follows its name in some line ("LISTA IS A LONG WALK TO THE EAST",
      // "IN VOND, A SHORT WALK TO THE NORTH", "HERE IN VOND")
    g_talk.placeConvs++;
    std::string all;
    for (const speech::Turn& t : c.turns) all += t.text + " ";
    for (const std::string& n : g_placeNames) {
      if (introducedHere(all, n) < 0 || placesBefore.count(n)) continue;
      bool placed = false;
      for (size_t p = all.find(n); p != std::string::npos && !placed; p = all.find(n, p + 1)) {
        const std::string after = all.substr(p + n.size(), 48);
        if (after.find("NORTH") != std::string::npos || after.find("SOUTH") != std::string::npos || after.find(" EAST") != std::string::npos ||
            after.find(" WEST") != std::string::npos || (p >= 8 && all.compare(p - 8, 8, "HERE IN ") == 0))
          placed = true;
      }
      if (placed) continue;
      g_talk.placeIssues++;
      if (g_talk.placeExamples.size() < 300) g_talk.placeExamples.push_back(n + " in: " + all.substr(0, 160));
      break;
    }
  }
  uint32_t seen = 0;
  {
    const std::string arch = g_talk.reviewKey.substr(0, g_talk.reviewKey.find('|'));
    for (const speech::Issue& x : is) {
      const std::string line = x.turn >= 0 && x.turn < (int)c.turns.size() ? c.turns[(size_t)x.turn].text : std::string();
      g_talk.sites[arch + " | " + (x.check == C_ANSWER ? "answer" : speech::checkName(x.check)) + " | " + asTemplate(x.what).substr(0, 90) + "  <<  " +
                   asTemplate(line).substr(0, 200)]++;
    }
  }
  for (const speech::Issue& x : is) {
    if (seen & x.check) continue;
    seen |= x.check;
    g_talk.byCheck[x.check]++;
    auto& ex = g_talk.examples[x.check];
    if (ex.size() < 400) ex.push_back(c.who + " (turn " + std::to_string(x.turn) + "): " + x.what);
    if (std::getenv("EMB_SAGADUMP") && x.check == speech::C_REPEAT) printf("---- %s\n%s", g_talk.reviewKey.c_str(), fmt.c_str());
  }
  if (!g_talk.reviewKey.empty() && g_talk.reviewPer > 0 && g_talk.reviewTaken[g_talk.reviewKey] < g_talk.reviewPer) {
    g_talk.reviewTaken[g_talk.reviewKey]++;
    g_talk.reviewed++;
    std::string t = "#" + std::to_string(g_talk.reviewed) + " " + g_talk.reviewKey + "\n" + fmt;
    for (const speech::Issue& x : is)
      t += "   !! " + std::string(x.check == C_ANSWER ? "answer does not take up the option" : speech::checkName(x.check)) + " (turn " +
           std::to_string(x.turn) + "): " + x.what + "\n";
    g_talk.review += t + "\n";
  }
}

// the turn as the panel shows it; optIx: each shown option's index in the stage (hidden ones are not offered)
speech::Turn turnFor(Game& g, const story::Instance& in, const dsl::Script& sc, bool introduce, std::vector<int>& optIx) {
  speech::Turn t;
  const dsl::StageDef& G = sc.stages[(size_t)in.stage];
  const story::Binding* b = story::bindingOf(in, G.talk);
  t.speaker = b ? b->name : G.talk;
  t.text = g.story.talkLine(g, in, introduce);
  optIx.assign(G.opts.size(), -1);
  for (size_t i = 0; i < G.opts.size(); i++) {
    if (!g.story.optionShown(g, in, (int)i)) continue;
    optIx[i] = (int)t.opts.size();
    t.opts.push_back(story::fillText(g, in, G.opts[i].label));
  }
  t.opts.push_back("FAREWELL.");
  return t;
}

struct Walk {
  int nodes = 0, ends = 0, bad = 0;
  std::set<std::string> endNames, memo;
  size_t maxSay = 0, over300 = 0;
  std::string maxSayText;
};

// the stage just entered pays no more than its rewards and gifts, and its gear is in the place's band
void checkPay(Game& g, const story::Instance& in, const dsl::Script& sc, int gold0, size_t inv0, int D, const std::string& name, Walk& w) {
  if (in.stage < 0 || in.stage >= (int)sc.stages.size()) return;
  const dsl::StageDef& G = sc.stages[(size_t)in.stage];
  int allowGold = 0, items = 0;
  // (M8b, 15.27.4) the gear a reward may give: a stage on the way Uncommon to Rare (the M6b rule), a success ending
  // (a finale) more: Uncommon or better for a tale, Epic / Legendary or better for a campaign (story::rewardSource)
  const gear::LootSrc src = story::rewardSrcOf(sc, G);
  int tierMax = 0;
  for (const dsl::EffDef& e : G.effs) {
    if (e.type == dsl::EffType::Reward) {
      const story::RewardRoll R = story::rewardRoll(D, e.n, 0, nullptr, 1);
      allowGold += R.gold;
      if (e.n >= 2) items++;
      tierMax = std::max(tierMax, e.n);
    }
    if (e.type == dsl::EffType::Gold && e.n > 0) allowGold += e.n;
    if (e.type == dsl::EffType::Give) items++;
  }
  const int got = g.gold - gold0;
  if (got > allowGold) { out("FAIL: saga %s stage %s pays %d gold (its rewards allow %d)\n", name.c_str(), G.name.c_str(), got, allowGold); w.bad++; }
  int newItems = 0;
  for (size_t i = inv0; i < g.inv.size(); i++) {
    const Item& it = g.inv[i];
    if (it.kind == ItemKind::Quest) continue;
    newItems++;
    const gear::DropSource ds = story::rewardSource(D, tierMax, 0, g.plLevel, src);
    if ((int)it.rarity > (int)ds.atMost) { out("FAIL: saga %s stage %s gives a %s piece (%s at most)\n", name.c_str(), G.name.c_str(), rarityName(it.rarity), rarityName(ds.atMost)); w.bad++; }
    if ((int)it.rarity < (int)ds.atLeast) { out("FAIL: saga %s stage %s (%s) gives a %s piece (%s at least)\n", name.c_str(), G.name.c_str(), gear::lootSrcName(src), rarityName(it.rarity), rarityName(ds.atLeast)); w.bad++; }
    std::string why;
    if (!gear::lootValid(it, &why)) { out("FAIL: saga %s stage %s: %s\n", name.c_str(), G.name.c_str(), why.c_str()); w.bad++; }
    if (it.ilvl && gear::bandOf(it.ilvl) != gear::bandOf(gear::itemLevelFor(ds))) { out("FAIL: saga %s stage %s gives item level %d at danger %d (out of band)\n", name.c_str(), G.name.c_str(), it.ilvl, D); w.bad++; }
  }
  if (newItems > items) { out("FAIL: saga %s stage %s gives %d pieces (its rewards allow %d)\n", name.c_str(), G.name.c_str(), newItems, items); w.bad++; }
}

void walk(Game& g, uint32_t id, const dsl::Script& sc, const std::string& name, Walk& w, std::map<std::string, int>& onPath, int depth,
          bool realmToo, int D, TalkPath tp) {
  story::Instance* in = g.story.find(id);
  if (!in) { out("FAIL: saga %s: the instance vanished\n", name.c_str()); w.bad++; return; }
  const std::string st = g.story.stageName(*in);
  const std::string key = st + "|" + varsKey(in->vars);
  if (w.memo.count(key)) { closeConv(tp); return; }
  w.nodes++;
  for (const std::string& t : g.story.stageTexts(g, *in))
    if (const char* why = unreadable(t)) { out("FAIL: saga %s stage %s: %s in: %s\n", name.c_str(), st.c_str(), why, t.c_str()); w.bad++; }
  const std::string say = g.story.sayText(g, *in);
  if (say.size() > w.maxSay) { w.maxSay = say.size(); w.maxSayText = say; }
  if (say.size() > 300) w.over300++;
  if (in->done || g.story.isEnd(*in)) {
    closeConv(tp);
    w.memo.insert(key);
    w.ends++;
    w.endNames.insert(st);
    bool closed = false;
    for (const Quest& q : g.quests) if (q.id == in->questId && q.type == QType::Story && q.state == QState::Done) closed = true;
    // (fixer M6b r2) turned down at the opening: declined, the journal keeps no entry
    if (!in->questId && in->failed) closed = true;
    if (!closed) { out("FAIL: saga %s ended at %s but its journal quest is not done\n", name.c_str(), st.c_str()); w.bad++; }
    return;
  }
  if (onPath[st] > 0 || depth > 80) { out("FAIL: saga %s: a loop through stage %s\n", name.c_str(), st.c_str()); w.bad++; return; }
  w.memo.insert(key);
  onPath[st]++;
  const Snap s0 = snap(g, realmToo);
  if (g.story.isGoal(*in)) {
    closeConv(tp);
    const int gold0 = g.gold;
    const size_t inv0 = g.inv.size();
    if (!g.story.debugComplete(g, id)) { out("FAIL: saga %s stage %s: the objective cannot be completed\n", name.c_str(), st.c_str()); w.bad++; }
    else {
      if (const story::Instance* in2 = g.story.find(id)) checkPay(g, *in2, sc, gold0, inv0, D, name, w);
      walk(g, id, sc, name, w, onPath, depth + 1, realmToo, D, tp);
    }
    restore(g, s0);
    onPath[st]--;
    return;
  }
  const int n = g.story.optionCount(*in);
  if (n <= 0) { out("FAIL: saga %s stage %s: a dialogue with no options\n", name.c_str(), st.c_str()); w.bad++; onPath[st]--; return; }
  // (which options are checks: read now, before any branch moves the instance on or a restore moves it in memory)
  std::vector<uint8_t> isCheck((size_t)n, 0);
  for (int i = 0; i < n; i++) isCheck[(size_t)i] = g.story.optionIsCheck(*in, i) ? 1 : 0;
  // (M7b) this talk stage's turn of the conversation in front of the player
  const std::string R = in->stage >= 0 && in->stage < (int)sc.stages.size() ? sc.stages[(size_t)in->stage].talk : std::string();
  if (!tp.conv.turns.empty() && tp.role != R) closeConv(tp);
  if (tp.conv.turns.empty()) {
    const story::Binding* b = story::bindingOf(*in, R);
    tp.conv.who = b ? b->name : R;
    tp.conv.role = R;
    tp.role = R;
  }
  std::vector<int> optIx;
  tp.conv.turns.push_back(turnFor(g, *in, sc, !tp.met.count(R), optIx));
  tp.met.insert(R);
  bool first = true;
  for (int i = 0; i < n; i++) {
    const bool check = isCheck[(size_t)i] != 0;
    for (int c = check ? 1 : 0; c <= (check ? 2 : 0); c++) {
      if (!first) restore(g, s0);
      first = false;
      const int gold0 = g.gold;
      const size_t inv0 = g.inv.size();
      if (!g.story.choose(g, id, i, true, c)) { out("FAIL: saga %s stage %s: option %d cannot be chosen\n", name.c_str(), st.c_str(), i); w.bad++; continue; }
      if (g_trace) { const story::Instance* t2 = g.story.find(id); printf("  trace %s -[%d/%d]-> %s\n", st.c_str(), i, c, t2 ? g.story.stageName(*t2).c_str() : "?"); }
      if (const story::Instance* in2 = g.story.find(id)) checkPay(g, *in2, sc, gold0, inv0, D, name, w);
      TalkPath t2 = tp;
      if (i < (int)optIx.size() && optIx[(size_t)i] >= 0) t2.conv.turns.back().chosen = optIx[(size_t)i];
      else t2.forced = true;
      if (const story::Instance* in2 = g.story.find(id)) {
        const bool talkNext = !in2->done && in2->stage >= 0 && in2->stage < (int)sc.stages.size() && sc.stages[(size_t)in2->stage].kind == dsl::StageKind::Talk;
        const bool same = talkNext && sc.stages[(size_t)in2->stage].talk == R;
        std::string text;
        const int shown = t2.conv.turns.back().chosen;
        std::string said;   // (everything said in this conversation so far, as Engine::said_ holds it)
        for (const speech::Turn& T0 : t2.conv.turns) said += T0.text + " ";
        const std::string heard = said + "\n" + (shown >= 0 ? t2.conv.turns.back().opts[(size_t)shown] : std::string());
        const story::Engine::Reply rp = g.story.replyAfter(g, *in2, R, same, heard, st, text);
        if (rp == story::Engine::Reply::Close) {
          speech::Turn T;
          T.speaker = t2.conv.turns.back().speaker;
          // (an ending told as a story is shown under the tale's title: Engine::choose)
          if (sc.stages[(size_t)in2->stage].kind == dsl::StageKind::End && story::Engine::narration(text, t2.conv.who)) T.speaker = sc.title;
          T.text = text;
          T.opts.push_back("FAREWELL.");
          t2.conv.turns.push_back(T);
          closeConv(t2);
        } else if (rp == story::Engine::Reply::Silent) closeConv(t2);
      }
      walk(g, id, sc, name, w, onPath, depth + 1, realmToo, D, t2);
    }
  }
  restore(g, s0);
  onPath[st]--;
}

// ---------------------------------------------------------------- the cast is real
int checkCast(Game& g, const story::Instance& in, const dsl::Script& sc, const std::string& name) {
  int bad = 0;
  auto fail = [&](const std::string& m) { out("FAIL: saga %s: %s\n", name.c_str(), m.c_str()); bad++; };
  for (size_t ri = 0; ri < sc.roles.size() && ri < in.cast.size(); ri++) {
    const dsl::RoleDef& R = sc.roles[ri];
    const story::Binding& b = in.cast[ri];
    if (b.name.empty()) fail("role '" + R.name + "' has no name");
    if (b.hasPos && g.world.src) {
      if (std::abs(b.gx) > (1 << 28) || std::abs(b.gy) > (1 << 28)) fail("role '" + R.name + "' lies off the world");
      else if (R.kind == dsl::RoleKind::Site || R.kind == dsl::RoleKind::Ruin || R.kind == dsl::RoleKind::Beast || R.kind == dsl::RoleKind::Boss) {
        const ew::MacroSample m = g.world.src->macroFar(b.gx, b.gy);
        if (m.elev < ew::ELEV_SEA && !m.water) fail("role '" + R.name + "' (" + b.name + ") lies in the sea");
      }
    }
    switch (R.kind) {
      case dsl::RoleKind::Site: case dsl::RoleKind::Ruin: case dsl::RoleKind::Capital: {
        const ew::Gid sid = b.site ? b.site : b.id;
        if (story::siteByIdLoad(g, sid) < 0) fail("role '" + R.name + "' (" + b.name + ") is not a place that loads");
        break;
      }
      case dsl::RoleKind::Resident:
        if (b.trade == story::RESIDENT_CENSUS) {
          const int ix = story::residentIndexOf(g, b);
          const life::Census* c = g.life.find(b.site);
          if (ix < 0 || !c) fail("resident '" + R.name + "' (" + b.name + ") is not in the census");
          else if (c->res[(size_t)ix].flags & life::RF_DEAD) fail("resident '" + R.name + "' (" + b.name + ") is dead");
          else if (c->res[(size_t)ix].name != b.name) fail("resident '" + R.name + "': the census calls them " + c->res[(size_t)ix].name);
        } else if (R.a != "any") fail("resident '" + R.name + "' (" + R.a + ") was invented");
        break;
      case dsl::RoleKind::Beast: {
        bool found = false;
        for (const foes::NamedUnique& u : foes::namedInRegion(*g.world.src, ew::regionOf(b.gx), ew::regionOf(b.gy))) if (u.id == b.id) found = true;
        if (!found) fail("beast '" + R.name + "' (" + b.name + ") is not a named unique of its region");
        break;
      }
      case dsl::RoleKind::Boss: {
        bool ok = false;
        const foes::WorldBoss wb = foes::worldBossOf(*g.world.src, ew::EndlessSource::kcellOf(b.gx), ew::EndlessSource::kcellOf(b.gy), ok);
        if (!ok || wb.id != b.id) fail("boss '" + R.name + "' (" + b.name + ") is not its kingdom cell's world boss");
        break;
      }
      case dsl::RoleKind::Kingdom:
        if (!g.realm.kingdom(b.id) && !(g.world.src && g.world.src->kingdom(b.id))) fail("kingdom '" + R.name + "' is unknown");
        break;
      default: break;
    }
  }
  return bad;
}

// ---------------------------------------------------------------- hooks that have what the archetype needs
// the world events an event story needs, forced at its hook place (as rpg_test --story does)
void forceHookEvent(Game& g, const dsl::Script& s, int site) {
  if (s.hook != dsl::HookKind::Event || site < 0) return;
  const Site S = g.world.sites[(size_t)site];
  const int32_t hx = g.world.ox + S.ex, hy = g.world.oy + S.ey;
  const ew::Gid land = story::landAt(g, hx, hy);
  const ew::Gid home = S.homeKingdom >= 0 ? g.world.kingdoms[(size_t)S.homeKingdom].id : 0;
  g.realm.noteSite(S.id, home, (uint8_t)S.type, hx, hy);
  if (s.hookEv == (int)realm::EvType::Famine) g.realm.forceFamine(S.id, g.day);
  else if (s.hookEv == (int)realm::EvType::WarDeclared) {
    ew::Gid other = 0;
    for (const realm::KingdomState& K : g.realm.kingdoms()) if (K.id != land && !K.fallen) { other = K.id; break; }
    if (other && land) {
      if (!g.realm.atWar(other, land)) g.realm.forceWar(other, land, g.day);
      g.realm.forceEvent(realm::EvType::WarDeclared, other, land, S.id, hx, hy, g.day);
    }
  } else g.realm.forceEvent((realm::EvType)s.hookEv, land, 0, S.id, hx, hy, g.day);
}

// the living world the library asks for, made at the start town once per seed: a war of its kingdom, a famine, a
// household in mourning, a hungry neighbour (every census of the loaded window is built)
void makeWorldFacts(Game& g) {
  const int ss = g.world.startSite;
  if (ss < 0) return;
  const Site S = g.world.sites[(size_t)ss];
  const int32_t hx = g.world.ox + S.ex, hy = g.world.oy + S.ey;
  const ew::Gid land = story::landAt(g, hx, hy);
  ew::Gid other = 0;
  double bd = 1e30;
  for (const realm::KingdomState& K : g.realm.kingdoms()) {
    if (K.id == land || K.fallen) continue;
    const ew::KingdomPlan* kp = g.world.src->kingdom(K.id);
    const double d = kp ? std::hypot((double)(kp->gx - hx), (double)(kp->gy - hy)) : 1e20;
    if (d < bd) { bd = d; other = K.id; }
  }
  if (other && land && !g.realm.atWar(other, land)) g.realm.forceWar(other, land, g.day);
  // every settlement of the loaded window: a famine, a death in a household of three or more (its kin and friends
  // grieve), and a hungry, penniless neighbour
  std::vector<int> towns = {ss};
  for (int si : g.world.nearSites)
    if (si >= 0 && si < (int)g.world.sites.size() && si != ss && g.world.sites[(size_t)si].settlement()) towns.push_back(si);
  for (int si : towns) {
    const Site T = g.world.sites[(size_t)si];
    const ew::Gid home = T.homeKingdom >= 0 ? g.world.kingdoms[(size_t)T.homeKingdom].id : 0;
    g.realm.noteSite(T.id, home, (uint8_t)T.type, g.world.ox + T.ex, g.world.oy + T.ey);
    g.realm.forceFamine(T.id, g.day);
    life::Census* C = g.life.census(g.world, si);
    if (!C) continue;
    std::map<int, int> hh;
    for (const life::Resident& r : C->res) if (!(r.flags & life::RF_DEAD)) hh[r.household]++;
    int died = -1;
    for (const life::Resident& r : C->res)
      if (!(r.flags & (life::RF_DEAD | life::RF_AWAY)) && r.age >= 30 && hh[r.household] >= 3 && r.actor < 0) { died = r.idx; break; }
    if (died >= 0) g.life.residentDied(T.id, died, g.day);
    if (life::Census* M = g.life.findMut(T.id))
      for (life::Resident& r : M->res)
        if (!(r.flags & (life::RF_DEAD | life::RF_AWAY | life::RF_GRIEVING)) && r.age >= 20) { r.need[(size_t)life::Need::Hunger] = 20; r.need[(size_t)life::Need::Money] = 15; break; }
    if (g_sverbose) {
      int grieving = 0, ties = 0, actorsHere = 0, residents = 0;
      std::map<int, int> roles;
      for (const life::Resident& r : C->res) if (r.flags & life::RF_GRIEVING) grieving++;
      for (const life::Tie& t : C->ties) if (t.b != life::PLAYER_TIE) ties++;
      for (const Actor& a : g.actors) if (a.npc && a.human && story::homeSiteOf(g, a) == si) { actorsHere++; roles[(int)a.role]++; if (a.resident >= 0) residents++; }
      std::string rl;
      for (const auto& kv : roles) rl += std::string(" ") + dsl::tradeName(kv.first) + "x" + std::to_string(kv.second);
      printf("world facts at %s: %zu residents, %d grieving, %d ties; %d people out (%d residents):%s\n", T.name.c_str(), C->res.size(), grieving, ties,
             actorsHere, residents, rl.c_str());
    }
  }
}

bool tradeFits(int trade, Role r) {
  if (r == Role::Child || r == Role::King || r == Role::Herald) return false;
  if (trade == -2 || trade < 0) return r != Role::Guard && r != Role::Jarl;
  return trade == (int)r;
}

// (M8) the staged companion of a companion story: every member parted first, then a census adult of `site` recruited
// with the stake the archetype needs. The kin of a "kindead" story is marked dead in the census (dropCompanions
// restores it)
struct Staged { ew::Gid site = 0; int deadIdx = -1; uint16_t deadFlags = 0; int resIdx = -1, resActor = -1; };
Staged g_staged;
void dropCompanions(Game& g) {
  for (int i = (int)g.party.members.size() - 1; i >= 0; i--) g.partyDismiss(i);
  g.party.fallen.clear();
  if (g_staged.deadIdx >= 0)
    if (life::Census* c = g.life.findMut(g_staged.site))
      if (g_staged.deadIdx < (int)c->res.size()) c->res[(size_t)g_staged.deadIdx].flags = g_staged.deadFlags;
  if (g_staged.resIdx >= 0)   // (the staged resident's own street body is theirs again)
    if (life::Census* c = g.life.findMut(g_staged.site))
      if (g_staged.resIdx < (int)c->res.size() && c->res[(size_t)g_staged.resIdx].actor < 0 && g_staged.resActor >= 0 &&
          story::host().findActor && story::host().findActor(g, g_staged.resActor) >= 0)
        c->res[(size_t)g_staged.resIdx].actor = g_staged.resActor;
  g_staged = Staged();
}
uint32_t startCompanion(Game& g, const std::string& id, const dsl::Script& sc, int site, std::string& why) {
  dropCompanions(g);
  if (site < 0 || site >= (int)g.world.sites.size() || !g.world.sites[(size_t)site].settlement()) { why = "no settlement"; return 0; }
  life::Census* C = g.life.census(g.world, site);
  if (!C) { why = "no census at " + g.world.sites[(size_t)site].name; return 0; }
  const ew::Gid sid = g.world.sites[(size_t)site].id;
  const int fact = sc.hookEv;
  int tries = 0;
  for (size_t ri = 0; ri < C->res.size() && tries < 12; ri++) {
    C = g.life.census(g.world, site);
    if (!C || ri >= C->res.size()) break;
    const life::Resident r = C->res[ri];
    if ((r.flags & (life::RF_DEAD | life::RF_AWAY | life::RF_KEY | life::RF_COMPANION)) || r.age < 18 || r.age > 60 || r.job == life::Job::Child) continue;
    // (the town's tellers keep their posts: no priest, keeper, smith, merchant or guard is staged as the companion)
    if (r.job == life::Job::Priest || r.job == life::Job::Innkeeper || r.job == life::Job::Smith || r.job == life::Job::Merchant ||
        r.job == life::Job::Guard || r.job == life::Job::Noble)
      continue;
    // the household kin the stake is about
    int kin = -1;
    for (const life::Resident& o : C->res)
      if (o.idx != r.idx && o.household == r.household && !(o.flags & (life::RF_DEAD | life::RF_AWAY | life::RF_COMPANION | life::RF_KEY)) &&
          o.job != life::Job::Priest && o.job != life::Job::Innkeeper && o.job != life::Job::Smith && o.job != life::Job::Merchant &&
          o.job != life::Job::Guard && o.job != life::Job::Noble) { kin = o.idx; break; }
    if (fact == dsl::COMPANION_KIN_DEAD && kin < 0) continue;
    tries++;
    Actor a;
    a.id = -97; a.npc = true; a.human = true; a.site = site; a.resident = r.idx; a.p = g.pl().p + Vec2(18, 0); a.name = r.name;
    g_staged.site = sid; g_staged.resIdx = r.idx; g_staged.resActor = r.actor;
    const int mi = g.partyRecruit(a, party::Origin::Tavern);
    if (mi < 0) { why = "the recruit failed"; return 0; }
    party::Member& m = g.party.members[(size_t)mi];
    m.stake = party::Stake();
    m.stake.hook = (party::Hook)sc.hookTrade;
    m.stake.site = sid;
    m.stake.seed = (uint32_t)(r.lookSeed | 1u);
    if (fact == dsl::COMPANION_VETERAN) m.p.brave = 1;
    if (fact == dsl::COMPANION_DESERTER) m.p.brave = -1;
    m.stake.day = g.day - 400;
    const Site& S = g.world.sites[(size_t)site];
    if (S.kingdom >= 0) m.stake.kingdom = g.world.kingdoms[(size_t)S.kingdom].id;
    const int rh = g.world.findSiteNear(g.world.ox + S.ex, g.world.oy + S.ey, SiteType::Ruin, 3);
    if (rh >= 0) m.stake.ruin = g.world.sites[(size_t)rh].id;
    if (kin >= 0 && (fact == dsl::COMPANION_KIN_ALIVE || fact == dsl::COMPANION_KIN_DEAD || m.stake.hook == party::Hook::Love)) {
      m.stake.kin = life::npcId(sid, kin);
      if (fact == dsl::COMPANION_KIN_DEAD) {
        g_staged.deadIdx = kin; g_staged.deadFlags = C->res[(size_t)kin].flags;
        C->res[(size_t)kin].flags |= life::RF_DEAD;
      }
    }
    m.approval = 60;
    m.bond = 40;
    std::string w;
    const uint32_t inst = g.story.start(g, id, m.actor, site, &w);
    if (inst) return inst;
    why = w;
    dropCompanions(g);
  }
  if (why.empty()) why = "no adult to stage as a companion in " + g.world.sites[(size_t)site].name;
  return 0;
}

// cast `id` at a hook place of this world that has what it needs; the instance id (0: none fits, why says so)
uint32_t startAtFittingHook(Game& g, const std::string& id, const dsl::Script& sc, uint32_t needs, std::string& why) {
  const int32_t gx = g.world.ox + (int32_t)std::floor(g.pl().p.x / TILE), gy = g.world.oy + (int32_t)std::floor(g.pl().p.y / TILE);
  std::vector<int> sites;
  auto add = [&](int h) { if (h >= 0 && std::find(sites.begin(), sites.end(), h) == sites.end()) sites.push_back(h); };
  switch (sc.hook) {
    case dsl::HookKind::Herald: {
      const ew::Gid land = story::landAt(g, gx, gy);
      if (const realm::KingdomState* K = g.realm.kingdom(land)) add(story::siteByIdLoad(g, K->capital));
      add(g.world.findSiteNear(gx, gy, SiteType::City, 4, true));
      break;
    }
    case dsl::HookKind::Ruin:
      add(g.world.findSiteNear(gx, gy, SiteType::Ruin, 3));
      for (int si : g.world.nearSites) if (si >= 0 && g.world.sites[(size_t)si].type == SiteType::Ruin) add(si);
      break;
    case dsl::HookKind::Board:
      add(g.world.findSiteNear(gx, gy, SiteType::Town, 3));
      add(g.world.findSiteNear(gx, gy, SiteType::City, 4));
      for (int si : g.world.nearSites) if (si >= 0 && (g.world.sites[(size_t)si].type == SiteType::Town || g.world.sites[(size_t)si].type == SiteType::City)) add(si);
      break;
    default:
      add(g.world.startSite);
      for (int si : g.world.nearSites) if (si >= 0 && g.world.sites[(size_t)si].settlement()) add(si);
      break;
  }
  why = "no hook place";
  std::vector<std::string> reasons;
  auto keep = [&]() { if (why != "no hook place" && reasons.size() < 3 && (reasons.empty() || reasons.back() != why)) reasons.push_back(why); };
  if (sc.hook == dsl::HookKind::Companion) {
    // (M8) a companion's own story: a census resident of the hook settlement recruited, their stake made to fit the
    // archetype's (its kin of the household, alive or dead as the fact says; their kingdom; the nearest ruin)
    // (where the run stands: the settlement here and the nearest of each kind too, the start village may be out of reach)
    add(g.curSite >= 0 && g.curSite < (int)g.world.sites.size() && g.world.sites[(size_t)g.curSite].settlement() ? g.curSite : -1);
    add(g.world.findSiteNear(gx, gy, SiteType::Village, 3));
    add(g.world.findSiteNear(gx, gy, SiteType::Town, 3));
    add(g.world.findSiteNear(gx, gy, SiteType::City, 4));
    why = "no settlement near to stage a companion in";
    for (int site : sites)
      if (const uint32_t inst = startCompanion(g, id, sc, site, why)) return inst;
    // (nowhere here holds a census in reach: back to the start village, whose census is always there)
    const int home = g.world.startSite;
    if (home >= 0 && home < (int)g.world.sites.size()) {
      g.world.sites[(size_t)home].discovered = true;
      if (g.fastTravel(home)) {
        steps(g, 60);
        if (const uint32_t inst = startCompanion(g, id, sc, g.world.startSite, why)) return inst;
      }
    }
    return 0;
  }
  for (int site : sites) {
    keep();
    Hook hk;
    hk.kind = (int)sc.hook;
    hk.site = site;
    hk.ev = sc.hook == dsl::HookKind::Event ? sc.hookEv : -1;
    forceHookEvent(g, sc, site);
    const uint32_t miss = needsMissing(g, hk, needs);
    if (miss) {
      why = std::string(g.world.sites[(size_t)site].name) + " lacks";
      for (uint32_t b = 1; b && b <= N_EVENT; b <<= 1) if (miss & b) why += std::string(" ") + needName(b);
      continue;
    }
    if (sc.hook == dsl::HookKind::Npc || sc.hook == dsl::HookKind::Herald) {
      int tried = 0;
      // (census residents first: kin and friends of the giver need a household)
      for (size_t k2 = 1; k2 < 2 * g.actors.size() && tried < 16; k2++) {
        const size_t k = k2 % g.actors.size();
        const bool residentPass = k2 < g.actors.size();
        if (k == 0) continue;
        const Actor& a = g.actors[k];
        if ((a.resident >= 0) != residentPass) continue;
        if (!a.npc || !a.human || story::isStoryPerson(a) || story::homeSiteOf(g, a) != site || a.st == AState::Dead) continue;
        if (sc.hook == dsl::HookKind::Herald ? a.role != Role::Herald : !tradeFits(sc.hookTrade, a.role)) continue;
        tried++;
        std::string w;
        if (const uint32_t inst = g.story.start(g, id, a.id, site, &w)) return inst;
        why = w;
      }
      // the census's people of that trade who are not out in the street now (the innkeeper behind the bar, the priest
      // at prayer, the farmer in the far field): their own body is brought into play beside the player, as the life
      // simulation does when the player comes near them (a resident's street identity: slot LIFE_SLOT0 + index)
      if (sc.hook == dsl::HookKind::Npc && story::host().spawnHuman && g.world.sites[(size_t)site].settlement()) {
        life::Census* C = g.life.census(g.world, site);
        for (size_t ri = 0; C && ri < C->res.size() && tried < 32; ri++) {
          const life::Resident r = C->res[ri];
          if ((r.flags & (life::RF_DEAD | life::RF_AWAY)) || r.age < 16 || !tradeFits(sc.hookTrade, life::jobRole(r.job))) continue;
          if (r.actor >= 0 && story::host().findActor(g, r.actor) >= 0) continue;   // (out already: tried above)
          tried++;
          Spawn sp;
          sp.npc = true;
          sp.role = life::jobRole(r.job);
          sp.site = site;
          sp.slot = 3000 + (int)ri;
          const int aid = story::host().spawnHuman(g, sp, g.pl().p.x + 24.0f, g.pl().p.y);
          const int ai = story::host().findActor(g, aid);
          if (ai < 0) continue;
          Actor& a = g.actors[(size_t)ai];
          a.site = site; a.bldg = -1; a.slot = 3000 + (int)ri; a.resident = (int)ri; a.name = r.name; a.fromMap = false;
          C->res[ri].actor = aid;
          std::string w;
          const uint32_t inst = g.story.start(g, id, aid, site, &w);
          // (the body goes again: the walk needs only its cast)
          C = g.life.census(g.world, site);
          if (C && ri < C->res.size()) C->res[ri].actor = -1;
          const int ai2 = story::host().findActor(g, aid);
          if (ai2 >= 0) g.actors.erase(g.actors.begin() + ai2);
          if (inst) return inst;
          why = w;
        }
      }
      if (sc.hook == dsl::HookKind::Npc) { if (!tried) why = std::string("no ") + (sc.hookTrade >= 0 ? dsl::tradeName(sc.hookTrade) : "TALKER") + " to tell it in " + g.world.sites[(size_t)site].name; continue; }
    }
    std::string w;
    if (const uint32_t inst = g.story.start(g, id, -1, site, &w)) return inst;
    why = w;
  }
  keep();
  if (!reasons.empty()) { why.clear(); for (const std::string& r : reasons) why += (why.empty() ? "" : "; ") + r; }
  return 0;
}

// ... and when nothing in the loaded window fits (a teller with no friends, no priest in this hamlet), the next towns
// along: the player travels there (the window follows) and the world's facts are made there too
uint32_t startAnywhere(Game& g, const std::string& id, uint32_t needs, std::string& why) {
  const dsl::Script* sc = story::scriptById(id);
  if (!sc) { why = "no script"; return 0; }
  if (const uint32_t inst = startAtFittingHook(g, id, *sc, needs, why)) return inst;
  const int32_t px = g.world.ox + (int32_t)std::floor(g.pl().p.x / TILE), py = g.world.oy + (int32_t)std::floor(g.pl().p.y / TILE);
  std::vector<ew::SettlementNode> towns = g.world.src->settlementsIn(px - 1400, py - 1400, px + 1400, py + 1400, true);
  std::sort(towns.begin(), towns.end(), [&](const ew::SettlementNode& a, const ew::SettlementNode& b) {
    const int64_t da = (int64_t)(a.x - px) * (a.x - px) + (int64_t)(a.y - py) * (a.y - py), db = (int64_t)(b.x - px) * (b.x - px) + (int64_t)(b.y - py) * (b.y - py);
    return da != db ? da < db : a.id < b.id;
  });
  const std::string first = why;
  int moves = 0;
  for (const ew::SettlementNode& T : towns) {
    if (moves >= 9) break;   // (M8: nine towns: a priest-told tale of grief needs both in one place)
    const int64_t d2 = (int64_t)(T.x - px) * (T.x - px) + (int64_t)(T.y - py) * (T.y - py);
    if (d2 < 200 * 200) continue;   // (in the window already)
    moves++;
    g.teleportGlobal(T.x, T.y + 2);
    steps(g, 30);
    makeWorldFacts(g);
    sc = story::scriptById(id);
    if (!sc) break;
    if (const uint32_t inst = startAtFittingHook(g, id, *sc, needs, why)) return inst;
  }
  why = first + " (and " + std::to_string(moves) + " towns further: " + why + ")";
  return 0;
}

// the i-th spec of a seed's run: every tier-2 archetype in turn, a twist drawn for each slot (or none), every voice
Spec specFor(uint64_t seed, int i, const std::vector<const Archetype*>& arch) {
  Spec s;
  const Archetype& a = *arch[(size_t)i % arch.size()];
  uint64_t r = ew::mix64(seed * 0x9E3779B97F4A7C15ull + (uint64_t)i);
  s.arch = a.id;
  s.voice = (int)(r % 8);
  r = ew::mix64(r);
  std::vector<Motive> ms;
  for (int m = 0; m < (int)Motive::COUNT; m++) if (!a.motives || (a.motives & motiveBit((Motive)m))) ms.push_back((Motive)m);
  s.motive = ms[(size_t)(r % ms.size())];
  r = ew::mix64(r);
  s.seed = (uint32_t)r;
  const std::map<std::string, std::string> slots = slotsOf(a.body);
  std::vector<std::string> chosen;
  for (int k = 0; k < 3; k++) {
    if (!slots.count("t" + std::to_string(k + 1))) continue;
    r = ew::mix64(r);
    if (r % 4 == 0) continue;   // a quarter of the slots stay empty
    std::vector<const Twist*> fit;
    for (const Twist& t : twists()) if (twistFitsSlot(a, t, k, chosen)) fit.push_back(&t);
    if (fit.empty()) continue;
    r = ew::mix64(r);
    const Twist* t = fit[(size_t)(r % fit.size())];
    s.twist[k] = t->id;
    chosen.push_back(t->id);
  }
  return s;
}

uint32_t needsOf(const Spec& s) {
  uint32_t n = 0;
  if (const Archetype* a = archetype(s.arch)) n |= a->needs;
  for (int k = 0; k < 3; k++) if (const Twist* t = twist(s.twist[k])) n |= t->needs;
  return n;
}

Game* freshWorld(uint64_t seed) {
  Game* g = new Game(seed);
  g->newEndlessGame(seed);
  g->mode = Mode::Play;
  g->godMode = true;
  g->noWildSpawns = true;
  steps(*g, 90);
  makeWorldFacts(*g);
  return g;
}

// ---------------------------------------------------------------- the play walk: offers across settlements
int playWalk(uint64_t seed, int settlementsN) {
  int bad = 0;
  Game g(seed);
  g.newEndlessGame(seed);
  g.mode = Mode::Play;
  g.godMode = true;
  g.noWildSpawns = true;
  steps(g, 60);
  struct Rec { Spec s; int32_t gx, gy; uint32_t hook; int day; std::string id, where; };
  std::vector<Rec> recs;
  const int32_t sx = g.world.ox + (int32_t)std::floor(g.pl().p.x / TILE), sy = g.world.oy + (int32_t)std::floor(g.pl().p.y / TILE);
  std::vector<ew::SettlementNode> towns = g.world.src->settlementsIn(sx - 2600, sy - 2600, sx + 2600, sy + 2600, true);
  std::sort(towns.begin(), towns.end(), [&](const ew::SettlementNode& a, const ew::SettlementNode& b) {
    const int64_t da = (int64_t)(a.x - sx) * (a.x - sx) + (int64_t)(a.y - sy) * (a.y - sy), db = (int64_t)(b.x - sx) * (b.x - sx) + (int64_t)(b.y - sy) * (b.y - sy);
    return da != db ? da < db : a.id < b.id;
  });
  if ((int)towns.size() > settlementsN) towns.resize((size_t)settlementsN);
  int asked = 0, offered = 0, accepted = 0, boards = 0, boardOffers = 0, visited = 0;
  for (size_t ti = 0; ti < towns.size(); ti++) {
    const ew::SettlementNode& T = towns[ti];
    g.teleportGlobal(T.x, T.y + 2);
    steps(g, 30);
    g.day += 2;   // (two days on the road between towns)
    const int si = story::siteByIdLoad(g, T.id);
    if (si < 0) continue;
    visited++;
    auto record = [&](const std::string& id, const Hook& hk, const std::string& where) {
      Spec s;
      if (!parseSpecId(id, s)) { out("FAIL: play walk: offer %s is not a spec id\n", id.c_str()); bad++; return; }
      int32_t hx = 0, hy = 0;
      const uint32_t hh = hookHashOf(g, hk, hx, hy);
      recs.push_back({s, hx, hy, hh, g.day, id, where});
      noteOffered(g, hk, id);
      // some offers are taken (and told at once: the walker does not play them out)
      if ((ew::mix64(hh ^ (uint64_t)g.day) % 3) == 0) {
        const dsl::Script* sc = story::scriptById(id);
        const int actor = hk.actor;
        std::string why;
        const uint32_t inst = sc ? g.story.start(g, id, actor, hk.site, &why) : 0;
        if (!inst) { out("FAIL: play walk: the offer %s at %s cannot be begun: %s\n", id.c_str(), where.c_str(), why.c_str()); bad++; return; }
        noteTold(g, id, inst, hk);
        g.story.drop(inst);
        accepted++;
      }
    };
    for (size_t k = 1; k < g.actors.size(); k++) {
      const Actor& a = g.actors[k];
      if (!a.npc || !a.human || story::isStoryPerson(a) || story::homeSiteOf(g, a) != si) continue;
      asked++;
      const std::string id = offerFor(g, a);
      if (id.empty()) continue;
      offered++;
      Hook hk;
      hk.kind = (int)dsl::HookKind::Npc;
      hk.actor = a.id;
      hk.site = si;
      // (asking twice gives the same pitch)
      if (offerFor(g, a) != id) { out("FAIL: play walk: %s offers a different story when asked again\n", a.name.c_str()); bad++; }
      record(id, hk, a.name + " of " + T.name);
    }
    if (story::isBoardSite(g, si)) {
      boards++;
      const std::string id = offerForPlace(g, (int)dsl::HookKind::Board, si);
      if (!id.empty()) {
        boardOffers++;
        Hook hk;
        hk.kind = (int)dsl::HookKind::Board;
        hk.site = si;
        record(id, hk, "the board of " + T.name);
      }
    }
  }
  // the guard: no two offers within GUARD_RADIUS share archetype + twists (unless one hook repeated its own pitch)
  int pairsNear = 0;
  for (size_t i = 0; i < recs.size(); i++)
    for (size_t j = i + 1; j < recs.size(); j++) {
      const int64_t dx = recs[i].gx - recs[j].gx, dy = recs[i].gy - recs[j].gy;
      if (dx * dx + dy * dy > (int64_t)Guard::GUARD_RADIUS * Guard::GUARD_RADIUS) continue;
      pairsNear++;
      const Spec& a = recs[i].s;
      const Spec& b = recs[j].s;
      if (a.arch == b.arch && a.twist[0] == b.twist[0] && a.twist[1] == b.twist[1] && a.twist[2] == b.twist[2] && recs[i].hook != recs[j].hook) {
        out("FAIL: play walk: %s (%s, day %d) and %s (%s, day %d) share archetype and twists within %d tiles\n", recs[i].id.c_str(),
            recs[i].where.c_str(), recs[i].day, recs[j].id.c_str(), recs[j].where.c_str(), recs[j].day, (int)Guard::GUARD_RADIUS);
        bad++;
      }
    }
  std::set<std::string> archs;
  for (const Rec& r : recs) archs.insert(r.s.arch);
  printf("sagas play walk seed %llu: %d settlements over %d days, %d people asked, %d offered a story (%.1f %%), %d boards (%d offers), "
         "%d taken; %zu archetypes; %d offer pairs within %d tiles checked\n",
         (unsigned long long)seed, visited, visited * 2, asked, offered, asked ? 100.0 * offered / asked : 0.0, boards, boardOffers, accepted,
         archs.size(), pairsNear, (int)Guard::GUARD_RADIUS);
  return bad;
}

int sagasCmd(int argc, char** argv) {
  uint64_t A = 1, B = 3;
  int N = 40, sample = 0;
  bool play = true;
  std::string only, review, talkReport;
  int talkExamples = 0;
  g_talk = TalkStats();
  for (int i = 2; i < argc; i++) {
    if (!strcmp(argv[i], "--seeds") && i + 1 < argc) parseSeedRange(argv[++i], A, B);
    else if (!strcmp(argv[i], "--verbose")) g_sverbose = true;
    else if (!strcmp(argv[i], "--no-play")) play = false;
    else if (!strcmp(argv[i], "--trace")) g_trace = true;
    else if (!strcmp(argv[i], "--only") && i + 1 < argc) only = argv[++i];
    else if (!strcmp(argv[i], "--sample") && i + 1 < argc) sample = atoi(argv[++i]);
    else if (!strcmp(argv[i], "--review") && i + 1 < argc) review = argv[++i];
    else if (!strcmp(argv[i], "--review-per") && i + 1 < argc) g_talk.reviewPer = atoi(argv[++i]);
    else if (!strcmp(argv[i], "--talk-examples") && i + 1 < argc) talkExamples = atoi(argv[++i]);
    else if (!strcmp(argv[i], "--talk-report") && i + 1 < argc) talkReport = argv[++i];
    else if (argv[i][0] >= '0' && argv[i][0] <= '9') N = atoi(argv[i]);
  }
  std::vector<const Archetype*> arch;
  const bool noComp = std::getenv("EMB_SAGAS_NOCOMP") != nullptr;   // (debug: the library without the companions' own stories)
  for (const Archetype& a : archetypes())
    if (a.tier == 2 && (only.empty() || only == a.id) && !(noComp && std::string(a.body).find("hook companion") != std::string::npos)) arch.push_back(&a);
  if (arch.empty()) { printf("FAIL: sagas: no story archetypes\n"); return 1; }
  int bad = 0, stories = 0, cast = 0, nodes = 0, ends = 0;
  size_t maxSay = 0, over300 = 0;
  std::string maxSayText;
  std::set<std::string> pairs, triples, texts;
  long phraseUses = 0, phraseRepeats = 0;
  double castMs = 0, walkMs = 0, composeMs = 0;
  auto t0 = std::chrono::steady_clock::now();
  for (uint64_t seed = A; seed <= B; seed++) {
    g_curSeed = seed;
    Game* gp = freshWorld(seed);
    Game& g = *gp;
    std::set<uint32_t> famSeen;
    int seedBad = 0;
    for (int i = 0; i < N; i++) {
      const Spec s = specFor(seed, i, arch);
      const std::string id = specId(s);
      stories++;
      auto c0 = std::chrono::steady_clock::now();
      Composed c, c2;
      const bool ok = compose(s, c);
      compose(s, c2);
      if (c.text != c2.text) { out("FAIL: saga %s composes differently twice\n", id.c_str()); seedBad++; }
      if (!ok) {
        out("FAIL: saga %s does not compose:\n", id.c_str());
        for (size_t k = 0; k < c.errors.size() && k < 5; k++) out("    %s\n", c.errors[k].c_str());
        seedBad++;
        continue;
      }
      texts.insert(c.text.substr(c.text.find('\n')));
      for (uint32_t f : c.families) { phraseUses++; if (!famSeen.insert(f).second) phraseRepeats++; }
      const dsl::Script* sc = script(id);
      composeMs += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - c0).count();
      if (!sc) { out("FAIL: saga %s: script() refused what compose() accepted\n", id.c_str()); seedBad++; continue; }
      // rewards inside the bands
      for (const dsl::StageDef& G : sc->stages)
        for (const dsl::EffDef& e : G.effs) {
          if (e.type == dsl::EffType::Loot) { out("FAIL: saga %s: `do loot` in a composed story (use reward)\n", id.c_str()); seedBad++; }
          if (e.type == dsl::EffType::Gold && e.n > 40) { out("FAIL: saga %s: `do gold %d` (over 40: use reward)\n", id.c_str(), e.n); seedBad++; }
        }
      const bool realmToo = touchesRealm(*sc);
      const Snap s0 = snap(g, true);
      auto c1 = std::chrono::steady_clock::now();
      std::string why;
      // (M8) a companion's own story is staged where the run stands; when it had to travel to find its places, the run
      // goes back afterwards, so the companions' stories do not move the other archetypes' hook places
      const int32_t comeX = g.world.ox + (int32_t)std::floor(g.pl().p.x / TILE), comeY = g.world.oy + (int32_t)std::floor(g.pl().p.y / TILE);
      const bool compStory = sc->hook == dsl::HookKind::Companion;
      auto comeBack = [&]() {
        if (!compStory) return;
        const int32_t nx = g.world.ox + (int32_t)std::floor(g.pl().p.x / TILE), ny = g.world.oy + (int32_t)std::floor(g.pl().p.y / TILE);
        if ((int64_t)(nx - comeX) * (nx - comeX) + (int64_t)(ny - comeY) * (ny - comeY) < 120 * 120) return;
        g.teleportGlobal(comeX, comeY);
        steps(g, 30);
      };
      const uint32_t inst = startAnywhere(g, id, needsOf(s), why);
      castMs += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - c1).count();
      if (!inst) { out("FAIL: saga %s cannot be cast: %s\n", id.c_str(), why.c_str()); seedBad++; restore(g, s0); dropCompanions(g); comeBack(); continue; }
      cast++;
      sc = script(id);   // (the game ran while the hook was sought: the cache may have moved)
      if (!sc) { out("FAIL: saga %s: lost from the cache while it runs\n", id.c_str()); seedBad++; restore(g, s0); continue; }
      pairs.insert(s.arch + "|" + s.twist[0] + "|" + s.twist[1] + "|" + s.twist[2]);
      {
        std::string shape;
        for (const story::Binding& b : g.story.find(inst)->cast) { shape += std::to_string(b.kind) + "." + std::to_string(b.trade) + ","; }
        triples.insert(s.arch + "|" + s.twist[0] + "|" + s.twist[1] + "|" + s.twist[2] + "|" + shape);
      }
      seedBad += checkCast(g, *g.story.find(inst), *sc, id);
      const int D = story::storyDanger(g, *g.story.find(inst));
      if (sample > 0 && (int)(stories - 1) % std::max(1, N * (int)(B - A + 1) / sample) == 0) {
        const story::Instance* in = g.story.find(inst);
        printf("--- %s  \"%s\" (giver %s)\n", id.c_str(), sc->title.c_str(), in && !in->cast.empty() ? in->cast[0].name.c_str() : "?");
      }
      auto c2t = std::chrono::steady_clock::now();
      Walk w;
      std::map<std::string, int> onPath;
      TalkPath tp;
      {
        // (M7b) the cast's names the conversations may rely on only once introduced, and the hook's own turn: the
        // teller's line instead of a greeting and the pitch, chosen (storyTalk); the opening stage goes on from it
        g_castNames.clear();
        g_placeNames.clear();
        g_allPlaces.clear();
        g_ambiguous.clear();
        g_nameRole.clear();
        const story::Instance* in0 = g.story.find(inst);
        for (size_t ri = 0; in0 && ri < in0->cast.size() && ri < sc->roles.size(); ri++) {
          const dsl::RoleKind k = sc->roles[ri].kind;
          const std::string& nm = in0->cast[ri].name;
          if (nm.empty()) continue;
          if (nm.size() >= 3) g_nameRole.push_back({nm, "{" + sc->roles[ri].name + "}"});
          if (k == dsl::RoleKind::Giver && sc->hook != dsl::HookKind::Npc && sc->hook != dsl::HookKind::Herald) continue;
          const bool person = k == dsl::RoleKind::Giver || k == dsl::RoleKind::Person || k == dsl::RoleKind::Foe || k == dsl::RoleKind::Npc ||
                              k == dsl::RoleKind::Resident || k == dsl::RoleKind::Beast || k == dsl::RoleKind::Boss;
          const bool place = k == dsl::RoleKind::Site || k == dsl::RoleKind::Ruin || k == dsl::RoleKind::Capital;
          // (an npc bound by trade, "THE SMITH", is no proper name: nobody introduces the smith)
          if (person && nm.compare(0, 4, "THE ") != 0 && std::find(g_castNames.begin(), g_castNames.end(), nm) == g_castNames.end()) g_castNames.push_back(nm);
          if (place || k == dsl::RoleKind::Site) g_allPlaces.push_back(nm);
          if (place && sc->roles[ri].name != "home" && std::find(g_placeNames.begin(), g_placeNames.end(), nm) == g_placeNames.end()) g_placeNames.push_back(nm);
        }
        const char* hookW = sc->hook == dsl::HookKind::Npc ? "npc" : sc->hook == dsl::HookKind::Board ? "board" : sc->hook == dsl::HookKind::Herald ? "herald"
                            : sc->hook == dsl::HookKind::Ruin ? "ruin" : sc->hook == dsl::HookKind::Companion ? "companion" : "event";
        g_talk.reviewKey = s.arch + "|" + hookW + "|" + motiveName(s.motive);
        g_pitch = in0 ? story::fillText(g, *in0, sc->pitch) : std::string();
        if (in0 && sc->hook == dsl::HookKind::Npc && !in0->cast.empty() && !sc->hint.empty()) {
          speech::Turn T;
          T.speaker = in0->cast[0].name;
          T.text = g.story.hookLine(g, *in0, *sc, true);
          T.opts.push_back(story::fillText(g, *in0, sc->pitch));
          T.opts.push_back("FAREWELL.");
          T.chosen = 0;
          tp.conv.who = T.speaker;
          tp.conv.role = "giver";
          tp.role = "giver";
          tp.conv.turns.push_back(T);
          tp.met.insert("giver");
        }
      }
      {   // (a name the world gave twice, or that is a word of another bound name, "THE KINGDOM OF ARN" and a tinker ARN:
          //  which one a line means cannot be told, so it is not judged)
        const story::Instance* in1 = g.story.find(inst);
        std::vector<std::string> keep;
        for (const std::string& n : g_castNames) {
          int uses = 0;
          std::vector<std::string> others;
          for (size_t ri = 0; in1 && ri < in1->cast.size() && ri < sc->roles.size(); ri++) {
            others.push_back(in1->cast[ri].name);
            if (sc->roles[ri].kind == dsl::RoleKind::Ruin)   // (a ruin's old name and its last lord are names too)
              for (const char* f : {".old}", ".lord}", ".builder}"}) others.push_back(story::fillText(g, *in1, "{" + sc->roles[ri].name + f));
          }
          // (a name that is also a word the story's own text uses, a person called OLD, is not judged either)
          {
            std::string raw = sc->hint + " ";
            for (const dsl::StageDef& G : sc->stages) raw += G.say + " ";
            if (introducedHere(raw, n) >= 0) uses += 2;
          }
          for (const std::string& other : others) {
            if (other == n) { uses++; continue; }
            const size_t p = other.find(n);
            if (p != std::string::npos && (p == 0 || other[p - 1] == ' ') && (p + n.size() == other.size() || other[p + n.size()] == ' ' || other[p + n.size()] == '\'')) uses += 2;
          }
          if (uses <= 1) keep.push_back(n);
          else g_ambiguous.insert(n);
        }
        g_castNames = keep;
      }
      std::sort(g_nameRole.begin(), g_nameRole.end(), [](const std::pair<std::string, std::string>& a, const std::pair<std::string, std::string>& b) {
        return a.first.size() != b.first.size() ? a.first.size() > b.first.size() : a.first < b.first;
      });
      walk(g, inst, *sc, id, w, onPath, 0, realmToo, D, tp);
      g_talk.seenConv.clear();
      walkMs += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - c2t).count();
      nodes += w.nodes;
      ends += w.ends;
      seedBad += w.bad;
      if (w.maxSay > maxSay) { maxSay = w.maxSay; maxSayText = w.maxSayText; }
      over300 += w.over300;
      for (const dsl::StageDef& G : sc->stages)
        if (G.kind == dsl::StageKind::End && !w.endNames.count(G.name)) { out("FAIL: saga %s: no path reaches the ending '%s'\n", id.c_str(), G.name.c_str()); seedBad++; }
      if (g_sverbose) {
        int priests = 0;
        for (const Actor& pa : g.actors) if (pa.npc && pa.human && pa.role == Role::Priest && story::homeSiteOf(g, pa) == g.world.startSite) priests++;
        out("saga seed %llu: %s cast, %d states, %d endings reached (%d priests at home, %zu members)\n", (unsigned long long)seed, id.c_str(), w.nodes,
            (int)w.endNames.size(), priests, g.party.members.size());
        fflush(stdout);
      }
      restore(g, s0);
      g.story.drop(inst);
      dropCompanions(g);
      comeBack();
      trimCache(g.story);
    }
    printf("sagas seed %llu: %d stories, %d failures\n", (unsigned long long)seed, N, seedBad);
    bad += seedBad;
    delete gp;
  }
  if (play) bad += playWalk(A, 30);
  const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
  const double per1000 = stories ? 1000.0 / stories : 0;
  printf("sagas: %d composed, %d cast, %d story states walked, %d endings reached, %.0f ms (%.2f ms a story: compose %.2f, cast %.2f, walk %.2f)\n",
         stories, cast, nodes, ends, ms, stories ? ms / stories : 0.0, stories ? composeMs / stories : 0.0, stories ? castMs / stories : 0.0,
         stories ? walkMs / stories : 0.0);
  const double pairsK = pairs.size() * per1000, textsK = texts.size() * per1000;
  printf("  uniqueness: %.0f distinct (archetype, twists) pairs, %.0f (archetype, twists, cast shape) triples and %.0f distinct texts per "
         "1,000 stories; phrase-family reuse %.1f %%\n",
         pairsK, triples.size() * per1000, textsK, phraseUses ? 100.0 * phraseRepeats / phraseUses : 0.0);
  printf("  the longest line said: %zu characters (%zu lines over 300)%s%s\n", maxSay, over300, g_sverbose ? ": " : "", g_sverbose ? maxSayText.c_str() : "");
  const LibraryReport lr = report();
  if (lr.tier2 >= 60 && stories >= 1000) {
    if (textsK < 900) { printf("FAIL: sagas: %.0f distinct texts per 1,000 stories (the bar: 900)\n", textsK); bad++; }
    if (pairsK < 300) { printf("FAIL: sagas: %.0f distinct (archetype, twists) pairs per 1,000 stories (the bar: 300)\n", pairsK); bad++; }
  } else {
    printf("  (uniqueness not gated yet: %d story archetypes of the 60 that make the library full, %d stories)\n", lr.tier2, stories);
  }
  {   // (M7b) the conversations' coherence
    const long C = std::max(1L, g_talk.convs);
    printf("sagas talk: %ld conversations (%ld turns) linted; %ld answers checked; %ld on paths only the walker takes (an option the "
           "dialogue hides) not linted\n", g_talk.convs, g_talk.turns, g_talk.answers, g_talk.forcedConvs);
    const uint32_t order[] = {speech::C_EMPTY, speech::C_PLACEHOLDER, speech::C_CASE, speech::C_PANEL, speech::C_OPTION, speech::C_REPEAT, speech::C_DOUBLE,
                              speech::C_STITCH, speech::C_FRAGMENT, speech::C_NAME, speech::C_ECHO, speech::C_PUNCT, speech::C_SELF, C_ANSWER};
    for (uint32_t c : order) {
      const long n = g_talk.byCheck.count(c) ? g_talk.byCheck[c] : 0;
      printf("  %-46s %6ld conversations (%5.2f %%)\n", c == C_ANSWER ? "answer does not take up the option" : speech::checkName(c), n, 100.0 * n / C);
      if (talkExamples > 0 && g_talk.examples.count(c))
        for (size_t k = 0; k < g_talk.examples[c].size() && (int)k < talkExamples; k++) printf("      e.g. %s\n", g_talk.examples[c][k].c_str());
    }
    printf("  (raw lint: %ld name findings, %ld doubled / punctuation findings, before the closer reading above)\n", g_talk.rawName, g_talk.rawDouble);
    printf("  %-46s %6ld conversations (%5.2f %%) [soft]\n", "(speech::answers) answer does not follow", g_talk.talkAnswer, 100.0 * g_talk.talkAnswer / C);
    printf("  %-46s %6ld conversations (%5.2f %%) [soft]\n", "a place named but never placed", g_talk.placeIssues,
           100.0 * g_talk.placeIssues / std::max(1L, g_talk.placeConvs));
    if (talkExamples > 0)
      for (size_t k = 0; k < g_talk.placeExamples.size() && (int)k < talkExamples; k++) printf("      e.g. %s\n", g_talk.placeExamples[k].c_str());
    // the gates (15.23): nothing unfilled, nothing off the panel, every answer a reply to its option, nobody naming
    // themselves as "him", every person introduced before they are relied on, no phrase twice in one conversation
    {
      const uint32_t hard[] = {speech::C_EMPTY, speech::C_PLACEHOLDER, speech::C_CASE, speech::C_PANEL, speech::C_OPTION, speech::C_ECHO, speech::C_SELF,
                               speech::C_NAME, speech::C_REPEAT, C_ANSWER};
      for (uint32_t c : hard) {
        const long n = g_talk.byCheck.count(c) ? g_talk.byCheck[c] : 0;
        if (!n) continue;
        printf("FAIL: sagas talk: %ld conversations with %s\n", n, c == C_ANSWER ? "an answer that does not take up the option" : speech::checkName(c));
        for (size_t k = 0; g_talk.examples.count(c) && k < g_talk.examples[c].size() && k < 5; k++) printf("      e.g. %s\n", g_talk.examples[c][k].c_str());
        bad++;
      }
      // the soft ones are reported (stitched sentences, one-word fragments, broken punctuation, unfinished sentences)
      long soft = 0;
      for (uint32_t c : {speech::C_STITCH, speech::C_FRAGMENT, speech::C_DOUBLE, speech::C_PUNCT}) soft += g_talk.byCheck.count(c) ? g_talk.byCheck[c] : 0;
      printf("sagas talk: %ld conversations with a soft finding (stitched / fragment / punctuation)\n", soft);
    }
    if (!talkReport.empty()) {
      std::vector<std::pair<long, std::string>> v;
      for (const auto& kv : g_talk.sites) v.push_back({kv.second, kv.first});
      std::sort(v.begin(), v.end(), [](const std::pair<long, std::string>& a, const std::pair<long, std::string>& b) { return a.first != b.first ? a.first > b.first : a.second < b.second; });
      if (FILE* f = fopen(talkReport.c_str(), "w")) {
        for (const auto& e : v) fprintf(f, "%6ld  %s\n", e.first, e.second.c_str());
        fclose(f);
      }
    }
    if (!review.empty()) {
      FILE* f = fopen(review.c_str(), "w");
      if (f) { fputs(g_talk.review.c_str(), f); fclose(f); printf("sagas: wrote %d conversations for review to %s\n", g_talk.reviewed, review.c_str()); }
      else { printf("FAIL: sagas: cannot write %s\n", review.c_str()); bad++; }
    }
  }
  printf("sagas: %d failures\n", bad);
  return bad ? 1 : 0;
}

// ---------------------------------------------------------------- the writing-quality review lens
void printStory(Game& g, uint32_t inst, const dsl::Script& sc, const std::string& id) {
  const story::Instance* in = g.story.find(inst);
  if (!in) return;
  printf("=====================================================================================================\n");
  printf("%s\n\"%s\"  (archetype: %s)\n", id.c_str(), sc.title.c_str(), sc.archetype.c_str());
  printf("PITCH: %s\n", sc.pitch.c_str());
  if (!sc.hint.empty()) printf("HINT: %s\n", sc.hint.c_str());
  printf("CAST:");
  for (size_t ri = 0; ri < in->cast.size() && ri < sc.roles.size(); ri++) {
    const story::Binding& b = in->cast[ri];
    std::string extra;
    if (sc.roles[ri].kind == dsl::RoleKind::Resident) extra = " [" + story::fillText(g, *in, "{" + b.role + ".job}") + ", " + story::fillText(g, *in, "{" + b.role + ".kin}") + "]";
    printf(" %s=%s%s;", b.role.c_str(), b.name.c_str(), extra.c_str());
  }
  printf("\n");
  for (const dsl::StageDef& G : sc.stages) {
    std::string kind;
    if (G.kind == dsl::StageKind::Talk) kind = "talk " + G.talk;
    else if (G.kind == dsl::StageKind::Goal) kind = "goal -> " + G.then;
    else kind = G.success ? "END (success)" : "END (failure)";
    printf("  [%s] %s\n", G.name.c_str(), kind.c_str());
    if (!G.say.empty()) printf("      SAY: %s\n", story::fillText(g, *in, G.say).c_str());
    if (!G.journal.empty()) printf("      JOURNAL: %s\n", story::fillText(g, *in, G.journal).c_str());
    for (const dsl::OptDef& o : G.opts)
      printf("      > %s  -> %s%s\n", story::fillText(g, *in, o.label).c_str(), o.to.c_str(), o.check ? (" / else " + o.orElse).c_str() : "");
    for (const dsl::EffDef& e : G.effs) {
      if (e.type == dsl::EffType::Remember) printf("      (%s remembers: %s)\n", e.args[0].c_str(), story::fillText(g, *in, e.text).c_str());
      else if (e.type == dsl::EffType::Fact) printf("      (the town says: %s)\n", story::fillText(g, *in, e.text).c_str());
      else if (e.type == dsl::EffType::Reward) printf("      (reward %s)\n", e.n == 0 ? "small" : e.n == 1 ? "fair" : e.n == 2 ? "rich" : "great");
      else if (e.type == dsl::EffType::Give) printf("      (gives: %s)\n", story::fillText(g, *in, e.text).c_str());
      else if (e.type == dsl::EffType::Secret) printf("      (a smithing secret of %s)\n", e.args[0].c_str());
    }
  }
}

int sagaReadCmd(int argc, char** argv) {
  uint64_t A = 1, B = 1;
  int N = 10;
  for (int i = 2; i < argc; i++) {
    if (!strcmp(argv[i], "--seeds") && i + 1 < argc) parseSeedRange(argv[++i], A, B);
    else if (argv[i][0] >= '0' && argv[i][0] <= '9') N = atoi(argv[i]);
  }
  std::vector<const Archetype*> arch;
  for (const Archetype& a : archetypes()) if (a.tier == 2) arch.push_back(&a);
  if (arch.empty()) { printf("no story archetypes\n"); return 1; }
  int printed = 0, failed = 0;
  for (uint64_t seed = A; seed <= B && printed < N; seed++) {
    Game* gp = freshWorld(seed);
    Game& g = *gp;
    const int per = (int)((N - printed + (int)(B - seed)) / (int)(B - seed + 1));
    for (int i = 0; i < per && printed < N; i++) {
      // a random story of the library (not the archetypes in order: the review samples the whole library)
      const uint64_t r = ew::mix64(seed * 7919u + (uint64_t)i * 104729u);
      const Spec s = specFor(seed ^ 0xBEEF, (int)(r % 1000003u), arch);
      const std::string id = specId(s);
      const dsl::Script* sc = script(id);
      if (!sc) { printf("(%s does not compose)\n", id.c_str()); failed++; continue; }
      const Snap s0 = snap(g, true);
      std::string why;
      const uint32_t inst = startAnywhere(g, id, needsOf(s), why);
      if (!inst) { printf("(%s cannot be cast: %s)\n", id.c_str(), why.c_str()); failed++; restore(g, s0); continue; }
      sc = script(id);
      if (sc) printStory(g, inst, *sc, id);
      printed++;
      restore(g, s0);
      g.story.drop(inst);
    }
    delete gp;
  }
  printf("saga-read: %d stories printed, %d could not be told\n", printed, failed);
  return failed ? 1 : 0;
}

}  // namespace

RPG_TEST_CMD("--sagas", "M6b generated stories: compose N per seed on real worlds, cast them, walk every path; rewards, the guard, uniqueness [N] [--seeds A..B] [--sample K] [--no-play] [--verbose]", sagasCmd);
RPG_TEST_CMD("--saga-read", "M6b writing review lens: print N composed stories in full, cast on real worlds [N] [--seeds A..B]", sagaReadCmd);
