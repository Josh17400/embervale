// EMBERVALE animation pick (anim.h). P2a phase A (lead): rig 1 reproduces render.cpp's pre-P2a frame pick exactly.
// P2a ANIM lane: the rig 2 side per STYLE_P2 10 (walk by distance, idle phases per actor, windup / strike / follow split
// by the family's pace, the combo variant, cast, the bow, fall and lying) and the timing the sim and the view share
// (the family effects, the enemy windups by family, the dead's life and cap).
#include "rpg/sim/anim.h"
#include <algorithm>
#include <cmath>
#include "rpg/art/doll/doll.h"
#include "rpg/art/doll/doll_internal.h"   // (P2b) the elder's own walk clip (pool::clipsOf on "man.elder")
#include "rpg/sim/game.h"
#include "rpg/sim/skill_ops.h"

namespace {

// the v1 standing-sheet pick (render.cpp before P2a, verbatim in its numbers): column and the clip it belongs to
AnimPick pickV1(const Actor& a) {
  AnimPick k;
  k.family = art::familyOf(a.look);
  switch (a.st) {
    case AState::Walk: k.clip = art::Clip::Walk; k.frame = (uint8_t)(1 + (int)(a.animT * 9) % 4); k.frames = 4; k.t = a.animT; break;
    case AState::Roll: k.clip = art::Clip::Roll; k.frame = (uint8_t)(1 + (int)(a.stT * 20) % 4); k.frames = 4; k.t = a.stT; break;
    case AState::Windup: k.clip = art::Clip::Windup; k.frame = 5; k.t = a.stT; break;
    case AState::Cast: k.clip = art::Clip::Cast; k.frame = 5; k.t = a.stT; break;
    case AState::Strike: k.clip = art::Clip::Strike; k.frame = 6; k.t = a.stT; break;
    case AState::Recover: k.clip = art::Clip::Follow; k.frame = 6; k.t = a.stT; break;
    case AState::Hurt: k.clip = art::Clip::Hurt; k.frame = 7; k.t = a.stT; break;
    case AState::Dead: k.clip = art::Clip::Lying; k.frame = 7; k.t = a.stT; break;
    default: k.clip = art::Clip::Idle; k.frame = 0; k.t = a.stT; break;   // Idle, Down
  }
  if (k.clip == art::Clip::Windup || k.clip == art::Clip::Strike || k.clip == art::Clip::Follow)
    k.variant = (uint8_t)std::clamp(a.combo, 0, art::RIG_VARIANTS - 1);
  return k;
}

// a per-actor number in [0, 1) (phases, so a crowd never breathes or blinks as one)
float own(const Actor& a, uint32_t salt) {
  uint32_t h = (uint32_t)a.id * 2654435761u ^ salt * 0x9E3779B9u;
  h ^= h >> 15; h *= 0x2C1B3C6Du; h ^= h >> 12; h *= 0x297A2D39u; h ^= h >> 15;
  return (float)(h & 0xFFFFu) / 65536.0f;
}

// the plan's enemy windups by family (s; VISION_PLAN 15.36 ANIM), in the ratio they keep to each other; the bow keeps
// the actor's own windup
constexpr float kPlanWindup[art::FAMILY_COUNT] = {0.24f, 0.22f, 0.28f, 0.38f, 0.32f, 0.36f, 0.30f, 0.30f, 0.0f};
// the bandits' spawn mix (looks.cpp: a sword or an axe, one in two) puts the plan's mean at 0.30: a body's own windup
// (a bandit's 0.36, a story chief's 0.42...) is scaled by plan / 0.30, so the spawn-weighted mean stays what it was (A3)
constexpr float kPlanMean = 0.30f;

}  // namespace

// ---- the hero's pace and the durations of the swing states (what game.cpp / ai.cpp run on)
float AnimOps::heroPace(const Game& g) {
  // (P2a depth) the greatsword swings at its own pace 1.18 (its blows hit x 1.18 in game.cpp meleeHit: DPS unchanged)
  float t = SkillOps::swingTime(g);
  if (heroGreatsword(g)) t *= kGreatswordPace;
  return t;
}
bool AnimOps::heroGreatsword(const Game& g) {
  return g.eqWeapon >= 0 && g.eqWeapon < (int)g.inv.size() && g.inv[(size_t)g.eqWeapon].kind == ItemKind::Weapon &&
         g.inv[(size_t)g.eqWeapon].sub == (uint8_t)WeaponType::Greatsword;
}

