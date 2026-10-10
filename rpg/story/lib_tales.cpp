// M4 "Banners": the tier-2 story library (owner 15.9: story quests with real dialogue, characters with motives, a twist
// or a choice, consequences that persist; archetypes borrowed from myth, folklore, scripture and classic fantasy: their
// structures and motifs, never anyone's names or plots). STORY lane. The language: rpg/story/dsl.h.
//
// Each script is its own raw literal (MSVC caps one literal near 16 KB); talesSource() joins them once.
//
//   prodigal     THE PRODIGAL'S RETURN      an innkeeper's son ran off to the bandits; a father's cruel word
//   forgemark    A BROTHER'S BETRAYAL       bad steel under a dead father's mark; the betrayal is not who you think
//   omen         THE PROPHECY MISREAD       a town will drive out its herb-wife over a carving nobody can read
//   warmring     THE CURSED GIFT            a dead man's ring, owed to the living (begins in a ruin)
//   wager        THE TRICKSTER'S BARGAIN    three riddles, a map, and the trick inside the trick
//   foundling    THE HEIR IN HIDING         a farmer's foundling, a hound-and-tower ring, riders in green
//   hungry       THE FLOOD AS JUDGEMENT     a famine read as the river's revenge; the miller's barn (begins with the news)
//   hollowhill   A DEAL WITH THE FAE COURT  a hunter's child dancing under the hill; the price of a name
//   pardon       THE EXILE'S HOMECOMING     a banished shield-man's plea, carried to a rival court
//   laststand    THE SACRIFICIAL STAND      an old captain and a road at dawn (begins with a war)
//   psalter      THE NAME AMONG THE DEAD    a missing child at the ruin her family came from (a notice board)
#include <string>
#include "rpg/story/dsl.h"

