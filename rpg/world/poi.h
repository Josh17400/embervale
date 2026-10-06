// M2 Wayfinder: the wayside places between settlements (VISION_PLAN M2, PLAN.md task 4) and the world's wonders.
// Shared contract (frozen after M2 phase A): the WORLD lane places and stamps them (SitePlan::kind), the SIM lane gives
// them life (who lives there, what interacting does), the VIEW lane paints their props and shows them on the map.
//
// A Vignette is a small site (SiteType::Vignette, Site::kind = VignetteKind): a footprint of a few tiles with props,
// sometimes people or monsters (spawns streamed with its chunk), found by walking near it like any site. A Wonder
// (SiteType::Wonder, Site::kind = WonderKind) is a rare, large landmark that is worth a detour and shows on the map from
// afar once rumoured.
//
// What a vignette's records carry (WORLD lane writes them, SIM lane reads them):
//   - its SitePlan: type Vignette, kind, a small footprint (w, h about 5..11), the heart (ex, ey) = the tile the place
//     centres on (the menhir ring's heart stone, the camp's fire, the grave, the toll post at the bridge head), level,
//     a name ("HUNTER'S CAMP" or a personal one: "OLD BRAN'S CAMP", "THE WHISPERING STONES", "TROLLBRIDGE AT <RIVER>");
//   - props stamped into the chunk (the art::Prop vignette props, with Filler over wildFootprint), a Chest where there
//     is loot (Caravan, Watchtower; looted by Game::lootKey like any overworld chest);
//   - spawns (SpawnPlan, site = the vignette): slot 0 is the keeper: Role::Hunter / Fisher / Herbalist / Traveller
//     (npc = true), or the toll bridge's troll (mon = Troll, boss = true); slots 1.. are the others (a caravan's
//     ambushers and the watchtower's archers: bandit = true). The SIM lane decides behaviour from (kind, slot).
// A wonder: type Wonder, kind, a footprint around its prop (wildFootprint plus a clearing), heart = the prop's tile, a
// name of its own ("THE ELDER OAK OF <REGION>"), no spawns. Its LandmarkPlan (same name) lets the map label it.
#pragma once
#include <cstdint>
#include "rpg/sim/world.h"

namespace ew {

enum class VignetteKind : uint8_t {
  Caravan,         // an overturned caravan: a wrecked wagon (art::Prop::CaravanWreck), spilled crates, a loot chest, and
                   // a bandit ambush that springs when the player comes close
  HunterCamp,      // a hunter's camp: a tent, a fire, hide racks; a friendly Role::Hunter who sells pelts and gives a hunt
  StandingStones,  // a ring of menhirs (art::Prop::StandingStone): touching the heart stone grants a blessing once a day
  Watchtower,      // a ruined watchtower (art::Prop::WatchtowerRuin): bandit archers in the nest, a chest at its foot
  FishingHut,      // a fishing shack by a lake or river (art::Prop::FishingShack), a jetty, drying racks; Role::Fisher
  TollBridge,      // a road bridge over a river with a troll under it who demands a toll (pay, or fight)
  HerbGarden,      // a walled herb garden (art::Prop::HerbBed rows); Role::Herbalist sells herbs and potions
  LoneGrave,       // a lone grave (art::Prop::GraveCairn) whose inscription is a rumour hook (a ruin, a lost heirloom)
  Wayrest,         // a roadside fire and bedroll (art::Prop::Bedroll) where a Role::Traveller trades news (rumours)
  COUNT
};

enum class WonderKind : uint8_t {
  ElderTree,       // a colossal ancient tree (art::Prop::ElderTree) in a glade
  Colossus,        // the weathered statue of a forgotten king (art::Prop::Colossus), half sunk in the land
  Starfall,        // a fallen star's crater ringed with glowing shards (art::Prop::StarShard)
  DragonBones,     // the bones of an ancient dragon (art::Prop::DragonBones) bleaching in the open
  COUNT
};

inline const char* vignetteName(VignetteKind k) {
  static const char* n[] = {"OVERTURNED CARAVAN", "HUNTER'S CAMP", "STANDING STONES", "RUINED WATCHTOWER", "FISHING HUT",
                            "TOLL BRIDGE", "HERB GARDEN", "LONE GRAVE", "WAYFARERS' REST"};
  static_assert(sizeof(n) / sizeof(n[0]) == (size_t)VignetteKind::COUNT, "a name for every vignette");
  return (int)k < (int)VignetteKind::COUNT ? n[(int)k] : "WAYSIDE";
}
inline const char* wonderName(WonderKind k) {
  static const char* n[] = {"ELDER TREE", "COLOSSUS", "STARFALL", "DRAGON BONES"};
  static_assert(sizeof(n) / sizeof(n[0]) == (size_t)WonderKind::COUNT, "a name for every wonder");
  return (int)k < (int)WonderKind::COUNT ? n[(int)k] : "WONDER";
}
// what the HUD and the map call a site's kind: "HUNTER'S CAMP" for a vignette, "WONDER" for a wonder, else the type
inline const char* poiKindName(SiteType t, uint8_t kind) {
  if (t == SiteType::Vignette) return vignetteName((VignetteKind)kind);
  if (t == SiteType::Wonder) return wonderName((WonderKind)kind);
  return siteTypeName(t);
}
// one number per distinct kind of point of interest (the repetition audit and the start guarantee count these):
// a site type, or 100 + vignette kind, or 200 + wonder kind
inline int poiKindKey(SiteType t, uint8_t kind) {
  if (t == SiteType::Vignette) return 100 + kind;
  if (t == SiteType::Wonder) return 200 + kind;
  return (int)t;
}

}  // namespace ew
