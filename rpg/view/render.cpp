// World rendering: sprite bank, y-sorted scene, effects, lighting, weather, event handling.
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include "rpg/sim/deco.h"
#include "rpg/view/prop_traits.h"
#include "rpg/view/view.h"
#include "rpg/world/economy.h"
#include "rpg/world/source.h"

using art::Prop;
using art::Monster;

namespace {
Color col(uint32_t c, float a = 1) { return Color((c & 255) / 255.0f, ((c >> 8) & 255) / 255.0f, ((c >> 16) & 255) / 255.0f, a); }
Canvas paintInteriorPiece(uint64_t key) { return art::interiorPiece((uint32_t)key); }
Canvas paintStall(uint64_t key) { return art::marketStallVariant((int)(key & 255)); }
Canvas paintTradeStall(uint64_t key) {
  return art::marketStallForm((int)(key & 15), (int)((key >> 4) & 15), (int)((key >> 8) & 15), ((key >> 12) & 1) != 0);
}
Canvas paintMarketTable(uint64_t key) { return art::marketTable((int)(key & 15), (int)((key >> 4) & 15), ((key >> 8) & 1) != 0); }
Canvas paintGroundCloth(uint64_t key) { return art::groundCloth((int)(key & 15), (int)((key >> 4) & 15), ((key >> 8) & 1) != 0); }
Canvas paintMineRail(uint64_t key) { return art::mineRail((int)(key & 15)); }
Canvas paintMineHill(uint64_t key) { return art::mineHill((int)(key & 3), (int)((key >> 2) & 3)); }
Canvas paintRuin(uint64_t key) { return art::ruinVariant((Prop)((key >> 8) & 255), (int)(key & 255)); }

// M0b interiors: stairs and doors in the room's material, wall decor fitted to a partition's short face
// (interiorPropKey, rpg/sim/deco.h). A door into a private room (a bedroom, a guest room, the stockroom) stands shut
// until someone comes near it; other doors stand open.
uint32_t interiorPropTexKey(const Game& g, const Map& m, int tx, int ty, Prop p) {
  uint32_t key = interiorPropKey(m, tx, ty, p);
  if (!key || (p != Prop::DoorH && p != Prop::DoorV)) return key;
  bool priv = false;
  auto side = [&](int x, int y) {
    int ri = m.roomIndexAt(x, y);
    if (ri >= 0 && ri < (int)m.rooms.size() && roomPrivate(m.rooms[(size_t)ri].kind)) priv = true;
  };
  if (p == Prop::DoorH) { side(tx, ty + 1); side(tx, ty - 2); }
  else { side(tx - 1, ty); side(tx + 1, ty); }
  if (!priv) return key;
  Vec2 c(tx * 16 + 8.0f, (p == Prop::DoorH ? ty * 16.0f : ty * 16 + 8.0f));
  for (const Actor& a : g.actors)
    if (a.st != AState::Dead && std::fabs(a.p.x - c.x) < 20 && std::fabs(a.p.y - c.y) < 26) return key;
  return key | (2u << 24);   // variant bit 2: shut
}
}  // namespace

bool View::init(Pix& pix, Audio& audio) {
  pix_ = &pix;
  audio_ = &audio;
  for (int i = 0; i < (int)Prop::COUNT; i++) props_.push_back(pix.bake(art::propSprite((Prop)i)));
  for (int i = 0; i < (int)Monster::COUNT; i++) monsters_.push_back(pix.bake(art::monsterSheet((Monster)i)));
  for (int i = 0; i < (int)art::Fx::COUNT; i++) fx_.push_back(pix.bake(art::fxSprite((art::Fx)i)));
  gateTex_ = pix.bake(art::gateHouse(7));
  gate_ = gateTex_;
  {
    Canvas c(14, 6);
    for (int y = 0; y < 6; y++)
      for (int x = 0; x < 14; x++) {
        float dx = (x - 6.5f) / 7.0f, dy = (y - 2.5f) / 3.0f;
        float d = dx * dx + dy * dy;
        if (d < 1) c.set(x, y, rgba(20, 16, 30, (int)(110 * (1 - d * 0.5f))));
      }
    shadow_ = pix.bake(c);
    Canvas b(40, 12);
    for (int y = 0; y < 12; y++)
      for (int x = 0; x < 40; x++) {
        float dx = (x - 19.5f) / 20.0f, dy = (y - 5.5f) / 6.0f;
        float d = dx * dx + dy * dy;
        if (d < 1) b.set(x, y, rgba(20, 16, 30, (int)(100 * (1 - d * 0.6f))));
      }
    shadowBig_ = pix.bake(b);
  }
  {
    Canvas l(64, 64);
    for (int y = 0; y < 64; y++)
      for (int x = 0; x < 64; x++) {
        float d = std::hypot(x - 31.5f, y - 31.5f) / 32.0f;
        float a = d < 1 ? (1 - d) * (1 - d) : 0;
        l.set(x, y, rgba(255, 255, 255, (int)(255 * a)));
      }
    light_ = pix.bake(l);
    Canvas w(64, 64);
    for (int y = 0; y < 64; y++)
      for (int x = 0; x < 64; x++) {
        float n = vnoise(x / 6.0f, y / 3.0f, 777);
        // tileable-ish: fade at edges handled by the scroll; sparse highlight streaks
        if (n > 0.70f && (y % 3) == 0) w.set(x, y, rgba(170, 215, 245, (int)((n - 0.7f) * 600)));
      }
    water_ = pix.bake(w);
    Canvas v(120, 68);
    for (int y = 0; y < 68; y++)
      for (int x = 0; x < 120; x++) {
        float dx = (x - 59.5f) / 60.0f, dy = (y - 33.5f) / 34.0f;
        float d = std::sqrt(dx * dx + dy * dy);
        float a = smooth01(clampf((d - 0.65f) / 0.5f, 0, 1));
        v.set(x, y, rgba(8, 6, 16, (int)(150 * a)));
      }
    vignette_ = pix.bake(v);
    Canvas wt(1, 1); wt.set(0, 0, rgba(255, 255, 255));
    white_ = pix.bake(wt);
  }
  lightMap_ = pix.makeTarget((Pix::W + 1) / 2, (Pix::H + 1) / 2);
  miniPx_.assign(64 * 64, 0);
  pix.miniInit(64, 64);
  return true;
}

const Tex& View::humanTex(const art::HumanLook& L) {
  uint64_t k = L.key();
  humanUsed_[k] = t_;
  auto it = humans_.find(k);
  if (it != humans_.end()) return it->second;
  return humans_[k] = pix_->bake(art::humanSheet(L));
}

// Least-recently-used trimming of the sprite caches (called once per frame before anything is drawn, so no reference
// handed out earlier in the frame is invalidated): building sprites past 192 (with their night variant, smoke, window
// and fade facts), character sheets past 320. Only entries unused for a few seconds go.
void View::trimCaches() {
  if (t_ - lruT_ < 1.0f) return;
  lruT_ = t_;
  auto trim = [&](std::unordered_map<uint64_t, Tex>& tex, std::unordered_map<uint64_t, float>& used, size_t cap, auto&& extra) {
    if (tex.size() <= cap) return;
    std::vector<std::pair<float, uint64_t>> age;
    age.reserve(tex.size());
    for (auto& kv : tex) {
      auto u = used.find(kv.first);
      age.push_back({u == used.end() ? -1.0f : u->second, kv.first});
    }
    std::sort(age.begin(), age.end());
    size_t drop = tex.size() - cap * 3 / 4;   // trim to three quarters, so this does not run every second
    for (size_t i = 0; i < drop && i < age.size(); i++) {
      if (t_ - age[i].first < 4.0f) break;   // still in use
      const uint64_t k = age[i].second;
      auto it = tex.find(k);
      if (it != tex.end()) { pix_->destroy(it->second); tex.erase(it); }
      used.erase(k);
      extra(k);
    }
  };
  trim(bldgTex_, bldgUsed_, 192, [&](uint64_t k) {
    auto n = bldgNight_.find(k);
    if (n != bldgNight_.end()) { pix_->destroy(n->second); bldgNight_.erase(n); }
    bldgSmoke_.erase(k); bldgTopRow_.erase(k); bldgWin_.erase(k);
  });
  trim(humans_, humanUsed_, 320, [](uint64_t) {});
}

// Buildings whose sprite may show in a rectangle of map pixels. Large maps (an endless window collects every
// building of the session's nearby towns) keep an index per 32 x 32-tile cell; small ones are simply scanned.
void View::bldgsIn(const Game& g, const Map& m, float x0, float y0, float x1, float y1, std::vector<int>& out) {
  out.clear();
  const int n = (int)m.bldgs.size();
  auto overlaps = [&](const Bldg& b) {
    return !(b.r.x * 16 - 16 > x1 || (b.r.x + b.r.w) * 16 + 16 < x0 || (b.r.y - 6) * 16 > y1 || (b.r.y + b.r.h) * 16 + 8 < y0);
  };
  if (n < 48) {
    for (int i = 0; i < n; i++) if (overlaps(m.bldgs[i])) out.push_back(i);
    return;
  }
  const int key = g.mode == Mode::Title ? 0 : g.mapKey();
  if (bgridMap_ != &m || bgridKey_ != key || bgridN_ != m.bldgs.size() || bgridOX_ != g.world.ox || bgridOY_ != g.world.oy ||
      bgridW_ != (m.w + 31) / 32 || bgridH_ != (m.h + 31) / 32) {
    bgridMap_ = &m; bgridKey_ = key; bgridN_ = m.bldgs.size(); bgridOX_ = g.world.ox; bgridOY_ = g.world.oy;
    bgridW_ = (m.w + 31) / 32; bgridH_ = (m.h + 31) / 32;
    bgrid_.assign((size_t)bgridW_ * bgridH_, std::vector<int>());
    for (int i = 0; i < n; i++) {
      const Bldg& b = m.bldgs[i];
      const int cx0 = std::max(0, (b.r.x - 1) / 32), cx1 = std::min(bgridW_ - 1, (b.r.x + b.r.w + 1) / 32);
      const int cy0 = std::max(0, (b.r.y - 6) / 32), cy1 = std::min(bgridH_ - 1, (b.r.y + b.r.h + 1) / 32);
      if (b.r.x + b.r.w + 1 < 0 || b.r.y + b.r.h + 1 < 0) continue;   // a record of a town outside the window
      for (int cy = cy0; cy <= cy1; cy++)
        for (int cx = cx0; cx <= cx1; cx++) bgrid_[(size_t)cy * bgridW_ + cx].push_back(i);
    }
    bstamp_.assign((size_t)n, 0);
    bstampN_ = 0;
  }
  if (++bstampN_ == 0) { std::fill(bstamp_.begin(), bstamp_.end(), 0u); bstampN_ = 1; }
  const int cx0 = std::max(0, (int)std::floor(x0 / 512) - 1), cx1 = std::min(bgridW_ - 1, (int)std::floor(x1 / 512) + 1);
  const int cy0 = std::max(0, (int)std::floor(y0 / 512) - 1), cy1 = std::min(bgridH_ - 1, (int)std::floor(y1 / 512) + 1);
  for (int cy = cy0; cy <= cy1; cy++)
    for (int cx = cx0; cx <= cx1; cx++)
      for (int i : bgrid_[(size_t)cy * bgridW_ + cx]) {
        if (bstamp_[(size_t)i] == bstampN_) continue;
        bstamp_[(size_t)i] = bstampN_;
        if (overlaps(m.bldgs[i])) out.push_back(i);
      }
  std::sort(out.begin(), out.end());
}

