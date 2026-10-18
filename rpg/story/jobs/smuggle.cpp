// P1b radiant job kind 8: THE NIGHT LANDING (content: jobs_b.md section 3; judge notes resolved). JOBS lane.
//
// The same trade seen from both sides; a ring lands boats on a coast (SEA) or leads mules over a pass (HILL).
//   CATCH (guard, merchant, lord): a smugglers' camp. The captain speaks first (SITE-TALK) by the goods: salt and
//     spirit (rent on my own shore), grain (folk eating bark), war-steel (treason: no let-go road), mist-glass (SEA only:
//     it sings at night). Arrest them (a fight; the captain falls; a crate as proof), let them go with one crate as proof
//     (the crew must believe you: a name they have heard, fame 4; else they fight), take the bribe (the giver's spotter
//     sees the boat or the mules go free: no fee), or, for grain, ship it with them and call the camp empty.
//   RUN (merchant, innkeeper, villager, farmer, fisher): a cache in a cave; whatever lairs there wants it too; carry it to
//     the buyer in another town, who pays on delivery (judge: every reason names that buyer; no BY DAWN, no gate check).
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
{?v_catch
role den site camp near
role capt person male in den
}
{?v_run
role cache site cave near
role dest site town near
role buyer person [[male|female]] at dest
}
{?v_catch
{?x_grain
role hungry site village near
role taker person [[male|female]] at hungry
}
}

stage start
  talk giver
  say "[[=reason]] [[=ask]] [[THE PAY IS FAIR.|I PAY WELL FOR QUIET WORK.|THERE'S GOOD COIN IN IT.]]"
  opt "[[I'LL DEAL WITH IT.|CONSIDER IT DONE.|I'LL GO TONIGHT.]]" -> go
  opt "[[NOT MY BUSINESS.|NOT TODAY.|FIND SOMEONE ELSE.]]" -> decline

{?v_catch
stage go
  goal goto den
  journal "[[=goods]] IS SMUGGLED THROUGH {DEN}, {DEN.DIR}. BRING BACK A CRATE AS PROOF AND REPORT TO {GIVER}."
  then parley

stage parley
  talk capt
  say "[[=parley]]"
  journal "{CAPT}, THE SMUGGLERS' CAPTAIN AT {DEN}, WANTS A WORD BEFORE ANY BLADE IS DRAWN."
  opt "[[ARREST THEM.|YOU'RE ALL UNDER ARREST.]]" -> arrest
{!x_steel
  opt "TAKE ONE CRATE AND GO. NO CREW." check fame 4 -> letgo else arrest
}
  opt "[[TAKE THE BRIBE AND WALK.|HOW MUCH, THEN?]]" -> bribe
{?x_grain
  opt "I'LL CARRY IT TO THE HUNGRY MYSELF." -> ship
}

stage arrest
  goal slay capt
  say "[[THEN WE DO IT THE HARD WAY.|YOU'LL REGRET THIS.]]"
  journal "THE RING AT {DEN} WILL NOT COME QUIETLY. {CAPT} MUST FALL."
  do hostile capt
  then seized

stage seized
  goal goto den
  journal "THE RING IS BROKEN. TAKE A CRATE OF [[=goods]] FROM {DEN} TO {GIVER} AS PROOF."
  do give "A CRATE OF [[=goods]]" [[=icon]] heavy
  then report_arrest

stage report_arrest
  talk giver
  say "[[=arrestline]]"
  journal "BRING THE CRATE OF [[=goods]] TO {GIVER} IN {HOME}."
  opt "[[THE RING IS BROKEN.|{CAPT} IS DEAD. HERE'S THE PROOF.]]" -> arrest_end

{!x_steel
stage letgo
  talk capt
  say "[[=letline]]"
  journal "THE CREW AT {DEN} STOOD DOWN AND LEFT. ONE CRATE OF [[=goods]] IS YOURS AS PROOF."
  do give "A CRATE OF [[=goods]]" [[=icon]] heavy
  do calm den
  do remember capt "YOU LET US GO. A SAFE COVE IS A SAFE COVE. ASK FOR ME."
  opt "[[GO, BEFORE I CHANGE MY MIND.|ONE CRATE. GO.]]" -> report_let

stage report_let
  talk giver
  say "ONE CRATE AND NO CREW. THE CARGO IS OURS, AT LEAST, AND THE CREW GONE. I SHALL ASK NO MORE THAN THAT."
  journal "BRING THE CRATE OF [[=goods]] TO {GIVER} IN {HOME}."
  opt "[[THEY WON'T BE BACK.|ONE CRATE, NO CREW.]]" -> let_end
}

stage bribe
  talk capt
  say "SENSIBLE. HERE: FORTY IN SILVER, AND WE WERE NEVER HERE. THE CRATES COME WITH US."
  journal "YOU TOOK {CAPT}'S BRIBE. THE CREW AND THE CARGO ARE GONE. {GIVER} WILL HEAR OF IT."
  do gold 40
  do deed bribed
  opt "[[NEVER HERE.|PLEASURE DOING BUSINESS.]]" -> report_bribe

stage report_bribe
  talk giver
  say "MY SPOTTER SAW [[=freeline]] FREE. YOUR PURSE IS FATTER THAN YOUR WORD. THERE IS NO FEE FOR YOU HERE."
  journal "{GIVER}'S SPOTTER SAW THE SMUGGLERS GO FREE."
  opt "[[...|IT WASN'T LIKE THAT.]]" -> bribe_end

{?x_grain
stage ship
  goal goto hungry
  journal "CARRY THE SMUGGLED GRAIN TO {TAKER} IN {HUNGRY}, WHERE THEY ARE EATING BARK."
  do give "SACKS OF GRAIN" sheaf heavy
  do remember capt "YOU CARRIED OUR GRAIN TO THE HUNGRY YOURSELF. I DON'T UNDERSTAND YOU. I LIKE YOU."
  then hungry_door

stage hungry_door
  talk taker
  say "GRAIN? REAL GRAIN? THIS TOWN OWES YOU A QUIET DEBT. BREAD TOMORROW. REMEMBER IT, AND WE WILL TOO."
  journal "GIVE THE GRAIN TO {TAKER} IN {HUNGRY}, THEN TELL {GIVER} WHAT YOU FOUND."
  do take "SACKS OF GRAIN"
  opt "[[EAT WELL.|SAY NOTHING OF ME.]]" -> report_ship

stage report_ship
  talk giver
  say "EMPTY? THE CART TRACKS SAY OTHERWISE. ...I WILL WRITE EMPTY IN THE LEDGER. NO FEE FOR AN EMPTY CAMP."
  journal "TELL {GIVER} THE CAMP AT {DEN} WAS EMPTY."
  opt "[[IT WAS EMPTY.|NOTHING THERE.]]" -> ship_end
}
}

