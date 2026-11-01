// rpg/app/script_runner.cpp - see script_runner.h
#include "rpg/app/script_runner.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "rpg/app/app.h"
#include "rpg/app/input_inject.h"
#include "rpg/sim/gear.h"
#include "rpg/sim/loot_ops.h"
#include "rpg/view/skills_ui.h"
#include "rpg/world/source.h"


bool g_scriptPhone = false;
void setEnvIfUnset(const char* k, const char* v) {
  if (std::getenv(k)) return;
#ifdef _WIN32
  _putenv_s(k, v);
#else
  setenv(k, v, 0);
#endif
}

bool loadScript(const char* path, std::vector<ScriptCmd>& out, uint64_t& seed) {
  FILE* f = std::fopen(path, "rb");
  if (!f) return false;
  std::string all;
  char buf[4096];
  size_t n;
  while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) all.append(buf, n);
  std::fclose(f);
  float last = 0;
  int lineNo = 0;
  size_t pos = 0;
  while (pos <= all.size()) {
    size_t e = all.find('\n', pos);
    if (e == std::string::npos) e = all.size();
    std::string ln = all.substr(pos, e - pos);
    pos = e + 1;
    lineNo++;
    size_t hash = ln.find('#');
    if (hash != std::string::npos) ln.resize(hash);
    std::vector<std::string> tok;
    size_t i = 0;
    while (i < ln.size()) {
      while (i < ln.size() && (ln[i] == ' ' || ln[i] == '\t' || ln[i] == '\r')) i++;
      size_t j = i;
      while (j < ln.size() && ln[j] != ' ' && ln[j] != '\t' && ln[j] != '\r') j++;
      if (j > i) tok.push_back(ln.substr(i, j - i));
      i = j;
    }
    if (tok.empty()) continue;
    if (tok[0] == "seed" && tok.size() >= 2) { if (!seed) seed = (uint64_t)std::atoll(tok[1].c_str()); continue; }
    if (tok[0] == "phone") { g_scriptPhone = true; continue; }
    if (tok[0] == "worldgen") { std::printf("script %s:%d: 'worldgen' is retired (M2: every world is endless)\n", path, lineNo); return false; }
    char* endp = nullptr;
    bool rel = tok[0][0] == '+';
    float t = std::strtof(tok[0].c_str() + (rel ? 1 : 0), &endp);
    if (!endp || *endp || tok.size() < 2) { std::printf("script %s:%d: expected '<time> <command>': %s\n", path, lineNo, ln.c_str()); continue; }
    ScriptCmd c;
    c.t = rel ? last + t : t;
    last = c.t;
    c.a.assign(tok.begin() + 1, tok.end());
    for (auto& ch : c.a[0]) ch = (char)std::tolower((unsigned char)ch);
    c.line = lineNo;
    for (size_t k = 1; k < tok.size(); k++) { if (k > 1) c.text += ' '; c.text += tok[k]; }
    out.push_back(c);
  }
  std::stable_sort(out.begin(), out.end(), [](const ScriptCmd& x, const ScriptCmd& y) { return x.t < y.t; });
  return true;
}

SDL_Scancode keyFromName(std::string s) {
  std::string u = s;
  for (auto& ch : u) ch = (char)std::toupper((unsigned char)ch);
  if (u == "ENTER") s = "Return";
  else if (u == "ESC") s = "Escape";
  else if (u == "SHIFT") s = "Left Shift";
  else if (u == "LSHIFT") s = "Left Shift";
  else if (u == "RSHIFT") s = "Right Shift";
  else if (u == "CTRL") s = "Left Ctrl";
  else if (u == "ALT") s = "Left Alt";
  else if (u == "META" || u == "SUPER") s = "Left GUI";   // (Witness 0.7.3) the OS key is "meta"; "super" kept for old scripts
  else if (u == "MINUS") s = "-";        // (P2a) the ZOOM keys: - and =
  else if (u == "EQUALS" || u == "PLUS") s = "=";
  return SDL_GetScancodeFromName(s.c_str());
}

// breadth-first path over the current map's walkable tiles, toward the reachable tile nearest (gx, gy)
bool findPath(const Map& m, int sx, int sy, int gx, int gy, std::vector<int>& path) {
  path.clear();
  if (!m.in(sx, sy)) return false;
  const int R = 90;   // search box radius in tiles
  int x0 = std::max(0, std::min(sx, gx) - R), y0 = std::max(0, std::min(sy, gy) - R);
  int x1 = std::min(m.w - 1, std::max(sx, gx) + R), y1 = std::min(m.h - 1, std::max(sy, gy) + R);
  int bw = x1 - x0 + 1, bh = y1 - y0 + 1;
  std::vector<int> from((size_t)bw * bh, -1);
  std::vector<int> q;
  auto id = [&](int x, int y) { return (y - y0) * bw + (x - x0); };
  q.push_back(id(sx, sy));
  from[(size_t)id(sx, sy)] = id(sx, sy);
  int best = id(sx, sy), bestD = std::abs(sx - gx) * std::abs(sx - gx) + std::abs(sy - gy) * std::abs(sy - gy);
  for (size_t h = 0; h < q.size(); h++) {
    int x = q[h] % bw + x0, y = q[h] / bw + y0;
    int d = (x - gx) * (x - gx) + (y - gy) * (y - gy);
    if (d < bestD) { bestD = d; best = q[h]; }
    if (d == 0) break;
    static const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
    for (int k = 0; k < 4; k++) {
      int nx = x + dx[k], ny = y + dy[k];
      if (nx < x0 || ny < y0 || nx > x1 || ny > y1 || from[(size_t)id(nx, ny)] >= 0 || m.blocked(nx, ny)) continue;
      // never path across a flight of stairs that is not the goal (stepping on it changes the floor)
      const int pr = m.propAt(nx, ny);
      if ((pr == (int)art::Prop::StairsDown + 1 || pr == (int)art::Prop::StairsUp + 1) && (nx != gx || ny != gy)) continue;
      // (owner) nor across a building's way in that is not the goal (an open front's bays: stepping in enters)
      if (m.kind == MapKind::Overworld && m.bldgAt[(size_t)ny * m.w + nx] >= 0 && (nx != gx || ny != gy)) continue;
      from[(size_t)id(nx, ny)] = q[h];
      q.push_back(id(nx, ny));
    }
  }
  for (int c = best; ; c = from[(size_t)c]) {
    path.push_back((c / bw + y0) * m.w + (c % bw + x0));
    if (from[(size_t)c] == c) break;
  }
  std::reverse(path.begin(), path.end());
  return true;
}

