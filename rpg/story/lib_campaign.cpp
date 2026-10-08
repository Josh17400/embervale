// M4 "Banners": the first campaign (owner 15.9: "a succession crisis", "a lost heir"; recurring characters, factions,
// betrayals, branching outcomes that change the world: who rules). STORY lane. The language: rpg/story/dsl.h.
//
// THE EMPTY THRONE. Cast from the player's real kingdom: its ruler as the realm records them, its capital and seat of
// power, its nearest rival kingdom, a town for the ruler's brother, a village for the hidden heir, a ruin whose dead
// keep the charter (its own record names the old lord), a bandit camp for the claimant's sellswords. A herald in the
// capital begins it.
//
//   herald -> steward -> the dying ruler (an audience in the seat of power) -> [the claimant at his hold]
//   -> the charter in the ruin -> the heir in her village, and the choice:
//     LOYAL      the heir is told the truth -> the claimant's captain is hunted -> the heir is presented at court
//                -> the rival's envoy wants its gold back: war, repayment or a stare -> the ruler dies -> CROWNED HEIR
//     BETRAYAL   (a pact with the claimant) the heir is led to him -> burn the charter or keep it -> the ruler dies
//                -> CROWNED CLAIMANT (with the rival's money behind him); a change of heart at the last hour turns
//                back to the loyal road
//     MERCY      the heir does not want a crown: the charter is burned, the steward is lied to -> CROWNED CLAIMANT
// Every ending changes the realm: the succession (Engine::rulerOf; a REALM forceSuccession is requested), the ruler's
// death and the rival's war or trade as realm events, reputation, marks the world remembers.
#include <string>
#include "rpg/story/dsl.h"