{?v_run
stage go
  goal goto cache
  journal "A CARGO OF [[=goods]] WAITS AT {CACHE}, {CACHE.DIR}. CARRY IT TO {BUYER} AT {DEST}, WHO PAYS ON DELIVERY."
  then rivals

stage rivals
  goal kill 3 any in cache
  journal "SOMETHING HAS MOVED INTO {CACHE} AND SITS ON THE CARGO. CLEAR IT OUT."
  then carry

stage carry
  goal goto dest
  journal "CARRY [[=load]] TO {BUYER} AT {DEST}, {DEST.DIR}. KEEP IT DRY, KEEP IT QUIET."
  do give "[[=load]]" [[=icon]] heavy
  then deliver

stage deliver
  talk buyer
  say "[[=buyerline]]"
  journal "HAND [[=load]] TO {BUYER} AT {DEST}."
  opt "[[COUNT IT.|ALL THERE.|NOBODY SAW ME.]]" -> run_end
}

{?v_catch
stage arrest_end
  end success
  say "YOUR PAY, BY ORDER OF THE CROWN. AND A LITTLE OF MY OWN."
  journal "THE SMUGGLING RING AT {DEN} IS BROKEN. {CAPT} IS DEAD, THE CARGO SEIZED."
  do take "A CRATE OF [[=goods]]"
  do reward fair [[=arrestpay]]
  do deed guard
  do remember giver "THE RING AT {DEN} IS BROKEN. THE TOLLS COME IN AGAIN."
[[=arrestfx]]

{!x_steel
stage let_end
  end success
  journal "THE CREW AT {DEN} LEFT WITH THEIR LIVES. {GIVER} HAS ONE CRATE OF [[=goods]] AS PROOF."
  do take "A CRATE OF [[=goods]]"
  do reward fair 60
  do rumour
  do remember giver "ONE CRATE, NO CREW. THE COVE IS QUIET. GOOD ENOUGH."
[[=letfx]]
}

stage bribe_end
  end fail
  journal "YOU TOOK THE SMUGGLERS' BRIBE. {GIVER} KNOWS, AND PAYS NOTHING."
  do remember giver "YOUR PURSE IS FATTER THAN YOUR WORD. DON'T ASK ME FOR WORK."

{?x_grain
stage ship_end
  end success
  journal "YOU CARRIED THE SMUGGLED GRAIN TO THE HUNGRY OF {HUNGRY} AND CALLED THE CAMP EMPTY. {GIVER} LOOKED AWAY."
  do xp 70
  do deed mercy
  do deed lied
  do rumour
  do fact "A STARVING TOWN, {HUNGRY}, WAS FED WITH SMUGGLED GRAIN."
}
}

