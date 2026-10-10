// M6b "Sagas": archetypes, Sarah J. Maas-like themes (never names, characters, places, invented terms or plots). VOICE
// lane. The template markup and role conventions: rpg/story/saga.h.
//
// Only the feelings and shapes are borrowed: rival houses of the hidden folk and their deadly courtesy; a bargain
// written on the skin; two enemies forced to the same fire; a hidden heir and what the crown costs; a curse only a
// free choice of love or sacrifice can break; trials under the mountain. Every house, title and name here is our own.
//
//   rival_houses     THE TRUCE AT THE MILL       two hidden houses renew their truce at a mortal mill; the truce-gift
//                                                is gone and each blames the other
//   skin_bargain     THE MARK ON THE WRIST       a bargain inked on a friend's skin; the stranger calls in the last
//                                                service
//   enemy_fire       THE ENEMY AT THE FIRE       two sworn enemies of feuding houses must go to the same cave
//   crown_price      THE CROWN'S PRICE           a quiet neighbour is the last of a fallen house; the old oath-keepers
//                                                want a queen or king, and the crown wants everything
//   bound_lord       THE LORD WHO CANNOT LEAVE   a cursed lord in a ruin, a visitor who loves him, and what breaks it
//   mountain_trials  THE TRIALS UNDER THE HILL   a beloved held in a deep court; three trials; the court's offer
#include <vector>
#include "rpg/story/arch/arch_tables.h"

