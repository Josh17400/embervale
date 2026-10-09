// The paper-doll equipment screen (the menu's EQUIP tab): a 4x turning preview of the hero with the 11 equipment
// slots around it, the selected slot's item and stats, and the pack's items for that slot with deltas against what
// is worn. M0 hero lane.
// Keys: Up/Down (W/S) pick a slot, Left/Right (A/D) pick an item for it, Enter equips / takes off the picked item,
// X or Backspace takes the slot's item off, [ and ] turn the preview. Touch: tap a slot, tap an item to wear it
// (tap a worn item to take it off), the arrows turn the preview and scroll the list.
#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unordered_set>
#include <vector>
#include "rpg/culture/culture.h"
#include "rpg/sim/craft.h"
#include "rpg/sim/gear.h"
#include "rpg/view/view.h"
#include "rpg/world/source.h"

void heroStage(Pix& P, const Tex& light, float cx, float footY, float size);   // creator.cpp
// M3: the personal arms on a shield (creator.cpp; a View::cachedTex key and its painter)
uint64_t creatorArmsKey(const cult::Heraldry& h, int size);
Canvas creatorPaintArms(uint64_t key);

namespace {
const Color kGoldC(0.98f, 0.82f, 0.42f), kTextC(0.93f, 0.9f, 0.82f), kDimC(0.62f, 0.58f, 0.52f);
const Color kUp(0.5f, 1, 0.5f), kDown(1, 0.5f, 0.45f);

struct Slot { ItemKind kind; const char* name; art::Icon ghost; int col, row; };
// left column: what is worn on the body; right column: what is held, and the ring
const Slot kSlots[11] = {
    {ItemKind::Helmet, "HEAD", art::Icon::Helmet, 0, 0},   {ItemKind::Amulet, "NECK", art::Icon::Amulet, 0, 1},
    {ItemKind::Cloak, "CLOAK", art::Icon::Cloak, 0, 2},    {ItemKind::Armor, "BODY", art::Icon::Armor, 0, 3},
    {ItemKind::Gloves, "HANDS", art::Icon::Gloves, 0, 4},  {ItemKind::Boots, "FEET", art::Icon::Boots, 0, 5},
    {ItemKind::Weapon, "WEAPON", art::Icon::Sword, 1, 0},  {ItemKind::Shield, "SHIELD", art::Icon::Shield, 1, 1},
    {ItemKind::Bow, "BOW", art::Icon::Bow, 1, 2},          {ItemKind::Staff, "STAFF", art::Icon::Staff, 1, 3},
    {ItemKind::Ring, "RING", art::Icon::Ring, 1, 4},
};
constexpr int kNSlots = 11;
int kListRows = 5;
constexpr float kBox = 28;
float kPitch = 33;

// ---- layout (box coordinates; at 480 x 270 the menu panel spans 8..472 x 6..264 and its tabs end at y 33). M2: the
// box is the whole safe area, so relayout() spreads the slot columns, widens the detail column and lets the slots and
// the stage grow with the height; at 480 x 270 it gives exactly the M0 layout the scripts tap.
float kColX[2] = {18, 168};
constexpr float kTopY = 36;
float kDollCX = 110, kDollFootY = 150, kStageH = 196;
float kDX = 206, kDW = 260;                           // detail column
constexpr float kListY = 112, kRowH = 24;
float kBtnY = 238;
void relayout(int W, int H) {
  const int ex = std::max(0, W - 480), ey = std::max(0, H - 270);
  kColX[1] = 168 + std::floor(ex * 0.2f);
  kDollCX = 110 + std::floor(ex * 0.1f);
  kPitch = std::min(40.0f, 33 + std::floor(ey / 6.0f));
  kStageH = 196 + (float)ey;
  kDollFootY = 150 + std::floor(ey * 0.6f);
  kDX = kColX[1] + 38;
  kDW = (float)W - 14 - kDX;
  kBtnY = (float)H - 32;
  kListRows = std::max(5, (int)((kBtnY - 8 - kListY) / kRowH));
}

struct PState { int slot = 3, cand = 0, scroll = 0, face = -1, armed = -1; float hold = 0; } S;   // armed: the row a tap picked

Color colOf(uint32_t c) { return Color((c & 255) / 255.0f, ((c >> 8) & 255) / 255.0f, ((c >> 16) & 255) / 255.0f); }
bool inR(Vec2 p, float x, float y, float w, float h) { return p.x >= x && p.y >= y && p.x < x + w && p.y < y + h; }
float slotX(int i) { return kColX[kSlots[i].col]; }
float slotY(int i) { return kTopY + kSlots[i].row * kPitch; }

int equippedIn(Game& g, ItemKind k) {
  int* e = g.equipSlot(k);
  return e && *e >= 0 && *e < (int)g.inv.size() ? *e : -1;
}
// (M6 fixer r3) why a legendary cannot be worn yet ("" when it can): attunement opens at level 10 (gear::attunementSlots);
// the EQUIP tab says so on the row and on its button, as the ITEMS tab does (hud.cpp)
std::string attuneBlock(Game& g, int idx, int eq) {
  if (idx < 0 || idx >= (int)g.inv.size() || idx == eq || !gear::attunes(g.inv[(size_t)idx])) return "";
  const int slots = gear::attunementSlots(g.plLevel);
  int worn = g.craft.live.stats.legendaries;
  if (eq >= 0 && eq < (int)g.inv.size() && gear::attunes(g.inv[(size_t)eq])) worn--;
  if (worn < slots) return "";
  return slots == 0 ? "FROM LEVEL 10" : "ATTUNED FULL";
}
std::vector<int> candidates(const Game& g, ItemKind k) {
  std::vector<int> v;
  for (int i = 0; i < (int)g.inv.size(); i++) if (g.inv[i].kind == k) v.push_back(i);
  return v;
}
// the headline number of an item and its unit
// (M6 fixer) a piece's damage or armour at the hero's level (gear::usePower: level sync), the number the doll's ARMOR
// and DAMAGE totals add up, so the whole is never shown as less than one of its parts
int headline(const Item& it, const char*& unit, int plLevel) {
  const int synced = (int)std::lround(gear::usePower(it, plLevel));
  switch (it.kind) {
    case ItemKind::Weapon: case ItemKind::Bow: unit = "DMG"; return synced;
    case ItemKind::Staff: unit = "SPELL"; return it.power * 2;
    case ItemKind::Ring: case ItemKind::Amulet: unit = it.ench == Ench::Fortify ? "ARM" : (it.ench == Ench::Health ? "HP" : it.ench == Ench::Magicka ? "MP" : "SP");
      return it.ench == Ench::Fortify ? it.enchPow / 2 : it.enchPow;
    default: unit = "ARM"; return synced;
  }
}
const char* unitName(const char* u) {
  if (!std::strcmp(u, "DMG")) return "DAMAGE";
  if (!std::strcmp(u, "ARM")) return "ARMOR";
  return u;
}
std::string enchLine(const Item& it) {
  switch (it.ench) {
    case Ench::Fire: return "+" + std::to_string(it.enchPow) + " FIRE DAMAGE";
    case Ench::Frost: return "+" + std::to_string(it.enchPow) + " FROST DAMAGE";
    case Ench::Drain: return "DRAINS " + std::to_string(it.enchPow / 2) + " HEALTH";
    case Ench::Health: return "+" + std::to_string(it.enchPow) + " MAX HEALTH";
    case Ench::Magicka: return "+" + std::to_string(it.enchPow) + " MAX MAGICKA";
    case Ench::Stamina: return "+" + std::to_string(it.enchPow) + " MAX STAMINA";
    case Ench::Fortify: return "+" + std::to_string(it.enchPow / 2) + " ARMOR";
    default: return "";
  }
}
// M6: "VETHMARKI VETHSTEEL", "QASRI BRONZE", "LEATHER": the maker culture's adjective and the material (an alloy by
// its own name); empty for things not made of anything (food, potions, classic pieces of no culture and no material)
std::string makerLine(const Game& g, const Item& it) {
  const cult::Culture* c = it.culture && g.world.src ? &g.world.src->culture(it.culture) : nullptr;
  std::string mat;
  if (it.mat == Mat::Alloy && c && it.alloy >= 1 && it.alloy <= c->arms.alloys.size()) mat = c->arms.alloys[(size_t)it.alloy - 1].name;
  else if (it.mat != Mat::None && it.mat != Mat::Alloy) mat = matName(it.mat);
  if (!c) return mat;
  return mat.empty() ? c->adjective + " MAKE" : c->adjective + " " + mat;
}
void clampCand(const Game& g) {
  int n = (int)candidates(g, kSlots[S.slot].kind).size();
  S.cand = std::clamp(S.cand, 0, std::max(0, n - 1));
  if (S.cand < S.scroll) S.scroll = S.cand;
  if (S.cand >= S.scroll + kListRows) S.scroll = S.cand - kListRows + 1;
  S.scroll = std::clamp(S.scroll, 0, std::max(0, n - kListRows));
}
// select a slot and put the cursor on what it wears
void pickSlot(Game& g, int i) {
  S.slot = (i + kNSlots) % kNSlots;
  std::vector<int> c = candidates(g, kSlots[S.slot].kind);
  int eq = equippedIn(g, kSlots[S.slot].kind);
  S.cand = 0;
  for (int k = 0; k < (int)c.size(); k++) if (c[k] == eq) S.cand = k;
  S.scroll = 0;
  S.armed = -1;
  clampCand(g);
}
// (M6 fixer) the doll's sheet: the hero's own sheet with every held piece (the weapon, the shield, the bow or staff on
// the back) parted from the body by a line of ink. At 4x the painter's pieces are all there, but a spear head in the
// armour's own alloy beside the helm, or a shield in that metal against the mail, melt into the figure; the ink line
// is what a hand-drawn doll would give them. One sheet is kept (rebaked when the look changes).
const Tex& dollTex(Pix& P, const art::HumanLook& L) {
  static uint64_t key = 0;
  static Tex tex;
  const uint64_t k = L.key() ^ 0xD0115EEDull;
  if (tex.t && key == k) return tex;
  Canvas full = art::humanSheet(L);
  art::HumanLook bare = L;
  bare.weapon = 0; bare.shield = false; bare.shieldStyle = 0; bare.shieldForm = 0; bare.backItem = 0;
  const bool held = bare.key() != L.key();
  if (held) {
    const Canvas body = art::humanSheet(bare);
    const int W = full.w, H = full.h;
    // a held piece's pixels: those in colours the bare figure never uses (the shaft's wood, the blade's ramp, the
    // shield's field and rim), where they differ from the bare figure (a pixel-wise diff alone also catches the
    // armour's pattern shifting round a piece)
    std::unordered_set<uint32_t> bodyCols;
    for (int y = 0; y < H; y++)
      for (int x = 0; x < W; x++) bodyCols.insert(body.get(x, y));
    std::vector<uint8_t> mask((size_t)W * H, 0);
    for (int y = 0; y < H; y++)
      for (int x = 0; x < W; x++) {
        const uint32_t v = full.get(x, y);
        if ((v >> 24) && v != body.get(x, y) && !bodyCols.count(v)) mask[(size_t)y * W + x] = 1;
      }
    const uint32_t ink = rgba(18, 14, 20);
    Canvas out = full;
    for (int y = 0; y < H; y++)
      for (int x = 0; x < W; x++) {
        // only the figure's own pixels, untouched by the piece (a shield's field is the piece, not the body)
        if (mask[(size_t)y * W + x] || !(full.get(x, y) >> 24) || full.get(x, y) != body.get(x, y)) continue;
        // a body pixel touching a held piece (in its own cell): the parting line
        const int cx0 = x / art::HUMAN_W * art::HUMAN_W, cy0 = y / art::HUMAN_H * art::HUMAN_H;
        bool touch = false;
        for (int d = 0; d < 4 && !touch; d++) {
          const int nx = x + (d == 0) - (d == 1), ny = y + (d == 2) - (d == 3);
          if (nx < cx0 || ny < cy0 || nx >= cx0 + art::HUMAN_W || ny >= cy0 + art::HUMAN_H) continue;
          touch = mask[(size_t)ny * W + nx] != 0;
        }
        if (touch) out.set(x, y, ink);
      }
    full = out;
  }
  if (tex.t) SDL_DestroyTexture(tex.t);
  tex = P.bake(full);
  key = k;
  return tex;
}
}  // namespace

