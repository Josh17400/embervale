// P1b radiant job kind 13: MEND THE WORKS (content: jobs_c.md section 3; judge notes resolved). JOBS lane.
//
// A bridge, a mill, a wall, a hall roof, a shrine, a pier or a boat slip is broken, and the settlement cannot wait. The
// giver has paid for the materials at the yard of a neighbouring town: carry the load home (heavy; a deadline: late and
// the town's price of the good rises), give it to the foreman, then LEND A HAND (a day's labour; when trouble is near it
// comes at dusk), KEEP WATCH (only when trouble is near: the night's wave, builders asleep and safe, a better fee), or
// leave them to it (less pay, no Find). Inside the old work the builders FIND something that forces a small choice about
// who the past belongs to (a toll-box, a dead miller's ledger, a child in the wall, a magpie's hoard, a founders' jar, a
// drowned sailor, a dead shipwright's lines).
// Judge: the trouble's creature is chosen per work and named only where it is true (wolves at the bridge, the roof and
// the strand; a boar at the mill; bandits at the wall; mud crabs up the slip; the shrine has none); the hall's beam is one
// great timber (logs); the mill's debt is sworn for the next harvest and the miller's daughter offers the finder a cut out
// loud; the toll-box for the lord and the ring for the kin go through the giver (no new jobs spawned); the slip's lines
// speak of the thaw only in winter.
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
role yard site town near
role seller person [[male|female]] at yard
role foreman person [[male|female]] at home
var late 0
var safe 0
var away 0

stage start
  talk giver
  say "[[=reason]] [[=ask]]"
  opt "[[I'LL FETCH IT.|I'LL SEE IT STANDING AGAIN.|WHERE'S THE YARD?]]" -> haul
  opt "[[NOT TODAY.|FIND A CARTER.|I'M NO BUILDER.]]" -> decline

stage haul
  goal goto yard
  journal "[[=desc]] THE LOAD IS PAID FOR AT {YARD}, {YARD.DIR}: SEE {SELLER}."
  deadline [[=clock]] -> haul_late
  then load

stage haul_late
  goal goto yard
  journal "THE WORK IS LATE, AND {HOME} FEELS IT. FETCH THE LOAD FROM {SELLER} AT {YARD}."
  do set late 1
  then load

stage load
  talk seller
  say "FOR {GIVER} OF {HOME}? PAID FOR, EVERY PIECE. [[=load]], AND MIND YOUR BACK. IT'S HEAVIER THAN IT LOOKS."
  journal "LOAD [[=load]] AT {YARD} AND CARRY IT TO {FOREMAN}, THE FOREMAN AT {HOME}."
  do give "[[=load]]" [[=icon]] heavy
  opt "[[I'LL MANAGE.|LOAD IT UP.]]" -> carry

stage carry
  goal goto home
  journal "CARRY [[=load]] HOME TO {FOREMAN}, THE FOREMAN AT {HOME}."
  then foreman_talk

stage foreman_talk
  talk foreman
{?tw_trouble
  say "[[=open]] [[=troubleline]]"
}
{!tw_trouble
  say "[[=open]] [[=calmline]]"
}
  journal "HAND THE LOAD TO {FOREMAN}, AND DECIDE HOW TO HELP."
  do take "[[=load]]"
  opt "[[I'LL LEND A HAND.|GIVE ME A HAMMER.]]" -> hand
{?tw_trouble
  opt "[[I'LL KEEP WATCH.|I'LL WATCH THE DARK FOR YOU.]]" -> watch_wait
}
  opt "[[I HAVE PLACES TO BE.|YOU'LL MANAGE WITHOUT ME.]]" -> leave

stage hand
  goal wait 1
  journal "LABOUR A DAY AT THE WORKS IN {HOME}."
  do xp 20
{?tw_trouble
  then hand_fight
}
{!tw_trouble
  then find
}

{?tw_trouble
stage hand_fight
  goal time 0 5
  journal "[[=dusk]] FIGHT BESIDE THE BUILDERS UNTIL THE SMALL HOURS."
  do ambush [[=foe]] [[=wave]]
  then find

stage watch_wait
  goal time 21 24
  journal "KEEP WATCH OVER THE WORKS OF {HOME} TONIGHT. THE BUILDERS SLEEP."
  then watch

stage watch
  goal time 2 5
  journal "[[=night]] HOLD UNTIL THE SMALL HOURS. THE BUILDERS SLEEP SAFE."
  do set safe 1
  do ambush [[=foe]] [[=wavew]]
  then find
}

stage leave
  goal wait 2
  journal "THE BUILDERS OF {HOME} FINISH THE WORK WITHOUT YOU. COME BACK IN A DAY OR TWO."
  do set away 1
  then report

