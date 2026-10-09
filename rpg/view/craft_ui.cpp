// M6 Steel: the forge screen (Mode::Forge, Game::bench; rpg/sim/craft.h). NUMBERS lane.
// Phone-first, in the same modal box as the shop: a tab strip by material (MATERIALS, LEATHER, BRONZE, IRON, STEEL,
// the culture's alloys, OTHER), the recipe rows with the result's icon (View::itemTex), the picked recipe's detail
// beside them (the result large, each need as held / needed, the smithing skill and secrets it asks, the item level and
// numbers it will have) and a MAKE button. Taps and keys: a tap picks a tab or a row, MAKE makes it, X closes; the
// arrows move (left / right: tabs), Enter makes, Escape closes. Touch rows are finger-sized (24 px).
#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>
#include "engine/audio.h"
#include "rpg/culture/culture.h"
#include "rpg/sim/craft.h"
#include "rpg/sim/gear.h"
#include "rpg/view/view.h"
#include "rpg/world/source.h"

namespace {
const Color kGold(0.98f, 0.82f, 0.42f), kText(0.93f, 0.9f, 0.82f), kDim(0.62f, 0.58f, 0.52f);
const Color kOk(0.52f, 0.92f, 0.50f), kBad(0.95f, 0.45f, 0.40f);

struct FTab { std::string name; std::vector<int> rows; };
// the bench's recipes grouped by what they are made of (the order of Game::bench is kept inside each tab)
std::vector<FTab> forgeTabs(const Game& g) {
  std::vector<FTab> t;
  auto add = [&](const std::string& n, int i) {
    for (FTab& x : t) if (x.name == n) { x.rows.push_back(i); return; }
    t.push_back({n, {i}});
  };
  const cult::Culture* bc = craft::benchCulture(g);
  for (int i = 0; i < (int)g.bench.recipes.size(); i++) {
    const craft::Recipe& r = g.bench.recipes[(size_t)i];
    if (!r.gear()) { add("MATERIALS", i); continue; }
    switch (r.mat) {
      case Mat::Leather: add("LEATHER", i); break;
      case Mat::Bronze: add("BRONZE", i); break;
      case Mat::Iron: add("IRON", i); break;
      case Mat::Steel: add("STEEL", i); break;
      case Mat::Alloy: {
        const cult::Culture* c = r.culture && g.world.src ? &g.world.src->culture(r.culture) : bc;
        add(c && r.alloy >= 1 && r.alloy <= c->arms.alloys.size() ? c->arms.alloys[(size_t)r.alloy - 1].name : std::string("ALLOY"), i);
        break;
      }
      default: add("OTHER", i); break;
    }
  }
  // a fixed order: materials first, then the metals by tier, the alloys, the rest
  auto rank = [](const std::string& n) {
    static const char* o[] = {"MATERIALS", "LEATHER", "BRONZE", "IRON", "STEEL"};
    for (int k = 0; k < 5; k++) if (n == o[k]) return k;
    return n == "OTHER" ? 9 : 6;
  };
  std::stable_sort(t.begin(), t.end(), [&](const FTab& a, const FTab& b) { return rank(a.name) < rank(b.name); });
  return t;
}

struct FLay {
  float pitch; int rows;
  float lx, lw, dx, dw;          // list column, detail column
  float tabY, tabH, listY;
  float btnX, btnY, btnW, btnH;
  float xX, xY, xW, xH;          // close button
};
FLay forgeLay(bool touch, int nTabs) {
  FLay L;
  const float W = (float)Pix::W, H = (float)Pix::H;
  L.pitch = touch ? 24.0f : 14.0f;
  L.xW = touch ? 28.0f : 20.0f; L.xH = touch ? 22.0f : 12.0f;
  L.xX = W - 16 - L.xW; L.xY = touch ? 9.0f : 10.0f;
  L.tabY = touch ? 34.0f : 26.0f; L.tabH = touch ? 22.0f : 13.0f;
  (void)nTabs;
  L.listY = L.tabY + L.tabH + 6;
  L.lx = 18;
  L.dw = std::clamp(std::floor(W * 0.38f), 160.0f, 260.0f);
  L.dx = W - 18 - L.dw;
  L.lw = L.dx - 12 - L.lx;
  L.btnW = L.dw; L.btnH = touch ? 26.0f : 18.0f; L.btnX = L.dx;
  L.btnY = H - 18 - L.btnH;
  L.rows = std::max(3, (int)((H - 22 - L.listY) / L.pitch));
  return L;
}
// a list longer than its rows gives its last row to the page buttons on touch
int listRows(const FLay& L, bool touch, int n) { return touch && n > L.rows ? L.rows - 1 : L.rows; }

// what the recipe makes, as an Item to draw and describe (gear at the bench's item level, capped by the material)
Item previewOf(const Game& g, const craft::Recipe& r) {
  const cult::Culture* c = r.culture && g.world.src ? &g.world.src->culture(r.culture) : craft::benchCulture(g);
  if (!r.gear()) return craft::makeStuff(r.out, r.outCount, r.culture, r.alloy, c);
  Rng rr(0xF0E6u);
  const int cap = gear::bandHi(gear::matBandCap(r.mat, c, r.alloy));
  Item it = gear::makeGearC(rr, r.kind, r.sub, std::clamp(craft::benchDanger(g), 1, cap), Rarity::Common, r.culture ? c : nullptr, r.mat, r.alloy);
  if (r.culture) it.culture = r.culture;
  return it;
}
int heldOf(const Game& g, const craft::Need& n) {
  int c = 0;
  for (const Item& it : g.inv) {
    if (it.kind != ItemKind::Material || it.sub != (uint8_t)n.stuff) continue;
    if ((n.stuff == craft::Stuff::AlloyIngot || n.stuff == craft::Stuff::Reagent) && (it.culture != n.culture || it.alloy != n.alloy)) continue;
    c += it.count;
  }
  return c;
}
std::string needName(const Game& g, const craft::Need& n) {
  if ((n.stuff == craft::Stuff::AlloyIngot || n.stuff == craft::Stuff::Reagent) && n.culture && g.world.src) {
    const cult::Culture& c = g.world.src->culture(n.culture);
    return craft::makeStuff(n.stuff, 1, n.culture, n.alloy, &c).name;
  }
  return craft::stuffInfo(n.stuff).name;
}
std::string fit(const std::string& s, float w) {
  const int n = std::max(0, (int)((w + 1) / 6));
  return (int)s.size() <= n ? s : s.substr(0, (size_t)std::max(0, n - 1)) + ".";
}
}  // namespace

