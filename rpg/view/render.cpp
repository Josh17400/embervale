// World rendering: sprite bank, y-sorted scene, effects, lighting, weather, event handling.
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <string>
#include <unordered_map>
#include <vector>
#include <SDL3/SDL.h>
#include "rpg/art/art_city.h"
#include "rpg/art/art_parts.h"
#include "rpg/art/doll/doll.h"
#include "rpg/art/doll/doll_envelope.h"
#include "rpg/art/art_party.h"
#include "rpg/sim/anim.h"
#include "rpg/sim/deco.h"
#include "rpg/sim/foes.h"
#include "rpg/sim/gear_look.h"
#include "rpg/sim/life.h"
#include "rpg/sim/mark_tags.h"   // markKey, the biome-seen key
#include "rpg/sim/monster_stats.h"   // heavyWindup
#include "rpg/story/story_api.h"
#include "rpg/view/prop_traits.h"
#include "rpg/view/script_api.h"
#include "rpg/view/magic_state.h"
#include "rpg/view/realm_ui.h"
#include "rpg/view/rise_view.h"
#include "rpg/view/screen.h"
#include "rpg/view/view.h"
#include "rpg/view/render_internal.h"   // beasts::, rview:: (shared with render_sorted.cpp)
#include "rpg/world/biomes.h"
#include "rpg/world/economy.h"
#include "rpg/world/ids.h"
#include "rpg/world/source.h"
#include "rpg/world/urban.h"

using art::Prop;
using art::Monster;

// ---- (M6 BEASTS) the view of ranked foes: elites, champions, named uniques, bosses and world bosses ------------------
// While the world is drawn, the monster branch queues what must show over the night's darkness (the glow sheets of
// glowing eyes, ember cracks, crystal glints and a golem's rune; the name plates; the world boss's bar); drawWeather's
// tail draws them after the lighting. `memo` is what the view remembers of each foe near the player between frames (its
// last state, for the shriek and the slam; its boss phase, for the flash; whether its sting or roar has played).
namespace beasts {   // (the types: render_internal.h)
std::vector<Glow> glows;
// (P2c ladder, contracts 6b) the living finish queued by the sorted pass: glow-plane tiles (t set) and light points (the
// spark's star and the motes: t null, a w x h block), drawn additively after the lighting
std::vector<Finish> finish;
int finishFigures = 0;
// (M6 fixer) the ranked foes and the flyers, drawn again as a faint ghost over everything once the world is down, so a
// named unique in its bamboo, a world boss under the canopy or a harpy behind a ruin's wall still shows where it is
std::vector<Glow> xray;
std::vector<Plate> plates;
std::unordered_map<int, Memo> memo;
float now = 0, stingT = -10;
bool isRank(const Actor& a, foes::Rank r) { return a.rank == (uint8_t)r; }
// the ranks that wear an aura and a name plate
bool marked(const Actor& a) {
  return isRank(a, foes::Rank::Elite) || isRank(a, foes::Rank::Champion) || isRank(a, foes::Rank::Named);
}
// the aura's colour: the first affix's (a champion pack shares its affixes, so the pack shares one colour); a named
// unique without affixes burns gold
uint32_t auraOf(const Actor& a) {
  if (!marked(a) && !isRank(a, foes::Rank::WorldBoss)) return 0;
  uint32_t c = foes::auraColor(a.affixes);
  if (!c) c = isRank(a, foes::Rank::WorldBoss) ? rgba(255, 70, 40) : isRank(a, foes::Rank::Named) ? rgba(255, 200, 70)
            : isRank(a, foes::Rank::Champion) ? rgba(255, 130, 60) : rgba(220, 220, 255);
  return c;
}
// Draw scale: always 1. Every 16-bit sprite on screen shares one pixel grid. (M6 fixer r4, review: "the 2x Scale2x
// world-boss bake breaks the pixel grid and reads as mush") the world boss used to draw at 2x from an EPX bake, which
// doubled its pixel density against the world round it and smeared its soft-shaded painting; it now draws at 1x like
// every beast, its rank told by its own aura, a heavier shadow, the ember rim at night, its name plate and its bar.
// (Boss-sized painted sheets, like the dragon's, are the art lane's follow-up.)
int drawScale(const Actor&) { return 1; }
art::MonsterLook lookOf(const Actor& a) {
  art::MonsterLook L;
  L.base = a.mon; L.overlays = a.overlays; L.tint = a.bodyTint; L.scale = a.scalePct;
  return L;
}
// the light a golem's rune casts (its material's colour)
Color runeLight(const Actor& a) {
  switch (art::golemMaterial(a.bodyTint)) {
    case 1: return Color(1.0f, 0.45f, 0.85f);
    case 2: return Color(1.0f, 0.75f, 0.3f);
    case 3: return Color(1.0f, 0.5f, 0.15f);
    default: return Color(0.4f, 0.85f, 1.0f);
  }
}
// what the last frame drew (for the scripts' `expect beasts` and the perf budget: auras + plates + glows <= 0.3 ms)
Stats cur, last;
double msSince(Clock::time_point t0) { return std::chrono::duration<double, std::milli>(Clock::now() - t0).count(); }
}  // namespace beasts

// (Witness, View::witnessRelease) the foes' view memo back to its start: actor ids repeat from run to run, so a foe of the
// last run that had roared would silence this run's
static void witnessResetBeasts() {
  beasts::glows.clear(); beasts::xray.clear(); beasts::plates.clear(); beasts::memo.clear();
  beasts::now = 0; beasts::stingT = -10;
  beasts::cur = beasts::Stats(); beasts::last = beasts::Stats();
}
EMB_VIEW_RESET(witnessResetBeasts);

// (M6 BEASTS) the last frame's ranked-foe overlays (rpg/view/script_beasts.cpp reads them)
void beastsViewStats(int& auras, int& plates, int& glows, int& bossBar, double& ms) {
  auras = beasts::last.auras; plates = beasts::last.plates; glows = beasts::last.glows; bossBar = beasts::last.bossBar; ms = beasts::last.ms;
}