// test helpers shared by the command-line flags and the script commands
void App::doGoto(const std::string& w, bool enterIt) {
  SiteType want = SiteType::City;
  if (w == "town" || w == "market") want = SiteType::Town; else if (w == "village") want = SiteType::Village; else if (w == "cave") want = SiteType::Cave;
  else if (w == "ruin") want = SiteType::Ruin; else if (w == "camp") want = SiteType::BanditCamp; else if (w == "lair") want = SiteType::DragonLair;
  else if (w == "shrine") want = SiteType::Shrine;
  int si = game.world.nearestSite(game.world.sites[game.world.startSite].ex, game.world.sites[game.world.startSite].ey, want);
  if (w == "capital") {   // M1: the nearest kingdom capital to the start (the king's palace)
    si = -1;
    int bd = 1 << 30;
    const Site& h = game.world.sites[game.world.startSite];
    for (int i = 0; i < (int)game.world.sites.size(); i++) {
      const Site& s = game.world.sites[i];
      if (!s.capital) continue;
      int d = (s.ex - h.ex) * (s.ex - h.ex) + (s.ey - h.ey) * (s.ey - h.ey);
      if (d < bd) { bd = d; si = i; }
    }
  }
  if (game.world.endless) {
    // the endless world: the nearest one in the region plans around the start (settlements are far apart: the nearest
    // town may lie beyond the records the window has loaded)
    const Site& h = game.world.sites[(size_t)game.world.startSite];
    int far = game.world.findSiteNear(game.world.ox + h.ex, game.world.oy + h.ey, w == "capital" ? SiteType::City : want, 8, w == "capital");
    if (w == "capital" && far < 0) far = game.world.findSiteNear(game.world.ox + h.ex, game.world.oy + h.ey, SiteType::Town, 8, true);
    if (w == "market") {   // (M1 economy) the nearest market town: a real crossroads of trade (region.cpp: six roads,
                           // or four and the town's own roll; a village at five roads and a rarer roll)
      far = game.world.findSiteNear(game.world.ox + h.ex, game.world.oy + h.ey, SiteType::Town, 12, false, (int)ew::Archetype::Market);
      if (far < 0) far = game.world.findSiteNear(game.world.ox + h.ex, game.world.oy + h.ey, SiteType::City, 12, false, (int)ew::Archetype::Market);
    }
    if (far >= 0) si = far;
  }
  if (si < 0) { std::printf("goto %s: none found\n", w.c_str()); return; }
  game.world.sites[si].discovered = true;
  game.fastTravel(si);
  if (enterIt && (want == SiteType::Cave || want == SiteType::Ruin)) {
    const Site& s = game.world.sites[si];
    game.pl().p = Vec2(s.ex * 16 + 8.0f, s.ey * 16 + 6.0f);
    game.update(SIM_DT, Input());
  }
  if (enterIt && (want == SiteType::City || want == SiteType::Town || want == SiteType::Village)) {
    const Site& s = game.world.sites[si];
    for (int b = s.bldgFirst; b < s.bldgFirst + s.bldgCount; b++)
      if (game.world.over.bldgs[b].type == art::Building::Inn) {
        game.pl().p = Vec2(game.world.over.bldgs[b].doorX() * 16 + 8.0f, game.world.over.bldgs[b].doorY() * 16 + 10.0f);
        game.update(SIM_DT, Input());
        break;
      }
  }
  game.sleepFade = 0;
  view.snap(game);
}

// PT test-only: talk to the innkeeper (0 greeting, 1 job offer, 2 shop)
void App::doTalk(int step) {
  if (!game.inside) return;
  for (size_t k = 1; k < game.actors.size(); k++)
    if (game.actors[k].role == Role::Innkeeper) {
      game.pl().p = game.actors[k].p + Vec2(0, 20);
      Input ti; ti.interact = true;
      game.update(SIM_DT, ti);
      if (game.mode == Mode::Dialogue && step >= 1) {
        const char* want = step == 1 ? "WORK" : "WARES";
        { const int o = dlgReach(game, [&](const DlgOpt& x) { return x.label.find(want) != std::string::npos; }, true); if (o >= 0) game.dialogueChoose(o); }
      }
      break;
    }
}

// test helper: a full armour set of one band (1 leather .. 7 emberforged), equipped, plus a weapon and a bow.
// (M8b, LootSrc::Debug) made by the gear rules at the band's middle item level (never the classic makers): the armour,
// weapon and bow Common, the ring and amulet Uncommon (a jewel's worth is its affix), so every piece passes lootValid
void App::giveGear(int band, const std::string& weaponName) {
  band = std::clamp(band, 1, 7);
  const int il = (gear::bandLo(band) + gear::bandHi(band)) / 2;
  Rng r(game.seed * 31 + (uint64_t)band);
  auto wear = [&](Item it) {
    game.inv.push_back(it);
    int idx = (int)game.inv.size() - 1;
    int* e = game.equipSlot(it.kind);
    if (e && *e != idx) game.useItem(idx);
  };
  const ItemKind kinds[] = {ItemKind::Armor, ItemKind::Helmet, ItemKind::Gloves, ItemKind::Boots, ItemKind::Cloak, ItemKind::Shield};
  for (int k = 0; k < 6; k++) wear(gear::makeGearC(r, kinds[k], 0, il, Rarity::Common, nullptr, band == 1 && kinds[k] != ItemKind::Cloak ? Mat::Leather : Mat::None));
  static const char* wn[] = {"sword", "axe", "mace", "dagger", "greatsword"};
  int wt = 0;
  for (int i = 0; i < 5; i++) if (weaponName == wn[i]) wt = i;
  wear(gear::makeGearC(r, ItemKind::Weapon, wt, il, Rarity::Common, nullptr));
  wear(gear::makeGearC(r, ItemKind::Bow, 0, il, Rarity::Common, nullptr));
  wear(gear::makeGearC(r, ItemKind::Amulet, 0, il, Rarity::Uncommon, nullptr));
  wear(gear::makeGearC(r, ItemKind::Ring, 0, il, Rarity::Uncommon, nullptr));
}

void App::doFight(const std::string& name, int n) {
  if (game.eqWeapon < 0) game.debugKit();   // the real start is shirt-only (M0): fights get the test kit
  static const char* names[] = {"wolf", "boar", "bear", "slime", "spider", "bat", "skeleton", "draugr", "goblin", "troll", "wraith", "mudcrab", "icewolf", "frostspider", "sandworm", "dragon",
                                "scorpion", "hyena", "lurker", "yeti", "wisp", "emberhound", "blightspawn",   // (M3c wildlife)
                                "harpy", "golem",   // (M6)
                                "seaserpent", "kraken", "merrow"};   // (M10: they swim; a fight on land strands them)
  for (int m = 0; m < (int)art::Monster::COUNT && m < (int)(sizeof(names) / sizeof(names[0])); m++)
    if (name == names[m]) game.debugSpawn((art::Monster)m, n > 0 ? n : (m == (int)art::Monster::Dragon ? 1 : 3), 60);
}

void App::fail(int line, const std::string& msg) {
  std::printf("script FAIL (line %d): %s\n", line, msg.c_str());
  scriptFails++;
  lastFail = msg;
#ifdef EMB_WITNESS
  if (witness && !witnessInCall) witness_log(WITNESS_ERROR, "script: %s", msg.c_str());
#endif
}

