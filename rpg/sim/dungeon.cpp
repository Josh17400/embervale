// Sub-level generation: natural caves (cellular automata), ancient ruins (rooms + corridors), building interiors.
#include <algorithm>
#include <queue>
#include <utility>
#include <vector>
#include "rpg/sim/deco.h"
#include "rpg/sim/interior_v4.h"
#include "rpg/sim/world.h"

using art::Prop;

namespace {
void bfs(const Map& m, int sx, int sy, std::vector<int>& dist) {
  dist.assign((size_t)m.w * m.h, -1);
  std::queue<int> q;
  dist[(size_t)sy * m.w + sx] = 0;
  q.push(sy * m.w + sx);
  static const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
  while (!q.empty()) {
    int c = q.front(); q.pop();
    int x = c % m.w, y = c / m.w;
    for (int k = 0; k < 4; k++) {
      int nx = x + dx[k], ny = y + dy[k];
      if (!m.in(nx, ny) || groundSolid(m.at(nx, ny))) continue;
      size_t ni = (size_t)ny * m.w + nx;
      if (dist[ni] >= 0) continue;
      dist[ni] = dist[(size_t)c] + 1;
      q.push((int)ni);
    }
  }
}
int wallsAround(const Map& m, int x, int y, Ground wall) {
  int n = 0;
  for (int oy = -1; oy <= 1; oy++)
    for (int ox = -1; ox <= 1; ox++)
      if ((ox || oy) && (!m.in(x + ox, y + oy) || m.at(x + ox, y + oy) == wall)) n++;
  return n;
}
void spawnMonsters(Map& m, const std::vector<int>& dist, int exitX, int exitY, Rng& r, art::Monster theme, int count, int site) {
  std::vector<int> cells;
  for (int i = 0; i < m.w * m.h; i++)
    if (dist[(size_t)i] > 14) cells.push_back(i);
  int slot = 0;
  for (int k = 0; k < count && !cells.empty(); k++) {
    int c = cells[r.irange((int)cells.size())];
    int x = c % m.w, y = c / m.w;
    if (std::abs(x - exitX) + std::abs(y - exitY) < 10) continue;
    Spawn sp; sp.x = x; sp.y = y; sp.site = site; sp.slot = slot++;
    // mixed inhabitants: mostly theme, some extras
    static const art::Monster extra[] = {art::Monster::Bat, art::Monster::Spider, art::Monster::Skeleton, art::Monster::Slime};
    sp.mon = r.f() < 0.7f ? theme : extra[r.irange(4)];
    if (theme == art::Monster::Draugr || theme == art::Monster::Wraith) sp.mon = r.f() < 0.6f ? art::Monster::Draugr : (r.f() < 0.5f ? art::Monster::Skeleton : theme);
    if (theme == art::Monster::Goblin && r.f() < 0.5f) sp.mon = art::Monster::Goblin;
    m.spawns.push_back(sp);
  }
}
}  // namespace

void genCave(Map& m, const Site& s, uint32_t seed) {
  Rng r(seed);
  m.kind = MapKind::Cave;
  m.seed = seed;
  int W = 72 + r.irange(16), H = 56 + r.irange(12);
  m.alloc(W, H, Ground::CaveWall);
  std::vector<uint8_t> a((size_t)W * H), b;
  for (int y = 0; y < H; y++)
    for (int x = 0; x < W; x++)
      a[(size_t)y * W + x] = (x < 2 || y < 2 || x >= W - 2 || y >= H - 2) ? 1 : (r.f() < 0.46f ? 1 : 0);
  for (int it = 0; it < 5; it++) {
    b = a;
    for (int y = 1; y < H - 1; y++)
      for (int x = 1; x < W - 1; x++) {
        int n = 0;
        for (int oy = -1; oy <= 1; oy++) for (int ox = -1; ox <= 1; ox++) if (ox || oy) n += a[(size_t)(y + oy) * W + x + ox];
        b[(size_t)y * W + x] = n >= 5 || (it < 2 && n <= 1) ? 1 : 0;
      }
    a.swap(b);
  }
  for (int i = 0; i < W * H; i++) m.ground[(size_t)i] = (uint8_t)(a[(size_t)i] ? Ground::CaveWall : Ground::CaveFloor);
  // exit near the bottom middle: find a floor tile closest to (W/2, H-4)
  int ex = W / 2, ey = H - 4, bd = 1 << 30;
  for (int y = 2; y < H - 2; y++)
    for (int x = 2; x < W - 2; x++)
      if (m.at(x, y) == Ground::CaveFloor && m.at(x, y - 1) == Ground::CaveFloor) {
        int d = (x - W / 2) * (x - W / 2) + (y - (H - 4)) * (y - (H - 4)) * 3;
        if (d < bd) { bd = d; ex = x; ey = y; }
      }
  std::vector<int> dist;
  bfs(m, ex, ey, dist);
  // seal everything unreachable
  int far = 0, fx = ex, fy = ey;
  for (int i = 0; i < W * H; i++) {
    if (m.ground[(size_t)i] == (uint8_t)Ground::CaveFloor && dist[(size_t)i] < 0) m.ground[(size_t)i] = (uint8_t)Ground::CaveWall;
    if (dist[(size_t)i] > far) { far = dist[(size_t)i]; fx = i % W; fy = i / W; }
  }
  m.exitX = ex; m.exitY = ey;
  m.setProp(ex, ey, Prop::Ladder);
  bool icy = s.theme == art::Monster::FrostSpider || s.theme == art::Monster::IceWolf;
  // underground pools
  for (int k = 0; k < 3; k++) {
    int px = 4 + r.irange(W - 8), py = 4 + r.irange(H - 8);
    if (dist[(size_t)py * W + px] < 8) continue;
    for (int oy = -2; oy <= 2; oy++)
      for (int ox = -3; ox <= 3; ox++) {
        int x = px + ox, y = py + oy;
        if (m.at(x, y) == Ground::CaveFloor && ox * ox * 0.5f + oy * oy < 4.5f && dist[(size_t)y * W + x] > 6 && wallsAround(m, x, y, Ground::CaveWall) < 3)
          m.setG(x, y, icy ? Ground::Ice : Ground::Water);
      }
  }
  bfs(m, ex, ey, dist);
  // decor
  for (int y = 1; y < H - 1; y++)
    for (int x = 1; x < W - 1; x++) {
      if (m.at(x, y) != Ground::CaveFloor || m.propAt(x, y) || dist[(size_t)y * W + x] < 3) continue;
      int wn = wallsAround(m, x, y, Ground::CaveWall);
      float q = r.f();
      if (wn >= 3 && q < 0.10f) m.setProp(x, y, icy ? Prop::Crystal : (r.f() < 0.5f ? Prop::Stalagmite : Prop::Mushrooms));
      else if (wn >= 2 && q < 0.13f) m.setProp(x, y, s.theme == art::Monster::Spider || s.theme == art::Monster::FrostSpider ? Prop::Cobweb : Prop::Bones);
      else if (q < 0.008f) m.setProp(x, y, Prop::Crystal);
      else if (q < 0.016f) m.setProp(x, y, Prop::Torch);
    }
  // chests in dead ends far from the exit
  int chests = 0;
  for (int tries = 0; tries < 800 && chests < 4; tries++) {
    int x = 2 + r.irange(W - 4), y = 2 + r.irange(H - 4);
    if (m.at(x, y) != Ground::CaveFloor || m.propAt(x, y) || dist[(size_t)y * W + x] < 15) continue;
    if (wallsAround(m, x, y, Ground::CaveWall) < 5) continue;
    m.setProp(x, y, Prop::Chest);
    chests++;
  }
  // boss lair at the far end
  m.setProp(fx, fy, Prop::Chest);
  for (int oy = -2; oy <= 2; oy++)
    for (int ox = -2; ox <= 2; ox++)
      if ((ox || oy) && m.at(fx + ox, fy + oy) == Ground::CaveFloor && m.propAt(fx + ox, fy + oy) == (int)Prop::Torch + 1) m.setP(fx + ox, fy + oy, 0);
  spawnMonsters(m, dist, ex, ey, r, s.theme, 10 + r.irange(6), -1);
  // boss: nearest floor next to the far chest
  Spawn boss; boss.boss = true; boss.slot = 99; boss.x = fx; boss.y = fy;
  static const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
  for (int k = 0; k < 4; k++) if (m.at(fx + dx[k], fy + dy[k]) == Ground::CaveFloor && !m.propAt(fx + dx[k], fy + dy[k])) { boss.x = fx + dx[k]; boss.y = fy + dy[k]; break; }
  boss.mon = s.theme == art::Monster::Spider ? art::Monster::Spider : s.theme == art::Monster::Bat ? art::Monster::Troll
           : s.theme == art::Monster::Skeleton ? art::Monster::Draugr : s.theme == art::Monster::Goblin ? art::Monster::Troll : s.theme;
  m.spawns.push_back(boss);
  // the arrival tile above the exit must always be free
  if (m.propAt(m.exitX, m.exitY - 1) == (int)Prop::Chest + 1) m.setProp(m.exitX - 1 >= 0 && !groundSolid(m.at(m.exitX - 1, m.exitY - 1)) ? m.exitX - 1 : m.exitX, m.exitY - 2, Prop::Chest);
  m.setP(m.exitX, m.exitY - 1, 0);
  m.rebuildSolid();
}

