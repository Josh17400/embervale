// rpg/core/art_ids.h: the art vocabulary every layer may name without the painters: Prop (with its predicates,
// footprints and stall geometry), HumanLook (Hair, Outfit), Monster and MonsterLook, Building (with BuildingFacts),
// Fx, Posture and Species. Header-only; includes nothing from rpg/ outside rpg/core. Anyone may include it.
// The painters that draw these live in rpg/art/art_*.h (each includes this file); HumanLook::key() is in art_human.cpp.
#pragma once
#include <cstdint>
#include "engine/pix.h"   // rgba() (HumanLook's default colours)

namespace art {

// ---------------------------------------------------------------- props (world objects)
// Each prop is its own canvas with its natural size (propW x propH). Anchor = bottom-centre of the canvas
// sits on the bottom-centre of the prop's tile. "frames" > 1 means the canvas holds that many frames side by side
// (each propW wide) for animation (torches, campfire, fountain).
enum class Prop : uint8_t {
  // nature
  OakTree, OakTree2, PineTree, PineTree2, SnowPine, BirchTree, DeadTree, WillowTree, PalmTree, AutumnTree,
  Bush, BerryBush, SnowBush, Boulder, Rock, MossRock, SnowRock, Stump, Log,
  Flowers1, Flowers2, Flowers3, TallGrass, Reeds, Cactus, Mushrooms, LilyPad, Fern,
  // camp / civilisation
  Chest, ChestOpen, Barrel, Crate, Torch, Campfire, Signpost, Well, FenceH, FenceV, Lamppost, Tent,
  MarketStall, Haystack, Anvil, Cart, Fountain, Statue, Banner, Woodpile, Gravestone, Shrine,
  // dungeon
  CaveEntrance,   // rock arch with dark opening, ~48x32, sits on a mountain/cliff edge
  Ladder,         // ladder up out of a cave (exit), 16x24
  Stalagmite, Crystal, Bones, SkullPile, Cobweb, Brazier, Coffin, Urn, IronDoor, Altar,
  // interior furniture
  Bed, Table, Chair, Shelf, Fireplace, Rug, Counter, Barrel2, PlantPot, Cauldron, Bookshelf, Throne,
  // ---- M0 interiors (WORLDGEN_V3 genInterior). Wall decor hangs on the back wall: it is placed on the wall tile
  //      (row 1 of an interior) and its canvas bottom is the foot of the wall.
  Tapestry, WallShelf, HerbBundle, Antlers, Painting, Sconce, Window, WallShield, ToolRack, PanRack, Wreath, HolySymbol,
  // furniture. One-tile pieces unless noted; Hearth and Forge are 3 tiles wide (the two side tiles hold Filler).
  // Long tables and counters are built from L/M/R segments that join seamlessly.
  Cupboard, Wardrobe, Stool, Bench, Cradle, SpinningWheel, Loom, Desk, Workbench, Hearth, Forge,
  TableSmall, TableMeal, TableWork, TableL, TableM, TableR, CounterL, CounterM, CounterR,
  Filler,         // invisible, solid: the extra footprint tiles of a multi-tile piece
  // ---- M0b rooms and storeys (WORLDGEN_V7 genInterior; rpg/sim/rooms.h has the geometry contract). Phase A painted
  //      placeholders; the interior-art lane paints them for real. Stairs and doors are walk-through.
  StairsUp,       // a flight climbing north into the wall face above its tile: the steps fill the tile and rise over the
                  // face into a dark opening (16x40). Walking onto it takes you up a floor
  StairsDown,     // the head of a flight going down: a railed stairwell opening in the floor (16x24)
  DoorH,          // an interior door in an E-W partition, on its face-row doorway tile: frame, lintel over the cap row,
                  // the leaf standing open (16x36)
  DoorV,          // an interior door in an N-S partition, seen edge-on in its frame (16x32)
  Nightstand,     // a bedside table with a candle (12x16)
  Washstand,      // basin and jug on a stand (14x20)
  Oven,           // a domed bread oven / cook stove against a wall, fire glowing in its mouth (16x28, 4 frames)
  PrepTable,      // a kitchen work table: chopping board, knife, vegetables (16x18)
  BottleShelf,    // a bar's back shelf of bottles and mugs, against a wall (16x34)
  WeaponRack,     // spears, swords and a shield in a rack, against a wall (16x30)
  Lectern,        // a reading stand with an open book (12x20)
  Candelabra,     // a tall standing candle-holder (10x28, 4 frames)
  DisplayTable,   // a shop's table with wares laid out (16x18)
  QuenchTub,      // a smith's quenching tub with tongs (16x16)
  Grindstone,     // a treadle grindstone (16x20)
  Pillar,         // a stone column for halls, naves and keeps (16x48)
  BunkBed,        // a two-tier bunk, head to the wall (20x40)
  Dresser,        // a chest of drawers with a mirror, against a wall (16x30)
  // ---- M1 ruins: the standing remains of old halls (overworld ruin sites). The view draws each tile with its own
  //      variant (ruinVariant), so a run of wall never repeats one broken top.
  RuinWall,       // a tile of broken masonry wall in 3/4 view: a 16 px deep top over a south face (16x36). Runs join
                  // seamlessly side by side, and north-south runs show one continuous wall top
  RuinColumn,     // a snapped-off column on its plinth (16x40)
  // ---- M1 economy (owner 2026-10-05; rpg/world/economy.h). Market stalls by trade: a counter three tiles wide under a
  //      sloping awning, the trade's goods on the counter and hanging under the awning, facing any of four ways (see
  //      StallFacing below: the prop's tile and its Filler say which). The counter is solid; the stall keeper stands on
  //      the tile behind it. The view varies the awning cloth per stall (marketStallFacing).
  StallProduce, StallFish, StallCloth, StallPottery, StallMeat, StallBread, StallTools,
  StallTimber,    // (M1 fixer) the lumber trade's stall: sawn planks, split firewood and a log or two, an axe on the rail
  //      market and trade clutter, and the specialisations' yard machinery
  Sacks,          // a heap of grain / flour sacks (20x16)
  Baskets,        // two wicker baskets of produce (20x14)
  DryingRack,     // a fish-drying rack: a pole frame hung with split fish (30x30)
  HideRack,       // a tanner's stretching frame with a hide laced in it (24x28)
  OreCart,        // a mine cart heaped with ore on a stub of rail (26x22)
  OrePile,        // a heap of broken ore (22x14)
  MineEntrance,   // a timbered adit into the hillside (44x36); stands on its middle tile, Filler either side
  LogPile,        // trunks stacked for the sawmill (34x20)
  Trough,         // a wooden water trough for the beasts (26x14)
  WaterWheel,     // a watermill's undershot wheel turning in the river beside the mill (24x34, 4 frames); stands on the
                  // water tile against the mill's side wall
  // ---- M1 fixer round 2: a grand market has more than one kind of stall, and the trades' work yards
  MarketTable,    // an open trestle table of goods two tiles wide, sometimes under a square sunshade (48x44). Stands on
                  // its WEST tile (the canvas's left third is empty), Filler on the east tile; the view picks its goods
                  // and shade by tile (marketTable; on the overworld the view draws that 3/4 model's own canvas at
                  // marketTableOrigin, likewise the ground cloth and Cart)
  GroundCloth,    // a cloth spread on the paving with goods laid out on it, two tiles wide (48x20); like MarketTable
  MarketCross,    // a market cross: a stepped stone base, a shaft and a carved head (32x60), a market's centrepiece
  Sheep,          // a grazing sheep (20x18, 8 frames: head down cropping, then up)
  Cow,            // a grazing cow (30x24, 8 frames)
  MineRail,       // a tile of mine-cart track (16x16, flat, walk-over); the view joins it to its neighbours (mineRail)
  PenShelter,     // an open-fronted lean-to shed for the flock, hay in its rack (48x44); middle tile, Filler either side
  MineHill,       // the mine: a grassy knoll, its south face a cliff of layered rock with the timbered adit cut into its
                  // foot (80x76); stands on the adit's tile, the knoll's five-by-four footprint all Filler (mineHill)
  // ---- M3 culture furniture (ArchStyle::furniture, VISION_PLAN 5.6 "chairs against cushions and low tables against
  //      hammocks"). Phase A placeholders (sizes below are the canvases; the ARCHITECTURE lane paints them in 3/4 view)
  Cushion,        // floor cushions round a low table (16x12); walk-over, like a chair
  LowTable,       // a low table for floor seating, a tray and cups on it (20x16)
  Hammock,        // a hammock slung between two posts / wall hooks, head to the wall (28x22)
  SleepingMat,    // a rolled-out sleeping mat with a bolster and blanket (yurts, marsh huts) (24x14); walk-over
  // ---- M2 Wayfinder: mountains, the wayside places (rpg/world/poi.h vignettes) and the wonders. Painted in
  //      rpg/art/art_wild.cpp (phase A placeholders; the art owner paints them for real and may change canvas sizes).
  //      FOOTPRINTS ARE FROZEN (wildFootprint below): the prop stands on the bottom-centre tile of its footprint and the
  //      generator puts Filler on every other footprint tile.
  Peak,           // a rock spire with ridgelines and a snow cap where it is cold (about 48x64), y-sorted like a tree,
                  // with a ground shadow; the view draws peakVariant(v, land) per tile so a range never repeats one peak
  StandingStone,  // a tall weathered menhir with lichen (about 16x34); a ring of them is a vignette
  CaravanWreck,   // an overturned covered wagon, a wheel up, its canvas torn, crates spilled (about 48x32)
  WatchtowerRuin, // a broken round stone watchtower with a timber nest (about 48x72)
  FishingShack,   // a plank fishing shack on low stilts with nets hung out (about 48x44)
  TollPost,       // a road toll post: a striped boom on a post, a toll box and a crude sign (about 16x30)
  HerbBed,        // a low timber-edged bed of herbs in rows (about 16x14)
  GraveCairn,     // a lone grave: a cairn of stones and a weathered marker (about 20x24)
  Bedroll,        // a bedroll beside the embers of a roadside fire (about 24x12); flat, walked over
  ElderTree,      // a colossal ancient tree, its roots spreading over the ground (about 96x112)
  Colossus,       // the weathered statue of a forgotten king, cracked and mossy (about 48x112)
  StarShard,      // a jagged glowing shard of a fallen star (about 16x28, 4 frames: the glow pulses); a light source
  DragonBones,    // the skull, spine and ribs of an ancient dragon bleaching in the grass (about 112x56)
  GreatPeak,      // (M2 fixer round 2) a whole mountain massif (136x140): the summit of a named peak or a range's crest,
                  // flanked by Peaks; drawn as peakVariant(8 + v, land)
  // ---- M3b interiors (VISION_PLAN 15.14: interiors derived from the builder's blueprints). Furniture, classified as
  //      bld::Kind::Furniture in rpg/build/registry.cpp. (Not wild props: art_props.cpp's own wild test stops at
  //      GreatPeak.)
  FirePitL,       // a long hearth down a hall's middle: a stone-kerbed trench of embers and low flames, west end (16x24, 4 frames)
  FirePitM,       // its middle segments: the trench runs on east and west
  FirePitR,       // its east end
  Stove,          // a yurt's iron stove under the crown ring: a firebox, a kettle, the flue pipe rising (16x44, 4 frames)
  TrainingDummy,  // a warrior lodge's straw dummy on a post, a battered shield on its arm (16x30)
  FoldScreen,     // a folding screen of painted panels (paper, silk or reed by the room), stands on the floor (24x26)
  // ---- M3c Wildlands flora (VISION_PLAN 15.15 / 15.16): the biomes' own plants and rocks. Painted in
  //      rpg/art/art_flora.cpp (phase A: stand-ins; the LAND lane paints them for real in the 3/4 top-down view with
  //      top-left light and may change the canvas sizes). The view draws floraVariant(p, v, eco) per tile (a wood is
  //      never one tree cloned). All stand on ONE tile (a tree's crown spreads over its neighbours like the classic
  //      trees'; the forest rule keeps a free tile between solid ones). Trees, solid:
  AcaciaTree,     // a flat umbrella crown on a forked trunk (savanna, scrubland, oasis) (about 40x40)
  BaobabTree,     // a swollen bottle trunk, a small crown of stubby branches (savanna) (about 36x44)
  GiantTree,      // an old-growth giant: massive buttressed trunk, deep crown (old-growth forest) (about 48x64)
  GnarledTree,    // a twisted near-black tree, a thin dark crown (dark forest, blight edges) (about 36x44)
  BlossomTree,    // a cherry in full bloom, pink crown (blossom grove) (about 36x40)
  BambooClump,    // a clump of tall jointed stems with leaf sprays (bamboo forest) (about 24x48)
  JungleTree,     // a tall broadleaf, buttress roots, hanging vines (jungle) (about 44x56)
  GiantMushroom,  // a mushroom the size of a tree, glowing gills at night (mushroom forest) (about 36x44)
  SilverTree,     // a slender pale tree with silver-green leaves (silverwood) (about 32x52)
  MangroveTree,   // a crown on arching stilt roots (mangrove coast) (about 40x40)
  SwampCypress,   // a buttressed cypress with knees and hanging moss (flooded forest, bogs) (about 32x48)
  PetrifiedTree,  // a stone trunk, standing or broken, banded red and grey (petrified forest) (about 24x40)
  LarchTree,      // a golden-green larch (taiga bog, alpine meadows) (about 28x48)
  JuniperTree,    // a low wind-bent juniper (heath, chalk downs, sea cliffs, steppe) (about 28x28)
  //      rocks and shrubs, solid:
  Hoodoo,         // a banded sandstone pillar with a cap rock (badlands) (about 24x44)
  CrystalSpire,   // a cluster of tall glowing crystals (crystal barrens; a light source) (about 24x40, 4 frames)
  BasaltColumns,  // a knot of hexagonal basalt columns (ash fields, sea cliffs) (about 28x32)
  IceSerac,       // a blue-white ice block, its lit face cracked (glacier) (about 28x34)
  TermiteMound,   // a tall ridged earth mound (savanna) (about 16x28)
  AshVent,        // a smoking fissure ringed with sulphur (ash fields; a light source) (about 20x16, 4 frames)
  Gorse,          // a thorny gorse bush, yellow flowers (heath, chalk downs) (about 18x16)
  Thornbush,      // a grey thorn scrub (scrubland, steppe, badlands) (about 18x16)
  //      ground cover, walked over (drawn under feet like TallGrass):
  Heather,        // purple heather tussocks (heath and moor)
  Wildflowers,    // a dense patch of mixed flowers (flower meadows, alpine meadows)
  PrairieGrass,   // tall golden grass (prairie, savanna, steppe)
  CottonGrass,    // white cotton-grass tufts (bogs, tundra)
  Agave,          // a low spiky succulent rosette (scrubland, stony desert)
  DryBrush,       // dry twiggy brush / tumbleweed (desert, steppe)
  Saltbush,       // grey-blue saltbush (salt flats' rims)
  Lichen,         // lichen and moss cushions on stones (tundra, glacier edges)
  Wrack,          // seaweed and driftwood on the tideline (shingle, beaches)
  Shells,         // shells and coral pieces on the sand (coral strand)
  GlowCaps,       // a ring of small glowing mushrooms (mushroom forest; a faint light at night)
  Blightweed,     // black thorny tendrils (blighted land)
  SilverFern,     // pale silver ferns (silverwood)
  JungleFern,     // big broad leaves (jungle, bamboo forest)
  Petals,         // fallen blossom petals on the ground (blossom grove)
  // ---- M4 Banners: the war made visible (VISION_PLAN 4.5). Painted in rpg/art/art_war.cpp (phase A stand-ins; the
  //      WARDS lane paints them for real in the 3/4 top-down view with top-left light and may change the canvas
  //      sizes). Placed at RUNTIME by the war's overlays (never by the generator). FOOTPRINTS ARE FROZEN (m4Footprint
  //      below: the prop stands on its footprint's bottom-centre tile, Filler on the others). Colours that belong to a
  //      kingdom (tent cloth, pennants) are drawn by the view from the owner's arms (WARDS lane, render code).
  WarTent,        // a soldier's ridge tent in the attacker's cloth, guy ropes, a pennant (about 48x36); 3x2
  CommandTent,    // the commander's round pavilion, a banner pole on its crown (about 52x52); 3x2
  Catapult,       // a timber siege engine, its arm cocked, a stone in the sling (about 48x44); 3x2
  Palisade,       // a run of sharpened stakes; joins its neighbours (16x24); solid
  RefugeeTent,    // a patched lean-to of blankets on poles, bundles beside it (about 44x30); 3x2
  Rubble,         // a heap of fallen stone and charred beams where a building stood (about 20x16); solid
  Ash,            // a scorch of ash and cinders on the ground (16x16); walked over, drawn flat
  Scaffold,       // timber scaffolding and a ladder against a wall being rebuilt (16x40); solid
  Barricade,      // a checkpoint's barrier of crossed timbers on a bridge or road (24x22); solid
  // ---- M4 Banners: the realm's voice and the lore of fallen places (VISION_PLAN 4.6, 15.3). Painted in
  //      rpg/art/art_lore.cpp (phase A stand-ins; the STORY lane paints them for real). Placed at runtime (notice
  //      boards in town squares) or by the ruin generator (dungeon.cpp, STORY lane).
  NoticeBoard,    // a roofed timber board pinned with notices: bounties, proclamations, houses for sale (24x34); solid
  Inscription,    // a carved standing slab in a dead culture's script (16x30); solid
  ToppledStatue,  // a fallen statue of a named ruler, its head broken off (about 48x24); 3x1, solid
  Mural,          // a faded painted wall in a ruin: a battle, a coronation (16x30, wall decor like Tapestry)
  NamedGrave,     // a carved grave with a name and the year (16x22); solid
  LostJournal,    // a satchel and a journal fallen open on the floor (14x10); walked over
  // ---- M7 Home (VISION_PLAN 8.1, 8.2): painted in rpg/art/art_home.cpp (ART lane; phase A stand-in). Placed at runtime
  //      by home::stampWindow on lots and vacant houses for sale (or by the settlement generator on its lots).
  ForSaleSign,    // a post with a hanging board, "FOR SALE" in the culture's letters (about 16x28); solid
  // ---- M7b Cities (VISION_PLAN 15.23, 15.24): street furniture, parks, gardens and souks. Painted in rpg/art/art_city.cpp
  //      (CITYART lane; phase A stand-ins, the lane paints them for real in the 3/4 top-down view with top-left light
  //      and may change the canvas sizes). Placed by the settlement generator (CITY lane) in cities (a town's square at
  //      most), NEVER in a village. FOOTPRINTS ARE FROZEN (cityFootprint below).
  Bollard,        // a squat stone bollard / corner stone at a junction, a square's edge or a gate (about 10x16); solid
  Planter,        // a planter box of flowers or a clipped shrub, stone in the rich quarter, timber elsewhere (16x18); solid
  GardenWall,     // a run of low garden wall with a coping (16x24); joins its neighbours (like Palisade); solid
  Hedge,          // a clipped hedge (16x22); joins its neighbours; solid
  FlowerBed,      // a kerbed bed of flowers in a park, a close or a rich court (16x14); solid (walked round)
  FruitTree,      // an orchard / courtyard fruit tree (apple, pear, orange or pomegranate by the climate) (28x34); solid
  Monument,       // a tall column or obelisk on a stepped plinth, the seat's square (20x64); solid
  Pavilion,       // a park's bandstand / gazebo in the culture's style (48x48); 3x2 footprint, solid
  Canopy,         // a cloth awning strung over a souk street (16x40): walked UNDER (not solid), drawn over the walker
  // ---- M10 Beyond the Mist (VISION_PLAN 2.12, 15.4, 15.31): harbours, piers and the sea's places. Painted in
  //      rpg/art/art_sea.cpp (phase A stand-ins; the ART lane paints them for real in the 3/4 top-down view with
  //      top-left light and may change the canvas sizes). Placed by the WORLD lane (ports' harbours and jetties on
  //      Ground::Bridge piers and the shore; wrecks, lighthouses). FOOTPRINTS ARE FROZEN (seaFootprint below).
  MooringPost,    // a timber bollard / pile on a pier's edge, a rope coiled on it (10x18); solid
  NetRack,        // fishing nets hung to dry on a pole frame (about 28x24); solid
  FishBaskets,    // wicker baskets and a crate of the catch on a pier (16x14); solid
  Buoy,           // a painted buoy bobbing on the water (12x14, 4 frames); on a water tile, solid to boats
  Anchor,         // a great iron anchor leaning on a coil of chain (16x18); solid
  CargoPile,      // crates, barrels and sacks on the quay (about 24x22); solid
  Slipway,        // the shipwright's slipway: a hull on its cradle with ribs half planked (about 48x36); 3x2, solid
  HarbourDesk,    // the harbourmaster's booth on the pier: a roofed desk, a ledger, a lantern (about 28x34); solid
  Lighthouse,     // a lighthouse tower on a headland, its lamp lit at night (about 32x80); 1x1 base, solid (a light)
  Wreck,          // a ship's broken hull on a reef or a beach, ribs and a snapped mast (about 56x40); 3x2, solid
  // ---- M11 High Ground (VISION_PLAN 11.3, 13 M11, 15.32): the summit shrine and its trail. Painted in
  //      rpg/art/art_relief.cpp (phase A stand-ins; the FALLS lane paints them for real in the 3/4 top-down view with
  //      top-left light and may change the canvas sizes). Placed by the WORLD lane (a summit shrine on the top level of a
  //      named peak or a range's crest, cairns along the trail up, prayer flags by the shrine). FOOTPRINTS ARE FROZEN
  //      (reliefFootprint below).
  SummitShrine,   // an old open shrine on the summit: a stepped stone dais, a weathered idol or altar under a small
                  // roof, offerings, two braziers lit at night (about 48x48); 3x2, solid (a light)
  Cairn,          // a waymarker of stacked stones on the trail up (about 14x20); solid
  PrayerFlags,    // a line of faded cloth flags strung between two poles, fluttering (about 32x28, 4 frames); solid
  // ---- M12 Rise and Fall (VISION_PLAN 13 M12, 15.33): the player's (and a warlord's) standard and the watchtower a lord
  //      raises. Painted in rpg/art/art_rise.cpp (phase A stand-ins; the VIEW lane paints them for real and draws them
  //      in the owner's colours through the view, as the war props are). FOOTPRINTS ARE FROZEN (riseFootprint below).
  //      PROP BUDGET: Prop is a uint8_t and the chunk caches store prop + 1 (0 none), so Prop::COUNT may not pass 255.
  //      After these two the ONE slot left is reserved: anything else M12 shows (works, warlord forts, resettlers) is
  //      drawn by the view or reuses existing props (Palisade, WarTent, Scaffold, RefugeeTent, Shrine, Barricade...).
  LordBanner,     // a tall standard on a pole: the lord's (or a warlord's) arms on a long banner, waving (about 20x48,
                  // 4 frames); solid
  Watchtower,     // a timber watchtower on four legs with a roofed platform and a ladder (about 32x64); 1x1 base, solid
  COUNT
};
// M4: the war's props (art_war.cpp) and the lore props (art_lore.cpp)
inline bool isWarProp(Prop p) { return (int)p >= (int)Prop::WarTent && (int)p <= (int)Prop::Barricade; }
inline bool isLoreProp(Prop p) { return (int)p >= (int)Prop::NoticeBoard && (int)p <= (int)Prop::LostJournal; }
// M7: the home props (art_home.cpp)
inline bool isHomeProp(Prop p) { return p == Prop::ForSaleSign; }
// M7b: the city props (art_city.cpp) and their frozen footprints (w odd, h >= 1: the prop stands on the bottom row's
// middle tile, Filler on the rest of the box; 1 x 1 for the rest)
inline bool isCityProp(Prop p) { return (int)p >= (int)Prop::Bollard && (int)p <= (int)Prop::Canopy; }
// M10: the sea props (art_sea.cpp) and their frozen footprints (as the city props': w odd, the prop on the bottom row's
// middle tile, Filler on the rest)
inline bool isSeaProp(Prop p) { return (int)p >= (int)Prop::MooringPost && (int)p <= (int)Prop::Wreck; }
// M11: the relief props (art_relief.cpp) and their frozen footprints (as the sea props': w odd, the prop on the bottom
// row's middle tile, Filler on the rest)
inline bool isReliefProp(Prop p) { return (int)p >= (int)Prop::SummitShrine && (int)p <= (int)Prop::PrayerFlags; }
// M12: the rise props (art_rise.cpp) and their frozen footprints (1 x 1 each: the tower stands on its base tile)
inline bool isRiseProp(Prop p) { return p == Prop::LordBanner || p == Prop::Watchtower; }
inline void riseFootprint(Prop p, int& w, int& h) { (void)p; w = 1; h = 1; }
inline void reliefFootprint(Prop p, int& w, int& h) {
  if (p == Prop::SummitShrine) { w = 3; h = 2; return; }
  w = 1; h = 1;
}
inline void seaFootprint(Prop p, int& w, int& h) {
  if (p == Prop::Slipway || p == Prop::Wreck) { w = 3; h = 2; return; }
  w = 1; h = 1;
}
inline void cityFootprint(Prop p, int& w, int& h) {
  if (p == Prop::Pavilion) { w = 3; h = 2; return; }
  w = 1; h = 1;
}
// M4: the frozen tile footprint of a war or lore prop (w odd, h >= 1): it stands on the bottom row's middle tile; the
// rest of the w x h box holds Filler. 1 x 1 for the rest.
inline void m4Footprint(Prop p, int& w, int& h) {
  switch (p) {
    case Prop::WarTent: case Prop::CommandTent: case Prop::Catapult: case Prop::RefugeeTent: w = 3; h = 2; return;
    case Prop::ToppledStatue: w = 3; h = 1; return;
    default: w = 1; h = 1; return;
  }
}
// M3c: the Wildlands flora ranges (one shared answer for every reader: the forest rule, crowns, the map, projectiles)
inline bool isWildlandsFlora(Prop p) { return (int)p >= (int)Prop::AcaciaTree && (int)p <= (int)Prop::Petals; }
inline bool isWildlandsSolid(Prop p) { return (int)p >= (int)Prop::AcaciaTree && (int)p <= (int)Prop::Thornbush; }
// a tree (solid trunk, a crown the view thins when the hero walks behind it): the classic OakTree .. AutumnTree and the
// M3c trees (ElderTree, a 5x3 wonder, is not one)
inline bool isTreeProp(Prop p) {
  return ((int)p >= (int)Prop::OakTree && (int)p <= (int)Prop::AutumnTree) || ((int)p >= (int)Prop::AcaciaTree && (int)p <= (int)Prop::JuniperTree) ||
         p == Prop::FruitTree;   // (M7b) the orchards' and courtyards' fruit trees
}
// plants, rocks and ground cover of the wild (the classic OakTree .. Fern and the M3c flora)
inline bool isFloraProp(Prop p) { return (int)p <= (int)Prop::Fern || isWildlandsFlora(p); }
// a solid wild prop the forest rule spaces out (trees, bushes, stumps, logs, rocks, boulders, cacti, the M3c solids)
inline bool isWildSolidProp(Prop p) {
  if (isTreeProp(p) || isWildlandsSolid(p)) return true;
  switch (p) {
    case Prop::Bush: case Prop::BerryBush: case Prop::SnowBush: case Prop::Boulder: case Prop::Rock: case Prop::MossRock:
    case Prop::SnowRock: case Prop::Stump: case Prop::Log: case Prop::Cactus: return true;
    default: return false;
  }
}
// M2: the frozen tile footprint of a wayside / wonder prop (w odd, h >= 1): the prop stands on the bottom row's middle
// tile; every other tile of the w x h box (rows above and columns either side) holds Filler. 1 x 1 for the rest.
inline void wildFootprint(Prop p, int& w, int& h) {
  switch (p) {
    case Prop::Peak: w = 3; h = 2; return;
    case Prop::CaravanWreck: w = 3; h = 1; return;
    case Prop::WatchtowerRuin: w = 3; h = 2; return;
    case Prop::FishingShack: w = 3; h = 2; return;
    case Prop::ElderTree: w = 5; h = 3; return;
    case Prop::Colossus: w = 3; h = 2; return;
    case Prop::DragonBones: w = 7; h = 2; return;
    case Prop::GreatPeak: w = 7; h = 3; return;
    default: w = 1; h = 1; return;
  }
}
inline bool isWildProp(Prop p) { return (int)p >= (int)Prop::Peak && (int)p <= (int)Prop::GreatPeak; }
// M1 economy: the market stall trades in prop order (ew::StallTrade); stallProp(t) and back
constexpr int kStallTrades = 8;
inline Prop stallProp(int trade) { return (Prop)((int)Prop::StallProduce + (trade % kStallTrades)); }
inline bool isStall(Prop p) { return (int)p >= (int)Prop::StallProduce && (int)p <= (int)Prop::StallTimber; }
inline int stallTrade(Prop p) { return (int)p - (int)Prop::StallProduce; }
inline bool isVendorProp(Prop p) { return isStall(p) || p == Prop::MarketTable || p == Prop::GroundCloth; }
// ---- (stall facings, owner 2026-10-05: "the stalls look like a picture on a wall, and all face the screen") Every
// stall is painted in the game's high 3/4 projection (the buildings' oblique: ground depth 1:1, height 1:1 up the
// screen) as a 3D model, in one of four facings: 0 S (its customers south: the open front toward the camera), 1 N (its
// customers north: we see its back), 2 E and 3 W (its customers east / west: side profiles). The tiles:
//  - S / N: the counter runs east-west over three tiles, the prop on the middle one, Filler west and east; the
//    keeper's row (walkable, under the awning) is north of it (S) or south of it (N);
//  - E / W: the counter runs north-south over three tiles, the prop on the SOUTH one, Filler on the two north of it;
//    the keeper's column is west of it (E) or east of it (W).
// Which of each pair by the parity of the prop's GLOBAL tile (the generator picks the row or column to suit), so the
// map needs no extra data: S / N by gy (even S), E / W by gx (even E).
constexpr int kStallFacings = 4;
enum StallFacing : int { StallS = 0, StallN = 1, StallE = 2, StallW = 3 };
inline int stallFacingOf(bool alongNS, int32_t gx, int32_t gy) { return alongNS ? ((gx & 1) ? StallW : StallE) : ((gy & 1) ? StallN : StallS); }
// the facing of the stall whose prop stands on window tile (x, y); prop(x, y) returns Map::propAt (Prop + 1). An
// east-west counter has Filler either side of its prop; a north-south one has Filler on the two tiles north of it.
template <class PropAt>
int stallFacingAt(PropAt&& prop, int x, int y, int32_t ox, int32_t oy) {
  const int F = (int)Prop::Filler + 1;
  const bool ew = prop(x - 1, y) == F && prop(x + 1, y) == F;
  const bool ns = !ew && prop(x, y - 1) == F && prop(x, y - 2) == F;
  return stallFacingOf(ns, x + ox, y + oy);
}
// a stall's tiles (dx, dy from its prop tile, the prop's own first): the three solid counter tiles
inline void stallCounterTile(int facing, int i, int& dx, int& dy) {
  if (facing <= StallN) { dx = i == 0 ? 0 : (i == 1 ? -1 : 1); dy = 0; }
  else { dx = 0; dy = -i; }
}
// the stall keeper of a stall facing f: the tile they walk up to (post, from the prop tile), the point they stand on
// (px from the prop tile's top-left) and which way they look (Actor::face: 0 S, 1 N, 2 E, 3 W: toward the customers).
// Customers stand across the counter (talk spot: custDx / custDy from the prop tile). (stalls fixer round 3, owner:
// "why are the vendors standing on the end?") Always behind the MIDDLE of the counter, on its inside, facing out:
//  - S: inside the counter's tile against its back edge (the counter hides their legs, the head shows under the eave);
//  - N: in their row a little behind the counter (we see them from behind under the roof's high back edge);
//  - E / W: in the middle of their column beside the counter's middle tile (post one tile in and one north of the
//    prop), under the full side awning (art_market.cpp geoOf; awnings fixer, owner: "Don't worry if it covers the
//    NPC"): the roof draws over them.
struct StallKeeperSpot { int postDx, postDy; float standX, standY; int face; int custDx, custDy; };
inline StallKeeperSpot stallKeeperSpot(int facing) {
  switch (facing) {
    case StallN: return {0, 1, 8.0f, 21.0f, 1, 0, -1};
    case StallE: return {-1, -1, -8.0f, -8.0f, 2, 1, -1};
    case StallW: return {1, -1, 24.0f, -8.0f, 3, -1, -1};
    default: return {0, -1, 8.0f, 2.0f, 0, 0, 1};
  }
}
// the vendor a keeper posted on window tile (tx, ty) keeps: a stall whose keeper's post that tile is (any facing), else
// an open table or cloth just south of it. Gives the vendor's prop tile and facing (S for tables and cloths).
template <class PropAt>
bool stallOfPost(PropAt&& prop, int tx, int ty, int32_t ox, int32_t oy, int& sx, int& sy, int& facing) {
  for (int f = 0; f < kStallFacings; f++) {
    const StallKeeperSpot k = stallKeeperSpot(f);
    const int x = tx - k.postDx, y = ty - k.postDy, q = prop(x, y);
    if (q && isStall((Prop)(q - 1)) && stallFacingAt(prop, x, y, ox, oy) == f) { sx = x; sy = y; facing = f; return true; }
  }
  const int q = prop(tx, ty + 1);
  if (q && isVendorProp((Prop)(q - 1))) { sx = tx; sy = ty + 1; facing = StallS; return true; }
  return false;
}
// (stalls fixer round 3) the stall whose keeper stands at (hx, hy) (px, window: Actor::home), from the keeper spots of
// the four facings (a side or back keeper stands on their post tile, not on the prop's). Leaves sx, sy, facing alone
// and returns false when no stall's keeper stands there (a table's or a cloth's seller)
template <class PropAt>
bool stallOfKeeper(PropAt&& prop, float hx, float hy, int32_t ox, int32_t oy, int& sx, int& sy, int& facing) {
  for (int f = 0; f < kStallFacings; f++) {
    const StallKeeperSpot k = stallKeeperSpot(f);
    const float fx = (hx - k.standX) / 16.0f, fy = (hy - k.standY) / 16.0f;
    const int x = (int)(fx < 0 ? fx - 0.5f : fx + 0.5f), y = (int)(fy < 0 ? fy - 0.5f : fy + 0.5f);
    const float ex = x * 16.0f + k.standX - hx, ey = y * 16.0f + k.standY - hy;
    if (ex * ex + ey * ey > 0.25f) continue;
    const int q = prop(x, y);
    if (q && isStall((Prop)(q - 1)) && stallFacingAt(prop, x, y, ox, oy) == f) { sx = x; sy = y; facing = f; return true; }
  }
  return false;
}
// (stalls fixer round 3) the y the view sorts a stall by, px from its prop tile's top. N: just north of where its keeper
// stands (stallKeeperSpot standY), so the keeper draws after the stall. S: after them at 15 (the counter stands in
// front of the keeper). E / W (awnings fixer): after them at 15 too, so the full side awning draws over its keeper
// (and over anyone in the keeper's column under it) instead of the keeper standing on the roof
inline float stallSortY(int facing) { return facing == StallN ? stallKeeperSpot(facing).standY - 1.0f : 15.0f; }

// Values are saved (Appearance::hair): append only. Bun and Curls arrived with the M0 character creator.
enum class Hair : uint8_t { Bald, Short, Long, Ponytail, Mohawk, Braids, Bun, Curls, COUNT };
enum class Outfit : uint8_t {
  Tunic,      // shirt + trousers (villager / player default)
  Dress,      // long skirt
  Robe,       // mage / priest, hooded optional
  Leather,    // leather armour (brown, studs)
  Chain,      // iron chainmail (grey)
  Plate,      // steel plate (bright steel, pauldrons)
  Elven,      // gold-green ornate armour
  Ebony,      // black/purple dark armour
  Guard,      // city guard: chain + tabard in tabardColor
  Rags,       // beggar / prisoner
  COUNT
};

struct HumanLook {
  uint32_t skin = rgba(232, 184, 140);
  uint32_t hairColor = rgba(90, 56, 30);
  uint32_t topColor = rgba(60, 100, 160);     // tunic/dress/robe/tabard main colour
  uint32_t bottomColor = rgba(80, 64, 50);    // trousers
  uint32_t tabardColor = rgba(150, 40, 40);   // Guard outfit trim, robe trim
  Hair hair = Hair::Short;
  Outfit outfit = Outfit::Tunic;
  bool beard = false;
  bool helmet = false;   // drawn over hair, style follows outfit (leather cap, iron helm, steel great-helm, elven, ebony horns)
  bool hood = false;     // cloth hood (robes, bandits)
  bool cape = false;     // cape behind (visible esp. from behind), colour = tabardColor
  bool shield = false;   // round shield on off-hand
  uint8_t weapon = 0;    // 0 none, 1 sword, 2 axe, 3 bow, 4 staff, 5 dagger, 6 hammer/pick, (M4) 8 spear / polearm, 9 walking
                         // stick — drawn in hand, swings in frames 5/6
  uint32_t weaponColor = rgba(200, 205, 215);   // blade/metal colour (tier tint)

