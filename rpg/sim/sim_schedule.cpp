// EMBERVALE simulation: Game::update, the update order (SOLID 4.1). This file is the determinism contract: the
// steps below run in this order every update, each one documented where it is called. Never reorder them, and never
// add a step between two others without the reason written next to it: saves, Witness replays and the sim digest
// all depend on this order. Moved verbatim from game.cpp.
#include "rpg/sim/game.h"
#include <cstdlib>
#include <chrono>
#include <cstdio>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string>
#include <vector>
#include "engine/sound_ids.h"
#include "rpg/sim/anim.h"
#include "rpg/sim/game_internal.h"
#include "rpg/sim/gear.h"
#include "rpg/sim/gear_look.h"   // (P2c wave-2 fix round, D86) the bandit chief's ladder tier
#include "rpg/sim/home.h"
#include "rpg/sim/power_ops.h"
#include "rpg/sim/magic_ops.h"
#include "rpg/sim/lord_ops.h"
#include "rpg/sim/sea_ops.h"
#include "rpg/sim/jobs_ops.h"
#include "rpg/sim/service_ops.h"
#include "rpg/sim/powers.h"
#include "rpg/sim/skill_ops.h"
#include "rpg/sim/stream.h"
#include "rpg/sim/war.h"
#include "rpg/culture/society.h"
#include "rpg/sim/life_hooks.h"

// ------------------------------------------------------------------ main update
namespace {
// (P2a ANIM) rig 2: where each body stood before the step, so a walker's animT runs by the distance it walked (STYLE_P2
// 10: the walk by the whole px drawn, anim.cpp animWalkFrame). One Game steps at a time per thread.
struct WalkMark { int id; Vec2 p; bool walking; };   // (SLICE-WALK-SLIDE) walking: the state before the step
thread_local std::vector<WalkMark> tlWalk;
}  // namespace

