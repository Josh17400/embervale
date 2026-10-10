// M6b "Sagas": THE COURT UNDER THE HILL (campaign `fae`): the fae-court intrigue. CAMPAIGNS lane. Folk-tale fae (the
// hollow hill, food that binds, names that bind, the changeling) with Maas-like THEMES only (rival houses of an
// immortal court, a bargain marked on the skin, enemies to allies, a hidden queen and the price of a crown, a trial
// under the mountain): no names, places, terms or plot of any modern work.
//
// Hook: a notice board. People have gone missing from the town since midsummer; one who pinned the notice (a real
// resident) lost someone dear. The way in is a hollow hill; the old stones of a ruin are its door. Under the hill two
// houses of the court contend for an empty throne: the lady of the holly, who hates mortals and is the throne's hidden
// heir, and the prince of the rowan house, all charm, who has been taking mortals to pay for his claim.
//   head      the one who pinned the notice; the missing one
//   arc 1     THE WAY IN               the revel at the stones (eat nothing) | the door that asks a name
//   arc 2     THE HOUSES               the holly lady's bargain (a mark on the skin) | the prince's courtesy
//   arc 3     THE TRIAL                three tasks under the hill | the hunt by moonlight
//   arc 4     THE HIDDEN QUEEN         the lady's true name | the changeling in the cradle
//   finale    the empty throne: crown the lady (the taken come home; she ends a mortal war as her price) | crown the
//             prince (the taken stay; the town's fields fail) | iron and salt (the hill sealed; its craft saved) |
//             stay under the hill yourself (a failure)
// Recurring: the poster, the taken one, the lady, the prince; the town; the land and its rival.
#include <vector>
#include "rpg/story/saga.h"

