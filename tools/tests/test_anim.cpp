// rpg_test --anim (P2a Clear Sight, VISION_PLAN 15.36 item 11: the lead's frozen harness; a lane reports a harness
// problem instead of editing it). The animation pick (rpg/sim/anim.h) and the timing the view and the sim share.
//   rpg_test --anim [--seeds A..B] [--gates] [--write]
// Always (phase A, CI): A1, the v1 frame pick at rig 1 against a frozen copy of render.cpp's pre-P2a switch over every
// state x time x combo, and the golden hash tests/fixtures/golden_anim_v1.txt (--write records it).
// --gates (the P2a DONE gates the ANIM lane works to; FAIL until it lands):
//   A2  9 distinct swing effects by family (animSwingFx at rig 1 and 2, their sprites pairwise different); the hero's
//       effect ends with the strike state within one frame (dagger pace 0.8, mace 1.25: sim steps)
//   A3  enemy windups by family measured in the sim (dagger < unarmed < sword < spear = staff < axe < mace < greatsword,
//       in the plan's ratios within 5 %), each equal to animEnemyWindup within 1.5 frames; the spawn-weighted mean over
//       the bandits the sim makes stays today's 0.36 +- 0.01 s (measured in phase A: the bandits spawn with Actor::windup
//       0.36; the plan's 0.30 read ai.cpp's folk branch, the guards' and militia's swing)
//   A4  rig 2: a killed bandit shows Lying 0.45 s or less after the killing blow; lies animCorpseLife(2) = 60 s (gone by
//       60.2); at most animCorpseCap(2) = 24 at once with 30 killed; the hero walks through a corpse
//   A5  rig 2: the hero's fall plays under the death screen (Fall, then Lying by 0.45 s of Mode::Dead)
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <vector>
#include "rpg/art.h"
#include "rpg/sim/anim.h"
#include "rpg/sim/gear.h"
#include "tools/tests/tests.h"

