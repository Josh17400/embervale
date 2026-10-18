// P1b radiant job kind 9: THE PRICE OF A PERSON (content: jobs_b.md section 4; judge notes resolved). JOBS lane.
//
// Negotiation instead of a raid: outlaws sent a note, a price, a place, and no threat. The player carries the giver's
// purse into a bandit camp and talks to the go-between (SITE-TALK): pay (one time in three the price goes up while you
// walked: a fight, and the purse back from the chief's chest), bargain (a name the chief has heard of, fame 4: the price
// drops; a failed bargain asks a fifth more, from the player's own purse, which the giver repays at home), slip in
// behind (a street-trained hand, background urchin: the jailer ends quietly; else the alarm), or draw steel: the
// HOSTAGE CLOCK (a day, on the tracker) runs while the jailer's knife is at the captive's throat. One time in five it was
// a con: the captive and an outlaw staged it to pay their passage together (never with a spouse). The giver's voice
// follows who asks (family, employer, patron, one of ours) and the money's source (savings, coffers, alms, the hat, the
// ox the neighbours sold): every line fits the giver it is spoken by (judge: the inn's lines only from the innkeeper).
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
role camp site camp near
role captive person [[male|female]] in camp
{!tw_con
role between person male in camp
role jailer person male in camp
}
{?tw_con
role lover person [[male|female]] in camp
}

stage start
  talk giver
  say "[[=reason]] [[=ask]]"
  opt "[[I'LL CARRY THE PURSE.|I'LL BRING {CAPTIVE} HOME.|GIVE ME THE PURSE.]]" -> go
  opt "[[I CAN'T. NOT THIS.|NOT TODAY.|FIND SOMEONE ELSE.]]" -> decline

stage go
  goal goto camp
  journal "OUTLAWS AT {CAMP}, {CAMP.DIR}, HOLD {CAPTIVE} FOR [[=ran]] GOLD. CARRY {GIVER}'S PURSE THERE AND BRING {CAPTIVE.HIM} HOME."
  do give "[[=purse]]" gold
  then [[=meet]]

