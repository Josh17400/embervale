// M0b interiors (WORLDGEN_V7): rooms and storeys that make sense (VISION_PLAN 15.7, binding).
//
// A building is planned once per call, for all of its floors (cheap: geometry only), so the floors agree:
//   1. a template per building type picks the floor size from the footprint and carves rooms out of each floor
//      (rpg/sim/rooms.h contract: E-W partitions are a cap row over a face row, N-S partitions one column, doors are
//      single doorway tiles with wall on both sides); whatever is left is the floor's main room (common room, hall,
//      nave, corridor...);
//   2. the stairs: floor f's StairsUp stands below a horizontal wall face, and floor f+1's StairsDown on the same tile;
//   3. doors from every carved room into its neighbour, clear of the stairs.
// If a template's random parameters do not fit (a door with no legal place, stairs that do not line up) it draws again;
// after many misses a plain single-room plan is used.
// The floor being generated is then furnished room by room (anchors against walls, function groups with a gap rule,
// wall decor on every visible wall face, clutter), with a BFS after each group: every doorway, the stairs, the entrance
// and every usable object stay reachable and at least 60 % of the floor stays walkable.
#include "rpg/sim/interior_v4.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <initializer_list>
#include <utility>
#include <vector>
#include "rpg/sim/deco.h"

using art::Building;
using art::Prop;

namespace {

// ===================================================================================================== geometry
struct RoomDef {
  RoomKind kind = RoomKind::Hall;
  IRect r;
  bool carved = false;
  int doorX = -1, doorY = -1;   // the doorway tile with the door prop (an E-W door: its face-row tile)
  bool doorH = false;
  int8_t guest = -1;
  int bedX = -1, bedY = -1;     // set while furnishing
};

struct Geo {
  int W = 0, H = 0;
  std::vector<uint8_t> wall;     // 1 = wall
  std::vector<int8_t> room;      // room index per floor tile, -1 for walls
  std::vector<RoomDef> rooms;    // rooms[0] is the main room: what is left after carving
  std::vector<int> doorTiles;    // every doorway tile (an E-W doorway has two)
  std::vector<int> approach;     // the tiles in front of and behind every doorway
  std::vector<int> keep;         // stairs and arrival tiles: no doorway may touch them
  Stairs up, down;
  int up2 = -1, down2 = -1;      // the second tile of a two-tile flight (tile index), -1 for a one-tile flight

  int I(int x, int y) const { return y * W + x; }
  bool in(int x, int y) const { return x >= 0 && y >= 0 && x < W && y < H; }
  bool isWall(int x, int y) const { return !in(x, y) || wall[(size_t)I(x, y)] != 0; }
  bool isFloor(int x, int y) const { return in(x, y) && wall[(size_t)I(x, y)] == 0; }
  int roomOf(int x, int y) const { return in(x, y) ? room[(size_t)I(x, y)] : -1; }

  void init(int w, int h, RoomKind mainKind) {
    W = w; H = h;
    wall.assign((size_t)W * H, 1);
    room.assign((size_t)W * H, -1);
    for (int y = 2; y <= H - 2; y++)
      for (int x = 1; x <= W - 2; x++) wall[(size_t)I(x, y)] = 0;
    rooms.assign(1, RoomDef());
    rooms[0].kind = mainKind;
    doorTiles.clear(); approach.clear(); keep.clear();
    up = Stairs(); down = Stairs();
    up2 = down2 = -1;
  }

  // Carves the room [x0..x1] x [y0..y1] (floor tiles, inclusive) and walls it off from the rest of the floor: an E-W
  // partition (2 rows) on a side that does not touch the outer wall north or south, a N-S partition (1 column) east
  // or west. Rooms may share partitions. Returns the room index, or -1 when it does not fit.
  int carve(int x0, int y0, int x1, int y1, RoomKind k) {
    if (x1 - x0 < 1 || y1 - y0 < 1 || x0 < 1 || y0 < 2 || x1 > W - 2 || y1 > H - 2) return -1;
    const bool n = y0 > 2, s = y1 < H - 2, w = x0 > 1, e = x1 < W - 2;
    if ((n && y0 < 5) || (s && y1 > H - 5) || (w && x0 < 3) || (e && x1 > W - 4)) return -1;
    for (int y = y0; y <= y1; y++)
      for (int x = x0; x <= x1; x++)
        if (wall[(size_t)I(x, y)] || room[(size_t)I(x, y)] >= 1) return -1;
    std::vector<int> wt;
    const int ex0 = w ? x0 - 1 : x0, ex1 = e ? x1 + 1 : x1;
    for (int x = ex0; x <= ex1; x++) {
      if (n) { wt.push_back(I(x, y0 - 2)); wt.push_back(I(x, y0 - 1)); }
      if (s) { wt.push_back(I(x, y1 + 1)); wt.push_back(I(x, y1 + 2)); }
    }
    for (int y = y0; y <= y1; y++) {
      if (w) wt.push_back(I(x0 - 1, y));
      if (e) wt.push_back(I(x1 + 1, y));
    }
    for (int t : wt) if (room[(size_t)t] >= 1) return -1;
    for (int t : wt) wall[(size_t)t] = 1;
    RoomDef R;
    R.kind = k;
    R.r = IRect{x0, y0, x1 - x0 + 1, y1 - y0 + 1};
    R.carved = true;
    rooms.push_back(R);
    int ri = (int)rooms.size() - 1;
    for (int y = y0; y <= y1; y++)
      for (int x = x0; x <= x1; x++) room[(size_t)I(x, y)] = (int8_t)ri;
    return ri;
  }

  // The floor left over is the main room: it must be one connected piece.
  bool finish() {
    int start = -1;
    for (int i = 0; i < W * H; i++) if (!wall[(size_t)i] && room[(size_t)i] < 0) { start = i; break; }
    if (start < 0) return false;
    std::vector<int> q{start};
    room[(size_t)start] = 0;
    for (size_t h = 0; h < q.size(); h++) {
      int x = q[h] % W, y = q[h] / W;
      static const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
      for (int k = 0; k < 4; k++) {
        int nx = x + dx[k], ny = y + dy[k];
        if (!isFloor(nx, ny) || room[(size_t)I(nx, ny)] >= 0) continue;
        room[(size_t)I(nx, ny)] = 0;
        q.push_back(I(nx, ny));
      }
    }
    for (int i = 0; i < W * H; i++) if (!wall[(size_t)i] && room[(size_t)i] < 0) return false;
    bbox();
    return q.size() >= 6;
  }
  void bbox() {
    std::vector<int> x0(rooms.size(), W), y0(rooms.size(), H), x1(rooms.size(), -1), y1(rooms.size(), -1);
    for (int y = 0; y < H; y++)
      for (int x = 0; x < W; x++) {
        int ri = room[(size_t)I(x, y)];
        if (ri < 0 || wall[(size_t)I(x, y)]) continue;
        x0[ri] = std::min(x0[ri], x); y0[ri] = std::min(y0[ri], y); x1[ri] = std::max(x1[ri], x); y1[ri] = std::max(y1[ri], y);
      }
    for (size_t i = 0; i < rooms.size(); i++)
      if (x1[i] >= 0) rooms[i].r = IRect{x0[i], y0[i], x1[i] - x0[i] + 1, y1[i] - y0[i] + 1};
  }

  bool nearKeep(int t) const {
    int x = t % W, y = t / W;
    for (int k : keep)
      if (std::abs(k % W - x) <= 1 && std::abs(k / W - y) <= 1) return true;
    return false;
  }
  bool nearDoor(int t) const {
    int x = t % W, y = t / W;
    for (int d : doorTiles)
      if (std::abs(d % W - x) <= 1 && std::abs(d / W - y) <= 1) return true;
    return false;
  }
  bool isKeep(int t) const { return std::find(keep.begin(), keep.end(), t) != keep.end(); }

