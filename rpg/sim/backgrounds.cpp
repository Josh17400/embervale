// Background names and texts (backgrounds.h). The perk effects live where they act: Game::finishCreator lists them.
// Keep each perk line true to what the code does (the creator shows it).
#include "rpg/sim/backgrounds.h"

const BackgroundInfo& backgroundInfo(Background b) {
  static const BackgroundInfo t[] = {
      {"WANDERER", "NO SPECIAL TRAIT.", "YOU REMEMBER LITTLE BEFORE THE ROAD."},
      {"BLACKSMITH'S CHILD", "SMITHS CHARGE YOU A FIFTH LESS.", "YOUR FATHER'S HAMMER WAS SOLD TO PAY HIS DEBTS."},
      {"HUNTER", "BEASTS NOTICE YOU LATER. WOLVES ALWAYS GIVE A PELT.", "A WHITE WOLF TOOK YOUR LAST CATCH, AND YOUR BOW."},
      {"TEMPLE NOVICE", "YOU KNOW MEND. BLESSINGS LAST HALF AGAIN AS LONG.", "YOU LEFT THE TEMPLE BEFORE YOUR VOWS."},
      {"STREET URCHIN", "MERCHANTS GIVE YOU A STREET PRICE.", "SOMEONE IN THE CAPITAL STILL WANTS YOU FOUND."},
      {"FARMHAND", "STAMINA RETURNS FASTER. FOOD HEALS HALF AGAIN AS MUCH.", "THE HARVEST FAILED, AND THE FARM WITH IT."},
      {"EXILED NOBLE", "GUARDS AND JARLS OFFER YOU MORE WORK.", "YOUR HOUSE FELL. ONE KINGDOM WANTS YOU IN CHAINS."},
      {"SAILOR", "SEA LEGS: MORE STAMINA. CAPTAINS WILL TRUST YOU.", "YOUR SHIP WENT DOWN OFF A COAST NO MAP SHOWS."},
      {"MARKED ONE", "A FAINT ECHO OF A LOST MAGIC: MORE MAGICKA.", "A MARK ON YOUR WRIST GLOWS NEAR OLD RUINS."},
  };
  int i = (int)b;
  return t[i >= 0 && i < (int)Background::COUNT ? i : 0];
}
