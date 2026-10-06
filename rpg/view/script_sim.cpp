// M2 SIM lane: script commands for travel, the new quest types and the wayside places (rpg/view/script_api.h;
// `embervale --script-help` lists them). Test only: none of them touches a save.
//   travel <name words> | travel town|village|city <tiles east> | travel quest|start   [carriage]
//                               start a journey behind the fade (Game::beginTravel) to that place (made discovered)
//   waittravel                  script time stands still until the journey has arrived (prints what it cost)
//   expect travel done          the last journey arrived with no chunk generated on the main thread at the arrival
//   expect quote ok|refused [words] <place>   (place as for travel) the travel quote's verdict and its reason
//   offerquest deliver|heirloom|missing|named|protect|clear|bounty|hunt
//                               the nearest person who can offers that job as a dialogue (choose I'LL DO IT)
//   questgo                     to the tracked quest's objective: the recipient's door (inside), the cave or ruin
//                               (inside), the bandit camp's edge, the fields
//   questchest                  inside: stand beside the tracked Heirloom's chest, facing it (then key E)
//   talkquest                   talk to the tracked quest's person (the parcel's recipient, the missing person)
//   killquest                   the tracked quest's raiders (a Protect wave) or named chief fall
//   dawn                        the morning after the tracked Protect night
//   wayside caravan|hunter|stones|watchtower|fishing|toll|herbs|grave|wayrest   stand at the south edge of the nearest
//                               such place (from the region plans; the WORLD lane places them)
//   waysideheart                stand just south of the place's heart (the heart stone, the grave), facing it
//   onbridge                    step onto the toll bridge (the troll stops whoever has not paid today)
//   talkkeeper                  talk to the place's keeper (the hunter, the fisher, the herbalist, the traveller)
//   expect wayside blessed|tollpaid|troll hostile|ambush   the stones' blessing / the toll paid today / the troll
//                               fights / the caravan's ambush is up
//   expect rumoured [n]         at least n places are rumoured (heard of, not found)
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include "rpg/sim/game.h"
#include "rpg/view/script_api.h"
#include "rpg/view/view.h"
#include "rpg/world/poi.h"
#include "rpg/world/source.h"

