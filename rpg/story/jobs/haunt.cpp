// P1b radiant job kind 6: A HOUSE THAT WAITS (content: jobs_b.md section 1; judge notes resolved). JOBS lane.
//
// A ghost with a grievance in a ruin near the town. The player learns what one named dead person wants (SITE-TALK: the
// ghost waits in the ruin) and chooses how to end it, with a price on every road:
//   unburied  the bones lie in a chest under the fallen stone: carry them to holy ground (RITE), or banish the ghost
//             (the dead fight; the haunting may come back, the pay is less)
//   wronged   (priest, lord, guard, innkeeper: an office with a past) the proof that the office of those days lied is
//             in the box: carry it home, and at the giver READ IT ALOUD (the office pays restitution from its purse, so
//             the courier's fee is halved, by the giver's own choice once they read it) or BURN IT (full pay; but that
//             night the hearths go cold: THE ASH TALKS BACK, and the ghost must be faced again: confess, or banish it)
//   post      (guard, lord, innkeeper; an old ruin's sentry who never stood down) blow the dented watch-horn from the
//             guardroom (relieved: the strongbox is yours), talk it down with a name it has heard (it takes the key with
//             it), or banish it
//   hoax      no ghost: a family of four keeping a roof with a sheet, a chain and a lamp. Drive them out, let them stay
//             (and keep or refuse their last coin), or send them to the giver's town, where they make their own way and
//             wait at the turn-in (no four-person escort)
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
role house site ruin near
role ghost person [[male|female]] in house

stage start
  talk giver
  say "[[=reason]] [[=ask]] [[THE PAY IS FAIR.|I WILL PAY FOR PEACE.|NAME YOUR FEE WHEN IT IS DONE.]]"
  opt "[[I'LL GO TO {HOUSE}.|I'LL ASK THE DEAD WHAT IT WANTS.|LEAVE THE DEAD TO ME.]]" -> go
  opt "[[NOT GHOSTS. NOT TODAY.|FIND A PRIEST.|I DON'T DEAL WITH THE DEAD.]]" -> decline

stage go
  goal goto house
  journal "[[=desc]]"
  then meet

{?v_unburied
stage meet
  talk ghost
  say "YOU ARE WARM. HOW RUDE OF YOU. I HAVE WAITED A LONG TIME. NOBODY CAME FOR MY BONES. THEY LIE IN A CHEST UNDER THE FALLEN STONE, IN HERE. TAKE THEM TO HOLY GROUND."
  journal "{GHOST} WALKS AT {HOUSE}, AND WANTS THE BONES IN THE CHEST UNDER THE FALLEN STONE CARRIED TO HOLY GROUND."
  opt "[[I WILL CARRY THEM HOME.|YOUR BONES GO TO HOLY GROUND.]]" -> bones
  opt "[[REST, OR I END YOU.|THE DEAD GET NO SAY. BEGONE.]]" -> banish

stage bones
  goal fetch "{GHOST}'S BONES" in house
  journal "FIND {GHOST}'S BONES IN THE CHEST UNDER THE FALLEN STONE AT {HOUSE}, THEN CARRY THEM TO {GIVER}."
  then rite

stage rite
  talk giver
  say "[[=riteline]]"
  journal "BRING {GHOST}'S BONES TO {GIVER} IN {HOME}, FOR HOLY GROUND."
  opt "[[THE WALKING HAS STOPPED.|{GHOST} IS AT PEACE.]]" -> rite_end
}