namespace story {
namespace dsl {

namespace {

const char* const kTales[] = {
R"DSL(
script prodigal
title THE PRODIGAL'S RETURN
archetype THE PRODIGAL'S RETURN
tier 2
hook npc innkeeper
pitch "YOU LOOK LIKE YOU HAVEN'T SLEPT."
hint "SORRY. I'M POURING WITH ONE HAND AND COUNTING THE DOOR WITH THE OTHER. WAITING FOR SOMEONE WHO ISN'T COMING."
role giver giver
role home site home
role camp site camp near
role son person male at camp
var sorry 0

stage start
  talk giver
  say "MY BOY {SON} EMPTIED THE TILL AND RAN OFF WITH A CREW THAT USED TO DRINK HERE. THEY CAMP AT {CAMP}, {CAMP.DIR} OF HERE. HE'S A FOOL, BUT HE'S MY FOOL. WILL YOU BRING HIM HOME?"
  opt "I'LL BRING HIM BACK." -> seek
  opt "WHY DID HE LEAVE?" -> why
  opt "FIND HIM YOURSELF." -> refused

stage why
  talk giver
  say "I TOLD HIM HE'D NEVER BE HALF THE PERSON HIS MOTHER WAS. THE LAST THING I SAID TO HIM. HE LEFT THAT NIGHT WITH THE TILL AND HIS GRANDFATHER'S KNIFE, AND I'VE SAID IT TO MYSELF EVERY NIGHT SINCE."
  opt "THEN I'LL FIND HIM." -> seek
  opt "YOU OWE HIM AN APOLOGY." -> sorry

stage sorry
  talk giver
  say "...AYE. I DO. IF YOU FIND HIM, TELL HIM I SAID IT FIRST. TELL HIM HIS FATHER IS THE ONE WHO SHOULD BE ASHAMED."
  do set sorry 1
  opt "I'LL TELL HIM." -> seek

stage refused
  end fail
  say "THEN DRINK YOUR ALE AND GO. IT'S ON THE HOUSE. I DON'T WANT YOUR COIN IN MY TILL; HE'S ALREADY EMPTIED IT ONCE."
  journal "YOU TURNED {GIVER} AWAY."
  do remember giver "YOU AGAIN. THE ONE WHO WOULDN'T LOOK FOR MY BOY. DRINK UP AND MOVE ALONG."

stage seek
  goal goto camp
  then found
  say "HE'S GOT A SCAR ON HIS CHIN FROM THE CELLAR STAIRS. YOU'LL KNOW HIM. AND MIND THE OTHERS: THEY'RE NOT FOOLS, THEY'RE WORSE."
  journal "{GIVER}'S SON {SON} RAN OFF TO THE BANDITS AT {CAMP}. FIND HIM AND BRING HIM HOME TO {HOME}."

stage found
  talk son
  say "THE OLD {GIVER.MAN} SENT YOU? HA. DID {GIVER.HE} TELL YOU WHAT {GIVER.HE} SAID TO ME? NO, OF COURSE NOT. I'M NOT GOING BACK TO WIPE TABLES AND HEAR HOW I FAILED MY MOTHER."
  opt "THE SHAME IS {GIVER.HIS}, NOT YOURS." if var sorry 1 -> reconciled
  opt "BANDITS? IS THIS A LIFE?" check level 3 -> persuaded else stubborn
  opt "THEN STAY. I'LL SAY YOU'RE DEAD." -> lie

stage reconciled
  talk son
  say "{GIVER.HE} SAID THAT? {GIVER.HE} NEVER SAID SORRY FOR SO MUCH AS A SPILT CUP. ...ALRIGHT. I'LL GO HOME. TELL {GIVER.HIM} TO PUT THE KETTLE ON, AND NOT TO MAKE A SPEECH."
  opt "I'LL WALK YOU TO THE ROAD." -> homeward

stage persuaded
  talk son
  say "...NO. NO, IT ISN'T. I'VE SLEPT IN THE RAIN A MONTH FOR MEN WHO'D SELL ME FOR A HORSE. I'LL GO. DON'T TELL THE OLD {GIVER.MAN} I CRIED."
  opt "GO, BEFORE THEY NOTICE." -> homeward

stage stubborn
  talk son
  say "A BETTER ONE THAN WIPING TABLES. GO HOME, HERO. AND IF YOU TRY TO DRAG ME, THE LADS WILL GUT YOU FOR THE PRACTICE."
  opt "THEN I'LL TELL {GIVER.HIM} THE TRUTH." -> truthward
  opt "THEN I'LL SAY YOU'RE DEAD." -> lie

stage homeward
  goal goto home
  then reunion
  do moves son home
  journal "{SON} IS WALKING HOME TO {HOME}. GO AND TELL {GIVER}."

stage reunion
  talk giver
  say "HE'S HERE. HE CAME IN AN HOUR AGO, SOAKED THROUGH, AND HE HUGGED ME. HE HASN'T DONE THAT SINCE HE WAS SIX. TAKE THIS, AND EAT HERE FREE AS LONG AS I KEEP THIS PLACE."
  opt "LOOK AFTER EACH OTHER." -> home_end

stage home_end
  end success
  journal "{SON} IS HOME, AND {GIVER} WILL NOT FORGET WHO BROUGHT HIM."
  do gold 60
  do xp 90
  do remember giver "THERE'S THE ONE WHO BROUGHT MY BOY HOME! SIT DOWN. THE STEW IS ON THE HOUSE, AND SO ARE YOU."
  do fact "{SON} CAME HOME TO {HOME} FROM THE BANDIT CAMP AT {CAMP}, AND {GIVER} CLOSED THE INN FOR A NIGHT."
  do mark son_home

stage lie
  goal goto home
  then tell_dead
  do hide son
  journal "YOU WILL TELL {GIVER} THAT {SON} IS DEAD. HE IS NOT."

stage tell_dead
  talk giver
  say "DEAD? ...I SEE. I SEE. HIS GRANDFATHER'S KNIFE, DID THEY... NO. DON'T TELL ME. HERE, FOR YOUR TROUBLE. I THINK I'LL CLOSE EARLY TONIGHT. I THINK I'LL CLOSE EARLY FOR A WHILE."
  opt "I'M SORRY." -> dead_end

stage dead_end
  end success
  journal "YOU TOLD {GIVER} THAT {SON} DIED AMONG THE BANDITS. {GIVER} BELIEVED YOU."
  do gold 20
  do remember giver "NO LAUGHING TONIGHT, IF YOU DON'T MIND. I'M IN MOURNING. MY BOY... WELL. YOU KNOW."
  do fact "{GIVER} OF {HOME} MOURNS A SON WHO IS ALIVE AND ROBBING CARTS AT {CAMP}."

stage truthward
  goal goto home
  then tell_truth
  journal "{SON} WILL NOT COME HOME. TELL {GIVER} THE TRUTH."

stage tell_truth
  talk giver
  say "HE WON'T COME. ...WELL. HE'S ALIVE. THAT'S MORE THAN I DESERVE, MAYBE. I'LL WRITE TO HIM. I'M BAD WITH WORDS, BUT I'LL WRITE."
  opt "WRITE IT. HE MIGHT READ IT." -> truth_end

stage truth_end
  end success
  journal "{SON} STAYED AT {CAMP}. {GIVER} IS WRITING HIM A LETTER, ONE WORD A NIGHT."
  do gold 30
  do xp 60
  do remember giver "STILL NO REPLY. I'VE WRITTEN NINE LETTERS. THE TENTH WILL BE SHORTER, AND KINDER."
)DSL",

R"DSL(
script forgemark
title A BROTHER'S BETRAYAL
archetype A BETRAYAL BY A BROTHER
tier 2
hook npc smith
pitch "WHY IS YOUR DOOR SCORCHED?"
hint "DON'T MIND THE DOOR. SOMEONE TRIED TO BURN IT. THEY THINK I KILLED A MAN WITH BAD STEEL."
role giver giver
role home site home
role town site town near
role bro person male at town
role watch npc guard at town
role forge npc smith at town

stage start
  talk giver
  say "SOMEONE IS STAMPING MY FATHER'S MARK ON ROTTEN STEEL. A SWORD SNAPPED IN A WATCHMAN'S HAND AND HE DIED, AND HIS WIDOW SPAT AT MY DOOR. THE BLADES COME FROM {TOWN}. MY BROTHER {BRO} KEEPS A FORGE THERE."
  opt "I'LL FIND OUT WHO." -> seek
  opt "YOU THINK YOUR BROTHER DID IT?" -> suspicion
  opt "NOT MY AFFAIR." -> refused

stage suspicion
  talk giver
  say "I THINK HE'S HATED ME SINCE FATHER LEFT ME THE ANVIL AND HIM THE HOUSE. HE SAID I GOT THE BETTER HALF. MAYBE I DID. BUT I NEVER THOUGHT HE'D PUT FATHER'S MARK ON A LIE."
  opt "I'LL HEAR HIS SIDE." -> seek

stage refused
  end fail
  journal "YOU LEFT {GIVER} TO {GIVER.HIS} SCORCHED DOOR."
  do remember giver "THE DOOR'S STILL BLACK, IF YOU WERE WONDERING. THE WIDOW SPITS ON IT EVERY MARKET DAY. I DON'T BLAME HER. I'D SPIT TOO."

stage seek
  goal goto town
  then confront
  say "HE HAS A FORGE BY THE RIVER GATE, IF HE STILL HAS ANYTHING. HE HAMMERS LEFT-HANDED, LIKE FATHER DID. TELL HIM... NO. DON'T TELL HIM ANYTHING FROM ME."
  journal "BAD STEEL UNDER {GIVER}'S FATHER'S MARK COMES FROM {TOWN}. FIND {BRO}, {GIVER}'S BROTHER."

stage confront
  talk bro
  say "{GIVER} SENT YOU? THEN {GIVER.HE} DIDN'T TELL YOU {GIVER.HE} SOLD ME THE MARK. NINE YEARS AGO, DROWNING IN DICE DEBT. THE BAD STEEL IS MINE, AYE: MY APPRENTICE SKIMPS THE CHARCOAL. BUT THE MARK IS MINE TOO."
  opt "SHOW ME THE BILL OF SALE." -> bill
  opt "SACK THE BOY. PAY THE WIDOW." -> amends
  opt "THE WATCH WILL HEAR OF THIS." -> expose

stage bill
  talk bro
  say "HERE. {GIVER}'S HAND, {GIVER.HIS} SEAL, NINE WINTERS OLD. {GIVER.HE} WAS SO DRUNK {GIVER.HE} SPELLED {GIVER.HIS} OWN NAME TWO WAYS. ASK {GIVER.HIM} WHERE {GIVER.HIS} SILVER WENT THAT WINTER."
  do give "A BILL OF SALE FOR A MARK" scroll
  opt "I'LL TAKE THIS TO {GIVER}." -> carry_bill
  opt "SACK THE BOY. PAY THE WIDOW." -> amends

stage amends
  talk bro
  say "...AYE. THE BOY GOES, AND THE WIDOW GETS EVERY COIN THOSE BLADES MADE ME. TELL {GIVER} THE MARK IS CLEAN AGAIN. AND TELL {GIVER.HIM} I MISS THE OLD FORGE. NOT THE ANVIL. THE NOISE OF TWO HAMMERS."
  opt "I'LL TELL {GIVER.HIM}." -> back_amends

stage expose
  talk watch
  say "BAD STEEL UNDER A FALSE MARK, AND A WATCHMAN DEAD OF IT? WE'LL SHUT HIS FORGE BY NIGHTFALL. THE GUILD SELLS IT TO WHOEVER PAYS HIS FINE. THANK YOU, STRANGER. I THINK."
  opt "SO BE IT." -> back_exposed

stage carry_bill
  goal goto home
  then confess
  journal "{BRO} SAYS {GIVER} SOLD HIM THE MARK YEARS AGO. YOU CARRY THE BILL OF SALE BACK TO {HOME}."

stage confess
  talk giver
  say "WHERE DID YOU... OH. OH, GODS. THE DICE. THAT WINTER. I REMEMBER THE TAVERN AND NOTHING AFTER. NINE YEARS I'VE HATED HIM FOR A THING I DID WITH MY OWN HAND."
  opt "GO TO HIM. MEND IT." -> mend_end
  opt "BURN IT. NOBODY NEEDS TO KNOW." -> burn_end

stage back_amends
  goal goto home
  then tell_amends
  journal "{BRO} WILL PAY THE WIDOW AND KEEP THE MARK CLEAN. TELL {GIVER}."

stage tell_amends
  talk giver
  say "CLEAN? AND HE PAID THE WIDOW HIMSELF? ...HE MISSES THE NOISE OF TWO HAMMERS. SO DO I. I'LL WALK TO {TOWN} AT THE NEW MOON. DON'T TELL HIM I'M COMING."
  opt "I WON'T SAY A WORD." -> amends_end

stage back_exposed
  goal goto home
  then tell_exposed
  journal "THE WATCH OF {TOWN} SHUT {BRO}'S FORGE. TELL {GIVER}."

stage tell_exposed
  talk giver
  say "SHUT? THEY SHUT HIS FORGE? I WANTED THE MARK CLEAN, NOT MY BROTHER IN THE GUTTER. ...WELL. A MAN DIED. MAYBE THAT'S THE PRICE. IT DOESN'T FEEL LIKE JUSTICE. IT FEELS LIKE WINNING."
  opt "IT HAD TO BE DONE." -> exposed_end

stage mend_end
  end success
  journal "{GIVER} WENT TO {TOWN} WITH THE BILL OF SALE IN {GIVER.HIS} HAND AND AN APOLOGY IN {GIVER.HIS} MOUTH."
  do take "A BILL OF SALE FOR A MARK"
  do gold 50
  do xp 90
  do remember giver "MY BROTHER AND I SHARE THE MARK NOW. HALF THE ORDERS EACH, AND WE ARGUE ABOUT ALL OF THEM. IT'S WONDERFUL."
  do fact "THE BROTHERS {GIVER} AND {BRO} FORGE UNDER ONE MARK AGAIN, AFTER NINE YEARS OF SILENCE."

stage burn_end
  end success
  journal "THE BILL OF SALE IS ASH IN {GIVER}'S FORGE. NOBODY WILL EVER KNOW. EXCEPT YOU, AND {GIVER}, AND {BRO}."
  do take "A BILL OF SALE FOR A MARK"
  do gold 70
  do remember giver "WE DON'T TALK ABOUT THE BILL. WE DON'T TALK ABOUT MY BROTHER. WHAT CAN I SELL YOU?"

stage amends_end
  end success
  journal "THE MARK IS CLEAN, THE WIDOW IS PAID, AND {GIVER} IS GOING TO {TOWN} AT THE NEW MOON."
  do gold 55
  do xp 80
  do remember giver "BACK FROM {TOWN} WITH A BLACK EYE AND A BROTHER. BEST TRADE I EVER MADE."
  do fact "{BRO} OF {TOWN} SACKED HIS APPRENTICE AND PAID A WATCHMAN'S WIDOW FROM HIS OWN PURSE."

stage exposed_end
  end success
  journal "{BRO}'S FORGE IN {TOWN} WAS SHUT AND SOLD BY THE GUILD."
  do gold 60
  do xp 70
  do shop forge "THE GUILD SOLD ME {BRO}'S FORGE FOR HIS FINE. I PAID TOO LITTLE. I KNOW IT, AND SO DOES EVERYONE."
  do remember giver "THE MARK IS CLEAN. MY BROTHER SWEEPS A STABLE IN {TOWN}. BOTH OF THOSE ARE TRUE."
)DSL",

R"DSL(
script omen
title THE PROPHECY MISREAD
archetype A PROPHECY MISREAD
tier 2
hook npc priest
pitch "WHY IS THE TOWN WHISPERING?"
hint "PRAY WITH ME, IF YOU WOULD. THE PEOPLE WANT A SIGN FROM THE GODS AND I'M AFRAID I'VE GIVEN THEM THE WRONG ONE."
role giver giver
role home site home
role ruin ruin near
role witch person female at home
var asked 0

stage start
  talk giver
  say "THE OLD WORDS SAY: WHEN THE STONES OF {RUIN.OLD} SPEAK, {HOME} SHALL DROWN IN RED. THE STONES MOANED LAST NEW MOON. THE PEOPLE BLAME {WITCH}, OUR HERB-WIFE. THEY WANT HER GONE BY THE FULL MOON."
  opt "WHERE DO THE OLD WORDS COME FROM?" -> source
  opt "LET ME SPEAK TO {WITCH}." -> herbwife
  opt "IT'S NOT MY TOWN." -> refused

stage source
  talk giver
  say "A CARVED STONE AT {RUIN}, {RUIN.DIR}, IN THE TONGUE OF {RUIN.OLD}. MY MASTER COPIED IT FORTY YEARS AGO. HIS EYES WERE ALREADY GOING, GODS REST HIM. I HAVE ONLY EVER READ HIS COPY."
  do set asked 1
  opt "THEN I'LL READ THE STONE." -> seek
  opt "LET ME SPEAK TO {WITCH} FIRST." -> herbwife

stage herbwife
  talk witch
  say "THEY THINK I MOAN AT STONES? I'M FIFTY-THREE AND I CAN'T WALK TO THE RUIN WITHOUT A STICK. THE WIND BLOWS THROUGH THE OLD WELLS THERE AND THE STONES SING. ANY SHEPHERD COULD TELL THEM."
  opt "THEN I'LL GO SEE THE STONES." -> seek
  opt "WILL YOU LEAVE QUIETLY?" -> leave_quiet

stage leave_quiet
  talk witch
  say "LEAVE? I DELIVERED HALF THIS TOWN. I SET THE MAYOR'S ARM. I'LL LEAVE WHEN THEY CARRY ME, AND THEN THEY'LL BE SORRY, BECAUSE NOBODY ELSE KNOWS WHAT THE BLUE FLOWERS ARE FOR."
  opt "THEN I'LL READ THE STONE." -> seek

stage refused
  end fail
  journal "YOU LEFT {HOME} TO ITS PROPHECY."
  do remember giver "THE FULL MOON CAME. I DID WHAT THE WORDS SAID. I HOPE YOU SLEEP BETTER THAN I DO."

stage seek
  goal use inscription in ruin
  then reckon
  say "GO CAREFULLY. THE DEAD OF {RUIN.OLD} DO NOT LOVE THE LIVING, AND THE CARVED STONES LIE DEEP."
  journal "THE PROPHECY OF {HOME} COMES FROM A CARVED STONE IN {RUIN}. READ THE OLD WORDS YOURSELF."

stage reckon
  goal goto home
  then verdict
  journal "THE CARVING DOES NOT SAY 'DROWN IN RED'. IT SAYS '{HOME} SHALL DRINK THE RED': A HARVEST TOAST. GO BACK TO {GIVER}."

stage verdict
  talk giver
  say "YOU READ IT? WITH YOUR OWN EYES? TELL ME. TELL ME QUICKLY, BEFORE I LOSE MY NERVE."
  opt "IT'S A HARVEST TOAST. NOT A CURSE." -> truth
  opt "YOU READ IT RIGHT. SHE MUST GO." -> exile
  opt "A CURSE, BUT ONE ONLY SHE CAN LIFT." -> trick

stage truth
  talk giver
  say "'SHALL DRINK THE RED.' A TOAST. FORTY YEARS WE'VE FEARED A WINE CUP. I'LL SAY IT FROM THE STEPS AT DUSK AND THEY WILL LAUGH AT ME. GOOD. LET THEM LAUGH. IT'S BETTER THAN WHAT THEY MEANT TO DO."
  opt "BETTER LAUGHED AT THAN WRONG." -> truth_end

stage exile
  talk giver
  say "...THEN IT IS AS I FEARED. SHE'LL BE GONE BY MORNING. I'LL WALK HER TO THE ROAD MYSELF, SO NOBODY THROWS ANYTHING, AND I'LL CARRY HER BAG OF BLUE FLOWERS. MAY THE GODS FORGIVE ALL OF US."
  opt "THE GODS CAN DO AS THEY LIKE." -> exile_end

stage trick
  talk giver
  say "A CURSE ONLY SHE CAN LIFT? THEN THEY NEED HER, NOT HER ABSENCE. CLEVER, AND NOT QUITE A LIE. I'LL PREACH IT. THE GODS MAY FROWN, BUT THEY'VE FORGIVEN ME WORSE."
  opt "A KIND LIE IS STILL KIND." -> trick_end

stage truth_end
  end success
  journal "{GIVER} TOLD {HOME} THE TRUTH FROM THE TEMPLE STEPS. THE TOWN LAUGHED, AND {WITCH} STAYED."
  do gold 45
  do xp 90
  do remember giver "THEY STILL RAISE A CUP TO ME AND SAY 'DROWN IN RED!' AND LAUGH. I LET THEM. IT IS A GOOD SOUND."
  do fact "THE PRIEST OF {HOME} MISREAD AN OLD STONE FOR FORTY YEARS. THE TOWN DRINKS TO IT NOW."
  do mark omen_truth

stage exile_end
  end success
  journal "{WITCH} WAS DRIVEN FROM {HOME} ON THE WORD OF A STONE YOU KNOW SAYS NOTHING OF THE KIND."
  do gold 30
  do hide witch
  do remember giver "THE TOWN IS CALM. NOBODY KNOWS WHAT THE BLUE FLOWERS ARE FOR ANY MORE. TWO CHILDREN HAVE THE COUGH."
  do fact "THE HERB-WIFE {WITCH} WAS DRIVEN OUT OF {HOME} OVER A PROPHECY. NOBODY KNOWS WHERE SHE WENT."

stage trick_end
  end success
  journal "{GIVER} PREACHED THAT ONLY {WITCH} CAN LIFT THE CURSE OF {RUIN.OLD}. SHE IS SUDDENLY VERY POPULAR."
  do gold 40
  do xp 80
  do remember giver "{WITCH} CHARGES DOUBLE FOR TEA NOW. SHE SAYS IT'S CURSE-LIFTING TEA. I SAY NOTHING, AND I DRINK IT."
  do fact "THE HERB-WIFE OF {HOME} IS SAID TO HOLD BACK AN OLD CURSE WITH HER TEA."
)DSL",

R"DSL(
script warmring
title THE CURSED GIFT
archetype A CURSED GIFT
tier 2
hook ruin
pitch "THE DEAD MAN'S RING"
role giver giver
role town site town near
role heir person female at town

stage start
  goal goto town
  then meet
  say "BENEATH THE PAGES LIES A SILVER RING, WARM AS A LIVING HAND. THE LAST LINE IN THE JOURNAL READS: 'GIVE IT BACK TO {HEIR}'S BLOOD IN {TOWN}. IT WAS NEVER MINE.'"
  do give "A WARM SILVER RING" ring
  journal "A RING FROM A DEAD MAN'S JOURNAL IN {GIVER}. IT ASKS TO GO BACK TO {HEIR}'S FAMILY IN {TOWN}."

stage meet
  talk heir
  say "THAT RING? MY GREAT-GRANDMOTHER'S. SHE GAVE IT TO A MAN WHO SWORE HE'D COME BACK RICH FROM {GIVER}. HE NEVER CAME BACK AT ALL. YOU READ HIS JOURNAL? WHAT DID IT SAY?"
  opt "IT'S YOURS. HE WANTED IT HOME." -> returned
  opt "HE WROTE THAT HE WAS SORRY." -> sorry
  opt "FINDERS, KEEPERS." -> keep

stage sorry
  talk heir
  say "SORRY. A HUNDRED YEARS LATE AND STILL SORRY. ...GRANDMOTHER WOULD HAVE LAUGHED AND FORGIVEN HIM. SHE FORGAVE EVERYONE BUT THE TAX MAN AND HER SISTER. SHE KEPT A CANDLE IN THE WINDOW FOR HIM UNTIL SHE MARRIED. THEN SHE KEPT IT FOR LONGER."
  opt "THE RING, THEN." -> returned

stage returned
  end success
  journal "THE WARM RING IS HOME ON {HEIR}'S HAND IN {TOWN}."
  do take "A WARM SILVER RING"
  do gold 45
  do xp 70
  do remember heir "THE RING HASN'T LEFT MY HAND. IT'S NOT WARM ANY MORE. JUST RIGHT."
  do fact "A RING LOST IN {GIVER} A HUNDRED YEARS AGO CAME HOME TO {TOWN}."

stage keep
  talk heir
  say "THEN KEEP IT, AND KEEP WHAT COMES WITH IT. IT WAS NEVER LUCKY FOR ANYONE IT WASN'T OWED TO. ASK HIM. OH, YOU CAN'T. HE'S IN {GIVER}."
  opt "SUPERSTITION." -> cold

stage cold
  goal wait 2
  then bite
  do gold -25
  journal "YOU KEPT THE SILVER RING. IT IS NOT WARM NOW BUT COLD AS A GRAVE, AND YOUR PURSE IS LIGHTER EVERY MORNING."

stage bite
  goal goto town
  then curse
  journal "THE RING HAS TIGHTENED UNTIL YOUR FINGER IS WHITE. IT WANTS TO GO HOME, OR BACK INTO THE DARK. {HEIR} LIVES IN {TOWN}."

stage curse
  talk heir
  say "YOU CAME BACK. YOUR HAND... IT DOES THAT. IT DID IT TO MY UNCLE AND HE LOST THE FINGER. GIVE IT HERE, OR TAKE IT BACK WHERE YOU FOUND IT. IT DOESN'T CARE WHICH, AS LONG AS IT ISN'T YOU."
  opt "TAKE IT. I'M DONE WITH IT." -> late_end
  opt "I'LL PUT IT BACK IN THE DARK." -> rebury

stage late_end
  end success
  journal "YOU GAVE THE RING BACK TO {HEIR}. YOUR FINGER IS PINK AGAIN."
  do take "A WARM SILVER RING"
  do xp 50
  do remember heir "THE RING LIKES ME. IT NEVER LIKED YOU. NO OFFENCE."
  do fact "A RING LOST IN {GIVER} A HUNDRED YEARS AGO CAME HOME TO {TOWN}, THE LONG WAY ROUND."

stage rebury
  goal use grave in giver
  then buried
  journal "TAKE THE RING BACK TO {GIVER} AND LAY IT ON A GRAVE THERE."

stage buried
  end success
  journal "THE SILVER RING LIES ON A NAMELESS GRAVE IN {GIVER}. IT IS WARM AGAIN. YOU DO NOT TOUCH IT TO CHECK."
  do take "A WARM SILVER RING"
  do xp 60
  do remember heir "YOU PUT IT BACK WITH HIM? ...GOOD. LET THEM HAVE EACH OTHER. GRANDMOTHER HAD THREE HUSBANDS ANYWAY."
  do fact "A WARM SILVER RING LIES ON A NAMELESS GRAVE IN {GIVER}. THE LIVING LEAVE IT ALONE."
)DSL",

