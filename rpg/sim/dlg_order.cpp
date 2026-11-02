// EMBERVALE RPG: one order of dialogue options (P4 item 4, TALK lane; talk.h section 6).
// Public: speech::optRow, speech::optStanding, speech::optQuest, speech::optKindName, speech::orderFirstPage,
//   speech::startCursor
//
// Before P4 the first page listed the options in the order talkTo's hooks ran, and grouped the services under WHAT CAN
// YOU OFFER? only when the page would hold more than FIRST_PAGE rows: any passing option (bread in the pack, a job on
// offer, a reward due) tipped it, so the same speaker's options moved between visits and a number picked the wrong one.
// Now every option has a kind from ONE table (kOrder, rows by action range or value). A kind is STANDING (the speaker's
// trade, offered on every visit) or PASSING (this visit only). The first page lists the standing kinds in table order,
// then the passing ones, then FAREWELL; the headers are decided by the speaker's standing kinds alone, so a passing
// option never regroups the page.
#include <algorithm>
#include "rpg/sim/game_internal.h"
#include "rpg/sim/life_ops.h"
#include "rpg/sim/skill_ops.h"
#include "rpg/sim/talk.h"

using namespace gsim;

namespace {

// how a kind sits on the first page
enum : uint8_t {
  K_SERVICE = 1,   // standing: one of the services (grouped under WHAT CAN YOU OFFER? when the speaker has many)
  K_HOUSE = 2,     // standing service: the house and kitchen business (its own header for a keeper with much to offer)
  K_STANDING = 4,  // standing: the speaker's trade, offered on every visit
  K_QUEST = 8,     // passing: a quest step the player came for (the keyboard cursor starts on the first of these)
  K_KIND = 16,     // passing: a kindness (under HOW ARE YOU KEEPING? when the speaker has it)
  K_BEASTS = 32,   // passing: the beasts' word (with the inn's news when the services are grouped)
  K_CHAT = 64,     // standing: HOW ARE YOU KEEPING? (the kindnesses' header)
};
struct Row {
  int lo, hi;        // the actions [lo, hi) of this kind
  uint8_t flags;
  const char* name;  // for tests and dumps
};
constexpr uint8_t SV = K_SERVICE | K_STANDING;
// the one table, in the order of the first page and of every group's page. An option takes the NARROWEST row whose
// action range holds it (so a single action inside a lane's range, like the sea's fish order, has its own row).
// The passing rows run from what lasts (the news, the war, the beasts' word, the main quest, a story) to what this
// visit alone brings (a job on offer, a kindness, a parcel, COLLECT just above FAREWELL), so the options that come back
// keep their numbers when a visit adds one.
constexpr Row kOrder[] = {
    // ---- standing: the speaker's trade
    {A_TRADE, A_TRADE + 1, SV, "wares"},
    {A_REST, A_REST + 1, SV, "room"},
    {A_RUMOR, A_RUMOR + 1, SV, "rumour"},
    {LifeOps::L_MEAL, LifeOps::L_MEAL + 1, SV, "meal"},
    {A_HEAL, A_HEAL + 1, SV, "blessing"},
    {A_LEARN, A_LEARN + 1, SV, "lesson"},
    {DLG_CRAFT, DLG_M6_END, SV, "forge"},
    {DLG_SKILLS, DLG_SKILLS_END, SV, "training"},
    {DLG_MAGIC, DLG_MAGIC_END, SV, "magic"},
    {DLG_HOME, DLG_HOME_END, SV | K_HOUSE, "house"},
    {DLG_SEA, DLG_SEA_END, SV, "sea"},
    {DLG_SERVICE, DLG_SERVICE_END, K_STANDING, "army"},
    {DLG_LORD, DLG_LORD_END, K_STANDING, "lordship"},
    {LifeOps::L_CHAT, LifeOps::L_CHAT + 1, K_STANDING | K_CHAT, "how-are-you"},
    // ---- passing: this visit only (what lasts first, what this visit alone brings last)
    {A_NEWS, A_NEWS + 1, 0, "news"},
    {DLG_WAR, DLG_END, 0, "war"},
    {DLG_FOES, DLG_CRAFT, K_BEASTS, "beasts"},
    {A_MAIN, A_MAIN + 1, K_QUEST, "main-quest"},
    {DLG_STORY, DLG_STORY + story::SA_START, K_QUEST, "story-scene"},
    {DLG_STORY + story::SA_START, DLG_WAR, 0, "story-pitch"},   // (a pitch gives way when the speaker has other business)
    {DLG_PARTY, DLG_PARTY_END, 0, "companion"},
    {SkillOps::DLG_COMPANION, SkillOps::DLG_COMPANION + 1, 0, "companion-teach"},
    {DLG_LIFE, DLG_LIFE_END, 0, "life"},
    {A_ESCORT, A_ESCORT + 1, 0, "escort"},
    {DLG_SEA + 95, DLG_SEA + 96, K_QUEST, "mist-word"},          // (sea_trade.cpp K_WORD: the word of a land told to a king)
    {DLG_SERVICE + 5, DLG_SERVICE + 6, K_QUEST, "envoy-word"},   // (service_game.cpp K_DELIVER: a court's message)
    {DLG_SEA + 90, DLG_SEA + 91, 0, "fish-order"},               // (sea_trade.cpp K_FISHJOB: a fishmonger's order)
    {LifeOps::L_SUPPLY, LifeOps::L_SUPPLY + 1, 0, "supply"},
    {A_ASK, A_ASK + 1, 0, "job"},
    {LifeOps::L_FEED, LifeOps::L_EMPLOY + 1, K_KIND, "kindness"},
    {A_DELIVER, A_DELIVER + 1, K_QUEST, "parcel"},
    {A_TURNIN, A_TURNIN + 1, K_QUEST, "collect"},
};
constexpr int kRows = (int)(sizeof(kOrder) / sizeof(kOrder[0]));
// ranks: a row's index twice over; an action no row names sits among the passing kinds, just before the escort (with
// what lasts, never between COLLECT and FAREWELL); the headers lead; FAREWELL (and an answer's own leave-taking) is last
int rowNamed(const char* n) {
  for (int i = 0; i < kRows; i++) if (std::string(kOrder[i].name) == n) return i;
  return kRows;
}
constexpr int RANK_HEADER = -1;
constexpr int RANK_BYE = 2 * kRows + 1;

int rowOf(int action) {
  int best = -1;
  for (int i = 0; i < kRows; i++)
    if (action >= kOrder[i].lo && action < kOrder[i].hi && (best < 0 || kOrder[i].hi - kOrder[i].lo < kOrder[best].hi - kOrder[best].lo)) best = i;
  return best;
}
uint8_t flagsOf(int action) { const int r = rowOf(action); return r >= 0 ? kOrder[r].flags : 0; }
int rankOf(const DlgOpt& o) {
  if (o.action == A_BYE) return RANK_BYE;
  if (o.action == A_GROUP) return RANK_HEADER;
  static const int other = 2 * rowNamed("escort") - 1;
  const int r = rowOf(o.action);
  return r >= 0 ? 2 * r : other;
}

// a speaker whose standing options, FAREWELL and this many passing rows would overflow the first page groups the
// services (1: at 5 standing options or more; 2 grouped too many small trades, 0 left 5 % of the sampled talks paging)
constexpr int PASSING_ROOM = 1;

}  // namespace

