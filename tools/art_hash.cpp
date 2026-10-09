// EMBERVALE art hash: prints one FNV-1a hash per art category (and a total) over a fixed, deterministic set of
// sprites. Used to prove a refactor is pixel-identical, and to see which categories a change touched.
//   art_hash            -> per-category lines + "TOTAL <hash>"
//   art_hash -v         -> one line per sprite
// No SDL window is opened; only the CPU painters run.
// The main categories (and TOTAL) cover the sprites that existed before M0, using the pre-M0 enum sizes, so appending
// enum values never moves them; anything appended later is hashed in the "+new" lines (not part of TOTAL).
// Pre-M0 baseline: humans 3a7e04166da3888e monsters 861869e52656df48 props 8c9c5159f8c5843c buildings aca5bc7c4bef688c
//                  walls cd867c5f650fd69b icons 0fd3e91f6e4dca99 fx e0ecd0fcb344c159 TOTAL 46d7a9fe07d4f7b0
#include <cstdint>
#include "rpg/build/blueprint.h"
#include <cstdio>
#include <cstring>
#include <string>

#include "engine/pix.h"
#include "rpg/art.h"

namespace {
bool g_verbose = false;

struct H {
  uint64_t h = 1469598103934665603ull;
  void byte(uint8_t b) { h ^= b; h *= 1099511628211ull; }
  void u32(uint32_t v) { for (int i = 0; i < 4; i++) byte((uint8_t)(v >> (i * 8))); }
  void canvas(const Canvas& c) {
    u32((uint32_t)c.w); u32((uint32_t)c.h);
    for (uint32_t p : c.px) u32(p);
  }
};

struct Cat {
  const char* name;
  H h;
  int n = 0;
  void add(const Canvas& c, const std::string& what) {
    h.canvas(c);
    n++;
    if (g_verbose) {
      H one; one.canvas(c);
      std::printf("  %-10s %-28s %3dx%-3d %016llx\n", name, what.c_str(), c.w, c.h, (unsigned long long)one.h);
    }
  }
};

// pre-M0 enum sizes
constexpr int kOutfits = 10, kHairs = 6, kMonsters = 16, kProps = 74, kBuildings = 10, kIcons = 35, kFx = 12;
}  // namespace

