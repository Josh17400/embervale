// Sub-level generation: natural caves (cellular automata), ancient ruins (rooms + corridors), building interiors
// (the M0b rooms generator in interior_v4.cpp).
#include <cstdlib>
#include <algorithm>
#include <mutex>
#include <unordered_map>
#include <queue>
#include <utility>
#include <vector>
#include "rpg/sim/deco.h"
#include "rpg/sim/interior_v4.h"
#include "rpg/sim/magic.h"
#include "rpg/sim/world.h"
#include "rpg/story/story_internal.h"

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

// ===================================================================================== M10: Trials of the Threshold
namespace {
std::mutex g_trialMx;
std::unordered_map<uint64_t, TrialLayout>& trialReg() {
  static std::unordered_map<uint64_t, TrialLayout> r;
  return r;
}
// the trial's dungeon (rooms.h): an antechamber at the foot, two halls behind sealed gates, the sanctum at the head;
// pillars down the halls, braziers in the corners, the tradition's puzzle in each hall
void genTrial(Map& m, const Site& s, uint32_t seed, TrialLayout& T) {
  using G = magic::Signature;
  m.kind = MapKind::Ruin;
  m.seed = seed;
  const int W = 48, H = 54;
  m.alloc(W, H, Ground::CaveWall);
  const TrialSpec& S = T.spec;
  const G sig = (G)S.sig;
  const bool second = S.which > 0;
  T.plates.clear(); T.gates.clear(); T.chasms.clear();
  auto carve = [&](IRect rc, bool chamfer) {
    for (int y = rc.y; y < rc.y + rc.h; y++)
      for (int x = rc.x; x < rc.x + rc.w; x++) {
        const bool cx = x == rc.x || x == rc.x + rc.w - 1, cy = y == rc.y || y == rc.y + rc.h - 1;
        if (chamfer && cx && cy) continue;   // (the corners cut: no box rooms)
        m.setG(x, y, Ground::StoneFloor);
      }
  };
  const IRect sanct{15, 4, 18, 7}, hall2{7, 16, 34, 10}, hall1{7, 31, 34, 10}, ante{18, 45, 12, 7};
  carve(sanct, true); carve(hall2, true); carve(hall1, true); carve(ante, true);
  carve(IRect{22, 11, 4, 5}, false);    // sanctum <- hall 2
  carve(IRect{22, 26, 4, 5}, false);    // hall 2 <- hall 1
  carve(IRect{22, 41, 4, 4}, false);    // hall 1 <- antechamber
  // shrine niches along the halls' back walls: the walls are not straight runs
  for (const IRect& h : {hall1, hall2})
    for (int x = h.x + 3; x < h.x + h.w - 3; x += 7) {
      if (x >= 19 && x <= 27) continue;
      m.setG(x, h.y - 1, Ground::StoneFloor); m.setG(x + 1, h.y - 1, Ground::StoneFloor);
    }
  T.hall[0] = hall1; T.hall[1] = hall2; T.hall[2] = sanct;
  m.exitX = 24; m.exitY = ante.y + ante.h - 1;
  m.setProp(m.exitX, m.exitY, Prop::Ladder);
  // the dressing: pillars down both halls, braziers in the corners, statues by the antechamber's door
  for (const IRect& h : {hall1, hall2}) {
    for (int x = h.x + 2; x < h.x + h.w - 2; x += 6) {
      if (x >= 19 && x <= 28) continue;
      m.setProp(x, h.y + 1, Prop::Pillar);
      m.setProp(x, h.y + h.h - 2, Prop::Pillar);
    }
    m.setProp(h.x + 1, h.y + 1, Prop::Brazier); m.setProp(h.x + h.w - 2, h.y + 1, Prop::Brazier);
  }
  m.setProp(ante.x + 1, ante.y + 1, Prop::Statue); m.setProp(ante.x + ante.w - 2, ante.y + 1, Prop::Statue);
  m.setProp(ante.x + 2, ante.y + 1, Prop::Brazier); m.setProp(ante.x + ante.w - 3, ante.y + 1, Prop::Brazier);
  m.setProp(sanct.x + 1, sanct.y + 1, Prop::Brazier); m.setProp(sanct.x + sanct.w - 2, sanct.y + 1, Prop::Brazier);
  m.setProp(sanct.x + 3, sanct.y, Prop::Statue); m.setProp(sanct.x + sanct.w - 4, sanct.y, Prop::Statue);
  m.setProp(24, sanct.y, Prop::Altar);
  m.setProp(27, sanct.y, Prop::Chest);
  // the gates: four tiles across the corridor, two deep, right above the hall they close
  auto gate = [&](int y, uint8_t room, int8_t mask) { TrialGate g; g.x = 22; g.y = (int16_t)y; g.w = 4; g.h = 2; g.room = room; g.mask = mask; T.gates.push_back(g); };
  auto plate = [&](int x, int y, uint8_t kind, uint8_t arg, int8_t gt, uint8_t room) {
    TrialPlate p; p.x = (int16_t)x; p.y = (int16_t)y; p.kind = kind; p.arg = arg; p.gate = gt; p.room = room; T.plates.push_back(p);
  };
  int slot = 0;
  auto foe = [&](int x, int y, art::Monster mon) {
    if (m.at(x, y) != Ground::StoneFloor || m.propAt(x, y)) return;
    Spawn sp; sp.x = x; sp.y = y; sp.site = -1; sp.slot = slot++; sp.mon = mon; m.spawns.push_back(sp);
  };
  const art::Monster theme = s.theme == art::Monster::Spider ? art::Monster::Skeleton : s.theme;
  // an inscription beside each gate: the hall's lesson
  m.setProp(20, hall1.y, Prop::Inscription);
  m.setProp(20, hall2.y, Prop::Inscription);
  switch (sig) {
    case G::Song: {
      gate(29, 1, -1); gate(14, 2, -1);
      // three singing stones with a note plate before each, in a shuffled order (listen, and read the motif's glyph
      // over the gate)
      static const int xs[3] = {13, 23, 33};
      const int sh = (int)(hash32(seed) % 3u);
      for (int i = 0; i < 3; i++) {
        const int x = xs[(i + sh) % 3];
        m.setProp(x, hall1.y + 3, Prop::StandingStone);
        plate(x, hall1.y + 5, TP_NOTE, S.motif[i], 0, 1);
      }
      static const int xs2[3] = {10, 24, 37};
      for (int i = 0; i < 3; i++) {
        const int x = xs2[(i + 2 - sh + 3) % 3];
        m.setProp(x, hall2.y + 3, Prop::StandingStone);
        plate(x, hall2.y + 5, TP_NOTE, S.motif[i], 1, 2);
      }
      if (second) {   // the second trial: a false stone sings a note outside the motif
        uint8_t odd = (uint8_t)((S.motif[0] + 1) % 7);
        while (odd == S.motif[1] || odd == S.motif[2] || odd == S.motif[0]) odd = (uint8_t)((odd + 1) % 7);
        m.setProp(17, hall2.y + 3, Prop::StandingStone);
        plate(17, hall2.y + 5, TP_NOTE, odd, 1, 2);
      }
      for (int k = 0; k < (second ? 6 : 3); k++) foe(12 + k * 5, hall2.y + 7, theme);
      break;
    }
    case G::Rune: {
      gate(29, 1, -1); gate(14, 2, -1);
      // a lattice of runes across each hall, two staggered rows a tile apart: whatever crosses the hall crosses a rune
      // (the guardians come over them at the hero: lure them on, keep your own feet off)
      const uint8_t a = (uint8_t)magic::Essence::Fire, b = (uint8_t)magic::Essence::Frost;
      for (int hall = 0; hall < 2; hall++) {
        const IRect& h = hall ? hall2 : hall1;
        for (int i = 0; i < 8; i++) {   // (three rows: whatever path crosses the hall passes a rune's reach)
          plate(9 + i * 4, h.y + 3, TP_RUNE, (i + hall) % 2 ? a : b, -1, (uint8_t)(hall + 1));
          plate(11 + i * 4, h.y + 5, TP_RUNE, (i + hall) % 3 ? b : a, -1, (uint8_t)(hall + 1));
          plate(10 + i * 4, h.y + 7, TP_RUNE, (i + hall + 1) % 2 ? a : b, -1, (uint8_t)(hall + 1));
        }
      }
      for (int k = 0; k < 4; k++) foe(12 + k * 8, hall1.y + 1, theme);
      for (int k = 0; k < (second ? 7 : 5); k++) foe(12 + k * 5, hall2.y + 1, theme);
      break;
    }
    case G::Mask: {
      const int8_t k1 = (int8_t)(hash32(seed ^ 0x3A5Cu) % 3u), k2 = (int8_t)((k1 + 1 + (int)(hash32(seed) % 2u)) % 3);
      gate(29, 1, k1); gate(14, 2, k2);
      static const int xs[3] = {13, 23, 33};
      for (int i = 0; i < 3; i++) plate(xs[i], hall1.y + 6, TP_MASK, (uint8_t)i, -1, 1);
      for (int i = 0; i < 3; i++) plate(11 + i * 13, hall2.y + 7, TP_MASK, (uint8_t)((i + 1) % 3), -1, 2);
      for (int k = 0; k < (second ? 6 : 4); k++) foe(10 + k * 6, hall2.y + 3, theme);
      break;
    }
    case G::Verdant: {
      // a chasm of dark water across each hall (wall to wall: no way round), a seed pod on the near bank
      const IRect c1{hall1.x, hall1.y + 3, hall1.w, 3}, c2{hall2.x, hall2.y + 3, hall2.w, 3};
      for (const IRect& c : {c1, c2})
        for (int y = c.y; y < c.y + c.h; y++)
          for (int x = c.x; x < c.x + c.w; x++) {
            if (m.at(x, y) == Ground::StoneFloor) m.setG(x, y, Ground::Water);
            m.setP(x, y, 0);
          }
      T.chasms.push_back(c1); T.chasms.push_back(c2);
      plate(23, c1.y + c1.h + 1, TP_POD, 0, -1, 1);
      plate(23, c2.y + c2.h + 1, TP_POD, 1, -1, 2);
      for (int k = 0; k < (second ? 5 : 3); k++) foe(12 + k * 6, hall2.y + 1, theme);
      break;
    }
    case G::Blood: {
      gate(29, 1, -1); gate(14, 2, -1);
      m.setProp(14, hall1.y, Prop::Altar);
      plate(14, hall1.y + 2, TP_ALTAR, 0, 0, 1);
      m.setProp(12, hall2.y, Prop::Altar); m.setProp(35, hall2.y, Prop::Altar);
      plate(12, hall2.y + 2, TP_ALTAR, 0, 1, 2);
      plate(35, hall2.y + 2, TP_ALTAR, 0, 1, 2);
      for (int k = 0; k < (second ? 6 : 4); k++) foe(14 + k * 5, hall2.y + 6, theme);
      break;
    }
    case G::Dream: {
      gate(29, 1, -1); gate(14, 2, -1);
      plate(10, hall1.y + 5, TP_PRESS, 0, 0, 1); plate(37, hall1.y + 5, TP_PRESS, 0, 0, 1);
      plate(23, hall1.y + 7, TP_MIRROR, 0, 0, 1);
      plate(9, hall2.y + 4, TP_PRESS, 0, 1, 2); plate(38, hall2.y + 4, TP_PRESS, 0, 1, 2);
      plate(23, hall2.y + 7, TP_MIRROR, 0, 1, 2);
      for (int k = 0; k < (second ? 6 : 4); k++) foe(13 + k * 5, hall2.y + 2, theme);
      break;
    }
    default: gate(29, 1, -1); gate(14, 2, -1); break;
  }
  // the sanctum: its plate before the altar passes the trial; the second trial's guardian keeps it
  plate(24, sanct.y + 3, TP_SANCTUM, 0, -1, 3);
  T.sanctumX = 24; T.sanctumY = sanct.y + 3;
  if (second) {
    Spawn boss; boss.boss = true; boss.slot = 99; boss.x = 24; boss.y = sanct.y + 5;
    boss.mon = theme == art::Monster::Wraith ? art::Monster::Wraith : art::Monster::Draugr;
    m.spawns.push_back(boss);
  }
  // nothing stands on a plate or a gate, or on the arrival tile
  for (const TrialPlate& p : T.plates) m.setP(p.x, p.y, 0);
  for (const TrialGate& g : T.gates)
    for (int y = g.y; y < g.y + g.h; y++) for (int x = g.x; x < g.x + g.w; x++) m.setP(x, y, 0);
  m.setP(m.exitX, m.exitY - 1, 0);
  m.rebuildSolid();
  for (const TrialGate& g : T.gates)
    for (int y = g.y; y < g.y + g.h; y++) for (int x = g.x; x < g.x + g.w; x++) if (m.in(x, y)) m.solid[(size_t)y * m.w + x] = 1;
}
}  // namespace

