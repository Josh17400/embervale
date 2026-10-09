// Arms gallery (M6 Steel, ARMS lane): the culture arms and armour grammar on the hero, built the way the game builds it
// (gear::makeGear -> wearGear -> humanSheet), so what is judged here is what the paper doll and the world show.
//   arms_gallery <outDir>   writes (each also as *_1x.png for judging at game scale):
//     sets.png     one row pair per archetype (12): its best alloy kit with its polearm, then a steel kit with its blade
//                  and its bow on the back; 3 facings, walk, wind-up, strike, hurt, the left flip; its icon row
//     lineup.png   every archetype's alloy kit side by side (down / up / side idle): the 16 px silhouette check
//     tiers.png    the 4 universal tiers (leather, bronze, iron, steel) + the alloys of one culture, and a poor bandit
//     forms.png    every helm form, body form, shield form, blade, polearm and bow alone on a plain steel rig
//     sheens.png   the 8 sheens on one plate kit, plus the legendary glow frames
#include <initializer_list>
#include <vector>

#include "tools/preview/preview_util.h"
#include "rpg/culture/culture.h"
#include "rpg/sim/gear.h"
#include "rpg/sim/gear_look.h"

namespace {

using art::HumanLook;

constexpr int kArch = (int)cult::Archetype::COUNT;
std::vector<cult::Culture> g_cult;

const cult::Culture* lookup(uint64_t id) {
  for (const cult::Culture& c : g_cult)
    if (c.id == id) return &c;
  return nullptr;
}

Canvas cellOf(const Canvas& sheet, int row, int frame) {
  Canvas c(art::HUMAN_W, art::HUMAN_H);
  for (int y = 0; y < c.h; y++)
    for (int x = 0; x < c.w; x++) c.set(x, y, sheet.get(frame * art::HUMAN_W + x, row * art::HUMAN_H + y));
  return c;
}
Canvas flipped(const Canvas& s) {
  Canvas d(s.w, s.h);
  for (int y = 0; y < s.h; y++)
    for (int x = 0; x < s.w; x++) d.set(s.w - 1 - x, y, s.get(x, y));
  return d;
}
void putHero(Board& b, const Canvas& cell, int x, int y) {
  for (int j = -1; j <= 1; j++)
    for (int i = -5; i <= 5; i++) {
      if (i * i / 25.0f + j * j / 2.0f > 1.0f) continue;
      uint32_t p = b.c.get(x + 8 + i, y + 22 + j);
      b.c.set(x + 8 + i, y + 22 + j, art::shade(p, 0.72f));
    }
  b.put(cell, x, y);
}
struct Shot { int row, frame; bool flip = false; };
const Shot kAll[] = {{0, 0}, {0, 1}, {0, 3}, {0, 5}, {0, 6}, {0, 7}, {1, 0}, {1, 2}, {1, 5}, {1, 6},
                     {2, 0}, {2, 1}, {2, 2}, {2, 3}, {2, 5}, {2, 6}, {2, 7}, {2, 0, true}};
constexpr int kShots = (int)(sizeof kAll / sizeof kAll[0]);

void saveBoth(const Board& b, const std::string& dir, const std::string& name) {
  savePng(b.c, dir + "/" + name + ".png", 4);
  savePng(b.c, dir + "/" + name + "_1x.png", 1);
}

Appearance hero(int i) {
  Appearance a;
  static const uint32_t skins[4] = {rgba(236, 188, 146), rgba(190, 134, 94), rgba(250, 220, 190), rgba(124, 80, 56)};
  static const uint32_t hairs[4] = {rgba(120, 70, 36), rgba(40, 32, 36), rgba(220, 180, 100), rgba(150, 64, 36)};
  a.skin = skins[i & 3];
  a.hairColor = hairs[(i >> 1) & 3];
  a.hair = (uint8_t)(1 + i % 3);
  a.beard = (i % 3) == 0;
  return a;
}

struct Kit {
  std::vector<Item> items;   // stable storage for WornGear pointers
  WornGear w;
};
// a full culture kit: body, helmet, shield, gloves, boots, cloak, the weapon (sub), a bow on the back
Kit makeKit(const cult::Culture* c, Mat mat, uint8_t alloy, int weaponSub, Rarity rar, int ilvl, bool bow, bool shield = true) {
  Kit k;
  Rng r(0xA4A5u + (c ? (uint32_t)c->seed : 7u) * 31u + (uint32_t)mat * 7u + (uint32_t)weaponSub);
  const uint64_t cid = c ? c->id : 0;
  auto mk = [&](ItemKind kind, int sub, Mat m) {
    Item it = gear::makeGear(r, kind, sub, ilvl, rar, cid, m, alloy);
    return it;
  };
  k.items.reserve(10);
  k.items.push_back(mk(ItemKind::Armor, 0, mat));
  k.items.push_back(mk(ItemKind::Helmet, 0, mat));
  k.items.push_back(mk(ItemKind::Shield, 0, mat));
  k.items.push_back(mk(ItemKind::Gloves, 0, mat));
  k.items.push_back(mk(ItemKind::Boots, 0, mat));
  k.items.push_back(mk(ItemKind::Cloak, 0, Mat::Cloth));
  k.items.push_back(mk(ItemKind::Weapon, weaponSub, mat == Mat::Leather ? Mat::Iron : mat));
  k.items.push_back(mk(ItemKind::Bow, 0, Mat::Wood));
  if (c && !c->arms.cloth) k.items[5].tint = 0;
  else if (c) k.items[5].tint = c->arms.cloth | 0xFF000000u;
  k.w.armor = &k.items[0]; k.w.helmet = &k.items[1];
  if (shield) k.w.shield = &k.items[2];
  k.w.gloves = &k.items[3]; k.w.boots = &k.items[4]; k.w.cloak = &k.items[5]; k.w.weapon = &k.items[6];
  if (bow) k.w.bow = &k.items[7];
  return k;
}
HumanLook dress(const Kit& k, const cult::Culture* home, int heroI) {
  HumanLook L = appearanceLook(hero(heroI), home);
  wearGear(L, k.w, lookup);
  return L;
}
void iconRow(Board& b, const Kit& k, int x, int y) {
  for (size_t i = 0; i < k.items.size(); i++) {
    const Item& it = k.items[i];
    b.put(art::itemIconLook(gearIcon(it, lookup(it.culture))), x + (int)i * 18, y);
  }
}
void sheetRow(Board& b, const HumanLook& L, int x, int y) {
  const Canvas sheet = art::humanSheet(L);
  for (int i = 0; i < kShots; i++) {
    Canvas c = cellOf(sheet, kAll[i].row, kAll[i].frame);
    if (kAll[i].flip) c = flipped(c);
    putHero(b, c, x + i * 18, y);
  }
}
const char* formWord(int kind, int f) {
  static const char* helm[] = {"NASAL", "KETTLE", "GREAT", "SPANGEN", "HORNED", "PLUMED", "AVENTAIL", "MASKED", "CRESTED", "WINGED"};
  static const char* body[] = {"PADDED", "LEATHER", "MAIL", "SCALE", "LAMELLAR", "BRIGAND", "PLATE", "LEAF"};
  static const char* shield[] = {"ROUND", "KITE", "HEATER", "TOWER", "CRESCENT", "OVAL", "BUCKLER", "LEAF", "NONE"};
  static const char* blade[] = {"STRAIGHT", "LEAF", "FALCHION", "SCIMITAR", "KHOPESH", "WAVY", "BROAD", "CURVED", "GLAIVE"};
  static const char* pole[] = {"SPEAR", "GLAIVE", "HALBERD", "NONE"};
  static const char* bow[] = {"SELF", "RECURVE", "COMPOSITE", "CROSSBOW", "LONGBOW"};
  switch (kind) {
    case 0: return helm[std::clamp(f, 0, 9)];
    case 1: return body[std::clamp(f, 0, 7)];
    case 2: return shield[std::clamp(f, 0, 8)];
    case 3: return blade[std::clamp(f, 0, 8)];
    case 4: return pole[std::clamp(f, 0, 3)];
    default: return bow[std::clamp(f, 0, 4)];
  }
}

void sets(const std::string& dir) {
  const int rowH = 30;
  Board b(8 + 100 + kShots * 18 + 8, 8 + kArch * (2 * rowH + 24) + 8);
  int y = 6;
  for (int a = 0; a < kArch; a++) {
    const cult::Culture& C = g_cult[(size_t)a];
    const cult::ArmsStyle& A = C.arms;
    b.text(4, y, std::string(cult::archetypeName(C.archetype)) + "  " + C.name);
    char buf[160];
    std::snprintf(buf, sizeof buf, "%s/%s %s/%s %s %s %s %s P%d S%d C%d CAPE%d", formWord(0, (int)A.helm[0]), formWord(0, (int)A.helm[1]),
                  formWord(1, (int)A.body[0]), formWord(1, (int)A.body[1]), formWord(2, (int)A.shield), formWord(3, (int)A.blade),
                  formWord(4, (int)A.polearm), formWord(5, (int)A.bow), A.pauldron, A.skirt, A.crest, A.cape);
    b.text(4 + 140, y, buf, rgba(220, 230, 200));
    y += 10;
    const uint8_t best = (uint8_t)A.alloys.size();
    const Kit ka = makeKit(&C, best ? Mat::Alloy : Mat::Steel, best, (int)WeaponType::Spear, Rarity::Epic, 30, false);
    const Kit ks = makeKit(&C, Mat::Steel, 0, (int)WeaponType::Sword, Rarity::Rare, 20, true);
    b.text(4, y + 10, best ? A.alloys.back().name : "STEEL", rgba(250, 230, 160));
    sheetRow(b, dress(ka, &C, a), 104, y);
    y += rowH;
    b.text(4, y + 10, "STEEL");
    sheetRow(b, dress(ks, &C, a + 1), 104, y);
    y += rowH;
    iconRow(b, ka, 104, y);
    iconRow(b, ks, 104 + 8 * 18 + 10, y);
    y += 24;
  }
  saveBoth(b, dir, "sets");
}

void lineup(const std::string& dir) {
  Board b(8 + kArch * 58, 8 + 3 * 30 + 12);
  for (int a = 0; a < kArch; a++) {
    const cult::Culture& C = g_cult[(size_t)a];
    const uint8_t best = (uint8_t)C.arms.alloys.size();
    const Kit k = makeKit(&C, best ? Mat::Alloy : Mat::Steel, best, a & 1 ? (int)WeaponType::Sword : (int)WeaponType::Spear, Rarity::Rare, 30, false);
    const Kit s = makeKit(&C, Mat::Steel, 0, (int)WeaponType::Sword, Rarity::Rare, 20, false);
    const Canvas sh = art::humanSheet(dress(k, &C, a)), ss = art::humanSheet(dress(s, &C, a + 2));
    const int x = 6 + a * 58;
    b.text(x, 2, std::string(cult::archetypeName(C.archetype)).substr(0, 9), rgba(250, 240, 210));
    putHero(b, cellOf(sh, 0, 0), x, 12);
    putHero(b, cellOf(sh, 1, 0), x + 18, 12);
    putHero(b, cellOf(sh, 2, 0), x + 36, 12);
    putHero(b, cellOf(ss, 0, 0), x, 42);
    putHero(b, cellOf(ss, 1, 0), x + 18, 42);
    putHero(b, cellOf(ss, 2, 0), x + 36, 42);
    putHero(b, cellOf(sh, 0, 6), x, 72);
    putHero(b, cellOf(sh, 2, 6), x + 18, 72);
    putHero(b, cellOf(ss, 2, 1), x + 36, 72);
  }
  saveBoth(b, dir, "lineup");
}

void tiers(const std::string& dir) {
  const int rowH = 30;
  const cult::Culture& C = g_cult[(size_t)cult::Archetype::Heartland];
  const int nA = (int)C.arms.alloys.size();
  Board b(8 + 100 + kShots * 18 + 8, 8 + (6 + nA) * (rowH + 22) + 8);
  int y = 6;
  struct T { const char* n; Mat m; uint8_t al; const cult::Culture* c; int ilvl; Rarity r; };
  std::vector<T> ts = {{"LEATHER", Mat::Leather, 0, &C, 3, Rarity::Common}, {"BRONZE", Mat::Bronze, 0, &C, 5, Rarity::Common},
                       {"IRON", Mat::Iron, 0, &C, 10, Rarity::Common},     {"STEEL", Mat::Steel, 0, &C, 18, Rarity::Uncommon},
                       {"HEARTH IRON", Mat::Iron, 0, nullptr, 10, Rarity::Common}};
  for (int i = 0; i < nA; i++) ts.push_back({"ALLOY", Mat::Alloy, (uint8_t)(i + 1), &C, 30, Rarity::Rare});
  ts.push_back({"BANDIT", Mat::Leather, 0, &g_cult[(size_t)cult::Archetype::Steppe], 2, Rarity::Common});
  for (const T& t : ts) {
    const Kit k = makeKit(t.c, t.m, t.al, (int)WeaponType::Sword, t.r, t.ilvl, false, t.m != Mat::Leather);
    b.text(4, y + 10, t.al ? C.arms.alloys[(size_t)t.al - 1].name : t.n);
    sheetRow(b, dress(k, t.c, 2), 104, y);
    y += rowH;
    iconRow(b, k, 104, y);
    y += 22;
  }
  saveBoth(b, dir, "tiers");
}

HumanLook plain() {
  HumanLook L = appearanceLook(hero(0), nullptr);
  L.armorStyle = 3; L.helmStyle = 3; L.gloves = 3; L.boots = 3;
  L.bodyForm = (uint8_t)((int)cult::BodyArm::Mail + 1);
  L.pauldron = 2; L.skirt = 2; L.crest = 1;
  L.tabardColor = rgba(60, 90, 150);
  L.plumeColor = rgba(190, 50, 40);
  L.weapon = 1; L.weaponColor = rgba(200, 205, 215);
  return L;
}
// one form family on its own board, big enough to judge: per entry the down / up / side idle, then the down and side
// strike and the side walk
void formBoard(const std::string& dir, const std::string& name, const std::vector<std::pair<std::string, HumanLook>>& v) {
  const int cols = 3, cw = 6 * 17 + 8, rh = 36;
  const int rows = ((int)v.size() + cols - 1) / cols;
  Board b(8 + cols * cw, 6 + rows * rh);
  for (size_t i = 0; i < v.size(); i++) {
    const int x = 6 + (int)(i % cols) * cw, y = 4 + (int)(i / cols) * rh;
    b.text(x, y, v[i].first, rgba(240, 240, 220));
    const Canvas s = art::humanSheet(v[i].second);
    const Shot sh[6] = {{0, 0}, {1, 0}, {2, 0}, {0, 6}, {2, 6}, {2, 2}};
    for (int k = 0; k < 6; k++) putHero(b, cellOf(s, sh[k].row, sh[k].frame), x + k * 17, y + 9);
  }
  savePng(b.c, dir + "/" + name + ".png", 5);
  savePng(b.c, dir + "/" + name + "_1x.png", 1);
}
void forms(const std::string& dir) {
  std::vector<std::pair<std::string, HumanLook>> v;
  for (int f = 0; f < 10; f++) { HumanLook L = plain(); L.helmForm = (uint8_t)(f + 1); L.crest = 2; v.push_back({formWord(0, f), L}); }
  formBoard(dir, "f_helms", v); v.clear();
  for (int f = 0; f < 8; f++) { HumanLook L = plain(); L.helmStyle = 0; L.bodyForm = (uint8_t)(f + 1); L.pauldron = 3; L.skirt = 3; v.push_back({formWord(1, f), L}); }
  for (int f = 0; f < 4; f++) { HumanLook L = plain(); L.helmStyle = 0; L.bodyForm = (uint8_t)(f * 2 + 1); L.pauldron = (uint8_t)(f + 1); L.skirt = (uint8_t)(4 - f);
    L.armsOrnament = (uint16_t)(1 << (f * 2 + 1)); v.push_back({std::string("DIALS ") + std::to_string(f), L}); }
  formBoard(dir, "f_bodies", v); v.clear();
  for (int f = 0; f < 8; f++) { HumanLook L = plain(); L.helmStyle = 0; L.shieldForm = (uint8_t)(f + 1); L.trimColor = rgba(200, 205, 215); v.push_back({formWord(2, f), L}); }
  formBoard(dir, "f_shields", v); v.clear();
  for (int f = 0; f < 9; f++) { HumanLook L = plain(); L.helmStyle = 0; L.bladeForm = (uint8_t)(f + 1); v.push_back({formWord(3, f), L}); }
  formBoard(dir, "f_blades", v); v.clear();
  for (int f = 0; f < 9; f++) {
    HumanLook L = plain();
    L.helmStyle = 0;
    if (f < 4) { L.weapon = 8; L.polearmForm = (uint8_t)f; } else { L.weapon = 3; L.bowForm = (uint8_t)(f - 3); }
    v.push_back({f < 4 ? (f ? formWord(4, f - 1) : "M4 SPEAR") : formWord(5, f - 4), L});
  }
  for (int f = 0; f < 6; f++) {   // on the back
    HumanLook L = plain(); L.helmStyle = 0; L.weapon = 1; L.backItem = 1; L.bowForm = (uint8_t)f;
    v.push_back({std::string("BACK ") + (f ? formWord(5, f - 1) : "M0"), L});
  }
  formBoard(dir, "f_weapons", v);
}

void sheens(const std::string& dir) {
  const int rowH = 30;
  Board b(8 + 100 + kShots * 18 + 8, 8 + 10 * rowH + 8);
  int y = 6;
  static const uint32_t lt[8] = {rgba(150, 146, 140), rgba(206, 210, 218), rgba(120, 126, 140), rgba(176, 186, 200),
                                 rgba(150, 160, 190), rgba(120, 210, 170), rgba(226, 232, 244), rgba(226, 186, 104)};
  static const uint32_t dk[8] = {rgba(64, 60, 58), rgba(98, 104, 118), rgba(44, 46, 56), rgba(70, 78, 96),
                                 rgba(60, 56, 80), rgba(30, 90, 70), rgba(120, 130, 170), rgba(126, 86, 36)};
  static const char* nm[8] = {"MATTE", "BRIGHT", "DARK", "BANDED", "IRIDESCENT", "GLOWING", "PALE", "BURNISHED"};
  for (int s = 0; s < 8; s++) {
    HumanLook L = plain();
    L.bodyForm = (uint8_t)((int)cult::BodyArm::Plate + 1);
    L.helmForm = (uint8_t)((int)cult::HelmForm::GreatHelm + 1);
    L.shieldForm = (uint8_t)((int)cult::ShieldForm::Heater + 1);
    L.armourTint = lt[s] | 0xFF000000u; L.armourTint2 = dk[s] | 0xFF000000u; L.sheen = (uint8_t)(s + 1);
    b.text(4, y + 10, nm[s]);
    sheetRow(b, L, 104, y);
    y += rowH;
  }
  for (int g = 0; g < 2; g++) {
    HumanLook L = plain();
    L.bodyForm = (uint8_t)((int)cult::BodyArm::Scale + 1);
    L.cloak = 3; L.cloakColor = rgba(60, 40, 90);
    L.glow = g ? 31 : 3; L.glowColor = g ? rgba(140, 220, 255) : rgba(255, 190, 80);
    b.text(4, y + 10, g ? "GLOW ALL" : "GLOW W+B");
    sheetRow(b, L, 104, y);
    y += rowH;
  }
  saveBoth(b, dir, "sheens");
}

// every icon form in iron, steel, bronze, two alloys (banded, glowing) and leather; the last columns epic and legendary
void iconsBoard(const std::string& dir) {
  struct Row { art::Icon icon; int n; const char* name; };
  const Row rows[] = {{art::Icon::Helmet, 10, "HELMS"}, {art::Icon::Armor, 8, "BODIES"}, {art::Icon::Shield, 8, "SHIELDS"},
                      {art::Icon::Sword, 9, "SWORDS"}, {art::Icon::Greatsword, 9, "GREAT"}, {art::Icon::Dagger, 9, "DAGGERS"},
                      {art::Icon::Spear, 4, "SPEARS"}, {art::Icon::Bow, 6, "BOWS"}};
  struct Met { uint32_t t, t2; uint8_t sheen, mat; };
  const Met mets[4] = {{rgba(200, 208, 220), 0, 0, 4}, {rgba(214, 156, 88), rgba(104, 58, 34), 0, 2},
                       {rgba(176, 186, 200), rgba(70, 78, 96), 4, 5}, {rgba(120, 210, 170), rgba(30, 90, 70), 6, 5}};
  Board b(8 + 60 + 10 * 4 * 18, 8 + 8 * 20 + 30);
  int y = 4;
  for (const Row& r : rows) {
    b.text(4, y + 5, r.name);
    for (int m = 0; m < 4; m++)
      for (int f = 0; f < r.n; f++) {
        art::IconLook l;
        l.icon = r.icon; l.form = (uint8_t)f; l.tint = mets[m].t; l.tint2 = mets[m].t2; l.sheen = mets[m].sheen; l.mat = mets[m].mat;
        l.accent = m & 1 ? rgba(40, 70, 150) : rgba(150, 40, 40);
        l.ornament = (uint16_t)(m == 2 ? 4 : (m == 3 ? 16 : 0));
        l.rarity = (uint8_t)(f == r.n - 1 ? 4 : (f == r.n - 2 ? 3 : 1));
        b.put(art::itemIconLook(l), 64 + (m * 10 + f) * 18 - m * 0, y);
      }
    y += 20;
  }
  // classic vs the M6 stand-ins: spear, ingot (tints)
  for (int t = 0; t < 6; t++) {
    static const uint32_t tints[6] = {0, rgba(150, 150, 158), rgba(214, 156, 88), rgba(200, 208, 220), rgba(222, 186, 92), rgba(160, 90, 60)};
    b.put(art::itemIcon(art::Icon::Ingot, tints[t]), 64 + t * 18, y + 4);
    b.put(art::itemIcon(art::Icon::Spear, tints[t]), 64 + 120 + t * 18, y + 4);
  }
  savePng(b.c, dir + "/icons.png", 5);
  savePng(b.c, dir + "/icons_1x.png", 1);
}

}  // namespace