// (M11 TERRAIN) View::scriptSky's value for "weather clear": no rain, snow, fog or sand at all (review shots)
constexpr int kSkyClear = 99;
namespace rview {   // (SOLID 3.4a: was anonymous; render_internal.h declares what render_sorted.cpp uses)

// (P1b ENGINE lane) a story's hand cart: rpg/art/art_handcart.cpp (P2a phase A moved the model there, pixel-identical)
Canvas paintHandcart(uint64_t key) { return art::handcartSprite(key); }

// (P1b ENGINE lane) a beacon a story lit on a watchtower's broken top: a tall fire in four flickers (a dark red skirt,
// orange, yellow, a white heart), drawn into the drum over the jagged rim; and the warm glow it throws at night
std::vector<Vec2> g_beaconFires;   // this frame's lit beacons (world px of the fire's foot): the night pass lights them
Canvas paintBeaconFlame(uint64_t key) {
  const int fr = (int)(key & 3);
  Canvas c(BF_W, BF_H);
  const uint32_t col[5] = {rgba(120, 28, 18), rgba(196, 64, 24), rgba(242, 132, 40), rgba(255, 206, 92), rgba(255, 246, 210)};
  auto n1 = [&](float x, float y) {   // a cheap smooth noise from hashed lattice points
    const int xi = (int)std::floor(x), yi = (int)std::floor(y);
    const float fx = x - xi, fy = y - yi;
    auto h = [&](int a, int b) { return (float)(hash32((uint32_t)(a * 73856093) ^ (uint32_t)(b * 19349663) ^ (uint32_t)(fr * 83492791)) % 1000u) / 1000.0f; };
    const float a = h(xi, yi) + (h(xi + 1, yi) - h(xi, yi)) * fx, b = h(xi, yi + 1) + (h(xi + 1, yi + 1) - h(xi, yi + 1)) * fx;
    return a + (b - a) * fy;
  };
  const float cx = BF_W * 0.5f - 0.5f, base = BF_H - 2.0f;
  for (int y = 0; y < BF_H; y++)
    for (int x = 0; x < BF_W; x++) {
      const float up = (base - y) / (BF_H - 3.0f);            // 0 at the fire's foot, 1 at its tip
      if (up < -0.05f) continue;
      const float sway = std::sin(up * 4.2f + fr * 1.57f) * 1.6f * up;
      const float wdt = (1.0f - up) * 8.5f + 1.2f;            // the tongue narrows as it rises
      const float dx = std::fabs(x - cx - sway) / wdt;
      const float lick = n1(x * 0.45f, y * 0.35f + fr * 2.0f) * 0.55f;
      const float v = 1.0f - dx - up * 0.55f + lick - 0.25f;
      if (v <= 0.0f) continue;
      const int k = v > 0.95f ? 4 : v > 0.7f ? 3 : v > 0.42f ? 2 : v > 0.18f ? 1 : 0;
      c.set(x, y, col[k]);
    }
  // the burning wood at its foot
  for (int x = 4; x < BF_W - 4; x++) { c.set(x, BF_H - 1, rgba(52, 30, 20)); if (x & 1) c.set(x, BF_H - 2, rgba(255, 150, 60)); }
  return c;
}

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
  if ((key >> 16) & 1) art::setMarketSnow(13.0f, (uint32_t)(key & 0xFFFFu));   // (fixer M5 r3) a snowy town's stalls carry snow on their roofs
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
  Canvas c = (key >> 9) & 1 ? art::marketCartBroken((int)(key & 15)) : art::marketCart((int)(key & 15));
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
// (P1 fixer r2) the speaker of a spoken plate (Actor::id; the player and the dead never), or null
const Actor* speakerOf(const Game& g, int id) {
  for (size_t k = 1; k < g.actors.size(); k++)
    if (g.actors[k].id == id) return &g.actors[k];
  return nullptr;
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
// (M8 ART) a ruin piece knowing its run: key low bits = variant (0..7) | RuinWall neighbours (N1 E2 S4 W8) << 4
Canvas paintRuin(uint64_t key) { return art::ruinVariantRun((Prop)((key >> 8) & 255), (int)(key & 7), (int)((key >> 4) & 15)); }
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
uint32_t interiorPropTexKey(const Game& g, const Map& m, int tx, int ty, Prop p, int frame) {
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
}  // namespace rview
using namespace rview;

// (P2a) the helpers rpg/view/render_anim.cpp's human draw shares with the sorted pass here
bool rvBathing(const Game& g, const Map& m, const Actor& a) { return bathing(g, m, a); }
art::Berth rvBerthAt(const Map& m, int tx, int ty, int& kit) { return berthAt(m, tx, ty, kit); }
Canvas rvFloorCushion(uint64_t key) { return paintFloorCushion(key); }
void rvPushGlow(const Tex* t, int sx, int sw, int sh, float x, float y, bool flip, float a) { beasts::glows.push_back({t, sx, sw, sh, x, y, flip, a}); }

bool View::init(Pix& pix, Audio& audio) {
  pix_ = &pix;
  audio_ = &audio;
  for (int i = 0; i < (int)Prop::COUNT; i++) sprites_.props_.push_back(pix.bake(art::propSprite((Prop)i)));
  for (int i = 0; i < (int)Monster::COUNT; i++) sprites_.monsters_.push_back(pix.bake(art::monsterSheet((Monster)i)));
  for (int i = 0; i < (int)art::Fx::COUNT; i++) sprites_.fx_.push_back(pix.bake(art::fxSprite((art::Fx)i)));
  sprites_.gateTex_ = pix.bake(art::gateHouse(7));
  sprites_.gate_ = sprites_.gateTex_;
  {
    Canvas c(14, 6);
    for (int y = 0; y < 6; y++)
      for (int x = 0; x < 14; x++) {
        float dx = (x - 6.5f) / 7.0f, dy = (y - 2.5f) / 3.0f;
        float d = dx * dx + dy * dy;
        if (d < 1) c.set(x, y, rgba(20, 16, 30, (int)(110 * (1 - d * 0.5f))));
      }
    sprites_.shadow_ = pix.bake(c);
    Canvas b(40, 12);
    for (int y = 0; y < 12; y++)
      for (int x = 0; x < 40; x++) {
        float dx = (x - 19.5f) / 20.0f, dy = (y - 5.5f) / 6.0f;
        float d = dx * dx + dy * dy;
        if (d < 1) b.set(x, y, rgba(20, 16, 30, (int)(100 * (1 - d * 0.6f))));
      }
    sprites_.shadowBig_ = pix.bake(b);
  }
  {
    Canvas l(64, 64);
    for (int y = 0; y < 64; y++)
      for (int x = 0; x < 64; x++) {
        float d = std::hypot(x - 31.5f, y - 31.5f) / 32.0f;
        float a = d < 1 ? (1 - d) * (1 - d) : 0;
        l.set(x, y, rgba(255, 255, 255, (int)(255 * a)));
      }
    sprites_.light_ = pix.bake(l);
    Canvas w(64, 64);
    for (int y = 0; y < 64; y++)
      for (int x = 0; x < 64; x++) {
        float n = vnoise(x / 6.0f, y / 3.0f, 777);
        // tileable-ish: fade at edges handled by the scroll; sparse highlight streaks
        if (n > 0.70f && (y % 3) == 0) w.set(x, y, rgba(170, 215, 245, (int)((n - 0.7f) * 600)));
      }
    sprites_.water_ = pix.bake(w);
    Canvas v(120, 68);
    for (int y = 0; y < 68; y++)
      for (int x = 0; x < 120; x++) {
        float dx = (x - 59.5f) / 60.0f, dy = (y - 33.5f) / 34.0f;
        float d = std::sqrt(dx * dx + dy * dy);
        float a = smooth01(clampf((d - 0.65f) / 0.5f, 0, 1));
        v.set(x, y, rgba(8, 6, 16, (int)(150 * a)));
      }
    sprites_.vignette_ = pix.bake(v);
    Canvas wt(1, 1); wt.set(0, 0, rgba(255, 255, 255));
    sprites_.white_ = pix.bake(wt);
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

const Tex& SpriteCache::humanTex(View& v, const art::HumanLook& L, bool child) {
  uint64_t k = L.key() ^ (child ? 0xC41D0000C41Dull : 0);
  humanUsed_[k] = v.t_;
  auto it = humans_.find(k);
  if (it != humans_.end()) return it->second;
  const auto t0 = std::chrono::steady_clock::now();
  Canvas sheet = art::humanSheet(L);
  if (child) childBody(sheet, true);
  const Tex& t = humans_[k] = v.pix_->bake(sheet);
  liftMaskFor(v, sheet, t);   // (P2a LENS) the night lift's interior mask
  if (std::getenv("EMB_TIMING")) {
    static double total = 0;
    static int n = 0;
    const double one = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    total += one;
    std::printf("character sheet painted: %d so far, %.1f ms total, %.3f ms this one\n", ++n, total, one);   // (P2a: per sheet, the p50 / p95 baseline)
  }
  if (v.witness && !child) {   // (P2a, Witness state.player.sprite.box) each cell's opaque box
    std::vector<SpriteCache::CellBox>& bx = humanBoxes_[L.key()];
    bx.assign((size_t)art::HUMAN_FRAMES * 3, SpriteCache::CellBox());
    for (int row = 0; row < 3; row++)
      for (int f = 0; f < art::HUMAN_FRAMES; f++) {
        SpriteCache::CellBox& b = bx[(size_t)(row * art::HUMAN_FRAMES + f)];
        int x0 = 99, y0 = 99, x1 = -1, y1 = -1;
        for (int y = 0; y < art::HUMAN_H; y++)
          for (int x = 0; x < art::HUMAN_W; x++)
            if (sheet.get(f * art::HUMAN_W + x, row * art::HUMAN_H + y) >> 24) { x0 = std::min(x0, x); y0 = std::min(y0, y); x1 = std::max(x1, x); y1 = std::max(y1, y); }
        if (x1 >= 0) { b.x0 = (int8_t)x0; b.y0 = (int8_t)y0; b.x1 = (int8_t)x1; b.y1 = (int8_t)y1; }
      }
  }
  return t;
}

// ---- P2a phase A (VISION_PLAN 15.36 items 3 and 4): the projection and the v2 atlases. LENS owns these from here (the
//      world layer, the MB caps on the people caches, the bake queue for folk entering the stream radius).
Vec2 Camera::worldCam(const View& v) const { return Vec2(std::floor(cam_.x + v.fx_.shakeOff_.x), std::floor(cam_.y + v.fx_.shakeOff_.y)); }
Vec2 Camera::toUi(const View& v, Vec2 w) const {
  if (!v.pix_) return w;
  const Pix::Layer& wl = v.pix_->worldLayer();
  const Pix::Layer& ul = v.pix_->uiLayer();
  const Vec2 c = worldCam(v);
  const float dx = wl.offX + (w.x - c.x) * wl.scale, dy = wl.offY + (w.y - c.y) * wl.scale;   // device px
  return Vec2((dx - ul.offX) / ul.scale, (dy - ul.offY) / ul.scale);
}
Vec2 Camera::toWorld(const View& v, Vec2 u) const {
  if (!v.pix_) return u;
  const Pix::Layer& wl = v.pix_->worldLayer();
  const Pix::Layer& ul = v.pix_->uiLayer();
  const Vec2 c = worldCam(v);
  const float dx = ul.offX + u.x * ul.scale, dy = ul.offY + u.y * ul.scale;
  return Vec2(c.x + (dx - wl.offX) / wl.scale, c.y + (dy - wl.offY) / wl.scale);
}
float Camera::uiPerWorld(const View& v) const {
  if (!v.pix_) return 1;
  return v.pix_->worldLayer().scale / std::max(1e-3f, v.pix_->uiLayer().scale);
}

// ---- P2a LENS: the world view, ZOOM, the HUD's bands seen from the world, the ring, the night lift, the pips
namespace {
// the night lift's strength: the Moonwash copy of a figure is added at kLiftK x the darkness (amb 0.12 / 0.15 / 0.30 at
// midnight: darkness 0.81)
// (measured, p2a_night.txt: a figure's median is held down by its ink outline, hair and trousers, about 50 luma under
// the meadow by day; at 1.0 the hero stands +12 over the meadow at 23:00 with the outline still 32 under the ground)
const float kLiftK = std::getenv("EMB_LIFTK") ? (float)std::atof(std::getenv("EMB_LIFTK")) : 0.5f;
// the flat Moonwash glaze over the figure's interior, x the darkness
const float kLiftFlat = std::getenv("EMB_LIFTF") ? (float)std::atof(std::getenv("EMB_LIFTF")) : 0.25f;
// the ring's front arc added again after the light map, x its alpha x the darkness
const float kRingNightK = std::getenv("EMB_RINGK") ? (float)std::atof(std::getenv("EMB_RINGK")) : 0.3f;

// (P2a slice, SLICE-WALK-JITTER) the camera rounded once a frame, in View::update: cam_ then holds the whole-pixel camera
// that every draw, worldCam() and toUi read, and the eased camera's dropped fraction waits in s_camRes for the next
// frame's easing. An axis that moves with the hero is rounded in the hero's own phase: his feet are drawn at round(p) -
// cam, so that axis takes round(p) - round(p - c) and his place on the screen changes only when the camera's lag behind
// him changes (it eases), never because the hero and the camera round on different frames (round 2: pivot.x 717 / 720
// on 56 of 119 frames of a steady walk). A resting axis (a fixed room, the map's edge, the camera settled) is floored
// alone, as before, so the world never shimmers while the camera is still.
Vec2 s_camRes, s_camPrev;
bool s_camPrevOk = false, s_camFollowX = false, s_camFollowY = false;
// (test hook) EMB_CAMSNAP=0: the pre-fix camera (the eased camera floored at each draw), for before / after comparisons
const bool kCamSnap = !(std::getenv("EMB_CAMSNAP") && std::atoi(std::getenv("EMB_CAMSNAP")) == 0);
Vec2 rvSnapCamera(Vec2 c, Vec2 hero, bool withHero, float dt) {
  Vec2 s(std::floor(c.x), std::floor(c.y));
  auto axis = [&](float cv, float pv, float prev, bool& follow, float& out) {
    const float speed = s_camPrevOk && dt > 0 ? std::fabs(cv - prev) / dt : 0.0f;   // world px a second
    follow = withHero && speed >= (follow ? 8.0f : 15.0f);
    if (follow) out = std::floor(pv + 0.5f) - std::floor(pv - cv + 0.5f);
  };
  axis(c.x, hero.x, s_camPrev.x, s_camFollowX, s.x);
  axis(c.y, hero.y, s_camPrev.y, s_camFollowY, s.y);
  s_camPrev = c;
  s_camPrevOk = true;
  return s;
}
}  // namespace
int View::viewW() const {
  if (!pix_) return Pix::W;
  const Pix::Layer& wl = pix_->worldLayer();
  return wl.w > 0 ? wl.w : Pix::W;
}
int View::viewH() const {
  if (!pix_) return Pix::H;
  const Pix::Layer& wl = pix_->worldLayer();
  return wl.h > 0 ? wl.h : Pix::H;
}
void Camera::uiInsetsToWorld(const View& v, float l, float t, float r, float b, float out[4]) const {
  out[0] = l; out[1] = t; out[2] = r; out[3] = b;
  if (!v.pix_) return;
  const Pix::Layer& wl = v.pix_->worldLayer();
  const Pix::Layer& ul = v.pix_->uiLayer();
  if (wl.w <= 0 || ul.w <= 0) return;
  const float U = ul.scale, D = wl.scale;
  out[0] = (ul.offX + l * U - wl.offX) / D;
  out[1] = (ul.offY + t * U - wl.offY) / D;
  out[2] = (float)wl.w - (ul.offX + (ul.w - r) * U - wl.offX) / D;
  out[3] = (float)wl.h - (ul.offY + (ul.h - b) * U - wl.offY) / D;
}

// Once a frame before the camera moves: a zoom (or a window resize) keeps the hero where they stand on the screen (the
// camera's centre moves so the hero's device position holds; the follow then eases the lead back in its ~0.15 s), the
// ring's flare (hit, or walked into a crowd), the v2 bake queue.
void View::lensUpdate(Game& g, float dt) {
  const int vw = viewW(), vh = viewH();
  const int d = pix_ ? std::max(1, (int)std::lround(pix_->worldLayer().scale)) : 1;
  if (camera_.lensD_ > 0 && (d != camera_.lensD_ || vw != camera_.lensViewW_ || vh != camera_.lensViewH_) && g.mode != Mode::Title) {
    const Vec2 hero = g.pl().p;
    const Vec2 oc = camera_.cam_ + Vec2(camera_.lensViewW_ * 0.5f, camera_.lensViewH_ * 0.5f);
    const Vec2 nc = hero - (hero - oc) * ((float)camera_.lensD_ / (float)d);
    camera_.cam_ = nc - Vec2(vw * 0.5f, vh * 0.5f);
  }
  camera_.lensD_ = d; camera_.lensViewW_ = vw; camera_.lensViewH_ = vh;
  // the ring flares for a second when the hero is hit or walks into a crowd (4 or more people within 3 tiles)
  if (ringFlare_ > 0) ringFlare_ = std::max(0.0f, ringFlare_ - dt);
  if (g.mode == Mode::Play) {
    const Actor& p = g.pl();
    if (ringHpWas_ >= 0 && p.hp < ringHpWas_ - 0.01f) ringFlare_ = 1.0f;
    ringHpWas_ = p.hp;
    int crowdN = 0;
    for (const Actor& a : g.actors)
      if (!a.player && a.human && a.st != AState::Dead && len2(a.p - p.p) < 48.0f * 48.0f) crowdN++;
    const bool crowd = crowdN >= 4;
    if (crowd && !ringCrowdWas_) ringFlare_ = 1.0f;
    ringCrowdWas_ = crowd;
  } else ringHpWas_ = -1;
  sprites_.rigBakeQueue(*this, g);
}

namespace {
// the hero's ring (STYLE_P2 8): a 1 px Ember Gold ellipse w x h, its front (lower) arc a step brighter, a 1 px Ring Shadow
// line (Ink at 45 %) under the front arc. key: w | h << 8 | pale << 16. On a pale ground (sand, snow, ice) the gold sits
// close to the ground's own value, so there the rim deepens to a warm amber and the Ring Shadow runs round the whole
// ellipse (outside it, a touch firmer): the ring then reads by its dark edge (p2a_night.txt: 35 luma or more on all four
// grounds). Canvas: one px of margin round the ellipse (the shadow's room); the ellipse's centre is the canvas centre.
Canvas paintRing(uint64_t key) {
  const int w = (int)(key & 255), h = (int)((key >> 8) & 255);
  const bool pale = ((key >> 16) & 1) != 0;
  Canvas c(w + 2, h + 2);
  const float rx = w * 0.5f, ry = h * 0.5f;
  auto in = [&](int x, int y) {   // ellipse coordinates (0..w-1, 0..h-1)
    if (x < 0 || y < 0 || x >= w || y >= h) return false;
    const float u = (x + 0.5f - rx) / rx, v = (y + 0.5f - ry) / ry;
    return u * u + v * v <= 1.0f;
  };
  auto rimAt = [&](int x, int y) { return in(x, y) && !(in(x - 1, y) && in(x + 1, y) && in(x, y - 1) && in(x, y + 1)); };
  const uint32_t rim = pale ? rgba(0xD8, 0x8E, 0x2A) : rgba(0xF2, 0xC1, 0x4E);
  const uint32_t front = pale ? rgba(0xF0, 0xB0, 0x44) : rgba(0xFF, 0xDE, 0x82);
  const uint32_t shade = rgba(0x1E, 0x16, 0x2A, pale ? 190 : 115);
  for (int y = 0; y < h; y++)
    for (int x = 0; x < w; x++)
      if (rimAt(x, y)) c.set(x + 1, y + 1, y * 2 >= h ? front : rim);
  for (int y = -1; y <= h; y++)
    for (int x = -1; x <= w; x++) {
      if (in(x, y)) continue;
      if (pale) {   // round the whole ellipse: every clear pixel touching the rim (4-neighbours)
        if (rimAt(x - 1, y) || rimAt(x + 1, y) || rimAt(x, y - 1) || rimAt(x, y + 1)) c.set(x + 1, y + 1, shade);
      } else if (y * 2 >= h && rimAt(x, y - 1)) {   // under the front arc only
        c.set(x + 1, y + 1, shade);
      }
    }
  if (pale)   // and inside it: the gold line inked on both sides (its dark edge then outweighs it on sand and snow)
    for (int y = 0; y < h; y++)
      for (int x = 0; x < w; x++)
        if (in(x, y) && !rimAt(x, y) && (rimAt(x - 1, y) || rimAt(x + 1, y) || rimAt(x, y - 1) || rimAt(x, y + 1))) c.set(x + 1, y + 1, shade);
  return c;
}
// a ground whose own value sits near the gold's (by day: sand 175, snow 186 luma against Ember Gold's 190)
bool paleGround(Ground gr) { return gr == Ground::Sand || gr == Ground::Snow || gr == Ground::Ice; }
}  // namespace

void View::drawRing(const Game& g, const Actor& a, Vec2 cam) {
  if (!a.player || !screen::get().ring || g.mode == Mode::Title || g.mode == Mode::Creator || a.st == AState::Dead) return;
  const art::RigMetrics& M = rigFor(a.look);
  const Map& m = g.mode == Mode::Title ? g.world.over : g.map();
  const int gtx = (int)std::floor(a.p.x / 16), gty = (int)std::floor(a.p.y / 16);
  const bool pale = m.in(gtx, gty) && paleGround(m.at(gtx, gty));
  const Tex& t = sprites_.cachedTex(*this, (uint64_t)M.ringW | (uint64_t)M.ringH << 8 | (uint64_t)(pale ? 1 : 0) << 16 | 0x61ull << 56, paintRing);
  const float k = clampf(ringFlare_, 0, 1);
  const float alpha = 0.80f + 0.15f * k;
  // centred on the feet (the canvas has a px of margin all round)
  const float x = std::floor(a.p.x - M.ringW * 0.5f - 1 - cam.x + 0.5f), y = std::floor(a.p.y - M.ringH * 0.5f - 1 - cam.y + 0.5f);
  ringOn_ = true; ringX_ = x; ringY_ = y; ringA_ = alpha; ringTex_ = &t;
  pix_->blitEx(t, 0, 0, t.w, t.h, x, y, (float)t.w, (float)t.h, false, Color(1, 1, 1, alpha));
  lensStats.ringA = alpha;
}

// The night lift (STYLE_P2 8): after the light map, each human figure drawn in the sorted pass is added again in
// Moonwash, scaled by how dark the night is, so people stay a step brighter than the ground they stand on (lamps and
// the hero's torch still win: the lift is a fraction of the figure's own colours, never a flat glow).
// (P2a LENS fix r3, NIGHT-LIFT-THROUGH-OCCLUDERS) only what shows of a figure is lifted. The sorted pass records, besides
// the figures' blits (liftRecs_), every plain blit drawn round them (g_occRecs: tents, awnings, crowns, roofs, the
// people in front), and for each figure blit how many of those came before it (BlitRec::occ, stamped by Pix at the blit
// itself, so a horse's head or rump painted after its rider in the same drawable still covers him: fix r4). drawNightLift
// replays that order into its own target: a figure blit first clears the lift under it (its own body hides what it stands before),
// then adds its lift; every later blit clears the lift by its alpha. The target is then added over the lit world.
namespace rview {   // (SOLID 3.4a: was anonymous; see render_internal.h)
// (P2b VIEW-B fix r3, RIG2-ROOF-OVER-YARD) each building sprite's first opaque row per column (-1: an empty column),
// by bldgKey, filled with the sprite (storeBldg) and dropped with it: the sorted pass hides a rig-2 villager standing
// behind a roof by how deep his feet are under that column's top (drawWorld, case 3)
std::unordered_map<uint64_t, std::vector<int16_t>> g_bldgColTop;
Tex g_liftT;
std::vector<Pix::BlitRec> g_occRecs;
int g_liftEpoch = 0;
// dst.rgb *= 1 - src.a (the blit's coverage clears the lift under it)
SDL_BlendMode liftEraseMode() {
  static const SDL_BlendMode m = SDL_ComposeCustomBlendMode(SDL_BLENDFACTOR_ZERO, SDL_BLENDFACTOR_ONE_MINUS_SRC_ALPHA, SDL_BLENDOPERATION_ADD,
                                                            SDL_BLENDFACTOR_ZERO, SDL_BLENDFACTOR_ONE_MINUS_SRC_ALPHA, SDL_BLENDOPERATION_ADD);
  return m;
}
// the erase blend works on this renderer (checked once on the lift's target; the software renderer has no custom blends)
bool liftEraseOk(SDL_Texture* t) {
  static int ok = -1;
  if (ok < 0) { ok = SDL_SetTextureBlendMode(t, liftEraseMode()) ? 1 : 0; SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND); }
  return ok == 1;
}
void liftErase(SDL_Renderer* ren, const Pix::BlitRec& r) {
  if (!r.t) return;
  SDL_SetTextureBlendMode(r.t, liftEraseMode());
  SDL_SetTextureAlphaModFloat(r.t, clampf(r.a, 0, 1));
  const SDL_FRect s{(float)r.sx, (float)r.sy, (float)r.sw, (float)r.sh};
  const SDL_FRect d{r.dx, r.dy, r.dw, r.dh};
  SDL_RenderTextureRotated(ren, r.t, &s, &d, 0.0, nullptr, r.flip ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
  SDL_SetTextureBlendMode(r.t, SDL_BLENDMODE_BLEND);
  SDL_SetTextureAlphaModFloat(r.t, 1.0f);
}
}  // namespace rview

void View::drawNightLift(const Game& g, float dark) {
  lensStats.liftFigs = 0;
  struct Clear { std::vector<Pix::BlitRec>& l; ~Clear() { l.clear(); g_occRecs.clear(); } } clearAll{liftRecs_};
  if (g.mode == Mode::Title || g.mode == Mode::Creator || g.inside || dark < 0.05f || liftRecs_.empty()) return;
  // the lift's own target (the world view's size, one texel per world px), cleared to nothing
  static const bool noMask = std::getenv("EMB_LIFT_NOMASK") != nullptr;   // (test hook) the unmasked lift, for an A/B
  SDL_Renderer* ren = pix_->renderer();
  bool masked = !noMask && ren != nullptr;
  if (masked) {
    if (g_liftEpoch != Pix::texEpoch) { g_liftEpoch = Pix::texEpoch; g_liftT = Tex(); }
    if (g_liftT.w != Pix::W || g_liftT.h != Pix::H || !g_liftT.t) {
      pix_->destroy(g_liftT);
      g_liftT = pix_->makeTarget(Pix::W, Pix::H);
      if (g_liftT.t) SDL_SetTextureScaleMode(g_liftT.t, SDL_SCALEMODE_NEAREST);
    }
    masked = g_liftT.t != nullptr && liftEraseOk(g_liftT.t);
  }
  if (masked) {
    pix_->setTarget(&g_liftT);
    SDL_SetRenderDrawColor(ren, 0, 0, 0, 0);
    SDL_RenderClear(ren);
  }
  // the figures drawn so far (their union): a later blit outside it has nothing to clear
  float ux0 = 1e9f, uy0 = 1e9f, ux1 = -1e9f, uy1 = -1e9f;
  // each figure blit's place among the occluders, as Pix stamped it (never before an earlier figure's: the replay runs
  // forward; a blit made with no occluder record set counts as right after the figure before it)
  size_t oi = liftRecs_[0].occ != Pix::kNoOcc ? liftRecs_[0].occ : 0, occAt = oi;
  auto eraseTo = [&](size_t end) {
    for (; oi < end && oi < g_occRecs.size(); oi++) {
      const Pix::BlitRec& o = g_occRecs[oi];
      if (o.dx >= ux1 || o.dy >= uy1 || o.dx + o.dw <= ux0 || o.dy + o.dh <= uy0) continue;
      liftErase(ren, o);
    }
  };
  // two parts: the figure's own colours x Moonwash (keeps its shading) and a flat Moonwash glaze over its interior (the
  // mask: the ink outline is left out, so the edge stays dark while the body's darker half rises with the rest)
  const float k = kLiftK * dark, kf = kLiftFlat * dark;
  const Color mw(0x8F / 255.0f, 0xA6 / 255.0f, 0xD8 / 255.0f, 1);
  const Vec2 hc = g.pl().p - camera_.worldCam(*this);   // the hero's feet in the world view (their torch's middle)
  const float hx = hc.x, hy = hc.y;
  int maskBakes = 0;
  for (size_t ri = 0; ri < liftRecs_.size(); ri++) {
    const Pix::BlitRec& r = liftRecs_[ri];
    if (masked) {
      if (r.occ != Pix::kNoOcc) occAt = std::max(occAt, (size_t)r.occ);
      eraseTo(occAt);
      liftErase(ren, r);
      ux0 = std::min(ux0, r.dx); uy0 = std::min(uy0, r.dy); ux1 = std::max(ux1, r.dx + r.dw); uy1 = std::max(uy1, r.dy + r.dh);
    }
    if (r.a < 0.5f || r.sh < 12) continue;   // a bather's tinted under-water half, a cushion
    // fullest at the hero (in their torchlight), easing to kLiftFar from 24 to 96 px off: folk round about read as pale
    // ghosts at full strength; at 40 % an unlit villager on sand / snow sank under L6 (|fig - gnd| 7 / 3), at 60 % they
    // stand 12 or more over every ground (L6, fix round 2)
    constexpr float kLiftFar = 0.6f;
    const float fx = r.dx + r.dw * 0.5f - hx, fy = r.dy + r.dh - hy;
    const float nearK = 1.0f - (1.0f - kLiftFar) * clampf((std::sqrt(fx * fx + fy * fy) - 24.0f) / 72.0f, 0, 1);
    lensStats.liftFigs++;
    if (k > 0) pix_->blitRec(r, Color(mw.r * k * nearK, mw.g * k * nearK, mw.b * k * nearK, r.a), 1);
    auto mi = sprites_.liftMask_.find(r.t);
    if (kf > 0 && mi != sprites_.liftMask_.end()) {
      SpriteCache::LiftMask& lm = mi->second;
      if (!lm.tex.t && !lm.m.empty() && (witness || maskBakes++ < 8)) {   // its first night: the mask's texture (8 a frame)
        Canvas c(lm.w, lm.h);
        for (size_t i = 0; i < lm.m.size(); i++) if (lm.m[i]) c.px[i] = rgba(255, 255, 255);
        lm.tex = pix_->bake(c);
      }
      if (!lm.tex.t) continue;
      Pix::BlitRec q = r;
      q.t = lm.tex.t;
      pix_->blitRec(q, Color(mw.r, mw.g, mw.b, r.a * kf * nearK), 1);
    }
  }
  if (masked) {
    eraseTo(g_occRecs.size());
    // back to the world: the summed lift (its alpha 0) added over the lit world as it stands
    pix_->setTarget(nullptr);
    SDL_SetTextureBlendMode(g_liftT.t, SDL_BLENDMODE_ADD_PREMULTIPLIED);
    SDL_SetTextureColorModFloat(g_liftT.t, 1, 1, 1);
    SDL_SetTextureAlphaModFloat(g_liftT.t, 1);
    const SDL_FRect d{0, 0, (float)g_liftT.w, (float)g_liftT.h};
    SDL_RenderTexture(ren, g_liftT.t, nullptr, &d);
  }
  liftRecs_.clear();
  // the ring glows through the night: its front arc (below the feet) and its back arc's two ends (beside the legs): what no
  // part of the standing hero covers
  if (ringOn_ && ringTex_ && ringTex_->t) {
    const Tex& t = *ringTex_;
    const Color c(1, 1, 1, ringA_ * dark * kRingNightK);
    const int sy = t.h / 2 + 1, sh = t.h - sy;
    if (sh > 0) pix_->blitEx(t, 0, sy, t.w, sh, ringX_, ringY_ + sy, (float)t.w, (float)sh, false, c, 1);
    const int cx = t.w / 2, legs = t.w >= 24 ? 6 : 5;   // the legs' half-width round the feet (v1 / v2)
    const int lw = cx - legs, rx0 = cx + legs;
    if (lw > 0) {
      pix_->blitEx(t, 0, 0, lw, sy, ringX_, ringY_, (float)lw, (float)sy, false, c, 1);
      pix_->blitEx(t, rx0, 0, t.w - rx0, sy, ringX_ + rx0, ringY_, (float)(t.w - rx0), (float)sy, false, c, 1);
    }
  }
}

// Off-screen pips (STYLE_P2 18; the UI layer): a 7 x 5 Warning Ember chevron with an Ink edge at the safe area's edge,
// pointing at each aggro'd hostile outside the view within its aggro radius (bandits notice the hero from beyond half the
// view); it pulses slowly, faster as the foe closes. Out of the HUD's corners like the quest arrow.
void View::drawPips(const Game& g) {
  lensStats.pips = 0;
  if (g.mode != Mode::Play && g.mode != Mode::Dialogue) return;
  if (g.mode == Mode::Dialogue) return;   // the talk panel is modal
  Pix& P = *pix_;
  const float L = (float)Pix::SL, T = (float)Pix::ST, R = (float)(Pix::W - Pix::SR), B = (float)(Pix::H - Pix::SB);
  const Actor& hp = g.pl();
  static const char* kUp[5] = {"...#...", "..###..", ".#####.", "###.###", "##...##"};
  const Color ember(0xE8 / 255.0f, 0x60 / 255.0f, 0x3C / 255.0f), ink(0x1E / 255.0f, 0x16 / 255.0f, 0x2A / 255.0f);
  struct Placed { float x, y; };
  std::vector<Placed> placed;
  for (const Actor& a : g.actors) {
    if (a.player || !a.hostile || !a.aggro || a.st == AState::Dead || a.st == AState::Down || a.critter > 0) continue;
    const float d = len(a.p - hp.p);
    if (d > std::max(a.aggroR, 60.0f)) continue;
    // in view: any part of the body inside the safe area (a figure's middle, a few px of margin)
    const float bodyH = a.human ? (float)rigFor(a.look).figH(a.look) :(float)art::lookCellH(beasts::lookOf(a)) * beasts::drawScale(a);
    const Vec2 mid = camera_.toUi(*this, a.p - Vec2(0, bodyH * 0.5f));
    const float k = camera_.uiPerWorld(*this);
    if (mid.x > L - 4 * k && mid.x < R + 4 * k && mid.y > T - bodyH * 0.5f * k && mid.y < B + bodyH * 0.5f * k) continue;
    // where the ray from the safe area's centre to the foe meets the edge (8 px in)
    const Vec2 c((L + R) * 0.5f, (T + B) * 0.5f);
    const Vec2 dir = norm(mid - c);
    const float kk = std::min(((R - L) * 0.5f - 8) / std::max(0.01f, std::fabs(dir.x)), ((B - T) * 0.5f - 8) / std::max(0.01f, std::fabs(dir.y)));
    Vec2 at = c + dir * kk;
    // out of the HUD's corners: the vitals block top-left, the minimap and quest column top-right
    if (at.x < L + 150 && at.y < T + 60) { if (std::fabs(dir.x) > std::fabs(dir.y)) at.y = T + 64; else at.x = L + 154; }
    if (hudColLeft_ < 1e8f && at.x > hudColLeft_ - 4 && at.y < hudColBottom_ + 4) {
      if (std::fabs(dir.x) > std::fabs(dir.y)) at.y = hudColBottom_ + 8; else at.x = hudColLeft_ - 8;
    }
    // one pip per spot: a pack behind one edge shows as a short row, not a blot
    for (int guard = 0; guard < 6; guard++) {
      bool clash = false;
      for (const Placed& q : placed) if (std::fabs(q.x - at.x) < 9 && std::fabs(q.y - at.y) < 9) clash = true;
      if (!clash) break;
      if (std::fabs(dir.x) > std::fabs(dir.y)) at.y += 10; else at.x += 10;
    }
    placed.push_back({at.x, at.y});
    // the facing: the edge it rides (up, down, left, right)
    const int face = std::fabs(dir.x) * (B - T) > std::fabs(dir.y) * (R - L) ? (dir.x > 0 ? 3 : 2) : (dir.y > 0 ? 1 : 0);
    const float close = clampf(1.0f - d / std::max(60.0f, a.aggroR), 0, 1);
    const float rate = 3.0f + 7.0f * close;   // faster as the foe closes
    const float pulse = 0.72f + 0.28f * std::sin(t_ * rate + (float)a.id);
    auto fill = [&](int x, int y) {   // the pattern in the pip's facing (7 x 5 up; turned for the others)
      int u = x, v = y;
      if (face == 1) v = 4 - y;
      if (face == 2 || face == 3) { u = y; v = x; if (face == 3) v = 4 - x; }
      return u >= 0 && u < 7 && v >= 0 && v < 5 && kUp[v][u] == '#';
    };
    const int pw = face >= 2 ? 5 : 7, ph = face >= 2 ? 7 : 5;
    const float x0 = std::floor(at.x - pw / 2.0f), y0 = std::floor(at.y - ph / 2.0f);
    for (int y = -1; y <= ph; y++)
      for (int x = -1; x <= pw; x++) {
        if (fill(x, y)) continue;
        bool edge = false;
        for (int j = -1; j <= 1 && !edge; j++) for (int i = -1; i <= 1 && !edge; i++) edge = fill(x + i, y + j);
        if (edge) P.rect(x0 + x, y0 + y, 1, 1, Color(ink.r, ink.g, ink.b, 0.9f * pulse));
      }
    for (int y = 0; y < ph; y++)
      for (int x = 0; x < pw; x++)
        if (fill(x, y)) P.rect(x0 + x, y0 + y, 1, 1, Color(std::min(1.0f, ember.r * (0.9f + 0.2f * close)), ember.g, ember.b, pulse));
    lensStats.pips++;
  }
}

namespace rview {   // (SOLID 3.4a: was anonymous; see render_internal.h)
uint64_t rigKey(const art::HumanLook& L, art::RigPart part, bool child) {
  return ew::mix64(L.key() ^ ((uint64_t)part + 1) * 0x9E3779B97F4A7C15ull ^ (child ? 0x2C41D2ull : 0));
}
// the v2 bake budget per frame (VISION_PLAN 15.36 "bake": 2 ms, 1.2 ms on the web) and the v2 atlases' byte cap (of the
// phone's 24 MB for all people textures: the v1 sheets and pose sheets keep the rest while both rigs live)
#ifdef __EMSCRIPTEN__
constexpr double kRigBakeMs = 1.2;
#else
const double kRigBakeMs = std::getenv("EMB_WEBSIM") ? 1.2 : 2.0;
#endif
// (P2c SWITCH, C8) 16 MB unless the capital sweep at 15:00 evicts an on-screen look (then 20 MB, inside the 24 MB people
// budget): View::rigCapMB_ (EMB_RIG_CAP_MB overrides it for the measurement)
constexpr float kStreamR = 34.0f * 16.0f;   // the stream radius (px): folk inside it are baked before they are seen
}  // namespace rview

// a v2 atlas: from the cache, baked now within this frame's budget (a person on screen needs it), or queued (drawn v1
// until the queue reaches it). A Witness run bakes inline: no picture depends on CPU speed.
View::RigTex SpriteCache::rigTex(View& v, const art::HumanLook& L, art::RigPart part, bool child) {
  RigTex r;
  if (art::rigVersion() < 2 || !art::rig2Covers(L)) return r;   // v1 is drawn: nothing to bake
  // (P2b VIEW-B) per-part coverage: rig2Covers holds LOCO and DEATH (and ACTION for the armed); the RUN and an unarmed
  // body's ACTION are drawn only where the doll covers them too (else the caller's next part: the walk, the idle), so a
  // part with a missing map never shows a hole
  if ((part == art::RigPart::Run || part == art::RigPart::Action || part == art::RigPart::Drawn) && !art::doll::dollCovers(L, part)) return r;
  const uint64_t k = rigKey(L, part, child);
  auto it = rigTex_.find(k);
  if (it == rigTex_.end()) {
    if (!v.witness && !rigBakeNow_ && !rigBakeBudget(v)) {   // over this frame's budget: queued, nearest first next frame (a menu figure bakes now)
      bool queued = false;
      for (const SpriteCache::RigJob& j : rigQueue_) if (j.part == part && j.child == child && j.look.key() == L.key()) queued = true;
      if (!queued) { SpriteCache::RigJob j; j.look = L; j.part = part; j.child = child; j.d2 = 0; rigQueue_.insert(rigQueue_.begin(), j); }
      return r;
    }
    const auto t0 = std::chrono::steady_clock::now();
    SpriteCache::RigEntry e;
    e.sheet = art::humanRig(L, part);
    if (!e.sheet.empty() && e.sheet.atlas.w > 0) {
      e.tex = v.pix_->bake(e.sheet.atlas);
      liftMaskFor(v, e.sheet.atlas, e.tex);   // (P2a slice) the night lift's interior mask: v2 people rise inside their ink
      // (P2c ladder) the glow plane beside it (its small canvas kept: the motes are born on its pixels)
      // the light it adds is the glow's colour half-way to white: a saturated rest colour added to itself only clips its
      // full channels (the first in-game breath measured +14/255), its pale light whitens the gem and runes as they breathe
      if (!e.sheet.glow.empty()) {
        Canvas lightKey = e.sheet.glow.atlas;
        for (uint32_t& c : lightKey.px)
          if (c >> 24) {
            auto up = [](uint32_t v) { return (uint32_t)(v + (255u - v) * 55u / 100u); };
            c = 0xFF000000u | up(c >> 16 & 255u) << 16 | up(c >> 8 & 255u) << 8 | up(c & 255u);
          }
        e.glowTex = v.pix_->bake(lightKey);
      }
    }
    // (polish i1) a T6 look keeps each frame's silhouette: the aura breathes round it, the edge sweep stays on the piece
    // (P2c SWITCH) and a Mythic wearer's at any tier: its flame hugs the doll frame (C7)
    if ((L.topTier() >= 6 || L.mythic || (L.glow && L.glowColor == 0xFF2868FFu)) && e.sheet.atlas.w > 0) {
      e.masks.reserve(e.sheet.frames.size());
      for (const art::FrameRect& f : e.sheet.frames) e.masks.push_back(art::finish::maskOf(e.sheet.atlas, f.x, f.y, f.w, f.h));
    }
    e.sheet.atlas = Canvas();   // the frames are kept; the pixels live in the texture
    it = rigTex_.emplace(k, std::move(e)).first;
    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    rigBakeFrameMs_ += ms;
    v.lensStats.rigBaked++;
    // (P2c SWITCH, C8) an atlas baked again after the cap evicted it: the cache was too small for what is on screen
    if (rigEvicted_.erase(k)) v.lensStats.rigRebakes++;
    if (part == art::RigPart::Action) v.lensStats.actionMsMax = std::max(v.lensStats.actionMsMax, ms);
    if (std::getenv("EMB_TIMING")) std::printf("rig atlas baked: part %d %.3f ms\n", (int)part, ms);
  }
  it->second.used = v.t_;
  r.tex = &it->second.tex;
  r.sheet = &it->second.sheet;
  if (it->second.glowTex.t) r.glowTex = &it->second.glowTex;
  if (!it->second.masks.empty()) r.masks = &it->second.masks;
  return r;
}
// (P2c ladder, contracts 6b; STYLE_P2 27.7) the living finish of the rig-2 frame just drawn at (x2, y2) (its pivot at
// pivX, pivY; world-view px): art::finish::figureLights (the glow breathing with its 2 px halo, the T6 aura or the Mythic's
// flame ring, the edge sweep grip to tip with its star, flashed on a strike), the gleam (the figure's own pixels through a
// slanted band), and the world-space motes / embers (born on this frame's glow pixels, rising where they were born: a
// walker trails them; a striking Mythic sheds them off the blade's tip). Queued here (the sorted pass), drawn additively
// after the lighting (drawWeather's tail). Rig 2 only.
void View::queueLivingFinish(const Actor& a, const art::HumanLook& L, const RigTex& rt, const art::FrameRect& f, const art::RigAnchors* an,
                             float x2, float y2, float pivX, float pivY, float alpha, bool strike) {
  queueLivingFinishId(a.id, L, rt, f, an, x2, y2, pivX, pivY, alpha, strike, false);
}
// (P2c SWITCH) the same by the figure's id; `menu`: a menu figure (its motes in the figure's own frame, no world camera)
void View::queueLivingFinishId(int id, const art::HumanLook& L, const RigTex& rt, const art::FrameRect& f, const art::RigAnchors* an,
                               float x2, float y2, float pivX, float pivY, float alpha, bool strike, bool menu) {
  if (!art::finish::enabled() || !rt.ready() || alpha <= 0.02f) return;
  const int top = L.topTier();
  const bool steady = top == 4 && L.sheen == 6;   // (P2c forge, D28) a glowing alloy's seams from T4, steady
  if (top < 5 && !steady) return;
  const art::RigSheet& S = *rt.sheet;
  const size_t fi = (size_t)(&f - S.frames.data());
  if (fi >= S.frames.size()) return;
  const float phase = art::finish::actorPhase((uint32_t)id);
  std::vector<art::finish::GlowPx> gpx;
  if (fi < S.glow.first.size()) S.glow.each(fi, [&](int gx, int gy, uint32_t c) { gpx.push_back({gx, gy, c}); });
  uint32_t hot = 0;
  {
    int hotL = -1;
    for (const auto& g : gpx) { const int l = (int)(g.c & 255) * 3 + (int)(g.c >> 8 & 255) * 6 + (int)(g.c >> 16 & 255); if (l > hotL) { hotL = l; hot = g.c; } }
  }
  auto push = [&](const art::finish::Light& q, float ox, float oy) {
    beasts::finish.push_back({nullptr, 0, 0, 0, 0, ox + q.x, oy + q.y, q.w, q.h,
                              Color((q.c & 255) / 255.0f, (q.c >> 8 & 255) / 255.0f, (q.c >> 16 & 255) / 255.0f, q.a * alpha)});
  };
  art::finish::Figure F;
  F.topTier = top; F.mythic = L.mythic != 0; F.t = t_; F.phase = phase; F.glow = &gpx; F.strike = strike; F.steady = steady;
  F.mask = rt.masks && fi < rt.masks->size() ? &(*rt.masks)[fi] : nullptr;
  F.bladeT6 = L.tier[art::HumanLook::TIER_WEAPON] >= 6;
  // the line in frame px (the grip: its mark, else the weapon hand's fist)
  const bool gripOn = an && an->gripX != art::kNoAnchor, handOn = an && an->handRX != art::kNoAnchor;
  if (an && (gripOn || handOn) && an->tipX != art::kNoAnchor) {
    F.line = true;
    F.gx = (gripOn ? an->gripX : an->handRX + 1.0f) + f.px; F.gy = (gripOn ? an->gripY : an->handRY + 1.0f) + f.py;
    F.tx = an->tipX + f.px; F.ty = an->tipY + f.py;
  }
  {
    std::vector<art::finish::Light> lights;
    art::finish::figureLights(F, lights);
    // (P2c SWITCH, C7: "the Mythic aura hugs doll frames, alpha only within 3 px of the silhouette") a light whose pixel
    // stands more than kAuraReach px off the frame's silhouette is not drawn: a flame tongue's tip (6b draws them 1-4.5 px
    // tall) stops at 3 px. The motes and embers below are particles, not the aura: they rise free.
    constexpr int kAuraReach = 3;
    for (const art::finish::Light& q : lights) {
      if (F.mask) {
        const int cx = (int)std::floor(q.x + q.w * 0.5f), cy = (int)std::floor(q.y + q.h * 0.5f);
        bool near = false;
        for (int j = -kAuraReach; j <= kAuraReach && !near; j++)
          for (int i = -kAuraReach; i <= kAuraReach && !near; i++) near = F.mask->at(cx + i, cy + j);
        if (!near) continue;
      }
      push(q, x2, y2);
    }
  }
  finishAuraEnd_ = beasts::finish.size();   // the aura's lights end here (the particles and the gleam follow)
  if (top < 6) { beasts::finishFigures++; return; }   // (T4 steady and T5: no motes, no gleam)
  // the motes / embers: world space (view px + the camera), so the camera's motion never drags them along
  {
    art::finish::Particles& ps = finishParts_[id];
    const float cx = menu ? 0.0f : std::floor(camera_.cam_.x + 0.5f), cy = menu ? 0.0f : std::floor(camera_.cam_.y + 0.5f);
    const float wx = x2 + cx, wy = y2 + cy;
    art::finish::particlesStep(ps, (uint32_t)id * 2654435761u + 0x5EEDu, t_, F.mythic, wx, wy, gpx, F.line && strike, wx + F.tx, wy + F.ty);
    std::vector<art::finish::Light> lights;
    art::finish::particleLights(ps, t_, hot, lights);
    for (const art::finish::Light& q : lights) push(q, -cx, -cy);
  }
  // (ladder i3) the gleam: the figure's own pixels added back through a slanted band sweeping across it
  {
    const float u = art::finish::gleamAt(t_, phase);
    if (u >= 0) {
      const Color gc = L.mythic ? Color(1.0f, 0.62f, 0.3f, art::finish::kGleamA * alpha) : Color(1, 1, 1, art::finish::kGleamA * alpha);
      for (int y = 0; y < f.h; y += 2) {
        const int c0 = std::max(0, art::finish::gleamCol(u, f.w, f.h, y)), c1 = std::min<int>(f.w, art::finish::gleamCol(u, f.w, f.h, y) + art::finish::kGleamW);
        const int rh = std::min(2, f.h - y);
        if (c1 > c0) beasts::finish.push_back({rt.tex, f.x + c0, f.y + y, c1 - c0, rh, x2 + c0, y2 + y, (float)(c1 - c0), (float)rh, gc});
      }
    }
  }
  (void)pivX; (void)pivY;
  beasts::finishFigures++;
}
// ---- (P2c SWITCH) the menus on the doll (rig 2; PLAN C7, "The envelope in production": menu frames sized for the T6
// envelope, the living finish and the stir in every draw path). Rig 1 never reaches these: every menu keeps its v1 draw.
namespace {
// the T6 envelope round the pivot (doll_envelope.h): the fin at F-41, 28 px shoulders (14 a side), the long cloak 8 px
// behind the body's box (8 + 8 a side facing R / L); the feet's row under the pivot
constexpr int kEnvUp = 41, kEnvDown = 1;
int envSide() { const auto& e6 = art::doll::envelopeOf(6); return std::max<int>(e6.shoulders / 2, 8 + e6.cloakBack); }
// the menu figure's own id for its phase and motes (never an actor's: actors count up from 0)
int menuFigId(int where, int id) { return -(100000 + where * 64 + (id & 63)); }
}  // namespace

int View::menuDollScale(const art::HumanLook& L, const MenuStage& st, float feetX, float feetY, int want, bool swing) {
  if (art::rigVersion() < 2 || !art::rig2Covers(L)) return 0;
  const uint64_t k = L.key() ^ (swing ? 0x5A1C6E0011ull : 0x3E7B0022ull);
  auto it = sprites_.menuBoxes_.find(k);
  if (it == sprites_.menuBoxes_.end()) {
    if (sprites_.menuBoxes_.size() > 64) sprites_.menuBoxes_.clear();
    SpriteCache::MenuBox b;
    b.u = kEnvUp; b.d = kEnvDown; b.l = b.r = envSide();
    auto take = [&](art::RigPart part) {
      if ((part == art::RigPart::Run || part == art::RigPart::Action) && !art::doll::dollCovers(L, part)) return;
      sprites_.rigBakeNow_ = true;
      const RigTex rt = sprites_.rigTex(*this, L, part);
      sprites_.rigBakeNow_ = false;
      if (!rt.ready()) return;
      for (const art::FrameRect& f : rt.sheet->frames) {
        b.l = std::max<int>(b.l, f.px); b.r = std::max<int>(b.r, f.w - f.px);
        b.u = std::max<int>(b.u, f.py); b.d = std::max<int>(b.d, f.h - f.py);
      }
    };
    take(art::RigPart::Loco);
    if (swing) { take(art::RigPart::Run); take(art::RigPart::Action); }
    it = sprites_.menuBoxes_.emplace(k, b).first;
  }
  const SpriteCache::MenuBox& b = it->second;
  const float fx = std::floor(feetX), fy = std::floor(feetY);
  int s = std::max(1, want);
  for (; s > 1; s--)
    if (fx - b.l * s >= st.x && fx + b.r * s <= st.x + st.w && fy - b.u * s >= st.y && fy + b.d * s <= st.y + st.h) break;
  return s;
}

// the blows facing `facing` (windup, strike, follow; the combo's three variants) stand inside the stage at this multiple
bool View::menuSwingFits(const art::HumanLook& L, const MenuStage& st, float feetX, float feetY, int scale, int facing) {
  if (art::rigVersion() < 2 || !art::rig2Covers(L) || !art::doll::dollCovers(L, art::RigPart::Action)) return false;
  sprites_.rigBakeNow_ = true;
  const RigTex rt = sprites_.rigTex(*this, L, art::RigPart::Action);
  sprites_.rigBakeNow_ = false;
  if (!rt.ready()) return false;
  const float fx = std::floor(feetX), fy = std::floor(feetY);
  for (art::Clip c : {art::Clip::Windup, art::Clip::Strike, art::Clip::Follow})
    for (int v = 0; v < 3; v++)
      for (int i = 0; i < rt.sheet->count(facing, c, v); i++) {
        const art::FrameRect* f = rt.sheet->frame(facing, c, v, i);
        if (!f) continue;
        const float x0 = fx - f->px * scale, y0 = fy - f->py * scale;
        if (x0 < st.x || y0 < st.y || x0 + f->w * scale > st.x + st.w || y0 + f->h * scale > st.y + st.h) return false;
      }
  return true;
}

bool View::drawMenuDoll(const art::HumanLook& L, int where, int facing, MenuGait gait, float feetX, float feetY, int scale, const MenuStage& st,
                        float alpha, int topRows, int id) {
  if (art::rigVersion() < 2 || !art::rig2Covers(L) || scale < 1 || !pix_) return false;
  Pix& P = *pix_;
  facing = std::clamp(facing, 0, 3);
  const int fid = menuFigId(where, id);
  const float own = art::finish::actorPhase((uint32_t)fid);   // a figure's own breath, out of step with the next
  auto get = [&](art::RigPart part) {
    sprites_.rigBakeNow_ = true;
    const RigTex r = sprites_.rigTex(*this, L, part);
    sprites_.rigBakeNow_ = false;
    return r;
  };
  RigTex rt;
  art::RigPart part = art::RigPart::Loco;
  art::Clip clip = art::Clip::Idle;
  int variant = 0, fi = 0;
  auto idle = [&]() {
    part = art::RigPart::Loco; clip = art::Clip::Idle; variant = 0;
    rt = get(part);
    // neutral and breathe alternate 0.55 s (the breathe frame carries the T5-T6 stir: the breeze views); a 0.12 s blink
    // every 4.4 s when the clip has one (anim.cpp's idle)
    const float t = t_ + own * 1.1f;
    fi = (int)(t / 0.55f) & 1;
    if (rt.ready() && rt.sheet->count(facing, clip, 0) > 2 && std::fmod(t_ + own * 4.4f, 4.4f) < 0.12f) fi = 2;
  };
  switch (gait) {
    case MenuGait::Idle: idle(); break;
    case MenuGait::Walk:   // on the spot at the walk's pace (about 9 frames a second)
      part = art::RigPart::Loco; clip = art::Clip::Walk; rt = get(part);
      if (rt.ready()) fi = (int)(t_ * 9.0f) % std::max(1, rt.sheet->count(facing, clip, 0));
      break;
    case MenuGait::Run:    // the RUN part's cycle where the doll covers it, else the walk a little quicker
      if (art::doll::dollCovers(L, art::RigPart::Run)) { part = art::RigPart::Run; clip = art::Clip::Walk; rt = get(part); }
      if (!rt.ready()) { part = art::RigPart::Loco; clip = art::Clip::Walk; rt = get(part); }
      if (rt.ready()) fi = (int)(t_ * 12.0f) % std::max(1, rt.sheet->count(facing, clip, 0));
      break;
    case MenuGait::Swing: {   // a blow every 1.6 s (windup 2 x 0.12 s, strike 2 x 0.07, follow 2 x 0.10), the combo's
                              // three variants in turn, the idle between
      if (!art::doll::dollCovers(L, art::RigPart::Action)) { idle(); break; }
      if (menuSwingT0_ < 0 || menuSwingT0_ > t_) menuSwingT0_ = t_;
      const float u = std::fmod(t_ - menuSwingT0_, 1.6f);
      const int v = (int)((t_ - menuSwingT0_) / 1.6f) % 3;
      if (u >= 0.58f) { idle(); break; }
      variant = v;
      if (u < 0.24f) { clip = art::Clip::Windup; fi = (int)(u / 0.12f); }
      else if (u < 0.38f) { clip = art::Clip::Strike; fi = (int)((u - 0.24f) / 0.07f); }
      else { clip = art::Clip::Follow; fi = (int)((u - 0.38f) / 0.10f); }
      part = art::RigPart::Action;
      rt = get(part);
      if (!rt.ready() || !rt.sheet->frame(facing, clip, variant, fi)) idle();
      break;
    }
  }
  if (!rt.ready()) return false;
  const art::FrameRect* f = rt.sheet->frame(facing, clip, variant, fi);
  if (!f) { idle(); f = rt.ready() ? rt.sheet->frame(facing, clip, variant, fi) : nullptr; }
  if (!f) return false;
  const art::RigAnchors* an = rt.sheet->anchorsOf(facing, clip, variant, fi);
  const float s = (float)scale;
  const float x0 = std::floor(feetX) - f->px * s, y0 = std::floor(feetY) - f->py * s;
  // topRows > 0 (a dialogue's head-and-shoulders): the frame cut to the stage's rows (the breath moves inside the cut)
  int r0 = 0, rows = f->h;
  if (topRows > 0) {
    r0 = std::clamp((int)std::ceil((st.y - y0) / s - 1e-3f), 0, (int)f->h);
    rows = std::clamp((int)std::floor((st.y + st.h - y0) / s + 1e-3f), r0, (int)f->h);
  }
  if (rows > r0) P.blitEx(*rt.tex, f->x, f->y + r0, f->w, rows - r0, x0, y0 + r0 * s, f->w * s, (rows - r0) * s, false, Color(1, 1, 1, alpha));
  MenuFigure mf;
  mf.where = where; mf.rig = 2; mf.scale = scale; mf.x = x0; mf.y = y0 + r0 * s; mf.w = (int)(f->w * scale); mf.h = (rows - r0) * scale;
  mf.part = (int)part; mf.clip = (int)clip; mf.frame = fi; mf.facing = facing; mf.tier = L.topTier(); mf.stage = st;
  mf.cropped = st.w > 0 && (x0 < st.x - 0.01f || mf.y < st.y - 0.01f || x0 + mf.w > st.x + st.w + 0.01f || mf.y + mf.h > st.y + st.h + 0.01f);
  // the living finish on the menu figure: the world's own lights (queued against the frame's top-left), drawn here at
  // the figure's multiple, additively (no night pass in a menu). The furthest lit pixel off the silhouette is recorded.
  const size_t n0 = beasts::finish.size();
  queueLivingFinishId(fid, L, rt, *f, an, 0, 0, (float)f->px, (float)f->py, alpha, clip == art::Clip::Strike, true);
  if (beasts::finish.size() > n0) {
    const size_t fiAll = (size_t)(f - rt.sheet->frames.data());
    const art::finish::Mask* mk = rt.masks && fiAll < rt.masks->size() ? &(*rt.masks)[fiAll] : nullptr;
    int far = -1;
    const size_t auraEnd = finishAuraEnd_;
    for (size_t i = n0; i < beasts::finish.size(); i++) {
      const beasts::Finish& q = beasts::finish[i];
      if (q.y >= rows || q.y + q.h <= r0) continue;   // outside a dialogue's cut
      const float h = std::min(q.h, (float)rows - q.y);
      if (q.t) P.blitEx(*q.t, q.sx, q.sy, q.sw, std::max(1, (int)std::ceil(h * q.sh / std::max(1e-3f, q.h))), x0 + q.x * s, y0 + q.y * s, q.w * s, h * s, false, q.c, 1);
      else P.rectAdd(x0 + q.x * s, y0 + q.y * s, q.w * s, h * s, q.c);
      mf.lights++;
      if (!mk || q.t || q.c.a <= 0.02f || i >= auraEnd) continue;   // (the aura's lights only: motes rise free)
      // the light's pixels' distance from the silhouette (Chebyshev, frame px; 0 on the figure)
      for (int yy = (int)std::floor(q.y); yy < (int)std::ceil(q.y + h); yy++)
        for (int xx = (int)std::floor(q.x); xx < (int)std::ceil(q.x + q.w); xx++) {
          int d = 0;
          if (!mk->at(xx, yy)) {
            d = 99;
            for (int r = 1; r <= 8 && d == 99; r++)
              for (int j = -r; j <= r && d == 99; j++)
                for (int i2 = -r; i2 <= r; i2++)
                  if ((std::abs(j) == r || std::abs(i2) == r) && mk->at(xx + i2, yy + j)) { d = r; break; }
          }
          far = std::max(far, d);
        }
    }
    beasts::finish.resize(n0);
    mf.auraOut = far;
  }
  menuFigs.push_back(mf);
  return true;
}

void View::noteMenuV1(int where, float x, float y, int w, int h, const MenuStage& st) {
  v1Draws++;
  MenuFigure mf;
  mf.where = where; mf.rig = 1; mf.x = x; mf.y = y; mf.w = w; mf.h = h; mf.stage = st;
  mf.cropped = st.w > 0 && (x < st.x - 0.01f || y < st.y - 0.01f || x + w > st.x + st.w + 0.01f || y + h > st.y + st.h + 0.01f);
  menuFigs.push_back(mf);
}

// a party portrait's living finish: the bust's glow pixels (portraitGlow_, kept when the bust was cut from the doll frame)
// breathing with the wearer's phase, each with its 4-neighbour halo
void View::portraitFinish(uint32_t memberId, float x, float y, float scale, float alpha, int sx, int sy, int sw, int sh) {
  if (art::rigVersion() < 2 || !art::finish::enabled() || !pix_) return;
  auto lk = sprites_.portraitLook_.find(memberId);
  if (lk == sprites_.portraitLook_.end() || lk->second.second < 5) return;
  auto it = sprites_.portraitGlow_.find(lk->second.first);
  if (it == sprites_.portraitGlow_.end() || it->second.empty()) return;
  const art::finish::Pulse pu = art::finish::pulseOf(lk->second.second);
  const float ph = art::finish::actorPhase((uint32_t)memberId);
  auto up = [](uint32_t v) { return (float)(v + (255u - v) * 55u / 100u) / 255.0f; };
  auto isGlow = [&](int gx, int gy) {
    for (const art::finish::GlowPx& o : it->second) if (o.x == gx && o.y == gy) return true;
    return false;
  };
  auto inSrc = [&](int px, int py) { return px >= sx && py >= sy && px < sx + sw && py < sy + sh; };
  static const int d4[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
  for (const art::finish::GlowPx& g : it->second) {
    if (!inSrc(g.x, g.y)) continue;
    const float a = art::finish::pixelLight(pu, t_, ph, g.x, g.y) * alpha;
    if (a <= 0.01f) continue;
    const Color c(up(g.c & 255u), up(g.c >> 8 & 255u), up(g.c >> 16 & 255u), a);
    pix_->rectAdd(x + (g.x - sx) * scale, y + (g.y - sy) * scale, scale, scale, c);
    const Color hc(c.r, c.g, c.b, a * art::finish::kHalo);
    for (const auto& d : d4) {
      const int nx = g.x + d[0], ny = g.y + d[1];
      if (!inSrc(nx, ny) || isGlow(nx, ny)) continue;
      pix_->rectAdd(x + (nx - sx) * scale, y + (ny - sy) * scale, scale, scale, hc);
    }
  }
}

// a v2 posture atlas (art::humanPosture2: the posture's frames as clip Idle, variant 0, in four facings), cached with the
// v2 atlases and baked within the same budget; not ready while the painters do not cover the look
View::RigTex SpriteCache::rigPoseTex(View& v, const art::HumanLook& L, art::Posture p, uint8_t variant) {
  RigTex r;
  if (art::rigVersion() < 2 || !art::rig2Covers(L)) return r;
  const uint64_t k = ew::mix64(L.key() ^ 0x9051E2ull ^ ((uint64_t)p + 1) << 40 ^ (uint64_t)variant << 52);
  auto it = rigTex_.find(k);
  if (it == rigTex_.end()) {
    if (!v.witness && !rigBakeNow_ && !rigBakeBudget(v)) return r;   // next frame (drawn v1 meanwhile; a body on screen bakes now)
    const auto t0 = std::chrono::steady_clock::now();
    SpriteCache::RigEntry e;
    // (P2b VIEW-B) per-part coverage: a posture the doll does not cover for this look (a map missing in its legs, arms,
    // hang or prop keys) stays an empty entry: v1's posture sheet draws the whole figure, never a doll with a hole
    if (art::doll::dollCoversPosture(L, p, variant)) e.sheet = art::humanPosture2(L, p, variant);
    if (!e.sheet.empty() && e.sheet.atlas.w > 0) { e.tex = v.pix_->bake(e.sheet.atlas); liftMaskFor(v, e.sheet.atlas, e.tex); }
    e.sheet.atlas = Canvas();
    it = rigTex_.emplace(k, std::move(e)).first;
    rigBakeFrameMs_ += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
  }
  it->second.used = v.t_;
  r.tex = &it->second.tex;
  r.sheet = &it->second.sheet;
  return r;
}
// (P2a LENS) a rider (the hero's or a companion's) on v2: the Ride posture's frame for the facing, its hip anchor on
// the saddle's seat point (hipX, hipY: screen px; v1's seat line), else its pivot on v1's feet point (pivX, pivY);
// never flipped. False (draw v1) until the painters cover the look. The blit is recorded for the night lift.
bool View::drawRiderV2(const art::HumanLook& L, int facing, float hipX, float hipY, float pivX, float pivY, Color c, int blend, RideGhost* ghost,
                       int horseFrame, int riderId) {
  sprites_.rigBakeNow_ = true;   // (P2c flip, D103) a rider on screen bakes now: never a v1 rider while the atlas waits
  const RigTex rt = sprites_.rigPoseTex(*this, L, art::Posture::Ride, 0);
  sprites_.rigBakeNow_ = false;
  if (!rt.ready()) return false;
  // (P2c SWITCH) the Ride frame per horse frame: the posture's frames step with the horse's cell (its gait's own beat),
  // the hip anchor of that frame on the saddle's seat point of that cell
  const int n = std::max(1, rt.sheet->count(facing, art::Clip::Idle, 0));
  const int ri = ((horseFrame % n) + n) % n;
  const art::FrameRect* f = rt.sheet->frame(facing, art::Clip::Idle, 0, ri);
  if (!f) return false;
  const art::RigAnchors* an = rt.sheet->anchorsOf(facing, art::Clip::Idle, 0, ri);
  float x, y;
  if (an && an->hipX != art::kNoAnchor && an->hipY != art::kNoAnchor) { x = hipX - (f->px + an->hipX); y = hipY - (f->py + an->hipY); }
  else { x = pivX - f->px; y = pivY - f->py; }
  x = std::floor(x + 0.5f); y = std::floor(y + 0.5f);
  if (blend == 0) pix_->recordBlits(&liftRecs_);
  pix_->blitEx(*rt.tex, f->x, f->y, f->w, f->h, x, y, (float)f->w, (float)f->h, false, c, blend);
  pix_->recordBlits(nullptr);
  // (P2c SWITCH) a T5-T6 rider's living finish (the posture atlas keeps no glow plane: the aura, rim and sweep from the
  // frame's anchors; masks only where the LOCO atlas keeps them)
  if (blend == 0 && riderId >= 0 && L.topTier() >= 5) queueLivingFinishId(riderId, L, rt, *f, an, x, y, x + f->px, y + f->py, c.a, false, false);
  if (ghost) { ghost->t = rt.tex; ghost->sx = f->x; ghost->sy = f->y; ghost->w = f->w; ghost->h = f->h; ghost->flip = false; ghost->x = x; ghost->y = y; }
  return true;
}
// (P2b VIEW-B) the drawn doll frame's top against the rig's 32 px reference crown (markerY 36 = 4 px of air over it):
// markers, telegraphs, plates and bars move by this, so they sit over a child's head, a hat's crown or a seated body
// as they do over the reference man. A body not drawn on the doll within the last quarter second keeps the metrics.
float View::topLift(const Actor& a) const {
  if (art::rigVersion() < 2 || !a.human) return 0;
  auto it = drawnHead_.find(a.id);
  if (it == drawnHead_.end() || !it->second.v2 || t_ - it->second.t > 0.25f || t_ < it->second.t) return 0;
  const float above = std::floor(a.p.y + 0.5f) - std::floor(it->second.y + 0.5f);
  return above - (float)(art::rig(2).markerY - 4);
}
bool SpriteCache::rigBakeBudget(View& v) {
  if (rigBakeFrameT_ != v.t_) { rigBakeFrameT_ = v.t_; rigBakeFrameMs_ = 0; v.lensStats.rigBaked = 0; }
  return rigBakeFrameMs_ < kRigBakeMs;
}
// The bake queue (rig 2): the looks of the folk inside the stream radius whose LOCO atlas is not baked (a fighter's
// ACTION too once it hunts the hero), nearest first, rebuilt four times a second; each frame bakes from its head within
// the budget, so folk are baked before they walk into view.
void SpriteCache::rigBakeQueue(View& v, const Game& g) {
  v.lensStats.rigQueued = (int)rigQueue_.size();
  if (art::rigVersion() < 2 || g.mode == Mode::Title || g.mode == Mode::Creator) { rigQueue_.clear(); return; }
  if (v.t_ - rigQueueT_ > 0.25f || v.t_ < rigQueueT_) {
    rigQueueT_ = v.t_;
    std::vector<SpriteCache::RigJob> q;
    const Vec2 pp = g.pl().p;
    for (const Actor& a : g.actors) {
      if (!a.human || a.st == AState::Dead) continue;
      const float d2 = len2(a.p - pp);
      if (d2 > kStreamR * kStreamR || !art::rig2Covers(a.look)) continue;
      const bool child = a.role == Role::Child;
      // (P2b VIEW-B) a part joins the queue only where the doll covers it (rigTex would draw the fallback anyway)
      auto want = [&](art::RigPart part, float bias = 0) {
        if (rigTex_.count(rigKey(a.look, part, child))) return;
        if ((part == art::RigPart::Run || part == art::RigPart::Action || part == art::RigPart::Drawn) && !art::doll::dollCovers(a.look, part)) return;
        SpriteCache::RigJob j; j.look = a.look; j.part = part; j.child = child; j.d2 = d2 + bias;
        q.push_back(j);
      };
      want(art::RigPart::Loco);
      // (P2b) the run's own drawings before a body breaks into one: the hero, a runner, and anyone who may run already
      // moving near the switch (a guard or a bandit giving chase), so the run never starts on v1 or the walk's frames
      const bool fighting = a.hostile && (a.aggro || d2 < 200.0f * 200.0f);
      if (animMayRun(a) && (a.player || a.gait || a.moveV > art::doll::kGaitSwitch * 0.6f || fighting)) want(art::RigPart::Run, 64.0f * 64.0f);
      if (a.player || fighting) want(art::RigPart::Action);
      // (P2c SWITCH) the DRAWN part (the drawn idle, walk and run) beside ACTION once a fighter has aggro: a guard who
      // draws on the hero steps forward on the doll's drawn frames, never the sheathed walk with a blade in the air
      if (a.player || (fighting && a.aggro)) want(art::RigPart::Drawn, 32.0f * 32.0f);
      // a fighter in the fray may fall: its DEATH baked beforehand, so the fall's first frame is the doll's (after the
      // nearer LOCO and ACTION atlases)
      if (a.player || (fighting && a.aggro)) want(art::RigPart::Death, 96.0f * 96.0f);
    }
    std::sort(q.begin(), q.end(), [](const SpriteCache::RigJob& x, const SpriteCache::RigJob& y) { return x.d2 < y.d2; });
    rigQueue_.swap(q);
  }
  // (Witness) a fixed count instead of the millisecond budget: how many atlases a step baked must not depend on CPU
  // speed or machine load (it showed as state.screen.peopleMB differing between `witness test --repeat 2` passes)
  int witnessBakes = 1;
  while (!rigQueue_.empty() && (v.witness ? witnessBakes-- > 0 : rigBakeBudget(v))) {
    const SpriteCache::RigJob j = rigQueue_.front();
    rigQueue_.erase(rigQueue_.begin());
    if (rigTex_.count(rigKey(j.look, j.part, j.child))) continue;
    (void)rigTex(v, j.look, j.part, j.child);
  }
  v.lensStats.rigBakeMs = rigBakeFrameMs_;
}

void View::rigChanged() {
  if (!pix_) return;
  auto drop = [&](std::unordered_map<uint64_t, Tex>& m) { for (auto& kv : m) pix_->destroy(kv.second); m.clear(); };
  for (auto& kv : sprites_.liftMask_) if (kv.second.tex.t) pix_->destroy(kv.second.tex);
  sprites_.liftMask_.clear();
  drop(sprites_.humans_); sprites_.humanUsed_.clear(); sprites_.humanBoxes_.clear();
  drop(sprites_.poseTex_); sprites_.poseUsed_.clear();
  drop(sprites_.monsterLooks_);
  drop(sprites_.bldgTex_); sprites_.bldgUsed_.clear(); drop(sprites_.bldgNight_); drop(sprites_.bldgGlow_); sprites_.bldgTopRow_.clear(); g_bldgColTop.clear(); sprites_.bldgSmoke_.clear();
  drop(sprites_.styledProps_); drop(sprites_.homeTex_); drop(sprites_.laneTex_); drop(sprites_.critterTex_); drop(sprites_.partyTex_); drop(sprites_.mythicAura_);
  drop(sprites_.lampDark_); drop(sprites_.warPropTex_); drop(sprites_.armsTex_);
  for (auto& kv : sprites_.rigTex_) { pix_->destroy(kv.second.tex); if (kv.second.glowTex.t) pix_->destroy(kv.second.glowTex); }
  sprites_.rigTex_.clear();
  sprites_.rigQueue_.clear();
  for (Tex& t : sprites_.monsters_) pix_->destroy(t);
  sprites_.monsters_.clear();
  for (int i = 0; i < (int)Monster::COUNT; i++) sprites_.monsters_.push_back(pix_->bake(art::monsterSheet((Monster)i)));
  for (Tex& t : sprites_.props_) pix_->destroy(t);
  sprites_.props_.clear();
  for (int i = 0; i < (int)Prop::COUNT; i++) sprites_.props_.push_back(pix_->bake(art::propSprite((Prop)i)));
}


const Tex* SpriteCache::poseTex(View& v, const art::HumanLook& L, art::Posture p, uint8_t variant, bool child) {
  const uint64_t k = ew::mix64(L.key() ^ ((uint64_t)p + 1) * 0xC2B2AE3D27D4EB4Full ^ (uint64_t)variant << 56 ^ (child ? 0x5EEDC41Dull : 0));
  poseUsed_[k] = v.t_;
  auto it = poseTex_.find(k);
  if (it != poseTex_.end()) return &it->second;
  if (!poseBudget(v)) return nullptr;
  const auto t0 = std::chrono::steady_clock::now();
  Canvas sheet = art::humanPostureSheet(L, p, variant);
  if (child) childBody(sheet, !art::postureInfo(p).seated);
  const Tex& t = poseTex_[k] = v.pix_->bake(sheet);
  liftMaskFor(v, sheet, t);
  poseSpent(v, t0);
  return &t;
}
const Tex* SpriteCache::sleeperTex(View& v, const art::HumanLook& L, art::Berth b, int kit, int frame) {
  const uint64_t k = ew::mix64(L.key() ^ 0x51EE9E5ull ^ ((uint64_t)b << 40) ^ ((uint64_t)(kit + 1) << 48) ^ ((uint64_t)(frame & 1) << 60));
  poseUsed_[k] = v.t_;
  auto it = poseTex_.find(k);
  if (it != poseTex_.end()) return &it->second;
  if (!poseBudget(v)) return nullptr;
  const auto t0 = std::chrono::steady_clock::now();
  const Canvas sc = art::sleeperSprite(L, b, kit, frame & 1);
  const Tex& t = poseTex_[k] = v.pix_->bake(sc);
  liftMaskFor(v, sc, t);
  poseSpent(v, t0);
  return &t;
}
// (M5) a village animal's sheet by kind and coat (variant: 16 coats at most per kind)
const Tex& SpriteCache::critterTex(View& v, int kind, uint32_t variant) {
  const uint64_t k = (uint64_t)kind << 8 | (variant & 15u);
  auto it = critterTex_.find(k);
  if (it != critterTex_.end()) return it->second;
  return critterTex_[k] = v.pix_->bake(art::critterSheet((art::Critter)kind, variant & 15u));
}
// (M5) a lamppost whose lamp is out (by day, or before the lamplighter's round reaches it): its sprite with every
// lit pixel (the glass, the flame, the glowing paper or orb: warm or pale and bright) turned to dark, cold glass with a
// faint top-left glint, so the same lamp reads unlit in the same light
const Tex& SpriteCache::lampDarkTex(View& v, const art::PropStyle* ps) {
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
      // (fixer M7b r1, review: "daytime lamp posts read as bare bollards") the unlit glass is a pale, sky-grey pane that
      // reads as a lantern head against the dark iron, not a dark blot that merges with its frame
      const float k2 = 0.44f + 0.14f * lum - 0.12f * t;   // darker toward the bottom (the light comes from above)
      int nr = (int)(255 * k2 * 0.80f), ng = (int)(255 * k2 * 0.88f), nb = (int)(255 * k2 * 1.12f);
      // the sky caught on the glass: its top-left pixels (the light comes from the top-left)
      const bool left = x == 0 || !mask[(size_t)y * c.w + x - 1], top = y == 0 || !mask[(size_t)(y - 1) * c.w + x];
      const bool top2 = y >= 2 && mask[(size_t)(y - 1) * c.w + x] && !mask[(size_t)(y - 2) * c.w + x];
      if (left && top) { nr += 70; ng += 78; nb += 86; }
      else if ((left && top2) || (top && x >= 1 && mask[(size_t)y * c.w + x - 1] && (x < 2 || !mask[(size_t)y * c.w + x - 2]))) { nr += 34; ng += 38; nb += 44; }
      c.set(x, y, rgba(std::min(255, nr), std::min(255, ng), std::min(255, nb), (int)(v >> 24)));
    }
  return lampDark_[k] = v.pix_->bake(c);
}

// Least-recently-used trimming of the sprite caches (called once per frame before anything is drawn, so no reference
// handed out earlier in the frame is invalidated): building sprites past 192 (with their night variant, smoke, window
// and fade facts), character sheets past 320. Only entries unused for a few seconds go.
void SpriteCache::trimCaches(View& v) {
  if (v.t_ - lruT_ < 1.0f) return;
  lruT_ = v.t_;
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
      if (v.t_ - age[i].first < 4.0f) break;   // still in use
      const uint64_t k = age[i].second;
      auto it = tex.find(k);
      if (it != tex.end()) { dropLiftMask(v, it->second); v.pix_->destroy(it->second); tex.erase(it); }
      used.erase(k);
      extra(k);
    }
  };
  trim(bldgTex_, bldgUsed_, 192, [&](uint64_t k) {
    auto n = bldgNight_.find(k);
    if (n != bldgNight_.end()) { v.pix_->destroy(n->second); bldgNight_.erase(n); }
    auto gl = bldgGlow_.find(k);
    if (gl != bldgGlow_.end()) { v.pix_->destroy(gl->second); bldgGlow_.erase(gl); }
    bldgSmoke_.erase(k); bldgTopRow_.erase(k); g_bldgColTop.erase(k); bldgWin_.erase(k);
  });
  trim(humans_, humanUsed_, 320, [](uint64_t) {});
  trim(poseTex_, poseUsed_, 160, [](uint64_t) {});   // (M5) pose sheets: a busy tavern holds a few dozen
  trimRigCaches(v);   // (P2a LENS) the v2 atlases: capped in MB
  if (lampDark_.size() > 64) { for (auto& kv : lampDark_) v.pix_->destroy(kv.second); lampDark_.clear(); }
  // (M6 BEASTS) variant monster sheets (and their glow and 2x bakes) and the auras: a few dozen at most in one place;
  // a long journey through many lands drops them all and re-bakes what is still on screen
  if (monsterLooks_.size() > 96) { for (auto& kv : monsterLooks_) v.pix_->destroy(kv.second); monsterLooks_.clear(); }
  if (auraTex_.size() > 48) { for (auto& kv : auraTex_) v.pix_->destroy(kv.second); auraTex_.clear(); }
  for (auto it = beasts::memo.begin(); it != beasts::memo.end();) {
    if (beasts::now - it->second.lastT > 20.0f) it = beasts::memo.erase(it); else ++it;
  }
  if (festTex_.size() > 96) { for (auto& kv : festTex_) v.pix_->destroy(kv.second); festTex_.clear(); }
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
              bake_.chunks_.size(), sprites_.bldgTex_.size(), sprites_.humans_.size(), sprites_.wallTiles_.size(), sprites_.styledProps_.size(), sprites_.kingdomTex_.size());
  std::fflush(stdout);
  perf_ = Perf();
}
const Tex& SpriteCache::itemTex(View& v, const Game& g, const Item& item) {
  const cult::Culture* maker = item.culture && g.world.src ? &g.world.src->culture(item.culture) : nullptr;
  const art::IconLook L = gearIcon(item, maker);
  const uint64_t k = L.key();
  auto it = itemIcons_.find(k);
  if (it != itemIcons_.end()) return it->second;
  return itemIcons_[k] = v.pix_->bake(art::itemIconLook(L));
}
const Tex& SpriteCache::monsterTex(View& v, const Actor& a) {
  if (!a.overlays && !a.bodyTint) return monsters_[(int)a.mon];
  const art::MonsterLook L = beasts::lookOf(a);
  const uint64_t k = L.key();   // (M6 BEASTS) bit 63: the glow sheet
  auto it = monsterLooks_.find(k);
  if (it != monsterLooks_.end()) return it->second;
  return monsterLooks_[k] = v.pix_->bake(art::monsterSheetLook(L));
}
const Tex& SpriteCache::auraTex(View& v, uint32_t color, int w) {
  const uint64_t k = (uint64_t)color << 16 | (uint64_t)(uint16_t)w;
  auto it = auraTex_.find(k);
  if (it != auraTex_.end()) return it->second;
  return auraTex_[k] = v.pix_->bake(art::auraSprite(color, w));
}
const Tex& SpriteCache::iconTex(View& v, art::Icon i, uint32_t tint) {
  uint64_t k = (uint64_t)i << 32 | tint;
  auto it = icons_.find(k);
  if (it != icons_.end()) return it->second;
  return icons_[k] = v.pix_->bake(art::itemIcon(i, tint));
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
uint64_t SpriteCache::bldgKey(const Map& m, const Bldg& b, int index) const {
  uint64_t k = bldgViewRequest(m, b).key();
  k ^= (uint64_t)index * 0x9E3779B97F4A7C15ull;
  return k;
}
// A building sprite and what the renderer reads off it (chimney mouths, the first opaque row, the lit-window night
// variant and its window centres). A pure function of the building (M2: an arrival paints them on worker threads,
// paintBldg; storeBldg turns them into textures on the main thread).
SpriteCache::BldgPaint SpriteCache::paintBldg(const Bldg& b, uint64_t key) {
  static const Map empty;
  art::BuildingInfo info;
  Canvas c = art::buildingSprite(bld::design(bldgViewRequest(empty, b)), &info);
  return paintBldgPost(std::move(c), info, key, b.seed);
}
SpriteCache::BldgPaint SpriteCache::paintBldgPost(Canvas canvas, art::BuildingInfo& info, uint64_t key, uint32_t seed) {
  SpriteCache::BldgPaint p;
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

const Tex& SpriteCache::storeBldg(View& v, SpriteCache::BldgPaint& p) {
  const uint64_t k = p.key;
  bldgSmoke_[k] = std::move(p.smoke);
  bldgTopRow_[k] = p.topRow;
  {
    std::vector<int16_t>& ct = g_bldgColTop[k];   // per column: the first opaque row and the last row of its run
    ct.assign((size_t)p.c.w * 2, (int16_t)-1);
    for (int x = 0; x < p.c.w; x++)
      for (int y = std::max(0, p.topRow - 1); y < p.c.h; y++)
        if ((p.c.get(x, y) >> 24) > 96) {
          int e = y;
          while (e + 1 < p.c.h && (p.c.get(x, e + 1) >> 24) > 96) e++;
          ct[(size_t)x * 2] = (int16_t)y;
          ct[(size_t)x * 2 + 1] = (int16_t)e;
          break;
        }
  }
  bldgWin_[k] = std::move(p.wins);
  {
    auto gl = bldgGlow_.find(k);
    if (gl != bldgGlow_.end()) { v.pix_->destroy(gl->second); bldgGlow_.erase(gl); }
    auto n = bldgNight_.find(k);
    if (n != bldgNight_.end()) { v.pix_->destroy(n->second); bldgNight_.erase(n); }
  }
  if (p.anyGlass) { bldgNight_[k] = v.pix_->bake(p.night); bldgGlow_[k] = v.pix_->bake(p.glow); }
  return bldgTex_[k] = v.pix_->bake(p.c);
}

// A building sprite, painted now if it is not ready (a job already under way is finished). Behind the web arrival's
// fade (no threads) a building is never painted in one go: it advances by a step within the budget and, while it is
// unfinished, a blank texture stands in and the arrival is held a frame longer (travelArrive lifts the fade only after
// two frames with nothing left to paint: arrival_.frames is wound back), so a palace is spread over frames.
const Tex& SpriteCache::bldgTex(View& v, const Bldg& b, int index) {
  static const Map empty;
  const Map& m = v.bldgMap_ ? *v.bldgMap_ : empty;
  uint64_t k = bldgKey(m, b, index);
  bldgUsed_[k] = v.t_;
  auto it = bldgTex_.find(k);
  if (it != bldgTex_.end()) return it->second;
  if (v.bake_.arriving_ && v.pix_) {
#ifdef __EMSCRIPTEN__
    const bool st = true;
#else
    static const bool st = std::getenv("EMB_WEBSIM") != nullptr;
#endif
    if (!st) {
      // desktop: behind the arrival's black the worker threads paint the buildings (travelArrive stores them as they
      // finish, and holds the fade until every one in view is there); drawing must never paint one in the meantime
      if (!blankTex_.t) { Canvas c(1, 1); blankTex_ = v.pix_->bake(c); }
      return blankTex_;
    }
    if (st) {
      // one budget per frame for every stepped paint (travelArrive and drawWorld both ask)
      static float frameT = -1;
      static double frameMs = 0;
      if (frameT != v.t_) { frameT = v.t_; frameMs = 0; }
      if (frameMs < 6.0) {
        const auto t0 = std::chrono::steady_clock::now();
        const bool done = bldgPaintStep(v, b, index, std::min(4.0, 6.0 - frameMs));
        frameMs += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
        if (done) return bldgTex_[k];
      }
      v.bake_.arrival_.frames = 0;
      if (!blankTex_.t) { Canvas c(1, 1); blankTex_ = v.pix_->bake(c); }
      return blankTex_;
    }
  }
  bldgPaintStep(v, b, index, 1e9);
  return bldgTex_[k];
}

// Advance (or start) the paint of b's sprite by about budgetMs; true once it is stored. Several jobs may be under way
// (one per building); a job not touched for a few seconds is dropped by the next call.
bool SpriteCache::bldgPaintStep(View& v, const Bldg& b, int index, double budgetMs) {
  static const Map empty;
  const Map& m = v.bldgMap_ ? *v.bldgMap_ : empty;
  const uint64_t k = bldgKey(m, b, index);
  if (bldgTex_.count(k)) return true;
  using Clock = std::chrono::steady_clock;
  const auto t0 = Clock::now();
  SpriteCache::BldgJob* J = nullptr;
  for (SpriteCache::BldgJob& j : bldgJobs_) if (j.key == k) { J = &j; break; }
  if (!J) {
    for (size_t i = 0; i < bldgJobs_.size();)
      if (v.t_ - bldgJobs_[i].used > 5.0f) bldgJobs_.erase(bldgJobs_.begin() + (std::ptrdiff_t)i); else i++;
    SpriteCache::BldgJob nj;
    nj.key = k;
    nj.seed = b.seed;
    nj.job = art::beginBuilding(bld::design(bldgViewRequest(empty, b)));
    bldgJobs_.push_back(std::move(nj));
    J = &bldgJobs_.back();
  }
  J->used = v.t_;
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
  SpriteCache::BldgPaint p = paintBldgPost(std::move(c), info, k, seed);
  storeBldg(v, p);
  bldgUsed_[k] = v.t_;
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
  // (M7) the player's own house (built or bought): a lamp is left burning for the owner until midnight, and while the
  // owner is about the farm (within ~14 tiles) the hearth glows on through the night
  if (b.home == 1 || b.home == 2) {
    const float hr = g.hour < 12 ? g.hour + 24 : g.hour;
    if (hr < 24.0f) return true;
    const float dx = g.pl().p.x / 16.0f - (b.r.x + b.r.w * 0.5f), dy = g.pl().p.y / 16.0f - (b.r.y + b.r.h * 0.5f);
    return dx * dx + dy * dy < 14.0f * 14.0f;
  }
  if (b.home == 3) return false;   // a building site has no windows yet
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
const Tex& SpriteCache::wallTileTex(View& v, uint32_t key) {
  const uint64_t ck = wallTexKey(key);
  auto it = wallTiles_.find(ck);
  if (it != wallTiles_.end()) return it->second;
  const uint32_t slot = (key & art::WALL_LOOK_MASK) >> art::WALL_LOOK_SHIFT;
  const Canvas c = slot && slot < fortLooks_.size() ? art::wallTile(key, fortLooks_[slot]) : art::wallTile(key & ~art::WALL_LOOK_MASK);
  return wallTiles_[ck] = v.pix_->bake(c);
}
// (M3b) a settlement's fortifications from its culture (urban: village 0, town 1, city 2, capital 3)
bool SpriteCache::fortPartsOfSite(const Game& g, int site, bld::FortParts& out) const {
  if (site < 0 || site >= (int)g.world.sites.size()) return false;
  const Site& s = g.world.sites[(size_t)site];
  if (!s.settlement()) return false;
  const cult::Culture* c = g.world.cultureOf(site);
  if (!c) return false;
  const int urban = s.capital ? 3 : (s.type == SiteType::City ? 2 : (s.type == SiteType::Town ? 1 : 0));
  out = bld::fortParts(*c, urban, s.seed);
  return true;
}
const Tex& SpriteCache::fortGateTex(View& v, const cult::Heraldry& arms, const bld::FortParts& f) {
  const uint64_t key = ew::mix64(arms.key() ^ (bld::fortKey(f) * 0x9E3779B97F4A7C15ull) ^ 0x6A7E5ull);
  auto it = kingdomTex_.find(key);
  if (it != kingdomTex_.end()) return it->second;
  return kingdomTex_[key] = v.pix_->bake(art::gateHouse(7, arms, f));
}
// (M3b) the culture of the land at a tile for the wayside's built props: its settlement's, else its region's
const art::PropStyle* SpriteCache::landStyleAt(View& v, const Game& g, int tx, int ty) {
  static const art::PropStyle classic;
  if (!g.world.endless) return &classic;
  const art::PropStyle* ps = propStyleAt(v, g, tx, ty);
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
const Tex& SpriteCache::kingdomTex(View& v, int kind, const Kingdom& k, uint32_t seed) {
  const uint64_t key = ew::mix64(((uint64_t)k.color << 32 | k.color2) ^ ((uint64_t)k.emblem << 8 | (uint64_t)kind) ^ ((uint64_t)seed << 40));
  auto it = kingdomTex_.find(key);
  if (it != kingdomTex_.end()) return it->second;
  Canvas c = kind == 0 ? art::kingdomBanner(k.color, k.color2, k.emblem) : art::gateHouse(seed, k.color, k.color2, k.emblem);
  return kingdomTex_[key] = v.pix_->bake(c);
}

// M3: the culture of the settlement a prop tile lies in decides its style (fences, wells, lamps, statues, stalls...).
// The site is looked up once per 8 x 8-tile block of the world (global tiles, so a window shift keeps the cache) and
// the style once per site; tiles outside settlements keep the classic look.
// (M5) the settlement (its Site::id) a tile of the overworld lies in, cached per 8 x 8-tile block of the world (0: none)
ew::Gid LifeView::siteIdAt(View& v, const Game& g, int tx, int ty) {
  if (!g.world.endless) return 0;
  const uint64_t wid = (uint64_t)g.world.seed * 0x9E3779B97F4A7C15ull ^ (uint64_t)(uintptr_t)&g.world;
  if (wid != v.sprites_.sitePropsWorld_ || v.sprites_.propSiteBlock_.size() > 8192) { v.sprites_.sitePropsWorld_ = wid; v.sprites_.siteProps_.clear(); v.sprites_.propSiteBlock_.clear(); }
  const int32_t gx = tx + g.world.ox, gy = ty + g.world.oy;
  const uint64_t bk = ((uint64_t)(uint32_t)(gx >> 3) << 32) | (uint32_t)(gy >> 3);
  auto bi = v.sprites_.propSiteBlock_.find(bk);
  if (bi != v.sprites_.propSiteBlock_.end()) return (ew::Gid)bi->second;
  int si = g.world.siteAt(tx, ty, 2);
  if (si >= 0 && !g.world.sites[(size_t)si].settlement()) si = -1;
  const uint64_t sid = si >= 0 ? (uint64_t)g.world.sites[(size_t)si].id : 0;
  v.sprites_.propSiteBlock_[bk] = sid;
  return (ew::Gid)sid;
}
// (M5) a settlement's mood flags where a tile lies (life::MF_*; 0 outside settlements), and the scripts' forcing
uint16_t LifeView::lifeFlagsAt(View& v, const Game& g, int tx, int ty) {
  if (g.mode == Mode::Title) return 0;
  const ew::Gid sid = siteIdAt(v, g, tx, ty);
  if (!sid) return 0;
  uint16_t f = g.life.find(sid) ? g.life.moodFlags(sid) : 0;
  if (forceFest_ >= 0) f = (uint16_t)(forceFest_ ? (f | life::MF_FESTIVAL) : (f & ~life::MF_FESTIVAL));
  if (forceShut_ >= 0) f = (uint16_t)(forceShut_ ? (f | life::MF_SHUTTERED) : (f & ~(life::MF_SHUTTERED | life::MF_FAMINE)));
  if (festivalHere(g, sid)) f |= life::MF_FESTIVAL;
  return f;
}
bool LifeView::festivalHere(const Game& g, ew::Gid site) {
  if (forceFest_ >= 0) return forceFest_ != 0;
  return site && g.life.find(site) && (g.life.festival(site, g.day) || (g.life.moodFlags(site) & life::MF_FESTIVAL));
}
const art::PropStyle* SpriteCache::propStyleAt(View& v, const Game& g, int tx, int ty) {
  static const art::PropStyle classic;
  if (!g.world.endless) return &classic;
  int si = -1;
  const uint64_t sid = (uint64_t)v.life_.siteIdAt(v, g, tx, ty);
  if (!sid) return &classic;
  auto it = siteProps_.find(sid);
  if (it != siteProps_.end()) return &it->second;
  if (si < 0)   // the block was cached: find the settlement by its id (rare: once per settlement)
    for (size_t k = 0; k < g.world.sites.size(); k++) if ((uint64_t)g.world.sites[k].id == sid) { si = (int)k; break; }
  if (si < 0) return &classic;   // not in the records just now: draw it plain this frame, ask again next time
  const cult::Culture* C = g.world.cultureOf(si);
  return &(siteProps_[sid] = C ? C->props : classic);
}
const Tex& SpriteCache::styledPropTex(View& v, art::Prop p, const art::PropStyle& st) {
  const uint64_t k = ew::mix64(st.key() ^ ((uint64_t)p << 48) ^ 0x5717ull);
  auto it = styledProps_.find(k);
  if (it != styledProps_.end()) return it->second;
  return styledProps_[k] = v.pix_->bake(art::propSprite(p, st));
}
// a kingdom's arms (cult::Heraldry) on a standing banner (kind 0) or a gatehouse in a wall style (kind 1)
const Tex& SpriteCache::heraldryTex(View& v, int kind, const cult::Heraldry& h, uint32_t seed, int wallStyle) {
  const uint64_t key = ew::mix64(h.key() ^ ((uint64_t)kind << 60) ^ ((uint64_t)seed << 32) ^ ((uint64_t)wallStyle << 40) ^ 0x4E5Aull);
  auto it = kingdomTex_.find(key);
  if (it != kingdomTex_.end()) return it->second;
  Canvas c = kind == 0 ? art::bannerSprite(h)
                       : art::gateHouse(seed, h.field, h.charge, h.emblem, (art::CityWall)std::clamp(wallStyle, 0, (int)art::CityWall::COUNT - 1));
  return kingdomTex_[key] = v.pix_->bake(c);
}

void View::snap(Game& g) {
  const Actor& p = g.pl();
  const float VW = (float)viewW(), VH = (float)viewH();   // (P2a) the world view, in world px
  camera_.cam_ = Vec2(p.p.x - VW / 2.0f, p.p.y - 10 - VH / 2.0f);
  const Map& m = g.map();
  float mw = m.w * 16.0f, mh = m.h * 16.0f;
  if (mw > VW) camera_.cam_.x = clampf(camera_.cam_.x, 0, mw - VW); else camera_.cam_.x = (mw - VW) / 2;
  if (mh > VH) camera_.cam_.y = clampf(camera_.cam_.y, 0, mh - VH); else camera_.cam_.y = (mh - VH) / 2;
  camera_.lensD_ = 0;   // (a snap is not a zoom: nothing to hand over)
  s_camRes = Vec2(); s_camPrevOk = false;   // (P2a slice) a cut: no eased fraction or camera speed carried over
}

// ------------------------------------------------------------------ events -> particles / audio / UI
void FxState::spawnParticles(View& v, const Event& e, Game& g) {
  Rng r((uint32_t)(v.t_ * 1000) ^ (uint32_t)(e.p.x * 7 + e.p.y * 13));
  auto add = [&](Vec2 p, Vec2 v, float life, Color c, float size, float grav) {
    Particle q; q.p = p; q.v = v; q.life = q.max = life; q.c = c; q.size = size; q.grav = grav; parts_.push_back(q);
  };
  auto addFx = [&](art::Fx f, Vec2 p, float life) {
    Particle q; q.p = p; q.life = q.max = life; q.kind = 1; q.fx = (int)f; parts_.push_back(q);
  };
  switch (e.type) {
    case Ev::Sfx: v.audio_->play((Sfx)e.a, e.f, e.vol); mfxNoteSfx(v, e.a); break;   // (M9: the magic's voice won't double it)
    case Ev::Hit:
      for (int i = 0; i < 6; i++) add(e.p, Vec2(r.range(-70, 70), r.range(-80, 20)), 0.25f, Color(1, 1, 0.85f), 1, 200);
      break;
    case Ev::Blood:
      for (int i = 0; i < 7; i++) add(e.p, Vec2(r.range(-50, 50), r.range(-70, 0)), 0.45f, Color(0.7f, 0.08f, 0.08f), r.f() < 0.5f ? 2.f : 1.f, 260);
      break;
    case Ev::Explode:
      if (mfxClaimExplode(v, e.p)) break;   // (M9) a classic FIREBOLT's burst: the magic's own impact is there (render_magic.cpp)
      addFx(art::Fx::Explosion, e.p, 0.45f);
      for (int i = 0; i < 14; i++) add(e.p, Vec2(r.range(-90, 90), r.range(-100, 40)), 0.5f, Color(1, r.range(0.4f, 0.8f), 0.2f), 1.5f, 120);
      shake_ = std::max(shake_, 2.0f * e.f);
      break;
    case Ev::Sparkle: for (int i = 0; i < 8; i++) add(e.p, Vec2(r.range(-30, 30), r.range(-50, -10)), 0.7f, Color(1, 0.95f, 0.6f), 1, 0); break;
    case Ev::Dust: addFx(art::Fx::Dust, e.p + Vec2(r.range(-3, 3), -2), 0.3f); break;
    case Ev::Heal: addFx(art::Fx::Heal, e.p + Vec2(0, -10), 0.6f); for (int i = 0; i < 10; i++) add(e.p + Vec2(r.range(-8, 8), r.range(-16, 0)), Vec2(0, r.range(-40, -15)), 0.8f, Color(0.5f, 1, 0.6f), 1, 0); break;
    case Ev::Frost: addFx(art::Fx::Frost, e.p + Vec2(0, -6), 0.35f); break;
    case Ev::Text: {
      if (e.s.empty()) {   // (P1 fixer r2) a hush: the speaker's plate taken down (life_game.cpp hush)
        if (e.id)
          texts_.erase(std::remove_if(texts_.begin(), texts_.end(), [&](const FloatText& t) { return t.speech && t.actor == (int)e.id - 1; }), texts_.end());
        break;
      }
      FloatText ft; ft.p = e.p + Vec2(r.range(-4, 4), 0); ft.s = e.s; ft.c = col((uint32_t)e.a);
      // (M5) words (an overheard remark, a greeting by name, a plea) are a spoken line: held over the speaker long
      // enough to read on a phone (about 2 s plus a beat per word), on a dark plate; numbers and one-word calls still
      // rise and fade
      // (P1 fixer) a line that trails in ("...OLD BRANDR WANTS THE WOLVES CULLED...": overheard gossip) is spoken too: it
      // was drawn as rising grey text with no plate, through the E: TALK label
      if (e.s.size() >= 8 && e.s.find(' ') != std::string::npos && ((e.s[0] >= 'A' && e.s[0] <= 'Z') || e.s[0] == '"' || e.s[0] == 0x27 || (e.s[0] == '.' && e.s.size() > 3 && e.s[3] >= 'A' && e.s[3] <= 'Z'))) {
        ft.speech = true;
        ft.p = e.p;
        // (P1 fixer r2) a line with its speaker (Event::id = actor id + 1, life_game.cpp say): the plate rides over them
        if (e.id)
          if (const Actor* sp = speakerOf(g, (int)e.id - 1)) {
            ft.actor = sp->id; ft.off = e.p - sp->p;
            // (P2a) the sim places lines over a v1 head: a speaker drawn on v2 is that much taller
            // (P2b VIEW-B) and by the doll frame's own top (a child, a sitter, a hat)
            if (sp->human) { const float lift = (float)(v.rigFor(sp->look).headTop - art::rig(1).headTop) + v.topLift(*sp); ft.off.y -= lift; ft.p.y -= lift; }
          }
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
          if (texts_[k].speech && ((ft.actor >= 0 && texts_[k].actor == ft.actor) ||
                                   (std::fabs(texts_[k].p.x - ft.p.x) < 10 && std::fabs(texts_[k].p.y - ft.p.y) < 14)))
            texts_.erase(texts_.begin() + (std::ptrdiff_t)k);
          else k++;
      }
      texts_.push_back(ft);
      break;
    }
    case Ev::Discover: {
      banner_ = "DISCOVERED"; bannerSub_ = e.s; bannerT_ = 4.0f; v.realm_.bannerState_.clear();
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
      banner_ = "LEVEL UP"; bannerSub_ = "LEVEL " + std::to_string(e.a) + "  -  CHOOSE A STAT IN THE MENU"; bannerT_ = 4.5f; v.realm_.bannerState_.clear();
      // a bigger moment: a layered fanfare, two rings of light and a column of rising sparks
      v.audio_->play(Sfx::LevelUp, 0.5f, 1.2f);
      v.audio_->play(Sfx::QuestDone, 1.5f, 0.8f);
      v.audio_->play(Sfx::Discover, 0.75f, 0.7f);
      for (int i = 0; i < 32; i++) { float a = i / 32.0f * TAU; add(e.p + Vec2(0, -10), Vec2(std::cos(a) * 75, std::sin(a) * 48), 1.0f, Color(1, 0.85f, 0.3f), 2.0f, 0); }
      for (int i = 0; i < 20; i++) { float a = i / 20.0f * TAU; add(e.p + Vec2(0, -10), Vec2(std::cos(a) * 38, std::sin(a) * 24), 0.8f, Color(1, 1, 0.8f), 1.0f, 0); }
      for (int i = 0; i < 26; i++) add(e.p + Vec2(r.range(-9, 9), r.range(-4, 2)), Vec2(r.range(-6, 6), r.range(-90, -40)), r.range(0.8f, 1.4f), Color(1, r.range(0.75f, 0.95f), 0.35f), 1, -20);
      shake_ = std::max(shake_, 3.0f);
      break;
    case Ev::QuestUpdate: {
      // the same line already showing as the centre notice (Game::say) is not repeated as a toast, nor a line the
      // banner shows whole (M7 fix: "YOUR HOUSE IS FINISHED" twice)
      const bool bannerWhole = (e.f == 0 || e.f == 1) && e.s.find(':') == std::string::npos;
      // (P1 fixer r2) a quest done: the centre banner says QUEST COMPLETE over its title, so no toast repeats it, and its
      // own QUEST STARTED line still up goes (a street errand starts and ends in a breath: three toasts and a banner
      // for one caught thief)
      bool bannerSays = false;
      if (e.f == 1 && e.s.rfind("QUEST COMPLETE: ", 0) == 0) {
        bannerSays = true;
        const std::string started = "QUEST STARTED: " + e.s.substr(16), added = "NEW QUEST: " + e.s.substr(16);   // (an untracked one)
        toasts_.erase(std::remove_if(toasts_.begin(), toasts_.end(), [&](const Toast& t) { return t.s == started || t.s == added; }), toasts_.end());
      }
      if (!(g.noticeT > 0 && g.notice == e.s) && !bannerWhole && !bannerSays) { Toast t; t.s = e.s; t.c = e.f == 1 ? Color(1, 0.85f, 0.3f) : Color(0.9f, 0.85f, 0.7f); toasts_.push_back(t); }
      if (e.f == 0 || e.f == 1) { v.realm_.bannerState_.clear(); banner_ = e.f == 1 ? "QUEST COMPLETE" : "NEW QUEST"; bannerSub_ = e.s.substr(e.s.find(':') == std::string::npos ? 0 : e.s.find(':') + 2); bannerT_ = 3.5f; }
      break;
    }
    case Ev::Notice: {
      if (!(g.noticeT > 0 && g.notice == e.s)) { Toast t; t.s = e.s; t.c = col((uint32_t)e.a); v.loot_.lootNotice(v, g, e, t); toasts_.push_back(t); }
      break;
    }
    case Ev::Shake: shake_ = std::max(shake_, e.f); break;
    case Ev::Border: v.realm_.heraldEvent(v, g, e); break;   // (M4) the herald at a kingdom's border (realm_hud.cpp)
    case Ev::News: v.realm_.newsEvent(v, g, e); break;       // (M4) a world event heard of
    case Ev::Banter: case Ev::PartyAlert: v.party_.partyEvent(g, e); break;   // (M8) a companion speaks / an alert (party_hud.cpp)
    case Ev::Power: v.powerEvent(g, e); break;                         // (M8b) a worn power fired (render_powers.cpp)
    case Ev::Spell: v.magicEvent(g, e); break;                         // (M9) a phase of a spell's life (render_magic.cpp)
    case Ev::Skill: v.skillEvent(g, e); break;                         // (M9) a skill moment for the HUD (skills_ui.cpp)
    case Ev::Sea: v.seaEvent(g, e); break;                             // (M10) a moment at sea (render_sea.cpp)
    case Ev::Rise: v.riseEvent(g, e); break;                           // (M12) the player's place in history (render_rise.cpp)
    case Ev::MapChange: v.fade_ = 1.0f; v.snap(g); break;
    case Ev::WindowShift:   // M1: the endless window moved; everything overworld moved by e.p pixels
      v.camera_.cam_ += e.p;
      for (Particle& q : parts_) if (q.world) q.p += e.p;
      for (FloatText& ft : texts_) ft.p += e.p;
      for (View::PowerFx& f : v.powerFx_) { f.p += e.p; f.q += e.p; }   // (M8b) a burst, star mark or arc in flight moves too
      v.magicShift(e.p);                                           // (M9) spell effects and decals
      v.seaShift(e.p);                                             // (M10) wakes and splashes
      v.reliefShift(e.p);                                          // (M11) mist and spray
      break;
    default: break;
  }
}

void View::update(Game& g, float dt) {
  // M2: the arrival's terrain, behind the fade (M2 fixer round 2: not while paused: finishing the journey then set the
  // fade to black with no game update to lift it)
  if (g.travel.phase == TravelPhase::Arrive) { if (g.mode == Mode::Play) travelArrive(g); }
  else if (bake_.arriving_) { bake_.arriving_ = false; bake_.arrival_ = TerrainBaker::Arrival(); }   // (the journey ended without us: headless, a death)
  t_ += dt;
  perfTick(dt);
  modeT_ += dt;
  if (g.mode != lastMode_) {
    modeT_ = 0;
    if (g.mode == Mode::Dialogue) { dlgChars_ = 0; dlgSel_ = 0; dlgFirst_ = 0; }
    if (g.mode == Mode::Shop) { shopSide_ = 0; shopSel_ = 0; shopArm_ = -1; }
    lastMode_ = g.mode;
  }
  for (const Event& e : g.events) fx_.spawnParticles(*this, e, g);
  g.events.clear();
  party_.partyUpdate(*this, g, dt);   // (M8) the party's portraits, radial, alerts, banter and the hound's howl (party_hud.cpp)
  powerUpdate(g, dt);   // (M8b) the powers' effects age (render_powers.cpp)
  magicUpdate(g, dt);   // (M9) the spells' effects age (render_magic.cpp)
  seaUpdate(g, dt);     // (M10) wakes, splashes, rain, lightning (render_sea.cpp)
  reliefUpdate(g, dt);  // (M11) the falls' mist and spray, their roar (render_relief.cpp)
  riseUpdate(g, dt);    // (M12) the herald's banners of the player's rise (render_rise.cpp)
  loot_.lootUpdate(*this, g, dt);    // (M8b) the loot chime for a fine piece in view, a Mythic wearer's embers (loot_ui.cpp)
  updateBanners(g);   // (M1, M4) the arrival banner
  if (realm_.herald_.t > 0 && g.mode == Mode::Play) realm_.herald_.t -= dt;
  if (fx_.toasts_.size() > 5) fx_.toasts_.erase(fx_.toasts_.begin(), fx_.toasts_.begin() + (fx_.toasts_.size() - 5));
  float VW = 0, VH = 0;   // the world view (VW x VH world px), as lensUpdate left it
  const bool camHeld = camera_.updateCamera(*this, g, dt, VW, VH);
  updateBeastCues(g, dt);   // (M6 BEASTS)
  fx_.updateShake(*this, g, dt, camHeld);
  fx_.updateEffects(*this, g, dt);   // particles, floating texts, toasts, banners, the dialogue's typing

  updateMusic(g, dt);

  const bool outdoors = g.mode != Mode::Title && !g.inside;
  updateBiomeBanner(g, dt, outdoors);   // (M3c LIFE)
  updateWeather(g, dt, outdoors);
  updateAmbience(g, outdoors);
  updateDrift(g, dt, outdoors, VW, VH);
}

// (M1) arriving in a settlement: the banner (update)
void View::updateBanners(Game& g) {
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
    const bool changed = cs >= 0 && cs == realm_.arriveSite_ && (own != realm_.arriveOwner_ || flags != realm_.arriveFlags_);
    if (cs != realm_.arriveSite_ || changed) {
      if (cs >= 0) {
        const Site& S = g.world.sites[(size_t)cs];
        std::string line, state;
        uint32_t stc = 0;
        realm_.arrivalText(g, cs, line, state, stc);
        const bool arrival = fx_.banner_.find(" OF ") != std::string::npos || fx_.banner_.find(" VILLAGE") != std::string::npos ||
                             fx_.banner_.find(" TOWN") != std::string::npos || fx_.banner_.find(" CITY") != std::string::npos;
        if (!line.empty()) {
          if (fx_.bannerT_ > 0 && fx_.banner_ == "DISCOVERED") { fx_.banner_ = "DISCOVERED  -  " + line; realm_.bannerState_ = state; realm_.bannerStateCol_ = stc; }
          else if (fx_.bannerT_ <= 0 || arrival || changed) {
            fx_.banner_ = line; fx_.bannerSub_ = S.name; fx_.bannerT_ = changed ? 4.0f : 3.5f;
            realm_.bannerState_ = state; realm_.bannerStateCol_ = stc;
            // (fixer M4 r1, review: three bands stacked on arrival) the arrival banner names the kingdom: the border
            // herald's plaque gives way to it rather than stacking above it
            realm_.herald_.t = 0;
          }
        }
      } else if (realm_.arriveSite_ >= 0 && realm_.arriveSite_ < (int)g.world.sites.size() && fx_.bannerT_ > 0.5f &&
                 fx_.bannerSub_ == g.world.sites[(size_t)realm_.arriveSite_].name) {
        // (fixer M4 r2) left the place (a teleport, a quick walk out): its arrival banner fades out now instead of
        // naming a town the hero no longer stands in
        fx_.bannerT_ = 0.5f;
      }
      realm_.arriveSite_ = cs;
      realm_.arriveOwner_ = own;
      realm_.arriveFlags_ = flags;
    }
  }
}

// the camera: the follow, the room framing, the title drift, the pixel rounding and the hold, as one unit (update)
bool Camera::updateCamera(View& v, Game& g, float dt, float& VW, float& VH) {
  // camera follows with a little lead in the aim direction (P2a: framing the world view, VW x VH world px)
  // (P2b `camhold`, Witness reset param camera:"hold": the same flag) the camera stays where it was drawn: no follow, no
  // zoom recentring, no shake (a window shift still carries it with the world, a snap or a map change still cuts)
  const bool camHeld = v.p2b.camHold && g.mode != Mode::Title;
  const Vec2 heldCam = cam_;
  cam_ += s_camRes;   // (P2a slice) the eased camera again: the draw read it rounded (rvSnapCamera, below)
  s_camRes = Vec2();
  v.lensUpdate(g, dt);
  VW = (float)v.viewW(); VH = (float)v.viewH();
  if (g.mode != Mode::Title) {
    const Actor& p = g.pl();
    Vec2 want(p.p.x - VW / 2.0f + p.aim.x * 10, p.p.y - 10 - VH / 2.0f + p.aim.y * 6);
    if (g.mode == Mode::Build) v.home_.buildCamWant(v, g, want);   // (M7) the build ghost in view, clear of the panel
    // (M7) the ridden horse's hooves: the walk's even four-beat, the gallop's three quick beats and a pause
    {
      float& strideT = v.strideT_;
      const float sp = len(p.vel);
      if (g.mode == Mode::Play && g.homeRiding() && !g.inside && p.st == AState::Walk && sp > 6.0f) {
        const bool gal = sp > home::WALK_SPEED_PX * 1.3f;
        const float stride = gal ? 0.40f : 0.62f;
        static const float kWalk[4] = {0.0f, 0.25f, 0.5f, 0.75f}, kGal[3] = {0.0f, 0.17f, 0.32f};
        const float before = strideT;
        strideT = std::fmod(strideT + dt / stride, 1.0f);
        const float* beats = gal ? kGal : kWalk;
        const int nb = gal ? 3 : 4;
        for (int b = 0; b < nb; b++) {
          const float at = beats[b];
          const bool crossed = before <= strideT ? (at > before && at <= strideT) : (at > before || at <= strideT);
          if (crossed) v.audio_->play(Sfx::Hoof, 0.92f + 0.04f * (float)b + (gal ? 0.06f : 0.0f), gal ? 0.55f : 0.42f);
        }
      } else strideT = 0;
    }
    cam_ += (want - cam_) * std::min(1.0f, dt * 6.0f);
    const Map& m = g.map();
    float mw = m.w * 16.0f, mh = m.h * 16.0f;
    if (m.kind == MapKind::Interior) {
      // M0b fix round: indoors the HUD (vitals and purse top-left, minimap and quest column top-right, the touch
      // buttons bottom-right) must never hide part of a room for good. A room that fits clear of the HUD stays put
      // (centred, or centred in the free area); one that doesn't pans with the player far enough that every corner
      // can be brought out from under the HUD.
      float padT = 46.0f + Pix::ST, padR = 134.0f + Pix::SR, padB = (v.touchUI ? 48.0f : 0.0f) + Pix::SB, padL = (float)Pix::SL;
      // (M7) decorating: the HUD is away and the catalogue panel covers the right side (buildmode.cpp sideLay)
      if (g.mode == Mode::Build && g.home.ui.mode == home::UiMode::Decorate) {
        padT = 18.0f + Pix::ST; padB = 4.0f + Pix::SB; padR = (v.touchUI ? 190.0f : 176.0f) + 10.0f + Pix::SR;
      }
      {   // (P2a) the HUD's bands are UI px from the UI canvas's edges: as world px from the world view's edges
        float wp[4];
        uiInsetsToWorld(v, padL, padT, padR, padB, wp);
        padL = std::max(0.0f, wp[0]); padT = std::max(0.0f, wp[1]); padR = std::max(0.0f, wp[2]); padB = std::max(0.0f, wp[3]);
      }
      auto axis = [](float& c, float want, float lo, float hi, float scr, float pa, float pb) {
        float len = hi - lo;
        if (len <= scr - 2 * std::max(pa, pb)) c = lo - (scr - len) / 2;             // fits clear, centred
        else if (len + pa + pb <= scr) c = lo - pa - (scr - pa - pb - len) / 2;      // fits in the free area
        else c = clampf(want, lo - pa, hi - scr + pb);                               // pans with the player
      };
      float wantX = cam_.x, wantY = cam_.y;
      axis(cam_.x, wantX, 0, mw, VW, padL, padR);
      axis(cam_.y, wantY, -16, mh, VH, padT, padB);   // the back wall rises a row above the map
    } else {
      if (mw > VW) cam_.x = clampf(cam_.x, 0, mw - VW); else cam_.x = (mw - VW) / 2;
      if (mh > VH) cam_.y = clampf(cam_.y, 0, mh - VH); else cam_.y = (mh - VH) / 2;
    }
  } else {
    // title: slow drift across the start region
    const Site& home = g.world.sites[g.world.startSite];
    Vec2 c0(home.r.cx() * 16.0f - VW / 2.0f - 300, home.r.cy() * 16.0f - VH / 2.0f - 120);
    if (v.titleT_ == 0 || v.titleT_ > 90) { cam_ = c0; v.titleT_ = 0.001f; }
    v.titleT_ += dt;
    cam_ = c0 + Vec2(v.titleT_ * 8.0f, v.titleT_ * 2.5f);
  }
  if (kCamSnap) {   // (P2a slice) the camera rounded once a frame, with the hero's phase on an axis that follows him
    const Vec2 eased = cam_;
    cam_ = rvSnapCamera(eased, g.mode != Mode::Title ? g.pl().p : eased, g.mode != Mode::Title, dt);
    s_camRes = eased - cam_;
  }
  if (camHeld) { cam_ = heldCam; s_camRes = Vec2(); }
  return camHeld;
}

// (M6 BEASTS) the foes' cues the view plays (update)
void View::updateBeastCues(Game& g, float dt) {
  // (M6 BEASTS) what the view hears and shows of the foes' moves: the harpy's shriek as it rears for the swoop, the
  // golem's slam (a shake and dust), the sting when an elite or a champion first comes into view, a world boss's roar
  // when it turns on the player, and a boss passing a phase (a roar, a shake, a white-hot flash). The sim may also play
  // these Sfx itself (Game::sfx); the view's are keyed on what it sees, once per foe and move.
  beasts::now += dt;
  if (g.mode == Mode::Play) {
    const Vec2 pp = g.pl().p;
    for (const Actor& a : g.actors) {
      if (a.player || a.critter > 0 || !a.hostile) continue;
      const float d2 = len2(a.p - pp);
      if (d2 > 360.0f * 360.0f) continue;
      beasts::Memo& mm = beasts::memo[a.id];
      mm.lastT = beasts::now;
      if (mm.flashT > 0) mm.flashT -= dt;
      if (mm.shriekT > 0) mm.shriekT -= dt;
      const float vol = clampf(1.15f - std::sqrt(d2) / 280.0f, 0.15f, 1.0f);
      const uint8_t st = (uint8_t)a.st;
      if (st != mm.st && mm.st != 255 && !a.human) {
        if (a.mon == Monster::Harpy && a.st == AState::Windup && mm.shriekT <= 0) {
          audio_->play(Sfx::HarpyShriek, 0.92f + (a.id % 7) * 0.025f, vol);
          mm.shriekT = 2.5f;
        }
        if (a.mon == Monster::Golem && a.st == AState::Strike) {
          audio_->play(Sfx::GolemSlam, a.heavy ? 1.0f : 1.3f, vol * (a.heavy ? 1.0f : 0.55f));
          if (a.heavy) {
            fx_.shake_ = std::max(fx_.shake_, 3.5f * vol);
            const Vec2 hit = a.p + a.aim * 14.0f;
            for (int k = 0; k < 6; k++) {
              Particle q;
              q.p = hit + Vec2(std::cos(k * 1.05f) * 10.0f, std::sin(k * 1.05f) * 5.0f);
              q.life = q.max = 0.4f; q.kind = 1; q.fx = (int)art::Fx::Dust;
              fx_.parts_.push_back(q);
            }
          }
        }
      }
      mm.st = st;
      if (a.st == AState::Dead) continue;
      if (!mm.seen && beasts::marked(a) && d2 < 210.0f * 210.0f) {   // an elite comes into view (a pack stings once)
        mm.seen = true;
        if (beasts::now - beasts::stingT > 3.0f) {
          audio_->play(Sfx::EliteSting, beasts::isRank(a, foes::Rank::Named) ? 0.84f : 1.0f, 0.75f);
          beasts::stingT = beasts::now;
        }
      }
      if (beasts::isRank(a, foes::Rank::WorldBoss) && a.aggro && !mm.roared) {
        mm.roared = true;
        audio_->play(Sfx::BossRoar, 1.0f, 1.0f);
        fx_.shake_ = std::max(fx_.shake_, 4.0f);
      }
      if (a.bossPhase > mm.phase) {   // a boss passed a phase mark
        mm.phase = a.bossPhase;
        mm.flashT = 0.45f;
        fx_.shake_ = std::max(fx_.shake_, 4.5f);
        audio_->play(Sfx::BossRoar, 1.12f, 0.85f);
      }
    }
  }
}

// the screen shake and the finisher's camera nudge (update)
void FxState::updateShake(View& v, Game& g, float dt, bool camHeld) {
  shake_ = std::max(0.0f, shake_ - dt * 12);
  shakeOff_ = shake_ > 0.1f && !camHeld ? Vec2(std::sin(v.t_ * 91) * shake_, std::cos(v.t_ * 77) * shake_ * 0.7f) : Vec2();
  // (P2c SWITCH, delight) the camera nudge on a heavy finisher's impact (AnimPick::impact: hit 3 of the greatsword, axe and
  // mace, FOLK-SIM-C's flag): the view leans 2 px the way the blow lands and settles back on a half sine over 0.18 s, a
  // little shorter or longer each time (never a shake, never lockstep with the swing's frames). The hero's own blows and a
  // foe's landing within 64 px of the hero; rig 2 only, never while the camera is held.
  if (art::rigVersion() >= 2 && g.mode == Mode::Play && !camHeld) {
    const Actor& pl = g.pl();
    bool hit = false;
    Vec2 dir;
    auto faceDir = [](int f) { return f == 0 ? Vec2(0, 1) : f == 1 ? Vec2(0, -1) : f == 2 ? Vec2(1, 0) : Vec2(-1, 0); };
    for (const Actor& a : g.actors) {
      if (!(a.player || a.human) || a.st != AState::Strike) continue;
      if (!a.player && len2(a.p - pl.p) > 64.0f * 64.0f) continue;
      if (animPick(g, a, 2).impact) { hit = true; dir = faceDir(a.face); if (a.player) break; }
    }
    if (hit && !v.camera_.nudgeWas_) { v.camera_.nudgeT_ = 0; v.camera_.nudgeDir_ = dir; v.camera_.nudgeLen_ = 0.16f + 0.04f * (float)((hash32((uint32_t)(v.t_ * 1000.0f)) % 100u) / 100.0f); }
    v.camera_.nudgeWas_ = hit;
    if (v.camera_.nudgeT_ >= 0) {
      v.camera_.nudgeT_ += dt;
      if (v.camera_.nudgeT_ >= v.camera_.nudgeLen_) v.camera_.nudgeT_ = -1;
      else shakeOff_ += v.camera_.nudgeDir_ * (2.0f * std::sin(3.14159265f * v.camera_.nudgeT_ / v.camera_.nudgeLen_));
    }
  }
}

// particles, floating texts and toasts age; the banners, the fade and the dialogue's typing (update)
void FxState::updateEffects(View& v, Game& g, float dt) {
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
    else if (texts_[i].t < 0.25f) { texts_[i].p.y -= dt * 12; texts_[i].off.y -= dt * 12; }   // (M5) a spoken line lifts a few pixels, then holds
    if (texts_[i].speech && texts_[i].actor >= 0) {   // (P1 fixer r2) over its speaker wherever they go (gone: it stays put)
      const Actor* sp = speakerOf(g, texts_[i].actor);
      if (sp && sp->st != AState::Dead) texts_[i].p = sp->p + texts_[i].off;
      else texts_[i].actor = -1;
    }
    if (texts_[i].t > texts_[i].life) texts_.erase(texts_.begin() + i); else i++;
  }
  // toasts wait while a dialogue or menu is open, so "OLD BLADE" is still there when the player looks up
  const bool modal = g.mode == Mode::Dialogue || g.mode == Mode::Shop || g.mode == Mode::Menu || g.mode == Mode::LevelUp || g.mode == Mode::Paused ||
                     g.mode == Mode::Forge || g.mode == Mode::Build;
  for (size_t i = 0; i < toasts_.size();) {
    if (modal) { i++; continue; }
    toasts_[i].t += dt;
    if (toasts_[i].t > 4.0f) toasts_.erase(toasts_.begin() + i); else i++;
  }
  if (bannerT_ > 0 && g.mode != Mode::Dialogue) bannerT_ -= dt;   // shown once the dialogue closes (drawHud)
  if (v.fade_ > 0) v.fade_ = std::max(0.0f, v.fade_ - dt * 2.2f);
  {
    float before = v.dlgChars_;
    v.dlgChars_ += dt * 70;
    if (g.mode == Mode::Dialogue && (int)(before / 3) != (int)(v.dlgChars_ / 3) && v.dlgChars_ < (float)g.dlg.text.size()) {
      size_t ci = std::min(g.dlg.text.size() - 1, (size_t)v.dlgChars_);
      if (g.dlg.text[ci] != ' ') v.audio_->play(Sfx::Talk, 0.7f + (hash32((uint32_t)std::hash<std::string>()(g.dlg.speaker)) % 80) / 100.0f, 0.8f);
    }
  }
}

// the music director (update)
void View::updateMusic(Game& g, float dt) {
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
    else if (realm_.nearSiege(g)) want = Music::Wild;   // (M4) a besieged town has no jig: the land's music in the Ominous mood
    else if (g.curSite >= 0 && (g.world.sites[g.curSite].type == SiteType::City || g.world.sites[g.curSite].type == SiteType::Town || g.world.sites[g.curSite].type == SiteType::Village)) want = Music::Town;
    else if (g.isNight()) want = Music::Night;
  }
  // (M5) the life of the place: the bard's piece in the gathering place (or faintly from its door at night), the
  // festival's music on the plaza, the crowd, the animals, the alarm bell through a night raid
  bool festive = false;
  want = life_.lifeMusic(*this, g, want, dt, festive);
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
  if (want != music_ || style != musicStyleOn_ || festive != life_.musicFestive_) {
    music_ = want;
    musicStyleOn_ = style;
    life_.musicFestive_ = festive;
    const MusicStyle ms = MusicStyle::unpack(style);
    audio_->setMusic(want, style ? &ms : nullptr, festive);
  }
  life_.m5Stats_.music = (int)want;
}

// (M3c LIFE) the land under the player and its biome banner (update)
void View::updateBiomeBanner(Game& g, float dt, bool outdoors) {
  // (M3c LIFE) the land under the player (its eco), looked up a few times a second
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
      // the key is mark_tags.h markKey(kBiomeSeenIdBase + eco, kBiomeSeenTag = 8))
      const uint64_t key = gsim::markKey(gsim::kBiomeSeenIdBase + (uint64_t)e, (gsim::Mk)gsim::kBiomeSeenTag);
      // (M3c fixer, review: a stale or wrong banner) a banner still waiting behind a big one, or not yet shown, gives
      // way to the land the player now stands in (its biome stays unmarked, so it greets them next time)
      if (biomeBannerT_ >= 3.9f && biomeBannerEco_ >= 0 && biomeBannerEco_ != e) {
        g.marks.erase(gsim::markKey(gsim::kBiomeSeenIdBase + (uint64_t)biomeBannerEco_, (gsim::Mk)gsim::kBiomeSeenTag));
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
      if (biomeBannerT_ >= 3.9f) g.marks.erase(gsim::markKey(gsim::kBiomeSeenIdBase + (uint64_t)biomeBannerEco_, (gsim::Mk)gsim::kBiomeSeenTag));
      biomeBannerT_ = biomeBannerT_ >= 3.9f ? 0.0f : std::min(biomeBannerT_, 0.3f);
      if (biomeBannerT_ <= 0) biomeBannerEco_ = -1;
    }
  }
  if (biomeBannerT_ > 0 && ((fx_.bannerT_ <= 0 && realm_.herald_.t <= 0) || biomeBannerT_ < 3.9f) && g.mode == Mode::Play) biomeBannerT_ -= dt;   // (it waits for a big banner)
}

// the weather (update)
void View::updateWeather(Game& g, float dt, bool outdoors) {
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
      if (skyOverride_ == kSkyClear) { Wx z; z.er = want.er; z.eg = want.eg; z.eb = want.eb; want = z; }   // (M11 TERRAIN) a script's clear sky
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
}

// the land's sound bed and mood (update)
void View::updateAmbience(Game& g, bool outdoors) {
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
    audio_->setMood(realm_.nearSiege(g) ? (uint8_t)Mood::Ominous : (uint8_t)ecoInfo(he).mood);   // (M4) the Siege mood
    audio_->setDaylight(g.mode == Mode::Title ? 1.0f : clampf(g.daylight(), 0, 1));
  }
}

// the ambient particles: the land's own drift, the dust indoors (update)
void View::updateDrift(Game& g, float dt, bool outdoors, float VW, float VH) {
  // ambient particles (world layer): the land's own drift
  if (outdoors && hereEco_ >= 0) {
    const Eco e = (Eco)hereEco_;
    const Biome b = ecoFamily(e);
    Rng r((uint32_t)(t_ * 997));
    auto spawn = [&](Vec2 p, Vec2 v, float life, Color c, float size, int layer) {
      Particle q; q.p = p; q.v = v; q.life = q.max = life; q.c = c; q.size = size; q.layer = layer; fx_.parts_.push_back(q);
    };
    auto anywhere = [&]() { return camera_.cam_ + Vec2(r.range(0, VW), r.range(0, VH)); };
    auto fromTop = [&]() { return camera_.cam_ + Vec2(r.range(-40, VW), -4); };
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
      spawn(camera_.cam_ + Vec2(r.range(0, VW), r.range(VH * 0.3f, VH + 10)), Vec2(r.range(-8, 8), r.range(-26, -12)), r.range(1.5f, 3.0f),
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
    Particle q; q.p = camera_.cam_ + Vec2(r.range(0, VW), r.range(0, VH)); q.v = Vec2(r.range(-3, 3), r.range(-5, -1));
    q.life = q.max = 3; q.c = Color(0.7f, 0.65f, 0.6f); q.size = 1; q.layer = 1; fx_.parts_.push_back(q);
  }
  if (fx_.parts_.size() > 600) fx_.parts_.erase(fx_.parts_.begin(), fx_.parts_.begin() + (fx_.parts_.size() - 600));
}

// ------------------------------------------------------------------ world

uint64_t View::mapIdFor(const Game& g, int key) const {
  uint64_t mapId = hash32((uint32_t)g.world.seed ^ (uint32_t)(g.world.seed >> 32)) * 2654435761ull + (uint64_t)(key + 7);
  mapId ^= (uint64_t)g.world.genVersion << 56 | (uint64_t)(g.world.endless ? 1 : 0) << 55;
  return mapId;
}

uint64_t View::terrainFrame(Game& g, const Map*& mp) {
  const Map& m = g.mode == Mode::Title ? g.world.over : g.map();
  mp = &m;
  int key = g.mode == Mode::Title ? 0 : g.mapKey();
  if (key != bake_.lastMapKey_) { bake_.lastMapKey_ = key; }
  const uint64_t mapId = mapIdFor(g, key);
  // M1: an endless overworld's terrain chunks are keyed by GLOBAL chunk (the window origin in chunks plus the local
  // chunk), so a window shift keeps every chunk already baked (terrain.cpp)
  const bool endlessOver = key == 0 && g.world.endless && m.kind == MapKind::Overworld;
  bake_.chunkEndless_ = endlessOver;
  bake_.chunkOX_ = endlessOver ? (int)std::floor(g.world.ox / 32.0) : 0;
  bake_.chunkOY_ = endlessOver ? (int)std::floor(g.world.oy / 32.0) : 0;
  return mapId;
}

// (fixer M4 r2) when this frame's world draw began: the ahead-of-time building painter takes only what is left of a
// frame's budget (a fresh capital's walk drew 20 ms frames on the web path: 33 people plus a 3 ms paint step each)
std::chrono::steady_clock::time_point rview::g_drawT0;

// (SOLID 3.4a) the wall layout (towers, joins, gate flanks), once per map: drawWorld
void View::layoutWallKeys(Game& g, const Map& m, uint64_t mapId, bool endlessOver) {
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
      auto it = sprites_.wallKeyCache_.find(wallId);
      if (it != sprites_.wallKeyCache_.end() && it->second.size() == (size_t)m.w * m.h) {
        wallKeys_ = it->second;
        auto fl = sprites_.fortLookCache_.find(wallId);
        sprites_.fortLooks_ = fl != sprites_.fortLookCache_.end() ? fl->second : std::vector<bld::FortParts>(1);
      } else {
        sprites_.fortLooks_.assign(1, bld::FortParts{});
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
          // (fixer M7b r2, review: "a stream runs under a hedge wall") a living hedge has no arch to carry over a river:
          // where the water runs through, the hedge parts (the tile keeps its wall: nobody walks through the stream's gap)
          for (size_t i = 0; i < wallKeys_.size(); i++)
            if ((wallKeys_[i] & art::WALL_BIT_CULVERT) && m.wall[i] == 1 + (int)art::CityWall::Thorn) wallKeys_[i] = 0;
          // (M3 fixer) an opening without a gatehouse (a side gate, a gate in a north-south run) is spanned: the wall walk
          // runs on over it on a vault (art::WALL_BIT_SPAN), so no road runs through a bare cut between two tower stubs.
          // The span's tiles stay walkable (Map::wall is untouched); only their sprites and their neighbours' joins change.
          spanWallGaps(g, m);
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
                      if (sprites_.fortLooks_.size() < 8 && sprites_.fortPartsOfSite(g, site, fp)) { slot = (int)sprites_.fortLooks_.size(); sprites_.fortLooks_.push_back(fp); }
                      siteSlot[site] = slot;
                    }
                  }
                  blockSlot[bk] = slot;
                }
                if (slot > 0) k = (k & ~art::WALL_LOOK_MASK) | ((uint32_t)slot << art::WALL_LOOK_SHIFT);
              }
          }
        } else art::wallKeys(m.wall.data(), m.w, m.h, nullptr, 0, wallKeys_);
        if (sprites_.wallKeyCache_.size() > 4) { sprites_.wallKeyCache_.clear(); sprites_.fortLookCache_.clear(); }
        sprites_.wallKeyCache_[wallId] = wallKeys_;
        sprites_.fortLookCache_[wallId] = sprites_.fortLooks_;
      }
    }
    wallTodo_.clear();
    // (M3b) the texture caches stay bounded: a long journey through many cultures repaints rather than hoards
    if (sprites_.wallTiles_.size() > 1500) sprites_.wallTiles_.clear();
    if (sprites_.styledProps_.size() > 900) sprites_.styledProps_.clear();
    std::unordered_map<uint32_t, bool> queued;
    for (uint32_t k : wallKeys_)
      if (k && !sprites_.wallTiles_.count(sprites_.wallTexKey(k)) && !queued.count(k)) { queued[k] = true; wallTodo_.push_back(k); }
  }
  // wall tiles not seen yet are painted ahead, one per frame (the ones on screen are painted on first use anyway)
  while (!wallTodo_.empty()) {
    uint32_t k = wallTodo_.back();
    wallTodo_.pop_back();
    if (!sprites_.wallTiles_.count(sprites_.wallTexKey(k))) { sprites_.wallTileTex(*this, k); break; }
  }
}