{?v_run
stage run_end
  end success
  say "[[=buyerpay]]"
  journal "[[=load]] REACHED {BUYER} AT {DEST}. NOBODY ASKED ANY QUESTIONS."
  do take "[[=load]]"
  do reward fair [[=runpay]]
  do remember buyer "THE QUIET CARRIER. COME AGAIN."
[[=runfx]]
}

stage decline
  end fail
  journal "YOU TURNED DOWN {GIVER}'S NIGHT WORK."
)DSL";

enum { V_CATCH, V_RUN };
enum { G_SALT, G_SPIRIT, G_STEEL, G_GRAIN, G_GLASS };

void prep(const JobCtx& c, uint64_t seed, std::map<std::string, std::string>& P) {
  Dice d(seed);
  const bool sea = c.coast;
  const int good = c.extra == 1 ? G_STEEL : c.extra == 2 ? G_GRAIN : c.extra == 3 ? G_GLASS : (c.twist == 1 ? G_SPIRIT : G_SALT);
  static const char* names[] = {"SALT", "SPIRIT", "WAR-STEEL", "GRAIN", "MIST-GLASS"};
  static const char* icons[] = {"seeds", "potion", "sword", "sheaf", "gem"};
  const std::string goods = names[good];
  P["goods"] = goods; P["icon"] = icons[good];
  const Role t = (Role)c.trade;
  std::vector<std::string> R, T;
  if (c.variant == V_CATCH) {
    T = {sea ? "THE NIGHT LANDING" : "THE NIGHT CROSSING", "BREAK THE RING", "DARK LANTERNS", "THE LEDGER LIES"};
    if (good == G_SALT || good == G_SPIRIT) T.push_back("THE MISSING TOLL");
    switch (good) {
      case G_STEEL: R = {"WITH THE REALM AT WAR, EVERY CRATE OF WAR-STEEL LEAVING {DEN} ARMS THE ENEMY. THAT IS NOT TRADE. IT IS TREASON."}; break;
      case G_GRAIN: R = {"OUR GRANARY IS HALF EMPTY, AND CARTS OF GRAIN LEAVE {DEN} BY NIGHT FOR OTHER TOWNS. STOP IT, OR EXPLAIN IT TO ME."}; break;
      case G_GLASS: R = {"THE CROWN FORBIDS MIST-GLASS. SOMEONE AT {DEN} GATHERS IT OFF THE ROCKS WHEN THE FOG LIFTS. THE LAST BUYER WENT QUIET."}; break;
      default:
        if (t == Role::Merchant) R = {"I PAY THE TOLL ON EVERY LOAD OF " + goods + ". SOMEONE AT {DEN} SELLS IT FOR HALF, AND I AM FINISHED UNLESS THEY ARE."};
        else if (t == Role::Jarl) R = {sea ? "THEY USE MY SHORE FOR A LANDING AND KEEP MY CUT. THE " + goods + " AT {DEN} MUST GO, AND THEY WITH IT."
                                          : "THEY USE MY PASS FOR A CROSSING AND KEEP MY CUT. THE " + goods + " AT {DEN} MUST GO, AND THEY WITH IT."};
        else R = {sea ? "SOMEONE LANDS " + goods + " AT {DEN} ON DARK NIGHTS, AND NOT A COIN OF TOLL REACHES THE TREASURY."
                      : "A MULE TRAIN OF " + goods + " CROSSES THE PASS BY {DEN} ON DARK NIGHTS. NOT A COIN OF TOLL REACHES THE TREASURY."};
        break;
    }
    P["ask"] = (t == Role::Merchant || t == Role::Jarl) ? d.pick({"HEAR THEM OUT IF YOU MUST, THEN DECIDE.", "BRING BACK A CRATE. THE CAPTAIN IS A BONUS."})
                                                        : "BRING BACK A CRATE. THE CAPTAIN IS A BONUS.";
    switch (good) {
      case G_GRAIN: P["parley"] = "IT IS GRAIN, NOT GOLD. WE CARRY IT TO FOLK EATING BARK. ARREST US AND THEY STARVE."; break;
      case G_STEEL: P["parley"] = "SIX CRATES. A BUYER ACROSS THE LINE. I DO NOT ASK WHOSE WAR IT IS. THE PAY IS BETTER THAN THE CAUSE."; break;
      case G_GLASS: P["parley"] = "I DID NOT FIND IT. IT FOUND ME. THE MIST LEAVES IT ON THE ROCKS. SELL IT, AND IT SINGS AT NIGHT. DO YOU WANT IT SINGING IN YOUR TOWN?"; break;
      default: P["parley"] = std::string("A FEW BARRELS. NOBODY BLEEDS FOR THEM. THE CROWN CALLS IT A CRIME. I CALL IT RENT ON MY OWN ") + (sea ? "SHORE." : "HILL."); break;
    }
    P["letline"] = sea ? "ONE CRATE, AND WE TAKE THE BOAT OUT WITH THE REST. DONE. THERE IS A COVE TO THE NORTH NOBODY WATCHES. FOR LATER."
                       : "ONE CRATE, AND WE LEAD THE MULES BACK OVER THE PASS. DONE. THERE IS A PASS-HOUSE NOBODY WATCHES. FOR LATER.";
    P["freeline"] = sea ? "THE BOAT SAIL" : "THE MULES WALK";
    std::string al;
    switch (t) {
      case Role::Merchant: al = "THE CAPTAIN FELL? HALF PRICE IS OVER, THEN. I BLESS YOU."; break;
      case Role::Jarl: al = "THE CAPTAIN FELL? THE ROAD IS MINE AGAIN. TAKE THIS."; break;
      default: al = "THE RING IS BROKEN. CARGO SEIZED. A CRATE IS PROOF, AND A CAPTAIN IS A CAGE."; break;
    }
    if (good == G_GLASS) al += " THE CRATE GOES TO THE VAULT. ...DOES IT HUM? IT HUMS.";
    P["arrestline"] = al;
    P["arrestpay"] = good == G_STEEL ? "130" : "110";
    P["arrestfx"] = good == G_GLASS ? "  do fact \"MIST-GLASS WAS SEIZED AT {DEN}. THE CROWN'S VAULT SINGS AT NIGHT.\"" : "";
    P["letfx"] = good == G_GRAIN ? "  do deed mercy" : good == G_GLASS ? "  do fact \"MIST-GLASS SLIPPED PAST {HOME}. A CELLAR SOMEWHERE HUMS.\"" : "";
  } else {
    T = {"A QUIET CARGO", "THE LONG WAY ROUND", "KEEP YOUR LANTERN LOW", "NOTHING TO DECLARE"};
    if (good == G_SALT || good == G_SPIRIT) T.push_back("NO QUESTIONS, NO TOLL");
    const int n = 3 + c.tier;
    static const char* cw[] = {"ZERO", "ONE", "TWO", "THREE", "FOUR", "FIVE", "SIX"};
    const std::string count = cw[n > 6 ? 6 : n];
    P["load"] = good == G_GRAIN ? count + " SACKS OF GRAIN" : good == G_SPIRIT ? count + " KEGS OF SPIRIT" : count + " CRATES OF " + goods;
    switch (t) {
      case Role::Innkeeper: R = {"MY COUSIN {BUYER} KEEPS THE INN AT {DEST} AND SERVES THE TAX-FREE " + goods + ". MY CARRIER TOOK SICK, AND THE LOAD WAITS AT {CACHE}."}; break;
      case Role::Fisher: R = {sea ? "THE SALTERS AT {DEST} PAY DOUBLE FOR " + goods + " THAT NEVER MET THE TOLLMAN. A BOAT LEFT A LOAD AT THE COVE BY {CACHE}."
                                  : "THE TOLLMAN TAKES A THIRD OF EVERYTHING WE SALT. {BUYER} AT {DEST} BUYS IT CLEAN. THE LOAD WAITS AT {CACHE}."}; break;
      case Role::Merchant: R = {"I NEED HANDS WITH NO CREST ON THEM. " + goods + " WAITS AT {CACHE}, AND {BUYER} AT {DEST} PAYS ON DELIVERY. ASK NO QUESTIONS."}; break;
      default:
        if (good == G_GRAIN) R = {"{DEST} IS STARVING, AND THE TOLL KEEPS THE GRAIN OUT. A LOAD WAITS AT {CACHE}. {BUYER} WILL TAKE IT IN."};
        else R = {"THE TOLLMAN AT THE PASS TAKES A THIRD OF EVERY SACK. {BUYER} AT {DEST} BUYS " + goods + " THAT NEVER MET HIM. IT WAITS AT {CACHE}."};
        break;
    }
    P["ask"] = d.pick({"GET IT TO {BUYER} UNSEEN.", "KEEP IT DRY, KEEP IT QUIET."});
    switch (good) {
      case G_GRAIN: P["buyerline"] = "THIS TOWN OWES YOU A QUIET DEBT. BREAD TOMORROW. REMEMBER IT."; P["buyerpay"] = "HERE. IT ISN'T MUCH. IT'S EVERYTHING THE TOWN COULD PUT IN THE HAT."; break;
      case G_GLASS: P["buyerline"] = "IT SINGS. CAN YOU HEAR IT? I CAN'T HEAR WHAT IT SAYS."; P["buyerpay"] = "SOMEBODY ELSE'S PROBLEM NOW. HERE'S YOUR PAY. GO, BEFORE IT STARTS AGAIN."; break;
      default: P["buyerline"] = "DID ANYONE SEE YOU? GOOD. COUNT IT IN FRONT OF ME. WE ARE HONEST THIEVES."; P["buyerpay"] = "PAID IN FULL, AND THE CARRIER'S CUT ON TOP."; break;
    }
    P["runpay"] = std::to_string(100 + 8 * n);
    P["runfx"] = good == G_GRAIN ? "  do deed charity" : good == G_GLASS ? "  do rumour" : "  do deed stole";
  }
  P["reason"] = R[(size_t)d.below((int)R.size())];
  P["title"] = T[(size_t)d.below((int)T.size())];
}