int main(int argc, char** argv) {
  std::string dir = argc > 1 ? argv[1] : ".";
  for (int a = 0; a < kArch; a++) {
    cult::Culture c = cult::Atlas::make((cult::Archetype)a, 1000u + (uint32_t)a * 17u);
    c.id = cult::familyId(a, 7);
    g_cult.push_back(std::move(c));
  }
  if (argc > 2 && std::string(argv[2]) == "--dump") {   // the arms palette of every archetype (judging aid)
    for (const cult::Culture& c : g_cult) {
      const cult::ArmsStyle& A = c.arms;
      std::printf("%-10s helm %d/%d shield %d metal %08x cloth %08x plume %08x leather %08x\n", cult::archetypeName(c.archetype),
                  (int)A.helm[0], (int)A.helm[1], (int)A.shield, A.metal, A.cloth, A.plume, A.leather);
      const Kit ks = makeKit(&c, Mat::Steel, 0, (int)WeaponType::Sword, Rarity::Rare, 20, false);
      const HumanLook L = dress(ks, &c, 0);
      std::printf("   steel look: helmStyle %d helmForm %d crest %d orn %04x body %d/%d shield %d field %08x device %08x tint %08x\n",
                  L.helmStyle, L.helmForm, L.crest, L.armsOrnament, L.armorStyle, L.bodyForm, L.shieldForm, L.shieldField, L.shieldDevice,
                  L.armourTint);
      for (const cult::Alloy& al : A.alloys)
        std::printf("   alloy %-12s sheen %d color %08x color2 %08x\n", al.name.c_str(), (int)al.sheen, al.color, al.color2);
    }
    return 0;
  }
  sets(dir);
  lineup(dir);
  tiers(dir);
  forms(dir);
  sheens(dir);
  iconsBoard(dir);
  return 0;
}