stage find
  talk foreman
  say "[[=findline]]"
  journal "THE BUILDERS FOUND SOMETHING IN THE OLD WORK. {FOREMAN} WANTS YOUR WORD ON IT."
  opt "[[=f1]]" -> found1
  opt "[[=f2]]" [[=f2if]]-> found2
  opt "[[=f3]]" -> found3

stage found1
  talk foreman
  say "[[=r1]]"
  journal "THE FIND IS SETTLED. TELL {GIVER} THE WORK IS DONE."
[[=fx1]]
  opt "[[TELL {GIVER} IT'S DONE.|GOOD WORK, ALL OF YOU.]]" -> report

stage found2
  talk foreman
  say "[[=r2]]"
  journal "THE FIND IS SETTLED. TELL {GIVER} THE WORK IS DONE."
[[=fx2]]
  opt "[[TELL {GIVER} IT'S DONE.|GOOD WORK, ALL OF YOU.]]" -> report

stage found3
  talk foreman
  say "[[=r3]]"
  journal "THE FIND IS SETTLED. TELL {GIVER} THE WORK IS DONE."
[[=fx3]]
  opt "[[TELL {GIVER} IT'S DONE.|GOOD WORK, ALL OF YOU.]]" -> report

stage report
  talk giver
  say "[[YOU'RE BACK. COME AND LOOK.|WELL? COME AND SEE IT.|LOOK AT IT. JUST LOOK.]]"
  journal "TELL {GIVER} IN {HOME} THE WORK IS DONE."
  opt "[[IT STANDS.|THE WORK IS DONE.]]" if novar late 1 if novar away 1 if novar safe 1 -> done_end
  opt "[[IT STANDS, AND NOBODY WAS HURT.|DONE. THE BUILDERS SLEPT SAFE.]]" if is safe 1 if novar late 1 -> safe_end
  opt "[[IT STANDS, LATE AS IT IS.|LATE, BUT DONE.]]" if is late 1 if novar away 1 -> late_end
  opt "[[THE BUILDERS FINISHED IT.|IT'S DONE, WITHOUT ME.]]" if is away 1 -> away_end

stage done_end
  end success
  say "[[=done]]"
  journal "[[=donej]]"
  do reward fair 100
  do remember giver "[[=mem]]"
  do census home mood 3
  do census home price [[=good]] 90
  do notice "{HOME} REMEMBERS WHO MENDED IT."

stage safe_end
  end success
  say "[[=done]] AND NOT ONE BUILDER HURT. THAT'S WORTH MORE."
  journal "[[=donej]] YOU KEPT WATCH, AND THE BUILDERS SLEPT SAFE."
  do reward fair 125
  do remember giver "[[=mem]]"
  do census home mood 4
  do census home price [[=good]] 90
  do notice "{HOME} REMEMBERS WHO MENDED IT."

stage late_end
  end success
  say "[[=lateline]]"
  journal "[[=donej]] IT CAME LATE, AND {HOME} PAID FOR THE WAIT."
  do reward fair 80
  do remember giver "[[=mem]]"
  do census home mood 1
  do census home price [[=good]] 115

stage away_end
  end success
  say "[[=awayline]]"
  journal "THE BUILDERS OF {HOME} FINISHED THE WORK. YOU BROUGHT THE MATERIALS."
  do reward fair 70
  do remember giver "THE WORKS STAND. YOU BROUGHT THE LOAD, AT LEAST."
  do census home mood 1
  do census home price [[=good]] 95

stage decline
  end fail
  journal "YOU TURNED DOWN {GIVER}'S BROKEN WORKS."
)DSL";

enum { W_BRIDGE, W_MILL, W_WALL, W_ROOF, W_SHRINE, W_PIER, W_SLIP };

