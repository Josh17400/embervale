// P1b radiant job kind 11: CHAMPION'S BOUT (content: jobs_c.md section 1; judge notes resolved). JOBS lane.
//
// A house, a trade, an inn or a quay has a quarrel that custom settles by a bout, and the giver cannot fight. The player
// stands as champion before the rival's yard, to the yield (BOUT RULES: the bout goal; the yield and the knock-down are
// the ENGINE lane's), and the quarrel is not always what the giver said. Every grievance (fifteen, by giver: the lord's
// words, grave and bull; the watch's bridge and gate; the merchant's tally, cargo and pitch; the smith's iron, contract
// and mark; the innkeeper's ladle, cask and sign; the harbour's berth, dues and salvage) keys the reason, the win, the
// loss and the prize, so no line belongs to another quarrel. Twists, about half the time:
//   A THE BAD CLAIM (words, grave, bull, tally, cargo, berth, dues): the rival's side of it: fight anyway, withdraw and
//     tell the giver (shamed, then grateful), or carry a fair settlement home (a name the rival has heard of, fame 5)
//   B THE HURT CHAMPION (the watch's and the lord's honour bouts, the smith's steel, the ladle): wait three days, pay a
//     bonesetter, or fight now
//   C THE LOADED WEAPON (trade, steel, the quay, the ladle): the rival's second offers a favour that is a trap; a trained
//     eye catches it (background noble, blacksmith, sailor, farmhand), and the yard concedes the quarrel
// Losing is not dying: the rival kneels by the player, the yard carries them to the inn, and the giver is kind. The
// culture-only grievances (the holmgang, the highland green) are not used: no line names a ground or a custom it may not
// have (judge). The repeat-giver CHALLENGE needs a memory of bouts won across jobs: not in this kind.
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
role yard site [[=yardtype]] near
role rival person [[male|female]] at yard
{?danger
role sibling person [[male|female]] at yard
}
{?tw_loaded
role second person [[male|female]] at yard
}

stage start
  talk giver
  say "[[=reason]] [[=ask]] [[=pay]]"
{?v_steel
  opt "[[I'LL CARRY YOUR BLADE.|LEND ME THE BLADE. I'LL STAND.]]" -> lend
}
{!v_steel
  opt "[[I'LL STAND FOR YOU.|I'LL BE YOUR CHAMPION.|TO THE YIELD, THEN.]]" -> go
}
  opt "[[NOT MY QUARREL.|NOT TODAY.|FIND ANOTHER CHAMPION.]]" -> decline

{?v_steel
stage lend
  talk giver
  say "HERE. MY BEST WORK. THE JOURNEYMAN WILL ASK TO SEE IT, AND THE YARD WILL JUDGE IT AS MUCH AS YOU. WIELD IT WELL."
  journal "{GIVER} LENT YOU A BLADE FOR THE BOUT AT {YARD}."
  do give "{GIVER}'S BLADE" sword
  opt "[[IT WILL NOT SHAME YOU.|A FINE BLADE.]]" -> go
}

stage go
  goal goto yard
  journal "{GIVER} OF {HOME} NEEDS A CHAMPION. FIGHT {RIVAL} AT {YARD}, {YARD.DIR}, TO THE YIELD."
  then parley

stage parley
  talk rival
  say "[[=parley]]"
  journal "{RIVAL} WAITS IN THE YARD AT {YARD}. TO THE YIELD."
{?v_steel
  opt "I STAND FOR {HOME}." if have "{GIVER}'S BLADE" -> bout
  opt "[[THE BLADE IS LOST. I'LL GET ANOTHER.|I'LL FETCH ANOTHER BLADE.]]" -> noblade
}
{!v_steel
  opt "I STAND FOR {HOME}." -> bout
}
  opt "[[IS THERE NO OTHER WAY?|MUST IT BE A BOUT?]]" -> [[=otherway]]
  opt "[[NOT TODAY.|I WITHDRAW.]]" -> walkaway

{?v_steel
stage noblade
  talk giver
  say "YOU LOST MY BLADE? ...HERE. THE OTHER ONE. IT IS NEARLY AS GOOD. DO NOT LOSE THIS ONE."
  journal "THE JOURNEYMAN WANTS TO SEE {GIVER}'S BLADE. FETCH ANOTHER FROM {GIVER}."
  do give "{GIVER}'S BLADE" sword
  opt "[[I WON'T.|THANK YOU.]]" -> parley2

stage parley2
  talk rival
  say "THAT IS THE BLADE? GOOD. DRAW IT. LET THE YARD SEE IT."
  journal "{RIVAL} WAITS IN THE YARD AT {YARD}. TO THE YIELD."
  opt "I STAND FOR {HOME}." -> bout
}

{?tw_none
stage noway
  talk rival
  say "THERE IS THE BOUT. THAT IS THE WAY. NO SPEECHES. THE YARD IS WATCHING."
  journal "{RIVAL} WILL HAVE THE BOUT."
  opt "I STAND FOR {HOME}." -> bout
  opt "[[THEN NOT TODAY.|I WITHDRAW.]]" -> walkaway
}

