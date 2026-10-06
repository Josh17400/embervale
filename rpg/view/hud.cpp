// HUD, menus, dialogue, shop, title/death screens, and all input (keyboard, mouse, touch).
#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <string>
#include <vector>
#include "rpg/view/view.h"
#include "rpg/world/poi.h"

void heroStage(Pix& P, const Tex& light, float cx, float footY, float size);   // creator.cpp

namespace {
Color col(uint32_t c, float a = 1) { return Color((c & 255) / 255.0f, ((c >> 8) & 255) / 255.0f, ((c >> 16) & 255) / 255.0f, a); }
// a kingdom's banner colour lifted toward parchment so a dark field still reads as text over the world
// (M2 fixer) the kingdom's colour lifted well toward a light parchment, so it reads over grass, roofs and snow
Color mixKing(Color c) { return Color(c.r * 0.40f + 0.58f, c.g * 0.40f + 0.55f, c.b * 0.40f + 0.48f, c.a); }
const Color kGold(0.98f, 0.82f, 0.42f), kText(0.93f, 0.9f, 0.82f), kDim(0.62f, 0.58f, 0.52f), kPanel(0.07f, 0.06f, 0.08f);

// on-screen touch buttons
enum Btn { B_ATTACK, B_BOW, B_SPELL, B_ROLL, B_POTION, B_MENU, B_COUNT };
struct BtnDef { float x, y, r; };
// laid out on the 480 x 270 design screen; btnPos() anchors them to the safe area's bottom-right corner (the menu
// button: its top-right), so on a wide phone they stay under the right thumb and clear of the notch
const BtnDef kBtn[B_COUNT] = {{424, 222, 23}, {374, 240, 14}, {380, 196, 14}, {424, 172, 14}, {464, 176, 11}, {466, 12, 10}};
BtnDef btnPos(int b) {
  BtnDef d = kBtn[b];
  d.x = (float)(Pix::W - Pix::SR) - (480 - d.x);
  d.y = b == B_MENU ? (float)Pix::ST + d.y : (float)(Pix::H - Pix::SB) - (270 - d.y);
  return d;
}
// the resting place of the movement stick (bottom-left of the safe area)
Vec2 stickRest() { return Vec2((float)Pix::SL + 60, (float)(Pix::H - Pix::SB) - 55); }

// menuTab_ ids are stable (scripts and --menu use them); EQUIP (M0) is id 5 but shows second
const char* kTabs[] = {"ITEMS", "QUESTS", "MAP", "HERO", "SYSTEM", "EQUIP"};
constexpr int NTABS = 6;
const int kTabOrder[NTABS] = {0, 5, 1, 2, 3, 4};
int tabStep(int tab, int dir) {
  int at = 0;
  for (int i = 0; i < NTABS; i++) if (kTabOrder[i] == tab) at = i;
  return kTabOrder[(at + dir + NTABS) % NTABS];
}

// The HUD's step line under the tracked quest title: the journal's "what to do next" (Game::questStatus), cut to
// the quest column's width.
std::string trackedStep(const Game& g, const Quest& q) {
  std::string s = g.questStatus(q);
  if (s.empty() && q.state == QState::Complete) s = "RETURN TO " + q.giverName;
  if (s.size() <= 30) return s;
  // drop a trailing clause (" IN <TOWN>", " (NORTH-EAST)", " NEAR <TOWN>") rather than cutting a word in half
  size_t cut = std::string::npos;
  for (const char* sep : {" IN ", " NEAR ", " AT ", " (", ": "}) {
    size_t p = s.find(sep);
    while (p != std::string::npos) {
      if (p >= 8 && p <= 30 && (cut == std::string::npos || p > cut)) cut = p;
      p = s.find(sep, p + 1);
    }
  }
  if (cut != std::string::npos) {
    // (M1) a place clause ("IN BRIGHTFELL") is where to go: it moves to a second line instead of being dropped
    std::string rest = s.substr(cut + 1);
    if (rest.size() > 30) rest = rest.substr(0, 30);
    if (rest.rfind("IN ", 0) == 0 || rest.rfind("NEAR ", 0) == 0 || rest.rfind("AT ", 0) == 0) return s.substr(0, cut) + "\n" + rest;
    return s.substr(0, cut);
  }
  s = s.substr(0, 30);
  size_t sp = s.find_last_of(' ');
  return sp != std::string::npos && sp > 18 ? s.substr(0, sp) : s;
}

// Menu layout, shared by drawMenu and tap(): on touch the ITEMS and QUESTS rows are finger-sized (24 px) with page
// buttons under the list, and the action buttons 24 px tall; keyboard/mouse keeps the dense 12 px list.
// (M2) every measure follows the box, which is the whole safe area: the rows follow its height, the columns its width.
struct MenuLay { float pitch; int rows; float btnY, btnH, trackY, trackH, sysH; };
MenuLay menuLay(bool touch) {
  if (touch) return {24.0f, std::max(5, (Pix::H - 96) / 24), (float)Pix::H - 46, 24.0f, (float)Pix::H - 46, 24.0f, 26.0f};
  return {12.0f, std::max(12, (Pix::H - 64) / 12), (float)Pix::H - 40, 16.0f, (float)Pix::H - 40, 16.0f, 20.0f};
}
// The list | detail columns of a menu tab: the list widens with the box by `share` of the extra width, the detail pane
// takes the rest (at 480: ITEMS 18+236 | 272..460, QUESTS 18+200 | 236..456)
struct Cols { float lx, lw, div, dx, dw; };
Cols menuCols(float baseW, float share) {
  Cols c;
  c.lx = 18;
  c.lw = std::floor(baseW + std::max(0, Pix::W - 480) * share);
  c.div = c.lx + c.lw + 8;
  c.dx = c.div + 10;
  c.dw = (float)Pix::W - 20 - c.dx;
  return c;
}
Cols itemCols() { return menuCols(236, 0.5f); }
Cols questCols() { return menuCols(200, 0.4f); }
// the page buttons under a touch list (UP at the left end, DOWN at the right end; the "1-8 OF 15" count between)
struct PageBtns { float upX, downX, y, w, h; };
PageBtns pageBtns(const Cols& c, const MenuLay& L) { return {c.lx, c.lx + c.lw - 52, L.btnY, 52, L.btnH}; }
// the HERO tab: a card of numbers on the left (lx, lw), a lit stage on the right with the hero and the level-up choice
struct HeroLay { float lx, lw, sx, sy, sw, sh, pcx, footY, scale, bx, by, bw, bh; };
HeroLay heroLay(bool touch, bool perk) {
  HeroLay H;
  const float W = (float)Pix::W, Hh = (float)Pix::H;
  H.lx = 26;
  H.lw = std::floor(std::clamp(W * 0.42f, 190.0f, 280.0f));
  H.sx = H.lx + H.lw + 22; H.sy = 44; H.sw = W - 20 - H.sx; H.sh = Hh - 20 - H.sy;
  H.pcx = std::floor(H.sx + H.sw / 2);
  H.bh = touch ? 24.0f : 18.0f;
  H.bw = std::min(H.sw - 24, 140.0f);
  H.bx = std::floor(H.pcx - H.bw / 2);
  H.by = H.sy + H.sh - 8 - 3 * H.bh - 8;
  // the hero as large as the stage allows (4x on a 270-tall screen, 3x with the level-up buttons under him)
  const float room = perk ? H.by - (H.sy + 18) - 10 : H.sh - 40;
  H.scale = std::clamp(std::floor(room / art::HUMAN_H), 2.0f, 5.0f);
  H.footY = perk ? H.by - 12 : H.sy + H.sh - 34;
  return H;
}
// the SYSTEM tab's first button: the block (four buttons, the seed, the key help) centred in the panel's height
float sysTop(float top, float sh, bool touch) {
  const float block = 4 * (sh + 6) + 16 + (touch ? 0.0f : 49.0f);
  return top + std::max(22.0f, std::floor((Pix::H - 8 - top - block) / 2));
}
// a string cut to fit w pixels of 5x7 text (a word boundary when one is near)
std::string fitText(const std::string& s, float w) {
  const int n = std::max(1, (int)((w + 1) / 6));
  if ((int)s.size() <= n) return s;
  std::string t = s.substr(0, (size_t)n);
  size_t sp = t.find_last_of(' ');
  if (sp != std::string::npos && (int)sp >= n - 6) t.resize(sp);
  return t;
}
// the menu's tab strip and close button: the tabs run from x 20 to the close button at the strip's right end
struct TabLay { float x0, tw, y, h, xX, xY, xW, xH; };
TabLay tabLay(bool touch) {
  TabLay t;
  t.y = touch ? 8.0f : 11.0f;
  t.h = touch ? 24.0f : 15.0f;
  t.xW = touch ? 28.0f : 20.0f;
  t.xH = t.h;
  t.xX = (float)Pix::W - 18 - t.xW;
  t.xY = t.y;
  t.x0 = 20;
  t.tw = (t.xX - 4 - t.x0) / 6.0f;
  return t;
}

// (M2) the tracked objective's tile in window coordinates: Game::questTarget, else the quest's own global tile (a far
// objective in the endless world: it may lie well outside the loaded window; the arrow and the minimap pin still point)
bool questTile(const Game& g, const Quest& q, int& tx, int& ty) {
  if (g.questTarget(q.id, tx, ty)) return true;
  if (q.state == QState::Done || !q.hasPos) return false;
  const int64_t lx = (int64_t)q.tgx - g.world.ox, ly = (int64_t)q.tgy - g.world.oy;
  tx = (int)std::clamp<int64_t>(lx, -1000000, 1000000);
  ty = (int)std::clamp<int64_t>(ly, -1000000, 1000000);
  return true;
}
// a distance in tiles for the edge arrow: "87", "1.4K", "23K"
std::string distText(float d) {
  const int n = (int)d;
  if (n < 1000) return std::to_string(n);
  char b[16];
  if (n < 10000) std::snprintf(b, sizeof b, "%d.%dK", n / 1000, (n % 1000) / 100);
  else std::snprintf(b, sizeof b, "%dK", n / 1000);
  return b;
}
std::vector<std::string> wrap(const std::string& s, int maxChars) {
  std::vector<std::string> lines;
  std::string line, word;
  auto flush = [&]() {
    if (word.empty()) return;
    if (!line.empty() && (int)(line.size() + 1 + word.size()) > maxChars) { lines.push_back(line); line.clear(); }
    if (!line.empty()) line += ' ';
    line += word;
    word.clear();
  };
  for (char c : s) {
    if (c == ' ') flush();
    else if (c == '\n') { flush(); lines.push_back(line); line.clear(); }
    else word += c;
  }
  flush();
  if (!line.empty()) lines.push_back(line);
  return lines;
}

std::string itemStats(const Game& g, const Item& it) {
  std::string s;
  switch (it.kind) {
    case ItemKind::Weapon: s = "DAMAGE " + std::to_string(it.power); break;
    case ItemKind::Bow: s = "ARROW DAMAGE " + std::to_string(it.power); break;
    case ItemKind::Staff: s = "SPELL POWER +" + std::to_string(it.power * 2) + "%"; break;
    case ItemKind::Armor: case ItemKind::Helmet: case ItemKind::Shield: case ItemKind::Gloves: case ItemKind::Boots: case ItemKind::Cloak:
      s = "ARMOR " + std::to_string(it.power); break;
    case ItemKind::Potion: s = "RESTORES " + std::to_string(it.power); break;
    case ItemKind::Food: s = "HEALS " + std::to_string(it.power); break;
    case ItemKind::Arrows: s = "AMMUNITION"; break;
    case ItemKind::Quest: s = "QUEST ITEM"; break;
    default: s = "VALUE " + std::to_string(it.value); break;
  }
  (void)g;
  if (it.ench != Ench::None) {
    switch (it.ench) {
      case Ench::Fire: s += "\nBURNS FOR " + std::to_string(it.enchPow) + " FIRE"; break;
      case Ench::Frost: s += "\nFREEZES FOR " + std::to_string(it.enchPow) + " FROST"; break;
      case Ench::Drain: s += "\nDRAINS " + std::to_string(it.enchPow / 2) + " HEALTH"; break;
      case Ench::Health: s += "\n+" + std::to_string(it.enchPow) + " MAX HEALTH"; break;
      case Ench::Magicka: s += "\n+" + std::to_string(it.enchPow) + " MAX MAGICKA"; break;
      case Ench::Stamina: s += "\n+" + std::to_string(it.enchPow) + " MAX STAMINA"; break;
      case Ench::Fortify: s += "\n+" + std::to_string(it.enchPow / 2) + " ARMOR"; break;
      default: break;
    }
  }
  if (it.kind != ItemKind::Quest) s += "\nVALUE " + std::to_string(it.value);
  return s;
}
// a usable prop (chest, shrine, bed...) the attack button may "use" instead: only when no enemy is close,
// so mashing attack beside a shrine in a fight still swings the sword
int usablePropAt(const Game& g, int& tx, int& ty) {
  int pr = g.interactProp(tx, ty);
  if (!pr) return 0;
  // (M0b fix round 2: another guest's bed, or the innkeeper's, stays usable: the button reads LOOK and a tap explains
  // whose it is and that the innkeeper lets rooms, instead of swinging a weapon in a room full of people)
  for (size_t i = 1; i < g.actors.size(); i++) {
    const Actor& a = g.actors[i];
    if (a.hostile && a.st != AState::Dead && len2(a.p - g.pl().p) < 70 * 70) return 0;
  }
  return pr;
}
const char* useVerb(int pr);
// the verb for the prop at (tx, ty): a bed that is not yours to sleep in is only looked at
const char* useVerbAt(const Game& g, int pr, int tx, int ty) {
  if (pr == (int)art::Prop::Bed + 1 && !g.bedIsYours(tx, ty)) return "LOOK";
  return useVerb(pr);
}
const char* useVerb(int pr) {
  switch ((art::Prop)(pr - 1)) {
    case art::Prop::Chest: return "OPEN";
    case art::Prop::Shrine: case art::Prop::Altar: return "PRAY";
    case art::Prop::BerryBush: return "PICK";
    case art::Prop::Signpost: return "READ";
    case art::Prop::Bed: case art::Prop::Hammock: case art::Prop::SleepingMat: return "SLEEP";
    default: return "USE";
  }
}
bool equipped(const Game& g, int i) {
  return i == g.eqWeapon || i == g.eqBow || i == g.eqStaff || i == g.eqArmor || i == g.eqHelmet || i == g.eqShield || i == g.eqRing || i == g.eqAmulet ||
         i == g.eqGloves || i == g.eqBoots || i == g.eqCloak;
}
int equippedOf(const Game& g, ItemKind k) {
  switch (k) {
    case ItemKind::Weapon: return g.eqWeapon; case ItemKind::Bow: return g.eqBow; case ItemKind::Staff: return g.eqStaff;
    case ItemKind::Armor: return g.eqArmor; case ItemKind::Helmet: return g.eqHelmet; case ItemKind::Shield: return g.eqShield;
    case ItemKind::Ring: return g.eqRing; case ItemKind::Amulet: return g.eqAmulet;
    case ItemKind::Gloves: return g.eqGloves; case ItemKind::Boots: return g.eqBoots; case ItemKind::Cloak: return g.eqCloak; default: return -1;
  }
}
}  // namespace