struct Work {
  const char* good;       // the census good (price after the work)
  const char* icon;
  const char* open;       // the foreman's opening: the good, the place to stack it
  const char* trouble;    // ... and with trouble near (work or watch)
  const char* calm;       // ... and with none
  const char* foe; const char* foeWord;
  const char* find;
  const char* f[3]; const char* r[3]; const char* fx[3];
  const char* done; const char* late; const char* away; const char* donej;
};
const Work kWorks[] = {
    {"planks", "ingot", "PLANKS? GOOD. I WAS ABOUT TO BUILD IT OUT OF PRAYERS. SET THEM THERE. NOT THERE. THERE.",
     "WOLVES COME DOWN TO THE WATER AT NIGHT, AND THEY'VE TAKEN TO WATCHING US. WILL YOU WORK, OR WATCH FOR US?",
     "ARE YOU STAYING FOR THE WORK, OR ONLY FOR THE THANKS?", "wolf", "WOLVES",
     "WE FOUND THIS IN THE OLD ABUTMENT. A TOLL-BOX, WITH OLD COIN INSIDE, A HUNDRED YEARS OLD. THE LORD CLAIMS ALL TREASURE. BUT THE LORD IS FAR, AND WE ARE HERE.",
     {"GIVE IT TO THE VILLAGE.", "KEEP IT.", "TAKE IT TO THE LORD."},
     {"FOR THE BRIDGE-WARDENS' CHEST, THEN. IT'LL PAY FOR THE NEXT FLOOD. WE'LL CUT YOUR NAME IN THE RAIL.",
      "...YOURS, THEN. I DIDN'T SEE IT. I WON'T SEE YOU, EITHER, FOR A WHILE.",
      "THE LORD'S, THEN. {GIVER} WILL SEND IT ON WITH A LETTER. HONEST TO A FAULT, YOU."},
     {"  do fame 2\n  do remember foreman \"YOUR NAME IS CUT IN THE BRIDGE RAIL. THE CARTERS TOUCH IT FOR LUCK.\"",
      "  do gold 40\n  do deed stole\n  do remember foreman \"I DON'T TALK ABOUT THE TOLL-BOX. NOT WITH YOU.\"",
      "  do reward small 70\n  do remember giver \"THE LORD SENT THANKS FOR THE OLD TOLL-BOX. A LETTER WITH A SEAL.\""},
     "IT STANDS! IT ACTUALLY STANDS! THE FIRST CART CROSSED AT NOON, AND THE CARTER KISSED THE RAIL.",
     "LATE, BUT NOT TOO LATE. THE CARTS ARE ROLLING AGAIN. YOU HAVE MY THANKS.",
     "THE WORKERS DID WELL. I OWE YOU FOR THE PLANKS, AT LEAST. THE REST IS THEIRS.", "THE BRIDGE OF {HOME} STANDS AGAIN."},
    {"ingot", "ingot", "INGOTS, AT LAST. THE WHEEL HAS STOOD A WEEK, AND SO HAVE MY WAGES. LAY THEM BY THE SHAFT AND MIND THE RACE.",
     "A BOAR HAS BEEN AT THE GRAIN SACKS EVERY NIGHT SINCE THE WHEEL STOPPED. WORK, OR WATCH?",
     "THE SHAFT NEEDS FOUR HANDS. YOURS WOULD MAKE IT FIVE. HELPING?", "boar", "A HUNGRY BOAR",
     "HERE. THE DEAD MILLER'S LEDGER. FORTY YEARS OF GRAIN LENT TO THIS VILLAGE, NEVER ASKED BACK. THE LAST PAGE SAYS: LET IT BE FORGOTTEN, IF THEY NEED TO.",
     {"BURN IT.", "READ IT OUT AT THE MILL.", "SAY NOTHING."},
     {"THE MILLER'S DAUGHTER WEPT WHEN IT BURNED, AND THEN SHE LAUGHED. SHE WANTS YOU TO HAVE HIS TALLY-STICK.",
      "READ OUT, THEN. THE VILLAGE SWEARS TO REPAY IT AT THE NEXT HARVEST. THE DAUGHTER SAYS, OUT LOUD, THE FINDER GETS A TENTH. A DEBT READ ALOUD IS PAID IN ANGER, MIND.",
      "BACK IN THE WALL IT GOES. SOME THINGS ARE BETTER LEFT IN THE WALL."},
     {"  do xp 60\n  do fame 1\n  do give \"THE MILLER'S TALLY-STICK\" bone\n  do remember giver \"YOU BURNED THE MILLER'S LEDGER. HIS DAUGHTER STILL SMILES AT YOU.\"",
      "  do gold 40\n  do remember giver \"THE MILLER'S DEBT, READ OUT AT THE MILL. WE'LL PAY IT AT HARVEST. NOBODY'S HAPPY.\"",
      "  do remember foreman \"THE LEDGER'S BACK IN THE WALL. YOU AND I KNOW. NOBODY ELSE.\""},
     "LISTEN! THE WHEEL, AND THE STONES GRINDING. I HAVE MISSED THAT NOISE. THE FIRST SACK IS YOURS.",
     "SOME GRAIN SPOILED WHILE WE WAITED, BUT THE MILL TURNS. THANK YOU.", "THE MILLERS FITTED THE SHAFT WITHOUT YOU. I OWE YOU FOR THE IRON ONLY.",
     "THE MILL OF {HOME} TURNS AGAIN."},
    {"logs", "ingot", "LOGS. GOOD, STRAIGHT ONES. LAY THEM BY THE GAP. WE WORK BY DAY AND SLEEP IN OUR BOOTS.",
     "BANDITS WATCH US FROM THE HILL AT NIGHT. WORK, OR WATCH THE DARK?", "BANDITS WATCH US FROM THE HILL AT NIGHT. WORK, OR WATCH THE DARK?",
     "bandit", "BANDITS",
     "THERE WAS A CHILD IN THE FILL OF THIS WALL. SET THERE ON PURPOSE, FOR LUCK, THE OLD STORIES SAY. WHAT DO WE DO WITH THE CHILD?",
     {"BURY THE CHILD PROPERLY.", "SEAL THE CHILD BACK IN.", "LEAVE IT TO THE PRIEST."},
     {"A PROPER GRAVE, THEN, WITH THE PRIEST'S WORDS. ...I'LL TAKE MY CAP OFF FOR THAT.",
      "SEALED IN, AND THE WALL WILL HOLD. ...I WON'T FORGET YOU SAID IT. I WON'T SPEAK OF IT, EITHER.",
      "THE PRIEST WILL KNOW WHAT'S RIGHT. I'LL SEND FOR THEM."},
     {"  do deed grave\n  do fame 1\n  do xp 40\n  do remember foreman \"THE CHILD IN THE WALL HAS A GRAVE NOW. YOU DID THAT.\"",
      "  do deed abandoned\n  do remember foreman \"I HAVE NOTHING TO SAY TO YOU.\"",
      "  do remember foreman \"THE PRIEST TOOK THE CHILD. A QUIET THING.\""},
     "THE GAP IS CLOSED. I SLEPT, FOR THE FIRST TIME IN A WEEK. THE WATCH CHEERED AS THE LAST STAKE WENT IN.",
     "THE WALL STANDS, THOUGH NOT BEFORE THE SECOND RAID. WE LOST LITTLE. THANK YOU.",
     "THE WALL IS UP. THE WATCH BUILT IT. I OWE YOU FOR THE TIMBER, ALL THE SAME.", "THE WALL OF {HOME} IS WHOLE AGAIN."},
    {"logs", "ingot", "A BEAM! ONE GREAT TIMBER, AND DRY? YOU ARE A SAINT. THE OLD ONE CRACKED AGAIN LAST NIGHT, AND THE WHOLE HALL SANG ALONG.",
     "WOLVES COME FOR THE WARMTH OF THE HALL AT NIGHT. HANDS, OR WATCH?",
     "STACK IT BY THE DOOR. CAN YOU HOLD A LADDER, OR ONLY ADMIRE ONE?", "wolf", "WOLVES",
     "IT'S A MAGPIE'S HOARD, IN THE OLD BEAM! LOOK, A BUTTON FROM MY MOTHER'S BEST COAT, GONE THIRTY YEARS. THE THIEF WAS A BIRD.",
     {"RETURN WHAT YOU CAN.", "TAKE IT FOR THE ROOF FUND.", "LEAVE THE BIRD ITS HOARD."},
     {"BUTTONS BACK TO THEIR COATS, THEN. HALF THE HALL WILL BE WEEPING INTO THEIR SOUP. THEY'LL FEED YOU FOR IT.",
      "THE ROOF FUND THANKS THE MAGPIE. AND YOU.", "LEAVE IT TO THE BIRD? ...FAIR. IT DID THE WORK. LOOK, IT'S WATCHING YOU GO."},
     {"  do give \"A HOT MEAL IN THE HALL\" meat\n  do remember giver \"THE MAGPIE'S BUTTONS WENT HOME. MY MOTHER'S COAT IS WHOLE AGAIN.\"",
      "  do gold 30", "  do xp 20\n  do remember foreman \"THE MAGPIE STILL FOLLOWS YOU TO THE DOOR. IT KNOWS A FRIEND.\""},
     "THE BEAM IS NEW, AND NOT A CREAK! THE WHOLE HALL STOOD UNDER IT AND SANG, AND THE ROOF DID NOT SING BACK.",
     "THE SNOW CAME DOWN BEFORE THE ROOF WENT UP, BUT NOBODY WAS UNDER IT. THANKS.", "THE ROOF IS ON. THE CARPENTERS WERE QUICK. I OWE YOU FOR THE TIMBER.",
     "THE HALL ROOF OF {HOME} HAS A NEW BEAM."},
    {"pottery", "ore", "TILES, AND NOT A CRACK IN THEM? THE PRIEST WILL WEEP. STACK THEM BY THE BELL ROPE.", "", "I CAN LAY THEM, BUT I CANNOT PRAY THEM IN. WILL YOU CARRY?",
     "wolf", "",
     "UNDER THE ALTAR-STEP, A SEALED JAR: GRAIN, A COIN, AND A STRIP OF LEAD. MAY THIS ROOF OUTLAST US ALL. THE FOUNDERS BURIED IT. WHAT NOW?",
     {"BURY IT AGAIN UNDER THE NEW TILES.", "ADD A COIN OF MY OWN. (10 GOLD)", "KEEP THE COIN."},
     {"UNDER THE NEW TILES, THEN, FOR THE NEXT HUNDRED YEARS. THE PRIEST WILL BLESS YOU FOR IT.",
      "YOUR COIN BESIDE THEIRS, AND A SLIP OF YOUR OWN. HERE, THE PRIEST MADE YOU A CHARM FROM THE OLD LEAD.",
      "...KEEP IT, THEN. THE PRIEST WON'T SAY ANYTHING. NOT FOR A WEEK, ANYWAY."},
     {"  do fame 1\n  do xp 30\n  do remember giver \"THE FOUNDERS' JAR IS UNDER THE NEW TILES. YOU HONOURED THEM.\"",
      "  do gold -10\n  do fame 2\n  do xp 40\n  do give \"THE ROOF-COIN\" gold",
      "  do gold 25\n  do remember giver \"THE FOUNDERS' COIN. ...I WON'T SPEAK OF IT. NOT THIS WEEK.\""},
     "THE BELL RANG THIS MORNING. THE WHOLE VILLAGE CAME OUT TO HEAR IT. THE ROOF IS WHOLE AND SO ARE WE. BLESS YOU.",
     "THE RAIN GOT IN FOR A WEEK, BUT THE ROOF IS WHOLE, AND THE BELL IS LOUD.", "THE ROOF IS DONE. YOU CARRIED THE TILES, AND THAT IS HALF THE PRAYER.",
     "THE SHRINE OF {HOME} IS ROOFED, AND ITS BELL RINGS."},
    {"logs", "ingot", "LOGS. GOOD ONES, TARRED FOR THE SALT. THE SEA TOOK HALF THE PIER AND THREW THE REST AT THE BOATS. STACK THEM THERE.",
     "WOLVES COME DOWN TO THE STRAND AT NIGHT FOR THE SPOILED CATCH. HOLD THE ROPE, OR THE DARK?",
     "HOLD THE ROPE WHEN I SAY HOLD, AND TRY NOT TO LET GO.", "wolf", "WOLVES",
     "THE SILT HAD A BOAT IN IT, AND A SAILOR IN THE BOAT. THERE IS A RING ON A FINGER. NO NAME ON THE RING.",
     {"BURY THE SAILOR ASHORE.", "TAKE THE RING.", "THE RING GOES TO THE HARBOUR, FOR KIN."},
     {"ASHORE, WITH THE PRIEST'S WORDS. THE SEA HAD THEM LONG ENOUGH.", "...THE RING IS YOURS, THEN. THE HARBOUR WON'T LIKE IT.",
      "{GIVER} WILL KEEP THE RING AND ASK EVERY BOAT THAT COMES IN. SOMEONE WILL KNOW IT."},
     {"  do deed grave\n  do remember giver \"THE SAILOR FROM THE SILT IS BURIED ASHORE. A GOOD THING.\"",
      "  do gold 40\n  do remember giver \"THE DROWNED SAILOR'S RING. YOU TOOK IT. I REMEMBER.\"",
      "  do rumour\n  do reward small 20\n  do remember giver \"STILL ASKING ABOUT THAT RING. A WIDOW TWO TOWNS OVER, MAYBE.\""},
     "THE FIRST BOAT TIED UP AT NOON, AND THE CATCH CAME OFF DRY. THE OLD PIER WAS NEVER THIS SOLID. THANK YOU.",
     "WE LOST SOME FISH TO THE WAIT, BUT THE BOATS LAND AGAIN. YOU HAVE MY THANKS.", "THE PIER IS OUT AGAIN. THE FISHERS BUILT IT. I OWE YOU FOR THE LOGS.",
     "THE PIER OF {HOME} STANDS AGAIN."},
    {"planks", "ingot", "[[=slipopen]]", "THE SEA SENDS THINGS UP THE SLIP AT NIGHT. MUD CRABS THE SIZE OF DOGS. HAULING WITH US, OR WATCHING?",
     "THE HULL WEIGHS MORE THAN IT LOOKS. ARE YOU HAULING WITH US?", "mudcrab", "MUD CRABS",
     "UNDER THE OLD SLIP, A STONE. SOMEONE SCORED A HULL ON IT, EVERY CURVE, FOR A BOAT I'VE NEVER SEEN. IT LOOKS FAST. THE BUILDER DIED. SO WHOSE IS IT?",
     {"GIVE THE LINES TO THE YARD.", "SELL COPIES TO THE OTHER YARD.", "SEAL THEM BACK UNDER THE SLIP."},
     {"THE YARD WILL BUILD HER. THE LINES ARE YOURS TO RIDE, WHENEVER SHE SAILS.", "SOLD. ...THE YARD HERE WILL BE WARY OF YOU A WHILE.",
      "BACK UNDER THE SLIP. THE LINES WAIT FOR THE NEXT CENTURY, THEN."},
     {"  do fame 2\n  do remember foreman \"THE FAST HULL FROM THE STONE IS ON THE SLIP NOW. THE LINES ARE YOURS TO RIDE.\"",
      "  do gold 40\n  do remember foreman \"YOU SOLD THE DEAD MAN'S LINES. I KNOW. EVERYONE KNOWS.\"",
      "  do remember foreman \"THE LINES ARE UNDER THE SLIP AGAIN. SOMETIMES I THINK ABOUT THEM.\""},
     "[[=slipdone]]", "THE HULL SPLIT A LITTLE ON THE SHINGLE BEFORE WE GOT IT ON. IT WILL SAIL.",
     "[[=slipaway]]", "THE BOAT SLIP OF {HOME} IS WHOLE, AND THE HULL SAFE ON IT."},
};