{?tw_claim
stage claim
  talk rival
  say "[[=claim]]"
  journal "{RIVAL} SAYS THE QUARREL IS NOT WHAT {GIVER} TOLD YOU."
  opt "[[I FIGHT FOR WHO PAYS ME.|STILL. THE BOUT.]]" -> bout_paid
  opt "THEN I WITHDRAW, AND TELL THEM." -> withdrew
  opt "LET ME ASK PEACE OF THEM." check fame 5 -> peace else peace_fail

stage peace_fail
  talk rival
  say "YOU HAVE A KIND FACE AND NO NAME. THE BOUT."
  journal "{RIVAL} WON'T HEAR PEACE FROM A STRANGER."
  opt "THE BOUT, THEN." -> bout

stage bout_paid
  goal bout rival
  say "THEN YOU KNOW WHAT YOU FIGHT FOR. SO DOES THE YARD."
  journal "BEAT {RIVAL} TO THE YIELD AT {YARD}. THE YARD KNOWS WHAT YOU CHOSE."
  do remember rival "YOU KNEW, AND FOUGHT ANYWAY. FOR PAY."
  then [[=afterwin]]
  else lost

stage peace
  talk rival
  say "THEY WOULD TAKE THAT? [[=settle]] THEN THAT IS THE END OF IT. NOBODY BLEEDS TODAY. I AM GLAD IT WAS YOU THEY SENT."
  journal "CARRY {RIVAL}'S SETTLEMENT HOME TO {GIVER} IN {HOME}."
  do remember rival "YOU CARRIED PEACE BETWEEN US. I WON'T FORGET IT."
  opt "[[I'LL TELL THEM.|PEACE, THEN.]]" -> report_peace

stage withdrew
  talk giver
  say "[[=withdrewline]]"
  journal "YOU WITHDREW FROM THE BOUT AT {YARD}. TELL {GIVER} WHY."
  opt "[[IT HAD TO BE SAID.|THEY DESERVED THE TRUTH.]]" -> withdrew_end

stage report_peace
  talk giver
  say "A SETTLEMENT, AND NOBODY BLED. [[=settle]] ...I WOULD NOT HAVE THOUGHT OF IT. YOU ARE WORTH TWICE WHAT I PAY YOU."
  journal "TELL {GIVER} THE TERMS OF THE PEACE."
  opt "[[PEACE IS CHEAPER.|EVERYONE WALKS HOME.]]" -> peace_end
}

{?tw_hurt
stage hurt
  talk rival
  say "DO NOT LOOK AT THE ARM. A MULE SAT ON IT. ONLY MY SECOND KNOWS, AND NOW YOU. IF I REFUSE THE BOUT, MY SIDE LOSES. IF I FIGHT, I LOSE MORE. BUT I WILL FIGHT."
  journal "{RIVAL}'S ARM IS IN A SLING."
  opt "I'LL WAIT THREE DAYS." -> wait3
  opt "I'LL PAY A BONESETTER. (30 GOLD)" if gold 30 -> setter
  opt "[[THEN WE FIGHT NOW.|NOW, THEN.]]" -> bout

stage wait3
  goal wait 3
  say "THREE DAYS. I WILL EAT LIKE A BULL."
  journal "WAIT THREE DAYS IN {YARD} FOR {RIVAL}'S ARM TO MEND."
  do remember rival "YOU WAITED THREE DAYS FOR MY ARM. THE YARD TALKS OF NOTHING ELSE."
  then bout_fair

stage setter
  talk rival
  say "THE ONE BY THE WELL. RUDE, BUT GOOD. ...THERE. SET, AND BOUND. NOW WE CAN DO THIS PROPERLY."
  journal "THE BONESETTER SET {RIVAL}'S ARM."
  do gold -30
  do remember rival "YOU PAID FOR MY ARM, THEN BEAT ME FAIRLY. STRANGE CHAMPION."
  opt "[[TO THE YIELD.|NOW WE FIGHT.]]" -> bout_fair

stage bout_fair
  goal bout rival
  journal "BEAT {RIVAL} TO THE YIELD AT {YARD}. THE YARD HEARD OF YOUR FAIR PLAY."
  do xp 20
  then [[=afterwin]]
  else lost
}

{?tw_loaded
stage offer
  talk second
  say "[[=offerline]]"
  journal "{RIVAL}'S SECOND OFFERS YOU A FAVOUR BEFORE THE BOUT."
  opt "[[=decline]]" -> bout
  opt "[[=lookopt]]" check bg [[=eye]] -> caught else fine
  opt "[[=takeopt]]" -> trap

stage fine
  talk second
  say "LOOKS FINE TO ME, FRIEND. WELL?"
  journal "{RIVAL}'S SECOND OFFERS YOU A FAVOUR BEFORE THE BOUT."
  opt "[[=decline]]" -> bout
  opt "[[=takeopt]]" -> trap

stage trap
  goal bout rival
  say "[[=trapline]]"
  journal "BEAT {RIVAL} TO THE YIELD AT {YARD}, WITH A TRICK IN YOUR HAND."
  then trap_won
  else lost

stage trap_won
  talk rival
  say "ON MY HONOUR, I KNEW NOTHING OF THAT. THE YARD HAS TURNED ON MY SECOND. KEEP MY HAND. AND THIS."
  journal "{RIVAL} KNEW NOTHING OF THE TRICK. THE YARD TURNED ON THE SECOND."
  do gold 20
  do remember rival "MY OWN SECOND CHEATED. YOU WON ANYWAY. I OWE YOU."
  opt "[[NO MALICE.|A FAIR BOUT, IN THE END.]]" -> [[=afterwinb]]

stage caught
  talk rival
  say "[[=caughtline]] ...I KNEW NOTHING OF THIS. THE BOUT IS YOURS. THE SHAME IS MINE, NOT YOURS."
  journal "YOU CAUGHT THE SECOND'S TRICK. {RIVAL} CONCEDED THE QUARREL."
  opt "[[NO MALICE.|THE SHAME IS YOUR SECOND'S.]]" -> report_caught

stage report_caught
  talk giver
  say "[[=caughtgiver]]"
  journal "TELL {GIVER} THE QUARREL WAS CONCEDED."
  opt "[[THE YARD SAW IT ALL.|THEY CHEATED.]]" -> caught_end
}

