// EMBERVALE animation pick (P2a phase A, VISION_PLAN 15.36 item 6; STYLE_P2 10): which clip, family, combo variant and
// frame a human shows this moment. Pure and headless (reads the Game and the Actor, changes nothing), so rpg_test
// --anim checks it without a window. FROZEN after phase A (lanes ADD, never rename or change meaning).
//
//   rig 1 (P2a's shipped default): `frame` is today's v1 sheet column, exactly as render.cpp picked it before P2a
//          (0 idle, 1-4 walk at 9 fps, 5 windup / cast, 6 strike / recover, 7 hurt / dead; the roll runs the walk
//          columns at 20 fps): rpg_test --anim golden (A1)
//   rig 2  ANIM fills it per STYLE_P2 10 (walk by distance, idle phases per actor, windup / strike / follow split by the
//          family's pace, the combo variant, cast, the bow, fall and lying): `frame` indexes the v2 atlas clip
//          (art::RigSheet::frame(facing, clip, variant, frame)) and `frames` is the clip's frame count
// The ANIM lane owns anim.cpp; the view (rpg/view/render_anim.cpp) draws what it picks.
#pragma once
#include <cstdint>
#include "rpg/art/art_fx.h"
#include "rpg/art/art_rig.h"

class Game;
struct Actor;
struct Rng;
enum class Role : uint8_t;   // rpg/sim/world.h

struct AnimPick {
  art::Clip clip = art::Clip::Idle;
  art::Family family = art::Family::Unarmed;
  uint8_t variant = 0;   // the combo hit (0 forehand, 1 backhand, 2 finisher) for Windup / Strike / Follow; else 0
  uint8_t frame = 0;     // rig 1: the v1 sheet column; rig 2: the frame within the clip
  uint8_t frames = 1;    // frames in the clip (rig 1: the v1 columns the clip cycles through)
  float t = 0;           // seconds into the clip (the actor's state time; the walk's distance clock for Walk)
  // (P2b FOLK-SIM, added) rig 2: the atlas part that holds the frame (rig 1: Loco), the gait and the weapon's state.
  //   Loco    idle, walk and hurt with any weapon sheathed (variant 0)
  //   Run     the run cycle (clip Walk) above the gait switch: variant 0 sheathed (the drawn run: DRAWN walk variant 2)
  //   Drawn   (P2c) the drawn idle and walk (clips Idle / Walk, variant 1), the drawn run (Walk variant 2)
  //   Action  the swings, cast, the bow, block and roll
  //   Death   fall and lying
  art::RigPart part = art::RigPart::Loco;
  uint8_t gait = 0;      // 0 walk, 1 run (art::doll::Gait)
  bool drawn = false;    // a fighter's weapon is out (P2c: every weapon class; animDrawn), else it rides as `carry` says
  // ---- (P2c FOLK-SIM-C, production; docs/P2C_CONTRACTS.md 6) the pick pieces the action areas and SWITCH draw on
  uint8_t carry = 0;     // AnimCarry: how what is in hand rides out of a fight (none, sheathed, upright, slung, back, tool)
  bool bash = false;     // Roll: a shield bearer's roll, the shield leading the tuck (Roll variant 1; falls back to 0)
  bool impact = false;   // a heavy finisher's impact: the first kImpactShow s of hit 3's Strike (greatsword, axe, mace)
  uint8_t xbow = 0;      // AnimXbow: the crossbow's state (aim Draw v1, shoot Release v1, crank Draw v2; bows keep v0)
  uint8_t face = 0;      // the facing to draw (Actor::face; a drawn fighter at rest turns to face its threat)
};
// how a weapon rides when it is not drawn (STYLE_P2 25.7; PLAN "Sheaths and back items")
enum class AnimCarry : uint8_t {
  None,       // nothing in hand (unarmed)
  Sheathed,   // sword, dagger: the scabbard; axe, mace: through the belt
  Upright,    // spear, glaive, halberd, staff, the walking stick: carried upright in the right hand
  Slung,      // bow (on the back with the quiver), crossbow (slung)
  Back,       // greatsword: across the back
  Tool,       // a civil trade's tool, ledger, scroll or horn: in hand at rest, never drawn
};
// the crossbow's states (AnimPick::xbow) and where their frames live: aim = Clip::Draw variant 1 (2 frames: raise, sight
// held), shoot = Clip::Release variant 1 (1 frame, 80 ms), crank = Clip::Draw variant 2 (3 frames over the reload)
enum class AnimXbow : uint8_t { None, Aim, Shoot, Crank };
constexpr int kXbowAimVariant = 1, kXbowShootVariant = 1, kXbowCrankVariant = 2;
constexpr int kXbowAimFrames = 2, kXbowCrankFrames = 3;
constexpr float kXbowShoot = 0.08f;      // s the shoot frame shows (the bow's release)
constexpr float kXbowCrankFrame = 0.11f; // s a crank frame shows on the hero's reload (a foe's crank fills its strike + recover)
// (P2c) the block: a 0.15 s visual flash (no sim state but Actor::blockT) on the sword's parry and a shield bearer's
// parry; Clip::Block, 3 frames: raise 30, impact 50, brace 70 ms (the owner, 2026-10-23)
constexpr float kBlockShow = 0.15f;
constexpr float kBlockRaise = 0.03f, kBlockImpact = 0.05f, kBlockBrace = 0.07f;
// (P2c) the heavy finisher's impact window (s from the Strike's start; SWITCH's camera nudge and the impact flash)
constexpr float kImpactShow = 0.07f;
// (P2c) a fighter drawn by a threat turns to face it at rest: after a reaction of kTurnReact (+ up to kTurnSpread its own),
// a quarter turn at once, a half turn through the side facing nearer the threat for kTurnStep
constexpr float kTurnReact = 0.06f, kTurnSpread = 0.08f, kTurnStep = 0.10f;
constexpr float kTurnWin = 0.35f;   // s of the turn window (animStep: a fresh threat sets Actor::drawnT to kTurnBase + kTurnWin,
constexpr float kTurnBase = 1000.0f; // which runs down to kTurnBase and then holds kThreatHold: drawnT > kTurnBase marks it)
// the carry of a look (what is in hand and whether it is a civil trade's)
AnimCarry animCarry(const art::HumanLook& L);
// the look holds a crossbow (weapon 3, bowForm crossbow)
bool animCrossbow(const art::HumanLook& L);
// the roll is a shield bearer's bash (shield worn and not slung: one-hand weapons, unarmed)
bool animBash(const Actor& a);