void registerTrial(uint64_t siteId, const TrialSpec& spec) {
  std::lock_guard<std::mutex> lk(g_trialMx);
  TrialLayout& L = trialReg()[siteId];
  L.spec = spec;
}
const TrialLayout* trialLayoutOf(uint64_t siteId) {
  std::lock_guard<std::mutex> lk(g_trialMx);
  const auto it = trialReg().find(siteId);
  return it == trialReg().end() ? nullptr : &it->second;
}
void clearTrials() {
  std::lock_guard<std::mutex> lk(g_trialMx);
  trialReg().clear();
}

void genRuin(Map& m, const Site& s, uint32_t seed) {
  {   // (M10) a hidden kingdom's Trial of the Threshold (rooms.h)
    TrialLayout* T = nullptr;
    {
      std::lock_guard<std::mutex> lk(g_trialMx);
      const auto it = trialReg().find(s.id);
      if (it != trialReg().end()) T = &it->second;
    }
    if (T) { genTrial(m, s, seed, *T); return; }
  }
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

// M4 (STORY lane, owner 15.3): the lore of a ruin (rpg/story/lore.cpp decides what each prop tells). Laid over the map
// genRuin made, after it is loaded: the generator's own output (its goldens) is untouched, and the same key gives the
// same tiles on every visit. A slab or a grave stands against a wall in a room (never in a corridor), a mural hangs on
// a flat stretch of wall, a journal lies on an open floor, a toppled statue lies across a room's middle; none on the way
// in, on a spawn, beside a chest or within four tiles of another.
void story::placeLoreProps(Map& m, uint64_t key, const std::vector<int>& props, std::vector<story::Engine::PropRef>& out) {
  out.clear();
  if (m.w <= 0 || m.h <= 0) return;
  std::vector<int> dist;
  bfs(m, m.exitX, m.exitY, dist);
  auto floorAt = [&](int x, int y) { return m.in(x, y) && m.at(x, y) == Ground::StoneFloor; };
  auto wallAt = [&](int x, int y) { return !m.in(x, y) || groundSolid(m.at(x, y)); };
  auto clearAt = [&](int x, int y) { return floorAt(x, y) && m.propAt(x, y) == 0; };
  std::vector<std::pair<int, int>> spawns;
  for (const Spawn& sp : m.spawns) spawns.push_back({sp.x, sp.y});
  auto onSpawn = [&](int x, int y) {
    for (const auto& p : spawns) if (std::abs(p.first - x) <= 1 && p.second == y) return true;
    return false;
  };
  // (M10) a Trial of the Threshold: no slab on its puzzle (its plates, gates, chasms, the way through)
  const TrialLayout* trial = trialLayoutOf(key);
  auto puzzleTile = [&](int x, int y) {
    if (!trial) return false;
    if (x >= 20 && x <= 27) return true;
    for (const TrialPlate& p : trial->plates) if (std::abs(p.x - x) <= 2 && std::abs(p.y - y) <= 2) return true;
    for (const TrialGate& g : trial->gates) if (x >= g.x - 2 && x < g.x + g.w + 2 && y >= g.y - 2 && y < g.y + g.h + 2) return true;
    for (const IRect& c : trial->chasms) if (x >= c.x - 1 && x < c.x + c.w + 1 && y >= c.y - 2 && y < c.y + c.h + 2) return true;
    return false;
  };
  for (size_t i = 0; i < props.size(); i++) {
    const Prop p = (Prop)props[i];
    int bx = -1, by = -1, best = -1;
    for (int y = 2; y < m.h - 2; y++)
      for (int x = 2; x < m.w - 2; x++) {
        const int d = dist[(size_t)y * m.w + x];
        if (d < 9 || !clearAt(x, y) || onSpawn(x, y) || puzzleTile(x, y)) continue;
        if (std::abs(x - m.exitX) <= 2 && std::abs(y - m.exitY) <= 3) continue;
        bool nearOther = false;
        for (const auto& o : out) if (std::abs(o.tx - x) <= 4 && std::abs(o.ty - y) <= 4) nearOther = true;
        if (nearOther) continue;
        bool ok = false;
        switch (p) {
          case Prop::Inscription: case Prop::NamedGrave:
            ok = wallAt(x, y - 1) && clearAt(x - 1, y) && clearAt(x + 1, y) && clearAt(x, y + 1) && floorAt(x - 2, y + 1) &&
                 floorAt(x + 2, y + 1) && floorAt(x - 1, y + 2) && floorAt(x + 1, y + 2);
            break;
          case Prop::Mural:
            ok = wallAt(x - 1, y - 1) && wallAt(x, y - 1) && wallAt(x + 1, y - 1) && floorAt(x - 1, y) && floorAt(x + 1, y) && floorAt(x, y + 1);
            break;
          case Prop::LostJournal: {
            ok = true;
            for (int dy = -1; dy <= 1 && ok; dy++) for (int dx = -1; dx <= 1 && ok; dx++) ok = clearAt(x + dx, y + dy);
            break;
          }
          case Prop::ToppledStatue: {
            ok = true;
            for (int dy = -1; dy <= 1 && ok; dy++) for (int dx = -2; dx <= 2 && ok; dx++) ok = clearAt(x + dx, y + dy) && !onSpawn(x + dx, y + dy);
            break;
          }
          default: ok = true; break;
        }
        if (!ok) continue;
        // a chest beside it would crowd the room
        bool chest = false;
        for (int dy = -2; dy <= 2; dy++) for (int dx = -2; dx <= 2; dx++) { const int q = m.propAt(x + dx, y + dy); if (q == (int)Prop::Chest + 1 || q == (int)Prop::Altar + 1) chest = true; }
        if (chest) continue;
        const int score = (int)(hash2(x, y, (uint32_t)(key ^ (key >> 32)) + (uint32_t)i * 7919u) % 1000) + std::min(d, 60) * 6;
        if (score > best) { best = score; bx = x; by = y; }
      }
    if (best < 0) continue;
    m.setProp(bx, by, p);
    if (p == Prop::ToppledStatue) { m.setProp(bx - 1, by, Prop::Filler); m.setProp(bx + 1, by, Prop::Filler); }
    story::Engine::PropRef r;
    r.tx = bx; r.ty = by; r.clue = (int)i; r.prop = (int)p;
    out.push_back(r);
  }
  m.rebuildSolid();
}

namespace {
// ===================================================================================== interiors, WORLDGEN_V7 (M0b)
// Rooms and storeys (VISION_PLAN 15.7): the generator lives in rpg/sim/interior_v4.cpp.
void genInteriorV4(Map& m, const Bldg& b, uint32_t seed, int floor) { genInteriorRooms(m, b, seed, floor); }
}  // namespace

void genInterior(Map& m, const Bldg& b, uint32_t seed, int floor) {
  m = Map();
  // M2: every building is furnished by the M0b rooms generator (interior_v4.cpp). The pre-M0b interiors (V2, V3) went
  // with the classic island; a Bldg is always WORLDGEN_V7 or later.
  genInteriorV4(m, b, seed, std::clamp(floor, 0, b.floors() - 1));
}
