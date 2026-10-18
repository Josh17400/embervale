// P1b radiant job kind 12: BOUNDS DISPUTE (content: jobs_c.md section 2; judge notes resolved). JOBS lane.
//
// Two neighbours swear the same ground is theirs. Hear both, find the one old witness (or the old deed in a fallen
// place), read the ground itself, and set the stone where the evidence says the line ran. What is true is rolled once
// (the giver's line, the neighbour's, or common ground: no line at all) and the witness, the deed and the ground always
// agree with it, so a careful player can always work it out. The giver offers a PURSE for a ruling their way, but never
// asks for a lie. Six grounds: a farmer's hedge and ditch (in the village), a lord's oaks between two settlements, a
// priest's churchyard strip, a fisher's reach of river, a city merchant's stalls in the square, a hunter's stag-run.
// ONE MONEY MODEL (judge): the fee is the band's fair pay. Ruled for a giver who was right: the fee and the purse
// (half again). Ruled for the neighbour who was right: the giver pays the honest part of the fee (four tenths), the
// neighbour the rest (six tenths). Common ground, truly: each side pays three quarters. Bought (ruled for the giver
// against the evidence): the fee and the purse, warmly, and the cheated side calls you bought where it counts. Wronged
// (ruled against the evidence otherwise): the giver pays nothing; a favoured neighbour presses four tenths on you.
// The ruling is made at the giver (the spot stone's art is optional: the fallback of the spec). The three-day notice is
// replaced by what the cheated side says and does at once (a rumour, their memory, the town's record).
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
{?v_wood|v_reach|v_range
role othertown site [[village|town]] near
role other person [[male|female]] at othertown
}
{!v_wood|v_reach|v_range
role other person [[male|female]] at home
}
{?tw_witness
{?v_wood|v_reach|v_range
role witness person [[male|female]] at othertown
}
{!v_wood|v_reach|v_range
role witness person [[male|female]] at home
}
}
{?tw_charter
role deedruin site ruin near
}

stage start
  talk giver
  say "[[=reason]] [[I NEED SOMEONE WHO OWES NEITHER OF US A THING.|FIND THE TRUE LINE, AND SET THE STONE.|HEAR US BOTH, THEN READ THE GROUND.]]"
  opt "[[I'LL HEAR BOTH SIDES.|I'LL FIND THE TRUE LINE.|I'LL WALK THE BOUNDS.]]" -> claimtalk
  opt "[[NOT MY QUARREL.|NOT TODAY.|SETTLE IT YOURSELVES.]]" -> decline

stage claimtalk
  talk giver
  say "[[=claim]]"
  journal "[[=desc]] HEAR {GIVER}'S CLAIM."
  opt "[[I'LL HEAR {OTHER} TOO.|AND THE OTHER SIDE?|I'LL RULE TRUE.]]" -> [[=first]]

{?v_wood|v_reach|v_range
stage go_other
  goal goto othertown
  journal "[[=desc]] FIRST, HEAR {OTHER} AT {OTHERTOWN}, {OTHERTOWN.DIR}."
  then other
}

stage other
  talk other
  say "[[=otherline]]"
  journal "[[=desc]] HEAR {OTHER}'S SIDE."
  opt "[[I'LL ASK.|THANK YOU.|I'LL SEE FOR MYSELF.]]" -> [[=afterother]]

{?tw_witness
stage witness
  talk witness
  say "[[=testimony]]"
  journal "[[=desc]] {WITNESS} WALKED THE BOUNDS LONG AGO."
  opt "[[THANK YOU.|THAT HELPS.|I'LL LOOK AT THE GROUND.]]" -> spot
}

{?tw_charter
stage deed
  goal fetch "THE OLD CHARTER" in deedruin
  journal "THE OLD CHARTER LIES IN [[=deedplace]] AT {DEEDRUIN}, {DEEDRUIN.DIR}. FETCH IT."
  then deedread

stage deedread
  goal goto [[=spotrole]]
  journal "THE CHARTER IS FOUND. NOW READ THE GROUND ITSELF, AND RULE."
  do notice "[[=deeda]]"
  do notice "[[=deedb]]"
  then report
}

{?tw_witness
stage spot
  goal goto [[=spotrole]]
  journal "READ THE GROUND AT THE DISPUTED LINE ITSELF, THEN RULE BEFORE {GIVER}."
  then report
}

stage report
  talk giver
  say "WELL? YOU HEARD THEM BOTH, AND YOU READ THE GROUND. [[=signsay]] WHERE DOES THE STONE GO?"
  journal "RULE THE LINE BEFORE {GIVER} IN {HOME}. THE GROUND: [[=signshort]]"
  do notice "[[=signa]]"
  opt "[[=labela]]" -> [[=rulea]]
  opt "[[=labelb]]" -> [[=ruleb]]
  opt "[[=labelc]]" -> [[=rulec]]

