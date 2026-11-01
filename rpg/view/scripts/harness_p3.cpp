// rpg/view/scripts/harness_p3.cpp - the P3 HARNESS lane's script checks (test only; docs/FIX_BATCH_P3.md 8.5).
//   expect face <0-3>     the hero faces 0 down, 1 up, 2 right, 3 left (Actor::face; a `key d` tap in play turns the
//                         hero right: tools/scripts/p3_keys.txt)
#include <string>
#include "rpg/sim/game.h"
#include "rpg/view/script_api.h"

namespace {

bool expFace(ScriptCtx& c) {
  static const char* kName[] = {"down", "up", "right", "left"};
  const std::string w = c.arg(2);
  int want = -1;
  if (w.size() == 1 && w[0] >= '0' && w[0] <= '3') want = w[0] - '0';
  for (int k = 0; k < 4 && want < 0; k++) if (w == kName[k]) want = k;
  if (want < 0) { c.fail("expect face <0-3|down|up|right|left>"); return true; }
  const int have = c.game.pl().face;
  if (have != want)
    c.fail("expect face " + w + ": the hero faces " + std::to_string(have) + " (" + (have >= 0 && have < 4 ? kName[have] : "?") + ")");
  return true;
}

}  // namespace

EMB_SCRIPT_CMD("expect:face", "expect face <0-3|down|up|right|left>: the hero's facing (0 down, 1 up, 2 right, 3 left; P3 B5, test only)", expFace);
