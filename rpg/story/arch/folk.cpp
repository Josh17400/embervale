// M6b "Sagas": archetypes, folk and fairy tale (close retellings allowed). VOICE lane. The template markup and role
// conventions: rpg/story/saga.h.
//
//   cursed_gift      THE GIFT THAT WOULD NOT LEAVE    a pretty thing bought for a copper; it can only be sold, given or
//                                                     buried with silver where roads meet (and who deserves it?)
//   fair_bargain     WHAT GREETS YOU AT THE GATE      seven full barns for the first thing to greet the giver at the
//                                                     gate; the fair folk come to collect (the letter of the bargain)
//   changeling       THE CHILD WHO WAS NOT            a cradle that watches like a grandfather; ale brewed in an
//                                                     eggshell; an old one of the hill who wanted to die loved
//   true_name        THE NAME THAT BINDS              a little grey spinner's price and the name he sings by his fire
//   three_tasks      THE THREE TASKS                  a hard father's tasks for a suitor; the third is a truth
//   youngest         THE YOUNGEST OF THREE            the proud elder brothers sleep by the spring; bread for a wayfarer
//   thorn_sleep      THE SLEEPER IN THE THORNS        a slighted wise woman's curse at a naming feast; who was not invited
//   wild_man         THE WILD MAN OF THE WOOD         the thing that kills sheep was a husband, a brother, a son
//   piper            THE PIPER UNPAID                 the rats went into the river; the reeve kept the purse; the children
//                                                     hum a tune in their sleep
//   wolf_door        THE WOLF AT THE DOOR             a sweet voice at the barred door, singing the children's names
//   honest_axe       THE HONEST AXE                   gold, silver and iron at the drowned pool; the greedy neighbour
//   stolen_shadow    THE STOLEN SHADOW                a shadow sold for a run of luck; the shadow does not want to go back
//   feather_cloak    THE FEATHER CLOAK                a wife who came out of the water, and the cloak hidden from her
//   stone_soup       THE STONE SOUP                   a lean season, a stranger's pot and a stone; every door shut
#include <vector>
#include "rpg/story/arch/arch_tables.h"

