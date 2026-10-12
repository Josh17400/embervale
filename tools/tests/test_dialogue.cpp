// rpg_test --dialogue [--seeds A..B] [--dump FILE] [--sample N] [--lenient] [--max-people N]: the M7b dialogue review
// (VISION_PLAN 15.23, 15.24). TALK lane file.
//
// Real conversations, recorded as the player sees them (speech::Recorder on Game::talkRec: the options on show, an
// option group opened as the player opens it): per seed the start village, the nearest town and the nearest city; in
// each, the people in the streets and the people inside its inn, shop, smithy, temple and seat of power. Every person is
// talked to once per option of their opening (each path a fresh conversation: open, choose that option, and where the
// answer offers more, choose its first real option; an option group's header is opened and each of its options
// followed), so every answer is read against the option that asked for it. Options that would leave the scene (the
// shop screen, a bed, a journey, a theft) are recorded but not followed. Then the states a conversation must also read
// well in: the jobs taken there handed in (the givers met a second time), the village at night, the village in famine
// and its kingdom at war.
//
// Every conversation is linted (speech::lint, every check: placeholders, case, the phone panel, repeated phrases,
// doubled words, stitched fragments, one-word fragments, names before their introduction, answers that ignore or do
// not follow the choice, unfinished sentences, speakers naming themselves) and reviewed as a player reads it (the
// dialogue-quality lens: a crowded first page, the same opening line from several people of one place, a quest
// giver reading the journal aloud). --dump FILE writes every conversation in the phone panel's wrap with its issues
// (the reviewer reads these); --sample N prints N of them. Gate: every issue fails (--lenient: only placeholder, case
// and empty lines, the phase A gate).
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
#include "rpg/sim/game.h"
#include "rpg/sim/game_internal.h"
#include "rpg/sim/life.h"
#include "rpg/sim/party.h"
#include "rpg/sim/party_voice.h"
#include "rpg/sim/talk.h"
#include "tools/tests/tests.h"