void App::plTile(int& x, int& y) { x = (int)std::floor(game.pl().p.x / TILE); y = (int)std::floor((game.pl().p.y - 2) / TILE); }
int App::findActorIdx(int id) { for (size_t k = 1; k < game.actors.size(); k++) if (game.actors[k].id == id) return (int)k; return -1; }

void App::startWalk(const ScriptCmd& c) {
  walk = Walk();
  walk.on = true;
  walk.wox = game.world.ox; walk.woy = game.world.oy;
  walk.line = c.line;
  std::vector<std::string> a(c.a.begin() + 1, c.a.end());
  auto num = [](const std::string& v) { return !v.empty() && (std::isdigit((unsigned char)v[0]) || v[0] == '.'); };
  bool tiles = a.size() >= 2 && num(a[0]) && num(a[1]);
  size_t nTarget = tiles ? 2 : 1;
  if (a.size() > nTarget && num(a[nTarget])) walk.timeout = (float)std::atof(a[nTarget].c_str());
  if (a.empty()) { walk.on = false; fail(c.line, "walkto needs a target"); return; }
  walk.what = a[0];
  int px, py;
  plTile(px, py);
  static const struct { const char* n; art::Building b; } bt[] = {
      {"inn", art::Building::Inn}, {"shop", art::Building::Shop}, {"smithy", art::Building::Smithy}, {"temple", art::Building::Temple},
      {"keep", art::Building::Keep}, {"tower", art::Building::Tower}, {"house", art::Building::House}, {"farmhouse", art::Building::Farmhouse},
      {"palace", art::Building::Palace}, {"barracks", art::Building::Barracks}};
  static const struct { const char* n; Role r; } rt[] = {
      {"innkeeper", Role::Innkeeper}, {"merchant", Role::Merchant}, {"smith", Role::Smith}, {"priest", Role::Priest},
      {"jarl", Role::Jarl}, {"guard", Role::Guard}, {"villager", Role::Villager}, {"mage", Role::Mage}, {"farmer", Role::Farmer},
      {"king", Role::King}, {"captain", Role::Captain}};
  if (tiles) { walk.kind = 0; walk.tx = std::atoi(a[0].c_str()); walk.ty = std::atoi(a[1].c_str()); return; }
  if (walk.what == "upstairs" || walk.what == "downstairs") {
    bool up = walk.what == "upstairs";
    const Stairs& s = up ? game.sub.up : game.sub.down;
    if (!game.inside || game.subBldg < 0 || !s.valid()) { walk.on = false; fail(c.line, "walkto " + walk.what + ": no stairs here"); return; }
    walk.kind = 4; walk.tx = s.x; walk.ty = s.y; walk.floor0 = game.subFloor; return;
  }
  if (walk.what == "exit") {
    if (!game.inside) { walk.on = false; fail(c.line, "walkto exit: not inside"); return; }
    walk.kind = 3; walk.tx = game.sub.exitX; walk.ty = game.sub.exitY; return;
  }
  for (auto& b : bt)
    if (walk.what == b.n) {
      if (game.inside) { walk.on = false; fail(c.line, "walkto " + walk.what + ": already inside"); return; }
      int best = -1; float bd = 1e30f;
      const auto& B = game.world.over.bldgs;
      for (size_t i = 0; i < B.size(); i++) {
        if (B[i].type != b.b) continue;
        float d = std::hypot((float)(B[i].doorX() - px), (float)(B[i].doorY() - py));
        if (d < bd) { bd = d; best = (int)i; }
      }
      if (best < 0 || bd > 120) { walk.on = false; fail(c.line, "walkto " + walk.what + ": none nearby"); return; }
      walk.kind = 1; walk.bldg = best; walk.tx = B[best].doorX(); walk.ty = B[best].doorY() + 1;
      return;
    }
  for (auto& r : rt)
    if (walk.what == r.n) {
      int best = -1; float bd = 1e30f;
      for (size_t k = 1; k < game.actors.size(); k++) {
        const Actor& ac = game.actors[k];
        if (!ac.npc || ac.role != r.r || ac.st == AState::Dead) continue;
        float d = len2(ac.p - game.pl().p);
        if (d < bd) { bd = d; best = ac.id; }
      }
      if (best < 0) { walk.on = false; fail(c.line, "walkto " + walk.what + ": nobody with that role here"); return; }
      walk.kind = 2; walk.actorId = best;
      return;
    }
  // (M2) giver: the nearest person with a job to offer or a reward waiting (any role). When nobody is in play yet
  // (townsfolk who hid from a fight come back out a while later: on a loaded machine the sim runs behind script
  // time) the walk waits for one (kind 5) until its timeout.
  if (walk.what == "giver") {
    walk.kind = 5;
    return;
  }
  // a person by name (as rpg_test's EMB_SCRIPT_INFO lists them): walkto VIGRIMA
  for (size_t k = 1; k < game.actors.size(); k++) {
    const Actor& ac = game.actors[k];
    if (ac.npc && ac.st != AState::Dead && ac.name == walk.what) { walk.kind = 2; walk.actorId = ac.id; return; }
  }
  walk.on = false;
  fail(c.line, "walkto: unknown target '" + walk.what + "'");
}

void App::endWalk(bool ok, const char* why) {
  setMove(false, false, false, false);
  if (!ok) fail(walk.line, "walkto " + walk.what + ": " + why);
  else std::printf("script %.2f: walkto %s done in %.1f s\n", scriptT, walk.what.c_str(), walk.t);
  if (ok && walk.talk) { pushKey(SDL_SCANCODE_E, true); pushKey(SDL_SCANCODE_E, false); }
  walk.on = false;
}

int App::findGiver() {
  int best = -1; float bd = 1e30f;
  for (int pass = 0; pass < 2 && best < 0; pass++)   // a reward waiting first, then a job on offer
    for (size_t k = 1; k < game.actors.size(); k++) {
      const Actor& ac = game.actors[k];
      if (!ac.npc || ac.st == AState::Dead || !(pass == 0 ? game.rewardWaiting(ac) : game.offersWork(ac))) continue;
      float d = len2(ac.p - game.pl().p);
      if (d < bd) { bd = d; best = ac.id; }
    }
  return best;
}