namespace story {
namespace saga {
namespace arch {

namespace {

const char* const kCursedGift = R"SAGA(
title [[THE GIFT THAT WOULD NOT LEAVE|A PRETTY THING FOR A COPPER|THE CROSSROADS BARGAIN]]
hook npc any
pitch "[[pitch=YOU KEEP LOOKING AT THAT DOOR.|SOMEONE IN THERE ISN'T WELL?|WHAT'S WRONG IN THAT HOUSE?]]"
hint "[[~pitch|DON'T MIND ME. I'M WAITING FOR SOMEONE TO TAKE A THING OFF. THEY WON'T.|DON'T MIND ME. I'M WAITING FOR SOMEONE TO TAKE A THING OFF. THEY WON'T.|IT WAS A COPPER AT A FAIR. A COPPER. HOW CAN A COPPER COST SO MUCH?]]"
role giver giver
role home site home
role kin resident kin of giver
role cross site village near
role peddler person female at cross
var back 0
var bury 0
var pass 0
slot t1 -> retrieve deceit rival wonder world
slot t2 -> bury mercy price wonder

stage start
  talk giver
  say "MY {KIN.KIN} {KIN} BOUGHT [[gift=A SILVER COMB|A GLASS NECKLACE|A RED SHAWL|A BONE FLUTE]] FROM A PEDDLER AT {CROSS} FOR A COPPER. SINCE THEN [[THE MILK SOURS WHEN {KIN.HE} PASSES|{KIN.HE} NEITHER SLEEPS NOR EATS|EVERY CANDLE GUTTERS NEAR {KIN.HIM}]], AND {KIN.HE} WON'T TAKE IT OFF. <<plea>>"
  opt "I'LL FIND THE PEDDLER." -> seek
  opt "TAKE IT OFF {KIN.HIM} BY FORCE." -> force
  opt "IT'S ONLY A TRINKET." -> refused

stage force
  talk kin
  say "<<threat:greed>> IT'S MINE. I PAID FOR IT. IT'S THE ONLY PRETTY THING I'VE EVER OWNED, AND IT LIKES ME. IT TELLS ME SO AT NIGHT."
  opt "THEN I'LL FIND WHO SOLD IT." -> seek
  opt "...KEEP IT, THEN." -> refused

stage refused
  end fail
  say "<<farewell>> I'LL SIT WITH {KIN.HIM} TONIGHT. SOMEONE SHOULD."
  journal "YOU LEFT {KIN} OF {HOME} WITH [[=gift]], AND WHATEVER CAME WITH IT."
  do remember giver "[[=gift]]. STILL ON. STILL PRETTY. {KIN} IS NEITHER."

stage seek
  goal goto cross
  then peddler
  say "SHE HAD A GREY CART AND ONE GLOVE. <<warning>>"
  journal "{KIN} OF {HOME} WILL NOT PART WITH [[=gift]] BOUGHT AT {CROSS}, AND IS FADING. FIND THE PEDDLER WHO SOLD IT."

stage peddler
  talk peddler
  say "[[=gift]]? SO IT FOUND A NEW NECK. I DIDN'T SELL IT, STRANGER. I GOT FREE OF IT. IT CAN'T BE STOLEN OR THROWN AWAY: ONLY SOLD, ONLY GIVEN, OR BURIED WITH SILVER WHERE ROADS MEET. MY [[MOTHER|AUNT|FIRST HUSBAND]] SOLD IT TO ME."
  opt "THEN TAKE IT BACK YOURSELF." -> takeback
  opt "I'LL BURY IT, THEN." -> how
  opt "WHO WOULD DESERVE IT?" -> deserve

stage takeback
  talk peddler
  say "TAKE IT BACK. ELEVEN YEARS I CARRIED IT, AND THEN I SOLD IT TO THE FIRST FOOL WHO SMILED AT MY CART. THAT WAS IT TALKING IN ME. <<apology:shame>> YES. TAKE ME TO {HOME}."
  do set back 1
  do moves peddler home
  opt "WALK WITH ME." -> @t1

stage how
  talk peddler
  say "YOUR OWN SILVER, NOT BORROWED, AND THE ROAD MUST TAKE IT AFTER DARK, AT {CROSS} WHERE THE FOUR WAYS MEET. I NEVER HAD SILVER ENOUGH. <<blessing>>"
  do set bury 1
  opt "THEN I'LL FETCH IT." -> @t1

stage deserve
  talk peddler
  say "THE [[usurer=MONEYLENDER|TOLL-KEEPER|GRAIN FACTOR]] OF {HOME}. HE TOOK EVERY ROOF ON MY MOTHER'S STREET FOR DEBT. LET HIM BUY A PRETTY THING CHEAP. <<curse>>"
  opt "SO BE IT." -> pass_set
  opt "NO. THAT ISN'T JUSTICE." -> how

stage pass_set
  talk peddler
  say "THEN SELL IT TO HIM FOR A COPPER, AND SMILE WHEN YOU DO. IT LIKES A SMILE."
  do set pass 1
  opt "I'LL SMILE." -> @t1

stage retrieve
  goal goto home
  then give_up
  journal "[[=gift]] CAN ONLY BE SOLD OR GIVEN. GO BACK TO {HOME} AND ASK {KIN} TO PART WITH IT."

stage give_up
  talk kin
  say "YOU WANT IT? ...I'M SO TIRED. IT HAS TO BE SOLD, DOESN'T IT? I DON'T KNOW HOW I KNOW THAT. A COPPER, THEN. TAKE IT BEFORE I CHANGE MY MIND."
  do give "[[=gift]]" amulet
  opt "BACK TO THE PEDDLER." if var back 1 -> returned
  opt "TO THE CROSSROADS." if var bury 1 -> @t2
  opt "TO THE [[=usurer]]." if var pass 1 -> sold

stage returned
  end success
  say "THE PEDDLER TOOK [[=gift]] IN BOTH HANDS LIKE A CHILD COME HOME. {KIN} SLEPT A DAY AND A NIGHT. THE PEDDLER WAS GONE BEFORE DAWN, SINGING."
  journal "THE PEDDLER TOOK BACK [[=gift]] AND ITS CURSE. {KIN} IS WAKING UP."
  do take "[[=gift]]"
  do reward fair
  do befriend kin
  do remember giver "{KIN} ATE TWO BOWLS TONIGHT. TWO. <<thanks>>"
  do fact "A PEDDLER CAME TO {HOME} TO TAKE BACK HER OWN CURSE, AND WALKED AWAY SINGING UNDER IT."

stage bury
  goal goto cross
  then buried
  journal "CARRY [[=gift]] TO {CROSS}, WHERE THE FOUR WAYS MEET, AND BURY IT WITH SILVER OF YOUR OWN."

stage buried
  end success
  say "YOU DUG BY STARLIGHT AND LAID YOUR OWN SILVER ON TOP. IN THE MORNING THE GROUND WAS SMOOTH, AS IF NO SPADE HAD EVER TOUCHED IT."
  journal "[[=gift]] LIES UNDER THE CROSSROADS AT {CROSS} WITH YOUR SILVER. {KIN} IS WELL AGAIN."
  do take "[[=gift]]"
  do gold -10
  do reward fair
  do befriend kin
  do remember kin "I DREAM OF IT SOMETIMES. IN THE DREAM I DON'T WANT IT. THAT'S HOW I KNOW I'M WELL."
  do fact "SOMETHING IS BURIED WHERE THE ROADS MEET AT {CROSS}. TRAVELLERS STEP AROUND THE SPOT WITHOUT KNOWING WHY."

stage sold
  end success
  say "THE [[=usurer]] PAID A COPPER AND LAUGHED AT THE BARGAIN. BY THE NEW MOON HIS LEDGERS WERE ASH AND HIS DEBTORS SLEPT EASY. NOBODY ASKED WHY."
  journal "YOU SOLD [[=gift]] TO THE [[=usurer]] OF {HOME}. {KIN} IS WELL. THE [[=usurer]] IS NOT."
  do take "[[=gift]]"
  do reward small
  do mark sold_a_curse
  do remember kin "THEY SAY THE [[=usurer]] TALKS TO SOMETHING AT NIGHT. I KNOW WHAT. I TRY TO PITY HIM."
  do fact "THE [[=usurer]] OF {HOME} BOUGHT A PRETTY THING CHEAP AND HAS NOT SLEPT SINCE."
)SAGA";

const char* const kFairBargain = R"SAGA(
title [[WHAT GREETS YOU AT THE GATE|THE FAIR BARGAIN|SEVEN FULL BARNS]]
hook npc any
pitch "[[pitch=WHY IS YOUR GATE TIED SHUT?|YOU LOOK LIKE YOU'RE COUNTING DAYS.|THAT'S A GOOD BARN. A FULL ONE.]]"
hint "[[~pitch|THE LAST HARVEST IS IN. THE LAST ONE. YOU DON'T KNOW WHAT THAT MEANS. GOOD.|THE LAST HARVEST IS IN. THE LAST ONE. YOU DON'T KNOW WHAT THAT MEANS. GOOD.|EVERY BARN IN THE VALLEY BURST THIS YEAR. MINE MOST OF ALL. I CAN'T BEAR TO LOOK AT IT.]]"
role giver giver
role home site home
role kin resident kin of giver
role hill site cave near
role tall person [[male|female]] at hill
var loophole 0
var o_goat 0
var o_sub 0
var o_iron 0
var o_gone 0
slot t1 -> homeward deceit price rival wonder
slot t2 -> reckoning mercy return wonder

stage start
  talk giver
  say "[[years=SEVEN|NINE]] YEARS AGO, IN THE BLIGHT YEAR, A TALL STRANGER AT {HILL} OFFERED ME FULL BARNS FOR THE FIRST THING THAT GREETED ME AT MY GATE THAT NIGHT. I THOUGHT OF THE DOG. THE DOG WAS ASLEEP. {KIN} RAN OUT. <<grief:fear>>"
  opt "I'LL GO TO THE HILL." -> seek
  opt "WHAT WERE THE EXACT WORDS?" -> words
  opt "A BARGAIN IS A BARGAIN." -> refused

stage words
  talk giver
  say "'THE FIRST THING THAT GREETS YOU AT YOUR GATE.' AND {KIN} DIDN'T GREET ME, NOT REALLY. {KIN.HE} RAN OUT SHOUTING THAT THE GOAT HAD EATEN THE WASHING. <<doubt>>"
  do set loophole 1
  opt "THAT MAY BE ENOUGH." -> seek
  opt "WORDS WON'T SAVE {KIN.HIM}." -> refused

stage refused
  end fail
  say "<<refusal:grief>> THEN I'LL STAND AT THE GATE MYSELF TONIGHT, AND SEE WHO THEY TAKE."
  journal "YOU WOULD NOT GO TO {HILL} FOR {GIVER} AND {KIN}."
  do remember giver "THEY CAME AT MOONRISE. I STOOD AT THE GATE. THEY LAUGHED AT ME AND WENT AWAY. THEY'LL BE BACK."

stage seek
  goal goto hill
  then court
  say "THEY COME AT MOONRISE, ON THE LAST NIGHT OF THE HARVEST. <<urgency>>"
  journal "THE FAIR FOLK OF {HILL} COME TO COLLECT {GIVER}'S BARGAIN: {KIN}. SPEAK FOR {KIN.HIM} BEFORE MOONRISE."

stage court
  talk tall
  say "THE ONE WHO OWES SENDS A STRANGER. HOW VERY MORTAL. [[=years]] FULL BARNS, AND NOT ONE THANK YOU. WE COLLECT AT MOONRISE: THE FIRST THING THAT GREETED {GIVER} AT THE GATE. FAIR IS FAIR."
  opt "{KIN} NEVER GREETED {GIVER.HIM}." if var loophole 1 -> letter
  opt "TAKE SOMETHING OF MINE INSTEAD." -> substitute
  opt "I CARRY COLD IRON." -> iron
  opt "FAIR IS FAIR. TAKE {KIN.HIM}." -> gone

stage letter
  talk tall
  say "SHOUTING ABOUT A GOAT. OH, THAT IS DELICIOUS. A GREETING IS A GREETING, BUT A COMPLAINT IS NOT. WE ARE BOUND BY WORDS AS YOU ARE BOUND BY BONES. VERY WELL. WHAT DID GREET {GIVER.HIM}? THE GOAT, I THINK."
  do set o_goat 1
  opt "THE GOAT IT IS." -> @t1

stage substitute
  talk tall
  say "FROM YOU? WHAT HAVE YOU THAT WE WANT... [[YOUR LAUGH|YOUR NEXT SUMMER|YOUR SLEEP ON MOONLIT NIGHTS]]. NOT FOREVER. WE ARE NOT CRUEL. ONLY UNTIL SOMEONE CHARMS IT BACK."
  opt "TAKE IT." -> sub_paid
  opt "NOT THAT. NEVER THAT." -> iron

stage sub_paid
  talk tall
  say "DONE, AND WRITTEN ON THE WIND. HOW STRANGE YOU ARE, TO PAY ANOTHER'S DEBT. WE WILL TALK ABOUT YOU FOR A HUNDRED YEARS."
  do set o_sub 1
  opt "GO WELL, THEN." -> @t1

stage iron
  talk tall
  say "IRON. HOW TEDIOUS. KEEP YOUR MORTAL, THEN. AND KEEP YOUR BARNS AS THEY WERE BEFORE US: EMPTY. THE BLIGHT WAS ONLY SLEEPING."
  do set o_iron 1
  opt "LET IT COME." -> @t1

stage gone
  talk tall
  say "WISE. {KIN} WILL DANCE, AND NEVER AGE, AND NEVER COME HOME. TELL {GIVER} IT WAS PAINLESS. IT WILL EVEN BE TRUE."
  do set o_gone 1
  do moves kin hill
  opt "...TELL {GIVER.HIM} YOURSELF." -> @t1

stage homeward
  goal goto home
  then @t2
  journal "MOONRISE HAS COME AND GONE AT {HILL}. GO BACK TO {GIVER} IN {HOME}."

stage reckoning
  talk giver
  say "YOU'RE BACK. I SAT UP ALL NIGHT WITH THE GATE TIED SHUT AND {KIN} ASLEEP ACROSS MY KNEES LIKE A CHILD. TELL ME. WHAT DID IT COST?"
  opt "THEY TOOK THE GOAT." if var o_goat 1 -> goat_end
  opt "THEY TOOK SOMETHING OF MINE." if var o_sub 1 -> sub_end
  opt "NOTHING. YOUR BARNS WILL EMPTY." if var o_iron 1 -> iron_end
  opt "{KIN} IS UNDER THE HILL." if var o_gone 1 -> gone_end

stage goat_end
  end success
  say "THE GOAT WAS GONE IN THE MORNING, AND THE WASHING WITH IT. {GIVER} LAUGHED UNTIL {GIVER.HE} CRIED, AND THEN JUST CRIED."
  journal "THE FAIR FOLK OF {HILL} TOOK {GIVER}'S GOAT, BY THE LETTER OF THE BARGAIN. {KIN} STAYS."
  do reward fair
  do befriend kin
  do remember giver "I BOUGHT A NEW GOAT. I DON'T LET IT NEAR THE GATE. <<proverb>>"
  do fact "IN {HOME} THEY SAY THE FAIR FOLK OF {HILL} ONCE TOOK A GOAT FOR A CHILD, AND WERE GLAD OF THE JOKE."

stage sub_end
  end success
  say "{GIVER} WOULD NOT LET GO OF YOUR HAND. SOMETHING OF YOURS IS UNDER THE HILL NOW, DANCING. SOME NIGHTS YOU ALMOST HEAR IT."
  journal "YOU PAID {GIVER}'S DEBT TO THE FAIR FOLK OF {HILL} WITH SOMETHING OF YOUR OWN. {KIN} STAYS."
  do reward rich
  do mark fae_debt
  do fame 1
  do befriend kin
  do remember giver "THE ONE WHO PAID MY DEBT. <<blessing>> SIT. EAT. YOU NEVER PAY FOR ANYTHING HERE."

stage iron_end
  end success
  say "{KIN} STAYED. THE BARNS SAGGED BY SPRING, AND THE BLIGHT CAME BACK INTO THE BARLEY. {GIVER} SAYS IT IS CHEAP AT THE PRICE. {GIVER.HE} SAYS IT EVERY DAY."
  journal "YOU DROVE OFF THE FAIR FOLK OF {HILL} WITH IRON. {KIN} STAYS. THE GOOD YEARS ARE OVER."
  do reward small
  do mark hill_insulted
  do remember giver "LEAN AGAIN. I DON'T CARE. I HAVE {KIN}. <<oath>>"
  do fact "THE FAIR FOLK OF {HILL} WERE TURNED AWAY WITH IRON, AND THE FIELDS OF {HOME} ARE LEAN AGAIN."

stage gone_end
  end fail
  say "{GIVER} SAID NOTHING. {GIVER.HE} UNTIED THE GATE, AND LEFT IT OPEN, AND WENT INSIDE."
  journal "YOU LET THE FAIR FOLK TAKE {KIN} UNDER {HILL}. THE BARNS OF {HOME} ARE FULL."
  do remember giver "THE BARNS ARE FULL. TAKE WHAT YOU LIKE. I DON'T WANT ANY OF IT."
  do fact "{KIN} OF {HOME} DANCES UNDER {HILL} AND WILL NOT AGE. ON STILL NIGHTS THE MUSIC CARRIES."
)SAGA";

const char* const kChangeling = R"SAGA(
title [[THE CHILD WHO WAS NOT|ALE IN AN EGGSHELL|THE OLD EYES IN THE CRADLE]]
hook npc any
pitch "[[pitch=IS THE BABY ALL RIGHT?|YOU LOOK LIKE YOU HAVEN'T SLEPT.|WHY IS THERE IRON OVER THE CRADLE?]]"
hint "[[~pitch|THE CHILD DOESN'T CRY. EVER. IT JUST WATCHES ME. LIKE IT'S WAITING FOR ME TO GET SOMETHING WRONG.|THE CHILD DOESN'T CRY. EVER. IT JUST WATCHES ME. LIKE IT'S WAITING FOR ME TO GET SOMETHING WRONG.|THE OLD WIVES HAVE A WORD FOR A CHILD LIKE MINE. I WON'T SAY IT.]]"
role giver giver
role home site home
role gossip resident friend of giver
role hill site cave near
role nurse person female at hill
var proven 0
slot t1 -> seek wonder identity deceit
slot t2 -> mercy_wait mercy price return

stage start
  talk giver
  say "[[age=EIGHT MONTHS|ELEVEN MONTHS|A YEAR]] OLD, AND IT EATS FOR THREE AND NEVER GROWS. IT NEVER CRIES. IT WATCHES ME LIKE A [[GRANDFATHER|MAGISTRATE|CROW ON A GATE]]. MY CHILD WAS NOT LIKE THIS. <<plea:fear>>"
  opt "LET ME SEE THE CHILD." -> cradle
  opt "WHAT DO THE OLD WIVES SAY?" -> hearth
  opt "ALL BABIES ARE STRANGE." -> refused

stage hearth
  talk gossip
  say "LAY IT ON THE HOT HEARTHSTONE. IF IT'S {GIVER}'S, IT SCREAMS AND YOU SNATCH IT BACK. IF IT ISN'T, IT GOES UP THE CHIMNEY LAUGHING. MY GRANDMOTHER SAW IT DONE. <<proverb>>"
  opt "NOBODY BURNS A CHILD." -> cradle
  opt "THEN TELL {GIVER} TO DO IT." -> burned

stage burned
  end fail
  say "{GIVER} HELD THE CHILD OVER THE HEARTH A LONG MOMENT, AND THEN SAT DOWN ON THE FLOOR WITH IT AND WEPT. NEITHER OF THEM SCREAMED."
  journal "YOU TOLD {GIVER} TO PUT A CHILD ON THE HEARTH. {GIVER.HE} COULD NOT DO IT, AND WILL NOT FORGIVE YOU FOR ASKING."
  do remember giver "YOU. YOU SAID BURN IT. GET OUT OF MY SIGHT."
  do fact "THEY SAY A STRANGER IN {HOME} ONCE TOLD A [[=age]] CHILD'S FAMILY TO BURN IT. THE FAMILY WOULDN'T."

stage refused
  end fail
  say "<<doubt>> MAYBE. MAYBE I'M MAD. I'D RATHER BE MAD."
  journal "YOU TOLD {GIVER} THAT ALL BABIES ARE STRANGE."
  do remember giver "IT STILL WATCHES ME. IT SMILED YESTERDAY. I WISH IT HADN'T."

stage cradle
  talk giver
  say "LOOK AT IT. NO, LOOK. IT KNOWS WE'RE TALKING ABOUT IT. [[IT SMELLS OF MOSS AND COLD STONE.|ITS HANDS ARE OLD. A CHILD'S HANDS AREN'T OLD.|IT HUMS WHEN IT THINKS I'M ASLEEP.]] TELL ME I'M WRONG."
  opt "BREW ALE IN AN EGGSHELL BEFORE IT." -> eggshell
  opt "MAYBE IT'S ONLY SICKLY." -> sickly

stage sickly
  talk giver
  say "SICKLY. THE HERB-WIFE SAID SO TOO, AND GAVE ME A TEA THAT DIDN'T HELP. <<doubt>> AND IF IT IS? WHAT THEN?"
  opt "THEN TRY THE EGGSHELL. AND KNOW." -> eggshell
  opt "THEN LOVE IT AS IT IS." -> loved_end

stage loved_end
  end success
  say "{GIVER} TOOK THE CHILD UP, AND FOR THE FIRST TIME IN MONTHS IT SLEPT IN SOMEONE'S ARMS. WHAT IT WAS, NOBODY EVER LEARNED. IT GREW SLOWLY, AND HUMMED, AND WAS LOVED."
  journal "YOU TOLD {GIVER} TO LOVE THE CHILD AS IT IS. NOBODY IN {HOME} KNOWS WHAT IT IS. IT IS LOVED."
  do reward small
  do remember giver "IT SAID ITS FIRST WORD TODAY. IT WAS AN OLD WORD. NOBODY KNOWS WHAT IT MEANS. I DON'T CARE."
  do fact "THERE IS A CHILD IN {HOME} WHO HUMS OLD TUNES AND NEVER CATCHES COLD. ITS FAMILY ADORES IT."

stage eggshell
  talk giver
  say "I DID IT. HALF AN EGGSHELL, A DROP OF MALT. IT SAT UP CLEAR AS YOU AND SAID: I HAVE SEEN THE ACORN BEFORE THE OAK, BUT NEVER ALE IN AN EGGSHELL. THEN IT LAUGHED. <<omen>>"
  do set proven 1
  opt "THEN YOUR CHILD IS UNDER {HILL}." -> @t1

stage seek
  goal goto hill
  then nurse_talk
  say "BRING MY BABY HOME. <<vow>>"
  journal "{GIVER}'S CHILD WAS TAKEN UNDER {HILL} AND SOMETHING OLD LEFT IN ITS CRADLE. GO TO THE HILL."

stage nurse_talk
  talk nurse
  say "YOU'VE COME FOR THE MORTAL BABY. IT IS FAT AND LOUD AND WE ARE ALL FOND OF IT. AND OURS? OUR OLD ONE WANTED TO DIE IN A WARM HOUSE, LOVED, THE WAY YOUR KIND DIE. IS THAT SO MUCH TO ASK?"
  opt "SWAP THEM BACK. NOW." -> swap_end
  opt "LET YOURS DIE THERE. THEN SWAP." -> @t2
  opt "GIVE IT BACK OR I TAKE IT." check level 4 -> forced_end else thrown_out

stage mercy_wait
  goal wait 3
  then mercy_end
  journal "THE OLD ONE OF {HILL} IS DYING IN {GIVER}'S CRADLE, WARM AND LOVED. WHEN IT IS OVER, THE HILL WILL GIVE BACK THE BABY."

stage swap_end
  end success
  say "AT DUSK THE CRADLE HELD A FAT, FURIOUS, ORDINARY BABY, AND {GIVER} HAS NOT STOPPED SMILING SINCE. THE HILL WAS QUIET THAT NIGHT. QUIETER THAN USUAL."
  journal "THE FAIR FOLK OF {HILL} GAVE BACK {GIVER}'S CHILD."
  do reward fair
  do remember giver "LISTEN TO IT SCREAM! ISN'T IT WONDERFUL? <<thanks>>"
  do fact "A CHILD OF {HOME} WAS TAKEN UNDER {HILL} AND GIVEN BACK, THEY SAY. IT STILL CRIES AT THE FULL MOON."

stage mercy_end
  end success
  say "ON THE THIRD NIGHT THE OLD ONE STOPPED HUMMING. {GIVER} BURIED IT UNDER THE APPLE TREE. AT DAWN THERE WAS A BABY ON THE DOORSTEP, WRAPPED IN MOSS, ASLEEP."
  journal "{GIVER} LET THE OLD ONE OF {HILL} DIE LOVED, AND THE HILL GAVE BACK THE BABY WITH A GIFT IN ITS FIST."
  do reward rich
  do remember giver "THE APPLE TREE FLOWERS TWICE A YEAR NOW. NOBODY ASKS WHY. <<saying>>"
  do fact "THERE IS AN APPLE TREE IN {HOME} THAT FLOWERS TWICE A YEAR, OVER AN OLD THING THAT WAS LOVED."
  do mark hill_friend

stage forced_end
  end success
  say "THE NURSE SPAT AND GAVE YOU THE BABY. BY THE TIME YOU REACHED {HOME} THE CRADLE WAS EMPTY OF ANYTHING OLD. THE HILL WILL REMEMBER YOUR FACE."
  journal "YOU TOOK {GIVER}'S CHILD BACK FROM {HILL} BY FORCE. THE HILL REMEMBERS."
  do reward small
  do mark hill_insulted
  do remember giver "MY BABY IS HOME. THE DOG WON'T GO NEAR THE HILL ROAD NOW. NEITHER WILL I."

stage thrown_out
  end fail
  say "YOU WOKE ON THE HILLSIDE WITH DEW IN YOUR MOUTH. THE HILL WAS ONLY A HILL. IN {HOME}, THE CRADLE STILL WATCHES."
  journal "THE FAIR FOLK OF {HILL} THREW YOU OUT. {GIVER}'S CHILD IS STILL UNDER THE HILL."
  do remember giver "YOU CAME BACK WITHOUT IT. <<grief>>"
)SAGA";

const char* const kTrueName = R"SAGA(
title [[THE NAME THAT BINDS|THE GREY SPINNER|WHAT IS MY NAME?]]
hook npc any
pitch "[[pitch=WHAT ARE YOU MUTTERING?|WHY ARE YOU WRITING NAMES?|THAT'S FINE THREAD.]]"
hint "[[~pitch|TOBIN. TAVISH. TOMAS. TOLLY. NO. NO. HOW MANY NAMES ARE THERE IN THE WORLD?|TOBIN. TAVISH. TOMAS. TOLLY. NO. NO. HOW MANY NAMES ARE THERE IN THE WORLD?|SILVER THREAD, AND I CAN'T SPIN WOOL WITHOUT KNOTTING IT. ASK ME HOW.]]"
role giver giver
role home site home
role hill site cave near
role spinner person male at hill
var kind 0
slot t1 -> back deceit rival wonder price
slot t2 -> named mercy identity

stage start
  talk giver
  say "I TOLD THE REEVE I COULD SPIN A SACK OF FLAX INTO SILVER THREAD. I WAS DRUNK AND PROUD. A LITTLE GREY MAN SPUN IT FOR ME, THREE NIGHTS, AND AT THE NEW MOON HE COMES FOR HIS PRICE: [[price=MY FIRSTBORN|MY VOICE|MY NAME]]. UNLESS I GUESS HIS. <<plea>>"
  opt "I'LL LEARN HIS NAME." -> seek
  opt "WHERE DID HE COME FROM?" -> where
  opt "YOU MADE THE BARGAIN." -> refused

stage where
  talk giver
  say "OUT OF THE CHIMNEY SMOKE. AND WHEN HE LEFT, HE WENT UP {HILL.DIR}, TOWARD {HILL}, HOPPING ON ONE FOOT. <<omen>>"
  opt "THEN THAT'S WHERE I'LL LISTEN." -> seek

stage refused
  end fail
  say "<<grief:fear>> THEN I'LL GUESS. I HAVE A LIST. IT'S VERY LONG."
  journal "YOU LEFT {GIVER} OF {HOME} TO GUESS A NAME ALONE."
  do remember giver "I GUESSED THREE HUNDRED NAMES. HE LAUGHED AT EVERY ONE. HE TOOK [[=price]]."

stage seek
  goal goto hill
  then fire
  journal "A LITTLE GREY SPINNER WANTS [[=price]] FROM {GIVER} OF {HOME} UNLESS {GIVER.HE} GUESSES HIS NAME. LISTEN AT {HILL}."

stage fire
  talk spinner
  say "HE DOESN'T SEE YOU. HE DANCES ROUND A FIRE OF [[THORN|BRACKEN|OLD BONES]] AND SINGS: NOBODY IN {HOME} SHALL EVER KNOW, THAT [[name=GRISTLEWICK|TOMMELKIN|HOBBIN GREYCOAT|NIDDERKNAP]] IS MY NAME, HO! THEN HE STOPS, AND SNIFFS."
  opt "STEP INTO THE FIRELIGHT." -> face
  opt "SLIP AWAY WITH THE NAME." -> @t1

stage face
  talk spinner
  say "YOU HEARD. OF COURSE YOU HEARD. EVERYONE HEARS, IN THE END; THAT IS HOW THE STORY GOES. DO YOU KNOW WHY I SING IT? NOBODY HAS SAID MY NAME KINDLY IN THREE HUNDRED YEARS."
  opt "THEN I'LL SAY IT KINDLY." -> kindly
  opt "I'LL TELL {GIVER} EVERYTHING." -> @t1
  opt "SERVE ME, [[=name]]." -> bound_end

stage kindly
  talk spinner
  say "...[[=name]]. AGAIN? OH. OH, THAT IS BETTER THAN ANY [[=price]]. TELL YOUR SPINNER THE DEBT IS PAID. AND TELL THEM MY NAME, SO SOMEONE ELSE KNOWS IT."
  do set kind 1
  opt "I WILL." -> @t1

stage bound_end
  end success
  say "HE WENT STILL AS A STRUCK BELL. YES, HE SAID, IN A VERY SMALL VOICE. AND THE DEBT IS GONE, AND {GIVER} IS FREE, AND SOMETHING SMALL AND GREY NOW FOLLOWS YOU AT A DISTANCE."
  journal "YOU BOUND THE GREY SPINNER OF {HILL} BY HIS NAME. {GIVER}'S DEBT IS GONE. HE FOLLOWS YOU NOW."
  do reward fair
  do mark bound_a_name
  do remember giver "FREE? JUST LIKE THAT? WHAT DID YOU DO TO HIM? ...NO. DON'T TELL ME. <<relief>>"
  do fact "SOMEONE BOUND A LITTLE GREY SPINNER BY HIS NAME AT {HILL}. HE HAS NOT BEEN SEEN TO DANCE SINCE."

stage back
  goal goto home
  then reveal
  journal "YOU KNOW THE GREY SPINNER'S NAME. GET BACK TO {GIVER} IN {HOME} BEFORE THE NEW MOON."

stage reveal
  talk giver
  say "THE NEW MOON IS TONIGHT. HE'S COMING DOWN THE CHIMNEY, I CAN HEAR HIM HUMMING. DO YOU... TELL ME YOU HAVE IT."
  opt "HE FORGAVE THE DEBT." if var kind 1 -> kind_end
  opt "HIS NAME IS [[=name]]." -> @t2

stage named
  talk giver
  say "[[=name]]. I COULD SHOUT IT IN HIS FACE AND WATCH HIM TEAR HIMSELF IN TWO. HE'D HAVE TAKEN [[=price]]. OR I COULD SAY IT LIKE A NEIGHBOUR. WHICH?"
  opt "SHOUT IT. HE EARNED IT." -> shout_end
  opt "SAY IT GENTLY." -> gentle_end

stage kind_end
  end success
  say "THAT NIGHT A SMALL GREY SHAPE SAT ON THE CHIMNEY POT AND SANG A SONG WITH {GIVER}'S NAME IN IT. IT WAS A KIND SONG."
  journal "THE GREY SPINNER FORGAVE {GIVER}'S DEBT FOR THE SOUND OF HIS NAME SAID KINDLY."
  do reward fair
  do remember giver "THERE'S A SKEIN OF SILVER THREAD ON MY STEP EVERY NEW MOON. I SAY HIS NAME WHEN I PICK IT UP."
  do fact "A LITTLE GREY SPINNER LEAVES SILVER THREAD ON A DOORSTEP IN {HOME}. THE HOUSE SAYS HIS NAME KINDLY."

stage shout_end
  end success
  say "{GIVER} SHOUTED IT. THE GREY MAN STAMPED SO HARD HIS FOOT WENT THROUGH THE FLOOR, AND HE TORE HIMSELF FREE OF IT, AND WAS GONE, AND THE HOUSE STANK OF WET ASH FOR A WEEK."
  journal "{GIVER} SHOUTED THE GREY SPINNER'S NAME AND HE TORE HIMSELF AWAY. THE DEBT IS BROKEN."
  do reward fair
  do remember giver "THERE'S A HOLE IN MY FLOOR THE SHAPE OF A FOOT. I PUT A RUG ON IT. <<relief>>"
  do fact "IN {HOME} THERE IS A HOLE IN A FLOOR THE SHAPE OF A SMALL FOOT, WHERE A BARGAIN BROKE."

stage gentle_end
  end success
  say "{GIVER} SAID IT AS YOU'D GREET A NEIGHBOUR OVER A FENCE. THE GREY MAN BLINKED, AND SAT DOWN BY THE FIRE, AND CRIED, AND STAYED FOR SUPPER."
  journal "{GIVER} SAID THE GREY SPINNER'S NAME GENTLY. HE SAT DOWN TO SUPPER INSTEAD OF COLLECTING."
  do reward rich
  do remember giver "HE COMES TO SUPPER ON THE NEW MOON. HE EATS NOTHING AND TALKS ALL NIGHT. I'VE GROWN FOND OF HIM."
  do fact "A LITTLE GREY MAN EATS NOTHING AT A SUPPER TABLE IN {HOME} EVERY NEW MOON, AND IS CALLED BY HIS NAME."
)SAGA";