R"DSL(
script wager
title THE TRICKSTER'S BARGAIN
archetype THE TRICKSTER'S BARGAIN
tier 2
hook npc merchant
pitch "WHAT'S THE MAP IN YOUR HAT?"
hint "AH, A TRAVELLER WITH QUICK EYES. QUICK EYES, QUICK WITS? I HAVE A WAGER FOR QUICK WITS."
role giver giver
role home site home
role cave site cave near
role thief foe male at cave
role stall npc merchant at home

stage start
  talk giver
  say "A WAGER, FRIEND. THREE RIDDLES. ANSWER ALL THREE AND THIS MAP IS YOURS: A HOARD IN {CAVE}, {CAVE.DIR}. MISS ONE AND I KEEP THIRTY OF YOUR GOLD. FAIR'S FAIR, AND I AM VERY FAIR."
  opt "DEAL. [30 GOLD]" if gold 30 -> r1
  opt "WHAT'S THE CATCH?" -> catch
  opt "I DON'T GAMBLE." -> refused

stage catch
  talk giver
  say "THE CATCH IS THAT I AM VERY GOOD AT RIDDLES. THE MAP IS REAL. THE HOARD IS REAL. ASK ANYONE. WELL, DON'T ASK ANYONE. ASK ME."
  opt "FINE. DEAL. [30 GOLD]" if gold 30 -> r1
  opt "NO. I DON'T TRUST YOU." -> refused

