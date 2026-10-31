// LifeOps (life_ops.h, TOWNSFOLK lane): plans and outdoor spots.
// planAt, the outdoor spots of a settlement (scanSpots, outdoorSpot), their reservations and the field lunches.
// Moved verbatim from life_game.cpp (SOLID 1.2); the Game::life* hooks that call these stay in life_game.cpp.
#include "rpg/sim/life_ops.h"

life::Plan LifeOps::planAt(Game& g, const life::Census& c, const life::Resident& r, double abs) {
  const int d = (int)std::floor(abs / 24.0);
  float h = (float)(abs - d * 24.0);
  return g.life.plan(g.world, c, r, d, h);
}

void LifeOps::scanSpots(Game& g, int si, RT::SiteRt& S) {
  S.scannedShift = g.world.windowShifts;
  for (auto* v : {&S.farm, &S.water, &S.wood, &S.mine, &S.pasture, &S.plaza, &S.market, &S.street, &S.lamps, &S.yard, &S.edge}) v->clear();
  const Site& st = g.world.sites[(size_t)si];
  const Map& m = g.world.over;
  const int pad = 14;
  const int x0 = std::max(1, st.r.x - pad), y0 = std::max(1, st.r.y - pad);
  const int x1 = std::min(m.w - 2, st.r.x + st.r.w + pad), y1 = std::min(m.h - 2, st.r.y + st.r.h + pad);
  auto walk = [&](int x, int y) { return m.in(x, y) && !m.blocked(x, y) && m.bldgAt[(size_t)y * m.w + x] < 0 && !m.wall[(size_t)y * m.w + x]; };
  auto propIs = [&](int x, int y, std::initializer_list<Prop> ps) {
    const int p = m.propAt(x, y);
    if (!p) return false;
    for (Prop q : ps) if ((int)q + 1 == p) return true;
    return false;
  };
  static const int dx[4] = {0, 0, 1, -1}, dy[4] = {1, -1, 0, 0};
  auto put = [&](std::vector<RT::Spot>& v, int x, int y, int face) { v.push_back({g.world.ox + x, g.world.oy + y, (int8_t)face}); };
  for (int y = y0; y <= y1; y++)
    for (int x = x0; x <= x1; x++) {
      if (!walk(x, y)) continue;
      const Ground gr = m.at(x, y);
      const bool inSite = st.r.contains(x, y);
      if (gr == Ground::Farmland) put(S.farm, x, y, 0);
      if (inSite && gr == Ground::Plaza) put(S.plaza, x, y, 0);
      if (inSite && gr == Ground::Road && ((x + y) % 2 == 0)) put(S.street, x, y, 0);
      for (int k = 0; k < 4; k++) {
        const int nx = x + dx[k], ny = y + dy[k];
        if (!m.in(nx, ny)) continue;
        const Ground ng = m.at(nx, ny);
        const int face = k == 0 ? 0 : k == 1 ? 1 : k == 2 ? 2 : 3;
        if (groundWater(ng) && gr != Ground::Bridge && m.propAt(nx, ny) == 0) put(S.water, x, y, face);
        if (propIs(nx, ny, {Prop::Woodpile, Prop::LogPile, Prop::Stump})) put(S.wood, x, y, face);
        if (propIs(nx, ny, {Prop::OrePile, Prop::OreCart, Prop::MineEntrance, Prop::MineHill})) put(S.mine, x, y, face);
        if (propIs(nx, ny, {Prop::Sheep, Prop::Cow, Prop::PenShelter, Prop::Trough, Prop::Haystack})) put(S.pasture, x, y, face);
        if (k == 1 && m.propAt(nx, ny) && art::isVendorProp((Prop)(m.propAt(nx, ny) - 1))) put(S.market, x, y, 1);
        if (k == 1 && propIs(nx, ny, {Prop::Lamppost})) put(S.lamps, x, y, 1);
      }
      if (!inSite && (x == st.r.x - 2 || x == st.r.x + st.r.w + 1 || y == st.r.y - 2 || y == st.r.y + st.r.h + 1) && (gr == Ground::Road || gr == Ground::Bridge))
        put(S.edge, x, y, 0);
    }
  // yards: open ground beside farmhouses, houses and huts (hens, goats, a dog by the door)
  for (int b = st.bldgFirst; b < st.bldgFirst + st.bldgCount && b < (int)m.bldgs.size(); b++) {
    const Bldg& B = m.bldgs[(size_t)b];
    if (B.site != si) continue;
    const bool farmy = B.type == art::Building::Farmhouse || B.type == art::Building::House || B.type == art::Building::Hut || B.type == art::Building::Granary;
    if (!farmy) continue;
    for (int y = B.r.y - 1; y <= B.r.y + B.r.h + 1; y++)
      for (int x = B.r.x - 2; x <= B.r.x + B.r.w + 1; x++) {
        if (!walk(x, y)) continue;
        const Ground gr = m.at(x, y);
        if (gr == Ground::Road || gr == Ground::Plaza) continue;
        if (x == B.doorX() && y == B.doorY() + 1) continue;
        put(S.yard, x, y, 0);
      }
  }
  // no plaza (a village green): the open tiles round the heart
  if (S.plaza.size() < 6)
    for (int y = st.ey - 4; y <= st.ey + 4; y++)
      for (int x = st.ex - 6; x <= st.ex + 6; x++)
        if (walk(x, y)) put(S.plaza, x, y, 0);
  if (S.street.size() < 6)
    for (int y = st.r.y; y < st.r.y + st.r.h; y++)
      for (int x = st.r.x; x < st.r.x + st.r.w; x++)
        if (walk(x, y) && (m.at(x, y) == Ground::Road || m.at(x, y) == Ground::Dirt) && ((x * 7 + y * 3) % 5 == 0)) put(S.street, x, y, 0);
  // (P1) only spots a body can walk to from the town: a woodpile in a yard fenced all round, a gap shut in by houses,
  // are left out (someone placed there could never leave it). A flood over the scanned area from the heart (or, when
  // the heart stands on something solid, from the nearest street spot)
  {
    const int W = x1 - x0 + 1, H = y1 - y0 + 1;
    if (W > 0 && H > 0) {
      std::vector<uint8_t> seen((size_t)W * H, 0);
      std::vector<int> q;
      q.reserve(4096);
      auto push = [&](int x, int y) {
        if (x < x0 || y < y0 || x > x1 || y > y1 || !walk(x, y)) return;
        uint8_t& s = seen[(size_t)(y - y0) * W + (x - x0)];
        if (s) return;
        s = 1;
        q.push_back((y - y0) * W + (x - x0));
      };
      push(st.ex, st.ey);
      if (q.empty())
        for (const RT::Spot& s : S.street) { push(s.x - g.world.ox, s.y - g.world.oy); if (!q.empty()) break; }
      for (size_t i = 0; i < q.size(); i++) {
        const int x = q[i] % W + x0, y = q[i] / W + y0;
        push(x + 1, y); push(x - 1, y); push(x, y + 1); push(x, y - 1);
      }
      // (a town whose heart reaches only a sliver of it is left as scanned: better a stuck few than an empty street)
      if (q.size() * 4 >= (size_t)W * H / 8)
        for (auto* v : {&S.farm, &S.water, &S.wood, &S.mine, &S.pasture, &S.plaza, &S.market, &S.street, &S.lamps, &S.yard, &S.edge})
          v->erase(std::remove_if(v->begin(), v->end(), [&](const RT::Spot& s) {
                     const int x = s.x - g.world.ox, y = s.y - g.world.oy;
                     return x < x0 || y < y0 || x > x1 || y > y1 || !seen[(size_t)(y - y0) * W + (x - x0)];
                   }),
                   v->end());
    }
  }
}