{?x_giver
stage right_end
  end success
  say "[[=giverright]]"
  journal "YOU SET THE STONE WHERE {GIVER}'S LINE RAN, AS THE EVIDENCE SAID. {GIVER} PAID THE FEE AND THE PURSE."
  do reward fair [[=rightpay]]
  do remember giver "YOU SET MY STONE TRUE. THE PURSE WAS EARNED HONESTLY."
  do remember other "THE STONE WENT {GIVER}'S WAY. THE GROUND SAID SO. I CAN'T ARGUE WITH THE GROUND."
  do census home mood 2
  do census home stock produce 1
}

{!x_giver
stage bought_end
  end success
  say "[[=boughtline]]"
  journal "YOU RULED FOR {GIVER} AGAINST THE EVIDENCE. {GIVER} PAID, WARMLY. {OTHER}'S KIN CALL YOU BOUGHT, AND THE NAME TRAVELS."
  do reward fair [[=rightpay]]
  do rumour
  do remember other "YOU WERE BOUGHT. THE GROUND SAID ONE THING AND YOU SAID ANOTHER. I KNOW YOUR FACE."
  do fact "{OTHER}'S KIN SAY THE STONE ON THE {HOME} LINE WAS SET FOR A PURSE."
  do census home mood -2
  do census home stock produce 1
}

{?x_neighbour
stage nb_giver
  talk giver
  say "[[=nbgiver]]"
  journal "YOU SET THE STONE WHERE {OTHER}'S LINE RAN. {GIVER} PAID THE HONEST PART OF THE FEE. {OTHER} OWES THE REST."
  do reward fair 40
  do remember giver "THE STONE WENT THEIR WAY. FAIR. I MOVED IT WITHOUT KNOWING."
  opt "[[I'LL SEE {OTHER} FOR THE REST.|FAIR'S FAIR.]]" -> nb_collect

stage nb_collect
  talk other
  say "[[=nbother]]"
  journal "COLLECT THE REST OF THE FEE FROM {OTHER}."
  opt "[[THANK YOU.|THE GROUND SAID SO.]]" -> nb_end

stage nb_end
  end success
  journal "THE STONE STANDS WHERE {OTHER}'S LINE RAN, AS THE EVIDENCE SAID. BOTH SIDES PAID THEIR PART OF THE FEE."
  do reward fair 60
  do remember other "THE STONE WENT MY WAY BECAUSE IT WAS TRUE. THANK YOU FOR THAT."
}

{?x_common
stage common_giver
  talk giver
  say "[[=comgiver]]"
  journal "YOU DECLARED THE GROUND COMMON, AS THE EVIDENCE SAID. EACH SIDE PAYS THREE QUARTERS OF THE FEE."
  do reward fair 75
  do remember giver "COMMON GROUND. ALL THAT SHOUTING FOR A STRIP NOBODY OWNED."
  opt "[[I'LL SEE {OTHER} FOR THEIR PART.|EACH PAYS THEIR SHARE.]]" -> com_collect

stage com_collect
  talk other
  say "[[=comother]]"
  journal "COLLECT {OTHER}'S THREE QUARTERS OF THE FEE."
  opt "[[THANK YOU.|IT'S FAIR.]]" -> com_end

stage com_end
  end success
  journal "THE DISPUTED GROUND IS COMMON, AS IT ALWAYS WAS. BOTH SIDES PAID THEIR SHARE."
  do reward fair 75
  do xp 40
  do remember other "COMMON GROUND. I'LL WAVE AT {GIVER} ACROSS IT NOW."
}

{!x_neighbour
stage wronged_b
  talk giver
  say "[[=wronggiver]]"
  journal "YOU SET THE STONE WHERE {OTHER}'S LINE RAN, AGAINST THE EVIDENCE. {GIVER} PAYS NOTHING."
  do remember giver "YOU RULED AGAINST THE GROUND ITSELF. I WILL SETTLE IT MY OWN WAY."
  opt "[[SO BE IT.|I RULED AS I SAW IT.]]" -> thanks_b

stage thanks_b
  talk other
  say "FAIR OR NOT, I'LL TAKE IT. THE QUARREL IS OVER. HERE: FOUR TENTHS OF YOUR FEE, FOR THE STONE."
  journal "{OTHER} PRESSES FOUR TENTHS OF THE FEE ON YOU."
  opt "[[...THANK YOU.|IT'S DONE.]]" -> wronged_end
}

{!x_common
stage wronged_c
  talk giver
  say "[[=wronggiver]] COMMON? ALL THAT FOR A GATE?"
  journal "YOU DECLARED THE GROUND COMMON AGAINST THE EVIDENCE. {GIVER} PAYS NOTHING."
  do remember giver "COMMON, YOU SAID. THE GROUND SAID OTHERWISE. NO FEE FOR THAT."
  opt "[[SO BE IT.|I RULED AS I SAW IT.]]" -> thanks_c

stage thanks_c
  talk other
  say "COMMON, THEN. FAIR OR NOT, I'LL TAKE IT. HERE: FOUR TENTHS OF YOUR FEE."
  journal "{OTHER} PRESSES FOUR TENTHS OF THE FEE ON YOU."
  opt "[[...THANK YOU.|IT'S DONE.]]" -> wronged_end
}