void App::stepWalk(float dt) {
  walk.t += dt;
  if (!game.inside && (game.world.ox != walk.wox || game.world.oy != walk.woy)) {
    // the endless window moved under the walk: the target's window tiles moved with it, the path is stale
    walk.tx -= game.world.ox - walk.wox; walk.ty -= game.world.oy - walk.woy;
    walk.wox = game.world.ox; walk.woy = game.world.oy;
    walk.path.clear(); walk.step = 0; walk.replanT = 0;
  }
  if (walk.kind == 5) {   // walkto giver: waiting for someone with work or a reward to be in play
    const int id = findGiver();
    if (id >= 0) { walk.kind = 2; walk.actorId = id; }
    else if (walk.t > walk.timeout) {
      // say where the givers of the quests waiting to be turned in are (a test that fails here needs to know)
      for (const Quest& q : game.quests) {
        if (q.state != QState::Complete) continue;
        int found = 0;
        for (size_t k = 1; k < game.actors.size(); k++) {
          const Actor& ac = game.actors[k];
          if (ac.name != q.giverName) continue;
          found++;
          std::printf("script:   giver %s of '%s': in play, %.0f tiles away, state %d, site %d bldg %d slot %d (quest: %d %d %d)\n", ac.name.c_str(),
                      q.title.c_str(), len(ac.p - game.pl().p) / TILE, (int)ac.st, ac.site, ac.bldg, ac.slot, q.giverSite, q.giverBldg, q.giverSlot);
        }
        if (!found) std::printf("script:   giver %s of '%s': not in play\n", q.giverName.c_str(), q.title.c_str());
      }
      endWalk(false, "nobody here offers work or holds a reward");
      return;
    }
    else { setMove(false, false, false, false); return; }
  }
  if (walk.t > walk.timeout) {
    // (M7c integration) where it stuck: the player (@), the target (T), blocked tiles (#) round the target
    {
      int qx, qy;
      plTile(qx, qy);
      const Map& mm = game.map();
      std::printf("script:   walk stuck at %d,%d, target %d,%d\n", qx, qy, walk.tx, walk.ty);
      for (int y = walk.ty - 6; y <= walk.ty + 6; y++) {
        std::string row = "script:   ";
        for (int x = walk.tx - 12; x <= walk.tx + 12; x++)
          row += (x == qx && y == qy) ? '@' : (x == walk.tx && y == walk.ty) ? 'T' : !mm.in(x, y) ? ' ' : mm.blocked(x, y) ? '#' : '.';
        std::printf("%s\n", row.c_str());
      }
      // (M8b merge) a walk to someone: why the prompt is not theirs (who has it, a foe in reach, the mode)
      if (walk.kind == 2) {
        const int k = findActorIdx(walk.actorId);
        const int it = game.interactTarget();
        const int ik = it >= 0 ? findActorIdx(it) : -1;
        std::printf("script:   walking to %s, %.1f px off; the prompt is %s; a foe in reach %d; mode %d\n",
                    k >= 0 ? game.actors[(size_t)k].name.c_str() : "?", k >= 0 ? len(game.actors[(size_t)k].p - game.pl().p) : -1.0f,
                    ik >= 0 ? game.actors[(size_t)ik].name.c_str() : "nobody's", (int)game.foeInReach(), (int)game.mode);
        for (size_t a = 1; a < game.actors.size(); a++) {
          const Actor& ac = game.actors[a];
          if (ac.st == AState::Dead || len(ac.p - game.pl().p) > 60.0f) continue;
          std::printf("script:     near: %s npc %d hostile %d aggro %d target-me %d comp %d %.1f px\n", ac.name.c_str(), (int)ac.npc, (int)ac.hostile,
                      (int)ac.aggro, (int)(ac.target == game.pl().id), (int)ac.comp, len(ac.p - game.pl().p));
        }
      }
    }
    endWalk(false, "timed out");
    return;
  }
  if (game.mode != Mode::Play) { setMove(false, false, false, false); return; }   // a dialogue or menu is up: wait
  int px, py;
  plTile(px, py);
  // arrived?
  if (walk.kind == 1 && game.inside) { endWalk(true, ""); return; }
  if (walk.kind == 3 && !game.inside) { endWalk(true, ""); return; }
  if (walk.kind == 4 && game.subFloor != walk.floor0) { endWalk(true, ""); return; }
  if (walk.kind == 2) {
    int k = findActorIdx(walk.actorId);
    if (k < 0 && walk.what == "giver") {
      // (M7b) the giver walked off (indoors, out of the window): look again for the nearest one until the timeout
      walk.kind = 5; walk.actorId = -1; walk.path.clear(); walk.step = 0; walk.replanT = 0;
      setMove(false, false, false, false);
      return;
    }
    if (k < 0) { endWalk(false, "they left"); return; }
    // in reach: keep closing in a moment longer (a step from the edge of reach, a wandering innkeeper or a neighbour
    // who comes nearer turns the next key press into the wrong conversation), unless already close
    if (game.interactTarget() == walk.actorId) {
      walk.closeT += dt;
      if (len(game.actors[k].p - game.pl().p) < 20.0f || walk.closeT > 0.6f) { endWalk(true, ""); return; }
    }
    // (M8b merge, m2_sim_quests) a keeper at their stall is talked to only from the customers' side (interactTarget):
    // the walk makes for a free tile in front of the counter, not the keeper's own tile (from beside or behind the
    // stall, 9 px off, the prompt was nobody's and the turn-in timed out)
    const Actor& A = game.actors[(size_t)k];
    Vec2 at = A.p;
    if (A.stallKeeper && len2(A.p - A.home) < 4.0f) {
      const Map& mm = game.map();
      for (float tiles : {2.0f, 1.0f, 2.5f}) {
        const Vec2 q = A.p + faceVec(A.face) * (tiles * TILE);
        const int qx = (int)std::floor(q.x / TILE), qy = (int)std::floor((q.y - 2) / TILE);
        if (mm.in(qx, qy) && !mm.blocked(qx, qy)) { at = q; break; }
      }
    }
    walk.tx = (int)std::floor(at.x / TILE); walk.ty = (int)std::floor((at.y - 2) / TILE);
  }
  if (walk.kind == 0 && px == walk.tx && py == walk.ty) { endWalk(true, ""); return; }
  if (walk.kind == 4 && walk.what == "downstairs" && game.stairsAsleep()) {
    // just climbed: the stairwell sleeps until you have stepped off the tile you arrived on. Step to a free
    // neighbour (south first) and the stairs are awake again.
    static const int nd[4][2] = {{0, 1}, {1, 0}, {-1, 0}, {0, -1}};
    for (auto& d : nd) {
      int nx = px + d[0], ny = py + d[1];
      int pr = game.sub.propAt(nx, ny);
      if (!game.sub.in(nx, ny) || game.sub.blocked(nx, ny) || pr == (int)art::Prop::StairsDown + 1 || pr == (int)art::Prop::StairsUp + 1) continue;
      setMove(d[1] < 0, d[0] < 0, d[1] > 0, d[0] > 0);
      return;
    }
  }
  if (walk.kind == 4 && px == walk.tx && py == walk.ty) {   // on the steps: keep walking onto them until the floor changes
    // (up: into the flight, north; down: a step south across the stairwell, as a player coming from its foot would)
    float cx = walk.tx * TILE + 8.0f - game.pl().p.x;
    bool up = walk.what == "upstairs";
    setMove(up, cx < -2, !up, cx > 2);
    return;
  }
  // at the approach tile: the last step is a push through the door (up) or onto the exit (down)
  if ((walk.kind == 1 || walk.kind == 3) && px == walk.tx && (py == walk.ty || (walk.kind == 3 && py == walk.ty - 1))) {
    walk.finalT += dt;
    float cx = walk.tx * TILE + 8.0f - game.pl().p.x;
    setMove(walk.kind == 1, cx < -2, walk.kind == 3, cx > 2);
    return;
  }
  walk.replanT -= dt;
  if (walk.replanT <= 0 || walk.step >= walk.path.size()) {
    findPath(game.map(), px, py, walk.tx, walk.ty, walk.path);
    walk.step = 0;
    walk.replanT = walk.kind == 2 ? 0.4f : 1.0f;
  }
  const Map& m = game.map();
  while (walk.step < walk.path.size()) {
    int wx = walk.path[walk.step] % m.w, wy = walk.path[walk.step] / m.w;
    Vec2 tgt(wx * TILE + 8.0f, wy * TILE + 10.0f);
    Vec2 d = tgt - game.pl().p;
    if (std::fabs(d.x) <= 2.5f && std::fabs(d.y) <= 2.5f) { walk.step++; continue; }
    setMove(d.y < -2.5f, d.x < -2.5f, d.y > 2.5f, d.x > 2.5f);
    return;
  }
  setMove(false, false, false, false);   // the nearest reachable tile: wait there (an actor may still come within reach)
}

