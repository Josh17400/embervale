// P2a RIG-BODY: the style slice's script command (VISION_PLAN 15.36 "The style slice", STYLE_P2 19). ADDED beside the
// phase A harness (script_p2a.cpp), nothing in it renamed or changed: `body` switches only the body base, so the slice
// scripts dress the in-game hero as the slice's people with this (the man in Chestnut short hair, the woman in
// Chestnut braids) and the woman hero is the slice's woman, not the man's haircut on a woman's body.
//   hair <style> [colour]   the hero's hair (Appearance::hair): bald short long ponytail mohawk braids bun curls; and,
//                           optionally, its colour by the creator's names: raven umber chestnut auburn copper wheat
//                           flax ash silver plum (creator.cpp kHairCols, STYLE_P2 6)
// A test command (scripts run without a save): it changes the live Appearance only.
#include <cctype>
#include <cstdint>
#include <string>
#include "rpg/art/art_human.h"
#include "rpg/sim/anim.h"
#include "rpg/sim/game.h"
#include "rpg/view/script_api.h"

namespace {

std::string lowerOf(std::string s) {
  for (char& ch : s) ch = (char)std::tolower((unsigned char)ch);
  return s;
}

bool cmdHair(ScriptCtx& c) {
  static const char* styles[] = {"bald", "short", "long", "ponytail", "mohawk", "braids", "bun", "curls"};
  static_assert(sizeof(styles) / sizeof(styles[0]) == (size_t)art::Hair::COUNT, "a name per art::Hair");
  const std::string w = lowerOf(c.arg(1));
  int style = -1;
  for (int i = 0; i < (int)art::Hair::COUNT; i++)
    if (w == styles[i]) style = i;
  if (style < 0) { c.fail("hair: bald|short|long|ponytail|mohawk|braids|bun|curls [colour] (got '" + c.arg(1) + "')"); return true; }
  if (!c.arg(2).empty()) {
    static const char* names[10] = {"raven", "umber", "chestnut", "auburn", "copper", "wheat", "flax", "ash", "silver", "plum"};
    static const uint32_t cols[10] = {rgba(36, 30, 34),    rgba(70, 44, 30),    rgba(120, 70, 36),   rgba(150, 64, 36),
                                      rgba(200, 104, 44),  rgba(220, 180, 100), rgba(232, 220, 182), rgba(150, 148, 150),
                                      rgba(228, 226, 218), rgba(120, 50, 110)};
    const std::string cw = lowerOf(c.arg(2));
    int k = -1;
    for (int i = 0; i < 10; i++)
      if (cw == names[i]) k = i;
    if (k < 0) { c.fail("hair: colour raven|umber|chestnut|auburn|copper|wheat|flax|ash|silver|plum (got '" + c.arg(2) + "')"); return true; }
    c.game.app.hairColor = cols[k];
  }
  c.game.app.hair = (uint8_t)style;
  AnimOps::refreshHero(c.game);
  return true;
}
EMB_SCRIPT_CMD("hair", "hair <style> [colour]: the hero's hair (P2a style slice: bald short long ponytail mohawk braids bun curls; the creator's colour names)", cmdHair);

}  // namespace