const char* const kThreeTasks = R"SAGA(
title [[THE THREE TASKS|A HARD FATHER'S TERMS|THE SUITOR'S TRIALS]]
hook npc any
pitch "[[pitch=WHY THE LONG FACE?|YOU LOOK LIKE A LOVESICK CALF.|WHO ARE YOU WRITING THAT POEM FOR?]]"
hint "[[~pitch|THREE TASKS. HE SETS EVERY SUITOR THE SAME THREE TASKS. TWO BROKE THEIR NECKS AND ONE MARRIED A GOAT-HERD IN DESPAIR.|LOVE IS EASY. FATHERS ARE NOT.|LOVE IS EASY. FATHERS ARE NOT.]]"
role giver giver
role home site home
role beloved resident any
role elder person male at home
role den site cave near
slot t1 -> riddle rival price world
slot t2 -> third deceit rival wonder

stage start
  talk giver
  say "I'D MARRY {BELOVED}, AND {BELOVED} WOULD MARRY ME. BUT {BELOVED.HIS} FATHER {ELDER} SETS EVERY SUITOR THREE TASKS, AND NOBODY HAS PASSED THEM. I'M NO HERO. <<plea:love>>"
  opt "WHAT ARE THE TASKS?" -> tasks
  opt "DOES {BELOVED} WANT THIS?" -> want
  opt "FIND ANOTHER SWEETHEART." -> refused

stage want
  talk beloved
  say "{GIVER}? WE'VE WALKED OUT TOGETHER SINCE THE HARVEST DANCE. MY FATHER WILL NEVER... BUT YES. TELL {GIVER.HIM} YES. AND TELL {GIVER.HIM} TO HURRY, BEFORE MY FATHER SETS A FOURTH."
  opt "THEN WHAT ARE THE TASKS?" -> tasks

stage refused
  end fail
  say "<<refusal:love>> NO. I'D SOONER BREAK MY NECK."
  journal "YOU WOULD NOT HELP {GIVER} WIN {BELOVED}."
  do remember giver "STILL THREE TASKS. STILL NO HERO. I'M LEARNING TO CLIMB, THOUGH."

stage tasks
  talk giver
  say "FIRST, CLEAR THE [[beasts=WOLVES|BOARS|WILD DOGS]] OUT OF HIS HIGH PASTURE BY {DEN}. SECOND, BRING HIM WATER THAT NEVER TOUCHED THE GROUND. THIRD, TELL HIM A TRUTH HE HAS NEVER HEARD. THE THIRD FRIGHTENS ME MOST."
  opt "I'LL START WITH THE PASTURE." -> pasture

stage pasture
  goal kill 3 beast in den
  then @t1
  journal "THE FIRST TASK: CLEAR THE [[=beasts]] FROM {ELDER}'S HIGH PASTURE AT {DEN}."

stage riddle
  talk elder
  say "THE [[=beasts]] ARE GONE? HM. YOU'RE NOT THE SUITOR, BUT I SAID NOTHING ABOUT HELPERS. SECOND TASK: WATER THAT NEVER TOUCHED THE GROUND. WELL?"
  opt "RAIN, CAUGHT IN A CUP." -> @t2
  opt "WATER FROM YOUR OWN WELL." -> wrong
  opt "SILVER NEVER TOUCHES GROUND. HERE." check gold 15 -> bribed else wrong

stage wrong
  talk elder
  say "WELL WATER HAS TOUCHED THE BOTTOM OF THE WELL. ONE MORE TRY, AND THEN I LET THE DOGS DECIDE. <<insult>>"
  opt "DEW, THEN. FROM THE LEAVES." -> @t2

stage bribed
  talk elder
  say "COIN. HM. I'LL ALLOW IT, SINCE MOST WATER COMES TO ME THROUGH A TAX MAN ANYWAY. I'LL THINK LESS OF YOU BOTH."
  do gold -15
  opt "THINK WHAT YOU LIKE." -> @t2

stage third
  talk elder
  say "THE LAST, AND THE ONE THAT SENDS THEM HOME: TELL ME A TRUTH I HAVE NEVER HEARD. MIND, I'VE HEARD A GREAT MANY."
  opt "YOU SET THESE TASKS SO NOBODY WINS." -> lonely
  opt "{BELOVED} WILL GO, TASKS OR NO." -> leaving
  opt "YOU'RE A GOOD FATHER." -> flattery

stage flattery
  talk elder
  say "I HEAR THAT EVERY TUESDAY FROM MY OWN MOUTH. THAT'S NOT A TRUTH I'VE NEVER HEARD. <<doubt>> TRY AGAIN, OR GO."
  opt "YOU SET THESE TASKS SO NOBODY WINS." -> lonely
  opt "{BELOVED} WILL GO, TASKS OR NO." -> leaving

stage lonely
  talk elder
  say "...SINCE {BELOVED.HIS} MOTHER DIED, THIS HOUSE HAS HAD TWO VOICES IN IT. YOU WANT TO MAKE IT ONE. THAT IS TRUE, AND NOBODY HAS EVER SAID IT TO MY FACE. VERY WELL. THEY MAY WED. AND THEY EAT HERE EVERY FEAST DAY."
  opt "EVERY FEAST DAY. AGREED." -> wed_end

stage leaving
  talk elder
  say "GO? WITHOUT A WORD TO ME? ...THEN THE TASKS WERE NEVER FOR THE SUITORS. THEY WERE FOR ME. TELL {GIVER} THAT {BELOVED} IS FREE TO CHOOSE. AND THAT I HOPE {BELOVED.HE} CHOOSES TO VISIT."
  opt "I'LL TELL THEM BOTH." -> free_end

stage wed_end
  end success
  say "THEY WED AT THE NEXT NEW MOON. {ELDER} GAVE A SPEECH NOBODY COULD FOLLOW AND CRIED THROUGH ALL OF IT."
  journal "{GIVER} AND {BELOVED} ARE WED. {ELDER} WEPT, AND MADE THEM PROMISE THE FEAST DAYS."
  do reward fair
  do befriend beloved
  do remember giver "MARRIED! ME! <<thanks:love>> COME TO SUPPER. NOT A FEAST DAY, THOUGH. THOSE ARE TAKEN."
  do fact "{GIVER} OF {HOME} PASSED {ELDER}'S THREE TASKS WITH A STRANGER'S HELP, AND WED {BELOVED}."