void View::drawPaperdoll(Game& g, float x, float y, float w, float h) {
  (void)x; (void)y; (void)w; (void)h;
  Pix& P = *pix_;
  relayout(Pix::W, Pix::H);
  clampCand(g);
  S.hold = std::max(0.0f, S.hold - 1.0f / 60.0f);

  // ---- the doll: a lit stage, the hero at 4x turning slowly, his numbers below
  P.rect(kDollCX - 56, kTopY, 112, kStageH, Color(0.03f, 0.025f, 0.04f, 0.55f));
  P.frame(kDollCX - 56, kTopY, 112, kStageH, Color(0.3f, 0.24f, 0.15f));
  heroStage(P, light_, kDollCX, kDollFootY, 0.8f);
  static const int order[4] = {0, 2, 1, 3};
  int face = S.face >= 0 && S.hold > 0 ? S.face : order[(int)(t_ / 1.8f) & 3];
  int frame = 1 + ((int)(t_ * 5.0f) & 3);
  const Tex& t = dollTex(*pix_, g.pl().look);
  const float sc = 4;
  P.blitEx(t, frame * art::HUMAN_W, (face == 3 ? 2 : face) * art::HUMAN_H, art::HUMAN_W, art::HUMAN_H, kDollCX - art::HUMAN_W * sc / 2,
           kDollFootY - (art::HUMAN_H - 1) * sc, art::HUMAN_W * sc, art::HUMAN_H * sc, face == 3);
  button(kDollCX - 52, kDollFootY + 8, 28, 24, "<", false);
  button(kDollCX + 24, kDollFootY + 8, 28, 24, ">", false);
  P.text(kDollCX, kDollFootY + 16, "TURN", 1, kDimC, 1);
  P.text(kDollCX, kDollFootY + 42, "ARMOR " + std::to_string((int)g.armorRating()), 1, kTextC, 1);
  P.text(kDollCX, kDollFootY + 53, "DAMAGE " + std::to_string((int)g.weaponDamage()), 1, kTextC, 1);
  P.text(kDollCX, kDollFootY + 64, g.app.name, 1, kGoldC, 1);
  if (!g.app.heraldry.empty()) {   // M3: the hero's arms beside the name, and the people they are
    const Tex& arms = cachedTex(creatorArmsKey(g.app.heraldry, 16), creatorPaintArms);
    P.blitEx(arms, 0, 0, arms.w, arms.h, kDollCX + 34, kDollFootY + 36, 16, 16 * (float)arms.h / std::max(1, arms.w));
  }
  if (g.app.people) P.text(kDollCX, kDollFootY + 75, g.app.people == 2 ? "ELF" : "HALF-BREED", 1, kDimC, 1);

  // ---- the slots
  for (int i = 0; i < kNSlots; i++) {
    float sx = slotX(i), sy = slotY(i);
    bool sel = i == S.slot;
    int eq = equippedIn(g, kSlots[i].kind);
    P.rect(sx, sy, kBox, kBox, sel ? Color(0.3f, 0.22f, 0.12f, 0.95f) : Color(0.1f, 0.085f, 0.08f, 0.95f));
    P.frame(sx, sy, kBox, kBox, sel ? kGoldC : Color(0.42f, 0.34f, 0.22f));
    P.rect(sx + 1, sy + 1, kBox - 2, 1, Color(1, 1, 1, 0.06f));
    if (eq >= 0) {
      const Item& it = g.inv[eq];
      P.blitEx(itemTex(g, it), 0, 0, 16, 16, sx + 2, sy + 2, 24, 24);
      P.rect(sx + 2, sy + kBox - 3, kBox - 4, 1, colOf(rarityColor(it.rarity)));
      if (kSlots[i].kind == ItemKind::Shield && !g.app.heraldry.empty()) {   // M3: the shield bears the hero's arms
        const Tex& arms = cachedTex(creatorArmsKey(g.app.heraldry, 16), creatorPaintArms);
        P.blitEx(arms, 0, 0, arms.w, arms.h, sx + kBox - 13, sy + kBox - 15, 11, 11 * (float)arms.h / std::max(1, arms.w));
      }
    } else {
      P.blitEx(iconTex(kSlots[i].ghost, 0), 0, 0, 16, 16, sx + 6, sy + 6, 16, 16, false, Color(0.3f, 0.28f, 0.26f, 0.45f));
    }
  }

  // ---- detail column
  const Slot& sl = kSlots[S.slot];
  int eq = equippedIn(g, sl.kind);
  P.text(kDX, kTopY + 2, sl.name, 1, kGoldC);
  if (eq >= 0) {
    const Item& it = g.inv[eq];
    P.blitEx(itemTex(g, it), 0, 0, 16, 16, kDX, kTopY + 14, 32, 32);
    wrapText(kDX + 38, kTopY + 15, kDW - 40, it.name, colOf(rarityColor(it.rarity)));
    const char* unit = "";
    int v = headline(it, unit, g.plLevel);
    std::string line = it.kind == ItemKind::Staff ? "SPELL POWER +" + std::to_string(v) + "%" : std::string(unitName(unit)) + " " + std::to_string(v);
    // a piece above the hero's level is held back to it (level sync): its full number, at its own item level
    if (it.ilvl && it.kind != ItemKind::Staff && it.kind != ItemKind::Ring && it.kind != ItemKind::Amulet && v < it.power)
      line += "  (" + std::to_string(it.power) + " AT ILVL " + std::to_string(it.ilvl) + ")";
    std::string en = enchLine(it);
    if (it.kind == ItemKind::Ring || it.kind == ItemKind::Amulet) line = en, en = "";
    P.text(kDX + 38, kTopY + 37, line, 1, kTextC);
    if (!en.empty()) P.text(kDX + 38, kTopY + 47, en, 1, Color(0.6f, 0.75f, 1.0f));
    // M6: who made it and of what (the maker culture's word, the material or its alloy), as the HUD tooltip names them
    std::string made = makerLine(g, it);
    if (!made.empty()) {
      const int maxC = std::max(8, (int)((kDW - 40) / 6));
      if ((int)made.size() > maxC) made = made.substr(0, (size_t)maxC);
      P.text(kDX + 38, kTopY + (en.empty() ? 47 : 57), made, 1, Color(0.78f, 0.7f, 0.56f));
    }
  } else {
    P.text(kDX, kTopY + 20, "NOTHING WORN", 1, kDimC);
  }
  P.rect(kDX, kListY - 8, kDW, 1, Color(0.4f, 0.32f, 0.2f));
  std::vector<int> c = candidates(g, sl.kind);
  if (c.empty()) {
    P.text(kDX + 4, kListY + 8, "NONE IN YOUR PACK", 1, kDimC);
  } else {
    for (int r = 0; r < kListRows && S.scroll + r < (int)c.size(); r++) {
      int k = S.scroll + r, idx = c[k];
      const Item& it = g.inv[idx];
      float ry = kListY + r * kRowH;
      bool hot = k == S.cand, worn = idx == eq;
      P.rect(kDX, ry, kDW - 28, kRowH - 2, hot ? Color(0.3f, 0.22f, 0.12f, 0.85f) : Color(0.1f, 0.085f, 0.08f, 0.6f));
      if (hot) P.frame(kDX, ry, kDW - 28, kRowH - 2, kGoldC);
      P.blit(itemTex(g, it), kDX + 3, ry + 3);
      std::string nm = it.name;
      const int maxC = std::max(8, (int)((kDW - 28 - 22 - 34) / 6));
      if ((int)nm.size() > maxC) nm = nm.substr(0, (size_t)maxC);
      P.text(kDX + 22, ry + 8, nm, 1, colOf(rarityColor(it.rarity)));
      const char* unit = "";
      int v = headline(it, unit, g.plLevel);
      if (worn) P.text(kDX + kDW - 32, ry + 8, "WORN", 1, kGoldC, 2);
      else if (!attuneBlock(g, idx, eq).empty()) P.text(kDX + kDW - 32, ry + 8, "LV10", 1, kDown, 2);
      else {
        int d = eq >= 0 ? v - headline(g.inv[eq], unit, g.plLevel) : v;
        std::string ds = (d > 0 ? "+" : "") + std::to_string(d);
        P.text(kDX + kDW - 32, ry + 8, ds, 1, d > 0 ? kUp : (d < 0 ? kDown : kDimC), 2);
      }
    }
    if ((int)c.size() > kListRows) {
      button(kDX + kDW - 24, kListY, 24, 24, "-", S.scroll > 0);
      button(kDX + kDW - 24, kListY + (kListRows - 1) * kRowH, 24, 24, "+", S.scroll + kListRows < (int)c.size());
      P.text(kDX + kDW - 12, kListY + 52, std::to_string(S.cand + 1) + "/" + std::to_string(c.size()), 1, kDimC, 1);
    }
  }
  bool canAct = !c.empty();
  bool wearingHot = canAct && c[S.cand] == eq;
  const std::string blocked = canAct ? attuneBlock(g, c[S.cand], eq) : std::string();
  if (canAct && blocked.empty()) button(kDX, kBtnY, std::floor((kDW - 12) / 2), 24, std::string(wearingHot ? "TAKE OFF" : "EQUIP") + (touchUI ? "" : " (ENT)"), true);
  else if (canAct) button(kDX, kBtnY, std::floor((kDW - 12) / 2), 24, blocked, false);
  if (!blocked.empty()) P.text(kDX, kBtnY - 11, blocked == "ATTUNED FULL" ? "NO ATTUNEMENT SLOT FREE" : "ATTUNEMENT OPENS AT LEVEL 10", 1, kDown);
  // (M6 fixer r4) the worn piece selected: the main button already says TAKE OFF, so no second button doing the same
  if (eq >= 0 && !wearingHot) button(kDX + std::floor((kDW - 12) / 2) + 8, kBtnY, std::floor((kDW - 12) / 2), 24, touchUI ? "TAKE OFF" : "TAKE OFF (X)", false);
}

