// M8 Fellowship: the graves of the fallen (VISION_PLAN 15.6.2: a grave you can visit, their belongings, others who
// remember them) and the looks VIEW reads for the party's own sprites. FELLOWS lane.
//   - a companion who dies for good is buried in their home settlement's temple yard (beside its temple, else near the
//     heart of the place) when it lies in the Active Window, else where they fell (the overworld tile; inside a cave or
//     a building: by its door). A grave is one solid tile stamped art::Prop::Filler (never drawn: VIEW draws
//     art::graveSprite there from Game::partyGraveLook), stamped again whenever World::placeSerial changes, like the
//     M7 yard objects, so walking away, saving and loading keep it;
//   - the player stands at it and presses use: partyInteract -> partyGraveTalk (VOICE: the epitaph, flowers, a keepsake
//     from the belongings laid there) -> partyGraveAct.
#include <algorithm>
#include <cmath>
#include <vector>
#include "rpg/sim/game.h"
#include "rpg/sim/game_internal.h"
#include "rpg/sim/home.h"
#include "rpg/sim/life.h"
#include "rpg/sim/party.h"

using party::Fallen;
using party::Member;

namespace {
constexpr float T = (float)TILE;
// a tile a grave may take: open ground, nothing on it, no building, no yard, no road; the ground round it open too
bool graveOk(const Game& g, int x, int y) {
  const Map& M = g.world.over;
  if (!M.in(x - 1, y - 1) || !M.in(x + 1, y + 1)) return false;
  for (int dy = -1; dy <= 1; dy++)
    for (int dx = -1; dx <= 1; dx++) {
      const int xx = x + dx, yy = y + dy;
      const size_t i = (size_t)yy * M.w + xx;
      if (M.bldgAt[i] >= 0 || M.wall[i]) return false;
      const Ground gr = M.at(xx, yy);
      if (groundSolid(gr) || gr == Ground::Bridge || gr == Ground::Ice) return false;
      if (dx == 0 && dy == 0) {
        if (M.prop[i] || M.solid[i] || gr == Ground::Road || gr == Ground::Plaza || gr == Ground::Farmland) return false;
      } else if (M.solid[i]) return false;   // (never boxed in: the ground round it stays walkable)
    }
  // (fixer M8 r1, review: "sited against doors and pens") a burial plot keeps its distance: no building, door, fence
  // or pen within two tiles
  for (int dy = -2; dy <= 2; dy++)
    for (int dx = -2; dx <= 2; dx++) {
      const int xx = x + dx, yy = y + dy;
      if (!M.in(xx, yy)) return false;
      const size_t i = (size_t)yy * M.w + xx;
      if (M.bldgAt[i] >= 0 || M.wall[i]) return false;
      if (M.solid[i] && M.prop[i]) return false;
      if (g.home.plotAt(g.world.ox + xx, g.world.oy + yy) >= 0) return false;
    }
  if (g.home.plotAt(g.world.ox + x, g.world.oy + y) >= 0) return false;
  for (const Fallen& f : g.party.fallen)
    if ((f.graveFlags & party::GF_SITED) && std::abs(f.gx - (g.world.ox + x)) <= 1 && std::abs(f.gy - (g.world.oy + y)) <= 1) return false;
  return true;
}
// a quiet tile: natural ground with no road, square or beaten path beside it (a grave is no street furniture)
bool quiet(const Game& g, int x, int y) {
  const Map& M = g.world.over;
  for (int dy = -1; dy <= 1; dy++)
    for (int dx = -1; dx <= 1; dx++) {
      const Ground gr = M.at(x + dx, y + dy);
      if (gr == Ground::Road || gr == Ground::Plaza || gr == Ground::Dirt || gr == Ground::Farmland) return false;
    }
  return true;
}
// the nearest open tile to (cx, cy) in rings (deterministic order): a quiet one within r first, else any open one
bool nearestOk(const Game& g, int cx, int cy, int r, int& ox, int& oy) {
  for (int pass = 0; pass < 2; pass++)
    for (int d = 0; d <= r; d++)
      for (int dy = -d; dy <= d; dy++)
        for (int dx = -d; dx <= d; dx++) {
          if (std::max(std::abs(dx), std::abs(dy)) != d) continue;
          if (graveOk(g, cx + dx, cy + dy) && (pass == 1 || quiet(g, cx + dx, cy + dy))) { ox = cx + dx; oy = cy + dy; return true; }
        }
  return false;
}
}  // namespace

