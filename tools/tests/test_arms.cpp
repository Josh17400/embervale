// rpg_test --arms (M6 Steel, ARMS lane): the culture arms and armour looks.
//   - every archetype's kit (best alloy, and steel) gives its own HumanLook key, and the painted cells of any two
//     archetypes differ substantially in the head / torso (>= 40 pixels over the down and side idle cells)
//   - no transparent holes inside a figure (a transparent pixel the cell's border cannot reach)
//   - the M6 pieces (polearm and bow forms, alloy tint, glow) never paint on a cell's left / right / top edge
//   - pre-M6 looks keep their keys (golden keys) and their pixels when the M6 fields are 0; each M6 field moves the key
//   - wearGear on nothing equals appearanceLook; heartland classic pieces set no M6 field; bronze has its own metal;
//     poor pieces lose their ornament
//   - gearIcon / itemIconLook are deterministic, and culture pieces paint culture icons
// rpg_test --arms [--seeds A..B] [--print]   (--print: the golden keys, to refresh them)
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "tools/tests/tests.h"
#include "rpg/culture/culture.h"
#include "rpg/sim/gear.h"
#include "rpg/sim/gear_look.h"

// the icon painters (rpg_test links rpg_sim only; test_hero.cpp already compiles art_core and art_human in)
#include "rpg/art/art_items.cpp"

