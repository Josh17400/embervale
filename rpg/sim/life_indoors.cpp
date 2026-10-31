// LifeOps (life_ops.h, TOWNSFOLK lane): interiors.
// Furniture scan, seats, beds and stations, populate and placeIndoors by schedule, service, chat pairs, interiorFolk.
// Moved verbatim from life_game.cpp (SOLID 1.2); the Game::life* hooks that call these stay in life_game.cpp.
#include "rpg/sim/life_ops.h"

// ---------------------------------------------------------------- interiors
void LifeOps::scanInterior(Game& g) {
  RT& R = rt(g);
  RT::Interior& I = R.in;
  I = RT::Interior();
  I.key = g.mapKey();
  const Map& m = g.sub;
  auto room = [&](int x, int y) { return m.roomIndexAt(x, y); };
  static const int dx[4] = {0, 0, 1, -1}, dy[4] = {1, -1, 0, 0};
  // ((M7c fixer) the floor the player can walk to from the way in (the door, the stairs): a worker's station tile
  // that the furniture walls off (seed 7's start smithy: the anvil's side between the quench tub, a barrel, the
  // bench and the wall) is never a post, or the smith stands where nobody can reach her to talk or trade)
  std::vector<uint8_t> reach((size_t)m.w * m.h, 0);
  {
    std::vector<int> q;
    auto seedAt = [&](int x, int y) {
      if (!m.in(x, y) || reach[(size_t)y * m.w + x]) return;
      reach[(size_t)y * m.w + x] = 1;
      q.push_back(y * m.w + x);
    };
    if (m.exitY >= 0) {
      seedAt(m.exitX, m.exitY);
      for (int16_t e : m.exits) seedAt(e, m.exitY);
    }
    for (const Stairs* st : {&m.up, &m.down})
      if (st->valid())
        for (int k = 0; k < 4; k++)
          if (m.in(st->x + dx[k], st->y + dy[k]) && !m.blocked(st->x + dx[k], st->y + dy[k])) seedAt(st->x + dx[k], st->y + dy[k]);
    if (q.empty()) std::fill(reach.begin(), reach.end(), (uint8_t)1);   // (no way in known: no filter)
    for (size_t h = 0; h < q.size(); h++) {
      const int x = q[h] % m.w, y = q[h] / m.w;
      for (int k = 0; k < 4; k++) {
        const int nx = x + dx[k], ny = y + dy[k];
        if (m.in(nx, ny) && !m.blocked(nx, ny)) seedAt(nx, ny);
      }
    }
  }
  for (int y = 1; y < m.h - 1; y++)
    for (int x = 1; x < m.w - 1; x++) {
      const int pr = m.propAt(x, y);
      if (!pr) {
        // a bath's pool (the water of a ground floor inside): a bather may stand in it (never at its rim row: the
        // coping is drawn there), nobody else
        if (groundWater(m.at(x, y))) {
          if (m.floor == 0 && groundWater(m.at(x, y - 1))) I.poolTiles.push_back(y * m.w + x);
          continue;
        }
        if (!m.blocked(x, y) && !m.isExit(x, y)) I.floorTiles.push_back(y * m.w + x);
        continue;
      }
      const Prop p = (Prop)(pr - 1);
      if (seatProp(p)) {
        // facing the table beside it (a pew faces the altar: north)
        int face = -1;
        for (int k = 0; k < 4; k++) {
          const int q = m.propAt(x + dx[k], y + dy[k]);
          if (q && tableProp((Prop)(q - 1))) { face = k == 0 ? 0 : k == 1 ? 1 : k == 2 ? 2 : 3; break; }
        }
        // (M5 fixer r2) no table: a pew faces the altar (north); a bench by a bath faces its pool; any other seat faces
        // the middle of its room (a bench against the north wall faces down into the room, never into a bare wall)
        if (face < 0) {
          const int ri = room(x, y);
          const RoomKind rk = roomKind(m, ri);
          if (rk == RoomKind::Nave) face = 1;
          else {
            float tx = -1, ty = -1, bd = 1e9f;
            for (int yy = std::max(0, y - 6); yy <= std::min(m.h - 1, y + 6); yy++)
              for (int xx = std::max(0, x - 6); xx <= std::min(m.w - 1, x + 6); xx++)
                if (groundWater(m.at(xx, yy))) {
                  const float d = (float)((xx - x) * (xx - x) + (yy - y) * (yy - y));
                  if (d < bd) { bd = d; tx = (float)xx; ty = (float)yy; }
                }
            if (tx < 0 && ri >= 0 && ri < (int)m.rooms.size()) {
              const IRect& rr = m.rooms[(size_t)ri].r;
              tx = rr.x + (rr.w - 1) * 0.5f; ty = rr.y + (rr.h - 1) * 0.5f;
            }
            if (tx < 0) face = 0;
            else {
              const float ddx = tx - x, ddy = ty - y;
              if (std::fabs(ddx) > std::fabs(ddy) + 0.5f) face = ddx > 0 ? 2 : 3;
              else face = ddy < -0.5f ? 1 : 0;
            }
          }
        }
        I.seats.push_back({x, y, p, room(x, y), (int8_t)face});
      } else if (bedProp(p)) {
        I.beds.push_back({x, y, p, room(x, y), 0});
      } else if (p == Prop::Anvil || p == Prop::Forge || p == Prop::Oven || p == Prop::Cauldron || p == Prop::Loom || p == Prop::SpinningWheel ||
                 p == Prop::Lectern || p == Prop::Desk || p == Prop::Altar || p == Prop::Workbench || p == Prop::Grindstone || p == Prop::PrepTable ||
                 p == Prop::Bookshelf || p == Prop::Hearth || counterProp(p) || p == Prop::Stove || p == Prop::FirePitM) {
        // the standing tile: free floor beside it, the worker facing it (below it first: the 3/4 view shows the work)
        // (a free-standing piece, the anvil or a wheel, is worked from its side so the piece and the work both show)
        static const int pref[4][3] = {{0, 1, 1}, {1, 0, 3}, {-1, 0, 2}, {0, -1, 0}};
        static const int side[4][3] = {{-1, 0, 2}, {1, 0, 3}, {0, 1, 1}, {0, -1, 0}};
        const bool freeStanding = p == Prop::Anvil || p == Prop::Grindstone || p == Prop::SpinningWheel || p == Prop::Cauldron;
        for (int k = 0; k < 4; k++) {
          const int (&pk)[3] = freeStanding ? side[k] : pref[k];
          const int sx = x + pk[0], sy = y + pk[1];
          if (!m.in(sx, sy) || m.blocked(sx, sy) || m.propAt(sx, sy) || m.isExit(sx, sy)) continue;
          if (!reach[(size_t)sy * m.w + sx]) continue;   // (walled off by the furniture: no post there)
          // a bar is worked from behind (its north side), never from the customers' side
          if (counterProp(p) && pk[1] != -1) continue;
          I.stations.push_back({sx, sy, p, room(sx, sy), (int8_t)pk[2]});
          // (a free-standing piece keeps every free side, best first: when one is taken the worker still stands at
          //  the piece, never at the next prop in the room)
          if (!freeStanding) break;
        }
      }
    }
  // floor seats: the open floor round a low table, a long fire trench or a yurt's stove is sat on cross-legged (a
  // feast tent, a tea room without cushions)
  for (int y = 1; y < m.h - 1; y++)
    for (int x = 1; x < m.w - 1; x++) {
      const int pr = m.propAt(x, y);
      if (!pr) continue;
      const Prop p = (Prop)(pr - 1);
      if (!(p == Prop::LowTable || p == Prop::FirePitL || p == Prop::FirePitM || p == Prop::FirePitR || p == Prop::Stove)) continue;
      for (int k = 0; k < 4; k++) {
        const int sx = x + dx[k], sy = y + dy[k];
        if (!m.in(sx, sy) || m.blocked(sx, sy) || m.propAt(sx, sy) || m.isExit(sx, sy)) continue;
        // (never behind a fire or a stove: in the 3/4 view the flames and the flue would stand in front of the sitter)
        // (a long hearth's fires burn at its segments' joins, between the places: its far side is sat at too, the
        //  patrons facing the room across the embers)
        if (k == 1 && p != Prop::LowTable && p != Prop::FirePitL && p != Prop::FirePitM && p != Prop::FirePitR) continue;
        bool dup = false;
        for (const RT::Furn& s : I.seats) if (s.x == sx && s.y == sy) dup = true;
        if (dup) continue;
        // facing the table / the fire (k: the seat lies below, above, right, left of it)
        const int face = k == 0 ? 1 : k == 1 ? 0 : k == 2 ? 3 : 2;
        I.seats.push_back({sx, sy, Prop::COUNT, room(sx, sy), (int8_t)face});
      }
    }
}