void View::drawForge(Game& g) {
  Pix& P = *pix_;
  P.rect(0, 0, Pix::W, Pix::H, Color(0, 0, 0, 0.5f));
  if (settingsOpen_) return;
  const UiBox box = uiBox(300);
  P.pushBox(box.x, box.y, box.w, box.h);
  struct Pop { Pix& p; ~Pop() { p.popBox(); } } pop{P};
  const std::vector<FTab> tabs = forgeTabs(g);
  const FLay L = forgeLay(touchUI, (int)tabs.size());
  panel(8, 6, Pix::W - 16, Pix::H - 12);
  // the title, the smith's skill, the close X
  const std::string sk = "SMITHING " + std::to_string(g.craft.skillLevel());
  P.text(18, 14, fit(g.bench.title.empty() ? std::string("THE FORGE") : g.bench.title, L.xX - 40 - P.textW(sk, 1) - 18), 1, kGold);
  P.text(L.xX - 10, L.xY + std::floor((L.xH - 7) / 2), sk, 1, kDim, 2);
  button(L.xX, L.xY, L.xW, L.xH, "X", false);
  if (tabs.empty()) {
    P.text((float)Pix::W / 2, L.listY + 20, "NOTHING TO MAKE HERE", 1, kDim, 1);
    return;
  }
  // a recipe asked for (a script's forgepick, the one just made): its tab and row
  if (g.bench.focus >= 0) {
    for (int k = 0; k < (int)tabs.size(); k++)
      for (int i = 0; i < (int)tabs[(size_t)k].rows.size(); i++)
        if (tabs[(size_t)k].rows[(size_t)i] == g.bench.focus) { forgeTab_ = k; forgeSel_ = i; }
    g.bench.focus = -1;
  }
  forgeTab_ = std::clamp(forgeTab_, 0, (int)tabs.size() - 1);
  const FTab& T = tabs[(size_t)forgeTab_];
  forgeSel_ = std::clamp(forgeSel_, 0, std::max(0, (int)T.rows.size() - 1));
  // the tab strip (the whole box width; long names shortened to fit)
  {
    const float tw = std::floor(((float)Pix::W - 36) / (float)tabs.size());
    for (int k = 0; k < (int)tabs.size(); k++) {
      const float x = 18 + k * tw;
      button(x, L.tabY, tw - 3, L.tabH, fit(tabs[(size_t)k].name, tw - 9), k == forgeTab_);
      bool any = false;
      for (int ri : tabs[(size_t)k].rows) any |= craft::canMake(g.bench.recipes[(size_t)ri], g.inv, g.craft, nullptr);
      if (any) P.rect(x + tw - 9, L.tabY + 2, 3, 3, kOk);   // something here can be made now
    }
  }
  // the rows
  const int n = (int)T.rows.size();
  const int rows = listRows(L, touchUI, n);
  if (forgeSel_ < forgeScroll_) forgeScroll_ = forgeSel_;
  if (forgeSel_ >= forgeScroll_ + rows) forgeScroll_ = forgeSel_ - rows + 1;
  forgeScroll_ = std::clamp(forgeScroll_, 0, std::max(0, n - rows));
  const cult::Culture* bc = craft::benchCulture(g);
  for (int r = 0; r < rows && forgeScroll_ + r < n; r++) {
    const int i = forgeScroll_ + r;
    const craft::Recipe& R = g.bench.recipes[(size_t)T.rows[(size_t)i]];
    const float y = L.listY + r * L.pitch;
    if (i == forgeSel_) P.rect(L.lx - 2, y - 2, L.lw, L.pitch - (touchUI ? 2 : 0), Color(0.3f, 0.22f, 0.12f, 0.8f));
    else if (touchUI) P.rect(L.lx - 2, y - 2, L.lw, L.pitch - 2, Color(0.16f, 0.12f, 0.08f, 0.45f));
    const float isz = touchUI ? 16.0f : 12.0f;
    const Item pv = previewOf(g, R);
    P.blitEx(itemTex(g, pv), 0, 0, 16, 16, L.lx, y - 2 + std::floor((L.pitch - (touchUI ? 2 : 0) - isz) / 2), isz, isz);
    const bool ok = craft::canMake(R, g.inv, g.craft, nullptr);
    const float ty = y + std::floor((L.pitch - 8) / 2) - 2;
    std::string nm = craft::recipeName(R, R.culture && g.world.src ? &g.world.src->culture(R.culture) : bc);
    const std::string tag = ok ? "READY" : (g.craft.skillLevel() < R.skill ? "SKILL " + std::to_string(R.skill) : "");
    const float nx = L.lx + isz + 4;
    P.text(nx, ty, fit(nm, L.lx + L.lw - 8 - nx - P.textW(tag, 1) - 6), 1, ok ? kText : kDim);
    if (!tag.empty()) P.text(L.lx + L.lw - 8, ty, tag, 1, ok ? kOk : kBad, 2);
  }
  if (rows < L.rows) {   // touch: page buttons in the last row
    const float py = L.listY + rows * L.pitch - 2;
    button(L.lx - 2, py, 50, L.pitch - 2, "UP", false);
    button(L.lx + L.lw - 52, py, 50, L.pitch - 2, "DOWN", false);
    P.text(L.lx - 2 + L.lw / 2, py + std::floor((L.pitch - 9) / 2),
           std::to_string(forgeScroll_ + 1) + "-" + std::to_string(std::min(n, forgeScroll_ + rows)) + " OF " + std::to_string(n), 1, kDim, 1);
  }
  P.rect(L.dx - 7, L.listY - 2, 1, (float)Pix::H - L.listY - 12, Color(0.4f, 0.32f, 0.2f));
  // the detail column
  if (n == 0) return;
  const craft::Recipe& R = g.bench.recipes[(size_t)T.rows[(size_t)forgeSel_]];
  const cult::Culture* rc = R.culture && g.world.src ? &g.world.src->culture(R.culture) : bc;
  const Item pv = previewOf(g, R);
  P.blitEx(itemTex(g, pv), 0, 0, 16, 16, L.dx, L.listY, 32, 32);
  float y = L.listY + 2;
  const std::string title = craft::recipeName(R, rc);
  P.text(L.dx + 38, y, fit(title, L.dw - 40), 1, kText);
  y += 10;
  if (R.gear()) {
    const int pw = (int)std::lround(gear::usePower(pv, g.plLevel));
    const char* what = R.kind == ItemKind::Weapon ? "DAMAGE " : R.kind == ItemKind::Bow ? "ARROWS " : "ARMOR ";
    P.text(L.dx + 38, y, "ITEM LEVEL " + std::to_string((int)pv.ilvl), 1, kDim); y += 10;
    P.text(L.dx + 38, y, what + std::to_string(pw), 1, kText); y += 10;
  } else {
    // (M6 fixer r4) a smithy's bench carries the smelter's recipes too (fillBench): name the furnace here, never
    // send the player to a smelter for something this forge makes
    const bool here = R.at == g.bench.at || (g.bench.at == art::Building::Smithy && R.at == art::Building::Smelter);
    const std::string where = here ? (R.at == g.bench.at ? "AT THIS " + std::string(craft::stationName(R.at)) : std::string("IN THIS FORGE'S FURNACE"))
                                   : std::string("AT THE ") + craft::stationName(R.at);
    P.text(L.dx + 38, y, fit(where, L.dw - 40), 1, kDim); y += 10;
  }
  y = std::max(y + 4, L.listY + 38);
  P.text(L.dx, y, "NEEDS", 1, kGold);
  y += 11;
  for (const craft::Need& nd : R.in) {
    const int have = heldOf(g, nd);
    const bool enough = have >= nd.count;
    const Item ic = craft::makeStuff(nd.stuff, 1);
    P.blitEx(itemTex(g, ic), 0, 0, 16, 16, L.dx, y - 2, 12, 12);
    const std::string cnt = std::to_string(have) + "/" + std::to_string((int)nd.count);
    P.text(L.dx + 15, y, fit(needName(g, nd), L.dw - 20 - P.textW(cnt, 1) - 6), 1, kText);
    P.text(L.dx + L.dw - 2, y, cnt, 1, enough ? kOk : kBad, 2);
    y += touchUI ? 13.0f : 11.0f;
  }
  if (R.skill) {
    const bool sOk = g.craft.skillLevel() >= R.skill;
    P.text(L.dx, y, "SMITHING " + std::to_string((int)R.skill), 1, sOk ? kDim : kBad);
    y += 11;
  }
  std::string why;
  const bool ok = craft::canMake(R, g.inv, g.craft, &why);
  if (!ok && (why.find("PATTERN") != std::string::npos || why.find("ALLOY") != std::string::npos)) {
    P.text(L.dx, y, fit(why, L.dw), 1, kBad);
    y += 11;
  }
  // the last thing made (or a hint) over the button
  const std::string status = !g.bench.lastMade.empty() ? g.bench.lastMade : std::string();
  if (!status.empty() && y + 10 < L.btnY - 4) P.text(L.dx, L.btnY - 12, fit(status, L.dw), 1, kOk);
  button(L.btnX, L.btnY, L.btnW, L.btnH, ok ? (touchUI ? "MAKE" : "MAKE (ENTER)") : "MAKE", ok);
  if (!ok) P.rect(L.btnX, L.btnY, L.btnW, L.btnH, Color(0.05f, 0.04f, 0.06f, 0.45f));   // greyed: not yet
  if (!touchUI) P.text(L.lx + L.lw / 2, (float)Pix::H - 17, "ARROWS PICK  LEFT/RIGHT TABS  ESC CLOSE", 1, kDim, 1);
}

