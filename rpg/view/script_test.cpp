// M2 phase A: script commands that keep the M0/M1 scripts meaningful on the endless world (the classic island and its
// fixed coordinates and names are retired). Registered through rpg/view/script_api.h; `embervale --script-help`.
//   gate [n] out|on|in          stand outside, in the passage of, or just inside the n-th nearest city gate
//   taplabel                    tap the "TAP: <verb>" label over the usable thing in front of the player
//   givequest bounty|hunt [mon] [n]   the nearest person in play (not a guard or a child) hands the player a bounty on
//                               the nearest bandit camp, or a hunt of n monsters (tracked; the giver gets the "!")
//   gotosite quest|other|start|camp|cave|ruin...   stand at the edge of the tracked quest's site, the nearest bandit camp
//                               that is NOT its target, the start village, or the nearest site of that type
//   fightquest                  spawn what the tracked hunt still needs, around the player
//   expect sidequest active|complete|done [type]   a quest other than the main one and the opening is in that state
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>
#include "rpg/sim/game.h"
#include "rpg/view/script_api.h"
#include "rpg/view/view.h"

namespace {

// the n-th gate nearest the heart of the walled place the player is at (stable while the player moves round it)
int nearestGate(Game& g, int nth) {
  std::vector<std::pair<float, int>> c;
  Vec2 p = g.pl().p * (1.0f / TILE);
  int city = -1;
  float cd = 1e30f;
  for (int i = 0; i < (int)g.world.sites.size(); i++) {
    const Site& s = g.world.sites[(size_t)i];
    if (s.type != SiteType::City && s.type != SiteType::Town) continue;
    float d = std::hypot(s.ex - p.x, s.ey - p.y);
    if (d < cd) { cd = d; city = i; }
  }
  if (city >= 0 && cd < 200) p = Vec2((float)g.world.sites[(size_t)city].ex, (float)g.world.sites[(size_t)city].ey);
  for (size_t i = 0; i < g.world.gates.size(); i++) {
    const auto& gt = g.world.gates[i];
    const float d = std::hypot(gt.first + 1.5f - p.x, gt.second - p.y);
    if (d > 160.0f) continue;   // (fixer r2) another settlement's gate is not this one's
    // (fixer round 3) a seat of power's compound gate (the palace forecourt's gatehouse, a few tiles before the seat's
    // door) is no city gate: `gate out` there left the player inside the compound among its pools and statues
    bool compound = false;
    for (const Bldg& B : g.world.over.bldgs)
      if ((bldgIsSeat(B) || (B.civic & bld::CIVIC_SEAT)) && std::abs(B.doorX() - (gt.first + 1)) <= 3 && gt.second - B.doorY() > 0 &&
          gt.second - B.doorY() <= 24)
        compound = true;
    if (compound) continue;
    c.push_back({d, (int)i});
  }
  std::sort(c.begin(), c.end());
  return nth < (int)c.size() ? c[(size_t)nth].second : -1;
}

bool cmdGate(ScriptCtx& c) {
  Game& g = c.game;
  const int nth = c.arg(1).empty() || !std::isdigit((unsigned char)c.arg(1)[0]) ? 0 : std::atoi(c.arg(1).c_str());
  const std::string where = c.arg(c.arg(1).empty() || !std::isdigit((unsigned char)c.arg(1)[0]) ? 1 : 2);
  // the same gate for out / on / in (remembered by its global tile: the window moves under the teleports)
  static int lastN = -1;
  static int32_t lastGX = 0, lastGY = 0;
  // (fixer round 3) `gate forget`: drop the remembered gate (the next `gate 0` picks the nearest afresh). Scripts used
  // `gate 1 on` for that, which fails where a city has a single gate now that a seat compound's gate is no city gate
  if (c.arg(1) == "forget") { lastN = -1; std::printf("script: gate: forgotten\n"); return true; }
  int gi = -1;
  // (fixer r2) only while that gate is still near: after a teleport to another settlement the gate there is meant
  const float ptx = g.pl().p.x / TILE, pty = g.pl().p.y / TILE;
  if (lastN == nth)
    for (size_t i = 0; i < g.world.gates.size(); i++)
      if (g.world.gates[i].first + g.world.ox == lastGX && g.world.gates[i].second + g.world.oy == lastGY &&
          std::hypot(g.world.gates[i].first - ptx, g.world.gates[i].second - pty) < 120.0f)
        gi = (int)i;
  if (gi < 0) gi = nearestGate(g, nth);
  if (gi >= 0) { lastN = nth; lastGX = g.world.gates[(size_t)gi].first + g.world.ox; lastGY = g.world.gates[(size_t)gi].second + g.world.oy; }
  if (gi < 0) { c.fail("gate: no city gate near (goto a walled city first)"); return true; }
  const int gx = g.world.gates[(size_t)gi].first + 1, gy = g.world.gates[(size_t)gi].second;
  // which side is outside: away from the city the gate belongs to
  int site = g.world.siteAt(gx, gy, 4);
  bool southOut = true;
  if (site >= 0) southOut = gy > g.world.sites[(size_t)site].r.cy();
  const int out = southOut ? 1 : -1;
  int ty = gy;
  if (where == "out" || where.empty()) ty = gy + 3 * out;
  else if (where == "in") ty = gy - 3 * out;
  else if (where != "on") { c.fail("gate: out|on|in, not '" + where + "'"); return true; }
  const int32_t ggx = g.world.ox + gx, ggy = g.world.oy + ty;   // (global: the teleport moves the window)
  g.teleportGlobal(ggx, ggy);
  g.pl().aim = Vec2(0, (float)-out);
  g.pl().face = out > 0 ? 1 : 0;
  g.mode = Mode::Play;
  c.view.snap(g);
  std::printf("script: gate %d %s at global %d,%d\n", nth, where.c_str(), ggx, ggy);
  return true;
}
EMB_SCRIPT_CMD("gate", "gate [n] out|on|in: stand outside, in the passage of, or inside the n-th nearest city gate (gate forget: drop the remembered one)", cmdGate);

bool cmdTapLabel(ScriptCtx& c) {
  Game& g = c.game;
  int tx = 0, ty = 0;
  if (!g.interactProp(tx, ty)) { c.fail("taplabel: nothing usable in front of the player"); return true; }
  const Vec2 cam(std::floor(c.view.camera().x), std::floor(c.view.camera().y));
  const Vec2 s = Vec2(tx * 16 + 8.0f, ty * 16 - 4.0f) - cam;   // hud.cpp: the label is drawn at s.y - 10
  std::printf("script: taplabel at %.0f,%.0f (%s half)\n", s.x, s.y - 10, s.x < Pix::W / 2 ? "the stick's" : "the buttons'");
  c.tap(s.x, s.y - 10);
  return true;
}
EMB_SCRIPT_CMD("taplabel", "taplabel: tap the TAP: <verb> label over the usable thing in front of the player", cmdTapLabel);

int nearestPerson(Game& g) {
  int best = -1;
  float bd = 1e30f;
  for (size_t k = 1; k < g.actors.size(); k++) {
    const Actor& a = g.actors[k];
    if (!a.npc || a.st == AState::Dead || a.role == Role::Guard || a.role == Role::Child || a.role == Role::King) continue;
    float d = len2(a.p - g.pl().p);
    if (d < bd) { bd = d; best = (int)k; }
  }
  return best;
}
int siteTypeByName(const std::string& n, SiteType& t) {
  static const struct { const char* n; SiteType t; } k[] = {{"camp", SiteType::BanditCamp}, {"cave", SiteType::Cave}, {"ruin", SiteType::Ruin},
                                                           {"shrine", SiteType::Shrine}, {"village", SiteType::Village}, {"town", SiteType::Town},
                                                           {"city", SiteType::City}, {"lair", SiteType::DragonLair}, {"vignette", SiteType::Vignette},
                                                           {"wonder", SiteType::Wonder}};
  for (auto& e : k) if (n == e.n) { t = e.t; return 1; }
  return 0;
}
int nearestOfType(Game& g, SiteType t, int exclude) {
  const Vec2 p = g.pl().p * (1.0f / TILE);
  int best = -1;
  float bd = 1e30f;
  for (int i = 0; i < (int)g.world.sites.size(); i++) {
    const Site& s = g.world.sites[(size_t)i];
    if (s.type != t || i == exclude) continue;
    float d = std::hypot(s.ex - p.x, s.ey - p.y);
    if (d < bd) { bd = d; best = i; }
  }
  if (best < 0) best = g.world.findSiteNear(g.world.ox + (int)p.x, g.world.oy + (int)p.y, t, 6);
  return best;
}

bool cmdGiveQuest(ScriptCtx& c) {
  Game& g = c.game;
  const int k = nearestPerson(g);
  if (k < 0) { c.fail("givequest: nobody in play to give it"); return true; }
  const Actor giver = g.actors[(size_t)k];
  Quest q;
  q.id = g.nextQuestId++;
  q.state = QState::Active;
  q.giverName = giver.name; q.giverSite = giver.site; q.giverBldg = giver.bldg; q.giverSlot = giver.slot;
  if (c.arg(1) == "bounty") {
    const int camp = nearestOfType(g, SiteType::BanditCamp, -1);
    if (camp < 0) { c.fail("givequest bounty: no bandit camp near"); return true; }
    q.type = QType::Bounty;
    q.target = camp;
    q.title = "BOUNTY: " + g.world.sites[(size_t)camp].name;
    q.desc = "THE CHIEF OF " + g.world.sites[(size_t)camp].name + " HAS A PRICE ON HIS HEAD.";
    q.gold = 150; q.xp = 60;
  } else if (c.arg(1) == "hunt") {
    static const char* names[] = {"wolf", "boar", "bear", "slime", "spider", "bat", "skeleton", "draugr", "goblin", "troll"};
    q.type = QType::Hunt;
    q.mon = art::Monster::Boar;
    for (int m = 0; m < 10; m++) if (c.arg(2) == names[m]) q.mon = (art::Monster)m;
    q.need = c.arg(3).empty() ? 6 : std::max(1, std::atoi(c.arg(3).c_str()));
    q.title = "THE HUNT";
    q.desc = "KILL " + std::to_string(q.need) + " OF THEM.";
    q.gold = 90; q.xp = 50;
  } else { c.fail("givequest: bounty|hunt"); return true; }
  g.quests.push_back(q);
  g.trackedQuest = q.id;
  std::printf("script: givequest %s from %s: %s\n", c.arg(1).c_str(), giver.name.c_str(), q.title.c_str());
  return true;
}
EMB_SCRIPT_CMD("givequest", "givequest bounty | hunt [monster] [n]: the nearest person hands over a tracked quest", cmdGiveQuest);

bool cmdGotoSite(ScriptCtx& c) {
  Game& g = c.game;
  const Quest* tq = g.questById(g.trackedQuest);
  int si = -1;
  if (c.arg(1) == "quest") si = tq ? tq->target : -1;
  else if (c.arg(1) == "start") si = g.world.startSite;
  else if (c.arg(1) == "other") si = nearestOfType(g, SiteType::BanditCamp, tq ? tq->target : -1);
  else {
    SiteType t;
    if (!siteTypeByName(c.arg(1), t)) { c.fail("gotosite: quest|other|camp|cave|ruin|..."); return true; }
    si = nearestOfType(g, t, -1);
  }
  if (si < 0 || si >= (int)g.world.sites.size()) { c.fail("gotosite " + c.arg(1) + ": none"); return true; }
  if (g.inside) g.debugLeave();
  const Site s = g.world.sites[(size_t)si];
  // the south edge of the place (a camp's chief and a cave's mouth are approached from there)
  // (a camp: just south of its heart, where the chief keeps the fire)
  const int ty = s.type == SiteType::Cave ? s.ey + 2 : s.type == SiteType::BanditCamp ? s.ey + 3 : s.r.y + s.r.h + 2;
  g.teleportGlobal(g.world.ox + s.ex, g.world.oy + ty);
  g.pl().aim = Vec2(0, -1);
  g.pl().face = 1;
  g.mode = Mode::Play;
  c.view.snap(g);
  std::printf("script: gotosite %s -> %s %s\n", c.arg(1).c_str(), siteTypeName(s.type), s.name.c_str());
  return true;
}
EMB_SCRIPT_CMD("gotosite", "gotosite quest|other|start|<type>: stand at the edge of the tracked quest's site / another camp / the start village / the nearest of a type", cmdGotoSite);

bool cmdFightQuest(ScriptCtx& c) {
  Game& g = c.game;
  const Quest* q = g.questById(g.trackedQuest);
  if (!q || q->type != QType::Hunt) { c.fail("fightquest: the tracked quest is not a hunt"); return true; }
  if (g.eqWeapon < 0) g.debugKit();
  g.debugSpawn(q->mon, std::max(1, q->need - q->have), 60);
  return true;
}
EMB_SCRIPT_CMD("fightquest", "fightquest: spawn what the tracked hunt still needs around the player", cmdFightQuest);

bool expSideQuest(ScriptCtx& c) {
  Game& g = c.game;
  static const char* sn[] = {"active", "complete", "done"};
  static const char* tn[] = {"main", "clear", "hunt", "retrieve", "bounty", "deliver", "heirloom", "missing", "namedbandit", "protect"};
  const std::string want = c.arg(2), type = c.arg(3);
  int found = 0;
  for (const Quest& q : g.quests) {
    if (q.type == QType::Main || q.title == "A BLADE OF YOUR OWN") continue;
    if (!type.empty() && ((int)q.type >= 10 || type != tn[(int)q.type])) continue;
    if (want == sn[(int)q.state]) found++;
  }
  if (!found) c.fail("expected a side quest " + want + (type.empty() ? std::string() : " of type " + type));
  return true;
}
EMB_SCRIPT_CMD("expect:sidequest", "expect sidequest active|complete|done [type]: a quest other than the main one and the opening", expSideQuest);

// ---- M2 UI lane: screen-tour helpers (m2_ui_screens.txt). Key presses turn the touch layout off (as a real keyboard
// does), so a tour navigates with keys and then picks the layout to photograph with `touch`.
bool cmdTouch(ScriptCtx& c) {
  c.view.touchUI = c.arg(1) != "0";
  return true;
}
EMB_SCRIPT_CMD("touch", "touch 0|1: the touch layout off / on (as the first finger or key would switch it)", cmdTouch);

bool cmdLevelUp(ScriptCtx& c) {
  Game& g = c.game;
  g.perkPts = std::max(1, g.perkPts);
  g.mode = Mode::LevelUp;
  return true;
}
EMB_SCRIPT_CMD("levelup", "levelup: a perk point waits and the level-up screen opens", cmdLevelUp);

bool cmdDie(ScriptCtx& c) {
  Game& g = c.game;
  g.godMode = false;
  Actor& p = g.pl();
  p.hp = 0;
  p.st = AState::Dead;
  p.stT = 0;
  g.mode = Mode::Dead;
  return true;
}
EMB_SCRIPT_CMD("die", "die: the hero falls where he stands (the death screen)", cmdDie);

bool cmdDialogue(ScriptCtx& c) {   // a long line and n options (default 5): the dialogue panel's worst case
  Game& g = c.game;
  const int n = c.arg(1).empty() ? 5 : std::clamp(std::atoi(c.arg(1).c_str()), 1, 8);
  g.dlg = Dialogue();
  g.dlg.speaker = "OLD BRAN THE HUNTER";
  g.dlg.text = "THE WOLVES HAVE COME DOWN FROM THE HIGH PASS AGAIN, AND THEY ARE BOLDER THIS WINTER THAN ANY I REMEMBER. "
               "THREE OF MY SNARES WERE TORN OPEN LAST NIGHT AND THE TRACKS LEAD EAST, TOWARD THE STANDING STONES. "
               "IF YOU HAVE A STEADY HAND WITH A BOW, I COULD USE IT.";
  static const char* opts[8] = {"I WILL HUNT THEM DOWN FOR YOU.", "WHAT DO YOU KNOW OF THE STANDING STONES?",
                                "WHAT DO YOU HAVE FOR SALE?", "TELL ME ABOUT THE ROAD TO THE NORTH AND THE TOLL BRIDGE ON THE RIVER.",
                                "ANY NEWS FROM THE CAPITAL?", "WHERE CAN I REST TONIGHT?", "WHO ARE YOU?", "FAREWELL."};
  for (int i = 0; i < n; i++) g.dlg.opts.push_back(DlgOpt{opts[i == n - 1 ? 7 : i], 0, 0});
  g.mode = Mode::Dialogue;
  return true;
}
EMB_SCRIPT_CMD("dialogue", "dialogue [n]: a test conversation with a long line and n options (layout checks; choosing closes it)", cmdDialogue);

}  // namespace