// M2: commands registered through rpg/view/script_api.h. A command that returns false is retried every frame (script
// time stands still) until it returns true or its timeout passes.
void App::callExt(float dt) {
  ScriptCtx ctx{game, view, extRun.cmd.a, extRun.cmd.line, scriptT, extRun.waited, [&](const std::string& m) { fail(extRun.cmd.line, m); },
                [&](float x, float y) {
                  SDL_FingerID id = nextFinger++;
                  pushFinger(SDL_EVENT_FINGER_DOWN, id, x, y);
                  lifts.push_back({id, x, y, scriptT + 0.08f});
                }};
  if (extRun.e->fn(ctx)) { extRun.e = nullptr; return; }
  extRun.waited += dt;
  if (extRun.waited > extRun.e->timeout) { fail(extRun.cmd.line, std::string(extRun.e->name) + ": timed out"); extRun.e = nullptr; }
}

bool App::startExt(const std::string& name, const ScriptCmd& c) {
  for (const ScriptExt& e : scriptExts())
    if (name == e.name) {
      extRun.e = &e; extRun.cmd = c; extRun.waited = 0;
      callExt(0);
      return true;
    }
  return false;
}

bool App::modeByName(const std::string& s, Mode& m) {
  static const char* n[] = {"title", "play", "dialogue", "menu", "shop", "levelup", "dead", "paused", "creator", "forge", "build"};
  for (int i = 0; i < (int)(sizeof n / sizeof n[0]); i++) if (s == n[i]) { m = (Mode)i; return true; }
  return false;
}