stage refused
  end fail
  journal "YOU WALKED AWAY FROM {GIVER}'S WAGER."
  do remember giver "THE ONE WHO WOULDN'T WAGER! WISE. DULL, BUT WISE. BUY SOMETHING."

stage r1
  talk giver
  say "FIRST: I HAVE CITIES BUT NO HOUSES, FORESTS BUT NO TREES, AND RIVERS WITHOUT A DROP OF WATER. WHAT AM I?"
  opt "A MAP." -> r2
  opt "A DREAM." -> lost
  opt "A KING'S LEDGER." -> lost

stage r2
  talk giver
  say "HM. SECOND: THE MORE OF ME YOU TAKE, THE MORE OF ME YOU LEAVE BEHIND. WHAT AM I?"
  opt "FOOTSTEPS." -> r3
  opt "GOLD." -> lost
  opt "YEARS." -> lost

stage r3
  talk giver
  say "HM! LAST, AND NOBODY GETS IT: WHAT CAN RUN BUT NEVER WALKS, HAS A MOUTH BUT NEVER TALKS, HAS A BED BUT NEVER SLEEPS?"
  opt "A RIVER." -> won
  opt "A THIEF." -> lost
  opt "A BELL." -> lost

stage lost
  end fail
  journal "{GIVER} WON YOUR THIRTY GOLD WITH A RIDDLE. NEXT TIME, THINK ABOUT RIVERS."
  do gold -30
  do remember giver "BACK FOR ANOTHER WAGER? NO? WISE. I BOUGHT A HAT WITH YOUR GOLD. IT'S A VERY GOOD HAT."

stage won
  talk giver
  say "...WELL. WELL, WELL. NOBODY GETS THE RIVER. A DEAL'S A DEAL: HERE, THE MAP. THE HOARD IS DEEP IN {CAVE}. DON'T DAWDLE. HOARDS ARE IMPATIENT THINGS."
  do give "A PEDDLER'S TREASURE MAP" map
  opt "PLEASURE DOING BUSINESS." -> hunt

stage hunt
  goal enter cave
  then ambush
  journal "{GIVER}'S MAP MARKS A HOARD DEEP IN {CAVE}."

stage ambush
  goal slay thief
  then confront
  journal "THERE IS NO HOARD IN {CAVE}: ONLY {THIEF}, {GIVER}'S PARTNER, WAITING IN THE DARK FOR WHOEVER WINS THE WAGER. SURVIVE HIM."

stage confront
  talk giver
  say "YOU'RE... ALIVE. AND {THIEF} ISN'T, I SUPPOSE. LISTEN. LISTEN! THE HOARD WAS ALWAYS A STORY. EVERYONE NEEDS A STORY. YOU GOT A GREAT ONE. A THRILLING ONE. WITH A FIGHT IN IT."
  opt "MY THIRTY BACK. AND THIRTY MORE." -> repaid
  opt "THE WATCH WILL HEAR YOUR STORY." -> exposed
  opt "IT WAS A GOOD STORY. KEEP IT." -> spared

stage repaid
  end success
  journal "{GIVER} PAID YOU SIXTY GOLD TO KEEP QUIET ABOUT THE HOARD IN {CAVE}."
  do take "A PEDDLER'S TREASURE MAP"
  do gold 60
  do xp 80
  do remember giver "YOU! TAKE ANYTHING, AT COST. JUST... NOT A WORD ABOUT THE RIVER, EH?"

stage exposed
  end success
  journal "THE WATCH TOOK {GIVER}'S STALL IN {HOME} AND {GIVER} WITH IT."
  do take "A PEDDLER'S TREASURE MAP"
  do gold 40
  do xp 90
  do shop stall "THE WATCH GAVE ME THE RIDDLER'S STALL. NO WAGERS HERE, FRIEND. JUST HONEST PRICES, MOSTLY."
  do remember giver "FINED, FLOGGED, AND BACK BEHIND A STALL THE SIZE OF A COFFIN. YOU. NO WAGERS. GO AWAY."
  do fact "A PEDDLER OF {HOME} WAS TAKEN BY THE WATCH FOR SENDING WINNERS OF HIS WAGERS TO DIE IN {CAVE}."

stage spared
  end success
  journal "YOU LET {GIVER} KEEP {GIVER.HIS} STORY. {GIVER.HE} OWES YOU, AND KNOWS IT."
  do take "A PEDDLER'S TREASURE MAP"
  do xp 70
  do fame 1
  do remember giver "MY FRIEND! THE ONE WHO KNOWS ABOUT RIVERS! THE STORY GETS BETTER EVERY TIME I TELL IT. YOU KILL SIX MEN NOW."
)DSL",

R"DSL(
script foundling
title THE FOUNDLING'S RING
archetype A RIGHTFUL HEIR IN HIDING
tier 2
hook npc farmer
pitch "YOU KEEP WATCHING THE ROAD."
hint "TWO RIDERS IN GREEN CLOAKS CAME BY YESTERDAY. ASKING QUESTIONS. I DIDN'T LIKE THE QUESTIONS."
role giver giver
role home site home
role town site town near
role lass person female at home
role rider person male at town
role land kingdom home

stage start
  talk giver
  say "TWENTY WINTERS AGO I FOUND A BABY IN MY HAYRICK WITH A GOLD RING ON A CORD. I RAISED HER AS {LASS}, MY OWN. NOW RIDERS IN GREEN ASK AT EVERY DOOR FOR A GIRL OF TWENTY WITH A RING. I'VE HIDDEN HER IN THE LOFT TWICE. SHE'S TWENTY. SHE LAUGHS AT ME."
  opt "I'LL FIND OUT WHO THEY ARE." -> riders
  opt "LET ME SEE THE RING." -> ring
  opt "HIDE HER AND HOPE." -> refused