void Game::update(float dt, const Input& in) {
  // (P2a ANIM) the hero's fall plays under the death screen: the view runs it on its own clock (render_anim.cpp); a
  // headless step (rpg_test --anim) advances the body's state time the same way
  if (mode == Mode::Dead && art::rigVersion() >= 2 && pl().st == AState::Dead) pl().stT += dt;
  if (mode != Mode::Play) return;
  if (travel.phase != TravelPhase::None) { travelStep(dt); return; }   // M2: on the road behind the fade (travel.cpp)
  // 1. the clocks: the stamina flash, the hit-stop freeze, the slow-motion blip, sim time and the hour of the day
  if (stFlash > 0) stFlash -= dt;
  if (hitStop > 0) { hitStop -= dt; return; }
  if (slowMo > 0) { slowMo -= dt; dt *= 0.3f; }   // perfect roll / level-up: a brief slow-motion blip
  time += dt;
  float prevHour = hour;
  // (owner) daytime lasts 20 real minutes and the night 10: the 14.5 day hours (5:30-20:00, isNight) run over 1200 s,
  // the 9.5 night hours over 600 s
  hour += dt * (isNight() ? 9.5f / 600.0f : 14.5f / 1200.0f);
  if (hour >= 24) { hour -= 24; day++; }
  (void)prevHour;
  // 2. the notice, the shrine blessing and the sleep fade run down
  if (noticeT > 0) noticeT -= dt;
  if (blessT > 0) { blessT -= dt; if (blessT <= 0) recalcPlayer(); }
  if (sleepFade > 0) sleepFade = std::max(0.0f, sleepFade - dt);

  if (!inside) prepInteriorTick(dt);   // (M3c carry) the interior of the door ahead, before it is reached
  const bool rig2 = art::rigVersion() >= 2;
  if (rig2) {
    tlWalk.resize(actors.size());
    for (size_t i = 0; i < actors.size(); i++) tlWalk[i] = WalkMark{actors[i].id, actors[i].p, actors[i].st == AState::Walk};
  }
  // 3. the player, then the lanes that ride on the player's step (powers, skills, magic, sea, lordship, service, jobs)
  updatePlayer(dt, in);
  PowerOps::step(*this, dt);   // M8b: the worn powers' clocks, fire patches, auras, stars, spectral wolves (powers.cpp)
  SkillOps::step(*this, dt);   // M9: distances walked / ridden, sneaking, familiarity (skills_game.cpp, SKILLS lane)
  MagicOps::step(*this, dt);   // M9: casts in progress, concentration, surfaces and their combos (magic_game.cpp, MAGIC lane)
  SeaOps::step(*this, dt);     // M10: the wind, storms, the Mist, the boats, voyages, encounters (sea_game.cpp, SEA lane)
  LordOps::step(*this, dt);    // M12: the fief's day: taxes, tribute, works (lord_game.cpp, LORD lane)
  ServiceOps::step(*this, dt); // M12: the army's day: wages, orders, missions (service_game.cpp, SERVICE lane)
  JobOps::step(*this, dt);     // P1: leads going stale, last week's job pools dropped (jobs.cpp, JOBS lane)
  // 4. the endless window: recentre on the player, keep the streamer's wish list current; then the fog of war
  if (world.endless && !inside) { maybeRecentre(); prefetchTick(dt); }
  if (!inside && (exploreT_ -= dt) <= 0) {   // fog of war: what the screen shows around the player (global tiles)
    exploreT_ = 0.25f;
    // (a box a little wider than the 4x phone view, about 40 x 24 tiles: what the player has seen is on the map)
    explored.markAround(world.ox + (int)std::floor(pl().p.x / TILE), world.oy + (int)std::floor(pl().p.y / TILE), 24);
  }
  // 5. the threats round the townsfolk, then every actor's step (NPC level of detail first)
  collectHostiles();
  perf.npcAwake = perf.npcAsleep = 0;
  const float foeK = PowerOps::foeScale(*this);   // (M8b) Chronos: the hero's foes move at a third of the pace
  for (size_t i = 1; i < actors.size(); i++) {
    Actor& a = actors[i];
    // NPC level of detail (VISION_PLAN 4.4): townsfolk far off screen with nothing to react to think a few times a
    // second with the gathered time; everything near, fighting or in a ringing town runs every step
    if (a.npc && sleepy(a)) {
      a.asleep = true;
      perf.npcAsleep++;
      a.lodAcc += dt;
      if (a.lodAcc < 0.2f) continue;
      float acc = std::min(a.lodAcc, 0.25f);
      a.lodAcc = 0;
      updateActor(a, acc);
      continue;
    }
    if (a.npc) perf.npcAwake++;
    if (a.asleep) { a.asleep = false; a.lodAcc = 0; }
    updateActor(a, a.hostile ? dt * foeK : dt);
  }
  // separation between bodies
  for (size_t i = 0; i < actors.size(); i++)
    for (size_t j = i + 1; j < actors.size(); j++) {
      Actor& a = actors[i]; Actor& b = actors[j];
      if (a.st == AState::Dead || b.st == AState::Dead || a.fly || b.fly) continue;
      if (a.asleep && b.asleep) continue;
      Vec2 d = b.p - a.p;
      float r = a.radius + b.radius;
      float l2 = len2(d);
      if (l2 >= r * r || l2 < 1e-4f) continue;
      float l = std::sqrt(l2);
      Vec2 push = d * ((r - l) / l * 0.5f);
      // (M5) the watch running to a fight has the right of way through the townsfolk (and through the animals): the
      // crowd steps aside, the guard is not shoved back
      auto fighting = [](const Actor& x) { return x.npc && x.role == Role::Guard && x.target >= 0; };
      auto yields = [](const Actor& x) { return x.npc && !x.hostile && x.role != Role::Guard; };
      const bool aYields = (fighting(b) && yields(a)) || (a.critter && !b.critter);
      const bool bYields = (fighting(a) && yields(b)) || (b.critter && !a.critter);
      // (M5 fixer) a body on furniture (seated, abed, at the anvil: Actor::useX) is not shoved off it: the other steps
      // aside (two of them leave each other be)
      const bool aFixed = a.npc && a.useX >= 0, bFixed = b.npc && b.useX >= 0;
      if (aFixed && bFixed) continue;
      if (aFixed) { if (!b.player) moveActor(b, push * 2.0f); continue; }
      if (bFixed) { if (!a.player) moveActor(a, push * -2.0f); continue; }
      if (aYields && !bYields) { if (!a.player) moveActor(a, push * -2.0f); continue; }
      if (bYields && !aYields) { if (!b.player) moveActor(b, push * 2.0f); continue; }
      if (!a.player) moveActor(a, push * -1.0f);
      if (!b.player) moveActor(b, push);
      if (a.player && b.npc) moveActor(b, push);
    }
  // 6. the town bells and the watch; then each lane's world step, timed for --perf, in this fixed order
  updateTownDefence(dt);
  // (M6 fixer) each subsystem's step timed for --perf (PerfCounters::sysWorstMs): the hitches' suspects
  auto timed = [&](int sys, auto&& fn) {
    const auto t0 = std::chrono::steady_clock::now();
    fn();
    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    if (ms > perf.sysWorstMs[sys]) perf.sysWorstMs[sys] = ms;
  };
  using PC = PerfCounters;
  timed(PC::S_QUEST, [&] { questTick(dt); waysideTick(dt); });   // M2: deliveries, escorts, the Protect night; the toll bridge
  timed(PC::S_REALM, [&] { realmStep(dt); });   // M4: the living world (realm_game.cpp, REALM lane)
  timed(PC::S_WAR, [&] { warStep(dt); });       // M4: camps, patrols, refugees, garrisons, war damage (war_game.cpp, WARDS lane)
  timed(PC::S_STORY, [&] { storyStep(dt); });   // M4: stories, rumours, heralds, lore (rpg/story/story_game.cpp, STORY lane)
  timed(PC::S_LIFESTEP, [&] { LifeHooks::step(*this, dt); }); // M5: the townsfolk's actors by their plans (life_game.cpp, TOWNSFOLK lane)
  timed(PC::S_RAID, [&] { LifeHooks::raidStep(*this, dt); });     // M5: night raids by monster pressure (life_raids.cpp, CITIZENS lane)
  timed(PC::S_FOE, [&] { foeStep(dt); });       // M6: elites' affixes, boss phases, named uniques, world bosses (foes_game.cpp)
  timed(PC::S_HOME, [&] { homeStep(dt); });     // M7: the player's plots, crops, animals, building, riding (home_game.cpp)
  timed(PC::S_PARTY, [&] { partyStep(dt); });   // M8: companions, deeds, the downed, graves, banter (party_game.cpp)
  timed(PC::S_LIFETICK, [&] { life.tick(*this, dt); });   // M5: needs, plans, stock and moods in aggregate (life.cpp)
  // 7. missiles, pickups, roaming spawns, the location label
  updateProjectiles(dt);
  updatePickups(dt);
  if (!inside) timed(PC::S_SPAWN, [&] { updateSpawning(dt); });
  updateLocation();
  // cleanup corpses. (P2a ANIM) at rig 2 the human dead lie animCorpseLife (60 s, fading over the last 5), at most
  // animCorpseCap at once: past cap - 3 the oldest start their fade early, past the cap the oldest go at once; beasts
  // and rig 1 keep today's 6 s
  const int rv = art::rigVersion();
  const float humanLife = animCorpseLife(rv);
  if (const int cap = animCorpseCap(rv)) {
    int lying = 0;   // corpses not yet fading: at most cap - 3 (the fading ones make up the rest, gone within 5 s)
    for (size_t i = 1; i < actors.size(); i++) lying += actors[i].human && actors[i].st == AState::Dead && actors[i].stT < humanLife - kCorpseFade;
    while (lying > cap - 3) {
      Actor* oldest = nullptr;   // the oldest corpse not yet fading
      for (size_t i = 1; i < actors.size(); i++) {
        Actor& a = actors[i];
        if (a.human && a.st == AState::Dead && a.stT < humanLife - kCorpseFade && (!oldest || a.stT > oldest->stT)) oldest = &a;
      }
      if (!oldest) break;
      oldest->stT = humanLife - kCorpseFade;
      lying--;
    }
    int over = -cap;
    for (size_t i = 1; i < actors.size(); i++) over += actors[i].human && actors[i].st == AState::Dead;
    for (; over > 0; over--) {
      size_t at = 0;
      for (size_t i = 1; i < actors.size(); i++)
        if (actors[i].human && actors[i].st == AState::Dead && actors[i].stT <= humanLife && (!at || actors[i].stT > actors[at].stT)) at = i;
      if (!at) break;
      actors[at].stT = humanLife + 1.0f;   // gone in the pass below
    }
  }
  for (size_t i = 1; i < actors.size();) {
    Actor& a = actors[i];
    bool far = a.wild && len2(a.p - pl().p) > (48.0f * TILE) * (48.0f * TILE);
    if ((a.st == AState::Dead && a.stT > (a.human ? humanLife : 6.0f)) || far) {
      if (a.id == dragonId_) dragonId_ = -1;
      actors.erase(actors.begin() + i);
    } else i++;
  }
  // (P2a ANIM) rig 2: a walking human's animT runs by the distance walked over its own speed (anim.cpp: the walk frame
  // is animWalkFrame(animT x speed)); a body standing, fighting or carried across a window shift runs on time as before
  if (rig2) {
    for (size_t i = 0; i < actors.size(); i++) {
      Actor& a = actors[i];
      if (!(a.human || a.player)) continue;
      const WalkMark* w = i < tlWalk.size() && tlWalk[i].id == a.id ? &tlWalk[i] : nullptr;
      if (!w) for (const WalkMark& m : tlWalk) if (m.id == a.id) { w = &m; break; }
      const float d = w ? len(a.p - w->p) : -1.0f;
      if (a.st == AState::Walk && d >= 0 && d < 32.0f) {
        // (P2a slice, SLICE-WALK-SLIDE) the DRAWN travel: the whole world px the feet moved on the screen along the step's
        // main axis (render_anim.cpp puts the pivot at round(p) on the whole-pixel camera), so anim.cpp's frames change
        // on the px where the art's planted sole has stepped and the foot lands back on its spot; a walk starts on the
        // passing frame (animWalkBegin: its planted foot is the idle stance's back foot)
        const float sp = std::max(1.0f, a.speed);
        const Vec2 s = a.p - w->p;
        const float drawn = std::fabs(s.x) >= std::fabs(s.y) ? std::fabs(std::floor(a.p.x + 0.5f) - std::floor(w->p.x + 0.5f))
                                                              : std::fabs(std::floor(a.p.y + 0.5f) - std::floor(w->p.y + 0.5f));
        int n = (int)std::floor(a.animT * sp + 0.5f);   // the whole px walked so far (animT keeps them over the speed)
        if (!w->walking) {   // (P2b) setting off: the gait by the step's own speed, then the body's own walk / run table
          if (dt > 0) animSetOff(a, d / dt);
          n = animWalkBegin(a, n);
        }
        const float before = (float)n / sp;
        a.animT = (float)(n + (int)drawn) / sp;
        // the hero's footstep every 0.29 s of walk clock (updatePlayer's rule at rig 1), counted here where the clock
        // moves: it now moves in whole px (1 or 2 a frame at 74 px/s), so updatePlayer's "animT - dt" window would skip
        // or double a step
        if (a.player && (int)(a.animT / kHeroStepPeriod) != (int)(before / kHeroStepPeriod)) {
          const float pitch = 0.85f + rng_.f() * 0.3f;   // (drawn either way: the RNG stream does not depend on riding)
          if (!homeRiding()) sfx((int)Sfx::Step, a.p, pitch, 0.5f);
        }
      } else a.animT += dt;
      // (P2b FOLK-SIM) the gait (switch at 54 px/s, the drawn speed smoothed), the walk clock re-phased on a switch, and
      // the sheathed / drawn clock (rpg/sim/anim.cpp animStep)
      animStep(*this, a, d >= 0 && d < 32.0f ? d : -1.0f, dt);
    }
  }
}
