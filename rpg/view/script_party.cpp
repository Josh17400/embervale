// M8 Fellowship script commands (lead, phase A; the FELLOWS lane owns this file and adds to it; the VIEW lane adds its
// screen commands in rpg/view/script_partyview.cpp). They stage companions for screenshots and checks, never touching the
// player's save. Member index m defaults to 0.
//   party recruit [n]                  the n-th (0) census resident of the settlement the player is in (else the start
//                                      village) who may travel, joins at once beside the player
//   party cmd <follow|wait|attack|defend|skill|passive|home> [m]   an order (as the radial gives it)  (append "refused": it must be refused)
//   party deed <word> [weight]         a deed on the bus (party::deedName words, lower case, spaces as '-': killed-an-innocent,
//                                      or the enum's own word: rescued, mercy, cruelty, stole, joinedwar...)
//   party down [m] | party kill [m] | party revive [m]   downed now / dies for good on the next step / back on their feet
//   party approval <v> [m] | party bond <v> [m]          set them
//   party near [m]                     the member stands beside the player
//   party talk [m]                     the player steps up to the member and presses use (their conversation opens)
//   party stats                        print the party (stdout)
//   expect party members <n> | with <n> | fallen <n> | state <with|downed|waiting|home|left> [m] |
//                approval <min> <max> [m] | near <tiles> [m] | banter | share <lo> <hi>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include "rpg/culture/culture.h"
#include "rpg/sim/game.h"
#include "rpg/sim/game_internal.h"
#include "rpg/sim/home.h"
#include "rpg/sim/life.h"
#include "rpg/sim/party.h"
#include "rpg/world/source.h"
#include "rpg/view/script_api.h"