namespace story {
namespace saga {
namespace camp {

namespace {

const char* const kHead = R"SAGA(
title THE COURT UNDER THE HILL
hook board
pitch "MISSING SINCE MIDSUMMER. ASK WITHIN."
hint "MISSING SINCE MIDSUMMER: SIX SOULS. ANYONE WHO HAS SEEN LIGHTS ON THE HILL AT NIGHT, ASK WITHIN."
role giver giver
role home site home
role land kingdom home
role rival kingdom rival
role poster resident any
role hill site cave near
role stones ruin near
role taken person [[female|male]] at hill
role lady person female at hill
role prince person male at hill
var bargain 0
var holly 0
var names 0

stage notice
  talk poster
  say "YOU READ IT? I PINNED IT. NOBODY ELSE WOULD. SIX GONE SINCE MIDSUMMER, AND MY [[rel=SISTER'S CHILD|APPRENTICE|BETROTHED]] {TAKEN} THE LAST, FOUR NIGHTS AGO. THE ELDERS SAY WOLVES. WOLVES DON'T LEAVE A RING OF MUSHROOMS ROUND THE BED."
  journal "SIX HAVE GONE FROM {HOME} SINCE MIDSUMMER. {POSTER}, THE {POSTER.JOB}, WANTS {TAKEN} BACK."
  opt "TELL ME ABOUT THE LIGHTS." -> lights
  opt "WHERE WAS {TAKEN} LAST SEEN?" -> lights
  opt "WOLVES, MOST LIKELY." -> wolves

stage lights
  talk poster
  say "ON THE HILL AT {HILL}, {HILL.DIR}. MUSIC, TOO, LIKE A FIDDLE PLAYED UNDER WATER. MY GRANDMOTHER SAID THE OLD STONES OF {STONES} WERE A DOOR, AND YOU KEEP IRON IN YOUR POCKET WHEN YOU PASS THEM. I LAUGHED AT HER. <<plea>>"
  opt "I'LL GO TO THE HILL." -> setout
  opt "WHAT DID {TAKEN} LOVE MOST?" -> loved

stage loved
  talk poster
  say "SINGING. {TAKEN} SANG AT THE WASH-TUBS, AT THE WELL, IN {TAKEN.HIS} SLEEP. WHY? ...OH. YOU THINK THAT'S WHY THEY TOOK {TAKEN.HIM}. BECAUSE THEY WANTED THE SINGING. DON'T BRING ME BACK SOMETHING THAT ONLY SINGS."
  opt "I'LL BRING {TAKEN.HIM} BACK WHOLE." -> setout

stage wolves
  end fail
  say "{POSTER} TAKES THE NOTICE DOWN WHILE YOU WATCH, FOLDS IT VERY SMALL, AND PUTS IT IN {POSTER.HIS} APRON. THAT NIGHT THERE ARE LIGHTS ON THE HILL AGAIN, AND MUSIC."
  journal "YOU SAID WOLVES. {POSTER} OF {HOME} TOOK DOWN THE NOTICE."
  do remember poster "WOLVES, YOU SAID. A SEVENTH WENT LAST NIGHT. WOLVES."

stage setout
  talk poster
  say "TAKE THIS: MY GRANDMOTHER'S NAIL. SHE SAID THE HILL HAS HAD NO QUEEN FOR [[age=FOUR HUNDRED|SIX HUNDRED|NINE HUNDRED]] YEARS. EAT NOTHING THEY OFFER. DRINK NOTHING. GIVE NO NAME, NOT EVEN A FALSE ONE: THEY CAN TELL." AND
  do give "AN OLD IRON NAIL" key
  opt "NOTHING EATEN, NO NAME GIVEN." -> @next
  opt "[[I'LL KEEP IT CLOSE.|IRON. I'LL REMEMBER.]]" -> @next
)SAGA";

const char* const kFinale = R"SAGA(
stage throne
  talk lady
  do moves prince hill
  do moves lady hill
  ?arc_fa_true_name say "THE THRONE UNDER THE HILL HAS STOOD EMPTY [[=age]] YEARS. YOU KNOW WHAT I AM NOW, MORTAL. SO DOES HE. THE COURT WILL CROWN WHOEVER THE GUEST NAMES: THAT IS THE OLDEST LAW WE HAVE. NAME, THEN."
  ?arc_fa_changeling say "THE THRONE UNDER THE HILL HAS STOOD EMPTY [[=age]] YEARS. THE COURT WILL CROWN WHOEVER THE GUEST NAMES: THE OLDEST LAW WE HAVE, AND YOU ARE OUR ONLY GUEST WHO IS NOT ALSO A PRISONER. NAME, THEN."
  opt "THE LADY OF THE HOLLY." -> crown_lady
  opt "THE ROWAN PRINCE." -> crown_prince
  opt "NO ONE. IRON AND SALT ON THE DOOR." -> sealed
  opt "LET ME STAY HERE. FOREVER." -> stayed

stage crown_lady
  talk lady
  say "...ME. YOU NAME ME, AFTER I SET THE HOUNDS ON YOU. MORTALS ARE STRANGER THAN SONGS. VERY WELL: YOUR SIX GO HOME AT DAWN WITH NO DAY LOST. AND I PAY A QUEEN'S PRICE. {LAND} AND {RIVAL} WILL FIND THEY NO LONGER WISH TO FIGHT."
  opt "WHAT DOES THAT COST YOU?" -> lady_cost
  opt "THANK YOU, MAJESTY." -> lady_end

stage lady_cost
  talk lady
  say "MY HATRED OF YOU, MOSTLY. I HAD BEEN SAVING IT FOR A THOUSAND YEARS. ...DON'T SMILE. IT IS VERY UNBECOMING IN A MORTAL. GO HOME, {PLAYER}. COME BACK AT MIDSUMMER. I WILL PRETEND I DID NOT ASK."
  opt "AT MIDSUMMER, THEN." -> lady_end

stage lady_end
  end success
  say "AT DAWN SIX PEOPLE WALKED DOWN THE HILL INTO {HOME} WITH GRASS IN THEIR HAIR, CERTAIN IT WAS STILL MIDSUMMER. {TAKEN} WAS SINGING. ACROSS {LAND}, CAPTAINS OF TWO KINGDOMS WOKE FROM THE SAME DREAM AND COULD NOT REMEMBER WHY THEY HATED EACH OTHER."
  journal "THE LADY OF THE HOLLY IS QUEEN UNDER THE HILL. THE TAKEN ARE HOME IN {HOME}, AND {LAND} HAS PEACE WITH {RIVAL}."
  do take "AN OLD IRON NAIL"
  do realm peace land rival
  do reward great
  do mark fae_queen_lady
  do remember poster "{TAKEN} CAME HOME SINGING. {TAKEN.HE} SAYS IT WAS ONE NIGHT. IT WAS FORTY. I DON'T CORRECT {TAKEN.HIM}."
  do remember taken "I DREAM OF A WOMAN IN A CROWN OF HOLLY. SHE ASKS AFTER YOU. WHAT DID YOU DO?"
  do fact "THE SIX TAKEN FROM {HOME} CAME HOME AT DAWN. THEY SAY A QUEEN UNDER {HILL} MADE PEACE BETWEEN {LAND} AND {RIVAL}."

stage crown_prince
  talk prince
  say "ME! OF COURSE ME. YOU HAVE EXQUISITE TASTE, MORTAL. THE CROWN FITS, DOESN'T IT? ...YOUR SIX? OH, THEY STAY. THE COURT NEEDS MUSIC. BUT {HOME} WILL BE PAID, I PROMISE YOU THAT. WE ALWAYS PAY. IT IS JUST THAT WE PAY IN OUR OWN COIN."
  opt "THAT WASN'T THE BARGAIN." -> prince_end
  opt "...SO BE IT." -> prince_end

stage prince_end
  end fail
  say "THE MUSIC UNDER THE HILL GREW LOUDER THAT AUTUMN, AND SWEETER. THE FIELDS AROUND {HOME} GREW NOTHING AT ALL. THE PRINCE'S COIN, THE OLD WOMEN SAID: A YEAR OF HUNGER FOR EVERY VOICE KEPT."
  journal "THE ROWAN PRINCE RULES UNDER THE HILL. THE TAKEN STAY, AND THE FIELDS OF {HOME} ARE BARREN."
  do take "AN OLD IRON NAIL"
  do realm famine home
  do mark fae_queen_prince
  do remember poster "THERE'S MUSIC ON THE HILL EVERY NIGHT NOW. I KNOW THE VOICE IN IT. YOU CROWNED THE ONE WHO KEPT {TAKEN.HIM}."
  do fact "THE FIELDS OF {HOME} FAILED THE YEAR A STRANGER CROWNED A PRINCE UNDER {HILL}. THEY SAY THE MUSIC NEVER STOPS."

stage sealed
  talk lady
  say "IRON. SALT. ON OUR DOOR. YOU WOULD SHUT US IN THE DARK FOREVER... BRING YOUR SIX OUT FIRST, THEN. I WILL NOT STOP YOU. TAKE THIS TOO: THE SILVER-WORKING OF OUR SMITHS, WHICH NO MORTAL HAS SEEN. SOMEONE SHOULD REMEMBER WE WERE BEAUTIFUL."
  opt "I'LL REMEMBER." -> sealed_end

stage sealed_end
  end success
  say "YOU LED SIX STUMBLING PEOPLE OUT INTO DAYLIGHT AND DROVE THE NAIL INTO THE OLD STONES OF {STONES} AND POURED THE SALT. THE HILL WENT QUIET. IT HAS BEEN QUIET EVER SINCE, AND THE CHILDREN OF {HOME} PLAY ON IT AND NEVER KNOW WHY IT FEELS SAD."
  journal "THE DOOR UNDER {HILL} IS SEALED WITH IRON AND SALT. THE TAKEN ARE HOME, AND THE COURT IS SHUT AWAY FOREVER."
  do take "AN OLD IRON NAIL"
  do secret hill
  do reward rich
  do mark fae_sealed
  do remember poster "THE HILL'S QUIET NOW. {TAKEN} DOESN'T SING ANY MORE. {TAKEN.HE} SAYS THERE'S NOTHING LEFT TO SING ABOUT."
  do fact "THE OLD STONES OF {STONES} HAVE AN IRON NAIL DRIVEN INTO THEM, AND SALT ON THE GROUND. NOTHING HAS GONE MISSING SINCE."

stage stayed
  end fail
  say "YOU STAYED FOR ONE DANCE. WHEN YOU CAME UP THE HILL WAS GREEN AGAIN, AND {POSTER} WAS AN OLD WOMAN WHO DID NOT KNOW YOU, AND NOBODY IN {HOME} HAD HEARD OF ANYONE GOING MISSING. NOT FOR YEARS."
  journal "YOU STAYED UNDER THE HILL. NOBODY IN {HOME} REMEMBERS THE TAKEN, OR YOU."
  do take "AN OLD IRON NAIL"
  do mark fae_stayed
  do remember poster "I'M SORRY, DO I KNOW YOU? YOU HAVE THE LOOK OF SOMEONE WHO'S BEEN DANCING."
  do fact "A STRANGER CAME DOWN FROM {HILL} ASKING AFTER SIX NAMES NOBODY IN {HOME} KNEW."
)SAGA";

// ---- arc 1: the way in
const char* const kRevel = R"SAGA(
stage %stones
  goal enter stones
  then %dance
  do moves prince stones
  say "THE STONES OF {STONES} STAND IN A RING ON THE HILLSIDE. AT MOONRISE THE GAPS BETWEEN THEM FILL WITH LIGHT, LIKE DOORWAYS."
  journal "AT MOONRISE THE OLD STONES OF {STONES}, {STONES.DIR}, OPEN ON THE COURT. GO TO THE REVEL. EAT NOTHING."
stage %dance
  talk prince
  say "A GUEST! A REAL ONE, WITH MUD ON. HOW DELIGHTFUL. DANCE WITH ME, EAT A PLUM, TELL ME YOUR NAME. NO? THEN AT LEAST TELL ME WHY YOU SMELL OF IRON. IT'S VERY RUDE AT A PARTY."
  opt "I'M LOOKING FOR {TAKEN}." -> %ask
  opt "ONE PLUM CAN'T HURT." -> %plum
  opt "I'LL DANCE. NOTHING ELSE." -> %dance2
stage %plum
  talk prince
  say "IT CAN'T! THAT'S THE MARVELLOUS THING. IT DOESN'T HURT AT ALL. YOU'LL JUST FIND THAT YOUR OWN FOOD TASTES OF ASHES FOR A WHILE. A YEAR. SEVEN. WHO COUNTS? ...NOW. WHO DID YOU SAY YOU WERE LOOKING FOR?"
  do add bargain 1
  opt "{TAKEN}. FROM {HOME}." -> %ask
stage %dance2
  talk prince
  say "YOU DANCE LIKE A PLOUGHHORSE IN LOVE. I ADORE IT. ...THE GIRL? THE BOY? FROM {HOME}, WITH THE VOICE? OH, {TAKEN.HE} IS PERFECTLY HAPPY. COME DOWN AND SEE. AS MY GUEST, NOT MY PRISONER. THERE IS A DIFFERENCE, I PROMISE. A SMALL ONE."
  opt "SHOW ME, THEN." -> @next
stage %ask
  talk prince
  say "{TAKEN}! WITH THE VOICE! {TAKEN.HE} SINGS FOR THE WHOLE COURT NOW. YOU'LL WANT TO TAKE {TAKEN.HIM} HOME, I SUPPOSE. EVERYONE DOES. COME DOWN AND ASK {TAKEN.HIM} IF {TAKEN.HE} WANTS TO GO. GUEST'S HONOUR: YOU WILL WALK OUT AGAIN."
  opt "GUEST'S HONOUR. I'LL HOLD YOU TO IT." -> @next
  opt "LEAD ON." -> @next
)SAGA";

const char* const kNameDoor = R"SAGA(
stage %hill
  goal enter hill
  then %door
  journal "THE WAY UNDER THE HILL IS THROUGH {HILL}, {HILL.DIR}. THE DOOR IS SAID TO ASK EVERY TRAVELLER A QUESTION."
stage %door
  talk lady
  say "THE DOOR ASKS, MORTAL, AND I AM THE DOOR TONIGHT, FOR MY SINS. WHAT IS YOUR NAME? ...SILENCE. SO YOU HAVE BEEN TAUGHT SOMETHING. THEN I ASK ANOTHER: WHAT DO YOU LOVE THAT YOU WOULD NOT TRADE FOR {TAKEN}?"
  opt "NOTHING. I'D TRADE IT ALL." -> %all
  opt "MY NAME. YOU'LL NOT HAVE IT." -> %name
  opt "A RIDDLE FOR A RIDDLE?" -> %riddle
stage %all
  talk lady
  say "EVERYTHING? FOR SOMEONE ELSE'S [[=rel]]? THAT IS EITHER SAINTLY OR STUPID, AND IN MY EXPERIENCE MORTALS DO NOT KNOW WHICH THEMSELVES. I WILL TAKE A SMALL PART OF EVERYTHING. YOUR SHADOW, A LITTLE. YOU WON'T MISS IT. MUCH."
  do add bargain 1
  opt "...TAKE IT." -> @next
stage %name
  talk lady
  say "GOOD. KEEP IT. THE LAST MORTAL WHO GAVE ME A NAME IS STILL SWEEPING MY HALLS, AND HE HAS FORGOTTEN WHY. PASS, THEN. AND TELL NO ONE THE DOOR WAS KIND. I HAVE A REPUTATION."
  do add names 1
  do add holly 1
  opt "YOUR SECRET IS SAFE." -> @next
stage %riddle
  talk lady
  say "...BOLD. ASK."
  opt "WHAT KEEPS A THRONE EMPTY?" -> %throne
  opt "WHY DO YOU HATE US?" -> %hate
stage %throne
  talk lady
  say "A CROWN THAT WILL NOT SIT ON THE HEAD IT WAS MEANT FOR, AND A HEAD THAT WILL NOT BOW TO THE ONE THAT WANTS IT. ...YOU ASKED A BETTER QUESTION THAN YOU KNEW. PASS, MORTAL. AND WATCH THE PRINCE OF ROWAN. HE SMILES WITH ALL HIS TEETH."
  do add holly 1
  opt "I'LL WATCH HIM." -> @next
stage %hate
  talk lady
  say "BECAUSE YOU DIE. YOU COME HERE, AND WE LOVE YOU, AND THEN YOU DIE, IN SIXTY YEARS OR SEVENTY, AND WE GO ON AND ON WITH THE SHAPE OF YOU IN OUR HALLS. PASS. AND DO NOT ASK ME THAT AGAIN."
  do add holly 1
  opt "...I'M SORRY." -> @next
)SAGA";