// EMB_PERF=1: once a second, the frame times, the terrain bakes (background and inline) and the texture caches
void View::perfTick(float dt) {
  static const bool on = std::getenv("EMB_PERF") != nullptr;
  if (!on) return;
  perf_.frames++;
  perf_.frameMs += dt * 1000.0f;
  perf_.worstFrameMs = std::max(perf_.worstFrameMs, (double)dt * 1000.0);
  perf_.t += dt;
  if (perf_.t < 1.0f) return;
  std::printf("perf: frame avg %.1f ms worst %.1f ms | bakes %d (+%d inline) %.1f ms, worst %.1f ms | chunks %zu, bldg tex %zu, humans %zu, wall tiles %zu\n",
              perf_.frameMs / std::max(1, perf_.frames), perf_.worstFrameMs, perf_.bakes, perf_.inlineBakes, perf_.bakeMs, perf_.worstBakeMs,
              chunks_.size(), bldgTex_.size(), humans_.size(), wallTiles_.size());
  std::fflush(stdout);
  perf_ = Perf();
}
const Tex& View::iconTex(art::Icon i, uint32_t tint) {
  uint64_t k = (uint64_t)i << 32 | tint;
  auto it = icons_.find(k);
  if (it != icons_.end()) return it->second;
  return icons_[k] = pix_->bake(art::itemIcon(i, tint));
}
// a building's look: the style of the biome it stands in (the M0 stand-in for its culture), tinted by Bldg::roof.
// Test hook: EMB_ARCH_BIOME=<Biome index> paints every building in that biome's style (screenshots of snow, desert
// and swamp architecture from any start village); unset in normal play.
static int archBiomeOverride() {
  static int v = [] { const char* e = std::getenv("EMB_ARCH_BIOME"); return e ? std::atoi(e) : -1; }();
  return v;
}
// (M1) the building's own biome (Bldg::biome, where it stands), not the tile's: an endless window may not cover it
static art::ArchStyle bldgStyle(const Map& m, const Bldg& b) {
  (void)m;
  int biome = archBiomeOverride() >= 0 ? archBiomeOverride() : (int)b.biome;
  return art::withRoofTint(art::urbanize(art::archForBiome(biome, b.seed), b.urban, b.seed), b.roof);
}
uint64_t View::bldgKey(const Map& m, const Bldg& b, int index) const {
  uint64_t k = bldgStyle(m, b).key();
  k ^= (uint64_t)index * 0x9E3779B97F4A7C15ull;
  k ^= ((uint64_t)b.r.w << 8 | (uint64_t)b.r.h << 16 | (uint64_t)b.type << 24 | (uint64_t)b.seed << 32);
  k ^= ((uint64_t)b.storeys << 1 | (uint64_t)(b.hearth ? 1 : 0)) * 0xC2B2AE3D27D4EB4Full;   // M0b facts
  k ^= ew::mix64(((uint64_t)b.banner << 32 | b.banner2) ^ ((uint64_t)b.emblem << 56));     // M1 kingdom banner
  return k;
}
const Tex& View::bldgTex(const Bldg& b, int index) {
  static const Map empty;
  const Map& m = bldgMap_ ? *bldgMap_ : empty;
  uint64_t k = bldgKey(m, b, index);
  bldgUsed_[k] = t_;
  auto it = bldgTex_.find(k);
  if (it != bldgTex_.end()) return it->second;
  art::BuildingInfo info;
  auto t0 = std::chrono::steady_clock::now();
  Canvas c = art::buildingSprite(b.type, b.r.w, b.r.h, bldgStyle(m, b), b.seed, &info, bldgFacts(b));
  if (std::getenv("EMB_TIMING")) {
    static double total = 0;
    static int n = 0;
    total += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    std::printf("building %d painted: %d so far, %.1f ms total\n", index, ++n, total);
  }
  std::vector<Vec2>& smoke = bldgSmoke_[k];
  smoke.clear();
  for (int i = 0; i < info.smokeN; i++) smoke.push_back(Vec2((float)info.smokeX[i], (float)info.smokeY[i]));
  int topRow = 0;
  for (bool found = false; topRow < c.h && !found; topRow++)
    for (int x = 0; x < c.w; x++) if ((c.get(x, topRow) >> 24) > 96) { found = true; break; }
  bldgTopRow_[k] = topRow;
  // night: the lit-window variant and one light pool per window (centre of each connected run of panes)
  std::vector<Vec2>& wins = bldgWin_[k];
  wins.clear();
  bool anyGlass = false;
  for (uint8_t v : info.glass) if (v) { anyGlass = true; break; }
  if (anyGlass) {
    bldgNight_[k] = pix_->bake(art::buildingNight(c, info.glass, b.seed));
    std::vector<uint8_t> seen(info.glass.size(), 0);
    for (int y = 0; y < c.h; y++)
      for (int x = 0; x < c.w; x++) {
        size_t i = (size_t)y * c.w + x;
        if (!info.glass[i] || seen[i]) continue;
        // flood the pane group (8-connected, through the mullion gap of a pixel or two)
        std::vector<int> q{(int)i};
        seen[i] = 1;
        float sx = 0, sy = 0;
        for (size_t h = 0; h < q.size(); h++) {
          int qx = q[h] % c.w, qy = q[h] / c.w;
          sx += qx; sy += qy;
          for (int oy = -2; oy <= 2; oy++)
            for (int ox = -2; ox <= 2; ox++) {
              int nx = qx + ox, ny = qy + oy;
              if (nx < 0 || ny < 0 || nx >= c.w || ny >= c.h) continue;
              size_t ni = (size_t)ny * c.w + nx;
              if (!info.glass[ni] || seen[ni]) continue;
              seen[ni] = 1;
              q.push_back((int)ni);
            }
        }
        if (q.size() >= 4) wins.push_back(Vec2(sx / q.size() + 0.5f, sy / q.size() + 0.5f));
      }
  } else bldgNight_.erase(k);
  return bldgTex_[k] = pix_->bake(c);
}

bool View::windowsLit(const Game& g, const Bldg& b) const {
  if (g.inside || g.daylight() > 0.55f) return false;
  if (b.type == art::Building::Inn || b.type == art::Building::Temple || b.type == art::Building::Keep) return true;   // never all asleep
  // households go to bed: fewer windows lit deep in the night, each house on its own schedule
  uint32_t h = hash32((uint32_t)(b.seed + g.day * 7919u));
  float bed = 22.0f + (h % 5);   // 22..26 (26 = past 2 o'clock)
  float hr = g.hour < 12 ? g.hour + 24 : g.hour;
  if (hr > bed && hr < 29.5f) return false;
  return h % 4 != 0;
}
const Tex& View::wallTileTex(uint32_t key) {
  auto it = wallTiles_.find(key);
  if (it != wallTiles_.end()) return it->second;
  return wallTiles_[key] = pix_->bake(art::wallTile(key));
}

// kingdom-coloured sprites, cached per kingdom look: kind 0 the standing banner, 1 the gatehouse (with its seed)
const Tex& View::kingdomTex(int kind, const Kingdom& k, uint32_t seed) {
  const uint64_t key = ew::mix64(((uint64_t)k.color << 32 | k.color2) ^ ((uint64_t)k.emblem << 8 | (uint64_t)kind) ^ ((uint64_t)seed << 40));
  auto it = kingdomTex_.find(key);
  if (it != kingdomTex_.end()) return it->second;
  Canvas c = kind == 0 ? art::kingdomBanner(k.color, k.color2, k.emblem) : art::gateHouse(seed, k.color, k.color2, k.emblem);
  return kingdomTex_[key] = pix_->bake(c);
}

const Tex& View::cachedTex(uint64_t key, Canvas (*paint)(uint64_t key)) {
  auto it = laneTex_.find(key);
  if (it != laneTex_.end()) return it->second;
  return laneTex_[key] = pix_->bake(paint(key));
}

void View::snap(Game& g) {
  const Actor& p = g.pl();
  cam_ = Vec2(p.p.x - Pix::W / 2.0f, p.p.y - 10 - Pix::H / 2.0f);
  const Map& m = g.map();
  float mw = m.w * 16.0f, mh = m.h * 16.0f;
  if (mw > Pix::W) cam_.x = clampf(cam_.x, 0, mw - Pix::W); else cam_.x = (mw - Pix::W) / 2;
  if (mh > Pix::H) cam_.y = clampf(cam_.y, 0, mh - Pix::H); else cam_.y = (mh - Pix::H) / 2;
}

// ------------------------------------------------------------------ events -> particles / audio / UI
void View::spawnParticles(const Event& e, Game& g) {
  Rng r((uint32_t)(t_ * 1000) ^ (uint32_t)(e.p.x * 7 + e.p.y * 13));
  auto add = [&](Vec2 p, Vec2 v, float life, Color c, float size, float grav) {
    Particle q; q.p = p; q.v = v; q.life = q.max = life; q.c = c; q.size = size; q.grav = grav; parts_.push_back(q);
  };
  auto addFx = [&](art::Fx f, Vec2 p, float life) {
    Particle q; q.p = p; q.life = q.max = life; q.kind = 1; q.fx = (int)f; parts_.push_back(q);
  };
  switch (e.type) {
    case Ev::Sfx: audio_->play((Sfx)e.a, e.f, e.vol); break;
    case Ev::Hit:
      for (int i = 0; i < 6; i++) add(e.p, Vec2(r.range(-70, 70), r.range(-80, 20)), 0.25f, Color(1, 1, 0.85f), 1, 200);
      break;
    case Ev::Blood:
      for (int i = 0; i < 7; i++) add(e.p, Vec2(r.range(-50, 50), r.range(-70, 0)), 0.45f, Color(0.7f, 0.08f, 0.08f), r.f() < 0.5f ? 2.f : 1.f, 260);
      break;
    case Ev::Explode:
      addFx(art::Fx::Explosion, e.p, 0.45f);
      for (int i = 0; i < 14; i++) add(e.p, Vec2(r.range(-90, 90), r.range(-100, 40)), 0.5f, Color(1, r.range(0.4f, 0.8f), 0.2f), 1.5f, 120);
      shake_ = std::max(shake_, 2.0f * e.f);
      break;
    case Ev::Sparkle: for (int i = 0; i < 8; i++) add(e.p, Vec2(r.range(-30, 30), r.range(-50, -10)), 0.7f, Color(1, 0.95f, 0.6f), 1, 0); break;
    case Ev::Dust: addFx(art::Fx::Dust, e.p + Vec2(r.range(-3, 3), -2), 0.3f); break;
    case Ev::Heal: addFx(art::Fx::Heal, e.p + Vec2(0, -10), 0.6f); for (int i = 0; i < 10; i++) add(e.p + Vec2(r.range(-8, 8), r.range(-16, 0)), Vec2(0, r.range(-40, -15)), 0.8f, Color(0.5f, 1, 0.6f), 1, 0); break;
    case Ev::Frost: addFx(art::Fx::Frost, e.p + Vec2(0, -6), 0.35f); break;
    case Ev::Text: {
      FloatText ft; ft.p = e.p + Vec2(r.range(-4, 4), 0); ft.s = e.s; ft.c = col((uint32_t)e.a); texts_.push_back(ft);
      break;
    }
    case Ev::Discover:
      banner_ = "DISCOVERED"; bannerSub_ = e.s; bannerT_ = 4.0f;
      break;
    case Ev::LevelUp:
      banner_ = "LEVEL UP"; bannerSub_ = "LEVEL " + std::to_string(e.a) + "  -  CHOOSE A STAT IN THE MENU"; bannerT_ = 4.5f;
      // a bigger moment: a layered fanfare, two rings of light and a column of rising sparks
      audio_->play(Sfx::LevelUp, 0.5f, 1.2f);
      audio_->play(Sfx::QuestDone, 1.5f, 0.8f);
      audio_->play(Sfx::Discover, 0.75f, 0.7f);
      for (int i = 0; i < 32; i++) { float a = i / 32.0f * TAU; add(e.p + Vec2(0, -10), Vec2(std::cos(a) * 75, std::sin(a) * 48), 1.0f, Color(1, 0.85f, 0.3f), 2.0f, 0); }
      for (int i = 0; i < 20; i++) { float a = i / 20.0f * TAU; add(e.p + Vec2(0, -10), Vec2(std::cos(a) * 38, std::sin(a) * 24), 0.8f, Color(1, 1, 0.8f), 1.0f, 0); }
      for (int i = 0; i < 26; i++) add(e.p + Vec2(r.range(-9, 9), r.range(-4, 2)), Vec2(r.range(-6, 6), r.range(-90, -40)), r.range(0.8f, 1.4f), Color(1, r.range(0.75f, 0.95f), 0.35f), 1, -20);
      shake_ = std::max(shake_, 3.0f);
      break;
    case Ev::QuestUpdate: {
      // the same line already showing as the centre notice (Game::say) is not repeated as a toast
      if (!(g.noticeT > 0 && g.notice == e.s)) { Toast t; t.s = e.s; t.c = e.f == 1 ? Color(1, 0.85f, 0.3f) : Color(0.9f, 0.85f, 0.7f); toasts_.push_back(t); }
      if (e.f == 0 || e.f == 1) { banner_ = e.f == 1 ? "QUEST COMPLETE" : "NEW QUEST"; bannerSub_ = e.s.substr(e.s.find(':') == std::string::npos ? 0 : e.s.find(':') + 2); bannerT_ = 3.5f; }
      break;
    }
    case Ev::Notice: {
      if (!(g.noticeT > 0 && g.notice == e.s)) { Toast t; t.s = e.s; t.c = col((uint32_t)e.a); toasts_.push_back(t); }
      break;
    }
    case Ev::Shake: shake_ = std::max(shake_, e.f); break;
    case Ev::MapChange: fade_ = 1.0f; snap(g); break;
    case Ev::WindowShift:   // M1: the endless window moved; everything overworld moved by e.p pixels
      cam_ += e.p;
      for (Particle& q : parts_) if (q.world) q.p += e.p;
      for (FloatText& ft : texts_) ft.p += e.p;
      break;
    default: break;
  }
}