namespace speech {

int optRow(int action) { return rankOf(DlgOpt{std::string(), action, 0, 0}); }
bool optStanding(int action) { return (flagsOf(action) & K_STANDING) != 0 || action == A_GROUP; }
bool optQuest(int action) { return (flagsOf(action) & K_QUEST) != 0; }
const char* optKindName(int action) {
  if (action == A_BYE) return "farewell";
  if (action == A_GROUP) return "header";
  const int r = rowOf(action);
  return r >= 0 ? kOrder[r].name : "other";
}

void orderFirstPage(Dialogue& d) {
  // 1. one order: the table's (a stable sort, so options of one kind keep the order their hook gave them)
  std::stable_sort(d.opts.begin(), d.opts.end(), [](const DlgOpt& a, const DlgOpt& b) { return rankOf(a) < rankOf(b); });
  // 2. the headers, by the speaker's standing kinds alone (never by how many passing options this visit brings)
  int standing = 0, serv = 0, house = 0;
  bool chat = false;
  for (const DlgOpt& o : d.opts) {
    if (o.grp != 0) continue;
    const uint8_t f = flagsOf(o.action);
    if (f & K_STANDING) standing++;
    if (f & K_SERVICE) serv++;
    if (f & K_HOUSE) house++;
    if (f & K_CHAT) chat = true;
  }
  int at = 0;   // where the next header goes (the headers lead the page, in the order of their groups' first rows)
  if (serv >= 2 && standing + 1 + PASSING_ROOM > FIRST_PAGE) {
    for (DlgOpt& o : d.opts) if (o.grp == 0 && (flagsOf(o.action) & K_SERVICE)) o.grp = 1;
    d.opts.insert(d.opts.begin() + at++, DlgOpt{"WHAT CAN YOU OFFER?", A_GROUP, 1, 0});
    // a keeper with more to offer than one page holds: the house and kitchen business gets its own header
    if (serv > FIRST_PAGE - 1 && house >= 2 && serv - house >= 2) {
      for (DlgOpt& o : d.opts) if (o.grp == 1 && (flagsOf(o.action) & K_HOUSE)) o.grp = 3;
      d.opts.insert(d.opts.begin() + at++, DlgOpt{"WHAT ELSE DO YOU DO HERE?", A_GROUP, 3, 0});
    }
    // the keeper's word of the beasts about goes with the inn's news
    for (DlgOpt& o : d.opts) if (o.grp == 0 && (flagsOf(o.action) & K_BEASTS)) o.grp = 1;
  }
  // 3. the kindnesses come up when the player asks how the speaker is keeping (L_CHAT shows them: lifeChoose)
  if (chat)
    for (DlgOpt& o : d.opts) if (o.grp == 0 && (flagsOf(o.action) & K_KIND)) o.grp = 2;
}

int startCursor(const Dialogue& d) {
  int k = 0;
  for (const DlgOpt& o : d.opts) {
    if (!d.shows(o)) continue;
    if (d.group == 0 && optQuest(o.action)) return k;
    k++;
  }
  return 0;
}

}  // namespace speech