namespace story {
namespace dsl {

namespace {

const char* const kCampaign[] = {
R"DSL(
script emptythrone
title THE EMPTY THRONE
archetype A SUCCESSION CRISIS AND A LOST HEIR
tier 3
hook herald
pitch "WHAT IS IT, HERALD?"
hint "HEAR YE, HEAR YE! ALL IS WELL IN THE PALACE! ...A WORD, FRIEND. QUIETLY. YOU HAVE THE LOOK OF SOMEONE WHO CAN KEEP ONE."
role giver giver
role land kingdom home
role lord ruler land
role seat capital land
role rival kingdom rival
role steward person female at seat
role envoy person male at seat
role hold site town near
role claimant person male at hold
role hamlet site village near
role heir person female at hamlet
role crypt ruin near
role camp site camp near
role captain foe male at camp
var favour 0

stage herald
  talk giver
  say "BY ORDER OF THE STEWARD OF THE ROYAL HOUSEHOLD, QUIETLY: THE {LORD.TITLE} IS ILL, AND SWORDS OF GOOD NAME ARE ASKED TO THE PALACE. THE STEWARD, {STEWARD}, WAITS BY THE SEAT OF POWER. NOT A WORD IN THE TAVERNS."
  opt "I'LL GO TO THE STEWARD." -> summons
  opt "HOW ILL IS THE {LORD.TITLE}?" -> herald2
  opt "NOT MY KINGDOM'S TROUBLE." -> decline

stage herald2
  talk giver
  say "I'M A HERALD. I SHOUT WHAT I'M GIVEN. BUT I'VE SHOUTED THE SAME PROCLAMATION OF GOOD HEALTH NINE DAYS RUNNING, AND THE PHYSICIANS HAVE STOPPED SMILING ON THE STAIRS."
  opt "I'LL GO TO THE STEWARD." -> summons
  opt "NOT MY KINGDOM'S TROUBLE." -> decline

stage decline
  end fail
  journal "YOU LEFT THE PALACE OF {SEAT} TO ITS WHISPERS."
  do remember giver "HEAR YE! THE {LORD.TITLE} IS IN EXCELLENT HEALTH! ...DON'T LOOK AT ME LIKE THAT. I SHOUT WHAT I'M GIVEN."

stage summons
  goal goto seat
  then steward1
  journal "THE {LORD.TITLE} OF {LAND} IS ILL. THE STEWARD OF THE ROYAL HOUSEHOLD, {STEWARD}, WAITS AT {SEAT}."

stage steward1
  talk steward
  say "THE {LORD.TITLE} IS DYING. THE PHYSICIANS GIVE {LORD.HIM} A MONTH. THERE IS NO CHILD, AND {LORD.HIS} BROTHER {CLAIMANT}, LORD OF {HOLD}, IS ALREADY HIRING SPEARS. BUT THERE IS A RUMOUR, OLD AND QUIET, OF A CHILD HIDDEN AT BIRTH."
  opt "TELL ME OF THE HIDDEN CHILD." -> steward2
  opt "WHY NOT {CLAIMANT}?" -> steward3
  opt "WHAT DOES THE {LORD.TITLE} WANT?" -> audience

stage steward2
  talk steward
  say "THE PRINCESS DIED IN CHILDBED TWENTY YEARS AGO, THEY SAY, AND THE BABY WITH HER. THE MIDWIFE SAID OTHERWISE ON HER DEATHBED: THAT A CHARTER WAS HIDDEN AMONG THE DEAD OF {CRYPT.OLD}, WHERE NOBODY OF {LAND} EVER GOES."
  opt "WHY NOT {CLAIMANT}?" -> steward3
  opt "I MUST SEE THE {LORD.TITLE}." -> audience

stage steward3
  talk steward
  say "{CLAIMANT} IS NOT A MONSTER. {CLAIMANT} IS WORSE: A BORROWER. {RIVAL} HAS LENT {CLAIMANT.HIM} GOLD AND SPEARS, AND {RIVAL} WILL WANT PAYING IN LAND. BUT HEAR THE {LORD.TITLE} BEFORE YOU HEAR ME."
  opt "I MUST SEE THE {LORD.TITLE}." -> audience
  opt "I'D HEAR {CLAIMANT} OUT FIRST." -> to_claimant

stage audience
  talk lord
  journal "THE {LORD.TITLE} OF {LAND} LIES ILL IN THE SEAT OF POWER AT {SEAT}. SEEK AN AUDIENCE."
  say "SO {STEWARD} FOUND ME A SWORD. GOOD. LISTEN, I TIRE QUICKLY. MY SISTER'S CHILD LIVES. I SENT THE BABE AWAY MYSELF, TO KEEP IT FROM MY BROTHER. FIND THE CHARTER IN {CRYPT}. FIND THE CHILD. DON'T LET {CLAIMANT} FIND EITHER."
  opt "WHY HIDE THE CHILD FROM HIM?" -> audience2
  opt "WHAT IF {CLAIMANT} RULES BETTER?" -> audience3
  opt "I'LL FIND THE CHARTER." -> crypt

stage audience2
  talk lord
  say "BECAUSE MY BROTHER WOULD HAVE DROWNED IT IN A BUCKET AND WEPT AT THE FUNERAL. HE LOVES ME, IN HIS WAY. HE LOVES THE CROWN MORE. NOW GO, BEFORE I COUGH UP SOMETHING THE PHYSICIANS WILL WANT TO KEEP."
  opt "I'LL FIND THE CHARTER." -> crypt
  opt "I'LL HEAR {CLAIMANT} OUT FIRST." -> to_claimant

stage audience3
  talk lord
  say "THEN YOU WILL MAKE YOUR CHOICE, AND I WILL BE DEAD AND UNABLE TO HAVE YOU HANGED FOR IT. BUT ASK YOURSELF WHO PAID FOR HIS SPEARS. NOTHING {RIVAL} LENDS EVER COMES BACK SMALLER."
  opt "I'LL FIND THE CHARTER." -> crypt
  opt "I'LL HEAR {CLAIMANT} OUT FIRST." -> to_claimant

stage to_claimant
  goal goto hold
  then claimant1
  journal "{CLAIMANT}, BROTHER OF THE {LORD.TITLE}, HOLDS {HOLD}, {HOLD.DIR} OF HERE. HEAR WHAT {CLAIMANT.HE} HAS TO SAY."

stage claimant1
  talk claimant
  say "THE STEWARD SENT YOU TO SPY, OR YOU CAME TO SELL? SIT, EITHER WAY. MY BROTHER IS DYING AND THE CROWN NEEDS A HEAD THAT ISN'T FULL OF GHOSTS. THERE IS NO HIDDEN CHILD. BUT IF A CHARTER TURNS UP, BRING IT TO ME."
  opt "AND IF THE CHILD IS REAL?" -> claimant2
  opt "WHAT'S IT WORTH TO YOU?" -> pact
  opt "I SERVE THE {LORD.TITLE}, NOT YOU." -> crypt

stage claimant2
  talk claimant
  say "THEN THE CHILD IS A STRANGER RAISED BY PIG-FARMERS, AND {RIVAL}'S SPEARS WILL EAT {LAND} WHILE THE LITTLE LORD LEARNS WHICH FORK TO USE. I'M NOT THE VILLAIN OF THIS STORY. I'M THE ONLY ONE IN IT WHO'S READY."
  opt "WHAT'S IT WORTH TO YOU?" -> pact
  opt "I'LL TAKE MY CHANCES WITH THE CHILD." -> crypt

stage pact
  talk claimant
  say "A WISE QUESTION. EIGHTY NOW, THREE HUNDRED WHEN THE CHARTER IS IN MY HAND, AND A HALL OF YOUR OWN WHEN I'M CROWNED. MY CAPTAIN, {CAPTAIN}, KNOWS YOUR FACE NOW. YOU'LL PASS WHERE OTHERS WON'T."
  do gold 80
  do set favour 1
  opt "DONE." -> crypt

stage crypt
  goal fetch "THE SEALED CHARTER" in crypt
  then charter
  journal "A CHARTER NAMING THE HIDDEN HEIR OF {LAND} LIES AMONG THE DEAD OF {CRYPT.OLD} AT {CRYPT}, {CRYPT.DIR}."

stage charter
  goal goto hamlet
  then heir1
  journal "THE CHARTER BEARS THE ROYAL SEAL. THE CHILD WAS GIVEN TO A FAMILY IN {HAMLET}, AND THE NAME WRITTEN THERE IS {HEIR}."

stage heir1
  talk heir
  say "WHO ARE YOU? WHAT DO YOU... THAT SEAL. MY MOTHER HAD A RING WITH THAT SEAL ON IT. SHE SAID SHE FOUND IT IN A DITCH. WHY ARE YOU LOOKING AT ME LIKE THAT?"
  opt "YOU ARE THE {LORD.TITLE}'S HEIR." if novar favour 1 -> heir_truth
  opt "SOMEONE WANTS TO MEET YOU." if var favour 1 -> heir_trap
  opt "BURN THIS. FORGET YOU SAW ME." -> heir_burn

stage heir_truth
  talk heir
  say "ME? I MUCK OUT A TANNER'S YARD. ...BUT MOTHER ALWAYS SAID I HAD MY FATHER'S CHIN AND WOULDN'T SAY WHOSE. AND {CLAIMANT}'S MEN WERE ASKING ABOUT ME IN THE TAVERN LAST WEEK. I THOUGHT THEY WANTED A WIFE."
  opt "FIRST, WE DEAL WITH THEM." -> hunt
  opt "WE RIDE FOR {SEAT} TONIGHT." -> ride

stage hunt
  goal slay captain
  then ride
  journal "{CLAIMANT}'S SELLSWORD {CAPTAIN} HUNTS {HEIR}. HE CAMPS AT {CAMP}, {CAMP.DIR}. END THE HUNT."

stage ride
  goal goto seat
  then presented
  do moves heir seat
  journal "BRING {HEIR} AND THE SEALED CHARTER TO THE {LORD.TITLE} AT {SEAT}."

stage presented
  talk lord
  say "...SHE HAS HER MOTHER'S EYES. AND MY FATHER'S SCOWL, GODS HELP HER. {HEIR}, COME HERE. NO, DON'T KNEEL. YOU'LL DO ENOUGH OF THAT FOR PRIESTS. {STEWARD}! THE CHARTER TO THE COUNCIL, AND BAR THE GATES."
  do take "THE SEALED CHARTER"
  opt "LONG LIVE THE HEIR." -> envoy1

stage envoy1
  talk envoy
  say "AN HEIR FROM A TANNER'S YARD. HOW MOVING. {RIVAL} HAD AN UNDERSTANDING WITH LORD {CLAIMANT}, AND A GREAT DEAL OF GOLD IN IT. MY MASTER WILL ASK WHO REPAYS THAT GOLD. THINK CAREFULLY ABOUT YOUR ANSWER."
  opt "THE DEBT IS {CLAIMANT}'S. GO COLLECT." -> war_vigil
  opt "{LAND} WILL REPAY THE GOLD." -> peace_vigil
  opt "TELL YOUR MASTER WE'LL SEE." check level 10 -> cowed_vigil else war_vigil

stage war_vigil
  goal wait 2
  then crowning
  do realm war rival land
  journal "{RIVAL} HAS DECLARED WAR OVER {CLAIMANT}'S DEBT. IN THE PALACE AT {SEAT}, THE {LORD.TITLE} IS DYING."

stage peace_vigil
  goal wait 2
  then crowning
  do realm event tradedeal land rival
  do rep rival 5
  journal "{LAND} WILL REPAY {RIVAL} FROM ITS OWN TREASURY. IN THE PALACE AT {SEAT}, THE {LORD.TITLE} IS DYING."

stage cowed_vigil
  goal wait 2
  then crowning
  do fame 3
  journal "{RIVAL}'S ENVOY LEFT WITHOUT ANOTHER WORD. IN THE PALACE AT {SEAT}, THE {LORD.TITLE} IS DYING."

stage crowning
  talk heir
  say "{LORD} DIED IN THE NIGHT. {LORD.HE} HELD MY HAND AND CALLED ME BY MY MOTHER'S NAME. THEY'VE PUT A CROWN ON ME THAT WAS MADE FOR SOMEONE TWICE MY SIZE. YOU'LL STAY FOR THE FEAST? I DON'T KNOW ANYONE HERE BUT YOU."
  do realm event ruleddied land
  opt "I'LL STAY. LONG LIVE THE {LORD.TITLE}." -> heir_end
  opt "I'M NO COURTIER. BUT I'LL STAY." -> heir_end

stage heir_end
  end success
  journal "{HEIR}, A TANNER'S GIRL OF {HAMLET}, WEARS THE CROWN OF {LAND}. YOU FOUND HER."
  do realm succession land heir
  do rep land 15
  do fame 4
  do gold 300
  do xp 400
  do mark heir_crowned
  do remember giver "LONG LIVE THE {LORD.TITLE} {HEIR}! AND LONG LIVE THE ONE WHO FOUND HER, THEY SAY IN THE PALACE KITCHENS. I SAY IT TOO."
  do fact "{HEIR}, RAISED IN {HAMLET}, WAS CROWNED IN {SEAT} WITH THE SEALED CHARTER OF HER BIRTH. {CLAIMANT} FLED TO {RIVAL}."

stage heir_trap
  talk heir
  say "MEET WHO? ...ALRIGHT. MOTHER ALWAYS SAID THE RING WOULD BRING STRANGERS ONE DAY, AND THAT I SHOULD GO WITH THE POLITE ONES. YOU'RE POLITE. MOSTLY."
  opt "THIS WAY. TO {HOLD}." -> deliver
  opt "...NO. RUN. {CLAIMANT} WANTS YOU GONE." -> turncoat

stage turncoat
  talk heir
  say "GONE? YOU WERE BRINGING ME TO... ALRIGHT. ALRIGHT, I'M NOT STUPID. I'M ONLY A LITTLE STUPID. WHERE DO WE GO? AND WHY SHOULD I TRUST YOU NOW?"
  do set favour 0
  do fact "{CLAIMANT} OF {HOLD} PAID A STRANGER EIGHTY GOLD FOR A CHARTER, AND THE STRANGER KEPT BOTH."
  opt "I'LL KILL HIS CAPTAIN FIRST." -> hunt
  opt "TO {SEAT}. NOW. TRUST THAT." -> ride

stage deliver
  goal goto hold
  then claimant_end
  do moves heir hold
  journal "BRING {HEIR} AND THE CHARTER TO {CLAIMANT} AT {HOLD}, AS AGREED."

stage claimant_end
  talk claimant
  say "SO THIS IS MY NIECE. YOU HAVE MY BROTHER'S CHIN, GIRL. DON'T LOOK AT ME LIKE THAT: YOU'LL LIVE. YOU'LL LIVE QUIETLY, VERY FAR FROM HERE, WITH A PENSION AND A NEW NAME. AND THE CHARTER?"
  opt "HERE. BURN IT." -> vigil_b
  opt "I KEPT IT. INSURANCE." -> vigil_keep

stage vigil_b
  goal wait 2
  then crowned_b
  do take "THE SEALED CHARTER"
  do hide heir
  do moves claimant seat
  journal "THE CHARTER IS ASH AND {HEIR} IS ON A SHIP. IN {SEAT}, THE {LORD.TITLE} IS DYING. {CLAIMANT} WAITS AT THE DOOR."

stage vigil_keep
  goal wait 2
  then crowned_k
  do hide heir
  do moves claimant seat
  journal "YOU KEEP THE CHARTER, AND {CLAIMANT} KNOWS IT. IN {SEAT}, THE {LORD.TITLE} IS DYING."

stage crowned_b
  talk claimant
  say "THE {LORD.TITLE} IS DEAD. LONG LIVE THE {LORD.TITLE}: ME. {RIVAL} SENDS ITS CONGRATULATIONS AND ITS BILL. HERE IS YOUR THREE HUNDRED. THE HALL WILL TAKE LONGER. HALLS ARE DEAR, AND I AM SUDDENLY VERY POOR."
  do realm event ruleddied land
  opt "A PLEASURE, MAJESTY." -> claimant_win

stage crowned_k
  talk claimant
  say "THE CROWN IS MINE, AND YOU HAVE A PIECE OF PAPER THAT SAYS IT ISN'T. CLEVER. DANGEROUS. HERE IS YOUR GOLD, AND MY ADVICE: NEVER SLEEP IN THE SAME BED TWICE IN {LAND}."
  do realm event ruleddied land
  opt "SLEEP WELL YOURSELF, MAJESTY." -> claimant_keep

stage claimant_win
  end success
  journal "{CLAIMANT} RULES {LAND}, WITH {RIVAL}'S GOLD BEHIND THE THRONE. NO HEIR WAS EVER FOUND. OFFICIALLY."
  do realm succession land claimant
  do realm event tradedeal land rival
  do gold 300
  do xp 300
  do mark claimant_crowned
  do remember giver "LONG LIVE THE {LORD.TITLE}! THE NEW ONE. YES. LONG LIVE {CLAIMANT}. I SAY IT LOUDER EVERY DAY, IN CASE SOMEONE IS COUNTING."
  do fact "{CLAIMANT} TOOK THE THRONE OF {LAND} WITH {RIVAL}'S GOLD. THEY SAY A GIRL FROM {HAMLET} SAILED THE SAME WEEK."

stage claimant_keep
  end success
  journal "{CLAIMANT} RULES {LAND}. THE CHARTER OF THE TRUE HEIR IS STILL OUT THERE, AND SO ARE YOU."
  do realm succession land claimant
  do realm event tradedeal land rival
  do gold 300
  do xp 300
  do mark claimant_crowned
  do mark charter_kept
  do remember giver "LONG LIVE {CLAIMANT}! ...THEY SAY YOU HAVE SOMETHING THE {LORD.TITLE} WANTS. THEY SAY IT IN WHISPERS. I DON'T WHISPER. I'M A HERALD."
  do fact "THE SEALED CHARTER OF {LAND}'S TRUE HEIR IS SAID TO BE IN A STRANGER'S PACK. {CLAIMANT} SLEEPS BADLY."

stage heir_burn
  talk heir
  say "...THANK YOU. I THINK. I DON'T WANT A CROWN. I WANT THE TANNER'S SON TO ASK ME TO THE MIDSUMMER DANCE, AND HE WILL, IF I STOP SMELLING OF HIDES FOR ONE DAY."
  do take "THE SEALED CHARTER"
  opt "THEN I WAS NEVER HERE." -> no_heir

stage no_heir
  goal goto seat
  then steward_end
  do hide heir
  journal "THE HEIR OF {LAND} DOES NOT WANT TO BE FOUND. TELL {STEWARD} AT {SEAT}, OR TELL {STEWARD.HIM} NOTHING AT ALL."

stage steward_end
  talk steward
  say "NOTHING? NO CHILD, NO CHARTER? THEN {CLAIMANT} TAKES THE THRONE, AND {RIVAL} TAKES {CLAIMANT}. ...YOU'RE LYING. I CAN SEE IT. AND I SUSPECT YOU'RE LYING FOR A GOOD REASON. I WON'T ASK."
  opt "SOME PEOPLE DESERVE A SMALL LIFE." -> quiet_end

stage quiet_end
  end success
  journal "{CLAIMANT} RULES {LAND}. IN {HAMLET}, A TANNER'S GIRL DANCED AT MIDSUMMER AND NEVER KNEW HOW CLOSE SHE CAME."
  do realm event ruleddied land
  do realm succession land claimant
  do gold 120
  do xp 250
  do mark heir_hidden
  do remember giver "LONG LIVE {CLAIMANT}, I SUPPOSE. YOU LOOK LIKE SOMEONE WHO KNOWS SOMETHING. DON'T TELL ME. HERALDS CAN'T KEEP SECRETS."
  do fact "THE HIDDEN HEIR OF {LAND} WAS NEVER FOUND. IN {HAMLET}, A TANNER'S GIRL DANCED AT MIDSUMMER."
)DSL",
};

}  // namespace

const char* campaignSource() {
  static const std::string all = [] {
    std::string s;
    for (const char* t : kCampaign) s += t;
    return s;
  }();
  return all.c_str();
}

}  // namespace dsl
}  // namespace story
