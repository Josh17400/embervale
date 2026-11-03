// rpg_test --look-key: (SOLID 5.5) HumanLook::key() is the identity of a painted sheet (the renderer caches baked sheets by
// it), so every field that changes pixels must change the key. This test flips each HumanLook field, twice (its low bit
// and its top bit; a bool flips, an enum steps forward and back), from the default look and from a "busy" look where
// every field is already set, and asserts that key() changes each time. It runs at rig 2 (the doll, where every field
// enters the key; rig 1 deliberately keys only the v1 fields) and restores the rig it found. key() itself is untouched.
// A new HumanLook field trips the sizeof check below: add it to kFields (and to key()) and update the size.
#include <cstdint>
#include <cstdio>
#include <type_traits>
#include "rpg/art/art_items.h"
#include "rpg/art/art_rig.h"
#include "rpg/core/art_ids.h"
#include "tools/tests/tests.h"

namespace {

using art::HumanLook;

// one flip of a field: bit 0 the low bit, bit 1 the top bit (bools: negate; enums: next / previous value)
template <class T>
void flip(T& v, int bit) {
  if constexpr (std::is_same_v<T, bool>) {
    v = !v;
  } else if constexpr (std::is_enum_v<T>) {
    using U = std::underlying_type_t<T>;
    const int n = (int)T::COUNT, u = (int)(U)v;
    v = (T)(U)(bit ? (u + n - 1) % n : (u + 1) % n);
  } else {
    using U = std::make_unsigned_t<T>;
    const U mask = bit ? (U)((U)1 << (sizeof(T) * 8 - 1)) : (U)1;
    v = (T)((U)v ^ mask);
  }
}

struct Field {
  const char* name;
  void (*flip)(HumanLook&, int bit);
};
#define LK(f) Field{#f, [](HumanLook& l, int b) { flip(l.f, b); }}
#define LKA(f, i) Field{#f "[" #i "]", [](HumanLook& l, int b) { flip(l.f[i], b); }}
const Field kFields[] = {
    LK(skin), LK(hairColor), LK(topColor), LK(bottomColor), LK(tabardColor), LK(hair), LK(outfit), LK(beard), LK(helmet),
    LK(hood), LK(cape), LK(shield), LK(weapon), LK(weaponColor),
    // M0
    LK(build), LK(eyeColor), LK(helmStyle), LK(armorStyle), LK(gloves), LK(boots), LK(cloak), LK(cloakColor),
    LK(shieldStyle), LK(backItem), LK(amulet), LK(trimColor), LK(ring),
    // M3
    LK(people), LK(cut), LK(headwear), LK(headColor), LK(pattern), LK(patternColor), LK(facePaint), LK(jewellery),
    LK(helmForm), LK(bodyForm), LK(shieldForm), LK(bladeForm), LK(armsOrnament), LK(pauldron), LK(skirt), LK(crest),
    LK(plumeColor),
    // M6
    LK(polearmForm), LK(bowForm), LK(armourTint), LK(armourTint2), LK(sheen), LK(glow), LK(glowColor), LK(shieldField),
    LK(shieldDevice),
    // P2a / P2b
    LK(body), LK(age), LK(head), LK(nose), LK(brow), LK(marks), LK(hairVar), LK(beardStyle), LK(neck), LK(garb), LK(civil),
    // P2c
    LKA(tier, 0), LKA(tier, 1), LKA(tier, 2), LKA(tier, 3), LKA(tier, 4), LKA(tier, 5), LKA(tier, 6), LKA(tier, 7),
    LK(rare), LK(metalKind), LK(bladeKind), LK(mythic), LK(gemColor), LK(motif), LK(floor), LK(kitVar), LK(alloyFam),
    LKA(forge, 0), LKA(forge, 1), LKA(forge, 2), LKA(forge, 3), LKA(forge, 4), LKA(forge, 5), LKA(forge, 6), LKA(forge, 7),
    LK(forgeKit), LK(alloyRank), LK(forgeGuise), LK(forgeShape), LK(makerPeople), LK(relicHelm), LK(relicBody), LK(king),
    LK(makers),   // (P5) the maker people per worn piece
};
#undef LK
#undef LKA
static_assert(HumanLook::TIER_SLOTS == 8, "look-key: kFields lists tier[0..7] and forge[0..7]");
static_assert(sizeof(HumanLook) == 232, "look-key: HumanLook changed size: add the new field to kFields (and to key())");

int lookKeyCmd(int, char**) {
  const int rigWas = art::rigVersion();
  art::setRigVersion(2);
  int fails = 0, checks = 0;
  HumanLook busy;   // every field set (each flipped once from its default)
  for (const Field& f : kFields) f.flip(busy, 0);
  const HumanLook bases[2] = {HumanLook{}, busy};
  const char* baseName[2] = {"default", "busy"};
  for (int b = 0; b < 2; b++) {
    const uint64_t k0 = bases[b].key();
    for (const Field& f : kFields)
      for (int bit = 0; bit < 2; bit++) {
        HumanLook l = bases[b];
        f.flip(l, bit);
        checks++;
        if (l.key() == k0) {
          fails++;
          std::printf("FAIL look-key: %s look, %s %s flip leaves key() unchanged\n", baseName[b], f.name, bit ? "top-bit" : "low-bit");
        }
      }
  }
  // the key is a pure function of the look: equal looks, equal keys
  if (HumanLook{}.key() != HumanLook{}.key() || HumanLook(busy).key() != busy.key()) {
    fails++;
    std::printf("FAIL look-key: equal looks give different keys\n");
  }
  // (P5) the icon's maker people and look tier change IconLook::key(); unset, the key is the pre-P5 one
  {
    art::IconLook il;
    const uint64_t k0 = il.key();
    for (int f = 0; f < 2; f++)
      for (int v : {1, 12}) {
        art::IconLook m = il;
        (f ? m.tier : m.people) = (uint8_t)v;
        checks++;
        if (m.key() == k0) {
          fails++;
          std::printf("FAIL look-key: IconLook %s=%d leaves key() unchanged\n", f ? "tier" : "people", v);
        }
      }
  }
  art::setRigVersion(rigWas);
  const int nf = (int)(sizeof(kFields) / sizeof(kFields[0]));
  std::printf("%s look-key: %d fields (last: %s; + IconLook people, tier), %d flips, %d unchanged keys\n", fails ? "FAIL" : "PASS", nf,
              kFields[nf - 1].name, checks, fails);
  return fails ? 1 : 0;
}

}  // namespace

RPG_TEST_CMD("--look-key", "SOLID 5.5: flipping any HumanLook field changes HumanLook::key() (rig 2)", lookKeyCmd);