stage ring
  talk giver
  say "HERE. A HOUND AND A TOWER, CUT FINE. NOBODY HERE KNOWS THE ARMS. {LASS} WEARS IT ON FEAST DAYS AND THINKS IT'S BRASS. I NEVER TOLD HER OTHERWISE."
  opt "I'LL ASK THE RIDERS ABOUT IT." -> riders

stage refused
  end fail
  journal "YOU TOLD {GIVER} TO HIDE {LASS}."
  do remember giver "THE RIDERS CAME BACK. I SAID SHE'D DIED OF THE FEVER. SHE'S IN THE HAYLOFT. WE DON'T SLEEP MUCH."

stage riders
  goal goto town
  then rider_talk
  journal "RIDERS IN GREEN CLOAKS ARE ASKING ABOUT {GIVER}'S FOUNDLING, {LASS}. THEY WERE SEEN IN {TOWN}."

stage rider_talk
  talk rider
  say "A HOUND AND A TOWER? ...THEN SHE LIVES. SHE IS THE LAST CHILD OF THE HOUSE OF THE HOUND, BURNED OUT IN THE FEUD. HER UNCLE WANTS HER HOME. LANDS, A NAME, A MATCH WITH A LORD OF {LAND}."
  opt "AND IF SHE DOESN'T WANT IT?" -> choice_q
  opt "SHE'S IN {HOME}. GO TO HER." -> revealed
  opt "I'VE NEVER HEARD OF HER." -> denied

stage choice_q
  talk rider
  say "THEN SHE IS A FOOL, AND A FREE ONE. I AM A SWORN MAN, NOT A KIDNAPPER. ASK HER, IF YOU LIKE. I'LL WAIT HERE THREE DAYS, AND THEN I'LL TELL MY LORD THE TRAIL WENT COLD."
  opt "I'LL ASK HER." -> ask_lass

stage ask_lass
  goal goto home
  then lass_talk
  journal "{RIDER} WILL WAIT IN {TOWN} WHILE {LASS} DECIDES WHO SHE WANTS TO BE."

stage lass_talk
  talk lass
  say "A HOUSE? AN UNCLE? I HAVE A HOUSE. THE ROOF LEAKS AND MY FATHER SNORES LIKE A BULL. ...BUT A TOWER. I'VE DREAMED OF A TOWER SINCE I WAS SMALL. WHAT WOULD YOU DO?"
  opt "GO. SEE WHAT YOU ARE." -> goes_end
  opt "STAY. YOU'RE HOME ALREADY." -> stays_end

stage goes_end
  end success
  journal "{LASS} RODE AWAY IN A GREEN CLOAK TO THE HOUSE OF THE HOUND. {GIVER} WAVED UNTIL SHE WAS GONE."
  do moves lass town
  do rep land 5
  do gold 40
  do xp 90
  do remember giver "SHE WRITES EVERY MONTH. SHE SIGNS 'YOUR DAUGHTER', NOT 'LADY'. THAT'S ENOUGH FOR ME. NEARLY."
  do fact "{LASS}, FOUNDLING OF {HOME}, IS THE LAST OF THE HOUSE OF THE HOUND. THE TOWER HAS ITS HEIR."

stage stays_end
  end success
  journal "{LASS} SENT THE RIDERS AWAY. SHE IS A FARMER'S DAUGHTER IN {HOME}, BY CHOICE."
  do gold 30
  do xp 80
  do remember giver "SHE CHOSE THE LEAKY ROOF. SHE WEARS THE RING EVERY DAY NOW, NOT JUST FEAST DAYS. THAT'S HER ANSWER."
  do fact "THE HOUSE OF THE HOUND IS SAID TO HAVE AN HEIR IN {HOME} WHO WOULD RATHER MILK GOATS."

stage revealed
  end success
  journal "YOU TOLD THE RIDERS WHERE {LASS} LIVES. THEY RODE FOR {HOME} THAT HOUR."
  do moves lass town
  do rep land 3
  do gold 60
  do remember giver "YOU TOLD THEM. SHE'S GONE TO BE A LADY. SHE DIDN'T EVEN FINISH HER BREAKFAST."
  do fact "THE HOUSE OF THE HOUND TOOK BACK ITS LAST CHILD FROM A FARM IN {HOME}."

stage denied
  end success
  journal "YOU SENT THE RIDERS AWAY EMPTY-HANDED. {LASS} NEVER KNEW."
  do xp 70
  do remember giver "THE RIDERS LEFT. YOU SENT THEM AWAY? THEN WE OWE YOU EVERYTHING, AND {LASS} DOESN'T EVEN KNOW IT."
)DSL",

R"DSL(
script hungry
title THE MILLER'S BARN
archetype A FLOOD OR FAMINE AS JUDGEMENT
tier 2
hook event famine
pitch "THE HUNGRY TOWN"
role giver giver
role ev event famine
role land kingdom home
role elder person male at giver
role miller person male at giver

stage start
  talk elder
  say "YOU'VE COME IN A BAD SEASON. THE HARVEST FAILED AND THE PRIEST SAYS IT'S JUDGEMENT. FOLK SAY {MILLER} THE MILLER HAS A BARN FULL OF GRAIN HE WON'T SELL. TONIGHT THEY MEAN TO TAKE IT. OR BURN IT."
  opt "I'LL TALK TO THE MILLER." -> miller
  opt "JUDGEMENT FOR WHAT?" -> judgement
  opt "LET THEM TAKE IT." -> mob

stage judgement
  talk elder
  say "OUR FATHERS DROWNED THE OLD VALLEY FOR THE MILLPOND. THREE FARMS UNDER WATER, AND A CHAPEL. THE PRIEST SAYS THE RIVER REMEMBERS. I SAY HUNGRY PEOPLE WILL BELIEVE ANYTHING THAT HAS A VILLAIN IN IT."
  opt "I'LL TALK TO THE MILLER." -> miller
  opt "THEN LET THEM HAVE THEIR VILLAIN." -> mob

stage miller
  talk miller
  say "HOARDING? I'M KEEPING SEED. EAT THE SEED AND THERE'S NO HARVEST NEXT YEAR EITHER, AND WE ALL DIE SLOWER. ...AND {LAND}'S TAX MEN PAY DOUBLE FOR WHAT'S LEFT. I WON'T LIE TO YOU ABOUT THAT."
  opt "SELL HALF, FAIRLY. KEEP THE SEED." check level 3 -> fair else stubborn
  opt "I'LL BUY A CART FOR THE TOWN. [60]" if gold 60 -> bought
  opt "OPEN THE BARN, OR I WILL." -> forced

stage stubborn
  talk miller
  say "NO. NOT FOR YOU, NOT FOR THE PRIEST, NOT FOR THE RIVER. MY FATHER DUG THAT POND WITH HIS HANDS. I WON'T BE ROBBED FOR IT."
  opt "THEN I'LL BUY IT. [60 GOLD]" if gold 60 -> bought
  opt "THEN I'LL OPEN IT FOR THEM." -> forced
  opt "THEN THE MOB CAN HAVE YOU." -> mob

stage fair
  talk miller
  say "...HALF, AT LAST YEAR'S PRICE. AND THE SEED STAYS LOCKED. YOU'LL STAND AT THE DOOR WITH ME WHEN I TELL THEM? THEY WON'T THROW STONES AT SOMEONE WITH A SWORD."
  opt "I'LL STAND WITH YOU." -> fair_end

stage bought
  talk miller
  say "A CART FOR SIXTY. YOU'RE EITHER A SAINT OR A FOOL. THE TOWN WILL SAY SAINT, AND I'LL SAY NOTHING, AND WE'LL ALL EAT."
  do gold -60
  opt "SEE THAT THEY ALL GET SOME." -> bought_end

stage forced
  talk miller
  say "YOU'D ROB A MAN FOR STRANGERS? ...FINE. TAKE IT. TAKE THE SEED TOO. AND WHEN THE FIELDS ARE BARE NEXT SPRING, REMEMBER WHO OPENED THE DOOR."
  opt "THEY'RE HUNGRY NOW." -> forced_end

stage mob
  talk elder
  say "...SO BE IT. GODS FORGIVE US. I'LL OPEN THE GATE FOR THEM MYSELF, SO AT LEAST NOBODY BREAKS IT. AND I'LL STAND IN FRONT OF THE MILLER'S CHILDREN WHILE THEY DO IT."
  opt "IT'S YOUR TOWN." -> mob_end