{!tw_con
stage handover
  talk between
  say "YOU ARE THE ERRAND RUNNER. SHOW ME. [[=ran]] GOLD. COUNT IT BEFORE ME. THEN {CAPTIVE} WALKS. NO TRICKS."
  journal "THE GO-BETWEEN AT {CAMP} WANTS [[=ran]] GOLD FOR {CAPTIVE}."
  opt "[[PAY THE FULL PRICE.|HERE. COUNT IT.]]" -> pay
  opt "[[THAT'S TOO MUCH. HALF.|LET'S TALK ABOUT THAT PRICE.]]" check fame 4 -> bargained else short
  opt "SLIP ROUND THE BACK INSTEAD." check bg urchin -> slip else steel
  opt "[[DRAW STEEL.|NO. I'M TAKING {CAPTIVE.HIM}.]]" -> steel

stage pay
  talk between
{?tw_double
  say "THE PRICE WENT UP WHILE YOU WALKED. ...THEN WE KEEP BOTH. GET THEM!"
}
{!tw_double
  say "[[=ran]], ALL THERE. {CAPTIVE} WALKS. GO, BEFORE THE CHIEF CHANGES HIS MIND."
}
  journal "YOU PAID THE GO-BETWEEN AT {CAMP}."
  do take "[[=purse]]"
{?tw_double
  opt "[[THEN WE DO THIS THE HARD WAY.|YOU'LL REGRET THAT.]]" -> double
}
{!tw_double
  opt "[[COME ON, {CAPTIVE}.|LET'S GO HOME.]]" -> walk_safe
}

{?tw_double
stage double
  goal kill 3 bandit in camp
  journal "THE CAMP AT {CAMP} DOUBLE-CROSSED YOU. FIGHT, AND TAKE THE PURSE BACK FROM THE CHIEF'S CHEST."
  do ambush bandit 2
  then double_out

stage double_out
  goal escort captive to home walk
  journal "THE PURSE IS BACK IN YOUR PACK. WALK {CAPTIVE} HOME TO {HOME}."
  do give "[[=purse]]" gold
  do follow captive
  then report_double
  else captive_lost
}

{!tw_double
stage walk_safe
  goal escort captive to home walk
  journal "WALK {CAPTIVE} HOME TO {GIVER} IN {HOME}."
  do follow captive
  then report_safe
  else captive_lost
}

stage bargained
  talk between
  say "FINE. YOU HAVE A TONGUE. [[=low]] GOLD, AND TAKE {CAPTIVE.HIM}. KEEP THE REST OF YOUR PURSE."
  journal "YOU TALKED THE PRICE DOWN TO [[=low]] GOLD. WALK {CAPTIVE} HOME, WITH WHAT IS LEFT OF THE PURSE."
  do take "[[=purse]]"
  do give "[[=left]]" gold
  opt "[[A PLEASURE.|COME ON, {CAPTIVE}.]]" -> walk_bargain

stage walk_bargain
  goal escort captive to home walk
  journal "WALK {CAPTIVE} HOME TO {GIVER}, WITH WHAT IS LEFT OF THE PURSE."
  do follow captive
  then report_bargain
  else captive_lost

stage short
  talk between
  say "NOT ENOUGH. THE CHIEF SAYS A FIFTH MORE, FOR THE INSULT. MAKE UP THE REST FROM YOUR OWN PURSE, OR WE'RE DONE TALKING."
  journal "THE GO-BETWEEN WANTS [[=extra]] GOLD MORE, FROM YOUR OWN POCKET."
  opt "PAY THE EXTRA. ([[=extra]] GOLD)" if gold [[=extra]] -> paid_extra
  opt "[[THEN WE'RE DONE TALKING.|NO MORE.]]" -> steel

stage paid_extra
  talk between
  say "THERE. THAT WASN'T SO HARD. {CAPTIVE} WALKS."
  journal "YOU PAID THE PRICE AND A FIFTH MORE. WALK {CAPTIVE} HOME."
  do take "[[=purse]]"
  do gold -[[=extra]]
  opt "[[COME ON, {CAPTIVE}.|LET'S GO.]]" -> walk_extra

stage walk_extra
  goal escort captive to home walk
  journal "WALK {CAPTIVE} HOME TO {GIVER} IN {HOME}."
  do follow captive
  then report_extra
  else captive_lost

stage slip
  goal slay jailer
  journal "YOU SLIPPED ROUND THE BACK OF {CAMP} UNSEEN. END {JAILER} QUIETLY AND WALK {CAPTIVE} OUT."
  do hostile jailer
  then walk_slip

stage walk_slip
  goal escort captive to home walk
  journal "THE CAMP IS WAKING BEHIND YOU. WALK {CAPTIVE} HOME TO {GIVER}."
  do follow captive
  do ambush bandit 2
  then report_slip
  else captive_lost

stage steel
  goal slay jailer
  say "THEN WE KEEP BOTH, FOOLS! ...THEN {CAPTIVE} PAYS FOR YOUR TEMPER."
  journal "{JAILER} HAS A KNIFE AT {CAPTIVE}'S THROAT. KILL {JAILER} BEFORE THE DAY IS OUT, OR {CAPTIVE} DIES."
  do hostile jailer
  do ambush bandit 3
  deadline 1 -> steel_failed
  then walk_steel

stage walk_steel
  goal escort captive to home walk
  journal "{JAILER} IS DEAD. WALK {CAPTIVE} HOME TO {GIVER}."
  do follow captive
  then report_steel
  else captive_lost

{!tw_double
stage report_safe
  talk giver
  say "[[=safe]]"
  journal "BRING {CAPTIVE} HOME TO {GIVER} IN {HOME}."
  do stay captive
  opt "[[{CAPTIVE.HE}'S HOME.|SAFE AND SOUND.]]" -> safe_end
}

{?tw_double
stage report_double
  talk giver
  say "[[=double]]"
  journal "BRING {CAPTIVE} AND THE PURSE HOME TO {GIVER} IN {HOME}."
  do stay captive
  do take "[[=purse]]"
  opt "[[THEY CHEATED. I CHEATED BACK.|THE PURSE IS WHOLE.]]" -> double_end
}

stage report_bargain
  talk giver
  say "[[=bargain]]"
  journal "BRING {CAPTIVE} AND WHAT IS LEFT OF THE PURSE HOME TO {GIVER}."
  do stay captive
  do take "[[=left]]"
  opt "[[THEY TOOK LESS.|A BARGAIN.]]" -> bargain_end

stage report_extra
  talk giver
  say "[[=safe]] AND YOU PAID A FIFTH MORE FROM YOUR OWN PURSE? THEN IT COMES BACK TO YOU FROM MINE."
  journal "BRING {CAPTIVE} HOME TO {GIVER} IN {HOME}."
  do stay captive
  do gold [[=extra]]
  opt "[[THANK YOU.|IT WAS WORTH IT.]]" -> safe_end

stage report_slip
  talk giver
  say "[[=safe]] ...IN AND OUT, AND THE CAMP NEVER KNEW? THE PURSE IS WHOLE. HOW?"
  journal "BRING {CAPTIVE} HOME TO {GIVER}, AND THE UNTOUCHED PURSE."
  do stay captive
  do take "[[=purse]]"
  opt "[[QUIETLY.|A TRADE SECRET.]]" -> slip_end

stage report_steel
  talk giver
  say "[[=steelwon]]"
  journal "BRING {CAPTIVE} HOME TO {GIVER}, AND THE UNTOUCHED PURSE."
  do stay captive
  do take "[[=purse]]"
  opt "[[IT WAS CLOSE.|{JAILER} IS DEAD.]]" -> steel_end
}