float animWindupTime(const Game& g, const Actor& a) {
  if (a.player) return (g.eqWeapon < 0 ? 0.05f : 0.07f) * AnimOps::heroPace(g);
  if (a.comp) return a.windup;
  if (a.hostile) return animEnemyWindup(a);
  return 0.30f;   // ai.cpp's folk branch: guards, militia
}
float animStrikeTime(const Game& g, const Actor& a) {
  if (a.player) return 0.11f * AnimOps::heroPace(g);
  if (a.hostile) return a.heavy ? 0.4f : 0.28f;   // ai.cpp updateHostile
  return 0.25f;                                    // ai.cpp's folk branch
}
float animRecoverTime(const Game& g, const Actor& a) {
  if (a.player) return (a.combo == 2 ? 0.28f : g.eqWeapon < 0 ? 0.08f : 0.13f) * AnimOps::heroPace(g);
  if (a.hostile) return a.heavy ? 0.75f : 0.35f;
  return 0.30f;
}

// ---- the pick
namespace {

// a frame by the share of the state gone: the first frame until `split` of the duration, then the second
uint8_t twoFrames(float t, float dur, float split) { return (uint8_t)(t < split * std::max(1e-3f, dur) ? 0 : 1); }

bool sheathable(const Actor& a) {   // the resolver's sheathed kinds (doll_resolve.cpp needsOf: sword, axe, dagger, mace)
  const uint8_t w = a.look.weapon;
  return w == 1 || w == 2 || w == 5 || w == 6;
}

AnimPick pickV2(const Game& g, const Actor& a) {
  AnimPick k;
  k.family = art::familyOf(a.look);
  const bool bow = k.family == art::Family::Bow;
  k.t = a.stT;
  k.frames = 1;
  k.drawn = animDrawn(a);
  k.part = art::RigPart::Action;   // (P2b) every clip but the sheathed idle / walk / hurt, the run and the fall
  auto swingVariant = [&](bool afterStrike) -> uint8_t {
    if (a.player) return (uint8_t)std::clamp(a.combo, 0, art::RIG_VARIANTS - 1);
    // foes never combo: the blows go round the three variants (ai.cpp counts them as the windup ends), folk by person
    if (a.hostile) return (uint8_t)(((a.atkN - (afterStrike ? 1 : 0)) % 3 + 3) % 3);
    return (uint8_t)(a.id % 3);
  };
  switch (a.st) {
    case AState::Walk: {
      // (P2a slice, SLICE-WALK-SLIDE) by the whole px walked on the screen (game.cpp sums the drawn travel into animT
      // over the body's own speed): the frame changes on the very px where the art's planted sole has stepped, so the
      // foot lands back where it stood at any pace (round(): animT x speed is a whole number but for float rounding)
      const float dist = a.animT * std::max(1.0f, a.speed);
      k.clip = art::Clip::Walk; k.frames = 6; k.t = dist;
      const int px = (int)std::floor(dist + 0.5f);
      // (P2b) the body's own walk or run table (the doll's clips: man 19, woman 20 px a cycle, the run about 36); the
      // run is the RUN part (variant 1 drawn), the drawn walk the ACTION part's variant 1, the sheathed walk LOCO's 0
      k.gait = (uint8_t)(a.gait && animMayRun(a) ? 1 : 0);
      const WalkTableRef T = animWalkTable(a, k.gait);
      if (T.frames > 0) { k.frames = (uint8_t)T.frames; k.frame = (uint8_t)T.frameAt((float)px); }
      else k.frame = (uint8_t)animWalkFrame(px);
      if (k.gait) { k.part = art::RigPart::Run; k.variant = (uint8_t)(k.drawn ? 1 : 0); }
      else { k.part = k.drawn ? art::RigPart::Action : art::RigPart::Loco; k.variant = (uint8_t)(k.drawn ? 1 : 0); }
      break;
    }
    case AState::Roll:   // crouch, tuck, over, rise: 0.085 s each over the sim's 0.34 s roll
      k.clip = art::Clip::Roll; k.frames = 4; k.frame = (uint8_t)std::clamp((int)(a.stT / 0.085f), 0, 3);
      break;
    case AState::Windup: {
      const float dur = animWindupTime(g, a);
      if (bow) {   // nock, chest, cheek over the draw
        k.clip = art::Clip::Draw; k.frames = 3; k.frame = (uint8_t)std::clamp((int)(a.stT / std::max(1e-3f, dur) * 3.0f), 0, 2);
      } else {
        k.clip = art::Clip::Windup; k.frames = 2; k.frame = twoFrames(a.stT, dur, 0.45f); k.variant = swingVariant(false);
      }
      break;
    }
    case AState::Strike:
      if (bow) {   // the release shows 0.08 s, then the bow comes down
        if (a.stT < 0.08f) { k.clip = art::Clip::Release; k.frames = 1; }
        else { k.clip = art::Clip::Idle; k.frames = 3; }
      } else {
        k.clip = art::Clip::Strike; k.frames = 2; k.frame = twoFrames(a.stT, animStrikeTime(g, a), 0.40f); k.variant = swingVariant(true);
      }
      break;
    case AState::Recover:
      if (bow) { k.clip = art::Clip::Idle; k.frames = 3; }
      else { k.clip = art::Clip::Follow; k.frames = 2; k.frame = twoFrames(a.stT, animRecoverTime(g, a), 0.50f); k.variant = swingVariant(true); }
      break;
    case AState::Cast: {
      if (a.player && bow) {   // the hero's shot leaves at once (shootArrow): the release, then nocking the next
        if (a.stT < 0.08f) { k.clip = art::Clip::Release; k.frames = 1; }
        else { k.clip = art::Clip::Draw; k.frames = 3; k.frame = 0; }
        break;
      }
      k.clip = art::Clip::Cast; k.frames = 2;
      // gather while a spell winds up or is held (MagicOps keeps the Cast state fresh), release after
      const bool gathering = a.player ? g.arcana.live.cast.spell != 0 : a.stT < 0.11f;
      k.frame = (uint8_t)(gathering ? 0 : 1);
      break;
    }
    case AState::Hurt: k.clip = art::Clip::Hurt; k.part = art::RigPart::Loco; break;
    case AState::Dead: {
      k.part = art::RigPart::Death;
      // the fall (recoil 0.10, buckle 0.12, topple 0.12 s from the killing blow), then lying, the head toward the fall
      // side: the way the blow threw the body (its knock is frozen at death), else away from where it faced
      uint8_t side;
      if (std::fabs(a.knock.x) > 1.0f) side = a.knock.x > 0 ? 1 : 0;
      else if (a.face == 2) side = 0;
      else if (a.face == 3) side = 1;
      else side = (uint8_t)(a.id & 1);
      k.variant = side;
      if (a.stT < 0.34f) {
        k.clip = art::Clip::Fall; k.frames = 3;
        k.frame = (uint8_t)(a.stT < 0.10f ? 0 : a.stT < 0.22f ? 1 : 2);
      } else {
        k.clip = art::Clip::Lying; k.frames = 1; k.t = a.stT - 0.34f;
      }
      break;
    }
    default: {   // Idle (and a skeleton's Down): neutral and breathe alternate 0.55 s; a 0.12 s blink every 3-6 s
      k.clip = art::Clip::Idle; k.frames = 3;
      const float t = a.stT + own(a, 1) * 1.1f;
      k.frame = (uint8_t)((int)(t / 0.55f) & 1);
      const float period = 3.0f + 3.0f * own(a, 2);
      if (std::fmod(a.stT + own(a, 3) * period, period) < 0.12f) k.frame = 2;
      // (P2b) the drawn idle is the ACTION part's variant 1; sheathed (or nothing to sheathe), LOCO's variant 0
      k.part = k.drawn ? art::RigPart::Action : art::RigPart::Loco;
      k.variant = (uint8_t)(k.drawn ? 1 : 0);
      break;
    }
  }
  return k;
}

}  // namespace