// The dialogue panel's layout, shared by drawDialogue, tap() and menuKey (M2: from the box's real width; the panel
// grows with the wrapped line, so a long speech is never cut, and an option too long for one row wraps onto two).
// The speaker's portrait stands at the panel's left when the box is wide enough (a phone's safe area).
namespace {
struct DlgLay {
  float x, w, y, h;
  bool portrait;
  float tx, tw;              // the speech's column
  int per;                   // characters per speech line
  std::vector<std::string> lines;
  int total = 0;             // characters to type
  std::vector<float> optTop, optH;
  std::vector<std::vector<std::string>> optLines;
};
DlgLay dlgLay(const Game& g, float rowH) {
  DlgLay L;
  const Dialogue& d = g.dlg;
  L.x = 20; L.w = (float)Pix::W - 40;
  bool human = false;
  for (const Actor& a : g.actors) if (a.id == d.actor && a.npc && a.human) human = true;
  L.portrait = human && Pix::W >= 540;
  L.tx = L.x + (L.portrait ? 64.0f : 10.0f);
  L.tw = L.x + L.w - 10 - L.tx;
  L.per = std::max(10, (int)(L.tw / 6));
  L.lines = wrap(d.text, L.per);
  for (auto& l : L.lines) L.total += (int)l.size();
  const int optPer = std::max(10, (int)((L.w - 40) / 6));
  float oh = 0;
  for (const DlgOpt& o : d.opts) {
    L.optLines.push_back(wrap(o.label, optPer));
    if (L.optLines.back().empty()) L.optLines.back().push_back("");
    const float h = rowH + (float)(L.optLines.back().size() - 1) * 9;
    L.optH.push_back(h);
    oh += h;
  }
  // the speech: at least two lines' room (three with a portrait), and never more than the box allows
  float textH = std::max((float)L.lines.size(), L.portrait ? 4.0f : 2.0f) * 9;
  const float maxText = (float)Pix::H - 16 - 34 - oh;
  textH = std::min(textH, std::max(18.0f, maxText));
  L.h = 22 + textH + 10 + oh + 6;
  L.y = (float)Pix::H - L.h - 8;
  float top = L.y + 22 + textH + 10;
  for (float h : L.optH) { L.optTop.push_back(top); top += h; }
  return L;
}
}  // namespace

// ------------------------------------------------------------------ drawing helpers
void View::panel(float x, float y, float w, float h, float alpha) {
  Pix& P = *pix_;
  P.rect(x + 2, y + 2, w, h, Color(0, 0, 0, 0.35f * alpha));
  P.rect(x, y, w, h, Color(kPanel.r, kPanel.g, kPanel.b, alpha));
  P.frame(x, y, w, h, Color(0.55f, 0.45f, 0.28f, alpha));
  P.frame(x + 1, y + 1, w - 2, h - 2, Color(0.22f, 0.18f, 0.12f, alpha));
  // corner studs
  for (float cx : {x + 1, x + w - 3}) for (float cy : {y + 1, y + h - 3}) P.rect(cx, cy, 2, 2, Color(0.9f, 0.75f, 0.4f, alpha));
}
void View::button(float x, float y, float w, float h, const std::string& label, bool hot) {
  Pix& P = *pix_;
  P.rect(x, y, w, h, hot ? Color(0.36f, 0.27f, 0.14f, 0.95f) : Color(0.16f, 0.13f, 0.11f, 0.95f));
  P.frame(x, y, w, h, hot ? kGold : Color(0.45f, 0.37f, 0.24f));
  P.text(x + w / 2, y + (h - 7) / 2, label, 1, hot ? Color(1, 0.95f, 0.8f) : kText, 1);
}
void View::wrapText(float x, float y, float w, const std::string& s, Color c, int maxChars, int lineH) {
  int per = std::max(4, (int)(w / 6));
  auto lines = wrap(s, per);
  int shown = 0;
  for (size_t i = 0; i < lines.size(); i++) {
    std::string L = lines[i];
    if (maxChars >= 0) {
      if (shown >= maxChars) break;
      if (shown + (int)L.size() > maxChars) L = L.substr(0, maxChars - shown);
      shown += (int)lines[i].size();
    }
    pix_->text(x, y + i * lineH, L, 1, c);
  }
}

// ------------------------------------------------------------------ main draw
void View::draw(Game& g, bool hasSave) {
  Pix& P = *pix_;
  hasSave_ = hasSave;
  P.begin(Color(0, 0, 0));
  drawWorld(g);
  drawLighting(g);
  drawWeather(g, 0);
  // quest-giver markers after the night/weather pass so the "!" stays readable in the dark
  if (g.mode != Mode::Title && g.mode != Mode::Creator)
    drawMarkers(g, Vec2(std::floor(cam_.x + shakeOff_.x), std::floor(cam_.y + shakeOff_.y)));
  // floating combat text (screen)
  Vec2 cam(std::floor(cam_.x), std::floor(cam_.y));
  for (const FloatText& f : texts_) {
    float a = clampf(1.2f - f.t, 0, 1);
    P.text(f.p.x - cam.x + 1, f.p.y - cam.y + 1, f.s, 1, Color(0, 0, 0, a * 0.7f), 1);
    P.text(f.p.x - cam.x, f.p.y - cam.y, f.s, 1, Color(f.c.r, f.c.g, f.c.b, a), 1);
  }
  if (g.mode == Mode::Title) { drawTitle(g, hasSave); }
  else if (g.mode == Mode::Creator) { drawCreator(g); }
  else {
    // (M1 round 3) the HUD steps away under the menu and the shop (its bars, gold, minimap and the hamburger showed
    // round the panel's edges on a wide phone)
    if (g.mode != Mode::Menu && g.mode != Mode::Shop && g.mode != Mode::LevelUp) drawHud(g);
    if (touchUI && (g.mode == Mode::Play)) drawTouch(g);
    if (g.mode == Mode::Dialogue) drawDialogue(g);
    // toasts and the banner wait while a dialogue is open (render.cpp holds their timers): on a phone the touch
    // panel is tall and anything drawn over it hides the speaker and the first line
    if (g.mode != Mode::Shop && g.mode != Mode::Menu && g.mode != Mode::Dialogue) drawToasts();
    if (g.mode == Mode::Shop) drawShop(g);
    if (g.mode == Mode::Menu) drawMenu(g);
    if (g.mode == Mode::LevelUp) drawLevelUp(g);
    if (g.mode == Mode::Dead) drawDead(g);
  }
  if (settingsOpen_) drawSettings();
  float f = std::max(fade_, g.sleepFade > 0 ? std::min(1.0f, g.sleepFade) : 0.0f);
  if (f > 0) P.rect(0, 0, Pix::W, Pix::H, Color(0, 0, 0, f));
  // (M2) travel behind the fade: where to and how long, on the black (a respawn says nothing: the death screen did)
  // (M2 fixer round 2: not while paused: the PAUSED box below says what is going on)
  if (g.travelling() && g.travel.kind != 2 && f > 0.05f && g.mode != Mode::Paused) {
    std::string dest = g.travel.site >= 0 && g.travel.site < (int)g.world.sites.size() ? g.world.sites[(size_t)g.travel.site].name : std::string("THE ROAD");
    const int hrs = std::max(1, (int)std::lround(g.travel.hours));
    const std::string l1 = (g.travel.kind == 1 ? "BY CARRIAGE TO " : "TRAVELLING TO ") + dest;
    const std::string l2 = std::to_string(hrs) + (hrs == 1 ? " HOUR" : " HOURS") + " ON THE ROAD";
    const UiBox b = uiBox(270);
    const float cx = b.x + b.w / 2.0f, cy = b.y + b.h / 2.0f, a = clampf((f - 0.05f) * 3, 0, 1);
    const int sc = P.textW(l1, 2) <= b.w - 24 ? 2 : 1;
    P.textS(cx, cy - 6 - 7 * sc, l1, sc, Color(kGold.r, kGold.g, kGold.b, a), 1);
    P.rect(cx - 60, cy + 2, 120, 1, Color(kGold.r, kGold.g, kGold.b, 0.5f * a));
    P.text(cx, cy + 9, l2, 1, Color(kText.r, kText.g, kText.b, a), 1);
    // a dotted road that fills while the journey is under way
    const int dots = 9, lit = std::min(dots, (int)(t_ * 4) % (dots + 1));
    for (int k = 0; k < dots; k++) P.rect(cx - 40 + k * 10, cy + 24, 2, 2, k < lit ? Color(kGold.r, kGold.g, kGold.b, a) : Color(0.35f, 0.3f, 0.25f, a));
  }
  // (M2 fixer round 2) the pause box over everything, the fade too: a pause on the road (a key, or the phone switching
  // apps) used to leave a black screen that looked hung
  if (g.mode == Mode::Paused) {
    P.rect(0, 0, Pix::W, Pix::H, Color(0, 0, 0, 0.5f));
    const UiBox b = uiBox(270);
    P.pushBox(b.x, b.y, b.w, b.h);
    P.text(Pix::W / 2, Pix::H / 2 - 15, "PAUSED", 3, kGold, 1);
    P.text(Pix::W / 2, Pix::H / 2 + 15, "ESC / TAP TO RESUME", 1, kText, 1);
    if (g.travelling() && g.travel.kind != 2) {
      const std::string dest = g.travel.site >= 0 && g.travel.site < (int)g.world.sites.size() ? g.world.sites[(size_t)g.travel.site].name : std::string("THE ROAD");
      P.text(Pix::W / 2, Pix::H / 2 + 30, "ON THE ROAD TO " + dest, 1, Color(kText.r, kText.g, kText.b, 0.7f), 1);
    }
    P.popBox();
  }
  if (!loadingCard.empty()) {   // (M1) the world is being made: say so instead of a frozen title
    P.rect(0, 0, Pix::W, Pix::H, Color(0.03f, 0.025f, 0.04f));
    P.text(Pix::W / 2, Pix::H / 2 - 12, loadingCard, 2, kGold, 1);
    P.text(Pix::W / 2, Pix::H / 2 + 14, "...", 2, kText, 1);
  }
}

// (M2, owner carry-over 1) The panels' box: the whole safe area (at least 480 x 270: screen.cpp guarantees that much),
// so on a phone the menus, the shop, the dialogue and the creator use its real width and height instead of a centred
// 480-wide box with dead bands either side. Every panel lays itself out from Pix::W / Pix::H inside the box. Only a
// very large logical canvas (a big window in PIXEL PERFECT at 1x) is capped, so text never strings out across a
// whole monitor: at most kMaxBoxW x max(maxH, kMaxBoxH), centred.
namespace { constexpr int kMaxBoxW = 760, kMaxBoxH = 400; }
View::UiBox View::uiBox(int maxH) const {
  const int aw = Pix::W - Pix::SL - Pix::SR, ah = Pix::H - Pix::ST - Pix::SB;
  UiBox b;
  b.w = std::max(1, std::min(kMaxBoxW, aw));
  b.h = std::max(1, std::min(std::max(maxH, kMaxBoxH), ah));
  b.x = Pix::SL + (aw - b.w) / 2;
  b.y = Pix::ST + (ah - b.h) / 2;
  return b;
}
// the modal panels' box (menu, shop, dialogue, level-up): the same safe-area box; the MAP tab gets all of it
View::UiBox View::modalBox(const Game& g) const {
  UiBox b = uiBox(300);
  if (g.mode == Mode::Menu && menuTab_ == 2) {
    const int aw = Pix::W - Pix::SL - Pix::SR, ah = Pix::H - Pix::ST - Pix::SB;
    b.w = std::max(1, aw); b.h = std::max(1, ah);
    b.x = Pix::SL; b.y = Pix::ST;
  }
  return b;
}
int View::titleItems() const { return hasSave_ ? 3 : 2; }