stage free_end
  end success
  say "{BELOVED} CHOSE {GIVER}, AND CHOSE TO VISIT. {ELDER} KEEPS TWO CHAIRS BY THE FIRE AND COMPLAINS ABOUT BOTH."
  journal "{ELDER} GAVE UP HIS TASKS. {BELOVED} IS FREE TO CHOOSE, AND CHOSE {GIVER}."
  do reward fair
  do befriend beloved
  do remember elder "NO MORE TASKS. I'VE TAKEN UP BEES INSTEAD. THEY DON'T LEAVE. WELL, THEY DO, BUT THEY COME BACK."
  do fact "{ELDER} OF {HOME} SET NO MORE TASKS AFTER A STRANGER TOLD HIM THE TRUTH ABOUT THEM."
)SAGA";

const char* const kYoungest = R"SAGA(
title [[THE YOUNGEST OF THREE|THE SPRING OF THE PROUD SLEEP|BREAD FOR THE WAYFARER]]
hook npc any
pitch "[[pitch=ARE YOU UNWELL?|YOU'RE GREY AS ASH. SIT.|WHO ARE YOU WAITING FOR?]]"
hint "[[~pitch|MY HEART GOES TOO FAST, THEN TOO SLOW. THE OLD WIVES SAY THERE'S A CURE. THE OLD WIVES SAY A LOT.|MY HEART GOES TOO FAST, THEN TOO SLOW. THE OLD WIVES SAY THERE'S A CURE. THE OLD WIVES SAY A LOT.|TWO SONS WENT, AND NEITHER CAME BACK. NOW THE LAST ONE WANTS TO GO. I WANT TO TIE THE CHILD TO THE TABLE LEG.]]"
role giver giver
role home site home
role cave site cave near
role youngest person [[male|female]] at home
role eldest person male at cave
role wayfarer person [[male|female]] at cave
var kind 0
slot t1 -> home_cure rival price wonder world
slot t2 -> home_brothers betrayal deceit mercy

stage start
  talk giver
  say "[[cure=THE WATER OF THE GREY SPRING|THE BITTER MOSS|THE WHITE APPLE]] AT {CAVE} WOULD MEND MY HEART, THE OLD WIVES SAY. MY TWO ELDEST WENT FOR IT AT THE NEW MOON AND NEVER CAME BACK. NOW {YOUNGEST}, MY LAST, MEANS TO GO. GO WITH {YOUNGEST.HIM}. <<plea>>"
  opt "I'LL GO WITH {YOUNGEST.HIM}." -> road
  opt "WHAT WERE THE ELDEST LIKE?" -> elders
  opt "KEEP THE CHILD HOME." -> refused

stage elders
  talk giver
  say "[[BOLD|LOUD|HANDSOME]] AND PROUD, BOTH. THEY LAUGHED AT {YOUNGEST} FOR TALKING TO BIRDS AND GIVING AWAY BREAD. I LAUGHED TOO, GODS FORGIVE ME. <<grief:shame>>"
  opt "THEN I'LL GO WITH THE ONE WHO GIVES." -> road
  opt "KEEP THE CHILD HOME." -> refused

stage refused
  end fail
  say "<<farewell>> {YOUNGEST} WILL GO ANYWAY, YOU KNOW. TONIGHT, PROBABLY. ALONE."
  journal "YOU WOULD NOT GO WITH {YOUNGEST} TO {CAVE}."
  do remember giver "{YOUNGEST} WENT AT DAWN WITH A LOAF AND A KNIFE. I'M WAITING AGAIN. I'M VERY GOOD AT WAITING NOW."

stage road
  goal goto cave
  then wayfarer_talk
  do moves youngest cave
  journal "{GIVER}'S TWO ELDEST WENT TO {CAVE} FOR [[=cure]] AND DID NOT RETURN. GO THERE WITH {YOUNGEST}, THE LAST."

stage wayfarer_talk
  talk wayfarer
  say "BREAD FOR AN OLD ONE? TWO FINE YOUNG FELLOWS PASSED HERE AT THE NEW MOON AND GAVE ME THEIR BOOTS TO SMELL. ...YOU HAVE A KIND FACE. THE CHILD HAS A KINDER ONE."
  opt "SHARE OUR BREAD." -> shared
  opt "WE'VE NOTHING TO SPARE." -> spurned

stage shared
  talk wayfarer
  say "THEN HEAR THIS. THE PROUD ONES LIE IN THE MOSS BY THE SPRING, ASLEEP AS STONES. ONE CUP CAN BE TAKEN, AND ONE ONLY. BUT WHAT IS SHARED IS NOT TAKEN. <<blessing>>"
  do set kind 1
  opt "WE'LL REMEMBER." -> inside

stage spurned
  talk wayfarer
  say "THEN GO IN AS THEY DID. <<proverb>>"
  opt "WE WILL." -> inside

stage inside
  goal enter cave
  then spring
  journal "{GIVER}'S TWO ELDEST SONS LIE SOMEWHERE IN {CAVE}. SO DOES [[=cure]]."

stage spring
  talk youngest
  say "THERE THEY ARE, IN THE MOSS LIKE STONES, MY BROTHERS. AND [[=cure]]. THERE'S ONLY ENOUGH FOR ONE. MY {GIVER.FATHER}, OR MY BROTHERS? YOU CHOOSE. I CAN'T."
  opt "TAKE IT TO YOUR {GIVER.FATHER}." -> @t1
  opt "WAKE YOUR BROTHERS." -> brothers_wake
  opt "SHARE IT. ALL OF THEM." if var kind 1 -> shared_wake

stage home_cure
  goal goto home
  then cure_end
  journal "{YOUNGEST} CARRIES [[=cure]] HOME TO {GIVER}. THE BROTHERS SLEEP ON IN {CAVE}."

stage brothers_wake
  talk eldest
  say "WE SLEPT? HOW LONG... YOU SPENT {GIVER.FATHER}'S CURE ON US? ON US? WE CALLED YOU A FOOL FOR TALKING TO BIRDS. ...LITTLE ONE. THANK YOU."
  do moves eldest home
  opt "GO HOME. ALL OF YOU." -> brothers_end

stage shared_wake
  talk eldest
  say "A SIP EACH, AND WE WAKE, AND THE FLASK IS STILL HALF FULL? THE WAYFARER'S RIDDLE. LET ME CARRY IT HOME, LITTLE ONE. {GIVER.FATHER} WILL TAKE IT BETTER FROM {GIVER.HIS} ELDEST."
  do moves eldest home
  opt "LET HIM CARRY IT." -> @t2
  opt "NO. {YOUNGEST} CARRIES IT." -> honest_end

stage home_brothers
  goal goto home
  then claimed_end
  journal "{GIVER}'S ELDEST CARRIES THE CURE HOME AND THE CREDIT WITH IT. {YOUNGEST} WALKS BEHIND."

stage cure_end
  end success
  say "{GIVER} DRANK AND STOOD UP STRAIGHT FOR THE FIRST TIME IN A YEAR, AND THEN SAT DOWN AGAIN AND ASKED WHERE THE OTHERS WERE. {YOUNGEST} DID NOT ANSWER."
  journal "{GIVER} IS MENDED. {GIVER.HIS} TWO ELDEST STILL SLEEP IN THE MOSS OF {CAVE}."
  do reward fair
  do remember youngest "I GO TO {CAVE} EVERY MONTH AND TALK TO MY BROTHERS. THEY DON'T ANSWER. I TELL THEM ABOUT THE BIRDS."
  do fact "TWO SONS OF {HOME} SLEEP IN THE MOSS OF {CAVE}. THEIR YOUNGEST SIBLING VISITS THEM."

stage brothers_end
  end success
  say "THE BROTHERS CARRIED {YOUNGEST} HOME ON THEIR SHOULDERS. {GIVER}'S HEART STILL SKIPS. {GIVER} SAYS IT SKIPS FOR JOY NOW, AND WILL NOT BE TOLD OTHERWISE."
  journal "{YOUNGEST} SPENT THE CURE ON THE BROTHERS WHO MOCKED {YOUNGEST.HIM}. ALL THREE CAME HOME TO {HOME}."
  do reward fair
  do remember giver "ALL THREE AT ONE TABLE. MY HEART CAN DO WHAT IT LIKES NOW. <<thanks:love>>"
  do fact "THE YOUNGEST OF {GIVER}'S HOUSE GAVE AWAY A CURE TO WAKE TWO PROUD BROTHERS. THEY DON'T LAUGH AT BIRDS NOW."

stage honest_end
  end success
  say "{YOUNGEST} PUT THE FLASK IN {GIVER}'S HANDS. THE BROTHERS STOOD BEHIND, RED TO THE EARS. NOBODY SAID A WORD ABOUT BIRDS EVER AGAIN."
  journal "THE CURE AND ALL THREE CHILDREN CAME HOME TO {GIVER}. THE YOUNGEST CARRIED IT."
  do reward rich
  do remember youngest "MY BROTHERS ASK ME WHAT THE BIRDS SAY NOW. THEY EVEN LISTEN TO THE ANSWER."
  do fact "THE YOUNGEST OF {GIVER}'S HOUSE BROUGHT HOME A CURE AND TWO PROUD BROTHERS. {HOME} TELLS IT AT EVERY HARVEST."

stage claimed_end
  end success
  say "THE ELDEST KNELT AND GAVE {GIVER} THE FLASK, AND TOLD A GOOD STORY ABOUT IT. {YOUNGEST} STOOD IN THE DOORWAY AND SAID NOTHING. {GIVER} LOOKED AT THE DOORWAY A LONG TIME."
  journal "THE CURE CAME HOME TO {GIVER} IN THE ELDEST'S HANDS. EVERYONE KNOWS WHO REALLY CARRIED IT. NOBODY SAYS SO."
  do reward fair
  do remember youngest "HE TELLS IT DIFFERENTLY EVERY TIME. IN SOME OF THEM I'M NOT EVEN THERE. I DON'T MIND. MUCH."
  do mark eldest_lied
)SAGA";

const char* const kThornSleep = R"SAGA(
title [[THE SLEEPER IN THE THORNS|THE UNINVITED GUEST|A HUNDRED YEARS OR A WORD]]
hook npc any
pitch "[[pitch=WHY ARE THERE BRIARS ON YOUR DOOR?|IS SOMEONE SICK IN THERE?|YOU'RE WHISPERING. WHY?]]"
hint "[[~pitch|THE SLEEPER WON'T WAKE. NOT FOR WATER, NOT FOR SALT, NOT FOR A SLAP. AND THE BRIARS KEEP GROWING THROUGH THE SHUTTERS.|THE SLEEPER WON'T WAKE. NOT FOR WATER, NOT FOR SALT, NOT FOR A SLAP. AND THE BRIARS KEEP GROWING THROUGH THE SHUTTERS.|HUSH. NOT THAT ANY NOISE WOULD WAKE THE SLEEPER IN THERE.]]"
role giver giver
role home site home
role sleeper resident kin of giver
role hut site cave near
role aunt person female at hut
var forced 0
slot t1 -> visit mercy identity wonder
slot t2 -> wait_remedy price deceit

stage start
  talk giver
  say "AT THE NAMING FEAST FOR {SLEEPER} WE SET TWELVE PLACES, AND AN OLD WOMAN WE DID NOT ASK CAME TO THE DOOR AND SAID: ON THE DAY OF {SLEEPER.HIS} [[TWENTIETH|SIXTEENTH|EIGHTEENTH]] YEAR, A THORN. LAST WEEK, A THORN. NOW {SLEEPER.HE} SLEEPS, AND THE BRIARS GROW. <<plea>>"
  opt "WHO WAS THE OLD WOMAN?" -> who
  opt "I'LL FIND A CURE." -> remedy
  opt "LET {SLEEPER.HIM} SLEEP." -> refused

stage who
  talk giver
  say "SHE LIVES BY {HUT}. WE DID NOT ASK HER BECAUSE... <<apology:shame>> BECAUSE SHE IS MY {GIVER.FATHER}'S SISTER, AND SHE WAS MAD, AND WE WERE ASHAMED OF HER IN FRONT OF THE NEIGHBOURS. THERE. NOW YOU KNOW."
  opt "THEN I'LL GO TO HER." -> @t1
  opt "I'LL FIND A CURE INSTEAD." -> remedy

stage refused
  end fail
  journal "YOU LEFT {SLEEPER} OF {HOME} ASLEEP IN THE BRIARS."
  do remember giver "THE BRIARS ARE OVER THE ROOF NOW. THEY FLOWER. THE NEIGHBOURS SAY IT'S BEAUTIFUL."

stage visit
  goal goto hut
  then aunt_talk
  journal "AN OLD WOMAN OF {HUT}, {GIVER}'S OWN AUNT, CURSED {SLEEPER} AT A FEAST SHE WAS NEVER ASKED TO. FIND HER."

stage aunt_talk
  talk aunt
  say "TWELVE PLACES, AND NOT ONE FOR ME. I WATCHED THROUGH THE SHUTTERS AS THEY ATE MY SISTER'S BREAD. I WAS NOT MAD, STRANGER. I WAS POOR, AND I TALKED TO MYSELF BECAUSE NOBODY ELSE WOULD. <<curse>>"
  opt "COME TO {HOME}. SIT AT THE TABLE." -> welcome
  opt "LIFT IT, OR ANSWER FOR IT." -> threaten

stage welcome
  talk aunt
  say "AT THE TABLE? WITH THEM? IN FRONT OF THE NEIGHBOURS? ...YES. YES, IF THEY'LL SAY MY NAME AT GRACE. THAT'S MY PRICE. NOT GOLD. MY NAME, OUT LOUD."
  do moves aunt home
  opt "THEY'LL SAY IT." -> welcome_end

stage threaten
  talk aunt
  say "ANSWER? I HAVE ANSWERED FOR BEING ALIVE FOR SIXTY YEARS. <<threat:vengeance>> ...FINE. THE CHILD WAKES. AND THE HOUSE THAT SHUT ME OUT STAYS SHUT TO LUCK."
  do set forced 1
  opt "SO BE IT." -> forced_end

stage remedy
  goal fetch "A HIP OF THE WHITE BRIAR" in hut
  then @t2
  journal "THE OLD WIVES OF {HOME} SAY A HIP OF THE WHITE BRIAR FROM {HUT} WILL BREAK A SLEEPING CURSE, GIVEN TIME. FETCH ONE."

stage wait_remedy
  goal wait 3
  then remedy_end
  journal "THE WHITE BRIAR SIMMERS BY {SLEEPER}'S BED. IT WORKS SLOWLY, IF IT WORKS AT ALL."