namespace story {
namespace saga {
namespace arch {

namespace {

const char* const kRivalHouses = R"SAGA(
title [[THE TRUCE AT THE MILL|TWO HOUSES AND A STOLEN GIFT|THE MIDSUMMER TRUCE]]
hook npc any
pitch "[[pitch=WHY ARE THERE TWO TABLES LAID?|WHO ARE THE TALL GUESTS?|YOU'RE SHAKING. WHAT IS IT?]]"
hint "[[~pitch|ONCE A YEAR THEY COME TO MY HOUSE, THE TALL ONES, AND SIT AT TWO TABLES AND DO NOT LOOK AT EACH OTHER. THIS YEAR SOMETHING IS WRONG.|ONCE A YEAR THEY COME TO MY HOUSE, THE TALL ONES, AND SIT AT TWO TABLES AND DO NOT LOOK AT EACH OTHER. THIS YEAR SOMETHING IS WRONG.|IF YOU SEE SOMEONE TOO BEAUTIFUL, DON'T THANK THEM FOR ANYTHING.]]"
role giver giver
role home site home
role hill site cave near
role stag person female at home
role moth person male at hill
var proof 0
slot t1 -> hill_go deceit rival betrayal
slot t2 -> judgement mercy price wonder

stage start
  talk giver
  say "EVERY MIDSUMMER TWO HOUSES OF THE HIDDEN FOLK, [[stag_h=THE WHITE STAG|THE HOLLY CROWN|THE BRIGHT ANTLER]] AND [[moth_h=THE GREY MOTH|THE BLACKTHORN|THE DROWNED BELL]], RENEW THEIR TRUCE AT MY TABLE OVER A CUP OF [[RED GLASS|BLACK SILVER|OLD BONE]]. THE CUP IS GONE. EACH SAYS THE OTHER TOOK IT. <<urgency:fear>>"
  opt "WHAT HAPPENS IF THE TRUCE FAILS?" -> fails
  opt "I'LL FIND THE CUP." -> stag_talk
  opt "HIDDEN FOLK SETTLE THEIR OWN." -> refused

stage fails
  talk giver
  say "THEIR WAR. FOUGHT IN OUR FIELDS, WITH OUR CROPS FOR BANNERS AND OUR CHILDREN FOR PAWNS. MY GRANDMOTHER REMEMBERED THE LAST ONE. SHE NEVER SAID WHAT SHE SAW. <<omen>>"
  opt "THEN I'LL FIND THE CUP." -> stag_talk

stage refused
  end fail
  say "<<farewell>> I'LL LAY THE TABLES ANYWAY. MAYBE THEY'LL BE KIND."
  journal "YOU LEFT THE TRUCE OF THE HIDDEN HOUSES TO FAIL AT {GIVER}'S TABLE IN {HOME}."
  do remember giver "THE BARLEY WENT BLACK IN ONE NIGHT. THE CHILDREN DREAM OF ANTLERS. I DON'T SLEEP."
  do fact "THE TRUCE OF THE HIDDEN HOUSES FAILED AT {HOME}. THE FIELDS THERE WILL REMEMBER."

stage stag_talk
  talk stag
  say "THE MORTAL'S CHAMPION. HOW SWEET. LET ME SAVE YOU TIME: THE HOUSE OF [[=moth_h]] HAS ALWAYS WANTED THIS WAR, AND THEIR ENVOY HOLDS COURT AT {HILL}. ASK HIM WHERE OUR CUP IS. WATCH HIS HANDS WHEN HE ANSWERS."
  opt "I'LL ASK HIM." -> @t1
  opt "YOU SEEM VERY SURE." -> stag_sure

stage stag_sure
  talk stag
  say "I AM ALWAYS VERY SURE. IT IS THE ONLY THING THAT KEEPS ONE ALIVE AT COURT. <<threat:pride>>"
  opt "THEN I'LL ASK HIM." -> @t1

stage hill_go
  goal goto hill
  then moth_talk
  journal "THE TRUCE-CUP OF THE HIDDEN HOUSES IS GONE FROM {GIVER}'S TABLE. THE ENVOY OF [[=moth_h]] KEEPS COURT AT {HILL}."

stage moth_talk
  talk moth
  say "OUR CUP? I WOULD SOONER STEAL MY OWN SHADOW. SHE TOOK IT HERSELF, OUR LOVELY ENVOY OF [[=stag_h]]: HER QUEEN DIES SOON, AND A WAR WOULD MAKE HER THE NEXT ONE. BUT I CANNOT PROVE IT. CAN YOU?"
  opt "SHOW ME YOUR HANDS." -> hands
  opt "I'LL SEARCH HER ROOM." -> search

stage hands
  talk moth
  say "WISE. EMPTY, SEE? AND INK-STAINED: I HAVE BEEN WRITING LETTERS OF APOLOGY FOR A THEFT I DID NOT DO, BECAUSE PEACE IS CHEAPER THAN PRIDE. <<saying>>"
  opt "THEN I'LL SEARCH HER ROOM." -> search

stage search
  goal fetch "A CUP OF [[=stag_h]]" in home
  then @t2
  do set proof 1
  journal "THE ENVOY OF [[=moth_h]] SAYS THE ENVOY OF [[=stag_h]] HID THE TRUCE-CUP HERSELF. SEARCH {HOME}."

stage judgement
  talk stag
  say "YOU FOUND IT. IN MY OWN TRUNK, UNDER MY OWN GOWNS. HOW CARELESS OF ME. ...WELL, MORTAL? WHAT WILL YOU DO WITH WHAT YOU KNOW? WHATEVER YOU CHOOSE, CHOOSE IT BEAUTIFULLY."
  opt "SHAME HER BEFORE BOTH TABLES." -> shamed_end
  opt "PUT IT BACK. SAY NOTHING. FOR A PRICE." -> bargain_end
  opt "GIVE IT TO THE OTHER HOUSE." -> moth_end

stage shamed_end
  end success
  say "YOU SET THE CUP ON THE TABLE AND SAID WHERE YOU FOUND IT. BOTH HOUSES STOOD. NOBODY SPOKE. THE ENVOY OF [[=stag_h]] BOWED TO YOU, VERY LOW, AND WAS NEVER SEEN AGAIN."
  journal "YOU SHAMED THE ENVOY OF [[=stag_h]] BEFORE BOTH HIDDEN HOUSES. THE TRUCE HOLDS FOR ANOTHER YEAR."
  do take "A CUP OF [[=stag_h]]"
  do reward rich
  do mark exposed_an_envoy
  do remember giver "BOTH TABLES LEFT ME A GIFT. A WHITE FEATHER AND A GREY ONE. I DON'T TOUCH EITHER. <<relief>>"
  do fact "THE HIDDEN HOUSES OF [[=stag_h]] AND [[=moth_h]] KEEP THEIR TRUCE AT {HOME}. A STRANGER SAVED IT ONCE."

stage bargain_end
  end success
  say "THE ENVOY SMILED LIKE A KNIFE BEING SHEATHED. THE CUP WAS ON THE TABLE AT MOONRISE AND THE TRUCE WAS SWORN. YOU ARE OWED A FAVOUR BY A HOUSE OF THE HIDDEN FOLK. YOU SHOULD BE AFRAID OF THAT."
  journal "YOU KEPT THE ENVOY OF [[=stag_h]]'S SECRET FOR A FAVOUR. THE TRUCE HOLDS. SHE OWES YOU."
  do take "A CUP OF [[=stag_h]]"
  do reward fair
  do mark owed_by_the_stag
  do remember stag "MY FRIEND, THE MORTAL WHO KNOWS WHERE I KEEP MY GOWNS. CALL IN YOUR FAVOUR SOON. I GROW RESTLESS."

stage moth_end
  end success
  say "THE ENVOY OF [[=moth_h]] TOOK THE CUP, AND THANKED YOU, AND YOU REMEMBERED TOO LATE THAT YOU SHOULD NOT HAVE LET HIM. THE TRUCE HELD. THE HOUSE OF [[=stag_h]] HAS A NEW ENVOY. NOBODY SAYS WHAT HAPPENED TO THE OLD ONE."
  journal "YOU GAVE THE TRUCE-CUP TO [[=moth_h]]. THE TRUCE HOLDS. THE ENVOY OF [[=stag_h]] IS GONE."
  do take "A CUP OF [[=stag_h]]"
  do reward fair
  do mark thanked_by_the_moth
  do remember moth "YOU GAVE ME A JUSTICE I WILL DINE ON FOR A CENTURY. I AM IN YOUR DEBT. TRY NOT TO MAKE ME PAY IT."
  do fact "AN ENVOY OF THE HIDDEN HOUSE OF [[=stag_h]] VANISHED AFTER A MIDSUMMER AT {HOME}. THE HOUSE OF [[=moth_h]] SMILES ABOUT IT."
)SAGA";

const char* const kSkinBargain = R"SAGA(
title [[THE MARK ON THE WRIST|INKED IN SILVER|THE LAST SERVICE]]
hook npc any
pitch "[[pitch=WHAT'S THAT MARK ON YOUR FRIEND?|WHY DOES SHE HIDE HER WRIST?|IS THAT INK OR A BURN?]]"
hint "[[~pitch|SHE MADE A BARGAIN TO SAVE SOMEONE. NOW THE BARGAIN IS SAVING HER FOR ITSELF.|SHE MADE A BARGAIN TO SAVE SOMEONE. NOW THE BARGAIN IS SAVING HER FOR ITSELF.|IT GLOWS AT THE NEW MOON. LIKE A COAL UNDER THE SKIN. AND THEN MY FRIEND GOES AWAY FOR A WEEK.]]"
role giver giver
role home site home
role friend resident friend of giver
role hill site cave near
role binder person [[male|female]] at hill
slot t1 -> hill_go deceit rival wonder
slot t2 -> last mercy price identity

stage start
  talk giver
  say "{FRIEND} MADE A BARGAIN [[years=FIVE|SEVEN|NINE]] YEARS AGO WITH ONE OF THE HIDDEN FOLK: A LIFE SAVED, FOR ONE WEEK IN EVERY MONTH IN SERVICE. IT'S INKED ON {FRIEND.HIS} WRIST IN SILVER. NOW THE INK BURNS, AND THE BINDER CALLS IN A LAST SERVICE. <<plea:love>>"
  opt "WHAT IS THE LAST SERVICE?" -> service
  opt "I'LL GO TO THE BINDER." -> @t1
  opt "A BARGAIN'S A BARGAIN." -> refused

stage service
  talk friend
  say "{GIVER} TOLD YOU? ...THE BINDER WANTS ME TO BRING THEM [[task=THE NAME OF OUR PRIEST|A LOCK OF A SLEEPING CHILD'S HAIR|THE KEY TO THE TEMPLE]]. I WON'T. IF I REFUSE, THE MARK TAKES ALL MY WEEKS. ALL OF THEM. <<grief:fear>>"
  opt "THEN WE'LL READ THE BARGAIN AGAIN." -> @t1
  opt "DO IT. IT'S A SMALL THING." -> served_end

stage refused
  end fail
  journal "YOU WOULD NOT STAND BETWEEN {FRIEND} OF {HOME} AND THE BINDER'S MARK."
  do remember giver "{FRIEND} HAS BEEN GONE THREE WEEKS. THE INK HAD SPREAD TO {FRIEND.HIS} ELBOW BEFORE {FRIEND.HE} WENT."

stage served_end
  end fail
  say "{FRIEND} DID IT. THE INK FADED BY MORNING. {FRIEND.HE} HAS NOT LOOKED ANYONE IN THE EYE SINCE, AND THE TEMPLE BELL HAS A CRACK NOBODY CAN EXPLAIN."
  journal "YOU TOLD {FRIEND} TO PERFORM THE LAST SERVICE. THE BARGAIN IS ENDED. SOMETHING ELSE HAS BEGUN."
  do mark served_the_binder
  do remember friend "FREE. THAT'S WHAT IT'S CALLED. I CAN'T SLEEP IN THE DARK NOW. <<apology:shame>>"
  do fact "THE TEMPLE BELL OF {HOME} CRACKED ON A STILL NIGHT. SOME SAY A BARGAIN WAS KEPT."

stage hill_go
  goal goto hill
  then binder_talk
  journal "{FRIEND} OF {HOME} WEARS A BINDER'S MARK IN SILVER INK, AND THE LAST SERVICE IS DUE. SEEK THE BINDER AT {HILL}."

stage binder_talk
  talk binder
  say "{FRIEND}'S CHAMPION. YOU WANT TO READ THE BARGAIN? READ IT, THEN. EVERY WORD IS FAIR. I DID NOT HIDE THE PRICE. I ONLY DID NOT SAY IT ALOUD. THAT IS NOT A LIE, AMONG MY KIND. <<proverb>>"
  opt "THE MARK SAYS ONE WEEK. NOT ALL." -> letter
  opt "PUT THE MARK ON ME INSTEAD." -> @t2
  opt "WHY DO YOU NEED IT?" -> why

stage why
  talk binder
  say "...BECAUSE I AM BOUND TOO. MY OWN WRIST BURNS WHEN I FAIL MY QUEEN. YOU THOUGHT WE WERE FREE? NOBODY UNDER THE HILL IS FREE. WE ONLY HAVE BETTER MANNERS ABOUT IT."
  opt "THEN WE'LL CHEAT THEM BOTH." -> letter
  opt "PUT THE MARK ON ME INSTEAD." -> @t2

stage letter
  talk binder
  say "ONE WEEK IN EVERY MONTH, IT SAYS. AND A LAST SERVICE IS NOT A WEEK. ...CLEVER MORTAL. THE INK AGREES WITH YOU. THE BARGAIN ENDS AS IT WAS WRITTEN, AND I WILL ANSWER FOR IT. SMILE FOR ME. I WON'T BE SMILING LATER."
  opt "COME WITH ME, THEN. LEAVE THEM." -> fled_end
  opt "GO WELL." -> letter_end

stage last
  talk binder
  say "ON YOU? WILLINGLY? ...HOLD OUT YOUR WRIST. IT WILL ONLY BURN FOR A LIFETIME. NO, I JEST. ONE SERVICE, SOMEDAY, NOT TODAY. AND {FRIEND} IS FREE AT DAWN."
  opt "WRITE IT." -> marked_end

stage letter_end
  end success
  say "BY DAWN THE SILVER ON {FRIEND}'S WRIST WAS ONLY A PALE SCAR LIKE A RING. {FRIEND} SLEPT A WHOLE NIGHT FOR THE FIRST TIME IN [[=years]] YEARS."
  journal "YOU READ THE BINDER'S BARGAIN BY ITS LETTER. {FRIEND} OF {HOME} IS FREE."
  do reward fair
  do befriend friend
  do remember friend "A WHOLE NIGHT. YOU DON'T KNOW WHAT THAT IS. <<thanks:fear>>"
  do fact "{FRIEND} OF {HOME} WORE A BINDER'S SILVER MARK FOR [[=years]] YEARS. IT FADED TO A PALE RING."

stage fled_end
  end success
  say "THE BINDER LOOKED AT THE HILL A LONG TIME. THEN THEY TOOK YOUR HAND AND WALKED OUT INTO MORTAL DAYLIGHT, AND FLINCHED AT IT, AND KEPT WALKING. {FRIEND}'S MARK FADED BY DAWN."
  journal "YOU FREED {FRIEND} BY THE LETTER OF THE BARGAIN, AND THE BINDER FLED THE HILL WITH YOU."
  do reward fair
  do befriend friend
  do moves binder home
  do remember binder "THE SUN IS VERY LOUD HERE. I AM LEARNING TO BAKE. IT IS A KIND OF BARGAIN WITH FLOUR."
  do fact "A STRANGER WITH SILVER-INKED WRISTS LIVES IN {HOME} NOW, AND BAKES BADLY, AND FLINCHES AT BELLS."

stage marked_end
  end success
  say "THE INK RAN DOWN YOUR WRIST COLD AS RIVER WATER AND SETTLED INTO A THIN SILVER LINE. {FRIEND} WEPT. YOU WILL BE CALLED, SOMEDAY. NOT TODAY."
  journal "YOU TOOK {FRIEND}'S BARGAIN ONTO YOUR OWN SKIN. ONE SERVICE, SOMEDAY, IS OWED UNDER {HILL}."
  do reward rich
  do mark binder_mark
  do befriend friend
  do remember friend "YOUR WRIST. LET ME SEE IT. ...EVERY DAY, I'LL ASK TO SEE IT. EVERY DAY. <<vow:love>>"
)SAGA";

const char* const kEnemyFire = R"SAGA(
title [[THE ENEMY AT THE FIRE|TWO HOUSES, ONE ROAD|THE FEUD AND THE CAVE]]
hook npc any
pitch "[[pitch=WHO'S THAT YOU'RE GLARING AT?|WHY IS THERE A LINE IN THE DIRT?|YOUR FAMILIES DON'T SPEAK?]]"
hint "[[~pitch|MY GRANDFATHER KILLED HERS. HERS KILLED MY UNCLE. NOW WE'RE BOTH ROBBED BY THE SAME BANDITS. FUNNY.|THERE'S A LINE DOWN THE MIDDLE OF THIS TOWN. NOT ON ANY MAP. EVERYONE KNOWS WHERE IT IS.|MY GRANDFATHER KILLED HERS. HERS KILLED MY UNCLE. NOW WE'RE BOTH ROBBED BY THE SAME BANDITS. FUNNY.]]"
role giver giver
role home site home
role rival person [[male|female]] at home
role cave site cave near
role chief foe male at cave
var trust 0
slot t1 -> cave_go rival betrayal world
slot t2 -> after betrayal mercy price

stage start
  talk giver
  say "BANDITS TOOK MY FAMILY'S [[thing=SEAL RING|DEED BOX|MOTHER'S BONES]] AND {RIVAL}'S TOO, OUT OF BOTH HOUSES ON THE SAME NIGHT. THEY HOLE UP IN {CAVE}. OUR FAMILIES HAVE KILLED EACH OTHER FOR [[THREE|FOUR]] GENERATIONS. NOW {RIVAL} SAYS WE GO TOGETHER. <<doubt>>"
  opt "THEN GO TOGETHER. I'LL COME TOO." -> rival_talk
  opt "WHY DID THE FEUD START?" -> feud
  opt "THEN GO ALONE." -> refused

stage feud
  talk giver
  say "A WELL. A BOUNDARY STONE. A WEDDING THAT DIDN'T HAPPEN. EVERY GRANDMOTHER TELLS IT DIFFERENTLY. NOBODY ALIVE WAS THERE. <<proverb>>"
  opt "THEN GO TOGETHER. I'LL COME TOO." -> rival_talk

stage refused
  end fail
  journal "YOU WOULD NOT GO TO {CAVE} WITH {GIVER} AND {RIVAL}."
  do remember giver "{RIVAL} WENT ALONE AND CAME BACK BLOODY AND EMPTY-HANDED. NOW IT'S MY FAULT TOO. WONDERFUL."

stage rival_talk
  talk rival
  say "SO {GIVER} BROUGHT A HIRED SWORD. FINE. I'LL WATCH YOUR BACK BECAUSE I NEED IT WHOLE, NOT BECAUSE I LIKE IT. <<threat:pride>> WE GO AT DUSK."
  opt "WE GO AT DUSK, THEN." -> @t1

stage cave_go
  goal kill 3 bandit in cave
  then fire
  journal "{GIVER} AND {RIVAL}, ENEMIES OF OLD, HUNT THE BANDITS OF {CAVE} TOGETHER. YOU GO WITH THEM."

stage fire
  talk rival
  say "THE BANDITS ARE DONE, AND THE CHIEF HAS RUN DEEPER IN. WE'LL CAMP HERE. ...{GIVER} TOOK A BLADE MEANT FOR ME TONIGHT. WHY? I'D HAVE LET IT LAND ON {GIVER.HIM}. I THINK I WOULD HAVE."
  opt "ASK {GIVER.HIM}. NOT ME." -> ask
  opt "TELL {GIVER.HIM} WHAT YOU JUST TOLD ME." -> ask
  opt "SLEEP. WE HUNT AT DAWN." -> hunt

stage ask
  talk giver
  say "WHY? I DON'T KNOW. MY ARM MOVED. ...MY GRANDMOTHER SAID {RIVAL}'S PEOPLE HAVE NO HEARTS. I'VE BEEN WATCHING. {RIVAL} SHARES WATER FIRST AND SLEEPS LAST. <<apology:shame>>"
  do set trust 1
  opt "SAY THAT TO {RIVAL}." -> hunt

stage hunt
  goal slay chief
  then @t2
  journal "THE BANDIT CHIEF OF {CAVE} HOLDS WHAT WAS STOLEN FROM BOTH HOUSES. {GIVER} AND {RIVAL} HUNT HIM WITH YOU."

stage after
  talk rival
  say "IT'S OVER. HERE: BOTH HOUSES' THINGS, IN ONE SACK. I COULD TAKE THE SACK AND RUN AND GO HOME A HERO. {GIVER} WOULD NEVER CATCH ME. WHAT DO YOU THINK I SHOULD DO, HIRED SWORD?"
  opt "SPLIT IT, AND WALK HOME TOGETHER." -> together_end
  opt "WALK HOME SIDE BY SIDE. SAY NOTHING." if var trust 1 -> bond_end
  opt "RUN, THEN. SEE WHAT IT BUYS YOU." -> run_end

stage together_end
  end success
  say "THEY WALKED INTO {HOME} TOGETHER IN FULL DAYLIGHT, ACROSS THE LINE NOBODY DRAWS, AND BOTH HOUSES CAME OUT TO STARE. NOBODY THREW ANYTHING. IT'S A START."
  journal "{GIVER} AND {RIVAL} CAME HOME TOGETHER WITH WHAT WAS STOLEN. THE FEUD IS... QUIETER."
  do reward fair
  do remember giver "{RIVAL} NODDED TO ME IN THE MARKET. I NODDED BACK. MY AUNT FAINTED. <<relief>>"
  do fact "THE FEUDING HOUSES OF {HOME} HUNTED BANDITS TOGETHER ONCE. THE LINE IN THE DIRT IS FADING."

stage bond_end
  end success
  say "THEY WALKED BACK SIDE BY SIDE AND SAID NOTHING FOR A DAY'S ROAD. AT THE EDGE OF {HOME}, {RIVAL} TOOK {GIVER}'S HAND, ONCE, AND LET GO. NOBODY SAW. YOU DID."
  journal "{GIVER} AND {RIVAL}, ENEMIES BY BLOOD, CAME HOME SOMETHING ELSE. NOBODY KNOWS WHAT YET."
  do reward rich
  do remember rival "DON'T LOOK AT ME LIKE THAT. NOTHING HAPPENED. ...ASK ME AGAIN AT MIDSUMMER. <<oath>>"
  do fact "THE HEIRS OF TWO FEUDING HOUSES OF {HOME} ARE SEEN WALKING THE SAME ROAD AT DUSK. THEIR GRANDMOTHERS ARE FURIOUS."

stage run_end
  end success
  say "{RIVAL} LOOKED AT THE SACK, AND AT {GIVER}, AND AT YOU. THEN {RIVAL.HE} PUT IT DOWN AND WALKED HOME ALONE WITHOUT IT. {GIVER} CARRIED BOTH HOUSES' THINGS HOME, AND DIDN'T KNOW WHAT TO DO WITH THEM."
  journal "YOU DARED {RIVAL} TO RUN WITH BOTH HOUSES' THINGS. {RIVAL.HE} WOULDN'T. THE FEUD IS NOT OVER."
  do reward small
  do remember rival "YOU THOUGHT I'D RUN. EVERYONE THINKS THAT OF MY PEOPLE. I THOUGHT YOU WERE DIFFERENT. <<insult>>"
)SAGA";

const char* const kCrownPrice = R"SAGA(
title [[THE CROWN'S PRICE|THE LAST OF THE HOUSE|A QUEEN IN A BAKER'S APRON]]
hook npc any
pitch "[[pitch=WHY ARE YOU BOWING TO THE BAKER?|WHO ARE THE OLD MEN IN GREY?|WHAT'S IN THAT BUNDLE?]]"
hint "[[~pitch|THERE IS A HOUSE THAT FELL BEFORE YOU WERE BORN, STRANGER. ITS LAST CHILD SELLS LOAVES THREE STREETS FROM HERE.|I SWORE AN OATH TO A FAMILY THAT NO LONGER EXISTS. EXCEPT THAT IT DOES.|I SWORE AN OATH TO A FAMILY THAT NO LONGER EXISTS. EXCEPT THAT IT DOES.]]"
role giver giver
role home site home
role heir resident any
role love resident friend of heir
role ruin ruin near
var claimed 0
slot t1 -> ruin_go deceit rival prophecy
slot t2 -> choose price betrayal mercy

stage start
  talk giver
  say "I SERVED THE HOUSE OF {RUIN.LORD} AT {RUIN} AS A BOY, BEFORE IT FELL. NOW I HAVE PROOF THAT {HEIR}, A {HEIR.JOB} THREE STREETS FROM HERE, IS THE LAST OF ITS BLOOD. THE OLD OATH-KEEPERS WANT {HEIR.HIM} CROWNED. <<vow:devotion>>"
  opt "DOES {HEIR} WANT A CROWN?" -> heir_talk
  opt "WHAT'S THE PROOF?" -> proof
  opt "LET THE DEAD HOUSE LIE." -> refused

stage proof
  talk giver
  say "A RING FOUND IN {HEIR.HIS} MOTHER'S BED-STRAW, CUT WITH THE ARMS OF {RUIN.LORD}. AND THE OLD RECORD, IN THE VAULT AT {RUIN}, THAT NAMES THE CHILD SMUGGLED OUT THE NIGHT IT FELL. <<oath>>"
  opt "LET ME TALK TO {HEIR}." -> heir_talk
  opt "I'LL FETCH THE RECORD." -> @t1

stage refused
  end fail
  journal "YOU TOLD {GIVER} TO LET THE FALLEN HOUSE OF {RUIN.LORD} LIE."
  do remember giver "THE OATH-KEEPERS WENT TO {HEIR} WITHOUT YOU. IT WENT BADLY. NOBODY IS SPEAKING."

stage heir_talk
  talk heir
  say "A CROWN? I'M A {HEIR.JOB}. I'M GOOD AT IT. I HAVE {LOVE}, AND A ROOF, AND BREAD ON TIME. ...BUT AT NIGHT I DREAM OF A HALL I'VE NEVER SEEN, AND KNOW THE WAY TO EVERY ROOM. <<doubt>>"
  opt "THEN SEE THE RECORD FIRST." -> @t1
  opt "KEEP YOUR ROOF. BURN THE RING." -> hidden_end

stage ruin_go
  goal fetch "THE RECORD OF {RUIN.LORD}" in ruin
  then @t2
  journal "IN THE VAULT OF {RUIN} LIES THE RECORD THAT NAMES {HEIR} OF {HOME} AS THE LAST OF THE HOUSE OF {RUIN.LORD}. FETCH IT."

stage choose
  talk heir
  say "IT'S TRUE, THEN. AND THE OATH-KEEPERS SAY A CROWNED HEAD CANNOT WED A {LOVE.JOB}. THEY SAY IT KINDLY. THEY SAY IT AGAIN AND AGAIN. TELL ME WHAT TO DO. NO. DON'T. ...TELL ME."
  opt "TAKE THE CROWN. ON YOUR OWN TERMS." -> crowned_end
  opt "THE CROWN COSTS {LOVE}. REFUSE IT." -> refused_crown_end
  opt "BURN THE RECORD. BE NOBODY." -> burned_end

stage hidden_end
  end success
  say "{HEIR} DROPPED THE RING IN THE OVEN WITH THE MORNING'S BREAD. THE OATH-KEEPERS WENT HOME. {GIVER} DID NOT SPEAK TO YOU FOR A MONTH, AND THEN BOUGHT YOU A DRINK."
  journal "{HEIR} OF {HOME} BURNED THE RING OF {RUIN.LORD} AND STAYS A {HEIR.JOB}."
  do reward small
  do remember giver "I SWORE TO A HOUSE. THE HOUSE SAID NO THANK YOU. I SUPPOSE THAT'S ITS RIGHT. <<proverb>>"

stage crowned_end
  end success
  say "{HEIR} TOOK THE RING AND THE NAME, AND THEN TOOK {LOVE}'S HAND IN FRONT OF EVERY OATH-KEEPER AND SAID: THESE ARE MY TERMS. NOBODY DARED ARGUE WITH THAT FACE. IT WAS {RUIN.LORD}'S FACE."
  journal "{HEIR} OF {HOME} CLAIMED THE NAME OF {RUIN.LORD}, ON {HEIR.HIS} OWN TERMS, WITH {LOVE} AT {HEIR.HIS} SIDE."
  do take "THE RECORD OF {RUIN.LORD}"
  do reward rich
  do befriend heir
  do mark crowned_the_heir
  do fact "THE FALLEN HOUSE OF {RUIN.LORD} HAS AN HEIR AGAIN, A {HEIR.JOB} OF {HOME}, WHO SWORE ON A LOAF AND A RING."

stage refused_crown_end
  end success
  say "{HEIR} READ THE RECORD TWICE, KISSED IT, AND GAVE IT TO {GIVER}. KEEP IT, {HEIR.HE} SAID. IF THE WORLD EVER NEEDS THIS HOUSE, IT KNOWS WHERE I BAKE."
  journal "{HEIR} OF {HOME} KNOWS {HEIR.HE} IS THE LAST OF {RUIN.LORD}, AND CHOSE {LOVE} OVER THE CROWN."
  do take "THE RECORD OF {RUIN.LORD}"
  do reward fair
  do befriend heir
  do remember heir "SOMETIMES I WALK TO {RUIN} AND KNOW EVERY ROOM. THEN I COME HOME. HOME IS THE BETTER ROOM. <<blessing>>"

stage burned_end
  end success
  say "THE OLD PARCHMENT WENT UP LIKE DRY STRAW. {GIVER} WATCHED IT BURN AND SAID NOTHING, AND THEN KNELT TO {HEIR} ANYWAY, ONCE, IN THE ASH, WHERE NOBODY ELSE COULD SEE."
  journal "THE RECORD OF {RUIN.LORD} IS ASH. NOBODY CAN PROVE WHO {HEIR} OF {HOME} IS. {GIVER} KNOWS."
  do take "THE RECORD OF {RUIN.LORD}"
  do reward fair
  do remember giver "I STILL BOW WHEN {HEIR} PASSES. {HEIR.HE} HATES IT. I CAN'T HELP IT. <<oath>>"
  do fact "SOMEONE IN {HOME} IS SAID TO BE THE LAST OF {RUIN.LORD}. THE PROOF WAS BURNED. THE OLD SERVANT STILL BOWS."
)SAGA";

