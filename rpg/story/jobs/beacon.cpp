// P1b radiant job kind 15: BEACON CHAIN (content: jobs_c.md section 5; judge notes resolved). JOBS lane.
//
// The old watchtowers have gone dark and the settlement is blind. Carry the fuel out, clear each tower's nest, lay the
// logs (USE BEACON at a hilltop watchtower's fire basket), and at the farthest tower wait for dusk: the watcher posted
// there SEES the thing the giver fears (campfires on the ridge, torches on the old road, the pack gathering, the Mist
// coming in over the reef), the fire is lit in answer, and the watchers carry it tower to tower back home. A beacon here
// is a WARNING RELAY, never a guide light for ships. Two towers for a village, three for a town or a city.
//   A THE LAST WATCHMAN (a middle tower): an old man who has waited forty years for the signal, with dry wood and a grey
//     dog; the bandits of the ridge leave him be (no nest)
//   B THE KEEPER (the last tower of a MIST chain): the keeper holds the headland tower overlooking the reef with a whaling
//     harpoon and a dog, and begs you not to light it: the Mist answers fires
//   C THE DECOY FIRE (the last tower of a land chain): a bandit crew holds it to light ONE false fire at full dark; their
//     captain's tally sheet tells the plan: light two fires, none, or one
//   danger: THE COUNTER holds the nearest tower, counting every traveller on the road; the friend of the one who came
//     back is chained at the tower's foot
// The towers are the wayside watchtowers (ENGINE lane); until they are cast the fallback is the old ruins on the heights
// round about (the spec's fallback: no beacon basket there, so the fuel is laid on arrival, and their nests are the
// dead). Judge: no firearms; the headland tower; the friend at the tower's foot; the watchers sit down as the fuel is laid
// (no timed spawn); a chain within a day of home in any direction (feasible: the offer rate is the towers' own).
#include <map>
#include <string>
#include <vector>
#include "rpg/sim/world.h"
#include "rpg/story/radiant.h"

