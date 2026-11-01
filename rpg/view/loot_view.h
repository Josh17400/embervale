// LootView (SOLID 4.2, View part: the M8b Treasure lane), a value member of View (View::loot_). Every rarity-coloured
// surface draws through it: rarityText / rarityFrame / rarityIcon, the item card and its compare (itemCard, wornFor,
// cmpVerdict, attuneWhy, cmpArrow), the pack's EQUIP / USE (packUse), the loot on the ground (beams, lights, the
// chime), the Mythic wearers' aura cells and embers, and the counters the scripts read. Bodies: rpg/view/loot_ui.cpp.
#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>
#include "engine/pix.h"
#include "rpg/art.h"
#include "rpg/sim/game.h"
#include "rpg/view/loot_card.h"
class View;

struct LootView {
  struct LootStats {
    int beams[6] = {};        // pickups drawn last frame with their rarity's ground presence (index: rarity)
    int cards = 0, compares = 0, mythicText = 0, frames = 0;   // item cards, compare panels, Mythic names, rarity frames
    int chimes = 0;           // LootChime plays so far
    int embers = 0;           // Mythic wearers drawn with their flame (embers spawned so far)
  };
  LootStats lootShown_, lootCount_;
  lootp::CardInfo lastCard_;
  float lootChimeT_ = -100;                        // t_ of the last chime (a pickup right after its sighting stays quiet)
  float emberT_ = 0;
  // the aura cell for a Mythic look (nothing for any other) at the figure's top-left (x, y), sc screen px per sprite px
  void mythicAuraAt(View& v, const art::HumanLook& L, int frame, int row, bool flip, float x, float y, float sc);
  void rarityText(View& v, float x, float y, const std::string& s, Rarity r, float alpha = 1, int align = 0, bool shadow = false);
  void rarityFrame(View& v, float x, float y, float w, float h, Rarity r, bool hot = false, bool small = false);
  void rarityIcon(View& v, const Game& g, const Item& it, float x, float y, float size);   // the icon in its rarity cell
  void cmpArrow(View& v, float x, float y, int dir);        // 1 up (green), -1 down (red), 0 even (a grey dash)
  // the item card at (x, y) w wide down to maxY; cmp: compare against `vs` (nullptr: the slot is empty); worn: the
  // piece is the one worn (a WORN tag instead of the compare). Returns the y under it.
  float itemCard(View& v, const Game& g, const Item& it, float x, float y, float w, float maxY, bool cmp, const Item* vs, bool worn, bool bigIcon = true);
  const Item* wornFor(const Game& g, const Item& it, int selfIndex, bool& isWorn) const;   // the slot's worn piece
  int cmpVerdict(const Game& g, const Item& it, const Item* vs) const;                     // 1 better, -1 worse, 0 even
  int attuneWhy(const Game& g, const Item& it, const Item* vs, std::string* name = nullptr) const;   // 0 / 1 lv10 / 2 full
  void packUse(View& v, Game& g, int i);   // the pack's EQUIP / USE (a great piece that cannot be worn: the reason blinks)
  float lootBlockFlash_ = 0;   // (M8b fixer r2) a tap on a disabled EQUIP: the card's reason line blinks a moment
  void drawLootGround(View& v, Game& g, const Pickup& k, int idx, Vec2 cam, float bob);   // the sorted pass (case 4)
  void lootLights(View& v, Game& g, const std::function<void(Vec2, float, Color, float)>& light);
  void lootUpdate(View& v, Game& g, float dt);              // chimes for Epic+ coming into view, the Mythic wearers' embers
  void lootNotice(View& v, Game& g, const Event& e, Toast& t);   // an item pickup notice: its rarity, the chime
};
