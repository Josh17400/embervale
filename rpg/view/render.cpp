// World rendering: sprite bank, y-sorted scene, effects, lighting, weather, event handling.
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include "rpg/art/art_parts.h"
#include "rpg/sim/deco.h"
#include "rpg/view/prop_traits.h"
#include "rpg/view/script_api.h"
#include "rpg/view/view.h"
#include "rpg/world/biomes.h"
#include "rpg/world/economy.h"
#include "rpg/world/ids.h"
#include "rpg/world/source.h"

using art::Prop;
using art::Monster;

namespace {
// (M5 fixer r2) a townsperson standing in a bath's pool (life_game.cpp bathe): drawn chest-deep, no floor shadow
bool bathing(const Game& g, const Map& m, const Actor& a) {
  if (m.kind != MapKind::Interior || !a.human || a.player || !a.npc || a.posture != art::Posture::None || a.st != AState::Idle) return false;
  (void)g;
  const int tx = (int)std::floor(a.p.x / 16.0f), ty = (int)std::floor(a.p.y / 16.0f);
  return m.in(tx, ty) && groundWater(m.at(tx, ty));
}
Color col(uint32_t c, float a = 1) { return Color((c & 255) / 255.0f, ((c >> 8) & 255) / 255.0f, ((c >> 16) & 255) / 255.0f, a); }
Canvas paintInteriorPiece(uint64_t key) { return art::interiorPiece((uint32_t)key); }
Canvas paintStall(uint64_t key) { return art::marketStallVariant((int)(key & 255)); }
Canvas paintTradeStall(uint64_t key) {
  if ((key >> 16) & 1) art::setMarketSnow(13.0f);   // (fixer M5 r3) a snowy town's stalls carry snow on their roofs
  Canvas c = art::marketStallFacing((int)(key & 15), (int)((key >> 4) & 15), (int)((key >> 8) & 15), ((key >> 12) & 1) != 0, (int)((key >> 13) & 3));
  art::setMarketSnow(1e9f);
  return c;
}
// (stall facings) the market's 3/4 models: their ground shadows and the carts
Canvas paintStallShadow(uint64_t key) { return art::marketStallShadow((int)(key & 15), (int)((key >> 4) & 3)); }
Canvas paintTableShadow(uint64_t key) { return art::marketTableShadow((int)(key & 15)); }
Canvas paintClothShadow(uint64_t) { return art::groundClothShadow(); }
Canvas paintCart(uint64_t key) {
  if ((key >> 8) & 1) art::setMarketSnow(4.0f);
  Canvas c = art::marketCart((int)(key & 15));
  art::setMarketSnow(1e9f);
  return c;
}
// (fixer M5 r3) the market's props lie under snow where the trees carry it (the snow lands, high taiga)
bool snowyTile(const Map& m, int tx, int ty) {
  const Biome hb = m.biomeAt(tx, ty);
  return m.at(tx, ty) == Ground::Snow || hb == Biome::Snow || (hb == Biome::Taiga && m.heightAt(tx, ty) >= 4) ||
         (hb == Biome::Mountain && m.heightAt(tx, ty) >= 5);
}
Canvas paintCartShadow(uint64_t key) { return art::marketCartShadow((int)(key & 15)); }
// the facing of the stall whose prop stands on window tile (tx, ty) of the overworld (art_props.h StallFacing)
int stallFacingOn(const Game& g, const Map& m, int tx, int ty) {
  if (m.kind != MapKind::Overworld) return art::StallS;
  return art::stallFacingAt([&](int x, int y) { return m.propAt(x, y); }, tx, ty, g.world.ox, g.world.oy);
}
// the stall's form (cloth booth, canvas tent, timber booth): one per row (S / N, by its global row) or column (E / W)
int stallFormOn(const Game& g, int tx, int ty, int facing) {
  return facing >= art::StallE ? ew::stallFormAt(0x3C00000 ^ (tx + g.world.ox)) : ew::stallFormAt(ty + g.world.oy);
}
// a cart's look by its global tile: its load and which way its shafts point
int cartLook(const Game& g, int tx, int ty) {
  uint32_t h = (uint32_t)(tx + g.world.ox) * 374761393u + (uint32_t)(ty + g.world.oy) * 668265263u + 6247u;
  h = (h ^ (h >> 13)) * 1274126177u;
  return (int)((h ^ (h >> 16)) % (uint32_t)art::kCartLooks);
}
Canvas paintMarketTable(uint64_t key) { return art::marketTable((int)(key & 15), (int)((key >> 4) & 15), ((key >> 8) & 1) != 0); }
Canvas paintGroundCloth(uint64_t key) { return art::groundCloth((int)(key & 15), (int)((key >> 4) & 15), ((key >> 8) & 1) != 0); }
Canvas paintMineRail(uint64_t key) { return art::mineRail((int)(key & 15)); }
Canvas paintMineHill(uint64_t key) { return art::mineHill((int)(key & 3), (int)((key >> 2) & 3)); }
Canvas paintRuin(uint64_t key) { return art::ruinVariant((Prop)((key >> 8) & 255), (int)(key & 255)); }
Canvas paintTallGrass(uint64_t key) { return art::tallGrassVariant((int)(key & 15)); }
// (fixer M5 r3) a floor cushion under someone sitting cross-legged on the bare floor (a low table's or a hearth's
// place): a plump square in one of four dyes, its top face lit from the upper left, a darker front side, a piped edge,
// a button tuft in the middle and tassels at the near corners; 18 x 10, the seat line 6 px above its foot
Canvas paintFloorCushion(uint64_t key) {
  static const uint32_t dyes[4][5] = {
      {rgba(70, 20, 26), rgba(118, 32, 36), rgba(162, 52, 44), rgba(196, 86, 58), rgba(226, 132, 92)},     // madder
      {rgba(28, 34, 72), rgba(42, 60, 112), rgba(62, 92, 152), rgba(98, 132, 186), rgba(150, 182, 220)},   // indigo
      {rgba(82, 56, 22), rgba(132, 92, 34), rgba(176, 130, 48), rgba(206, 166, 72), rgba(232, 204, 120)},  // ochre
      {rgba(30, 52, 36), rgba(46, 82, 52), rgba(68, 116, 70), rgba(102, 150, 92), rgba(150, 190, 128)}};   // moss
  const uint32_t* R = dyes[key & 3];
  const uint32_t trim = rgba(226, 200, 140), shade = rgba(20, 16, 30, 90);
  Canvas c(18, 10);
  // the soft shadow it throws to the lower right
  for (int x = 3; x < 18; x++) c.set(x, 9, shade);
  for (int y = 3; y < 9; y++) c.set(17, y, shade);
  for (int y = 0; y < 9; y++)
    for (int x = 0; x < 17; x++) {
      const bool corner = (x == 0 || x == 16) && (y == 0 || y == 8);
      if (corner) continue;
      uint32_t col;
      if (y >= 6) col = y == 8 ? R[0] : R[1];                       // the front side, in shade
      else {
        const float u = (x - 8.0f) / 8.0f, v = (y - 2.5f) / 3.0f;
        const float lit = -0.55f * u - 0.45f * v - 0.6f * (u * u + v * v) + 0.15f;   // plump: lit up-left, falling off
        col = lit > 0.25f ? R[4] : lit > -0.05f ? R[3] : lit > -0.45f ? R[2] : R[1];
      }
      if (y == 0 || x == 0 || x == 16) col = y == 0 || x == 0 ? R[3] : R[1];   // the piping catches the light
      if (y == 5 && x > 0 && x < 16) col = R[1];                               // the crease where top meets front
      c.set(x, y, col);
    }
  c.set(8, 2, R[0]); c.set(8, 3, trim);                                       // the button tuft
  c.set(0, 8, trim); c.set(0, 9, R[2]); c.set(16, 8, trim); c.set(16, 9, R[2]);   // tassels
  return c;
}
Canvas paintBoulder(uint64_t key) { return art::boulderVariant((int)(key & 15)); }
// (M3c) a Wildlands flora prop: bits 16..23 the prop, 8..15 the eco, 0..7 the variant
Canvas paintFlora(uint64_t key) { return art::floraVariant((art::Prop)((key >> 16) & 255), (int)(key & 255), (int)((key >> 8) & 255)); }
// (M3c fixer round 2) a leafy tree in the snow (key: the prop)
Canvas paintWinterTree(uint64_t key) { return art::winterTree((art::Prop)(key & 255)); }
// (M3b) a standing stone: variant (low 4 bits) in the culture (bits 8..15: archetype + 1, 0 the land's field stone)
Canvas paintStandingStone(uint64_t key) {
  return art::standingStoneVariant((int)(key & 15), bld::standingStoneParts((int)((key >> 8) & 255), (uint32_t)(key & 15) * 2654435761u));
}
// (M3c) key: bits 0-2 the variant, 3-6 the land (prop_traits.cpp peakLand, 0..15), bit 7 a GreatPeak
Canvas paintPeak(uint64_t key) { return art::peakVariant((int)(key & 7) + ((key >> 7) & 1 ? 8 : 0), (int)((key >> 3) & 15)); }
// M2: the big wild props the hero can walk behind: they thin out like a tree crown (the ghost shows him through)
bool tallWild(Prop p) { return p == Prop::Peak || p == Prop::GreatPeak || p == Prop::ElderTree || p == Prop::Colossus || p == Prop::WatchtowerRuin; }

// M0b interiors: stairs and doors in the room's material, wall decor fitted to a partition's short face
// (interiorPropKey, rpg/sim/deco.h). A door into a private room (a bedroom, a guest room, the stockroom) stands shut
// until someone comes near it; other doors stand open.
uint32_t interiorPropTexKey(const Game& g, const Map& m, int tx, int ty, Prop p, int frame = 0) {
  uint32_t key = interiorPropKey(m, tx, ty, p);
  if (key && (key & 255u) == (uint32_t)art::Piece::Culture && p == Prop::Hearth) return key | ((uint32_t)(frame & 3) << 24);   // (M3 fixer)
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
// (M5, ART bedFit) the berth a bed prop on tile (tx, ty) is (its culture kit for a people's own bed; -1 the classic)
art::Berth berthAt(const Map& m, int tx, int ty, int& kit) {
  kit = -1;
  const int pr = m.propAt(tx, ty);
  if (!pr) return art::Berth::Ground;
  const Prop p = (Prop)(pr - 1);
  const uint32_t key = interiorPropKey(m, tx, ty, p);
  const uint32_t kind = key & 31u, style = (key >> 8) & 255u, a = (key >> 16) & 255u, b = (key >> 24) & 255u;
  if (key && kind == (uint32_t)art::Piece::Culture && a == (uint32_t)Prop::Bed) { kit = (int)style; return b == 1 ? art::Berth::LongBed : art::Berth::Bed; }
  if (key && kind == (uint32_t)art::Piece::Styled) {
    if (a == 4) return art::Berth::LongBed;
    if (a == 6) return art::Berth::LongHammock;
    if (a == 7) return art::Berth::LongMat;
  }
  switch (p) {
    case Prop::Bed: return art::Berth::Bed;
    case Prop::BunkBed: return art::Berth::BunkLow;
    case Prop::Hammock: return art::Berth::Hammock;
    case Prop::SleepingMat: return art::Berth::Mat;
    case Prop::Bedroll: return art::Berth::Bedroll;
    default: return art::Berth::Ground;
  }
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

// (M5 integration) a child: the rig has one adult body, so a child's sheet is the adult's with body rows taken out of
// every 16x24 cell and what was above them let down (the feet stay on the ground line, the head keeps its size: a
// child's proportions). The rig's landmarks: head rows 3..10, torso 11..16, hips 17, legs 17..21, ground 22. Two torso
// rows go always; two leg rows too unless the cell is seated (bent legs keep their shape and the seat line).
static void childBody(Canvas& c, bool legs) {
  const int W = art::HUMAN_W, H = art::HUMAN_H;
  const int drop[4] = {13, 15, 18, 20};
  const int nd = legs ? 4 : 2;
  for (int cy = 0; cy + H <= c.h; cy += H)
    for (int cx = 0; cx + W <= c.w; cx += W) {
      int dst = H - 1;
      for (int src = H - 1; src >= 0; src--) {
        bool skip = false;
        for (int d = 0; d < nd; d++) if (drop[d] == src) skip = true;
        if (skip) continue;
        for (int x = 0; x < W; x++) c.set(cx + x, cy + dst, c.get(cx + x, cy + src));
        dst--;
      }
      for (; dst >= 0; dst--) for (int x = 0; x < W; x++) c.set(cx + x, cy + dst, 0);
    }
}

const Tex& View::humanTex(const art::HumanLook& L, bool child) {
  uint64_t k = L.key() ^ (child ? 0xC41D0000C41Dull : 0);
  humanUsed_[k] = t_;
  auto it = humans_.find(k);
  if (it != humans_.end()) return it->second;
  const auto t0 = std::chrono::steady_clock::now();
  Canvas sheet = art::humanSheet(L);
  if (child) childBody(sheet, true);
  const Tex& t = humans_[k] = pix_->bake(sheet);
  if (std::getenv("EMB_TIMING")) {
    static double total = 0;
    static int n = 0;
    total += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    std::printf("character sheet painted: %d so far, %.1f ms total\n", ++n, total);
  }
  return t;
}

// (M5) a pose sheet (art::humanPostureSheet: POSTURE_FRAMES x 3 facings) by look and posture. A painted pose costs about
// a character sheet; a tavern filling up asks for a dozen at once, so a frame bakes poses only within ~2.5 ms (the web:
// 1.5 ms; at least one per frame) and returns nullptr for the rest, which stand on their standing sheet a frame or two.
bool View::poseBudget() {
  if (poseFrameT_ != t_) { poseFrameT_ = t_; poseFrameMs_ = 0; }
#ifdef __EMSCRIPTEN__
  const double budget = 1.5;
#else
  static const double budget = std::getenv("EMB_WEBSIM") ? 1.5 : 2.5;
#endif
  return poseFrameMs_ < budget;
}
void View::poseSpent(std::chrono::steady_clock::time_point t0) {
  poseFrameMs_ += std::max(0.05, std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count());
  m5Count_.poseBakes++;
}
const Tex* View::poseTex(const art::HumanLook& L, art::Posture p, uint8_t variant, bool child) {
  const uint64_t k = ew::mix64(L.key() ^ ((uint64_t)p + 1) * 0xC2B2AE3D27D4EB4Full ^ (uint64_t)variant << 56 ^ (child ? 0x5EEDC41Dull : 0));
  poseUsed_[k] = t_;
  auto it = poseTex_.find(k);
  if (it != poseTex_.end()) return &it->second;
  if (!poseBudget()) return nullptr;
  const auto t0 = std::chrono::steady_clock::now();
  Canvas sheet = art::humanPostureSheet(L, p, variant);
  if (child) childBody(sheet, !art::postureInfo(p).seated);
  const Tex& t = poseTex_[k] = pix_->bake(sheet);
  poseSpent(t0);
  return &t;
}
const Tex* View::sleeperTex(const art::HumanLook& L, art::Berth b, int kit, int frame) {
  const uint64_t k = ew::mix64(L.key() ^ 0x51EE9E5ull ^ ((uint64_t)b << 40) ^ ((uint64_t)(kit + 1) << 48) ^ ((uint64_t)(frame & 1) << 60));
  poseUsed_[k] = t_;
  auto it = poseTex_.find(k);
  if (it != poseTex_.end()) return &it->second;
  if (!poseBudget()) return nullptr;
  const auto t0 = std::chrono::steady_clock::now();
  const Tex& t = poseTex_[k] = pix_->bake(art::sleeperSprite(L, b, kit, frame & 1));
  poseSpent(t0);
  return &t;
}
// (M5) a village animal's sheet by kind and coat (variant: 16 coats at most per kind)
const Tex& View::critterTex(int kind, uint32_t variant) {
  const uint64_t k = (uint64_t)kind << 8 | (variant & 15u);
  auto it = critterTex_.find(k);
  if (it != critterTex_.end()) return it->second;
  return critterTex_[k] = pix_->bake(art::critterSheet((art::Critter)kind, variant & 15u));
}
// (M5) a lamppost whose lamp is out (by day, or before the lamplighter's round reaches it): its sprite with every
// lit pixel (the glass, the flame, the glowing paper or orb: warm or pale and bright) turned to dark, cold glass with a
// faint top-left glint, so the same lamp reads unlit in the same light
const Tex& View::lampDarkTex(const art::PropStyle* ps) {
  const uint64_t k = ps && !ps->classic() ? ew::mix64(ps->key() ^ 0x1A3D0FFull) : 0;
  auto it = lampDark_.find(k);
  if (it != lampDark_.end()) return it->second;
  Canvas c = ps && !ps->classic() ? art::propSprite(Prop::Lamppost, *ps) : art::propSprite(Prop::Lamppost);
  // the lit pixels: bright and warm (glass, flame, paper) or bright and pale-cold (an elven orb)
  auto lit = [](uint32_t v) {
    if ((v >> 24) < 40) return false;
    const int r = (int)(v & 255), gg = (int)((v >> 8) & 255), b = (int)((v >> 16) & 255);
    const int mx = std::max(r, std::max(gg, b)), mn = std::min(r, std::min(gg, b));
    if (mx < 170) return false;
    const bool warm = r >= 200 && gg >= 95 && r - b >= 60;
    const bool pale = mx >= 215 && mn >= 175;   // a white-hot core or a pale glow
    return warm || pale;
  };
  std::vector<uint8_t> mask((size_t)c.w * c.h, 0);
  int minY = c.h, maxY = -1;
  for (int y = 0; y < c.h; y++)
    for (int x = 0; x < c.w; x++)
      if (lit(c.get(x, y))) { mask[(size_t)y * c.w + x] = 1; minY = std::min(minY, y); maxY = std::max(maxY, y); }
  for (int y = 0; y < c.h; y++)
    for (int x = 0; x < c.w; x++) {
      if (!mask[(size_t)y * c.w + x]) continue;
      const uint32_t v = c.get(x, y);
      const int r = (int)(v & 255), gg = (int)((v >> 8) & 255), b = (int)((v >> 16) & 255);
      // its brightness kept as a little shading, the hue taken to a dusky blue-grey glass
      const float lum = (r * 0.3f + gg * 0.55f + b * 0.15f) / 255.0f;
      const float t = maxY > minY ? (float)(y - minY) / (float)(maxY - minY) : 0.5f;
      const float k2 = 0.20f + 0.12f * lum - 0.07f * t;   // darker toward the bottom (the light comes from above)
      int nr = (int)(255 * k2 * 0.80f), ng = (int)(255 * k2 * 0.88f), nb = (int)(255 * k2 * 1.12f);
      // the sky caught on the glass: its top-left pixels (the light comes from the top-left)
      const bool left = x == 0 || !mask[(size_t)y * c.w + x - 1], top = y == 0 || !mask[(size_t)(y - 1) * c.w + x];
      const bool top2 = y >= 2 && mask[(size_t)(y - 1) * c.w + x] && !mask[(size_t)(y - 2) * c.w + x];
      if (left && top) { nr += 70; ng += 78; nb += 86; }
      else if ((left && top2) || (top && x >= 1 && mask[(size_t)y * c.w + x - 1] && (x < 2 || !mask[(size_t)y * c.w + x - 2]))) { nr += 34; ng += 38; nb += 44; }
      c.set(x, y, rgba(std::min(255, nr), std::min(255, ng), std::min(255, nb), (int)(v >> 24)));
    }
  return lampDark_[k] = pix_->bake(c);
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
    auto gl = bldgGlow_.find(k);
    if (gl != bldgGlow_.end()) { pix_->destroy(gl->second); bldgGlow_.erase(gl); }
    bldgSmoke_.erase(k); bldgTopRow_.erase(k); bldgWin_.erase(k);
  });
  trim(humans_, humanUsed_, 320, [](uint64_t) {});
  trim(poseTex_, poseUsed_, 160, [](uint64_t) {});   // (M5) pose sheets: a busy tavern holds a few dozen
  if (lampDark_.size() > 64) { for (auto& kv : lampDark_) pix_->destroy(kv.second); lampDark_.clear(); }
  if (festTex_.size() > 96) { for (auto& kv : festTex_) pix_->destroy(kv.second); festTex_.clear(); }
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
  std::printf("perf: frame avg %.1f ms worst %.1f ms | bakes %d (+%d inline) %.1f ms, worst %.1f ms | chunks %zu, bldg tex %zu, humans %zu, wall tiles %zu, styled props %zu, gates+banners %zu\n",
              perf_.frameMs / std::max(1, perf_.frames), perf_.worstFrameMs, perf_.bakes, perf_.inlineBakes, perf_.bakeMs, perf_.worstBakeMs,
              chunks_.size(), bldgTex_.size(), humans_.size(), wallTiles_.size(), styledProps_.size(), kingdomTex_.size());
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
// (M1) the building's own biome (Bldg::biome, where it stands), not the tile's: an endless window may not cover it.
// (M3) the culture's style the settlement generator chose (Bldg::arch; bldgArch falls back to the biome style).
static art::ArchStyle bldgStyle(const Map& m, const Bldg& b) {
  (void)m;
  if (archBiomeOverride() >= 0)
    return art::withRoofTint(art::urbanize(art::archForBiome(archBiomeOverride(), b.seed), b.urban, b.seed), b.roof);
  return bldgArch(b);
}
// M3b: the builder's request for a building as the view paints it (the style override above applied), and its
// blueprint: every building sprite is painted from its blueprint (rpg/build/blueprint.h)
static bld::Request bldgViewRequest(const Map& m, const Bldg& b) {
  bld::Request r = bldgRequest(b);
  r.style = bldgStyle(m, b);
  return r;
}
uint64_t View::bldgKey(const Map& m, const Bldg& b, int index) const {
  uint64_t k = bldgViewRequest(m, b).key();
  k ^= (uint64_t)index * 0x9E3779B97F4A7C15ull;
  return k;
}
// A building sprite and what the renderer reads off it (chimney mouths, the first opaque row, the lit-window night
// variant and its window centres). A pure function of the building (M2: an arrival paints them on worker threads,
// paintBldg; storeBldg turns them into textures on the main thread).
View::BldgPaint View::paintBldg(const Bldg& b, uint64_t key) {
  static const Map empty;
  art::BuildingInfo info;
  Canvas c = art::buildingSprite(bld::design(bldgViewRequest(empty, b)), &info);
  return paintBldgPost(std::move(c), info, key, b.seed);
}
View::BldgPaint View::paintBldgPost(Canvas canvas, art::BuildingInfo& info, uint64_t key, uint32_t seed) {
  BldgPaint p;
  p.key = key;
  p.c = std::move(canvas);
  const Canvas& c = p.c;
  for (int i = 0; i < info.smokeN; i++) p.smoke.push_back(Vec2((float)info.smokeX[i], (float)info.smokeY[i]));
  int topRow = 0;
  for (bool found = false; topRow < c.h && !found; topRow++)
    for (int x = 0; x < c.w; x++) if ((c.get(x, topRow) >> 24) > 96) { found = true; break; }
  p.topRow = topRow;
  // night: the lit-window variant and one light pool per window (centre of each connected run of panes)
  for (uint8_t v : info.glass) if (v) { p.anyGlass = true; break; }
  // (fixer M5 r3, review: "steppe yurts show no occupancy light at night") a building with no window at all (a yurt, a
  // tent: its light comes through the door and the crown) shows its household awake by the open doorway: the dark of
  // each painted doorway is lit like a pane (the lamp inside seen past the rolled-up flap or the drawn curtain)
  if (!p.anyGlass && info.glass.size() == c.px.size())
    for (const auto& d : info.doors)
      for (int y = std::max(0, d[2] - 16); y <= std::min(c.h - 1, d[2]); y++)
        for (int x = std::max(0, d[0]); x <= std::min(c.w - 1, d[1]); x++) {
          const uint32_t q = c.get(x, y);
          if ((q >> 24) < 200) continue;
          const int r = (int)(q & 255), gg = (int)((q >> 8) & 255), bb = (int)((q >> 16) & 255);
          if (r + gg + bb < 150 && std::max(r, std::max(gg, bb)) - std::min(r, std::min(gg, bb)) < 40) {
            info.glass[(size_t)y * c.w + x] = 1;
            p.anyGlass = true;
          }
        }
  if (p.anyGlass) {
    p.night = art::buildingNight(c, info.glass, seed);
    // the light map's mask of the panes (half size: a cell is lit when any of its 2x2 pixels is glass)
    p.glow = Canvas((c.w + 1) / 2, (c.h + 1) / 2);
    for (int y = 0; y < c.h; y++)
      for (int x = 0; x < c.w; x++)
        if (info.glass[(size_t)y * c.w + x]) p.glow.set(x / 2, y / 2, 0xFFFFFFFFu);
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
        if (q.size() >= 4) p.wins.push_back(Vec2(sx / q.size() + 0.5f, sy / q.size() + 0.5f));
      }
  }
  return p;
}

const Tex& View::storeBldg(BldgPaint& p) {
  const uint64_t k = p.key;
  bldgSmoke_[k] = std::move(p.smoke);
  bldgTopRow_[k] = p.topRow;
  bldgWin_[k] = std::move(p.wins);
  {
    auto gl = bldgGlow_.find(k);
    if (gl != bldgGlow_.end()) { pix_->destroy(gl->second); bldgGlow_.erase(gl); }
    auto n = bldgNight_.find(k);
    if (n != bldgNight_.end()) { pix_->destroy(n->second); bldgNight_.erase(n); }
  }
  if (p.anyGlass) { bldgNight_[k] = pix_->bake(p.night); bldgGlow_[k] = pix_->bake(p.glow); }
  return bldgTex_[k] = pix_->bake(p.c);
}

// A building sprite, painted now if it is not ready (a job already under way is finished). Behind the web arrival's
// fade (no threads) a building is never painted in one go: it advances by a step within the budget and, while it is
// unfinished, a blank texture stands in and the arrival is held a frame longer (travelArrive lifts the fade only after
// two frames with nothing left to paint: arrival_.frames is wound back), so a palace is spread over frames.
const Tex& View::bldgTex(const Bldg& b, int index) {
  static const Map empty;
  const Map& m = bldgMap_ ? *bldgMap_ : empty;
  uint64_t k = bldgKey(m, b, index);
  bldgUsed_[k] = t_;
  auto it = bldgTex_.find(k);
  if (it != bldgTex_.end()) return it->second;
  if (arriving_ && pix_) {
#ifdef __EMSCRIPTEN__
    const bool st = true;
#else
    static const bool st = std::getenv("EMB_WEBSIM") != nullptr;
#endif
    if (!st) {
      // desktop: behind the arrival's black the worker threads paint the buildings (travelArrive stores them as they
      // finish, and holds the fade until every one in view is there); drawing must never paint one in the meantime
      if (!blankTex_.t) { Canvas c(1, 1); blankTex_ = pix_->bake(c); }
      return blankTex_;
    }
    if (st) {
      // one budget per frame for every stepped paint (travelArrive and drawWorld both ask)
      static float frameT = -1;
      static double frameMs = 0;
      if (frameT != t_) { frameT = t_; frameMs = 0; }
      if (frameMs < 6.0) {
        const auto t0 = std::chrono::steady_clock::now();
        const bool done = bldgPaintStep(b, index, std::min(4.0, 6.0 - frameMs));
        frameMs += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
        if (done) return bldgTex_[k];
      }
      arrival_.frames = 0;
      if (!blankTex_.t) { Canvas c(1, 1); blankTex_ = pix_->bake(c); }
      return blankTex_;
    }
  }
  bldgPaintStep(b, index, 1e9);
  return bldgTex_[k];
}

// Advance (or start) the paint of b's sprite by about budgetMs; true once it is stored. Several jobs may be under way
// (one per building); a job not touched for a few seconds is dropped by the next call.
bool View::bldgPaintStep(const Bldg& b, int index, double budgetMs) {
  static const Map empty;
  const Map& m = bldgMap_ ? *bldgMap_ : empty;
  const uint64_t k = bldgKey(m, b, index);
  if (bldgTex_.count(k)) return true;
  using Clock = std::chrono::steady_clock;
  const auto t0 = Clock::now();
  BldgJob* J = nullptr;
  for (BldgJob& j : bldgJobs_) if (j.key == k) { J = &j; break; }
  if (!J) {
    for (size_t i = 0; i < bldgJobs_.size();)
      if (t_ - bldgJobs_[i].used > 5.0f) bldgJobs_.erase(bldgJobs_.begin() + (std::ptrdiff_t)i); else i++;
    BldgJob nj;
    nj.key = k;
    nj.seed = b.seed;
    nj.job = art::beginBuilding(bld::design(bldgViewRequest(empty, b)));
    bldgJobs_.push_back(std::move(nj));
    J = &bldgJobs_.back();
  }
  J->used = t_;
  const bool done = art::stepBuilding(*J->job, budgetMs);
  const double ms = std::chrono::duration<double, std::milli>(Clock::now() - t0).count();
  static const bool timing = std::getenv("EMB_TIMING") != nullptr;
  if (budgetMs < 1e8) worstPaintStepMs_ = std::max(worstPaintStepMs_, ms);
  if (!done) {
    if (timing) std::printf("building %d: paint step %.2f ms (unfinished)\n", index, ms);
    return false;
  }
  art::BuildingInfo info;
  Canvas c = art::finishBuilding(*J->job, &info);
  const uint32_t seed = J->seed;
  for (size_t i = 0; i < bldgJobs_.size(); i++)
    if (bldgJobs_[i].key == k) { bldgJobs_.erase(bldgJobs_.begin() + (std::ptrdiff_t)i); break; }
  BldgPaint p = paintBldgPost(std::move(c), info, k, seed);
  storeBldg(p);
  bldgUsed_[k] = t_;
  if (timing) {
    static double total = 0;
    static int n = 0;
    const double all = std::chrono::duration<double, std::milli>(Clock::now() - t0).count();
    total += all;
    worstPaintStepMs_ = std::max(worstPaintStepMs_, budgetMs < 1e8 ? all : 0.0);
    std::printf("building %d painted: %d so far, %.1f ms total (last step %.2f ms, worst stepped %.2f ms)\n", index, ++n, total, all,
                worstPaintStepMs_);
  }
  return true;
}

bool View::windowsLit(const Game& g, const Bldg& b) const {
  if (g.inside || g.daylight() > 0.55f) return false;
  // never all asleep: inns, temples, keeps and (M3b) the seat of power, whatever the society builds it as
  if (b.type == art::Building::Inn || b.type == art::Building::Temple || b.type == art::Building::Keep || bldgIsSeat(b)) return true;
  // (M5) by occupancy: a household's windows are warm while someone is home and awake (the census's hourly
  // occupancy: Life::occupants, 0 = nobody in, or all asleep after the 23:00 hour), dark when the house stands empty;
  // a building whose census has not run this hour keeps the rule below
  const std::vector<Bldg>& all = g.world.over.bldgs;
  if (!all.empty() && &b >= all.data() && &b < all.data() + all.size() && b.site >= 0 && b.site < (int)g.world.sites.size()) {
    const Site& s = g.world.sites[(size_t)b.site];
    const int off = (int)(&b - all.data()) - s.bldgFirst;
    if (off >= 0 && off < s.bldgCount) {
      const int occ = g.life.occupants(g.world, b.site, off);
      if (occ == 0) return false;
      if (occ > 0) {
        // people in: lit until the household's bedtime (a lamp left burning by the last one up); by deep night only a
        // few stay lit (a late reader, a sick child)
        const uint32_t h = hash32((uint32_t)(b.seed + g.day * 7919u));
        const float bed = 22.0f + (float)(h % 4) * 0.5f + std::min(2.0f, (float)occ * 0.25f);   // a full house sits up longer
        const float hr = g.hour < 12 ? g.hour + 24 : g.hour;
        if (hr > bed && hr < 29.5f) return h % 9 == 0;
        return true;
      }
    }
  }
  // households go to bed: fewer windows lit deep in the night, each house on its own schedule
  uint32_t h = hash32((uint32_t)(b.seed + g.day * 7919u));
  float bed = 22.0f + (h % 5);   // 22..26 (26 = past 2 o'clock)
  float hr = g.hour < 12 ? g.hour + 24 : g.hour;
  if (hr > bed && hr < 29.5f) return false;
  return h % 4 != 0;
}
// (M3b) a wall tile's texture key: the key itself (M3 look), or the key without its look slot mixed with the look's
// parts (the slot's meaning changes from window to window; the parts do not)
uint64_t View::wallTexKey(uint32_t key) const {
  const uint32_t slot = (key & art::WALL_LOOK_MASK) >> art::WALL_LOOK_SHIFT;
  if (!slot || slot >= fortLooks_.size()) return key & ~art::WALL_LOOK_MASK;
  return (ew::mix64(((uint64_t)(key & ~art::WALL_LOOK_MASK) << 20) ^ bld::fortKey(fortLooks_[slot])) | (1ull << 63));
}
const Tex& View::wallTileTex(uint32_t key) {
  const uint64_t ck = wallTexKey(key);
  auto it = wallTiles_.find(ck);
  if (it != wallTiles_.end()) return it->second;
  const uint32_t slot = (key & art::WALL_LOOK_MASK) >> art::WALL_LOOK_SHIFT;
  const Canvas c = slot && slot < fortLooks_.size() ? art::wallTile(key, fortLooks_[slot]) : art::wallTile(key & ~art::WALL_LOOK_MASK);
  return wallTiles_[ck] = pix_->bake(c);
}
// (M3b) a settlement's fortifications from its culture (urban: village 0, town 1, city 2, capital 3)
bool View::fortPartsOfSite(const Game& g, int site, bld::FortParts& out) const {
  if (site < 0 || site >= (int)g.world.sites.size()) return false;
  const Site& s = g.world.sites[(size_t)site];
  if (!s.settlement()) return false;
  const cult::Culture* c = g.world.cultureOf(site);
  if (!c) return false;
  const int urban = s.capital ? 3 : (s.type == SiteType::City ? 2 : (s.type == SiteType::Town ? 1 : 0));
  out = bld::fortParts(*c, urban, s.seed);
  return true;
}
const Tex& View::fortGateTex(const cult::Heraldry& arms, const bld::FortParts& f) {
  const uint64_t key = ew::mix64(arms.key() ^ (bld::fortKey(f) * 0x9E3779B97F4A7C15ull) ^ 0x6A7E5ull);
  auto it = kingdomTex_.find(key);
  if (it != kingdomTex_.end()) return it->second;
  return kingdomTex_[key] = pix_->bake(art::gateHouse(7, arms, f));
}
// (M3b) the culture of the land at a tile for the wayside's built props: its settlement's, else its region's
const art::PropStyle* View::landStyleAt(const Game& g, int tx, int ty) {
  static const art::PropStyle classic;
  if (!g.world.endless) return &classic;
  const art::PropStyle* ps = propStyleAt(g, tx, ty);
  if (!ps->classic()) return ps;
  const uint64_t wid = (uint64_t)g.world.seed * 0x9E3779B97F4A7C15ull ^ (uint64_t)(uintptr_t)&g.world;
  if (wid != landStyleWorld_ || landStyle_.size() > 4096) { landStyleWorld_ = wid; landStyle_.clear(); }
  const int32_t gx = tx + g.world.ox, gy = ty + g.world.oy;
  const uint64_t bk = ((uint64_t)(uint32_t)(gx >> 3) << 32) | (uint32_t)(gy >> 3);
  auto it = landStyle_.find(bk);
  if (it != landStyle_.end()) return &it->second;
  const cult::Culture* c = g.world.cultureAtTile(tx, ty);
  return &(landStyle_[bk] = c ? c->props : classic);
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

// M3: the culture of the settlement a prop tile lies in decides its style (fences, wells, lamps, statues, stalls...).
// The site is looked up once per 8 x 8-tile block of the world (global tiles, so a window shift keeps the cache) and
// the style once per site; tiles outside settlements keep the classic look.
// (M5) the settlement (its Site::id) a tile of the overworld lies in, cached per 8 x 8-tile block of the world (0: none)
ew::Gid View::siteIdAt(const Game& g, int tx, int ty) {
  if (!g.world.endless) return 0;
  const uint64_t wid = (uint64_t)g.world.seed * 0x9E3779B97F4A7C15ull ^ (uint64_t)(uintptr_t)&g.world;
  if (wid != sitePropsWorld_ || propSiteBlock_.size() > 8192) { sitePropsWorld_ = wid; siteProps_.clear(); propSiteBlock_.clear(); }
  const int32_t gx = tx + g.world.ox, gy = ty + g.world.oy;
  const uint64_t bk = ((uint64_t)(uint32_t)(gx >> 3) << 32) | (uint32_t)(gy >> 3);
  auto bi = propSiteBlock_.find(bk);
  if (bi != propSiteBlock_.end()) return (ew::Gid)bi->second;
  int si = g.world.siteAt(tx, ty, 2);
  if (si >= 0 && !g.world.sites[(size_t)si].settlement()) si = -1;
  const uint64_t sid = si >= 0 ? (uint64_t)g.world.sites[(size_t)si].id : 0;
  propSiteBlock_[bk] = sid;
  return (ew::Gid)sid;
}
// (M5) a settlement's mood flags where a tile lies (life::MF_*; 0 outside settlements), and the scripts' forcing
uint16_t View::lifeFlagsAt(const Game& g, int tx, int ty) {
  if (g.mode == Mode::Title) return 0;
  const ew::Gid sid = siteIdAt(g, tx, ty);
  if (!sid) return 0;
  uint16_t f = g.life.find(sid) ? g.life.moodFlags(sid) : 0;
  if (forceFest_ >= 0) f = (uint16_t)(forceFest_ ? (f | life::MF_FESTIVAL) : (f & ~life::MF_FESTIVAL));
  if (forceShut_ >= 0) f = (uint16_t)(forceShut_ ? (f | life::MF_SHUTTERED) : (f & ~(life::MF_SHUTTERED | life::MF_FAMINE)));
  if (festivalHere(g, sid)) f |= life::MF_FESTIVAL;
  return f;
}
bool View::festivalHere(const Game& g, ew::Gid site) {
  if (forceFest_ >= 0) return forceFest_ != 0;
  return site && g.life.find(site) && (g.life.festival(site, g.day) || (g.life.moodFlags(site) & life::MF_FESTIVAL));
}
const art::PropStyle* View::propStyleAt(const Game& g, int tx, int ty) {
  static const art::PropStyle classic;
  if (!g.world.endless) return &classic;
  int si = -1;
  const uint64_t sid = (uint64_t)siteIdAt(g, tx, ty);
  if (!sid) return &classic;
  auto it = siteProps_.find(sid);
  if (it != siteProps_.end()) return &it->second;
  if (si < 0)   // the block was cached: find the settlement by its id (rare: once per settlement)
    for (size_t k = 0; k < g.world.sites.size(); k++) if ((uint64_t)g.world.sites[k].id == sid) { si = (int)k; break; }
  if (si < 0) return &classic;   // not in the records just now: draw it plain this frame, ask again next time
  const cult::Culture* C = g.world.cultureOf(si);
  return &(siteProps_[sid] = C ? C->props : classic);
}
const Tex& View::styledPropTex(art::Prop p, const art::PropStyle& st) {
  const uint64_t k = ew::mix64(st.key() ^ ((uint64_t)p << 48) ^ 0x5717ull);
  auto it = styledProps_.find(k);
  if (it != styledProps_.end()) return it->second;
  return styledProps_[k] = pix_->bake(art::propSprite(p, st));
}
// a kingdom's arms (cult::Heraldry) on a standing banner (kind 0) or a gatehouse in a wall style (kind 1)
const Tex& View::heraldryTex(int kind, const cult::Heraldry& h, uint32_t seed, int wallStyle) {
  const uint64_t key = ew::mix64(h.key() ^ ((uint64_t)kind << 60) ^ ((uint64_t)seed << 32) ^ ((uint64_t)wallStyle << 40) ^ 0x4E5Aull);
  auto it = kingdomTex_.find(key);
  if (it != kingdomTex_.end()) return it->second;
  Canvas c = kind == 0 ? art::bannerSprite(h)
                       : art::gateHouse(seed, h.field, h.charge, h.emblem, (art::CityWall)std::clamp(wallStyle, 0, (int)art::CityWall::COUNT - 1));
  return kingdomTex_[key] = pix_->bake(c);
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
      FloatText ft; ft.p = e.p + Vec2(r.range(-4, 4), 0); ft.s = e.s; ft.c = col((uint32_t)e.a);
      // (M5) words (an overheard remark, a greeting by name, a plea) are a spoken line: held over the speaker long
      // enough to read on a phone (about 2 s plus a beat per word), on a dark plate; numbers and one-word calls still
      // rise and fade
      if (e.s.size() >= 8 && e.s.find(' ') != std::string::npos && ((e.s[0] >= 'A' && e.s[0] <= 'Z') || e.s[0] == '"' || e.s[0] == 0x27)) {
        ft.speech = true;
        ft.p = e.p;
        int words = 1;
        for (char ch : e.s) if (ch == ' ') words++;
        ft.life = std::min(5.5f, 1.8f + 0.32f * (float)words);
        // the same words already hang near by (two guards of the watch, a chorus of greetings): said once
        bool dup = false;
        for (const FloatText& o : texts_)
          if (o.speech && o.s == ft.s && std::fabs(o.p.x - ft.p.x) < 120 && std::fabs(o.p.y - ft.p.y) < 60) dup = true;
        if (dup) break;
        // a new line from (about) the same speaker replaces the old one
        for (size_t k = 0; k < texts_.size();)
          if (texts_[k].speech && std::fabs(texts_[k].p.x - ft.p.x) < 10 && std::fabs(texts_[k].p.y - ft.p.y) < 14) texts_.erase(texts_.begin() + (std::ptrdiff_t)k);
          else k++;
      }
      texts_.push_back(ft);
      break;
    }
    case Ev::Discover: {
      banner_ = "DISCOVERED"; bannerSub_ = e.s; bannerT_ = 4.0f; bannerState_.clear();
      // (M2) what was found heads the line: "DISCOVERED - HUNTER'S CAMP", "WONDER DISCOVERED - ELDER TREE" (a settlement
      // keeps the plain word: the arrival banner adds its trade and kingdom to it in update())
      if (e.a >= 0 && e.a < (int)g.world.sites.size()) {
        const Site& st = g.world.sites[(size_t)e.a];
        if (st.type == SiteType::Wonder) { banner_ = std::string("WONDER DISCOVERED  -  ") + ew::poiKindName(st.type, st.kind); bannerT_ = 5.0f; }
        else if (!st.settlement()) banner_ = std::string("DISCOVERED  -  ") + ew::poiKindName(st.type, st.kind);
      }
      break;
    }
    case Ev::LevelUp:
      banner_ = "LEVEL UP"; bannerSub_ = "LEVEL " + std::to_string(e.a) + "  -  CHOOSE A STAT IN THE MENU"; bannerT_ = 4.5f; bannerState_.clear();
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
      if (e.f == 0 || e.f == 1) { bannerState_.clear(); banner_ = e.f == 1 ? "QUEST COMPLETE" : "NEW QUEST"; bannerSub_ = e.s.substr(e.s.find(':') == std::string::npos ? 0 : e.s.find(':') + 2); bannerT_ = 3.5f; }
      break;
    }
    case Ev::Notice: {
      if (!(g.noticeT > 0 && g.notice == e.s)) { Toast t; t.s = e.s; t.c = col((uint32_t)e.a); toasts_.push_back(t); }
      break;
    }
    case Ev::Shake: shake_ = std::max(shake_, e.f); break;
    case Ev::Border: heraldEvent(g, e); break;   // (M4) the herald at a kingdom's border (realm_hud.cpp)
    case Ev::News: newsEvent(g, e); break;       // (M4) a world event heard of
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
  // M2: the arrival's terrain, behind the fade (M2 fixer round 2: not while paused: finishing the journey then set the
  // fade to black with no game update to lift it)
  if (g.travel.phase == TravelPhase::Arrive) { if (g.mode == Mode::Play) travelArrive(g); }
  else if (arriving_) { arriving_ = false; arrival_ = Arrival(); }   // (the journey ended without us: headless, a death)
  t_ += dt;
  perfTick(dt);
  modeT_ += dt;
  if (g.mode != lastMode_) {
    modeT_ = 0;
    if (g.mode == Mode::Dialogue) { dlgChars_ = 0; dlgSel_ = 0; dlgFirst_ = 0; }
    if (g.mode == Mode::Shop) { shopSide_ = 0; shopSel_ = 0; shopArm_ = -1; }
    lastMode_ = g.mode;
  }
  for (const Event& e : g.events) spawnParticles(e, g);
  g.events.clear();
  // (M1) arriving in a settlement: a banner with its name and its kingdom (merged into DISCOVERED the first time)
  // (M4) its CURRENT owner in its society's word and its state; shown again when either changes while the player is
  // there (a conquest, a siege laid or broken: realm_hud.cpp arrivalText)
  if (g.mode == Mode::Play && !g.inside) {
    const int cs = g.curSite >= 0 && g.curSite < (int)g.world.sites.size() && g.world.sites[g.curSite].settlement() ? g.curSite : -1;
    ew::Gid own = 0;
    uint16_t flags = 0;
    if (cs >= 0) {
      const Site& S = g.world.sites[(size_t)cs];
      own = S.kingdom >= 0 ? g.world.kingdoms[(size_t)S.kingdom].id : 0;
      if (const realm::SettlementState* st = g.realm.settlement(S.id))
        flags = st->flags & (realm::SS_BESIEGED | realm::SS_BURNED | realm::SS_OCCUPIED | realm::SS_FAMINE | realm::SS_RUINED | realm::SS_ABANDONED);
    }
    const bool changed = cs >= 0 && cs == arriveSite_ && (own != arriveOwner_ || flags != arriveFlags_);
    if (cs != arriveSite_ || changed) {
      if (cs >= 0) {
        const Site& S = g.world.sites[(size_t)cs];
        std::string line, state;
        uint32_t stc = 0;
        arrivalText(g, cs, line, state, stc);
        const bool arrival = banner_.find(" OF ") != std::string::npos || banner_.find(" VILLAGE") != std::string::npos ||
                             banner_.find(" TOWN") != std::string::npos || banner_.find(" CITY") != std::string::npos;
        if (!line.empty()) {
          if (bannerT_ > 0 && banner_ == "DISCOVERED") { banner_ = "DISCOVERED  -  " + line; bannerState_ = state; bannerStateCol_ = stc; }
          else if (bannerT_ <= 0 || arrival || changed) {
            banner_ = line; bannerSub_ = S.name; bannerT_ = changed ? 4.0f : 3.5f;
            bannerState_ = state; bannerStateCol_ = stc;
            // (fixer M4 r1, review: three bands stacked on arrival) the arrival banner names the kingdom: the border
            // herald's plaque gives way to it rather than stacking above it
            herald_.t = 0;
          }
        }
      } else if (arriveSite_ >= 0 && arriveSite_ < (int)g.world.sites.size() && bannerT_ > 0.5f &&
                 bannerSub_ == g.world.sites[(size_t)arriveSite_].name) {
        // (fixer M4 r2) left the place (a teleport, a quick walk out): its arrival banner fades out now instead of
        // naming a town the hero no longer stands in
        bannerT_ = 0.5f;
      }
      arriveSite_ = cs;
      arriveOwner_ = own;
      arriveFlags_ = flags;
    }
  }
  if (herald_.t > 0 && g.mode == Mode::Play) herald_.t -= dt;
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
    if (!texts_[i].speech) texts_[i].p.y -= dt * 22;
    else if (texts_[i].t < 0.25f) texts_[i].p.y -= dt * 12;   // (M5) a spoken line lifts a few pixels, then holds
    if (texts_[i].t > texts_[i].life) texts_.erase(texts_.begin() + i); else i++;
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
    else if (nearSiege(g)) want = Music::Wild;   // (M4) a besieged town has no jig: the land's music in the Ominous mood
    else if (g.curSite >= 0 && (g.world.sites[g.curSite].type == SiteType::City || g.world.sites[g.curSite].type == SiteType::Town || g.world.sites[g.curSite].type == SiteType::Village)) want = Music::Town;
    else if (g.isNight()) want = Music::Night;
  }
  // (M5) the life of the place: the bard's piece in the gathering place (or faintly from its door at night), the
  // festival's music on the plaza, the crowd, the animals, the alarm bell through a night raid
  bool festive = false;
  want = lifeMusic(g, want, dt, festive);
  // M3: the Town / Wild / Night pieces play in the culture of the place (the settlement's, else the land's: its
  // kingdom's dialect or its culture cell's family), so crossing a border changes the music (VISION_PLAN 5.6). Looked
  // up a few times a second; a change of style crossfades like a change of piece.
  musicStyleT_ -= dt;
  if (musicStyleT_ <= 0 && g.world.endless) {
    musicStyleT_ = 0.5f;
    const cult::Culture* C = nullptr;
    if (g.inside && g.subBldg >= 0 && g.subBldg < (int)g.world.over.bldgs.size()) C = g.world.cultureOf(g.world.over.bldgs[(size_t)g.subBldg].site);
    else if (!g.inside && g.curSite >= 0) C = g.world.cultureOf(g.curSite);
    if (!C && !g.inside) C = g.world.cultureAtTile((int)(g.pl().p.x / TILE), (int)(g.pl().p.y / TILE));
    musicStyle_ = C ? C->music.pack() : 0;
  }
  const uint64_t style = (want == Music::Town || want == Music::Wild || want == Music::Night || want == Music::Combat || want == Music::Tavern) ? musicStyle_ : 0;
  if (want != music_ || style != musicStyleOn_ || festive != musicFestive_) {
    music_ = want;
    musicStyleOn_ = style;
    musicFestive_ = festive;
    const MusicStyle ms = MusicStyle::unpack(style);
    audio_->setMusic(want, style ? &ms : nullptr, festive);
  }
  m5Stats_.music = (int)want;

  // (M3c LIFE) the land under the player (its eco), looked up a few times a second
  const bool outdoors = g.mode != Mode::Title && !g.inside;
  ecoT_ -= dt;
  if (ecoT_ <= 0) {
    ecoT_ = 0.25f;
    const int ptx = (int)std::floor(g.pl().p.x / 16), pty = (int)std::floor(g.pl().p.y / 16);
    const int e = outdoors && g.map().kind == MapKind::Overworld ? (int)g.world.over.ecoAt(ptx, pty) : -1;
    // (fixer M4 r1, review: "THE FOREST" over a walled capital's streets) within a settlement the land's name gives way
    // to the place's own (the arrival banner, the kingdom line): no biome banner starts there, and one showing fades
    const bool inTown = outdoors && g.map().kind == MapKind::Overworld && g.settlementAt(g.pl().p) >= 0;
    if (e != hereEco_ && e >= 0 && !inTown && g.mode == Mode::Play) {
      // the first visit to a biome this adventure: a quiet banner naming it (Game::marks keeps it with the save;
      // the key is game_internal.h markKey(0x7E000 + eco, Mk::Biome = 8))
      const uint64_t key = ew::mix64((0x7E000ull + (uint64_t)e) ^ (8ull * 0xD1B54A32D192ED03ull));
      // (M3c fixer, review: a stale or wrong banner) a banner still waiting behind a big one, or not yet shown, gives
      // way to the land the player now stands in (its biome stays unmarked, so it greets them next time)
      if (biomeBannerT_ >= 3.9f && biomeBannerEco_ >= 0 && biomeBannerEco_ != e) {
        g.marks.erase(ew::mix64((0x7E000ull + (uint64_t)biomeBannerEco_) ^ (8ull * 0xD1B54A32D192ED03ull)));
        biomeBannerT_ = 0;
        biomeBannerEco_ = -1;
      }
      if (!g.marks.count(key) && e != (int)Eco::Ocean) {
        g.marks[key] = g.day;
        biomeBanner_ = std::string("THE ") + ecoName((Eco)e);
        biomeBannerT_ = 4.0f;
        biomeBannerEco_ = e;
      }
    }
    if (e >= 0 && !inTown) hereEco_ = e;
    else if (!outdoors) hereEco_ = -1;
    // shown only while the player is still out in that land: indoors or elsewhere it fades out at once (one still
    // waiting is dropped and its biome unmarked)
    if (biomeBannerT_ > 0 && biomeBannerEco_ >= 0 && (!outdoors || inTown || (e >= 0 && e != biomeBannerEco_))) {
      if (biomeBannerT_ >= 3.9f) g.marks.erase(ew::mix64((0x7E000ull + (uint64_t)biomeBannerEco_) ^ (8ull * 0xD1B54A32D192ED03ull)));
      biomeBannerT_ = biomeBannerT_ >= 3.9f ? 0.0f : std::min(biomeBannerT_, 0.3f);
      if (biomeBannerT_ <= 0) biomeBannerEco_ = -1;
    }
  }
  if (biomeBannerT_ > 0 && ((bannerT_ <= 0 && herald_.t <= 0) || biomeBannerT_ < 3.9f) && g.mode == Mode::Play) biomeBannerT_ -= dt;   // (it waits for a big banner)

  // the weather (drawWeather paints wx_): what each nearby tile's Sky makes of this day and hour, averaged over five
  // points round the player (so it fades in over a dozen tiles at a border) and eased over a few seconds
  if (outdoors && g.map().kind == MapKind::Overworld) {
    wxSampleT_ -= dt;
    if (wxSampleT_ <= 0 || !wxInit_) {
      wxSampleT_ = 0.5f;
      const int slot = g.day * 8 + (int)(g.hour / 3);
      uint32_t w = hash32((uint32_t)slot ^ (uint32_t)g.seed) % 100, w2 = hash32((uint32_t)slot * 2654435761u ^ (uint32_t)g.seed ^ 0x5EEDu) % 100;
      if (skyOverride_ >= 0) w = w2 = 0;   // (a script's forced weather: at full strength)
      const float h = g.hour;
      const float hot = clampf(std::min(h - 9.5f, 17.5f - h) / 1.5f, 0, 1);    // midday heat for the shimmer
      const float morning = clampf(1.0f - std::fabs(h - 6.5f) / 3.5f, 0, 1);  // the fogs are thickest at dawn
      Wx want;
      float er = 0, eg = 0, eb = 0, en = 0;
      const int ptx = (int)std::floor(g.pl().p.x / 16), pty = (int)std::floor(g.pl().p.y / 16);
      static const int ox[5] = {0, -7, 7, 0, 0}, oy[5] = {0, 0, 0, -6, 6};
      for (int k = 0; k < 5; k++) {
        const Eco e = g.world.over.ecoAt(ptx + ox[k], pty + oy[k]);
        Wx t;
        switch (skyOverride_ >= 0 && skyOverride_ < (int)Sky::COUNT ? (Sky)skyOverride_ : ecoInfo(e).sky) {
          case Sky::Temperate: t.rain = w < 22 ? 1.0f : 0.0f; t.drizzle = w >= 22 && w < 30 ? 1.0f : 0.0f; t.fog = morning * (w2 < 30 ? 0.5f : 0.0f); break;
          case Sky::Showery: t.rain = w < 45 ? 1.0f : 0.0f; t.drizzle = w >= 45 && w < 70 ? 1.0f : 0.0f; t.fog = 0.25f + morning * 0.3f; break;
          case Sky::Misty: t.fog = 0.75f + morning * 0.25f; t.drizzle = w < 40 ? 1.0f : 0.0f; t.rain = w < 12 ? 0.6f : 0.0f; break;
          case Sky::Dry: t.rain = w < 5 ? 0.8f : 0.0f; t.shimmer = hot * 0.7f; t.sand = w2 < 10 ? 0.35f : 0.0f; break;
          case Sky::Arid: t.sand = w < 24 ? 1.0f : (w2 < 30 ? 0.3f : 0.0f); t.shimmer = hot; break;
          case Sky::Snowy: t.snow = w < 55 ? 1.0f : 0.25f; t.fog = w2 < 25 ? 0.3f : 0.0f; break;
          case Sky::Blizzard: t.blizz = w < 45 ? 1.0f : 0.0f; t.snow = w < 45 ? 0.6f : 0.8f; t.fog = 0.25f; break;
          case Sky::Monsoon: t.pour = w < 40 ? 1.0f : 0.0f; t.rain = w >= 40 && w < 60 ? 0.8f : 0.0f; t.drizzle = w >= 60 && w < 75 ? 1.0f : 0.0f; t.fog = 0.35f + morning * 0.3f; break;
          case Sky::Ashfall: t.ash = 0.6f + (w < 40 ? 0.4f : 0.0f); break;
          case Sky::Eerie: t.eerie = 0.7f + (w < 40 ? 0.3f : 0.0f); t.fog = e == Eco::Blight ? 0.35f : 0.15f; break;
          default: break;
        }
        if (t.eerie > 0) {
          float r = 0.75f, gg = 0.9f, b = 1.0f;
          if (e == Eco::MushroomForest) { r = 0.80f; gg = 0.55f; b = 1.0f; }
          else if (e == Eco::CrystalBarrens) { r = 0.60f; gg = 0.90f; b = 1.0f; }
          else if (e == Eco::Silverwood) { r = 0.85f; gg = 1.0f; b = 0.92f; }
          else if (e == Eco::Blight) { r = 0.62f; gg = 0.80f; b = 0.32f; }
          er += r; eg += gg; eb += b; en += 1;
        }
        const float f = k == 0 ? 0.4f : 0.15f;
        want.rain += t.rain * f; want.pour += t.pour * f; want.drizzle += t.drizzle * f; want.fog += t.fog * f; want.snow += t.snow * f;
        want.blizz += t.blizz * f; want.sand += t.sand * f; want.shimmer += t.shimmer * f; want.ash += t.ash * f; want.eerie += t.eerie * f;
      }
      if (en > 0) { want.er = er / en; want.eg = eg / en; want.eb = eb / en; } else { want.er = wx_.er; want.eg = wx_.eg; want.eb = wx_.eb; }
      wxWant_ = want;
      if (!wxInit_) { wx_ = want; wxInit_ = true; }
    }
    const float k = std::min(1.0f, dt * 0.35f);
    auto ease = [&](float& v, float t) { v += (t - v) * k; };
    ease(wx_.rain, wxWant_.rain); ease(wx_.pour, wxWant_.pour); ease(wx_.drizzle, wxWant_.drizzle); ease(wx_.fog, wxWant_.fog);
    ease(wx_.snow, wxWant_.snow); ease(wx_.blizz, wxWant_.blizz); ease(wx_.sand, wxWant_.sand); ease(wx_.shimmer, wxWant_.shimmer);
    ease(wx_.ash, wxWant_.ash); ease(wx_.eerie, wxWant_.eerie);
    ease(wx_.er, wxWant_.er); ease(wx_.eg, wxWant_.eg); ease(wx_.eb, wxWant_.eb);
  } else if (g.inside) wxInit_ = false;   // (back outside, the weather is simply there)

  // the land's sound bed and the wilderness music's mood (engine/audio.cpp): silent indoors, in caves and the menus,
  // softer in towns and dialogue, under a fight
  {
    const Eco he = hereEco_ >= 0 ? (Eco)hereEco_ : Eco::Meadow;
    float lvl = 1.0f;
    if (!outdoors || hereEco_ < 0) lvl = 0;
    switch (g.mode) {
      case Mode::Play: break;
      case Mode::Dialogue: lvl *= 0.35f; break;
      default: lvl = 0; break;
    }
    if (g.curSite >= 0 && g.curSite < (int)g.world.sites.size() && g.world.sites[(size_t)g.curSite].settlement()) lvl *= 0.4f;
    if (combatT_ > 0) lvl *= 0.6f;
    if (g.travel.phase != TravelPhase::None) lvl *= 0.5f;
    ambLevel_ = lvl;
    audio_->setAmbient((uint8_t)ecoInfo(he).amb, lvl);
    audio_->setMood(nearSiege(g) ? (uint8_t)Mood::Ominous : (uint8_t)ecoInfo(he).mood);   // (M4) the Siege mood
    audio_->setDaylight(g.mode == Mode::Title ? 1.0f : clampf(g.daylight(), 0, 1));
  }

  // ambient particles (world layer): the land's own drift
  if (outdoors && hereEco_ >= 0) {
    const Eco e = (Eco)hereEco_;
    const Biome b = ecoFamily(e);
    Rng r((uint32_t)(t_ * 997));
    auto spawn = [&](Vec2 p, Vec2 v, float life, Color c, float size, int layer) {
      Particle q; q.p = p; q.v = v; q.life = q.max = life; q.c = c; q.size = size; q.layer = layer; parts_.push_back(q);
    };
    auto anywhere = [&]() { return cam_ + Vec2(r.range(0, (float)Pix::W), r.range(0, (float)Pix::H)); };
    auto fromTop = [&]() { return cam_ + Vec2(r.range(-40, (float)Pix::W), -4); };
    // fireflies over the meadows, the reeds and the woods on a warm night
    const bool flies = (b == Biome::Plains && !ecoHas(e, EF_COLD) && e != Eco::Blight) || b == Biome::Forest || b == Biome::Autumn ||
                       (b == Biome::Swamp && e != Eco::PeatBog);
    const float flyRate = e == Eco::Jungle || e == Eco::FloodedForest || e == Eco::FlowerMeadow || e == Eco::LakeDistrict ? 10.0f : 6.0f;
    if (g.isNight() && flies && r.f() < dt * flyRate)
      spawn(anywhere(), Vec2(r.range(-6, 6), r.range(-6, 6)), r.range(2, 4), Color(0.8f, 1.0f, 0.4f), 1, 1);
    // falling leaves in the autumn woods (and now and then in a broadleaf wood)
    if ((b == Biome::Autumn && r.f() < dt * 5) || ((e == Eco::MixedForest || e == Eco::BirchWood) && r.f() < dt * 0.8f))
      spawn(fromTop(), Vec2(r.range(8, 20), r.range(14, 24)), 10,
            b == Biome::Autumn ? (r.f() < 0.5f ? Color(0.85f, 0.35f, 0.15f) : Color(0.95f, 0.7f, 0.25f)) : Color(0.75f, 0.78f, 0.3f), 2, 2);
    // petals drifting down in the blossom groves
    if (e == Eco::BlossomGrove && r.f() < dt * 7)
      spawn(fromTop(), Vec2(r.range(10, 26), r.range(10, 18)), 12, r.f() < 0.6f ? Color(1.0f, 0.78f, 0.86f) : Color(0.98f, 0.92f, 0.95f), r.f() < 0.3f ? 2.f : 1.f, 2);
    // embers rising off the ash fields
    if (e == Eco::AshFields && r.f() < dt * 9)
      spawn(cam_ + Vec2(r.range(0, (float)Pix::W), r.range(Pix::H * 0.3f, (float)Pix::H + 10)), Vec2(r.range(-8, 8), r.range(-26, -12)), r.range(1.5f, 3.0f),
            r.f() < 0.5f ? Color(1.0f, 0.55f, 0.15f) : Color(1.0f, 0.82f, 0.35f), 1, 1);
    // spores in the mushroom wood, glints on the crystal barrens, silver motes in the silverwood, flies over the blight
    if (e == Eco::MushroomForest && r.f() < dt * 6)
      spawn(anywhere(), Vec2(r.range(-4, 4), r.range(-8, -2)), r.range(3, 5), Color(0.85f, 0.6f, 1.0f), 1, 1);
    if (e == Eco::CrystalBarrens && r.f() < dt * 5)
      spawn(anywhere(), Vec2(0, 0), r.range(0.4f, 0.9f), Color(0.8f, 0.95f, 1.0f), 1, 1);
    if (e == Eco::Silverwood && r.f() < dt * 5)
      spawn(anywhere(), Vec2(r.range(-3, 3), r.range(-5, -1)), r.range(3, 5), Color(0.9f, 1.0f, 0.95f), 1, 1);
    if (e == Eco::Blight && r.f() < dt * 6)
      spawn(anywhere(), Vec2(r.range(-30, 30), r.range(-20, 20)), r.range(0.6f, 1.2f), Color(0.12f, 0.1f, 0.1f), 1, 0);
    // pollen over a summer meadow by day
    if (!g.isNight() && (e == Eco::FlowerMeadow || e == Eco::Meadow || e == Eco::AlpineMeadow) && r.f() < dt * 2)
      spawn(anywhere(), Vec2(r.range(4, 10), r.range(-3, 3)), r.range(3, 5), Color(1.0f, 0.95f, 0.7f), 1, 1);
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
  int kind;     // 0 prop, 1 building, 2 wall, 3 actor, 4 pickup, 5 projectile, 6 gate, 7 (M5) festival pole
  int idx;
  int tx, ty;
};
}  // namespace