  // ---- M0 equipment and appearance looks. Every default reproduces the pre-M0 pixels exactly (art_hash checks it),
  //      so NPC looks built before these fields existed are unchanged. 0 always means "none / legacy behaviour".
  //      Armour pieces share one material BAND id: 1 leather, 2 iron, 3 steel, 4 gilded, 5 jade, 6 obsidian,
  //      7 emberforged (see kBand* below; the player's item tiers map onto it in rpg/sim/player.cpp).
  uint8_t build = 0;        // body build: 0 average, 1 slim (narrow waist), 2 broad (heavy shoulders, deep chest)
  uint32_t eyeColor = 0;    // 0 = the palette's default eye colour
  uint8_t helmStyle = 0;    // 0 = legacy (style follows outfit when helmet is set), else a helmet BAND (always drawn):
                            //   leather cap, iron nasal helm, steel great-helm, gilded winged, jade crested, obsidian horns,
                            //   emberforged horned crown
  uint8_t armorStyle = 0;   // 0 = legacy (outfit decides), else a body-armour BAND (overrides the outfit's torso/legs)
  uint8_t gloves = 0;       // 0 none, else a glove BAND (leather gloves .. gauntlets with a flared cuff)
  uint8_t boots = 0;        // 0 none (legacy shoes), else a boot BAND (taller boots; metal greaves from steel up)
  uint8_t cloak = 0;        // 0 none, else a cloak style: 1 hooded travel cloak, 2 fur mantle, 3 long noble cloak with
                            //   gilt trim; colour = cloakColor. Replaces `cape` when set.
  uint32_t cloakColor = rgba(110, 40, 40);
  uint8_t shieldStyle = 0;  // 0 = legacy round shield (when shield is set), else 1 round, 2 heater, 3 kite, 4 tower
                            //   (always drawn); the field is tabardColor, the rim trimColor (or weaponColor)
  uint8_t backItem = 0;     // carried on the back, bits: 1 bow (+ quiver), 2 staff
  bool amulet = false;      // a pendant at the collar that glints on the idle frame
  uint32_t trimColor = 0;   // 0 = none; metal trim tint for armour pieces (tier tint): the shield rim
  bool ring = false;        // a ring glint on the weapon hand (idle frame)
  static constexpr int kBands = 7, kCloaks = 3, kShields = 4;