{?v_wronged
stage meet
  talk ghost
  say "YOU ARE WARM. HOW RUDE OF YOU. I HAVE WAITED A LONG TIME. THE [[=office]] OF THOSE DAYS SWORE I STOLE. I NEVER DID. THE PROOF IS IN THE BOX, IN HERE. LET THE TOWN READ IT. OR LET IT LIE."
  journal "{GHOST} WALKS AT {HOUSE}, ACCUSED OF THEFT BY THE [[=office]] OF THOSE DAYS. THE PROOF IS IN A BOX THERE."
  opt "[[I WILL BRING THE PROOF HOME.|THE TOWN WILL SEE THE PROOF.]]" -> proof
  opt "[[THE DEAD GET NO SAY. BEGONE.|REST, OR I END YOU.]]" -> banish

stage proof
  goal fetch "A SEALED LEDGER" in house
  journal "FIND THE SEALED LEDGER AT {HOUSE} AND CARRY IT TO {GIVER} IN {HOME}."
  then read

stage read
  talk giver
  say "...LET US SEE WHAT IT SAYS. THE OLD [[=office]]'S OWN HAND. A FORGED TALLY. THE REAL THIEF NAMED. {GHOST} WAS INNOCENT. WHAT DO WE DO WITH THIS?"
  journal "{GIVER} HAS READ THE PROOF. IT IS FOR YOU TO SAY WHAT BECOMES OF IT."
  opt "READ IT ALOUD TO THE TOWN." -> aloud
  opt "BURN IT." -> burn

stage burn
  talk giver
  say "ASH. THE TOWN KEEPS ITS QUIET, AND YOU YOUR FULL FEE. ...I HOPE THE DEAD HAVE SHORT MEMORIES."
  journal "YOU BURNED THE PROOF OF {GHOST}'S INNOCENCE. {GIVER} PAID IN FULL."
  do take "A SEALED LEDGER"
  do reward fair 110
  do deed lied
  do fact "THE PROOF OF {GHOST}'S INNOCENCE WAS BURNED IN {HOME}."
  opt "[[IT'S DONE.|NOBODY NEEDS TO KNOW.]]" -> midnight

stage midnight
  goal time 0 4
  journal "THE PROOF IS ASH. WAIT OUT THE NIGHT IN {HOME}."
  then ash

stage ash
  talk giver
  say "{GHOST} IS BACK, AND WORSE. THE HEARTHS OF {HOME} WENT COLD AT MIDNIGHT, EVERY ONE. WE BURNED THE PROOF TOGETHER. WE WILL ANSWER FOR IT TOGETHER. GO BACK."
  journal "THE ASH TALKS BACK. EVERY HEARTH IN {HOME} WENT COLD AT MIDNIGHT. FACE {GHOST} AT {HOUSE} AGAIN."
  opt "[[I'LL GO BACK.|I'LL ANSWER FOR IT.]]" -> ash_go

stage ash_go
  goal goto house
  journal "GO BACK TO {HOUSE} AND FACE {GHOST}."
  then ash_ghost

stage ash_ghost
  talk ghost
  say "YOU BURNED MY NAME TO ASH. EVERY HEARTH IN {HOME} GOES COLD. TELL THE TOWN, OR I WILL."
  journal "{GHOST} WANTS THE TRUTH TOLD IN {HOME}."
  opt "I WILL TELL THEM. ALL OF IT." check fame 3 -> confess else last_banish
  opt "[[BEGONE.|NO MORE.]]" -> last_banish

stage confess
  talk giver
  say "THEN TELL IT LOUD. I WILL LISTEN. ...A LIAR TELLS THE TRUTH. LATE. FINE. THE ALMS GO TO {GHOST}'S LINE, AND THE HEARTHS ARE WARM."
  journal "YOU CONFESSED THE BURNING IN {HOME}. {GHOST} RESTS."
  opt "[[IT'S TOLD.|THE TOWN KNOWS NOW.]]" -> confess_end

stage last_banish
  goal kill 4 undead in house
  say "LIAR. LIAR. THEN HOLD THE DARK, IF YOU CAN."
  journal "{GHOST} WILL NOT BELIEVE YOU. THE DEAD OF {HOUSE} RISE. DESTROY THEM."
  then last_end
}

