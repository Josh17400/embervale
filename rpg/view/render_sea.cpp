// M10 "Beyond the Mist": the sea on screen (View's SEA block, rpg/view/view.h). ART lane.
//
// Hooks (render.cpp / hud.cpp call them):
//   seaEvent       Ev::Sea, the update's event pass          seaUpdate     update, after magicUpdate
//   seaShift       Ev::WindowShift                            drawSeaWater  drawWorld, after the terrain, before
//                                                                           drawMagicGround (waves, foam, wakes)
//   seaSorted      collect the sorted pass's sea drawables   drawSeaSorted Drawable kind 10 (a boat; the one aboard
//                                                                           sorts just before the hero, who stands on it)
//   seaLights      drawLighting                               drawSeaOver   View::draw after the weather (the Mist, storms)
//   drawSeaHud     drawHud (desktop and phone)                seaFillInput  the HUD's input pass (Input::reel)
// and a few free functions render.cpp / hud.cpp declare: seaHeroLift (the hero stands on the deck), seaHeroUnderRig
// (the rig stood over the hero: the ghost silhouette shows him), seaDrawSwimmer (a sea creature half under the swell),
// seaHudRect (the sail panel's place: the quest arrow keeps out of it).
//
// What is drawn:
//   - SHIPS (art::shipCell, 32 headings): the boats we own (moored ones furled; the one aboard split in two layers round
//     the hero, its sails drawing with the wind and braced with the helm), each harbour's own moored ships at its
//     moorings (ew::PortPlan::moorings) in the port's culture, pirates' black-sailed sloops (Sea::Live::ships);
//   - the WATER's life over the baked terrain: whitecaps on the open sea (thick in a storm), sun glints, the surf
//     breaking along every natural shore (the terrain's own shoreline field), wakes and the bow wave, oar strokes,
//     splashes, a telegraphed blow's ring, the fishing line and its float;
//   - the SKY over the sea: the Mist's drifting banks (thickening with Live::mist; colder and darker while it turns the
//     boat; a ring of fog round a lit centre when the chart parts it), storms (the darkened sky, slanting rain, lightning);
//   - the HUD: the sail panel (wind, heading, speed, point of sail, hull), the Mist's grip, the fishing panel (the bite
//     cue, the tension meter, the run) and the catch card.
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <SDL3/SDL.h>
#include "rpg/art.h"
#include "rpg/art/art_sea.h"
#include "rpg/culture/culture.h"
#include "rpg/sim/game.h"
#include "rpg/sim/sea.h"
#include "rpg/sim/sea_ops.h"
#include "rpg/view/view.h"
#include "rpg/world/source.h"

// terrain.cpp: the organic shoreline's field at a global pixel (0 dry .. 1 wet, -1 away from any shore)
float seaShoreField(const Map& m, int gox, int goy, int px, int py);

namespace {
constexpr float kTau = 6.2831853f;
inline float frand(uint32_t h) { return (float)(hash32(h) & 0xFFFFFF) / 16777216.0f; }
inline uint32_t hh(int a, int b, uint32_t s) { return hash32((uint32_t)a * 73856093u ^ (uint32_t)b * 19349663u ^ s * 83492791u); }
Color rgbaC(uint32_t c, float a = 1) { return Color((c & 255) / 255.0f, ((c >> 8) & 255) / 255.0f, ((c >> 16) & 255) / 255.0f, a); }
int fineDirOf(float heading) {
  int d = (int)std::lround(heading / (kTau / art::SHIP_FINE_DIRS));
  return ((d % art::SHIP_FINE_DIRS) + art::SHIP_FINE_DIRS) % art::SHIP_FINE_DIRS;
}
// the hull's half length / half beam in px (the wake, the bow wave, the foam: matches art_sea.cpp's forms)
float hullHalfLen(int hull) { return hull >= 2 ? 38.0f : hull == 1 ? 21.0f : 11.5f; }
float hullHalfBeam(int hull) { return hull >= 2 ? 11.5f : hull == 1 ? 7.0f : 4.3f; }
}  // namespace

struct View::SeaViewState {
  struct Cell { Tex t; float used = 0; };
  std::unordered_map<uint64_t, Cell> cells;   // ship cells by (style, hull, heading, frame, layer)
  SeaViewStats last, cur;
  // ---- the water's moving things (window px)
  struct Puff { Vec2 p, v; float t = 0, life = 1, size = 1; uint8_t kind = 0; };   // 0 foam, 1 churn, 2 ring, 3 oar swirl
  std::vector<Puff> puffs;
  struct Drop { Vec2 p, v; float z = 0, vz = 0, life = 1; uint32_t col = 0; };     // spray, splinters
  std::vector<Drop> drops;
  struct Tele { Vec2 p; float t = 0, dur = 1; };                                    // a blow's warning ring
  std::vector<Tele> teles;
  struct Bolt { Vec2 a, b; float t = 0; uint32_t seed = 0; };
  std::vector<Bolt> bolts;
  float flash = 0;                // lightning's white
  Vec2 lastWake;                  // the boat aboard's last wake spawn (window px)
  bool wakeInit = false;
  float oarT = 0;                 // the rowboat's stroke clock
  // ---- the moored ships of the harbours in the window
  struct Moored { int32_t gx = 0, gy = 0; int hull = 0; int dir = 0; art::ShipStyle st; };
  std::vector<Moored> moored;
  int32_t mooredOx = INT32_MIN, mooredOy = INT32_MIN;
  float mooredT = 0;
  // ---- the surf: the shoreline's points per global tile (computed once each)
  struct Surf { int16_t x, y; int8_t nx, ny; };   // pixel within the tile window, the way to the water (x16)
  std::unordered_map<uint64_t, std::vector<Surf>> surf;
  const Map* surfMap = nullptr;
  int32_t surfOx = INT32_MIN, surfOy = INT32_MIN;
  // ---- the sky
  Tex fog[2];                     // tileable fog banks (soft) and wisps
  Tex fogRing;                    // a fog ring with a clear heart (the parted Mist)
  bool baked = false;
  float mistShown = 0, turnShown = 0, partShown = 0, darkShown = 0;
  float partT = 0;                // seconds since the Mist parted
  // ---- the HUD
  float tensionShown = 0, distShown = 1, speedShown = 0;
  struct CatchCard { float t = 0; int species = -1; int cm = 0; int rarity = 0; std::string line; };
  CatchCard card;
  std::unordered_map<int, Tex> fishTex;
  float biteT = 0;                // the bite cue's flash
  float lostT = 0;
  bool heroUnderRig = false;      // this frame: the boat's rig stood over the hero
  float hudX0 = 0, hudY0 = 0, hudX1 = 0, hudY1 = 0;   // the sail panel's rect this frame (0 size: none)
  float firstContactT = 0;
  float contactHide = 0;          // eased 0..1: the first-contact title eases down under a big banner while one shows
  // ---- the sea's sound (engine/audio.cpp: the sea bed, the sea's moments)
  float roarCd = 0, rumbleCd = 0, chatterT = 2, bellCd = 0, thunderCd = 0;
  int lastBrace = 0;
  std::string firstContact;
  // ---- the swell under the water's veil
  Tex swell[2];
  float stormSea = 0, stormSeaProbe = 0;   // eased: the hero is out on the storm sea (ew::SeaKind::Storm)
};

inline float bayer4(int x, int y) {
  static const int m[4][4] = {{0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}};
  return (m[y & 3][x & 3] + 0.5f) / 16.0f;
}

// the sea draw's own time (SeaViewStats::ms: --perf and the scripts read it)
struct SeaTimer {
  double& acc;
  std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
  explicit SeaTimer(double& a) : acc(a) {}
  ~SeaTimer() { acc += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count(); }
};

struct SeaViewOps {
  static View::SeaViewState& st(View& v) {
    if (!v.seaView_) v.seaView_ = std::make_shared<View::SeaViewState>();
    return *v.seaView_;
  }
  static Pix& pix(View& v) { return *v.pix_; }
  static float now(const View& v) { return v.t_; }
  static void event(View& v, Game& g, const Event& e) { v.seaEvent(g, e); }
  static bool touch(const View& v) { return v.touchUI; }

  // ---- ship cells
  static const Tex& cell(View& v, int hull, const art::ShipStyle& s, int dir, int frame, int layer) {
    View::SeaViewState& S = st(v);
    uint64_t k = art::shipStyleKey(hull, s);
    k ^= (uint64_t)(dir & 63) * 0x9E3779B97F4A7C15ull ^ (uint64_t)(frame & 1) << 61 ^ (uint64_t)(layer & 3) << 58;
    auto it = S.cells.find(k);
    if (it != S.cells.end()) { it->second.used = v.t_; return it->second.t; }
    if (S.cells.size() > 220) {   // forget the cells not drawn for a while
      for (auto i = S.cells.begin(); i != S.cells.end();) {
        if (v.t_ - i->second.used > 8.0f) { v.pix_->destroy(i->second.t); i = S.cells.erase(i); }
        else ++i;
      }
    }
    View::SeaViewState::Cell c;
    c.t = v.pix_->bake(art::shipCell(hull, s, dir, frame, layer));
    c.used = v.t_;
    return S.cells.emplace(k, c).first->second.t;
  }
  static void drawCell(View& v, const Tex& t, int hull, Vec2 scr, float alpha = 1.0f, Color tint = Color(1, 1, 1, 1)) {
    const float x = std::floor(scr.x) - (float)art::shipAnchorX(hull), y = std::floor(scr.y) - (float)art::shipAnchorY(hull);
    v.pix_->blitEx(t, 0, 0, t.w, t.h, x, y, (float)t.w, (float)t.h, false, Color(tint.r, tint.g, tint.b, tint.a * alpha));
  }

  // ---- a culture's ships (the yard's culture: its archetype, its colours, its arms on the pennant)
  static art::ShipStyle styleOf(const cult::Culture* C, bool furled) {
    art::ShipStyle s;
    s.furled = furled;
    s.pennant = 1;
    if (!C) { s.sailCut = 0; return s; }
    s.culture = (uint8_t)((int)C->archetype + 1);
    s.sailCut = art::shipSailCutFor(s.culture);
    if (C->hidden) {   // a people beyond the Mist: the crossed forms (the rig of their second line), their own dyes
      s.culture2 = (uint8_t)((int)C->archetype2 + 1);
      s.sailCut = art::shipSailCutFor(s.culture2);
      if (C->dress.cloth[1]) s.sail = art::mix(C->dress.cloth[1] | 0xFF000000u, rgba(236, 228, 212), 0.35f);
      if (C->dress.cloth[2]) s.sail2 = C->dress.cloth[2] | 0xFF000000u;
    }
    if (!C->heraldry.empty()) {
      s.pennantA = C->heraldry.field | 0xFF000000u;
      s.pennantB = (C->heraldry.charge ? C->heraldry.charge : C->heraldry.field2) | 0xFF000000u;
      if (!C->hidden) s.sail2 = C->heraldry.field | 0xFF000000u;   // the device in the arms' colour
    }
    if (C->arch.accentTint) s.trim = C->arch.accentTint | 0xFF000000u;
    return s;
  }
  static const cult::Culture* cultureById(Game& g, uint64_t id) {
    if (!id || !g.world.src) return nullptr;
    return &g.world.src->atlas().get((cult::CultureId)id);
  }
  // the boat's screen position (window px) and heading: the one aboard is the hero
  static bool boatAt(const Game& g, int i, Vec2& p, float& heading) {
    if (i < 0 || i >= (int)g.sea.boats.size()) return false;
    const sea::Boat& b = g.sea.boats[(size_t)i];
    if (b.sunk) return false;
    if (i == g.sea.aboard) { p = g.pl().p; heading = g.sea.live.heading; return true; }
    p = Vec2((float)(b.gx - g.world.ox) * TILE + 8.0f, (float)(b.gy - g.world.oy) * TILE + 8.0f);
    heading = b.heading;
    return true;
  }
  // the bob: whole frames, each boat on its own clock (the hero aboard rides the same clock)
  static int bobFrame(const View& v, int i, float speed) { return (int)(v.t_ * (1.05f + std::min(0.6f, speed * 0.05f)) + i * 0.37f) & 1; }
  // (P2b VIEW-B) the same clock as a phase in [0, 2): (int)phase is bobFrame
  static float oarPhase(const View& v, int i, float speed) {
    return std::fmod(std::max(0.0f, v.t_ * (1.05f + std::min(0.6f, speed * 0.05f)) + i * 0.37f), 2.0f);
  }
  static art::ShipStyle aboardStyle(Game& g, const sea::Boat& b) {
    art::ShipStyle s = styleOf(cultureById(g, b.culture), false);
    const sea::Sea::Live& L = g.sea.live;
    s.brace = (int8_t)(L.helm > 0.3f ? 1 : L.helm < -0.3f ? -1 : 0);
    const float top = std::max(0.1f, sea::hullInfo(b.hull).speed);
    const float k = std::fabs(L.speed) / top;
    s.fill = (uint8_t)(k < 0.22f ? 0 : k < 0.72f ? 1 : 2);
    if (b.upgrades & sea::UP_LANTERN) s.extras |= art::SX_LANTERN;
    if (b.upgrades & sea::UP_FIGUREHEAD) s.extras |= art::SX_FIGUREHEAD;
    if (b.upgrades & sea::UP_PLATING) s.extras |= art::SX_PLATING;
    if (L.sinkT > 0) { s.extras |= art::SX_WRECKED; s.fill = 0; }   // going down: torn canvas, broken spars, settling
    return s;
  }
  static art::ShipStyle mooredStyle(Game& g, const sea::Boat& b) {
    art::ShipStyle s = styleOf(cultureById(g, b.culture), true);
    if (b.upgrades & sea::UP_LANTERN) s.extras |= art::SX_LANTERN;
    if (b.upgrades & sea::UP_FIGUREHEAD) s.extras |= art::SX_FIGUREHEAD;
    if (b.upgrades & sea::UP_PLATING) s.extras |= art::SX_PLATING;
    return s;
  }