bool LifeOps::fieldJob(life::Job j) {
  return j == life::Job::Farmer || j == life::Job::Herder || j == life::Job::Woodcutter || j == life::Job::Fisher || j == life::Job::Miner;
}

// salt: (P1) the segment of the block (segOf + 1; 0 the block's first spot): a fresh spot of the same kind each segment
bool LifeOps::outdoorSpot(Game& g, int si, RT::SiteRt& S, const life::Census& c, const life::Resident& r, const life::Plan& p0,
                        int32_t& gx, int32_t& gy, int& face, int self, uint32_t salt) {
  using life::Act;
  using life::Place;
  using life::Job;
  if (p0.place == Place::Away || (r.flags & (life::RF_DEAD | life::RF_AWAY))) return false;
  // a field hand's midday meal is eaten out at the work (bread and cheese at the field's edge), not walked home for
  life::Plan p = p0;
  if (fieldLunch(g, r, p0)) { p.act = Act::Work; p.place = Place::Field; p.bldg = -1; }
  if (p.bldg >= 0 && p.act != Act::Emigrate) return false;
  const uint32_t h = mixu(c.seed ^ (uint64_t)r.idx * 7919u, (uint64_t)g.day * 31u + (uint64_t)p.act * 977u + (uint64_t)p.place + (uint64_t)salt * 0x9E3779B1ull);
  const std::vector<RT::Spot>* v = &S.street;
  switch (p.place) {
    case Place::Field:
      switch (r.job) {
        case Job::Fisher: v = &S.water; break;
        case Job::Woodcutter: v = &S.wood; break;
        case Job::Miner: v = &S.mine; break;
        case Job::Herder: v = &S.pasture; break;
        default: v = &S.farm; break;
      }
      if (v->empty()) v = !S.farm.empty() ? &S.farm : !S.pasture.empty() ? &S.pasture : &S.yard;
      if (r.job == Job::Hunter) v = !S.edge.empty() ? &S.edge : &S.street;
      break;
    case Place::Plaza: case Place::Gathering: case Place::Gathering2: case Place::Temple: v = &S.plaza; break;
    case Place::Market: v = !S.market.empty() ? &S.market : &S.plaza; break;
    case Place::Gate: v = !S.edge.empty() ? &S.edge : &S.street; break;
    case Place::Home: case Place::Work: case Place::Street: default: v = &S.street; break;
  }
  if (p.act == Act::Emigrate) v = !S.edge.empty() ? &S.edge : &S.street;
  if (p.act == Act::LightLamps && !S.lamps.empty()) v = &S.lamps;
  if (v->empty()) v = &S.plaza;
  // a home's own street: near the home door when the place is the street (labourers, homeless sleepers)
  const RT::Spot* s = nullptr;
  if ((p.place == Place::Street || p.place == Place::Home) && r.home >= 0 && !v->empty()) {
    int hx, hy;
    const life::Census& cc = c;
    const int hb = cc.bldgHandle(g.world.sites[(size_t)si], r.home);
    if (doorStep(g, hb, hx, hy)) {
      // the nearest of a few hashed candidates
      int best = 1 << 30;
      for (int k = 0; k < 8; k++) {
        const RT::Spot& q = (*v)[(h + (uint32_t)k * 40503u) % v->size()];
        if (!spotOk(g, q) || spotTaken(g, q.x, q.y, self)) continue;
        const int d = std::abs(q.x - (g.world.ox + hx)) + std::abs(q.y - (g.world.oy + hy));
        if (d < best) { best = d; s = &q; }
      }
    }
  }
  // out at the fields' and the woods' edge: never shoulder to shoulder with the next worker (a row of them on
  // neighbouring spots reads as a drilled line): the first hashed spot with nobody on the tiles round it
  // (the hunters, whose edge of town may be one short stretch of road, try the woods and the pastures next)
  const std::vector<RT::Spot>* lists[4] = {v, nullptr, nullptr, nullptr};
  if (p.place == Place::Field && r.job == Job::Hunter) { lists[1] = &S.wood; lists[2] = &S.pasture; lists[3] = &S.farm; }
  const bool spaced = p.place != Place::Market && p.act != Act::LightLamps && p.act != Act::Perform;
  for (const std::vector<RT::Spot>* vv : lists) {
    if (s || !spaced) break;
    if (!vv || vv->size() <= 2) continue;
    for (int k = 0; k < 12 && !s; k++) {
      const RT::Spot& q = (*vv)[(h + (uint32_t)k * 2654435761u) % vv->size()];
      if (!spotOk(g, q) || spotTaken(g, q.x, q.y, self)) continue;
      bool crowd = false;
      for (int oy = -1; oy <= 1 && !crowd; oy++)
        for (int ox = -2; ox <= 2 && !crowd; ox++)
          if ((ox || oy) && spotTaken(g, q.x + ox, q.y + oy, self)) crowd = true;
      if (!crowd) s = &q;
    }
  }
  if (!s) s = pick(g, *v, h, self);
  if (!s) s = pick(g, S.plaza, h, self);
  if (!s) return false;
  gx = s->x; gy = s->y; face = s->face;
  return true;
}