namespace {
void forgeMakeSel(Game& g, int recipe, Audio* audio) {
  std::string why;
  if (craft::benchMake(g, recipe, &why)) { if (audio) audio->play(Sfx::Buy); }
  else {
    g.bench.lastMade.clear();
    g.notice = why; g.noticeT = 2.5f;
    if (audio) audio->play(Sfx::MenuBack);
  }
}
}  // namespace

void View::forgeKey(Game& g, int key) {
  const std::vector<FTab> tabs = forgeTabs(g);
  if (key == SDLK_ESCAPE || key == SDLK_TAB) { g.mode = Mode::Play; audio_->play(Sfx::MenuBack); return; }
  if (tabs.empty()) return;
  forgeTab_ = std::clamp(forgeTab_, 0, (int)tabs.size() - 1);
  const int n = (int)tabs[(size_t)forgeTab_].rows.size();
  if (key == SDLK_UP || key == SDLK_W) { forgeSel_ = std::max(0, forgeSel_ - 1); audio_->play(Sfx::MenuMove); }
  if (key == SDLK_DOWN || key == SDLK_S) { forgeSel_ = std::min(std::max(0, n - 1), forgeSel_ + 1); audio_->play(Sfx::MenuMove); }
  if (key == SDLK_LEFT || key == SDLK_A) { forgeTab_ = (forgeTab_ + (int)tabs.size() - 1) % (int)tabs.size(); forgeSel_ = 0; forgeScroll_ = 0; audio_->play(Sfx::MenuMove); }
  if (key == SDLK_RIGHT || key == SDLK_D) { forgeTab_ = (forgeTab_ + 1) % (int)tabs.size(); forgeSel_ = 0; forgeScroll_ = 0; audio_->play(Sfx::MenuMove); }
  if ((key == SDLK_RETURN || key == SDLK_SPACE || key == SDLK_E) && n > 0) {
    forgeSel_ = std::clamp(forgeSel_, 0, n - 1);
    forgeMakeSel(g, tabs[(size_t)forgeTab_].rows[(size_t)forgeSel_], audio_);
  }
}

