// quest_traits.h: one traits row per QType (SOLID 5.3), in the enum's order: the word a test script names it by, its
// journal look ("icon"), the kind of place its map marker points at, the kind of journal line the HUD gives it, and the
// way a script's questgo reaches its objective. What a quest's people SAY per type (the journal step, the accept line,
// the reminder, the turn-in thanks) is the matching row of kQuestLines in rpg/sim/quests.cpp.
// Read by: rpg/sim/quests.cpp (markers), rpg/view/hud.cpp (journal and tracker), rpg/view/scripts/travel_quests.cpp
// (offerquest, questgo). Who may include it: anything that may include rpg/sim/game_types.h.
// A new QType appends a row here (and one in kQuestLines); the static_assert catches a missing one.
#pragma once
#include <cstddef>
#include <cstdint>
#include "rpg/sim/game_types.h"

enum class QIcon : uint8_t { Plain, Main };   // the journal's title colour: Main reads gold
// where questTargetM2 puts the marker
enum class QTarget : uint8_t {
  None,          // no M2 marker (the classic rules in Game::questTarget, or none)
  Site,          // the quest's site, else its place
  PosOrSite,     // its place, else its site
  PosOnly,       // its place, else none
  DeliverDoor,   // the recipient's door, else the site
  SupplySite,    // the site the goods come from (none once they are carried)
  HuntGround,    // a quarry in sight, else the den or the hunting ground
};
// the HUD's extra journal line
enum class QJournal : uint8_t {
  Plain,
  Clock,    // a story stage's deadline on the tracker's second line, coloured by how close it is
  Count,    // the hunt: its count kept on the tracker's first line, PROGRESS have/need on the page
  Shards,   // the main quest: EMBER SHARDS have/3 on the page while at stage 1
};
// how the questgo script command reaches the objective
enum class QGo : uint8_t {
  None,       // not for this kind of quest
  Building,   // into the recipient's building
  Dungeon,    // into the cave or ruin
  Camp,       // three tiles south of the site's rect, facing north
  Fields,     // a tile south of the place, facing north
  Ground,     // a dozen tiles south of the den or ground, facing north
};

struct QuestTraits {
  const char* name;    // the script word (offerquest <name>); nullptr: scripts never offer it
  const char* alias;   // a second script word, or nullptr
  QIcon icon;
  QTarget target;
  QJournal journal;
  QGo go;
};

// clang-format off
inline constexpr QuestTraits kQuestTraits[] = {
  /* Main        */ {nullptr,    nullptr,       QIcon::Main,  QTarget::None,        QJournal::Shards, QGo::None},
  /* Clear       */ {"clear",    nullptr,       QIcon::Plain, QTarget::None,        QJournal::Plain,  QGo::Camp},
  /* Hunt        */ {"hunt",     nullptr,       QIcon::Plain, QTarget::HuntGround,  QJournal::Count,  QGo::Ground},
  /* Retrieve    */ {nullptr,    nullptr,       QIcon::Plain, QTarget::None,        QJournal::Plain,  QGo::None},
  /* Bounty      */ {"bounty",   nullptr,       QIcon::Plain, QTarget::None,        QJournal::Plain,  QGo::Camp},
  /* Deliver     */ {"deliver",  nullptr,       QIcon::Plain, QTarget::DeliverDoor, QJournal::Plain,  QGo::Building},
  /* Heirloom    */ {"heirloom", nullptr,       QIcon::Plain, QTarget::Site,        QJournal::Plain,  QGo::Dungeon},
  /* Missing     */ {"missing",  nullptr,       QIcon::Plain, QTarget::Site,        QJournal::Plain,  QGo::Dungeon},
  /* NamedBandit */ {"named",    "namedbandit", QIcon::Plain, QTarget::Site,        QJournal::Plain,  QGo::Camp},
  /* Protect     */ {"protect",  nullptr,       QIcon::Plain, QTarget::PosOrSite,   QJournal::Plain,  QGo::Fields},
  /* Story       */ {nullptr,    nullptr,       QIcon::Plain, QTarget::PosOnly,     QJournal::Clock,  QGo::None},
  /* War         */ {nullptr,    nullptr,       QIcon::Plain, QTarget::PosOrSite,   QJournal::Plain,  QGo::None},
  /* Supply      */ {nullptr,    nullptr,       QIcon::Plain, QTarget::SupplySite,  QJournal::Plain,  QGo::None},
  /* Sea         */ {nullptr,    nullptr,       QIcon::Plain, QTarget::PosOnly,     QJournal::Plain,  QGo::None},
  /* Campaign    */ {nullptr,    nullptr,       QIcon::Plain, QTarget::PosOrSite,   QJournal::Plain,  QGo::None},
  /* Envoy       */ {nullptr,    nullptr,       QIcon::Plain, QTarget::PosOrSite,   QJournal::Plain,  QGo::None},
  /* Fief        */ {nullptr,    nullptr,       QIcon::Plain, QTarget::PosOrSite,   QJournal::Plain,  QGo::None},
  /* Errand      */ {nullptr,    nullptr,       QIcon::Plain, QTarget::PosOnly,     QJournal::Plain,  QGo::None},
};
// clang-format on
static_assert(sizeof(kQuestTraits) / sizeof(kQuestTraits[0]) == (size_t)QType::COUNT, "kQuestTraits: one row per QType, in its order");

// the row of a quest type (a value out of range, as from a damaged save, reads as a plain quest with no marker)
inline const QuestTraits& questTraits(QType t) {
  static constexpr QuestTraits kNone = {nullptr, nullptr, QIcon::Plain, QTarget::None, QJournal::Plain, QGo::None};
  return (size_t)t < (size_t)QType::COUNT ? kQuestTraits[(size_t)t] : kNone;
}