  // ---- M3 peoples and cultures (VISION_PLAN 5.5, 15.6). 0 everywhere = the M2 look, pixel for pixel (art_hash).
  //      Enum values are the culture engine's (rpg/culture/culture.h) + 1, so 0 can mean "legacy".
  uint8_t people = 0;       // 0 human, 1 half-breed (short pointed ears, a touch slimmer), 2 elf (long swept ears,
                            //   slender, a pixel taller, finer features)
  uint8_t cut = 0;          // garment cut: 0 = the outfit decides, else cult::Cut + 1 (tunic, robe, kaftan, wrap,
                            //   coat, kilt, poncho, gown) over the outfit's torso and legs
  uint8_t headwear = 0;     // 0 none, else cult::Headwear (+0: Headwear::None is 0) -- hood, cap, turban, fur hat,
                            //   veil, circlet, conical hat, headscarf; colour = headColor
  uint32_t headColor = 0;   // 0 = topColor
  uint8_t pattern = 0;      // cloth pattern on the garment: cult::Pattern (0 plain); colour = patternColor
  uint32_t patternColor = 0;
  uint8_t facePaint = 0;    // 0 none, 1..4 (stripes, dots, mask band, sun mark)
  uint8_t jewellery = 0;    // 0 none, 1 earrings, 2 torc / necklace, 3 both
  // culture arms: silhouettes over the material band (helmStyle / armorStyle / shieldStyle pick the band; these the
  // form). 0 = the band's own form.
  uint8_t helmForm = 0;     // cult::HelmForm + 1
  uint8_t bodyForm = 0;     // cult::BodyArm + 1
  uint8_t shieldForm = 0;   // cult::ShieldForm + 1 (when set it replaces shieldStyle's shape)
  uint8_t bladeForm = 0;    // cult::Blade + 1 (the sword's silhouette: leaf, falchion, scimitar, khopesh...)
  uint16_t armsOrnament = 0;// cult::ARM_* bits (plumes, fur trim, gilding, horse-tail, studs...)
  uint8_t pauldron = 0, skirt = 0, crest = 0;   // cult::ArmsStyle dials + 1 (0: the band's own)
  uint32_t plumeColor = 0;  // 0 = tabardColor