{?v_post
stage meet
  talk ghost
  say "HALT. WHO GOES THERE? ...I HOLD THIS POST UNTIL RELIEVED. NO ONE HAS COME. NOT ONE. THE HORN IS IN THE GUARDROOM, AS IT ALWAYS WAS."
  journal "AT {HOUSE} STANDS {GHOST}, A SENTRY WHO NEVER STOOD DOWN. THE WATCH-HORN IS IN THE GUARDROOM."
  opt "[[I'LL FIND THE HORN AND RELIEVE YOU.|STAND EASY. I'LL FETCH THE HORN.]]" -> horn
  opt "THE WAR ENDED LONG AGO." check fame 3 -> talked else banish
  opt "[[YOU HAVE NO POST. DIE.|STAND DOWN, OR FALL DOWN.]]" -> banish

stage horn
  goal fetch "A DENTED WATCH-HORN" in house
  journal "FIND THE DENTED WATCH-HORN IN THE GUARDROOM AT {HOUSE} AND BLOW IT FOR {GHOST}."
  then relieved

stage relieved
  talk ghost
  say "THE HORN. AT LAST. ...RELIEVED. THE WATCH IS YOURS, AND WHAT THE GUARDROOM HOLDS WITH IT. I HAVE STOOD LONG ENOUGH."
  journal "YOU BLEW THE HORN. {GHOST} IS RELIEVED. TELL {GIVER}."
  do take "A DENTED WATCH-HORN"
  do reward small 50
  do remember giver "THE HORN BLEW AT MIDNIGHT. HALF THE TOWN HEARD IT. THEN THE POST WAS QUIET."
  opt "[[REST NOW. THE WATCH IS KEPT.|YOU ARE RELIEVED.]]" -> post_report

stage talked
  talk ghost
  say "ENDED? ...THEN WHAT WAS IT FOR? TELL ME WHAT BECAME OF THE TOWN. ...THANK YOU. I WILL TAKE THE GUARDROOM KEY WITH ME. IT WAS ALWAYS MINE TO KEEP."
  journal "YOU TALKED {GHOST} DOWN. THE SENTRY TOOK THE GUARDROOM KEY INTO THE DARK. TELL {GIVER}."
  do xp 30
  do remember giver "YOU TALKED IT DOWN? SOFT WORK. THE POST SLEPT AT LAST. GOOD."
  opt "[[REST NOW.|THE WAR IS OVER.]]" -> post_report

stage post_report
  talk giver
  say "[[THEN THE POST IS HELD. GOOD. I'LL SLEEP.|NOT ONE SOUND FROM {HOUSE} ALL NIGHT. GOOD WORK.]]"
  journal "TELL {GIVER} IN {HOME} THAT THE SENTRY OF {HOUSE} STOOD DOWN."
  opt "[[THE SENTRY STOOD DOWN.|IT'S QUIET NOW.]]" -> rite_end
}

{?v_hoax
stage meet
  talk ghost
  say "SHH! YOU WILL SPOIL IT! NO GHOST. JUST FOUR OF US. THE ROOF IS GOOD AND NOBODY COMES. [[=sign]] [[=lastday]]"
  journal "THERE IS NO GHOST AT {HOUSE}: {GHOST}'S FAMILY OF FOUR LIVES THERE, WORKING THE HAUNTING TO KEEP THE ROOF."
  opt "[[PACK UP. YOU'RE LEAVING.|DRIVE THEM OUT.]]" -> drive
  opt "THEY STAY. I SAW NOTHING." -> stay
  opt "COME TO {HOME}. I'LL FIND ROOM." -> bring

stage drive
  talk giver
  say "GONE? A FAMILY? ...[[=driveline]]"
  journal "YOU DROVE {GHOST}'S FAMILY OUT OF {HOUSE}. TELL {GIVER}."
  opt "[[THE HOUSE IS EMPTY NOW.|NO MORE GHOST.]]" -> drive_end

stage stay
  talk ghost
  say "YOU'LL SAY NOTHING? ...THEN TAKE THIS. IT'S THE LAST OF OUR COIN, BUT IT'S YOURS."
  journal "YOU WILL TELL {GIVER} THE GHOST IS GONE. {GHOST} OFFERS THE LAST OF THE FAMILY'S COIN."
  opt "[[KEEP YOUR COIN.|BUY THE SMALL ONES SHOES.]]" -> stay_keep
  opt "[[I'LL TAKE IT.|THANK YOU.]]" -> stay_take

stage stay_keep
  talk giver
  say "QUIET AGAIN AT {HOUSE}? I DO NOT ASK HOW. HERE IS THE FEE."
  journal "TELL {GIVER} THAT {HOUSE} IS QUIET."
  do deed charity
  opt "[[IT'S QUIET.|NO MORE WALKING.]]" -> stay_end

stage stay_take
  talk giver
  say "QUIET AGAIN AT {HOUSE}? I DO NOT ASK HOW. HERE IS THE FEE."
  journal "TELL {GIVER} THAT {HOUSE} IS QUIET."
  do gold 25
  opt "[[IT'S QUIET.|NO MORE WALKING.]]" -> stay_end

stage bring
  talk giver
  say "A FAMILY OF FOUR AT MY DOOR, SAYING YOU SENT THEM? ...[[=bringline]]"
  journal "{GHOST}'S FAMILY MADE ITS OWN WAY TO {HOME}. THEY WAIT AT {GIVER}'S DOOR."
  opt "[[THEY NEED A ROOF.|THEY'LL WORK FOR IT.]]" -> bring_end
}