{?tw_con
stage con
  talk captive
  say "DO NOT LOOK PUZZLED. NOBODY HOLDS ME. {LOVER} AND I SCHEMED FOR MONTHS. THE PURSE PAYS PASSAGE FOR TWO. [[=conwhy]]"
  journal "{CAPTIVE} WAS NEVER HELD. THE RANSOM IS {CAPTIVE.HIS} PASSAGE MONEY, WITH {LOVER}."
  opt "THE PURSE IS YOURS. GO." -> con_go
  opt "THE PURSE GOES HOME. GO FREE." -> con_home
  opt "SPLIT IT. HALF EACH WAY." check fame 4 -> con_split else con_splitfail
  opt "[[YOU ARE COMING HOME.|NO. HOME. NOW.]]" -> con_force

stage con_go
  talk giver
  say "[[=congo]]"
  journal "YOU LET {CAPTIVE} GO WITH THE PURSE. TELL {GIVER} THE TRUTH."
  do take "[[=purse]]"
  do rumour
  opt "[[THAT'S THE TRUTH OF IT.|I'M SORRY.]]" -> congo_end

stage con_home
  talk giver
  say "[[=conhome]]"
  journal "{CAPTIVE} AND {LOVER} LEFT WITH NOTHING BUT EACH OTHER. BRING THE PURSE HOME TO {GIVER}."
  do take "[[=purse]]"
  opt "[[THE PURSE IS WHOLE.|THEY WANTED A LIFE.]]" -> conhome_end

stage con_split
  talk giver
  say "[[=consplit]]"
  journal "HALF THE PURSE WENT WITH THE LOVERS. BRING THE OTHER HALF HOME TO {GIVER}."
  do take "[[=purse]]"
  opt "[[HALF EACH WAY.|IT SEEMED FAIR.]]" -> consplit_end

stage con_splitfail
  talk giver
  say "THEY WOULDN'T SPLIT IT, AND THERE WAS A SCUFFLE. NOBODY DIED. THE PURSE IS WHOLE. {CAPTIVE} CURSED YOU, I HEAR. WELL."
  journal "THE LOVERS REFUSED THE SPLIT. THE PURSE CAME HOME WHOLE. BRING IT TO {GIVER}."
  do take "[[=purse]]"
  opt "[[THEY'LL MANAGE.|IT'S DONE.]]" -> splitfail_end

stage con_force
  goal escort captive to home walk
  journal "{CAPTIVE} COMES HOME, FURIOUS. WALK {CAPTIVE.HIM} TO {GIVER} IN {HOME}."
  do follow captive
  do remember lover "YOU TOOK {CAPTIVE} FROM ME. I WILL FIND YOU."
  then report_force
  else captive_lost

stage report_force
  talk giver
  say "[[=conforce]]"
  journal "BRING {CAPTIVE} AND THE PURSE HOME TO {GIVER}."
  do stay captive
  do take "[[=purse]]"
  opt "[[{CAPTIVE.HE}'S HOME.|AS YOU ASKED.]]" -> force_end
}