namespace {

// the review's own checks (beyond speech::lint), numbered past its bits
constexpr uint32_t R_PAGE = 1u << 20;     // more than speech::FIRST_PAGE options on a first page
constexpr uint32_t R_CHORUS = 1u << 21;   // the same opening line from three or more people of one place
constexpr uint32_t R_JOURNAL = 1u << 22;  // a quest's journal text spoken (q.desc names the giver in the third person)
const char* reviewName(uint32_t c) {
  switch (c) {
    case R_PAGE: return "crowded first page";
    case R_CHORUS: return "same opening line from many people";
    case R_JOURNAL: return "journal text spoken aloud";
    default: return speech::checkName(c);
  }
}

bool leaves(const std::string& label) {
  static const char* const k[] = {"WARES", "RENT A ROOM", "TO BED", "ABOUT MY ROOM", "TRAVEL", "RIDE TO", "CARRIAGE", "STEAL", "COPY THE",
                                  "BUY A HORSE", "SHOW ME", "BUILD", "DECORATE", "COOK", "WORK THE", "SMELT", "TAN ", "FORGE"};
  for (const char* s : k) if (label.find(s) != std::string::npos) return true;
  return false;
}
bool farewell(const std::string& label) {
  return label == "FAREWELL." || label == "LATER." || label == "NOT RIGHT NOW." || label == "GOODBYE." || label == "NEVER MIND." ||
         label == "NOT TODAY." || label == "SOMETHING ELSE.";
}

int findById(const Game& g, int id) {
  for (size_t k = 1; k < g.actors.size(); k++) if (g.actors[k].id == id) return (int)k;
  return -1;
}

// the options the panel shows now, as indices into dlg.opts
std::vector<int> shown(const Game& g) {
  std::vector<int> v;
  for (size_t i = 0; i < g.dlg.opts.size(); i++) if (g.dlg.shows(g.dlg.opts[i])) v.push_back((int)i);
  return v;
}
int shownByLabel(const Game& g, const std::string& label) {
  for (int i : shown(g)) if (g.dlg.opts[(size_t)i].label == label) return i;
  return -1;
}

// the conversations the story engine speaks in (a running story's scene, a story's pitch or hint, a story option
// followed): the SAGA lane's lines (rpg/story/**), reviewed apart (--saga-strict fails on them too)
std::set<size_t> g_storyConvs;
bool storyOpt(const DlgOpt& o) { return o.action >= DLG_STORY && o.action < DLG_WAR; }
void markStory(Game& g) { if (g.talkRec && !g.talkRec->convs.empty()) g_storyConvs.insert(g.talkRec->convs.size() - 1); }
void choose(Game& g, int k) {
  if (k >= 0 && k < (int)g.dlg.opts.size() && storyOpt(g.dlg.opts[(size_t)k])) markStory(g);
  g.dialogueChoose(k);
}

bool open(Game& g, int id) {
  const int ai = findById(g, id);
  if (ai < 0) return false;
  HomeOps::talkTo(g, g.actors[(size_t)ai]);
  if (g.mode != Mode::Dialogue) return false;
  bool story = g.dlg.topic == (uint8_t)speech::Topic::Story;
  for (int k : shown(g)) if (storyOpt(g.dlg.opts[(size_t)k])) story = true;   // (a story's pitch: its hint is the line)
  if (story) markStory(g);
  return true;
}

void after(Game& g) {
  g.mode = Mode::Play;
  for (int f = 0; f < 3; f++) { g.update(SIM_DT, Input()); g.events.clear(); }
  g.mode = Mode::Play;
}

// one step further where the answer goes on: its first real option
void stepOn(Game& g) {
  if (g.mode != Mode::Dialogue) return;
  for (int k : shown(g)) {
    const std::string& l = g.dlg.opts[(size_t)k].label;
    if (!farewell(l) && !leaves(l)) { choose(g, k); return; }
  }
}

// every path through one person's conversation (each option of the opening, one step further where it goes on; an
// option group's header opened and each of its options taken)
void talkPaths(Game& g, int id, int& convs) {
  if (!open(g, id)) return;
  convs++;
  std::vector<DlgOpt> opening;
  for (int k : shown(g)) opening.push_back(g.dlg.opts[(size_t)k]);
  g.mode = Mode::Play;
  for (size_t o = 0; o < opening.size() && o < 9; o++) {
    if (farewell(opening[o].label)) continue;
    if (!open(g, id)) return;
    convs++;
    const int pick = shownByLabel(g, opening[o].label);
    if (pick < 0) { g.mode = Mode::Play; continue; }
    if (leaves(opening[o].label)) { choose(g, pick); after(g); continue; }
    const bool group = g.dlg.opts[(size_t)pick].action == A_GROUP;
    choose(g, pick);
    if (!group) { stepOn(g); after(g); continue; }
    // the group's options, each its own path (open, the header, the option, a step on)
    std::vector<std::string> inner;
    for (int k : shown(g)) if (!farewell(g.dlg.opts[(size_t)k].label)) inner.push_back(g.dlg.opts[(size_t)k].label);
    after(g);
    for (size_t i = 0; i < inner.size() && i < 8; i++) {
      if (!open(g, id)) return;
      convs++;
      const int h = shownByLabel(g, opening[o].label);
      if (h < 0) { g.mode = Mode::Play; continue; }
      choose(g, h);
      const int k = shownByLabel(g, inner[i]);
      if (k >= 0) {
        choose(g, k);
        if (!leaves(inner[i])) stepOn(g);
      }
      after(g);
    }
  }
}

// the people of a site within the window (outdoors) or in the current interior
std::vector<int> peopleHere(const Game& g, int site, size_t cap) {
  std::vector<int> ids;
  for (size_t k = 1; k < g.actors.size() && ids.size() < cap; k++) {
    const Actor& a = g.actors[k];
    if (!a.npc || !a.human || a.hostile || a.st == AState::Dead) continue;
    if (site >= 0 && !g.inside && a.site != site) continue;
    ids.push_back(a.id);
  }
  return ids;
}

void settle(Game& g, int steps) {
  for (int f = 0; f < steps; f++) { g.update(SIM_DT, Input()); g.events.clear(); if (g.mode != Mode::Play) g.mode = Mode::Play; }
}

// the jobs these people gave, done (the ones that are won in the field), and each giver met again: the turn-in
void handIn(Game& g, const std::vector<int>& ids, int& convs) {
  for (int id : ids) {
    const int ai = findById(g, id);
    if (ai < 0) continue;
    bool any = false;
    for (Quest& q : g.quests)
      if (q.state == QState::Active && isGiver(q, g.actors[(size_t)ai]) &&
          (q.type == QType::Hunt || q.type == QType::Clear || q.type == QType::Bounty || q.type == QType::NamedBandit || q.type == QType::Heirloom ||
           q.type == QType::Missing || q.type == QType::Protect)) {
        if (q.type == QType::Hunt) q.have = q.need;
        q.state = QState::Complete;
        any = true;
      }
    if (any) talkPaths(g, id, convs);
  }
}

int sampleSite(Game& g, int si, size_t maxPeople, int& convs) {
  if (si < 0 || si >= (int)g.world.sites.size()) return 0;
  g.world.sites[(size_t)si].discovered = true;
  if (si != g.world.startSite && !g.fastTravel(si)) return 0;
  g.hour = 12;
  settle(g, 90);
  const ew::Gid sid = g.world.sites[(size_t)si].id;
  int people = 0;
  const std::vector<int> street = peopleHere(g, si, maxPeople);
  for (int id : street) { talkPaths(g, id, convs); people++; }
  handIn(g, street, convs);
  // inside: the trades and the seat
  const int sh = g.world.siteHandle(sid);
  if (sh < 0) return people;
  const Site S = g.world.sites[(size_t)sh];
  std::set<int> seenType;
  for (int b = S.bldgFirst; b < S.bldgFirst + S.bldgCount && b < (int)g.world.over.bldgs.size(); b++) {
    const Bldg& B = g.world.over.bldgs[(size_t)b];
    const bool want = B.type == art::Building::Inn || B.type == art::Building::Shop || B.type == art::Building::Smithy ||
                      B.type == art::Building::Temple || bldgIsSeat(B);
    if (!want || seenType.count((int)B.type)) continue;
    seenType.insert((int)B.type);
    if (!g.debugEnterBuilding(b)) continue;
    settle(g, 20);
    const std::vector<int> in = peopleHere(g, -1, 4);
    for (int id : in) { talkPaths(g, id, convs); people++; }
    handIn(g, in, convs);
    g.debugLeave();
    settle(g, 5);
  }
  return people;
}

// the start village again in the states its talk must also read well in: night, famine, its kingdom at war
int sampleStates(Game& g, int& convs) {
  const int si = g.world.startSite;
  if (si < 0 || !g.fastTravel(si)) return 0;
  int people = 0;
  g.hour = 22;
  settle(g, 60);
  for (int id : peopleHere(g, si, 4)) { talkPaths(g, id, convs); people++; }
  g.hour = 12;
  settle(g, 60);
  const Site S = g.world.sites[(size_t)si];
  const int32_t hx = g.world.ox + S.ex, hy = g.world.oy + S.ey;
  const ew::Gid home = S.homeKingdom >= 0 ? g.world.kingdoms[(size_t)S.homeKingdom].id : 0;
  g.realm.noteSite(S.id, home, (uint8_t)S.type, hx, hy);
  g.realm.forceFamine(S.id, g.day);
  if (life::Census* c = g.life.findMut(S.id)) {
    c->moodFlags |= life::MF_FAMINE | life::MF_HUNGRY;
    for (size_t r = 0; r < c->res.size(); r += 3) c->res[r].need[(int)life::Need::Hunger] = 10;
  }
  ew::Gid other = 0;
  for (const realm::KingdomState& K : g.realm.kingdoms()) if (K.id != home && !K.fallen) { other = K.id; break; }
  if (other && home && !g.realm.atWar(other, home)) g.realm.forceWar(other, home, g.day);
  g.realmSync();
  settle(g, 30);
  for (int id : peopleHere(g, si, 6)) { talkPaths(g, id, convs); people++; }
  return people;
}

int nearestOfType(const Game& g, SiteType t) {
  const Site& home = g.world.sites[(size_t)g.world.startSite];
  int best = -1;
  long long bd = 0;
  for (size_t i = 0; i < g.world.sites.size(); i++) {
    const Site& s = g.world.sites[i];
    if (s.type != t || (int)i == g.world.startSite) continue;
    const long long dx = s.ex - home.ex, dy = s.ey - home.ey, d = dx * dx + dy * dy;
    if (best < 0 || d < bd) { best = (int)i; bd = d; }
  }
  return best;
}

// ---------------------------------------------------------------- (M8 VOICE) the companions' talk
// A census companion recruited in the start village and in the nearest town (the recruit flow as a player meets it: a
// friend's offer, the pitch, their story), every page of their own conversation (orders, their story and its
// follow-ups, the deeds they remember, how they are keeping, their gear, what they think of the player, their personal
// quest's offer, a grievance), a member at home asked back, one who left and the amends another day, their kin's word
// while they travel, and after a death: the kin and the innkeeper of their home, and the grave.
int actorOfResident(const Game& g, int site, int idx) {
  for (size_t k = 1; k < g.actors.size(); k++) {
    const Actor& a = g.actors[k];
    if (a.resident == idx && a.site == site && a.npc && a.human && !a.comp && a.st != AState::Dead) return a.id;
  }
  return -1;
}
bool destructive(const std::string& l) {
  return l == "WAIT HERE FOR ME." || l.rfind("GO HOME", 0) == 0 || l.rfind("GO TO MY HOUSE", 0) == 0 || l == "WE PART WAYS HERE." ||
         l == "IF YOU DON'T LIKE IT, GO.";
}
// the first page, each of its options, and each option of the page that opens (one step further where it goes on)
void companionPaths(Game& g, int id, int& convs) {
  if (!open(g, id)) return;
  convs++;
  std::vector<std::string> front;
  for (int k : shown(g)) if (!farewell(g.dlg.opts[(size_t)k].label)) front.push_back(g.dlg.opts[(size_t)k].label);
  g.mode = Mode::Play;
  for (const std::string& f : front) {
    if (!open(g, id)) return;
    convs++;
    const int k = shownByLabel(g, f);
    if (k < 0) { after(g); continue; }
    choose(g, k);
    if (g.mode != Mode::Dialogue) { after(g); continue; }
    std::vector<std::string> inner;
    for (int i : shown(g)) {
      const std::string& l = g.dlg.opts[(size_t)i].label;
      if (!farewell(l) && !destructive(l)) inner.push_back(l);
    }
    // (the story's own choices move the quest on: the first is followed, the rest only shown)
    bool story = false;
    for (int i : shown(g)) if (storyOpt(g.dlg.opts[(size_t)i])) story = true;
    if (story) { stepOn(g); after(g); continue; }
    after(g);
    for (size_t i = 0; i < inner.size() && i < 7; i++) {
      if (!open(g, id)) return;
      convs++;
      const int h = shownByLabel(g, f);
      if (h < 0) { after(g); continue; }
      choose(g, h);
      const int j = shownByLabel(g, inner[i]);
      if (j >= 0) { choose(g, j); stepOn(g); }
      after(g);
    }
  }
}
// someone with the companion's quest offer, a grievance, the orders that change their state (last)
int sampleCompanions(Game& g, int& convs) {
  int people = 0;
  for (int i = (int)g.party.members.size() - 1; i >= 0; i--) g.partyDismiss(i);
  g.party.fallen.clear();
  g.plLevel = std::max(g.plLevel, party::LEADERSHIP_LEVEL);   // (two people may travel with the player)
  const int sites[2] = {g.world.startSite, nearestOfType(g, SiteType::Town)};
  for (int pass = 0; pass < 2; pass++) {
    const int si = sites[pass];
    if (si < 0 || si >= (int)g.world.sites.size() || !g.fastTravel(si)) continue;
    g.hour = 12;
    settle(g, 60);
    const ew::Gid sid = g.world.sites[(size_t)si].id;
    life::Census* C = g.life.census(g.world, si);
    if (!C) continue;
    // ---- a friend in the street: their offer, their pitch, their story; they join
    int who = -1, res = -1;
    for (size_t k = 1; k < g.actors.size() && who < 0; k++) {
      const Actor& a = g.actors[k];
      if (!a.npc || !a.human || a.comp || a.resident < 0 || a.site != si || a.role == Role::Guard || a.role == Role::Child) continue;
      const life::Resident& r = C->res[(size_t)a.resident];
      if ((r.flags & (life::RF_DEAD | life::RF_KEY | life::RF_AWAY)) || r.age < 18 || r.age > 60 || r.job == life::Job::Child) continue;
      who = a.id; res = a.resident;
    }
    if (who < 0) continue;
    g.life.befriend(sid, res);
    const size_t before = g.party.members.size();
    talkPaths(g, who, convs);
    people++;
    int mi = -1;
    for (size_t k = 0; k < g.party.members.size(); k++) if (g.party.members[k].npc == life::npcId(sid, res)) mi = (int)k;
    if (mi < 0 && g.party.members.size() == before) {
      const int ai = findById(g, who);
      if (ai > 0) mi = g.partyRecruit(g.actors[(size_t)ai], party::Origin::Resident);
    }
    if (mi < 0) { out("FAIL: dialogue: seed %llu: no companion recruited in %s\n", (unsigned long long)g_curSeed, g.world.sites[(size_t)si].name.c_str()); continue; }
    settle(g, 5);
    // ---- their own conversation: plain, then with memories, a quest to offer, a grievance
    auto comp = [&]() { return mi < (int)g.party.members.size() ? g.party.members[(size_t)mi].actor : -1; };
    companionPaths(g, comp(), convs);
    g.deed(party::Deed::Mercy, sid);
    g.deed(party::Deed::ClearedBandits, sid);
    g.deed(party::Deed::Stole, sid);
    settle(g, 3);
    companionPaths(g, comp(), convs);
    {
      party::Member& m = g.party.members[(size_t)mi];
      m.approval = 60; m.bond = 40;
      if (m.stake.hook == party::Hook::None) m.stake.hook = party::Hook::Ambition;
    }
    companionPaths(g, comp(), convs);
    {
      party::Member& m = g.party.members[(size_t)mi];
      if (m.questInst) g.story.drop(m.questInst);
      m.questInst = 0; m.questState = 0; m.flags = (uint16_t)(m.flags & ~(party::MF_QUEST_OFFERED | party::MF_QUEST_DONE));
      for (size_t q = 0; q < g.quests.size();) { if (g.quests[q].type == QType::Story) g.quests.erase(g.quests.begin() + (std::ptrdiff_t)q); else q++; }
      m.approval = -50;
    }
    companionPaths(g, comp(), convs);
    g.party.members[(size_t)mi].approval = 30;
    // ---- their kin, while they travel
    const party::voice::Bio b = party::voice::bio(g, g.party.members[(size_t)mi]);
    std::vector<int> kinIds;
    for (const party::voice::Kin& k : b.household) if (!k.dead && !k.away) { const int id = actorOfResident(g, si, k.idx); if (id >= 0) kinIds.push_back(id); }
    for (size_t k = 0; k < kinIds.size() && k < 2; k++) { talkPaths(g, kinIds[k], convs); people++; }
    // ---- the orders that change their state, each as its own talk; then home, asked back; then left, and amends
    companionPaths(g, comp(), convs);
    for (const char* order : {"WAIT HERE FOR ME.", "FOLLOW ME."}) {
      if (!open(g, comp())) break;
      convs++;
      const int o = shownByLabel(g, "I HAVE ORDERS FOR YOU.");
      if (o >= 0) { choose(g, o); const int w = shownByLabel(g, order); if (w >= 0) choose(g, w); }
      after(g);
    }
    if (g.partyCommand(mi, party::Cmd::Home)) {
      settle(g, 90);
      const int body = actorOfResident(g, si, res);
      if (body >= 0) talkPaths(g, body, convs);
    }
    if (g.party.members[(size_t)mi].state != party::State::With) g.partyCommand(mi, party::Cmd::Follow);
    if (g.partyCommand(mi, party::Cmd::Home)) {
      party::Member& m = g.party.members[(size_t)mi];
      m.state = party::State::Left;
      m.flags |= party::MF_LEFT_ONCE;
      m.approval = -75;
      m.mem.push_back({party::Deed::KilledInnocent, -12, g.day, sid});
      settle(g, 60);
      for (int dayStep = 0; dayStep < 2; dayStep++) {
        const int body = actorOfResident(g, si, res);
        if (body >= 0) talkPaths(g, body, convs);
        g.day++;
        settle(g, 5);
      }
    }
    // ---- a death: the kin and the innkeeper remember, and the grave
    if (pass == 1 && mi < (int)g.party.members.size()) {
      const party::Member m = g.party.members[(size_t)mi];
      party::Fallen f;
      f.id = m.id; f.name = m.name; f.voc = m.voc; f.origin = m.origin; f.culture = m.culture; f.npc = m.npc; f.site = m.site;
      f.gx = g.world.ox + (int32_t)std::floor(g.pl().p.x / TILE); f.gy = g.world.oy + (int32_t)std::floor(g.pl().p.y / TILE);
      f.day = g.day; f.cause = "FELL TO A WOLF PACK ON THE HIGH ROAD"; f.seed = 77; f.days = 40; f.kills = 12; f.bond = 55;
      f.belongings = m.inv;
      g.partyDismiss(mi);
      g.life.residentDied(sid, res, g.day);
      g.party.fallen.push_back(f);
      settle(g, 30);
      for (int d = 0; d < 4; d++) {
        for (size_t k = 0; k < kinIds.size() && k < 2; k++) talkPaths(g, kinIds[k], convs);
        g.day++;
        settle(g, 5);
      }
      const Site S = g.world.sites[(size_t)si];
      for (int bi = S.bldgFirst; bi < S.bldgFirst + S.bldgCount && bi < (int)g.world.over.bldgs.size(); bi++) {
        if (g.world.over.bldgs[(size_t)bi].type != art::Building::Inn || !g.debugEnterBuilding(bi)) continue;
        settle(g, 20);
        for (int d = 0; d < 3; d++) {
          for (int id : peopleHere(g, -1, 4)) { const int ai = findById(g, id); if (ai > 0 && g.actors[(size_t)ai].role == Role::Innkeeper) talkPaths(g, id, convs); }
          g.day++;
        }
        g.debugLeave();
        settle(g, 5);
        break;
      }
      // the grave: the epitaph, then each thing to do there
      const size_t n0 = g.party.fallen.size();
      for (int opt = 0; opt < 4 && n0; opt++) {
        PartyTalkOps::graveTalk(g, 0);
        if (g.mode != Mode::Dialogue) break;
        if (g.talkRec) g.talkRec->begin(g.dlg, g.dlg.speaker, "GRAVE", S.name, 0);
        convs++;
        const std::vector<int> sh = shown(g);
        if (opt < (int)sh.size() && !farewell(g.dlg.opts[(size_t)sh[(size_t)opt]].label)) { choose(g, sh[(size_t)opt]); stepOn(g); }
        after(g);
      }
    }
  }
  return people;
}

// the review lens over one seed's conversations (beyond the lint): a crowded first page, a chorus of one line, the
// journal read aloud
void review(const std::vector<speech::Conversation>& convs, const std::vector<std::string>& journal, std::vector<std::vector<speech::Issue>>& out) {
  std::map<std::string, std::set<uint64_t>> openers;   // (place | line) -> the people who opened a first meeting with it
  std::set<uint64_t> met;
  std::vector<bool> first(convs.size(), false);
  for (size_t i = 0; i < convs.size(); i++) {
    const speech::Conversation& c = convs[i];
    if (c.turns.empty() || !c.key || met.count(c.key)) continue;
    met.insert(c.key);
    first[i] = true;
    openers[c.where + "|" + c.turns[0].text].insert(c.key);
  }
  out.assign(convs.size(), {});
  for (size_t i = 0; i < convs.size(); i++) {
    const speech::Conversation& c = convs[i];
    if (c.turns.empty()) continue;
    if (c.turns[0].opts.size() > (size_t)speech::FIRST_PAGE) out[i].push_back({R_PAGE, 0, std::to_string(c.turns[0].opts.size()) + " options"});
    auto it = first[i] ? openers.find(c.where + "|" + c.turns[0].text) : openers.end();
    if (it != openers.end() && it->second.size() >= 3) out[i].push_back({R_CHORUS, 0, std::to_string(it->second.size()) + " people: " + c.turns[0].text.substr(0, 60)});
    for (size_t t = 0; t < c.turns.size(); t++)
      for (const std::string& d : journal)
        if (d.size() > 24 && c.turns[t].text.find(d) != std::string::npos) { out[i].push_back({R_JOURNAL, (int)t, d.substr(0, 60)}); break; }
  }
}

int cmdDialogue(int argc, char** argv) {
  uint64_t a = 1, b = 2;
  std::string dump;
  int sample = 0;
  bool lenient = false, sagaStrict = false;
  size_t maxPeople = 10;
  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "--seeds") && i + 1 < argc) parseSeedRange(argv[++i], a, b);
    else if (!strcmp(argv[i], "--dump") && i + 1 < argc) dump = argv[++i];
    else if (!strcmp(argv[i], "--sample") && i + 1 < argc) sample = std::max(0, atoi(argv[++i]));
    else if (!strcmp(argv[i], "--max-people") && i + 1 < argc) maxPeople = (size_t)std::max(1, atoi(argv[++i]));
    else if (!strcmp(argv[i], "--lenient")) lenient = true;
    else if (!strcmp(argv[i], "--saga-strict")) sagaStrict = true;
    else if (!strcmp(argv[i], "--strict")) lenient = false;   // (the default since the TALK lane's pass)
  }
  const auto t0 = std::chrono::steady_clock::now();
  int bad = 0, convs = 0, people = 0, flagged = 0, storyConvs = 0, storyFlagged = 0;
  std::map<uint32_t, int> storyByCheck;
  std::map<uint32_t, int> byCheck;
  std::map<std::string, int> byRole, issuesByRole;
  std::string dumpText;
  int printed = 0;
  for (uint64_t s = a; s <= b; s++) {
    g_curSeed = s;
    Game g(s);
    g.newEndlessGame(s);
    g.mode = Mode::Play;
    g.noWildSpawns = true;
    g.godMode = true;
    g.gold = 5000;
    speech::Recorder rec;
    g.talkRec = &rec;
    g_storyConvs.clear();
    int c0 = 0;
    people += sampleSite(g, g.world.startSite, maxPeople, c0);
    people += sampleSite(g, nearestOfType(g, SiteType::Town), maxPeople, c0);
    people += sampleSite(g, nearestOfType(g, SiteType::City), maxPeople, c0);
    people += sampleStates(g, c0);
    people += sampleCompanions(g, c0);   // (M8 VOICE)
    g.talkRec = nullptr;
    // the names the conversations may rely on only once introduced: everyone met here
    speech::LintCtx ctx;
    ctx.checks = speech::C_EVERY;
    {
      std::set<std::string> nm;
      for (const speech::Conversation& c : rec.convs)
        if (!c.who.empty() && c.who != c.role && c.who.find(' ') == std::string::npos) nm.insert(c.who);   // (a person's own name: not "GUARD")
      ctx.names.assign(nm.begin(), nm.end());
    }
    std::vector<std::string> journal;
    for (const Quest& q : g.quests) if (q.type != QType::Story && !q.desc.empty()) journal.push_back(q.desc);
    std::vector<std::vector<speech::Issue>> rv;
    review(rec.convs, journal, rv);
    std::set<std::string> metSoFar;   // the people the player has spoken with before this conversation (their names are known)
    for (size_t ci = 0; ci < rec.convs.size(); ci++) {
      const speech::Conversation& c = rec.convs[ci];
      ctx.known.assign(metSoFar.begin(), metSoFar.end());
      metSoFar.insert(c.who);
      convs++;
      const bool storyC = g_storyConvs.count(ci) != 0;
      byRole[c.role]++;
      std::vector<speech::Issue> is = speech::lint(c, ctx);
      // (fixer M7b r2) a name introduced in this conversation ("MY NEPHEW BRAIG") is known in the later ones (the
      // giver's reminder "HAVE YOU FOUND BRAIG?")
      for (const std::string& nm : ctx.names) {
        bool said = false, flaggedNm = false;
        for (const speech::Turn& tt : c.turns) if ((" " + tt.text + " ").find(" " + nm + " ") != std::string::npos || tt.text.find(nm + ".") != std::string::npos || tt.text.find(nm + ",") != std::string::npos || tt.text.find(nm + "?") != std::string::npos) said = true;
        for (const speech::Issue& x : is) if (x.check == speech::C_NAME && x.what == nm) flaggedNm = true;
        if (said && !flaggedNm) metSoFar.insert(nm);
      }
      is.insert(is.end(), rv[ci].begin(), rv[ci].end());
      if (storyC) {
        storyConvs++;
        if (!is.empty()) storyFlagged++;
        for (const speech::Issue& x : is) storyByCheck[x.check]++;
      } else {
        for (const speech::Issue& x : is) byCheck[x.check]++;
        if (!is.empty()) { issuesByRole[c.role]++; flagged++; }
      }
      for (const speech::Issue& x : is)
        if (x.check == speech::C_PLACEHOLDER || x.check == speech::C_CASE || x.check == speech::C_EMPTY || (!lenient && (!storyC || sagaStrict))) {
          out("FAIL: dialogue: seed %llu %s (%s) in %s, turn %d: %s: %s\n", (unsigned long long)s, c.who.c_str(), c.role.c_str(), c.where.c_str(), x.turn,
              reviewName(x.check), x.what.c_str());
          bad++;
        }
      if (!dump.empty() || printed < sample) {
        std::string t = speech::formatPhone(c);
        for (const speech::Issue& x : is) t += "   !! " + std::string(reviewName(x.check)) + " (turn " + std::to_string(x.turn) + "): " + x.what + "\n";
        if (!dump.empty()) dumpText += "seed " + std::to_string(s) + (storyC ? " [story] " : " ") + t + "\n";
        if (printed < sample) { printf("%s\n", t.c_str()); printed++; }
      }
    }
  }
  if (!dump.empty()) {
    FILE* f = fopen(dump.c_str(), "w");
    if (f) { fputs(dumpText.c_str(), f); fclose(f); printf("dialogue: wrote %s\n", dump.c_str()); }
    else { printf("FAIL: dialogue: cannot write %s\n", dump.c_str()); bad++; }
  }
  const double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
  printf("dialogue: %d conversations with %d people over seeds %llu..%llu (%.1f s)\n", convs, people, (unsigned long long)a, (unsigned long long)b, secs);
  printf("dialogue: by trade:");
  for (auto& kv : byRole) printf(" %s %d (%d with issues)", kv.first.c_str(), kv.second, issuesByRole[kv.first]);
  printf("\n");
  printf("dialogue: %d conversations with issues (%.1f%%):", flagged, convs ? 100.0 * flagged / convs : 0.0);
  for (auto& kv : byCheck) printf(" %s %d,", reviewName(kv.first), kv.second);
  printf("\n");
  printf("dialogue: story-engine conversations (SAGA lane, reported%s): %d, %d with issues:", sagaStrict ? " and gated" : "", storyConvs, storyFlagged);
  for (auto& kv : storyByCheck) printf(" %s %d,", reviewName(kv.first), kv.second);
  printf("\n");
  if (convs == 0) { printf("FAIL: dialogue: no conversation recorded\n"); bad++; }
  printf("dialogue: %d failures\n", bad);
  return bad ? 1 : 0;
}

}  // namespace

RPG_TEST_CMD("--dialogue", "M7b: real conversations recorded, linted and reviewed (strict) [--seeds A..B] [--dump FILE] [--sample N] [--lenient]", cmdDialogue);