// ---- (P2a slice, SLICE-WALK-SLIDE) the walk's frame by the whole px walked
namespace {
// where each of the six frames begins in the 28 px cycle: rig_body.cpp's walk table moves the planted sole 5, 4 and 5 px
// between frames (the near foot +7 / +2 / -2 / -7 under the hips, then the far foot the same), so each frame holds
// exactly the px its planted foot steps: contact 5, down 4, pass 5, contact 5, down 4, pass 5
constexpr int kWalkAt[6] = {0, 5, 9, 14, 19, 23};
int walkPhase(int px) { return ((px % kWalkCycle) + kWalkCycle) % kWalkCycle; }
}  // namespace
int animWalkFrame(int px) {
  const int c = walkPhase(px);
  int f = 5;
  while (f > 0 && c < kWalkAt[f]) f--;
  return f;
}
int animWalkBegin(int px) { return px - walkPhase(px) + kWalkAt[5]; }

// ---- (P2b FOLK-SIM) the doll's walk and run tables, the gait switch and the sheathed / drawn pick
namespace {
// the six bodies a table serves: the four body sets, then the man's and the woman's elder sets
enum TableBody { TB_MAN, TB_WOMAN, TB_YOUTH, TB_CHILD, TB_MAN_ELDER, TB_WOMAN_ELDER, TB_COUNT };
int tableBody(const art::HumanLook& L) {
  const art::doll::BodySet b = art::doll::bodySetOf(L);
  if (L.age == 3 && b == art::doll::BodySet::Man) return TB_MAN_ELDER;
  if (L.age == 3 && b == art::doll::BodySet::Woman) return TB_WOMAN_ELDER;
  return (int)b;
}
// an elder's walk from the "<body>.elder" clip set when the pool draws one (its own plants: the stoop's shorter step);
// else the adult's table (walkTable reads the base sets only). The same arithmetic as art::doll::walkTable.
art::doll::WalkTable elderWalk(art::doll::BodySet base) {
  using namespace art::doll::pool;
  const art::doll::WalkTable adult = art::doll::walkTable(base, art::doll::Gait::Walk, 2);
  const Pool& P = get();
  const std::string key = std::string(art::doll::bodySetName(base)) + ".elder";
  const uint16_t id = P.strId(key);
  if (id == 0xFFFF) return adult;
  const ClipRec* c = nullptr;
  for (const ClipRec* x : clipsOf(key, (uint8_t)art::RigPart::Loco, -1, -1))
    if (x->body == id && x->clip == (uint8_t)art::Clip::Walk && x->facing == 2) c = x;
  if (!c || !c->nFrames) return adult;
  art::doll::WalkTable t;
  t.frames = std::min<int>(c->nFrames, 16);
  t.cycle = c->cycle16 ? c->cycle16 / 16.0f : adult.cycle;
  float adv[16] = {}, total = 0;
  bool planted = true;
  for (int i = 0; i < t.frames; i++) {
    const FrameRec& f = P.frames[c->firstFrame + (uint32_t)i];
    if (f.plant0 == kNoPlant || f.plant1 == kNoPlant) { planted = false; break; }
    adv[i] = std::max(0.0f, (f.plant0 - f.plant1) / 16.0f);
    total += adv[i];
  }
  if (!planted || total <= 0) {
    for (int i = 0; i < t.frames; i++) t.start[i] = t.cycle * (float)i / (float)t.frames;
    return t;
  }
  t.cycle = total;
  float acc = 0;
  for (int i = 0; i < t.frames; i++) { t.start[i] = acc; acc += adv[i]; }
  return t;
}
struct Tables {
  uint32_t gen = 0xFFFFFFFFu;
  art::doll::WalkTable t[TB_COUNT][2];
};
const art::doll::WalkTable& tableOf(int body, int gait) {
  thread_local Tables T;   // (one Game steps per thread: rpg_test runs seeds in parallel)
  const uint32_t gen = art::doll::poolGeneration();
  if (T.gen != gen) {
    T.gen = gen;
    for (int b = 0; b < TB_COUNT; b++)
      for (int g = 0; g < 2; g++) {
        art::doll::WalkTable& w = T.t[b][g];
        w = art::doll::WalkTable();
        if (b == TB_MAN_ELDER || b == TB_WOMAN_ELDER) {
          if (g == 0) w = elderWalk(b == TB_MAN_ELDER ? art::doll::BodySet::Man : art::doll::BodySet::Woman);
        } else if (g == 0 || (b != TB_CHILD)) {
          w = art::doll::walkTable((art::doll::BodySet)b, g ? art::doll::Gait::Run : art::doll::Gait::Walk, 2);
        }
        if (w.frames <= 0 || w.cycle <= 0.5f) w.frames = 0;   // no clip: the caller keeps its own table
      }
  }
  return T.t[std::clamp(body, 0, TB_COUNT - 1)][gait ? 1 : 0];
}
bool threatened(const Game& g, const Actor& a) {
  const float r = std::max(0.0f, a.aggroR), r2 = r * r;
  for (const Actor& b : g.actors) {
    if (b.id == a.id || b.st == AState::Dead || b.indoors) continue;
    if (!factionsHostile(a.faction, b.faction) && !(a.player && b.hostile)) continue;
    if (len2(b.p - a.p) <= r2) return true;
  }
  return false;
}
}  // namespace