stage bout
  goal bout rival
  say "NO SPEECHES. THE YARD IS WATCHING. TO THE YIELD, AND I KEEP MY WORD."
  journal "BEAT {RIVAL} TO THE YIELD IN THE YARD AT {YARD}."
  then [[=afterwin]]
  else lost

{?danger
stage sibling_bout
  goal bout sibling
  say "I AM {RIVAL}'S SIBLING. BEAT ME TOO, OR THE YARD CALLS IT A DRAW."
  journal "{SIBLING}, {RIVAL}'S SIBLING, STEPS INTO THE YARD. BEAT {SIBLING} TOO."
  then yielded
  else lost
}

stage yielded
  talk rival
  say "ENOUGH. YOU HAVE IT. THAT WAS WELL FOUGHT, STRANGER. TELL THEM I BEAR NO MALICE."
  journal "{RIVAL} YIELDED. TELL {GIVER} IN {HOME}."
  opt "[[NOR I.|WELL FOUGHT.]]" -> report_won

stage report_won
  talk giver
  say "[[=winline]]"
  journal "TELL {GIVER} IN {HOME} THAT THE BOUT IS WON."
  opt "[[THE BOUT IS WON.|{RIVAL} YIELDED.]]" -> won_end

stage lost
  talk rival
  say "STAY DOWN. YOU HAVE EARNED THE REST. THAT WAS A GOOD BOUT. REMEMBER IT. THE YARD WILL CARRY YOU TO THE INN. [[=tail]]"
  journal "YOU LOST THE BOUT. TELL {GIVER}."
  opt "[[...|GOOD BOUT.]]" -> lost_rest

stage lost_rest
  goal wait 1
  journal "THE YARD CARRIED YOU TO THE INN AT {YARD}. REST A NIGHT."
  then report_lost

stage report_lost
  talk giver
  say "[[=lostline]]"
  journal "YOU LOST THE BOUT. TELL {GIVER} IN {HOME}."
  opt "[[I'M SORRY.|NEXT TIME.]]" -> lost_end

stage walkaway
  end fail
  journal "YOU WALKED AWAY FROM THE BOUT AT {YARD}. {GIVER} WILL NOT FORGET IT."
  do remember giver "YOU WALKED AWAY FROM MY BOUT. I HAVE NOTHING TO SAY TO YOU."

stage won_end
  end success
  journal "YOU STOOD FOR {GIVER} AT {YARD} AND WON TO THE YIELD."
  do reward fair [[=winpay]]
[[=winfx]]
  do remember giver "[[=winmem]]"
  do fact "A CHAMPION OF {HOME} WON THE BOUT AT {YARD}."

stage lost_end
  end fail
  journal "YOU STOOD FOR {GIVER} AT {YARD} AND LOST THE BOUT. NO GOLD; NO SHAME."
  do xp 20
  do remember giver "YOU STOOD FOR ME AND FELL. NO SHAME IN IT."

{?tw_claim
stage withdrew_end
  end success
  journal "YOU WITHDREW AND TOLD {GIVER} THE TRUTH OF THE QUARREL. HALF THE FEE, AND {GIVER}'S GRATITUDE."
  do reward fair 65
  do xp 80
  do remember giver "YOU MADE ME LOOK AT IT. I DIDN'T THANK YOU THEN. I DO NOW."

stage peace_end
  end success
  journal "YOU CARRIED A SETTLEMENT FROM {YARD} TO {HOME}. NOBODY BLED."
  do reward fair 130
  do xp 100
  do fame 1
  do remember giver "A SETTLEMENT, AND NOBODY BLED. I TELL IT AT EVERY FEAST."
}

{?tw_loaded
stage caught_end
  end success
  journal "YOU CAUGHT A CHEAT BEFORE THE BOUT AT {YARD}. THE QUARREL WAS CONCEDED TO {GIVER}."
  do reward fair 130
  do xp 60
[[=caughtfx]]
  do remember giver "THEY LOADED A WEAPON TO WIN. YOU CAUGHT IT. THE YARD WON'T FORGET."
}

stage decline
  end fail
  journal "YOU TURNED DOWN {GIVER}'S QUARREL."
)DSL";

enum { V_HONOUR, V_TRADE, V_STEEL, V_LADLE, V_SEA };
enum { T_NONE, T_CLAIM, T_HURT, T_LOADED };
enum { G_WORDS, G_GRAVE, G_BULL, G_BRIDGE, G_GATE, G_TALLY, G_CARGO, G_PITCH, G_IRON, G_CONTRACT, G_MARK, G_LADLE, G_CASK, G_SIGN, G_BERTH, G_DUES,
       G_SALVAGE };