void View::paperdollKey(Game& g, int key) {
  clampCand(g);
  auto move = [&]() { audio_->play(Sfx::MenuMove); };
  std::vector<int> c = candidates(g, kSlots[S.slot].kind);
  if (key == SDLK_UP || key == SDLK_W) { pickSlot(g, S.slot - 1); move(); return; }
  if (key == SDLK_DOWN || key == SDLK_S) { pickSlot(g, S.slot + 1); move(); return; }
  if (key == SDLK_LEFT || key == SDLK_A) { if (!c.empty()) { S.cand = (S.cand + (int)c.size() - 1) % (int)c.size(); clampCand(g); move(); } return; }
  if (key == SDLK_RIGHT || key == SDLK_D) { if (!c.empty()) { S.cand = (S.cand + 1) % (int)c.size(); clampCand(g); move(); } return; }
  if (key == SDLK_RETURN || key == SDLK_SPACE || key == SDLK_KP_ENTER) {
    if (c.empty()) return;
    if (!attuneBlock(g, c[S.cand], equippedIn(g, kSlots[S.slot].kind)).empty()) { audio_->play(Sfx::MenuBack); return; }
    g.useItem(c[S.cand]);
    return;
  }
  if (key == SDLK_X || key == SDLK_BACKSPACE) { int eq = equippedIn(g, kSlots[S.slot].kind); if (eq >= 0) g.useItem(eq); return; }
  if (key == SDLK_LEFTBRACKET || key == SDLK_RIGHTBRACKET) {
    static const int ring[4] = {0, 2, 1, 3};
    int cur = S.face >= 0 && S.hold > 0 ? S.face : 0, at = 0;
    for (int i = 0; i < 4; i++) if (ring[i] == cur) at = i;
    S.face = ring[(at + (key == SDLK_RIGHTBRACKET ? 1 : 3)) % 4];
    S.hold = 5;
  }
}

