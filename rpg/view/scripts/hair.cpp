// P2a style slice test command `hair` (the hero's hair style and colour, for the slice scripts p2a_slice / p2b_slice).
// (P2b FOLK-BODY: this file was accidentally overwritten on 2026-10-19 and rebuilt from its compiled object's strings and
// colour table: the command, its help and errors and the creator's ten colours are the original's. The main session can
// compare it with git's copy and keep either.)
//   hair <style> [colour]   style: bald short long ponytail mohawk braids bun curls (art::Hair); colour: the creator's
//                           names raven umber chestnut auburn copper wheat flax ash silver plum; the look rebuilt by the sim
#include <cstdint>
#include <string>
#include "rpg/sim/anim.h"
#include "rpg/sim/game.h"
#include "rpg/view/script_api.h"

namespace {

const char* const kStyles[8] = {"bald", "short", "long", "ponytail", "mohawk", "braids", "bun", "curls"};
const char* const kColourNames[10] = {"raven", "umber", "chestnut", "auburn", "copper", "wheat", "flax", "ash", "silver", "plum"};
// the creator's hair colours (rpg/view/creator.cpp kHairCols)
const uint32_t kColours[10] = {rgba(36, 30, 34),   rgba(70, 44, 30),    rgba(120, 70, 36),   rgba(150, 64, 36),  rgba(200, 104, 44),
                               rgba(220, 180, 100), rgba(232, 220, 182), rgba(150, 148, 150), rgba(228, 226, 218), rgba(120, 50, 110)};

std::string lower(std::string s) {
  for (char& ch : s) ch = (char)((ch >= 'A' && ch <= 'Z') ? ch - 'A' + 'a' : ch);
  return s;
}

bool cmdHair(ScriptCtx& c) {
  const std::string w = lower(c.arg(1));
  int style = -1;
  for (int i = 0; i < 8; i++) if (w == kStyles[i]) style = i;
  if (style < 0) { c.fail("hair: bald|short|long|ponytail|mohawk|braids|bun|curls [colour] (got '" + c.arg(1) + "')"); return true; }
  int colour = -1;
  if (c.a.size() > 2) {
    const std::string k = lower(c.arg(2));
    for (int i = 0; i < 10; i++) if (k == kColourNames[i]) colour = i;
    if (colour < 0) { c.fail("hair: colour raven|umber|chestnut|auburn|copper|wheat|flax|ash|silver|plum (got '" + c.arg(2) + "')"); return true; }
  }
  c.game.app.hair = (uint8_t)style;
  if (colour >= 0) c.game.app.hairColor = kColours[colour];
  AnimOps::refreshHero(c.game);
  return true;
}
EMB_SCRIPT_CMD("hair", "hair <style> [colour]: the hero's hair (P2a style slice: bald short long ponytail mohawk braids bun curls; the creator's colour names)", cmdHair);

}  // namespace
