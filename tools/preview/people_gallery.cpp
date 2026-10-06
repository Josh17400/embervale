// People gallery (M3 PEOPLE lane): the peoples, the dress grammar and the culture arms on the human rig.
//   people_gallery <outDir>   writes
//     cultures.png / cultures_1x.png   12 archetypes (cult::Atlas::make) x villager M/F, guard, priest, noble, bandit x
//                                      human, half-breed, elf, each in 3 facings (the census look: rpg/sim/looks.cpp)
//     culture_<NAME>.png               one archetype at 4x (the same figures, larger)
//     grammar.png / grammar_1x.png     every cut, headwear, pattern, face paint, helm form, body form, shield and blade
//     walk.png                         the walk and attack frames of a few looks (peoples x arms) in all facings
#include <cctype>
#include <initializer_list>

#include "rpg/culture/culture.h"
#include "rpg/sim/world.h"
#include "tools/preview/preview_util.h"

// rpg/sim/looks.cpp (the census): dress a person in a culture
namespace census {
struct Rank { bool royal = false, capital = false; };
void dress(art::HumanLook& L, std::string& name, Role r, bool female, const cult::Culture& C, const cult::Culture& owner,
           uint64_t seed, const Rank& rank);
}  // namespace census

namespace {

using art::HumanLook;

Canvas cellOf(const Canvas& sheet, int row, int frame) {
  Canvas c(art::HUMAN_W, art::HUMAN_H);
  for (int y = 0; y < c.h; y++)
    for (int x = 0; x < c.w; x++) c.set(x, y, sheet.get(frame * art::HUMAN_W + x, row * art::HUMAN_H + y));
  return c;
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
// three facings side by side (down, up, side)
int put3(Board& b, const HumanLook& L, int x, int y, int frame = 0) {
  Canvas sheet = art::humanSheet(L);
  for (int f = 0; f < 3; f++) putHero(b, cellOf(sheet, f, frame), x + f * 16, y);
  return x + 48;
}

enum class Who { VillagerM, VillagerF, Guard, Priest, Noble, Bandit, COUNT };
const char* const kWho[] = {"MAN", "WOMAN", "GUARD", "PRIEST", "NOBLE", "BANDIT"};

// makeLook's base look per role (rpg/sim/looks.cpp), without the random draws: a neutral person to dress
HumanLook baseLook(Who w, const cult::Culture& C, bool& female, Role& role) {
  HumanLook L;
  female = w == Who::VillagerF || w == Who::Priest;
  L.hair = female ? art::Hair::Long : art::Hair::Short;
  role = Role::Villager;
  switch (w) {
    case Who::Guard:
      role = Role::Guard;
      L.outfit = art::Outfit::Guard; L.helmet = true; L.shield = true; L.weapon = 1;
      L.tabardColor = C.heraldry.field ? C.heraldry.field : rgba(150, 40, 40);
      break;
    case Who::Priest:
      role = Role::Priest;
      L.outfit = art::Outfit::Robe; L.topColor = rgba(232, 234, 244); L.tabardColor = rgba(150, 34, 48);
      break;
    case Who::Noble:
      role = Role::King;
      L.outfit = art::Outfit::Plate; L.armorStyle = 4; L.helmStyle = 4; L.helmet = true; L.cloak = 3; L.weapon = 1;
      L.weaponColor = rgba(236, 206, 120); L.trimColor = rgba(236, 196, 92); L.amulet = true; L.ring = true;
      L.tabardColor = C.heraldry.field ? C.heraldry.field : rgba(130, 30, 40); L.cloakColor = L.tabardColor;
      break;
    case Who::Bandit:
      role = Role::Bandit;
      L.outfit = art::Outfit::Leather; L.tabardColor = rgba(80, 60, 50); L.weapon = 1;
      break;
    default: break;
  }
  return L;
}

HumanLook dressed(Who w, const cult::Culture& C, int people, uint64_t seed) {
  bool female = false;
  Role role = Role::Villager;
  HumanLook L = baseLook(w, C, female, role);
  std::string name;
  census::Rank rank;
  census::dress(L, name, role, female, C, C, seed, rank);
  L.people = (uint8_t)people;
  if (people == 2) L.beard = false;
  return L;
}

void saveBoth(const Board& b, const std::string& dir, const std::string& name, int scale = 4) {
  savePng(b.c, dir + "/" + name + ".png", scale);
  savePng(b.c, dir + "/" + name + "_1x.png", 1);
}

void cultures(const std::string& dir) {
  const int nA = (int)cult::Archetype::COUNT, nW = (int)Who::COUNT;
  const int cellW = 3 * 16 + 4;
  Board b(80 + nW * 3 * cellW, 14 + nA * 30);
  for (int p = 0; p < 3; p++)
    for (int w = 0; w < nW; w++) b.text(80 + (w * 3 + p) * cellW, 2, std::string(kWho[w]).substr(0, 3) + (p == 0 ? "" : p == 1 ? "/H" : "/E"));
  for (int a = 0; a < nA; a++) {
    const cult::Culture C = cult::Atlas::make((cult::Archetype)a, 1234u + (uint32_t)a * 7919u);
    const int y = 12 + a * 30;
    b.text(2, y + 9, cult::archetypeName((cult::Archetype)a));
    for (int w = 0; w < nW; w++)
      for (int p = 0; p < 3; p++) put3(b, dressed((Who)w, C, p, 77u + (uint64_t)a * 131 + (uint64_t)w * 17 + (uint64_t)p), 80 + (w * 3 + p) * cellW, y);
  }
  saveBoth(b, dir, "cultures", 2);
  // each archetype at 4x: 6 roles across, the 3 peoples down
  for (int a = 0; a < nA; a++) {
    const cult::Culture C = cult::Atlas::make((cult::Archetype)a, 1234u + (uint32_t)a * 7919u);
    Board one(8 + nW * cellW, 12 + 3 * 28);
    one.text(2, 2, cult::archetypeName((cult::Archetype)a));
    for (int p = 0; p < 3; p++)
      for (int w = 0; w < nW; w++) put3(one, dressed((Who)w, C, p, 77u + (uint64_t)a * 131 + (uint64_t)w * 17 + (uint64_t)p), 4 + w * cellW, 12 + p * 28);
    std::string nm = cult::archetypeName((cult::Archetype)a);
    for (char& ch : nm) ch = (char)std::tolower((unsigned char)ch);
    savePng(one.c, dir + "/culture_" + nm + ".png", 4);
  }
}

// every grammar element alone, in 3 facings
void grammar(const std::string& dir) {
  Board b(700, 520);
  int y = 2;
  auto rowLabel = [&](const char* s) { b.text(2, y + 9, s); };
  auto base = []() {
    HumanLook L;
    L.skin = rgba(222, 170, 126); L.hairColor = rgba(90, 56, 30); L.topColor = rgba(70, 110, 150); L.bottomColor = rgba(78, 60, 44);
    L.tabardColor = rgba(160, 40, 40);
    return L;
  };
  // peoples (bare head, long hair, a hood) and builds
  rowLabel("PEOPLES");
  {
    int x = 70;
    for (int p = 0; p < 3; p++)
      for (art::Hair hr : {art::Hair::Short, art::Hair::Long, art::Hair::Bald}) {
        HumanLook L = base(); L.people = (uint8_t)p; L.hair = hr;
        x = put3(b, L, x, y) + 4;
      }
  }
  y += 28;
  rowLabel("CUTS");
  {
    int x = 70;
    for (int c = 1; c <= 8; c++) { HumanLook L = base(); L.cut = (uint8_t)c; L.patternColor = rgba(220, 190, 90); x = put3(b, L, x, y) + 4; }
  }
  y += 28;
  rowLabel("PATTERN");
  {
    int x = 70;
    for (int p = 1; p <= 6; p++) { HumanLook L = base(); L.cut = (uint8_t)(p == 1 ? 3 : (p == 2 ? 6 : 2)); L.pattern = (uint8_t)p; L.patternColor = rgba(230, 200, 100); x = put3(b, L, x, y) + 4; }
  }
  y += 28;
  rowLabel("HEADWEAR");
  {
    int x = 70;
    for (int h = 1; h <= 8; h++) { HumanLook L = base(); L.headwear = (uint8_t)h; L.headColor = h == 4 ? rgba(120, 90, 66) : rgba(200, 180, 140); x = put3(b, L, x, y) + 4; }
  }
  y += 28;
  rowLabel("ELF HATS");
  {
    int x = 70;
    for (int h = 1; h <= 8; h++) { HumanLook L = base(); L.people = 2; L.headwear = (uint8_t)h; L.headColor = rgba(110, 150, 90); x = put3(b, L, x, y) + 4; }
  }
  y += 28;
  rowLabel("PAINT/JWL");
  {
    int x = 70;
    for (int f = 1; f <= 4; f++) { HumanLook L = base(); L.facePaint = (uint8_t)f; x = put3(b, L, x, y) + 4; }
    for (int j = 1; j <= 3; j++) { HumanLook L = base(); L.jewellery = (uint8_t)j; L.hair = art::Hair::Bald; L.people = 1; x = put3(b, L, x, y) + 4; }
  }
  y += 28;
  rowLabel("HELMS");
  {
    int x = 70;
    for (int h = 1; h <= 10; h++) {
      HumanLook L = base(); L.helmStyle = 2; L.helmForm = (uint8_t)h; L.armorStyle = 2; L.plumeColor = rgba(190, 40, 40);
      x = put3(b, L, x, y) + 4;
    }
  }
  y += 28;
  rowLabel("HELM STL");
  {
    int x = 70;
    for (int h = 1; h <= 10; h++) {
      HumanLook L = base(); L.helmStyle = 3; L.helmForm = (uint8_t)h; L.armorStyle = 3; L.plumeColor = rgba(40, 90, 190); L.crest = 4; L.armsOrnament = 4;
      x = put3(b, L, x, y) + 4;
    }
  }
  y += 28;
  rowLabel("BODIES");
  {
    int x = 70;
    for (int f = 1; f <= 8; f++) {
      HumanLook L = base(); L.armorStyle = f == 8 ? 4 : 2; L.bodyForm = (uint8_t)f; L.pauldron = 3; L.skirt = 3; L.boots = 2; L.gloves = 2;
      x = put3(b, L, x, y) + 4;
    }
  }
  y += 28;
  rowLabel("GUARDS");
  {
    int x = 70;
    for (int f = 1; f <= 8; f++) {
      HumanLook L = base(); L.outfit = art::Outfit::Guard; L.armorStyle = 2; L.bodyForm = (uint8_t)f; L.pauldron = (uint8_t)(1 + f % 4); L.skirt = (uint8_t)(1 + (f + 1) % 4);
      L.helmStyle = 2; L.helmForm = (uint8_t)f; L.weapon = 1; L.shieldForm = (uint8_t)f; L.bladeForm = (uint8_t)f; L.tabardColor = rgba(40, 70, 150);
      x = put3(b, L, x, y) + 4;
    }
  }
  y += 28;
  rowLabel("ORNAMENT");
  {
    int x = 70;
    for (int o = 0; o < 10; o++) {
      HumanLook L = base(); L.armorStyle = 3; L.bodyForm = 7; L.helmStyle = 3; L.helmForm = 1; L.pauldron = 4; L.skirt = 3; L.armsOrnament = (uint16_t)(1u << o);
      L.plumeColor = rgba(200, 50, 40);
      x = put3(b, L, x, y) + 4;
    }
  }
  y += 28;
  rowLabel("SHIELDS");
  {
    int x = 70;
    for (int s = 1; s <= 8; s++) {
      HumanLook L = base(); L.armorStyle = 2; L.shieldForm = (uint8_t)s; L.weapon = 1; L.tabardColor = rgba(170, 40, 40); L.trimColor = rgba(220, 190, 90);
      x = put3(b, L, x, y) + 4;
    }
  }
  y += 28;
  rowLabel("BLADES");
  {
    int x = 70;
    for (int bl = 1; bl <= 9; bl++) {
      HumanLook L = base(); L.weapon = 1; L.bladeForm = (uint8_t)bl; L.weaponColor = rgba(210, 214, 224);
      Canvas sheet = art::humanSheet(L);
      for (int f : {0, 5, 6}) { putHero(b, cellOf(sheet, 0, f), x, y); x += 16; }
      for (int f : {0, 6}) { putHero(b, cellOf(sheet, 2, f), x, y); x += 16; }
      x += 4;
    }
  }
  y += 28;
  saveBoth(b, dir, "grammar", 4);
}

void walk(const std::string& dir) {
  Board b(70 + 8 * 18 * 3, 8 + 6 * 28);
  int y = 4;
  const cult::Culture A = cult::Atlas::make(cult::Archetype::Starspire, 99);
  const cult::Culture B = cult::Atlas::make(cult::Archetype::Steppe, 99);
  const cult::Culture J = cult::Atlas::make(cult::Archetype::Jade, 99);
  HumanLook looks[6] = {dressed(Who::VillagerF, A, 2, 5), dressed(Who::Guard, A, 2, 6), dressed(Who::VillagerM, B, 1, 7),
                        dressed(Who::Guard, B, 0, 8), dressed(Who::Guard, J, 0, 9), dressed(Who::VillagerM, J, 0, 10)};
  const char* nm[6] = {"ELF F", "ELF GRD", "HB STEPPE", "STEPPE G", "JADE G", "JADE M"};
  for (int i = 0; i < 6; i++, y += 28) {
    b.text(2, y + 9, nm[i]);
    Canvas sheet = art::humanSheet(looks[i]);
    for (int r = 0; r < 3; r++)
      for (int f = 0; f < 8; f++) putHero(b, cellOf(sheet, r, f), 70 + (r * 8 + f) * 18, y);
  }
  saveBoth(b, dir, "walk", 4);
}

// "edges": which single M3 feature paints on a cell's left, right or (level-frame) top edge (the 1px outline must fit)
void edges() {
  auto count = [](const HumanLook& L, const HumanLook& B) {
    int n = 0;
    for (int row = 0; row < 3; row++)
      for (int f = 0; f < art::HUMAN_FRAMES; f++) {
        Canvas a = art::humanCellRaw(L, row, f), b = art::humanCellRaw(B, row, f);
        const bool bobUp = row == 2 ? (f == 2 || f == 4) : (f == 1 || f == 3);
        for (int y = 0; y < art::HUMAN_H; y++)
          for (int x = 0; x < art::HUMAN_W; x++) {
            const bool edge = x == 0 || x == art::HUMAN_W - 1 || (y == 0 && !bobUp);
            if (edge && (a.get(x, y) >> 24) && !(b.get(x, y) >> 24)) { n++; if (n == 1) std::printf("    first at row %d frame %d x %d y %d\n", row, f, x, y); }
          }
      }
    return n;
  };
  for (int w = 0; w <= 6; w++)
    for (int p = 0; p < 3; p++) {
      HumanLook B; B.weapon = (uint8_t)w; B.shield = true; B.people = (uint8_t)p;
      auto test = [&](const char* what, int v, HumanLook L) { if (int n = count(L, B)) std::printf("weapon %d people %d %s %d: %d\n", w, p, what, v, n); };
      for (int v = 1; v <= 8; v++) { HumanLook L = B; L.people = (uint8_t)p; L.cut = (uint8_t)v; test("cut", v, L); }
      for (int v = 1; v <= 8; v++) { HumanLook L = B; L.people = (uint8_t)p; L.headwear = (uint8_t)v; test("headwear", v, L); }
      for (int v = 1; v <= 10; v++) { HumanLook L = B; L.people = (uint8_t)p; L.helmStyle = 2; L.helmForm = (uint8_t)v; HumanLook B2 = B; B2.helmStyle = 2; if (int n = count(L, B2)) std::printf("weapon %d people %d helm %d: %d\n", w, p, v, n); }
      for (int v = 1; v <= 8; v++) for (int d = 1; d <= 4; d++) { HumanLook L = B; L.people = (uint8_t)p; L.armorStyle = 2; L.bodyForm = (uint8_t)v; L.pauldron = (uint8_t)d; L.skirt = (uint8_t)d; HumanLook B2 = B; B2.armorStyle = 2; if (int n = count(L, B2)) std::printf("weapon %d people %d body %d dial %d: %d\n", w, p, v, d, n); }
      for (int v = 1; v <= 8; v++) { HumanLook L = B; L.people = (uint8_t)p; L.shieldForm = (uint8_t)v; test("shield", v, L); }
      if (w == 1) for (int v = 1; v <= 9; v++) { HumanLook L = B; L.people = (uint8_t)p; L.bladeForm = (uint8_t)v; test("blade", v, L); }
      for (int v = 0; v < 10; v++) { HumanLook L = B; L.people = (uint8_t)p; L.helmStyle = 2; L.armorStyle = 2; L.bodyForm = 7; L.armsOrnament = (uint16_t)(1u << v); L.pauldron = 4; HumanLook B2 = B; B2.helmStyle = 2; B2.armorStyle = 2; if (int n = count(L, B2)) std::printf("weapon %d people %d ornament %d: %d\n", w, p, v, n); }
      { HumanLook L = B; L.people = (uint8_t)p; test("people", p, L); }
    }
}

}  // namespace

int main(int argc, char** argv) {
  std::string dir = argc > 1 ? argv[1] : ".";
  std::string only = argc > 2 ? argv[2] : "";
  if (only == "edges") { edges(); return 0; }
  if (only.empty() || only == "grammar") grammar(dir);
  if (only.empty() || only == "cultures") cultures(dir);
  if (only.empty() || only == "walk") walk(dir);
  return 0;
}