void prep(const JobCtx& c, uint64_t seed, std::map<std::string, std::string>& P) {
  Dice d(seed);
  const Work& W = kWorks[c.variant];
  const int n = 4 + 2 * c.tier + d.below(3);
  static const char* cw[] = {"ZERO", "ONE", "TWO", "THREE", "FOUR", "FIVE", "SIX", "SEVEN", "EIGHT", "NINE", "TEN", "ELEVEN", "TWELVE"};
  const std::string N = cw[n > 12 ? 12 : n];
  const bool city = c.tier == 2;
  std::string load;
  switch (c.variant) {
    case W_BRIDGE: load = city ? N + " PLANKS AND FOUR LOGS" : N + " PLANKS"; break;
    case W_MILL: load = N + " INGOTS"; break;
    case W_WALL: load = city ? N + " LOGS AND HINGE IRON" : N + " LOGS"; break;
    case W_ROOF: load = city ? "A GREAT BEAM AND TILES" : "A GREAT BEAM"; break;
    case W_SHRINE: load = N + " CRATES OF TILES"; break;
    case W_PIER: load = N + " TARRED LOGS"; break;
    default: load = city ? N + " PLANKS AND SAILCLOTH" : N + " PLANKS"; break;
  }
  P["load"] = load; P["good"] = W.good; P["icon"] = W.icon;
  P["clock"] = std::to_string(c.tier == 0 ? 12 : c.tier == 1 ? 8 : 6);
  P["open"] = W.open;
  P["troubleline"] = W.trouble;
  P["calmline"] = W.calm;
  P["foe"] = W.foe;
  P["wave"] = std::to_string(3 + (c.danger ? 1 : 0) + (c.tier >= 1 ? 1 : 0));
  P["wavew"] = std::to_string(2 + (c.danger ? 1 : 0) + (c.tier >= 1 ? 1 : 0));
  P["dusk"] = std::string(W.foeWord) + " COME AT DUSK.";
  P["night"] = std::string(W.foeWord) + " COME IN THE NIGHT.";
  P["findline"] = W.find;
  for (int k = 0; k < 3; k++) {
    P["f" + std::to_string(k + 1)] = W.f[k];
    P["r" + std::to_string(k + 1)] = W.r[k];
    P["fx" + std::to_string(k + 1)] = W.fx[k];
  }
  P["f2if"] = c.variant == W_SHRINE ? "if gold 10 " : "";
  P["done"] = W.done; P["lateline"] = W.late; P["awayline"] = W.away; P["donej"] = W.donej;
  // the slip: the city's sailcloth, and the thaw only in winter (judge)
  P["slipopen"] = city ? "PLANKS AND SAILCLOTH. THE HULL WILL WEEP WITH RELIEF. THE PLANKS GO BY THE KEEL BLOCKS, THE CLOTH BY THE DOOR."
                       : "PLANKS. GOOD, DRY ONES. THE HULL WILL WEEP WITH RELIEF. STACK THEM BY THE KEEL BLOCKS, AND MIND THE TIDE.";
  P["slipdone"] = c.season == 3 ? "THE HULL SITS ON THE NEW SLIP, SQUARE ON THE KEEL BLOCKS, LIKE A CAT ON A WARM STONE. IT WILL SEE THE THAW."
                                : "THE HULL SITS ON THE NEW SLIP, SQUARE ON THE KEEL BLOCKS, LIKE A CAT ON A WARM STONE. IT WILL SAIL BY THE NEXT MOON.";
  P["slipaway"] = city ? "THE SLIP IS UP. THE YARD BUILT IT. I OWE YOU FOR THE PLANKS AND THE CLOTH." : "THE SLIP IS UP. THE YARD BUILT IT. I OWE YOU FOR THE PLANKS, NO MORE.";
  P["pay"] = "125";
  P["mem"] = std::string(W.donej) == "" ? "THE WORKS STAND." : std::string("YOU MENDED IT. ") + W.donej;
  // the reason and the ask
  std::vector<std::string> R;
  const Role t = (Role)c.trade;
  switch (c.variant) {
    case W_BRIDGE:
      R = {"MY BROTHER SWORE THE FOOTBRIDGE WOULD TAKE AN AFTERNOON. NOW MY BROTHER IS IN THE RIVER, AND SO IS THE BRIDGE, SIDE BY SIDE."};
      if (t == Role::Merchant) R.push_back("MY WAGONS HAVE SAT AT THE RIVER FOR NINE DAYS. THE BRIDGE IS GONE AND THE TOLL WITH IT, AND I NEED THE ROAD OPEN.");
      if (c.season == 0) R.push_back("THE FLOOD TOOK THE BRIDGE LAST WEEK. WITHOUT IT THE CARTS GO THE LONG WAY, AND THE LONG WAY IS THREE DAYS.");
      if (c.season == 2 && t == Role::Farmer) R.push_back("THE HARVEST CARTS MUST CROSS THE MILL-RACE, AND THE FOOTBRIDGE ROTTED THROUGH UNDER THE LAST LOAD. HALF MY BARLEY IS ON THE WRONG BANK.");
      break;
    case W_MILL:
      R = {"THE MILLER'S GOAT GOT INTO THE GEARING AND THE GEARING LOST. THE GOAT IS FINE. THE MILL IS NOT, AND NEITHER IS THE MILLER'S TEMPER."};
      if (t == Role::Smith) R.push_back(c.war ? "THE WAR LEVY TOOK EVERY INGOT IN MY SHOP, AND THEN THE MILL SHAFT BROKE. THE REEVE SAYS FIND IRON FOR THE MILL OR EAT RAW BARLEY."
                                              : "THE MILL-WHEEL SHAFT SNAPPED IN THE MIDDLE OF THE GRINDING, AND I HAVE NO IRON TO FORGE ANOTHER. THE GRAIN IS ROTTING.");
      if (t == Role::Farmer) R.push_back("THE MILL SHAFT SNAPPED WITH A WEEK OF GRINDING STILL TO DO, AND MY GRAIN SITS IN THE SACKS. THE MILLER CAN MEND IT, IF SOMEONE FETCHES THE IRON.");
      break;
    case W_WALL:
      R = {"THE WALL OF {HOME} HAS A GAP A CART CAN DRIVE THROUGH, AND BANDITS COUNT OUR NIGHT WATCH FROM THE HILL. I NEED IT CLOSED BEFORE THE NEXT NEW MOON.",
           "A CART BACKED INTO THE WALL AND THE WALL LOST. THE CARTER SAYS THE WALL SHOULD HAVE MOVED. THE GAP IS REAL, AND SO ARE THE BANDITS WHO ADMIRE IT."};
      if (city) R.push_back("THE GATE HINGES SNAPPED UNDER A BANDIT RAM, AND THE SMITHS ARE BEHIND ON THE WAR ORDERS. BRING THE IRON, AND THE GATE HANGS TONIGHT.");
      break;
    case W_ROOF:
      R = {"THE HOUSE CAT FOUND A HOLE IN THE ROOF, AND SO DID THE RAIN. THE CAT IS DRY. THE HAMS ARE NOT, AND THE CELLAR IS A POND."};
      if (c.season == 3) R.push_back(t == Role::Jarl ? "THE GREAT HALL'S MAIN BEAM HAS SPLIT, AND THE COUNCIL MEETS UNDER IT AT THE NEXT MOON. A FALLEN BEAM IS A BAD WAY TO END A TREATY."
                                                     : "THE SNOW HAS CRACKED THE MAIN BEAM OF MY HALL. IF IT FALLS WITH A FULL HOUSE UNDER IT, IT WILL NOT BE A QUIET FALL.");
      break;
    case W_SHRINE:
      R = {"THE HARD WINTER SPLIT THE SHRINE ROOF, AND THE FOUNDERS' TILES CAME DOWN ON THE ALTAR. NOBODY WAS HURT. THE BELL IS DENTED, BUT IT STILL RINGS.",
           "A FAMILY OF BATS HAS MOVED INTO THE OPEN ROOF OF THE SHRINE AND NOW ATTENDS EVERY SERVICE. THEY ARE VERY DEVOUT. THE ROOF IS STILL OPEN."};
      break;
    case W_PIER:
      R = {"MY NEPHEW TIED A NEW BOAT TO THE PIER, AND THE BOAT WON THE ARGUMENT. THE PIER IS NOW MOSTLY FIREWOOD. THE BOAT IS PERFECTLY FINE."};
      if (c.season >= 2) R.push_back("THE LAST STORM TORE HALF THE PIER AWAY. THE BOATS CANNOT LAND THE CATCH, AND THE FISH ARE SPOILING ON THE DECK.");
      break;
    default:
      R = {"THE OLD SLIP GAVE WAY UNDER A HULL HALF BUILT, AND NOW THE HULL LIES ON ITS SIDE ON THE SHINGLE. EVERY TIDE MAKES IT WORSE."};
      if (c.season >= 2) R.push_back("THE STORM TOOK THE BOATHOUSE ROOF AND HALF THE SLIP. THE HARBOUR'S ONLY SPARE HULL SITS OUT IN THE WEATHER, AND IT WILL NOT SURVIVE ANOTHER BLOW.");
      break;
  }
  P["reason"] = R[(size_t)d.below((int)R.size())];
  P["ask"] = d.pick({"THE LOAD IS PAID FOR AT {YARD}. BRING IT, AND KEEP THE WORKERS ALIVE.", "FETCH THE LOAD FROM {YARD}, AND I'LL FEED THE WORKERS.",
                     "THE YARD AT {YARD} HAS IT READY. SEE IT STANDING AGAIN."});
  static const char* descs[] = {"THE BRIDGE OF {HOME} IS DOWN.", "THE MILL-WHEEL SHAFT OF {HOME} IS BROKEN.", "THE WALL OF {HOME} HAS A BREACH.",
                                "THE HALL ROOF OF {HOME} IS CRACKING.", "THE SHRINE ROOF OF {HOME} IS OPEN TO THE SKY.", "THE PIER OF {HOME} IS HALF GONE.",
                                "THE BOAT SLIP OF {HOME} HAS FALLEN."};
  P["desc"] = descs[c.variant];
  static const std::vector<std::vector<std::string>> T = {{"THREE DAYS ROUND", "PLANKS FOR THE RIVER", "A BROTHER IN THE RIVER"},
                                                          {"THE SILENT MILL", "GRAIN IN THE SACKS", "THE GOAT AND THE GEARS"},
                                                          {"A GAP IN THE WALL", "CLOSE THE GAP", "A CART IN THE WALL"},
                                                          {"THE CRACKED BEAM", "A ROOF FOR THE HALL", "THE CAT AND THE RAIN"},
                                                          {"A ROOF FOR THE BELL", "TILES AND PRAYERS", "BATS IN THE SHRINE"},
                                                          {"THE CATCH IS ROTTING", "LOGS FOR THE QUAY", "A NEPHEW AND A BOAT"},
                                                          {"SAVE THE HULL", "A HULL ON THE SHINGLE", "THE FALLEN SLIP"}};
  std::vector<std::string> titles = {"RAISE IT AGAIN", "HAMMERS AT DAWN"};
  for (const std::string& s : T[(size_t)c.variant]) titles.push_back(s);
  P["title"] = titles[(size_t)d.below((int)titles.size())];
}