// one script command (the built-in ones in their original order: input, the autopilot, travel and buildings here, the
// rest in runStateCmd)
void App::runCmd(const ScriptCmd& c) {
  const std::string& op = c.a[0];
  auto arg = [&](size_t i) { return i < c.a.size() ? c.a[i] : std::string(); };
  std::printf("script %.2f: %s\n", scriptT, c.text.c_str());
  if (op == "key") {
    SDL_Scancode sc = keyFromName(arg(1));
    if (sc == SDL_SCANCODE_UNKNOWN) { fail(c.line, "unknown key '" + arg(1) + "'"); return; }
    pushKey(sc, true); pushKey(sc, false);
  } else if (op == "hold") {
    SDL_Scancode sc = keyFromName(arg(1));
    if (sc == SDL_SCANCODE_UNKNOWN) { fail(c.line, "unknown key '" + arg(1) + "'"); return; }
    pushKey(sc, true);
    view.scriptHold(sc, true);
    held.push_back({sc, scriptT + (float)std::atof(arg(2).c_str())});
  } else if (op == "tap") {
    float x = (float)std::atof(arg(1).c_str()), y = (float)std::atof(arg(2).c_str());
    SDL_FingerID id = nextFinger++;
    pushFinger(SDL_EVENT_FINGER_DOWN, id, x, y);
    lifts.push_back({id, x, y, scriptT + 0.08f});
  } else if (op == "click") {
    float x = (float)std::atof(arg(1).c_str()), y = (float)std::atof(arg(2).c_str());
    pushClick(x, y, true); pushClick(x, y, false);
  } else if (op == "press" || op == "swipe") {   // (M9) a held finger / a drag (the quick-cast hold and swipe)
    size_t k = 1;
    float x = 0, y = 0;
    if (arg(1) == "quick") {
      const int b = std::atoi(arg(2).c_str()) - 1;
      const QuickBtn q = quickBtnPos(b < 0 ? 0 : b > 4 ? 4 : b);
      x = q.x; y = q.y; k = 3;
    } else {
      x = (float)std::atof(arg(1).c_str()); y = (float)std::atof(arg(2).c_str()); k = 3;
    }
    float x1 = x, y1 = y;
    if (op == "swipe") {
      if (arg(1) == "quick") { x1 = x + (float)std::atof(arg(k).c_str()); y1 = y + (float)std::atof(arg(k + 1).c_str()); }
      else { x1 = (float)std::atof(arg(k).c_str()); y1 = (float)std::atof(arg(k + 1).c_str()); }
      k += 2;
    }
    const float secs = std::max(0.05f, (float)std::atof(arg(k).c_str()));
    SDL_FingerID id = nextFinger++;
    pushFinger(SDL_EVENT_FINGER_DOWN, id, x, y);
    if (op == "swipe") {   // the drag in four steps over the first half of the gesture
      for (int s = 1; s <= 4; s++) drags.push_back({id, x + (x1 - x) * s / 4.0f, y + (y1 - y) * s / 4.0f, scriptT + secs * 0.5f * s / 4.0f});
    }
    lifts.push_back({id, x1, y1, scriptT + secs});
  } else if (op == "blur") {   // the window loses the focus (an alt-tab): play pauses, a recording lets go of what it holds
    if (witness) { fail(c.line, "blur: a harness's hidden window never loses the focus"); return; }
    focusLost(true);
  } else if (op == "shot") {
    pendingShot = arg(1).empty() ? "shot.png" : arg(1);
  } else if (op == "quit") {
    running = false;
  } else if (op == "walkto") {
    startWalk(c);
  } else if (op == "talkto") {   // walkto a person, then talk in the very frame they come in reach (no one else steps in)
    startWalk(c);
    if (walk.on && (walk.kind == 2 || walk.kind == 5)) walk.talk = true;
    else if (walk.on) { walk.on = false; setMove(false, false, false, false); fail(c.line, "talkto needs a person"); }
  } else if (op == "newgame") {
    startNew(startSeed);
  } else if (op == "goto") {
    doGoto(arg(1), arg(2) == "enter");
  } else if (op == "enter") {   // M0b: enter <type> [floor] [nth]
    static const struct { const char* n; art::Building b; } et[] = {
        {"inn", art::Building::Inn}, {"shop", art::Building::Shop}, {"smithy", art::Building::Smithy}, {"temple", art::Building::Temple},
        {"keep", art::Building::Keep}, {"tower", art::Building::Tower}, {"house", art::Building::House},
        {"stonehouse", art::Building::StoneHouse}, {"farmhouse", art::Building::Farmhouse}, {"hut", art::Building::Hut},
        {"palace", art::Building::Palace}, {"barracks", art::Building::Barracks},
        // M1 economy: the production buildings
        {"windmill", art::Building::Windmill}, {"watermill", art::Building::Watermill}, {"granary", art::Building::Granary},
        {"bakery", art::Building::Bakery}, {"butcher", art::Building::Butcher}, {"tannery", art::Building::Tanner},
        {"tanner", art::Building::Tanner}, {"fishmonger", art::Building::Fishmonger}, {"smelter", art::Building::Smelter},
        {"sawmill", art::Building::Sawmill}, {"weaver", art::Building::Weaver},
        // M3b: the society's purposes
        {"guildhall", art::Building::Guildhall}, {"exchange", art::Building::Exchange}, {"meadhall", art::Building::MeadHall},
        {"bathhouse", art::Building::Bathhouse}, {"teahouse", art::Building::TeaHouse}, {"lodge", art::Building::Lodge},
        {"councilhall", art::Building::CouncilHall}};
    int want = -1;
    for (auto& e : et) if (arg(1) == e.n) want = (int)e.b;
    int fl = std::atoi(arg(2).c_str()), nth = std::atoi(arg(3).c_str());
    int px, py;
    if (game.inside && game.subBldg >= 0) { px = game.world.over.bldgs[game.subBldg].doorX(); py = game.world.over.bldgs[game.subBldg].doorY(); }
    else plTile(px, py);
    std::vector<std::pair<float, int>> cand;
    const auto& B = game.world.over.bldgs;
    for (size_t i = 0; i < B.size(); i++)
      if ((int)B[i].type == want && fl < B[i].floors()) cand.push_back({std::hypot((float)(B[i].doorX() - px), (float)(B[i].doorY() - py)), (int)i});
    std::sort(cand.begin(), cand.end());
    // (fixer r2) the seat of power is the settlement's own: "enter palace" never walks across the map into another
    // realm's (enterseat enters whatever the society built here). Other types still take the nearest in the window.
    if (want == (int)art::Building::Palace) while (!cand.empty() && cand.back().first >= 160.0f) cand.pop_back();
    if (want < 0) fail(c.line, "enter: unknown building type '" + arg(1) + "'");
    else if (nth >= (int)cand.size()) fail(c.line, "enter " + arg(1) + ": no such building with floor " + arg(2) + (want == (int)art::Building::Palace ? " near (the seat of power here: enterseat)" : ""));
    else if (!game.debugEnterBuilding(cand[(size_t)nth].second, fl)) fail(c.line, "enter " + arg(1) + ": could not enter");
    else { game.mode = Mode::Play; view.snap(game); }
  } else if (op == "floor") {
    int fl = std::atoi(arg(1).c_str());
    if (!game.inside || game.subBldg < 0 || fl < 0 || fl >= game.world.over.bldgs[game.subBldg].floors()) fail(c.line, "floor " + arg(1) + ": not a floor here");
    else { game.changeFloor(fl); view.snap(game); }
  } else if (op == "choose") {   // M0b: choose the open dialogue's option whose label contains the words
    std::string t;
    for (size_t k = 1; k < c.a.size(); k++) { if (k > 1) t += ' '; t += c.a[k]; }
    // (fixer M7b r1) as the player would: an option on show, or the header (or SOMETHING ELSE.) that shows it tapped first
    int pickI = -1;
    if (game.mode == Mode::Dialogue) pickI = dlgReach(game, [&](const DlgOpt& o) { return o.label.find(t) != std::string::npos; }, true);
    if (game.mode != Mode::Dialogue || pickI < 0) fail(c.line, "choose: no dialogue option '" + t + "' the player can reach");
    else { game.dialogueChoose(pickI); view.snap(game); }
  } else if (op == "at") {   // M1: a global tile
    game.teleportGlobal((int32_t)std::atoi(arg(1).c_str()), (int32_t)std::atoi(arg(2).c_str()));
    game.mode = Mode::Play;
    view.snap(game);
  } else if (op == "nearshift") {   // M1: step to 2 tiles short of the window's east|west|north|south shift line
    int tx = 0, ty = 0;
    plTile(tx, ty);
    const int lo = World::WIN_SHIFT, hi = World::WIN - World::WIN_SHIFT;
    int ddx = 0, ddy = 0;
    if (arg(1) == "east") { tx = hi - 2; ddx = 1; } else if (arg(1) == "west") { tx = lo + 1; ddx = -1; }
    else if (arg(1) == "south") { ty = hi - 2; ddy = 1; } else if (arg(1) == "north") { ty = lo + 1; ddy = -1; }
    // a lane where the next few tiles across the line are open (so the walk really crosses it)
    const Map& M = game.world.over;
    for (int off = 0; off < 120; off++) {
      int o = (off & 1) ? -(off + 1) / 2 : off / 2;
      int x0 = tx + (ddy ? o : 0), y0 = ty + (ddx ? o : 0);
      bool open = true;
      for (int k = -1; k <= 6 && open; k++)
        if (M.blocked(x0 + ddx * k, y0 + ddy * k)) open = false;
      if (open) { tx = x0; ty = y0; break; }
    }
    { game.teleportGlobal(game.world.ox + tx, game.world.oy + ty); game.mode = Mode::Play; view.snap(game); }
  } else if (op == "walk") {   // M1: walk east|west|north|south SECS (a held direction key)
    static const struct { const char* n; SDL_Scancode sc; } dirs[] = {
        {"east", SDL_SCANCODE_D}, {"west", SDL_SCANCODE_A}, {"north", SDL_SCANCODE_W}, {"south", SDL_SCANCODE_S}};
    SDL_Scancode sc = SDL_SCANCODE_UNKNOWN;
    for (auto& d : dirs) if (arg(1) == d.n) sc = d.sc;
    if (sc == SDL_SCANCODE_UNKNOWN) fail(c.line, "walk: unknown direction '" + arg(1) + "'");
    else {
      pushKey(sc, true);
      view.scriptHold(sc, true);
      held.push_back({sc, scriptT + (float)std::atof(arg(2).c_str())});
    }
  } else if (op == "talk") {
    doTalk(std::atoi(arg(1).c_str()));
  } else if (op == "fight") {
    doFight(arg(1), std::atoi(arg(2).c_str()));
  } else if (op == "kit") {
    game.debugKit();
  } else if (op == "gear") {
    giveGear(std::atoi(arg(1).c_str()), arg(2));
  } else {
    runStateCmd(c);
  }
}