bool LifeOps::inRentedRoom(const Game& g, int x, int y) {
  return g.lodgingActive() && g.lodging.bldg == g.subBldg && g.lodging.floor == g.subFloor && g.sub.roomIndexAt(x, y) == g.lodging.room;
}

bool LifeOps::tileFree(Game& g, int x, int y) {
  const Map& m = g.sub;
  if (!m.in(x, y) || m.blocked(x, y) || m.isExit(x, y) || inRentedRoom(g, x, y)) return false;
  const int pr = m.propAt(x, y);
  if (pr && ((Prop)(pr - 1) == Prop::StairsUp || (Prop)(pr - 1) == Prop::StairsDown || (Prop)(pr - 1) == Prop::DoorH || (Prop)(pr - 1) == Prop::DoorV)) return false;
  for (size_t k = 1; k < g.actors.size(); k++)
    if (tileX(g.actors[k].p) == x && tileY(g.actors[k].p) == y && g.actors[k].st != AState::Dead) return false;
  return true;
}

// a standing body here would be drawn through someone seated: the seat's own tile (a sitter's body is drawn off its
// tile centre, so the actor scan alone misses it) or the tile straight above or below a taken seat
bool LifeOps::bySitter(Game& g, int x, int y) {
  for (const RT::Furn& s : rt(g).in.seats)
    if (s.x == x && std::abs(s.y - y) <= 1 && isUsed(g, s.x, s.y)) return true;
  return false;
}

// (M5 fixer r2) the bard keeps a clear ring: nobody sits or stands on a tile next to a performer (a body a step behind
// the bard hid the lute and most of the player at 1x)
bool LifeOps::nearPerformer(const Game& g, const Actor& a, int x, int y) {
  for (size_t k = 1; k < g.actors.size(); k++) {
    const Actor& o = g.actors[k];
    if (o.id == a.id || o.st == AState::Dead) continue;
    if (o.posture != Posture::Lute && o.posture != Posture::Flute && o.posture != Posture::Drum) continue;
    if (std::abs(tileX(o.p) - x) <= 1 && std::abs(tileY(o.p) - y) <= 1) return true;
  }
  return false;
}

void LifeOps::placeAt(Actor& a, int x, int y, int face) {
  a.p = tileCentre(x, y);
  a.home = a.p; a.goal = a.p;
  a.face = face;
  a.st = AState::Idle;
}

// sit an actor on a free seat (in rooms of the given kinds, near (nx, ny) when given); false: none free
bool LifeOps::sitDown(Game& g, Actor& a, std::initializer_list<RoomKind> kinds, Posture post, int nx, int ny) {
  RT& R = rt(g);
  int best = -1, bd = 1 << 30;
  for (size_t i = 0; i < R.in.seats.size(); i++) {
    const RT::Furn& s = R.in.seats[i];
    bool ok = kinds.size() == 0;
    for (RoomKind k : kinds) if (roomKind(g.sub, s.room) == k) ok = true;
    if (!ok || isUsed(g, s.x, s.y) || !tileFree(g, s.x, s.y) || nearPerformer(g, a, s.x, s.y)) continue;
    int d = nx >= 0 ? std::abs(s.x - nx) + std::abs(s.y - ny) : (int)(mixu((uint64_t)a.id, i) % 97u);
    // a chair is sat on facing its table (never into its back: art::seatFit); one that would face away goes last
    int ax, ay;
    if (!seatAt(s.p, s.face, ax, ay)) d += 1000;
    // a real seat first (a chair, a bench, a cushion), the bare floor by a fire last
    if (s.p == Prop::COUNT) d += 300;
    if (d < bd) { bd = d; best = (int)i; }
  }
  if (best < 0) return false;
  const RT::Furn& s = R.in.seats[(size_t)best];
  reserve(g, a, s.x, s.y);
  int face = s.face;
  int fax = 0, fay = 0;
  if (!seatAt(s.p, face, fax, fay)) { face = 0; seatAt(s.p, face, fax, fay); }
  // (M5 fixer r2) on the bench before a long hearth the patrons sit on its front edge, leaning in to the warmth: their
  // backs no longer hide the whole trench (the embers and the logs show over their shoulders, not just the flames)
  if (s.p == Prop::Bench && face == 1 && g.sub.in(s.x, s.y - 1)) {
    const int up = g.sub.propAt(s.x, s.y - 1);
    if (up == (int)Prop::FirePitL + 1 || up == (int)Prop::FirePitM + 1 || up == (int)Prop::FirePitR + 1) fay += 5;
  }
  placeAt(a, s.x, s.y, face);
  // the seat line on the seat's surface (art::seatFit: p from the seat tile's bottom-centre)
  a.p = Vec2(s.x * TILE + 8.0f + fax, s.y * TILE + 16.0f + fay);
  a.home = a.p; a.goal = a.p;
  a.posture = seatedPost(s.p, post);
  a.postureT = (float)(mixu(a.id, 7) % 100u) * 0.01f;
  return true;
}

bool LifeOps::standAt(Game& g, Actor& a, Prop want, Posture post, std::initializer_list<Prop> alt) {
  RT& R = rt(g);
  for (int pass = 0; pass < 2; pass++)
    for (const RT::Furn& s : R.in.stations) {
      bool ok = pass == 0 ? s.p == want : false;
      if (pass == 1) for (Prop q : alt) if (s.p == q) ok = true;
      if (!ok || isUsed(g, s.x, s.y) || !tileFree(g, s.x, s.y)) continue;
      reserve(g, a, s.x, s.y);
      placeAt(a, s.x, s.y, s.face);
      a.posture = post;
      return true;
    }
  return false;
}