void View::drawHud(Game& g) {
  Pix& P = *pix_;
  const Actor& p = g.pl();
  // vitals
  float mpMax = g.maxMp, stMax = g.maxSt;
  for (int idx : g.worn()) {
    if (idx < 0) continue;
    if (g.inv[idx].ench == Ench::Magicka) mpMax += g.inv[idx].enchPow;
    if (g.inv[idx].ench == Ench::Stamina) stMax += g.inv[idx].enchPow;
  }
  auto bar = [&](float x, float y, float w, float v, float mx, Color c, Color dark) {
    P.rect(x - 1, y - 1, w + 2, 6, Color(0.05f, 0.04f, 0.06f, 0.85f));
    P.rect(x, y, w, 4, Color(dark.r, dark.g, dark.b, 0.9f));
    float k = clampf(v / std::max(1.0f, mx), 0, 1);
    P.rect(x, y, w * k, 4, c);
    P.rect(x, y, w * k, 1, Color(std::min(1.0f, c.r + 0.3f), std::min(1.0f, c.g + 0.3f), std::min(1.0f, c.b + 0.3f)));
  };
  // M1: every HUD piece is anchored to the safe area (the notch side of a phone, the HUD MARGIN setting)
  const float L = (float)Pix::SL, T = (float)Pix::ST, R = (float)(Pix::W - Pix::SR), B = (float)(Pix::H - Pix::SB);
  P.textS(L + 6, T + 5, "LV " + std::to_string(g.plLevel), 1, kGold);
  bar(L + 36, T + 6, 90, p.hp, p.maxHp, Color(0.85f, 0.18f, 0.16f), Color(0.3f, 0.05f, 0.05f));
  bar(L + 36, T + 14, 70, g.mp, mpMax, Color(0.25f, 0.45f, 0.95f), Color(0.06f, 0.1f, 0.3f));
  bar(L + 36, T + 22, 70, g.stamina, stMax, Color(0.3f, 0.8f, 0.35f), Color(0.06f, 0.22f, 0.08f));
  if (g.stFlash > 0) P.rect(L + 35, T + 21, 72, 6, Color(1, 0.25f, 0.2f, ((int)(g.stFlash * 16) & 1) ? 0.75f : 0.3f));   // too winded to act
  // xp sliver
  P.rect(L + 6, T + 14, 26, 2, Color(0.1f, 0.1f, 0.1f, 0.8f));
  P.rect(L + 6, T + 14, 26 * clampf(g.plXp / (float)g.xpForNext(), 0, 1), 2, kGold);
  // gold, arrows, spell
  int arrows = 0;
  for (auto& it : g.inv) if (it.kind == ItemKind::Arrows) arrows += it.count;
  P.blit(iconTex(art::Icon::Gold, 0), L + 4, T + 28);
  P.textS(L + 20, T + 33, std::to_string(g.gold), 1, kGold);
  P.blit(iconTex(art::Icon::Arrows, 0), L + 56, T + 28);
  P.textS(L + 72, T + 33, std::to_string(arrows), 1, kText);
  if (g.spellsKnown & (1 << (int)g.spell)) P.textS(L + 100, T + 33, spellName(g.spell), 1, Color(0.6f, 0.75f, 1.0f));
  if (g.perkPts > 0 && ((int)(t_ * 2) & 1)) P.textS(L + 6, T + 44, "LEVEL UP! OPEN MENU", 1, kGold);
  if (g.blessT > 0) P.textS(L + 6, T + (g.perkPts > 0 ? 54 : 44), g.blessName, 1, Color(0.7f, 0.85f, 1.0f, 0.8f));

  // M0b fix round 3: inside a building the rooms fill the screen and a wide one runs on under the minimap and the
  // location / quest column. The building is in full view, so its minimap is dropped there, and the column fades to
  // a faint overlay (fainter still with the hero right under it) so the hearth corner and the shelves show through.
  float hudA = 1.0f, backA = 0.58f;   // (M2 fixer: 0.38 let bright paving, snow and roofs wash the lines out)
  bool showMini = true;
  if (g.inside && g.subBldg >= 0) {
    // (M1) a big hall (the palace, a keep) runs on under the minimap and the column: keep both readable over the
    // shelves and beds with a firmer backing, and step aside only while the hero is right under them
    const Map& m = g.map();
    const Vec2 cam(std::floor(cam_.x), std::floor(cam_.y));
    float mx1 = m.w * 16.0f - cam.x, my0 = -16.0f - cam.y;
    const Vec2 ps = g.pl().p - cam;
    if (mx1 > R - 74 && my0 < T + 90 && ps.x > R - 84 && ps.y < T + 100) showMini = false;
    if (mx1 > R - 160 && my0 < T + 130) {   // the rooms run on under the location / quest column
      backA = 0.74f;
      if (ps.x > R - 170 && ps.y < T + 160) { hudA = 0.35f; backA = 0.3f; }
    }
  }
  // minimap + location
  if (showMini) drawMinimap(g, R - 70, T + 24, 64);
  auto fa = [&](Color c) { c.a *= hudA; return c; };
  std::string loc = g.locName;
  {   // (M2 integration) long wayside names ("UNDERBRIDGE ON THE COLDBROOK"): more room on a wide screen, and a cut
      // falls between words, never mid-word
    const size_t maxc = Pix::W >= 560 ? 32 : 26;
    if (loc.size() > maxc) {
      const size_t sp = loc.find_last_of(' ', maxc);
      loc.resize(sp != std::string::npos && sp >= maxc / 2 ? sp : maxc);
      // never end on a dangling little word ("UNDERBRIDGE ON THE")
      for (const char* w : {" THE", " ON", " OF", " IN", " AT", " BY", " AND"}) {
        const size_t n = std::strlen(w);
        if (loc.size() > n + 4 && loc.compare(loc.size() - n, n, w) == 0) loc.resize(loc.size() - n);
      }
    }
  }
  // clock
  int hh = (int)g.hour, mm = (int)((g.hour - hh) * 60);
  char clock[16];
  std::snprintf(clock, sizeof clock, "%02d:%02d", hh, mm);
  std::string dayStr = std::string("DAY ") + std::to_string(g.day) + " " + clock;
  // (M1) kingdom identity: in a settlement, the line under its name says whose it is (and marks a capital)
  std::string kingLine;
  Color kingCol = kDim;
  if (g.curSite >= 0 && g.curSite < (int)g.world.sites.size() && g.world.sites[g.curSite].settlement())
    if (const Kingdom* k = g.world.kingdomOf(g.curSite)) {
      kingLine = (g.world.sites[g.curSite].capital ? "CAPITAL OF " : "KINGDOM OF ") + k->name;
      if (kingLine.size() > 26) kingLine = kingLine.substr(0, 26);
      if (k->color) kingCol = mixKing(col(k->color));
    }
  // (M2) a wayside place or a wonder: the line under its name says what it is ("HUNTER'S CAMP", "ELDER TREE"), unless
  // its name already does
  if (!g.inside && g.curSite >= 0 && g.curSite < (int)g.world.sites.size()) {
    const Site& cs = g.world.sites[(size_t)g.curSite];
    if (cs.type == SiteType::Vignette || cs.type == SiteType::Wonder) {
      const std::string kn = ew::poiKindName(cs.type, cs.kind);
      if (loc.find(kn) == std::string::npos) {
        kingLine = cs.type == SiteType::Wonder ? "WONDER - " + kn : kn;
        if (kingLine.size() > 26) kingLine = kn.substr(0, 26);
        kingCol = cs.type == SiteType::Wonder ? Color(0.75f, 0.85f, 1.0f) : Color(0.85f, 0.78f, 0.58f);
      }
    }
  }
  const float kdy = kingLine.empty() ? 0.0f : 9.0f;
  float colBottom = T + 150;   // the foot of the location / quest column (the quest arrow keeps below it)
  // (M2) a dialogue panel runs under the column on a wide screen: the column's lines stop above the panel's top
  float clipY = 1e9f;
  if (g.mode == Mode::Dialogue) {
    const UiBox db = uiBox(300);
    P.pushBox(db.x, db.y, db.w, db.h);
    clipY = db.y + dlgLay(g, dlgRowH()).y - 2;
    P.popBox();
  }
  auto colText = [&](float y, const std::string& s, Color c) { if (y + 8 <= clipY) P.textS(R - 5, y, s, 1, c, 2); };
  {
    // a soft backing so the location / clock / quest column stays readable over busy roofs and snow
    const Quest* tq = g.trackedQuest >= 0 ? g.questById(g.trackedQuest) : nullptr;
    bool hasQ = tq && tq->state != QState::Done;
    int wmax = std::max(std::max(P.textW(loc, 1), P.textW(dayStr, 1)), P.textW(kingLine, 1));
    if (hasQ) wmax = std::max(wmax, P.textW(tq->title.size() > 22 ? tq->title.substr(0, 22) : tq->title, 1));
    float extra = 0;
    if (hasQ) {
      const std::string st = trackedStep(g, *tq);
      const size_t nl = st.find('\n');
      wmax = std::max(wmax, P.textW(st.substr(0, nl), 1));
      if (nl != std::string::npos) { wmax = std::max(wmax, P.textW(st.substr(nl + 1), 1)); extra = 9.0f; }
    }
    float bh = (hasQ ? 42.0f : 21.0f) + kdy + extra;
    P.rect(R - wmax - 10, T + 88, (float)wmax + 8, std::max(0.0f, std::min(bh, clipY - (T + 88))), Color(0.03f, 0.02f, 0.05f, backA));
    colBottom = T + 88 + bh + 12;
  }
  colText(T + 91, loc, fa(kText));
  if (!kingLine.empty() && T + 108 <= clipY) {
    // the kingdom line in its colour, with a full dark outline (its tint sits close to the land's own colours)
    const Color o(0.02f, 0.02f, 0.04f, 0.9f * hudA);
    for (int d = 0; d < 4; d++) P.text(R - 5 + (d == 0 ? -1.0f : d == 1 ? 1.0f : 0.0f), T + 100 + (d == 2 ? -1.0f : d == 3 ? 1.0f : 0.0f), kingLine, 1, o, 2);
    P.text(R - 4, T + 101, kingLine, 1, o, 2);
    P.text(R - 5, T + 100, kingLine, 1, fa(kingCol), 2);
  }
  colText(T + 100 + kdy, dayStr, fa(kDim));
  if (!touchUI) { P.textS(R - 24, T + 7, "TAB", 1, kDim, 1); }
  else {
    const BtnDef b = btnPos(B_MENU);
    P.rect(b.x - 9, b.y - 8, 18, 16, Color(0.1f, 0.08f, 0.08f, 0.8f));
    P.frame(b.x - 9, b.y - 8, 18, 16, Color(0.6f, 0.5f, 0.3f));
    for (int k = 0; k < 3; k++) P.rect(b.x - 5, b.y - 4 + k * 4, 10, 2, kText);
  }

  // tracked quest
  if (g.trackedQuest >= 0) {
    const Quest* q = g.questById(g.trackedQuest);
    if (q && q->state != QState::Done) {
      const std::string obj = trackedStep(g, *q);
      std::string t = q->title.size() > 22 ? q->title.substr(0, 22) : q->title;
      colText(T + 112 + kdy, t, fa(kGold));
      if (!obj.empty()) {
        const size_t nl = obj.find('\n');
        colText(T + 121 + kdy, obj.substr(0, nl), fa(kText));
        if (nl != std::string::npos) colText(T + 130 + kdy, obj.substr(nl + 1), fa(kText));
      }
      // off-screen arrow toward the objective
      int tx, ty;
      if (!g.inside && questTile(g, *q, tx, ty)) {
        Vec2 tgt(tx * 16 + 8.0f, ty * 16 + 8.0f);
        Vec2 sc = tgt - Vec2(std::floor(cam_.x), std::floor(cam_.y));
        bool on = sc.x > L + 10 && sc.y > T + 10 && sc.x < R - 10 && sc.y < B - 10;
        float dist = len(tgt - p.p) / 16;
        if (!on) {
          // the arrow rides the edge of the safe area, around its centre
          Vec2 c((L + R) / 2.0f, (T + B) / 2.0f);
          Vec2 d = norm(sc - c);
          float k = std::min(((R - L) / 2.0f - 18) / std::max(0.01f, std::fabs(d.x)), ((B - T) / 2.0f - 18) / std::max(0.01f, std::fabs(d.y)));
          Vec2 a = c + d * k;
          // slide along the screen edge out of the HUD corners (vitals top-left, minimap + quest column top-right)
          if (a.x < L + 150 && a.y < T + 58) { if (a.y <= T + 20) a.x = L + 150; else a.y = T + 58; }
          // (M1 round 3) below the column's real foot (a two-line step and a kingdom line made it taller than the old
          // fixed limit, and the arrow and its distance sat on the quest's last line)
          if (a.x > R - 160 && a.y < colBottom) { if (a.y <= T + 20) a.x = R - 160; else a.y = colBottom; }
          // (M3 fixer round 3, review: "on the phone the quest compass draws inside the TALK button") with the touch
          // layout on, it also slides out of the action buttons (bottom right) and the movement stick (bottom left)
          if (touchUI) {
            const BtnDef bw = btnPos(B_BOW), rl = btnPos(B_ROLL), at = btnPos(B_ATTACK);
            const float cx0 = bw.x - bw.r - 24, cy0 = std::min(rl.y - rl.r, btnPos(B_SPELL).y - btnPos(B_SPELL).r) - 14;
            const float cyB = at.y + at.r;
            if (a.x > cx0 && a.y > cy0) { if (a.y >= cyB - 4 || a.y > B - 30) a.x = cx0; else a.y = cy0; }
            const Vec2 st = stickRest();
            const float sx1 = st.x + 50, sy0 = st.y - 50;
            if (a.x < sx1 && a.y > sy0) { if (a.y > B - 30) a.x = sx1; else a.y = sy0; }
          }
          // a filled arrowhead (dark outline first so it reads on snow and sand), gently pulsing toward the goal
          a = a + d * (std::sin(t_ * 6) * 1.5f);
          Vec2 side(-d.y, d.x);
          // a bold ">" chevron: two 2px arms meeting at the tip
          for (int pass = 0; pass < 2; pass++) {
            Color cc = pass == 0 ? Color(0.08f, 0.05f, 0.03f, 0.85f) : kGold;
            float grow = pass == 0 ? 1.0f : 0.0f;
            for (float t = 0; t <= 6.0f; t += 0.5f)
              for (int arm = -1; arm <= 1; arm += 2) {
                Vec2 q2 = a - d * t + side * (t * (float)arm);
                P.rect(std::floor(q2.x) - grow, std::floor(q2.y) - grow, 2 + grow * 2, 2 + grow * 2, cc);
              }
          }
          // the distance sits behind the arrowhead (further back on diagonals, where the label is widest)
          float back = 15 + 8 * std::fabs(d.x);
          P.textS(a.x - d.x * back, a.y - d.y * back - 3, distText(dist), 1, kGold, 1);
        } else if (!(q->state == QState::Complete && [&] {
                     // the giver already wears the world "!" bubble (drawMarkers): don't stack a second pin on it
                     for (const Actor& a2 : g.actors)
                       if (a2.npc && a2.st != AState::Dead && len2(a2.p - tgt) < 48.0f * 48.0f && g.rewardWaiting(a2)) return true;
                     return false;
                   }())) {
          // an outlined quest pin in the "!" bubble's style (render_markers.cpp): a gold diamond with a dark outline
          // and a tail pointing down. A door target (a building) carries it above the roof line, not on the facade.
          float pinY = sc.y - 26;   // tail tip
          const Map& om = g.map();
          for (int bi = 0; bi < (int)om.bldgs.size(); bi++) {
            const Bldg& b = om.bldgs[bi];
            if (b.doorX() != tx || b.doorY() != ty) continue;
            const uint64_t k = bldgKey(om, b, bi);
            auto bt = bldgTex_.find(k);
            auto tr = bldgTopRow_.find(k);
            if (bt != bldgTex_.end() && tr != bldgTopRow_.end()) {
              const float top = (b.r.y + b.r.h) * 16.0f + art::BLDG_PAD_B - bt->second.h + tr->second;
              pinY = std::min(pinY, top - std::floor(cam_.y) - 3);
            }
            break;
          }
          // never into the HUD: above the screen top or behind the vitals block, it sits just over the door instead
          if (pinY - 11 < T + 6 || (sc.x < L + 160 && pinY - 11 < T + 46)) pinY = sc.y - 22;
          const int bob = (int)std::lround(std::sin(t_ * 4) * 1.2f);
          static const char* kPin[12] = {
              "....ooo....", "...oHYYo...", "..oHYYYSo..", ".oHYYYYYSo.", "oHYYYWYYYSo", "oYYYWWWYYSo",
              ".oYYYWYYSo.", "..oYYYYSo..", "...oYSSo...", "....oSo....", "....oSo....", ".....o.....",
          };
          const Color outline(0.09f, 0.06f, 0.08f), hi(1.0f, 0.9f, 0.5f), gold(1.0f, 0.7f, 0.16f), goldShade(0.72f, 0.36f, 0.06f),
              core(1.0f, 0.98f, 0.9f);
          const int x0 = (int)std::floor(sc.x) - 5, y0 = (int)std::floor(pinY) - 11 + bob;
          for (int pass = 0; pass < 2; pass++)
            for (int r = 0; r < 12; r++)
              for (int c = 0; c < 11; c++) {
                char ch = kPin[r][c];
                if (ch == '.') continue;
                if (pass == 0) { P.rect((float)(x0 + c + 1), (float)(y0 + r + 1), 1, 1, Color(0, 0, 0, 0.35f)); continue; }
                P.rect((float)(x0 + c), (float)(y0 + r), 1, 1,
                       ch == 'o' ? outline : ch == 'H' ? hi : ch == 'S' ? goldShade : ch == 'W' ? core : gold);
              }
        }
      }
    }
  }

  // boss bar
  for (const Actor& a : g.actors) {
    if (!a.boss || a.st == AState::Dead || !a.aggro) continue;
    if (len2(a.p - p.p) > 260 * 260) continue;
    float w = 200, x = (Pix::W - w) / 2, y = B - 22;
    P.textS(Pix::W / 2, y - 10, a.name, 1, Color(1, 0.75f, 0.6f), 1);
    P.rect(x - 1, y - 1, w + 2, 6, Color(0, 0, 0, 0.85f));
    P.rect(x, y, w * clampf(a.hp / a.maxHp, 0, 1), 4, Color(0.75f, 0.12f, 0.1f));
    break;
  }

  // NPC name / interact prompt
  int it = g.mode == Mode::Play ? g.interactTarget() : -1;
  if (it >= 0) {
    for (const Actor& a : g.actors)
      if (a.id == it) {
        Vec2 s = a.p - Vec2(std::floor(cam_.x), std::floor(cam_.y));
        // a giver with a reward waiting wears the "!" bubble (drawMarkers) just above the head: the label goes over it
        float ly = g.rewardWaiting(a) ? s.y - art::HUMAN_H - 24.0f : s.y - 34.0f;
        // keep the label off the hero: when the hero's head is where the label would go, lift it clear
        const Vec2 hs = g.pl().p - Vec2(std::floor(cam_.x), std::floor(cam_.y));
        const float labelW = (float)std::max(P.textW(a.name, 1), touchUI ? 0 : P.textW("E: TALK", 1)) * 0.5f + 6;
        const float labelTop = ly - (touchUI ? 2.0f : 11.0f);
        if (std::fabs(hs.x - s.x) < labelW + 6 && hs.y - 30.0f < ly + 8 && hs.y > labelTop - 4)
          ly = std::min(ly, hs.y - 38.0f);
        // and off the HUD's top-left block (vitals, gold, arrows, spell name): an NPC near the top of a room had its
        // name run into the spell text ("FIREBOLTULONMAR"). There the label goes under the NPC's feet instead (the
        // floor below a person at the top of a room is open; the "!" bubble stays over the head).
        float lx = s.x;
        {
          const float hudR = L + (float)std::max(130, 100 + P.textW(spellName(g.spell), 1)) + 12;
          const float hudB = T + ((g.perkPts > 0 || g.blessT > 0) ? (g.perkPts > 0 && g.blessT > 0 ? 64.0f : 54.0f) : 46.0f);
          const float half = labelW - 6;
          const float topY = ly - (touchUI ? 0.0f : 9.0f);
          if (lx - half < hudR && topY < hudB) ly = s.y + (touchUI ? 4.0f : 13.0f);
        }
        P.textS(lx, ly, a.name, 1, kText, 1);
        if (!touchUI) P.textS(lx, ly - 9, "E: TALK", 1, kGold, 1);
      }
  } else if (g.mode == Mode::Play) {
    int ptx = 0, pty = 0, pr = usablePropAt(g, ptx, pty);
    if (pr) {
      Vec2 s = Vec2(ptx * 16 + 8.0f, pty * 16 - 4.0f) - Vec2(std::floor(cam_.x), std::floor(cam_.y));
      P.textS(s.x, s.y - 10, std::string(touchUI ? "TAP: " : "E: ") + useVerbAt(g, pr, ptx, pty), 1, kGold, 1);
    }
  }

  // (toasts are drawn by drawToasts, above the dialogue panel)
  // (M2 fixer round 2) not under a dialogue (it showed through the panel as ghost text), on a backing band (a 1-px
  // drop shadow was lost over foliage and birch trunks), and clear of the quest pointer's distance at the bottom
  if (g.noticeT > 0 && g.mode != Mode::Dialogue) {
    float a = clampf(g.noticeT, 0, 1);
    // M0b fix round 2: with the touch buttons on, a notice too wide to clear the bow button (x >= 360) breaks into two
    // centred lines at the space nearest its middle
    std::string l1 = g.notice, l2;
    if (touchUI && P.textW(g.notice, 1) > 2 * (btnPos(B_BOW).x - btnPos(B_BOW).r - 6 - Pix::W / 2)) {
      size_t mid = g.notice.size() / 2, best = std::string::npos;
      for (size_t i = 0; i < g.notice.size(); i++)
        if (g.notice[i] == ' ' && (best == std::string::npos || (i > mid ? i - mid : mid - i) < (best > mid ? best - mid : mid - best))) best = i;
      if (best != std::string::npos) { l1 = g.notice.substr(0, best); l2 = g.notice.substr(best + 1); }
    }
    float ny = B - 64 - (l2.empty() ? 0.0f : 9.0f);
    {
      const float bw = (float)std::max(P.textW(l1, 1), l2.empty() ? 0 : P.textW(l2, 1)) + 16;
      const float bh = l2.empty() ? 13.0f : 22.0f;
      P.rect(Pix::W / 2 - bw / 2, ny - 3, bw, bh, Color(0.04f, 0.03f, 0.05f, 0.62f * a));
      P.rect(Pix::W / 2 - bw / 2 + 4, ny - 3, bw - 8, 1, Color(kGold.r, kGold.g, kGold.b, 0.35f * a));
      P.rect(Pix::W / 2 - bw / 2 + 4, ny - 4 + bh, bw - 8, 1, Color(kGold.r, kGold.g, kGold.b, 0.35f * a));
    }
    for (const std::string* L : {&l1, &l2}) {
      if (L->empty()) continue;
      P.textS(Pix::W / 2 + 1, ny + 1, *L, 1, Color(0, 0, 0, a * 0.6f), 1);
      P.textS(Pix::W / 2, ny, *L, 1, Color(1, 0.95f, 0.85f, a), 1);
      ny += 9;
    }
  }
  // banner
  if (bannerT_ > 0 && g.mode != Mode::Dialogue) {
    float a = clampf(std::min(bannerT_, 4.0f - bannerT_) * 2.5f, 0, 1);
    float y = T + 56;
    P.rect(0, y - 6, Pix::W, 34, Color(0, 0, 0, 0.45f * a));
    P.rect(Pix::W / 2 - 90, y - 6, 180, 1, Color(kGold.r, kGold.g, kGold.b, a * 0.8f));
    P.rect(Pix::W / 2 - 90, y + 27, 180, 1, Color(kGold.r, kGold.g, kGold.b, a * 0.8f));
    P.textS(Pix::W / 2, y, banner_, 1, Color(kGold.r, kGold.g, kGold.b, a), 1);
    P.textS(Pix::W / 2 + 1, y + 11, bannerSub_, 2, Color(0, 0, 0, a * 0.5f), 1);
    P.textS(Pix::W / 2, y + 10, bannerSub_, 2, Color(1, 0.97f, 0.9f, a), 1);
  }
}