stage wronged_end
  end success
  journal "THE STONE STANDS WHERE THE EVIDENCE SAYS IT SHOULD NOT. {GIVER} PAID NOTHING; {OTHER} PAID A LITTLE."
  do reward fair 40
  do fact "A STONE ON THE {HOME} LINE WAS SET AGAINST THE OLD WITNESS AND THE GROUND."

stage decline
  end fail
  journal "YOU TURNED DOWN {GIVER}'S BOUNDARY QUARREL."
)DSL";

enum { V_FIELD, V_WOOD, V_GRAVE, V_REACH, V_STALLS, V_RANGE };
enum { T_WITNESS, T_CHARTER };
enum { X_GIVER, X_NEIGHBOUR, X_COMMON };

struct Ground {
  const char* claimA; const char* claimB;
  const char* labels[3];
  const char* testimony[3];   // by truth
  const char* sign[3];        // by truth (one line for the notice)
  const char* other;          // the other party: claim B and the witness named
  const char* otherCharter;   // ... and where the deed lies
  const char* deedplace;
  const char* giverClaim;     // at the accept: claim A and the purse
  const char* right;          // ruled A, truly
  const char* nbGiver; const char* nbOther;
  const char* comGiver; const char* comOther;
};
const Ground kGround[] = {
    // FIELD
    {"THE HAWTHORN HEDGE", "THE DRAINAGE DITCH", {"SET IT BY THE HEDGE.", "SET IT BY THE DITCH.", "LEAVE THE BALK COMMON."},
     {"WALK THE BOUNDS? BEHIND MY FATHER, WITH A SLAP AT EACH STONE SO I'D REMEMBER. THE LAST SLAP WAS AT THE HAWTHORN HEDGE. IT STANDS IN MY EAR.",
      "A SLAP AT EACH STONE, SO I'D REMEMBER. THE HARDEST WAS THE LAST, AT THE DRAINAGE DITCH, WHERE I FELL IN. THE STONE STOOD AT THE DITCH.",
      "A STONE? THERE WAS NEVER A STONE. THAT STRIP WAS THE TURNING-BALK. BOTH TEAMS TURNED THERE, AND NEITHER OWNED IT. MY FATHER SAID: LEAVE IT BE."},
     {"A SQUARE BED UNDER THE HEDGE, MOSSED.", "A SQUARE BED BY THE DITCH, BAKED HARD.", "NO STONE-BED. TWO TRACKS TURN HERE."},
     "SENT BY NEXT DOOR, ARE YOU? THE STONE STOOD BY THE DITCH. MY GRANDMOTHER DUG IT, ALONE, IN A WEEK. ASK THE OLD ONE AT THE WELL, {WITNESS}. THEY WALKED THE BOUNDS AND NEVER LIE.",
     "NEXT DOOR SENT YOU? THE OLD ONE AT THE WELL IS DEAD, REST THEM. THE REEVE'S ROLL SURVIVED THE FIRE. IT IS IN THE OLD CHAPEL AT {DEEDRUIN}. READ IT.",
     "THE OLD CHAPEL",
     "THE STONE STOOD BY THE HAWTHORN HEDGE. MY FATHER'S FATHER SAID SO. {OTHER} SAYS THE DITCH. RULE MY WAY AND THERE IS A PURSE ON TOP. RULE FAIR, AND I'LL STILL FEED YOU.",
     "SO THE HEDGE WAS THE LINE. THE BARLEY IS MINE, AND THE PEARS. THE FEE, AND HERE IS THE PURSE I PROMISED. TAKE A BASKET.",
     "THE DITCH? I MOVED IT WITHOUT KNOWING. MY FATHER'S FATHER ERRED, OR LIED. HERE IS MY PART OF THE FEE, FOUR TENTHS. {OTHER} OWES THE REST.",
     "THE DITCH! I KNEW IT, AND SO DID MY GRANDMOTHER. THE FALLEN PEARS ARE MINE TOO. HERE IS THE REST OF YOUR FEE, SIX TENTHS.",
     "A TURNING-BALK. ALL THIS TIME? THEN WHY HAVE WE BEEN SHOUTING? FALLEN PEARS FOR WHOEVER BENDS. HERE IS MY THREE QUARTERS OF THE FEE.",
     "THE BALK, COMMON? FINE. I'LL TURN MY TEAM THERE AND WAVE AT NEXT DOOR. HERE IS MY THREE QUARTERS OF THE FEE."},
    // WOOD
    {"THE SPLIT OAK", "THE BROOK", {"SET IT AT THE SPLIT OAK.", "SET IT AT THE BROOK.", "THE WOOD STAYS COMMON."},
     {"I CARRIED THE BASKET WHEN THEY WALKED THE WOOD. THEY SPLIT AN OAK WITH A WEDGE, AND SET THE STONE AT ITS FOOT. THE SPLIT OAK, YES. I SAW IT.",
      "THE BROOK WAS THE LINE. THEY SAID IT OVER AND OVER, BROOK, BROOK, BROOK, BECAUSE A CHILD FELL IN. THAT CHILD WAS ME. THE STONE STOOD AT THE BROOK.",
      "THE OAKS? COMMON, ALL OF THEM. THE OLD KINGS SAID DEAD WOOD FOR BOTH, LIVE OAK FOR NEITHER. THERE WAS NEVER A STONE IN THAT WOOD. NOT ONE."},
     {"A STONE-BED AT THE SPLIT OAK'S FOOT.", "A STONE-BED CUT INTO THE BROOK BANK.", "OAK AFTER OAK, AND NOT ONE STONE."},
     "I HAVE NOT SLEPT SINCE THE AXES CAME. THE STONE STOOD BY THE BROOK. GO TO THE OLD ONE AT THE REEVE'S HOUSE, {WITNESS}. THEY CARRIED THE BASKET WHEN IT WAS WALKED.",
     "I HAVE NOT SLEPT SINCE THE AXES CAME. THE DEED CHEST WENT TO THE ABBEY WHEN THE REEVE'S HOUSE BURNED, AND THE ABBEY FELL. IT IS IN THE CELLAR AT {DEEDRUIN}.",
     "THE FALLEN ABBEY'S CELLAR",
     "THE STONE STOOD AT THE SPLIT OAK, SO THE OAKS ARE ON MY SIDE. {OTHER} SAYS THE BROOK. THERE IS A PURSE FOR A RULING MY WAY. BUT RULE TRUE. I HAVE ENOUGH ENEMIES.",
     "THE SPLIT OAK. MY OAKS, THEN. THE FEE, AND HERE IS THE PURSE. I'LL KEEP THE AXES SHARP AND LEGAL. THANK YOU FOR THE TRUTH, EVEN SO.",
     "THE BROOK. SO THE OAKS ARE THEIRS. I HAD SWORN THEY WERE MINE. I WILL SAY SO IN THE HALL. HERE IS MY HONEST PART OF THE FEE, FOUR TENTHS.",
     "THE BROOK. NOW I CAN SLEEP. THE AXES COME DOWN TONIGHT. HERE IS THE REST OF YOUR FEE, SIX TENTHS, AND MY THANKS.",
     "COMMON WOOD. THE OLD KINGS KNEW BETTER THAN WE DID. WE'LL SHARE THE DEAD WOOD. HERE IS MY THREE QUARTERS OF THE FEE.",
     "COMMON WOOD. DEAD WOOD FOR BOTH. I'LL TELL THE REEVE. HERE IS MY THREE QUARTERS OF THE FEE."},
    // GRAVE (no purse: the temple pays the fee only)
    {"THE WALL'S OLD CORNER", "THE THREE ELMS", {"SET IT AT THE OLD CORNER.", "SET IT AT THE ELMS.", "THE STRIP IS THE DEAD'S."},
     {"I DUG HALF THESE GRAVES. THE WALL'S OLD CORNER WAS THE LINE, AND THE STONE STOOD IN IT. THE WALL WAS PULLED BACK WHEN I WAS YOUNG. NOT THE LINE.",
      "THE THREE ELMS WERE THE LINE. I PLANTED ONE OF THEM, AT THE STONE'S FOOT. THE GRAVE IS ON THE FIELD SIDE. IT WAS A POOR ONE, BURIED BY CHARITY.",
      "A LINE? THE STRIP WAS GIVEN TO THE DEAD, WHEN THE FIRST ABBOT DIED. IT IS NEITHER'S. THE DEED LIES IN THE ABBEY VAULT, BUT I KNOW IT BY HEART."},
     {"A CORNER-STONE, PAST THE PRESENT WALL.", "THREE ELMS, A STONE-BED AT THE OLDEST.", "A SMALL WORN STONE: HOLY GROUND."},
     "I WANT NO QUARREL WITH THE DEAD. BUT MY BARLEY RUNS TO THE THREE ELMS, AND THE GRAVE IS ON MY SIDE OF THEM. ASK THE OLD SEXTON AT THE LYCHGATE, {WITNESS}.",
     "I WANT NO QUARREL WITH THE DEAD. THE OLD SEXTON IS BURIED NOW. THE DEED LIES IN THE ABBEY VAULT AT {DEEDRUIN}, WHERE THE FIRST ABBOT LIES.",
     "THE ABBEY VAULT",
     "THE LINE RAN TO THE WALL'S OLD CORNER. {OTHER} SAYS THE THREE ELMS. THE GRAVE LIES BETWEEN THEM. I OFFER NO PURSE. THE TEMPLE PAYS THE FEE, AND I ASK ONLY THAT YOU RULE TRUE.",
     "THE CORNER IS OURS. THE DEAD NEED NO MOVING, THEN. THE TEMPLE'S FEE IS YOURS, WITH THANKS AND A BLESSING.",
     "THE ELMS. THEN THE GRAVE LIES IN THE FIELD, AND I WAS WRONG. {OTHER} HAS PROMISED A SQUARE ROUND IT UNPLOUGHED. HERE IS THE TEMPLE'S PART OF THE FEE, FOUR TENTHS.",
     "THE ELMS, THEN. I'LL LEAVE A SQUARE ROUND THE GRAVE, UNPLOUGHED. I NEVER WANTED IT MOVED, ONLY MY LINE KNOWN. HERE IS THE REST OF YOUR FEE.",
     "THE STRIP WAS THE DEAD'S ALL ALONG. NEITHER OF US MAY PLOUGH IT. GOOD. HERE IS THE TEMPLE'S THREE QUARTERS OF THE FEE.",
     "THE DEAD'S STRIP. THEN THERE IS NO QUARREL LEFT. I'LL PLOUGH ROUND IT. HERE IS MY THREE QUARTERS OF THE FEE."},
    // REACH
    {"THE OLD WILLOW", "THE NEW CHANNEL", {"THE LINE IS THE WILLOW.", "THE LINE FOLLOWS THE RIVER.", "NET ON ALTERNATE DAYS."},
     {"THE LINE WAS THE OLD WILLOW, AND THEY DROVE AN IRON PEG IN ITS ROOT SO THE RIVER COULD NOT CHANGE ITS MIND. THE PEG IS THERE, IF YOU WANT TO LOOK.",
      "WHEN THE RIVER MOVED IN MY FATHER'S TIME, THEY MOVED THE STONE WITH IT, TO THE NEW MIDDLE. THE LINE FOLLOWS THE WATER. THAT'S THE OLD CUSTOM HERE.",
      "A LINE? NOT FOR NETS. ODD DAYS {HOME}, EVEN DAYS {OTHERTOWN}. THE TALLY IS NOTCHED ON THE LANDING STONE. THE ARRANGEMENT IS OLDER THAN ME."},
     {"AN IRON PEG IN THE OLD WILLOW'S ROOT.", "STONE-BEDS STEPPING DOWN TO THE WATER.", "A LANDING-STONE NOTCHED ODD AND EVEN."},
     "THE WATER MOVED. THE LAW DID NOT. I FISH WHERE THE FISH ARE, NOT WHERE A DEAD MAN DREW A LINE. THE RIVER IS THE LINE. ASK THE OLD ONE AT THE LANDING, {WITNESS}.",
     "THE WATER MOVED. THE LAW DID NOT. THE LANDING CHARTER WENT TO THE MILL WHEN THE RIVER ROSE. THE MILL IS EMPTY NOW: {DEEDRUIN}. THE CHEST IS STILL IN IT.",
     "THE EMPTY MILL",
     "THE LINE IS THE OLD WILLOW. IT ALWAYS WAS. THE RIVER CAN MOVE, BUT A LINE IS A LINE. {OTHER} SAYS THE RIVER. GET ME MY WILLOW AND THERE IS A PURSE ON TOP.",
     "THE WILLOW! I TOLD THEM. A PEG IS A PEG. THE FEE, AND HERE IS THE PURSE, AND I'LL GUT YOUR FISH FOR A WEEK.",
     "THE LINE FOLLOWS THE WATER NOW. A RIVER IS A BAD NEIGHBOUR, BUT FAIR. HERE IS MY HONEST PART OF THE FEE, FOUR TENTHS.",
     "THE WATER IS THE LINE. I'LL KEEP MY NETS WHERE THE FISH ARE. HERE IS THE REST OF YOUR FEE, SIX TENTHS.",
     "ALTERNATE DAYS? THEN WE QUARRELLED FOR NOTHING. ODD DAYS ARE MINE, THEN. HERE IS MY THREE QUARTERS OF THE FEE.",
     "ODD DAYS TO THEM, EVEN TO ME. FINE. I'LL MEND THE TALLY STONE MYSELF. HERE IS MY THREE QUARTERS OF THE FEE."},
    // STALLS (judge: the pump was set beside the old trough; both stand today)
    {"THE PUMP", "THE TROUGH", {"THE LINE IS THE PUMP.", "THE LINE IS THE TROUGH.", "THE STALLS TAKE TURNS."},
     {"THE PUMP, DEAR. I PAINTED THE LINE MYSELF, FIFTY YEARS AGO, FROM THE PUMP HANDLE OUT. I KNOW IT BY MY KNEES.",
      "THE TROUGH, DEAR. THE PUMP CAME LATER, SET BESIDE THE OLD TROUGH. THE LINE RAN RIGHT THROUGH THE TROUGH, AND STILL DOES.",
      "A LINE? THE STALLS SWAP SIDES EACH MARKET MONTH, EAST TO WEST. THE CHARTER SAYS SO. NOBODY READS THE CHARTER. I DO. NO LINE, DEAR."},
     {"WHITE PAINT UNDER THE PUMP'S FOOT.", "WHITE PAINT UNDER THE TROUGH'S RIM.", "TWO OLD STRIPES, ONE OVER THE OTHER."},
     "AH, THE ARBITER. DO TRY THE PEARS. THE LINE RAN THROUGH THE TROUGH, OF COURSE. ASK THE OLDEST CLERK, {WITNESS}, ON THE GUILDHALL STEPS. THEY CAN'T BE BOUGHT.",
     "AH, THE ARBITER. DO TRY THE PEARS. THE OLDEST CLERK DIED IN WINTER. THE GUILD ROLL WAS SAVED FROM THE FIRE AND LEFT IN THE OLD HALL'S CELLAR, AT {DEEDRUIN}.",
     "THE OLD GUILDHALL'S CELLAR",
     "THE LINE RAN FROM THE PUMP. ANY CLERK KNOWS. {OTHER} CLAIMS THE TROUGH. SEE IT MY WAY AND THERE IS A PURSE ON TOP OF THE FEE. SEE IT TRUE, AND THE FEE STANDS.",
     "THE PUMP. IT WAS THE PUMP ALL ALONG. THE FEE, AND HERE IS THE PURSE, AND A SPOT BY THE FOUNTAIN. I HAVE A STALL TO PUSH OUT.",
     "THE TROUGH? THEN I OWE {OTHER} A PACE OF COBBLES AND AN APOLOGY. HERE IS MY HONEST PART OF THE FEE, FOUR TENTHS. DON'T TELL THE GUILD.",
     "THE TROUGH. I'LL REPAINT THE LINE TOMORROW. HERE IS THE REST OF YOUR FEE, SIX TENTHS. TRY THE PEARS.",
     "SWAPPING? EVERY MONTH? THE CHARTER? WE HAVE BEEN CHEATING OURSELVES. HERE IS MY THREE QUARTERS OF THE FEE.",
     "TAKING TURNS. THEN NEITHER OF US WON. A DRAW, HONESTLY. HERE IS MY THREE QUARTERS OF THE FEE."},
    // RANGE (judge: the witness in the other town says OUR ground)
    {"THE BLACKTHORN RIDGE", "THE SPRING", {"THE LINE IS THE RIDGE.", "THE LINE IS THE SPRING.", "THE RUN STAYS COMMON."},
     {"MY MASTER SHOT FROM THE BLACKTHORN RIDGE AND SAID: HERE ENDS OUR GROUND. WE BLAZED THE THORN, A CROSS CUT IN IT. THE RIDGE, YOUNG ONE.",
      "THE SPRING WAS THE LINE. WE DRANK AT IT FROM BOTH SIDES AND SET A CAIRN. THE RIDGE WAS ALWAYS NOBODY'S. THE CAIRN IS STILL THERE, MOSSY.",
      "A STAG-RUN IS COMMON. THAT IS THE OLD HUNT-LAW. FIRST ARROW BY LOTS, YEAR ABOUT. NOBODY DREW A LINE ACROSS A RUN. I SHOT THE LOTS FOR FORTY YEARS."},
     {"A BLACKTHORN WITH AN AXE-CUT CROSS.", "A MOSSY CAIRN BY THE SPRING.", "A WORN TRAIL. NO BLAZE, NO CAIRN."},
     "SPEAK QUIET. THE STAG IS NEAR. THE LINE RAN BY THE SPRING. NOT THE RIDGE. ASK THE OLD ONE AT THE SMOKEHOUSE, {WITNESS}. THEY SHOT ITS GRANDSIRE.",
     "SPEAK QUIET. THE STAG IS NEAR. THE OLD HUNTER IS GONE. THE HUNT-ROLL IS IN THE EMPTY LODGE AT {DEEDRUIN}, WHERE THEY KEPT THE LOTS AND THE LAW.",
     "THE EMPTY LODGE",
     "THE LINE IS THE BLACKTHORN RIDGE. {OTHER} SAYS THE SPRING. A STAG DOES NOT READ BOUNDARIES, BUT I DO. RULE MY WAY AND THERE IS A PURSE. YOU WILL EAT WELL EITHER WAY.",
     "THE RIDGE IS MINE, AND THE STAG GRAZES ON MY SIDE. THE FEE, AND HERE IS THE PURSE. EAT WELL TONIGHT, ON ME.",
     "THE SPRING. THEN THE STAG GRAZES ON THEIR SIDE. I'LL NOT FOLLOW IT THERE. HERE IS MY HONEST PART OF THE FEE, FOUR TENTHS.",
     "THE SPRING. THE STAG IS MINE AT DUSK, THEN. HERE IS THE REST OF YOUR FEE, SIX TENTHS. SPEAK QUIET.",
     "LOTS FOR THE FIRST ARROW? THE OLD LAW. I'LL DRAW FIRST THIS YEAR. FAIR. HERE IS MY THREE QUARTERS OF THE FEE.",
     "FIRST ARROW BY LOTS. I'LL BRING THE STONES. HERE IS MY THREE QUARTERS OF THE FEE."},
};