int WalkTableRef::frameAt(float px) const {
  if (frames <= 0 || cycle <= 0 || !start) return 0;
  float d = px - cycle * std::floor(px / cycle);
  if (d < 0) d += cycle;
  int f = 0;
  while (f + 1 < frames && d >= start[f + 1]) f++;
  return f;
}
WalkTableRef animWalkTable(const Actor& a, int gait) {
  const art::doll::WalkTable& w = tableOf(tableBody(a.look), gait && animMayRun(a) ? 1 : 0);
  WalkTableRef r;
  if (w.frames > 0) { r.frames = w.frames; r.cycle = w.cycle; r.start = w.start; }
  return r;
}
int animWalkBegin(const Actor& a, int px) {
  const WalkTableRef T = animWalkTable(a, a.gait);
  if (T.frames <= 0 || art::rigVersion() < 2) return animWalkBegin(px);
  // the last frame's first whole px in the cycle that holds px
  const float base = std::floor((float)px / T.cycle) * T.cycle;
  return (int)std::ceil(base + T.start[T.frames - 1] - 1e-3f);
}
bool animMayRun(const Actor& a) {
  if (!kRunOn) return false;
  const art::doll::BodySet b = art::doll::bodySetOf(a.look);
  return b != art::doll::BodySet::Child && a.look.age != 3;   // elders never run; children run in the Play posture
}
bool animDrawn(const Actor& a) {
  if (!sheathable(a)) return false;
  if (!kSheatheOn) return true;
  return a.drawnT > 0 || a.st == AState::Windup || a.st == AState::Strike || a.st == AState::Recover;
}
void animSetOff(Actor& a, float v) {
  if (v < 0) return;
  a.moveV = v;
  a.gait = (uint8_t)(animMayRun(a) && v > art::doll::kGaitSwitch ? 1 : 0);
}
void animStep(const Game& g, Actor& a, float moved, float dt) {
  if (dt <= 0) return;
  // the drawn speed, smoothed (a window shift or a teleport, moved < 0, holds it)
  if (moved >= 0) {
    const float v = moved / dt;
    const float k = dt / (kGaitTau + dt);   // (a first-order lag; no libm call: the same on MSVC and clang)
    a.moveV += (v - a.moveV) * k;
  }
  // the gait, while walking (the switch at 54 px/s; back to the walk kGaitBack under it)
  if (a.st == AState::Walk) {
    const int want = !animMayRun(a) ? 0 : a.gait ? (a.moveV >= art::doll::kGaitSwitch - kGaitBack ? 1 : 0) : (a.moveV > art::doll::kGaitSwitch ? 1 : 0);
    if (want != (int)a.gait) {
      // re-phase the walk clock onto the new table: the same share of the cycle (frame i of the walk becomes frame i of the
      // run), kept within a cycle of the px walked so far (the footstep clock barely moves)
      const WalkTableRef from = animWalkTable(a, a.gait), to = animWalkTable(a, want);
      const float sp = std::max(1.0f, a.speed);
      if (from.frames > 0 && to.frames > 0) {
        const float n = std::floor(a.animT * sp + 0.5f);
        const float share = (n - std::floor(n / from.cycle) * from.cycle) / from.cycle;   // 0..1 into the cycle
        const float m = std::floor(std::floor(n / to.cycle) * to.cycle + share * to.cycle + 0.5f);   // whole px
        a.animT = m / sp;
      }
      a.gait = (uint8_t)want;
    }
  } else if (a.st != AState::Hurt) a.gait = 0;   // a stand, a swing or a fall ends the run (a stagger keeps it)
  // the drawn clock: a swing holds it kDrawnHold s, a hostile in reach kThreatHold s; then it runs down
  if (sheathable(a)) {
    if (a.st == AState::Windup || a.st == AState::Strike || a.st == AState::Recover) a.drawnT = kDrawnHold;
    else {
      a.drawnT = std::max(0.0f, a.drawnT - dt);
      if (a.drawnT < kThreatHold && threatened(g, a)) a.drawnT = kThreatHold;
    }
  } else a.drawnT = 0;
}

