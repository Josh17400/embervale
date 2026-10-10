// M7 "Home": the player's property drawn in the world (View::homeCollect / drawHomeDraw / drawHomeFlats /
// drawHomeShadow) and the rider on horseback (View::drawMounted / drawMountShadow). VIEW lane.
//
// Phase A (lead, 2026-10-09) made first cuts over the art stand-ins. VIEW lane (M7 phase B):
//   - ground edits (farmland, paths) are flats: drawWorld draws them before the shadow pass, so the shadows of the yard
//     objects, the crops, the animals and the hero lie ON the tilled soil and the path stones, never under them;
//   - crops y-sort with the scene and lean in the wind (the top rows shift by a pixel on a slow wave across the field,
//     each tile on its own phase), wilted crops come from the art (paler, drooping) and do not sway;
//   - yard objects are painted in the culture style of the plot's house (Plot::style), else of the settlement the plot
//     belongs to, else of the land: a steppe farm's coop matches its yurt; fences and gates join (N/E/S/W);
//   - coops, pens and stables show how full they are (FarmObjLook::stage from the animals housed);
//   - every standing thing throws a ground shadow thrown a little down-right (the light is top-left), drawn in the
//     shadow pass;
//   - the building site of a house under construction (progress from its build days), centred on its footprint;
//   - the yard's lanterns and campfires are light sources at night (homeLights_, drawLighting) and the campfire smokes;
//   - the build ghost: the object (or the ground edit) at the ghost tile over a footprint grid, green where it fits and
//     red where it does not, and the plot's border dashed in gold; indoors (DECORATE) the furniture's prop the same way;
//   - the mounted rider: the horse's walk and gallop frames by speed, the rider's Posture::Ride cell seated at
//     art::riderSeat, the draw order per facing (facing down the horse's head and chest come over the rider, facing up
//     its rump over the rider's legs, side-on the near leg over the flank), the hurt flash, the i-frame blink, a shadow
//     as long as the horse, and the rider's silhouette for the hidden-hero ghost.
// Every texture is cached in homeTex_ (keys by kind in the top byte); nothing is painted twice.
#include <algorithm>
#include <chrono>
#include <cmath>
#include <string>
#include <unordered_map>
#include "rpg/culture/culture.h"
#include "rpg/sim/home.h"
#include "rpg/view/view.h"
#include "rpg/world/source.h"

namespace {
uint64_t hkey(uint64_t kind, uint64_t a, uint64_t b = 0, uint64_t c = 0) {
  return (kind << 56) ^ (a << 36) ^ (b << 16) ^ c ^ 0x484F4D45ull;   // "HOME"
}
// the culture whose style a plot's yard is built in: its house's, else its settlement's, else the land's (cached per
// plot id: the land's culture is a look-up in the atlas)
std::unordered_map<uint64_t, uint64_t> g_plotCulture;
const cult::Culture* plotCulture(const Game& g, const home::Plot& p) {
  if (!g.world.src) return nullptr;
  uint64_t id = p.style;
  if (!id) {
    const uint64_t key = p.id ^ ((uint64_t)(uint32_t)p.gx << 20) ^ (uint64_t)(uint32_t)p.gy;
    auto it = g_plotCulture.find(key);
    if (it != g_plotCulture.end()) id = it->second;
    else {
      const int si = p.site ? g.world.siteHandle(p.site) : -1;
      if (si >= 0)
        if (const cult::Culture* c = g.world.cultureOf(si)) id = c->id;
      if (!id) id = g.world.src->cultureAt(p.gx + p.w / 2, p.gy + p.h / 2);
      if (g_plotCulture.size() > 64) g_plotCulture.clear();
      g_plotCulture[key] = id;
    }
  }
  return id ? &g.world.src->culture(id) : nullptr;
}
// how full an animal house is (FarmObjLook::stage 0 empty .. 3 full)
int houseStage(const home::Plot& p, home::Obj kind) {
  int homes = 0, beasts = 0, cap = home::objInfo(kind).capacity;
  for (const home::PlacedObj& o : p.outside) if ((home::Obj)o.kind == kind) homes++;
  for (const home::AnimalRec& a : p.animals)
    if (!(a.flags & home::AF_LOST) && home::animalInfo((home::Animal)a.kind).home == kind) beasts++;
  if (!homes || !beasts || cap <= 0) return 0;
  return std::clamp((beasts * 3 + homes * cap - 1) / (homes * cap), 1, 3);
}
}  // namespace