bool LifeOps::lieDown(Game& g, Actor& a, int bedIdx) {
  RT& R = rt(g);
  if (bedIdx < 0 || bedIdx >= (int)R.in.beds.size()) return false;
  const RT::Furn& b = R.in.beds[(size_t)bedIdx];
  if (isUsed(g, b.x, b.y) || inRentedRoom(g, b.x, b.y)) return false;
  reserve(g, a, b.x, b.y);
  // the head on the pillow (art::bedFit: the generic Sleep cell's p from the berth's anchor tile bottom-centre; a
  // two-tile bed has its head piece (Filler) on the tile above)
  const bool longBed = g.sub.propAt(b.x, b.y - 1) == (int)Prop::Filler + 1;
  a.p = Vec2(b.x * TILE + 8.0f, b.y * TILE + 16.0f + (float)sleepAy(b.p, longBed, kitOf(g)));
  a.home = a.p; a.goal = a.p;
  a.face = 0;
  a.st = AState::Idle;
  a.posture = Posture::Sleep;
  a.postureT = 0;
  bubble(a, Bubble::Zzz, 2.0f + (float)(mixu(a.id, 3) % 300u) * 0.01f);
  return true;
}

bool LifeOps::standFree(Game& g, Actor& a, std::initializer_list<RoomKind> kinds, int nx, int ny, int maxD) {
  RT& R = rt(g);
  int best = -1, bd = 1 << 30;
  for (int t : R.in.floorTiles) {
    const int x = t % g.sub.w, y = t / g.sub.w;
    const int ri = g.sub.roomIndexAt(x, y);
    bool ok = kinds.size() == 0;
    for (RoomKind k : kinds) if (roomKind(g.sub, ri) == k) ok = true;
    if (!ok || !tileFree(g, x, y) || isUsed(g, x, y) || bySitter(g, x, y) || nearPerformer(g, a, x, y)) continue;
    // a body clear of the walls and furniture beside it
    if (!g.bodyFree(tileCentre(x, y), a.radius + 2.0f, false)) continue;
    if (nx >= 0 && std::abs(x - nx) + std::abs(y - ny) > maxD) continue;
    // near the anchor when there is one, else anywhere (by hash); never in a huddle (a body a step from another)
    int d = nx >= 0 ? (std::abs(x - nx) + std::abs(y - ny)) * 1000 + (int)(mixu((uint64_t)a.id, (uint64_t)t) % 1000u)
                    : (int)(mixu((uint64_t)a.id, (uint64_t)t) % 100000u);
    for (size_t k = 1; k < g.actors.size(); k++) {
      const Actor& o = g.actors[k];
      if (o.id == a.id || o.st == AState::Dead) continue;
      if (std::abs(tileX(o.p) - x) <= 1 && std::abs(tileY(o.p) - y) <= 1) d += 200000;
      // the keeper's counter stays clear for customers (the player talks to them from in front of it)
      if ((o.lifeBits & LB_KEY) && std::abs(tileX(o.p) - x) <= 2 && std::abs(tileY(o.p) - y) <= 3) d += 400000;
    }
    if (d < bd) { bd = d; best = t; }
  }
  if (best < 0) return false;
  placeAt(a, best % g.sub.w, best / g.sub.w, 0);
  return true;
}

// (M5 fixer r2) a bather: standing chest-deep in the pool (the view draws the body under the waterline), one to a
// tile and never a step from another bather, facing the nearest bather or the room
bool LifeOps::bathe(Game& g, Actor& a) {
  RT& R = rt(g);
  int best = -1, bd = 1 << 30;
  for (int t : R.in.poolTiles) {
    const int x = t % g.sub.w, y = t / g.sub.w;
    if (isUsed(g, x, y)) continue;
    bool clear = true;
    for (size_t k = 1; k < g.actors.size() && clear; k++) {
      const Actor& o = g.actors[k];
      if (o.id == a.id || o.st == AState::Dead) continue;
      if (std::abs(tileX(o.p) - x) <= 1 && std::abs(tileY(o.p) - y) <= 1) clear = false;
    }
    if (!clear) continue;
    const int d = (int)(mixu((uint64_t)a.id, (uint64_t)t) % 1000u);
    if (d < bd) { bd = d; best = t; }
  }
  if (best < 0) return false;
  const int x = best % g.sub.w, y = best / g.sub.w;
  reserve(g, a, x, y);
  a.useX = -1; a.useY = -1;   // (not furniture: the view finds the water under the feet)
  placeAt(a, x, y, 0);
  float bdd = 1e9f;
  for (size_t k = 1; k < g.actors.size(); k++) {
    const Actor& o = g.actors[k];
    if (o.id == a.id || !o.npc || o.st == AState::Dead) continue;
    const float d = len2(o.p - a.p);
    if (d < bdd && d < (5.0f * TILE) * (5.0f * TILE)) { bdd = d; a.face = faceOf(o.p - a.p); }
  }
  a.posture = Posture::None;
  return true;
}

// stand a step beside another standing patron who has no one to talk to (the two face each other)
bool LifeOps::standBeside(Game& g, Actor& a) {
  static const int ox[4] = {1, -1, 0, 0}, oy[4] = {0, 0, 1, -1};
  for (size_t k = 1; k < g.actors.size(); k++) {
    const Actor& o = g.actors[k];
    if (o.id == a.id || !(o.lifeBits & LB_LIFE) || o.posture != Posture::Drink) continue;
    int mates = 0;
    for (size_t j = 1; j < g.actors.size(); j++)
      if (j != k && g.actors[j].id != a.id && len2(g.actors[j].p - o.p) < (1.6f * TILE) * (1.6f * TILE)) mates++;
    if (mates) continue;   // a pair already
    const int x0 = tileX(o.p), y0 = tileY(o.p);
    for (int d = 0; d < 4; d++) {
      const int x = x0 + ox[d], y = y0 + oy[d];
      if (!tileFree(g, x, y) || g.sub.propAt(x, y) || isUsed(g, x, y) || bySitter(g, x, y) || nearPerformer(g, a, x, y) ||
          !g.bodyFree(tileCentre(x, y), a.radius + 1.0f, false)) continue;
      placeAt(a, x, y, faceTo(x0 - x, y0 - y));
      return true;
    }
  }
  return false;
}

// beds of a home's floor, counted once per (building, floor) (the household is shared out over the floors in order)
int LifeOps::bedsOnFloor(Game& g, int bi, int floor) {
  RT& R = rt(g);
  const Bldg& B = g.world.over.bldgs[(size_t)bi];
  const uint64_t key = (B.id ? B.id : (uint64_t)B.seed) * 8u + (uint64_t)floor;
  auto it = R.bedsOnFloor.find(key);
  if (it != R.bedsOnFloor.end()) return it->second;
  int n = 0;
  if (floor == g.subFloor && bi == g.subBldg) {
    for (const RT::Furn& b : R.in.beds) n += roomHomeBed(roomKind(g.sub, b.room)) ? 1 : 0;
  } else {
    Map m;
    genInterior(m, B, B.seed, floor);
    for (int y = 0; y < m.h; y++)
      for (int x = 0; x < m.w; x++) {
        const int pr = m.propAt(x, y);
        if (pr && bedProp((Prop)(pr - 1)) && roomHomeBed(roomKind(m, m.roomIndexAt(x, y)))) n++;
      }
  }
  R.bedsOnFloor[key] = n;
  return n;
}