  // A door from carved room ri into room `target`, on any of ri's partitions, as near (px, py) as the rules allow:
  // wall on both sides along the partition (never at a T or corner), never next to another doorway, its approaches
  // clear of the stairs.
  bool door(int ri, int target, int px, int py, Rng& r) {
    const RoomDef& R = rooms[(size_t)ri];
    if (!R.carved) return false;
    const int x0 = R.r.x, y0 = R.r.y, x1 = R.r.x + R.r.w - 1, y1 = R.r.y + R.r.h - 1;
    struct Cand { int a, b, inA, inB; bool h; int score; };   // a: door-prop tile, b: second doorway tile (E-W) or -1
    std::vector<Cand> cs;
    auto inT = [&](int x, int y) { return roomOf(x, y) == target && isFloor(x, y); };
    auto inR = [&](int x, int y) { return roomOf(x, y) == ri && isFloor(x, y); };
    auto add = [&](Cand c, int x, int y) {
      if (nearDoor(c.a) || (c.b >= 0 && nearDoor(c.b))) return;
      if (nearKeep(c.a) || (c.b >= 0 && nearKeep(c.b)) || isKeep(c.inA) || isKeep(c.inB)) return;
      c.score = std::abs(x - px) * 2 + std::abs(y - py) * 2 + r.irange(3);
      cs.push_back(c);
    };
    // south partition (cap y1+1, face y1+2): the target lies south
    if (y1 < H - 2)
      for (int x = x0 + 1; x <= x1 - 1; x++) {
        if (!isWall(x, y1 + 1) || !isWall(x, y1 + 2) || !inR(x, y1) || !inT(x, y1 + 3)) continue;
        if (!isWall(x - 1, y1 + 1) || !isWall(x + 1, y1 + 1) || !isWall(x - 1, y1 + 2) || !isWall(x + 1, y1 + 2)) continue;
        if (!inT(x - 1, y1 + 3) || !inT(x + 1, y1 + 3) || !inR(x - 1, y1) || !inR(x + 1, y1)) continue;
        add(Cand{I(x, y1 + 2), I(x, y1 + 1), I(x, y1), I(x, y1 + 3), true, 0}, x, y1 + 2);
      }
    // north partition (cap y0-2, face y0-1): the target lies north
    if (y0 > 2)
      for (int x = x0 + 1; x <= x1 - 1; x++) {
        if (!isWall(x, y0 - 2) || !isWall(x, y0 - 1) || !inR(x, y0) || !inT(x, y0 - 3)) continue;
        if (!isWall(x - 1, y0 - 2) || !isWall(x + 1, y0 - 2) || !isWall(x - 1, y0 - 1) || !isWall(x + 1, y0 - 1)) continue;
        if (!inT(x - 1, y0 - 3) || !inT(x + 1, y0 - 3) || !inR(x - 1, y0) || !inR(x + 1, y0)) continue;
        add(Cand{I(x, y0 - 1), I(x, y0 - 2), I(x, y0), I(x, y0 - 3), true, 0}, x, y0 - 1);
      }
    // west / east partitions (one column)
    for (int side = 0; side < 2; side++) {
      int wx = side == 0 ? x0 - 1 : x1 + 1, tx = side == 0 ? x0 - 2 : x1 + 2, rx = side == 0 ? x0 : x1;
      if (wx <= 0 || wx >= W - 1) continue;
      for (int y = y0 + 1; y <= y1 - 1; y++) {
        if (!isWall(wx, y) || !isWall(wx, y - 1) || !isWall(wx, y + 1)) continue;
        if (!inR(rx, y) || !inT(tx, y) || !inT(tx, y - 1) || !inT(tx, y + 1) || !inR(rx, y - 1) || !inR(rx, y + 1)) continue;
        add(Cand{I(wx, y), -1, I(rx, y), I(tx, y), false, 0}, wx, y);
      }
    }
    if (cs.empty()) return false;
    const Cand* best = &cs[0];
    for (const Cand& c : cs) if (c.score < best->score) best = &c;
    Cand c = *best;
    wall[(size_t)c.a] = 0; room[(size_t)c.a] = (int8_t)ri;
    doorTiles.push_back(c.a);
    if (c.b >= 0) { wall[(size_t)c.b] = 0; room[(size_t)c.b] = (int8_t)ri; doorTiles.push_back(c.b); }
    approach.push_back(c.inA); approach.push_back(c.inB);
    RoomDef& RR = rooms[(size_t)ri];
    RR.doorX = c.a % W; RR.doorY = c.a / W; RR.doorH = c.h;
    return true;
  }
};

// A flight of stairs is two tiles wide where it fits: (x, y), the trigger the Map records, and its companion (x + s, y)
// (s = 0: a one-tile flight, only in the plain fallback plan). Both tiles carry the stairs prop and both take you.
// flightOk: both tiles are floor of one room; a flight up also needs a wall face right above both tiles, running on
// past them (the back wall's face row or an E-W partition's face row), so the flight climbs north into the wall
bool flightOk(const Geo& g, int x, int y, int s, bool up) {
  int ri = g.roomOf(x, y);
  if (ri < 0) return false;
  for (int k = 0; k <= (s ? 1 : 0); k++) {
    int tx = x + k * s;
    if (!g.isFloor(tx, y) || g.roomOf(tx, y) != ri) return false;
    if (up && (!g.isWall(tx - 1, y - 1) || !g.isWall(tx, y - 1) || !g.isWall(tx + 1, y - 1) || !g.isWall(tx, y - 2))) return false;
  }
  return true;
}
// the arrival tile: next to the trigger tile (never the companion). Below (the foot of a flight up) in front of the
// flight first, else beside it. Above (a stairwell down) beside the opening first: arriving right south of the hole
// would leave a player who pushes on north (the way they climbed) walking straight back into it.
bool arrivalOf(const Geo& g, int x, int y, int s, bool down, int& ax, int& ay) {
  int ri = g.roomOf(x, y);
  int side = s ? -s : 1;
  // (above, a two-tile opening is also flanked on its companion's far side: (x + 2s, y))
  const int cu[5][2] = {{x, y + 1}, {x + side, y}, {x - side, y}, {x, y - 1}, {x, y - 1}};
  const int cd[5][2] = {{x + side, y}, {x + 2 * s, y}, {x - side, y}, {x, y + 1}, {x, y - 1}};
  const int (*c)[2] = down ? cd : cu;
  for (int k = 0; k < (down ? 5 : 3); k++) {
    int tx = c[k][0], ty = c[k][1];
    if ((s && tx == x + s && ty == y) || (tx == x && ty == y)) continue;
    if (g.isFloor(tx, ty) && g.roomOf(tx, ty) == ri) { ax = tx; ay = ty; return true; }
  }
  return false;
}

// Splits the columns a..b into rooms min..max wide with one wall column between neighbours.
bool splitSeg(int a, int b, int minW, int maxW, Rng& r, std::vector<std::pair<int, int>>& out) {
  int L = b - a + 1;
  if (L <= 0) return true;
  if (L < minW) return false;
  int kmin = std::max(1, (L + 1 + maxW) / (maxW + 1));
  int kmax = (L + 1) / (minW + 1);
  if (kmax < kmin) kmax = kmin = 1;
  int k = kmin + r.irange(kmax - kmin + 1);
  int tot = L - (k - 1);
  std::vector<int> w((size_t)k, tot / k);
  for (int i = 0; i < tot % k; i++) w[(size_t)i]++;
  for (int t = 0; t < k * 2; t++) {
    int i = r.irange(k), j = r.irange(k);
    if (i != j && w[(size_t)i] > minW && w[(size_t)j] < maxW) { w[(size_t)i]--; w[(size_t)j]++; }
  }
  int x = a;
  for (int i = 0; i < k; i++) { out.push_back({x, x + w[(size_t)i] - 1}); x += w[(size_t)i] + 1; }
  return true;
}

// ===================================================================================================== the plan
struct Plan {
  const Bldg* b = nullptr;
  Building type = Building::House;
  int W = 11, H = 9, ex = 5, floors = 1;
  bool mir = false;
  int var = 0, kd = 4, wealth = 1;
  std::vector<Geo> geo;
  // anchors for the furnishers, in map coordinates: the inn's bar or the shop's counter (shelves on row barY against
  // the partition face, the keeper's walkway barY+1, the counter on barY+2 over barX0..barX1, the gap at flapX)
  int barY = -1, barX0 = -1, barX1 = -1, flapX = -1;
  int forgeX = -1;      // smithy: the forge's centre column on the back wall
  int X(int x) const { return mir ? W - 1 - x : x; }
  int carve(Geo& g, int x0, int y0, int x1, int y1, RoomKind k) const {
    int a = X(x0), c = X(x1);
    return g.carve(std::min(a, c), y0, std::max(a, c), y1, k);
  }
};

int oddUp(int v) { return v % 2 == 0 ? v + 1 : v; }

// stairs from floor f up to f+1 with trigger (x, y) and companion (x + s, y) (map coordinates). Checks both floors
// and records them.
bool trySetStairs(Plan& P, int f, int x, int y, int s, bool needPublic) {
  Geo& a = P.geo[(size_t)f];
  Geo& b = P.geo[(size_t)f + 1];
  int ax, ay, bx, by;
  if (!flightOk(a, x, y, s, true) || !flightOk(b, x, y, s, false)) return false;
  if (!arrivalOf(a, x, y, s, false, ax, ay) || !arrivalOf(b, x, y, s, true, bx, by)) return false;
  if (needPublic && roomPrivate(a.rooms[(size_t)a.roomOf(x, y)].kind) && a.rooms[(size_t)a.roomOf(x, y)].kind != RoomKind::Stockroom) return false;
  if (b.roomOf(x, y) != 0) return false;   // upstairs the flight arrives in the corridor / landing
  // the entrance lane and the other staircase of this floor stay clear
  for (int k = 0; k <= (s ? 1 : 0); k++) {
    int tx = x + k * s;
    if (f == 0 && std::abs(tx - P.ex) <= 1 && y >= P.H - 4) return false;
    for (int t : a.keep) if (std::abs(t % a.W - tx) <= 2 && std::abs(t / a.W - y) <= 2) return false;
    for (int t : b.keep) if (std::abs(t % b.W - tx) <= 2 && std::abs(t / b.W - y) <= 2) return false;
  }
  a.up = Stairs{x, y, ax, ay};
  b.down = Stairs{x, y, bx, by};
  a.keep.push_back(a.I(x, y)); a.keep.push_back(a.I(ax, ay));
  b.keep.push_back(b.I(x, y)); b.keep.push_back(b.I(bx, by));
  if (s) {
    a.up2 = a.I(x + s, y); b.down2 = b.I(x + s, y);
    a.keep.push_back(a.up2); b.keep.push_back(b.down2);
  }
  return true;
}
// a two-tile flight that covers column x (on either side of it); a one-tile flight only when allowSingle
bool setStairs(Plan& P, int f, int x, int y, bool needPublic = true, bool allowSingle = false) {
  static const int cand[4][2] = {{0, 1}, {0, -1}, {1, -1}, {-1, 1}};
  for (auto& c : cand)
    if (trySetStairs(P, f, x + c[0], y, c[1], needPublic)) return true;
  return allowSingle && trySetStairs(P, f, x, y, 0, needPublic);
}

RoomKind mainKindOf(Building t, int f, int floors) {
  if (f > 0 && t == Building::Windmill) return RoomKind::Workshop;   // (M1 economy) the millstone loft
  if (f > 0) return t == Building::Tower ? (f == floors - 1 ? RoomKind::Bedroom : RoomKind::Study) : RoomKind::Corridor;
  switch (t) {
    // (M1 economy) the production buildings: the tower mill's stone floor stores the grain and the flour; the trades
    // sell over a counter; the mills, the sawmill and the tannery are work halls; the granary is one great store
    case Building::Windmill: case Building::Granary: return RoomKind::Storeroom;
    case Building::Bakery: case Building::Butcher: case Building::Fishmonger: case Building::Weaver: return RoomKind::Shopfloor;
    case Building::Smelter: return RoomKind::Forge;
    case Building::Watermill: case Building::Sawmill: case Building::Tanner: return RoomKind::Workshop;
    case Building::Inn: return RoomKind::Common;
    case Building::Shop: return RoomKind::Shopfloor;
    case Building::Smithy: return RoomKind::Forge;
    case Building::Temple: return RoomKind::Nave;
    case Building::Keep: case Building::Palace: return RoomKind::ThroneHall;
    case Building::Barracks: return RoomKind::Hall;
    case Building::Tower: return RoomKind::Workshop;
    case Building::Hut: return RoomKind::Cottage;
    default: return RoomKind::Hall;
  }
}

// guests are numbered back row first, west to east (the innkeeper's "room 1, 2, 3")
void numberGuests(Geo& g) {
  std::vector<int> ids;
  for (int i = 1; i < (int)g.rooms.size(); i++) if (g.rooms[(size_t)i].kind == RoomKind::GuestRoom) ids.push_back(i);
  std::sort(ids.begin(), ids.end(), [&](int a, int b) {
    const IRect &ra = g.rooms[(size_t)a].r, &rb = g.rooms[(size_t)b].r;
    return ra.y != rb.y ? ra.y < rb.y : ra.x < rb.x;
  });
  for (size_t k = 0; k < ids.size(); k++) g.rooms[(size_t)ids[k]].guest = (int8_t)k;
}

// rooms along the back wall (rows 2..depth) over the columns a..b (unmirrored), leaving a two-column nook at nook,
// nook+1 (unmirrored, -1 none) that reaches the back wall (the stairwell); and along the front wall (rows y0..H-2)
// if y0 >= 0
bool carveRows(Plan& P, Geo& g, int depth, int nook, int frontY, int minW, int maxW, Rng& r, std::vector<int>& back, std::vector<int>& front) {
  std::vector<std::pair<int, int>> segs;
  if (nook >= 0) {
    if (!splitSeg(1, nook - 2, minW, maxW, r, segs) || !splitSeg(nook + 3, P.W - 2, minW, maxW, r, segs)) return false;
  } else if (!splitSeg(1, P.W - 2, minW, maxW, r, segs)) return false;
  for (auto& s : segs) {
    int ri = P.carve(g, s.first, 2, s.second, depth, RoomKind::Bedroom);
    if (ri < 0) return false;
    back.push_back(ri);
  }
  if (frontY >= 0) {
    std::vector<std::pair<int, int>> fs;
    if (!splitSeg(1, P.W - 2, minW, maxW, r, fs)) return false;
    for (auto& s : fs) {
      int ri = P.carve(g, s.first, frontY, s.second, P.H - 2, RoomKind::Bedroom);
      if (ri < 0) return false;
      front.push_back(ri);
    }
  }
  return true;
}
// a valid nook (columns nx, nx+1, unmirrored) for carveRows: both segments empty or at least minW wide
bool nookOk(int W, int nx, int minW) {
  int l = nx - 2, rr = W - 4 - nx;
  return nx >= 1 && nx + 1 <= W - 2 && (l <= 0 || l >= minW) && (rr <= 0 || rr >= minW);
}
// doors from every room in ids into room 0 (the corridor), from its side facing it
bool doorsTo(Geo& g, const std::vector<int>& ids, Rng& r) {
  for (int ri : ids) {
    const IRect& R = g.rooms[(size_t)ri].r;
    int px = R.x + 1 + r.irange(std::max(1, R.w - 2));
    if (!g.door(ri, 0, px, R.y < 4 ? R.y + R.h + 1 : R.y - 1, r)) return false;
  }
  return true;
}

// ---- INN: ground = common room with the bar under the kitchen, a storeroom, stairs; upstairs = rented rooms and the
// innkeeper's own along a corridor
bool planInn(Plan& P, Rng& r) {
  const int W = P.W, H = P.H;
  P.kd = (H >= 15 && r.f() < 0.4f) ? 5 : 4;
  const int kd = P.kd;
  int kw = 5 + r.irange(W >= 19 ? 3 : 2);
  bool store = r.f() < 0.55f;
  int sw = 3 + r.irange(2);
  int al0 = kw + 2, al1 = store ? W - 3 - sw : W - 2;   // the common room's own stretch of back wall (unmirrored)
  if (store && al1 - al0 + 1 < 3) { store = false; al1 = W - 2; }
  const bool dbl = kd + 7 <= H - 3;   // rooms on both sides of the corridor when two rows fit in front
  int mode = store && r.f() < 0.5f ? 0 : 1;   // 0: stairs under the storeroom's partition, 1: at the back wall
  if (P.floors < 2) mode = -1;
  // stairs column (unmirrored)
  int sxU = -1;
  if (mode == 0) sxU = W - 2 - r.irange(sw);
  if (mode == 1) {
    std::vector<int> c;
    for (int x = al0; x + 1 <= al1; x++) if (nookOk(W, x, 3) && (x == al0 || x + 1 == al1 || al1 - al0 >= 6)) c.push_back(x);
    if (c.empty()) return false;
    sxU = r.f() < 0.3f ? c.back() : c[(size_t)r.irange((int)c.size())];   // (M1: not always at the far end)
    if (al1 - al0 + 1 < 5 && sxU != al0 && sxU + 1 != al1) return false;   // leave the hearth its three tiles
  }
  // ground floor
  Geo& g0 = P.geo[0];
  g0.init(W, H, RoomKind::Common);
  int kit = P.carve(g0, 1, 2, kw, kd, RoomKind::Kitchen);
  int sto = store ? P.carve(g0, W - 1 - sw, 2, W - 2, kd, RoomKind::Storeroom) : 0;
  if (kit < 0 || sto < 0 || !g0.finish()) return false;
  P.barY = kd + 3;
  P.barX0 = std::min(P.X(1), P.X(kw - 1)); P.barX1 = std::max(P.X(1), P.X(kw - 1));
  P.flapX = P.X(kw);
  // upstairs
  if (P.floors >= 2) {
    Geo& g1 = P.geo[1];
    g1.init(W, H, RoomKind::Corridor);
    std::vector<int> back, front;
    // (M1) inns differ upstairs: room widths (cramped cells or roomy chambers), where the innkeeper sleeps, and a
    // linen room or a guests' sitting room in the bigger ones
    const int style = r.irange(3);
    const int minW = style == 2 ? 4 : 3, maxW = style == 0 ? 4 : style == 1 ? 5 : 6;
    if (!carveRows(P, g1, kd, mode == 1 ? sxU : -1, dbl ? kd + 7 : -1, minW, maxW, r, back, front)) {
      g1.init(W, H, RoomKind::Corridor);
      back.clear(); front.clear();
      if (!carveRows(P, g1, kd, mode == 1 ? sxU : -1, dbl ? kd + 7 : -1, 3, 5, r, back, front)) return false;
    }
    if (!g1.finish()) return false;
    if (!setStairs(P, 0, P.X(sxU), mode == 0 ? kd + 3 : 2)) return false;
    // the innkeeper's room: the quiet end (the back room farthest from the stairs), or the biggest front room, or a
    // back room at random; the rest are let
    int own = back[0];
    const float ow = r.f();
    if (ow < 0.5f || front.empty()) {
      for (int ri : back)
        if (std::abs(g1.rooms[(size_t)ri].r.cx() - P.X(sxU)) > std::abs(g1.rooms[(size_t)own].r.cx() - P.X(sxU))) own = ri;
    } else if (ow < 0.8f) {
      own = front[0];
      for (int ri : front) if (g1.rooms[(size_t)ri].r.w > g1.rooms[(size_t)own].r.w) own = ri;
    } else own = back[(size_t)r.irange((int)back.size())];
    int guests = 0;
    for (int ri : back) { g1.rooms[(size_t)ri].kind = ri == own ? RoomKind::OwnerRoom : RoomKind::GuestRoom; guests += ri != own; }
    for (int ri : front) { g1.rooms[(size_t)ri].kind = ri == own ? RoomKind::OwnerRoom : RoomKind::GuestRoom; guests += ri != own; }
    // a linen store (or, rarely, a sitting room for the guests) in place of one room when there are rooms to spare
    if (guests >= 5 && r.f() < 0.6f) {
      std::vector<int> all;
      for (int ri : back) if (ri != own) all.push_back(ri);
      for (int ri : front) if (ri != own) all.push_back(ri);
      int pickI = all[(size_t)r.irange((int)all.size())];
      g1.rooms[(size_t)pickI].kind = r.f() < 0.7f ? RoomKind::Storeroom : RoomKind::Study;
      guests--;
    }
    if (guests < 2) return false;
    if (!doorsTo(g1, back, r) || !doorsTo(g1, front, r)) return false;
    numberGuests(g1);
  }
  // ground floor doors: the kitchen opens behind the bar, the storeroom into the common room
  if (!g0.door(kit, 0, P.X(2 + r.irange(std::max(1, kw - 2))), kd + 2, r)) return false;
  if (store) {
    const IRect& R = g0.rooms[(size_t)sto].r;
    if (!g0.door(sto, 0, R.x + r.irange(R.w), r.f() < 0.5f ? kd + 2 : 3, r)) return false;
  }
  // the kitchen door must open behind the counter
  int dx = g0.rooms[(size_t)kit].doorX;
  if (dx < P.barX0 || dx > P.barX1 || g0.rooms[(size_t)kit].doorY != kd + 2) return false;
  return true;
}

// ---- HOUSE / STONE HOUSE: one storey = hall + private bedroom (cottage for the smallest); two = hall and kitchen
// below, bedrooms (and a study in a wealthy home) above
bool planHouse(Plan& P, Rng& r) {
  const int W = P.W, H = P.H;
  const Bldg& b = *P.b;
  Geo& g0 = P.geo[0];
  if (P.floors == 1) {
    if (b.type == Building::House && b.r.w <= 3 && b.r.h <= 2 && r.f() < 0.4f) {
      g0.init(W, H, RoomKind::Cottage);
      P.var = 0;
      return g0.finish();
    }
    g0.init(W, H, RoomKind::Hall);
    P.var = 1 + r.irange(W >= 13 ? 5 : 3);
    if (P.var == 3 && W < 13) P.var = 4;
    int bd = (H >= 12 && r.f() < 0.5f) ? 5 : 4;
    if (P.var == 4 || P.var == 5) {   // the bedroom in a front corner (its door in the partition facing the hall),
                                      // and in a wider home a pantry in the opposite back corner
      int bw = 3 + r.irange(2), d = 3 + (H >= 12 ? r.irange(2) : 0);
      int bed = P.carve(g0, 1, H - 1 - d, bw, H - 2, RoomKind::Bedroom);
      int pan = P.var == 5 ? P.carve(g0, W - 4, 2, W - 2, 4, r.f() < 0.5f ? RoomKind::Kitchen : RoomKind::Storeroom) : 0;
      if (bed < 0 || pan < 0 || !g0.finish() || g0.roomOf(P.ex, H - 2) != 0) return false;
      if (!g0.door(bed, 0, r.f() < 0.5f ? P.X(2 + r.irange(bw - 1)) : P.X(bw + 1), r.f() < 0.5f ? H - 2 - d : H - 3, r)) return false;
      return P.var == 4 || g0.door(pan, 0, P.X(W - 3), 6, r);
    }
    if (P.var == 1) {   // a bedroom in a back corner, the hall wraps around it
      int bw = 3 + r.irange(2);
      int bed = P.carve(g0, 1, 2, bw, bd, RoomKind::Bedroom);
      if (bed < 0 || !g0.finish()) return false;
      bool south = r.f() < 0.5f;
      return g0.door(bed, 0, south ? P.X(2 + r.irange(bw - 1)) : P.X(bw + 1), south ? bd + 2 : 3 + r.irange(bd - 2), r);
    }
    if (P.var == 2) {   // the bedroom along a side wall, the whole depth of the house
      int bw = 3 + (W >= 13 ? r.irange(2) : 0);
      int bed = P.carve(g0, 1, 2, bw, H - 2, RoomKind::Bedroom);
      if (bed < 0 || !g0.finish() || g0.roomOf(P.ex, H - 2) != 0) return false;
      return g0.door(bed, 0, P.X(bw + 1), 3 + r.irange(std::max(1, H - 6)), r);
    }
    // the back strip: bedroom in one corner, a pantry or storeroom in the other, the hall and its hearth between
    int bw = 3 + r.irange(2), sw = 3;
    int bed = P.carve(g0, 1, 2, bw, bd, RoomKind::Bedroom);
    int sto = P.carve(g0, W - 1 - sw, 2, W - 2, bd, r.f() < 0.5f ? RoomKind::Storeroom : RoomKind::Kitchen);
    if (bed < 0 || sto < 0 || !g0.finish()) return false;
    if ((W - 3 - sw) - (bw + 2) + 1 < 3) return false;
    return g0.door(bed, 0, P.X(bw / 2 + 1), bd + 2, r) && g0.door(sto, 0, P.X(W - 2 - sw / 2), bd + 2, r);
  }
  // two storeys
  g0.init(W, H, RoomKind::Hall);
  P.kd = 4;
  int kw = 4 + (W >= 13 ? r.irange(2) : 0);
  bool sto = W >= 13 && r.f() < 0.5f;
  int sw = 3;
  int kit = P.carve(g0, 1, 2, kw, P.kd, RoomKind::Kitchen);
  int st = sto ? P.carve(g0, W - 1 - sw, 2, W - 2, P.kd, RoomKind::Storeroom) : 0;
  if (kit < 0 || st < 0 || !g0.finish()) return false;
  int al0 = kw + 2, al1 = sto ? W - 3 - sw : W - 2;
  int mode = r.f() < 0.5f ? 0 : 1;   // stairs under the kitchen's partition, or at the back wall of the hall
  int sxU = -1;
  if (mode == 0) sxU = 1 + r.irange(kw);
  else {
    std::vector<int> c;
    for (int x = al0; x + 1 <= al1; x++) if (nookOk(W, x, 3)) c.push_back(x);
    if (c.empty()) return false;
    sxU = c[(size_t)r.irange((int)c.size())];
  }
  P.var = mode * 2 + (sto ? 1 : 0);
  Geo& g1 = P.geo[1];
  g1.init(W, H, RoomKind::Corridor);
  std::vector<int> back, front;
  if (!carveRows(P, g1, P.kd, mode == 1 ? sxU : -1, -1, 3, 6, r, back, front) || !g1.finish()) return false;
  if (!setStairs(P, 0, P.X(sxU), mode == 0 ? P.kd + 3 : 2)) return false;
  // the parents take the widest room, a wealthy home keeps a study, the rest are the children's
  int big = back[0];
  for (int ri : back) if (g1.rooms[(size_t)ri].r.w > g1.rooms[(size_t)big].r.w) big = ri;
  bool study = P.wealth >= 2 && back.size() >= 3;
  for (int ri : back) g1.rooms[(size_t)ri].kind = RoomKind::Bedroom;
  if (study) for (int ri : back) if (ri != big) { g1.rooms[(size_t)ri].kind = RoomKind::Study; break; }
  if (!doorsTo(g1, back, r)) return false;
  // ground doors: the kitchen into the hall; the storeroom too
  bool kSouth = mode == 1 ? r.f() < 0.6f : r.f() < 0.3f;
  if (!g0.door(kit, 0, kSouth ? P.X(2 + r.irange(std::max(1, kw - 2))) : P.X(kw + 1), kSouth ? P.kd + 2 : 3, r)) return false;
  if (sto && !g0.door(st, 0, P.X(W - 3), P.kd + 2, r)) return false;
  return true;
}

// ---- FARMHOUSE: hall and kitchen, a bedroom, the barn end
bool planFarm(Plan& P, Rng& r) {
  const int W = P.W, H = P.H;
  Geo& g0 = P.geo[0];
  g0.init(W, H, RoomKind::Hall);
  int bw = 4 + r.irange(2);
  int hallW = W - 3 - bw;   // hall columns 1..hallW (unmirrored)
  int barn = P.carve(g0, W - 1 - bw, 2, W - 2, H - 2, RoomKind::Barn);
  if (barn < 0) return false;
  bool frontBed = r.f() < 0.45f;
  int b2 = 3 + r.irange(2);
  if (!frontBed) b2 = std::min(b2, hallW - 4);
  if (b2 < 3) return false;
  int bd = 4 + (H >= 12 ? r.irange(2) : 0);
  int bed = frontBed ? P.carve(g0, 1, H - 2 - (bd - 2), b2, H - 2, RoomKind::Bedroom) : P.carve(g0, 1, 2, b2, bd, RoomKind::Bedroom);
  if (bed < 0 || !g0.finish() || g0.roomOf(P.ex, H - 2) != 0) return false;
  P.var = (frontBed ? 2 : 0) + (bw - 4);
  const IRect& BR = g0.rooms[(size_t)bed].r;
  if (!g0.door(bed, 0, r.f() < 0.5f ? P.X(b2 + 1) : BR.cx(), frontBed ? BR.y - 1 : BR.y + BR.h + 1, r)) return false;
  return g0.door(barn, 0, P.X(W - 2 - bw), H / 2 + r.irange(3) - 1, r);
}

// ---- SMITHY: the forge hall, the forge against the back wall under the exterior's forge stack (east); sometimes the
// smith's bedroom or a store in the west
bool planSmithy(Plan& P, Rng& r) {
  const int W = P.W, H = P.H;
  P.mir = false;
  Geo& g0 = P.geo[0];
  g0.init(W, H, RoomKind::Forge);
  // the forge's column under the stack: the corner, a step in, or (in a wide shop) two steps in
  { float f = r.f(); P.forgeX = f < 0.45f ? W - 3 : (f < 0.8f || W < 15 ? W - 4 : W - 5); }
  P.var = r.irange(9);
  if (P.var == 7 && W < 15) P.var = 1;
  if (P.var == 7) {   // the smith's household along the back west: a kitchen with its own hearth, the bedroom beside it
    int kw = 3 + (W >= 17 ? r.irange(2) : 0), bw = 3;
    if (kw + 1 + bw + 1 >= P.forgeX - 2) return false;
    int kit = P.carve(g0, 1, 2, kw, 4, RoomKind::Kitchen);
    int bed = P.carve(g0, kw + 2, 2, kw + 1 + bw, 4, RoomKind::Bedroom);
    if (kit < 0 || bed < 0 || !g0.finish()) return false;
    return g0.door(kit, 0, 2 + r.irange(kw - 1), 6, r) && g0.door(bed, 0, kw + 2 + r.irange(bw), 6, r);
  }
  if (P.var == 8) {   // the two front corners: the smith's bed by the street on the west, the iron store under the forge
    int bw = 3 + (W >= 15 ? r.irange(2) : 0), sw = 3, d = 3 + (H >= 12 ? r.irange(2) : 0);
    if (W - 2 - sw - 1 - (bw + 1) < 3) return false;
    int bed = P.carve(g0, 1, H - 1 - d, bw, H - 2, RoomKind::Bedroom);
    int sto = P.carve(g0, W - 1 - sw, H - 1 - d, W - 2, H - 2, RoomKind::Storeroom);
    if (bed < 0 || sto < 0 || !g0.finish() || g0.roomOf(P.ex, H - 2) != 0) return false;
    bool bn = r.f() < 0.5f, sn = r.f() < 0.5f;
    return g0.door(bed, 0, bn ? 2 + r.irange(bw - 1) : bw + 1, bn ? H - 2 - d : H - 3, r) &&
           g0.door(sto, 0, sn ? W - 1 - sw + 1 : W - 2 - sw, sn ? H - 2 - d : H - 3, r);
  }
  if (P.var == 4) {   // the coal and iron store in the front corner under the forge's side; the smith's bed at the back west
    int sw = 3 + r.irange(2), sd = 3;
    bool bedB = r.f() < 0.5f;
    int bw = 3 + r.irange(2);
    int sto = P.carve(g0, W - 1 - sw, H - 1 - sd, W - 2, H - 2, RoomKind::Storeroom);
    int bed = bedB ? P.carve(g0, 1, 2, bw, 4, RoomKind::Bedroom) : 0;
    if (sto < 0 || bed < 0 || !g0.finish()) return false;
    if (!g0.door(sto, 0, r.f() < 0.5f ? W - 2 - sw : W - 2 - r.irange(sw - 1), r.f() < 0.5f ? H - 3 : H - 2 - sd, r)) return false;
    return !bedB || g0.door(bed, 0, r.f() < 0.5f ? bw + 1 : 2 + r.irange(bw - 1), r.f() < 0.5f ? 3 : 6, r);
  }
  if (P.var == 5) {   // the smith sleeps in a narrow room down the whole west side
    int bw = 3 + (W >= 15 ? r.irange(2) : 0);
    int bed = P.carve(g0, 1, 2, bw, H - 2, RoomKind::Bedroom);
    if (bed < 0 || !g0.finish() || g0.roomOf(P.ex, H - 2) != 0) return false;
    return g0.door(bed, 0, bw + 1, 3 + r.irange(std::max(1, H - 6)), r);
  }
  if (P.var == 6) {   // a west wing the other way round: the store at the back, the bedroom by the street
    int bw = 3 + r.irange(2), sd = 3 + (H >= 12 ? r.irange(2) : 0);
    int sto = P.carve(g0, 1, 2, bw, 1 + sd, RoomKind::Storeroom);
    int bed = P.carve(g0, 1, sd + 4, bw, H - 2, RoomKind::Bedroom);
    if (sto < 0 || bed < 0 || !g0.finish()) return false;
    return g0.door(sto, 0, r.f() < 0.5f ? bw + 1 : 2, r.f() < 0.5f ? 3 : sd + 3, r) && g0.door(bed, 0, bw + 1, H - 3, r);
  }
  if (P.var == 3 && H < 12) P.var = 1;
  if (P.var == 3) {   // a west wing: the smith's bedroom at the back, the iron and coal store in front
    int bw = 3 + r.irange(2), bd = 4;
    int bed = P.carve(g0, 1, 2, bw, bd, RoomKind::Bedroom);
    int sto = P.carve(g0, 1, bd + 3, bw, H - 2, RoomKind::Storeroom);
    if (bed < 0 || sto < 0 || !g0.finish()) return false;
    return g0.door(bed, 0, bw + 1, 3, r) && g0.door(sto, 0, bw + 1, H - 3, r);
  }
  if (P.var == 1) {
    int bw = 3 + r.irange(2), bd = 4;
    int bed = P.carve(g0, 1, 2, bw, bd, RoomKind::Bedroom);
    if (bed < 0 || !g0.finish()) return false;
    bool south = r.f() < 0.5f;
    return g0.door(bed, 0, south ? 2 + r.irange(bw - 1) : bw + 1, south ? bd + 2 : 3, r);
  }
  if (P.var == 2) {
    int sw = 3 + (W >= 15 ? r.irange(2) : 0), sd = 3;
    int sto = P.carve(g0, 1, H - 1 - sd, sw, H - 2, RoomKind::Storeroom);
    if (sto < 0 || !g0.finish()) return false;
    return g0.door(sto, 0, r.f() < 0.5f ? sw + 1 : 2, r.f() < 0.5f ? H - 2 : H - 2 - sd, r);
  }
  return g0.finish();
}

// ---- SHOP: the stockroom behind the counter; a bedroom behind it in a 1-storey shop with a hearth; the shopkeeper
// lives upstairs in a 2-storey one (reached through the stockroom)
bool planShop(Plan& P, Rng& r) {
  const int W = P.W, H = P.H;
  const Bldg& b = *P.b;
  P.kd = 4;
  const int kd = P.kd;
  Geo& g0 = P.geo[0];
  g0.init(W, H, RoomKind::Shopfloor);
  bool bedroom = P.floors == 1 && b.hearth;
  int L = W - 2;
  int sw = bedroom ? 4 + r.irange(std::max(1, L - 1 - 3 - 4 + 1)) : L;
  if (bedroom && L - sw - 1 < 3) return false;
  int sto = P.carve(g0, 1, 2, sw, kd, RoomKind::Stockroom);
  int bed = bedroom ? P.carve(g0, sw + 2, 2, W - 2, kd, RoomKind::Bedroom) : 0;
  if (sto < 0 || bed < 0 || !g0.finish()) return false;
  P.var = bedroom ? 1 : 0;
  P.barY = kd + 3;
  P.barX0 = std::min(P.X(1), P.X(W - 3)); P.barX1 = std::max(P.X(1), P.X(W - 3));
  P.flapX = P.X(W - 2);
  int sxU = -1;
  if (P.floors >= 2) {
    std::vector<int> c;
    for (int x = 1; x + 1 <= sw; x++) if (nookOk(W, x, 3)) c.push_back(x);
    if (c.empty()) return false;
    sxU = c[(size_t)r.irange((int)c.size())];
    Geo& g1 = P.geo[1];
    g1.init(W, H, RoomKind::Hall);   // the family's living room upstairs
    std::vector<int> back, front;
    if (!carveRows(P, g1, kd, sxU, -1, 3, 6, r, back, front) || !g1.finish()) return false;
    if (!setStairs(P, 0, P.X(sxU), 2)) return false;
    int big = back[0];
    for (int ri : back) if (g1.rooms[(size_t)ri].r.w > g1.rooms[(size_t)big].r.w) big = ri;
    bool kitDone = false;
    for (int ri : back) {
      RoomKind k = RoomKind::Bedroom;
      if (ri == big) k = RoomKind::OwnerRoom;
      else if (!kitDone) { k = b.hearth ? RoomKind::Kitchen : RoomKind::Study; kitDone = true; }
      g1.rooms[(size_t)ri].kind = k;
    }
    if (!doorsTo(g1, back, r)) return false;
    P.var = 2;
  }
  if (!g0.door(sto, 0, P.X(2 + r.irange(std::max(1, sw - 2))), kd + 2, r)) return false;
  if (bedroom) {
    if (r.f() < 0.6f) { if (!g0.door(bed, sto, P.X(sw + 1), 3, r)) return false; }
    else if (!g0.door(bed, 0, P.X(sw + 3), kd + 2, r)) return false;
  }
  // the stockroom door opens behind the counter
  int dx = g0.rooms[(size_t)sto].doorX;
  return dx >= P.barX0 && dx <= P.barX1;
}

// ---- TEMPLE: the nave oriented to the altar, a vestry beside it, sometimes the priest's cell
bool planTemple(Plan& P, Rng& r) {
  const int W = P.W, H = P.H;
  Geo& g0 = P.geo[0];
  g0.init(W, H, RoomKind::Nave);
  int vw = 3 + (W >= 17 ? r.irange(2) : 0);
  int vd = 4 + (H >= 15 ? r.irange(2) : 0);
  int ves = P.carve(g0, 1, 2, vw, vd, RoomKind::Vestry);
  bool cell = W >= 17 || r.f() < 0.5f;
  int bw = 3;
  int bed = cell ? P.carve(g0, W - 1 - bw, 2, W - 2, vd, RoomKind::Bedroom) : 0;
  if (ves < 0 || bed < 0 || !g0.finish()) return false;
  for (int x = P.ex - 2; x <= P.ex + 2; x++) if (g0.roomOf(x, 2) != 0) return false;
  P.var = (cell ? 1 : 0) + (vw - 3) * 2;
  if (!g0.door(ves, 0, r.f() < 0.5f ? P.X(vw + 1) : P.X(2), r.f() < 0.5f ? 3 : vd + 2, r)) return false;
  if (cell && !g0.door(bed, 0, P.X(W - 2 - bw), vd + 2, r)) return false;
  return true;
}

// ---- KEEP: the throne hall (the throne on the axis at the far wall) with its wings, in one of four arrangements:
//   0 barracks and kitchen in the back corners (a storeroom by the gate in a deep keep);
//   1 a row of four rooms along the back: barracks and armoury, kitchen and pantry, the hall's end wall between;
//   2 a full-depth wing down one side (barracks over the armoury), the kitchen in the opposite back corner;
//   3 the guardroom by the gate, the kitchen in a back corner, the steward's records room or a store opposite.
// Upstairs, one of three: rooms along a gallery (both sides when it is deep enough); the lord's quarters as a
// full-depth wing with the household's rooms along the back; or two wings of two rooms either side of a long gallery.
// The stairs stand in the hall: under a wing's partition or at the back wall clear of the throne.
bool planKeep(Plan& P, Rng& r) {
  const int W = P.W, H = P.H, ex = P.ex;
  Geo& g0 = P.geo[0];
  g0.init(W, H, RoomKind::ThroneHall);
  const int scheme = r.irange(4);
  struct DoorReq { int ri, px, py; };
  std::vector<DoorReq> dq;
  auto south = [&](int ri) { const IRect& R = g0.rooms[(size_t)ri].r; return DoorReq{ri, R.x + 1 + r.irange(std::max(1, R.w - 2)), R.y + R.h + 1}; };
  auto east = [&](int ri) { const IRect& R = g0.rooms[(size_t)ri].r; bool e = R.x < ex; return DoorReq{ri, e ? R.x + R.w : R.x - 1, R.y + 1 + r.irange(std::max(1, R.h - 2))}; };
  auto north = [&](int ri) { const IRect& R = g0.rooms[(size_t)ri].r; return DoorReq{ri, R.x + 1 + r.irange(std::max(1, R.w - 2)), R.y - 2}; };
  int kw = 4 + r.irange(3), kd = 4 + r.irange(2);
  int kit = P.carve(g0, W - 1 - kw, 2, W - 2, kd, RoomKind::Kitchen);
  if (kit < 0) return false;
  switch (scheme) {
    case 0: {
      int bw = 5 + r.irange(2), bd = 5 + (H >= 16 ? r.irange(2) : 0);
      int bar = P.carve(g0, 1, 2, bw, bd, RoomKind::Barracks);
      bool store = H >= 16 && bd <= H - 11 && r.f() < 0.5f;
      int sto = store ? P.carve(g0, 1, H - 4, 4 + r.irange(2), H - 2, RoomKind::Storeroom) : 0;
      if (bar < 0 || sto < 0 || !g0.finish()) return false;
      dq.push_back(r.f() < 0.5f ? south(bar) : east(bar));
      if (store) dq.push_back(r.f() < 0.5f ? north(sto) : east(sto));
      break;
    }
    case 1: {
      int bw = 4 + r.irange(2), aw = 3, pw = 3;
      int bar = P.carve(g0, 1, 2, bw, kd, RoomKind::Barracks);
      int arm = P.carve(g0, bw + 2, 2, bw + 1 + aw, kd, RoomKind::Storeroom);
      int pan = P.carve(g0, W - 3 - kw - pw, 2, W - 3 - kw, kd, RoomKind::Storeroom);
      if (bar < 0 || arm < 0 || pan < 0 || !g0.finish()) return false;
      dq.push_back(south(bar)); dq.push_back(south(arm)); dq.push_back(south(pan));
      break;
    }
    case 2: {
      int ww = 5 + r.irange(2), mid = 5 + r.irange(std::max(1, H - 11));
      int bar = P.carve(g0, 1, 2, ww, mid, RoomKind::Barracks);
      int arm = P.carve(g0, 1, mid + 3, ww, H - 2, RoomKind::Storeroom);
      if (bar < 0 || arm < 0 || !g0.finish()) return false;
      dq.push_back(east(bar)); dq.push_back(east(arm));
      break;
    }
    default: {
      int gw = 5 + r.irange(2), gd = 3 + r.irange(2);
      int grd = P.carve(g0, 1, H - 1 - gd, gw, H - 2, RoomKind::Barracks);
      bool rec = r.f() < 0.6f;
      int other = rec ? P.carve(g0, 1, 2, 4 + r.irange(2), 4 + r.irange(2), RoomKind::Study)
                      : P.carve(g0, W - 1 - (4 + r.irange(2)), H - 4, W - 2, H - 2, RoomKind::Storeroom);
      if (grd < 0 || other < 0 || !g0.finish()) return false;
      dq.push_back(r.f() < 0.5f ? north(grd) : east(grd));
      dq.push_back(rec ? (r.f() < 0.5f ? south(other) : east(other)) : (r.f() < 0.5f ? north(other) : east(other)));
      break;
    }
  }
  dq.push_back(r.f() < 0.5f ? south(kit) : east(kit));
  for (int x = ex - 3; x <= ex + 3; x++) if (g0.roomOf(x, 2) != 0) return false;
  for (int y = 3; y <= H - 2; y++) if (g0.roomOf(ex, y) != 0) return false;   // the aisle from the gate to the throne
  int us = r.irange(3);
  P.var = scheme * 4 + us;
  if (P.floors >= 2) {
    Geo& g1 = P.geo[1];
    g1.init(W, H, RoomKind::Corridor);
    std::vector<int> ids;
    if (us == 0) {   // rooms along a gallery
      int ud = 4 + r.irange(2);
      bool dbl = ud + 7 <= H - 4 && r.f() < 0.75f;
      std::vector<int> back, front;
      if (!carveRows(P, g1, ud, -1, dbl ? ud + 7 : -1, 5, 9, r, back, front)) return false;
      ids = back; ids.insert(ids.end(), front.begin(), front.end());
    } else if (us == 1) {   // the lord's wing and the household along the back
      int lw = 6 + r.irange(3), ud = 4 + r.irange(2);
      int lord = P.carve(g1, 1, 2, lw, H - 2, RoomKind::OwnerRoom);
      if (lord < 0) return false;
      ids.push_back(lord);
      std::vector<std::pair<int, int>> segs;
      if (!splitSeg(lw + 2, W - 2, 4, 7, r, segs)) return false;
      for (auto& sg : segs) { int ri = P.carve(g1, sg.first, 2, sg.second, ud, RoomKind::Bedroom); if (ri < 0) return false; ids.push_back(ri); }
    } else {   // two wings of two rooms
      int sw = 5 + r.irange(2), m1 = 5 + r.irange(std::max(1, H - 11)), m2 = 5 + r.irange(std::max(1, H - 11));
      int a = P.carve(g1, 1, 2, sw, m1, RoomKind::Bedroom), b = P.carve(g1, 1, m1 + 3, sw, H - 2, RoomKind::Bedroom);
      int c = P.carve(g1, W - 1 - sw, 2, W - 2, m2, RoomKind::Bedroom), d = P.carve(g1, W - 1 - sw, m2 + 3, W - 2, H - 2, RoomKind::Bedroom);
      if (a < 0 || b < 0 || c < 0 || d < 0) return false;
      ids = {a, b, c, d};
      if (W - 2 * sw - 4 >= 9 && r.f() < 0.6f) {   // a study at the back of the gallery, between the wings
        int s0 = sw + 3, s1 = W - 4 - sw;
        int st = P.carve(g1, s0 + 1, 2, s1 - 1, 4, RoomKind::Study);
        if (st >= 0) ids.push_back(st);
      }
    }
    if (!g1.finish()) return false;
    // who lives where: the lord takes the largest room, then a study, a store for the linen, the rest are bedrooms
    int big = -1;
    for (int ri : ids) {
      const IRect& R = g1.rooms[(size_t)ri].r;
      if (g1.rooms[(size_t)ri].kind == RoomKind::OwnerRoom) { big = ri; break; }
      if (big < 0 || R.w * R.h > g1.rooms[(size_t)big].r.w * g1.rooms[(size_t)big].r.h) big = ri;
    }
    g1.rooms[(size_t)big].kind = RoomKind::OwnerRoom;
    bool study = false, store = false;
    for (int ri : ids) if (g1.rooms[(size_t)ri].kind == RoomKind::Study) study = true;
    for (int ri : ids) {
      RoomDef& R = g1.rooms[(size_t)ri];
      if (ri == big || R.kind == RoomKind::Study) continue;
      if (!study) { R.kind = RoomKind::Study; study = true; continue; }
      if (!store && ids.size() >= 5 && r.f() < 0.5f) { R.kind = RoomKind::Storeroom; store = true; continue; }
      R.kind = RoomKind::Bedroom;
    }
    if (!study) return false;
    // the stairs: anywhere along a wall face in the hall that lines up with the gallery upstairs
    std::vector<std::pair<int, int>> sc;
    for (int y = 2; y < H - 2; y++)
      for (int x = 1; x < W - 1; x++) {
        if (g0.roomOf(x, y) != 0 || !g0.isWall(x, y - 1) || std::abs(x - ex) <= 3) continue;
        sc.push_back({x, y});
      }
    for (size_t i = sc.size(); i > 1; i--) std::swap(sc[i - 1], sc[(size_t)r.irange((int)i)]);
    bool ok = false;
    for (auto& c : sc) if ((ok = setStairs(P, 0, c.first, c.second))) break;
    if (!ok) return false;
    for (int ri : ids) {
      const IRect& R = g1.rooms[(size_t)ri].r;
      if (!g1.door(ri, 0, R.x + 1 + r.irange(std::max(1, R.w - 2)), R.y + R.h / 2, r)) return false;
    }
  }
  for (const DoorReq& d : dq)
    if (!g0.door(d.ri, 0, d.px, d.py, r)) return false;
  return true;
}

// ---- PALACE (M1, VISION_PLAN 15.8): the king's palace. Below, the throne hall runs the whole depth down the middle (the
// throne on the axis at the far wall, the aisle to it from the doors), with the wings either side: the kitchen and the
// steward's records or a store at the back, the guardroom and the armoury by the front. The hall reaches the side walls
// between them, so the wings open off a cross hall. A broad flight climbs from the hall's back bay, clear of the dais,
// to the royal apartments upstairs: rooms along the gallery (both sides when it is deep enough): the king's
// bedchamber, the council chamber, a library and the household's bedchambers.
bool planPalace(Plan& P, Rng& r) {
  const int W = P.W, H = P.H, ex = P.ex;
  Geo& g0 = P.geo[0];
  g0.init(W, H, RoomKind::ThroneHall);
  const int ww = 6 + r.irange(3), kd = 5 + r.irange(2), gd = 4 + r.irange(2);
  if (ww + 2 > ex - 6 || H - 3 - gd <= kd + 3) return false;
  int kit = P.carve(g0, 1, 2, ww, kd, RoomKind::Kitchen);
  int grd = P.carve(g0, 1, H - 1 - gd, ww, H - 2, RoomKind::Barracks);
  int rec = P.carve(g0, W - 1 - ww, 2, W - 2, kd, r.f() < 0.5f ? RoomKind::Study : RoomKind::Storeroom);
  int arm = P.carve(g0, W - 1 - ww, H - 1 - gd, W - 2, H - 2, RoomKind::Storeroom);
  if (kit < 0 || grd < 0 || rec < 0 || arm < 0 || !g0.finish()) return false;
  for (int x = ex - 3; x <= ex + 3; x++) if (g0.roomOf(x, 2) != 0) return false;
  for (int y = 3; y <= H - 2; y++) if (g0.roomOf(ex, y) != 0) return false;   // the aisle from the doors to the throne
  // the stairs (unmirrored column): the hall's back bay between a wing and the dais
  std::vector<int> cand;
  for (int x = ww + 2; x + 1 <= ex - 5; x++) if (nookOk(W, x, 5)) cand.push_back(x);
  for (int x = ex + 4; x + 1 <= W - ww - 3; x++) if (nookOk(W, x, 5)) cand.push_back(x);
  if (cand.empty()) return false;
  const int sxU = cand[(size_t)r.irange((int)cand.size())];
  // upstairs: the royal apartments
  Geo& g1 = P.geo[1];
  g1.init(W, H, RoomKind::Corridor);
  const int ud = 5 + r.irange(2);
  const bool dbl = ud + 7 <= H - 5;
  std::vector<int> back, front;
  if (!carveRows(P, g1, ud, sxU, dbl ? ud + 7 : -1, 5, 9, r, back, front)) return false;
  if (!g1.finish()) return false;
  std::vector<int> ids = back;
  ids.insert(ids.end(), front.begin(), front.end());
  if (ids.size() < 4) return false;
  // who has which room: the king the largest, the council the next, a library, the household's bedchambers
  std::vector<int> bySize = ids;
  std::stable_sort(bySize.begin(), bySize.end(), [&](int a, int b) {
    const IRect &ra = g1.rooms[(size_t)a].r, &rb = g1.rooms[(size_t)b].r;
    return ra.w * ra.h > rb.w * rb.h;
  });
  // (M1 round 3) and the rest are not all bedrooms (a grid of near-identical bedchambers, mirrored pairs, read as a
  // dormitory): the household's bedchambers among a royal chapel, a guest chamber for envoys, a wardrobe store and a
  // second study, dealt round in turn
  static const RoomKind rest[6] = {RoomKind::Bedroom, RoomKind::Vestry, RoomKind::GuestRoom, RoomKind::Bedroom, RoomKind::Storeroom, RoomKind::Study};
  {
    int k = 0;
    for (size_t i = 2; i + 1 < bySize.size(); i++) g1.rooms[(size_t)bySize[i]].kind = rest[k++ % 6];
  }
  g1.rooms[(size_t)bySize[0]].kind = RoomKind::OwnerRoom;
  g1.rooms[(size_t)bySize[1]].kind = RoomKind::Council;
  g1.rooms[(size_t)bySize[bySize.size() - 1]].kind = RoomKind::Study;
  if (!setStairs(P, 0, P.X(sxU), 2)) return false;
  if (!doorsTo(g1, back, r) || !doorsTo(g1, front, r)) return false;
  // the ground floor's doors into the hall
  if (!g0.door(kit, 0, P.X(2 + r.irange(std::max(1, ww - 2))), kd + 2, r)) return false;
  if (!g0.door(grd, 0, P.X(2 + r.irange(std::max(1, ww - 2))), H - 2 - gd, r)) return false;
  if (!g0.door(rec, 0, P.X(W - 3 - r.irange(std::max(1, ww - 2))), kd + 2, r)) return false;
  if (!g0.door(arm, 0, P.X(W - 3 - r.irange(std::max(1, ww - 2))), H - 2 - gd, r)) return false;
  P.var = ww * 16 + kd * 4 + gd + (dbl ? 1000 : 0);
  return true;
}

// ---- BARRACKS (M1): the guard's hall below (the mess table, the hearth, weapon racks) with the armoury and a kitchen at
// the back; upstairs the dormitories of bunks and the captain's room along a corridor.
bool planBarracks(Plan& P, Rng& r) {
  const int W = P.W, H = P.H;
  Geo& g0 = P.geo[0];
  g0.init(W, H, RoomKind::Hall);
  const int aw = 4 + r.irange(2), ad = 4 + r.irange(2);
  const bool kitchen = W >= 19 && r.f() < 0.65f;
  const int kw = 4 + r.irange(2);
  int arm = P.carve(g0, 1, 2, aw, ad, RoomKind::Storeroom);
  int kit = kitchen ? P.carve(g0, W - 1 - kw, 2, W - 2, ad, RoomKind::Kitchen) : 0;
  if (arm < 0 || kit < 0 || !g0.finish()) return false;
  std::vector<int> cand;
  for (int x = aw + 2; x + 1 <= (kitchen ? W - 3 - kw : W - 2); x++) if (nookOk(W, x, 4)) cand.push_back(x);
  if (cand.empty()) return false;
  const int sxU = cand[(size_t)r.irange((int)cand.size())];
  Geo& g1 = P.geo[1];
  g1.init(W, H, RoomKind::Corridor);
  const int ud = 4 + r.irange(2);
  const bool dbl = ud + 7 <= H - 4;
  std::vector<int> back, front;
  if (!carveRows(P, g1, ud, sxU, dbl ? ud + 7 : -1, 4, 7, r, back, front)) return false;
  if (!g1.finish()) return false;
  std::vector<int> ids = back;
  ids.insert(ids.end(), front.begin(), front.end());
  if (ids.size() < 2) return false;
  int cap = ids[0];
  for (int ri : ids) {
    g1.rooms[(size_t)ri].kind = RoomKind::Barracks;
    const IRect &a = g1.rooms[(size_t)ri].r, &b = g1.rooms[(size_t)cap].r;
    if (a.w * a.h < b.w * b.h) cap = ri;   // the captain keeps the smallest room to himself
  }
  g1.rooms[(size_t)cap].kind = RoomKind::OwnerRoom;
  if (!setStairs(P, 0, P.X(sxU), 2)) return false;
  if (!doorsTo(g1, back, r) || !doorsTo(g1, front, r)) return false;
  if (!g0.door(arm, 0, P.X(2 + r.irange(std::max(1, aw - 2))), ad + 2, r)) return false;
  if (kitchen && !g0.door(kit, 0, P.X(W - 3 - r.irange(std::max(1, kw - 2))), ad + 2, r)) return false;
  P.var = aw * 8 + ad + (kitchen ? 100 : 0) + (dbl ? 1000 : 0);
  return true;
}

// ---- TOWER: one round room per floor (laboratory, library, the mage's bedroom), stairs along the back wall
bool planTower(Plan& P, Rng& r) {
  bool closet = r.f() < 0.45f;
  // fix round 3: the mage tower is round outside, so its floors are round inside: the front corners are walled in on
  // a curve (three tiles each, five on a wide tower), the back wall stays straight for the stairs and the shelves
  const int W = P.W, H = P.H;
  auto roundOff = [&](Geo& g, bool left, bool right) {
    std::vector<std::pair<int, int>> cut = {{1, H - 2}, {2, H - 2}, {1, H - 3}};
    if (W >= 13 && H >= 13) { cut.push_back({3, H - 2}); cut.push_back({1, H - 4}); }
    for (auto& c : cut) {
      if (left) { g.wall[(size_t)g.I(c.first, c.second)] = 1; g.room[(size_t)g.I(c.first, c.second)] = -1; }
      if (right) { g.wall[(size_t)g.I(W - 1 - c.first, c.second)] = 1; g.room[(size_t)g.I(W - 1 - c.first, c.second)] = -1; }
    }
  };
  for (int f = 0; f < P.floors; f++) {
    Geo& g = P.geo[(size_t)f];
    g.init(P.W, P.H, mainKindOf(P.type, f, P.floors));
    if (f == 0 && closet && P.carve(g, 1, P.H - 4, 3, P.H - 2, RoomKind::Storeroom) < 0) return false;
    // the closet fills the left front corner on the ground floor: only its outermost tile is rounded off there
    bool closetLeft = f == 0 && closet && !P.mir, closetRight = f == 0 && closet && P.mir;
    roundOff(g, !closetLeft, !closetRight);
    if (closetLeft) { g.wall[(size_t)g.I(1, H - 2)] = 1; g.room[(size_t)g.I(1, H - 2)] = -1; }
    if (closetRight) { g.wall[(size_t)g.I(W - 2, H - 2)] = 1; g.room[(size_t)g.I(W - 2, H - 2)] = -1; }
    if (!g.finish()) return false;
  }
  for (int f = 0; f + 1 < P.floors; f++) {
    int x = (f % 2 == 0) ? P.X(1 + r.irange(2)) : P.X(P.W - 2 - r.irange(2));
    if (!setStairs(P, f, x, 2, false)) return false;
  }
  if (closet && !P.geo[0].door(1, 0, P.X(2), P.H - 5, r)) return false;
  P.var = (P.mir ? 1 : 0) + (closet ? 2 : 0);
  return true;
}

// ---- HUT: one room for living and sleeping, or a sleeping alcove off it, a pantry, a byre for the beasts
bool planHut(Plan& P, Rng& r) {
  const int W = P.W, H = P.H;
  Geo& g0 = P.geo[0];
  const Bldg& b = *P.b;
  int v = r.irange(5);
  if ((v == 2 || v == 4) && P.ex < 5) v = r.f() < 0.5f ? 1 : 3;
  P.var = v;
  if (v == 0) { g0.init(W, H, RoomKind::Cottage); return g0.finish(); }
  if (v == 1) {   // a sleeping alcove in a back corner, the rest is the living room
    g0.init(W, H, RoomKind::Hall);
    int bw = 3 + (W >= 11 ? r.irange(2) : 0);
    int bed = P.carve(g0, 1, 2, bw, 4, RoomKind::Bedroom);
    if (bed < 0 || !g0.finish()) return false;
    bool south = r.f() < 0.5f;
    return g0.door(bed, 0, south ? P.X(2 + r.irange(bw - 1)) : P.X(bw + 1), south ? 6 : 3, r);
  }
  if (v == 2) {   // the bed behind a wall down one side
    g0.init(W, H, RoomKind::Hall);
    int bed = P.carve(g0, 1, 2, 3, H - 2, RoomKind::Bedroom);
    if (bed < 0 || !g0.finish() || g0.roomOf(P.ex, H - 2) != 0) return false;
    return g0.door(bed, 0, P.X(4), 3 + r.irange(std::max(1, H - 6)), r);
  }
  if (v == 3) {   // one room, and a pantry in a back corner
    g0.init(W, H, RoomKind::Cottage);
    int pan = P.carve(g0, W - 4, 2, W - 2, 4, RoomKind::Storeroom);
    if (pan < 0 || !g0.finish()) return false;
    return g0.door(pan, 0, r.f() < 0.5f ? P.X(W - 3) : P.X(W - 5), r.f() < 0.5f ? 6 : 3, r);
  }
  // one room for the family and a byre down one side for the goat and the hens
  g0.init(W, H, RoomKind::Cottage);
  int byre = P.carve(g0, W - 4, 2, W - 2, H - 2, b.owner == Role::Farmer || r.f() < 0.6f ? RoomKind::Barn : RoomKind::Storeroom);
  if (byre < 0 || !g0.finish() || g0.roomOf(P.ex, H - 2) != 0) return false;
  return g0.door(byre, 0, P.X(W - 5), 3 + r.irange(std::max(1, H - 6)), r);
}

// ---- (M1 economy) WORK HALL: the mill's, the sawmill's or the tannery's hall (the granary's store), a store or the
// keeper's room walled off in a back corner, its door onto the hall
bool planWorkshop(Plan& P, Rng& r) {
  Geo& g0 = P.geo[0];
  g0.init(P.W, P.H, mainKindOf(P.type, 0, 1));
  if (P.type == Building::Granary && r.f() < 0.6f) { P.var = 0; return g0.finish(); }   // one great store of grain
  const int bw = 3 + r.irange(2), bd = 4 + (P.H >= 12 ? r.irange(2) : 0);
  const RoomKind k = P.type == Building::Tanner && r.f() < 0.5f ? RoomKind::Bedroom : (P.type == Building::Granary ? RoomKind::OwnerRoom : RoomKind::Storeroom);
  const int ri = P.carve(g0, 1, 2, bw, bd, k);
  if (ri < 0 || !g0.finish() || g0.roomOf(P.ex, P.H - 2) != 0) return false;
  const bool south = r.f() < 0.5f;
  P.var = 1 + (south ? 1 : 0) + (bw - 3) * 2 + (bd - 4) * 4;
  return g0.door(ri, 0, south ? P.X(2 + r.irange(bw - 1)) : P.X(bw + 1), south ? bd + 2 : 3, r);
}

// the fallback: one room per floor, stairs at the back wall
void planPlain(Plan& P) {
  for (int f = 0; f < P.floors; f++) {
    P.geo[(size_t)f].init(P.W, P.H, mainKindOf(P.type, f, P.floors));
    if (f > 0 && P.type != Building::Tower) P.geo[(size_t)f].rooms[0].kind = RoomKind::Bedroom;
    P.geo[(size_t)f].finish();
  }
  for (int f = 0; f + 1 < P.floors; f++) {
    bool ok = false;
    for (int k = 0; k < P.W && !ok; k++) {
      int x = (f % 2 == 0) ? 1 + k : P.W - 2 - k;
      ok = setStairs(P, f, x, 2, false, true);
    }
  }
  P.var = 99;
}

void planSize(Plan& P) {
  const Bldg& b = *P.b;
  int w = b.r.w, h = b.r.h;
  switch (b.type) {
    case Building::Inn: P.W = std::max(17, 2 * w + 7); P.H = std::max(13, 2 * h + 9); break;
    case Building::Keep: P.W = std::max(23, 2 * w + 9); P.H = std::max(14, 2 * h + 8); break;
    case Building::Palace: P.W = std::max(33, 2 * w + 9); P.H = std::max(20, 2 * h + 9); break;   // M1: the king's palace
    case Building::Barracks: P.W = std::max(17, 2 * w + 5); P.H = std::max(14, 2 * h + 7); break;
    case Building::Temple: P.W = std::max(15, 2 * w + 5); P.H = std::max(13, 2 * h + 7); break;
    case Building::Shop: case Building::Bakery: case Building::Butcher: case Building::Fishmonger: case Building::Weaver:
      P.W = std::max(13, 2 * w + 5); P.H = std::max(14, 2 * h + 8); break;
    case Building::Smithy: case Building::Smelter: {   // fix round 2: a smithy's floor follows its footprint, give or take a stride, so no two
                               // smithies share one plan
      uint32_t hs = hash32(b.seed ^ 0x5A17F0u);
      P.W = std::max(13, 2 * w + 5) + (int)(hs % 3) * 2;
      P.H = std::max(10, 2 * h + 6) + (int)((hs >> 4) % 3);
      break;
    }
    case Building::Farmhouse: P.W = std::max(15, 2 * w + 5); P.H = std::max(12, 2 * h + 6); break;
    case Building::Tower: case Building::Windmill: P.W = std::max(11, 2 * w + 5); P.H = std::max(11, 2 * h + 5); break;
    case Building::Hut: {   // a hut's room follows its footprint, give or take a stride (V7: not every hut alike)
      uint32_t h = hash32(b.seed ^ 0x51ED2Bu);
      P.W = std::max(9, 2 * w + 5 + (int)(h % 3) * 2 - 2);
      P.H = 9 + (int)((h >> 4) % 2);
      break;
    }
    default: P.W = std::max(11, 2 * w + 5); P.H = std::max(P.floors >= 2 ? 11 : 10, 2 * h + (P.floors >= 2 ? 7 : 6)); break;
  }
  P.W = std::min(oddUp(P.W), b.type == Building::Palace ? 45 : 31);
  P.H = std::min(P.H, b.type == Building::Palace ? 26 : 24);
  P.ex = P.W / 2;
}

Plan makePlan(const Bldg& b, uint32_t seed) {
  Plan P;
  P.b = &b;
  P.type = b.type;
  P.floors = std::max(1, std::min(3, b.floors()));
  planSize(P);
  Rng wr(seed ^ 0x2F6A9D1Bu);
  P.wealth = b.type == Building::Hut ? 0 : (b.type == Building::StoneHouse ? 2 : 1);
  if (b.type == Building::Keep || b.type == Building::Temple || b.type == Building::Palace) P.wealth = 3;
  if (b.type == Building::Barracks) P.wealth = 1;
  if (P.floors >= 2 && P.wealth == 1 && wr.f() < 0.3f) P.wealth = 2;
  if (P.wealth > 0 && P.wealth < 3 && wr.f() < 0.2f) P.wealth--;
  Rng r(seed ^ 0xB4A1E5C3u);
  for (int attempt = 0; attempt < 40; attempt++) {
    Plan T = P;
    T.geo.assign((size_t)T.floors, Geo());
    T.mir = r.f() < 0.5f;
    bool ok = false;
    switch (b.type) {
      case Building::Inn: ok = planInn(T, r); break;
      case Building::House: case Building::StoneHouse: ok = planHouse(T, r); break;
      case Building::Farmhouse: ok = T.floors == 1 && planFarm(T, r); break;
      case Building::Smithy: ok = T.floors == 1 && planSmithy(T, r); break;
      case Building::Shop: ok = planShop(T, r); break;
      case Building::Temple: ok = T.floors == 1 && planTemple(T, r); break;
      case Building::Keep: ok = planKeep(T, r); break;
      case Building::Palace: ok = planPalace(T, r); break;
      case Building::Barracks: ok = planBarracks(T, r); break;
      case Building::Tower: ok = planTower(T, r); break;
      case Building::Hut: ok = T.floors == 1 && planHut(T, r); break;
      // (M1 economy) the production buildings on the plans of their kind (the trades behind a shop counter: the bakery's
      // back room is its bakehouse, the weaver's the loom room)
      case Building::Bakery: case Building::Butcher: case Building::Fishmonger: case Building::Weaver:
        ok = planShop(T, r);
        if (ok && (b.type == Building::Bakery || b.type == Building::Weaver))
          for (RoomDef& R : T.geo[0].rooms)
            if (R.kind == RoomKind::Stockroom) R.kind = b.type == Building::Bakery ? RoomKind::Kitchen : RoomKind::Workshop;
        break;
      case Building::Smelter: ok = T.floors == 1 && planSmithy(T, r); break;
      case Building::Windmill: ok = planTower(T, r); break;
      case Building::Watermill: case Building::Sawmill: case Building::Tanner: case Building::Granary: ok = T.floors == 1 && planWorkshop(T, r); break;
      default:
        T.geo[0].init(T.W, T.H, mainKindOf(b.type, 0, 1));
        ok = T.floors == 1 && T.geo[0].finish();
        break;
    }
    if (ok && T.geo[0].roomOf(T.ex, T.H - 2) != 0) ok = false;   // the front door opens into the main room
    if (ok) {
      for (int f = 0; f < T.floors; f++) {
        const Geo& g = T.geo[(size_t)f];
        if (f + 1 < T.floors && !g.up.valid()) ok = false;
        if (f > 0 && !g.down.valid()) ok = false;
        for (size_t i = 1; i < g.rooms.size(); i++) if (g.rooms[i].doorX < 0) ok = false;
      }
    }
    if (ok) {
      for (Geo& g : T.geo) g.bbox();   // the rooms' boxes now include their doorways
      return T;
    }
  }
  Plan T = P;
  T.geo.assign((size_t)T.floors, Geo());
  planPlain(T);
  return T;
}

// ===================================================================================================== furnishing
bool tallProp(Prop p) {
  switch (p) {
    case Prop::Bed: case Prop::Cupboard: case Prop::Wardrobe: case Prop::Bookshelf: case Prop::Shelf: case Prop::Loom:
    case Prop::Hearth: case Prop::Forge: case Prop::Filler: case Prop::Throne: case Prop::Altar: case Prop::Statue:
    case Prop::Banner: case Prop::Barrel2: case Prop::Fireplace: case Prop::BottleShelf: case Prop::WeaponRack:
    case Prop::Dresser: case Prop::Oven: case Prop::BunkBed: case Prop::Pillar: case Prop::Candelabra: case Prop::StairsUp:
      return true;
    default: return false;
  }
}
bool usableProp(Prop p) { return p == Prop::Bed || p == Prop::BunkBed || p == Prop::Chest || p == Prop::Altar; }

struct Weighted { int what; int w; };
int pickW(Rng& r, const std::vector<Weighted>& v) {
  int tot = 0;
  for (auto& e : v) tot += std::max(0, e.w);
  if (tot <= 0) return v.empty() ? 0 : v[0].what;
  int k = r.irange(tot);
  for (auto& e : v) { if (k < std::max(0, e.w)) return e.what; k -= std::max(0, e.w); }
  return v.back().what;
}

struct Fit {
  Map& m;
  Rng& r;
  const Plan& P;
  const Geo& g;
  int W, H;
  std::vector<int16_t> grp;
  std::vector<uint8_t> lane;
  struct Rec { int i; uint8_t prop; int16_t grp; };
  std::vector<Rec> log;
  int groups = 0, floorTiles = 0, sx = 0, sy = 0;
  std::vector<int> dist, q, taken;
  Biome biome = Biome::Plains;
  // fix round 3: how this house seats its tables (hashed from the building, no rng draw): 0 chairs on the far side and
  // stools in front, 1 stools all round (plain homes, taverns), 2 chairs on the far side only, the ends left open
  int seatStyle = 0;