stage welcome_end
  end success
  say "THEY SET A THIRTEENTH PLACE AND SAID HER NAME AT GRACE. {SLEEPER} WOKE BEFORE THE SOUP WAS COLD, YAWNED, AND ASKED WHO THE OLD LADY WAS."
  journal "{GIVER}'S AUNT CAME TO THE TABLE IN {HOME}, AND {SLEEPER} WOKE."
  do reward rich
  do befriend sleeper
  do remember giver "THIRTEEN PLACES NOW, EVERY FEAST. THE NEIGHBOURS STARE. LET THEM. <<saying>>"
  do fact "A FAMILY OF {HOME} SETS A THIRTEENTH PLACE FOR AN OLD AUNT NOW. A SLEEPING CURSE WAS BROKEN BY IT."

stage forced_end
  end success
  say "{SLEEPER} WOKE AT DUSK, SNEEZING BRIAR POLLEN. THE COW DIED THAT WEEK, AND THE WELL WENT SOUR THE NEXT. THE OLD WOMAN WAS NOT SEEN AGAIN."
  journal "YOU FORCED {GIVER}'S AUNT TO LIFT THE CURSE. {SLEEPER} IS AWAKE. THE HOUSE'S LUCK IS NOT."
  do reward small
  do mark house_unlucky
  do remember giver "{SLEEPER} IS AWAKE. THAT'S WHAT MATTERS. ...THE ROOF LEAKS NOW. IT NEVER LEAKED."

stage remedy_end
  end success
  say "ON THE THIRD MORNING {SLEEPER} OPENED {SLEEPER.HIS} EYES AND SAID: SHE WAS IN MY DREAM. AN OLD WOMAN AT A WINDOW, WATCHING US EAT. WHO IS SHE?"
  journal "THE WHITE BRIAR WOKE {SLEEPER}. {SLEEPER.HE} DREAMS OF AN OLD WOMAN AT A WINDOW, AND ASKS QUESTIONS."
  do take "A HIP OF THE WHITE BRIAR"
  do reward fair
  do befriend sleeper
  do remember sleeper "I WALKED TO {HUT} TODAY. SHE SHUT THE DOOR ON ME. I'LL GO BACK TOMORROW. AND THE DAY AFTER."
)SAGA";

const char* const kWildMan = R"SAGA(
title [[THE WILD MAN OF THE WOOD|THE THING THAT TAKES THE SHEEP|WHAT RUNS ON ALL FOURS]]
hook npc any
pitch "[[pitch=WHAT TOOK YOUR SHEEP?|YOU'RE SHARPENING THAT LIKE YOU MEAN IT.|WHAT'S OUT AT THE TREE LINE?]]"
hint "[[~pitch|SOMETHING TAKES A SHEEP A WEEK. IT LEAVES THE FLEECE FOLDED. FOLDED. WHAT ANIMAL FOLDS A FLEECE?|THE HUNTERS WANT IT DEAD. I WANT TO KNOW WHAT IT IS FIRST.|SOMETHING TAKES A SHEEP A WEEK. IT LEAVES THE FLEECE FOLDED. FOLDED. WHAT ANIMAL FOLDS A FLEECE?]]"
role giver giver
role home site home
role den site cave near
role wild foe male at den
role wise person female at home
var token 0
slot t1 -> den_go identity mercy wonder deceit
slot t2 -> after return price world

stage start
  talk giver
  say "A YEAR AGO MY [[rel=HUSBAND|BROTHER|ELDEST]], {WILD}, WENT TO {DEN} FOR [[A STRAY EWE|FIREWOOD|A BET]] AND NEVER CAME BACK. THAT SPRING THE WILD THING CAME, AND TAKES A SHEEP A WEEK. THE HUNTERS WANT IT DEAD. <<doubt>>"
  opt "THEN I'LL HUNT IT." -> hunt
  opt "WHO ELSE KNOWS ABOUT THIS?" -> wise_talk
  opt "A WOLF IS A WOLF." -> refused

stage refused
  end fail
  say "<<grief>> THE HUNTERS WILL GET IT SOONER OR LATER. I SUPPOSE I DON'T WANT TO KNOW."
  journal "YOU TOLD {GIVER} THAT A WOLF IS A WOLF."
  do remember giver "THE HUNTERS MISSED IT AGAIN. GOOD. NO. I DON'T KNOW WHAT I MEAN."

stage wise_talk
  talk wise
  say "THE WILD THING? IT'S {WILD}. HE BROKE AN OATH AT {DEN}, OVER A [[STONE|SPRING|GRAVE]] HE SHOULD HAVE LEFT ALONE, AND THE WOOD TOOK HIS WORDS AWAY. LEAVE HIM SOMETHING OF HOME AT THE DEN MOUTH, AND HE MAY REMEMBER. OR HE MAY NOT. <<proverb>>"
  opt "WHAT DID HE LOVE MOST?" -> token_ask
  opt "A CURSED BEAST IS STILL A BEAST." -> hunt

stage token_ask
  talk giver
  say "HIS [[keep=WHISTLE|FLUTE|OLD BELT KNIFE]]. HE CARVED IT HIMSELF AND PLAYED IT BADLY EVERY EVENING. HERE. TAKE IT. <<vow>>"
  do give "{WILD}'S [[=keep]]" bone
  do set token 1
  opt "I'LL LEAVE IT AT THE DEN." -> @t1

stage hunt
  goal slay wild
  then slain
  journal "THE WILD THING OF {DEN} TAKES A SHEEP A WEEK FROM {HOME}. HUNT IT DOWN."

stage den_go
  goal enter den
  then gone_wild
  do hide wild
  journal "LEAVE {WILD}'S [[=keep]] AT THE MOUTH OF {DEN}, WHERE THE WILD THING SLEEPS, AND SEE WHAT COMES OF IT."

stage gone_wild
  goal goto home
  then @t2
  do take "{WILD}'S [[=keep]]"
  journal "YOU LEFT THE [[=keep]] AT {DEN}. IN THE NIGHT, SOMEONE PLAYED IT. BADLY. GO BACK TO {GIVER}."

stage after
  talk giver
  say "HE CAME DOWN THE HILL AT DAWN ON HIS OWN TWO FEET, WITH THE [[=keep]] IN HIS FIST AND NOT A WORD IN HIM. HE'S IN THE BARN. THE VILLAGE WANTS HIM GONE BEFORE THE NEXT SHEEP."
  opt "KEEP HIM. WORDS COME BACK." -> keep_end
  opt "TAKE HIM TO THE PRIESTS." -> temple_end

stage slain
  talk wise
  say "YOU KILLED IT. AND WHEN IT FELL, WHAT DID IT LOOK LIKE? ...YES. A MAN, THIN AS A RAKE. I TRIED TO TELL THEM. <<grief:shame>>"
  opt "I'LL TELL {GIVER} MYSELF." -> slain_end
  opt "LET {GIVER} THINK IT WAS A WOLF." -> lie_end

stage slain_end
  end success
  say "{GIVER} WENT UP TO {DEN} WITH A SPADE AND CAME BACK AFTER DARK. {GIVER.HE} THANKED YOU. {GIVER.HE} MEANT IT. {GIVER.HE} HAS NOT SPOKEN TO YOU SINCE."
  journal "YOU KILLED THE WILD THING OF {DEN}. IT WAS {WILD}, {GIVER}'S [[=rel]]. {GIVER} KNOWS."
  do reward fair
  do remember giver "THE SHEEP ARE SAFE. I KNOW. I KNOW. <<grief>>"
  do fact "THE WILD THING OF {DEN} WAS A MAN OF {HOME} UNDER A CURSE. IT IS BURIED THERE WITH A NAME ON ITS STONE."

stage lie_end
  end success
  say "THE HUNTERS HUNG A WOLF PELT ON THE INN DOOR, AND {GIVER} BOUGHT EVERYONE A DRINK. YOU DRANK ONE TOO. IT TASTED OF NOTHING."
  journal "YOU KILLED THE WILD THING OF {DEN} AND LET {GIVER} BELIEVE IT WAS A WOLF."
  do reward fair
  do mark kept_a_grave_secret
  do remember wise "SHE STILL WAITS FOR HIM TO COME HOME. YOU LET HER. I LET HER. WE'LL BOTH ANSWER FOR IT."

stage keep_end
  end success
  say "IT TOOK A WINTER. HE SAID HIS FIRST WORD AT MIDSUMMER, AND IT WAS {GIVER}'S NAME, AND THE WHOLE VILLAGE PRETENDED NOT TO CRY."
  journal "{WILD} CAME HOME FROM {DEN}. {GIVER} KEPT HIM, AND HIS WORDS ARE COMING BACK ONE BY ONE."
  do reward rich
  do remember giver "HE PLAYS THE [[=keep]] AGAIN. JUST AS BADLY. I'VE NEVER HEARD ANYTHING SO BEAUTIFUL. <<thanks>>"
  do fact "{WILD} OF {HOME} WAS A WILD THING OF THE WOOD FOR A YEAR, AND CAME HOME. THEY SAY HE STILL WON'T EAT MUTTON."

stage temple_end
  end success
  say "THE PRIESTS TOOK HIM IN. {GIVER} VISITS ON FEAST DAYS. HE KNOWS {GIVER.HIM} NOW, MOSTLY. THE SHEEP OF {HOME} ARE SAFE."
  journal "{WILD} CAME BACK FROM {DEN} AND LIVES WITH THE PRIESTS NOW, LEARNING TO SPEAK."
  do reward fair
  do remember giver "HE SAID MY NAME LAST FEAST DAY. THEN HE SAID IT TO THE GOAT. IT'S A START."
  do fact "A MAN WHO WAS A WILD THING OF {DEN} LIVES WITH THE PRIESTS NEAR {HOME}, LEARNING HIS WORDS AGAIN."
)SAGA";