stage fair_end
  end success
  journal "THE MILLER SOLD HALF HIS GRAIN AT LAST YEAR'S PRICE AND KEPT THE SEED. {GIVER} WILL SOW AGAIN."
  do rep land 3
  do xp 90
  do remember elder "WE'LL SOW IN SPRING. NOBODY BURNED ANYTHING. I DIDN'T THINK I'D SAY THAT THIS YEAR."
  do fact "THE MILLER OF {GIVER} SOLD GRAIN AT LAST YEAR'S PRICE IN THE HUNGRY SEASON, WITH A STRANGER AT HIS DOOR."

stage bought_end
  end success
  journal "YOU BOUGHT A CART OF GRAIN FOR {GIVER}. THEY ARE CALLING YOU A SAINT. YOU ARE POORER."
  do fame 2
  do xp 80
  do remember elder "THE SAINT OF THE CART! THE CHILDREN MADE A SONG. IT'S TERRIBLE. THEY SING IT EVERY NIGHT."
  do fact "A STRANGER BOUGHT BREAD FOR ALL OF {GIVER} IN THE HUNGRY SEASON."

stage forced_end
  end success
  journal "YOU OPENED THE MILLER'S BARN. {GIVER} ATE TONIGHT, AND THE SEED WITH IT."
  do realm famine giver
  do xp 60
  do remember miller "COME TO SEE THE BARE FIELDS? THERE THEY ARE. YOU FED THEM ONE WINTER. NOW FEED THEM THE NEXT."
  do fact "THE SEED CORN OF {GIVER} WAS EATEN IN THE HUNGRY SEASON. NEXT YEAR WILL BE WORSE."

stage mob_end
  end success
  journal "THE MOB TOOK THE MILLER'S BARN. NOBODY DIED. THE MILL WILL NOT TURN AGAIN THIS YEAR."
  do realm event harvestfailed land giver
  do xp 40
  do remember miller "THEY TOOK THE GRAIN AND BROKE THE WHEEL FOR GOOD MEASURE. YOU WATCHED. I SAW YOU WATCH."
  do fact "THE MILL OF {GIVER} WAS BROKEN BY A HUNGRY MOB, AND ITS MILLER WILL NOT FORGET WHO STOOD BY."
)DSL",

R"DSL(
script hollowhill
title THE HOLLOW HILL
archetype A DEAL WITH A FAE COURT
tier 2
hook npc hunter
pitch "WHO ARE THE TRACKS FOR?"
hint "BAREFOOT TRACKS, LEADING INTO THE HILL. NO TOES. I'VE HUNTED THIRTY YEARS AND I'VE NEVER SEEN A TRACK WITH NO TOES."
role giver giver
role home site home
role hill site cave near
role lady person female at hill
role lad person male at home
var paid 0

stage start
  talk giver
  say "MY {LAD.SON} {LAD} FOLLOWED A LIGHT INTO THE HOLLOW HILL AT {HILL} THREE NIGHTS AGO. THE OLD WIVES SAY THE FAIR FOLK KEEP WHAT WANDERS IN. I'VE GONE IN WITH A TORCH. THERE'S NOTHING THERE BUT ROCK AND LAUGHTER."
  do hide lad
  opt "I'LL GO TO THE HILL." -> seek
  opt "THE FAIR FOLK? REALLY?" -> doubt
  opt "I CAN'T HELP YOU." -> refused

stage doubt
  talk giver
  say "I KNOW A WOLF'S TRACK AND A MAN'S. THESE WENT INTO THE HILL AND DIDN'T COME OUT, AND THERE WERE NO TOES. AT NIGHT I HEAR FIDDLES UNDER THE TURF. SO YES. REALLY."
  opt "I'LL GO TO THE HILL." -> seek

stage refused
  end fail
  journal "YOU LEFT {GIVER} TO LISTEN FOR FIDDLES UNDER THE TURF."
  do remember giver "I GO TO THE HILL EVERY NIGHT AND LEAVE MILK AND A HEEL OF BREAD. SOMETIMES I THINK I HEAR {LAD.HIM} LAUGHING. THAT'S THE WORST OF IT."

stage seek
  goal goto hill
  then court
  say "TAKE IRON. AND DON'T EAT ANYTHING THEY OFFER YOU. AND DON'T SAY THANK YOU. MY GRANDMOTHER SAID NEVER SAY THANK YOU TO THEM."
  journal "{GIVER}'S {LAD.SON} {LAD} WENT INTO THE HOLLOW HILL AT {HILL}, {HILL.DIR}. THE FAIR FOLK MAY HAVE {LAD.HIM}."

stage court
  talk lady
  say "A MORTAL WITH MANNERS, OR WITHOUT? WE SHALL SEE. THE CHILD DANCES WITH US. THE CHILD IS HAPPY. WHY TAKE HAPPINESS HOME TO A HOUSE THAT SMELLS OF BLOOD AND WET DOG?"
  opt "BECAUSE {LAD.HE} IS LOVED THERE." -> price
  opt "WHAT WOULD YOU TAKE FOR {LAD.HIM}?" -> price
  opt "BECAUSE I HAVE COLD IRON." -> iron

stage price
  talk lady
  say "EVERYTHING HAS A PRICE, AND MINE IS SMALL. GIVE ME YOUR NAME. NOT TO KEEP. TO KNOW. OR SING ME A SONG I HAVE NEVER HEARD, IF YOU HAVE ONE IN YOU. OR GO HOME, AND LET THE CHILD DANCE."
  opt "TAKE MY NAME." -> name
  opt "A TEMPLE HYMN, THEN." if bg novice -> song
  opt "A SONG OF MY OWN." check level 4 -> song else badsong
  opt "NO PRICE. I'LL TAKE {LAD.HIM}." -> iron

stage badsong
  talk lady
  say "THAT IS A DRINKING SONG FROM A HUNDRED TAVERNS. I HAVE HEARD IT SUNG BY DROWNED SAILORS AND HANGED MEN. TRY AGAIN, OR PAY, OR GO."
  opt "TAKE MY NAME, THEN." -> name
  opt "I'LL TAKE {LAD.HIM} BY FORCE." -> iron

stage name
  talk lady
  say "{PLAYER}. OH, THAT IS A PRETTY ONE. I SHALL SAY IT SOMETIMES WHEN YOU ARE ASLEEP, AND YOU WILL DREAM OF DANCING. TAKE THE CHILD. MIND THE STEP: IT IS ALWAYS ONE MORE THAN YOU THINK."
  do set paid 1
  opt "COME, {LAD}. HOME." -> back

stage song
  talk lady
  say "...OH. OH, THAT WAS NEW. NO ONE HAS SUNG ME ANYTHING NEW IN THREE HUNDRED YEARS. TAKE THE CHILD, AND TAKE THIS FOR THE SONG. DON'T SPEND IT. IT WOULD BE RUDE."
  do set paid 2
  do give "A LEAF OF FAE SILVER" gem
  opt "COME, {LAD}. HOME." -> back

stage iron
  talk lady
  say "IRON. HOW RUDE. HOW VERY MORTAL. TAKE {LAD.HIM}, THEN, AND KNOW THAT THE HILL REMEMBERS RUDENESS LONGER THAN IT REMEMBERS KINDNESS. AND IT REMEMBERS KINDNESS FOREVER."
  do set paid 3
  opt "LET IT REMEMBER." -> back

stage back
  goal goto home
  then reunion
  do show lad
  journal "{LAD} IS OUT OF THE HILL, BLINKING AT THE SUN AND HUMMING. BRING {LAD.HIM} HOME TO {GIVER} IN {HOME}."

stage reunion
  talk giver
  say "{LAD}! {LAD}! ...{LAD.HE} SAYS {LAD.HE} WAS GONE ONE NIGHT. ONE NIGHT, AND {LAD.HIS} HAIR IS LONGER. WHAT DID YOU GIVE THEM? WHAT DID IT COST?"
  opt "ONLY MY NAME." if var paid 1 -> name_end
  opt "A SONG. THAT WAS ALL." if var paid 2 -> song_end
  opt "COLD IRON. SHE WON'T FORGET." if var paid 3 -> iron_end
  opt "NOTHING YOU NEED TO WORRY ABOUT." -> iron_end

stage name_end
  end success
  journal "THE FAIR FOLK OF {HILL} KNOW YOUR NAME NOW. {LAD} IS HOME, AND HUMS TUNES NOBODY TAUGHT {LAD.HIM}."
  do gold 40
  do xp 90
  do fame -1
  do remember giver "YOU STILL DREAM OF DANCING? {LAD} DOES. WE DON'T TALK ABOUT IT. WE JUST LEAVE THE WINDOW OPEN."
  do fact "THE FAIR FOLK OF {HILL} TRADED {GIVER}'S CHILD FOR A STRANGER'S NAME."

stage song_end
  end success
  journal "YOU SANG FOR THE LADY OF THE HILL, AND SHE LET {LAD} GO. SHE ALSO PAID YOU, WHICH NOBODY IN {HOME} BELIEVES."
  do gold 30
  do xp 110
  do remember giver "A SONG! THEY TOOK A SONG! I'VE BEEN SINGING AT THE HILL EVERY NIGHT SINCE. NOTHING. THEY MUST HAVE TASTE."
  do fact "SOMEONE SANG A NEW SONG UNDER THE HOLLOW HILL AT {HILL}, AND THE FAIR FOLK PAID IN SILVER."

