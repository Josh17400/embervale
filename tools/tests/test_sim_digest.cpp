// rpg_test --sim-digest: the sim's identity probe (docs/SOLID_PLAN.md wave 0, step 0.2). A refactor step that moves code
// must leave every bit of the simulation as it was; this prints a fingerprint of the whole sim at fixed checkpoints so a
// later build can be compared against the 119de74 record.
//
//   rpg_test --sim-digest [--seeds 1..20] [--secs 120] [--every 10] [--fight 30] [--fevery 2]   print the record
//   rpg_test --sim-digest --check FILE [--seeds A..B]                          compare against a record
//
// Each seed: a new endless game (newTestGame) in the Witness deterministic mode (streamThreads off, syncStreams on),
// the opening played by the opening bot (seed_run.cpp openingStep), then the wandering bot of seed_run.cpp (hunt the
// nearest hostile, else wander; bow, spell, roll, interact on their cadences; god mode) for --secs sim seconds at
// SIM_DT, then a seeded SKIRMISH of --fight sim seconds (default 30): hostiles are spawned beside the bot (a bandit
// every 3 s while fewer than 3 stand within 120 px, every fourth foe a wolf; levels from the hero's) and the bot fights
// them, so combat timing (windups, strikes, cooldowns, hits: st, stT, atkCd, hp) is in the record. The wander phase
// alone never meets a hostile human in most seeds (an enemy windup constant moved nothing in it, judge round 2).
// At the end of the opening (t=0), every --every seconds of the wander and every --fevery seconds (default 2) of the
// skirmish (t = secs + 2, secs + 4, ...) it writes three lines:
//   C <seed> <t> mode=.. time=.. day=.. hour=.. gold=.. kills=.. xp=.. lvl=.. ser=<hash> len=<bytes> act=<hash> n=<actors>
//     rng=<Game rng state> bot=<bot rng state> marks=<hash>/<count> evs=<events so far>
//   S <seed> <t> <one 32-bit hash per 1 KB block of the Game::serialize bytes>
//   A <seed> <t> <per actor: id:pos.hp.mode.anim.target as 16-bit hashes of those field groups>
// The C line's act hash covers every listed field exactly (64-bit); the S and A lines only serve to NAME what changed.
// --check runs the seeds the record holds (or --seeds), compares line by line and, at the first differing checkpoint
// of each seed, names the field: a C field, the first differing serialize block (byte range), or the first differing
// actor (index, id) and field group. Exit code 0 only when every checkpoint is identical.
// Floats are compared as their bit patterns (strict identity, owner ruling D3).
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>
#include "rpg/sim/power_ops.h"
#include "rpg/sim/skill_ops.h"
#include "tools/tests/tests.h"

bool openingStep(Game& g, std::vector<int>& path, int& clock, Input& in);   // seed_run.cpp
void newTestGame(Game& g, uint64_t seed);                                  // seed_run.cpp

