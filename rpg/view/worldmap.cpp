// The world map (M1, VISION_PLAN 2.11): an endless world needs a map that pans and zooms over global tiles, from the
// street around the hero out to whole kingdoms. The minimap stays on the Active Window (hud.cpp).
//   - zoom: 8 levels, 0.25 .. 32 tiles per map pixel; pinch / wheel / Q E / the + - buttons
//   - pan: drag (finger or mouse), the arrow keys; ME (or C) centres on the hero
//   - unexplored land is parchment (Game::explored, 8 x 8-tile cells); explored land shows the generator's macro
//     fields (biome, water, relief) and, inside the window at close zoom, the real tiles (roads, rivers, buildings)
//   - kingdom borders (macro().kingdom changes) and labels; discovered places, the hero and the quest target
//   - tap a discovered place, then FAST TRAVEL (Game::fastTravel)
// The map is painted in 64 x 64-pixel tiles cached per zoom level, a few per frame (a budget, so the phone never
// stalls); tiles not painted yet show plain parchment. Classic islands (no generator) read their one map instead.
#include <SDL3/SDL.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <initializer_list>
#include <string>
#include <unordered_map>
#include <vector>
#include "rpg/view/view.h"
#include "rpg/world/source.h"

namespace {
constexpr int TS = 64;   // map pixels per cached tile side
const float kZoom[] = {0.25f, 0.5f, 1, 2, 4, 8, 16, 32};
constexpr int NZOOM = (int)(sizeof kZoom / sizeof kZoom[0]);
const Color kGold(0.98f, 0.82f, 0.42f), kText(0.93f, 0.9f, 0.82f), kDim(0.62f, 0.58f, 0.52f), kInk(0.18f, 0.12f, 0.08f);

inline uint32_t C(int r, int g, int b) { return rgba(std::clamp(r, 0, 255), std::clamp(g, 0, 255), std::clamp(b, 0, 255)); }
inline uint32_t mixc(uint32_t a, uint32_t b, float t) {
  auto ch = [&](int s) { return (int)(((a >> s) & 255) + ((int)((b >> s) & 255) - (int)((a >> s) & 255)) * t); };
  return C(ch(0), ch(8), ch(16));
}
inline uint32_t shadec(uint32_t a, float k) { return C((int)((a & 255) * k), (int)(((a >> 8) & 255) * k), (int)(((a >> 16) & 255) * k)); }
Color colOf(uint32_t c, float a = 1) { return Color((c & 255) / 255.0f, ((c >> 8) & 255) / 255.0f, ((c >> 16) & 255) / 255.0f, a); }

struct MapState {
  bool init = false;
  uint64_t world = 0;            // the world the view belongs to (a new game re-centres and drops the tiles)
  double cx = 0, cy = 0;         // the centre in global tiles
  int zi = 4;
  uint64_t sig = 0;              // the explored mask's signature the tiles were painted with
  struct T { Tex tex; float used = 0; };
  std::unordered_map<uint64_t, T> tiles;
  // pointers: one finger / the mouse drags, two fingers pinch
  struct Ptr { uint64_t id; Vec2 p, start; };
  std::vector<Ptr> ptrs;
  float pinchD = 0;
  int pinchZ = 4;
  bool moved = false;
  float rx = 0, ry = 0, rw = 1, rh = 1;   // the map rectangle drawn last (box coordinates)
};
MapState S;

uint64_t tileKey(int zi, int ix, int iy) { return ((uint64_t)(uint32_t)zi << 56) ^ ((uint64_t)(uint32_t)(ix & 0xFFFFFFF) << 28) ^ (uint32_t)(iy & 0xFFFFFFF); }

uint64_t exploredSig(const Game& g) {
  uint64_t h = g.explored.regions.size() * 0x9E3779B97F4A7C15ull;
  for (auto& kv : g.explored.regions) {
    uint64_t s = kv.first;
    for (uint8_t b : kv.second) s = s * 1099511628211ull + b;
    h ^= ew::mix64(s);
  }
  return h;
}

// the colour of a known tile of the window (close zoom)
uint32_t groundInk(Ground gr) {
  switch (gr) {
    case Ground::DeepWater: return C(70, 104, 140);
    case Ground::Water: return C(96, 136, 166);
    case Ground::Sand: return C(218, 200, 150);
    case Ground::Snow: return C(236, 236, 232);
    case Ground::Rock: return C(140, 128, 112);
    case Ground::Road: case Ground::Bridge: return C(132, 98, 62);
    case Ground::Plaza: return C(160, 132, 98);
    case Ground::Swamp: return C(124, 130, 92);
    case Ground::ForestFloor: return C(110, 136, 86);
    case Ground::Autumn: return C(184, 142, 88);
    case Ground::Tundra: return C(156, 160, 132);
    case Ground::Farmland: return C(166, 140, 96);
    case Ground::Dirt: return C(170, 142, 104);
    default: return C(156, 172, 114);
  }
}
uint32_t biomeInk(Biome b) {
  switch (b) {
    case Biome::Ocean: return C(70, 104, 140);
    case Biome::Beach: return C(218, 200, 150);
    case Biome::Forest: return C(112, 138, 88);
    case Biome::Autumn: return C(184, 142, 88);
    case Biome::Taiga: return C(118, 136, 112);
    case Biome::Snow: return C(234, 236, 236);
    case Biome::Swamp: return C(124, 130, 92);
    case Biome::Desert: return C(222, 196, 142);
    case Biome::Mountain: return C(150, 140, 126);
    default: return C(160, 176, 116);
  }
}
const uint32_t kParch = C(214, 194, 152), kParchDark = C(188, 164, 122);
}  // namespace