  // ---- the harbours' own ships, refreshed with the window
  static void refreshMoored(View& v, Game& g) {
    View::SeaViewState& S = st(v);
    if (!g.world.src || g.inside) { S.moored.clear(); return; }
    if (S.mooredOx == g.world.ox && S.mooredOy == g.world.oy && v.t_ - S.mooredT < 30.0f) return;
    S.mooredOx = g.world.ox; S.mooredOy = g.world.oy; S.mooredT = v.t_;
    S.moored.clear();
    const Map& m = g.world.over;
    std::vector<ew::PortPlan> ports = g.world.src->portsIn(g.world.ox, g.world.oy, g.world.ox + m.w, g.world.oy + m.h);
    for (const ew::PortPlan& P : ports) {
      const cult::Culture* C = g.world.cultureAtTile(P.x - g.world.ox, P.y - g.world.oy);
      int n = 0;
      for (const ew::GTile& t : P.moorings) {
        const int tx = t.x - g.world.ox, ty = t.y - g.world.oy;
        if (!m.in(tx, ty) || !groundWater(m.at(tx, ty))) continue;
        const uint32_t h = hh(t.x, t.y, 0x5EA1);
        View::SeaViewState::Moored M;
        M.gx = t.x; M.gy = t.y;
        const bool deep = m.at(tx, ty) == Ground::DeepWater;
        const bool city = P.type == SiteType::City;
        M.hull = !P.harbour || !deep ? 0 : (city && (h % 7) == 0 && n > 0) ? 2 : (h % 3) ? 1 : 0;
        // lie alongside the shore: the heading runs across the way to the nearest land
        float lx = 0, ly = 0;
        for (int oy = -3; oy <= 3; oy++)
          for (int ox = -3; ox <= 3; ox++) {
            if (!ox && !oy) continue;
            const Ground q = m.at(tx + ox, ty + oy);
            if (groundWater(q)) continue;
            const float w = 1.0f / (float)(ox * ox + oy * oy);
            lx += ox * w; ly += oy * w;
          }
        const float a = (lx == 0 && ly == 0) ? (h & 1 ? 0.0f : 1.5708f) : std::atan2(ly, lx) + ((h & 2) ? 1.5708f : -1.5708f);
        M.dir = fineDirOf(a);
        M.st = styleOf(C, true);
        if (M.hull == 0) M.st.extras |= art::SX_NETS;
        S.moored.push_back(M);
        n++;
      }
    }
  }

  // ---- the shoreline's surf points of one global tile
  static const std::vector<View::SeaViewState::Surf>& surfOf(View& v, const Map& m, int gox, int goy, int gtx, int gty) {
    View::SeaViewState& S = st(v);
    const uint64_t key = (uint64_t)(uint32_t)gtx << 32 | (uint32_t)gty;
    auto it = S.surf.find(key);
    if (it != S.surf.end()) return it->second;
    std::vector<View::SeaViewState::Surf> pts;
    const int lx = gtx - gox, ly = gty - goy;
    bool mixed = false, anyWater = false;
    for (int oy = -1; oy <= 1; oy++)
      for (int ox = -1; ox <= 1; ox++) {
        const Ground q = m.at(lx + ox, ly + oy);
        const bool w = groundWater(q) || q == Ground::Bridge;
        if (groundWater(q)) anyWater = true;
        if (w != (groundWater(m.at(lx, ly)) || m.at(lx, ly) == Ground::Bridge)) mixed = true;
        if (m.urbAt(lx + ox, ly + oy)) { mixed = false; anyWater = false; oy = 2; break; }   // a quay keeps its own edge
      }
    if (mixed && anyWater && m.in(lx, ly)) {
      for (int py = 0; py < 16; py += 2)
        for (int px = 0; px < 16; px += 2) {
          const int gx = gtx * 16 + px, gy = gty * 16 + py;
          const float c = seaShoreField(m, gox, goy, gx, gy);
          if (c < 0) continue;
          const float r = seaShoreField(m, gox, goy, gx + 2, gy), d = seaShoreField(m, gox, goy, gx, gy + 2);
          if (r < 0 || d < 0) continue;
          // the contour v = 0.5 passes between this sample and a neighbour
          if ((c > 0.5f) == (r > 0.5f) && (c > 0.5f) == (d > 0.5f)) continue;
          float nx = r - c, ny = d - c;   // the gradient points to the water
          const float l = std::sqrt(nx * nx + ny * ny);
          if (l < 1e-4f) continue;
          nx /= l; ny /= l;
          View::SeaViewState::Surf s;
          s.x = (int16_t)px; s.y = (int16_t)py;
          s.nx = (int8_t)std::lround(nx * 16); s.ny = (int8_t)std::lround(ny * 16);
          pts.push_back(s);
        }
    }
    if (S.surf.size() > 6000) S.surf.clear();
    return S.surf.emplace(key, std::move(pts)).first->second;
  }

  // ---- the sky's fog textures
  static void bake(View& v) {
    View::SeaViewState& S = st(v);
    if (S.baked) return;
    S.baked = true;
    for (int k = 0; k < 2; k++) {
      const int N = 256;
      Canvas c(N, N);
      for (int y = 0; y < N; y++)
        for (int x = 0; x < N; x++) {
          // tileable noise: summed sines on the torus plus hashed lattice noise wrapped at N
          auto wn = [&](float fx, float fy, int cells, uint32_t seed) {
            const float gx = fx * cells / N, gy = fy * cells / N;
            const int ix = (int)std::floor(gx), iy = (int)std::floor(gy);
            const float ax = gx - ix, ay = gy - iy;
            const float sx = ax * ax * (3 - 2 * ax), sy = ay * ay * (3 - 2 * ay);
            auto L = [&](int i, int j) { return frand(hh(((i % cells) + cells) % cells, ((j % cells) + cells) % cells, seed)); };
            const float a = L(ix, iy), b = L(ix + 1, iy), cc = L(ix, iy + 1), d = L(ix + 1, iy + 1);
            return (a + (b - a) * sx) * (1 - sy) + (cc + (d - cc) * sx) * sy;
          };
          float n = k == 0 ? wn((float)x, (float)y, 4, 11) * 0.50f + wn((float)x, (float)y, 8, 12) * 0.28f + wn((float)x, (float)y, 16, 13) * 0.14f + wn((float)x, (float)y, 32, 14) * 0.08f
                           : wn((float)x, (float)y * 2.0f, 8, 21) * 0.55f + wn((float)x, (float)y, 16, 22) * 0.30f + wn((float)x, (float)y, 32, 23) * 0.15f;
          float a = k == 0 ? std::max(0.0f, (n - 0.38f) / 0.42f) : std::max(0.0f, (n - 0.52f) / 0.30f);
          a = std::min(1.0f, a);
          a = a * a * (3 - 2 * a);
          // a light top and a cooler belly: the banks have volume (lit from the top-left)
          const float lit = std::max(0.0f, std::min(1.0f, 0.5f + (wn((float)x - 4, (float)y - 4, 4, 11) - wn((float)x, (float)y, 4, 11)) * 3.0f));
          const int r = (int)(206 + 34 * lit), g = (int)(214 + 30 * lit), b = (int)(224 + 22 * lit);
          const float d = (bayer4(x, y) - 0.5f) * 0.10f;   // dithered alpha: soft edges without banding
          c.set(x, y, rgba(r, g, b, (int)(std::max(0.0f, std::min(1.0f, a + d)) * (k == 0 ? 210 : 150))));
        }
      S.fog[k] = v.pix_->bake(c);
    }
    {
      const int W = 320, H = 180;
      Canvas c(W, H);
      for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
          const float dx = (x - W * 0.5f + 0.5f) / (W * 0.5f), dy = (y - H * 0.5f + 0.5f) / (H * 0.5f);
          const float d = std::sqrt(dx * dx + dy * dy);
          const float n = (vnoise(x / 14.0f, y / 14.0f, 77) - 0.5f) * 0.22f + (vnoise(x / 5.0f, y / 5.0f, 78) - 0.5f) * 0.08f;
          float a = std::max(0.0f, std::min(1.0f, (d - 0.48f + n) / 0.55f));
          a = a * a * (3 - 2 * a);
          c.set(x, y, rgba(214, 222, 230, (int)(a * 235)));
        }
      S.fogRing = v.pix_->bake(c);
    }
  }

  // ---- a spray of droplets
  static void spray(View& v, Vec2 p, int n, float power, uint32_t col, uint32_t seed) {
    View::SeaViewState& S = st(v);
    for (int i = 0; i < n; i++) {
      View::SeaViewState::Drop d;
      const float a = frand(seed + i * 7) * kTau, sp = (10 + 30 * frand(seed + i * 13)) * power;
      d.p = p + Vec2(std::cos(a) * 2.0f, std::sin(a) * 1.2f);
      d.v = Vec2(std::cos(a) * sp, std::sin(a) * sp * 0.6f);
      d.z = 0; d.vz = (30 + 60 * frand(seed + i * 17)) * power;
      d.life = 0.6f + 0.6f * frand(seed + i * 19);
      d.col = col;
      S.drops.push_back(d);
    }
  }
  static void ring(View& v, Vec2 p, float size, float life, uint8_t kind = 2) {
    View::SeaViewState::Puff q;
    q.p = p; q.size = size; q.life = life; q.kind = kind;
    st(v).puffs.push_back(q);
  }
  // an ellipse ring of foam on the water (rx along x, ry along y)
  static void ellipseRing(Pix& P, Vec2 c, float rx, float ry, Color col, uint32_t seed = 0, float gaps = 0) {
    const int n = std::max(10, (int)(rx * 2.2f));
    for (int i = 0; i < n; i++) {
      if (gaps > 0 && frand(seed + i * 31) < gaps) continue;
      const float a = i * kTau / n;
      P.rect(std::floor(c.x + std::cos(a) * rx), std::floor(c.y + std::sin(a) * ry), 1, 1, col);
    }
  }
};

View::SeaViewStats View::seaViewStats() const { return seaView_ ? seaView_->last : SeaViewStats(); }

// ---------------------------------------------------------------- free functions render.cpp / hud.cpp call
float seaHeroLift(View& v, const Game& g) {
  if (!g.sea.atSea()) return 0;
  const sea::Boat& b = g.sea.boats[(size_t)g.sea.aboard];
  return (float)(art::shipDeckY((int)b.hull) - SeaViewOps::bobFrame(v, g.sea.aboard, std::fabs(g.sea.live.speed)));
}
// (P2b VIEW-B, the rower) the rowboat's stroke as a phase in [0, 2): the same clock as bobFrame, whose whole frames
// alternate the oars (art::shipCell frame 0: blades forward, the catch; frame 1: blades aft, the finish), so the Row
// posture's reach / catch / pull / recover keys change on the half-frames and the oars change between catch and pull
float seaOarPhase(View& v, const Game& g) {
  if (!g.sea.atSea()) return 0;
  return SeaViewOps::oarPhase(v, g.sea.aboard, std::fabs(g.sea.live.speed));
}
bool seaHeroUnderRig(View& v) { return SeaViewOps::st(v).heroUnderRig; }
void seaArtEvent(View& v, Game& g, const Event& e) { SeaViewOps::event(v, g, e); }
bool seaHudRect(View& v, float& x0, float& y0, float& x1, float& y1) {
  const View::SeaViewState& S = SeaViewOps::st(v);
  if (S.hudX1 <= S.hudX0) return false;
  x0 = S.hudX0; y0 = S.hudY0; x1 = S.hudX1; y1 = S.hudY1;
  return true;
}