void PartyOps::siteGrave(Game& g, Fallen& f) {
  const World& W = g.world;
  // their home settlement, when it is here in the window: beside its temple (the temple yard), else near its heart
  const int si = f.site ? W.siteHandle(f.site) : -1;
  if (si >= 0 && W.sites[(size_t)si].settlement()) {
    const Site& S = W.sites[(size_t)si];
    if (W.over.in(S.ex, S.ey)) {
      // no temple: the quiet edge of the place (one side of its footprint, chosen by its id), where a village lays
      // its dead
      static const int side[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
      const int k = (int)(hash32((uint32_t)S.id ^ (uint32_t)(S.id >> 32) ^ 0x6A5Eu) % 4u);
      int cx = side[k][0] > 0 ? S.r.x + S.r.w + 1 : side[k][0] < 0 ? S.r.x - 2 : S.r.x + S.r.w / 2;
      int cy = side[k][1] > 0 ? S.r.y + S.r.h + 1 : side[k][1] < 0 ? S.r.y - 2 : S.r.y + S.r.h / 2;
      if (S.r.w <= 0 || S.r.h <= 0) { cx = S.ex; cy = S.ey; }
      if (const life::Census* c = g.life.find(S.id)) {
        const int tb = c->temple >= 0 ? S.bldgFirst + c->temple : -1;
        if (tb >= 0 && tb < (int)W.over.bldgs.size()) {
          const Bldg& b = W.over.bldgs[(size_t)tb];
          cx = b.r.x + b.r.w + 1; cy = b.r.y + b.r.h - 1;   // the yard on its east side
        }
      }
      int x = 0, y = 0;
      if (nearestOk(g, cx, cy, 12, x, y)) {
        f.gx = W.ox + x; f.gy = W.oy + y;
        f.graveFlags |= party::GF_SITED | party::GF_HOME;
        return;
      }
    }
  }
  // where they fell (f.gx / f.gy hold it: the door or the cave mouth when it was inside)
  int x = 0, y = 0;
  if (nearestOk(g, f.gx - W.ox, f.gy - W.oy, 8, x, y)) {
    f.gx = W.ox + x; f.gy = W.oy + y;
    f.graveFlags |= party::GF_SITED;
  }
}

void PartyOps::gravesStep(Game& g) {
  party::Party& P = g.party;
  if (P.fallen.empty()) return;
  World& W = g.world;
  // one fallen while the window was elsewhere (a load of an old grave without a place): sited now
  for (Fallen& f : P.fallen)
    if (!(f.graveFlags & party::GF_SITED) && W.over.in(f.gx - W.ox, f.gy - W.oy) && !g.inside) { siteGrave(g, f); P.graveDirty = true; }
  if (!P.graveDirty && P.graveSerial == W.placeSerial) return;
  Map& M = W.over;
  if (M.prop.empty() || M.solid.size() != M.prop.size()) return;
  P.graveSerial = W.placeSerial;
  P.graveDirty = false;
  P.stamped.clear();
  for (const Fallen& f : P.fallen) {
    if (!(f.graveFlags & party::GF_SITED)) continue;
    const int x = f.gx - W.ox, y = f.gy - W.oy;
    if (!M.in(x, y)) continue;
    const size_t i = (size_t)y * M.w + x;
    if (M.bldgAt[i] >= 0) continue;
    M.setProp(x, y, art::Prop::Filler);
    M.solid[i] = 1;
    P.stamped.push_back({f.gx, f.gy});
  }
}

int PartyOps::graveNear(const Game& g, Vec2 p, float r) {
  int best = -1;
  float bd = r * r;
  for (size_t i = 0; i < g.party.fallen.size(); i++) {
    const Fallen& f = g.party.fallen[i];
    if (!(f.graveFlags & party::GF_SITED)) continue;
    const Vec2 c((f.gx - g.world.ox) * T + 8.0f, (f.gy - g.world.oy) * T + 10.0f);
    const float d2 = len2(c - p);
    if (d2 < bd) { bd = d2; best = (int)i; }
  }
  return best;
}

int Game::partyGraveAt(int tx, int ty) const {
  for (size_t i = 0; i < party.fallen.size(); i++) {
    const Fallen& f = party.fallen[i];
    if ((f.graveFlags & party::GF_SITED) && f.gx - world.ox == tx && f.gy - world.oy == ty) return (int)i;
  }
  return -1;
}

art::GraveLook Game::partyGraveLook(int fi) const {
  art::GraveLook L;
  if (fi < 0 || fi >= (int)party.fallen.size()) return L;
  const Fallen& f = party.fallen[(size_t)fi];
  L.rite = (uint8_t)std::min<int>(f.rite, art::GRAVE_RITES - 1);
  L.weapon = f.weapon;
  L.fresh = day - f.day < 3;
  L.flowers = (f.graveFlags & party::GF_FLOWERS) != 0;
  L.seed = f.seed;
  return L;
}

art::HoundLook Game::partyHoundLook(int mi) const {
  art::HoundLook L;
  if (mi < 0 || mi >= (int)party.members.size()) return L;
  const Member& m = party.members[(size_t)mi];
  L.species = m.species > 1 ? 1 : m.species;
  const int days = day - m.joinedDay;
  L.age = (m.flags & party::MF_PUP) ? (uint8_t)(days < party::PUP_DAYS / 2 ? 0 : 1) : (uint8_t)2;
  L.collar = m.bond >= party::SKILL_BOND[1] ? 2 : 1;
  L.seed = m.lookSeed;
  return L;
}