int main(int argc, char** argv) {
  for (int i = 1; i < argc; i++) if (!std::strcmp(argv[i], "-v")) g_verbose = true;
  using namespace art;
  Cat humans{"humans"}, monsters{"monsters"}, props{"props"}, buildings{"buildings"}, walls{"walls"}, icons{"icons"}, fx{"fx"};
  Cat humansNew{"humans+new"}, monstersNew{"monst+new"}, propsNew{"props+new"}, buildingsNew{"bldgs+new"}, iconsNew{"icons+new"},
      fxNew{"fx+new"};

  // humans: every outfit x a few hair/flag/weapon combinations
  for (int o = 0; o < kOutfits; o++)
    for (int v = 0; v < 4; v++) {
      HumanLook L;
      L.outfit = (Outfit)o;
      L.hair = (Hair)((o + v) % kHairs);
      L.beard = v & 1;
      L.helmet = v == 2;
      L.hood = v == 3 && o == (int)Outfit::Robe;
      L.cape = v == 1;
      L.shield = v >= 2;
      L.weapon = (uint8_t)((o + v) % 7);
      L.topColor = rgba(40 + o * 17, 90 + v * 30, 160 - o * 9);
      humans.add(humanSheet(L), "outfit" + std::to_string(o) + "/v" + std::to_string(v));
    }
  for (int o = kOutfits; o < (int)Outfit::COUNT; o++) {
    HumanLook L;
    L.outfit = (Outfit)o;
    humansNew.add(humanSheet(L), "outfit" + std::to_string(o));
  }
  for (int h = kHairs; h < (int)Hair::COUNT; h++) {
    HumanLook L;
    L.hair = (Hair)h;
    humansNew.add(humanSheet(L), "hair" + std::to_string(h));
  }
  // M6 Steel: the culture arms grammar (polearms, bows, alloy sheens, legendary glow) and the culture icons
  for (int s = 0; s <= 8; s++) {
    HumanLook L;
    L.armorStyle = L.helmStyle = L.gloves = L.boots = 3;
    L.bodyForm = (uint8_t)(1 + s % 8); L.helmForm = (uint8_t)(1 + s); L.shieldForm = (uint8_t)(1 + s % 8);
    L.pauldron = (uint8_t)(s % 5); L.skirt = (uint8_t)((s + 2) % 5); L.crest = (uint8_t)(s % 4);
    L.weapon = s < 3 ? 8 : (s < 6 ? 3 : 1);
    L.polearmForm = (uint8_t)(1 + s % 3); L.bowForm = (uint8_t)(1 + s % 5); L.backItem = s >= 6 ? 1 : 0;
    if (s) { L.armourTint = rgba(180, 150 + s * 8, 120); L.armourTint2 = rgba(70, 50, 40 + s * 6); L.sheen = (uint8_t)s; }
    if (s == 8) { L.glow = 31; L.glowColor = rgba(140, 220, 255); L.cloak = 1; }
    humansNew.add(humanSheet(L), "m6arms" + std::to_string(s));
  }
  for (int f = 0; f <= 10; f++) {
    static const Icon ic[6] = {Icon::Helmet, Icon::Armor, Icon::Shield, Icon::Sword, Icon::Spear, Icon::Bow};
    for (int k = 0; k < 6; k++) {
      IconLook l;
      l.icon = ic[k]; l.form = (uint8_t)f; l.tint = rgba(214, 156, 88); l.tint2 = rgba(104, 58, 34);
      l.sheen = (uint8_t)(f % 9); l.ornament = (uint16_t)(f * 53 % 1024); l.accent = rgba(40, 70, 150); l.rarity = (uint8_t)(f % 5);
      iconsNew.add(itemIconLook(l), "m6icon" + std::to_string(k) + "/" + std::to_string(f));
    }
  }
  for (int m = 0; m < (int)Monster::COUNT; m++) (m < kMonsters ? monsters : monstersNew).add(monsterSheet((Monster)m), "monster" + std::to_string(m));
  for (int p = 0; p < (int)Prop::COUNT; p++) (p < kProps ? props : propsNew).add(propSprite((Prop)p), "prop" + std::to_string(p));
  for (int b = 0; b < (int)Building::COUNT; b++) {
    static const int sizes[4][2] = {{4, 3}, {5, 4}, {6, 4}, {7, 5}};
    for (int s = 0; s < 4; s++)
      for (int roof = 0; roof < 2; roof++)
        (b < kBuildings ? buildings : buildingsNew)
            .add(buildingSprite(bld::design(bld::classicRequest((Building)b, sizes[s][0], sizes[s][1], roof ? rgba(150, 60, 50) : 0, 1234u + s * 77u))),
                 "bldg" + std::to_string(b) + "/" + std::to_string(sizes[s][0]) + "x" + std::to_string(sizes[s][1]) + (roof ? "/tint" : ""));
  }
  for (int m = 0; m < 16; m++) walls.add(wallPiece(m), "wall" + std::to_string(m));
  walls.add(gatePiece(), "gate");
  for (int i = 0; i < (int)Icon::COUNT; i++) {
    Cat& c = i < kIcons ? icons : iconsNew;
    c.add(itemIcon((Icon)i), "icon" + std::to_string(i));
    c.add(itemIcon((Icon)i, rgba(120, 200, 110)), "icon" + std::to_string(i) + "/tint");
  }
  for (int f = 0; f < (int)Fx::COUNT; f++) (f < kFx ? fx : fxNew).add(fxSprite((Fx)f), "fx" + std::to_string(f));

  H total;
  for (Cat* c : {&humans, &monsters, &props, &buildings, &walls, &icons, &fx}) {
    std::printf("%-10s %4d sprites  %016llx\n", c->name, c->n, (unsigned long long)c->h.h);
    total.u32((uint32_t)c->h.h); total.u32((uint32_t)(c->h.h >> 32));
  }
  std::printf("TOTAL %016llx\n", (unsigned long long)total.h);
  for (Cat* c : {&humansNew, &monstersNew, &propsNew, &buildingsNew, &iconsNew, &fxNew})
    if (c->n) std::printf("%-10s %4d sprites  %016llx\n", c->name, c->n, (unsigned long long)c->h.h);
  return 0;
}