{!v_hoax
stage banish
  goal kill 4 undead in house
  say "THEN I END YOU FIRST."
  journal "THE DEAD OF {HOUSE} RISE AT {GHOST}'S CALL. DESTROY THEM AND DRIVE {GHOST} OUT."
  then banished

stage banished
  talk giver
  say "[[THE HOUSE IS QUIET. FOR NOW.|A SPIRIT SENT AWAY CAN COME BACK. I PAY FOR THE WORK, NOT THE PEACE.]]"
  journal "YOU DROVE {GHOST} OUT OF {HOUSE}. TELL {GIVER}."
  opt "[[IT'S DONE.|THE DEAD ARE DUST.]]" -> banish_end

}

{?v_unburied|v_post
stage rite_end
  end success
  say "[[THANK YOU. IT IS QUIET NOW.|THE NIGHTS ARE QUIET. THANK YOU.]]"
  journal "{GHOST} RESTS. {HOUSE} IS QUIET, AND THE HAUNTING WILL NOT RETURN."
  do reward fair 110
  do take "{GHOST}'S BONES"
  do calm house
  do remember giver "[[=ritemem]]"
  do fact "{GHOST}, WHO WALKED AT {HOUSE}, WAS GIVEN PEACE."
  do census home mood 4
  do census home stock produce 2
}

{?v_wronged
stage aloud
  end success
  say "[[=aloudline]] THE TOWN LEARNS THE OLD [[=office]] LIED. {GHOST}'S NAME IS CLEAN. THE [[=purse]] PAYS THE LINE ITS DUE, SO YOUR FEE IS HALF. YOU CHOSE WELL."
  journal "YOU READ THE PROOF ALOUD IN {HOME}. {GHOST} WAS INNOCENT, AND RESTS. THE [[=purse]] PAID RESTITUTION."
  do take "A SEALED LEDGER"
  do reward fair 55
  do xp 50
  do fame 1
  do rumour
  do deed charity
  do calm house
  do remember giver "THE [[=office]] OF THOSE DAYS WAS WRONG. WE PAID WHAT WE OWED. I'M GLAD."
  do fact "{GHOST} WAS INNOCENT. THE PROOF WAS READ ALOUD IN {HOME}."
  do census home mood 4
  do census home stock produce 2

stage confess_end
  end success
  journal "YOU CONFESSED. {GHOST} RESTS, AND THE HEARTHS OF {HOME} ARE WARM AGAIN."
  do reward fair 40
  do fame 1
  do fact "A CONFESSION IN {HOME}: THE PROOF OF {GHOST}'S INNOCENCE WAS BURNED, AND THE TRUTH TOLD."
  do remember giver "A LIAR TELLS THE TRUTH. LATE. FINE."
  do calm house
  do census home mood 2
  do census home stock produce 1

stage last_end
  end success
  say "THE HEARTHS ARE LIT AGAIN. THE NAME STAYS DEAD."
  journal "YOU BANISHED {GHOST} FOR GOOD. {HOUSE} IS CLOSED, AND {GHOST}'S NAME STAYS DEAD."
  do remember giver "HEARTHS LIT. THE NAME STAYS DEAD. I DON'T SLEEP WELL."
  do xp 20
}

