// The dialogue's option routing (SOLID 4.1): which lane's choose handles an option in which DLG_* range (game_types.h).
// One row per range, in the order Game::dialogueChoose has always asked them (story first, the lord last); an option
// is offered to the row whose [lo, hi) holds its action. The ranges are sorted and never overlap (checked below), so
// at most one row can take an option. A new lane's range: a DLG_* pair in game_types.h, then one row here.
#include "rpg/sim/game_internal.h"
#include "rpg/sim/craft_hooks.h"
#include "rpg/sim/foe_hooks.h"
#include "rpg/sim/life_hooks.h"
#include "rpg/sim/lord_ops.h"
#include "rpg/sim/magic_ops.h"
#include "rpg/sim/party_voice.h"
#include "rpg/sim/sea_ops.h"
#include "rpg/sim/service_ops.h"
#include "rpg/sim/skill_ops.h"
#include "rpg/sim/story_ops.h"
#include "rpg/sim/war_ops.h"

namespace {

struct DlgRow {
  int lo, hi;                                    // the option actions [lo, hi) this row takes
  bool (*choose)(Game& g, const DlgOpt& o);      // the lane's choose: true handled
};

constexpr DlgRow kDlgRoutes[] = {
    {DLG_STORY, DLG_WAR, &StoryOps::choose},             // M4 STORY (rpg/story/story_game.cpp)
    {DLG_WAR, DLG_END, &WarOps::choose},                 // M4 WARDS (war_game.cpp)
    {DLG_LIFE, DLG_LIFE_END, &LifeHooks::choose},        // M5 TOWNSFOLK (life_game.cpp)
    {DLG_FOES, DLG_CRAFT, &FoeHooks::choose},            // M6 FOES (foes_game.cpp)
    {DLG_CRAFT, DLG_M6_END, &CraftHooks::choose},        // M6 NUMBERS / FORGE (craft_game.cpp)
    {DLG_HOME, DLG_HOME_END, &HomeOps::choose},          // M7 HOMESTEAD (home_talk.cpp)
    {DLG_PARTY, DLG_PARTY_END, &PartyTalkOps::choose},   // M8 VOICE (party_talk.cpp)
    {DLG_SKILLS, DLG_SKILLS_END, &SkillOps::choose},     // M9 SKILLS (skills_game.cpp)
    {DLG_MAGIC, DLG_MAGIC_END, &MagicOps::choose},       // M9 MAGIC (magic_game.cpp)
    {DLG_SEA, DLG_SEA_END, &SeaOps::choose},             // M10 SEA (sea_game.cpp)
    {DLG_SERVICE, DLG_SERVICE_END, &ServiceOps::choose}, // M12 SERVICE (service_game.cpp)
    {DLG_LORD, DLG_LORD_END, &LordOps::choose},          // M12 LORD (lord_game.cpp)
};

constexpr bool routesSorted() {
  for (const DlgRow& r : kDlgRoutes)
    if (r.lo >= r.hi) return false;
  for (size_t i = 1; i < sizeof(kDlgRoutes) / sizeof(kDlgRoutes[0]); i++)
    if (kDlgRoutes[i - 1].hi > kDlgRoutes[i].lo) return false;
  return true;
}
static_assert(routesSorted(), "the DLG_* ranges must be sorted and must not overlap (game_types.h)");

}  // namespace

bool gsim::dlgRoute(Game& g, const DlgOpt& o) {
  for (const DlgRow& r : kDlgRoutes)
    if (o.action >= r.lo && o.action < r.hi && r.choose(g, o)) return true;
  return false;
}