  Fit(Map& m_, Rng& r_, const Plan& P_, const Geo& g_) : m(m_), r(r_), P(P_), g(g_), W(m_.w), H(m_.h) {
    grp.assign((size_t)W * H, -1);
    lane.assign((size_t)W * H, 0);
    for (int i = 0; i < W * H; i++) if (!groundSolid((Ground)m.ground[(size_t)i])) floorTiles++;
    uint32_t h = P.b ? (P.b->seed * 2654435761u) ^ 0x5bd1e995u : 0u;
    h ^= h >> 15;
    seatStyle = (int)(h % 3u);
    if (P.wealth == 0 && seatStyle == 2) seatStyle = 1;   // poor homes have stools, not chairs
  }
  // how many of prop p this floor holds already
  int countProp(Prop p) const {
    int n = 0;
    for (size_t i = 0; i < m.prop.size(); i++) n += m.prop[i] == (int)p + 1;
    return n;
  }
  int I(int x, int y) const { return y * W + x; }
  bool floorT(int x, int y) const { return m.in(x, y) && !groundSolid(m.at(x, y)); }
  bool wallT(int x, int y) const { return !m.in(x, y) || m.at(x, y) == Ground::InteriorWall; }
  bool freeT(int x, int y) const { return floorT(x, y) && !m.propAt(x, y) && !lane[(size_t)I(x, y)]; }
  int roomOf(int x, int y) const { return g.roomOf(x, y); }
  bool inRoom(int x, int y, int ri) const { return floorT(x, y) && roomOf(x, y) == ri; }
  bool wallN(int x, int y) const { return wallT(x, y - 1); }
  bool wallSide(int x, int y) const { return wallT(x - 1, y) || wallT(x + 1, y); }
  bool wallS(int x, int y) const { return wallT(x, y + 1); }
  void setLane(int x, int y) { if (m.in(x, y)) lane[(size_t)I(x, y)] = 1; }
  bool gapOk(int x0, int y0, int x1, int y1, int gid) const {
    for (int y = y0 - 1; y <= y1 + 1; y++)
      for (int x = x0 - 1; x <= x1 + 1; x++) {
        if (!floorT(x, y)) continue;
        int o = grp[(size_t)I(x, y)];
        if (o >= 0 && o != gid) return false;
      }
    return true;
  }
  bool rectFree(int x0, int y0, int x1, int y1, int ri) const {
    for (int y = y0; y <= y1; y++)
      for (int x = x0; x <= x1; x++) if (!freeT(x, y) || roomOf(x, y) != ri) return false;
    return true;
  }
  void put(int x, int y, Prop p, int gid) {
    size_t i = (size_t)I(x, y);
    log.push_back({(int)i, m.prop[i], grp[i]});
    m.setProp(x, y, p);
    grp[i] = (int16_t)gid;
  }
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
    q.clear();
    dist[(size_t)I(sx, sy)] = 0;
    q.push_back(I(sx, sy));
    static const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
    for (size_t h = 0; h < q.size(); h++) {
      int c = q[h], x = c % W, y = c / W;
      for (int k = 0; k < 4; k++) {
        int nx = x + dx[k], ny = y + dy[k];
        if (!m.in(nx, ny) || solidT(nx, ny) || dist[(size_t)I(nx, ny)] >= 0) continue;
        dist[(size_t)I(nx, ny)] = dist[(size_t)c] + 1;
        q.push_back(I(nx, ny));
      }
    }
  }
  bool reached(int x, int y) const { return m.in(x, y) && dist[(size_t)I(x, y)] >= 0; }
  bool besideReached(int x, int y) const { return reached(x + 1, y) || reached(x - 1, y) || reached(x, y + 1) || reached(x, y - 1); }
  bool valid() {
    bfs();
    int free = 0;
    for (int y = 0; y < H; y++)
      for (int x = 0; x < W; x++) {
        if (!floorT(x, y)) continue;
        size_t i = (size_t)I(x, y);
        if (lane[i] && dist[i] < 0) return false;
        if (dist[i] >= 0) free++;
        int p = m.prop[i];
        if (p && usableProp((Prop)(p - 1)) && !besideReached(x, y)) return false;
      }
    return free * 10 >= floorTiles * 6;
  }
  template <class F>
  bool group(F&& place) {
    size_t mk = log.size();
    int gid = groups++;
    if (!place(gid) || !valid()) { rollback(mk); return false; }
    return true;
  }
  void deco(int x, int y, Deco d) { if (m.in(x, y)) m.deco[(size_t)I(x, y)] = (uint8_t)d; }
  template <class F>
  void tiles(int ri, F&& f) const {   // visits every floor tile of room ri (bounding box order)
    const IRect& R = g.rooms[(size_t)ri].r;
    for (int y = R.y; y < R.y + R.h; y++)
      for (int x = R.x; x < R.x + R.w; x++)
        if (inRoom(x, y, ri)) f(x, y);
  }
  template <class T>
  void shuffle(std::vector<T>& v) { for (size_t i = v.size(); i > 1; i--) std::swap(v[i - 1], v[(size_t)r.irange((int)i)]); }
};