{?v_hoax
stage drive_end
  end success
  journal "{GHOST}'S FAMILY LEFT {HOUSE}. A FAMILY SLEEPS BY THE {HOME} GATE NOW."
  do reward fair 80
  do rumour
  do fact "A FAMILY WAS DRIVEN FROM {HOUSE} AND SLEEPS BY THE GATE OF {HOME}."
  do remember giver "THE GHOST WAS A FAMILY. I'M PLEASED, AND A LITTLE ASHAMED."

stage stay_end
  end success
  journal "YOU TOLD {GIVER} THE GHOST IS GONE. {GHOST}'S FAMILY KEEPS THE ROOF AT {HOUSE}."
  do reward fair 80
  do deed lied
  do remember ghost "YOU SAW NOTHING. WE REMEMBER THAT."

stage bring_end
  end success
  journal "{GHOST}'S FAMILY HAS A ROOF IN {HOME} NOW, THANKS TO YOU AND {GIVER}."
  do reward fair 75
  do xp 30
  do deed charity
  do moves ghost home
  do remember ghost "A REAL ROOF. YOU DID THAT."
  do census home mood 2
  do census home refugees 1
}

{!v_hoax
stage banish_end
  end success
  journal "{GHOST} WAS DRIVEN OUT OF {HOUSE} BY FORCE. THE DEAD MAY RISE THERE AGAIN."
  do reward fair 80
  do remember giver "A SPIRIT SENT AWAY CAN COME BACK. WE'LL SEE."
}

stage decline
  end fail
  journal "YOU TURNED DOWN THE HAUNTING AT {HOUSE}."
)DSL";

enum { V_UNBURIED, V_WRONGED, V_POST, V_HOAX };