// (M3 fixer) the spans over the openings without a gatehouse: layoutWallKeys
void View::spanWallGaps(Game& g, const Map& m) {
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
  // (M7c fixer, review: "the approach road runs into a tower, not through the opening") a north-south span's
  // south jamb tower rose two tiles over the passage in the 3/4 view and hid the road's way through: the vault's
  // mouths showed above the road, the tower stood on it. The south tower steps one tile down the run (where the
  // run goes on straight), so the road meets the mouths in the flanks with the roof over them
  for (const IRect& r : spans) {
    if (r.w != 1) continue;
    const int sx = r.x, sy = r.y + 3;
    if (!m.in(sx - 1, sy) || !m.in(sx + 1, sy + 3) || !(wallKeys_[wallIdx(sx, sy)] & art::WALL_BIT_TOWER)) continue;
    // the run below straight (one tile wide) for k tiles: the tower goes two tiles down when it can, else one
    auto plain = [&](int y) { return m.wall[wallIdx(sx, y)] && wallKeys_[wallIdx(sx, y)] && !m.wall[wallIdx(sx - 1, y)] && !m.wall[wallIdx(sx + 1, y)]; };
    const int to = plain(sy + 1) && plain(sy + 2) && plain(sy + 3) ? sy + 2 : (plain(sy + 1) && plain(sy + 2) ? sy + 1 : -1);
    if (to < 0) continue;
    wallKeys_[wallIdx(sx, sy)] &= ~art::WALL_BIT_TOWER;
    wallKeys_[wallIdx(sx, sy + 1)] &= ~art::WALL_BIT_TOWER_N;
    wallKeys_[wallIdx(sx, to)] |= art::WALL_BIT_TOWER;
    wallKeys_[wallIdx(sx, to + 1)] |= art::WALL_BIT_TOWER_N;
  }
}