  // ---- M6 Steel: the rest of the culture arms grammar and the metal a piece is made of (VISION_PLAN 7.2, 15.11).
  //      0 everywhere = the M5 look, pixel for pixel (art_hash). weapon 8 (M4) is the polearm; 9 a walking stick.
  uint8_t polearmForm = 0;  // weapon 8: cult::Polearm + 1 (spear, glaive, halberd); 0 = the M4 spear
  uint8_t bowForm = 0;      // weapon 3 and a bow on the back: cult::BowKind + 1 (self, recurve, composite, crossbow,
                            // longbow); 0 = the classic bow
  uint32_t armourTint = 0;  // 0 = each piece's material band ramp; else the metal's light key (bronze, a culture alloy)
                            // the armour pieces (helm, body, gloves, boots, shield rim) are painted in
  uint32_t armourTint2 = 0; // its dark key (0: derived from armourTint)
  uint8_t sheen = 0;        // cult::Sheen + 1 of that metal (matte, bright, dark, banded, iridescent, glowing, pale,
                            // burnished); 0 plain
  uint8_t glow = 0;         // legendary pieces that glint (bits: 1 weapon, 2 body, 4 helm, 8 shield, 16 cloak);
                            // the colour is glowColor
  uint32_t glowColor = 0;
  uint32_t shieldField = 0;  // (M6 fixer) a maker culture's shield paint: the field (0: tabardColor, the hero's arms)
  uint32_t shieldDevice = 0; // and its device (0: gold)

