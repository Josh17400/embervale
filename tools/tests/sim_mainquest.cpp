// rpg_test: THE DRAGON'S SHADOW end to end on the endless world, played by a teleporting bot (SIM lane, M1). Also run
// per seed by runSeed (seed_run.cpp), and on its own:
//   rpg_test --mainquest [--seeds A..B]
// The bot fast-travels to the story city, walks into the Keep and takes the quest from the jarl, travels to each of the
// three shard ruins, goes in by the entrance and slays the warlord, saves and reloads half way (every reference by
// stable id), returns the shards to the jarl, travels to Ashfang's lair and kills the dragon. Each step checks what the
// game says: the quest stage, the tracked target (questTarget) pointing at the right place, discovery, the window
// recentring on every teleport.
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include "tools/tests/tests.h"

int mainQuestBot(uint64_t seed, bool verbose);

namespace {
Quest* mainQuest(Game& g) {
  for (Quest& q : g.quests) if (q.type == QType::Main) return &q;
  return nullptr;
}
void tick(Game& g, int frames) {
  for (int f = 0; f < frames; f++) {
    g.update(SIM_DT, Input());
    g.events.clear();
    if (g.mode == Mode::LevelUp) { g.chooseLevelUp(0); g.mode = Mode::Play; }
    if (g.mode == Mode::Shop) g.mode = Mode::Play;
    if (g.mode == Mode::Dead) g.respawn();
  }
}
int findRole(const Game& g, Role r) {
  for (size_t k = 1; k < g.actors.size(); k++) if (g.actors[k].npc && g.actors[k].role == r && g.actors[k].st != AState::Dead) return (int)k;
  return -1;
}
// talk to the actor at index k and pick the option whose label contains `want` (false if there is none)
bool talkAndChoose(Game& g, int k, const char* want) {
  g.mode = Mode::Play;
  g.pl().p = g.actors[(size_t)k].p + Vec2(0, 14);
  Input t; t.interact = true;
  g.update(SIM_DT, t);
  if (g.mode != Mode::Dialogue) return false;
  for (size_t o = 0; o < g.dlg.opts.size(); o++)
    if (g.dlg.opts[o].label.find(want) != std::string::npos) { g.dialogueChoose((int)o); g.mode = Mode::Play; return true; }
  g.mode = Mode::Play;
  return false;
}
// into the story city's Keep, to the jarl
int enterKeepFindJarl(Game& g) {
  const Site cap = g.world.sites[(size_t)g.world.capital];
  g.world.ensureSiteRecords(g.world.capital);
  int keep = -1;
  for (int b = cap.bldgFirst; b < cap.bldgFirst + cap.bldgCount; b++)
    if (g.world.over.bldgs[(size_t)b].type == art::Building::Keep) keep = b;
  if (keep < 0) return -2;
  if (!g.debugEnterBuilding(keep, 0)) return -3;
  g.mode = Mode::Play;
  tick(g, 2);
  return findRole(g, Role::Jarl);
}
}  // namespace