const char* const kPiper = R"SAGA(
title [[THE PIPER UNPAID|THE TUNE THE CHILDREN HUM|THE RAT-CATCHER'S PURSE]]
hook npc any
pitch "[[pitch=WHAT'S THAT TUNE THE CHILDREN HUM?|WHY ARE THE DOORS ROPED SHUT?|NO RATS HERE. LUCKY.]]"
hint "[[~pitch|THE CHILDREN WALK IN THEIR SLEEP NOW. ALL OF THEM. ALL TOWARD THE SAME HILL.|THE CHILDREN WALK IN THEIR SLEEP NOW. ALL OF THEM. ALL TOWARD THE SAME HILL.|LUCKY? WE PAID FOR THAT LUCK. OR RATHER, WE DIDN'T. THAT'S THE TROUBLE.]]"
role giver giver
role home site home
role reeve person male at home
role hill site cave near
role piper person [[male|female]] at hill
slot t1 -> seek rival deceit world
slot t2 -> final mercy price betrayal

stage start
  talk giver
  say "RATS WERE IN THE GRAIN AND THE CRADLES. A PIPER IN A [[RED|PATCHED|GREEN]] COAT PIPED THEM INTO THE RIVER FOR [[purse=FORTY SILVER|A PURSE OF SILVER|A SACK OF SILVER]], AND REEVE {REEVE} PAID HIM IN LAUGHTER. NOW THE CHILDREN HUM HIS TUNE IN THEIR SLEEP AND WALK TOWARD {HILL}. <<urgency>>"
  opt "THE REEVE MUST PAY." -> reeve_talk
  opt "I'LL FIND THE PIPER." -> @t1
  opt "A DEAL'S BETWEEN THE REEVE AND HIM." -> refused

stage refused
  end fail
  say "<<curse>> TELL THAT TO MY CHILD WHEN {GIVER.HE} WALKS OUT OF THE DOOR TONIGHT."
  journal "YOU LEFT {HOME} TO ITS PIPER AND ITS REEVE."
  do remember giver "WE TIE THE CHILDREN TO THEIR BEDS NOW. THEY HUM THROUGH THE ROPE."

stage reeve_talk
  talk reeve
  say "PAY? THE RATS WOULD HAVE LEFT ON THEIR OWN BY SPRING. AND THAT PURSE IS THE BRIDGE FUND. THE BRIDGE, STRANGER. <<boast:pride>>"
  opt "PAY HIM, OR {HOME} HEARS WHY NOT." check fame 1 -> reeve_pays else reeve_spits
  opt "THEN I'LL DEAL WITH THE PIPER." -> @t1

stage reeve_pays
  talk reeve
  say "...YOU'D TELL THEM? YES. YOU WOULD. VERY WELL. [[=purse]], AND NOT A COIN MORE, AND I WANT IT KNOWN I PAID IT GLADLY. I DIDN'T."
  do give "THE PIPER'S PURSE" gold
  opt "I'LL CARRY IT." -> @t1

stage reeve_spits
  talk reeve
  say "AND WHO ARE YOU TO {HOME}? A STRANGER WITH MUD ON YOUR BOOTS AND OPINIONS IN YOUR MOUTH. GO AWAY. <<insult>>"
  opt "THEN I'LL FIND THE PIPER." -> @t1

stage seek
  goal goto hill
  then piper_talk
  journal "THE CHILDREN OF {HOME} WALK IN THEIR SLEEP TOWARD {HILL}, HUMMING. THE UNPAID PIPER IS THERE."

stage piper_talk
  talk piper
  say "YOU'VE COME ABOUT THE CHILDREN. I WON'T HURT THEM. I'LL ONLY TAKE THEM WHERE THE REEVE'S PROMISES GO: INTO THE HILL, WHERE NOTHING IS OWED. UNLESS SOMEONE PAYS. <<threat:vengeance>>"
  opt "HERE. THE REEVE'S PURSE." if have "THE PIPER'S PURSE" -> paid_end
  opt "THEN I'LL PAY. [20 GOLD]" check gold 20 -> player_paid else broke
  opt "PLAY ME FOR THEM. A BETTER TUNE." check level 4 -> @t2 else broke

stage broke
  talk piper
  say "NOT ENOUGH, AND NOT GOOD ENOUGH. BUT YOU TRIED, WHICH IS MORE THAN THE REEVE DID. I'LL MAKE YOU A BARGAIN: THE CHILDREN STAY. THE REEVE COMES WITH ME INSTEAD."
  opt "TAKE THE REEVE." -> reeve_taken_end
  opt "NO. NOBODY GOES." -> @t2

stage final
  talk piper
  say "A TUNE FOR A TUNE, THEN. THE HILL WILL JUDGE. ...HM. YOURS HAS A HOLE IN IT WHERE A MOTHER'S VOICE SHOULD BE. MINE TOO. VERY WELL. I'LL STOP PLAYING, AND YOU'LL OWE ME A SONG. SOMEDAY."
  opt "SOMEDAY." -> song_end

stage paid_end
  end success
  say "THE PIPER WEIGHED THE PURSE, AND LAUGHED, AND PLAYED ONE LAST TUNE THAT SENT EVERY CHILD IN {HOME} BACK TO BED FOR GOOD. THE BRIDGE WILL HAVE TO WAIT."
  journal "THE REEVE OF {HOME} PAID THE PIPER AT LAST. THE CHILDREN SLEEP IN THEIR BEDS."
  do take "THE PIPER'S PURSE"
  do reward fair
  do remember reeve "THE BRIDGE IS STILL A FORD. EVERYONE REMINDS ME. EVERY TIME THEY GET THEIR FEET WET."
  do fact "THE REEVE OF {HOME} TRIED TO CHEAT A PIPER, AND PAID IN THE END, AND THE TOWN STILL HAS NO BRIDGE."

stage player_paid
  talk piper
  say "FROM YOUR OWN PURSE? FOR CHILDREN NOT YOURS, IN A TOWN NOT YOURS? ...KEEP SOME. I'LL TAKE HALF AND THE LESSON. THEY'LL SLEEP TONIGHT."
  do gold -10
  opt "AND THE REEVE?" -> generous_end

stage generous_end
  end success
  say "THE PIPER LEFT THAT NIGHT. THE REEVE DINES ALONE NOW: EVERYONE IN {HOME} KNOWS WHOSE PURSE PAID AND WHOSE DIDN'T."
  journal "YOU PAID THE PIPER YOURSELF. THE CHILDREN OF {HOME} SLEEP. THE REEVE DOES NOT, MUCH."
  do reward fair
  do fame 1
  do remember giver "NOBODY BUYS THE REEVE A DRINK NOW. THEY BUY YOU THREE. <<praise>>"

stage song_end
  end success
  say "THE PIPER WALKED INTO THE HILL, STILL HUMMING, AND THE CHILDREN WOKE IN THEIR OWN BEDS WITH MUD ON THEIR FEET AND NO TUNE IN THEIR HEADS."
  journal "YOU OUT-PLAYED THE PIPER, OR NEAR ENOUGH. THE CHILDREN OF {HOME} ARE FREE. YOU OWE A SONG."
  do reward fair
  do mark owes_a_song
  do remember giver "THE CHILDREN HUM A NEW TUNE NOW. YOURS. IT'S NOT AS GOOD. THANK THE GODS."
  do fact "THE CHILDREN OF {HOME} ONCE WALKED IN THEIR SLEEP TOWARD {HILL}. A STRANGER PLAYED THEM HOME."

stage reeve_taken_end
  end success
  say "THE REEVE WENT INTO THE HILL AT MIDNIGHT, WALKING IN HIS SLEEP AND HUMMING. THE CHILDREN STAYED. THE PURSE WAS FOUND IN HIS BED, FULL."
  journal "THE PIPER TOOK {REEVE} THE REEVE INTO {HILL} INSTEAD OF THE CHILDREN. YOU AGREED TO IT."
  do hide reeve
  do reward small
  do mark gave_the_reeve
  do remember giver "THE CHILDREN ARE SAFE. THE REEVE IS... SOMEWHERE. I DON'T ASK. NOBODY ASKS."
  do fact "THE REEVE OF {HOME} WALKED INTO {HILL} ONE NIGHT HUMMING, AND DID NOT COME OUT. THE TOWN BUILT ITS BRIDGE."
)SAGA";

const char* const kWolfDoor = R"SAGA(
title [[THE WOLF AT THE DOOR|THE SWEET VOICE AT THE BOARDS|LIFT THE BAR, LITTLE ONES]]
hook npc any
pitch "[[pitch=WHY IS YOUR DOOR BARRED AT NOON?|YOU KEEP LOOKING BACK AT THE HOUSE.|WHO SINGS AT YOUR DOOR?]]"
hint "[[~pitch|DON'T LET ANYONE IN. THAT'S WHAT I TELL THEM. DON'T LET ANYONE IN.|DON'T LET ANYONE IN. THAT'S WHAT I TELL THEM. DON'T LET ANYONE IN.|SOMEONE SINGS TO MY CHILDREN THROUGH THE DOOR WHILE I'M AT MARKET. HE KNOWS THEIR NAMES.]]"
role giver giver
role home site home
role camp site camp near
role wolf person male at camp
slot t1 -> tracks deceit identity rival
slot t2 -> confront mercy betrayal price

stage start
  talk giver
  say "WHILE I'M AT MARKET, A MAN COMES TO MY DOOR AND SINGS TO MY [[TWO|THREE]] LITTLE ONES THROUGH THE BOARDS. HE KNOWS THEIR NAMES. HE ASKS THEM TO LIFT THE BAR, IN MY NAME. YESTERDAY THE YOUNGEST NEARLY DID. <<plea:fear>>"
  opt "I'LL WATCH THE HOUSE." -> watch
  opt "WHAT DOES HIS VOICE SOUND LIKE?" -> voice
  opt "BAR THE DOOR AND STAY HOME." -> refused

stage voice
  talk giver
  say "SWEET. TOO SWEET, LIKE HONEY ON A KNIFE. THE CHILDREN SAY HIS HAND THROUGH THE CAT-HOLE WAS ROUGH AND GREY. <<omen>>"
  opt "I'LL WATCH THE HOUSE." -> watch
  opt "I'LL FIND WHERE HE SLEEPS." -> @t1

stage refused
  end fail
  say "<<refusal:fear>> AND WHO GOES TO MARKET? THE CHILDREN EAT, YOU KNOW."
  journal "YOU TOLD {GIVER} TO STAY HOME AND BAR THE DOOR."
  do remember giver "HE STILL COMES. I STAY HOME NOW. WE EAT LESS. WE'RE ALIVE."

stage watch
  goal wait 1
  then @t1
  journal "A MAN SINGS TO {GIVER}'S CHILDREN THROUGH THE DOOR WHILE {GIVER} IS AT MARKET. WATCH THE HOUSE."

stage tracks
  goal goto camp
  then @t2
  journal "THE SINGER'S TRACKS LEAD FROM {GIVER}'S DOOR IN {HOME} TO {CAMP}. FOLLOW THEM."

stage confront
  talk wolf
  say "YOU FOLLOWED ME. A GOOD TRACKER. THEY'RE MINE, YOU KNOW, THE LITTLE ONES. {GIVER} PUT ME OUT WHEN I TOOK TO THE ROAD WITH THESE LADS. I ONLY WANT TO HEAR THEM LAUGH ONCE MORE BEFORE SOMEONE HANGS ME."
  opt "THEN ASK {GIVER}. NOT THE CHILDREN." -> ask_giver
  opt "NEVER GO NEAR THEM AGAIN." check level 3 -> driven_end else fight
  opt "THE WATCH WILL HANG YOU SOONER." -> hanged_end

stage ask_giver
  goal goto home
  then plea_home
  do moves wolf home
  journal "{WOLF}, A ROAD-MAN OF {CAMP}, IS THE FATHER OF {GIVER}'S CHILDREN. HE WILL ASK {GIVER} AT THE DOOR, NOT THE CHILDREN."

stage plea_home
  talk giver
  say "{WOLF}. OF COURSE IT'S {WOLF}. HE'S OUT THERE ON THE STEP WITH HIS HAT IN HIS HANDS, LIKE A SUITOR. HE WAS NEVER CRUEL TO THEM. ONLY TO ME, AND ONLY WITH LIES. WHAT DO I DO?"
  opt "ONE HOUR. YOU IN THE ROOM." -> visit_end
  opt "SEND HIM AWAY." -> sent_end

stage fight
  goal kill 3 bandit in camp
  then fought_end
  journal "{WOLF}'S ROAD-LADS WILL NOT LET HIM BE DRIVEN OFF. DEAL WITH THEM AT {CAMP}."

stage driven_end
  end success
  say "HE LOOKED AT YOU A LONG TIME, THEN PICKED UP HIS PACK. NOBODY SINGS AT {GIVER}'S DOOR NOW. THE YOUNGEST ASKS WHERE THE SONG WENT."
  journal "YOU DROVE {WOLF} AWAY FROM {GIVER}'S DOOR. THE CHILDREN ARE SAFE, AND DON'T KNOW WHAT THEY LOST."
  do reward fair
  do hide wolf
  do remember giver "QUIET AT LAST. THE YOUNGEST STILL HUMS HIS SONG SOMETIMES. I DON'T HAVE THE HEART TO STOP IT."

stage fought_end
  end success
  say "THE LADS RAN OR FELL. {WOLF} WAS GONE BY MORNING, AND THE CAMP WITH HIM. THE DOOR OF {GIVER}'S HOUSE STANDS OPEN IN THE DAYTIME AGAIN."
  journal "YOU BROKE {WOLF}'S BAND AT {CAMP}. HE HAS NOT BEEN SEEN NEAR {HOME} SINCE."
  do reward fair
  do hide wolf
  do remember giver "THE DOOR'S OPEN. I'D FORGOTTEN WHAT THE KITCHEN LOOKS LIKE IN SUNLIGHT. <<relief>>"

stage hanged_end
  end success
  say "THE WATCH TOOK HIM AT DAWN. THE CHILDREN WERE NOT TOLD WHY THE BELL RANG. {GIVER} WAS, AND SAT DOWN, AND DID NOT GET UP FOR A WHILE."
  journal "YOU GAVE {WOLF} TO THE WATCH. HE WAS THE CHILDREN'S FATHER. {GIVER} KNOWS YOU KNEW."
  do reward fair
  do hide wolf
  do mark gave_to_the_rope
  do remember giver "IT HAD TO BE DONE. I KNOW. DON'T COME ROUND FOR A WHILE."
  do fact "A ROAD-MAN WAS HANGED NEAR {HOME} FOR SINGING AT A DOOR. HIS CHILDREN STILL HUM THE SONG."

stage visit_end
  end success
  say "ONE HOUR, WITH {GIVER} IN THE CORNER AND A POKER IN THE FIRE. THE CHILDREN LAUGHED SO HARD THE NEIGHBOURS CAME TO SEE. THEN HE WENT, AS HE PROMISED."
  journal "{WOLF} SAW HIS CHILDREN FOR ONE HOUR, UNDER {GIVER}'S EYE, AND KEPT HIS WORD TO GO."
  do reward rich
  do remember giver "HE SENDS A CARVED BIRD EVERY SPRING. NO LETTER. JUST A BIRD. THE CHILDREN HAVE NINE NOW."
  do fact "A ROAD-MAN SENDS CARVED BIRDS TO {HOME} EVERY SPRING. NOBODY ASKS WHO FOR."

stage sent_end
  end success
  say "{GIVER} OPENED THE DOOR A HAND'S WIDTH AND SAID ONE WORD. {WOLF} PUT HIS HAT ON AND WENT. THE CHILDREN WATCHED FROM THE WINDOW AND DID NOT KNOW WHAT THEY WERE WATCHING."
  journal "{GIVER} SENT {WOLF} AWAY FROM THE DOOR. THE CHILDREN ARE SAFE, AND ASK QUESTIONS."
  do reward fair
  do hide wolf
  do remember giver "THEY ASK WHO THE MAN WITH THE HAT WAS. I SAY A TRAVELLER. ONE DAY I'LL SAY MORE."
)SAGA";

const char* const kHonestAxe = R"SAGA(
title [[THE HONEST AXE|GOLD, SILVER AND IRON|WHAT THE POOL GIVES BACK]]
hook npc any
pitch "[[pitch=IS THAT AXE GOLD?|WHERE'S YOUR NEIGHBOUR?|WHY ARE YOU STARING AT THE WATER?]]"
hint "[[~pitch|I DROPPED MY AXE IN THE OLD POOL AND THE POOL GAVE ME THREE. NOW MY NEIGHBOUR IS IN IT.|HONESTY PAYS, THEY SAY. IT PAID ME. IT DROWNED HIM.|HONESTY PAYS, THEY SAY. IT PAID ME. IT DROWNED HIM.]]"
role giver giver
role home site home
role ruin ruin near
role lady person female at ruin
role greedy person male at ruin
slot t1 -> pool rival wonder deceit
slot t2 -> plead mercy price betrayal

stage start
  talk giver
  say "MY AXE FELL IN THE DROWNED POOL AT {RUIN}. A PALE LADY ROSE WITH A GOLD AXE: MINE? NO. SILVER? NO. MY OWN IRON ONE? YES. SHE GAVE ME ALL THREE. MY NEIGHBOUR {GREEDY} HEARD, THREW IN HIS OWN, AND SAID YES TO GOLD. <<grief:shame>>"
  opt "AND HE NEVER CAME BACK?" -> neighbour
  opt "I'LL GO TO THE POOL." -> @t1
  opt "GREED DROWNED HIM. LEAVE IT." -> refused

stage neighbour
  talk giver
  say "HIS BOOTS ARE ON THE BANK. HIS WIFE SITS BY THEM. HE WAS A FOOL, BUT HE LENT ME HIS CART EVERY HARVEST. AND HE ONLY WENT BECAUSE I BOASTED. <<plea:shame>>"
  opt "THEN I'LL GO TO THE POOL." -> @t1
  opt "YOUR BOAST, YOUR TROUBLE." -> refused

stage refused
  end fail
  say "<<doubt>> MAYBE YOU'RE RIGHT. IT DOESN'T FEEL RIGHT."
  journal "YOU LEFT {GREEDY} IN THE DROWNED POOL AT {RUIN}."
  do remember giver "I SOLD THE GOLD AXE. I BOUGHT HIS WIFE A NEW CART. IT DIDN'T HELP EITHER OF US."

stage pool
  goal goto ruin
  then lady_test
  journal "{GREEDY} OF {HOME} LIED TO THE PALE LADY OF THE DROWNED POOL AT {RUIN} AND WENT UNDER. SPEAK TO HER."

stage lady_test
  talk lady
  say "ANOTHER ONE. YOU HAVE COME FOR SOMETHING. EVERYONE DOES. TELL ME, STRANGER, BEFORE YOU ASK: IS THIS GOLD CUP YOURS? [[I FOUND IT ON THE ROAD.|IT FELL FROM YOUR PACK.|IT HAS YOUR MARK ON IT.]]"
  opt "NO. IT ISN'T MINE." -> @t2
  opt "YES. IT'S MINE." -> liar_end

stage plead
  talk lady
  say "HONEST. TWO IN ONE SEASON. YOU WANT THE LIAR BACK. HE IS COLD AND SORRY AND VERY BORING. I WILL GIVE HIM BACK FOR WHAT I GAVE AWAY: THE GOLD AXE. OR FOR SOMETHING OF YOURS THAT IS TRUE."
  opt "{GIVER} WILL BRING BACK THE AXE." -> axe_end
  opt "TAKE MY OWN BLADE. IT'S TRUE." -> blade_end
  opt "KEEP HIM. HE LIED." -> kept_end