void View::update(Game& g, float dt) {
  t_ += dt;
  perfTick(dt);
  modeT_ += dt;
  if (g.mode != lastMode_) {
    modeT_ = 0;
    if (g.mode == Mode::Dialogue) { dlgChars_ = 0; dlgSel_ = 0; }
    if (g.mode == Mode::Shop) { shopSide_ = 0; shopSel_ = 0; shopArm_ = -1; }
    lastMode_ = g.mode;
  }
  for (const Event& e : g.events) spawnParticles(e, g);
  g.events.clear();
  // (M1) arriving in a settlement: a banner with its name and its kingdom (merged into DISCOVERED the first time)
  if (g.mode == Mode::Play && !g.inside) {
    const int cs = g.curSite >= 0 && g.curSite < (int)g.world.sites.size() && g.world.sites[g.curSite].settlement() ? g.curSite : -1;
    if (cs != arriveSite_) {
      if (cs >= 0) {
        // (M1 economy) what the place lives from heads the line: "MINING VILLAGE  -  KINGDOM OF ..."
        const Site& S = g.world.sites[(size_t)cs];
        std::string sp = S.special ? std::string(ew::specialtyName((ew::Specialty)S.special)) + " " + siteTypeName(S.type) : std::string();
        // (M1 fixer) a market town says so: the trading hub of its region
        if (S.archetype == (uint8_t)ew::Archetype::Market && S.type != SiteType::Village)
          sp = std::string("MARKET ") + siteTypeName(S.type) + (S.special ? std::string(" - ") + ew::specialtyName((ew::Specialty)S.special) : std::string());
        const Kingdom* k = g.world.kingdomOf(cs);
        const std::string kl = k ? (S.capital ? "CAPITAL OF THE KINGDOM OF " : "KINGDOM OF ") + k->name : std::string();
        const std::string line = sp.empty() ? kl : (kl.empty() ? sp : sp + "  -  " + kl);
        const bool arrival = banner_.rfind("KINGDOM", 0) == 0 || banner_.rfind("CAPITAL", 0) == 0 || banner_.find(" VILLAGE") != std::string::npos ||
                             banner_.find(" TOWN") != std::string::npos || banner_.find(" CITY") != std::string::npos;
        if (!line.empty()) {
          if (bannerT_ > 0 && banner_ == "DISCOVERED") banner_ = "DISCOVERED  -  " + line;
          else if (bannerT_ <= 0 || arrival) {
            banner_ = line; bannerSub_ = S.name; bannerT_ = 3.5f;
          }
        }
      }
      arriveSite_ = cs;
    }
  }
  if (toasts_.size() > 5) toasts_.erase(toasts_.begin(), toasts_.begin() + (toasts_.size() - 5));
  // camera follows with a little lead in the aim direction
  if (g.mode != Mode::Title) {
    const Actor& p = g.pl();
    Vec2 want(p.p.x - Pix::W / 2.0f + p.aim.x * 10, p.p.y - 10 - Pix::H / 2.0f + p.aim.y * 6);
    cam_ += (want - cam_) * std::min(1.0f, dt * 6.0f);
    const Map& m = g.map();
    float mw = m.w * 16.0f, mh = m.h * 16.0f;
    if (m.kind == MapKind::Interior) {
      // M0b fix round: indoors the HUD (vitals and purse top-left, minimap and quest column top-right, the touch
      // buttons bottom-right) must never hide part of a room for good. A room that fits clear of the HUD stays put
      // (centred, or centred in the free area); one that doesn't pans with the player far enough that every corner
      // can be brought out from under the HUD.
      const float padT = 46.0f + Pix::ST, padR = 134.0f + Pix::SR, padB = (touchUI ? 48.0f : 0.0f) + Pix::SB, padL = (float)Pix::SL;
      auto axis = [](float& c, float want, float lo, float hi, float scr, float pa, float pb) {
        float len = hi - lo;
        if (len <= scr - 2 * std::max(pa, pb)) c = lo - (scr - len) / 2;             // fits clear, centred
        else if (len + pa + pb <= scr) c = lo - pa - (scr - pa - pb - len) / 2;      // fits in the free area
        else c = clampf(want, lo - pa, hi - scr + pb);                               // pans with the player
      };
      float wantX = cam_.x, wantY = cam_.y;
      axis(cam_.x, wantX, 0, mw, Pix::W, padL, padR);
      axis(cam_.y, wantY, -16, mh, Pix::H, padT, padB);   // the back wall rises a row above the map
    } else {
      if (mw > Pix::W) cam_.x = clampf(cam_.x, 0, mw - Pix::W); else cam_.x = (mw - Pix::W) / 2;
      if (mh > Pix::H) cam_.y = clampf(cam_.y, 0, mh - Pix::H); else cam_.y = (mh - Pix::H) / 2;
    }
  } else {
    // title: slow drift across the start region
    const Site& home = g.world.sites[g.world.startSite];
    Vec2 c0(home.r.cx() * 16.0f - Pix::W / 2.0f - 300, home.r.cy() * 16.0f - Pix::H / 2.0f - 120);
    if (titleT_ == 0 || titleT_ > 90) { cam_ = c0; titleT_ = 0.001f; }
    titleT_ += dt;
    cam_ = c0 + Vec2(titleT_ * 8.0f, titleT_ * 2.5f);
  }
  shake_ = std::max(0.0f, shake_ - dt * 12);
  shakeOff_ = shake_ > 0.1f ? Vec2(std::sin(t_ * 91) * shake_, std::cos(t_ * 77) * shake_ * 0.7f) : Vec2();
  for (size_t i = 0; i < parts_.size();) {
    Particle& q = parts_[i];
    q.life -= dt;
    if (q.life <= 0) { parts_.erase(parts_.begin() + i); continue; }
    q.v.y += q.grav * dt;
    q.p += q.v * dt;
    i++;
  }
  for (size_t i = 0; i < texts_.size();) {
    texts_[i].t += dt;
    texts_[i].p.y -= dt * 22;
    if (texts_[i].t > 0.9f) texts_.erase(texts_.begin() + i); else i++;
  }
  // toasts wait while a dialogue or menu is open, so "OLD BLADE" is still there when the player looks up
  const bool modal = g.mode == Mode::Dialogue || g.mode == Mode::Shop || g.mode == Mode::Menu || g.mode == Mode::LevelUp || g.mode == Mode::Paused;
  for (size_t i = 0; i < toasts_.size();) {
    if (modal) { i++; continue; }
    toasts_[i].t += dt;
    if (toasts_[i].t > 4.0f) toasts_.erase(toasts_.begin() + i); else i++;
  }
  if (bannerT_ > 0 && g.mode != Mode::Dialogue) bannerT_ -= dt;   // shown once the dialogue closes (drawHud)
  if (fade_ > 0) fade_ = std::max(0.0f, fade_ - dt * 2.2f);
  {
    float before = dlgChars_;
    dlgChars_ += dt * 70;
    if (g.mode == Mode::Dialogue && (int)(before / 3) != (int)(dlgChars_ / 3) && dlgChars_ < (float)g.dlg.text.size()) {
      size_t ci = std::min(g.dlg.text.size() - 1, (size_t)dlgChars_);
      if (g.dlg.text[ci] != ' ') audio_->play(Sfx::Talk, 0.7f + (hash32((uint32_t)std::hash<std::string>()(g.dlg.speaker)) % 80) / 100.0f, 0.8f);
    }
  }

  // music director
  Music want = Music::Wild;
  if (g.mode == Mode::Title) want = Music::Title;
  else if (g.mode == Mode::Dead) want = Music::Silence;
  else {
    bool fight = false, boss = false;
    for (const Actor& a : g.actors)
      if (a.hostile && a.aggro && a.st != AState::Dead && len2(a.p - g.pl().p) < 170 * 170) { fight = true; if (a.boss) boss = true; }
    if (g.alarmSite >= 0) fight = true;   // town alarm bell: combat music even when the fight is across town
    if (fight) combatT_ = boss ? 6.0f : 4.0f;
    else combatT_ = std::max(0.0f, combatT_ - dt);
    if (boss && fight) want = Music::Boss;
    else if (combatT_ > 0) want = Music::Combat;
    else if (g.inside && g.subBldg >= 0) want = Music::Town;
    else if (g.inside) want = Music::Cave;
    else if (g.curSite >= 0 && (g.world.sites[g.curSite].type == SiteType::City || g.world.sites[g.curSite].type == SiteType::Town || g.world.sites[g.curSite].type == SiteType::Village)) want = Music::Town;
    else if (g.isNight()) want = Music::Night;
  }
  if (want != music_) { music_ = want; audio_->setMusic(want); }

  // ambient particles
  if (g.mode != Mode::Title && !g.inside) {
    const Actor& p = g.pl();
    Biome b = g.world.over.biomeAt((int)(p.p.x / 16), (int)(p.p.y / 16));
    Rng r((uint32_t)(t_ * 997));
    if (g.isNight() && (b == Biome::Plains || b == Biome::Forest || b == Biome::Swamp || b == Biome::Autumn) && r.f() < dt * 6) {
      Particle q; q.p = cam_ + Vec2(r.range(0, Pix::W), r.range(0, Pix::H)); q.v = Vec2(r.range(-6, 6), r.range(-6, 6));
      q.life = q.max = r.range(2, 4); q.c = Color(0.8f, 1.0f, 0.4f); q.size = 1; q.layer = 1; parts_.push_back(q);
    }
    if (b == Biome::Autumn && r.f() < dt * 5) {
      Particle q; q.p = cam_ + Vec2(r.range(0, Pix::W), -4); q.v = Vec2(r.range(8, 20), r.range(14, 24));
      q.life = q.max = 10; q.c = r.f() < 0.5f ? Color(0.85f, 0.35f, 0.15f) : Color(0.95f, 0.7f, 0.25f); q.size = 2; q.layer = 2; parts_.push_back(q);
    }
  }
  if (g.inside && g.subSite >= 0 && hashf((int)(t_ * 10), 0, 9) < dt * 4) {
    Rng r((uint32_t)(t_ * 331));
    Particle q; q.p = cam_ + Vec2(r.range(0, Pix::W), r.range(0, Pix::H)); q.v = Vec2(r.range(-3, 3), r.range(-5, -1));
    q.life = q.max = 3; q.c = Color(0.7f, 0.65f, 0.6f); q.size = 1; q.layer = 1; parts_.push_back(q);
  }
  if (parts_.size() > 600) parts_.erase(parts_.begin(), parts_.begin() + (parts_.size() - 600));
}

