// rpg_test --banter [--seeds A..B] [--dump FILE]: the M8 Fellowship banter gate (VISION_PLAN 9, 15.26; rpg/sim/banter.cpp,
// rpg/sim/party_voice.h). VOICE lane.
//
// Per seed, census companions of the start village and the nearest town (and a bonded hound):
//   - every line of every kind through the M7b lint (speech::lint: placeholders, case and the font, the panel,
//     repeated phrases, doubled words, stitched fragments, one-word fragments, unfinished sentences, speakers naming
//     themselves) and the banter's own fit (two rows of the phone panel):
//       deedLine for every Deed, by four natures and the delta's sign and size; cryLine for kinds 1 2 3 5 6 7 8 (kind 5 for
//       every origin); epitaph in every rite (two or three short lines, the last one ended); aboutLine;
//       banterLine for every trigger (each one must speak for a member with what it needs: a stake, a homeland, a
//       ruin, a hound...);
//   - culture-border lines for every ordered pair of peoples met in the test world (a member of one entering the land
//     of the other);
//   - banterStep in staged scenes: a night road, a culture border crossed, the weather on screen, a wonder found, a
//     hard fight won, the player badly hurt, the homeland's war, approval and trust thresholds, two companions talking
//     to each other at LEADERSHIP_LEVEL with an answer, the hound; the gap (no line within BANTER_GAP_SECS but the
//     exempt ones), no line twice in a row and none twice in a day.
// --dump FILE writes every line, by kind, for the review lens.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <vector>
#include "rpg/culture/culture.h"
#include "rpg/sim/game_internal.h"
#include "rpg/sim/life.h"
#include "rpg/sim/party.h"
#include "rpg/sim/party_voice.h"
#include "rpg/sim/talk.h"
#include "rpg/world/source.h"
#include "tools/tests/tests.h"