// ---------------------------------------------------------------- the swell (under the terrain's translucent water)
// Two tileable sheets of crests and troughs, drifting against each other over the sea's deep colour: the water's veil
// (terrain.cpp) lies over them, so they read as the swell rolling under the surface everywhere there is water. Darker,
// greener and faster in a storm and out on the storm sea.
void seaDrawUnderlay(View& v, const Game& g, Vec2 cam) {
  View::SeaViewState& S = SeaViewOps::st(v);
  SeaTimer tm(S.cur.ms);
  Pix& P = SeaViewOps::pix(v);
  const float t = SeaViewOps::now(v);
  if (!S.swell[0].t) {
    for (int k = 0; k < 2; k++) {
      const int N = 128;
      Canvas c(N, N);
      // tileable lattice noise (wraps at N)
      auto wn = [&](float fx, float fy, int cells, uint32_t seed) {
        const float gx = fx * cells / N, gy = fy * cells / N;
        const int ix = (int)std::floor(gx), iy = (int)std::floor(gy);
        const float ax = gx - ix, ay = gy - iy;
        const float sx = ax * ax * (3 - 2 * ax), sy = ay * ay * (3 - 2 * ay);
        auto L = [&](int i, int j) { return frand(hh(((i % cells) + cells) % cells, ((j % cells) + cells) % cells, seed)); };
        const float a = L(ix, iy), b = L(ix + 1, iy), cc = L(ix, iy + 1), d = L(ix + 1, iy + 1);
        return (a + (b - a) * sx) * (1 - sy) + (cc + (d - cc) * sx) * sy;
      };
      // the swell: wavy bands across the sea (their rows bent by a slow warp), each crest a broken line of light with
      // its trough a shadow two px below; the finer sheet is the chop riding on it
      const float period = k == 0 ? 16.0f : 8.0f, warpA = k == 0 ? 14.0f : 6.0f;
      for (int y = 0; y < N; y++)
        for (int x = 0; x < N; x++) {
          const float warp = (wn((float)x, (float)y, 4, 0x5E11 + k) - 0.5f) * warpA + (wn((float)x, (float)y, 8, 0x5E21 + k) - 0.5f) * warpA * 0.35f;
          float band = std::fmod((float)y + warp + 256.0f, period);
          const float brk = wn((float)x, (float)y * 0.5f, k == 0 ? 16 : 32, 0x5E31 + k);   // where the crest breaks off
          if (band < 1.0f && brk > (k == 0 ? 0.42f : 0.55f)) {
            const float a = std::min(1.0f, (brk - 0.4f) * 3.0f);
            c.set(x, y, rgba(178, 220, 248, (int)((k == 0 ? 140 : 100) * a)));
          } else if (k == 0 && band >= 1.0f && band < 2.0f && brk > 0.5f) {
            c.set(x, y, rgba(110, 168, 220, (int)(60 * std::min(1.0f, (brk - 0.5f) * 4.0f))));
          } else if (band >= (k == 0 ? 3.0f : 2.0f) && band < (k == 0 ? 4.5f : 3.0f) && brk > 0.38f) {
            c.set(x, y, rgba(12, 34, 78, (int)((k == 0 ? 90 : 60) * std::min(1.0f, (brk - 0.38f) * 3.0f))));
          }
        }
      S.swell[k] = P.bake(c);
    }
  }
  // the storm sea under the hero (sampled now and then, eased)
  if (g.world.src && (S.stormSeaProbe -= 1.0f / 60.0f) <= 0) {
    S.stormSeaProbe = 0.5f;
    const int32_t gx = (int32_t)std::floor(g.pl().p.x / TILE) + g.world.ox, gy = (int32_t)std::floor(g.pl().p.y / TILE) + g.world.oy;
    const bool storm = g.world.src->seaAt(gx, gy) == ew::SeaKind::Storm;
    S.stormSea += ((storm ? 1.0f : 0.0f) - S.stormSea) * 0.15f;
  }
  const float storm = std::min(1.0f, std::max(S.darkShown, S.stormSea * 0.7f));
  const Color calm(0.16f, 0.33f, 0.57f), rough(0.10f, 0.22f, 0.32f);
  P.rect(0, 0, (float)Pix::W, (float)Pix::H, Color(calm.r + (rough.r - calm.r) * storm, calm.g + (rough.g - calm.g) * storm, calm.b + (rough.b - calm.b) * storm));
  const float N = 128.0f;
  for (int layer = 0; layer < 3; layer++) {
    const int k = layer == 1 ? 1 : 0;
    const float sp = 1.0f + storm * 1.8f;
    const float dx = layer == 0 ? t * 3.0f * sp + std::sin(t * 0.37f) * 5.0f : layer == 1 ? -t * 4.5f * sp : t * 1.6f * sp + 61.0f;
    const float dy = layer == 0 ? t * 1.1f * sp : layer == 1 ? std::sin(t * 0.5f) * 3.0f + 17.0f : -t * 0.8f * sp + 43.0f;
    float rx = std::fmod(cam.x - dx, N), ry = std::fmod(cam.y - dy, N);
    if (rx < 0) rx += N;
    if (ry < 0) ry += N;
    const float ox = -rx, oy = -ry;
    const float a = layer == 0 ? 0.80f + 0.20f * std::sin(t * 0.9f) : layer == 1 ? 0.65f : 0.45f + 0.45f * storm;
    for (float y = std::floor(oy); y < Pix::H; y += N)
      for (float x = std::floor(ox); x < Pix::W; x += N) P.blitEx(S.swell[k], 0, 0, 128, 128, x, y, N, N, layer == 2, Color(1, 1, 1, a));
  }
}

// ---------------------------------------------------------------- events and update
void View::seaEvent(Game& g, const Event& e) {
  SeaViewState& S = SeaViewOps::st(*this);
  const uint32_t seed = hash32((uint32_t)(t_ * 1000.0f) ^ (uint32_t)e.a * 977u);
  // the sea's moments in the ear (the sim plays its own splashes, roars and the cast: these sit with them)
  if (audio_ && !g.inside) {
    const float nearK = 1.0f / (1.0f + len(e.p - g.pl().p) / (10.0f * TILE));
    switch ((sea::SeaEv)e.a) {
      case sea::SeaEv::Board: audio_->play(g.sea.atSea() && g.sea.boats[(size_t)g.sea.aboard].hull != sea::Hull::Rowboat ? Sfx::SailFlap : Sfx::OarStroke, 1.0f, 0.7f); break;
      case sea::SeaEv::Moor: audio_->play(Sfx::ShipBell, 1.0f, 0.5f); break;
      case sea::SeaEv::HullHit: audio_->play(Sfx::HullCrack, 1.0f, std::min(1.0f, 0.5f + e.f * 0.05f)); break;
      case sea::SeaEv::Sink: audio_->play(Sfx::HullCrack, 0.8f, 1.0f); audio_->play(Sfx::WaveCrash, 0.7f, 1.0f); break;
      case sea::SeaEv::Telegraph: {   // a serpent rising: its roar (not every blow: once in a while)
        for (const Actor& a : g.actors)
          if (a.id == (int)e.id && a.mon == art::Monster::SeaSerpent && S.roarCd <= 0) { audio_->play(Sfx::SerpentRoar, 1.0f, 0.4f + 0.6f * nearK); S.roarCd = 5.0f; }
        break;
      }
      case sea::SeaEv::Tentacle: if (e.f > 0 && S.rumbleCd <= 0) { audio_->play(Sfx::KrakenRumble, 1.0f, 0.4f + 0.6f * nearK); S.rumbleCd = 2.5f; } break;
      case sea::SeaEv::Lightning: if (S.thunderCd <= 0) { audio_->play(Sfx::Thunder, 0.9f + 0.2f * frand(seed), 0.6f + 0.3f * std::min(1.0f, e.f)); S.thunderCd = 1.5f; } break;
      case sea::SeaEv::StormBegin: audio_->play(Sfx::Thunder, 0.75f, 0.35f); break;
      case sea::SeaEv::Reel: audio_->play(Sfx::ReelClick, 0.9f + 0.6f * std::min(1.0f, e.f), 0.5f); break;
      case sea::SeaEv::Bite: audio_->play(Sfx::FishPlop, 1.0f, 0.8f); break;
      case sea::SeaEv::Lost: if (e.f > 0.5f) audio_->play(Sfx::LineSnap, 1.0f, 0.8f); break;
      case sea::SeaEv::MistEnter: audio_->play(Sfx::MistWhisper, 1.0f, 0.6f); break;
      case sea::SeaEv::MistTurn: audio_->play(Sfx::MistWhisper, 0.85f, 0.9f); break;
      case sea::SeaEv::MistPart: case sea::SeaEv::FirstContact: audio_->play(Sfx::MistChime, 1.0f, 0.9f); break;
      default: break;
    }
  }
  switch ((sea::SeaEv)e.a) {
    case sea::SeaEv::Board: case sea::SeaEv::Disembark:
      SeaViewOps::ring(*this, e.p, 6, 0.9f);
      SeaViewOps::spray(*this, e.p, 6, 0.5f, rgba(230, 244, 250), seed);
      S.wakeInit = false;
      break;
    case sea::SeaEv::HullHit:
      SeaViewOps::spray(*this, e.p, 10, 0.8f, rgba(132, 92, 56), seed);
      SeaViewOps::spray(*this, e.p, 8, 0.9f, rgba(230, 244, 250), seed + 5);
      SeaViewOps::ring(*this, e.p, 8, 0.8f);
      shake_ = std::max(shake_, 0.18f);
      break;
    case sea::SeaEv::Sink:
      for (int k = 0; k < 4; k++) SeaViewOps::ring(*this, e.p, 10.0f + k * 6, 1.6f + k * 0.4f);
      SeaViewOps::spray(*this, e.p, 24, 1.2f, rgba(230, 244, 250), seed);
      SeaViewOps::spray(*this, e.p, 12, 0.9f, rgba(120, 84, 52), seed + 3);
      break;
    case sea::SeaEv::Splash: case sea::SeaEv::Tentacle: {
      const float f = e.type == Ev::Sea && (sea::SeaEv)e.a == sea::SeaEv::Tentacle ? (e.f > 0 ? 1.0f : 0.6f) : std::max(0.2f, e.f);
      SeaViewOps::ring(*this, e.p, 4 + 10 * f, 0.7f + 0.6f * f);
      SeaViewOps::spray(*this, e.p, (int)(4 + 18 * f), 0.4f + 0.8f * f, rgba(230, 244, 250), seed);
      break;
    }
    case sea::SeaEv::Telegraph: {
      SeaViewState::Tele t;
      t.p = e.p; t.dur = std::max(0.2f, e.f);
      S.teles.push_back(t);
      break;
    }
    case sea::SeaEv::Lightning: {
      SeaViewState::Bolt b;
      b.a = e.q; b.b = e.p; b.seed = seed;
      S.bolts.push_back(b);
      S.flash = 1.0f;
      break;
    }
    case sea::SeaEv::MistPart:
      S.partT = 0.001f;
      break;
    case sea::SeaEv::Cast:
      SeaViewOps::ring(*this, e.p, 3, 0.6f);
      break;
    case sea::SeaEv::Bite:
      S.biteT = 1.0f;
      SeaViewOps::spray(*this, e.p, 5, 0.35f, rgba(230, 244, 250), seed);
      SeaViewOps::ring(*this, e.p, 4, 0.5f);
      break;
    case sea::SeaEv::Strike:
      SeaViewOps::spray(*this, e.p, 8, 0.5f, rgba(230, 244, 250), seed);
      break;
    case sea::SeaEv::Landed:
      S.card.t = 4.0f;
      S.card.species = (int)e.id;
      S.card.rarity = (int)e.f;
      S.card.line = e.s;
      S.card.cm = g.sea.live.line.cm;
      SeaViewOps::spray(*this, e.p, 10, 0.6f, rgba(230, 244, 250), seed);
      break;
    case sea::SeaEv::Lost:
      S.lostT = 1.2f;
      break;
    case sea::SeaEv::FirstContact:
      S.firstContactT = 7.0f;
      S.firstContact = e.s;
      break;
    default: break;
  }
}