void prep(const JobCtx& c, uint64_t seed, std::map<std::string, std::string>& P) {
  Dice d(seed);
  const Role t = (Role)c.trade;
  P["office"] = t == Role::Priest ? "PRIEST" : t == Role::Jarl ? "LORD" : t == Role::Guard ? "WATCHMASTER" : "INNKEEPER";
  P["purse"] = t == Role::Priest ? "ALMS BOX" : t == Role::Jarl ? "HALL'S COFFER" : t == Role::Guard ? "WATCH FUND" : "INN'S TILL";
  std::vector<std::string> R;
  const bool cold = c.season >= 2;
  // the reasons (tags: giver; the wants each can lead to; HOAX only on reasons a family could fake)
  switch (t) {
    case Role::Priest: R.push_back("MY BELL RINGS ITSELF AT MIDNIGHT. FOLK SAY {GHOST} WALKS THE HALLS OF {HOUSE} AND WANTS SOMETHING."); break;
    case Role::Innkeeper: R.push_back("NOBODY BUYS A BED AFTER DARK SINCE {GHOST} STARTED WALKING AT {HOUSE}. I AM SELLING CANDLES AT A LOSS."); break;
    case Role::Jarl: R.push_back("THE HALL DOGS HOWL TOWARD {HOUSE} EVERY NIGHT. I CAN FINE A THIEF. I CANNOT FINE {GHOST}."); break;
    case Role::Mage: R.push_back("{HOUSE} IS COLD AT NOON BY MY OWN INSTRUMENTS. A WILL HOLDS {GHOST} THERE. THE DEAD IGNORE ME. THEY MAY TALK TO YOU."); break;
    case Role::Guard: R.push_back("THE WATCH CAN FACE A KNIFE. IT CANNOT FACE WHAT STANDS AT {HOUSE}. THE TOWN CALLS IT {GHOST}."); break;
    default:
      R.push_back("THE COWS WON'T CROSS THE FIELD BY {HOUSE}. AT DUSK SOMETHING STANDS IN THE WINDOW. FOLK SAY IT IS {GHOST}.");
      R.push_back("MY MOTHER SAYS {GHOST} NEVER LEFT A JOB HALF DONE. NOW SOMETHING MOANS IN THE OLD HOUSE AT {HOUSE} EVERY NIGHT.");
      break;
  }
  if (c.variant != V_POST && t != Role::Priest && t != Role::Jarl && t != Role::Mage && t != Role::Guard)
    R.push_back("A FIGURE WITH NO LIGHT WALKS THE ROAD FROM {HOUSE} AT NIGHT. FOLK CALL IT {GHOST}.");
  if (c.variant == V_UNBURIED && c.war) R = {"NOBODY HAD TIME TO BURY EVERYONE AFTER THE FIGHTING. {GHOST} FELL AT {HOUSE} AND HAS NO GRAVE."};
  // HOAX needs a sign a family could produce: only the cows-and-window, the moaning, the figure, the empty beds
  if (c.variant == V_HOAX) {
    R.clear();
    if (t == Role::Innkeeper) R.push_back("NOBODY BUYS A BED AFTER DARK SINCE {GHOST} STARTED WALKING AT {HOUSE}. I AM SELLING CANDLES AT A LOSS.");
    else {
      R.push_back("THE COWS WON'T CROSS THE FIELD BY {HOUSE}. AT DUSK SOMETHING STANDS IN THE WINDOW. FOLK SAY IT IS {GHOST}.");
      R.push_back("MY MOTHER SAYS {GHOST} NEVER LEFT A JOB HALF DONE. NOW SOMETHING MOANS IN THE OLD HOUSE AT {HOUSE} EVERY NIGHT.");
    }
  }
  const size_t ri = (size_t)d.below((int)R.size());
  const std::string reason = R[ri];
  P["reason"] = reason;
  // the hoax's sign: the family explains the very sign the reason named
  if (reason.find("BED") != std::string::npos) P["sign"] = "WE MOAN FROM THE CELLAR AT DUSK. TRAVELLERS RUN. WE KEEP THE ROOF.";
  else if (reason.find("COWS") != std::string::npos) P["sign"] = "THE COWS FLEE MY SINGING. THE WINDOW LAMP IS A LANTERN.";
  else if (reason.find("MOANS") != std::string::npos) P["sign"] = "THE MOANING IS ME. I SING BADLY. THE CHILDREN SAY I SOUND DEAD ALREADY.";
  else P["sign"] = "THE SHEET IS OUR BEST ONE. I WALK THE ROAD WITH NO LAMP.";
  P["lastday"] = c.season == 3 ? "WE ARE TEN DAYS FROM THE THAW." : c.war ? "THE FIGHTING BURNED OUR FARM." : "THE RENT ROSE. THE ROOF WAS FREE.";
  P["ask"] = d.pick({"FIND OUT WHAT {GHOST} WANTS.", "GO IN AND GIVE {GHOST} PEACE, OR DRIVE {GHOST} OUT.", "SPEAK TO {GHOST}, AND THEN FIX IT.", "MAKE THE WALKING STOP."});
  P["desc"] = d.pick({"THE DEAD {GHOST} WALKS AT {HOUSE}, {HOUSE.DIR}. LEARN WHAT BINDS {GHOST}, END THE HAUNTING AND TELL {GIVER}.",
                      "{GIVER} OF {HOME} WANTS {GHOST} AT PEACE. {HOUSE} LIES {HOUSE.DIR}. ASK THE DEAD WHAT IT WANTS."});
  // the giver's words at the end, by trade
  switch (t) {
    case Role::Priest: P["riteline"] = "BRING THE BONES TO THE CHURCHYARD. I WILL SAY THE WORDS MYSELF TONIGHT."; break;
    case Role::Jarl: P["riteline"] = "THE BONES WILL HAVE A PLOT BY THE WALL. I'LL SEE TO IT."; break;
    case Role::Innkeeper: P["riteline"] = "BONES? GIVE THEM HERE. I'LL CARRY THEM TO THE PRIEST MYSELF. ...I SLEPT THROUGH LAST NIGHT, YOU KNOW."; break;
    case Role::Guard: P["riteline"] = "THE WATCH WILL CARRY THE BONES TO HOLY GROUND. WITH HONOURS."; break;
    case Role::Mage: P["riteline"] = "{HOUSE} IS WARM AGAIN. I MEASURED IT THIS MORNING. GIVE ME THE BONES."; break;
    default: P["riteline"] = "I WILL CARRY THEM TO THE PRIEST. THE BELLS HAVE STOPPED. THANK YOU."; break;
  }
  P["ritemem"] = t == Role::Priest ? "THE BELL SLEEPS AT NIGHT NOW. SO DO I." : "{HOUSE} IS QUIET. NOBODY TALKS ABOUT IT NOW.";
  P["aloudline"] = t == Role::Priest ? "THE CHURCH OF THOSE DAYS WAS WRONG." : t == Role::Jarl ? "THE HALL OF THOSE DAYS WAS WRONG."
                   : t == Role::Guard ? "THE WATCH OF THOSE DAYS WAS WRONG." : "THIS INN SWORE FALSE ONCE.";
  P["driveline"] = t == Role::Priest ? "A HARD THING. I WILL SEE THEM FED." : t == Role::Innkeeper ? "I WILL LEAVE A BOWL AT THE GATE FOR THEM."
                                                                                                  : "THE LAW IS THE LAW. IT STILL STINGS.";
  P["bringline"] = t == Role::Priest ? "THE CHAPEL LOFT IS DRY. COME IN, ALL OF YOU." : t == Role::Innkeeper ? "THE LOFT IS WARM. PEEL POTATOES."
                   : t == Role::Jarl ? "THE BARN HAS A STOVE. MIND IT." : "...I WILL FIND A ROOF FOR THEM. SOMEHOW.";
  std::vector<std::string> T = {"A HOUSE THAT WAITS", "THE DEAD WALK", "WHAT THE DEAD WANT", "WHO WALKS THERE?", "KNOCKING IN THE DARK"};
  if (cold) T.push_back("LONG NIGHTS");
  if (c.variant == V_POST) T.push_back("STILL ON GUARD");
  if (t == Role::Priest) T.push_back("THE BELL RINGS ITSELF");
  P["title"] = T[(size_t)d.below((int)T.size())];
}

