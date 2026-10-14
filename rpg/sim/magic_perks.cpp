// M9 "Arcana & Mastery": the magic schools' perk trees (15.5 "perk trees per school", 15.29.2 "dual-casting"), drawn
// as constellations by the UI like every other tree (skills::perkTable routes here). MAGIC lane. Every perk works:
// magic_game.cpp reads them by index (the P_* / D_* / R_* / C_* / I_* / A_* enums there). Indices are saved (a bit per
// index in skills::Track::perks): append only within a tree.
#include <iterator>
#include "rpg/sim/magic.h"

namespace magic {

namespace {
using skills::PerkDef;
// {name, line, need, parent, parent2, x, y}: the star's place on the -8..8 grid (y 7 the root at the bottom; the tree
// climbs and branches like its school: Destruction a three-pronged flame, Restoration a rising sun, Conjuration a rift,
// Illusion a mask, Alteration a shield)
const PerkDef kDestruction[] = {
    {"NOVICE DESTRUCTION", "NOVICE DESTRUCTION SPELLS COST HALF", 1, -1, -1, 0, 7},
    {"DUAL CASTING", "DUAL-CAST DESTRUCTION: AN OVERCHARGED, BIGGER SPELL", 20, 0, -1, 0, 3},
    {"AUGMENTED FLAMES", "FIRE SPELLS +25% DAMAGE", 30, 0, -1, -4, 2},
    {"AUGMENTED FROST", "FROST SPELLS +25% DAMAGE", 30, 0, -1, 4, 2},
    {"IMPACT", "A DUAL-CAST STAGGERS WHAT IT HITS", 50, 1, -1, 0, -1},
    {"AUGMENTED SHOCK", "STORM SPELLS +25% DAMAGE", 30, 0, -1, 2, 1},
    {"APPRENTICE DESTRUCTION", "APPRENTICE DESTRUCTION SPELLS COST HALF", 25, 0, -1, -2, 5},
    {"ADEPT DESTRUCTION", "ADEPT AND HIGHER DESTRUCTION SPELLS COST A QUARTER LESS", 50, 6, -1, -2, 1},
    {"INTENSE FLAMES", "A BURNING FOE BELOW 30% HEALTH PANICS AND FLEES", 50, 2, -1, -6, -2},
    {"DEEP FREEZE", "FROST ON A CHILLED FOE FREEZES IT SOLID", 60, 3, -1, 6, -2},
    {"DISINTEGRATE", "STORM SPELLS FINISH A FOE LEFT BELOW 15% HEALTH", 70, 5, -1, 3, -5},
};
const PerkDef kRestoration[] = {
    {"NOVICE RESTORATION", "NOVICE RESTORATION SPELLS COST HALF", 1, -1, -1, 0, 7},
    {"DUAL CASTING", "DUAL-CAST RESTORATION: A STRONGER HEAL OR WARD", 20, 0, -1, 0, 3},
    {"RESPITE", "HEALING SPELLS ALSO RESTORE STAMINA", 40, 0, -1, -4, 2},
    {"RECOVERY", "MAGICKA RETURNS FASTER", 30, 0, -1, 4, 4},
    {"WARD ABSORB", "A WARD STRUCK GIVES BACK HALF THE BLOW AS MAGICKA", 50, 1, -1, 3, 0},
    {"SUNWARDEN", "LIGHT BURNS THE UNDEAD TWICE OVER", 40, 0, -1, -6, -1},
    {"APPRENTICE RESTORATION", "APPRENTICE RESTORATION SPELLS COST HALF", 25, 0, -1, -2, 5},
    {"WARM HANDS", "HEALING SPELLS HEAL 40% MORE", 50, 2, -1, -3, -2},
    {"AVOID DEATH", "ONCE A DAY A KILLING BLOW LEAVES YOU STANDING, HEALED", 90, 7, 4, 0, -6},
};
const PerkDef kConjuration[] = {
    {"NOVICE CONJURATION", "NOVICE CONJURATION SPELLS COST HALF", 1, -1, -1, 0, 7},
    {"DUAL CASTING", "DUAL-CAST CONJURATION: SUMMONS LAST TWICE AS LONG", 20, 0, -1, 0, 3},
    {"TWIN SOULS", "TWO SUMMONS AT ONCE", 70, 1, -1, 0, -2},
    {"SUMMONER", "SUMMONS HAVE 40% MORE HEALTH", 30, 0, -1, 3, 4},
    {"MYSTIC BINDING", "BOUND WEAPONS STRIKE 50% HARDER", 20, 0, -1, -4, 4},
    {"NECROMANCY", "THE DEAD YOU RAISE OR CALL BITE 50% HARDER", 40, 0, -1, -4, 0},
    {"APPRENTICE CONJURATION", "APPRENTICE CONJURATION SPELLS COST HALF", 25, 0, -1, -2, 5},
    {"ELEMENTAL POTENCY", "FLAME AND STORM ATRONACHS STRIKE 50% HARDER", 60, 3, -1, 4, 0},
    {"SOUL HARVEST", "A FOE YOUR SUMMON KILLS GIVES YOU 10 MAGICKA", 50, 5, -1, -5, -4},
};
const PerkDef kIllusion[] = {
    {"NOVICE ILLUSION", "NOVICE ILLUSION SPELLS COST HALF", 1, -1, -1, 0, 7},
    {"DUAL CASTING", "DUAL-CAST ILLUSION: WIDER AND LONGER", 20, 0, -1, 0, 3},
    {"QUIET CASTING", "SPELLS MAKE NO SOUND FOES CAN HEAR", 50, 0, -1, 4, 1},
    {"ANIMAGE", "SPELLS OF THE MIND REACH BEASTS 10 LEVELS HIGHER", 20, 0, -1, -4, 4},
    {"KINDRED MAGE", "SPELLS OF THE MIND REACH PEOPLE 10 LEVELS HIGHER", 40, 3, -1, -5, 1},
    {"HYPNOTIC GAZE", "CALM, CHARM, FEAR AND BLINDNESS LAST HALF AGAIN AS LONG", 30, 0, -1, 3, 4},
    {"APPRENTICE ILLUSION", "APPRENTICE ILLUSION SPELLS COST HALF", 25, 0, -1, -2, 5},
    {"MASTER OF THE MIND", "SPELLS OF THE MIND WORK ON THE UNDEAD AND ON CONSTRUCTS", 90, 4, 2, 0, -5},
    {"ASPECT OF TERROR", "FRIGHTENED FOES TAKE 20% MORE FROM YOU", 60, 5, -1, 5, -2},
};
const PerkDef kAlteration[] = {
    {"NOVICE ALTERATION", "NOVICE ALTERATION SPELLS COST HALF", 1, -1, -1, 0, 7},
    {"DUAL CASTING", "DUAL-CAST ALTERATION: STRONGER AND LONGER", 20, 0, -1, 0, 3},
    {"MAGIC RESISTANCE", "FOES' SPELLS DO 15% LESS", 40, 0, -1, -4, 1},
    {"MAGE ARMOUR", "STONE AURAS LAST TWICE AS LONG", 30, 0, -1, 4, 4},
    {"STABILITY", "ALTERATION SPELLS LAST HALF AGAIN AS LONG", 50, 1, -1, 3, 0},
    {"APPRENTICE ALTERATION", "APPRENTICE ALTERATION SPELLS COST HALF", 25, 0, -1, -2, 5},
    {"FAR BLINK", "BLINK REACHES HALF AGAIN AS FAR", 40, 0, -1, 5, -2},
    {"SURFACE SHAPER", "THE SURFACES YOU LAY LAST TWICE AS LONG", 60, 4, -1, 1, -4},
    {"ATRONACH", "15% OF FOES' SPELL DAMAGE COMES BACK AS MAGICKA", 70, 2, -1, -4, -4},
};
}  // namespace

const PerkDef* schoolPerks(skills::Skill s, int& n) {
#define EMB_PERKS(T) do { n = (int)std::size(T); return T; } while (0)
  switch (s) {
    case skills::Skill::Destruction: EMB_PERKS(kDestruction);
    case skills::Skill::Restoration: EMB_PERKS(kRestoration);
    case skills::Skill::Conjuration: EMB_PERKS(kConjuration);
    case skills::Skill::Illusion: EMB_PERKS(kIllusion);
    case skills::Skill::Alteration: EMB_PERKS(kAlteration);
    default: break;
  }
#undef EMB_PERKS
  static const PerkDef none[1] = {{"", "", 0, -1, -1, 0, 0}};
  n = 0;
  return none;
}

}  // namespace magic