  // ---- P2a Clear Sight (VISION_PLAN 15.36, rpg/art/art_rig.h): the body base the v2 rig paints. v1 ignores both (no
  //      v1 pixel moves); mixed into key() only when set, so every earlier look keeps its key. Derived, never saved:
  //      looks.cpp sets body from the census `female`, gear_look.cpp from Appearance::female.
  uint8_t body = 0;          // 0 man, 1 woman
  uint8_t age = 0;           // 0 adult, 1 child, 2 youth (P2b: census children aged 10-14), 3 elder (P2b: census age 60+)

  // ---- P2b New People (VISION_PLAN 15.36 "The look fields"; rpg/art/doll/doll.h): what the doll draws a face and a cut
  //      from. Derived, never saved: looks.cpp fills them from its own hash stream at the end of census::dress (after
  //      every existing draw), gear_look.cpp from a hash of the Appearance. v1 ignores them (no v1 pixel moves) and they
  //      enter key() only at rig 2 (rig 1's keys, caches and goldens stay identical until the flip). 0 = A3's.
  uint8_t head = 0;          // head type (doll::headTypeName): man 0 oval 1 square 2 round 3 long 4 craggy 5 heavy, 6 fine
                             //   7 sharp 8 long-elven; woman 0 oval 1 heart 2 round 3 long 4 square 5 full, 6-8 the elven;
                             //   youth the soft four of its sex (oval round long fine); child 0 round 1 oval 2 pointed 3 elfin
  uint8_t nose = 0;          // man 0 button 1 straight 2 long 3 hooked 4 broad; woman 0 button 1 straight 2 upturned 3 aquiline
  uint8_t brow = 0;          // man 0 straight 1 heavy 2 arched 3 thin; woman 0 arched 1 straight 2 thin 3 full
  uint8_t marks = 0;         // bits: 1 freckles, 2 scar, 4 beauty mark, 8 weathered, 16 lined, 32 gaunt (never on kids)
  uint8_t hairVar = 0;       // the style within the hair family (doll::hairStyleOf): Bald (bald, shaved); Short (short,
                             //   cropped, tousled, spiky, receding); Long (long loose, long wavy, shag, bob); Ponytail
                             //   (ponytail, high ponytail, pigtails); Mohawk (mohawk, topknot); Braids (braid, twin braids,
                             //   locs, crown braid); Bun (bun, top bun); Curls (curly, big curls)
  uint8_t beardStyle = 0;    // when beard: 0 full 1 stubble 2 short boxed 3 long 4 braided 5 goatee 6 moustache 7 Van Dyke
                             //   8 mutton chops 9 handlebar
  uint8_t neck = 0;          // neckwear: 0 none 1 scarf 2 kerchief 3 collar (torc and amulet come from jewellery / amulet)
  uint8_t garb = 0;          // a role's work garment over the outfit and cut: 0 none (they decide) 1 work apron (smith,
                             //   shipwright) 2 smock (farmer, labourer) 3 jerkin (hunter); (FOLK-SIM, added) the
                             //   commoners' own wear beside the culture's cut: 4 dress 5 tunic (shirt and trousers) 6 coat
                             //   7 culture tunic 8 kilt 9 gown (the doll draws it; v1 keeps the cut); (P2b production)
                             //   10 rags (the poorest) 11 kaftan 12 wrap 13 poncho (a woman in her people's men's cut)
  uint8_t civil = 0;         // (P2b fix TRADES-DRAWN-ON-V1) 1: a civil trade (smith, shipwright, hunter, scholar,
                             //   lamplighter) that never fights: its v1 leathers are work clothes (the doll draws its
                             //   garb, no armour: doll_resolve.cpp armourOf) and its tool rides at rest (no ACTION:
                             //   art_rig.cpp rig2Covers). Derived, never saved; v1 ignores it; in key() only at rig 2

