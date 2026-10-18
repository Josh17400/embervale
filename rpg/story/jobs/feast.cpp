// P1b radiant job kind 14: FEAST WATCH (content: jobs_c.md section 4; judge notes resolved). JOBS lane.
//
// A wedding, a harvest supper, a boat's launch, an envoys' supper or the longest-night feast is TONIGHT. Offered by
// day: provision the table at the market, be there by dusk (a missed dusk fails the job), keep the door, the guests and
// the cups until the dawn toast. What goes wrong depends on what is near and who is at the table:
//   THE ROAST   the smell calls the wolves out of the woods: herd the dancers indoors (a decoy, or a name the guests
//               have heard), or stand in the yard with them in it
//   THE GIFTS   (wedding dowry, launch purse) bandits are coming for the chests: hide them under the head table, in the
//               cellar with you on the stair, or in a decoy chest of turnips on the gate-cart
//   THE FEUD    two houses, hands on knives: sit them down (a name they have heard, fame 8), take it outside (a bout,
//               not to the death), or let them fight
//   THE CUP     (weddings) someone means to spoil the night with a purge in the cup of the one being married: gather
//               the clues from dusk; at the midnight toast name the culprit before the cup is drunk. A wrong name
//               makes a scene, a fresh cup is poured, and the culprit tries again at the dawn toast: a second chance
//   THE UNCLE   (weddings, light) the old uncle fights a wedding guest at every family wedding: arm-wrestle him, sit
//               him down, or let him
// Judge: the clue sequence is spelled out (above); nobody is called groom or bride by sex; the long night is any
// culture's; the roast's beasts are wolves; the feud's fallback is the bout word (the ENGINE lane's yield).
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
role seller person [[male|female]] at home
{?v_wedding|v_envoys
role spouse person [[male|female]] at home
}
{?v_envoys
role envoytown site town near
}
{?tw_roast
role guest person [[male|female]] at home
}
{?tw_feud
role hothead person male at home
}
{?tw_cup
role servant person [[male|female]] at home
role dish person [[male|female]] at home
role cousin person [[male|female]] at home
role rival person [[male|female]] at home
role stranger person [[male|female]] at home
}
{?tw_uncle
role uncle person male at home
}
var saved 0
var spoiled 0
var wrong 0

stage start
  talk giver
  say "[[=event]] [[=worry]] [[=ask]]"
  opt "[[I'LL KEEP YOUR DOOR.|TILL DAWN, THEN.|I'LL BE THERE BY DUSK.]]" -> provision
  opt "[[NOT TONIGHT.|I HAVE PLANS.|FIND ANOTHER BLADE.]]" -> decline

stage provision
  talk seller
  say "FOR THE FEAST TONIGHT? [[=provline]] PAID FOR BY {GIVER}. CARRY IT CAREFULLY."
  journal "{GIVER} HOLDS A FEAST TONIGHT. FETCH THE PROVISIONS FROM {SELLER} IN {HOME}, THEN BE THERE BY DUSK."
  do give "[[=provision]]" [[=provicon]]
  opt "[[CAREFULLY. GOT IT.|TO THE FEAST, THEN.]]" -> dusk

stage dusk
  goal time 18 23
  journal "BRING [[=provision]] TO {GIVER}'S FEAST AND BE THERE BY DUSK."
  then host

stage host
  talk giver
  say "[[=hostline]]"
  journal "THE FEAST AT {HOME} HAS BEGUN. KEEP THE DOOR, THE GUESTS AND THE CUPS UNTIL DAWN."
  do take "[[=provision]]"
  deadline 1 -> missed
  opt "[[I'LL BE EVERYWHERE.|I'M WATCHING.|ENJOY YOUR FEAST.]]" -> [[=first]]

{?tw_roast
stage roast_wait
  goal time 22 24
  journal "THE DANCING HAS MOVED OUT BY THE FIRE. WATCH THE DARK BEYOND IT."
  then roast

stage roast
  talk guest
  say "THE DOGS HAVE STOPPED BARKING. ...THAT IS WORSE THAN THE BARKING, ISN'T IT? NO MATTER! [[=guestline]]"
  journal "THE DOGS WENT QUIET. WOLVES ARE COMING TO THE SMELL, AND THE GUESTS ARE IN THE YARD."
  opt "[[=decoy]]" -> herded
  opt "PLEASE, GO INSIDE. IT IS COLD." check fame 4 -> herded else yard
  opt "[[I'LL STAND HERE.|LET THEM DANCE. I'LL HOLD.]]" -> yard

stage herded
  goal time 1 5
  say "[[=herdline]]"
  journal "THE GUESTS ARE INDOORS. HOLD THE YARD AGAINST THE WOLVES UNTIL THE SMALL HOURS."
  do set saved 1
  do ambush wolf [[=wave]]
  then dawn

stage yard
  goal time 1 5
  journal "THE GUESTS DANCE ON IN THE YARD. HOLD IT AGAINST THE WOLVES UNTIL THE SMALL HOURS."
  do ambush wolf [[=wavehard]]
  then dawn
}