// ---- arc 2: the houses
const char* const kHollyBargain = R"SAGA(
stage %halls
  goal enter hill
  then %offer
  do moves prince hill
  journal "UNDER {HILL}: HALLS OF ROOT AND SILVER, AND TWO HOUSES THAT HATE EACH OTHER. THE LADY OF THE HOLLY HAS ASKED TO SEE YOU."
stage %offer
  talk lady
  say "THE PRINCE OF ROWAN TOOK YOUR SIX. HE PAYS HIS DEBTS TO THE COURT IN MORTAL VOICES, AND HIS CLAIM TO THE THRONE GROWS WITH EVERY SONG. I WANT HIM BROKEN. YOU WANT YOUR SIX. A BARGAIN, THEN, MARKED ON THE SKIN."
  opt "WHAT IS THE MARK?" -> %mark
  opt "NO MARKS. MY WORD IS ENOUGH." -> %word
stage %mark
  talk lady
  say "A HOLLY LEAF, HERE, INSIDE THE WRIST. WHILE IT IS THERE YOU CANNOT LIE TO ME, NOR I TO YOU. WHEN YOU HAVE DONE ME ONE SERVICE OF MY CHOOSING, IT FADES. I WILL NOT CHOOSE CRUELLY. I WILL NOT CHOOSE KINDLY EITHER."
  opt "MARK ME." -> %marked
  opt "NO. MY WORD OR NOTHING." -> %word
