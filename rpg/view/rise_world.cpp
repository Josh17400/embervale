// M12 "Rise and Fall" VIEW lane: the rise in the world (VISION_PLAN 4.5 "banners follow, charred and rebuilding
// places", 13 M12, 15.33). Everything here is drawn by the view over the loaded window (the prop budget is spent): no
// map tile changes.
//   - Prop::LordBanner flies its owner's arms: the realm that holds the tile's settlement (a warlord's near its camp)
//   - the player's holdings show their works (lordship.h: "the VIEW lane draws them on the village"): the lord's
//     standard by the heart, a watchtower at the edge, the market cross, a wayside shrine, the granary's sacks, and the
//     work being raised as a scaffold with its timber, thatch or stone beside it (where a real prop of the work already
//     stands, the prop is the work)
//   - a warband's camp flies a ragged standard among trophy pikes; a warlord's camp two standards in the warlord's
//     colours, a ring of trophies and a rough stockade of stakes; a warlord's seat its standard at the heart
//   - a resettled place (SS_REBUILDING) has scaffolds, stacked timber, thatch bundles, new stone and a cart
//   - a town a world boss raided (BeastRaid in the last 20 days) is torn by claw scars and smokes from its roofs
// Spots are chosen once per window (free tiles near the place's heart, clear of buildings, walls, water and props),
// deterministic by the site's id, and cached until the window moves.
#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <vector>
#include "rpg/art/art_props.h"
#include "rpg/art/art_rise.h"
#include "rpg/sim/lord_ops.h"
#include "rpg/sim/lordship.h"
#include "rpg/view/prop_traits.h"
#include "rpg/view/realm_ui.h"
#include "rpg/view/rise_view.h"

namespace {
enum PieceKind { P_BANNER = 1, P_PROP, P_WARSTD, P_TROPHY, P_STORES, P_SCAFFOLD, P_STAKE, P_CLAW, P_WARPROP, P_ASH, P_SHELL };

struct SpotCache {
  int32_t ox = 1 << 30, oy = 1 << 30;
  int w = 0;
  std::unordered_map<uint64_t, std::vector<std::pair<int, int>>> spots;
};
SpotCache& cache() { static SpotCache c; return c; }

uint32_t hashU(uint64_t x) { x ^= x >> 33; x *= 0xff51afd7ed558ccdull; x ^= x >> 33; return (uint32_t)x; }

// a free tile for a standing piece: walkable, no building within a tile, no wall, no prop, not on a road or plaza
bool freeTile(const Map& m, int x, int y) {
  if (!m.in(x, y) || m.blocked(x, y)) return false;
  for (int dy = -2; dy <= 1; dy++)
    for (int dx = -1; dx <= 1; dx++) {
      const int qx = x + dx, qy = y + dy;
      if (!m.in(qx, qy)) return false;
      if (m.bldgAt[(size_t)qy * m.w + qx] >= 0) return false;
      if (!m.wall.empty() && m.wall[(size_t)qy * m.w + qx]) return false;
    }
  if (m.prop[(size_t)y * m.w + x] != 0) return false;
  const Ground gr = m.at(x, y);
  return gr != Ground::Road && gr != Ground::Bridge && !groundSolid(gr) && gr != Ground::Swamp;   // (a square takes stores and banners)
}
// `count` spots in rings r0..r1 round (cx, cy), at least `gap` tiles apart, the ring order shuffled by the key
// (M12 fixer) a spot for a tall standing piece (a standard): nothing standing 1-2 tiles in front of it to hide its foot
// (a tree's crown, a stall's counter) and no crown right behind it to tangle with its cloth
bool clearTall(const Map& m, int x, int y) {
  for (int dy = -2; dy <= 2; dy++)
    for (int dx = -1; dx <= 1; dx++) {
      if (!dy && !dx) continue;
      if (!m.in(x + dx, y + dy)) return false;
      if (m.prop[(size_t)(y + dy) * m.w + x + dx] != 0) return false;
    }
  return true;
}
// mode 0: a standing piece (freeTile); 1: flat on the ground (only walkable, no building); 2: a tall standing piece
std::vector<std::pair<int, int>> findSpots(const Map& m, int cx, int cy, int r0, int r1, int count, int gap, uint64_t key, int mode = 0) {
  SpotCache& C = cache();
  auto it = C.spots.find(key);
  if (it != C.spots.end()) return it->second;
  std::vector<std::pair<int, int>> out;
  for (int r = r0; r <= r1 && (int)out.size() < count; r++) {
    std::vector<std::pair<int, int>> ring;
    for (int dy = -r; dy <= r; dy++)
      for (int dx = -r; dx <= r; dx++)
        if (std::max(std::abs(dx), std::abs(dy)) == r) ring.push_back({cx + dx, cy + dy});
    // a stable shuffle by the key, so spots do not all line up on the ring's first side
    const uint32_t hs = hashU(key ^ (uint64_t)r * 0x9E37ull);
    for (size_t i = ring.size(); i > 1; i--) std::swap(ring[i - 1], ring[(hs * 2654435761u + (uint32_t)i * 40503u) % i]);
    for (const auto& p : ring) {
      if ((int)out.size() >= count) break;
      const bool ok = mode == 1 ? (m.in(p.first, p.second) && !m.blocked(p.first, p.second) && m.bldgAt[(size_t)p.second * m.w + p.first] < 0)
                                : freeTile(m, p.first, p.second) && (mode != 2 || clearTall(m, p.first, p.second));
      if (!ok) continue;
      bool near = false;
      for (const auto& q : out) near |= std::abs(q.first - p.first) < gap && std::abs(q.second - p.second) < gap;
      if (!near) out.push_back(p);
    }
  }
  C.spots[key] = out;
  return out;
}
// the realm that holds a loaded settlement now (the realm's record, else the world's)
ew::Gid ownerOfSite(const Game& g, int h) {
  if (h < 0) return 0;
  const Site& s = g.world.sites[(size_t)h];
  if (const realm::SettlementState* st = g.realm.settlement(s.id)) if (st->owner) return st->owner;
  return s.kingdom >= 0 ? g.world.kingdoms[(size_t)s.kingdom].id : 0;
}
bool inView(Vec2 cam, int tx, int ty, int padT = 6) {
  return tx * 16 > cam.x - padT * 16 && tx * 16 < cam.x + Pix::W + padT * 16 && ty * 16 > cam.y - padT * 16 && ty * 16 < cam.y + Pix::H + padT * 16 + 64;
}
}  // namespace