// Toasts (items received, quest steps, notices): a column on the left under the vitals, clear of the thumbs (the
// touch stick and buttons sit at the bottom), the centre notice and the quest column; newest at the top.
// (M2) a toast never runs under the minimap or the quest column (the right 160 px of the safe area): a long one wraps
// onto a second line, and the column stops above the touch stick.
void View::drawToasts() {
  Pix& P = *pix_;
  const float L = (float)Pix::SL, R = (float)(Pix::W - Pix::SR), B = (float)(Pix::H - Pix::SB);
  const int per = std::max(16, (int)((R - 166 - (L + 6)) / 6));
  const float bottom = B - (touchUI ? 86.0f : 30.0f);
  float ty = (float)Pix::ST + 92;
  int shown = 0;
  for (int i = (int)toasts_.size() - 1; i >= 0 && shown < 5; i--, shown++) {
    const Toast& t = toasts_[i];
    float a = clampf(4.0f - t.t, 0, 1);
    auto lines = wrap(t.s, per);
    if (ty + lines.size() * 10.0f > bottom) break;
    for (size_t k = 0; k < lines.size(); k++) {
      const std::string& s = lines[k];
      float w = (float)P.textW(s, 1);
      P.rect(L + 3, ty - 2, w + 6, k + 1 < lines.size() ? 10.0f : 10.0f, Color(0.03f, 0.02f, 0.05f, 0.42f * a));
      P.textS(L + 7, ty + 1, s, 1, Color(0, 0, 0, a * 0.6f));
      P.textS(L + 6, ty, s, 1, Color(t.c.r, t.c.g, t.c.b, a));
      ty += k + 1 < lines.size() ? 10.0f : 11.0f;
    }
  }
}

void View::drawTouch(Game& g) {
  Pix& P = *pix_;
  // joystick
  if (stick_.on) {
    Vec2 d = stick_.cur - stick_.start;
    float l = len(d);
    if (l > 26) d = d * (26 / l);
    for (int r = 26; r > 0; r -= 2) P.rect(stick_.start.x - r, stick_.start.y - 1, r * 2, 2, Color(1, 1, 1, 0.0f));
    P.blitEx(light_, 0, 0, 64, 64, stick_.start.x - 30, stick_.start.y - 30, 60, 60, false, Color(0.9f, 0.9f, 1, 0.25f));
    P.blitEx(light_, 0, 0, 64, 64, stick_.start.x + d.x - 14, stick_.start.y + d.y - 14, 28, 28, false, Color(1, 1, 1, 0.6f));
  } else {
    // (M1 round 3) the stick at rest is drawn like the buttons (a dark disc in a ring, with its thumb and four
    // direction ticks), so a new phone player sees where to drag: the old faint glow vanished on grass and cobbles
    const Vec2 rest = stickRest();
    const float R = 26, r2 = 11;
    for (int yy = (int)-R; yy <= (int)R; yy++) {
      const float hw = std::sqrt(std::max(0.0f, R * R - yy * yy));
      P.rect(rest.x - hw, rest.y + yy, hw * 2, 1, Color(0.08f, 0.07f, 0.09f, 0.32f));
    }
    for (int k = 0; k < 64; k++) {
      const float a = k / 64.0f * TAU;
      P.rect(rest.x + std::cos(a) * R, rest.y + std::sin(a) * R, 1, 1, Color(0.9f, 0.85f, 0.75f, 0.5f));
    }
    for (int k = 0; k < 4; k++) {   // direction ticks
      const float a = k * TAU / 4;
      const float cx = rest.x + std::cos(a) * (R - 6), cy = rest.y + std::sin(a) * (R - 6);
      P.rect(cx - 1, cy - 1, 2, 2, Color(0.95f, 0.9f, 0.8f, 0.45f));
    }
    for (int yy = (int)-r2; yy <= (int)r2; yy++) {
      const float hw = std::sqrt(std::max(0.0f, r2 * r2 - yy * yy));
      P.rect(rest.x - hw, rest.y + yy, hw * 2, 1, Color(0.85f, 0.82f, 0.78f, 0.38f));
    }
    for (int k = 0; k < 32; k++) {
      const float a = k / 32.0f * TAU;
      P.rect(rest.x + std::cos(a) * r2, rest.y + std::sin(a) * r2, 1, 1, Color(0.12f, 0.1f, 0.14f, 0.6f));
    }
  }
  int target = g.interactTarget();
  int utx = 0, uty = 0, usePr = target < 0 ? usablePropAt(g, utx, uty) : 0;
  for (int b = 0; b < B_MENU; b++) {
    const BtnDef d = btnPos(b);
    bool held = false;
    for (auto& f : fingers_) if (f.on && f.button == b) held = true;
    Color ring = held ? Color(1, 0.9f, 0.6f, 0.9f) : Color(0.9f, 0.85f, 0.75f, 0.45f);
    P.blitEx(light_, 0, 0, 64, 64, d.x - d.r * 1.3f, d.y - d.r * 1.3f, d.r * 2.6f, d.r * 2.6f, false, Color(0.05f, 0.04f, 0.06f, 0.0f));
    // disc
    for (int yy = (int)-d.r; yy <= (int)d.r; yy++) {
      float hw = std::sqrt(std::max(0.0f, d.r * d.r - yy * yy));
      P.rect(d.x - hw, d.y + yy, hw * 2, 1, Color(0.08f, 0.07f, 0.09f, held ? 0.65f : 0.42f));
    }
    for (int k = 0; k < 40; k++) { float a = k / 40.0f * TAU; P.rect(d.x + std::cos(a) * d.r, d.y + std::sin(a) * d.r, 1, 1, ring); }
    switch (b) {
      case B_ATTACK:
        if (target >= 0) P.text(d.x, d.y - 3, "TALK", 1, kGold, 1);
        else if (usePr) P.text(d.x, d.y - 3, useVerbAt(g, usePr, utx, uty), 1, kGold, 1);
        else if (g.eqWeapon >= 0) P.blitEx(iconTex(g.inv[g.eqWeapon].icon, g.inv[g.eqWeapon].tint), 0, 0, 16, 16, d.x - 12, d.y - 12, 24, 24);
        else {
          // unarmed (the shirt-only start): a clenched fist, knuckles up, lit from the top-left
          static const char* kFist[] = {
              "..####.....",
              ".#aab#####.",
              "#aabb#aab#.",
              "#abbb#abb##",
              "#abbb#abbc#",
              "#abbbbbbbc#",
              "#bbbbbbbcc#",
              ".#bbbbbbc#.",
              "..#bbbbcc#.",
              "...#bbbc#..",
              "...#cccc#..",
              "....####...",
          };
          const Color ink(0.12f, 0.07f, 0.08f), la(1.0f, 0.86f, 0.70f), mb(0.88f, 0.66f, 0.50f), dc(0.62f, 0.40f, 0.33f);
          for (int yy = 0; yy < 12; yy++)
            for (int xx = 0; xx < 11; xx++) {
              char ch = kFist[yy][xx];
              if (ch == '.') continue;
              Color c = ch == '#' ? ink : ch == 'a' ? la : ch == 'b' ? mb : dc;
              P.rect(d.x - 11 + xx * 2, d.y - 12 + yy * 2, 2, 2, c);
            }
        }
        break;
      case B_BOW: P.blit(iconTex(art::Icon::Bow, 0), d.x - 8, d.y - 8); break;
      case B_SPELL: P.blit(iconTex(g.spell == Spell::Heal ? art::Icon::PotionGreen : (g.spell == Spell::IceSpike ? art::Icon::Gem : art::Icon::Staff), g.spell == Spell::Flames ? rgba(255, 120, 40) : rgba(120, 200, 255)), d.x - 8, d.y - 8); break;
      case B_ROLL: P.text(d.x, d.y - 3, "ROLL", 1, kText, 1); break;
      case B_POTION: P.blit(iconTex(art::Icon::PotionRed, 0), d.x - 8, d.y - 8); break;
      default: break;
    }
  }
}

void View::drawMinimap(Game& g, float x, float y, int size) {
  Pix& P = *pix_;
  miniT_ -= 0.016f;
  const Map& m = g.map();
  const Actor& p = g.pl();
  int ptx = (int)(p.p.x / 16), pty = (int)(p.p.y / 16);
  // window origin: centred on the player outdoors; inside, clamped to the map so a dungeon fills the box
  int ox = ptx - 32, oy = pty - 32;
  if (g.inside) {
    ox = m.w <= 64 ? (m.w - 64) / 2 : std::clamp(ox, 0, m.w - 64);
    oy = m.h <= 64 ? (m.h - 64) / 2 : std::clamp(oy, 0, m.h - 64);
  }
  if (miniT_ <= 0) {
    miniT_ = 0.2f;
    for (int yy = 0; yy < 64; yy++)
      for (int xx = 0; xx < 64; xx++) {
        int tx = ox + xx, ty = oy + yy;
        uint32_t k = rgba(10, 10, 14);
        if (m.in(tx, ty)) {
          Ground gr = m.at(tx, ty);
          switch (gr) {
            case Ground::DeepWater: k = rgba(30, 56, 104); break;
            case Ground::Water: k = rgba(52, 96, 150); break;
            case Ground::Sand: k = rgba(206, 188, 130); break;
            case Ground::Snow: k = rgba(220, 228, 236); break;
            case Ground::Rock: k = rgba(104, 98, 94); break;
            case Ground::CaveWall: case Ground::InteriorWall: k = rgba(30, 26, 26); break;
            case Ground::CaveFloor: k = rgba(110, 98, 88); break;
            case Ground::Road: case Ground::Bridge: case Ground::Plaza: k = rgba(176, 156, 118); break;
            case Ground::StoneFloor: k = rgba(130, 128, 136); break;
            case Ground::WoodFloor: k = rgba(140, 100, 66); break;
            case Ground::Dirt: case Ground::Farmland: k = rgba(134, 104, 70); break;
            case Ground::ForestFloor: k = rgba(56, 100, 52); break;
            case Ground::Autumn: k = rgba(160, 104, 52); break;
            case Ground::Tundra: k = rgba(104, 122, 96); break;
            case Ground::Swamp: k = rgba(70, 86, 56); break;
            default: k = rgba(86, 150, 66); break;
          }
          // (M2 fixer) the view's snowline (terrain.cpp reliefPixel): taiga and snowfields from level 4, mountains from
          // 5 lie under snow, so the minimap shows them white as the world does
          if (m.kind == MapKind::Overworld && !m.biome.empty() &&
              (gr == Ground::Grass || gr == Ground::Meadow || gr == Ground::ForestFloor || gr == Ground::Tundra || gr == Ground::Autumn)) {
            const Biome bb = m.biomeAt(tx, ty);
            const int lv = m.heightAt(tx, ty);
            if (((bb == Biome::Snow || bb == Biome::Taiga) && lv >= 4) || (bb == Biome::Mountain && lv >= 5)) k = rgba(214, 222, 232);
          }
          size_t i = (size_t)ty * m.w + tx;
          if (m.bldgAt[i] >= 0) k = rgba(150, 70, 56);
          else if (m.wall[i]) k = rgba(70, 66, 66);
          else if (m.solid[i]) k = art::shade(k, 0.7f);
        }
        miniPx_[(size_t)yy * 64 + xx] = k;
      }
    // (M2 fixer round 2) the mountains: a peak or a massif shows as a little dark mountain over its footprint (a lit
    // west flank, a shaded east one), so a summit the map names is on the minimap too
    if (m.kind == MapKind::Overworld)
      for (int yy = 0; yy < 64 + 3; yy++)
        for (int xx = -4; xx < 64 + 4; xx++) {
          const int tx = ox + xx, ty = oy + yy;
          if (!m.in(tx, ty)) continue;
          const int pp = m.propAt(tx, ty);
          if (pp != (int)art::Prop::Peak + 1 && pp != (int)art::Prop::GreatPeak + 1) continue;
          int fw = 1, fh = 1;
          art::wildFootprint((art::Prop)(pp - 1), fw, fh);
          const int hh = fh + 1;   // a row taller than the footprint: the summit stands above its foot
          for (int j = 0; j < hh; j++) {
            const int half = std::max(0, (fw / 2) * (j + 1) / hh);
            for (int i = -half; i <= half; i++) {
              const int qx = xx + i, qy = yy - hh + 1 + j;
              if (qx < 0 || qy < 0 || qx >= 64 || qy >= 64) continue;
              miniPx_[(size_t)qy * 64 + qx] = j == 0 ? rgba(236, 240, 246) : i < 0 ? rgba(120, 114, 124) : rgba(78, 74, 90);
            }
          }
        }
    pix_->miniUpdate(miniPx_.data());
  }
  P.rect(x - 2, y - 2, size + 4, size + 4, Color(0.05f, 0.04f, 0.05f, 0.9f));
  P.frame(x - 2, y - 2, size + 4, size + 4, Color(0.55f, 0.45f, 0.28f));
  P.miniDraw(x, y, size / 64.0f);
  // hostiles + npcs
  for (const Actor& a : g.actors) {
    if (a.player || a.st == AState::Dead) continue;
    int ax = (int)(a.p.x / 16) - ox, ay = (int)(a.p.y / 16) - oy;
    if (ax < 0 || ay < 0 || ax >= 64 || ay >= 64) continue;
    P.rect(x + ax * size / 64.0f, y + ay * size / 64.0f, 1, 1, a.hostile ? Color(1, 0.25f, 0.2f) : Color(0.9f, 0.9f, 0.5f));
  }
  if (!g.inside)
    for (const Site& s : g.world.sites) {
      if (!s.discovered) continue;
      int ax = s.ex - ox, ay = s.ey - oy;
      if (ax < 1 || ay < 1 || ax >= 63 || ay >= 63) continue;
      Color c = s.type == SiteType::Cave || s.type == SiteType::Ruin ? Color(0.75f, 0.6f, 1) : s.type == SiteType::BanditCamp ? Color(1, 0.4f, 0.3f) : Color(1, 0.9f, 0.6f);
      P.rect(x + ax - 1, y + ay - 1, 3, 3, Color(0, 0, 0, 0.8f));
      P.rect(x + ax, y + ay, 1, 1, c);
    }
  P.rect(x + (ptx - ox) * size / 64.0f - 1, y + (pty - oy) * size / 64.0f - 1, 3, 3, Color(1, 1, 1));
  int tx, ty;
  const Quest* mq = g.trackedQuest >= 0 ? g.questById(g.trackedQuest) : nullptr;
  if (!g.inside && mq && questTile(g, *mq, tx, ty)) {
    int ax = std::clamp(tx - ox, 1, 62), ay = std::clamp(ty - oy, 1, 62);
    if (((int)(t_ * 3)) & 1) P.rect(x + ax - 1, y + ay - 1, 3, 3, kGold);
  }
}