// the people of the building the player is in, by the clock (loadMapActors has made the generator's spawns; their
// key person was kept or dropped by lifeSpawned)
void LifeOps::populate(Game& g) {
  RT& R = rt(g);
  // the generator spawns whose residents are elsewhere go
  for (size_t k = g.actors.size(); k-- > 1;)
    if (g.actors[k].lifeBits & LB_DROP) { g.actors.erase(g.actors.begin() + (std::ptrdiff_t)k); }
  // reservations of this map start over
  R.used.clear();
  scanInterior(g);
  const int bi = g.subBldg;
  if (bi < 0) return;
  const Bldg& B = g.world.over.bldgs[(size_t)bi];
  const int si = B.site;
  life::Census* c = censusOfSite(g, si);
  // a census just built has not had its first aggregate hour yet (its needs, and so its plans, move then): run it now
  if (c && c->lastHour != g.day * 24 + (int)g.hour) g.life.tick(g, 0.0f);
  // the key person and the travellers first (they hold their places)
  for (size_t k = 1; k < g.actors.size(); k++) {
    Actor& a = g.actors[k];
    if (a.lifeBits & LB_KEY) keyPost(g, a, c, B);
    else if (a.lifeBits & LB_GUEST) guestPlace(g, a);
  }
  if (!c || !g.world.sites[(size_t)si].settlement()) return;
  const Site& st = g.world.sites[(size_t)si];
  const int off = bi - st.bldgFirst;
  if (off < 0 || off >= st.bldgCount) return;
  if (warEmpty(g, si) || B.charred == 2) return;
  RT::SiteRt& S = siteRt(g, si);
  refreshPlans(g, S, *c);
  R.in.stamp = S.planStamp;
  const bool gatherHere = off == c->gathering || off == c->gathering2;
  // who is here this hour, in census order
  std::vector<int> here;
  for (size_t i = 0; i < c->res.size() && i < (size_t)LIFE_SLOTN; i++) {
    const life::Resident& r = c->res[i];
    if (r.flags & (life::RF_DEAD | life::RF_AWAY)) continue;
    if (r.job == life::Job::Guard) continue;
    if (workKey(r) && r.keyBldg == off && r.actor >= 0 && g.findActor(r.actor) > 0) continue;   // the keeper at their post (kept above)
    if (S.plans[i].bldg != off) continue;
    here.push_back((int)i);
  }
  // the staff and the bard take their places first (a full hall never leaves the bard outside)
  std::stable_partition(here.begin(), here.end(), [&](int i) {
    const life::Act a = S.plans[(size_t)i].act;
    return a == life::Act::Work || a == life::Act::Perform;
  });
  // sleepers are shared out over the floors' beds in census order (the household's own beds; the inn's staff in
  // the keeper's room, never a guest room)
  int sleepersBefore = 0;
  for (int f = 0; f < g.subFloor; f++) sleepersBefore += bedsOnFloor(g, bi, f);
  std::vector<int> myBeds;
  for (size_t b = 0; b < R.in.beds.size(); b++) if (roomHomeBed(roomKind(g.sub, R.in.beds[b].room))) myBeds.push_back((int)b);
  int sleeperN = 0, placed = 0;
  for (int idx : here) {
    const life::Resident& r = c->res[(size_t)idx];
    const life::Plan& p = S.plans[(size_t)idx];
    if (p.act == life::Act::Sleep) {
      const int n = sleeperN++;
      if (n < sleepersBefore || n - sleepersBefore >= (int)myBeds.size()) continue;   // asleep on another floor
      Actor& a = g.actors[(size_t)g.findActor(spawnResident(g, si, *c, idx, tileCentre(g.sub.exitX, g.sub.exitY - 1), true))];
      if (!lieDown(g, a, myBeds[(size_t)(n - sleepersBefore)])) { a.lifeBits |= LB_GONE; continue; }
      placed++;
      continue;
    }
    if (g.subFloor != 0) continue;   // the day's life is on the ground floor
    if (placed >= 18) break;
    const int id = spawnResident(g, si, *c, idx, tileCentre(g.sub.exitX, g.sub.exitY - 1), true);
    Actor& a = g.actors[(size_t)g.findActor(id)];
    if (!placeIndoors(g, a, *c, r, p, gatherHere)) { a.lifeBits |= LB_GONE; continue; }
    placed++;
  }
  // (fixer M5 r3, review: "village inns are near-empty in the evening while the mood is CONTENT") a content town's
  // gathering place fills of an evening even where few have it in their plans (a village's handful of regulars): its
  // free neighbours drop in for a drink (those at home or about, never the sleeping, the working or anyone out in the
  // street), and somebody takes up the culture's instrument when no bard is playing
  const bool evening = g.hour >= 18.0f && g.hour < 23.5f;
  const bool innHere = B.type == art::Building::Inn;   // (the inn is a gathering place too, whatever the culture's own)
  if ((gatherHere || innHere) && g.subFloor == 0 && evening && placed < 18 && (c->moodFlags & life::MF_CONTENT) &&
      !(c->moodFlags & (life::MF_HUNGRY | life::MF_FAMINE | life::MF_WARTORN | life::MF_EMIGRATING))) {
    int patrons = 0;
    bool bard = false;
    for (size_t k = 1; k < g.actors.size(); k++) {
      const Actor& o = g.actors[k];
      if (!(o.lifeBits & LB_LIFE) || (o.lifeBits & LB_GONE)) continue;
      auto it = R.act.find(o.id);
      if (it == R.act.end()) continue;
      const life::Act oa = it->second.act;
      if (oa == life::Act::Perform) bard = true;
      if (oa == life::Act::Tavern || oa == life::Act::Socialise || oa == life::Act::Eat || oa == life::Act::Brawl) patrons++;
    }
    const int target = std::clamp((int)R.in.seats.size() * 2 / 3, 6, 12);
    const int n = (int)c->res.size();
    const int start = n ? (int)(mixu(c->seed ^ 0xA1E5ull, (uint64_t)g.day) % (uint32_t)n) : 0;
    // (a quiet town abed early still has its night owls: when the free ones run short, the sociable who were
    //  for bed stay up a while longer, until 23:00)
    bool owls = false;
    auto freeTonight = [&](int i) {
      const life::Resident& r = c->res[(size_t)i];
      if (i >= LIFE_SLOTN || (r.flags & (life::RF_DEAD | life::RF_AWAY | life::RF_GRIEVING))) return false;
      if (r.job == life::Job::Guard || r.job == life::Job::Child || r.job == life::Job::Noble || r.job == life::Job::Beggar) return false;
      if (r.actor >= 0 && g.findActor(r.actor) > 0) return false;   // out in the street, or already here
      const life::Act pa = S.plans[(size_t)i].act;
      if (S.plans[(size_t)i].bldg == off) return false;             // already placed above
      if (owls && pa == life::Act::Sleep && g.hour < 23.0f && (r.traits & life::TR_SOCIABLE) && r.job != life::Job::Elder) return true;
      return pa == life::Act::Home || pa == life::Act::Wander || pa == life::Act::Socialise || pa == life::Act::Eat;
    };
    auto drop = [&](int i, life::Act act) {
      life::Plan p2;
      p2.act = act; p2.place = life::Place::Gathering; p2.bldg = (int16_t)off;
      const int id = spawnResident(g, si, *c, i, tileCentre(g.sub.exitX, g.sub.exitY - 1), true);
      const int k = g.findActor(id);
      if (k <= 0) return false;
      Actor& a = g.actors[(size_t)k];
      if (!placeIndoors(g, a, *c, c->res[(size_t)i], p2, true)) { a.lifeBits |= LB_GONE; return false; }
      R.rt(a.id).dropIn = true;
      placed++;
      return true;
    };
    for (int round = 0; round < 2; round++, owls = true) {
    if (!bard)   // the musician: the town's bard if free, else a sociable soul with an instrument
      for (int pass = 0; pass < 2 && !bard; pass++)
        for (int j = 0; j < n && !bard; j++) {
          const int i = (start + j) % n;
          if (!freeTonight(i)) continue;
          const life::Resident& r = c->res[(size_t)i];
          if (pass == 0 ? r.job != life::Job::Bard : !(r.traits & life::TR_SOCIABLE)) continue;
          bard = drop(i, life::Act::Perform);
        }
    for (int j = 0; j < n && patrons < target && placed < 18; j++) {
      const int i = (start + j) % n;
      if (!freeTonight(i)) continue;
      if (drop(i, life::Act::Tavern)) patrons++;
    }
    }
  }
  // drop the ones that found no place
  for (size_t k = g.actors.size(); k-- > 1;)
    if (g.actors[k].lifeBits & LB_GONE) removeActorAt(g, k);
  pairChats(g);
}