AnimPick animPick(const Game& g, const Actor& a, int rigVersion) {
  if (!a.human && !a.player) return AnimPick();
  if (rigVersion < 2) return pickV1(a);
  return pickV2(g, a);
}

AnimPick animPick(const Game& g, const Actor& a) { return animPick(g, a, art::rigVersion()); }

void AnimOps::refreshHero(Game& g) { g.recalcPlayer(); }
int AnimOps::spawnBandit(Game& g, Vec2 p, int weapon) {
  Spawn sp;
  sp.bandit = true;
  sp.role = Role::Bandit;
  sp.site = g.world.startSite;
  sp.slot = 9000 + (int)g.actors.size();
  const int id = g.spawnHuman(sp, p);
  for (size_t i = 0; i < g.actors.size(); i++)
    if (g.actors[i].id == id) {
      if (weapon >= 0) { g.actors[i].look.weapon = (uint8_t)weapon; g.actors[i].ranged = weapon == 3; }
      return (int)i;
    }
  return -1;
}
void AnimOps::damage(Game& g, int actorIndex, float dmg) {
  if (actorIndex < 0 || actorIndex >= (int)g.actors.size()) return;
  g.damage(g.actors[(size_t)actorIndex], dmg, g.actors[(size_t)actorIndex].p + Vec2(-8, 0), 0);
}