bool choose(const JobCtx& w, uint64_t dice, JobCtx& out) {
  Dice d(dice);
  out = w;
  std::vector<int> works;
  switch ((Role)w.trade) {
    case Role::Farmer: works = {W_BRIDGE, W_MILL}; break;
    case Role::Merchant: if (w.tier >= 1) works.push_back(W_BRIDGE); if (w.port) works.push_back(W_SLIP); break;
    case Role::Smith: if (w.tier <= 1) works.push_back(W_MILL); break;
    case Role::Jarl: if (w.tier >= 1) { works.push_back(W_WALL); works.push_back(W_ROOF); } break;
    case Role::Guard: if (w.tier >= 1) works.push_back(W_WALL); break;
    case Role::Innkeeper: if (w.tier <= 1) works.push_back(W_ROOF); break;
    case Role::Priest: if (w.tier <= 1) works.push_back(W_SHRINE); break;
    case Role::Fisher: if (w.coast) works.push_back(W_PIER); if (w.port) works.push_back(W_SLIP); break;
    default: break;
  }
  if (works.empty()) return false;
  out.variant = works[(size_t)d.below((int)works.size())];
  // trouble near: always at a wall; never at the shrine; else with danger, or one time in two
  out.twist = out.variant == W_WALL ? 1 : out.variant == W_SHRINE ? 0 : (w.danger || d.chance(50)) ? 1 : 0;
  return true;
}