// one cached tile: a 65 x 65 sample grid (one extra row and column for the coast / border / hillshade tests)
static Canvas paintMapTile(Game& g, int zi, int ix, int iy) {
  const float z = kZoom[zi];
  const int N = TS + 1;
  struct Smp { uint32_t c; uint8_t h; bool water, known; uint64_t kingdom; Ground gr; bool bldg; };
  std::vector<Smp> grid((size_t)N * N);
  const bool endless = g.world.endless && g.world.src;
  const Map& m = g.world.over;
  // (M1) the edge of the explored land wanders (a warped look-up of the 8-tile fog cells: no square blocks), and a
  // discovered settlement shows whole (no town cut by a straight fog edge)
  auto seenSoft = [&](int32_t gx, int32_t gy) {
    const float jx = (vnoise(gx / 13.0f, gy / 13.0f, 701) - 0.5f) * 14.0f + (vnoise(gx / 5.0f, gy / 5.0f, 703) - 0.5f) * 4.0f;
    const float jy = (vnoise(gx / 13.0f, gy / 13.0f, 709) - 0.5f) * 14.0f + (vnoise(gx / 5.0f, gy / 5.0f, 711) - 0.5f) * 4.0f;
    return g.explored.seen(gx + (int32_t)std::lround(jx), gy + (int32_t)std::lround(jy));
  };
  struct GRect { int32_t x0, y0, x1, y1; };
  std::vector<GRect> towns, cores;   // discovered settlements: with a margin (shown whole) / their built area
  if (endless) {
    const int32_t tx0 = (int32_t)std::floor(ix * TS * z) - 8, ty0 = (int32_t)std::floor(iy * TS * z) - 8;
    const int32_t tx1 = (int32_t)std::floor((ix + 1) * TS * z) + 8, ty1 = (int32_t)std::floor((iy + 1) * TS * z) + 8;
    for (const Site& st : g.world.sites) {
      if (!st.discovered || !st.settlement()) continue;
      GRect r{g.world.ox + st.r.x - 6, g.world.oy + st.r.y - 6, g.world.ox + st.r.x + st.r.w + 6, g.world.oy + st.r.y + st.r.h + 6};
      if (r.x1 < tx0 || r.y1 < ty0 || r.x0 > tx1 || r.y0 > ty1) continue;
      towns.push_back(r);
      cores.push_back(GRect{r.x0 + 6, r.y0 + 6, r.x1 - 6, r.y1 - 6});
    }
  }
  for (int y = 0; y < N; y++)
    for (int x = 0; x < N; x++) {
      const int32_t gx = (int32_t)std::floor((ix * TS + x - 1 + 0.5f) * z), gy = (int32_t)std::floor((iy * TS + y - 1 + 0.5f) * z);
      Smp s{kParch, 0, false, true, 0, Ground::Void, false};
      const int lx = gx - g.world.ox, ly = gy - g.world.oy;
      if (endless) {
        s.known = seenSoft(gx, gy);
        // zoomed out a map pixel spans many fog cells: it is known if any part of it was seen, so the roads walked and
        // the towns visited stay visible as the map zooms out (instead of washing out below a pixel)
        if (!s.known && z >= 4) {
          const int32_t hz = (int32_t)(z * 0.5f);
          s.known = seenSoft(gx - hz, gy - hz) || seenSoft(gx + hz, gy - hz) || seenSoft(gx - hz, gy + hz) || seenSoft(gx + hz, gy + hz);
        }
        for (const GRect& r : towns)
          if (!s.known && gx >= r.x0 && gy >= r.y0 && gx < r.x1 && gy < r.y1) s.known = true;
        if (!s.known && z >= 2) {
          // the unexplored land is not blank: the coasts and the open water are sketched in faintly (the realm's
          // outline, as an old map shows it), the rest stays parchment
          ew::MacroSample ms = z >= 8 ? g.world.src->macroFar(gx, gy) : g.world.src->macro(gx, gy);
          s.water = ms.water;
        }
        if (s.known) {
          if (z <= 2 && m.in(lx, ly)) {
            s.gr = m.at(lx, ly);
            s.c = groundInk(s.gr);
            s.water = groundWater(s.gr);
            s.h = (uint8_t)m.heightAt(lx, ly);
            s.bldg = m.bldgAt[(size_t)ly * m.w + lx] >= 0;
            ew::MacroSample ms = g.world.src->macro(gx, gy);
            s.kingdom = ms.kingdom;
          } else {
            ew::MacroSample ms = z >= 8 ? g.world.src->macroFar(gx, gy) : g.world.src->macro(gx, gy);
            s.water = ms.water;
            s.c = ms.water ? (ms.elev < ew::ELEV_SEA - 4000 ? C(70, 104, 140) : C(96, 136, 166)) : biomeInk(ms.biome);
            s.h = ms.height;
            s.kingdom = ms.kingdom;
            // (M1 round 3) the land is drawn, not washed in one green: woods stippled with little trees, mountains
            // hatched, marsh in dashes, the built area of a discovered settlement in roofs and streets
            if (!ms.water) {
              const int mx = ix * TS + x, my = iy * TS + y;
              const Biome b = ms.biome;
              if (b == Biome::Forest || b == Biome::Taiga || b == Biome::Autumn) {
                const int jx = (int)(hash2(mx / 3, my / 3, 731) % 2);
                if ((mx + jx) % 3 == 0 && my % 3 == 0) s.c = shadec(s.c, 0.68f);
                else if ((mx + jx) % 3 == 0 && my % 3 == 1) s.c = shadec(s.c, 0.84f);
              } else if (b == Biome::Mountain) {
                if (((mx - my) & 3) == 0) s.c = shadec(s.c, 0.82f);
              } else if (b == Biome::Swamp) {
                if (my % 3 == 0 && (mx % 5) < 3) s.c = shadec(s.c, 0.8f);
              } else if (b == Biome::Plains || b == Biome::Beach) {
                if (hashf(mx, my, 733) < 0.05f) s.c = shadec(s.c, 0.88f);
              }
              for (const GRect& r : cores)
                if (gx >= r.x0 && gy >= r.y0 && gx < r.x1 && gy < r.y1) {
                  const bool street = ((gx - r.x0) % 9) < 2 || ((gy - r.y0) % 8) < 2;
                  s.c = street ? C(176, 150, 112) : C(150, 84, 64);
                  s.bldg = !street;
                  break;
                }
            }
          }
        }
      } else if (m.in(gx, gy)) {
        s.gr = m.at(gx, gy);
        s.c = groundInk(s.gr);
        s.water = groundWater(s.gr) || s.gr == Ground::Void;
        s.bldg = m.bldgAt[(size_t)gy * m.w + gx] >= 0;
      } else { s.c = C(70, 104, 140); s.water = true; }
      grid[(size_t)y * N + x] = s;
    }
  Canvas c(TS, TS);
  for (int y = 0; y < TS; y++)
    for (int x = 0; x < TS; x++) {
      const Smp& s = grid[(size_t)(y + 1) * N + x + 1];
      const Smp& nw = grid[(size_t)y * N + x];
      const Smp& n = grid[(size_t)y * N + x + 1];
      const Smp& w = grid[(size_t)(y + 1) * N + x];
      const int px = ix * TS + x, py = iy * TS + y;
      const float grain = hashf(px, py, 77);
      uint32_t k;
      if (!s.known) {
        // parchment: paper grain and a faint hatching, a little darker along the edge of the known land
        k = mixc(kParch, kParchDark, grain * 0.35f + (((px + py) & 7) == 0 ? 0.25f : 0.0f));
        if (endless && z >= 2) {   // the sketched outline of the unexplored coasts and seas
          if (s.water) k = mixc(k, C(120, 150, 170), ((px * 3 + py * 5) & 7) == 0 ? 0.42f : 0.28f);
          if ((!n.known && s.water != n.water) || (!w.known && s.water != w.water)) k = mixc(k, C(64, 52, 44), 0.45f);
        }
        if (n.known || w.known || nw.known) k = shadec(k, 0.9f);
      } else {
        k = s.c;
        if (!s.water) {
          // hillshade from the relief levels (light from the north-west), and a touch lighter with height
          const int dh = (int)s.h - (int)nw.h;
          k = shadec(k, std::clamp(0.97f + 0.025f * s.h + 0.07f * dh, 0.75f, 1.2f));
        } else {
          if ((((px * 3 + py * 5) & 15) == 0)) k = shadec(k, 1.08f);   // a few wave strokes
        }
        if (s.bldg) k = C(128, 72, 58);
        // coastlines in ink
        if ((n.known && s.water != n.water) || (w.known && s.water != w.water)) k = C(64, 52, 44);
        // kingdom borders: a dotted line where the ruler changes (between known cells only)
        if (((n.known && s.kingdom != n.kingdom) || (w.known && s.kingdom != w.kingdom)) && !s.water && ((px + py) % 3 != 0)) k = C(150, 40, 44);
        // the paper shows through the paint, and the known land fades into the parchment at its edge (dithered)
        float fade = 0.16f + grain * 0.06f;
        if (z >= 8) {}   // (zoomed far out a walked road is a pixel or two wide: no edge fade, or it vanishes)
        else if (!n.known || !w.known || !nw.known) fade = 0.55f;
        else {
          const Smp& e = grid[(size_t)(y + 1) * N + std::min(N - 1, x + 2)];
          const Smp& so = grid[(size_t)std::min(N - 1, y + 2) * N + x + 1];
          if (!e.known || !so.known) fade = 0.4f;
        }
        if (fade > 0.3f && grain < 0.35f) k = mixc(k, kParch, 0.85f);
        k = mixc(k, kParch, fade);
      }
      c.set(x, y, k);
    }
  return c;
}