// where a resident is in the room by what they do (sit, work at a station, perform, stand about)
bool LifeOps::placeIndoors(Game& g, Actor& a, life::Census& c, const life::Resident& r, const life::Plan& p, bool gatherHere) {
  using life::Act;
  using life::Job;
  RT& R = rt(g);
  RT::ARt& A = R.rt(a.id);
  A.act = p.act; A.place = p.place; A.pbldg = p.bldg; A.kind = T_SPOT; A.arrived = true;
  a.posture = Posture::None;
  switch (p.act) {
    case Act::Perform: {
      // the bard: a free spot in the hall near its fire, facing the room, the culture's instrument
      int hx = -1, hy = -1;
      for (const RT::Furn& s : R.in.stations) if (s.p == Prop::Hearth || s.p == Prop::FirePitM || s.p == Prop::Stove) { hx = s.x; hy = s.y; break; }
      if (hx < 0) { hx = g.sub.w / 2; hy = g.sub.h / 2; }
      if (!standFree(g, a, {RoomKind::Common, RoomKind::Feast, RoomKind::TeaRoom, RoomKind::Court, RoomKind::Bath, RoomKind::Hall}, hx, hy + 1) &&
          !standFree(g, a, {}, hx, hy + 1))
        return false;
      a.face = 0;
      a.posture = bardPosture(g.world.cultureOf(a.site));
      return true;
    }
    case Act::Tavern: case Act::Socialise: case Act::Bathe: {
      // (M5 fixer r2) a bathhouse's evening is spent in the water: bathers first, then the benches round the pool
      if (!R.in.poolTiles.empty() && (p.act == Act::Bathe || (mixu((uint64_t)a.id, (uint64_t)g.day + 0xBA7Eull) % 5u) < 3) && bathe(g, a))
        return true;
      const Posture post = p.act == Act::Bathe ? Posture::Sit : ((mixu((uint64_t)a.id, (uint64_t)g.day) % 4u) == 0 ? Posture::SitEat : Posture::SitDrink);
      if (sitDown(g, a, {RoomKind::Common, RoomKind::Feast, RoomKind::TeaRoom, RoomKind::Bath, RoomKind::Court, RoomKind::Parlour, RoomKind::Hall, RoomKind::ThroneHall, RoomKind::Changing}, post))
        return true;
      // standing about the hall with a drink: beside someone else standing (a pair, facing), else anywhere in it
      if (!standBeside(g, a) &&
          !standFree(g, a, {RoomKind::Common, RoomKind::Feast, RoomKind::TeaRoom, RoomKind::Bath, RoomKind::Court, RoomKind::Hall, RoomKind::Changing}, -1, -1))
        return false;
      a.posture = Posture::Drink;
      return true;
    }
    case Act::Eat: {
      const Posture post = Posture::SitEat;
      if (sitDown(g, a, gatherHere ? std::initializer_list<RoomKind>{RoomKind::Common, RoomKind::Feast, RoomKind::TeaRoom, RoomKind::Hall}
                                   : std::initializer_list<RoomKind>{RoomKind::Hall, RoomKind::Cottage, RoomKind::Kitchen, RoomKind::Common, RoomKind::Feast},
                  post))
        return true;
      if (!standFree(g, a, {}, -1, -1)) return false;
      a.posture = Posture::Eat;
      return true;
    }
    case Act::Pray: {
      if (sitDown(g, a, {RoomKind::Nave, RoomKind::Court}, Posture::Pray)) { a.face = 1; return true; }
      if (!standFree(g, a, {RoomKind::Nave, RoomKind::Court}, -1, -1) && !standFree(g, a, {}, -1, -1)) return false;
      a.face = 1;
      a.posture = Posture::Pray;
      return true;
    }
    case Act::Work: {
      bool ok = false;
      switch (r.job) {
        case Job::Smith: ok = standAt(g, a, Prop::Anvil, Posture::Hammer, {Prop::Grindstone, Prop::Forge}); break;
        case Job::Baker: ok = standAt(g, a, Prop::Oven, Posture::Stir, {Prop::PrepTable, Prop::Cauldron, Prop::Hearth}); break;
        case Job::Innkeeper: ok = standAt(g, a, Prop::CounterM, Posture::None, {Prop::CounterL, Prop::CounterR, Prop::Counter}); break;
        case Job::Server: {
          // the server keeps the tables (sweeping, wiping) and brings the mugs round (serve); the bar is the keeper's
          a.lifeBits |= LB_SERVER;
          RT& RR = rt(g);
          if (!RR.in.seats.empty()) {
            const RT::Furn& s = RR.in.seats[mixu((uint64_t)a.id, (uint64_t)g.day) % RR.in.seats.size()];
            ok = standFree(g, a, {RoomKind::Common, RoomKind::Feast, RoomKind::TeaRoom, RoomKind::Hall, RoomKind::Bath, RoomKind::Court}, s.x, s.y, 3);
            if (ok) a.posture = Posture::Sweep;
          }
          if (!ok) ok = standAt(g, a, Prop::CounterM, Posture::None, {Prop::CounterL, Prop::CounterR, Prop::Counter, Prop::Cauldron, Prop::Oven});
          break;
        }
        case Job::Tailor: ok = standAt(g, a, Prop::Loom, Posture::None, {Prop::SpinningWheel, Prop::Workbench, Prop::Counter}); break;
        case Job::Scholar: ok = standAt(g, a, Prop::Lectern, Posture::Read, {Prop::Desk, Prop::Bookshelf}); break;
        case Job::Priest: ok = standAt(g, a, Prop::Altar, Posture::Pray, {Prop::Lectern}); break;
        case Job::Merchant: ok = standAt(g, a, Prop::CounterM, Posture::None, {Prop::CounterL, Prop::CounterR, Prop::Counter, Prop::Desk}); break;
        case Job::Miner: case Job::Woodcutter: ok = standAt(g, a, Prop::Workbench, Posture::Chop, {Prop::Grindstone}); break;
        case Job::Noble: ok = sitDown(g, a, {RoomKind::ThroneHall, RoomKind::Council, RoomKind::Assembly, RoomKind::Hall}, Posture::Sit); break;
        default: break;
      }
      if (ok) return true;
      // sweeping: servants, stablehands, apprentices without a station
      if (!standFree(g, a, {}, -1, -1)) return false;
      a.posture = (r.job == Job::Servant || r.job == Job::Server || r.job == Job::Stablehand || r.job == Job::Innkeeper || r.job == Job::Merchant)
                      ? Posture::Sweep : Posture::None;
      return true;
    }
    case Act::Home: case Act::Wander: case Act::Play: default: {
      // at home in the day: by the hearth or at the table; children about the room
      if (r.job == Job::Child) {
        if (!standFree(g, a, {}, -1, -1)) return false;
        a.posture = (mixu((uint64_t)a.id, (uint64_t)g.day) % 2u) ? Posture::SitFloor : Posture::None;   // a game on the floor, or about
        return true;
      }
      if ((r.job == Job::Elder || (mixu((uint64_t)a.id, (uint64_t)g.day * 3u) % 2u) == 0) &&
          sitDown(g, a, {RoomKind::Hall, RoomKind::Cottage, RoomKind::Parlour, RoomKind::Common, RoomKind::Feast}, Posture::Sit))
        return true;
      if (!standFree(g, a, {}, -1, -1)) return false;
      if (r.flags & life::RF_GRIEVING) bubble(a, Bubble::Tear, 4.0f);
      a.posture = (mixu((uint64_t)a.id, 11u) % 3u) == 0 ? Posture::Sweep : Posture::None;
      return true;
    }
  }
}