// ------------------------------------------------------------------ world
namespace {
struct Drawable {
  float y;
  int kind;     // 0 prop, 1 building, 2 wall, 3 actor, 4 pickup, 5 projectile, 6 gate
  int idx;
  int tx, ty;
};
}  // namespace

void View::drawWorld(Game& g) {
  Pix& P = *pix_;
  fadePaintMs_ = 0;
  const Map& m = g.mode == Mode::Title ? g.world.over : g.map();
  int key = g.mode == Mode::Title ? 0 : g.mapKey();
  if (key != lastMapKey_) { lastMapKey_ = key; }
  uint64_t mapId = hash32((uint32_t)g.world.seed ^ (uint32_t)(g.world.seed >> 32)) * 2654435761ull + (uint64_t)(key + 7);
  mapId ^= (uint64_t)g.world.genVersion << 56 | (uint64_t)(g.world.endless ? 1 : 0) << 55;
  // M1: an endless overworld's terrain chunks are keyed by GLOBAL chunk (the window origin in chunks plus the local
  // chunk), so a window shift keeps every chunk already baked (terrain.cpp)
  const bool endlessOver = key == 0 && g.world.endless && m.kind == MapKind::Overworld;
  chunkEndless_ = endlessOver;
  chunkOX_ = endlessOver ? (int)std::floor(g.world.ox / 32.0) : 0;
  chunkOY_ = endlessOver ? (int)std::floor(g.world.oy / 32.0) : 0;
  Vec2 cam(std::floor(cam_.x + shakeOff_.x), std::floor(cam_.y + shakeOff_.y));
  trimCaches();
  // water layer underneath the terrain (shows through translucent water pixels)
  if (m.kind == MapKind::Overworld) {
    P.rect(0, 0, Pix::W, Pix::H, Color(0.16f, 0.36f, 0.6f));
    float ox = std::fmod(t_ * 5.0f + cam.x * 0.0f, 64.0f), oy = std::fmod(std::sin(t_ * 0.6f) * 6 + 64, 64.0f);
    for (float y = -64; y < Pix::H + 64; y += 64)
      for (float x = -64; x < Pix::W + 64; x += 64) {
        float wx = x - std::fmod(cam.x, 64.0f) + ox, wy = y - std::fmod(cam.y, 64.0f) + oy;
        P.blit(water_, wx, wy, false, Color(1, 1, 1, 0.55f + 0.25f * std::sin(t_ * 1.7f + x * 0.01f)));
        P.blit(water_, wx + 32 - ox * 1.6f, wy + 17, true, Color(1, 1, 1, 0.35f));
      }
  } else {
    P.rect(0, 0, Pix::W, Pix::H, Color(0.04f, 0.035f, 0.05f));
    if (m.kind == MapKind::Cave || m.kind == MapKind::Ruin) {
      // dark water in caves animates gently too
      P.rect(0, 0, Pix::W, Pix::H, Color(0.08f, 0.14f, 0.2f));
    }
  }
  // terrain chunks
  int c0x = (int)std::floor(cam.x / 512), c0y = (int)std::floor(cam.y / 512);
  int c1x = (int)std::floor((cam.x + Pix::W) / 512), c1y = (int)std::floor((cam.y + Pix::H) / 512);
  bakeVisibleNow(m, mapId, c0x, c0y, c1x, c1y);
  for (int cy = c0y; cy <= c1y; cy++)
    for (int cx = c0x; cx <= c1x; cx++) {
      if (cx < 0 || cy < 0 || cx * 32 >= m.w || cy * 32 >= m.h) continue;
      Tex t = chunkTex(m, mapId, cx, cy);
      P.blit(t, cx * 512 - cam.x, cy * 512 - cam.y);
    }
  prefetch(m, mapId, cam);
  pumpBake(5.0);
  // wall layout (towers, joins, gate flanks): once per map
  bldgMap_ = &m;
  // the id also carries the generator version and gate count: a new game on the same seed with another generator
  // has the same map id but other walls
  // (M1: and the endless window's origin, as the layout is in window tiles)
  uint64_t wallId = mapId ^ ((uint64_t)g.world.genVersion << 58) ^ ((uint64_t)g.world.gates.size() << 48);
  if (endlessOver) wallId ^= ew::mix64(((uint64_t)(uint32_t)g.world.ox << 32) | (uint32_t)g.world.oy);
  if (wallKeysId_ != wallId) {
    wallKeysId_ = wallId;
    const auto tw0 = std::chrono::steady_clock::now();
    struct WallTime {
      std::chrono::steady_clock::time_point t0;
      ~WallTime() {
        if (std::getenv("EMB_TIMING"))
          std::printf("wall keys: %.1f ms\n", std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count());
      }
    } wallTime{tw0};
    bool any = false;
    for (uint8_t v : m.wall) if (v) { any = true; break; }
    wallKeys_.clear();
    if (any) {
      auto it = wallKeyCache_.find(wallId);
      if (it != wallKeyCache_.end() && it->second.size() == (size_t)m.w * m.h) wallKeys_ = it->second;
      else {
        if (m.kind == MapKind::Overworld) {
          art::wallKeys(m.wall.data(), m.w, m.h, g.world.gates.data(), (int)g.world.gates.size(), wallKeys_);
          // a river under the wall (WORLDGEN_V5 keeps its water there): no tower standing in it, and a water gate in
          // the wall's face so the river visibly flows through instead of stopping at a dam
          for (int y = 0; y < m.h; y++)
            for (int x = 0; x < m.w; x++) {
              uint32_t& k = wallKeys_[(size_t)y * m.w + x];
              if (!k || !groundWater(m.at(x, y))) continue;
              if (k & art::WALL_BIT_TOWER) {
                k &= ~art::WALL_BIT_TOWER;
                if (y + 1 < m.h && wallKeys_[(size_t)(y + 1) * m.w + x]) wallKeys_[(size_t)(y + 1) * m.w + x] &= ~art::WALL_BIT_TOWER_N;
              }
              k |= art::WALL_BIT_CULVERT;
            }
        } else art::wallKeys(m.wall.data(), m.w, m.h, nullptr, 0, wallKeys_);
        if (wallKeyCache_.size() > 4) wallKeyCache_.clear();
        wallKeyCache_[wallId] = wallKeys_;
      }
    }
    wallTodo_.clear();
    std::unordered_map<uint32_t, bool> queued;
    for (uint32_t k : wallKeys_)
      if (k && !wallTiles_.count(k) && !queued.count(k)) { queued[k] = true; wallTodo_.push_back(k); }
  }
  // wall tiles not seen yet are painted ahead, one per frame (the ones on screen are painted on first use anyway)
  while (!wallTodo_.empty()) {
    uint32_t k = wallTodo_.back();
    wallTodo_.pop_back();
    if (!wallTiles_.count(k)) { wallTileTex(k); break; }
  }
  // collect drawables
  std::vector<Drawable> list;
  int tx0 = (int)std::floor(cam.x / 16) - 3, ty0 = (int)std::floor(cam.y / 16) - 2;
  int tx1 = tx0 + Pix::W / 16 + 6, ty1 = ty0 + Pix::H / 16 + 7;
  drawDeco(m, cam, std::max(0, tx0), std::max(0, ty0), std::min(m.w, tx1), std::min(m.h, ty1));
  const Actor& pl = g.pl();
  for (int ty = std::max(0, ty0); ty < std::min(m.h, ty1); ty++)
    for (int tx = std::max(0, tx0); tx < std::min(m.w, tx1); tx++) {
      int pr = m.prop[(size_t)ty * m.w + tx];
      if (pr) {
        Prop p = (Prop)(pr - 1);
        if (flatProp(p)) {
          if (uint32_t ik = interiorPropTexKey(g, m, tx, ty, p)) {
            const Tex& t = cachedTex(0x01ull << 56 | ik, paintInteriorPiece);
            P.blit(t, tx * 16 + 8 - t.w / 2 - cam.x, ty * 16 + 16 - t.h - cam.y);
          } else {
            const Tex& t = props_[(int)p];
            int fw = art::propW(p);
            int frames = std::max(1, art::propFrames(p));
            int fr = frames > 1 ? (int)(t_ * 8 + tx * 3) % frames : 0;
            P.blitRegion(t, fr * fw, 0, fw, t.h, tx * 16 + 8 - fw / 2 - cam.x, ty * 16 + 16 - t.h - cam.y);
          }
        } else list.push_back({ty * 16.0f + 15.0f, 0, pr - 1, tx, ty});
      }
      uint32_t wk = wallKeys_.empty() ? 0u : wallKeys_[(size_t)ty * m.w + tx];
      if (wk) list.push_back({ty * 16.0f + ((wk & art::WALL_BIT_TOWER) ? 15.3f : 15.0f), 2, (int)wk, tx, ty});
    }
  {
    static std::vector<int> vis;
    bldgsIn(g, m, cam.x, cam.y, cam.x + Pix::W, cam.y + Pix::H, vis);
    for (int bi : vis) list.push_back({(m.bldgs[bi].r.y + m.bldgs[bi].r.h) * 16.0f - 1.0f, 1, bi, 0, 0});
  }
  // paint the sprites of buildings near the player ahead of time, at most one per frame, so walking into a town
  // never stalls on a burst of building paints
  if (!m.bldgs.empty() && g.mode != Mode::Title) {
    for (int k = 0; k < 12; k++) {
      bldgPrefetch_ = (bldgPrefetch_ + 1) % (int)m.bldgs.size();
      const Bldg& b = m.bldgs[bldgPrefetch_];
      if (std::fabs(b.r.cx() * 16.0f - pl.p.x) > 720 || std::fabs(b.r.cy() * 16.0f - pl.p.y) > 520) continue;
      if (bldgTex_.count(bldgKey(m, b, bldgPrefetch_))) continue;
      bldgTex(b, bldgPrefetch_);
      break;
    }
  }
  for (int i = 0; i < (int)g.actors.size(); i++) {
    const Actor& a = g.actors[i];
    if (a.p.x < cam.x - 80 || a.p.x > cam.x + Pix::W + 80 || a.p.y < cam.y - 40 || a.p.y > cam.y + Pix::H + 80) continue;
    list.push_back({a.p.y + (a.fly ? 60.0f : 0.0f) - (a.st == AState::Dead ? 8.0f : 0.0f), 3, i, 0, 0});
  }
  for (int i = 0; i < (int)g.pickups.size(); i++) list.push_back({g.pickups[i].p.y - 4, 4, i, 0, 0});
  // projectiles fly at chest height: sort them by the ground point beneath (arrows, spells, spit, dragon fire)
  for (int i = 0; i < (int)g.projs.size(); i++) {
    const Projectile& pr = g.projs[i];
    if (pr.p.x < cam.x - 24 || pr.p.x > cam.x + Pix::W + 24 || pr.p.y < cam.y - 24 || pr.p.y > cam.y + Pix::H + 40) continue;
    list.push_back({pr.p.y + 8.0f, 5, i, 0, 0});
  }
  if (m.kind == MapKind::Overworld)
    for (auto& gt : g.world.gates) {
      if ((gt.first + 5) * 16 < cam.x || (gt.first - 3) * 16 > cam.x + Pix::W || gt.second * 16 + 24 < cam.y || gt.second * 16 - 60 > cam.y + Pix::H) continue;
      list.push_back({gt.second * 16.0f + 15.6f, 6, 0, gt.first, gt.second});
    }
  std::sort(list.begin(), list.end(), [](const Drawable& a, const Drawable& b) { return a.y < b.y; });

  // shadows first (under everything standing)
  for (const Drawable& d : list) {
    if (d.kind == 3) {
      const Actor& a = g.actors[d.idx];
      if (a.st == AState::Dead) continue;
      bool big = a.radius > 7;
      const Tex& s = big ? shadowBig_ : shadow_;
      float sc = big ? std::min(1.6f, a.radius / 10.0f) : 1.0f;
      if (a.mon == Monster::Dragon) sc = a.fly ? 1.8f : 2.0f;
      P.blitEx(s, 0, 0, s.w, s.h, a.p.x - s.w * sc / 2 - cam.x, a.p.y - s.h * sc / 2 - cam.y, s.w * sc, s.h * sc, false, Color(1, 1, 1, a.fly ? 0.6f : 1));
    } else if (d.kind == 0) {
      Prop p = (Prop)d.idx;
      int sh = propShadow(p);
      if (sh == 1) P.blitEx(shadowBig_, 0, 0, 40, 12, d.tx * 16 + 8 - 12 - cam.x, d.ty * 16 + 11 - cam.y, 24, 8, false, Color(1, 1, 1, 0.8f));
      else if (sh == 2) P.blitEx(shadowBig_, 0, 0, 40, 12, d.tx * 16 + 8 - 11 - cam.x, d.ty * 16 + 12 - cam.y, 22, 7, false, Color(1, 1, 1, 0.7f));
      else if (sh == 3) {   // (M1 economy) the stall's awning shades the ground a little east of it (the sun is up-left)
        P.blitEx(shadowBig_, 0, 0, 40, 12, d.tx * 16 + 8 - 22 - cam.x, d.ty * 16 + 9 - cam.y, 50, 11, false, Color(1, 1, 1, 0.75f));
      } else if (sh == 4) {   // (M1 fixer round 2) a two-tile table or cloth: its shade under both its tiles
        P.blitEx(shadowBig_, 0, 0, 40, 12, d.tx * 16 + 1 - cam.x, d.ty * 16 + 10 - cam.y, 32, 8, false, Color(1, 1, 1, 0.6f));
      } else if (sh == 5) {   // a beast's own small shadow
        P.blitEx(shadowBig_, 0, 0, 40, 12, d.tx * 16 + 8 - 9 - cam.x, d.ty * 16 + 12 - cam.y, 18, 5, false, Color(1, 1, 1, 0.6f));
      }
    } else if (d.kind == 6) {
      // the gate passage lies in the gatehouse's shade, deepest under the vault
      P.rect(d.tx * 16 + 3 - cam.x, d.ty * 16 - cam.y, 42, 16, Color(0.10f, 0.07f, 0.20f, 0.28f));
      P.rect(d.tx * 16 + 3 - cam.x, d.ty * 16 - cam.y, 42, 10, Color(0.10f, 0.07f, 0.20f, 0.22f));
    }
  }
  bool ghost = false;   // the hero is hidden behind a building or a tree crown: show a silhouette over it
  const Tex* ghostTex = nullptr;
  int ghostFr = 0, ghostRow = 0;
  bool ghostFlip = false;
  float ghostX = 0, ghostY = 0;
  for (const Drawable& d : list) {
    switch (d.kind) {
      case 0: {
        Prop p = (Prop)d.idx;
        if (m.kind == MapKind::Interior)
          if (uint32_t ik = interiorPropTexKey(g, m, d.tx, d.ty, p)) {
            const Tex& t = cachedTex(0x01ull << 56 | ik, paintInteriorPiece);
            P.blit(t, d.tx * 16 + 8 - t.w / 2 - cam.x, d.ty * 16 + 16 - t.h - cam.y);
            break;
          }
        const Tex* tp = &props_[d.idx];
        // (M1) every market stall on the overworld has its own awning and goods (by its tile: stable, never repeats
        // on one square)
        if (p == Prop::MarketStall && m.kind == MapKind::Overworld) {
          const int32_t gx = d.tx + (m.kind == MapKind::Overworld ? g.world.ox : 0), gy = d.ty + (m.kind == MapKind::Overworld ? g.world.oy : 0);
          const uint32_t h = hash2(gx, gy, 6151);
          tp = &cachedTex(0x02ull << 56 | (uint64_t)(h % 36), paintStall);
        }
        // (M1 economy) a trade's stall: the awning cloth steps along a row (a stall three tiles on wears the next
        // cloth, so neighbours never match), and differs between rows and squares
        // (M1 fixer round 2) each row of a market keeps one stall form (cloth booths, canvas tents or shingled timber
        // booths; by the row, so a row reads as one covered run and the next row may differ); after its closing hour
        // a stall is packed up (its stock under a cover, a curtain or shutter down): the hour its keeper leaves
        const bool vendorOpen = g.mode == Mode::Title || m.kind != MapKind::Overworld || ew::stallOpen(d.tx + g.world.ox, d.ty + g.world.oy, g.hour);
        if (art::isStall(p) && m.kind == MapKind::Overworld) {
          const int32_t gx = d.tx + g.world.ox, gy = d.ty + g.world.oy;
          const uint32_t row = hash2(0, gy, 6163) % (uint32_t)art::kStallAwnings;
          const int64_t col3 = gx >= 0 ? gx / 3 : -((-(int64_t)gx + 2) / 3);   // floor(gx / 3)
          const uint32_t aw = (uint32_t)(((col3 * 5 + (int64_t)row) % art::kStallAwnings + art::kStallAwnings) % art::kStallAwnings);
          const uint32_t form = (uint32_t)ew::stallFormAt(gy);
          tp = &cachedTex(0x04ull << 56 | (uint64_t)(vendorOpen ? 0 : 1) << 12 | (uint64_t)form << 8 | (uint64_t)aw << 4 | (uint64_t)art::stallTrade(p), paintTradeStall);
        }
        if ((p == Prop::MarketTable || p == Prop::GroundCloth) && m.kind == MapKind::Overworld) {
          const int32_t gx = d.tx + g.world.ox, gy = d.ty + g.world.oy;
          const uint64_t k = (uint64_t)(vendorOpen ? 0 : 1) << 8;
          if (p == Prop::MarketTable) tp = &cachedTex(0x05ull << 56 | k | (uint64_t)ew::tableShadeAt(gx, gy) << 4 | (uint64_t)ew::tableGoodsAt(gx, gy), paintMarketTable);
          else tp = &cachedTex(0x06ull << 56 | k | (uint64_t)ew::clothColourAt(gx, gy) << 4 | (uint64_t)ew::clothGoodsAt(gx, gy), paintGroundCloth);
        }
        // (M1 fixer round 2) the mine hill: its shape by its tile, its top by its land (snow, dry grass or green)
        if (p == Prop::MineHill && m.kind == MapKind::Overworld) {
          const Biome bb = m.biomeAt(d.tx, d.ty);
          const int land = bb == Biome::Snow || bb == Biome::Taiga || bb == Biome::Mountain ? 1 : (bb == Biome::Desert ? 2 : 0);
          tp = &cachedTex(0x08ull << 56 | (uint64_t)land << 2 | (uint64_t)(hash2(d.tx + g.world.ox, d.ty + g.world.oy, 6211) & 3u), paintMineHill);
        }
        // (M1 fixer round 2) the mine's track joins its neighbours (and runs in under the adit's frame)
        if (p == Prop::MineRail) {
          auto railAt = [&](int x, int y) {
            const int q = m.propAt(x, y);
            return q == (int)Prop::MineRail + 1 || q == (int)Prop::OreCart + 1;
          };
          int j = 0;
          if (railAt(d.tx, d.ty - 1) || m.propAt(d.tx, d.ty - 1) == (int)Prop::MineEntrance + 1 || m.propAt(d.tx, d.ty - 1) == (int)Prop::MineHill + 1) j |= 1;
          if (railAt(d.tx + 1, d.ty)) j |= 2;
          if (railAt(d.tx, d.ty + 1)) j |= 4;
          if (railAt(d.tx - 1, d.ty)) j |= 8;
          tp = &cachedTex(0x07ull << 56 | (uint64_t)j, paintMineRail);
        }
        // (M1) every piece of a ruin's walls and columns has its own broken top (by its global tile)
        if ((p == Prop::RuinWall || p == Prop::RuinColumn) && m.kind == MapKind::Overworld) {
          const uint32_t h = hash2(d.tx + g.world.ox, d.ty + g.world.oy, 6173);
          const bool joinN = p == Prop::RuinWall && m.propAt(d.tx, d.ty - 1) == (int)Prop::RuinWall + 1;
          tp = &cachedTex(0x03ull << 56 | (uint64_t)p << 8 | (uint64_t)(h % 8) | (joinN ? 8u : 0u), paintRuin);
        }
        // (M1) a banner inside a kingdom's settlement flies that kingdom's colours
        if (p == Prop::Banner && m.kind == MapKind::Overworld && g.mode != Mode::Title)
          if (const Kingdom* k = g.world.kingdomOf(g.world.siteAt(d.tx, d.ty, 2))) tp = &kingdomTex(0, *k, 0);
        const Tex& t = *tp;
        int fw = art::propW(p), fh = art::propH(p);
        int frames = std::max(1, art::propFrames(p));
        int fr = frames > 1 ? (int)(t_ * 8 + d.tx * 3 + d.ty) % frames : 0;
        bool flipP = false;
        if (p == Prop::Sheep || p == Prop::Cow) {   // (M1 fixer round 2) the beasts graze at their own slow pace, either way round
          const uint32_t h = hash2(d.tx + g.world.ox, d.ty + g.world.oy, 6197);
          fr = (int)(t_ * 1.6f + (float)(h % 97u) * 0.37f) % frames;
          flipP = ((h >> 8) & 1) != 0;
        }
        float jx = 0, jy = 0;
        if (natureProp(p) && m.kind == MapKind::Overworld) { uint32_t h = hash2(d.tx, d.ty, 55); jx = (float)((int)(h % 7) - 3); jy = (float)((int)((h >> 4) % 3) - 1); }
        float x = d.tx * 16 + 8 - fw / 2 + jx - cam.x, y = d.ty * 16 + 16 - fh + jy - cam.y;
        float alpha = 1;
        // a tree crown over the hero (drawn before it): the crown thins out and the hero shows through (ghost below)
        if (treeProp(p) && pl.p.y < d.ty * 16 + 15 && pl.p.y > d.ty * 16 + 16 - fh + 8 && std::fabs(pl.p.x - (d.tx * 16 + 8 + jx)) < fw * 0.5f) {
          alpha = 0.6f;
          ghost = true;
        }
        P.blitEx(t, fr * fw, 0, fw, fh, x, y, (float)fw, (float)fh, flipP, Color(1, 1, 1, alpha));
        // (M1 fixer) at dusk an open stall hangs a lit lantern under its valance (the light pass adds its glow); after
        // the stall's closing hour it is dark and its keeper has gone
        if (p == Prop::MarketTable && m.kind == MapKind::Overworld && g.mode != Mode::Title && g.daylight() < 0.55f && vendorOpen) {
          // an open table's candle lantern stands at its end
          P.rect(x + 43, y + 24, 3, 1, Color(0.32f, 0.24f, 0.16f));
          P.rect(x + 43, y + 25, 3, 4, Color(1.0f, 0.78f, 0.38f));
          P.rect(x + 44, y + 26, 1, 2, Color(1.0f, 0.97f, 0.78f));
        }
        if (art::isStall(p) && m.kind == MapKind::Overworld && g.mode != Mode::Title && g.daylight() < 0.55f && vendorOpen) {
          P.rect(x + 6, y + 16, 1, 2, Color(0.22f, 0.18f, 0.14f));   // the hook
          P.rect(x + 4, y + 18, 5, 1, Color(0.32f, 0.24f, 0.16f));   // the cap
          P.rect(x + 4, y + 19, 5, 4, Color(1.0f, 0.78f, 0.38f));    // the glass
          P.rect(x + 5, y + 20, 3, 2, Color(1.0f, 0.97f, 0.78f));    // the flame
          P.rect(x + 4, y + 23, 5, 1, Color(0.32f, 0.24f, 0.16f));   // the base
        }
        if (p == Prop::Campfire || p == Prop::Brazier) {
          Rng r((uint32_t)(t_ * 30) + d.tx * 7);
          if (r.f() < 0.3f) { Particle q; q.p = Vec2(d.tx * 16 + 8 + r.range(-3, 3), d.ty * 16 + 6.0f); q.v = Vec2(r.range(-5, 5), r.range(-30, -15)); q.life = q.max = 0.8f; q.c = Color(1, 0.6f, 0.2f); q.size = 1; parts_.push_back(q); }
        }
        break;
      }
      case 1: {
        const Bldg& b = m.bldgs[d.idx];
        // (M1 round 3) behind a fade (a fast travel's arrival, a new game) the buildings not painted yet are painted
        // a few per frame within a budget instead of all in the arrival frame (a capital's 70-90 sprites took ~150 ms
        // on the desktop, more on a phone's wasm); the fade hides the ones still waiting
        if ((g.sleepFade > 0.5f || fade_ > 0.5f) && !bldgTex_.count(bldgKey(m, b, d.idx))) {
          if (fadePaintMs_ >= 10.0) break;
          const auto t0 = std::chrono::steady_clock::now();
          bldgTex(b, d.idx);
          fadePaintMs_ += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
        }
        const Tex* tp = &bldgTex(b, d.idx);
        if (m.kind == MapKind::Overworld && g.mode != Mode::Title && windowsLit(g, b)) {
          auto nt = bldgNight_.find(bldgKey(m, b, d.idx));
          if (nt != bldgNight_.end()) tp = &nt->second;
        }
        const Tex& t = *tp;
        const float bottom = (b.r.y + b.r.h) * 16.0f, top = bottom + art::BLDG_PAD_B - t.h;
        float x = b.r.x * 16.0f - art::BLDG_PAD_X - cam.x, y = top - cam.y;
        // the hero behind the building (feet hidden under its walls or roof): the building stays solid and the hero
        // shows through it as a silhouette (drawn after everything, below). Fading the whole house - door, windows and
        // all - turned buildings into ghosts whenever the hero passed behind a roof.
        auto tr = bldgTopRow_.find(bldgKey(m, b, d.idx));
        float vis = top + (tr != bldgTopRow_.end() ? (float)tr->second : 0.0f);
        if (pl.p.y < bottom - 4 && pl.p.y > vis + 6 && pl.p.x > b.r.x * 16 - 2 && pl.p.x < (b.r.x + b.r.w) * 16 + 2) ghost = true;
        P.blitEx(t, 0, 0, t.w, t.h, x, y, (float)t.w, (float)t.h, false, Color(1, 1, 1, 1));
        // chimney smoke: soft puffs that rise, drift east with the wind and spread
        auto sm = bldgSmoke_.find(bldgKey(m, b, d.idx));
        if (sm != bldgSmoke_.end() && g.mode != Mode::Title)
          for (size_t si = 0; si < sm->second.size(); si++) {
            Rng r((uint32_t)(t_ * 40) * 31u + (uint32_t)d.idx * 977u + (uint32_t)si * 13u);
            if (r.f() > 0.10f) continue;
            Particle q;
            q.p = Vec2(b.r.x * 16.0f - art::BLDG_PAD_X + sm->second[si].x + r.range(-1, 1), top + sm->second[si].y);
            q.v = Vec2(r.range(2, 7), r.range(-13, -8));
            q.life = q.max = r.range(2.2f, 3.2f);
            float gr = r.range(0.72f, 0.86f);
            q.c = Color(gr, gr, gr + 0.04f);
            q.size = r.f() < 0.5f ? 2.0f : 1.0f;
            q.grav = -1.5f;
            q.kind = 2;
            parts_.push_back(q);
          }
        break;
      }
      case 2: {
        const Tex& t = wallTileTex((uint32_t)d.idx);
        P.blit(t, d.tx * 16.0f - art::WALL_OX - cam.x, d.ty * 16.0f - art::WALL_OY - cam.y);
        break;
      }
      case 6: {
        // (M1) the gatehouse flies the banner of the kingdom whose town it guards
        const Tex* gt = &gateTex_;
        if (const Kingdom* k = g.world.kingdomOf(g.world.siteAt(d.tx + 1, d.ty, 3))) gt = &kingdomTex(1, *k, 7);
        P.blit(*gt, d.tx * 16.0f - art::GATE_OX - cam.x, d.ty * 16.0f - art::GATE_OY - cam.y);
        break;
      }
      case 3: {
        const Actor& a = g.actors[d.idx];
        float flash = a.flash > 0 ? a.flash / 0.12f : 0;
        float alpha = a.st == AState::Dead ? clampf(1.0f - (a.stT - 4.0f) / 2.0f, 0, 1) : 1.0f;
        if (a.player && a.iframes > 0 && a.st != AState::Roll && ((int)(t_ * 20) & 1)) alpha *= 0.5f;
        if (a.human) {
          const Tex& t = humanTex(a.look);
          int row = a.face == 0 ? 0 : a.face == 1 ? 1 : 2;
          bool flip = a.face == 3;
          int fr = 0;
          switch (a.st) {
            case AState::Walk: fr = 1 + (int)(a.animT * 9) % 4; break;
            case AState::Roll: fr = 1 + (int)(a.stT * 20) % 4; break;
            case AState::Windup: case AState::Cast: fr = 5; break;
            case AState::Strike: fr = 6; break;
            case AState::Recover: fr = 6; break;
            case AState::Hurt: case AState::Dead: fr = 7; break;
            default: fr = 0; break;
          }
          float bob = a.st == AState::Roll ? 3.0f : 0;
          float x = a.p.x - art::HUMAN_W / 2.0f - cam.x, y = a.p.y - art::HUMAN_H + 2 + bob - cam.y;
          if (a.st == AState::Dead) y += 3;
          P.blitEx(t, fr * art::HUMAN_W, row * art::HUMAN_H, art::HUMAN_W, art::HUMAN_H, x, y, (float)art::HUMAN_W, (float)art::HUMAN_H, flip, Color(1, 1, 1, alpha));
          if (a.player) { ghostTex = &t; ghostFr = fr; ghostRow = row; ghostFlip = flip; ghostX = x; ghostY = y; }
          if (flash > 0) P.blitEx(t, fr * art::HUMAN_W, row * art::HUMAN_H, art::HUMAN_W, art::HUMAN_H, x, y, (float)art::HUMAN_W, (float)art::HUMAN_H, flip, Color(1, 1, 1, flash), 1);
          if (a.burnT > 0 && ((int)(t_ * 10) & 1)) P.rectAdd(x + 5, y + 6, 6, 10, Color(0.6f, 0.25f, 0.05f, 0.6f));
        } else {
          const Tex& t = monsters_[(int)a.mon];
          int cw = art::monsterCellW(a.mon), chh = art::monsterCellH(a.mon);
          int fr = 0;
          switch (a.st) {
            case AState::Walk: fr = (int)(a.animT * (a.mon == Monster::Bat ? 14 : 8)) % 4; break;
            case AState::Windup: fr = 4; break;
            case AState::Strike: fr = 5; break;
            case AState::Hurt: fr = 6; break;
            case AState::Dead: case AState::Down: fr = 7; break;
            default: fr = (int)(a.animT * 3) % 2; break;
          }
          if (a.fly && a.mon == Monster::Dragon) fr = (int)(a.animT * 6) % 4;
          bool flip = a.aim.x < 0;
          float lift = a.fly ? 34 + std::sin(a.animT * 3) * 3 : (a.flying && a.st != AState::Dead ? 6 + std::sin(a.animT * 6) * 2 : 0);
          float x = a.p.x - cw / 2.0f - cam.x, y = a.p.y - chh + 2 - lift - cam.y;
          // collapsed bones rattle harder as they are about to stand back up
          if (a.st == AState::Down && a.stT > 1.2f) x += std::sin(t_ * 60) * std::min(2.0f, (a.stT - 1.2f) * 1.5f);
          // a heavy windup rears back; a wolf crouches before the lunge
          if (a.st == AState::Windup && a.heavy) y -= std::min(3.0f, a.stT * 6);
          if (a.st == AState::Windup && a.lunge) y += 1;
          P.blitEx(t, fr * cw, 0, cw, chh, x, y, (float)cw, (float)chh, flip, Color(1, a.slowT > 0 ? 0.85f : 1, a.slowT > 0 ? 1 : 1, alpha));
          if (a.slowT > 0) P.blitEx(t, fr * cw, 0, cw, chh, x, y, (float)cw, (float)chh, flip, Color(0.2f, 0.4f, 0.7f, 0.5f), 1);
          if (flash > 0) P.blitEx(t, fr * cw, 0, cw, chh, x, y, (float)cw, (float)chh, flip, Color(1, 1, 1, flash), 1);
          if (a.burnT > 0 && ((int)(t_ * 10) & 1)) P.rectAdd(x + cw * 0.3f, y + chh * 0.3f, cw * 0.4f, chh * 0.5f, Color(0.6f, 0.25f, 0.05f, 0.5f));
        }
        // melee slash arc
        if ((a.st == AState::Strike || (a.st == AState::Recover && a.stT < 0.05f)) && (a.player || a.human)) {
          art::Fx f = a.face == 0 ? art::Fx::SlashDown : a.face == 1 ? art::Fx::SlashUp : art::Fx::Slash;
          const Tex& ft = fx_[(int)f];
          int fw = art::fxW(f), fh = art::fxH(f), frames = std::max(1, art::fxFrames(f));
          float prog = a.st == AState::Strike ? a.stT / 0.11f : 1.0f;
          int fr = std::min(frames - 1, (int)(prog * frames));
          Vec2 c = a.p + Vec2(0, -8) + a.aim * 11;
          Color tint = a.player && a.combo == 2 ? Color(1, 0.9f, 0.6f) : Color(1, 1, 1, 0.9f);
          float sc = a.player && a.combo == 2 ? 1.3f : 1.0f;
          P.blitEx(ft, fr * fw, 0, fw, fh, c.x - fw * sc / 2 - cam.x, c.y - fh * sc / 2 - cam.y, fw * sc, fh * sc, a.face == 3, tint);
        }
        // attack telegraph
        if (a.hostile && a.st == AState::Windup && a.mon != Monster::Dragon) {
          float lift = a.human ? 30.0f : art::monsterCellH(a.mon) + 6.0f;
          float pulse = 0.6f + 0.4f * std::sin(t_ * 30);
          if (a.heavy) {
            // heavy slam: a ground ring that fills in as the blow comes, and a double "!!" - roll out (or through)
            float wu = a.mon == Monster::Troll ? 0.9f : 0.8f;
            float k = clampf(a.stT / wu, 0, 1);
            Vec2 c = a.p + a.aim * 10.0f;
            float r = 26.0f + a.radius;
            int seg = 44;
            for (int i = 0; i < seg; i++) {   // the danger zone: a solid outline...
              float an = i * TAU / seg;
              float px = c.x + std::cos(an) * r - cam.x, py = c.y + std::sin(an) * r * 0.55f - cam.y;
              P.rect(px - 1, py - 1, 2, 2, Color(1, 0.22f, 0.12f, 0.55f + 0.45f * k));
            }
            for (float ir = 4; ir < r * k; ir += 4) {   // ...that fills from the middle as the blow comes
              int n = std::max(8, (int)(ir * 1.2f));
              for (int i = 0; i < n; i++) {
                float an = i * TAU / n;
                P.rect(c.x + std::cos(an) * ir - cam.x, c.y + std::sin(an) * ir * 0.55f - cam.y, 1, 1, Color(1, 0.35f, 0.2f, 0.25f + 0.3f * k));
              }
            }
            Color rc(1, 0.2f + 0.3f * k, 0.15f, pulse);
            P.rect(a.p.x - 4 - cam.x, a.p.y - lift - 2 - cam.y, 2, 6, rc);
            P.rect(a.p.x - 4 - cam.x, a.p.y - lift + 5 - cam.y, 2, 2, rc);
            P.rect(a.p.x + 2 - cam.x, a.p.y - lift - 2 - cam.y, 2, 6, rc);
            P.rect(a.p.x + 2 - cam.x, a.p.y - lift + 5 - cam.y, 2, 2, rc);
          } else {
            Color tc = a.lunge ? Color(1, 0.65f, 0.2f, pulse) : Color(1, 0.3f, 0.2f, pulse);
            P.rect(a.p.x - 1 - cam.x, a.p.y - lift - cam.y, 2, 5, tc);
            P.rect(a.p.x - 1 - cam.x, a.p.y - lift + 6 - cam.y, 2, 2, tc);
            if (a.lunge) {   // a short streak showing where the lunge will go
              for (int i = 1; i <= 4; i++) {
                Vec2 q = a.p + Vec2(0, -3) + a.aim * (6.0f + i * 5.0f);
                P.rect(q.x - cam.x, q.y - cam.y, 1, 1, Color(1, 0.65f, 0.2f, 0.5f - i * 0.08f));
              }
            }
          }
        }
        // health bar for damaged enemies
        if (a.hostile && a.st != AState::Dead && a.st != AState::Down && a.hp < a.maxHp && !a.boss) {
          float lift = a.fly ? 34 : 0;
          float bw = 16, x = a.p.x - bw / 2 - cam.x, y = a.p.y - (a.human ? 26 : art::monsterCellH(a.mon) + 2) - lift - cam.y;
          P.rect(x - 1, y - 1, bw + 2, 4, Color(0.05f, 0.03f, 0.05f, 0.8f));
          P.rect(x, y, bw * clampf(a.hp / a.maxHp, 0, 1), 2, Color(0.85f, 0.2f, 0.18f));
        }
        break;
      }
      case 4: {
        const Pickup& k = g.pickups[d.idx];
        float bob = std::sin(t_ * 4 + d.idx) * 1.5f;
        const Tex& t = k.gold > 0 ? iconTex(art::Icon::Gold, 0) : iconTex(k.item.icon, k.item.tint);
        P.blitEx(shadow_, 0, 0, 14, 6, k.p.x - 5 - cam.x, k.p.y - 2 - cam.y, 10, 4, false, Color(1, 1, 1, 0.8f));
        P.blit(t, k.p.x - 8 - cam.x, k.p.y - 16 + bob - cam.y);
        if (k.gold == 0 && k.item.rarity >= Rarity::Rare && ((int)(t_ * 3 + d.idx) % 3 == 0))
          P.rectAdd(k.p.x - 1 - cam.x, k.p.y - 18 + bob - cam.y, 2, 2, col(rarityColor(k.item.rarity), 0.8f));
        break;
      }
      case 5: {
        const Projectile& pr = g.projs[d.idx];
        Vec2 dir = norm(pr.v);
        float x = pr.p.x - cam.x, y = pr.p.y - cam.y;
        switch (pr.kind) {
          case ProjKind::Arrow:
            for (int i = 0; i < 8; i++) P.rect(x - dir.x * i, y - dir.y * i, 1, 1, i < 2 ? Color(0.85f, 0.85f, 0.9f) : (i > 5 ? Color(0.95f, 0.95f, 0.95f) : Color(0.55f, 0.38f, 0.2f)));
            break;
          case ProjKind::Fireball: case ProjKind::DragonFire: {
            const Tex& t = fx_[(int)art::Fx::Fireball];
            int fw = art::fxW(art::Fx::Fireball), fh = art::fxH(art::Fx::Fireball), frames = std::max(1, art::fxFrames(art::Fx::Fireball));
            int fr = (int)(t_ * 14 + d.idx) % frames;
            float sc = pr.kind == ProjKind::DragonFire ? 1.5f : 1.0f;
            P.blitEx(t, fr * fw, 0, fw, fh, x - fw * sc / 2, y - fh * sc / 2, fw * sc, fh * sc, dir.x < 0);
            Rng r((uint32_t)(t_ * 60) + d.idx);
            Particle q; q.p = pr.p; q.v = Vec2(r.range(-10, 10), r.range(-10, 10)); q.life = q.max = 0.3f; q.c = Color(1, r.range(0.3f, 0.7f), 0.1f); q.size = 1; parts_.push_back(q);
            break;
          }
          case ProjKind::IceSpike: {
            const Tex& t = fx_[(int)art::Fx::Frost];
            int fw = art::fxW(art::Fx::Frost), fh = art::fxH(art::Fx::Frost);
            P.blitEx(t, 0, 0, fw, fh, x - fw / 2.0f, y - fh / 2.0f, (float)fw, (float)fh, dir.x < 0);
            break;
          }
          case ProjKind::Spit:
            P.rect(x - 2, y - 2, 4, 4, Color(0.55f, 0.85f, 0.3f));
            P.rect(x - 1, y - 1, 2, 2, Color(0.8f, 1, 0.6f));
            break;
          case ProjKind::Magic:
            P.rectAdd(x - 3, y - 3, 6, 6, Color(0.4f, 0.3f, 0.9f, 0.7f));
            P.rect(x - 1, y - 1, 2, 2, Color(0.85f, 0.8f, 1));
            break;
        }
        break;
      }
      default: break;
    }
  }
  // the hidden hero: a cool, see-through silhouette over whatever hides it, so it reads as "behind" the roof or crown
  // rather than standing on it (a faint additive rim keeps it visible on dark slate as well as on pale thatch)
  if (ghost && ghostTex && g.mode != Mode::Title) {
    const float W = (float)art::HUMAN_W, H = (float)art::HUMAN_H;
    P.blitEx(*ghostTex, ghostFr * art::HUMAN_W, ghostRow * art::HUMAN_H, art::HUMAN_W, art::HUMAN_H, ghostX, ghostY, W, H, ghostFlip, Color(0.42f, 0.48f, 0.72f, 0.55f));
    P.blitEx(*ghostTex, ghostFr * art::HUMAN_W, ghostRow * art::HUMAN_H, art::HUMAN_W, art::HUMAN_H, ghostX, ghostY, W, H, ghostFlip, Color(0.18f, 0.22f, 0.36f, 0.5f), 1);
  }
  // particles (world layer)
  for (const Particle& q : parts_) {
    float a = clampf(q.life / q.max, 0, 1);
    if (q.kind == 1) {
      art::Fx f = (art::Fx)q.fx;
      const Tex& t = fx_[q.fx];
      int fw = art::fxW(f), fh = art::fxH(f), frames = std::max(1, art::fxFrames(f));
      int fr = std::min(frames - 1, (int)((1 - a) * frames));
      P.blitRegion(t, fr * fw, 0, fw, fh, q.p.x - fw / 2.0f - cam.x, q.p.y - fh / 2.0f - cam.y);
      continue;
    }
    if (q.kind == 2) {   // chimney smoke: grows and fades as it rises
      float sz = q.size + (1 - a) * 2.5f;
      P.rect(q.p.x - sz * 0.5f - cam.x, q.p.y - sz * 0.5f - cam.y, sz, sz, Color(q.c.r, q.c.g, q.c.b, 0.55f * a * std::min(1.0f, (1 - a) * 6 + 0.2f)));
      continue;
    }
    if (q.layer == 1) { P.rectAdd(q.p.x - cam.x, q.p.y - cam.y, 1, 1, Color(q.c.r, q.c.g, q.c.b, a * (0.5f + 0.5f * std::sin(t_ * 5 + q.p.x)))); continue; }
    P.rect(q.p.x - cam.x, q.p.y - cam.y, q.size, q.size, Color(q.c.r, q.c.g, q.c.b, std::min(1.0f, a * 1.5f)));
  }
}

