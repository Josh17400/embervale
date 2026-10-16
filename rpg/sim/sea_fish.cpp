// M10 "Beyond the Mist": fishing (VISION_PLAN 15.4; rpg/sim/sea_ops.h). SEA lane.
//
// Cast from a shore, a pier or a boat facing water (the use button: interactLabel "FISH"). The float flies out
// (Casting), settles (Waiting) until something bites (Bite: a short window to STRIKE), then the fight (Reeling): hold
// the reel (Input::reel, the HUD's held button) to bring the fish in while the line's tension climbs; let go when the
// fish runs or the line snaps at tension 1; slack too long and it slips the hook. Landed or lost (SeaEv), the catch
// goes into the pack as food (Ingr::Fish for cooking, a fishmonger buys it) and into the catch book (Sea::catches).
// Species come from the water (lake, river, coast, deep, the Mist), the climate, the season and the hour; rare fish
// and one legendary per water (a name and a tale, caught once).
#include "rpg/sim/sea_ops.h"

#include "engine/audio.h"

#include <algorithm>
#include <cmath>
#include "rpg/sim/game_internal.h"
#include "rpg/world/source.h"

using namespace sea;
using gsim::faceOf;

namespace {
bool waterTile(const Map& m, int tx, int ty) { return m.in(tx, ty) && groundWater(m.at(tx, ty)) && !m.solid[(size_t)ty * m.w + tx] && m.propAt(tx, ty) == 0; }
}  // namespace

uint8_t SeaOps::watersAt(const Game& g, int tx, int ty) {
  const Map& m = g.world.over;
  if (!m.in(tx, ty) || !groundWater(m.at(tx, ty))) return 0;
  uint8_t w = 0;
  const int32_t gx = g.world.ox + tx, gy = g.world.oy + ty;
  if (m.at(tx, ty) == Ground::DeepWater) w = FW_DEEP;
  else if (g.world.src && g.world.endless && g.world.src->macro(gx, gy).eco == Eco::Ocean) w = FW_COAST;
  else {
    int n = 0;   // a lake is wide water, a river a ribbon of it
    for (int dy = -4; dy <= 4; dy++)
      for (int dx = -4; dx <= 4; dx++) if (m.in(tx + dx, ty + dy) && groundWater(m.at(tx + dx, ty + dy))) n++;
    w = n >= 50 ? FW_LAKE : FW_RIVER;
  }
  if (g.world.src && g.world.endless && (g.world.src->mistAt(gx, gy) > 2000 || g.world.src->hiddenLandAt(gx, gy))) w |= FW_MIST;
  return w;
}

bool SeaOps::fishSpot(const Game& g, Vec2& bob, uint8_t& water) {
  water = 0;
  if (g.inside) return false;
  const Map& m = g.world.over;
  const Actor& p = g.pl();
  if (g.sea.atSea()) {
    // from a boat: over the rail on the side she faces, else either beam
    // the side she faces first, then her beams, then anywhere round the hull; a little farther out, then close in
    const float hr = hullInfo(g.sea.boats[(size_t)g.sea.aboard].hull).radius;
    const float a0 = std::atan2(p.aim.y, p.aim.x);
    static const float kOff[8] = {0.0f, 1.5708f, -1.5708f, 0.785f, -0.785f, 2.356f, -2.356f, 3.1416f};
    for (float r : {hr + 22.0f, hr + 12.0f})
      for (float o : kOff) {
        const Vec2 c = p.p + Vec2(std::cos(a0 + o), std::sin(a0 + o)) * r;
        if (waterTile(m, (int)std::floor(c.x / TILE), (int)std::floor(c.y / TILE))) { bob = c; return true; }
      }
    return false;
  }
  if (g.sea.live.swimming || g.homeRiding()) return false;
  const int hx = (int)std::floor(p.p.x / TILE), hy = (int)std::floor(p.p.y / TILE);
  if (m.in(hx, hy) && groundWater(m.at(hx, hy))) return false;
  // facing water within three tiles: the float lands a couple of tiles out (still on water)
  const Vec2 f = norm(p.aim);
  for (int s = 1; s <= 3; s++) {
    const Vec2 c = p.p + f * ((float)s * TILE);
    const int tx = (int)std::floor(c.x / TILE), ty = (int)std::floor(c.y / TILE);
    if (!waterTile(m, tx, ty)) continue;
    Vec2 out = c;
    for (int k = 1; k <= 2; k++) {
      const Vec2 o = c + f * ((float)k * TILE);
      if (waterTile(m, (int)std::floor(o.x / TILE), (int)std::floor(o.y / TILE))) out = o; else break;
    }
    bob = out;
    return true;
  }
  return false;
}