namespace story {
namespace radiant {

namespace {

const char* const kBody = R"DSL(
title [[=title]]
role giver giver
role home site home
{!fb
role tower1 site watchtower near
role tower2 site watchtower near
}
{?fb
role tower1 site ruin near
role tower2 site ruin near
}
{?x_three
{!fb
role tower3 site watchtower near
}
{?fb
role tower3 site ruin near
}
}
role watcher person [[male|female]] at [[=last]]
{?tw_watchman
role oldman person male at [[=mid]]
}
{?tw_keeper
role keeper person [[male|female]] at [[=last]]
}
{?tw_decoy
role captain person male at [[=last]]
}
{?danger
role counter person male at tower1
role friend person [[male|female]] at tower1
}
var signal 0

stage start
  talk giver
  say "[[=reason]] [[=ask]] [[=codeline]]"
  opt "[[I'LL LIGHT YOUR CHAIN.|GIVE ME THE LOGS.|WAKE THE TOWERS? DONE.]]" -> go1
  opt "[[NOT TODAY.|FIND ANOTHER.|TOO MANY HILLS.]]" -> decline

stage go1
  goal goto tower1
  journal "CARRY THE FUEL OUT TO THE FIRST TOWER, {TOWER1}, {TOWER1.DIR}. CLEAR IT AND LAY THE LOGS."
  do give "[[=fuel]]" ingot heavy
  then [[=after1]]

{!danger
{?x_three|tw_none|tw_keeper|tw_decoy
stage nest1
  goal kill 3 [[=nestfoe]] in tower1
  journal "[[=nestj]] AT {TOWER1}. CLEAR THEM OUT."
  then lay1
}
}

{?danger
stage counter_fight
  goal slay counter
  say "SEVENTEEN CARTS, NINE WALKERS, ONE FOOL WITH A LANTERN. I COUNT THEM ALL. A FIRE ON THIS TOWER SEES MY ROAD. SO I COUNT YOU TOO."
  journal "THE COUNTER HOLDS {TOWER1}, AND COUNTS EVERY TRAVELLER ON THE ROAD. END HIM."
  do hostile counter
  do ambush bandit 2
  then friend_talk

stage friend_talk
  talk friend
  say "YOU CAME. THE COUNTER SAID NO ONE WOULD. I KEPT COUNTING THE DAYS ON THE WALL. FORTY-ONE. IS MY FRIEND ALIVE? TELL THEM I NEVER BLAMED THEM. ONE OF US HAD TO RUN."
  journal "THE FRIEND OF THE ONE WHO CAME BACK WAS CHAINED AT THE FOOT OF {TOWER1}, ALIVE."
  do give "THE COUNTER'S TALLY-STICK" bone
  do moves friend home
  do remember friend "FORTY-ONE DAYS. THEN YOU. I WON'T FORGET IT."
  opt "[[GO HOME. THEY'RE WAITING.|YOUR FRIEND IS ALIVE. GO.]]" -> lay1
}

{?tw_watchman
stage old1
  talk oldman
  say "AT LAST. I KNEW SOMEONE WOULD COME. THE CAPTAIN TOLD ME TO WAIT FOR THE SIGNAL FROM THE FAR TOWER. IT NEVER CAME. I HAVE WAITED FORTY YEARS."
  journal "AN OLD WATCHMAN SITS AT THE FOOT OF {[[=midup]]} WITH DRY WOOD AND A GREY DOG."
  opt "YOUR SIGNAL COMES TONIGHT." -> old_tonight
  opt "THAT WAR ENDED FORTY YEARS AGO." -> old_ended

stage old_tonight
  talk oldman
  say "TONIGHT? ...I SEE IT IN YOUR FACE. THEN I'M READY. GO ON. I'LL WATCH FOR IT."
  journal "THE OLD WATCHMAN WILL LIGHT HIS OWN TOWER WHEN THE FAR FIRE SHOWS."
  do set signal 1
  do fame 1
  opt "[[WATCH FOR IT, THEN.|THE FIRE IS COMING.]]" -> [[=afterold]]

stage old_ended
  talk oldman
  say "...THEN WHAT WAS THE WAITING FOR?"
  journal "THE OLD WATCHMAN SITS DOWN HEAVILY."
  opt "FOR TONIGHT. A FIRE IS COMING." -> old_horn
  opt "NOTHING. I AM SORRY." -> old_gone

stage old_horn
  talk oldman
  say "FOR TONIGHT. ...HERE. MY SIGNAL HORN. FORTY YEARS I KEPT IT DRY. WHEN THE FIRE SHOWS, I'LL LIGHT MINE MYSELF."
  journal "THE OLD WATCHMAN GAVE YOU HIS SIGNAL HORN. HE WILL LIGHT HIS OWN TOWER."
  do set signal 2
  do fame 2
  do give "THE OLD WATCHMAN'S HORN" sword
  opt "[[I'LL KEEP IT DRY TOO.|THANK YOU.]]" -> [[=afterold]]

stage old_gone
  talk oldman
  say "...THEN I'LL GO HOME. COME, DOG."
  journal "THE OLD WATCHMAN WALKED AWAY DOWN THE HILL WITH HIS DOG."
  opt "[[GOODBYE.|...]]" -> [[=afterold]]
}

stage lay1
{!fb
  goal use beacon in tower1
}
{?fb
  goal goto tower1
}
  journal "LAY THE LOGS IN THE FIRE BASKET OF {TOWER1}."
  then go2

stage go2
  goal goto tower2
  journal "THE FIRST TOWER IS LAID, AND {GIVER}'S WATCHER SITS BY ITS COLD HEARTH. ON TO {TOWER2}, {TOWER2.DIR}."
  do notice "YOU LAY THE LOGS. THE OLD SOOT STILL BLACKENS THE STONE."
  do notice "{GIVER}'S WATCHER SITS DOWN WITH A FLINT."
[[=fueltake1]]
  then [[=after2]]

{?x_three
{!tw_watchman
stage nest2
  goal kill 3 [[=nestfoe]] in tower2
  journal "[[=nestj]] AT {TOWER2}. CLEAR THEM OUT."
  then lay2
}
}
{!x_three
{!tw_keeper|tw_decoy
stage nest2
  goal kill 3 [[=nestfoe]] in tower2
  journal "[[=nestj]] AT {TOWER2}. CLEAR THEM OUT."
  then lay2
}
}

stage lay2
{!fb
  goal use beacon in tower2
}
{?fb
  goal goto tower2
}
  journal "LAY THE LOGS IN THE FIRE BASKET OF {TOWER2}."
  then [[=afterlay2]]

{?x_three
stage go3
  goal goto tower3
  journal "TWO TOWERS ARE LAID. THE FARTHEST IS {TOWER3}, {TOWER3.DIR}."
  do notice "YOU LAY THE LOGS. THE OLD SOOT STILL BLACKENS THE STONE."
  do notice "{GIVER}'S WATCHER SITS DOWN WITH A FLINT."
[[=fueltake2]]
  then [[=after3]]

{!tw_keeper|tw_decoy
stage nest3
  goal kill 3 [[=nestfoe]] in tower3
  journal "[[=nestj]] AT {TOWER3}. CLEAR THEM OUT."
  then lay3
}

stage lay3
{!fb
  goal use beacon in tower3
}
{?fb
  goal goto tower3
}
  journal "LAY THE LOGS IN THE FIRE BASKET OF {TOWER3}, THE FARTHEST TOWER."
  then dusk
}

stage dusk
  goal time 19 23
  journal "EVERY TOWER IS LAID. WAIT FOR DUSK AT [[=lastname]]."
  do notice "{GIVER}'S LAST WATCHER SITS BY THE COLD HEARTH."
  do take "[[=fuel]]"
  then [[=afterdusk]]

{!tw_decoy
stage sighting
  talk watcher
  say "[[=sighting]] LIGHT IT. LIGHT IT NOW."
  journal "THE WATCHER AT [[=lastname]] HAS SEEN WHAT {GIVER} FEARED."
{?tw_keeper
  opt "[[LIGHT IT.|THE FIRE, NOW.]]" -> keeper_talk
}
{!tw_keeper
  opt "[[LIGHT IT.|THE FIRE, NOW.]]" -> lit
}
}

{?tw_keeper
stage keeper_talk
  talk keeper
  say "DON'T LIGHT IT. PLEASE. THE MIST ANSWERS FIRES. I'VE SEEN IT. SHAPES. SHIPS THAT SAIL INTO NO PORT I'VE EVER HEARD OF."
  journal "THE KEEPER OF [[=lastname]] BEGS YOU NOT TO LIGHT IT."
  opt "I MUST. THE HARBOUR HAS TO KNOW." -> lit_mist
  opt "LIGHT IT, AND SHUTTER IT AFTER A WATCH." -> shuttered
  opt "TELL ME WHAT YOU HAVE SEEN." -> keeper_story
  opt "[[THEN I WILL NOT.|IT STAYS DARK.]]" -> dark

stage keeper_story
  talk keeper
  say "FIVE WINTERS AGO I LIT THIS TOWER TO WARN THE HARBOUR OF THE MIST. IT OPENED LIKE A DOOR. A SHIP CAME THROUGH, SAILS BLACK, NOBODY ON DECK. IT BOWED TO THE TOWER, AND WENT."
  journal "THE KEEPER TOLD YOU WHAT CAME OUT OF THE MIST FIVE WINTERS AGO."
  do xp 30
  do fact "A KEEPER ON THE HEADLAND SAW A BLACK-SAILED SHIP COME OUT OF THE MIST AND BOW TO THE TOWER."
  opt "I MUST LIGHT IT ANYWAY." -> lit_mist
  opt "LIGHT IT, AND SHUTTER IT AFTER A WATCH." -> shuttered
  opt "[[THEN I WILL NOT.|IT STAYS DARK.]]" -> dark

stage lit_mist
  talk keeper
  say "THEN STAND BACK FROM IT. ...THERE. THE FIRE CATCHES, AND FAR BEHIND, A SPARK ANSWERS. AND OUT ON THE WATER... A LIGHT BLINKS IN THE MIST. THEN ANOTHER."
  journal "THE CHAIN IS LIT. SOMETHING IN THE MIST BLINKED BACK."
  do rumour
  opt "[[...TELL {GIVER}.|IT'S DONE.]]" -> report_lit

stage shuttered
  talk keeper
  say "ONE WATCH. THE WATCHERS ONLY NEED THE FLASH. I WILL SHUTTER IT AT THE SECOND BELL."
  journal "THE CHAIN CARRIED ITS WARNING, AND THE FIRE WAS SHUTTERED AT THE SECOND BELL."
  opt "[[THE SECOND BELL.|AGREED.]]" -> report_shutter
}

{?tw_decoy
stage decoy
  goal slay captain
  say "WRONG TOWER, FRIEND. WRONG NIGHT."
  journal "NO CAMPFIRES ON THE RIDGE, NO TORCHES: ONLY SMOKE AND LAUGHTER FROM [[=lastname]]. A BANDIT CREW HOLDS IT. TAKE IT."
  do hostile captain
  do ambush bandit 3
  then tally

stage tally
  talk watcher
  say "THE CAPTAIN CARRIED THIS. READ IT: DUSK, LIGHT THE TOWER. ONE FIRE, AND THE WATCH RIDES OUT TO AN EMPTY RIDGE. MIDNIGHT: THE EAST GATE, THEN THE GRAIN. WHAT DO WE LIGHT?"
  journal "THE CAPTAIN'S TALLY SHEET: ONE FALSE FIRE, THE WATCH DRAWN OUT, THEN THE EAST GATE AT MIDNIGHT."
  do give "A TALLY SHEET" scroll
  opt "LIGHT TWO FIRES: HOLD THE WALL." if have "A TALLY SHEET" -> two_fires
  opt "LIGHT NO FIRE." -> no_fire
  opt "LIGHT ONE FIRE." -> one_fire
}

{!tw_keeper|tw_decoy
stage lit
  talk watcher
  say "THE FIRE CATCHES, AND SHOWS FOR TEN MILES. FAR BEHIND, A SPARK ANSWERS. THEN ANOTHER, SMALLER, FARTHER STILL. [[=oldlit]]"
  journal "THE CHAIN IS LIT, TOWER TO TOWER, BACK TO {HOME}."
  opt "[[TELL {GIVER}.|THE VALLEY SEES US.]]" -> report_lit
}

{!tw_decoy
stage report_lit
  talk giver
  say "[[=litline]] [[=dangerline]]"
  journal "TELL {GIVER} IN {HOME} THE CHAIN IS LIT."
  opt "[[THE CHAIN IS LIT.|THEY SAW IT.]]" -> lit_end
}

{?tw_keeper
stage report_shutter
  talk giver
  say "THE FIRE WAS LIT, AND SHUTTERED AT THE SECOND BELL. A KEEPER'S CHOICE. FAIR. EVERY BOAT CAME IN."
  journal "TELL {GIVER} THE CHAIN WAS LIT AND SHUTTERED."
  opt "[[THE KEEPER WAS RIGHT TO ASK.|IT WORKED.]]" -> shutter_end
}

{?tw_decoy
stage two_fires
  talk giver
  say "TWO FIRES! THE WATCH HELD THE WALL. THEY CAME TO THE EAST GATE AT MIDNIGHT AND FOUND IT BARRED. THANK YOU."
  journal "TWO FIRES: THE WATCH HELD THE WALL, AND THE EAST GATE WAS BARRED."
  do take "A TALLY SHEET"
  opt "[[THE TALLY SHEET TOLD ME.|THEY WON'T TRY THAT AGAIN.]]" -> two_end

stage no_fire
  talk giver
  say "NO FIRE, AND THE WATCH STAYED HOME. THEY CAME TO THE EAST GATE, AND WE HELD IT, HALF ASLEEP. BARELY."
  journal "NO FIRE: THE EAST GATE HELD, BARELY."
  do take "A TALLY SHEET"
  opt "[[IT HELD.|I'M SORRY IT WAS CLOSE.]]" -> none_end

stage one_fire
  talk giver
  say "ONE FIRE, AND THE WATCH RODE OUT TO AN EMPTY RIDGE. THE EAST GATE HELD, JUST, WITH COOKS ON THE WALL."
  journal "ONE FIRE: THE WATCH RODE OUT TO NOTHING, AND THE EAST GATE HELD WITH COOKS ON THE WALL."
  do take "A TALLY SHEET"
  opt "[[I'M SORRY.|IT HELD, AT LEAST.]]" -> one_end
}

{!tw_decoy
stage lit_end
  end success
  journal "THE BEACON CHAIN FROM {HOME} BURNS AGAIN. [[=litj]]"
  do reward fair [[=pay]]
  do remember giver "THE NIGHT THE TOWERS BURNED AGAIN. {HOME} WILL REMEMBER THE FIRES."
  do fact "THE OLD BEACON CHAIN OF {HOME} WAS LIT AGAIN, AND CARRIED ITS WARNING."
  do census home mood 3
  do census home stock logs -2
[[=litfx]]
}

{?tw_keeper
stage shutter_end
  end success
  journal "THE CHAIN CARRIED THE MIST'S WARNING; THE KEEPER SHUTTERED THE FIRE AT THE SECOND BELL."
  do reward fair 80
  do remember keeper "YOU LISTENED. YOU LIT IT ANYWAY. YOU LISTENED, THOUGH."
  do census home mood 2
  do census home stock logs -1

stage dark
  end fail
  say "THE HARBOUR WAS BLIND TONIGHT, THEN. A FISHING BOAT DID NOT COME BACK."
  journal "YOU LEFT THE HEADLAND TOWER DARK. THE MIST CAME IN UNWARNED, AND A BOAT WAS LOST."
  do remember giver "THE HARBOUR WAS BLIND THE NIGHT YOU WALKED AWAY. A BOAT IS GONE."
  do census home mood -3
}

{?tw_decoy
stage two_end
  end success
  journal "TWO FIRES ON THE FAR TOWER: THE WATCH HELD THE WALL, AND THE BANDITS' PLAN FAILED AT THE EAST GATE."
  do reward fair 100
  do xp 20
  do remember giver "TWO FIRES. THE EAST GATE HELD. YOU READ THEIR PLAN BETTER THAN THEY DID."
  do census home mood 3
  do census home stock logs -2

stage none_end
  end success
  journal "NO FIRE: THE EAST GATE HELD, HALF ASLEEP."
  do reward fair 80
  do remember giver "NO FIRE, AND THE GATE HELD. BARELY."
  do census home mood 1
  do census home stock logs -1

stage one_end
  end success
  journal "ONE FIRE: THE WATCH RODE OUT TO AN EMPTY RIDGE, AND THE EAST GATE HELD WITH COOKS ON THE WALL."
  do reward fair 60
  do remember giver "ONE FIRE. THE WATCH RODE OUT TO NOTHING. WE HELD, JUST."
  do census home mood -1
  do census home stock logs -1
}

stage decline
  end fail
  journal "YOU TURNED DOWN {GIVER}'S BEACON CHAIN."
)DSL";

enum { V_WAR, V_RAID, V_WOLVES, V_MIST };
enum { T_NONE, T_WATCHMAN, T_KEEPER, T_DECOY };

void prep(const JobCtx& c, uint64_t seed, std::map<std::string, std::string>& P) {
  Dice d(seed);
  const bool three = c.extra == 1;
  const int v = c.variant, tw = c.twist;
  P["last"] = three ? "tower3" : "tower2";
  P["lastname"] = three ? "{TOWER3}" : "{TOWER2}";
  P["mid"] = three ? "tower2" : "tower1";
  P["midup"] = three ? "TOWER2" : "TOWER1";
  const int n = three ? 6 : 4;
  P["fuel"] = std::string(n == 6 ? "SIX" : "FOUR") + " TARRED LOGS";
  P["nestfoe"] = c.fallback ? "undead" : "bandit";
  P["nestj"] = c.fallback ? "THE DEAD WALK THE OLD WATCH-RUIN" : "BANDIT ARCHERS HOLD THE TIMBER NEST";
  // which stage follows each arrival: the nest, the old watchman (a middle tower), the counter (the nearest, danger)
  const bool mid1 = tw == T_WATCHMAN && !three, mid2 = tw == T_WATCHMAN && three;
  P["after1"] = c.danger ? "counter_fight" : mid1 ? "old1" : "nest1";
  P["after2"] = mid2 ? "old1" : (three ? "nest2" : (tw == T_KEEPER ? "lay2" : tw == T_DECOY ? "lay2" : "nest2"));
  P["afterold"] = mid1 ? "lay1" : "lay2";
  P["afterlay2"] = three ? "go3" : "dusk";
  P["after3"] = tw == T_KEEPER || tw == T_DECOY ? "lay3" : "nest3";
  P["afterdusk"] = tw == T_DECOY ? "decoy" : "sighting";
  // the fallback lays the fuel on arrival: two logs taken at each tower (the last tower's at dusk)
  P["fueltake1"] = "";
  P["fueltake2"] = "";
  static const char* sight[] = {"A SPRAY OF CAMPFIRES ON THE FAR RIDGE. FORTY, FIFTY. NO HUNTING PARTY. THEY ARE TWO DAYS FROM {HOME}.",
                                "TORCHES ON THE OLD ROAD, IN A LINE. SLOW, AND ARMED. THEY WALK FOR {HOME} AND THINK NOBODY SEES.",
                                "SHAPES ON THE RIDGE AT DUSK, MORE THAN YOU CAN COUNT. THE PACK GATHERS, AND EYES CATCH THE LAST OF THE LIGHT.",
                                "A WALL OF GREY ON THE SEA, MOVING. THE MIST COMES IN OVER THE REEF, FASTER THAN A HORSE CAN RUN."};
  P["sighting"] = sight[v];
  P["oldlit"] = tw == T_WATCHMAN ? "AND AT THE OLD WATCHMAN'S TOWER, A CRY CARRIES ON THE WIND: FORTY YEARS! I SAW IT!" : "";
  static const char* lit[] = {"THE PATROL WAS ON THE FRONTIER BY MORNING. THE RAIDERS FOUND THE GATES BARRED AND TURNED BACK. {HOME} WILL REMEMBER THE FIRES.",
                              "THE WALLS WERE MANNED BEFORE THE TORCHES REACHED THE ROAD'S END. THEY LOOKED UP AT US AND WALKED AWAY.",
                              "THE VILLAGES BARRED THEIR DOORS AND BROUGHT THE FLOCKS IN BEFORE THE PACK CAME DOWN. THEY SAW YOUR FIRE. NOT A SHEEP LOST.",
                              "THE FIRES CAME DOWN THE HILLS ONE BY ONE, AND THE BELL RANG. EVERY BOAT WAS IN BEFORE THE MIST TOUCHED THE QUAY."};
  std::string ll = c.danger ? "YOU FOUND THE FRIEND ALIVE? THEY'RE AT THE GATE, WEEPING, THE TWO OF THEM. AND THE FIRES CAME DOWN THE HILLS IN TIME." : lit[v];
  if (tw == T_WATCHMAN) ll += " AN OLD WATCHMAN LIT HIS OWN? THE OLD FOOL. I'LL SEND A BED AND A PENSION.";
  P["litline"] = ll;
  P["dangerline"] = "";
  P["litj"] = v == V_MIST ? "EVERY BOAT CAME IN BEFORE THE MIST." : v == V_WOLVES ? "THE FLOCKS CAME IN BEFORE THE PACK." : "THE WALLS WERE MANNED IN TIME.";
  P["pay"] = std::to_string(100 + (three ? 30 : 0) + (c.danger ? 30 : 0));
  std::string fx;
  if (v == V_WAR || v == V_RAID) fx += "  do deed defended\n";
  if (c.danger) fx += "  do fame 2\n";
  P["litfx"] = fx;
  P["codeline"] = v == V_WAR || v == V_RAID ? "ONE FIRE: THEY COME, RIDE OUT. TWO FIRES: THEY ARE AT THE GATE. LIGHT ONE, UNLESS YOU KNOW BETTER."
                                            : "LIGHT THE LAST AT DUSK, WHEN THE WATCHER SEES IT COMING.";
  std::vector<std::string> R;
  const Role t = (Role)c.trade;
  switch (v) {
    case V_WAR: R = {"THE OLD SIGNAL TOWERS AROUND {HOME} HAVE BEEN DARK SINCE MY FATHER'S TIME. WITH THE REALM AT WAR, I WANT TO SEE THE ENEMY BEFORE THE ENEMY SEES US."}; break;
    case V_RAID: R = {"THE RAIDERS WERE AT THE GATE BEFORE ANYONE KNEW. THE TOWERS USED TO WARN US, AND NO ONE HAS LIT THEM IN A GENERATION."}; break;
    case V_WOLVES: R = {t == Role::Hunter ? "THE WOLVES RUN THE HILLS EVERY WINTER, AND NOBODY WARNS THE VILLAGES ANY MORE. A FIRE ON THE RIDGE SAYS: BAR THE DOORS."
                                          : "THE WOLVES CAME DOWN FROM THE HIGH GROUND AND TOOK THE FARMS BEFORE ANYONE KNEW. A FIRE ON THE RIDGE WOULD HAVE WARNED THE VALLEY."}; break;
    default:
      R = {t == Role::Priest ? "SOMETHING WALKS IN THE MIST AT NIGHT. THE OLD FIRES TOLD US WHEN IT WAS COMING, SO WE COULD BAR THE DOORS. I WOULD RATHER KNOW."
                             : "THE MIST ROLLS IN FASTER THAN A BELL CAN RING, AND THE BOATS ARE OUT BEFORE ANYONE KNOWS. THE OLD HILL FIRES CARRIED THE WORD. THEY ARE COLD."};
      break;
  }
  if (c.danger) R = {"SOMETHING HAS TAKEN THE NEAREST TOWER. I SENT TWO PEOPLE TO LIGHT IT AND ONE CAME BACK. THE ONE WHO CAME BACK WILL NOT SAY WHAT WAS SEEN."};
  R.push_back("THE LAST TIME ANYONE LIT A TOWER, THEY SET THE WHOLE HILL ON FIRE AND BURNED A SHEPHERD'S HAT. YOU WILL BE MORE CAREFUL, I HOPE.");
  P["reason"] = R[(size_t)d.below((int)R.size())];
  P["ask"] = d.pick({"LAY THE FUEL IN EVERY TOWER, THEN LIGHT THE LAST.", "WAKE THE CHAIN.", "LET THE VALLEY SEE US AGAIN."});
  std::vector<std::string> T = {"LIGHT THE CHAIN", "THE DARK TOWERS", "THE VALLEY'S BEACON"};
  if (v == V_WAR) { T.push_back("FIRES ON THE RIDGE"); T.push_back("BLIND ON THE BORDER"); }
  if (v == V_RAID || v == V_WOLVES) T.push_back("A WARNING FIRE");
  if (v == V_MIST) { T.push_back("FIRES BEFORE THE FOG"); T.push_back("WARN THE HARBOUR"); }
  if (v == V_WOLVES) T.push_back("BAR THE DOORS");
  if (c.danger) T.push_back("THE FIRST TOWER");
  P["title"] = T[(size_t)d.below((int)T.size())];
}

bool choose(const JobCtx& w, uint64_t dice, JobCtx& out) {
  Dice d(dice);
  out = w;
  switch ((Role)w.trade) {
    case Role::Guard: out.variant = w.war ? V_WAR : V_RAID; break;
    case Role::Jarl: out.variant = w.war ? V_WAR : V_WOLVES; break;
    case Role::Hunter: out.variant = V_WOLVES; break;
    case Role::Fisher: if (!w.coast) return false; out.variant = V_MIST; break;
    case Role::Priest: if (!w.coast) return false; out.variant = V_MIST; break;
    default: return false;
  }
  out.extra = w.tier >= 1 ? 1 : 0;   // three towers for a town or a city
  const int roll = d.below(100);
  out.twist = T_NONE;
  if (roll < 25) out.twist = T_WATCHMAN;
  else if (roll < 50 && out.variant == V_MIST) out.twist = T_KEEPER;
  else if (roll < 50 && (out.variant == V_WAR || out.variant == V_RAID)) out.twist = T_DECOY;
  if (w.danger && out.twist == T_WATCHMAN && !out.extra) out.twist = T_NONE;   // the counter holds the nearest tower
  return true;
}

JobCtx walk(Role t, int v, int twist, int towers3, bool danger, bool coast = false, bool war = false) {
  JobCtx c;
  c.trade = (int)t; c.variant = v; c.twist = twist; c.extra = towers3; c.danger = danger; c.coast = coast; c.war = war; c.season = 2;   // (the watchtowers first; composeCast falls back to the ruins)
  c.tier = towers3 ? 1 : 0;
  return c;
}

}  // namespace

const KindDef& kindBeacon() {
  static const KindDef K = [] {
    KindDef k;
    k.id = "beacon";
    k.title = "BEACON CHAIN";
    k.lead = "WANTS THE OLD BEACON CHAIN LIT";
    k.body = kBody;
    k.variants = {"war", "raid", "wolves", "mist"};
    k.twists = {"none", "watchman", "keeper", "decoy"};
    k.extras = {"two", "three"};
    k.trades = {(int)Role::Guard, (int)Role::Jarl, (int)Role::Hunter, (int)Role::Fisher, (int)Role::Priest};
    k.fallback = true;
    k.choose = choose;
    k.prep = prep;
    k.walks = {walk(Role::Hunter, V_WOLVES, T_WATCHMAN, 0, false), walk(Role::Fisher, V_MIST, T_KEEPER, 1, false, true),
               walk(Role::Guard, V_RAID, T_DECOY, 1, false), walk(Role::Jarl, V_WAR, T_WATCHMAN, 1, true, false, true),
               walk(Role::Priest, V_MIST, T_NONE, 0, true, true)};
    return k;
  }();
  return K;
}

}  // namespace radiant
}  // namespace story