void View::seaUpdate(Game& g, float dt) {
  SeaViewState& S = SeaViewOps::st(*this);
  S.last = S.cur;
  S.cur = SeaViewStats();
  for (auto& q : S.puffs) { q.t += dt; q.p += q.v * dt; q.v = q.v * (1.0f - std::min(1.0f, dt * 0.8f)); }
  S.puffs.erase(std::remove_if(S.puffs.begin(), S.puffs.end(), [](const SeaViewState::Puff& q) { return q.t >= q.life; }), S.puffs.end());
  for (auto& d : S.drops) { d.p += d.v * dt; d.z += d.vz * dt; d.vz -= 220.0f * dt; d.life -= dt; }
  S.drops.erase(std::remove_if(S.drops.begin(), S.drops.end(), [](const SeaViewState::Drop& d) { return d.life <= 0 || d.z < -1; }), S.drops.end());
  for (auto& t : S.teles) t.t += dt;
  S.teles.erase(std::remove_if(S.teles.begin(), S.teles.end(), [](const SeaViewState::Tele& t) { return t.t >= t.dur + 0.3f; }), S.teles.end());
  for (auto& b : S.bolts) b.t += dt;
  S.bolts.erase(std::remove_if(S.bolts.begin(), S.bolts.end(), [](const SeaViewState::Bolt& b) { return b.t > 0.35f; }), S.bolts.end());
  S.flash = std::max(0.0f, S.flash - dt * 3.2f);
  S.biteT = std::max(0.0f, S.biteT - dt * 1.4f);
  S.lostT = std::max(0.0f, S.lostT - dt);
  S.card.t = std::max(0.0f, S.card.t - dt);
  // (fixer M10 r1) the title plays AT the landing, never queued behind LEVEL UP / NEW QUEST (it landed 10 s late, in a
  // town or an inn): while a big banner shows it eases down under it (contactHide: 0 its own place, 1 under the
  // banner); walked indoors it fades out early
  S.contactHide += ((bannerT_ > 0 ? 1.0f : 0.0f) - S.contactHide) * std::min(1.0f, dt * 5.0f);
  if (g.inside && S.firstContactT > 0.6f) S.firstContactT = 0.6f;
  S.firstContactT = std::max(0.0f, S.firstContactT - dt);
  if (S.puffs.size() > 900) S.puffs.erase(S.puffs.begin(), S.puffs.begin() + (S.puffs.size() - 900));
  if (S.drops.size() > 600) S.drops.erase(S.drops.begin(), S.drops.begin() + (S.drops.size() - 600));
  // the sky's eased values
  const sea::Sea::Live& L = g.sea.live;
  const float k = std::min(1.0f, dt * 1.6f);
  S.mistShown += (L.mist - S.mistShown) * k;
  S.turnShown += ((L.turningBack ? 1.0f : 0.0f) - S.turnShown) * std::min(1.0f, dt * 1.2f);
  S.darkShown += (L.stormDark - S.darkShown) * std::min(1.0f, dt * 0.8f);
  const bool parted = L.mistLand && g.sea.hasChart(L.mistLand);
  S.partShown += ((parted ? 1.0f : 0.0f) - S.partShown) * std::min(1.0f, dt * 0.5f);
  if (S.partT > 0) S.partT += dt;
  if (S.partT > 8.0f) S.partT = 0;
  // the HUD's eased values
  S.tensionShown += (L.line.tension - S.tensionShown) * std::min(1.0f, dt * 9.0f);
  S.distShown += (L.line.dist - S.distShown) * std::min(1.0f, dt * 6.0f);
  S.speedShown += (std::fabs(L.speed) - S.speedShown) * std::min(1.0f, dt * 3.0f);
  // the boat aboard leaves a wake: a pair of foam puffs off its quarters every few px of way, a churn astern
  if (g.sea.atSea() && !g.inside) {
    const sea::Boat& b = g.sea.boats[(size_t)g.sea.aboard];
    const int hull = (int)b.hull;
    const Vec2 p = g.pl().p;
    const Vec2 F(std::cos(L.heading), std::sin(L.heading)), R(-F.y, F.x);
    if (!S.wakeInit) { S.lastWake = p; S.wakeInit = true; }
    const float hl = hullHalfLen(hull), hb = hullHalfBeam(hull);
    const float step = hull >= 2 ? 4.0f : 3.0f;
    if (len(p - S.lastWake) > step) {
      const float sp = std::fabs(L.speed);
      S.lastWake = p;
      const uint32_t sd = hash32((uint32_t)(t_ * 997.0f));
      for (int side = -1; side <= 1; side += 2) {
        SeaViewState::Puff q;
        const float j = frand(sd + side * 7) - 0.5f;
        q.p = p - F * (hl * (0.55f + 0.2f * frand(sd + side))) + R * (side * (hb * 0.85f + j));
        q.v = R * (side * (7.0f + sp * 2.2f) * (0.8f + 0.4f * frand(sd + side * 3))) - F * (1.0f + sp * 0.4f);
        q.life = 1.6f + 0.4f * frand(sd + side * 5) + 0.2f * sp / 4.0f;
        q.size = (hull >= 2 ? 2.2f : 1.5f) * (0.8f + 0.5f * frand(sd + side * 11));
        q.kind = 0;
        S.puffs.push_back(q);
      }
      SeaViewState::Puff c;
      c.p = p - F * (hl * 0.95f);
      c.v = F * -3.0f;
      c.life = 1.4f;
      c.size = hb * 0.5f;
      c.kind = 1;
      S.puffs.push_back(c);
    }
    // a rowboat's oars stir the water at each stroke
    if (hull == 0 && std::fabs(L.speed) > 0.6f) {
      S.oarT += dt * (1.05f + std::min(0.6f, std::fabs(L.speed) * 0.05f));
      if (S.oarT >= 1.0f) {
        S.oarT -= 1.0f;
        for (int side = -1; side <= 1; side += 2) SeaViewOps::ring(*this, p + R * (side * 13.5f) - F * 3.0f, 2.5f, 0.8f, 3);
      }
    }
  } else S.wakeInit = false;
  // pirates' wakes
  for (const sea::Sea::Ship& sh : L.ships) {
    if (sh.speed < 1.0f || frand(hash32((uint32_t)(t_ * 30)) ^ sh.id) > 0.45f) continue;
    const Vec2 F(std::cos(sh.heading), std::sin(sh.heading)), R(-F.y, F.x);
    for (int side = -1; side <= 1; side += 2) {
      SeaViewState::Puff q;
      q.p = sh.p - F * 16.0f + R * (side * 5.0f);
      q.v = R * (side * 6.0f);
      q.life = 1.8f;
      q.size = 1.4f;
      S.puffs.push_back(q);
    }
  }
  S.cur.wakes = (int)S.puffs.size();
  // ---- the sea's bed: the swell under the hull, the shore's surf, gulls by a harbour, the wind in the sails, the
  //      ship's timbers, the oars, the storm, the Mist
  S.roarCd -= dt; S.rumbleCd -= dt; S.bellCd -= dt; S.thunderCd -= dt;
  if (audio_) {
    Audio::SeaSound snd;
    if (!g.inside && g.mode != Mode::Title && g.mode != Mode::Dead) {
      const Map& m = g.world.over;
      const int hx = (int)std::floor(g.pl().p.x / TILE), hy = (int)std::floor(g.pl().p.y / TILE);
      int land = 0, wet = 0;
      for (int oy = -5; oy <= 5; oy += 2)
        for (int ox = -5; ox <= 5; ox += 2) {
          if (!m.in(hx + ox, hy + oy)) continue;
          if (groundWater(m.at(hx + ox, hy + oy))) wet++; else land++;
        }
      const float landK = land + wet > 0 ? (float)land / (float)(land + wet) : 0.0f;
      const bool atSea = g.sea.atSea();
      const float coast = wet > 0 && land > 0 ? std::min(1.0f, 4.0f * std::min(landK, 1.0f - landK)) : 0.0f;
      bool harbour = false;
      for (const auto& M : S.moored)
        if (std::abs(M.gx - (hx + g.world.ox)) < 24 && std::abs(M.gy - (hy + g.world.oy)) < 18) { harbour = true; break; }
      snd.surf = coast * (atSea ? 0.9f : 0.5f);
      snd.gulls = harbour ? 1.0f : coast * 0.35f;
      if (atSea || L.swimming) snd.open = 0.55f + 0.45f * std::min(1.0f, std::fabs(L.speed) / 6.0f);
      if (atSea) {
        const sea::Boat& b = g.sea.boats[(size_t)g.sea.aboard];
        const int hull = (int)b.hull;
        const float top = std::max(0.1f, sea::hullInfo(b.hull).speed);
        snd.creak = hull == 0 ? 0.25f : hull == 1 ? 0.6f : 1.0f;
        if (hull > 0) snd.wind = std::min(1.0f, 0.25f + 0.75f * std::fabs(L.speed) / top) * (0.4f + 0.6f * L.wind.speed);
        if (hull == 0 && std::fabs(L.speed) > 0.6f) snd.oars = 1.05f + std::min(0.6f, std::fabs(L.speed) * 0.05f);
        const int brace = L.helm > 0.3f ? 1 : L.helm < -0.3f ? -1 : 0;   // the yards swinging over: the canvas cracks
        if (hull > 0 && brace != S.lastBrace) audio_->play(Sfx::SailFlap, 1.0f, 0.45f);
        S.lastBrace = brace;
      }
      snd.storm = S.darkShown;
      snd.mist = S.mistShown;
      snd.turn = S.turnShown;
      // merrow calling to each other round the boat
      if ((S.chatterT -= dt) <= 0) {
        S.chatterT = 2.5f + 3.0f * frand(hash32((uint32_t)(t_ * 100)));
        for (const Actor& a : g.actors)
          if (a.mon == art::Monster::Merrow && a.st != AState::Dead && len2(a.p - g.pl().p) < (12.0f * TILE) * (12.0f * TILE)) {
            audio_->play(Sfx::MerrowChatter, 0.9f + 0.25f * frand(a.id * 977u), 0.5f);
            break;
          }
      }
      // a ship's bell now and then in a harbour
      if (harbour && S.bellCd <= 0) { S.bellCd = 25.0f + 20.0f * frand(hash32((uint32_t)(t_ * 10))); audio_->play(Sfx::ShipBell, 0.95f + 0.1f * frand(hash32((uint32_t)t_)), 0.25f); }
    }
    audio_->setSea(snd);
  }
  SeaViewOps::refreshMoored(*this, g);
}

void View::seaShift(Vec2 d) {
  if (!seaView_) return;
  SeaViewState& S = *seaView_;
  for (auto& q : S.puffs) q.p += d;
  for (auto& q : S.drops) q.p += d;
  for (auto& t : S.teles) t.p += d;
  for (auto& b : S.bolts) { b.a += d; b.b += d; }
  S.lastWake += d;
}

