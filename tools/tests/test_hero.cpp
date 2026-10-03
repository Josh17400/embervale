// rpg_test lane checks: the hero (equipment looks, paper doll, character creator, backgrounds). M0 hero lane.
// Called once per seed after runSeed; returns failures, reports each with out("FAIL: ...").
//   - every equippable kind changes its own look field (and only the fields that belong to it)
//   - equip then unequip restores the look exactly (same key)
//   - HumanLook::key() separates different looks and agrees on equal ones
//   - every gear / appearance combination renders inside its 16x24 cells (nothing touches a cell's edge)
//   - the chosen Appearance reaches the player's look; item bands map onto look bands
//   - the M0 slots drop as loot and are named, iconed and priced
#include <cstring>
#include <string>
#include <vector>

#include "tools/tests/tests.h"

// rpg_test links only rpg_sim (headless), but these checks need the human painter and HumanLook::key(). Until the
// build links rpg_art into rpg_test, compile the two painter files into this unit. When it does, define
// EMB_RPG_TEST_HAS_ART for rpg_test (or delete these lines) so the symbols are not defined twice.
#ifndef EMB_RPG_TEST_HAS_ART
#include "rpg/art/art_core.cpp"
#include "rpg/art/art_human.cpp"
#endif

namespace {

// every look field as a list, so two looks can be compared field by field (index = field id below)
std::vector<uint64_t> fields(const art::HumanLook& L) {
  return {L.skin, L.hairColor, L.topColor, L.bottomColor, L.tabardColor, (uint64_t)L.hair, (uint64_t)L.outfit, L.beard, L.helmet, L.hood,
          L.cape, L.shield, L.weapon, L.weaponColor, L.build, L.eyeColor, L.helmStyle, L.armorStyle, L.gloves, L.boots, L.cloak,
          L.cloakColor, L.shieldStyle, L.backItem, L.amulet, L.trimColor, L.ring};
}
enum F { fSkin, fHairC, fTop, fBottom, fTabard, fHair, fOutfit, fBeard, fHelmet, fHood, fCape, fShield, fWeapon, fWeaponC, fBuild,
         fEye, fHelmStyle, fArmorStyle, fGloves, fBoots, fCloak, fCloakC, fShieldStyle, fBackItem, fAmulet, fTrim, fRing };

// a bare game with just the player (no world): enough for the inventory, recalcPlayer and the look
struct Bare {
  Game g;
  explicit Bare(uint64_t seed) : g(seed) {
    Actor p;
    p.player = true; p.human = true; p.faction = Faction::Player;
    g.actors.push_back(p);
    refresh();
  }
  void refresh() { g.inv.push_back(makeFood(0)); g.dropItem((int)g.inv.size() - 1); }   // dropItem() runs recalcPlayer()
  int add(const Item& it) { g.inv.push_back(it); return (int)g.inv.size() - 1; }
  const art::HumanLook& look() const { return g.pl().look; }
};

Item itemOf(ItemKind k, Rng& r, int level) {
  switch (k) {
    case ItemKind::Weapon: return makeWeapon(r, level);
    case ItemKind::Bow: return makeBow(r, level);
    case ItemKind::Staff: return makeStaff(r, level);
    case ItemKind::Ring: case ItemKind::Amulet: { Item j = makeJewel(r, level); j.kind = k; return j; }
    default: return makeArmor(r, level, k);
  }
}

// which look fields an equipped kind may change (its own field plus its colour)
std::vector<int> ownFields(ItemKind k) {
  switch (k) {
    case ItemKind::Weapon: case ItemKind::Bow: case ItemKind::Staff: return {fWeapon, fWeaponC};
    case ItemKind::Armor: return {fArmorStyle};
    case ItemKind::Helmet: return {fHelmStyle};
    case ItemKind::Shield: return {fShieldStyle, fTrim};
    case ItemKind::Ring: return {fRing};
    case ItemKind::Amulet: return {fAmulet};
    case ItemKind::Gloves: return {fGloves};
    case ItemKind::Boots: return {fBoots};
    case ItemKind::Cloak: return {fCloak, fCloakC};
    default: return {};
  }
}
int mainField(ItemKind k) { return ownFields(k).empty() ? -1 : ownFields(k)[0]; }

// Every cell keeps what M0 paints off its left, right and top edges, so the 1px outline always fits (the bottom
// row is the ground line). Measured on the cells before the outline, against the same look stripped of everything M0
// added, because a few legacy weapon poses (the axe head held low) already touch the edge and NPC looks must not change.
int newEdgePixels(const art::HumanLook& L) {
  art::HumanLook B = L;
  B.helmStyle = B.armorStyle = B.gloves = B.boots = B.cloak = B.shieldStyle = B.backItem = B.build = 0;
  B.amulet = B.ring = false;
  B.eyeColor = 0;
  if (B.hair >= art::Hair::Bun) B.hair = art::Hair::Short;
  int n = 0;
  for (int row = 0; row < 3; row++)
    for (int f = 0; f < art::HUMAN_FRAMES; f++) {
      Canvas a = art::humanCellRaw(L, row, f), b = art::humanCellRaw(B, row, f);
      for (int y = 0; y < art::HUMAN_H; y++)
        for (int x = 0; x < art::HUMAN_W; x++) {
          // on the walk frames that bob up a pixel (front/back 1 and 3, side 2 and 4) a helmet's dome may touch the
          // top row; what matters is that nothing sits there on the level frames (it would be cut off when bobbing)
          bool bobUp = row == 2 ? (f == 2 || f == 4) : (f == 1 || f == 3);
          bool edge = x == 0 || x == art::HUMAN_W - 1 || (y == 0 && !bobUp);
          if (!edge) continue;
          if ((a.get(x, y) >> 24) && !(b.get(x, y) >> 24)) n++;
        }
    }
  return n;
}

art::HumanLook randomLook(Rng& r) {
  art::HumanLook L;
  L.skin = rgba(90 + r.irange(160), 60 + r.irange(160), 40 + r.irange(140));
  L.hairColor = rgba(r.irange(256), r.irange(256), r.irange(256));
  L.topColor = rgba(r.irange(256), r.irange(256), r.irange(256));
  L.bottomColor = rgba(r.irange(256), r.irange(256), r.irange(256));
  L.hair = (art::Hair)r.irange((int)art::Hair::COUNT);
  L.beard = r.f() < 0.4f;
  L.build = (uint8_t)r.irange(3);
  L.eyeColor = r.f() < 0.5f ? 0 : rgba(r.irange(256), r.irange(256), r.irange(256));
  L.helmStyle = (uint8_t)r.irange(art::HumanLook::kBands + 1);
  L.armorStyle = (uint8_t)r.irange(art::HumanLook::kBands + 1);
  L.gloves = (uint8_t)r.irange(art::HumanLook::kBands + 1);
  L.boots = (uint8_t)r.irange(art::HumanLook::kBands + 1);
  L.cloak = (uint8_t)r.irange(art::HumanLook::kCloaks + 1);
  L.cloakColor = rgba(r.irange(256), r.irange(256), r.irange(256));
  L.shieldStyle = (uint8_t)r.irange(art::HumanLook::kShields + 1);
  L.trimColor = r.f() < 0.5f ? 0 : rgba(r.irange(256), r.irange(256), r.irange(256));
  static const uint8_t weapons[] = {0, 1, 2, 3, 4, 5, 6};
  L.weapon = weapons[r.irange(7)];
  L.backItem = (uint8_t)r.irange(4);
  L.amulet = r.f() < 0.5f;
  L.ring = r.f() < 0.5f;
  return L;
}

}  // namespace