{!tw_con
stage safe_end
  end success
  journal "{CAPTIVE} IS HOME. THE RANSOM WAS PAID."
  do reward fair 110
  do moves captive home
  do remember giver "YOU CARRIED MY PURSE INTO THAT CAMP AND {CAPTIVE} OUT OF IT."
  do remember captive "YOU CAME FOR ME WITH A PURSE AND A STEADY HAND."

{?tw_double
stage double_end
  end success
  journal "THE OUTLAWS RAISED THE PRICE. YOU FOUGHT, TOOK THE PURSE BACK AND BROUGHT {CAPTIVE} HOME."
  do reward fair 110
  do loot
  do moves captive home
  do remember giver "THEY CHEATED, AND YOU CHEATED THEM BACK. THE PURSE CAME HOME WHOLE."
}

stage bargain_end
  end success
  say "TAKE YOUR SHARE OUT OF WHAT YOU SAVED. YOU EARNED IT."
  journal "YOU BARGAINED THE RANSOM DOWN AND BROUGHT {CAPTIVE} HOME. {GIVER} TIPPED YOU OUT OF THE SAVINGS."
  do reward fair 140
  do moves captive home
  do remember giver "THE BARGAIN YOU STRUCK AT {CAMP}. I STILL TELL IT."

stage slip_end
  end success
  journal "YOU SLIPPED {CAPTIVE} OUT OF {CAMP} UNSEEN. THE PURSE NEVER LEFT YOUR PACK."
  do reward fair 110
  do moves captive home
  do remember giver "IN AND OUT, AND NOT A COIN SPENT. I DON'T ASK HOW."

stage steel_end
  end success
  journal "YOU DREW STEEL AT {CAMP}. {JAILER} FELL, AND {CAPTIVE} CAME HOME. THE PURSE IS UNTOUCHED."
  do reward fair 90
  do moves captive home
  do remember giver "YOU SPENT BLOOD, NOT GOLD. IT WORKED. I STILL SHAKE."

stage steel_failed
  end fail
  say "[[=steelfail]]"
  journal "THE DAY RAN OUT AT {CAMP}. {CAPTIVE} DIED UNDER {JAILER}'S KNIFE. THE PURSE WENT BACK TO {GIVER}."
  do take "[[=purse]]"
  do remember giver "I WOULD HAVE PAID. REMEMBER THAT."
}

{?tw_con
stage congo_end
  end success
  journal "{CAPTIVE} AND {LOVER} SAILED WITH THE PURSE. {GIVER} HEARD THE TRUTH FROM YOU, AND PAID THE COURIER FOR IT."
  do reward fair 55
  do remember giver "A LETTER CAME FROM {CAPTIVE}. FROM FAR AWAY. HAPPY, IT SAYS."

stage conhome_end
  end success
  journal "THE PURSE CAME HOME. {CAPTIVE} AND {LOVER} WENT TO EARN THEIR PASSAGE THEMSELVES."
  do reward fair 110
  do rumour
  do remember giver "THEY SAY TWO TRAVELLERS WORK A FARM FAR AWAY. I HOPE IT'S THEM."

stage consplit_end
  end success
  journal "HALF THE PURSE PAID THE LOVERS' PASSAGE. HALF CAME HOME."
  do reward fair 100
  do deed charity
  do remember giver "HALF FOR LOVE, HALF FOR HOME. IT WAS FAIR, I SUPPOSE."

stage splitfail_end
  end success
  journal "THE LOVERS WOULD NOT SPLIT THE PURSE. IT CAME HOME WHOLE; {CAPTIVE} DID NOT."
  do reward fair 90
  do remember captive "YOU TRIED TO TAKE HALF OUR LIFE. WE'LL EARN IT ANOTHER WAY."

stage force_end
  end success
  journal "YOU DRAGGED {CAPTIVE} HOME. THE PURSE IS BACK. {CAPTIVE} WILL NOT STAY."
  do reward fair 90
  do moves captive home
  do remember giver "{CAPTIVE} IS HOME, AND HATES US FOR IT. AT LEAST {CAPTIVE.HE} LIVES."
}