namespace {

std::string lw(std::string s) {
  for (char& ch : s) if (ch >= 'A' && ch <= 'Z') ch = (char)(ch + 32);
  return s;
}
int memberArg(ScriptCtx& c, size_t i) {
  const std::string w = c.arg(i);
  const int m = w.empty() ? 0 : std::atoi(w.c_str());
  if (m < 0 || m >= (int)c.game.party.members.size()) { c.fail("party: no member " + std::to_string(m)); return -1; }
  return m;
}
int actorOf(Game& g, int m) {
  for (size_t i = 1; i < g.actors.size(); i++) if (g.actors[i].comp == m + 1 && g.actors[i].st != AState::Dead) return (int)i;
  return -1;
}
int deedWord(const std::string& w) {
  static const char* const words[] = {"questdone", "questfailed", "rescued", "refugees", "charity", "befriended", "mercy", "cruelty",
                                      "killedinnocent", "killedguard", "stole", "bandits", "beast", "monster", "joinedwar", "defended",
                                      "ruin", "tavern", "prayed", "bribed", "lied", "oathbreak", "defiledgrave", "abandoned"};
  static_assert(sizeof(words) / sizeof(words[0]) == (size_t)party::Deed::COUNT, "a word for every deed");
  for (int d = 0; d < (int)party::Deed::COUNT; d++) {
    if (w == words[d]) return d;
    std::string n = lw(party::deedName((party::Deed)d));
    for (char& ch : n) if (ch == ' ') ch = '-';
    if (w == n) return d;
  }
  return -1;
}
int cmdWord(const std::string& w) {
  static const char* const words[] = {"follow", "wait", "attack", "defend", "skill", "passive", "home"};
  static_assert(sizeof(words) / sizeof(words[0]) == (size_t)party::Cmd::COUNT, "a word for every command");
  for (int k = 0; k < (int)party::Cmd::COUNT; k++) if (w == words[k]) return k;
  return -1;
}
int stateWord(const std::string& w) {
  static const char* const words[] = {"with", "downed", "waiting", "home", "left", "dead"};
  static_assert(sizeof(words) / sizeof(words[0]) == (size_t)party::State::COUNT, "a word for every state");
  for (int k = 0; k < (int)party::State::COUNT; k++) if (w == words[k]) return k;
  return -1;
}

bool cmdParty(ScriptCtx& c) {
  Game& g = c.game;
  const std::string what = lw(c.arg(1));
  if (what == "recruit") {
    const int want = c.arg(2).empty() ? 0 : std::atoi(c.arg(2).c_str());
    int si = g.curSite >= 0 && g.curSite < (int)g.world.sites.size() && g.world.sites[(size_t)g.curSite].settlement() ? g.curSite : g.world.startSite;
    const life::Census* cs = g.life.census(g.world, si);
    if (!cs) { c.fail("party recruit: no census here"); return true; }
    int n = 0;
    for (const life::Resident& r : cs->res) {
      if ((r.flags & (life::RF_DEAD | life::RF_KEY | life::RF_AWAY)) || r.job == life::Job::Child || r.job == life::Job::Elder || r.age < 18) continue;
      if (n++ < want) continue;
      Actor a;
      a.id = -98; a.npc = true; a.human = true; a.site = si; a.resident = r.idx;
      a.p = g.pl().p + Vec2(18, 0);
      if (g.partyRecruit(a, party::Origin::Tavern) < 0) c.fail("party recruit: " + r.name + " would not join");
      return true;
    }
    c.fail("party recruit: nobody here may travel");
    return true;
  }
  if (what == "cmd") {
    const int k = cmdWord(lw(c.arg(2)));
    const int m = memberArg(c, 3);
    if (k < 0) { c.fail("party cmd: unknown order " + c.arg(2)); return true; }
    // "party cmd <order> <m> refused": the order must be refused (fixer M8 r2)
    const bool wantRefused = lw(c.arg(4)) == "refused";
    if (m >= 0) {
      const bool ok = g.partyCommand(m, (party::Cmd)k);
      if (!ok && !wantRefused) c.fail("party cmd " + c.arg(2) + ": refused");
      else if (ok && wantRefused) c.fail("party cmd " + c.arg(2) + ": was obeyed, should be refused");
    }
    return true;
  }
  if (what == "deed") {
    const int d = deedWord(lw(c.arg(2)));
    if (d < 0) { c.fail("party deed: unknown deed " + c.arg(2)); return true; }
    g.deed((party::Deed)d, 0, c.arg(3).empty() ? 1 : std::atoi(c.arg(3).c_str()));
    return true;
  }
  if (what == "down" || what == "kill" || what == "revive" || what == "near" || what == "talk" || what == "burn") {
    const int m = memberArg(c, 2);
    if (m < 0) return true;
    const int ai = actorOf(g, m);
    party::Member& M = g.party.members[(size_t)m];
    if (what == "down") { if (ai > 0) g.debugFell(g.actors[(size_t)ai].id); else c.fail("party down: not in play"); return true; }
    if (what == "kill") {
      if (M.state != party::State::Downed && ai > 0) g.debugFell(g.actors[(size_t)ai].id);
      M.downT = 0; M.reviveT = 0;
      return true;
    }
    if (what == "revive") {
      if (M.state != party::State::Downed) return true;
      M.state = party::State::With; M.downT = 0; M.reviveT = 0;
      if (ai > 0) { Actor& a = g.actors[(size_t)ai]; a.st = AState::Idle; a.posture = art::Posture::None; a.hp = a.maxHp * 0.3f; }
      return true;
    }
    if (ai < 0) { c.fail("party " + what + ": not in play"); return true; }
    Actor& a = g.actors[(size_t)ai];
    if (what == "burn") { a.burnT = 3.0f; return true; }   // (fixer M8 r2) set them alight, as an ember hound's bite does
    if (what == "near") { a.p = g.pl().p + Vec2(16, 0); a.navNext = -1; return true; }
    // talk: step up beside them, face them, press use
    g.pl().p = a.p + Vec2(-14, 0);
    g.pl().aim = Vec2(1, 0); g.pl().face = 2;
    Input in;
    in.interact = true;
    g.update(SIM_DT, in);
    if (g.mode != Mode::Dialogue) c.fail("party talk: no conversation opened");
    return true;
  }
  if (what == "approval" || what == "bond") {
    const int v = std::atoi(c.arg(2).c_str());
    const int m = memberArg(c, 3);
    if (m < 0) return true;
    if (what == "approval") g.party.members[(size_t)m].approval = (int8_t)std::clamp(v, -100, 100);
    else g.party.members[(size_t)m].bond = (uint8_t)std::clamp(v, 0, 100);
    return true;
  }
  if (what == "stats") {
    for (size_t i = 0; i < g.party.members.size(); i++) {
      const party::Member& m = g.party.members[i];
      party::Member again = m;   // (fixer M8 r1) the look its census would dress now (it must not be what they wear)
      again.hasLook = false;
      const uint64_t fresh = m.voc == party::Vocation::Hound ? 0ull : g.partyLook(again).key();
      printf("party %zu: %s, %s %s, %s, approval %d, bond %d, days %u, kills %u, downs %u, look %016llx (dressed afresh %016llx)\n", i, m.name.c_str(), party::originName(m.origin),
             party::vocationName(m.voc), party::stateName(m.state), m.approval, m.bond, m.days, m.kills, m.downs,
             (unsigned long long)(m.voc == party::Vocation::Hound ? 0ull : g.partyLook(m).key()), (unsigned long long)fresh);
    }
    for (const party::Fallen& f : g.party.fallen) printf("party fallen: %s (%s), day %d\n", f.name.c_str(), f.cause.c_str(), f.day);
    const party::Stats& s = g.party.stats;
    printf("party stats: taken player %.0f party %.0f, worst lag %.1f tiles, catch-ups %d, downs %d, revives %d, deaths %d, banter %d\n", s.dmgPlayer,
           s.dmgParty, s.worstLagTiles, s.catchUps, s.downs, s.revives, s.deaths, s.banter);
    return true;
  }
  // ---- FELLOWS lane (phase B): staging for the M8 screenshots and checks
  if (what == "regular") {   // the tavern regular of the settlement the player is in (or of the inn they stand in)
    int si = g.inside && g.subBldg >= 0 ? g.world.over.bldgs[(size_t)g.subBldg].site : g.curSite;
    if (si < 0 || si >= (int)g.world.sites.size()) { c.fail("party regular: not in a settlement"); return true; }
    const std::vector<int> regs = g.partyRegulars(si);
    if (regs.empty()) { c.fail("party regular: " + g.world.sites[(size_t)si].name + " has no tavern regular"); return true; }
    // their body here when they are in play, else a body for them beside the player
    int ai = -1;
    for (size_t i = 1; i < g.actors.size(); i++) if (g.actors[i].resident == regs[0] && g.actors[i].site == si && !g.actors[i].comp) ai = (int)i;
    Actor tmp;
    tmp.id = -97; tmp.npc = true; tmp.human = true; tmp.site = si; tmp.resident = regs[0]; tmp.p = g.pl().p + Vec2(18, 0);
    Actor& who = ai > 0 ? g.actors[(size_t)ai] : tmp;
    const Game::PartyOffer o = g.partyOffer(who);
    if (!o.ok) { c.fail("party regular: they would not come (" + o.why + ")"); return true; }
    if (g.partyRecruit(who, o.origin) < 0) c.fail("party regular: the recruit failed");
    return true;
  }
  if (what == "merc") {   // hire the nearest sellsword (in an inn)
    int best = -1;
    float bd = 1e30f;
    for (size_t i = 1; i < g.actors.size(); i++) {
      const Actor& a = g.actors[i];
      if (a.partyTag < party::PT_MERC0 || a.partyTag >= party::PT_MERC0 + party::MERC_MAX) continue;
      const float d = len2(a.p - g.pl().p);
      if (d < bd) { bd = d; best = (int)i; }
    }
    if (best < 0) { c.fail("party merc: no sellsword here"); return true; }
    g.gold = std::max(g.gold, 500);
    if (g.partyRecruit(g.actors[(size_t)best], party::Origin::Mercenary) < 0) c.fail("party merc: not hired");
    return true;
  }
  if (what == "pup") {   // party pup [hound|wolf]
    std::string why;
    if (g.partyAdoptAnimal(lw(c.arg(2)) == "wolf" ? 1 : 0, 0xB0B5u + (uint32_t)g.party.nextId, g.pl().p + Vec2(-14, 4), why) < 0) c.fail("party pup: " + why);
    return true;
  }
  if (what == "ally" || what == "voc") {   // party ally <vocation> [bond]: a saga's ally of that vocation joins; party voc <vocation> [m]
    static const char* const vw[] = {"warrior", "archer", "mage", "healer", "rogue", "hound"};
    int v = -1;
    for (int k = 0; k < (int)party::Vocation::COUNT; k++) if (lw(c.arg(2)) == vw[k]) v = k;
    if (v < 0) { c.fail("party " + what + ": unknown vocation " + c.arg(2)); return true; }
    int m = -1;
    if (what == "ally") {
      if (g.plLevel < party::LEADERSHIP_LEVEL && g.party.people() >= party::capacity(g.plLevel)) g.plLevel = party::LEADERSHIP_LEVEL;
      const uint32_t seed = 0xA11E0u + (uint32_t)g.party.nextId * 977u;
      Rng rr(seed);
      const bool female = rr.f() < 0.45f;
      std::string name = "TESTER";
      if (const cult::Culture* cu = g.world.cultureOf(g.world.startSite)) name = cult::personName(*cu, seed, female);
      m = g.partyRecruitStranger(name, female, g.world.sites[(size_t)g.world.startSite].culture, seed, party::Origin::Saga, g.pl().p + Vec2(18, 8));
      if (m < 0) { c.fail("party ally: the party is full"); return true; }
      if (!c.arg(3).empty()) g.party.members[(size_t)m].bond = (uint8_t)std::clamp(std::atoi(c.arg(3).c_str()), 0, 100);
    } else m = memberArg(c, 3);
    if (m < 0) return true;
    party::Member& M = g.party.members[(size_t)m];
    M.voc = (party::Vocation)v;
    M.inv.clear();
    M.eq.fill(-1);
    PartyOps::kit(g, M, std::max(1, g.plLevel));
    const int ai = actorOf(g, m);
    if (ai > 0) {
      Actor& a = g.actors[(size_t)ai];
      a.human = M.voc != party::Vocation::Hound;
      if (a.human) a.look = g.partyLook(M);
      PartyOps::body(g, M, a);
      a.hp = a.maxHp;
    }
    return true;
  }
  if (what == "dismiss") {   // party dismiss [m]: parting for good
    const int m = memberArg(c, 2);
    if (m >= 0) g.partyDismiss(m);
    return true;
  }
  if (what == "horse") {   // party horse [m]: the member gets a horse of the player's breed (or the common one)
    const int m = memberArg(c, 2);
    if (m < 0) return true;
    std::string why;
    g.gold = std::max(g.gold, 5000);
    if (!g.partyBuyHorse(m, g.home.ridingBreed, why)) c.fail("party horse: " + why);
    return true;
  }
  if (what == "bind") {   // party bind [m]: the player steps up to the downed member and starts binding their wounds
    const int m = memberArg(c, 2);
    if (m < 0) return true;
    const int ai = actorOf(g, m);
    if (ai < 0 || g.party.members[(size_t)m].state != party::State::Downed) { c.fail("party bind: not downed"); return true; }
    g.pl().p = g.actors[(size_t)ai].p + Vec2(-13, 1);
    g.pl().face = 2; g.pl().aim = Vec2(1, 0);
    g.party.members[(size_t)m].reviveT = 0.001f;
    return true;
  }
  if (what == "grave") {   // party grave [flowers] [i]: stand before a grave (flowers: lay them)
    const int i = c.arg(c.arg(2) == "flowers" ? 3 : 2).empty() ? 0 : std::atoi(c.arg(c.arg(2) == "flowers" ? 3 : 2).c_str());
    if (i < 0 || i >= (int)g.party.fallen.size()) { c.fail("party grave: no grave " + std::to_string(i)); return true; }
    const party::Fallen& f = g.party.fallen[(size_t)i];
    if (lw(c.arg(2)) == "flowers") { g.partyGraveAct(i, 1); return true; }
    g.teleportGlobal(f.gx, f.gy + 2);
    // a step to the side of it and below, looking at it (the stone and its flowers in sight, not behind the player)
    g.pl().p = Vec2((f.gx - g.world.ox + 1) * (float)TILE + 4.0f, (f.gy - g.world.oy + 1) * (float)TILE + 10.0f);
    g.pl().face = 3; g.pl().aim = norm(Vec2(-1, -0.6f));
    return true;
  }
  if (what == "house") {   // party house: a cottage built at once on a made-up lot beside the player
    const int32_t px = g.world.ox + (int32_t)(g.pl().p.x / TILE), py = g.world.oy + (int32_t)(g.pl().p.y / TILE);
    const int pi = home::debugLot(g, px + 3, py - 6, home::LotSize::Medium);
    if (pi < 0) { c.fail("party house: no lot"); return true; }
    const uint64_t cul = g.world.src->cultureAt(px, py);
    if (cul && !g.home.knowsStyle(cul)) g.home.styles.push_back(cul);
    g.gold = std::max(g.gold, 100000);
    std::string why;
    if (!home::startBuild(g, pi, home::Shell::Cottage, cul, 0, 0, why)) { c.fail("party house: " + why); return true; }
    g.update(SIM_DT, Input());
    g.day += home::shellInfo(home::Shell::Cottage).days;
    g.update(SIM_DT, Input());
    return true;
  }
  c.fail("party: unknown " + c.arg(1));
  return true;
}
EMB_SCRIPT_CMD("party","party recruit [n] | cmd ORDER [m] | deed WORD [w] | down|kill|revive|near|talk|burn [m] | approval V [m] | bond V [m] | stats: stage M8 companions", cmdParty);

bool expParty(ScriptCtx& c) {
  Game& g = c.game;
  const std::string what = lw(c.arg(2));
  auto num = [&](size_t i) { return std::atoi(c.arg(i).c_str()); };
  if (what == "members") { if ((int)g.party.members.size() != num(3)) c.fail("expect party members: " + std::to_string(g.party.members.size())); return true; }
  if (what == "with") { if (g.party.with() != num(3)) c.fail("expect party with: " + std::to_string(g.party.with())); return true; }
  if (what == "fallen") { if ((int)g.party.fallen.size() != num(3)) c.fail("expect party fallen: " + std::to_string(g.party.fallen.size())); return true; }
  if (what == "banter") { if (g.party.lastBanter.empty()) c.fail("expect party banter: nothing said"); return true; }
  if (what == "share") {
    const double tot = g.party.stats.dmgPlayer + g.party.stats.dmgParty;
    const int pct = tot > 0 ? (int)std::lround(100.0 * g.party.stats.dmgParty / tot) : 0;
    if (pct < num(3) || pct > num(4)) c.fail("expect party share: " + std::to_string(pct) + " %");
    return true;
  }
  if (what == "state" || what == "approval" || what == "near") {
    const int m = memberArg(c, what == "state" ? 4 : what == "approval" ? 5 : 4);
    if (m < 0) return true;
    const party::Member& M = g.party.members[(size_t)m];
    if (what == "state") {
      const int s = stateWord(lw(c.arg(3)));
      if (s < 0 || (int)M.state != s) c.fail(std::string("expect party state: ") + party::stateName(M.state));
    } else if (what == "approval") {
      if (M.approval < num(3) || M.approval > num(4)) c.fail("expect party approval: " + std::to_string(M.approval));
    } else {
      const int ai = actorOf(g, m);
      const float d = ai > 0 ? len(g.actors[(size_t)ai].p - g.pl().p) / TILE : 1e9f;
      if (d > (float)num(3)) c.fail("expect party near: " + std::to_string(d) + " tiles");
    }
    return true;
  }
  if (what == "posture" || what == "mounted") {   // expect party posture <downed|kneel|sleep|sit|ride|none> [m] | mounted [m]
    static const char* const pw[] = {"none", "downed", "kneel", "sleep", "sit", "ride"};
    static const art::Posture pp[] = {art::Posture::None, art::Posture::Downed, art::Posture::Kneel, art::Posture::Sleep, art::Posture::Sit, art::Posture::Ride};
    const int m = memberArg(c, what == "mounted" ? 3 : 4);
    if (m < 0) return true;
    const int ai = actorOf(g, m);
    if (ai < 0) { c.fail("expect party " + what + ": not in play"); return true; }
    art::Posture want = art::Posture::Ride;
    if (what == "posture") {
      int k = -1;
      for (int i = 0; i < 6; i++) if (lw(c.arg(3)) == pw[i]) k = i;
      if (k < 0) { c.fail("expect party posture: unknown " + c.arg(3)); return true; }
      want = pp[k];
    }
    const art::Posture got = g.actors[(size_t)ai].posture;
    const bool seated = want == art::Posture::Sit && (got == art::Posture::Sit || got == art::Posture::SitEat || got == art::Posture::SitFloor);
    if (got != want && !seated) c.fail("expect party " + what + ": posture " + std::to_string((int)got));
    return true;
  }
  if (what == "kneel") { if (g.pl().posture != art::Posture::Kneel) c.fail("expect party kneel: the player is not kneeling"); return true; }
  if (what == "grave") {   // expect party grave [home]: a grave stands in the world (in the home village)
    bool ok = false;
    for (const party::Fallen& f : g.party.fallen)
      if ((f.graveFlags & party::GF_SITED) && (lw(c.arg(3)) != "home" || (f.graveFlags & party::GF_HOME))) ok = true;
    if (!ok) c.fail("expect party grave: none");
    return true;
  }
  if (what == "mercs") {
    int n = 0;
    for (const Actor& a : g.actors) n += a.partyTag >= party::PT_MERC0 && a.partyTag < party::PT_MERC0 + party::MERC_MAX;
    // (fixer M8 r3) "expect party mercs N exact": exactly N (a dead sellsword's place at the inn stays empty)
    if (n < num(3) || (lw(c.arg(4)) == "exact" && n != num(3))) c.fail("expect party mercs: " + std::to_string(n));
    return true;
  }
  c.fail("expect party: unknown " + c.arg(2));
  return true;
}
EMB_SCRIPT_CMD("expect:party", "expect party members N | with N | fallen N | state S [m] | approval MIN MAX [m] | near TILES [m] | banter | share LO HI", expParty);

}  // namespace