void View::drawLighting(Game& g) {
  Pix& P = *pix_;
  const Map& m = g.mode == Mode::Title ? g.world.over : g.map();
  float day = g.daylight();
  Color amb;
  bool interior = g.mode != Mode::Title && g.inside && g.subBldg >= 0;
  bool dungeon = g.mode != Mode::Title && g.inside && g.subSite >= 0;
  if (dungeon) amb = Color(0.46f, 0.42f, 0.52f);   // (M1: readable on a phone; torches and the hero's light lift it further)
  else if (interior) {
    // M0b: rooms follow the day; by night only the hearths, candles and lamps keep them lit
    float d = clampf(day, 0, 1);
    amb = Color(lerpf(0.36f, 0.78f, d), lerpf(0.32f, 0.68f, d), lerpf(0.44f, 0.58f, d));
  }
  else {
    Color night(0.12f, 0.15f, 0.30f), dusk(1.0f, 0.72f, 0.55f), noon(1, 1, 1);
    float h = g.hour;
    bool evening = h > 12;
    if (day >= 1) amb = noon;
    else if (day <= 0) amb = night;
    else {
      Color mid = evening ? dusk : Color(1.0f, 0.82f, 0.7f);
      amb = day > 0.5f ? Color(lerpf(mid.r, 1, (day - 0.5f) * 2), lerpf(mid.g, 1, (day - 0.5f) * 2), lerpf(mid.b, 1, (day - 0.5f) * 2))
                       : Color(lerpf(night.r, mid.r, day * 2), lerpf(night.g, mid.g, day * 2), lerpf(night.b, mid.b, day * 2));
    }
  }
  if (amb.r > 0.99f && amb.g > 0.99f && amb.b > 0.99f) return;
  Vec2 cam(std::floor(cam_.x + shakeOff_.x), std::floor(cam_.y + shakeOff_.y));
  // (M1) the light map is half the canvas: re-made when the screen fit changes the canvas size
  if (lightMap_.w != (Pix::W + 1) / 2 || lightMap_.h != (Pix::H + 1) / 2) {
    P.destroy(lightMap_);
    lightMap_ = P.makeTarget((Pix::W + 1) / 2, (Pix::H + 1) / 2);
  }
  P.setTarget(&lightMap_);
  SDL_Renderer* ren = P.renderer();
  (void)ren;
  P.rect(0, 0, (float)lightMap_.w, (float)lightMap_.h, amb);
  float dark = 1 - (amb.r + amb.g + amb.b) / 3;
  auto light = [&](Vec2 wp, float r, Color c, float k) {
    float x = (wp.x - cam.x) * 0.5f, y = (wp.y - cam.y) * 0.5f, rr = r * 0.5f;
    if (x < -rr || y < -rr || x > lightMap_.w + rr || y > lightMap_.h + rr) return;
    P.blitEx(light_, 0, 0, 64, 64, x - rr, y - rr, rr * 2, rr * 2, false, Color(c.r, c.g, c.b, k), 1);
  };
  float flick = 0.9f + 0.1f * std::sin(t_ * 13) * std::sin(t_ * 7.3f);
  // the player carries a torch at night / underground
  if (g.mode != Mode::Title) light(g.pl().p + Vec2(0, -8), dungeon ? 150.0f : 100.0f, Color(1, 0.85f, 0.65f), (dungeon ? 0.95f : 0.8f * dark) * flick);
  int tx0 = (int)std::floor(cam.x / 16) - 6, ty0 = (int)std::floor(cam.y / 16) - 6;
  for (int ty = std::max(0, ty0); ty < std::min(m.h, ty0 + Pix::H / 16 + 12); ty++)
    for (int tx = std::max(0, tx0); tx < std::min(m.w, tx0 + Pix::W / 16 + 12); tx++) {
      int pr = m.prop[(size_t)ty * m.w + tx];
      if (!pr) continue;
      float r; Color c;
      if (interior && (Prop)(pr - 1) == Prop::Window) {   // daylight falls in through the windows
        if (day > 0.05f) light(Vec2(tx * 16 + 8.0f, ty * 16 + 22.0f), 70, Color(1.0f, 0.95f, 0.82f), 0.55f * clampf(day, 0, 1));
        continue;
      }
      if (!interior && art::isStall((Prop)(pr - 1))) {   // (M1 fixer) an open stall's lantern at dusk
        if (dark > 0.15f && m.kind == MapKind::Overworld && ew::stallOpen(tx + g.world.ox, ty + g.world.oy, g.hour)) {
          const float f = 0.9f + 0.1f * std::sin(t_ * 7 + tx * 1.3f);
          const float k = std::min(1.0f, dark * 2.2f) * f;
          light(Vec2(tx * 16 - 9.5f, ty * 16 - 16.5f), 34, Color(1.0f, 0.86f, 0.55f), 0.95f * k);
          light(Vec2(tx * 16 + 8.0f, ty * 16 + 4.0f), 72, Color(1.0f, 0.66f, 0.34f), 0.6f * k);
        }
        continue;
      }
      if (!interior && (Prop)(pr - 1) == Prop::MarketTable) {   // (M1 fixer round 2) an open table's candle lantern
        if (dark > 0.15f && m.kind == MapKind::Overworld && ew::stallOpen(tx + g.world.ox, ty + g.world.oy, g.hour)) {
          const float f = 0.9f + 0.1f * std::sin(t_ * 7 + tx * 1.7f);
          const float k = std::min(1.0f, dark * 2.2f) * f;
          light(Vec2(tx * 16 + 20.5f, ty * 16 - 1.0f), 26, Color(1.0f, 0.86f, 0.55f), 0.9f * k);
          light(Vec2(tx * 16 + 16.0f, ty * 16 + 6.0f), 54, Color(1.0f, 0.66f, 0.34f), 0.5f * k);
        }
        continue;
      }
      if (!propLight((Prop)(pr - 1), r, c)) continue;
      float f = 0.85f + 0.15f * std::sin(t_ * 9 + tx * 1.7f + ty);
      light(Vec2(tx * 16 + 8.0f, ty * 16 + 4.0f), r, c, 0.9f * f);
    }
  // warm windows at night: each lit window glows (it reads at full colour through the dark) and spills a little
  // light onto the wall and the street in front; a soft pool at the door of every household still up
  static std::vector<int> litB;
  if (!g.inside && dark > 0.2f && g.mode != Mode::Title) bldgsIn(g, m, cam.x - 80, cam.y - 80, cam.x + Pix::W + 80, cam.y + Pix::H + 120, litB);
  else litB.clear();
  for (int bi : litB) {
    {
      const Bldg& b = m.bldgs[(size_t)bi];
      Vec2 c(b.r.x * 16 + b.r.w * 8.0f, (b.r.y + b.r.h) * 16 - 10.0f);
      if (c.x < cam.x - 80 || c.x > cam.x + Pix::W + 80 || c.y < cam.y - 120 || c.y > cam.y + Pix::H + 80) continue;
      if (!windowsLit(g, b)) continue;
      light(c + Vec2(0, 8), 18 + b.r.w * 4.0f, Color(1, 0.7f, 0.35f), 0.40f * dark);
      auto w = bldgWin_.find(bldgKey(m, b, (int)bi));
      auto t = bldgTex_.find(bldgKey(m, b, (int)bi));
      if (w == bldgWin_.end() || t == bldgTex_.end()) continue;
      const float bottom = (b.r.y + b.r.h) * 16.0f, top = bottom + art::BLDG_PAD_B - t->second.h;
      float f = 0.94f + 0.06f * std::sin(t_ * 5 + bi);
      for (const Vec2& wc : w->second) {
        Vec2 wp(b.r.x * 16.0f - art::BLDG_PAD_X + wc.x, top + wc.y);
        light(wp, 22, Color(1, 0.86f, 0.6f), 0.95f * dark * f);   // the pane itself, near full brightness
        light(wp + Vec2(0, 8), 44, Color(1, 0.62f, 0.3f), 0.30f * dark * f);   // spill on the wall and ground
      }
    }
  }
  // the lanterns either side of every gate passage
  if (!g.inside && m.kind == MapKind::Overworld && dark > 0.15f)
    for (auto& gt : g.world.gates) {
      Vec2 c(gt.first * 16 + 24.0f, gt.second * 16.0f);
      if (c.x < cam.x - 80 || c.x > cam.x + Pix::W + 80 || c.y < cam.y - 80 || c.y > cam.y + Pix::H + 80) continue;
      float f = 0.9f + 0.1f * std::sin(t_ * 8 + gt.first);
      light(Vec2(gt.first * 16.0f, gt.second * 16.0f - 1), 40, Color(1, 0.72f, 0.38f), 0.75f * f);
      light(Vec2(gt.first * 16.0f + 47, gt.second * 16.0f - 1), 40, Color(1, 0.72f, 0.38f), 0.75f * f);
    }
  for (const Projectile& pr : g.projs) {
    if (pr.kind == ProjKind::Fireball || pr.kind == ProjKind::DragonFire) light(pr.p, 60, Color(1, 0.6f, 0.25f), 0.9f);
    else if (pr.kind == ProjKind::IceSpike || pr.kind == ProjKind::Magic) light(pr.p, 40, Color(0.5f, 0.6f, 1), 0.8f);
  }
  for (const Particle& q : parts_) if (q.kind == 1 && q.fx == (int)art::Fx::Explosion) light(q.p, 90, Color(1, 0.6f, 0.3f), q.life / q.max);
  for (const Actor& a : g.actors) if (a.mon == Monster::Wraith && a.hostile && a.st != AState::Dead) light(a.p + Vec2(0, -10), 40, Color(0.5f, 0.5f, 1), 0.6f);
  for (const Pickup& k : g.pickups) if (k.gold == 0 && k.item.rarity >= Rarity::Rare) light(k.p, 24, col(rarityColor(k.item.rarity)), 0.5f);
  P.setTarget(nullptr);
  P.blitEx(lightMap_, 0, 0, lightMap_.w, lightMap_.h, 0, 0, lightMap_.w * 2.0f, lightMap_.h * 2.0f, false, Color(1, 1, 1, 1), 2);
}