namespace {

std::string up(std::string s) {
  for (char& c : s) c = (char)std::toupper((unsigned char)c);
  return s;
}
void playerGlobal(Game& g, int32_t& gx, int32_t& gy) {
  gx = g.world.ox + (int)std::floor(g.pl().p.x / TILE);
  gy = g.world.oy + (int)std::floor(g.pl().p.y / TILE);
  if (g.inside && g.subBldg >= 0) { gx = g.world.ox + g.world.over.bldgs[(size_t)g.subBldg].doorX(); gy = g.world.oy + g.world.over.bldgs[(size_t)g.subBldg].doorY() + 1; }
  if (g.inside && g.subSite >= 0) { gx = g.world.ox + g.world.sites[(size_t)g.subSite].ex; gy = g.world.oy + g.world.sites[(size_t)g.subSite].ey; }
}

// the place a travel / quote line names (from word `from` on; a trailing "carriage" is not part of it)
int placeOf(ScriptCtx& c, size_t from, bool& carriage) {
  Game& g = c.game;
  std::vector<std::string> w;
  for (size_t k = from; k < c.a.size(); k++) w.push_back(c.a[k]);
  carriage = !w.empty() && up(w.back()) == "CARRIAGE";
  if (carriage) w.pop_back();
  if (w.empty()) return -1;
  const std::string first = up(w[0]);
  int32_t px, py;
  playerGlobal(g, px, py);
  if (first == "QUEST") { const Quest* q = g.questById(g.trackedQuest); return q ? q->target : -1; }
  if (first == "START") return g.world.startSite;
  if (first == "TOWN" || first == "VILLAGE" || first == "CITY") {
    const int far = w.size() > 1 ? std::atoi(w[1].c_str()) : 400;
    const SiteType t = first == "TOWN" ? SiteType::Town : first == "CITY" ? SiteType::City : SiteType::Village;
    return g.world.findSiteNear(px + far, py + far / 6, t, 4);
  }
  std::string name;
  for (size_t k = 0; k < w.size(); k++) name += (k ? " " : "") + up(w[k]);
  for (int i = 0; i < (int)g.world.sites.size(); i++) if (g.world.sites[(size_t)i].name.find(name) != std::string::npos) return i;
  return -1;
}

bool cmdTravel(ScriptCtx& c) {
  Game& g = c.game;
  bool carriage = false;
  const int si = placeOf(c, 1, carriage);
  if (si < 0) { c.fail("travel: no such place: " + c.rest(1)); return true; }
  g.world.sites[(size_t)si].discovered = true;
  const TravelQuote q = g.travelQuote(si, carriage);
  std::printf("script: travel to %s%s: %s, %.1f h, %d gold%s%s\n", g.world.sites[(size_t)si].name.c_str(), carriage ? " by carriage" : "",
              q.ok ? "ok" : "refused", q.hours, q.gold, q.why.empty() ? "" : ": ", q.why.c_str());
  if (g.mode != Mode::Play && g.mode != Mode::Menu) g.mode = Mode::Play;
  if (!g.beginTravel(si, carriage)) c.fail("travel refused: " + q.why);
  return true;
}
EMB_SCRIPT_CMD("travel", "travel <name words> | town|village|city <tiles east> | quest | start [carriage]: a journey behind the fade", cmdTravel);

bool cmdWaitTravel(ScriptCtx& c) {
  Game& g = c.game;
  if (g.travelling()) return false;
  const Travel& t = g.lastTravel;
  std::printf("script: journey done: %s, %.1f h, gathered in %.0f ms over %d steps, arrival step %.1f ms, worst travel step %.1f ms, "
              "%d chunks generated at the arrival\n",
              t.site >= 0 && t.site < (int)g.world.sites.size() ? g.world.sites[(size_t)t.site].name.c_str() : "?", t.hours, t.gatherMs, t.steps,
              t.arriveMs, t.worstStepMs, t.syncChunks);
  return true;
}
ScriptExt waitTravelExt() { return ScriptExt{"waittravel", "waittravel: script time waits until the journey has arrived", cmdWaitTravel, 30}; }
const int kWaitTravelReg = registerScriptExt(waitTravelExt());

bool expTravel(ScriptCtx& c) {
  Game& g = c.game;
  if (c.arg(2) == "done") {
    if (g.travelling()) c.fail("still on the road");
    else if (g.lastTravel.site < 0) c.fail("no journey has arrived");
    else if (g.lastTravel.syncChunks > 0) c.fail(std::to_string(g.lastTravel.syncChunks) + " chunks generated on the main thread at the arrival");
  } else c.fail("expect travel done");
  return true;
}
EMB_SCRIPT_CMD("expect:travel", "expect travel done: the last journey arrived with 0 chunks generated on the main thread", expTravel);

bool expQuote(ScriptCtx& c) {
  Game& g = c.game;
  const std::string want = c.arg(2);
  // the words before the place: a refusal reason to look for (up to the first word naming a place)
  size_t k = 3;
  std::string why;
  while (k < c.a.size()) {
    const std::string w = up(c.a[k]);
    if (w == "TOWN" || w == "VILLAGE" || w == "CITY" || w == "QUEST" || w == "START" || w == "AT") break;
    why += (why.empty() ? "" : " ") + w;
    k++;
  }
  if (k < c.a.size() && up(c.a[k]) == "AT") k++;
  bool carriage = false;
  const int si = placeOf(c, k, carriage);
  if (si < 0) { c.fail("expect quote: no such place"); return true; }
  g.world.sites[(size_t)si].discovered = true;
  const TravelQuote q = g.travelQuote(si, carriage);
  std::printf("script: quote %s%s: %s %.1f h %d gold %s\n", g.world.sites[(size_t)si].name.c_str(), carriage ? " by carriage" : "", q.ok ? "ok" : "refused",
              q.hours, q.gold, q.why.c_str());
  if (want == "ok" && !q.ok) c.fail("refused: " + q.why);
  else if (want == "refused" && q.ok) c.fail("the journey was allowed");
  else if (want == "refused" && !why.empty() && q.why.find(why) == std::string::npos) c.fail("refused for '" + q.why + "', not '" + why + "'");
  return true;
}
EMB_SCRIPT_CMD("expect:quote", "expect quote ok|refused [reason words] [at] <place> [carriage]: the travel quote", expQuote);

QType typeOf(const std::string& w) {
  static const struct { const char* n; QType t; } k[] = {{"deliver", QType::Deliver}, {"heirloom", QType::Heirloom}, {"missing", QType::Missing},
                                                        {"named", QType::NamedBandit}, {"namedbandit", QType::NamedBandit}, {"protect", QType::Protect},
                                                        {"clear", QType::Clear}, {"bounty", QType::Bounty}, {"hunt", QType::Hunt}};
  for (auto& e : k) if (w == e.n) return e.t;
  return QType::COUNT;
}

bool cmdOfferQuest(ScriptCtx& c) {
  Game& g = c.game;
  const QType t = typeOf(c.arg(1));
  if (t == QType::COUNT) { c.fail("offerquest deliver|heirloom|missing|named|protect|clear|bounty|hunt"); return true; }
  std::vector<std::pair<float, int>> people;
  for (size_t k = 1; k < g.actors.size(); k++) {
    const Actor& a = g.actors[k];
    if (!a.npc || !a.human || a.st == AState::Dead || a.role == Role::Guard || a.role == Role::Child || a.role == Role::King || a.site < 0) continue;
    people.push_back({len2(a.p - g.pl().p), a.id});
  }
  std::sort(people.begin(), people.end());
  for (auto& p : people)
    if (g.debugOfferTalk(p.second, t)) {
      std::printf("script: offerquest %s from %s: %s\n", c.arg(1).c_str(), g.dlg.speaker.c_str(), g.dlg.text.c_str());
      return true;
    }
  c.fail("offerquest " + c.arg(1) + ": nobody in play offers one");
  return true;
}
EMB_SCRIPT_CMD("offerquest", "offerquest <type>: the nearest person who can offers that job as a dialogue", cmdOfferQuest);

// the quest a command acts on: the tracked one when it is a side job, else the newest open side job (the opening
// keeps the journal's pin until the old blade is collected)
Quest* tracked(Game& g) {
  for (Quest& q : g.quests) if (q.id == g.trackedQuest && q.type != QType::Main && q.type != QType::Retrieve) return &q;
  for (auto it = g.quests.rbegin(); it != g.quests.rend(); ++it)
    if (it->state != QState::Done && it->type != QType::Main && it->type != QType::Retrieve) return &*it;
  return nullptr;
}

bool cmdQuestGo(ScriptCtx& c) {
  Game& g = c.game;
  Quest* q = tracked(g);
  if (!q) { c.fail("questgo: no tracked quest"); return true; }
  g.mode = Mode::Play;
  switch (q->type) {
    case QType::Deliver: {
      if (q->destBldg < 0) {   // the recipient's place: its records come in with the window (the building is found then)
        const Site& s = g.world.sites[(size_t)q->target];
        g.teleportGlobal(g.world.ox + s.ex, g.world.oy + s.ey + 3);
        g.update(SIM_DT, Input());
      }
      if (q->destBldg < 0 || !g.debugEnterBuilding(q->destBldg, 0)) { c.fail("questgo: cannot reach the recipient's building"); return true; }
      break;
    }
    case QType::Heirloom: case QType::Missing:
      if (!g.debugEnterSite(q->target)) { c.fail("questgo: cannot enter the quest's cave or ruin"); return true; }
      break;
    case QType::NamedBandit: case QType::Bounty: case QType::Clear: {
      const Site& s = g.world.sites[(size_t)q->target];
      g.teleportGlobal(g.world.ox + s.ex, g.world.oy + s.r.y + s.r.h + 3);
      g.pl().aim = Vec2(0, -1); g.pl().face = 1;
      break;
    }
    case QType::Protect: g.teleportGlobal(q->tgx, q->tgy + 1); g.pl().aim = Vec2(0, -1); g.pl().face = 1; break;
    default: c.fail("questgo: not for this kind of quest"); return true;
  }
  c.view.snap(g);
  std::printf("script: questgo %s\n", q->title.c_str());
  return true;
}
EMB_SCRIPT_CMD("questgo", "questgo: to the tracked quest's objective (inside a building, cave or ruin where it lies)", cmdQuestGo);

bool cmdQuestChest(ScriptCtx& c) {
  Game& g = c.game;
  const Quest* q = tracked(g);
  if (!g.inside || g.subSite < 0 || !q) { c.fail("questchest: not inside the quest's cave or ruin"); return true; }
  // the chest placed for the keepsake (its tile is in the game's marks)
  int bx = -1, by = -1;
  if (!g.questChestAt(q->id, bx, by)) bx = -1;
  if (bx < 0) { c.fail("questchest: no chest"); return true; }
  static const int sides[4][2] = {{0, 1}, {0, -1}, {1, 0}, {-1, 0}};
  for (auto& s : sides) {
    if (g.sub.blocked(bx + s[0], by + s[1])) continue;
    g.pl().p = Vec2((bx + s[0]) * TILE + 8.0f, (by + s[1]) * TILE + 10.0f);
    g.pl().aim = Vec2((float)-s[0], (float)-s[1]);
    g.pl().face = s[1] > 0 ? 1 : s[1] < 0 ? 0 : (s[0] > 0 ? 3 : 2);
    c.view.snap(g);
    return true;
  }
  c.fail("questchest: the chest is walled in");
  return true;
}
EMB_SCRIPT_CMD("questchest", "questchest: inside, stand beside the tracked Heirloom's chest facing it", cmdQuestChest);

// talk to the actor of this id (stand close and press the interact key for one step)
bool talkTo(Game& g, int id) {
  static const Vec2 offs[] = {{0, 12}, {0, 20}, {10, 0}, {-10, 0}, {0, -10}};
  for (Vec2 o : offs) {
    int k = -1;
    for (size_t j = 1; j < g.actors.size(); j++) if (g.actors[j].id == id) k = (int)j;
    if (k < 0) return false;
    g.mode = Mode::Play;
    g.pl().p = g.actors[(size_t)k].p + o;
    g.pl().aim = norm(g.actors[(size_t)k].p - g.pl().p);
    Input in; in.interact = true;
    g.update(SIM_DT, in);
    if (g.mode == Mode::Dialogue && g.dlg.actor == id) return true;
  }
  return false;
}

bool cmdTalkQuest(ScriptCtx& c) {
  Game& g = c.game;
  const Quest* q = tracked(g);
  if (!q) { c.fail("talkquest: no tracked quest"); return true; }
  int id = -1;
  for (size_t k = 1; k < g.actors.size(); k++) {
    const Actor& a = g.actors[k];
    if (!a.npc || a.st == AState::Dead) continue;
    if (a.quest == q->id || (q->type == QType::Deliver && a.name == q->subject)) id = a.id;
  }
  if (id < 0) { c.fail("talkquest: the quest's person is not here"); return true; }
  if (!talkTo(g, id)) c.fail("talkquest: could not talk");
  c.view.snap(g);
  return true;
}
EMB_SCRIPT_CMD("talkquest", "talkquest: talk to the tracked quest's person (a parcel's recipient, a missing person)", cmdTalkQuest);

bool cmdKillQuest(ScriptCtx& c) {
  Game& g = c.game;
  const Quest* q = tracked(g);
  if (!q) { c.fail("killquest: no tracked quest"); return true; }
  std::vector<int> ids;
  for (size_t k = 1; k < g.actors.size(); k++) {
    const Actor& a = g.actors[k];
    if (a.st == AState::Dead || !a.hostile) continue;
    if (a.quest == q->id || (q->type == QType::NamedBandit && a.boss && a.site == q->target)) ids.push_back(a.id);
  }
  for (int id : ids) g.debugFell(id);
  std::printf("script: killquest: %zu fell\n", ids.size());
  if (ids.empty()) c.fail("killquest: nothing of the quest's to fight here");
  return true;
}
EMB_SCRIPT_CMD("killquest", "killquest: the tracked quest's raiders or named chief fall", cmdKillQuest);

bool cmdKillNear(ScriptCtx& c) {
  Game& g = c.game;
  const float r = (c.a.size() > 1 ? (float)std::atof(c.arg(1).c_str()) : 20.0f) * TILE;
  std::vector<int> ids;
  for (size_t k = 1; k < g.actors.size(); k++) {
    const Actor& a = g.actors[k];
    if (a.st != AState::Dead && a.hostile && len2(a.p - g.pl().p) <= r * r) ids.push_back(a.id);
  }
  for (int id : ids) g.debugFell(id);
  std::printf("script: killnear: %zu fell\n", ids.size());
  return true;
}
// a debug print of the tiles round the player: the level digit, then R rock / ~ water / = road / . other ground,
// and '/' for a ramp, '|' for a cliff face; the player's tile in brackets
bool cmdDumpTiles(ScriptCtx& c) {
  Game& g = c.game;
  const Map& m = g.map();
  const int r = c.a.size() > 1 ? std::atoi(c.arg(1).c_str()) : 10;
  const int px = (int)std::floor(g.pl().p.x / TILE), py = (int)std::floor(g.pl().p.y / TILE);
  for (int y = py - r; y <= py + r; y++) {
    std::string line;
    for (int x = px - r; x <= px + r; x++) {
      const Ground gr = m.at(x, y);
      const uint8_t hb = m.heightBits(x, y);
      char k = gr == Ground::Rock ? 'R' : (gr == Ground::Water || gr == Ground::DeepWater) ? '~' : gr == Ground::Road ? '=' : '.';
      if (hb & Map::HEIGHT_RAMP) k = '/';
      else if (hb & Map::HEIGHT_CLIFF) k = '|';
      line += (x == px && y == py) ? '[' : ' ';
      line += c.arg(2) == "b" ? (char)('0' + (int)m.biomeAt(x, y)) : c.arg(2) == "g" ? (char)('A' + (int)gr) : (char)('0' + (hb & Map::HEIGHT_LEVEL));
      line += k;
    }
    std::printf("tiles %4d: %s\n", y, line.c_str());
  }
  return true;
}
EMB_SCRIPT_CMD("dumptiles", "dumptiles [r] [b|g]: print the levels (or biomes, or ground letters) and grounds of the tiles round the player (debug)", cmdDumpTiles);

EMB_SCRIPT_CMD("killnear","killnear [tiles]: every hostile within that many tiles (default 20) falls", cmdKillNear);

bool cmdDawn(ScriptCtx& c) {
  Game& g = c.game;
  const Quest* q = tracked(g);
  if (!q || q->type != QType::Protect) { c.fail("dawn: the tracked quest is not a Protect"); return true; }
  g.day = q->deadlineDay + 1;
  g.hour = 5.6f;
  return true;
}
EMB_SCRIPT_CMD("dawn", "dawn: the morning after the tracked Protect night", cmdDawn);

// ---- the wayside
int kindOf(const std::string& w) {
  static const char* n[] = {"caravan", "hunter", "stones", "watchtower", "fishing", "toll", "herbs", "grave", "wayrest"};
  for (int i = 0; i < 9; i++) if (w == n[i]) return i;
  return -1;
}
int g_lastWayside = -1;

bool cmdWayside(ScriptCtx& c) {
  Game& g = c.game;
  const int kind = kindOf(c.arg(1));
  if (kind < 0) { c.fail("wayside caravan|hunter|stones|watchtower|fishing|toll|herbs|grave|wayrest"); return true; }
  if (g.inside) g.debugLeave();
  int32_t px, py;
  playerGlobal(g, px, py);
  const int32_t rx0 = ew::regionOf(px), ry0 = ew::regionOf(py);
  ew::Gid best = 0;
  int64_t bd = INT64_MAX;
  for (int r = 0; r <= 14; r++) {   // (toll bridges are rare: a road over a river)
    if (best && (int64_t)(r - 1) * ew::REGION * (r - 1) * ew::REGION > bd) break;
    for (int32_t ry = ry0 - r; ry <= ry0 + r; ry++)
      for (int32_t rx = rx0 - r; rx <= rx0 + r; rx++) {
        if (std::max(std::abs(rx - rx0), std::abs(ry - ry0)) != r) continue;
        const ew::RegionPlan R = g.world.regionPlan(rx, ry);
        for (const ew::SitePlan& p : R.sites) {
          if (p.type != SiteType::Vignette || p.kind != kind) continue;
          const int64_t dx = p.ex - px, dy = p.ey - py, d = dx * dx + dy * dy;
          if (d < bd) { bd = d; best = p.id; }
        }
      }
  }
  const int si = best ? g.world.ensureSite(best) : -1;
  if (si < 0) { c.fail(std::string("wayside: no ") + ew::vignetteName((ew::VignetteKind)kind) + " planned near (the WORLD lane places them)"); return true; }
  const Site s = g.world.sites[(size_t)si];
  const int32_t sgx = g.world.ox + s.ex, sgy = g.world.oy + s.ey;
  g.teleportGlobal(g.world.ox + s.ex, g.world.oy + s.r.y + s.r.h + 1);
  g.pl().aim = Vec2(0, -1); g.pl().face = 1;
  g.mode = Mode::Play;
  g_lastWayside = si;
  c.view.snap(g);
  std::printf("script: wayside %s -> %s at %d,%d (%d tiles away)\n", c.arg(1).c_str(), s.name.c_str(), sgx, sgy, (int)std::sqrt((double)bd));
  return true;
}
EMB_SCRIPT_CMD("wayside", "wayside <kind>: stand at the south edge of the nearest wayside place of that kind", cmdWayside);

bool cmdWaysideHeart(ScriptCtx& c) {
  Game& g = c.game;
  if (g_lastWayside < 0) { c.fail("waysideheart: no wayside place (use wayside first)"); return true; }
  const Site& s = g.world.sites[(size_t)g_lastWayside];
  // the free tile nearest the heart, south of it first, facing the heart
  static const int sides[4][2] = {{0, 1}, {1, 0}, {-1, 0}, {0, -1}};
  for (int r = 1; r <= 2; r++)
    for (auto& sd : sides) {
      const int x = s.ex + sd[0] * r, y = s.ey + sd[1] * r;
      if (g.world.over.blocked(x, y)) continue;
      g.pl().p = Vec2(x * TILE + 8.0f, y * TILE + 10.0f);
      g.pl().aim = Vec2((float)-sd[0], (float)-sd[1]);
      g.pl().face = sd[1] > 0 ? 1 : sd[1] < 0 ? 0 : (sd[0] > 0 ? 3 : 2);
      c.view.snap(g);
      return true;
    }
  c.fail("waysideheart: no free tile beside the heart");
  return true;
}
EMB_SCRIPT_CMD("waysideheart", "waysideheart: stand beside the wayside place's heart (the heart stone, the grave), facing it", cmdWaysideHeart);

bool cmdOnBridge(ScriptCtx& c) {
  Game& g = c.game;
  if (g_lastWayside < 0) { c.fail("onbridge: use wayside toll first"); return true; }
  const Site& s = g.world.sites[(size_t)g_lastWayside];
  int bx = -1, by = -1, bd = 1 << 30;
  for (int y = s.r.y - 4; y < s.r.y + s.r.h + 4; y++)
    for (int x = s.r.x - 4; x < s.r.x + s.r.w + 4; x++)
      if (g.world.over.at(x, y) == Ground::Bridge) {
        const int d = std::abs(x - s.ex) + std::abs(y - s.ey);
        if (d < bd) { bd = d; bx = x; by = y; }
      }
  if (bx < 0) { c.fail("onbridge: no bridge at " + s.name); return true; }
  g.pl().p = Vec2(bx * TILE + 8.0f, by * TILE + 10.0f);
  c.view.snap(g);
  return true;
}
EMB_SCRIPT_CMD("onbridge", "onbridge: step onto the toll bridge of the wayside place", cmdOnBridge);

bool cmdTalkKeeper(ScriptCtx& c) {
  Game& g = c.game;
  int id = -1;
  float bd = 1e30f;
  for (size_t k = 1; k < g.actors.size(); k++) {
    const Actor& a = g.actors[k];
    if (!a.npc || a.st == AState::Dead || a.site < 0 || g.world.sites[(size_t)a.site].type != SiteType::Vignette) continue;
    const float d = len2(a.p - g.pl().p);
    if (d < bd) { bd = d; id = a.id; }
  }
  if (id < 0) { c.fail("talkkeeper: no wayside keeper in play"); return true; }
  if (!talkTo(g, id)) c.fail("talkkeeper: could not talk");
  c.view.snap(g);
  return true;
}
EMB_SCRIPT_CMD("talkkeeper", "talkkeeper: talk to the nearest wayside keeper (hunter, fisher, herbalist, traveller, troll)", cmdTalkKeeper);

bool expWayside(ScriptCtx& c) {
  Game& g = c.game;
  const std::string w = c.arg(2);
  if (w == "blessed") { if (!(g.blessT > 0 && g.blessName == "BLESSING OF THE STONES")) c.fail("no blessing of the stones"); }
  else if (w == "tollpaid") { if (g_lastWayside < 0 || !g.tollPaid(g_lastWayside)) c.fail("the toll is not paid"); }
  else if (w == "troll") {
    bool hostile = false;
    for (const Actor& a : g.actors) if (a.mon == art::Monster::Troll && !a.human && a.site == g_lastWayside && a.hostile) hostile = true;
    if (!hostile) c.fail("the troll does not fight");
  } else if (w == "ambush") {
    int n = 0;
    for (const Actor& a : g.actors) if (a.hostile && a.human && a.site == g_lastWayside && a.st != AState::Dead) n++;
    if (!n) c.fail("no ambush");
  } else c.fail("expect wayside blessed|tollpaid|troll hostile|ambush");
  return true;
}
EMB_SCRIPT_CMD("expect:wayside", "expect wayside blessed|tollpaid|troll hostile|ambush", expWayside);

bool expRumoured(ScriptCtx& c) {
  Game& g = c.game;
  const int want = c.arg(2).empty() ? 1 : std::atoi(c.arg(2).c_str());
  int n = 0;
  for (const Site& s : g.world.sites) if (s.rumoured) n++;
  if (n < want) c.fail("rumoured places: " + std::to_string(n) + " (want " + std::to_string(want) + ")");
  return true;
}
EMB_SCRIPT_CMD("expect:rumoured", "expect rumoured [n]: at least n places are heard of but not found", expRumoured);

}  // namespace

namespace {
bool cmdGold(ScriptCtx& c) {
  c.game.gold = std::max(0, std::atoi(c.arg(1).c_str()));
  return true;
}
EMB_SCRIPT_CMD("gold", "gold N: the purse holds N gold (test only)", cmdGold);
}  // namespace

namespace {
bool cmdNextDay(ScriptCtx& c) {
  c.game.day++;
  c.game.hour = c.arg(1).empty() ? 8.0f : (float)std::atof(c.arg(1).c_str());
  return true;
}
EMB_SCRIPT_CMD("nextday", "nextday [hour]: the next morning (default 08:00): daily things reset (tolls, blessings, news)", cmdNextDay);
}  // namespace