// ---- building blocks: each tries the room's free spots in a shuffled order and keeps the first group that validates
// against the north wall (a horizontal face): beds, shelves, hearths. where: 0 any, 1 corners first, 2 the middle first
bool northPiece(Fit& F, int ri, Prop p, int where, int* outX = nullptr, int* outY = nullptr, bool outerOnly = false) {
  std::vector<std::pair<int, int>> c;
  F.tiles(ri, [&](int x, int y) {
    if (!F.wallN(x, y) || !F.freeT(x, y) || (outerOnly && y != 2)) return;
    if (tallProp(p) && p != Prop::Bed && (!F.wallT(x - 1, y - 1) || !F.wallT(x + 1, y - 1))) return;   // a cupboard does not hide a door frame
    c.push_back({x, y});
  });
  F.shuffle(c);
  const IRect& R = F.g.rooms[(size_t)ri].r;
  if (where == 1) std::stable_sort(c.begin(), c.end(), [&](const std::pair<int, int>& a, const std::pair<int, int>& b) { return F.wallSide(a.first, a.second) > F.wallSide(b.first, b.second); });
  if (where == 2) std::stable_sort(c.begin(), c.end(), [&](const std::pair<int, int>& a, const std::pair<int, int>& b) { return std::abs(a.first - R.cx()) < std::abs(b.first - R.cx()); });
  for (auto& t : c)
    if (F.group([&](int gid) { F.put(t.first, t.second, p, gid); return true; })) {
      if (outX) *outX = t.first;
      if (outY) *outY = t.second;
      return true;
    }
  return false;
}
// a bed, head to the wall, two tiles long: the head tile against the north wall holds Filler, the bed (the prop the
// game and the tests use: RoomInfo::bedX/bedY) stands on the tile in front of it. where 1: corners first
bool bedPiece(Fit& F, int ri, int where, int* outX, int* outY) {
  std::vector<std::pair<int, int>> c;
  F.tiles(ri, [&](int x, int y) {
    if (!F.wallN(x, y) || !F.freeT(x, y) || !F.freeT(x, y + 1) || F.roomOf(x, y + 1) != ri) return;
    c.push_back({x, y});
  });
  F.shuffle(c);
  if (where == 1) std::stable_sort(c.begin(), c.end(), [&](const std::pair<int, int>& a, const std::pair<int, int>& b) { return F.wallSide(a.first, a.second) > F.wallSide(b.first, b.second); });
  for (auto& t : c)
    if (F.group([&](int gid) { F.put(t.first, t.second, Prop::Filler, gid); F.put(t.first, t.second + 1, Prop::Bed, gid); return true; })) {
      *outX = t.first; *outY = t.second + 1;
      return true;
    }
  return false;
}
// the row of a bed's head (the wall it stands against is right above it)
int bedHead(const Fit& F, int bx, int by) { return F.m.propAt(bx, by - 1) == (int)Prop::Filler + 1 ? by - 1 : by; }

// a 3-wide piece (Filler, p, Filler) against the north wall centred on cx (or anywhere when cx < 0)
bool northWide(Fit& F, int ri, Prop p, int cx, bool outerOnly, int* outX = nullptr) {
  std::vector<std::pair<int, int>> c;
  F.tiles(ri, [&](int x, int y) {
    if (cx >= 0 && x != cx) return;
    if (outerOnly && y != 2) return;
    for (int k = -1; k <= 1; k++)
      if (!F.wallN(x + k, y) || !F.freeT(x + k, y) || F.roomOf(x + k, y) != ri || !F.wallT(x + k, y - 2)) return;
    if (!F.wallT(x - 2, y - 1) || !F.wallT(x + 2, y - 1)) return;
    c.push_back({x, y});
  });
  F.shuffle(c);
  for (auto& t : c)
    if (F.group([&](int gid) {
          F.put(t.first - 1, t.second, Prop::Filler, gid); F.put(t.first, t.second, p, gid); F.put(t.first + 1, t.second, Prop::Filler, gid);
          return true;
        })) {
      if (outX) *outX = t.first;
      return true;
    }
  return false;
}
// low pieces along the side and front walls (barrels, crates, woodpiles, chests)
bool wallPiece(Fit& F, int ri, Prop p, bool northOk = false, bool gap = true) {
  // fix round 3: firewood and hay are kept in one place, not a stack by every wall: at most two of each per floor (a
  // barn may hold more hay); past that the spot gets a crate or a barrel instead
  if (p == Prop::Woodpile && F.countProp(Prop::Woodpile) >= 2) p = F.r.f() < 0.5f ? Prop::Crate : Prop::Barrel;
  if (p == Prop::Haystack && F.countProp(Prop::Haystack) >= 2) p = Prop::Crate;
  std::vector<std::pair<int, int>> c;
  F.tiles(ri, [&](int x, int y) {
    if (!F.freeT(x, y)) return;
    bool n = F.wallN(x, y);
    if (!(F.wallSide(x, y) || F.wallS(x, y) || (northOk && n))) return;
    if (n && !northOk) return;
    c.push_back({x, y});
  });
  F.shuffle(c);
  // spots with a sprite right above or below come last: two pieces in one column overlap and read as stacked
  auto stacked = [&](const std::pair<int, int>& t) {
    for (int dy : {-1, 1}) {
      int q = F.m.propAt(t.first, t.second + dy);
      if (q && q != (int)Prop::Filler + 1 && q != (int)Prop::DoorH + 1 && q != (int)Prop::DoorV + 1) return true;
    }
    return false;
  };
  std::stable_sort(c.begin(), c.end(), [&](const std::pair<int, int>& a, const std::pair<int, int>& b) { return !stacked(a) && stacked(b); });
  for (size_t k = 0; k < c.size() && k < 24; k++)
    if (F.group([&](int gid) { if (gap && !F.gapOk(c[k].first, c[k].second, c[k].first, c[k].second, gid) && F.r.f() < 0.7f) return false; F.put(c[k].first, c[k].second, p, gid); return true; }))
      return true;
  return false;
}
// a table (1..len tiles) with seats around it, standing free in the room with the gap rule
bool tableSet(Fit& F, int ri, int x, int y, int len, Prop single, int seats, bool benches, int gid) {
  int x1 = x + len - 1;
  if (!F.rectFree(x - 1, y - 1, x1 + 1, y + 1, ri) || !F.gapOk(x - 1, y - 1, x1 + 1, y + 1, gid)) return false;
  if (len == 1) F.put(x, y, single, gid);
  else {
    F.put(x, y, Prop::TableL, gid);
    for (int k = x + 1; k < x1; k++) F.put(k, y, Prop::TableM, gid);
    F.put(x1, y, Prop::TableR, gid);
  }
  if (benches) {
    for (int k = x; k <= x1; k++) { F.put(k, y - 1, Prop::Bench, gid); F.put(k, y + 1, Prop::Bench, gid); }
    return true;
  }
  // seats around the table, never all on one side: facing pairs first (north and south of the same board, or both
  // ends), then the rest. A chair can only stand north of a table (its back to the wall of the room, facing us); the
  // other sides get stools.
  std::vector<std::pair<int, int>> spots;
  std::vector<std::pair<int, int>> nsPairs;
  for (int k = x; k <= x1; k++) nsPairs.push_back({k, 0});
  F.shuffle(nsPairs);
  bool endsFirst = len == 1 && F.r.f() < 0.45f;
  if (endsFirst) { spots.push_back({x - 1, y}); spots.push_back({x1 + 1, y}); }
  for (auto& k : nsPairs) { spots.push_back({k.first, y - 1}); spots.push_back({k.first, y + 1}); }
  if (!endsFirst) { spots.push_back({x - 1, y}); spots.push_back({x1 + 1, y}); }
  if (seats == 1) { spots.resize(std::min<size_t>(spots.size(), 2)); F.shuffle(spots); }
  // fix round 3: not one stamp everywhere. The house's seat style, and per table a hashed variation (no rng draw):
  // some tables lose a seat or two, a few have a chair pulled out to an end
  uint32_t th = hash32((uint32_t)(x * 73856093) ^ (uint32_t)(y * 19349663) ^ (uint32_t)(F.P.b ? F.P.b->seed : 0u));
  int style = F.seatStyle;
  if (th % 5u == 0) style = (style + 1) % 3;
  if (seats >= 3 && (th >> 4) % 3u == 0) seats--;
  int placed = 0;
  for (auto& s : spots) {
    if (placed >= seats) break;
    bool north = s.second == y - 1, south = s.second == y + 1;
    if (style == 2 && south && len >= 2) continue;          // chairs on the far side only: the near side left open
    Prop seat = style == 1 ? Prop::Stool : (north ? Prop::Chair : Prop::Stool);
    F.put(s.first, s.second, seat, gid);
    placed++;
  }
  return placed >= std::min(2, seats);
}
bool tableIn(Fit& F, int ri, int len, Prop top, int seats, bool benches, int tries, int* ox = nullptr, int* oy = nullptr) {
  std::vector<std::pair<int, int>> c;
  F.tiles(ri, [&](int x, int y) { c.push_back({x, y}); });
  F.shuffle(c);
  int t = 0;
  for (auto& s : c) {
    if (t++ >= tries) break;
    if (F.group([&](int gid) { return tableSet(F, ri, s.first, s.second, len, top, seats, benches, gid); })) {
      if (ox) *ox = s.first;
      if (oy) *oy = s.second;
      return true;
    }
  }
  return false;
}
void rugRect(Fit& F, int ri, int x0, int y0, int x1, int y1, Deco d) {
  for (int y = y0; y <= y1; y++)
    for (int x = x0; x <= x1; x++)
      if (F.inRoom(x, y, ri) && !(x == F.m.exitX && y == F.m.exitY) && F.m.decoAt(x, y) == 0) F.deco(x, y, d);
}
// furniture a rug may lie under: tables and their seats. Anything else on a rug (a chest on its edge, a barrel on its
// corner) reads as two sprites overlapping
bool rugOk(int p) {
  if (!p) return true;
  switch ((Prop)(p - 1)) {
    case Prop::TableSmall: case Prop::TableMeal: case Prop::TableWork: case Prop::TableL: case Prop::TableM: case Prop::TableR:
    case Prop::Chair: case Prop::Stool: case Prop::Bench:
      return true;
    default: return false;
  }
}
// a w x h rug in room ri as near (cx, cy) (its centre) as it fits on clear floor; false when it fits nowhere
bool rugFit(Fit& F, int ri, int w, int h, int cx, int cy, Deco d) {
  const IRect& R = F.g.rooms[(size_t)ri].r;
  int best = -1, bd = 1 << 30;
  for (int y0 = R.y; y0 + h <= R.y + R.h; y0++)
    for (int x0 = R.x; x0 + w <= R.x + R.w; x0++) {
      bool ok = true;
      for (int y = y0; y < y0 + h && ok; y++)
        for (int x = x0; x < x0 + w && ok; x++)
          ok = F.inRoom(x, y, ri) && !(x == F.m.exitX && y == F.m.exitY) && F.m.decoAt(x, y) == 0 && rugOk(F.m.propAt(x, y)) &&
               !F.g.isKeep(F.I(x, y));
      if (!ok) continue;
      int dx = x0 * 2 + w - 1 - cx * 2, dy = y0 * 2 + h - 1 - cy * 2;
      int sc = dx * dx + dy * dy;
      if (sc < bd) { bd = sc; best = F.I(x0, y0); }
    }
  if (best < 0) return false;
  rugRect(F, ri, best % F.W, best / F.W, best % F.W + w - 1, best / F.W + h - 1, d);
  return true;
}
// after furnishing: a rug that something other than a table or a seat ended up standing on goes (whole)
void rugCleanup(Fit& F) {
  std::vector<uint8_t> seen((size_t)F.W * F.H, 0);
  for (int i = 0; i < F.W * F.H; i++) {
    int d = F.m.deco[(size_t)i];
    if (seen[(size_t)i] || d < (int)Deco::RugRed || d > (int)Deco::RugGold) continue;
    std::vector<int> blob{i};
    seen[(size_t)i] = 1;
    bool bad = false;
    for (size_t h = 0; h < blob.size(); h++) {
      int x = blob[h] % F.W, y = blob[h] / F.W;
      if (!rugOk(F.m.prop[(size_t)blob[h]])) bad = true;
      static const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
      for (int k = 0; k < 4; k++) {
        int nx = x + dx[k], ny = y + dy[k];
        if (!F.m.in(nx, ny) || seen[(size_t)F.I(nx, ny)] || F.m.decoAt(nx, ny) != d) continue;
        seen[(size_t)F.I(nx, ny)] = 1;
        blob.push_back(F.I(nx, ny));
      }
    }
    if (bad) for (int t : blob) F.m.deco[(size_t)t] = 0;
  }
}
Deco rugFor(Fit& F) {
  static const Deco rugs[4] = {Deco::RugRed, Deco::RugBlue, Deco::RugGreen, Deco::RugGold};
  if (F.biome == Biome::Snow || F.biome == Biome::Taiga) return F.r.f() < 0.6f ? Deco::RugGold : Deco::RugRed;   // furs and wool
  if (F.biome == Biome::Desert) return F.r.f() < 0.5f ? Deco::RugRed : Deco::RugBlue;
  return rugs[F.r.irange(4)];
}