// what a human shows now. Non-humans (monsters, critters) get the Idle pick; the view keeps their own frame logic.
AnimPick animPick(const Game& g, const Actor& a);
// the same at an explicit rig version (tests compare rig 1 and rig 2 without flipping the global switch)
AnimPick animPick(const Game& g, const Actor& a, int rigVersion);

// ---- timing the view and the sim share (phase A: today's numbers; the ANIM lane makes them per family and per rig and
//      makes game.cpp / ai.cpp read them, so rpg_test --anim measures exactly what plays). FROZEN names.
// the swing effect a human's strike shows now and how far through it is (0..1; -1: no effect this moment). v1: one
// Slash (SlashDown / SlashUp by facing) for every family, at stT / 0.11 through the strike, held at 1 for 0.05 s of
// the recover. ANIM (STYLE_P2 12): the family's Fx (art::Fx::SwingArc...), ending with the strike state within a frame.
art::Fx animSwingFx(const Game& g, const Actor& a);
float animSwingProgress(const Game& g, const Actor& a);
// how long the actor's strike state lasts (s): the hero 0.11 x the family's pace (AnimOps::heroPace); a hostile 0.28
// (0.4 heavy: ai.cpp updateHostile), the folk branch of ai.cpp (guards, militia) 0.25
float animStrikeTime(const Game& g, const Actor& a);
// (ANIM, added) the windup and recover states' lengths the sim runs (s): the hero (0.07 / 0.05 unarmed) and (0.13 /
// 0.08 unarmed / 0.28 the finisher) x the pace; a foe its animEnemyWindup and 0.35 (0.75 heavy); a companion its own
float animWindupTime(const Game& g, const Actor& a);
float animRecoverTime(const Game& g, const Actor& a);
// (ANIM, added) the swing effect of a family (STYLE_P2 12): PunchStar, StabGlint, SwingArc, HeavyArc, ChopArc,
// SmashRing, ThrustStreak, BluntSwish, Twang
art::Fx animFamilyFx(art::Family f);
// an enemy human's windup (s) by what it holds. v1: the same whatever it holds: a bandit's Actor::windup, 0.36 s
// (game.cpp spawnHuman; measured in the sim: 22 frames), 0.30 s for the folk branch of ai.cpp (guards, militia).
// ANIM: by family in the plan's ratios (dagger 0.22, unarmed 0.24, sword 0.28, spear / staff 0.30, axe 0.32, mace 0.36,
// greatsword 0.38), scaled so the spawn-weighted mean over the bandits stays today's 0.36 +- 0.01 (A3: no balance
// change; the plan's "0.30" read ai.cpp's folk branch, the bandits spawn with 0.36)
float animEnemyWindup(const Actor& a);
// how long the dead lie (s) and how many may lie at once (0: no cap). v1 (rig 1): 6 s, no cap (game.cpp). ANIM at
// rig 2: 60 s (fading over the last 5), at most 24, the oldest fading first (A4)
float animCorpseLife(int rigVersion);
int animCorpseCap(int rigVersion);
// (ANIM, added) a body's opacity as it lies (1 alive): v1 fades 4-6 s; a human at rig 2 over the last kCorpseFade s of
// its animCorpseLife (game.cpp starts the oldest ones' fade early when more than the cap lie at once)
constexpr float kCorpseFade = 5.0f;
float animCorpseAlpha(const Actor& a, int rigVersion);
// (ANIM, added) the greatsword's own pace (its blows hit x the same: DPS unchanged)
constexpr float kGreatswordPace = 1.18f;
// (ANIM, added; P2a slice SLICE-WALK-SLIDE) the v2 walk runs on the WHOLE world px a body has moved on the screen:
// game.cpp sums that drawn travel into Actor::animT (animT x speed = px) and animWalkFrame turns it into the frame. The
// six frames change on the px where rig_body.cpp's planted sole has stepped (5 / 4 / 5 px a stance: a 28 px cycle, 15.9
// fps at the hero's 74 px/s, 8.6 at 40), so at every frame change the planted foot is back on the spot it landed on.
// animWalkBegin(px) is the px count a walk starts from: the passing frame, whose planted foot is the idle stance's back
// foot to the pixel (the step off).
constexpr int kWalkCycle = 28;
int animWalkFrame(int px);
int animWalkBegin(int px);