void View::drawWorldMap(Game& g, float x, float y, float w, float h) {
  Pix& P = *pix_;
  const uint64_t wid = g.world.seed ^ ((uint64_t)g.world.genVersion << 56) ^ (g.world.endless ? 1ull << 55 : 0);
  auto playerG = [&](double& gx, double& gy) {
    Vec2 pp = g.pl().p;
    if (g.inside) {
      if (g.subSite >= 0) pp = Vec2(g.world.sites[g.subSite].ex * 16.0f, g.world.sites[g.subSite].ey * 16.0f);
      else if (g.subBldg >= 0) pp = Vec2(g.world.over.bldgs[g.subBldg].doorX() * 16.0f, g.world.over.bldgs[g.subBldg].doorY() * 16.0f);
    }
    gx = pp.x / 16.0 + g.world.ox; gy = pp.y / 16.0 + g.world.oy;
  };
  if (!S.init || S.world != wid) {
    for (auto& kv : S.tiles) pix_->destroy(kv.second.tex);
    S.tiles.clear();
    S.init = true; S.world = wid;
    playerG(S.cx, S.cy);
    S.zi = g.world.endless ? 4 : 2;
  }
  static float lastDraw = -10;
  if (t_ - lastDraw > 0.5f && mapSel_ < 0) playerG(S.cx, S.cy);   // the map was closed: open it on the hero
  lastDraw = t_;
  const uint64_t sig = exploredSig(g);
  if (sig != S.sig) {   // the hero has seen more: repaint (the tiles fill back in over a few frames)
    for (auto& kv : S.tiles) pix_->destroy(kv.second.tex);
    S.tiles.clear();
    S.sig = sig;
  }
  S.rx = x; S.ry = y; S.rw = w; S.rh = h;
  const float z = kZoom[S.zi];
  P.rect(x, y, w, h, colOf(kParch));
  // the tiles in view, painted within a per-frame budget (nearest the centre first)
  const double left = S.cx / z - w / 2, top = S.cy / z - h / 2;   // map pixel at the rectangle's top-left
  const int ix0 = (int)std::floor(left / TS), iy0 = (int)std::floor(top / TS);
  const int ix1 = (int)std::floor((left + w) / TS), iy1 = (int)std::floor((top + h) / TS);
  std::vector<std::pair<int, int>> todo;
  const auto t0 = std::chrono::steady_clock::now();
  P.pushBox((int)x, (int)y, (int)w, (int)h);   // clip the tiles to the map rectangle
  for (int iy = iy0; iy <= iy1; iy++)
    for (int ix = ix0; ix <= ix1; ix++) {
      auto it = S.tiles.find(tileKey(S.zi, ix, iy));
      const float sx = (float)std::floor(ix * TS - left), sy = (float)std::floor(iy * TS - top);
      if (it == S.tiles.end()) { todo.push_back({ix, iy}); continue; }
      it->second.used = t_;
      P.blit(it->second.tex, sx, sy);
    }
  std::sort(todo.begin(), todo.end(), [&](const std::pair<int, int>& a, const std::pair<int, int>& b) {
    auto d = [&](const std::pair<int, int>& q) { double dx = (q.first + 0.5) * TS - (left + w / 2), dy = (q.second + 0.5) * TS - (top + h / 2); return dx * dx + dy * dy; };
    return d(a) < d(b);
  });
  for (auto& q : todo) {
    if (std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count() > 6.0) break;
    MapState::T t;
    t.tex = pix_->bake(paintMapTile(g, S.zi, q.first, q.second));
    t.used = t_;
    S.tiles[tileKey(S.zi, q.first, q.second)] = t;
    P.blit(t.tex, (float)std::floor(q.first * TS - left), (float)std::floor(q.second * TS - top));
  }
  // bound the cache (the oldest tiles go)
  if (S.tiles.size() > 192) {
    std::vector<std::pair<float, uint64_t>> age;
    for (auto& kv : S.tiles) age.push_back({kv.second.used, kv.first});
    std::sort(age.begin(), age.end());
    for (size_t i = 0; i < age.size() - 128; i++) { pix_->destroy(S.tiles[age[i].second].tex); S.tiles.erase(age[i].second); }
  }
  auto toScr = [&](double gx, double gy) { return Vec2((float)(gx / z - left), (float)(gy / z - top)); };
  // (M1) labels are placed after the markers, most important first, and one that would overlap a placed label (or
  // the hero's marker) tries the other side of its marker and is otherwise left out: the selected place, capitals,
  // cities, the kingdoms, towns, villages
  struct Lab { int prio; float x, y, alt; std::string s; Color ink, halo; };
  std::vector<Lab> labs;
  if (g.world.endless && z >= 4)
    for (const Kingdom& k : g.world.kingdoms) {
      if (!g.explored.seen(k.gx, k.gy)) continue;
      Vec2 s = toScr(k.gx + 0.5, k.gy + 0.5);
      if (s.x < -60 || s.y < -20 || s.x > w + 60 || s.y > h + 20) continue;
      labs.push_back({3, s.x, s.y - 14, s.y + 6, "KINGDOM OF " + k.name, colOf(k.color ? k.color : rgba(150, 40, 44)), Color(0.95f, 0.9f, 0.8f, 0.7f)});
    }
  // discovered places
  for (int i = 0; i < (int)g.world.sites.size(); i++) {
    const Site& st = g.world.sites[i];
    if (!st.discovered) continue;
    Vec2 s = toScr(st.ex + g.world.ox + 0.5, st.ey + g.world.oy + 0.5);
    if (s.x < -8 || s.y < -8 || s.x > w + 8 || s.y > h + 8) continue;
    Color c;
    float r = 1.5f;
    switch (st.type) {
      case SiteType::City: c = Color(0.95f, 0.85f, 0.4f); r = 3; break;
      case SiteType::Town: c = Color(0.95f, 0.85f, 0.55f); r = 2.5f; break;
      case SiteType::Village: c = Color(0.9f, 0.85f, 0.7f); r = 2; break;
      case SiteType::Cave: c = Color(0.2f, 0.15f, 0.1f); break;
      case SiteType::Ruin: c = st.mainQuest ? Color(1, 0.45f, 0.1f) : Color(0.45f, 0.3f, 0.55f); break;
      case SiteType::BanditCamp: c = Color(0.75f, 0.15f, 0.12f); break;
      case SiteType::Shrine: c = Color(0.4f, 0.7f, 1); break;
      case SiteType::DragonLair: c = Color(1, 0.3f, 0.1f); r = 2.5f; break;
      default: c = kText; break;
    }
    P.rect(s.x - r - 1, s.y - r - 1, r * 2 + 2, r * 2 + 2, Color(0.1f, 0.06f, 0.04f));
    P.rect(s.x - r, s.y - r, r * 2, r * 2, c);
    if (st.capital) {   // a crown over a capital
      P.rect(s.x - 3, s.y - r - 5, 7, 2, kGold);
      for (int k = 0; k < 3; k++) P.rect(s.x - 3 + k * 3, s.y - r - 7, 1, 2, kGold);
    }
    if (st.cleared && (st.type == SiteType::Cave || st.type == SiteType::Ruin || st.type == SiteType::BanditCamp)) P.rect(s.x - 0.5f, s.y - 0.5f, 1, 1, Color(1, 1, 1));
    if (i == mapSel_) P.frame(s.x - r - 3, s.y - r - 3, r * 2 + 6, r * 2 + 6, Color(1, 1, 1));
    // (M1 round 3) every discovered settlement is named at the land zoom (the start village and a visited town were
    // bare squares there), towns and cities further out
    const bool label = st.type == SiteType::City || (st.type == SiteType::Town && z <= 16) || (st.settlement() && z <= 8) || i == mapSel_;
    if (label) {
      const int prio = i == mapSel_ ? 0 : st.capital ? 1 : st.type == SiteType::City ? 2 : st.type == SiteType::Town ? 4 : 5;
      labs.push_back({prio, s.x, s.y + r + 2, s.y - r - (st.capital ? 17 : 11), st.name, kInk, Color(0.95f, 0.9f, 0.8f, 0.6f)});
    }
  }
  // (M1 round 3) the tracked quest's destination: a large pulsing marker named after the place it leads to (even one
  // not discovered yet), so the gold tick on the map says where the quest goes
  int qtx = 0, qty = 0;
  const bool hasQ = g.trackedQuest >= 0 && g.questTarget(g.trackedQuest, qtx, qty);
  Vec2 qs;
  if (hasQ) {
    qs = toScr(qtx + g.world.ox + 0.5, qty + g.world.oy + 0.5);
    const bool onMap = qs.x >= 0 && qs.y >= 0 && qs.x < w && qs.y < h;
    qs.x = std::clamp(qs.x, 6.0f, w - 7); qs.y = std::clamp(qs.y, 12.0f, h - 6);
    std::string nm;
    int bd = 12 * 12;
    for (const Site& st : g.world.sites) {
      const int d = (st.ex - qtx) * (st.ex - qtx) + (st.ey - qty) * (st.ey - qty);
      if (d <= bd) { bd = d; nm = st.name; }
    }
    if (nm.empty()) nm = "QUEST";
    else nm += " (QUEST)";
    if (!onMap) nm += " >";
    labs.push_back({0, qs.x, qs.y + 6, qs.y - 20, nm, Color(0.45f, 0.24f, 0.04f), Color(1.0f, 0.95f, 0.8f, 0.8f)});
  }
  {
    std::stable_sort(labs.begin(), labs.end(), [](const Lab& a, const Lab& b) { return a.prio < b.prio; });
    double pgx0, pgy0;
    playerG(pgx0, pgy0);
    const Vec2 hp = toScr(pgx0, pgy0);
    struct Box { float x0, y0, x1, y1; };
    std::vector<Box> taken{{hp.x - 4, hp.y - 4, hp.x + 4, hp.y + 4}};
    auto clear = [&](const Box& b) {
      for (const Box& o : taken) if (b.x0 < o.x1 && b.x1 > o.x0 && b.y0 < o.y1 && b.y1 > o.y0) return false;
      return true;
    };
    for (Lab l : labs) {
      const float tw = (float)P.textW(l.s, 1);
      l.x = std::clamp(l.x, tw / 2 + 2, std::max(tw / 2 + 2, w - tw / 2 - 2));   // (M1 round 3) whole on the map, not cut at its edge
      bool placed = false;
      for (float yy : {l.y, l.alt}) {
        Box b{l.x - tw / 2 - 1, yy - 1, l.x + tw / 2 + 1, yy + 9};
        if (!clear(b) && l.prio > 0) continue;
        taken.push_back(b);
        P.text(l.x + 1, yy + 1, l.s, 1, l.halo, 1);
        P.text(l.x, yy, l.s, 1, l.ink, 1);
        placed = true;
        break;
      }
      (void)placed;
    }
  }
  // the hero
  double pgx, pgy;
  playerG(pgx, pgy);
  Vec2 ps = toScr(pgx, pgy);
  if (((int)(t_ * 3)) & 1) { P.rect(ps.x - 3, ps.y, 7, 1, Color(1, 1, 1)); P.rect(ps.x, ps.y - 3, 1, 7, Color(1, 1, 1)); }
  P.rect(ps.x - 1, ps.y - 1, 3, 3, Color(0.85f, 0.1f, 0.1f));
  if (hasQ) {
    // a gold diamond in a dark outline with a pulsing ring round it
    const float pr = 7 + 2.5f * (0.5f + 0.5f * std::sin(t_ * 5));
    for (int k = 0; k < 28; k++) {
      const float a = k / 28.0f * 6.2831853f;
      P.rect(std::floor(qs.x + std::cos(a) * pr), std::floor(qs.y + std::sin(a) * pr), 1, 1, Color(0.98f, 0.82f, 0.42f, 0.8f));
    }
    for (int pass = 0; pass < 2; pass++) {
      const int rr = pass == 0 ? 5 : 4;
      for (int yy = -rr; yy <= rr; yy++) {
        const int hw = rr - std::abs(yy);
        P.rect(qs.x - hw, qs.y + yy, hw * 2 + 1, 1, pass == 0 ? Color(0.18f, 0.1f, 0.04f) : kGold);
      }
    }
    P.rect(qs.x, qs.y - 2, 1, 3, Color(0.45f, 0.24f, 0.04f));
    P.rect(qs.x, qs.y + 2, 1, 1, Color(0.45f, 0.24f, 0.04f));
  }
  // scale bar
  {
    const float tilesPer40 = 40 * z;
    std::string sc = tilesPer40 >= 1000 ? std::to_string((int)(tilesPer40 / 100) / 10.0).substr(0, 3) + "K TILES" : std::to_string((int)tilesPer40) + " TILES";
    P.rect(6, h - 9, 40, 2, kInk);
    P.text(50, h - 12, sc, 1, kInk);
  }
  P.popBox();
  P.frame(x - 1, y - 1, w + 2, h + 2, Color(0.4f, 0.3f, 0.2f));
}