namespace {

using party::voice::Trig;

int g_bad = 0;
std::map<std::string, int> g_kinds;      // lines linted by kind
std::map<std::string, std::vector<std::string>> g_dump;
void fail(const std::string& m) { out("FAIL: banter: %s\n", m.c_str()); g_bad++; }

void lintLine(const std::string& who, const std::string& line, const std::string& what, bool banter = true) {
  if (line.empty()) return;
  g_kinds[what]++;
  auto& d = g_dump[what];
  if (d.size() < 400 && std::find(d.begin(), d.end(), who + ": " + line) == d.end()) d.push_back(who + ": " + line);
  speech::Conversation c;
  c.who = who;
  speech::Turn t;
  t.speaker = who; t.text = line;
  c.turns.push_back(t);
  speech::LintCtx ctx;
  ctx.known.push_back(who);
  for (const speech::Issue& i : speech::lint(c, ctx)) fail(what + ": \"" + line + "\": " + speech::checkName(i.check) + ": " + i.what);
  if (!speech::fitsPanel(line)) fail(what + ": does not fit the phone panel: " + line);
  if (banter && speech::wrapLines(line, speech::PANEL_CHARS).size() > 2) fail(what + ": more than two rows of banter: " + line);
}

void steps(Game& g, int n) { for (int f = 0; f < n; f++) { g.update(SIM_DT, Input()); g.events.clear(); if (g.mode != Mode::Play) g.mode = Mode::Play; } }

// a census adult of site `si` recruited (-1 none)
int recruitAt(Game& g, int si, int skip = 0) {
  life::Census* c = g.life.census(g.world, si);
  if (!c) return -1;
  int seen = 0;
  for (size_t ri = 0; ri < c->res.size(); ri++) {
    const life::Resident r = c->res[ri];
    if ((r.flags & (life::RF_DEAD | life::RF_KEY | life::RF_AWAY | life::RF_COMPANION)) || r.job == life::Job::Child || r.age < 18 || r.age > 60) continue;
    if (seen++ < skip) continue;
    Actor a;
    a.id = -97; a.npc = true; a.human = true; a.site = si; a.resident = r.idx; a.p = g.pl().p + Vec2(18, 0); a.name = r.name;
    return g.partyRecruit(a, party::Origin::Tavern);
  }
  return -1;
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

// the coverage of one member's lines (every deed, cry, the epitaph, their story, every trigger)
void coverage(Game& g, int mi, std::map<int, int>& trigLines) {
  party::Member& m = g.party.members[(size_t)mi];
  const party::Personality keep = m.p;
  static const int8_t natures[4][4] = {{2, 2, 2, 2}, {-2, -2, -2, -2}, {2, -2, 0, -2}, {-1, 2, 2, 1}};
  for (const auto& n : natures) {
    m.p.brave = n[0]; m.p.kind = n[1]; m.p.pious = n[2]; m.p.lawful = n[3];
    for (int d = 0; d < (int)party::Deed::COUNT; d++) {
      int any = 0;
      for (int delta : {-14, -6, 6, 14}) {
        const std::string l = party::deedLine(g, m, (party::Deed)d, delta);
        if (!l.empty()) any++;
        lintLine(m.name, l, "deed " + std::string(party::deedName((party::Deed)d)));
      }
      if (!any) fail(std::string("no deed line for ") + party::deedName((party::Deed)d));
    }
  }
  m.p = keep;
  for (int k : {1, 2, 3, 5, 6, 7, 8}) {
    const std::string l = party::cryLine(g, m, k);
    if (l.empty()) fail("no cry of kind " + std::to_string(k) + " for " + m.name);
    lintLine(m.name, l, "cry " + std::to_string(k));
  }
  const party::Origin o0 = m.origin;
  for (int o = 0; o < (int)party::Origin::Animal; o++) {
    m.origin = (party::Origin)o;
    const std::string l = party::cryLine(g, m, 5);
    if (l.empty()) fail("no joining cry for origin " + std::string(party::originName((party::Origin)o)));
    lintLine(m.name, l, "cry 5 " + std::string(party::originName((party::Origin)o)));
  }
  m.origin = o0;
  const std::string about = party::aboutLine(g, m);
  if (about.empty()) fail("no story (aboutLine) for " + m.name);
  lintLine(m.name, about, "about", false);
  // every stake told (the stake's sentence) and every trigger with what it needs
  const party::Stake s0 = m.stake;
  for (int h = 1; h < (int)party::Hook::COUNT; h++) {
    m.stake.hook = (party::Hook)h;
    lintLine(m.name, party::aboutLine(g, m), "about " + std::string(party::hookName((party::Hook)h)), false);
  }
  m.stake = s0;
  const Site& home = g.world.sites[(size_t)g.world.startSite];
  if (!m.stake.ruin) {
    const int rh = g.world.findSiteNear(g.world.ox + home.ex, g.world.oy + home.ey, SiteType::Ruin, 3);
    if (rh >= 0) m.stake.ruin = g.world.sites[(size_t)rh].id;
  }
  for (int t = 0; t < (int)Trig::COUNT; t++) {
    const Trig tr = (Trig)t;
    if (tr == Trig::Pair) continue;
    uint64_t arg = 0;
    std::string name;
    party::Stake keepS = m.stake;
    if (tr == Trig::Border) {   // another people's land: the first culture met that is not theirs
      for (const Site& S : g.world.sites) if (S.culture && cult::familyOf(S.culture) != cult::familyOf(m.culture)) { arg = S.culture; break; }
      if (!arg) continue;
    }
    if (tr == Trig::StakeSite) m.stake.hook = party::Hook::BurnedHome;
    if (tr == Trig::Wonder) name = "THE WEEPING STONES";
    if (tr == Trig::Hound) name = "BRAN";
    if (tr == Trig::War) {
      const ew::Gid hl = party::voice::bio(g, m).homelandId;
      for (const realm::KingdomState& K : g.realm.kingdoms()) if (K.id != hl && !K.fallen) { arg = K.id; break; }
    }
    int said = 0;
    for (uint32_t salt = 0; salt < 6; salt++) {
      const std::string l = party::voice::banterLine(g, mi, tr, arg, name, salt * 977u);
      if (l.empty()) continue;
      said++;
      lintLine(m.name, l, std::string("banter ") + party::voice::trigName(tr));
    }
    if (tr == Trig::HardFight) for (uint32_t salt = 0; salt < 4; salt++) lintLine(m.name, party::voice::banterLine(g, mi, tr, 1, "", salt * 31u), "banter HARD FIGHT (downed)");
    m.stake = keepS;
    trigLines[t] += said;
    const bool needsMore = tr == Trig::Border || tr == Trig::War || tr == Trig::StakeRuin;
    if (!said && !needsMore) fail(std::string("no ") + party::voice::trigName(tr) + " line for " + m.name);
  }
  // the grave in every rite
  party::Fallen f;
  f.name = m.name; f.voc = m.voc; f.culture = m.culture; f.site = m.site; f.days = 45; f.kills = 12; f.bond = 60; f.seed = 7;
  std::set<int> rites;
  for (const Site& S : g.world.sites) {
    if (!S.culture || !g.world.src) continue;
    const int b = g.world.src->culture(S.culture).faith.burial;
    if (rites.count(b)) continue;
    rites.insert(b);
    f.culture = S.culture;
    for (uint32_t sd = 0; sd < 3; sd++) {
      f.seed = sd * 7919u + 1;
      const std::string e = party::epitaph(g, f);
      int parts = 1;
      for (size_t p = e.find(" / "); p != std::string::npos; p = e.find(" / ", p + 3)) parts++;
      if (parts < 2 || parts > 3) fail("an epitaph of " + std::to_string(parts) + " lines: " + e);
      std::string t = e;
      for (size_t p = t.find(" / "); p != std::string::npos; p = t.find(" / ")) t.replace(p, 3, "\n");
      for (const std::string& row : speech::wrapLines(t, speech::PANEL_CHARS)) (void)row;
      lintLine("THE GRAVE OF " + m.name, t, "epitaph rite " + std::to_string(b), false);
    }
  }
}

// every ordered pair of the peoples met here: a member of the first entering the land of the second
int borderPairs(Game& g, int mi) {
  std::vector<uint64_t> cults;
  std::set<uint64_t> fam;
  auto add = [&](uint64_t c) { if (c && fam.insert(cult::familyOf(c)).second) cults.push_back(c); };
  for (const Site& S : g.world.sites) add(S.culture);
  if (g.world.src) {
    const Site& home = g.world.sites[(size_t)g.world.startSite];
    for (int dy = -3; dy <= 3; dy++)
      for (int dx = -3; dx <= 3; dx++) add(g.world.src->cultureAt(g.world.ox + home.ex + dx * 900, g.world.oy + home.ey + dy * 900));
  }
  party::Member& m = g.party.members[(size_t)mi];
  const uint64_t keep = m.culture;
  int pairs = 0;
  for (uint64_t a : cults)
    for (uint64_t b : cults) {
      if (cult::familyOf(a) == cult::familyOf(b)) continue;
      m.culture = a;
      bool any = false;
      for (uint32_t salt = 0; salt < 3; salt++) {
        const std::string l = party::voice::banterLine(g, mi, Trig::Border, b, "", salt * 131u);
        if (l.empty()) continue;
        any = true;
        lintLine(m.name, l, "banter BORDER (pairs)");
      }
      if (!any) fail("no border line for " + g.world.src->culture(a).adjective + " entering " + g.world.src->culture(b).adjective + " land");
      pairs++;
    }
  m.culture = keep;
  return pairs;
}

// banterStep's gap, repeats and the staged scenes
void scenes(Game& g, uint64_t seed, int mi) {
  party::Party& P = g.party;
  party::voice::banterResetCounts();
  auto stepLine = [&](float dt) {
    const std::string before = P.lastBanter;
    const int nb = P.stats.banter;
    party::banterStep(g, dt);
    return P.stats.banter != nb ? P.lastBanter : std::string();
  };
  // ---- the gap: nothing within BANTER_GAP_SECS (the night road would speak otherwise)
  g.hour = 23.0f;
  // (the start village by its global tile, read once: the sites vector moves as the window does, so no reference is kept)
  const int32_t homeX = g.world.ox + g.world.sites[(size_t)g.world.startSite].ex, homeY = g.world.oy + g.world.sites[(size_t)g.world.startSite].ey;
  g.teleportGlobal(homeX + 70, homeY + 50);
  steps(g, 20);
  if (g.curSite >= 0 || g.inside) { out("banter: seed %llu: the road scene landed in a site; the night test is skipped\n", (unsigned long long)seed); }
  else {
    P.banterT = party::BANTER_GAP_SECS * 0.5f;
    if (!stepLine(0.0f).empty()) fail("a line within the banter gap");
    P.banterT = party::BANTER_GAP_SECS + 1;
    const std::string l = stepLine(0.0f);
    if (l.empty()) fail("no banter on a night road");
    lintLine(P.members[(size_t)mi].name, l, "step night");
  }
  // ---- no line twice in a row, none twice in a day (forced through many gaps, hours and weathers)
  {
    std::string prev;
    std::set<std::string> today;
    int day0 = g.day, said = 0;
    for (int k = 0; k < 40; k++) {
      g.hour = (float)((k * 5) % 24) + 0.5f;
      if (k % 12 == 11) g.day++;
      if (g.day != day0) { today.clear(); day0 = g.day; }
      P.banterT = party::BANTER_GAP_SECS + 1;
      const std::string l = stepLine(party::BANTER_GAP_SECS * 2.1f);
      if (l.empty()) continue;
      said++;
      if (l == prev) fail("the same banter line twice in a row: " + l);
      if (today.count(l)) fail("the same banter line twice in a day: " + l);
      today.insert(l);
      prev = l;
      lintLine(P.lastBanterBy >= 0 && P.lastBanterBy < (int)P.members.size() ? P.members[(size_t)P.lastBanterBy].name : std::string("?"), l, "step forced");
    }
    if (said < 5) fail("only " + std::to_string(said) + " banter lines in 40 forced gaps");
  }
  g.hour = 12.0f;
  // ---- a culture border crossed: home ground first, then the land of another people under the player's feet
  {
    party::Member& m = P.members[(size_t)mi];
    const int32_t hx = homeX, hy = homeY;
    int32_t bx = 0, by = 0;
    bool found = false;
    for (int r = 1; r <= 14 && !found; r++)
      for (int k = 0; k < 8 * r && !found; k++) {
        const double ang = 6.283185307179586 * k / (8.0 * r);
        const int32_t tx = hx + (int32_t)std::lround(std::cos(ang) * 400.0 * r), ty = hy + (int32_t)std::lround(std::sin(ang) * 400.0 * r);
        const uint64_t c = g.world.src ? g.world.src->cultureAt(tx, ty) : 0;
        if (c && m.culture && cult::familyOf(c) != cult::familyOf(m.culture)) { bx = tx; by = ty; found = true; }
      }
    if (!found) out("banter: seed %llu: no other people's land within reach; the border crossing is skipped\n", (unsigned long long)seed);
    else {
      m.talkBits &= 0xFFFFu;   // (no border remarked yet)
      g.hour = 12.0f;
      g.teleportGlobal(hx + 3, hy + 3);
      steps(g, 10);
      for (int k = 0; k < 3; k++) { P.banterT = 0; party::banterStep(g, 1.1f); }   // (the land is looked at: home)
      g.teleportGlobal(bx, by);
      steps(g, 10);
      const int bc = party::voice::banterCount(Trig::Border);
      std::string l;
      for (int k = 0; k < 6 && party::voice::banterCount(Trig::Border) == bc; k++) { P.banterT = party::BANTER_GAP_SECS + 1; l = stepLine(1.1f); }
      if (party::voice::banterCount(Trig::Border) == bc) fail("no line on crossing into another people's land (said: " + l + ")");
      else lintLine(m.name, l, "step border");
      g.teleportGlobal(homeX + 70, homeY + 50);
      steps(g, 10);
    }
  }
  // ---- a wonder found
  {
    Event e;
    e.type = Ev::Discover; e.f = 2.0f; e.s = "THE SINGING FALLS"; e.p = g.pl().p;
    g.events.push_back(e);
    P.banterT = 30.0f;
    const std::string l = stepLine(0.016f);
    g.events.clear();
    if (l.find("SINGING FALLS") == std::string::npos) fail("no line at a wonder found (said: " + l + ")");
    else lintLine(P.members[(size_t)P.lastBanterBy].name, l, "step wonder");
  }
  // ---- the homeland at war
  {
    const party::voice::Bio b = party::voice::bio(g, P.members[(size_t)mi]);
    ew::Gid other = 0;
    for (const realm::KingdomState& K : g.realm.kingdoms()) if (K.id != b.homelandId && !K.fallen) { other = K.id; break; }
    if (b.homelandId && other) {
      if (!g.realm.atWar(other, b.homelandId)) g.realm.forceWar(other, b.homelandId, g.day);
      P.members[(size_t)mi].talkBits &= ~(7u * party::voice::TB_WAR0);
      P.banterT = party::BANTER_GAP_SECS + 1;
      std::string l;
      for (int k = 0; k < 3 && l.find(" WAR") == std::string::npos; k++) { P.banterT = party::BANTER_GAP_SECS + 1; l = stepLine(0.016f); }
      if (l.find("WAR") == std::string::npos) fail("no line on the homeland's war (said: " + l + ")");
      else lintLine(P.members[(size_t)mi].name, l, "step war");
    }
  }
  // ---- approval and trust
  {
    party::Member& m = P.members[(size_t)mi];
    m.approval = 55; m.bond = 55;
    m.talkBits &= ~(party::voice::TB_PRAISE | party::voice::TB_TRUST);
    int got = 0;
    for (int k = 0; k < 4; k++) {
      P.banterT = party::BANTER_GAP_SECS + 1;
      const int pc = party::voice::banterCount(Trig::Praise), tc = party::voice::banterCount(Trig::Trust);
      const std::string l = stepLine(0.016f);
      if (party::voice::banterCount(Trig::Praise) > pc || party::voice::banterCount(Trig::Trust) > tc) { got++; lintLine(m.name, l, "step approval"); }
    }
    if (got < 2) fail("the approval and trust lines were not both said");
  }
  // ---- a hard fight won (a hostile engaged, the player badly hurt, then calm): the warning, then the after-word
  {
    Actor e;
    e.id = 990001; e.hostile = true; e.aggro = true; e.p = g.pl().p + Vec2(30, 0); e.maxHp = 10; e.hp = 10; e.npc = false; e.human = false;
    g.actors.push_back(e);
    const float hp0 = g.pl().hp;
    g.pl().hp = g.pl().maxHp * 0.2f;
    P.banterT = 50.0f;
    const std::string warn = stepLine(0.016f);
    if (warn.empty()) fail("no warning when the player is badly hurt in a fight");
    else lintLine(P.members[(size_t)P.lastBanterBy].name, warn, "step low health");
    for (size_t k = 1; k < g.actors.size(); k++) if (g.actors[k].id == 990001) { g.actors.erase(g.actors.begin() + (std::ptrdiff_t)k); break; }
    g.pl().hp = hp0;
    P.banterT = 30.0f;
    stepLine(2.0f);
    const std::string after = stepLine(3.0f);
    if (party::voice::banterCount(Trig::HardFight) < 1) fail("no line after a hard fight (said: " + after + ")");
    else lintLine(P.members[(size_t)P.lastBanterBy].name, after, "step hard fight");
  }
  // ---- the weather on screen: a slot whose sky falls on the player's tile
  {
    bool found = false;
    for (int k = 0; k < 80 && !found; k++) {
      g.day += 1;
      g.hour = 12.5f;
      if (party::voice::weatherNow(g) == Trig::COUNT) continue;
      found = true;
      P.banterT = party::BANTER_GAP_SECS + 1;
      const int before = 0;
      (void)before;
      const std::string l = stepLine(0.016f);
      if (l.empty()) fail("no line about the weather on screen");
      else lintLine(P.members[(size_t)P.lastBanterBy].name, l, std::string("step weather ") + party::voice::trigName(party::voice::weatherNow(g)));
    }
    if (!found) out("banter: seed %llu: no weather fell here in 80 days (the land's sky is clear)\n", (unsigned long long)seed);
  }
}

// the two-companion exchange (LEADERSHIP_LEVEL) and the hound
void company(Game& g, int a, int b) {
  party::Party& P = g.party;
  std::string la, lb;
  int pairs = 0;
  for (uint32_t salt = 0; salt < 24; salt++)
    if (party::voice::exchange(g, a, b, salt * 7919u, la, lb)) {
      pairs++;
      lintLine(P.members[(size_t)a].name, la, "pair line");
      lintLine(P.members[(size_t)b].name, lb, "pair answer");
    }
  if (!pairs) fail("two companions had nothing to say to each other");
  // through banterStep: a line, then the other's answer a few seconds later (the other triggers said already)
  for (int k : {a, b}) P.members[(size_t)k].talkBits |= 7u * party::voice::TB_WAR0 | party::voice::TB_PRAISE | party::voice::TB_TRUST;
  const int pc0 = party::voice::banterCount(Trig::Pair);
  for (int k = 0; k < 16 && party::voice::banterCount(Trig::Pair) == pc0; k++) {
    P.banterT = party::BANTER_GAP_SECS + 1;
    party::banterStep(g, party::BANTER_GAP_SECS * 2.1f);
    party::banterStep(g, 4.0f);   // (an answer, when the line was one)
  }
  if (party::voice::banterCount(Trig::Pair) == pc0) { fail("two companions with the player never spoke to each other: " + P.lastBanter); return; }
  // the line and its answer: said by the two of them, the answer last
  const int by = P.lastBanterBy;
  if (by != a && by != b) fail("an exchange answered by someone else");
  lintLine(by >= 0 ? P.members[(size_t)by].name : std::string("?"), P.lastBanter, "step pair answer");
}

void play(uint64_t seed, int& people, int& pairsTotal, std::map<int, int>& trigLines) {
  Game g(seed);
  g.newEndlessGame(seed);
  g.mode = Mode::Play;
  g.noWildSpawns = true;
  g.godMode = true;
  steps(g, 30);
  const int mi = recruitAt(g, g.world.startSite);
  if (mi < 0) { fail("no recruit in the start village"); return; }
  people++;
  coverage(g, mi, trigLines);
  pairsTotal += borderPairs(g, mi);
  scenes(g, seed, mi);
  // the nearest town's own (from LEADERSHIP_LEVEL two people travel with the player), and a hound
  g.plLevel = party::LEADERSHIP_LEVEL;
  const int town = nearestOfType(g, SiteType::Town);
  int second = -1;
  if (town >= 0 && g.fastTravel(town)) { steps(g, 60); second = recruitAt(g, town); }
  if (second < 0) second = recruitAt(g, g.world.startSite, 1);
  if (second < 0) { fail("no second companion"); return; }
  people++;
  coverage(g, second, trigLines);
  company(g, mi, second);
  const int hound = g.partyRecruitStranger("BRAN", false, g.party.members[(size_t)mi].culture, 1234567u, party::Origin::Animal, g.pl().p + Vec2(10, 0));
  if (hound < 0) fail("no hound");
  else {
    for (int k : {1, 2, 3, 5}) if (!party::cryLine(g, g.party.members[(size_t)hound], k).empty()) fail("the hound cries out in words");
    std::string l;
    for (uint32_t salt = 0; salt < 6; salt++) {
      l = party::voice::banterLine(g, mi, Trig::Hound, 0, "BRAN", salt * 31u);
      lintLine(g.party.members[(size_t)mi].name, l, "banter HOUND (with a hound)");
    }
  }
  // a deed through the bus: the member it moved most says their line
  {
    party::Member& m = g.party.members[(size_t)mi];
    m.p.kind = 2; m.p.lawful = 2;
    const int nb = g.party.stats.banter;
    g.deed(party::Deed::KilledInnocent, g.world.sites[(size_t)g.world.startSite].id, 2);
    steps(g, 2);
    if (g.party.stats.banter == nb) fail("no word from anyone after an innocent was killed");
    else lintLine("?", g.party.lastBanter, "deed through the bus");
  }
}

int banterCmd(int argc, char** argv) {
  uint64_t a = 1, b = 3;
  std::string dump;
  for (int i = 2; i < argc; i++) {
    if (!std::strcmp(argv[i], "--seeds") && i + 1 < argc && !parseSeedRange(argv[++i], a, b)) { printf("bad --seeds\n"); return 2; }
    else if (!std::strcmp(argv[i], "--dump") && i + 1 < argc) dump = argv[++i];
  }
  g_bad = 0;
  g_kinds.clear();
  g_dump.clear();
  int people = 0, pairs = 0;
  std::map<int, int> trig;
  for (uint64_t s = a; s <= b; s++) { g_curSeed = s; play(s, people, pairs, trig); }
  int total = 0;
  for (const auto& kv : g_kinds) total += kv.second;
  printf("banter: %d lines linted for %d companions over seeds %llu..%llu; %d border pairs of peoples\n", total, people, (unsigned long long)a,
         (unsigned long long)b, pairs);
  printf("banter: lines by trigger:");
  for (int t = 0; t < (int)Trig::COUNT; t++) if (trig.count(t)) printf(" %s %d,", party::voice::trigName((Trig)t), trig[t]);
  printf("\n");
  if (!dump.empty()) {
    if (FILE* f = fopen(dump.c_str(), "w")) {
      for (const auto& kv : g_dump) {
        fprintf(f, "== %s (%d)\n", kv.first.c_str(), g_kinds[kv.first]);
        for (const std::string& l : kv.second) fprintf(f, "   %s\n", l.c_str());
      }
      fclose(f);
      printf("banter: wrote %s\n", dump.c_str());
    }
  }
  printf("banter: %d failure(s)\n", g_bad);
  return g_bad ? 1 : 0;
}

}  // namespace

RPG_TEST_CMD("--banter", "M8 Fellowship banter gate: every trigger staged, border lines for every pair of peoples, cries, deeds, epitaphs, their stories, the gap and no repeats, two companions talking, all through the M7b lint [--seeds A..B] [--dump FILE]", banterCmd);