// a building's key person at their post: the smith at the anvil, the priest at the altar, the baker at the oven...
void LifeOps::keyPost(Game& g, Actor& a, life::Census* c, const Bldg& B) {
  using art::Building;
  (void)c;
  switch (B.type) {
    case Building::Smithy: case Building::Smelter: standAt(g, a, Prop::Anvil, Posture::Hammer, {Prop::Forge, Prop::Grindstone}); break;
    case Building::Bakery: standAt(g, a, Prop::Oven, Posture::Stir, {Prop::PrepTable, Prop::Cauldron}); break;
    case Building::Temple: if (standAt(g, a, Prop::Altar, Posture::Pray, {Prop::Lectern})) a.face = 1; break;
    case Building::Tower: break;   // the mage keeps the generator's place (and goes up to bed at night: loadMapActors)
    case Building::Weaver: standAt(g, a, Prop::Loom, Posture::None, {Prop::SpinningWheel}); break;
    default: {
      // a keeper behind their counter where the generator did not already put them there
      const int x = tileX(a.p), y = tileY(a.p);
      bool behind = false;
      for (const RT::Furn& s : rt(g).in.stations) if (counterProp(s.p) && std::abs(s.x - x) + std::abs(s.y - y) <= 1) behind = true;
      if (!behind && (B.type == Building::Inn || B.type == Building::Shop || B.type == Building::Butcher || B.type == Building::Fishmonger ||
                      B.type == Building::MeadHall || B.type == Building::TeaHouse))
        standAt(g, a, Prop::CounterM, Posture::None, {Prop::CounterL, Prop::CounterR, Prop::Counter});
      if (a.useX < 0) reserve(g, a, tileX(a.p), tileY(a.p));
      break;
    }
  }
  if (a.useX < 0) reserve(g, a, tileX(a.p), tileY(a.p));
  a.lifeBits |= LB_KEY;
}

// an inn's traveller: in bed at night (their own guest room), about their room by day
void LifeOps::guestPlace(Game& g, Actor& a) {
  if (!(g.hour >= 21.0f || g.hour < 7.0f)) return;
  RT& R = rt(g);
  const int ri = g.sub.roomIndexAt(tileX(a.p), tileY(a.p));
  for (size_t b = 0; b < R.in.beds.size(); b++)
    if (R.in.beds[b].room == ri && lieDown(g, a, (int)b)) return;
}

// seated neighbours chat in pairs
void LifeOps::pairChats(Game& g) {
  RT& R = rt(g);
  for (size_t i = 1; i < g.actors.size(); i++) {
    Actor& a = g.actors[i];
    if (!(a.lifeBits & LB_LIFE) || (a.lifeBits & LB_SERVER) || a.posture == Posture::Sleep) continue;
    RT::ARt& A = R.rt(a.id);
    if (A.partner >= 0) continue;
    for (size_t j = i + 1; j < g.actors.size(); j++) {
      Actor& o = g.actors[j];
      if (!(o.lifeBits & LB_LIFE) || (o.lifeBits & LB_SERVER) || o.posture == Posture::Sleep) continue;
      RT::ARt& B = R.rt(o.id);
      if (B.partner >= 0 || len2(o.p - a.p) > (2.2f * TILE) * (2.2f * TILE)) continue;
      A.partner = o.id; B.partner = a.id;
      A.t = 0.5f; B.t = 1.7f;
      break;
    }
  }
}