stage iron_end
  end success
  journal "YOU TOOK {LAD} FROM THE HILL WITH COLD IRON. THE HILL REMEMBERS."
  do gold 40
  do xp 80
  do mark hill_insulted
  do remember giver "THE MILK SOURS EVERY THIRD DAY NOW. THE DOG WON'T GO NEAR THE HILL. BUT {LAD} IS HOME, SO THE MILK CAN SOUR."
  do fact "THE HOLLOW HILL AT {HILL} REMEMBERS AN INSULT, AND THE MILK OF {HOME} SOURS FOR IT."
)DSL",

R"DSL(
script pardon
title THE EXILE'S PLEA
archetype AN EXILE AND A HOMECOMING
tier 2
hook npc villager
pitch "YOUR ACCENT ISN'T FROM HERE."
hint "NO. I'M FROM FURTHER OFF THAN YOU'D GUESS. FIFTEEN YEARS FURTHER."
role giver giver
role home site home
role land kingdom home
role rival kingdom rival
role lord ruler rival
role seat capital rival

stage start
  talk giver
  say "I WAS A SWORN SHIELD OF {LORD} OF {RIVAL}. FIFTEEN YEARS AGO I WAS BANISHED FOR A THEFT FROM THE TREASURY. I DIDN'T DO IT. I KNOW WHO DID, AND HE'S DEAD NOW. I WANT TO GO HOME BEFORE I DIE TOO."
  opt "WHAT WOULD YOU HAVE ME DO?" -> ask
  opt "HOW DO I KNOW YOU'RE INNOCENT?" -> proof
  opt "THAT'S A LONG ROAD FOR A STRANGER." -> refused

stage proof
  talk giver
  say "YOU DON'T. WEIGH IT: THE TREASURER WAS MY BROTHER-IN-ARMS. I TOOK THE BLAME BECAUSE HE HAD THREE CHILDREN AND I HAD A HORSE. HE DIED LAST WINTER. HIS WIDOW WROTE TO ME. THE LETTER IS ALL THE PROOF I HAVE."
  opt "GIVE ME THE LETTER." -> ask
  opt "A FALSE CONFESSION IS STILL A LIE." -> ask

stage refused
  end fail
  journal "YOU WOULD NOT CARRY {GIVER}'S PLEA TO {RIVAL}."
  do remember giver "STILL HERE. STILL FROM SOMEWHERE ELSE. I SAY GOOD MORNING IN MY OWN TONGUE, QUIETLY, SO I DON'T FORGET HOW IT GOES."

stage ask
  talk giver
  say "TAKE THIS TO {SEAT}, TO THE COURT OF {LORD}. IF THE {LORD.TITLE} PARDONS ME, I WALK HOME. IF NOT, AT LEAST SOMEONE WILL HAVE SAID IT OUT LOUD IN THAT HALL, AFTER FIFTEEN YEARS."
  do give "THE EXILE'S PLEA" letter
  opt "I'LL CARRY IT." -> travel

stage travel
  goal goto seat
  then audience
  journal "CARRY {GIVER}'S PLEA TO {SEAT}, CAPITAL OF {RIVAL}, AND PUT IT IN THE HANDS OF {LORD}."

stage audience
  talk lord
  say "{GIVER}? I REMEMBER. I SIGNED THE BANISHMENT MYSELF, AND I WAS YOUNG AND VERY SURE OF THINGS. ...GIVE IT HERE. LET ME READ WHAT AN EXILE HAS TO SAY TO A CROWN."
  opt "{GIVER.HE} LIED TO SAVE A FRIEND." -> mercy
  opt "THE TRUE THIEF IS DEAD. IT'S DONE." -> justice
  opt "I'LL ANSWER FOR {GIVER.HIM}." check level 8 -> respect else rebuke

stage mercy
  talk lord
  say "A FALSE CONFESSION IS STILL A LIE TO THE CROWN. BUT A LIE FOR A FRIEND'S CHILDREN... I HAD A FRIEND ONCE TOO. {GIVER.HE} MAY COME HOME. {GIVER.HE} MAY NOT SERVE AGAIN. TELL {GIVER.HIM} THAT, EXACTLY."
  do take "THE EXILE'S PLEA"
  opt "I'LL TELL {GIVER.HIM} EXACTLY." -> home_pardon

stage justice
  talk lord
  say "JUSTICE IS NOT A MATTER OF WHO HAPPENS TO BE DEAD. NO. THE BANISHMENT STANDS. TELL {GIVER.HIM} I AM SORRY. I AM, A LITTLE. THAT IS MORE THAN MOST OF THEM GET."
  do take "THE EXILE'S PLEA"
  opt "THEN I'LL CARRY THAT BACK." -> home_refused

stage respect
  talk lord
  say "BOLD. MY FATHER WOULD HAVE HAD YOU WHIPPED FOR THAT. I RATHER LIKE IT. {GIVER.HE} MAY COME HOME, AND YOU MAY COME TO MY COURT WHEN YOU PLEASE. DON'T PLEASE TOO OFTEN."
  do take "THE EXILE'S PLEA"
  do rep rival 5
  opt "YOU'RE GENEROUS." -> home_pardon

stage rebuke
  talk lord
  say "YOU STAND IN MY HALL AND THREATEN ME ON A TRAITOR'S BEHALF? GUARDS, SEE THIS ONE TO THE GATE. THE BANISHMENT STANDS, AND NOW IT HAS A REASON."
  do take "THE EXILE'S PLEA"
  do rep rival -10
  opt "SO MUCH FOR COURTESY." -> home_refused

stage home_pardon
  goal goto home
  then joy
  journal "{LORD} HAS PARDONED {GIVER}. CARRY THE NEWS BACK TO {HOME}."

stage joy
  talk giver
  say "PARDONED? I CAN... I CAN GO HOME? I'VE HAD A BAG PACKED UNDER MY BED FOR FIFTEEN YEARS. THE CLOTHES IN IT DON'T FIT ANY MORE. I DON'T CARE. I DON'T CARE AT ALL."
  opt "GO HOME, SHIELD." -> pardon_end

stage pardon_end
  end success
  journal "{GIVER} WAS PARDONED BY {LORD} AND IS GOING HOME TO {RIVAL}."
  do gold 70
  do xp 120
  do rep rival 5
  do remember giver "I LEAVE AT THE NEW MOON. HOME. I'D FORGOTTEN HOW THAT WORD TASTES. LIKE BREAD, IT TURNS OUT."
  do fact "{GIVER}, AN EXILE OF {RIVAL}, WAS PARDONED BY {LORD} AND WENT HOME AFTER FIFTEEN YEARS."

stage home_refused
  goal goto home
  then grief
  journal "{LORD} WILL NOT PARDON {GIVER}. TAKE THE ANSWER BACK TO {HOME}."

stage grief
  talk giver
  say "NO. ...WELL. I KNEW. I KNEW, BUT I HAD TO ASK. THANK YOU FOR SAYING IT IN THAT HALL. SOMEONE HEARD IT. THAT WILL HAVE TO DO."
  opt "THEY HEARD IT. I MADE SURE." -> refused_end

stage refused_end
  end success
  journal "{GIVER} REMAINS AN EXILE. BUT THE TRUTH WAS SPOKEN IN THE HALL OF {LORD}."
  do gold 30
  do xp 80
  do remember giver "I UNPACKED THE BAG. FIFTEEN YEARS, AND I FINALLY UNPACKED IT. IT FEELS LIKE LIVING HERE, AT LAST."
)DSL",

R"DSL(
script laststand
title THE ROAD AT DAWN
archetype A SACRIFICIAL STAND
tier 2
hook event wardeclared
pitch "THE OLD CAPTAIN'S CALL"
role giver giver
role land kingdom home
role war war land
role captain person male at giver
role raider foe male at giver
role folk npc villager at giver

stage start
  talk captain
  say "THE WAR HAS COME TO {GIVER}, WHETHER {GIVER} LIKES IT OR NOT. FORAGERS FROM {WAR.FOE} BURNED THE NEXT FARM DOWN THE ROAD. THEY'LL BE HERE AT DAWN. I'M SIXTY-ONE, MY SWORD ARM IS MOSTLY MEMORY, AND I'M ALL THE GARRISON THERE IS."
  do hide raider
  opt "THEN I'LL STAND WITH YOU." -> plan
  opt "GET THE VILLAGE OUT. RUN." -> flee
  opt "WHY STAY AT ALL?" -> why

stage why
  talk captain
  say "MY WIFE IS IN THE CHURCHYARD HERE. MY SON TOO. I'VE RUN FROM ENOUGH THINGS IN MY LIFE. AT MY AGE YOU CHOOSE WHERE YOU STAND, IF YOU'RE LUCKY, AND I'M LUCKY."
  opt "THEN I'LL STAND WITH YOU." -> plan
  opt "STAND, THEN. I'LL GET THEM OUT." -> alone