// ------------------------------------------------------------------ menus
void View::drawMenu(Game& g) {
  Pix& P = *pix_;
  P.rect(0, 0, Pix::W, Pix::H, Color(0, 0, 0, 0.55f));
  if (settingsOpen_) return;   // the settings screen replaces the menu while it is open
  const UiBox box = modalBox(g);
  P.pushBox(box.x, box.y, box.w, box.h);
  struct Pop { Pix& p; ~Pop() { p.popBox(); } } pop{P};
  panel(8, 6, Pix::W - 16, Pix::H - 12);
  // tabs, and the close X at the strip's right end (finger-sized on touch)
  const TabLay T = tabLay(touchUI);
  for (int i = 0; i < NTABS; i++) button(T.x0 + i * T.tw, T.y, T.tw - 4, T.h, kTabs[kTabOrder[i]], kTabOrder[i] == menuTab_);
  button(T.xX, T.xY, T.xW, T.xH, "X", false);
  float top = 34;
  // a touch list's page buttons and its "1-8 OF 15" count (keyboard: the count alone, under the list)
  auto pager = [&](const Cols& C, const MenuLay& L, int first, int n) {
    if (n <= L.rows) return;
    const std::string cnt = std::to_string(first + 1) + "-" + std::to_string(std::min(n, first + L.rows)) + " OF " + std::to_string(n);
    if (touchUI) {
      const PageBtns pb = pageBtns(C, L);
      button(pb.upX, pb.y, pb.w, pb.h, "UP", false);
      button(pb.downX, pb.y, pb.w, pb.h, "DOWN", false);
      P.text(C.lx + C.lw / 2, pb.y + std::floor((pb.h - 7) / 2), cnt, 1, kDim, 1);
    } else {
      P.text(C.lx + C.lw / 2, Pix::H - 18, cnt, 1, kDim, 1);
    }
  };
  switch (menuTab_) {
    case 0: {   // items: the pack on the left, the picked item's detail beside it
      int n = (int)g.inv.size();
      menuSel_ = std::clamp(menuSel_, 0, std::max(0, n - 1));
      const MenuLay L = menuLay(touchUI);
      const Cols C = itemCols();
      int rows = L.rows;
      if (menuSel_ < menuScroll_) menuScroll_ = menuSel_;
      if (menuSel_ >= menuScroll_ + rows) menuScroll_ = menuSel_ - rows + 1;
      menuScroll_ = std::clamp(menuScroll_, 0, std::max(0, n - rows));
      for (int r = 0; r < rows && menuScroll_ + r < n; r++) {
        int i = menuScroll_ + r;
        const Item& it = g.inv[i];
        const float rowTop = top + 10 + r * L.pitch, y = rowTop + std::floor((L.pitch - 8) / 2);
        if (i == menuSel_) P.rect(C.lx, rowTop, C.lw, L.pitch - (touchUI ? 1 : 0), Color(0.3f, 0.22f, 0.12f, 0.8f));
        else if (touchUI && (r & 1)) P.rect(C.lx, rowTop, C.lw, L.pitch - 1, Color(0.16f, 0.12f, 0.08f, 0.35f));
        const float isz = touchUI ? 16.0f : 12.0f;
        P.blitEx(iconTex(it.icon, it.tint), 0, 0, 16, 16, C.lx + 2, rowTop + std::floor((L.pitch - isz) / 2), isz, isz);
        const float nx = C.lx + (touchUI ? 22.0f : 18.0f);
        std::string cnt = it.count > 1 ? " (" + std::to_string(it.count) + ")" : "";
        std::string name = fitText(it.name, C.lx + C.lw - 14 - nx - P.textW(cnt, 1)) + cnt;
        P.text(nx, y, name, 1, col(rarityColor(it.rarity)));
        if (equipped(g, i)) P.text(C.lx + C.lw - 6, y, "E", 1, kGold, 2);
      }
      if (n == 0) P.text(30, top + 14, "YOUR PACK IS EMPTY", 1, kDim);
      pager(C, L, menuScroll_, n);
      // detail
      P.rect(C.div, top + 8, 1, Pix::H - top - 24, Color(0.4f, 0.32f, 0.2f));
      if (n > 0) {
        const Item& it = g.inv[menuSel_];
        P.blitEx(iconTex(it.icon, it.tint), 0, 0, 16, 16, C.dx, top + 12, 32, 32);
        wrapText(C.dx + 38, top + 14, C.dw - 38, it.name, col(rarityColor(it.rarity)));
        const char* rn[] = {"COMMON", "UNCOMMON", "RARE", "EPIC", "LEGENDARY"};
        P.text(C.dx + 38, top + 36, rn[(int)it.rarity], 1, kDim);
        auto lines = wrap(itemStats(g, it), std::max(10, (int)(C.dw / 6)));
        float sy = top + 54;
        for (size_t k = 0; k < lines.size(); k++, sy += 10) P.text(C.dx, sy, lines[k], 1, kText);
        int eq = equippedOf(g, it.kind);
        if (eq >= 0 && eq != menuSel_ && eq < (int)g.inv.size()) {
          int diff = it.power - g.inv[eq].power;
          P.text(C.dx, std::max(top + 104, sy + 6), std::string("VS EQUIPPED: ") + (diff >= 0 ? "+" : "") + std::to_string(diff), 1, diff >= 0 ? Color(0.5f, 1, 0.5f) : Color(1, 0.5f, 0.45f));
        }
        bool canEquip = itemEquippable(it.kind);
        std::string use = canEquip ? (equipped(g, menuSel_) ? "UNEQUIP" : "EQUIP") : (it.kind == ItemKind::Potion || it.kind == ItemKind::Food || it.name.rfind("SPELL TOME", 0) == 0 ? "USE" : "");
        const float bw = std::floor((C.dw - 20) / 2);
        if (!use.empty()) button(C.dx, L.btnY, bw, L.btnH, use + (touchUI ? "" : " (ENT)"), true);
        if (it.kind != ItemKind::Quest) button(C.dx + bw + 8, L.btnY, bw, L.btnH, touchUI ? "DROP" : "DROP (X)", false);
      }
      break;
    }
    case 1: {   // quests: the journal on the left, the picked quest beside it
      std::vector<int> order;
      for (int i = 0; i < (int)g.quests.size(); i++) if (g.quests[i].state != QState::Done) order.push_back(i);
      for (int i = 0; i < (int)g.quests.size(); i++) if (g.quests[i].state == QState::Done) order.push_back(i);
      menuSel_ = std::clamp(menuSel_, 0, std::max(0, (int)order.size() - 1));
      const MenuLay L = menuLay(touchUI);
      const Cols C = questCols();
      if (menuSel_ < menuScroll_) menuScroll_ = menuSel_;
      if (menuSel_ >= menuScroll_ + L.rows) menuScroll_ = menuSel_ - L.rows + 1;
      menuScroll_ = std::clamp(menuScroll_, 0, std::max(0, (int)order.size() - L.rows));
      for (int r = 0; r < L.rows && menuScroll_ + r < (int)order.size(); r++) {
        const int oi = menuScroll_ + r;
        const Quest& q = g.quests[order[oi]];
        const float rowTop = top + 10 + r * L.pitch, y = rowTop + std::floor((L.pitch - 8) / 2);
        if (oi == menuSel_) P.rect(C.lx, rowTop, C.lw, L.pitch - (touchUI ? 1 : 0), Color(0.3f, 0.22f, 0.12f, 0.8f));
        else if (touchUI && (r & 1)) P.rect(C.lx, rowTop, C.lw, L.pitch - 1, Color(0.16f, 0.12f, 0.08f, 0.35f));
        Color c = q.state == QState::Done ? kDim : (q.type == QType::Main ? kGold : kText);
        P.text(C.lx + 4, y, (q.id == g.trackedQuest ? "> " : "  ") + fitText(q.title, C.lw - 20), 1, c);
      }
      pager(C, L, menuScroll_, (int)order.size());
      P.rect(C.div, top + 8, 1, Pix::H - top - 24, Color(0.4f, 0.32f, 0.2f));
      if (!order.empty()) {
        const Quest& q = g.quests[order[menuSel_]];
        float y = top + 12;
        const int per = std::max(10, (int)(C.dw / 6));
        for (const std::string& l : wrap(q.title, per)) { P.text(C.dx, y, l, 1, q.type == QType::Main ? kGold : kText); y += 10; }
        y += 2;
        std::string st = q.state == QState::Done ? "COMPLETED" : q.state == QState::Complete ? "READY TO TURN IN" : "IN PROGRESS";
        if (q.state != QState::Done) { std::string qs = g.questStatus(q); if (!qs.empty()) st = qs; }
        for (const std::string& l : wrap(st, per)) { P.text(C.dx, y, l, 1, q.state == QState::Complete ? Color(0.5f, 1, 0.5f) : kDim); y += 10; }
        y += 6;
        // the description fills the room down to the progress / reward lines (never under them)
        const bool prog = (q.type == QType::Hunt) || (q.type == QType::Main && q.stage == 1);
        const bool rew = q.gold > 0 && q.state != QState::Done;
        float footY = L.trackY - 8 - (rew ? 12.0f : 0.0f) - (prog ? 12.0f : 0.0f);
        const int maxLines = std::max(1, (int)((footY - y) / 10));
        auto dl = wrap(q.desc, per);
        for (int k = 0; k < (int)dl.size() && k < maxLines; k++) {
          std::string l = dl[(size_t)k];
          if (k == maxLines - 1 && (int)dl.size() > maxLines) l = fitText(l, C.dw - 18) + "...";
          P.text(C.dx, y + k * 10, l, 1, kText);
        }
        float fy = L.trackY - 8 - (rew ? 12.0f : 0.0f) - (prog ? 12.0f : 0.0f) + 2;
        if (q.type == QType::Hunt) { P.text(C.dx, fy, "PROGRESS " + std::to_string(q.have) + "/" + std::to_string(q.need), 1, kGold); fy += 12; }
        if (q.type == QType::Main && q.stage == 1) { P.text(C.dx, fy, "EMBER SHARDS " + std::to_string(q.have) + "/3", 1, kGold); fy += 12; }
        if (rew) P.text(C.dx, fy, "REWARD " + std::to_string(q.gold) + " GOLD", 1, kGold);
        if (q.state != QState::Done) button(C.dx, L.trackY, std::min(C.dw, touchUI ? 140.0f : 110.0f), L.trackH, q.id == g.trackedQuest ? "TRACKED" : "TRACK", q.id == g.trackedQuest);
      } else P.text(C.dx, top + 14, "NO QUESTS YET", 1, kDim);
      break;
    }
    case 2:   // map (worldmap.cpp, M2: the whole tab - the map, its side column, legend and travel)
      drawMapTab(g, top);
      break;
    case 3: {   // hero: the numbers in a card on the left, the hero on a lit stage on the right (the level-up choice under him)
      const Actor& p = g.pl();
      const HeroLay H = heroLay(touchUI, g.perkPts > 0);
      const float x = H.lx;
      float y = top + 12;
      P.text(x, y, "LEVEL " + std::to_string(g.plLevel), 2, kGold);
      y += 18;
      {   // the XP toward the next level as a bar
        const float bw = H.lw, k = clampf(g.plXp / (float)std::max(1, g.xpForNext()), 0, 1);
        P.rect(x, y, bw, 5, Color(0.05f, 0.04f, 0.06f, 0.9f));
        P.rect(x + 1, y + 1, (bw - 2) * k, 3, kGold);
        P.rect(x + 1, y + 1, (bw - 2) * k, 1, Color(1, 0.95f, 0.7f));
        P.text(x + bw, y + 8, "XP " + std::to_string(g.plXp) + " / " + std::to_string(g.xpForNext()), 1, kDim, 2);
        y += 22;
      }
      float mpMax = g.maxMp, stMax = g.maxSt;   // include enchantment bonuses, same as the HUD bars
      for (int idx : g.worn()) {
        if (idx < 0 || idx >= (int)g.inv.size()) continue;
        if (g.inv[idx].ench == Ench::Magicka) mpMax += g.inv[idx].enchPow;
        if (g.inv[idx].ench == Ench::Stamina) stMax += g.inv[idx].enchPow;
      }
      const float sp = Pix::H >= 285 ? 11.0f : 10.0f;
      auto stat = [&](const std::string& n, const std::string& v, Color c) { P.text(x, y, n, 1, kDim); P.text(x + H.lw, y, v, 1, c, 2); y += sp; };
      auto vital = [&](const std::string& n, float v, float mx, Color c) {
        stat(n, std::to_string((int)v) + " / " + std::to_string((int)mx), kText);
        const float k = clampf(v / std::max(1.0f, mx), 0, 1);
        P.rect(x, y - 2, H.lw, 2, Color(0.12f, 0.1f, 0.1f, 0.9f));
        P.rect(x, y - 2, H.lw * k, 2, c);
        y += 3;
      };
      vital("HEALTH", p.hp, p.maxHp, Color(0.85f, 0.18f, 0.16f));
      vital("MAGICKA", g.mp, mpMax, Color(0.25f, 0.45f, 0.95f));
      vital("STAMINA", g.stamina, stMax, Color(0.3f, 0.8f, 0.35f));
      y += 4;
      stat("WEAPON DAMAGE", std::to_string((int)g.weaponDamage()), kText);
      stat("ARMOR", std::to_string((int)g.armorRating()), kText);
      y += 4;
      stat("GOLD", std::to_string(g.gold), kGold);
      stat("KILLS", std::to_string(g.kills), kText);
      stat("DUNGEONS CLEARED", std::to_string(g.dungeonsCleared), kText);
      y += 4;
      std::string spells;
      for (int s = 0; s < (int)Spell::COUNT; s++) if (g.spellsKnown & (1 << s)) { if (!spells.empty()) spells += ", "; spells += spellName((Spell)s); }
      P.text(x, y, "SPELLS", 1, kDim);
      auto sl = wrap(spells.empty() ? "NONE" : spells, std::max(8, (int)((H.lw - 48) / 6)));
      for (size_t k = 0; k < sl.size() && y + k * 10 < Pix::H - 20; k++) P.text(x + H.lw, y + k * 10, sl[k], 1, Color(0.6f, 0.75f, 1.0f), 2);
      // the stage
      P.rect(H.sx, H.sy, H.sw, H.sh, Color(0.03f, 0.025f, 0.04f, 0.55f));
      P.frame(H.sx, H.sy, H.sw, H.sh, Color(0.3f, 0.24f, 0.15f));
      heroStage(P, light_, H.pcx, H.footY, H.scale / 5.0f);
      const Tex& t = humanTex(p.look);
      const int frame = 1 + ((int)(t_ * 5.0f) & 3);
      P.blitEx(t, frame * art::HUMAN_W, 0, art::HUMAN_W, art::HUMAN_H, H.pcx - art::HUMAN_W * H.scale / 2, H.footY - (art::HUMAN_H - 1) * H.scale,
               art::HUMAN_W * H.scale, art::HUMAN_H * H.scale);
      if (g.perkPts > 0) {
        P.textS(H.pcx, H.sy + 6, "LEVEL UP! CHOOSE A STAT", 1, kGold, 1);
        for (int i = 0; i < 3; i++) {
          static const char* lb[3] = {"+12 HEALTH", "+12 MAGICKA", "+12 STAMINA"};
          button(H.bx, H.by + i * (H.bh + 4), H.bw, H.bh, lb[i], levelSel_ == i);
        }
      } else {
        P.textS(H.pcx, H.footY + 10, g.app.name.empty() ? p.name : g.app.name, 1, kGold, 1);
        P.text(H.pcx, H.footY + 21, backgroundInfo(g.background).name, 1, kDim, 1);
      }
      break;
    }
    case 4: {   // system
      const float sh = menuLay(touchUI).sysH, pitch = sh + 6;
      const float bw = std::clamp(std::floor(Pix::W * 0.36f), 140.0f, 220.0f), bx = std::floor((Pix::W - bw) / 2);
      menuSel_ = std::clamp(menuSel_, 0, 3);
      const float by = sysTop(top, sh, touchUI);
      button(bx, by, bw, sh, "SAVE GAME", menuSel_ == 0);
      button(bx, by + pitch, bw, sh, "SETTINGS", menuSel_ == 1);
      button(bx, by + pitch * 2, bw, sh, touchUI ? "TOUCH CONTROLS: ON" : "TOUCH CONTROLS: OFF", menuSel_ == 2);
      button(bx, by + pitch * 3, bw, sh, "SAVE AND QUIT", menuSel_ == 3);
      const float ty = by + pitch * 4 + 8;
      P.text(Pix::W / 2, ty, "WORLD SEED " + std::to_string(g.seed), 1, kDim, 1);
      if (!touchUI) {
        const char* keys[] = {"WASD MOVE   J/SPACE ATTACK   K BOW   L SPELL", "SHIFT ROLL   E TALK/OPEN   Q POTION   R SWAP SPELL", "TAB MENU   M MAP   ESC PAUSE   F11 FULLSCREEN"};
        for (int i = 0; i < 3; i++) P.text(Pix::W / 2, ty + 16 + i * 11, keys[i], 1, kDim, 1);
      }
      break;
    }
    case 5:   // equip (paperdoll.cpp)
      drawPaperdoll(g, 8, 30, Pix::W - 16, Pix::H - 36);
      break;
  }
}