struct Griev {
  const char* id;
  const char* reason;
  const char* win;
  const char* lost;
  const char* title;
  const char* claim;      // twist A: the rival's side ("" none)
  const char* settle;     // twist A: the fair settlement
  const char* withdrew;   // twist A: the giver, shamed then grateful
};
const Griev kG[] = {
    {"words", "{RIVAL} OF {YARD} CALLED MY HOUSE OATH-BREAKERS BEFORE A FULL HALL. CUSTOM SAYS ONLY A BOUT ANSWERS THAT, AND MY SWORD ARM IS PAST IT.",
     "THE WHOLE HALL WENT QUIET. THEN LOUD. NO ONE CALLS MY HOUSE OATH-BREAKERS NOW. SIT AT MY TABLE, CHAMPION.",
     "I HEARD. THE WHOLE HALL HEARD, I THINK. YOU STOOD FOR ME AND FELL. NO SHAME IN IT. SIT BY MY FIRE.", "A NAME TO ANSWER",
     "I SAID THE WORDS, AND THEY ARE TRUE. IN THE FAMINE, MY FAMILY LENT YOUR LORD'S FATHER SEED-GRAIN FOR SPRING. IT WAS NEVER PAID. ASK YOUR LORD.",
     "THE SEED-GRAIN RETURNED AT HARVEST, DOUBLED, AND A PLAIN WORD SPOKEN IN THE HALL.",
     "THE SEED-GRAIN. I WAS A CHILD. I SAW THE SACKS IN OUR BARN, AND I LOOKED AWAY. I HAVE LOOKED AWAY SINCE. YOU MADE ME LOOK. I WILL PAY IT BACK."},
    {"grave", "AT THE FEAST OF THE DEAD, {RIVAL} OF {YARD} KICKED OVER MY FATHER'S GRAVE-STONE AND CALLED IT A SLIP. MY LEG IS LAME. CUSTOM LETS ME NAME A CHAMPION.",
     "THE STONE STANDS AGAIN. {RIVAL} RAISED IT WITH {RIVAL.HIS} OWN HANDS. MY FATHER SLEEPS EASIER. SO DO I.",
     "THE STONE STAYS WHERE IT WAS KICKED. YOU STOOD FOR MY FATHER, AND THAT IS MORE THAN MY OWN CHILDREN DID. SIT AND REST.", "A HOUSE'S HONOUR",
     "I KICKED THE STONE. I WAS DRUNK AT MY OWN CHILD'S WAKE, AND IT WAS DARK. THREE TIMES I WALKED TO YOUR LORD'S GATE TO SAY SORRY. THE STEWARD TURNED ME OUT.",
     "I RAISE THE STONE WITH MY OWN HANDS, AND PAY FOR A NEW CARVING.",
     "THREE TIMES, AT MY OWN GATE? THE STEWARD SAID NOT A WORD OF IT. I HEARD ONLY THAT THE STONE FELL. I WILL ASK THEM IN. TELL THEM SO."},
    {"bull", "{RIVAL} OF {YARD} SAYS OUR WHITE BULL WAS BRED FROM THEIRS. THE ELDERS SAY A BOUT DECIDES. MY CHILDREN ARE AWAY AND I AM TOO OLD.",
     "THE BOUT HAS SPOKEN. THE WHITE BULL IS OURS, AND THE ELDERS NODDED. COME EAT. THERE IS A WHOLE SHEEP.",
     "THE YARD FAVOURED THEM, THEN. WE'LL GIVE THEM A CALF AND CALL IT KIN. YOU FOUGHT WELL. HAVE SOME MUTTON.", "THE WHITE BULL",
     "THERE IS A BETTER WAY THAN A BRUISE. MY GRANDFATHER LENT YOURS OUR WHITE BULL FOR ONE SEASON. THE FIRST CALF WAS TO COME HOME. IT NEVER DID.",
     "THE FIRST CALF SENT HOME, AND THE TWO FAMILIES SHARE A FEAST.",
     "THE FIRST CALF? THAT CALF SIRED MY WHOLE HERD. I HAD FORGOTTEN THE LOAN. THEY SHALL HAVE A CALF, AND A FEAST. THE WHITE BULL IS KIN, AFTER ALL."},
    {"bridge", "A SERGEANT OF MINE HELD A BRIDGE AND DIED THERE. {RIVAL} OF {YARD} CALLS THE SERGEANT A RUNAWAY. THE WATCH WILL ANSWER FOR THE DEAD.",
     "THE SERGEANT'S NAME IS CLEAN TODAY. THE WATCH STOOD UP WHEN YOU CAME IN. THEY WILL STAND AGAIN IF YOU RETURN.",
     "THE SERGEANT STAYS CALLED A COWARD. BUT YOU STOOD ON THE BRIDGE FOR THEM. THAT IS ALL ANY OF US CAN DO.", "THE WATCH ANSWERS", "", "", ""},
    {"gate", "{RIVAL} OF {YARD} SAYS MY GATE-WARDEN OPENED THE POSTERN TO RAIDERS. THE WARDEN WAS FOUND AT THE POST WITH A SPEAR-WOUND. THE WATCH ANSWERS FOR ITS OWN.",
     "THE WARDEN WALKED THE WALL TODAY WITH HEAD HIGH. THE SCAR SPEAKS NOW. THE WATCH OWES YOU A FAVOUR.",
     "THE WARDEN STILL CARRIES THE WORD. THE WATCH WILL TELL THE TRUTH ITSELF IN TIME. THANK YOU FOR STANDING.", "THE WATCH ANSWERS", "", "", ""},
    {"tally", "A HOUSE AT {YARD} SWEARS IT PAID ME, AND I SWEAR IT DID NOT. NEITHER OF US HAS THE TALLY. THEIR CHAMPION {RIVAL} WAITS, AND I NEED MINE.",
     "THEY YIELDED THE DEBT AT THE KNEE. NOW THE HOUSE MUST PAY, AND PAY WELL. A TENTH OF IT IS YOURS, AS PROMISED.",
     "SO THE DEBT STANDS AS THEY SAY. NEVER MIND. THE LEDGER OUTLIVES US BOTH.", "PAID OR NOT PAID",
     "THE DEBT WAS PAID, IN FULL, IN WOOL. HERE IS MY HOUSE'S HALF OF THE TALLY. THE OTHER HALF IS IN YOUR MERCHANT'S STRONGBOX. ASK TO SEE THEM MATCHED.",
     "BOTH HALVES OF THE TALLY MATCHED BEFORE A CLERK, AND THE HALF-FEE PAID.",
     "THE TALLY MATCHES? THEN THEY PAID ME, AND I SHOUTED FOR A WEEK. SHAMEFUL. HALF A FEE FOR HONESTY. SPEND IT WELL."},
    {"cargo", "THE HOUSE AT {YARD} WILL NOT PAY FOR FIFTY SACKS OF MY SALT. THEY SAY IT CAME DAMP. MY CARTER SWEARS IT WAS DRY. THEIR CHAMPION {RIVAL} WAITS.",
     "THEY YIELDED. THE SALT IS PAID FOR, EVERY SACK. THE REEVE WROTE IT DOWN. A TENTH OF IT IS YOURS, AS PROMISED.",
     "SO THE SALT IS WRITTEN OFF. FIFTY SACKS. I WILL SURVIVE, AND REMEMBER.", "DAMP SALT, DRAWN BLADE",
     "THE SALT WAS DAMP. HERE IS A SACK, SEALED AT THE QUAY BY THE REEVE. YOUR CARTER LEFT THE LOAD UNCOVERED IN THE RAIN FOR TWO DAYS. BREAK IT.",
     "ONE SACK BROKEN BEFORE THE REEVE, AND THE LOSS SPLIT.",
     "SEALED AT THE QUAY, AND DAMP IN THE SACK? MY CARTER LEFT IT IN THE RAIN. THEN THE LOSS IS MINE. I WILL SEND THEM A DISCOUNT."},
    {"pitch", "THE FRONT PITCH AT THE {YARD} FAIR GOES TO THE HOUSE WHOSE CHAMPION WINS. {RIVAL} HAS HELD IT SEVEN YEARS RUNNING, AND I WANT IT FOR MY GOODS.",
     "THE FRONT PITCH IS MINE! THE FAIR ROARED. SEVEN YEARS I SOLD IN THEIR SHADOW. NOW THEY SELL IN MINE.",
     "ANOTHER YEAR IN THEIR SHADOW. WELL. THE FAIR RUNS ON. I'LL SELL FROM THE BACK, AND LOUDER.", "THE FRONT PITCH", "", "", ""},
    {"iron", "THE SMITH AT {YARD} CALLS MY BLADES NAIL-IRON. THEIR CHAMPION {RIVAL} WILL CARRY THEIR SWORD AGAINST ONE OF MINE. YOU CARRY MINE.",
     "THEIR BLADE IS YOURS. LOOK AT IT. NAIL-IRON, THEY SAID. NOW THEIR SMITH ASKS HOW I TEMPER. I WILL NOT SAY.",
     "MY STEEL FELL TO THEIRS. IT HAS HAPPENED BEFORE. I WILL FORGE MORE. KEEP THE BLADE. IT DESERVES A HAND.", "SETTLED BY STEEL", "", "", ""},
    {"contract", "THE GARRISON BUYS ALL ITS SWORDS FROM ONE FORGE, AND THE CAPTAIN WILL NOT CHOOSE BETWEEN MINE AND {YARD}'S. {RIVAL} CARRIES THEIR BLADE.",
     "THE GARRISON TAKES MY SWORDS THIS YEAR, ALL OF THEM. THE CAPTAIN SAYS THE YARD HAS CHOSEN. SO HAVE I.",
     "THE GARRISON TAKES THEIR SWORDS. FINE. I WILL SELL TO FARMERS AND FOOLS. KEEP THE BLADE. IT DID NOT FAIL YOU.", "THE GARRISON'S SWORDS", "", "", ""},
    {"mark", "THE SMITH AT {YARD} WAS MY APPRENTICE AND NOW STAMPS MY MARK ON THEIR BLADES. THE GUILD SAYS A BOUT DECIDES WHOSE MARK IT IS.",
     "MY MARK IS MINE AGAIN. THE GUILD STRUCK THEIR STAMP FROM THE ROLL. LET THEM FIND A MARK OF THEIR OWN.",
     "THEY KEEP MY MARK, THEN. LET THEM WEAR IT WELL. I WILL CUT A NEWER ONE. KEEP THE BLADE. IT IS YOURS NOW.", "WHOSE MARK IS IT?", "", "", ""},
    {"ladle", "{HOME} AND {YARD} HAVE STOLEN ONE DENTED LADLE BACK AND FORTH FOR FORTY YEARS. EVERY HARVEST FAIR IT GOES TO THE STAVES, AND THEIR {RIVAL} HAS NEVER LOST.",
     "THE LADLE! THE ACTUAL LADLE! KEEP IT UNTIL NEXT HARVEST. THEY WILL STEAL IT BACK. THEY ALWAYS DO.",
     "LOST? TO A BAKER?! WELL. THEY KEEP THE LADLE ANOTHER YEAR. SIT. HAVE ALE, ON THE HOUSE. YOU EARNED THE BRUISES.", "THE LADLE BOUT", "", "", ""},
    {"cask", "THE FAIR AT {YARD} GIVES ITS ONE BEER-TENT TO THE INN WHOSE CHAMPION WINS ON PADDED STAVES. {RIVAL} HAS WON THREE YEARS RUNNING. MY CASKS SULK.",
     "THE BEER-TENT IS OURS! EVERY CASK IS ROLLING. DRINK UNTIL YOUR EARS RING. THE FIRST MUG IS YOURS.",
     "THEIR TENT AGAIN, AND MY CASKS SULK. SIT. THE FIRST MUG HERE IS YOURS. YOU EARNED EVERY BRUISE.", "THE FIRST BARREL", "", "", ""},
    {"sign", "THE INN AT {YARD} HAS PAINTED A SIGN LIKE MINE, DOWN TO THE GOOSE. TWO INNS, ONE NAME. THE REEVE SAYS STAVES, AND THE LOSER REPAINTS. {RIVAL} IS THEIR CHAMPION.",
     "THEY REPAINT TOMORROW. THE GOOSE IS MINE, AND THEY GET A DUCK. I TOLD THE REEVE. DRINK, FRIEND, DRINK.",
     "WE REPAINT. A DUCK! MY GOOSE, A DUCK! ...SIT. ALE ON THE HOUSE. YOU EARNED THE BRUISES.", "TWO INNS, ONE NAME", "", "", ""},
    {"berth", "{RIVAL}, A SHIP'S MASTER OUT OF {YARD}, TOOK A BERTH I HAD SOLD AND CALLED ME A LIAR ON MY OWN QUAY. SEA-RIGHT LETS ME NAME A BLADE TO ANSWER.",
     "THE BERTH IS MINE AND THE QUAY KNOWS IT. SEA-RIGHT HAS SPOKEN. NAME YOUR DRINK.",
     "THE BERTH GOES TO THAT BLOWHARD. YOU FOUGHT LIKE A SAILOR. THAT IS THE HIGHEST THING I CAN SAY.", "A BERTH BY BLADE",
     "THE BERTH WAS SOLD TWICE, FRIEND. HERE IS MY PAID CHIT, WITH THE SEAL. THE HARBOUR SOLD IT AGAIN, THEN CALLED ME THE THIEF.",
     "THE BERTH FEE REFUNDED, AND A DRINK.", "TWICE SOLD? MY CLERK, THE FOOL! TELL THE MASTER I OWE A BERTH AND A DRINK. YOU HAVE SAVED MY NAME."},
    {"dues", "{RIVAL}, A MASTER OUT OF {YARD}, SAILED WITHOUT PAYING HARBOUR DUES AND CALLED MY TOLL A ROBBERY. SEA-RIGHT SAYS A BLADE ON THE QUAY ANSWERS THAT.",
     "THE DUES ARE PAID, WITH INTEREST, AND THE MASTER APOLOGISED TO THE QUAY. SEA-RIGHT HOLDS. NAME YOUR DRINK.",
     "THE DUES ARE WAIVED FOR THAT BLOWHARD. WELL FOUGHT, LANDSMAN. THE HARBOUR WILL REMEMBER WHO STOOD ON THE QUAY.", "WHO PAYS THE DUES?",
     "THE OLD CHARTER FREES THE SHIPS OF {YARD} FROM DUES. HERE IS MY SEALED COPY. YOUR HARBOUR'S COPY BURNED IN THE FIRE, SO THEY SAY. LET US MATCH THEM.",
     "THE TWO CHARTERS MATCHED IN THE HARBOUR OFFICE, THE DUES REFUNDED.",
     "A CHARTER OLDER THAN MY CHAIR? THEN I HAVE TAXED THEM WRONG FOR TEN YEARS. I WILL REFUND IT AND WRITE IT DOWN."},
    {"salvage", "{RIVAL} TOWED A WRECK INTO MY HARBOUR AND CLAIMS HALF ITS CARGO. SEA-RIGHT ALLOWS A THIRD, NOT A HALF. SO WE SETTLE IT ON THE QUAY.",
     "A THIRD, BY SEA-RIGHT, AND NOT A BARREL MORE. THE MASTER SHOOK ON IT. THE CARGO IS SPLIT. NAME YOUR DRINK.",
     "HALF THE CARGO GOES TO THAT SHARK. I SHOULD HAVE HIRED A DEEPER SAILOR. SIT. THE TEA IS ON ME.", "A WRECK'S SHARE", "", "", ""},
};