// ---------------------------------------------------------------- the lord's standard in its owner's arms
const Tex* View::riseBannerTex(Game& g, int tx, int ty) {
  RiseViewState& S = RiseViewOps::st(*this);
  S.bannerCount++;
  ew::Gid k = 0;
  const int h = g.world.siteAt(tx, ty, 3);
  if (h >= 0 && g.world.sites[(size_t)h].settlement()) k = ownerOfSite(g, h);
  if (!k) {   // a warlord's standard at its camp
    const int32_t gx = g.world.ox + tx, gy = g.world.oy + ty;
    for (const realm::Camp& c : g.realm.camps())
      if ((c.flags & realm::CF_WARLORD) && c.realm && std::abs(c.gx - gx) < 24 && std::abs(c.gy - gy) < 24) { k = c.realm; break; }
  }
  if (!k) return nullptr;
  cult::Heraldry hr;
  RiseViewOps::armsOf(g, k, hr, nullptr);
  const uint64_t key = ew::mix64(hr.key() ^ 0x10ADBA77ull);
  auto it = S.worldTex.find(key);
  if (it != S.worldTex.end()) return &it->second;
  return &(S.worldTex[key] = pix_->bake(art::lordBannerSprite(hr)));
}

// ---------------------------------------------------------------- collecting this frame's pieces
void View::riseSorted(Game& g, Vec2 cam, const std::function<void(float, int)>& push) {
  RiseViewState& S = RiseViewOps::st(*this);
  S.pieces.clear();
  S.flats.clear();
  S.stats.works = 0; S.stats.warlordPieces = 0; S.stats.rebuildPieces = 0; S.stats.raidScars = 0;
  S.stats.lordBanners = S.bannerCount;   // (last frame's: the banners are counted as they are drawn)
  S.bannerCount = 0;
  const float dt = std::clamp(t_ - S.smokeT, 0.0f, 0.1f);
  S.smokeT = t_;
  if (g.inside || !g.world.endless || g.mode == Mode::Title) return;
  const Map& m = g.world.over;
  if (m.kind != MapKind::Overworld || m.bldgAt.empty()) return;
  SpotCache& C = cache();
  if (C.ox != g.world.ox || C.oy != g.world.oy || C.w != m.w || C.spots.size() > 256) { C.spots.clear(); C.ox = g.world.ox; C.oy = g.world.oy; C.w = m.w; }
  auto add = [&](int kind, int tx, int ty, int var, ew::Gid k) {
    RiseViewState::Piece p;
    p.kind = kind; p.x = tx * 16.0f + 8; p.y = ty * 16.0f + 16; p.var = var; p.key = k;
    if (kind == P_CLAW || kind == P_ASH) S.flats.push_back(p);
    else {
      S.pieces.push_back(p);
      push(ty * 16.0f + 15.6f, (int)S.pieces.size() - 1);
    }
  };
  // a real prop of this kind inside the site's box (then the prop is the work)
  auto hasProp = [&](const Site& s, art::Prop p) {
    for (int y = std::max(0, s.r.y); y < std::min(m.h, s.r.y + s.r.h); y++)
      for (int x = std::max(0, s.r.x); x < std::min(m.w, s.r.x + s.r.w); x++)
        if (m.prop[(size_t)y * m.w + x] == (int)p + 1) return true;
    return false;
  };
  // ---- 1. the player's holdings and their works (only where the LORD lane has not stamped real props: then they are the works)
  const std::vector<LordOps::WorkTile> realWorks = g.lordship.holdings.empty() ? std::vector<LordOps::WorkTile>{} : LordOps::stamped(g);
  for (const lord::Holding& H : g.lordship.holdings) {
    const int h = g.world.siteHandle(H.site);
    if (h < 0) continue;
    const Site& s = g.world.sites[(size_t)h];
    if (!inView(cam, s.ex, s.ey, 40)) continue;
    bool stampedHere = false;
    for (const LordOps::WorkTile& w : realWorks) {
      const int x = w.gx - g.world.ox, y = w.gy - g.world.oy;
      if (x >= s.r.x - 24 && x < s.r.x + s.r.w + 24 && y >= s.r.y - 24 && y < s.r.y + s.r.h + 24) { stampedHere = true; break; }
    }
    if (stampedHere) continue;
    const ew::Gid owner = ownerOfSite(g, h);
    const auto sp = findSpots(m, s.ex, s.ey, 2, 12, 8, 3, H.site ^ 0x40C5ull);
    auto at = [&](size_t i, int kind, int var, ew::Gid k) {
      if (i < sp.size() && inView(cam, sp[i].first, sp[i].second)) { add(kind, sp[i].first, sp[i].second, var, k); S.stats.works++; }
    };
    if ((H.works & lord::LW_BANNER) && !hasProp(s, art::Prop::LordBanner)) {
      // the standard on a spot of its own, clear in front and behind (never behind a tree or through a stall)
      const auto bs = findSpots(m, s.ex, s.ey, 2, 12, 1, 3, H.site ^ 0xBA77ull, 2);
      if (bs.empty()) at(0, P_BANNER, 0, owner);
      else if (inView(cam, bs[0].first, bs[0].second)) { add(P_BANNER, bs[0].first, bs[0].second, 0, owner); S.stats.works++; }
    }
    if ((H.works & lord::LW_WATCHTOWER) && !hasProp(s, art::Prop::Watchtower)) {
      // the tower at the village's edge (its own spot search, farther out)
      const auto ts = findSpots(m, s.ex, s.ey, 8, 18, 1, 3, H.site ^ 0x70E7ull);
      if (!ts.empty() && inView(cam, ts[0].first, ts[0].second)) { add(P_PROP, ts[0].first, ts[0].second, (int)art::Prop::Watchtower, 0); S.stats.works++; }
    }
    if (H.works & lord::LW_MARKET) at(1, P_PROP, (int)art::Prop::MarketCross, 0);
    if (H.works & lord::LW_SHRINE) at(2, P_PROP, (int)art::Prop::Shrine, 0);
    if (H.works & lord::LW_GRANARY) { at(3, P_PROP, (int)art::Prop::Sacks, 0); at(4, P_PROP, (int)art::Prop::Barrel, 0); }
    if (H.building) { at(5, P_SCAFFOLD, 0, 0); at(6, P_STORES, 0, 0); at(7, P_STORES, H.building == lord::LW_WALLS || H.building == lord::LW_SHRINE ? 2 : 1, 0); }
  }
  // ---- 2. warbands' and warlords' camps
  for (const realm::Camp& c : g.realm.camps()) {
    if ((c.flags & realm::CF_CLEARED) || !(c.flags & (realm::CF_WARBAND | realm::CF_WARLORD))) continue;
    const int h = g.world.siteHandle(c.site);
    if (h < 0) continue;
    const Site& s = g.world.sites[(size_t)h];
    if (!inView(cam, s.ex, s.ey, 40)) continue;
    const bool lord = (c.flags & realm::CF_WARLORD) && c.realm;
    // (M12 fixer) a warlord's seat: a stockade close round the camp's clearing (one joined ring of stakes, a gateway to
    // the south between gateposts, a breach or two where it was never finished), its two standards flanking the gate,
    // the trophies inside along the fence, never by the fire
    const int RX = 7, RY = 5;
    std::vector<std::pair<int, int>> ring;
    if (lord) {
      int px = 1 << 30, py = 1 << 30;
      for (int a = 0; a <= 96; a++) {
        const float ang = a / 96.0f * 6.2831853f;
        const float wob = 1.0f + 0.08f * std::sin(ang * 3.0f + (float)(c.site % 7));
        const int x = s.ex + (int)std::lround(std::cos(ang) * RX * wob), y = s.ey + (int)std::lround(std::sin(ang) * RY * wob);
        if (x == px && y == py) continue;
        if (px != (1 << 30) && x != px && y != py) ring.push_back({x, py});   // 4-connected: the run joins on a diagonal
        ring.push_back({x, y});
        px = x; py = y;
      }
      std::vector<std::pair<int, int>> kept;
      const int breach = (int)(hashU(c.site ^ 0xB4EAull) % 1000);
      for (size_t i = 0; i < ring.size(); i++) {
        const auto& p = ring[i];
        if (std::abs(p.first - s.ex) <= 1 && p.second > s.ey) continue;   // the gateway (south)
        const int ph = (int)((i * 1000) / std::max<size_t>(1, ring.size()));
        if (std::abs(ph - breach) < 18 || std::abs(ph - (breach + 500) % 1000) < 12) continue;   // the breaches
        bool dup = false;
        for (const auto& q : kept) dup |= q == p;
        if (!dup && freeTile(m, p.first, p.second)) kept.push_back(p);
      }
      ring.swap(kept);
    }
    auto inRing = [&](int x, int y) { for (const auto& q : ring) if (q.first == x && q.second == y) return true; return false; };
    const auto sp = findSpots(m, s.ex, s.ey, lord ? 3 : 2, lord ? RY - 1 : 9, lord ? 6 : 4, 2, c.site ^ 0xCA3Bull);
    int std0 = 0;
    if (lord) {
      // the standards at the gateposts, just inside
      const int gy = s.ey + RY - 1;
      for (int side = -1; side <= 1; side += 2) {
        int x = s.ex + side * 2, y = gy;
        for (int t = 0; t < 3 && !(freeTile(m, x, y) && !inRing(x, y)); t++) y--;
        if (freeTile(m, x, y) && !inRing(x, y) && inView(cam, x, y)) { add(P_WARSTD, x, y, 0, c.realm); S.stats.warlordPieces++; std0++; }
      }
    }
    for (size_t i = 0; i < sp.size(); i++) {
      if (!inView(cam, sp[i].first, sp[i].second) || inRing(sp[i].first, sp[i].second)) continue;
      if (std::abs(sp[i].first - s.ex) <= 2 && sp[i].second >= s.ey + RY - 3 && lord) continue;   // the gate's standards' room
      if ((int)i < (lord ? 2 - std0 : 1) && (!lord || std0 < 2)) add(P_WARSTD, sp[i].first, sp[i].second, 0, lord ? c.realm : 0);
      else add(P_TROPHY, sp[i].first, sp[i].second, (int)(hashU(c.site + i) & 3), lord ? c.realm : 0);
      S.stats.warlordPieces++;
    }
    if (lord) {
      for (const auto& p : ring) {
        if (!inView(cam, p.first, p.second)) continue;
        int mask = 0, gates = 0;
        if (inRing(p.first, p.second - 1)) mask |= 1;
        if (inRing(p.first + 1, p.second)) mask |= 2;
        if (inRing(p.first, p.second + 1)) mask |= 4;
        if (inRing(p.first - 1, p.second)) mask |= 8;
        // the gateposts: the ring's tiles either side of the south gateway
        if (p.second > s.ey && p.first == s.ex - 2 && inRing(p.first + 4, p.second)) gates |= 2;
        if (p.second > s.ey && p.first == s.ex + 2 && inRing(p.first - 4, p.second)) gates |= 8;
        add(P_STAKE, p.first, p.second, mask | gates << 4, 0);
        S.stats.warlordPieces++;
      }
    }
  }
  // ---- 3. warlords' seats, resettled places, boss-raided towns (the raids of the last 20 days, gathered once)
  std::vector<ew::Gid> raidedSites;
  for (const realm::WorldEvent& e : g.realm.events())
    if (e.type == realm::EvType::BeastRaid && e.site && g.day - (int)e.day <= 20) raidedSites.push_back(e.site);
  for (size_t h = 0; h < g.world.sites.size(); h++) {
    const Site& s = g.world.sites[h];
    if (!s.settlement() || !inView(cam, s.ex, s.ey, 50)) continue;
    const ew::Gid owner = ownerOfSite(g, (int)h);
    const realm::KingdomState* K = owner ? g.realm.kingdom(owner) : nullptr;
    if (K && K->origin == realm::KO_WARLORD) {
      const auto sp = findSpots(m, s.ex, s.ey, 1, 8, 3, 2, s.id ^ 0x5EA7ull);
      for (size_t i = 0; i < sp.size(); i++)
        if (inView(cam, sp[i].first, sp[i].second)) { add(i == 0 ? P_WARSTD : P_TROPHY, sp[i].first, sp[i].second, (int)(i + 1) & 3, owner); S.stats.warlordPieces++; }
    }
    const realm::SettlementState* st = g.realm.settlement(s.id);
    if (st && (st->flags & realm::SS_REBUILDING)) {
      // (M12 fixer) the rebuilding happens AT the houses: the two houses nearest the heart each have a scaffold against
      // their front wall, the rubble of what fell heaped by it and the new timber, thatch or stone stacked beside; a
      // cart of materials stands out on the open ground (scattered stores on an intact square read as nothing)
      int done = 0;
      std::vector<std::pair<int, int>> used;
      auto flatFree = [&](int x, int y) {
        if (!m.in(x, y) || m.blocked(x, y) || m.bldgAt[(size_t)y * m.w + x] >= 0 || m.prop[(size_t)y * m.w + x]) return false;
        if (!m.wall.empty() && m.wall[(size_t)y * m.w + x]) return false;
        const Ground gr = m.at(x, y);
        if (gr == Ground::Road || gr == Ground::Bridge || groundSolid(gr)) return false;
        for (const auto& u : used) if (std::abs(u.first - x) < 2 && u.second == y) return false;
        return true;
      };
      std::vector<std::pair<int, int>> houses;   // (distance, index)
      for (size_t bi = 0; bi < m.bldgs.size(); bi++) {
        const Bldg& b = m.bldgs[bi];
        if (b.site != (int)h) continue;
        const int ddx = b.r.x + b.r.w / 2 - s.ex, ddy = b.r.y + b.r.h / 2 - s.ey;
        houses.push_back({ddx * ddx + ddy * ddy, (int)bi});
      }
      std::sort(houses.begin(), houses.end());
      for (size_t k = 0; k < houses.size() && done < 2; k++) {
        const Bldg& b = m.bldgs[(size_t)houses[k].second];
        if (houses[k].first > 20 * 20) break;
        int fy = b.r.y + b.r.h;   // the row in front of the house
        const int dx0 = b.doorX();
        int sx = -1, side = 1;
        for (int x = b.r.x; x < b.r.x + b.r.w; x++)
          if (std::abs(x - dx0) >= 2 && flatFree(x, fy) && flatFree(x + (x < dx0 ? -1 : 1), fy)) { sx = x; side = x < dx0 ? -1 : 1; break; }
        // else against a side wall (a town's houses front their streets)
        for (int y = b.r.y + b.r.h - 1; sx < 0 && y > b.r.y; y--)
          for (int sd = -1; sd <= 1 && sx < 0; sd += 2) {
            const int x = sd < 0 ? b.r.x - 1 : b.r.x + b.r.w;
            if (flatFree(x, y) && flatFree(x + sd, y)) { sx = x; fy = y; side = sd; }
          }
        if (sx < 0 || !inView(cam, sx, fy)) continue;
        add(P_SCAFFOLD, sx, fy, 0, 0); used.push_back({sx, fy}); S.stats.rebuildPieces++;
        add(P_WARPROP, sx + side, fy, (int)art::Prop::Rubble, 0); used.push_back({sx + side, fy}); S.stats.rebuildPieces++;
        if (flatFree(sx + 2 * side, fy + 1)) { add(P_STORES, sx + 2 * side, fy + 1, (int)(houses[k].second % 3), 0); used.push_back({sx + 2 * side, fy + 1}); S.stats.rebuildPieces++; }
        done++;
      }
      // (M12 fixer r2) and by the heart the house that burned, being cleared to be raised again: its shell (sooty stone
      // walls broken off, charred posts, the fallen roof beam on its scorched floor), a scaffold at its side where the new
      // frame goes up, the timber stacked by it and the cart that brought it. The place is the free 4 x 3 lot (with a
      // tile clear round it) nearest the heart, chosen once per window (the camera never moves it)
      const uint64_t lotKey = s.id ^ 0x5E11B0A7ull;
      auto li = C.spots.find(lotKey);
      if (li == C.spots.end()) {
        std::vector<std::pair<int, int>> lot;
        auto clearLot = [&](int x0, int y0) {
          for (int y = y0 - 1; y <= y0 + 3; y++)
            for (int x = x0 - 1; x <= x0 + 4; x++) {
              if (!m.in(x, y) || m.prop[(size_t)y * m.w + x] || m.bldgAt[(size_t)y * m.w + x] >= 0) return false;
              const bool inner = x >= x0 && x <= x0 + 3 && y >= y0 && y <= y0 + 2;
              if (inner && (m.blocked(x, y) || (!m.wall.empty() && m.wall[(size_t)y * m.w + x]))) return false;
              const Ground gr = m.at(x, y);
              if (inner && (gr == Ground::Road || gr == Ground::Bridge || groundSolid(gr) || gr == Ground::Swamp)) return false;
            }
          return true;
        };
        int best = 1 << 30;
        for (int dy = -9; dy <= 9; dy++)
          for (int dx = -12; dx <= 12; dx++) {
            const int x0 = s.ex + dx - 2, y0 = s.ey + dy - 1;
            const int d2 = dx * dx + dy * dy * 2 + (dy < 0 ? 6 : 0);   // nearest, a little sooner in front of the heart
            if (d2 >= best || d2 < 4 || !clearLot(x0, y0)) continue;
            best = d2;
            lot.assign(1, {x0, y0});
          }
        li = C.spots.emplace(lotKey, lot).first;
      }
      if (!li->second.empty()) {
        const int x0 = li->second[0].first, y0 = li->second[0].second;
        const int v = (int)(hashU(s.id ^ 0x5E11ull) & 3);
        if (inView(cam, x0 + 2, y0 + 1)) {
          add(P_ASH, x0, y0, v, 0);
          add(P_SHELL, x0, y0, 1 | v << 4, 0);
          add(P_SHELL, x0, y0 + 2, 2 | v << 4, 0);
          add(P_SCAFFOLD, x0 + 4, y0 + 1, 0, 0);
          if (flatFree(x0 + 5, y0 + 2)) add(P_STORES, x0 + 5, y0 + 2, (int)(s.id % 3), 0);
          if (flatFree(x0 - 2, y0 + 2)) add(P_PROP, x0 - 2, y0 + 2, (int)art::Prop::Cart, 0);
          if (flatFree(x0 + 1, y0 + 3)) add(P_WARPROP, x0 + 1, y0 + 3, (int)art::Prop::Rubble, 0);
          S.stats.rebuildPieces += 4;
        }
      }
    }
    // a world boss's raid in the last 20 days
    const bool raided = std::find(raidedSites.begin(), raidedSites.end(), s.id) != raidedSites.end();
    if (raided) {
      // (M12 fixer) the rakes on open ground (never under the well or a stall), each its own of eight
      const auto sp = findSpots(m, s.ex, s.ey, 1, 10, 5, 4, s.id ^ 0xC1A4ull);
      for (size_t i = 0; i < sp.size(); i++)
        if (inView(cam, sp[i].first, sp[i].second)) { add(P_CLAW, sp[i].first, sp[i].second, (int)((hashU(s.id) + i * 3) & 7), 0); S.stats.raidScars++; }
      // what the beast smashed: rubble and ash heaped by the walls it tore at
      const auto rb = findSpots(m, s.ex, s.ey, 2, 10, 3, 3, s.id ^ 0x2B1Eull);
      for (size_t i = 0; i < rb.size(); i++)
        if (inView(cam, rb[i].first, rb[i].second)) add(P_WARPROP, rb[i].first, rb[i].second, (int)(i == 1 ? art::Prop::Ash : art::Prop::Rubble), 0);
      // smoke from the roofs nearest the heart (dark, thick, drifting east)
      if (dt > 0) {
        int n = 0;
        for (const Bldg& b : m.bldgs) {
          if (b.site != (int)h || n >= 4) continue;
          if (std::abs(b.r.x - s.ex) > 14 || std::abs(b.r.y - s.ey) > 12) continue;
          n++;
          Rng r((uint32_t)(t_ * 1000.0f) * 131u + (uint32_t)b.r.x * 977u + (uint32_t)b.r.y);
          // (M12 fixer) a thick column a roof at a time (a faint wisp did not read as a town that burned)
          if (r.f() > dt * 34.0f) continue;
          Particle q;
          q.p = Vec2(b.r.x * 16.0f + b.r.w * 8.0f + r.range(-6, 6), (b.r.y + b.r.h) * 16.0f - b.r.h * 16.0f - 4 + r.range(0, 6));
          q.v = Vec2(r.range(3, 8), r.range(-17, -10));
          q.life = q.max = r.range(3.6f, 5.2f);
          const float gr = r.range(0.10f, 0.20f);
          q.c = Color(gr, gr * 0.92f, gr * 0.94f);
          q.size = r.range(4.5f, 7.5f);
          q.grav = -1.5f;
          q.kind = 2;
          parts_.push_back(q);
        }
      }
    }
  }
}