bool choose(const JobCtx& w, uint64_t dice, JobCtx& out) {
  Dice d(dice);
  out = w;
  const Role t = (Role)w.trade;
  bool catchOk = t == Role::Guard || t == Role::Merchant || t == Role::Jarl;
  bool runOk = t == Role::Merchant || t == Role::Innkeeper || t == Role::Villager || t == Role::Farmer || t == Role::Fisher;
  if (!catchOk && !runOk) return false;
  out.variant = catchOk && runOk ? (d.chance(50) ? V_CATCH : V_RUN) : catchOk ? V_CATCH : V_RUN;
  // the goods: war-steel at war (catch only), grain in winter want, mist-glass on a coast, else salt or spirit
  out.twist = d.chance(50) ? 1 : 0;   // spirit or salt
  out.extra = 0;
  const int roll = d.below(100);
  if (out.variant == V_CATCH && w.war && roll < 40) out.extra = 1;
  else if (w.season == 3 && roll < 55) out.extra = 2;
  else if (w.coast && roll >= 80) out.extra = 3;
  return true;
}

JobCtx walk(Role t, int variant, int extra, int twist, bool coast, bool war = false) {
  JobCtx c;
  c.trade = (int)t; c.variant = variant; c.extra = extra; c.twist = twist; c.coast = coast; c.war = war; c.season = 2;
  return c;
}

}  // namespace

const KindDef& kindSmuggle() {
  static const KindDef K = [] {
    KindDef k;
    k.id = "smuggle";
    k.title = "THE NIGHT LANDING";
    k.lead = "HAS NIGHT WORK, ASK QUIETLY";
    k.body = kBody;
    k.variants = {"catch", "run"};
    k.twists = {"salt", "spirit"};
    k.extras = {"plain", "steel", "grain", "glass"};
    k.trades = {(int)Role::Guard, (int)Role::Merchant, (int)Role::Jarl, (int)Role::Innkeeper, (int)Role::Villager, (int)Role::Farmer, (int)Role::Fisher};
    k.choose = choose;
    k.prep = prep;
    k.walks = {walk(Role::Merchant, V_CATCH, 0, 0, true), walk(Role::Jarl, V_CATCH, 1, 0, false, true), walk(Role::Guard, V_CATCH, 2, 0, false),
               walk(Role::Merchant, V_CATCH, 3, 0, true), walk(Role::Innkeeper, V_RUN, 0, 1, false), walk(Role::Farmer, V_RUN, 2, 0, false),
               walk(Role::Fisher, V_RUN, 3, 0, true)};
    return k;
  }();
  return K;
}

}  // namespace radiant
}  // namespace story
