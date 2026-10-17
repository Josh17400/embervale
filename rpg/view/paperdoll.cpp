// The paper-doll equipment screen (the menu's EQUIP tab): a 4x turning preview of the hero with the 11 equipment
// slots around it, the selected slot's item and stats, and the pack's items for that slot with deltas against what
// is worn. M0 hero lane.
// Keys: Up/Down (W/S) pick a slot, Left/Right (A/D) pick an item for it, Enter equips / takes off the picked item,
// X or Backspace takes the slot's item off, [ and ] turn the preview. Touch: tap a slot, tap an item to wear it
// (tap a worn item to take it off), the arrows turn the preview and scroll the list.
#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <map>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>
#include "rpg/culture/culture.h"
#include "rpg/sim/craft.h"
#include "rpg/sim/gear.h"
#include "rpg/sim/gear_look.h"
#include "rpg/view/view.h"
#include "rpg/view/rise_view.h"
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

// armed: the row a tap picked; (M8b) peek: a pack piece was picked by hand (a key or a tap), so its card and the compare
// against the worn piece float over the doll until the slot changes
struct PState { int slot = 3, cand = 0, scroll = 0, face = -1, armed = -1; float hold = 0; bool peek = false; } S;

std::vector<std::string> wrapD(const std::string& s, int maxChars);   // (below, with the PARTY page)
std::string fitTextD(const std::string& s, float w);
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
  S.peek = false;
  clampCand(g);
}
// (M8b fixer r2, review: "a worn Legendary's gold frame looks like the selection highlight") the picked slot's cue is
// never a rarity's colour: white corner brackets three pixels outside the slot (clear of a Mythic's ember rim and
// flame fringe), breathing, each with a dark drop shadow so they read over any frame
void selCursor(Pix& P, float t, float x, float y, float w, float h) {
  const float a = 0.72f + 0.28f * std::sin(t * 5.0f);
  const Color c(1.0f, 1.0f, 1.0f, a), sh(0.02f, 0.01f, 0.03f, 0.8f);
  const float x0 = x - 3, y0 = y - 3, x1 = x + w + 2, y1 = y + h + 2, L = 6;
  for (int pass = 0; pass < 2; pass++) {
    const float o = pass == 0 ? 1.0f : 0.0f;
    const Color k = pass == 0 ? sh : c;
    P.rect(x0 + o, y0 + o, L, 1, k); P.rect(x0 + o, y0 + o, 1, L, k);
    P.rect(x1 - L + 1 + o, y0 + o, L, 1, k); P.rect(x1 + o, y0 + o, 1, L, k);
    P.rect(x0 + o, y1 + o, L, 1, k); P.rect(x0 + o, y1 - L + 1 + o, 1, L, k);
    P.rect(x1 - L + 1 + o, y1 + o, L, 1, k); P.rect(x1 + o, y1 - L + 1 + o, 1, L, k);
  }
}
// (M6 fixer) the doll's sheet: the hero's own sheet with every held piece (the weapon, the shield, the bow or staff on
// the back) parted from the body by a line of ink. At 4x the painter's pieces are all there, but a spear head in the
// armour's own alloy beside the helm, or a shield in that metal against the mail, melt into the figure; the ink line
// is what a hand-drawn doll would give them. One sheet is kept (rebaked when the look changes).
// (M8b fixer r2/r3, review: "the Mythic aura is a ring of loose orange squares") the doll's sheet is baked without the
// Mythic glint; the flame is the world's own aura cell (View::mythicAuraTex) scaled with the figure, behind it.
const Tex& dollTex(Pix& P, const art::HumanLook& Lw) {
  static uint64_t key = 0;
  static Tex tex;
  const uint64_t k = Lw.key() ^ 0xD0115EEDull;
  if (tex.t && key == k) return tex;
  const bool mythic = Lw.glow && Lw.glowColor == kMythicGlow;
  art::HumanLook L = Lw;
  if (mythic) { L.glow = 0; L.glowColor = 0; }
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
  art::HumanLook heroLook = g.pl().look;
  RiseViewOps::dressHero(g, heroLook);   // (M12) the sworn realm's tabard and the rank's mark (render_rise.cpp)
  const Tex& t = dollTex(*pix_, heroLook);
  // (M8b fixer r3, review: "the portrait is blown up to ~10 screen px per sprite pixel and reads as muddy noise") 3x:
  // nearer the icons' scale beside it, the figure's pixels stay readable, with a drop shadow under the feet
  const float sc = 3;
  P.rect(std::floor(kDollCX - 13), kDollFootY, 26, 2, Color(0.02f, 0.01f, 0.03f, 0.55f));
  P.rect(std::floor(kDollCX - 9), kDollFootY + 2, 18, 1, Color(0.02f, 0.01f, 0.03f, 0.35f));
  mythicAuraAt(g.pl().look, frame, face == 3 ? 2 : face, face == 3, kDollCX - art::HUMAN_W * sc / 2, kDollFootY - (art::HUMAN_H - 1) * sc, sc);
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
  // (M8b fixer r2, review: "ember licks show through above the item card") the compare card covers the doll's side:
  // a covered slot's burning frame (its flame fringe rises above the slot, past the card's top edge) is not drawn
  const bool cardOpen = S.peek && !candidates(g, kSlots[S.slot].kind).empty();
  for (int i = 0; i < kNSlots; i++) {
    float sx = slotX(i), sy = slotY(i);
    bool sel = i == S.slot;
    int eq = equippedIn(g, kSlots[i].kind);
    if (cardOpen && sx + kBox <= kDX - 10) continue;   // both slot columns sit under the card
    P.rect(sx, sy, kBox, kBox, sel ? Color(0.3f, 0.22f, 0.12f, 0.95f) : Color(0.1f, 0.085f, 0.08f, 0.95f));
    // (M8b) the slot's frame is the worn piece's rarity (a Mythic's burns); an empty slot keeps the quiet frame
    if (eq >= 0) rarityFrame(sx, sy, kBox, kBox, g.inv[eq].rarity, false);
    else P.frame(sx, sy, kBox, kBox, Color(0.42f, 0.34f, 0.22f));
    P.rect(sx + 1, sy + 1, kBox - 2, 1, Color(1, 1, 1, 0.06f));
    if (sel) selCursor(P, t_, sx, sy, kBox, kBox);
    if (eq >= 0) {
      const Item& it = g.inv[eq];
      P.blitEx(itemTex(g, it), 0, 0, 16, 16, sx + 2, sy + 2, 24, 24);
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
    P.rect(kDX - 1, kTopY + 13, 34, 34, Color(0.04f, 0.03f, 0.05f, 0.6f));
    rarityFrame(kDX - 1, kTopY + 13, 34, 34, it.rarity);
    P.blitEx(itemTex(g, it), 0, 0, 16, 16, kDX, kTopY + 14, 32, 32);
    {   // (M8b) the name in its ladder colour (a Mythic's in the flame), wrapped to two lines, the rarity after it
      const auto nl = wrapD(it.name, std::max(8, (int)((kDW - 40) / 6)));
      for (size_t q = 0; q < nl.size() && q < 2; q++) rarityText(kDX + 38, kTopY + 15 + (float)q * 9, nl[q], it.rarity);
      if (nl.size() < 2) {   // RARITY  ITEM LEVEL N (15.27.3: the item level shows wherever the piece is described)
        float rx = kDX + 38;
        {   // (M8b fixer r2) COMMON too, as the item card says it
          rarityText(rx, kTopY + 24, rarityName(it.rarity), it.rarity, 0.85f);
          rx += (float)P.textW(rarityName(it.rarity), 1) + 6;
        }
        if (it.ilvl) P.text(rx, kTopY + 24, "ITEM LEVEL " + std::to_string((int)it.ilvl), 1, kDimC);
      }
    }
    const char* unit = "";
    int v = headline(it, unit, g.plLevel);
    std::string line = it.kind == ItemKind::Staff ? "SPELL POWER +" + std::to_string(v) + "%" : std::string(unitName(unit)) + " " + std::to_string(v);
    // a piece above the hero's level is held back to it (level sync): its full number, at its own item level
    if (it.ilvl && it.kind != ItemKind::Staff && it.kind != ItemKind::Ring && it.kind != ItemKind::Amulet && v < it.power)
      line += "  (" + std::to_string(it.power) + " AT ILVL " + std::to_string(it.ilvl) + ")";
    std::string en = enchLine(it);
    if (it.kind == ItemKind::Ring || it.kind == ItemKind::Amulet) line = en, en = "";
    if (line.empty() && it.affixCount()) line = affixText(it.affix[0].kind, gear::affixUse(it, it.affix[0].kind, g.plLevel));
    P.text(kDX + 38, kTopY + 37, line, 1, line.rfind("ARMOR", 0) == 0 || line.rfind("DAMAGE", 0) == 0 || line.rfind("SPELL", 0) == 0 ? kTextC : Color(0.62f, 0.80f, 1.0f));
    if (!en.empty()) P.text(kDX + 38, kTopY + 47, en, 1, Color(0.6f, 0.75f, 1.0f));
    // (M8b) its affixes and power in plain words, as many as the two lines hold; the rest on its card (pick the row)
    const int maxC = std::max(8, (int)((kDW - 40) / 6));
    std::vector<std::string> extra;
    for (const ItemAffix& a : it.affix)
      if (a.kind != Affix::None && affixText(a.kind, gear::affixUse(it, a.kind, g.plLevel)) != line)
        extra.push_back(affixText(a.kind, gear::affixUse(it, a.kind, g.plLevel)));
    if (it.unique != Unique::None) extra.push_back(std::string(uniqueInfo(it.unique).name));
    if (!extra.empty() && en.empty()) {
      P.text(kDX + 38, kTopY + 47, extra[0].substr(0, (size_t)maxC), 1, extra.size() == 1 && it.unique != Unique::None ? Color(1.0f, 0.70f, 0.28f) : Color(0.62f, 0.80f, 1.0f));
      if (extra.size() == 2) P.text(kDX + 38, kTopY + 57, extra[1].substr(0, (size_t)maxC), 1, it.unique != Unique::None ? Color(1.0f, 0.70f, 0.28f) : Color(0.62f, 0.80f, 1.0f));
      else if (extra.size() > 2) P.text(kDX + 38, kTopY + 57, fitTextD("+" + std::to_string(extra.size() - 1) + " MORE: PICK ITS ROW", (float)maxC * 6), 1, kDimC);
    } else {
      // M6: who made it and of what (the maker culture's word, the material or its alloy), as the HUD tooltip names them
      std::string made = makerLine(g, it);
      if (!made.empty()) {
        if ((int)made.size() > maxC) made = made.substr(0, (size_t)maxC);
        P.text(kDX + 38, kTopY + (en.empty() ? 47 : 57), made, 1, Color(0.78f, 0.7f, 0.56f));
      }
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
      rarityIcon(g, it, kDX + 3, ry + 3, 16);   // (M8b) in its rarity's cell
      std::string nm = it.name;
      const int maxC = std::max(8, (int)((kDW - 28 - 22 - 40) / 6));
      if ((int)nm.size() > maxC) nm = nm.substr(0, (size_t)maxC);
      rarityText(kDX + 22, ry + 8, nm, it.rarity);
      const char* unit = "";
      int v = headline(it, unit, g.plLevel);
      if (worn) P.text(kDX + kDW - 32, ry + 8, "WORN", 1, kGoldC, 2);
      else if (!attuneBlock(g, idx, eq).empty()) P.text(kDX + kDW - 32, ry + 8, "LV10", 1, kDown, 2);
      else {
        // (M8b) the headline's change as a number and the overall verdict (gear::powerScore) as a pixel arrow
        int d = eq >= 0 ? v - headline(g.inv[eq], unit, g.plLevel) : v;
        std::string ds = (d > 0 ? "+" : "") + std::to_string(d);
        P.text(kDX + kDW - 40, ry + 8, ds, 1, d > 0 ? kUp : (d < 0 ? kDown : kDimC), 2);
        cmpArrow(kDX + kDW - 38, ry + 8, cmpVerdict(g, it, eq >= 0 ? &g.inv[(size_t)eq] : nullptr));
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
  // (M8b) a pack piece picked by hand: its card and the compare against the worn piece (the worn piece: its card, WORN),
  // over the doll's side of the screen (the list and the buttons stay in view; a tap on a slot under it picks that
  // slot, which closes it; a tap on the card puts it away)
  if (S.peek && canAct) {
    const float px = 12, py = kTopY - 2, pw = kDX - 10 - px, ph = (float)Pix::H - 10 - py;
    P.rect(px, py, pw, ph, Color(0.05f, 0.04f, 0.06f, 1.0f));
    P.frame(px, py, pw, ph, Color(0.55f, 0.45f, 0.28f));
    P.rect(px + 1, py + 1, pw - 2, 1, Color(1, 1, 1, 0.05f));
    // a notch pointing at the picked row
    const float ny = std::clamp(kListY + (float)(S.cand - S.scroll) * kRowH + 6, py + 6, py + ph - 14);
    for (int k = 0; k < 4; k++) P.rect(px + pw + (float)k, ny + (float)k, 1, 9 - 2.0f * (float)k, Color(0.55f, 0.45f, 0.28f));
    itemCard(g, g.inv[(size_t)c[S.cand]], px + 7, py + 7, pw - 14, py + ph - 5, true, eq >= 0 && !wearingHot ? &g.inv[(size_t)eq] : nullptr, wearingHot);
  }
}

// (M8b scripts) the slot and pack row picked by hand, as the arrow keys do: the compare card shows
void View::paperdollPeek(Game& g, int slot, int cand) {
  relayout(Pix::W, Pix::H);
  pickSlot(g, slot);
  const int n = (int)candidates(g, kSlots[S.slot].kind).size();
  S.cand = std::clamp(cand, 0, std::max(0, n - 1));
  S.peek = true;
  clampCand(g);
}

void View::paperdollKey(Game& g, int key) {
  clampCand(g);
  auto move = [&]() { audio_->play(Sfx::MenuMove); };
  std::vector<int> c = candidates(g, kSlots[S.slot].kind);
  if (key == SDLK_UP || key == SDLK_W) { pickSlot(g, S.slot - 1); move(); return; }
  if (key == SDLK_DOWN || key == SDLK_S) { pickSlot(g, S.slot + 1); move(); return; }
  if (key == SDLK_LEFT || key == SDLK_A) { if (!c.empty()) { S.cand = (S.cand + (int)c.size() - 1) % (int)c.size(); S.peek = true; clampCand(g); move(); } return; }
  if (key == SDLK_RIGHT || key == SDLK_D) { if (!c.empty()) { S.cand = (S.cand + 1) % (int)c.size(); S.peek = true; clampCand(g); move(); } return; }
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
  // (M8b) a tap on the compare card (not on a slot under it) puts it away
  if (S.peek && inR(p, 12, kTopY - 2, kDX - 22, (float)Pix::H - 10 - (kTopY - 2))) {
    const std::vector<int> cc = candidates(g, kSlots[S.slot].kind);
    if (!cc.empty()) { S.peek = false; audio_->play(Sfx::MenuMove); return; }
  }
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
      S.peek = true;
      if (again && c[S.cand] != eq) { paperdollKey(g, SDLK_RETURN); S.armed = -1; }
      else audio_->play(Sfx::MenuMove);
      return;
    }
  if (!c.empty() && inR(p, kDX, kBtnY, std::floor((kDW - 12) / 2), 24)) { paperdollKey(g, SDLK_RETURN); S.armed = -1; return; }
  if (eq >= 0 && !(!c.empty() && c[S.cand] == eq) && inR(p, kDX + std::floor((kDW - 12) / 2) + 8, kBtnY, std::floor((kDW - 12) / 2), 24)) { g.useItem(eq); return; }
}

// ================================================================ M8 Fellowship: the PARTY page (VIEW lane)
// The menu's PARTY tab (View::kPartyTab), in the EQUIP tab's style. A row of the companions' faces at the top (tap one;
// [ and ] on the keyboard), REMEMBERED at its right end (N). The page: the companion turning slowly on a lit stage in
// their own look, every worn piece in its maker culture's style (Game::partyLook, the human sheet), their eleven slots
// round it (party::Slot) and SUPPLIES (potions and food to keep). Beside it STORY (name, vocation, origin, approval and
// bond with the SKILL_BOND marks, the three bond skills with SkillInfo::what, party::aboutLine, days together, kills)
// or, once a slot is picked, GEAR: what they wear there (TAKE BACK: Game::partyUnequip) and the pack's pieces for it
// (a tap hands one over and they put it on: Game::partyEquip; supplies: Game::partyGive). REMEMBERED: the fallen
// (Party::fallen) with their cause, day, epitaph and where the grave lies.
// Keys: [ ] the companion, W / S a slot (STORY above the first), A / D the pack's row, Enter gives, X takes back,
// N REMEMBERED (W / S its rows there).
namespace {
std::vector<std::string> wrapD(const std::string& s, int maxChars) {
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
    else word += c;
  }
  flush();
  if (!line.empty()) lines.push_back(line);
  return lines;
}
std::string fitTextD(const std::string& s, float w) {
  const int n = std::max(1, (int)((w + 1) / 6));
  if ((int)s.size() <= n) return s;
  std::string t = s.substr(0, (size_t)n);
  const size_t sp = t.find_last_of(' ');
  if (sp != std::string::npos && (int)sp >= n - 6) t.resize(sp);
  return t;
}
// the companion's slots round the figure: the body on the left, what is held (and the supplies) on the right
struct PSlot { int slot; ItemKind kind; const char* name; art::Icon ghost; int col, row; };
constexpr int kSupplies = party::SLOTS;   // the pseudo-slot of potions and food
const PSlot kPSlots[12] = {
    {party::S_HELMET, ItemKind::Helmet, "HEAD", art::Icon::Helmet, 0, 0},   {party::S_AMULET, ItemKind::Amulet, "NECK", art::Icon::Amulet, 0, 1},
    {party::S_CLOAK, ItemKind::Cloak, "CLOAK", art::Icon::Cloak, 0, 2},     {party::S_ARMOR, ItemKind::Armor, "BODY", art::Icon::Armor, 0, 3},
    {party::S_GLOVES, ItemKind::Gloves, "HANDS", art::Icon::Gloves, 0, 4},  {party::S_BOOTS, ItemKind::Boots, "FEET", art::Icon::Boots, 0, 5},
    {party::S_WEAPON, ItemKind::Weapon, "WEAPON", art::Icon::Sword, 1, 0},  {party::S_SHIELD, ItemKind::Shield, "SHIELD", art::Icon::Shield, 1, 1},
    {party::S_BOW, ItemKind::Bow, "BOW", art::Icon::Bow, 1, 2},             {party::S_STAFF, ItemKind::Staff, "STAFF", art::Icon::Staff, 1, 3},
    {party::S_RING, ItemKind::Ring, "RING", art::Icon::Ring, 1, 4},         {kSupplies, ItemKind::Potion, "SUPPLIES", art::Icon::PotionRed, 1, 5},
};
constexpr int kNPSlots = 12;
struct PartyLay {
  float chipY, chipS, chipPitch, remX, remW;
  float slotX[2], slotTop, box, pitch, stageX, stageW, stageY, stageH, figCX, figFootY, figScale;
  float dX, dW, dTop, btnY, rowH;
};
PartyLay partyLay(int W, int H, bool touch) {
  PartyLay L;
  L.chipY = 38; L.chipS = 26; L.chipPitch = 30;
  L.remW = 92; L.remX = (float)W - 18 - L.remW;
  const int ey = std::max(0, H - 270);
  L.box = 24; L.pitch = std::min(32.0f, 27.0f + std::floor(ey / 6.0f));
  L.slotTop = 72;
  L.slotX[0] = 18;
  L.stageX = L.slotX[0] + L.box + 6;
  L.stageW = std::floor(92 + std::max(0, W - 480) * 0.08f);
  L.slotX[1] = L.stageX + L.stageW + 6;
  L.stageY = L.slotTop; L.stageH = 6 * L.pitch - 4;
  L.figCX = std::floor(L.stageX + L.stageW / 2);
  L.figScale = L.stageH >= 4 * art::HUMAN_H + 30 ? 4.0f : 3.0f;
  L.figFootY = std::floor(L.stageY + L.stageH - 20);
  L.dX = L.slotX[1] + L.box + 14;
  L.dW = (float)W - 18 - L.dX;
  L.dTop = 72;
  L.rowH = touch ? 24.0f : 18.0f;
  L.btnY = (float)H - 32;
  return L;
}
float pslotX(const PartyLay& L, int i) { return L.slotX[kPSlots[i].col]; }
float pslotY(const PartyLay& L, int i) { return L.slotTop + kPSlots[i].row * L.pitch; }
// the pack's items that fit a slot (supplies: potions and food); what the hero wears stays on the hero
std::vector<int> packFor(Game& g, int slot) {
  std::vector<int> v;
  for (int i = 0; i < (int)g.inv.size(); i++) {
    const ItemKind k = g.inv[(size_t)i].kind;
    if (slot == kSupplies ? (k == ItemKind::Potion || k == ItemKind::Food) : party::slotOf(k) == slot) {
      if (int* e = g.equipSlot(k); e && *e == i) continue;
      v.push_back(i);
    }
  }
  return v;
}
const Item* wornBy(const party::Member& m, int slot) {
  if (slot < 0 || slot >= party::SLOTS) return nullptr;
  const int e = m.eq[(size_t)slot];
  return e >= 0 && e < (int)m.inv.size() ? &m.inv[(size_t)e] : nullptr;
}
std::string partyNote;          // the page's last word ("ASTRID PUTS ON THE IRON HELM", or why not)
Color partyNoteC;
float partyNoteT = -100;
const Color kAppC(0.95f, 0.78f, 0.36f), kBondC(0.52f, 0.78f, 1.0f);
// a bar from lo to hi filled from `zero` to v, with tick marks (approval -100..100 and its thresholds; bond 0..100
// and the skills' marks)
void meter(Pix& P, float x, float y, float w, float v, float lo, float hi, Color c, std::initializer_list<float> marks, float zero) {
  P.rect(x - 1, y - 1, w + 2, 7, Color(0.03f, 0.02f, 0.04f, 0.95f));
  P.rect(x, y, w, 5, Color(0.14f, 0.11f, 0.10f));
  const auto at = [&](float q) { return x + (q - lo) / (hi - lo) * w; };
  const float z = at(zero), vv = at(std::clamp(v, lo, hi));
  const float a = std::min(z, vv), b = std::max(z, vv);
  const Color cc = v < zero ? Color(0.86f, 0.30f, 0.24f) : c;
  P.rect(a, y, b - a, 5, cc);
  P.rect(a, y, b - a, 1, Color(std::min(1.0f, cc.r + 0.25f), std::min(1.0f, cc.g + 0.25f), std::min(1.0f, cc.b + 0.25f)));
  for (float mk : marks) P.rect(std::floor(at(mk)), y - 2, 1, 9, Color(0.95f, 0.9f, 0.8f, 0.8f));
}
int suppliesOf(const party::Member& m) {
  int n = 0;
  for (const Item& x : m.inv) if (x.kind == ItemKind::Potion || x.kind == ItemKind::Food) n += std::max(1, (int)x.count);
  return n;
}
}  // namespace

void View::drawPartyPage(Game& g) {
  Pix& P = *pix_;
  const PartyLay L = partyLay(Pix::W, Pix::H, touchUI);
  const int n = (int)g.party.members.size();
  if (n == 0 && !g.party.fallen.empty()) partyPage_ = 1;
  if (g.party.fallen.empty()) partyPage_ = 0;
  partySel_ = std::clamp(partySel_, 0, std::max(0, n - 1));
  // ---- the faces row and REMEMBERED
  for (int i = 0; i < n; i++) {
    const party::Member& m = g.party.members[(size_t)i];
    const float x = 18 + i * L.chipPitch, y = L.chipY;
    const bool sel = partyPage_ == 0 && i == partySel_;
    P.rect(x, y, L.chipS, L.chipS, sel ? Color(0.30f, 0.22f, 0.12f, 0.95f) : Color(0.10f, 0.085f, 0.08f, 0.95f));
    const bool dim = m.state == party::State::Waiting || m.state == party::State::Home || m.state == party::State::Left;
    const Tex& pt = portraitTex(g, m, m.state == party::State::Downed ? 2 : 0);
    P.blitEx(pt, 0, 0, art::PORTRAIT_W, art::PORTRAIT_H, x + 1, y + 1, (float)art::PORTRAIT_W, (float)art::PORTRAIT_H, false,
             dim ? Color(0.6f, 0.6f, 0.66f, 0.85f) : Color(1, 1, 1, 1));
    P.frame(x, y, L.chipS, L.chipS, sel ? kGoldC : Color(0.42f, 0.34f, 0.22f));
  }
  if (n == 0) P.text(18, L.chipY + 9, "NO ONE TRAVELS WITH YOU NOW", 1, kDimC);
  else if (partyPage_ == 0) {   // the picked one's name and how they stand, beside the faces
    const party::Member& m = g.party.members[(size_t)partySel_];
    const float nx = 18 + n * L.chipPitch + 6;
    if (nx + 120 < L.remX) {
      P.textS(nx, L.chipY + 4, m.name, 1, kGoldC);
      P.text(nx, L.chipY + 15, party::stateName(m.state), 1, m.state == party::State::Downed ? Color(1, 0.45f, 0.38f) : kDimC);
    }
  }
  if (!g.party.fallen.empty()) button(L.remX, L.chipY + 2, L.remW, 22, "REMEMBERED", partyPage_ == 1);
  P.rect(16, L.chipY + L.chipS + 4, (float)Pix::W - 32, 1, Color(0.4f, 0.32f, 0.2f));
  const float noteA = clampf(2.5f - (t_ - partyNoteT), 0, 1);

  if (partyPage_ == 1) {
    // ---- REMEMBERED: the fallen, the newest first; the picked one's stone beside the list
    const int nf = (int)g.party.fallen.size();
    partyScroll_ = std::clamp(partyScroll_, 0, std::max(0, nf - 1));
    const float lx = 18, lw = std::floor(std::min(220.0f, (Pix::W - 36) * 0.42f)), top = L.dTop;
    const float rh = touchUI ? 24.0f : 18.0f;
    const int rows = std::max(3, (int)((Pix::H - 20 - top) / rh));
    const int first = std::clamp(partyScroll_ - rows + 1, 0, std::max(0, nf - rows));
    for (int r = 0; r < rows && first + r < nf; r++) {
      const int k = first + r;
      const party::Fallen& f = g.party.fallen[(size_t)(nf - 1 - k)];
      const float y = top + r * rh;
      P.rect(lx, y, lw, rh - 2, k == partyScroll_ ? Color(0.30f, 0.22f, 0.12f, 0.85f) : Color(0.10f, 0.085f, 0.08f, 0.6f));
      if (k == partyScroll_) P.frame(lx, y, lw, rh - 2, kGoldC);
      const float gx = lx + 5, gy = y + std::floor((rh - 2 - 10) / 2);   // a small headstone, lit from the left
      P.rect(gx, gy + 1, 7, 9, Color(0.08f, 0.06f, 0.06f));
      P.rect(gx + 1, gy, 5, 1, Color(0.08f, 0.06f, 0.06f));
      P.rect(gx + 1, gy + 1, 5, 8, Color(0.74f, 0.74f, 0.78f));
      P.rect(gx + 4, gy + 1, 2, 8, Color(0.52f, 0.52f, 0.58f));
      P.rect(gx + 2, gy + 3, 3, 1, Color(0.4f, 0.4f, 0.46f));
      P.text(lx + 18, y + std::floor((rh - 2 - 7) / 2), fitTextD(f.name, lw - 66), 1, kTextC);
      P.text(lx + lw - 5, y + std::floor((rh - 2 - 7) / 2), "DAY " + std::to_string(f.day), 1, kDimC, 2);
    }
    if (nf > 0) {
      const party::Fallen& f = g.party.fallen[(size_t)(nf - 1 - partyScroll_)];
      const float dx = lx + lw + 16, dw = (float)Pix::W - 18 - dx;
      float y = top;
      const int per = std::max(12, (int)(dw / 6));
      P.text(dx, y, f.name, P.textW(f.name, 2) <= dw ? 2 : 1, kGoldC);
      y += 18;
      P.text(dx, y, std::string(party::vocationName(f.voc)) + "  -  " + party::originName(f.origin), 1, kDimC);
      y += 12;
      for (const std::string& l : wrapD(f.cause.empty() ? std::string("FELL ON THE ROAD") : f.cause, per)) { P.text(dx, y, l, 1, Color(0.95f, 0.66f, 0.56f)); y += 10; }
      P.text(dx, y, "DIED ON DAY " + std::to_string(f.day) + ",  " + std::to_string(f.days) + (f.days == 1 ? " DAY" : " DAYS") + " TOGETHER", 1, kTextC);
      y += 10;
      if (f.kills) { P.text(dx, y, std::to_string(f.kills) + (f.kills == 1 ? " FOE FELL TO THEM" : " FOES FELL TO THEM"), 1, kTextC); y += 10; }
      y += 6;
      // the epitaph as carved: its lines centred between two rules
      const std::string ep = party::epitaph(g, f);
      std::vector<std::string> el;
      size_t s0 = 0;
      while (s0 <= ep.size()) {
        const size_t s1 = ep.find(" / ", s0);
        const std::string part = ep.substr(s0, s1 == std::string::npos ? std::string::npos : s1 - s0);
        for (const std::string& l : wrapD(part, per - 2)) el.push_back(l);
        if (s1 == std::string::npos) break;
        s0 = s1 + 3;
      }
      P.rect(dx + 10, y, dw - 20, 1, Color(0.55f, 0.45f, 0.28f));
      y += 6;
      for (const std::string& l : el) { if (y > Pix::H - 46) break; P.text(dx + dw / 2, y, l, 1, Color(0.86f, 0.84f, 0.78f), 1); y += 10; }
      P.rect(dx + 10, y + 1, dw - 20, 1, Color(0.55f, 0.45f, 0.28f));
      y += 10;
      // where the stone stands, from here
      if (!g.inside && y < Pix::H - 30) {
        const Actor& pl = g.pl();
        const double hx = (double)g.world.ox + pl.p.x / 16.0, hy = (double)g.world.oy + pl.p.y / 16.0;
        const double ddx = f.gx - hx, ddy = f.gy - hy, d = std::sqrt(ddx * ddx + ddy * ddy);
        std::string where;
        if (d < 6) where = "THEIR GRAVE IS HERE";
        else {
          static const char* dirs[8] = {"EAST", "SOUTH-EAST", "SOUTH", "SOUTH-WEST", "WEST", "NORTH-WEST", "NORTH", "NORTH-EAST"};
          const int o = ((int)std::lround(std::atan2(ddy, ddx) / 0.785398) % 8 + 8) % 8;
          where = "THEIR GRAVE LIES " + std::to_string((int)d) + " PACES " + dirs[o];
        }
        P.text(dx, y, where, 1, kDimC);
        y += 10;
      }
      std::string marks;
      if (f.graveFlags & party::GF_FLOWERS) marks = "FLOWERS LAID";
      if (f.graveFlags & party::GF_KEEPSAKE) marks += marks.empty() ? "A KEEPSAKE TAKEN" : ",  A KEEPSAKE TAKEN";
      if (!marks.empty() && y < Pix::H - 20) P.text(dx, y, marks, 1, Color(0.62f, 0.86f, 0.6f));
    }
    return;
  }
  if (n == 0) return;

  // ---- the companion on the stage
  party::Member& m = g.party.members[(size_t)partySel_];
  const bool hound = m.voc == party::Vocation::Hound;
  P.rect(L.stageX, L.stageY, L.stageW, L.stageH, Color(0.03f, 0.025f, 0.04f, 0.55f));
  P.frame(L.stageX, L.stageY, L.stageW, L.stageH, Color(0.3f, 0.24f, 0.15f));
  heroStage(P, light_, L.figCX, L.figFootY, 0.62f);
  // (fixer M8 r2, review: "a crude upscaled walk frame, side-on mid-stride, changing between the tabs") the doll stands
  // still, facing the player, in the idle pose (every layer of their gear readable), and turns only on < TURN >
  const int turn = partyDollTurn_ & 3;
  if (hound) {
    const Tex& ht = houndTex(m, g);
    static const int rowOf[4] = {2, 0, 2, 1};   // (r3) in profile first: an animal reads as itself side-on, then front, other side, back
    const int fr = art::HF_IDLE + ((int)(t_ * 2) & 1);
    // (fixer M8 r3, review: "a tiny sprite low in an empty panel, under a glow not centred on it") the animal scaled
    // by its own drawn size (a pup fills a third of its cell), an integer scale for crisp pixels, as big as the stage
    // allows; its feet on the stage's floor disc, the body in the glow
    static std::map<uint64_t, std::array<int, 4>> boxes;   // look key -> the idle frames' opaque box (x0, y0, x1, y1)
    const art::HoundLook hl = houndLookOf(m, g);
    auto bit = boxes.find(hl.key());
    if (bit == boxes.end()) {
      const Canvas sh = art::houndSheet(hl);
      std::array<int, 4> bb{{art::HOUND_W, art::HOUND_H, -1, -1}};
      for (int row = 0; row < 3; row++)
        for (int f2 = art::HF_IDLE; f2 <= art::HF_IDLE + 1; f2++)
          for (int y = 0; y < art::HOUND_H; y++)
            for (int x = 0; x < art::HOUND_W; x++)
              if ((sh.get(f2 * art::HOUND_W + x, row * art::HOUND_H + y) >> 24) > 0) {
                bb[0] = std::min(bb[0], x); bb[1] = std::min(bb[1], y); bb[2] = std::max(bb[2], x); bb[3] = std::max(bb[3], y);
              }
      if (bb[2] < 0) bb = {{0, 0, art::HOUND_W - 1, art::HOUND_H - 1}};
      bit = boxes.emplace(hl.key(), bb).first;
    }
    const std::array<int, 4>& bb = bit->second;
    const float bw = (float)(bb[2] - bb[0] + 1), bh = (float)(bb[3] - bb[1] + 1);
    const float sc = std::clamp(std::floor(std::min((L.stageW - 10.0f) / bw, (L.figFootY - L.stageY - 40.0f) / bh)), 3.0f, 8.0f);
    const float hx = L.figCX - ((bb[0] + bb[2] + 1) * 0.5f) * sc, hy = L.figFootY + 1 - (bb[3] + 1) * sc;
    P.blitEx(ht, fr * art::HOUND_W, rowOf[turn] * art::HOUND_H, art::HOUND_W, art::HOUND_H, turn == 2 ? L.figCX - (art::HOUND_W - (bb[0] + bb[2] + 1) * 0.5f) * sc : hx, hy,
             art::HOUND_W * sc, art::HOUND_H * sc, turn == 2);
  } else {
    // their own figure in what they wear (their street look dressed in their gear, each piece in its makers' style)
    const int ai = partySel_ < (int)partyActor_.size() ? partyActor_[(size_t)partySel_] : -1;
    const art::HumanLook look = ai > 0 ? g.actors[(size_t)ai].look : g.partyLook(m);
    static const int order[4] = {0, 2, 1, 3};
    const int face = order[turn];
    const int frame = 0;   // idle: standing, both feet on the stage
    const Tex& t = dollTex(*pix_, look);
    const float sc = L.figScale;
    mythicAuraAt(look, frame, face == 3 ? 2 : face, face == 3, L.figCX - art::HUMAN_W * sc / 2, L.figFootY - (art::HUMAN_H - 1) * sc, sc);
    P.blitEx(t, frame * art::HUMAN_W, (face == 3 ? 2 : face) * art::HUMAN_H, art::HUMAN_W, art::HUMAN_H, L.figCX - art::HUMAN_W * sc / 2,
             L.figFootY - (art::HUMAN_H - 1) * sc, art::HUMAN_W * sc, art::HUMAN_H * sc, face == 3);
  }
  P.text(L.figCX, L.figFootY + 8, "LEVEL " + std::to_string(party::levelFor(g.plLevel)), 1, kDimC, 1);
  {   // (fixer M8 r2) < TURN > at the stage's foot, and what their gear makes of them at its head (as the hero's EQUIP)
    const float bw = 16, bh = 13, by = L.figFootY + 5;
    button(L.stageX + 3, by, bw, bh, "<", false);
    button(L.stageX + L.stageW - bw - 3, by, bw, bh, ">", false);
    const int ai = partySel_ < (int)partyActor_.size() ? partyActor_[(size_t)partySel_] : -1;
    const party::Body b = party::bodyFor(m, g.plLevel);
    const float armor = ai > 0 ? g.actors[(size_t)ai].armor : b.armor, dmg = ai > 0 ? g.actors[(size_t)ai].dmg : b.dmg;
    const float hp = ai > 0 ? g.actors[(size_t)ai].maxHp : b.maxHp;
    P.text(L.figCX, L.stageY + 4, "ARMOR " + std::to_string((int)std::lround(armor)), 1, kTextC, 1);
    P.text(L.figCX, L.stageY + 14, "DAMAGE " + std::to_string((int)std::lround(dmg)), 1, kTextC, 1);
    P.text(L.figCX, L.stageY + 24, "HEALTH " + std::to_string((int)std::lround(hp)), 1, kDimC, 1);
  }
  // ---- the slots
  partySlot_ = std::clamp(partySlot_, -1, kNPSlots - 1);
  for (int i = 0; i < kNPSlots; i++) {
    if (hound && kPSlots[i].slot != kSupplies) continue;
    const float sx = pslotX(L, i), sy = pslotY(L, i);
    const bool sel = partyFocus_ == 1 && i == partySlot_;
    P.rect(sx, sy, L.box, L.box, sel ? Color(0.3f, 0.22f, 0.12f, 0.95f) : Color(0.1f, 0.085f, 0.08f, 0.95f));
    const Item* it = wornBy(m, kPSlots[i].slot);
    // (M8b) the companion's slots framed in their worn pieces' rarity, as the hero's are
    if (it && kPSlots[i].slot != kSupplies) rarityFrame(sx, sy, L.box, L.box, it->rarity, false);
    else P.frame(sx, sy, L.box, L.box, Color(0.42f, 0.34f, 0.22f));
    P.rect(sx + 1, sy + 1, L.box - 2, 1, Color(1, 1, 1, 0.06f));
    if (sel) selCursor(P, t_, sx, sy, L.box, L.box);
    if (kPSlots[i].slot == kSupplies) {
      const int sup = suppliesOf(m);
      P.blitEx(iconTex(art::Icon::PotionRed, 0), 0, 0, 16, 16, sx + 4, sy + 4, 16, 16, false, sup ? Color(1, 1, 1, 1) : Color(0.3f, 0.28f, 0.26f, 0.45f));
      if (sup) P.textS(sx + L.box - 2, sy + L.box - 9, std::to_string(sup), 1, kTextC, 2);
    } else if (it) {
      P.blitEx(itemTex(g, *it), 0, 0, 16, 16, sx + 4, sy + 4, 16, 16);
    } else {
      P.blitEx(iconTex(kPSlots[i].ghost, 0), 0, 0, 16, 16, sx + 4, sy + 4, 16, 16, false, Color(0.3f, 0.28f, 0.26f, 0.45f));
    }
  }
  // ---- the detail column: STORY or GEAR
  const float dx = L.dX, dw = L.dW;
  const float tabH = touchUI ? 22.0f : 16.0f;
  button(dx, L.dTop - 2, 58, tabH, "STORY", partyFocus_ == 0);
  button(dx + 62, L.dTop - 2, 52, tabH, "GEAR", partyFocus_ == 1);
  float y = L.dTop + tabH + 6;
  const int per = std::max(12, (int)(dw / 6));
  if (partyFocus_ == 0) {
    P.text(dx, y, std::string(party::vocationName(m.voc)) + "  -  " + party::originName(m.origin), 1, kTextC);
    y += 10;
    std::string st = party::stateName(m.state);
    if (m.flags & party::MF_WOUNDED) st += "  -  WOUNDED";
    P.text(dx, y, st, 1, m.state == party::State::Downed ? Color(1, 0.45f, 0.38f) : kDimC);
    y += 13;
    const float bw = std::min(dw - 82, 150.0f);
    P.text(dx, y, "APPROVAL", 1, kDimC);
    meter(P, dx + 56, y + 1, bw, (float)m.approval, -100, 100, kAppC, {(float)party::APPROVAL_LEAVE, (float)party::APPROVAL_COMPLAIN, (float)party::APPROVAL_QUEST}, 0);
    P.text(dx + 62 + bw, y, std::to_string(m.approval), 1, m.approval < party::APPROVAL_COMPLAIN ? Color(1, 0.5f, 0.42f) : kTextC);
    y += 11;
    P.text(dx, y, "BOND", 1, kDimC);
    meter(P, dx + 56, y + 1, bw, (float)m.bond, 0, 100, kBondC, {(float)party::SKILL_BOND[0], (float)party::SKILL_BOND[1], (float)party::SKILL_BOND[2]}, 0);
    P.text(dx + 62 + bw, y, std::to_string(m.bond), 1, kTextC);
    y += 14;
    // the three bond skills: a lit diamond when it is theirs
    const auto sk = party::skillsOf(m.voc);
    const uint8_t un = party::unlockedSkills(m);
    const float foot = (float)Pix::H - 22;
    for (int k = 0; k < 3; k++) {
      const party::SkillInfo& si = party::skillInfo(sk[(size_t)k]);
      const bool on = (un >> k) & 1;
      const Color dc = on ? kBondC : Color(0.32f, 0.3f, 0.3f);
      for (int j = -2; j <= 2; j++) P.rect(dx + 3 - (2 - std::abs(j)), y + 3 + j, (float)(2 * (2 - std::abs(j)) + 1), 1, dc);
      P.text(dx + 10, y, si.name, 1, on ? Color(0.75f, 0.88f, 1.0f) : kDimC);
      if (!on) P.text(dx + dw, y, "BOND " + std::to_string((int)si.bond), 1, kDimC, 2);
      else if (m.skillCd[k] > 0) P.text(dx + dw, y, std::to_string((int)std::ceil(m.skillCd[k])) + "S", 1, kDimC, 2);
      y += 9;
      const auto wl = wrapD(si.what, per - 2);
      for (size_t q = 0; q < wl.size() && q < 2 && y + 9 <= foot - 10; q++) { P.text(dx + 10, y, wl[q], 1, on ? kTextC : Color(0.5f, 0.47f, 0.43f)); y += 9; }
      y += 2;
    }
    y += 3;
    // their own story (VOICE: party::aboutLine), as much as the room allows
    const auto al = wrapD(party::aboutLine(g, m), per);
    for (size_t q = 0; q < al.size() && y + 9 <= foot - 4; q++) { P.text(dx, y, al[q], 1, Color(0.86f, 0.80f, 0.68f)); y += 9; }
    P.text(dx, foot, std::to_string(m.days) + (m.days == 1 ? " DAY" : " DAYS") + " TOGETHER   KILLS " + std::to_string(m.kills) +
           (m.downs ? "   DOWNED " + std::to_string(m.downs) : std::string()), 1, kDimC);
  } else {
    // GEAR: the picked slot, what they wear in it and the pack's pieces for it
    if (partySlot_ < 0) partySlot_ = hound ? kNPSlots - 1 : 3;
    const PSlot& ps = kPSlots[partySlot_];
    P.text(dx, y, ps.name, 1, kGoldC);
    const Item* it = wornBy(m, ps.slot);
    if (ps.slot == kSupplies) {
      int pots = 0, food = 0;
      for (const Item& x : m.inv) { if (x.kind == ItemKind::Potion) pots += std::max(1, (int)x.count); else if (x.kind == ItemKind::Food) food += std::max(1, (int)x.count); }
      P.text(dx + 60, y, std::to_string(pots) + (pots == 1 ? " POTION, " : " POTIONS, ") + std::to_string(food) + " FOOD", 1, kTextC);
      y += 10;
      P.text(dx, y, fitTextD("THEY DRINK A POTION BELOW A QUARTER HEALTH", dw), 1, kDimC);
      y += 4;
    } else if (it) {
      rarityIcon(g, *it, dx, y + 9, 16);
      rarityText(dx + 20, y + 10, fitTextD(it->name, dw - 20 - 92), it->rarity);
      const std::string made = makerLine(g, *it);
      if (!made.empty()) P.text(dx + 20, y + 19, fitTextD(made, dw - 20 - 92), 1, Color(0.78f, 0.7f, 0.56f));
      button(dx + dw - 86, y + 8, 86, touchUI ? 22.0f : 18.0f, touchUI ? "TAKE BACK" : "TAKE BACK (X)", false);
      y += 12;
      // (M8b fix, 15.27.2 "show in plain words") what the piece does for them: its affixes and power, a hero-only one
      // (it works only on the hero: gear::affixWorksForCompanion) marked so
      std::vector<std::pair<std::string, bool>> lines;
      for (const ItemAffix& a : it->affix)
        if (a.kind != Affix::None) lines.push_back({affixText(a.kind, gear::affixUse(*it, a.kind, g.plLevel)), gear::affixWorksForCompanion(a.kind)});
      if (it->unique != Unique::None) lines.push_back({uniqueInfo(it->unique).name, gear::uniqueWorksForCompanion(it->unique)});
      for (size_t q = 0; q < lines.size() && q < 3; q++) {
        std::string s = lines[q].first + (lines[q].second ? "" : " (HERO ONLY)");
        if (q == 2 && lines.size() > 3) s = "+" + std::to_string(lines.size() - 2) + " MORE";
        P.text(dx + 20, y + 19, fitTextD(s, dw - 20), 1, lines[q].second || (q == 2 && lines.size() > 3) ? Color(0.62f, 0.80f, 1.0f) : kDimC);
        y += 9;
      }
    } else {
      P.text(dx + 60, y, "NOTHING WORN", 1, kDimC);
    }
    y += 22;
    P.rect(dx, y - 4, dw, 1, Color(0.4f, 0.32f, 0.2f));
    const std::vector<int> c = packFor(g, ps.slot);
    if (c.empty()) P.text(dx + 4, y + 4, ps.slot == kSupplies ? "NO POTIONS OR FOOD IN YOUR PACK" : "NOTHING IN YOUR PACK FOR IT", 1, kDimC);
    const int rows = std::max(2, (int)(((float)Pix::H - 14 - y) / L.rowH));
    partyPack_ = std::clamp(partyPack_, 0, std::max(0, (int)c.size() - 1));
    const int first = std::clamp(partyPack_ - rows + 1, 0, std::max(0, (int)c.size() - rows));
    for (int r = 0; r < rows && first + r < (int)c.size(); r++) {
      const int k = first + r;
      const Item& pi = g.inv[(size_t)c[(size_t)k]];
      const float ry = y + r * L.rowH;
      const bool hot = k == partyPack_;
      P.rect(dx, ry, dw, L.rowH - 2, hot ? Color(0.3f, 0.22f, 0.12f, 0.85f) : Color(0.1f, 0.085f, 0.08f, 0.6f));
      if (hot) P.frame(dx, ry, dw, L.rowH - 2, kGoldC);
      const float isz = touchUI ? 16.0f : 12.0f;
      rarityIcon(g, pi, dx + 3, ry + std::floor((L.rowH - 2 - isz) / 2), isz);
      const std::string nm = pi.name + (pi.count > 1 ? " (" + std::to_string(pi.count) + ")" : std::string());
      rarityText(dx + 22, ry + std::floor((L.rowH - 2 - 7) / 2), fitTextD(nm, dw - 22 - 52), pi.rarity);
      P.text(dx + dw - 4, ry + std::floor((L.rowH - 2 - 7) / 2), ps.slot == kSupplies ? "GIVE" : "EQUIP", 1, hot ? kGoldC : kDimC, 2);
      // (M8b) better or worse than what they wear there (gear::powerScore at their level: the hero's, as Game::partyEquip)
      if (ps.slot != kSupplies && itemEquippable(pi.kind))
        cmpArrow(dx + dw - 4 - (float)P.textW("EQUIP", 1) - 10, ry + std::floor((L.rowH - 2 - 7) / 2), cmpVerdict(g, pi, it));
    }
    if ((int)c.size() > rows) P.text(dx + dw, y - 14, std::to_string(partyPack_ + 1) + "/" + std::to_string(c.size()), 1, kDimC, 2);
  }
  if (noteA > 0 && !partyNote.empty()) {
    const float w = (float)P.textW(partyNote, 1) + 10, nx = std::max(12.0f, std::floor(L.figCX - w / 2));
    const float ny = (float)Pix::H - 21;
    P.rect(nx, ny, w, 12, Color(0.05f, 0.04f, 0.06f, 0.92f * noteA));
    P.frame(nx, ny, w, 12, Color(partyNoteC.r, partyNoteC.g, partyNoteC.b, 0.8f * noteA));
    P.text(nx + 5, ny + 3, partyNote, 1, Color(partyNoteC.r, partyNoteC.g, partyNoteC.b, noteA));
  }
}

void View::partyPageKey(Game& g, int key) {
  const int n = (int)g.party.members.size();
  auto mv = [&]() { audio_->play(Sfx::MenuMove); };
  if (key == SDLK_N && !g.party.fallen.empty()) { partyPage_ ^= 1; partyScroll_ = 0; mv(); return; }
  if (partyPage_ == 1) {
    const int nf = (int)g.party.fallen.size();
    if (key == SDLK_UP || key == SDLK_W) { partyScroll_ = std::max(0, partyScroll_ - 1); mv(); }
    if (key == SDLK_DOWN || key == SDLK_S) { partyScroll_ = std::min(std::max(0, nf - 1), partyScroll_ + 1); mv(); }
    if ((key == SDLK_LEFTBRACKET || key == SDLK_RIGHTBRACKET) && n > 0) { partyPage_ = 0; mv(); }
    return;
  }
  if (n == 0) return;
  partySel_ = std::clamp(partySel_, 0, n - 1);
  if (key == SDLK_LEFTBRACKET) { partySel_ = (partySel_ + n - 1) % n; partyPack_ = 0; mv(); return; }
  if (key == SDLK_RIGHTBRACKET) { partySel_ = (partySel_ + 1) % n; partyPack_ = 0; mv(); return; }
  const bool hound = g.party.members[(size_t)partySel_].voc == party::Vocation::Hound;
  if (key == SDLK_UP || key == SDLK_W || key == SDLK_DOWN || key == SDLK_S) {
    const int dir = key == SDLK_UP || key == SDLK_W ? -1 : 1;
    if (hound) { partyFocus_ = dir > 0 ? 1 : 0; partySlot_ = kNPSlots - 1; mv(); return; }
    if (partyFocus_ == 0) { if (dir > 0) { partyFocus_ = 1; partySlot_ = 0; } }
    else if (partySlot_ + dir < 0) partyFocus_ = 0;
    else partySlot_ = std::min(kNPSlots - 1, partySlot_ + dir);
    partyPack_ = 0;
    mv();
    return;
  }
  if (partyFocus_ != 1 || partySlot_ < 0) return;
  const PSlot& ps = kPSlots[partySlot_];
  const std::vector<int> c = packFor(g, ps.slot);
  if (key == SDLK_LEFT || key == SDLK_A) { if (!c.empty()) { partyPack_ = (partyPack_ + (int)c.size() - 1) % (int)c.size(); mv(); } return; }
  if (key == SDLK_RIGHT || key == SDLK_D) { if (!c.empty()) { partyPack_ = (partyPack_ + 1) % (int)c.size(); mv(); } return; }
  const std::string who = g.party.members[(size_t)partySel_].name;
  if ((key == SDLK_RETURN || key == SDLK_SPACE || key == SDLK_KP_ENTER) && !c.empty()) {
    const int idx = c[(size_t)std::clamp(partyPack_, 0, (int)c.size() - 1)];
    const std::string nm = g.inv[(size_t)idx].name;
    const bool ok = ps.slot == kSupplies ? g.partyGive(partySel_, idx) : g.partyEquip(partySel_, idx);
    partyNote = ok ? (ps.slot == kSupplies ? who + " TAKES THE " + nm : who + " PUTS ON THE " + nm) : who + " CANNOT USE THAT";
    if (partyNote.size() > 46) partyNote = ok ? (ps.slot == kSupplies ? who + " TAKES IT" : who + " PUTS IT ON") : who + " CANNOT USE THAT";
    partyNoteC = ok ? Color(0.6f, 0.95f, 0.6f) : Color(1, 0.55f, 0.48f);
    partyNoteT = t_;
    audio_->play(ok ? Sfx::Buy : Sfx::MenuBack);
    return;
  }
  if ((key == SDLK_X || key == SDLK_BACKSPACE) && ps.slot != kSupplies && wornBy(g.party.members[(size_t)partySel_], ps.slot)) {
    const std::string nm = wornBy(g.party.members[(size_t)partySel_], ps.slot)->name;
    const bool ok = g.partyUnequip(partySel_, ps.slot);
    partyNote = ok ? "YOU TAKE BACK THE " + nm : who + " KEEPS IT FOR NOW";
    if (partyNote.size() > 46) partyNote = ok ? "YOU TAKE IT BACK" : who + " KEEPS IT FOR NOW";
    partyNoteC = ok ? kTextC : Color(1, 0.55f, 0.48f);
    partyNoteT = t_;
    audio_->play(ok ? Sfx::Pickup : Sfx::MenuBack);
  }
}

void View::partyPageTap(Game& g, Vec2 p) {
  const PartyLay L = partyLay(Pix::W, Pix::H, touchUI);
  const int n = (int)g.party.members.size();
  for (int i = 0; i < n; i++)
    if (inR(p, 18 + i * L.chipPitch - 2, L.chipY - 2, L.chipS + 4, L.chipS + 4)) { partySel_ = i; partyPage_ = 0; partyPack_ = 0; audio_->play(Sfx::MenuMove); return; }
  if (!g.party.fallen.empty() && inR(p, L.remX, L.chipY, L.remW, 26)) { partyPage_ ^= 1; partyScroll_ = 0; audio_->play(Sfx::MenuMove); return; }
  if (partyPage_ == 1) {
    const int nf = (int)g.party.fallen.size();
    const float lw = std::floor(std::min(220.0f, (Pix::W - 36) * 0.42f)), rh = touchUI ? 24.0f : 18.0f;
    const int rows = std::max(3, (int)((Pix::H - 20 - L.dTop) / rh));
    const int first = std::clamp(partyScroll_ - rows + 1, 0, std::max(0, nf - rows));
    for (int r = 0; r < rows && first + r < nf; r++)
      if (inR(p, 18, L.dTop + r * rh, lw, rh)) { partyScroll_ = first + r; audio_->play(Sfx::MenuMove); return; }
    return;
  }
  if (n == 0) return;
  partySel_ = std::clamp(partySel_, 0, n - 1);
  const bool hound = g.party.members[(size_t)partySel_].voc == party::Vocation::Hound;
  {   // (fixer M8 r2) the doll's < TURN > (a finger-sized reach round the small buttons)
    const float by = L.figFootY + 5;
    if (inR(p, L.stageX, by - 5, 24, 24)) { partyDollTurn_ = (partyDollTurn_ + 3) & 3; audio_->play(Sfx::MenuMove); return; }
    if (inR(p, L.stageX + L.stageW - 24, by - 5, 24, 24)) { partyDollTurn_ = (partyDollTurn_ + 1) & 3; audio_->play(Sfx::MenuMove); return; }
  }
  for (int i = 0; i < kNPSlots; i++) {
    if (hound && kPSlots[i].slot != kSupplies) continue;
    if (inR(p, pslotX(L, i) - 2, pslotY(L, i) - 2, L.box + 4, L.box + 4)) {
      // a tap on the worn slot already picked takes its piece back
      if (partyFocus_ == 1 && partySlot_ == i && kPSlots[i].slot != kSupplies && wornBy(g.party.members[(size_t)partySel_], kPSlots[i].slot)) { partyPageKey(g, SDLK_X); return; }
      partyFocus_ = 1; partySlot_ = i; partyPack_ = 0;
      audio_->play(Sfx::MenuMove);
      return;
    }
  }
  const float tabH = touchUI ? 22.0f : 16.0f;
  if (inR(p, L.dX, L.dTop - 4, 58, tabH + 4)) { partyFocus_ = 0; audio_->play(Sfx::MenuMove); return; }
  if (inR(p, L.dX + 62, L.dTop - 4, 52, tabH + 4)) { partyFocus_ = 1; if (partySlot_ < 0) partySlot_ = hound ? kNPSlots - 1 : 3; audio_->play(Sfx::MenuMove); return; }
  if (partyFocus_ != 1 || partySlot_ < 0) return;
  const PSlot& ps = kPSlots[partySlot_];
  float y = L.dTop + tabH + 6;
  if (ps.slot == kSupplies) y += 4 + 10;
  else if (wornBy(g.party.members[(size_t)partySel_], ps.slot)) {
    if (inR(p, L.dX + L.dW - 86, y + 6, 86, (touchUI ? 22.0f : 18.0f) + 4)) { partyPageKey(g, SDLK_X); return; }
    y += 12;
  }
  y += 22;
  const std::vector<int> c = packFor(g, ps.slot);
  const int rows = std::max(2, (int)(((float)Pix::H - 14 - y) / L.rowH));
  const int first = std::clamp(partyPack_ - rows + 1, 0, std::max(0, (int)c.size() - rows));
  for (int r = 0; r < rows && first + r < (int)c.size(); r++)
    if (inR(p, L.dX, y + r * L.rowH, L.dW, L.rowH)) { partyPack_ = first + r; partyPageKey(g, SDLK_RETURN); return; }
}