// ---------------------------------------------------------------- drawing
namespace {
const Tex& worldTex(View::RiseViewState& S, Pix& P, uint64_t key, const std::function<Canvas()>& make) {
  auto it = S.worldTex.find(key);
  if (it != S.worldTex.end()) return it->second;
  return S.worldTex[key] = P.bake(make());
}
}  // namespace

void View::drawRiseGround(Game& g, Vec2 cam) {
  (void)g;
  RiseViewState& S = RiseViewOps::st(*this);
  Pix& P = *pix_;
  for (const RiseViewState::Piece& p : S.flats) {
    if (p.kind == P_ASH) {   // (M12 fixer r2) the burned house's scorched floor (its frame: one tile above its back row)
      const Tex& t = worldTex(S, P, 0xA5B0A700ull | (uint64_t)(p.var & 3), [&] { return art::burntShell(0, p.var & 3); });
      P.blit(t, std::floor(p.x - 8 - cam.x), std::floor(p.y - 32 - cam.y));
      continue;
    }
    const Tex& t = worldTex(S, P, 0xC1A30000ull | (uint64_t)p.var, [&] { return art::clawScar(p.var); });
    P.blit(t, std::floor(p.x - t.w / 2.0f - cam.x), std::floor(p.y - 8 - t.h / 2.0f - cam.y));
  }
}

