// Party gallery (M8 "Fellowship", ART lane): the party portraits, the bonded hound / wolf, the graves of the fallen,
// the Downed and Kneel postures (rpg/art/art_party.h, art_life.h) and the ruin walls' runs (art_props.h).
//   party_gallery <outDir>   writes (each zoomed and at 1x as *_1x.png)
//     portraits_cultures.png  every mood x the 12 cultures' villagers and soldiers (both sexes)
//     portraits_kit.png       every mood x the armour bands, helm forms, hair styles, headwear, peoples, cloaks
//     hounds.png              hound and wolf x pup / half grown / grown x coats x collars: 10 frames x 3 facings
//                             (and the flipped left), with their portraits in every mood
//     graves.png              the five rites x plain / fresh / flowers / both x the laid weapons, on grass and earth
//     postures.png            Downed, Kneel (binding), Kneel (mourning) x looks x 3 facings (and left) x 2 frames,
//                             a companion downed beside the one binding the wound, a mourner at a grave
//     ruins.png               a ruin hall's walls (east-west and north-south runs, corners) as the view lays them
//   party_gallery --check     every portrait, hound cell, grave and posture cell: non-empty, inside its cell with a
//                             pixel's room for the outline (the portrait bust and the feet may touch the bottom row),
//                             outlined, deterministic between two runs; non-zero on failure
#include <cctype>
#include <initializer_list>

#include "rpg/culture/culture.h"
#include "rpg/sim/world.h"
#include "tools/preview/preview_util.h"

namespace census {
struct Rank { bool royal = false, capital = false; };
void dress(art::HumanLook& L, std::string& name, Role r, bool female, const cult::Culture& C, const cult::Culture& owner,
           uint64_t seed, const Rank& rank);
}  // namespace census