stage captive_lost
  end fail
  say "{CAPTIVE} DID NOT LIVE TO SEE {HOME}. THE ROAD BACK FROM {CAMP} WAS THE LAST OF IT."
  journal "{CAPTIVE} DIED ON THE WAY HOME FROM {CAMP}."
  do remember giver "{CAPTIVE} DIED ON THE ROAD HOME. THE PURSE MEANS NOTHING NOW."

stage decline
  end fail
  journal "YOU TURNED DOWN {GIVER}'S RANSOM."
)DSL";

enum { V_FAMILY, V_EMPLOYER, V_PATRON, V_OURS };
enum { T_PLAIN, T_DOUBLE, T_CON };
enum { PAID_SAVINGS, PAID_COFFER, PAID_ALMS, PAID_HAT, PAID_OX };

void prep(const JobCtx& c, uint64_t seed, std::map<std::string, std::string>& P) {
  Dice d(seed);
  const Role t = (Role)c.trade;
  const int ran = 100 + 60 * c.tier + 10 * d.below(5);
  const int low = ran * 55 / 100, extra = ran / 5;
  P["ran"] = std::to_string(ran); P["low"] = std::to_string(low); P["extra"] = std::to_string(extra);
  P["purse"] = "A PURSE OF " + std::to_string(ran) + " GOLD";
  P["left"] = "WHAT IS LEFT OF THE PURSE";
  P["meet"] = c.twist == T_CON ? "con" : "handover";
  // who asks: the relation word and where the purse came from
  int paid = PAID_SAVINGS;
  std::string rel;
  switch (t) {
    case Role::Merchant: rel = "FACTOR"; paid = PAID_COFFER; break;
    case Role::Jarl: rel = "STEWARD"; paid = PAID_COFFER; break;
    case Role::Mage: rel = "STUDENT"; paid = PAID_COFFER; break;
    case Role::Priest: rel = "NOVICE"; paid = PAID_ALMS; break;
    case Role::Innkeeper: rel = "REGULAR"; paid = PAID_HAT; break;
    case Role::Guard: rel = "SERGEANT"; paid = PAID_SAVINGS; break;
    case Role::Farmer: if (c.extra == 1) { rel = "NEIGHBOUR"; paid = PAID_OX; } else rel = d.chance(50) ? "{CAPTIVE.SON}" : "{CAPTIVE.BROTHER}"; break;
    default: rel = d.chance(50) ? "{CAPTIVE.SON}" : "{CAPTIVE.BROTHER}"; break;
  }
  std::vector<std::string> R;
  switch (t) {
    case Role::Merchant: R = {"A NOTE CAME WITH THE LAST CARAVAN: MY FACTOR {CAPTIVE} FOR " + P["ran"] + " GOLD, AT {CAMP}. THE BOOKS SAY {CAPTIVE.HE} IS WORTH IT."}; break;
    case Role::Jarl: R = {"{CAPTIVE}, MY STEWARD, RODE OUT TO COUNT SHEEP. A NOTE CAME BACK: " + P["ran"] + " GOLD AT {CAMP}. I CANNOT BE SEEN TO PAY OUTLAWS, SO YOU WILL."}; break;
    case Role::Mage: R = {"{CAPTIVE} WAS MY BEST STUDENT AND IS NOW MY MOST EXPENSIVE. THE NOTE SAYS " + P["ran"] + " GOLD AT {CAMP}. MAKE THEM BELIEVE YOU ARE NOBODY."}; break;
    case Role::Priest: R = {"THE TEMPLE DOES NOT PAY OUTLAWS. BUT THE ALMS BOX HELD A PURSE NOBODY WOULD OWN, AND THE NOTE FOR MY NOVICE {CAPTIVE} NAMES THE SAME SUM."}; break;
    case Role::Innkeeper: R = {"{CAPTIVE} DRINKS AT MY FIRE EVERY NIGHT. THE NOTE SAYS " + P["ran"] + " GOLD AT {CAMP}. WE PASSED THE HAT. THIS IS THE HAT."}; break;
    case Role::Guard: R = {"ORDERS SAY NO TERMS WITH OUTLAWS. THE NOTE ASKS " + P["ran"] + " GOLD AT {CAMP} FOR SERGEANT {CAPTIVE}, WHO CARRIED ME OUT OF THE LAST FIGHT."}; break;
    default:
      if (paid == PAID_OX) R = {"THREE FAMILIES SOLD THE GOOD OX TO MAKE THIS PURSE FOR OUR NEIGHBOUR {CAPTIVE}. THE NOTE SAYS {CAMP}. DO NOT LOSE IT."};
      else R = {"THEY WANT " + P["ran"] + " GOLD FOR MY " + rel + ", {CAPTIVE}. THIS IS EVERYTHING WE HAVE. I AM ASKING A STRANGER TO CARRY IT."};
      break;
  }
  if (c.season == 2) R.push_back("THE ROADS ARE BUSY, SO THE OUTLAWS ARE FAT. THEY STILL NAMED A PRICE FOR {CAPTIVE}: " + P["ran"] + " GOLD AT {CAMP}.");
  if (c.war) R.push_back("WITH THE SOLDIERS AT THE BORDER, THE ROADS BELONG TO THE BAND AT {CAMP}. THE NOTE FOR {CAPTIVE} SAYS " + P["ran"] + " GOLD.");
  P["reason"] = R[(size_t)d.below((int)R.size())];
  P["ask"] = d.pick({"CARRY THE PURSE AND BRING {CAPTIVE.HIM} HOME.", "PAY THEM, OR DO BETTER THAN PAY THEM.", "BRING {CAPTIVE.HIM} OUT. THE METHOD IS YOURS."});
  const int v = c.variant;
  // the turn-in lines by voice (and, for one of ours, by the purse: the hat, the savings, the ox)
  auto ours = [&](const char* hat, const char* sav, const char* ox) { return std::string(paid == PAID_HAT ? hat : paid == PAID_OX ? ox : sav); };
  switch (v) {
    case V_FAMILY:
      P["safe"] = "{CAPTIVE}! COME HERE. LET ME LOOK AT YOU. ...YOU CARRIED MY LIFE IN A PURSE.";
      P["bargain"] = "GOLD BACK, AND {CAPTIVE} HOME? TAKE YOUR SHARE OF WHAT YOU SAVED. YOU EARNED IT.";
      P["double"] = "YOU FOUGHT FOR {CAPTIVE}. BLESSINGS ON YOU. I OWE YOU MORE THAN GOLD.";
      P["steelwon"] = "BLOOD ON THE PURSE, AND {CAPTIVE.HE} LIVES. A HARD ROAD. I ASKED FOR IT.";
      P["steelfail"] = "THE PURSE IS HERE. {CAPTIVE} IS NOT. I WILL NEVER FORGIVE YOU. I KNOW IT.";
      P["conwhy"] = "THEY WOULD NEVER HAVE LET ME GO.";
      P["conhome"] = "A PURSE FOR A LIE. THE LIE IS WORSE. BUT {CAPTIVE.HE} IS ALIVE. THAT IS ENOUGH.";
      P["consplit"] = "HALF. FINE. HALF IS FAIR. THEY WILL NOT STARVE. NOR WILL I.";
      P["conforce"] = "{CAPTIVE.HE} IS HOME. {CAPTIVE.HE} HATES US. ...SO BE IT. AT LEAST {CAPTIVE.HE} LIVES.";
      break;
    case V_EMPLOYER:
      P["safe"] = "{CAPTIVE} IS BACK, ALREADY AUDITING. THE BOOKS BALANCE. THANK YOU.";
      P["bargain"] = "GOLD UNSPENT AND {CAPTIVE} HOME. A DREAM. YOUR CUT IS ON THE TABLE.";
      P["double"] = "THEY CHEATED. YOU CHEATED THEM BACK. THE PURSE IS WHOLE. I LIKE THAT.";
      P["steelwon"] = "YOU SPENT BLOOD, NOT GOLD. FAIR. THE PURSE IS UNTOUCHED. NOTED.";
      P["steelfail"] = "{CAPTIVE} DIED. THE PURSE IS WHOLE. I WOULD HAVE PAID. REMEMBER THAT.";
      P["conwhy"] = "MY CONTRACT RUNS SEVEN MORE YEARS.";
      P["conhome"] = "A STAGED KIDNAP, IN MY OWN BOOKS. THE PURSE IS HOME. THAT IS ENOUGH.";
      P["consplit"] = "HALF TO THEM, HALF TO THE BOOKS. CALL IT SEVERANCE.";
      P["conforce"] = "{CAPTIVE} IS AT THE DESK, FURIOUS. BY WINTER {CAPTIVE.HE} WILL BE GONE AGAIN.";
      break;
    case V_PATRON:
      P["safe"] = "{CAPTIVE} IS HOME. NO LECTURE TODAY. ...MAYBE A SHORT ONE. THANK YOU.";
      P["bargain"] = "GOLD SAVED AND {CAPTIVE} SAFE. A FEE FOR THE TONGUE, THEN. TAKE IT.";
      P["double"] = "AN OUTLAW CLAIMED A BETTER PRICE. ...AND LOST. WELL DONE.";
      P["steelwon"] = "A BLUNT SOLUTION. IT WORKED. UNSPENT PURSE. {CAPTIVE} UNHURT.";
      P["steelfail"] = "A WISE HEAD, DEAD OF A QUICK ONE. YOU SHOULD HAVE PAID. THAT IS ALL.";
      P["conwhy"] = "THE HOUSE KEEPS ITS OWN FOR LIFE.";
      P["conhome"] = "STAGED, AND NEARLY WELL DONE. THE PURSE IS HOME. THAT IS A LESSON.";
      P["consplit"] = "HALF FOR LOVE, HALF FOR LEDGERS. TELL {CAPTIVE} TO WRITE. NOT OFTEN.";
      P["conforce"] = "A STUDENT DRAGGED HOME LEARNS NOTHING. {CAPTIVE.HE} WILL LEAVE AGAIN. I KNOW IT.";
      break;
    default:
      P["safe"] = ours("ONE OF OURS CAME HOME. SIT. THE FIRST ROUND IS MINE.", "ONE OF OURS CAME HOME. THANK YOU. I WON'T FORGET IT.",
                       "OUR NEIGHBOUR IS HOME. THE THREE FAMILIES WILL WANT TO SHAKE YOUR HAND.");
      P["bargain"] = ours("GOLD BACK IN THE HAT, AND {CAPTIVE} HOME. KEEP A SHARE. YOU EARNED IT.", "SAVINGS SAVED, AND {CAPTIVE} HOME. KEEP A SHARE.",
                          "SOME OF THE OX MONEY BACK? KEEP A SHARE. THE REST GOES TO THE FAMILIES.");
      P["double"] = "THEY CHEATED. YOU HIT BACK. GOOD. ONE OF OURS IS HOME. THAT IS ALL.";
      P["steelwon"] = "THE PURSE IS WHOLE AND THEY ARE DEAD. ...GOOD. ONE OF OURS IS HOME.";
      P["steelfail"] = ours("{CAPTIVE} IS GONE. THE PURSE IS HERE. WE WOULD HAVE EMPTIED THE INN TWICE.", "{CAPTIVE} IS GONE. I WOULD HAVE GIVEN EVERY COIN.",
                            "{CAPTIVE} IS GONE. THREE FAMILIES SOLD AN OX FOR NOTHING.");
      P["conwhy"] = t == Role::Guard ? "A SERGEANT IS NEVER DISCHARGED." : "NOBODY EVER LEAVES THIS PLACE. I WILL.";
      P["conhome"] = "ONE OF OURS, AND A LIAR. STILL OURS. THE PURSE IS WHOLE. THAT IS ENOUGH.";
      P["consplit"] = ours("HALF GOES HOME. THAT BUYS A ROUND. TELL {CAPTIVE} NOT TO COME BACK POOR.", "HALF GOES HOME. FAIR. TELL {CAPTIVE} NOT TO COME BACK POOR.",
                           "HALF THE OX COMES HOME. THE FAMILIES WILL TAKE THAT.");
      P["conforce"] = "{CAPTIVE} IS BACK. FURIOUS. ALIVE. ...WE WILL TAKE THE ANGER FIRST.";
      break;
  }
  // the purse is yours, go: by who paid (the courier is paid for the truth: judge)
  switch (paid) {
    case PAID_COFFER: P["congo"] = "{CAPTIVE} FLED WITH THE COFFERS? ...IS {CAPTIVE.HE} WELL? THEN CALL IT WAGES. AND YOU BROUGHT ME THE TRUTH. THAT IS WORTH HALF A FEE."; break;
    case PAID_ALMS: P["congo"] = "THE ALMS WERE FOR THE POOR. ...THEN {CAPTIVE.HE} IS POOR NOW. FAIR. YOU BROUGHT THE TRUTH HOME. HALF A FEE FOR THAT."; break;
    case PAID_HAT: P["congo"] = "THE WHOLE INN PASSED THE HAT. ...IS {CAPTIVE.HE} WELL? THEN THEY DRINK TO IT. HALF A FEE, FOR THE TRUTH."; break;
    case PAID_OX: P["congo"] = "THREE FAMILIES SOLD THEIR OX FOR THIS. THE GOOD OX. FOR A WEDDING TRIP. ...{CAPTIVE} LIVES. I WILL TELL THEM. HALF A FEE. GO."; break;
    default: P["congo"] = "{CAPTIVE} RAN OFF WITH MY SAVINGS? ...IS {CAPTIVE.HE} WELL? THEN SPEND IT WELL, CHILD. YOU BROUGHT ME THE TRUTH. HALF A FEE FOR THAT."; break;
  }
  std::vector<std::string> T = {"THE PRICE OF A PERSON", "A PURSE AND A NOTE", "HELD FOR A PRICE", "THE RANSOM NOTE", "NO HEROES, JUST GOLD",
                                "NO THREAT IN THE NOTE", "EVERYTHING IN A PURSE"};
  P["title"] = T[(size_t)d.below((int)T.size())];
}