void View::drawWeather(Game& g, float dt) {
  (void)dt;
  Pix& P = *pix_;
  if (g.inside || g.mode == Mode::Title) {
    P.blitEx(vignette_, 0, 0, vignette_.w, vignette_.h, 0, 0, (float)Pix::W, (float)Pix::H, false, Color(1, 1, 1, g.inside && g.subSite >= 0 ? 1.0f : 0.6f));
    return;
  }
  const Actor& p = g.pl();
  Biome b = g.world.over.biomeAt((int)(p.p.x / 16), (int)(p.p.y / 16));
  uint32_t w = hash32((uint32_t)(g.day * 8 + (int)(g.hour / 3)) ^ (uint32_t)g.seed) % 10;
  bool cold = b == Biome::Snow || b == Biome::Mountain || b == Biome::Taiga;
  bool rain = w < 3 && !cold && b != Biome::Desert;
  bool snow = cold && w < 6;
  bool fog = b == Biome::Swamp;
  if (rain) {
    P.rect(0, 0, Pix::W, Pix::H, Color(0.1f, 0.12f, 0.2f, 0.18f));
    for (int i = 0; i < 90 * Pix::W / 480; i++) {
      const float ww = Pix::W + 40.0f, wh = Pix::H + 30.0f;   // (M1) the drops wrap over the whole canvas
      float sx = std::fmod(hashf(i, 0, 5) * 600 + t_ * 120 - cam_.x * 1.0f, ww);
      float sy = std::fmod(hashf(i, 1, 5) * 400 + t_ * 330 - cam_.y * 1.0f, wh);
      if (sx < 0) sx += ww;
      if (sy < 0) sy += wh;
      sx -= 20; sy -= 15;
      for (int k = 0; k < 4; k++) P.rect(sx - k * 0.5f, sy - k * 1.5f, 1, 1, Color(0.7f, 0.8f, 0.95f, 0.45f - k * 0.08f));
    }
  }
  if (snow) {
    for (int i = 0; i < 110 * Pix::W / 480; i++) {
      float sp = 14 + hashf(i, 3, 7) * 18;
      const float ww = Pix::W + 20.0f, wh = Pix::H + 20.0f;
      float sx = std::fmod(hashf(i, 0, 7) * 600 + std::sin(t_ * 0.8f + i) * 10 - cam_.x * 1.0f + t_ * 6, ww);
      float sy = std::fmod(hashf(i, 1, 7) * 400 + t_ * sp - cam_.y * 1.0f, wh);
      if (sx < 0) sx += ww;
      if (sy < 0) sy += wh;
      sx -= 10; sy -= 10;
      float s = hashf(i, 2, 7) < 0.3f ? 2.f : 1.f;
      P.rect(sx, sy, s, s, Color(1, 1, 1, 0.8f));
    }
  }
  if (fog) {
    for (int i = 0; i < 6 * (Pix::W + 220) / 700 + 1; i++) {
      float x = std::fmod(i * 140 + t_ * 8 - cam_.x * 0.3f + 7000.0f, (float)Pix::W + 220.0f) - 160;
      P.blitEx(light_, 0, 0, 64, 64, x, 40 + i * 30 + std::sin(t_ * 0.3f + i) * 10, 260, 90, false, Color(0.75f, 0.8f, 0.75f, 0.12f));
    }
  }
  // falling leaves / fireflies are world particles drawn earlier; light vignette always
  P.blitEx(vignette_, 0, 0, vignette_.w, vignette_.h, 0, 0, (float)Pix::W, (float)Pix::H, false, Color(1, 1, 1, 0.45f));
}