// the interior's life over time: bubbles, service, the bard, the festival, comings and goings
bool LifeOps::interiorFolk(Game& g, Actor& a, float dt) {
  RT& R = rt(g);
  RT::ARt& A = R.rt(a.id);
  a.postureT += dt;
  if (a.bubbleT > 0 && (a.bubbleT -= dt) <= 0) a.bubble = Bubble::None;
  A.t -= dt;
  // leaving: walk to the way out, then gone
  if (A.kind == T_EXIT) {
    a.posture = Posture::None;
    const Vec2 ex = tileCentre(g.sub.exitX, g.sub.exitY - 1);
    if (len2(ex - a.p) < 6.0f * 6.0f) { a.lifeBits |= LB_GONE; return true; }
    if (!g.navStep(a, ex, a.speed * kWalk, dt)) a.lifeBits |= LB_GONE;
    a.st = AState::Walk;
    return true;
  }
  if (A.kind == T_SPOT && !A.arrived) {   // walking to the place decided for them (a seat, a station, the stairs)
    const Vec2 to = tileCentre(A.gx, A.gy);
    a.posture = Posture::None;
    if (len2(to - a.p) < 3.0f * 3.0f || !g.navStep(a, to, a.speed * kWalk, dt)) {
      A.arrived = true;
      if (A.then == 1) { a.lifeBits |= LB_GONE; return true; }
      a.p = A.dest;
      // (M7) a spot taken since it was chosen (a notice board put up, a FOR SALE sign): stand on the nearest clear tile
      if (!g.inside && A.post == Posture::None && !g.bodyFree(a.p, a.radius, false)) a.p = snapWalk(g, a.p);
      a.home = a.p; a.goal = a.p;
      a.posture = A.post;
      a.face = A.destFace;
      a.st = AState::Idle;
      a.postureT = 0;
    } else a.st = AState::Walk;
    return true;
  }
  a.st = AState::Idle;
  switch (a.posture) {
    case Posture::Sleep:
      if (a.bubbleT <= 0 && A.t <= 0) { bubble(a, Bubble::Zzz, 2.0f); A.t = 4.0f + (float)(mixu(a.id, (uint64_t)g.time) % 300u) * 0.01f; }
      return true;
    case Posture::Lute: case Posture::Drum: case Posture::Flute:
      if (A.t <= 0) {
        A.t = 2.0f;
        if ((mixu(a.id, (uint64_t)(g.time * 2)) % 3u) == 0) bubble(a, Bubble::Note, 1.4f);
        // the last chord of a tune: the room cheers
        if ((mixu(a.id, (uint64_t)(g.time / 30.0f)) % 9u) == 0 && fmodf(g.time, 30.0f) < 2.0f) g.sfx((int)Sfx::Cheer, a.p, 1.0f, 0.5f);
      }
      return true;
    default: break;
  }
  // service: the server takes a mug to a seated patron who has none, then back to the bar
  if (a.resident >= 0 && a.site >= 0) {
    life::Census* c = g.life.findMut(g.world.sites[(size_t)a.site].id);
    if (c && a.resident < (int)c->res.size() && c->res[(size_t)a.resident].job == life::Job::Server && serve(g, a, A, dt)) return true;
  }
  // a brawl (an unhappy town's tavern, MF_BRAWLS): two at it with their fists, the keeper shouting, then one storms out
  if (A.act == life::Act::Brawl) {
    if (A.partner < 0) {
      for (size_t k = 1; k < g.actors.size(); k++) {
        Actor& o = g.actors[k];
        if (o.id == a.id || !(o.lifeBits & LB_LIFE)) continue;
        auto it = R.act.find(o.id);
        if (it == R.act.end() || it->second.act != life::Act::Brawl || it->second.partner >= 0 || len2(o.p - a.p) > (7.0f * TILE) * (7.0f * TILE)) continue;
        A.partner = o.id; it->second.partner = a.id;
        A.t2 = 0; it->second.t2 = 0;
        break;
      }
    }
    const int k = A.partner >= 0 ? g.findActor(A.partner) : -1;
    if (k > 0) {
      Actor& o = g.actors[(size_t)k];
      release(g, a);
      a.posture = Posture::None;
      if (len2(o.p - a.p) > 15.0f * 15.0f) { if (!g.navStep(a, o.p, a.speed * 0.8f, dt)) A.partner = -1; a.st = AState::Walk; return true; }
      a.face = faceOf(o.p - a.p);
      A.t2 += dt;
      if (a.st != AState::Windup && a.st != AState::Strike && fmodf(A.t2 + (float)(a.id % 7) * 0.2f, 1.5f) < dt) { a.st = AState::Windup; a.stT = 0; }
      if (a.st == AState::Windup && a.stT > 0.3f) { a.st = AState::Strike; a.stT = 0; g.sfx((int)Sfx::Hit, a.p, 1.3f, 0.25f); }
      if (a.st == AState::Strike && a.stT > 0.25f) a.st = AState::Idle;
      if (a.bubbleT <= 0) bubble(a, Bubble::Anger, 1.0f);
      if (A.t2 > 3.0f && A.t2 - dt <= 3.0f)
        for (size_t j = 1; j < g.actors.size(); j++)
          if ((g.actors[j].lifeBits & LB_KEY) && onScreen(g, g.actors[j].p)) { say(g, g.actors[j], "TAKE IT OUTSIDE, YOU TWO!", rgba(255, 190, 150)); break; }
      if (A.t2 > 12.0f && a.id < o.id) {   // over: the one storms off home, the other back to their drink
        A.kind = T_EXIT; A.partner = -1; a.st = AState::Idle;
        auto it = R.act.find(o.id);
        if (it != R.act.end()) { it->second.partner = -1; it->second.act = life::Act::Tavern; }
        o.st = AState::Idle;
        o.posture = Posture::Drink;
      }
      return true;
    }
  }
  // chat pairs
  if (A.partner >= 0) {
    const int k = g.findActor(A.partner);
    if (k > 0 && A.t <= 0) {
      Actor& o = g.actors[(size_t)k];
      if (a.posture == Posture::None || a.posture == Posture::Drink || a.posture == Posture::Sweep) a.face = faceOf(o.p - a.p);
      A.t = 2.6f + (float)(mixu(a.id, (uint64_t)(g.time * 3)) % 240u) * 0.01f;
      static const Bubble chat[] = {Bubble::Talk, Bubble::Talk, Bubble::Mug, Bubble::Exclaim, Bubble::Talk, Bubble::Question};
      Bubble b = chat[mixu(a.id ^ o.id, (uint64_t)(g.time * 2)) % 6u];
      if (a.posture == Posture::Sit || a.posture == Posture::SitEat || a.posture == Posture::SitFloor || a.posture == Posture::SitFloorEat) b = b == Bubble::Mug ? Bubble::Talk : b;
      bubble(a, b, 1.5f);
    }
  }
  // a festival night: toasts and dancing
  if (a.site >= 0 && g.life.festival(g.world.sites[(size_t)a.site].id, g.day) && g.hour >= 18.0f) {
    if (a.posture == Posture::SitDrink && A.t2 <= 0 && (mixu(a.id, (uint64_t)(g.time / 6.0f)) % 5u) == 0) {
      a.posture = Posture::Cheer; a.postureT = 0; A.t2 = 2.0f;
      if (mixu(a.id, (uint64_t)g.time) % 3u == 0) g.sfx((int)Sfx::Cheer, a.p, 1.0f, 0.4f);
    }
    if (a.posture == Posture::Drink || a.posture == Posture::None) a.posture = Posture::Dance;
  }
  if (A.t2 > 0 && (A.t2 -= dt) <= 0 && a.posture == Posture::Cheer) a.posture = Posture::SitDrink;
  return true;
}

bool LifeOps::serve(Game& g, Actor& a, RT::ARt& A, float dt) {
  // 0 at the bar, 1 taking a mug to a patron, 2 handing it over, 3 back to the bar
  switch (A.servePhase) {
    case 0: {
      if (A.t > 0) return false;
      A.t = 3.0f;
      for (size_t k = 1; k < g.actors.size(); k++) {
        Actor& o = g.actors[k];
        if (o.id == a.id || !(o.lifeBits & LB_LIFE) || (o.lifeBits & LB_SERVED) || !isSeated(o.posture)) continue;
        A.serving = o.id;
        A.barX = tileX(a.p); A.barY = tileY(a.p);   // the way back
        release(g, a);
        A.servePhase = 1;
        return true;
      }
      return false;
    }
    case 1: {
      const int k = g.findActor(A.serving);
      if (k <= 0) { A.servePhase = 3; return true; }
      Actor& o = g.actors[(size_t)k];
      a.posture = Posture::Carry;
      if (len2(o.p - a.p) > 17.0f * 17.0f) {
        if (!g.navStep(a, o.p, a.speed * 0.7f, dt)) { A.servePhase = 3; return true; }
        a.st = AState::Walk;
        return true;
      }
      A.t2 = 1.6f;
      A.servePhase = 2;
      a.face = faceOf(o.p - a.p);
      a.st = AState::Idle;
      a.posture = Posture::None;
      o.lifeBits |= LB_SERVED;
      o.posture = (o.posture == Posture::SitFloor || o.posture == Posture::SitFloorEat || o.posture == Posture::SitFloorDrink) ? Posture::SitFloorDrink
                                                                                                                             : Posture::SitDrink;
      bubble(o, Bubble::Mug, 1.8f);
      g.sfx((int)Sfx::Coin, o.p, 1.4f, 0.25f);
      return true;
    }
    case 2:
      a.st = AState::Idle;
      if ((A.t2 -= dt) <= 0) A.servePhase = 3;
      return true;
    default: {
      if (A.barX < 0) { A.servePhase = 0; return false; }
      const Vec2 bar = tileCentre(A.barX, A.barY);
      if (len2(bar - a.p) > 3.0f * 3.0f) {
        if (!g.navStep(a, bar, a.speed * 0.7f, dt)) a.p = bar;
        a.st = AState::Walk;
        a.posture = Posture::None;
        return true;
      }
      a.st = AState::Idle;
      reserve(g, a, A.barX, A.barY);
      a.face = 0;
      A.servePhase = 0;
      A.t = 6.0f + (float)(mixu(a.id, (uint64_t)g.time) % 400u) * 0.01f;
      return true;
    }
  }
}