{?tw_gifts
stage gifts
  talk giver
  say "[[=giftsline]]"
  journal "BANDITS ARE COMING FOR [[=prize]]. CHOOSE WHERE IT GOES."
  opt "[[=hideopen]]" -> gifts_hall
  opt "[[=hidestair]]" -> gifts_stair
  opt "[[=hidedecoy]] (15 GOLD)" if gold 15 -> gifts_decoy

stage gifts_hall
  goal time 1 5
  journal "THE BANDITS COME INTO THE FEAST ITSELF. HOLD UNTIL THE SMALL HOURS."
  do ambush bandit [[=wavehard]]
  then dawn

stage gifts_stair
  goal time 1 5
  journal "HOLD [[=stair]] AGAINST THE BANDITS UNTIL THE SMALL HOURS."
  do set saved 1
  do ambush bandit [[=wave]]
  then dawn

stage gifts_decoy
  goal time 1 5
  journal "THE BANDITS TOOK THE DECOY AND RAN. CATCH THEM AWAY FROM THE GUESTS, AND HOLD UNTIL THE SMALL HOURS."
  do gold -15
  do set saved 1
  do ambush bandit [[=waveeasy]]
  then dawn
}

{?tw_feud
stage feud_wait
  goal time 22 24
  journal "THE NIGHT WEARS ON. WATCH THE TABLES."
  then feud

stage feud
  talk hothead
  say "[[=feudline]]"
  journal "TWO SIDES AT THE FEAST, HANDS ON KNIVES. THE ROOM HAS GONE SILENT."
  opt "SIT DOWN, BOTH OF YOU." check fame 8 -> feud_peace else feud_brawl
  opt "[[OUTSIDE. NOW.|TAKE IT OUTSIDE.]]" -> feud_brawl
  opt "[[LET THEM FIGHT.|NOT MY QUARREL.]]" -> feud_spoiled

stage feud_peace
  talk hothead
  say "[[=peaceline]]"
  journal "THE FEUD SAT DOWN. A CUP WAS POURED ACROSS THE TABLE."
  do set saved 1
  do xp 20
  opt "[[TO THE NIGHT.|DRINK, THEN.]]" -> watch_out

stage feud_brawl
  goal bout hothead
  say "WHO ARE YOU TO SAY SIT? ...OUTSIDE, THEN!"
  journal "BEAT {HOTHEAD} IN THE YARD. NOT TO THE DEATH: A FEAST IS NO PLACE FOR A GRAVE."
  then watch_out
  else watch_out

stage feud_spoiled
  goal time 1 5
  journal "THEY FOUGHT. SOMEONE WAS HURT, AND THE FEAST IS SPOILED. SEE IT THROUGH TO DAWN."
  do set spoiled 1
  then dawn

stage watch_out
  goal time 1 5
  journal "THE FEUD IS SETTLED, ONE WAY OR ANOTHER. SEE THE FEAST THROUGH TO DAWN."
  then dawn
}

{?tw_cup
stage servant_talk
  talk servant
  say "[[=servantline]]"
  journal "A LETTER SAID WATCH THE CUPS. THE SWEET LOAF IS BROKEN AT MIDNIGHT, WITH A TOAST. FIND OUT WHO MEANS TO SPOIL IT."
  opt "[[THANK YOU. ANYTHING ELSE?|WHO ELSE SAW?]]" -> dish_talk

stage dish_talk
  talk dish
  say "[[=dishline]]"
  journal "THE SERVANT SAW A HAND, AND THE DISH-WASHER FOUND A VIAL. THREE GUESTS ARE SUSPECT: {COUSIN}, {RIVAL} AND {STRANGER}."
  opt "[[KEEP IT HIDDEN.|SAY NOTHING YET.]]" -> midnight

stage midnight
  goal time 23 24
  journal "AT MIDNIGHT THE LOAF IS BROKEN AND THE CUPS RAISED. NAME THE CULPRIT BEFORE {SPOUSE} DRINKS."
  then accuse

stage accuse
  talk giver
  say "THE LOAF IS ON THE TABLE AND THE CUPS ARE FULL. YOU SAID SOMEONE MEANS TO SPOIL IT. WHO? SAY IT NOW, BEFORE {SPOUSE} DRINKS."
  journal "NAME THE CULPRIT TO {GIVER} BEFORE THE MIDNIGHT TOAST."
  opt "{COUSIN}, THE COUSIN." -> [[=accC]]
  opt "{RIVAL}, THE COOK'S RIVAL." -> [[=accR]]
  opt "{STRANGER}, THE STRANGER." -> [[=accS]]

stage wrong1
  talk giver
  say "AN INNOCENT GUEST, ACCUSED AT MY TABLE? ...I'LL POUR {SPOUSE} A FRESH CUP, AND TAKE THAT POOR SOUL ASIDE WITH A CUP OF APOLOGY. WATCH THE DAWN TOAST. IF YOU'RE RIGHT ABOUT A CULPRIT, THEY'LL TRY AGAIN."
  journal "YOU NAMED THE WRONG GUEST. A FRESH CUP WAS POURED. THE CULPRIT WILL TRY AGAIN AT THE DAWN TOAST."
  do set wrong 1
  do remember giver "YOU ACCUSED THE WRONG GUEST AT MY TABLE. WE MANAGED."
  opt "[[I'LL WATCH THE DAWN TOAST.|I WON'T MISS IT TWICE.]]" -> second_wait

stage second_wait
  goal time 5 7
  journal "AT THE DAWN TOAST THE CULPRIT WILL TRY AGAIN. WATCH THE CUPS."
  then second

stage second
  talk giver
  say "THE DAWN TOAST. LOOK, A HAND OVER THE CUP AGAIN. WHO? QUICKLY."
  journal "NAME THE CULPRIT AT THE DAWN TOAST."
  opt "{COUSIN}, THE COUSIN." -> [[=accC2]]
  opt "{RIVAL}, THE COOK'S RIVAL." -> [[=accR2]]
  opt "{STRANGER}, THE STRANGER." -> [[=accS2]]

stage confess
  talk [[=culprit]]
  say "[[=confession]]"
  journal "THE CULPRIT CONFESSED. THE CUP WAS NEVER DRUNK."
  do set saved 1
  opt "[[=confessopt]]" -> [[=afterconfess]]

{?danger
stage culprit_runs
  goal slay [[=culprit]]
  say "PAID BY A HOUSE I WILL NOT NAME. AND NOW I RUN!"
  journal "THE STRANGER DREW A KNIFE AND RAN. STOP THEM."
  do hostile [[=culprit]]
  then dawn
}

stage cup_lost
  talk giver
  say "WRONG AGAIN, AND THE CUP WAS DRUNK. {SPOUSE} SPENT THE MORNING BEHIND THE BARN. A PURGE, NOT A POISON. THE DAY IS SPOILED, NOT ENDED."
  journal "THE CULPRIT WAS NOT CAUGHT. THE CUP WAS DRUNK, AND THE DAY SPOILED."
  do set spoiled 1
  opt "[[I'M SORRY.|I GOT IT WRONG.]]" -> dawn
}