// ---- input (box coordinates)
void View::worldMapZoom(int dz, Vec2 at) {
  const int nz = std::clamp(S.zi + dz, 0, NZOOM - 1);
  if (nz == S.zi) return;
  // keep the point under the cursor / pinch in place
  const float z0 = kZoom[S.zi], z1 = kZoom[nz];
  const double ax = at.x - (S.rx + S.rw / 2), ay = at.y - (S.ry + S.rh / 2);
  const bool inside = at.x >= S.rx && at.y >= S.ry && at.x < S.rx + S.rw && at.y < S.ry + S.rh;
  if (inside) { S.cx += ax * (z0 - z1); S.cy += ay * (z0 - z1); }
  S.zi = nz;
  audio_->play(Sfx::MenuMove);
}

void View::worldMapCentre(Game& g, int site) {
  if (site < 0) {
    Vec2 pp = g.pl().p;
    if (g.inside && g.subSite >= 0) pp = Vec2(g.world.sites[g.subSite].ex * 16.0f, g.world.sites[g.subSite].ey * 16.0f);
    else if (g.inside && g.subBldg >= 0) pp = Vec2(g.world.over.bldgs[g.subBldg].doorX() * 16.0f, g.world.over.bldgs[g.subBldg].doorY() * 16.0f);
    S.cx = pp.x / 16.0 + g.world.ox; S.cy = pp.y / 16.0 + g.world.oy;
    return;
  }
  if (site < (int)g.world.sites.size()) { S.cx = g.world.sites[site].ex + g.world.ox + 0.5; S.cy = g.world.sites[site].ey + g.world.oy + 0.5; }
}