int heroChecks(uint64_t seed) {
  int bad = 0;
  Rng r(seed * 1000003ull + 77);

  // ---- 1. the shirt-only hero, then every equippable kind alone: it changes its own field, and taking it off
  //         restores the exact look
  {
    Bare b(seed);
    const art::HumanLook base = b.look();
    if (base.weapon || base.helmStyle || base.armorStyle || base.gloves || base.boots || base.cloak || base.shieldStyle || base.backItem ||
        base.amulet || base.ring || base.cape || base.shield || base.helmet) {
      out("FAIL: hero: the unequipped look is not shirt-only");
      bad++;
    }
    const ItemKind kinds[] = {ItemKind::Weapon, ItemKind::Bow, ItemKind::Staff, ItemKind::Armor, ItemKind::Helmet, ItemKind::Shield,
                              ItemKind::Ring, ItemKind::Amulet, ItemKind::Gloves, ItemKind::Boots, ItemKind::Cloak};
    for (ItemKind k : kinds) {
      if (!itemEquippable(k) || !b.g.equipSlot(k)) { out("FAIL: hero: kind %d is not equippable", (int)k); bad++; continue; }
      Item it = itemOf(k, r, 1 + r.irange(30));
      int idx = b.add(it);
      b.g.useItem(idx);
      if (*b.g.equipSlot(k) != idx) { out("FAIL: hero: equipping kind %d did not fill its slot", (int)k); bad++; }
      std::vector<uint64_t> f0 = fields(base), f1 = fields(b.look());
      std::vector<int> own = ownFields(k);
      int mf = mainField(k);
      if (mf < 0 || f0[mf] == f1[mf]) { out("FAIL: hero: equipping %s left its look field unchanged", it.name.c_str()); bad++; }
      for (size_t i = 0; i < f0.size(); i++) {
        bool allowed = false;
        for (int o : own) allowed |= (int)i == o;
        if (!allowed && f0[i] != f1[i]) { out("FAIL: hero: equipping %s changed look field %d too", it.name.c_str(), (int)i); bad++; }
      }
      if (b.look().key() == base.key()) { out("FAIL: hero: equipping %s did not change the look key", it.name.c_str()); bad++; }
      b.g.useItem(idx);   // take it off again
      if (b.look().key() != base.key() || fields(b.look()) != fields(base)) { out("FAIL: hero: unequipping %s did not restore the look", it.name.c_str()); bad++; }
      b.g.dropItem(idx);
    }
    // a melee weapon in hand puts the bow and the staff on the back
    int sw = b.add(makeWeapon(r, 3, (int)WeaponType::Sword, false)), bw = b.add(makeBow(r, 3)), st = b.add(makeStaff(r, 3));
    b.g.useItem(bw);
    if (b.look().weapon != 3 || b.look().backItem) { out("FAIL: hero: a lone bow should be in hand"); bad++; }
    b.g.useItem(sw);
    b.g.useItem(st);
    if (b.look().weapon != 1 || b.look().backItem != 3) { out("FAIL: hero: sword in hand should carry bow+staff on the back (got %d/%d)", b.look().weapon, b.look().backItem); bad++; }
    for (int i : {sw, bw, st}) b.g.useItem(i);   // all off again
    // a full random kit on and off round-trips too
    std::vector<int> worn;
    for (ItemKind k : kinds) { int i = b.add(itemOf(k, r, 1 + r.irange(30))); worn.push_back(i); }
    for (int i : worn) if (*b.g.equipSlot(b.g.inv[i].kind) != i) b.g.useItem(i);
    for (int i : worn) if (*b.g.equipSlot(b.g.inv[i].kind) == i) b.g.useItem(i);
    if (b.look().key() != base.key()) { out("FAIL: hero: a full kit on and off did not restore the look"); bad++; }
  }

  // ---- 2. item tiers map onto look bands (leather 1, iron 2, steel 3 ... emberforged 7)
  {
    Bare b(seed);
    for (int t = 0; t <= 5; t++)
      for (ItemKind k : {ItemKind::Armor, ItemKind::Helmet, ItemKind::Gloves, ItemKind::Boots}) {
        Item it = makeArmor(r, 1, k);
        it.tier = (uint8_t)t;
        it.name = std::string(t == 0 ? "IRON" : tierName(t)) + " PIECE";
        int idx = b.add(it);
        b.g.useItem(idx);
        const art::HumanLook& L = b.look();
        int got = k == ItemKind::Armor ? L.armorStyle : k == ItemKind::Helmet ? L.helmStyle : k == ItemKind::Gloves ? L.gloves : L.boots;
        if (got != t + 2) { out("FAIL: hero: tier %d %s mapped to band %d", t, it.name.c_str(), got); bad++; }
        b.g.dropItem(idx);
      }
    Item lea = makeArmor(r, 1, ItemKind::Boots);
    lea.tier = 0; lea.name = "LEATHER BOOTS";
    int idx = b.add(lea);
    b.g.useItem(idx);
    if (b.look().boots != 1) { out("FAIL: hero: leather boots should be band 1"); bad++; }
  }

  // ---- 3. the appearance reaches the look
  {
    Bare b(seed);
    Appearance& a = b.g.app;
    a.skin = rgba(124, 80, 56); a.skinTone = 6; a.hair = (uint8_t)art::Hair::Curls; a.hairColor = rgba(220, 180, 100);
    a.beard = true; a.eyeColor = rgba(56, 140, 80); a.build = 2; a.topColor = rgba(150, 50, 44); a.bottomColor = rgba(60, 70, 96);
    b.refresh();
    const art::HumanLook& L = b.look();
    if (L.skin != a.skin || L.hairColor != a.hairColor || L.hair != art::Hair::Curls || !L.beard || L.eyeColor != a.eyeColor ||
        L.build != 2 || L.topColor != a.topColor || L.bottomColor != a.bottomColor) {
      out("FAIL: hero: the appearance did not reach the player's look");
      bad++;
    }
    a.hair = 200;   // out of range (a save from a newer build): falls back to short hair, never out of the enum
    b.refresh();
    if (b.look().hair != art::Hair::Short) { out("FAIL: hero: an unknown hair id was not clamped"); bad++; }
  }

  // ---- 4. key(): different looks get different keys, equal looks equal keys
  {
    std::vector<art::HumanLook> looks;
    for (int i = 0; i < 300; i++) looks.push_back(randomLook(r));
    for (size_t i = 0; i < looks.size(); i++)
      for (size_t j = i + 1; j < looks.size(); j++) {
        bool same = fields(looks[i]) == fields(looks[j]);
        if (same != (looks[i].key() == looks[j].key())) { out("FAIL: hero: key() %s for looks %zu and %zu", same ? "differs" : "collides", i, j); bad++; }
      }
    // each single-field change moves the key
    art::HumanLook base = randomLook(r);
    for (int fi = 0; fi < (int)fields(base).size(); fi++) {
      art::HumanLook L = base;
      switch (fi) {
        case fSkin: L.skin ^= 1; break;               case fHairC: L.hairColor ^= 1; break;
        case fTop: L.topColor ^= 1; break;            case fBottom: L.bottomColor ^= 1; break;
        case fTabard: L.tabardColor ^= 1; break;      case fHair: L.hair = (art::Hair)(((int)L.hair + 1) % (int)art::Hair::COUNT); break;
        case fOutfit: L.outfit = (art::Outfit)(((int)L.outfit + 1) % (int)art::Outfit::COUNT); break;
        case fBeard: L.beard = !L.beard; break;       case fHelmet: L.helmet = !L.helmet; break;
        case fHood: L.hood = !L.hood; break;          case fCape: L.cape = !L.cape; break;
        case fShield: L.shield = !L.shield; break;    case fWeapon: L.weapon = (uint8_t)((L.weapon + 1) % 7); break;
        case fWeaponC: L.weaponColor ^= 1; break;     case fBuild: L.build = (uint8_t)((L.build + 1) % 3); break;
        case fEye: L.eyeColor ^= 1; break;            case fHelmStyle: L.helmStyle ^= 1; break;
        case fArmorStyle: L.armorStyle ^= 1; break;   case fGloves: L.gloves ^= 1; break;
        case fBoots: L.boots ^= 1; break;             case fCloak: L.cloak ^= 1; break;
        case fCloakC: L.cloakColor ^= 1; break;       case fShieldStyle: L.shieldStyle ^= 1; break;
        case fBackItem: L.backItem ^= 1; break;       case fAmulet: L.amulet = !L.amulet; break;
        case fTrim: L.trimColor ^= 1; break;          case fRing: L.ring = !L.ring; break;
        default: break;
      }
      if (L.key() == base.key()) { out("FAIL: hero: changing look field %d does not change key()", fi); bad++; }
    }
  }

  // ---- 5. every combination stays inside its cells: all bands x every slot, random mixes, hairs x builds
  {
    int edges = 0, sheets = 0;
    auto check = [&](const art::HumanLook& L, const char* what, int a, int b2) {
      sheets++;
      int n = newEdgePixels(L);
      if (n) { out("FAIL: hero: %s %d/%d paints %d pixels on its cell edges", what, a, b2, n); edges++; }
    };
    if (seed % 5 == 1) {   // the exhaustive sweep is seed-independent: run it on a few seeds only
      for (int band = 1; band <= art::HumanLook::kBands; band++)
        for (int cl = 0; cl <= art::HumanLook::kCloaks; cl++)
          for (int sh = 0; sh <= art::HumanLook::kShields; sh++) {
            art::HumanLook L;
            L.helmStyle = L.armorStyle = L.gloves = L.boots = (uint8_t)band;
            L.cloak = (uint8_t)cl; L.shieldStyle = (uint8_t)sh; L.weapon = (uint8_t)(1 + (band + sh) % 6);
            L.backItem = (uint8_t)((band + cl) % 4); L.amulet = L.ring = true; L.build = (uint8_t)(band % 3);
            check(L, "band/cloak", band, cl * 10 + sh);
          }
      for (int h = 0; h < (int)art::Hair::COUNT; h++)
        for (int bl = 0; bl < 3; bl++)
          for (int w = 0; w <= 6; w++) {
            art::HumanLook L;
            L.hair = (art::Hair)h; L.build = (uint8_t)bl; L.weapon = (uint8_t)w; L.beard = (h + w) & 1;
            check(L, "hair/build", h, bl * 10 + w);
          }
    }
    for (int i = 0; i < 24; i++) check(randomLook(r), "random look", (int)seed, i);
    bad += edges;
    (void)sheets;
  }

  // ---- 6. the new hairs paint differently from the old ones; the M0 slots drop as loot with names and icons
  {
    art::HumanLook a, b2;
    a.hair = art::Hair::Short;
    for (art::Hair h : {art::Hair::Bun, art::Hair::Curls}) {
      b2.hair = h;
      if (art::humanSheet(a).px == art::humanSheet(b2).px) { out("FAIL: hero: hair %d paints like short hair", (int)h); bad++; }
    }
    int got[3] = {0, 0, 0};
    for (int i = 0; i < 3000; i++) {
      Item it = randomLoot(r, 1 + r.irange(30), r.f() < 0.2f);
      int k = it.kind == ItemKind::Gloves ? 0 : it.kind == ItemKind::Boots ? 1 : it.kind == ItemKind::Cloak ? 2 : -1;
      if (k < 0) continue;
      got[k]++;
      static const art::Icon icons[3] = {art::Icon::Gloves, art::Icon::Boots, art::Icon::Cloak};
      if (it.icon != icons[k] || it.name.empty() || it.power <= 0 || it.value <= 0) {
        out("FAIL: hero: dropped %s has icon %d power %d value %d", it.name.c_str(), (int)it.icon, it.power, it.value);
        bad++;
      }
    }
    if (!got[0] || !got[1] || !got[2]) { out("FAIL: hero: loot never drops gloves/boots/cloak (%d/%d/%d)", got[0], got[1], got[2]); bad++; }
  }
  return bad;
}