stage flee
  talk captain
  say "RUN? WHERE TO, WITH THE CARTS AND THE OLD ONES? THEY'D RIDE US DOWN BY NOON. ...NO. YOU TAKE THEM INTO THE WOODS. I'LL SLOW THE FORAGERS DOWN. I'M GOOD AT SLOW."
  opt "THEN I'LL STAND WITH YOU." -> plan
  opt "AS YOU WISH, CAPTAIN." -> alone

stage plan
  talk captain
  say "GOOD. THEIR LEADER IS {RAIDER}. KILL HIM AND THE REST RUN: FORAGERS AREN'T SOLDIERS, THEY'RE THIEVES WITH PERMISSION. GET SOME SLEEP. WE START AT FIRST LIGHT."
  opt "AT FIRST LIGHT." -> dawn

stage dawn
  goal wait 1
  then fight
  journal "{CAPTAIN} AND YOU WILL HOLD THE ROAD INTO {GIVER} AT DAWN AGAINST THE FORAGERS OF {WAR.FOE}."

stage fight
  goal slay raider
  then after
  do show raider
  do sound bell
  journal "THE FORAGERS ARE HERE. KILL {RAIDER}, THEIR LEADER, AND THE REST WILL RUN."

stage after
  talk captain
  say "HA! THEY RAN! THEY RAN FROM AN OLD MAN AND A STRANGER! ...I'M BLEEDING, I THINK. NO, NO, IT'S NOTHING. IT'S NOTHING A DRINK WON'T..."
  opt "LET ME BIND THE WOUND." check level 3 -> saved_end else lost_end
  opt "SIT DOWN, CAPTAIN. YOU'VE EARNED IT." -> lost_end

stage alone
  goal wait 1
  then alone_end
  journal "{CAPTAIN} HOLDS THE ROAD INTO {GIVER} ALONE AT DAWN, WHILE THE VILLAGE HIDES IN THE WOODS."

stage saved_end
  end success
  journal "YOU AND {CAPTAIN} HELD THE ROAD INTO {GIVER}. HE LIVES, AND TELLS IT BADLY AND OFTEN."
  do rep land 6
  do gold 50
  do xp 140
  do remember captain "THERE'S MY SHIELD-MATE! TELL THEM HOW MANY THERE WERE. NO, MORE THAN THAT. MORE."
  do remember folk "THE STRANGER WHO STOOD WITH OLD {CAPTAIN}! YOU'LL NOT PAY FOR A THING IN {GIVER}."
  do fact "OLD {CAPTAIN} AND A STRANGER HELD THE ROAD INTO {GIVER} AGAINST THE FORAGERS OF {WAR.FOE}."

stage lost_end
  end success
  journal "YOU HELD THE ROAD INTO {GIVER}. {CAPTAIN} DIED OF HIS WOUND BEFORE NOON, SITTING IN THE SUN."
  do hide captain
  do rep land 5
  do gold 40
  do xp 130
  do remember folk "WE BURIED {CAPTAIN} BESIDE HIS WIFE. YOU STOOD WITH HIM. THAT'S WORTH SOMETHING HERE. IT'S WORTH EVERYTHING."
  do fact "OLD {CAPTAIN} DIED HOLDING THE ROAD INTO {GIVER}. THE VILLAGE CARVED HIS NAME ON THE MILESTONE."

stage alone_end
  end success
  journal "{CAPTAIN} HELD THE ROAD ALONE AND DID NOT COME BACK. EVERY SOUL OF {GIVER} REACHED THE WOODS ALIVE."
  do hide captain
  do hide raider
  do rep land 3
  do xp 90
  do remember folk "HE BOUGHT US THE MORNING. ONE OLD MAN. WE'RE ALL HERE BECAUSE OF ONE OLD MAN AND YOU."
  do fact "OLD {CAPTAIN} HELD THE ROAD INTO {GIVER} ALONE AND DIED THERE, SO THAT THE VILLAGE COULD RUN."
)DSL",

R"DSL(
script psalter
title THE NAME AMONG THE DEAD
archetype THE RETURN TO A LOST HOMELAND
tier 2
hook board
pitch "READ: MISSING CHILD"
hint "MISSING: A CHILD OF FIFTEEN, LAST SEEN ON THE ROAD TO THE OLD RUIN. ASK FOR THE MOTHER BY THE WELL. REWARD."
role giver giver
role ruin ruin near
role mother person female at giver
role girl person female at ruin

stage start
  talk mother
  say "YOU READ THE NOTICE? NOBODY READS THE NOTICES. MY {GIRL.SON} {GIRL} WENT TO {RUIN} TO FIND {GIRL.HIS} GRANDFATHER'S NAME ON THE OLD GRAVES. OUR FAMILY CAME FROM {RUIN.OLD}, BEFORE IT FELL. THAT WAS THREE DAYS AGO."
  opt "I'LL FIND {GIRL.HIM}." -> seek
  opt "WHY WOULD {GIRL.HE} GO THERE?" -> why

stage why
  talk mother
  say "{GIRL.HE} WANTS TO KNOW WHO WE WERE. I TOLD {GIRL.HIM} WE WERE NOBODY AND IT DIDN'T MATTER. I LIED ON BOTH COUNTS, AND {GIRL.HE} KNEW IT. {GIRL.HE} ALWAYS KNOWS."
  opt "I'LL FIND {GIRL.HIM}." -> seek

stage seek
  goal goto ruin
  then found
  journal "{GIRL}, A CHILD OF {GIVER}, WENT TO {RUIN} TO FIND A GRANDFATHER'S GRAVE. FIND {GIRL.HIM}."

stage found
  talk girl
  say "I CAN'T GO BACK IN. THERE ARE DEAD MEN IN THERE, WALKING. BUT I FOUND HIM! GRANDFATHER'S NAME, ON A STONE. AND HIS PSALTER IS STILL IN THERE. I DROPPED IT WHEN THEY CAME. IT HAS OUR NAMES IN THE FRONT."
  opt "GO HOME. I'LL GET THE PSALTER." -> fetch
  opt "FORGET THE BOOK. WE LEAVE NOW." -> leave

stage fetch
  goal fetch "A WATER-STAINED PSALTER" in ruin
  then homeward_book
  do moves girl giver
  journal "{GIRL} HAS GONE HOME. {GIRL.HIS} GRANDFATHER'S PSALTER LIES SOMEWHERE DEEP IN {RUIN}."

stage homeward_book
  goal goto giver
  then book
  journal "BRING THE PSALTER FROM {RUIN} HOME TO {GIRL}'S MOTHER IN {GIVER}."

stage book
  talk mother
  say "{GIRL} IS HOME, AND YOU... THE PSALTER. HER GRANDFATHER'S. LOOK, IN THE FRONT: OUR NAMES, BACK AND BACK, TO SERVANTS OF {RUIN.LORD}. WE WERE NOT NOBODY. I WAS WRONG TO SAY WE WERE."
  opt "KEEP IT. IT'S YOURS." -> book_end
  opt "SOME NAMES ARE BETTER LEFT." -> book_end

stage leave
  goal goto giver
  then plain
  do moves girl giver
  journal "BRING {GIRL} SAFE HOME TO {GIVER}."

stage plain
  talk mother
  say "{GIRL}! YOU STUPID, WONDERFUL... THANK YOU. THANK YOU. {GIRL.HE} SAYS {GIRL.HE} FOUND THE NAME. THAT'S ENOUGH. A NAME IS ENOUGH, FOR NOW."
  opt "BRAVE, LIKE {GIRL.HIS} KIN." -> plain_end

stage book_end
  end success
  journal "THE PSALTER OF A FAMILY OF {RUIN.OLD} IS HOME IN {GIVER}, AND ITS NAMES ARE READ ALOUD."
  do take "A WATER-STAINED PSALTER"
  do gold 50
  do xp 100
  do remember mother "WE READ THE NAMES ON FEAST DAYS NOW. ALL OF THEM. IT TAKES AN HOUR. NOBODY MINDS."
  do fact "A FAMILY OF {GIVER} DESCENDS FROM THE SERVANTS OF {RUIN.LORD}, LAST LORD OF {RUIN.OLD}."

stage plain_end
  end success
  journal "{GIRL} IS SAFE HOME IN {GIVER}. THE PSALTER STILL LIES IN {RUIN}."
  do gold 40
  do xp 80
  do remember mother "{GIRL} TALKS OF GOING BACK FOR THE BOOK. I SAY WHEN SHE'S OLDER. SHE SAYS I SAID THAT LAST YEAR. SHE'S RIGHT. SHE'S USUALLY RIGHT."
)DSL",
};

}  // namespace

const char* talesSource() {
  static const std::string all = [] {
    std::string s;
    for (const char* t : kTales) s += t;
    return s;
  }();
  return all.c_str();
}

}  // namespace dsl
}  // namespace story