  // ---- P2c the gear ladder (docs/P2C_CONTRACTS.md 3b; rpg/sim/gear_ladder.h): derived, never saved; v1 ignores them and
  //      key() mixes them only at rig 2 and only when set. Filled by wearGear (each worn piece's tier) and census::dress
  //      (the rank's tier); the looks macros (gear::applyLadderSpec) for tools.
  enum : int { TIER_BODY, TIER_HELM, TIER_SHIELD, TIER_WEAPON, TIER_GLOVES, TIER_BOOTS, TIER_CLOAK, TIER_BACK, TIER_SLOTS };
  uint8_t tier[TIER_SLOTS] = {};  // 0 none, 1-6 (T1 Scrap .. T6 Legendary) per slot: body helm shield weapon (in hand)
                                  // gloves boots cloak back (the bow or staff on the back)
  uint8_t rare = 0;          // bits by slot (1 << TIER_*): the Rare mark (the piece wears the next tier's trim)
  uint8_t metalKind = 0;     // the armour's metal: 0 legacy; 1 leather 2 bronze 3 iron 4 steel 5 gilded 6 jade 7 obsidian
                             // 8 ember 9 starmetal 10 alloy (armourTint) 11 mythic
  uint8_t bladeKind = 0;     // the same codes for the piece in hand (0: metalKind)
  uint8_t mythic = 0;        // bits by slot: a Mythic piece (T6 with the flame)
  uint32_t gemColor = 0;     // 0: the glow colour, else the metal's default glow
  uint8_t motif = 0;         // the filigree's motif by the body's maker people: 0 oak (Heartland, default) 1 clouds (Jade)
                             // 2 laurel (Imperial) 3 knot (Fjordfolk, Highland)
  uint8_t floor = 0;         // (polish i1) bits by slot (1 << TIER_*): an empty slot drawn as the T1 floor (gear::fillLadder:
                             // nothing worn there; the resolver draws T1's padded jerkin / leather boots; rig 2)
  uint8_t kitVar = 0;        // (polish i1) a ranked fighter's own kit (census: rankTiers, from the person's face; no draw):
                             // 0 none, else 1-255; the resolver varies each piece's tier a step, the dials, gloves, the
                             // sergeant's cloak, a bare head now and then (doll_resolve.cpp ladderLook; rig 2)
  uint8_t alloyFam = 0;      // (alloys i1) the alloy's metal family (art::doll::alloyFamily*; 1-based, 0: from armourTint's
                             // hue): gear::fillLadder picks it per maker culture so a people's alloys never share a family
  // (P2c forge styles, docs/P2C_CONTRACTS.md 3c) each worn slot's forge style (TIER_* index), a byte per axis: 0 base, 1
  // shoulder, 2 crest, 3 cut, 4 edge, 5 collar, 6 limb, 7 fittings (rpg/art/doll/doll_forge.h); 0 no style (today's pixels).
  // Derived (gear::fillLadder, rankTiers, the looks macros), never saved; v1 ignores it; key() mixes it at rig 2 only when set
  uint64_t forge[TIER_SLOTS] = {};
  // (P2c forge i2) 1: the universal materials' hero styles (TANNER'S, FOUNDRY, WROUGHT, ARMOURER'S) wear their own helm and
  // shield shapes (doll_resolve styleKitOf): the Heartland's kit, the materials board (mat=); another people's leather,
  // bronze, iron and steel keep its own helm and shield. Derived, never saved; key() mixes it at rig 2 only when set
  uint8_t forgeKit = 0;
  // (P2c forge i2) the worn culture alloy's rank (1 everyday, 2 warriors', 3 masterwork; 0 none): the masterwork's shoulders
  // a size larger, and a culture's alloys each on their own shield shape (doll_resolve). Derived, never saved; key() at rig 2
  uint8_t alloyRank = 0;
  // (P2c forge i3) the worn culture alloy's guise (art::doll::forge::Guise packed: helm, skirt, cloak, shoulder size; 0 none):
  // gear::alloyGuise. Derived, never saved; key() at rig 2 only when set
  uint16_t forgeGuise = 0;
  // (P2c forge round 2, contracts 3c.1) the worn armour's SILHOUETTE (art::doll::forge shape axes packed: helm bits 0-4,
  // skirt 5-7, mass 8-10, layer 11-13, arm 14-15, motif 16-19, bit 31 set; 0 none: round 1's pixels): gear::shapeOf per
  // worn piece (the helm piece the helm family, the body piece the rest). Derived, never saved; key() at rig 2 only when set
  uint32_t forgeShape = 0;
  // (P2c wave-2 fix round, D74) the body piece's maker people (cult::Archetype + 1; 0 none): the people's T2 cowl and capelet
  // hems by people (doll flags `people-<name>`, the Highland's `hem-plaid`). Derived (gear::fillLadder), never saved; key()
  // at rig 2 only when set
  uint8_t makerPeople = 0;
  // (P2c wave-2 fix round, D83 / D84) a Mythic relic helm's own metal style (art::doll::forge base: PARADE, KNAPPED, FURNACE,
  // STARFALL; 0 none): the resolver gives it that metal's helm family under the black, gold and fire. Derived (gear::fillLadder),
  // never saved; key() at rig 2 only when set
  uint8_t relicHelm = 0;
  // (wave-2 fix round 2, D84 restated) a Mythic relic body's own metal style (as relicHelm): the resolver keeps that metal's
  // outline (STARFALL's split skirt, FURNACE's heavy arms) under MYTHIC's finish, so the Mythic measures at its T6's area or more
  uint8_t relicBody = 0;
  // (wave-2 fix round 4, D96) 1: a king (Role::King; census::rankSlots sets it): the doll's `king` flag, his full cape and his
  // realm's deep cloak ramp. Derived, never saved; key() at rig 2 only when set
  uint8_t king = 0;
  int topTier() const { int t = 0; for (uint8_t v : tier) t = v > t ? v : t; return t; }