{?tw_uncle
stage uncle_wait
  goal time 22 24
  journal "THE OLD UNCLE HAS BEEN DRINKING SINCE DUSK. KEEP HIM IN HIS CHAIR, KINDLY."
  then uncle_talk

stage uncle_talk
  talk uncle
  say "I'LL FIGHT {SPOUSE}! WHO'S WITH ME?! NOBODY IS GOOD ENOUGH FOR MY NIECE! ...{SPOUSE.HE} JUST SAID: UNCLE, WE TALKED ABOUT THIS."
  journal "THE OLD UNCLE IS ON HIS FEET, FISTS UP."
  opt "ARM-WRESTLE ME INSTEAD." check level 3 -> uncle_won else uncle_lost
  opt "SIT DOWN, UNCLE." check fame 8 -> uncle_sits else uncle_shoulder
  opt "[[LET HIM.|LET THEM SORT IT OUT.]]" -> uncle_brawl

stage uncle_won
  talk uncle
  say "WELL, BLOW ME DOWN. A WORTHY ONE! SIT, SIT. POUR THIS ONE A CUP!"
  journal "YOU PUT THE UNCLE'S HAND DOWN. HE SAT, DELIGHTED."
  do set saved 1
  do xp 20
  opt "[[A CUP, THEN.|NO HARD FEELINGS.]]" -> uncle_rest

stage uncle_lost
  talk uncle
  say "HA! SEE? AGAIN, AGAIN! ...ANOTHER TIME. I'M TIRED. GOOD ARM ON YOU, FOR A STRANGER."
  journal "THE UNCLE WON THE ARM-WRESTLE AND SAT DOWN, DELIGHTED ALL THE SAME."
  do set saved 1
  opt "[[YOU WIN.|NEXT TIME.]]" -> uncle_rest

stage uncle_sits
  talk uncle
  say "...FINE. FINE! I'M SITTING. SOMEONE BRING ME CAKE."
  journal "THE UNCLE SAT DOWN AT YOUR WORD."
  do set saved 1
  do xp 20
  opt "[[CAKE, THEN.|GOOD MAN.]]" -> uncle_rest

stage uncle_shoulder
  talk uncle
  say "NOT YOU TOO! ...OOF. ALL RIGHT. ALL RIGHT, I'M DOWN. NO HARM DONE."
  journal "THE UNCLE RUSHED. YOU STOPPED HIM WITH A SHOULDER, AND NOBODY WAS HURT."
  do set saved 1
  opt "[[SORRY, UNCLE.|SIT TIGHT.]]" -> uncle_rest

stage uncle_brawl
  goal time 1 5
  journal "THE UNCLE AND {SPOUSE} BRAWLED. A NOSE IS BROKEN. SEE THE NIGHT THROUGH TO DAWN."
  do set spoiled 1
  then dawn

stage uncle_rest
  goal time 1 5
  journal "THE UNCLE IS IN HIS CHAIR. SEE THE NIGHT THROUGH TO DAWN."
  then dawn
}

stage dawn
  goal time 5 9
  journal "SEE THE FEAST AT {HOME} THROUGH TO THE DAWN TOAST."
  then toast

