// M8b Treasure: the loot internals the --loot-sources audit drives (Game's loot paths are private). SOURCES lane (it
// may add wrappers). The wrappers are inline; chestSource (the rule that names a chest's LootSrc, shared by
// Game::openChest and the audit so both agree) is defined in rpg/sim/game_rpg.cpp.
#pragma once
#include "rpg/sim/game.h"
#include "rpg/sim/gear.h"
#include "rpg/sim/quest_ops.h"

// API (SOLID 5.6). Stable test/script API: the members named below are called from outside rpg/sim and
// rpg/story, by name, so keep their names, signatures and meaning (a change breaks those callers):
//   tests and scripts (rpg/app, tools/tests):
//     chestSource, completeQuest, dropLoot, giveFirstWeapon, openChest, shopSrc, shopStock.
// Every other member is internal to the lane: only rpg/sim and rpg/story call it, and it may change with the lane.
struct LootOps {
  static void dropLoot(Game& g, const Actor& a);   // kill(): the loot a fallen actor drops (game_items.cpp)
  static void openChest(Game& g, int tx, int ty);   // interact(): a chest of the current map opened (game_items.cpp)
  static void completeQuest(Game& g, Quest& q) { QuestOps::completeQuest(g, q); }
  static void giveFirstWeapon(Game& g, Quest& q) { QuestOps::giveFirstWeapon(g, q); }
  static std::vector<Item> shopStock(Game& g, const Actor& a);   // openShop: what a merchant has this restock period (game_shop.cpp)
  // (M8b, 15.27.4) what a chest at (tx, ty) of the current map is as a loot source: a dungeon's hoard (the chest by its
  // boss: LootSrc::Hoard, boost 1), a ruin's cache or a chest at a wayside place / wonder / shrine off the road
  // (LootSrc::Explore, boost 1), else a plain Chest; a dungeon of D >= the player's level + 3 boosts one more. D, the
  // finder's level and the culture are filled; the caller rolls with it.
  static gear::DropSource chestSource(const Game& g, int tx, int ty);
  // (M8b) the shop's source of a seller: Smith / Mage / Priest are a settlement's shops (LootSrc::Shop), the rest
  // (a market merchant, a wayside trader, a traveller) Merchant
  static gear::LootSrc shopSrc(Role r) {
    return r == Role::Smith || r == Role::Mage || r == Role::Priest ? gear::LootSrc::Shop : gear::LootSrc::Merchant;
  }
};