// ---- the family effects (STYLE_P2 12)
art::Fx animFamilyFx(art::Family f) {
  switch (f) {
    case art::Family::Dagger: return art::Fx::StabGlint;
    case art::Family::Sword: return art::Fx::SwingArc;
    case art::Family::Greatsword: return art::Fx::HeavyArc;
    case art::Family::Axe: return art::Fx::ChopArc;
    case art::Family::Mace: return art::Fx::SmashRing;
    case art::Family::Spear: return art::Fx::ThrustStreak;
    case art::Family::Staff: return art::Fx::BluntSwish;
    case art::Family::Bow: return art::Fx::Twang;
    default: return art::Fx::PunchStar;
  }
}
art::Fx animSwingFx(const Game& g, const Actor& a) {
  (void)g;
  return animFamilyFx(art::familyOf(a.look));
}
float animSwingProgress(const Game& g, const Actor& a) {
  if (!(a.player || a.human)) return -1;
  const bool bow = art::familyOf(a.look) == art::Family::Bow;
  if (bow) {   // the Twang at the bow as the arrow leaves (a foe's strike; the hero's shot is its Cast)
    const bool loosed = a.st == AState::Strike || (a.player && a.st == AState::Cast);
    return loosed && a.stT < 0.16f ? a.stT / 0.16f : -1.0f;
  }
  // the effect runs over the strike state and ends with it (the family's pace: a dagger quick, a mace slow)
  if (a.st == AState::Strike) return std::min(1.0f, a.stT / std::max(1e-3f, animStrikeTime(g, a)));
  return -1;
}
float animEnemyWindup(const Actor& a) {
  // a hostile human's windup by what it holds, its own (Actor::windup: a bandit's 0.36) scaled by the family's share of
  // the plan's mean; the bow and every beast keep their own; ai.cpp's folk branch (guards, militia) swings at 0.30
  if (!a.hostile) return 0.30f;
  const float base = a.windup > 0 ? a.windup : 0.30f;
  if (!a.human || a.ranged) return base;
  const art::Family f = art::familyOf(a.look);
  if (f == art::Family::Bow) return base;
  return base * kPlanWindup[(int)f] / kPlanMean;
}
float animCorpseLife(int rigVersion) { return rigVersion >= 2 ? 60.0f : 6.0f; }
int animCorpseCap(int rigVersion) { return rigVersion >= 2 ? 24 : 0; }
float animCorpseAlpha(const Actor& a, int rigVersion) {
  if (a.st != AState::Dead) return 1.0f;
  if (rigVersion < 2 || !a.human) return std::clamp(1.0f - (a.stT - 4.0f) / 2.0f, 0.0f, 1.0f);   // v1: fades 4-6 s
  return std::clamp((animCorpseLife(rigVersion) - a.stT) / kCorpseFade, 0.0f, 1.0f);              // the last 5 s of 60
}
