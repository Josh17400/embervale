// The Witness snapshot (witness_state.h). Positions are in tiles of the current map (float; the player's pixel
// position too), so tests can use dist(); state.entities lists the people and beasts within 40 tiles, nearest first.
#ifdef EMB_WITNESS
#include "rpg/view/witness_state.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include "rpg/sim/factions.h"
#include "rpg/sim/game.h"
#include "rpg/view/view.h"

namespace {
const char* roleName(Role r) {
  static const char* n[] = {"Villager", "Guard", "Merchant", "Smith", "Innkeeper", "Priest", "Jarl", "Farmer", "Child", "Mage", "Bandit", "King",
                            "Hunter", "Fisher", "Herbalist", "Traveller", "Herald", "Soldier", "Captain", "Refugee",
                            "Harbourmaster", "Shipwright", "Sailor", "Pirate", "Steward", "Warlord", "Envoy"};
  static_assert(sizeof(n) / sizeof(n[0]) == (size_t)Role::COUNT, "a name for every role");
  return (int)r < (int)Role::COUNT ? n[(int)r] : "Person";
}
// the script driver's `fight` names, capitalised: what count('Wolf') matches
const char* monsterType(art::Monster m) {
  static const char* n[] = {"Wolf", "Boar", "Bear", "Slime", "Spider", "Bat", "Skeleton", "Draugr", "Goblin", "Troll", "Wraith", "Mudcrab",
                            "Icewolf", "Frostspider", "Sandworm", "Dragon", "Scorpion", "Hyena", "Lurker", "Yeti", "Wisp", "Emberhound",
                            "Blightspawn", "Harpy", "Golem", "Seaserpent", "Kraken", "Merrow"};
  static_assert(sizeof(n) / sizeof(n[0]) == (size_t)art::Monster::COUNT, "a type for every monster");
  return (int)m < (int)art::Monster::COUNT ? n[(int)m] : "Monster";
}
const char* modeName(Mode m) {
  static const char* n[] = {"title", "play", "dialogue", "menu", "shop", "levelup", "dead", "paused", "creator", "forge", "build"};
  return (size_t)m < sizeof(n) / sizeof(n[0]) ? n[(int)m] : "other";
}
const char* stateName(AState s) {
  static const char* n[] = {"idle", "walk", "windup", "strike", "recover", "roll", "hurt", "dead", "cast", "down"};
  return (size_t)s < sizeof(n) / sizeof(n[0]) ? n[(int)s] : "other";
}
const char* questState(QState s) { return s == QState::Active ? "active" : s == QState::Complete ? "complete" : "done"; }
// to 1/100 (positions in tiles, hp, the hour): the double nearest the 2-decimal value, so the JSON stays short
double r2(double v) {
  char b[48];
  std::snprintf(b, sizeof b, "%.2f", v);
  return std::strtod(b, nullptr);
}
void tilePos(witness::Json& j, Vec2 p) { j.vec(r2(p.x / TILE), r2(p.y / TILE)); }
}  // namespace