const char* const kBoundLord = R"SAGA(
title [[THE LORD WHO CANNOT LEAVE|THE CURSE OF THE WALLS|WHAT LOVE COSTS]]
hook npc any
pitch "[[pitch=WHERE DOES YOUR KIN GO AT NIGHT?|YOU LOOK WORRIED SICK.|WHY ARE THERE ROSES ON YOUR SILL?]]"
hint "[[~pitch|THEY SAY THERE'S A MAN IN THE RUIN WHO HASN'T AGED IN A HUNDRED YEARS. THEY SAY IT LIKE IT'S ROMANTIC.|THEY SAY THERE'S A MAN IN THE RUIN WHO HASN'T AGED IN A HUNDRED YEARS. THEY SAY IT LIKE IT'S ROMANTIC.|EVERY NIGHT TO THE OLD RUIN, AND HOME BEFORE DAWN WITH ROSES THAT DON'T GROW IN THIS SOIL. I'M NOT A FOOL.]]"
role giver giver
role home site home
role kin resident kin of giver
role ruin ruin near
role lord person male at ruin
slot t1 -> ruin_go wonder deceit identity
slot t2 -> breaking price mercy betrayal

stage start
  talk giver
  say "MY {KIN.KIN} {KIN} GOES TO {RUIN} EVERY NIGHT, TO A MAN WHO CANNOT LEAVE ITS WALLS. A CURSE FROM THE DAYS OF {RUIN.LORD}, {KIN.HE} SAYS, THAT ONLY LOVE OR SACRIFICE BREAKS. I FEAR THE SACRIFICE. <<plea:fear>>"
  opt "I'LL MEET THIS LORD." -> @t1
  opt "LET ME TALK TO {KIN} FIRST." -> kin_talk
  opt "LOVE IS {KIN.HIS} BUSINESS." -> refused