stage toast
  talk giver
  say "A {GIVER.PEOPLE} TOAST, THEN: TO THE ONE WHO KEPT THE DOOR! [[=toastextra]]"
  journal "THE DAWN TOAST AT {HOME}."
  opt "[[TO THE NIGHT.|TO THE HOUSE.]]" if is saved 1 if novar spoiled 1 -> quiet_end
  opt "[[IT WAS A LONG NIGHT.|TO THE MORNING.]]" if novar saved 1 if novar spoiled 1 -> shaken_end
  opt "[[I'M SORRY ABOUT THE NIGHT.|IT COULD HAVE GONE BETTER.]]" if is spoiled 1 -> spoiled_end

stage quiet_end
  end success
  say "[[=quiet]]"
  journal "THE FEAST AT {HOME} RAN FROM DUSK TO DAWN, AND NOBODY WAS HURT."
  do reward fair 125
  do give "A BASKET FROM THE FEAST" bread
  do remember giver "THE NIGHT YOU KEPT THE DOOR. THEY STILL ASK FOR YOU BY NAME."
  do census home festival
  do census home mood 4

stage shaken_end
  end success
  say "[[=shaken]]"
  journal "THE FEAST AT {HOME} SURVIVED THE NIGHT, SHAKEN BUT WHOLE."
  do reward fair 100
  do remember giver "A LONG NIGHT, THE FEAST. WE GOT THROUGH IT."
  do census home festival
  do census home mood 2

stage spoiled_end
  end success
  say "THE NIGHT IS OVER, AND NOT THE NIGHT I WANTED. YOU STOOD THE DOOR, AT LEAST. HERE: TWO THIRDS OF THE FEE."
  journal "THE FEAST AT {HOME} WAS SPOILED. {GIVER} PAID TWO THIRDS OF THE FEE."
  do reward fair 67
  do remember giver "THE FEAST WAS SPOILED. I DON'T BLAME YOU. NOT ENTIRELY."
  do census home mood -2
  do census home stock produce 2

stage missed
  end fail
  journal "THE FEAST AT {HOME} WENT ON WITHOUT ITS GUARD. {GIVER} IS NOT PLEASED."
  do remember giver "YOU NEVER CAME TO THE FEAST. I WATCHED THE DOOR MYSELF."

stage decline
  end fail
  journal "YOU TURNED DOWN {GIVER}'S FEAST."
)DSL";

enum { V_WEDDING, V_HARVEST, V_LAUNCH, V_ENVOYS, V_LONGNIGHT };
enum { T_ROAST, T_GIFTS, T_FEUD, T_CUP, T_UNCLE };
enum { P_SPIT, P_FISH, P_BREAD };