// ---------------------------------------------------------------- the water's life (over the terrain, under the bodies)
void View::drawSeaWater(Game& g, Vec2 cam) {
  if (g.inside) return;
  SeaViewState& S = SeaViewOps::st(*this);
  SeaTimer tm(S.cur.ms);
  Pix& P = *pix_;
  const Map& m = g.world.over;
  const sea::Sea::Live& L = g.sea.live;
  const float day = g.daylight();
  const float storm = std::max(S.darkShown, L.storm.on ? L.storm.power : 0.0f);
  const int tx0 = (int)std::floor(cam.x / 16) - 1, ty0 = (int)std::floor(cam.y / 16) - 1;
  const int tx1 = (int)std::floor((cam.x + Pix::W) / 16) + 1, ty1 = (int)std::floor((cam.y + Pix::H) / 16) + 1;
  // ---- whitecaps and glints on the open water (world-anchored cells, each on its own clock)
  {
    const int cs = 20;
    const int cx0 = (int)std::floor(cam.x / cs) - 1, cy0 = (int)std::floor(cam.y / cs) - 1;
    const int cx1 = (int)std::floor((cam.x + Pix::W) / cs) + 1, cy1 = (int)std::floor((cam.y + Pix::H) / cs) + 1;
    const int32_t gx0 = g.world.ox * 16, gy0 = g.world.oy * 16;   // anchor to the world, not the window
    const float capP = 0.10f + 0.55f * storm;
    const Color foamC(0.90f, 0.96f, 1.0f, 0.85f), foamD(0.70f, 0.84f, 0.92f, 0.6f);
    const float windX = std::cos(L.wind.dir), windY = std::sin(L.wind.dir);
    for (int cy = cy0; cy <= cy1; cy++)
      for (int cx = cx0; cx <= cx1; cx++) {
        const int wcx = (int)std::floor((float)(cx * cs + gx0) / cs), wcy = (int)std::floor((float)(cy * cs + gy0) / cs);
        const uint32_t h = hh(wcx, wcy, 0xCA95);
        const float px = cx * cs + (h & 15) + 2.0f, py = cy * cs + ((h >> 4) & 15) + 2.0f;
        const int tx = (int)std::floor(px / 16), ty = (int)std::floor(py / 16);
        if (!m.in(tx, ty)) continue;
        const Ground q = m.at(tx, ty);
        if (!groundWater(q)) continue;
        // keep off the shore (the surf owns it)
        bool open = true;
        for (int k = 0; k < 4 && open; k++) open = groundWater(m.at(tx + (k == 0) - (k == 1), ty + (k == 2) - (k == 3)));
        if (!open) continue;
        const float period = 3.2f - 1.2f * storm + frand(h ^ 0x77) * 1.6f;
        const float ph = std::fmod(t_ / period + frand(h ^ 0x99), 1.0f);
        const bool deep = q == Ground::DeepWater;
        if (frand(h ^ 0x31) < (deep ? capP : capP * 0.4f) && ph < 0.42f) {
          // a crest curling over and breaking into foam, drifting with the wind
          // a crest curling over (a little arch of white), then breaking into foam that trails downwind and thins out
          const float a = ph / 0.42f, len0 = (deep ? 4.0f : 2.5f) + 4.0f * storm;
          const float x = std::floor(px + windX * a * 3.0f), y = std::floor(py + windY * a * 2.0f);
          const int w = (int)std::max(1.0f, len0 * std::min(1.0f, a * 2.5f));
          const Color c = a < 0.6f ? foamC : foamD;
          const float fade = 1.0f - a * a;
          for (int i = 0; i < w; i++) {
            const float u = w > 1 ? (float)i / (w - 1) : 0.5f;
            const float lift = (u > 0.2f && u < 0.8f && w > 3 && a < 0.7f) ? 1.0f : 0.0f;
            P.rect(x - w / 2 + i, y - lift, 1, 1, Color(c.r, c.g, c.b, c.a * fade * (0.6f + 0.4f * std::sin(u * 3.14159f))));
          }
          if (w > 2) P.rect(x - w / 2 + 1, y + 1, (float)(w - 2), 1, Color(0.30f, 0.50f, 0.70f, 0.30f * fade));   // its shadow side
          const int fl = (int)(a * (3 + 4 * storm));
          for (int i = 0; i < fl; i++) {   // the foam left behind
            const float fx = x - windX * (2.0f + 5.0f * frand(h + i * 7)) + (frand(h + i * 11) - 0.5f) * w;
            const float fy = y - windY * (2.0f + 3.0f * frand(h + i * 13)) + 1.0f + frand(h + i * 17) * 1.5f;
            P.rect(std::floor(fx), std::floor(fy), 1, 1, Color(0.85f, 0.93f, 1.0f, 0.55f * fade));
          }
          if (storm > 0.5f && (h & 3) == 0) {   // spindrift streaking off it on the gale
            for (int i = 0; i < 6; i++) P.rect(std::floor(x + windX * (i * 2.0f + a * 6.0f)), std::floor(y + windY * (i * 1.3f) - 1), 1, 1, Color(0.9f, 0.96f, 1.0f, 0.4f * (1.0f - i / 6.0f) * fade));
          }
          S.cur.waveTiles++;
        } else if (day > 0.35f && frand(h ^ 0x52) < 0.22f * (1.0f - storm) && std::fmod(t_ * 1.7f + frand(h ^ 0x13) * 9.0f, 2.3f) < 0.12f) {
          // a glint of the sun
          const float k2 = (day - 0.35f) / 0.65f;
          P.rect(std::floor(px), std::floor(py), 1, 1, Color(1.0f, 0.98f, 0.86f, 0.9f * k2));
          P.rect(std::floor(px) - 1, std::floor(py), 3, 1, Color(1.0f, 0.98f, 0.86f, 0.35f * k2));
          S.cur.waveTiles++;
        }
      }
  }
  // ---- the surf along every natural shore: lines of foam that run up the beach and draw back, a second line behind
  {
    int gox = g.world.ox, goy = g.world.oy;
    if (S.surfMap != &m || S.surfOx != gox || S.surfOy != goy) { S.surfMap = &m; S.surfOx = gox; S.surfOy = goy; }
    const float swell = 1.0f + storm * 1.5f;
    int budget = 900;
    for (int ty = ty0; ty <= ty1 && budget > 0; ty++)
      for (int tx = tx0; tx <= tx1 && budget > 0; tx++) {
        if (!m.in(tx, ty)) continue;
        const auto& pts = SeaViewOps::surfOf(*this, m, gox, goy, tx + gox, ty + goy);
        for (const auto& s : pts) {
          const float nx = s.nx / 16.0f, ny = s.ny / 16.0f;
          const float gx = (float)((tx + gox) * 16 + s.x), gy = (float)((ty + goy) * 16 + s.y);
          // the phase runs along the coast (world-anchored) so the waves arrive in sets
          const float ph = t_ * 0.55f - (gx * 0.021f + gy * 0.017f) + std::sin(gx * 0.05f) * 0.3f;
          const float wv = 0.5f + 0.5f * std::sin(ph * kTau);
          const float x = (float)(tx * 16 + s.x), y = (float)(ty * 16 + s.y);
          // the wash: foam at the waterline, pushed a little up the beach at the crest
          const float d1 = (0.6f + 2.6f * wv) * swell;
          const float a1 = 0.55f + 0.4f * wv;
          P.rect(std::floor(x + nx * (1.0f - d1 * 0.6f)), std::floor(y + ny * (1.0f - d1 * 0.6f)), 2, 1, Color(0.93f, 0.97f, 1.0f, a1));
          // the breaker behind it, out on the water, rolling in
          const float ph2 = std::fmod(ph + 0.35f, 1.0f);
          const float d2 = (2.0f + 7.0f * (1.0f - ph2)) * swell;
          const float a2 = (ph2 < 0.75f ? ph2 / 0.75f : (1.0f - ph2) / 0.25f) * 0.55f;
          if (a2 > 0.05f && ((s.x + s.y) & 2)) P.rect(std::floor(x + nx * d2), std::floor(y + ny * d2), 2, 1, Color(0.86f, 0.94f, 1.0f, a2));
          budget--;
          S.cur.foamTiles++;
        }
      }
  }
  // ---- wakes, churn, rings
  for (const auto& q : S.puffs) {
    const float a = 1.0f - q.t / q.life;
    const float x = std::floor(q.p.x - cam.x), y = std::floor(q.p.y - cam.y);
    if (x < -20 || y < -20 || x > Pix::W + 20 || y > Pix::H + 20) continue;
    switch (q.kind) {
      case 0: {   // foam off the quarters, spreading into a V and breaking up into flecks as it ages
        const float s = q.size * (1.0f + q.t * 0.9f);
        const uint32_t hs = (uint32_t)(q.life * 1e4f);
        const int n = q.t < 0.4f ? 4 : q.t < 1.0f ? 3 : 2;
        for (int i = 0; i < n; i++) {
          const float ox = (frand(hs + i * 13) - 0.5f) * s * 2.0f, oy = (frand(hs + i * 29) - 0.5f) * s;
          const float fa = a * a * (i == 0 ? 0.9f : 0.6f);
          P.rect(std::floor(x + ox), std::floor(y + oy), (i == 0 && q.t < 0.6f) ? 2.0f : 1.0f, 1, Color(0.93f, 0.98f, 1.0f, fa));
        }
        if (q.t < 1.2f) P.rect(std::floor(x - s * 0.5f), y + 1, std::max(1.0f, s), 1, Color(0.62f, 0.82f, 0.94f, 0.30f * a));
        break;
      }
      case 1: {   // the churned water astern: pale, foamy, quickly gone
        const float s = q.size * (1.0f + q.t * 0.8f);
        P.rect(std::floor(x - s), y - 1, std::floor(s * 2), 3, Color(0.58f, 0.78f, 0.90f, 0.18f * a));
        const uint32_t hs = (uint32_t)(q.p.x * 31 + q.p.y * 17);
        for (int i = 0; i < 3; i++)
          if (frand(hs + i) < a) P.rect(std::floor(x + (frand(hs + i * 7) - 0.5f) * s * 2), std::floor(y + (frand(hs + i * 11) - 0.5f) * 2), 1, 1, Color(0.95f, 0.98f, 1.0f, 0.7f * a));
        break;
      }
      case 2: case 3: {   // a ring spreading on the water
        const float r = q.size * (0.4f + q.t / q.life * (q.kind == 3 ? 1.2f : 1.6f));
        SeaViewOps::ellipseRing(P, Vec2(x, y), r, r * 0.55f, Color(0.92f, 0.97f, 1.0f, 0.7f * a), (uint32_t)(q.p.x * 7), 0.15f);
        if (q.kind == 2 && r > 4) SeaViewOps::ellipseRing(P, Vec2(x, y), r * 0.6f, r * 0.33f, Color(0.72f, 0.86f, 0.94f, 0.4f * a), (uint32_t)(q.p.y * 5), 0.3f);
        break;
      }
      default: break;
    }
  }
  // ---- the bow wave of every boat under way
  auto bowWave = [&](Vec2 p, float heading, int hull, float speed) {
    if (speed < 0.8f) return;
    const Vec2 F(std::cos(heading), std::sin(heading)), R(-F.y, F.x);
    const float hl = hullHalfLen(hull), hb = hullHalfBeam(hull);
    const float k = std::min(1.0f, speed / 6.0f);
    for (int i = 0; i < 14; i++) {
      const float t = 0.55f + 0.45f * i / 13.0f;
      const float w = hb * std::sqrt(std::max(0.0f, 1.0f - t * t * t)) + 1.2f + (1.0f - t) * 2.0f * k;
      for (int side = -1; side <= 1; side += 2) {
        if (frand(hh(i, side, (uint32_t)(t_ * 12))) < 0.25f) continue;
        const Vec2 q = p + F * (t * hl + 1.5f) + R * (side * w);
        P.rect(std::floor(q.x - cam.x), std::floor(q.y - cam.y), 1, 1, Color(0.94f, 0.98f, 1.0f, 0.8f * k));
      }
    }
    const Vec2 tip = p + F * (hl + 2.5f);
    P.rect(std::floor(tip.x - cam.x) - 1, std::floor(tip.y - cam.y), 3, 1, Color(0.94f, 0.98f, 1.0f, 0.7f * k));
  };
  if (g.sea.atSea()) bowWave(g.pl().p, L.heading, (int)g.sea.boats[(size_t)g.sea.aboard].hull, std::fabs(L.speed));
  for (const sea::Sea::Ship& sh : L.ships) bowWave(sh.p, sh.heading, (int)sh.hull, sh.speed / 16.0f);
  // ---- a blow's warning: a ring on the water closing in on where it lands
  for (const auto& t : S.teles) {
    const float k = std::min(1.0f, t.t / t.dur);
    const float r = 4.0f + 18.0f * (1.0f - k);
    const float pulse = 0.55f + 0.45f * std::sin(t_ * 18.0f);
    const Vec2 c(t.p.x - cam.x, t.p.y - cam.y);
    SeaViewOps::ellipseRing(P, c, r, r * 0.55f, Color(1.0f, 0.42f, 0.30f, (0.45f + 0.4f * k) * pulse));
    SeaViewOps::ellipseRing(P, c, 4.0f + 4.0f * k, (4.0f + 4.0f * k) * 0.55f, Color(0.10f, 0.14f, 0.22f, 0.35f * k));   // the shadow rising from below
    if (t.t > t.dur) SeaViewOps::ellipseRing(P, c, 10.0f, 5.5f, Color(1, 1, 1, 0.6f * (1.0f - (t.t - t.dur) / 0.3f)));
  }
  // ---- the fishing line and its float
  const sea::Sea::Line& ln = L.line;
  if (ln.phase != sea::Sea::Cast::None && ln.phase != sea::Sea::Cast::Landed) {
    const Actor& pl = g.pl();
    const float lift = seaHeroLift(*this, g);
    const float side = pl.face == 3 ? -1.0f : pl.face == 2 ? 1.0f : (ln.bob.x < pl.p.x ? -1.0f : 1.0f);
    const Vec2 tip = pl.p + Vec2(side * 9.0f, -20.0f - lift);   // the rod's tip over the hero's hand
    const bool taut = ln.phase == sea::Sea::Cast::Reeling || ln.phase == sea::Sea::Cast::Bite;
    Vec2 bob = ln.bob;
    if (ln.phase == sea::Sea::Cast::Casting) bob = tip + (ln.bob - tip) * std::min(1.0f, ln.t / 0.45f);
    // the rod
    for (int i = 0; i < 9; i++) P.rect(std::floor(pl.p.x + side * (3.0f + i * 0.7f) - cam.x), std::floor(pl.p.y - 12.0f - lift - i * 0.95f - cam.y), 1, 1, Color(0.42f, 0.28f, 0.18f, 1));
    // the line: a sagging curve, straight when taut
    const int n = 26;
    const float sag = taut ? 1.0f + 6.0f * (1.0f - S.tensionShown) : 14.0f;
    Vec2 prev = tip;
    for (int i = 1; i <= n; i++) {
      const float t = (float)i / n;
      Vec2 q = tip + (bob - tip) * t;
      q.y += std::sin(t * 3.14159f) * sag;
      if (ln.phase == sea::Sea::Cast::Casting) q.y -= std::sin(t * 3.14159f) * 22.0f * (1.0f - std::min(1.0f, ln.t / 0.45f));
      const Vec2 d = q - prev;
      const int steps = std::max(1, (int)std::max(std::fabs(d.x), std::fabs(d.y)));
      for (int s = 0; s < steps; s++) {
        const Vec2 r = prev + d * ((float)s / steps);
        P.rect(std::floor(r.x - cam.x), std::floor(r.y - cam.y), 1, 1, Color(0.90f, 0.92f, 0.94f, taut ? 0.85f : 0.6f));
      }
      prev = q;
    }
    // the float: bobbing, ripples round it; under on a bite; a running wake when a fish is on
    const float bx = std::floor(bob.x - cam.x), by = std::floor(bob.y - cam.y);
    if (ln.phase == sea::Sea::Cast::Waiting) {
      const float b2 = std::sin(t_ * 3.0f) > 0.6f ? 1.0f : 0.0f;
      if (std::fmod(t_, 1.6f) < 0.02f) SeaViewOps::ring(*this, ln.bob, 3.0f, 1.0f, 3);
      P.rect(bx - 1, by - 3 + b2, 3, 2, Color(0.92f, 0.24f, 0.20f, 1));
      P.rect(bx - 1, by - 1 + b2, 3, 1, Color(0.96f, 0.94f, 0.88f, 1));
      P.rect(bx, by - 4 + b2, 1, 1, Color(0.2f, 0.16f, 0.14f, 1));
      P.rect(bx - 2, by + b2, 5, 1, Color(0.80f, 0.92f, 1.0f, 0.45f));
    } else if (ln.phase == sea::Sea::Cast::Bite || ln.phase == sea::Sea::Cast::Reeling) {
      const float r = 2.0f + std::sin(t_ * 20.0f) * 0.8f;
      P.rect(bx - r, by, r * 2 + 1, 1, Color(0.95f, 0.98f, 1.0f, 0.85f));
      P.rect(bx - 1, by - 1, 3, 1, Color(0.95f, 0.98f, 1.0f, 0.6f));
      if (ln.phase == sea::Sea::Cast::Reeling && frand((uint32_t)(t_ * 20)) < 0.4f) SeaViewOps::spray(*this, ln.bob, 1, 0.3f, rgba(230, 244, 250), (uint32_t)(t_ * 977));
    } else if (ln.phase == sea::Sea::Cast::Casting) {
      P.rect(bx - 1, by - 2, 3, 2, Color(0.92f, 0.24f, 0.20f, 1));
    }
  }
  // ---- spray and splinters (in the air: drawn here, over the water)
  for (const auto& d : S.drops) {
    const float x = std::floor(d.p.x - cam.x), y = std::floor(d.p.y - d.z - cam.y);
    P.rect(x, y, 1, 1, rgbaC(d.col, std::min(1.0f, d.life * 2.0f)));
  }
}

// ---------------------------------------------------------------- the sorted pass: the ships
// idx: 0..999 a boat we own (aboard: its BACK layer), 1000 + i its FRONT layer, 2000 + j a harbour's moored ship,
// 3000 + k a ship not ours (Sea::Live::ships)
void View::seaSorted(Game& g, Vec2 cam, const std::function<void(float, int)>& push) {
  SeaViewState& S = SeaViewOps::st(*this);
  S.heroUnderRig = false;
  if (g.inside) return;
  auto onScreen = [&](Vec2 p, float pad) { return p.x > cam.x - pad && p.x < cam.x + Pix::W + pad && p.y > cam.y - pad && p.y < cam.y + Pix::H + pad + 60; };
  for (int i = 0; i < (int)g.sea.boats.size() && i < 1000; i++) {
    Vec2 p;
    float h = 0;
    if (!SeaViewOps::boatAt(g, i, p, h)) continue;
    if (!onScreen(p, 80)) continue;
    if (i == g.sea.aboard) { push(p.y - 0.5f, i); push(p.y + 0.5f, 1000 + i); }
    else push(p.y, i);
  }
  for (int j = 0; j < (int)S.moored.size(); j++) {
    const SeaViewState::Moored& M = S.moored[(size_t)j];
    bool taken = false;   // a boat of ours lies there
    for (const sea::Boat& b : g.sea.boats) if (!b.sunk && b.gx == M.gx && b.gy == M.gy) taken = true;
    if (taken) continue;
    const Vec2 p((float)(M.gx - g.world.ox) * TILE + 8.0f, (float)(M.gy - g.world.oy) * TILE + 8.0f);
    if (onScreen(p, 80)) push(p.y, 2000 + j);
  }
  for (int k = 0; k < (int)g.sea.live.ships.size(); k++) {
    const sea::Sea::Ship& sh = g.sea.live.ships[(size_t)k];
    if (onScreen(sh.p, 80)) push(sh.p.y, 3000 + k);
  }
}