void prep(const JobCtx& c, uint64_t seed, std::map<std::string, std::string>& P) {
  Dice d(seed);
  const Ground& G = kGround[c.variant];
  const bool between = c.variant == V_WOOD || c.variant == V_REACH || c.variant == V_RANGE;
  const int truth = c.extra;
  P["first"] = between ? "go_other" : "other";
  P["spotrole"] = between ? "othertown" : "home";
  P["claim"] = G.giverClaim;
  P["otherline"] = c.twist == T_CHARTER ? G.otherCharter : G.other;
  P["afterother"] = c.twist == T_CHARTER ? "deed" : "witness";
  P["deedplace"] = G.deedplace;
  P["testimony"] = G.testimony[truth];
  P["deeda"] = truth == X_COMMON ? "THE DEED NAMES NO MARK AT ALL." : "THE DEED NAMES THE MARK.";
  P["deedb"] = truth == X_GIVER ? std::string("THE LINE RAN AT ") + G.claimA + "." : truth == X_NEIGHBOUR ? std::string("THE LINE RAN AT ") + G.claimB + "."
                                                                                                          : "THIS GROUND IS COMMON, FOR ALL.";
  P["signa"] = G.sign[truth];
  P["signshort"] = G.sign[truth];
  P["signsay"] = "";
  P["labela"] = G.labels[0]; P["labelb"] = G.labels[1]; P["labelc"] = G.labels[2];
  P["rulea"] = truth == X_GIVER ? "right_end" : "bought_end";
  P["ruleb"] = truth == X_NEIGHBOUR ? "nb_giver" : "wronged_b";
  P["rulec"] = truth == X_COMMON ? "common_giver" : "wronged_c";
  P["giverright"] = G.right;
  P["boughtline"] = c.variant == V_GRAVE ? "THE CORNER IS OURS, AND NOBODY CAN SAY OTHERWISE NOW. THE TEMPLE'S FEE. ASK NOTHING MORE OF ME."
                                         : "THE LINE IS MINE, AND NOBODY CAN SAY OTHERWISE. HERE IS THE PURSE I SWORE, AND THE FEE. ASK NOTHING MORE OF ME.";
  P["rightpay"] = c.variant == V_GRAVE ? "100" : "150";   // the temple offers no purse
  P["nbgiver"] = G.nbGiver; P["nbother"] = G.nbOther; P["comgiver"] = G.comGiver; P["comother"] = G.comOther;
  // the wronged giver: what they could know (the other side's crowing, the witness or the deed, the ground)
  P["wronggiver"] = c.twist == T_CHARTER ? "{OTHER} CAME CROWING TO MY DOOR WITH YOUR RULING. THE CHARTER SAYS OTHERWISE, AND SO DOES THE GROUND. NO FEE FOR YOU TODAY."
                                         : "{OTHER} CAME CROWING TO MY DOOR WITH YOUR RULING. {WITNESS} SAYS OTHERWISE, AND SO DOES THE GROUND. NO FEE FOR YOU TODAY.";
  static const char* descs[] = {"{GIVER} AND {OTHER} OF {HOME} QUARREL OVER A HEDGE AND A DITCH.", "THE OAKS BETWEEN {HOME} AND {OTHERTOWN} ARE DISPUTED.",
                                "{GIVER}'S CHURCHYARD AND {OTHER}'S FIELD DISAGREE ABOUT THE LINE.", "THE RIVER BETWEEN {HOME} AND {OTHERTOWN} HAS MOVED.",
                                "TWO STALLS IN THE SQUARE OF {HOME} DISAGREE ABOUT WHERE ONE ENDS.", "THE HUNTING GROUND BETWEEN {HOME} AND {OTHERTOWN} HAS TWO CLAIMANTS."};
  P["desc"] = descs[c.variant];
  std::vector<std::string> R;
  switch (c.variant) {
    case V_FIELD: R = {"THE FLOOD TOOK THE HEDGE-STAKES. {OTHER} NEXT DOOR HAS SET NEW ONES A PACE INTO MY BARLEY, AND MY OLD PEAR TREE NOW STANDS ON THE WRONG SIDE.",
                       "{OTHER} NEXT DOOR SAYS MY PEAR TREE LEANS INTO THEIR YARD, SO THE PEARS BELONG TO THEM. THE TREE HAS BEEN MINE FIFTY YEARS."}; break;
    case V_WOOD: R = {"THE STONE BETWEEN {HOME} AND {OTHERTOWN} STOOD AT THE SPLIT OAK, SO THE OAKS ARE MINE. {OTHER} SAYS THE BROOK, AND AXES ARE OUT."}; break;
    case V_GRAVE: R = {"{OTHER}'S BARLEY RUNS UP TO AN OLD GRAVE AT THE EDGE OF OUR CHURCHYARD. {OTHER} WANTS TO PLOUGH ROUND IT. I SAY IT IS HOLY GROUND."};
                  if (c.season == 0) R.push_back("{OTHER}'S PLOUGH CAUGHT THE STONE OF AN OLD GRAVE AND SPLIT IT. NOW WE DISAGREE WHERE THE HOLY GROUND ENDS.");
                  break;
    case V_REACH: R = {"THE RIVER MOVED LAST SPRING AND TOOK THE LINE WITH IT, SO THEY SAY. {OTHER} OF {OTHERTOWN} NOW NETS WHERE MY FATHER NETTED."}; break;
    case V_STALLS: R = {"THE LINE PAINTED ON THE COBBLES WASHED AWAY. {OTHER} SAYS OUR STALLS DIVIDED AT THE TROUGH. I SAY AT THE PUMP. WE NEVER SPEAK."};
                   if (c.season == 2) R.push_back("THE FAIR BRINGS TWICE THE STALLS AND HALF THE ROOM. {OTHER} AND I SAY THE LINE FELL IN DIFFERENT PLACES.");
                   break;
    default: R = {"A STAG CROSSES THE LINE BETWEEN {HOME} AND {OTHERTOWN} EVERY DUSK, AND NEITHER SIDE WILL LET THE OTHER'S ARROW AT IT."};
             if (c.season == 2) R.push_back("THE RUT IS ON, AND TWO HUNTING PARTIES STOOD NOSE TO NOSE YESTERDAY. NOBODY KNOWS WHICH SIDE OF THE LINE THE STAG BELONGS TO.");
             break;
  }
  if (c.war && (c.variant == V_FIELD || c.variant == V_WOOD || c.variant == V_REACH || c.variant == V_RANGE))
    R.push_back("SOLDIERS CAMPED HERE AND PULLED THE MARKER STONES INTO THEIR FIRE-RINGS. NOW {OTHER} AND I AGREE ON NOTHING.");
  P["reason"] = R[(size_t)d.below((int)R.size())];
  std::vector<std::string> T = {"THE MARCH STONE", "A LINE IN THE EARTH", "WALKING THE BOUNDS", "WHERE THE STONE STOOD"};
  static const std::vector<std::vector<std::string>> own = {{"WHOSE FURROW?", "A HEDGE TOO FAR"}, {"WHOSE OAKS?"}, {"THE GRAVE ON THE LINE", "GROUND OF THE DEAD"},
                                                            {"THE RIVER MOVED", "WHOSE REACH?"}, {"THE PAINTED LINE", "PUMP OR TROUGH"}, {"WHOSE STAG?", "THE RUT AND THE RIDGE"}};
  for (const std::string& s : own[(size_t)c.variant]) T.push_back(s);
  P["title"] = T[(size_t)d.below((int)T.size())];
}