void View::forgeTap(Game& g, Vec2 p0) {
  Pix& P = *pix_;
  const UiBox box = uiBox(300);
  const Vec2 p = inBox(box, p0);
  P.pushBox(box.x, box.y, box.w, box.h);
  struct Pop { Pix& p; ~Pop() { p.popBox(); } } pop{P};
  auto inR = [&](float x, float y, float w, float h) { return p.x >= x && p.y >= y && p.x < x + w && p.y < y + h; };
  const std::vector<FTab> tabs = forgeTabs(g);
  const FLay L = forgeLay(touchUI, (int)tabs.size());
  if (inR(L.xX - 4, 0, L.xW + 14, L.xY + L.xH + 6) || p.y < -4 || p.y > Pix::H + 4) { g.mode = Mode::Play; audio_->play(Sfx::MenuBack); return; }
  if (tabs.empty()) return;
  forgeTab_ = std::clamp(forgeTab_, 0, (int)tabs.size() - 1);
  // tabs
  const float tw = std::floor(((float)Pix::W - 36) / (float)tabs.size());
  for (int k = 0; k < (int)tabs.size(); k++)
    if (inR(18 + k * tw, L.tabY - 3, tw - 3, L.tabH + 5)) {
      if (k != forgeTab_) { forgeTab_ = k; forgeSel_ = 0; forgeScroll_ = 0; audio_->play(Sfx::MenuMove); }
      return;
    }
  const FTab& T = tabs[(size_t)forgeTab_];
  const int n = (int)T.rows.size();
  // MAKE
  if (inR(L.btnX - 4, L.btnY - 3, L.btnW + 8, L.btnH + 6)) {
    if (n > 0) forgeMakeSel(g, T.rows[(size_t)std::clamp(forgeSel_, 0, n - 1)], audio_);
    return;
  }
  const int rows = listRows(L, touchUI, n);
  if (rows < L.rows) {   // page buttons
    const float py = L.listY + rows * L.pitch - 2;
    if (inR(L.lx - 2, py - 2, 54, L.pitch + 2)) { forgeSel_ = std::max(0, forgeSel_ - rows); forgeScroll_ = std::max(0, forgeScroll_ - rows); audio_->play(Sfx::MenuMove); return; }
    if (inR(L.lx + L.lw - 54, py - 2, 54, L.pitch + 2)) { forgeSel_ = std::min(n - 1, forgeSel_ + rows); forgeScroll_ = std::min(std::max(0, n - rows), forgeScroll_ + rows); audio_->play(Sfx::MenuMove); return; }
  }
  for (int r = 0; r < rows && forgeScroll_ + r < n; r++) {
    const float y = L.listY + r * L.pitch - 2;
    if (inR(L.lx - 4, y, L.lw + 4, L.pitch)) {
      const int i = forgeScroll_ + r;
      if (i == forgeSel_ && !touchUI) forgeMakeSel(g, T.rows[(size_t)i], audio_);   // a mouse's second click makes it
      forgeSel_ = i;
      audio_->play(Sfx::MenuMove);
      return;
    }
  }
}