void prep(const JobCtx& c, uint64_t seed, std::map<std::string, std::string>& P) {
  Dice d(seed);
  const Role t = (Role)c.trade;
  const int v = c.variant, tw = c.twist, prov = c.extra;
  // the event: who weds whom (nobody named by sex), and the second sentence by the threat
  std::string ev;
  switch (v) {
    case V_WEDDING:
      ev = t == Role::Innkeeper ? "MY DAUGHTER WEDS {SPOUSE} AT DUSK TONIGHT, AND HALF THE COUNTRYSIDE IS COMING."
         : t == Role::Merchant ? "TWO TRADING HOUSES MARRY TONIGHT, MY NIECE TO {SPOUSE}, WITH THE WHOLE GUILD AT THE TABLE."
         : t == Role::Priest ? "I HANDFAST {SPOUSE} AT THE SHRINE AT DUSK TONIGHT, AND THE SUPPER IN MY YARD FOLLOWS."
                             : "MY HEIR WEDS {SPOUSE} TONIGHT IN THE GREAT HALL, BEFORE EVERY HOUSE OF NOTE.";
      break;
    case V_HARVEST: ev = d.pick({"THE HARVEST FEAST IS ON THE THRESHING FLOOR TONIGHT, AS EVERY YEAR.", "THE LAST SHEAF IS IN, AND THE WHOLE VALLEY EATS IN MY BARN TONIGHT."}); break;
    case V_LAUNCH: ev = d.pick({"A NEW BOAT TAKES THE WATER AT DUSK TONIGHT, AND THE QUAY FEASTS TILL MORNING.", "THE BOAT WE BUILT ALL WINTER GOES IN AT DUSK, AND THE QUAY HAS A FEAST WAITING."}); break;
    case V_ENVOYS: ev = d.pick({"ENVOYS FROM {ENVOYTOWN} SIT AT MY TABLE TONIGHT, AND THEIR GUARDS STAND IN MY YARD.", "A TREATY SUPPER IS TONIGHT, WITH ENVOYS FROM {ENVOYTOWN} AT MY HIGH TABLE."}); break;
    default: ev = t == Role::Innkeeper ? "THE LONGEST NIGHT FILLS MY INN TO THE RAFTERS TONIGHT, AND EVERY BENCH IS SPOKEN FOR."
                                       : "THE LONGEST NIGHT OF THE YEAR IS TONIGHT, AND THE WHOLE HOLD FEASTS IN MY HALL."; break;
  }
  P["event"] = ev;
  static const char* roastW[] = {"THE SUPPER SMELL WILL CALL THE WOLVES OUT OF THE WOODS, AND I WANT THE DANCING LEFT IN PEACE.",
                                 "THE WOLVES HAVE BEEN CLOSE ALL WEEK, SO I WANT A BLADE AT THE BARN DOOR.",
                                 "ROAST SMOKE CARRIES FOR MILES, AND THE WOLVES WILL FOLLOW IT TO THE QUAY.",
                                 "THE WOLVES CAME TO MY WALLS ONCE ALREADY THIS YEAR. I CANNOT HAVE ENVOYS EATEN BETWEEN COURSES.",
                                 "THE WOLVES HAVE BEEN HUNGRY ALL WINTER, AND A FEAST SMELLS LIKE A PROMISE TO THEM."};
  static const char* feudW[] = {"TWO HOUSES AT THE TABLE HAVE AN OLD GRUDGE, AND I NEED SOMEONE WHO OWES NEITHER SIDE A FAVOUR.",
                                "THE NEIGHBOUR WHOSE PIGS RUINED MY BARLEY IS COMING TONIGHT. BOTH FAMILIES CARRY KNIVES FOR BREAD.",
                                "THE RIVAL CREW IS COMING. THEIR NETS WERE CUT LAST SPRING, AND MY CREW HAS SAID NOTHING SINCE.",
                                "THE ENVOYS' GUARDS AND MY OWN HAVE OLD BLOOD BETWEEN THEM. I NEED SOMEONE NEITHER SIDE OWES A FAVOUR.",
                                "THE HALL FILLS WITH ARMED FOLK WHO HAVE BEEN INDOORS TOO LONG, AND TWO FAMILIES THAT HATE EACH OTHER SIT ON ONE BENCH."};
  switch (tw) {
    case T_ROAST: P["worry"] = roastW[v]; break;
    case T_GIFTS: P["worry"] = v == V_WEDDING ? "THE DOWRY SITS ON THE HIGH TABLE, THE OUTLAWS KNOW IT, AND I CANNOT WATCH THE DOOR AND POUR AT ONCE."
                                              : "THE LAUNCH PURSE SITS IN A BOX UNDER THE TABLE, AND THE OUTLAWS KNOW THE DATE."; break;
    case T_FEUD: P["worry"] = feudW[v]; break;
    case T_CUP: P["worry"] = c.danger ? "ONE GUEST IS NOT ON MY LIST, AND STANDS TOO NEAR THE CUPS. KEEP THE CUPS." : "AN UNSIGNED LETTER SAYS WATCH THE CUPS. SOMEONE MEANS TO SPOIL THE NIGHT."; break;
    default:
      P["event"] = "THE OLD UNCLE GETS DRUNK AND TRIES TO FIGHT WHOEVER IS MARRYING AT EVERY WEDDING IN THE FAMILY. {SPOUSE} WEDS AT DUSK TONIGHT.";
      P["worry"] = "IT MUST NOT HAPPEN AGAIN. KEEP THE UNCLE IN THE CHAIR, KINDLY.";
      break;
  }
  P["ask"] = d.pick({"KEEP THE DOOR TILL DAWN.", "BRING THE FOOD AND WATCH THE NIGHT.", "SEE THEM FED, AND SEE THEM SAFE."});
  static const char* provs[] = {"A PIG FOR THE SPIT", "THREE BASKETS OF FISH", "THE FEAST BREAD"};
  static const char* provIcons[] = {"meat", "meat", "bread"};
  static const char* provLines[] = {"THE PIG FOR THE SPIT, DRESSED AND READY.", "THREE BASKETS OF FISH, FRESH THIS MORNING.", "THE FEAST BREAD, STILL WARM."};
  P["provision"] = provs[prov]; P["provicon"] = provIcons[prov]; P["provline"] = provLines[prov];
  static const char* hosts[] = {
      "YOU CAME. THE FIDDLER IS TUNING. THE SWEET LOAF IS BROKEN AT MIDNIGHT. IF ANYTHING GOES WRONG, I WILL PRETEND I DID NOT SEE IT UNTIL IT IS FIXED. YOU UNDERSTAND.",
      "THE LAST SHEAF IS IN. THE FIRST JUG IS OPEN. KEEP YOUR BLADE WHERE THE CHILDREN CANNOT FIND IT, AND NEAR THE BARN DOOR, WHERE YOU CAN.",
      "THE BOAT IS ON THE SLIP, DECKED WITH RIBBONS. WHEN THE ROPE IS CUT, IT GOES IN. SIT BY THE ROPE, AND WATCH WHO WALKS UP THE QUAY.",
      "THE ENVOYS ARE IN THE HALL. THEIR GUARDS ARE IN THE YARD, WITH MINE. SMILE AT EVERYONE. TRUST NO ONE'S RIGHT HAND. BE EVERYWHERE.",
      "THE LONGEST NIGHT. THE WINE IS WARM, THE FIRE IS HIGH, AND EVERYONE HAS A KNIFE. KEEP THE DOOR, KEEP THE PEACE, AND KEEP YOUR CUP FULL."};
  P["hostline"] = hosts[v];
  P["first"] = tw == T_ROAST ? "roast_wait" : tw == T_GIFTS ? "gifts" : tw == T_FEUD ? "feud_wait" : tw == T_CUP ? "servant_talk" : "uncle_wait";
  P["wave"] = std::to_string(3 + c.tier);
  P["wavehard"] = std::to_string(4 + c.tier);
  P["waveeasy"] = std::to_string(2 + c.tier);
  // the roast, by what is on the fire
  static const char* guest[] = {"I CAN SMELL THE ROAST ON THE WIND! I'M GOING TO BASTE IT MYSELF!", "I CAN SMELL THE FISH ON THE FIRE! I'M GOING TO TURN THEM MYSELF!",
                                "THE OVENS SMELL LIKE HEAVEN! I'M GOING TO STEAL THE FIRST LOAF!"};
  static const char* decoy[] = {"THE COOK IS CARVING INSIDE NOW!", "THE COOK IS SERVING FISH INSIDE!", "THE FIRST LOAF IS BROKEN INSIDE!"};
  static const char* herd[] = {"CARVING? YES. YES, I AM. COME IN, ALL OF YOU. HOT AND FIRST!", "SERVING? YES. YES, I AM. COME IN, ALL OF YOU. HOT AND FIRST!",
                               "BREAKING? YES. YES, I AM. COME IN, ALL OF YOU. WARM AND FIRST!"};
  P["guestline"] = guest[prov]; P["decoy"] = decoy[prov]; P["herdline"] = herd[prov];
  // the gifts
  const bool wed = v == V_WEDDING;
  P["prize"] = wed ? "THE DOWRY" : "THE LAUNCH PURSE";
  P["giftsline"] = wed ? "THE DOWRY IS TWO CHESTS OF SILVER AND A FEATHER BED. THE OUTLAWS KNOW IT. THE CHESTS ARE WHAT THEY WILL TAKE, AND THEY ARE COMING. WHERE DO THEY GO?"
                       : "THE LAUNCH PURSE IS ONE STRONGBOX, THE WAGES FOR THE FIRST VOYAGE. THE OUTLAWS KNOW THE DATE. THEY WILL TAKE THE BOX. WHERE DOES IT GO?";
  P["hideopen"] = wed ? "UNDER THE HEAD TABLE." : "UNDER THE LAUNCH TABLE.";
  P["hidestair"] = wed ? "IN THE CELLAR. I'LL HOLD THE STAIR." : "IN THE NET LOFT. I'LL HOLD THE LADDER.";
  P["hidedecoy"] = wed ? "A DECOY CHEST ON THE CART." : "A DECOY BOX ON THE HAND-CART.";
  P["stair"] = wed ? "THE CELLAR STAIR" : "THE NET LOFT LADDER";
  // the feud, by the event
  static const char* feudL[] = {"YOUR GRANDFATHER BURNED OUR BARN. ...YOUR GRANDFATHER BROKE THE OATH. ...I'LL SETTLE IT HERE AND NOW.",
                                "YOUR PIGS ATE MY BARLEY. EVERY ROW. ...YOUR FENCE WAS A SUGGESTION. ...SAY THAT AGAIN, WITH YOUR FACE UP.",
                                "YOUR CREW CUT OUR NETS LAST SPRING. ...YOUR CREW TOOK OUR BERTH FIRST. ...OUTSIDE, WHERE THE TIDE CAN WATCH.",
                                "YOUR KING'S SHIPS SANK OUR GRAIN-BOAT. ...YOUR KING SOLD US ROTTEN GRAIN. ...THEN LET US WRITE THE TREATY OUTSIDE.",
                                "YOU LEFT MY BROTHER ON THE ICE. ...I WENT FOR HELP, AND CAME TOO LATE. ...THEN SAY IT AGAIN, OUTSIDE."};
  static const char* peaceL[] = {"...FINE. A CUP, THEN. ONE. TO THE BARN THAT BURNED. MAY IT BE THE LAST THING THAT EVER DOES.",
                                 "...FINE. ONE JUG, AND YOU REPAY THE BARLEY. IN PIGS. DONE. TO THE LAST EAR OF CORN.",
                                 "...A ROUND, THEN. THE NETS ARE MENDED BY MORNING, BY BOTH CREWS. TO THE BOAT, AND TO ITS WAKE.",
                                 "...THE TREATY IS FOR THE KINGS. WE ARE ONLY THE ONES WHO STAND IN THE COLD. A CUP, THEN. TO THE DOOR.",
                                 "...A CUP. TO THE ONE ON THE ICE. AND TO THE ONE WHO RAN FOR HELP. THE NIGHT IS LONG ENOUGH FOR BOTH."};
  P["feudline"] = feudL[v]; P["peaceline"] = peaceL[v];
  // the cup: one culprit, every clue consistent with them
  const int culprit = (int)(d.next() % 3u);   // 0 cousin, 1 cook's rival, 2 stranger
  const int cul = c.danger ? 2 : culprit;
  static const char* servantL[] = {"A HAND OVER THE CUP OF {SPOUSE}. A GREEN CUFF, SLEEVE PUSHED BACK. I LOOKED AWAY. I SHOULD NOT HAVE.",
                                   "A HAND OVER THE CUP OF {SPOUSE}, WHITE TO THE WRIST. FLOUR, I THOUGHT. THE KITCHEN IS FULL OF IT TONIGHT.",
                                   "A HAND OVER THE CUP OF {SPOUSE}. A GLOVE, MUD ON THE CUFF. NOT ONE OF OURS. I MEANT TO SAY SOMETHING. I DID NOT."};
  static const char* dishL[] = {"FOUND THIS UNDER THE CORNER BENCH BY THE HEARTH, WHERE {COUSIN} SITS. SMELLS SWEET AND BITTER. BAD FOR THE BELLY, NOT THE LIFE.",
                                "FOUND THIS ON THE KITCHEN SHELF, BEHIND THE SALT, WHERE {RIVAL} WORKS. SWEET AND BITTER. BAD FOR THE BELLY, NOT THE LIFE.",
                                "FOUND THIS IN THE CLOAK RACK, IN A MUDDY POCKET, BY {STRANGER}'S SEAT. SWEET AND BITTER. BAD FOR THE BELLY, NOT THE LIFE."};
  static const char* confL[] = {"{SPOUSE} HAS EVERYTHING. I WANTED ONE BAD HOUR FOR {SPOUSE.HIM}. THAT IS ALL. I NEVER MEANT TO HURT ANYONE.",
                                "MY BREAD WAS BETTER. THEY KNEW IT. I WANTED THEM TO CHOKE ON THEIR OWN CAKE. ...ON THEIR OWN BREAD. I AM SORRY.",
                                "I WAS PAID BY A HOUSE I WILL NOT NAME, TO MAKE THE FAMILY LOOK CARELESS. I DID IT."};
  static const char* roleOf[] = {"cousin", "rival", "stranger"};
  P["servantline"] = servantL[cul]; P["dishline"] = dishL[cul]; P["confession"] = confL[cul]; P["culprit"] = roleOf[cul];
  P["confessopt"] = cul == 2 && c.danger ? "[[STOP!|DON'T MOVE.]]" : "[[OUT, QUIETLY, BY MORNING.|GO HOME. SAY NOTHING.]]";
  P["afterconfess"] = cul == 2 && c.danger ? "culprit_runs" : "dawn";
  for (int k = 0; k < 3; k++) {
    static const char* key[] = {"accC", "accR", "accS"};
    P[key[k]] = k == cul ? "confess" : "wrong1";
    P[std::string(key[k]) + "2"] = k == cul ? "confess" : "cup_lost";
  }
  // the dawn: the toast's extra and the giver's words, by the event and the night
  P["toastextra"] = tw == T_GIFTS ? (wed ? "TURNIPS, IF THEY TOOK THE DECOY! A CHEST OF STOLEN TURNIPS!" : "BALLAST, IF THEY TOOK THE DECOY! A BOX OF STOLEN BALLAST!")
                                  : "MAY THE CUPS NEVER RUN DRY.";
  static const char* quietL[] = {"NOT A BROKEN CUP. NOT A BROKEN HEAD. WE DANCED TILL THE SUN CAME UP. THE COUPLE ASKED FOR YOU BY NAME.",
                                 "THE LAST SHEAF IS IN AND THE LAST JUG IS EMPTY. NOT A SINGLE SCRATCH. THE BARN WILL REMEMBER YOU A YEAR.",
                                 "THE BOAT SLID IN LIKE BUTTER, AND THE QUAY SANG TILL DAWN. NOT A KNIFE OUT. YOU HAVE A BERTH ON ITS FIRST RUN.",
                                 "THE ENVOYS LEFT AT DAWN, FULL OF MY WINE AND NOT ONE INSULT. THAT IS A TREATY, OF A SORT. THANK YOU.",
                                 "THE LONGEST NIGHT IS OVER, AND NOBODY BLEEDS. THE SUN IS UP ON A QUIET HOUSE. YOU ARE A WONDER."};
  static const char* shakenL[] = {"THE COUPLE SAW NOTHING. I TOLD THE WHOLE TABLE THE NOISE WAS DRUMS. LET THAT STAND. THE DANCING NEVER STOPPED.",
                                  "THE BARN IS WHOLE AND THE SHEAVES ARE SAFE. THE CHILDREN THOUGHT IT WAS A GAME. LET THEM THINK SO.",
                                  "THE QUAY WILL TELL THAT STORY FOR YEARS. THE BOAT IS UNSCRATCHED, AND SO ARE THE GUESTS, MOSTLY. THANK YOU.",
                                  "THE ENVOYS WILL NOT MENTION THE LITTLE SCUFFLE, AND NEITHER WILL I. THE TREATY STANDS. A GOOD NIGHT, STRANGER.",
                                  "THE HALL IS STILL WARM AND THE KNIVES ARE STILL SHEATHED. THAT WAS CLOSE. I'LL FILL YOUR CUP TILL SPRING."};
  P["quiet"] = tw == T_CUP ? "SO THAT IS WHO IT WAS. I'LL SEE THEM OUT IN THE MORNING, WITH NO NOISE. THE COUPLE WILL NEVER KNOW HOW CLOSE IT CAME."
               : tw == T_UNCLE ? "THE UNCLE DANCED WITH {SPOUSE}, IF YOU CAN BELIEVE IT. NOBODY WILL SAY HOW. I WILL SAY: THANK YOU." : quietL[v];
  P["shaken"] = shakenL[v];
  std::vector<std::string> T = {"KEEP THE DOOR", "GUARD THE TOAST", "A NIGHT OF FEASTING"};
  switch (v) {
    case V_WEDDING: T.push_back("WATCH THE WEDDING"); T.push_back("DANCING TILL DAWN");
                    if (tw == T_CUP) T.push_back("A CUP TOO MANY");
                    if (tw == T_UNCLE) T.push_back("MIND THE UNCLE");
                    if (tw == T_GIFTS) T.push_back("WATCH THE DOWRY");
                    break;
    case V_HARVEST: T.push_back("THE HARVEST FEAST"); T.push_back("KEEP THE BARN"); break;
    case V_LAUNCH: T.push_back("THE LAUNCH FEAST"); T.push_back("A NIGHT ON THE QUAY"); if (tw == T_GIFTS) T.push_back("THE LAUNCH PURSE"); break;
    case V_ENVOYS: T.push_back("A TABLE OF RIVALS"); T.push_back("THE ENVOYS' SUPPER"); break;
    default: T.push_back("THE LONG NIGHT FEAST"); T.push_back("THE LONGEST NIGHT"); break;
  }
  if (tw == T_ROAST) T.push_back("THE SMELL OF SUPPER");
  if (tw == T_FEUD) T.push_back("OLD GRUDGES AT TABLE");
  P["title"] = T[(size_t)d.below((int)T.size())];
}