void View::homeCollect(Game& g, const Map& m, Vec2 cam) {
  homeLights_.clear();
  if (g.home.plots.empty()) return;
  Pix& P = *pix_;
  // (a farm first seen paints ~60 sprites: they are spread over frames, ~3 ms of painting a frame; a piece not painted
  // yet waits a frame or two, the ghost and the building site never wait)
  double bakeMs = 0;
  auto tex = [&](uint64_t key, auto&& paint) -> const Tex& {
    auto it = homeTex_.find(key);
    if (it != homeTex_.end()) return it->second;
    const auto t0 = std::chrono::steady_clock::now();
    const Tex& t = homeTex_[key] = P.bake(paint());
    bakeMs += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    return t;
  };
  auto can = [&](uint64_t key) { return bakeMs < 3.0 || homeTex_.count(key) != 0; };
  const World& W = g.world;
  const home::Ui& U = g.home.ui;
  const bool building = g.mode == Mode::Build;
  // ---------------------------------------------------------------- indoors: the DECORATE ghost (the placed furniture
  // is stamped into the interior map as props by the sim and drawn with the room)
  if (m.kind != MapKind::Overworld) {
    if (!building || U.mode != home::UiMode::Decorate || U.plot < 0 || U.plot >= (int)g.home.plots.size() || U.sel < 0 ||
        U.sel >= (int)home::Obj::COUNT || U.justPlaced())   // (just placed: the new piece shows, not a red ghost over it)
      return;
    const home::Plot& p = g.home.plots[(size_t)U.plot];
    const home::Obj ok = (home::Obj)U.sel;
    const home::ObjInfo& oi = home::objInfo(ok);
    int w = oi.w, h = oi.h;
    if (U.turned && (oi.flags & home::OBJ_ROTATES)) std::swap(w, h);
    std::string why;
    const bool fits = home::canPlaceInside(p, m, ok, U.gx, U.gy, U.turned, why);
    const Color tint = fits ? Color(0.62f, 1.0f, 0.62f, 0.8f) : Color(1.0f, 0.48f, 0.42f, 0.8f);
    for (int y = 0; y < h; y++)
      for (int x = 0; x < w; x++) {
        HomeDraw c;
        c.ui = 1; c.tint = tint;
        c.x = (U.gx + x) * 16.0f; c.y = (U.gy + y) * 16.0f; c.sw = 16; c.sh = 16;
        c.sortY = 1e9f - 1;
        homeDraws_.push_back(c);
      }
    HomeDraw d;
    d.tint = Color(tint.r, tint.g, tint.b, 0.82f);
    d.sortY = 1e9f;
    if (ok == home::Obj::Trophy || oi.prop == art::Prop::COUNT) {
      const Tex& t = ok == home::Obj::Trophy ? tex(hkey(9, 1), [&] { return art::trophySprite(art::Monster::Wolf, 0); })
                                             : tex(hkey(9, 2), [&] { return art::paintingSprite(art::PaintingSpec()); });
      d.tex = &t; d.sw = t.w; d.sh = t.h;
    } else {
      const art::Prop pr = oi.prop;
      if ((int)pr >= (int)props_.size()) return;
      d.tex = &props_[(size_t)pr];
      d.sw = art::propW(pr); d.sh = props_[(size_t)pr].h;
    }
    d.x = U.gx * 16.0f + w * 8.0f - d.sw / 2.0f;
    d.y = (U.gy + h) * 16.0f - d.sh;
    homeDraws_.push_back(d);
    return;
  }
  // ---------------------------------------------------------------- the yards in view
  const float vx0 = cam.x - 64, vy0 = cam.y - 64, vx1 = cam.x + Pix::W + 64, vy1 = cam.y + Pix::H + 96;
  const bool night = g.daylight() < 0.6f;
  for (int pi = 0; pi < (int)g.home.plots.size(); pi++) {
    const home::Plot& p = g.home.plots[(size_t)pi];
    const int lx = p.gx - W.ox, ly = p.gy - W.oy;
    if ((lx + p.w) * 16 < vx0 || lx * 16 > vx1 || (ly + p.h) * 16 < vy0 || ly * 16 > vy1) continue;
    const cult::Culture* C = plotCulture(g, p);
    const art::ArchStyle* style = C ? &C->arch : nullptr;
    const uint64_t styleKey = C ? (C->id & 0xFFFFFFull) : 0;
    // (M7 fix r3, review: "forest-floor root lines run through the fenced yard") a kept yard over the woods' floor:
    // trodden grass in place of the litter, roots and veins (flats under the ground edits; faded toward the wild)
    {
      auto woods = [&](int x, int y) {
        if (x < 0 || y < 0 || x >= p.w || y >= p.h || !m.in(lx + x, ly + y)) return 0;
        // the woods' floor: the forest grounds, and grass the woods' biome paints as its litter (the jungle's loam
        // with its roots, the dark wood's rot...)
        const Ground gr = m.at(lx + x, ly + y);
        if (gr != Ground::ForestFloor && gr != Ground::Autumn && gr != Ground::Grass && gr != Ground::Meadow) return 0;
        const Eco e = m.ecoAt(lx + x, ly + y);
        const bool wood = (e >= Eco::MixedForest && e <= Eco::AutumnWood) || e == Eco::Taiga || e == Eco::TaigaBog || e == Eco::FloodedForest || e == Eco::Mangrove;
        if (!wood && gr != Ground::ForestFloor && gr != Ground::Autumn) return 0;
        return gr == Ground::Autumn || e == Eco::AutumnWood ? 2 : 1;
      };
      for (int y = 0; y < p.h; y++)
        for (int x = 0; x < p.w; x++) {
          const int k = woods(x, y);
          if (!k) continue;
          const float wx = (lx + x) * 16.0f, wy = (ly + y) * 16.0f;
          if (wx + 16 < cam.x || wy + 16 < cam.y || wx > cam.x + Pix::W || wy > cam.y + Pix::H) continue;
          // joined toward the yard's other woods tiles; faded toward the wild beyond the plot and toward the yard's
          // own clean meadow (no hard seam where the two grounds meet)
          auto yard = [&](int nx, int ny) { return woods(nx, ny) != 0; };
          uint8_t joins = 0;
          if (yard(x, y - 1)) joins |= 1;
          if (yard(x + 1, y)) joins |= 2;
          if (yard(x, y + 1)) joins |= 4;
          if (yard(x - 1, y)) joins |= 8;
          const int32_t tx = p.gx + x, ty = p.gy + y;   // (each tile its own: the grass's patches run on across tiles)
          const uint64_t key = hkey(10, (uint64_t)joins | ((uint64_t)k << 4), (uint64_t)(uint32_t)tx & 0xFFFFFu, ((uint64_t)(uint32_t)ty & 0x3FFFu) << 42);
          if (!can(key)) continue;
          const Tex& t = tex(key, [&] { return art::yardGroundTile(joins, k - 1, tx, ty); });
          HomeDraw d;
          d.sortY = -2e9f + (float)(ly + y);
          d.flat = true;
          d.tex = &t; d.sw = t.w; d.sh = t.h;
          d.x = wx; d.y = wy;
          homeDraws_.push_back(d);
        }
    }
    // ground edits: flats (drawn before the shadow pass)
    for (int y = 0; y < p.h; y++)
      for (int x = 0; x < p.w; x++) {
        const int k = home::groundAt(p, x, y);
        if (!k) continue;
        const float wx = (lx + x) * 16.0f, wy = (ly + y) * 16.0f;
        if (wx + 16 < cam.x || wy + 16 < cam.y || wx > cam.x + Pix::W || wy > cam.y + Pix::H) continue;
        uint8_t joins = 0;
        auto same = [&](int nx, int ny) { const int n = home::groundAt(p, nx, ny); return k == 1 ? n == 1 : (n == 2 || n == 3); };
        if (same(x, y - 1)) joins |= 1;
        if (same(x + 1, y)) joins |= 2;
        if (same(x, y + 1)) joins |= 4;
        if (same(x - 1, y)) joins |= 8;
        bool wet = k == 3;
        for (const home::CropRec& c : p.crops) if (c.x == x && c.y == y && g.day - (int)c.lastWaterDay <= 0) wet = true;
        const uint32_t v = (uint32_t)((p.gx + x) * 7 + (p.gy + y) * 13) & 3u;
        if (!can(k == 1 ? hkey(1, joins, v) : hkey(2, joins, v, wet))) continue;
        const Tex& t = k == 1 ? tex(hkey(1, joins, v), [&] { return art::pathTile(joins, 0, v); })
                              : tex(hkey(2, joins, v, wet), [&] { return art::farmlandTile(joins, wet, v); });
        HomeDraw d;
        d.sortY = -1e9f + (float)(ly + y);
        d.flat = true;
        d.tex = &t; d.sw = t.w; d.sh = t.h;
        d.x = wx; d.y = wy;
        homeDraws_.push_back(d);
      }
    // crops
    for (const home::CropRec& c : p.crops) {
      const bool wilt = c.wilted(g.day);
      const uint32_t v = (uint32_t)((p.gx + c.x) * 31 + (p.gy + c.y) * 17) & 7u;
      if (!can(hkey(3, c.kind, c.stage * 2u + (wilt ? 1u : 0u), v))) continue;
      const Tex& t = tex(hkey(3, c.kind, c.stage * 2u + (wilt ? 1u : 0u), v), [&] { return art::cropSprite((art::Crop)c.kind, c.stage, v, wilt); });
      HomeDraw d;
      d.tex = &t; d.sw = t.w; d.sh = t.h;
      d.x = (lx + c.x) * 16.0f + 8.0f - t.w / 2.0f;
      d.y = (ly + c.y) * 16.0f + 16.0f - t.h;
      d.sortY = (ly + c.y) * 16.0f + 13.0f;
      if (!wilt && c.stage >= 1) d.sway = c.stage >= 2 ? 1.0f : 0.6f;
      if (c.stage >= 2) {   // a grown crop shades the furrow a little (down-right)
        d.shadow = 3;
        d.shw = 12; d.shh = 4; d.shx = (lx + c.x) * 16.0f + 4.0f; d.shy = (ly + c.y) * 16.0f + 12.0f; d.shA = 0.45f;
      }
      homeDraws_.push_back(d);
    }
    // yard objects
    auto objAt = [&](int x, int y, home::Obj a, home::Obj b) {
      for (const home::PlacedObj& o : p.outside)
        if (o.x == x && o.y == y && ((home::Obj)o.kind == a || (home::Obj)o.kind == b)) return true;
      return false;
    };
    for (const home::PlacedObj& o : p.outside) {
      const home::Obj ok = (home::Obj)o.kind;
      const art::FarmObj fa = home::farmObjArt(ok);
      if (fa == art::FarmObj::COUNT) continue;
      const home::ObjInfo& oi = home::objInfo(ok);
      int w = oi.w, h = oi.h;
      if (o.turned() && (oi.flags & home::OBJ_ROTATES)) std::swap(w, h);
      const float bx = (lx + o.x) * 16.0f, by = (ly + o.y + h) * 16.0f;   // the footprint's bottom-left (world px)
      if (bx + w * 16 + 48 < cam.x || bx - 48 > cam.x + Pix::W || by + 16 < cam.y || by - 96 > cam.y + Pix::H) continue;
      art::FarmObjLook L;
      L.kind = fa;
      L.turned = o.turned();
      L.variant = (uint32_t)(o.x * 7 + o.y * 13) & 3u;
      L.style = style;
      if (oi.flags & home::OBJ_JOINS) {
        const home::Obj f = home::Obj::Fence, gt = home::Obj::Gate;
        if (objAt(o.x, o.y - 1, f, gt)) L.joins |= 1;
        if (objAt(o.x + 1, o.y, f, gt)) L.joins |= 2;
        if (objAt(o.x, o.y + 1, f, gt)) L.joins |= 4;
        if (objAt(o.x - 1, o.y, f, gt)) L.joins |= 8;
      }
      if (oi.flags & home::OBJ_ANIMALS) L.stage = (uint8_t)houseStage(p, ok);
      if (!can(hkey(4, (uint64_t)fa | styleKey << 8, L.joins | (L.turned ? 16u : 0u) | (uint64_t)L.stage << 5, L.variant))) continue;
      const Tex& t = tex(hkey(4, (uint64_t)fa | styleKey << 8, L.joins | (L.turned ? 16u : 0u) | (uint64_t)L.stage << 5, L.variant),
                         [&] { return art::farmObjSprite(L); });
      HomeDraw d;
      d.tex = &t; d.sw = t.w; d.sh = t.h;
      d.x = bx + w * 8.0f - t.w / 2.0f;
      d.y = by - t.h;
      d.sortY = by - 1.0f;
      // the ground shadow, thrown down-right (top-left light): footprint-wide under the solid ones, small under posts
      const bool post = ok == home::Obj::Lantern || ok == home::Obj::Scarecrow || ok == home::Obj::Banner || ok == home::Obj::Sapling ||
                        ok == home::Obj::Statue || ok == home::Obj::Beehive;
      if (ok == home::Obj::Fence || ok == home::Obj::Gate) {
        // a fence's shade: a soft band along the run's foot (east-west), or down its east side (a north-south run)
        d.shadow = 3;
        d.shA = 0.55f;   // (M7 fix) a visible shade (0.4 was lost on grass)
        if ((L.joins & 5) && !(L.joins & 10)) { d.shw = 6; d.shh = 18; d.shx = bx + 8.0f; d.shy = by - 17.0f; }
        else { d.shw = 18; d.shh = 4; d.shx = bx + 1.0f; d.shy = by - 4.0f; }
      } else if (post) {
        d.shadow = 3;
        d.shw = 14; d.shh = 5; d.shx = bx + 4.0f; d.shy = by - 5.0f; d.shA = 0.6f;
      } else if (ok != home::Obj::FlowerBed) {
        const float sw = w * 16.0f * 0.92f + 6, sh = std::max(6.0f, std::min(h * 16.0f * 0.5f, sw * 0.28f));
        d.shadow = 3;
        // (M7 fix) thrown further down-right, out from under the sprite (it hid under the trough, the rack, the crate)
        d.shw = sw; d.shh = sh; d.shx = bx + w * 8.0f - sw / 2 + 5; d.shy = by - sh * 0.45f; d.shA = 0.78f;
      }
      homeDraws_.push_back(d);
      // lights: a lantern's flame and pool, a campfire's flicker (and its smoke)
      if (ok == home::Obj::Lantern && night) {
        const float k = 0.9f + 0.1f * std::sin(t_ * 8 + o.x * 1.3f);
        homeLights_.push_back({Vec2(bx + 8.0f, by - t.h + 6.0f), 30, Color(1.0f, 0.92f, 0.66f), 1.0f * k});
        homeLights_.push_back({Vec2(bx + 8.0f, by - 6.0f), 88, Color(1.0f, 0.70f, 0.38f), 0.78f * k});
      } else if (ok == home::Obj::Forge && night) {
        // (M7 fix r3, review: "the yard forge gives no light at night") its fire bed (paintForge: on the hearth block
        // under the hood, left of the anvil) glows by night as it does by day
        const float k = 0.85f + 0.15f * std::sin(t_ * 11 + o.x) * std::sin(t_ * 5.3f + o.y);
        const float fx = bx + 13.0f, fy = by - 30.0f;
        homeLights_.push_back({Vec2(fx, fy), 26, Color(1.0f, 0.74f, 0.40f), 1.0f * k});
        homeLights_.push_back({Vec2(fx, fy + 14.0f), 78, Color(1.0f, 0.50f, 0.22f), 0.6f * k});
      } else if (ok == home::Obj::Campfire) {
        const float k = 0.82f + 0.18f * std::sin(t_ * 13 + o.y) * std::sin(t_ * 7.1f + o.x);
        if (night) {
          homeLights_.push_back({Vec2(bx + 8.0f, by - 6.0f), 34, Color(1.0f, 0.78f, 0.42f), 0.95f * k});
          homeLights_.push_back({Vec2(bx + 8.0f, by - 2.0f), 92, Color(1.0f, 0.55f, 0.25f), 0.62f * k});
        }
        if (hashf((int)(t_ * 12), o.x * 7 + o.y, 77) < 0.18f && parts_.size() < 560) {   // a wisp of smoke, now and then
          Particle q;
          q.p = Vec2(bx + 7.0f + hashf((int)(t_ * 31), o.x, 3) * 3, by - 10.0f);
          q.v = Vec2(2.0f + hashf((int)(t_ * 17), o.y, 5) * 4, -10.0f - hashf((int)(t_ * 13), o.x, 9) * 4);
          q.life = q.max = 2.4f;
          q.c = Color(0.72f, 0.70f, 0.70f);
          q.size = 1; q.grav = -1.2f; q.kind = 2;
          parts_.push_back(q);
        }
      }
    }
    // the building site of a house going up (centred on its footprint; the same anchor as the finished sprite)
    if (p.kind == home::PlotKind::Lot && p.state == home::PlotState::Building && p.hw) {
      const home::ShellInfo& si = home::shellInfo((home::Shell)p.shell);
      const int span = std::max(1, (int)p.buildDoneDay - (int)p.buildStartDay);
      const float hours = (float)(g.day - (int)p.buildStartDay) * 24.0f + g.hour;
      const int prog = std::clamp((int)(hours * 4.0f / (span * 24.0f)), 0, 3);
      const Tex& t = tex(hkey(5, p.hw * 16u + p.hh, si.storeys, (uint64_t)prog | (uint64_t)p.houseSeed << 4), [&] {
        return art::scaffoldSprite(p.hw, p.hh, si.storeys, prog, p.houseSeed);
      });
      const float fx = (lx + p.hx) * 16.0f, fb = (ly + p.hy + p.hh) * 16.0f;
      HomeDraw d;
      d.tex = &t; d.sw = t.w; d.sh = t.h;
      d.x = fx + p.hw * 8.0f - t.w / 2.0f;
      d.y = fb + (t.w > p.hw * 16 ? (float)art::BLDG_PAD_B : 0.0f) - t.h;
      d.sortY = fb - 1.0f;
      homeDraws_.push_back(d);
    }
    // the plot's border while its yard is being laid out
    if (building && U.mode == home::UiMode::Yard && U.plot == pi) {
      HomeDraw b;
      b.ui = 2;
      b.x = lx * 16.0f; b.y = ly * 16.0f; b.sw = p.w * 16; b.sh = p.h * 16;
      b.tint = Color(0.98f, 0.84f, 0.45f, 0.9f);
      b.sortY = 1e9f - 2;
      homeDraws_.push_back(b);
    }
  }
  // ---------------------------------------------------------------- the build ghost (the yard)
  if (building && U.mode == home::UiMode::Yard && U.plot >= 0 && U.plot < (int)g.home.plots.size() && U.sel >= 0 &&
      U.sel < (int)home::Obj::COUNT && !U.justPlaced()) {
    const home::Plot& p = g.home.plots[(size_t)U.plot];
    const home::Obj ok = (home::Obj)U.sel;
    const art::FarmObj fa = home::farmObjArt(ok);
    std::string why;
    const bool fits = home::canPlaceOutside(p, ok, U.gx, U.gy, U.turned, why);
    const int lx = p.gx - W.ox + U.gx, ly = p.gy - W.oy + U.gy;
    const home::ObjInfo& oi = home::objInfo(ok);
    int w = oi.w, h = oi.h;
    if (U.turned && (oi.flags & home::OBJ_ROTATES)) std::swap(w, h);
    const Color tint = fits ? Color(0.62f, 1.0f, 0.62f, 0.8f) : Color(1.0f, 0.48f, 0.42f, 0.8f);
    for (int y = 0; y < h; y++)
      for (int x = 0; x < w; x++) {
        HomeDraw c;
        c.ui = 1; c.tint = tint;
        c.x = (lx + x) * 16.0f; c.y = (ly + y) * 16.0f; c.sw = 16; c.sh = 16;
        c.sortY = 1e9f - 1;
        homeDraws_.push_back(c);
      }
    HomeDraw d;
    d.tint = Color(tint.r, tint.g, tint.b, fits ? 0.8f : 0.6f);   // red: see-through, so what is in the way shows
    d.sortY = 1e9f;
    if (fa != art::FarmObj::COUNT) {
      const cult::Culture* C = plotCulture(g, p);
      art::FarmObjLook L;
      L.kind = fa; L.turned = U.turned; L.style = C ? &C->arch : nullptr;
      const uint64_t styleKey = C ? (C->id & 0xFFFFFFull) : 0;
      const Tex& t = tex(hkey(4, (uint64_t)fa | styleKey << 8, L.turned ? 16u : 0u, 0), [&] { return art::farmObjSprite(L); });
      d.tex = &t; d.sw = t.w; d.sh = t.h;
      d.x = lx * 16.0f + w * 8.0f - t.w / 2.0f;
      d.y = (ly + h) * 16.0f - t.h;
    } else {   // a ground edit: the tile itself
      const Tex& t = ok == home::Obj::Path ? tex(hkey(1, 0, 0), [&] { return art::pathTile(0, 0, 0); })
                                           : tex(hkey(2, 0, 0, 0), [&] { return art::farmlandTile(0, false, 0); });
      d.tex = &t; d.sw = t.w; d.sh = t.h;
      d.x = lx * 16.0f; d.y = ly * 16.0f;
    }
    homeDraws_.push_back(d);
  }
}