int mainQuestBot(uint64_t seed, bool verbose) {
  int bad = 0;
  auto fail = [&](const std::string& s) { out("FAIL: main quest bot: %s\n", s.c_str()); bad++; };
  auto t0 = std::chrono::steady_clock::now();
  Game g(seed);
  g.newEndlessGame(seed);
  g.mode = Mode::Play;
  g.godMode = true;
  g.noWildSpawns = true;
  g.debugKit();
  int travels = 0;
  Quest* mq = mainQuest(g);
  if (!mq) { fail("no main quest at the start"); return bad; }
  if (g.world.capital < 0 || g.world.sites[(size_t)g.world.capital].type != SiteType::City) { fail("no story city"); return bad; }
  const ew::Gid capId = g.world.sites[(size_t)g.world.capital].id;
  // the journal points at the story city (its keep's door once its records are loaded, else the city)
  {
    int tx = 0, ty = 0;
    if (!g.questTarget(mq->id, tx, ty)) fail("stage 0: the main quest has no target");
    const Site& c = g.world.sites[(size_t)g.world.capital];
    if (std::abs(tx - c.ex) > c.r.w + 8 || std::abs(ty - c.ey) > c.r.h + 8) fail("stage 0: the target is not in the story city");
  }
  // 1. the jarl
  g.world.sites[(size_t)g.world.capital].discovered = true;
  if (!g.fastTravel(g.world.capital)) { fail("fast travel to the story city refused"); return bad; }
  travels++;
  if (g.world.sites[(size_t)g.world.capital].id != capId) fail("the story city's handle changed");
  int jarl = enterKeepFindJarl(g);
  if (jarl == -2) { fail("the story city has no keep"); return bad; }
  if (jarl < 0) { fail("no jarl in the story city's keep"); return bad; }
  if (!talkAndChoose(g, jarl, "STOPPED")) { fail("the jarl offers no main quest option"); return bad; }
  mq = mainQuest(g);
  if (mq->stage != 1) fail("after the jarl: stage " + std::to_string(mq->stage));
  // 2. the three shard ruins
  for (int round = 0; round < 3 && mainQuest(g)->stage == 1; round++) {
    int tx = 0, ty = 0;
    mq = mainQuest(g);
    if (!g.questTarget(mq->id, tx, ty)) { fail("stage 1: no target"); break; }
    int ruin = -1;
    for (int i = 0; i < (int)g.world.sites.size(); i++) {
      const Site& s = g.world.sites[(size_t)i];
      if (s.mainQuest && !s.cleared && s.ex == tx && s.ey == ty) ruin = i;
    }
    if (ruin < 0) { fail("stage 1: the target is not an uncleared shard ruin"); break; }
    if (!g.world.sites[(size_t)ruin].discovered) fail("stage 1: shard ruin not discovered (the jarl marks them)");
    const ew::Gid rid = g.world.sites[(size_t)ruin].id;
    const int rep0 = g.world.sstats.entrancesRepaired;
    if (!g.fastTravel(ruin)) { fail("fast travel to a shard ruin refused"); break; }
    // (M1 round 3) every cave and ruin stamps its door on its own entrance tile: a repair is a generator bug
    if (g.world.sstats.entrancesRepaired > rep0)
      fail("seed " + std::to_string(seed) + ": " + std::to_string(g.world.sstats.entrancesRepaired - rep0) + " dungeon entrances around " +
           g.world.sites[(size_t)g.world.siteHandle(rid)].name + " had lost their door to the generator (World put them back)");
    travels++;
    ruin = g.world.siteHandle(rid);
    const Site s = g.world.sites[(size_t)ruin];
    if (!g.world.over.in(s.ex, s.ey)) { fail("the shard ruin is outside the window after travelling there"); break; }
    // walk in by the entrance
    g.pl().p = Vec2(s.ex * TILE + 8.0f, s.ey * TILE + 6.0f);
    tick(g, 2);
    if (!g.inside || g.subSite != ruin) {
      const Map& m = g.world.over;
      int at = g.world.siteAt(s.ex, s.ey);
      std::string around;
      for (int dy = -3; dy <= 3; dy++)
        for (int dx = -3; dx <= 3; dx++) {
          int pr = m.propAt(s.ex + dx, s.ey + dy);
          if (pr == (int)art::Prop::IronDoor + 1 || pr == (int)art::Prop::CaveEntrance + 1)
            around += " door at " + std::to_string(dx) + "," + std::to_string(dy);
        }
      out("  ground at the entrance %d, solid %d%s\n", (int)m.at(s.ex, s.ey), m.blocked(s.ex, s.ey), around.c_str());
      fail("could not enter shard ruin " + s.name + " (entrance prop " + std::to_string(m.propAt(s.ex, s.ey)) + ", site there " +
           (at >= 0 ? g.world.sites[(size_t)at].name + " " + siteTypeName(g.world.sites[(size_t)at].type) : std::string("none")) +
           ", inside " + std::to_string(g.inside) + " site " + std::to_string(g.subSite) + ")");
      break;
    }
    int boss = -1;
    for (size_t k = 1; k < g.actors.size(); k++) if (g.actors[k].boss && g.actors[k].st != AState::Dead) boss = (int)k;
    if (boss < 0) { fail("no warlord in " + s.name); break; }
    if (verbose) out("  shard ruin %s: %s\n", s.name.c_str(), g.actors[(size_t)boss].name.c_str());
    int have0 = mainQuest(g)->have;
    g.actors[(size_t)boss].hp = 0.5f; g.actors[(size_t)boss].burnT = 1.0f;   // the player's fire finishes it
    tick(g, 30);
    if (!g.world.sites[(size_t)ruin].cleared) fail("slaying the warlord did not clear " + s.name);
    if (mainQuest(g)->stage == 1 && mainQuest(g)->have != have0 + 1) fail("no shard from " + s.name);
  }
  mq = mainQuest(g);
  if (mq->stage != 2) { fail("after three ruins: stage " + std::to_string(mq->stage) + ", shards " + std::to_string(mq->have)); return bad; }
  // 3. save and reload half way: everything by stable id
  {
    std::vector<uint8_t> a, b;
    g.serialize(a);
    Game h(1);
    if (!h.deserialize(a)) { fail("the half-way save did not load"); return bad; }
    h.serialize(b);
    if (a != b) fail("half-way save round trip is not byte-identical");
    h.mode = Mode::Play; h.godMode = true; h.noWildSpawns = true;
    if (h.world.sites[(size_t)h.world.capital].id != capId) fail("the story city is another after reloading");
    int shards = 0;
    for (const Item& it : h.inv) if (it.kind == ItemKind::Quest && it.name == "EMBER SHARD") shards += it.count;
    if (shards != 3) fail("shards after reloading: " + std::to_string(shards));
    g = h;
  }
  // 4. back to the jarl
  if (!g.fastTravel(g.world.capital)) { fail("fast travel back to the story city refused"); return bad; }
  travels++;
  jarl = enterKeepFindJarl(g);
  if (jarl < 0) { fail("no jarl on the return"); return bad; }
  if (!talkAndChoose(g, jarl, "HUNT THE DRAGON")) { fail("the jarl does not take the shards"); return bad; }
  mq = mainQuest(g);
  if (mq->stage != 3) { fail("after returning the shards: stage " + std::to_string(mq->stage)); return bad; }
  // 5. Ashfang
  if (g.world.lair < 0) { fail("no lair"); return bad; }
  {
    int tx = 0, ty = 0;
    const Site& L = g.world.sites[(size_t)g.world.lair];
    if (!g.questTarget(mq->id, tx, ty) || tx != L.ex || ty != L.ey) fail("stage 3 does not point at the lair");
  }
  if (!g.fastTravel(g.world.lair)) { fail("fast travel to the lair refused (discovered " + std::to_string(g.world.sites[(size_t)g.world.lair].discovered) + ")"); return bad; }
  travels++;
  int dragon = -1;
  for (int f = 0; f < 60 && dragon < 0; f++) {
    tick(g, 1);
    for (size_t k = 1; k < g.actors.size(); k++) if (g.actors[k].mon == art::Monster::Dragon && !g.actors[k].npc) dragon = (int)k;
  }
  if (dragon < 0) { fail("Ashfang never appeared at the lair"); return bad; }
  g.actors[(size_t)dragon].hp = 0.5f; g.actors[(size_t)dragon].burnT = 1.0f;
  g.actors[(size_t)dragon].fly = false;
  tick(g, 40);
  mq = mainQuest(g);
  if (mq->state != QState::Done) fail("killing Ashfang did not finish the main quest (stage " + std::to_string(mq->stage) + ")");
  double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
  if (verbose || bad) out("main quest bot seed %llu: %s in %.0f ms, %d fast travels, %d window moves\n", (unsigned long long)seed,
                          bad ? "FAILED" : "done", ms, travels, g.world.sstats.shifts);
  return bad;
}

namespace {
int cmdMainQuest(int argc, char** argv) {
  uint64_t a = 1, b = 10;
  for (int i = 1; i < argc; i++)
    if (!strcmp(argv[i], "--seeds") && i + 1 < argc) parseSeedRange(argv[++i], a, b);
  int bad = 0, failed = 0;
  for (uint64_t s = a; s <= b; s++) {
    g_curSeed = s;
    int f = mainQuestBot(s, true);
    bad += f;
    if (f) failed++;
  }
  printf("mainquest: %llu seeds, %d failed\n", (unsigned long long)(b - a + 1), failed);
  return bad ? 1 : 0;
}
}  // namespace

RPG_TEST_CMD("--mainquest", "THE DRAGON'S SHADOW end to end on the endless world by a teleporting bot [--seeds A..B]", cmdMainQuest);