stage %marked
  talk lady
  say "THERE. IT STINGS, DOESN'T IT? IT WILL STING WHENEVER YOU THINK OF LYING. ...YOU THOUGHT OF IT JUST NOW, I FELT IT. NOT TO ME. TO SOMEONE AT HOME. HOW INTERESTING."
  do set bargain 2
  do add holly 1
  do mark fae_holly_mark
  opt "WHAT IS THE SERVICE?" -> @next
stage %word
  talk lady
  say "A MORTAL'S WORD. I HAVE A DRAWER FULL OF THEM, ALL BROKEN. ...BUT YOU KEPT YOUR NAME AT THE DOOR. VERY WELL. YOUR WORD, THEN, AND IF YOU BREAK IT I WILL COME TO {HOME} MYSELF, AND I WILL NOT BRING FLOWERS."
  opt "IT WON'T BREAK." -> @next
)SAGA";

const char* const kRowanCourtesy = R"SAGA(
stage %halls
  goal enter hill
  then %feast
  do moves prince hill
  journal "UNDER {HILL}: HALLS OF ROOT AND SILVER. THE PRINCE OF ROWAN HAS MADE YOU HIS GUEST OF HONOUR. THAT IS RARELY GOOD."
stage %feast
  talk prince
  say "SIT BY ME. YES, THERE. YOUR {TAKEN} IS SINGING, LISTEN... ISN'T IT EXQUISITE? THE LADY OF THE HOLLY WOULD HAVE ALL OF YOU MORTALS KEPT OUT OF THE HILL, OR KEPT IN IT AS SERVANTS. I KEEP YOU AS GUESTS. YOU SEE THE DIFFERENCE?"
  opt "LET {TAKEN} GO HOME." -> %price
  opt "WHAT DO YOU WANT FROM ME?" -> %price
