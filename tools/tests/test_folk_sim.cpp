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
    std::printf("  neck: none %d scarf %d kerchief %d collar %d; garb (0-9):", neck[0], neck[1], neck[2], neck[3]);
    for (int i = 0; i <= 9; i++) std::printf(" %d", garbN[i]);
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
    check(endF >= 0 && k.drawn && k.variant == 1 && k.part == art::RigPart::Action && k.clip == art::Clip::Idle, "F9 drawn after a swing",
          fmt("1 s after: clip %s variant %d part %d drawn %d", art::clipName(k.clip), k.variant, (int)k.part, (int)k.drawn));
    standStill(g, 2.85f);   // 3.85 s after the swing: still drawn
    const bool still = animPick(g, g.pl(), 2).drawn;
    standStill(g, 0.3f);    // 4.15 s: sheathed
    k = animPick(g, g.pl(), 2);
    check(still && !k.drawn && k.variant == 0 && k.part == art::RigPart::Loco, "F9 sheathed after 4 s",
          fmt("3.85 s drawn %d; 4.15 s drawn %d variant %d", (int)still, (int)k.drawn, k.variant));
    // walking drawn: the ACTION part's walk variant 1; running drawn: RUN variant 1
    tick(g, in);
    for (int f = 0; f < 40; f++) tick(g, Input());
    Run rr = walkEast(g, 1.0f, 0.5f);
    k = animPick(g, g.pl(), 2);
    check(k.drawn && k.part == art::RigPart::Run && k.variant == 1, "F9 drawn run", fmt("gait %d part %d variant %d", rr.gait, (int)k.part, k.variant));
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
  art::setRigVersion(was);
  std::printf("folk-sim: %d checks, %d failed\n", g_checks, g_fails);
  return g_fails ? 1 : 0;
}
RPG_TEST_CMD("--folk-sim", "P2b FOLK-SIM: census looks, ages, garments, gait switch, walk tables, sheathed / drawn [--seeds A..B] [--radius R] [--pool F] [--report]",
             folkSimCmd);

}  // namespace
