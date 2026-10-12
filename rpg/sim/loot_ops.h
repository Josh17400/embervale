// M8b Treasure: the loot internals the --loot-sources audit drives (Game's loot paths are private). SOURCES lane (it
// may add wrappers). The wrappers are inline; chestSource (the rule that names a chest's LootSrc, shared by
// Game::openChest and the audit so both agree) is defined in rpg/sim/game_rpg.cpp.
#pragma once
#include "rpg/sim/game.h"
#include "rpg/sim/gear.h"

struct LootOps {
  static void dropLoot(Game& g, const Actor& a) { g.dropLoot(a); }
  static void openChest(Game& g, int tx, int ty) { g.openChest(tx, ty); }
  static void completeQuest(Game& g, Quest& q) { g.completeQuest(q); }
  static void giveFirstWeapon(Game& g, Quest& q) { g.giveFirstWeapon(q); }
  static std::vector<Item> shopStock(Game& g, const Actor& a) { return g.shopStock(a); }
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