// runCmd's second half: the chests, equipment, the clock, the menu, pos and every expect check, then the registered
// commands (rpg/view/script_api.h). runCmd has printed the line.
void App::runStateCmd(const ScriptCmd& c) {
  const std::string& op = c.a[0];
  auto arg = [&](size_t i) { return i < c.a.size() ? c.a[i] : std::string(); };
  if (op == "chestgo") {
    // (M8b SOURCES) chestgo [hoard|explore|chest]: stand beside the nearest closed chest of the current map (of that
    // loot source: LootOps::chestSource), facing it, so `key e` opens it through the real Game::openChest
    const std::string want = arg(1);
    Map& m = game.map();
    const Vec2 pp = game.pl().p;
    int bx = -1, by = -1;
    float best = 1e30f;
    for (int y = 0; y < m.h; y++)
      for (int x = 0; x < m.w; x++) {
        if (m.propAt(x, y) != (int)art::Prop::Chest + 1) continue;
        const gear::LootSrc s = LootOps::chestSource(game, x, y).src;
        if (want == "hoard" && s != gear::LootSrc::Hoard) continue;
        if (want == "explore" && s != gear::LootSrc::Explore) continue;
        if (want == "chest" && s != gear::LootSrc::Chest) continue;
        const float dx = x * TILE + 8.0f - pp.x, dy = y * TILE + 8.0f - pp.y;
        float d = dx * dx + dy * dy;
        // one well inside the map first (the camera stops at the map's edge, where the HUD panels cover a find)
        if (x < 10 || y < 7 || x >= m.w - 10 || y >= m.h - 7) d += 1e9f;
        if (d < best) { best = d; bx = x; by = y; }
      }
    bool placed = false;
    if (bx >= 0) {
      static const int sides[4][2] = {{0, 1}, {-1, 0}, {1, 0}, {0, -1}};
      for (auto& s : sides) {
        if (m.blocked(bx + s[0], by + s[1])) continue;
        game.pl().p = Vec2((bx + s[0]) * TILE + 8.0f, (by + s[1]) * TILE + 10.0f);
        game.pl().aim = Vec2((float)-s[0], (float)-s[1]);
        game.pl().face = s[1] > 0 ? 1 : s[1] < 0 ? 0 : (s[0] > 0 ? 3 : 2);
        view.snap(game);
        placed = true;
        std::printf("script: chestgo: a %s chest at %d %d\n", gear::lootSrcName(LootOps::chestSource(game, bx, by).src), bx, by);
        break;
      }
    }
    if (!placed) fail(c.line, "chestgo " + want + ": no reachable closed chest here");
    else { lastChestX = bx; lastChestY = by; }
  } else if (op == "chestaway") {
    // (M8b SOURCES) chestaway: right after `key e`, step 4 tiles clear of the chest chestgo found (outside the
    // pickups' 40 px pull), toward the map's middle, so a shot shows the find lying on the floor
    Map& m = game.map();
    bool moved = false;
    std::vector<std::pair<int, int>> dirs = {{0, 1}, {1, 0}, {-1, 0}, {0, -1}, {1, 1}, {-1, 1}, {1, -1}, {-1, -1}};
    const int cx = m.w / 2 - lastChestX, cy = m.h / 2 - lastChestY;
    std::stable_sort(dirs.begin(), dirs.end(), [&](const std::pair<int, int>& a, const std::pair<int, int>& b) {
      return a.first * cx + a.second * cy > b.first * cx + b.second * cy;
    });
    for (int dist = 4; dist <= 6 && !moved && lastChestX >= 0; dist++)
      for (const auto& d : dirs) {
        const int x = lastChestX + d.first * dist, y = lastChestY + d.second * dist;
        if (m.blocked(x, y)) continue;
        game.pl().p = Vec2(x * TILE + 8.0f, y * TILE + 10.0f);
        game.pl().aim = Vec2((float)-d.first, (float)-d.second);
        moved = true;
        break;
      }
    if (!moved) fail(c.line, "chestaway: no free tile 4-6 tiles from the chest");
  } else if (op == "strip") {
    for (ItemKind k : {ItemKind::Weapon, ItemKind::Bow, ItemKind::Staff, ItemKind::Armor, ItemKind::Helmet, ItemKind::Shield,
                       ItemKind::Ring, ItemKind::Amulet, ItemKind::Gloves, ItemKind::Boots, ItemKind::Cloak})
      if (int* e = game.equipSlot(k)) if (*e >= 0) game.useItem(*e);
  } else if (op == "god") {
    game.godMode = arg(1).empty() || arg(1) != "0";
  } else if (op == "hour") {
    game.hour = (float)std::atof(arg(1).c_str());
  } else if (op == "menu") {
    view.openMenu(game, std::atoi(arg(1).c_str()));
  } else if (op == "log") {
    // printed above
  } else if (op == "pos") {   // M1: print where the player is (window tile, global tile, window origin, site)
    int tx = 0, ty = 0;
    plTile(tx, ty);
    std::printf("script: pos window tile %d,%d global %d,%d origin %d,%d inside %d site %s\n", tx, ty, game.world.ox + tx, game.world.oy + ty,
                game.world.ox, game.world.oy, game.inside ? 1 : 0, game.curSite >= 0 ? game.world.sites[(size_t)game.curSite].name.c_str() : "-");
    if (game.curSite >= 0) {
      int guards = 0, folk = 0;
      for (size_t k = 1; k < game.actors.size(); k++)
        if (game.actors[k].npc && game.actors[k].site == game.curSite) (game.actors[k].role == Role::Guard ? guards : folk)++;
      const Site& cs = game.world.sites[(size_t)game.curSite];
      auto it = game.world.siteSpawns.find(game.curSite);
      std::printf("script:   %s %s, %d buildings, %zu spawns; in play %d townsfolk, %d guards\n", siteTypeName(cs.type), cs.name.c_str(), cs.bldgCount,
                  it == game.world.siteSpawns.end() ? (size_t)0 : it->second.size(), folk, guards);
    }
    if (arg(1) == "map") {   // the tiles around: '#' blocked, '.' open, '@' the player
      for (int y = ty - 3; y <= ty + 3; y++) {
        std::string row;
        for (int x = tx - 8; x <= tx + 8; x++) row += (x == tx && y == ty) ? '@' : (game.map().blocked(x, y) ? '#' : '.');
        std::printf("script:   %s\n", row.c_str());
      }
      std::printf("script:   player px %.1f,%.1f state %d mode %d\n", game.pl().p.x, game.pl().p.y, (int)game.pl().st, (int)game.mode);
    }
  } else if (op == "expect") {
    std::string what = arg(1), want = arg(2);
    if (what == "mode") {
      Mode m;
      static const char* n[] = {"title", "play", "dialogue", "menu", "shop", "levelup", "dead", "paused", "creator", "forge", "build"};
      if (!modeByName(want, m)) fail(c.line, "expect mode: unknown mode '" + want + "'");
      else if (game.mode != m) fail(c.line, "expected mode " + want + ", got " + n[(int)game.mode]);
    } else if (what == "gold") {
      if (game.gold != std::atoi(want.c_str())) fail(c.line, "expected gold " + want + ", got " + std::to_string(game.gold));
    } else if (what == "name") {
      if (game.app.name != want) fail(c.line, "expected name " + want + ", got " + game.app.name);
    } else if (what == "background") {
      if ((int)game.background != std::atoi(want.c_str())) fail(c.line, "expected background " + want + ", got " + std::to_string((int)game.background));
    } else if (what == "slot") {
      static const struct { const char* n; ItemKind k; } sk[] = {
          {"weapon", ItemKind::Weapon}, {"bow", ItemKind::Bow}, {"staff", ItemKind::Staff}, {"armor", ItemKind::Armor},
          {"helmet", ItemKind::Helmet}, {"shield", ItemKind::Shield}, {"ring", ItemKind::Ring}, {"amulet", ItemKind::Amulet},
          {"gloves", ItemKind::Gloves}, {"boots", ItemKind::Boots}, {"cloak", ItemKind::Cloak}};
      int* e = nullptr;
      for (auto& q : sk) if (want == q.n) e = game.equipSlot(q.k);
      bool on = arg(3) != "0";
      if (!e) fail(c.line, "expect slot: unknown slot '" + want + "'");
      else if ((*e >= 0) != on) fail(c.line, "expected slot " + want + (on ? " worn" : " empty"));
    } else if (what == "floor") {
      if (game.subFloor != std::atoi(want.c_str())) fail(c.line, "expected floor " + want + ", got " + std::to_string(game.subFloor));
    } else if (what == "kingdom") {   // M1: the settlement here belongs to a kingdom (expect kingdom capital: its capital)
      int si = game.inside && game.subBldg >= 0 ? game.world.over.bldgs[(size_t)game.subBldg].site : game.curSite;
      const Kingdom* K = game.world.kingdomOf(si);
      if (!K) fail(c.line, "expected a kingdom's settlement here");
      else if (want == "capital" && !game.world.sites[(size_t)si].capital) fail(c.line, "expected " + game.world.sites[(size_t)si].name + " to be the capital of " + K->name);
      else std::printf("script: %s, kingdom of %s%s\n", game.world.sites[(size_t)si].name.c_str(), K->name.c_str(), game.world.sites[(size_t)si].capital ? " (capital)" : "");
    } else if (what == "npc") {   // M1: someone in play with this role (king, guard...) or title word (royal)
      bool ok = false;
      for (size_t k = 1; k < game.actors.size(); k++) {
        const Actor& ac = game.actors[k];
        if (!ac.npc || ac.st == AState::Dead) continue;
        if ((want == "king" && ac.role == Role::King) || (want == "guard" && ac.role == Role::Guard) || (want == "jarl" && ac.role == Role::Jarl) ||
            (want == "innkeeper" && ac.role == Role::Innkeeper) || (want == "royal" && ac.name.find("ROYAL") != std::string::npos))
          ok = true;
      }
      if (!ok) fail(c.line, "expected an npc '" + want + "' in play");
    } else if (what == "inside") {
      if ((want != "0") != game.inside) fail(c.line, std::string("expected inside ") + want + ", got " + (game.inside ? "1" : "0"));
    } else if (what == "quest" || what == "heard" || what == "option" || what == "text" || what == "tracked") {
      // the rest of the line (original case) is a substring to look for
      auto rest = [&](size_t from) { std::string r; for (size_t k = from; k < c.a.size(); k++) { if (k > from) r += ' '; r += c.a[k]; } return r; };
      if (what == "quest") {   // expect quest active|complete|done|none <title words>
        std::string t = rest(3);
        const Quest* q = nullptr;
        for (const Quest& x : game.quests) if (x.title.find(t) != std::string::npos) q = &x;
        static const char* sn[] = {"active", "complete", "done"};
        std::string got = q ? sn[(int)q->state] : "none";
        if (got != want) fail(c.line, "expected quest '" + t + "' " + want + ", got " + got);
      } else if (what == "tracked") {   // expect tracked <title words>
        std::string t = rest(2), got = "none";
        for (const Quest& x : game.quests) if (x.id == game.trackedQuest) got = x.title;
        if (got.find(t) == std::string::npos) fail(c.line, "expected tracked quest '" + t + "', got " + got);
      } else if (what == "heard") {   // a notice (say) since the script began contains these words
        std::string t = rest(2);
        bool ok = false;
        for (const std::string& h : heard) if (h.find(t) != std::string::npos) ok = true;
        if (!ok) fail(c.line, "never heard '" + t + "'" + (heard.empty() ? std::string() : " (last: " + heard.back() + ")"));
      } else if (what == "option") {   // the open dialogue offers an option containing these words
        std::string t = rest(2);
        // (fixer M7b r1) on show, or behind a header the player can tap (never one no page shows)
        const bool ok = game.mode == Mode::Dialogue && dlgReach(game, [&](const DlgOpt& o) { return o.label.find(t) != std::string::npos; }, false) >= 0;
        if (!ok) fail(c.line, "no dialogue option '" + t + "' the player can reach" + (game.dlg.opts.empty() ? std::string() : " (first: " + game.dlg.opts[0].label + ")"));
      } else {   // text: the open dialogue's text contains these words
        std::string t = rest(2);
        if (game.mode != Mode::Dialogue || game.dlg.text.find(t) == std::string::npos) fail(c.line, "dialogue text lacks '" + t + "': " + game.dlg.text);
      }
    } else if (!startExt("expect:" + what, c)) fail(c.line, "expect: unknown check '" + what + "'");
  } else if (!startExt(op, c)) {
    fail(c.line, "unknown command '" + op + "'");
  }
}

