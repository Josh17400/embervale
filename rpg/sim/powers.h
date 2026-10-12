// M8b Treasure (VISION_PLAN 15.27, 15.28): what affixes and unique / Mythic powers DO in play. Contract written by the
// M8b lead (phase A, 2026-10-12); the POWERS lane is its only editor (it may ADD; it never renames, removes or changes
// the meaning of what is listed here) and implements it in rpg/sim/powers.cpp (+ the combat code in game.cpp,
// player.cpp, the companions' party_game.cpp / party_ai.cpp, and the view of the effects in rpg/view/render_powers.cpp).
//
// The rule the whole milestone keeps (owner): "no item above Common may roll without at least one working, visible
// affix". An affix or power rolls only when affixImplemented / uniqueImplemented (gear.h) say it works, and those are
// defined HERE, by the lane that makes them work, so the generator can never hand out a power that does nothing.
#pragma once
#include <cstdint>
#include <vector>
#include "rpg/sim/items.h"

namespace powers {

// does this affix / power change a number in combat or the sim today (the generator only rolls working ones)
bool affixWorks(Affix a);
bool uniqueWorks(Unique u);

// the runtime state of the worn powers (cooldowns, trails, summons). It lives in gear::Live::fx (gear.h), which lives
// in craft::Knowledge::live: never saved (timers restart on a load, cooldowns ready). The POWERS lane adds fields.
struct State {
  float trailT = 0;        // Inferno: seconds to the next fire patch
  float auraT = 0;         // BrambleCrown: seconds to the next aura tick
  float regenT = 0;        // Affix::HealthRegen: seconds to the next heal
  float slowT = 0;         // Chronos (and TimeSlow): time slowed left (seconds)
  float chronosCd = 0;     // Chronos: cooldown
  float phoenixCd = 0;     // Phoenix: cooldown (180 s)
  float dodgeT = 0;        // a roll's perfect-dodge window left
  int hits = 0;            // landed hits (Tempest, Stormcaller...)
  uint32_t summonId = 0;   // Wolfking: the spectral wolf's actor id (0 none)
  float summonT = 0;       // its time left
  // ---- POWERS lane (phase B) additions
  struct Patch { Vec2 p; float t = 0, life = 3.0f; uint32_t seed = 0; };   // Inferno / FireTrail: fire on the ground
  std::vector<Patch> patches;       // (map pixels; translated with the window, cleared with the map)
  Vec2 lastPatch;                   // where the last patch was laid (a new one every PATCH_STEP px walked)
  struct Star { Vec2 at; float t = 0; float dmg = 0; int by = 0; };     // Starfall: a star on its way down
  std::vector<Star> stars;
  struct Call { Vec2 at; float life = 12.0f; int by = 0; };             // Wolfking / SpectralWolf: a wolf to call
  std::vector<Call> calls;          // (queued: actors are never added inside damage() / kill())
  float starCd = 0;        // Starfall: at most one star in STAR_GAP s
  float tempestCd = 0;     // Tempest: one chain a swing (the cone's other foes do not chain again)
  float unseenT = 0;       // Shadowstep: foes cannot see the hero
  float shadowCd = 0;      // Shadowstep: cooldown
  float lastHealFx = 0;    // the last lifesteal mote (throttled for the view)
  // what fired (tests, reports): procs by Fx kind, and a few measured totals
  int procs[16] = {};
  int chained = 0;         // foes struck by a Tempest / Chain Lightning arc
  int starHits = 0;        // foes struck by a falling star
  int wolvesCalled = 0;    // spectral wolves summoned
  int rebirths = 0;        // Phoenix saves
  float healed = 0;        // health healed by Soulfeast / HealthRegen / the kill heals
  float auraDealt = 0;     // BrambleCrown's damage
  float slowedSecs = 0;    // seconds of Chronos' time slow
};

// ---- tuning (POWERS lane). Every number an effect uses lives here, so the tests and the words agree.
constexpr float TEMPEST_FRAC = 0.35f;    // an arc's damage: this share of the wearer's weapon damage
constexpr int TEMPEST_ARCS = 3;          // foes a Tempest chain reaches
constexpr float TEMPEST_REACH = 72.0f;   // px from one link to the next
constexpr float SOULFEAST_PCT = 6.0f;    // % of the damage dealt healed
constexpr float SOULFEAST_KILL_PCT = 5.0f;   // % of max health a kill heals
constexpr float BRAMBLE_REACH = 24.0f;   // px (1.5 tiles)
constexpr float BRAMBLE_FRAC = 0.30f;    // the aura's damage a second: this share of the weapon damage
constexpr float CHRONOS_SECS = 3.0f;     // time slowed
constexpr float CHRONOS_SCALE = 0.35f;   // foes and their missiles run at this speed
constexpr float CHRONOS_WINDOW = 0.2f;   // the perfect-dodge window at a roll's start (s)
constexpr float CHRONOS_CD = 10.0f;
constexpr float PHOENIX_CD = 180.0f;
constexpr float PHOENIX_HP = 0.30f;      // health a rebirth leaves
constexpr float PHOENIX_REACH = 44.0f;   // the burst's reach (px)
constexpr float STAR_FRAC = 0.8f;        // a falling star: this share of the weapon damage to every foe round the mark
constexpr float STAR_REACH = 28.0f;
constexpr float STAR_DELAY = 0.28f;      // the star's fall
constexpr float STAR_GAP = 0.6f;
constexpr float WOLF_SECS = 12.0f;       // Wolfking's wolf (SpectralWolf's: 10)
constexpr float SPECTRAL_SECS = 10.0f;
constexpr int WOLF_CAP = 2;              // spectral wolves at once per master (another kill renews the oldest)
constexpr float PATCH_STEP = 10.0f;      // px walked between two Inferno patches
constexpr float PATCH_LIFE = 3.0f;
constexpr float PATCH_R = 10.0f;         // a patch's reach
constexpr int PATCH_CAP = 14;
constexpr float SHADOW_SECS = 1.5f, SHADOW_CD = 6.0f;
constexpr float SPLIT_RANGE = 96.0f;     // SplitArrows: past 6 tiles an arrow splits in three
constexpr float REGEN_EVERY = 5.0f;      // Affix::HealthRegen: N health every 5 s
constexpr float BURN_SECS = 3.0f;        // Affix::Burn: N damage a second for 3 s
// Actor::powTag values (rpg/sim/game.h)
constexpr uint8_t PT_WOLF = 1;           // a summoned spectral wolf (powT its time left, powBy its master's id)

// Ev::Power (rpg/sim/game.h): what the view draws. Event::a = the Fx kind, p = where (from), q = to (a lightning arc's
// end, a star's landing), f = magnitude (0..1+), id = the actor it follows (0 none). Drawn by View::powerEvent /
// drawPowerFx (rpg/view/render_powers.cpp, the POWERS lane).
enum class Fx : uint8_t { Lightning, FirePatch, Burn, Thorns, TimeSlow, Lifesteal, Phoenix, Star, Wolf, Proc, COUNT };

}  // namespace powers