bool SeaOps::castLine(Game& g) {
  Sea::Live& L = g.sea.live;
  if (L.line.phase != Sea::Cast::None || L.fishCd > 0 || foesNear(g, 10.0f * TILE) > 0) return false;   // (no fishing with foes about)
  Vec2 bob;
  uint8_t w = 0;
  if (!fishSpot(g, bob, w)) return false;
  Sea::Line& ln = L.line;
  ln = Sea::Line();
  ln.phase = Sea::Cast::Casting;
  ln.t = 0;
  ln.bob = bob;
  ln.fromBoat = g.sea.atSea();
  ln.bite = 3.0f + 8.0f * rng(g).f();   // (Waiting: seconds to the first nibble)
  ln.dist = 1;
  const Vec2 d = bob - g.pl().p;
  g.pl().face = faceOf(d);
  g.pl().aim = norm(d);
  g.pl().st = AState::Idle;
  ev(g, SeaEv::Cast, bob, 1, 0, "", g.pl().p);
  snd(g, (int)Sfx::Swing, g.pl().p, 1.5f, 0.5f);
  return true;
}

Item SeaOps::fishItem(int sp, int cm) {
  const std::vector<Species>& S = species();
  const Species& s = S[(size_t)std::clamp(sp, 0, (int)S.size() - 1)];
  Item it = makeFood(4);   // (a fish: Ingr::Fish to the cook, home::ingredientOf)
  it.name = s.name;
  it.tint = s.color;
  const float avg = 0.5f * (s.minCm + s.maxCm);
  const float size = std::clamp((float)cm / std::max(1.0f, avg), 0.5f, 1.8f);
  it.value = std::max(2, (int)std::lround(s.value * size));
  it.power = (int16_t)std::clamp(10 + (int)s.value / 5, 10, 60);
  return it;
}