void View::drawSeaSorted(Game& g, int idx, Vec2 cam) {
  SeaViewState& S = SeaViewOps::st(*this);
  Pix& P = *pix_;
  if (idx >= 3000) {   // a pirate sloop (or another ship that is not ours)
    const int k = idx - 3000;
    if (k >= (int)g.sea.live.ships.size()) return;
    const sea::Sea::Ship& sh = g.sea.live.ships[(size_t)k];
    art::ShipStyle st;
    st.extras = sh.kind == 1 ? art::SX_PIRATE : 0;
    st.sailCut = 0;
    st.fill = sh.fleeing || sh.speed > 20 ? 2 : 1;
    st.brace = (int8_t)((sh.id & 1) ? 1 : -1);
    const int hull = (int)sh.hull;
    const Tex& t = SeaViewOps::cell(*this, hull, st, fineDirOf(sh.heading), SeaViewOps::bobFrame(*this, (int)sh.id, 3.0f), art::SHIP_LAYER_ALL);
    SeaViewOps::drawCell(*this, t, hull, Vec2(sh.p.x - cam.x, sh.p.y - cam.y));
    S.cur.boats++;
    return;
  }
  if (idx >= 2000) {   // a harbour's own ship, moored, its sails furled
    const int j = idx - 2000;
    if (j >= (int)S.moored.size()) return;
    const SeaViewState::Moored& M = S.moored[(size_t)j];
    const Vec2 p((float)(M.gx - g.world.ox) * TILE + 8.0f, (float)(M.gy - g.world.oy) * TILE + 8.0f);
    const Tex& t = SeaViewOps::cell(*this, M.hull, M.st, M.dir, SeaViewOps::bobFrame(*this, j + 11, 0), art::SHIP_LAYER_ALL);
    SeaViewOps::drawCell(*this, t, M.hull, Vec2(p.x - cam.x, p.y - cam.y));
    S.cur.boats++;
    return;
  }
  const bool front = idx >= 1000;
  const int i = front ? idx - 1000 : idx;
  Vec2 p;
  float h = 0;
  if (!SeaViewOps::boatAt(g, i, p, h)) return;
  const sea::Boat& b = g.sea.boats[(size_t)i];
  const int hull = (int)b.hull;
  const bool aboard = i == g.sea.aboard;
  art::ShipStyle st = aboard ? SeaViewOps::aboardStyle(g, b) : SeaViewOps::mooredStyle(g, b);
  if (i == g.sea.live.sinkBoat && g.sea.live.sinkT > 0) { st.extras |= art::SX_WRECKED; st.furled = false; st.fill = 0; }
  const int f = SeaViewOps::bobFrame(*this, i, aboard ? std::fabs(g.sea.live.speed) : 0.0f);
  const int layer = !aboard ? art::SHIP_LAYER_ALL : front ? art::SHIP_LAYER_FRONT : art::SHIP_LAYER_BACK;
  const Tex& t = SeaViewOps::cell(*this, hull, st, fineDirOf(h), f, layer);
  const Vec2 scr(p.x - cam.x, p.y - cam.y);
  if (!front) { SeaViewOps::drawCell(*this, t, hull, scr); S.cur.boats++; return; }
  // the front layer: drawn after the hero. Where it stands over him (a sail before him on a southerly heading) the
  // render's ghost shows him through it; the cell goes down at full strength except a little thinner over his body
  const float x0 = std::floor(scr.x) - (float)art::shipAnchorX(hull), y0 = std::floor(scr.y) - (float)art::shipAnchorY(hull);
  const float lift = seaHeroLift(*this, g);
  float hx0 = std::floor(scr.x) - 6, hx1 = std::floor(scr.x) + 6, hy0 = std::floor(scr.y) - lift - 21, hy1 = std::floor(scr.y) - lift;
  if (art::rigVersion() >= 2) {
    // (P2b HOLDERS-B) rig 2: the floor lies inside the hull (art_holders2.h kGunwale2 / kDeckRail2), so the near side
    // always hides the legs below the gunwale or rail, as a real boat does. Only what stands before the body over the
    // rail (a sail, a shroud) counts as cover: the hull is never thinned over him, and his legs never show through it
    const int rail = art::holderPx(hull <= 0 ? art::Holder::RowboatGunwale : art::Holder::DeckRail);
    hx0 = std::floor(scr.x) - 8; hx1 = std::floor(scr.x) + 8;
    hy0 = std::floor(scr.y) - lift - 33; hy1 = std::floor(scr.y) - lift - (float)rail;
  }
  // does the front layer cover the hero's body? (sample its alpha mask: the cell's canvas is cheap to re-ask at the
  // fine heading; once per frame for the one boat)
  const int fx0 = (int)(hx0 - x0), fy0 = (int)(hy0 - y0), fx1 = (int)(hx1 - x0), fy1 = (int)(hy1 - y0);
  static uint64_t lastKey = 0;
  static int lastCover = 0;
  const uint64_t key = art::shipStyleKey(hull, st) ^ (uint64_t)fineDirOf(h) << 40 ^ (uint64_t)f << 50;
  if (key != lastKey) {
    lastKey = key;
    const Canvas c = art::shipCell(hull, st, fineDirOf(h), f, art::SHIP_LAYER_FRONT);
    int cover = 0;
    for (int y = fy0; y < fy1; y++)
      for (int x = fx0; x < fx1; x++) if ((c.get(x, y) >> 24) > 0) cover++;
    lastCover = cover;
  }
  // (P2b HOLDERS-B) rig 2: a rowboat has no sail; the oars before the rower are in his hands, never cover to see through
  // and over the 16 x 17 box above a sloop's rail a mast alone (3 px wide) is no cover either: a sail is (90 px or more)
  S.heroUnderRig = art::rigVersion() >= 2 ? hull > 0 && lastCover >= 90 : lastCover > 30;
  const float cw = (float)t.w, chh = (float)t.h;
  if (!S.heroUnderRig) { P.blitEx(t, 0, 0, t.w, t.h, x0, y0, cw, chh); return; }
  // full strength round the hero, a touch see-through over him (the ghost silhouette does the rest)
  auto part = [&](float sx, float sy, float sw, float sh, float a) {
    if (sw <= 0 || sh <= 0) return;
    P.blitEx(t, (int)sx, (int)sy, (int)sw, (int)sh, x0 + sx, y0 + sy, sw, sh, false, Color(1, 1, 1, a));
  };
  const float ax = std::max(0.0f, (float)fx0), ay = std::max(0.0f, (float)fy0), bx = std::min(cw, (float)fx1), by = std::min(chh, (float)fy1);
  part(0, 0, cw, ay, 1.0f);
  part(0, by, cw, chh - by, 1.0f);
  part(0, ay, ax, by - ay, 1.0f);
  part(bx, ay, cw - bx, by - ay, 1.0f);
  part(ax, ay, bx - ax, by - ay, 0.72f);
}

// ---------------------------------------------------------------- lights: lanterns at night, the lighthouse lamp
void View::seaLights(Game& g, const std::function<void(Vec2, float, Color, float)>& light) {
  if (g.inside) return;
  const float night = std::max(0.0f, std::min(1.0f, 1.0f - g.daylight() * 1.6f));
  const float storm = SeaViewOps::st(*this).darkShown;
  const float k = std::max(night, storm * 0.8f);
  if (k < 0.02f) return;
  const float flick = 0.88f + 0.12f * std::sin(t_ * 11.0f) * std::sin(t_ * 6.1f);
  auto lantern = [&](Vec2 p, int hull, const art::ShipStyle& st, float heading, float power) {
    int lx, ly;
    art::shipLanternAt(hull, st, fineDirOf(heading), lx, ly);
    const Vec2 w(p.x - art::shipAnchorX(hull) + lx, p.y - art::shipAnchorY(hull) + ly);
    light(w, hull >= 2 ? 70.0f : 54.0f, Color(1.0f, 0.78f, 0.45f), k * power * flick);
  };
  for (int i = 0; i < (int)g.sea.boats.size(); i++) {
    Vec2 p;
    float h;
    if (!SeaViewOps::boatAt(g, i, p, h)) continue;
    const sea::Boat& b = g.sea.boats[(size_t)i];
    if ((int)b.hull == 0) continue;
    lantern(p, (int)b.hull, SeaViewOps::mooredStyle(g, b), h, i == g.sea.aboard ? 0.9f : 0.55f);
    if (i == g.sea.aboard && (b.upgrades & sea::UP_LANTERN)) {
      const Vec2 F(std::cos(h), std::sin(h));
      light(p + F * (hullHalfLen((int)b.hull) + 4.0f) + Vec2(0, -8), 120.0f, Color(1.0f, 0.86f, 0.6f), k * 0.7f * flick);
    }
  }
  const SeaViewState& S = SeaViewOps::st(*this);
  for (const auto& M : S.moored) {
    if (M.hull == 0) continue;   // (fixer M10 r1) every moored ship shows her stern lantern (half of them were dark)
    const Vec2 p((float)(M.gx - g.world.ox) * TILE + 8.0f, (float)(M.gy - g.world.oy) * TILE + 8.0f);
    lantern(p, M.hull, M.st, M.dir * kTau / art::SHIP_FINE_DIRS, (hh(M.gx, M.gy, 0x1A) & 1) ? 0.4f : 0.55f);
  }
  for (const sea::Sea::Ship& sh : g.sea.live.ships) light(sh.p + Vec2(0, -14), 50.0f, Color(1.0f, 0.55f, 0.35f), k * 0.6f * flick);
  // the lighthouses' lamps (the prop's lantern room, high over its foot)
  const Map& m = g.world.over;
  const int tx0 = (int)std::floor(cam_.x / 16) - 4, ty0 = (int)std::floor(cam_.y / 16) - 2;
  for (int ty = std::max(0, ty0); ty < std::min(m.h, ty0 + Pix::H / 16 + 10); ty++)
    for (int tx = std::max(0, tx0); tx < std::min(m.w, tx0 + Pix::W / 16 + 8); tx++)
      if (m.propAt(tx, ty) == (int)art::Prop::Lighthouse + 1) {
        const Vec2 lamp(tx * 16.0f + 8.0f, ty * 16.0f + 16.0f - (float)art::seaPropH(art::Prop::Lighthouse) + 12.0f);
        light(lamp, 150.0f, Color(1.0f, 0.86f, 0.55f), k * 0.95f);
      } else if (m.propAt(tx, ty) == (int)art::Prop::HarbourDesk + 1) {   // (fixer M10 r1) the harbourmaster's lantern
        light(Vec2(tx * 16.0f + 8.0f, ty * 16.0f + 16.0f - (float)art::seaPropH(art::Prop::HarbourDesk) * 0.55f), 96.0f, Color(1.0f, 0.8f, 0.5f), k * 0.85f * flick);
      } else if (m.propAt(tx, ty) == (int)art::Prop::MooringPost + 1 && (hh(g.world.ox + tx, g.world.oy + ty, 0x1B) % 3u) == 0) {
        // a lantern hung on every third pile along the pier: pools of warm light on the planks and the water
        light(Vec2(tx * 16.0f + 8.0f, ty * 16.0f + 2.0f), 44.0f, Color(1.0f, 0.76f, 0.44f), k * 0.5f * flick);
      }
}