namespace {

int g_fail = 0;

// ---- A1: render.cpp's frame pick before P2a, frozen here (never edit: it IS the reference)
int v1Reference(const Actor& a) {
  int fr = 0;
  switch (a.st) {
    case AState::Walk: fr = 1 + (int)(a.animT * 9) % 4; break;
    case AState::Roll: fr = 1 + (int)(a.stT * 20) % 4; break;
    case AState::Windup: case AState::Cast: fr = 5; break;
    case AState::Strike: fr = 6; break;
    case AState::Recover: fr = 6; break;
    case AState::Hurt: case AState::Dead: fr = 7; break;
    default: fr = 0; break;
  }
  return fr;
}

std::string goldenPath() { return std::string(EMB_SOURCE_DIR) + "/tests/fixtures/golden_anim_v1.txt"; }

int checkA1(bool write) {
  const int f0 = g_fail;
  Game g(1);
  uint64_t h = 1469598103934665603ull;
  auto mix = [&](uint64_t v) { h ^= v; h *= 1099511628211ull; };
  int n = 0;
  for (int st = 0; st <= (int)AState::Down; st++)
    for (int combo = 0; combo < 3; combo++)
      for (int k = 0; k <= 720; k++) {
        Actor a;
        a.human = true;
        a.st = (AState)st;
        a.combo = combo;
        a.animT = a.stT = (float)k / 256.0f;   // (exact binary fractions: the golden is the same on MSVC and clang)
        a.look.weapon = (uint8_t)(k % 13);
        const AnimPick p = animPick(g, a, 1);
        const int want = v1Reference(a);
        if (p.frame != want) {
          if (g_fail - f0 < 5) std::printf("FAIL: A1 state %d t %.4f combo %d: animPick frame %d, v1 %d\n", st, a.stT, combo, p.frame, want);
          g_fail++;
        }
        mix((uint64_t)st); mix((uint64_t)combo); mix((uint64_t)k); mix(p.frame); mix((uint64_t)p.clip); mix(p.variant); mix(p.frames);
        n++;
      }
  char hx[24];
  std::snprintf(hx, sizeof hx, "%016llx", (unsigned long long)h);
  if (write) {
    if (FILE* f = std::fopen(goldenPath().c_str(), "wb")) {
      std::fprintf(f, "# P2a phase A: rpg_test --anim --write (the v1 frame pick at rig 1 over %d samples; never re-recorded by a lane)\n%s\n", n, hx);
      std::fclose(f);
      std::printf("anim: golden written: %s\n", hx);
    }
  } else {
    std::string want;
    if (FILE* f = std::fopen(goldenPath().c_str(), "rb")) {
      char line[256];
      while (std::fgets(line, sizeof line, f))
        if (line[0] != '#') { want = line; break; }
      std::fclose(f);
    }
    while (!want.empty() && (want.back() == '\n' || want.back() == '\r')) want.pop_back();
    if (want.empty()) { std::printf("FAIL: A1 no golden %s (rpg_test --anim --write)\n", goldenPath().c_str()); g_fail++; }
    else if (want != hx) { std::printf("FAIL: A1 golden %s, now %s\n", want.c_str(), hx); g_fail++; }
  }
  std::printf("A1 v1 frame pick at rig 1: %d samples, %s (%s)\n", n, g_fail == f0 ? "identical" : "DIFFERS", hx);
  return g_fail - f0;
}

// ---- the sim gates' stage: a fresh endless game, the hero alone on open ground, nothing wild
uint8_t weaponOf(art::Family f) {
  switch (f) {
    case art::Family::Dagger: return 5;
    case art::Family::Sword: return 1;
    case art::Family::Greatsword: return art::WEAPON_GREATSWORD;
    case art::Family::Axe: return 2;
    case art::Family::Mace: return 6;
    case art::Family::Spear: return 8;
    case art::Family::Staff: return 4;
    case art::Family::Bow: return 3;
    default: return 0;
  }
}
void stage(Game& g, uint64_t seed) {
  g.newEndlessGame(seed);
  g.mode = Mode::Play;
  g.noWildSpawns = true;
  g.hour = 12;
  for (int f = 0; f < 120; f++) { g.update(SIM_DT, Input()); g.events.clear(); g.mode = Mode::Play; g.actors.resize(1); }
  g.actors.resize(1);
}
void tick(Game& g, const Input& in) { g.update(SIM_DT, in); g.events.clear(); }
void arm(Game& g, WeaponType t) {
  Rng r(91u + (uint32_t)t);
  g.inv.clear();
  g.eqWeapon = g.eqBow = g.eqStaff = g.eqArmor = g.eqHelmet = g.eqShield = g.eqRing = g.eqAmulet = g.eqGloves = g.eqBoots = g.eqCloak = -1;
  g.inv.push_back(gear::makeGear(r, ItemKind::Weapon, (int)t, 5, Rarity::Common, 0, Mat::Iron));
  g.eqWeapon = 0;
  AnimOps::refreshHero(g);
}

int checkA2(uint64_t seed) {
  const int f0 = g_fail;
  // the 9 effects
  {
    Game g(1);
    for (int rv = 1; rv <= 2; rv++) {
      art::setRigVersion(rv);
      std::set<int> fx;
      std::vector<Canvas> sp;
      for (int f = 0; f < art::FAMILY_COUNT; f++) {
        Actor a;
        a.human = true; a.st = AState::Strike; a.stT = 0.02f; a.face = 2;
        a.look.weapon = weaponOf((art::Family)f);
        const art::Fx x = animSwingFx(g, a);
        if (fx.insert((int)x).second) sp.push_back(art::fxSprite(x));
      }
      int same = 0;
      for (size_t i = 0; i < sp.size(); i++)
        for (size_t j = i + 1; j < sp.size(); j++) same += sp[i].w == sp[j].w && sp[i].h == sp[j].h && sp[i].px == sp[j].px;
      if (fx.size() != (size_t)art::FAMILY_COUNT || same)
        std::printf("FAIL: A2 rig %d: %zu distinct swing effects for %d families (%d identical sprite pairs)\n", rv, fx.size(), art::FAMILY_COUNT, same), g_fail++;
    }
    art::setRigVersion(1);
  }
  // the effect ends with the strike (the hero, sim steps)
  for (WeaponType t : {WeaponType::Dagger, WeaponType::Mace, WeaponType::Sword}) {
    Game g(seed);
    stage(g, seed);
    arm(g, t);
    Input in;
    in.attack = true;
    // the frame the effect reaches its end (progress 1) while the strike lasts; if the strike ends first, where its
    // progress rate would have reached 1 (an effect too slow for the family is late, not "done at the strike's end")
    int strikeOn = -1, strikeOff = -1;
    float fxDone = -1, prFirst = -1, prLast = -1;
    int fFirst = -1, fLast = -1;
    for (int f = 0; f < 120 && strikeOff < 0; f++) {
      tick(g, in);
      in.attack = false;
      const Actor& p = g.pl();
      if (p.st == AState::Strike && strikeOn < 0) strikeOn = f;
      if (strikeOn >= 0 && p.st != AState::Strike && strikeOff < 0) { strikeOff = f; break; }
      const float pr = animSwingProgress(g, p);
      if (p.st == AState::Strike && pr >= 0) {
        if (fFirst < 0) { fFirst = f; prFirst = pr; }
        fLast = f; prLast = pr;
        if (fxDone < 0 && pr >= 1.0f) fxDone = (float)f;
      }
    }
    if (strikeOn < 0 || strikeOff < 0) { std::printf("FAIL: A2 %s: the hero never struck\n", t == WeaponType::Dagger ? "dagger" : t == WeaponType::Mace ? "mace" : "sword"); g_fail++; continue; }
    if (fxDone < 0) {
      const float rate = fLast > fFirst ? (prLast - prFirst) / (float)(fLast - fFirst) : 0.0f;
      fxDone = rate > 1e-4f ? (float)fLast + (1.0f - prLast) / rate : 999.0f;
    }
    const float d = std::fabs(fxDone - (float)strikeOff);
    std::printf("A2 %s (pace %.2f): strike %d frames, the effect reaches its end at frame %.1f of it (off by %.1f)\n",
                t == WeaponType::Dagger ? "dagger" : t == WeaponType::Mace ? "mace" : "sword", AnimOps::heroPace(g), strikeOff - strikeOn,
                fxDone - (float)strikeOn, d);
    if (d > 1.0f) { std::printf("FAIL: A2 the effect ends %.1f frames from the strike state's end (1 or less)\n", d); g_fail++; }
  }
  return g_fail - f0;
}

// the windup a bandit holding family f shows in the sim (s), or -1
float measureWindup(uint64_t seed, art::Family f, float& api) {
  api = -1;
  Game g(seed);
  stage(g, seed);
  g.godMode = true;
  const int bi = AnimOps::spawnBandit(g, g.pl().p + Vec2(14, 0), weaponOf(f));
  if (bi < 0) return -1;
  g.actors[(size_t)bi].ranged = false;
  api = animEnemyWindup(g.actors[(size_t)bi]);   // what the API says of this very bandit
  int on = -1;
  for (int k = 0; k < 60 * 8; k++) {
    tick(g, Input());
    g.mode = Mode::Play;
    if (bi >= (int)g.actors.size()) return -1;
    const Actor& b = g.actors[(size_t)bi];
    if (b.st == AState::Windup && on < 0) on = k;
    if (on >= 0 && b.st != AState::Windup) return (k - on) * SIM_DT;
  }
  return -1;
}

// today's spawn-weighted mean enemy windup (phase A, measured: every bandit spawns with Actor::windup 0.36 s)
constexpr double kTodayMeanWindup = 0.36;

int checkA3(uint64_t seed) {
  const int f0 = g_fail;
  static const float plan[art::FAMILY_COUNT] = {0.24f, 0.22f, 0.28f, 0.38f, 0.32f, 0.36f, 0.30f, 0.30f, 0.0f};   // by Family (bow: none)
  float w[art::FAMILY_COUNT] = {};
  for (int f = 0; f < art::FAMILY_COUNT; f++) {
    if (f == (int)art::Family::Bow) continue;
    float api = -1;
    w[f] = measureWindup(seed, (art::Family)f, api);
    std::printf("A3 %-10s windup %.3f s in the sim, animEnemyWindup %.3f\n", art::familyName((art::Family)f), w[f], api);
    if (w[f] < 0) { std::printf("FAIL: A3 %s: no windup measured\n", art::familyName((art::Family)f)); g_fail++; continue; }
    if (std::fabs(w[f] - api) > SIM_DT * 1.5f) { std::printf("FAIL: A3 %s: the sim winds up %.3f s, animEnemyWindup says %.3f\n", art::familyName((art::Family)f), w[f], api); g_fail++; }
  }
  if (w[(int)art::Family::Sword] > 0)
    for (int f = 0; f < art::FAMILY_COUNT; f++) {
      if (f == (int)art::Family::Bow || w[f] <= 0) continue;
      const float want = plan[f] / plan[(int)art::Family::Sword], have = w[f] / w[(int)art::Family::Sword];
      if (std::fabs(have - want) > 0.05f * want) { std::printf("FAIL: A3 %s: windup %.2f x the sword's, the plan %.2f\n", art::familyName((art::Family)f), have, want); g_fail++; }
    }
  // the spawn-weighted mean over the bandits the sim makes
  {
    Game g(seed);
    stage(g, seed);
    double sum = 0;
    int n = 0;
    std::map<int, int> fam;
    for (int k = 0; k < 300; k++) {
      const int bi = AnimOps::spawnBandit(g, g.pl().p + Vec2(400.0f + (float)(k % 20) * 8, (float)(k / 20) * 8), -1);
      if (bi < 0) continue;
      const Actor& b = g.actors[(size_t)bi];
      if (b.ranged || b.look.weapon == 3) continue;
      sum += animEnemyWindup(b);
      fam[(int)art::familyOf(b.look)]++;
      n++;
    }
    const double mean = n ? sum / n : 0;
    std::printf("A3 spawn-weighted mean windup over %d melee bandits: %.3f s (families:", n, mean);
    for (const auto& kv : fam) std::printf(" %s %d", art::familyName((art::Family)kv.first), kv.second);
    std::printf(")\n");
    if (std::fabs(mean - kTodayMeanWindup) > 0.01) { std::printf("FAIL: A3 the mean windup %.3f s is not today's %.2f +- 0.01\n", mean, kTodayMeanWindup); g_fail++; }
  }
  return g_fail - f0;
}

int checkA4(uint64_t seed) {
  const int f0 = g_fail;
  art::setRigVersion(2);
  {   // lying after the killing blow; the corpse's life
    Game g(seed);
    stage(g, seed);
    g.godMode = true;
    const int bi = AnimOps::spawnBandit(g, g.pl().p + Vec2(40, 0), 1);
    const int id = bi >= 0 ? g.actors[(size_t)bi].id : -1;
    AnimOps::damage(g, bi, 1e6f);
    float lyingAt = -1, goneAt = -1;
    for (int k = 0; k < 60 * 62 && goneAt < 0; k++) {
      int at = -1;
      for (size_t i = 0; i < g.actors.size(); i++) if (g.actors[i].id == id) at = (int)i;
      if (at < 0) { goneAt = k * SIM_DT; break; }
      const AnimPick p = animPick(g, g.actors[(size_t)at], 2);
      if (lyingAt < 0 && p.clip == art::Clip::Lying) lyingAt = k * SIM_DT;
      tick(g, Input());
      g.mode = Mode::Play;
    }
    std::printf("A4 rig 2: lying %.2f s after the blow, the corpse gone at %.1f s\n", lyingAt, goneAt);
    if (lyingAt < 0 || lyingAt > 0.45f) { std::printf("FAIL: A4 lying %.2f s after the killing blow (0.45 or less)\n", lyingAt); g_fail++; }
    const float life = animCorpseLife(2);
    if (std::fabs(life - 60.0f) > 0.01f) { std::printf("FAIL: A4 animCorpseLife(2) %.1f s (60)\n", life); g_fail++; }
    if (goneAt < 59.8f || goneAt > 60.2f) { std::printf("FAIL: A4 the corpse lay %.1f s (60)\n", goneAt); g_fail++; }
  }
  {   // the cap; walking through a corpse
    Game g(seed);
    stage(g, seed);
    g.godMode = true;
    int most = 0;
    for (int k = 0; k < 30; k++) {
      const int bi = AnimOps::spawnBandit(g, g.pl().p + Vec2(60.0f + (float)(k % 6) * 10, -30.0f + (float)(k / 6) * 12), 1);
      AnimOps::damage(g, bi, 1e6f);
      for (int s = 0; s < 6; s++) { tick(g, Input()); g.mode = Mode::Play; }
      int dead = 0;
      for (const Actor& a : g.actors) dead += a.human && a.st == AState::Dead;
      most = std::max(most, dead);
    }
    std::printf("A4 rig 2: at most %d corpses at once with 30 killed (cap %d)\n", most, animCorpseCap(2));
    if (animCorpseCap(2) != 24 || most > 24) { std::printf("FAIL: A4 %d corpses at once (24 at most)\n", most); g_fail++; }
    // a corpse right in the hero's way
    const Vec2 start = g.pl().p;
    const int bi = AnimOps::spawnBandit(g, start + Vec2(10, 0), 1);
    AnimOps::damage(g, bi, 1e6f);
    for (int s = 0; s < 30; s++) { tick(g, Input()); g.mode = Mode::Play; }
    Input in;
    in.move = Vec2(1, 0);
    for (int s = 0; s < 30; s++) { tick(g, in); g.mode = Mode::Play; }
    if (g.pl().p.x < start.x + 18.0f) { std::printf("FAIL: A4 a corpse blocks the hero (walked %.1f px of 18)\n", g.pl().p.x - start.x); g_fail++; }
  }
  art::setRigVersion(1);
  return g_fail - f0;
}

int checkA5(uint64_t seed) {
  const int f0 = g_fail;
  art::setRigVersion(2);
  Game g(seed);
  stage(g, seed);
  g.godMode = false;
  AnimOps::damage(g, 0, 1e6f);
  bool fell = false;
  float lyingAt = -1;
  for (int k = 0; k < 60 && lyingAt < 0; k++) {
    tick(g, Input());
    const AnimPick p = animPick(g, g.pl(), 2);
    if (p.clip == art::Clip::Fall) fell = true;
    if (p.clip == art::Clip::Lying) lyingAt = (k + 1) * SIM_DT;
  }
  std::printf("A5 rig 2: the hero's fall %s, lying at %.2f s (mode %d)\n", fell ? "plays" : "never plays", lyingAt, (int)g.mode);
  if (!fell || lyingAt < 0 || lyingAt > 0.45f) { std::printf("FAIL: A5 the hero's fall under the death screen\n"); g_fail++; }
  art::setRigVersion(1);
  return g_fail - f0;
}

int animCmd(int argc, char** argv) {
  bool gates = false, write = false;
  uint64_t a = 1, b = 1;
  for (int i = 1; i < argc; i++) {
    if (!std::strcmp(argv[i], "--gates")) gates = true;
    else if (!std::strcmp(argv[i], "--write")) write = true;
    else if (!std::strcmp(argv[i], "--seeds") && i + 1 < argc) parseSeedRange(argv[++i], a, b);
  }
  g_fail = 0;
  checkA1(write);
  if (gates)
    for (uint64_t s = a; s <= b; s++) {
      std::printf("-- seed %llu\n", (unsigned long long)s);
      checkA2(s);
      checkA3(s);
      checkA4(s);
      checkA5(s);
    }
  std::printf(g_fail ? "anim: %d failure(s)\n" : "anim: 0 failures\n", g_fail);
  return g_fail ? 1 : 0;
}
RPG_TEST_CMD("--anim", "P2a animation pick and timing [--seeds A..B] [--gates] [--write]: A1 always; --gates A2-A5", animCmd);

}  // namespace