bool choose(const JobCtx& w, uint64_t dice, JobCtx& out) {
  Dice d(dice);
  out = w;
  const Role t = (Role)w.trade;
  if (t != Role::Priest && t != Role::Mage && t != Role::Innkeeper && t != Role::Guard && t != Role::Jarl && t != Role::Farmer && t != Role::Villager) return false;
  // the want, from the giver's pool: UNBURIED 3, WRONGED 2 (an office), POST 2 (a watch, a gate, a door); then HOAX
  // one time in five on the reasons a family could fake (farmers, villagers, innkeepers)
  std::vector<int> pool = {V_UNBURIED, V_UNBURIED, V_UNBURIED};
  if (t == Role::Priest || t == Role::Jarl || t == Role::Guard || t == Role::Innkeeper) { pool.push_back(V_WRONGED); pool.push_back(V_WRONGED); }
  if (t == Role::Guard || t == Role::Jarl || t == Role::Innkeeper) { pool.push_back(V_POST); pool.push_back(V_POST); }
  out.variant = pool[(size_t)d.below((int)pool.size())];
  if ((t == Role::Farmer || t == Role::Villager || t == Role::Innkeeper) && d.below(5) == 0) out.variant = V_HOAX;
  if (w.war && out.variant != V_HOAX) out.variant = V_UNBURIED;   // after the fighting: the unburied dead
  return true;
}

JobCtx walk(Role t, int variant, int season) {
  JobCtx c;
  c.trade = (int)t; c.variant = variant; c.season = season;
  return c;
}

}  // namespace

const KindDef& kindHaunt() {
  static const KindDef K = [] {
    KindDef k;
    k.id = "haunt";
    k.title = "A HOUSE THAT WAITS";
    k.lead = "WANTS A HAUNTING ENDED";
    k.body = kBody;
    k.variants = {"unburied", "wronged", "post", "hoax"};
    k.trades = {(int)Role::Priest, (int)Role::Mage, (int)Role::Innkeeper, (int)Role::Guard, (int)Role::Jarl, (int)Role::Farmer, (int)Role::Villager};
    k.choose = choose;
    k.prep = prep;
    k.walks = {walk(Role::Mage, V_UNBURIED, 2), walk(Role::Priest, V_WRONGED, 3), walk(Role::Guard, V_POST, 0), walk(Role::Farmer, V_HOAX, 3),
               walk(Role::Innkeeper, V_HOAX, 1)};
    return k;
  }();
  return K;
}

}  // namespace radiant
}  // namespace story