// ---------------------------------------------------------------- the sky over the sea: the Mist, storms, lightning
void View::drawSeaOver(Game& g, Vec2 cam) {
  if (g.inside) return;
  SeaViewState& S = SeaViewOps::st(*this);
  SeaTimer tm(S.cur.ms);
  SeaViewOps::bake(*this);
  Pix& P = *pix_;
  const float W = (float)Pix::W, H = (float)Pix::H;
  // ---- the lighthouse's beam sweeping the night sea
  {
    const float night = std::max(0.0f, std::min(1.0f, 1.0f - g.daylight() * 1.6f));
    const Map& m = g.world.over;
    if (night > 0.05f) {
      const int tx0 = (int)std::floor(cam.x / 16) - 4, ty0 = (int)std::floor(cam.y / 16) - 2;
      for (int ty = std::max(0, ty0); ty < std::min(m.h, ty0 + Pix::H / 16 + 10); ty++)
        for (int tx = std::max(0, tx0); tx < std::min(m.w, tx0 + Pix::W / 16 + 8); tx++)
          if (m.propAt(tx, ty) == (int)art::Prop::Lighthouse + 1) {
            const Vec2 lamp(tx * 16.0f + 8.0f - cam.x, ty * 16.0f + 16.0f - (float)art::seaPropH(art::Prop::Lighthouse) + 12.0f - cam.y);
            const float a = t_ * 0.9f;
            const Vec2 d(std::cos(a), std::sin(a) * 0.6f);
            for (int i = 2; i < 60; i++) {
              const float w = 1.0f + i * 0.35f;
              const Vec2 q = lamp + d * (i * 3.0f);
              P.rectAdd(std::floor(q.x - w * 0.5f), std::floor(q.y - w * 0.3f), w, std::max(1.0f, w * 0.6f), Color(0.30f, 0.26f, 0.14f, night * (1.0f - i / 60.0f) * 0.55f));
            }
          }
    }
  }
  // ---- the storm: the darkened sky, slanting rain, lightning
  const float dark = S.darkShown;
  if (dark > 0.01f) {
    P.rect(0, 0, W, H, Color(0.06f, 0.08f, 0.14f, std::min(0.62f, dark * 0.62f)));
    const sea::Sea::Live& L = g.sea.live;
    const float wx = std::cos(L.wind.dir) * (0.4f + L.wind.speed), slant = 0.35f + 0.5f * std::fabs(wx);
    const int n = (int)(40 + 220 * dark);
    for (int i = 0; i < n; i++) {
      const uint32_t h = hash32((uint32_t)i * 2654435761u);
      const float sp = 260.0f + 120.0f * frand(h);
      const float x0 = frand(h ^ 0x51) * (W + 80) - 40, y0 = frand(h ^ 0x93) * (H + 60);
      float y = std::fmod(y0 + t_ * sp, H + 60) - 30;
      float x = std::fmod(x0 + (wx >= 0 ? 1.0f : -1.0f) * slant * (y + t_ * sp * 0.2f) + W + 80, W + 80) - 40;
      const float l = 4.0f + 4.0f * frand(h ^ 0x17);
      for (int s = 0; s < (int)l; s++)
        P.rect(std::floor(x + (wx >= 0 ? 1.0f : -1.0f) * slant * s), std::floor(y + s), 1, 1, Color(0.70f, 0.78f, 0.88f, 0.45f * dark));
      if ((h & 7) == 0 && std::fmod(y0 + t_ * sp, H + 60) > H) P.rect(std::floor(x), std::floor(y0 * 0.7f), 2, 1, Color(0.85f, 0.92f, 1.0f, 0.4f * dark));   // a splash
    }
  }
  for (const auto& b : S.bolts) {
    const float a = 1.0f - b.t / 0.35f;
    Vec2 p(b.a.x - cam.x, b.a.y - cam.y);
    const Vec2 end(b.b.x - cam.x, b.b.y - cam.y);
    const int segs = 9;
    for (int i = 1; i <= segs; i++) {
      Vec2 q = Vec2(b.a.x - cam.x, b.a.y - cam.y) + (end - Vec2(b.a.x - cam.x, b.a.y - cam.y)) * ((float)i / segs);
      if (i < segs) q.x += (frand(b.seed + i * 13) - 0.5f) * 16.0f;
      const Vec2 d = q - p;
      const int steps = std::max(1, (int)std::max(std::fabs(d.x), std::fabs(d.y)));
      for (int s = 0; s < steps; s++) {
        const Vec2 r = p + d * ((float)s / steps);
        P.rectAdd(std::floor(r.x) - 1, std::floor(r.y), 3, 1, Color(0.35f, 0.40f, 0.70f, a));
        P.rect(std::floor(r.x), std::floor(r.y), 1, 1, Color(1.0f, 1.0f, 1.0f, a));
      }
      p = q;
    }
  }
  if (S.flash > 0.01f) P.rectAdd(0, 0, W, H, Color(0.55f, 0.58f, 0.70f, S.flash * 0.55f));
  // ---- the Mist: drifting banks over a cold veil, thickening with the density; colder and closing in while it turns
  //      the boat; parted by the chart into a ring of fog round a clear, lit heart
  const float m = std::max(S.mistShown, S.partT > 0 ? 0.6f : 0.0f);
  if (m > 0.01f) {
    const float part = S.partShown;
    const float turn = S.turnShown;
    const float inner = m * (1.0f - part * 0.92f);   // the banks over the middle of the screen
    // a cold veil (bluer and darker while it turns the boat back)
    P.rect(0, 0, W, H, Color(0.72f - 0.22f * turn, 0.78f - 0.18f * turn, 0.84f - 0.08f * turn, std::min(0.62f, inner * 0.55f)));
    // the drifting banks, two layers at their own pace and scale (world-anchored, so the boat sails through them)
    for (int layer = 0; layer < 3; layer++) {
      const Tex& ft = S.fog[layer == 2 ? 1 : 0];
      const float sc = layer == 0 ? 2.0f : 1.0f;
      const float size = 256.0f * sc;
      const float vx = (layer == 0 ? 6.0f : layer == 1 ? -9.0f : 14.0f) * (1.0f + turn * 2.0f), vy = layer == 1 ? 3.0f : -2.0f;
      const float ox = std::fmod(-(cam.x * (0.6f + 0.2f * layer)) + t_ * vx, size), oy = std::fmod(-(cam.y * (0.6f + 0.2f * layer)) + t_ * vy, size);
      const float a = std::min(1.0f, inner * (layer == 0 ? 0.95f : layer == 1 ? 0.75f : 0.6f));
      const Color tint = Color(1.0f - 0.25f * turn, 1.0f - 0.2f * turn, 1.0f - 0.08f * turn, a);
      for (float y = oy - size; y < H; y += size)
        for (float x = ox - size; x < W; x += size) P.blitEx(ft, 0, 0, 256, 256, std::floor(x), std::floor(y), size, size, false, tint);
    }
    // the fog ring at the edges (always: the Mist closes round the boat; with the chart it is all that is left)
    P.blitEx(S.fogRing, 0, 0, 320, 180, -W * 0.08f, -H * 0.08f, W * 1.16f, H * 1.16f, false,
             Color(1.0f - 0.3f * turn, 1.0f - 0.25f * turn, 1.0f - 0.1f * turn, std::min(1.0f, m * (0.75f + 0.25f * part))));
    // turning back: the dark closes in at the edges, and the shapes of drowned ships slide past in the fog
    if (turn > 0.02f) {
      P.blitEx(vignette_, 0, 0, vignette_.w, vignette_.h, 0, 0, W, H, false, Color(0.4f, 0.45f, 0.6f, turn * 0.9f));
      for (int k = 0; k < 2; k++) {
        // (fixer M10 r1) quartering headings (SE, NE), drifting the way their bows point: the deck and the hull's
        // side both show, in the same 3/4 view as the hero's boat (a beam-on hull read as a flat side elevation)
        const float x = std::fmod(t_ * (7.0f + 4.0f * k) + k * 260.0f, W + 200.0f) - 100.0f;
        const float y = H * (0.30f + 0.36f * k) + (x - W * 0.5f) * (k ? -0.22f : 0.22f);
        art::ShipStyle ghost;
        ghost.extras = art::SX_WRECKED;
        ghost.sailCut = 0;
        const Tex& t = SeaViewOps::cell(*this, 2 - k, ghost, k ? 28 : 4, 0, art::SHIP_LAYER_ALL);
        SeaViewOps::drawCell(*this, t, 2 - k, Vec2(x, y), turn * 0.22f, Color(0.55f, 0.62f, 0.72f, 1));
      }
    }
    // the parting: the light breaking through over the open water ahead
    if (part > 0.02f || S.partT > 0) {
      const float burst = S.partT > 0 ? std::sin(std::min(1.0f, S.partT / 8.0f) * 3.14159f) : 0.0f;
      const float k = std::max(part * 0.55f, burst) * std::max(m, 0.5f);
      for (int i = 0; i < 5; i++) {   // shafts of sun slanting from the top-left: soft-edged, each breathing on its own
        const float x = W * (0.08f + 0.19f * i + 0.05f * std::sin(i * 2.3f)) + std::sin(t_ * 0.25f + i) * 8.0f;
        const float breathe = 0.55f + 0.45f * std::sin(t_ * 0.6f + i * 1.9f);
        const float w = 10.0f + 8.0f * std::fabs(std::sin(i * 1.7f));
        for (float y = -10; y < H; y += 2) {
          const float fall = (1.0f - y / H) * (1.0f - y / H);
          const float a = k * 0.55f * fall * breathe;
          const float x0 = std::floor(x + y * 0.45f);
          P.rectAdd(x0 + w * 0.3f, std::floor(y), w * 0.4f, 2, Color(0.30f, 0.26f, 0.14f, a));
          P.rectAdd(x0, std::floor(y), w, 2, Color(0.30f, 0.26f, 0.14f, a * 0.45f));
          P.rectAdd(x0 - w * 0.4f, std::floor(y), w * 1.8f, 2, Color(0.30f, 0.26f, 0.14f, a * 0.18f));
        }
      }
      P.blitEx(light_, 0, 0, 64, 64, W * 0.15f, -H * 0.2f, W * 0.7f, H * 0.9f, false, Color(1.0f, 0.92f, 0.70f, k * 0.5f), 1);
    }
  }
}

// ---------------------------------------------------------------- the HUD
namespace {
// a little framed panel in the HUD's look (dark, a gold hairline, a lit top edge)
void seaPanel(Pix& P, float x, float y, float w, float h, float a = 0.86f) {
  P.rect(x, y, w, h, Color(0.06f, 0.06f, 0.09f, a));
  P.frame(x, y, w, h, Color(0.62f, 0.50f, 0.28f, 0.9f));
  P.rect(x + 1, y + 1, w - 2, 1, Color(1.0f, 0.9f, 0.6f, 0.18f));
}
// a 1px line between two points
void segment(Pix& P, Vec2 a, Vec2 b, Color c) {
  const Vec2 d = b - a;
  const int n = std::max(1, (int)std::max(std::fabs(d.x), std::fabs(d.y)));
  for (int i = 0; i <= n; i++) P.rect(std::floor(a.x + d.x * i / n), std::floor(a.y + d.y * i / n), 1, 1, c);
}
}  // namespace