void View::paperdollTap(Game& g, Vec2 p) {
  relayout(Pix::W, Pix::H);   // (tap() has pushed the menu's box)
  clampCand(g);
  for (int i = 0; i < kNSlots; i++)
    if (inR(p, slotX(i) - 2, slotY(i) - 2, kBox + 4, kBox + 4)) { pickSlot(g, i); audio_->play(Sfx::MenuMove); return; }
  if (inR(p, kDollCX - 52, kDollFootY + 8, 28, 24)) { paperdollKey(g, SDLK_LEFTBRACKET); return; }
  if (inR(p, kDollCX + 24, kDollFootY + 8, 28, 24)) { paperdollKey(g, SDLK_RIGHTBRACKET); return; }
  std::vector<int> c = candidates(g, kSlots[S.slot].kind);
  int eq = equippedIn(g, kSlots[S.slot].kind);
  if ((int)c.size() > kListRows) {
    if (inR(p, kDX + kDW - 24, kListY, 24, 24)) { S.scroll = std::max(0, S.scroll - 1); S.cand = std::max(S.scroll, std::min(S.cand, S.scroll + kListRows - 1)); return; }
    if (inR(p, kDX + kDW - 24, kListY + (kListRows - 1) * kRowH, 24, 24)) {
      S.scroll = std::min((int)c.size() - kListRows, S.scroll + 1);
      S.cand = std::max(S.scroll, std::min(S.cand, S.scroll + kListRows - 1));
      return;
    }
  }
  for (int r = 0; r < kListRows && S.scroll + r < (int)c.size(); r++)
    if (inR(p, kDX, kListY + r * kRowH, kDW - 28, kRowH)) {
      // (M6 fixer r3) a tap picks the row; a second tap on the picked row (or the EQUIP button) wears it, so the
      // tap-slot, tap-item, tap-EQUIP flow no longer takes the piece straight off again
      const bool again = S.armed == S.scroll + r;
      S.cand = S.armed = S.scroll + r;
      if (again && c[S.cand] != eq) { paperdollKey(g, SDLK_RETURN); S.armed = -1; }
      else audio_->play(Sfx::MenuMove);
      return;
    }
  if (!c.empty() && inR(p, kDX, kBtnY, std::floor((kDW - 12) / 2), 24)) { paperdollKey(g, SDLK_RETURN); S.armed = -1; return; }
  if (eq >= 0 && !(!c.empty() && c[S.cand] == eq) && inR(p, kDX + std::floor((kDW - 12) / 2) + 8, kBtnY, std::floor((kDW - 12) / 2), 24)) { g.useItem(eq); return; }
}