stage liar_end
  end fail
  say "THE LADY SMILED AND SANK. THE CUP WENT WITH HER. SO DID YOUR PURSE, THOUGH YOU DID NOT SEE IT GO."
  journal "YOU LIED TO THE LADY OF THE DROWNED POOL AT {RUIN}. {GREEDY} IS STILL UNDER THE WATER."
  do gold -20
  do mark lied_to_the_pool
  do remember giver "YOU SAID YES? TO HER? OH, YOU FOOL. YOU FOOL. AT LEAST YOU CAME BACK."

stage axe_end
  end success
  say "{GIVER} CARRIED THE GOLD AXE TO THE POOL AND THREW IT IN WITHOUT BEING ASKED. {GREEDY} CAME UP COUGHING WEED, AND THE TWO OF THEM WALKED HOME WITHOUT A WORD AND WITH ONE CART."
  journal "{GIVER} GAVE BACK THE GOLD AXE TO THE DROWNED POOL, AND {GREEDY} CAME HOME."
  do reward fair
  do remember giver "I'M AN IRON-AXE MAN AGAIN. SUITS ME. {GREEDY} STILL BORROWS IT. <<proverb>>"
  do fact "A WOODCUTTER OF {HOME} GAVE A GOLD AXE BACK TO THE DROWNED POOL AT {RUIN} TO BUY HIS NEIGHBOUR'S LIFE."

stage blade_end
  end success
  say "YOUR OWN BLADE SANK WITHOUT A RIPPLE. {GREEDY} CAME UP COUGHING WEED AND CALLING FOR HIS MOTHER. THE LADY KEPT YOUR BLADE, AND SOMETIMES, YOU THINK, SHE POLISHES IT."
  journal "YOU GAVE YOUR OWN BLADE TO THE LADY OF THE POOL AT {RUIN} FOR {GREEDY}'S LIFE."
  do reward rich
  do fame 1
  do remember greedy "YOU GAVE YOUR BLADE FOR ME. ME. I'D HAVE SOLD YOU FOR THE GOLD. I'M NOT SAYING IT TWICE."
  do fact "A STRANGER BOUGHT BACK A DROWNED LIAR FROM THE POOL AT {RUIN} WITH AN HONEST BLADE."

stage kept_end
  end success
  say "THE LADY NODDED, AS IF YOU HAD PASSED A SECOND TEST. {GREEDY}'S BOOTS STAYED ON THE BANK. HIS WIFE STILL SITS BY THEM."
  journal "YOU LEFT {GREEDY} TO THE DROWNED POOL AT {RUIN}. HE LIED; THE POOL KEEPS LIARS."
  do reward small
  do remember giver "HIS WIFE WON'T LOOK AT ME. I DON'T BLAME HER. <<grief:shame>>"
  do fact "THE DROWNED POOL AT {RUIN} KEEPS A LIAR OF {HOME}. HIS BOOTS ARE STILL ON THE BANK."
)SAGA";

const char* const kStolenShadow = R"SAGA(
title [[THE STOLEN SHADOW|A SHADOW FOR A RUN OF LUCK|THE GREY MAN'S SACK]]
hook npc any
pitch "[[pitch=WHY DO THE DOGS BARK AT HIM?|WHAT'S WRONG WITH THAT ONE?|HE CASTS NO SHADOW.]]"
hint "[[~pitch|LUCK, HE SAYS. HE SOLD SOMETHING FOR LUCK. I DIDN'T ASK WHAT. I SHOULD HAVE.|DON'T STARE. EVERYONE STARES. HE CASTS NO SHADOW, AND THE DOGS HATE HIM, AND HE WON'T SAY WHY.|LUCK, HE SAYS. HE SOLD SOMETHING FOR LUCK. I DIDN'T ASK WHAT. I SHOULD HAVE.]]"
role giver giver
role home site home
role kin resident kin of giver
role camp site camp near
role grey person male at camp
slot t1 -> seek deceit rival world
slot t2 -> choose mercy wonder price

stage start
  talk giver
  say "MY {KIN.KIN} {KIN} SOLD {KIN.HIS} SHADOW TO A GREY MAN FOR [[luck=A RUN OF LUCK AT DICE|A YEAR OF FAT HARVESTS|THE LOVE OF A WIDOW]]. IT WORKED. NOW THE DOGS BARK AT {KIN.HIM}, THE PRIEST WON'T BLESS {KIN.HIM}, AND {KIN.HE} GROWS THINNER BY THE WEEK. <<plea>>"
  opt "WHERE IS THE GREY MAN?" -> @t1
  opt "LET ME TALK TO {KIN}." -> kin_talk
  opt "{KIN.HE} MADE THE TRADE." -> refused

stage kin_talk
  talk kin
  say "IT WAS ONLY A SHADOW. WHAT DOES A SHADOW DO? ...IT KEPT ME COMPANY. I DIDN'T KNOW THAT TILL IT WAS GONE. <<grief:shame>>"
  opt "I'LL BRING IT BACK." -> @t1

stage refused
  end fail
  say "<<farewell>> I'LL BUY {KIN.HIM} A LAMP. LAMPS DON'T HELP, BUT I'LL BUY ONE."
  journal "YOU LEFT {KIN} OF {HOME} WITHOUT A SHADOW."
  do remember giver "THE DOGS BIT {KIN} TODAY. JUST A NIP. THEY'VE NEVER BITTEN ANYONE."

stage seek
  goal goto camp
  then grey_talk
  say "HE HAD A SACK THAT MOVED BY ITSELF. <<warning>>"
  journal "{KIN} OF {HOME} SOLD {KIN.HIS} SHADOW FOR [[=luck]]. THE GREY MAN WHO BOUGHT IT CAMPS AT {CAMP}."

stage grey_talk
  talk grey
  say "THAT ONE? A SHADOW WITH A LIMP AND A HUM. IT'S IN THE SACK WITH THE OTHERS. I DON'T SELL BACK. I WAGER. ONE THROW: YOUR SHADOW AGAINST HIS. OR PAY, IF YOU'RE DULL. <<greet>>"
  opt "ONE THROW, THEN." check level 3 -> won else lost_throw
  opt "I'LL PAY. [25 GOLD]" check gold 25 -> bought else poor
  opt "OPEN THE SACK. NOW." -> forced

stage lost_throw
  talk grey
  say "OH, BAD LUCK. BUT I'M FOND OF YOU, SO I'LL NOT TAKE YOURS. NOT TODAY. TAKE HIS AND GO, AND WATCH WHERE YOU STAND AT NOON."
  do mark grey_owed
  opt "...THANK YOU?" -> @t2

stage won
  talk grey
  say "DOUBLE SIXES. YOU CHEAT, OR THE GODS LIKE YOU, AND EITHER WAY I CAN'T ABIDE IT. TAKE IT."
  opt "GLADLY." -> @t2

stage bought
  talk grey
  say "COIN. HOW ORDINARY. VERY WELL. IT'S THE ONE TRYING TO HOLD HANDS WITH THE OTHERS."
  do gold -25
  opt "I'LL TAKE IT." -> @t2

stage poor
  talk grey
  say "THAT'S NOT TWENTY-FIVE. THAT'S A HOPE WITH A HOLE IN IT. NO MATTER: I LIKE A HOPE. TAKE IT. YOU OWE ME A KINDNESS."
  do mark grey_owed
  opt "I'LL OWE YOU." -> @t2

stage forced
  talk grey
  say "OPEN IT YOURSELF. ...THERE. NOW TWENTY SHADOWS ARE LOOSE IN THE WORLD WITH NOBODY TO FOLLOW, AND THAT IS YOUR DOING. HIS IS THE ONE THAT STAYED. <<curse>>"
  do mark loosed_the_shadows
  opt "...IT STAYED?" -> @t2

stage choose
  talk kin
  say "IT'S HERE? WHERE? ...OH. IT WON'T COME NEAR ME. IT SITS BY THE WALL WITH ITS KNEES UP. I KICKED IT, YOU KNOW. WHEN I WAS SMALL, I USED TO KICK AT IT. I THOUGHT IT WAS A GAME."
  opt "ASK IT. KINDLY." -> asked_end
  opt "LET IT GO FREE." -> freed_end

stage asked_end
  end success
  say "{KIN} KNELT ON THE FLOOR AND SAID SORRY TO A PATCH OF DARK BY THE WALL. AFTER A LONG TIME THE DARK MOVED, AND LAY DOWN AT {KIN.HIS} FEET, WHERE IT BELONGED."
  journal "{KIN} OF {HOME} ASKED {KIN.HIS} SHADOW BACK, KINDLY, AND IT CAME."
  do reward fair
  do befriend kin
  do remember kin "I WAVE AT IT IN THE MORNINGS. IT WAVES BACK. DON'T LAUGH. <<thanks:love>>"
  do fact "{KIN} OF {HOME} ONCE SOLD {KIN.HIS} SHADOW FOR [[=luck]]. {KIN.HE} GOT IT BACK BY SAYING SORRY TO IT."

stage freed_end
  end success
  say "THE SHADOW SLID OUT UNDER THE DOOR AND WAS GONE INTO THE EVENING. {KIN} STOOD IN THE LAMPLIGHT, SHADOWLESS, AND LAUGHED, AND COULD NOT SAY WHY."
  journal "YOU LET {KIN}'S SHADOW GO FREE. {KIN} WILL LIVE WITHOUT ONE."
  do reward small
  do remember kin "THE DOGS STILL BARK. I BARK BACK NOW. SOMEWHERE OUT THERE MY SHADOW IS FREE. GOOD FOR IT."
  do fact "SOMEWHERE NEAR {HOME} A SHADOW WALKS WITH NOBODY TO FOLLOW. CHILDREN SAY IT WAVES."
)SAGA";

const char* const kFeatherCloak = R"SAGA(
title [[THE FEATHER CLOAK|WHAT CAME OUT OF THE WATER|THE WIFE WHO WATCHES THE SKY]]
hook npc any
pitch "[[pitch=WHO IS SHE, BY THE WATER?|WHY DOES SHE WATCH THE BIRDS?|SHE'S CRYING. SHOULD SOMEONE GO?]]"
hint "[[~pitch|EVERY DUSK SHE STANDS IN THE SHALLOWS AND WATCHES THE WHITE BIRDS GO OVER. EVERY DUSK.|EVERY DUSK SHE STANDS IN THE SHALLOWS AND WATCHES THE WHITE BIRDS GO OVER. EVERY DUSK.|SOME THINGS YOU HIDE FROM LOVE, AND SOME THINGS LOVE HIDES FROM YOU.]]"
role giver giver
role home site home
role kin resident kin of giver
role wife person female at home
role cave site cave near
slot t1 -> seek wonder deceit rival
slot t2 -> choose mercy price return

stage start
  talk giver
  say "{WIFE}, WHO WED MY {KIN.KIN} {KIN}, CAME OUT OF THE [[MERE|LAKE|MARSH]] [[years=SEVEN|TEN|TWELVE]] SPRINGS AGO WITH NOTHING ON HER BUT RAIN. {KIN} HID HER FEATHER CLOAK SO SHE COULDN'T FLY HOME. SHE KNOWS NOW. <<grief:love>>"
  opt "LET ME TALK TO {WIFE}." -> wife_talk
  opt "WHERE DID {KIN} HIDE IT?" -> where
  opt "LEAVE MARRIED FOLK ALONE." -> refused

stage wife_talk
  talk wife
  say "HE IS NOT CRUEL. HE WAS AFRAID. [[=years]] YEARS I'VE WASHED HIS SHIRTS AND LOVED HIM, AND EVERY SPRING MY SISTERS CALL ME FROM THE WATER. I'D GO, AND I'D COME BACK. NOBODY WHO HIDES A THING BELIEVES THAT."
  opt "I'LL FIND THE CLOAK." -> where

stage where
  talk giver
  say "{KIN} TAKES THE CART TO {CAVE} EVERY MONTH AND COMES BACK WITH NOTHING. THAT'S WHERE IT IS, I'D WAGER MY TEETH. <<doubt>>"
  opt "THEN I'LL FETCH IT." -> @t1

stage refused
  end fail
  say "<<proverb>> AYE. BUT SHE STANDS IN THE WATER EVERY DUSK, AND IT'S GETTING COLD."
  journal "YOU WOULD NOT MEDDLE IN {KIN} AND {WIFE}'S MARRIAGE."
  do remember giver "SHE STILL STANDS IN THE SHALLOWS AT DUSK. HER LIPS GO BLUE. {KIN} CARRIES HER IN."

stage seek
  goal fetch "A CLOAK OF WHITE FEATHERS" in cave
  then @t2
  journal "{KIN} OF {HOME} HID {WIFE}'S FEATHER CLOAK IN {CAVE}. FIND IT."

stage choose
  talk kin
  say "YOU FOUND IT. OF COURSE YOU FOUND IT. ...IF SHE HAS IT, SHE'LL GO. I'VE KNOWN IT EVERY DAY FOR [[=years]] YEARS. WHAT WOULD YOU HAVE ME DO?"
  opt "GIVE IT TO HER YOURSELF." -> given_end
  opt "I'LL GIVE IT TO HER." -> player_end
  opt "BURN IT. SHE'LL STAY." -> burned_end

stage given_end
  end success
  say "{KIN} PUT THE CLOAK IN {WIFE}'S HANDS AT DUSK. SHE FLEW AT ONCE, WHITE OVER THE WATER, AND {KIN} STOOD THERE ALL NIGHT. AT DAWN SHE WALKED OUT OF THE SHALLOWS AND TOOK HIS HAND."
  journal "{KIN} GAVE {WIFE} HER FEATHER CLOAK. SHE FLEW, AND CAME BACK BY CHOICE."
  do take "A CLOAK OF WHITE FEATHERS"
  do reward rich
  do befriend kin
  do remember wife "EVERY SPRING I FLY WITH MY SISTERS. EVERY SUMMER I COME HOME. HE STILL WAITS ON THE SHORE. I LIKE THAT HE WAITS."
  do fact "A WOMAN OF {HOME} FLIES WITH THE WHITE BIRDS EVERY SPRING AND COMES HOME EVERY SUMMER."

stage player_end
  end success
  say "YOU GAVE {WIFE} THE CLOAK BEHIND {KIN}'S BACK. SHE KISSED YOUR CHEEK AND WAS GONE BEFORE YOU COULD SEE HER FACE. SHE HAS NOT COME BACK. NOT YET."
  journal "YOU GAVE {WIFE} HER CLOAK. SHE FLEW. {KIN} WAITS ON THE SHORE."
  do take "A CLOAK OF WHITE FEATHERS"
  do hide wife
  do reward fair
  do remember kin "YOU GAVE IT HER. YOU WERE RIGHT TO. I HATE YOU A LITTLE. SHE'LL COME BACK. SHE SAID SHE WOULD. <<grief:hope>>"

stage burned_end
  end success
  say "THE FEATHERS BURNED BLUE. {WIFE} SCREAMED ONCE, FAR AWAY BY THE WATER, AND THEN NEVER AGAIN. SHE STAYED. SHE NEVER SANG AGAIN EITHER."
  journal "YOU HAD {WIFE}'S FEATHER CLOAK BURNED. SHE CAN NEVER LEAVE {HOME} NOW."
  do take "A CLOAK OF WHITE FEATHERS"
  do reward small
  do mark burned_the_cloak
  do remember wife "YOU. I KNOW WHAT YOU SAID TO HIM. I KNOW. <<curse>>"
  do fact "A WOMAN OF {HOME} STANDS IN THE SHALLOWS AT DUSK AND WATCHES THE WHITE BIRDS. SHE NO LONGER WEEPS. SHE NO LONGER SINGS."
)SAGA";