int variantOf(int g) {
  switch (g) {
    case G_WORDS: case G_GRAVE: case G_BULL: case G_BRIDGE: case G_GATE: return V_HONOUR;
    case G_TALLY: case G_CARGO: case G_PITCH: return V_TRADE;
    case G_IRON: case G_CONTRACT: case G_MARK: return V_STEEL;
    case G_LADLE: case G_CASK: case G_SIGN: return V_LADLE;
    default: return V_SEA;
  }
}

void prep(const JobCtx& c, uint64_t seed, std::map<std::string, std::string>& P) {
  Dice d(seed);
  const Griev& G = kG[c.extra < (int)(sizeof(kG) / sizeof(kG[0])) ? c.extra : 0];
  const int v = c.variant;
  P["yardtype"] = v == V_LADLE ? "village" : "town";
  P["reason"] = G.reason;
  P["ask"] = d.pick({"STAND FOR ME ON THE DAY.", "WIN IT, AND I WILL OWE YOU.", "WIN IT, AND LET NOBODY BLEED MORE THAN THE TERMS."});
  P["pay"] = v == V_LADLE ? "THE PRIZE IS GLORY, ALE, AND A LITTLE COIN." : "THE FEE IS GOOD, AND THE YARD WILL KNOW YOUR NAME.";
  static const char* parley[] = {
      "SO THERE IS A CHAMPION AFTER ALL. I HAD HALF HOPED THERE WOULD NOT BE. TO THE YIELD. THEN WE KNEEL AND DRINK. ARE THE TERMS CLEAR?",
      "A CHAMPION. HOW TIDY. I AM PAID TO STAND HERE UNTIL SUNDOWN, WIN OR NOT. WE YIELD AT THE KNEE. NO ONE DIES FOR A BARGAIN. WILL YOU STAND?",
      "MY MASTER'S BLADE AGAINST YOURS. I'LL NOT SEE OUR FORGE SHAMED. IS THAT THE BLADE YOU WERE LENT? DRAW IT. LET THE YARD SEE IT.",
      "A NEW FACE! WE WERE TIRED OF BEATING THE OLD ONES. HELLO, STRANGER! THREE CLEAN HITS TAKES THE DAY. NO BITING, NO EYES, NO CRYING YET.",
      "THE QUAY HAS WAITED FOR YOU, LANDSMAN. OR ARE YOU A SAILOR? HARD TO TELL IN THAT HAT. BLADE AND BUCKLER, TO THE YIELD. THE HARBOUR IS WATCHING."};
  P["parley"] = parley[v];
  static const char* tails[] = {"GO HOME AND TELL THEM I WAS FAIR.", "THE CLAIM STANDS. YOU KNOW THE RULES.", "MY MASTER'S STEEL HELD. TELL YOURS.",
                                "THE PRIZE STAYS HERE! BRING THE BEER!", "THE QUAY IS MINE. NO HARD FEELINGS."};
  P["tail"] = tails[v];
  P["winline"] = G.win;
  P["lostline"] = G.lost;
  P["claim"] = G.claim;
  P["settle"] = G.settle;
  P["withdrewline"] = G.withdrew;
  P["otherway"] = c.twist == T_CLAIM ? "claim" : c.twist == T_HURT ? "hurt" : c.twist == T_LOADED ? "offer" : "noway";
  P["afterwin"] = c.danger ? "sibling_bout" : "yielded";
  P["afterwinb"] = c.danger ? "sibling_bout" : "report_won";
  P["winmem"] = "YOU STOOD FOR ME AT {YARD}. THE QUARREL IS SETTLED, AND I SLEEP.";
  // the pay and the prize (the innkeepers' bouts pay a third in coin, and fame and a keepsake)
  std::string fx;
  int pay = 130;
  switch (c.extra) {
    case G_TALLY: case G_CARGO: case G_PITCH: pay = 140; break;
    case G_IRON: case G_CONTRACT: case G_MARK: fx = "  do give \"THE {YARD} BLADE\" sword"; break;
    case G_LADLE: pay = 45; fx = "  do fame 5\n  do give \"THE DENTED LADLE\" gold"; break;
    case G_CASK: pay = 45; fx = "  do fame 5\n  do give \"A CASK OF FAIR ALE\" potion"; break;
    case G_SIGN: pay = 45; fx = "  do fame 5\n  do give \"THE PAINTED GOOSE-BOARD\" scroll"; break;
    default: break;
  }
  P["winpay"] = std::to_string(pay);
  P["winfx"] = fx;
  // twist C: the second's favour, the eye that catches it, the trap and the giver's line, by weapon
  switch (v) {
    case V_TRADE:
      P["offerline"] = "TAKE THIS ONE. YOURS LOOKS TIRED. IT HAS SERVED OUR HOUSE FOR YEARS. ONLY LEND IT. I WANT IT BACK.";
      P["decline"] = "I'LL USE MY OWN."; P["lookopt"] = "LET ME LOOK AT IT."; P["takeopt"] = "I'LL USE IT, THEN."; P["eye"] = "noble";
      P["caughtline"] = "THE POMMEL IS POURED WITH LEAD. IT WOULD PULL YOUR GUARD DOWN BY HALF.";
      P["trapline"] = "THE POINT DROPS A HAND'S WIDTH. THE BLADE IS TOO HEAVY AT THE END.";
      P["caughtgiver"] = "THEY LOADED A SWORD TO WIN? THEN THE YARD HAS HEARD IT. THEIR HOUSE LOST THE QUARREL BY ITS OWN HAND.";
      P["caughtfx"] = "";
      break;
    case V_STEEL:
      P["offerline"] = "THE GRIP-WRAP ON YOUR BLADE IS FRAYED. LET ME REWIND IT. IT TAKES A MOMENT. WE WANT A FAIR FIGHT, NOT A SLIP.";
      P["decline"] = "I'LL REWIND IT MYSELF."; P["lookopt"] = "LET ME LOOK AT THE WRAP."; P["takeopt"] = "VERY WELL, THEN."; P["eye"] = "blacksmith";
      P["caughtline"] = "THAT CORD IS WAXED WITH TALLOW. IT WOULD SLIP IN A SWEATING PALM.";
      P["trapline"] = "THE WRAP SLIDES. THE BLADE TWISTS IN YOUR HAND AND CUTS YOUR PALM.";
      P["caughtgiver"] = "A GREASED GRIP, AT THEIR OWN FORGE'S BOUT? THEY LOST THEIR OWN CASE. THEIR BLADE COMES TO ME AS FORFEIT.";
      P["caughtfx"] = "  do give \"THE {YARD} BLADE\" sword";
      break;
    case V_SEA:
      P["offerline"] = "NO BUCKLER OF YOUR OWN, LANDSMAN? TAKE THIS ONE. IT HAS SEEN A WAR. THE QUAY WOULD LIKE A FAIR FIGHT.";
      P["decline"] = "I'LL USE MY OWN."; P["lookopt"] = "LET ME LOOK AT IT."; P["takeopt"] = "I'LL USE IT, THEN."; P["eye"] = "sailor";
      P["caughtline"] = "THE STRAP IS CUT HALFWAY THROUGH. ONE HARD BLOW AND IT WOULD PART.";
      P["trapline"] = "THE STRAP PARTS ON THE FIRST BLOW. THE BUCKLER SPINS AWAY OVER THE QUAY.";
      P["caughtgiver"] = "A CUT STRAP, ON MY QUAY? THE HARBOUR WILL HEAR OF IT TONIGHT. THE QUARREL IS MINE. THE DRINKS, TOO.";
      P["caughtfx"] = "";
      break;
    default:
      P["offerline"] = "TAKE OUR SPARE STAVE. THE ASH IS GOOD. WELL OILED. IT WILL SING FOR YOU.";
      P["decline"] = "I'LL USE MY OWN."; P["lookopt"] = "LET ME LOOK AT IT."; P["takeopt"] = "I'LL USE IT, THEN."; P["eye"] = "farmhand";
      P["caughtline"] = "THIS STAVE IS LEADED. AN ASH ONE DOES NOT SWING LIKE A BELL.";
      P["trapline"] = "THE LEADED STAVE DRAGS AND TWISTS. YOUR WRIST FEELS IT AT ONCE.";
      P["caughtgiver"] = "A LEADED STAVE, IN A BOUT FOR FUN?! THE DAY IS YOURS. THE SHAME IS THEIRS FOR A GOOD LONG WHILE.";
      P["caughtfx"] = "";
      break;
  }
  std::vector<std::string> T = {"TO THE YIELD", "ONE FALL DECIDES", G.title};
  if (v == V_HONOUR) T.push_back("A HOUSE'S HONOUR");
  if (v == V_TRADE) T.push_back("A DEBT IN STEEL");
  if (v == V_STEEL) T.push_back("TEST OF THE BLADE");
  if (v == V_SEA) T.push_back("BLADES ON THE QUAY");
  P["title"] = T[(size_t)d.below((int)T.size())];
}

