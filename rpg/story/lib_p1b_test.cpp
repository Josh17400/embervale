// P1b "New Roads Content" (lead, phase A; VISION_PLAN 15.35): the mechanics test script. `family test`: never offered
// (offerGates refuses it); rpg_test --content-p1b and --story start it by hand and walk every branch, so every P1b
// language addition (dsl.h) is parsed, validated and run once: settle / needs / once site / cooldown / gossip, a person
// `in` a cave, a site `with` filters, deadline, escort (+ else), bout (+ else), is / days / nodays / season / hour,
// do days / census / follow / stay / hostile / calm / ambush / give heavy / reward <band> <pct>, {var.<name>}, the time
// goal, a young person. (gossip is checked on P1b families only: a test script carries none.)
#include "rpg/story/dsl.h"

namespace story {
namespace dsl {

namespace {
const char* const kP1bTest = R"DSL(
script p1b_mechanics
title THE TEST ROAD
archetype p1b mechanics test
tier 2
family test
hook npc any
pitch "IS THERE A TEST ROAD?"
hint "THE TEST ROAD STARTS HERE."
settle village town city
needs level 1
once site
cooldown 3
role giver giver
role home site home
role land kingdom home
role town site town near with min 20
role den site cave near
role carter person male at home
role champ person male at home
role keeper person female in den
role boy person male young at home
var late 0
var paid 0
var lost 0

stage start
  talk giver
  say "{CARTER} TAKES A CRATE TO {TOWN}. WALK WITH HIM. IF HE DIES ON THE ROAD, COME AND TELL ME."
  opt "I'LL WALK WITH HIM." -> road
  opt "NOT TODAY." -> refuse

stage road
  goal escort carter to town wagon
  then duel
  else lost_road
  deadline 6 -> too_late
  journal "WALK {CARTER} AND HIS CRATE TO {TOWN}."
  do give "A HEAVY TEST CRATE" key heavy
  do follow carter

stage duel
  goal bout champ
  then dusk
  else beaten
  journal "{CHAMP} OF {TOWN} WANTS A FAIR FIGHT FOR THE CRATE."
  do stay carter
  do take "A HEAVY TEST CRATE"

stage dusk
  goal time 19 23
  then keeper
  journal "WAIT FOR DUSK, WHEN {BOY} SAYS THE KEEPER COMES OUT."
  do ambush wolf 2

stage keeper
  talk keeper
  say "YOU WON, AND YOU FOUND ME IN {DEN}. IT IS {VAR.LATE} DAYS SINCE YOU SET OUT, BY MY COUNT."
  do days late
  do set lost 2
  opt "IT IS SPRING." if season spring -> paid_end
  opt "IT IS LATE AT NIGHT." if hour 20 4 -> paid_end
  opt "I CAME QUICKLY." if nodays 3 -> paid_end
  opt "I TOOK MY TIME." if days 3 -> hostile_end
  opt "TWO WERE LOST." if is lost 2 -> calm_end
  opt "PAY ME HALF." if novar paid 1 -> paid_end

stage paid_end
  end success
  journal "THE TEST ROAD IS WALKED. {TOWN} HAS ITS CRATE."
  do reward fair 150
  do census home mood 5
  do census home stock planks 6
  do census home price planks 90
  do census town festival
  do remember giver "THE TEST ROAD WAS WALKED."

stage hostile_end
  end success
  journal "THE KEEPER OF {DEN} TURNED ON YOU."
  do hostile keeper
  do reward small 50
  do census home famine 3
  do fact "THE KEEPER OF THE TEST CAVE TURNED ON A TRAVELLER."

stage calm_end
  end success
  journal "THE DEN IS QUIET NOW."
  do calm den
  do census home refugees 2
  do reward small
  do remember keeper "YOU KEPT COUNT. GOOD."

stage lost_road
  end fail
  say "{CARTER} FELL ON THE ROAD TO {TOWN}, AND THE CRATE WITH HIM."
  journal "{CARTER} DIED ON THE ROAD TO {TOWN}."
  do census home mood -5
  do remember giver "{CARTER} NEVER CAME BACK."

stage beaten
  end fail
  say "{CHAMP} STANDS OVER YOU AND TAKES THE CRATE. NOBODY IS HURT BUT YOUR PRIDE."
  journal "{CHAMP} BEAT YOU AND TOOK THE CRATE."
  do remember champ "A FAIR FIGHT. I WON IT."

stage too_late
  end fail
  say "SIX DAYS ON THE ROAD. {TOWN} BOUGHT ITS CRATES ELSEWHERE."
  journal "THE CRATE CAME TOO LATE TO {TOWN}."
  do remember giver "SIX DAYS FOR ONE CRATE."

stage refuse
  end fail
  journal "YOU TURNED DOWN THE TEST ROAD."

# (P1b ENGINE lane) the runtime's own test scripts: rpg_test --p1b-engine plays each mechanic in real game steps
# (a long walk with a follower, a drove, a bout won and lost, a person turned hostile and slain, a calmed cave, people
# waiting inside a cave, a ruin and a camp, a burden, a beacon on a watchtower, the vignette and filter casting)
script p1b_engine_walk
title THE LONG CARRY
archetype p1b engine test
tier 2
family test
hook npc any
pitch "IS THERE A LONG ROAD?"
hint "THE LONG ROAD STARTS HERE."
role giver giver
role home site home
role far site town far
role walker person male at home

stage start
  talk giver
  say "{WALKER} MUST GO TO {FAR}. WALK WITH HIM."
  opt "I WILL." -> road
  opt "NOT TODAY." -> refuse

stage road
  goal escort walker to far walk
  then arrive
  else lost
  journal "WALK {WALKER} TO {FAR}."

stage arrive
  end success
  say "{WALKER} IS SAFE IN {FAR}. HE SHAKES YOUR HAND."
  journal "{WALKER} REACHED {FAR}."
  do reward small
  do remember giver "{WALKER} GOT THERE SAFE."

stage lost
  end fail
  say "{WALKER} FELL ON THE ROAD TO {FAR}."
  journal "{WALKER} DIED ON THE ROAD."

stage refuse
  end fail
  journal "YOU TURNED DOWN THE LONG ROAD."

script p1b_engine_drove
title THE TEST DROVE
archetype p1b engine test
tier 2
family test
hook npc any
pitch "IS THERE A DROVE TO WALK?"
hint "THE DROVE IS READY."
role giver giver
role home site home
role town site town near with min 20
role drover person female at home

stage start
  talk giver
  say "{DROVER} DRIVES THE BEASTS TO {TOWN}. GO WITH HER."
  opt "I WILL." -> road
  opt "NOT TODAY." -> refuse

stage road
  goal escort drover to town drove
  then arrive
  else lost
  journal "DRIVE THE BEASTS TO {TOWN} WITH {DROVER}."

stage arrive
  end success
  say "THE DROVE IS PENNED IN {TOWN}. {DROVER} COUNTS THEM IN."
  journal "THE DROVE REACHED {TOWN}."
  do reward small
  do remember drover "NOT ONE BEAST LOST."

stage lost
  end fail
  say "{DROVER} FELL ON THE ROAD, AND THE BEASTS SCATTERED."
  journal "{DROVER} DIED ON THE DROVE ROAD."

stage refuse
  end fail
  journal "YOU TURNED DOWN THE DROVE."

script p1b_engine_bout
title THE TEST BOUT
archetype p1b engine test
tier 2
family test
hook npc any
pitch "WHO WANTS A FAIR FIGHT?"
hint "SOMEONE HERE WANTS A FAIR FIGHT."
role giver giver
role home site home
role champ person male at home

stage start
  talk giver
  say "{CHAMP} WANTS A BOUT. NOT TO THE DEATH."
  opt "I'LL FIGHT HIM." -> bout
  opt "NOT TODAY." -> refuse

stage bout
  goal bout champ
  then won
  else beaten
  journal "BEAT {CHAMP} IN A FAIR FIGHT."

stage won
  end success
  say "{CHAMP} GETS UP AND GRINS. A FAIR FIGHT, FAIRLY WON."
  journal "YOU BEAT {CHAMP}."
  do reward small
  do remember champ "A FAIR FIGHT, FAIRLY LOST."

stage beaten
  end fail
  say "{CHAMP} HELPS YOU UP. NOBODY IS HURT BUT YOUR PRIDE."
  journal "{CHAMP} BEAT YOU."

stage refuse
  end fail
  journal "YOU TURNED DOWN THE BOUT."

script p1b_engine_places
title THE TEST PLACES
archetype p1b engine test
tier 2
family test
hook npc any
pitch "ARE THERE PLACES TO TEST?"
hint "THE PLACES ARE READY."
role giver giver
role home site home
role tower site watchtower near
role hut site dwelling near with road
role den site cave near
role crypt site ruin near
role camp site camp near
role mill site town near with makes bread
role hermit person female in den
role scholar person male in crypt
role captive person male young in camp
role rogue person male at home

stage start
  talk giver
  say "LIGHT THE BEACON ON {TOWER}, THEN SEE WHO WAITS IN {DEN}."
  opt "I WILL." -> beacon
  opt "NOT TODAY." -> refuse

stage beacon
  goal use beacon in tower
  then hermit
  journal "LIGHT THE BEACON ON {TOWER}."

stage hermit
  talk hermit
  say "YOU FOUND ME IN {DEN}. THE SCHOLAR IS IN {CRYPT}."
  opt "I'LL FIND HIM." -> scholar

stage scholar
  talk scholar
  say "THE BOY IS HELD IN {CAMP}. GO QUIETLY."
  opt "I'M GOING." -> captive

stage captive
  talk captive
  say "THEY TREAT ME WELL ENOUGH. TELL MY MOTHER IN {MILL}."
  opt "I WILL." -> calm

stage calm
  goal wait 1
  then turn
  journal "CARRY THE SACK. {DEN} IS QUIET NOW."
  do calm den
  do give "A HEAVY SACK" key heavy

stage turn
  goal slay rogue
  then done
  journal "{ROGUE} TURNED ON YOU. DEAL WITH HIM."
  do take "A HEAVY SACK"
  do hostile rogue

stage done
  end success
  journal "THE PLACES ARE TESTED."
  do reward small
  do remember giver "THE PLACES ARE TESTED."

stage refuse
  end fail
  journal "YOU TURNED DOWN THE PLACES."
)DSL";
}  // namespace

const char* p1bTestSource() { return kP1bTest; }

}  // namespace dsl
}  // namespace story