const char* const kStoneSoup = R"SAGA(
title [[THE STONE SOUP|A POT AND A STONE|EVERY DOOR SHUT]]
hook npc any
pitch "[[pitch=WHY IS EVERY SHUTTER CLOSED?|WHO'S THAT WITH THE BIG POT?|IS THERE FOOD IN TOWN?]]"
hint "[[~pitch|LEAN TIMES. EVERYONE'S GOT A SACK UNDER THE FLOORBOARDS AND A STARVING FACE AT THE DOOR.|THERE'S A STRANGER IN THE SQUARE BOILING A STONE. A STONE. AND PEOPLE ARE WATCHING.|LEAN TIMES. EVERYONE'S GOT A SACK UNDER THE FLOORBOARDS AND A STARVING FACE AT THE DOOR.]]"
role giver giver
role home site home
role cook person [[male|female]] at home
role miser resident any
role mother resident friend of giver
var exposed 0
slot t1 -> square rival deceit world
slot t2 -> feast mercy price betrayal

stage start
  talk giver
  say "THE HARVEST WAS THIN AND EVERY DOOR IN {HOME} IS SHUT. NOBODY SHARES, EVERYBODY HIDES A SACK. NOW A STRANGER, {COOK}, HAS SET A POT IN THE SQUARE AND IS BOILING A STONE. SAYS IT MAKES SOUP. <<doubt>>"
  opt "LET'S GO AND SEE." -> square_talk
  opt "A CON. I'LL EXPOSE IT." -> expose
  opt "LET THEM BOIL ROCKS." -> refused

stage refused
  end fail
  journal "YOU LEFT {HOME} TO ITS SHUT DOORS AND ITS STRANGER WITH A STONE."
  do remember giver "THE STRANGER LEFT. THE STONE STAYED IN THE SQUARE. CHILDREN SIT ON IT. NOBODY SHARES."

stage expose
  talk cook
  say "A CON? OF COURSE IT'S A CON, FRIEND. IT'S THE BEST CON THERE IS. WATCH, AND THEN DECIDE WHETHER TO SHOUT. <<proverb>>"
  do set exposed 1
  opt "...I'LL WATCH." -> @t1
  opt "NO. I'LL TELL THEM NOW." -> told_end

stage square_talk
  talk cook
  say "A FINE STONE SOUP. GOOD AS IT IS, OF COURSE. BUT AN ONION WOULD LIFT IT. JUST ONE. AND I HEAR {MISER} HAS ONIONS. NOT THAT I'D ASK. <<greet>>"
  opt "I'LL ASK {MISER}." -> @t1

stage square
  goal goto home
  then miser_talk
  journal "{COOK}, A STRANGER IN {HOME}, IS MAKING SOUP FROM A STONE IN THE SQUARE. IT NEEDS ONLY A LITTLE SOMETHING. ASK {MISER}."

stage miser_talk
  talk miser
  say "AN ONION? FOR A STONE? ...WELL. IF THE STONE'S DOING MOST OF THE WORK. ONE ONION. AND MAYBE THE CARROT, SINCE IT'S GOING SOFT. AND I SUPPOSE THE HAM BONE. DON'T TELL ANYONE IT WAS ME."
  opt "AND {MOTHER}? WHAT WILL SHE GIVE?" -> mother_talk

stage mother_talk
  talk mother
  say "{MISER} GAVE A HAM BONE? {MISER}? THEN I'VE BARLEY, AND I'LL NOT BE OUTDONE BY {MISER}. AND THE NEIGHBOURS WILL WANT TO BE SEEN, NOW. OH, THEY WILL."
  opt "TO THE SQUARE, THEN." -> @t2

stage feast
  talk cook
  say "LOOK AT THEM. EVERY DOOR IN {HOME} OPEN, EVERY HAND IN THE POT. AND THE STONE? STILL A STONE. NOW, FRIEND, DO YOU TELL THEM IT WAS NEVER THE STONE? OR DO I LEAVE IT HERE, FOR THE NEXT LEAN YEAR?"
  opt "LEAVE THE STONE. LET THEM BELIEVE." -> stone_end
  opt "TELL THEM. THEY DID THIS." -> truth_end

stage told_end
  end success
  say "YOU SHOUTED IT ACROSS THE SQUARE. THE STRANGER BOWED, PACKED THE POT AND LEFT. THE DOORS STAYED SHUT. YOU WERE RIGHT, AND {HOME} WAS JUST AS HUNGRY."
  journal "YOU EXPOSED {COOK}'S STONE SOUP AS A TRICK. YOU WERE RIGHT. NOBODY ATE."
  do reward small
  do remember giver "YOU WERE RIGHT ABOUT THE STONE. I KEEP THINKING ABOUT THAT. BEING RIGHT. <<proverb>>"

stage stone_end
  end success
  say "THE STRANGER LEFT THE STONE ON THE WELL WALL AND WALKED OFF WHISTLING. EVERY LEAN WINTER SINCE, {HOME} BOILS IT IN THE SQUARE, AND EVERYONE BRINGS A LITTLE SOMETHING."
  journal "{HOME} ATE STONE SOUP TOGETHER. THE STONE STAYS ON THE WELL WALL FOR THE NEXT LEAN YEAR."
  do reward fair
  do befriend miser
  do remember giver "THE SOUP STONE IS ON THE WELL WALL. NOBODY TOUCHES IT EXCEPT TO COOK. <<saying>>"
  do fact "THERE IS A SOUP STONE ON THE WELL WALL OF {HOME}. IN LEAN YEARS THE WHOLE TOWN COOKS WITH IT."

stage truth_end
  end success
  say "YOU TOLD THEM. FOR A MOMENT NOBODY SPOKE. THEN {MISER} LAUGHED SO HARD SOUP CAME OUT OF HIS NOSE, AND THE WHOLE SQUARE LAUGHED WITH HIM, AND NOBODY WENT HOME UNTIL THE POT WAS DRY."
  journal "YOU TOLD {HOME} THE STONE WAS NEVER THE SOUP. THEY ATE ANYWAY, AND KNOW IT WAS THEMSELVES."
  do reward fair
  do befriend mother
  do remember mother "WE DON'T NEED A STONE NOW. WE JUST NEED SOMEONE TO START. I START. <<boast:pride>>"
  do fact "IN THE LEAN YEAR, {HOME} LEARNED IT COULD FEED ITSELF IF SOMEONE WENT FIRST."
)SAGA";

void add(std::vector<Archetype>& v, const char* id, const char* name, uint32_t themes, uint32_t needs, uint16_t motives,
         uint32_t twists, const char* body) {
  Archetype a;
  a.id = id;
  a.name = name;
  a.source = Source::Folk;
  a.themes = themes;
  a.needs = needs;
  a.tier = 2;
  a.motives = motives;
  a.twists = twists;
  a.body = body;
  v.push_back(a);
}

uint16_t mb(Motive a, Motive b, Motive c, Motive d = Motive::COUNT) {
  uint16_t m = (uint16_t)(motiveBit(a) | motiveBit(b) | motiveBit(c));
  if (d != Motive::COUNT) m |= motiveBit(d);
  return m;
}

}  // namespace

void addFolk(std::vector<Archetype>& v) {
  using M = Motive;
  add(v, "cursed_gift", "THE GIFT THAT WOULD NOT LEAVE", TH_CURSE | TH_GREED | TH_JUDGEMENT | TH_MERCY | TH_TEMPTATION,
      N_VILLAGE, mb(M::Fear, M::Love, M::Grief, M::Shame), TF_DECEIT | TF_RIVAL | TF_WONDER | TF_WORLD | TF_MERCY | TF_PRICE,
      kCursedGift);
  add(v, "fair_bargain", "WHAT GREETS YOU AT THE GATE", TH_BARGAIN | TH_TRICKERY | TH_SACRIFICE | TH_KINSHIP | TH_WONDER,
      N_CAVE, mb(M::Fear, M::Love, M::Shame, M::Grief), TF_DECEIT | TF_PRICE | TF_RIVAL | TF_WONDER | TF_MERCY | TF_RETURN,
      kFairBargain);
  add(v, "changeling", "THE CHILD WHO WAS NOT", TH_WONDER | TH_MERCY | TH_KINSHIP | TH_LOVE | TH_BARGAIN, N_CAVE | N_FRIENDS,
      mb(M::Fear, M::Love, M::Grief, M::Hope), TF_WONDER | TF_IDENTITY | TF_DECEIT | TF_MERCY | TF_PRICE | TF_RETURN, kChangeling);
  add(v, "true_name", "THE NAME THAT BINDS", TH_BARGAIN | TH_TRICKERY | TH_PRIDE | TH_MERCY | TH_WONDER, N_CAVE,
      mb(M::Fear, M::Shame, M::Pride, M::Love), TF_DECEIT | TF_RIVAL | TF_WONDER | TF_PRICE | TF_MERCY | TF_IDENTITY, kTrueName);
  add(v, "three_tasks", "THE THREE TASKS", TH_LOVE | TH_COURAGE | TH_KINSHIP | TH_TRICKERY | TH_GRIEF, N_CAVE,
      mb(M::Love, M::Hope, M::Pride), TF_RIVAL | TF_PRICE | TF_WORLD | TF_DECEIT | TF_WONDER, kThreeTasks);
  add(v, "youngest", "THE YOUNGEST OF THREE", TH_PRIDE | TH_MERCY | TH_KINSHIP | TH_HOSPITALITY | TH_SACRIFICE, N_CAVE,
      mb(M::Fear, M::Love, M::Grief, M::Hope), TF_RIVAL | TF_PRICE | TF_WONDER | TF_WORLD | TF_BETRAYAL | TF_DECEIT | TF_MERCY,
      kYoungest);
  add(v, "thorn_sleep", "THE SLEEPER IN THE THORNS", TH_CURSE | TH_HOSPITALITY | TH_KINSHIP | TH_MERCY | TH_PRIDE, N_CAVE,
      mb(M::Shame, M::Love, M::Fear, M::Grief), TF_MERCY | TF_IDENTITY | TF_WONDER | TF_PRICE | TF_DECEIT, kThornSleep);
  add(v, "wild_man", "THE WILD MAN OF THE WOOD", TH_CURSE | TH_MERCY | TH_LOVE | TH_HOMECOMING | TH_GRIEF, N_CAVE,
      mb(M::Grief, M::Love, M::Hope, M::Fear), TF_IDENTITY | TF_MERCY | TF_WONDER | TF_DECEIT | TF_RETURN | TF_PRICE | TF_WORLD,
      kWildMan);
  add(v, "piper", "THE PIPER UNPAID", TH_BARGAIN | TH_GREED | TH_JUDGEMENT | TH_VENGEANCE | TH_PRIDE, N_CAVE,
      mb(M::Fear, M::Love, M::Duty, M::Vengeance), TF_RIVAL | TF_DECEIT | TF_WORLD | TF_MERCY | TF_PRICE | TF_BETRAYAL, kPiper);
  add(v, "wolf_door", "THE WOLF AT THE DOOR", TH_TRICKERY | TH_KINSHIP | TH_MERCY | TH_JUDGEMENT | TH_LOVE, N_CAMP,
      mb(M::Fear, M::Love, M::Duty), TF_DECEIT | TF_IDENTITY | TF_RIVAL | TF_MERCY | TF_BETRAYAL | TF_PRICE, kWolfDoor);
  add(v, "honest_axe", "THE HONEST AXE", TH_GREED | TH_JUDGEMENT | TH_MERCY | TH_SACRIFICE | TH_WONDER, N_RUIN,
      mb(M::Shame, M::Grief, M::Duty), TF_RIVAL | TF_WONDER | TF_DECEIT | TF_MERCY | TF_PRICE | TF_BETRAYAL, kHonestAxe);
  add(v, "stolen_shadow", "THE STOLEN SHADOW", TH_BARGAIN | TH_CURSE | TH_GREED | TH_MERCY | TH_FREEDOM, N_CAMP,
      mb(M::Fear, M::Love, M::Shame, M::Grief), TF_DECEIT | TF_RIVAL | TF_WORLD | TF_MERCY | TF_WONDER | TF_PRICE, kStolenShadow);
  add(v, "feather_cloak", "THE FEATHER CLOAK", TH_LOVE | TH_FREEDOM | TH_TRICKERY | TH_WONDER | TH_GRIEF, N_CAVE,
      mb(M::Love, M::Grief, M::Shame, M::Hope), TF_WONDER | TF_DECEIT | TF_RIVAL | TF_MERCY | TF_PRICE | TF_RETURN, kFeatherCloak);
  add(v, "stone_soup", "THE STONE SOUP", TH_HUNGER | TH_TRICKERY | TH_HOSPITALITY | TH_FRIENDSHIP | TH_GREED,
      N_FRIENDS, mb(M::Hope, M::Greed, M::Pride, M::Duty), TF_RIVAL | TF_DECEIT | TF_WORLD | TF_MERCY | TF_PRICE | TF_BETRAYAL,
      kStoneSoup);
}

}  // namespace arch
}  // namespace saga
}  // namespace story