stage kin_talk
  talk kin
  say "HE'S NOT A MONSTER. HE'S LONELY AND FUNNY AND VERY, VERY OLD, AND HE WON'T LET ME TRY TO BREAK IT. HE SAYS THE LAST ONE WHO TRIED IS STILL IN THE WALLS. <<grief:love>>"
  opt "THEN I'LL MEET HIM." -> @t1

stage refused
  end fail
  journal "YOU LEFT {KIN} OF {HOME} TO WALK TO {RUIN} EVERY NIGHT."
  do remember giver "{KIN} DIDN'T COME HOME LAST NIGHT. THE ROSES ON THE SILL ARE BLACK THIS MORNING."

stage ruin_go
  goal goto ruin
  then lord_talk
  journal "A CURSED LORD IN {RUIN} CANNOT LEAVE ITS WALLS. {KIN} OF {HOME} LOVES HIM. MEET HIM."

stage lord_talk
  talk lord
  say "YOU'RE THE ONE {KIN} TALKS ABOUT. HE'S AFRAID FOR {KIN.HIM}, YOUR {GIVER}. SO AM I. THE CURSE BREAKS IF SOMEONE GIVES UP SOMETHING TRUE FOR ME, FREELY. THE LAST ONE GAVE HER LIFE. I WILL NOT HAVE IT AGAIN."
  opt "WHAT IS THE CURSE'S HEART?" -> heart
  opt "I'LL GIVE SOMETHING. NOT {KIN}." -> @t2
  opt "THEN NOBODY BREAKS IT." -> stay_end

