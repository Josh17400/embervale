// M9 "Arcana & Mastery": the perk trees of every non-magic skill (15.29.2), drawn as constellations by the UI
// (rpg/view/skills_ui.cpp). SKILLS lane. Every perk changes how the game plays (rpg/sim/skills_game.cpp, game.cpp,
// player.cpp, ai.cpp, craft_game.cpp, home_game.cpp make them work; rpg_test --skills proves each one).
// Indices are saved (a bit per index in skills::Track::perks): append only within a tree, and keep skills.h's perk
// index enums (skills::perk) in the same order. A tree: a root at the top of the -8..8 grid, branches falling away
// below it, a capstone at the foot that needs two branches.
#include <iterator>
#include "rpg/sim/magic.h"
#include "rpg/sim/skills.h"

namespace skills {

namespace {
// {name, line, need, parent, parent2, x, y}
const PerkDef kSword[] = {
    {"BLADESMAN", "+15% DAMAGE WITH SWORDS", 1, -1, -1, 0, 7},
    {"PARRY", "SWING INTO A BLOW AS IT LANDS: IT IS TURNED ASIDE AND THE FOE REELS", 20, 0, -1, -4, 3},
    {"KEEN EDGE", "SWORD CRITS COME 10% MORE OFTEN", 30, 0, -1, 4, 3},
    {"RIPOSTE", "A PARRY OPENS THE FOE: YOUR NEXT BLOW IS A CRIT", 50, 1, -1, -6, -2},
    {"WHIRLWIND", "THE COMBO FINISHER STRIKES ALL ROUND YOU", 70, 1, 2, 0, -4},
    {"FLURRY", "SWORD SWINGS COST A THIRD LESS STAMINA", 40, 2, -1, 6, -1},
    {"BLADEMASTER", "A SWORD CRIT MAKES THE FOE STAGGER", 90, 3, 4, -2, -8},
};
const PerkDef kAxe[] = {
    {"AXEMAN", "+15% DAMAGE WITH AXES", 1, -1, -1, 0, 7},
    {"BLEEDING CUTS", "AXE BLOWS MAKE BEASTS AND THE UNARMOURED BLEED", 20, 0, -1, -4, 3},
    {"CLEAVE", "THE COMBO FINISHER REACHES WIDER AND FARTHER", 40, 0, -1, 4, 3},
    {"HEADSMAN", "+40% AXE DAMAGE ON FOES BELOW A QUARTER HEALTH", 70, 1, 2, 0, -4},
    {"DEEP WOUNDS", "BLEEDS LAST TWICE AS LONG AND BITE TWICE AS HARD", 50, 1, -1, -6, -1},
    {"HEW", "AXE POWER ATTACKS STAGGER", 30, 0, -1, 7, 5},   // (M9 fixer) clear of CLEAVE: its line no longer runs through it
    {"BLOODLUST", "KILLING A BLEEDING FOE GIVES BACK 25 STAMINA", 85, 3, 4, -3, -8},
};
const PerkDef kMace[] = {
    {"MACEMAN", "+15% DAMAGE WITH MACES", 1, -1, -1, 0, 7},
    {"CRUSHING BLOWS", "MACE POWER ATTACKS STAGGER ANY FOE, EVEN THE ARMOURED", 20, 0, -1, -4, 3},
    {"BONEBREAKER", "MACE BLOWS IGNORE A THIRD OF ARMOUR", 40, 0, -1, 4, 3},
    {"SKULL CRUSHER", "SKELETONS YOU STRIKE WITH A MACE NEVER RISE AGAIN", 60, 1, 2, 0, -4},
    {"HAMMERFALL", "THE MACE FINISHER KNOCKS FOES FLAT: A LONG STAGGER", 40, 1, -1, -6, -1},
    {"ARMOUR BREAKER", "A FOE YOU STRIKE LOSES A FIFTH OF ITS ARMOUR FOR 6 S", 55, 2, -1, 6, -1},
    {"SHOCKWAVE", "THE MACE FINISHER'S SHOCKWAVE STAGGERS EVERY FOE CLOSE BY", 85, 3, 4, -2, -8},
};
const PerkDef kSpear[] = {
    {"SPEARMAN", "+15% DAMAGE WITH SPEARS", 1, -1, -1, 0, 7},
    {"SET AGAINST CHARGE", "A CHARGER STRUCK MID-RUSH IS STOPPED AND STAGGERED", 20, 0, -1, -4, 3},
    {"LONG REACH", "SPEARS REACH ANOTHER HALF TILE", 40, 0, -1, 4, 3},
    {"IMPALE", "THE COMBO FINISHER PIERCES FAR DOWN THE LINE", 60, 1, 2, 0, -4},
    {"KEEP AT BAY", "SPEAR BLOWS DRIVE FOES BACK TWICE AS FAR", 30, 0, -1, -7, 0},
    {"GIANT SLAYER", "+25% SPEAR DAMAGE TO HUGE FOES AND FLIERS", 50, 2, -1, 6, -1},
    {"PHALANX", "WITH A SPEAR AND A SHIELD, A CHARGER THAT HITS YOU IS STAGGERED", 80, 3, 4, -3, -8},
};
const PerkDef kDagger[] = {
    {"CUTPURSE'S HAND", "+15% DAMAGE WITH DAGGERS", 1, -1, -1, 0, 7},
    {"BACKSTAB", "AN UNAWARE FOE TAKES TRIPLE DAMAGE FROM A DAGGER", 20, 0, -1, -4, 3},
    {"QUICK HANDS", "DAGGERS SWING 15% FASTER", 30, 0, -1, 4, 3},
    {"SILENT KILL", "A FOE KILLED UNAWARE MAKES NO SOUND: NONE NEAR IT WAKES", 50, 1, -1, -5, -2},
    {"ASSASSIN'S BLADE", "DAGGER BACKSTABS DEAL FIVE TIMES THE DAMAGE", 70, 1, -1, -2, -5},
    {"LACERATE", "DAGGER CRITS MAKE ANY FOE BLEED", 40, 2, -1, 6, -1},
    {"SHADOW STRIKE", "A KILL WHILE SNEAKING LEAVES YOU UNSEEN FOR 2 S", 85, 3, 5, 2, -8},
};
const PerkDef kBow[] = {
    {"OVERDRAW", "+15% DAMAGE WITH BOWS", 1, -1, -1, 0, 7},
    {"EAGLE EYE", "ARROWS FLY 25% FASTER AND FARTHER", 20, 0, -1, -4, 3},
    {"STEADY DRAW", "AS YOU LOOSE, TIME SLOWS FOR A HEARTBEAT (EVERY 6 S)", 50, 1, -1, -5, -2},
    {"HUNTER'S MARK", "+30% DAMAGE TO A BEAST YOUR ARROW STRUCK FIRST", 40, 0, -1, 4, 3},
    {"QUICK SHOT", "BOWS DRAW 20% FASTER", 30, 0, -1, 0, 3},
    {"POWER SHOT", "AN ARROW STAGGERS WHAT IT HITS", 60, 1, -1, -1, -3},
    {"RANGER", "ARROWS FIND THE GAPS: NO PENALTY AGAINST SHIELDS", 70, 3, 4, 5, -3},
    {"TWIN SHOT", "EVERY THIRD ARROW FLIES AS A PAIR", 90, 2, 5, -3, -8},
};
const PerkDef kUnarmed[] = {
    {"IRON FISTS", "+30% DAMAGE WITH BARE HANDS", 1, -1, -1, 0, 7},
    {"BRAWLER", "THE THIRD PUNCH STAGGERS", 25, 0, -1, -4, 3},
    {"HAYMAKER", "+25% BARE-HANDED DAMAGE TO PEOPLE", 40, 0, -1, 4, 3},
    {"IRON SKIN", "BARE-HANDED, BLOWS HURT YOU 10% LESS", 50, 1, -1, -5, -2},
    {"FLURRY OF FISTS", "PUNCHES COST NO STAMINA", 60, 2, -1, 5, -2},
    {"STUNNING BLOW", "THE THIRD PUNCH STUNS FOR A FULL SECOND", 85, 3, 4, 0, -7},
};
const PerkDef kBlock[] = {
    {"SHIELD WALL", "A SHIELD STOPS 10% MORE OF A BLOW", 1, -1, -1, 0, 7},
    {"SHIELD BASH", "A ROLL WITH A SHIELD BASHES AND STAGGERS THE FOES IN YOUR PATH", 30, 0, -1, -4, 3},
    {"DEFLECT ARROWS", "WITH A SHIELD, ARROWS AND SPIT DO HALF DAMAGE", 20, 0, -1, 4, 3},
    {"STEADFAST", "WITH A SHIELD, BLOWS NO LONGER STAGGER YOU", 40, 0, -1, 0, 1},
    {"QUICK REFLEXES", "A BLOW THAT WOULD TAKE A FIFTH OF YOUR HEALTH IS HALVED (EVERY 10 S)", 50, 1, -1, -5, -3},
    {"BULWARK", "A SHIELD STOPS 20% MORE OF A BLOW", 70, 2, 3, 4, -5},
};
const PerkDef kLight[] = {
    {"AGILE DEFENCE", "LIGHT ARMOUR STOPS 10% MORE", 1, -1, -1, 0, 7},
    {"WIND RUNNER", "IN LIGHT ARMOUR, STAMINA COMES BACK 20% FASTER", 30, 0, -1, -4, 3},
    {"DEFT ROLL", "IN LIGHT ARMOUR A ROLL COSTS HALF THE STAMINA", 50, 1, -1, -5, -2},
    {"UNHINDERED", "IN LIGHT ARMOUR YOU MOVE 5% FASTER", 20, 0, -1, 4, 3},
    {"CUSTOM FIT", "A FULL SET OF LIGHT ARMOUR: +15% ARMOUR", 40, 3, -1, 5, -1},
    {"EVASION", "A PERFECT DODGE IN LIGHT ARMOUR GIVES BACK THE ROLL'S STAMINA", 60, 2, -1, -2, -5},
    {"MATCHLESS", "IN LIGHT ARMOUR A PERFECT DODGE LEAVES YOU UNSEEN FOR A MOMENT", 90, 4, 5, 2, -8},
};
const PerkDef kMedium[] = {
    {"BALANCED KIT", "MEDIUM ARMOUR STOPS 10% MORE", 1, -1, -1, 0, 7},
    {"STEADY", "IN MEDIUM ARMOUR, ONLY HEAVY BLOWS STAGGER YOU", 30, 0, -1, -4, 3},
    {"WELL BALANCED", "MEDIUM ARMOUR NO LONGER ADDS TO A ROLL'S COST", 20, 0, -1, 4, 3},
    {"SOLDIER'S WIND", "+20 STAMINA IN MEDIUM ARMOUR", 40, 2, -1, 5, -1},
    {"CUSTOM FIT", "A FULL SET OF MEDIUM ARMOUR: +15% ARMOUR", 50, 1, -1, -5, -1},
    {"RALLY", "BELOW A THIRD OF YOUR HEALTH, STAMINA REFILLS (EVERY 60 S)", 70, 3, -1, 3, -5},
    {"VETERAN", "MEDIUM ARMOUR STOPS 10% MORE AND MAKES HALF THE NOISE", 90, 4, 5, -2, -8},
};
const PerkDef kHeavy[] = {
    {"JUGGERNAUT", "HEAVY ARMOUR STOPS 10% MORE", 1, -1, -1, 0, 7},
    {"WELL FITTED", "HEAVY ARMOUR NO LONGER SLOWS YOUR WALK", 30, 0, -1, -4, 3},
    {"CHARGE", "A ROLL IN HEAVY ARMOUR IS A SHOULDER CHARGE THAT STAGGERS", 50, 1, -1, -5, -2},
    {"CONDITIONING", "HEAVY ARMOUR NO LONGER SLOWS YOUR STAMINA", 40, 0, -1, 4, 3},
    {"CUSTOM FIT", "A FULL SET OF HEAVY ARMOUR: +15% ARMOUR", 30, 0, -1, 1, 1},
    {"IMMOVABLE", "IN HEAVY ARMOUR, BLOWS DO NOT KNOCK YOU BACK OR STAGGER YOU", 60, 3, -1, 5, -2},
    {"UNSTOPPABLE", "A SHOULDER CHARGE BOWLS FOES OVER AND HURTS THEM", 80, 2, 5, 0, -5},
    {"REFLECT BLOWS", "ONE MELEE BLOW IN TEN IS TURNED BACK ON ITS STRIKER", 100, 6, -1, 0, -8},
};
const PerkDef kHeritage[] = {
    {"KINSHIP OF MAKE", "A FULL SET OF ONE PEOPLE'S ARMOUR: UP TO +15% ARMOUR AS YOU KNOW IT", 10, -1, -1, 0, 7},
    {"MASTER OF METALS", "STEEL AND ALLOYS YOU KNOW WELL: UP TO +8% DAMAGE", 40, 0, -1, 4, 3},
    {"BLADE AND MAIL", "A WEAPON OF THE SAME PEOPLE AS YOUR ARMOUR: +10% DAMAGE", 25, 0, -1, -4, 3},
    {"OLD BRONZE", "BRONZE, BONE AND WOOD PIECES: +10% ARMOUR", 20, 0, -1, 0, 2},
    {"ONE OF THEIR OWN", "IN A FULL SET OF A PEOPLE'S MAKE, THEIR SOLDIERS SENSE YOU HALF AS FAR", 60, 0, 2, -4, -4},
    {"HOMELAND'S PRIDE", "YOUR OWN PEOPLE'S MAKE WORN: +15 HEALTH", 50, 1, -1, 4, -3},
};
const PerkDef kAthletics[] = {
    {"LONG STRIDE", "+5% RUNNING SPEED", 1, -1, -1, 0, 7},
    {"SECOND WIND", "STAMINA COMES BACK 15% FASTER", 30, 0, -1, -4, 3},
    {"LIGHT FEET", "A ROLL CARRIES YOU 10% FARTHER", 20, 0, -1, 4, 3},
    {"ENDURANCE", "+20 STAMINA", 40, 1, -1, -4, -2},
    {"TRAVELLER", "OUT OF A FIGHT YOU WALK 10% FASTER", 60, 2, 3, 2, -5},
};
const PerkDef kSmithing[] = {
    {"STEEL SMITH", "IRON AND STEEL PIECES YOU FORGE ARE 10% STRONGER", 20, -1, -1, 0, 7},
    {"FINE TEMPER", "EVERY PIECE YOU FORGE IS 10% STRONGER", 50, 0, -1, -4, 2},
    {"THRIFTY SMELTER", "ONE SMELT IN THREE GIVES AN EXTRA INGOT", 30, 0, -1, 4, 2},
    {"QUICK STUDY", "TAKING A PIECE APART TEACHES TWICE AS MUCH", 40, 2, -1, 5, -3},
    {"MASTERWORK", "ALLOY AND CULTURE PIECES YOU FORGE ARE 15% STRONGER", 80, 1, -1, -3, -5},
};
const PerkDef kCooking[] = {
    {"HEARTY", "THE WELL FED OF A MEAL YOU COOKED LASTS TWICE AS LONG", 20, -1, -1, 0, 7},
    {"SECOND HELPING", "ONE MEAL IN THREE YOU COOK COMES WITH A SECOND PORTION", 40, 0, -1, -4, 1},
    {"FIELD COOK", "A MEAL YOU COOKED HEALS 25 MORE HEALTH WHEN EATEN", 60, 0, -1, 4, 1},
};
const PerkDef kFarming[] = {
    {"GREEN THUMB", "CROPS YOU HARVEST YIELD ONE MORE", 20, -1, -1, 0, 7},
    {"RAIN MAKER", "WATERING ALSO WATERS THE CROPS BESIDE IT", 40, 0, -1, -4, 1},
    {"GOOD HUSBANDRY", "WHAT YOU COLLECT FROM YOUR ANIMALS IS DOUBLED", 50, 0, -1, 4, 1},
};
const PerkDef kRiding[] = {
    {"HORSEMAN", "YOUR HORSE RUNS 10% FASTER", 20, -1, -1, 0, 7},
    {"SURE SEAT", "ONLY A BLOW OF A TENTH OF YOUR HEALTH KNOCKS YOU FROM THE SADDLE", 30, 0, -1, -4, 1},
    {"STABLE HAND", "STABLE KEEP COSTS YOU HALF", 50, 0, -1, 4, 1},
};
const PerkDef kSpeech[] = {
    {"HAGGLER", "PRICES ON A SHELF ARE 10% LOWER FOR YOU", 1, -1, -1, 0, 7},
    {"INTIMIDATE", "A NEW OPTION: THREATEN A MERCHANT INTO A LOWER PRICE", 30, 0, -1, -4, 3},
    {"PERSUADE", "A NEW OPTION: TALK A MERCHANT INTO A BETTER PRICE", 30, 0, -1, 4, 3},
    {"TRAINER'S PET", "TRAINERS CHARGE YOU A QUARTER LESS", 40, 0, -1, 0, 1},
    {"SILVER TONGUE", "PERSUASION AND THREATS NEVER FAIL", 70, 1, 2, 0, -5},
};
const PerkDef kStealth[] = {
    {"SOFT STEP", "FOES SENSE YOU 20% LESS WHEN YOU SNEAK", 1, -1, -1, 0, 7},
    {"SHADOW", "SNEAKING IN LIGHT ARMOUR OR NONE, YOU ARE ALL BUT SILENT", 40, 0, -1, -4, 2},
    {"SNEAK ATTACK", "SNEAKING, ANY BLOW TO THE UNAWARE DEALS DOUBLE DAMAGE", 30, 0, -1, 4, 2},
    {"VANISH", "SNEAKING, ONCE YOU ARE 8 TILES AWAY A FOE LOSES YOUR TRAIL", 70, 1, -1, -3, -4},
};
// (reserved: the world has no locks yet. Its skill stays at its start level and the tree waits for them.)
const PerkDef kLock[] = {
    {"NIMBLE PICK", "LOCKS ARE EASIER TO PICK", 1, -1, -1, 0, 7},
};
}  // namespace

const PerkDef* perkTable(Skill s, int& n) {
#define EMB_PERKS(T) do { n = (int)std::size(T); return T; } while (0)
  switch (s) {
    case Skill::Sword: EMB_PERKS(kSword);
    case Skill::Axe: EMB_PERKS(kAxe);
    case Skill::Mace: EMB_PERKS(kMace);
    case Skill::Spear: EMB_PERKS(kSpear);
    case Skill::Dagger: EMB_PERKS(kDagger);
    case Skill::Bow: EMB_PERKS(kBow);
    case Skill::Unarmed: EMB_PERKS(kUnarmed);
    case Skill::Block: EMB_PERKS(kBlock);
    case Skill::LightArmour: EMB_PERKS(kLight);
    case Skill::MediumArmour: EMB_PERKS(kMedium);
    case Skill::HeavyArmour: EMB_PERKS(kHeavy);
    case Skill::Heritage: EMB_PERKS(kHeritage);
    case Skill::Athletics: EMB_PERKS(kAthletics);
    case Skill::Smithing: EMB_PERKS(kSmithing);
    case Skill::Cooking: EMB_PERKS(kCooking);
    case Skill::Farming: EMB_PERKS(kFarming);
    case Skill::Riding: EMB_PERKS(kRiding);
    case Skill::Speech: EMB_PERKS(kSpeech);
    case Skill::Stealth: EMB_PERKS(kStealth);
    case Skill::Lockpicking: EMB_PERKS(kLock);
    default: break;
  }
#undef EMB_PERKS
  if (isMagic(s)) return magic::schoolPerks(s, n);
  static const PerkDef none[1] = {{"", "", 0, -1, -1, 0, 0}};
  n = 0;
  return none;
}

}  // namespace skills