bool choose(const JobCtx& w, uint64_t dice, JobCtx& out) {
  Dice d(dice);
  out = w;
  std::vector<int> evs;
  switch ((Role)w.trade) {
    case Role::Innkeeper: evs = {V_WEDDING}; if (w.season == 3) evs.push_back(V_LONGNIGHT); break;
    case Role::Merchant: case Role::Priest: evs = {V_WEDDING}; break;
    case Role::Jarl: evs = {V_WEDDING}; if (w.tier >= 1) evs.push_back(V_ENVOYS); if (w.season == 3) evs.push_back(V_LONGNIGHT); break;
    case Role::Farmer: evs = {V_HARVEST}; break;
    case Role::Fisher: if (!w.port) return false; evs = {V_LAUNCH}; break;
    default: return false;
  }
  out.variant = evs[(size_t)d.below((int)evs.size())];
  // the threat: beasts near (one time in three), bandits for a dowry or a launch purse, the feud, the cup (weddings),
  // the uncle (quiet weddings); the danger flag forces the cup with the stranger at a wedding
  const bool wed = out.variant == V_WEDDING;
  std::vector<int> th = {T_ROAST, T_FEUD};
  if (wed || out.variant == V_LAUNCH) th.push_back(T_GIFTS);
  if (wed) { th.push_back(T_CUP); th.push_back(T_UNCLE); }
  out.twist = th[(size_t)d.below((int)th.size())];
  if (wed && w.danger) out.twist = T_CUP;
  // the provision, by the season and the place: meat in autumn and winter, fish at a port, else bread
  out.extra = w.port ? P_FISH : (w.season >= 2 ? P_SPIT : P_BREAD);
  return true;
}