  // Identity of the painted sheet: equal keys must mean identical sheets. The renderer caches baked sheets by it,
  // so every field that changes pixels must be mixed in here (art_human.cpp).
  uint64_t key() const;
};

enum class Monster : uint8_t {
  Wolf, Boar, Bear, Slime, Spider, Bat, Skeleton, Draugr, Goblin, Troll, Wraith, Mudcrab,
  IceWolf, FrostSpider, Sandworm,   // biome variants
  Dragon,                           // boss: big (~64x48), wings
  // M3c Wildlands wildlife (appended; phase A stand-ins recolour the classic painters, the LIFE lane paints them):
  Scorpion,                         // giant desert scorpion: pincers, a raised sting (dunes, badlands, scrub, stony desert)
  Hyena,                            // a spotted hyena, sloping back (savanna, steppe, scrubland); hunts in packs
  Lurker,                           // a marsh crocodile, low and long, jaws (mangrove, jungle, reed marsh, flooded forest)
  Yeti,                             // a shaggy white ape-giant (glacier, snowfields, tundra)
  Wisp,                             // a floating glow of light, flickering (mushroom forest, silverwood, crystal barrens, bogs at night)
  EmberHound,                       // a black hound with glowing cracks and a smoking mane (ash fields)
  Blightspawn,                      // a hunched, thorn-grown husk (blighted land, dark forest)
  // M6 Steel (VISION_PLAN 7.6: two new families; phase A stand-ins recolour the bat and the troll, the BEASTS lane
  // paints them):
  Harpy,                            // a winged woman-bird of the cliffs and crags: feathered wings, talons, a shriek;
                                    // flies, swoops from above (sea cliffs, mountains, badlands, alpine meadows)
  Golem,                            // a hulking walker of stacked stone (or crystal, or clay) with a glowing core rune:
                                    // slow, armoured, a ground-pounding slam (ruins, mountains, crystal barrens)
  // M10 Beyond the Mist (VISION_PLAN 7.6, 2.12; phase A stand-ins reuse the sandworm, spider and goblin painters, the
  // ART lane paints them; the SEA lane gives them their moves). They SWIM (Actor::swim): water is their ground.
  SeaSerpent,                       // a great sea serpent: coils breaking the swell, a finned crest, a striking head;
                                    // open and storm seas (a world boss of the deep at its greatest)
  Kraken,                           // the kraken: a mantle under the water, tentacles rising round a boat, an ink cloud;
                                    // the storm seas (a world boss)
  Merrow,                           // a merrow: a scaled fish-folk raider with a fin crest and a coral spear; reefs,
                                    // wrecks and sea caves, in bands of 2-4; amphibious (comes ashore on beaches)
  COUNT
};
// ---------------------------------------------------------------- M6 variants (VISION_PLAN 7.6 "Variant looks")
// Overlays drawn on top of the existing painters (a Crystal Troll or an Ashen Wolf costs no new rig), in the sheet's own
// frames (they follow the move cycle, the wind-up, the hurt and the dead frame).
enum : uint16_t {
  MO_HORNS = 1 << 0,      // curling horns on the head
  MO_SPIKES = 1 << 1,     // a ridge of spikes down the back
  MO_CRYSTAL = 1 << 2,    // crystal growths (glints)
  MO_MOSS = 1 << 3,       // moss and lichen on the back
  MO_RIME = 1 << 4,       // frost rime, icicles under the belly
  MO_EMBER = 1 << 5,      // glowing ember cracks
  MO_PLATES = 1 << 6,     // armour plates (bandit-strapped iron, or bony scutes)
  MO_EYES = 1 << 7,       // glowing eyes
  MO_BONEMASK = 1 << 8,   // a skull mask over the face
  MO_ALL = 0x1FF,
  // (M6 fixer round 2) not a drawn overlay but a body size: the GREAT form, painted at native resolution in a bigger
  // cell (lookCellW / lookCellH). Only the lurker has one (the world-boss WYRM); other species ignore the bit.
  MO_GREAT = 1 << 9
};
struct MonsterLook {
  Monster base = Monster::Wolf;
  uint32_t tint = 0;        // body recolour (an "Ashen" or "Crystal" palette; 0: the species' own colours)
  uint16_t overlays = 0;    // MO_* bits
  uint8_t scale = 0;        // draw scale in percent for the renderer (0 = 100; elites ~115, named ~130); the sheet
                            // itself is painted at the species' cell size
  bool classic() const { return tint == 0 && overlays == 0; }
  uint64_t key() const { return (uint64_t)base | (uint64_t)overlays << 8 | (uint64_t)tint << 24; }   // scale is draw-time
};
// (M6 fixer round 2, review: "elite overlays are stamped on families whose anatomy can't carry them") the overlays a
// species' body can carry: a wisp (a ball of light) takes only frost or embers, a slime no horns, plates, spikes or
// mask, a flyer no plates or horns, the crawlers in their own shells no horns or plates. The painter applies it, and
// foes::variantFor stores only what fits (so the look's key and its glow agree).
// (inline: the simulation, which does not link the art, uses it too)
inline uint16_t overlaysFit(Monster m, uint16_t o) {
  switch (m) {
    case Monster::Wisp: return (uint16_t)(o & (MO_EMBER | MO_RIME | MO_GREAT));
    case Monster::Slime: return (uint16_t)(o & ~(MO_HORNS | MO_PLATES | MO_SPIKES | MO_BONEMASK));
    case Monster::Bat: case Monster::Harpy: return (uint16_t)(o & ~(MO_PLATES | MO_HORNS));
    case Monster::Mudcrab: case Monster::Spider: case Monster::FrostSpider: case Monster::Scorpion: case Monster::Sandworm:
      return (uint16_t)(o & ~(MO_HORNS | MO_PLATES));
    default: return o;
  }
}

enum class Building : uint8_t {
  House,       // cottage in the local style
  StoneHouse,  // the same, built in stone
  Inn,         // two storeys, hanging sign with a mug, lanterns
  Smithy,      // open forge bay, anvil sign, big chimney
  Shop,        // general store, awning and sign
  Temple,      // stone, steeple/bell tower (dome in the desert), rose window
  Keep,        // castle keep for the city lord (jarl): crenellations, corner towers, banners
  Tower,       // mage tower, tall round, conical roof
  Farmhouse,   // wide barn
  Hut,         // small shack (village / bandits)
  Palace,      // M1: the king's palace in a capital (multi-storey hall, throne room). Until the towns lane paints it,
               // paintedAs() draws it as a Keep
  Barracks,    // M1: the royal guard's barracks in a palace compound (and city garrisons); painted as a StoneHouse
               // until it has its own look
  // M1 economy (owner 2026-10-05, rpg/world/economy.h): the production buildings that make each settlement's living.
  // Each paints on the frame of a base type (artBase) with its own trade's sign, yard and machinery.
  Windmill,    // a round tapering tower under a cap, four lattice sails (2 storeys: the stone floor, the millstone loft)
  Watermill,   // a stone mill house with an undershot water wheel on its river side (BuildingFacts::variant bit 0: west)
  Granary,     // a raised barn of grain (on the farm frame), the sheaf sign
  Bakery,      // a shop front with the loaf sign, an oven chimney
  Butcher,     // a shop front with the ham sign
  Tanner,      // a workshop house with the hide sign (stretching frames in its yard)
  Fishmonger,  // a shop front with the fish sign
  Smelter,     // a stone furnace house (on the smithy frame), the ingot sign, a tall stack
  Sawmill,     // an open timber shed (on the farm frame), the saw sign
  Weaver,      // a shop front with the spool sign
  // ---- M3b "Builders & Societies" (VISION_PLAN 15.14): the purposes the society generator (rpg/culture/society.h)
  //      asks for. Their LOOK comes from the builder (rpg/build/blueprint.h: culture x purpose x wealth parts), so a
  //      steppe mead hall is a feasting tent and an imperial bathhouse a domed hall. artBase() only names the class of
  //      building whose clearance (riseBudgetTiles) they share.
  Guildhall,   // the craft guilds' hall (towns, cities; with the Exchange the seat of a merchant republic)
  Exchange,    // the merchants' exchange / counting house on the market square (mercantile cities)
  MeadHall,    // a long feasting hall round a fire (fjordfolk, highland; the steppe's feasting tent)
  Bathhouse,   // public baths (imperial, dune, jade, river)
  TeaHouse,    // a tea house (jade, steppe)
  Lodge,       // a warrior lodge / wardens' hall (sworn warrior bands, elven wardens)
  CouncilHall, // the elders' or the high council's hall (clan elders, the elven high council, the moot)
  COUNT
};
// M1 economy: the class a building's clearance follows (bldgRiseTiles, riseBudgetTiles) and, for the production
// buildings, the trade it is built for. M3b: the massing itself is the builder's (bld::design), never artBase's.
inline Building artBase(Building b) {
  switch (b) {
    case Building::Windmill: return Building::Tower;
    case Building::Watermill: return Building::StoneHouse;
    case Building::Granary: case Building::Sawmill: return Building::Farmhouse;
    case Building::Bakery: case Building::Butcher: case Building::Fishmonger: case Building::Weaver: return Building::Shop;
    case Building::Tanner: return Building::House;
    case Building::Smelter: return Building::Smithy;
    // M3b purposes: the clearance class only (the builder gives each its own massing per culture)
    case Building::Guildhall: return Building::StoneHouse;
    case Building::Exchange: return Building::Shop;
    case Building::MeadHall: return Building::Farmhouse;
    case Building::Bathhouse: return Building::StoneHouse;
    case Building::TeaHouse: return Building::Inn;
    case Building::Lodge: return Building::Barracks;
    case Building::CouncilHall: return Building::Temple;
    default: return b;
  }
}
// M1: the type whose painter draws b while a new type has no look of its own yet. The palace and the barracks have
// their own painters now (art_building.cpp), so every type paints as itself.
inline Building paintedAs(Building b) { return b; }
// M0b: what the world generator decided about a building that its exterior must show (VISION_PLAN 15.7: the exterior
// and the interior agree). Bldg carries these (world.h bldgFacts). The defaults reproduce the classic M0 look.
struct BuildingFacts {
  int storeys = 0;      // storeys the walls show (window rows, floor beams, wall height); 0 = defaultStoreys(type).
                        // From WORLDGEN_V7 the interior has exactly this many floors joined by stairs.
  bool hearth = true;   // false: nothing inside burns a fire, so the roof carries NO chimney. true: chimneys as the
                        // style and the type decide (a style without chimneys may still show none: chimney => hearth,
                        // not the reverse)
  // M1 kingdom identity (VISION_PLAN 15.8): the ruling kingdom's banner colours (rgba, banner == 0: none) and emblem.
  // Keeps, palaces, barracks and inns fly it where the painter sees fit.
  uint32_t banner = 0, banner2 = 0;
  int emblem = 0;
  int variant = 0;      // M1 economy: type-specific detail (the watermill: bit 0 = its wheel on the west side)
  // M4 Banners (VISION_PLAN 4.5): war damage drawn over the building (Bldg::charred): 0 whole (the classic pixels and
  // keys), 1 scorched (soot over the windows and eaves, singed roof patches), 2 burned out (roof holed to the rafters,
  // blackened walls, no glass lit at night), 3 being rebuilt (scaffolding and fresh timber over a scorched shell)
  int charred = 0;
};
// The storeys each type showed before M0b (and still shows for worlds before WORLDGEN_V7): inns and keeps 2, mage
// towers 3, everything else 1. Header-only: the simulation (which does not link the art) needs it.
inline int defaultStoreys(Building b) {
  switch (b) {
    case Building::Inn: case Building::Keep: case Building::Barracks: case Building::Palace: case Building::Windmill:
    case Building::Guildhall: case Building::TeaHouse: return 2;
    case Building::Tower: return 3;
    default: return 1;
  }
}
// M3b: how many tiles a building's sprite may rise above its footprint's top row. MIRRORS the generator's clearance
// (world.h bldgRiseTiles / rpg/world/town_rules.h townRiseTiles: a generator constant, never measured from the art):
// the builder plans its massing under it and the painter fits every volume under it (arch_gallery --check holds them
// together on real worlds).
inline int riseBudgetTiles(Building t, int storeys) {
  if (t == Building::Palace) return 7;
  if (t == Building::Barracks) return 4;
  if (t == Building::Windmill) return 6;
  const Building b = artBase(t);
  int r = (b == Building::Tower || b == Building::Temple) ? 5 : ((b == Building::Keep || b == Building::Inn) ? 4 : 3);
  if (storeys >= 2 && (b == Building::House || b == Building::StoneHouse || b == Building::Shop || b == Building::Farmhouse)) r = r > 4 ? r : 4;
  return r;
}

enum class Fx : uint8_t {
  Slash,       // 4 frames 24x24, white-silver arc sweeping clockwise, drawn facing right (renderer flips/rotates by choosing variant)
  SlashDown,   // 4 frames 24x24, arc for facing down
  SlashUp,     // 4 frames 24x24, arc for facing up
  Arrow,       // 1 frame 12x3 pointing right
  ArrowDown,   // 1 frame 3x12 pointing down
  Fireball,    // 4 frames 12x12
  Explosion,   // 6 frames 32x32
  Sparkle,     // 4 frames 8x8 (pickups, level up)
  Blood,       // 4 frames 8x8 red splash
  Dust,        // 4 frames 8x8 footstep/roll dust
  Frost,       // 4 frames 12x12 ice bolt
  Heal,        // 4 frames 16x16 green rising crosses
  // ---- P2a Clear Sight (VISION_PLAN 15.36 item 5, STYLE_P2 6 and 12): one swing effect per weapon family. Phase A
  //      stubs copy Slash (24x24, 4 frames); RIG-ARMS paints them at their own sizes and frame counts (fxW / fxH /
  //      fxFrames always tell the truth) and ANIM times them by the family's pace.
  SwingArc,      // sword and dagger: the Steel Arc crescent (32 x 32, 4 frames)
  HeavyArc,      // greatsword: the Heavy Arc with a trailing ghost blade (48 x 40, 5 frames)
  ChopArc,       // axe: a short heavy arc (28 x 28, 4 frames); Chips on a hit
  SmashRing,     // mace / hammer: the impact ring and dust (32 x 16, 5 frames); sparks on metal
  StabGlint,     // dagger stab: the Glint at the tip (12 x 6, 2 frames)
  ThrustStreak,  // spear: the Thrust Streak along the aim (24 x 5, 3 frames)
  BluntSwish,    // staff / walking stick: the Swish (28 x 28, 3 frames, no glint)
  PunchStar,     // fists: the Punch Star (9 x 9, 2 frames)
  Twang,         // bow release: the Twang at the bow (8 x 8, 2 frames)
  COUNT
};

// Values are runtime only (Actor::posture); append anyway.
enum class Posture : uint8_t {
  None,       // the standing sheet (humanSheet) decides: idle, walk, attack, hurt
  Sit,        // seated on a chair / bench / stool / cushion (the culture's furniture)
  SitEat,     // seated, eating (a spoon or bread to the mouth)
  SitDrink,   // seated, a mug raised
  Eat,        // standing, eating from the hand
  Drink,      // standing, a mug
  Cheer,      // standing, a mug raised high (a toast, a festival)
  Hammer,     // the smith at the anvil: hammer up / down
  Hoe,        // the farmer in the field
  Sweep,      // a broom: shopkeepers, servants, the innkeeper
  Chop,       // an axe: woodcutters, the woodpile
  Stir,       // a ladle in a pot: the cook, the innkeeper's kitchen
  Carry,      // a sack or basket on the shoulder (walk frames: the view picks them while moving)
  Fish,       // a rod held out over the water
  Sleep,      // lying in bed under the blanket (drawn OVER the bed prop: PostureInfo::lying)
  Wave,       // a hand raised in greeting
  Play,       // children at tag: arms up, a hop (run frames)
  Dance,      // festival / tavern dancing (a turning step)
  Lute,       // the bard plucking a lute (the culture's string instrument)
  Drum,       // a hand drum
  Flute,      // a flute / pipe
  Pray,       // hands joined, head bowed (temples, shrines)
  Beg,        // sitting on the ground, a bowl held out (Outfit::Rags)
  Lamp,       // the lamplighter's pole raised to a lamp
  Read,       // a book or a ledger held open (scholars, the exchange's clerks)
  // ---- ART lane, phase B (appended): seated on the floor (cushions round a low table, sleeping mats, the ground):
  //      cross-legged. seatFit(...).sit / seatedPosture() map Sit / SitEat / SitDrink onto these for floor seats.
  SitFloor,      // cross-legged on a cushion / mat / the ground
  SitFloorEat,   // cross-legged, eating
  SitFloorDrink, // cross-legged, a cup raised
  // ---- M7 Home (lead, phase A; ART lane paints it): astride a horse (art::horseSheet / riderSeat): legs either side,
  //      the reins in the hands; phase A stand-in: the seated sheet
  Ride,
  // ---- M8 Fellowship (lead, phase A; ART lane paints them; stand-ins: the floor-seated and the praying sheets)
  Downed,     // a companion at 0 HP, bleeding out: slumped on one knee, a hand pressed to the wound, the head low
  Kneel,      // kneeling on one knee: binding a friend's wounds, mourning at a grave (ART phase B: variant 0 binds the
              //   wounds, both hands low before the knee winding linen; variant 1 mourns, the head bowed, a hand on the
              //   raised knee: humanPostureSheet(look, Kneel, 1) at a grave)
  // ---- P2b New People (lead, phase A; POSES-B paints it on the doll, VIEW-B syncs it to the oars): seated on a rowboat's
  //      thwart: reach, catch and pull. v1 stand-in (rig 1 never asks for it): the seated sheet
  Row,
  COUNT
};

enum Species { kHuman = 0, kSkeletonSp, kDraugrSp, kGoblinSp };

// Layout and range guards (SOLID 2a.2): HumanLook's size and the enum counts are as at 119de74; append-only enums
// may grow these on purpose, never by accident.
static_assert(sizeof(HumanLook) == 232, "HumanLook layout changed");
static_assert((int)Prop::COUNT == 254, "Prop::COUNT changed");
static_assert((int)Monster::COUNT == 28, "Monster::COUNT changed");

}  // namespace art
