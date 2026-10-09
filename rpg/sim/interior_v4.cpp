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
#include <cstdio>
#include <cstdlib>
#include <chrono>
#include <initializer_list>
#include <memory>
#include <utility>
#include <vector>
#include "rpg/build/blueprint.h"
#include "rpg/culture/culture.h"
#include "rpg/culture/society.h"
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
  uint8_t floorStyle = 0;       // M3b: RoomInfo::floorStyle (0 the room style's floor, else art::FloorStyle + 1)
};

// M3b: the floor's tiles. FLOOR is walked on; PART an interior partition (rooms.h contract); SHELL the outer wall (the
// rectangle's border, or the ring round a round floor, the walls of an L's yard corner or a cross's arms); VOID lies
// beyond the shell (a round room's corners, an L's missing corner): nothing there, the view leaves it black.
enum : uint8_t { T_FLOOR = 0, T_PART = 1, T_SHELL = 2, T_VOID = 3 };

struct Geo {
  int W = 0, H = 0;
  std::vector<uint8_t> wall;     // T_FLOOR (0) or a wall: T_PART, T_SHELL, T_VOID
  std::vector<int8_t> room;      // room index per floor tile, -1 for walls
  std::vector<RoomDef> rooms;    // rooms[0] is the main room: what is left after carving
  std::vector<int> doorTiles;    // every doorway tile (an E-W doorway has two)
  std::vector<int> approach;     // the tiles in front of and behind every doorway
  std::vector<int> keep;         // stairs and arrival tiles: no doorway may touch them
  Stairs up, down;
  int up2 = -1, down2 = -1;      // the second tile of a two-tile flight (tile index), -1 for a one-tile flight
  bool shaped = false;           // the floor is not the full rectangle (it has VOID tiles)

  int I(int x, int y) const { return y * W + x; }
  bool in(int x, int y) const { return x >= 0 && y >= 0 && x < W && y < H; }
  bool isWall(int x, int y) const { return !in(x, y) || wall[(size_t)I(x, y)] != 0; }
  bool isFloor(int x, int y) const { return in(x, y) && wall[(size_t)I(x, y)] == 0; }
  bool isShell(int x, int y) const { return !in(x, y) || wall[(size_t)I(x, y)] >= T_SHELL; }
  int roomOf(int x, int y) const { return in(x, y) ? room[(size_t)I(x, y)] : -1; }

  void reset(RoomKind mainKind) {
    rooms.assign(1, RoomDef());
    rooms[0].kind = mainKind;
    doorTiles.clear(); approach.clear(); keep.clear();
    up = Stairs(); down = Stairs();
    up2 = down2 = -1;
  }
  void init(int w, int h, RoomKind mainKind) {
    W = w; H = h;
    wall.assign((size_t)W * H, T_SHELL);
    room.assign((size_t)W * H, -1);
    for (int y = 2; y <= H - 2; y++)
      for (int x = 1; x <= W - 2; x++) wall[(size_t)I(x, y)] = T_FLOOR;
    shaped = false;
    reset(mainKind);
  }
  // M3b: a floor of any shape: floor where fl(x, y) (only rows 2..H-2, columns 1..W-2 count), the shell the walls
  // round it (8-neighbours of the floor, and the two back-wall rows over a floor that reaches row 2), void beyond
  template <class F>
  void initShape(int w, int h, RoomKind mainKind, F&& fl) {
    W = w; H = h;
    wall.assign((size_t)W * H, T_VOID);
    room.assign((size_t)W * H, -1);
    for (int y = 2; y <= H - 2; y++)
      for (int x = 1; x <= W - 2; x++)
        if (fl(x, y)) wall[(size_t)I(x, y)] = T_FLOOR;
    for (int y = 0; y < H; y++)
      for (int x = 0; x < W; x++) {
        if (wall[(size_t)I(x, y)] == T_FLOOR) continue;
        bool near = false;
        for (int oy = -1; oy <= 1 && !near; oy++)
          for (int ox = -1; ox <= 1 && !near; ox++) near = isFloor(x + ox, y + oy);
        if (near || (y == 0 && isFloor(x, 2))) wall[(size_t)I(x, y)] = T_SHELL;
      }
    shaped = false;
    for (uint8_t t : wall) if (t == T_VOID) shaped = true;
    reset(mainKind);
  }

  // Carves the room [x0..x1] x [y0..y1] (floor tiles, inclusive) and walls it off from the rest of the floor: an E-W
  // partition (2 rows) on a side where floor goes on north or south, a N-S partition (1 column) east or west. Rooms
  // may share partitions. clip (M3b, shaped floors): the rectangle may reach over the shell (a room against a round
  // wall); only its floor tiles are the room. Returns the room index, or -1 when it does not fit.
  int carve(int x0, int y0, int x1, int y1, RoomKind k, bool clip = false) {
    if (x1 - x0 < 1 || y1 - y0 < 1 || x0 < 1 || y0 < 2 || x1 > W - 2 || y1 > H - 2) return -1;
    int area = 0;
    for (int y = y0; y <= y1; y++)
      for (int x = x0; x <= x1; x++) {
        const uint8_t t = wall[(size_t)I(x, y)];
        if (t == T_PART || room[(size_t)I(x, y)] >= 1 || (t >= T_SHELL && !clip)) return -1;
        area += t == T_FLOOR;
      }
    if (area < 4) return -1;
    // a side needs a partition where the floor goes on beyond it
    auto floorRow = [&](int y) { for (int x = x0; x <= x1; x++) if (isFloor(x, y)) return true; return false; };
    auto floorCol = [&](int x) { for (int y = y0; y <= y1; y++) if (isFloor(x, y)) return true; return false; };
    const bool n = y0 > 2 && floorRow(y0 - 1), s = y1 < H - 2 && floorRow(y1 + 1), w = x0 > 1 && floorCol(x0 - 1), e = x1 < W - 2 && floorCol(x1 + 1);
    if ((n && y0 < 5) || (s && y1 > H - 5) || (w && x0 < 3) || (e && x1 > W - 4)) return -1;
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
    for (int t : wt) if (wall[(size_t)t] == T_FLOOR) wall[(size_t)t] = T_PART;
    RoomDef R;
    R.kind = k;
    R.r = IRect{x0, y0, x1 - x0 + 1, y1 - y0 + 1};
    R.carved = true;
    rooms.push_back(R);
    int ri = (int)rooms.size() - 1;
    for (int y = y0; y <= y1; y++)
      for (int x = x0; x <= x1; x++)
        if (wall[(size_t)I(x, y)] == T_FLOOR) room[(size_t)I(x, y)] = (int8_t)ri;
    return ri;
  }

  // The floor left over is the main room: it must be one connected piece.
  // (M3b) the biggest piece is the main room; a few tiles cut off from it (between a curved wall and an alcove's
  // partition) are walled up; a bigger piece cut off fails the draw
  bool finish() {
    std::vector<int8_t> comp((size_t)W * H, -1);
    std::vector<std::vector<int>> parts;
    for (int i = 0; i < W * H; i++) {
      if (wall[(size_t)i] || room[(size_t)i] >= 0 || comp[(size_t)i] >= 0) continue;
      std::vector<int> q{i};
      const int8_t ci = (int8_t)std::min<size_t>(parts.size(), 120);
      comp[(size_t)i] = ci;
      for (size_t h = 0; h < q.size(); h++) {
        int x = q[h] % W, y = q[h] / W;
        static const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
        for (int k = 0; k < 4; k++) {
          int nx = x + dx[k], ny = y + dy[k];
          if (!isFloor(nx, ny) || room[(size_t)I(nx, ny)] >= 0 || comp[(size_t)I(nx, ny)] >= 0) continue;
          comp[(size_t)I(nx, ny)] = ci;
          q.push_back(I(nx, ny));
        }
      }
      parts.push_back(q);
    }
    if (parts.empty()) return false;
    size_t big = 0;
    for (size_t k = 1; k < parts.size(); k++) if (parts[k].size() > parts[big].size()) big = k;
    for (size_t k = 0; k < parts.size(); k++) {
      if (k == big) { for (int t : parts[k]) room[(size_t)t] = 0; continue; }
      if (parts[k].size() > 6) return false;
      for (int t : parts[k]) wall[(size_t)t] = T_PART;
    }
    bbox();
    return parts[big].size() >= 6;
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
      if (wall[(size_t)c.a] != T_PART || (c.b >= 0 && wall[(size_t)c.b] != T_PART)) return;   // (M3b) never through the shell
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
    wall[(size_t)c.a] = T_FLOOR; room[(size_t)c.a] = (int8_t)ri;
    doorTiles.push_back(c.a);
    if (c.b >= 0) { wall[(size_t)c.b] = T_FLOOR; room[(size_t)c.b] = (int8_t)ri; doorTiles.push_back(c.b); }
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
// the plan template families (interiorTemplate names them; the inn audit counts them)
enum class Tmpl : uint8_t {
  Plain, Classic,
  // inns by culture (VISION_PLAN 15.14 leftover: inns no longer share one floor plan)
  InnHeartland, InnMeadHall, InnDrovers, InnTaberna, InnCaravanserai, InnFondouk, InnYurt, InnStilt, InnTeaHouse, InnParlour,
  InnHostel, InnTreeHall, InnSpire, InnBooths,
  // shapes and the new purposes
  RoundHome, RoundHall, CourtHouse, LHouse, Longhouse, CrossTemple, RoundTemple, FrontCourt,
  Guildhall, Exchange, MeadHall, FeastTent, Bathhouse, TeaHouse, Lodge, CouncilHall,
  // seats of power
  SeatCastle, SeatCourt, SeatGreatHall, SeatTent, SeatSanctum, SeatDoge, SeatSpire, SeatTree, SeatStilt,
  COUNT
};
const char* tmplName(Tmpl t) {
  static const char* n[] = {"plain", "classic",
                            "inn: heartland", "inn: mead hall", "inn: drovers' corridor", "inn: taberna", "inn: caravanserai", "inn: fondouk",
                            "inn: yurt alcoves", "inn: stilt boardwalk", "inn: tea house", "inn: bar and parlour", "inn: pilgrims' hostel",
                            "inn: tree hall", "inn: spire", "inn: sleeping booths",
                            "round home", "round hall", "courtyard house", "L house", "longhouse", "cross temple", "round temple", "front court",
                            "guildhall", "exchange", "mead hall", "feast tent", "bathhouse", "tea house", "warrior lodge", "council hall",
                            "seat: castle", "seat: court palace", "seat: great hall", "seat: tent court", "seat: sanctum", "seat: doge's hall",
                            "seat: council spire", "seat: tree palace", "seat: stilt hall"};
  static_assert(sizeof(n) / sizeof(n[0]) == (size_t)Tmpl::COUNT, "a name per template");
  return (int)t < (int)Tmpl::COUNT ? n[(int)t] : "?";
}

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
  // M3b: what the builder's blueprint says (rpg/build/blueprint.h InteriorShape) and who the building is for
  bld::Floorplan fp = bld::Floorplan::Rect;
  int lCorner = 3, courtW = 8, courtH = 8;
  int culture = -1;     // cult::Archetype of the people who built it (-1: the classic look)
  int seat = -1;        // cult::Seat when this is a seat of power (Bldg::seat - 1), else -1
  bool royal = false;   // the ruler's seat (bldgIsRoyalSeat): the king sits on its ground floor
  art::Furniture furn = art::Furniture::Chairs;
  Tmpl tmpl = Tmpl::Plain;
  // shaped floors: an L's missing corner [lx0..lx1] x [ly0..ly1]; a cross's nave half-width and transept rows; the
  // court of a courtyard plan [cx0..cx1] x [cy0..cy1] (room 0, open to the sky); a hall's long hearth row (-1 none)
  int lx0 = 0, lx1 = -1, ly0 = 0, ly1 = -1;
  int naveHW = 2, tr0 = 3, tr1 = 6;
  int cx0 = -1, cx1 = -1, cy0 = -1, cy1 = -1;
  int pitY = -1, pitX0 = -1, pitX1 = -1;
  int bodyW = 0, bodyH = 0;   // the body volume's extent in tiles (0: the footprint)
  std::vector<int> pool;      // a bathhouse's pool tiles (ground floor; Ground::Water, part of the bath room)
  int X(int x) const { return mir ? W - 1 - x : x; }
  bool shapedFp() const { return fp == bld::Floorplan::Round || fp == bld::Floorplan::L || fp == bld::Floorplan::Cross; }
  int carve(Geo& g, int x0, int y0, int x1, int y1, RoomKind k) const {
    int a = X(x0), c = X(x1);
    return g.carve(std::min(a, c), y0, std::max(a, c), y1, k, g.shaped);
  }
  // is (x, y) floor in this plan's shape (rows 2..H-2, columns 1..W-2)
  bool inShape(int x, int y) const {
    switch (fp) {
      case bld::Floorplan::Round: {
        // the disc inscribed in the floor area, tested at tile centres (doubled coordinates, integer maths)
        const int64_t dx = 2 * x + 1 - W, dy = 2 * y - H, rx = W - 2, ry = H - 3;
        return dx * dx * ry * ry * 20 + dy * dy * rx * rx * 20 <= rx * rx * ry * ry * 21;
      }
      case bld::Floorplan::L: return !(x >= lx0 && x <= lx1 && y >= ly0 && y <= ly1);
      case bld::Floorplan::Cross: return std::abs(x - ex) <= naveHW || (y >= tr0 && y <= tr1);
      default: return true;
    }
  }
  // the floor of one storey, in the blueprint's shape
  void initFloor(Geo& g, RoomKind k) const {
    if (shapedFp()) g.initShape(W, H, k, [&](int x, int y) { return inShape(x, y); });
    else g.init(W, H, k);
  }
};

// (M3b) a debugging aid: EMB_INTERIOR_WHY=1 counts which line of the templates turned a plan down (printed by the
// tests); nope(line) returns false
int gNopeLine[8192];
bool nope(int line) { if (line >= 0 && line < 8192) gNopeLine[line]++; return false; }

int oddUp(int v) { return v % 2 == 0 ? v + 1 : v; }
// (M3b fixer) the peoples whose halls burn a long hearth down their middle (a stone-kerbed trench of embers): the
// northern halls. Everyone else keeps the fire on a hearth or in a short pit (a stilt house never has a trench of
// embers on its plank floor). -1: the classic look.
bool trenchCulture(int c) {
  using A = cult::Archetype;
  return c < 0 || c == (int)A::Fjordfolk || c == (int)A::Highland || c == (int)A::Heartland;
}
// (M3b) the dispatcher and the wrappers that plan a purpose's own template inside a shape (declared here: the inn and
// purpose templates call them, they call the templates back)
bool planFor(Plan& T, Rng& r);
bool planEmbeddedL(Plan& T, Rng& r);
bool planFrontCourt(Plan& T, Rng& r);

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
// M3b (shaped floors): stairs from floor f anywhere a flight fits under a wall face of the floor's main room (a round
// room's back arc, an L's inner wall), on the preferred side first (side < 0 west, > 0 east, 0 either), nearest the
// back. Deterministic: candidates in a fixed order, the rng only breaks ties.
bool stairsAnywhere(Plan& P, int f, int side, Rng& r, bool allowSingle = false) {
  struct C { int x, y, score; };
  std::vector<C> cs;
  const Geo& a = P.geo[(size_t)f];
  for (int y = 2; y < P.H - 2; y++)
    for (int x = 1; x < P.W - 1; x++) {
      if (a.roomOf(x, y) != 0 || !a.isWall(x, y - 1)) continue;
      // (clear of the bar's strip and the long hearth's row)
      if (f == 0 && P.barY >= 0 && y >= P.barY - 1 && y <= P.barY + 4 && x >= std::min(P.barX0, P.flapX) - 2 && x <= std::max(P.barX1, P.flapX) + 2) continue;
      if (f == 0 && P.pitY >= 0 && std::abs(y - P.pitY) <= 2) continue;
      // (the throne's place on the axis at the far wall stays clear in keeps, palaces and seats)
      if (f == 0 && (P.type == Building::Keep || P.type == Building::Palace || P.seat >= 0 || P.type == Building::Temple) && std::abs(x - P.ex) <= (P.fp == bld::Floorplan::Round ? 1 : 3) && y <= 5) continue;
      int sc = y * 4 + r.irange(3);
      if (side < 0 && x > P.W / 2) sc += 40;
      if (side > 0 && x < P.W / 2) sc += 40;
      if (std::abs(x - P.ex) <= 1) sc += 12;   // not straight in front of the way in
      cs.push_back({x, y, sc});
    }
  std::stable_sort(cs.begin(), cs.end(), [](const C& p, const C& q) { return p.score < q.score; });
  for (const C& c : cs)
    if (setStairs(P, f, c.x, c.y, false, false)) return true;
  if (allowSingle)
    for (const C& c : cs)
      if (setStairs(P, f, c.x, c.y, false, true)) return true;
  return false;
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
    // M3b: the society's buildings
    case Building::Guildhall: return RoomKind::Assembly;
    case Building::Exchange: return RoomKind::Trading;
    case Building::MeadHall: return RoomKind::Feast;
    case Building::Bathhouse: return RoomKind::Bath;
    case Building::TeaHouse: return RoomKind::TeaRoom;
    case Building::Lodge: return RoomKind::Training;
    case Building::CouncilHall: return RoomKind::Assembly;
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
// M3b: the guest rooms are numbered across the whole building, the ground floor's first (an inn may let rooms on
// both floors)
void numberGuestsAll(Plan& P) {
  int base = 0;
  for (Geo& g : P.geo) {
    numberGuests(g);
    int n = 0;
    for (size_t i = 1; i < g.rooms.size(); i++)
      if (g.rooms[i].kind == RoomKind::GuestRoom) { g.rooms[i].guest = (int8_t)(g.rooms[i].guest + base); n++; }
    base += n;
  }
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

bool clipBar(Plan& P, const Geo& g);

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
  P.initFloor(g0, RoomKind::Common);
  int kit = P.carve(g0, 1, 2, kw, kd, RoomKind::Kitchen);
  int sto = store ? P.carve(g0, W - 1 - sw, 2, W - 2, kd, RoomKind::Storeroom) : 0;
  if (kit < 0 || sto < 0 || !g0.finish()) return false;
  P.barY = kd + 3;
  P.barX0 = std::min(P.X(1), P.X(kw - 1)); P.barX1 = std::max(P.X(1), P.X(kw - 1));
  P.flapX = P.X(kw);
  if (!clipBar(P, g0)) return false;
  // upstairs
  if (P.floors >= 2) {
    Geo& g1 = P.geo[1];
    P.initFloor(g1, RoomKind::Corridor);
    std::vector<int> back, front;
    // (M1) inns differ upstairs: room widths (cramped cells or roomy chambers), where the innkeeper sleeps, and a
    // linen room or a guests' sitting room in the bigger ones
    const int style = r.irange(3);
    const int minW = style == 2 ? 4 : 3, maxW = style == 0 ? 4 : style == 1 ? 5 : 6;
    if (!carveRows(P, g1, kd, mode == 1 ? sxU : -1, dbl ? kd + 7 : -1, minW, maxW, r, back, front)) {
      P.initFloor(g1, RoomKind::Corridor);
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
    // (draws sequenced explicitly, right argument first as MSVC/GCC evaluate them: clang goes left to right)
    const int sy = r.f() < 0.5f ? kd + 2 : 3;
    const int sx = R.x + r.irange(R.w);
    if (!g0.door(sto, 0, sx, sy, r)) return false;
  }
  // the kitchen door must open behind the counter
  int dx = g0.rooms[(size_t)kit].doorX;
  if (dx < P.barX0 || dx > P.barX1 || g0.rooms[(size_t)kit].doorY != kd + 2) return false;
  return true;
}

// ===================================================================================================== M3b inns
// VISION_PLAN 15.14 leftover: inns no longer share one floor plan. Each people plans its inn its own way (a caravanserai
// round a court, a mead hall with its long hearth, a steppe yurt's felt alcoves, a stilt inn's boardwalk, a tea house
// with its rooms off a screened passage...), always by 15.7: the common room with the bar under the kitchen, guest beds
// never in the common room, every rented room walled, with a door, a bed, a chest and a candle, the innkeeper's room
// apart. A single-storey inn lets its rooms behind partitions on the ground floor; a taller one upstairs.

// is the bar's strip (the shelves' row, the walkway, the counter, the customers' row) open floor of room `ri`
bool barFits(const Plan& P, const Geo& g, int ri = 0) {
  if (P.barY < 0) return nope(__LINE__);
  for (int y = P.barY; y <= P.barY + 3; y++)
    for (int x = std::min(P.barX0, P.flapX); x <= std::max(P.barX1, P.flapX); x++)
      if (!g.isFloor(x, y) || g.roomOf(x, y) != ri) return nope(__LINE__);
  return g.isWall(P.barX0, P.barY - 1) && g.isWall(P.barX1, P.barY - 1);
}
// (M3b) on a shaped floor (a round room) the bar's strip is cut to the floor it stands on, the flap kept at its end
bool clipBar(Plan& P, const Geo& g) {
  if (!g.shaped || P.barY < 0) return true;
  const bool right = P.flapX > P.barX1;
  int lo = std::min(P.barX0, P.flapX), hi = std::max(P.barX1, P.flapX);
  auto ok = [&](int x) {
    for (int y = P.barY; y <= P.barY + 3; y++) if (!g.isFloor(x, y) || g.roomOf(x, y) != 0) return nope(__LINE__);
    return true;
  };
  while (lo <= hi && !ok(lo)) lo++;
  while (hi >= lo && !ok(hi)) hi--;
  for (int x = lo; x <= hi; x++) if (!ok(x)) return nope(__LINE__);
  if (hi - lo < 2) return nope(__LINE__);
  if (right) { P.flapX = hi; P.barX1 = std::min(P.barX1, hi - 1); P.barX0 = std::max(P.barX0, lo); }
  else { P.flapX = lo; P.barX0 = std::max(P.barX0, lo + 1); P.barX1 = std::min(P.barX1, hi); }
  return P.barX1 - P.barX0 >= 1 && g.isWall(P.barX0, P.barY - 1) && g.isWall(P.barX1, P.barY - 1);
}

// the kitchen [x0..x1] x [2..kd] (unmirrored; the back wall above it) with the bar below it: the keeper's shelves on
// the row under its partition face, the walkway, the counter over the kitchen's columns but one, the flap at that one
int innKitchenBar(Plan& P, Geo& g, int x0, int x1, int kd, bool flapEast) {
  int kit = P.carve(g, x0, 2, x1, kd, RoomKind::Kitchen);
  if (kit < 0) return -1;
  P.kd = kd;
  P.barY = kd + 3;
  const int a = flapEast ? x0 : x0 + 1, b = flapEast ? x1 - 1 : x1;
  P.barX0 = std::min(P.X(a), P.X(b)); P.barX1 = std::max(P.X(a), P.X(b));
  P.flapX = P.X(flapEast ? x1 : x0);
  return kit;
}
// the kitchen's door, opening behind the counter (into the walkway under the shelves' wall)
bool innKitchenDoor(Plan& P, Geo& g, int kit, Rng& r) {
  if (!g.door(kit, 0, P.barX0 + r.irange(std::max(1, P.barX1 - P.barX0 + 1)), P.barY - 1, r)) return nope(__LINE__);
  const RoomDef& K = g.rooms[(size_t)kit];
  return K.doorX >= P.barX0 && K.doorX <= P.barX1 && K.doorY == P.barY - 1;
}
// who sleeps where among the carved rooms ids: the innkeeper takes ids[own] (own < 0: the room farthest from (qx, qy),
// the quiet end); a big inn keeps a linen store; the rest are let. False: fewer than two left to let
bool letRooms(Geo& g, const std::vector<int>& ids, int own, int qx, int qy, Rng& r) {
  if (ids.size() < 3) return nope(__LINE__);
  int o = own >= 0 && own < (int)ids.size() ? ids[(size_t)own] : ids[0];
  if (own < 0)
    for (int ri : ids) {
      const IRect &a = g.rooms[(size_t)ri].r, &b = g.rooms[(size_t)o].r;
      if (std::abs(a.cx() - qx) + std::abs(a.cy() - qy) > std::abs(b.cx() - qx) + std::abs(b.cy() - qy)) o = ri;
    }
  int guests = 0;
  for (int ri : ids) { g.rooms[(size_t)ri].kind = ri == o ? RoomKind::OwnerRoom : RoomKind::GuestRoom; guests += ri != o; }
  if (guests >= 5 && r.f() < 0.6f) {
    std::vector<int> c;
    for (int ri : ids) if (ri != o) c.push_back(ri);
    g.rooms[(size_t)c[(size_t)r.irange((int)c.size())]].kind = RoomKind::Storeroom;
    guests--;
  }
  return guests >= 2;
}
// the rented rooms upstairs (every floor above the ground): rooms along a corridor, behind the back wall and (deep
// inns) along the front, the innkeeper's at the quiet end. The flight up from the ground floor stands at (sxU, sy)
// (unmirrored column): at the back wall (sy == 2, the rooms leave it a nook) or under a back room's partition face
bool innUpstairs(Plan& P, Rng& r, int sxU, int sy, int kd) {
  for (int f = 1; f < P.floors; f++) {
    Geo& g1 = P.geo[(size_t)f];
    P.initFloor(g1, RoomKind::Corridor);
    const bool dbl = kd + 7 <= P.H - 3;
    std::vector<int> back, front;
    const int style = r.irange(3);
    const int minW = style == 2 ? 4 : 3, maxW = style == 0 ? 4 : style == 1 ? 5 : 6;
    // the landing over the flight reaches the back wall (no rented room right over the stairs, whose door would crowd
    // the head of the flight)
    int nook = f == 1 ? std::min(sxU, P.W - 3) : -1;
    if (nook >= 0 && !nookOk(P.W, nook, 3)) nook = nookOk(P.W, nook - 1, 3) ? nook - 1 : -1;
    if (!carveRows(P, g1, kd, nook, dbl ? kd + 7 : -1, minW, maxW, r, back, front)) {
      P.initFloor(g1, RoomKind::Corridor);
      back.clear(); front.clear();
      if (!carveRows(P, g1, kd, nook, dbl ? kd + 7 : -1, 3, 5, r, back, front)) return nope(__LINE__);
    }
    if (!g1.finish()) return nope(__LINE__);
    if (f == 1) { if (!setStairs(P, 0, P.X(sxU), sy)) return nope(__LINE__); }
    else if (!stairsAnywhere(P, f - 1, f % 2 ? 1 : -1, r)) return nope(__LINE__);
    std::vector<int> all = back;
    all.insert(all.end(), front.begin(), front.end());
    const Stairs& st = P.geo[(size_t)f].down;
    if (!letRooms(g1, all, f == P.floors - 1 || r.f() < 0.5f ? -1 : r.irange((int)all.size()), st.x, st.y, r)) return nope(__LINE__);
    if (!doorsTo(g1, back, r) || !doorsTo(g1, front, r)) return nope(__LINE__);
  }
  return true;
}
// a back-wall nook for the stairs in the common room over the columns [a..b] (unmirrored), clear of the bar
int innNook(const Plan& P, int a, int b, Rng& r) {
  std::vector<int> c;
  for (int x = a; x + 1 <= b; x++) if (nookOk(P.W, x, 3)) c.push_back(x);
  return c.empty() ? -1 : c[(size_t)r.irange((int)c.size())];
}
// doors from each room in ids into `target`, on the side nearest (tx, ty)
bool doorsToward(Geo& g, const std::vector<int>& ids, int target, int tx, int ty, Rng& r) {
  for (int ri : ids) {
    const IRect& R = g.rooms[(size_t)ri].r;
    int px = std::clamp(tx, R.x, R.x + R.w - 1), py = std::clamp(ty, R.y, R.y + R.h - 1);
    if (px == tx && py == ty) py = R.y + R.h;
    if (!g.door(ri, target, px, py, r)) return nope(__LINE__);
  }
  return true;
}

// ---- heartland: the classic inn (planInn) when it has rooms upstairs; one storey: the kitchen in a back corner, the
// rented rooms in a row along the rest of the back wall, the common room before them
bool planInnHeartland(Plan& P, Rng& r) {
  P.tmpl = Tmpl::InnHeartland;
  if (P.floors >= 2) return planInn(P, r);
  const int W = P.W, H = P.H;
  Geo& g0 = P.geo[0];
  P.initFloor(g0, RoomKind::Common);
  const int kd = 4 + (H >= 16 && r.f() < 0.5f ? 1 : 0), kw = 4 + r.irange(2);
  const int kit = innKitchenBar(P, g0, 1, kw, kd, true);
  if (kit < 0) return nope(__LINE__);
  std::vector<std::pair<int, int>> segs;
  if (!splitSeg(kw + 2, W - 2, 3, 4, r, segs) || segs.size() < 3) return nope(__LINE__);
  std::vector<int> ids;
  for (auto& s : segs) { int ri = P.carve(g0, s.first, 2, s.second, kd, RoomKind::GuestRoom); if (ri < 0) return nope(__LINE__); ids.push_back(ri); }
  if (!g0.finish() || !barFits(P, g0)) return nope(__LINE__);
  if (!letRooms(g0, ids, -1, P.X(1), kd, r)) return nope(__LINE__);
  if (!innKitchenDoor(P, g0, kit, r)) return nope(__LINE__);
  return doorsToward(g0, ids, 0, P.ex, kd + 3, r);
}

// ---- fjordfolk: a mead-hall inn. The kitchen and the bar at the west end, the long hearth down the middle of the
// hall with benches by it; one storey: sleeping booths along the back wall (each walled with its door), else lofts
bool planInnMeadHall(Plan& P, Rng& r) {
  P.tmpl = Tmpl::InnMeadHall;
  const int W = P.W, H = P.H;
  Geo& g0 = P.geo[0];
  P.initFloor(g0, RoomKind::Common);
  const int kd = 4, kw = 4 + r.irange(2);
  const int kit = innKitchenBar(P, g0, 1, kw, kd, true);
  if (kit < 0) return nope(__LINE__);
  std::vector<int> ids;
  int sxU = -1;
  // (M3b fixer) one storey: the sleeping booths stand along the hall's front wall either side of the doors (the
  // northern hall sleeps its guests on the side platforms), and along the back beside the kitchen only when the hall
  // is too narrow for three at the front
  int frontD = 0;
  if (P.floors == 1) {
    std::vector<std::pair<int, int>> segs;
    const int fd = 3;
    if (splitSeg(1, P.ex - 3, 3, 5, r, segs) && splitSeg(P.ex + 3, W - 2, 3, 5, r, segs) && segs.size() >= 3 && H - 1 - fd > P.barY + 7) {
      for (auto& s : segs) { int ri = P.carve(g0, s.first, H - 1 - fd, s.second, H - 2, RoomKind::GuestRoom); if (ri < 0) return nope(__LINE__); ids.push_back(ri); }
      frontD = fd + 2;
    } else {
      segs.clear();
      if (!splitSeg(kw + 2, W - 2, 3, 4, r, segs) || segs.size() < 3) return nope(__LINE__);
      for (auto& s : segs) { int ri = P.carve(g0, s.first, 2, s.second, kd, RoomKind::GuestRoom); if (ri < 0) return nope(__LINE__); ids.push_back(ri); }
    }
  } else if ((sxU = innNook(P, kw + 3, W - 3, r)) < 0) return nope(__LINE__);
  if (!g0.finish() || !barFits(P, g0)) return nope(__LINE__);
  // the long hearth: down the hall's middle row, east of the bar, split where the way from the door crosses it
  P.pitY = (P.barY + 4 + H - 3 - frontD) / 2;
  P.pitX0 = std::max(P.barX0, P.barX1) + 3; P.pitX1 = W - 4;
  if (P.mir) { P.pitX0 = 3; P.pitX1 = std::min(P.barX0, P.barX1) - 3; }
  if (P.pitY <= P.barY + 3 || P.pitY >= H - 2 || P.pitX1 - P.pitX0 < 5) P.pitY = -1;
  if (!innKitchenDoor(P, g0, kit, r)) return nope(__LINE__);
  if (P.floors == 1) {
    if (!letRooms(g0, ids, -1, P.X(1), kd, r)) return nope(__LINE__);
    return doorsToward(g0, ids, 0, P.ex, frontD ? 2 : kd + 3, r);
  }
  return innUpstairs(P, r, sxU, 2, kd);
}

// ---- highland: a drovers' inn. The kitchen and the bar at the back west; the rented rooms stacked down the east
// wall off their own corridor, the innkeeper's at its far end
bool planInnDrovers(Plan& P, Rng& r) {
  P.tmpl = Tmpl::InnDrovers;
  const int W = P.W, H = P.H;
  Geo& g0 = P.geo[0];
  P.initFloor(g0, RoomKind::Common);
  const int kd = 4, kw = 4 + r.irange(2);
  const int kit = innKitchenBar(P, g0, 1, kw, kd, true);
  if (kit < 0) return nope(__LINE__);
  if (P.floors >= 2) {
    const int sw = 3;
    const int sto = P.carve(g0, W - 1 - sw, 2, W - 2, kd, RoomKind::Storeroom);
    if (sto < 0 || !g0.finish() || !barFits(P, g0) || !innKitchenDoor(P, g0, kit, r)) return nope(__LINE__);
    if (!innUpstairs(P, r, W - 2 - r.irange(sw - 1), kd + 3, kd)) return nope(__LINE__);
    return g0.door(sto, 0, P.X(W - 2 - sw), 3, r) || g0.door(sto, 0, P.X(W - 3), kd + 2, r);
  }
  const int gw = 3 + r.irange(2), gx0 = W - 1 - gw;
  // rooms three rows deep down the east wall
  std::vector<int> ids;
  for (int y = 2; y + 2 <= H - 2; y += 5) {
    int ri = P.carve(g0, gx0, y, W - 2, (y + 2 > H - 5 ? H - 2 : y + 2), RoomKind::GuestRoom);
    if (ri < 0) break;
    ids.push_back(ri);
  }
  if (ids.size() < 3) return nope(__LINE__);
  int ce = g0.rooms[(size_t)ids.back()].r.y + 2;
  if (ce > H - 5) ce = H - 2;
  const int cor = P.carve(g0, gx0 - 3, 2, gx0 - 2, ce, RoomKind::Corridor);
  if (cor < 0 || gx0 - 4 <= kw + 4) return nope(__LINE__);
  if (!g0.finish() || !barFits(P, g0)) return nope(__LINE__);
  if (!letRooms(g0, ids, 0, 0, 0, r)) return nope(__LINE__);
  if (!innKitchenDoor(P, g0, kit, r)) return nope(__LINE__);
  for (int ri : ids) if (!g0.door(ri, cor, P.X(gx0 - 1), g0.rooms[(size_t)ri].r.cy(), r)) return nope(__LINE__);
  return g0.door(cor, 0, P.X(gx0 - 4), std::min(ce, H - 3) - 1 - r.irange(2), r);
}

// ---- imperial: a taberna. The kitchen in the middle of the back wall with the bar before it, a store in a back
// corner; one storey: the rented rooms in the two front corners either side of the street door
bool planInnTaberna(Plan& P, Rng& r) {
  P.tmpl = Tmpl::InnTaberna;
  const int W = P.W, H = P.H, ex = P.ex;
  Geo& g0 = P.geo[0];
  P.initFloor(g0, RoomKind::Common);
  const int kd = 4, kw = 5 + r.irange(2);
  const int k0 = ex - kw / 2 + (r.f() < 0.5f ? 1 : 0), k1 = k0 + kw - 1;
  const int kit = innKitchenBar(P, g0, k0, k1, kd, r.f() < 0.5f);
  if (kit < 0) return nope(__LINE__);
  const int sw = k0 - 2;
  // (M3b fixer: a narrow two-storey taberna keeps the west back wall for its flight, no store there)
  const int sto = sw >= 3 && sw <= 5 && (P.floors == 1 || W >= 21) ? P.carve(g0, 1, 2, sw, kd, RoomKind::Storeroom) : 0;
  if (sto < 0) return nope(__LINE__);
  std::vector<int> ids;
  if (P.floors == 1) {
    const int fd = 3 + (H >= 17 ? 1 : 0);
    // the front corners: two rooms either side when they are wide, one when not
    for (int side = 0; side < 2; side++) {
      // (a low taberna: the corner rooms' partitions keep clear of the bar's strip before the kitchen)
      const bool low = H - 3 - fd <= P.barY + 3;
      const int sx0 = std::min(P.barX0, P.flapX), sx1 = std::max(P.barX1, P.flapX);
      const int a = side ? std::max(ex + 3, low ? sx1 + 3 : 0) : 1, b = side ? W - 2 : std::min(ex - 3, low ? sx0 - 3 : W);
      std::vector<std::pair<int, int>> segs;
      if (b - a < 2 || !splitSeg(a, b, 3, 5, r, segs)) return nope(__LINE__);
      for (auto& s : segs) { int ri = P.carve(g0, s.first, H - 1 - fd, s.second, H - 2, RoomKind::GuestRoom); if (ri < 0) return nope(__LINE__); ids.push_back(ri); }
    }
    // (M3b fixer) a narrow taberna lets a room in the back corner beside the kitchen too
    if (ids.size() < 3 && W - 2 - (k1 + 2) >= 2) { const int ri = P.carve(g0, k1 + 2, 2, W - 2, kd, RoomKind::GuestRoom); if (ri >= 0) ids.push_back(ri); }
  }
  if (!g0.finish() || !barFits(P, g0) || !innKitchenDoor(P, g0, kit, r)) return nope(__LINE__);
  if (sto && !g0.door(sto, 0, g0.rooms[(size_t)sto].r.cx(), kd + 2, r)) return nope(__LINE__);
  if (P.floors == 1) {
    if (!letRooms(g0, ids, -1, ex, H - 2, r)) return nope(__LINE__);
    return doorsToward(g0, ids, 0, ex, 2, r);
  }
  int sxU = innNook(P, k1 + 2, W - 3, r);
  if (sxU < 0 && !sto) sxU = innNook(P, 1, k0 - 2, r);
  return sxU >= 0 && innUpstairs(P, r, sxU, 2, kd);
}

// ---- dune (no court): a fondouk. The common hall runs from the door to the kitchen at the back (its pantries either
// side of it); the rented rooms open off the hall down both sides
bool planInnFondouk(Plan& P, Rng& r) {
  P.tmpl = Tmpl::InnFondouk;
  const int W = P.W, H = P.H;
  Geo& g0 = P.geo[0];
  P.initFloor(g0, RoomKind::Common);
  const int sw = 3 + r.irange(2), kd = 4;
  const int kx1 = P.floors >= 2 ? sw + 5 + r.irange(2) : W - 3 - sw;   // (two storeys: the hall keeps a stretch of back wall)
  const int kit = innKitchenBar(P, g0, sw + 2, kx1, kd, r.f() < 0.5f);
  if (kit < 0) return nope(__LINE__);
  // the pantries behind the side rooms, their doors into the kitchen
  const int p0 = P.carve(g0, 1, 2, sw, kd, RoomKind::Storeroom), p1 = P.carve(g0, W - 1 - sw, 2, W - 2, kd, RoomKind::Storeroom);
  if (p0 < 0 || p1 < 0) return nope(__LINE__);
  std::vector<int> ids;
  const int rh = H - kd - 5 >= 9 ? 4 : 3;
  for (int side = 0; side < 2; side++)
    for (int y = kd + 3; y + rh - 1 <= H - 2; y += rh + 2) {
      int ri = P.carve(g0, side ? W - 1 - sw : 1, y, side ? W - 2 : sw, y + rh - 1, P.floors == 1 ? RoomKind::GuestRoom : RoomKind::Storeroom);
      if (ri < 0) break;
      ids.push_back(ri);
    }
  if (ids.size() < (P.floors == 1 ? 3u : 0u)) return nope(__LINE__);
  if (!g0.finish()) return nope(__LINE__);
  if (!barFits(P, g0)) return nope(__LINE__);
  if (!innKitchenDoor(P, g0, kit, r)) return nope(__LINE__);
  if (P.floors >= 2) {
    // two storeys: the side rooms below are the stores and the guests' parlour, the rooms are let upstairs; the flight
    // climbs the hall's back wall beside the kitchen
    for (size_t k = 0; k < ids.size(); k++) g0.rooms[(size_t)ids[k]].kind = k == 0 ? RoomKind::Parlour : RoomKind::Storeroom;
    const int sxU = innNook(P, kx1 + 2, W - 4 - sw, r);
    if (sxU < 0 || !innUpstairs(P, r, sxU, 2, kd)) return nope(__LINE__);
  }
  for (int pi : {p0, p1}) {
    const IRect& R = g0.rooms[(size_t)pi].r;
    if (!g0.door(pi, kit, R.x < P.ex ? R.x + R.w : R.x - 1, R.cy(), r) && !g0.door(pi, 0, R.x < P.ex ? R.x + R.w : R.x - 1, R.cy(), r) &&
        !g0.door(pi, 0, R.cx(), R.y + R.h + 1, r)) return nope(__LINE__);
  }
  for (int ri : ids) {
    const IRect& R = g0.rooms[(size_t)ri].r;
    if (!g0.door(ri, 0, R.x < P.ex ? R.x + R.w : R.x - 1, R.cy(), r)) return nope(__LINE__);
  }
  return P.floors >= 2 || letRooms(g0, ids, -1, P.ex, H - 2, r);
}

// ---- marsh: a stilt inn. At the back a boardwalk runs from the common room between the guests' cabins and the
// kitchen's; the bar stands under the kitchen cabin's wall
bool planInnStilt(Plan& P, Rng& r) {
  P.tmpl = Tmpl::InnStilt;
  const int W = P.W, H = P.H, ex = P.ex;
  Geo& g0 = P.geo[0];
  P.initFloor(g0, RoomKind::Common);
  const int by = H - 9;   // the back block: rows 2..by
  // (M3b fixer) a taller inn keeps one cabin each side (the kitchen's and a store's), its rooms let upstairs
  const bool tall = P.floors >= 2;
  if (by < (tall ? 5 : 9)) return nope(__LINE__);
  const int m = (by + 2) / 2 - 1;   // the split of each side's cabins (three rows each at least)
  // the boardwalk two tiles wide on the door's axis; cabins either side, the kitchen the front west one
  const int cor = P.carve(g0, ex - 1, 2, ex + 1, by, RoomKind::Corridor);
  if (cor < 0) return nope(__LINE__);
  int kit = -1;
  std::vector<int> ids;
  for (int side = 0; side < 2; side++)
    for (int part = tall ? 1 : 0; part < 2; part++) {
      const int x0 = side ? ex + 3 : 1, x1 = side ? W - 2 : ex - 3;
      const int y0 = part && !tall ? m + 3 : 2, y1 = part ? by : m;
      const bool k = side == 0 && part == 1;
      int ri = P.carve(g0, x0, y0, x1, y1, k ? RoomKind::Kitchen : RoomKind::GuestRoom);
      if (ri < 0) return nope(__LINE__);
      if (k) kit = ri; else ids.push_back(ri);
    }
  // the bar under the kitchen cabin's south wall
  const IRect& K = g0.rooms[(size_t)kit].r;
  P.kd = by;
  P.barY = by + 3;
  P.barX0 = K.x + (P.mir ? 1 : 0); P.barX1 = K.x + K.w - 1 - (P.mir ? 0 : 1);
  P.flapX = P.mir ? K.x : K.x + K.w - 1;
  if (!g0.finish() || !barFits(P, g0)) return nope(__LINE__);
  if (!g0.door(kit, 0, (P.barX0 + P.barX1) / 2, by + 2, r)) return nope(__LINE__);
  if (g0.rooms[(size_t)kit].doorX < P.barX0 || g0.rooms[(size_t)kit].doorX > P.barX1) return nope(__LINE__);
  if (!g0.door(cor, 0, ex, by + 2, r)) return nope(__LINE__);
  for (int ri : ids) if (!g0.door(ri, cor, g0.rooms[(size_t)ri].r.x < ex ? ex - 2 : ex + 2, g0.rooms[(size_t)ri].r.cy(), r)) return nope(__LINE__);
  if (P.floors == 1) return letRooms(g0, ids, -1, ex, by + 2, r);
  // (M3b fixer) a taller stilt inn: the cabins below are its net and fish stores and the guests' parlour, the rooms are
  // let upstairs; the flight climbs from the head of the boardwalk (never the heartland's plan for a marsh people)
  for (size_t k = 0; k < ids.size(); k++) g0.rooms[(size_t)ids[k]].kind = k == 0 ? RoomKind::Parlour : RoomKind::Storeroom;
  return innUpstairs(P, r, ex - 1, 2, 4);
}

// ---- jade: a tea-house inn. The tea room by the street, the kitchen at its west end, the counter beside it against
// the wall behind; one storey: the rented rooms open off a screened passage across the back; two: private tea rooms
// along the back and the rented rooms upstairs
bool planInnTeaHouse(Plan& P, Rng& r) {
  P.tmpl = Tmpl::InnTeaHouse;
  const int W = P.W, H = P.H;
  Geo& g0 = P.geo[0];
  P.initFloor(g0, RoomKind::Common);
  const int gd = 4;                      // the back rooms: rows 2..gd
  const bool passage = P.floors == 1;
  const int c0 = gd + 3, c1 = gd + 4;    // the passage (one storey)
  const int ty0 = passage ? c1 + 3 : gd + 3;   // the tea room's first row (the counter's shelves)
  if (ty0 + (passage ? 5 : 4) > H - 2) return nope(__LINE__);
  std::vector<std::pair<int, int>> segs;
  if (!splitSeg(1, W - 2, 3, 5, r, segs) || segs.size() < 3) return nope(__LINE__);
  std::vector<int> ids;
  for (auto& s : segs) { int ri = P.carve(g0, s.first, 2, s.second, gd, passage ? RoomKind::GuestRoom : RoomKind::Parlour); if (ri < 0) return nope(__LINE__); ids.push_back(ri); }
  const int cor = passage ? P.carve(g0, 1, c0, W - 2, c1, RoomKind::Corridor) : 0;
  if (cor < 0) return nope(__LINE__);
  const int kw = passage ? 3 + r.irange(2) : 5;   // (two storeys: a pantry's door may come through its back wall)
  const int kit = P.carve(g0, 1, ty0, kw, H - 2, RoomKind::Kitchen);
  if (kit < 0) return nope(__LINE__);
  P.barY = ty0;
  P.barX0 = std::min(P.X(kw + 2), P.X(kw + 4)); P.barX1 = std::max(P.X(kw + 2), P.X(kw + 4));
  P.flapX = P.X(kw + 5);
  P.kd = ty0 - 2;
  if (!g0.finish() || !barFits(P, g0)) return nope(__LINE__);
  // the kitchen door into the walkway behind the counter (its east wall, the walkway's row)
  if (!g0.door(kit, 0, P.X(kw + 1), P.barY + 1, r) || g0.rooms[(size_t)kit].doorY != P.barY + 1) return nope(__LINE__);
  if (passage) {
    if (!g0.door(cor, 0, P.ex + (r.f() < 0.5f ? 2 : -2), c1 + 2, r)) return nope(__LINE__);
    for (int ri : ids) if (!g0.door(ri, cor, g0.rooms[(size_t)ri].r.cx(), gd + 2, r)) return nope(__LINE__);
    return letRooms(g0, ids, -1, P.ex, c1, r);
  }
  for (int ri : ids) {
    // the room over the kitchen is its pantry (its door into the kitchen), the rest private tea rooms
    if (g0.door(ri, 0, g0.rooms[(size_t)ri].r.cx(), gd + 2, r)) continue;
    g0.rooms[(size_t)ri].kind = RoomKind::Storeroom;
    if (!g0.door(ri, kit, g0.rooms[(size_t)ri].r.cx(), gd + 2, r)) return nope(__LINE__);
  }
  // the flight up from the tea room under the tea rooms' wall, east of the counter, between their doors
  // (M3b fixer) every column east of the counter in turn (shuffled), so a narrow tea house finds its flight too
  std::vector<int> sxs;
  for (int x = kw + 6; x <= W - 3; x++) sxs.push_back(x);
  for (size_t i = sxs.size(); i > 1; i--) std::swap(sxs[i - 1], sxs[(size_t)r.irange((int)i)]);
  for (size_t k = 0; k < sxs.size() && k < 10; k++) {
    const std::vector<Geo> keep = P.geo;
    if (innUpstairs(P, r, sxs[k], ty0, gd)) return true;
    P.geo = keep;
  }
  return nope(__LINE__);
}

// ---- river: a bar and a parlour. The heartland's kitchen and store at the back; a dining parlour walled off in a
// front corner for the merchants; one storey: the rented rooms between kitchen and store
bool planInnParlour(Plan& P, Rng& r) {
  P.tmpl = Tmpl::InnParlour;
  const int W = P.W, H = P.H, ex = P.ex;
  Geo& g0 = P.geo[0];
  P.initFloor(g0, RoomKind::Common);
  const int kd = 4, kw = 4 + r.irange(2), sw = 3;
  const int kit = innKitchenBar(P, g0, 1, kw, kd, true);
  const int sto = kit >= 0 ? P.carve(g0, W - 1 - sw, 2, W - 2, kd, RoomKind::Storeroom) : -1;
  if (kit < 0 || sto < 0) return nope(__LINE__);
  const int pw = 4 + r.irange(2), pd = 3 + (H >= 16 ? 1 : 0);
  const int par = P.carve(g0, W - 1 - pw, H - 1 - pd, W - 2, H - 2, RoomKind::Parlour);
  if (par < 0 || W - 1 - pw <= ex + 2) return nope(__LINE__);
  std::vector<int> ids;
  if (P.floors == 1) {
    std::vector<std::pair<int, int>> segs;
    if (!splitSeg(kw + 2, W - 3 - sw, 3, 4, r, segs) || segs.size() < 2) return nope(__LINE__);
    for (auto& s : segs) { int ri = P.carve(g0, s.first, 2, s.second, kd, RoomKind::GuestRoom); if (ri < 0) return nope(__LINE__); ids.push_back(ri); }
  }
  if (!g0.finish() || !barFits(P, g0) || !innKitchenDoor(P, g0, kit, r)) return nope(__LINE__);
  if (P.floors >= 2 && !innUpstairs(P, r, W - 2 - r.irange(sw - 1), kd + 3, kd)) return nope(__LINE__);
  if (!g0.door(sto, 0, P.X(W - 3), kd + 2, r) && !g0.door(sto, 0, P.X(W - 2 - sw), 3, r)) return nope(__LINE__);
  if (!g0.door(par, 0, P.X(W - 1 - pw - 1), H - 2 - r.irange(pd - 1), r) && !g0.door(par, 0, P.X(W - 1 - pw + 1), H - 2 - pd, r)) return nope(__LINE__);
  if (P.floors == 1) {
    if (!doorsToward(g0, ids, 0, ex, kd + 3, r)) return nope(__LINE__);
    // (M3b fixer) a narrow inn lets its back corner store too, so it has the rooms it needs
    std::vector<int> all = ids;
    if (all.size() < 3) { g0.rooms[(size_t)sto].kind = RoomKind::GuestRoom; all.push_back(sto); }
    return letRooms(g0, all, -1, ex, H - 2, r);
  }
  return true;
}

// ---- sun temple: a pilgrims' hostel. The kitchen in the middle of the back wall, the bar before it; narrow cells
// along the rest of the back (one storey), the refectory's long tables in front
bool planInnHostel(Plan& P, Rng& r) {
  P.tmpl = Tmpl::InnHostel;
  const int W = P.W, H = P.H, ex = P.ex;
  Geo& g0 = P.geo[0];
  P.initFloor(g0, RoomKind::Common);
  const int kd = 4 + (H >= 16 ? 1 : 0), kw = 5;
  const int khw = P.floors == 1 && r.f() < 0.5f ? 1 : 2;   // (a narrower kitchen leaves room for more cells)
  const int k0 = ex - khw, k1 = ex + khw;
  const int kit = innKitchenBar(P, g0, k0, k1, kd, r.f() < 0.5f);
  if (kit < 0) return nope(__LINE__);
  std::vector<int> ids;
  if (P.floors == 1) {
    std::vector<std::pair<int, int>> segs;
    if (!splitSeg(1, k0 - 2, 3, 5, r, segs) || !splitSeg(k1 + 2, W - 2, 3, 5, r, segs)) return nope(__LINE__);
    for (auto& s : segs) { int ri = P.carve(g0, s.first, 2, s.second, kd, RoomKind::GuestRoom); if (ri < 0) return nope(__LINE__); ids.push_back(ri); }
    // a narrow hostel puts more cells in its front corners, either side of the door
    if (ids.size() < 3)
      for (int side = 0; side < 2; side++) {
        int ri = P.carve(g0, side ? W - 5 : 1, H - 5, side ? W - 2 : 4, H - 2, RoomKind::GuestRoom);
        if (ri >= 0) ids.push_back(ri);
      }
    if (ids.size() < 3) return nope(__LINE__);
  } else {
    const int sto = P.carve(g0, 1, 2, std::min(4, k0 - 2), kd, RoomKind::Storeroom);
    if (sto < 0) return nope(__LINE__);
    ids.push_back(sto);
  }
  (void)kw;
  if (!g0.finish() || !barFits(P, g0) || !innKitchenDoor(P, g0, kit, r)) return nope(__LINE__);
  if (P.floors == 1) return letRooms(g0, ids, -1, ex, kd, r) && doorsToward(g0, ids, 0, ex, kd + 3, r);
  if (!g0.door(ids[0], 0, g0.rooms[(size_t)ids[0]].r.cx(), kd + 2, r)) return nope(__LINE__);
  const int sxU = innNook(P, k1 + 2, W - 3, r);
  return sxU >= 0 && innUpstairs(P, r, sxU, 2, kd);
}

// ---- sylvan (rect): a tree hall. The hall grows round a living trunk in its middle; the kitchen and bar at the back
// east; one storey: rooms walled off in the other three corners
bool planInnTreeHall(Plan& P, Rng& r) {
  P.tmpl = Tmpl::InnTreeHall;
  const int W = P.W, H = P.H, ex = P.ex;
  Geo& g0 = P.geo[0];
  P.initFloor(g0, RoomKind::Common);
  const int kd = 4, kw = 4 + r.irange(2);
  const int kit = innKitchenBar(P, g0, W - 1 - kw, W - 2, kd, false);
  if (kit < 0) return nope(__LINE__);
  std::vector<int> ids;
  if (P.floors == 1) {
    const int cw = 4 + r.irange(2), cd = 3 + (H >= 16 ? 1 : 0);
    int a = P.carve(g0, 1, 2, cw, kd, RoomKind::GuestRoom);
    int b = P.carve(g0, 1, H - 1 - cd, cw, H - 2, RoomKind::GuestRoom);
    int c = P.carve(g0, W - 1 - cw, H - 1 - cd, W - 2, H - 2, RoomKind::GuestRoom);
    if (a < 0 || b < 0 || c < 0 || cw + 2 >= ex - 1) return nope(__LINE__);
    ids = {a, b, c};
  }
  if (!g0.finish() || !barFits(P, g0) || !innKitchenDoor(P, g0, kit, r)) return nope(__LINE__);
  if (P.floors == 1) {
    if (!letRooms(g0, ids, 0, 0, 0, r)) return nope(__LINE__);
    for (int ri : ids) {
      const IRect& R = g0.rooms[(size_t)ri].r;
      if (!g0.door(ri, 0, R.x < ex ? R.x + R.w : R.x - 1, R.cy(), r) && !g0.door(ri, 0, R.cx(), R.y < 4 ? R.y + R.h + 1 : R.y - 1, r)) return nope(__LINE__);
    }
    return true;
  }
  const int sxU = innNook(P, 1, W - 3 - kw - 1, r);
  return sxU >= 0 && innUpstairs(P, r, sxU, 2, kd);
}

// ---- starspire: a spire inn. A narrow hall: the rented rooms in a file down the west wall, the kitchen and the bar
// at the back east, the innkeeper's room in the front east corner
bool planInnSpire(Plan& P, Rng& r) {
  P.tmpl = Tmpl::InnSpire;
  const int W = P.W, H = P.H;
  Geo& g0 = P.geo[0];
  P.initFloor(g0, RoomKind::Common);
  const int kd = 4, kw = 4 + r.irange(2);
  const int kit = innKitchenBar(P, g0, W - 1 - kw, W - 2, kd, false);
  if (kit < 0) return nope(__LINE__);
  std::vector<int> ids;
  if (P.floors == 1) {
    const int gw = 3 + r.irange(2);
    for (int y = 2; y + 2 <= H - 2; y += 5) {
      int ri = P.carve(g0, 1, y, gw, (y + 2 > H - 5 ? H - 2 : y + 2), RoomKind::GuestRoom);
      if (ri < 0) break;
      ids.push_back(ri);
    }
    const int ow = 4, od = 3;
    // (M3b fixer) a low hall (a long body): the innkeeper's room steps west of the bar's strip, clear of the counter
    int ox1 = W - 2;
    if (H - 3 - od <= P.barY + 3) ox1 = W - 1 - kw - 3;
    if (ox1 - ow + 1 <= P.ex + 2) return nope(__LINE__);
    int o = P.carve(g0, ox1 - ow + 1, H - 1 - od, ox1, H - 2, RoomKind::OwnerRoom);
    if (o < 0 || ids.size() < 2) return nope(__LINE__);
    if (!g0.finish() || !barFits(P, g0) || !innKitchenDoor(P, g0, kit, r)) return nope(__LINE__);
    for (int ri : ids) {
      const IRect& R = g0.rooms[(size_t)ri].r;
      if (!g0.door(ri, 0, R.x < P.ex ? R.x + R.w : R.x - 1, R.cy(), r)) return nope(__LINE__);
    }
    return g0.door(o, 0, g0.rooms[(size_t)o].r.cx(), H - 2 - od, r) || g0.door(o, 0, P.X(ox1 - ow), H - 3, r);
  }
  if (!g0.finish() || !barFits(P, g0) || !innKitchenDoor(P, g0, kit, r)) return nope(__LINE__);
  const int sxU = innNook(P, 1, W - 3 - kw - 1, r);
  return sxU >= 0 && innUpstairs(P, r, sxU, 2, kd);
}

// ---- steppe (rect): a felt tent inn. Felt booths down both side walls round the stove, the kitchen and the bar in
// the back west corner
bool planInnBooths(Plan& P, Rng& r) {
  P.tmpl = Tmpl::InnBooths;
  const int W = P.W, H = P.H;
  Geo& g0 = P.geo[0];
  P.initFloor(g0, RoomKind::Common);
  const int kd = 4, kw = 5;
  const int kit = innKitchenBar(P, g0, 1, kw, kd, true);
  if (kit < 0) return nope(__LINE__);
  std::vector<int> ids;
  const int bw = 3;
  for (int y = 2; y + 2 <= H - 2; y += 5) {   // the east wall's booths
    int ri = P.carve(g0, W - 1 - bw, y, W - 2, (y + 2 > H - 5 ? H - 2 : y + 2), RoomKind::GuestRoom);
    if (ri < 0) break;
    ids.push_back(ri);
  }
  for (int y = kd + 9; y + 2 <= H - 2; y += 5) {   // the west wall's, below the bar's strip
    int ri = P.carve(g0, 1, y, bw, y + 2, RoomKind::GuestRoom);
    if (ri < 0) break;
    ids.push_back(ri);
  }
  if (ids.size() < (P.floors == 1 ? 3u : 1u)) return nope(__LINE__);
  if (!g0.finish()) return nope(__LINE__);
  if (!barFits(P, g0)) return nope(__LINE__);
  if (!innKitchenDoor(P, g0, kit, r)) return nope(__LINE__);
  for (int ri : ids) {
    const IRect& R = g0.rooms[(size_t)ri].r;
    if (!g0.door(ri, 0, R.x < P.ex ? R.x + R.w : R.x - 1, R.cy(), r)) return nope(__LINE__);
  }
  if (P.floors == 1) return letRooms(g0, ids, -1, P.ex, H - 2, r);
  for (int ri : ids) g0.rooms[(size_t)ri].kind = RoomKind::Storeroom;
  const int sxU = innNook(P, kw + 3, W - 3 - bw - 1, r);
  return sxU >= 0 && innUpstairs(P, r, sxU, 2, kd);
}

// ---- a round inn (steppe yurt, sylvan tree hall): the kitchen in the back of the circle with the bar before it, felt
// alcoves walled off round the sides, each with its door; the stove (or the living trunk) in the middle
bool planInnYurt(Plan& P, Rng& r, bool tree) {
  P.tmpl = tree ? Tmpl::InnTreeHall : Tmpl::InnYurt;
  const int W = P.W, H = P.H, ex = P.ex;
  for (int f = 0; f < P.floors; f++) {
    Geo& g = P.geo[(size_t)f];
    P.initFloor(g, f == 0 ? RoomKind::Common : RoomKind::Corridor);
    int kit = -1;
    if (f == 0) {
      // (fix) one storey: the counter stays a tile clear of the side alcoves' doors (a counter row across the whole
      // middle blocked their doorways and the furnisher dropped it)
      kit = P.floors >= 2 ? innKitchenBar(P, g, ex - 3, ex, 4, false) : innKitchenBar(P, g, ex - 3, ex + 3, 4, r.f() < 0.5f);
      if (kit < 0) return nope(__LINE__);
      if (P.floors == 1) {   // (the counter and its flap drawn in a tile at each end, under the kitchen's wall still)
        const bool right = P.flapX > P.barX1;
        P.barX0++; P.barX1--;
        P.flapX += right ? -1 : 1;
      }
    }
    // alcoves round the sides: west and east, two deep when the circle is big
    std::vector<int> ids;
    const int aw = W >= 19 ? 4 : 3;
    const int cy = H / 2;
    // (a partition above needs three rows of floor over it; on the kitchen's floor the alcoves start below its wall)
    const int y0a = std::max(f == 0 ? 8 : 5, cy - 4);
    const int ys[2] = {y0a, y0a + 5};
    if (f > 0 || P.floors == 1)
      for (int side = 0; side < 2; side++)
        for (int k = 0; k < (H >= 17 ? 2 : 1); k++) {
          const int y0 = ys[k], y1 = y0 + 2;
          int ri = P.carve(g, side ? W - 1 - aw : 1, y0, side ? W - 2 : aw, y1, RoomKind::GuestRoom);
          if (ri >= 0) ids.push_back(ri);
        }
    if (!g.finish()) return nope(__LINE__);
    if (f == 0 && (!clipBar(P, g) || !barFits(P, g) || !innKitchenDoor(P, g, kit, r))) return nope(__LINE__);
    if (f > 0 || P.floors == 1) {
      if (!letRooms(g, ids, -1, ex, H - 2, r)) return nope(__LINE__);
      for (int ri : ids) {
        const IRect& R = g.rooms[(size_t)ri].r;
        if (!g.door(ri, 0, R.x < ex ? R.x + R.w : R.x - 1, R.cy(), r)) {
          return nope(__LINE__);
        }
      }
    }
  }
  for (int f = 0; f + 1 < P.floors; f++) {
    // the first flight climbs the back arc beside the kitchen; any higher ones wherever a wall lets them
    bool ok = false;
    if (f == 0) for (int x = ex + 2; x <= ex + 4 && !ok; x++) ok = setStairs(P, 0, P.X(x), 2);
    if (!ok && !stairsAnywhere(P, f, f % 2 ? 1 : -1, r)) return nope(__LINE__);
  }
  return true;
}

// ---- a courtyard inn (dune, imperial, river...): a caravanserai. The gate opens into the court (a well, troughs,
// the travellers' carts); the common room spans the back range with the kitchen at its end, the counter along its back
// wall; the rented rooms open onto the court down both sides; stables and a store flank the gate
bool planInnCaravanserai(Plan& P, Rng& r) {
  P.tmpl = Tmpl::InnCaravanserai;
  const int W = P.W, H = P.H, ex = P.ex;
  Geo& g0 = P.geo[0];
  P.initFloor(g0, RoomKind::Court);
  const int bd = std::max(6, P.cy0 - 3);   // the back range: rows 2..bd
  const int cy0 = bd + 3, cy1 = H - 7;     // the court
  if (cy1 - cy0 < 2) return nope(__LINE__);
  const int kw = 3 + r.irange(2), sw = 3 + r.irange(2);
  // the back range: the kitchen at the west end, the common room the rest
  const int kit = P.carve(g0, 1, 2, kw, bd, RoomKind::Kitchen);
  const int com = P.carve(g0, kw + 2, 2, W - 2, bd, RoomKind::Common);
  if (kit < 0 || com < 0) return nope(__LINE__);
  // the counter along the common room's back wall beside the kitchen
  P.barY = 2;
  P.barX0 = std::min(P.X(kw + 2), P.X(kw + 4)); P.barX1 = std::max(P.X(kw + 2), P.X(kw + 4));
  P.flapX = P.X(kw + 5);
  P.kd = 0;
  // the rented rooms down both sides of the court, the stables and the store either side of the gate
  std::vector<int> ids;
  for (int side = 0; side < 2; side++)
    for (int y = cy0; y + 2 <= cy1; y += 5) {
      int ri = P.carve(g0, side ? W - 1 - sw : 1, y, side ? W - 2 : sw, std::min(y + 2, cy1), RoomKind::GuestRoom);
      if (ri >= 0) ids.push_back(ri);
    }
  const int st0 = P.carve(g0, 1, H - 4, ex - 3, H - 2, RoomKind::Barn);
  const int st1 = P.carve(g0, ex + 3, H - 4, W - 2, H - 2, ids.size() < 3 ? RoomKind::GuestRoom : RoomKind::Storeroom);
  if (st1 >= 0 && ids.size() < 3) ids.push_back(st1);
  if (st0 < 0 || st1 < 0 || ids.size() < 3) return nope(__LINE__);
  if (!g0.finish() || !barFits(P, g0, com)) return nope(__LINE__);
  if (!g0.door(kit, com, P.X(kw + 1), 3, r) || g0.rooms[(size_t)kit].doorY != 3) return nope(__LINE__);
  if (!g0.door(com, 0, ex + (r.f() < 0.5f ? 2 : -2), bd + 2, r)) return nope(__LINE__);
  if (!letRooms(g0, ids, -1, ex, H - 2, r)) return nope(__LINE__);
  for (int ri : ids) {
    const IRect& R = g0.rooms[(size_t)ri].r;
    if (!g0.door(ri, 0, R.x < ex ? R.x + R.w : R.x - 1, R.cy(), r)) return nope(__LINE__);
  }
  if (!g0.door(st0, 0, g0.rooms[(size_t)st0].r.cx(), H - 5, r) || (g0.rooms[(size_t)st1].doorX < 0 && !g0.door(st1, 0, g0.rooms[(size_t)st1].r.cx(), H - 5, r))) return nope(__LINE__);
  return P.floors == 1;   // (one storey round its court)
}

// the inn for this building: by its blueprint's plan, then its people
bool planInnCultureOnce(Plan& P, Rng& r);
bool planInnCulture(Plan& P, Rng& r) {
  const Plan keep = P;
  for (int k = 0; k < 24; k++) {   // (M3b fixer: more draws before a people gives up its own plan)
    if (planInnCultureOnce(P, r)) return true;
    P = keep;
  }
  // a draw that does not fit its people's plan falls back to the heartland's on a plain body (never to no inn at all)
  P = keep;
  if (P.fp != bld::Floorplan::Rect && P.fp != bld::Floorplan::Long) return false;
  return planInnHeartland(P, r);
}
bool planInnCultureOnce(Plan& P, Rng& r) {
  using A = cult::Archetype;
  const A cu = P.culture >= 0 ? (A)P.culture : A::COUNT;
  // (M3b fixer) the people choose the inn, the footprint only its variant: a round body is a yurt or a tree hall
  // (no other plan fits a disc), a court a caravanserai only where the peoples build them round a court (the dune's,
  // the steppe's caravan roads, the imperial mansio, a heartland or river coaching inn); any other people's court inn
  // is its own plan behind the court; a long body is planned by its people (only the fjordfolk's is a mead hall)
  switch (P.fp) {
    case bld::Floorplan::Round: return planInnYurt(P, r, cu == A::Sylvan || cu == A::Starspire);
    case bld::Floorplan::Courtyard:
      if (P.floors == 1 && (cu == A::Dune || cu == A::Steppe || cu == A::Imperial || cu == A::Heartland || cu == A::River || cu == A::COUNT))
        return planInnCaravanserai(P, r);
      return planFrontCourt(P, r);
    case bld::Floorplan::L: return planEmbeddedL(P, r);
    default: break;
  }
  switch (cu) {
    case A::Fjordfolk: return planInnMeadHall(P, r);
    case A::Highland: return planInnDrovers(P, r);
    case A::Imperial: return planInnTaberna(P, r);
    case A::Dune: return planInnFondouk(P, r);
    case A::Steppe: return planInnBooths(P, r);
    case A::Marsh: return planInnStilt(P, r);
    case A::Jade: return planInnTeaHouse(P, r);
    case A::River: return planInnParlour(P, r);
    case A::SunTemple: return planInnHostel(P, r);
    case A::Sylvan: return planInnTreeHall(P, r);
    case A::Starspire: return planInnSpire(P, r);
    default: return planInnHeartland(P, r);
  }
}

// ---- HOUSE / STONE HOUSE: one storey = hall + private bedroom (cottage for the smallest); two = hall and kitchen
// below, bedrooms (and a study in a wealthy home) above
bool planHouse(Plan& P, Rng& r) {
  const int W = P.W, H = P.H;
  const Bldg& b = *P.b;
  Geo& g0 = P.geo[0];
  if (P.floors == 1) {
    if (b.type == Building::House && b.r.w <= 3 && b.r.h <= 2 && r.f() < 0.4f) {
      P.initFloor(g0, RoomKind::Cottage);
      P.var = 0;
      return g0.finish();
    }
    P.initFloor(g0, RoomKind::Hall);
    P.var = 1 + r.irange(W >= 13 ? 5 : 3);
    if (P.var == 3 && W < 13) P.var = 4;
    int bd = (H >= 12 && r.f() < 0.5f) ? 5 : 4;
    if (P.var == 4 || P.var == 5) {   // the bedroom in a front corner (its door in the partition facing the hall),
                                      // and in a wider home a pantry in the opposite back corner
      int bw = 3 + r.irange(2), d = 3 + (H >= 12 ? r.irange(2) : 0);
      int bed = P.carve(g0, 1, H - 1 - d, bw, H - 2, RoomKind::Bedroom);
      int pan = P.var == 5 ? P.carve(g0, W - 4, 2, W - 2, 4, r.f() < 0.5f ? RoomKind::Kitchen : RoomKind::Storeroom) : 0;
      if (bed < 0 || pan < 0 || !g0.finish() || g0.roomOf(P.ex, H - 2) != 0) return nope(__LINE__);
      const int dy = r.f() < 0.5f ? H - 2 - d : H - 3;   // (sequenced: the right argument's draw first, as MSVC/GCC)
      const int dx = r.f() < 0.5f ? P.X(2 + r.irange(bw - 1)) : P.X(bw + 1);
      if (!g0.door(bed, 0, dx, dy, r)) return nope(__LINE__);
      return P.var == 4 || g0.door(pan, 0, P.X(W - 3), 6, r);
    }
    if (P.var == 1) {   // a bedroom in a back corner, the hall wraps around it
      int bw = 3 + r.irange(2);
      int bed = P.carve(g0, 1, 2, bw, bd, RoomKind::Bedroom);
      if (bed < 0 || !g0.finish()) return nope(__LINE__);
      bool south = r.f() < 0.5f;
      return g0.door(bed, 0, south ? P.X(2 + r.irange(bw - 1)) : P.X(bw + 1), south ? bd + 2 : 3 + r.irange(bd - 2), r);
    }
    if (P.var == 2) {   // the bedroom along a side wall, the whole depth of the house
      int bw = 3 + (W >= 13 ? r.irange(2) : 0);
      int bed = P.carve(g0, 1, 2, bw, H - 2, RoomKind::Bedroom);
      if (bed < 0 || !g0.finish() || g0.roomOf(P.ex, H - 2) != 0) return nope(__LINE__);
      return g0.door(bed, 0, P.X(bw + 1), 3 + r.irange(std::max(1, H - 6)), r);
    }
    // the back strip: bedroom in one corner, a pantry or storeroom in the other, the hall and its hearth between
    int bw = 3 + r.irange(2), sw = 3;
    int bed = P.carve(g0, 1, 2, bw, bd, RoomKind::Bedroom);
    int sto = P.carve(g0, W - 1 - sw, 2, W - 2, bd, r.f() < 0.5f ? RoomKind::Storeroom : RoomKind::Kitchen);
    if (bed < 0 || sto < 0 || !g0.finish()) return nope(__LINE__);
    if ((W - 3 - sw) - (bw + 2) + 1 < 3) return nope(__LINE__);
    return g0.door(bed, 0, P.X(bw / 2 + 1), bd + 2, r) && g0.door(sto, 0, P.X(W - 2 - sw / 2), bd + 2, r);
  }
  // two storeys
  P.initFloor(g0, RoomKind::Hall);
  P.kd = 4;
  int kw = 4 + (W >= 13 ? r.irange(2) : 0);
  bool sto = W >= 13 && r.f() < 0.5f;
  int sw = 3;
  int kit = P.carve(g0, 1, 2, kw, P.kd, RoomKind::Kitchen);
  int st = sto ? P.carve(g0, W - 1 - sw, 2, W - 2, P.kd, RoomKind::Storeroom) : 0;
  if (kit < 0 || st < 0 || !g0.finish()) return nope(__LINE__);
  int al0 = kw + 2, al1 = sto ? W - 3 - sw : W - 2;
  int mode = r.f() < 0.5f ? 0 : 1;   // stairs under the kitchen's partition, or at the back wall of the hall
  int sxU = -1;
  if (mode == 0) sxU = 1 + r.irange(kw);
  else {
    std::vector<int> c;
    for (int x = al0; x + 1 <= al1; x++) if (nookOk(W, x, 3)) c.push_back(x);
    if (c.empty()) return nope(__LINE__);
    sxU = c[(size_t)r.irange((int)c.size())];
  }
  P.var = mode * 2 + (sto ? 1 : 0);
  Geo& g1 = P.geo[1];
  P.initFloor(g1, RoomKind::Corridor);
  std::vector<int> back, front;
  if (!carveRows(P, g1, P.kd, mode == 1 ? sxU : -1, -1, 3, 6, r, back, front) || !g1.finish()) return nope(__LINE__);
  if (!setStairs(P, 0, P.X(sxU), mode == 0 ? P.kd + 3 : 2)) return nope(__LINE__);
  // the parents take the widest room, a wealthy home keeps a study, the rest are the children's
  int big = back[0];
  for (int ri : back) if (g1.rooms[(size_t)ri].r.w > g1.rooms[(size_t)big].r.w) big = ri;
  bool study = P.wealth >= 2 && back.size() >= 3;
  for (int ri : back) g1.rooms[(size_t)ri].kind = RoomKind::Bedroom;
  if (study) for (int ri : back) if (ri != big) { g1.rooms[(size_t)ri].kind = RoomKind::Study; break; }
  if (!doorsTo(g1, back, r)) return nope(__LINE__);
  // ground doors: the kitchen into the hall; the storeroom too
  bool kSouth = mode == 1 ? r.f() < 0.6f : r.f() < 0.3f;
  if (!g0.door(kit, 0, kSouth ? P.X(2 + r.irange(std::max(1, kw - 2))) : P.X(kw + 1), kSouth ? P.kd + 2 : 3, r)) return nope(__LINE__);
  if (sto && !g0.door(st, 0, P.X(W - 3), P.kd + 2, r)) return nope(__LINE__);
  return true;
}

// ---- FARMHOUSE: hall and kitchen, a bedroom, the barn end
bool planFarm(Plan& P, Rng& r) {
  const int W = P.W, H = P.H;
  Geo& g0 = P.geo[0];
  P.initFloor(g0, RoomKind::Hall);
  int bw = 4 + r.irange(2);
  int hallW = W - 3 - bw;   // hall columns 1..hallW (unmirrored)
  int barn = P.carve(g0, W - 1 - bw, 2, W - 2, H - 2, RoomKind::Barn);
  if (barn < 0) return nope(__LINE__);
  bool frontBed = r.f() < 0.45f;
  int b2 = 3 + r.irange(2);
  if (!frontBed) b2 = std::min(b2, hallW - 4);
  if (b2 < 3) return nope(__LINE__);
  int bd = 4 + (H >= 12 ? r.irange(2) : 0);
  int bed = frontBed ? P.carve(g0, 1, H - 2 - (bd - 2), b2, H - 2, RoomKind::Bedroom) : P.carve(g0, 1, 2, b2, bd, RoomKind::Bedroom);
  if (bed < 0 || !g0.finish() || g0.roomOf(P.ex, H - 2) != 0) return nope(__LINE__);
  P.var = (frontBed ? 2 : 0) + (bw - 4);
  const IRect& BR = g0.rooms[(size_t)bed].r;
  if (!g0.door(bed, 0, r.f() < 0.5f ? P.X(b2 + 1) : BR.cx(), frontBed ? BR.y - 1 : BR.y + BR.h + 1, r)) return nope(__LINE__);
  return g0.door(barn, 0, P.X(W - 2 - bw), H / 2 + r.irange(3) - 1, r);
}

// ---- SMITHY: the forge hall, the forge against the back wall under the exterior's forge stack (east); sometimes the
// smith's bedroom or a store in the west
bool planSmithy(Plan& P, Rng& r) {
  const int W = P.W, H = P.H;
  P.mir = false;
  Geo& g0 = P.geo[0];
  P.initFloor(g0, RoomKind::Forge);
  // the forge's column under the stack: the corner, a step in, or (in a wide shop) two steps in
  { float f = r.f(); P.forgeX = f < 0.45f ? W - 3 : (f < 0.8f || W < 15 ? W - 4 : W - 5); }
  P.var = r.irange(9);
  if (P.var == 7 && W < 15) P.var = 1;
  if (P.var == 7) {   // the smith's household along the back west: a kitchen with its own hearth, the bedroom beside it
    int kw = 3 + (W >= 17 ? r.irange(2) : 0), bw = 3;
    if (kw + 1 + bw + 1 >= P.forgeX - 2) return nope(__LINE__);
    int kit = P.carve(g0, 1, 2, kw, 4, RoomKind::Kitchen);
    int bed = P.carve(g0, kw + 2, 2, kw + 1 + bw, 4, RoomKind::Bedroom);
    if (kit < 0 || bed < 0 || !g0.finish()) return nope(__LINE__);
    return g0.door(kit, 0, 2 + r.irange(kw - 1), 6, r) && g0.door(bed, 0, kw + 2 + r.irange(bw), 6, r);
  }
  if (P.var == 8) {   // the two front corners: the smith's bed by the street on the west, the iron store under the forge
    int bw = 3 + (W >= 15 ? r.irange(2) : 0), sw = 3, d = 3 + (H >= 12 ? r.irange(2) : 0);
    if (W - 2 - sw - 1 - (bw + 1) < 3) return nope(__LINE__);
    int bed = P.carve(g0, 1, H - 1 - d, bw, H - 2, RoomKind::Bedroom);
    int sto = P.carve(g0, W - 1 - sw, H - 1 - d, W - 2, H - 2, RoomKind::Storeroom);
    if (bed < 0 || sto < 0 || !g0.finish() || g0.roomOf(P.ex, H - 2) != 0) return nope(__LINE__);
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
    if (sto < 0 || bed < 0 || !g0.finish()) return nope(__LINE__);
    // (draws sequenced: the right argument's first, as MSVC/GCC evaluate them)
    const int sy = r.f() < 0.5f ? H - 3 : H - 2 - sd;
    const int sx = r.f() < 0.5f ? W - 2 - sw : W - 2 - r.irange(sw - 1);
    if (!g0.door(sto, 0, sx, sy, r)) return nope(__LINE__);
    if (!bedB) return true;
    const int by = r.f() < 0.5f ? 3 : 6;
    const int bx = r.f() < 0.5f ? bw + 1 : 2 + r.irange(bw - 1);
    return g0.door(bed, 0, bx, by, r);
  }
  if (P.var == 5) {   // the smith sleeps in a narrow room down the whole west side
    int bw = 3 + (W >= 15 ? r.irange(2) : 0);
    int bed = P.carve(g0, 1, 2, bw, H - 2, RoomKind::Bedroom);
    if (bed < 0 || !g0.finish() || g0.roomOf(P.ex, H - 2) != 0) return nope(__LINE__);
    return g0.door(bed, 0, bw + 1, 3 + r.irange(std::max(1, H - 6)), r);
  }
  if (P.var == 6) {   // a west wing the other way round: the store at the back, the bedroom by the street
    int bw = 3 + r.irange(2), sd = 3 + (H >= 12 ? r.irange(2) : 0);
    int sto = P.carve(g0, 1, 2, bw, 1 + sd, RoomKind::Storeroom);
    int bed = P.carve(g0, 1, sd + 4, bw, H - 2, RoomKind::Bedroom);
    if (sto < 0 || bed < 0 || !g0.finish()) return nope(__LINE__);
    const int sy = r.f() < 0.5f ? 3 : sd + 3;   // (sequenced: the right argument's draw first, as MSVC/GCC)
    const int sx = r.f() < 0.5f ? bw + 1 : 2;
    return g0.door(sto, 0, sx, sy, r) && g0.door(bed, 0, bw + 1, H - 3, r);
  }
  if (P.var == 3 && H < 12) P.var = 1;
  if (P.var == 3) {   // a west wing: the smith's bedroom at the back, the iron and coal store in front
    int bw = 3 + r.irange(2), bd = 4;
    int bed = P.carve(g0, 1, 2, bw, bd, RoomKind::Bedroom);
    int sto = P.carve(g0, 1, bd + 3, bw, H - 2, RoomKind::Storeroom);
    if (bed < 0 || sto < 0 || !g0.finish()) return nope(__LINE__);
    return g0.door(bed, 0, bw + 1, 3, r) && g0.door(sto, 0, bw + 1, H - 3, r);
  }
  if (P.var == 1) {
    int bw = 3 + r.irange(2), bd = 4;
    int bed = P.carve(g0, 1, 2, bw, bd, RoomKind::Bedroom);
    if (bed < 0 || !g0.finish()) return nope(__LINE__);
    bool south = r.f() < 0.5f;
    return g0.door(bed, 0, south ? 2 + r.irange(bw - 1) : bw + 1, south ? bd + 2 : 3, r);
  }
  if (P.var == 2) {
    int sw = 3 + (W >= 15 ? r.irange(2) : 0), sd = 3;
    int sto = P.carve(g0, 1, H - 1 - sd, sw, H - 2, RoomKind::Storeroom);
    if (sto < 0 || !g0.finish()) return nope(__LINE__);
    const int sy = r.f() < 0.5f ? H - 2 : H - 2 - sd;   // (sequenced: the right argument's draw first, as MSVC/GCC)
    const int sx = r.f() < 0.5f ? sw + 1 : 2;
    return g0.door(sto, 0, sx, sy, r);
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
  P.initFloor(g0, RoomKind::Shopfloor);
  bool bedroom = P.floors == 1 && b.hearth;
  int L = W - 2;
  int sw = bedroom ? 4 + r.irange(std::max(1, L - 1 - 3 - 4 + 1)) : L;
  if (bedroom && L - sw - 1 < 3) return nope(__LINE__);
  int sto = P.carve(g0, 1, 2, sw, kd, RoomKind::Stockroom);
  int bed = bedroom ? P.carve(g0, sw + 2, 2, W - 2, kd, RoomKind::Bedroom) : 0;
  if (sto < 0 || bed < 0 || !g0.finish()) return nope(__LINE__);
  P.var = bedroom ? 1 : 0;
  P.barY = kd + 3;
  P.barX0 = std::min(P.X(1), P.X(W - 3)); P.barX1 = std::max(P.X(1), P.X(W - 3));
  P.flapX = P.X(W - 2);
  if (!clipBar(P, g0)) return nope(__LINE__);
  int sxU = -1;
  if (P.floors >= 2) {
    std::vector<int> c;
    for (int x = 1; x + 1 <= sw; x++) if (nookOk(W, x, 3)) c.push_back(x);
    if (c.empty()) return nope(__LINE__);
    sxU = c[(size_t)r.irange((int)c.size())];
    Geo& g1 = P.geo[1];
    P.initFloor(g1, RoomKind::Hall);   // the family's living room upstairs
    std::vector<int> back, front;
    if (!carveRows(P, g1, kd, sxU, -1, 3, 6, r, back, front) || !g1.finish()) return nope(__LINE__);
    if (!setStairs(P, 0, P.X(sxU), 2)) return nope(__LINE__);
    int big = back[0];
    for (int ri : back) if (g1.rooms[(size_t)ri].r.w > g1.rooms[(size_t)big].r.w) big = ri;
    bool kitDone = false;
    for (int ri : back) {
      RoomKind k = RoomKind::Bedroom;
      if (ri == big) k = RoomKind::OwnerRoom;
      else if (!kitDone) { k = b.hearth ? RoomKind::Kitchen : RoomKind::Study; kitDone = true; }
      g1.rooms[(size_t)ri].kind = k;
    }
    if (!doorsTo(g1, back, r)) return nope(__LINE__);
    P.var = 2;
  }
  if (!g0.door(sto, 0, P.X(2 + r.irange(std::max(1, sw - 2))), kd + 2, r)) return nope(__LINE__);
  if (bedroom) {
    if (r.f() < 0.6f) { if (!g0.door(bed, sto, P.X(sw + 1), 3, r)) return nope(__LINE__); }
    else if (!g0.door(bed, 0, P.X(sw + 3), kd + 2, r)) return nope(__LINE__);
  }
  // the stockroom door opens behind the counter
  int dx = g0.rooms[(size_t)sto].doorX;
  return dx >= P.barX0 && dx <= P.barX1;
}

// ---- TEMPLE: the nave oriented to the altar, a vestry beside it, sometimes the priest's cell
// (M3b fixer round 3) each people's god-house keeps its side rooms its own way: the heartland church and the star
// temple their vestry and cell in the back corners (the star priests' a library); the highland kirk, the river church
// and the jade temple side chapels / monks' cells off the middle of the nave; the fjordfolk hof and the dune prayer
// hall one room (or none) so the hall stays open. The furnishing (furnishNave) is the people's own too.
bool planTempleSides(Plan& P, Rng& r, bool both, RoomKind a, RoomKind b) {
  const int W = P.W, H = P.H;
  Geo& g0 = P.geo[0];
  P.initFloor(g0, RoomKind::Nave);
  const int vw = 3 + (W >= 19 ? 1 : 0);
  const int y0 = 5 + (H >= 17 ? 1 : 0), y1 = std::min(H - 5, y0 + 3 + (H >= 16 ? r.irange(2) : 0));
  if (y1 - y0 < 2 || vw + 3 >= P.ex - 1) return nope(__LINE__);
  const bool west = r.f() < 0.5f;
  const int ra = P.carve(g0, west ? 1 : W - 1 - vw, y0, west ? vw : W - 2, y1, a);
  const int rb = both ? P.carve(g0, west ? W - 1 - vw : 1, y0, west ? W - 2 : vw, y1, b) : 0;
  if (ra < 0 || rb < 0 || !g0.finish()) return nope(__LINE__);
  for (int x = P.ex - 2; x <= P.ex + 2; x++) if (g0.roomOf(x, 2) != 0) return nope(__LINE__);
  for (int ri : {ra, rb}) {
    if (ri <= 0) continue;
    const IRect& R = g0.rooms[(size_t)ri].r;
    if (!g0.door(ri, 0, R.x < P.ex ? R.x + R.w : R.x - 1, R.cy(), r)) return nope(__LINE__);
  }
  P.var = (both ? 1 : 0) + (west ? 2 : 0);
  return true;
}

bool planTemple(Plan& P, Rng& r) {
  using A = cult::Archetype;
  switch ((A)(P.culture >= 0 ? P.culture : (int)A::COUNT)) {
    case A::Highland: if (planTempleSides(P, r, false, RoomKind::Vestry, RoomKind::Vestry)) return true; break;
    case A::River: if (planTempleSides(P, r, true, RoomKind::Vestry, RoomKind::Study)) return true; break;
    case A::Jade: if (planTempleSides(P, r, true, RoomKind::Vestry, RoomKind::Bedroom)) return true; break;
    case A::Fjordfolk: case A::Dune: case A::Steppe: case A::Marsh: case A::Sylvan: case A::Imperial: {
      // one open hall: the priest's things kept behind screens in the nave (furnishNave)
      Geo& g = P.geo[0];
      P.initFloor(g, RoomKind::Nave);
      if (g.finish()) { P.var = 4; return true; }
      break;
    }
    default: break;
  }
  const int W = P.W, H = P.H;
  Geo& g0 = P.geo[0];
  P.initFloor(g0, RoomKind::Nave);
  int vw = 3 + (W >= 17 ? r.irange(2) : 0);
  int vd = 4 + (H >= 15 ? r.irange(2) : 0);
  int ves = P.carve(g0, 1, 2, vw, vd, RoomKind::Vestry);
  bool cell = W >= 17 || r.f() < 0.5f;
  int bw = 3;
  // (the star priests' second room is their library of star charts)
  int bed = cell ? P.carve(g0, W - 1 - bw, 2, W - 2, vd, P.culture == (int)cult::Archetype::Starspire ? RoomKind::Study : RoomKind::Bedroom) : 0;
  if (ves < 0 || bed < 0 || !g0.finish()) return nope(__LINE__);
  for (int x = P.ex - 2; x <= P.ex + 2; x++) if (g0.roomOf(x, 2) != 0) return nope(__LINE__);
  P.var = (cell ? 1 : 0) + (vw - 3) * 2;
  const int vy = r.f() < 0.5f ? 3 : vd + 2;   // (sequenced: the right argument's draw first, as MSVC/GCC)
  const int vx = r.f() < 0.5f ? P.X(vw + 1) : P.X(2);
  if (!g0.door(ves, 0, vx, vy, r)) return nope(__LINE__);
  if (cell && !g0.door(bed, 0, P.X(W - 2 - bw), vd + 2, r)) return nope(__LINE__);
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
  P.initFloor(g0, RoomKind::ThroneHall);
  const int scheme = r.irange(4);
  struct DoorReq { int ri, px, py; };
  std::vector<DoorReq> dq;
  auto south = [&](int ri) { const IRect& R = g0.rooms[(size_t)ri].r; return DoorReq{ri, R.x + 1 + r.irange(std::max(1, R.w - 2)), R.y + R.h + 1}; };
  auto east = [&](int ri) { const IRect& R = g0.rooms[(size_t)ri].r; bool e = R.x < ex; return DoorReq{ri, e ? R.x + R.w : R.x - 1, R.y + 1 + r.irange(std::max(1, R.h - 2))}; };
  auto north = [&](int ri) { const IRect& R = g0.rooms[(size_t)ri].r; return DoorReq{ri, R.x + 1 + r.irange(std::max(1, R.w - 2)), R.y - 2}; };
  int kw = 4 + r.irange(3), kd = 4 + r.irange(2);
  int kit = P.carve(g0, W - 1 - kw, 2, W - 2, kd, RoomKind::Kitchen);
  if (kit < 0) return nope(__LINE__);
  switch (scheme) {
    case 0: {
      int bw = 5 + r.irange(2), bd = 5 + (H >= 16 ? r.irange(2) : 0);
      int bar = P.carve(g0, 1, 2, bw, bd, RoomKind::Barracks);
      bool store = H >= 16 && bd <= H - 11 && r.f() < 0.5f;
      int sto = store ? P.carve(g0, 1, H - 4, 4 + r.irange(2), H - 2, RoomKind::Storeroom) : 0;
      if (bar < 0 || sto < 0 || !g0.finish()) return nope(__LINE__);
      dq.push_back(r.f() < 0.5f ? south(bar) : east(bar));
      if (store) dq.push_back(r.f() < 0.5f ? north(sto) : east(sto));
      break;
    }
    case 1: {
      int bw = 4 + r.irange(2), aw = 3, pw = 3;
      int bar = P.carve(g0, 1, 2, bw, kd, RoomKind::Barracks);
      int arm = P.carve(g0, bw + 2, 2, bw + 1 + aw, kd, RoomKind::Storeroom);
      int pan = P.carve(g0, W - 3 - kw - pw, 2, W - 3 - kw, kd, RoomKind::Storeroom);
      if (bar < 0 || arm < 0 || pan < 0 || !g0.finish()) return nope(__LINE__);
      dq.push_back(south(bar)); dq.push_back(south(arm)); dq.push_back(south(pan));
      break;
    }
    case 2: {
      int ww = 5 + r.irange(2), mid = 5 + r.irange(std::max(1, H - 11));
      int bar = P.carve(g0, 1, 2, ww, mid, RoomKind::Barracks);
      int arm = P.carve(g0, 1, mid + 3, ww, H - 2, RoomKind::Storeroom);
      if (bar < 0 || arm < 0 || !g0.finish()) return nope(__LINE__);
      dq.push_back(east(bar)); dq.push_back(east(arm));
      break;
    }
    default: {
      int gw = 5 + r.irange(2), gd = 3 + r.irange(2);
      int grd = P.carve(g0, 1, H - 1 - gd, gw, H - 2, RoomKind::Barracks);
      bool rec = r.f() < 0.6f;
      int other;
      if (rec) {   // (sequenced: the right argument's draw first, as MSVC/GCC evaluate them)
        const int y1 = 4 + r.irange(2);
        const int x1 = 4 + r.irange(2);
        other = P.carve(g0, 1, 2, x1, y1, RoomKind::Study);
      } else {
        other = P.carve(g0, W - 1 - (4 + r.irange(2)), H - 4, W - 2, H - 2, RoomKind::Storeroom);
      }
      if (grd < 0 || other < 0 || !g0.finish()) return nope(__LINE__);
      dq.push_back(r.f() < 0.5f ? north(grd) : east(grd));
      dq.push_back(rec ? (r.f() < 0.5f ? south(other) : east(other)) : (r.f() < 0.5f ? north(other) : east(other)));
      break;
    }
  }
  dq.push_back(r.f() < 0.5f ? south(kit) : east(kit));
  for (int x = ex - 3; x <= ex + 3; x++) if (g0.roomOf(x, 2) != 0) return nope(__LINE__);
  for (int y = 3; y <= H - 2; y++) if (g0.roomOf(ex, y) != 0) return nope(__LINE__);   // the aisle from the gate to the throne
  int us = r.irange(3);
  P.var = scheme * 4 + us;
  if (P.floors >= 2) {
    Geo& g1 = P.geo[1];
    P.initFloor(g1, RoomKind::Corridor);
    std::vector<int> ids;
    if (us == 0) {   // rooms along a gallery
      int ud = 4 + r.irange(2);
      bool dbl = ud + 7 <= H - 4 && r.f() < 0.75f;
      std::vector<int> back, front;
      if (!carveRows(P, g1, ud, -1, dbl ? ud + 7 : -1, 5, 9, r, back, front)) return nope(__LINE__);
      ids = back; ids.insert(ids.end(), front.begin(), front.end());
    } else if (us == 1) {   // the lord's wing and the household along the back
      int lw = 6 + r.irange(3), ud = 4 + r.irange(2);
      int lord = P.carve(g1, 1, 2, lw, H - 2, RoomKind::OwnerRoom);
      if (lord < 0) return nope(__LINE__);
      ids.push_back(lord);
      std::vector<std::pair<int, int>> segs;
      if (!splitSeg(lw + 2, W - 2, 4, 7, r, segs)) return nope(__LINE__);
      for (auto& sg : segs) { int ri = P.carve(g1, sg.first, 2, sg.second, ud, RoomKind::Bedroom); if (ri < 0) return nope(__LINE__); ids.push_back(ri); }
    } else {   // two wings of two rooms
      int sw = 5 + r.irange(2), m1 = 5 + r.irange(std::max(1, H - 11)), m2 = 5 + r.irange(std::max(1, H - 11));
      int a = P.carve(g1, 1, 2, sw, m1, RoomKind::Bedroom), b = P.carve(g1, 1, m1 + 3, sw, H - 2, RoomKind::Bedroom);
      int c = P.carve(g1, W - 1 - sw, 2, W - 2, m2, RoomKind::Bedroom), d = P.carve(g1, W - 1 - sw, m2 + 3, W - 2, H - 2, RoomKind::Bedroom);
      if (a < 0 || b < 0 || c < 0 || d < 0) return nope(__LINE__);
      ids = {a, b, c, d};
      if (W - 2 * sw - 4 >= 9 && r.f() < 0.6f) {   // a study at the back of the gallery, between the wings
        int s0 = sw + 3, s1 = W - 4 - sw;
        int st = P.carve(g1, s0 + 1, 2, s1 - 1, 4, RoomKind::Study);
        if (st >= 0) ids.push_back(st);
      }
    }
    if (!g1.finish()) return nope(__LINE__);
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
    if (!study) return nope(__LINE__);
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
    if (!ok) return nope(__LINE__);
    for (int ri : ids) {
      const IRect& R = g1.rooms[(size_t)ri].r;
      if (!g1.door(ri, 0, R.x + 1 + r.irange(std::max(1, R.w - 2)), R.y + R.h / 2, r)) return nope(__LINE__);
    }
  }
  for (const DoorReq& d : dq)
    if (!g0.door(d.ri, 0, d.px, d.py, r)) return nope(__LINE__);
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
  P.initFloor(g0, RoomKind::ThroneHall);
  const int ww = 6 + r.irange(3), kd = 5 + r.irange(2), gd = 4 + r.irange(2);
  if (ww + 2 > ex - 6 || H - 3 - gd <= kd + 3) return nope(__LINE__);
  int kit = P.carve(g0, 1, 2, ww, kd, RoomKind::Kitchen);
  int grd = P.carve(g0, 1, H - 1 - gd, ww, H - 2, RoomKind::Barracks);
  int rec = P.carve(g0, W - 1 - ww, 2, W - 2, kd, r.f() < 0.5f ? RoomKind::Study : RoomKind::Storeroom);
  int arm = P.carve(g0, W - 1 - ww, H - 1 - gd, W - 2, H - 2, RoomKind::Storeroom);
  if (kit < 0 || grd < 0 || rec < 0 || arm < 0 || !g0.finish()) return nope(__LINE__);
  for (int x = ex - 3; x <= ex + 3; x++) if (g0.roomOf(x, 2) != 0) return nope(__LINE__);
  for (int y = 3; y <= H - 2; y++) if (g0.roomOf(ex, y) != 0) return nope(__LINE__);   // the aisle from the doors to the throne
  if (P.floors < 2) {   // (M3b) a palace of one storey: the ruler's chamber is the back wing
    g0.rooms[(size_t)rec].kind = RoomKind::OwnerRoom;
    if (!g0.door(kit, 0, P.X(2 + r.irange(std::max(1, ww - 2))), kd + 2, r)) return nope(__LINE__);
    if (!g0.door(grd, 0, P.X(2 + r.irange(std::max(1, ww - 2))), H - 2 - gd, r)) return nope(__LINE__);
    if (!g0.door(rec, 0, P.X(W - 3 - r.irange(std::max(1, ww - 2))), kd + 2, r)) return nope(__LINE__);
    return g0.door(arm, 0, P.X(W - 3 - r.irange(std::max(1, ww - 2))), H - 2 - gd, r);
  }
  // the stairs (unmirrored column): the hall's back bay between a wing and the dais
  std::vector<int> cand;
  for (int x = ww + 2; x + 1 <= ex - 5; x++) if (nookOk(W, x, 5)) cand.push_back(x);
  for (int x = ex + 4; x + 1 <= W - ww - 3; x++) if (nookOk(W, x, 5)) cand.push_back(x);
  if (cand.empty()) return nope(__LINE__);
  const int sxU = cand[(size_t)r.irange((int)cand.size())];
  // upstairs: the royal apartments
  Geo& g1 = P.geo[1];
  P.initFloor(g1, RoomKind::Corridor);
  const int ud = 5 + r.irange(2);
  const bool dbl = ud + 7 <= H - 5;
  std::vector<int> back, front;
  if (!carveRows(P, g1, ud, sxU, dbl ? ud + 7 : -1, 5, 9, r, back, front)) return nope(__LINE__);
  if (!g1.finish()) return nope(__LINE__);
  std::vector<int> ids = back;
  ids.insert(ids.end(), front.begin(), front.end());
  if (ids.size() < 4) return nope(__LINE__);
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
  if (!setStairs(P, 0, P.X(sxU), 2)) return nope(__LINE__);
  if (!doorsTo(g1, back, r) || !doorsTo(g1, front, r)) return nope(__LINE__);
  // the ground floor's doors into the hall
  if (!g0.door(kit, 0, P.X(2 + r.irange(std::max(1, ww - 2))), kd + 2, r)) return nope(__LINE__);
  if (!g0.door(grd, 0, P.X(2 + r.irange(std::max(1, ww - 2))), H - 2 - gd, r)) return nope(__LINE__);
  if (!g0.door(rec, 0, P.X(W - 3 - r.irange(std::max(1, ww - 2))), kd + 2, r)) return nope(__LINE__);
  if (!g0.door(arm, 0, P.X(W - 3 - r.irange(std::max(1, ww - 2))), H - 2 - gd, r)) return nope(__LINE__);
  P.var = ww * 16 + kd * 4 + gd + (dbl ? 1000 : 0);
  return true;
}

// ---- BARRACKS (M1): the guard's hall below (the mess table, the hearth, weapon racks) with the armoury and a kitchen at
// the back; upstairs the dormitories of bunks and the captain's room along a corridor.
bool planBarracks(Plan& P, Rng& r) {
  const int W = P.W, H = P.H;
  Geo& g0 = P.geo[0];
  P.initFloor(g0, RoomKind::Hall);
  const int aw = 4 + r.irange(2), ad = 4 + r.irange(2);
  const bool kitchen = W >= 19 && r.f() < 0.65f;
  const int kw = 4 + r.irange(2);
  int arm = P.carve(g0, 1, 2, aw, ad, RoomKind::Storeroom);
  int kit = kitchen ? P.carve(g0, W - 1 - kw, 2, W - 2, ad, RoomKind::Kitchen) : 0;
  if (arm < 0 || kit < 0) return nope(__LINE__);
  if (P.floors < 2) {   // (M3b) a barracks of one storey sleeps its guards in a dormitory at the back
    // (carved BEFORE the floor is finished: finish() hands every left-over tile to the hall, so a room carved after it
    // would leave the hall's bounds and the room table out of step)
    const int dw = 5 + r.irange(2);
    const int dx0 = W - 1 - dw - (kitchen ? kw + 1 : 0);
    if (dx0 <= aw + 2) return nope(__LINE__);
    const int dorm = P.carve(g0, dx0, 2, W - 2 - (kitchen ? kw + 1 : 0), ad, RoomKind::Barracks);
    if (dorm < 0 || !g0.finish()) return nope(__LINE__);
    if (!g0.door(arm, 0, P.X(2 + r.irange(std::max(1, aw - 2))), ad + 2, r)) return nope(__LINE__);
    if (kitchen && !g0.door(kit, 0, P.X(W - 3 - r.irange(std::max(1, kw - 2))), ad + 2, r)) return nope(__LINE__);
    return g0.door(dorm, 0, g0.rooms[(size_t)dorm].r.cx(), ad + 2, r);
  }
  if (!g0.finish()) return nope(__LINE__);
  std::vector<int> cand;
  for (int x = aw + 2; x + 1 <= (kitchen ? W - 3 - kw : W - 2); x++) if (nookOk(W, x, 4)) cand.push_back(x);
  if (cand.empty()) return nope(__LINE__);
  const int sxU = cand[(size_t)r.irange((int)cand.size())];
  Geo& g1 = P.geo[1];
  P.initFloor(g1, RoomKind::Corridor);
  const int ud = 4 + r.irange(2);
  const bool dbl = ud + 7 <= H - 4;
  std::vector<int> back, front;
  if (!carveRows(P, g1, ud, sxU, dbl ? ud + 7 : -1, 4, 7, r, back, front)) return nope(__LINE__);
  if (!g1.finish()) return nope(__LINE__);
  std::vector<int> ids = back;
  ids.insert(ids.end(), front.begin(), front.end());
  if (ids.size() < 2) return nope(__LINE__);
  int cap = ids[0];
  for (int ri : ids) {
    g1.rooms[(size_t)ri].kind = RoomKind::Barracks;
    const IRect &a = g1.rooms[(size_t)ri].r, &b = g1.rooms[(size_t)cap].r;
    if (a.w * a.h < b.w * b.h) cap = ri;   // the captain keeps the smallest room to himself
  }
  g1.rooms[(size_t)cap].kind = RoomKind::OwnerRoom;
  if (!setStairs(P, 0, P.X(sxU), 2)) return nope(__LINE__);
  if (!doorsTo(g1, back, r) || !doorsTo(g1, front, r)) return nope(__LINE__);
  if (!g0.door(arm, 0, P.X(2 + r.irange(std::max(1, aw - 2))), ad + 2, r)) return nope(__LINE__);
  if (kitchen && !g0.door(kit, 0, P.X(W - 3 - r.irange(std::max(1, kw - 2))), ad + 2, r)) return nope(__LINE__);
  P.var = aw * 8 + ad + (kitchen ? 100 : 0) + (dbl ? 1000 : 0);
  return true;
}

// ---- TOWER: one round room per floor (laboratory, library, the mage's bedroom), stairs along the back wall
bool planTower(Plan& P, Rng& r) {
  bool closet = r.f() < 0.45f;
  // M3b: the floors take the blueprint's shape: a round tower (or windmill) is a round room on every floor (the walls
  // follow the circle, the corners beyond are void); the stairs climb the back arc, west and east by turns. A closet
  // for the stores in a front quarter of the ground floor.
  const int H = P.H;
  for (int f = 0; f < P.floors; f++) {
    Geo& g = P.geo[(size_t)f];
    P.initFloor(g, mainKindOf(P.type, f, P.floors));
    if (f == 0 && closet && P.carve(g, 1, H - 5, 4, H - 2, RoomKind::Storeroom) < 0) closet = false;
    if (!g.finish()) return nope(__LINE__);
  }
  for (int f = 0; f + 1 < P.floors; f++)
    if (!stairsAnywhere(P, f, (f % 2 == 0) == !P.mir ? -1 : 1, r)) return nope(__LINE__);
  if (closet) {
    const IRect& R = P.geo[0].rooms[1].r;
    if (!P.geo[0].door(1, 0, R.cx(), R.y - 1, r)) return nope(__LINE__);
  }
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
  if (v == 0) { P.initFloor(g0, RoomKind::Cottage); return g0.finish(); }
  if (v == 1) {   // a sleeping alcove in a back corner, the rest is the living room
    P.initFloor(g0, RoomKind::Hall);
    int bw = 3 + (W >= 11 ? r.irange(2) : 0);
    int bed = P.carve(g0, 1, 2, bw, 4, RoomKind::Bedroom);
    if (bed < 0 || !g0.finish()) return nope(__LINE__);
    bool south = r.f() < 0.5f;
    return g0.door(bed, 0, south ? P.X(2 + r.irange(bw - 1)) : P.X(bw + 1), south ? 6 : 3, r);
  }
  if (v == 2) {   // the bed behind a wall down one side
    P.initFloor(g0, RoomKind::Hall);
    int bed = P.carve(g0, 1, 2, 3, H - 2, RoomKind::Bedroom);
    if (bed < 0 || !g0.finish() || g0.roomOf(P.ex, H - 2) != 0) return nope(__LINE__);
    return g0.door(bed, 0, P.X(4), 3 + r.irange(std::max(1, H - 6)), r);
  }
  if (v == 3) {   // one room, and a pantry in a back corner
    P.initFloor(g0, RoomKind::Cottage);
    int pan = P.carve(g0, W - 4, 2, W - 2, 4, RoomKind::Storeroom);
    if (pan < 0 || !g0.finish()) return nope(__LINE__);
    const int py = r.f() < 0.5f ? 6 : 3;   // (sequenced: the right argument's draw first, as MSVC/GCC)
    const int px = r.f() < 0.5f ? P.X(W - 3) : P.X(W - 5);
    return g0.door(pan, 0, px, py, r);
  }
  // one room for the family and a byre down one side for the goat and the hens
  P.initFloor(g0, RoomKind::Cottage);
  int byre = P.carve(g0, W - 4, 2, W - 2, H - 2, b.owner == Role::Farmer || r.f() < 0.6f ? RoomKind::Barn : RoomKind::Storeroom);
  if (byre < 0 || !g0.finish() || g0.roomOf(P.ex, H - 2) != 0) return nope(__LINE__);
  return g0.door(byre, 0, P.X(W - 5), 3 + r.irange(std::max(1, H - 6)), r);
}

// ---- (M1 economy) WORK HALL: the mill's, the sawmill's or the tannery's hall (the granary's store), a store or the
// keeper's room walled off in a back corner, its door onto the hall
bool planWorkshop(Plan& P, Rng& r) {
  Geo& g0 = P.geo[0];
  P.initFloor(g0, mainKindOf(P.type, 0, 1));
  if (P.type == Building::Granary && r.f() < 0.6f) { P.var = 0; return g0.finish(); }   // one great store of grain
  const int bw = 3 + r.irange(2), bd = 4 + (P.H >= 12 ? r.irange(2) : 0);
  const RoomKind k = P.type == Building::Tanner && r.f() < 0.5f ? RoomKind::Bedroom : (P.type == Building::Granary ? RoomKind::OwnerRoom : RoomKind::Storeroom);
  const int ri = P.carve(g0, 1, 2, bw, bd, k);
  if (ri < 0 || !g0.finish() || g0.roomOf(P.ex, P.H - 2) != 0) return nope(__LINE__);
  const bool south = r.f() < 0.5f;
  P.var = 1 + (south ? 1 : 0) + (bw - 3) * 2 + (bd - 4) * 4;
  return g0.door(ri, 0, south ? P.X(2 + r.irange(bw - 1)) : P.X(bw + 1), south ? bd + 2 : 3, r);
}

// ===================================================================================================== M3b shapes
bool planClassic(Plan& T, Rng& r);

// stairs from floor f of a carved room's back wall (a hall at the back of a court): any tile of room ri under the back
// wall that lines up with the floor above
bool stairsInRoom(Plan& P, int f, int ri, Rng& r) {
  const Geo& g = P.geo[(size_t)f];
  std::vector<int> xs;
  for (int x = 1; x < P.W - 1; x++) if (g.roomOf(x, 2) == ri && std::abs(x - P.ex) > 3) xs.push_back(x);
  for (size_t i = xs.size(); i > 1; i--) std::swap(xs[i - 1], xs[(size_t)r.irange((int)i)]);
  for (int x : xs) if (setStairs(P, f, x, 2, true)) return true;
  return nope(__LINE__);
}

// ---- a round home (a steppe yurt, a rondavel, an elven pod): one room under the roof's crown, the stove or the fire
// in its middle, the beds and mats round the wall; a second storey (an elven pod's) is the sleeping loft
bool planRoundHome(Plan& P, Rng& r) {
  P.tmpl = Tmpl::RoundHome;
  for (int f = 0; f < P.floors; f++) {
    Geo& g = P.geo[(size_t)f];
    P.initFloor(g, f == 0 ? (P.floors == 1 ? RoomKind::Cottage : RoomKind::Hall) : RoomKind::Bedroom);
    if (!g.finish()) return nope(__LINE__);
  }
  for (int f = 0; f + 1 < P.floors; f++)
    if (!stairsAnywhere(P, f, f % 2 ? 1 : -1, r)) return nope(__LINE__);
  P.var = P.mir ? 1 : 0;
  return true;
}

// ---- a round body for the purposes 15.7 checks closely: the purpose's main room fills the circle, its rooms walled
// off round the sides. A shop keeps its stockroom across the back of the circle with the counter before it; a round
// keep or palace its throne at the back of the circle, the kitchen and the guardroom at the sides, the lord's rooms
// upstairs; a round smithy its forge on the back arc; a round barracks its armoury below and bunks above.
bool planRoundStrict(Plan& P, Rng& r) {
  P.tmpl = Tmpl::RoundHall;
  const Building t = P.type;
  const bool shop = t == Building::Shop || t == Building::Bakery || t == Building::Butcher || t == Building::Fishmonger || t == Building::Weaver ||
                    t == Building::Exchange;
  const bool keep = t == Building::Keep || t == Building::Palace;
  const int W = P.W, H = P.H, ex = P.ex, cy = H / 2;
  const int aw = W >= 19 ? 4 : 3;
  int sto = -1;
  for (int f = 0; f < P.floors; f++) {
    Geo& g = P.geo[(size_t)f];
    RoomKind mk = mainKindOf(t, 0, P.floors);
    if (f > 0) mk = t == Building::Barracks ? RoomKind::Barracks : (keep ? RoomKind::Corridor : RoomKind::Hall);
    P.initFloor(g, mk);
    if (f == 0 && shop && (sto = P.carve(g, 1, 2, W - 2, 4, RoomKind::Stockroom)) < 0) return nope(__LINE__);
    std::vector<RoomKind> kinds;
    if (keep) kinds = f == 0 ? (P.floors == 1 ? std::vector<RoomKind>{RoomKind::Kitchen, RoomKind::Barracks, RoomKind::OwnerRoom}
                                                : std::vector<RoomKind>{RoomKind::Kitchen, RoomKind::Barracks})
                             : std::vector<RoomKind>{RoomKind::OwnerRoom, RoomKind::Study, RoomKind::Bedroom, RoomKind::Council};
    // (fix) a round barracks of one storey (a steppe riders' tent) sleeps its guards in two felt-walled alcoves either
    // side of the hall, the arms store beside them
    else if (t == Building::Barracks) kinds = f == 0 ? (P.floors == 1 ? std::vector<RoomKind>{RoomKind::Barracks, RoomKind::Barracks, RoomKind::Storeroom}
                                                                      : std::vector<RoomKind>{RoomKind::Storeroom})
                                                     : std::vector<RoomKind>{RoomKind::OwnerRoom};
    else if (shop) { if (f > 0) kinds = {RoomKind::OwnerRoom, P.b->hearth ? RoomKind::Kitchen : RoomKind::Bedroom}; }
    else kinds = {RoomKind::Storeroom};
    std::vector<int> ids;
    const int ys[2] = {std::max(5, cy - 4), std::max(5, cy - 4) + 5};   // (a partition above needs three rows of floor over it)
    for (size_t k = 0; k < kinds.size() && k < 4; k++) {
      const int side = (int)(k % 2), y0 = ys[k / 2];
      int ri = P.carve(g, side ? W - 1 - aw : 1, y0, side ? W - 2 : aw, y0 + 2, kinds[k]);
      if (ri < 0) { if (k < 2) return nope(__LINE__); continue; }
      ids.push_back(ri);
    }
    if (!g.finish()) return nope(__LINE__);
    for (int ri : ids) {
      const IRect& R = g.rooms[(size_t)ri].r;
      if (!g.door(ri, 0, R.x < ex ? R.x + R.w : R.x - 1, R.cy(), r)) return nope(__LINE__);
    }
    if (f == 0 && keep) for (int x = ex - 1; x <= ex + 1; x++) if (g.roomOf(x, 2) != 0) return nope(__LINE__);
  }
  if (shop) {
    Geo& g0 = P.geo[0];
    P.kd = 4;
    P.barY = 7;
    P.barX0 = std::min(P.X(1), P.X(W - 3)); P.barX1 = std::max(P.X(1), P.X(W - 3));
    P.flapX = P.X(W - 2);
    if (!clipBar(P, g0) || !barFits(P, g0)) return nope(__LINE__);
    if (!g0.door(sto, 0, (P.barX0 + P.barX1) / 2, 6, r)) return nope(__LINE__);
    const int dx = g0.rooms[(size_t)sto].doorX;
    if (dx < P.barX0 || dx > P.barX1) return nope(__LINE__);
  }
  for (int f = 0; f + 1 < P.floors; f++) {
    bool ok = false;
    if (f == 0 && shop)
      for (int x = ex - 2; x <= ex + 2 && !ok; x++) ok = P.geo[0].roomOf(x, 2) == sto && setStairs(P, 0, x, 2, false);
    if (!ok && !stairsAnywhere(P, f, f % 2 ? 1 : -1, r)) return nope(__LINE__);
  }
  return true;
}

// ---- a round hall for any other purpose on a round body: the purpose's main room fills the circle; a store walled off
// in the back of it when the circle is big; round rooms upstairs (a study, a bedroom) reached by the back arc
bool planRoundHall(Plan& P, Rng& r) {
  P.tmpl = Tmpl::RoundHall;
  const int W = P.W;
  for (int f = 0; f < P.floors; f++) {
    Geo& g = P.geo[(size_t)f];
    P.initFloor(g, f == 0 ? mainKindOf(P.type, 0, P.floors) : (f == P.floors - 1 ? RoomKind::Bedroom : RoomKind::Study));
    if (f == 0 && W >= 15 && r.f() < 0.6f) {
      const int side = r.f() < 0.5f ? 1 : W - 5;
      const int ri = P.carve(g, side, P.H / 2 - 1, side + 3, P.H / 2 + 1, RoomKind::Storeroom);
      if (ri >= 0 && !g.finish()) return nope(__LINE__);
      if (ri >= 0) {
        const IRect& R = g.rooms[(size_t)ri].r;
        if (!g.door(ri, 0, R.x < P.ex ? R.x + R.w : R.x - 1, R.cy(), r)) return nope(__LINE__);
        continue;
      }
    }
    if (!g.finish()) return nope(__LINE__);
  }
  for (int f = 0; f + 1 < P.floors; f++)
    if (!stairsAnywhere(P, f, f % 2 ? 1 : -1, r)) return nope(__LINE__);
  return true;
}

// ---- a courtyard house (dune, imperial domus, jade siheyuan): the door leads through the front range into an open
// court (paving, plants, a well or a fountain); the family's hall faces it across the back with the bedrooms and the
// kitchen beside it, rooms down the sides and either side of the way in, every door onto the court. A second storey
// is a loft over the back range.
bool planCourtHouse(Plan& P, Rng& r) {
  P.tmpl = Tmpl::CourtHouse;
  const int W = P.W, H = P.H, ex = P.ex;
  Geo& g0 = P.geo[0];
  P.initFloor(g0, RoomKind::Court);
  const int bd = P.cy0 - 3, fy = P.cy1 + 3;
  if (bd < 4 || fy > H - 4) return nope(__LINE__);
  std::vector<std::pair<int, int>> segs;
  if (!splitSeg(1, W - 2, 4, 9, r, segs) || segs.size() < 2) return nope(__LINE__);
  std::vector<int> back, rest;
  for (auto& s : segs) { int ri = P.carve(g0, s.first, 2, s.second, bd, RoomKind::Bedroom); if (ri < 0) return nope(__LINE__); back.push_back(ri); }
  // the hall is the back room on the court's axis (or the widest), the kitchen at one end
  int hall = back[0];
  for (int ri : back) { const IRect& R = g0.rooms[(size_t)ri].r; if (R.x <= ex && R.x + R.w > ex) hall = ri; }
  g0.rooms[(size_t)hall].kind = RoomKind::Hall;
  int kit = hall == back.front() ? back.back() : back.front();
  g0.rooms[(size_t)kit].kind = RoomKind::Kitchen;
  // the side ranges: one room each side when there is width for it
  const int sw = P.cx0 - 2;
  if (sw >= 3) {
    int a = P.carve(g0, 1, P.cy0, sw, P.cy1, r.f() < 0.5f ? RoomKind::Storeroom : RoomKind::Bedroom);
    int b = P.carve(g0, P.cx1 + 2, P.cy0, W - 2, P.cy1, P.wealth >= 2 ? RoomKind::Study : RoomKind::Storeroom);
    if (a >= 0) rest.push_back(a);
    if (b >= 0) rest.push_back(b);
  }
  // the front range either side of the way in
  for (int side = 0; side < 2; side++) {
    int ri = P.carve(g0, side ? ex + 3 : 1, fy, side ? W - 2 : ex - 3, H - 2, side ? RoomKind::Storeroom : (P.b->owner == Role::Merchant ? RoomKind::Stockroom : RoomKind::Workshop));
    if (ri < 0) return nope(__LINE__);
    rest.push_back(ri);
  }
  if (!g0.finish()) return nope(__LINE__);
  // every door onto the court where the room meets it; a corner room that does not opens into its neighbour (the hall,
  // the kitchen or a store)
  for (int ri : back) if (!doorsToward(g0, {ri}, 0, ex, P.cy0, r) && !g0.door(ri, hall, ex, 3, r) && !g0.door(ri, kit, ex, 3, r)) return nope(__LINE__);
  for (int ri : rest) {
    if (doorsToward(g0, {ri}, 0, ex, (P.cy0 + P.cy1) / 2, r)) continue;
    bool ok = false;
    for (int j = 1; j < (int)g0.rooms.size() && !ok; j++)
      if (j != ri && !roomPrivate(g0.rooms[(size_t)j].kind)) ok = g0.door(ri, j, ex, (P.cy0 + P.cy1) / 2, r);
    if (!ok) return nope(__LINE__);
  }
  if (P.floors >= 2) {
    // the loft over the back range: one long room, reached from the hall's back wall
    Geo& g1 = P.geo[1];
    g1.initShape(W, H, RoomKind::Bedroom, [&](int, int y) { return y <= bd; });
    if (!g1.finish() || !stairsInRoom(P, 0, hall, r)) return nope(__LINE__);
    for (int f = 2; f < P.floors; f++) {
      P.geo[(size_t)f].initShape(W, H, RoomKind::Study, [&](int, int y) { return y <= bd; });
      if (!P.geo[(size_t)f].finish() || !stairsAnywhere(P, f - 1, 0, r)) return nope(__LINE__);
    }
  }
  return true;
}

// ---- an L house: the yard corner is outside; the wing (the arm away from the door) is its own room (a bedroom, the
// kitchen, a farm's byre), the hall the rest. Upstairs the wing is a bedroom too, the landing the rest.
bool planLHouse(Plan& P, Rng& r) {
  P.tmpl = Tmpl::LHouse;
  const int W = P.W;
  RoomKind wingKind = P.type == Building::Farmhouse ? RoomKind::Barn : (P.floors >= 2 && r.f() < 0.5f ? RoomKind::Kitchen : RoomKind::Bedroom);
  if (P.type == Building::Shop) wingKind = RoomKind::Stockroom;
  // a one-storey home whose wing is not its bedroom sleeps in its hall (a farm's byre wing: the family's beds by the fire)
  const RoomKind mainK = P.floors == 1 && wingKind != RoomKind::Bedroom ? RoomKind::Cottage : RoomKind::Hall;
  int wx0, wx1, wy1;
  if (P.lCorner >= 2) { wx0 = P.lx0; wx1 = P.lx1; wy1 = P.ly0 - 1; }                                   // over the front yard
  else { wx0 = P.lCorner == 0 ? P.lx1 + 1 : 1; wx1 = P.lCorner == 0 ? W - 2 : P.lx0 - 1; wy1 = P.ly1; }   // beside the back yard
  for (int f = 0; f < P.floors; f++) {
    Geo& g = P.geo[(size_t)f];
    P.initFloor(g, f == 0 ? mainK : RoomKind::Corridor);
    const int wing = g.carve(wx0, 2, wx1, wy1, f == 0 ? wingKind : RoomKind::Bedroom, true);
    if (wing < 0) return nope(__LINE__);
    // upstairs a second bedroom in a front corner of the main arm (the one that is not the yard's)
    int extra = 0;
    if (f > 0) {
      const bool west = P.lCorner == 3 || P.lCorner == 0;
      extra = g.carve(west ? 1 : W - 5, P.H - 5, west ? 4 : W - 2, P.H - 2, RoomKind::Bedroom, true);
      if (extra < 0) return nope(__LINE__);
    }
    if (!g.finish()) return nope(__LINE__);
    const IRect& R = g.rooms[(size_t)wing].r;
    if (!g.door(wing, 0, R.x + R.w / 2, R.y + R.h + 1, r) && !g.door(wing, 0, R.x < P.ex ? R.x + R.w : R.x - 1, R.cy(), r)) return nope(__LINE__);
    if (extra) {
      const IRect& E = g.rooms[(size_t)extra].r;
      if (!g.door(extra, 0, E.cx(), E.y - 1, r) && !g.door(extra, 0, E.x < P.ex ? E.x + E.w : E.x - 1, E.cy(), r)) return nope(__LINE__);
    }
  }
  for (int f = 0; f + 1 < P.floors; f++)
    if (!stairsAnywhere(P, f, 0, r)) return nope(__LINE__);
  return true;
}

// ---- a longhouse (fjordfolk, highland, marsh): one long room round the long hearth down its middle, the family's
// beds by the walls, the byre at one end under the same roof (farmers), a store at the other; a loft above
bool planLonghouse(Plan& P, Rng& r) {
  P.tmpl = Tmpl::Longhouse;
  const int W = P.W, H = P.H;
  Geo& g0 = P.geo[0];
  P.initFloor(g0, P.floors == 1 ? RoomKind::Cottage : RoomKind::Hall);
  const bool byre = P.b->owner == Role::Farmer || P.type == Building::Farmhouse || r.f() < 0.5f;
  const int bw = 4 + r.irange(2);
  int by = 0;
  if (byre && (by = P.carve(g0, W - 1 - bw, 2, W - 2, H - 2, RoomKind::Barn)) < 0) return nope(__LINE__);
  if (!g0.finish()) return nope(__LINE__);
  P.pitY = H / 2;
  const int e = byre ? W - 1 - bw - 3 : W - 4;
  P.pitX0 = std::min(P.X(3), P.X(e)); P.pitX1 = std::max(P.X(3), P.X(e));
  if (by && !g0.door(by, 0, P.X(W - 2 - bw), P.pitY + 2, r)) return nope(__LINE__);
  if (!trenchCulture(P.culture)) P.pitY = -1;   // (M3b fixer) the southern and marsh peoples' fire is on the hearth
  for (int f = 1; f < P.floors; f++) {
    P.initFloor(P.geo[(size_t)f], RoomKind::Bedroom);
    if (!P.geo[(size_t)f].finish() || !stairsAnywhere(P, f - 1, f % 2 ? -1 : 1, r)) return nope(__LINE__);
  }
  return true;
}

// ---- a cross temple: the nave from the door to the altar, the transepts across it near the altar end; the vestry in
// one arm's end, a side chapel (the priest's study) in the other
bool planCrossTemple(Plan& P, Rng& r) {
  P.tmpl = Tmpl::CrossTemple;
  const int W = P.W;
  Geo& g0 = P.geo[0];
  P.initFloor(g0, RoomKind::Nave);
  const int aw = std::min(4, P.ex - P.naveHW - 3);
  if (aw < 3) return nope(__LINE__);
  const int ves = P.carve(g0, 1, P.tr0, aw, P.tr1, RoomKind::Vestry);
  const int cha = r.f() < 0.6f ? P.carve(g0, W - 1 - aw, P.tr0, W - 2, P.tr1, RoomKind::Study) : 0;
  if (ves < 0 || cha < 0 || !g0.finish()) return nope(__LINE__);
  if (!g0.door(ves, 0, P.X(aw + 1), (P.tr0 + P.tr1) / 2, r)) return nope(__LINE__);
  if (cha && !g0.door(cha, 0, P.X(W - 2 - aw), (P.tr0 + P.tr1) / 2, r)) return nope(__LINE__);
  return P.floors == 1;
}

// ---- a round temple (a tholos, a felt shrine, a living-wood grove hall): one round nave about the altar's axis. (M3b
// fixer round 3) no walled vestry: a rectangle of partitions against a curved wall ends in a straight cut short of
// the shell; the priest's things stand behind folding screens in the nave instead (furnishNave)
bool planRoundTemple(Plan& P, Rng& r) {
  (void)r;
  P.tmpl = Tmpl::RoundTemple;
  Geo& g0 = P.geo[0];
  P.initFloor(g0, RoomKind::Nave);
  if (!g0.finish()) return nope(__LINE__);
  for (int x = P.ex - 1; x <= P.ex + 1; x++) if (g0.roomOf(x, 2) != 0) return nope(__LINE__);
  return P.floors == 1;
}

// ---- a front court (any purpose on a courtyard blueprint without a court plan of its own): the building's own plan
// behind an open court; the gate in the front wall opens into the court, the court's door into the building, a stable
// or a store walled off at a side of the court with its door onto it
bool planFrontCourt(Plan& T, Rng& r) {
  const int W = T.W, H = T.H, ex = T.ex;
  const int cd = std::clamp((H - 3) * (T.courtH ? T.courtH : 6) / 16, 4, 6);
  Plan S = T;
  S.fp = bld::Floorplan::Rect;
  S.H = H - cd - 1;   // the building: rows 0..S.H-1; its front wall row becomes the partition's cap
  if (S.H < 10) return nope(__LINE__);
  S.geo.assign((size_t)S.floors, Geo());
  if (!planFor(S, r)) return nope(__LINE__);
  if (S.geo[0].roomOf(ex, S.H - 2) != 0) return nope(__LINE__);
  for (int f = 0; f < S.floors; f++) {
    const Geo& g = S.geo[(size_t)f];
    if ((f + 1 < S.floors && !g.up.valid()) || (f > 0 && !g.down.valid())) return nope(__LINE__);
    for (size_t i = 1; i < g.rooms.size(); i++) if (g.rooms[i].doorX < 0) return nope(__LINE__);
  }
  const Tmpl inner = S.tmpl;
  T = S;
  T.H = H;
  T.fp = bld::Floorplan::Courtyard;
  T.tmpl = Tmpl::FrontCourt;
  (void)inner;
  for (int f = 0; f < T.floors; f++) {
    const Geo& s = S.geo[(size_t)f];
    Geo& g = T.geo[(size_t)f];
    g.H = H;
    g.wall.assign((size_t)W * H, f == 0 ? T_SHELL : T_VOID);
    g.room.assign((size_t)W * H, -1);
    for (int i = 0; i < W * S.H; i++) { g.wall[(size_t)i] = s.wall[(size_t)i]; g.room[(size_t)i] = s.room[(size_t)i]; }
    if (f > 0) {   // the upper floors stand over the building only: the shell's front row, void beyond
      for (int x = 0; x < W; x++) if (g.wall[(size_t)g.I(x, S.H - 1)] == T_VOID) g.wall[(size_t)g.I(x, S.H - 1)] = T_SHELL;
      g.shaped = true;
      continue;
    }
    for (int x = 1; x < W - 1; x++) {
      g.wall[(size_t)g.I(x, S.H - 1)] = T_PART;
      g.wall[(size_t)g.I(x, S.H)] = T_PART;
      for (int y = S.H + 1; y <= H - 2; y++) g.wall[(size_t)g.I(x, y)] = T_FLOOR;
    }
  }
  Geo& g0 = T.geo[0];
  // a stable or a store against one side of the court (as deep as the court: the building's front wall is its back)
  const int sd = cd - 1, side = r.f() < 0.5f;
  std::vector<int> sides;
  const int sx0 = side ? W - 5 : 1, sx1 = side ? W - 2 : 4;
  if (std::abs(sx0 - ex) > 3 && std::abs(sx1 - ex) > 3) {
    const int ri = g0.carve(sx0, H - 1 - sd, sx1, H - 2, T.type == Building::Farmhouse || T.type == Building::Inn ? RoomKind::Barn : RoomKind::Storeroom, false);
    if (ri >= 0) sides.push_back(ri);
  }
  // the court: the rest of the front rows, its door the doorway through the building's front wall
  RoomDef C;
  C.kind = RoomKind::Court;
  C.carved = true;
  g0.rooms.push_back(C);
  const int ci = (int)g0.rooms.size() - 1;
  for (int y = S.H + 1; y <= H - 2; y++)
    for (int x = 1; x < W - 1; x++)
      if (g0.isFloor(x, y) && g0.roomOf(x, y) < 0) g0.room[(size_t)g0.I(x, y)] = (int8_t)ci;
  for (int y = S.H - 1; y <= S.H; y++) { g0.wall[(size_t)g0.I(ex, y)] = T_FLOOR; g0.room[(size_t)g0.I(ex, y)] = (int8_t)ci; g0.doorTiles.push_back(g0.I(ex, y)); }
  g0.approach.push_back(g0.I(ex, S.H - 2)); g0.approach.push_back(g0.I(ex, S.H + 1));
  g0.rooms[(size_t)ci].doorX = ex; g0.rooms[(size_t)ci].doorY = S.H; g0.rooms[(size_t)ci].doorH = true;
  g0.bbox();
  for (int ri : sides) {
    const IRect& R = g0.rooms[(size_t)ri].r;
    if (!g0.door(ri, ci, R.cx(), R.y - 1, r) && !g0.door(ri, ci, R.x < ex ? R.x + R.w : R.x - 1, R.cy(), r)) return nope(__LINE__);
  }
  T.cx0 = 1; T.cx1 = W - 2; T.cy0 = S.H + 1; T.cy1 = H - 2;
  return true;
}

// ---- an L for any purpose: the purpose's own plan (its template, whatever it is) on the L's body, the biggest
// rectangle with the front door in it, embedded in the floor; the wing (the arm beyond the yard corner) walled off as a
// room of its own on every floor, its door into the body's main room
bool planFor(Plan& T, Rng& r);
bool planEmbeddedL(Plan& T, Rng& r) {
  const int W = T.W, H = T.H;
  // the body [bx0..bx1] x [by0..H-2] (floor tiles) and the wing [wx0..wx1] x [2..wy1] in T's coordinates; the wall
  // between them is the body plan's own outer wall
  int bx0 = 1, bx1 = W - 2, by0 = 2, wx0, wx1, wy1;
  if (T.lCorner >= 2) {   // a front corner is the yard: the body is the leg with the door, the wing the back part beside it
    if (T.lCorner == 3) { bx1 = T.lx0 - 1; wx0 = T.lx0 + 1; wx1 = W - 2; }
    else { bx0 = T.lx1 + 1; wx0 = 1; wx1 = T.lx1 - 1; }
    wy1 = T.ly0 - 1;
  } else {                // a back corner is the yard: the body is the front part, the wing the back arm beside the yard
    by0 = T.ly1 + 3;
    if (T.lCorner == 0) { wx0 = T.lx1 + 1; wx1 = W - 2; } else { wx0 = 1; wx1 = T.lx0 - 1; }
    wy1 = T.ly1;
  }
  if (wx1 - wx0 < 1 || wy1 < 3 || bx1 - bx0 < 8 || H - 2 - by0 < 7) return nope(__LINE__);
  Plan S = T;
  S.fp = bld::Floorplan::Rect;
  S.W = bx1 - bx0 + 3;
  S.H = H - by0 + 2;
  const int dx = bx0 - 1, dy = by0 - 2;
  S.ex = T.ex - dx;
  if (S.ex < 3 || S.ex > S.W - 4) return nope(__LINE__);
  S.geo.assign((size_t)S.floors, Geo());
  if (!planFor(S, r)) return nope(__LINE__);
  if (S.geo[0].roomOf(S.ex, S.H - 2) < 0) return nope(__LINE__);
  for (int f = 0; f < S.floors; f++) {
    const Geo& g = S.geo[(size_t)f];
    if ((f + 1 < S.floors && !g.up.valid()) || (f > 0 && !g.down.valid())) return nope(__LINE__);
    for (size_t i = 1; i < g.rooms.size(); i++) if (g.rooms[i].doorX < 0) return nope(__LINE__);
  }
  const Tmpl inner = S.tmpl;
  const Plan Ts = T;   // the L's own parameters
  T = S;
  T.W = W; T.H = H; T.ex = Ts.ex; T.fp = bld::Floorplan::L; T.lCorner = Ts.lCorner;
  T.lx0 = Ts.lx0; T.lx1 = Ts.lx1; T.ly0 = Ts.ly0; T.ly1 = Ts.ly1;
  T.tmpl = inner;
  auto mapI = [&](int si) { return (si / S.W + dy) * W + si % S.W + dx; };
  auto mapSt = [&](Stairs s) { if (s.valid()) { s.x += dx; s.y += dy; s.ax += dx; s.ay += dy; } return s; };
  if (T.barY >= 0) { T.barY += dy; T.barX0 += dx; T.barX1 += dx; T.flapX += dx; }
  if (T.forgeX >= 0) T.forgeX += dx;
  if (T.pitY >= 0) { T.pitY += dy; T.pitX0 += dx; T.pitX1 += dx; }
  if (T.cy0 >= 0) { T.cx0 += dx; T.cx1 += dx; T.cy0 += dy; T.cy1 += dy; }
  for (int& t : T.pool) t = mapI(t);
  for (int f = 0; f < T.floors; f++) {
    const Geo& s = S.geo[(size_t)f];
    Geo& g = T.geo[(size_t)f];
    Ts.initFloor(g, s.rooms[0].kind);   // the L's shell and void
    g.rooms = s.rooms;
    for (RoomDef& R : g.rooms) { R.r.x += dx; R.r.y += dy; if (R.doorX >= 0) { R.doorX += dx; R.doorY += dy; } if (R.bedX >= 0) { R.bedX += dx; R.bedY += dy; } }
    for (int y = 0; y < S.H; y++)
      for (int x = 0; x < S.W; x++) {
        const int ti = (y + dy) * W + x + dx;
        const uint8_t st = s.wall[(size_t)(y * S.W + x)];
        if (st == T_FLOOR || st == T_PART) { g.wall[(size_t)ti] = st; g.room[(size_t)ti] = s.room[(size_t)(y * S.W + x)]; }
        else if (g.wall[(size_t)ti] == T_FLOOR) {
          // the body's own outer wall: a partition where the wing lies beyond it, the shell where the yard does
          const int tx = x + dx, ty = y + dy;
          const bool part = tx >= wx0 - 1 && tx <= wx1 + 1 && ty <= wy1 + 2;
          g.wall[(size_t)ti] = part ? T_PART : T_SHELL;
          g.room[(size_t)ti] = -1;
        }
      }
    g.doorTiles.clear(); g.approach.clear(); g.keep.clear();
    for (int t : s.doorTiles) g.doorTiles.push_back(mapI(t));
    for (int t : s.approach) g.approach.push_back(mapI(t));
    for (int t : s.keep) g.keep.push_back(mapI(t));
    g.up = mapSt(s.up); g.down = mapSt(s.down);
    g.up2 = s.up2 >= 0 ? mapI(s.up2) : -1; g.down2 = s.down2 >= 0 ? mapI(s.down2) : -1;
    // the wing: a room of its own (a store below, a bedroom above; a farm's byre), its door into the body
    RoomKind wk = f > 0 ? RoomKind::Bedroom : RoomKind::Storeroom;
    if (f == 0 && T.type == Building::Farmhouse) wk = RoomKind::Barn;
    if (f == 0 && (T.type == Building::Inn || T.type == Building::TeaHouse)) wk = RoomKind::Parlour;
    RoomDef Wr;
    Wr.kind = wk;
    Wr.carved = true;
    g.rooms.push_back(Wr);
    const int wi = (int)g.rooms.size() - 1;
    for (int y = 2; y <= wy1; y++)
      for (int x = wx0; x <= wx1; x++)
        if (g.wall[(size_t)g.I(x, y)] == T_FLOOR && g.room[(size_t)g.I(x, y)] < 0) g.room[(size_t)g.I(x, y)] = (int8_t)wi;
    g.shaped = true;
    for (int i = 0; i < W * H; i++) if (g.wall[(size_t)i] == T_FLOOR && g.room[(size_t)i] < 0) return nope(__LINE__);   // nothing left over
    g.bbox();
    const IRect& R = g.rooms[(size_t)wi].r;
    if (R.w < 2 || R.h < 2) return nope(__LINE__);
    // the door: through the wall to the body, into a public room (the main room first)
    bool ok = false;
    for (int pass = 0; pass < 2 && !ok; pass++)   // (a public room first; else the room it backs onto: a store off a stockroom)
      for (int target = 0; target < (int)s.rooms.size() && !ok; target++) {
        if (target && pass == 0 && roomPrivate(g.rooms[(size_t)target].kind)) continue;
        if (pass == 1 && (g.rooms[(size_t)target].kind == RoomKind::GuestRoom || g.rooms[(size_t)target].kind == RoomKind::OwnerRoom)) continue;
        ok = Ts.lCorner >= 2 ? g.door(wi, target, Ts.lCorner == 3 ? R.x - 1 : R.x + R.w, R.cy(), r) : g.door(wi, target, R.cx(), R.y + R.h + 1, r);
      }
    if (!ok) return nope(__LINE__);
  }
  return true;
}

// ---- the plain plan on a shaped floor: one room per floor; an L's wing walled off as a room of its own
bool planShapedPlain(Plan& P, Rng& r) {
  P.tmpl = Tmpl::Plain;
  for (int f = 0; f < P.floors; f++) {
    Geo& g = P.geo[(size_t)f];
    P.initFloor(g, f == 0 ? mainKindOf(P.type, 0, P.floors) : (P.type == Building::Tower ? RoomKind::Study : RoomKind::Bedroom));
    int wing = -1;
    if (P.fp == bld::Floorplan::L) {
      int wx0, wx1, wy1;
      if (P.lCorner >= 2) { wx0 = P.lx0; wx1 = P.lx1; wy1 = P.ly0 - 1; }
      else { wx0 = P.lCorner == 0 ? P.lx1 + 1 : 1; wx1 = P.lCorner == 0 ? P.W - 2 : P.lx0 - 1; wy1 = P.ly1; }
      wing = g.carve(wx0, 2, wx1, wy1, f == 0 ? RoomKind::Storeroom : RoomKind::Bedroom, true);
    }
    if (!g.finish()) return nope(__LINE__);
    if (wing >= 0) {
      const IRect& R = g.rooms[(size_t)wing].r;
      if (!g.door(wing, 0, R.x + R.w / 2, R.y + R.h + 1, r) && !g.door(wing, 0, R.x < P.ex ? R.x + R.w : R.x - 1, R.cy(), r)) return nope(__LINE__);
    }
  }
  for (int f = 0; f + 1 < P.floors; f++)
    if (!stairsAnywhere(P, f, f % 2 ? 1 : -1, r, true)) return nope(__LINE__);
  return true;
}

// ===================================================================================================== M3b purposes
// rooms along a corridor on every floor above the ground (a guildhall's, an exchange's, a lodge's upper floors): the
// kinds dealt in turn from `kinds` (the biggest room takes the first), the flight up at (sxU, sy) (unmirrored column)
bool upperRooms(Plan& P, Rng& r, int sxU, int sy, int kd, std::initializer_list<RoomKind> kinds) {
  for (int f = 1; f < P.floors; f++) {
    Geo& g1 = P.geo[(size_t)f];
    P.initFloor(g1, RoomKind::Corridor);
    std::vector<int> back, front;
    const bool dbl = kd + 7 <= P.H - 3;
    // the landing over the flight reaches the back wall (no room right over the stairs)
    int nook = f == 1 ? std::min(sxU, P.W - 3) : -1;
    if (nook >= 0 && !nookOk(P.W, nook, 4)) nook = nookOk(P.W, nook - 1, 4) ? nook - 1 : -1;
    if (!carveRows(P, g1, kd, nook, dbl ? kd + 7 : -1, 4, 7, r, back, front) &&
        (P.initFloor(g1, RoomKind::Corridor), back.clear(), front.clear(), !carveRows(P, g1, kd, nook, dbl ? kd + 7 : -1, 3, 6, r, back, front)))
      return nope(__LINE__);
    if (!g1.finish()) return nope(__LINE__);
    if (f == 1) { if (!setStairs(P, 0, P.X(sxU), sy)) return nope(__LINE__); }
    else if (!stairsAnywhere(P, f - 1, f % 2 ? 1 : -1, r)) return nope(__LINE__);
    std::vector<int> all = back;
    all.insert(all.end(), front.begin(), front.end());
    std::stable_sort(all.begin(), all.end(), [&](int a, int b) { return g1.rooms[(size_t)a].r.w * g1.rooms[(size_t)a].r.h > g1.rooms[(size_t)b].r.w * g1.rooms[(size_t)b].r.h; });
    size_t k = 0;
    for (int ri : all) g1.rooms[(size_t)ri].kind = *(kinds.begin() + (k++ % kinds.size()));
    if (!doorsTo(g1, back, r) || !doorsTo(g1, front, r)) return nope(__LINE__);
  }
  return true;
}

// ---- a guildhall: the guild's meeting hall (the masters' long table, the guild's banners and chests), the clerk's
// records and the guild's strongroom at the back; upstairs the masters' council chamber and the warden's rooms. The
// doge's (a merchant republic's seat): the hall is his council chamber, his chair at the head of the table on the axis.
bool planGuildhall(Plan& P, Rng& r, Tmpl t) {
  P.tmpl = t;
  const int W = P.W;
  Geo& g0 = P.geo[0];
  P.initFloor(g0, t == Tmpl::SeatDoge ? RoomKind::ThroneHall : RoomKind::Assembly);
  const int kd = 4, sw = 3 + r.irange(2);
  const int stu = P.carve(g0, 1, 2, sw, kd, RoomKind::Study);
  const int sto = P.carve(g0, W - 1 - sw, 2, W - 2, kd, RoomKind::Storeroom);
  // (M3b fixer) the doge's hall is the republic's counting house too: the notaries' office and the merchants' waiting
  // parlour walled off either side of the doors
  std::vector<int> front;
  if (t == Tmpl::SeatDoge && P.H >= 15) {
    const int fw = 4 + r.irange(2), fd = 3 + (P.H >= 18 ? 1 : 0);
    if (fw + 2 < P.ex - 3) {
      const int a = P.carve(g0, 1, P.H - 1 - fd, fw, P.H - 2, RoomKind::Study);
      const int b = P.carve(g0, W - 1 - fw, P.H - 1 - fd, W - 2, P.H - 2, RoomKind::Parlour);
      if (a < 0 || b < 0) return nope(__LINE__);
      front = {a, b};
    }
  }
  if (stu < 0 || sto < 0 || !g0.finish()) return nope(__LINE__);
  for (int ri : front) {
    const IRect& R = g0.rooms[(size_t)ri].r;
    if (!g0.door(ri, 0, R.x < P.ex ? R.x + R.w : R.x - 1, R.cy(), r) && !g0.door(ri, 0, R.cx(), R.y - 1, r)) return nope(__LINE__);
  }
  for (int x = P.ex - 2; x <= P.ex + 2; x++) if (g0.roomOf(x, 2) != 0) return nope(__LINE__);
  if (P.floors >= 2) {
    int sxU = r.f() < 0.5f ? innNook(P, sw + 2, P.ex - 4, r) : -1;   // either side of the head of the table
    if (sxU < 0) sxU = innNook(P, P.ex + 4, P.W - 3 - sw, r);
    if (sxU < 0) sxU = innNook(P, sw + 2, P.ex - 4, r);
    if (sxU < 0 || !upperRooms(P, r, sxU, 2, kd,
                               {RoomKind::Council, t == Tmpl::SeatDoge ? RoomKind::OwnerRoom : RoomKind::Study, RoomKind::Bedroom, RoomKind::Study, RoomKind::Storeroom}))
      return nope(__LINE__);
  }
  if (!g0.door(stu, 0, g0.rooms[(size_t)stu].r.cx(), kd + 2, r) || !g0.door(sto, 0, g0.rooms[(size_t)sto].r.cx(), kd + 2, r)) return nope(__LINE__);
  return true;
}

// ---- an exchange: the trading floor, its counters along the back with the strongroom behind them (the scales, the
// ledgers, the coin on the counter), the factor's office at one end; ledger desks for the clerks about the floor; the
// factors' rooms upstairs
bool planExchange(Plan& P, Rng& r) {
  P.tmpl = Tmpl::Exchange;
  const int W = P.W;
  Geo& g0 = P.geo[0];
  P.initFloor(g0, RoomKind::Trading);
  const int kd = 4, ow = P.floors >= 2 ? 5 : 4;
  const int vault = P.carve(g0, 1, 2, W - 3 - ow, kd, RoomKind::Stockroom);
  const int off = P.carve(g0, W - 1 - ow, 2, W - 2, kd, RoomKind::Study);
  if (vault < 0 || off < 0 || !g0.finish()) return nope(__LINE__);
  P.kd = kd;
  P.barY = kd + 3;
  P.barX0 = std::min(P.X(1), P.X(W - 5 - ow)); P.barX1 = std::max(P.X(1), P.X(W - 5 - ow));
  P.flapX = P.X(W - 4 - ow);
  if (!barFits(P, g0)) return nope(__LINE__);
  if (!g0.door(vault, 0, (P.barX0 + P.barX1) / 2 + r.irange(3) - 1, kd + 2, r)) return nope(__LINE__);
  const int dx = g0.rooms[(size_t)vault].doorX;
  if (dx < P.barX0 || dx > P.barX1) return nope(__LINE__);
  if (P.floors >= 2 && !upperRooms(P, r, W - 3, kd + 3, kd, {RoomKind::OwnerRoom, RoomKind::Study, RoomKind::Study, RoomKind::Storeroom})) return nope(__LINE__);
  if (!g0.door(off, 0, P.X(W - 1 - ow + 1), kd + 2, r)) return nope(__LINE__);
  return true;
}

// ---- a round feasting hall (the steppe's feast tent, a round mead hall): the fire in the middle, the high seat at the
// back of the circle, the store walled off at one side
bool planFeastTent(Plan& P, Rng& r, Tmpl t) {
  P.tmpl = t == Tmpl::MeadHall ? Tmpl::FeastTent : t;
  Geo& g0 = P.geo[0];
  P.initFloor(g0, t == Tmpl::MeadHall ? RoomKind::Feast : RoomKind::ThroneHall);
  const int side = r.f() < 0.5f;
  const int sto = P.carve(g0, side ? P.W - 5 : 1, P.H / 2 - 1, side ? P.W - 2 : 4, P.H / 2 + 1, RoomKind::Storeroom);
  if (sto < 0 || !g0.finish()) return nope(__LINE__);
  const IRect& R = g0.rooms[(size_t)sto].r;
  if (!g0.door(sto, 0, R.x < P.ex ? R.x + R.w : R.x - 1, R.cy(), r)) return nope(__LINE__);
  for (int x = P.ex - 1; x <= P.ex + 1; x++) if (g0.roomOf(x, 2) != 0) return nope(__LINE__);
  P.pitY = P.H / 2; P.pitX0 = P.ex - 1; P.pitX1 = P.ex + 1;
  for (int f = 1; f < P.floors; f++) {
    P.initFloor(P.geo[(size_t)f], RoomKind::Bedroom);
    if (!P.geo[(size_t)f].finish() || !stairsAnywhere(P, f - 1, 0, r)) return nope(__LINE__);
  }
  return true;
}

// ---- a mead hall (and the jarl's great hall, the marsh elders' stilt hall): the feasting hall round the long hearth
// down its middle (split where the way from the door crosses it), the high seat on the axis at the far wall, benches
// and long tables down both sides; the kitchen and the mead store walled off in the back corners; lofts above
bool planMeadHall(Plan& P, Rng& r, Tmpl t) {
  if (P.fp == bld::Floorplan::Round) return planFeastTent(P, r, t);
  P.tmpl = t;
  const int W = P.W, H = P.H;
  Geo& g0 = P.geo[0];
  const bool seat = t != Tmpl::MeadHall;
  P.initFloor(g0, seat ? RoomKind::ThroneHall : RoomKind::Feast);
  const int kw = 5 + r.irange(2), kd = 4 + (H >= 15 ? 1 : 0);
  const int kit = P.carve(g0, 1, 2, kw, kd, RoomKind::Kitchen);
  const int sto = P.carve(g0, W - 1 - kw, 2, W - 2, kd, RoomKind::Storeroom);
  if (kit < 0 || sto < 0 || !g0.finish()) return nope(__LINE__);
  for (int x = P.ex - 3; x <= P.ex + 3; x++) if (g0.roomOf(x, 2) != 0) return nope(__LINE__);
  P.pitY = (kd + 3 + H - 2) / 2;
  P.pitX0 = 3; P.pitX1 = W - 4;
  if (P.pitY < kd + 4 || P.pitY > H - 4) return nope(__LINE__);
  if (!trenchCulture(P.culture)) P.pitY = -1;   // (M3b fixer) no trench of embers but in the northern halls
  if (P.floors >= 2 &&
      !upperRooms(P, r, W - 3, kd + 3, kd,
                  seat ? std::initializer_list<RoomKind>{RoomKind::OwnerRoom, RoomKind::Bedroom, RoomKind::Study, RoomKind::Bedroom, RoomKind::Storeroom}
                       : std::initializer_list<RoomKind>{RoomKind::OwnerRoom, RoomKind::Storeroom, RoomKind::Bedroom}))
    return nope(__LINE__);
  if (!g0.door(kit, 0, P.X(2 + r.irange(kw - 2)), kd + 2, r) || !g0.door(sto, 0, P.X(W - 2 - kw + 1), kd + 2, r)) return nope(__LINE__);
  return true;
}

// ---- a bathhouse: the way in is the changing room (benches, shelves of linen, pegs); the bathing hall behind it with
// its pool sunk in the floor, a coping round it and benches by the walls; the furnace room that heats it at one side.
// A round bathhouse (a tholos) is one round hall round its round pool.
bool planBathhouse(Plan& P, Rng& r) {
  P.tmpl = Tmpl::Bathhouse;
  const int W = P.W, H = P.H;
  Geo& g0 = P.geo[0];
  P.pool.clear();
  if (P.fp == bld::Floorplan::Round) {
    P.initFloor(g0, RoomKind::Bath);
    if (!g0.finish()) return nope(__LINE__);
    const int cx = P.ex, cy = H / 2, rr = std::max(1, (W - 2) / 6);
    for (int y = cy - rr; y <= cy + rr; y++)
      for (int x = cx - rr; x <= cx + rr; x++)
        if ((x - cx) * (x - cx) + (y - cy) * (y - cy) <= rr * rr + rr && g0.roomOf(x, y) == 0) P.pool.push_back(g0.I(x, y));
  } else {
    P.initFloor(g0, RoomKind::Changing);
    // (M5 fixer r2: two capitals' bathhouses were the same plan) the furnace room's width, the pool's size and where it
    // lies in the hall vary by the building
    const int cd = 4, fw = 3 + r.irange(3);
    const int by = H - 2 - cd - 2;   // the bathing hall: rows 2..by
    if (by < 7) return nope(__LINE__);
    const int bath = P.carve(g0, 1, 2, W - 3 - fw, by, RoomKind::Bath);
    const int furn = P.carve(g0, W - 1 - fw, 2, W - 2, by, RoomKind::Storeroom);
    if (bath < 0 || furn < 0 || !g0.finish()) return nope(__LINE__);
    const IRect& B = g0.rooms[(size_t)bath].r;
    int px0 = B.x + 2, px1 = B.x + B.w - 3, py0 = B.y + 2, py1 = B.y + B.h - 3;
    if (px1 - px0 < 2 || py1 - py0 < 1) return nope(__LINE__);
    {   // a smaller pool set off to one side, or one nearer the far wall, leaving a wider walk on the other
      const int shrink = std::min(px1 - px0 - 2, r.irange(3));
      if (shrink > 0) { if (r.f() < 0.5f) px0 += shrink; else px1 -= shrink; }
      if (py1 - py0 >= 3 && r.f() < 0.5f) py1--;
    }
    for (int y = py0; y <= py1; y++)
      for (int x = px0; x <= px1; x++) P.pool.push_back(g0.I(x, y));
    if (!g0.door(bath, 0, P.ex, by + 2, r) || !g0.door(furn, 0, g0.rooms[(size_t)furn].r.cx(), by + 2, r)) return nope(__LINE__);
  }
  if (P.floors >= 2) {
    if (P.fp == bld::Floorplan::Round) {
      for (int f = 1; f < P.floors; f++) {
        P.initFloor(P.geo[(size_t)f], RoomKind::Parlour);
        if (!P.geo[(size_t)f].finish() || !stairsAnywhere(P, f - 1, 0, r)) return nope(__LINE__);
      }
      return true;
    }
    // the rest rooms and the keeper's upstairs, the flight from the changing room under the hall's wall
    return upperRooms(P, r, 2 + r.irange(3), H - 2 - 4 + 1, H - 2 - 4 - 2, {RoomKind::Parlour, RoomKind::OwnerRoom, RoomKind::Parlour, RoomKind::Storeroom});
  }
  return true;
}

// ---- a tea house: the tea room by the street (low tables and cushions, folding screens), the tea counter against the
// tea kitchen's wall with the kettle on its stove; private tea rooms walled off down one side; upstairs more of them
// and the tea master's room
bool planTeaHouse(Plan& P, Rng& r) {
  P.tmpl = Tmpl::TeaHouse;
  const int W = P.W, H = P.H;
  Geo& g0 = P.geo[0];
  P.initFloor(g0, RoomKind::TeaRoom);
  const int kd = 4, kw = 4 + r.irange(2), pw = 4;
  const int kit = innKitchenBar(P, g0, 1, kw, kd, true);
  if (kit < 0) return nope(__LINE__);
  std::vector<int> ids;
  for (int y = 2; y + 2 <= H - 2; y += 5) {
    int ri = P.carve(g0, W - 1 - pw, y, W - 2, (y + 2 > H - 5 ? H - 2 : y + 2), RoomKind::Parlour);
    if (ri < 0) break;
    ids.push_back(ri);
  }
  if (ids.empty() || !g0.finish() || !barFits(P, g0) || !innKitchenDoor(P, g0, kit, r)) return nope(__LINE__);
  for (int ri : ids) if (!g0.door(ri, 0, P.X(W - 2 - pw), g0.rooms[(size_t)ri].r.cy(), r)) return nope(__LINE__);
  if (P.floors >= 2) {
    const int sxU = innNook(P, kw + 3, W - 4 - pw, r);
    return sxU >= 0 && upperRooms(P, r, sxU, 2, kd, {RoomKind::OwnerRoom, RoomKind::Parlour, RoomKind::Parlour, RoomKind::Storeroom});
  }
  return true;
}

// ---- a warrior lodge: the training floor (straw dummies, weapon racks along the walls), the armoury at the back and
// the sworn warriors' bunk room beside it, the lodge master's room; a lodge with an upper floor sleeps up there and
// keeps its mess kitchen below
bool planLodge(Plan& P, Rng& r) {
  P.tmpl = Tmpl::Lodge;
  const int W = P.W;
  Geo& g0 = P.geo[0];
  P.initFloor(g0, RoomKind::Training);
  const int kd = 4 + (P.H >= 15 ? 1 : 0), aw = 4 + r.irange(2), ow = 4;
  const int arm = P.carve(g0, 1, 2, aw, kd, RoomKind::Storeroom);
  int mid = -1, own = -1;
  if (P.floors == 1) {
    mid = P.carve(g0, aw + 2, 2, W - 3 - ow, kd, RoomKind::Barracks);
    own = P.carve(g0, W - 1 - ow, 2, W - 2, kd, RoomKind::OwnerRoom);
  } else {
    mid = P.carve(g0, W - 1 - ow - 1, 2, W - 2, kd, RoomKind::Kitchen);
  }
  if (arm < 0 || mid < 0 || (P.floors == 1 && own < 0) || !g0.finish()) return nope(__LINE__);
  if (!g0.door(arm, 0, g0.rooms[(size_t)arm].r.cx(), kd + 2, r) || !g0.door(mid, 0, g0.rooms[(size_t)mid].r.cx(), kd + 2, r)) return nope(__LINE__);
  if (own >= 0 && !g0.door(own, 0, g0.rooms[(size_t)own].r.cx(), kd + 2, r)) return nope(__LINE__);
  if (P.floors >= 2) {
    const int sxU = innNook(P, aw + 3, W - 5 - ow, r);
    return sxU >= 0 && upperRooms(P, r, sxU, 2, kd, {RoomKind::Barracks, RoomKind::Barracks, RoomKind::OwnerRoom, RoomKind::Barracks});
  }
  return true;
}

// ---- a council hall (the clan elders' moot, the high council's chamber; the council spire's and the tree palace's
// seat): the council sits in a ring round the middle of the hall, the speaker's seat on the axis at the far wall; the
// archive and the robing room in the back corners (a round hall: an alcove at one side); the speaker's rooms upstairs
bool planCouncilHall(Plan& P, Rng& r, Tmpl t) {
  P.tmpl = t;
  const int W = P.W, H = P.H;
  Geo& g0 = P.geo[0];
  const bool seat = t != Tmpl::CouncilHall;
  P.initFloor(g0, seat ? RoomKind::ThroneHall : RoomKind::Assembly);
  std::vector<int> ids;
  if (P.fp == bld::Floorplan::Round) {
    const int side = r.f() < 0.5f;
    int a = P.carve(g0, side ? W - 5 : 1, H / 2, side ? W - 2 : 4, H / 2 + 2, RoomKind::Study);
    if (a < 0 || !g0.finish()) return nope(__LINE__);
    const IRect& R = g0.rooms[(size_t)a].r;
    if (!g0.door(a, 0, R.x < P.ex ? R.x + R.w : R.x - 1, R.cy(), r)) return nope(__LINE__);
  } else {
    const int kd = 4, sw = 5;
    int a = P.carve(g0, 1, 2, sw, kd, RoomKind::Study);
    int b = P.carve(g0, W - 1 - sw, 2, W - 2, kd, seat ? RoomKind::OwnerRoom : RoomKind::Vestry);
    if (a < 0 || b < 0 || !g0.finish()) return nope(__LINE__);
    ids = {a, b};
  }
  for (int x = P.ex - 1; x <= P.ex + 1; x++) if (g0.roomOf(x, 2) != 0) return nope(__LINE__);
  if (P.floors >= 2) {
    if (P.fp == bld::Floorplan::Round) {
      for (int f = 1; f < P.floors; f++) {
        P.initFloor(P.geo[(size_t)f], f == P.floors - 1 ? RoomKind::OwnerRoom : RoomKind::Study);
        if (!P.geo[(size_t)f].finish() || !stairsAnywhere(P, f - 1, f % 2 ? 1 : -1, r)) return nope(__LINE__);
      }
      return true;
    }
    if (!upperRooms(P, r, 1, 7, 4, {RoomKind::OwnerRoom, RoomKind::Study, RoomKind::Bedroom, RoomKind::Council})) return nope(__LINE__);
  }
  for (int ri : ids) {
    const IRect& R = g0.rooms[(size_t)ri].r;
    if (!g0.door(ri, 0, R.x + R.w - 2, 6, r) && !g0.door(ri, 0, R.x + 1, 6, r)) return nope(__LINE__);
  }
  return true;
}

// ---- seats of power (VISION_PLAN 15.14: the society's seat, never the shared castle kit)
// the khan's great tent: the dais at the back of the circle (the throne cushions on their carpets), carpets over the
// whole floor round the stove, the khan's sleeping alcove and the treasury walled off with felt at the sides
bool planTentSeat(Plan& P, Rng& r) {
  P.tmpl = Tmpl::SeatTent;
  Geo& g0 = P.geo[0];
  P.initFloor(g0, RoomKind::ThroneHall);
  // (M3b fixer) one round felt hall under the crown: only the khan's sleeping alcove is walled off (with the lattice, at
  // one side of the circle, against the drum); the treasury is screened off with felt (furnishSeatHall)
  const int side = r.f() < 0.5f, cy = P.H / 2;
  const int a = P.carve(g0, side ? P.W - 5 : 1, cy, side ? P.W - 2 : 4, cy + 2, RoomKind::OwnerRoom);
  if (a < 0 || !g0.finish()) return nope(__LINE__);
  {
    const IRect& R = g0.rooms[(size_t)a].r;
    if (!g0.door(a, 0, R.x < P.ex ? R.x + R.w : R.x - 1, R.cy(), r)) return nope(__LINE__);
  }
  for (int x = P.ex - 2; x <= P.ex + 2; x++) if (g0.roomOf(x, 2) != 0 && g0.roomOf(x, 3) != 0) return nope(__LINE__);
  for (int f = 1; f < P.floors; f++) {
    P.initFloor(P.geo[(size_t)f], RoomKind::Bedroom);
    if (!P.geo[(size_t)f].finish() || !stairsAnywhere(P, f - 1, 0, r)) return nope(__LINE__);
  }
  return true;
}
// a courtyard palace: the gate opens into the court (a fountain, plants, the guard), the throne hall faces it across
// the back on the axis, the council chamber and the ruler's apartments beside it; the kitchen and the guardroom down
// the sides, guard lodges either side of the gate; a second storey over the back range for the household
bool planCourtSeat(Plan& P, Rng& r) {
  P.tmpl = Tmpl::SeatCourt;
  const int W = P.W, H = P.H, ex = P.ex;
  Geo& g0 = P.geo[0];
  P.initFloor(g0, RoomKind::Court);
  const int bd = P.cy0 - 3, fy = P.cy1 + 3;
  if (bd < 5 || fy > H - 4) return nope(__LINE__);
  const int tw = std::max(9, (W - 2) / 2) | 1;   // the throne hall on the axis
  const int t0 = ex - tw / 2, t1 = ex + tw / 2;
  if (t0 < 5 || t1 > W - 6) return nope(__LINE__);
  // (M3b fixer) each people's court its own: the jade court's ministries and tea pavilion round its lotus ponds, the
  // dune palace's diwan and kitchens along the rills of its garden court, the imperial palace's records hall and
  // guard round the impluvium; the gate lodges house the guard, the kitchens, the treasury or the armoury
  using A = cult::Archetype;
  const A cu = P.culture >= 0 ? (A)P.culture : A::COUNT;
  RoomKind sideA = RoomKind::Kitchen, sideB = RoomKind::Barracks, lodgeA = RoomKind::Barracks, lodgeB = RoomKind::Storeroom;
  if (cu == A::Jade) { sideA = RoomKind::Study; sideB = RoomKind::Parlour; lodgeA = RoomKind::Barracks; lodgeB = RoomKind::Kitchen; }
  else if (cu == A::Dune) { sideA = RoomKind::Parlour; sideB = RoomKind::Kitchen; lodgeA = RoomKind::Barracks; lodgeB = RoomKind::Storeroom; }
  else if (cu == A::Imperial) { sideA = RoomKind::Study; sideB = RoomKind::Barracks; lodgeA = RoomKind::Kitchen; lodgeB = RoomKind::Storeroom; }
  if (r.f() < 0.5f) { std::swap(sideA, sideB); }
  if (r.f() < 0.5f) { std::swap(lodgeA, lodgeB); }
  const bool councilWest = r.f() < 0.5f;
  // (M3b round 3) each people's palace its own PLAN, not one plan retinted (four cultures shared a back range of three
  // rooms, a court, two side wings and two gate lodges round a one-tile passage):
  //   imperial  the basilica: the throne hall the whole width of the back range, the council chamber and the
  //             emperor's apartments the two side ranges along the peristyle, the guard and the kitchens by the gate
  //   jade      three detached pavilions across the back (alleys between them), ministries two to a side, a wide
  //             gate passage between the guard and the kitchens
  //   dune      the iwan: the diwan the whole of one side, the kitchens and the treasury the other, and no gate
  //             lodges: the court runs to the front wall under its arcade, the rills crossing its garden
  //   others    the original court palace
  int variant = cu == A::Imperial ? 1 : cu == A::Jade ? 2 : cu == A::Dune ? 3 : 0;
  const int sw = P.cx0 - 2;
  if (sw < 3 && variant != 0) variant = 0;
  // jade's pavilions stand apart: the throne pavilion narrower, an alley either side of it opening onto the court
  const int jt0 = ex - 4, jt1 = ex + 4;
  if (variant == 2 && (jt0 - 4 < 3 || jt0 - 2 < P.cx0 || jt1 + 2 > P.cx1)) variant = 0;
  int th = -1, co = -1, ap = -1;
  std::vector<int> rest;
  if (variant == 1) {
    th = P.carve(g0, 1, 2, W - 2, bd, RoomKind::ThroneHall);
    co = P.carve(g0, 1, P.cy0, sw, P.cy1, councilWest ? RoomKind::Council : RoomKind::OwnerRoom);
    ap = P.carve(g0, P.cx1 + 2, P.cy0, W - 2, P.cy1, councilWest ? RoomKind::OwnerRoom : RoomKind::Council);
    if (th < 0 || co < 0 || ap < 0) return nope(__LINE__);
    rest.push_back(co); rest.push_back(ap);
    const bool kw = r.f() < 0.5f;
    for (int side = 0; side < 2; side++) {
      int ri = P.carve(g0, side ? ex + 3 : 1, fy, side ? W - 2 : ex - 3, H - 2, (side == 0) == kw ? RoomKind::Kitchen : RoomKind::Barracks);
      if (ri < 0) return nope(__LINE__);
      rest.push_back(ri);
    }
  } else if (variant == 2) {
    th = P.carve(g0, jt0, 2, jt1, bd, RoomKind::ThroneHall);
    co = P.carve(g0, 1, 2, jt0 - 4, bd, councilWest ? RoomKind::Council : RoomKind::OwnerRoom);
    ap = P.carve(g0, jt1 + 4, 2, W - 2, bd, councilWest ? RoomKind::OwnerRoom : RoomKind::Council);
    if (th < 0 || co < 0 || ap < 0) return nope(__LINE__);
    // the ministries: two to a side where the court is deep enough, one otherwise
    const int m = (P.cy0 + P.cy1) / 2;
    const bool two = m - 1 - P.cy0 >= 1 && P.cy1 - (m + 2) >= 1;
    RoomKind wk[2] = {RoomKind::Study, RoomKind::Parlour}, ek[2] = {RoomKind::Kitchen, RoomKind::Storeroom};
    if (r.f() < 0.5f) std::swap(wk[0], wk[1]);
    if (r.f() < 0.5f) std::swap(ek[0], ek[1]);
    for (int side = 0; side < 2; side++) {
      const int x0 = side ? P.cx1 + 2 : 1, x1 = side ? W - 2 : sw;
      const RoomKind* ks = side ? ek : wk;
      if (two) {
        int a = P.carve(g0, x0, P.cy0, x1, m - 1, ks[0]), b = P.carve(g0, x0, m + 2, x1, P.cy1, ks[1]);
        if (a < 0 || b < 0) return nope(__LINE__);
        rest.push_back(a); rest.push_back(b);
      } else {
        int a = P.carve(g0, x0, P.cy0, x1, P.cy1, ks[0]);
        if (a < 0) return nope(__LINE__);
        rest.push_back(a);
      }
    }
    for (int side = 0; side < 2; side++) {
      int ri = P.carve(g0, side ? ex + 4 : 1, fy, side ? W - 2 : ex - 4, H - 2, side ? RoomKind::Kitchen : RoomKind::Barracks);
      if (ri < 0) return nope(__LINE__);
      rest.push_back(ri);
    }
  } else {
    th = P.carve(g0, t0, 2, t1, bd, RoomKind::ThroneHall);
    co = P.carve(g0, 1, 2, t0 - 2, bd, councilWest ? RoomKind::Council : RoomKind::OwnerRoom);
    ap = P.carve(g0, t1 + 2, 2, W - 2, bd, councilWest ? RoomKind::OwnerRoom : RoomKind::Council);
    if (th < 0 || co < 0 || ap < 0) return nope(__LINE__);
    if (variant == 3) {
      // the diwan down one whole side, the kitchens and the treasury down the other; no gate lodges
      const bool dw = r.f() < 0.5f;
      const int m = (P.cy0 + P.cy1) / 2;
      const bool two = m - 1 - P.cy0 >= 1 && P.cy1 - (m + 2) >= 1;
      int a = P.carve(g0, dw ? 1 : P.cx1 + 2, P.cy0, dw ? sw : W - 2, P.cy1, RoomKind::Parlour);
      if (a < 0) return nope(__LINE__);
      rest.push_back(a);
      const int x0 = dw ? P.cx1 + 2 : 1, x1 = dw ? W - 2 : sw;
      if (two) {
        int b = P.carve(g0, x0, P.cy0, x1, m - 1, RoomKind::Kitchen), c = P.carve(g0, x0, m + 2, x1, P.cy1, RoomKind::Storeroom);
        if (b < 0 || c < 0) return nope(__LINE__);
        rest.push_back(b); rest.push_back(c);
      } else {
        int b = P.carve(g0, x0, P.cy0, x1, P.cy1, RoomKind::Kitchen);
        if (b < 0) return nope(__LINE__);
        rest.push_back(b);
      }
    } else {
      if (sw >= 3) {
        int a = P.carve(g0, 1, P.cy0, sw, P.cy1, sideA);
        int b = P.carve(g0, P.cx1 + 2, P.cy0, W - 2, P.cy1, sideB);
        if (a < 0 || b < 0) return nope(__LINE__);
        rest.push_back(a); rest.push_back(b);
      }
      for (int side = 0; side < 2; side++) {
        int ri = P.carve(g0, side ? ex + 3 : 1, fy, side ? W - 2 : ex - 3, H - 2, side ? lodgeB : lodgeA);
        if (ri < 0) return nope(__LINE__);
        rest.push_back(ri);
      }
    }
  }
  if (!g0.finish()) return nope(__LINE__);
  // the court's water (ground floor pools, Ground::Water: solid, the coping drawn round them), clear of the court's
  // edges (every door's approach) by two tiles: the jade court's two lotus ponds either side of the way, the dune
  // garden's rills crossing it, the imperial impluvium on the axis (the way passes round it)
  P.pool.clear();
  {
    const int py0 = P.cy0 + 2, py1 = P.cy1 - 2;
    auto pond = [&](int x0, int x1, int y0, int y1) {
      if (x1 < x0 || y1 < y0) return;
      for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++)
          if (g0.roomOf(x, y) == 0 && x > P.cx0 && x < P.cx1) P.pool.push_back(g0.I(x, y));
    };
    if (py1 >= py0) {
      if (cu == A::Jade) {
        const int inset = r.irange(2);
        if (ex - 3 - (P.cx0 + 2 + inset) >= 1) { pond(P.cx0 + 2 + inset, ex - 3, py0, py1); pond(ex + 3, P.cx1 - 2 - inset, py0, py1); }
      } else if (cu == A::Dune) {
        const int off = 2 + r.irange(2);
        if (ex - off > P.cx0 + 1) {
          pond(ex - off, ex - off, P.cy0 + 1, P.cy1 - 1); pond(ex + off, ex + off, P.cy0 + 1, P.cy1 - 1);
          // (round 3) the cross rill of the chahar bagh: four garden quarters, the way and the court's edges open
          const int my = (P.cy0 + P.cy1) / 2;
          if (variant == 3 && P.cy1 - P.cy0 >= 5) { pond(P.cx0 + 2, ex - off - 1, my, my); pond(ex + off + 1, P.cx1 - 2, my, my); }
        }
      } else if (cu == A::Imperial) {
        const int hw = std::min(2, (P.cx1 - P.cx0) / 2 - 3);
        if (hw >= 1) pond(ex - hw, ex + hw, py0, py1);
      }
    }
  }
  if (!g0.door(th, 0, ex + (r.f() < 0.5f ? 2 : -2), bd + 2, r)) return nope(__LINE__);
  if (variant != 1) {
    if (!g0.door(co, th, t0 - 1, (2 + bd) / 2, r) && !g0.door(co, 0, g0.rooms[(size_t)co].r.cx(), bd + 2, r)) return nope(__LINE__);
    if (!g0.door(ap, th, t1 + 1, (2 + bd) / 2, r) && !g0.door(ap, 0, g0.rooms[(size_t)ap].r.cx(), bd + 2, r)) return nope(__LINE__);
  }
  if (!doorsToward(g0, rest, 0, ex, (P.cy0 + P.cy1) / 2, r)) return nope(__LINE__);
  P.var = variant;
  for (int f = 1; f < P.floors; f++) {
    Geo& g1 = P.geo[(size_t)f];
    g1.initShape(W, H, RoomKind::Corridor, [&](int, int y) { return y <= bd; });
    // the household over the council chamber and the apartments, the gallery over the throne hall
    int a = g1.carve(1, 2, t0 - 2, bd, RoomKind::Bedroom, true), b = g1.carve(t1 + 2, 2, W - 2, bd, RoomKind::Study, true);
    if (a < 0 || b < 0 || !g1.finish()) return nope(__LINE__);
    if (f == 1 ? !stairsInRoom(P, 0, th, r) : !stairsAnywhere(P, f - 1, 0, r)) return nope(__LINE__);
    if (!g1.door(a, 0, t0 - 1, (2 + bd) / 2, r) || !g1.door(b, 0, t1 + 1, (2 + bd) / 2, r)) return nope(__LINE__);
  }
  return true;
}

// ---- (M3b fixer) a theocracy's high temple (the ruler's seat, the capital lord's): never the castle kit. One great
// sanctum hall from the doors to the high priest's seat between the gods' altars, colonnades down it; the vestry and
// the scriptorium (or the temple treasury) walled off in the back corners, the priests' cells and the offerings store
// either side of the doors; the high priest's household upstairs
bool planSanctumSeat(Plan& P, Rng& r) {
  P.tmpl = Tmpl::SeatSanctum;
  const int W = P.W, H = P.H, ex = P.ex;
  Geo& g0 = P.geo[0];
  P.initFloor(g0, RoomKind::ThroneHall);
  const int vw = 4 + r.irange(2), vd = 4 + (H >= 18 ? r.irange(2) : 0);
  const int cw = 4 + r.irange(2), cd = 3 + (H >= 18 ? 1 : 0);
  if (vw + 2 >= ex - 3 || cw + 2 >= ex - 2 || vd + 6 >= H - 2 - cd) return nope(__LINE__);
  const int ves = P.carve(g0, 1, 2, vw, vd, RoomKind::Vestry);
  // (the ruler's high temple keeps the council of priests in the back room; one storey: the high priest sleeps by the doors)
  const bool royalSeat = P.type == Building::Palace;
  const int scr = P.carve(g0, W - 1 - vw, 2, W - 2, vd, royalSeat ? RoomKind::Council : (r.f() < 0.5f ? RoomKind::Study : RoomKind::Storeroom));
  const int cel = P.carve(g0, 1, H - 1 - cd, cw, H - 2, P.floors == 1 ? RoomKind::OwnerRoom : RoomKind::Bedroom);
  const int off = P.carve(g0, W - 1 - cw, H - 1 - cd, W - 2, H - 2, RoomKind::Storeroom);
  if (ves < 0 || scr < 0 || cel < 0 || off < 0 || !g0.finish()) return nope(__LINE__);
  for (int x = ex - 2; x <= ex + 2; x++) if (g0.roomOf(x, 2) != 0) return nope(__LINE__);   // the seat (the altars beside it where they fit)
  for (int y = 3; y <= H - 2; y++) if (g0.roomOf(ex, y) != 0) return nope(__LINE__);
  if (P.floors >= 2) {
    int sxU = innNook(P, vw + 2, ex - 8, r);
    if (sxU < 0) sxU = innNook(P, ex + 7, W - 4 - vw, r);
    if (sxU < 0) sxU = innNook(P, vw + 2, ex - 4, r);   // (a small sanctum: the flight where one altar would stand)
    if (sxU < 0 || !upperRooms(P, r, sxU, 2, 4, {RoomKind::OwnerRoom, RoomKind::Vestry, RoomKind::Study, RoomKind::Bedroom, RoomKind::Storeroom})) return nope(__LINE__);
  }
  for (int ri : {ves, scr}) if (!g0.door(ri, 0, g0.rooms[(size_t)ri].r.cx(), vd + 2, r) && !g0.door(ri, 0, ri == ves ? vw + 1 : W - 2 - vw, 3, r)) return nope(__LINE__);
  for (int ri : {cel, off}) if (!g0.door(ri, 0, g0.rooms[(size_t)ri].r.cx(), H - 2 - cd, r) && !g0.door(ri, 0, ri == cel ? cw + 1 : W - 2 - cw, H - 3, r)) return nope(__LINE__);
  // the floor: great flags laid irregularly (a regular grid of tiles read as graph paper at 1x)
  // (M3b fixer round 3: the dune high temple's glazed mosaic, never the sun folk's great flags)
  g0.rooms[0].floorStyle = (uint8_t)((int)(P.culture == (int)cult::Archetype::Dune ? art::FloorStyle::Mosaic : art::FloorStyle::Flagstone) + 1);
  return true;
}

bool planRoundStrict(Plan& P, Rng& r);
// ---- an L keep or palace: the throne hall fills the floor, the throne on the axis at the far wall; the kitchen and the
// guardroom walled off in corners away from the aisle; the lord's own room in the wing (a front-corner yard) or the back
// arm's far corner (a back-corner yard); upstairs the household's rooms in the corners of the L
bool planLCastle(Plan& P, Rng& r) {
  const int W = P.W, H = P.H, ex = P.ex;
  const bool front = P.lCorner >= 2;
  for (int f = 0; f < P.floors; f++) {
    Geo& g = P.geo[(size_t)f];
    P.initFloor(g, f == 0 ? RoomKind::ThroneHall : RoomKind::Corridor);
    std::vector<int> ids;
    auto add = [&](int x0, int y0, int x1, int y1, RoomKind k) { int ri = g.carve(x0, y0, x1, y1, k, true); if (ri >= 0) ids.push_back(ri); return ri >= 0; };
    // the arm with the door: its two corners away from the aisle; the lord's room
    const bool legWest = P.lCorner == 3, legEast = P.lCorner == 2;
    const int cw = 5;
    const RoomKind a = f == 0 ? RoomKind::Kitchen : RoomKind::Bedroom, b = f == 0 ? RoomKind::Barracks : RoomKind::Council;
    if (legWest) { if (!add(1, 2, cw, 5, a) || !add(1, H - 5, cw, H - 2, b)) return nope(__LINE__); }
    else if (legEast) { if (!add(W - 1 - cw, 2, W - 2, 5, a) || !add(W - 1 - cw, H - 5, W - 2, H - 2, b)) return nope(__LINE__); }
    else { if (!add(1, H - 5, cw, H - 2, a) || !add(W - 1 - cw, H - 5, W - 2, H - 2, b)) return nope(__LINE__); }
    if (front) {   // the wing beyond the yard: the lord's
      const int wx0 = P.lCorner == 3 ? P.lx0 : 1, wx1 = P.lCorner == 3 ? W - 2 : P.lx1;
      if (!add(wx0, 2, wx1, P.ly0 - 1, f == 0 ? RoomKind::OwnerRoom : RoomKind::Study)) return nope(__LINE__);
    } else {       // the back arm's far corner
      const bool east = P.lCorner == 0;
      if (!add(east ? W - 1 - cw : 1, 2, east ? W - 2 : cw, 4, f == 0 ? RoomKind::OwnerRoom : RoomKind::Study)) return nope(__LINE__);
    }
    if (f > 0) add(ex - 2, H - 5, ex + 2, H - 2, RoomKind::OwnerRoom);
    if (!g.finish()) return nope(__LINE__);
    if (f == 0) {
      for (int x = ex - 1; x <= ex + 1; x++) if (g.roomOf(x, 2) != 0) return nope(__LINE__);
      for (int y = 3; y <= H - 2; y++) if (g.roomOf(ex, y) != 0) return nope(__LINE__);
    }
    for (int ri : ids) {
      const IRect& R = g.rooms[(size_t)ri].r;
      if (!g.door(ri, 0, R.x < ex ? R.x + R.w : R.x - 1, R.cy(), r) && !g.door(ri, 0, R.cx(), R.y < H / 2 ? R.y + R.h + 1 : R.y - 1, r)) return nope(__LINE__);
    }
  }
  for (int f = 0; f + 1 < P.floors; f++)
    if (!stairsAnywhere(P, f, f % 2 ? 1 : -1, r)) return nope(__LINE__);
  return true;
}
// a keep or a palace on whatever floor plan its blueprint has
bool planCastle(Plan& P, Rng& r) {
  if (P.fp == bld::Floorplan::L) return planLCastle(P, r);
  if (P.fp == bld::Floorplan::Round) return planRoundStrict(P, r);
  if (P.fp == bld::Floorplan::Courtyard && P.type == Building::Palace) return planCourtSeat(P, r);
  return P.type == Building::Keep ? planKeep(P, r) : planPalace(P, r);
}

bool planSeat(Plan& P, Rng& r) {
  switch ((cult::Seat)P.seat) {
    case cult::Seat::CourtPalace:
      if (P.fp == bld::Floorplan::Courtyard) return planCourtSeat(P, r);
      { const bool ok = planCastle(P, r); P.tmpl = Tmpl::SeatCourt; return ok; }
    case cult::Seat::GreatHall: return planMeadHall(P, r, Tmpl::SeatGreatHall);
    case cult::Seat::StiltHall: return planMeadHall(P, r, Tmpl::SeatStilt);
    case cult::Seat::TentCourt: return planTentSeat(P, r);
    case cult::Seat::TempleComplex:
      P.tmpl = Tmpl::SeatSanctum;
      if (P.type == Building::Temple) {
        const bool ok = P.fp == bld::Floorplan::Cross ? planCrossTemple(P, r) : (P.fp == bld::Floorplan::Round ? planRoundTemple(P, r) : planTemple(P, r));
        P.tmpl = Tmpl::SeatSanctum;
        return ok;
      }
      // (M3b fixer) the high temple's own plan on a plain body; a shaped one (a round or L body) keeps the shape's
      if (P.fp == bld::Floorplan::Rect || P.fp == bld::Floorplan::Long || P.fp == bld::Floorplan::Courtyard || P.fp == bld::Floorplan::Cross) {
        const bld::Floorplan keepFp = P.fp;
        P.fp = bld::Floorplan::Rect;
        if (planSanctumSeat(P, r)) return true;
        P.fp = keepFp;
      }
      { const bool ok = planCastle(P, r); P.tmpl = Tmpl::SeatSanctum; return ok; }
    case cult::Seat::GuildExchange: return planGuildhall(P, r, Tmpl::SeatDoge);
    case cult::Seat::CouncilSpire: return planCouncilHall(P, r, Tmpl::SeatSpire);
    case cult::Seat::TreePalace: return planCouncilHall(P, r, Tmpl::SeatTree);
    default: {
      const bool ok = P.type == Building::Keep || P.type == Building::Palace ? planCastle(P, r) : planClassic(P, r);
      P.tmpl = Tmpl::SeatCastle;
      return ok;
    }
  }
}

// the fallback: one room per floor, stairs at the back wall
void planPlain(Plan& P) {
  for (int f = 0; f < P.floors; f++) {
    P.initFloor(P.geo[(size_t)f], mainKindOf(P.type, f, P.floors));
    if (f > 0 && P.type != Building::Tower) P.geo[(size_t)f].rooms[0].kind = RoomKind::Bedroom;
    P.geo[(size_t)f].finish();
  }
  Rng r(P.b ? P.b->seed ^ 0x9A1Au : 1u);
  for (int f = 0; f + 1 < P.floors; f++) {
    bool ok = false;
    for (int k = 0; k < P.W && !ok; k++) {
      int x = (f % 2 == 0) ? 1 + k : P.W - 2 - k;
      ok = setStairs(P, f, x, 2, false, true);
    }
    if (!ok) stairsAnywhere(P, f, f % 2 == 0 ? -1 : 1, r, true);
  }
  P.var = 99;
  P.tmpl = Tmpl::Plain;
}

void planSize(Plan& P) {
  const Bldg& b = *P.b;
  int w = b.r.w, h = b.r.h;
  // M3b: a single body (rect, round, long) is sized from the body volume's proportions; an L, a courtyard or a cross
  // from the whole footprint (its wings and court are part of the plan)
  if ((P.fp == bld::Floorplan::Rect || P.fp == bld::Floorplan::Round || P.fp == bld::Floorplan::Long) && P.bodyW > 0) {
    w = std::clamp(P.bodyW, 2, b.r.w);
    h = std::clamp(P.bodyH, 2, b.r.h);
  }
  switch (b.type) {
    case Building::Guildhall: P.W = std::max(P.floors >= 2 ? 23 : 19, 2 * w + 7); P.H = std::max(14, 2 * h + 8); break;
    case Building::Exchange: P.W = std::max(17, 2 * w + 5); P.H = std::max(13, 2 * h + 7); break;
    case Building::MeadHall: P.W = std::max(21, 2 * w + 9); P.H = std::max(12, 2 * h + 6); break;
    case Building::Bathhouse: P.W = std::max(17, 2 * w + 7); P.H = std::max(14, 2 * h + 8); break;
    case Building::TeaHouse: P.W = std::max(P.floors >= 2 ? 19 : 15, 2 * w + 5); P.H = std::max(12, 2 * h + 7); break;
    case Building::Lodge: P.W = std::max(17, 2 * w + 5); P.H = std::max(13, 2 * h + 7); break;
    case Building::CouncilHall: P.W = std::max(17, 2 * w + 7); P.H = std::max(14, 2 * h + 8); break;
    default: break;
  }
  switch (b.type) {
    case Building::Guildhall: case Building::Exchange: case Building::MeadHall: case Building::Bathhouse: case Building::TeaHouse:
    case Building::Lodge: case Building::CouncilHall: break;
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
  // (M3b) a single-storey inn lets its rooms on the ground floor: it needs the room for them
  if (b.type == Building::Inn && P.floors == 1) { P.W += 2; P.H += 3; }
  // seats of power: the hall the society rules from
  if (P.seat >= 0) {
    switch ((cult::Seat)P.seat) {
      case cult::Seat::GreatHall: case cult::Seat::StiltHall: P.W = std::max(P.W, 27); P.H = std::max(P.H, 15); break;
      case cult::Seat::TentCourt: case cult::Seat::TreePalace: P.W = std::max(P.W, P.royal ? 25 : 21); break;
      case cult::Seat::CourtPalace: P.W = std::max(P.W, 27); P.H = std::max(P.H, 20); break;
      default: P.W = std::max(P.W, 23); P.H = std::max(P.H, 16); break;
    }
  }
  const bool big = b.type == Building::Palace || P.seat >= 0;
  P.W = std::min(oddUp(P.W), big ? 45 : 31);
  P.H = std::min(P.H, big ? 26 : 24);
  // the blueprint's shape sets the proportions: a round body is a disc (as deep as it is wide), a long hall at least
  // twice as wide as deep, a cross deep enough for its transepts
  switch (P.fp) {
    case bld::Floorplan::Round:
      P.W = oddUp(std::clamp(std::max(P.W, P.H - 1), 9, 25));
      if (P.floors >= 3) P.W = std::max(P.W, 17);   // two flights on the back arc, three tiles apart
      if (b.type == Building::Inn && P.floors == 1) P.W = std::min(25, P.W + 2);   // (fix) a one-storey yurt inn: its alcoves round the drum need the room
      P.H = P.W + 1;
      break;
    case bld::Floorplan::Long: P.W = std::min(oddUp(std::max(P.W, P.H * 2 - 3)), big ? 45 : 37); break;
    case bld::Floorplan::Cross: P.H = std::max(P.H, 14); P.W = std::max(P.W, 15); break;
    case bld::Floorplan::Courtyard: {
      P.W = std::max(P.W, 17);
      P.H = std::max(P.H, 17);
      // a purpose planned behind a front court (planFrontCourt) keeps its own depth and gains the court's
      const bool home = b.type == Building::House || b.type == Building::StoneHouse || b.type == Building::Farmhouse;
      const bool own = home || (b.type == Building::Inn && P.floors == 1) || P.seat == (int)cult::Seat::CourtPalace;
      if (!own) P.H = std::min(P.H + 7, big ? 30 : 28);
      break;
    }
    case bld::Floorplan::L: {
      // the body (the arm with the door) keeps the purpose's own size; the yard corner and the wing beside it are added:
      // a front-corner yard widens the floor, a back-corner yard deepens it
      P.W = std::max(P.W, 15); P.H = std::max(P.H, 12);
      const int lw0 = std::max(4, (P.W - 2) / 3), lh0 = std::max(3, (P.H - 3) / 3);
      if (P.lCorner >= 2) P.W = std::min(P.W + lw0 + 1, big ? 45 : 37);
      else P.H = std::min(P.H + lh0 + 2, big ? 30 : 28);
      break;
    }
    default: break;
  }
  P.W = oddUp(P.W);
  P.ex = P.W / 2;
  // the shapes' parts
  if (P.fp == bld::Floorplan::L) {
    // the missing corner (the yard): a third of the body each way; a front corner keeps clear of the door column
    int lw = std::max(4, (P.W - 2) / 4), lh = std::max(3, (P.H - 3) / 4);
    if (P.lCorner >= 2) lw = std::min(lw, P.ex - 3);
    if (lw < 4 || P.W - 2 - lw < 6 || P.H - 3 - lh < 5) { P.fp = bld::Floorplan::Rect; return; }
    const bool east = P.lCorner == 1 || P.lCorner == 3, south = P.lCorner >= 2;
    P.lx0 = east ? P.W - 1 - lw : 1; P.lx1 = east ? P.W - 2 : lw;
    P.ly0 = south ? P.H - 1 - lh : 2; P.ly1 = south ? P.H - 2 : 1 + lh;
  }
  if (P.fp == bld::Floorplan::Cross && b.type != Building::Temple) P.fp = bld::Floorplan::Rect;
  if (P.fp == bld::Floorplan::Cross) {
    P.naveHW = std::max(2, (P.W - 2) / 6);
    P.tr0 = P.H >= 17 ? 4 : 3;
    P.tr1 = P.tr0 + (P.H >= 18 ? 4 : 3);
    if (P.ex - P.naveHW < 5) P.fp = bld::Floorplan::Rect;
  }
  if (P.fp == bld::Floorplan::Courtyard) {
    // the back range (rows 2 .. 1 + depth), its wall, the court, the front range's wall and at least three rows of it
    const int cw = P.courtW ? P.courtW : 7, ch = P.courtH ? P.courtH : 7;
    const int bdep = P.H >= 21 ? 5 : 4;
    int w2 = std::clamp((P.W - 2) * cw / 16, 3, P.W - 10);
    if (w2 % 2 == 0) w2--;   // centred on the door's axis
    P.cx0 = P.ex - w2 / 2; P.cx1 = P.ex + w2 / 2;
    P.cy0 = 2 + bdep + 2;
    const int h2 = std::clamp((P.H - 3) * ch / 16, 3, P.H - P.cy0 - 6);
    P.cy1 = P.cy0 + h2 - 1;
    if (h2 < 3 || P.cy1 > P.H - 7 || P.cx0 < 5) P.fp = bld::Floorplan::Rect;
  }
}

// the template for this building (by the seat it is, its purpose, the blueprint's floor plan and its people); false
// when the random draw does not fit (makePlan draws again)
bool planFor(Plan& T, Rng& r);

Plan makePlan(const Bldg& b, const bld::Blueprint& bp, uint32_t seed) {
  Plan P;
  P.b = &b;
  P.type = b.type;
  P.floors = std::max(1, std::min(3, b.floors()));
  // M3b: the blueprint's interior shape and the society's seat
  P.fp = bp.interior.plan < bld::Floorplan::COUNT ? bp.interior.plan : bld::Floorplan::Rect;
  P.lCorner = bp.interior.lCorner & 3;
  P.courtW = bp.interior.courtW; P.courtH = bp.interior.courtH;
  P.culture = bp.interior.culture >= 1 && bp.interior.culture <= (int)cult::Archetype::COUNT ? bp.interior.culture - 1 : -1;
  P.furn = bp.interior.furniture;
  P.seat = (b.civic & bld::CIVIC_SEAT) && b.seat >= 1 && b.seat <= (int)cult::Seat::COUNT ? b.seat - 1 : -1;
  if (P.seat < 0 && (b.type == Building::Palace || b.type == Building::Keep) && b.civic) P.seat = (int)cult::Seat::Castle;
  // (fix) a lord's keep without the seat flag (a capital's keep for the main quest, a city's jarl's keep) is built
  // outside as its people's seat (bld::design: the culture's own seat for a keep): its inside follows the same plan,
  // a khan's lord in a tent court, a jarl in a great hall, never the shared European castle hall
  if (P.seat < 0 && b.styled && (b.type == Building::Palace || b.type == Building::Keep) && bp.seat >= 1 && bp.seat <= (int)cult::Seat::COUNT) P.seat = bp.seat - 1;
  P.royal = bldgIsRoyalSeat(b);
  // (M3b fixer) the starspire council spire is round outside: its council chamber is round inside too (a square
  // checkered hall inside a round spire read wrong)
  if (P.seat == (int)cult::Seat::CouncilSpire && (P.fp == bld::Floorplan::Rect || P.fp == bld::Floorplan::Long)) P.fp = bld::Floorplan::Round;
  if (const bld::Volume* v = bp.body()) {
    P.bodyW = (v->x1 - v->x0 + 15) / 16;
    P.bodyH = (v->y1 - v->y0 + 15) / 16;
  }
  planSize(P);
  Rng wr(seed ^ 0x2F6A9D1Bu);
  P.wealth = b.type == Building::Hut ? 0 : (b.type == Building::StoneHouse ? 2 : 1);
  if (b.type == Building::Keep || b.type == Building::Temple || b.type == Building::Palace) P.wealth = 3;
  if (b.type == Building::Barracks) P.wealth = 1;
  if (P.floors >= 2 && P.wealth == 1 && wr.f() < 0.3f) P.wealth = 2;
  if (P.wealth > 0 && P.wealth < 3 && wr.f() < 0.2f) P.wealth--;
  // (M3b) the builder's wealth (Bldg::wealth: size and finery) lifts or lowers it a step
  if (b.styled && P.wealth < 3) P.wealth = std::clamp(P.wealth + ((int)b.wealth >= 3 ? 1 : 0) - ((int)b.wealth == 0 && P.wealth > 0 ? 1 : 0), 0, 3);
  if (P.seat >= 0 || b.civic) P.wealth = std::max(P.wealth, 2);
  if (P.seat >= 0) P.wealth = 3;
  Rng r(seed ^ 0xB4A1E5C3u);
  for (int attempt = 0; attempt < 40; attempt++) {
    Plan T = P;
    T.geo.assign((size_t)T.floors, Geo());
    T.mir = r.f() < 0.5f;
    bool ok = planFor(T, r);
    if (ok && T.geo[0].roomOf(T.ex, T.H - 2) < 0) ok = false;   // the front door opens onto a floor
    // the front door opens into the main room (a front court's gate: into the court, whose door leads on)
    if (ok && T.geo[0].roomOf(T.ex, T.H - 2) != 0 && T.tmpl != Tmpl::FrontCourt) ok = false;
    if (ok) {
      for (int f = 0; f < T.floors; f++) {
        const Geo& g = T.geo[(size_t)f];
        if (f + 1 < T.floors && !g.up.valid()) ok = false;
        if (f > 0 && !g.down.valid()) ok = false;
        for (size_t i = 1; i < g.rooms.size(); i++) if (g.rooms[i].doorX < 0) ok = false;
        // (M3b) every floor tile reachable from the way in (the front door, or the stairs up to this floor)
        if (ok) {
          const int sx = f == 0 ? T.ex : g.down.x, sy = f == 0 ? T.H - 2 : g.down.y;
          std::vector<uint8_t> seen((size_t)T.W * T.H, 0);
          std::vector<int> q{g.I(sx, sy)};
          seen[(size_t)g.I(sx, sy)] = 1;
          for (size_t h = 0; h < q.size(); h++) {
            const int x = q[h] % T.W, y = q[h] / T.W;
            static const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
            for (int k = 0; k < 4; k++) {
              const int nx = x + dx[k], ny = y + dy[k];
              if (!g.isFloor(nx, ny) || seen[(size_t)g.I(nx, ny)]) continue;
              seen[(size_t)g.I(nx, ny)] = 1;
              q.push_back(g.I(nx, ny));
            }
          }
          int fl = 0;
          for (int i = 0; i < T.W * T.H; i++) fl += g.wall[(size_t)i] == T_FLOOR;
          if ((int)q.size() != fl) ok = nope(__LINE__);
        }
      }
    }
    if (ok) {
      for (Geo& g : T.geo) g.bbox();   // the rooms' boxes now include their doorways
      numberGuestsAll(T);
      return T;
    }
  }
  Plan T = P;
  T.geo.assign((size_t)T.floors, Geo());
  planPlain(T);
  for (Geo& g : T.geo) g.bbox();
  return T;
}

// the classic templates by purpose (M0b..M3): rect plans, which also run on shaped floors (round, L, cross: the rooms
// are carved clipped to the shape)
bool planClassic(Plan& T, Rng& r) {
  const Bldg& b = *T.b;
  bool ok = false;
  T.tmpl = Tmpl::Classic;
  {
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
        T.initFloor(T.geo[0], mainKindOf(b.type, 0, 1));
        ok = T.floors == 1 && T.geo[0].finish();
        break;
    }
  }
  return ok;
}

bool planFor(Plan& T, Rng& r) {
  const Building t = T.type;
  if (T.seat >= 0) return planSeat(T, r);
  if (t == Building::Inn) return planInnCulture(T, r);
  const bool home = t == Building::House || t == Building::StoneHouse || t == Building::Hut || t == Building::Farmhouse;
  const bool society = (int)t >= (int)Building::Guildhall;
  if (society) {
    // the society's buildings: a court before them, their plan on an L's body, round halls; else their own plans
    if (T.fp == bld::Floorplan::Courtyard) return planFrontCourt(T, r);
    if (T.fp == bld::Floorplan::L && planEmbeddedL(T, r)) return true;
    if (T.fp == bld::Floorplan::Round) {
      if (t == Building::MeadHall) return planMeadHall(T, r, Tmpl::MeadHall);
      if (t == Building::Bathhouse) return planBathhouse(T, r);
      if (t == Building::CouncilHall) return planCouncilHall(T, r, Tmpl::CouncilHall);
      if (t == Building::Exchange) return planRoundStrict(T, r);
      return planRoundHall(T, r);
    }
    switch (t) {
      case Building::Guildhall: return planGuildhall(T, r, Tmpl::Guildhall);
      case Building::Exchange: return planExchange(T, r);
      case Building::MeadHall: return planMeadHall(T, r, Tmpl::MeadHall);
      case Building::Bathhouse: return planBathhouse(T, r);
      case Building::TeaHouse: return planTeaHouse(T, r);
      case Building::Lodge: return planLodge(T, r);
      default: return planCouncilHall(T, r, Tmpl::CouncilHall);
    }
  }
  if ((t == Building::Keep || t == Building::Palace) && (T.fp == bld::Floorplan::L || T.fp == bld::Floorplan::Round)) {
    const bool ok = planCastle(T, r);
    T.tmpl = Tmpl::Classic;
    return ok;
  }
  // the purposes whose rooms 15.7 checks closely (a counter, a forge on an outer wall, a throne hall...) keep their own
  // plans on any shape; the rest fall back to the shape's plain plan
  const bool strict = t == Building::Shop || t == Building::Bakery || t == Building::Butcher || t == Building::Fishmonger ||
                      t == Building::Weaver || t == Building::Smithy || t == Building::Smelter || t == Building::Keep ||
                      t == Building::Palace || t == Building::Barracks || t == Building::Temple;
  auto classicOr = [&](bool (*alt)(Plan&, Rng&)) {
    if (planClassic(T, r)) return true;
    if (strict) return false;
    T.barY = T.barX0 = T.barX1 = T.flapX = T.forgeX = T.pitY = -1;
    return alt(T, r);
  };
  switch (T.fp) {
    case bld::Floorplan::Round:
      if (t == Building::Tower || t == Building::Windmill) return planTower(T, r);
      if (home) return planRoundHome(T, r);
      if (t == Building::Temple) return planRoundTemple(T, r);
      if (strict) return planRoundStrict(T, r);
      return planRoundHall(T, r);
    case bld::Floorplan::Courtyard:
      if (home && t != Building::Hut) return planCourtHouse(T, r);
      return planFrontCourt(T, r);
    case bld::Floorplan::L:
      if (home && t != Building::Hut) return planLHouse(T, r);
      if (planEmbeddedL(T, r)) return true;
      return classicOr(planShapedPlain);
    case bld::Floorplan::Cross:
      if (t == Building::Temple) return planCrossTemple(T, r);
      return classicOr(planShapedPlain);
    case bld::Floorplan::Long:
      if (home) return planLonghouse(T, r);
      return planClassic(T, r);
    default: return planClassic(T, r);
  }
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
bool usableProp(Prop p) { return p == Prop::Bed || p == Prop::BunkBed || p == Prop::Chest || p == Prop::Altar || p == Prop::Hammock; }

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
  // M3 (VISION_PLAN 5.6): the culture's furnishing (Bldg::arch): what people sit and sleep on, the accent its rugs take,
  // and whether its rooms are strewn with rugs (felt yurts, living-wood halls)
  art::Furniture furn = art::Furniture::Chairs;
  uint32_t accent = 0;
  float rugBias = 0;
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
  bool wallT(int x, int y) const { return !m.in(x, y) || m.at(x, y) == Ground::InteriorWall || m.at(x, y) == Ground::Void; }
  bool freeT(int x, int y) const { return floorT(x, y) && !m.propAt(x, y) && !lane[(size_t)I(x, y)]; }
  int roomOf(int x, int y) const { return g.roomOf(x, y); }
  bool inRoom(int x, int y, int ri) const { return floorT(x, y) && roomOf(x, y) == ri; }
  bool wallN(int x, int y) const { return wallT(x, y - 1); }
  // (M3b) the wall north of (x, y) is the outer wall: the back wall (row 1), or the shell of a shaped floor
  bool outerN(int x, int y) const { return wallT(x, y - 1) && (y - 1 <= 1 || m.decoAt(x, y - 1) == (int)Deco::Shell); }
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
  bool offFloor = false;   // (M3b) a group put something off the floor (a shaped room's edge): it fails
  void put(int x, int y, Prop p, int gid) {
    if (!floorT(x, y)) { offFloor = true; return; }
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
    offFloor = false;
    if (!place(gid) || offFloor || !valid()) { rollback(mk); offFloor = false; return false; }
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
// (fixer M6b r3) pieces whose sprite is wider than their tile: a hand cart (30 px), a haystack, a tall cabinet. They
// spill over the tiles beside them, so nothing else stands there (two cabinets hid the table between them), and the
// cart and the haystack keep off walls and doorways at their sides (seed 42's inn: a cart across a partition's door)
bool wideProp(Prop p) { return p == Prop::Cart || p == Prop::Haystack || p == Prop::Cupboard || p == Prop::Wardrobe; }
bool sideClear(const Fit& F, int x, int y, Prop p) {
  const bool wide = wideProp(p), broad = p == Prop::Cart || p == Prop::Haystack;
  for (int dx = -1; dx <= 1; dx += 2) {
    const int q = F.m.propAt(x + dx, y);
    const bool real = q && q != (int)Prop::Filler + 1;
    if (wide && real) return false;
    if (real && wideProp((Prop)(q - 1))) return false;
    if (broad) {
      if (!F.floorT(x + dx, y)) return false;
      for (int dy = -1; dy <= 1; dy++) {   // (nor beside a doorway in the wall behind or in front of it)
        if (!F.m.in(x + dx, y + dy)) continue;
        const int i = F.I(x + dx, y + dy);
        if (std::find(F.g.doorTiles.begin(), F.g.doorTiles.end(), i) != F.g.doorTiles.end()) return false;
      }
      if (std::abs(x + dx - F.sx) + std::abs(y - F.sy) <= 1) return false;   // nor beside the way in
    }
  }
  return true;
}
bool northPiece(Fit& F, int ri, Prop p, int where, int* outX = nullptr, int* outY = nullptr, bool outerOnly = false) {
  std::vector<std::pair<int, int>> c;
  F.tiles(ri, [&](int x, int y) {
    if (!F.wallN(x, y) || !F.freeT(x, y) || (outerOnly && !F.outerN(x, y))) return;
    if (!sideClear(F, x, y, p)) return;
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
// M3: where a people sleeps in hammocks, a home's bed is a hammock slung against the north wall (a poor hut's a sleeping
// mat rolled out by it); a rented room, a palace or a keep keeps its bed (the inn's guests sleep in theirs)
Prop sleepPropFor(const Fit& F, RoomKind kind) {
  if (F.furn != art::Furniture::Hammocks || kind == RoomKind::GuestRoom || kind == RoomKind::Barracks) return Prop::Bed;
  const Building t = F.P.type;
  if (t == Building::Palace || t == Building::Keep || t == Building::Inn) return Prop::Bed;
  return (t == Building::Hut || F.P.wealth == 0) ? Prop::SleepingMat : Prop::Hammock;
}
bool bedPiece(Fit& F, int ri, int where, int* outX, int* outY, Prop piece = Prop::Bed) {
  std::vector<std::pair<int, int>> c;
  F.tiles(ri, [&](int x, int y) {
    if (!F.wallN(x, y) || !F.freeT(x, y) || !F.freeT(x, y + 1) || F.roomOf(x, y + 1) != ri) return;
    // (M3b fixer) on a shaped floor (a yurt's stepped arc) a bed keeps a column clear of any other tall piece: two
    // beds a step apart on the arc drew into each other
    if (F.g.shaped)
      for (int oy = -2; oy <= 3; oy++)
        for (int ox = -1; ox <= 1; ox++) {
          const int q = F.m.propAt(x + ox, y + oy);
          if (q && (q == (int)Prop::Filler + 1 || tallProp((Prop)(q - 1)))) return;
        }
    c.push_back({x, y});
  });
  F.shuffle(c);
  if (where == 1) std::stable_sort(c.begin(), c.end(), [&](const std::pair<int, int>& a, const std::pair<int, int>& b) { return F.wallSide(a.first, a.second) > F.wallSide(b.first, b.second); });
  for (auto& t : c)
    if (F.group([&](int gid) { F.put(t.first, t.second, Prop::Filler, gid); F.put(t.first, t.second + 1, piece, gid); return true; })) {
      *outX = t.first; *outY = t.second + 1;
      return true;
    }
  return false;
}
// the row of a bed's head (the wall it stands against is right above it)
int bedHead(const Fit& F, int bx, int by) { return F.m.propAt(bx, by - 1) == (int)Prop::Filler + 1 ? by - 1 : by; }
// (M3 fixer round 2) the last resort for a home with no room left for its bed: a sleeping mat rolled out by the back
// wall, even on a walkway (it is stepped over), so no home is without somewhere to sleep
bool matAnywhere(Fit& F, int ri, int* outX, int* outY) {
  std::vector<std::pair<int, int>> c;
  F.tiles(ri, [&](int x, int y) { if (F.wallN(x, y) && F.floorT(x, y) && !F.m.propAt(x, y)) c.push_back({x, y}); });
  for (auto& t : c)
    if (F.group([&](int gid) { F.put(t.first, t.second, Prop::SleepingMat, gid); return true; })) {
      *outX = t.first; *outY = t.second;
      return true;
    }
  return false;
}

// a 3-wide piece (Filler, p, Filler) against the north wall centred on cx (or anywhere when cx < 0)
bool northWide(Fit& F, int ri, Prop p, int cx, bool outerOnly, int* outX = nullptr) {
  std::vector<std::pair<int, int>> c;
  F.tiles(ri, [&](int x, int y) {
    if (cx >= 0 && x != cx) return;
    if (outerOnly && !F.outerN(x, y)) return;
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
    if (!sideClear(F, x, y, p)) return;
    // (fixer M6b r3) a tall piece (a folding screen) never stands in front of another piece, nor a piece behind one:
    // the screen hid the chests behind it
    auto tallAt = [&](int q) { return q && q != (int)Prop::Filler + 1 && ((Prop)(q - 1) == Prop::FoldScreen || tallProp((Prop)(q - 1))); };
    const int qN = F.m.propAt(x, y - 1), qS = F.m.propAt(x, y + 1);
    if ((p == Prop::FoldScreen || tallProp(p)) && qN && qN != (int)Prop::Filler + 1 && qN != (int)Prop::DoorH + 1 && qN != (int)Prop::DoorV + 1) return;
    if (tallAt(qS)) return;
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
  // M3: a people who sit on the floor eat at a low table on cushions (one table, cushions all round); a benches people
  // seat their long tables with benches; a stools people use stools. Work tables stay work tables.
  const bool dining = single != Prop::TableWork;
  const bool cushions = F.furn == art::Furniture::Cushions && dining;
  if (cushions) { len = 1; single = Prop::LowTable; benches = false; }
  else if (F.furn == art::Furniture::Benches && dining && len >= 2) benches = true;
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
    if (cushions) seat = Prop::Cushion;
    else if (F.furn == art::Furniture::Stools || F.furn == art::Furniture::Hammocks || F.furn == art::Furniture::Cushions) seat = Prop::Stool;   // (a work table: a stool)
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
  // (fixer M6b r3) a rug never lies over part of another: the second one was cut into an L round the first, or its
  // border crossed the other's (seed 7's yurt keep). One that would overlap a rug already down is left rolled up; a
  // runner (one tile wide) passes under it as before
  if (x0 != x1 && y0 != y1)
  for (int y = y0; y <= y1; y++)
    for (int x = x0; x <= x1; x++) {
      if (!F.inRoom(x, y, ri)) continue;
      const int k = F.m.decoAt(x, y);
      if (k >= (int)Deco::RugRed && k <= (int)Deco::RugGold) return;
    }
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
    case Prop::LowTable: case Prop::Cushion:   // (M3)
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
      // (M3c fixer round 3) on a shaped floor (a round room) a rug keeps a tile clear of the shell: the wall is drawn
      // on the true curve through the edge tiles, and a rug there was cut off by it
      if (ok && F.g.shaped)
        for (int y = y0 - 1; y <= y0 + h && ok; y++)
          for (int x = x0 - 1; x <= x0 + w && ok; x++) ok = F.floorT(x, y);
      // (fixer M6b r3) nor right against a rug of its own colour: the two merged into one L-shaped rug with a notch
      for (int y = y0 - 1; y <= y0 + h && ok; y++)
        for (int x = x0 - 1; x <= x0 + w && ok; x++)
          if ((y == y0 - 1 || y == y0 + h || x == x0 - 1 || x == x0 + w) && F.m.decoAt(x, y) == (int)d) ok = false;
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
  // M3: most rugs in the culture's accent colour (the nearest of the four dyes)
  if (F.accent && F.r.f() < 0.75f) {
    static const int ref[4][3] = {{170, 40, 40}, {40, 70, 160}, {40, 120, 60}, {200, 160, 50}};
    const int ar = (int)(F.accent & 255u), ag = (int)((F.accent >> 8) & 255u), ab = (int)((F.accent >> 16) & 255u);
    int best = 0, bd = 1 << 30;
    for (int k = 0; k < 4; k++) {
      const int d = (ar - ref[k][0]) * (ar - ref[k][0]) + (ag - ref[k][1]) * (ag - ref[k][1]) + (ab - ref[k][2]) * (ab - ref[k][2]);
      if (d < bd) { bd = d; best = k; }
    }
    return rugs[best];
  }
  if (F.biome == Biome::Snow || F.biome == Biome::Taiga) return F.r.f() < 0.6f ? Deco::RugGold : Deco::RugRed;   // furs and wool
  if (F.biome == Biome::Desert) return F.r.f() < 0.5f ? Deco::RugRed : Deco::RugBlue;
  return rugs[F.r.irange(4)];
}

// a room much bigger than its anchors gets a sitting corner and more storage, so it never reads as an empty hall
void bigRoomFill(Fit& F, int ri) {
  const IRect& R = F.g.rooms[(size_t)ri].r;
  int area = R.w * R.h;
  if (area < 28) return;
  const int seats = 2 + F.r.irange(2);   // (sequenced: the right argument's draw first, as MSVC/GCC)
  const Prop top = F.r.f() < 0.5f ? Prop::TableSmall : Prop::TableMeal;
  tableIn(F, ri, 1, top, seats, false, 40);
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
  else if (sleepPropFor(F, kind) != Prop::Bed) {
    // (M3 fixer round 2) two tiles long like a bed (its head on the wall tile's Filler), so it reads as the room's bed;
    // a single-tile piece by the wall only where a room has no space for it
    const Prop sp = sleepPropFor(F, kind);
    if (!bedPiece(F, ri, 1, &bx, &by, sp) && !northPiece(F, ri, sp, 1, &bx, &by)) northPiece(F, ri, sp, 0, &bx, &by);
  } else if (!bedPiece(F, ri, 1, &bx, &by) && !northPiece(F, ri, Prop::Bed, 1, &bx, &by)) northPiece(F, ri, Prop::Bed, 0, &bx, &by);
  if (bx < 0 && !barracks) matAnywhere(F, ri, &bx, &by);
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
  if (F.r.f() < (kind == RoomKind::GuestRoom ? 0.45f : 0.65f) + F.rugBias && R.h >= 3) {
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
    // (M3b) no outer wall for it (a kitchen behind a partition): the fire on any wall, its flue up the partition
    if (hx < 0 && !northPiece(F, ri, Prop::Oven, 1, &hx)) northWide(F, ri, Prop::Hearth, -1, false, &hx);
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

int centreFire(Fit& F, int ri, int x, int y);
void longHearth(Fit& F, int ri, int y, int x0, int x1);
void furnishFeast(Fit& F, int ri, Ctx& cx);
void livingTrunk(Fit& F, int ri, int x, int y);

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
  auto placeBed = [&]() {   // one-room home: the bed in a back corner, a chest at its foot
    int bx = -1, by = -1;
    const Prop sp = sleepPropFor(F, kind);
    if (bedPiece(F, ri, 1, &bx, &by, sp) || northPiece(F, ri, sp, 1, &bx, &by) || matAnywhere(F, ri, &bx, &by)) {
      RoomDef& RD = const_cast<RoomDef&>(F.g.rooms[(size_t)ri]);
      RD.bedX = bx; RD.bedY = by;
      const int hy = bedHead(F, bx, by);
      if (F.r.f() < 0.6f) F.group([&](int gid) { if (!F.freeT(bx, by + 1)) return false; F.put(bx, by + 1, Prop::Chest, gid); return true; });
      if (F.r.f() < 0.25f) F.group([&](int gid) { int s = F.wallT(bx - 1, hy) ? 1 : -1; if (!F.freeT(bx + s, hy) || !F.wallN(bx + s, hy)) return false; F.put(bx + s, hy, Prop::Cradle, gid); return true; });
    }
  };
  // (M3 fixer round 2) a hut's one small room takes its bed first: its back wall could be all hearth, leaving the
  // family nowhere to sleep
  const bool bedFirst = cottage && b.type == Building::Hut;
  if (bedFirst) placeBed();
  // (M3b) a round home's fire burns in its middle under the crown (a yurt's stove), a longhouse's down its length
  int hx = -1;
  if (F.m.floor == 0 && F.P.tmpl == Tmpl::RoundHome) hx = centreFire(F, ri, F.P.ex, F.H / 2);
  if (F.m.floor == 0 && F.P.tmpl == Tmpl::Longhouse && F.P.pitY >= 0) { longHearth(F, ri, F.P.pitY, F.P.pitX0, F.P.pitX1); hx = F.P.pitX0; }
  // the hearth on the outer back wall (a home without a separate kitchen)
  if (hx < 0 && F.P.b->hearth && !cx.kitchenFire && F.m.floor == 0) {
    bool desert = F.biome == Biome::Desert && F.r.f() < 0.8f;   // a domed clay oven, not an open fire
    if (desert) northPiece(F, ri, Prop::Oven, 1, &hx, nullptr, true);
    if (hx < 0 && !northWide(F, ri, Prop::Hearth, -1, true, &hx) && F.r.f() < 0.5f) northPiece(F, ri, Prop::Oven, 1, &hx, nullptr, true);
  }
  if (cottage && !bedFirst) placeBed();
  // (M3b) round homes and longhouses sleep the family round the walls: more beds or mats under the back arc
  if (cottage && (F.P.tmpl == Tmpl::RoundHome || F.P.tmpl == Tmpl::Longhouse)) {
    const Prop sp = sleepPropFor(F, kind);
    for (int k = 0; k < (R.w >= 11 ? 2 : 1); k++) { int bx = -1, by = -1; if (!bedPiece(F, ri, 1, &bx, &by, sp)) break; }
  }
  // the table: in the middle of the hall, chairs around it, a rug under it in better homes
  bool big = R.w >= 6 && F.r.f() < 0.6f;
  Prop top = F.r.f() < 0.55f ? Prop::TableMeal : (b.owner == Role::Mage || (F.P.wealth >= 2 && F.r.f() < 0.4f) ? Prop::TableWork : Prop::TableSmall);
  int tx = -1, ty = -1;
  if (tableIn(F, ri, big ? 2 : 1, top, 2 + F.r.irange(3), false, 40, &tx, &ty) || tableIn(F, ri, 1, top, 2, false, 40, &tx, &ty)) {
    if (cx.ownerX < 0) { cx.ownerX = tx; cx.ownerY = ty + 1; }
    if ((F.P.wealth >= 1 && F.r.f() < 0.65f + F.rugBias) || F.biome == Biome::Desert || F.furn == art::Furniture::Cushions) rugRect(F, ri, tx - 1, ty - 1, tx + (big ? 2 : 1), ty + 1, rugFor(F));
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
  if (F.P.type == Building::Bathhouse && kind == RoomKind::Storeroom && F.m.floor == 0) {   // (M3b) the bath's furnace room
    if (!northWide(F, ri, Prop::Hearth, -1, false)) northPiece(F, ri, Prop::Oven, 1);
    for (int k = 0; k < 2 + R.w * R.h / 12; k++) wallPiece(F, ri, k % 3 == 2 ? Prop::Barrel : Prop::Woodpile);
    return;
  }
  // (M3b fixer) the jade and dune court palaces keep their treasury by the gate, a theocracy's high temple its
  // offerings: chests of tribute, sacks and baskets, jars, never the castle's armoury of weapon racks
  const bool treasury = F.m.floor == 0 && kind == RoomKind::Storeroom && (F.P.type == Building::Palace || F.P.type == Building::Keep) &&
                        ((F.P.tmpl == Tmpl::SeatCourt && (F.P.culture == (int)cult::Archetype::Jade || F.P.culture == (int)cult::Archetype::Dune)) ||
                         F.P.tmpl == Tmpl::SeatSanctum);
  if (treasury) {
    northPiece(F, ri, Prop::Shelf, 1);
    if (R.w >= 5) northPiece(F, ri, F.r.f() < 0.5f ? Prop::Cupboard : Prop::Shelf, 0);
    static const Prop tr[6] = {Prop::Chest, Prop::Sacks, Prop::Chest, Prop::Baskets, Prop::Urn, Prop::Barrel2};
    for (int k = 0; k < 3 + R.w * R.h / 8; k++) wallPiece(F, ri, tr[(size_t)((k + F.r.irange(2)) % 6)]);
    return;
  }
  if ((F.P.type == Building::Keep || F.P.type == Building::Palace || F.P.type == Building::Barracks || F.P.type == Building::Lodge) && kind == RoomKind::Storeroom && F.m.floor == 0) {   // the keep's armoury (M3b: the lodge's)
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
    // (draws sequenced: right argument first, as MSVC/GCC evaluate them)
    const bool cloth = F.r.f() < 0.3f;
    const int seats = 2 + F.r.irange(2);
    const Prop top = F.r.f() < 0.5f ? Prop::TableMeal : Prop::TableSmall;
    const int tlen = F.r.f() < 0.5f ? 2 : 1;
    if (tableIn(F, ri, tlen, top, seats, cloth, 40, &tx, &ty))
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
        const bool lcloth = F.r.f() < 0.4f;   // (sequenced: the right argument's draw first, as MSVC/GCC)
        const int lseats = 4 + F.r.irange(2);
        if (tableIn(F, ri, 3, Prop::TableMeal, lseats, lcloth, 60, &lx, &ly))
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
  // (M3b) the inn's own idiom: the mead hall's long hearth, the yurt's stove, the tree hall's living trunk
  const Tmpl t = F.P.tmpl;
  const int midY = (std::max(F.P.barY + 4, R.y) + R.y + R.h - 1) / 2;
  if (t == Tmpl::InnMeadHall && F.P.pitY >= 0) longHearth(F, ri, F.P.pitY, F.P.pitX0, F.P.pitX1);
  if (t == Tmpl::InnYurt || t == Tmpl::InnBooths) centreFire(F, ri, F.P.ex, midY);
  if (t == Tmpl::InnTreeHall) livingTrunk(F, ri, F.P.ex, midY);
  // the hearth on the common room's own stretch of back wall
  int hx = -1;
  if (t != Tmpl::InnYurt && t != Tmpl::InnBooths && northWide(F, ri, Prop::Hearth, -1, true, &hx)) rugRect(F, ri, hx - 1, 3, hx + 1, 4, rugFor(F));
  if (t == Tmpl::InnTeaHouse) {   // the tea house's screens and low tables
    for (int k = 0; k < 2; k++) wallPiece(F, ri, Prop::FoldScreen, true);
  }
  // (M3b fixer) the inns' own halls: the mead hall's roof posts in two rows along the hearth and its long boards with
  // benches; the pilgrims' hostel's refectory, long tables in ranks down both sides of the way
  if (t == Tmpl::InnMeadHall && F.P.pitY >= 0)
    for (int k = 0; k < 2; k++) {
      const int py = F.P.pitY + (k ? 3 : -3);
      for (int x = R.x + 2; x < R.x + R.w - 2; x += 4)
        if (std::abs(x - F.P.ex) > 1) F.group([&](int gid) { if (!pillarOk(F, x, py, ri) || !F.gapOk(x, py, x, py, gid)) return false; F.put(x, py, Prop::Pillar, gid); return true; });
    }
  int refectory = 0;
  if (t == Tmpl::InnHostel) {
    const int y0 = F.P.barY + 5;
    for (int y = y0; y <= R.y + R.h - 2; y += 3)
      for (int side = 0; side < 2; side++) {
        const int x0 = side ? F.P.ex + 3 : R.x + 2, x1 = side ? R.x + R.w - 3 : F.P.ex - 3;
        const int len = std::min(5, x1 - x0 + 1);
        if (len < 2) continue;
        if (F.group([&](int gid) { return tableSet(F, ri, side ? x0 : x1 - len + 1, y, len, Prop::TableMeal, 0, true, gid); })) { cx.seats.push_back({x0, y + 1}); refectory++; }
      }
  }
  // tables: long ones with benches and round ones with stools, as many as the gap rule allows
  int want = t == Tmpl::InnHostel && refectory >= 2 ? 1 : 4 + R.w * R.h / 40 + F.r.irange(2);
  const bool longOnly = t == Tmpl::InnMeadHall;
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
    bool lng = longOnly || F.r.f() < 0.7f;
    int len = lng ? 2 + F.r.irange(2) : 1;
    Prop top = F.r.f() < 0.6f ? Prop::TableMeal : Prop::TableSmall;
    bool benches = lng && (longOnly || F.r.f() < 0.65f);
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
  if (fx < 0 && F.g.shaped) northWide(F, ri, Prop::Forge, -1, false, &fx);   // (M3b: an L's body: its flue in the wing's wall)
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

void furnishNave(Fit& F, int ri, Ctx& cx);   // (M3b fixer round 3) each people's own: after the seat halls below

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

void furnishSeatHall(Fit& F, int ri, Ctx& cx);
void furnishCourtHall(Fit& F, int ri, Ctx& cx);
bool screenedNook(Fit& F, int ri, int x, int y, Prop a, Prop b);
void furnishThrone(Fit& F, int ri, Ctx& cx) {
  // (M3b) the seats of power in their society's idiom (furnishSeatHall); the castle's and the court palace's hall below
  if (F.P.tmpl != Tmpl::SeatCastle && F.P.tmpl != Tmpl::SeatCourt && F.P.tmpl != Tmpl::Classic && F.P.tmpl != Tmpl::Plain &&
      F.P.tmpl != Tmpl::FrontCourt) {
    furnishSeatHall(F, ri, cx);
    return;
  }
  // (M3b fixer) the jade and dune court palaces' throne halls in their own idiom, never the castle's feasting hall
  if (F.P.tmpl == Tmpl::SeatCourt && (F.P.culture == (int)cult::Archetype::Jade || F.P.culture == (int)cult::Archetype::Dune)) {
    furnishCourtHall(F, ri, cx);
    return;
  }
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

// ===================================================================================================== M3b furnishing
using Arch = cult::Archetype;
Arch cultureOf(const Fit& F) { return F.P.culture >= 0 && F.P.culture < (int)Arch::COUNT ? (Arch)F.P.culture : Arch::COUNT; }
bool feltRoom(const Fit& F) { return interiorStyle(F.m) == art::RoomStyle::Felt || cultureOf(F) == Arch::Steppe; }

// one piece in room ri as near (x, y) as it fits (rings outward, a fixed order), keeping the gap rule when gap
bool putNear(Fit& F, int ri, Prop p, int x, int y, int maxR, bool gap, int* ox = nullptr, int* oy = nullptr) {
  for (int rr = 0; rr <= maxR; rr++)
    for (int dy = -rr; dy <= rr; dy++)
      for (int dx = -rr; dx <= rr; dx++) {
        if (std::max(std::abs(dx), std::abs(dy)) != rr) continue;
        const int tx = x + dx, ty = y + dy;
        if (!F.inRoom(tx, ty, ri) || !F.freeT(tx, ty)) continue;
        if (F.group([&](int gid) { if (gap && !F.gapOk(tx, ty, tx, ty, gid)) return false; F.put(tx, ty, p, gid); return true; })) {
          if (ox) *ox = tx;
          if (oy) *oy = ty;
          return true;
        }
      }
  return false;
}
// a ring of felt or rugs round (x, y) (the fire's tile is left bare)
void rugRing(Fit& F, int ri, int x, int y, int w) {
  Deco d = rugFor(F);
  // (fixer M6b r3) not the colour of a rug already beside it (the two merged into one notched rug)
  auto nearCol = [&](Deco c) {
    for (int dy = -2; dy <= 2; dy++)
      for (int dx = -2; dx <= w + 1; dx++) if (F.m.decoAt(x + dx, y + dy) == (int)c) return true;
    return false;
  };
  for (int k = 0; k < 4 && nearCol(d); k++) d = (Deco)((int)Deco::RugRed + ((int)d - (int)Deco::RugRed + 1) % 4);
  for (int dy = -1; dy <= 1; dy++)
    for (int dx = -1; dx <= w; dx++)
      if ((dy || dx < 0 || dx >= w) && F.inRoom(x + dx, y + dy, ri) && !F.m.propAt(x + dx, y + dy) && F.m.decoAt(x + dx, y + dy) == 0) F.deco(x + dx, y + dy, d);
}
// the fire in the middle of a round room or a hall: a yurt's iron stove under the crown ring (felt rooms, the steppe),
// else a short kerbed fire pit; felt or rugs round it. Returns the fire's column, -1 when none fits
int centreFire(Fit& F, int ri, int x, int y) {
  int fx = -1, fy = -1;
  if (feltRoom(F)) {
    if (putNear(F, ri, Prop::Stove, x, y, 2, true, &fx, &fy)) { rugRing(F, ri, fx, fy, 1); return fx; }
    return -1;
  }
  for (int k = 0; k < 9 && fx < 0; k++) {
    const int tx = x - 1 + (k % 3) - (k % 3 == 2 ? 2 : 0), ty = y + (k / 3 == 1 ? -1 : (k / 3 == 2 ? 1 : 0));
    if (F.group([&](int gid) {
          if (!F.inRoom(tx, ty, ri) || !F.inRoom(tx + 1, ty, ri) || !F.freeT(tx, ty) || !F.freeT(tx + 1, ty)) return false;
          if (!F.gapOk(tx, ty, tx + 1, ty, gid)) return false;
          F.put(tx, ty, Prop::FirePitL, gid); F.put(tx + 1, ty, Prop::FirePitR, gid);
          return true;
        })) { fx = tx; fy = ty; }
  }
  if (fx >= 0) rugRing(F, ri, fx, fy, 2);
  return fx;
}
// the long hearth along row y from x0 to x1 (map columns), split where the way from the door (the axis ex) crosses it,
// with benches down its north side where they fit
void longHearth(Fit& F, int ri, int y, int x0, int x1) {
  const int ex = F.P.ex;
  std::vector<std::pair<int, int>> segs;
  if (x1 - x0 >= 6 && ex > x0 && ex < x1) { segs.push_back({x0, ex - 2}); segs.push_back({ex + 2, x1}); }
  else segs.push_back({x0, x1});
  for (auto& s : segs) {
    if (s.second - s.first < 1) continue;
    const bool ok = F.group([&](int gid) {
      for (int x = s.first; x <= s.second; x++) if (!F.inRoom(x, y, ri) || !F.freeT(x, y)) return false;
      for (int x = s.first; x <= s.second; x++) F.put(x, y, x == s.first ? Prop::FirePitL : (x == s.second ? Prop::FirePitR : Prop::FirePitM), gid);
      return true;
    });
    if (!ok) continue;
    // benches by the fire: down its south side (facing it), a gap at each end to walk round
    for (int x = s.first; x <= s.second; x++)
      if (F.inRoom(x, y + 1, ri) && F.freeT(x, y + 1)) F.group([&](int gid) { F.put(x, y + 1, Prop::Bench, gid); return true; });
  }
  for (int yy = 3; yy < F.H - 1; yy++) F.setLane(ex, yy);
}
// the high seat on the axis at the far wall (the throne, drawn in its people's idiom), banners or posts by it, a dais rug
bool highSeat(Fit& F, int ri, Ctx& cx) {
  const int ex = F.P.ex;
  int y = 2;
  while (y < F.H - 2 && !F.inRoom(ex, y, ri)) y++;
  const bool ok = F.group([&](int gid) {
    if (!F.wallN(ex, y)) return false;
    F.put(ex - 1, y, Prop::Filler, gid); F.put(ex, y, Prop::Throne, gid); F.put(ex + 1, y, Prop::Filler, gid);
    return true;
  });
  if (!ok) return false;
  for (int s : {-2, 2}) if (F.freeT(ex + s, y) && F.inRoom(ex + s, y, ri)) F.group([&](int gid) { F.put(ex + s, y, cultureOf(F) == Arch::Steppe ? Prop::Banner : Prop::Banner, gid); return true; });
  rugRect(F, ri, ex - 1, y + 1, ex + 1, y + 2, Deco::RugGold);
  cx.ownerX = ex; cx.ownerY = y + 1;
  return true;
}
// seats in a ring round (x, y) at radius rad, the aisle up the axis below the centre left open
void ringSeats(Fit& F, int ri, int x, int y, int rad, Prop seat) {
  for (int dy = -rad; dy <= rad; dy++)
    for (int dx = -rad; dx <= rad; dx++) {
      const int d2 = dx * dx + dy * dy;
      if (d2 < rad * rad - rad || d2 > rad * rad + rad) continue;
      const int tx = x + dx, ty = y + dy;
      if (tx == F.P.ex && ty > y) continue;
      if (!F.inRoom(tx, ty, ri) || !F.freeT(tx, ty)) continue;
      F.group([&](int gid) { F.put(tx, ty, seat, gid); return true; });
    }
}
Prop floorSeat(const Fit& F) { return F.furn == art::Furniture::Cushions || F.furn == art::Furniture::Hammocks ? Prop::Cushion : Prop::Bench; }

// the open court: a well or a fountain in its middle, flowerbeds and plants round its edges, benches; an inn's court
// keeps the travellers' carts, hay and a trough; the walls round it have windows onto it
void furnishCourt(Fit& F, int ri, Ctx& cx) {
  const IRect& R = F.g.rooms[(size_t)ri].r;
  const Arch cu = cultureOf(F);
  int ccx = R.cx(), ccy = R.cy();
  if (F.P.cy0 >= 0 && F.P.cy1 >= F.P.cy0) ccy = (F.P.cy0 + F.P.cy1) / 2;
  // the way from the gate stays open (round 3: only over the court's own tiles; a court whose alleys reach the back
  // wall must not lane the throne's place in the hall above)
  for (int y = R.y; y < R.y + R.h; y++) if (F.inRoom(F.P.ex, y, ri)) F.setLane(F.P.ex, y);
  // (M3b fixer) a court with water of its own (the palace courts' ponds, rills and impluvium) has no fountain; the jade
  // and dune courts never the tiered European one
  const bool water = !F.P.pool.empty() && F.m.floor == 0;
  const bool fountain = !water && cu != Arch::Jade && cu != Arch::Dune &&
                        (F.P.wealth >= 2 || cu == Arch::Imperial || cu == Arch::Starspire || F.P.seat >= 0);
  bool placed = water;
  if (water) {
    const int ex = F.P.ex;
    int py0 = F.H, py1 = -1, px0 = F.W, px1 = -1;
    for (int t : F.P.pool) { py0 = std::min(py0, t / F.W); py1 = std::max(py1, t / F.W); px0 = std::min(px0, t % F.W); px1 = std::max(px1, t % F.W); }
    auto one = [&](int x, int y, Prop p) {
      return F.group([&](int gid) { if (!F.inRoom(x, y, ri) || !F.freeT(x, y) || !F.gapOk(x, y, x, y, gid)) return false; F.put(x, y, p, gid); return true; });
    };
    if (cu == Arch::Jade) {
      // bronze censers in pairs up the way between the lotus ponds, stone lions by the hall's steps, pines in pots
      for (int y = py0; y <= py1; y += 3) for (int s : {-2, 2}) one(ex + s, y, Prop::Brazier);
      for (int s : {-3, 3}) one(ex + s, R.y, Prop::Statue);
      for (int s : {-2, 2}) one(ex + s, R.y + R.h - 1, Prop::Statue);
      for (int k = 0; k < 4; k++) wallPiece(F, ri, Prop::PlantPot, false, true);
      wallPiece(F, ri, Prop::Bench);
    } else if (cu == Arch::Dune) {
      // orange trees in great pots down the outer side of each rill, lanterns at the ends, a diwan's cushions and a
      // low table in a corner of the court
      for (int y = py0 + 1; y <= py1; y += 2)
        for (int s : {-1, 1}) { const int x = s < 0 ? px0 - 1 : px1 + 1; one(x, y, Prop::PlantPot); }
      for (int s : {-1, 1}) { const int x = s < 0 ? px0 : px1; one(x, py0 - 1, Prop::Brazier); }
      const art::Furniture keep = F.furn;
      F.furn = art::Furniture::Cushions;
      tableIn(F, ri, 1, Prop::LowTable, 3, false, 60);
      F.furn = keep;
      for (int k = 0; k < 2; k++) wallPiece(F, ri, Prop::PlantPot, false, true);
    } else {
      // the impluvium: statues at its corners, plants and benches round the walls
      for (int c = 0; c < 4; c++) one(c & 1 ? px1 + 1 : px0 - 1, c & 2 ? py1 + 1 : py0 - 1, c < 2 ? Prop::Statue : Prop::PlantPot);
      for (int k = 0; k < 2; k++) wallPiece(F, ri, Prop::Bench);
      for (int k = 0; k < 3; k++) wallPiece(F, ri, Prop::PlantPot, false, true);
    }
    (void)ex;
  }
  if (fountain) {
    // a fountain's basin blocks the tiles either side and its spout the one behind (Map::rebuildSolid): Filler there
    for (int k = 0; k < 9 && !placed; k++) {
      const int x = ccx + (k % 3) - 1 + (F.P.ex == ccx ? 2 : 0), y = ccy + (k / 3) - 1;
      placed = F.group([&](int gid) {
        for (int dx = -1; dx <= 1; dx++) if (!F.inRoom(x + dx, y, ri) || !F.freeT(x + dx, y)) return false;
        if (!F.inRoom(x, y - 1, ri) || !F.freeT(x, y - 1) || !F.gapOk(x - 1, y - 1, x + 1, y, gid)) return false;
        F.put(x - 1, y, Prop::Filler, gid); F.put(x, y, Prop::Fountain, gid); F.put(x + 1, y, Prop::Filler, gid); F.put(x, y - 1, Prop::Filler, gid);
        return true;
      });
    }
  }
  if (!placed) putNear(F, ri, Prop::Well, ccx + (F.P.ex == ccx ? 2 : 0), ccy, 2, true);
  const bool inn = F.P.type == Building::Inn;
  if (water) { if (cx.ownerX < 0) { cx.ownerX = ccx; cx.ownerY = R.y + R.h - 1; } return; }
  if (inn) {   // the travellers' court
    wallPiece(F, ri, Prop::Cart);
    wallPiece(F, ri, Prop::Trough);
    wallPiece(F, ri, Prop::Haystack);
    for (int k = 0; k < 3; k++) wallPiece(F, ri, k % 2 ? Prop::Crate : Prop::Barrel);
  }
  // plants and flowerbeds round the edges (the corners first), benches between
  const Prop bed[3] = {Prop::Flowers1, Prop::Flowers2, Prop::Flowers3};
  int plants = 2 + R.w * R.h / 14;
  for (int k = 0; k < plants; k++) wallPiece(F, ri, k % 3 == 0 ? Prop::PlantPot : (cu == Arch::Dune || cu == Arch::SunTemple ? Prop::PlantPot : bed[(size_t)F.r.irange(3)]), false, true);
  if (!inn) {
    wallPiece(F, ri, Prop::Bench);
    if (R.w * R.h >= 30) { wallPiece(F, ri, Prop::Bench); wallPiece(F, ri, Prop::Bush); }
  }
  if (cx.ownerX < 0) { cx.ownerX = ccx; cx.ownerY = ccy + 1; }
}

// the bathing hall: benches along its walls facing the pool, a statue in a niche, braziers for warmth, towels shelved
void furnishBath(Fit& F, int ri, Ctx& cx) {
  const IRect& R = F.g.rooms[(size_t)ri].r;
  // (M5 fixer r2) the niche holds a statue, or a brazier, or a potted palm, by the house
  const float niche = F.r.f();
  northPiece(F, ri, niche < 0.55f ? Prop::Statue : (niche < 0.8f ? Prop::Brazier : Prop::PlantPot), 2);
  for (int k = 0; k < 2 + R.w / 5; k++) wallPiece(F, ri, Prop::Bench, true, true);
  northPiece(F, ri, Prop::Cupboard, 1);   // the linen
  wallPiece(F, ri, Prop::Brazier);
  wallPiece(F, ri, Prop::PlantPot);
  if (R.w * R.h >= 40) { wallPiece(F, ri, Prop::Brazier); wallPiece(F, ri, Prop::PlantPot); if (F.r.f() < 0.6f) northPiece(F, ri, Prop::Statue, 1); }
  // bathers stand at the pool's edge
  for (int t : F.P.pool) {
    const int x = t % F.W, y = t / F.W;
    if (F.inRoom(x, y + 1, ri) && !F.m.propAt(x, y + 1) && cx.seats.size() < 4 && ((x + y) % 3 == 0)) cx.seats.push_back({x, y + 1});
  }
  if (cx.ownerX < 0) { cx.ownerX = R.cx(); cx.ownerY = R.y + R.h - 1; }
}
void furnishChanging(Fit& F, int ri, Ctx& cx) {
  const IRect& R = F.g.rooms[(size_t)ri].r;
  for (int k = 0; k < 2 + R.w / 6; k++) wallPiece(F, ri, Prop::Bench, true, true);
  northPiece(F, ri, Prop::Cupboard, 1);   // linen and robes
  northPiece(F, ri, F.r.f() < 0.5f ? Prop::Wardrobe : Prop::Cupboard, 0);
  wallPiece(F, ri, Prop::Washstand);
  wallPiece(F, ri, Prop::Chest);
  wallPiece(F, ri, Prop::PlantPot);
  if (cx.ownerX < 0) {   // the bath keeper takes the coin by the door
    int ox = -1, oy = -1;
    if (putNear(F, ri, Prop::TableSmall, F.P.ex + 3, F.H - 4, 3, true, &ox, &oy)) { cx.ownerX = ox; cx.ownerY = oy - 1; }
  }
}
// a tea room: low tables with cushions (or the people's own seats), folding screens between them, plants; the counter
void furnishTeaRoom(Fit& F, int ri, Ctx& cx) {
  barCounter(F, ri, false, cx);
  const IRect& R = F.g.rooms[(size_t)ri].r;
  const art::Furniture keep = F.furn;
  F.furn = art::Furniture::Cushions;   // tea is taken at low tables
  int want = 3 + R.w * R.h / 40;
  for (int k = 0; k < want; k++) {
    int tx = -1, ty = -1;
    if (tableIn(F, ri, 1, Prop::TableSmall, 4, false, 40, &tx, &ty)) { cx.seats.push_back({tx, ty + 1}); if (F.r.f() < 0.6f) rugRect(F, ri, tx - 1, ty - 1, tx + 1, ty + 1, rugFor(F)); }
  }
  F.furn = keep;
  for (int k = 0; k < 2 + R.w / 8; k++) wallPiece(F, ri, Prop::FoldScreen, true);
  wallPiece(F, ri, Prop::PlantPot);
  northPiece(F, ri, Prop::Shelf, 0);
}
void furnishParlour(Fit& F, int ri) {
  const IRect& R = F.g.rooms[(size_t)ri].r;
  const art::Furniture keep = F.furn;
  if (F.P.type == Building::TeaHouse || cultureOf(F) == Arch::Jade || cultureOf(F) == Arch::Dune || cultureOf(F) == Arch::Steppe) F.furn = art::Furniture::Cushions;
  int tx = -1, ty = -1;
  if (tableIn(F, ri, R.w >= 5 ? 2 : 1, Prop::TableMeal, 3, false, 40, &tx, &ty) || tableIn(F, ri, 1, Prop::TableSmall, 2, false, 40, &tx, &ty))
    rugRect(F, ri, tx - 1, ty - 1, tx + (R.w >= 5 ? 2 : 1), ty + 1, rugFor(F));
  F.furn = keep;
  northPiece(F, ri, F.r.f() < 0.5f ? Prop::Cupboard : Prop::Shelf, 1);
  if (F.P.type == Building::TeaHouse) wallPiece(F, ri, Prop::FoldScreen, true);
  wallPiece(F, ri, Prop::PlantPot);
  if (R.w * R.h >= 12) northPiece(F, ri, Prop::Candelabra, 0);
}
// the training floor: straw dummies in a row out in the open, weapon racks along the walls, benches to rest on
void furnishTraining(Fit& F, int ri, Ctx& cx) {
  const IRect& R = F.g.rooms[(size_t)ri].r;
  const int y = R.y + std::max(2, R.h / 2 - 1);
  int placed = 0;
  for (int x = R.x + 2; x < R.x + R.w - 2 && placed < 2 + R.w / 6; x += 3) {
    if (std::abs(x - F.P.ex) <= 1) continue;
    if (putNear(F, ri, Prop::TrainingDummy, x, y, 1, true)) placed++;
  }
  for (int k = 0; k < 2 + R.w / 5; k++) northPiece(F, ri, Prop::WeaponRack, k == 0 ? 1 : 0);
  wallPiece(F, ri, Prop::Bench, false, true);
  wallPiece(F, ri, Prop::Bench, false, true);
  wallPiece(F, ri, Prop::Chest);
  wallPiece(F, ri, Prop::Barrel);
  cx.ownerX = R.cx(); cx.ownerY = y + 2;
  cx.seats.push_back({R.x + 2, y + 1});
  cx.seats.push_back({R.x + R.w - 3, y + 1});
}
// the feasting hall (a mead hall, the jarl's great hall, the elders' stilt hall, a feast tent): the high seat at the far
// wall, the long hearth down the middle with benches by it, long tables with benches either side, roof posts in two
// rows, the household's shields on the walls
void furnishFeast(Fit& F, int ri, Ctx& cx) {
  const IRect& R = F.g.rooms[(size_t)ri].r;
  highSeat(F, ri, cx);
  if (F.P.pitY >= 0) {
    if (F.P.tmpl == Tmpl::FeastTent || F.g.shaped) centreFire(F, ri, F.P.ex, F.P.pitY);
    else longHearth(F, ri, F.P.pitY, F.P.pitX0, F.P.pitX1);
  } else if (!northWide(F, ri, Prop::Hearth, -1, true)) {   // (M3b fixer) no trench: the people's hearth on the wall
    northWide(F, ri, Prop::Hearth, -1, false);
  }
  // roof posts in two rows clear of the hearth and the axis
  if (!F.g.shaped && R.h >= 9)
    for (int k = 0; k < 2; k++) {
      const int py = F.P.pitY + (k ? 3 : -3);
      for (int x = R.x + 3; x < R.x + R.w - 3; x += 5)
        if (std::abs(x - F.P.ex) > 2) F.group([&](int gid) { if (!pillarOk(F, x, py, ri) || !F.gapOk(x, py, x, py, gid)) return false; F.put(x, py, Prop::Pillar, gid); return true; });
    }
  // the long tables either side of the hearth
  int want = 2 + R.w * R.h / 60;
  for (int k = 0; k < want; k++) {
    int tx = -1, ty = -1;
    if (tableIn(F, ri, 3, Prop::TableMeal, 0, true, 60, &tx, &ty) || tableIn(F, ri, 2, Prop::TableMeal, 0, true, 60, &tx, &ty)) cx.seats.push_back({tx, ty + 1});
  }
  for (int k = 0; k < 2; k++) northPiece(F, ri, k ? Prop::WeaponRack : Prop::Banner, 0);
  wallPiece(F, ri, Prop::Barrel2);
  wallPiece(F, ri, Prop::Chest);
  if (R.w * R.h >= 80) { wallPiece(F, ri, Prop::Barrel); wallPiece(F, ri, Prop::Bench); }
}
// a meeting hall: a guild's long table under its banners (the masters' chairs down it), or a council's ring of seats
// round the hearth with the speaker's chair at the far wall
void furnishAssembly(Fit& F, int ri, Ctx& cx) {
  const IRect& R = F.g.rooms[(size_t)ri].r;
  if (F.P.tmpl == Tmpl::CouncilHall) {
    int y = 2;
    while (y < F.H - 2 && !F.inRoom(F.P.ex, y, ri)) y++;
    F.group([&](int gid) { if (!F.wallN(F.P.ex, y) || !F.freeT(F.P.ex, y)) return false; F.put(F.P.ex, y, Prop::Chair, gid); return true; });
    cx.ownerX = F.P.ex; cx.ownerY = y + 1;
    const int cy = std::min(R.y + R.h - 4, y + 5);
    const int fx = centreFire(F, ri, F.P.ex, cy);
    ringSeats(F, ri, fx >= 0 ? fx : F.P.ex, cy, 3, floorSeat(F));
    northPiece(F, ri, Prop::Banner, 1);
    northPiece(F, ri, Prop::Banner, 1);
  } else {
    // the guild's long table on the axis, the master's chair at its head
    int tx = -1, ty = -1, len = 0;
    for (int l = std::clamp(R.w - 8, 3, 6); l >= 2 && tx < 0; l--) {
      const int x0 = F.P.ex - l / 2, y0 = R.y + std::max(3, R.h / 2 - 1);
      if (F.group([&](int gid) { return tableSet(F, ri, x0, y0, l, Prop::TableMeal, l * 2, false, gid); })) { tx = x0; ty = y0; len = l; }
    }
    if (tx < 0) tableIn(F, ri, 3, Prop::TableMeal, 6, false, 60, &tx, &ty);
    if (tx >= 0) { rugRect(F, ri, tx - 1, ty - 1, tx + std::max(1, len), ty + 1, rugFor(F)); cx.ownerX = tx; cx.ownerY = ty - 1; }
    for (int k = 0; k < 3; k++) northPiece(F, ri, k == 1 ? Prop::Bookshelf : Prop::Banner, k == 1 ? 0 : 1);
  }
  wallPiece(F, ri, Prop::Chest);
  wallPiece(F, ri, Prop::Chest);
  northPiece(F, ri, Prop::Candelabra, 0);
  wallPiece(F, ri, Prop::PlantPot);
  if (R.w * R.h >= 70) { wallPiece(F, ri, Prop::Bench); wallPiece(F, ri, Prop::Candelabra); }
}
// the exchange's trading floor: the counters (scales, ledgers, the coin) before the strongroom, the clerks' ledger
// desks about the floor, a bench for the waiting merchants
void furnishTrading(Fit& F, int ri, Ctx& cx) {
  barCounter(F, ri, false, cx);
  const IRect& R = F.g.rooms[(size_t)ri].r;
  for (int k = 0; k < 2 + R.w / 8; k++) {
    std::vector<std::pair<int, int>> c;
    F.tiles(ri, [&](int x, int y) { if (y >= F.P.barY + 5 && y <= F.H - 4 && std::abs(x - F.P.ex) >= 2) c.push_back({x, y}); });
    F.shuffle(c);
    for (auto& s : c)
      if (F.group([&](int gid) {
            if (!F.freeT(s.first, s.second) || !F.freeT(s.first, s.second + 1) || !F.gapOk(s.first, s.second, s.first, s.second + 1, gid)) return false;
            F.put(s.first, s.second, Prop::Desk, gid); F.put(s.first, s.second + 1, Prop::Stool, gid);
            return true;
          }))
        break;
  }
  wallPiece(F, ri, Prop::Bench, false, true);
  wallPiece(F, ri, Prop::Chest);
  wallPiece(F, ri, Prop::PlantPot);
  wallPiece(F, ri, Prop::Candelabra);
}

// a living trunk in an elven hall: three columns of living wood (the Pillar in the room's material) side by side
void livingTrunk(Fit& F, int ri, int x, int y) {
  F.group([&](int gid) {
    for (int dx = -1; dx <= 1; dx++) if (!pillarOk(F, x + dx, y, ri)) return false;
    if (!F.gapOk(x - 1, y, x + 1, y, gid)) return false;
    for (int dx = -1; dx <= 1; dx++) F.put(x + dx, y, Prop::Pillar, gid);
    return true;
  });
  for (int dx : {-2, 2}) putNear(F, ri, Prop::PlantPot, x + dx, y + 1, 1, false);
}
// the seat halls of the societies: the khan's tent (the dais, carpets over the floor round the stove, low tables and
// cushions down the sides), the jarl's and the elders' feasting halls, the high councils' rings, the doge's council
// table, the high priest's sanctum (the gods' altars flanking his seat)
void furnishSeatHall(Fit& F, int ri, Ctx& cx) {
  const IRect& R = F.g.rooms[(size_t)ri].r;
  const int ex = F.P.ex;
  const Tmpl t = F.P.tmpl;
  const bool marshMoot = t == Tmpl::SeatStilt && F.P.pitY < 0;   // (M3b fixer) the marsh elders' hall: no feasting hall kit
  if ((t == Tmpl::SeatGreatHall || t == Tmpl::SeatStilt || t == Tmpl::FeastTent) && !marshMoot) furnishFeast(F, ri, cx);
  else highSeat(F, ri, cx);
  const int sy = std::max(R.y, cx.ownerY - 1);   // the high seat's row
  const int midY = std::min(R.y + R.h - 3, std::max(sy + 4, R.y + R.h / 2));
  if (t == Tmpl::SeatTent) {
    // (fix) a khan's court, not an empty floor: the felt runner from the door to the dais, the two painted roof poles
    // (bagana) either side of the stove under the crown, the court seated on cushions in a ring round the fire, the
    // feast's low tables down both sides, the horse-tail standards behind the dais, chests and racks at the lattice
    centreFire(F, ri, ex, midY);
    for (int s : {-2, 2}) putNear(F, ri, Prop::Pillar, ex + s, midY - 1, 1, true);
    ringSeats(F, ri, ex, midY, 3, Prop::Cushion);
    // (M3c fixer round 3) the runner is laid after the stove and in two lengths (door to the fire's ring, the ring to
    // the dais): laid first, right through the stove's tile, the whole carpet was taken up again (rugCleanup)
    rugRect(F, ri, ex - 1, midY + 3, ex + 1, R.y + R.h - 2, Deco::RugRed);
    rugRect(F, ri, ex - 1, sy + 3, ex + 1, midY - 3, Deco::RugRed);
    for (int k = 0; k < 4; k++) rugFit(F, ri, 3, 2, ex + (k % 2 ? 5 : -5), sy + 3 + (k / 2) * 3, rugFor(F));
    const art::Furniture keep = F.furn;
    F.furn = art::Furniture::Cushions;
    // (M3c fixer round 3, review: "about 15 identical diamond rugs, each with the same pair of red cushions and the
    // same low table, are spread evenly with no focal throne area") a few feast tables, not a grid of them; the dais
    // gets a broad carpet of its own so the khan's seat is the focus at the back of the circle
    rugRect(F, ri, ex - 2, sy + 1, ex + 2, sy + 2, Deco::RugGold);
    for (int k = 0; k < 2 + R.w * R.h / 240; k++) {
      int tx = -1, ty = -1;
      if (tableIn(F, ri, 1, Prop::LowTable, 3, false, 40, &tx, &ty)) rugFit(F, ri, 3, 3, tx, ty, k % 2 ? Deco::RugBlue : rugFor(F));
    }
    F.furn = keep;
    for (int k = 0; k < 2; k++) northPiece(F, ri, Prop::Banner, 1);
    // (M3b fixer) the khan's sleeping place and the treasury screened off with felt at the sides of the circle
    screenedNook(F, ri, R.x + 2, midY + 1, Prop::Chest, Prop::Chest);
    screenedNook(F, ri, R.x + R.w - 4, midY + 1, Prop::Chest, Prop::Barrel);
    wallPiece(F, ri, Prop::Chest);
    wallPiece(F, ri, Prop::Barrel);
    northPiece(F, ri, Prop::WeaponRack, 0);
    wallPiece(F, ri, Prop::WeaponRack);
  } else if (t == Tmpl::SeatSpire || t == Tmpl::SeatTree) {
    if (t == Tmpl::SeatTree) livingTrunk(F, ri, ex, midY);
    else rugFit(F, ri, 3, 3, ex, midY, Deco::RugBlue);
    ringSeats(F, ri, ex, midY, 3, floorSeat(F) == Prop::Cushion ? Prop::Cushion : Prop::Bench);
    // (M3b fixer) a round chamber, not a bare floor: a ring of slender columns round the council, the founders'
    // statues on the back arc, lecterns and candle-stands round the wall
    if (F.g.shaped) {
      const int rad = std::max(5, std::min(R.w, R.h) / 2 - 3);
      static const int dir[12][2] = {{10, 0}, {9, -5}, {5, -9}, {0, -10}, {-5, -9}, {-9, -5}, {-10, 0}, {-9, 5}, {-5, 9}, {5, 9}, {9, 5}, {0, 10}};
      for (int k = 0; k < 11; k++) {
        const int x = ex + dir[k][0] * rad / 10, y = midY + dir[k][1] * rad / 10;
        if (std::abs(x - ex) <= 1 && y > midY) continue;   // the way in
        F.group([&](int gid) { if (!pillarOk(F, x, y, ri) || !F.gapOk(x, y, x, y, gid)) return false; F.put(x, y, Prop::Pillar, gid); return true; });
      }
    }
    northPiece(F, ri, Prop::Statue, 1);
    northPiece(F, ri, Prop::Statue, 1);
    northPiece(F, ri, Prop::Bookshelf, 0);
    northPiece(F, ri, Prop::Candelabra, 0);
    for (int k = 0; k < 2; k++) wallPiece(F, ri, Prop::Candelabra);
    for (int k = 0; k < 2; k++) wallPiece(F, ri, Prop::Lectern);
    wallPiece(F, ri, Prop::PlantPot);
    wallPiece(F, ri, Prop::PlantPot);
  } else if (marshMoot) {
    // the marsh elders' long hall: the elders' moot round a low table on reed mats in the middle (cushions in a ring),
    // the hearth on the wall, the roof posts in two rows, baskets, fish jars and nets' gear along the walls, reed
    // screens; the floor is the boards over the water
    if (!northWide(F, ri, Prop::Hearth, -1, true)) northWide(F, ri, Prop::Hearth, -1, false);
    rugFit(F, ri, 5, 3, ex, midY, Deco::RugGreen);
    putNear(F, ri, Prop::LowTable, ex, midY, 1, true);
    ringSeats(F, ri, ex, midY, 3, Prop::Cushion);
    for (int k = 0; k < 2; k++) {
      const int py = midY + (k ? 4 : -4);
      for (int x = R.x + 3; x < R.x + R.w - 3; x += 5)
        if (std::abs(x - ex) > 4) F.group([&](int gid) { if (!pillarOk(F, x, py, ri) || !F.gapOk(x, py, x, py, gid)) return false; F.put(x, py, Prop::Pillar, gid); return true; });
    }
    const art::Furniture keep = F.furn;
    F.furn = art::Furniture::Cushions;
    for (int k = 0; k < 2; k++) tableIn(F, ri, 1, Prop::LowTable, 3, false, 40);
    F.furn = keep;
    static const Prop gear[8] = {Prop::Baskets, Prop::Barrel, Prop::Sacks, Prop::FoldScreen, Prop::Baskets, Prop::Chest, Prop::PlantPot, Prop::Barrel2};
    for (int k = 0; k < 6 + R.w / 6; k++) wallPiece(F, ri, gear[(size_t)(k % 8)]);
    northPiece(F, ri, Prop::Shelf, 0);
    northPiece(F, ri, Prop::Banner, 1);
  } else if (t == Tmpl::SeatDoge) {
    // the council table down the axis from the doge's chair, the councillors' chairs either side
    for (int l = std::min(5, std::max(2, R.h - (sy - R.y) - 6)); l >= 2; l--) {
      bool done = false;
      const int y0 = sy + 3;
      done = F.group([&](int gid) {
        if (!F.rectFree(ex - 2, y0 - 1, ex + 2, y0 + l, ri)) return false;
        for (int k = 0; k < l; k++) {
          F.put(ex, y0 + k, Prop::TableMeal, gid);
          F.put(ex - 1, y0 + k, Prop::Chair, gid); F.put(ex + 1, y0 + k, Prop::Stool, gid);
        }
        return true;
      });
      if (done) { rugRect(F, ri, ex - 2, y0 - 1, ex + 2, y0 + l, Deco::RugRed); break; }
    }
    for (int k = 0; k < 2; k++) northPiece(F, ri, k ? Prop::Bookshelf : Prop::Banner, 1);
    // (M3b fixer) the republic's counting house, not a bare hall: colonnades either side of the council, the clerks'
    // ledger desks in the side bays, the money-changers' tables, the guilds' banners on the back wall, chests of coin,
    // benches for the waiting merchants
    if (R.w >= 15)
      for (int y = sy + 3; y < R.y + R.h - 2; y += 3)
        for (int s : {-5, 5})
          F.group([&](int gid) { const int x = ex + s; if (!pillarOk(F, x, y, ri) || !F.gapOk(x, y, x, y, gid)) return false; F.put(x, y, Prop::Pillar, gid); return true; });
    for (int k = 0; k < 2 + R.w / 7; k++) {
      std::vector<std::pair<int, int>> c;
      F.tiles(ri, [&](int x, int y) { if (y >= sy + 3 && y <= R.y + R.h - 3 && std::abs(x - ex) >= 7) c.push_back({x, y}); });
      F.shuffle(c);
      for (auto& q : c)
        if (F.group([&](int gid) {
              const bool table = k % 3 == 2;
              if (!F.freeT(q.first, q.second) || !F.freeT(q.first, q.second + 1) || !F.gapOk(q.first, q.second, q.first, q.second + 1, gid)) return false;
              F.put(q.first, q.second, table ? Prop::DisplayTable : Prop::Desk, gid); F.put(q.first, q.second + 1, Prop::Stool, gid);
              return true;
            }))
          break;
    }
    for (int k = 0; k < 3; k++) northPiece(F, ri, Prop::Banner, 0);
    northPiece(F, ri, Prop::Bookshelf, 0);
    wallPiece(F, ri, Prop::Chest);
    wallPiece(F, ri, Prop::Chest);
    wallPiece(F, ri, Prop::Lectern);
    for (int k = 0; k < 2; k++) wallPiece(F, ri, Prop::Bench, false, true);
    wallPiece(F, ri, Prop::Candelabra);
    wallPiece(F, ri, Prop::Candelabra);
    wallPiece(F, ri, Prop::PlantPot);
    wallPiece(F, ri, Prop::PlantPot);
  } else if (t == Tmpl::SeatSanctum && cultureOf(F) == Arch::Dune) {
    // (M3b fixer round 3) the dune high temple is no sun temple: no idols (one god, no images), a hypostyle forest of
    // tiled piers in four rows, hanging lamps on stands down the way, the faithful's prayer rugs in ranks between the
    // piers, the pulpit beside the seat, palms in pots and the alms chests at the walls
    const int pdx = R.w >= 23 ? 6 : 5;
    for (int y = sy + 3; y < R.y + R.h - 2; y += 2)
      for (int s : {-pdx, -2 - pdx / 2, 2 + pdx / 2, pdx})
        F.group([&](int gid) { const int x = ex + s; if (!pillarOk(F, x, y, ri) || !F.gapOk(x, y, x, y, gid)) return false; F.put(x, y, Prop::Pillar, gid); return true; });
    for (int s : {-2, 2}) putNear(F, ri, Prop::Lectern, ex + s * 2, sy + 1, 1, true);
    for (int y = sy + 4; y < R.y + R.h - 3; y += 4)
      for (int s : {-2, 2}) putNear(F, ri, Prop::Candelabra, ex + s, y, 0, true);
    for (int y = sy + 3, row = 0; y < R.y + R.h - 3; y += 3, row++)
      for (int x = R.x + 1; x < R.x + R.w - 1; x++) {
        if (std::abs(x - ex) < 3 || !F.inRoom(x, y, ri) || !F.inRoom(x, y + 1, ri) || F.m.propAt(x, y) || F.m.propAt(x, y + 1)) continue;
        const Deco d = ((x + row) & 1) ? Deco::RugRed : Deco::RugGreen;
        F.deco(x, y, d); F.deco(x, y + 1, d);
      }
    for (int k = 0; k < 4; k++) wallPiece(F, ri, Prop::PlantPot);
    for (int k = 0; k < 2; k++) wallPiece(F, ri, Prop::Chest);
    rugRect(F, ri, ex - 2, sy + 1, ex + 2, sy + 1, Deco::RugBlue);
    rugRect(F, ri, ex, sy + 2, ex, R.y + R.h - 1, Deco::RugBlue);
  } else if (t == Tmpl::SeatSanctum) {
    // the gods' altars either side of the high priest's seat, braziers before them, statues in the corners
    for (int s : {-5, 5})
      F.group([&](int gid) {
        const int x = ex + s;
        for (int dx = -1; dx <= 1; dx++) if (!F.freeT(x + dx, sy) || !F.inRoom(x + dx, sy, ri) || !F.wallN(x + dx, sy)) return false;
        F.put(x - 1, sy, Prop::Filler, gid); F.put(x, sy, Prop::Altar, gid); F.put(x + 1, sy, Prop::Filler, gid);
        return true;
      });
    for (int s : {-3, 3}) putNear(F, ri, Prop::Brazier, ex + s, sy + 2, 1, true);
    northPiece(F, ri, Prop::Statue, 1);
    northPiece(F, ri, Prop::Statue, 1);
    // (M3b fixer) no rows of one-pixel pews across graph paper: the colonnades of the god's house down the processional
    // way, braziers in the side aisles, the worshippers' kneeling mats in pairs by the way, statues and offering urns
    // along the walls, the dais before the seat
    const int pdx = R.w >= 23 ? 5 : 4;
    for (int y = sy + 4; y < R.y + R.h - 2; y += 3)
      for (int s : {-pdx, pdx})
        F.group([&](int gid) { const int x = ex + s; if (!pillarOk(F, x, y, ri) || !F.gapOk(x, y, x, y, gid)) return false; F.put(x, y, Prop::Pillar, gid); return true; });
    for (int y = sy + 5; y < R.y + R.h - 3; y += 4)
      for (int s : {-pdx - 3, pdx + 3})
        F.group([&](int gid) { const int x = ex + s; if (!F.inRoom(x, y, ri) || !F.freeT(x, y) || !F.gapOk(x, y, x, y, gid)) return false; F.put(x, y, Prop::Brazier, gid); return true; });
    for (int y = sy + 6; y < R.y + R.h - 3; y += 3)
      for (int s : {-2, 2})
        F.group([&](int gid) { const int x = ex + s; if (!F.inRoom(x, y, ri) || !F.freeT(x, y)) return false; F.put(x, y, Prop::Cushion, gid); return true; });
    for (int k = 0; k < 2; k++) wallPiece(F, ri, Prop::Statue);
    for (int k = 0; k < 3; k++) wallPiece(F, ri, k == 1 ? Prop::PlantPot : Prop::Urn);
    rugRect(F, ri, ex - 2, sy + 1, ex + 2, sy + 1, Deco::RugGold);
    rugRect(F, ri, ex, sy + 2, ex, R.y + R.h - 1, Deco::RugGold);
  }
  for (int y = sy + 1; y < F.H - 1; y++) if (t != Tmpl::SeatDoge) F.setLane(ex, y);
  // the guard at the high seat
  cx.seats.push_back({ex - 2, sy + 2});
  cx.seats.push_back({ex + 2, sy + 2});
}

// (M3b fixer) a corner screened off without walls: two folding screens side by side in front, the pieces kept there
// (chests, a barrel) right behind them; as near (x, y) (the screens' west tile) as it fits
bool screenedNook(Fit& F, int ri, int x, int y, Prop a, Prop b) {
  for (int rr = 0; rr <= 3; rr++)
    for (int dy = -rr; dy <= rr; dy++)
      for (int dx = -rr; dx <= rr; dx++) {
        if (std::max(std::abs(dx), std::abs(dy)) != rr) continue;
        const int tx = x + dx, ty = y + dy;
        if (F.group([&](int gid) {
              for (int k = 0; k < 2; k++)
                for (int j = -1; j <= 0; j++) if (!F.inRoom(tx + k, ty + j, ri) || !F.freeT(tx + k, ty + j)) return false;
              if (!F.gapOk(tx, ty - 1, tx + 1, ty, gid)) return false;
              F.put(tx, ty - 1, a, gid); F.put(tx + 1, ty - 1, b, gid);
              F.put(tx, ty, Prop::FoldScreen, gid); F.put(tx + 1, ty, Prop::FoldScreen, gid);
              return true;
            }))
          return true;
      }
  return false;
}
// (M3b fixer) the throne hall of a jade or a dune court palace. The jade court: painted screens flanking the dragon
// throne, red columns in two rows, bronze censers, the ministers' kneeling cushions in ranks either side of the way.
// The dune court: an arcade either side, the divans of the court (low tables and cushions) down the side bays on
// carpets, lanterns and the sultan's gifts.
void furnishCourtHall(Fit& F, int ri, Ctx& cx) {
  const IRect& R = F.g.rooms[(size_t)ri].r;
  const int ex = F.P.ex;
  const bool jade = F.P.culture == (int)cult::Archetype::Jade;
  highSeat(F, ri, cx);
  const int sy = std::max(R.y, cx.ownerY - 1);
  for (int y = sy + 1; y < F.H - 1; y++) F.setLane(ex, y);
  auto one = [&](int x, int y, Prop p) {
    return F.group([&](int gid) { if (!F.inRoom(x, y, ri) || !F.freeT(x, y) || !F.gapOk(x, y, x, y, gid)) return false; F.put(x, y, p, gid); return true; });
  };
  rugRect(F, ri, ex, sy + 2, ex, R.y + R.h - 1, jade ? Deco::RugRed : Deco::RugGold);
  const int pdx = jade ? 3 : 4;
  for (int y = sy + 3; y < R.y + R.h - 1; y += jade ? 2 : 3)
    for (int s : {-pdx, pdx})
      F.group([&](int gid) { const int x = ex + s; if (!pillarOk(F, x, y, ri) || !F.gapOk(x, y, x, y, gid)) return false; F.put(x, y, Prop::Pillar, gid); return true; });
  if (jade) {
    for (int s : {-3, 3}) one(ex + s, sy, Prop::FoldScreen);
    for (int s : {-2, 2}) one(ex + s, sy + 2, Prop::Brazier);
    for (int y = sy + 4; y < R.y + R.h - 1; y += 2)
      for (int s : {-1, 1})
        for (int d = pdx + 2; d <= pdx + 3; d++) {
          const int x = ex + s * d;
          if (F.inRoom(x, y, ri) && F.freeT(x, y)) F.group([&](int gid) { F.put(x, y, Prop::Cushion, gid); return true; });
        }
    for (int k = 0; k < 2; k++) wallPiece(F, ri, Prop::Candelabra);
    for (int k = 0; k < 2; k++) wallPiece(F, ri, Prop::PlantPot);
    northPiece(F, ri, Prop::Statue, 1);
  } else {
    for (int s : {-2, 2}) one(ex + s, sy + 2, Prop::Brazier);
    const art::Furniture keep = F.furn;
    F.furn = art::Furniture::Cushions;
    for (int k = 0; k < 4 + R.w * R.h / 80; k++) {
      int tx = -1, ty = -1;
      std::vector<std::pair<int, int>> c;
      F.tiles(ri, [&](int x, int y) { if (y >= sy + 3 && std::abs(x - ex) >= pdx + 2) c.push_back({x, y}); });
      F.shuffle(c);
      for (auto& q : c)
        if (F.group([&](int gid) { return tableSet(F, ri, q.first, q.second, 1, Prop::LowTable, 4, false, gid); })) { tx = q.first; ty = q.second; break; }
      if (tx >= 0) rugRect(F, ri, tx - 1, ty - 1, tx + 1, ty + 1, k % 2 ? Deco::RugRed : Deco::RugBlue);
    }
    F.furn = keep;
    for (int k = 0; k < 3; k++) wallPiece(F, ri, Prop::PlantPot);
    wallPiece(F, ri, Prop::Chest);
    wallPiece(F, ri, Prop::Urn);
    for (int k = 0; k < 2; k++) wallPiece(F, ri, Prop::Brazier);
  }
  cx.seats.push_back({ex - 2, sy + 3});
  cx.seats.push_back({ex + 2, sy + 3});
}

// ---- the nave (M3b fixer round 3: every people's god-house its own, never one stone church with pews everywhere).
// The altar always stands on the axis at the far wall; what flanks it, who sits where and what fills the floor is the
// people's: the heartland's pews and red runner; the highland kirk's ancestors' tombs and a few plain benches at the
// back; the fjordfolk hof's carved posts, wall benches and fire bowls before the idols; the steppe shrine's stove under
// the crown, cushions in a ring and felt; the dune prayer hall's arcades and ranks of prayer rugs; the jade temple's red
// columns, incense and kneeling cushions; the river church's choir stalls, pulpit and blue runner; the marsh spirit
// house's fire and kneeling cushions among the ancestor posts; the sylvan grove's living tree; the imperial rotunda's ring of
// columns round the fire with the gods in their niches; the sun temple's processional way of braziers and gods; the
// star temple's ranks of reading stands under the crystals.
namespace {
bool navePut(Fit& F, int ri, int x, int y, Prop p, bool gap = true) {
  return F.group([&](int gid) {
    if (!F.inRoom(x, y, ri) || !F.freeT(x, y) || (gap && !F.gapOk(x, y, x, y, gid))) return false;
    F.put(x, y, p, gid);
    return true;
  });
}
bool navePillar(Fit& F, int ri, int x, int y) {
  return F.group([&](int gid) { if (!pillarOk(F, x, y, ri) || !F.gapOk(x, y, x, y, gid)) return false; F.put(x, y, Prop::Pillar, gid); return true; });
}
}  // namespace

void furnishNave(Fit& F, int ri, Ctx& cx) {
  using A = cult::Archetype;
  const A cu = cultureOf(F);
  const int ex = F.P.ex, W = F.W, H = F.H;
  const IRect& R = F.g.rooms[(size_t)ri].r;
  const int yEnd = R.y + R.h - 1;               // the nave's last floor row (by the doors)
  const int midY = std::clamp((R.y + yEnd) / 2 + 1, 5, std::max(5, yEnd - 3));
  bool hasVestry = false;
  for (const RoomDef& D : F.g.rooms) hasVestry |= D.kind == RoomKind::Vestry;
  // the shrines with something in the middle of the floor (a stove, a living tree, the rotunda's fire) leave the way
  // round it; the rest keep the aisle from the door to the altar clear
  const bool centred = cu == A::Steppe || cu == A::Sylvan || cu == A::Imperial;
  for (int y = 3; y <= H - 2; y++)
    if (!centred || y < midY - 2 || y > midY + 2) F.setLane(ex, y);
  // the altar between the people's own pair: candles, idols, horse-tail standards, incense, crystals
  Prop flank = Prop::Candelabra;
  switch (cu) {
    case A::Fjordfolk: case A::Imperial: case A::Jade: flank = Prop::Statue; break;
    case A::Highland: case A::SunTemple: case A::Marsh: flank = Prop::Brazier; break;
    case A::Steppe: flank = Prop::Banner; break;
    case A::Sylvan: flank = Prop::PlantPot; break;
    case A::Starspire: flank = Prop::Crystal; break;
    default: break;
  }
  // (M3c fixer, seed 22: an L-shaped highland kirk) where a flanking piece would wall off a pocket of the far wall (a
  // corner behind the vestry), the altar stands with one flank or none: the altar itself always takes the axis
  const bool altared = F.group([&](int gid) {
    F.put(ex - 1, 2, Prop::Filler, gid); F.put(ex, 2, Prop::Altar, gid); F.put(ex + 1, 2, Prop::Filler, gid);
    for (int s : {-2, 2}) if (F.freeT(ex + s, 2) && F.roomOf(ex + s, 2) == ri) F.put(ex + s, 2, flank, gid);
    return true;
  });
  if (!altared && !F.group([&](int gid) {
        F.put(ex - 1, 2, Prop::Filler, gid); F.put(ex, 2, Prop::Altar, gid); F.put(ex + 1, 2, Prop::Filler, gid);
        return true;
      }))
    F.group([&](int gid) { F.put(ex, 2, Prop::Altar, gid); return true; });
  const bool big = W >= 17 || H >= 15;
  auto sideRows = [&](int y0, int step, int dx0, int dx1, Prop p, bool gap) {   // p either side of the aisle, dx0..dx1 out
    for (int y = y0; y <= yEnd - 2; y += step)
      for (int s : {-1, 1})
        for (int d = dx0; d <= dx1; d++) navePut(F, ri, ex + s * d, y, p, gap);
  };
  switch (cu) {
    case A::Highland: {
      // the ancestors' kirk: the clan's forefathers in stone in the front half either side of the way, braziers by them,
      // a few plain benches at the back for the living
      for (int y = 5; y <= midY; y += 3)
        for (int s : {-1, 1}) { navePut(F, ri, ex + s * 3, y, Prop::Statue); navePut(F, ri, ex + s * 3, y + 1, Prop::Brazier); }
      for (int y = midY + 3; y <= yEnd - 2; y += 2)
        F.group([&](int gid) {
          for (int s : {-1, 1})
            for (int d = 2; d <= 4; d++) if (F.freeT(ex + s * d, y) && F.roomOf(ex + s * d, y) == ri) F.put(ex + s * d, y, Prop::Bench, gid);
          return true;
        });
      for (int k = 0; k < 2; k++) wallPiece(F, ri, Prop::Urn);
      rugRect(F, ri, ex - 1, 3, ex + 1, 3, Deco::RugGreen);
      break;
    }
    case A::Fjordfolk: {
      // the hof: two rows of carved posts, fire bowls before the idols, the folk on benches down the walls
      for (int y = 4; y <= yEnd - 2; y += 2) for (int s : {-3, 3}) navePillar(F, ri, ex + s, y);
      for (int s : {-1, 1}) navePut(F, ri, ex + s, 4, Prop::Brazier);
      for (int y = 4; y <= yEnd - 2; y++) {
        int xw = -1, xe = -1;
        for (int x = R.x; x < R.x + R.w; x++) if (F.inRoom(x, y, ri)) { if (xw < 0) xw = x; xe = x; }
        if (xw >= 0 && F.wallT(xw - 1, y)) navePut(F, ri, xw, y, Prop::Bench, false);
        if (xe >= 0 && F.wallT(xe + 1, y)) navePut(F, ri, xe, y, Prop::Bench, false);
      }
      for (int k = 0; k < 2; k++) wallPiece(F, ri, Prop::Barrel);   // the mead of the sacrifice
      break;
    }
    case A::Steppe: {
      // the felt shrine: the stove under the crown in the middle, worshippers' cushions in a ring on the felt, the
      // horse-tail standards by the altar, the offerings in chests at the lattice
      // (fixer M6b r3, review: "the two steppe temples match") three shrines: the stove in the middle with the ring of
      // the faithful round it; the stove drawn up before the altar with a felt runner from the door and the faithful in
      // two arcs; the stove by the door for the herders' offerings, the altar's felt before the standards and the
      // ancestors' cushions along the lattice
      const int form = F.r.irange(3);
      if (form == 1) {
        const int sy2 = std::max(R.y + 2, midY - 2);
        putNear(F, ri, Prop::Stove, ex, sy2, 1, true);
        rugRing(F, ri, ex, sy2, 1);
        rugRect(F, ri, ex, sy2 + 2, ex, yEnd, rugFor(F));
        for (int s2 : {-1, 1}) for (int k = 0; k < 3; k++) putNear(F, ri, Prop::Cushion, ex + s2 * (2 + k), sy2 + 2 + k, 1, true);
        for (int s2 : {-1, 1}) navePut(F, ri, ex + s2 * 2, 3, Prop::Banner);
      } else if (form == 2) {
        const int sy2 = std::min(yEnd - 2, midY + 1);
        const int sx2 = ex + (F.r.f() < 0.5f ? -3 : 3);
        int fx = sx2, fy = sy2;
        putNear(F, ri, Prop::Stove, sx2, sy2, 1, true, &fx, &fy);
        rugRing(F, ri, fx, fy, 1);
        ringSeats(F, ri, fx, fy, 2, Prop::Cushion);
        rugFit(F, ri, 3, 2, ex, R.y + 3, Deco::RugRed);
        for (int k = 0; k < 4; k++) wallPiece(F, ri, Prop::Cushion);
        for (int s2 : {-1, 1}) navePut(F, ri, ex + s2 * 3, R.y + 2, Prop::Banner);
        wallPiece(F, ri, Prop::LowTable);
      } else {
        putNear(F, ri, Prop::Stove, ex, midY, 1, true);
        rugRing(F, ri, ex, midY, 1);
        ringSeats(F, ri, ex, midY, 3, Prop::Cushion);
      }
      for (int k = 0; k < 3; k++) wallPiece(F, ri, k == 1 ? Prop::Urn : Prop::Chest);
      break;
    }
    case A::Dune: {
      // the prayer hall: arcades of tiled piers, the faithful's prayer rugs in ranks facing the altar (each its own rug:
      // neighbours alternate colour), lamps, the pulpit beside the altar
      for (int y = 4; y <= yEnd - 2; y += 3) for (int s : {-4, 4}) navePillar(F, ri, ex + s, y);
      navePut(F, ri, ex - 3, 3, Prop::Lectern);
      // (fixer M6b r3, review: "30+ identical prayer rugs in a strict grid") the ranks fill the front of the hall only
      // (the faithful stand nearest the altar), not one rug by a pier, each rug its own colour (a fixed hash of its
      // place, no draw), and a gap here and there where a rug is rolled away
      for (int y = 5, row = 0; y <= yEnd - 3 && row < 2; y += 3, row++)
        for (int x = R.x + 1; x < R.x + R.w - 1; x++) {
          if (std::abs(x - ex) < 1 || !F.inRoom(x, y, ri) || !F.inRoom(x, y + 1, ri) || F.m.propAt(x, y) || F.m.propAt(x, y + 1)) continue;
          bool pier = false;
          for (int dy = -1; dy <= 2; dy++)
            for (int dx = -1; dx <= 1; dx++) {
              const int q = F.m.propAt(x + dx, y + dy);
              if (q && tallProp((Prop)(q - 1))) pier = true;
            }
          if (pier) continue;
          const uint32_t hh = ((uint32_t)(x + 31) * 2654435761u) ^ ((uint32_t)(row + 7) * 40503u) ^ (uint32_t)(F.P.ex * 977);
          if ((hh >> 7) % 6 == 0) continue;
          static const Deco cols[4] = {Deco::RugRed, Deco::RugBlue, Deco::RugGreen, Deco::RugGold};
          Deco d = cols[(hh >> 11) % 4];
          if (F.m.decoAt(x - 1, y) == (int)d) d = cols[((hh >> 11) + 1) % 4];   // (each its own rug: neighbours differ)
          F.deco(x, y, d); F.deco(x, y + 1, d);
        }
      for (int k = 0; k < 3; k++) wallPiece(F, ri, Prop::PlantPot);
      wallPiece(F, ri, Prop::Urn);
      break;
    }
    case A::Jade: {
      // red columns in two rows, bronze censers before the altar, kneeling cushions in ranks between the columns and
      // the way, the painted screens at the walls
      for (int y = 4; y <= yEnd - 2; y += 2) for (int s : {-3, 3}) navePillar(F, ri, ex + s, y);
      for (int s : {-1, 1}) navePut(F, ri, ex + s, 4, Prop::Brazier);
      sideRows(6, 2, 1, 2, Prop::Cushion, false);
      for (int k = 0; k < 2; k++) wallPiece(F, ri, Prop::FoldScreen);
      wallPiece(F, ri, Prop::LowTable);
      rugRect(F, ri, ex - 1, 3, ex + 1, 3, Deco::RugRed);
      break;
    }
    case A::River: {
      // the river church: choir stalls along both sides of the front half facing each other, the pulpit by the altar,
      // candle stands down the back, the blue runner of the water god
      for (int y = 4; y <= midY; y++)
        for (int s : {-1, 1}) navePut(F, ri, ex + s * 3, y, Prop::Bench, false);
      navePut(F, ri, ex + 2, 3, Prop::Lectern);
      for (int y = midY + 2; y <= yEnd - 2; y += 3) for (int s : {-3, 3}) navePut(F, ri, ex + s, y, Prop::Candelabra);
      for (int k = 0; k < 2; k++) wallPiece(F, ri, Prop::Bench);
      rugRect(F, ri, ex, 3, ex, yEnd, Deco::RugBlue);
      break;
    }
    case A::Marsh: {
      // the spirit house: the fire on its hearth stones, the carved ancestor posts, urns of the dead, kneeling cushions
      const int fx = centreFire(F, ri, ex - 2, midY);
      (void)fx;
      for (int y = 4; y <= yEnd - 2; y += 3) for (int s : {-4, 4}) navePillar(F, ri, ex + s, y);
      for (int k = 0; k < 3; k++) wallPiece(F, ri, k == 1 ? Prop::PlantPot : Prop::Urn);
      sideRows(midY + 2, 2, 2, 3, Prop::Cushion, true);
      break;
    }
    case A::Sylvan: {
      // the grove: a living tree grown up through the floor, its worshippers on cushions round it, plants everywhere
      livingTrunk(F, ri, ex, midY);
      ringSeats(F, ri, ex, midY, 3, Prop::Cushion);
      for (int k = 0; k < 4; k++) wallPiece(F, ri, Prop::PlantPot);
      rugRect(F, ri, ex - 1, 3, ex + 1, 3, Deco::RugGreen);
      break;
    }
    case A::Imperial: {
      // the rotunda: the eternal flame in the middle in a ring of columns, the gods in their niches round the wall
      navePut(F, ri, ex, midY, Prop::Brazier, false);
      for (int k = 0; k < 8; k++) {
        static const int dx[8] = {-3, 3, -3, 3, -2, 2, 0, 0}, dy[8] = {-2, -2, 2, 2, 0, 0, -3, 3};
        if (dx[k] == 0 && dy[k] > 0) continue;   // the way in stays open
        navePillar(F, ri, ex + dx[k], midY + dy[k]);
      }
      for (int k = 0; k < 4; k++) wallPiece(F, ri, Prop::Statue);
      for (int k = 0; k < 2; k++) wallPiece(F, ri, Prop::Brazier);
      rugRect(F, ri, ex - 1, midY + 3, ex + 1, midY + 3, Deco::RugGold);
      break;
    }
    case A::SunTemple: {
      // the processional way: fire braziers down both sides of it, squat columns beyond, the gods along the walls and
      // the offerings (urns, low offering tables) before the altar
      for (int y = 5; y <= yEnd - 2; y += 3) for (int s : {-2, 2}) navePut(F, ri, ex + s, y, Prop::Brazier);
      for (int y = 4; y <= yEnd - 2; y += 3) for (int s : {-4, 4}) navePillar(F, ri, ex + s, y);
      for (int s : {-1, 1}) navePut(F, ri, ex + s * 3, 3, Prop::Urn);
      for (int k = 0; k < 2; k++) wallPiece(F, ri, Prop::Statue);
      wallPiece(F, ri, Prop::LowTable);
      break;
    }
    case A::Starspire: {
      // the star temple: the astronomer-priests' reading stands in ranks facing the crystals, tall candles between,
      // the blue runner of the night sky
      sideRows(5, 3, 2, 4, Prop::Lectern, true);
      for (int y = 6; y <= yEnd - 2; y += 6) for (int s : {-5, 5}) navePut(F, ri, ex + s, y, Prop::Candelabra);
      for (int k = 0; k < 2; k++) wallPiece(F, ri, Prop::Candelabra);
      rugRect(F, ri, ex, 3, ex, yEnd, Deco::RugBlue);
      break;
    }
    default: {
      // the heartland church: pews in rows facing the altar either side of the aisle, the red runner, columns in a big one
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
          for (int x : {2, W - 3}) navePillar(F, ri, x, y);
      for (int x = 1; x <= W - 2; x++)
        if (F.roomOf(x, 2) == ri && F.wallT(x - 1, 2) != F.wallT(x + 1, 2) && std::abs(x - ex) > 3)
          F.group([&](int gid) { if (!F.freeT(x, 2) || !F.wallN(x, 2)) return false; F.put(x, 2, F.r.f() < 0.6f ? Prop::Statue : Prop::Brazier, gid); return true; });
      rugRect(F, ri, ex, 3, ex, H - 2, Deco::RugRed);
      break;
    }
  }
  // a hall with no vestry keeps the priest's things behind screens in a corner by the doors
  if (!hasVestry && F.P.tmpl != Tmpl::SeatSanctum)
    if (!screenedNook(F, ri, R.x + 2, yEnd - 1, Prop::Chest, Prop::Cupboard)) screenedNook(F, ri, R.x + R.w - 4, yEnd - 1, Prop::Chest, Prop::Cupboard);
  cx.ownerX = ex + 1; cx.ownerY = 3;
  // (M3b) a theocracy's lord-priest rules from his seat beside the altar
  if (F.P.tmpl == Tmpl::SeatSanctum)
    for (int s : {4, -4, 5, -5}) {
      const int x = ex + s;
      if (F.group([&](int gid) {
            for (int dx = -1; dx <= 1; dx++) if (!F.freeT(x + dx, 2) || F.roomOf(x + dx, 2) != ri || !F.wallN(x + dx, 2)) return false;
            F.put(x - 1, 2, Prop::Filler, gid); F.put(x, 2, Prop::Throne, gid); F.put(x + 1, 2, Prop::Filler, gid);
            return true;
          })) { cx.ownerX = x; cx.ownerY = 3; break; }
    }
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
    case RoomKind::Nave:
      // (M3b fixer round 3) the people's own hangings: shields and antlers in the hof, the clans' shields in the kirk,
      // painted scrolls and star charts, the reed people's herbs, the grove's wreaths
      switch (cultureOf(F)) {
        case Arch::Fjordfolk: pool = {{P(Prop::WallShield), 3}, {P(Prop::Antlers), 2}, {P(Prop::Tapestry), 1}}; break;
        case Arch::Highland: pool = {{P(Prop::WallShield), 3}, {P(Prop::Tapestry), 2}}; break;
        case Arch::Jade: case Arch::River: case Arch::Starspire: case Arch::Imperial: pool = {{P(Prop::Painting), 3}, {P(Prop::Tapestry), 2}}; break;
        case Arch::Marsh: pool = {{P(Prop::HerbBundle), 3}, {P(Prop::Antlers), 1}, {P(Prop::WallShield), 1}}; break;
        case Arch::Sylvan: pool = {{P(Prop::Wreath), 3}, {P(Prop::HerbBundle), 2}}; break;
        default: pool = {{P(Prop::Tapestry), 5}, {P(Prop::Wreath), 1}}; break;
      }
      break;
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
  // (M3b fixer) a felt tent's lattice wall: no framed paintings, no candles in wall sconces, no glazed windows (the
  // light comes through the crown): felt hangings, shields, bundles of herbs
  const bool felt = feltRoom(F);
  if (felt) {
    for (Weighted& w : pool) if (w.what == (int)Prop::Painting || w.what == (int)Prop::Wreath) w.what = (int)Prop::Tapestry;
    pool.push_back({(int)Prop::WallShield, 2});
    pool.push_back({(int)Prop::HerbBundle, 1});
  }
  auto put = [&](int t, Prop p) { F.m.prop[(size_t)t] = (uint8_t)((int)p + 1); };
  auto isFree = [&](int t) { return F.m.prop[(size_t)t] == 0; };
  bool outerWindows = kind != RoomKind::Forge && kind != RoomKind::Nave && kind != RoomKind::ThroneHall && kind != RoomKind::Storeroom && kind != RoomKind::Barn && !felt;
  if (kind == RoomKind::Nave && F.m.propAt(F.P.ex, 1) == 0 && F.roomOf(F.P.ex, 2) == ri) put(F.I(F.P.ex, 1), Prop::HolySymbol);   // the sun disc above the altar
  // windows on the outer back wall: one or two, never side by side
  if (outerWindows) {
    int want = faces.size() >= 9 ? 2 : 1;
    std::vector<int> c;
    for (int t : faces) if ((t / F.W == 1 || F.m.deco[(size_t)t] == (uint8_t)Deco::Shell) && isFree(t)) c.push_back(t);   // (M3b: a shaped floor's shell too)
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
    if (felt) continue;
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
      // (fixer M4 r3) never right behind a tall piece (a statue, a pillar, a candle-stand): the piece in front is drawn
      // over him and he reads as one merged sprite (seed 5's high-temple door guard stood behind a plinth statue)
      const int prS = F.m.propAt(tx, ty + 1);
      const bool hidden = prS && tallProp((Prop)(prS - 1));
      // (fixer M6b r3) nor right in front of a brazier, a candle-stand or a statue: its flame (or head) rose out of his
      // helmet (seed 42's desert keep: the two guards flanking the dais wore the braziers behind them as burning hats)
      const int prN = ty > 0 ? F.m.propAt(tx, ty - 1) : 0;
      const bool crowned = prN && ((Prop)(prN - 1) == Prop::Brazier || (Prop)(prN - 1) == Prop::Candelabra || (Prop)(prN - 1) == Prop::Statue);
      const bool onFire = pr && ((Prop)(pr - 1) == Prop::Brazier || (Prop)(pr - 1) == Prop::Candelabra);
      int d = (tx - x) * (tx - x) + (ty - y) * (ty - y) + (pr ? 3 : 0) + (ri >= 0 && F.roomOf(tx, ty) != ri ? 40 : 0) + (hidden ? 200 : 0) +
              (crowned ? 200 : 0) + (onFire ? 400 : 0);
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

art::RoomStyle wallStyleOf(const Bldg& b, const Plan& P, Rng& r) {
  art::ArchStyle a = bldgArch(b);
  using A = cult::Archetype;
  const A cu = P.culture >= 0 ? (A)P.culture : A::COUNT;
  // M3b: the seats of power in their society's idiom
  if (P.seat >= 0) {
    switch ((cult::Seat)P.seat) {
      case cult::Seat::TentCourt: return art::RoomStyle::Felt;
      case cult::Seat::TreePalace: return art::RoomStyle::Living;
      case cult::Seat::CouncilSpire: return art::RoomStyle::Marble;
      case cult::Seat::GreatHall: return a.wall == art::WallMat::Log ? art::RoomStyle::Log : art::RoomStyle::Timber;
      case cult::Seat::StiltHall: return art::RoomStyle::Log;
      case cult::Seat::CourtPalace: return cu == A::Jade ? art::RoomStyle::Paper : (cu == A::Dune ? art::RoomStyle::Tile : art::RoomStyle::Marble);
      // (M3b fixer round 3) the sun folk's plastered sanctum, the dune high temple's glazed tile, never one shared hall
      // (a seat built as a temple is that people's temple below)
      case cult::Seat::TempleComplex:
        if (b.type == Building::Temple) break;
        return cu == A::SunTemple ? art::RoomStyle::Adobe : (cu == A::Dune ? art::RoomStyle::Tile : art::RoomStyle::Hall);
      case cult::Seat::GuildExchange: return art::RoomStyle::Plaster;
      default: return art::RoomStyle::Hall;
    }
  }
  // the new purposes
  switch (b.type) {
    case Building::Bathhouse: return cu == A::Imperial || cu == A::Starspire ? art::RoomStyle::Marble : (cu == A::Jade || cu == A::Fjordfolk ? art::RoomStyle::Timber : art::RoomStyle::Tile);
    case Building::TeaHouse: return cu == A::Jade || cu == A::COUNT ? art::RoomStyle::Paper : (a.wall == art::WallMat::Felt ? art::RoomStyle::Felt : art::RoomStyle::Plaster);
    case Building::Exchange: return cu == A::Dune ? art::RoomStyle::Tile : (cu == A::Starspire ? art::RoomStyle::Marble : art::RoomStyle::Plaster);
    case Building::Guildhall: return a.wall == art::WallMat::Stone || a.wall == art::WallMat::Ashlar ? art::RoomStyle::Hall : art::RoomStyle::Timber;
    case Building::MeadHall: case Building::Lodge:
      if (a.wall == art::WallMat::Felt) return art::RoomStyle::Felt;
      if (a.wall == art::WallMat::Living) return art::RoomStyle::Living;
      return a.wall == art::WallMat::Log ? art::RoomStyle::Log : (a.wall == art::WallMat::Stone || a.wall == art::WallMat::Ashlar ? art::RoomStyle::Stone : art::RoomStyle::Timber);
    case Building::CouncilHall: return cu == A::Starspire ? art::RoomStyle::Marble : (cu == A::Sylvan ? art::RoomStyle::Living : (cu == A::Marsh ? art::RoomStyle::Log : art::RoomStyle::Hall));
    default: break;
  }
  switch (b.type) {
    case Building::Temple: case Building::Keep: case Building::Palace:
      // (M3b fixer round 3) a god-house in its people's material: the stave church's timber, the hof-builders' and the
      // marsh folk's logs, the pagoda's paper screens, the river church's whitewash over brick, the kirk's rough stone,
      // the pyramid's plaster, the star temple's arcane vault, the rotunda's marble
      if (b.type == Building::Temple)
        switch (cu) {
          case A::Fjordfolk: return art::RoomStyle::Timber;
          case A::Marsh: return art::RoomStyle::Log;
          case A::Jade: return art::RoomStyle::Paper;
          case A::River: return art::RoomStyle::Plaster;
          case A::Highland: return art::RoomStyle::Stone;
          case A::SunTemple: return art::RoomStyle::Adobe;
          case A::Starspire: return art::RoomStyle::Arcane;
          case A::Imperial: return art::RoomStyle::Marble;
          default: break;
        }
      if (cu == A::Starspire) return art::RoomStyle::Marble;
      if (cu == A::Sylvan) return art::RoomStyle::Living;
      if (a.wall == art::WallMat::Felt) return art::RoomStyle::Felt;
      if (cu == A::Dune && b.type == Building::Temple) return art::RoomStyle::Tile;
      return art::RoomStyle::Hall;
    case Building::Barracks: return art::RoomStyle::Stone;
    case Building::Tower: return cu == A::Starspire ? art::RoomStyle::Marble : (cu == A::Sylvan ? art::RoomStyle::Living : art::RoomStyle::Arcane);
    default: break;
  }
  // (M3b) the homes, inns and shops of the new walls: a yurt's felt lattice, living wood, jade's paper screens
  if (b.type != Building::Smithy && b.type != Building::Smelter) {
    // a round body under a felt roof is a yurt, whatever its walls are dressed in outside: felt over the lattice inside
    if (P.fp == bld::Floorplan::Round && (a.roofMat == art::RoofMat::Felt || cu == A::Steppe)) return art::RoomStyle::Felt;
    if (a.wall == art::WallMat::Felt) return art::RoomStyle::Felt;
    if (a.wall == art::WallMat::Living) return art::RoomStyle::Living;
    if (cu == A::Jade && (a.wall == art::WallMat::Timber || a.wall == art::WallMat::Plank || a.wall == art::WallMat::Plaster)) return art::RoomStyle::Paper;
    if (cu == A::Starspire && (a.wall == art::WallMat::Ashlar || a.wall == art::WallMat::Stone)) return art::RoomStyle::Marble;
  }
  if (b.type == Building::Smithy || b.type == Building::Smelter) {   // fix round 2: the forge hall in the culture's own material, sooted where it is stone
    // (fixer M6b r3, review: "the steppe adobe smithy has a grey cut-stone shell inside, the desert one log walls and a
    // plank floor") the sand and steppe peoples' forges are mud-brick inside whatever their street face is dressed in
    if (cu == A::Dune || cu == A::Steppe || cu == A::SunTemple) return art::RoomStyle::Adobe;
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
    // M3 culture walls: dressed and rough stone are stone halls; reed and wattle are rustic log rooms on rushes;
    // planks and living wood are timber; a yurt's felt is a light plastered look (its rugs do the rest)
    case art::WallMat::Ashlar: case art::WallMat::Rubble: return art::RoomStyle::Stone;
    case art::WallMat::Wattle: return art::RoomStyle::Log;
    case art::WallMat::Plank: case art::WallMat::Living: return art::RoomStyle::Timber;
    case art::WallMat::Felt: return art::RoomStyle::Plaster;
    default: break;
  }
  if (b.type == Building::StoneHouse) return art::RoomStyle::Stone;
  if (b.type == Building::Hut || b.type == Building::Farmhouse) return r.f() < 0.6f ? art::RoomStyle::Log : art::RoomStyle::Timber;
  if (b.type == Building::Inn || b.type == Building::Shop) return r.f() < 0.5f ? art::RoomStyle::Plaster : art::RoomStyle::Timber;
  return r.f() < 0.3f ? art::RoomStyle::Plaster : art::RoomStyle::Timber;
}

}  // namespace

const char* interiorTemplate(const Bldg& b, const bld::Blueprint& bp, uint32_t seed) { return tmplName(makePlan(b, bp, seed).tmpl); }

void interiorWhy(int* lines, int n, bool reset) {
  for (int i = 0; i < n && i < 8192; i++) lines[i] = gNopeLine[i];
  if (reset) for (int& v : gNopeLine) v = 0;
}

namespace {

// (M4 VIEW lane, the web's seat-of-power entry hitch) genInteriorRooms as a resumable job: the same statements in the
// same order (so the same Rng draws and the same Map, bit for bit), cut into units (the blueprint, the plan, the
// shell, each room's furnishing, each room's wall decor, the rugs, the people, each room's clutter, the finish) that a
// caller may spread over frames (InteriorJob::step with a budget). genInteriorRooms runs every unit in one go.
struct InteriorBuild {
  enum Phase { PH_BP, PH_PLAN, PH_SHELL, PH_SETUP, PH_FURNISH, PH_WALLS, PH_RUGS, PH_PEOPLE, PH_CLUTTER, PH_FINISH, PH_DONE };
  const Bldg& b;
  bld::Blueprint bp;
  uint32_t seed;
  int floor;
  Map& m;
  Plan P;
  Rng r;
  int W = 0, H = 0;
  Geo* ggp = nullptr;
  std::unique_ptr<Fit> Fp;
  Ctx cx;
  std::vector<int> order;
  bool childDone = false;
  int phase = PH_PLAN;
  size_t idx = 0;
  InteriorBuild(Map& m_, const Bldg& b_, uint32_t seed_, int floor_, const bld::Blueprint* bp_)
      : b(b_), seed(seed_), floor(floor_), m(m_) {
    if (bp_) bp = *bp_;
    else phase = PH_BP;
  }
  void shell() {
  floor = std::clamp(floor, 0, P.floors - 1);
  // (M3b fixer round 3) a nave's floor in its people's idiom: the pagoda's rush mats, the river church's red tiles,
  // the star temple's white marble, the rotunda's mosaic (the rest by the room style: the hof's boards, the kirk's and
  // the pyramid's flags, the dune hall's mosaic, felt, roots)
  if (b.type == Building::Temple && !P.geo.empty() && !P.geo[0].rooms.empty() && P.geo[0].rooms[0].kind == RoomKind::Nave) {
    art::FloorStyle fs = art::FloorStyle::COUNT;
    switch ((cult::Archetype)(P.culture >= 0 ? P.culture : (int)cult::Archetype::COUNT)) {
      case cult::Archetype::Jade: fs = art::FloorStyle::Tatami; break;
      case cult::Archetype::River: fs = art::FloorStyle::Terracotta; break;
      case cult::Archetype::Starspire: fs = art::FloorStyle::Marble; break;
      case cult::Archetype::Imperial: fs = art::FloorStyle::Mosaic; break;
      case cult::Archetype::SunTemple: fs = art::FloorStyle::Flagstone; break;
      default: break;
    }
    if (fs != art::FloorStyle::COUNT) P.geo[0].rooms[0].floorStyle = (uint8_t)((int)fs + 1);
  }
  const Geo& g = P.geo[(size_t)floor];
  r = Rng((seed ^ 0x6C8E9CF5u) + (uint32_t)floor * 0x9E3779B9u);
  W = P.W; H = P.H;
  m.kind = MapKind::Interior;
  m.seed = seed;
  m.alloc(W, H, Ground::InteriorWall);
  m.floor = floor;
  // floors: stone in stone buildings, the desert's tiles, flagged kitchens; planks elsewhere
  bool stone = b.type == Building::Keep || b.type == Building::Temple || b.type == Building::Palace || b.type == Building::Barracks || b.type == Building::StoneHouse || b.type == Building::Tower ||
               b.type == Building::Smithy || b.type == Building::Smelter || b.type == Building::Windmill || b.type == Building::Watermill ||
               b.type == Building::Bathhouse || b.type == Building::Exchange || b.type == Building::Guildhall || b.type == Building::CouncilHall ||
               b.biome == Biome::Desert ||
               (b.styled && (b.arch.wall == art::WallMat::Ashlar || b.arch.wall == art::WallMat::Adobe));   // (M3: flags, fired tiles)
  // (M3b fixer) a marsh people's halls stand on stilts over the water: boards (strewn with rushes), never cobbles
  if (P.culture == (int)cult::Archetype::Marsh && b.type != Building::Smithy && b.type != Building::Smelter && b.type != Building::Bathhouse) stone = false;
  // (M3b fixer) the sun folk build in stone: flags underfoot in their inns and halls too, never timber boards
  if (P.culture == (int)cult::Archetype::SunTemple && b.type != Building::Hut) stone = true;
  // (M3b round 3) a timber great hall (the jarl's, the chief's) is floored with boards strewn with rushes, never cobbles
  if (P.seat == (int)cult::Seat::GreatHall) stone = false;
  // (M3b fixer round 3) the fjordfolk's stave church is boarded like their halls
  if (b.type == Building::Temple && P.culture == (int)cult::Archetype::Fjordfolk) stone = false;
  Ground base = stone && !(floor > 0 && (b.type == Building::StoneHouse)) ? Ground::StoneFloor : Ground::WoodFloor;
  for (int y = 0; y < H; y++)
    for (int x = 0; x < W; x++) {
      const uint8_t t = g.wall[(size_t)g.I(x, y)];
      if (t == T_VOID) { m.setG(x, y, Ground::Void); continue; }
      if (t != T_FLOOR) continue;
      int ri = g.roomOf(x, y);
      RoomKind k = ri >= 0 ? g.rooms[(size_t)ri].kind : RoomKind::Hall;
      Ground gr = k == RoomKind::Kitchen ? Ground::StoneFloor : base;
      if (k == RoomKind::Court) gr = Ground::Plaza;   // (M3b) the open court: paved, under the sky
      m.setG(x, y, gr);
    }
  // (M3b) the bath pools: water sunk in the floor (solid: you bathe by standing at the edge)
  for (int t : P.pool) if (floor == 0) m.ground[(size_t)t] = (uint8_t)Ground::Water;
  m.exits.clear();
  m.exitOpen = false;
  if (floor == 0) {
    m.exitX = P.ex; m.exitY = H - 1;
    m.setG(P.ex, H - 1, m.at(P.ex, H - 2));
    // (owner 2026-10-06) a building walked into between pillars (an open front: colonnade, arcade, veranda, iwan; or
    // open galleries beside a door): its front wall opens in a bay inside for each walk-in bay outside, at the same
    // offset from the way in (scaled when the inside is narrower), wherever the floor runs to the wall there. Entering
    // by the k-th way in outside puts you at the k-th inside (Game::enterBuilding), and back out the same way.
    const bld::OpenFront of = bld::openFront(bp);
    const int wt = std::max(1, (int)b.r.w), dcol = wt / 2;
    if (of.gaps && !of.raised) {
      m.exitOpen = of.open();
      for (int c = 0; c < std::min(32, wt); c++) {
        if (c != dcol && !((of.gaps >> c) & 1u)) continue;
        const int off = c - dcol;
        const int x0 = W - 2 >= wt ? P.ex + off : P.ex + (off * (W - 2) + (off < 0 ? -wt / 2 : wt / 2)) / wt;
        auto okX = [&](int x) {
          if (x == P.ex) return true;
          return x >= 1 && x <= W - 2 && g.isFloor(x, H - 2) && g.wall[(size_t)g.I(x, H - 1)] != T_VOID && !g.isFloor(x, H - 1) &&
                 (m.exits.empty() || m.exits.back() < x);
        };
        // where the floor does not reach the wall there (a partition, a round wall's curve), the next column toward the
        // way in, then the next away from it
        int x = -1;
        for (int d : {0, off < 0 ? 1 : -1, off < 0 ? -1 : 1})
          if (x < 0 && (d == 0 || x0 + d != P.ex) && okX(x0 + d)) x = x0 + d;
        if (x < 0 || (!m.exits.empty() && m.exits.back() >= x)) continue;
        m.exits.push_back((int16_t)x);
        if (x != P.ex) m.setG(x, H - 1, m.at(x, H - 2));
      }
      if (m.exits.size() < 2 && !m.exitOpen) m.exits.clear();   // a door alone
    }
  } else {
    m.exitX = -1; m.exitY = -1;
  }
  // the room's look: the wall style on the back wall's face row (M3b: and on row 0, where shaped floors keep it; their
  // outer wall tiles are marked as the shell)
  Rng sr(seed ^ 0x1D2C3B4Au);
  art::RoomStyle rs = wallStyleOf(b, P, sr);
  for (int x = 0; x < W; x++) m.deco[(size_t)x] = (uint8_t)((int)Deco::WallTimber + (int)rs);
  if (g.shaped) {
    for (int y = 1; y < H; y++)
      for (int x = 0; x < W; x++)
        if (g.wall[(size_t)g.I(x, y)] == T_SHELL && !(floor == 0 && m.isExit(x, y))) m.deco[(size_t)(y * W + x)] = (uint8_t)Deco::Shell;
    m.deco[0] = (uint8_t)Deco::Shell;   // (the map's mark: a shaped floor, drawn by its shell)
    // (M3b round 3) and a round one: its outline drawn as the true ellipse of Plan::inShape (decoLayers)
    if (P.fp == bld::Floorplan::Round) m.deco[(size_t)(W - 1)] = (uint8_t)Deco::Shell;
  } else {
    for (int x = 1; x < W - 1; x++) m.deco[(size_t)(1 * W + x)] = (uint8_t)((int)Deco::WallTimber + (int)rs);
  }
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

  }
  void setup() {
  // furnish
  Plan& PP = P;
  ggp = &PP.geo[(size_t)floor];
  Geo& gg = *ggp;
  Fp.reset(new Fit(m, r, PP, gg));
  Fit& F = *Fp;
  F.biome = b.biome;
  {
    const art::ArchStyle A = bldgArch(b);
    F.furn = A.furniture;
    F.accent = b.styled ? A.accentTint : 0;
    m.kit = b.styled ? A.culture : 0;   // (M3 fixer) the people's own hearth, shelves, beds and chests (the view's)
    F.rugBias = b.styled && (A.wall == art::WallMat::Felt || A.wall == art::WallMat::Living) ? 0.3f : 0.0f;
  }
  if (floor == 0) { F.sx = m.exitX; F.sy = m.exitY; } else { F.sx = m.down.ax; F.sy = m.down.ay; }
  for (int t : gg.doorTiles) F.lane[(size_t)t] = 1;
  for (int t : gg.approach) F.lane[(size_t)t] = 1;
  for (int t : gg.keep) F.lane[(size_t)t] = 1;
  if (m.up.valid()) {   // and the tile in front of the arrival, so the way onto the stairs stays open
    int fx = m.up.ax + (m.up.ax - m.up.x), fy = m.up.ay + (m.up.ay - m.up.y);
    if (gg.isFloor(fx, fy) && gg.roomOf(fx, fy) == gg.roomOf(m.up.x, m.up.y)) F.lane[(size_t)gg.I(fx, fy)] = 1;
  }
  if (floor == 0) { F.setLane(P.ex, H - 1); F.setLane(P.ex, H - 2); F.setLane(P.ex, H - 3); }
  for (int16_t x : m.exits) { F.setLane(x, H - 1); F.setLane(x, H - 2); }   // (owner) an open front's bays stay clear
  // one-tile passages (a stair nook, the gap beside a partition) stay clear
  for (int y = 2; y < H - 1; y++)
    for (int x = 1; x < W - 1; x++)
      if (gg.isFloor(x, y) && ((gg.isWall(x - 1, y) && gg.isWall(x + 1, y)) || (gg.isWall(x, y - 1) && gg.isWall(x, y + 1)))) F.setLane(x, y);
  // order: the rooms whose anchors decide the rest first (kitchens before halls: the hall skips its hearth then)
  order.clear();
  for (int i = 0; i < (int)gg.rooms.size(); i++) order.push_back(i);
  auto prio = [&](int i) {
    RoomKind k = gg.rooms[(size_t)i].kind;
    if (k == RoomKind::Kitchen) return 0;
    if (k == RoomKind::Common || k == RoomKind::Shopfloor || k == RoomKind::Forge || k == RoomKind::Nave || k == RoomKind::ThroneHall) return 1;
    if (k == RoomKind::Feast || k == RoomKind::TeaRoom || k == RoomKind::Trading || k == RoomKind::Bath || k == RoomKind::Assembly ||
        k == RoomKind::Training || k == RoomKind::Court) return 1;
    return 2;
  };
  std::stable_sort(order.begin(), order.end(), [&](int a, int c) { return prio(a) < prio(c); });
  }
  void furnishOne(int ri) {
    Geo& gg = *ggp;
    Fit& F = *Fp;
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
      // M3b
      case RoomKind::Court: furnishCourt(F, ri, cx); break;
      case RoomKind::Bath: furnishBath(F, ri, cx); break;
      case RoomKind::Changing: furnishChanging(F, ri, cx); break;
      case RoomKind::TeaRoom: furnishTeaRoom(F, ri, cx); break;
      case RoomKind::Parlour: furnishParlour(F, ri); break;
      case RoomKind::Training: furnishTraining(F, ri, cx); break;
      case RoomKind::Feast: furnishFeast(F, ri, cx); break;
      case RoomKind::Assembly: furnishAssembly(F, ri, cx); break;
      case RoomKind::Trading: furnishTrading(F, ri, cx); break;
      default: break;
    }
    }
  void people() {
    Geo& gg = *ggp;
    Fit& F = *Fp;
  // spawns (slots 16*floor+k): the owner at their post on the ground floor, inn patrons at the tables, the jarl's guards
  if (floor == 0) {
    Rng orr(seed ^ 0x0A11CE5u);
    bool ownerHome = b.owner != Role::Villager || P.seat >= 0 || orr.f() < 0.75f;
    int postX = -1, postY = -1;
    if (ownerHome) {
      Spawn o; o.npc = true; o.role = b.owner; o.slot = 0; o.site = b.site;
      // (M3b) the ruler sits on the royal seat's high seat whatever the society built it as; a lord on his own
      if (P.royal) o.role = Role::King;
      else if (P.seat >= 0 && (b.owner == Role::Villager || b.owner == Role::King)) o.role = Role::Jarl;
      int ox = cx.ownerX >= 0 ? cx.ownerX : W / 2, oy = cx.ownerX >= 0 ? cx.ownerY : H / 2;
      spawnNear(F, o, ox, oy, F.g.roomOf(ox, oy));
      m.spawns.push_back(o);
      postX = o.x; postY = o.y;
    }
    // (M3b) the gathering places are full: patrons at the inn's, the mead hall's and the tea house's tables, bathers
    const bool gathering = b.type == Building::Inn || b.type == Building::MeadHall || b.type == Building::TeaHouse || b.type == Building::Bathhouse;
    if (gathering && P.seat < 0) {
      int n = 2 + orr.irange(2);
      int main = 0;   // the room the patrons sit in (a caravanserai's common room is off its court)
      for (size_t i = 0; i < gg.rooms.size(); i++)
        if (gg.rooms[i].kind == RoomKind::Common || gg.rooms[i].kind == RoomKind::Feast || gg.rooms[i].kind == RoomKind::TeaRoom || gg.rooms[i].kind == RoomKind::Bath) { main = (int)i; break; }
      for (int k = 1; k <= n; k++) {
        Spawn p; p.npc = true; p.role = Role::Villager; p.slot = k; p.site = b.site;
        auto at = k - 1 < (int)cx.seats.size() ? cx.seats[(size_t)k - 1] : std::pair<int, int>{W / 2 + k, H - 4};
        spawnNear(F, p, at.first, at.second, main, postX, postY);
        m.spawns.push_back(p);
      }
    }
    if (b.type == Building::Lodge || b.type == Building::Guildhall || b.type == Building::CouncilHall || b.type == Building::Exchange) {
      const Role who = b.type == Building::Lodge ? Role::Guard : Role::Villager;
      for (int k = 1; k <= 2; k++) {
        Spawn p; p.npc = true; p.role = who; p.slot = k; p.site = b.site;
        auto at = k - 1 < (int)cx.seats.size() ? cx.seats[(size_t)k - 1] : std::pair<int, int>{W / 2 + (k == 1 ? -3 : 3), H / 2};
        spawnNear(F, p, at.first, at.second, 0, postX, postY);
        m.spawns.push_back(p);
      }
    }
    if (P.seat >= 0 && b.type != Building::Keep && b.type != Building::Palace)   // the seat's guard
      for (int k = 1; k <= (P.royal ? 4 : 2); k++) {
        Spawn gd; gd.npc = true; gd.role = Role::Guard; gd.slot = k; gd.site = b.site;
        auto at = k - 1 < (int)cx.seats.size() ? cx.seats[(size_t)k - 1] : std::pair<int, int>{P.ex + (k % 2 ? -2 : 2), P.H - 3};
        spawnNear(F, gd, at.first, at.second, -1);
        m.spawns.push_back(gd);
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
      {   // a servant about the gallery (draws sequenced: the right argument's first, as MSVC/GCC evaluate them)
        const int sy = G.y + ur.irange(G.h);
        const int sx = G.x + ur.irange(G.w);
        add(Role::Villager, sx, sy, 0);
      }
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
      if (ur.f() < 0.6f) {   // the maid on her rounds (draws sequenced: the right argument's first, as MSVC/GCC)
        const int my = G.y + ur.irange(G.h);
        const int mx = G.x + ur.irange(G.w);
        add(Role::Villager, mx, my, 0);
      }
    } else if (b.type == Building::House || b.type == Building::StoneHouse || b.type == Building::Shop) {
      std::vector<int> beds = rooms(RoomKind::Bedroom);
      if (!beds.empty() && ur.f() < 0.7f) {   // (sequenced: the bed's draw, then the sleeper's, as MSVC/GCC)
        const int bed = beds[(size_t)ur.irange((int)beds.size())];
        const Role who = b.type == Building::Shop || ur.f() < 0.4f ? Role::Villager : Role::Child;
        atBed(who, bed);
      }
    } else if (b.type == Building::Tower && floor < P.floors - 1 && ur.f() < 0.7f) {
      add(Role::Villager, G.cx(), G.cy(), 0);   // an apprentice at the books
    }
  }
  }
  void finish() {
    Geo& gg = *ggp;
  // the rooms, as the game and the tests read them
  m.rooms.clear();
  for (const RoomDef& R : gg.rooms) {
    RoomInfo ri;
    ri.kind = R.kind;
    ri.r = R.r;
    ri.doorX = R.doorX; ri.doorY = R.doorY;
    ri.bedX = R.bedX; ri.bedY = R.bedY;
    ri.guest = R.guest;
    ri.floorStyle = R.floorStyle;
    m.rooms.push_back(ri);
  }
  m.rebuildSolid();
  }
  // one unit; true once the floor is done
  bool unit() {
    switch (phase) {
      case PH_BP: bp = bldgBlueprint(b); phase = PH_PLAN; return false;
      case PH_PLAN: P = makePlan(b, bp, seed); phase = PH_SHELL; return false;
      case PH_SHELL: shell(); phase = PH_SETUP; return false;
      case PH_SETUP: setup(); phase = PH_FURNISH; idx = 0; return false;
      case PH_FURNISH:
        if (idx < order.size()) { furnishOne(order[idx++]); return false; }
        phase = PH_WALLS; idx = 0; return false;
      case PH_WALLS:
        if (idx < order.size()) { const int ri = order[idx++]; wallDecorRoom(*Fp, ri, ggp->rooms[(size_t)ri].kind); return false; }
        phase = PH_RUGS; return false;
      case PH_RUGS: rugCleanup(*Fp); phase = PH_PEOPLE; return false;
      case PH_PEOPLE: people(); phase = PH_CLUTTER; idx = 0; return false;
      case PH_CLUTTER:
        if (idx < order.size()) { const int ri = order[idx++]; clutterRoom(*Fp, ri, ggp->rooms[(size_t)ri].kind); return false; }
        phase = PH_FINISH; return false;
      case PH_FINISH: finish(); phase = PH_DONE; return true;
      default: return true;
    }
  }
};

}  // namespace

void genInteriorRooms(Map& m, const Bldg& b, uint32_t seed, int floor) { genInteriorRooms(m, b, bldgBlueprint(b), seed, floor); }

void genInteriorRooms(Map& m, const Bldg& b, const bld::Blueprint& bp, uint32_t seed, int floor) {
  InteriorBuild ib(m, b, seed, floor, &bp);
  while (!ib.unit()) {}
}

struct InteriorJob::Impl {
  Bldg b;
  Map m;
  InteriorBuild ib;
  bool done = false;
  Impl(const Bldg& b_, uint32_t seed, int floor) : b(b_), ib(m, b, seed, floor, nullptr) {}
};
InteriorJob::InteriorJob(const Bldg& b, uint32_t seed, int floor) {
  // genInterior's contract (rpg/sim/dungeon.cpp): an empty map, the floor clamped to the building's
  d_.reset(new Impl(b, seed, std::clamp(floor, 0, b.floors() - 1)));
}
InteriorJob::~InteriorJob() = default;
bool InteriorJob::step(double budgetMs, double* worstUnitMs) {
  if (d_->done) return true;
  const auto t0 = std::chrono::steady_clock::now();
  for (;;) {
    const auto u0 = std::chrono::steady_clock::now();
    d_->done = d_->ib.unit();
    const auto u1 = std::chrono::steady_clock::now();
    if (worstUnitMs) *worstUnitMs = std::max(*worstUnitMs, std::chrono::duration<double, std::milli>(u1 - u0).count());
    if (d_->done) return true;
    if (std::chrono::duration<double, std::milli>(u1 - t0).count() >= budgetMs) return false;
  }
}
bool InteriorJob::done() const { return d_->done; }
Map& InteriorJob::map() { return d_->m; }

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