bool choose(const JobCtx& w, uint64_t dice, JobCtx& out) {
  Dice d(dice);
  out = w;
  switch ((Role)w.trade) {
    case Role::Villager: out.variant = V_FAMILY; break;
    case Role::Farmer: out.variant = d.chance(35) ? V_OURS : V_FAMILY; out.extra = out.variant == V_OURS ? 1 : 0; break;
    case Role::Merchant: case Role::Jarl: out.variant = V_EMPLOYER; break;
    case Role::Mage: case Role::Priest: out.variant = V_PATRON; break;
    case Role::Innkeeper: case Role::Guard: out.variant = V_OURS; break;
    default: return false;
  }
  const int roll = d.below(15);
  out.twist = roll < 3 ? T_CON : roll < 7 ? T_DOUBLE : T_PLAIN;   // the con one in five; the double-cross one in three of the rest
  return true;
}

JobCtx walk(Role t, int variant, int twist, int extra = 0, int tier = 0) {
  JobCtx c;
  c.trade = (int)t; c.variant = variant; c.twist = twist; c.extra = extra; c.tier = tier; c.season = 2;
  return c;
}

}  // namespace

const KindDef& kindRansom() {
  static const KindDef K = [] {
    KindDef k;
    k.id = "ransom";
    k.title = "THE PRICE OF A PERSON";
    k.lead = "NEEDS A RANSOM CARRIED";
    k.body = kBody;
    k.variants = {"family", "employer", "patron", "ours"};
    k.twists = {"plain", "double", "con"};
    k.extras = {"none", "ox"};
    k.trades = {(int)Role::Merchant, (int)Role::Mage, (int)Role::Priest, (int)Role::Jarl, (int)Role::Innkeeper, (int)Role::Guard, (int)Role::Farmer, (int)Role::Villager};
    k.choose = choose;
    k.prep = prep;
    k.walks = {walk(Role::Merchant, V_EMPLOYER, T_PLAIN, 0, 1), walk(Role::Innkeeper, V_OURS, T_DOUBLE), walk(Role::Farmer, V_OURS, T_CON, 1),
               walk(Role::Priest, V_PATRON, T_CON), walk(Role::Villager, V_FAMILY, T_DOUBLE)};
    return k;
  }();
  return K;
}

}  // namespace radiant
}  // namespace story