stage %price
  talk prince
  say "WHAT EVERYONE WANTS: TO BE LOVED, AND TO BE KING. THE FIRST I MANAGE. THE SECOND NEEDS A GUEST TO NAME ME. A MORTAL GUEST, BY THE OLD LAW. DO IT, AND YOUR SIX GO HOME... EVENTUALLY. WE MEASURE THE WORD DIFFERENTLY."
  opt "AND IF I DON'T?" -> %threat
  opt "I'LL CONSIDER IT." -> %consider
stage %threat
  talk prince
  say "THEN NOTHING! NOTHING AT ALL. I'M NOT A MONSTER. YOU'LL SIMPLY FIND THE WAY OUT HARDER TO REMEMBER EACH DAY, UNTIL ONE MORNING YOU WAKE UP AND YOU'RE SINGING TOO. ...MORE WINE?"
  opt "NO WINE." -> @next
  opt "...ONE CUP." -> %wine
stage %wine
  talk prince
  say "THERE. ISN'T THAT BETTER? DON'T LOOK SO WORRIED. ONE CUP ONLY MAKES YOU A LITTLE BIT OURS."
  do add bargain 1
  opt "I SHOULD GO." -> @next
stage %consider
  talk prince
  say "CONSIDER! MORTALS LOVE TO CONSIDER. IT'S LIKE WATCHING A CANDLE DECIDE WHETHER TO BURN. TAKE YOUR TIME. YOU HAVE MUCH LESS OF IT THAN I DO."
  opt "[[I KNOW.|THAT'S WHAT MAKES IT PRECIOUS.]]" -> @next
)SAGA";

// ---- arc 3: the trial
const char* const kThreeTasks = R"SAGA(
stage %court
  talk lady
  say "THE COURT HAS NOTICED YOU, WHICH IS WORSE THAN BEING EATEN. THEY SET A GUEST THREE TASKS, BY THE OLD LAW. FAIL ONE AND YOU ARE OURS. THE FIRST: BRING BACK THE VOICE OF THE HILL'S OLD GUARDIAN, WHICH IT LOST IN THE DEEP STONES."
  journal "THE COURT UNDER THE HILL HAS SET YOU THREE TASKS. THE FIRST: THE GUARDIAN'S LOST VOICE, IN THE DEEP STONES OF {STONES}."
  opt "WHERE IS IT?" -> %first