void witnessState(witness::Json& j, const Game& g, const View& v, const WitnessExtra& x) {
  const Actor& P = g.pl();
  j.begin_object();
  j.field("mode", modeName(g.mode)).field("day", g.day).field("hour", r2(g.hour)).field("time", r2(g.time));
  if (g.mode == Mode::Menu) j.field("menu", v.menuTab());
  if (g.mode == Mode::Dialogue) {
    j.key("dialogue").begin_object().field("speaker", g.dlg.speaker).field("text", g.dlg.text.substr(0, 400)).key("options").begin_array();
    for (const DlgOpt& o : g.dlg.opts) j.value(o.label);
    j.end_array().end_object();
  }
  if (g.noticeT > 0 && !g.notice.empty()) j.field("notice", g.notice);

  // the player
  auto eq = [&](int i) -> std::string { return i >= 0 && i < (int)g.inv.size() ? g.inv[(size_t)i].name : std::string(); };
  j.key("player").begin_object().field("id", P.id).field("type", "Player").field("name", g.app.name.empty() ? std::string("YOU") : g.app.name);
  j.key("pos"); tilePos(j, P.p);
  j.key("px").vec(r2(P.p.x), r2(P.p.y));
  if (!g.inside) {
    const int gx = g.world.ox + (int)std::floor(P.p.x / TILE), gy = g.world.oy + (int)std::floor((P.p.y - 2) / TILE);
    j.key("global").vec(gx, gy);
    // tests have no memory between steps: how far the player is from where the reset put them (global tiles)
    if (x.hasStart) j.key("start").vec(x.startGX, x.startGY).field("moved", r2(std::hypot(gx - x.startGX, gy - x.startGY)));
  }
  j.field("hp", r2(P.hp)).field("maxHp", r2(P.maxHp)).field("magicka", r2(g.mp)).field("maxMagicka", r2(g.maxMp))
      .field("stamina", r2(g.stamina)).field("maxStamina", r2(g.maxSt)).field("state", stateName(P.st))
      .field("gold", g.gold).field("level", g.plLevel).field("xp", g.plXp).field("xpNext", g.xpForNext())
      .field("kills", g.kills).field("god", g.godMode).field("inside", g.inside).field("floor", g.subFloor);
  j.key("equipped").begin_object().field("weapon", eq(g.eqWeapon)).field("bow", eq(g.eqBow)).field("armor", eq(g.eqArmor))
      .field("helmet", eq(g.eqHelmet)).field("shield", eq(g.eqShield)).end_object();
  j.field("items", (int)g.inv.size());
  j.end_object();

  // quests (the tracked one first, then by id)
  j.key("quests").begin_array();
  std::vector<const Quest*> qs;
  for (const Quest& q : g.quests) qs.push_back(&q);
  std::stable_sort(qs.begin(), qs.end(), [&](const Quest* a, const Quest* b) { return (a->id == g.trackedQuest) > (b->id == g.trackedQuest); });
  for (size_t i = 0; i < qs.size() && i < 24; i++) {
    const Quest& q = *qs[i];
    j.begin_object().field("id", q.id).field("title", q.title).field("type", (int)q.type).field("state", questState(q.state))
        .field("have", q.have).field("need", q.need).field("giver", q.giverName).field("tracked", q.id == g.trackedQuest).end_object();
  }
  j.end_array();

  // where
  j.key("location").begin_object().field("region", g.locName);
  const int si = g.inside && g.subBldg >= 0 ? g.world.over.bldgs[(size_t)g.subBldg].site : g.inside ? g.subSite : g.curSite;
  if (si >= 0 && si < (int)g.world.sites.size()) {
    const Site& s = g.world.sites[(size_t)si];
    j.field("site", s.name).field("siteType", siteTypeName(s.type)).field("capital", s.capital);
    if (const Kingdom* K = g.world.kingdomOf(si)) j.field("kingdom", K->name);
  }
  j.end_object();
  if (g.mode == Mode::Play || g.mode == Mode::Dialogue) j.field("scene", g.inside ? "inside" : "overworld");

  // the people and beasts within 40 tiles, nearest first (at most 60)
  std::vector<std::pair<float, const Actor*>> near;
  for (size_t k = 1; k < g.actors.size(); k++) {
    const Actor& a = g.actors[k];
    const float d = len(a.p - P.p) / TILE;
    if (d <= 40.0f) near.push_back({d, &a});
  }
  std::stable_sort(near.begin(), near.end(), [](const auto& a, const auto& b) { return a.first < b.first || (a.first == b.first && a.second->id < b.second->id); });
  j.key("entities").begin_array();
  j.begin_object().field("id", P.id).field("type", "Player").key("pos"); tilePos(j, P.p); j.field("hp", r2(P.hp)).end_object();
  int hostiles = 0;
  for (size_t i = 0; i < near.size() && i < 60; i++) {
    const Actor& a = *near[i].second;
    const bool person = a.npc || a.human;
    if (a.hostile && a.st != AState::Dead) hostiles++;
    j.begin_object().field("id", a.id).field("type", person ? roleName(a.role) : monsterType(a.mon))
        .field("kind", a.comp ? "companion" : person ? "person" : "beast").field("name", a.name);
    j.key("pos"); tilePos(j, a.p);
    j.field("dist", r2(near[i].first)).field("hp", r2(a.hp)).field("maxHp", r2(a.maxHp)).field("level", a.level)
        .field("hostile", a.hostile).field("aggro", a.aggro).field("faction", factionName(a.faction)).field("state", stateName(a.st));
    j.end_object();
  }
  j.end_array();

  // counts (as --perf and the CI suites track them) and the harness's own facts
  j.key("counts").begin_object().field("actors", (int)g.actors.size()).field("nearby", (int)near.size()).field("hostilesNearby", hostiles)
      .field("npcAwake", g.perf.npcAwake).field("npcAsleep", g.perf.npcAsleep).field("hostiles", g.perf.hostiles)
      .field("activeSites", g.perf.activeSites).field("pickups", (int)g.pickups.size()).field("projectiles", (int)g.projs.size())
      .field("quests", (int)g.quests.size()).field("dungeonsCleared", g.dungeonsCleared).end_object();
  j.key("script").begin_object().field("busy", x.scriptBusy).field("fails", x.scriptFails).field("lastFail", x.lastFail).end_object();
  j.key("save").begin_object().field("enabled", x.saveEnabled).field("written", x.savesWritten).end_object();
  {   // FNV-1a of witnessDigest: every actor and the clock exactly (two runs that agree here agree on the whole world)
    uint64_t h = 1469598103934665603ull;
    for (unsigned char ch : witnessDigest(g)) { h ^= ch; h *= 1099511628211ull; }
    char b[24];
    std::snprintf(b, sizeof b, "%016llx", (unsigned long long)h);
    j.field("digest", std::string(b));
  }
  j.end_object();
}

std::string witnessDigest(const Game& g) {
  // exact (hex floats): every actor, not only the 60 nearest the snapshot lists, and the clock
  std::string s;
  char b[256];
  std::snprintf(b, sizeof b, "t%a h%a d%d ox%d oy%d in%d site%d n%d|", g.time, g.hour, g.day, g.world.ox, g.world.oy, (int)g.inside, g.curSite,
                (int)g.actors.size());
  s += b;
  for (const Actor& a : g.actors) {
    std::snprintf(b, sizeof b, "%d:%d:%d:%d:%a,%a:%a:%d;", a.id, (int)a.npc, (int)a.role, (int)a.mon, a.p.x, a.p.y, a.hp, (int)a.st);
    s += b;
  }
  std::snprintf(b, sizeof b, "|g%d xp%d k%d inv%zu q%zu p%zu", g.gold, g.plXp, g.kills, g.inv.size(), g.quests.size(), g.pickups.size());
  return s + b;
}

void View::witnessRelease() {
  shutdown();            // the bakers joined (a rebuilt View starts its own on its first prefetch)
  bldgAsync_.clear();    // (std::async futures: waits for a paint still running)
  bldgAsyncKeys_.clear();
  if (pix_) pix_->destroyUnmarked();
  witnessResetBeasts();
  witnessResetWorldMap();
  witnessResetPaperdoll();
  witnessResetPartyHud();
  witnessResetHud();
}
#endif
