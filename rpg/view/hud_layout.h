// hud_layout.h: the HUD layout and lookup helpers shared by drawing (hud.cpp, dialogue_ui.cpp, shop_ui.cpp,
// screens_ui.cpp) and tapping (input.cpp), so every touch rect has a single source. Private to those five files.
// Moved byte for byte from hud.cpp's anonymous namespaces (SOLID 1.4); here inline, in namespace hudlay.
#pragma once
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <string>
#include <vector>
#include "rpg/story/story.h"
#include "rpg/culture/culture.h"
#include "rpg/sim/craft.h"
#include "rpg/sim/gear.h"
#include "rpg/sim/home.h"
#include "rpg/sim/magic.h"
#include "rpg/sim/sea_ops.h"
#include "rpg/sim/skill_ops.h"
#include "rpg/sim/skills.h"
#include "rpg/view/realm_ui.h"
#include "rpg/view/skills_ui.h"
#include "rpg/view/view.h"
#include "rpg/world/poi.h"
#include "rpg/world/source.h"

int partyUseHere(const Game& g, int& idx);   // (M8) party_view.cpp: 1 a grave in reach, 2 a downed companion, 0 none

namespace hudlay {
// the use button's verb from the yard (M7 home::interactLabel) or else the sea (M10 SeaOps::interactLabel: BOARD,
// DISEMBARK, FISH...): each shares its interact's decision, so the prompt always says what the button does
inline std::string useVerbHere(const Game& g) {
  std::string v = home::interactLabel(g);
  return v.empty() ? SeaOps::interactLabel(g) : v;
}
inline const Color kGold(0.98f, 0.82f, 0.42f), kText(0.93f, 0.9f, 0.82f), kDim(0.62f, 0.58f, 0.52f), kPanel(0.07f, 0.06f, 0.08f);

// on-screen touch buttons
enum Btn { B_ATTACK, B_BOW, B_SPELL, B_ROLL, B_POTION, B_MENU, B_COUNT };
struct BtnDef { float x, y, r; };
// laid out on the 480 x 270 design screen; btnPos() anchors them to the safe area's bottom-right corner (the menu
// button: its top-right), so on a wide phone they stay under the right thumb and clear of the notch
inline const BtnDef kBtn[B_COUNT] = {{424, 222, 23}, {374, 240, 14}, {380, 196, 14}, {424, 172, 14}, {464, 176, 11}, {466, 12, 10}};
inline BtnDef btnPos(int b) {
  BtnDef d = kBtn[b];
  d.x = (float)(Pix::W - Pix::SR) - (480 - d.x);
  d.y = b == B_MENU ? (float)Pix::ST + d.y : (float)(Pix::H - Pix::SB) - (270 - d.y);
  return d;
}
// menuTab_ ids are stable (scripts and --menu use them); EQUIP (M0) is id 5 but shows second
// (M8) PARTY (id 6, View::kPartyTab) shows third, after EQUIP, while there is a party or a grave (g_partyTab: set by the
// menu's draw, keys and taps from View::partyTabShown)
// (M9) SKILLS (id 7, View::kSkillsTab) and SPELLS (id 8, View::kSpellsTab) follow EQUIP / PARTY: the hero's gear, then
// what the hero can do. MAP keeps the fifth place of the party-less strip (and QUESTS follows it), so a tap on the
// strip's middle still opens the map as it did on the phone before M9 (tools/scripts/m2_phone_touch.txt)
inline bool g_partyTab = false;
inline const int kTabOrder8[8] = {0, 5, 7, 8, 2, 1, 3, 4}, kTabOrder9[9] = {0, 5, 6, 7, 8, 2, 1, 3, 4};
inline int nTabs() { return g_partyTab ? 9 : 8; }
inline int tabAt(int i) { return g_partyTab ? kTabOrder9[i] : kTabOrder8[i]; }
inline int tabStep(int tab, int dir) {
  int at = 0;
  const int n = nTabs();
  for (int i = 0; i < n; i++) if (tabAt(i) == tab) at = i;
  return tabAt((at + dir + n) % n);
}

// The HUD's step line under the tracked quest title: the journal's "what to do next" (Game::questStatus), cut to
// the quest column's width.
// (fixer M4 r3) the journal opens on the tracked quest (else the newest open one), not on the first row: a quest just
// taken is the one the player wants to read
// (P1 Fresh Roads, JOBS lane) the QUESTS page's rows: the open quests, then the journal's LEADS (jobs.h: what was
// heard or seen that points at work not yet offered; open ones newest first, then the stale ones, dimmed; taken ones
// are gone: their quest is above), then the finished quests
struct JRow { int q = -1; int lead = -1; };
inline std::vector<JRow> journalRows(const Game& g) {
  std::vector<JRow> rows;
  for (int i = 0; i < (int)g.quests.size(); i++) if (g.quests[i].state != QState::Done) rows.push_back({i, -1});
  const auto& L = g.jobBook.leads;
  for (int pass = 0; pass < 2; pass++)
    for (int i = (int)L.size() - 1; i >= 0; i--)
      if (L[(size_t)i].state == (pass == 0 ? jobs::LS_OPEN : jobs::LS_STALE)) rows.push_back({-1, i});
  for (int i = 0; i < (int)g.quests.size(); i++) if (g.quests[i].state == QState::Done) rows.push_back({i, -1});
  return rows;
}
inline int journalRowOf(const Game& g) {
  const std::vector<JRow> rows = journalRows(g);
  int newest = -1;
  for (int r = 0; r < (int)rows.size(); r++) {
    if (rows[(size_t)r].q < 0) continue;
    const Quest& q = g.quests[(size_t)rows[(size_t)r].q];
    if (q.id == g.trackedQuest) return r;
    if (q.state != QState::Done) newest = r;
  }
  return std::max(0, newest);
}

// Menu layout, shared by drawMenu and tap(): on touch the ITEMS and QUESTS rows are finger-sized (24 px) with page
// buttons under the list, and the action buttons 24 px tall; keyboard/mouse keeps the dense 12 px list.
// (M2) every measure follows the box, which is the whole safe area: the rows follow its height, the columns its width.
struct MenuLay { float pitch; int rows; float btnY, btnH, trackY, trackH, sysH; };
inline MenuLay menuLay(bool touch) {
  if (touch) return {24.0f, std::max(5, (Pix::H - 96) / 24), (float)Pix::H - 46, 24.0f, (float)Pix::H - 46, 24.0f, 26.0f};
  return {12.0f, std::max(12, (Pix::H - 64) / 12), (float)Pix::H - 40, 16.0f, (float)Pix::H - 40, 16.0f, 20.0f};
}
// The list | detail columns of a menu tab: the list widens with the box by `share` of the extra width, the detail pane
// takes the rest (at 480: ITEMS 18+236 | 272..460, QUESTS 18+200 | 236..456)
struct Cols { float lx, lw, div, dx, dw; };
inline Cols menuCols(float baseW, float share) {
  Cols c;
  c.lx = 18;
  c.lw = std::floor(baseW + std::max(0, Pix::W - 480) * share);
  c.div = c.lx + c.lw + 8;
  c.dx = c.div + 10;
  c.dw = (float)Pix::W - 20 - c.dx;
  return c;
}
inline Cols itemCols() { return menuCols(236, 0.5f); }
inline Cols questCols() { return menuCols(200, 0.4f); }
// the page buttons under a touch list (UP at the left end, DOWN at the right end; the "1-8 OF 15" count between)
struct PageBtns { float upX, downX, y, w, h; };
inline PageBtns pageBtns(const Cols& c, const MenuLay& L) { return {c.lx, c.lx + c.lw - 52, L.btnY, 52, L.btnH}; }
// the HERO tab: a card of numbers on the left (lx, lw), a lit stage on the right with the hero and the level-up choice
struct HeroLay { float lx, lw, sx, sy, sw, sh, pcx, footY, scale, bx, by, bw, bh; };
inline HeroLay heroLay(bool touch, bool perk) {
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
inline float sysTop(float top, float sh, bool touch) {
  const float block = 4 * (sh + 6) + 16 + (touch ? 0.0f : 60.0f);
  return top + std::max(22.0f, std::floor((Pix::H - 8 - top - block) / 2));
}
// a string cut to fit w pixels of 5x7 text (a word boundary when one is near)
inline std::string fitText(const std::string& s, float w) {
  const int n = std::max(1, (int)((w + 1) / 6));
  if ((int)s.size() <= n) return s;
  std::string t = s.substr(0, (size_t)n);
  size_t sp = t.find_last_of(' ');
  if (sp != std::string::npos && (int)sp >= n - 6) t.resize(sp);
  return t;
}
// the menu's tab strip and close button: the tabs run from x 20 to the close button at the strip's right end
struct TabLay { float x0, tw, y, h, xX, xY, xW, xH; };
inline TabLay tabLay(bool touch) {
  TabLay t;
  t.y = touch ? 8.0f : 11.0f;
  t.h = touch ? 24.0f : 15.0f;
  t.xW = touch ? 28.0f : 20.0f;
  t.xH = t.h;
  t.xX = (float)Pix::W - 18 - t.xW;
  t.xY = t.y;
  t.x0 = 20;
  t.tw = (t.xX - 4 - t.x0) / (float)nTabs();
  return t;
}
inline std::vector<std::string> wrap(const std::string& s, int maxChars) {
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

// (M6 NUMBERS) an item's detail lines. A line may start with a colour tag: \x01 an affix (blue), \x02 the unique
// power (orange), \x03 a warning (level sync, attunement: amber), \x04 quiet (maker, lore: dim). statLines draws them.
inline std::string itemStats(const Game& g, const Item& it) {
  std::string s;
  const bool gearItem = itemEquippable(it.kind) && it.ilvl > 0;
  const cult::Culture* maker = it.culture && g.world.src ? &g.world.src->culture(it.culture) : nullptr;
  const int eff = gear::effLevel(it.ilvl, g.plLevel);
  if (gearItem) {
    s = gear::bandWord(gear::bandOf(it.ilvl), maker) + " - ITEM LEVEL " + std::to_string((int)it.ilvl);
    if (eff < it.ilvl) s += "\n\x03SYNCED TO " + std::to_string(eff) + " (YOUR LEVEL + 4)";
    s += "\n";
  }
  const int pw = (int)std::lround(gear::usePower(it, g.plLevel));
  switch (it.kind) {
    case ItemKind::Weapon: s += "DAMAGE " + std::to_string(pw); break;
    case ItemKind::Bow: s += "ARROW DAMAGE " + std::to_string(pw); break;
    case ItemKind::Staff: s += "SPELL POWER +" + std::to_string(pw * 2) + "%"; break;
    case ItemKind::Armor: case ItemKind::Helmet: case ItemKind::Shield: case ItemKind::Gloves: case ItemKind::Boots: case ItemKind::Cloak:
      s += "ARMOR " + std::to_string(pw); break;
    case ItemKind::Ring: case ItemKind::Amulet: if (!gearItem) s += "JEWELLERY"; break;
    case ItemKind::Potion: s += "RESTORES " + std::to_string(it.power) + "\n\x04SHARED COOLDOWN 8 S\n\x04" "CARRY 10 AT MOST"; break;
    // (fixer M5 r3, review: "nothing tells the player that food cures Hungry") food is what ends Hungry (15.2)
    case ItemKind::Food: s += "HEALS " + std::to_string(it.power) + "\nENDS HUNGER: WELL FED"; break;
    case ItemKind::Arrows: s += "AMMUNITION"; break;
    case ItemKind::Quest: s += "QUEST ITEM"; break;
    case ItemKind::Material: s += "CRAFTING MATERIAL\n\x04" "FOR THE SMELTER, FORGE OR TANNERY"; break;
    default:
      if (craft::isLore(it)) s += "RUIN LORE\n\x04USE IT TO STUDY THE SECRET";
      break;
  }
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
  // (M8b) every affix in plain words (items.h affixText: "+12% DAMAGE VS BEASTS", "BURNS FOES FOR 6 DMG/S")
  for (const ItemAffix& a : it.affix) {
    if (a.kind == Affix::None) continue;
    s += "\n\x01" + affixText(a.kind, gear::affixUse(it, a.kind, g.plLevel));
  }
  if (it.unique != Unique::None) s += "\n\x02" + std::string(uniqueInfo(it.unique).name) + ": " + uniqueInfo(it.unique).line;
  if (gearItem) {
    std::string who = maker ? maker->adjective + " WORK" : std::string("HEARTLAND WORK");
    const std::string mw = gear::matWord(it, maker);
    if (!mw.empty()) who += ", " + mw;
    else if (it.mat != Mat::None) who += std::string(", ") + matName(it.mat);
    s += "\n\x04" + who;
    if (it.flags & IF_CRAFTED) s += "\n\x04" "CRAFTED";
  }
  if (it.rarity >= Rarity::Legendary && gearItem) {
    s += "\n\x04" + gear::legendaryLore(it, maker);
    const int slots = gear::attunementSlots(g.plLevel);
    // (M6 fixer r2, review: "'0/0' reads like a bug") below level 10 say plainly when attunement opens
    if (slots == 0) s += "\n\x03" "ATTUNEMENT OPENS AT LEVEL 10";
    else s += "\n\x03" "ATTUNED " + std::to_string(g.craft.live.stats.legendaries) + "/" + std::to_string(slots);
  }
  if (it.kind != ItemKind::Quest) s += "\nVALUE " + std::to_string(it.value);
  return s;
}
// the colour a detail line's tag asks for (and the line without it)
inline Color statColor(std::string& l, Color base) {
  if (l.empty()) return base;
  switch (l[0]) {
    case '\x01': l.erase(0, 1); return Color(0.55f, 0.78f, 1.0f);
    case '\x02': l.erase(0, 1); return Color(1.0f, 0.68f, 0.24f);
    case '\x03': l.erase(0, 1); return Color(0.98f, 0.78f, 0.36f);
    case '\x04': l.erase(0, 1); return Color(0.62f, 0.58f, 0.52f);
    default: return base;
  }
}
// the detail lines wrapped to `chars`, each keeping its colour: x, y the first line; stops at maxY. Returns the next y.
inline float statLines(Pix& P, const Game& g, const Item& it, float x, float y, int chars, float maxY, Color base) {
  std::string all = itemStats(g, it), line;
  std::vector<std::string> raw;
  for (char ch : all) { if (ch == '\n') { raw.push_back(line); line.clear(); } else line += ch; }
  raw.push_back(line);
  for (std::string l : raw) {
    const Color c = statColor(l, base);
    for (const std::string& w : wrap(l, chars)) {
      if (y + 8 > maxY) return y;
      P.text(x, y, w, 1, c);
      y += 10;
    }
  }
  return y;
}
// the plain one-line form (the narrow shop strip): tags removed
inline std::string statsPlain(const Game& g, const Item& it) {
  std::string s = itemStats(g, it), o;
  for (char ch : s) { if (ch == '\n') o += "  "; else if ((unsigned char)ch >= 32) o += ch; }
  return o;
}
// a usable prop (chest, shrine, bed...) the attack button may "use" instead: only when no enemy is close,
// so mashing attack beside a shrine in a fight still swings the sword
inline int usablePropAt(const Game& g, int& tx, int& ty) {
  if (g.mode == Mode::Play && g.foeInReach()) return 0;   // (fixer M4 r3) mid-fight the button swings
  int pr = g.interactProp(tx, ty);
  if (!pr) pr = story::wantedProp(g, tx, ty);   // (P1b) a beacon to light, a crate to search: a story asked for it
  if (!pr) return 0;
  // (M0b fix round 2: another guest's bed, or the innkeeper's, stays usable: the button reads LOOK and a tap explains
  // whose it is and that the innkeeper lets rooms, instead of swinging a weapon in a room full of people)
  for (size_t i = 1; i < g.actors.size(); i++) {
    const Actor& a = g.actors[i];
    if (a.hostile && a.st != AState::Dead && len2(a.p - g.pl().p) < 70 * 70) return 0;
  }
  return pr;
}
inline bool equipped(const Game& g, int i) {
  return i == g.eqWeapon || i == g.eqBow || i == g.eqStaff || i == g.eqArmor || i == g.eqHelmet || i == g.eqShield || i == g.eqRing || i == g.eqAmulet ||
         i == g.eqGloves || i == g.eqBoots || i == g.eqCloak;
}

// The dialogue panel's layout, shared by drawDialogue, tap() and menuKey (M2: from the box's real width; the panel
// grows with the wrapped line, so a long speech is never cut, and an option too long for one row wraps onto two).
// The speaker's portrait stands at the panel's left when the box is wide enough (a phone's safe area).
struct DlgLay {
  float x, w, y, h;
  bool portrait;
  float tx, tw;              // the speech's column
  int per;                   // characters per speech line
  std::vector<std::string> lines;
  int total = 0;             // characters to type
  std::vector<float> optTop, optH;   // per option (all of them); an option off this page has optH 0
  std::vector<std::vector<std::string>> optLines;
  // (M5) a long option list pages: options [first, first + count) show, and a MORE row under them turns the page
  int first = 0, count = 0;
  bool more = false;
  float moreTop = 0, moreH = 0;
  float optsTop = 0;         // the top of the option block (the divider sits just above it)
};
// The first option of the page that holds option `sel`, for a list paged from option 0 (pages are greedy, so the
// keyboard and a tap on MORE walk the same pages).
// (M7b) the options on show (Dialogue::shows: the first page, or the option group opened), as indices into dlg.opts
inline std::vector<int> dlgShown(const Dialogue& d) {
  std::vector<int> v;
  for (size_t i = 0; i < d.opts.size(); i++) if (d.shows(d.opts[i])) v.push_back((int)i);
  if (v.empty()) for (size_t i = 0; i < d.opts.size(); i++) v.push_back((int)i);   // (never a panel without a way out)
  return v;
}
inline DlgLay dlgLay(const Game& g, float rowH, int first = 0) {
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
  const std::vector<int> shown = dlgShown(d);
  for (int oi : shown) {
    const DlgOpt& o = d.opts[(size_t)oi];
    L.optLines.push_back(wrap(o.label, optPer));
    if (L.optLines.back().empty()) L.optLines.back().push_back("");
    const float h = rowH + (float)(L.optLines.back().size() - 1) * 9;
    L.optH.push_back(h);
    oh += h;
  }
  const int n = (int)shown.size();
  L.optTop.assign((size_t)n, -1e6f);
  // the room the options may take: the panel stays on screen (8 px margins) with its title and two speech lines
  const float minText = 18.0f;
  float avail = (float)Pix::H - 16 - 22 - minText - 10 - 6;
  // (fixer M7b r2, review: "the phone's 7-option inn panel covers 85 % of the screen and hides the gold") with the
  // finger-sized rows the options take at most about half the screen; the rest page behind MORE
  if (rowH >= 20.0f) avail = std::min(avail, std::floor((float)Pix::H * 0.5f));
  float shownH = oh;
  if (oh <= avail) { L.first = 0; L.count = n; }
  else {
    L.more = true; L.moreH = rowH;
    L.first = std::clamp(first, 0, std::max(0, n - 1));
    float used = 0; int c = 0;
    for (int i = L.first; i < n; i++) {
      if (c > 0 && used + L.optH[(size_t)i] + L.moreH > avail) break;
      used += L.optH[(size_t)i]; c++;
    }
    L.count = c;
    shownH = used + L.moreH;
  }
  // the speech: at least two lines' room (four with a portrait), and never more than the box allows
  float textH = std::max((float)L.lines.size(), L.portrait ? 4.0f : 2.0f) * 9;
  const float maxText = (float)Pix::H - 16 - 22 - 10 - 6 - shownH;
  textH = std::min(textH, std::max(minText, maxText));
  L.h = 22 + textH + 10 + shownH + 6;
  L.y = std::max(8.0f, (float)Pix::H - L.h - 8);
  float top = L.y + 22 + textH + 10;
  L.optsTop = top;
  for (int i = 0; i < n; i++) {
    if (i < L.first || i >= L.first + L.count) { L.optH[(size_t)i] = 0; continue; }
    L.optTop[(size_t)i] = top; top += L.optH[(size_t)i];
  }
  if (L.more) L.moreTop = top;
  return L;
}
// The first option of the page holding `sel`: walk the pages exactly as dlgLay builds them.
inline int dlgPageFirst(const Game& g, float rowH, int sel) {
  int first = 0;
  for (int guard = 0; guard < 64; guard++) {
    const DlgLay L = dlgLay(g, rowH, first);
    if (!L.more || sel < L.first + L.count || L.count <= 0) return L.first;
    first = L.first + L.count;
  }
  return first;
}

// Shop layout, shared by drawShop and tap(): touch rows are finger-sized (24 px) and a tap only selects; the BUY / SELL
// button does the deal (a worn item asks twice). (M2) On a wide box (a phone's safe area) the picked item's detail
// and the deal button stand in a column beside the two lists; at 480 they sit in a strip under them.
struct ShopLay {
  float pitch; int rows;
  bool side;                   // the detail column beside the lists
  float x0[2], lw;             // BUY and SELL list x, list width
  float dx, dw;                // detail column (side) or strip (centred, full width)
  float detailY, btnX, btnY, btnW, btnH, hintY, hintX;
  float xX, xY, xW, xH;        // close button
};
inline ShopLay shopLay(bool touch) {
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
inline int shopScroll(int sel, int rows) { return sel >= rows ? sel - rows + 1 : 0; }
// the item rows of one list: on touch a list longer than the rows gives its last row to the page buttons
inline int shopRows(const ShopLay& L, bool touch, int n) { return touch && n > L.rows ? L.rows - 1 : L.rows; }
}  // namespace hudlay