stage %first
  goal fetch "A VOICE IN A STOPPERED JAR" in stones
  then %second
  say "THE JAR HUMS IN YOUR HAND LIKE A BEE IN A CUP. WHEN YOU HOLD IT TO YOUR EAR, SOMEONE VERY OLD IS SAYING SORRY."
  journal "FIND THE GUARDIAN'S VOICE IN THE DEEP STONES OF {STONES}, {STONES.DIR}, AND BRING IT BACK UNDER THE HILL."
stage %second
  talk lady
  say "THE VOICE. GOOD. THE SECOND TASK IS TO SPEND A NIGHT IN THE HALL OF MIRRORS AND COME OUT AS YOURSELF. MOST COME OUT AS SOMEONE ELSE. ONE CAME OUT AS A HARE. ...WHY ARE YOU SMILING?"
  do take "A VOICE IN A STOPPERED JAR"
  opt "BECAUSE I'VE ONLY EVER BEEN ME." -> %mirrors
  opt "BECAUSE I'D MAKE A GOOD HARE." -> %mirrors
stage %mirrors
  goal wait 1
  then %third
  say "IN THE MIRRORS YOU ARE A KING, A CORPSE, A CHILD IN {HOME}, A HARE. YOU SAY YOUR OWN NAME UNDER YOUR BREATH ALL NIGHT, AND NOT ONCE ALOUD."
  journal "SPEND A NIGHT IN THE HALL OF MIRRORS UNDER {HILL}, AND COME OUT AS YOURSELF."
stage %third
  talk prince
  say "YOU'RE STILL YOU. HOW TIRESOME. THE THIRD TASK IS MINE TO SET, AND IT IS SIMPLE: TELL THE COURT, TRUTHFULLY, THAT THE LADY OF THE HOLLY IS UNFIT TO RULE. ONE SENTENCE. THEN YOU'VE PASSED, AND YOUR SIX ARE YOURS."
  opt "SHE IS UNFIT TO RULE." -> %lie
  opt "I WON'T SAY IT." -> %refuse
stage %lie
  talk lady
  say "...THE MARK STUNG YOU, DIDN'T IT? OR YOUR CONSCIENCE DID, WHICH IS A POORER SORT OF HOLLY. THE COURT HEARD YOU. THEY ALWAYS HEAR. I WILL NOT FORGET THIS, MORTAL. NOR, I THINK, WILL YOU."
  do set holly 0
  do add bargain 1
  opt "I DID WHAT I HAD TO." -> @next
stage %refuse
  talk prince
  say "WON'T? THEN YOU FAIL THE THIRD TASK, AND YOU ARE THE COURT'S, AND... OH. OH, THAT'S CLEVER. A TASK THE GUEST CAN ONLY FAIL BY LYING CAN'T BE FAILED BY TELLING THE TRUTH. WHO TAUGHT YOU THE OLD LAW?"
  do add holly 1
  do add names 1
  opt "NOBODY. IT'S JUST HONEST." -> @next
)SAGA";

const char* const kMoonHunt = R"SAGA(
stage %horns
  talk prince
  say "A HUNT TONIGHT! THE COURT'S FAVOURITE ENTERTAINMENT. WE RIDE THE MOONLIGHT, AND YOU, MY DEAR GUEST, ARE THE QUARRY. REACH THE OLD STONES BEFORE DAWN AND YOU'VE WON THE COURT'S RESPECT. FALL, AND, WELL. YOU'LL SING."
  journal "THE COURT HUNTS YOU BY MOONLIGHT. REACH THE OLD STONES OF {STONES} BEFORE DAWN."
  opt "THEN I'D BETTER RUN." -> %run
  opt "AND IF I DON'T RUN?" -> %stand
stage %stand
  talk prince
  say "DON'T RUN? THEN IT ISN'T A HUNT, IT'S A... HM. I HADN'T CONSIDERED. THE HOUNDS WON'T KNOW WHAT TO DO WITH QUARRY THAT SITS DOWN AND WAITS. HOW EMBARRASSING FOR EVERYONE. ...RUN ANYWAY. PLEASE. FOR ME."
  opt "...FINE." -> %run
stage %run
  goal kill 4 beast
  then %stones
  say "THE HORNS ARE SILVER AND THE HOUNDS ARE MADE OF SOMETHING THAT ISN'T QUITE DOG. THEY ARE CLOSING."
  journal "THE COURT'S HOUNDS ARE ON YOUR HEELS. BREAK THROUGH THEM TO THE OLD STONES OF {STONES}."
