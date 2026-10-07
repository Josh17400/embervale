// rpg_test per-seed check "hunt" (M3c fixer): every beast a hunt quest can name in a biome (ew::huntTargets) can
// actually come out there, from one of its dens (ew::denOf) or as a lone roamer (ew::roamerOf) the spawner lets out
// near home. Lands where no beast lives at all (open sea, bare mountain rock) are skipped. The tables do not depend on
// the seed, so the check runs once per rpg_test process.
#include <cstdio>
#include "rpg/art/art_monsters.h"
#include "rpg/world/biomes.h"
#include "rpg/world/wildlife.h"
#include "tools/tests/tests.h"

namespace {

int huntChecks(uint64_t seed) {
  int bad = 0;
  static bool tablesDone = false;
  if (!tablesDone) {
    tablesDone = true;
    for (int ei = 0; ei < (int)Eco::COUNT; ei++) {
      const Eco e = (Eco)ei;
      bool any = false;
      for (int m = 0; m < (int)art::Monster::COUNT; m++) any = any || ew::canMeet(e, (art::Monster)m);
      art::Monster t[3];
      ew::huntTargets(e, t);
      for (int i = 0; i < 3; i++) {
        if (!any) break;
        if (!ew::canMeet(e, t[i])) {
          out("FAIL: hunt target %d of %s (monster %d) never spawns there\n", i, ecoName(e), (int)t[i]);
          bad++;
        }
      }
    }
  }
  (void)seed;
  return bad;
}
RPG_SEED_CHECK("hunt", huntChecks);

}  // namespace