void View::drawWorld(Game& g) {
  Pix& P = *pix_;
  g_drawT0 = std::chrono::steady_clock::now();
  // (P1b fix) this frame's lit beacons only: drawLighting returns early in full daylight before its own clear, and
  // the list used to grow every frame until dusk (one huge lighting frame, unbounded memory)
  g_beaconFires.clear();
  beasts::plates.clear();   // (P2a LENS) the UI pass (drawBeastPlates) takes this frame's; a `ui 0` frame leaves them
  liftRecs_.clear();
  g_occRecs.clear();
  party_.graveTops_.clear();
  {   // (M4) last frame's overlay counts for the scripts (the map's are kept from its own draw)
    const int mk = realm_.m4Stats_.mapMarkers, bp = realm_.m4Stats_.borderPx;
    realm_.m4Stats_ = realm_.m4Count_;
    realm_.m4Stats_.mapMarkers = mk; realm_.m4Stats_.borderPx = bp;
    realm_.m4Count_ = M4Stats();
  }
  {   // (M5) the same for the life overlays (the sound's numbers are kept: update() sets them)
    const int mu = life_.m5Stats_.music;
    const float tl = life_.m5Stats_.tavernLevel, cr = life_.m5Stats_.crowd;
    life_.m5Stats_ = life_.m5Count_;
    life_.m5Stats_.music = mu; life_.m5Stats_.tavernLevel = tl; life_.m5Stats_.crowd = cr;
    life_.m5Count_ = M5Stats();
  }
  fadePaintMs_ = bake_.arriving_ || g.travelling() ? 10.0 : 0.0;   // (M2: an arrival paints its buildings in travelArrive, within its budget)
  const Map* mp = nullptr;
  const uint64_t mapId = terrainFrame(g, mp);
  const Map& m = *mp;
  const bool endlessOver = bake_.chunkEndless_;
  if (g.mode != Mode::Title) bake_.rebakeHomeHouses(*this, g, m, mapId);   // (M7) the ground under the player's houses: their shadows
  Vec2 cam(std::floor(camera_.cam_.x + fx_.shakeOff_.x), std::floor(camera_.cam_.y + fx_.shakeOff_.y));
  sprites_.trimCaches(*this);
  beasts::glows.clear();    // (M6 BEASTS) this frame's glow sheets and name plates (drawn after the lighting)
  beasts::finish.clear(); beasts::finishFigures = 0;   // (P2c ladder) the living finish
  for (auto it = finishParts_.begin(); it != finishParts_.end();)   // (polish i1) motes / embers of actors gone 2 s
    it = (it->second.lastT < t_ - 2.0f || it->second.lastT > t_) ? finishParts_.erase(it) : std::next(it);
  beasts::plates.clear();
  beasts::last = beasts::cur;
  beasts::cur = beasts::Stats();
  // water layer underneath the terrain (shows through translucent water pixels)
  if (m.kind == MapKind::Overworld) {
    seaDrawUnderlay(*this, g, cam);   // (M10 ART) the sea's swell: rolling crests over the deep colour (render_sea.cpp)
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
  if (!bake_.arriving_) bake_.bakeVisibleNow(*this, m, mapId, c0x, c0y, c1x, c1y);
  for (int cy = c0y; cy <= c1y; cy++)
    for (int cx = c0x; cx <= c1x; cx++) {
      if (cx < 0 || cy < 0 || cx * 32 >= m.w || cy * 32 >= m.h) continue;
      Tex t = bake_.chunkTex(*this, m, mapId, cx, cy);
      P.blit(t, cx * 512 - cam.x, cy * 512 - cam.y);
    }
  bake_.prefetch(*this, m, mapId, cam);
  if (!bake_.arriving_ && !g.inside && m.kind == MapKind::Overworld) prepareInterior(g);   // (M3c) the door ahead's interior
  // (an arrival pumps its own budget: travelArrive; M2 fixer round 3: under an open menu, whose map paints its own
  // tiles on the same frames, the world's bake waits its turn with a small share)
  if (!bake_.arriving_) bake_.pumpBake(*this, g.mode == Mode::Menu ? 1.5 : 5.0);
  layoutWallKeys(g, m, mapId, endlessOver);
  if (g.mode != Mode::Title) drawReliefWater(g, cam);   // (M11) waterfalls over the faces (render_relief.cpp)
  if (g.mode != Mode::Title) drawSeaWater(g, cam);      // (M10) waves, foam, wakes over the water (render_sea.cpp)
  if (g.mode != Mode::Title) drawMagicGround(g, cam);   // (M9) surfaces, decals, ground glyphs under the bodies (render_magic.cpp)
  if (g.mode != Mode::Title && !g.inside) drawIncidentGround(g, cam);   // (P1 fixer) a broken cart's wheel in the road (render_markers.cpp)
  drawSortedPass(g, m, cam);   // (SOLID 3.4a) collect, sort, shadows, the sorted draws, the hidden hero: render_sorted.cpp
  const bool recOcc = g.mode != Mode::Title && !g.inside;   // (P2a LENS fix r3; as the sorted pass's)
  // (M5) a festival's bunting hangs overhead, over everyone walking under it
  if (g.mode != Mode::Title && m.kind == MapKind::Overworld) {
    P.recordOthers(recOcc ? &g_occRecs : nullptr);   // (P2a LENS fix r3) the bunting hangs over the folk under it
    life_.drawFestival(*this, g, m, cam);
    P.recordOthers(nullptr);
  } else {
    life_.festLights_.clear();
  }
  // particles (world layer)
  for (const Particle& q : fx_.parts_) {
    float a = clampf(q.life / q.max, 0, 1);
    if (q.kind == 1) {
      art::Fx f = (art::Fx)q.fx;
      const Tex& t = sprites_.fx_[q.fx];
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
    float& ck = coldK_;   // eased, so walking off the snow at dusk does not switch the light at once
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
  if (amb.r > 0.99f && amb.g > 0.99f && amb.b > 0.99f) { liftRecs_.clear(); g_occRecs.clear(); lensStats.liftFigs = 0; return; }
  Vec2 cam(std::floor(camera_.cam_.x + fx_.shakeOff_.x), std::floor(camera_.cam_.y + fx_.shakeOff_.y));
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
    P.blitEx(sprites_.light_, 0, 0, 64, 64, x - rr, y - rr, rr * 2, rr * 2, false, Color(c.r, c.g, c.b, k), 1);
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
            !(life_.lifeFlagsAt(*this, g, tx, ty) & (life::MF_SHUTTERED | life::MF_FAMINE))) {
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
            !(life_.lifeFlagsAt(*this, g, tx, ty) & (life::MF_SHUTTERED | life::MF_FAMINE))) {
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
      const uint64_t bk = sprites_.bldgKey(m, b, (int)bi);
      auto w = sprites_.bldgWin_.find(bk);
      auto t = sprites_.bldgTex_.find(bk);
      if (w == sprites_.bldgWin_.end() || t == sprites_.bldgTex_.end()) continue;
      const float bottom = (b.r.y + b.r.h) * 16.0f, top = bottom + art::BLDG_PAD_B - t->second.h;
      float f = 0.94f + 0.06f * std::sin(t_ * 5 + bi);
      // (fixer M5 r3) the panes themselves through their mask: only the glass is lit (a round pool per window lit
      // the roofs in front of an upper facade at near daylight brightness)
      auto gm = sprites_.bldgGlow_.find(bk);
      if (gm != sprites_.bldgGlow_.end())
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
    for (size_t i = 0; i < life_.festLights_.size(); i++) {
      const Vec2 lp = life_.festLights_[i];
      const float f = 0.88f + 0.12f * std::sin(t_ * 6.3f + (float)i * 1.9f);
      const float k = std::min(1.0f, dark * 2.0f) * f;
      light(lp, 26, Color(1.0f, 0.80f, 0.52f), 0.9f * k);
      light(lp + Vec2(0, 16), 56, Color(1.0f, 0.58f, 0.34f), 0.42f * k);
    }
  // (M7) the yard's lanterns and campfires (home_view.cpp homeCollect: their flame and pool)
  if (!g.inside && dark > 0.12f)
    for (const LightPool& lp : home_.homeLights_) light(lp.p, lp.r, lp.c, lp.k * std::min(1.0f, dark * 2.0f));
  // (P1b) a beacon a story lit: a tight bloom at the fire and a wide warm pool over the hilltop round the tower
  for (const Vec2& bf : g_beaconFires) {
    const float f = 0.88f + 0.12f * std::sin(t_ * 7.3f + bf.x * 0.01f);
    light(bf, 30, Color(1.0f, 0.86f, 0.56f), 0.95f * f);
    light(bf + Vec2(0, 30), 120, Color(1.0f, 0.55f, 0.25f), 0.55f * f * std::min(1.0f, dark * 2.0f));
  }
  g_beaconFires.clear();
  {   // (M4) the siege camps' tent lanterns and the red smoulder of burned-out buildings (realm_render.cpp)
    static std::vector<LightPool> pools;
    realm_.m4Lights(*this, g, m, cam, dark, pools);
    for (const LightPool& lp : pools) light(lp.p, lp.r, lp.c, lp.k);
  }
  for (const Projectile& pr : g.projs) {
    if (pr.spell) continue;   // (M9) a spell's missile is lit by magicLights
    if (pr.kind == ProjKind::Fireball || pr.kind == ProjKind::DragonFire) light(pr.p, 60, Color(1, 0.6f, 0.25f), 0.9f);
    else if (pr.kind == ProjKind::Magic && pr.ench == Ench::None && !pr.fromPlayer) light(pr.p, 40, Color(0.4f, 1.0f, 0.95f), 0.8f);
    else if (pr.kind == ProjKind::IceSpike || pr.kind == ProjKind::Magic) light(pr.p, 40, Color(0.5f, 0.6f, 1), 0.8f);
  }
  for (const Particle& q : fx_.parts_) if (q.kind == 1 && q.fx == (int)art::Fx::Explosion) light(q.p, 90, Color(1, 0.6f, 0.3f), q.life / q.max);
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
    // (M6 BEASTS) a golem's rune lights its chest; an elite's aura lights the ground round it
    if (a.mon == Monster::Golem) light(a.p + Vec2(0, -18.0f * beasts::drawScale(a)), 34, beasts::runeLight(a), 0.55f * (0.85f + 0.15f * std::sin(t_ * 5 + a.id)));
    if (const uint32_t ac = beasts::auraOf(a)) light(a.p + Vec2(0, -3), 40, col(ac), 0.5f * std::min(1.0f, dark * 1.5f));
  }
  loot_.lootLights(*this, g, light);   // (M8b) the loot's beams and pools, a Mythic wearer's smoulder (loot_ui.cpp)
  magicLights(g, light);  // (M9) the spells' light: lit trails, impact flashes, burning ground (render_magic.cpp)
  seaLights(g, light);    // (M10) boat lanterns, the lighthouse's lamp (render_sea.cpp)
  riseLights(g, light);   // (M12) the watchtowers' lanterns (rise_world.cpp)
  P.setTarget(nullptr);
  P.blitEx(lightMap_, 0, 0, lightMap_.w, lightMap_.h, 0, 0, lightMap_.w * 2.0f, lightMap_.h * 2.0f, false, Color(1, 1, 1, 1), 2);
  drawNightLift(g, dark);        // (P2a LENS) people a step brighter than the ground they stand on
  realm_.m4Emissive(*this, g, m, cam, dark);   // (fixer M4 r3) what burns glows over the dark
}

// (M3c LIFE) the weather of the land: wx_ (eased in update from the eco's Sky) painted in screen space. Every layer is
// a few dozen rects or soft blobs, sized to the canvas, so the phone's frame budget hardly notices.
void View::drawWeather(Game& g, float dt) {
  (void)dt;
  Pix& P = *pix_;
  // (M6 BEASTS) after the weather (and after every early return below): the glowing parts of the variant looks, the
  // name plates of the elites, champions and named uniques, and a world boss's bar
  // (P2a LENS) the plates and the boss bar are the UI layer's now: drawBeastPlates, after the world (beasts::plates waits)
  auto beastsOver = [&] {
    if (g.mode == Mode::Title || g.mode == Mode::Creator) { beasts::glows.clear(); beasts::plates.clear(); beasts::xray.clear(); return; }
    const auto t0 = beasts::Clock::now();
    struct Tally { beasts::Clock::time_point t; ~Tally() { beasts::cur.ms += beasts::msSince(t); } } tally{t0};
    beasts::cur.glows += (int)beasts::glows.size();
    const Vec2 cam(std::floor(camera_.cam_.x + fx_.shakeOff_.x), std::floor(camera_.cam_.y + fx_.shakeOff_.y));
    // the glow stands out as much as the night is dark (a little by day: embers and runes still smoulder)
    const bool dungeon = g.inside && g.subSite >= 0;
    const float dk = dungeon ? 0.7f : g.inside ? clampf(1.0f - g.daylight(), 0, 1) * 0.6f : clampf(1.0f - g.daylight() * 1.4f, 0, 1);
    const float ga = 0.2f + 0.8f * dk;
    // the ghosts first (a faint silhouette over whatever stands in front of them: canopy, walls, roofs), then the glows
    // (drawn after the lighting: dimmed by the night as the land round it is)
    const float lk = 1.0f - 0.75f * dk;
    for (const beasts::Glow& q : beasts::xray)
      P.blitEx(*q.t, q.sx, 0, q.sw, q.sh, q.x, q.y, (float)q.sw, (float)q.sh, q.flip, Color(lk, lk * 0.92f, lk * 0.86f, 0.42f));
    beasts::xray.clear();
    for (const beasts::Glow& q : beasts::glows)
      P.blitEx(*q.t, q.sx, 0, q.sw, q.sh, q.x, q.y, (float)q.sw, (float)q.sh, q.flip, Color(1, 1, 1, ga * q.a), 1);
    beasts::glows.clear();
    // (P2c ladder) the living finish: a shimmer by day, a soft glow at night (STYLE_P2 27.7)
    const float fa = 0.85f + 0.15f * dk;   // (ladder i3) by day too: the panel could not see the breath at 0.6
    for (const beasts::Finish& q : beasts::finish) {
      Color c = q.c;
      c.a *= fa;
      if (q.t) P.blitEx(*q.t, q.sx, q.sy, q.sw, q.sh, q.x, q.y, q.w, q.h, false, c, 1);
      else P.rectAdd(q.x, q.y, q.w, q.h, c);
    }
    beasts::finish.clear();
  };
  beasts::OnExit<decltype(beastsOver)> beastsAfter{beastsOver};
  drawWeatherSky(g);
}

// (M6 BEASTS; P2a LENS: the UI layer, anchored on the world through toUi) the name plates of the elites, champions and
// named uniques, and a world boss's bar. The plates were collected by the sorted pass (beasts::plates, map px).
void View::drawBeastPlates(Game& g) {
  Pix& P = *pix_;
  bossBarBottom_ = 0;
  if (g.mode != Mode::Play) { beasts::plates.clear(); return; }
  const auto t0 = beasts::Clock::now();
  struct Tally { beasts::Clock::time_point t; ~Tally() { beasts::cur.ms += beasts::msSince(t); } } tally{t0};
  // map px to UI px (affine): x -> ox + x * k
  const Vec2 uo = camera_.toUi(*this, Vec2(0, 0));
  const float uk = camera_.uiPerWorld(*this);
  auto ux = [&](float x) { return uo.x + x * uk; };
  auto uy = [&](float y) { return uo.y + y * uk; };
  const float L = (float)Pix::SL, T = (float)Pix::ST, R = (float)(Pix::W - Pix::SR);
    // a world boss's bar is found first: its strip at the top is kept clear of the plates (and of the herald's ribbon
    // and the biome banner, which read bossBarBottom_)
    const Actor* wb = nullptr;
    float best = 330.0f * 330.0f;
    for (const Actor& a : g.actors) {
      if (!beasts::isRank(a, foes::Rank::WorldBoss) || a.st == AState::Dead || !a.hostile) continue;
      const float d2 = len2(a.p - g.pl().p);
      if ((a.aggro || d2 < 200.0f * 200.0f) && d2 < best) { best = d2; wb = &a; }
    }
    bossBarBottom_ = 0;
    // the name plates (M6 fixer, review: "plates stack on top of each other and cover the creatures"): one plate per
    // champion pack and per crowd of like elites (with its count), the base name only (the aura tells the affixes) and
    // the affix words in a small second line on a named unique and the nearest hunter alone; never over the hero, the
    // boss bar or another plate; the nearest four that fit, the ones hunting the player first
    std::sort(beasts::plates.begin(), beasts::plates.end(), [&](const beasts::Plate& x, const beasts::Plate& y) {
      const bool ax = g.actors[(size_t)x.idx].aggro, ay = g.actors[(size_t)y.idx].aggro;
      return ax != ay ? ax : x.d2 < y.d2;
    });
    const bool phone = Pix::W < 700;
    const int maxW = phone ? 96 : 120;
    struct Rect4 { float x0, y0, x1, y1; };
    std::vector<Rect4> placed;
    {   // the hero's body: plates slide off it
      const Vec2 hp = camera_.toUi(*this, g.pl().p);
      const float fh = (float)rigFor(g.pl().look).figH(g.pl().look) + 4.0f;
      placed.push_back({hp.x - 8 * uk, hp.y - fh * uk, hp.x + 8 * uk, hp.y + 2 * uk});
    }
    if (hudColLeft_ < 1e8f && hudColBottom_ > 0)   // (M6 fixer r2) the location / quest column (last frame's): no plate under it
      placed.push_back({hudColLeft_, T, R, hudColBottom_});
    if (biomeBannerT_ > 0 && !biomeBanner_.empty() && !g.inside) {   // the first-visit biome banner (hud.cpp) at the top centre
      const float half = std::min((float)P.textW(biomeBanner_, 1) / 2 + 26, Pix::W / 2.0f - 70);
      const float by = std::max(biomeBannerY(half), (wb ? T + 21.0f : 0.0f) + 10);   // (M9 fixer) under the quick bar too
      placed.push_back({Pix::W / 2.0f - half, by - 4, Pix::W / 2.0f + half, by + 11});
    }
    if (realm_.herald_.t > 0) {   // and the herald's ribbon
      const float hw = (float)P.textW(realm_.herald_.title, 1) / 2 + 40;
      const float hy = std::max(T + 5, wb ? T + 24.0f : 0.0f);
      placed.push_back({Pix::W / 2.0f - hw - 30, hy, Pix::W / 2.0f + hw + 30, hy + 28});
    }
    // the base name: the name without its affix words, cut to its last words until it fits
    auto baseName = [&](const Actor& a) {
      std::string rest = a.name;
      for (int k = 0; k < (int)foes::Affix::COUNT; k++) {
        const std::string an = std::string(foes::affixInfo((foes::Affix)k).name) + " ";
        size_t at;
        while ((at = rest.find(an)) != std::string::npos) rest.erase(at, an.size());
      }
      while (P.textW(rest, 1) > maxW) {
        const size_t sp = rest.find(' ');
        if (sp == std::string::npos) break;
        rest = rest.substr(sp + 1);
      }
      return rest;
    };
    // (M6 fixer r2, review: "the plates overlap each other and cover the monsters under them") every plated creature's
    // body is an obstacle for the OTHER plates too: a plate slides off the creatures round it as it does off the hero
    std::vector<std::pair<int, Rect4>> bodies;
    bodies.reserve(beasts::plates.size());
    for (const beasts::Plate& pl : beasts::plates)
      bodies.push_back({pl.idx, {std::floor(ux(pl.x) - 8 * uk), std::floor(uy(pl.y)), std::floor(ux(pl.x) + 8 * uk), std::floor(uy(pl.feet) + 2 * uk)}});
    struct Shown { std::string key; float x, y; int pack, n; size_t rect; float y0; };
    std::vector<Shown> shownV;
    bool firstHunter = true;
    int shown = 0;
    for (const beasts::Plate& pl : beasts::plates) {
      if (pl.idx < 0 || pl.idx >= (int)g.actors.size()) continue;
      const Actor& a = g.actors[(size_t)pl.idx];
      const bool named = beasts::isRank(a, foes::Rank::Named), champ = beasts::isRank(a, foes::Rank::Champion);
      const std::string base = named ? a.name : baseName(a);
      // one plate for a champion pack, and for like elites close together: the shown one counts them
      bool merged = false;
      for (Shown& o : shownV) {
        const bool samePack = champ && a.pack > 0 && o.pack == a.pack;
        const bool alike = !named && o.key == base && (pl.x - o.x) * (pl.x - o.x) + (pl.y - o.y) * (pl.y - o.y) < 90.0f * 90.0f;
        if (samePack || alike) { o.n++; merged = true; break; }
      }
      if (merged || shown >= 4) continue;
      std::string words;
      int nw = 0;
      for (int k = 0; k < (int)foes::Affix::COUNT; k++)
        if (a.affixes & foes::affixBit((foes::Affix)k))
          if (nw++ < (named ? 3 : 2)) { if (!words.empty()) words += ' '; words += foes::affixInfo((foes::Affix)k).name; }
      std::string line1 = base;
      if (named && P.textW(line1, 1) > maxW + 90) line1 = baseName(a);
      // the affix words: always under a named unique; under the nearest hunting elite or champion only
      std::string line2;
      if (!words.empty() && (named || (firstHunter && a.aggro))) line2 = words;
      if (P.textW(line2, 1) > maxW + 20 && line2.find(' ') != std::string::npos) line2 = line2.substr(0, line2.find(' '));
      const uint32_t ac = beasts::auraOf(a);
      const Color rim = named ? Color(1.0f, 0.78f, 0.3f) : col(ac ? ac : rgba(220, 220, 255));
      const Color tc = named ? Color(1.0f, 0.84f, 0.36f) : champ ? Color(1.0f, 0.72f, 0.45f)
                                                         : Color(0.75f + rim.r * 0.25f, 0.75f + rim.g * 0.25f, 0.75f + rim.b * 0.25f);
      const int w = std::max(P.textW(line1, 1) + (named ? 0 : 20), line2.empty() ? 0 : P.textW(line2, 1));   // (room for " x3")
      const bool hurt = a.hp < a.maxHp;
      const float h = 9.0f + (line2.empty() ? 0.0f : 8.0f) + (hurt ? 3.0f : 0.0f);
      float cx = std::floor(ux(pl.x)), y0 = std::floor(uy(pl.y) - h);
      cx = clampf(cx, L + w / 2.0f + 4, R - w / 2.0f - 4);
      if (y0 < hudColBottom_ && cx + w / 2.0f + 3 > hudColLeft_) {   // under the quest column: slide a little, or leave it out
        const float nx = hudColLeft_ - w / 2.0f - 4;
        if (cx - nx > 40.0f) y0 = std::floor(uy(pl.feet) + 4.0f * uk);   // far under it: the plate goes under the feet
        else cx = nx;
      }
      const float top = T + 2 + (wb ? 22.0f : 0.0f);   // below the boss bar's strip
      if (y0 < top) y0 = top;
      // never over another plate, the hero, another plated creature or the quest column: its own place over the head,
      // then under the feet, then the same a plate's width to the left or right; a crowd leaves the rest out (M6 fixer
      // r2: stacking a plate above whatever was in the way put it over the next creature up)
      auto clearAt = [&](float x, float y) {
        if (y < top) return false;
        auto hits = [&](const Rect4& o) { return x - w / 2.0f - 3 < o.x1 && x + w / 2.0f + 3 > o.x0 && y - 1 < o.y1 && y + h + 1 > o.y0; };
        for (const Rect4& o : placed) if (hits(o)) return false;
        for (const auto& bo : bodies) if (bo.first != pl.idx && hits(bo.second)) return false;
        return true;
      };
      const float yF = std::floor(uy(pl.feet) + 4.0f * uk), shiftX = std::floor(w / 2.0f + 14.0f);
      const float cxs[3] = {cx, clampf(cx - shiftX, L + w / 2.0f + 4, R - w / 2.0f - 4), clampf(cx + shiftX, L + w / 2.0f + 4, R - w / 2.0f - 4)};
      bool clear = false;
      for (int k = 0; k < 6 && !clear; k++) {
        const float x = cxs[k / 2], y = (k & 1) ? yF : y0;
        if (clearAt(x, y)) { cx = x; y0 = y; clear = true; }
      }
      if (!clear) continue;
      if (a.aggro) firstHunter = false;
      shown++;
      beasts::cur.plates++;
      placed.push_back({cx - w / 2.0f - 3, y0 - 1, cx + w / 2.0f + 3, y0 + h + 1});
      shownV.push_back({base, pl.x, pl.y, champ ? a.pack : 0, 1, placed.size() - 1, y0});
      // fainter when it is not hunting the player (a calm elite's plate is a label, not an alarm)
      const float fa = a.aggro ? 1.0f : 0.7f;
      const float tx = named ? cx : cx - 10.0f;   // the name a little left: its count (if any) goes to the right
      P.rect(cx - w / 2.0f - 3, y0 - 1, (float)w + 6, h + 2, Color(0.02f, 0.02f, 0.04f, 0.42f * fa));
      P.rect(cx - w / 2.0f - 2, y0, (float)w + 4, h, Color(0.06f, 0.05f, 0.08f, 0.72f * fa));
      P.rect(cx - w / 2.0f - 2, y0, (float)w + 4, 1, Color(rim.r, rim.g, rim.b, 0.85f * fa));
      P.text(tx + 1, y0 + 2 + 1, line1, 1, Color(0, 0, 0, 0.8f * fa), 1);
      P.text(tx, y0 + 2, line1, 1, Color(tc.r, tc.g, tc.b, fa), 1);
      if (!line2.empty()) P.text(cx, y0 + 10, line2, 1, Color(0.82f, 0.74f, 0.62f, 0.9f * fa), 1);
      if (hurt) {
        const float bw = (float)w, by = y0 + h - 3;
        P.rect(cx - bw / 2.0f, by, bw, 2, Color(0.25f, 0.06f, 0.06f, 0.9f));
        P.rect(cx - bw / 2.0f, by, bw * clampf(a.hp / a.maxHp, 0, 1), 2, Color(0.85f, 0.2f, 0.18f));
      }
    }
    // the counts of the merged plates ("x3"), at the right end of their name line
    for (const Shown& o : shownV) {
      if (o.n < 2) continue;
      const Rect4& r4 = placed[o.rect];
      const std::string cnt = "x" + std::to_string(o.n);
      P.text(r4.x1 - 4 - (float)P.textW(cnt, 1), o.y0 + 2, cnt, 1, Color(1.0f, 0.86f, 0.5f), 0);
    }
    beasts::plates.clear();
    // a world boss's bar: at the top of the screen between the vitals (top-left) and the minimap / quest column
    // (top-right): its name, its health with notches at the phase marks, a pip per phase it has passed
    if (wb) {
      beasts::cur.bossBar = 1;
      const float gx0 = L + 178.0f, gx1 = std::min(R - 168.0f, hudColLeft_ < 1e8f ? hudColLeft_ - 6.0f : R - 168.0f);
      float bw = std::min(200.0f, gx1 - gx0 - 8.0f), bx, ty;
      if (bw >= 96.0f) { bx = std::floor((gx0 + gx1) * 0.5f - bw * 0.5f); ty = T + 3.0f; bossBarBottom_ = ty + 18.0f; bossBarX0_ = bx; bossBarX1_ = bx + bw; }
      else { bw = 150.0f; bx = L + 6.0f; ty = T + 58.0f; }   // a narrow screen: under the vitals
      const float hk = clampf(wb->hp / std::max(1.0f, wb->maxHp), 0, 1);
      auto mi = beasts::memo.find(wb->id);
      const float fl = mi != beasts::memo.end() ? clampf(mi->second.flashT / 0.45f, 0, 1) : 0.0f;
      P.textS(bx + bw * 0.5f, ty, wb->name, 1, Color(1.0f, 0.80f, 0.55f), 1);
      const float yb = ty + 9.0f;
      P.rect(bx - 2, yb - 2, bw + 4, 9, Color(0.03f, 0.02f, 0.04f, 0.85f));
      P.rect(bx - 1, yb - 1, bw + 2, 7, Color(0.45f, 0.30f, 0.18f, 0.9f));   // a bronze rim
      P.rect(bx, yb, bw, 5, Color(0.22f, 0.04f, 0.05f, 0.95f));
      P.rect(bx, yb, std::floor(bw * hk), 5, Color(0.78f + 0.2f * fl, 0.12f + 0.6f * fl, 0.10f + 0.5f * fl));
      P.rect(bx, yb, std::floor(bw * hk), 1, Color(1.0f, 0.45f, 0.35f));
      P.rect(bx, yb + 4, std::floor(bw * hk), 1, Color(0.5f, 0.06f, 0.08f));
      for (float m : {0.66f, 0.33f}) P.rect(std::floor(bx + bw * m), yb - 1, 1, 7, Color(0.05f, 0.03f, 0.05f, 0.95f));
      for (int i = 0; i < 3; i++) {   // the phases: lit pips for the ones passed, the current one pulsing
        const float px = std::floor(bx + bw - 3.0f - (2 - i) * 7.0f), py = ty + 3.0f;   // (beside the name: the quest
                                                                                          // arrow sits under the bar)
        const bool done = i < (int)wb->bossPhase, cur = i == (int)wb->bossPhase;
        const Color pc = done ? Color(0.95f, 0.35f, 0.2f) : cur ? Color(1.0f, 0.8f, 0.4f, 0.6f + 0.4f * std::sin(t_ * 6)) : Color(0.3f, 0.25f, 0.25f, 0.9f);
        P.rect(px - 1, py, 3, 1, pc); P.rect(px, py - 1, 1, 3, pc);
      }
    }
}

// the weather's sky in the world layer (drawWeather's body; the beasts' glows follow it)
void View::drawWeatherSky(Game& g) {
  Pix& P = *pix_;
  if (g.inside || g.mode == Mode::Title) {
    P.blitEx(sprites_.vignette_, 0, 0, sprites_.vignette_.w, sprites_.vignette_.h, 0, 0, (float)Pix::W, (float)Pix::H, false, Color(1, 1, 1, g.inside && g.subSite >= 0 ? 1.0f : 0.6f));
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
      float x = std::fmod(i * 140 + t_ * 8 - camera_.cam_.x * 0.3f + 7000.0f, (float)Pix::W + 220.0f) - 160;
      P.blitEx(sprites_.light_, 0, 0, 64, 64, x, 30 + i * (Pix::H / (float)n) + std::sin(t_ * 0.3f + i) * 10, 260, 90, false,
               Color(0.82f, 0.86f, 0.84f, 0.26f * W.fog));
    }
  }
  // rain, drizzle and downpours (the drops wrap over the whole canvas and move with the camera)
  const float rainN = (140 * W.rain + 220 * W.pour + 80 * W.drizzle) * sw;
  for (int i = 0; i < (int)rainN; i++) {
    const bool heavy = i < 220 * W.pour * sw, fine = !heavy && i >= (140 * W.rain + 220 * W.pour) * sw;
    const float ww = Pix::W + 40.0f, wh = Pix::H + 30.0f;
    const float vy = heavy ? 420 : fine ? 170 : 330, vx = heavy ? 150 : fine ? 40 : 120;
    float sx = wrap(hashf(i, 0, 5) * 600 + t_ * vx - camera_.cam_.x, ww) - 20;
    float sy = wrap(hashf(i, 1, 5) * 400 + t_ * vy - camera_.cam_.y, wh) - 15;
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
    const float rx = wrap(hashf(i + cyc * 131, 5, 5) * 900 - camera_.cam_.x, (float)Pix::W), ry = wrap(hashf(i + cyc * 131, 6, 5) * 600 - camera_.cam_.y, (float)Pix::H);
    const float r = 1 + ph * 3;
    P.rect(rx - r, ry, r * 2, 1, Color(0.80f, 0.88f, 1.0f, 0.35f * (1 - ph)));
  }
  // steam rising off warm ground after a downpour (monsoon lands)
  if (W.pour > 0.05f || (W.fog > 0.3f && W.rain + W.pour > 0.05f)) {
    for (int i = 0; i < 5; i++) {
      const float life = std::fmod(t_ * 0.12f + i * 0.2f, 1.0f);
      const float x = wrap(hashf(i, 9, 3) * 900 - camera_.cam_.x * 0.8f, Pix::W + 120.0f) - 60;
      const float y = Pix::H * 0.95f - life * Pix::H * 0.6f;
      P.blitEx(sprites_.light_, 0, 0, 64, 64, x, y, 90, 50, false, Color(0.9f, 0.92f, 0.95f, 0.10f * std::sin(life * 3.14159f) * std::max(W.pour, 0.5f)));
    }
  }
  // snow, and the blizzard's driving snow and white-out
  const float snowN = (110 * W.snow + 230 * W.blizz) * sw;
  for (int i = 0; i < (int)snowN; i++) {
    const bool drive = i < 230 * W.blizz * sw;
    const float sp = (drive ? 60 : 14) + hashf(i, 3, 7) * (drive ? 50 : 18);
    const float ww = Pix::W + 20.0f, wh = Pix::H + 20.0f;
    float sx = wrap(hashf(i, 0, 7) * 600 + std::sin(t_ * 0.8f + i) * (drive ? 4 : 10) - camera_.cam_.x + t_ * (drive ? 160 : 6), ww) - 10;
    float sy = wrap(hashf(i, 1, 7) * 400 + t_ * sp - camera_.cam_.y, wh) - 10;
    const float s = hashf(i, 2, 7) < 0.3f ? 2.f : 1.f;
    P.rect(sx + 1, sy + 1, drive ? s + 1 : s, s, Color(0.42f, 0.50f, 0.68f, 0.35f));   // a soft shadow: flakes read over snow too
    P.rect(sx, sy, drive ? s + 1 : s, s, Color(1, 1, 1, drive ? 0.75f : 0.9f));
  }
  if (W.blizz > 0.02f) {
    P.rect(0, 0, (float)Pix::W, (float)Pix::H, Color(0.92f, 0.95f, 1.0f, 0.38f * W.blizz));
    for (int i = 0; i < 4; i++) {   // gusts: brighter sheets of snow sweeping past
      const float x = wrap(t_ * 220 + i * 260 - camera_.cam_.x, Pix::W + 300.0f) - 200;
      P.blitEx(sprites_.light_, 0, 0, 64, 64, x, Pix::H * (0.15f + 0.22f * i), 320, 70, false, Color(1, 1, 1, 0.16f * W.blizz));
    }
  }
  // the sandstorm: streaks of blown sand racing past, and grit
  if (W.sand > 0.02f) {
    const int n = (int)(80 * W.sand * sw);
    for (int i = 0; i < n; i++) {
      const float ww = Pix::W + 60.0f;
      const float sx = wrap(hashf(i, 0, 11) * 900 + t_ * (240 + hashf(i, 2, 11) * 160) - camera_.cam_.x, ww) - 30;
      const float sy = wrap(hashf(i, 1, 11) * 500 + std::sin(t_ * 2 + i) * 4 - camera_.cam_.y * 0.5f, (float)Pix::H);
      const float len = 4 + hashf(i, 3, 11) * 12;
      P.rect(sx, sy, len, 1, Color(0.95f, 0.82f, 0.58f, 0.35f * W.sand));
    }
    for (int i = 0; i < 3; i++) {   // the dust's billows
      const float x = wrap(t_ * 140 + i * 300 - camera_.cam_.x * 0.6f, Pix::W + 300.0f) - 200;
      P.blitEx(sprites_.light_, 0, 0, 64, 64, x, Pix::H * (0.2f + 0.3f * i), 340, 110, false, Color(0.86f, 0.70f, 0.46f, 0.18f * W.sand));
    }
  }
  // ashfall: grey flakes drifting down, embers glowing on the way up
  if (W.ash > 0.02f) {
    const int n = (int)(90 * W.ash * sw);
    for (int i = 0; i < n; i++) {
      const float ww = Pix::W + 20.0f, wh = Pix::H + 20.0f;
      const float sx = wrap(hashf(i, 0, 13) * 600 + std::sin(t_ * 0.6f + i) * 12 - camera_.cam_.x + t_ * 10, ww) - 10;
      const float sy = wrap(hashf(i, 1, 13) * 400 + t_ * (10 + hashf(i, 2, 13) * 10) - camera_.cam_.y, wh) - 10;
      const float g0 = 0.45f + hashf(i, 4, 13) * 0.3f;
      P.rect(sx, sy, hashf(i, 5, 13) < 0.25f ? 2.f : 1.f, 1, Color(g0, g0 * 0.95f, g0 * 0.95f, 0.75f));
    }
    for (int i = 0; i < (int)(26 * W.ash * sw); i++) {
      const float sx = wrap(hashf(i, 6, 13) * 700 + std::sin(t_ * 1.3f + i) * 8 - camera_.cam_.x, Pix::W + 20.0f) - 10;
      const float sy = wrap(hashf(i, 7, 13) * 400 - t_ * (18 + hashf(i, 8, 13) * 14) - camera_.cam_.y, Pix::H + 20.0f) - 10;
      const float fl = 0.5f + 0.5f * std::sin(t_ * 9 + i * 2.3f);
      P.rectAdd(sx, sy, 1, 1, Color(1.0f, 0.55f + 0.3f * fl, 0.2f, 0.5f + 0.5f * fl));
    }
  }
  // the eerie lands: slow motes and glints in the land's colour
  if (W.eerie > 0.02f) {
    const int n = (int)(40 * W.eerie * sw);
    for (int i = 0; i < n; i++) {
      const float sx = wrap(hashf(i, 0, 17) * 700 + std::sin(t_ * 0.5f + i) * 14 - camera_.cam_.x * 0.9f, Pix::W + 20.0f) - 10;
      const float sy = wrap(hashf(i, 1, 17) * 400 - t_ * (4 + hashf(i, 2, 17) * 6) - camera_.cam_.y * 0.9f, Pix::H + 20.0f) - 10;
      const float pulse = 0.5f + 0.5f * std::sin(t_ * (1.5f + hashf(i, 3, 17) * 2) + i);
      P.rectAdd(sx, sy, 1, 1, Color(W.er, W.eg, W.eb, 0.25f + 0.55f * pulse));
      if (pulse > 0.93f) { P.rectAdd(sx - 1, sy, 3, 1, Color(W.er, W.eg, W.eb, 0.3f)); P.rectAdd(sx, sy - 1, 1, 3, Color(W.er, W.eg, W.eb, 0.3f)); }
    }
  }
  // falling leaves / fireflies / petals / embers are world particles drawn earlier; light vignette always (heavier in
  // a storm)
  P.blitEx(sprites_.vignette_, 0, 0, sprites_.vignette_.w, sprites_.vignette_.h, 0, 0, (float)Pix::W, (float)Pix::H, false,
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
Music LifeView::lifeMusic(View& v, Game& g, Music want, float dt, bool& festive) {
  festive = false;
  float gain = 1, muffle = 0, crowd = 0, lively = 0;
  const bool calm = want == Music::Town || want == Music::Night || want == Music::Wild;
  const bool playing = g.mode == Mode::Play || g.mode == Mode::Dialogue || g.mode == Mode::Menu || g.mode == Mode::Shop || g.mode == Mode::Paused ||
                       g.mode == Mode::Forge || g.mode == Mode::Build;
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
  v.audio_->setMusicLevel(gain, muffle);
  v.audio_->setCrowd(crowd, lively);
  m5Stats_.tavernLevel = want == Music::Tavern ? gain : 0.0f;
  m5Stats_.crowd = crowd;

  if (!playing || g.mode != Mode::Play) return want;
  const Vec2 pp = g.pl().p;
  // the animals' voices
  critterVoiceT_ -= dt;
  if (critterVoiceT_ <= 0) {
    Rng r((uint32_t)(v.t_ * 1000.0f) ^ 0xA11Eu);
    critterVoiceT_ = r.range(1.4f, 4.0f);
    const Actor* pick = nullptr;
    int seen = 0;
    for (const Actor& a : g.actors) {
      if (a.critter == 0 || a.st == AState::Dead || len2(a.p - pp) > 220.0f * 220.0f) continue;
      if (r.f() * (float)(++seen) < 1.0f) pick = &a;   // a fair pick among them
    }
    if (pick) {
      const float d = len(pick->p - pp), vol = 0.55f * clampf(1.0f - d / 240.0f, 0.15f, 1.0f);
      const art::Critter k = (art::Critter)(pick->critter - 1);
      const float pv = 0.92f + 0.16f * (float)(pick->critterVar % 7u) / 6.0f;   // each animal its own voice
      switch (k) {
        case art::Critter::Dog: if (r.f() < 0.45f || g.isNight()) v.audio_->play(Sfx::Bark, pv, vol); break;
        case art::Critter::Chicken: v.audio_->play(Sfx::Cluck, pv, vol * 0.8f); break;
        case art::Critter::Rooster: v.audio_->play(Sfx::Cluck, pv * 0.82f, vol); break;
        case art::Critter::Duck: v.audio_->play(Sfx::Cluck, pv * 0.62f, vol * 0.7f); break;
        case art::Critter::Cat: if (r.f() < 0.5f) v.audio_->play(Sfx::Meow, pv, vol * 0.8f); break;
        case art::Critter::Goat: v.audio_->play(Sfx::Meow, pv * 0.48f, vol * 0.9f); break;
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
      v.audio_->play(Sfx::Cheer, 1.0f, 0.6f * clampf(1.0f - len(a.p - pp) / 300.0f, 0.2f, 1.0f));
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
      if (g.isNight() && !g.inside && pd < 50) v.audio_->play(Sfx::Bell, 0.89f, clampf(1.15f - pd / 40.0f, 0.25f, 0.9f));
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
void LifeView::festivalPlan(Game& g, const Map& m, Vec2 cam) {
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
void LifeView::drawFestPole(View& v, const FestSpan& s, bool right, Vec2 cam) {
  Pix& P = *v.pix_;
  const int tx = right ? s.x1 : s.x0;
  const float bx = std::floor(tx * 16.0f + 8.0f - cam.x), by = std::floor(s.y * 16.0f + 13.0f - cam.y);
  const Color hi(0.72f, 0.54f, 0.34f), mid(0.52f, 0.36f, 0.22f), lo(0.33f, 0.22f, 0.14f), ink(0.16f, 0.10f, 0.08f);
  P.blitEx(v.sprites_.shadow_, 0, 0, v.sprites_.shadow_.w, v.sprites_.shadow_.h, bx - 5, by - 2, 10, 4, false, Color(1, 1, 1, 0.8f));
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
void LifeView::drawFestival(View& v, Game& g, const Map& m, Vec2 cam) {
  festLights_.clear();
  if (g.inside) return;
  Pix& P = *v.pix_;
  const float dark = clampf((0.62f - g.daylight()) / 0.45f, 0, 1);
  auto buntingTex = [&](int w, uint32_t ca, uint32_t cb, uint32_t seed) -> const Tex& {
    const uint64_t key = (uint64_t)w << 40 ^ (uint64_t)(ca & 0xFFFFFF) << 16 ^ (uint64_t)(cb & 0xFFFFFF) ^ (uint64_t)(seed & 1u) << 62;
    auto it = v.sprites_.festTex_.find(key);
    return it != v.sprites_.festTex_.end() ? it->second : (v.sprites_.festTex_[key] = v.pix_->bake(art::festivalBunting(w, ca, cb, seed)));
  };
  auto lanternTex = [&](uint32_t lc, uint32_t seed) -> const Tex& {
    const uint64_t key = (uint64_t)0x1A ^ ((uint64_t)(lc & 0xFFFFFF) << 8) ^ (1ull << 61) ^ ((uint64_t)(seed & 3u) << 4);
    auto it = v.sprites_.festTex_.find(key);
    return it != v.sprites_.festTex_.end() ? it->second : (v.sprites_.festTex_[key] = v.pix_->bake(art::festivalLantern(lc, seed & 3u)));
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
  v.bldgsIn(g, m, cam.x - 180, cam.y - 80, cam.x + Pix::W + 180, cam.y + Pix::H + 140, vis);
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
      auto tex = v.sprites_.bldgTex_.find(v.sprites_.bldgKey(m, a, bi));
      auto win = v.sprites_.bldgWin_.find(v.sprites_.bldgKey(m, a, bi));
      float side = 0;
      for (int sd : {1, -1}) {
        const float lx = doorCx + sd * 11.0f;
        bool clear = lx > a.r.x * 16.0f + 3 && lx < (a.r.x + a.r.w) * 16.0f - 3;
        if (clear && tex != v.sprites_.bldgTex_.end() && win != v.sprites_.bldgWin_.end()) {
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
      const float sw = std::sin(v.t_ * 1.7f + (float)(a.seed & 63u)) * 0.6f;   // a slow sway on its hook
      P.rect(std::floor(lx - cam.x), std::floor(hy - 2 - cam.y), 1, 2, Color(0.22f, 0.16f, 0.12f));   // the hook
      P.blit(t, std::floor(lx - t.w / 2.0f + sw - cam.x + 0.5f), std::floor(hy - cam.y));
      festLights_.push_back(Vec2(lx, hy + t.h * 0.55f));
      m5Count_.festLanterns++;
    }
  }
}