bool choose(const JobCtx& w, uint64_t dice, JobCtx& out) {
  Dice d(dice);
  out = w;
  std::vector<int> pool;
  switch ((Role)w.trade) {
    case Role::Jarl: pool = {G_WORDS, G_GRAVE, G_BULL}; if (w.season == 2) pool.push_back(G_GRAVE); break;
    case Role::Guard: pool = {G_BRIDGE, G_GATE}; break;
    case Role::Merchant: pool = {G_TALLY, G_CARGO}; if (w.season == 0 || w.season == 2) pool.push_back(G_PITCH); break;
    case Role::Smith: pool = {G_IRON, G_MARK}; if (w.tier >= 1) pool.push_back(G_CONTRACT); break;
    case Role::Innkeeper: pool = {G_SIGN}; if (w.season == 2) pool.push_back(G_LADLE); if (w.season == 0 || w.season == 2) pool.push_back(G_CASK); break;
    case Role::Fisher: if (!w.port) return false; pool = {G_BERTH, G_DUES}; if (w.season >= 2) pool.push_back(G_SALVAGE); break;
    default: return false;
  }
  out.extra = pool[(size_t)d.below((int)pool.size())];
  out.variant = variantOf(out.extra);
  // a twist about half the time, from those that fit the quarrel
  std::vector<int> tw;
  if (kG[out.extra].claim[0]) tw.push_back(T_CLAIM);
  if ((out.variant == V_HONOUR && (w.trade == (int)Role::Jarl || w.trade == (int)Role::Guard)) || out.variant == V_STEEL || out.variant == V_LADLE) tw.push_back(T_HURT);
  if (out.variant == V_TRADE || out.variant == V_STEEL || out.variant == V_SEA || out.variant == V_LADLE) tw.push_back(T_LOADED);
  out.twist = !tw.empty() && d.chance(50) ? tw[(size_t)d.below((int)tw.size())] : T_NONE;
  return true;
}