void genRuin(Map& m, const Site& s, uint32_t seed) {
  Rng r(seed);
  m.kind = MapKind::Ruin;
  m.seed = seed;
  int W = 70, H = 58;
  m.alloc(W, H, Ground::CaveWall);
  struct Room { IRect rc; };
  std::vector<Room> rooms;
  // entrance hall at the bottom middle
  rooms.push_back({IRect{W / 2 - 4, H - 10, 9, 7}});
  for (int tries = 0; tries < 400 && rooms.size() < 13; tries++) {
    int rw = 6 + r.irange(8), rh = 5 + r.irange(6);
    IRect rc{2 + r.irange(W - rw - 4), 2 + r.irange(H - rh - 14), rw, rh};
    bool ok = true;
    for (auto& o : rooms) if (rc.overlaps(o.rc, 2)) ok = false;
    if (ok) rooms.push_back({rc});
  }
  auto carve = [&](IRect rc) {
    for (int y = rc.y; y < rc.y + rc.h; y++) for (int x = rc.x; x < rc.x + rc.w; x++) m.setG(x, y, Ground::StoneFloor);
  };
  for (auto& rm : rooms) carve(rm.rc);
  // connect each room to its nearest already-connected room with an L corridor (2 wide)
  std::vector<int> conn{0};
  std::vector<uint8_t> isConn(rooms.size(), 0);
  isConn[0] = 1;
  while (conn.size() < rooms.size()) {
    int ba = -1, bb = -1, bd = 1 << 30;
    for (int a : conn)
      for (int b = 0; b < (int)rooms.size(); b++) {
        if (isConn[b]) continue;
        int dx = rooms[a].rc.cx() - rooms[b].rc.cx(), dy = rooms[a].rc.cy() - rooms[b].rc.cy();
        int d = dx * dx + dy * dy;
        if (d < bd) { bd = d; ba = a; bb = b; }
      }
    isConn[bb] = 1; conn.push_back(bb);
    int x0 = rooms[ba].rc.cx(), y0 = rooms[ba].rc.cy(), x1 = rooms[bb].rc.cx(), y1 = rooms[bb].rc.cy();
    bool hFirst = r.f() < 0.5f;
    int cx = hFirst ? x1 : x0, cy = hFirst ? y0 : y1;
    for (int x = std::min(x0, x1); x <= std::max(x0, x1); x++) { m.setG(x, cy, Ground::StoneFloor); m.setG(x, cy + 1, Ground::StoneFloor); }
    for (int y = std::min(y0, y1); y <= std::max(y0, y1); y++) { m.setG(cx, y, Ground::StoneFloor); m.setG(cx + 1, y, Ground::StoneFloor); }
  }
  m.exitX = rooms[0].rc.cx(); m.exitY = rooms[0].rc.y + rooms[0].rc.h - 1;
  m.setProp(m.exitX, m.exitY, Prop::Ladder);
  std::vector<int> dist;
  bfs(m, m.exitX, m.exitY, dist);
  // boss room = furthest room
  int bossRoom = 0, far = -1;
  for (int i = 1; i < (int)rooms.size(); i++) {
    int d = dist[(size_t)rooms[i].rc.cy() * W + rooms[i].rc.cx()];
    if (d > far) { far = d; bossRoom = i; }
  }
  for (int i = 0; i < (int)rooms.size(); i++) {
    IRect rc = rooms[i].rc;
    // braziers in corners, urns and coffins along walls
    m.setProp(rc.x, rc.y, Prop::Brazier);
    m.setProp(rc.x + rc.w - 1, rc.y, Prop::Brazier);
    for (int k = 0; k < rc.w * rc.h / 14; k++) {
      int x = rc.x + r.irange(rc.w), y = rc.y + r.irange(rc.h);
      if (m.propAt(x, y) || (x == m.exitX && std::abs(y - m.exitY) < 3)) continue;
      bool edge = x == rc.x || y == rc.y || x == rc.x + rc.w - 1 || y == rc.y + rc.h - 1;
      if (!edge) { if (r.f() < 0.3f) m.setProp(x, y, r.f() < 0.5f ? Prop::Bones : Prop::Cobweb); continue; }
      static const Prop deco[] = {Prop::Urn, Prop::Coffin, Prop::Urn, Prop::SkullPile, Prop::Cobweb, Prop::Statue};
      m.setProp(x, y, deco[r.irange(6)]);
    }
    if (i != 0 && i != bossRoom && r.f() < 0.4f) {
      int x = rc.x + 1 + r.irange(std::max(1, rc.w - 2)), y = rc.y;
      if (!m.propAt(x, y)) m.setProp(x, y, Prop::Chest);
    }
  }
  IRect br = rooms[bossRoom].rc;
  m.setProp(br.cx(), br.y, Prop::Altar);
  m.setProp(br.cx() + 1, br.y, Prop::Chest);
  spawnMonsters(m, dist, m.exitX, m.exitY, r, s.theme, 12 + r.irange(6), -1);
  Spawn boss; boss.boss = true; boss.slot = 99; boss.x = br.cx(); boss.y = br.cy() + 1;
  boss.mon = s.theme == art::Monster::Wraith ? art::Monster::Wraith : art::Monster::Draugr;
  m.spawns.push_back(boss);
  // the arrival tile above the exit must always be free
  if (m.propAt(m.exitX, m.exitY - 1) == (int)Prop::Chest + 1) m.setProp(m.exitX - 1 >= 0 && !groundSolid(m.at(m.exitX - 1, m.exitY - 1)) ? m.exitX - 1 : m.exitX, m.exitY - 2, Prop::Chest);
  m.setP(m.exitX, m.exitY - 1, 0);
  m.rebuildSolid();
}