uint64_t View::mapIdFor(const Game& g, int key) const {
  uint64_t mapId = hash32((uint32_t)g.world.seed ^ (uint32_t)(g.world.seed >> 32)) * 2654435761ull + (uint64_t)(key + 7);
  mapId ^= (uint64_t)g.world.genVersion << 56 | (uint64_t)(g.world.endless ? 1 : 0) << 55;
  return mapId;
}

uint64_t View::terrainFrame(Game& g, const Map*& mp) {
  const Map& m = g.mode == Mode::Title ? g.world.over : g.map();
  mp = &m;
  int key = g.mode == Mode::Title ? 0 : g.mapKey();
  if (key != lastMapKey_) { lastMapKey_ = key; }
  const uint64_t mapId = mapIdFor(g, key);
  // M1: an endless overworld's terrain chunks are keyed by GLOBAL chunk (the window origin in chunks plus the local
  // chunk), so a window shift keeps every chunk already baked (terrain.cpp)
  const bool endlessOver = key == 0 && g.world.endless && m.kind == MapKind::Overworld;
  chunkEndless_ = endlessOver;
  chunkOX_ = endlessOver ? (int)std::floor(g.world.ox / 32.0) : 0;
  chunkOY_ = endlessOver ? (int)std::floor(g.world.oy / 32.0) : 0;
  return mapId;
}