void View::drawRiseSorted(Game& g, int idx, Vec2 cam) {
  RiseViewState& S = RiseViewOps::st(*this);
  if (idx < 0 || idx >= (int)S.pieces.size()) return;
  const RiseViewState::Piece& p = S.pieces[(size_t)idx];
  Pix& P = *pix_;
  auto blitFrame = [&](const Tex& t, int fw, int fr) {
    P.blitEx(t, fr * fw, 0, fw, t.h, std::floor(p.x - fw / 2.0f - cam.x), std::floor(p.y - t.h - cam.y), (float)fw, (float)t.h);
  };
  switch (p.kind) {
    case P_BANNER: {
      cult::Heraldry hr;
      RiseViewOps::armsOf(g, p.key, hr, nullptr);
      const uint64_t key = ew::mix64(hr.key() ^ 0x10ADBA77ull);
      const Tex& t = worldTex(S, P, key, [&] { return art::lordBannerSprite(hr); });
      const int fw = art::risePropW(art::Prop::LordBanner);
      blitFrame(t, fw, (int)(t_ * 5 + p.x * 0.13f) % art::risePropFrames(art::Prop::LordBanner));
      S.bannerCount++;
      break;
    }
    case P_PROP: {
      const art::Prop pr = (art::Prop)p.var;
      if ((size_t)pr >= props_.size()) break;
      const Tex& t = props_[(size_t)pr];
      const int fw = art::propW(pr);
      if (propShadow(pr) >= 2 && pr != art::Prop::Watchtower)
        P.blitEx(shadowBig_, 0, 0, 40, 12, p.x - 11 - cam.x, p.y - 4 - cam.y, 22, 7, false, Color(1, 1, 1, 0.7f));
      blitFrame(t, fw, 0);
      break;
    }
    case P_WARSTD: {
      uint32_t field = rgba(86, 26, 30), trim = rgba(26, 22, 26);
      if (p.key) { cult::Heraldry hr; RiseViewOps::armsOf(g, p.key, hr, nullptr); field = hr.field; trim = hr.charge ? hr.charge : trim; }
      const uint64_t key = ew::mix64(((uint64_t)field << 32 | trim) ^ 0x3A4D57ull);
      const Tex& t = worldTex(S, P, key, [&] { return art::warlordStandard(field, trim); });
      blitFrame(t, art::kWarlordStdW, (int)(t_ * 4.5f + p.x * 0.11f) % 4);
      break;
    }
    case P_TROPHY: {
      uint32_t trim = rgba(110, 30, 34);
      if (p.key) { cult::Heraldry hr; RiseViewOps::armsOf(g, p.key, hr, nullptr); trim = hr.field; }
      const Tex& t = worldTex(S, P, ew::mix64(((uint64_t)trim << 8 | (uint64_t)p.var) ^ 0x7209ull), [&] { return art::trophyPike(p.var, trim); });
      blitFrame(t, t.w, 0);
      break;
    }
    case P_STORES: {
      const Tex& t = worldTex(S, P, 0x5702E000ull | (uint64_t)p.var, [&] { return art::resettlerStores(p.var); });
      blitFrame(t, t.w, 0);
      break;
    }
    case P_SHELL: {   // (M12 fixer r2) the burned house's back (at its back row) or front (at its front row), one frame
      const int part = p.var & 15, v = (p.var >> 4) & 3;
      const Tex& t = worldTex(S, P, 0x5E11B000ull | (uint64_t)part << 4 | (uint64_t)v, [&] { return art::burntShell(part, v); });
      P.blit(t, std::floor(p.x - 8 - cam.x), std::floor(p.y - 16 - (part == 2 ? 48 : 16) - cam.y));
      break;
    }
    case P_SCAFFOLD: {
      const Tex& t = worldTex(S, P, 0x5CAFF000ull, [&] { return art::warPropSprite(art::Prop::Scaffold, 0, 0); });
      blitFrame(t, t.w, 0);
      break;
    }
    case P_WARPROP: {
      const art::Prop wp = (art::Prop)p.var;
      const Tex& t = worldTex(S, P, 0x3A4B0000ull | (uint64_t)p.var, [&] { return art::warPropSprite(wp, 0, 0); });
      blitFrame(t, t.w, 0);
      break;
    }
    case P_STAKE: {
      // (M12 fixer) the stockade's stakes joined into their run (art::palisadePiece: mask in the low bits, gateposts above)
      const int sv = (int)(hashU((uint64_t)(int64_t)p.x * 31 + (uint64_t)(int64_t)p.y) & 3);
      const Tex& t = worldTex(S, P, 0x57A4E000ull | (uint64_t)(p.var & 255) << 4 | (uint64_t)sv, [&] { return art::palisadePiece(p.var & 15, (p.var >> 4) & 15, (uint32_t)sv); });
      blitFrame(t, t.w, 0);
      if ((p.var >> 4) & 2) {   // the gateway's lintel to the east post (4 tiles on), lashed, a skull hung from it
        const float x0 = std::floor(p.x - 8 + 11 - cam.x), x1 = std::floor(p.x - 8 + 4 * 16 + 5 - cam.x);
        const float y0 = std::floor(p.y - 4 - 27 - cam.y);
        P.rect(x0 + 3, std::floor(p.y - cam.y), x1 - x0, 2, Color(0.05f, 0.03f, 0.06f, 0.16f));
        P.rect(x0, y0, x1 - x0, 1, Color(0.62f, 0.47f, 0.31f));
        P.rect(x0, y0 + 1, x1 - x0, 2, Color(0.47f, 0.33f, 0.21f));
        P.rect(x0, y0 + 3, x1 - x0, 1, Color(0.33f, 0.22f, 0.15f));
        P.rect(x0, y0 + 4, x1 - x0, 1, Color(0.20f, 0.14f, 0.10f));
        const float mx = std::floor((x0 + x1) * 0.5f);
        P.rect(mx, y0 + 5, 1, 3, Color(0.40f, 0.32f, 0.22f));        // the cord
        P.rect(mx - 2, y0 + 8, 5, 4, Color(0.86f, 0.82f, 0.72f));    // the skull
        P.rect(mx - 1, y0 + 9, 1, 1, Color(0.15f, 0.10f, 0.10f));
        P.rect(mx + 1, y0 + 9, 1, 1, Color(0.15f, 0.10f, 0.10f));
        P.rect(mx - 2, y0 + 11, 5, 1, Color(0.62f, 0.58f, 0.50f));
      }
      break;
    }
    default: break;
  }
}