// The pre-M0 interior (Bldg::genVer < WORLDGEN_V3). Frozen: old saves rebuild exactly these rooms.
static void genInteriorV2(Map& m, const Bldg& b, uint32_t seed) {
  Rng r(seed);
  m.kind = MapKind::Interior;
  m.seed = seed;
  int W = std::max(11, b.r.w * 2 + 3), H = std::max(9, b.r.h * 2 + 5);
  if (b.type == art::Building::Keep) { W = 23; H = 15; }
  if (b.type == art::Building::Temple) { W = 15; H = 13; }
  if (b.type == art::Building::Inn) { W = 17; H = 12; }
  if (W % 2 == 0) W++;
  bool stone = b.type == art::Building::Keep || b.type == art::Building::Temple || b.type == art::Building::StoneHouse || b.type == art::Building::Tower;
  m.alloc(W, H, Ground::InteriorWall);
  for (int y = 2; y < H - 1; y++)
    for (int x = 1; x < W - 1; x++) m.setG(x, y, stone ? Ground::StoneFloor : Ground::WoodFloor);
  int ex = W / 2, ey = H - 1;
  m.setG(ex, ey, stone ? Ground::StoneFloor : Ground::WoodFloor);
  m.exitX = ex; m.exitY = ey;
  auto put = [&](int x, int y, Prop p) { if (m.at(x, y) != Ground::InteriorWall && !m.propAt(x, y) && !(x == ex && y >= H - 3)) m.setProp(x, y, p); };
  Spawn owner; owner.npc = true; owner.role = b.owner; owner.slot = 0; owner.site = b.site;
  switch (b.type) {
    case art::Building::Inn:
      for (int x = 2; x < 7; x++) put(x, 4, Prop::Counter);
      owner.x = 4; owner.y = 3;
      put(2, 2, Prop::Barrel); put(3, 2, Prop::Barrel2); put(6, 2, Prop::Shelf);
      put(W / 2, 2, Prop::Fireplace);
      for (int k = 0; k < 3; k++) { put(9 + (k % 2) * 4, 5 + (k / 2) * 3, Prop::Table); put(10 + (k % 2) * 4, 5 + (k / 2) * 3, Prop::Chair); put(8 + (k % 2) * 4, 5 + (k / 2) * 3, Prop::Chair); }
      put(W - 2, 2, Prop::Bed); put(W - 4, 2, Prop::Bed);
      put(W / 2, H - 4, Prop::Rug);
      for (int k = 1; k <= 2; k++) { Spawn p; p.npc = true; p.role = Role::Villager; p.slot = k; p.site = b.site; p.x = 9 + k * 2; p.y = 7; m.spawns.push_back(p); }
      break;
    case art::Building::Shop:
      for (int x = 2; x < W - 2; x++) if (x != ex) put(x, 4, Prop::Counter);
      owner.x = W / 2 - 2; owner.y = 3;
      for (int x = 1; x < W - 1; x += 2) put(x, 2, Prop::Shelf);
      put(1, H - 3, Prop::Barrel); put(W - 2, H - 3, Prop::Crate); put(W - 2, H - 4, Prop::Barrel2);
      put(2, H - 3, Prop::PlantPot);
      break;
    case art::Building::Smithy:
      put(2, 2, Prop::Fireplace); put(4, 3, Prop::Anvil); put(W - 2, 2, Prop::Shelf); put(W - 3, 2, Prop::Shelf);
      put(1, H - 3, Prop::Woodpile); put(W - 2, H - 3, Prop::Barrel); put(W - 2, H - 4, Prop::Crate);
      owner.x = 5; owner.y = 4;
      break;
    case art::Building::Temple:
      put(ex, 2, Prop::Altar); put(ex - 2, 2, Prop::Brazier); put(ex + 2, 2, Prop::Brazier);
      put(2, 2, Prop::Statue); put(W - 3, 2, Prop::Statue);
      for (int y = 5; y < H - 3; y += 2) { for (int x = 2; x < ex - 1; x++) put(x, y, Prop::Chair); for (int x = ex + 2; x < W - 2; x++) put(x, y, Prop::Chair); }
      for (int y = 3; y < H - 1; y++) if (!m.propAt(ex, y)) m.setProp(ex, y, Prop::Rug);
      owner.x = ex; owner.y = 3;
      break;
    case art::Building::Keep:
      put(ex, 2, Prop::Throne); put(ex - 3, 2, Prop::Banner); put(ex + 3, 2, Prop::Banner);
      put(2, 2, Prop::Bookshelf); put(3, 2, Prop::Bookshelf); put(W - 3, 2, Prop::Bookshelf); put(W - 4, 2, Prop::Shelf);
      for (int y = 3; y < H - 1; y++) if (!m.propAt(ex, y)) m.setProp(ex, y, Prop::Rug);
      for (int x = 3; x < 8; x++) put(x, 8, Prop::Table);
      for (int x = 3; x < 8; x++) { put(x, 7, Prop::Chair); put(x, 9, Prop::Chair); }
      put(W - 3, 6, Prop::Brazier); put(2, 6, Prop::Brazier); put(W - 3, 10, Prop::Brazier); put(2, 10, Prop::Brazier);
      owner.x = ex; owner.y = 3;
      for (int k = 1; k <= 2; k++) { Spawn g; g.npc = true; g.role = Role::Guard; g.slot = k; g.site = b.site; g.x = ex + (k == 1 ? -2 : 2); g.y = 4; m.spawns.push_back(g); }
      break;
    case art::Building::Tower:
      put(2, 2, Prop::Bookshelf); put(3, 2, Prop::Bookshelf); put(W - 3, 2, Prop::Bookshelf);
      put(ex, 4, Prop::Cauldron); put(W - 2, H - 3, Prop::PlantPot); put(2, 5, Prop::Table);
      owner.x = ex + 1; owner.y = 5;
      break;
    default: {   // homes and farms
      put(1 + r.irange(2), 2, Prop::Bed);
      put(W - 3, 2, Prop::Fireplace);
      int tx = 3 + r.irange(std::max(1, W - 7)), ty = 4 + r.irange(std::max(1, H - 8));
      put(tx, ty, Prop::Table); put(tx - 1, ty, Prop::Chair); put(tx + 1, ty, Prop::Chair);
      put(W - 2, H - 3, Prop::Barrel); put(1, H - 3, r.f() < 0.5f ? Prop::Shelf : Prop::Crate);
      if (r.f() < 0.5f) put(ex, H - 4, Prop::Rug);
      if (r.f() < 0.4f) put(W - 2, 3, Prop::Chest);
      if (b.type == art::Building::Farmhouse) { put(2, H - 3, Prop::Haystack); put(3, H - 3, Prop::Woodpile); }
      owner.x = tx; owner.y = ty + 1;
      break;
    }
  }
  if (b.owner != Role::Villager || r.f() < 0.75f) m.spawns.push_back(owner);
  // wall torches along the top wall
  for (int x = 3; x < W - 3; x += 5) if (!m.propAt(x, 1)) m.setProp(x, 1, Prop::Torch);
  m.rebuildSolid();
}

// ===================================================================================== interiors, WORLDGEN_V3 (M0)
// A lived-in room in five passes (VISION_PLAN 10.5): anchors against the walls by template, function groups with a
// 1-tile gap rule, wall decor on the back wall, small clutter on Map::deco, and validation after every group (a BFS from
// the door must reach every usable object and spawn, and at least 60 % of the floor must stay walkable; a group that
// breaks this is removed again). Density follows the job and the household's wealth.
namespace {
using art::Building;

struct Room {
  Map& m;
  Rng& r;
  int W, H, ex, ey;
  std::vector<int16_t> grp;      // furniture group per tile (-1 none)
  std::vector<uint8_t> lane;     // tiles that must stay free of furniture (the way in from the door)
  std::vector<uint8_t> wallUsed; // per column: the back wall there is hidden by furniture or already decorated
  struct Rec { int i; uint8_t prop; int16_t grp; };
  std::vector<Rec> log;
  int groups = 0;
  int floorTiles = 0;
  std::vector<int> dist;

