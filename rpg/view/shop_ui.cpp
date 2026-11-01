// shop_ui.cpp: the shop screen (View::drawShop) and its deal (View::shopDeal). Its layout (shopLay) is in
// hud_layout.h, shared with input.cpp's taps. Moved from hud.cpp (SOLID 1.4).
#include "rpg/view/hud_layout.h"

using namespace hudlay;

namespace {
int shopPrice(const Game& g, const Item& it, bool buying) {
  if (buying) return it.kind == ItemKind::Arrows ? it.count : it.value;
  // as Game::sell: the base price, then (M9) Speech's sell bonus
  return std::max(1, (int)((float)gear::sellPrice(it) * SkillOps::sellMul(g) + 0.5f));
}
}  // namespace

// The deal on the selected row. Selling something you wear asks first (a second press).
void View::shopDeal(Game& g) {
  if (shopSide_ == 0) g.buy(shopSel_);
  else {
    if (shopSel_ < 0 || shopSel_ >= (int)g.inv.size()) return;
    if (equipped(g, shopSel_) && shopArm_ != shopSel_) { shopArm_ = shopSel_; audio_->play(Sfx::MenuMove); return; }
    g.sell(shopSel_);
  }
  shopArm_ = -1;
  int n2 = shopSide_ == 0 ? (int)g.shop.stock.size() : (int)g.inv.size();
  shopSel_ = std::min(shopSel_, std::max(0, n2 - 1));
}