JobCtx walk(Role t, int ev, int threat, int prov, int season, int tier = 1, bool port = false, bool danger = false) {
  JobCtx c;
  c.trade = (int)t; c.variant = ev; c.twist = threat; c.extra = prov; c.season = season; c.tier = tier; c.port = port; c.danger = danger;
  return c;
}

}  // namespace

const KindDef& kindFeast() {
  static const KindDef K = [] {
    KindDef k;
    k.id = "feast";
    k.title = "FEAST WATCH";
    k.lead = "NEEDS A GUARD FOR A FEAST TONIGHT";
    k.body = kBody;
    k.variants = {"wedding", "harvest", "launch", "envoys", "longnight"};
    k.twists = {"roast", "gifts", "feud", "cup", "uncle"};
    k.extras = {"spit", "fish", "bread"};
    k.trades = {(int)Role::Innkeeper, (int)Role::Merchant, (int)Role::Priest, (int)Role::Jarl, (int)Role::Farmer, (int)Role::Fisher};
    k.choose = choose;
    k.prep = prep;
    k.walks = {walk(Role::Innkeeper, V_WEDDING, T_CUP, P_BREAD, 1), walk(Role::Merchant, V_WEDDING, T_GIFTS, P_SPIT, 2, 2),
               walk(Role::Farmer, V_HARVEST, T_ROAST, P_SPIT, 2, 0), walk(Role::Fisher, V_LAUNCH, T_FEUD, P_FISH, 1, 1, true),
               walk(Role::Jarl, V_ENVOYS, T_ROAST, P_SPIT, 3, 2), walk(Role::Priest, V_WEDDING, T_UNCLE, P_BREAD, 0),
               walk(Role::Jarl, V_LONGNIGHT, T_FEUD, P_SPIT, 3, 1), walk(Role::Innkeeper, V_WEDDING, T_CUP, P_SPIT, 3, 1, false, true)};
    return k;
  }();
  return K;
}

}  // namespace radiant
}  // namespace story
