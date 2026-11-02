// rpg/view/scripts/economy_p4.cpp - the P4 ECONOMY lane's script checks (test only; docs/FIX_BATCH_P4.md 9.1).
//   expect shopprice <words...> <op> <n>   the open shop's first row whose name holds every word (any case) costs
//                                          <op> n gold (op: < <= == >= >), as the BUY list shows it
//   p4row shop|pack <words...>             the cursor on the first BUY row / pack row holding every word, as the arrow
//                                          keys put it (a `key Return` then buys it, or wears it from the pack)
//   expect wornname <slot> <words...>      the piece worn in a slot (armor, helmet, weapon, ...) is named with every word
//   p4sell <words...>                      in the open shop, every unworn pack row named with the words sold, one at a time
//                                          (Game::sell, the deal the SELL list's Return makes), as a newcomer sells his hunt
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include "rpg/sim/game.h"
#include "rpg/view/script_api.h"
#include "rpg/view/view.h"

namespace {

std::string upper(std::string s) {
  for (char& ch : s) ch = (char)std::toupper((unsigned char)ch);
  return s;
}
bool holdsAll(const std::string& name, const std::vector<std::string>& words) {
  const std::string n = upper(name);
  for (const std::string& w : words) if (n.find(upper(w)) == std::string::npos) return false;
  return true;
}
std::vector<std::string> wordsOf(const ScriptCtx& c, size_t from, size_t to) {
  std::vector<std::string> w;
  for (size_t i = from; i < to && i < c.a.size(); i++) w.push_back(c.a[i]);
  return w;
}
int rowOf(const std::vector<Item>& items, const std::vector<std::string>& words) {
  for (size_t i = 0; i < items.size(); i++) if (holdsAll(items[i].name, words)) return (int)i;
  return -1;
}
std::string join(const std::vector<std::string>& w) {
  std::string r;
  for (const std::string& s : w) r += (r.empty() ? "" : " ") + s;
  return r;
}

bool expShopPrice(ScriptCtx& c) {
  const Game& g = c.game;
  if (c.a.size() < 5) { c.fail("expect shopprice <words...> <op> <n>"); return true; }
  if (g.mode != Mode::Shop) { c.fail("shopprice: no shop open"); return true; }
  const std::string op = c.a[c.a.size() - 2];
  const int want = std::atoi(c.a.back().c_str());
  const std::vector<std::string> words = wordsOf(c, 2, c.a.size() - 2);
  const int i = rowOf(g.shop.stock, words);
  if (i < 0) { c.fail("shopprice: no row named " + join(words) + " on the shelf"); return true; }
  const Item& it = g.shop.stock[(size_t)i];
  const int price = it.kind == ItemKind::Arrows ? it.count : it.value;
  const bool ok = op == "<" ? price < want : op == "<=" ? price <= want : op == "==" || op == "=" ? price == want : op == ">=" ? price >= want
                : op == ">" ? price > want : false;
  if (!ok) c.fail("shopprice: " + it.name + " costs " + std::to_string(price) + " (wanted " + op + " " + std::to_string(want) + ")");
  else std::printf("script: shopprice %s %d\n", it.name.c_str(), price);
  return true;
}
EMB_SCRIPT_CMD("expect:shopprice", "expect shopprice <words...> <op> <n>: the open shop's row named with the words costs <op> n gold (P4 ECONOMY, test only)",
               expShopPrice);

bool cmdP4Row(ScriptCtx& c) {
  Game& g = c.game;
  const std::string side = c.arg(1);
  const std::vector<std::string> words = wordsOf(c, 2, c.a.size());
  if ((side != "shop" && side != "pack") || words.empty()) { c.fail("p4row shop|pack <words...>"); return true; }
  const int i = rowOf(side == "shop" ? g.shop.stock : g.inv, words);
  if (i < 0) { c.fail("p4row: no " + side + " row named " + join(words)); return true; }
  std::string err;
  const std::vector<std::string> a = side == "shop" ? std::vector<std::string>{"lootsel", "shop", "buy", std::to_string(i)}
                                                    : std::vector<std::string>{"lootsel", "menu", std::to_string(i)};
  if (!c.view.lootSelect(g, a, err)) c.fail("p4row: " + err);
  return true;
}
EMB_SCRIPT_CMD("p4row", "p4row shop|pack <words...>: the cursor on the BUY row / pack row named with the words (P4 ECONOMY, test only)", cmdP4Row);

bool expWornName(ScriptCtx& c) {
  Game& g = c.game;
  static const struct { const char* n; ItemKind k; } kSlots[] = {
      {"weapon", ItemKind::Weapon}, {"bow", ItemKind::Bow}, {"armor", ItemKind::Armor}, {"helmet", ItemKind::Helmet},
      {"shield", ItemKind::Shield}, {"gloves", ItemKind::Gloves}, {"boots", ItemKind::Boots}, {"cloak", ItemKind::Cloak}};
  int* e = nullptr;
  for (const auto& s : kSlots) if (c.arg(2) == s.n) e = g.equipSlot(s.k);
  const std::vector<std::string> words = wordsOf(c, 3, c.a.size());
  if (!e || words.empty()) { c.fail("expect wornname <slot> <words...>"); return true; }
  if (*e < 0 || *e >= (int)g.inv.size()) { c.fail("wornname: nothing worn as " + c.arg(2)); return true; }
  if (!holdsAll(g.inv[(size_t)*e].name, words)) c.fail("wornname: " + c.arg(2) + " is " + g.inv[(size_t)*e].name + ", not " + join(words));
  return true;
}
EMB_SCRIPT_CMD("expect:wornname", "expect wornname <slot> <words...>: the piece worn there is named with the words (P4 ECONOMY, test only)", expWornName);

bool cmdP4Sell(ScriptCtx& c) {
  Game& g = c.game;
  const std::vector<std::string> words = wordsOf(c, 1, c.a.size());
  if (words.empty()) { c.fail("p4sell <words...>"); return true; }
  if (g.mode != Mode::Shop) { c.fail("p4sell: no shop open"); return true; }
  const int gold0 = g.gold;
  int sold = 0;
  for (int guard = 0; guard < 200; guard++) {
    int i = -1;
    for (size_t k = 0; k < g.inv.size() && i < 0; k++) {
      bool worn = false;
      for (int e : {g.eqWeapon, g.eqBow, g.eqStaff, g.eqArmor, g.eqHelmet, g.eqShield, g.eqRing, g.eqAmulet, g.eqGloves, g.eqBoots, g.eqCloak}) worn |= e == (int)k;
      if (!worn && holdsAll(g.inv[k].name, words)) i = (int)k;
    }
    if (i < 0 || !g.sell(i)) break;
    sold++;
  }
  std::printf("script: p4sell %s: %d sold for %d gold\n", join(words).c_str(), sold, g.gold - gold0);
  return true;
}
EMB_SCRIPT_CMD("p4sell", "p4sell <words...>: every unworn pack row named with the words sold to the open shop (P4 ECONOMY, test only)", cmdP4Sell);

}  // namespace