  Room(Map& m_, Rng& r_) : m(m_), r(r_) {}
  int I(int x, int y) const { return y * W + x; }
  bool floorT(int x, int y) const { return m.in(x, y) && !groundSolid(m.at(x, y)); }
  bool freeT(int x, int y) const { return floorT(x, y) && !m.propAt(x, y) && !lane[(size_t)I(x, y)]; }
  // the 1-tile gap rule: nothing of another group in the rectangle grown by one tile
  bool gapOk(int x0, int y0, int x1, int y1, int g) const {
    for (int y = y0 - 1; y <= y1 + 1; y++)
      for (int x = x0 - 1; x <= x1 + 1; x++) {
        if (!floorT(x, y)) continue;
        int o = grp[(size_t)I(x, y)];
        if (o >= 0 && o != g) return false;
      }
    return true;
  }
  bool rectFree(int x0, int y0, int x1, int y1) const {
    for (int y = y0; y <= y1; y++)
      for (int x = x0; x <= x1; x++) if (!freeT(x, y)) return false;
    return true;
  }
  void put(int x, int y, Prop p, int g) {
    size_t i = (size_t)I(x, y);
    log.push_back({(int)i, m.prop[i], grp[i]});
    m.setProp(x, y, p);
    grp[i] = (int16_t)g;
  }
  size_t mark() const { return log.size(); }
  void rollback(size_t mk) {
    while (log.size() > mk) {
      Rec rc = log.back();
      log.pop_back();
      m.prop[(size_t)rc.i] = rc.prop;
      grp[(size_t)rc.i] = rc.grp;
    }
  }
  bool solidT(int x, int y) const {
    if (!floorT(x, y)) return true;
    int p = m.propAt(x, y);
    return p && propSolid((Prop)(p - 1));
  }
  void bfs() {
    dist.assign((size_t)W * H, -1);
    std::queue<int> q;
    dist[(size_t)I(ex, ey)] = 0;
    q.push(I(ex, ey));
    static const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
    while (!q.empty()) {
      int c = q.front(); q.pop();
      int x = c % W, y = c / W;
      for (int k = 0; k < 4; k++) {
        int nx = x + dx[k], ny = y + dy[k];
        if (!m.in(nx, ny) || solidT(nx, ny) || dist[(size_t)I(nx, ny)] >= 0) continue;
        dist[(size_t)I(nx, ny)] = dist[(size_t)c] + 1;
        q.push(I(nx, ny));
      }
    }
  }
  bool reached(int x, int y) const { return m.in(x, y) && dist[(size_t)I(x, y)] >= 0; }
  bool besideReached(int x, int y) const { return reached(x + 1, y) || reached(x - 1, y) || reached(x, y + 1) || reached(x, y - 1); }
  // the room is valid when the door lane is open, at least 60 % of the floor is reachable and every usable object
  // (bed, chest, altar) can be stepped up to
  bool valid() {
    bfs();
    if (!reached(ex, ey - 1)) return false;
    int free = 0;
    for (int y = 0; y < H; y++)
      for (int x = 0; x < W; x++) {
        if (!floorT(x, y)) continue;
        if (reached(x, y)) free++;
        int p = m.propAt(x, y);
        if (!p) continue;
        Prop pp = (Prop)(p - 1);
        if ((pp == Prop::Bed || pp == Prop::Chest || pp == Prop::Altar) && !besideReached(x, y)) return false;
      }
    return free * 10 >= floorTiles * 6;
  }
  // places a group with the gap rule and validation; on failure everything it placed is removed again
  template <class F>
  bool group(F&& place) {
    size_t mk = mark();
    int g = groups++;
    if (!place(g) || !valid()) { rollback(mk); return false; }
    return true;
  }
  void hideWall(int x0, int x1) { for (int x = std::max(0, x0); x <= std::min(W - 1, x1); x++) wallUsed[(size_t)x] = 1; }
  void deco(int x, int y, Deco d) { if (m.in(x, y)) m.deco[(size_t)I(x, y)] = (uint8_t)d; }
};

bool tallProp(Prop p) {
  return p == Prop::Bed || p == Prop::Cupboard || p == Prop::Wardrobe || p == Prop::Bookshelf || p == Prop::Shelf || p == Prop::Loom ||
         p == Prop::Hearth || p == Prop::Forge || p == Prop::Filler || p == Prop::Throne || p == Prop::Altar || p == Prop::Statue ||
         p == Prop::Banner || p == Prop::Barrel2 || p == Prop::Fireplace;
}

// ---- building blocks (each returns false when it does not fit; the caller's group() then rolls back) ----------------
bool hearthAt(Room& R, int x, int g, Prop p = Prop::Hearth) {
  if (x < 2 || x > R.W - 3 || !R.rectFree(x - 1, 2, x + 1, 2) || !R.gapOk(x - 1, 2, x + 1, 2, g)) return false;
  R.put(x - 1, 2, Prop::Filler, g); R.put(x, 2, p, g); R.put(x + 1, 2, Prop::Filler, g);
  R.hideWall(x - 1, x + 1);
  return true;
}
bool backPiece(Room& R, int x, Prop p, int g) {   // furniture standing against the back wall
  if (!R.freeT(x, 2) || !R.gapOk(x, 2, x, 2, g)) return false;
  R.put(x, 2, p, g);
  if (tallProp(p)) R.hideWall(x, x);
  return true;
}
// a table (1 or 2 tiles) with seats around it; seats are walk-through, so they only need free tiles
bool tableSet(Room& R, int x, int y, int len, Prop single, int seats, bool benches, int g) {
  int x1 = x + len - 1;
  if (!R.rectFree(x - 1, y - 1, x1 + 1, y + 1) || !R.gapOk(x - 1, y - 1, x1 + 1, y + 1, g)) return false;
  if (len == 1) R.put(x, y, single, g);
  else {
    R.put(x, y, Prop::TableL, g);
    for (int k = x + 1; k < x1; k++) R.put(k, y, Prop::TableM, g);
    R.put(x1, y, Prop::TableR, g);
  }
  int placed = 0;
  // north side faces the viewer (chairs with their backs to the wall), south side shows stools or a bench
  std::vector<std::pair<int, int>> spots;
  for (int k = x; k <= x1; k++) { spots.push_back({k, y - 1}); spots.push_back({k, y + 1}); }
  if (len == 1 || !benches) { spots.push_back({x - 1, y}); spots.push_back({x1 + 1, y}); }
  for (size_t i = spots.size(); i > 1; i--) std::swap(spots[i - 1], spots[(size_t)R.r.irange((int)i)]);
  if (benches) {
    for (int k = x; k <= x1; k++) { R.put(k, y - 1, Prop::Bench, g); R.put(k, y + 1, Prop::Bench, g); }
    return true;
  }
  for (auto& s : spots) {
    if (placed >= seats) break;
    Prop seat = s.second == y - 1 ? Prop::Chair : Prop::Stool;
    R.put(s.first, s.second, seat, g);
    placed++;
  }
  return placed >= std::min(2, seats);
}
// a rug under a rectangle (only on floor tiles without clutter)
void rug(Room& R, int x0, int y0, int x1, int y1, Deco d) {
  for (int y = y0; y <= y1; y++)
    for (int x = x0; x <= x1; x++)
      if (R.floorT(x, y) && !(x == R.ex && y == R.ey)) R.deco(x, y, d);
}
// tries a placement at random spots until one works
template <class F>
bool tryAt(Room& R, int tries, int x0, int x1, int y0, int y1, F&& f) {
  if (x1 < x0 || y1 < y0) return false;
  for (int t = 0; t < tries; t++) {
    int x = x0 + R.r.irange(x1 - x0 + 1), y = y0 + R.r.irange(y1 - y0 + 1);
    if (R.group([&](int g) { return f(x, y, g); })) return true;
  }
  return false;
}
// a free reachable floor tile near (x, y) for a spawn; falls back to any reachable tile
void spawnNear(Room& R, Spawn& s, int x, int y, std::vector<int>& taken) {
  R.bfs();
  int best = -1, bd = 1 << 30;
  for (int ty = 2; ty < R.H - 1; ty++)
    for (int tx = 1; tx < R.W - 1; tx++) {
      if (!R.reached(tx, ty) || R.solidT(tx, ty) || (tx == R.ex && ty >= R.ey - 1)) continue;
      if (std::find(taken.begin(), taken.end(), R.I(tx, ty)) != taken.end()) continue;
      int pr = R.m.propAt(tx, ty);
      int d = (tx - x) * (tx - x) + (ty - y) * (ty - y) + (pr ? 3 : 0);
      if (d < bd) { bd = d; best = R.I(tx, ty); }
    }
  if (best < 0) best = R.I(R.ex, R.ey - 1);
  s.x = best % R.W; s.y = best / R.W;
  taken.push_back(best);
}

struct Weighted { int what; int w; };
int pick(Rng& r, const std::vector<Weighted>& v) {
  int tot = 0;
  for (auto& e : v) tot += e.w;
  if (tot <= 0) return v.empty() ? 0 : v[0].what;
  int k = r.irange(tot);
  for (auto& e : v) { if (k < e.w) return e.what; k -= e.w; }
  return v.back().what;
}

// ---- pass 3: wall decor on the back wall (row 1) ---------------------------------------------------------------------
void wallDecor(Room& R, Building type, Role owner, int wealth) {
  std::vector<Weighted> pool;
  auto P = [](Prop p) { return (int)p; };
  switch (type) {
    case Building::Inn: pool = {{P(Prop::Antlers), 3}, {P(Prop::WallShield), 2}, {P(Prop::PanRack), 2}, {P(Prop::WallShelf), 3}, {P(Prop::Tapestry), 1}, {P(Prop::HerbBundle), 2}, {P(Prop::Painting), 1}}; break;
    case Building::Shop: pool = {{P(Prop::WallShelf), 4}, {P(Prop::HerbBundle), 3}, {P(Prop::Painting), 1}, {P(Prop::Wreath), 1}}; break;
    case Building::Smithy: pool = {{P(Prop::ToolRack), 4}, {P(Prop::WallShield), 3}, {P(Prop::PanRack), 1}}; break;
    case Building::Temple: pool = {{P(Prop::Tapestry), 5}, {P(Prop::Wreath), 1}}; break;
    case Building::Keep: pool = {{P(Prop::Tapestry), 4}, {P(Prop::WallShield), 3}, {P(Prop::Antlers), 2}, {P(Prop::Painting), 1}}; break;
    case Building::Tower: pool = {{P(Prop::WallShelf), 4}, {P(Prop::HerbBundle), 2}, {P(Prop::Painting), 2}, {P(Prop::Tapestry), 1}}; break;
    default:
      pool = {{P(Prop::HerbBundle), 3}, {P(Prop::WallShelf), 3}, {P(Prop::Wreath), 2}, {P(Prop::PanRack), 2}, {P(Prop::Painting), wealth >= 1 ? 2 : 0},
              {P(Prop::Antlers), owner == Role::Farmer ? 2 : 1}, {P(Prop::Tapestry), wealth >= 2 ? 2 : 0}, {P(Prop::ToolRack), owner == Role::Farmer ? 1 : 0}};
      break;
  }
  std::vector<uint8_t>& used = R.wallUsed;
  auto okAt = [&](int x) { return x >= 1 && x <= R.W - 2 && !used[(size_t)x] && R.m.at(x, 1) == Ground::InteriorWall && !R.m.propAt(x, 1); };
  // the temple's sun disc hangs above the altar, the keep's shield above the throne
  if (type == Building::Temple) { R.m.setProp(R.ex, 1, Prop::HolySymbol); used[(size_t)R.ex] = 1; }
  // windows (homes, inns, shops, towers): one or two, never side by side
  bool windows = type != Building::Smithy && type != Building::Temple && type != Building::Keep;
  if (windows) {
    int want = R.W >= 15 ? 2 : 1;
    for (int t = 0; t < 20 && want > 0; t++) {
      int x = 2 + R.r.irange(std::max(1, R.W - 4));
      if (!okAt(x) || !okAt(x - 1) || !okAt(x + 1)) continue;
      R.m.setProp(x, 1, Prop::Window);
      used[(size_t)x] = 1;
      want--;
    }
  }
  // sconces spaced through the room for light, then decor in the gaps
  int lastSconce = -99;
  for (int x = 2; x <= R.W - 3; x++) {
    if (x - lastSconce < (type == Building::Keep || type == Building::Temple ? 4 : 5)) continue;
    if (!okAt(x)) continue;
    R.m.setProp(x, 1, Prop::Sconce);
    used[(size_t)x] = 1;
    lastSconce = x;
  }
  float density = type == Building::Hut ? 0.35f : (0.5f + wealth * 0.1f);
  int lastDecor = -99;
  for (int x = 1; x <= R.W - 2; x++) {
    if (!okAt(x) || x - lastDecor < 2 || R.r.f() > density) continue;
    Prop p = (Prop)pick(R.r, pool);
    R.m.setProp(x, 1, p);
    used[(size_t)x] = 1;
    lastDecor = x;
  }
}

// ---- pass 4: small clutter on the floor (Map::deco) -------------------------------------------------------------------
void clutter(Room& R, Building type, Role owner, int wealth, const std::vector<int>& spawnTiles) {
  std::vector<Weighted> pool;
  auto D = [](Deco d) { return (int)d; };
  switch (type) {
    case Building::Inn: pool = {{D(Deco::Jugs), 4}, {D(Deco::Bottles), 3}, {D(Deco::Plates), 2}, {D(Deco::Bread), 2}, {D(Deco::Cheese), 1}, {D(Deco::Kindling), 1}, {D(Deco::Broom), 1}, {D(Deco::Sacks), 1}, {D(Deco::Bucket), 1}}; break;
    case Building::Shop: pool = {{D(Deco::Sacks), 3}, {D(Deco::Basket), 3}, {D(Deco::Bottles), 2}, {D(Deco::FruitBowl), 1}, {D(Deco::Cheese), 1}, {D(Deco::Bread), 1}, {D(Deco::Laundry), 1}}; break;
    case Building::Smithy: pool = {{D(Deco::Tools), 5}, {D(Deco::Bucket), 2}, {D(Deco::Kindling), 3}, {D(Deco::Sacks), 1}, {D(Deco::Broom), 1}}; break;
    case Building::Temple: pool = {{D(Deco::Candles), 5}, {D(Deco::Books), 2}, {D(Deco::Scrolls), 1}}; break;
    case Building::Keep: pool = {{D(Deco::Candles), 2}, {D(Deco::Jugs), 2}, {D(Deco::Books), 1}, {D(Deco::Plates), 1}, {D(Deco::Scrolls), 1}}; break;
    case Building::Tower: pool = {{D(Deco::Books), 5}, {D(Deco::Scrolls), 4}, {D(Deco::Candles), 3}, {D(Deco::Bottles), 2}}; break;
    default:
      pool = {{D(Deco::Basket), 3}, {D(Deco::PotsPans), 2}, {D(Deco::Laundry), 2}, {D(Deco::Jugs), 2}, {D(Deco::Bread), 1}, {D(Deco::Bucket), 2}, {D(Deco::Broom), 2},
              {D(Deco::Kindling), 2}, {D(Deco::FruitBowl), 1}, {D(Deco::Cheese), 1}, {D(Deco::Candles), wealth >= 1 ? 1 : 0}, {D(Deco::Books), wealth >= 2 ? 1 : 0}};
      if (owner == Role::Farmer || type == Building::Farmhouse) { pool.push_back({D(Deco::Sacks), 4}); pool.push_back({D(Deco::Straw), 3}); pool.push_back({D(Deco::Basket), 3}); }
      if (owner == Role::Mage) { pool.push_back({D(Deco::Books), 4}); pool.push_back({D(Deco::Scrolls), 3}); }
      if (owner == Role::Smith) pool.push_back({D(Deco::Tools), 4});
      break;
  }
  float density = type == Building::Hut ? 0.22f : (type == Building::Keep || type == Building::Temple ? 0.14f : 0.3f + wealth * 0.04f);
  for (int y = 2; y < R.H - 1; y++)
    for (int x = 1; x < R.W - 1; x++) {
      if (!R.floorT(x, y) || R.m.propAt(x, y) || R.lane[(size_t)R.I(x, y)] || R.m.decoAt(x, y)) continue;
      if (std::find(spawnTiles.begin(), spawnTiles.end(), R.I(x, y)) != spawnTiles.end()) continue;
      if (std::abs(x - R.ex) <= 1 && y >= R.H - 3) continue;
      int walls = (!R.floorT(x - 1, y)) + (!R.floorT(x + 1, y)) + (!R.floorT(x, y - 1)) + (!R.floorT(x, y + 1));
      int near = 0;
      for (int k = 0; k < 4; k++) {
        static const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
        int p = R.m.propAt(x + dx[k], y + dy[k]);
        if (p && propSolid((Prop)(p - 1))) near++;
      }
      float pr = walls >= 2 ? 0.85f : (walls == 1 ? 0.4f : 0.0f);
      if (near && walls) pr = std::min(1.0f, pr + 0.15f);
      if (R.r.f() < pr * density * 2.0f) {
        Deco d = (Deco)pick(R.r, pool);
        // no two of a kind side by side (a row of identical jugs reads as a pattern, not a home)
        for (int t = 0; t < 4; t++) {
          bool twin = false;
          for (int oy = -1; oy <= 1; oy++)
            for (int ox2 = -1; ox2 <= 1; ox2++) if ((ox2 || oy) && R.m.decoAt(x + ox2, y + oy) == (int)d) twin = true;
          if (!twin) break;
          d = (Deco)pick(R.r, pool);
        }
        if (d == Deco::Broom && walls == 0) d = Deco::Basket;   // brooms lean on walls
        R.deco(x, y, d);
      }
    }
}

// ---- pass 1 + 2: anchors and function groups per building type ------------------------------------------------------
void furnishHome(Room& R, Building type, Role owner, int wealth, bool& chestPlaced, int& tx, int& ty) {
  Rng& r = R.r;
  int W = R.W, H = R.H;
  bool bedLeft = r.f() < 0.5f;
  // anchors: the hearth on the back wall, the bed in a back corner
  int hx = bedLeft ? W - 3 - r.irange(std::max(1, W / 3 - 1)) : 2 + r.irange(std::max(1, W / 3 - 1));
  if (!R.group([&](int g) { return hearthAt(R, hx, g); })) {
    for (int x = 2; x <= W - 3; x++) if (R.group([&](int g) { return hearthAt(R, x, g); })) break;
  }
  int bx = bedLeft ? 1 : W - 2;
  bool bed = R.group([&](int g) {
    if (!backPiece(R, bx, Prop::Bed, g)) return false;
    int side = bedLeft ? 1 : -1;
    // the household: a cradle beside the bed, or a chest at its foot, or a wardrobe in the corner beyond the cradle
    if (r.f() < 0.3f && R.freeT(bx + side, 2)) R.put(bx + side, 2, Prop::Cradle, g);
    else if (wealth >= 1 && r.f() < 0.6f && R.freeT(bx + side, 2)) { R.put(bx + side, 2, Prop::Wardrobe, g); R.hideWall(bx + side, bx + side); }
    if (r.f() < 0.4f && R.freeT(bx, 3)) { R.put(bx, 3, Prop::Chest, g); chestPlaced = true; }
    return true;
  });
  if (!bed) for (int x = 1; x <= W - 2; x++) if (R.group([&](int g) { return backPiece(R, x, Prop::Bed, g); })) break;
  if (chestPlaced) { chestPlaced = false; for (int x = 1; x < W - 1; x++) if (R.m.propAt(x, 3) == (int)Prop::Chest + 1) chestPlaced = true; }
  // the table, with 2 to 4 seats
  bool big = W >= 11 && r.f() < 0.7f;
  Prop top = r.f() < 0.55f ? Prop::TableMeal : (owner == Role::Mage || (wealth >= 2 && r.f() < 0.4f) ? Prop::TableWork : Prop::TableSmall);
  int seats = 2 + r.irange(3);
  bool tableOk = tryAt(R, 30, 2, W - 3 - (big ? 1 : 0), 4, H - 4, [&](int x, int y, int g) {
    if (!tableSet(R, x, y, big ? 2 : 1, top, seats, false, g)) return false;
    tx = x; ty = y;
    return true;
  });
  if (tableOk && wealth >= 1 && r.f() < 0.65f) {
    static const Deco rugs[4] = {Deco::RugRed, Deco::RugBlue, Deco::RugGreen, Deco::RugGold};
    rug(R, tx - 1, ty - 1, tx + (big ? 2 : 1), ty + 1, rugs[r.irange(4)]);
  }
  // storage: a cupboard or shelf on the back wall, with crates or a barrel beside it
  tryAt(R, 12, 1, W - 2, 2, 2, [&](int x, int y, int g) {
    (void)y;
    Prop p = r.f() < 0.6f ? Prop::Cupboard : Prop::Shelf;
    if (!backPiece(R, x, p, g)) return false;
    int side = r.f() < 0.5f ? -1 : 1;
    if (R.freeT(x + side, 2)) R.put(x + side, 2, r.f() < 0.5f ? Prop::Barrel : Prop::Crate, g);
    return true;
  });
  // a craft corner: a loom or a spinning wheel with a basket; a scholar's desk with a bookshelf
  float craft = owner == Role::Mage ? 1.0f : (type == Building::Hut ? 0.35f : 0.55f);
  if (r.f() < craft) {
    bool scholar = owner == Role::Mage || (wealth >= 2 && r.f() < 0.35f);
    tryAt(R, 14, 1, W - 2, 2, 2, [&](int x, int y, int g) {
      (void)y;
      if (scholar) {
        if (!backPiece(R, x, Prop::Bookshelf, g)) return false;
        int side = x < W / 2 ? 1 : -1;
        if (!R.freeT(x + side, 2)) return false;
        R.put(x + side, 2, Prop::Desk, g);
        if (R.freeT(x + side, 3)) R.put(x + side, 3, Prop::Stool, g);
        return true;
      }
      Prop p = r.f() < 0.5f ? Prop::Loom : Prop::SpinningWheel;
      if (!backPiece(R, x, p, g)) return false;
      int side = x < W / 2 ? 1 : -1;
      if (R.freeT(x + side, 2)) R.deco(x + side, 2, Deco::Basket);
      if (p == Prop::SpinningWheel && R.freeT(x, 3)) R.put(x, 3, Prop::Stool, g);
      return true;
    });
  }
  // farm and hearth stores along the side walls
  if (type == Building::Farmhouse || owner == Role::Farmer) {
    tryAt(R, 12, 1, W - 2, H - 3, H - 2, [&](int x, int y, int g) {
      if (!R.freeT(x, y) || !R.gapOk(x, y, x, y, g) || (x != 1 && x != W - 2)) return false;
      R.put(x, y, Prop::Haystack, g);
      return true;
    });
  }
  tryAt(R, 12, 1, W - 2, 3, H - 2, [&](int x, int y, int g) {
    if ((x != 1 && x != W - 2) || !R.freeT(x, y) || !R.gapOk(x, y, x, y, g)) return false;
    R.put(x, y, r.f() < 0.5f ? Prop::Woodpile : Prop::Barrel, g);
    return true;
  });
  if (wealth >= 1)
    tryAt(R, 10, 1, W - 2, 3, H - 2, [&](int x, int y, int g) {
      if ((x != 1 && x != W - 2) || !R.freeT(x, y) || !R.gapOk(x, y, x, y, g)) return false;
      R.put(x, y, r.f() < 0.5f ? Prop::PlantPot : Prop::Crate, g);
      return true;
    });
}

void furnishInn(Room& R, int& ox, int& oy, std::vector<std::pair<int, int>>& seats) {
  Rng& r = R.r;
  int W = R.W, H = R.H;
  R.group([&](int g) { return hearthAt(R, W / 2 + 1, g); });
  // the bar: counter segments, kegs and a cupboard behind; the innkeeper stands behind it
  R.group([&](int g) {
    R.put(2, 4, Prop::CounterL, g);
    for (int x = 3; x <= 5; x++) R.put(x, 4, Prop::CounterM, g);
    R.put(6, 4, Prop::CounterR, g);
    R.put(2, 2, Prop::Barrel2, g); R.put(3, 2, Prop::Barrel2, g);
    R.put(5, 2, Prop::Cupboard, g);
    R.put(6, 2, Prop::Barrel, g);
    R.hideWall(2, 6);
    return true;
  });
  ox = 4; oy = 3;
  rug(R, W / 2, 3, W / 2 + 2, 4, Deco::RugRed);
  // rented beds along the back wall to the right, a wardrobe between them
  R.group([&](int g) {
    if (!backPiece(R, W - 2, Prop::Bed, g)) return false;
    if (R.freeT(W - 3, 2)) { R.put(W - 3, 2, Prop::Wardrobe, g); R.hideWall(W - 3, W - 3); }
    if (R.freeT(W - 4, 2)) R.put(W - 4, 2, Prop::Bed, g);
    return true;
  });
  // the common room: long tables with benches and round tables with stools
  // every spot in a shuffled order, so the room fills as far as the gap rule allows
  std::vector<int> spots;
  for (int y = 6; y <= H - 3; y++)
    for (int x = 2; x <= W - 3; x++) spots.push_back(y * W + x);
  for (size_t i = spots.size(); i > 1; i--) std::swap(spots[i - 1], spots[(size_t)r.irange((int)i)]);
  int want = 6 + r.irange(2);
  for (size_t t = 0; t < spots.size() && want > 0; t++) {
    bool lng = r.f() < 0.55f;
    int len = lng ? 2 + r.irange(2) : 1;
    int x = spots[t] % W, y = spots[t] / W;
    if (x + len - 1 > W - 3) continue;
    Prop top = r.f() < 0.6f ? Prop::TableMeal : Prop::TableSmall;
    if (R.group([&](int g) { return tableSet(R, x, y, len, top, 2 + r.irange(3), lng && r.f() < 0.6f, g); })) {
      want--;
      seats.push_back({x, y + 1});
    }
  }
}

void furnishShop(Room& R, int& ox, int& oy) {
  Rng& r = R.r;
  int W = R.W, H = R.H;
  // the counter with a gap in front of the door, the merchant behind it
  R.group([&](int g) {
    int segs[2][2] = {{2, R.ex - 1}, {R.ex + 1, W - 3}};
    for (auto& s : segs) {
      if (s[1] - s[0] < 1) { if (s[1] == s[0]) R.put(s[0], 4, Prop::TableSmall, g); continue; }
      R.put(s[0], 4, Prop::CounterL, g);
      for (int x = s[0] + 1; x < s[1]; x++) R.put(x, 4, Prop::CounterM, g);
      R.put(s[1], 4, Prop::CounterR, g);
    }
    return true;
  });
  ox = std::max(2, R.ex - 2); oy = 3;
  // shelves of wares along the back wall
  static const Prop wares[4] = {Prop::Cupboard, Prop::Shelf, Prop::Bookshelf, Prop::Shelf};
  for (int x = 1; x <= W - 2; x++) {
    if (x == ox || r.f() < 0.25f) continue;
    Prop p = x == 1 || x == W - 2 ? (r.f() < 0.5f ? Prop::Barrel : Prop::Crate) : wares[r.irange(4)];
    R.group([&](int g) { return backPiece(R, x, p, g); });
  }
  // goods on the shop floor
  static const Prop goods[5] = {Prop::Barrel, Prop::Crate, Prop::Barrel2, Prop::PlantPot, Prop::Crate};
  for (int k = 0; k < 6; k++)
    tryAt(R, 6, 1, W - 2, 6, H - 2, [&](int x, int y, int g) {
      if ((x != 1 && x != W - 2 && y != H - 2) || !R.freeT(x, y) || !R.gapOk(x, y, x, y, g)) return false;
      R.put(x, y, goods[r.irange(5)], g);
      return true;
    });
}

void furnishSmithy(Room& R, int& ox, int& oy) {
  Rng& r = R.r;
  int W = R.W, H = R.H;
  R.group([&](int g) { return hearthAt(R, 2, g, Prop::Forge); });
  R.group([&](int g) {
    if (!R.freeT(4, 4) || !R.gapOk(4, 4, 4, 4, g)) return false;
    R.put(4, 4, Prop::Anvil, g);
    return true;
  });
  ox = 5; oy = 4;
  R.group([&](int g) {
    if (!backPiece(R, W - 3, Prop::Workbench, g)) return false;
    if (R.freeT(W - 2, 2)) R.put(W - 2, 2, Prop::Barrel, g);
    return true;
  });
  R.group([&](int g) { return backPiece(R, 6, Prop::Crate, g); });
  static const Prop stores[4] = {Prop::Woodpile, Prop::Crate, Prop::Barrel, Prop::Crate};
  for (int k = 0; k < 4; k++)
    tryAt(R, 8, 1, W - 2, 4, H - 2, [&](int x, int y, int g) {
      if ((x != 1 && x != W - 2) || !R.freeT(x, y) || !R.gapOk(x, y, x, y, g)) return false;
      R.put(x, y, stores[r.irange(4)], g);
      return true;
    });
}

void furnishTemple(Room& R, int& ox, int& oy) {
  int W = R.W, H = R.H, ex = R.ex;
  R.group([&](int g) {
    R.put(ex - 1, 2, Prop::Filler, g); R.put(ex, 2, Prop::Altar, g); R.put(ex + 1, 2, Prop::Filler, g);
    R.hideWall(ex - 1, ex + 1);
    if (R.freeT(ex - 3, 2)) R.put(ex - 3, 2, Prop::Brazier, g);
    if (R.freeT(ex + 3, 2)) R.put(ex + 3, 2, Prop::Brazier, g);
    return true;
  });
  R.group([&](int g) { return backPiece(R, 1, Prop::Statue, g) && backPiece(R, W - 2, Prop::Statue, g); });
  // pews either side of the aisle
  for (int y = 5; y < H - 2; y += 2)
    R.group([&](int g) {
      for (int x = 2; x <= ex - 2; x++) if (R.freeT(x, y)) R.put(x, y, Prop::Bench, g);
      for (int x = ex + 2; x <= W - 3; x++) if (R.freeT(x, y)) R.put(x, y, Prop::Bench, g);
      return true;
    });
  rug(R, ex, 3, ex, H - 2, Deco::RugRed);
  ox = ex; oy = 3;
}

void furnishKeep(Room& R, int& ox, int& oy) {
  int W = R.W, H = R.H, ex = R.ex;
  R.group([&](int g) {
    R.put(ex - 1, 2, Prop::Filler, g); R.put(ex, 2, Prop::Throne, g); R.put(ex + 1, 2, Prop::Filler, g);
    R.hideWall(ex - 1, ex + 1);
    if (R.freeT(ex - 3, 2)) R.put(ex - 3, 2, Prop::Banner, g);
    if (R.freeT(ex + 3, 2)) R.put(ex + 3, 2, Prop::Banner, g);
    return true;
  });
  rug(R, ex - 1, 3, ex + 1, 4, Deco::RugGold);
  rug(R, ex, 5, ex, H - 2, Deco::RugRed);
  R.group([&](int g) {
    for (int x : {2, 3}) if (!backPiece(R, x, Prop::Bookshelf, g)) return false;
    return true;
  });
  R.group([&](int g) { return backPiece(R, W - 3, Prop::Bookshelf, g) && backPiece(R, W - 4, Prop::Cupboard, g); });
  // the long feasting table with benches
  R.group([&](int g) { return tableSet(R, 3, 8, 5, Prop::TableMeal, 0, true, g); });
  R.group([&](int g) { return tableSet(R, W - 8, 8, 5, Prop::TableMeal, 0, true, g); });
  for (auto& b : std::vector<std::pair<int, int>>{{2, 5}, {W - 3, 5}, {2, H - 3}, {W - 3, H - 3}})
    R.group([&](int g) {
      if (!R.freeT(b.first, b.second) || !R.gapOk(b.first, b.second, b.first, b.second, g)) return false;
      R.put(b.first, b.second, Prop::Brazier, g);
      return true;
    });
  ox = ex; oy = 3;
}

void furnishTower(Room& R, int& ox, int& oy) {
  Rng& r = R.r;
  int W = R.W, H = R.H, ex = R.ex;
  R.group([&](int g) { return backPiece(R, 1, Prop::Bookshelf, g) && backPiece(R, 2, Prop::Bookshelf, g); });
  R.group([&](int g) {
    if (!backPiece(R, W - 3, Prop::Desk, g)) return false;
    if (R.freeT(W - 2, 2)) { R.put(W - 2, 2, Prop::Bookshelf, g); R.hideWall(W - 2, W - 2); }
    if (R.freeT(W - 3, 3)) R.put(W - 3, 3, Prop::Stool, g);
    return true;
  });
  R.group([&](int g) {
    if (!R.freeT(ex, 5) || !R.gapOk(ex, 5, ex, 5, g)) return false;
    R.put(ex, 5, Prop::Cauldron, g);
    return true;
  });
  tryAt(R, 20, 2, W - 3, 5, H - 4, [&](int x, int y, int g) { return tableSet(R, x, y, 1, Prop::TableWork, 1 + r.irange(2), false, g); });
  tryAt(R, 8, 1, W - 2, 2, 2, [&](int x, int y, int g) { (void)y; return backPiece(R, x, r.f() < 0.5f ? Prop::Cupboard : Prop::Shelf, g); });
  tryAt(R, 8, 1, W - 2, 4, H - 2, [&](int x, int y, int g) {
    if ((x != 1 && x != W - 2) || !R.freeT(x, y) || !R.gapOk(x, y, x, y, g)) return false;
    R.put(x, y, Prop::PlantPot, g);
    return true;
  });
  ox = ex + 1; oy = 5;
}

void genInteriorV3(Map& m, const Bldg& b, uint32_t seed) {
  Rng r(seed ^ 0x5BD1E995u);
  m.kind = MapKind::Interior;
  m.seed = seed;
  int W = std::max(11, b.r.w * 2 + 3), H = std::max(9, b.r.h * 2 + 5);
  if (b.type == Building::Keep) { W = 23; H = 15; }
  if (b.type == Building::Temple) { W = 15; H = 13; }
  if (b.type == Building::Inn) { W = 17; H = 14; }   // a deeper common room than v2 (room for two rows of tables)
  if (W % 2 == 0) W++;
  bool stone = b.type == Building::Keep || b.type == Building::Temple || b.type == Building::StoneHouse || b.type == Building::Tower ||
               b.type == Building::Smithy;
  m.alloc(W, H, Ground::InteriorWall);
  for (int y = 2; y < H - 1; y++)
    for (int x = 1; x < W - 1; x++) m.setG(x, y, stone ? Ground::StoneFloor : Ground::WoodFloor);
  int ex = W / 2, ey = H - 1;
  m.setG(ex, ey, stone ? Ground::StoneFloor : Ground::WoodFloor);
  m.exitX = ex; m.exitY = ey;

  // the room's look: wall style on the back wall row
  art::RoomStyle rs = art::RoomStyle::Timber;
  switch (b.type) {
    case Building::Hut: case Building::Farmhouse: rs = art::RoomStyle::Log; break;
    case Building::StoneHouse: rs = art::RoomStyle::Stone; break;
    case Building::Smithy: rs = art::RoomStyle::Soot; break;
    case Building::Temple: case Building::Keep: rs = art::RoomStyle::Hall; break;
    case Building::Tower: rs = art::RoomStyle::Arcane; break;
    case Building::House: rs = r.f() < 0.25f ? art::RoomStyle::Log : art::RoomStyle::Timber; break;
    default: break;
  }
  for (int x = 1; x < W - 1; x++) m.deco[(size_t)(1 * W + x)] = (uint8_t)((int)Deco::WallTimber + (int)rs);

  Room R(m, r);
  R.W = W; R.H = H; R.ex = ex; R.ey = ey;
  R.grp.assign((size_t)W * H, -1);
  R.lane.assign((size_t)W * H, 0);
  R.wallUsed.assign((size_t)W, 0);
  for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) if (R.floorT(x, y)) R.floorTiles++;
  R.lane[(size_t)R.I(ex, ey)] = 1;
  R.lane[(size_t)R.I(ex, ey - 1)] = 1;
  R.lane[(size_t)R.I(ex, ey - 2)] = 1;
  int wealth = b.type == Building::Hut ? 0 : (b.type == Building::StoneHouse ? 2 : 1);
  if (b.type == Building::Keep || b.type == Building::Temple) wealth = 3;
  if (wealth > 0 && wealth < 3 && r.f() < 0.2f) wealth--;