stage heart
  talk lord
  say "A STONE IN THE OLD HALL'S HEARTH, WHERE {RUIN.LORD} SPOKE THE WORDS. SMASH IT AND THE CURSE ENDS, AND SO DO I: I AM ONLY HELD TOGETHER BY IT. {KIN} DOES NOT KNOW THAT. DON'T TELL {KIN.HIM}."
  opt "THEN I WON'T SMASH IT." -> @t2
  opt "IT'S YOU OR {KIN}. I'LL SMASH IT." -> smashed_end

stage breaking
  talk kin
  say "YOU'RE HERE. HE TOLD ME TO GO HOME. I WON'T. IF SOMETHING TRUE IS THE PRICE, IT'S MINE TO PAY: I'LL STAY HERE, WITH HIM, AND NEVER GO HOME AGAIN. THAT'S TRUE ENOUGH. ISN'T IT?"
  opt "IT IS. STAY, IF YOU CHOOSE." -> stayed_end
  opt "LET ME PAY INSTEAD." -> paid_end

stage stay_end
  end fail
  say "HE THANKED YOU FOR UNDERSTANDING, AND WALKED YOU TO THE EDGE OF THE WALLS, WHERE HE HAD TO STOP. {KIN} WAS ALREADY WALKING UP THE HILL TOWARD HIM."
  journal "NOBODY BROKE THE CURSE OF {RUIN}. {KIN} OF {HOME} STILL WALKS THERE EVERY NIGHT."
  do remember giver "NOTHING CHANGED. {KIN} STILL GOES. {KIN.HE} LOOKS HAPPY AND THIN. I DON'T KNOW WHICH FRIGHTENS ME MORE."