// a room much bigger than its anchors gets a sitting corner and more storage, so it never reads as an empty hall
void bigRoomFill(Fit& F, int ri) {
  const IRect& R = F.g.rooms[(size_t)ri].r;
  int area = R.w * R.h;
  if (area < 28) return;
  tableIn(F, ri, 1, F.r.f() < 0.5f ? Prop::TableSmall : Prop::TableMeal, 2 + F.r.irange(2), false, 40);
  northPiece(F, ri, F.r.f() < 0.5f ? Prop::Cupboard : Prop::Wardrobe, 0);
  // (M1) a lord's rooms are furnished, not stored: no crates or barrels in a palace's or keep's chambers
  const bool grand = F.P.wealth >= 3;
  wallPiece(F, ri, grand || F.r.f() < 0.5f ? Prop::Chest : Prop::Crate);
  wallPiece(F, ri, Prop::PlantPot);
  if (area >= 48) {
    tableIn(F, ri, 1, Prop::TableSmall, 2, false, 40);
    wallPiece(F, ri, grand ? (F.r.f() < 0.5f ? Prop::Candelabra : Prop::Bench) : F.r.f() < 0.5f ? Prop::Barrel : Prop::Bench);
  }
}

// ---- rooms ----------------------------------------------------------------------------------------------------------
struct Ctx {
  int ownerX = -1, ownerY = -1;          // the owner's post (spawn slot 0, ground floor)
  std::vector<std::pair<int, int>> seats;   // inn patrons, keep guards
  bool kitchenFire = false;              // a kitchen on this floor already has the hearth
};

void furnishBedroom(Fit& F, int ri, RoomKind kind, bool child, Ctx& cx) {
  (void)cx;
  RoomDef& RD = const_cast<RoomDef&>(F.g.rooms[(size_t)ri]);
  int bx = -1, by = -1;
  bool barracks = kind == RoomKind::Barracks;
  if (barracks) { if (!northPiece(F, ri, Prop::BunkBed, 1, &bx, &by)) northPiece(F, ri, Prop::BunkBed, 0, &bx, &by); }
  else if (!bedPiece(F, ri, 1, &bx, &by) && !northPiece(F, ri, Prop::Bed, 1, &bx, &by)) northPiece(F, ri, Prop::Bed, 0, &bx, &by);
  RD.bedX = bx; RD.bedY = by;   // recorded on the map's RoomInfo by the caller
  const IRect& R = RD.r;
  if (barracks) {
    int more = std::max(1, R.w / 2);
    for (int k = 0; k < more; k++) northPiece(F, ri, Prop::BunkBed, 1);
    northPiece(F, ri, Prop::WeaponRack, 0);
    if (F.r.f() < 0.6f) northPiece(F, ri, Prop::WeaponRack, 0);
    for (int k = 0; k < 2; k++) wallPiece(F, ri, Prop::Chest);
    tableIn(F, ri, 1, Prop::TableSmall, 2, false, 20);
    return;
  }
  if (bx >= 0) {
    // the nightstand by the bed's head, the chest at its foot
    const int hy = bedHead(F, bx, by);
    int side = F.wallT(bx - 1, hy) ? 1 : (F.wallT(bx + 1, hy) ? -1 : (F.r.f() < 0.5f ? 1 : -1));
    bool ns = kind == RoomKind::GuestRoom || kind == RoomKind::OwnerRoom || F.r.f() < 0.7f;
    if (ns) {
      bool put = F.group([&](int gid) { if (!F.freeT(bx + side, hy) || !F.wallN(bx + side, hy)) return false; F.put(bx + side, hy, Prop::Nightstand, gid); return true; }) ||
                 F.group([&](int gid) { if (!F.freeT(bx - side, hy) || !F.wallN(bx - side, hy)) return false; F.put(bx - side, hy, Prop::Nightstand, gid); return true; });
      // a rented room always has its candle: anywhere on the wall if not by the bed
      if (!put && kind == RoomKind::GuestRoom && !northPiece(F, ri, Prop::Nightstand, 0)) wallPiece(F, ri, Prop::Nightstand, false, false);
    }
    if (child) F.group([&](int gid) { int x = bx - side; if (!F.freeT(x, hy) || !F.wallN(x, hy)) x = bx + side; if (!F.freeT(x, hy) || !F.wallN(x, hy)) return false; F.put(x, hy, Prop::Cradle, gid); return true; });
    bool chest = F.group([&](int gid) { if (by + 1 > R.y + R.h - 1 || !F.freeT(bx, by + 1)) return false; F.put(bx, by + 1, Prop::Chest, gid); return true; });
    if (!chest && kind != RoomKind::Bedroom) chest = wallPiece(F, ri, Prop::Chest, true, false);
    if (!chest && kind == RoomKind::Bedroom && F.r.f() < 0.5f) wallPiece(F, ri, Prop::Chest, true);
  }
  if (kind == RoomKind::OwnerRoom || (kind == RoomKind::Bedroom && F.P.wealth >= 1 && F.r.f() < 0.6f)) {
    northPiece(F, ri, F.r.f() < 0.5f ? Prop::Wardrobe : Prop::Dresser, 1);
    if (kind == RoomKind::OwnerRoom) northPiece(F, ri, F.P.type == Building::Inn || F.r.f() < 0.5f ? Prop::Desk : Prop::Dresser, 0);
  }
  if (kind == RoomKind::GuestRoom) {
    // a rented room always has its candle (a nightstand or a small table) and a chest for the guest's things
    auto has = [&](Prop p) { bool h = false; F.tiles(ri, [&](int x, int y) { if (F.m.propAt(x, y) == (int)p + 1) h = true; }); return h; };
    if (!has(Prop::Nightstand) && !northPiece(F, ri, Prop::Nightstand, 0) && !wallPiece(F, ri, Prop::Nightstand, false, false) &&
        !tableIn(F, ri, 1, Prop::TableSmall, 1, false, 30))
      wallPiece(F, ri, Prop::TableSmall, false, false);
    if (!has(Prop::Chest)) wallPiece(F, ri, Prop::Chest, true, false);
    if (F.r.f() < 0.5f) { if (!northPiece(F, ri, Prop::Washstand, 0)) wallPiece(F, ri, Prop::Washstand); }
    if (R.w * R.h >= 12 && F.r.f() < 0.55f) tableIn(F, ri, 1, Prop::TableSmall, 1, false, 16);
  }
  if (kind == RoomKind::Bedroom && !child && F.r.f() < 0.35f) northPiece(F, ri, F.r.f() < 0.5f ? Prop::Cupboard : Prop::Shelf, 0);
  if (F.P.type == Building::Tower || F.P.type == Building::Keep || F.P.type == Building::Palace) {
    northPiece(F, ri, Prop::Bookshelf, 0);
    if (F.r.f() < 0.6f) northPiece(F, ri, Prop::Candelabra, 0);
  }
  if (F.P.type == Building::Tower) {
    // fix round 3: the mage sleeps among the work: a writing desk and stool, a lectern with the open grimoire, a
    // brazier or a cauldron, a chest of reagents, an urn or a plant (the top floor was a bed and bare flagstones)
    int dx = -1, dy = -1;
    if (northPiece(F, ri, Prop::Desk, 0, &dx, &dy))
      F.group([&](int gid) { if (!F.freeT(dx, dy + 1)) return false; F.put(dx, dy + 1, Prop::Stool, gid); return true; });
    if (!wallPiece(F, ri, Prop::Lectern)) northPiece(F, ri, Prop::Lectern, 0);
    wallPiece(F, ri, F.r.f() < 0.5f ? Prop::Cauldron : Prop::Brazier);
    wallPiece(F, ri, Prop::Chest);
    if (R.w * R.h >= 20) wallPiece(F, ri, F.r.f() < 0.5f ? Prop::Urn : Prop::PlantPot);
  }
  if (F.biome == Biome::Desert && F.r.f() < 0.6f) wallPiece(F, ri, Prop::PlantPot);
  bigRoomFill(F, ri);
  if (F.r.f() < (kind == RoomKind::GuestRoom ? 0.45f : 0.65f) && R.h >= 3) {
    if (R.w * R.h >= 30) rugFit(F, ri, 3, 2, R.cx(), R.cy(), rugFor(F));   // the middle of a big room
    else if (bx >= 0) {   // beside the bed, where you step out of it
      int s2 = F.wallT(bx + 1, by) ? -1 : 1;
      if (!rugFit(F, ri, 2, 2, bx + s2, by + 1, rugFor(F))) rugFit(F, ri, 2, 1, bx + s2, by + 1, rugFor(F));
    }
  }
}

void furnishKitchen(Fit& F, int ri, Ctx& cx) {
  const IRect& R = F.g.rooms[(size_t)ri].r;
  bool fire = F.P.b->hearth;
  int hx = -1;
  if (fire) {
    bool hearth = F.r.f() < (F.biome == Biome::Desert ? 0.2f : 0.65f) && northWide(F, ri, Prop::Hearth, -1, true, &hx);
    if (!hearth) { if (!northPiece(F, ri, Prop::Oven, 1, &hx, nullptr, true)) northWide(F, ri, Prop::Hearth, -1, true, &hx); }
    else if (R.w >= 7 && F.r.f() < 0.5f) northPiece(F, ri, Prop::Oven, 1, nullptr, nullptr, true);
    cx.kitchenFire = hx >= 0;
  }
  // the prep table by the fire, pantry shelves and a cupboard on the wall, stores along the sides
  F.group([&](int gid) {
    for (int d : {2, -2, 3, -3, 1, -1}) {
      int x = hx + d, y = R.y;
      if (hx < 0) { x = R.x + F.r.irange(R.w); }
      if (F.freeT(x, y) && F.wallN(x, y) && F.roomOf(x, y) == ri) { F.put(x, y, Prop::PrepTable, gid); return true; }
    }
    return false;
  }) || northPiece(F, ri, Prop::PrepTable, 0) || wallPiece(F, ri, Prop::PrepTable);
  northPiece(F, ri, F.r.f() < 0.6f ? Prop::Cupboard : Prop::Shelf, 1);
  if (R.w >= 5) northPiece(F, ri, Prop::Shelf, 0);
  int stores = 2 + R.w * R.h / 10;
  static const Prop st[4] = {Prop::Barrel, Prop::Crate, Prop::Barrel2, Prop::Barrel};
  if (hx >= 0) wallPiece(F, ri, Prop::Woodpile);   // one stack of firewood for the fire, not a woodpile per wall
  for (int k = 0; k < stores; k++) wallPiece(F, ri, st[F.r.irange(4)]);
  if (R.w * R.h >= 15 && F.r.f() < 0.6f) tableIn(F, ri, 1, Prop::TableSmall, 1 + F.r.irange(2), false, 16);
}

void furnishHallish(Fit& F, int ri, RoomKind kind, Ctx& cx) {
  const IRect& R = F.g.rooms[(size_t)ri].r;
  const Bldg& b = *F.P.b;
  if (b.type == Building::Barracks && F.m.floor == 0) {
    // the guard's mess hall: weapon racks along the back, the long table with benches, the sergeant at its head
    northPiece(F, ri, Prop::WeaponRack, 1);
    northPiece(F, ri, Prop::WeaponRack, 0);
    int tx = -1, ty = -1;
    if (tableIn(F, ri, 3, Prop::TableMeal, 0, true, 60, &tx, &ty) || tableIn(F, ri, 2, Prop::TableMeal, 0, true, 60, &tx, &ty)) { cx.ownerX = tx; cx.ownerY = ty + 2; }
    wallPiece(F, ri, Prop::Chest);
  }
  bool cottage = kind == RoomKind::Cottage;
  // the hearth on the outer back wall (a home without a separate kitchen)
  int hx = -1;
  if (F.P.b->hearth && !cx.kitchenFire && F.m.floor == 0) {
    bool desert = F.biome == Biome::Desert && F.r.f() < 0.8f;   // a domed clay oven, not an open fire
    if (desert) northPiece(F, ri, Prop::Oven, 1, &hx, nullptr, true);
    if (hx < 0 && !northWide(F, ri, Prop::Hearth, -1, true, &hx) && F.r.f() < 0.5f) northPiece(F, ri, Prop::Oven, 1, &hx, nullptr, true);
  }
  if (cottage) {   // one-room home: the bed in a back corner, a chest at its foot
    int bx = -1, by = -1;
    if (bedPiece(F, ri, 1, &bx, &by) || northPiece(F, ri, Prop::Bed, 1, &bx, &by)) {
      RoomDef& RD = const_cast<RoomDef&>(F.g.rooms[(size_t)ri]);
      RD.bedX = bx; RD.bedY = by;
      const int hy = bedHead(F, bx, by);
      if (F.r.f() < 0.6f) F.group([&](int gid) { if (!F.freeT(bx, by + 1)) return false; F.put(bx, by + 1, Prop::Chest, gid); return true; });
      if (F.r.f() < 0.25f) F.group([&](int gid) { int s = F.wallT(bx - 1, hy) ? 1 : -1; if (!F.freeT(bx + s, hy) || !F.wallN(bx + s, hy)) return false; F.put(bx + s, hy, Prop::Cradle, gid); return true; });
    }
  }
  // the table: in the middle of the hall, chairs around it, a rug under it in better homes
  bool big = R.w >= 6 && F.r.f() < 0.6f;
  Prop top = F.r.f() < 0.55f ? Prop::TableMeal : (b.owner == Role::Mage || (F.P.wealth >= 2 && F.r.f() < 0.4f) ? Prop::TableWork : Prop::TableSmall);
  int tx = -1, ty = -1;
  if (tableIn(F, ri, big ? 2 : 1, top, 2 + F.r.irange(3), false, 40, &tx, &ty) || tableIn(F, ri, 1, top, 2, false, 40, &tx, &ty)) {
    if (cx.ownerX < 0) { cx.ownerX = tx; cx.ownerY = ty + 1; }
    if ((F.P.wealth >= 1 && F.r.f() < 0.65f) || F.biome == Biome::Desert) rugRect(F, ri, tx - 1, ty - 1, tx + (big ? 2 : 1), ty + 1, rugFor(F));
  }
  if (cx.ownerX < 0 && hx >= 0) { cx.ownerX = hx; cx.ownerY = R.y + 1; }
  // storage on the walls
  northPiece(F, ri, F.r.f() < 0.6f ? Prop::Cupboard : Prop::Shelf, 0);
  if (R.w * R.h >= 30) northPiece(F, ri, F.r.f() < 0.5f ? Prop::Shelf : Prop::Cupboard, 0);
  // a craft corner: loom or spinning wheel; a scholar's desk with a bookshelf
  if (F.r.f() < (b.type == Building::Hut ? 0.35f : 0.55f)) {
    bool scholar = b.owner == Role::Mage || (F.P.wealth >= 2 && F.r.f() < 0.4f);
    if (scholar) { if (northPiece(F, ri, Prop::Bookshelf, 1)) northPiece(F, ri, Prop::Desk, 0); }
    else northPiece(F, ri, F.r.f() < 0.5f ? Prop::Loom : Prop::SpinningWheel, 0);
  }
  bool cold = F.biome == Biome::Snow || F.biome == Biome::Taiga;
  if ((hx >= 0 || cold) && F.m.floor == 0) wallPiece(F, ri, Prop::Woodpile);
  wallPiece(F, ri, F.r.f() < 0.5f ? Prop::Barrel : Prop::Crate);
  if (F.P.wealth >= 1 || F.biome == Biome::Desert) wallPiece(F, ri, Prop::PlantPot);
  if (b.owner == Role::Farmer && F.r.f() < 0.6f) wallPiece(F, ri, Prop::Haystack);
  if (F.biome == Biome::Swamp && F.r.f() < 0.6f) wallPiece(F, ri, Prop::Barrel);
  if (R.w * R.h >= 40) bigRoomFill(F, ri);
}

void furnishStore(Fit& F, int ri, RoomKind kind) {
  const IRect& R = F.g.rooms[(size_t)ri].r;
  if ((F.P.type == Building::Keep || F.P.type == Building::Palace || F.P.type == Building::Barracks) && kind == RoomKind::Storeroom && F.m.floor == 0) {   // the keep's armoury
    for (int k = 0; k < 1 + R.w / 3; k++) northPiece(F, ri, Prop::WeaponRack, k == 0 ? 1 : 0);
    wallPiece(F, ri, Prop::Chest);
    wallPiece(F, ri, Prop::Crate);
    if (R.w * R.h >= 12) wallPiece(F, ri, Prop::Barrel);
    return;
  }
  northPiece(F, ri, kind == RoomKind::Stockroom && F.r.f() < 0.5f ? Prop::Cupboard : Prop::Shelf, 1);
  if (R.w >= 4) northPiece(F, ri, kind == RoomKind::Stockroom ? Prop::Shelf : (F.r.f() < 0.5f ? Prop::Shelf : Prop::Barrel2), 0);
  if (R.w >= 6) northPiece(F, ri, Prop::Shelf, 0);
  static const Prop st[6] = {Prop::Barrel, Prop::Crate, Prop::Barrel2, Prop::Crate, Prop::Barrel, Prop::Woodpile};
  static const Prop barn[5] = {Prop::Haystack, Prop::Woodpile, Prop::Barrel, Prop::Crate, Prop::Haystack};
  int n = 2 + R.w * R.h / 6;
  for (int k = 0; k < n; k++) wallPiece(F, ri, kind == RoomKind::Barn ? barn[F.r.irange(5)] : st[F.r.irange(6)], kind == RoomKind::Barn);
  if (kind == RoomKind::Barn) { northPiece(F, ri, Prop::Workbench, 0); northPiece(F, ri, Prop::Haystack, 1); }
}

void furnishStudy(Fit& F, int ri) {
  const IRect& R = F.g.rooms[(size_t)ri].r;
  int n = 1 + R.w / 3;
  for (int k = 0; k < n; k++) northPiece(F, ri, Prop::Bookshelf, k == 0 ? 1 : 0);
  int dx = -1, dy = -1;
  if (northPiece(F, ri, Prop::Desk, 2, &dx, &dy))
    F.group([&](int gid) { if (!F.freeT(dx, dy + 1)) return false; F.put(dx, dy + 1, Prop::Stool, gid); return true; });
  if (!wallPiece(F, ri, Prop::Lectern)) northPiece(F, ri, Prop::Lectern, 0);
  if (R.w * R.h >= 16) northPiece(F, ri, Prop::Candelabra, 0);
  if (R.w * R.h >= 20 && F.r.f() < 0.6f) tableIn(F, ri, 1, Prop::TableSmall, 2, false, 20);
  if (R.w * R.h >= 30) {   // a library floor: a reading table, more lecterns and candles, crates of scrolls
    tableIn(F, ri, 2, Prop::TableWork, 3, false, 40);
    wallPiece(F, ri, Prop::Lectern);
    wallPiece(F, ri, Prop::Crate);
    northPiece(F, ri, Prop::Candelabra, 0);
  }
  bigRoomFill(F, ri);
  if (F.r.f() < 0.7f && R.h >= 3 && !rugFit(F, ri, 3, R.h >= 5 ? 3 : 2, R.cx(), R.cy(), rugFor(F))) rugFit(F, ri, 2, 2, R.cx(), R.cy(), rugFor(F));
}

