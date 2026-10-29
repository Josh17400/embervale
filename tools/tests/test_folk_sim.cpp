// rpg_test --folk-sim (P2b FOLK-SIM, VISION_PLAN 15.36): the census's doll look fields, the census ages, the role
// garments, the gait switch at 54 px/s, the walk / run table and its re-phasing, the sheathed / drawn pick. One PASS /
// FAIL line per check. Options: --seeds A..B (the census seeds, default 1..20), --radius R (tiles around the start
// settlement, default 160: the variety harness's), --pool F (a pool file for the table checks), --report (counts).
#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <vector>
#include "rpg/art.h"
#include "rpg/art/doll/doll.h"
#include "rpg/culture/culture.h"
#include "rpg/sim/anim.h"
#include "rpg/sim/game.h"
#include "rpg/sim/game_internal.h"
#include "rpg/sim/gear.h"
#include "rpg/sim/gear_ladder.h"
#include "rpg/sim/gear_look.h"
#include "rpg/sim/loot_ops.h"
#include "rpg/world/source.h"
#include "rpg/sim/life.h"
#include "tools/tests/tests.h"

namespace census {
void ageLook(art::HumanLook& L, int years, bool childRole, bool elder, uint64_t seed);
}  // namespace census

namespace {

namespace doll = art::doll;
int g_fails = 0, g_checks = 0;
bool g_rep = false;

void check(bool ok, const char* id, const std::string& what) {
  g_checks++;
  if (!ok) g_fails++;
  std::printf("%s %-28s %s\n", ok ? "PASS" : "FAIL", id, what.c_str());
}
std::string fmt(const char* f, ...) {
  char b[512];
  va_list ap;
  va_start(ap, f);
  std::vsnprintf(b, sizeof b, f, ap);
  va_end(ap);
  return b;
}

// ---------------------------------------------------------------- the census (life_game.cpp dressResident's rules)
struct Folk { art::HumanLook look; Role role; life::Job job; int years; bool civil; std::string site; };
bool civil(const art::HumanLook& L) {   // (test_doll.cpp's civil(): no armour, helm, shield or culture arms)
  const bool armour = L.armorStyle || L.bodyForm || L.outfit == art::Outfit::Leather || L.outfit == art::Outfit::Chain || L.outfit == art::Outfit::Plate ||
                      L.outfit == art::Outfit::Elven || L.outfit == art::Outfit::Ebony || L.outfit == art::Outfit::Guard;
  if (armour || L.helmet || L.helmStyle || L.helmForm || L.shield || L.shieldStyle || L.shieldForm) return false;
  if (L.bladeForm || L.polearmForm > 1 || L.bowForm || L.pauldron || L.skirt || L.crest || L.gloves || L.boots) return false;
  return true;
}
void censusOf(uint64_t seed, int radius, std::vector<Folk>& out) {
  Game g(seed);
  g.newEndlessGame(seed);
  const Site& home = g.world.sites[(size_t)g.world.startSite];
  for (size_t si = 0; si < g.world.sites.size(); si++) {
    const Site& s = g.world.sites[si];
    if (s.type != SiteType::Village && s.type != SiteType::Town && s.type != SiteType::City) continue;
    if (std::abs(s.r.cx() - home.r.cx()) > radius || std::abs(s.r.cy() - home.r.cy()) > radius) continue;
    const life::Census* cp = g.life.census(g.world, (int)si);   // (built on first use: dressAsResident finds it)
    if (!cp) { std::printf("note: %s has no census yet (buildings not streamed)\n", s.name.c_str()); continue; }
    const life::Census c = *cp;
    if (g_rep) {
      const cult::Culture* C = g.world.cultureOf((int)si);
      std::printf("  seed %llu %s: %s, %zu folk, cut man %d woman %d\n", (unsigned long long)seed, s.name.c_str(), C ? cult::archetypeName(C->archetype) : "-",
                  c.res.size(), C ? (int)C->dress.cutM : -1, C ? (int)C->dress.cutF : -1);
    }
    for (const life::Resident& r : c.res) {
      if (r.job == life::Job::Guard) continue;
      Actor a;
      a.human = true; a.site = (int)si; a.role = life::jobRole(r.job);
      a.slot = 3000 + (int)r.idx; a.bldg = -1;   // (spawnResident's identity: LIFE_SLOT0 + idx)
      if (a.role == Role::Guard) a.role = Role::Villager;
      const Role role = a.role;
      g.dressAsResident(a, s.id, (int)r.idx);   // the real street look: makeLook + dressResident (ages and garb included)
      out.push_back({a.look, role, r.job, (int)r.age, civil(a.look), s.name});
    }
  }
}

// ---------------------------------------------------------------- the hero on the move
Vec2 g_p0;   // the hero's start (each walk begins there: open ground east of it)
void stage(Game& g, uint64_t seed) {
  g.newEndlessGame(seed);
  g.mode = Mode::Play;
  g.noWildSpawns = true;
  g.godMode = true;
  g.hour = 12;
  for (int f = 0; f < 60; f++) { g.update(SIM_DT, Input()); g.events.clear(); g.mode = Mode::Play; g.actors.resize(1); }
  g.actors.resize(1);
  // open ground: the nearest spot from which 80 px east is free for the hero's body
  const Vec2 s = g.pl().p;
  g_p0 = s;
  for (int r = 0; r <= 30; r++)
    for (int dy = -r; dy <= r; dy++)
      for (int dx = -r; dx <= r; dx++) {
        if (std::max(std::abs(dx), std::abs(dy)) != r) continue;
        const Vec2 c = s + Vec2((float)dx * 16.0f, (float)dy * 16.0f);
        bool free = true;
        for (int k = 0; k <= 80 && free; k += 4) free = ClimbOps::bodyFree(g, c + Vec2((float)k, 0), g.pl().radius);
        if (free) { g_p0 = c; g.pl().p = c; return; }
      }
}
void tick(Game& g, const Input& in) { g.update(SIM_DT, in); g.events.clear(); g.mode = Mode::Play; }
void arm(Game& g, WeaponType t) {
  Rng r(91u + (uint32_t)t);
  g.inv.clear();
  g.eqWeapon = g.eqBow = g.eqStaff = g.eqArmor = g.eqHelmet = g.eqShield = g.eqRing = g.eqAmulet = g.eqGloves = g.eqBoots = g.eqCloak = -1;
  g.inv.push_back(gear::makeGear(r, ItemKind::Weapon, (int)t, 5, Rarity::Common, 0, Mat::Iron));
  g.eqWeapon = 0;
  AnimOps::refreshHero(g);
}
// the hero walks east with the stick at `stick` for `secs`; returns the measured px/s over the last half and the gait
struct Run { float pxs = 0; int gait = 0; int part = 0; float moveV = 0; };
Run walkEast(Game& g, float stick, float secs) {
  g.pl().p = g_p0;
  Input in;
  in.move = Vec2(stick, 0);
  const int n = (int)std::lround(secs / SIM_DT);
  float x0 = 0;
  for (int f = 0; f < n; f++) {
    if (f == n / 2) x0 = g.pl().p.x;
    tick(g, in);
  }
  Run r;
  r.pxs = (g.pl().p.x - x0) / (SIM_DT * (float)(n - n / 2));
  const AnimPick k = animPick(g, g.pl(), 2);
  r.gait = k.gait; r.part = (int)k.part; r.moveV = g.pl().moveV;
  return r;
}
void standStill(Game& g, float secs) { for (int f = 0; f < (int)std::lround(secs / SIM_DT); f++) tick(g, Input()); }

int folkSimCmd(int argc, char** argv) {
  uint64_t sa = 1, sb = 20;
  int radius = 160;
  const char* poolFile = nullptr;
  for (int i = 1; i < argc; i++) {
    if (!std::strcmp(argv[i], "--seeds") && i + 1 < argc) parseSeedRange(argv[++i], sa, sb);
    else if (!std::strcmp(argv[i], "--radius") && i + 1 < argc) radius = std::atoi(argv[++i]);
    else if (!std::strcmp(argv[i], "--pool") && i + 1 < argc) poolFile = argv[++i];
    else if (!std::strcmp(argv[i], "--report")) g_rep = true;
  }
  const int was = art::rigVersion();

  // ---- F1-F4: the census
  {
    std::vector<Folk> folk;
    for (uint64_t s = sa; s <= sb; s++) censusOf(s, radius, folk);
    std::map<std::string, int> garb[2];
    std::set<int> styles, beards, heads[2], youthHeads, childHeads, elfHeads;
    int youths = 0, children = 0, elders = 0, adults = 0, elderLined = 0, kidMarks = 0, kidBeards = 0, ageWrong = 0, rig1Moved = 0;
    std::map<int, int> neck, garbN;
    for (const Folk& f : folk) {
      const art::HumanLook& L = f.look;
      // the census ages: child under 10, youth 10-14, elder from 60 (or the elder's job)
      const int want = f.job == life::Job::Child ? (f.years >= 10 && f.years <= 14 ? 2 : 1) : (f.job == life::Job::Elder || f.years >= 60) ? 3 : 0;
      if (L.age != want) ageWrong++;
      if (L.age == 2) { youths++; youthHeads.insert(L.head); }
      if (L.age == 1) { children++; childHeads.insert(L.head); }
      if (L.age == 3) { elders++; elderLined += (L.marks & 16) != 0; }
      if (L.age == 0) adults++;
      if ((L.age == 1 || L.age == 2) && L.marks) kidMarks++;
      if ((L.age == 1 || L.age == 2) && L.beard) kidBeards++;
      if (L.people == 2 && L.age != 1) elfHeads.insert(L.head);
      // rig 1 never sees the doll's fields: the key with them equals the key with them cleared
      art::setRigVersion(1);
      art::HumanLook z = L;
      z.head = z.nose = z.brow = z.marks = z.hairVar = z.beardStyle = z.neck = z.garb = 0;
      if (z.age == 2 || z.age == 3) z.age = 0;
      if (L.key() != z.key()) rig1Moved++;
      art::setRigVersion(2);
      neck[L.neck]++;
      garbN[L.garb]++;
      if (!f.civil) continue;
      if (L.age != 1) styles.insert(doll::hairStyleOf(L.hair, L.hairVar));
      if (L.beard && L.body == 0 && L.age != 1 && L.age != 2) beards.insert(L.beardStyle);
      if (L.age != 1) heads[L.body ? 1 : 0].insert(L.head);
      const doll::DollRecipe r = doll::resolve(L);
      for (const doll::Need& n : r.needs) if (n.role == doll::Role::Garment && L.age != 1 && L.age != 2) garb[L.body ? 1 : 0][n.value]++;
    }
    art::setRigVersion(was);
    std::printf("folk-sim census (seeds %llu..%llu, radius %d): %zu folk; adults %d, youths %d, children %d, elders %d\n", (unsigned long long)sa,
                (unsigned long long)sb, radius, folk.size(), adults, youths, children, elders);
    for (int b = 0; b < 2; b++) {
      std::printf("  garments %s:", b ? "woman" : "man");
      for (const auto& kv : garb[b]) std::printf(" %s %d", kv.first.c_str(), kv.second);
      std::printf("\n");
    }
    std::printf("  neck: none %d scarf %d kerchief %d collar %d; garb (0-13):", neck[0], neck[1], neck[2], neck[3]);
    for (int i = 0; i <= 13; i++) std::printf(" %d", garbN[i]);
    std::printf("\n");
    check(ageWrong == 0, "F1 census ages", fmt("%d of %zu folk with the wrong age (child <10, youth 10-14, elder 60+)", ageWrong, folk.size()));
    check(youths > 0 && elders > 0 && children > 0, "F1 ages present", fmt("youths %d, children %d, elders %d", youths, children, elders));
    bool soft = true;
    for (int h : youthHeads) soft = soft && (h == 0 || h == 2 || h == 3 || h == 6);
    bool kidH = true;
    for (int h : childHeads) kidH = kidH && h <= 3;
    bool elfH = true;
    for (int h : elfHeads) elfH = elfH && h >= 6 && h <= 8;
    check(soft, "F2 youth heads soft four", fmt("%zu youth head types seen, all oval / round / long / fine: %s", youthHeads.size(), soft ? "yes" : "no"));
    check(kidH, "F2 child heads", fmt("%zu child head types, all in the child four: %s", childHeads.size(), kidH ? "yes" : "no"));
    check(elfH && !elfHeads.empty(), "F2 elven heads", fmt("%zu elven adult head types, all elven (6-8): %s", elfHeads.size(), elfH ? "yes" : "no"));
    check(elders == elderLined, "F2 elders lined", fmt("%d of %d elders carry the lined mark", elderLined, elders));
    check(kidMarks == 0 && kidBeards == 0, "F2 kids clean", fmt("%d kids with marks, %d with a beard", kidMarks, kidBeards));
    check(rig1Moved == 0, "F3 rig 1 keys untouched", fmt("%d looks whose rig 1 key sees the doll fields", rig1Moved));
    check(styles.size() >= 18, "F4 hair styles", fmt("%zu of 24 (18)", styles.size()));
    check(beards.size() >= 9, "F4 beards", fmt("%zu of 10 values (9)", beards.size()));
    check(heads[0].size() == 9 && heads[1].size() == 9, "F4 head types", fmt("man %zu, woman %zu of 9", heads[0].size(), heads[1].size()));
    const bool work = garb[0].count("apron") && garb[1].count("apron") && garb[0].count("smock") && garb[1].count("smock") && garb[0].count("jerkin") &&
                      garb[1].count("jerkin");
    check(work, "F4 work garments", "apron, smock and jerkin each seen on civil men and women");
  }

  // ---- F5: determinism (the same resident twice: the same look, field for field)
  {
    std::vector<Folk> a, b;
    censusOf(sa, 60, a);
    censusOf(sa, 60, b);
    int diff = a.size() != b.size() ? 1 : 0;
    for (size_t i = 0; i < a.size() && i < b.size(); i++) {
      const art::HumanLook &x = a[i].look, &y = b[i].look;
      diff += x.head != y.head || x.nose != y.nose || x.brow != y.brow || x.marks != y.marks || x.hairVar != y.hairVar || x.beardStyle != y.beardStyle ||
              x.neck != y.neck || x.garb != y.garb || x.age != y.age;
    }
    check(diff == 0, "F5 deterministic", fmt("%d of %zu looks differ between two builds of one census", diff, a.size()));
  }

  // ---- F6-F7: the gait switch at 54 px/s (the hero: 74 px/s at full stick)
  art::setRigVersion(2);
  {
    Game g(sa);
    stage(g, sa);
    const float full = g.pl().speed;
    Run r0 = walkEast(g, 1.0f, 0.8f);
    check(r0.pxs > 54 && r0.gait == 1 && r0.part == (int)art::RigPart::Run, "F6 full stick runs",
          fmt("%.1f px/s (speed %.0f), gait %d, part %d (Run %d), moveV %.1f", r0.pxs, full, r0.gait, r0.part, (int)art::RigPart::Run, r0.moveV));
    standStill(g, 0.5f);
    const float sWalk = 50.0f / full, sRun = 58.0f / full;
    Run r1 = walkEast(g, sWalk, 0.8f);
    check(r1.gait == 0 && r1.part == (int)art::RigPart::Loco, "F6 50 px/s walks", fmt("%.1f px/s, gait %d, part %d", r1.pxs, r1.gait, r1.part));
    standStill(g, 0.5f);
    Run r2 = walkEast(g, sRun, 0.8f);
    check(r2.gait == 1, "F6 58 px/s runs", fmt("%.1f px/s, gait %d", r2.pxs, r2.gait));
    // stepping down through the switch: back to the walk once under 52
    Run r3 = walkEast(g, 51.0f / full, 0.8f);
    check(r3.gait == 0, "F6 run -> walk under 52", fmt("%.1f px/s, gait %d", r3.pxs, r3.gait));
    // the start: (SLICE-RUN-INGAME) a body that sets off at a run starts on the run (animSetOff), never a few walk
    // frames and a mid-stride switch
    standStill(g, 0.6f);
    g.pl().p = g_p0;
    standStill(g, 0.1f);
    Input in; in.move = Vec2(1, 0);
    int firstRun = -1;
    for (int f = 0; f < 40; f++) { tick(g, in); if (firstRun < 0 && g.pl().gait == 1) firstRun = f; }
    check(firstRun == 0, "F6 run starts", fmt("the run starts on step %d (0) of a full-stick start", firstRun));
    standStill(g, 0.5f);
    // elders and children never run
    g.pl().look.age = 3;
    Run re = walkEast(g, 1.0f, 0.6f);
    check(re.gait == 0, "F7 elder walks", fmt("an elder at %.1f px/s: gait %d", re.pxs, re.gait));
    g.pl().look.age = 1;
    Run rc = walkEast(g, 1.0f, 0.6f);
    check(rc.gait == 0, "F7 child walks", fmt("a child at %.1f px/s: gait %d", rc.pxs, rc.gait));
    g.pl().look.age = 0;
    // the frame clock: frames advance by distance, and the clip's frame count is the table's (or the 6 of kWalkAt)
    AnimPick k = animPick(g, g.pl(), 2);
    check(g.pl().st != AState::Walk || k.frames >= 1, "F7 walk frames", fmt("%d frames", k.frames));
  }

  // ---- F8: the walk / run tables and the re-phase on a switch (a pool with clips: --pool, else the embedded one)
  {
    if (poolFile) {
      std::string err;
      if (!doll::loadPoolFile(poolFile, &err)) std::printf("FAIL F8 pool %s: %s\n", poolFile, err.c_str());
    }
    Actor a;
    a.human = true;
    a.speed = 74;
    const WalkTableRef w = animWalkTable(a, 0), r = animWalkTable(a, 1);
    if (w.frames > 0) {
      bool mono = true;
      for (int i = 1; i < w.frames; i++) mono = mono && w.start[i] > w.start[i - 1];
      check(mono && w.cycle > 0, "F8 walk table", fmt("%d frames over %.1f px", w.frames, w.cycle));
      // re-phase: walking at frame i, switching to the run lands on frame i of the run (both 6 frames)
      if (r.frames == w.frames) {
        Game g(1);
        int bad = 0;
        for (int px = 0; px < (int)(w.cycle * 2); px++) {
          a.animT = (float)px / a.speed; a.gait = 0; a.st = AState::Walk; a.moveV = 70;
          const int fw = w.frameAt((float)px);
          animStep(g, a, 74 * SIM_DT, SIM_DT);
          const int fr = r.frameAt(std::floor(a.animT * a.speed + 0.5f));
          if (a.gait != 1 || std::abs(fr - fw) > 1) bad++;
        }
        check(bad == 0, "F8 re-phase walk->run", fmt("%d of %d px whose run frame is not the walk's (+-1)", bad, (int)(w.cycle * 2)));
      } else std::printf("note F8: no run clip with the walk's frames in this pool (walk %d, run %d): re-phase skipped\n", w.frames, r.frames);
    } else std::printf("note F8: the pool has no walk clip (frames 0): the pick keeps kWalkAt (28 px)\n");
    if (poolFile) doll::useEmbeddedPool();
  }

  // ---- F9-F10: the sheathed / drawn pick
  {
    Game g(sa);
    stage(g, sa);
    arm(g, WeaponType::Sword);
    standStill(g, 0.3f);
    AnimPick k = animPick(g, g.pl(), 2);
    check(g.pl().look.weapon == 1 && !k.drawn && k.variant == 0 && k.part == art::RigPart::Loco && k.clip == art::Clip::Idle, "F9 idle sheathed",
          fmt("weapon %d, clip %s variant %d part %d drawn %d", g.pl().look.weapon, art::clipName(k.clip), k.variant, (int)k.part, (int)k.drawn));
    Input in; in.attack = true;
    tick(g, in);
    int endF = -1;
    for (int f = 0; f < 120; f++) {
      tick(g, Input());
      const AState s = g.pl().st;
      if (s != AState::Windup && s != AState::Strike && s != AState::Recover) { endF = f; break; }
    }
    standStill(g, 1.0f);
    k = animPick(g, g.pl(), 2);
    check(endF >= 0 && k.drawn && k.variant == 1 && k.part == art::RigPart::Drawn && k.clip == art::Clip::Idle, "F9 drawn after a swing",
          fmt("1 s after: clip %s variant %d part %d drawn %d", art::clipName(k.clip), k.variant, (int)k.part, (int)k.drawn));
    standStill(g, 2.85f);   // 3.85 s after the swing: still drawn
    const bool still = animPick(g, g.pl(), 2).drawn;
    standStill(g, 0.3f);    // 4.15 s: sheathed
    k = animPick(g, g.pl(), 2);
    check(still && !k.drawn && k.variant == 0 && k.part == art::RigPart::Loco, "F9 sheathed after 4 s",
          fmt("3.85 s drawn %d; 4.15 s drawn %d variant %d", (int)still, (int)k.drawn, k.variant));
    // walking drawn: the DRAWN part's walk variant 1; running drawn: its walk variant 2 (P2b fix r1, B10; P2c: RigPart::Drawn)
    tick(g, in);
    for (int f = 0; f < 40; f++) tick(g, Input());
    Run rr = walkEast(g, 1.0f, 0.5f);
    k = animPick(g, g.pl(), 2);
    check(k.drawn && k.gait == 1 && k.part == art::RigPart::Drawn && k.variant == 2, "F9 drawn run", fmt("gait %d part %d variant %d", rr.gait, (int)k.part, k.variant));
    // a hostile within the hero's aggro radius draws the sword; gone, it is sheathed kThreatHold later
    standStill(g, 4.5f);
    const int bi = AnimOps::spawnBandit(g, g.pl().p + Vec2(60, 0), -1);
    tick(g, Input());
    const bool threat = animPick(g, g.pl(), 2).drawn;
    if (bi >= 0) g.actors.resize(1);
    standStill(g, kThreatHold + 0.2f);
    const bool calm = !animPick(g, g.pl(), 2).drawn;
    check(bi >= 0 && threat && calm, "F10 drawn near a hostile", fmt("bandit at 60 px: drawn %d; %.1f s after it is gone: sheathed %d", (int)threat, kThreatHold + 0.2f, (int)calm));
    // unarmed: never drawn
    g.eqWeapon = -1; g.inv.clear();
    AnimOps::refreshHero(g);
    tick(g, in);
    for (int f = 0; f < 30; f++) tick(g, Input());
    k = animPick(g, g.pl(), 2);
    check(!k.drawn && k.part != art::RigPart::Action, "F10 unarmed", fmt("drawn %d part %d", (int)k.drawn, (int)k.part));
  }

  // ---- F11: B7's garments as the variety harness sees them (test_doll.cpp censusA: makeLook per resident of the start
  //      region's settlements, a beggar in rags, the census ages; every civil resident of any age): man 13, woman 14
  {
    art::setRigVersion(2);
    std::map<std::string, int> seen[2];
    for (uint64_t s = sa; s <= sb; s++) {
      Game g(s);
      g.newEndlessGame(s);
      const Site& home = g.world.sites[(size_t)g.world.startSite];
      for (size_t si = 0; si < g.world.sites.size(); si++) {
        const Site& st = g.world.sites[si];
        if (st.type != SiteType::Village && st.type != SiteType::Town && st.type != SiteType::City) continue;
        if (std::abs(st.r.cx() - home.r.cx()) > 160 || std::abs(st.r.cy() - home.r.cy()) > 160) continue;
        life::Census c;
        life::buildCensus(g.world, (int)si, c);
        for (const life::Resident& r : c.res) {
          Actor a;
          a.human = true; a.site = (int)si; a.role = life::jobRole(r.job);
          for (uint32_t k = 0; k < 12; k++) {
            Rng rr(r.lookSeed + k * 7919u);
            Rng probe = rr;
            const bool fem = probe.f() < 0.45f;
            if (fem != r.female && k + 1 < 12) continue;
            HomeOps::makeLook(g, a, a.role, rr);
            break;
          }
          if (r.job == life::Job::Beggar) a.look.outfit = art::Outfit::Rags;
          if (!civil(a.look)) continue;
          for (const doll::Need& n : doll::resolve(a.look).needs)
            if (n.role == doll::Role::Garment) seen[a.look.body ? 1 : 0][n.value]++;
        }
      }
    }
    for (int b = 0; b < 2; b++) {
      std::printf("  B7 garments %s:", b ? "woman" : "man");
      for (const auto& kv : seen[b]) std::printf(" %s %d", kv.first.c_str(), kv.second);
      std::printf("\n");
    }
    check(seen[0].size() >= 13 && seen[1].size() >= 14, "F11 B7 garments", fmt("man %zu (13), woman %zu (14)", seen[0].size(), seen[1].size()));
  }

  // ---- F12: a created hero reaches every hair style and every beard (the hash of the Appearance)
  {
    std::set<int> styles, beards;
    const cult::Culture* home = nullptr;
    for (int i = 0; i < 4000; i++) {
      Appearance app;
      app.created = true;
      app.name = fmt("HERO%d", i);
      app.female = (i & 1) != 0;
      app.hair = (uint8_t)(i / 2 % (int)art::Hair::COUNT);
      app.beard = !app.female;
      app.people = (uint8_t)(i / 16 % 3);
      const art::HumanLook L = appearanceLook(app, home);
      styles.insert(doll::hairStyleOf(L.hair, L.hairVar));
      if (L.beard) beards.insert(L.beardStyle);
    }
    check(styles.size() == 24 && beards.size() >= 9, "F12 hero styles", fmt("%zu of 24 hair styles, %zu of 9 beards", styles.size(), beards.size()));
  }

  // ---- F13-F17 (P2c FOLK-SIM-C, production): the census on the ladder through Game::makeLook as the spawners call it
  //      F13 the rank's tier on every slot a role wears (body, helm, shield, cloak): PLAN "People"
  //      F14 the regional lift: soldiers, bandits and the watch a tier finer where the danger is band 4 (level 25+);
  //          the royal guard, captains, kings untouched
  //      F15 the herald: the padded coat (forge style) under the tabard, no helm, the cape at T3
  //      F16 D29: captains, the royal guard and jarls (T4) in the realm's alloy below its masterwork, kings (T5) in it
  //      F17 C11 fighters' faces: no head recipe (type, nose, brows, beard, hair colour) on more than 2 guards of a
  //          settlement or 2 soldiers of a squad
  {
    using H = art::HumanLook;
    int bad13 = 0, n13 = 0, bad14 = 0, n14 = 0, bad15 = 0, n15 = 0, bad16 = 0, n16 = 0;
    std::string first13, first14, first15, first16;
    int c11Worst = 0, c11Groups = 0, c11Guards = 0, c11Soldiers = 0;
    std::string c11First;
    for (uint64_t seed = sa; seed <= sb; seed++) {
      Game g(seed);
      g.newEndlessGame(seed);
      World& w = g.world;
      // a capital with its palace, and a village, of kingdoms that have them
      int cap = -1, vil = -1, palace = -1;
      for (size_t si = 0; si < w.sites.size(); si++) {
        const Site& s = w.sites[si];
        if (s.kingdom < 0 || !w.cultureOf((int)si)) continue;
        if (s.capital && cap < 0) cap = (int)si;
        if (s.type == SiteType::Village && vil < 0) vil = (int)si;
      }
      if (cap >= 0) {
        w.ensureSiteRecords(cap);
        for (size_t b = 0; b < w.over.bldgs.size(); b++)
          if (w.over.bldgs[b].site == cap && (w.over.bldgs[b].type == art::Building::Palace || w.over.bldgs[b].type == art::Building::Barracks)) { palace = (int)b; break; }
      }
      if (cap < 0 || vil < 0) { std::printf("note: seed %llu has no capital or village with a kingdom\n", (unsigned long long)seed); continue; }
      const ew::Gid kid = w.kingdoms[(size_t)w.sites[(size_t)cap].kingdom].id;
      struct Who { Role r; int site; int bldg; bool realm; int want; const char* name; };
      const Who who[] = {{Role::Bandit, vil, -1, false, 1, "bandit"},       {Role::Guard, vil, -1, false, 2, "village watch"},
                         {Role::Guard, cap, -1, false, 3, "capital watch"}, {Role::Guard, cap, palace, false, 4, "royal guard"},
                         {Role::Soldier, cap, -1, true, 2, "soldier"},      {Role::Captain, cap, -1, true, 4, "captain"},
                         {Role::Herald, cap, -1, true, 3, "herald"},        {Role::Jarl, vil, -1, false, 4, "jarl"},
                         {Role::King, cap, -1, false, 5, "king"},           {Role::Warlord, cap, -1, true, 3, "warlord"}};
      auto make = [&](const Who& x, int k) {
        Actor a;
        a.id = 90000 + k; a.human = true; a.npc = true; a.role = x.r;
        a.site = x.site; a.bldg = x.bldg; a.slot = 5000 + k; a.realm = x.realm ? kid : 0;
        Rng rr(hash32((uint32_t)a.slot * 2654435761u ^ (uint32_t)seed));
        AnimOps::makeLook(g, a, x.r, rr);
        return a.look;
      };
      std::map<std::string, H> officer;
      for (const Who& x : who) {
        if (x.bldg == -1 && x.r == Role::Guard && x.want == 4) continue;
        if (x.want == 4 && x.r == Role::Guard && palace < 0) continue;
        for (int k = 0; k < 8; k++) {
          const H L = make(x, k);
          n13++;
          const bool armour = L.tier[H::TIER_BODY] > 0;
          std::string why;
          if (!armour) why = "no body tier";
          else if (L.tier[H::TIER_BODY] != x.want) why = fmt("body T%d", L.tier[H::TIER_BODY]);
          else if ((L.helmet || L.helmStyle || L.helmForm) && L.tier[H::TIER_HELM] != x.want) why = fmt("helm T%d", L.tier[H::TIER_HELM]);
          else if (L.shieldForm != 9 && (L.shield || L.shieldStyle || L.shieldForm) && L.tier[H::TIER_SHIELD] != x.want) why = fmt("shield T%d", L.tier[H::TIER_SHIELD]);
          else if ((L.cloak || L.cape) && L.tier[H::TIER_CLOAK] != x.want) why = fmt("cloak T%d", L.tier[H::TIER_CLOAK]);
          if (!why.empty()) { bad13++; if (first13.empty()) first13 = fmt("%s (T%d): %s", x.name, x.want, why.c_str()); }
          if (k == 0) officer[x.name] = L;
          if (x.r == Role::Herald) {
            n15++;
            const bool ok = L.forge[H::TIER_BODY] != 0 && !L.helmet && !L.helmStyle && !L.helmForm && L.cape && L.tier[H::TIER_CLOAK] == 3 &&
                            L.outfit == art::Outfit::Guard;
            if (!ok) { bad15++; if (first15.empty()) first15 = fmt("forge %llx helm %d/%d/%d cape %d cloak T%d", (unsigned long long)L.forge[H::TIER_BODY], (int)L.helmet, L.helmStyle, L.helmForm, (int)L.cape, L.tier[H::TIER_CLOAK]); }
          }
        }
      }
      // D29: the officers' alloy and the king's masterwork
      {
        const H* kg = officer.count("king") ? &officer["king"] : nullptr;
        for (const char* o : {"captain", "royal guard", "jarl"}) {
          if (!officer.count(o)) continue;
          const H& L = officer[o];
          n16++;
          const bool ok = L.metalKind == 10 && L.alloyRank >= 1 && L.forge[H::TIER_BODY] && kg && kg->metalKind == 10 && kg->alloyRank >= L.alloyRank;
          if (!ok) { bad16++; if (first16.empty()) first16 = fmt("%s metal %d rank %d; king metal %d rank %d", o, L.metalKind, L.alloyRank, kg ? kg->metalKind : -1, kg ? kg->alloyRank : -1); }
        }
      }
      // the lift: the same men where the danger is level 30
      {
        const int lv0 = w.sites[(size_t)cap].level, lv1 = w.sites[(size_t)vil].level;
        w.sites[(size_t)cap].level = 30; w.sites[(size_t)vil].level = 30;
        for (const Who& x : who) {
          if (x.want == 4 && x.r == Role::Guard && palace < 0) continue;
          const bool lifts = x.r == Role::Soldier || x.r == Role::Bandit || (x.r == Role::Guard && x.want < 4);
          const H L = make(x, 0);
          n14++;
          const int want = lifts ? x.want + 1 : x.want;
          if (L.tier[H::TIER_BODY] != want) { bad14++; if (first14.empty()) first14 = fmt("%s at level 30: body T%d (T%d)", x.name, L.tier[H::TIER_BODY], want); }
        }
        w.sites[(size_t)cap].level = lv0; w.sites[(size_t)vil].level = lv1;
      }
      // C11: guards of each settlement in the start region, and squads of 8 soldiers of each kingdom
      auto recipe = [](const H& L) {
        return fmt("%d.%d.%d.%d.%08x", L.head, L.nose, L.brow, L.beard ? L.beardStyle : 99, L.hairColor);
      };
      auto group = [&](const std::vector<H>& men, const std::string& where) {
        std::map<std::string, int> n;
        for (const H& L : men) n[recipe(L)]++;
        for (const auto& kv : n) {
          c11Groups++;
          if (kv.second > c11Worst) c11Worst = kv.second;
          if (kv.second > 2 && c11First.empty()) c11First = fmt("%s: %d men share %s", where.c_str(), kv.second, kv.first.c_str());
        }
      };
      const Site& home = w.sites[(size_t)w.startSite];
      for (size_t si = 0; si < w.sites.size(); si++) {
        const Site& s = w.sites[si];
        if (s.type != SiteType::Village && s.type != SiteType::Town && s.type != SiteType::City) continue;
        if (std::abs(s.r.cx() - home.r.cx()) > radius || std::abs(s.r.cy() - home.r.cy()) > radius) continue;
        // the watch as spawnHuman makes it (a map spawn's slot, its key and Rng; the owner's realm): 12 men a settlement
        std::vector<H> guards;
        for (int k = 0; k < 12; k++) {
          Actor a;
          a.id = 96000 + k; a.human = true; a.npc = true; a.role = Role::Guard; a.site = (int)si; a.slot = 40 + k; a.bldg = -1;
          if (s.kingdom >= 0) a.realm = w.kingdoms[(size_t)s.kingdom].id;
          const uint64_t key = ((uint64_t)(a.site + 1) << 24) ^ ((uint64_t)(a.bldg + 1) << 12) ^ (uint64_t)a.slot;
          Rng rr(hash32((uint32_t)key) ^ (uint32_t)seed);
          AnimOps::makeLook(g, a, Role::Guard, rr);
          guards.push_back(a.look);
        }
        c11Guards += (int)guards.size();
        group(guards, "guards of " + s.name);
      }
      for (size_t ki = 0; ki < w.kingdoms.size() && ki < 8; ki++) {
        std::vector<H> squad;
        for (int k = 0; k < 8; k++) {
          Actor a;
          a.id = 95000 + k; a.human = true; a.npc = true; a.role = Role::Soldier; a.site = -1; a.bldg = -1; a.slot = 7000 + (int)ki * 16 + k;
          a.realm = w.kingdoms[ki].id;
          Rng rr(hash32((uint32_t)a.slot * 2654435761u ^ (uint32_t)a.realm ^ (uint32_t)(a.realm >> 32)) ^ (uint32_t)seed);
          AnimOps::makeLook(g, a, Role::Soldier, rr);
          squad.push_back(a.look);
        }
        c11Soldiers += (int)squad.size();
        group(squad, fmt("a squad of kingdom %zu", ki));
      }
    }
    check(bad13 == 0 && n13 > 0, "F13 rank tiers by slot", fmt("%d of %d looks off%s%s", bad13, n13, first13.empty() ? "" : "; first: ", first13.c_str()));
    check(bad14 == 0 && n14 > 0, "F14 regional lift", fmt("%d of %d off%s%s", bad14, n14, first14.empty() ? "" : "; first: ", first14.c_str()));
    check(bad15 == 0 && n15 > 0, "F15 herald padded", fmt("%d of %d heralds off%s%s", bad15, n15, first15.empty() ? "" : "; first: ", first15.c_str()));
    check(bad16 == 0 && n16 > 0, "F16 officers' alloy (D29)", fmt("%d of %d officers off%s%s", bad16, n16, first16.empty() ? "" : "; first: ", first16.c_str()));
    // F18: the bandit chief (spawnHuman: plate, helm and cape after makeLook) at T2 through retierLook
    {
      art::HumanLook L;
      L.outfit = art::Outfit::Leather; L.armorStyle = 1; L.weapon = 2;
      gear::rankTiers(L, 1, true, nullptr);
      L.outfit = art::Outfit::Plate; L.helmet = true; L.cape = true;
      retierLook(L, 2, nullptr);
      check(L.tier[H::TIER_BODY] == 2 && L.tier[H::TIER_HELM] == 2 && L.tier[H::TIER_CLOAK] == 2 && L.tier[H::TIER_WEAPON] == 2, "F18 chief retier",
            fmt("body T%d helm T%d cloak T%d weapon T%d (T2 each)", L.tier[H::TIER_BODY], L.tier[H::TIER_HELM], L.tier[H::TIER_CLOAK], L.tier[H::TIER_WEAPON]));
    }
    check(c11Worst <= 2 && c11Groups > 0, "F17 C11 fighters' faces", fmt("%d guards (12 a settlement), %d soldiers in squads of 8: largest group %d (2) over %d recipes%s%s", c11Guards, c11Soldiers, c11Worst, c11Groups, c11First.empty() ? "" : "; first: ", c11First.c_str()));
  }
  art::setRigVersion(was);
  std::printf("folk-sim: %d checks, %d failed\n", g_checks, g_fails);
  return g_fails ? 1 : 0;
}
RPG_TEST_CMD("--folk-sim", "P2b FOLK-SIM: census looks, ages, garments, gait switch, walk tables, sheathed / drawn [--seeds A..B] [--radius R] [--pool F] [--report]",
             folkSimCmd);

// ---------------------------------------------------------------- rpg_test --ladder-pace (P2c FOLK-SIM-C; PLAN "The gear
// ladder", proposal 3: "no measured gear-over-time curve exists"). At each pacing stage of VISION 1.4 (level, hours, the top
// of the danger band there) the look tiers (gear::lookTier) the shops stock (Game::shopStock: a smith's and a merchant's
// shelf at a settlement of that danger, the days and shelves varied) and the drops roll (gear::rollDropC: an ordinary body,
// a chest, a dungeon boss, the finder at the stage's level), 1,000 worn pieces each, printed beside PLAN's "When each tier
// shows up" table. A measure, not a rule: nothing here changes a roll. Checks (the rules the ladder follows): a shop never
// stocks T6 and never above T(band) with the Rare mark (one tier more with an Epic, the Honoured capital's cap); no T6
// drops below level 20 from an ordinary body (the Legendary floor). Options: --seed S (default 1), --rolls N (1000), --top
// (each stage at its band's top danger instead of its middle).
int ladderPaceCmd(int argc, char** argv) {
  uint64_t seed = 1;
  int rolls = 1000;
  for (int i = 1; i < argc; i++) {
    if (!std::strcmp(argv[i], "--seed") && i + 1 < argc) seed = std::strtoull(argv[++i], nullptr, 10);
    else if (!std::strcmp(argv[i], "--rolls") && i + 1 < argc) rolls = std::max(50, std::atoi(argv[++i]));
  }
  g_fails = 0; g_checks = 0;
  struct Stage { int level; float hours; int D; const char* where; };
  // the danger: the middle of VISION 1.4's band at each stage (1-6, 6-12, 12-18, 16-22, 20-27, 25-32, 32-42, up to 60);
  // --top uses the band's top (the frontier the player has just reached)
  bool top = false;
  for (int i = 1; i < argc; i++) if (!std::strcmp(argv[i], "--top")) top = true;
  const Stage stages[] = {{5, 0.5f, top ? 6 : 4, "start kingdom"},          {10, 3, top ? 12 : 9, "neighbouring kingdoms"},
                          {15, 7, top ? 18 : 15, "far side of the start culture"}, {20, 12, top ? 22 : 19, "2nd and 3rd culture families"},
                          {25, 19, top ? 27 : 24, "edge of the continent"},  {30, 28, top ? 32 : 29, "a 2nd continent"},
                          {40, 55, top ? 42 : 37, "far continents, the Mist"}, {50, 100, top ? 60 : 50, "anywhere"}};
  // PLAN "When each tier shows up": the tier worn for most of each span of hours
  auto planTier = [](float h) { return h < 3 ? 1 : h < 10 ? 2 : h < 25 ? 3 : h < 60 ? 4 : 5; };
  Game g(seed);
  g.newEndlessGame(seed);
  World& w = g.world;
  int si = -1;
  for (int pass = 0; pass < 3 && si < 0; pass++)   // a town or city, else a village; never a capital (its Honoured Epic shelf)
    for (size_t i = 0; i < w.sites.size() && si < 0; i++) {
      const SiteType t = w.sites[i].type;
      const bool kind = pass == 0 ? (t == SiteType::Town || t == SiteType::City) : pass == 1 ? t == SiteType::Village : true;
      if (kind && !w.sites[i].capital && w.cultureOf((int)i)) si = (int)i;
    }
  if (si < 0) { std::printf("FAIL ladder-pace: no town with a culture in seed %llu\n", (unsigned long long)seed); return 1; }
  const cult::Culture* cc = w.cultureOf(si);
  if (cc && w.src) cc = &w.src->culture(cc->id);
  auto worn = [](const Item& it) {
    return it.kind == ItemKind::Weapon || it.kind == ItemKind::Bow || it.kind == ItemKind::Staff || it.kind == ItemKind::Armor || it.kind == ItemKind::Helmet ||
           it.kind == ItemKind::Shield || it.kind == ItemKind::Gloves || it.kind == ItemKind::Boots || it.kind == ItemKind::Cloak;
  };
  auto makerOf = [&](const Item& it) -> const cult::Culture* {
    if (!it.culture || !w.src) return nullptr;
    return &w.src->culture(it.culture);
  };
  struct Hist { int n = 0, t[7] = {}, rare = 0, flame = 0; void add(const gear::LookTier& lt) { n++; t[std::clamp<int>(lt.tier, 0, 6)]++; rare += lt.rare; flame += lt.flame; } };
  auto row = [&](const Hist& h) {
    std::string s;
    for (int t = 1; t <= 6; t++) s += fmt(" %3d", h.n ? (int)std::lround(100.0 * h.t[t] / h.n) : 0);
    s += fmt("  r%3d", h.n ? (int)std::lround(100.0 * h.rare / h.n) : 0);
    return s;
  };
  auto mode = [](const Hist& h) { int b = 1; for (int t = 1; t <= 6; t++) if (h.t[t] > h.t[b]) b = t; return b; };
  std::printf("ladder-pace (seed %llu, %d worn pieces a column; shelves of %s, a %s town): percent of pieces at T1..T6, r = the Rare mark\n",
              (unsigned long long)seed, rolls, w.sites[(size_t)si].name.c_str(), cc ? cult::archetypeName(cc->archetype) : "?");
  std::printf("  lvl  hours  D  | shops  T1  T2  T3  T4  T5  T6     | bodies T1  T2  T3  T4  T5  T6     | chests T1  T2  T3  T4  T5  T6     | bosses T1  T2  T3  T4  T5  T6     | PLAN worn  shop/body mode\n");
  const int lv0 = w.sites[(size_t)si].level;
  int shopT6 = 0, shopOver = 0, earlyT6 = 0;
  std::string shopOverFirst;
  for (const Stage& st : stages) {
    Hist shop, body, chest, boss;
    // the shelves: a smith's and a merchant's at a settlement of the stage's danger, day after day
    w.sites[(size_t)si].level = st.D;
    for (int day = 0, k = 0; shop.n < rolls && day < 20000; day += 2, k++) {
      g.day = day;
      Actor a;
      a.id = 98000 + (k & 7); a.human = true; a.npc = true; a.site = si; a.bldg = -1; a.slot = 6000 + (k & 7);
      a.role = (k & 1) ? Role::Merchant : Role::Smith;
      for (const Item& it : LootOps::shopStock(g, a)) {
        if (!worn(it) || shop.n >= rolls) continue;
        const gear::LookTier lt = gear::lookTier(it, makerOf(it));
        if (!lt.tier) continue;
        shop.add(lt);
        if (lt.tier >= 6) shopT6++;
        // never above T(band) with the Rare mark (this town is no capital: its shelf's cap is Rare): the same piece at the
        // shelf's top item level (D + the spread) and Rare (a masterwork alloy is T5 at any band: lookTier says so)
        Item x = it;
        x.ilvl = (uint8_t)std::min(gear::MAX_D, st.D + gear::SHOP_BAND_SPREAD);
        x.rarity = Rarity::Rare;
        const gear::LookTier capT = gear::lookTier(x, makerOf(it));
        if (lt.tier > capT.tier) { shopOver++; if (shopOverFirst.empty()) shopOverFirst = fmt("D %d: T%d over T%d", st.D, lt.tier, capT.tier); }
      }
    }
    // the drops: an ordinary body, a chest, a dungeon boss
    Rng r(hash32((uint32_t)st.D * 7919u ^ (uint32_t)seed));
    auto roll = [&](Hist& h, gear::LootSrc src, uint8_t rank, bool isChest) {
      for (int tries = 0; h.n < rolls && tries < rolls * 400; tries++) {
        gear::DropSource ds;
        ds.D = st.D; ds.rank = rank; ds.chest = isChest; ds.src = src; ds.playerLevel = st.level; ds.culture = cc ? cc->id : 0;
        const Item it = gear::rollDropC(r, ds, cc);
        if (!worn(it)) continue;
        const gear::LookTier lt = gear::lookTier(it, makerOf(it));
        if (!lt.tier) continue;
        h.add(lt);
        if (src == gear::LootSrc::Body && rank == 0 && st.level < 20 && lt.tier >= 6) earlyT6++;
      }
    };
    roll(body, gear::LootSrc::Body, 0, false);
    roll(chest, gear::LootSrc::Chest, 0, true);
    roll(boss, gear::LootSrc::Boss, 2, false);
    std::printf("  %3d %6.1f %3d |      %s |      %s |      %s |      %s |   T%d      T%d/T%d\n", st.level, st.hours, st.D, row(shop).c_str(), row(body).c_str(),
                row(chest).c_str(), row(boss).c_str(), planTier(st.hours), mode(shop), mode(body));
  }
  w.sites[(size_t)si].level = lv0;
  std::printf("  PLAN \"When each tier shows up\": T1 hours 0-3, T2 3-10, T3 10-25, T4 25-60, T5 60-100, T6 earned from level 20 (bosses,\n"
              "  hoards, finales; one piece from level 10, two at 25, three at 40 by attunement), never bought\n");
  check(shopT6 == 0, "LP1 shops never T6", fmt("%d T6 pieces on the shelves", shopT6));
  check(shopOver == 0, "LP2 shop tier cap", fmt("%d pieces above T(band) + Rare / Epic%s%s", shopOver, shopOverFirst.empty() ? "" : "; first: ", shopOverFirst.c_str()));
  check(earlyT6 == 0, "LP3 no early Legendary", fmt("%d T6 pieces from ordinary bodies below level 20", earlyT6));
  std::printf("ladder-pace: %d checks, %d failed\n", g_checks, g_fails);
  return g_fails ? 1 : 0;
}
RPG_TEST_CMD("--ladder-pace", "P2c FOLK-SIM-C: the look tiers shops stock and drops roll at each pacing stage (VISION 1.4) beside PLAN's ladder table [--seed S] [--rolls N] [--top]",
             ladderPaceCmd);


}  // namespace