stage smashed_end
  end success
  say "THE HEARTHSTONE SPLIT LIKE ICE. HE SMILED AT YOU, AS IF YOU HAD DONE HIM A KINDNESS, AND THEN HE WAS DUST AND OLD ROSES. {KIN} FOUND THE ROSES IN THE MORNING."
  journal "YOU BROKE THE CURSE OF {RUIN}, AND THE LORD BOUND BY IT WITH IT. {KIN} KNOWS WHO DID IT."
  do reward fair
  do mark broke_the_walls
  do remember kin "YOU. HE ASKED YOU TO, DIDN'T HE? HE WOULD. ...I CAN'T FORGIVE YOU YET. ASK ME IN A YEAR."
  do fact "THE CURSE OF {RUIN} IS BROKEN. ROSES GROW THERE THAT DON'T GROW ANYWHERE ELSE."

stage stayed_end
  end success
  say "{KIN} STAYED. THE CURSE BROKE AT THE WORD, BECAUSE IT WAS TRUE, AND THE LORD WALKED OUT OF THE GATE FOR THE FIRST TIME IN A HUNDRED YEARS AND THEN WALKED STRAIGHT BACK IN. THEY LIVE THERE NOW. THEY MEND THE ROOF."
  journal "{KIN} OF {HOME} CHOSE TO STAY IN {RUIN} WITH THE LORD. THE CURSE BROKE. THEY STAYED ANYWAY."
  do reward rich
  do moves kin ruin
  do remember giver "{KIN} VISITS ON FEAST DAYS, WITH HIM. HE BRINGS ROSES. HE'S ACTUALLY VERY FUNNY. <<relief>>"
  do fact "A CURSED LORD OF {RUIN} WAS FREED BY SOMEONE WHO CHOSE TO STAY. THEY ARE MENDING THE OLD ROOF TOGETHER."