JobCtx walk(Role t, int g, int twist, int season, bool danger = false, bool port = false, int tier = 1) {
  JobCtx c;
  c.trade = (int)t; c.extra = g; c.variant = variantOf(g); c.twist = twist; c.season = season; c.danger = danger; c.port = port; c.tier = tier;
  return c;
}

}  // namespace

const KindDef& kindChampion() {
  static const KindDef K = [] {
    KindDef k;
    k.id = "champion";
    k.title = "CHAMPION'S BOUT";
    k.lead = "NEEDS A CHAMPION FOR A BOUT";
    k.body = kBody;
    k.variants = {"honour", "trade", "steel", "ladle", "sea"};
    k.twists = {"none", "claim", "hurt", "loaded"};
    for (const Griev& g : kG) k.extras.push_back(g.id);
    k.trades = {(int)Role::Jarl, (int)Role::Guard, (int)Role::Merchant, (int)Role::Smith, (int)Role::Innkeeper, (int)Role::Fisher};
    k.choose = choose;
    k.prep = prep;
    k.walks = {walk(Role::Jarl, G_WORDS, T_CLAIM, 2), walk(Role::Guard, G_BRIDGE, T_HURT, 1, true), walk(Role::Merchant, G_CARGO, T_LOADED, 0),
               walk(Role::Smith, G_IRON, T_LOADED, 3), walk(Role::Innkeeper, G_LADLE, T_HURT, 2, false, false, 0),
               walk(Role::Fisher, G_DUES, T_CLAIM, 3, false, true), walk(Role::Innkeeper, G_SIGN, T_NONE, 1)};
    return k;
  }();
  return K;
}

}  // namespace radiant
}  // namespace story