  Spawn owner; owner.npc = true; owner.role = b.owner; owner.slot = 0; owner.site = b.site;
  int ox = W / 2, oy = 4;
  std::vector<Spawn> extra;
  std::vector<std::pair<int, int>> innSeats;
  bool chest = false;
  switch (b.type) {
    case Building::Inn: furnishInn(R, ox, oy, innSeats); break;
    case Building::Shop: furnishShop(R, ox, oy); break;
    case Building::Smithy: furnishSmithy(R, ox, oy); break;
    case Building::Temple: furnishTemple(R, ox, oy); break;
    case Building::Keep: furnishKeep(R, ox, oy); break;
    case Building::Tower: furnishTower(R, ox, oy); break;
    default: {
      int tx = W / 2, ty = H / 2;
      furnishHome(R, b.type, b.owner, wealth, chest, tx, ty);
      ox = tx; oy = ty + 1;
      break;
    }
  }
  (void)chest;
  wallDecor(R, b.type, b.owner, wealth);

  // spawns: the owner at their post, inn guests by the tables, the jarl's guards beside the throne
  std::vector<int> taken;
  bool ownerHome = b.owner != Role::Villager || r.f() < 0.75f;
  if (ownerHome) { spawnNear(R, owner, ox, oy, taken); m.spawns.push_back(owner); }
  if (b.type == Building::Inn)
    for (int k = 1; k <= 2; k++) {
      Spawn p; p.npc = true; p.role = Role::Villager; p.slot = k; p.site = b.site;
      auto at = k - 1 < (int)innSeats.size() ? innSeats[(size_t)k - 1] : std::pair<int, int>{9 + k * 2, 7};
      spawnNear(R, p, at.first, at.second, taken);
      m.spawns.push_back(p);
    }
  if (b.type == Building::Keep)
    for (int k = 1; k <= 2; k++) {
      Spawn gd; gd.npc = true; gd.role = Role::Guard; gd.slot = k; gd.site = b.site;
      spawnNear(R, gd, ex + (k == 1 ? -2 : 2), 4, taken);
      m.spawns.push_back(gd);
    }
  clutter(R, b.type, b.owner, wealth, taken);
  m.rebuildSolid();
}
// ===================================================================================== interiors, WORLDGEN_V7 (M0b)
// Rooms and storeys (VISION_PLAN 15.7): the generator lives in rpg/sim/interior_v4.cpp.
void genInteriorV4(Map& m, const Bldg& b, uint32_t seed, int floor) { genInteriorRooms(m, b, seed, floor); }
}  // namespace

void genInterior(Map& m, const Bldg& b, uint32_t seed, int floor) {
  m = Map();
  if (b.genVer >= WORLDGEN_V7) genInteriorV4(m, b, seed, std::clamp(floor, 0, b.floors() - 1));
  else if (b.genVer >= WORLDGEN_V3) genInteriorV3(m, b, seed);
  else genInteriorV2(m, b, seed);
}