void View::drawDialogue(Game& g) {
  Pix& P = *pix_;
  if (settingsOpen_) return;
  const UiBox box = uiBox(300);
  P.pushBox(box.x, box.y, box.w, box.h);
  struct Pop { Pix& p; ~Pop() { p.popBox(); } } pop{P};
  const Dialogue& d = g.dlg;
  const float rowH = dlgRowH();
  const DlgLay L = dlgLay(g, rowH);
  const float x = L.x, w = L.w, y = L.y;
  int nOpt = (int)d.opts.size();
  panel(x, y, w, L.h, 0.94f);
  if (L.portrait) {
    // the speaker on a small lit stage, framed like the inventory's icons
    const float px = x + 8, py = y + 8, pw = 48, ph = std::min(58.0f, L.optTop.empty() ? L.h - 16 : L.optTop[0] - py - 6);
    P.rect(px, py, pw, ph, Color(0.03f, 0.025f, 0.04f, 0.9f));
    P.blitEx(light_, 0, 0, 64, 64, px - 8, py - 6, pw + 16, ph + 10, false, Color(1, 0.72f, 0.4f, 0.22f), 1);
    for (const Actor& a : g.actors)
      if (a.id == d.actor) {
        const Tex& t = humanTex(a.look);
        const float sc = 2.0f, hw = art::HUMAN_W * sc, hh = art::HUMAN_H * sc;
        P.blitEx(t, art::HUMAN_W, 0, art::HUMAN_W, art::HUMAN_H, px + (pw - hw) / 2, py + ph - hh - 2, hw, hh);
      }
    P.frame(px, py, pw, ph, Color(0.55f, 0.45f, 0.28f));
    P.frame(px + 1, py + 1, pw - 2, ph - 2, Color(0.22f, 0.18f, 0.12f));
  }
  P.text(L.tx, y + 8, d.speaker, 1, kGold);
  {   // the speech, typed out
    int shown = 0;
    const int maxLines = std::max(2, (int)((L.optTop.empty() ? L.h - 30 : L.optTop[0] - 10 - (y + 20)) / 9));
    for (size_t i = 0; i < L.lines.size() && (int)i < maxLines; i++) {
      if (shown >= (int)dlgChars_) break;
      std::string s = L.lines[i];
      if (shown + (int)s.size() > (int)dlgChars_) s = s.substr(0, (size_t)std::max(0, (int)dlgChars_ - shown));
      shown += (int)L.lines[i].size();
      P.text(L.tx, y + 20 + i * 9, s, 1, kText);
    }
  }
  dlgSel_ = std::clamp(dlgSel_, 0, std::max(0, nOpt - 1));
  // the options stay dimmed until the line has finished typing (a tap meanwhile only skips the text, see tap())
  const bool typing = dlgChars_ < L.total;
  P.rect(x + 8, L.optTop.empty() ? y : L.optTop[0] - 5, w - 16, 1, Color(0.4f, 0.32f, 0.2f, 0.6f));
  for (int i = 0; i < nOpt; i++) {
    const float top = L.optTop[(size_t)i], oh = L.optH[(size_t)i];
    const bool hot = i == dlgSel_ && !typing;
    Color tc = typing ? Color(0.45f, 0.42f, 0.37f) : hot ? Color(1, 0.95f, 0.8f) : Color(0.8f, 0.75f, 0.65f);
    if (typing) { if (touchUI) P.rect(x + 8, top + 1, w - 16, oh - 2, Color(0.12f, 0.09f, 0.06f, 0.4f)); }
    else if (hot) P.rect(x + 8, top + 1, w - 16, oh - 2, Color(0.32f, 0.24f, 0.12f, 0.85f));
    else if (touchUI) P.rect(x + 8, top + 1, w - 16, oh - 2, Color(0.16f, 0.12f, 0.08f, 0.55f));   // a full-width band
    const auto& ol = L.optLines[(size_t)i];
    const float ty = top + std::floor((oh - 7 - (ol.size() - 1) * 9.0f) / 2);
    for (size_t k = 0; k < ol.size(); k++) P.text(x + 26, ty + k * 9, ol[k], 1, tc);
    if (hot) P.text(x + 14, ty, ">", 1, kGold);
  }
}

// Shop layout, shared by drawShop and tap(): touch rows are finger-sized (24 px) and a tap only selects; the BUY / SELL
// button does the deal (a worn item asks twice). (M2) On a wide box (a phone's safe area) the picked item's detail
// and the deal button stand in a column beside the two lists; at 480 they sit in a strip under them.
namespace {
struct ShopLay {
  float pitch; int rows;
  bool side;                   // the detail column beside the lists
  float x0[2], lw;             // BUY and SELL list x, list width
  float dx, dw;                // detail column (side) or strip (centred, full width)
  float detailY, btnX, btnY, btnW, btnH, hintY, hintX;
  float xX, xY, xW, xH;        // close button
};
ShopLay shopLay(bool touch) {
  ShopLay L;
  const float W = (float)Pix::W, H = (float)Pix::H;
  L.pitch = touch ? 24.0f : 12.0f;
  L.side = W >= 560;
  L.xW = touch ? 28.0f : 20.0f; L.xH = touch ? 22.0f : 12.0f;
  L.xX = W - 16 - L.xW; L.xY = touch ? 9.0f : 10.0f;
  if (L.side) {
    L.dw = std::clamp(std::floor(W * 0.28f), 150.0f, 210.0f);
    L.dx = W - 18 - L.dw;
    const float listsW = L.dx - 12 - 18;
    L.lw = std::floor((listsW - 12) / 2);
    L.x0[0] = 18; L.x0[1] = 18 + L.lw + 12;
    L.btnW = L.dw; L.btnH = touch ? 26.0f : 18.0f; L.btnX = L.dx;
    L.btnY = H - 20 - L.btnH;
    L.detailY = 40;
    L.hintY = H - 18; L.hintX = 18 + listsW / 2;
    L.rows = std::max(4, (int)((H - 24 - 40) / L.pitch));
  } else {
    L.x0[0] = 18; L.x0[1] = W / 2 + 8; L.lw = W / 2 - 22;
    L.dx = 18; L.dw = W - 36;
    L.detailY = touch ? H - 62 : H - 30;
    L.btnW = 200; L.btnH = 22; L.btnX = W / 2 - 100; L.btnY = H - 50;
    L.hintY = touch ? H - 22 : H - 19; L.hintX = W / 2;
    L.rows = std::max(4, (int)((L.detailY - 6 - 40) / L.pitch));
  }
  return L;
}
int shopPrice(const Item& it, bool buying) {
  return buying ? (it.kind == ItemKind::Arrows ? it.count : it.value) : (it.kind == ItemKind::Arrows ? std::max(1, it.count / 3) : std::max(1, it.value * 2 / 5));
}
int shopScroll(int sel, int rows) { return sel >= rows ? sel - rows + 1 : 0; }
// the item rows of one list: on touch a list longer than the rows gives its last row to the page buttons
int shopRows(const ShopLay& L, bool touch, int n) { return touch && n > L.rows ? L.rows - 1 : L.rows; }
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
  P.blit(iconTex(art::Icon::Gold, 0), L.xX - 14 - P.textW(gs, 1) - 18, L.xY + L.xH / 2 - 8);
  P.text(L.xX - 14, L.xY + std::floor((L.xH - 7) / 2), gs, 1, kGold, 2);
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
      P.blitEx(iconTex(it.icon, it.tint), 0, 0, 16, 16, x0, y - 2 + std::floor((L.pitch - (touchUI ? 2 : 0) - isz) / 2), isz, isz);
      int price = shopPrice(it, buying);
      const std::string ps = std::to_string(price);
      std::string suf;
      if (it.count > 1 || it.kind == ItemKind::Arrows) suf = " x" + std::to_string(it.count);
      if (!buying && equipped(g, i)) suf += " (E)";
      const float nx = x0 + isz + 3, px = x0 + L.lw - 6;
      // (M2 fixer round 2) a long name is shortened by words before it is cut: "WOOL CLOAK OF THE MAGE" becomes
      // "WOOL CLOAK OF MAGE", then "CLOAK OF MAGE" (the cut "WOOL CLOAK OF THE (E)" hid what the item is)
      const float room = px - P.textW(ps, 1) - 8 - nx - P.textW(suf, 1);
      std::string full = it.name;
      if (P.textW(full, 1) > room) {
        const size_t ot = full.find(" OF THE ");
        if (ot != std::string::npos) full = full.substr(0, ot) + " OF " + full.substr(ot + 8);
      }
      if (P.textW(full, 1) > room) {
        const size_t of = full.find(" OF ");
        const size_t sp = full.find(' ');
        if (of != std::string::npos && sp != std::string::npos && sp < of) full = full.substr(sp + 1);
      }
      std::string nm = fitText(full, room) + suf;
      P.text(nx, ty, nm, 1, col(rarityColor(it.rarity)));
      bool afford = !buying || g.gold >= price;
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
  if (any) {
    const int price = shopPrice(src[shopSel_], shopSide_ == 0);
    deal = shopSide_ == 0 ? "BUY FOR " + std::to_string(price) + " GOLD" : armed ? "YOU WEAR IT - TAP AGAIN TO SELL" : "SELL FOR " + std::to_string(price) + " GOLD";
  }
  if (L.side) {
    // the detail column: the item large, its name, rarity, numbers and the deal
    if (any) {
      const Item& it = src[shopSel_];
      P.blitEx(iconTex(it.icon, it.tint), 0, 0, 16, 16, L.dx, L.detailY, 32, 32);
      float y = L.detailY + 2;
      for (const std::string& l : wrap(it.name, std::max(8, (int)((L.dw - 38) / 6)))) { P.text(L.dx + 38, y, l, 1, col(rarityColor(it.rarity))); y += 10; }
      const char* rn[] = {"COMMON", "UNCOMMON", "RARE", "EPIC", "LEGENDARY"};
      P.text(L.dx + 38, std::max(y + 2, L.detailY + 22), rn[(int)it.rarity], 1, kDim);
      y = std::max(y + 16, L.detailY + 42);
      for (const std::string& l : wrap(itemStats(g, it), std::max(10, (int)(L.dw / 6)))) { P.text(L.dx, y, l, 1, kText); y += 10; }
      if (shopSide_ == 0) {
        const int eq = equippedOf(g, it.kind);
        if (eq >= 0 && eq < (int)g.inv.size()) {
          const int diff = it.power - g.inv[eq].power;
          P.text(L.dx, y + 4, std::string("VS EQUIPPED: ") + (diff >= 0 ? "+" : "") + std::to_string(diff), 1, diff >= 0 ? Color(0.5f, 1, 0.5f) : Color(1, 0.5f, 0.45f));
        }
      }
      if (touchUI) {
        if (armed) { wrapText(L.dx, L.btnY - 22, L.dw, "YOU WEAR IT - TAP AGAIN TO SELL", kGold); button(L.btnX, L.btnY, L.btnW, L.btnH, "SELL ANYWAY", true); }
        else button(L.btnX, L.btnY, L.btnW, L.btnH, deal, true);
      } else {
        P.text(L.dx + L.dw / 2, L.btnY + 4, armed ? "ENTER AGAIN TO SELL" : (shopSide_ == 0 ? "ENTER TO BUY" : "ENTER TO SELL"), 1, armed ? kGold : kDim, 1);
      }
    } else P.text(L.dx, L.detailY + 4, "PICK AN ITEM", 1, kDim);
    P.text(L.hintX, L.hintY, touchUI ? "TAP AN ITEM, THEN THE BUTTON" : "ARROWS SELECT  LEFT/RIGHT SWITCH  ESC CLOSE", 1, kDim, 1);
    return;
  }
  // narrow: the detail line and the deal under the lists
  if (any) {
    std::string s = itemStats(g, src[shopSel_]);
    std::replace(s.begin(), s.end(), '\n', ' ');
    P.text(Pix::W / 2, L.detailY, fitText(s, Pix::W - 36), 1, kText, 1);
  }
  if (touchUI) {
    if (any) button(L.btnX, L.btnY, L.btnW, L.btnH, deal, true);
    P.text(Pix::W / 2, L.hintY, "TAP AN ITEM, THEN THE BUTTON TO BUY OR SELL", 1, kDim, 1);
  } else {
    P.text(Pix::W / 2, L.hintY, armed ? "YOU WEAR IT - PRESS ENTER AGAIN TO SELL" : "ARROWS SELECT  ENTER BUY/SELL  LEFT/RIGHT SWITCH  ESC CLOSE", 1,
           armed ? kGold : kDim, 1);
  }
}

void View::drawLevelUp(Game& g) { menuTab_ = 3; drawMenu(g); }