namespace {

struct Fnv {
  uint64_t h = 0xcbf29ce484222325ull;
  void bytes(const void* p, size_t n) {
    const uint8_t* b = (const uint8_t*)p;
    for (size_t i = 0; i < n; i++) { h ^= b[i]; h *= 0x100000001b3ull; }
  }
  template <class T> void v(const T& x) { bytes(&x, sizeof x); }
  void f(float x) { uint32_t u; std::memcpy(&u, &x, 4); v(u); }
};
uint32_t fbits(float x) { uint32_t u; std::memcpy(&u, &x, 4); return u; }
uint16_t fold16(uint64_t h) { return (uint16_t)(h ^ (h >> 16) ^ (h >> 32) ^ (h >> 48)); }

// the actor field groups the plan names: position, hp, mode, anim, target
enum Grp { GPos, GHp, GMode, GAnim, GTarget, GCOUNT };
const char* kGrpName[GCOUNT] = {"position (p, vel, knock, home, goal)", "hp (hp, maxHp, armor, dmg, level)",
                                "mode (st, stT, hostile, aggro, faction, role, flags)",
                                "anim (animT, face, gait, moveV, posture, postureT, blockT, drawnT)",
                                "target (target, thinkT, navGoal, navNext, navT, atkCd, comboT)"};
void actorGroups(const Actor& a, uint64_t out[GCOUNT]) {
  Fnv p, h, m, an, t;
  p.f(a.p.x); p.f(a.p.y); p.f(a.vel.x); p.f(a.vel.y); p.f(a.knock.x); p.f(a.knock.y);
  p.f(a.home.x); p.f(a.home.y); p.f(a.goal.x); p.f(a.goal.y); p.f(a.aim.x); p.f(a.aim.y);
  h.f(a.hp); h.f(a.maxHp); h.f(a.armor); h.f(a.dmg); h.v(a.level); h.f(a.iframes); h.f(a.burnT); h.f(a.slowT);
  m.v((uint8_t)a.st); m.f(a.stT); m.v(a.hostile); m.v(a.aggro); m.v((uint8_t)a.faction); m.v((uint8_t)a.role);
  m.v(a.fleeing); m.v(a.indoors); m.v(a.asleep); m.v(a.militia); m.v(a.combo); m.v(a.hitDone); m.v(a.atkN);
  an.f(a.animT); an.v(a.face); an.v(a.gait); an.f(a.moveV); an.v((uint8_t)a.posture); an.f(a.postureT); an.f(a.blockT);
  an.f(a.drawnT);
  t.v(a.target); t.f(a.thinkT); t.v(a.navGoal); t.v(a.navNext); t.f(a.navT); t.f(a.atkCd); t.f(a.shootCd); t.f(a.comboT);
  out[GPos] = p.h; out[GHp] = h.h; out[GMode] = m.h; out[GAnim] = an.h; out[GTarget] = t.h;
}

constexpr size_t kBlock = 1024;
const char* g_dumpDir = nullptr;   // --dump DIR: also write each checkpoint's serialize bytes (diagnosis of a FAIL)
int g_fight = 30, g_fevery = 2;    // the skirmish phase (--fight / --fevery; a record's header sets them)

// the three lines of one checkpoint
void checkpoint(Game& g, uint64_t seed, int t, const Rng& bot, int evs, std::vector<std::string>& lines) {
  std::vector<uint8_t> ser;
  g.serialize(ser);
  if (g_dumpDir) {
    std::string path = std::string(g_dumpDir) + "/ser_" + std::to_string(seed) + "_" + std::to_string(t) + ".bin";
    if (FILE* f = std::fopen(path.c_str(), "wb")) { std::fwrite(ser.data(), 1, ser.size(), f); std::fclose(f); }
  }
  Fnv sh;
  sh.bytes(ser.data(), ser.size());
  Fnv ah;
  std::string aline;
  char buf[160];
  for (size_t k = 0; k < g.actors.size(); k++) {
    const Actor& a = g.actors[k];
    uint64_t gr[GCOUNT];
    actorGroups(a, gr);
    ah.v(a.id);
    for (int i = 0; i < GCOUNT; i++) ah.v(gr[i]);
    std::snprintf(buf, sizeof buf, " %d:%04x.%04x.%04x.%04x.%04x", a.id, fold16(gr[0]), fold16(gr[1]), fold16(gr[2]), fold16(gr[3]),
                  fold16(gr[4]));
    aline += buf;
  }
  Fnv mh;
  for (const auto& kv : g.marks) { mh.v(kv.first); mh.v(kv.second); }
  const Rng& gr = PowerOps::rng(g);
  char c[512];
  std::snprintf(c, sizeof c,
                "C %llu %d mode=%d time=%08x day=%d hour=%08x gold=%d kills=%d xp=%d lvl=%d ser=%016llx len=%zu act=%016llx n=%zu "
                "rng=%016llx bot=%016llx marks=%016llx/%zu evs=%d",
                (unsigned long long)seed, t, (int)g.mode, fbits(g.time), g.day, fbits(g.hour), g.gold, g.kills, g.plXp, g.plLevel,
                (unsigned long long)sh.h, ser.size(), (unsigned long long)ah.h, g.actors.size(), (unsigned long long)gr.s,
                (unsigned long long)bot.s, (unsigned long long)mh.h, g.marks.size(), evs);
  lines.push_back(c);
  std::string s = "S " + std::to_string(seed) + " " + std::to_string(t);
  for (size_t o = 0; o < ser.size(); o += kBlock) {
    Fnv b;
    b.bytes(ser.data() + o, std::min(kBlock, ser.size() - o));
    std::snprintf(buf, sizeof buf, " %08x", (uint32_t)(b.h ^ (b.h >> 32)));
    s += buf;
  }
  lines.push_back(s);
  lines.push_back("A " + std::to_string(seed) + " " + std::to_string(t) + aline);
}

// a screen the bot opened by its interact cadence (the home's cooking / storage screen is Mode::Build) is closed as the
// view's closeBuild does: left open it froze the sim for the rest of the run (9 of 20 seeds at 119de74, digest v1)
void closeScreens(Game& g) {
  if (g.mode == Mode::Build) { g.mode = Mode::Play; g.home.ui.mode = home::UiMode::None; g.home.ui.why.clear(); }
  if (g.mode == Mode::Menu || g.mode == Mode::Paused) g.mode = Mode::Play;
}

// one seed: the opening, then the wandering bot (seed_run.cpp runSeed's loop, without its timing prints)
void runDigest(uint64_t seed, float secs, int every, std::vector<std::string>& lines) {
  g_curSeed = seed;
  Game g(seed);
  // the Witness determinism switches (game.h): no stream worker, and every cold generation (named uniques, world
  // bosses) made at once on this thread. With the worker on, the plain bot is NOT bit-repeatable: at 119de74 an A/A
  // pair differed in 5 of 20 seeds (a mark and the serialize bytes; actors and RNG equal), from worker timing.
  g.streamThreads = false;
  g.syncStreams = true;
  // ... and the realm's kingdom instantiation, which is time-sliced by the WALL CLOCK (Realm::focusBudgetMs, 2 ms a
  // call): with the budget on, the realm block of the save differed between A/A runs at 119de74 (1-4 seeds of 20, from
  // the first checkpoint). Unbounded, as Realm::focusNow does, every call finishes its work and the run is repeatable.
  newTestGame(g, seed);
  g.realm.focusBudgetMs = 1e18;   // (after the new game: Realm::reset puts the 2 ms back)
  g.mode = Mode::Play;
  g.godMode = true;
  Rng r(seed);
  Input in;
  int evCount = 0;
  {
    std::vector<int> path;
    int clock = 0;
    for (int of = 0; of < 300 * 60; of++) {
      Input oin;
      if (openingStep(g, path, clock, oin)) break;
      if (g.pl().st != AState::Dead) {
        for (size_t k = 1; k < g.actors.size(); k++)
          if (g.actors[k].hostile && g.actors[k].st != AState::Dead && len2(g.actors[k].p - g.pl().p) < 22 * 22) { oin.attack = of % 12 == 0; break; }
      }
      g.update(SIM_DT, oin);
      evCount += (int)g.events.size();
      g.events.clear();
      if (g.mode == Mode::Dialogue || g.mode == Mode::Shop || g.mode == Mode::LevelUp || g.mode == Mode::Forge) g.mode = Mode::Play;
      if (g.mode == Mode::Dead) g.respawn();
    }
  }
  const int frames = (int)(secs * 60), per = every * 60;
  for (int f = 0; f <= frames; f++) {
    if (f % per == 0) checkpoint(g, seed, f / 60, r, evCount, lines);
    if (f == frames) break;
    int tgt = -1;
    float bd = 200 * 200;
    for (size_t k = 1; k < g.actors.size(); k++) {
      const Actor& e = g.actors[k];
      if (!e.hostile || e.st == AState::Dead) continue;
      float d2 = len2(e.p - g.pl().p);
      if (d2 < bd) { bd = d2; tgt = (int)k; }
    }
    if (tgt >= 0) in.move = norm(g.actors[tgt].p - g.pl().p) * (bd > 18 * 18 ? 1.0f : 0.0f);
    else if (f % 90 == 0) { float a = r.f() * TAU; in.move = Vec2(std::cos(a), std::sin(a)); }
    in.attack = (f % 20) == 0 && tgt >= 0 && bd < 30 * 30;
    in.bow = (f % 97) == 0 && g.eqBow >= 0;
    in.spell = (f % 151) == 0;
    in.roll = (f % 211) == 0;
    in.interact = (f % 60) == 0;
    g.update(SIM_DT, in);
    evCount += (int)g.events.size();
    g.events.clear();
    if (g.mode == Mode::Dialogue) g.dialogueChoose(0), g.mode = Mode::Play;
    if (g.mode == Mode::Shop || g.mode == Mode::Forge) g.mode = Mode::Play;
    if (g.mode == Mode::Dead) g.respawn();
    closeScreens(g);
    if (g.pl().hp < g.pl().maxHp * 0.35f && f % 30 == 0) {
      Input q;
      q.potion = true;
      g.update(SIM_DT, q);
      evCount += (int)g.events.size();
      g.events.clear();
    }
  }
  // the skirmish: foes spawned beside the bot on a fixed ring (no libm), the bot closes in and swings, rolls and casts
  static const Vec2 kRing[8] = {{36, 0}, {-26, 26}, {0, -36}, {26, 26}, {-36, 0}, {26, -26}, {0, 36}, {-26, -26}};
  const int fFrames = g_fight * 60, fPer = std::max(1, g_fevery) * 60, t0 = (int)secs;
  int spawned = 0;
  Input fin;
  for (int f = 1; f <= fFrames; f++) {
    if ((f - 1) % 180 == 0) {
      int near = 0;
      for (size_t k = 1; k < g.actors.size(); k++)
        if (g.actors[k].hostile && g.actors[k].st != AState::Dead && len2(g.actors[k].p - g.pl().p) < 120 * 120) near++;
      if (near < 3) {
        const Vec2 at = g.pl().p + kRing[spawned % 8];
        const int lvl = std::max(1, g.plLevel + 1);
        if (spawned % 4 == 3) SkillOps::spawnMonster(g, art::Monster::Wolf, at, lvl);
        else PowerOps::spawnBandit(g, at, lvl);
        spawned++;
      }
    }
    int tgt = -1;
    float bd = 200 * 200;
    for (size_t k = 1; k < g.actors.size(); k++) {
      const Actor& e = g.actors[k];
      if (!e.hostile || e.st == AState::Dead) continue;
      float d2 = len2(e.p - g.pl().p);
      if (d2 < bd) { bd = d2; tgt = (int)k; }
    }
    fin.move = tgt >= 0 ? norm(g.actors[tgt].p - g.pl().p) * (bd > 16 * 16 ? 1.0f : 0.0f) : Vec2(0, 0);
    fin.attack = (f % 24) == 0 && tgt >= 0 && bd < 30 * 30;
    fin.spell = (f % 173) == 0;
    fin.roll = (f % 233) == 0;
    g.update(SIM_DT, fin);
    evCount += (int)g.events.size();
    g.events.clear();
    if (g.mode == Mode::Dialogue) g.dialogueChoose(0), g.mode = Mode::Play;
    if (g.mode == Mode::Shop || g.mode == Mode::Forge || g.mode == Mode::LevelUp) g.mode = Mode::Play;
    if (g.mode == Mode::Dead) g.respawn();
    closeScreens(g);
    if (f % fPer == 0) checkpoint(g, seed, t0 + f / 60, r, evCount, lines);
  }
}

std::vector<std::string> words(const std::string& s) {
  std::vector<std::string> w;
  std::istringstream is(s);
  std::string x;
  while (is >> x) w.push_back(x);
  return w;
}

// the first difference between a baseline and a fresh checkpoint (the three lines of each), in words
std::string explain(const std::vector<std::string>& base, const std::vector<std::string>& now) {
  std::string why;
  auto bc = words(base[0]), nc = words(now[0]);
  for (size_t i = 3; i < std::max(bc.size(), nc.size()); i++) {
    std::string b = i < bc.size() ? bc[i] : "(none)", n = i < nc.size() ? nc[i] : "(none)";
    if (b != n) { why += (why.empty() ? "" : ", ") + b + " -> " + n.substr(n.find('=') + 1); }
  }
  auto bs = words(base[1]), ns = words(now[1]);
  for (size_t i = 3; i < std::max(bs.size(), ns.size()); i++) {
    if (i >= bs.size() || i >= ns.size() || bs[i] != ns[i]) {
      char b[96];
      std::snprintf(b, sizeof b, "; first differing serialize block %zu (bytes %zu..%zu)", i - 3, (i - 3) * kBlock, (i - 2) * kBlock - 1);
      why += b;
      break;
    }
  }
  auto ba = words(base[2]), na = words(now[2]);
  for (size_t i = 3; i < std::max(ba.size(), na.size()); i++) {
    if (i < ba.size() && i < na.size() && ba[i] == na[i]) continue;
    if (i >= ba.size() || i >= na.size()) { why += "; actor list length differs from index " + std::to_string(i - 3); break; }
    std::string bid = ba[i].substr(0, ba[i].find(':')), nid = na[i].substr(0, na[i].find(':'));
    if (bid != nid) { why += "; actor index " + std::to_string(i - 3) + " is id " + nid + " (was id " + bid + ")"; break; }
    std::string bg = ba[i].substr(ba[i].find(':') + 1), ng = na[i].substr(na[i].find(':') + 1);
    std::string fields;
    for (int gi = 0; gi < GCOUNT; gi++)
      if (bg.substr((size_t)gi * 5, 4) != ng.substr((size_t)gi * 5, 4)) fields += std::string(fields.empty() ? "" : " | ") + kGrpName[gi];
    why += "; first differing actor index " + std::to_string(i - 3) + (i == 3 ? " (the player)" : "") + " id " + bid + ": " + fields;
    break;
  }
  return why.empty() ? "lines differ" : why;
}

int simDigest(int argc, char** argv) {
  uint64_t A = 1, B = 20;
  bool seedsGiven = false;
  float secs = 120;
  int every = 10;
  const char* checkFile = nullptr;
  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "--seeds") && i + 1 < argc) {
      if (!parseSeedRange(argv[++i], A, B)) { printf("bad --seeds range '%s'\n", argv[i]); return 2; }
      seedsGiven = true;
    } else if (!strcmp(argv[i], "--secs") && i + 1 < argc) secs = (float)atof(argv[++i]);
    else if (!strcmp(argv[i], "--every") && i + 1 < argc) every = std::max(1, atoi(argv[++i]));
    else if (!strcmp(argv[i], "--fight") && i + 1 < argc) g_fight = std::max(0, atoi(argv[++i]));
    else if (!strcmp(argv[i], "--fevery") && i + 1 < argc) g_fevery = std::max(1, atoi(argv[++i]));
    else if (!strcmp(argv[i], "--check") && i + 1 < argc) checkFile = argv[++i];
    else if (!strcmp(argv[i], "--dump") && i + 1 < argc) g_dumpDir = argv[++i];
  }
  g_quiet = true;   // the game's own out() lines (FAIL only) stay out of the record
  std::vector<uint64_t> seeds;
  // the record: its header sets secs / every, its C lines the seeds (an excluded seed is simply absent)
  std::map<std::pair<uint64_t, int>, std::vector<std::string>> base;
  if (checkFile) {
    std::ifstream f(checkFile);
    if (!f) { printf("sim-digest FAIL: cannot read %s\n", checkFile); return 2; }
    std::string line;
    std::set<uint64_t> have;
    g_fight = 0;   // a v1 record (no skirmish) has no "fight" in its header
    while (std::getline(f, line)) {
      if (!line.empty() && line.back() == '\r') line.pop_back();
      auto w = words(line);
      if (w.size() >= 2 && w[0] == "#") {
        for (size_t i = 1; i + 1 < w.size(); i++) {
          if (w[i] == "secs") secs = (float)atof(w[i + 1].c_str());
          if (w[i] == "every") every = atoi(w[i + 1].c_str());
          if (w[i] == "fight") g_fight = atoi(w[i + 1].c_str());
          if (w[i] == "fevery") g_fevery = atoi(w[i + 1].c_str());
        }
        continue;
      }
      if (w.size() < 3 || (w[0] != "C" && w[0] != "S" && w[0] != "A")) continue;
      uint64_t s = strtoull(w[1].c_str(), nullptr, 10);
      auto& v = base[{s, atoi(w[2].c_str())}];
      v.resize(3);
      v[w[0] == "C" ? 0 : w[0] == "S" ? 1 : 2] = line;
      if (w[0] == "C" && !have.count(s)) { have.insert(s); if (!seedsGiven) seeds.push_back(s); }
    }
    if (base.empty()) { printf("sim-digest FAIL: no checkpoints in %s\n", checkFile); return 2; }
  }
  if (seeds.empty()) for (uint64_t s = A; s <= B; s++) seeds.push_back(s);
  if (!checkFile) printf("# sim-digest v2 secs %g every %d fight %d fevery %d seeds %llu..%llu\n", secs, every, g_fight, g_fevery, (unsigned long long)seeds.front(), (unsigned long long)seeds.back());
  int checked = 0, same = 0, badSeeds = 0;
  Fnv all;
  for (uint64_t s : seeds) {
    std::vector<std::string> lines;
    runDigest(s, secs, every, lines);
    bool seedBad = false;
    for (size_t i = 0; i + 2 < lines.size(); i += 3) {
      all.bytes(lines[i].data(), lines[i].size());
      if (!checkFile) { printf("%s\n%s\n%s\n", lines[i].c_str(), lines[i + 1].c_str(), lines[i + 2].c_str()); continue; }
      auto cw = words(lines[i]);
      auto it = base.find({s, atoi(cw[2].c_str())});
      checked++;
      if (it == base.end() || it->second[0].empty()) {
        if (!seedBad) printf("sim-digest FAIL seed %llu t %s: checkpoint missing from the record\n", (unsigned long long)s, cw[2].c_str());
        seedBad = true;
        continue;
      }
      if (it->second[0] == lines[i] && it->second[1] == lines[i + 1] && it->second[2] == lines[i + 2]) { same++; continue; }
      if (!seedBad)
        printf("sim-digest FAIL seed %llu t %s s (first differing checkpoint): %s\n", (unsigned long long)s, cw[2].c_str(),
               explain(it->second, {lines[i], lines[i + 1], lines[i + 2]}).c_str());
      seedBad = true;
    }
    if (seedBad) badSeeds++;
    fflush(stdout);
  }
  if (!checkFile) {
    printf("# sim-digest: %zu seeds, digest %016llx\n", seeds.size(), (unsigned long long)all.h);
    return 0;
  }
  size_t want = 0;
  for (const auto& kv : base) if (std::find(seeds.begin(), seeds.end(), kv.first.first) != seeds.end()) want++;
  printf("sim-digest %s: %d/%zu checkpoints identical, %d of %zu seeds differ, digest %016llx\n",
         same == (int)want && checked == (int)want ? "PASS" : "FAIL", same, want, badSeeds, seeds.size(), (unsigned long long)all.h);
  return same == (int)want && checked == (int)want ? 0 : 1;
}

}  // namespace

RPG_TEST_CMD("--sim-digest", "the sim identity probe: seeds x checkpoints of serialize/actors/rng/marks [--seeds A..B] [--secs 120] [--every 10] [--fight 30] [--fevery 2] [--check FILE] [--dump DIR]", simDigest);