void View::drawShop(Game& g) {
  Pix& P = *pix_;
  P.rect(0, 0, Pix::W, Pix::H, Color(0, 0, 0, 0.5f));
  if (settingsOpen_) return;
  const UiBox box = uiBox(300);
  P.pushBox(box.x, box.y, box.w, box.h);
  struct Pop { Pix& p; ~Pop() { p.popBox(); } } pop{P};
  const ShopLay L = shopLay(touchUI);
  panel(8, 6, Pix::W - 16, Pix::H - 12);
  P.text(18, 14, fitText(g.shop.title, L.xX - 120), 1, kGold);
  const std::string gs = std::to_string(g.gold);
  P.blit(sprites_.iconTex(*this, art::Icon::Gold, 0), L.xX - 14 - P.textW(gs, 1) - 18, L.xY + L.xH / 2 - 8);
  P.text(L.xX - 14, L.xY + std::floor((L.xH - 7) / 2), gs, 1, kGold, 2);
  // (M6, 7.5 rule 8) the merchant's purse: what they can still pay for what you sell this restock
  {
    const auto pu = g.craft.live.purse.find(g.shop.key);
    if (pu != g.craft.live.purse.end()) {
      const std::string ms = "PURSE " + std::to_string(pu->second.second);
      const float mx = L.xX - 14 - P.textW(gs, 1) - 26;
      P.text(mx, L.xY + std::floor((L.xH - 7) / 2), ms, 1, pu->second.second < 50 ? Color(0.9f, 0.45f, 0.4f) : kDim, 2);
    }
  }
  // (M6 fixer, review: "selling more than the purse holds fails with no visible feedback") what the merchant can pay:
  // a sell price above it is red in the list and the deal button says so (-1: no purse, pays anything)
  int purseLeft = -1;
  { const auto pu = g.craft.live.purse.find(g.shop.key); if (pu != g.craft.live.purse.end()) purseLeft = pu->second.second; }
  button(L.xX, L.xY, L.xW, L.xH, "X", false);
  P.text(L.x0[0] + 2, 28, "BUY", 1, shopSide_ == 0 ? kGold : kDim);
  P.text(L.x0[1] + 2, 28, "SELL", 1, shopSide_ == 1 ? kGold : kDim);
  const float listBottom = 40 + L.rows * L.pitch;
  P.rect(L.x0[1] - 7, 26, 1, listBottom - 24, Color(0.4f, 0.32f, 0.2f));
  if (L.side) P.rect(L.dx - 7, 26, 1, (float)Pix::H - 44, Color(0.4f, 0.32f, 0.2f));
  auto list = [&](float x0, const std::vector<Item>& items, bool buying, int side) {
    int n = (int)items.size();
    int sel = shopSide_ == side ? shopSel_ : -1;
    const int rows = shopRows(L, touchUI, n);
    int scroll = shopScroll(sel, rows);
    if (rows < L.rows) {   // touch: the page buttons and the count in the last row
      const float py = 40 + rows * L.pitch - 2;
      button(x0 - 2, py, 44, L.pitch - 2, "UP", false);
      button(x0 + L.lw - 46, py, 44, L.pitch - 2, "DOWN", false);
      P.text(x0 - 2 + L.lw / 2, py + std::floor((L.pitch - 9) / 2), std::to_string(scroll + 1) + "-" + std::to_string(std::min(n, scroll + rows)) + " OF " + std::to_string(n), 1, kDim, 1);
    }
    for (int r = 0; r < rows && scroll + r < n; r++) {
      int i = scroll + r;
      const Item& it = items[i];
      float y = 40 + r * L.pitch;
      const float isz = touchUI ? 16.0f : 12.0f;
      float ty = y + std::floor((L.pitch - 8) / 2) - 2;
      if (touchUI && i != sel) P.rect(x0 - 2, y - 2, L.lw, L.pitch - 2, Color(0.16f, 0.12f, 0.08f, 0.45f));
      if (i == sel) P.rect(x0 - 2, y - 2, L.lw, L.pitch - (touchUI ? 2 : 0), Color(0.3f, 0.22f, 0.12f, 0.8f));
      loot_.rarityIcon(*this, g, it, x0, y - 2 + std::floor((L.pitch - (touchUI ? 2 : 0) - isz) / 2), isz);   // (M8b) in its rarity's cell
      int price = shopPrice(g, it, buying);
      const std::string ps = std::to_string(price);
      std::string suf;
      if (it.count > 1 || it.kind == ItemKind::Arrows) suf = " x" + std::to_string(it.count);
      if (!buying && equipped(g, i)) suf += " (E)";
      const float nx = x0 + isz + 3, px = x0 + L.lw - 6;
      // (M2 fixer round 2) a long name is shortened by words before it is cut: "WOOL CLOAK OF THE MAGE" becomes
      // "WOOL CLOAK OF MAGE", then "CLOAK OF MAGE" (the cut "WOOL CLOAK OF THE (E)" hid what the item is)
      const float room = px - P.textW(ps, 1) - 8 - nx - P.textW(suf, 1);
      // (M6 fixer r2, review: "the material is the M6 selling point and it disappears") the material and the piece stay;
      // the affix's " OF ..." goes first when the name is still too long (the detail column shows it whole)
      std::string full = it.name;
      if (P.textW(full, 1) > room) {
        const size_t ot = full.find(" OF THE ");
        if (ot != std::string::npos) full = full.substr(0, ot) + " OF " + full.substr(ot + 8);
      }
      if (P.textW(full, 1) > room) {
        const size_t of = full.find(" OF ");
        if (of != std::string::npos && of > 0) full = full.substr(0, of);
      }
      std::string nm = fitText(full, room) + suf;
      loot_.rarityText(*this, nx, ty, nm, it.rarity);
      bool afford = buying ? g.gold >= price : (purseLeft < 0 || price <= purseLeft);
      if (!buying && !afford) {   // (M6 fixer r2) what he can pay: all his purse (amber: less than it is worth)
        P.text(px, ty, std::to_string(std::max(0, purseLeft)), 1, purseLeft > 0 ? Color(0.95f, 0.66f, 0.3f) : Color(0.55f, 0.5f, 0.45f), 2);
        continue;
      }
      P.text(px, ty, ps, 1, afford ? kGold : Color(0.7f, 0.3f, 0.3f), 2);
    }
    if (n == 0) P.text(x0 + 2, 44, buying ? "SOLD OUT" : "NOTHING TO SELL", 1, kDim);
  };
  list(L.x0[0], g.shop.stock, true, 0);
  list(L.x0[1], g.inv, false, 1);
  const std::vector<Item>& src = shopSide_ == 0 ? g.shop.stock : g.inv;
  const bool any = shopSel_ >= 0 && shopSel_ < (int)src.size();
  const bool armed = shopSide_ == 1 && shopArm_ >= 0 && shopArm_ == shopSel_;
  std::string deal;
  bool cantPay = false;
  if (any) {
    const int price = shopPrice(g, src[shopSel_], shopSide_ == 0);
    deal = shopSide_ == 0 ? "BUY FOR " + std::to_string(price) + " GOLD" : armed ? "YOU WEAR IT - TAP AGAIN TO SELL" : "SELL FOR " + std::to_string(price) + " GOLD";
    // (M6 fixer r2, review: "the purse hard-blocks good loot") he buys it for all he has (Game::sell), or has nothing left
    if (shopSide_ == 1 && purseLeft >= 0 && price > purseLeft && !armed) {
      if (purseLeft > 0) deal = "SELL FOR HIS LAST " + std::to_string(purseLeft) + " GOLD";
      else { deal = "HE HAS NO GOLD LEFT"; cantPay = true; }
    }
    if (shopSide_ == 0 && g.gold < price) { deal = "NOT ENOUGH GOLD"; cantPay = true; }
  }
  if (L.side) {
    // the detail column: the item large, its name, rarity, numbers and the deal
    if (any) {
      const Item& it = src[shopSel_];
      // (M8b) the item card with its compare: on the BUY side against what that slot wears; on the SELL side the
      // piece itself (a worn one says so)
      bool isWorn = false;
      const Item* vs = loot_.wornFor(g, it, shopSide_ == 1 ? shopSel_ : -1, isWorn);
      loot_.itemCard(*this, g, it, L.dx + 1, L.detailY + 1, L.dw - 1, L.btnY - (touchUI && armed ? 26.0f : 6.0f), true, vs, isWorn);
      if (touchUI) {
        if (armed) { wrapText(L.dx, L.btnY - 22, L.dw, "YOU WEAR IT - TAP AGAIN TO SELL", kGold); button(L.btnX, L.btnY, L.btnW, L.btnH, "SELL ANYWAY", true); }
        else button(L.btnX, L.btnY, L.btnW, L.btnH, deal, !cantPay);
      } else if (cantPay) {
        P.text(L.dx + L.dw / 2, L.btnY + 4, deal, 1, Color(0.9f, 0.45f, 0.4f), 1);
      } else {
        P.text(L.dx + L.dw / 2, L.btnY + 4, armed ? "ENTER AGAIN TO SELL" : (shopSide_ == 0 ? "ENTER TO BUY" : "ENTER TO SELL"), 1, armed ? kGold : kDim, 1);
      }
    } else P.text(L.dx, L.detailY + 4, "PICK AN ITEM", 1, kDim);
    P.text(L.hintX, L.hintY, touchUI ? "TAP AN ITEM, THEN THE BUTTON" : "ARROWS SELECT  LEFT/RIGHT SWITCH  ESC CLOSE", 1, kDim, 1);
    return;
  }
  // narrow: the detail line and the deal under the lists
  if (any) {
    // (M8b) a piece of gear: its whole card and the compare float over the other list (the one not being browsed), and
    // the strip under the lists names the deal; anything else keeps the one-line detail
    const Item& it = src[shopSel_];
    if (!itemEquippable(it.kind)) {
      const std::string s = statsPlain(g, src[shopSel_]);
      P.text(Pix::W / 2, L.detailY, fitText(s, Pix::W - 36), 1, kText, 1);
    } else if (!touchUI) {   // (touch: the deal button below says it)
      P.text(Pix::W / 2, L.detailY, deal, 1, cantPay ? Color(0.9f, 0.45f, 0.4f) : kGold, 1);
    }
    if (itemEquippable(it.kind)) {
      const float cx = L.x0[1 - shopSide_] - 6, cw = L.lw + 2, cy0r = std::max(25.0f, L.xY + L.xH + 3), cy0 = cy0r > 26.0f ? 37.0f : cy0r, cy1 = 40 + L.rows * L.pitch;
      // (M8b fixer r2, review: "the card for a stocked BUY item appears under the SELL heading") the card covers the
      // other list's heading too: a tab over it says which side is being dealt, pointing at the list browsed
      const bool tab = cy0 >= 36;
      const float ty0 = tab ? 25.0f : cy0;
      P.rect(cx, ty0, cw, cy1 - ty0, Color(0.05f, 0.04f, 0.06f, 1.0f));
      P.frame(cx, ty0, cw, cy1 - ty0, Color(0.55f, 0.45f, 0.28f));
      if (tab) {
        P.text(cx + cw / 2, ty0 + 3, shopSide_ == 0 ? "< BUYING FROM THE STALL" : "SELLING FROM YOUR PACK >", 1, kGold, 1);
        P.rect(cx + 4, cy0 - 1, cw - 8, 1, Color(0.4f, 0.32f, 0.2f));
      }
      P.rect(cx + 1, ty0 + 1, cw - 2, 1, Color(1, 1, 1, 0.05f));
      bool isWorn = false;
      const Item* vs = loot_.wornFor(g, it, shopSide_ == 1 ? shopSel_ : -1, isWorn);
      loot_.itemCard(*this, g, it, cx + 6, cy0 + 6, cw - 12, cy1 - 4, true, vs, isWorn);
    }
  }
  if (touchUI) {
    if (any) button(L.btnX, L.btnY, L.btnW, L.btnH, deal, !cantPay);
    P.text(Pix::W / 2, L.hintY, "TAP AN ITEM, THEN THE BUTTON TO BUY OR SELL", 1, kDim, 1);
  } else {
    P.text(Pix::W / 2, L.hintY, armed ? "YOU WEAR IT - PRESS ENTER AGAIN TO SELL" : "ARROWS SELECT  ENTER BUY/SELL  LEFT/RIGHT SWITCH  ESC CLOSE", 1,
           armed ? kGold : kDim, 1);
  }
}