// the ground edits, before the shadow pass (drawWorld)
void View::drawHomeFlats(Vec2 cam) {
  Pix& P = *pix_;
  for (const HomeDraw& d : homeDraws_) {
    if (!d.flat || !d.tex) continue;
    P.blitEx(*d.tex, d.sx, d.sy, d.sw, d.sh, std::floor(d.x - cam.x + 0.5f), std::floor(d.y - cam.y + 0.5f), (float)d.sw, (float)d.sh, false, d.tint);
  }
}

// a home draw's ground shadow (the shadow pass)
void View::drawHomeShadow(const HomeDraw& d, Vec2 cam) {
  if (!d.shadow) return;
  Pix& P = *pix_;
  if (d.shadow == 3) {
    P.blitEx(shadowBig_, 0, 0, 40, 12, std::floor(d.shx - cam.x + 0.5f), std::floor(d.shy - cam.y + 0.5f), d.shw, d.shh, false, Color(1, 1, 1, d.shA));
    return;
  }
}

void View::drawHomeDraw(const HomeDraw& d, Vec2 cam) {
  Pix& P = *pix_;
  const float x = std::floor(d.x - cam.x + 0.5f), y = std::floor(d.y - cam.y + 0.5f);
  if (d.ui == 1) {   // a footprint cell of the ghost: a tinted wash and a crisp rim
    P.rect(x + 1, y + 1, (float)d.sw - 2, (float)d.sh - 2, Color(d.tint.r, d.tint.g, d.tint.b, 0.22f));
    P.frame(x, y, (float)d.sw, (float)d.sh, Color(d.tint.r * 0.9f, d.tint.g * 0.9f, d.tint.b * 0.9f, 0.85f));
    P.rect(x, y, 3, 1, Color(1, 1, 1, 0.6f));
    P.rect(x, y, 1, 3, Color(1, 1, 1, 0.6f));
    return;
  }
  if (d.ui == 2) {   // the plot's border: a dashed gold line just inside it, marching slowly
    const int off = (int)(t_ * 6) % 8;
    const Color c = d.tint, dk(0.1f, 0.07f, 0.03f, 0.55f);
    for (int i = -off; i < d.sw; i += 8) {
      const float a = std::max(0.0f, (float)i), b = std::min((float)d.sw, (float)i + 4);
      if (b <= a) continue;
      P.rect(x + a, y + 1, b - a, 1, dk); P.rect(x + a, y, b - a, 1, c);
      P.rect(x + a, y + d.sh, b - a, 1, dk); P.rect(x + a, y + d.sh - 1, b - a, 1, c);
    }
    for (int i = -off; i < d.sh; i += 8) {
      const float a = std::max(0.0f, (float)i), b = std::min((float)d.sh, (float)i + 4);
      if (b <= a) continue;
      P.rect(x + 1, y + a, 1, b - a, dk); P.rect(x, y + a, 1, b - a, c);
      P.rect(x + d.sw, y + a, 1, b - a, dk); P.rect(x + d.sw - 1, y + a, 1, b - a, c);
    }
    return;
  }
  if (!d.tex) return;
  // (phase A's shadow kinds 1 and 2 go down with the sprite; the VIEW lane's kind 3 went down in the shadow pass)
  if (d.shadow == 2) P.blitEx(shadowBig_, 0, 0, 40, 12, x + 2, y + d.sh - 7, (float)d.sw - 2, 8, false, Color(1, 1, 1, 0.55f));
  else if (d.shadow == 1) P.blitEx(shadow_, 0, 0, shadow_.w, shadow_.h, x + d.sw / 2.0f - 6, y + d.sh - 4, 12, 5, false, Color(1, 1, 1, 0.6f));
  if (d.sway > 0) {
    // the top of the plant leans with the wind: a slow wave that runs across the field (phase from the tile's x)
    const float ph = (d.x + d.y * 0.37f) * 0.11f;
    const float s = std::sin(t_ * 1.7f - ph) * d.sway + std::sin(t_ * 0.63f - ph * 0.5f) * 0.35f * d.sway;
    const int off = (int)std::lround(s);
    const int split = d.sh * 11 / 20;   // the upper rows sway, the stalks' foot stays put
    if (off != 0) {
      P.blitEx(*d.tex, d.sx, d.sy, d.sw, split, x + off, y, (float)d.sw, (float)split, d.flip, d.tint);
      P.blitEx(*d.tex, d.sx, d.sy + split, d.sw, d.sh - split, x, y + split, (float)d.sw, (float)(d.sh - split), d.flip, d.tint);
      return;
    }
  }
  P.blitEx(*d.tex, d.sx, d.sy, d.sw, d.sh, x, y, (float)d.sw, (float)d.sh, d.flip, d.tint);
}