// ---- (P2b FOLK-SIM, added) the doll's walk, run and weapon state at rig 2 (VISION_PLAN 15.36 FOLK-SIM; anim.cpp)
// The run and sheathing are new looks the owner settles at the slice sign-off: each is ONE switch here.
constexpr bool kRunOn = true;        // movers above art::doll::kGaitSwitch (54 px/s) run (false: everyone walks)
constexpr bool kSheatheOn = true;    // sheathable weapons ride sheathed out of a fight (false: always drawn, as v1)
constexpr float kDrawnHold = 4.0f;   // s a weapon stays drawn after the last swing state
constexpr float kThreatHold = 0.6f;  // s it stays drawn after the last hostile leaves the body's aggro radius
constexpr float kGaitBack = 2.0f;    // px/s of hysteresis: a runner walks again below kGaitSwitch - kGaitBack
constexpr float kGaitTau = 0.08f;    // s: the drawn speed's smoothing while moving (a body sets off on its own speed: animSetOff)
// the walk (gait 0) or run (1) table of the actor's body (its BodySet; an elder's own walk when the pool has one), from
// art::doll::walkTable; frames 0 when the pool has no clip (the pick then keeps the 28 px kWalkAt table)
struct WalkTableRef { int frames = 0; float cycle = 0; const float* start = nullptr; int frameAt(float px) const; };
WalkTableRef animWalkTable(const Actor& a, int gait);
// the px count a walk starts from on this body's table: its last frame (the passing frame whose planted foot is the
// idle stance's back foot); animWalkBegin(px) when the body has no table
int animWalkBegin(const Actor& a, int px);
// the gait the actor may take (elders and children always walk; kRunOn)
bool animMayRun(const Actor& a);
// the weapon is drawn now (swing, threat, or kSheatheOn off). (P2c) every weapon class a fighter holds: swords, axes,
// daggers, maces, greatswords, spears, staffs and bows (crossbows too); never a civil trade's tool, the walking stick, the
// ledger, scroll or horn, nor bare fists
bool animDrawn(const Actor& a);
// (P2c) the look's weapon may be drawn at all (animDrawn's class rule)
bool animDrawable(const art::HumanLook& L);
// rig 2, once per sim step after the body moved (Game::update): moved = the world px it travelled this step. Smooths the
// drawn speed (Actor::moveV), picks the gait with the switch at 54 px/s (the walk clock re-phased onto the new table, so
// the feet never jump a stride), and runs the drawn clock (Actor::drawnT: kDrawnHold after a swing state, kThreatHold
// while a hostile is within the body's aggro radius)
void animStep(const Game& g, Actor& a, float moved, float dt);
// (P2b slice fix, SLICE-RUN-INGAME) rig 2, on the step a body sets off (Game::update, before its walk clock starts):
// the drawn speed is the step's own (v px/s) and the gait follows it at once, so a body that sets off at a run starts on
// the run's drawings and table instead of six frames of walk and a mid-stride switch
void animSetOff(Actor& a, float v);

// the sim's internals the animation needs (Game befriends it; the ANIM lane ADDs here)
struct AnimOps {
  static void refreshHero(Game& g);        // the hero's look rebuilt (after Appearance::female or the gear changed: `body`)
  static float heroPace(const Game& g);    // the hero's family pace (SkillOps::swingTime: sword 1.0, mace 1.25, dagger 0.8;
                                           // ANIM: x kGreatswordPace with a greatsword in hand)
  static bool heroGreatsword(const Game& g);   // (ANIM, added) the hero's weapon is a greatsword
  // (tests: rpg_test --anim) a bandit at p (weapon: an art id for its look, -1 its own), its actor index; damage an actor
  static int spawnBandit(Game& g, Vec2 p, int weapon);
  static void damage(Game& g, int actorIndex, float dmg);
  // (P2c FOLK-SIM-C, tests: rpg_test --folk-sim, --ladder-pace) Game::makeLook as the spawners call it
  static void makeLook(Game& g, Actor& a, Role r, Rng& rr);
};