stage paid_end
  end success
  say "YOU GAVE THE WALLS [[YOUR BEST YEAR|A MEMORY YOU LOVED|YOUR LUCK]], AND THEY TOOK IT. THE CURSE BROKE. {KIN} AND THE LORD WALKED DOWN TO {HOME} TOGETHER AT DAWN, AND YOU WALKED BEHIND, LIGHTER BY SOMETHING."
  journal "YOU PAID THE PRICE OF THE CURSE OF {RUIN} YOURSELF. THE LORD IS FREE, AND {KIN} WITH HIM."
  do reward rich
  do mark paid_the_walls
  do fame 1
  do remember kin "YOU GAVE THEM SOMETHING OF YOURS. FOR US. I WILL NEVER UNDERSTAND YOU. <<thanks:love>>"
)SAGA";

const char* const kMountainTrials = R"SAGA(
title [[THE TRIALS UNDER THE HILL|THREE TRIALS AND A QUEEN|THE DEEP COURT'S GUEST]]
hook npc any
pitch "[[pitch=WHERE IS YOUR SWEETHEART?|WHY ARE YOU BANDAGED?|WHAT HAPPENED UNDER THE HILL?]]"
hint "[[~pitch|THE QUEEN UNDER THE HILL IS VERY BEAUTIFUL AND VERY BORED, AND BOREDOM IS THE CRUELLEST THING I KNOW.|I FAILED THE FIRST TRIAL. THE FIRST. THERE ARE THREE.|THE QUEEN UNDER THE HILL IS VERY BEAUTIFUL AND VERY BORED, AND BOREDOM IS THE CRUELLEST THING I KNOW.]]"
role giver giver
role home site home
role kin resident kin of giver
role cave site cave near
role queen person female at cave
role prisoner person [[male|female]] at cave
var spared 0
slot t1 -> trial_one rival price world
slot t2 -> offer deceit mercy wonder

stage start
  talk giver
  say "THE QUEEN UNDER THE HILL AT {CAVE} TOOK MY {KIN.KIN} {KIN} AT THE DANCE ON MIDSUMMER NIGHT. ANYONE MAY WIN {KIN.HIM} BACK WITH THREE TRIALS. I FAILED THE FIRST AND CRAWLED HOME. <<plea>>"
  opt "I'LL TAKE THE TRIALS." -> @t1
  opt "WHAT ARE THE TRIALS?" -> what
  opt "NOBODY WINS AGAINST A QUEEN." -> refused

stage what
  talk giver
  say "A BEAST IN THE DARK. A RIDDLE AT HER FEET. AND THE LAST NOBODY WILL SPEAK OF, BECAUSE NOBODY HAS REACHED IT. <<warning:love>>"
  opt "THEN I'LL REACH IT." -> @t1

stage refused
  end fail
  journal "YOU WOULD NOT TAKE THE TRIALS UNDER {CAVE} FOR {GIVER}'S {KIN.KIN}."
  do remember giver "I HEAR {KIN} LAUGHING UNDER THE HILL SOME NIGHTS. IT'S NOT {KIN.HIS} LAUGH ANY MORE."

stage trial_one
  goal kill 3 beast in cave
  then riddle
  journal "THE FIRST TRIAL UNDER {CAVE}: THE BEASTS IN THE DARK. WIN {KIN} BACK FOR {GIVER}."

stage riddle
  talk queen
  say "THE BEASTS ARE DEAD. HOW TIRESOME. THE SECOND TRIAL, THEN: WHAT DO I HAVE THAT YOU WILL NEVER HAVE, THAT YOU CAN GIVE ME, THAT I CAN NEVER KEEP?"
  opt "AN ENDING." -> mercy_trial
  opt "A NAME." -> wrong
  opt "TIME." -> wrong

stage wrong
  talk queen
  say "NO. BUT YOU ANSWERED QUICKLY, AND THAT AMUSED ME, AND I AM SO RARELY AMUSED. ONE MORE TRY. <<insult>>"
  opt "AN ENDING." -> mercy_trial

stage mercy_trial
  talk prisoner
  say "THE QUEEN SAYS THE THIRD TRIAL IS ME. I TRIED TO WIN MY OWN SWEETHEART BACK, LONG AGO, AND FAILED. SHE SAYS YOU MAY STAB ME, AND WIN, OR FREE ME, AND LOSE. SHE LIKES THIS TRIAL BEST."
  opt "I WON'T STAB YOU." -> freed
  opt "...FORGIVE ME." -> stabbed

stage freed
  talk queen
  say "YOU REFUSED. EVERY MORTAL BEFORE YOU STABBED. ...OH, THE THIRD TRIAL WAS NEVER THE STABBING. IT WAS THE REFUSING. TAKE YOUR {KIN}. TAKE THAT ONE TOO. AND NOW I MAKE YOU AN OFFER."
  do set spared 1
  opt "WHAT OFFER?" -> @t2

stage stabbed
  talk queen
  say "YOU STABBED. THEY ALL STAB. YOU FAILED THE THIRD TRIAL, MORTAL, BUT YOU AMUSED ME. I WILL MAKE YOU AN OFFER ANYWAY."
  opt "WHAT OFFER?" -> @t2

stage offer
  talk queen
  say "STAY. BE MY CHAMPION UNDER THE HILL, AND {KIN} GOES HOME FREE, WIN OR LOSE. NO MORTAL WAR, NO MORTAL HUNGER, ONLY DANCING AND RIDDLES FOR A THOUSAND YEARS. I SO RARELY ASK."
  opt "NO. {KIN} AND I GO HOME." if var spared 1 -> won_end
  opt "NO. I LOST. LET ME GO." if novar spared 1 -> lost_end
  opt "ONE YEAR. NOT A THOUSAND." -> year_end

stage won_end
  end success
  say "YOU WALKED OUT INTO MORNING WITH {KIN} ON ONE ARM AND THE OLD PRISONER ON THE OTHER, BLINKING. THE HILL CLOSED BEHIND YOU LIKE A MOUTH THAT HAS DECIDED TO SMILE."
  journal "YOU PASSED THE TRIALS UNDER {CAVE}. {KIN} IS HOME WITH {GIVER}, AND {PRISONER} IS FREE."
  do reward rich
  do befriend kin
  do moves prisoner home
  do remember giver "BOTH OF THEM AT MY TABLE. THE OLD ONE CRIES AT BREAD. <<thanks>>"
  do fact "SOMEONE PASSED THE TRIALS UNDER {CAVE} BY REFUSING TO KILL. THE QUEEN UNDER THE HILL TALKS OF NOTHING ELSE."

stage lost_end
  end fail
  say "THE QUEEN LET YOU GO, AS SHE PROMISED, AND KEPT {KIN} AND THE BLOOD ON THE FLOOR, AS SHE ALWAYS DOES."
  journal "YOU FAILED THE THIRD TRIAL UNDER {CAVE}. {KIN} STAYS WITH THE QUEEN."
  do remember giver "YOU CAME BACK ALONE. YOU CAME BACK WITH BLOOD ON YOU. <<grief>>"

stage year_end
  end success
  say "ONE YEAR. SHE LAUGHED, AND AGREED, AND {KIN} WOKE IN {GIVER}'S BED THE NEXT MORNING. YOU WOKE IN THE DARK, IN A CHAIR, WITH A CROWN OF BIRCH ON YOUR HEAD. THEN SHE LET YOU GO. A YEAR UNDER THE HILL IS A NIGHT ABOVE."
  journal "YOU BARGAINED A YEAR UNDER {CAVE} FOR {KIN}'S FREEDOM. IT PASSED IN A NIGHT. YOU REMEMBER ALL OF IT."
  do reward fair
  do mark a_year_under_the_hill
  do befriend kin
  do remember kin "YOU WERE GONE ONE NIGHT. YOU HAVE A YEAR IN YOUR EYES. I SAW IT. <<comfort:love>>"
)SAGA";

void add(std::vector<Archetype>& v, const char* id, const char* name, uint32_t themes, uint32_t needs, uint16_t motives,
         uint32_t twists, const char* body) {
  Archetype a;
  a.id = id;
  a.name = name;
  a.source = Source::Maas;
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

void addMaas(std::vector<Archetype>& v) {
  using M = Motive;
  add(v, "rival_houses", "THE TRUCE AT THE MILL", TH_BETRAYAL | TH_WONDER | TH_JUDGEMENT | TH_TRICKERY | TH_WAR, N_CAVE,
      mb(M::Fear, M::Duty, M::Hope), TF_DECEIT | TF_RIVAL | TF_BETRAYAL | TF_MERCY | TF_PRICE | TF_WONDER, kRivalHouses);
  add(v, "skin_bargain", "THE MARK ON THE WRIST", TH_BARGAIN | TH_SACRIFICE | TH_FRIENDSHIP | TH_FREEDOM | TH_TRICKERY,
      N_CAVE | N_FRIENDS, mb(M::Love, M::Fear, M::Duty, M::Hope), TF_DECEIT | TF_RIVAL | TF_WONDER | TF_MERCY | TF_PRICE | TF_IDENTITY,
      kSkinBargain);
  add(v, "enemy_fire", "THE ENEMY AT THE FIRE", TH_LOVE | TH_FRIENDSHIP | TH_PRIDE | TH_COURAGE | TH_REDEMPTION, N_CAVE,
      mb(M::Pride, M::Vengeance, M::Duty, M::Shame), TF_RIVAL | TF_BETRAYAL | TF_WORLD | TF_MERCY | TF_PRICE, kEnemyFire);
  add(v, "crown_price", "THE CROWN'S PRICE", TH_KINGSHIP | TH_LOVE | TH_SACRIFICE | TH_LOYALTY | TH_PROPHECY,
      N_RUIN | N_FALLEN | N_FRIENDS, mb(M::Devotion, M::Duty, M::Hope, M::Pride),
      TF_DECEIT | TF_RIVAL | TF_PROPHECY | TF_PRICE | TF_BETRAYAL | TF_MERCY, kCrownPrice);
  add(v, "bound_lord", "THE LORD WHO CANNOT LEAVE", TH_CURSE | TH_LOVE | TH_SACRIFICE | TH_FREEDOM | TH_WONDER, N_RUIN,
      mb(M::Fear, M::Love, M::Grief), TF_WONDER | TF_DECEIT | TF_IDENTITY | TF_PRICE | TF_MERCY | TF_BETRAYAL, kBoundLord);
  add(v, "mountain_trials", "THE TRIALS UNDER THE HILL", TH_COURAGE | TH_MERCY | TH_TEMPTATION | TH_LOVE | TH_WONDER, N_CAVE,
      mb(M::Love, M::Shame, M::Grief, M::Hope), TF_RIVAL | TF_PRICE | TF_WORLD | TF_DECEIT | TF_MERCY | TF_WONDER, kMountainTrials);
}

}  // namespace arch
}  // namespace saga
}  // namespace story