void App::scriptStep(float dt) {
  if (game.noticeT > 0 && !game.notice.empty() && (heard.empty() || heard.back() != game.notice)) heard.push_back(game.notice);
  // releases first, so a hold ending this frame lets go before new presses
  for (size_t i = 0; i < held.size();) {
    if (scriptT >= held[i].until) {
      view.scriptHold(held[i].sc, false);
      pushKey(held[i].sc, false);
      held.erase(held.begin() + i);
    } else i++;
  }
  for (size_t i = 0; i < drags.size();) {
    if (scriptT >= drags[i].at) { pushFinger(SDL_EVENT_FINGER_MOTION, drags[i].id, drags[i].x, drags[i].y); drags.erase(drags.begin() + i); }
    else i++;
  }
  for (size_t i = 0; i < lifts.size();) {
    if (scriptT >= lifts[i].at) { pushFinger(SDL_EVENT_FINGER_UP, lifts[i].id, lifts[i].x, lifts[i].y); lifts.erase(lifts.begin() + i); }
    else i++;
  }
  if (walk.on) { stepWalk(dt); return; }   // script time stands still while walking
  if (extRun.e) { callExt(dt); return; }   // ... and while a registered command is still busy
  while (scriptNext < script.size() && script[scriptNext].t <= scriptT && running) {
    runCmd(script[scriptNext++]);
    if (walk.on || extRun.e) return;
  }
  if (!witness && scriptNext >= script.size() && held.empty() && lifts.empty()) {
    if (scriptEndT < 0) scriptEndT = scriptT;
    if (scriptT - scriptEndT > 2.0f) { std::printf("script: end of script\n"); running = false; }
  }
  scriptT += dt;
}