bool choose(const JobCtx& w, uint64_t dice, JobCtx& out) {
  Dice d(dice);
  out = w;
  switch ((Role)w.trade) {
    case Role::Farmer: out.variant = V_FIELD; break;
    case Role::Jarl: out.variant = V_WOOD; break;
    case Role::Priest: out.variant = V_GRAVE; break;
    case Role::Fisher: out.variant = V_REACH; break;
    case Role::Merchant: if (w.tier < 2) return false; out.variant = V_STALLS; break;
    case Role::Hunter: out.variant = V_RANGE; break;
    default: return false;
  }
  const int r = d.below(10);
  out.extra = r < 4 ? X_GIVER : r < 8 ? X_NEIGHBOUR : X_COMMON;
  // the charter: a lord's wood in a city (its clerks lost the roll), else one time in four (the witness died since)
  out.twist = (out.variant == V_WOOD && w.tier == 2) || d.below(4) == 0 ? T_CHARTER : T_WITNESS;
  return true;
}

JobCtx walk(Role t, int variant, int twist, int truth, int tier = 0, bool danger = false) {
  JobCtx c;
  c.trade = (int)t; c.variant = variant; c.twist = twist; c.extra = truth; c.tier = tier; c.danger = danger; c.season = 1;
  return c;
}

}  // namespace