void furnishWorkshop(Fit& F, int ri, Ctx& cx) {
  const IRect& R = F.g.rooms[(size_t)ri].r;
  // (M1 economy) the trades' work rooms
  const Building t = F.P.type;
  if (t != Building::Tower) {
    if (t == Building::Windmill || t == Building::Watermill) {   // the millstones, sacks of grain and of flour, the miller's bench
      northPiece(F, ri, Prop::Grindstone, 1);
      if (R.w >= 5) northPiece(F, ri, Prop::Grindstone, 0);
      for (int k = 0; k < 2 + R.w * R.h / 12; k++) wallPiece(F, ri, k % 3 == 2 ? Prop::Barrel : Prop::Sacks);
      wallPiece(F, ri, Prop::Workbench);
    } else if (t == Building::Sawmill) {   // the saw bench, timber stacked to season, the saw doctor's grindstone
      northPiece(F, ri, Prop::Workbench, 1);
      northPiece(F, ri, Prop::Workbench, 0);
      tableIn(F, ri, 1, Prop::TableWork, 0, false, 20);
      for (int k = 0; k < 2 + R.w * R.h / 14; k++) wallPiece(F, ri, k % 2 ? Prop::Woodpile : Prop::Crate);
      wallPiece(F, ri, Prop::Grindstone);
    } else if (t == Building::Tanner) {   // the soaking vats, the scraping table, hides drying on the frame
      northPiece(F, ri, Prop::HideRack, 1);
      for (int k = 0; k < 2; k++) wallPiece(F, ri, Prop::QuenchTub);
      tableIn(F, ri, 1, Prop::TableWork, 0, false, 20);
      wallPiece(F, ri, Prop::Barrel);
      wallPiece(F, ri, Prop::Barrel2);
    } else if (t == Building::Weaver) {   // the loom by the window, the spinning wheel, wool in baskets
      northPiece(F, ri, Prop::Loom, 1);
      northPiece(F, ri, Prop::Shelf, 0);
      wallPiece(F, ri, Prop::SpinningWheel);
      wallPiece(F, ri, Prop::Baskets);
      wallPiece(F, ri, Prop::Crate);
    } else {
      northPiece(F, ri, Prop::Workbench, 1);
      wallPiece(F, ri, Prop::Crate);
      wallPiece(F, ri, Prop::Barrel);
    }
    if (cx.ownerX < 0) { cx.ownerX = R.cx(); cx.ownerY = R.cy(); }
    return;
  }
  // the laboratory: a cauldron in the middle, a work table, shelves of jars and books
  F.group([&](int gid) {
    int x = R.cx() + F.r.irange(3) - 1, y = R.y + 2 + F.r.irange(std::max(1, R.h - 5));
    if (!F.freeT(x, y) || !F.gapOk(x, y, x, y, gid)) return false;
    F.put(x, y, Prop::Cauldron, gid);
    cx.ownerX = x + 1; cx.ownerY = y;
    return true;
  });
  northPiece(F, ri, Prop::Bookshelf, 1);
  northPiece(F, ri, Prop::Shelf, 0);
  northPiece(F, ri, Prop::Cupboard, 0);
  tableIn(F, ri, 1, Prop::TableWork, 1 + F.r.irange(2), false, 30);
  wallPiece(F, ri, Prop::PlantPot);
  wallPiece(F, ri, F.r.f() < 0.5f ? Prop::Barrel : Prop::Crate);
  if (F.r.f() < 0.6f) northPiece(F, ri, Prop::Candelabra, 0);
  if (cx.ownerX < 0) { cx.ownerX = R.cx(); cx.ownerY = R.cy(); }
}

bool pillarOk(const Fit& F, int x, int y, int ri);
void furnishCorridor(Fit& F, int ri) {
  const IRect& R = F.g.rooms[(size_t)ri].r;
  // a runner down the middle of a long corridor; a lounge corner where it widens
  if (R.w >= 6 && R.h <= 3) {
    int y = R.y + (R.h >= 2 ? R.h - 1 : 0);
    for (int x = R.x + 1; x <= R.x + R.w - 2; x++) if (F.inRoom(x, y, ri)) F.deco(x, y, Deco::RugRed);
  }
  if (R.h >= 3 && R.w * R.h >= 24) {   // where the corridor widens into a landing: a sitting corner
    int tx = -1, ty = -1;
    if (tableIn(F, ri, F.r.f() < 0.5f ? 2 : 1, F.r.f() < 0.5f ? Prop::TableMeal : Prop::TableSmall, 2 + F.r.irange(2), F.r.f() < 0.3f, 40, &tx, &ty))
      rugRect(F, ri, tx - 1, ty - 1, tx + 2, ty + 1, rugFor(F));
    if (F.P.type == Building::Keep || F.P.type == Building::Palace) {
      northPiece(F, ri, Prop::Banner, 0); northPiece(F, ri, Prop::Banner, 0);
      // fix round 2: the keep's gallery is the household's sitting room too: shelves, a cupboard, a statue in a niche
      northPiece(F, ri, F.r.f() < 0.5f ? Prop::Bookshelf : Prop::Cupboard, 0);
      if (F.r.f() < 0.6f) northPiece(F, ri, Prop::Statue, 1);
      if (R.w * R.h >= 60) {
        // fix round 3: the gallery's open floor below the room row: a long table for the household with its seats
        // and a runner, columns where they stand free, statues and candle-stands along the walls
        int lx = -1, ly = -1;
        if (tableIn(F, ri, 3, Prop::TableMeal, 4 + F.r.irange(2), F.r.f() < 0.4f, 60, &lx, &ly))
          rugRect(F, ri, lx - 1, ly - 1, lx + 3, ly + 1, rugFor(F));
        for (int k = 0; k < 2; k++) {
          std::vector<std::pair<int, int>> pc;
          F.tiles(ri, [&](int x, int y) { if (pillarOk(F, x, y, ri) && !F.wallSide(x, y) && !F.wallS(x, y)) pc.push_back({x, y}); });
          F.shuffle(pc);
          for (size_t q = 0; q < pc.size() && q < 12; q++)
            if (F.group([&](int gid) { if (!F.gapOk(pc[q].first, pc[q].second, pc[q].first, pc[q].second, gid)) return false; F.put(pc[q].first, pc[q].second, Prop::Pillar, gid); return true; }))
              break;
        }
        wallPiece(F, ri, Prop::Statue);
        wallPiece(F, ri, Prop::Candelabra);
        int tx2 = -1, ty2 = -1;
        if (tableIn(F, ri, 1, Prop::TableSmall, 2 + F.r.irange(2), false, 40, &tx2, &ty2) && F.r.f() < 0.6f) rugRect(F, ri, tx2 - 1, ty2 - 1, tx2 + 1, ty2 + 1, rugFor(F));
        if (F.r.f() < 0.6f) tableIn(F, ri, 1, Prop::TableWork, 1, false, 30);
        northPiece(F, ri, Prop::Candelabra, 0);
      }
    }
    if (R.w * R.h >= 40) {
      tableIn(F, ri, 1, Prop::TableSmall, 2, false, 40);
      wallPiece(F, ri, Prop::Bench);
      wallPiece(F, ri, F.r.f() < 0.5f ? Prop::Chest : Prop::Barrel);
      wallPiece(F, ri, Prop::PlantPot);
    }
  }
  wallPiece(F, ri, Prop::PlantPot);
  if (R.w * R.h >= 20) wallPiece(F, ri, F.P.type == Building::Keep || F.P.type == Building::Palace ? Prop::Chest : Prop::Bench);
}

// the inn's bar or the shop's counter: wares and kegs against the partition, the keeper's walkway, the counter with
// a gap at one end, the customers' row in front
void barCounter(Fit& F, int ri, bool inn, Ctx& cx) {
  const Plan& P = F.P;
  if (P.barY < 0) return;
  int by = P.barY, wy = by + 1, cy = by + 2;
  for (int x = P.barX0; x <= P.barX1; x++) { F.setLane(x, wy); F.setLane(x, cy + 1); }
  F.setLane(P.flapX, wy); F.setLane(P.flapX, cy); F.setLane(P.flapX, by);
  F.group([&](int gid) {
    if (P.barX1 - P.barX0 < 1) { F.put(P.barX0, cy, Prop::TableSmall, gid); return true; }
    F.put(P.barX0, cy, Prop::CounterL, gid);
    for (int x = P.barX0 + 1; x < P.barX1; x++) F.put(x, cy, Prop::CounterM, gid);
    F.put(P.barX1, cy, Prop::CounterR, gid);
    return true;
  });
  // the back shelf: bottles and kegs at the inn, wares at the shop
  static const Prop innBack[5] = {Prop::BottleShelf, Prop::Barrel2, Prop::BottleShelf, Prop::Cupboard, Prop::Barrel2};
  static const Prop shopBack[6] = {Prop::Shelf, Prop::Cupboard, Prop::Bookshelf, Prop::Shelf, Prop::BottleShelf, Prop::Crate};
  int k = F.r.irange(5);
  for (int x = P.barX0; x <= P.barX1; x++) {
    if (!F.freeT(x, by) || F.roomOf(x, by) != ri || !F.wallN(x, by)) continue;
    if (!inn && F.r.f() < 0.2f) continue;
    Prop p = inn ? innBack[(k++) % 5] : shopBack[F.r.irange(6)];
    F.group([&](int gid) { F.put(x, by, p, gid); return true; });
  }
  // stools at the bar
  if (inn)
    for (int x = P.barX0; x <= P.barX1; x += 2)
      if (F.floorT(x, cy + 1) && !F.m.propAt(x, cy + 1)) { size_t mk = F.log.size(); F.put(x, cy + 1, Prop::Stool, F.groups++); if (!F.valid()) F.rollback(mk); }
  cx.ownerX = (P.barX0 + P.barX1) / 2; cx.ownerY = wy;
}

void furnishCommon(Fit& F, int ri, Ctx& cx) {
  barCounter(F, ri, true, cx);
  const IRect& R = F.g.rooms[(size_t)ri].r;
  // the hearth on the common room's own stretch of back wall
  int hx = -1;
  if (northWide(F, ri, Prop::Hearth, -1, true, &hx)) rugRect(F, ri, hx - 1, 3, hx + 1, 4, rugFor(F));
  // tables: long ones with benches and round ones with stools, as many as the gap rule allows
  int want = 4 + R.w * R.h / 40 + F.r.irange(2);
  std::vector<std::pair<int, int>> c;
  F.tiles(ri, [&](int x, int y) {
    bool inBar = x >= std::min(F.P.barX0, F.P.flapX) - 1 && x <= std::max(F.P.barX1, F.P.flapX) + 1 && y >= F.P.barY && y <= F.P.barY + 3;
    if (!inBar) c.push_back({x, y});
  });
  F.shuffle(c);
  for (auto& s : c) {
    if (want <= 0) break;
    // mostly long trestle tables with benches down both sides (or chairs and stools facing across), some round
    // tables with three or four stools around them
    bool lng = F.r.f() < 0.7f;
    int len = lng ? 2 + F.r.irange(2) : 1;
    Prop top = F.r.f() < 0.6f ? Prop::TableMeal : Prop::TableSmall;
    bool benches = lng && F.r.f() < 0.65f;
    int seats = lng ? 2 * len + F.r.irange(2) : 3 + F.r.irange(2);
    if (F.group([&](int gid) { return tableSet(F, ri, s.first, s.second, len, top, seats, benches, gid); })) {
      want--;
      cx.seats.push_back({s.first, s.second + 1});
    }
  }
  for (int k = 0; k < 3; k++) wallPiece(F, ri, k == 0 ? Prop::Barrel2 : (F.r.f() < 0.5f ? Prop::Barrel : Prop::PlantPot));
}

void furnishShopfloor(Fit& F, int ri, Ctx& cx) {
  barCounter(F, ri, false, cx);
  const IRect& R = F.g.rooms[(size_t)ri].r;
  // wares laid out on display tables in the customers' half, goods along the walls by the door
  int cy = F.P.barY + 2;
  for (int k = 0; k < 2 + R.w / 6; k++) {
    std::vector<std::pair<int, int>> c;
    F.tiles(ri, [&](int x, int y) { if (y >= cy + 2 && y <= F.H - 3 && std::abs(x - F.P.ex) >= 2) c.push_back({x, y}); });
    F.shuffle(c);
    for (auto& s : c)
      if (F.group([&](int gid) {
            if (!F.freeT(s.first, s.second) || !F.gapOk(s.first, s.second, s.first, s.second, gid)) return false;
            F.put(s.first, s.second, Prop::DisplayTable, gid);
            return true;
          }))
        break;
  }
  static const Prop goods[5] = {Prop::Barrel, Prop::Crate, Prop::Barrel2, Prop::PlantPot, Prop::Crate};
  for (int k = 0; k < 4; k++) wallPiece(F, ri, goods[F.r.irange(5)]);
}

void furnishForge(Fit& F, int ri, Ctx& cx) {
  const IRect& R = F.g.rooms[(size_t)ri].r;
  int fx = -1;
  if (!northWide(F, ri, Prop::Forge, F.P.forgeX, true, &fx)) northWide(F, ri, Prop::Forge, -1, true, &fx);
  if (fx < 0) fx = R.cx();
  // the anvil a step in front of the fire, the quench tub beside it, the grindstone and bench nearby
  int ax = fx + (F.r.f() < 0.35f ? (F.r.f() < 0.5f ? -1 : 1) : 0), ay = 4 + (F.r.f() < 0.3f ? 1 : 0);
  if (!F.freeT(ax, ay) || F.roomOf(ax, ay) != ri) { ax = fx; ay = 4; }
  F.group([&](int gid) {
    if (!F.freeT(ax, ay) || F.roomOf(ax, ay) != ri) return false;
    F.put(ax, ay, Prop::Anvil, gid);
    int s = F.r.f() < 0.5f ? -1 : 1;
    if (F.freeT(ax + s, ay) && F.roomOf(ax + s, ay) == ri) F.put(ax + s, ay, Prop::QuenchTub, gid);
    else if (F.freeT(ax - s, ay) && F.roomOf(ax - s, ay) == ri) F.put(ax - s, ay, Prop::QuenchTub, gid);
    return true;
  });
  // the grindstone and a work table out in the shop, iron and coal by the walls
  F.group([&](int gid) {
    for (int t = 0; t < 12; t++) {
      int x = R.x + 1 + F.r.irange(std::max(1, R.w - 2)), y = 5 + F.r.irange(std::max(1, R.y + R.h - 7));
      if (std::abs(x - ax) < 2 && std::abs(y - ay) < 2) continue;
      if (!F.freeT(x, y) || F.roomOf(x, y) != ri || !F.gapOk(x, y, x, y, gid)) continue;
      F.put(x, y, Prop::Grindstone, gid);
      return true;
    }
    return false;
  });
  // fix round 3: the smith's own table (a meal or a plain board, never a scholar's candle and book), finished blades
  // on a display table by the door, a bench for waiting customers: no smithy shows the same small table three times
  if (F.r.f() < 0.7f) tableIn(F, ri, 1, F.r.f() < 0.5f ? Prop::TableMeal : Prop::TableSmall, 1, false, 40);
  cx.ownerX = F.freeT(ax - 1, ay) ? ax - 1 : ax + 1; cx.ownerY = ay;
  northPiece(F, ri, Prop::Workbench, 0);
  northPiece(F, ri, Prop::WeaponRack, 0);
  if (F.r.f() < 0.5f) northPiece(F, ri, Prop::WeaponRack, 0);
  wallPiece(F, ri, Prop::Woodpile);
  // a bench for finished work and mending by the door (fix round 2: not every smithy the same; a big shop gets two)
  int area = 0;
  F.tiles(ri, [&](int, int) { area++; });
  const int benches = area >= 90 ? 2 : (F.r.f() < 0.55f ? 1 : 0);
  for (int n = 0; n < benches; n++) {
    std::vector<std::pair<int, int>> c;
    F.tiles(ri, [&](int x, int y) { if (y >= F.H - 4 && std::abs(x - F.P.ex) >= 2) c.push_back({x, y}); });
    F.shuffle(c);
    for (auto& t : c)
      if (F.group([&](int gid) { if (!F.freeT(t.first, t.second) || !F.gapOk(t.first, t.second, t.first, t.second, gid)) return false; F.put(t.first, t.second, n == 0 ? Prop::DisplayTable : Prop::Bench, gid); return true; }))
        break;
  }
  if (area >= 90) { northPiece(F, ri, Prop::Workbench, 0); wallPiece(F, ri, Prop::Barrel2); }
  // a big shop floor is a working floor: a second station out in the open (grindstone and quench tub, or an
  // apprentice's anvil), so the middle is not one bare expanse
  if (area >= 70)
    F.group([&](int gid) {
      std::vector<std::pair<int, int>> c;
      F.tiles(ri, [&](int x, int y) { if (y >= 5 && y <= F.H - 5 && std::abs(x - F.P.ex) >= 2) c.push_back({x, y}); });
      F.shuffle(c);
      for (auto& t : c) {
        int x = t.first, y = t.second;
        if (!F.rectFree(x - 1, y - 1, x + 2, y + 1, ri) || !F.gapOk(x - 1, y - 1, x + 2, y + 1, gid)) continue;
        bool anvil = F.r.f() < 0.5f;
        F.put(x, y, anvil ? Prop::Anvil : Prop::Grindstone, gid);
        F.put(x + 1, y, anvil ? Prop::QuenchTub : Prop::Barrel2, gid);
        return true;
      }
      return false;
    });
  static const Prop stores[5] = {Prop::Crate, Prop::Barrel, Prop::Crate, Prop::Barrel2, Prop::Barrel};   // iron and coal
  // (M1) the iron and coal kept together in a store corner, away from the street door and the fire: a stacked block
  // of crates, barrels and the woodpile against two walls (they were spread one by one along every wall, so the
  // room read as a bare floor with things dropped round it)
  {
    struct Cn { int x, y, dx, dy, score; };
    std::vector<Cn> cs;
    for (int sy = 0; sy < 2; sy++)
      for (int sx = 0; sx < 2; sx++) {
        int x = sx ? R.x + R.w - 1 : R.x, y = sy ? R.y + R.h - 1 : R.y;
        // slide off the walls' face rows to the first floor tile of the room
        for (int k = 0; k < 4 && !F.inRoom(x, y, ri); k++) y += sy ? -1 : 1;
        if (!F.inRoom(x, y, ri)) continue;
        const int score = std::abs(x - F.P.ex) * 2 + std::abs(y - (F.H - 1)) + std::abs(x - fx) * 2 + (int)F.r.irange(3);
        cs.push_back({x, y, sx ? -1 : 1, sy ? -1 : 1, score});
      }
    // (ties broken by position: std::sort's order for equal keys differs between standard libraries)
    std::sort(cs.begin(), cs.end(), [](const Cn& a, const Cn& b) { return a.score != b.score ? a.score > b.score : a.x != b.x ? a.x < b.x : a.y != b.y ? a.y < b.y : a.dy != b.dy ? a.dy < b.dy : a.dx < b.dx; });
    static const Prop block[6] = {Prop::Barrel2, Prop::Crate, Prop::Barrel, Prop::Crate, Prop::Woodpile, Prop::Barrel};
    bool done = false;
    for (const Cn& c : cs) {
      if (done) break;
      // a 3 x 2 block against the corner, trimmed to 3 + 2 or 2 + 2 if the full one does not fit
      for (int wdt = 3; wdt >= 2 && !done; wdt--)
        done = F.group([&](int gid) {
          int n = 0;
          for (int j = 0; j < 2; j++)
            for (int i = 0; i < wdt - j; i++) {
              const int x = c.x + i * c.dx, y = c.y + j * c.dy;
              if (!F.freeT(x, y) || F.roomOf(x, y) != ri) return false;
              F.put(x, y, block[(size_t)((n + (int)F.r.irange(2)) % 6)], gid);
              n++;
            }
          const int x0 = std::min(c.x, c.x + (wdt - 1) * c.dx), x1 = std::max(c.x, c.x + (wdt - 1) * c.dx);
          const int y0 = std::min(c.y, c.y + c.dy), y1 = std::max(c.y, c.y + c.dy);
          return F.gapOk(x0, y0, x1, y1, gid);
        });
    }
    const int loose = done ? 1 + R.w / 6 : 2 + R.w / 3;
    for (int k = 0; k < loose; k++) wallPiece(F, ri, stores[F.r.irange(5)]);
  }
}

// a free-standing column: its sprite rises about two tiles over its base, so the two tiles above it must be open floor
// (no partition's cap or face row, no tall furniture); a column against a wall would stand inside the wall
bool pillarOk(const Fit& F, int x, int y, int ri) {
  if (!F.freeT(x, y) || F.roomOf(x, y) != ri) return false;
  for (int k = 1; k <= 2; k++) {
    if (!F.floorT(x, y - k)) return false;
    int p = F.m.propAt(x, y - k);
    if (p && tallProp((Prop)(p - 1))) return false;
  }
  return true;
}

void furnishNave(Fit& F, int ri, Ctx& cx) {
  const int ex = F.P.ex, W = F.W, H = F.H;
  // the aisle from the door to the altar stays clear
  for (int y = 3; y <= H - 2; y++) F.setLane(ex, y);
  F.group([&](int gid) {
    F.put(ex - 1, 2, Prop::Filler, gid); F.put(ex, 2, Prop::Altar, gid); F.put(ex + 1, 2, Prop::Filler, gid);
    for (int s : {-2, 2}) if (F.freeT(ex + s, 2) && F.roomOf(ex + s, 2) == ri) F.put(ex + s, 2, Prop::Candelabra, gid);
    return true;
  });
  bool big = W >= 17 || H >= 15;
  // pews in rows facing the altar, either side of the aisle
  for (int y = 5; y <= H - 4; y += 2)
    F.group([&](int gid) {
      for (int x = 2; x <= W - 3; x++) {
        if (std::abs(x - ex) < 2 || !F.freeT(x, y) || F.roomOf(x, y) != ri) continue;
        if (big && (x == 2 || x == W - 3)) continue;
        F.put(x, y, Prop::Bench, gid);
      }
      return true;
    });
  if (big)
    for (int y = 6; y <= H - 4; y += 4)
      for (int x : {2, W - 3}) F.group([&](int gid) { if (!pillarOk(F, x, y, ri)) return false; F.put(x, y, Prop::Pillar, gid); return true; });
  // statues and braziers in the front corners of the nave
  for (int x = 1; x <= W - 2; x++)
    if (F.roomOf(x, 2) == ri && F.wallT(x - 1, 2) != F.wallT(x + 1, 2) && std::abs(x - ex) > 3)
      F.group([&](int gid) { if (!F.freeT(x, 2) || !F.wallN(x, 2)) return false; F.put(x, 2, F.r.f() < 0.6f ? Prop::Statue : Prop::Brazier, gid); return true; });
  rugRect(F, ri, ex, 3, ex, H - 2, Deco::RugRed);
  cx.ownerX = ex + 1; cx.ownerY = 3;
}