void View::riseLights(Game& g, const std::function<void(Vec2, float, Color, float)>& light) {
  if (g.inside || g.mode == Mode::Title) return;
  RiseViewState& S = RiseViewOps::st(*this);
  const float dark = std::clamp(1.0f - g.daylight() * 1.3f, 0.0f, 1.0f);
  if (dark < 0.12f) return;
  // the watchtowers' lanterns in their lookouts (the props and the works the view draws) and the pool they cast
  const Map& m = g.world.over;
  const Vec2 cam(std::floor(cam_.x), std::floor(cam_.y));
  auto lantern = [&](float bx, float by, float k) {   // bx, by: the tower's base (bottom centre of its tile)
    const float f = 0.88f + 0.12f * std::sin(t_ * 7.1f + bx * 0.3f);
    const float lx = bx + 3, ly = by - art::risePropH(art::Prop::Watchtower) + 27;
    light(Vec2(lx, ly), 22, Color(1.0f, 0.82f, 0.48f), 0.95f * k * f);
    light(Vec2(lx, ly + 10), 46, Color(1.0f, 0.62f, 0.32f), 0.45f * k * f);
  };
  const int tx0 = std::max(0, (int)std::floor(cam.x / 16) - 3), ty0 = std::max(0, (int)std::floor(cam.y / 16) - 2);
  const int tx1 = std::min(m.w, tx0 + Pix::W / 16 + 7), ty1 = std::min(m.h, ty0 + Pix::H / 16 + 8);
  for (int ty = ty0; ty < ty1; ty++)
    for (int tx = tx0; tx < tx1; tx++)
      if (m.prop[(size_t)ty * m.w + tx] == (int)art::Prop::Watchtower + 1) lantern(tx * 16 + 8.0f, ty * 16 + 16.0f, dark);
  for (const RiseViewState::Piece& p : S.pieces)
    if (p.kind == P_PROP && p.var == (int)art::Prop::Watchtower) lantern(p.x, p.y, dark);
}
