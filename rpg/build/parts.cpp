// The builder's non-building parts (VISION_PLAN 15.14, M3b): see rpg/build/parts.h. (M3b FORTIFICATIONS & GROUND
// lane) Every culture builds its own walls, gates, towers, roads, bridges and monuments:
//  - FortParts by culture x urban: the gate form (the heartland's drum-tower gatehouse, the empire's square towers, the
//    fjords' timber gate tower, the dune folk's tiled iwan, the jade kingdoms' gate pavilion and moon gates, the elves'
//    white arch and living arch, the highlands' earthwork cut, the marsh's hedge gap), its towers (drums, cones, square
//    towers under tile, pagodas, minarets, bastions, timber watch platforms), its coping, and for capitals a taller wall
//    and more finery;
//  - RoadParts: the material and its bonds (squares and streets), width, kerbs, bridges;
//  - MonumentParts: statues, obelisks, totems, standing stones, stelae, cairns, stupas, sacred trees, fire bowls and
//    fountains, with per-placement variants so a ring of stones never repeats one stone.
// Integer maths only; deterministic.
#include "rpg/build/parts.h"
#include <cstdint>

namespace bld {

namespace {
uint32_t hmix(uint32_t s, uint32_t k) {
  uint32_t h = s * 2654435761u ^ (k + 0x9E3779B9u + (s << 6) + (s >> 2));
  h ^= h >> 15; h *= 0x2C1B3C6Du; h ^= h >> 12; h *= 0x297A2D39u; h ^= h >> 15;
  return h;
}
constexpr uint32_t rgb(int r, int g, int b) { return (uint32_t)r | (uint32_t)g << 8 | (uint32_t)b << 16 | 0xFF000000u; }
using A = cult::Archetype;
}  // namespace

FortParts fortDefaults(art::CityWall wall) {
  FortParts f;
  f.wall = wall;
  switch (wall) {
    case art::CityWall::Stone: f.gate = GateForm::DrumTowers; f.tower = TowerForm::RoundDrum; f.coping = Coping::Merlons; break;
    case art::CityWall::Palisade: f.gate = GateForm::DrumTowers; f.tower = TowerForm::ConeDrum; f.coping = Coping::Points; break;
    case art::CityWall::Rampart: f.gate = GateForm::DrumTowers; f.tower = TowerForm::ConeDrum; f.coping = Coping::Points; break;
    case art::CityWall::Thorn: f.gate = GateForm::DrumTowers; f.tower = TowerForm::RoundDrum; f.coping = Coping::Hedge; break;
    case art::CityWall::Adobe: f.gate = GateForm::DrumTowers; f.tower = TowerForm::RoundDrum; f.coping = Coping::SteppedMerlons; break;
    case art::CityWall::WhiteStone: f.gate = GateForm::DrumTowers; f.tower = TowerForm::ConeDrum; f.coping = Coping::Rounded; break;
    case art::CityWall::Jade: f.gate = GateForm::DrumTowers; f.tower = TowerForm::ConeDrum; f.coping = Coping::TiledHood; break;
    case art::CityWall::Talud: f.gate = GateForm::Pylons; f.tower = TowerForm::Square; f.coping = Coping::SteppedMerlons; break;
    default: break;
  }
  return f;
}

FortParts fortParts(const cult::Culture& c, int urban, uint32_t seed) {
  FortParts f = fortDefaults(c.town.wall);
  const art::CityWall w = c.town.wall;
  const bool big = urban >= 2, capital = urban >= 3;
  f.culture = (uint8_t)((int)c.archetype + 1);
  f.variant = (uint8_t)(hmix(seed, 0xF0A7u) & 255u);
  switch (c.archetype) {
    case A::Fjordfolk:   // timber: a gate tower over the road, blockhouses (towns: watch platforms)
      if (w == art::CityWall::Palisade || w == art::CityWall::Rampart) { f.gate = GateForm::TimberGate; f.tower = big ? TowerForm::ConeDrum : TowerForm::Platform; }
      break;
    case A::Highland:    // a cut through the bank under a bridge-gate (ramparts) or square tower-houses in grey granite
      if (w == art::CityWall::Rampart) { f.gate = GateForm::Earthwork; f.tower = TowerForm::Bastion; }
      else { f.gate = GateForm::SquareTowers; f.tower = TowerForm::Square; f.stone = rgb(104, 106, 116); }
      break;
    case A::Heartland:   // the northern gatehouse: drum towers and a portcullis (the M2 look)
      if (w != art::CityWall::Stone) break;
      f.gate = GateForm::DrumTowers; f.tower = TowerForm::RoundDrum;
      break;
    case A::Imperial:    // warm travertine, square towers under red tile, a round arch
      if (w != art::CityWall::Stone) break;
      f.gate = GateForm::SquareTowers; f.tower = TowerForm::Square; f.stone = rgb(196, 176, 146); f.roof = rgb(178, 82, 54);
      break;
    case A::Dune:        // a tiled iwan between drums; minarets on a city's walls
      if (w != art::CityWall::Adobe) break;
      f.gate = GateForm::Iwan; f.tower = big ? TowerForm::Minaret : TowerForm::RoundDrum; f.trim = rgb(40, 118, 156);
      break;
    case A::Steppe:      // a timber gate tower hung with hides (palisade), a gap in the thorn
      if (w == art::CityWall::Palisade || w == art::CityWall::Rampart) { f.gate = GateForm::TimberGate; f.tower = TowerForm::Platform; }
      else if (w == art::CityWall::Thorn) { f.gate = GateForm::HedgeGap; f.tower = TowerForm::Platform; }
      break;
    case A::Marsh:       // hedge gaps and watch platforms on stilts
      if (w == art::CityWall::Thorn) { f.gate = GateForm::HedgeGap; f.tower = TowerForm::Platform; }
      else if (w == art::CityWall::Palisade || w == art::CityWall::Rampart) { f.gate = GateForm::TimberGate; f.tower = TowerForm::Platform; }
      break;
    case A::Jade:        // a gate pavilion (cities) or a moon gate (towns); pagoda towers
      if (w != art::CityWall::Jade) break;
      f.gate = big ? GateForm::Paifang : GateForm::MoonGate; f.tower = TowerForm::Pagoda;
      break;
    case A::River:       // red brick, square towers under tiled hoods
      if (w != art::CityWall::Stone) break;
      f.gate = GateForm::SquareTowers; f.tower = TowerForm::Square; f.coping = Coping::TiledHood; f.stone = rgb(160, 88, 66); f.roof = rgb(70, 82, 96);
      break;
    case A::SunTemple:   // battered pylons, square bastions
      if (w == art::CityWall::Talud) { f.gate = GateForm::Pylons; f.tower = TowerForm::Square; }
      break;
    case A::Sylvan:      // a living arch; the thorn grows into its own towers
      if (w == art::CityWall::Thorn) { f.gate = GateForm::LivingArch; f.tower = TowerForm::RoundDrum; }
      break;
    case A::Starspire:   // a slender white arch between spired towers
      if (w == art::CityWall::WhiteStone) { f.gate = GateForm::ElvenArch; f.tower = TowerForm::ConeDrum; }
      break;
    default: break;
  }
  f.height = (uint8_t)(capital && (w == art::CityWall::Stone || w == art::CityWall::Adobe || w == art::CityWall::WhiteStone ||
                                   w == art::CityWall::Jade || w == art::CityWall::Talud) ? 1 : 0);
  f.finery = (uint8_t)(capital ? 3 : (big ? 2 : (urban >= 1 ? 1 : 0)));
  f.accent = c.heraldry.field ? c.heraldry.field : c.arch.accentTint;
  return f;
}

bool gateFitsWall(GateForm g, art::CityWall w) {
  using W = art::CityWall;
  switch (g) {
    case GateForm::DrumTowers: return true;   // the M3 gatehouse comes in every material
    case GateForm::SquareTowers: return w == W::Stone || w == W::Adobe;
    case GateForm::Pylons: return w == W::Talud;
    case GateForm::TimberGate: return w == W::Palisade || w == W::Rampart;
    case GateForm::Iwan: return w == W::Adobe || w == W::Talud;
    case GateForm::Paifang: case GateForm::MoonGate: return w == W::Jade || w == W::WhiteStone || w == W::Stone;
    case GateForm::ElvenArch: return w == W::WhiteStone;
    case GateForm::LivingArch: return w == W::Thorn;
    case GateForm::Earthwork: return w == W::Rampart;
    case GateForm::HedgeGap: return w == W::Thorn;
    default: return false;
  }
}
bool towerFitsWall(TowerForm t, art::CityWall w) {
  using W = art::CityWall;
  switch (t) {
    case TowerForm::RoundDrum: return true;
    case TowerForm::ConeDrum: return w != W::Thorn && w != W::Talud;
    case TowerForm::Square: return w == W::Stone || w == W::Adobe || w == W::Talud;
    case TowerForm::Pagoda: return w == W::Jade || w == W::Stone;
    case TowerForm::Minaret: return w == W::Adobe || w == W::Talud;
    case TowerForm::Bastion: return w == W::Rampart || w == W::Stone || w == W::Adobe;
    case TowerForm::Platform: return w == W::Palisade || w == W::Rampart || w == W::Thorn;
    default: return false;
  }
}

uint64_t fortKey(const FortParts& f) {
  uint64_t k = 0xC6A4A7935BD1E995ull;
  auto mx = [&](uint64_t v) { k ^= v + 0x9E3779B97F4A7C15ull + (k << 6) + (k >> 2); k *= 0xFF51AFD7ED558CCDull; k ^= k >> 33; };
  mx((uint64_t)f.wall | (uint64_t)f.gate << 8 | (uint64_t)f.tower << 16 | (uint64_t)f.coping << 24 | (uint64_t)f.height << 32 |
     (uint64_t)f.finery << 40 | (uint64_t)f.culture << 48 | (uint64_t)f.variant << 56);
  mx((uint64_t)f.stone << 32 | f.trim);
  mx((uint64_t)f.roof << 32 | f.accent);
  return k | 1;
}

PaveBond paveBond(int material, bool street) {
  switch (material) {
    case 0: return PaveBond::Cobbles;                                   // heartland cobbles and flags (the classic)
    case 1: return street ? PaveBond::Crazy : PaveBond::Flags;          // the empire: polygonal basalt roads, travertine
    case 2: return PaveBond::Earth;                                     // beaten earth
    case 3: return street ? PaveBond::Running : PaveBond::Flags;        // sun-baked brick streets, sandstone slabs
    case 4: return PaveBond::Boards;                                    // plank decks
    case 5: return PaveBond::Crazy;                                     // moss-grown crazy paving
    case 6: return street ? PaveBond::Running : PaveBond::Hex;          // the star cities' hexagonal flags, ashlar setts
    case 7: return street ? PaveBond::Running : PaveBond::Crazy;        // the sun temples' terracotta: random flags
    case 8: return street ? PaveBond::Running : PaveBond::Basket;       // the jade kingdoms' blue-grey brick
    case 9: return street ? PaveBond::Running : PaveBond::Herringbone;  // the river towns' red brick
    default: return PaveBond::Cobbles;
  }
}

BridgeForm bridgeOfPaving(int material) {
  switch (material) {
    case 1: case 9: case 0: return BridgeForm::StoneArch;
    case 3: case 7: return BridgeForm::Causeway;
    case 5: return BridgeForm::Living;
    case 6: return BridgeForm::MoonArch;
    case 8: return BridgeForm::Covered;
    default: return BridgeForm::Planks;
  }
}

RoadParts roadParts(const cult::Culture& c, int urban, uint32_t seed) {
  RoadParts r;
  r.paving = c.town.paving;
  r.bond = paveBond(r.paving, false);
  r.streetBond = paveBond(r.paving, true);
  r.width = (uint8_t)(urban >= 3 ? 3 + (int)(hmix(seed, 0x51u) & 1u) : (urban >= 2 ? 3 : 2));
  r.kerbs = urban >= 2 && r.streetBond != PaveBond::Earth && r.streetBond != PaveBond::Boards;
  r.bridge = urban >= 1 ? bridgeOfPaving(r.paving) : BridgeForm::Planks;
  return r;
}

MonumentParts monumentOf(int culture, MonumentForm form, uint32_t seed, uint32_t stone, uint32_t metal) {
  MonumentParts m;
  m.form = form;
  const uint32_t h = hmix(seed, 0x57A6u + (uint32_t)form * 131u + (uint32_t)culture * 7919u);
  m.variant = (uint8_t)(h & 255u);
  m.finery = (uint8_t)((h >> 8) & 3u);
  // the culture's own stones (a standing stone's lichen and tone follow the land; the cultures carve theirs)
  static const uint32_t kStone[13] = {rgb(122, 118, 122), rgb(118, 114, 112), rgb(112, 112, 120), rgb(126, 122, 120), rgb(196, 176, 146),
                                      rgb(206, 170, 120), rgb(128, 116, 104), rgb(112, 120, 108), rgb(120, 126, 132), rgb(150, 110, 90),
                                      rgb(210, 192, 156), rgb(110, 122, 104), rgb(214, 220, 232)};
  static const uint32_t kAccent[13] = {0, rgb(176, 48, 40), rgb(46, 92, 70), rgb(64, 104, 146), rgb(150, 44, 38), rgb(40, 118, 156),
                                       rgb(196, 70, 40), rgb(206, 128, 46), rgb(176, 40, 40), rgb(40, 96, 72), rgb(40, 140, 136),
                                       rgb(120, 200, 150), rgb(90, 130, 220)};
  const int ci = culture >= 0 && culture <= 12 ? culture : 0;
  m.stone = stone ? stone : kStone[ci];
  m.metal = metal ? metal : rgb(176, 132, 60);
  m.accent = kAccent[ci];
  return m;
}

MonumentParts monumentParts(const cult::Culture& c, int kind, uint32_t seed) {
  static const MonumentForm byCentre[] = {MonumentForm::Fountain, MonumentForm::Statue, MonumentForm::Statue, MonumentForm::SacredTree,
                                          MonumentForm::FireBowl, MonumentForm::Obelisk, MonumentForm::StandingStone};
  const int centre = kind >= 0 ? kind : c.props.centre;
  MonumentForm f = centre >= 0 && centre < (int)(sizeof(byCentre) / sizeof(byCentre[0])) ? byCentre[centre] : MonumentForm::Statue;
  // the cultures' own monuments where the centrepiece is a statue or an obelisk
  if (f == MonumentForm::Statue) {
    switch (c.archetype) {
      case A::Fjordfolk: case A::Steppe: case A::Marsh: case A::Sylvan: f = MonumentForm::Totem; break;
      case A::Jade: f = MonumentForm::Stupa; break;
      case A::SunTemple: f = MonumentForm::Stele; break;
      case A::Highland: f = MonumentForm::Cairn; break;
      default: break;
    }
  }
  return monumentOf((int)c.archetype + 1, f, seed, c.props.stone, c.props.metal);
}

}  // namespace bld