stage %stones
  goal enter stones
  then %dawn
  do moves lady stones
  journal "THE STONES OF {STONES}, {STONES.DIR}. REACH THEM BEFORE DAWN."
stage %dawn
  talk lady
  say "YOU MADE IT. MUDDY, BLEEDING, FOUR OF MY HOUNDS SULKING IN THE BRACKEN. I CALLED THEM OFF, AT THE LAST. DON'T THANK ME. I WANTED TO SEE IF YOU WOULD STOP TO HELP THE ONE THAT FELL. YOU DID. WHY?"
  opt "IT WAS HURT." -> %why
  opt "I DIDN'T THINK." -> %why
stage %why
  talk lady
  say "...MY MOTHER WOULD HAVE LIKED YOU. THAT IS NOT A COMPLIMENT, UNDER THE HILL. SHE LIKED MORTALS, AND THEY KILLED HER FOR IT, AND I HAVE HATED YOUR KIND FOR [[=age]] YEARS. GO BACK DOWN. THE COURT RESPECTS YOU NOW. I'M NOT SURE I DO."
  do add holly 1
  do fame 1
  opt "YOU WILL." -> @next
  opt "I DON'T NEED YOU TO." -> @next
)SAGA";

// ---- arc 4: the hidden queen
const char* const kTrueName = R"SAGA(
stage %summons
  talk taken
  say "YOU CAME! I KNEW SOMEONE WOULD. LISTEN, I SING FOR THEM, AND THEY TALK WHILE I SING LIKE I'M FURNITURE. THE LADY OF THE HOLLY IS THE OLD QUEEN'S DAUGHTER. THE TRUE HEIR. SHE HIDES IT. IF THE COURT LEARNED HER NAME, THEY'D HAVE TO CROWN HER."
  journal "{TAKEN} HAS HEARD THE COURT'S WHISPERS: THE LADY OF THE HOLLY IS THE HIDDEN HEIR OF THE THRONE UNDER THE HILL."
  opt "WHY DOES SHE HIDE IT?" -> %why
  opt "ARE YOU ALL RIGHT?" -> %ok
stage %ok
  talk taken
  say "I'M... I DON'T KNOW. I'M NEVER HUNGRY HERE. I'M NEVER TIRED. I THINK I'VE BEEN SINGING FOR A YEAR, AND MY THROAT DOESN'T HURT. THAT'S WRONG, ISN'T IT? THAT'S HOW YOU KNOW IT'S WRONG. TELL {POSTER} I'M SORRY I WENT."
  opt "TELL {POSTER.HIM} YOURSELF. SOON." -> %why
stage %why
  talk lady
  do moves lady hill
  say "BECAUSE A QUEEN UNDER THE HILL PAYS FOR THE CROWN WITH WHAT SHE HATES MOST, AND I HATE YOUR KIND. TO RULE I MUST LOVE YOU. OR AT LEAST PROTECT YOU. A QUEEN OF HATRED, BOUND TO KEEP MORTALS SAFE. WOULD YOU WANT THAT CROWN?"
  opt "YOU'D MAKE A GOOD QUEEN." -> %good
  opt "THEN DON'T TAKE IT." -> %dont
stage %good
  talk lady
  say "...YOU BELIEVE THAT. THE HOLLY WOULD STING IF YOU DIDN'T. WELL. I WILL STAND AT THE THRONE, MORTAL. WHETHER I SIT IN IT IS YOURS TO SAY. THE COURT HAS DECIDED YOU ARE OUR GUEST, AND GUESTS NAME KINGS."
  do add holly 1
  opt "THEN I'LL NAME ONE." -> @next
stage %dont
  talk lady
  say "SPOKEN LIKE A MORTAL WHO HAS NEVER BEEN OFFERED ANYTHING. ...BUT IF NOT ME, HIM. AND HE PAYS FOR HIS CROWN IN VOICES. THINK ON THAT AT THE THRONE, BECAUSE YOU WILL BE THE ONE WHO NAMES IT."
  opt "I'LL THINK ON IT." -> @next
)SAGA";

const char* const kChangeling = R"SAGA(
stage %cradle
  goal goto home
  then %poster
  journal "A STRANGE WORD FROM {HOME}: {POSTER} SAYS {TAKEN} CAME HOME LAST NIGHT, AND IS NOT {TAKEN}."
stage %poster
  talk poster
  say "IT CAME HOME LAST NIGHT, WEARING {TAKEN}'S FACE. IT DOESN'T SING. IT EATS LIKE A WOLF AND LAUGHS AT THINGS THAT AREN'T FUNNY. THE OLD WAY IS TO PUT IT ON THE FIRE UNTIL IT SCREAMS ITS TRUE SHAPE. I CAN'T. I CAN'T. TELL ME WHAT TO DO."
  opt "NO FIRE. LET ME SPEAK TO IT." -> %speak
  opt "IRON. LAY THE NAIL ON IT." -> %iron