namespace {

using art::HumanLook;
using art::Posture;

Canvas cellOf(const Canvas& sheet, int col, int row, int w, int h) {
  Canvas c(w, h);
  for (int y = 0; y < h; y++)
    for (int x = 0; x < w; x++) c.set(x, y, sheet.get(col * w + x, row * h + y));
  return c;
}
Canvas flipped(const Canvas& s) {
  Canvas d(s.w, s.h);
  for (int y = 0; y < s.h; y++)
    for (int x = 0; x < s.w; x++) d.set(s.w - 1 - x, y, s.get(x, y));
  return d;
}
void shadowAt(Board& b, int cx, int cy, int rx, int ry) {
  for (int y = cy - ry; y <= cy + ry; y++)
    for (int x = cx - rx; x <= cx + rx; x++) {
      const float d = ((x - cx) * (x - cx)) / (float)(rx * rx) + ((y - cy) * (y - cy)) / (float)(ry * ry);
      if (d <= 1.0f) b.c.set(x, y, art::shade(b.c.get(x, y), 0.72f));
    }
}
// a dark panel like the HUD's party column
void panel(Board& b, int x0, int y0, int w, int h) {
  for (int y = y0; y < y0 + h; y++)
    for (int x = x0; x < x0 + w; x++) {
      const bool edge = x == x0 || y == y0 || x == x0 + w - 1 || y == y0 + h - 1;
      b.c.set(x, y, edge ? rgba(120, 100, 70) : rgba(34, 30, 40));
    }
}

// ---------------------------------------------------------------- looks
struct Look { std::string name; HumanLook L; };
HumanLook plainLook() {
  HumanLook L;
  L.skin = rgba(226, 176, 132); L.hairColor = rgba(96, 60, 34); L.topColor = rgba(70, 110, 160); L.bottomColor = rgba(84, 66, 50);
  return L;
}
uint32_t bandTint(int b) {
  static const uint32_t c[8] = {0, rgba(150, 150, 158), rgba(150, 150, 158), rgba(200, 208, 220), rgba(222, 186, 92),
                                rgba(120, 214, 140), rgba(92, 70, 120), rgba(200, 52, 44)};
  return c[std::clamp(b, 0, 7)];
}
HumanLook fullKit(int b) {
  HumanLook L = plainLook();
  L.skin = rgba(236, 188, 146); L.hairColor = rgba(120, 70, 36); L.tabardColor = rgba(170, 50, 40);
  L.helmStyle = (uint8_t)b; L.armorStyle = (uint8_t)b; L.gloves = (uint8_t)b; L.boots = (uint8_t)b;
  L.cloak = (uint8_t)(b % 3 + 1);
  static const uint32_t cloaks[8] = {0, rgba(60, 96, 62), rgba(128, 40, 40), rgba(52, 70, 120), rgba(48, 64, 140),
                                     rgba(58, 56, 64), rgba(110, 30, 40), rgba(40, 34, 40)};
  L.cloakColor = cloaks[b];
  L.shieldStyle = (uint8_t)((b - 1) % 4 + 1);
  L.trimColor = bandTint(b);
  L.weapon = 1; L.weaponColor = bandTint(b);
  L.backItem = 1;
  L.amulet = b >= 4;
  return L;
}
std::vector<Look> cultureLooks() {
  std::vector<Look> v;
  for (int a = 0; a < (int)cult::Archetype::COUNT; a++) {
    const cult::Culture C = cult::Atlas::make((cult::Archetype)a, 1234u + (uint32_t)a * 7919u);
    const std::string nm = std::string(cult::archetypeName((cult::Archetype)a)).substr(0, 7);
    const Role roles[4] = {Role::Villager, Role::Villager, Role::Soldier, Role::Captain};
    for (int k = 0; k < 4; k++) {
      HumanLook L = plainLook();
      const bool female = (k & 1) != 0;
      L.hair = female ? art::Hair::Long : art::Hair::Short;
      std::string name;
      census::Rank rank;
      census::dress(L, name, roles[k], female, C, C, 77u + (uint64_t)a * 131 + (uint64_t)k * 977, rank);
      v.push_back({nm + (k < 2 ? (female ? " F" : " M") : (k == 2 ? " SOL" : " CPT")), L});
    }
  }
  return v;
}
std::vector<Look> kitLooks() {
  std::vector<Look> v;
  for (int b = 1; b <= 7; b++) v.push_back({"BAND " + std::to_string(b), fullKit(b)});
  static const char* hf[10] = {"NASAL", "KETTLE", "GREAT", "SPANGEN", "HORNED", "PLUMED", "AVENTL", "MASKED", "CRESTED", "WINGED"};
  for (int f = 1; f <= 10; f++) {
    HumanLook L = fullKit(3); L.helmForm = (uint8_t)f; L.helmStyle = 3; L.plumeColor = rgba(190, 40, 40); L.cloak = 0;
    if (f == 6) L.crest = 3;
    v.push_back({hf[f - 1], L});
  }
  static const char* hn[8] = {"BALD", "SHORT", "LONG", "PONYTL", "MOHAWK", "BRAIDS", "BUN", "CURLS"};
  static const uint32_t hc[8] = {rgba(60, 40, 30), rgba(150, 90, 40), rgba(220, 190, 110), rgba(40, 30, 30), rgba(170, 50, 30), rgba(200, 120, 60), rgba(70, 50, 40), rgba(30, 24, 26)};
  static const uint32_t sk[8] = {rgba(236, 188, 146), rgba(200, 150, 110), rgba(240, 200, 170), rgba(140, 96, 66), rgba(226, 176, 132), rgba(110, 74, 54), rgba(250, 214, 186), rgba(172, 120, 84)};
  for (int h = 0; h < 8; h++) {
    HumanLook L = plainLook(); L.hair = (art::Hair)h; L.hairColor = hc[h]; L.skin = sk[h]; L.beard = h == 0 || h == 4;
    L.topColor = rgba(80 + h * 18, 110, 150 - h * 10);
    if (h & 1) L.outfit = art::Outfit::Dress;
    v.push_back({hn[h], L});
  }
  static const char* hw[8] = {"HOOD", "CAP", "TURBAN", "FURHAT", "VEIL", "CIRCLET", "CONICAL", "SCARF"};
  for (int w = 1; w <= 8; w++) {
    HumanLook L = plainLook(); L.headwear = (uint8_t)w; L.headColor = w == 7 ? 0 : rgba(150 - w * 8, 60 + w * 14, 70 + w * 10);
    L.hair = (w & 1) ? art::Hair::Long : art::Hair::Short; L.cut = (uint8_t)(w % 8 + 1);
    v.push_back({hw[w - 1], L});
  }
  { HumanLook L = plainLook(); L.people = 2; L.hair = art::Hair::Long; L.hairColor = rgba(230, 220, 170); L.topColor = rgba(80, 130, 90); L.cut = 8; L.jewellery = 3; v.push_back({"ELF", L}); }
  { HumanLook L = plainLook(); L.people = 2; L.hair = art::Hair::Short; L.hairColor = rgba(40, 40, 60); L.armorStyle = 4; L.helmStyle = 0; L.cloak = 3; L.cloakColor = rgba(40, 80, 60); v.push_back({"ELF KN", L}); }
  { HumanLook L = plainLook(); L.people = 1; L.cut = 7; L.topColor = rgba(170, 90, 50); L.pattern = 1; L.patternColor = rgba(230, 200, 100); L.facePaint = 1; v.push_back({"HALF", L}); }
  { HumanLook L = plainLook(); L.outfit = art::Outfit::Guard; L.helmet = true; L.tabardColor = rgba(40, 70, 150); v.push_back({"GUARD", L}); }
  { HumanLook L = plainLook(); L.outfit = art::Outfit::Robe; L.hood = true; L.topColor = rgba(90, 60, 110); L.tabardColor = rgba(220, 190, 80); v.push_back({"MAGE", L}); }
  { HumanLook L = plainLook(); L.outfit = art::Outfit::Rags; L.hair = art::Hair::Curls; L.skin = rgba(150, 100, 70); v.push_back({"RAGS", L}); }
  { HumanLook L = plainLook(); L.outfit = art::Outfit::Leather; L.hair = art::Hair::Ponytail; L.backItem = 1; L.cloak = 1; L.cloakColor = rgba(70, 90, 60); v.push_back({"RANGER", L}); }
  { HumanLook L = plainLook(); L.hairColor = rgba(210, 210, 206); L.beard = true; L.cloak = 2; L.cloakColor = rgba(90, 70, 50); L.backItem = 2; v.push_back({"ELDER", L}); }
  { HumanLook L = plainLook(); L.facePaint = 3; L.tabardColor = rgba(40, 60, 140); L.hair = art::Hair::Mohawk; L.jewellery = 2; L.outfit = art::Outfit::Leather; v.push_back({"PAINT", L}); }
  { HumanLook L = plainLook(); L.facePaint = 4; L.hair = art::Hair::Braids; L.amulet = true; L.outfit = art::Outfit::Chain; v.push_back({"SUNMARK", L}); }
  return v;
}

// ---------------------------------------------------------------- portraits
void portraitSheet(const std::string& dir, const std::string& name, const std::vector<Look>& lk) {
  const int perRow = 8, cw = 4 * 26 + 6, ch = 36;
  const int rows = ((int)lk.size() + perRow - 1) / perRow;
  Board b(perRow * cw + 4, rows * ch + 4);
  for (size_t i = 0; i < lk.size(); i++) {
    const int x = 2 + (int)(i % perRow) * cw, y = 2 + (int)(i / perRow) * ch;
    panel(b, x, y, 4 * 26 + 2, 26);
    for (int m = 0; m < 4; m++) b.put(art::portraitSprite(lk[i].L, m), x + 2 + m * 26, y + 1);
    b.text(x, y + 27, lk[i].name.substr(0, 17));
  }
  savePng(b.c, dir + "/" + name + ".png", 4);
  savePng(b.c, dir + "/" + name + "_1x.png", 1);
}

// ---------------------------------------------------------------- hounds
struct HoundShot { std::string name; art::HoundLook L; };
std::vector<HoundShot> hounds() {
  std::vector<HoundShot> v;
  static const char* ages[3] = {"PUP", "HALF", "GROWN"};
  for (int sp = 0; sp < 2; sp++)
    for (int age = 0; age < 3; age++)
      for (int k = 0; k < (age == 2 ? 6 : 2); k++) {
        art::HoundLook L;
        L.species = (uint8_t)sp; L.age = (uint8_t)age; L.seed = (uint32_t)(k * 5 + age * 3 + sp * 17 + 1);
        L.collar = (uint8_t)((k + age) % 3);
        v.push_back({std::string(sp ? "WOLF " : "HOUND ") + ages[age] + " " + std::to_string(k), L});
      }
  return v;
}
void houndSheets(const std::string& dir) {
  const std::vector<HoundShot> hs = hounds();
  const int cw = art::HOUND_W, chh = art::HOUND_H;
  const int rowH = 4 * (chh + 1) + 12;
  Board b(90 + art::HOUND_FRAMES * (cw + 2) + 4 * 26 + 10, (int)hs.size() * rowH + 10);
  for (size_t i = 0; i < hs.size(); i++) {
    const int y0 = 6 + (int)i * rowH;
    b.text(2, y0 + 30, hs[i].name);
    const Canvas sh = art::houndSheet(hs[i].L);
    for (int row = 0; row < 4; row++)
      for (int f = 0; f < art::HOUND_FRAMES; f++) {
        Canvas cell = cellOf(sh, f, row == 3 ? 2 : row, cw, chh);
        if (row == 3) cell = flipped(cell);
        const int x = 90 + f * (cw + 2), y = y0 + row * (chh + 1);
        shadowAt(b, x + cw / 2, y + chh - 2, 6, 1);
        b.put(cell, x, y);
      }
    const int px = 90 + art::HOUND_FRAMES * (cw + 2) + 6;
    panel(b, px, y0 + 20, 4 * 26 + 2, 26);
    for (int m = 0; m < 4; m++) b.put(art::houndPortrait(hs[i].L, m), px + 2 + m * 26, y0 + 21);
  }
  savePng(b.c, dir + "/hounds.png", 3);
  savePng(b.c, dir + "/hounds_1x.png", 1);
}

// ---------------------------------------------------------------- graves
void graveSheet(const std::string& dir) {
  static const char* rites[5] = {"HEADSTONE", "CAIRN", "POST", "STANDING", "URN NICHE"};
  const int weapons[8] = {0, 1, 2, 3, 4, 5, 6, 8};
  Board b(80 + 8 * 30, 10 + 5 * 2 * 36);
  for (int r = 0; r < 5; r++)
    for (int v = 0; v < 2; v++) {
      const int y = 8 + (r * 2 + v) * 36;
      b.text(2, y + 12, rites[r]);
      b.text(2, y + 21, v ? "FRESH" : "OLD");
      for (int k = 0; k < 8; k++) {
        art::GraveLook g;
        g.rite = (uint8_t)r; g.weapon = (uint8_t)weapons[k]; g.fresh = v == 1; g.flowers = (k % 3) != 0; g.seed = (uint32_t)(r * 31 + k * 7 + v);
        const Canvas s = art::graveSprite(g);
        b.put(s, 80 + k * 30, y);
      }
    }
  savePng(b.c, dir + "/graves.png", 4);
  savePng(b.c, dir + "/graves_1x.png", 1);
}

// ---------------------------------------------------------------- postures
std::vector<Look> postureLooks() {
  std::vector<Look> v;
  std::vector<Look> c = cultureLooks();
  for (size_t i = 0; i < c.size(); i += 5) v.push_back(c[i]);
  std::vector<Look> k = kitLooks();
  for (int i : {2, 6, 9, 20, 24, 26, 31, 33, 35, 36, 37, 38}) if (i < (int)k.size()) v.push_back(k[i]);
  return v;
}
void postureSheet(const std::string& dir) {
  const std::vector<Look> lk = postureLooks();
  struct P { Posture p; uint8_t v; const char* n; };
  const P ps[3] = {{Posture::Downed, 0, "DOWNED"}, {Posture::Kneel, 0, "BIND"}, {Posture::Kneel, 1, "MOURN"}};
  const int colW = 4 * 2 * 16 + 10;
  Board b(80 + 3 * colW + 120, 14 + (int)lk.size() * 27 + 80);
  for (int i = 0; i < 3; i++) b.text(80 + i * colW, 2, ps[i].n);
  for (size_t li = 0; li < lk.size(); li++) {
    const int y = 12 + (int)li * 27;
    b.text(2, y + 9, lk[li].name.substr(0, 12));
    for (int i = 0; i < 3; i++) {
      const Canvas sh = art::humanPostureSheet(lk[li].L, ps[i].p, ps[i].v);
      for (int row = 0; row < 4; row++)
        for (int f = 0; f < 2; f++) {
          Canvas cell = cellOf(sh, f, row == 3 ? 2 : row, 16, 24);
          if (row == 3) cell = flipped(cell);
          const int x = 80 + i * colW + (row * 2 + f) * 16;
          shadowAt(b, x + 8, y + 22, 5, 1);
          b.put(cell, x, y);
        }
    }
  }
  // scenes: a companion downed, a friend kneeling to bind the wound; a mourner at a grave with a hound sitting by
  const int sy = 14 + (int)lk.size() * 27 + 8;
  {
    const int x0 = 90;
    const Canvas dn = cellOf(art::humanPostureSheet(lk[3].L, Posture::Downed, 0), 0, 2, 16, 24);
    const Canvas bd = cellOf(art::humanPostureSheet(lk[1].L, Posture::Kneel, 0), 1, 2, 16, 24);
    shadowAt(b, x0 + 8, sy + 22, 5, 1); shadowAt(b, x0 + 24, sy + 22, 5, 1);
    b.put(bd, x0, sy); b.put(flipped(dn), x0 + 16, sy);
  }
  {
    const int x0 = 160;
    art::GraveLook g; g.rite = 0; g.weapon = 1; g.flowers = true; g.seed = 5;
    b.put(art::graveSprite(g), x0, sy - 6);
    const Canvas mr = cellOf(art::humanPostureSheet(lk[5].L, Posture::Kneel, 1), 0, 1, 16, 24);
    shadowAt(b, x0 + 12, sy + 44, 5, 1);
    b.put(mr, x0 + 4, sy + 22);
    art::HoundLook h; h.species = 0; h.age = 2; h.seed = 3; h.collar = 2;
    const Canvas dog = cellOf(art::houndSheet(h), art::HF_SIT, 2, art::HOUND_W, art::HOUND_H);
    shadowAt(b, x0 + 36 + 14, sy + 30, 6, 1);
    b.put(flipped(dog), x0 + 36, sy + 10);
  }
  savePng(b.c, dir + "/postures.png", 4);
  savePng(b.c, dir + "/postures_1x.png", 1);
}

// ---------------------------------------------------------------- ruins
void ruinSheet(const std::string& dir) {
  // a hall's walls as chunkgen lays them: a north wall with a door, east and west walls running north-south, corners
  // left: the M1 painter (ruinVariant with the north join); right: ruinVariantRun with the run's joins
  const int tw = 9, th = 8;
  Board b(2 * (tw * 16 + 48) + 40, th * 16 + 64);
  auto wallAt = [&](int x, int y) {
    if (x < 0 || y < 0 || x >= tw || y >= th) return false;
    if (x == 2 && y >= 2 && y <= 4) return true;   // an inner north-south stub
    return (y == 0 || y == th - 1 || x == 0 || x == tw - 1) && !(y == 0 && x == 4) && !(y == th - 1 && x >= 3 && x <= 5) && !(x == tw - 1 && y == 3);
  };
  for (int side = 0; side < 2; side++)
    for (int ty = 0; ty < th; ty++)
      for (int tx = 0; tx < tw; tx++) {
        if (!wallAt(tx, ty)) continue;
        const uint32_t h = hash2(tx * 7 + 3, ty * 13 + 1) % 8;
        const int joins = (wallAt(tx, ty - 1) ? 1 : 0) | (wallAt(tx + 1, ty) ? 2 : 0) | (wallAt(tx, ty + 1) ? 4 : 0) | (wallAt(tx - 1, ty) ? 8 : 0);
        const Canvas s = side ? art::ruinVariantRun(art::Prop::RuinWall, (int)h, joins) : art::ruinVariant(art::Prop::RuinWall, (int)h | ((joins & 1) ? 8 : 0));
        b.put(s, 24 + side * (tw * 16 + 48) + tx * 16 + 8 - s.w / 2, 32 + ty * 16 + 16 - s.h);
      }
  savePng(b.c, dir + "/ruins.png", 4);
  savePng(b.c, dir + "/ruins_1x.png", 1);
}

// ================================================================ --check
int g_fail = 0, g_items = 0;
void fail(const std::string& what) { if (g_fail < 300) std::printf("FAIL %s\n", what.c_str()); g_fail++; }
bool rawBounds(const Canvas& c, int& x0, int& y0, int& x1, int& y1) {
  x0 = c.w; y0 = c.h; x1 = -1; y1 = -1;
  for (int y = 0; y < c.h; y++)
    for (int x = 0; x < c.w; x++)
      if (c.get(x, y) >> 24) { x0 = std::min(x0, x); y0 = std::min(y0, y); x1 = std::max(x1, x); y1 = std::max(y1, y); }
  return x1 >= 0;
}
bool outlined(const Canvas& raw, const Canvas& fin) {
  for (int y = 0; y < raw.h; y++)
    for (int x = 0; x < raw.w; x++) {
      if (!(raw.get(x, y) >> 24)) continue;
      static const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
      for (int k = 0; k < 4; k++) {
        const int nx = x + dx[k], ny = y + dy[k];
        if (nx < 0 || ny < 0 || nx >= raw.w || ny >= raw.h) continue;
        if (!(fin.get(nx, ny) >> 24)) return false;
      }
    }
  return true;
}
// raw inside the cell with a pixel's room on every side (the bottom row may be touched when bottomOk)
void checkCell(const std::string& id, const Canvas& raw, const Canvas& fin, bool bottomOk) {
  g_items++;
  int x0, y0, x1, y1;
  if (!rawBounds(raw, x0, y0, x1, y1)) { fail(id + ": empty"); return; }
  if (x0 < 1 || y0 < 1 || x1 > raw.w - 2 || (!bottomOk && y1 > raw.h - 2))
    fail(id + ": leaves the cell (" + std::to_string(x0) + "," + std::to_string(y0) + ")-(" + std::to_string(x1) + "," + std::to_string(y1) + ")");
  if (!outlined(raw, fin)) fail(id + ": not outlined");
  int n = 0;
  for (uint32_t p : raw.px) if (p >> 24) n++;
  if (n < 20) fail(id + ": only " + std::to_string(n) + " px");
}
int check() {
  std::vector<Look> lk = cultureLooks();
  for (auto& k : kitLooks()) lk.push_back(k);
  for (const Look& l : lk)
    for (int m = 0; m < 4; m++) {
      const Canvas raw = art::portraitRaw(l.L, m), fin = art::portraitSprite(l.L, m);
      checkCell("portrait " + l.name + " mood " + std::to_string(m), raw, fin, true);
      if (fin.px != art::portraitSprite(l.L, m).px) fail("portrait " + l.name + ": not deterministic");
      if (fin.w != art::PORTRAIT_W || fin.h != art::PORTRAIT_H) fail("portrait size");
    }
  // moods differ from calm
  for (const Look& l : lk)
    for (int m = 1; m < 4; m++)
      if (art::portraitSprite(l.L, m).px == art::portraitSprite(l.L, 0).px) fail("portrait " + l.name + ": mood " + std::to_string(m) + " looks calm");
  for (const HoundShot& h : hounds()) {
    const Canvas raw = art::houndSheetRaw(h.L), fin = art::houndSheet(h.L);
    if (fin.w != art::HOUND_W * art::HOUND_FRAMES || fin.h != art::HOUND_H * 3) fail(h.name + ": sheet size");
    for (int row = 0; row < 3; row++)
      for (int f = 0; f < art::HOUND_FRAMES; f++) {
        const Canvas r = cellOf(raw, f, row, art::HOUND_W, art::HOUND_H), fc = cellOf(fin, f, row, art::HOUND_W, art::HOUND_H);
        const std::string id = h.name + " row " + std::to_string(row) + " f" + std::to_string(f);
        checkCell(id, r, fc, false);
        int x0, y0, x1, y1;
        if (rawBounds(r, x0, y0, x1, y1) && (y1 < art::HOUND_H - 3 || y1 > art::HOUND_H - 2)) fail(id + ": not standing on its row (lowest " + std::to_string(y1) + ")");
      }
    if (fin.px != art::houndSheet(h.L).px) fail(h.name + ": not deterministic");
    for (int m = 0; m < 4; m++) {
      const Canvas p = art::houndPortrait(h.L, m);
      int x0, y0, x1, y1;
      g_items++;
      if (!rawBounds(p, x0, y0, x1, y1)) fail(h.name + ": empty portrait");
      if (p.px != art::houndPortrait(h.L, m).px) fail(h.name + ": portrait not deterministic");
    }
  }
  for (int r = 0; r < art::GRAVE_RITES; r++)
    for (int w : {0, 1, 2, 3, 4, 5, 6, 8, 9})
      for (int v = 0; v < 4; v++) {
        art::GraveLook g; g.rite = (uint8_t)r; g.weapon = (uint8_t)w; g.fresh = v & 1; g.flowers = (v & 2) != 0; g.seed = (uint32_t)(r * 13 + w * 5 + v);
        const Canvas raw = art::graveRaw(g), fin = art::graveSprite(g);
        const std::string id = "grave rite " + std::to_string(r) + " weapon " + std::to_string(w) + " v" + std::to_string(v);
        checkCell(id, raw, fin, false);
        if (fin.px != art::graveSprite(g).px) fail(id + ": not deterministic");
        if (fin.w != art::GRAVE_W || fin.h != art::GRAVE_H) fail("grave size");
      }
  // graveLookFor: every culture gets a valid rite
  for (int a = 0; a < (int)cult::Archetype::COUNT; a++) {
    const cult::Culture C = cult::Atlas::make((cult::Archetype)a, 99u + (uint32_t)a);
    const art::GraveLook g = art::graveLookFor(&C, 7);
    if (g.rite >= art::GRAVE_RITES) fail("graveLookFor: rite out of range");
  }
  if (art::graveLookFor(nullptr, 3).rite != 0) fail("graveLookFor(nullptr) is not a headstone");
  // the postures (all looks, both Kneel variants)
  for (const Look& l : lk)
    for (int pv = 0; pv < 3; pv++) {
      const Posture P = pv == 0 ? Posture::Downed : Posture::Kneel;
      const uint8_t var = pv == 2 ? 1 : 0;
      const Canvas sh = art::humanPostureSheet(l.L, P, var);
      for (int row = 0; row < 3; row++)
        for (int f = 0; f < 2; f++) {
          const Canvas raw = art::humanPostureCellRaw(l.L, P, var, row, f), fin = cellOf(sh, f, row, 16, 24);
          checkCell(l.name + (pv == 0 ? " DOWNED" : (pv == 1 ? " BIND" : " MOURN")) + " row " + std::to_string(row) + " f" + std::to_string(f), raw, fin, true);
        }
      if (sh.px != art::humanPostureSheet(l.L, P, var).px) fail(l.name + ": posture not deterministic");
    }
  std::printf("party_gallery --check: %d items, %d failures\n", g_items, g_fail);
  return g_fail ? 1 : 0;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc >= 2 && std::string(argv[1]) == "--check") return check();
  const std::string dir = argc >= 2 ? argv[1] : ".";
  portraitSheet(dir, "portraits_cultures", cultureLooks());
  portraitSheet(dir, "portraits_kit", kitLooks());
  houndSheets(dir);
  graveSheet(dir);
  postureSheet(dir);
  ruinSheet(dir);
  return 0;
}