namespace {

int g_bad = 0;
const char* g_dump = nullptr;   // --dump DIR: failing cells as 8x PNGs (side by side with the bare look)
void dumpCell(const Canvas& a, const Canvas& b, const std::string& name) {
  if (!g_dump) return;
  const int S = 8, W = (a.w * 2 + 1) * S, H = a.h * S;
  std::vector<uint32_t> px((size_t)W * H, 0xFF406030u);
  for (int y = 0; y < H; y++)
    for (int x = 0; x < W; x++) {
      const int cx = x / S, cy = y / S;
      const Canvas& c = cx < a.w ? a : b;
      const int ix = cx < a.w ? cx : cx - a.w - 1;
      if (ix < 0) continue;
      const uint32_t v = c.get(ix, cy);
      if (v >> 24) px[(size_t)y * W + x] = v;
    }
  writePng((std::string(g_dump) + "/" + name + ".png").c_str(), W, H, px);
}
void fail(const std::string& s) {
  if (g_bad < 40) out("FAIL: arms: %s\n", s.c_str());
  g_bad++;
}

std::vector<cult::Culture> g_c;
const cult::Culture* lookup(uint64_t id) {
  for (const cult::Culture& c : g_c) if (c.id == id) return &c;
  return nullptr;
}

struct Kit { std::vector<Item> it; WornGear w; };
Kit kit(const cult::Culture* c, Mat mat, uint8_t alloy, int wsub, Rarity rar, int ilvl, uint32_t seed) {
  Kit k;
  Rng r(seed);
  const uint64_t id = c ? c->id : 0;
  for (ItemKind kd : {ItemKind::Armor, ItemKind::Helmet, ItemKind::Shield, ItemKind::Gloves, ItemKind::Boots, ItemKind::Cloak})
    k.it.push_back(gear::makeGear(r, kd, 0, ilvl, rar, id, kd == ItemKind::Cloak ? Mat::Cloth : mat, alloy));
  k.it.push_back(gear::makeGear(r, ItemKind::Weapon, wsub, ilvl, rar, id, mat == Mat::Leather ? Mat::Iron : mat, alloy));
  k.it.push_back(gear::makeGear(r, ItemKind::Bow, 0, ilvl, rar, id, Mat::Wood, 0));
  if (c && c->arms.cloth) k.it[5].tint = c->arms.cloth | 0xFF000000u;
  k.w.armor = &k.it[0]; k.w.helmet = &k.it[1]; k.w.shield = &k.it[2]; k.w.gloves = &k.it[3]; k.w.boots = &k.it[4];
  k.w.cloak = &k.it[5]; k.w.weapon = &k.it[6]; k.w.bow = &k.it[7];
  return k;
}
art::HumanLook dressed(const Kit& k, const cult::Culture* home) {
  Appearance a;
  art::HumanLook L = appearanceLook(a, home);
  wearGear(L, k.w, lookup);
  return L;
}

// holes the armour opens: pixels inside the figure (all four neighbours painted) left transparent where the same
// person unarmoured is painted. (Gaps between an arm, a weapon and the body are negative space, and the outline fills
// them; what must never happen is armour punching through to the ground.) where: the first one, for the report
int holes(const Canvas& c, const Canvas& bare, int* where = nullptr) {
  int n = 0;
  for (int y = 10; y < c.h - 1; y++)   // the body armour (the head's gaps around a raised weapon are the weapon's)
    for (int x = 1; x < c.w - 1; x++)
      if (!(c.get(x, y) >> 24) && (bare.get(x, y) >> 24) && (c.get(x - 1, y) >> 24) && (c.get(x + 1, y) >> 24) && (c.get(x, y - 1) >> 24) &&
          (c.get(x, y + 1) >> 24)) {
        if (!n && where) *where = y * 100 + x;
        n++;
      }
  return n;
}
// pixels the M6 fields add on a cell's left, right or top edge (against the same look without them)
int m6EdgePixels(const art::HumanLook& L) {
  art::HumanLook B = L;
  B.polearmForm = B.bowForm = B.sheen = B.glow = 0;
  B.armourTint = B.armourTint2 = B.glowColor = 0;
  int n = 0;
  for (int row = 0; row < 3; row++)
    for (int f = 0; f < art::HUMAN_FRAMES; f++) {
      const Canvas a = art::humanCellRaw(L, row, f), b = art::humanCellRaw(B, row, f);
      const bool bobUp = row == 2 ? (f == 2 || f == 4) : (f == 1 || f == 3);
      for (int y = 0; y < art::HUMAN_H; y++)
        for (int x = 0; x < art::HUMAN_W; x++) {
          const bool edge = x == 0 || x == art::HUMAN_W - 1 || (y == 0 && !bobUp);
          if (edge && (a.get(x, y) >> 24) && !(b.get(x, y) >> 24)) {
            if (!n) out("  (first new edge pixel: facing %d frame %d at x %d y %d)\n", row, f, x, y);
            n++;
          }
        }
    }
  return n;
}
int diffPixels(const art::HumanLook& a, const art::HumanLook& b) {
  int n = 0;
  for (int row : {0, 2}) {
    const Canvas ca = art::humanCellRaw(a, row, 0), cb = art::humanCellRaw(b, row, 0);
    for (int y = 0; y < 18; y++)   // the head and the torso
      for (int x = 0; x < art::HUMAN_W; x++) n += ca.get(x, y) != cb.get(x, y);
  }
  return n;
}

// pre-M6 looks (fixed, M0..M5 fields only) and their keys, frozen when M6 began: they must never move
art::HumanLook oldLook(int i) {
  art::HumanLook L;
  L.hair = (art::Hair)(i % 8);
  L.outfit = (art::Outfit)(i % 10);
  L.weapon = (uint8_t)(i % 7);
  L.helmStyle = (uint8_t)(i % 8); L.armorStyle = (uint8_t)((i * 3) % 8); L.boots = (uint8_t)(i % 4); L.cloak = (uint8_t)(i % 4);
  if (i >= 3) { L.people = (uint8_t)(i % 3); L.cut = (uint8_t)(i % 9); L.helmForm = (uint8_t)(i % 11); L.bodyForm = (uint8_t)(i % 9);
                L.shieldForm = (uint8_t)(i % 10); L.bladeForm = (uint8_t)(i % 10); L.armsOrnament = (uint16_t)(i * 37 % 1024);
                L.pauldron = (uint8_t)(i % 5); L.crest = (uint8_t)(i % 5); L.plumeColor = rgba(200, 40, 40); }
  return L;
}
const uint64_t kOldKeys[6] = {0xc9aaacdeea0d1475ull, 0x46b731532091d98cull, 0xd40e030f3b3c6607ull,
                              0x67641a868e3d9a61ull, 0x9702ab7a303bd69cull, 0x3f268663d4c12633ull};

int cmdArms(int argc, char** argv) {
  bool print = false;
  uint64_t A = 1, B = 3;
  for (int i = 2; i < argc; i++) {
    if (!std::strcmp(argv[i], "--print")) print = true;
    else if (!std::strcmp(argv[i], "--dump") && i + 1 < argc) g_dump = argv[++i];
    else if (!std::strcmp(argv[i], "--seeds") && i + 1 < argc) std::sscanf(argv[++i], "%llu..%llu", (unsigned long long*)&A, (unsigned long long*)&B);
  }
  g_bad = 0;
  const int nA = (int)cult::Archetype::COUNT;

  // ---- golden keys of pre-M6 looks; their pixels with every M6 field at 0; each M6 field moves the key
  for (int i = 0; i < 6; i++) {
    const art::HumanLook L = oldLook(i);
    if (print) std::printf("  old look %d key 0x%016llxull\n", i, (unsigned long long)L.key());
    else if (kOldKeys[i] && L.key() != kOldKeys[i]) fail("pre-M6 look " + std::to_string(i) + " changed its key");
    for (int f = 0; f < 7; f++) {
      art::HumanLook M = L;
      switch (f) {
        case 0: M.polearmForm = 2; break; case 1: M.bowForm = 3; break; case 2: M.armourTint = rgba(1, 2, 3); break;
        case 3: M.armourTint2 = rgba(1, 2, 3); break; case 4: M.sheen = 4; break; case 5: M.glow = 2; break; default: M.glowColor = 7; break;
      }
      if (M.key() == L.key()) fail("M6 field " + std::to_string(f) + " does not move the key");
    }
  }

  for (uint64_t seed = A; seed <= B; seed++) {
    g_c.clear();
    for (int a = 0; a < nA; a++) {
      cult::Culture c = cult::Atlas::make((cult::Archetype)a, (uint32_t)(seed * 7717u + (uint64_t)a * 31u));
      c.id = cult::familyId(a, (int32_t)seed);
      g_c.push_back(std::move(c));
    }
    // ---- 1. kits: distinct keys, distinct pixels, no holes, no new edge pixels
    std::vector<art::HumanLook> best, steel;
    for (int a = 0; a < nA; a++) {
      const cult::Culture& C = g_c[(size_t)a];
      const uint8_t nal = (uint8_t)C.arms.alloys.size();
      best.push_back(dressed(kit(&C, nal ? Mat::Alloy : Mat::Steel, nal, (int)WeaponType::Spear, Rarity::Legendary, 30, (uint32_t)seed), &C));
      steel.push_back(dressed(kit(&C, Mat::Steel, 0, (int)WeaponType::Sword, Rarity::Rare, 20, (uint32_t)seed + 9), &C));
      for (const art::HumanLook* L : {&best.back(), &steel.back()}) {
        if (!L->helmForm || !L->bodyForm) fail(std::string(cult::archetypeName(C.archetype)) + " kit carries no culture forms");
        for (int row = 0; row < 3; row++)
          for (int f = 0; f < art::HUMAN_FRAMES; f++)
            {
              int at = 0;
              art::HumanLook bare = *L;   // the same person and weapon, no armour
              bare.helmStyle = bare.armorStyle = bare.gloves = bare.boots = bare.cloak = bare.shieldStyle = bare.helmForm = bare.bodyForm = 0;
              bare.shieldForm = bare.pauldron = bare.skirt = bare.crest = 0;
              bare.armsOrnament = 0; bare.armourTint = 0; bare.sheen = 0; bare.glow = 0;
              bare.cut = 0; bare.pattern = 0;   // shirt and trousers: the legs as the armour's
              if (const int h = holes(art::humanCellRaw(*L, row, f), art::humanCellRaw(bare, row, f), &at)) {
                dumpCell(art::humanCellRaw(*L, row, f), art::humanCellRaw(bare, row, f),
                         std::string("hole_") + cult::archetypeName(C.archetype) + "_" + std::to_string(row) + "_" + std::to_string(f));
                fail(std::string(cult::archetypeName(C.archetype)) + " kit: " + std::to_string(h) + " armour holes (facing " + std::to_string(row) +
                     ", frame " + std::to_string(f) + ", first at x " + std::to_string(at % 100) + " y " + std::to_string(at / 100) + ")");
              }
            }
        if (const int e = m6EdgePixels(*L)) fail(std::string(cult::archetypeName(C.archetype)) + " kit: " + std::to_string(e) + " new edge pixels");
      }
      if (nal && !best.back().armourTint) fail(std::string(cult::archetypeName(C.archetype)) + " alloy kit has no alloy tint");
      if (!(best.back().glow & 2)) fail("a legendary body does not glow");
    }
    for (int a = 0; a < nA; a++)
      for (int b = a + 1; b < nA; b++) {
        const std::string pr = std::string(cult::archetypeName((cult::Archetype)a)) + "/" + cult::archetypeName((cult::Archetype)b);
        if (best[a].key() == best[b].key()) fail("alloy kits share a key: " + pr);
        if (steel[a].key() == steel[b].key()) fail("steel kits share a key: " + pr);
        const int d1 = diffPixels(best[a], best[b]), d2 = diffPixels(steel[a], steel[b]);
        if (d1 < 40) fail("alloy kits look alike (" + std::to_string(d1) + " px): " + pr);
        if (d2 < 40) fail("steel kits look alike (" + std::to_string(d2) + " px): " + pr);
      }
    // every polearm and bow form, on every facing, stays in the cell
    for (int f = 1; f <= 8; f++) {
      art::HumanLook L = steel[0];
      if (f <= 3) { L.weapon = 8; L.polearmForm = (uint8_t)f; }
      else { L.weapon = 1; L.backItem = 1; L.bowForm = (uint8_t)(f - 3); }
      if (const int e = m6EdgePixels(L)) fail("form " + std::to_string(f) + ": " + std::to_string(e) + " new edge pixels");
      L.weapon = 3;
      if (f > 3) if (const int e = m6EdgePixels(L)) fail("bow form " + std::to_string(f - 3) + " in hand: " + std::to_string(e) + " new edge pixels");
    }

    // ---- 2. the mapping rules
    {
      Appearance app;
      const art::HumanLook base = appearanceLook(app, &g_c[0]);
      art::HumanLook L = base;
      wearGear(L, WornGear{}, lookup);
      if (L.key() != base.key()) fail("wearGear on nothing changed the look");
      // heartland classic pieces: no M6 field
      Rng r(seed * 31 + 5);
      std::vector<Item> cl = {makeArmor(r, 10, ItemKind::Armor), makeArmor(r, 10, ItemKind::Helmet), makeArmor(r, 10, ItemKind::Shield),
                              makeWeapon(r, 10, 0), makeBow(r, 10)};
      WornGear w;
      w.armor = &cl[0]; w.helmet = &cl[1]; w.shield = &cl[2]; w.weapon = &cl[3]; w.bow = &cl[4];
      art::HumanLook H = base;
      wearGear(H, w, lookup);
      if (H.armourTint || H.sheen || H.polearmForm || H.bowForm || H.helmForm || H.bodyForm || H.shieldForm || H.bladeForm)
        fail("heartland classic pieces set culture / M6 fields");
      // bronze has its own metal, not the gilded band
      const Kit kb = kit(&g_c[2], Mat::Bronze, 0, (int)WeaponType::Sword, Rarity::Common, 8, (uint32_t)seed);
      const Kit ki = kit(&g_c[2], Mat::Iron, 0, (int)WeaponType::Sword, Rarity::Common, 8, (uint32_t)seed);
      const art::HumanLook Lb = dressed(kb, nullptr), Li = dressed(ki, nullptr);
      if (!Lb.armourTint || Lb.armorStyle == 4) fail("bronze armour has no bronze metal");
      // (M6 fixer r2) a culture's iron keeps the iron band and wears its smiths' finish matte (never a bright metal)
      if (Li.armorStyle != 2 || (Li.armourTint && Li.sheen != (uint8_t)((int)cult::Sheen::Matte + 1))) fail("iron armour is not the iron band");
      if (diffPixels(Lb, Li) < 40) fail("bronze and iron look alike");
      const Kit kl = kit(&g_c[2], Mat::Leather, 0, (int)WeaponType::Sword, Rarity::Common, 2, (uint32_t)seed);
      const art::HumanLook Ll = dressed(kl, nullptr);
      if (Ll.armorStyle != 1 || Ll.helmStyle != 1) fail("leather pieces are not the leather band");
      if (Ll.bodyForm != (int)cult::BodyArm::Leather + 1 && Ll.bodyForm != (int)cult::BodyArm::Padded + 1) fail("leather body takes a metal form");
      for (int a = 0; a < nA; a++) {   // poor pieces lose their ornament and crest
        const Kit kp = kit(&g_c[(size_t)a], Mat::Leather, 0, (int)WeaponType::Sword, Rarity::Common, 2, (uint32_t)seed + a);
        const art::HumanLook P = dressed(kp, nullptr);
        if (P.armsOrnament & ~(cult::ARM_FUR_TRIM | cult::ARM_STUDS | cult::ARM_TASSELS)) fail("a poor kit keeps rich ornament");
        if (P.crest > 1) fail("a poor helmet keeps its crest");
      }
    }

    // ---- 3. icons: deterministic, and culture pieces paint culture icons
    for (int a = 0; a < nA; a++) {
      const cult::Culture& C = g_c[(size_t)a];
      const Kit k = kit(&C, Mat::Alloy, (uint8_t)C.arms.alloys.size(), (int)WeaponType::Spear, Rarity::Epic, 30, (uint32_t)seed);
      for (const Item& it : k.it) {
        const art::IconLook l1 = gearIcon(it, &C), l2 = gearIcon(it, &C);
        if (l1.key() != l2.key()) fail("gearIcon is not deterministic");
        const Canvas c1 = art::itemIconLook(l1), c2 = art::itemIconLook(l2);
        if (c1.px != c2.px) fail("itemIconLook is not deterministic");
        if (c1.w != art::ICON || c1.h != art::ICON) fail("an icon is not 16x16");
        if (it.kind == ItemKind::Armor || it.kind == ItemKind::Helmet || it.kind == ItemKind::Weapon) {
          if (!l1.form) fail(std::string("a culture ") + (it.kind == ItemKind::Armor ? "body" : it.kind == ItemKind::Helmet ? "helmet" : "spear") + " icon has no form");
          if (c1.px == art::itemIcon(it.icon, it.tint).px) fail("a culture piece paints the classic icon");
        }
      }
    }
    // the classic icon is untouched when the look carries nothing new
    for (int i = 0; i < (int)art::Icon::COUNT; i++) {
      art::IconLook l;
      l.icon = (art::Icon)i;
      l.tint = rgba(120, 200, 110);
      if (art::itemIconLook(l).px != art::itemIcon(l.icon, l.tint).px) fail("a plain IconLook is not the classic icon");
    }
  }
  out("arms: %s (%d failures)\n", g_bad ? "FAILED" : "ALL OK", g_bad);
  return g_bad ? 1 : 0;
}

}  // namespace

RPG_TEST_CMD("--arms", "M6 culture arms and armour looks: distinct kits, holes, edges, keys, mapping, icons [--seeds A..B] [--print]", cmdArms);