// decide where a resident goes in this room (placeIndoors) and walk there from where they stand (false: no place)
bool LifeOps::walkToPlace(Game& g, Actor& a, life::Census& c, const life::Resident& r, const life::Plan& p, bool gatherHere) {
  RT& R = rt(g);
  const Vec2 from = a.p;
  const int face0 = a.face;
  if (!placeIndoors(g, a, c, r, p, gatherHere)) { a.p = from; return false; }
  RT::ARt& A = R.rt(a.id);
  A.dest = a.p; A.post = a.posture; A.destFace = a.face; A.then = 0;
  A.gx = tileX(a.p); A.gy = tileY(a.p);
  A.kind = T_SPOT; A.arrived = false;
  a.p = from; a.home = from; a.goal = from;
  a.face = face0;
  a.posture = Posture::None;
  return true;
}

// to bed: a free bed of the household on this floor, else up the stairs (gone), else out of the door
void LifeOps::goToBed(Game& g, Actor& a, RT::ARt& A) {
  RT& R = rt(g);
  release(g, a);
  a.posture = Posture::None;
  for (size_t b = 0; b < R.in.beds.size(); b++) {
    const RT::Furn& f = R.in.beds[b];
    if (!roomHomeBed(roomKind(g.sub, f.room)) || isUsed(g, f.x, f.y) || inRentedRoom(g, f.x, f.y)) continue;
    // the tile beside the bed to walk to, then lie down
    const Vec2 from = a.p;
    if (!lieDown(g, a, (int)b)) continue;
    A.dest = a.p; A.post = a.posture; A.destFace = a.face; A.then = 0;
    a.p = from; a.posture = Posture::None;
    // a free tile next to the bed (the bed itself is solid)
    static const int dx[4] = {0, 1, -1, 0}, dy[4] = {1, 0, 0, -1};
    A.gx = f.x; A.gy = f.y + 1;
    for (int k = 0; k < 4; k++)
      if (g.sub.in(f.x + dx[k], f.y + dy[k]) && !g.sub.blocked(f.x + dx[k], f.y + dy[k])) { A.gx = f.x + dx[k]; A.gy = f.y + dy[k]; break; }
    A.kind = T_SPOT; A.arrived = false;
    return;
  }
  if (g.sub.up.valid()) {
    A.gx = g.sub.up.x; A.gy = g.sub.up.y + 0;
    A.dest = tileCentre(A.gx, A.gy); A.then = 1;
    A.kind = T_SPOT; A.arrived = false;
    return;
  }
  A.kind = T_EXIT;
}

// the interior over time: residents whose plan takes them elsewhere walk out; arrivals walk in (ground floor)
void LifeOps::interiorTick(Game& g, float dt) {
  RT& R = rt(g);
  if (g.subBldg < 0 || R.in.key != g.mapKey()) return;
  R.in.tickT -= dt;
  if (R.in.tickT > 0) return;
  R.in.tickT = 1.5f;
  const Bldg& B = g.world.over.bldgs[(size_t)g.subBldg];
  const int si = B.site;
  life::Census* c = censusOfSite(g, si);
  if (!c) return;
  RT::SiteRt& S = siteRt(g, si);
  refreshPlans(g, S, *c);
  if (S.planStamp == R.in.stamp) return;
  R.in.stamp = S.planStamp;
  const Site& st = g.world.sites[(size_t)si];
  const int off = g.subBldg - st.bldgFirst;
  std::vector<uint8_t> here(c->res.size(), 0);
  int count = 0;
  for (size_t k = 1; k < g.actors.size(); k++) {
    Actor& a = g.actors[k];
    if (!(a.lifeBits & LB_LIFE) || a.resident < 0 || a.resident >= (int)c->res.size()) continue;
    here[(size_t)a.resident] = 1;
    count++;
    const life::Plan& p = S.plans[(size_t)a.resident];
    RT::ARt& A = R.rt(a.id);
    if (A.kind == T_EXIT) continue;
    // an evening's drop-in keeps their seat until late (one leaves a tick after 23:00, the rest by midnight)
    if (A.dropIn && p.bldg != off && (g.hour >= 18.0f && g.hour < 23.0f + (float)(mixu((uint64_t)a.id, 5u) % 10u) * 0.1f)) continue;
    if (p.bldg != off) {
      if (a.posture == Posture::Sleep) {   // the sleeper gets up and goes (or, upstairs, is simply gone)
        if (g.subFloor != 0) { a.lifeBits |= LB_GONE; continue; }
        release(g, a);
        a.posture = Posture::None;
        if (!g.bodyFree(a.p, a.radius, false)) { const Vec2 f = g.freeSpot(tileX(a.p), tileY(a.p) + 1); a.p = f; }
      }
      release(g, a);
      A.kind = T_EXIT;
      A.partner = -1;
      a.posture = Posture::None;
    } else if (p.act != A.act) {
      // a change of activity in the same building (a meal after work, bed after the evening)
      using life::Act;
      const bool sitAct = p.act == Act::Tavern || p.act == Act::Socialise || p.act == Act::Eat || p.act == Act::Bathe;
      if (p.act == Act::Sleep) { A.act = p.act; goToBed(g, a, A); continue; }
      if (sitAct && isSeated(a.posture)) {   // a meal ordered, a drink after supper: they stay in their seat
        const bool fl = a.posture == Posture::SitFloor || a.posture == Posture::SitFloorEat || a.posture == Posture::SitFloorDrink;
        a.posture = p.act == Act::Eat ? (fl ? Posture::SitFloorEat : Posture::SitEat) : (fl ? Posture::SitFloorDrink : Posture::SitDrink);
        A.act = p.act;
        continue;
      }
      release(g, a);
      A.act = p.act; A.place = p.place; A.pbldg = p.bldg;
      if (!walkToPlace(g, a, *c, c->res[(size_t)a.resident], p, off == c->gathering || off == c->gathering2)) A.kind = T_EXIT;
    }
  }
  if (g.subFloor != 0 || count >= 18) return;
  // arrivals by the door
  int arrivals = 0;
  for (size_t i = 0; i < c->res.size() && i < (size_t)LIFE_SLOTN && arrivals < 3; i++) {
    if (here[i]) continue;
    const life::Resident& r = c->res[i];
    if (r.flags & (life::RF_DEAD | life::RF_AWAY) || r.job == life::Job::Guard || (workKey(r) && r.keyBldg == off && r.actor >= 0 && g.findActor(r.actor) > 0)) continue;
    const life::Plan& p = S.plans[i];
    if (p.bldg != off || p.act == life::Act::Sleep) continue;
    const int id = spawnResident(g, si, *c, (int)i, tileCentre(g.sub.exitX, g.sub.exitY - 1), true);
    Actor& a = g.actors[(size_t)g.findActor(id)];
    // where they are going: decided now (a seat, a station), walked to from the door
    if (!walkToPlace(g, a, *c, r, p, off == c->gathering || off == c->gathering2)) { a.lifeBits |= LB_GONE; continue; }
    a.st = AState::Walk;
    g.sfx((int)Sfx::Door, a.p, 1.1f, 0.3f);
    arrivals++;
  }
}