JobCtx walk(Role t, int work, int trouble, int season, int tier = 0, bool coast = false, bool port = false) {
  JobCtx c;
  c.trade = (int)t; c.variant = work; c.twist = trouble; c.season = season; c.tier = tier; c.coast = coast; c.port = port;
  return c;
}

}  // namespace

const KindDef& kindMend() {
  static const KindDef K = [] {
    KindDef k;
    k.id = "mend";
    k.title = "MEND THE WORKS";
    k.lead = "NEEDS HANDS TO MEND THE WORKS";
    k.body = kBody;
    k.variants = {"bridge", "mill", "wall", "roof", "shrine", "pier", "slip"};
    k.twists = {"calm", "trouble"};
    k.trades = {(int)Role::Farmer, (int)Role::Merchant, (int)Role::Smith, (int)Role::Jarl, (int)Role::Guard, (int)Role::Innkeeper, (int)Role::Priest, (int)Role::Fisher};
    k.choose = choose;
    k.prep = prep;
    k.walks = {walk(Role::Farmer, W_BRIDGE, 1, 0), walk(Role::Smith, W_MILL, 0, 2, 1), walk(Role::Guard, W_WALL, 1, 1, 2), walk(Role::Innkeeper, W_ROOF, 1, 3),
               walk(Role::Priest, W_SHRINE, 0, 3), walk(Role::Fisher, W_PIER, 1, 2, 0, true), walk(Role::Merchant, W_SLIP, 0, 3, 2, true, true)};
    return k;
  }();
  return K;
}

}  // namespace radiant
}  // namespace story