bool boardwalkCoversTile(const Map& m, int tx, int ty);   // terrain.cpp (M4)

// (fixer M4 r2) when this frame's world draw began: the ahead-of-time building painter takes only what is left of a
// frame's budget (a fresh capital's walk drew 20 ms frames on the web path: 33 people plus a 3 ms paint step each)
static std::chrono::steady_clock::time_point g_drawT0;

void View::drawWorld(Game& g) {
  Pix& P = *pix_;
  g_drawT0 = std::chrono::steady_clock::now();
  {   // (M4) last frame's overlay counts for the scripts (the map's are kept from its own draw)
    const int mk = m4Stats_.mapMarkers, bp = m4Stats_.borderPx;
    m4Stats_ = m4Count_;
    m4Stats_.mapMarkers = mk; m4Stats_.borderPx = bp;
    m4Count_ = M4Stats();
  }
  {   // (M5) the same for the life overlays (the sound's numbers are kept: update() sets them)
    const int mu = m5Stats_.music;
    const float tl = m5Stats_.tavernLevel, cr = m5Stats_.crowd;
    m5Stats_ = m5Count_;
    m5Stats_.music = mu; m5Stats_.tavernLevel = tl; m5Stats_.crowd = cr;
    m5Count_ = M5Stats();
  }
  fadePaintMs_ = arriving_ || g.travelling() ? 10.0 : 0.0;   // (M2: an arrival paints its buildings in travelArrive, within its budget)
  const Map* mp = nullptr;
  const uint64_t mapId = terrainFrame(g, mp);
  const Map& m = *mp;
  const bool endlessOver = chunkEndless_;
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
  if (!arriving_) bakeVisibleNow(m, mapId, c0x, c0y, c1x, c1y);
  for (int cy = c0y; cy <= c1y; cy++)
    for (int cx = c0x; cx <= c1x; cx++) {
      if (cx < 0 || cy < 0 || cx * 32 >= m.w || cy * 32 >= m.h) continue;
      Tex t = chunkTex(m, mapId, cx, cy);
      P.blit(t, cx * 512 - cam.x, cy * 512 - cam.y);
    }
  prefetch(m, mapId, cam);
  if (!arriving_ && !g.inside && m.kind == MapKind::Overworld) prepareInterior(g);   // (M3c) the door ahead's interior
  // (an arrival pumps its own budget: travelArrive; M2 fixer round 3: under an open menu, whose map paints its own
  // tiles on the same frames, the world's bake waits its turn with a small share)
  if (!arriving_) pumpBake(g.mode == Mode::Menu ? 1.5 : 5.0);
  // wall layout (towers, joins, gate flanks): once per map
  bldgMap_ = &m;
  // the id also carries the generator version and gate count: a new game on the same seed with another generator
  // has the same map id but other walls
  // (M1: and the endless window's origin, as the layout is in window tiles)
  uint64_t wallId = mapId ^ ((uint64_t)g.world.genVersion << 58) ^ ((uint64_t)g.world.gates.size() << 48) ^ ((uint64_t)g.world.wallGaps.size() << 36);
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
      if (it != wallKeyCache_.end() && it->second.size() == (size_t)m.w * m.h) {
        wallKeys_ = it->second;
        auto fl = fortLookCache_.find(wallId);
        fortLooks_ = fl != fortLookCache_.end() ? fl->second : std::vector<bld::FortParts>(1);
      } else {
        fortLooks_.assign(1, bld::FortParts{});
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
          // M3: each settlement's wall style (Map::wall = 1 + art::CityWall)
          for (size_t i = 0; i < wallKeys_.size(); i++)
            if (wallKeys_[i] && m.wall[i] > 1) wallKeys_[i] |= ((uint32_t)(m.wall[i] - 1) << art::WALL_STYLE_SHIFT) & art::WALL_STYLE_MASK;
          // (M3 fixer) an opening without a gatehouse (a side gate, a gate in a north-south run) is spanned: the wall walk
          // runs on over it on a vault (art::WALL_BIT_SPAN), so no road runs through a bare cut between two tower stubs.
          // The span's tiles stay walkable (Map::wall is untouched); only their sprites and their neighbours' joins change.
          {
            auto wallIdx = [&](int x, int y) { return (size_t)y * m.w + x; };
            auto onSpan = [&](const IRect& r, int x, int y) { return x >= r.x && y >= r.y && x < r.x + r.w && y < r.y + r.h; };
            std::vector<IRect> spans;
            for (const IRect& r : g.world.wallGaps) {
              if (!((r.w == 1 && r.h == 3) || (r.w == 3 && r.h == 1))) continue;
              bool gate = false;
              for (const auto& gt : g.world.gates) if (r.h == 1 && gt.first == r.x && gt.second == r.y) gate = true;
              if (gate) continue;
              const int ax = r.w == 1 ? r.x : r.x - 1, ay = r.w == 1 ? r.y - 1 : r.y;
              const int bx = r.w == 1 ? r.x : r.x + 3, by = r.w == 1 ? r.y + 3 : r.y;
              if (!m.in(ax, ay) || !m.in(bx, by) || !m.wall[wallIdx(ax, ay)] || !m.wall[wallIdx(bx, by)]) continue;
              bool ok = true;
              for (int y = r.y; y < r.y + r.h; y++)
                for (int x = r.x; x < r.x + r.w; x++)
                  if (!m.in(x, y) || m.wall[wallIdx(x, y)] || groundWater(m.at(x, y))) ok = false;
              if (ok) spans.push_back(r);
            }
            auto spanAt = [&](int x, int y) {
              for (const IRect& r : spans) if (onSpan(r, x, y)) return true;
              return false;
            };
            auto aug = [&](int x, int y) { return m.in(x, y) && (m.wall[wallIdx(x, y)] != 0 || spanAt(x, y)); };
            static const int ndx[8] = {0, 1, 1, 1, 0, -1, -1, -1}, ndy[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
            for (const IRect& r : spans) {
              const int ax = r.w == 1 ? r.x : r.x - 1, ay = r.w == 1 ? r.y - 1 : r.y;
              const uint8_t sb = m.wall[wallIdx(ax, ay)];
              for (int y = r.y - 1; y <= r.y + r.h; y++)
                for (int x = r.x - 1; x <= r.x + r.w; x++) {
                  if (!aug(x, y)) continue;
                  uint32_t nb = 0;
                  for (int b = 0; b < 8; b++) if (aug(x + ndx[b], y + ndy[b])) nb |= 1u << b;
                  uint32_t& k = wallKeys_[wallIdx(x, y)];
                  if (onSpan(r, x, y)) {
                    const int pos = 1 + (r.w == 1 ? y - r.y : x - r.x);
                    k = nb | art::WALL_BIT_SPAN | ((uint32_t)pos << art::WALL_SPAN_POS_SHIFT) | ((uint32_t)((x * 7 + y * 13) & 3) << art::WALL_VAR_SHIFT);
                    if (sb > 1) k |= ((uint32_t)(sb - 1) << art::WALL_STYLE_SHIFT) & art::WALL_STYLE_MASK;
                    if (y > 0 && (wallKeys_[wallIdx(x, y - 1)] & art::WALL_BIT_TOWER)) k |= art::WALL_BIT_TOWER_N;
                  } else if (k) k = (k & ~0xFFu) | nb;
                }
            }
          }
          // (M3b forts) each settlement's walls in its culture's parts: a look slot per settlement in the key's bits
          // 29..31 (art::WALL_LOOK_*), up to seven settlements per window (more: the wall style's defaults)
          if (g.world.endless) {
            std::unordered_map<uint64_t, int> blockSlot;   // 8 x 8-tile block -> slot (-1: none)
            std::unordered_map<int, int> siteSlot;
            for (int y = 0; y < m.h; y++)
              for (int x = 0; x < m.w; x++) {
                uint32_t& k = wallKeys_[(size_t)y * m.w + x];
                if (!k) continue;
                const uint64_t bk = (uint64_t)(uint32_t)(x >> 3) << 32 | (uint32_t)(y >> 3);
                auto bi = blockSlot.find(bk);
                int slot = -1;
                if (bi != blockSlot.end()) slot = bi->second;
                else {
                  int site = -1;
                  for (int i = 0; i < (int)g.world.sites.size() && site < 0; i++) {
                    const Site& S = g.world.sites[(size_t)i];
                    if (!S.settlement()) continue;
                    const IRect& r = S.r;
                    if (x >= r.x - 10 && y >= r.y - 10 && x < r.x + r.w + 10 && y < r.y + r.h + 10) site = i;
                  }
                  if (site >= 0) {
                    auto si = siteSlot.find(site);
                    if (si != siteSlot.end()) slot = si->second;
                    else {
                      bld::FortParts fp;
                      if (fortLooks_.size() < 8 && fortPartsOfSite(g, site, fp)) { slot = (int)fortLooks_.size(); fortLooks_.push_back(fp); }
                      siteSlot[site] = slot;
                    }
                  }
                  blockSlot[bk] = slot;
                }
                if (slot > 0) k = (k & ~art::WALL_LOOK_MASK) | ((uint32_t)slot << art::WALL_LOOK_SHIFT);
              }
          }
        } else art::wallKeys(m.wall.data(), m.w, m.h, nullptr, 0, wallKeys_);
        if (wallKeyCache_.size() > 4) { wallKeyCache_.clear(); fortLookCache_.clear(); }
        wallKeyCache_[wallId] = wallKeys_;
        fortLookCache_[wallId] = fortLooks_;
      }
    }
    wallTodo_.clear();
    // (M3b) the texture caches stay bounded: a long journey through many cultures repaints rather than hoards
    if (wallTiles_.size() > 1500) wallTiles_.clear();
    if (styledProps_.size() > 900) styledProps_.clear();
    std::unordered_map<uint32_t, bool> queued;
    for (uint32_t k : wallKeys_)
      if (k && !wallTiles_.count(wallTexKey(k)) && !queued.count(k)) { queued[k] = true; wallTodo_.push_back(k); }
  }
  // wall tiles not seen yet are painted ahead, one per frame (the ones on screen are painted on first use anyway)
  while (!wallTodo_.empty()) {
    uint32_t k = wallTodo_.back();
    wallTodo_.pop_back();
    if (!wallTiles_.count(wallTexKey(k))) { wallTileTex(k); break; }
  }
  // collect drawables
  std::vector<Drawable> list;
  // (M2: the wonders are up to 7 tiles wide and 7 tall above their anchor tile: a wider margin below and aside)
  int tx0 = (int)std::floor(cam.x / 16) - 5, ty0 = (int)std::floor(cam.y / 16) - 2;
  int tx1 = tx0 + Pix::W / 16 + 10, ty1 = ty0 + Pix::H / 16 + 10;
  drawDeco(m, cam, std::max(0, tx0), std::max(0, ty0), std::min(m.w, tx1), std::min(m.h, ty1));
  const Actor& pl = g.pl();
  for (int ty = std::max(0, ty0); ty < std::min(m.h, ty1); ty++)
    for (int tx = std::max(0, tx0); tx < std::min(m.w, tx1); tx++) {
      int pr = m.prop[(size_t)ty * m.w + tx];
      // (M4) a walk-through cover (flowers, grass) under a diagonal boardwalk's deck is not drawn over the planks
      if (pr && m.kind == MapKind::Overworld && !propSolid((Prop)(pr - 1)) && boardwalkCoversTile(m, tx, ty)) pr = 0;
      if (pr) {
        Prop p = (Prop)(pr - 1);
        if (flatProp(p)) {
          if (uint32_t ik = interiorPropTexKey(g, m, tx, ty, p, (int)(t_ * 8 + tx * 3))) {
            const Tex& t = cachedTex(0x01ull << 56 | ik, paintInteriorPiece);
            P.blit(t, tx * 16 + 8 - t.w / 2 - cam.x, ty * 16 + 16 - t.h - cam.y);
          } else {
            const Tex* tp = &props_[(int)p];
            int fw = art::propW(p);
            int frames = std::max(1, art::propFrames(p));
            int fr = frames > 1 ? (int)(t_ * 8 + tx * 3) % frames : 0;
            // (M3c fixer) the ground covers of the wild sat at one offset in every tile, so a meadow read as planted rows:
            // each patch takes a sub-tile offset by its global tile (up to +-5 px across, +-4 down) and the Wildlands
            // covers their biome's variant
            float jx = 0, jy = 0;
            if (m.kind == MapKind::Overworld && natureProp(p)) {
              const uint32_t h = hash2(tx + g.world.ox, ty + g.world.oy, 6287);
              jx = (float)((int)(h % 11u) - 5);
              jy = (float)((int)((h >> 8) % 9u) - 4);
              if (art::isWildlandsFlora(p)) {
                const int nv = std::max(1, art::floraVariants(p));
                tp = &cachedTex(0x5Eull << 56 | (uint64_t)p << 16 | (uint64_t)m.ecoAt(tx, ty) << 8 | (uint64_t)((h >> 16) % (uint32_t)nv), paintFlora);
                fw = tp->w / frames;
              }
            }
            P.blitRegion(*tp, fr * fw, 0, fw, tp->h, tx * 16 + 8 - fw / 2 + jx - cam.x, ty * 16 + 16 - tp->h + jy - cam.y);
          }
        } else {
          // (stalls fixer round 3) a stall sorts just ahead of its keeper (art::stallSortY), whichever way it faces
          const float sy = art::isStall(p) && m.kind == MapKind::Overworld ? art::stallSortY(stallFacingOn(g, m, tx, ty)) : 15.0f;
          list.push_back({ty * 16.0f + sy, 0, pr - 1, tx, ty});
        }
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
  // (M3, owner carry-over 1) in steps of about 3 ms: a palace is painted over a dozen frames instead of stalling one
  const double drawnMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - g_drawT0).count();
  // (a heavy frame skips it, the next one paints; never more than two frames in a row, then a small step anyway, so the
  // streets ahead are still painted before they come into view)
  static int paintSkipped = 0;
  double paintBudget = std::min(3.0, 10.0 - drawnMs);
  if (paintBudget < 0.75 && ++paintSkipped > 2) paintBudget = 1.0;
  if (paintBudget >= 0.75) paintSkipped = 0;
  if (!m.bldgs.empty() && g.mode != Mode::Title && !g.travelling() && paintBudget >= 0.75) {   // (M2: a journey paints its arrival in travelArrive)
    bldgPrefetch_ %= (int)m.bldgs.size();
    const Bldg& cur = m.bldgs[(size_t)bldgPrefetch_];
    const uint64_t ck = bldgKey(m, cur, bldgPrefetch_);
    bool busy = false;
    if (!bldgTex_.count(ck))
      for (const BldgJob& j : bldgJobs_) if (j.key == ck) { busy = true; break; }
    if (busy) bldgPaintStep(cur, bldgPrefetch_, paintBudget);
    else {
      // (M3 fixer round 2) the nearest unpainted building to where the hero is heading, not the next index round the
      // list: in a capital of 200+ buildings the round-robin looked at 12 a frame and the walk reached unpainted streets
      // first (a 120-140 ms frame of on-demand paints on the first walk after arriving)
      const Vec2 ahead = pl.p + pl.vel * 0.6f;
      int best = -1;
      float bestD = 1e30f;
      for (int i = 0; i < (int)m.bldgs.size(); i++) {
        const Bldg& b = m.bldgs[(size_t)i];
        const float dx = b.r.cx() * 16.0f - ahead.x, dy = b.r.cy() * 16.0f - ahead.y;
        if (std::fabs(dx) > 720 || std::fabs(dy) > 520) continue;
        const float d = dx * dx + dy * dy * 1.6f;
        if (d >= bestD || bldgTex_.count(bldgKey(m, b, i))) continue;
        best = i; bestD = d;
      }
      if (best >= 0) { bldgPrefetch_ = best; bldgPaintStep(m.bldgs[(size_t)best], best, paintBudget); }
    }
  }
  for (int i = 0; i < (int)g.actors.size(); i++) {
    const Actor& a = g.actors[i];
    if (a.p.x < cam.x - 80 || a.p.x > cam.x + Pix::W + 80 || a.p.y < cam.y - 40 || a.p.y > cam.y + Pix::H + 80) continue;
    float sy = a.p.y + (a.fly ? 60.0f : 0.0f) - (a.st == AState::Dead ? 8.0f : 0.0f);
    // (M5) a body on furniture sorts by the furniture's tile (art::PostureInfo): a sleeper after its bed (the blanket over
    // it); a sitter facing down after its chair (in front of the chair back), facing up before it (the chair back hides
    // the small of the back; the table it faces, a row up, is drawn before it either way); side-on after it
    if (a.human && a.posture != art::Posture::None && a.useX >= 0 && a.useY >= 0 && a.st != AState::Dead && m.in(a.useX, a.useY)) {
      const art::PostureInfo pi = art::postureInfo(a.posture);
      if (pi.lying) sy = a.useY * 16.0f + 15.7f;
      else if (pi.seated) {   // (ART seatFit: front = after the seat)
        const int sp = m.propAt(a.useX, a.useY);
        const art::SeatFit f = art::seatFit(sp ? (art::Prop)(sp - 1) : art::Prop::COUNT, -1, a.face);
        sy = a.useY * 16.0f + (!f.ok || f.front ? 15.6f : 14.4f);
      }
    }
    list.push_back({sy, 3, i, 0, 0});
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
  // (M5) a festival's poles across the plaza (kind 7: idx = span, tx = 0 west / 1 east pole)
  festivalPlan(g, m, cam);
  for (int i = 0; i < (int)festSpans_.size(); i++) {
    const FestSpan& s = festSpans_[(size_t)i];
    if (s.y * 16 < cam.y - 40 || s.y * 16 > cam.y + Pix::H + 60 || s.x1 * 16 < cam.x - 40 || s.x0 * 16 > cam.x + Pix::W + 40) continue;
    list.push_back({s.y * 16.0f + 13.0f, 7, i, 0, 0});
    list.push_back({s.y * 16.0f + 13.0f, 7, i, 1, 0});
  }
  std::sort(list.begin(), list.end(), [](const Drawable& a, const Drawable& b) { return a.y < b.y; });

  // shadows first (under everything standing)
  for (const Drawable& d : list) {
    if (d.kind == 3) {
      const Actor& a = g.actors[d.idx];
      if (a.st == AState::Dead) continue;
      if (a.critter > 0) {   // (M5) a village animal: a shadow as long as its body (wider side-on)
        const int ck = a.critter - 1;
        const float w = art::critterCellW((art::Critter)ck) * (a.face >= 2 ? 0.8f : 0.55f), h = std::max(3.0f, w * 0.32f);
        P.blitEx(shadow_, 0, 0, shadow_.w, shadow_.h, a.p.x - w / 2 - cam.x, a.p.y - h / 2 - cam.y, w, h, false, Color(1, 1, 1, 0.85f));
        continue;
      }
      if (bathing(g, m, a)) continue;   // (M5 fixer r2) in the water: no shadow on the floor
      if (a.human && a.posture != art::Posture::None) {   // (M5) on a chair or in bed the furniture throws the shadow
        const art::PostureInfo pi = art::postureInfo(a.posture);
        if (pi.lying || (pi.seated && a.useY >= 0)) continue;
      }
      bool big = a.radius > 7;
      const Tex& s = big ? shadowBig_ : shadow_;
      float sc = big ? std::min(1.6f, a.radius / 10.0f) : 1.0f;
      if (a.mon == Monster::Dragon) sc = a.fly ? 1.8f : 2.0f;
      P.blitEx(s, 0, 0, s.w, s.h, a.p.x - s.w * sc / 2 - cam.x, a.p.y - s.h * sc / 2 - cam.y, s.w * sc, s.h * sc, false, Color(1, 1, 1, a.fly ? 0.6f : 1));
    } else if (d.kind == 0) {
      Prop p = (Prop)d.idx;
      // (stall facings) a stall, table, cloth or cart throws its own model's shadow (soft, to the lower right)
      if (m.kind == MapKind::Overworld && (art::isVendorProp(p) || p == Prop::Cart)) {
        const Tex* st;
        if (art::isStall(p)) {
          const int fc = stallFacingOn(g, m, d.tx, d.ty);
          st = &cachedTex(0x3Aull << 56 | (uint64_t)fc << 4 | (uint64_t)stallFormOn(g, d.tx, d.ty, fc), paintStallShadow);
        } else if (p == Prop::MarketTable) {   // (stalls fixer round 1) a packed-up table's shade is furled: no shade's shadow
          const bool open = g.mode == Mode::Title || ew::stallOpen(d.tx + g.world.ox, d.ty + g.world.oy, g.hour);
          st = &cachedTex(0x3Bull << 56 | (uint64_t)(open ? ew::tableShadeAt(d.tx + g.world.ox, d.ty + g.world.oy) : 0), paintTableShadow);
        }
        else if (p == Prop::GroundCloth) st = &cachedTex(0x3Cull << 56, paintClothShadow);
        else st = &cachedTex(0x3Eull << 56 | (uint64_t)cartLook(g, d.tx, d.ty), paintCartShadow);
        P.blit(*st, d.tx * 16 + art::kVendorShadowX - cam.x, d.ty * 16 + art::kVendorShadowY - cam.y);
        continue;
      }
      int sh = propShadow(p);
      if (sh == 1) P.blitEx(shadowBig_, 0, 0, 40, 12, d.tx * 16 + 8 - 12 - cam.x, d.ty * 16 + 11 - cam.y, 24, 8, false, Color(1, 1, 1, 0.8f));
      else if (sh == 2) P.blitEx(shadowBig_, 0, 0, 40, 12, d.tx * 16 + 8 - 11 - cam.x, d.ty * 16 + 12 - cam.y, 22, 7, false, Color(1, 1, 1, 0.7f));
      else if (sh == 3) {   // (M1 economy) the stall's awning shades the ground a little east of it (the sun is up-left)
        P.blitEx(shadowBig_, 0, 0, 40, 12, d.tx * 16 + 8 - 22 - cam.x, d.ty * 16 + 9 - cam.y, 50, 11, false, Color(1, 1, 1, 0.75f));
      } else if (sh == 4) {   // (M1 fixer round 2) a two-tile table or cloth: its shade under both its tiles
        P.blitEx(shadowBig_, 0, 0, 40, 12, d.tx * 16 + 1 - cam.x, d.ty * 16 + 10 - cam.y, 32, 8, false, Color(1, 1, 1, 0.6f));
      } else if (sh == 5) {   // a beast's own small shadow
        P.blitEx(shadowBig_, 0, 0, 40, 12, d.tx * 16 + 8 - 9 - cam.x, d.ty * 16 + 12 - cam.y, 18, 5, false, Color(1, 1, 1, 0.6f));
      } else if (sh >= 6) {   // (M2) a wild prop's ground shadow, as wide as its footprint, thrown a little down-right
        int fw = 1, fh = 1;
        art::wildFootprint(p, fw, fh);
        const float w = fw * 16.0f * (sh == 7 ? 1.05f : 0.82f) + 6, h = std::max(6.0f, w * 0.22f);
        P.blitEx(shadowBig_, 0, 0, 40, 12, d.tx * 16 + 8 - w / 2 + 3 - cam.x, d.ty * 16 + 15 - h * 0.6f - cam.y, w, h, false, Color(1, 1, 1, 0.75f));
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
          if (uint32_t ik = interiorPropTexKey(g, m, d.tx, d.ty, p, (int)(t_ * 8 + d.tx * 3))) {
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
          const art::PropStyle* ps = g.mode == Mode::Title ? nullptr : propStyleAt(g, d.tx, d.ty);
          tp = ps && !ps->classic() ? &styledPropTex(p, *ps) : &cachedTex(0x02ull << 56 | (uint64_t)(h % 36), paintStall);   // (M3b) the culture's
        }
        // (M1 economy) a trade's stall: the awning cloth steps along a row (a stall three tiles on wears the next
        // cloth, so neighbours never match), and differs between rows and squares
        // (M1 fixer round 2) each row of a market keeps one stall form (cloth booths, canvas tents or shingled timber
        // booths; by the row, so a row reads as one covered run and the next row may differ); after its closing hour
        // a stall is packed up (its stock under a cover, a curtain or shutter down): the hour its keeper leaves
        bool vendorOpen = g.mode == Mode::Title || m.kind != MapKind::Overworld || ew::stallOpen(d.tx + g.world.ox, d.ty + g.world.oy, g.hour);
        // (M5, 15.12) a hungry or famished town's stalls stay shuttered all day (nothing to sell): packed up like after hours
        if (vendorOpen && g.mode != Mode::Title && m.kind == MapKind::Overworld && (art::isVendorProp(p) || p == Prop::MarketStall) &&
            (lifeFlagsAt(g, d.tx, d.ty) & (life::MF_SHUTTERED | life::MF_FAMINE))) {
          vendorOpen = false;
          m5Count_.stallsShut++;
        }
        // (stall facings) a row running north-south (side profiles) steps its cloth along the column instead
        int stallFacing = art::StallS;
        if (art::isStall(p) && m.kind == MapKind::Overworld) {
          stallFacing = stallFacingOn(g, m, d.tx, d.ty);
          const bool ns = stallFacing >= art::StallE;
          const int32_t gx = d.tx + g.world.ox, gy = d.ty + g.world.oy;
          const int32_t along = ns ? gy : gx, across = ns ? gx : gy;
          const uint32_t row = hash2(ns ? 1 : 0, across, 6163) % (uint32_t)art::kStallAwnings;
          const int64_t col3 = along >= 0 ? along / 3 : -((-(int64_t)along + 2) / 3);   // floor(along / 3)
          const uint32_t aw = (uint32_t)(((col3 * 5 + (int64_t)row) % art::kStallAwnings + art::kStallAwnings) % art::kStallAwnings);
          const uint32_t form = (uint32_t)stallFormOn(g, d.tx, d.ty, stallFacing);
          const bool snowy = g.mode != Mode::Title && snowyTile(m, d.tx, d.ty);
          const uint64_t sk = 0x04ull << 56 | (uint64_t)(snowy ? 1 : 0) << 16 | (uint64_t)stallFacing << 13 | (uint64_t)(vendorOpen ? 0 : 1) << 12 |
                              (uint64_t)form << 8 | (uint64_t)aw << 4 | (uint64_t)art::stallTrade(p);
          const art::PropStyle* ps = g.mode == Mode::Title ? nullptr : propStyleAt(g, d.tx, d.ty);
          if (ps && !ps->classic() && (ps->awning || ps->awningA || ps->cloth)) {   // (M3) the culture's awnings, the layout untouched
            const uint64_t k2 = ew::mix64(sk ^ ps->key() ^ 0x57A11ull);
            auto it = styledProps_.find(k2);
            if (it != styledProps_.end()) tp = &it->second;
            else {
              if (snowy) art::setMarketSnow(13.0f);
              tp = &(styledProps_[k2] = pix_->bake(art::marketStallStyled(art::stallTrade(p), (int)aw, (int)form, !vendorOpen, stallFacing, *ps)));
              art::setMarketSnow(1e9f);
            }
          } else
            tp = &cachedTex(sk, paintTradeStall);
        }
        if ((p == Prop::MarketTable || p == Prop::GroundCloth) && m.kind == MapKind::Overworld) {
          const int32_t gx = d.tx + g.world.ox, gy = d.ty + g.world.oy;
          const uint64_t k = (uint64_t)(vendorOpen ? 0 : 1) << 8;
          const art::PropStyle* ps = g.mode == Mode::Title ? nullptr : propStyleAt(g, d.tx, d.ty);
          if (ps && !ps->classic()) {   // (M3b) the culture's cloths and timber, the layout and goods untouched
            const bool table = p == Prop::MarketTable;
            const int a1 = table ? ew::tableGoodsAt(gx, gy) : ew::clothGoodsAt(gx, gy), a2 = table ? ew::tableShadeAt(gx, gy) : ew::clothColourAt(gx, gy);
            const uint64_t k2 = ew::mix64(ps->key() ^ ((uint64_t)(table ? 1 : 2) << 56) ^ k ^ ((uint64_t)a2 << 4) ^ (uint64_t)a1 ^ 0x7AB1Eull);
            auto it = styledProps_.find(k2);
            tp = it != styledProps_.end() ? &it->second
                                          : &(styledProps_[k2] = pix_->bake(table ? art::marketTableStyled(a1, a2, !vendorOpen, *ps) : art::groundClothStyled(a1, a2, !vendorOpen, *ps)));
          } else if (p == Prop::MarketTable) tp = &cachedTex(0x05ull << 56 | k | (uint64_t)ew::tableShadeAt(gx, gy) << 4 | (uint64_t)ew::tableGoodsAt(gx, gy), paintMarketTable);
          else tp = &cachedTex(0x06ull << 56 | k | (uint64_t)ew::clothColourAt(gx, gy) << 4 | (uint64_t)ew::clothGoodsAt(gx, gy), paintGroundCloth);
        }
        // (M1 fixer round 2) the mine hill: its shape by its tile, its top by its land (snow, dry grass or green)
        if (p == Prop::MineHill && m.kind == MapKind::Overworld) {
          const Biome bb = m.biomeAt(d.tx, d.ty);
          const int land = bb == Biome::Snow || bb == Biome::Taiga || bb == Biome::Mountain ? 1 : (bb == Biome::Desert ? 2 : 0);
          const uint32_t mv = hash2(d.tx + g.world.ox, d.ty + g.world.oy, 6211) & 3u;
          const art::PropStyle* ps = g.mode == Mode::Title ? nullptr : propStyleAt(g, d.tx, d.ty);
          if (ps && !ps->classic()) {   // (M3b) its adit timbered in the culture's wood
            const uint64_t k2 = ew::mix64(ps->key() ^ (0x08ull << 56) ^ ((uint64_t)land << 2) ^ mv ^ 0x3111ull);
            auto it = styledProps_.find(k2);
            tp = it != styledProps_.end() ? &it->second : &(styledProps_[k2] = pix_->bake(art::mineHillStyled((int)mv, land, *ps)));
          } else tp = &cachedTex(0x08ull << 56 | (uint64_t)land << 2 | (uint64_t)mv, paintMineHill);
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
        if (p == Prop::Banner && m.kind == MapKind::Overworld && g.mode != Mode::Title) {
          const int bs = g.world.siteAt(d.tx, d.ty, 2);
          if (const Kingdom* k = g.world.kingdomOf(bs)) {
            // (M3) the kingdom's full arms where its culture gives them, else its two colours and emblem
            const cult::Culture* kc = g.world.cultureOfKingdom(g.world.sites[(size_t)bs].kingdom);
            tp = (kc && !kc->heraldry.empty()) ? &heraldryTex(0, kc->heraldry, 0, 0) : &kingdomTex(0, *k, 0);
          } else {   // (M3b) no kingdom: the culture's own banner
            const art::PropStyle* ps = propStyleAt(g, d.tx, d.ty);
            if (!ps->classic()) tp = &styledPropTex(p, *ps);
          }
        }
        // (M3) the settlement's culture styles its fences, wells, lamps, benches, statues, shrines and fire bowls
        if (m.kind == MapKind::Overworld && g.mode != Mode::Title && p != Prop::Banner && !art::isStall(p) && p != Prop::MarketStall &&
            p != Prop::MarketTable && p != Prop::GroundCloth && p != Prop::MineHill && p != Prop::StandingStone && art::propStyled(p)) {
          // (M3b) the wayside's built props (signposts, toll posts, graves) in the land's culture, the rest in their settlement's
          const bool wayside = p == Prop::Signpost || p == Prop::TollPost || p == Prop::GraveCairn || p == Prop::Gravestone || p == Prop::Shrine;
          const art::PropStyle* ps = wayside ? landStyleAt(g, d.tx, d.ty) : propStyleAt(g, d.tx, d.ty);
          if (!ps->classic()) tp = &styledPropTex(p, *ps);
          // (M3 fixer round 2) a dry-stone dyke joins its neighbours: one wall of one height, closed L / T corners
          // (M3b round 3) and so does a clipped hedge (a broken corner and a flat summer-green strip before), snow on
          // its top in the cold where the trees carry snow
          const bool hedge = !ps->classic() && ps->fence == art::Fence::Hedge;
          if (!ps->classic() && (ps->fence == art::Fence::StoneDyke || hedge) && (p == Prop::FenceH || p == Prop::FenceV)) {
            auto fz = [&](int x, int y) { const int q = m.propAt(x, y); return q == (int)Prop::FenceH + 1 || q == (int)Prop::FenceV + 1; };
            int mask = (fz(d.tx, d.ty - 1) ? 1 : 0) | (fz(d.tx + 1, d.ty) ? 2 : 0) | (fz(d.tx, d.ty + 1) ? 4 : 0) | (fz(d.tx - 1, d.ty) ? 8 : 0);
            if (!(mask & 10) && !(mask & 5)) mask = p == Prop::FenceH ? 10 : 5;   // a lone piece keeps its run's axis
            bool snow = false;
            if (hedge) {
              const Biome hb = m.biomeAt(d.tx, d.ty);
              snow = hb == Biome::Snow || m.at(d.tx, d.ty) == Ground::Snow || (hb == Biome::Taiga && m.heightAt(d.tx, d.ty) >= 4);
            }
            const uint64_t k = ew::mix64(ps->key() ^ ((uint64_t)(0x40 + mask + (hedge ? 16 : 0) + (snow ? 32 : 0)) << 48) ^ 0xD7CEull);
            auto it = styledProps_.find(k);
            const Tex& dt = it != styledProps_.end() ? it->second
                                                     : (styledProps_[k] = pix_->bake(hedge ? art::hedgePiece(mask, *ps, snow) : art::dykePiece(mask, *ps)));
            P.blit(dt, d.tx * 16.0f - cam.x, d.ty * 16.0f + 16.0f - 24.0f - cam.y);
            break;
          }
        }
        // (M2) every peak of a range is one of eight shapes in its land's rock (by its global tile: never one peak cloned)
        if ((p == Prop::Peak || p == Prop::GreatPeak) && m.kind == MapKind::Overworld) {
          const int land = peakLand(m, d.tx, d.ty);
          tp = &cachedTex(0x09ull << 56 | (p == Prop::GreatPeak ? 128ull : 0ull) | (uint64_t)(land & 15) << 3 | (uint64_t)(hash2(d.tx + g.world.ox, d.ty + g.world.oy, 6229) & 7u), paintPeak);
        }
        // (M2 fixer round 2) the wild's commonest props in variants by their global tile, some flipped
        bool flipVar = false;
        // (M3b) every standing stone of a ring is its own stone (height, lean, top, lichen), in its land's culture
        if (p == Prop::StandingStone && m.kind == MapKind::Overworld) {
          const uint32_t h = hash2(d.tx + g.world.ox, d.ty + g.world.oy, 6257);
          const art::PropStyle* ps = g.mode == Mode::Title ? nullptr : landStyleAt(g, d.tx, d.ty);
          const int cul = ps ? (int)ps->culture : 0;
          tp = &cachedTex(0x0Cull << 56 | (uint64_t)cul << 8 | (uint64_t)(h & 15u), paintStandingStone);   // (never flipped: the light)
        }
        if ((p == Prop::TallGrass || p == Prop::Boulder) && m.kind == MapKind::Overworld) {
          const uint32_t h = hash2(d.tx + g.world.ox, d.ty + g.world.oy, 6241);
          tp = &cachedTex((p == Prop::TallGrass ? 0x0Aull : 0x0Bull) << 56 | (uint64_t)(h & 15u), p == Prop::TallGrass ? paintTallGrass : paintBoulder);
          flipVar = ((h >> 5) & 1) != 0;
        }
        // (M3c) the Wildlands flora: a variant by its global tile, as its biome grows it (art_flora.cpp floraVariant;
        // the variants carry their own mirroring, so never flipped here: the light stays top-left)
        if (art::isWildlandsFlora(p) && m.kind == MapKind::Overworld) {
          const uint32_t h = hash2(d.tx + g.world.ox, d.ty + g.world.oy, 6271);
          const int nv = std::max(1, art::floraVariants(p));
          tp = &cachedTex(0x5Eull << 56 | (uint64_t)p << 16 | (uint64_t)m.ecoAt(d.tx, d.ty) << 8 | (uint64_t)(h % (uint32_t)nv), paintFlora);
        }
        // (M3c fixer round 2) a leafy classic tree standing in the snow (a town's oaks and willows, an elven heart tree)
        // carries snow on its crown like the pines beside it
        if (m.kind == MapKind::Overworld && (p == Prop::OakTree || p == Prop::OakTree2 || p == Prop::WillowTree || p == Prop::BirchTree ||
                                             p == Prop::AutumnTree || p == Prop::ElderTree)) {
          const Biome hb = m.biomeAt(d.tx, d.ty);
          const int lv = m.heightAt(d.tx, d.ty);
          if (m.at(d.tx, d.ty) == Ground::Snow || hb == Biome::Snow || (hb == Biome::Taiga && lv >= 4) || (hb == Biome::Mountain && lv >= 5))
            tp = &cachedTex(0x5Full << 56 | (uint64_t)p, paintWinterTree);
        }
        // (stall facings) the market's stalls, tables, cloths and carts are 3/4 models on canvases of their own size,
        // placed by their origin from the prop tile; an open stall or table hangs its lantern at dusk (the light pass
        // adds its glow); after the stall's closing hour it is dark and its keeper has gone
        if (m.kind == MapKind::Overworld && (art::isVendorProp(p) || p == Prop::Cart)) {
          int ox = 0, oy = 0;
          if (art::isStall(p)) art::stallOrigin(stallFacing, ox, oy);
          else if (p == Prop::MarketTable) art::marketTableOrigin(ox, oy);
          else if (p == Prop::GroundCloth) art::groundClothOrigin(ox, oy);
          else {
            tp = &cachedTex(0x3Dull << 56 | (uint64_t)(g.mode != Mode::Title && snowyTile(m, d.tx, d.ty) ? 1 : 0) << 8 | (uint64_t)cartLook(g, d.tx, d.ty), paintCart);
            art::marketCartOrigin(ox, oy);
          }
          const float x = d.tx * 16.0f + ox - cam.x, y = d.ty * 16.0f + oy - cam.y;
          P.blit(*tp, x, y);
          // (fixer M5 r3, review: "no silhouette when the hero is behind a stall") a stall's roof and counter drawn over
          // the hero (who sorts before it) hide them whole: the hero shows through as the buildings' silhouette
          if (art::isStall(p) && g.mode != Mode::Title && pl.p.y < d.y) {
            const bool ns = stallFacing >= art::StallE;
            const float hx0 = ns ? d.tx * 16.0f + ox + 8.0f : d.tx * 16.0f - 14.0f, hx1 = ns ? d.tx * 16.0f + ox + tp->w - 8.0f : d.tx * 16.0f + 30.0f;
            if (pl.p.x > hx0 && pl.p.x < hx1 && pl.p.y > d.ty * 16.0f + oy + 18.0f) ghost = true;
          }
          const bool lamp = g.mode != Mode::Title && g.daylight() < 0.55f && vendorOpen;
          if (lamp && p == Prop::MarketTable) {   // a candle lantern standing at the table's far end
            const float lx = d.tx * 16.0f + 27.0f - cam.x, ly = d.ty * 16.0f - 6.0f - cam.y;
            P.rect(lx - 1, ly - 3, 3, 1, Color(0.32f, 0.24f, 0.16f));
            P.rect(lx - 1, ly - 2, 3, 4, Color(1.0f, 0.78f, 0.38f));
            P.rect(lx, ly - 1, 1, 2, Color(1.0f, 0.97f, 0.78f));
            P.rect(lx - 1, ly + 2, 3, 1, Color(0.32f, 0.24f, 0.16f));
          }
          if (lamp && art::isStall(p)) {
            int lxi, lyi;
            art::stallLantern(stallFacing, lxi, lyi);
            const float lx = d.tx * 16.0f + lxi - cam.x, ly = d.ty * 16.0f + lyi - cam.y;
            P.rect(lx, ly - 5, 1, 2, Color(0.22f, 0.18f, 0.14f));       // the hook
            P.rect(lx - 2, ly - 3, 5, 1, Color(0.32f, 0.24f, 0.16f));   // the cap
            P.rect(lx - 2, ly - 2, 5, 4, Color(1.0f, 0.78f, 0.38f));    // the glass
            P.rect(lx - 1, ly - 1, 3, 2, Color(1.0f, 0.97f, 0.78f));    // the flame
            P.rect(lx - 2, ly + 2, 5, 1, Color(0.32f, 0.24f, 0.16f));   // the base
          }
          break;
        }
        // (M4) a war prop in the colours of the kingdom whose camp it belongs to (realm_render.cpp)
        bool warSwap = false;
        if (art::isWarProp(p) && m.kind == MapKind::Overworld && g.mode != Mode::Title)
          if (const Tex* wt = warPropTex(g, p, d.tx, d.ty)) { tp = wt; warSwap = true; }
        // (M5, VISION_PLAN 10.3) a street lamp burns only from the lamplighter's visit at dusk (19:00..20:30 along his
        // round) until it is put out at dawn; by day and before he comes its glass is dark
        bool lampFlameOn = false;
        if (p == Prop::Lamppost && m.kind == MapKind::Overworld && g.mode != Mode::Title) {
          if (life::lampLit(d.tx + g.world.ox, d.ty + g.world.oy, g.hour)) {
            m5Count_.lampsLit++;
            lampFlameOn = tp == &props_[(int)p];   // the classic lantern: its flame drawn live over the glass (ART lampHead)
          } else {
            tp = &lampDarkTex(propStyleAt(g, d.tx, d.ty));
            m5Count_.lampsDark++;
          }
        }
        const Tex& t = *tp;
        int fw = art::propW(p), fh = art::propH(p);
        int frames = warSwap ? 1 : std::max(1, art::propFrames(p));
        int fr = frames > 1 ? (int)(t_ * 8 + d.tx * 3 + d.ty) % frames : 0;
        bool flipP = flipVar;
        if (p == Prop::Sheep || p == Prop::Cow) {   // (M1 fixer round 2) the beasts graze at their own slow pace, either way round
          const uint32_t h = hash2(d.tx + g.world.ox, d.ty + g.world.oy, 6197);
          fr = (int)(t_ * 1.6f + (float)(h % 97u) * 0.37f) % frames;
          flipP = ((h >> 8) & 1) != 0;
        }
        float jx = 0, jy = 0;
        if (natureProp(p) && m.kind == MapKind::Overworld) {
          // (M3c fixer) by the global tile (the local one moved every patch when the window recentred); the ground
          // covers walked through (grasses, heather, ferns, brush) take a wider sub-tile offset so drifts of them never
          // line up in rows; the solids stay close to their tile (the forest rule's spacing)
          const uint32_t h = hash2(d.tx + g.world.ox, d.ty + g.world.oy, 55);
          if (art::isWildSolidProp(p)) { jx = (float)((int)(h % 7) - 3); jy = (float)((int)((h >> 4) % 3) - 1); }
          else { jx = (float)((int)(h % 11) - 5); jy = (float)((int)((h >> 4) % 7) - 3); }
        }
        float x = d.tx * 16 + 8 - fw / 2 + jx - cam.x, y = d.ty * 16 + 16 - fh + jy - cam.y;
        float alpha = 1;
        // a tree crown over the hero (drawn before it): the crown thins out and the hero shows through (ghost below)
        if ((treeProp(p) || tallWild(p)) && pl.p.y < d.ty * 16 + 15 && pl.p.y > d.ty * 16 + 16 - fh + 8 && std::fabs(pl.p.x - (d.tx * 16 + 8 + jx)) < fw * 0.5f) {
          alpha = tallWild(p) ? 0.55f : 0.6f;
          ghost = true;
        }
        P.blitEx(t, fr * fw, 0, fw, fh, x, y, (float)fw, (float)fh, flipP, Color(1, 1, 1, alpha));
        if (lampFlameOn) {   // (M5) the lit lamp's flame, flickering on its own clock
          const Tex& ft = cachedTex(0x4Full << 56, [](uint64_t) { return art::lampFlame(0); });
          int hx = 0, hy = 0;
          art::lampHead(hx, hy);
          const int ff = (int)(t_ * 7.0f + (float)(hash2(d.tx + g.world.ox, d.ty + g.world.oy, 6301) % 97u) * 0.31f) % art::LAMP_FLAME_FRAMES;
          P.blitRegion(ft, ff * art::LAMP_FLAME_W, 0, art::LAMP_FLAME_W, art::LAMP_FLAME_H, std::floor(x + hx - art::LAMP_FLAME_W / 2.0f), std::floor(y + hy - art::LAMP_FLAME_H + 1.0f));
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
          const bool done = bldgPaintStep(b, d.idx, std::max(0.5, std::min(4.0, 10.0 - fadePaintMs_)));   // (M3: in steps)
          fadePaintMs_ += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
          if (!done) break;
        }
        const Tex* tp = &bldgTex(b, d.idx);
        if (m.kind == MapKind::Overworld && g.mode != Mode::Title && windowsLit(g, b)) {
          auto nt = bldgNight_.find(bldgKey(m, b, d.idx));
          if (nt != bldgNight_.end()) tp = &nt->second;
          m5Count_.winLit++;
        } else if (m.kind == MapKind::Overworld && g.mode != Mode::Title && !g.inside && g.daylight() <= 0.55f) m5Count_.winDark++;
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
        if (b.charred && m.kind == MapKind::Overworld) burnedFx(g, m, b, d.idx, 0);   // (M4) a burned building smoulders
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
        // (M3) the gate in its wall's culture style (Map::wall on the flanking tile is 1 + CityWall) and the kingdom's arms
        int ws = 0;
        if (m.in(d.tx - 1, d.ty) && m.wall[(size_t)d.ty * m.w + d.tx - 1] > 1) ws = m.wall[(size_t)d.ty * m.w + d.tx - 1] - 1;
        else if (m.in(d.tx + 3, d.ty) && m.wall[(size_t)d.ty * m.w + d.tx + 3] > 1) ws = m.wall[(size_t)d.ty * m.w + d.tx + 3] - 1;
        const int gs = g.world.siteAt(d.tx + 1, d.ty, 3);
        bld::FortParts fp;
        if (fortPartsOfSite(g, gs, fp)) {   // (M3b) the gate in its culture's form, flying its kingdom's arms
          cult::Heraldry arms;
          if (const Kingdom* k = g.world.kingdomOf(gs)) {
            const cult::Culture* kc = g.world.cultureOfKingdom(g.world.sites[(size_t)gs].kingdom);
            if (kc && !kc->heraldry.empty()) arms = kc->heraldry;
            else { arms.field = k->color; arms.charge = k->color2; arms.emblem = k->emblem; }
          }
          gt = &fortGateTex(arms, fp);
        } else if (const Kingdom* k = g.world.kingdomOf(gs)) {
          const cult::Culture* kc = g.world.cultureOfKingdom(g.world.sites[(size_t)gs].kingdom);
          if (kc && !kc->heraldry.empty()) gt = &heraldryTex(1, kc->heraldry, 7, ws);
          else if (ws) { cult::Heraldry h; h.field = k->color; h.charge = k->color2; h.emblem = k->emblem; gt = &heraldryTex(1, h, 7, ws); }
          else gt = &kingdomTex(1, *k, 7);
        } else if (ws) {
          cult::Heraldry h;
          gt = &heraldryTex(1, h, 7, ws);
        }
        P.blit(*gt, d.tx * 16.0f - art::GATE_OX - cam.x, d.ty * 16.0f - art::GATE_OY - cam.y);
        break;
      }
      case 3: {
        const Actor& a = g.actors[d.idx];
        float flash = a.flash > 0 ? a.flash / 0.12f : 0;
        float alpha = a.st == AState::Dead ? clampf(1.0f - (a.stT - 4.0f) / 2.0f, 0, 1) : 1.0f;
        if (a.player && a.iframes > 0 && a.st != AState::Roll && ((int)(t_ * 20) & 1)) alpha *= 0.5f;
        if (a.human) {
          const bool child = a.role == Role::Child;   // (M5) a shorter body (childBody)
          const Tex* tp = &humanTex(a.look, child);
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
          // (M5) what the body is doing (art::Posture): its pose sheet, frames looped at the pose's pace (each actor a
          // little out of step with the next, so a tavern does not lift its mugs as one), sunk onto the seat by dy
          // (seated, begging), laid over the bed (one row: the bed's way); combat states keep the standing sheet
          Vec2 at = a.p;   // where the figure's feet are drawn (a seat's fit moves it onto the seat)
          bool drawn = false;
          bool floorSeat = false;   // (fixer M5 r3) sat on the bare floor: a cushion goes under them
          if (a.posture != art::Posture::None && (a.st == AState::Idle || a.st == AState::Walk)) {
            art::Posture pz = a.posture;
            const art::PostureInfo pi = art::postureInfo(pz);
            const int nf = std::clamp((int)pi.frames, 1, art::POSTURE_FRAMES);
            const bool stride = a.st == AState::Walk && (pz == art::Posture::Carry || pz == art::Posture::Play || pz == art::Posture::Dance);
            const float clock = (a.postureT > 0 ? a.postureT : t_) + (float)(hash32((uint32_t)a.id * 2654435761u) % 997u) * 0.0131f;
            const int pf = stride ? (int)(a.animT * 9) % nf : (int)(clock * std::max(0.1f, pi.fps)) % nf;
            uint8_t variant = 0;
            // (ART seatFit) on a seat: the posture for its kind (chair height or cross-legged) and the sitter's feet
            // where its seat line lands on the seat's surface; a facing the seat does not take keeps the figure where it is
            if (pi.seated && a.useX >= 0 && a.useY >= 0 && m.in(a.useX, a.useY)) {
              const int sp = m.propAt(a.useX, a.useY);
              const art::Prop seat = sp ? (art::Prop)(sp - 1) : art::Prop::COUNT;
              const art::SeatFit f = art::seatFit(seat, -1, a.face);
              if (f.ok) {
                pz = art::seatedPosture(seat, pz);
                floorSeat = seat == art::Prop::COUNT;
                at = Vec2(a.useX * 16.0f + 8.0f + (a.face == 3 ? -f.ax : f.ax), a.useY * 16.0f + 16.0f + f.ay);
                // (M5 fixer r2) on the bench before a long hearth: forward on its edge, so the embers and logs show over
                // the shoulders (life_game.cpp sitDown places the body the same)
                if (seat == art::Prop::Bench && a.face == 1 && m.in(a.useX, a.useY - 1)) {
                  const int up = m.propAt(a.useX, a.useY - 1);
                  if (up == (int)art::Prop::FirePitL + 1 || up == (int)art::Prop::FirePitM + 1 || up == (int)art::Prop::FirePitR + 1) at.y += 5;
                }
              }
            }
            // (ART bedFit) in a bed: the sleeper fitted to the berth, drawn right after it at the berth's own anchor
            if (pi.lying && a.useX >= 0 && a.useY >= 0 && m.in(a.useX, a.useY)) {
              int kit = -1;
              const art::Berth bk = berthAt(m, a.useX, a.useY, kit);
              const int sf = (int)(clock * std::max(0.1f, pi.fps)) & 1;
              if (const Tex* st = sleeperTex(a.look, bk, kit, sf)) {
                const art::BedFit bf = art::bedFit(bk, kit);
                const float sx = std::floor(a.useX * 16.0f + 8.0f - bf.w / 2.0f - cam.x), sy = std::floor(a.useY * 16.0f + 16.0f - bf.h - cam.y);
                P.blitEx(*st, 0, 0, st->w, st->h, sx, sy, (float)st->w, (float)st->h, false, Color(1, 1, 1, alpha));
                drawn = true;
                m5Count_.posed++;
              }
            }
            // the bard's instrument in his people's style (ART: humanPostureSheet variants by MusicStyle)
            if (pz == art::Posture::Lute || pz == art::Posture::Flute || pz == art::Posture::Drum) {
              const int cs = g.inside && g.subBldg >= 0 && g.subBldg < (int)g.world.over.bldgs.size() ? g.world.over.bldgs[(size_t)g.subBldg].site : a.site;
              if (const cult::Culture* C = cs >= 0 ? g.world.cultureOf(cs) : nullptr) {
                if (pz == art::Posture::Drum) variant = (uint8_t)C->music.perc;
                else if (art::bardPosture(C->music) == pz) variant = art::bardVariant(C->music);
              }
            }
            if (!drawn) {
              if (const Tex* pt = poseTex(a.look, pz, variant, child)) {
                tp = pt;
                fr = pf;
                if (pi.oneRow || pi.lying) { row = 0; flip = false; }
                m5Count_.posed++;
              } else {
                fr = stride ? 1 + (int)(a.animT * 9) % 4 : 0;   // the stand-in this frame: its own standing sheet
                m5Count_.poseFallback++;
              }
              bob += (float)art::postureInfo(pz).dy;
            }
          }
          const Tex& t = *tp;
          float x = at.x - art::HUMAN_W / 2.0f - cam.x, y = at.y - art::HUMAN_H + 2 + bob - cam.y;
          if (a.st == AState::Dead) y += 3;
          x = std::floor(x + 0.5f);   // (M5) crisp at 1x: whole pixels (a seated or sleeping body never shimmers)
          y = std::floor(y + 0.5f);
          // (M5 fixer r2) a bather in a bath's pool: head and shoulders above the water, a ring of ripples at the waterline
          if (!drawn && bathing(g, m, a)) {
            const int vis = 14, under = 5;   // head and shoulders in the air, the chest a blur under the water
            const float wy = std::floor(a.p.y - 3 - cam.y + 0.5f);
            const float bobW = (float)((int)(t_ * 1.3f + a.id * 0.7f) & 1);
            P.blitEx(t, fr * art::HUMAN_W, row * art::HUMAN_H, art::HUMAN_W, (float)vis, x, wy - vis + bobW, (float)art::HUMAN_W, (float)vis, flip, Color(1, 1, 1, alpha));
            P.blitEx(t, fr * art::HUMAN_W, row * art::HUMAN_H + vis, art::HUMAN_W, (float)under, x, wy + bobW, (float)art::HUMAN_W, (float)under, flip,
                     Color(0.45f, 0.75f, 0.85f, 0.38f * alpha));
            P.rect(x + 2, wy, 12, 1, Color(0.78f, 0.92f, 0.95f, 0.55f * alpha));
            P.rect(x + 1, wy + 1, 14, 1, Color(0.05f, 0.18f, 0.26f, 0.35f * alpha));
            P.rect(x + 4, wy - 1, 2, 1, Color(0.9f, 0.97f, 1.0f, 0.4f * alpha));
            drawn = true;
            m5Count_.posed++;
          }
          if (floorSeat && !drawn) {   // (fixer M5 r3) "seated patrons hover with no seat": a cushion under them
            const Tex& ct = cachedTex(0x6Aull << 56 | (uint64_t)(hash32((uint32_t)(a.useX * 73856093) ^ (uint32_t)(a.useY * 19349663)) & 3u),
                                      paintFloorCushion);
            P.blitEx(ct, 0, 0, ct.w, ct.h, std::floor(at.x - 9.0f - cam.x + 0.5f), std::floor(at.y - 5.0f - cam.y + 0.5f), (float)ct.w, (float)ct.h,
                     false, Color(1, 1, 1, alpha));
          }
          if (!drawn) P.blitEx(t, fr * art::HUMAN_W, row * art::HUMAN_H, art::HUMAN_W, art::HUMAN_H, x, y, (float)art::HUMAN_W, (float)art::HUMAN_H, flip, Color(1, 1, 1, alpha));
          if (!drawn) drawnHead_[a.id] = DrawnHead{at.x, y + cam.y, t_};
          if (a.player) { ghostTex = &t; ghostFr = fr; ghostRow = row; ghostFlip = flip; ghostX = x; ghostY = y; }
          if (flash > 0 && !drawn) P.blitEx(t, fr * art::HUMAN_W, row * art::HUMAN_H, art::HUMAN_W, art::HUMAN_H, x, y, (float)art::HUMAN_W, (float)art::HUMAN_H, flip, Color(1, 1, 1, flash), 1);
          if (a.burnT > 0 && ((int)(t_ * 10) & 1)) P.rectAdd(x + 5, y + 6, 6, 10, Color(0.6f, 0.25f, 0.05f, 0.6f));
        } else if (a.critter > 0) {
          // (M5) a village animal (art::critterSheet): columns 0 idle, 1-4 walk, 5-6 its own action, 7 lying down; rows
          // down / up / right (left flipped). Standing about it now and then does its thing (pecks, grooms, wags), each
          // on its own clock; a posture from the sim (Sleep: lying down; any other: the action) wins.
          const int ck = a.critter - 1;
          const Tex& t = critterTex(ck, a.critterVar);
          const int cw = art::critterCellW((art::Critter)ck), chh = art::critterCellH((art::Critter)ck);
          const int row = a.face == 0 ? 0 : a.face == 1 ? 1 : 2;
          const bool flip = a.face == 3;
          int fr = 0;
          const float own = (float)(hash32((uint32_t)a.id * 0x9E3779B1u) % 1000u) * 0.001f;
          if (a.st == AState::Walk) fr = 1 + (int)(a.animT * 10) % 4;
          else if (a.posture == art::Posture::Sleep) fr = 7;
          else if (a.posture != art::Posture::None) fr = 5 + (int)((a.postureT > 0 ? a.postureT : t_) * 3 + own * 7) % 2;
          else {
            const float ph = std::fmod(t_ * 0.21f + own * 3.7f, 1.0f);
            if (ph < 0.32f) fr = 5 + (int)(t_ * (ck == (int)art::Critter::Cat ? 2.5f : 5.0f) + own * 9) % 2;
          }
          const float x = std::floor(a.p.x - cw / 2.0f - cam.x + 0.5f), y = std::floor(a.p.y - chh + 2 - cam.y + 0.5f);
          P.blitEx(t, fr * cw, row * chh, cw, chh, x, y, (float)cw, (float)chh, flip, Color(1, 1, 1, alpha));
          if (flash > 0) P.blitEx(t, fr * cw, row * chh, cw, chh, x, y, (float)cw, (float)chh, flip, Color(1, 1, 1, flash), 1);
          m5Count_.critters++;
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
          // (M3c) a lurker that hasn't seen you lies low in the shallows or the mud: only its back, eyes and snout show,
          // with a ripple round it
          int srcH = chh;
          if (a.mon == Monster::Lurker && !a.aggro && a.st != AState::Dead && (a.st == AState::Idle || a.st == AState::Walk) && m.kind == MapKind::Overworld) {
            const int ltx = (int)std::floor(a.p.x / 16), lty = (int)std::floor((a.p.y - 2) / 16);
            bool wet = m.at(ltx, lty) == Ground::Swamp;
            for (int k = 0; k < 4 && !wet; k++) wet = groundWater(m.at(ltx + (k == 0) - (k == 1), lty + (k == 2) - (k == 3)));
            if (wet) {
              srcH = chh - 6;
              y += 6;
              const float rp = 0.5f + 0.5f * std::sin(t_ * 2.2f + a.id);
              P.rect(x + 4, y + srcH - 1, (float)cw - 8, 1, Color(0.75f, 0.85f, 0.85f, 0.35f + 0.2f * rp));
              P.rect(x + 8 - rp * 2, y + srcH + 1, (float)cw - 16 + rp * 4, 1, Color(0.75f, 0.85f, 0.85f, 0.18f));
            }
          }
          P.blitEx(t, fr * cw, 0, cw, srcH, x, y, (float)cw, (float)srcH, flip, Color(1, a.slowT > 0 ? 0.85f : 1, a.slowT > 0 ? 1 : 1, alpha));
          if (a.mon == Monster::Wisp && a.st != AState::Dead)   // light: an additive pass makes it glow on any ground
            P.blitEx(t, fr * cw, 0, cw, chh, x, y, (float)cw, (float)chh, flip, Color(0.6f, 1.0f, 1.0f, 0.55f + 0.25f * std::sin(t_ * 11 + a.id)), 1);
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
            // (the sim's heavyWindup, game_internal.h)
            const float wu = a.mon == Monster::Troll || a.mon == Monster::Yeti ? 0.9f : a.mon == Monster::Blightspawn ? 0.75f : 0.8f;
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
            if (pr.ench == Ench::Frost && !pr.fromPlayer) {   // (M3c) a blightspawn's spore clot: a soft sickly puff
              const float a = clampf(pr.life / 0.42f, 0, 1);
              P.rect(x - 3, y - 2, 6, 4, Color(0.48f, 0.36f, 0.50f, 0.35f * a));
              P.rect(x - 2, y - 3, 4, 6, Color(0.48f, 0.36f, 0.50f, 0.35f * a));
              P.rect(x - 1, y - 1, 2, 2, Color(0.72f, 0.86f, 0.38f, 0.8f * a));
              break;
            }
            P.rect(x - 2, y - 2, 4, 4, Color(0.55f, 0.85f, 0.3f));
            P.rect(x - 1, y - 1, 2, 2, Color(0.8f, 1, 0.6f));
            break;
          case ProjKind::Magic:
            if (pr.ench == Ench::None && !pr.fromPlayer) {   // (M3c) a wisp's bolt of light: cyan, with a short trail
              for (int i = 1; i <= 3; i++) P.rectAdd(x - dir.x * i * 2 - 1, y - dir.y * i * 2 - 1, 2, 2, Color(0.4f, 0.9f, 0.9f, 0.5f - i * 0.12f));
              P.rectAdd(x - 3, y - 3, 6, 6, Color(0.3f, 0.8f, 0.8f, 0.7f));
              P.rect(x - 1, y - 1, 2, 2, Color(0.9f, 1, 1));
              break;
            }
            P.rectAdd(x - 3, y - 3, 6, 6, Color(0.4f, 0.3f, 0.9f, 0.7f));
            P.rect(x - 1, y - 1, 2, 2, Color(0.85f, 0.8f, 1));
            break;
        }
        break;
      }
      case 7:   // (M5) a festival pole
        if (d.idx >= 0 && d.idx < (int)festSpans_.size()) drawFestPole(festSpans_[(size_t)d.idx], d.tx != 0, cam);
        break;
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
  // (M5) a festival's bunting hangs overhead, over everyone walking under it
  if (g.mode != Mode::Title && m.kind == MapKind::Overworld) drawFestival(g, m, cam);
  else festLights_.clear();
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
    Color night(0.12f, 0.15f, 0.30f), dusk(1.0f, 0.74f, 0.58f), noon(1, 1, 1);
    // (M2 fixer) over snow the warm dusk multiplied the white into salmon (it read as desert): the cold lands get a
    // cooler, rose-violet evening and a pale morning
    bool cold = false;
    if (g.mode != Mode::Title && !g.inside) {
      const Map& om = g.world.over;
      const int ptx = (int)std::floor(g.pl().p.x / 16), pty = (int)std::floor(g.pl().p.y / 16);
      const Biome pb = om.biomeAt(ptx, pty);
      cold = pb == Biome::Snow || om.at(ptx, pty) == Ground::Snow || (pb == Biome::Taiga && om.heightAt(ptx, pty) >= 4);
    }
    static float ck = -1;   // eased, so walking off the snow at dusk does not switch the light at once
    ck = ck < 0 ? (cold ? 1.0f : 0.0f) : ck + ((cold ? 1.0f : 0.0f) - ck) * 0.02f;
    auto mixC = [&](Color a, Color b) { return Color(lerpf(a.r, b.r, ck), lerpf(a.g, b.g, ck), lerpf(a.b, b.b, ck)); };
    dusk = mixC(dusk, Color(0.86f, 0.76f, 0.86f));
    float h = g.hour;
    bool evening = h > 12;
    if (day >= 1) amb = noon;
    else if (day <= 0) amb = night;
    else {
      Color mid = evening ? dusk : mixC(Color(1.0f, 0.82f, 0.7f), Color(0.90f, 0.86f, 0.90f));
      amb = day > 0.5f ? Color(lerpf(mid.r, 1, (day - 0.5f) * 2), lerpf(mid.g, 1, (day - 0.5f) * 2), lerpf(mid.b, 1, (day - 0.5f) * 2))
                       : Color(lerpf(night.r, mid.r, day * 2), lerpf(night.g, mid.g, day * 2), lerpf(night.b, mid.b, day * 2));
    }
    // (M3b round 3) the golden hour: from mid-afternoon the light warms and lowers (it stayed at full noon until 18:00
    // and barely moved by 19:00), deepest round sunset, giving way to the dusk grade as the light goes
    if (evening && h > 16.0f && h < 21.0f) {
      auto sm = [](float t) { t = clampf(t, 0, 1); return t * t * (3 - 2 * t); };
      const float w = 0.5f * sm((h - 16.0f) / 2.5f) * (1.0f - sm((h - 19.5f) / 1.5f));
      const Color warm = mixC(Color(1.0f, 0.80f, 0.60f), Color(0.92f, 0.80f, 0.90f));
      amb = Color(amb.r * lerpf(1, warm.r, w), amb.g * lerpf(1, warm.g, w), amb.b * lerpf(1, warm.b, w));
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
        if (dark > 0.15f && m.kind == MapKind::Overworld && ew::stallOpen(tx + g.world.ox, ty + g.world.oy, g.hour) &&
            !(lifeFlagsAt(g, tx, ty) & (life::MF_SHUTTERED | life::MF_FAMINE))) {
          const float f = 0.9f + 0.1f * std::sin(t_ * 7 + tx * 1.3f);
          const float k = std::min(1.0f, dark * 2.2f) * f;
          int lx, ly;   // (stall facings) the lantern hangs where its facing puts it, its warm pool on the ground below
          art::stallLantern(stallFacingOn(g, m, tx, ty), lx, ly);
          light(Vec2(tx * 16 + lx + 0.5f, ty * 16 + ly + 0.0f), 34, Color(1.0f, 0.86f, 0.55f), 0.95f * k);
          light(Vec2(tx * 16 + lx + 0.5f, ty * 16 + ly + 18.0f), 72, Color(1.0f, 0.66f, 0.34f), 0.6f * k);
        }
        continue;
      }
      if (!interior && (Prop)(pr - 1) == Prop::MarketTable) {   // (M1 fixer round 2) an open table's candle lantern
        if (dark > 0.15f && m.kind == MapKind::Overworld && ew::stallOpen(tx + g.world.ox, ty + g.world.oy, g.hour) &&
            !(lifeFlagsAt(g, tx, ty) & (life::MF_SHUTTERED | life::MF_FAMINE))) {
          const float f = 0.9f + 0.1f * std::sin(t_ * 7 + tx * 1.7f);
          const float k = std::min(1.0f, dark * 2.2f) * f;
          light(Vec2(tx * 16 + 27.5f, ty * 16 - 6.0f), 26, Color(1.0f, 0.86f, 0.55f), 0.9f * k);
          light(Vec2(tx * 16 + 16.0f, ty * 16 + 6.0f), 54, Color(1.0f, 0.66f, 0.34f), 0.5f * k);
        }
        continue;
      }
      if (!propLight((Prop)(pr - 1), r, c)) continue;
      // (M5) a street lamp gives light only once the lamplighter has lit it (life::lampLit)
      if ((Prop)(pr - 1) == Prop::Lamppost && !interior && m.kind == MapKind::Overworld && g.mode != Mode::Title &&
          !life::lampLit(tx + g.world.ox, ty + g.world.oy, g.hour))
        continue;
      float f = 0.85f + 0.15f * std::sin(t_ * 9 + tx * 1.7f + ty);
      light(Vec2(tx * 16 + 8.0f, ty * 16 + 4.0f), r, c, 0.9f * f);
      // (M5) a lit street lamp's head shines: a tight bright bloom round its glass (the pool alone lit the ground)
      if ((Prop)(pr - 1) == Prop::Lamppost && !interior && m.kind == MapKind::Overworld)
        light(Vec2(tx * 16 + 8.0f, ty * 16 + 16.0f - art::propH(Prop::Lamppost) + 6.0f), 26, Color(1.0f, 0.92f, 0.72f), 0.95f * f * std::min(1.0f, dark * 2.0f));
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
      // (fixer M5 r3) the door's pool lies on the street in front (it reached up over porch and portico roofs)
      light(c + Vec2(0, 14), 14 + b.r.w * 3.0f, Color(1, 0.7f, 0.35f), 0.40f * dark);
      const uint64_t bk = bldgKey(m, b, (int)bi);
      auto w = bldgWin_.find(bk);
      auto t = bldgTex_.find(bk);
      if (w == bldgWin_.end() || t == bldgTex_.end()) continue;
      const float bottom = (b.r.y + b.r.h) * 16.0f, top = bottom + art::BLDG_PAD_B - t->second.h;
      float f = 0.94f + 0.06f * std::sin(t_ * 5 + bi);
      // (fixer M5 r3) the panes themselves through their mask: only the glass is lit (a round pool per window lit
      // the roofs in front of an upper facade at near daylight brightness)
      auto gm = bldgGlow_.find(bk);
      if (gm != bldgGlow_.end())
        P.blitEx(gm->second, 0, 0, gm->second.w, gm->second.h, (b.r.x * 16.0f - art::BLDG_PAD_X - cam.x) * 0.5f, (top - cam.y) * 0.5f,
                 (float)gm->second.w, (float)gm->second.h, false, Color(1, 0.86f, 0.6f, 0.95f * dark * f), 1);
      const float groundRow = (float)t->second.h - art::BLDG_PAD_B - 30.0f;   // the ground floor's windows (sprite px)
      for (const Vec2& wc : w->second) {
        if (wc.y < groundRow) continue;   // an upper window lights its own glass, not the roofs below it
        Vec2 wp(b.r.x * 16.0f - art::BLDG_PAD_X + wc.x, top + wc.y);
        light(Vec2(wp.x, std::max(wp.y + 10.0f, bottom - 2.0f)), 36, Color(1, 0.62f, 0.3f), 0.30f * dark * f);   // spill on the street
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
  // (M5) a festival's paper lanterns at the doors and stalls: a warm bead of light and a soft pool on the ground below
  if (!g.inside && m.kind == MapKind::Overworld && dark > 0.15f)
    for (size_t i = 0; i < festLights_.size(); i++) {
      const Vec2 lp = festLights_[i];
      const float f = 0.88f + 0.12f * std::sin(t_ * 6.3f + (float)i * 1.9f);
      const float k = std::min(1.0f, dark * 2.0f) * f;
      light(lp, 26, Color(1.0f, 0.80f, 0.52f), 0.9f * k);
      light(lp + Vec2(0, 16), 56, Color(1.0f, 0.58f, 0.34f), 0.42f * k);
    }
  {   // (M4) the siege camps' tent lanterns and the red smoulder of burned-out buildings (realm_render.cpp)
    static std::vector<LightPool> pools;
    m4Lights(g, m, cam, dark, pools);
    for (const LightPool& lp : pools) light(lp.p, lp.r, lp.c, lp.k);
  }
  for (const Projectile& pr : g.projs) {
    if (pr.kind == ProjKind::Fireball || pr.kind == ProjKind::DragonFire) light(pr.p, 60, Color(1, 0.6f, 0.25f), 0.9f);
    else if (pr.kind == ProjKind::Magic && pr.ench == Ench::None && !pr.fromPlayer) light(pr.p, 40, Color(0.4f, 1.0f, 0.95f), 0.8f);
    else if (pr.kind == ProjKind::IceSpike || pr.kind == ProjKind::Magic) light(pr.p, 40, Color(0.5f, 0.6f, 1), 0.8f);
  }
  for (const Particle& q : parts_) if (q.kind == 1 && q.fx == (int)art::Fx::Explosion) light(q.p, 90, Color(1, 0.6f, 0.3f), q.life / q.max);
  // (M3c) the ash fields' red-brown glow under the haze, and the magic lands' faint night light
  if (!g.inside && g.mode != Mode::Title && dark > 0.2f && wx_.ash > 0.05f)
    P.rectAdd(0, 0, (float)lightMap_.w, (float)lightMap_.h, Color(0.30f, 0.10f, 0.04f, wx_.ash * dark * 0.6f));
  if (!g.inside && g.mode != Mode::Title && dark > 0.2f && wx_.eerie > 0.05f)
    P.rectAdd(0, 0, (float)lightMap_.w, (float)lightMap_.h, Color(wx_.er * 0.12f, wx_.eg * 0.12f, wx_.eb * 0.12f, wx_.eerie * dark));
  for (const Actor& a : g.actors) {
    if (a.human || !a.hostile || a.st == AState::Dead) continue;
    if (a.mon == Monster::Wraith) light(a.p + Vec2(0, -10), 40, Color(0.5f, 0.5f, 1), 0.6f);
    // (M3c) the wisp is a light; the ember hound smoulders
    else if (a.mon == Monster::Wisp) light(a.p + Vec2(0, -18), 56, Color(0.45f, 1.0f, 0.95f), 0.85f * (0.85f + 0.15f * std::sin(t_ * 11 + a.id)));
    else if (a.mon == Monster::EmberHound) light(a.p + Vec2(0, -10), 38, Color(1.0f, 0.5f, 0.2f), 0.7f * (0.8f + 0.2f * std::sin(t_ * 13 + a.id)));
  }
  for (const Pickup& k : g.pickups) if (k.gold == 0 && k.item.rarity >= Rarity::Rare) light(k.p, 24, col(rarityColor(k.item.rarity)), 0.5f);
  P.setTarget(nullptr);
  P.blitEx(lightMap_, 0, 0, lightMap_.w, lightMap_.h, 0, 0, lightMap_.w * 2.0f, lightMap_.h * 2.0f, false, Color(1, 1, 1, 1), 2);
  m4Emissive(g, m, cam, dark);   // (fixer M4 r3) what burns glows over the dark
}

// (M3c LIFE) the weather of the land: wx_ (eased in update from the eco's Sky) painted in screen space. Every layer is
// a few dozen rects or soft blobs, sized to the canvas, so the phone's frame budget hardly notices.
void View::drawWeather(Game& g, float dt) {
  (void)dt;
  Pix& P = *pix_;
  if (g.inside || g.mode == Mode::Title) {
    P.blitEx(vignette_, 0, 0, vignette_.w, vignette_.h, 0, 0, (float)Pix::W, (float)Pix::H, false, Color(1, 1, 1, g.inside && g.subSite >= 0 ? 1.0f : 0.6f));
    return;
  }
  const Wx& W = wx_;
  const float sw = Pix::W / 480.0f;
  auto wrap = [](float v, float m) { v = std::fmod(v, m); return v < 0 ? v + m : v; };
  // the sky's weight: low cloud darkens the land under rain and downpours
  const float gloom = W.rain * 0.20f + W.pour * 0.30f + W.drizzle * 0.08f;
  if (gloom > 0.01f) P.rect(0, 0, (float)Pix::W, (float)Pix::H, Color(0.1f, 0.12f, 0.2f, gloom));
  // haze: sand (tan), ash (red-brown), blizzard (white-out), eerie (the land's own tint)
  if (W.sand > 0.01f) P.rect(0, 0, (float)Pix::W, (float)Pix::H, Color(0.80f, 0.64f, 0.40f, 0.30f * W.sand));
  if (W.ash > 0.01f) P.rect(0, 0, (float)Pix::W, (float)Pix::H, Color(0.42f, 0.28f, 0.24f, 0.20f * W.ash));
  if (W.eerie > 0.01f) P.rect(0, 0, (float)Pix::W, (float)Pix::H, Color(W.er * 0.6f, W.eg * 0.6f, W.eb * 0.6f, 0.06f * W.eerie));
  // heat shimmer: a warm wash and faint wavering bands of brighter air low over the ground
  if (W.shimmer > 0.02f) {
    P.rect(0, 0, (float)Pix::W, (float)Pix::H, Color(1.0f, 0.86f, 0.62f, 0.05f * W.shimmer));
    for (int i = 0; i < 7; i++) {
      const float y = Pix::H * (0.38f + 0.09f * i) + std::sin(t_ * 1.7f + i * 1.9f) * 3.0f;
      const float x0 = std::sin(t_ * 0.6f + i) * 40.0f - 40.0f;
      P.rectAdd(x0, y, Pix::W + 80.0f, 1, Color(1.0f, 0.92f, 0.75f, 0.035f * W.shimmer));
      P.rectAdd(x0 + 30, y + 2, Pix::W * 0.6f, 1, Color(1.0f, 0.92f, 0.75f, 0.02f * W.shimmer));
    }
  }
  // fog banks drifting across (marsh, moor, dawn mist, steam over the jungle)
  if (W.fog > 0.02f) {
    P.rect(0, 0, (float)Pix::W, (float)Pix::H, Color(0.84f, 0.88f, 0.88f, 0.10f * W.fog));   // the mist's pale veil
    const int n = (int)(6 * (Pix::W + 220) / 700.0f) + 2;
    for (int i = 0; i < n; i++) {
      float x = std::fmod(i * 140 + t_ * 8 - cam_.x * 0.3f + 7000.0f, (float)Pix::W + 220.0f) - 160;
      P.blitEx(light_, 0, 0, 64, 64, x, 30 + i * (Pix::H / (float)n) + std::sin(t_ * 0.3f + i) * 10, 260, 90, false,
               Color(0.82f, 0.86f, 0.84f, 0.26f * W.fog));
    }
  }
  // rain, drizzle and downpours (the drops wrap over the whole canvas and move with the camera)
  const float rainN = (140 * W.rain + 220 * W.pour + 80 * W.drizzle) * sw;
  for (int i = 0; i < (int)rainN; i++) {
    const bool heavy = i < 220 * W.pour * sw, fine = !heavy && i >= (140 * W.rain + 220 * W.pour) * sw;
    const float ww = Pix::W + 40.0f, wh = Pix::H + 30.0f;
    const float vy = heavy ? 420 : fine ? 170 : 330, vx = heavy ? 150 : fine ? 40 : 120;
    float sx = wrap(hashf(i, 0, 5) * 600 + t_ * vx - cam_.x, ww) - 20;
    float sy = wrap(hashf(i, 1, 5) * 400 + t_ * vy - cam_.y, wh) - 15;
    const int len = heavy ? 9 : fine ? 3 : 7;
    for (int k = 0; k < len; k++) P.rect(sx - k * (vx / vy), sy - k * 1.0f, 1, 1, Color(0.78f, 0.86f, 1.0f, (fine ? 0.35f : 0.6f) * (1.0f - k / (float)(len + 1))));
    // a splash where a heavy drop lands
    if (heavy && (i & 7) == 0 && std::fmod(t_ * 3 + hashf(i, 3, 5), 1.0f) < 0.15f) P.rect(sx - 1, sy + 2, 3, 1, Color(0.8f, 0.88f, 1.0f, 0.4f));
  }
  // rings where the rain strikes the ground (screen-fixed to the land: they ride with the camera)
  const float ringsN = (30 * W.rain + 50 * W.pour + 10 * W.drizzle) * sw;
  for (int i = 0; i < (int)ringsN; i++) {
    const float ph = std::fmod(t_ * 2.2f + hashf(i, 4, 5), 1.0f);
    const int cyc = (int)(t_ * 2.2f + hashf(i, 4, 5));
    const float rx = wrap(hashf(i + cyc * 131, 5, 5) * 900 - cam_.x, (float)Pix::W), ry = wrap(hashf(i + cyc * 131, 6, 5) * 600 - cam_.y, (float)Pix::H);
    const float r = 1 + ph * 3;
    P.rect(rx - r, ry, r * 2, 1, Color(0.80f, 0.88f, 1.0f, 0.35f * (1 - ph)));
  }
  // steam rising off warm ground after a downpour (monsoon lands)
  if (W.pour > 0.05f || (W.fog > 0.3f && W.rain + W.pour > 0.05f)) {
    for (int i = 0; i < 5; i++) {
      const float life = std::fmod(t_ * 0.12f + i * 0.2f, 1.0f);
      const float x = wrap(hashf(i, 9, 3) * 900 - cam_.x * 0.8f, Pix::W + 120.0f) - 60;
      const float y = Pix::H * 0.95f - life * Pix::H * 0.6f;
      P.blitEx(light_, 0, 0, 64, 64, x, y, 90, 50, false, Color(0.9f, 0.92f, 0.95f, 0.10f * std::sin(life * 3.14159f) * std::max(W.pour, 0.5f)));
    }
  }
  // snow, and the blizzard's driving snow and white-out
  const float snowN = (110 * W.snow + 230 * W.blizz) * sw;
  for (int i = 0; i < (int)snowN; i++) {
    const bool drive = i < 230 * W.blizz * sw;
    const float sp = (drive ? 60 : 14) + hashf(i, 3, 7) * (drive ? 50 : 18);
    const float ww = Pix::W + 20.0f, wh = Pix::H + 20.0f;
    float sx = wrap(hashf(i, 0, 7) * 600 + std::sin(t_ * 0.8f + i) * (drive ? 4 : 10) - cam_.x + t_ * (drive ? 160 : 6), ww) - 10;
    float sy = wrap(hashf(i, 1, 7) * 400 + t_ * sp - cam_.y, wh) - 10;
    const float s = hashf(i, 2, 7) < 0.3f ? 2.f : 1.f;
    P.rect(sx + 1, sy + 1, drive ? s + 1 : s, s, Color(0.42f, 0.50f, 0.68f, 0.35f));   // a soft shadow: flakes read over snow too
    P.rect(sx, sy, drive ? s + 1 : s, s, Color(1, 1, 1, drive ? 0.75f : 0.9f));
  }
  if (W.blizz > 0.02f) {
    P.rect(0, 0, (float)Pix::W, (float)Pix::H, Color(0.92f, 0.95f, 1.0f, 0.38f * W.blizz));
    for (int i = 0; i < 4; i++) {   // gusts: brighter sheets of snow sweeping past
      const float x = wrap(t_ * 220 + i * 260 - cam_.x, Pix::W + 300.0f) - 200;
      P.blitEx(light_, 0, 0, 64, 64, x, Pix::H * (0.15f + 0.22f * i), 320, 70, false, Color(1, 1, 1, 0.16f * W.blizz));
    }
  }
  // the sandstorm: streaks of blown sand racing past, and grit
  if (W.sand > 0.02f) {
    const int n = (int)(80 * W.sand * sw);
    for (int i = 0; i < n; i++) {
      const float ww = Pix::W + 60.0f;
      const float sx = wrap(hashf(i, 0, 11) * 900 + t_ * (240 + hashf(i, 2, 11) * 160) - cam_.x, ww) - 30;
      const float sy = wrap(hashf(i, 1, 11) * 500 + std::sin(t_ * 2 + i) * 4 - cam_.y * 0.5f, (float)Pix::H);
      const float len = 4 + hashf(i, 3, 11) * 12;
      P.rect(sx, sy, len, 1, Color(0.95f, 0.82f, 0.58f, 0.35f * W.sand));
    }
    for (int i = 0; i < 3; i++) {   // the dust's billows
      const float x = wrap(t_ * 140 + i * 300 - cam_.x * 0.6f, Pix::W + 300.0f) - 200;
      P.blitEx(light_, 0, 0, 64, 64, x, Pix::H * (0.2f + 0.3f * i), 340, 110, false, Color(0.86f, 0.70f, 0.46f, 0.18f * W.sand));
    }
  }
  // ashfall: grey flakes drifting down, embers glowing on the way up
  if (W.ash > 0.02f) {
    const int n = (int)(90 * W.ash * sw);
    for (int i = 0; i < n; i++) {
      const float ww = Pix::W + 20.0f, wh = Pix::H + 20.0f;
      const float sx = wrap(hashf(i, 0, 13) * 600 + std::sin(t_ * 0.6f + i) * 12 - cam_.x + t_ * 10, ww) - 10;
      const float sy = wrap(hashf(i, 1, 13) * 400 + t_ * (10 + hashf(i, 2, 13) * 10) - cam_.y, wh) - 10;
      const float g0 = 0.45f + hashf(i, 4, 13) * 0.3f;
      P.rect(sx, sy, hashf(i, 5, 13) < 0.25f ? 2.f : 1.f, 1, Color(g0, g0 * 0.95f, g0 * 0.95f, 0.75f));
    }
    for (int i = 0; i < (int)(26 * W.ash * sw); i++) {
      const float sx = wrap(hashf(i, 6, 13) * 700 + std::sin(t_ * 1.3f + i) * 8 - cam_.x, Pix::W + 20.0f) - 10;
      const float sy = wrap(hashf(i, 7, 13) * 400 - t_ * (18 + hashf(i, 8, 13) * 14) - cam_.y, Pix::H + 20.0f) - 10;
      const float fl = 0.5f + 0.5f * std::sin(t_ * 9 + i * 2.3f);
      P.rectAdd(sx, sy, 1, 1, Color(1.0f, 0.55f + 0.3f * fl, 0.2f, 0.5f + 0.5f * fl));
    }
  }
  // the eerie lands: slow motes and glints in the land's colour
  if (W.eerie > 0.02f) {
    const int n = (int)(40 * W.eerie * sw);
    for (int i = 0; i < n; i++) {
      const float sx = wrap(hashf(i, 0, 17) * 700 + std::sin(t_ * 0.5f + i) * 14 - cam_.x * 0.9f, Pix::W + 20.0f) - 10;
      const float sy = wrap(hashf(i, 1, 17) * 400 - t_ * (4 + hashf(i, 2, 17) * 6) - cam_.y * 0.9f, Pix::H + 20.0f) - 10;
      const float pulse = 0.5f + 0.5f * std::sin(t_ * (1.5f + hashf(i, 3, 17) * 2) + i);
      P.rectAdd(sx, sy, 1, 1, Color(W.er, W.eg, W.eb, 0.25f + 0.55f * pulse));
      if (pulse > 0.93f) { P.rectAdd(sx - 1, sy, 3, 1, Color(W.er, W.eg, W.eb, 0.3f)); P.rectAdd(sx, sy - 1, 1, 3, Color(W.er, W.eg, W.eb, 0.3f)); }
    }
  }
  // falling leaves / fireflies / petals / embers are world particles drawn earlier; light vignette always (heavier in
  // a storm)
  P.blitEx(vignette_, 0, 0, vignette_.w, vignette_.h, 0, 0, (float)Pix::W, (float)Pix::H, false,
           Color(1, 1, 1, std::min(1.0f, 0.45f + 0.3f * (W.blizz + W.sand + W.pour))));
}

// (M5, VISION_PLAN 10.3) The sound of a settlement's life, decided once a frame next to the music director:
//   - inside a gathering place (the census's gathering / second gathering / inn, or any inn) while a bard performs
//     there (an actor in the Lute / Drum / Flute posture, or the census's bard performing in it this hour): the bard's
//     Music::Tavern in the culture's style (the festival's dance on a festival day), with the room's crowd murmur by
//     how many are in (the actors present, else the census's occupancy);
//   - outdoors at night within 13 tiles of the gathering place the bard plays in: the same piece heard through the
//     walls, quieter and more muffled with distance (Audio::setMusicLevel), and the murmur under it;
//   - outdoors in a settlement on its festival day (9:00 .. 1:00): the festival's music on the plaza, open, with a
//     lively crowd;
//   - animals: now and then a dog, a hen, a cat or a goat near the player gives voice (the sim's barks at wolves come as
//     its own events); a toast (the Cheer posture starting) raises a cheer; through a night raid the alarm bell tolls
//     between the sim's strokes, so it rings on and on.
// Combat, a boss, caves, the title and death keep their music (the crowd and the bleed fall silent).
Music View::lifeMusic(Game& g, Music want, float dt, bool& festive) {
  festive = false;
  float gain = 1, muffle = 0, crowd = 0, lively = 0;
  const bool calm = want == Music::Town || want == Music::Night || want == Music::Wild;
  const bool playing = g.mode == Mode::Play || g.mode == Mode::Dialogue || g.mode == Mode::Menu || g.mode == Mode::Shop || g.mode == Mode::Paused;
  int site = -1, inOff = -1;
  if (playing && g.inside && g.subBldg >= 0 && g.subSite < 0 && g.subBldg < (int)g.world.over.bldgs.size()) {
    site = g.world.over.bldgs[(size_t)g.subBldg].site;
    if (site >= 0 && site < (int)g.world.sites.size()) inOff = g.subBldg - g.world.sites[(size_t)site].bldgFirst;
  } else if (playing && !g.inside && g.curSite >= 0 && g.curSite < (int)g.world.sites.size()) site = g.curSite;
  const Site* S = site >= 0 && site < (int)g.world.sites.size() && g.world.sites[(size_t)site].settlement() ? &g.world.sites[(size_t)site] : nullptr;
  const life::Census* c = S ? g.life.find(S->id) : nullptr;
  const bool fest = S && festivalHere(g, S->id);
  // the census's bard: where he performs this hour (once a second)
  bardScanT_ -= dt;
  if (bardScanT_ <= 0 || (c && bardSite_ != site)) {
    bardScanT_ = 1.0f;
    bardBldg_ = -1;
    bardSite_ = site;
    if (c)
      for (const life::Resident& r : c->res)
        if (r.job == life::Job::Bard && r.act == life::Act::Perform && r.at >= 0 && !(r.flags & (life::RF_DEAD | life::RF_AWAY))) { bardBldg_ = r.at; break; }
  }
  auto bardPosture = [](art::Posture p) { return p == art::Posture::Lute || p == art::Posture::Drum || p == art::Posture::Flute; };
  if (S && g.inside && inOff >= 0) {
    const Bldg& b = g.world.over.bldgs[(size_t)g.subBldg];
    const bool gathering = (c && (inOff == c->gathering || inOff == c->gathering2 || inOff == c->inn)) || b.type == art::Building::Inn;
    if (gathering) {
      int people = 0;
      bool bard = bardBldg_ == inOff;
      for (const Actor& a : g.actors) {
        if (!a.npc || !a.human || a.st == AState::Dead) continue;
        people++;
        if (bardPosture(a.posture)) bard = true;
      }
      const int occ = g.life.occupants(g.world, site, inOff);
      const int n = std::max(people, occ);
      crowd = clampf((float)(n - 1) / 10.0f, 0, 1) * (g.isNight() || fest ? 1.0f : 0.6f);
      lively = fest ? 1.0f : (g.isNight() ? 0.3f : 0.0f);
      if (calm && bard) { want = Music::Tavern; festive = fest; }
    }
  } else if (S && !g.inside) {
    const float h = g.hour;
    if (calm && fest && (h >= 9.0f || h < 1.0f)) {   // the festival's band on the plaza
      want = Music::Tavern;
      festive = true;
      gain = 0.9f;
      crowd = g.isNight() ? 0.45f : 0.3f;
      lively = 1.0f;
    } else if (calm && g.isNight() && bardBldg_ >= 0 && bardBldg_ < S->bldgCount) {
      const Bldg& b = g.world.over.bldgs[(size_t)(S->bldgFirst + bardBldg_)];
      const Vec2 door(b.doorX() * 16.0f + 8.0f, (b.r.y + b.r.h) * 16.0f + 4.0f);
      const float d = len(g.pl().p - door) / 16.0f, R = 13.0f;
      if (d < R) {
        const float k = 1.0f - d / R;
        want = Music::Tavern;
        festive = fest;
        gain = 0.14f + 0.36f * k * k;
        muffle = 0.88f - 0.28f * k;
        const int occ = std::max(0, g.life.occupants(g.world, site, bardBldg_));
        crowd = clampf((float)(occ - 1) / 10.0f, 0, 1) * 0.8f;
        lively = 0.3f;
      }
    }
  }
  // the bleed's level holds through the crossfade when walking away (else the bard's fading piece swells unmuffled)
  if (want == Music::Tavern && muffle > 0) { heldGain_ = gain; heldMuffle_ = muffle; levelHoldT_ = 2.2f; }
  else if (levelHoldT_ > 0) {
    levelHoldT_ -= dt;
    if (want != Music::Tavern) { gain = heldGain_ + (gain - heldGain_) * clampf(1.0f - levelHoldT_ / 2.2f, 0, 1); muffle = heldMuffle_ * clampf(levelHoldT_ / 2.2f, 0, 1); }
  }
  if (!calm && want != Music::Tavern) { crowd *= 0.3f; }
  audio_->setMusicLevel(gain, muffle);
  audio_->setCrowd(crowd, lively);
  m5Stats_.tavernLevel = want == Music::Tavern ? gain : 0.0f;
  m5Stats_.crowd = crowd;

  if (!playing || g.mode != Mode::Play) return want;
  const Vec2 pp = g.pl().p;
  // the animals' voices
  critterVoiceT_ -= dt;
  if (critterVoiceT_ <= 0) {
    Rng r((uint32_t)(t_ * 1000.0f) ^ 0xA11Eu);
    critterVoiceT_ = r.range(1.4f, 4.0f);
    const Actor* pick = nullptr;
    int seen = 0;
    for (const Actor& a : g.actors) {
      if (a.critter == 0 || a.st == AState::Dead || len2(a.p - pp) > 220.0f * 220.0f) continue;
      if (r.f() * (float)(++seen) < 1.0f) pick = &a;   // a fair pick among them
    }
    if (pick) {
      const float d = len(pick->p - pp), v = 0.55f * clampf(1.0f - d / 240.0f, 0.15f, 1.0f);
      const art::Critter k = (art::Critter)(pick->critter - 1);
      const float pv = 0.92f + 0.16f * (float)(pick->critterVar % 7u) / 6.0f;   // each animal its own voice
      switch (k) {
        case art::Critter::Dog: if (r.f() < 0.45f || g.isNight()) audio_->play(Sfx::Bark, pv, v); break;
        case art::Critter::Chicken: audio_->play(Sfx::Cluck, pv, v * 0.8f); break;
        case art::Critter::Rooster: audio_->play(Sfx::Cluck, pv * 0.82f, v); break;
        case art::Critter::Duck: audio_->play(Sfx::Cluck, pv * 0.62f, v * 0.7f); break;
        case art::Critter::Cat: if (r.f() < 0.5f) audio_->play(Sfx::Meow, pv, v * 0.8f); break;
        case art::Critter::Goat: audio_->play(Sfx::Meow, pv * 0.48f, v * 0.9f); break;
        default: break;
      }
    }
  }
  // a toast: someone raises a mug high (the Cheer posture begins)
  cheerT_ -= dt;
  for (const Actor& a : g.actors) {
    if (!a.human || !a.npc) continue;
    uint8_t& lp = lastPosture_[a.id];
    if (a.posture == art::Posture::Cheer && lp != (uint8_t)art::Posture::Cheer && cheerT_ <= 0 && len2(a.p - pp) < 260.0f * 260.0f) {
      audio_->play(Sfx::Cheer, 1.0f, 0.6f * clampf(1.0f - len(a.p - pp) / 300.0f, 0.2f, 1.0f));
      cheerT_ = 2.5f;
    }
    lp = (uint8_t)a.posture;
  }
  if (lastPosture_.size() > 512) lastPosture_.clear();
  // the alarm bell through a night raid: a second, deeper stroke between the sim's (which strikes every 4.5 s)
  if (g.alarmSite >= 0 && g.alarmSite < (int)g.world.sites.size()) {
    if (alarmWas_ != g.alarmSite) { alarmWas_ = g.alarmSite; tollT_ = 2.25f; }
    tollT_ -= dt;
    if (tollT_ <= 0) {
      tollT_ = 4.5f;
      const Site& as = g.world.sites[(size_t)g.alarmSite];
      const float pd = len(Vec2(as.r.cx() * 16.0f + 8, as.r.cy() * 16.0f + 8) - pp) / 16.0f;
      if (g.isNight() && !g.inside && pd < 50) audio_->play(Sfx::Bell, 0.89f, clampf(1.15f - pd / 40.0f, 0.25f, 0.9f));
    }
  } else alarmWas_ = -1;
  return want;
}

// the colours a settlement's festival flies: its kingdom's two (when both are strong), else a festive pair by its seed
static void festColours(const Game& g, int si, uint32_t& ca, uint32_t& cb) {
  static const uint32_t pairs[6][2] = {{0xFF2E34C4u, 0xFF54C8F0u}, {0xFF38A0D6u, 0xFF3C2EB4u}, {0xFF488438u, 0xFF46C4ECu},
                                       {0xFF783CAAu, 0xFF78D6F0u}, {0xFF2878D6u, 0xFFAA7846u}, {0xFF282896u, 0xFFD7E6E6u}};
  const Site& S = g.world.sites[(size_t)si];
  const uint32_t* pr = pairs[hash32((uint32_t)S.seed ^ 0xFE57u) % 6u];
  ca = pr[0]; cb = pr[1];
  if (const Kingdom* k = g.world.kingdomOf(si)) {
    auto sat = [](uint32_t c) { const int r = (int)(c & 255), gg = (int)((c >> 8) & 255), b = (int)((c >> 16) & 255);
                                return std::max(r, std::max(gg, b)) - std::min(r, std::min(gg, b)); };
    if (k->color && k->color2 && sat(k->color) > 60) { ca = k->color | 0xFF000000u; cb = k->color2 | 0xFF000000u; }
  }
}

// (M5) The festival's strings across the plaza: in each festive settlement near the view, rows of open paving (Plaza,
// else Road) through its middle, at least three rows apart, each crossed by a string of pennants between two poles set
// on the run's end tiles (free of props and buildings). Planned once per settlement and window; the poles join the
// y-sorted scene, the strings and their night lanterns hang overhead (drawFestival).
void View::festivalPlan(Game& g, const Map& m, Vec2 cam) {
  if (g.inside || m.kind != MapKind::Overworld || g.mode == Mode::Title) { festSpans_.clear(); festPlanKey_ = 0; return; }
  // the festive settlements whose area reaches the view
  uint64_t key = (uint64_t)g.world.ox * 0x9E3779B97F4A7C15ull ^ (uint64_t)g.world.oy * 0xC2B2AE3D27D4EB4Full ^ (uint64_t)m.bldgs.size();
  static std::vector<int> fest;
  fest.clear();
  for (int si : g.world.nearSites) {
    if (si < 0 || si >= (int)g.world.sites.size()) continue;
    const Site& S = g.world.sites[(size_t)si];
    if (!S.settlement() || !festivalHere(g, S.id)) continue;
    if ((S.r.x + S.r.w) * 16 < cam.x - 200 || S.r.x * 16 > cam.x + Pix::W + 200 || (S.r.y + S.r.h) * 16 < cam.y - 200 || S.r.y * 16 > cam.y + Pix::H + 200) continue;
    fest.push_back(si);
    key = ew::mix64(key ^ (uint64_t)S.id);
  }
  if (fest.empty()) { festSpans_.clear(); festPlanKey_ = 0; return; }
  if (key == festPlanKey_) return;
  festPlanKey_ = key;
  festSpans_.clear();
  auto open = [&](int x, int y) {
    if (!m.in(x, y) || m.propAt(x, y) || m.bldgAt[(size_t)y * m.w + x] >= 0) return false;
    if (!m.wall.empty() && m.wall[(size_t)y * m.w + x]) return false;
    const Ground gr = m.at(x, y);
    return gr == Ground::Plaza || gr == Ground::Road;
  };
  auto clearAbove = [&](int x, int y) {   // nothing tall stands right behind a pole's top (a house's front, a wall)
    for (int k = 1; k <= 2; k++) if (m.in(x, y - k) && m.bldgAt[(size_t)(y - k) * m.w + x] >= 0) return false;
    return true;
  };
  for (int si : fest) {
    const Site& S = g.world.sites[(size_t)si];
    uint32_t ca, cb;
    festColours(g, si, ca, cb);
    const int cx = S.r.x + S.r.w / 2, cy = S.r.y + S.r.h / 2;
    const int rad = std::min(18, std::max(6, std::max(S.r.w, S.r.h) / 3));
    // candidate rows near the centre, best (longest plaza run through the centre column band) first
    struct Row { int y, x0, x1, score; };
    std::vector<Row> rows;
    for (int y = cy - rad; y <= cy + rad; y++) {
      int best0 = 0, best1 = -1;
      for (int x = cx - rad; x <= cx + rad;) {
        if (!(m.in(x, y) && (m.at(x, y) == Ground::Plaza || m.at(x, y) == Ground::Road) && m.bldgAt[(size_t)y * m.w + x] < 0)) { x++; continue; }
        int x1 = x;
        while (x1 + 1 <= cx + rad + 6 && m.in(x1 + 1, y) && (m.at(x1 + 1, y) == Ground::Plaza || m.at(x1 + 1, y) == Ground::Road) && m.bldgAt[(size_t)y * m.w + x1 + 1] < 0) x1++;
        if (x1 - x > best1 - best0) { best0 = x; best1 = x1; }
        x = x1 + 1;
      }
      if (best1 < 0) continue;
      // the poles stand one tile in from the run's ends, on open tiles
      int a = best0 + 1, b = best1 - 1;
      while (a < b && !(open(a, y) && clearAbove(a, y))) a++;
      while (b > a && !(open(b, y) && clearAbove(b, y))) b--;
      if (b - a < 5) continue;
      if (b - a > 16) { const int mid = (a + b) / 2; a = mid - 8; b = mid + 8; while (a < b && !open(a, y)) a++; while (b > a && !open(b, y)) b--; if (b - a < 5) continue; }
      // a roof standing just south of the string would rise in front of it (the string hangs ~2 tiles up)
      bool roofed = false;
      for (int yy = y + 1; yy <= y + 4 && !roofed; yy++)
        for (int x = a; x <= b && !roofed; x++) if (m.in(x, yy) && m.bldgAt[(size_t)yy * m.w + x] >= 0) roofed = true;
      if (roofed) continue;
      int plaza = 0;
      for (int x = a; x <= b; x++) plaza += m.at(x, y) == Ground::Plaza ? 2 : 1;
      rows.push_back({y, a, b, plaza * 4 - std::abs(y - cy) * 3});
    }
    std::sort(rows.begin(), rows.end(), [](const Row& p, const Row& q) { return p.score > q.score; });
    std::vector<int> taken;
    for (const Row& r : rows) {
      bool near = false;
      for (int ty : taken) if (std::abs(ty - r.y) < 4) near = true;
      if (near) continue;
      taken.push_back(r.y);
      FestSpan sp;
      sp.x0 = r.x0; sp.x1 = r.x1; sp.y = r.y; sp.ca = ca; sp.cb = cb; sp.seed = (uint32_t)S.seed + (uint32_t)r.y * 7919u;
      festSpans_.push_back(sp);
      if (taken.size() >= 3) break;
    }
  }
}

// a festival pole: a turned post with a pennant at its top (lit from the top-left), standing on the tile's foot
void View::drawFestPole(const FestSpan& s, bool right, Vec2 cam) {
  Pix& P = *pix_;
  const int tx = right ? s.x1 : s.x0;
  const float bx = std::floor(tx * 16.0f + 8.0f - cam.x), by = std::floor(s.y * 16.0f + 13.0f - cam.y);
  const Color hi(0.72f, 0.54f, 0.34f), mid(0.52f, 0.36f, 0.22f), lo(0.33f, 0.22f, 0.14f), ink(0.16f, 0.10f, 0.08f);
  P.blitEx(shadow_, 0, 0, shadow_.w, shadow_.h, bx - 5, by - 2, 10, 4, false, Color(1, 1, 1, 0.8f));
  P.rect(bx - 2, by - 33, 4, 34, ink);                 // the outline
  P.rect(bx - 1, by - 32, 1, 32, hi);                   // the lit side
  P.rect(bx, by - 32, 1, 32, lo);                       // the shaded side
  P.rect(bx - 3, by - 1, 6, 2, ink);                    // the foot (a peg block)
  P.rect(bx - 2, by - 1, 4, 1, mid);
  P.rect(bx - 2, by - 36, 3, 3, ink);                   // the finial
  P.rect(bx - 1, by - 35, 1, 1, Color(0.95f, 0.80f, 0.40f));
  // the pennant, flying east from the west pole and west from the east one
  const Color fc((s.ca & 255) / 255.0f, ((s.ca >> 8) & 255) / 255.0f, ((s.ca >> 16) & 255) / 255.0f);
  const int dir = right ? -1 : 1;
  for (int k = 0; k < 6; k++) {
    const int hgt = 5 - k * 5 / 6;
    const float x = bx + (dir > 0 ? 1 + k : -2 - k);
    P.rect(x, by - 32 + (k / 2 == 1 ? 1 : 0), 1, (float)std::max(1, hgt), k == 0 ? Color(std::min(1.0f, fc.r + 0.25f), std::min(1.0f, fc.g + 0.25f), std::min(1.0f, fc.b + 0.25f)) : fc);
  }
}

// (M5, 15.12 content towns) A festival day dresses the settlement: strings of pennants across the plaza between their
// poles (festivalPlan) and between the eaves of neighbouring houses whose eaves line up (east-west across a lane), in
// the kingdom's colours (else a festive pair by the settlement); at night paper lanterns hang along the strings and one
// by every door, on the side its windows leave free (their glow: festLights_, added by the light pass). Drawn over the
// scene: the strings hang overhead, the lanterns on the walls.
void View::drawFestival(Game& g, const Map& m, Vec2 cam) {
  festLights_.clear();
  if (g.inside) return;
  Pix& P = *pix_;
  const float dark = clampf((0.62f - g.daylight()) / 0.45f, 0, 1);
  auto buntingTex = [&](int w, uint32_t ca, uint32_t cb, uint32_t seed) -> const Tex& {
    const uint64_t key = (uint64_t)w << 40 ^ (uint64_t)(ca & 0xFFFFFF) << 16 ^ (uint64_t)(cb & 0xFFFFFF) ^ (uint64_t)(seed & 1u) << 62;
    auto it = festTex_.find(key);
    return it != festTex_.end() ? it->second : (festTex_[key] = pix_->bake(art::festivalBunting(w, ca, cb, seed)));
  };
  auto lanternTex = [&](uint32_t lc, uint32_t seed) -> const Tex& {
    const uint64_t key = (uint64_t)0x1A ^ ((uint64_t)(lc & 0xFFFFFF) << 8) ^ (1ull << 61) ^ ((uint64_t)(seed & 3u) << 4);
    auto it = festTex_.find(key);
    return it != festTex_.end() ? it->second : (festTex_[key] = pix_->bake(art::festivalLantern(lc, seed & 3u)));
  };
  static const uint32_t warm[3] = {0xFF3040D6u, 0xFF3C96ECu, 0xFF5AC8F0u};   // red, orange and gold paper
  // a string from (ax, ay) to (bx, by) (map pixels): the bunting stepped down the slope in 2 px strips, and at night a
  // lantern hung under the cord every ~34 px
  auto string = [&](float ax, float ay, float bx, float by, uint32_t ca, uint32_t cb, uint32_t seed) {
    const int w = (int)std::lround(bx - ax);
    if (w < 16) return;
    const Tex& t = buntingTex(w, ca, cb, seed);
    const float x0 = std::floor(ax - cam.x), y0 = ay - cam.y, dy = by - ay;
    if (x0 > Pix::W + 8 || x0 + t.w < -8 || std::max(y0, y0 + dy) < -20 || std::min(y0, y0 + dy) > Pix::H + 20) return;
    for (int x = 0; x < t.w; x += 2) {
      const float yy = std::floor(y0 + dy * ((float)x / (float)std::max(1, t.w - 1)));
      P.blitRegion(t, x, 0, std::min(2, t.w - x), t.h, x0 + (float)x, yy);
    }
    m5Count_.bunting++;
    if (dark > 0.05f) {
      const int n = std::max(1, (w + 8) / 34);
      for (int k = 1; k <= n; k++) {
        const float f = (float)k / (float)(n + 1);
        const float sag = 3.0f * 4.0f * f * (1.0f - f) + 1.0f;   // the cord's own sag (art::festivalBunting)
        const float lx = ax + f * (bx - ax), ly = ay + f * (by - ay) + sag + 1.0f;
        const Tex& lt = lanternTex(warm[(seed + (uint32_t)k) % 3u], seed + (uint32_t)k);
        P.blit(lt, std::floor(lx - lt.w / 2.0f - cam.x), std::floor(ly - cam.y));
        festLights_.push_back(Vec2(lx, ly + lt.h * 0.6f));
        m5Count_.festLanterns++;
      }
    }
  };
  // ---- the plaza's strings (poles are drawn in the scene)
  for (const FestSpan& s : festSpans_) string(s.x0 * 16.0f + 9.0f, s.y * 16.0f + 13.0f - 31.0f, s.x1 * 16.0f + 7.0f, s.y * 16.0f + 13.0f - 31.0f, s.ca, s.cb, s.seed);
  // ---- the houses: strings between eaves that line up, a lantern by each door at night
  static std::vector<int> vis;
  bldgsIn(g, m, cam.x - 180, cam.y - 80, cam.x + Pix::W + 180, cam.y + Pix::H + 140, vis);
  struct SiteF { int site; bool fest; uint32_t ca, cb, seed; };
  static std::vector<SiteF> sites;
  sites.clear();
  auto festOf = [&](int si) -> const SiteF* {
    for (const SiteF& s : sites) if (s.site == si) return s.fest ? &s : nullptr;
    SiteF f{si, false, 0, 0, 0};
    if (si >= 0 && si < (int)g.world.sites.size() && g.world.sites[(size_t)si].settlement()) {
      const Site& S = g.world.sites[(size_t)si];
      f.fest = festivalHere(g, S.id);
      f.seed = (uint32_t)S.seed;
      festColours(g, si, f.ca, f.cb);
    }
    sites.push_back(f);
    return f.fest ? &sites.back() : nullptr;
  };
  // the eave line of a building's front (map pixels): over the front wall, higher with each storey
  auto eaveY = [](const Bldg& b) { return (b.r.y + b.r.h) * 16.0f - 24.0f - 15.0f * (float)std::max(0, (int)b.storeys - 1); };
  for (int bi : vis) {
    const Bldg& a = m.bldgs[(size_t)bi];
    if (a.charred >= 2) continue;
    const SiteF* F = festOf(a.site);
    if (!F) continue;
    int best = -1, bestGap = 99;
    for (int bj : vis) {
      if (bj == bi) continue;
      const Bldg& b = m.bldgs[(size_t)bj];
      if (b.site != a.site || b.charred >= 2 || b.r.y + b.r.h != a.r.y + a.r.h || b.storeys != a.storeys) continue;
      const int gap = b.r.x - (a.r.x + a.r.w);
      if (gap < 2 || gap > 8 || gap >= bestGap) continue;
      best = bj; bestGap = gap;
    }
    if (best >= 0) {
      const Bldg& b = m.bldgs[(size_t)best];
      const float ax = (a.r.x + a.r.w) * 16.0f + 2, bx = b.r.x * 16.0f - 2, ay = eaveY(a);
      bool blocked = false;   // a house standing in the lane between them, or taller in front of the string
      for (int bk : vis) {
        if (bk == bi || bk == best) continue;
        const Bldg& c = m.bldgs[(size_t)bk];
        if ((c.r.x + c.r.w) * 16.0f < ax || c.r.x * 16.0f > bx) continue;
        const float cBottom = (c.r.y + c.r.h) * 16.0f;
        if (cBottom > ay - 8 && c.r.y * 16.0f - 40.0f < ay + 12) { blocked = true; break; }
      }
      if (!blocked) string(ax, ay, bx, ay, F->ca, F->cb, F->seed + (uint32_t)bi);
    }
    // the door's lantern, on the side of the door its windows leave free
    if (dark > 0.05f && !a.open.open) {
      const float doorCx = a.doorX() * 16.0f + 8.0f, hy = (a.r.y + a.r.h) * 16.0f - 27.0f;
      auto tex = bldgTex_.find(bldgKey(m, a, bi));
      auto win = bldgWin_.find(bldgKey(m, a, bi));
      float side = 0;
      for (int sd : {1, -1}) {
        const float lx = doorCx + sd * 11.0f;
        bool clear = lx > a.r.x * 16.0f + 3 && lx < (a.r.x + a.r.w) * 16.0f - 3;
        if (clear && tex != bldgTex_.end() && win != bldgWin_.end()) {
          const float top = (a.r.y + a.r.h) * 16.0f + art::BLDG_PAD_B - tex->second.h;
          for (const Vec2& wc : win->second) {
            const Vec2 wp(a.r.x * 16.0f - art::BLDG_PAD_X + wc.x, top + wc.y);
            if (std::fabs(wp.x - lx) < 9 && wp.y > hy - 8 && wp.y < hy + 16) clear = false;
          }
        }
        if (clear) { side = (float)sd; break; }
      }
      if (side == 0) continue;
      const float lx = doorCx + side * 11.0f;
      if (lx < cam.x - 16 || lx > cam.x + Pix::W + 16 || hy < cam.y - 20 || hy > cam.y + Pix::H + 20) continue;
      const Tex& t = lanternTex(warm[hash32(a.seed ^ 0x1A7E2u) % 3u], a.seed);
      const float sw = std::sin(t_ * 1.7f + (float)(a.seed & 63u)) * 0.6f;   // a slow sway on its hook
      P.rect(std::floor(lx - cam.x), std::floor(hy - 2 - cam.y), 1, 2, Color(0.22f, 0.16f, 0.12f));   // the hook
      P.blit(t, std::floor(lx - t.w / 2.0f + sw - cam.x + 0.5f), std::floor(hy - cam.y));
      festLights_.push_back(Vec2(lx, hy + t.h * 0.55f));
      m5Count_.festLanterns++;
    }
  }
}

// (M3c LIFE) test scripts: force the weather (screenshots of every Sky kind)
namespace {
bool cmdWeather(ScriptCtx& c) {
  static const char* names[] = {"temperate", "showery", "misty", "dry", "arid", "snowy", "blizzard", "monsoon", "ashfall", "eerie"};
  static_assert(sizeof(names) / sizeof(names[0]) == (size_t)Sky::COUNT, "a name for every Sky");
  const std::string w = c.arg(1);
  if (w == "auto" || w.empty()) { c.view.scriptSky(-1); return true; }
  for (int i = 0; i < (int)Sky::COUNT; i++)
    if (w == names[i]) { c.view.scriptSky(i); std::printf("weather: %s\n", names[i]); return true; }
  c.fail("weather: unknown kind " + w);
  return true;
}
}  // namespace
EMB_SCRIPT_CMD("weather", "weather temperate|showery|misty|dry|arid|snowy|blizzard|monsoon|ashfall|eerie|auto: force the weather (full strength)", cmdWeather);