void furnishVestry(Fit& F, int ri) {
  int dx = -1, dy = -1;
  if (northPiece(F, ri, Prop::Desk, 1, &dx, &dy))
    F.group([&](int gid) { if (!F.freeT(dx, dy + 1)) return false; F.put(dx, dy + 1, Prop::Stool, gid); return true; });
  northPiece(F, ri, F.r.f() < 0.5f ? Prop::Wardrobe : Prop::Cupboard, 0);
  northPiece(F, ri, Prop::Bookshelf, 0);
  if (!wallPiece(F, ri, Prop::Lectern)) northPiece(F, ri, Prop::Lectern, 0);
  wallPiece(F, ri, Prop::Chest);
  northPiece(F, ri, Prop::Candelabra, 0);
}

void furnishThrone(Fit& F, int ri, Ctx& cx) {
  const int ex = F.P.ex, W = F.W, H = F.H;
  // each lord's hall is dressed in its own way: colonnades or an open hall lined with candle-stands, one great table
  // or two side tables or a scatter of trestles, the dais carpet and the runner in the house's colours
  const int style = F.r.irange(3);           // 0 colonnade, 1 double colonnade near the walls, 2 open hall
  const int tables = F.r.irange(3);          // 0 two side tables, 1 one great table, 2 small trestles
  static const Deco runners[3] = {Deco::RugRed, Deco::RugBlue, Deco::RugGreen};
  const Deco runner = runners[F.r.irange(3)];
  for (int y = 3; y <= H - 2; y++) F.setLane(ex, y);
  int bdx = 2 + F.r.irange(2);
  F.group([&](int gid) {
    F.put(ex - 1, 2, Prop::Filler, gid); F.put(ex, 2, Prop::Throne, gid); F.put(ex + 1, 2, Prop::Filler, gid);
    for (int s : {-bdx, bdx}) if (F.freeT(ex + s, 2) && F.roomOf(ex + s, 2) == ri) F.put(ex + s, 2, Prop::Banner, gid);
    return true;
  });
  int dw = F.r.f() < 0.5f ? 1 : 2, dd = 3 + F.r.irange(2);
  rugRect(F, ri, ex - dw, 3, ex + dw, dd, F.r.f() < 0.7f ? Deco::RugGold : runner);
  rugRect(F, ri, ex, dd + 1, ex, H - 2, runner);
  // columns
  if (style != 2) {
    int pdx = style == 0 ? 3 + F.r.irange(2) : std::max(4, (W - 1) / 2 - 3 - F.r.irange(2));
    int step = 2 + F.r.irange(2), y0 = 5 + F.r.irange(2);
    for (int y = y0; y <= H - 4; y += step)
      for (int s : {-pdx, pdx})
        F.group([&](int gid) { int x = ex + s; if (!pillarOk(F, x, y, ri)) return false; F.put(x, y, Prop::Pillar, gid); return true; });
  } else {   // no columns: tall candle-stands along the runner
    for (int y = 6 + F.r.irange(2); y <= H - 4; y += 3)
      for (int s : {-2, 2})
        F.group([&](int gid) { int x = ex + s; if (!F.freeT(x, y) || F.roomOf(x, y) != ri) return false; F.put(x, y, Prop::Candelabra, gid); return true; });
  }
  // the feasting tables
  auto tableAt = [&](int side, int len, int seats, bool benches) {
    std::vector<std::pair<int, int>> c;
    F.tiles(ri, [&](int x, int y) { if (y >= 6 && (side < 0 ? x + len - 1 < ex - 2 : (side > 0 ? x > ex + 2 : true))) c.push_back({x, y}); });
    F.shuffle(c);
    for (auto& t : c)
      if (F.group([&](int gid) { return tableSet(F, ri, t.first, t.second, len, Prop::TableMeal, seats, benches, gid); })) return true;
    return false;
  };
  if (tables == 0) { tableAt(-1, 3, 0, true); tableAt(1, 3, 0, true); }
  else if (tables == 1) { int sd = F.r.f() < 0.5f ? -1 : 1; if (!tableAt(sd, 4 + F.r.irange(2), 0, true)) tableAt(sd, 3, 0, true); tableAt(-sd, 1, 3, false); }
  else {
    // fix round 3: trestles of different lengths, some with benches, some with chairs: not four copies of one set
    int n = 3 + F.r.irange(2);
    for (int k = 0; k < n; k++) {
      int len = 1 + F.r.irange(3);
      bool bn = len >= 2 && F.r.f() < 0.5f;
      tableAt(k % 2 ? 1 : -1, len, bn ? 0 : 2 + F.r.irange(3), bn);
    }
  }
  // fire: braziers flanking the dais, and more down the hall
  int bx = 4 + F.r.irange(3);
  for (int k = 0; k < 4; k++)
    F.group([&](int gid) {
      int x = k % 2 ? ex - bx : ex + bx, y = k < 2 ? 4 + F.r.irange(2) : H - 3 - F.r.irange(2);
      if (!F.freeT(x, y) || F.roomOf(x, y) != ri || !F.gapOk(x, y, x, y, gid)) return false;
      F.put(x, y, Prop::Brazier, gid);
      return true;
    });
  // trophies of the house in the back corners
  if (F.r.f() < 0.6f) northPiece(F, ri, F.r.f() < 0.5f ? Prop::Statue : Prop::WeaponRack, 1);
  if (F.r.f() < 0.4f) northPiece(F, ri, Prop::Statue, 1);
  // fix round 2: a lived-in hall, not a bare floor: the household's things along the walls (benches and chests for
  // petitioners and plate, racks and cupboards, a sideboard), a rug under each feasting table, plants by the doors;
  // how much depends on the hall's size
  int area = 0;
  F.tiles(ri, [&](int, int) { area++; });
  {
    static const Prop backs[6] = {Prop::WeaponRack, Prop::Cupboard, Prop::Banner, Prop::Bookshelf, Prop::Statue, Prop::Banner};
    int n = 2 + area / 70;
    for (int k = 0; k < n; k++) northPiece(F, ri, backs[F.r.irange(6)], 0);
    static const Prop sides[6] = {Prop::Bench, Prop::Chest, Prop::Bench, Prop::Barrel, Prop::PlantPot, Prop::Crate};
    int m2 = 3 + area / 50;
    // (a king's or a jarl's hall keeps its stores in the cellar and wings: no barrels or crates beside the throne;
    //  M1 round 3: the keep's hall too, not only the palace's)
    static const Prop royal[6] = {Prop::Bench, Prop::Chest, Prop::Bench, Prop::Statue, Prop::PlantPot, Prop::Candelabra};
    const Prop* sideSet = F.P.type == Building::Palace || F.P.type == Building::Keep ? royal : sides;
    for (int k = 0; k < m2; k++) wallPiece(F, ri, sideSet[F.r.irange(6)]);
    // a side table with its chairs where the floor is still open
    if (area >= 120) tableAt(F.r.f() < 0.5f ? -1 : 1, 1, 3, false);
    // rugs under the feasting tables
    F.tiles(ri, [&](int x, int y) {
      if (F.m.propAt(x, y) == (int)Prop::TableL + 1 && F.m.decoAt(x, y) == 0) {
        int x1 = x;
        while (F.m.propAt(x1 + 1, y) == (int)Prop::TableM + 1 || F.m.propAt(x1 + 1, y) == (int)Prop::TableR + 1) x1++;
        if (F.r.f() < 0.6f) rugRect(F, ri, x - 1, y - 1, x1 + 1, y + 1, runner);
      }
    });
  }
  cx.ownerX = ex; cx.ownerY = 3;
  // the guards: flanking the dais, or one at the dais and one by the gate
  if (F.r.f() < 0.5f) { cx.seats.push_back({ex - 2, 4}); cx.seats.push_back({ex + 2, 4}); }
  else { int sd = F.r.f() < 0.5f ? -1 : 1; cx.seats.push_back({ex + 2 * sd, 4}); cx.seats.push_back({ex - sd, H - 3}); }
}

// the palace's council chamber (M1): the long table in the middle with chairs all round, the realm's banners and
// candle-stands along the walls, books and chests of charters
void furnishCouncil(Fit& F, int ri) {
  const IRect& R = F.g.rooms[(size_t)ri].r;
  int tx = -1, ty = -1, len = 0;
  for (int l = std::clamp(R.w - 4, 2, 5); l >= 2 && tx < 0; l--) {
    int x0 = R.cx() - l / 2, y0 = R.y + R.h / 2;
    if (F.group([&](int gid) { return tableSet(F, ri, x0, y0, l, Prop::TableMeal, l * 2 + 2, false, gid); })) { tx = x0; ty = y0; len = l; }
    else if (tableIn(F, ri, l, Prop::TableMeal, l * 2, false, 60, &tx, &ty)) len = l;
  }
  if (tx >= 0) rugRect(F, ri, tx - 1, ty - 1, tx + len, ty + 1, F.r.f() < 0.6f ? Deco::RugGold : Deco::RugRed);
  northPiece(F, ri, Prop::Banner, 1);
  northPiece(F, ri, Prop::Bookshelf, 0);
  northPiece(F, ri, Prop::Banner, 1);
  northPiece(F, ri, Prop::Candelabra, 0);
  wallPiece(F, ri, Prop::Chest);
  if (R.w * R.h >= 30) { wallPiece(F, ri, Prop::Candelabra); wallPiece(F, ri, Prop::PlantPot); }
}

// ---- pass 3: wall decor on every visible face of the room (the back wall and partition face rows) -----------------
void wallDecorRoom(Fit& F, int ri, RoomKind kind) {
  std::vector<Weighted> pool;
  auto P = [](Prop p) { return (int)p; };
  const Bldg& b = *F.P.b;
  switch (kind) {
    case RoomKind::Kitchen: pool = {{P(Prop::PanRack), 4}, {P(Prop::HerbBundle), 4}, {P(Prop::WallShelf), 3}}; break;
    case RoomKind::Bedroom: case RoomKind::GuestRoom: pool = {{P(Prop::Painting), 2}, {P(Prop::Wreath), 1}, {P(Prop::WallShelf), 1}, {P(Prop::Tapestry), F.P.wealth >= 2 ? 2 : 0}}; break;
    case RoomKind::OwnerRoom: pool = {{P(Prop::Painting), 2}, {P(Prop::Tapestry), 2}, {P(Prop::WallShelf), 1}, {P(Prop::WallShield), b.type == Building::Keep || b.type == Building::Palace ? 2 : 0}}; break;
    case RoomKind::Common: pool = {{P(Prop::Antlers), 3}, {P(Prop::WallShield), 2}, {P(Prop::WallShelf), 2}, {P(Prop::HerbBundle), 1}, {P(Prop::Painting), 1}, {P(Prop::PanRack), 1}}; break;
    case RoomKind::Storeroom: case RoomKind::Stockroom: pool = {{P(Prop::WallShelf), 3}, {P(Prop::HerbBundle), 2}, {P(Prop::ToolRack), 1}}; break;
    case RoomKind::Barn: pool = {{P(Prop::ToolRack), 3}, {P(Prop::HerbBundle), 2}, {P(Prop::Antlers), 1}}; break;
    case RoomKind::Shopfloor: pool = {{P(Prop::WallShelf), 4}, {P(Prop::HerbBundle), 2}, {P(Prop::Painting), 1}, {P(Prop::Wreath), 1}}; break;
    case RoomKind::Forge: pool = {{P(Prop::ToolRack), 4}, {P(Prop::WallShield), 3}}; break;
    case RoomKind::Workshop: pool = {{P(Prop::WallShelf), 4}, {P(Prop::HerbBundle), 3}, {P(Prop::Painting), 1}}; break;
    case RoomKind::Study: pool = {{P(Prop::WallShelf), 3}, {P(Prop::Painting), 2}, {P(Prop::Tapestry), 2}}; break;
    case RoomKind::Nave: pool = {{P(Prop::Tapestry), 5}, {P(Prop::Wreath), 1}}; break;
    case RoomKind::Vestry: pool = {{P(Prop::Tapestry), 2}, {P(Prop::WallShelf), 2}, {P(Prop::Wreath), 1}}; break;
    case RoomKind::ThroneHall: pool = {{P(Prop::Tapestry), 4}, {P(Prop::WallShield), 3}, {P(Prop::Antlers), 2}}; break;
    case RoomKind::Barracks: pool = {{P(Prop::WallShield), 4}, {P(Prop::ToolRack), 1}}; break;
    case RoomKind::Corridor: pool = {{P(Prop::Painting), 2}, {P(Prop::Tapestry), b.type == Building::Keep || b.type == Building::Palace ? 3 : 1}, {P(Prop::Wreath), 1}}; break;
    case RoomKind::Council: pool = {{P(Prop::Tapestry), 4}, {P(Prop::WallShield), 2}, {P(Prop::Painting), 2}}; break;
    default:
      pool = {{P(Prop::HerbBundle), 3}, {P(Prop::WallShelf), 3}, {P(Prop::Wreath), 2}, {P(Prop::PanRack), 2}, {P(Prop::Painting), F.P.wealth >= 1 ? 2 : 0},
              {P(Prop::Antlers), b.owner == Role::Farmer || F.biome == Biome::Taiga || F.biome == Biome::Snow ? 3 : 1}, {P(Prop::Tapestry), F.P.wealth >= 2 ? 2 : 0}};
      break;
  }
  // the wall tiles this room sees: a wall with the room's floor right below, a horizontal run (not a doorway's edge),
  // not hidden behind tall furniture, not over the stairs
  std::vector<int> faces;
  F.tiles(ri, [&](int x, int y) {
    if (!F.wallT(x, y - 1) || !F.wallT(x, y - 2) || y - 1 < 1) return;
    if (!F.wallT(x - 1, y - 1) || !F.wallT(x + 1, y - 1)) return;
    if (F.m.propAt(x, y - 1)) return;
    int p = F.m.propAt(x, y);
    if (p && tallProp((Prop)(p - 1))) return;
    if (p == (int)Prop::StairsUp + 1) return;
    // a partition's short face has no room above a headboard: nothing hangs over a bed there or crowds it
    if (y - 1 >= 2)
      for (int o = -1; o <= 1; o++) {
        int q = F.m.propAt(x + o, y);
        if (q == (int)Prop::Bed + 1 || q == (int)Prop::BunkBed + 1 || (q == (int)Prop::Filler + 1 && F.m.propAt(x + o, y + 1) == (int)Prop::Bed + 1)) return;
      }
    faces.push_back(F.I(x, y - 1));
  });
  std::sort(faces.begin(), faces.end());
  auto put = [&](int t, Prop p) { F.m.prop[(size_t)t] = (uint8_t)((int)p + 1); };
  auto isFree = [&](int t) { return F.m.prop[(size_t)t] == 0; };
  bool outerWindows = kind != RoomKind::Forge && kind != RoomKind::Nave && kind != RoomKind::ThroneHall && kind != RoomKind::Storeroom && kind != RoomKind::Barn;
  if (kind == RoomKind::Nave && F.m.propAt(F.P.ex, 1) == 0 && F.roomOf(F.P.ex, 2) == ri) put(F.I(F.P.ex, 1), Prop::HolySymbol);   // the sun disc above the altar
  // windows on the outer back wall: one or two, never side by side
  if (outerWindows) {
    int want = faces.size() >= 9 ? 2 : 1;
    std::vector<int> c;
    for (int t : faces) if (t / F.W == 1 && isFree(t)) c.push_back(t);
    F.shuffle(c);
    for (int t : c) {
      if (want <= 0) break;
      bool ok = true;
      for (int o : {-1, 1}) { int u = t + o; if (F.m.prop[(size_t)u] == (int)Prop::Window + 1) ok = false; }
      if (!ok) continue;
      put(t, Prop::Window);
      want--;
    }
  }
  // sconces spaced for light, then decor in the gaps
  int lastS = -99, lastRow = -1;
  int gap = kind == RoomKind::ThroneHall || kind == RoomKind::Nave ? 4 : (kind == RoomKind::Corridor ? 3 : 5);
  for (int t : faces) {
    int x = t % F.W, y = t / F.W;
    if (y != lastRow) { lastS = -99; lastRow = y; }
    if (x - lastS < gap || !isFree(t)) continue;
    if (faces.size() < 3 && kind != RoomKind::Corridor) continue;
    put(t, Prop::Sconce);
    lastS = x;
  }
  float density = (b.type == Building::Hut ? 0.35f : 0.5f + F.P.wealth * 0.08f) * (F.biome == Biome::Desert ? 0.7f : 1.0f);
  int lastD = -99;
  lastRow = -1;
  for (int t : faces) {
    int x = t % F.W, y = t / F.W;
    if (y != lastRow) { lastD = -99; lastRow = y; }
    if (!isFree(t) || x - lastD < 2 || F.r.f() > density) continue;
    bool nb = false;
    for (int o : {-1, 1}) if (F.m.prop[(size_t)(t + o)] && F.m.prop[(size_t)(t + o)] != (int)Prop::Sconce + 1) nb = true;
    if (nb) continue;
    put(t, (Prop)pickW(F.r, pool));
    lastD = x;
  }
}

// ---- pass 4: small clutter on Map::deco -----------------------------------------------------------------------------
void clutterRoom(Fit& F, int ri, RoomKind kind) {
  // (M1 round 3) a throne hall is kept swept: no sacks, jugs or kindling on its floor (they gathered by the braziers)
  if (kind == RoomKind::ThroneHall) return;
  std::vector<Weighted> pool;
  auto D = [](Deco d) { return (int)d; };
  const Bldg& b = *F.P.b;
  float density = 0.3f;
  switch (kind) {
    case RoomKind::Kitchen: pool = {{D(Deco::PotsPans), 4}, {D(Deco::Bread), 3}, {D(Deco::Sacks), 3}, {D(Deco::Basket), 2}, {D(Deco::Cheese), 2}, {D(Deco::Bucket), 2}, {D(Deco::Kindling), 2}, {D(Deco::FruitBowl), 1}, {D(Deco::Jugs), 1}}; density = 0.42f; break;
    case RoomKind::Bedroom: case RoomKind::GuestRoom: case RoomKind::OwnerRoom: case RoomKind::Barracks:
      if (F.P.wealth >= 3 && kind != RoomKind::Barracks)   // (M1) royal and noble chambers: books and scrolls, no baskets
        pool = {{D(Deco::Books), 3}, {D(Deco::Scrolls), 2}, {D(Deco::Laundry), 1}};
      else
        pool = {{D(Deco::Laundry), 3}, {D(Deco::Books), F.P.wealth >= 1 ? 2 : 0}, {D(Deco::Basket), 2}, {D(Deco::Jugs), 1}, {D(Deco::Bucket), kind == RoomKind::GuestRoom ? 1 : 0}};
      density = 0.3f;
      break;
    case RoomKind::Common: pool = {{D(Deco::Jugs), 3}, {D(Deco::Bottles), 2}, {D(Deco::Sacks), 1}, {D(Deco::Kindling), 2}, {D(Deco::Broom), 1}, {D(Deco::Bucket), 1}}; density = 0.22f; break;
    case RoomKind::Storeroom: case RoomKind::Stockroom: pool = {{D(Deco::Sacks), 4}, {D(Deco::Basket), 3}, {D(Deco::Straw), 1}, {D(Deco::Kindling), 1}, {D(Deco::Broom), 1}}; density = 0.5f; break;
    case RoomKind::Barn: pool = {{D(Deco::Straw), 5}, {D(Deco::Sacks), 3}, {D(Deco::Basket), 2}, {D(Deco::Tools), 2}, {D(Deco::Bucket), 1}}; density = 0.55f; break;
    case RoomKind::Shopfloor: pool = {{D(Deco::Basket), 3}, {D(Deco::Sacks), 2}, {D(Deco::FruitBowl), 1}, {D(Deco::Bottles), 1}, {D(Deco::Cheese), 1}}; break;
    case RoomKind::Forge: pool = {{D(Deco::Tools), 5}, {D(Deco::Kindling), 3}, {D(Deco::Bucket), 2}, {D(Deco::Broom), 1}}; density = 0.35f; break;
    // (no loose candles on the floor anywhere: a cluster of them read as a golden hand at 1x; candles stand on
    // nightstands, candelabras and in sconces)
    case RoomKind::Workshop:
      if (F.P.type == Building::Tower) pool = {{D(Deco::Bottles), 3}, {D(Deco::Books), 2}, {D(Deco::Scrolls), 2}, {D(Deco::Basket), 1}};
      else pool = {{D(Deco::Tools), 3}, {D(Deco::Sacks), 3}, {D(Deco::Basket), 2}, {D(Deco::Straw), 2}, {D(Deco::Kindling), 1}};   // (M1 economy) a trade's floor
      density = 0.35f;
      break;
    case RoomKind::Study: pool = {{D(Deco::Books), 4}, {D(Deco::Scrolls), 3}}; density = 0.35f; break;
    case RoomKind::Council: pool = {{D(Deco::Scrolls), 3}, {D(Deco::Books), 2}}; density = 0.12f; break;
    case RoomKind::Nave: pool = {{D(Deco::Books), 1}, {D(Deco::Basket), 1}}; density = 0.05f; break;
    case RoomKind::Vestry: pool = {{D(Deco::Books), 3}, {D(Deco::Scrolls), 2}, {D(Deco::Basket), 1}}; density = 0.35f; break;
    case RoomKind::ThroneHall: pool = {{D(Deco::Jugs), 1}, {D(Deco::Kindling), 1}}; density = 0.04f; break;
    case RoomKind::Corridor: pool = {{D(Deco::Basket), 1}, {D(Deco::Laundry), 1}, {D(Deco::Broom), 1}}; density = 0.08f; break;
    default:
      pool = {{D(Deco::Basket), 3}, {D(Deco::PotsPans), 2}, {D(Deco::Laundry), 2}, {D(Deco::Jugs), 2}, {D(Deco::Bread), 1}, {D(Deco::Bucket), 2}, {D(Deco::Broom), 2},
              {D(Deco::Kindling), 2}, {D(Deco::FruitBowl), 1}, {D(Deco::Cheese), 1}, {D(Deco::Books), F.P.wealth >= 2 ? 1 : 0}};
      if (b.owner == Role::Farmer || b.type == Building::Farmhouse) { pool.push_back({D(Deco::Sacks), 4}); pool.push_back({D(Deco::Straw), 2}); }
      if (b.owner == Role::Mage) { pool.push_back({D(Deco::Books), 4}); pool.push_back({D(Deco::Scrolls), 3}); }
      if (b.owner == Role::Smith) pool.push_back({D(Deco::Tools), 4});
      break;
  }
  if (F.biome == Biome::Swamp) pool.push_back({D(Deco::Basket), 4});
  if (F.biome == Biome::Snow || F.biome == Biome::Taiga) pool.push_back({D(Deco::Kindling), 3});
  if (F.biome == Biome::Desert) density *= 0.6f;
  if (b.type == Building::Hut) density *= 0.8f;
  // Clutter gathers where things are kept and used: against a wall beside storage, a hearth, a counter or a bed, and
  // in corners; a few small groups per room, never strewn over open floor, never the same piece over and over.
  int area = 0;
  F.tiles(ri, [&](int, int) { area++; });
  int budget = std::min(10, (int)std::lround(area * density * 0.25f) + 1);
  const bool store = kind == RoomKind::Storeroom || kind == RoomKind::Stockroom || kind == RoomKind::Barn;
  auto anchor = [](int p) {
    if (!p) return false;
    switch ((Prop)(p - 1)) {
      case Prop::Barrel: case Prop::Barrel2: case Prop::Crate: case Prop::Woodpile: case Prop::Haystack: case Prop::Shelf:
      case Prop::Cupboard: case Prop::Hearth: case Prop::Oven: case Prop::PrepTable: case Prop::Workbench: case Prop::Bookshelf:
      case Prop::Desk: case Prop::Forge: case Prop::Anvil: case Prop::QuenchTub: case Prop::CounterL: case Prop::CounterM:
      case Prop::CounterR: case Prop::BottleShelf: case Prop::Wardrobe: case Prop::Dresser: case Prop::Chest: case Prop::Bed:
      case Prop::BunkBed: case Prop::Nightstand: case Prop::Washstand: case Prop::Lectern: case Prop::Cauldron: case Prop::Loom:
      case Prop::SpinningWheel: case Prop::Filler: case Prop::WeaponRack: case Prop::Grindstone: case Prop::TableWork:
        return true;
      default: return false;
    }
  };
  struct Spot { int x, y, score; };
  std::vector<Spot> spots;
  static const int dx4[4] = {1, -1, 0, 0}, dy4[4] = {0, 0, 1, -1};
  auto spotScore = [&](int x, int y) {
    if (!F.freeT(x, y) || F.m.decoAt(x, y)) return -1;
    if (std::find(F.taken.begin(), F.taken.end(), F.I(x, y)) != F.taken.end()) return -1;
    // (fix round 3) never on the tile right above a piece: its sprite rises over that tile and the basket would sit
    // on top of the plant pot or the barrel
    if (int q = F.m.propAt(x, y + 1); q && q != (int)Prop::Filler + 1) return -1;
    int walls = 0, near = 0;
    for (int k = 0; k < 4; k++) {
      walls += F.wallT(x + dx4[k], y + dy4[k]);
      if (anchor(F.m.propAt(x + dx4[k], y + dy4[k]))) near++;
    }
    if (!walls && !(near && store)) return -1;
    return (near ? 3 : 0) + (walls >= 2 ? 2 : 0);
  };
  F.tiles(ri, [&](int x, int y) { int sc = spotScore(x, y); if (sc >= 0) spots.push_back({x, y, sc}); });
  F.shuffle(spots);
  std::stable_sort(spots.begin(), spots.end(), [](const Spot& a, const Spot& c) { return a.score > c.score; });
  std::vector<int> count((size_t)Deco::Kindling + 1, 0);
  auto pick = [&](Deco avoid) {
    for (int t = 0; t < 6; t++) {
      Deco d = (Deco)pickW(F.r, pool);
      int cap = store && (d == Deco::Sacks || d == Deco::Straw || d == Deco::Basket) ? 3 : 2;
      if (d != avoid && count[(size_t)d] < cap) return d;
    }
    return Deco::None;
  };
  auto crowded = [&](int x, int y) {
    for (int oy = -1; oy <= 1; oy++)
      for (int ox = -1; ox <= 1; ox++) {
        int d = F.m.decoAt(x + ox, y + oy);
        if ((ox || oy) && d >= (int)Deco::Basket && d <= (int)Deco::Kindling) return true;
      }
    return false;
  };
  for (const Spot& sp : spots) {
    if (budget <= 0) break;
    if (sp.score == 0 && F.r.f() < 0.7f) continue;   // a bare stretch of wall: only now and then
    if (F.m.decoAt(sp.x, sp.y) || crowded(sp.x, sp.y)) continue;
    Deco d = pick(Deco::None);
    if (d == Deco::None) break;
    F.deco(sp.x, sp.y, d);
    count[(size_t)d]++;
    budget--;
    // a second piece beside the first makes a group (a sack by the basket, kindling by the bucket)
    if (budget > 0 && F.r.f() < 0.55f) {
      for (int k = 0; k < 4; k++) {
        int nx = sp.x + dx4[k], ny = sp.y + dy4[k];
        if (spotScore(nx, ny) < 0) continue;
        Deco d2 = pick(d);
        if (d2 == Deco::None) break;
        F.deco(nx, ny, d2);
        count[(size_t)d2]++;
        budget--;
        break;
      }
    }
  }
}