bool View::worldMapKey(Game& g, int key) {
  const float step = 24 * kZoom[S.zi];
  switch (key) {
    case SDLK_Q: case SDLK_MINUS: case SDLK_KP_MINUS: worldMapZoom(1, Vec2(-1, -1)); return true;
    case SDLK_E: case SDLK_EQUALS: case SDLK_KP_PLUS: worldMapZoom(-1, Vec2(-1, -1)); return true;
    case SDLK_LEFT: S.cx -= step; return true;
    case SDLK_RIGHT: S.cx += step; return true;
    case SDLK_UP: S.cy -= step; return true;
    case SDLK_DOWN: S.cy += step; return true;
    case SDLK_C: case SDLK_HOME: worldMapCentre(g, -1); return true;
    default: return false;
  }
}

// pointer phases: 0 down, 1 move, 2 up. Returns true for a tap (an up without a drag) the caller treats as a pick.
bool View::worldMapPointer(int phase, uint64_t id, Vec2 p) {
  auto find = [&]() -> MapState::Ptr* { for (auto& q : S.ptrs) if (q.id == id) return &q; return nullptr; };
  const float z = kZoom[S.zi];
  if (phase == 0) {
    if (p.x < S.rx || p.y < S.ry || p.x >= S.rx + S.rw || p.y >= S.ry + S.rh) return false;
    if (!find()) S.ptrs.push_back({id, p, p});
    if (S.ptrs.size() == 1) S.moved = false;
    if (S.ptrs.size() == 2) { S.pinchD = std::max(8.0f, len(S.ptrs[0].p - S.ptrs[1].p)); S.pinchZ = S.zi; S.moved = true; }
    return false;
  }
  MapState::Ptr* q = find();
  if (!q) return false;
  if (phase == 1) {
    const Vec2 d = p - q->p;
    if (S.ptrs.size() == 1) {
      if (len2(p - q->start) > 16) S.moved = true;
      if (S.moved) { S.cx -= d.x * z; S.cy -= d.y * z; }
    } else if (S.ptrs.size() >= 2) {
      q->p = p;
      const float dd = std::max(8.0f, len(S.ptrs[0].p - S.ptrs[1].p));
      // every doubling of the finger spread is one zoom level closer
      const int want = std::clamp(S.pinchZ - (int)std::lround(std::log2(dd / S.pinchD)), 0, NZOOM - 1);
      if (want != S.zi) worldMapZoom(want - S.zi, (S.ptrs[0].p + S.ptrs[1].p) * 0.5f);
      return false;
    }
    q->p = p;
    return false;
  }
  // up
  const bool tapped = !S.moved && S.ptrs.size() == 1;
  S.ptrs.erase(std::remove_if(S.ptrs.begin(), S.ptrs.end(), [&](const MapState::Ptr& r) { return r.id == id; }), S.ptrs.end());
  return tapped;
}

// the discovered place nearest a box point on the map (within 10 px), or -1
int View::worldMapPick(Game& g, Vec2 p) {
  const float z = kZoom[S.zi];
  const double left = S.cx / z - S.rw / 2, top = S.cy / z - S.rh / 2;
  int best = -1;
  float bd = 10 * 10;
  for (int i = 0; i < (int)g.world.sites.size(); i++) {
    const Site& s = g.world.sites[i];
    if (!s.discovered) continue;
    Vec2 q((float)((s.ex + g.world.ox + 0.5) / z - left + S.rx), (float)((s.ey + g.world.oy + 0.5) / z - top + S.ry));
    float d = len2(q - p);
    if (d < bd) { bd = d; best = i; }
  }
  return best;
}

int View::worldMapZoomLevel() const { return S.zi; }