bool SeaOps::fishInput(Game& g, const Input& in, float dt) {
  Sea::Live& L = g.sea.live;
  Sea::Line& ln = L.line;
  if (ln.phase == Sea::Cast::None) return false;
  Actor& p = g.pl();
  ln.t += dt;
  L.lineDrift += dt;
  const bool strike = in.interact || in.reel || in.attack || (L.autoReel && ln.phase == Sea::Cast::Bite && ln.t > 0.3f);
  const bool reel = in.reel || (L.autoReel && ln.surge <= 0 && ln.tension < 0.62f);
  const bool moving = len(in.move) > 0.55f;
  auto endLine = [&](float cd) { ln = Sea::Line(); L.fishCd = cd; };
  // walking off (or steering away) reels the line in
  if (moving && (ln.phase == Sea::Cast::Casting || ln.phase == Sea::Cast::Waiting || ln.phase == Sea::Cast::Bite)) { endLine(0.2f); return false; }
  // (fixer M10 r1) mid-fight the hero can always let go: a firm push of the stick, a roll, or a foe at arm's length
  // cuts the line, and the step goes on to move, fight or heal
  if (ln.phase != Sea::Cast::Landed && ln.phase != Sea::Cast::Lost &&
      ((ln.phase == Sea::Cast::Reeling && (len(in.move) > 0.8f || in.roll)) || foesNear(g, 3.5f * TILE) > 0)) {
    if (ln.phase == Sea::Cast::Reeling) tell(g, "YOU CUT THE LINE");
    endLine(0.4f);
    return false;
  }
  p.st = AState::Idle;
  switch (ln.phase) {
    case Sea::Cast::Casting:
      if (ln.t >= 0.55f) {
        ln.phase = Sea::Cast::Waiting; ln.t = 0;
        ev(g, SeaEv::Splash, ln.bob, 0.25f);
        snd(g, (int)Sfx::Splash, ln.bob, 1.6f, 0.35f);
      }
      return true;
    case Sea::Cast::Waiting: {
      if (in.interact) { endLine(0.2f); return true; }   // (reeled in empty)
      if (ln.t < ln.bite) return true;
      // something bites: which fish, by the water, the climate, the season and the hour
      const int tx = (int)std::floor(ln.bob.x / TILE), ty = (int)std::floor(ln.bob.y / TILE);
      const uint8_t water = watersAt(g, tx, ty);
      float temp = 0.5f;
      if (g.world.src && g.world.endless) temp = (float)g.world.src->macro(g.world.ox + tx, g.world.oy + ty).temp / 65536.0f;
      const int season = ((g.day - 1) / 30) & 3;
      const int sp = pickSpecies(water, climateOf(temp), season, g.hour, rng(g).f(), g.sea.catches);
      if (sp < 0) {
        if (ln.t > 16.0f) { tell(g, "NOTHING BITES HERE"); endLine(0.5f); }
        return true;
      }
      const Species& s = species()[(size_t)sp];
      ln.species = (uint16_t)sp;
      const float r = std::pow(rng(g).f(), 1.6f);
      ln.cm = (uint16_t)(s.minCm + (int)std::lround((s.maxCm - s.minCm) * r));
      ln.phase = Sea::Cast::Bite; ln.t = 0;
      ln.bite = s.rarity >= 3 ? 0.65f : s.rarity == 2 ? 0.8f : 1.0f;   // the window to strike
      ev(g, SeaEv::Bite, ln.bob, s.fight, 0, "");
      snd(g, (int)Sfx::Splash, ln.bob, 1.9f, 0.6f);
      return true;
    }
    case Sea::Cast::Bite:
      if (strike) {
        ln.phase = Sea::Cast::Reeling; ln.t = 0; ln.tension = 0.15f; ln.dist = 1;
        ln.surge = 0; ln.next = 1.2f + 1.5f * rng(g).f();
        ev(g, SeaEv::Strike, ln.bob, species()[ln.species].fight);
        snd(g, (int)Sfx::Swing, p.p, 0.8f, 0.6f);
        return true;
      }
      if (ln.t > ln.bite) {   // missed: it nibbles again, or is gone
        ln.phase = Sea::Cast::Waiting; ln.t = 0; ln.bite = 2.0f + 5.0f * rng(g).f();
        tell(g, "IT STOLE THE BAIT");
      }
      return true;
    case Sea::Cast::Reeling: {
      const Species& s = species()[ln.species];
      // the fish's runs: surges of a second or so every few seconds (harder fish surge more)
      if (ln.surge > 0) ln.surge -= dt;
      else if ((ln.next -= dt) <= 0) { ln.surge = 0.7f + 0.8f * rng(g).f() * s.fight; ln.next = 1.6f + 2.4f * rng(g).f() * (1.2f - s.fight * 0.5f); }
      const bool run = ln.surge > 0;
      if (reel) {
        ln.dist -= dt * (0.16f - 0.07f * s.fight) * (run ? 0.25f : 1.0f);
        ln.tension += dt * ((0.18f + 0.35f * s.fight) + (run ? 0.55f + 0.6f * s.fight : 0.0f));
      } else {
        ln.tension -= dt * 0.7f;
        ln.dist += dt * (run ? 0.10f + 0.10f * s.fight : 0.03f);
      }
      ln.tension = std::clamp(ln.tension, 0.0f, 1.0f);
      if ((int)(ln.t * 4.0f) != (int)((ln.t - dt) * 4.0f)) ev(g, SeaEv::Reel, ln.bob, ln.tension, run ? 1u : 0u);
      // the float comes in toward the rod as the fish does
      const Vec2 rod = p.p + Vec2(0, -6);
      if (ln.dist < 1.0f) ln.bob = rod + (ln.bob - rod) * std::clamp(1.0f - dt * 0.6f * (1.0f - ln.dist) / std::max(0.05f, ln.dist), 0.0f, 1.0f);
      if (ln.tension >= 1.0f) {
        ln.phase = Sea::Cast::Lost; ln.t = 0;
        ev(g, SeaEv::Lost, ln.bob, 1, 0, "THE LINE SNAPS!");
        tell(g, "THE LINE SNAPS!");
        return true;
      }
      if (ln.dist >= 1.3f) {
        ln.phase = Sea::Cast::Lost; ln.t = 0;
        ev(g, SeaEv::Lost, ln.bob, 0, 0, "IT SLIPPED THE HOOK");
        tell(g, "IT SLIPPED THE HOOK");
        return true;
      }
      if (ln.dist <= 0.0f) {
        ln.phase = Sea::Cast::Landed; ln.t = 0;
        const std::string line = std::string(s.name) + "  " + std::to_string(ln.cm) + " CM";
        ev(g, SeaEv::Landed, rod, (float)s.rarity, (uint32_t)ln.species, line);
        snd(g, (int)(s.rarity >= 2 ? Sfx::QuestDone : Sfx::Pickup), p.p, 1.0f, 0.8f);
        // the catch book and the pack
        auto it = std::lower_bound(g.sea.catches.begin(), g.sea.catches.end(), ln.species, [](const Catch& c, uint16_t v) { return c.species < v; });
        if (it == g.sea.catches.end() || it->species != ln.species) it = g.sea.catches.insert(it, Catch{ln.species, 0, 0});
        it->count++;
        const bool record = ln.cm > it->bestCm;
        it->bestCm = std::max(it->bestCm, ln.cm);
        g.sea.fishCaught++;
        g.addItem(fishItem(ln.species, ln.cm), false);
        g.gainXp(2 + 6 * s.rarity * s.rarity);
        if (s.rarity >= 3) { tell(g, std::string("LEGENDARY CATCH: ") + s.name + "! " + fishStory(ln.species)); g.realm.renown().fame += 15; }
        else tell(g, "CAUGHT: " + line + (record && it->count > 1 ? "  (YOUR BEST)" : ""));
        seaQuestStep(g, 3, 0, 0, 0, s.name, false);   // (a fishmonger's order)
      }
      return true;
    }
    case Sea::Cast::Landed:
      if (ln.t > 1.4f) endLine(0.6f);
      return true;
    case Sea::Cast::Lost:
      if (ln.t > 1.0f) endLine(0.4f);
      return true;
    default: return false;
  }
}