// ---------------------------------------------------------------- the rider
namespace {
// the horse's frame: idle 0, walk 1-4, gallop 5-7, paced by the ground speed
int horseFrame(const Actor& a, float t, bool& gallop) {
  const float sp = len(a.vel);
  gallop = false;
  if (a.st != AState::Walk || sp < 6.0f) return 0;
  gallop = sp > home::WALK_SPEED_PX * 1.3f;
  if (gallop) return 5 + (int)(t * sp / 13.0f) % 3;
  return 1 + (int)(t * std::max(40.0f, sp) / 9.0f) % 4;
}
}  // namespace

void View::drawMountShadow(Game& g, const Actor& a, Vec2 cam) {
  (void)g;
  Pix& P = *pix_;
  const bool side = a.face >= 2;
  const float w = side ? 32.0f : 18.0f, h = side ? 8.0f : 10.0f;
  P.blitEx(shadowBig_, 0, 0, 40, 12, std::floor(a.p.x - w / 2 + 2 - cam.x + 0.5f), std::floor(a.p.y - h / 2 + 1 - cam.y + 0.5f), w, h, false,
           Color(1, 1, 1, 0.85f));
}

bool View::drawMounted(Game& g, const Actor& a, Vec2 cam) {
  Pix& P = *pix_;
  rideGhost_ = RideGhost();
  const home::BreedInfo& bi = home::breedInfo((home::Breed)g.home.ridingBreed);
  const uint32_t seedv = g.home.ridingSeed;
  const uint64_t key = hkey(6, bi.coat & 0xFFFFFFu, seedv & 0xFFFFu, 1);
  auto it = homeTex_.find(key);
  if (it == homeTex_.end()) it = homeTex_.emplace(key, P.bake(art::horseSheet(bi.coat, seedv, true))).first;
  const Tex& ht = it->second;
  const int row = a.face == 0 ? 0 : a.face == 1 ? 1 : 2;
  const bool flip = a.face == 3;
  bool gallop = false;
  const int fr = horseFrame(a, t_, gallop);
  float alpha = 1.0f;
  if (a.iframes > 0 && a.st != AState::Roll && ((int)(t_ * 20) & 1)) alpha = 0.5f;
  const float flash = a.flash > 0 ? a.flash / 0.12f : 0.0f;
  const float fx = std::floor(a.p.x - cam.x + 0.5f), fy = std::floor(a.p.y + 2 - cam.y + 0.5f);
  const float hx = fx - art::HORSE_W / 2.0f, hy = fy - art::HORSE_H + 2;
  const int cx = fr * art::HORSE_W, cy = row * art::HORSE_H;
  // a part of the horse's cell (rows y0.. and columns x0..x0+w of the cell), flipped with the cell
  auto horsePart = [&](int x0, int y0, int w, int h, Color c, int blend) {
    if (w <= 0 || h <= 0) return;
    const int dx0 = flip ? art::HORSE_W - x0 - w : x0;
    P.blitEx(ht, cx + x0, cy + y0, w, h, hx + dx0, hy + y0, (float)w, (float)h, flip, c, blend);
  };
  auto horse = [&](Color c, int blend) { horsePart(0, 0, art::HORSE_W, art::HORSE_H, c, blend); };
  int dx = 0, dy = 0;
  art::riderSeat(row, fr, dx, dy);
  if (flip) dx = -dx;
  const Tex* rt = poseTex(a.look, art::Posture::Ride);
  const Tex& rider = rt ? *rt : humanTex(a.look);
  const float rx = std::floor(fx + dx - art::HUMAN_W / 2.0f), ry = std::floor(fy + dy - art::HUMAN_H + 2 + art::postureInfo(art::Posture::Ride).dy);
  const int rfr = rt ? 0 : 0;
  auto body = [&](Color c, int blend) {
    P.blitEx(rider, rfr * art::HUMAN_W, row * art::HUMAN_H, art::HUMAN_W, art::HUMAN_H, rx, ry, (float)art::HUMAN_W, (float)art::HUMAN_H, flip, c, blend);
  };
  // the seat in the cell's rows: what lies below it on the screen is in front of the rider's legs (facing up: the
  // rump; facing down: the chest, neck and head, which rise in front of the rider's belly)
  const int seatRow = std::clamp(art::HORSE_H - 2 + dy, 0, art::HORSE_H);
  const Color full(1, 1, 1, alpha);
  auto layer = [&](Color c, int blend) {
    horse(c, blend);
    body(c, blend);
    if (row == 1) horsePart(0, seatRow + 1, art::HORSE_W, art::HORSE_H - seatRow - 1, c, blend);
    else if (row == 0) {
      horsePart(art::HORSE_W / 2 - 6, std::max(0, seatRow - 5), 12, art::HORSE_H - std::max(0, seatRow - 5), c, blend);
      // (M7 fixer r2) the head carried high in front of the rider's chest: the sheet's head-only row over everything
      P.blitEx(ht, cx, 3 * art::HORSE_H, art::HORSE_W, art::HORSE_H, hx, hy, (float)art::HORSE_W, (float)art::HORSE_H, flip, c, blend);
    }
  };
  layer(full, 0);
  if (flash > 0) layer(Color(1, 1, 1, flash), 1);
  // a gallop throws up a little dust behind the hooves
  if (gallop && hashf((int)(t_ * 30), a.id, 41) < 0.35f && parts_.size() < 560) {
    Particle q;
    const Vec2 back = a.face == 0 ? Vec2(0, -6) : a.face == 1 ? Vec2(0, 4) : a.face == 2 ? Vec2(-12, 0) : Vec2(12, 0);
    q.p = a.p + back + Vec2(hashf((int)(t_ * 57), a.id, 3) * 6 - 3, -1);
    q.v = Vec2(back.x * 0.8f, -6.0f);
    q.life = q.max = 0.5f;
    q.c = Color(0.62f, 0.54f, 0.42f);
    q.size = 2; q.grav = 0; q.kind = 0;
    parts_.push_back(q);
  }
  rideGhost_.t = &rider; rideGhost_.fr = rfr; rideGhost_.row = row; rideGhost_.flip = flip; rideGhost_.x = rx; rideGhost_.y = ry;
  drawnHead_[a.id] = DrawnHead{fx + cam.x, ry + cam.y, t_};
  return true;
}