void View::drawSeaHud(Game& g) {
  SeaViewState& S = SeaViewOps::st(*this);
  S.hudX0 = S.hudX1 = S.hudY0 = S.hudY1 = 0;
  Pix& P = *pix_;
  const float L = (float)Pix::SL, T = (float)Pix::ST, R = (float)(Pix::W - Pix::SR), B = (float)(Pix::H - Pix::SB);
  const sea::Sea::Live& Lv = g.sea.live;
  const sea::Boat* b = SeaOps::aboardBoat(g);
  if (b && (g.mode == Mode::Play || g.mode == Mode::Dialogue)) {
    // ---- the sail panel: a compass (the boat's heading, the wind, the sail's point), the speed, the hull
    const float pw = 124, ph = 40;
    float px = L + 4, py = B - ph - 4;
    if (touchUI) {   // the phone: up the left edge, clear above the stick (and right of the party's portraits)
      py = B - 55.0f - 40.0f - ph;
      if (partyColB_ > 0 && partyColB_ > py - 4) px = std::max(px, partyColR_ + 4.0f);
    }
    S.hudX0 = px; S.hudY0 = py; S.hudX1 = px + pw; S.hudY1 = py + ph;
    seaPanel(P, px, py, pw, ph);
    const Vec2 c(px + 20, py + 20);
    const float r = 15;
    // the dial: a bezel with the cardinal ticks, N in gold
    for (int i = 0; i < 48; i++) {
      const float a = i * kTau / 48;
      P.rect(std::floor(c.x + std::cos(a) * r), std::floor(c.y + std::sin(a) * r), 1, 1, Color(0.70f, 0.58f, 0.34f, 0.85f));
    }
    for (int i = 0; i < 8; i++) {
      const float a = i * kTau / 8;
      const float rr = (i & 1) ? r - 2 : r - 3;
      segment(P, c + Vec2(std::cos(a), std::sin(a)) * rr, c + Vec2(std::cos(a), std::sin(a)) * (r - 1), Color(0.86f, 0.76f, 0.52f, 0.9f));
    }
    P.text(c.x - 2, c.y - r - 1 + 3, "N", 1, Color(0.98f, 0.82f, 0.42f));
    // the wind: a pale arrow across the dial toward where it blows, as long as it is strong, with streaks
    {
      const float wa = Lv.wind.dir, ws = std::max(0.25f, std::min(1.0f, Lv.wind.speed));
      const Vec2 d(std::cos(wa), std::sin(wa)), n(-d.y, d.x);
      const Vec2 tail = c - d * (r - 4) * ws, head = c + d * (r - 4) * ws;
      segment(P, tail, head, Color(0.62f, 0.86f, 1.0f, 0.95f));
      segment(P, head, head - d * 3.5f + n * 2.5f, Color(0.62f, 0.86f, 1.0f, 0.95f));
      segment(P, head, head - d * 3.5f - n * 2.5f, Color(0.62f, 0.86f, 1.0f, 0.95f));
      for (int k = -1; k <= 1; k += 2) {   // streaks drifting along it
        const float ph = std::fmod(t_ * 0.8f + (k > 0 ? 0.5f : 0.0f), 1.0f);
        const Vec2 s = c + n * (k * 5.0f) - d * (r - 5) + d * (2 * (r - 5) * ph);
        segment(P, s, s + d * 3.0f, Color(0.62f, 0.86f, 1.0f, 0.5f * std::sin(ph * 3.14159f)));
      }
    }
    // the boat: a little hull pointing along the heading
    {
      const Vec2 d(std::cos(Lv.heading), std::sin(Lv.heading)), n(-d.y, d.x);
      const Vec2 bow = c + d * 8.0f, st0 = c - d * 6.0f;
      for (float t = 0; t <= 1.0f; t += 0.08f) {
        const Vec2 q = st0 + (bow - st0) * t;
        const float w = 2.4f * std::sqrt(std::max(0.0f, 1.0f - std::pow(std::fabs(t * 2 - 1.1f), 2.0f)));
        segment(P, q - n * w, q + n * w, Color(0.72f, 0.50f, 0.30f, 1));
      }
      segment(P, st0, bow, Color(0.95f, 0.88f, 0.70f, 1));
      P.rect(std::floor(bow.x), std::floor(bow.y), 1, 1, Color(1, 1, 1, 1));
    }
    // the point of sail and the speed
    const float rel = std::fabs(std::remainder(Lv.heading - Lv.wind.dir, kTau));   // 0 running before it .. pi head to wind
    const char* pt = b->hull == sea::Hull::Rowboat ? "AT THE OARS" : rel < 0.6f ? "RUNNING" : rel < 1.25f ? "BROAD REACH" : rel < 1.95f ? "BEAM REACH" : rel < 2.6f ? "CLOSE HAULED" : "IN IRONS";
    char buf[64];
    std::snprintf(buf, sizeof buf, "%.1f KN", S.speedShown * 2.0f);
    const float tx = px + 40;
    P.textS(tx, py + 5, sea::hullName(b->hull), 1, Color(0.98f, 0.82f, 0.42f));
    P.textS(tx, py + 15, buf, 1, Color(0.93f, 0.90f, 0.82f));
    P.textS(tx, py + 25, pt, 1, rel > 2.6f && b->hull != sea::Hull::Rowboat ? Color(0.95f, 0.55f, 0.45f) : Color(0.62f, 0.86f, 1.0f));
    // the hull: a bar of planks down the panel's right edge
    const float hmax = std::max(1.0f, sea::hullInfo(b->hull).hull * ((b->upgrades & sea::UP_PLATING) ? 1.5f : 1.0f));
    const float k = std::max(0.0f, std::min(1.0f, b->hp / hmax));
    const float bx = px + pw - 8, by0 = py + 5, bh = ph - 10;
    P.rect(bx - 1, by0 - 1, 5, bh + 2, Color(0.02f, 0.02f, 0.04f, 0.9f));
    P.rect(bx, by0, 3, bh, Color(0.20f, 0.12f, 0.08f, 1));
    const Color hc = k > 0.5f ? Color(0.78f, 0.56f, 0.32f) : k > 0.25f ? Color(0.92f, 0.70f, 0.28f) : Color(0.92f, 0.30f, 0.22f);
    P.rect(bx, by0 + bh * (1 - k), 3, bh * k, hc);
    for (float y = by0 + 3; y < by0 + bh; y += 4) P.rect(bx, std::floor(y), 3, 1, Color(0, 0, 0, 0.25f));
    // ---- the Mist's grip: how long before it turns the boat (no chart), or the way it opens (the chart)
    if (Lv.mistLand && Lv.mist > 0.02f) {
      const bool chart = g.sea.hasChart(Lv.mistLand);
      const float my = py - 14;
      seaPanel(P, px, my, pw, 12, 0.8f);
      if (chart) P.textS(px + 4, my + 3, "THE MIST PARTS", 1, Color(1.0f, 0.92f, 0.66f));
      else {
        const float gk = std::min(1.0f, Lv.mistT / sea::MIST_TURN_SECS);
        P.textS(px + 4, my + 3, Lv.turningBack ? "TURNED BACK" : "THE MIST", 1, Color(0.80f, 0.86f, 0.94f));
        P.rect(px + 76, my + 4, 44, 4, Color(0.10f, 0.12f, 0.16f, 1));
        P.rect(px + 76, my + 4, 44 * gk, 4, Color(0.78f + 0.2f * gk, 0.84f - 0.3f * gk, 0.92f - 0.5f * gk, 1));
      }
      S.hudY0 = my;
    }
    if (Lv.storm.on && Lv.stormDark > 0.2f && ((int)(t_ * 2) & 1)) P.textS(px + pw - 40, py - 10, "STORM", 1, Color(0.95f, 0.75f, 0.40f));
  }
  // ---- the fishing panel: the float's bite cue, the tension meter and the run, beside the hero
  const sea::Sea::Line& ln = Lv.line;
  if (ln.phase != sea::Sea::Cast::None && g.mode == Mode::Play) {
    // (P2a integration) this panel is in the UI layer: the hero and the float go through toUi, world offsets through u
    const float u = uiPerWorld();
    const Vec2 hero = toUi(Vec2(g.pl().p.x, g.pl().p.y - seaHeroLift(*this, g)));
    const bool fight = ln.phase == sea::Sea::Cast::Reeling;
    if (ln.phase == sea::Sea::Cast::Bite || S.biteT > 0) {   // the bite: a bright "!" over the float, the button cue
      const Vec2 bob = toUi(Vec2(ln.bob.x, ln.bob.y));
      const float jump = std::fabs(std::sin(t_ * 14.0f)) * 3.0f;
      P.textS(std::floor(bob.x) - 2, std::floor(bob.y - 18 * u - jump), "!", 2, Color(1.0f, 0.86f, 0.30f));

    }
    if (fight) {
      // the meter: a vertical gauge right of the hero: green slack .. gold .. red snap; the needle eases
      float mx = std::floor(hero.x + 16 * u), my = std::floor(hero.y - 44 * u);
      if (mx + 30 > R) mx = std::floor(hero.x - 16 * u - 30);
      const float mh = 40;
      seaPanel(P, mx, my, 14, mh + 6, 0.85f);
      for (int i = 0; i < (int)mh; i++) {
        const float v = 1.0f - i / mh;   // top: 1 (snapping)
        const Color zc = v > 0.82f ? Color(0.86f, 0.24f, 0.20f) : v > 0.55f ? Color(0.92f, 0.74f, 0.28f) : Color(0.30f, 0.70f, 0.40f);
        P.rect(mx + 3, my + 3 + i, 8, 1, Color(zc.r * 0.45f, zc.g * 0.45f, zc.b * 0.45f, 1));
      }
      const float tv = std::max(0.0f, std::min(1.0f, S.tensionShown));
      const float fillH = mh * tv;
      const Color fc = tv > 0.82f ? Color(1.0f, 0.32f, 0.24f) : tv > 0.55f ? Color(1.0f, 0.82f, 0.32f) : Color(0.40f, 0.86f, 0.50f);
      P.rect(mx + 3, my + 3 + mh - fillH, 8, fillH, fc);
      P.rect(mx + 2, std::floor(my + 3 + mh - fillH), 10, 1, Color(1, 1, 1, 0.9f));
      if (tv > 0.82f && ((int)(t_ * 10) & 1)) P.frame(mx, my, 14, mh + 6, Color(1.0f, 0.4f, 0.3f, 1));
      // the run: how far the fish still is (a fish icon sliding home along a line under the hero)
      const float lw = 44, lx = std::floor(hero.x - lw / 2), ly = std::floor(hero.y + 8 * u);
      P.rect(lx, ly, lw, 1, Color(0.85f, 0.88f, 0.92f, 0.6f));
      const float fx = lx + lw * std::max(0.0f, std::min(1.0f, S.distShown));
      P.rect(std::floor(fx) - 2, ly - 1, 4, 3, Color(0.70f, 0.82f, 0.90f, 1));
      P.rect(std::floor(fx) + 2, ly - 1, 1, 1, Color(0.70f, 0.82f, 0.90f, 1));
      P.rect(std::floor(fx) + 2, ly + 1, 1, 1, Color(0.70f, 0.82f, 0.90f, 1));
    }
    if (S.lostT > 0) P.textS(std::floor(hero.x), std::floor(hero.y - 40 * u), "LOST IT...", 1, Color(0.9f, 0.6f, 0.5f, std::min(1.0f, S.lostT * 2)), 1);
  }
  // ---- the catch card: the fish painted, its name, its size, how rare
  if (S.card.t > 0 && S.card.species >= 0 && S.card.species < (int)sea::species().size()) {
    const sea::Species& sp = sea::species()[(size_t)S.card.species];
    auto it = S.fishTex.find(S.card.species);
    if (it == S.fishTex.end()) it = S.fishTex.emplace(S.card.species, pix_->bake(art::fishSprite(sp.color, (uint32_t)S.card.species * 2654435761u, sp.rarity))).first;
    const float in = std::min(1.0f, (4.0f - S.card.t) * 4.0f), out = std::min(1.0f, S.card.t * 2.0f);
    const float a = std::min(in, out);
    const float cw = 150, ch = 62;
    const float cx = std::floor((L + R) / 2 - cw / 2), cy = std::floor(T + 36 - (1.0f - in) * 12.0f);
    P.rect(cx + 2, cy + 2, cw, ch, Color(0, 0, 0, 0.35f * a));
    P.rect(cx, cy, cw, ch, Color(0.90f, 0.84f, 0.70f, 0.96f * a));   // parchment
    P.frame(cx, cy, cw, ch, Color(0.45f, 0.32f, 0.18f, a));
    P.frame(cx + 2, cy + 2, cw - 4, ch - 4, Color(0.62f, 0.48f, 0.28f, 0.7f * a));
    const Tex& ft = it->second;
    P.blitEx(ft, 0, 0, ft.w, ft.h, std::floor(cx + 8), std::floor(cy + ch / 2 - ft.h / 2.0f), (float)ft.w, (float)ft.h, false, Color(1, 1, 1, a));
    const float tx = cx + 12 + ft.w;
    P.text(tx, cy + 9, sp.name, 1, Color(0.24f, 0.14f, 0.08f, a));
    char buf[48];
    std::snprintf(buf, sizeof buf, "%d CM", S.card.cm > 0 ? S.card.cm : (int)sp.minCm);
    P.text(tx, cy + 21, buf, 1, Color(0.36f, 0.24f, 0.14f, a));
    static const char* kRare[] = {"COMMON", "UNCOMMON", "RARE", "LEGENDARY"};
    static const Color kRareC[] = {Color(0.40f, 0.34f, 0.26f), Color(0.20f, 0.46f, 0.24f), Color(0.20f, 0.34f, 0.66f), Color(0.70f, 0.42f, 0.10f)};
    const int rr = std::max(0, std::min(3, (int)sp.rarity));
    P.text(tx, cy + 33, kRare[rr], 1, Color(kRareC[rr].r, kRareC[rr].g, kRareC[rr].b, a));
    for (int i = 0; i <= rr; i++) {   // the stars
      const float sx = tx + i * 8, sy = cy + 46;
      P.rect(sx + 2, sy, 1, 5, Color(0.86f, 0.64f, 0.18f, a));
      P.rect(sx, sy + 2, 5, 1, Color(0.86f, 0.64f, 0.18f, a));
      P.rect(sx + 1, sy + 1, 3, 3, Color(0.96f, 0.80f, 0.30f, a));
    }
  }
  // ---- first contact: the land beyond the Mist named, a title over the sea
  if (S.firstContactT > 0 && !S.firstContact.empty()) {
    const float a = std::min(1.0f, std::min((7.0f - S.firstContactT) * 1.5f, S.firstContactT));
    const float y = std::floor(T + 70 + 36.0f * S.contactHide);   // (under the banner while one shows)
    const float w = (float)P.textW(S.firstContact, 1) + 30;
    P.rect(std::floor((L + R) / 2 - w / 2), y - 6, w, 20, Color(0.05f, 0.05f, 0.08f, 0.75f * a));
    P.rect(std::floor((L + R) / 2 - w / 2), y - 6, w, 1, Color(0.98f, 0.82f, 0.42f, a));
    P.rect(std::floor((L + R) / 2 - w / 2), y + 13, w, 1, Color(0.98f, 0.82f, 0.42f, a));
    P.textS((L + R) / 2, y, S.firstContact, 1, Color(1.0f, 0.92f, 0.70f, a), 1);
  }
}

// the reel: held while the action button (a finger on it), E, J or Space is down during a line
void View::seaFillInput(Game& g, Input& in) {
  in.reel = false;
  const sea::Sea::Cast ph = g.sea.live.line.phase;
  if (ph == sea::Sea::Cast::None) return;
  const bool* ks = SDL_GetKeyboardState(nullptr);
  bool held = ks[SDL_SCANCODE_J] || ks[SDL_SCANCODE_SPACE] || ks[SDL_SCANCODE_E] || scriptKeys_[SDL_SCANCODE_J] || scriptKeys_[SDL_SCANCODE_SPACE] ||
              scriptKeys_[SDL_SCANCODE_E];   // (the use key too: the prompt says "E: REEL")
  for (const auto& f : fingers_) if (f.on && f.button == 0) held = true;   // B_ATTACK (hud.cpp)
  in.reel = held;
}

// ---------------------------------------------------------------- a sea creature in the water (render.cpp's monster pass)
// Returns true when drawn here: a swimmer on the water shows only what is over the swell (its lower rows cut, a ripple
// ring and a lit line at the waterline), fades as it dives, and a kraken's tentacle uses the tentacle sheet.
bool seaDrawSwimmer(View& v, const Game& g, const Actor& a, const Tex& sheet, int fr, int cw, int chh, bool flip, float alpha, float flash, Vec2 cam) {
  if (!a.swim || g.inside) return false;
  const Map& m = g.world.over;
  const int tx = (int)std::floor(a.p.x / 16), ty = (int)std::floor((a.p.y - 2) / 16);
  if (!m.in(tx, ty) || !groundWater(m.at(tx, ty))) return false;
  Pix& P = SeaViewOps::pix(v);
  const float t = SeaViewOps::now(v);
  float dive = 0;
  int kind = 0;
  for (const auto& s : g.sea.live.swimmers) if (s.id == a.id) { dive = s.dive; kind = s.kind; }
  const bool dead = a.st == AState::Dead;
  const float aA = alpha * (1.0f - dive * 0.8f);
  const Tex* tex = &sheet;
  if (kind == 3) {   // a kraken's tentacle: its own sheet (art::seaTentacleSheet), rising out of the water as it surfaces
    static Tex tent;
    static int epoch = 0;   // (Witness) Pix::destroyUnmarked destroyed it: bake it again
    if (epoch != Pix::texEpoch) { epoch = Pix::texEpoch; tent = Tex(); }
    if (!tent.t) tent = P.bake(art::seaTentacleSheet());
    tex = &tent;
    cw = art::SEA_TENTACLE_W;
    chh = art::SEA_TENTACLE_H;
  }
  // how much shows over the swell: a serpent's coils and a kraken's mantle sit low, a merrow swims chest-deep, a
  // tentacle rises its whole height; diving sinks it all
  int cut = a.mon == art::Monster::Merrow ? 8 : (kind == 3 ? 2 : a.mon == art::Monster::Kraken ? 3 : 4);
  if (dead) cut = 2;
  const int h = std::max(2, chh - cut - (int)(dive * (chh - cut)));
  const float bob = std::floor(std::sin(t * 2.4f + a.id) * 0.8f + 0.5f);
  const float cx = std::floor(a.p.x - cam.x + 0.5f), wl = std::floor(a.p.y - cam.y + 0.5f) + bob;   // the waterline
  const float x = cx - std::floor(cw / 2.0f), y = wl - h;
  P.blitEx(*tex, fr * cw, 0, cw, h, x, y, (float)cw, (float)h, flip, Color(1, 1, 1, aA));
  if (flash > 0) P.blitEx(*tex, fr * cw, 0, cw, h, x, y, (float)cw, (float)h, flip, Color(1, 1, 1, flash * aA), 1);
  // what lies under the surface: a dark shape a little below the line
  P.blitEx(*tex, fr * cw, h, cw, std::min(cut, chh - h), x, wl + 1, (float)cw, (float)std::min(cut, chh - h), flip, Color(0.10f, 0.22f, 0.34f, 0.35f * aA));
  // the waterline: a lit line where the body breaks the water and a ring spreading from it
  const float w0 = std::max(6.0f, cw * 0.5f);
  {   // (fixer M10 r1) a broken, curling line of foam, never a ruler-straight bar (it read as a UI line)
    const int n = (int)w0;
    const float x0 = cx - std::floor(w0 * 0.5f);
    for (int i = 0; i < n; i++) {
      const float u = (i + 0.5f) / (float)n * 2.0f - 1.0f;   // -1..1 across the body
      const uint32_t hsh = hash32((uint32_t)a.id * 2654435761u ^ (uint32_t)i * 40503u ^ (uint32_t)(t * 6.0f) * 9176u);
      if ((hsh & 7u) < 2u) continue;                           // gaps that wander
      const float dy = std::floor(u * u * 1.6f + std::sin(t * 3.1f + i * 0.9f + a.id) * 0.6f + 0.5f);   // curls down at the ends
      const float k = (1.0f - 0.55f * u * u) * (0.55f + 0.2f * (float)((hsh >> 4) & 3u) / 3.0f);
      P.rect(x0 + i, wl + dy, 1, 1, Color(0.90f, 0.96f, 1.0f, k * alpha));
    }
  }
  const float rp = std::fmod(t * 0.9f + a.id * 0.3f, 1.0f);
  const float rr = w0 * 0.5f + 2 + rp * 6;
  SeaViewOps::ellipseRing(P, Vec2(cx, wl + 1), rr, rr * 0.4f, Color(0.88f, 0.95f, 1.0f, 0.45f * (1.0f - rp) * alpha), (uint32_t)a.id, 0.2f);
  return true;
}