const KindDef& kindBounds() {
  static const KindDef K = [] {
    KindDef k;
    k.id = "bounds";
    k.title = "BOUNDS DISPUTE";
    k.lead = "WANTS A BOUNDARY QUARREL SETTLED";
    k.body = kBody;
    k.variants = {"field", "wood", "grave", "reach", "stalls", "range"};
    k.twists = {"witness", "charter"};
    k.extras = {"giver", "neighbour", "common"};
    k.trades = {(int)Role::Farmer, (int)Role::Jarl, (int)Role::Priest, (int)Role::Fisher, (int)Role::Merchant, (int)Role::Hunter};
    k.choose = choose;
    k.prep = prep;
    k.walks = {walk(Role::Farmer, V_FIELD, T_WITNESS, X_GIVER), walk(Role::Jarl, V_WOOD, T_CHARTER, X_NEIGHBOUR, 2, true),
               walk(Role::Priest, V_GRAVE, T_WITNESS, X_COMMON), walk(Role::Fisher, V_REACH, T_WITNESS, X_NEIGHBOUR),
               walk(Role::Merchant, V_STALLS, T_CHARTER, X_COMMON, 2), walk(Role::Hunter, V_RANGE, T_WITNESS, X_GIVER, 0, true)};
    return k;
  }();
  return K;
}

}  // namespace radiant
}  // namespace story
