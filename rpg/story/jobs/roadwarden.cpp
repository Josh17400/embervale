// P1b radiant job kind 1: THE ROAD WARDEN (content: jobs_a.md section 1). JOBS lane.
//
// A hand cart (the driver between its shafts) must reach the next town. The driver is a person of the story who walks (drives) with the player; the gang
// (or the winter wolves, or the war's raiders) strikes as the wagon leaves the fields of home; one time in three the
// STRANDED CART of the buyer's own town lies by the road near the end, two raiders still at its load: help (a short
// fight, then walk its hurt driver in: a purse, the buyer's thanks, the giver sulks) or press on (the speed holds; the
// driver remembers). The buyer of the cargo (its trade follows the cargo) pays on arrival, more for good time.
// Variants by the giver's voice: merchant, innkeeper (short hauls of ale and herring), lord (the lord's own road),
// harbour (a merchant or a fisher of a port: landed cargo going inland).
#include "rpg/story/radiant.h"
#include <map>
#include <string>
#include <vector>
#include "rpg/sim/world.h"

namespace story {
namespace radiant {

namespace {

const char* const kBody = R"DSL(
title [[=title]]
role giver giver
role home site home
role dest site town near
role driver person [[male|female]] at home
role buyer person [[male|female]] at dest
?tw_stranded role rival person [[male|female]] at dest
var helped 0
var cartload [[=load]]

stage start
  talk giver
!danger  say "[[=reason]] {DEST} LIES {DEST.DIR}. [[=ask]] [[=pay]]"
?danger  say "[[=reason]] {DEST} LIES {DEST.DIR}. [[=ask]] THE GANGS ON THAT ROAD ARE NO JOKE THIS YEAR."
  opt "[[I'LL WALK WITH THE CART.|I'LL GUARD YOUR CART.|I'LL SEE IT THROUGH TO {DEST}.]]" -> meet
  opt "[[NOT THIS TIME.|NOT TODAY. FIND ANOTHER SWORD.|I'M BOUND ELSEWHERE.]]" -> decline

stage meet
  talk driver
  say "[[FINALLY. MY ARMS ARE ROTTING ON THESE SHAFTS.|STAY CLOSE. I DON'T LIKE THE HEDGES ON THAT ROAD.|WE ROLL NOW. KEEP YOUR BLADE LOOSE.]] [[=cargoline]]"
  journal "{GIVER} HIRED YOU TO GUARD THE [[=cargo]] CART FROM {HOME} TO {DEST}. {DRIVER} WAITS AT THE GATE BETWEEN THE CART'S SHAFTS."
  opt "[[LEAD ON.|ROLL OUT. I'M BEHIND YOU.|LET'S GO.]]" -> leg

stage leg
  goal escort driver to dest wagon
  say "[[=roadline]]"
  journal "WALK BESIDE {DRIVER}'S CART TO {DEST}, {DEST.DIR}. IF THE DRIVER DIES, THE JOB IS LOST. [[=foeword]] WATCH THE ROAD."
  do follow driver
  do ambush [[=foe]] [[=wave]]
?tw_stranded  then stranded
!tw_stranded  then check
  else lost

?tw_stranded stage stranded
?tw_stranded   talk driver
?tw_stranded   say "LOOK THERE. A {DEST} CART ON ITS SIDE, THE DRIVER HURT, TWO RAIDERS STILL PICKING AT THE LOAD. THEY UNDERCUT US AT {HOME}, THAT LOT. WE COULD HELP. OR WE COULD BE EARLY."
?tw_stranded   opt "[[WE HELP THEM.|DRIVER OR NOT, WE HELP.]]" -> help
?tw_stranded   opt "[[WE PRESS ON.|NOT OUR CART. DRIVE ON.]]" -> press

?tw_stranded stage help
?tw_stranded   goal escort rival to dest walk
?tw_stranded   say "THE RAIDERS DROP THE SACKS AND TURN ON YOU."
?tw_stranded   journal "DRIVE OFF THE RAIDERS AT THE OVERTURNED CART AND WALK {RIVAL}, ITS HURT DRIVER, INTO {DEST}."
?tw_stranded   do set helped 1
?tw_stranded   do ambush bandit 2
?tw_stranded   do follow rival
?tw_stranded   do stay driver
?tw_stranded   then helped
?tw_stranded   else rival_lost

?tw_stranded stage helped
?tw_stranded   talk rival
?tw_stranded   say "I HAD A PURSE ON ME. YOU'D BETTER HAVE IT, BEFORE SOME OTHER THIEF DOES. AND TELL {DEST} WHO PULLED ME OUT OF THE MUD."
?tw_stranded   journal "{RIVAL} IS SAFE IN {DEST}. NOW SEE {DRIVER}'S CART TO {BUYER}."
?tw_stranded   do reward small 60
?tw_stranded   do remember giver "YOU SAVED A {DEST} DRIVER ON MY ROAD? I'LL SULK FOR A DAY. THEN I'LL BUY YOU A DRINK."
?tw_stranded   do remember rival "YOU PULLED ME OUT OF THE MUD. I DON'T FORGET THAT."
?tw_stranded   opt "[[GET THAT LEG SEEN TO.|KEEP THE PURSE CLOSER NEXT TIME.]]" -> check_h

?tw_stranded stage rival_lost
?tw_stranded   talk driver
?tw_stranded   say "TOO LATE FOR THAT ONE. WE TRIED. THAT'S MORE THAN THE ROAD USUALLY GETS."
?tw_stranded   journal "THE {DEST} DRIVER DIED OF THE WOUNDS. SEE {DRIVER}'S CART TO {BUYER} IN {DEST}."
?tw_stranded   do remember giver "{DEST} LOST A DRIVER ON THAT ROAD, THOUGH YOU TRIED. I'M SORRY FOR IT."
?tw_stranded   opt "[[DRIVE ON.|LET'S FINISH THIS.]]" -> check_h

?tw_stranded stage press
?tw_stranded   talk driver
?tw_stranded   say "...I'LL REMEMBER THAT. DRIVE ON, THEN. WE'LL BE EARLY, AT LEAST."
?tw_stranded   journal "YOU LEFT THE {DEST} CART TO ITS RAIDERS. SEE {DRIVER}'S CART TO {BUYER} IN {DEST}."
?tw_stranded   do remember giver "{DEST} LOST A DRIVER ON THAT ROAD. I'M SORRY. I'M ALSO RELIEVED."
?tw_stranded   do remember driver "WE LEFT THAT DRIVER IN THE MUD. I STILL THINK ABOUT IT."
?tw_stranded   opt "[[DRIVE ON.|EYES ON THE ROAD.]]" -> check

stage check
  goal escort driver to dest wagon
  journal "BRING {DRIVER} AND THE [[=cargo]] TO {BUYER}, THE [[=buyerjob]] IN {DEST}, WHO PAYS ON ARRIVAL."
  do follow driver
  then buyer
  else lost

?tw_stranded stage check_h
?tw_stranded goal escort driver to dest wagon
?tw_stranded journal "BRING {DRIVER} AND THE [[=cargo]] TO {BUYER}, THE [[=buyerjob]] IN {DEST}, WHO PAYS ON ARRIVAL."
?tw_stranded do follow driver
?tw_stranded then buyer_h
?tw_stranded else lost

stage buyer
  talk buyer
  say "[[=arrive]] [[SMOKE FROM THE CHIMNEYS AND A WHOLE CART. A GOOD DAY.|NOT A WHEEL MISSING. GOOD.]]"
  journal "HAND THE [[=cargo]] CART TO {BUYER} IN {DEST}."
  do stay driver
  opt "[[HERE'S YOUR LOAD.|SIGN FOR IT, THEN.|IT'S ALL THERE.]]" -> paid
  opt "WE MADE GOOD TIME, TOO." if nodays 4 -> paid_fast

?tw_stranded stage buyer_h
?tw_stranded talk buyer
?tw_stranded say "[[=arrive]] AND YOU HELPED OUR DRIVER ON THE ROAD? THE FIRST DRINK IN {DEST} IS MINE."
?tw_stranded journal "HAND THE [[=cargo]] CART TO {BUYER} IN {DEST}."
?tw_stranded do stay driver
?tw_stranded opt "[[HERE'S YOUR LOAD.|SIGN FOR IT, THEN.|IT'S ALL THERE.]]" -> paid
?tw_stranded opt "WE MADE GOOD TIME, TOO." if nodays 4 -> paid_fast

stage paid
  end success
  say "[[=payline]]"
  journal "THE [[=cargo]] REACHED {DEST} WHOLE, AND {DRIVER} WITH IT. {BUYER} PAID ON ARRIVAL."
  do reward fair 115
  do census dest stock [[=good]] 8
  do census dest mood 3
  do remember buyer "[[=buyermem]]"
  do remember driver "[[=drivermem]]"
  do fact "A WARDEN WALKED THE [[=cargo]] CART FROM {HOME} TO {DEST}."

stage paid_fast
  end success
  say "EARLY, TOO? [[=payline]] AND A LITTLE MORE FOR THE SPEED."
  journal "THE [[=cargo]] REACHED {DEST} EARLY AND WHOLE. {BUYER} PAID THE SPEED BONUS."
  do reward fair 140
  do census dest stock [[=good]] 10
  do census dest mood 4
  do remember buyer "[[=buyermem]]"
  do remember driver "[[=drivermem]]"
  do fact "A WARDEN BROUGHT THE [[=cargo]] CART TO {DEST} AHEAD OF TIME."

stage lost
  end fail
  say "{DRIVER} DIED ON THE ROAD TO {DEST}, AND THE [[=cargo]] WAS LEFT TO THE DITCH AND THE CROWS."
  journal "{DRIVER} DIED ON THE ROAD TO {DEST}. THE JOB IS LOST."
  do remember giver "THE ROAD TOOK ANOTHER ONE. LEAVE ME BE A WHILE."
  do census home mood -3

stage decline
  end fail
  journal "YOU TURNED DOWN {GIVER}'S CART."
)DSL";

// load: what the hand cart shows (render.cpp paintHandcart, read from the instance var cartload): 1 sacks, 2 crates, 3 barrels
struct Cargo { const char* name; const char* good; const char* buyer; const char* arrive; int load; };
const Cargo kCargo[] = {
    {"SALT", "produce", "INNKEEPER", "SALT AT LAST. THE PICKLES WILL LIVE, AND SO WILL I.", 1},
    {"SEED GRAIN", "grain", "MERCHANT", "SEED GRAIN. NOW WE HAVE A SPRING.", 1},
    {"CLOTH", "cloth", "MERCHANT", "DYED CLOTH. THE WEDDING IS SAVED.", 2},
    {"SMALL BEER", "produce", "INNKEEPER", "BEER AT LAST. THE WELLS ARE FOUL THIS MONTH.", 3},
    {"SCYTHES", "tools", "FARMER", "SCYTHES. THE BARLEY STILL STANDS, THEN.", 2},
    {"CANDLES", "produce", "PRIEST", "CANDLES! THE TEMPLE HAS BEEN DARK A WEEK.", 2},
    {"HERRING", "fish", "INNKEEPER", "HERRING, AND STILL COLD. THAT'S LUCK.", 3},
    {"WOOL", "wool", "MERCHANT", "WOOL. THE LOOMS HAVE SAT STILL TOO LONG.", 1},
    {"BELL METAL", "ingot", "PRIEST", "BELL METAL. THE TOWER SINGS AGAIN.", 2},
    {"GEESE", "livestock", "INNKEEPER", "THE GEESE. ALIVE, ANGRY, PERFECT.", 2},
};
enum { C_SALT, C_SEED, C_CLOTH, C_BEER, C_SCYTHES, C_CANDLES, C_HERRING, C_WOOL, C_BELL, C_GEESE };
enum { V_MERCHANT, V_INNKEEPER, V_JARL, V_HARBOUR };

void prep(const JobCtx& c, uint64_t seed, std::map<std::string, std::string>& P) {
  Dice d(seed);
  // the cargo: by the giver (and the season for a merchant's)
  std::vector<int> pool;
  switch (c.variant) {
    case V_INNKEEPER: pool = {C_BEER, C_HERRING, C_SALT, C_GEESE}; break;
    case V_HARBOUR: pool = {C_SALT, C_HERRING, C_CLOTH, C_BELL}; break;
    case V_JARL: pool = {C_SEED, C_WOOL, C_SALT, C_SCYTHES}; break;
    default:
      switch (c.season) {
        case 0: pool = {C_CLOTH, C_SCYTHES, C_SEED, C_CANDLES}; break;   // the thaw rush
        case 1: pool = {C_HERRING, C_GEESE, C_CLOTH, C_BEER}; break;     // the feasts
        case 2: pool = {C_SEED, C_WOOL, C_SALT, C_BELL}; break;          // the harvest
        default: pool = {C_SALT, C_CANDLES, C_WOOL, C_HERRING}; break;   // salt, candles, wool
      }
  }
  if (c.war) pool = {C_SALT, C_SEED, C_WOOL, C_SCYTHES};   // what an army eats and wears
  const Cargo& K = kCargo[pool[(size_t)d.below((int)pool.size())]];
  const std::string cargo = K.name;
  const bool geese = cargo == "GEESE";
  P["cargo"] = cargo;
  P["load"] = std::to_string(K.load);
  P["good"] = K.good;
  P["buyerjob"] = K.buyer;
  P["arrive"] = K.arrive;
  // the ambushers: wolves follow wagons in winter; raiders in a war; else the road's gang
  const bool wolves = c.season == 3 && !c.war;
  P["foe"] = wolves ? "wolf" : "bandit";
  P["foeword"] = wolves ? "WOLVES" : c.war ? "RAIDERS" : "BANDITS";
  P["wave"] = std::to_string(3 + (c.tier >= 1 ? 1 : 0) + (c.danger ? 1 : 0) + (c.war ? 1 : 0));
  // the reason, by who asks and when
  std::vector<std::string> R;
  if (c.war) R.push_back("THE ARMY NEEDS " + cargo + " AND {DEST} HOLDS THE STORE. THE RAIDERS KNOW THE ROAD AS WELL AS I DO.");
  if (geese) R.push_back("A DOZEN CRATED GEESE FOR THE FEAST AT {DEST}. THEY BITE. THE ROAD HAS BANDITS. I CANNOT SAY WHICH IS WORSE.");
  switch (c.variant) {
    case V_MERCHANT:
      R.push_back("TWO DRIVERS DID NOT COME BACK THIS MONTH. THE NEXT CART TO {DEST} GOES WITH A WARDEN OR NOT AT ALL.");
      R.push_back("MARKET DAY AT {DEST} IS COMING. THE LOAD FETCHES DOUBLE IF IT ARRIVES EARLY, AND NOTHING IF IT NEVER ARRIVES.");
      R.push_back("THE TOLL-MAN ON THE ROAD TO {DEST} IS DEAD AND NOBODY WATCHES IT NOW. THE " + cargo + " CART HAS TO CROSS IT.");
      break;
    case V_INNKEEPER:
      R.push_back("I SELL ALE TO EVERY DRIVER ON THE {DEST} ROAD. DEAD DRIVERS DON'T DRINK. WALK THE " + cargo + " CART THROUGH FOR ME.");
      R.push_back("MY COUSIN KEEPS THE INN AT {DEST} AND HAS NOT SEEN A CART OF MINE IN A MONTH. THE " + cargo + " GOES TOMORROW.");
      break;
    case V_JARL:
      R.push_back("THE ROAD TO {DEST} IS THE LORD'S OWN, AND SOMEONE IS ROBBING ON IT. MY " + cargo + " GOES THROUGH WITH A WARDEN.");
      R.push_back("THE TOLL-MAN ON THE ROAD TO {DEST} IS DEAD AND NOBODY WATCHES IT NOW. THE " + cargo + " CART HAS TO CROSS IT.");
      break;
    case V_HARBOUR:
      R.push_back("THE SHIP IS IN AND THE HOLD IS FULL. {DEST} HAS PAID FOR THE " + cargo + " AND IT HAS TO ARRIVE WHOLE.");
      R.push_back("THE " + cargo + " CAME OFF THE BOAT THIS MORNING AND GOES INLAND TO {DEST}. THE ROAD UP FROM THE QUAY IS BAD.");
      break;
  }
  if (c.season == 2) R.push_back("THE LAST CART OF THE HARVEST GOES TO {DEST} THIS WEEK, AND THAT ROAD IS NO PLACE TO WALK ALONE.");
  if (c.season == 0) R.push_back("THE THAW IS HERE AND {DEST} IS OUT OF " + cargo + ". THE CART IS LOADED AND THE DRIVER IS TERRIFIED.");
  if (wolves) R.push_back("WOLVES FOLLOW CARTS IN WINTER. I HAVE " + cargo + " FOR {DEST} AND NOBODY TO WALK THE ROAD BESIDE IT.");
  P["reason"] = R[(size_t)d.below((int)R.size())];
  P["ask"] = d.pick({"WALK BESIDE THE CART ALL THE WAY. DON'T LET IT TURN BACK.", "KEEP THE DRIVER AND THE LOAD ALIVE TO {DEST}.",
                     "BE THE SPEAR AT THE TAIL OF THE CART."});
  P["pay"] = d.pick({"THE BUYER PAYS ON ARRIVAL, AND PAYS WELL.", "YOU'RE PAID WHEN IT ARRIVES. MORE IF IT'S EARLY.",
                     "THE BUYER AT {DEST} PAYS THE WARDEN ON ARRIVAL."});
  // the title (none over 22 with the longest names)
  std::vector<std::string> T = {"THE LONG HAUL", "BEHIND THE CART", "SAFE TO MARKET", "THE " + cargo + " CART", cargo + " ON THE ROAD",
                                "THE ROAD WARDEN"};
  if (c.variant == V_JARL) T.push_back("THE LORD'S ROAD");
  if (wolves) T.push_back("WHEELS AND WOLVES");
  if (c.war) T.push_back("AN ARMY'S CART");
  P["title"] = T[(size_t)d.below((int)T.size())];
  P["cargoline"] = geese ? "TWELVE GEESE IN CRATES, AND EVERY ONE KNOWS WHICH OF US IS SOFT." : "THE " + cargo + " IS LASHED DOWN. THE AXLE IS GREASED. THAT'S ALL I CAN PROMISE.";
  P["roadline"] = geese ? d.pick({"ONE GOOSE FOR EVERY BANDIT. SERIOUSLY.", "THEY HONKED OFF A WOLF LAST YEAR, THESE GEESE."})
                        : d.pick({"MY FATHER DROVE THIS ROAD FORTY YEARS.", "THE LEFT WHEEL SQUEALS. IT'S LUCKY.", "I'VE PULLED THIS CART SINCE I WAS TEN.",
                                  "SEE THAT CROW? HE'S SEEN US TOO.", "IF WE'RE ROBBED, I STAND BEHIND YOU.", "IF I STOP, PUSH. IF I RUN, PUSH HARDER."});
  P["payline"] = d.pick({"PAID AS PROMISED, AND A DRINK BESIDES.", "HERE'S THE WARDEN'S PAY. YOU EARNED IT ON THAT ROAD.", "COUNT IT. THEN HAVE A DRINK ON ME."});
  P["buyermem"] = d.pick({"YOU BROUGHT THE " + cargo + " CART IN WHOLE. ANY TIME.", "THE WARDEN WITH THE " + cargo + ". I REMEMBER YOU."});
  P["drivermem"] = d.pick({"STILL HAVE ALL MY FINGERS. THANKS TO YOU.", "THAT'S ONE GANG WE WON'T MEET TWICE."});
}

bool choose(const JobCtx& w, uint64_t dice, JobCtx& out) {
  Dice d(dice);
  out = w;
  switch ((Role)w.trade) {
    case Role::Merchant: out.variant = w.port && d.chance(50) ? V_HARBOUR : V_MERCHANT; break;
    case Role::Innkeeper: out.variant = V_INNKEEPER; break;
    case Role::Jarl: out.variant = V_JARL; break;
    case Role::Fisher: if (!w.port) return false; out.variant = V_HARBOUR; break;
    default: return false;
  }
  out.twist = d.below(3) == 0 ? 1 : 0;
  return true;
}

JobCtx walk(Role t, int season, int variant, int twist, bool war = false, bool port = false, bool danger = false) {
  JobCtx c;
  c.trade = (int)t; c.season = season; c.variant = variant; c.twist = twist; c.war = war; c.port = port; c.danger = danger;
  return c;
}

}  // namespace

const KindDef& kindRoadWarden() {
  static const KindDef K = [] {
    KindDef k;
    k.id = "roadwarden";
    k.title = "THE ROAD WARDEN";
    k.lead = "NEEDS A WARDEN FOR A CART";
    k.body = kBody;
    k.variants = {"merchant", "innkeeper", "jarl", "harbour"};
    k.twists = {"none", "stranded"};
    k.trades = {(int)Role::Merchant, (int)Role::Innkeeper, (int)Role::Jarl, (int)Role::Fisher};
    k.choose = choose;
    k.prep = prep;
    k.walks = {walk(Role::Merchant, 0, V_MERCHANT, 0), walk(Role::Innkeeper, 3, V_INNKEEPER, 1),
               walk(Role::Jarl, 2, V_JARL, 1, true, false, true), walk(Role::Fisher, 1, V_HARBOUR, 0, false, true)};
    return k;
  }();
  return K;
}

}  // namespace radiant
}  // namespace story