void View::drawTitle(Game& g, bool hasSave) {
  Pix& P = *pix_;
  (void)g;
  P.rect(0, 0, Pix::W, Pix::H, Color(0.02f, 0.02f, 0.05f, 0.35f));
  // embers rising over the whole screen
  for (int i = 0; i < 30 + Pix::W / 16; i++) {
    float ex = std::fmod(hashf(i, 0, 3) * Pix::W + std::sin(t_ + i) * 8 + Pix::W, (float)Pix::W);
    float ey = Pix::H - std::fmod(t_ * (12 + hashf(i, 1, 3) * 20) + hashf(i, 2, 3) * Pix::H, (float)Pix::H);
    P.rectAdd(ex, ey, 1, 1, Color(1, 0.5f, 0.2f, 0.6f));
  }
  if (settingsOpen_) return;   // the settings screen is drawn over the drifting world instead of the title
  const UiBox box = uiBox(270);
  P.pushBox(box.x, box.y, box.w, box.h);
  const float oy = std::floor((Pix::H - 270) / 2.0f);   // (M2) the box is the whole safe area: centre the 270-tall layout
  float y = 58 + oy;
  P.popBox();
  // soft dark bands behind the logo block and the foot line, so the words read over any village (screen-wide, eased)
  auto band = [&](float cy, float hh, float a) {
    for (int k = 0; k < (int)hh; k++) {
      const float d = std::fabs(k - hh / 2) / (hh / 2);
      P.rect(0, box.y + cy - hh / 2 + k, Pix::W, 1, Color(0.02f, 0.015f, 0.03f, a * (1 - d * d)));
    }
  };
  band(y + 26, 100, 0.55f);
  band((float)box.h - 11, 26, 0.6f);
  P.pushBox(box.x, box.y, box.w, box.h);
  // ember glow behind the logo
  P.blitEx(light_, 0, 0, 64, 64, Pix::W / 2 - 160, y - 50, 320, 120, false, Color(1, 0.45f, 0.15f, 0.35f + 0.1f * std::sin(t_ * 2)), 1);
  const std::string title = "EMBERVALE";
  for (int k = 3; k >= 1; k--) P.text(Pix::W / 2 + k, y + k, title, 5, Color(0.12f, 0.04f, 0.02f, 0.5f), 1);
  P.text(Pix::W / 2, y, title, 5, Color(1, 0.86f, 0.55f), 1);
  P.text(Pix::W / 2, y + 2, title, 5, Color(1, 0.7f, 0.35f, 0.35f), 1);
  P.textS(Pix::W / 2, y + 44, "A SAGA OF STEEL, SORCERY AND DRAGONFIRE", 1, Color(0.95f, 0.9f, 0.8f), 1);
  // CONTINUE (with a save), NEW ADVENTURE, SETTINGS: the same rows titleTap() and the keys use
  const int n = titleItems();
  titleSel_ = std::clamp(titleSel_, 0, n - 1);
  float by = (hasSave ? 138.0f : 150.0f) + oy;
  if (hasSave) { button(Pix::W / 2 - 70, by, 140, 22, "CONTINUE", titleSel_ == 0); by += 28; }
  button(Pix::W / 2 - 70, by, 140, 22, "NEW ADVENTURE", titleSel_ == (hasSave ? 1 : 0));
  by += 28;
  button(Pix::W / 2 - 50, by, 100, 20, "SETTINGS", titleSel_ == n - 1);
  if (!titleNote.empty()) P.textS(Pix::W / 2, by + 28, titleNote, 1, Color(1, 0.8f, 0.45f), 1);
  P.textS(Pix::W / 2, Pix::H - 14, "EVERY WORLD IS UNIQUE  -  PROCEDURALLY GENERATED", 1, Color(0.8f, 0.75f, 0.68f, 0.9f), 1);
  P.popBox();
}

void View::drawDead(Game& g) {
  Pix& P = *pix_;
  (void)g;
  P.rect(0, 0, Pix::W, Pix::H, Color(0.15f, 0, 0, 0.55f));
  const UiBox box = uiBox(270);
  P.pushBox(box.x, box.y, box.w, box.h);
  const float cy = std::floor(Pix::H / 2.0f);
  P.popBox();
  for (int k = 0; k < 90; k++) {   // a dark band behind the words, eased at its edges
    const float d = std::fabs(k - 45.0f) / 45.0f;
    P.rect(0, box.y + cy - 50 + k, Pix::W, 1, Color(0.06f, 0, 0, 0.6f * (1 - d * d)));
  }
  P.pushBox(box.x, box.y, box.w, box.h);
  P.text(Pix::W / 2 + 2, cy - 33, "YOU HAVE FALLEN", 3, Color(0, 0, 0, 0.6f), 1);
  P.text(Pix::W / 2, cy - 35, "YOU HAVE FALLEN", 3, Color(0.9f, 0.25f, 0.2f), 1);
  // the prompt (and input) waits a beat, so mashing attack as you fall doesn't skip the moment
  if (modeT_ > 1.2f) P.text(Pix::W / 2, cy + 5, touchUI ? "TAP TO RISE AGAIN" : "PRESS ENTER TO RISE AGAIN", 1, Color(kText.r, kText.g, kText.b, clampf((modeT_ - 1.2f) * 2, 0, 1)), 1);
  P.popBox();
}

// ------------------------------------------------------------------ input
void View::titleTap(Vec2 p) {
  if (settingsOpen_) { settingsTap(p); return; }
  const UiBox box = uiBox(270);
  p = inBox(box, p);
  if (std::fabs(p.x - box.w / 2.0f) > 80) return;
  // the rows drawTitle lays out: 28 px apart from 138 (with a save) or 150; each owns its whole pitch
  const float by = (hasSave_ ? 138.0f : 150.0f) + std::floor((box.h - 270) / 2.0f);
  const int row = (int)std::floor((p.y - (by - 3)) / 28.0f);
  if (row < 0 || row >= titleItems()) return;
  titleSel_ = row;
  if (row == titleItems() - 1) { openSettings(); audio_->play(Sfx::MenuSelect); }
  else if (hasSave_ && row == 0) wantContinue = true;
  else wantNewGame = true;
}

int View::buttonAt(Vec2 p) const {
  for (int b = 0; b < B_COUNT; b++) {
    const BtnDef d = btnPos(b);
    float r = d.r + 6;
    if (len2(p - Vec2(d.x, d.y)) < r * r) return b;
  }
  return -1;
}

Input View::input(Game& g) {
  Input in;
  const bool* ks = SDL_GetKeyboardState(nullptr);
  auto k = [&](SDL_Scancode sc) { return ks[sc] || scriptKeys_[sc]; };   // real keyboard or a test script's held key
  if (g.mode == Mode::Play) {
    if (k(SDL_SCANCODE_A) || k(SDL_SCANCODE_LEFT)) in.move.x -= 1;
    if (k(SDL_SCANCODE_D) || k(SDL_SCANCODE_RIGHT)) in.move.x += 1;
    if (k(SDL_SCANCODE_W) || k(SDL_SCANCODE_UP)) in.move.y -= 1;
    if (k(SDL_SCANCODE_S) || k(SDL_SCANCODE_DOWN)) in.move.y += 1;
    if (stick_.on) {
      Vec2 d = stick_.cur - stick_.start;
      float l = len(d);
      if (l > 4) in.move = d * (1.0f / l) * std::min(1.0f, l / 18.0f);
      if (l > 34) stick_.start += d * ((l - 34) / l);
    }
    // gamepad-like hold: attack repeats while held on keyboard
    if (k(SDL_SCANCODE_J) || k(SDL_SCANCODE_SPACE)) in.attack = true;
    // a finger held on the button swings again only while it reads ATTACK: when it shows a use verb (TALK, SLEEP,
    // OPEN, PRAY...) the tap is a use, and a held finger must not turn the next frame back into a swing
    for (auto& f : fingers_) {
      int hx = 0, hy = 0;
      if (f.on && f.button == B_ATTACK && g.interactTarget() < 0 && !usablePropAt(g, hx, hy)) in.attack = true;
    }
  }
  if (kAttack_) { in.attack = true; }
  in.bow = kBow_; in.spell = kSpell_; in.roll = kRoll_; in.interact = kUse_; in.potion = kPotion_; in.swapSpell = kSwap_;
  int utx = 0, uty = 0;
  // the attack key/button talks when someone is in reach; a tap on the world far from them stays an attack
  if (in.attack && g.mode == Mode::Play && kAttack_ && !kTapAttack_ && (g.interactTarget() >= 0 || usablePropAt(g, utx, uty))) { in.attack = false; in.interact = true; }
  kAttack_ = kBow_ = kSpell_ = kRoll_ = kUse_ = kPotion_ = kSwap_ = kTapAttack_ = false;
  return in;
}

void View::menuKey(Game& g, int key) {
  auto back = [&]() { g.mode = Mode::Play; audio_->play(Sfx::MenuBack); };
  if (g.mode == Mode::Menu || g.mode == Mode::LevelUp) {
    if (key == SDLK_ESCAPE || key == SDLK_TAB || key == SDLK_I) { back(); return; }
    // (M1) on the MAP tab Q / E zoom (worldMapKey); PageUp / PageDown and A / D still switch tabs there
    const bool mapQE = menuTab_ == 2 && g.mode == Mode::Menu && (key == SDLK_Q || key == SDLK_E);
    if (!mapQE && (key == SDLK_Q || key == SDLK_PAGEUP)) { menuTab_ = tabStep(menuTab_, -1); menuSel_ = 0; audio_->play(Sfx::MenuMove); return; }
    if (!mapQE && (key == SDLK_E || key == SDLK_PAGEDOWN)) { menuTab_ = tabStep(menuTab_, 1); menuSel_ = 0; audio_->play(Sfx::MenuMove); return; }
    if (menuTab_ == 5 && g.mode == Mode::Menu) { paperdollKey(g, key); return; }
    if (menuTab_ == 3 && g.perkPts > 0) {
      if (key == SDLK_UP || key == SDLK_W) levelSel_ = (levelSel_ + 2) % 3;
      if (key == SDLK_DOWN || key == SDLK_S) levelSel_ = (levelSel_ + 1) % 3;
      if (key == SDLK_RETURN || key == SDLK_SPACE) g.chooseLevelUp(levelSel_);
      return;
    }
    if (menuTab_ == 2 && mapTabKey(g, key)) return;   // worldmap.cpp
    if (key == SDLK_UP || key == SDLK_W) { menuSel_ = std::max(0, menuSel_ - 1); audio_->play(Sfx::MenuMove); }
    if (key == SDLK_DOWN || key == SDLK_S) { menuSel_++; audio_->play(Sfx::MenuMove); }
    if (key == SDLK_LEFT || key == SDLK_A) { menuTab_ = tabStep(menuTab_, -1); menuSel_ = 0; }
    if (key == SDLK_RIGHT || key == SDLK_D) { menuTab_ = tabStep(menuTab_, 1); menuSel_ = 0; }
    if (key == SDLK_RETURN || key == SDLK_SPACE) {
      if (menuTab_ == 0 && menuSel_ < (int)g.inv.size()) g.useItem(menuSel_);
      if (menuTab_ == 1) {
        std::vector<int> order;
        for (int i = 0; i < (int)g.quests.size(); i++) if (g.quests[i].state != QState::Done) order.push_back(i);
        if (menuSel_ < (int)order.size()) g.trackedQuest = g.quests[order[menuSel_]].id;
      }
      if (menuTab_ == 4) {
        if (menuSel_ == 0) { wantSave = true; banner_ = "GAME SAVED"; bannerSub_ = ""; bannerT_ = 2; }
        if (menuSel_ == 1) { openSettings(); audio_->play(Sfx::MenuSelect); }
        if (menuSel_ == 2) touchUI = !touchUI;
        if (menuSel_ == 3) { wantSave = true; wantsQuit = true; }
      }
    }
    if (key == SDLK_X && menuTab_ == 0 && menuSel_ < (int)g.inv.size() && g.inv[menuSel_].kind != ItemKind::Quest) g.dropItem(menuSel_);
    return;
  }
  if (g.mode == Mode::Dialogue) {
    if (key == SDLK_UP || key == SDLK_W) { dlgSel_ = std::max(0, dlgSel_ - 1); audio_->play(Sfx::MenuMove); }
    if (key == SDLK_DOWN || key == SDLK_S) { dlgSel_++; audio_->play(Sfx::MenuMove); }
    if (key == SDLK_RETURN || key == SDLK_SPACE || key == SDLK_E || key == SDLK_J) {
      int total = 0;
      {   // the same wrap as drawDialogue (its box)
        const UiBox box = uiBox(300);
        pix_->pushBox(box.x, box.y, box.w, box.h);
        total = dlgLay(g, dlgRowH()).total;
        pix_->popBox();
      }
      if (dlgChars_ < total) { dlgChars_ = 9999; return; }
      int sel = dlgSel_;
      std::string before = g.dlg.text;
      g.dialogueChoose(sel);
      if (g.dlg.text != before) dlgChars_ = 0;
      dlgSel_ = 0;
    }
    if (key == SDLK_ESCAPE) g.closeDialogue();
    return;
  }
  if (g.mode == Mode::Shop) {
    const int n = shopSide_ == 0 ? (int)g.shop.stock.size() : (int)g.inv.size();
    if (key == SDLK_UP || key == SDLK_W) { shopSel_ = std::max(0, shopSel_ - 1); shopArm_ = -1; }
    if (key == SDLK_DOWN || key == SDLK_S) { shopSel_ = std::min(std::max(0, n - 1), shopSel_ + 1); shopArm_ = -1; }
    if (key == SDLK_LEFT || key == SDLK_A || key == SDLK_RIGHT || key == SDLK_D) { shopSide_ ^= 1; shopSel_ = 0; shopArm_ = -1; }
    if (key == SDLK_RETURN || key == SDLK_SPACE) shopDeal(g);
    if (key == SDLK_ESCAPE || key == SDLK_TAB) { g.mode = Mode::Play; audio_->play(Sfx::MenuBack); }
  }
}