stage %speak
  talk poster
  say "IT SAID... IT SAID IT WAS SENT BY THE PRINCE OF ROWAN TO KEEP {TAKEN}'S PLACE WARM. IT SAID IT IS A CHILD OF THE COURT, AND IT HAS NEVER HAD A MOTHER, AND IT LIKES MINE. ...IT'S CRYING. CAN THEY CRY?"
  do add holly 1
  opt "TAKE IT BACK UNDER THE HILL. GENTLY." -> %return
stage %iron
  talk poster
  say "IT SCREAMED. NOT LIKE A CHILD. LIKE A KETTLE, LIKE A HARE IN A TRAP, AND THEN IT WAS A BUNDLE OF STICKS AND MOSS IN {TAKEN}'S CLOTHES. ...THE OLD WAY WORKS. I WISH IT HADN'T. I WISH I HADN'T WATCHED."
  do add bargain 1
  opt "NOW I GO BACK FOR THE REAL ONE." -> %return
stage %return
  goal enter hill
  then %reveal
  journal "THE PRINCE OF ROWAN SENT A CHANGELING TO {HOME} IN {TAKEN}'S PLACE. GO BACK UNDER {HILL}."
stage %reveal
  talk lady
  do moves lady hill
  say "A CHANGELING IN A MORTAL'S BED. HE IS SPENDING THE COURT'S CHILDREN TO KEEP HIS PRISONERS. ...I HAVE HIDDEN WHAT I AM FOR [[=age]] YEARS, MORTAL. I AM THE OLD QUEEN'S DAUGHTER. AND I AM DONE HIDING."
  opt "THEN STAND AT THE THRONE." -> @next
  opt "WHY TELL ME?" -> @next
)SAGA";

}  // namespace

void addFaeArcs(std::vector<Archetype>& v) {
  Archetype a;
  a.source = Source::Folk;
  a.tier = 3;
  a.needs = N_TOWN | N_CAVE | N_RUIN;

  a.id = "fa_revel"; a.name = "THE REVEL AT THE STONES";
  a.themes = TH_TEMPTATION | TH_WONDER | TH_HOSPITALITY; a.body = kRevel; v.push_back(a);
  a.id = "fa_name_door"; a.name = "THE DOOR THAT ASKS A NAME";
  a.themes = TH_WONDER | TH_TRICKERY | TH_COURAGE; a.body = kNameDoor; v.push_back(a);
  a.source = Source::Maas;
  a.id = "fa_holly_bargain"; a.name = "THE BARGAIN ON THE SKIN";
  a.themes = TH_BARGAIN | TH_LOVE | TH_TEMPTATION; a.body = kHollyBargain; v.push_back(a);
  a.id = "fa_rowan_courtesy"; a.name = "THE PRINCE'S COURTESY";
  a.themes = TH_TEMPTATION | TH_TRICKERY | TH_PRIDE; a.body = kRowanCourtesy; v.push_back(a);
  a.source = Source::Folk;
  a.id = "fa_three_tasks"; a.name = "THREE TASKS UNDER THE HILL";
  a.themes = TH_COURAGE | TH_TRICKERY | TH_WONDER; a.body = kThreeTasks; v.push_back(a);
  a.source = Source::Maas;
  a.id = "fa_moon_hunt"; a.name = "THE HUNT BY MOONLIGHT";
  a.themes = TH_COURAGE | TH_MERCY | TH_LOVE; a.body = kMoonHunt; v.push_back(a);
  a.id = "fa_true_name"; a.name = "THE HIDDEN QUEEN";
  a.themes = TH_KINGSHIP | TH_SACRIFICE | TH_LOVE; a.body = kTrueName; v.push_back(a);
  a.source = Source::Folk;
  a.id = "fa_changeling"; a.name = "THE CHANGELING IN THE BED";
  a.themes = TH_KINSHIP | TH_MERCY | TH_CURSE; a.body = kChangeling; v.push_back(a);
}

CampaignPlan faePlan() {
  CampaignPlan p;
  p.id = "fae";
  p.name = "THE COURT UNDER THE HILL";
  p.source = Source::Maas;
  p.themes = TH_BARGAIN | TH_WONDER | TH_KINGSHIP | TH_LOVE | TH_TRICKERY;
  p.needs = N_TOWN | N_CAVE | N_RUIN | N_KINGDOM | N_RIVAL;
  p.head = kHead;
  p.arcs = {ArcSlot{{"fa_revel", "fa_name_door"}}, ArcSlot{{"fa_holly_bargain", "fa_rowan_courtesy"}},
            ArcSlot{{"fa_three_tasks", "fa_moon_hunt"}}, ArcSlot{{"fa_true_name", "fa_changeling"}}};
  p.finale = kFinale;
  return p;
}

}  // namespace camp
}  // namespace saga
}  // namespace story