// a free reachable floor tile near (x, y) for a spawn, in room ri when possible, and (when avoidX >= 0) more than
// three tiles from (avoidX, avoidY): patrons keep clear of the innkeeper so talking to him reaches him
void spawnNear(Fit& F, Spawn& s, int x, int y, int ri, int avoidX = -1, int avoidY = -1) {
  F.bfs();
  int best = -1, bd = 1 << 30;
  for (int ty = 1; ty < F.H; ty++)
    for (int tx = 0; tx < F.W; tx++) {
      if (!F.reached(tx, ty) || F.solidT(tx, ty) || F.lane[(size_t)F.I(tx, ty)]) continue;
      if (std::find(F.taken.begin(), F.taken.end(), F.I(tx, ty)) != F.taken.end()) continue;
      if (avoidX >= 0 && std::abs(tx - avoidX) <= 3 && std::abs(ty - avoidY) <= 3) continue;
      int pr = F.m.propAt(tx, ty);
      int d = (tx - x) * (tx - x) + (ty - y) * (ty - y) + (pr ? 3 : 0) + (ri >= 0 && F.roomOf(tx, ty) != ri ? 40 : 0);
      if (d < bd) { bd = d; best = F.I(tx, ty); }
    }
  if (best < 0) {   // nothing off the lanes: any reachable free tile that is not stairs or a doorway
    for (int i = 0; i < F.W * F.H && best < 0; i++)
      if (F.dist[(size_t)i] > 1 && !F.solidT(i % F.W, i / F.W) && !F.g.isKeep(i) &&
          std::find(F.g.doorTiles.begin(), F.g.doorTiles.end(), i) == F.g.doorTiles.end() && i / F.W < F.H - 1)
        best = i;
  }
  if (best < 0) best = F.I(F.sx, F.sy);
  s.x = best % F.W; s.y = best / F.W;
  F.taken.push_back(best);
}

art::RoomStyle wallStyleOf(const Bldg& b, Rng& r) {
  switch (b.type) {
    case Building::Temple: case Building::Keep: case Building::Palace: return art::RoomStyle::Hall;
    case Building::Barracks: return art::RoomStyle::Stone;
    case Building::Tower: return art::RoomStyle::Arcane;
    default: break;
  }
  art::ArchStyle a = bldgArch(b);
  if (b.type == Building::Smithy || b.type == Building::Smelter) {   // fix round 2: the forge hall in the culture's own material, sooted where it is stone
    switch (a.wall) {
      case art::WallMat::Adobe: return art::RoomStyle::Adobe;
      case art::WallMat::Log: return art::RoomStyle::Log;
      case art::WallMat::Stone: case art::WallMat::Brick: return art::RoomStyle::Soot;
      case art::WallMat::Plaster: return r.f() < 0.5f ? art::RoomStyle::Timber : art::RoomStyle::Soot;
      default: return r.f() < 0.6f ? art::RoomStyle::Soot : art::RoomStyle::Timber;
    }
  }
  switch (a.wall) {
    case art::WallMat::Adobe: return art::RoomStyle::Adobe;
    case art::WallMat::Log: return art::RoomStyle::Log;
    case art::WallMat::Stone: case art::WallMat::Brick: return art::RoomStyle::Stone;
    case art::WallMat::Plaster: return art::RoomStyle::Plaster;
    default: break;
  }
  if (b.type == Building::StoneHouse) return art::RoomStyle::Stone;
  if (b.type == Building::Hut || b.type == Building::Farmhouse) return r.f() < 0.6f ? art::RoomStyle::Log : art::RoomStyle::Timber;
  if (b.type == Building::Inn || b.type == Building::Shop) return r.f() < 0.5f ? art::RoomStyle::Plaster : art::RoomStyle::Timber;
  return r.f() < 0.3f ? art::RoomStyle::Plaster : art::RoomStyle::Timber;
}

}  // namespace

void genInteriorRooms(Map& m, const Bldg& b, uint32_t seed, int floor) {
  Plan P = makePlan(b, seed);
  floor = std::clamp(floor, 0, P.floors - 1);
  const Geo& g = P.geo[(size_t)floor];
  Rng r((seed ^ 0x6C8E9CF5u) + (uint32_t)floor * 0x9E3779B9u);
  const int W = P.W, H = P.H;
  m.kind = MapKind::Interior;
  m.seed = seed;
  m.alloc(W, H, Ground::InteriorWall);
  m.floor = floor;
  // floors: stone in stone buildings, the desert's tiles, flagged kitchens; planks elsewhere
  bool stone = b.type == Building::Keep || b.type == Building::Temple || b.type == Building::Palace || b.type == Building::Barracks || b.type == Building::StoneHouse || b.type == Building::Tower ||
               b.type == Building::Smithy || b.type == Building::Smelter || b.type == Building::Windmill || b.type == Building::Watermill ||
               b.biome == Biome::Desert;
  Ground base = stone && !(floor > 0 && (b.type == Building::StoneHouse)) ? Ground::StoneFloor : Ground::WoodFloor;
  for (int y = 0; y < H; y++)
    for (int x = 0; x < W; x++) {
      if (g.isWall(x, y)) continue;
      int ri = g.roomOf(x, y);
      RoomKind k = ri >= 0 ? g.rooms[(size_t)ri].kind : RoomKind::Hall;
      m.setG(x, y, k == RoomKind::Kitchen ? Ground::StoneFloor : base);
    }
  if (floor == 0) {
    m.exitX = P.ex; m.exitY = H - 1;
    m.setG(P.ex, H - 1, m.at(P.ex, H - 2));
  } else {
    m.exitX = -1; m.exitY = -1;
  }
  // the room's look: the wall style on the back wall's face row
  Rng sr(seed ^ 0x1D2C3B4Au);
  art::RoomStyle rs = wallStyleOf(b, sr);
  for (int x = 1; x < W - 1; x++) m.deco[(size_t)(1 * W + x)] = (uint8_t)((int)Deco::WallTimber + (int)rs);
  // doors and stairs
  for (size_t i = 1; i < g.rooms.size(); i++) {
    const RoomDef& R = g.rooms[i];
    if (R.doorX >= 0) m.setProp(R.doorX, R.doorY, R.doorH ? Prop::DoorH : Prop::DoorV);
  }
  m.up = g.up;
  m.down = g.down;
  if (m.up.valid()) m.setProp(m.up.x, m.up.y, Prop::StairsUp);
  if (m.down.valid()) m.setProp(m.down.x, m.down.y, Prop::StairsDown);
  if (g.up2 >= 0) m.setProp(g.up2 % W, g.up2 / W, Prop::StairsUp);
  if (g.down2 >= 0) m.setProp(g.down2 % W, g.down2 / W, Prop::StairsDown);
  m.roomAt.assign((size_t)W * H, -1);
  for (int i = 0; i < W * H; i++) if (!groundSolid((Ground)m.ground[(size_t)i])) m.roomAt[(size_t)i] = g.room[(size_t)i] >= 0 ? g.room[(size_t)i] : 0;

  // furnish
  Plan& PP = P;
  Geo& gg = PP.geo[(size_t)floor];
  Fit F(m, r, PP, gg);
  F.biome = b.biome;
  if (floor == 0) { F.sx = m.exitX; F.sy = m.exitY; } else { F.sx = m.down.ax; F.sy = m.down.ay; }
  for (int t : gg.doorTiles) F.lane[(size_t)t] = 1;
  for (int t : gg.approach) F.lane[(size_t)t] = 1;
  for (int t : gg.keep) F.lane[(size_t)t] = 1;
  if (m.up.valid()) {   // and the tile in front of the arrival, so the way onto the stairs stays open
    int fx = m.up.ax + (m.up.ax - m.up.x), fy = m.up.ay + (m.up.ay - m.up.y);
    if (gg.isFloor(fx, fy) && gg.roomOf(fx, fy) == gg.roomOf(m.up.x, m.up.y)) F.lane[(size_t)gg.I(fx, fy)] = 1;
  }
  if (floor == 0) { F.setLane(P.ex, H - 1); F.setLane(P.ex, H - 2); F.setLane(P.ex, H - 3); }
  // one-tile passages (a stair nook, the gap beside a partition) stay clear
  for (int y = 2; y < H - 1; y++)
    for (int x = 1; x < W - 1; x++)
      if (gg.isFloor(x, y) && ((gg.isWall(x - 1, y) && gg.isWall(x + 1, y)) || (gg.isWall(x, y - 1) && gg.isWall(x, y + 1)))) F.setLane(x, y);
  Ctx cx;
  // order: the rooms whose anchors decide the rest first (kitchens before halls: the hall skips its hearth then)
  std::vector<int> order;
  for (int i = 0; i < (int)gg.rooms.size(); i++) order.push_back(i);
  auto prio = [&](int i) {
    RoomKind k = gg.rooms[(size_t)i].kind;
    if (k == RoomKind::Kitchen) return 0;
    if (k == RoomKind::Common || k == RoomKind::Shopfloor || k == RoomKind::Forge || k == RoomKind::Nave || k == RoomKind::ThroneHall) return 1;
    return 2;
  };
  std::stable_sort(order.begin(), order.end(), [&](int a, int c) { return prio(a) < prio(c); });
  bool childDone = false;
  for (int ri : order) {
    RoomKind k = gg.rooms[(size_t)ri].kind;
    switch (k) {
      case RoomKind::Bedroom: case RoomKind::GuestRoom: case RoomKind::OwnerRoom: case RoomKind::Barracks: {
        // in a two-storey home the smaller bedroom upstairs is the children's
        bool child = false;
        if (k == RoomKind::Bedroom && floor > 0 && (b.type == Building::House || b.type == Building::StoneHouse) && !childDone) {
          int big = -1;
          for (size_t i = 1; i < gg.rooms.size(); i++)
            if (gg.rooms[i].kind == RoomKind::Bedroom && (big < 0 || gg.rooms[i].r.w > gg.rooms[(size_t)big].r.w)) big = (int)i;
          child = ri != big;
          childDone = child;
        }
        furnishBedroom(F, ri, k, child, cx);
        break;
      }
      case RoomKind::Kitchen: furnishKitchen(F, ri, cx); break;
      case RoomKind::Hall: case RoomKind::Cottage: furnishHallish(F, ri, k, cx); break;
      case RoomKind::Storeroom: case RoomKind::Stockroom: case RoomKind::Barn: furnishStore(F, ri, k); break;
      case RoomKind::Study: furnishStudy(F, ri); break;
      case RoomKind::Workshop: furnishWorkshop(F, ri, cx); break;
      case RoomKind::Corridor: furnishCorridor(F, ri); break;
      case RoomKind::Common: furnishCommon(F, ri, cx); break;
      case RoomKind::Shopfloor: furnishShopfloor(F, ri, cx); break;
      case RoomKind::Forge: furnishForge(F, ri, cx); break;
      case RoomKind::Nave: furnishNave(F, ri, cx); break;
      case RoomKind::Vestry: furnishVestry(F, ri); break;
      case RoomKind::ThroneHall: furnishThrone(F, ri, cx); break;
      case RoomKind::Council: furnishCouncil(F, ri); break;
      default: break;
    }
  }
  for (int ri : order) wallDecorRoom(F, ri, gg.rooms[(size_t)ri].kind);
  rugCleanup(F);

  // spawns (slots 16*floor+k): the owner at their post on the ground floor, inn patrons at the tables, the jarl's guards
  if (floor == 0) {
    Rng orr(seed ^ 0x0A11CE5u);
    bool ownerHome = b.owner != Role::Villager || orr.f() < 0.75f;
    int postX = -1, postY = -1;
    if (ownerHome) {
      Spawn o; o.npc = true; o.role = b.owner; o.slot = 0; o.site = b.site;
      int ox = cx.ownerX >= 0 ? cx.ownerX : W / 2, oy = cx.ownerX >= 0 ? cx.ownerY : H / 2;
      spawnNear(F, o, ox, oy, F.g.roomOf(ox, oy));
      m.spawns.push_back(o);
      postX = o.x; postY = o.y;
    }
    if (b.type == Building::Inn) {
      int n = 2 + orr.irange(2);
      for (int k = 1; k <= n; k++) {
        Spawn p; p.npc = true; p.role = Role::Villager; p.slot = k; p.site = b.site;
        auto at = k - 1 < (int)cx.seats.size() ? cx.seats[(size_t)k - 1] : std::pair<int, int>{W / 2 + k, H - 4};
        spawnNear(F, p, at.first, at.second, 0, postX, postY);
        m.spawns.push_back(p);
      }
    }
    if (b.type == Building::Keep || b.type == Building::Palace)
      for (int k = 1; k <= (b.type == Building::Palace ? 4 : 2); k++) {
        Spawn gd; gd.npc = true; gd.role = Role::Guard; gd.slot = k; gd.site = b.site;
        // the palace: two at the dais, two by the doors
        auto at = k - 1 < (int)cx.seats.size() ? cx.seats[(size_t)k - 1] : std::pair<int, int>{P.ex + (k % 2 ? -2 : 2), k <= 2 ? 4 : P.H - 3};
        spawnNear(F, gd, at.first, at.second, 0);
        m.spawns.push_back(gd);
      }
    if (b.type == Building::Barracks)   // soldiers off duty in the mess hall
      for (int k = 1; k <= 2; k++) {
        Spawn gd; gd.npc = true; gd.role = Role::Guard; gd.slot = k; gd.site = b.site;
        spawnNear(F, gd, P.W / 2 + (k == 1 ? -3 : 3), P.H / 2, 0);
        m.spawns.push_back(gd);
      }
  } else {
    // upper floors are lived in too: the household, the inn's guests, the keep's servants and guards. (The game leaves
    // out anyone who would stand in the room you rented, and moves a tower's mage up to bed at night.)
    Rng ur(seed ^ 0x5EED0F1u ^ (uint32_t)floor * 0x01000193u);
    int slot = 16 * floor + 1;
    auto add = [&](Role role, int x, int y, int ri) {
      Spawn s; s.npc = true; s.role = role; s.slot = slot++; s.site = b.site;
      spawnNear(F, s, x, y, ri);
      m.spawns.push_back(s);
    };
    auto rooms = [&](RoomKind k) { std::vector<int> v; for (int i = 1; i < (int)gg.rooms.size(); i++) if (gg.rooms[(size_t)i].kind == k) v.push_back(i); return v; };
    auto atBed = [&](Role role, int ri) {   // in their room, a step away from the bed (not in front of it)
      const RoomDef& R = gg.rooms[(size_t)ri];
      int x = R.r.cx(), y = R.r.y + R.r.h - 1;
      if (R.bedX >= 0 && std::abs(x - R.bedX) < 1) x = R.bedX + (R.bedX - R.r.x < R.r.x + R.r.w - 1 - R.bedX ? 1 : -1);
      add(role, x, y, ri);
    };
    const IRect& G = gg.rooms[0].r;
    if (b.type == Building::Barracks) {
      for (int ri : rooms(RoomKind::Barracks)) if (ur.f() < 0.7f) atBed(Role::Guard, ri);   // a guard asleep off his watch
    } else if (b.type == Building::Keep || b.type == Building::Palace) {
      add(Role::Villager, G.x + ur.irange(G.w), G.y + ur.irange(G.h), 0);   // a servant about the gallery
      for (int ri : rooms(RoomKind::OwnerRoom)) {                            // a guard at the lord's door
        const RoomDef& R = gg.rooms[(size_t)ri];
        if (R.doorX >= 0) add(Role::Guard, R.doorX + (R.doorH ? 1 : 0), R.doorY + (R.doorH ? 1 : 0), 0);
      }
      std::vector<int> beds = rooms(RoomKind::Bedroom);
      if (!beds.empty() && ur.f() < 0.6f) atBed(Role::Villager, beds[(size_t)ur.irange((int)beds.size())]);
    } else if (b.type == Building::Inn) {
      std::vector<int> g = rooms(RoomKind::GuestRoom);
      F.shuffle(g);
      int n = std::min((int)g.size() - 1, 1 + ur.irange(2));
      for (int k = 0; k < n; k++) atBed(Role::Villager, g[(size_t)k]);
      if (ur.f() < 0.6f) add(Role::Villager, G.x + ur.irange(G.w), G.y + ur.irange(G.h), 0);   // the maid on her rounds
    } else if (b.type == Building::House || b.type == Building::StoneHouse || b.type == Building::Shop) {
      std::vector<int> beds = rooms(RoomKind::Bedroom);
      if (!beds.empty() && ur.f() < 0.7f) atBed(b.type == Building::Shop || ur.f() < 0.4f ? Role::Villager : Role::Child, beds[(size_t)ur.irange((int)beds.size())]);
    } else if (b.type == Building::Tower && floor < P.floors - 1 && ur.f() < 0.7f) {
      add(Role::Villager, G.cx(), G.cy(), 0);   // an apprentice at the books
    }
  }
  for (int ri : order) clutterRoom(F, ri, gg.rooms[(size_t)ri].kind);

  // the rooms, as the game and the tests read them
  m.rooms.clear();
  for (const RoomDef& R : gg.rooms) {
    RoomInfo ri;
    ri.kind = R.kind;
    ri.r = R.r;
    ri.doorX = R.doorX; ri.doorY = R.doorY;
    ri.bedX = R.bedX; ri.bedY = R.bedY;
    ri.guest = R.guest;
    m.rooms.push_back(ri);
  }
  m.rebuildSolid();
}

uint64_t interiorLayoutSignature(const Map& m) {
  uint64_t h = 1469598103934665603ull;
  auto mx = [&](uint64_t v) { h ^= v; h *= 1099511628211ull; };
  mx((uint64_t)m.w); mx((uint64_t)m.h);
  for (int y = 0; y < m.h; y++)
    for (int x = 0; x < m.w; x++) {
      int ri = m.roomIndexAt(x, y);
      uint64_t v = m.at(x, y) == Ground::InteriorWall ? 1 : 2;
      int p = m.propAt(x, y);
      if (p == (int)Prop::DoorH + 1 || p == (int)Prop::DoorV + 1 || p == (int)Prop::StairsUp + 1 || p == (int)Prop::StairsDown + 1) v |= (uint64_t)p << 4;
      if (ri >= 0 && ri < (int)m.rooms.size()) v |= (uint64_t)((int)m.rooms[(size_t)ri].kind + 1) << 16;
      mx(v);
    }
  return h;
}