void View::tap(Game& g, Vec2 p) {
  auto inR = [&](float x, float y, float w, float h) { return p.x >= x && p.y >= y && p.x < x + w && p.y < y + h; };
  if (g.mode == Mode::Title) { titleTap(p); return; }
  if (settingsOpen_) { settingsTap(p); return; }
  if (g.mode == Mode::Creator) { creatorTap(g, p); return; }
  if (g.mode == Mode::Dead) { if (modeT_ > 1.2f) g.respawn(); return; }
  if (g.mode == Mode::Paused) { g.mode = Mode::Play; return; }
  // the dialogue, the shop and the menu are laid out in a 480-wide box (drawDialogue / drawShop / drawMenu): the tap
  // moves into it, and Pix::W / Pix::H read the box while the hit tests run
  Pix& P = *pix_;
  const UiBox box = modalBox(g);
  p = inBox(box, p);
  P.pushBox(box.x, box.y, box.w, box.h);
  struct Pop { Pix& p; ~Pop() { p.popBox(); } } pop{P};
  if (g.mode == Mode::Dialogue) {
    const DlgLay L = dlgLay(g, dlgRowH());
    const int nOpt = (int)g.dlg.opts.size();
    // while the line is still typing, a tap anywhere only finishes it (the common "tap to skip" gesture must never
    // pick an option the player has not read yet; the keyboard path in menuKey does the same)
    if (dlgChars_ < L.total) { dlgChars_ = 9999; return; }
    for (int i = 0; i < nOpt; i++) {
      // each option owns its whole row band, the full panel width; the last one reaches down to the screen edge
      const float top = L.optTop[(size_t)i];
      const float bh = i == nOpt - 1 ? 1000.0f : L.optH[(size_t)i];   // (M1: on past the box, to the screen's real edge)
      if (inR(-1000.0f, top, Pix::W + 2000.0f, bh)) {
        std::string before = g.dlg.text;
        g.dialogueChoose(i);
        if (g.dlg.text != before) dlgChars_ = 0;
        dlgSel_ = 0;
        return;
      }
    }
    // a tap above the panel closes a conversation that only has one way out (FAREWELL)
    if (nOpt == 1 && p.y < L.y) { g.dialogueChoose(0); dlgSel_ = 0; }
    return;
  }
  if (g.mode == Mode::Shop) {
    const ShopLay L = shopLay(touchUI);
    if (inR(L.xX - 4, 0, L.xW + 14, L.xY + L.xH + 6)) { g.mode = Mode::Play; return; }
    // the deal button (touch): a tap on a row only selects it
    if (touchUI && inR(L.btnX - 4, L.btnY - 3, L.btnW + 8, L.btnH + 6)) { shopDeal(g); return; }
    for (int side = 0; side < 2; side++) {
      float x0 = L.x0[side];
      int n = side == 0 ? (int)g.shop.stock.size() : (int)g.inv.size();
      int sel = shopSide_ == side ? shopSel_ : -1;
      const int rows = shopRows(L, touchUI, n);
      int scroll = shopScroll(sel, rows);
      if (rows < L.rows) {   // the page buttons: the selection moves a page (the list follows it)
        const float py = 40 + rows * L.pitch - 2;
        const bool up = inR(x0 - 4, py - 2, 50, L.pitch + 4), down = inR(x0 + L.lw - 50, py - 2, 50, L.pitch + 4);
        if (up || down) {
          const int base = shopSide_ == side ? shopSel_ : 0;
          shopSide_ = side; shopArm_ = -1;
          shopSel_ = up ? std::max(0, scroll - 1) : std::min(n - 1, std::max(base, scroll + rows - 1) + rows);
          audio_->play(Sfx::MenuMove);
          return;
        }
      }
      for (int r = 0; r < rows && scroll + r < n; r++) {
        float y = 40 + r * L.pitch;
        if (inR(x0 - 2, y - 2, L.lw, L.pitch)) {
          int i = scroll + r;
          if (!(shopSide_ == side && shopSel_ == i)) shopArm_ = -1;
          shopSide_ = side; shopSel_ = i;
          audio_->play(Sfx::MenuMove);
          return;
        }
      }
    }
    return;
  }
  if (g.mode == Mode::Menu || g.mode == Mode::LevelUp) {
    const TabLay T = tabLay(touchUI);
    // the close X first (on touch its tap box reaches a little past it, into the panel's corner)
    if (touchUI ? inR(T.xX - 2, 0, T.xW + 18, T.xY + T.xH + 6) : inR(T.xX, T.xY, T.xW, T.xH)) { g.mode = Mode::Play; return; }
    for (int i = 0; i < NTABS; i++)
      if (touchUI ? inR(T.x0 - 2 + i * T.tw, 0, T.tw, T.y + T.h + 2) : inR(T.x0 + i * T.tw, T.y, T.tw - 4, T.h)) {
        menuTab_ = kTabOrder[i]; menuSel_ = 0; menuScroll_ = 0; mapSel_ = -1; audio_->play(Sfx::MenuMove); return;
      }
    float top = 34;
    const MenuLay L = menuLay(touchUI);
    // a touch list's page buttons: a page up / down
    auto paged = [&](const Cols& C, int n) {
      if (!touchUI || n <= L.rows) return false;
      const PageBtns pb = pageBtns(C, L);
      if (inR(pb.upX, pb.y - 2, pb.w, pb.h + 4)) { menuSel_ = std::max(0, menuSel_ - L.rows); menuScroll_ = std::max(0, menuScroll_ - L.rows); audio_->play(Sfx::MenuMove); return true; }
      if (inR(pb.downX, pb.y - 2, pb.w, pb.h + 4)) {
        menuScroll_ = std::min(std::max(0, n - L.rows), menuScroll_ + L.rows);
        menuSel_ = std::min(n - 1, std::max(menuSel_ + L.rows, menuScroll_));
        audio_->play(Sfx::MenuMove);
        return true;
      }
      return false;
    };
    switch (menuTab_) {
      case 0: {
        int n = (int)g.inv.size();
        const Cols C = itemCols();
        for (int r = 0; r < L.rows && menuScroll_ + r < n; r++) {
          if (inR(C.lx, top + 10 + r * L.pitch, C.lw, L.pitch)) {
            int i = menuScroll_ + r;
            if (i == menuSel_) g.useItem(i);
            menuSel_ = i;
            return;
          }
        }
        if (paged(C, n)) return;
        const float bw = std::floor((C.dw - 20) / 2);
        if (n > 0 && inR(C.dx, L.btnY, bw, L.btnH)) g.useItem(menuSel_);
        if (n > 0 && inR(C.dx + bw + 8, L.btnY, bw, L.btnH) && g.inv[menuSel_].kind != ItemKind::Quest) g.dropItem(menuSel_);
        // keyboard / mouse: scroll by clicking the list edges
        if (!touchUI && inR(C.lx, top + 4, C.lw, 6)) menuScroll_ = std::max(0, menuScroll_ - 5), menuSel_ = std::max(0, menuSel_ - 5);
        if (!touchUI && inR(C.lx, top + 10 + L.rows * L.pitch, C.lw, 12)) menuSel_ = std::min(n - 1, menuSel_ + 5);
        break;
      }
      case 1: {
        std::vector<int> order;
        for (int i = 0; i < (int)g.quests.size(); i++) if (g.quests[i].state != QState::Done) order.push_back(i);
        for (int i = 0; i < (int)g.quests.size(); i++) if (g.quests[i].state == QState::Done) order.push_back(i);
        const Cols C = questCols();
        for (int r = 0; r < L.rows && menuScroll_ + r < (int)order.size(); r++)
          if (inR(C.lx, top + 10 + r * L.pitch, C.lw, L.pitch)) { menuSel_ = menuScroll_ + r; return; }
        if (paged(C, (int)order.size())) return;
        // keyboard / mouse: scroll the journal by clicking just above / below the list
        if (!touchUI && inR(C.lx, top + 2, C.lw, 8)) { menuSel_ = std::max(0, menuSel_ - 5); return; }
        if (!touchUI && inR(C.lx, top + 10 + L.rows * L.pitch, C.lw, 14)) { menuSel_ = std::min((int)order.size() - 1, menuSel_ + 5); return; }
        if (menuSel_ < (int)order.size() && inR(C.dx, L.trackY, std::min(C.dw, touchUI ? 140.0f : 110.0f), L.trackH)) g.trackedQuest = g.quests[order[menuSel_]].id;
        break;
      }
      case 2:
        mapTabTap(g, p, top);
        break;
      case 3:
        if (g.perkPts > 0) {
          const HeroLay H = heroLay(touchUI, true);
          for (int i = 0; i < 3; i++) if (inR(H.bx, H.by + i * (H.bh + 4) - 2, H.bw, H.bh + 4)) { g.chooseLevelUp(i); levelSel_ = i; }
        }
        break;
      case 5:
        paperdollTap(g, p);
        break;
      case 4: {
        const float sh = L.sysH, pitch = sh + 6;
        const float bw = std::clamp(std::floor(Pix::W * 0.36f), 140.0f, 220.0f), bx = std::floor((Pix::W - bw) / 2), by = sysTop(top, sh, touchUI);
        if (inR(bx, by, bw, sh)) { menuSel_ = 0; wantSave = true; banner_ = "GAME SAVED"; bannerSub_ = ""; bannerT_ = 2; }
        if (inR(bx, by + pitch, bw, sh)) { menuSel_ = 1; openSettings(); audio_->play(Sfx::MenuSelect); }
        if (inR(bx, by + pitch * 2, bw, sh)) { menuSel_ = 2; touchUI = !touchUI; }
        if (inR(bx, by + pitch * 3, bw, sh)) { wantSave = true; wantsQuit = true; }
        break;
      }
    }
    return;
  }
}

void View::event(const SDL_Event& e, Game& g) {
  auto logical = [&](float wx, float wy) { float lx, ly; pix_->windowToLogical(wx, wy, lx, ly); return Vec2(lx, ly); };
  // (M1) the MAP tab: drags pan and pinches zoom the world map; a press that does not move is a tap (picks a place)
  const bool onMap = g.mode == Mode::Menu && menuTab_ == 2 && !settingsOpen_;
  auto mapPtr = [&](int phase, uint64_t id, Vec2 screenP) {
    Pix& P = *pix_;
    const UiBox box = modalBox(g);
    P.pushBox(box.x, box.y, box.w, box.h);   // Pix::H as drawMenu sees it
    const bool tapped = worldMapPointer(phase, id, inBox(box, screenP));
    P.popBox();
    return tapped;
  };
  switch (e.type) {
    case SDL_EVENT_MOUSE_WHEEL:
      if (onMap && e.wheel.y != 0) worldMapZoom(e.wheel.y > 0 ? -1 : 1, inBox(modalBox(g), mouse_));
      break;
    case SDL_EVENT_MOUSE_BUTTON_UP:
      if (onMap && e.button.which != SDL_TOUCH_MOUSEID && e.button.button == SDL_BUTTON_LEFT && mapPtr(2, 1ull << 62, logical(e.button.x, e.button.y)))
        tap(g, logical(e.button.x, e.button.y));
      break;
    case SDL_EVENT_KEY_DOWN: {
      if (e.key.repeat && g.mode == Mode::Play) break;
      SDL_Keycode k = e.key.key;
      touchUI = false;
      if (settingsOpen_) { settingsKey((int)k); break; }
      if (g.mode == Mode::Title) {
        if (k == SDLK_UP || k == SDLK_W) titleSel_ = std::max(0, titleSel_ - 1);
        if (k == SDLK_DOWN || k == SDLK_S) titleSel_ = std::min(titleItems() - 1, titleSel_ + 1);
        if (k == SDLK_RETURN || k == SDLK_SPACE) {
          if (titleSel_ == titleItems() - 1) { openSettings(); audio_->play(Sfx::MenuSelect); }
          else if (hasSave_ && titleSel_ == 0) wantContinue = true;
          else wantNewGame = true;
        }
        if (k == SDLK_N) wantNewGame = true;
        if (k == SDLK_ESCAPE) wantsQuit = true;
        break;
      }
      if (g.mode == Mode::Dead) { if ((k == SDLK_RETURN || k == SDLK_SPACE || k == SDLK_E) && modeT_ > 1.2f) g.respawn(); break; }
      if (g.mode == Mode::Paused) { if (k == SDLK_ESCAPE || k == SDLK_RETURN || k == SDLK_SPACE) g.mode = Mode::Play; break; }
      if (g.mode == Mode::Creator) { creatorKey(g, (int)k); break; }
      if (g.mode != Mode::Play) { menuKey(g, k); break; }
      switch (k) {
        case SDLK_ESCAPE: g.mode = Mode::Paused; break;
        // (M2 fixer round 2) no menu on the road: it stopped the journey mid-gather (and its SYSTEM tab saved and quit
        // with the fare paid and the hero still at the start)
        case SDLK_TAB: case SDLK_I: if (g.travelling()) break; g.mode = Mode::Menu; menuTab_ = g.perkPts > 0 ? 3 : 0; menuSel_ = 0; audio_->play(Sfx::MenuSelect); break;
        case SDLK_M: if (g.travelling()) break; g.mode = Mode::Menu; menuTab_ = 2; mapSel_ = -1; audio_->play(Sfx::MenuSelect); break;
        case SDLK_J: case SDLK_SPACE: kAttack_ = true; break;
        case SDLK_K: kBow_ = true; break;
        case SDLK_L: kSpell_ = true; break;
        case SDLK_LSHIFT: case SDLK_RSHIFT: kRoll_ = true; break;
        case SDLK_E: case SDLK_RETURN: kUse_ = true; break;
        case SDLK_Q: kPotion_ = true; break;
        case SDLK_R: kSwap_ = true; break;
        default: break;
      }
      break;
    }
    case SDL_EVENT_MOUSE_MOTION:
      mouse_ = logical(e.motion.x, e.motion.y);
      if (onMap && e.motion.which != SDL_TOUCH_MOUSEID) mapPtr(1, 1ull << 62, mouse_);
      break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN: {
      if (e.button.which == SDL_TOUCH_MOUSEID) break;
      Vec2 p = logical(e.button.x, e.button.y);
      mouse_ = p;
      if (g.mode == Mode::Title) {
        titleTap(p);
        break;
      }
      if (g.mode == Mode::Play) {
        if (e.button.button == SDL_BUTTON_LEFT) kAttack_ = true;
        else if (e.button.button == SDL_BUTTON_RIGHT) kBow_ = true;
        break;
      }
      if (onMap && e.button.button == SDL_BUTTON_LEFT) {
        const UiBox box = modalBox(g);
        const Vec2 bp = inBox(box, p);
        // on the map itself: the press may become a drag (the tap is decided on release)
        if (bp.x >= 16 && bp.x < box.w - 164 && bp.y >= 40 && bp.y < box.h - 12) { mapPtr(0, 1ull << 62, p); break; }
      }
      tap(g, p);
      break;
    }
    case SDL_EVENT_FINGER_DOWN: {
      touchUI = true;
      int ww, wh;
      SDL_GetWindowSize(pix_->window(), &ww, &wh);
      Vec2 p = logical(e.tfinger.x * ww, e.tfinger.y * wh);
      if (g.mode == Mode::Title) {
        titleTap(p);
        break;
      }
      if (onMap) {
        const UiBox box = modalBox(g);
        const Vec2 bp = inBox(box, p);
        if (bp.x >= 16 && bp.x < box.w - 164 && bp.y >= 40 && bp.y < box.h - 12) { mapPtr(0, (uint64_t)e.tfinger.fingerID, p); break; }
      }
      if (g.mode != Mode::Play) { tap(g, p); break; }
      int b = buttonAt(p);
      Finger f; f.id = e.tfinger.fingerID; f.on = true; f.start = f.cur = p; f.button = b; f.t0 = SDL_GetTicks();
      if (b == B_MENU) { if (!g.travelling()) { g.mode = Mode::Menu; menuTab_ = g.perkPts > 0 ? 3 : 0; menuSel_ = 0; audio_->play(Sfx::MenuSelect); } break; }
      if (b == B_ATTACK) kAttack_ = true;
      else if (b == B_BOW) kBow_ = true;
      else if (b == B_SPELL) kSpell_ = true;
      else if (b == B_ROLL) kRoll_ = true;
      else if (b == B_POTION) kPotion_ = true;
      else if (p.x < Pix::W * 0.55f && !stick_.on) { stick_ = f; stick_.button = -2; }
      else if (b < 0) {
        // tap on the world: talk to / open what's there, otherwise attack. Talking needs the tap on (or near) the
        // person or the hero, so a tap across the screen never opens a conversation by accident
        int utx = 0, uty = 0;
        bool talk = false;
        if (int it = g.interactTarget(); it >= 0) {
          const Vec2 cam(std::floor(cam_.x), std::floor(cam_.y));
          for (const Actor& a : g.actors)
            if (a.id == it && (len2(p - (a.p - cam - Vec2(0, 10))) < 40.0f * 40.0f || len2(p - (g.pl().p - cam - Vec2(0, 10))) < 40.0f * 40.0f)) talk = true;
        }
        if (talk || usablePropAt(g, utx, uty)) kUse_ = true; else { kAttack_ = true; kTapAttack_ = true; }
      }
      fingers_.push_back(f);
      break;
    }
    case SDL_EVENT_FINGER_MOTION: {
      int ww, wh;
      SDL_GetWindowSize(pix_->window(), &ww, &wh);
      Vec2 p = logical(e.tfinger.x * ww, e.tfinger.y * wh);
      if (onMap) mapPtr(1, (uint64_t)e.tfinger.fingerID, p);
      if (stick_.on && stick_.id == e.tfinger.fingerID) stick_.cur = p;
      for (auto& f : fingers_) if (f.id == e.tfinger.fingerID) f.cur = p;
      break;
    }
    case SDL_EVENT_FINGER_UP: case SDL_EVENT_FINGER_CANCELED: {
      if (onMap) {
        int ww, wh;
        SDL_GetWindowSize(pix_->window(), &ww, &wh);
        const Vec2 p = logical(e.tfinger.x * ww, e.tfinger.y * wh);
        if (mapPtr(2, (uint64_t)e.tfinger.fingerID, p) && e.type == SDL_EVENT_FINGER_UP) tap(g, p);
      }
      if (stick_.on && stick_.id == e.tfinger.fingerID) {
        stick_.on = false;
        // M0b fix round 3: a short, still tap on the left half opens the stick, but when it lands on the "TAP: SLEEP"
        // label / the prop it names, or on the person in reach, it is a use: the label says tap here, so it must work
        if (e.type == SDL_EVENT_FINGER_UP && g.mode == Mode::Play && SDL_GetTicks() - stick_.t0 < 350 && len2(stick_.cur - stick_.start) < 8.0f * 8.0f) {
          const Vec2 cam(std::floor(cam_.x), std::floor(cam_.y));
          const Vec2 q = stick_.start;
          bool use = false;
          int utx = 0, uty = 0;
          if (usablePropAt(g, utx, uty)) {
            Vec2 s = Vec2(utx * 16 + 8.0f, uty * 16 - 4.0f) - cam;   // the prop's label sits at s.y - 10
            if (std::fabs(q.x - s.x) < 32 && q.y > s.y - 22 && q.y < s.y + 24) use = true;
          }
          if (int it = g.interactTarget(); it >= 0)
            for (const Actor& a : g.actors)
              if (a.id == it && len2(q - (a.p - cam - Vec2(0, 10))) < 24.0f * 24.0f) use = true;
          if (use) kUse_ = true;
        }
      }
      for (size_t i = 0; i < fingers_.size();) if (fingers_[i].id == e.tfinger.fingerID) fingers_.erase(fingers_.begin() + i); else i++;
      break;
    }
    default: break;
  }
}
