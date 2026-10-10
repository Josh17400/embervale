// M6b "Sagas": THE BURDEN ACROSS KINGDOMS (campaign `burden`). CAMPAIGNS lane. Tolkien-like THEMES only (a fellowship
// carrying a corrupting thing it must not use, the small bearer on the long road, temptation, mercy to the broken, an
// ancient evil waking in a ruin): no names, places, terms or plot of any modern work.
//
// Hook: a ruin whose record tells of a made thing its makers could not unmake (lore.cpp burdenRuin). In its deepest room
// a scholar has opened a dead king's tomb and found the thing in his hands: a crown, a key or an amulet that whispers,
// commands the dead, and sets kingdoms dreaming of knives. Only the fire it was forged in can unmake it, and that forge
// is another ruin of the real world ({FORGE.OLD}, its last lord, how long ago it fell).
//   head      the scholar in the tomb; the thing passes to the player
//   arc 1     THE FIRST NIGHT        the dead who follow (use it once?) | the village that dreamed of knives
//   arc 2     THE SWORN              the captain at the ford (an oath) | the captain at the gate (an arrest)
//   arc 3     THE TEMPTATION         the bearer before you (a hermit kept alive by it) | the council of the capital
//   arc 4     THE BREAKING           the night the oath broke | what the scholar wrote down
//   finale    the forge: unmade (peace between the kingdoms) | unmade at a price | the sworn takes it (a new ruler and a
//             war) | the sworn throws it | buried with its secret kept (a smithing secret) | worn (war; a failure)
// Recurring: the scholar, the sworn captain; the ruler and capital of the land; the rival kingdom; the forge ruin.
#include <vector>
#include "rpg/story/saga.h"

namespace story {
namespace saga {
namespace camp {

namespace {

const char* const kHead = R"SAGA(
title THE BURDEN OF THE [[adj=GREY|COLD|HOLLOW|NAMELESS]] [[art=CROWN|KEY|AMULET]]
hook ruin
pitch "WHO'S THERE? SHOW YOURSELF."
hint "A LAMP BURNS IN THE DEEPEST ROOM, AND SOMEONE DOWN THERE IS TALKING TO IT."
role giver giver
role home site home
role land kingdom home
role rival kingdom rival
role lord ruler land
role seat capital land
role forge ruin near
role scholar person [[female|male]] at home
role town site town near
role sworn person [[male|female]] at town
var burden 0
var sworn 0
var lore 0
var angered 0

stage lamp
  talk scholar
  say "STAY BACK. NOT FROM ME: FROM IT. I OPENED THE INNER TOMB OF {HOME} [[days=THREE|FOUR|NINE]] DAYS AGO, AND THE DEAD KING IN IT WAS HOLDING THIS. THE [[=adj]] [[=art]]. I HAVEN'T SLEPT SINCE. IT TALKS, AND IT HAS LEARNED MY MOTHER'S VOICE."
  journal "IN THE DEEPEST ROOM OF {HOME}, {SCHOLAR} HAS FOUND SOMETHING IN A DEAD KING'S HANDS."
  opt "WHAT IS IT?" -> lamp2
  opt "PUT IT DOWN AND WALK AWAY." -> lamp3
  opt "NOT MY TOMB. NOT MY TROUBLE." -> refused

stage lamp2
  talk scholar
  say "THE WALLS SAY IT WAS MADE IN {FORGE.OLD} FOR {FORGE.LORD}, WHO WANTED TO BE OBEYED AND NEVER TO DIE, AND GOT BOTH. WHAT A FORGE MAKES, ONLY THAT FORGE UNMAKES. {FORGE} LIES {FORGE.DIR}. I CAN'T TAKE IT THERE. IT WON'T LET ME."
  opt "THEN I'LL CARRY IT." -> take
  opt "WHY NOT LEAVE IT IN THE TOMB?" -> lamp3
  opt "NOT MY TOMB. NOT MY TROUBLE." -> refused

stage lamp3
  talk scholar
  say "BECAUSE IT'S AWAKE. THE DEAD UPSTAIRS STARTED WALKING THE NIGHT I LIFTED THE LID, AND THE FARMS BELOW DREAM OF KNIVES. {LAND} AND {RIVAL} WERE TRADING GRAIN LAST MONTH. NOW THEY COUNT SPEARS. IT DOES THAT. <<warning>>"
  opt "THEN I'LL CARRY IT." -> take
  opt "NOT MY TOMB. NOT MY TROUBLE." -> refused

stage refused
  end fail
  say "{SCHOLAR} DOES NOT ARGUE. {SCHOLAR.HE} SITS BACK DOWN BY THE LAMP, CUPPED AROUND THE THING LIKE A CANDLE IN A WIND, AND STARTS TALKING TO IT AGAIN, VERY QUIETLY."
  journal "YOU LEFT {SCHOLAR} IN THE DEEP OF {HOME} WITH WHAT {SCHOLAR.HE} FOUND THERE."
  do remember scholar "YOU AGAIN. IT SAID YOU'D COME BACK. IT SAYS EVERYONE COMES BACK IN THE END."
  do mark burden_refused

stage take
  talk scholar
  say "HERE. DON'T LOOK AT IT TOO LONG, AND WHATEVER IT OFFERS, SAY NO OUT LOUD: IT HEARS SILENCE AS YES. ...I'M COMING WITH YOU. SOMEONE HAS TO WRITE THIS DOWN. AND I WANT TO SEE IT BREAK."
  journal "YOU CARRY THE [[=adj]] [[=art]] OUT OF {HOME}. IT IS HEAVIER THAN IT LOOKS."
  do give "THE [[=adj]] [[=art]]" [[=art]]
  do mark burden_taken
  opt "THEN WE GO TO {FORGE}." -> @next
  opt "[[STAY CLOSE.|WRITE QUICKLY, THEN.]]" -> @next
)SAGA";

const char* const kFinale = R"SAGA(
stage forge_heart
  talk scholar
  ?arc_bu_old_bearer say "THE FIRE-PIT OF {FORGE.OLD}. COLD FOR {FORGE.YEARS} YEARS, AND WARM UNDER MY HAND. SOMEWHERE BEHIND US AN OLD MAN IS WAITING TO BE TIRED. {PLAYER}, IT'S IN YOUR HANDS. IT HAS TO BE YOU."
  !arc_bu_old_bearer say "THE FIRE-PIT OF {FORGE.OLD}. COLD FOR {FORGE.YEARS} YEARS, AND WARM UNDER MY HAND. IT KNOWS WE'RE HERE. IT'S BEGGING. {PLAYER}, IT'S IN YOUR HANDS. IT HAS TO BE YOU."
  opt "THROW IT IN." check novar burden 2 -> unmade else wavering
  opt "GIVE IT TO {SWORN}." if var sworn 1 -> to_sworn
  opt "BURY IT HERE, UNBROKEN." -> bury
  opt "PUT IT ON." -> worn

stage wavering
  talk scholar
  say "YOU'RE NOT MOVING. {PLAYER}? YOUR HAND WON'T OPEN, WILL IT. YOU USED IT, AND NOW IT HAS HOOKS IN YOU. GIVE IT TO ME. I'VE WANTED TO HOLD IT SINCE THE TOMB, AND THAT'S WHY IT HAS TO BE ME WHO LETS GO."
  opt "TAKE IT. DO IT." -> scholar_throws
  opt "NO. IT'S MINE." -> worn

stage to_sworn
  talk sworn
  say "FOR ME? ...I CAN FEEL IT CHOOSING ME. {RIVAL}'S BANNERS BURNING, AND MY BROTHER THE LAST MAN WHO EVER DIED ON THAT BORDER. AND THE CROWN OF {LAND} AFTER, WHY NOT. UNLESS... TELL ME TO THROW IT, {PLAYER}. ONE WORD."
  opt "THROW IT, {SWORN}." -> sworn_throws
  opt "KEEP IT. END THE WAR YOUR WAY." -> sworn_crowned

stage bury
  talk scholar
  say "BURY IT? UNBROKEN, FOR THE NEXT FOOL WITH A LAMP? ...THE SMITHS' MARKS ARE ALL OVER THESE WALLS. HOW IT WAS MADE. KEEP THE KNOWING AND BURY THE THING, AND SOMEDAY SOMEONE MAKES SOMETHING GOOD WITH IT. OR SOMETHING WORSE."
  opt "COPY THE MARKS. BURY IT." -> buried
  opt "NO. BREAK IT. NOW." -> unmade

stage unmade
  end success
  say "THE [[=art]] SCREAMED IN THE VOICE OF EVERYONE YOU EVER LOVED, AND THEN IT WAS SLAG. {SCHOLAR} SAT DOWN ON THE FLOOR AND LAUGHED UNTIL {SCHOLAR.HE} CRIED. FAR AWAY, MEN WHO HAD BEEN SHARPENING SPEARS PUT THEM DOWN AND COULD NOT SAY WHY."
  journal "THE [[=adj]] [[=art]] IS UNMADE IN THE FIRE OF {FORGE.OLD}. {LAND} AND {RIVAL} HAVE STOPPED COUNTING SPEARS."
  do take "THE [[=adj]] [[=art]]"
  do realm peace land rival
  do reward great
  do fame 4
  do mark burden_unmade
  do remember scholar "I'M WRITING IT ALL DOWN. EVERYTHING EXCEPT HOW IT WAS MADE. THAT PART I'M LETTING DIE."
  do fact "THE [[=adj]] [[=art]] OF {FORGE.OLD} WAS CARRIED FROM {HOME} TO {FORGE} AND UNMADE THERE. THE WAR THAT WAS COMING DID NOT COME."

stage scholar_throws
  end success
  say "{SCHOLAR}'S FINGERS CLOSED ON IT AND WOULD NOT OPEN, SO {SCHOLAR.HE} PUT HAND AND ALL INTO THE FIRE. THE THING WENT OUT OF THE WORLD LIKE A TOOTH OUT OF A JAW. {SCHOLAR.HE} WRITES LEFT-HANDED NOW, AND SAYS THE PRICE WAS LOW."
  journal "THE [[=art]] IS UNMADE. {SCHOLAR} PAID FOR YOUR WEAKNESS WITH A HAND, AND WILL NOT LET YOU APOLOGISE."
  do take "THE [[=adj]] [[=art]]"
  do realm peace land rival
  do reward rich
  do mark burden_unmade
  do remember scholar "DON'T LOOK AT THE STUMP LIKE THAT. IT WAS ALWAYS GOING TO COST SOMEONE SOMETHING. I CHOSE WHAT."
  do fact "{SCHOLAR} OF {HOME} GAVE A HAND TO THE FIRE OF {FORGE} TO UNMAKE A CURSED THING. THE KINGDOMS MADE PEACE THAT SEASON."

stage sworn_throws
  end success
  say "{SWORN} LOOKED AT IT A LONG TIME. THEN {SWORN.HE} SAID {SWORN.HIS} BROTHER'S NAME, ONCE, AND THREW. WHEN THE FIRE DIED DOWN, {SWORN.HE} WAS STILL SAYING IT, BUT QUIETLY NOW, THE WAY YOU SAY A NAME AT A GRAVE."
  journal "{SWORN} OF {TOWN} THREW THE [[=adj]] [[=art]] INTO THE FIRE OF {FORGE.OLD}. PEACE HOLDS BETWEEN {LAND} AND {RIVAL}."
  do take "THE [[=adj]] [[=art]]"
  do realm peace land rival
  do reward great
  do mark burden_unmade
  do remember sworn "I DREAM ABOUT IT STILL. IN THE DREAM I KEEP IT. I WAKE UP GLAD EVERY TIME."
  do fact "{SWORN}, A CAPTAIN OF {TOWN}, HAD A KINGDOM IN {SWORN.HIS} HAND AT {FORGE} AND THREW IT IN THE FIRE."

stage sworn_crowned
  end success
  say "{SWORN} PUT IT ON. {SWORN.HE} DID NOT LOOK AT YOU AGAIN. A MONTH LATER {SWORN.HE} SAT IN {SEAT} AND NOBODY COULD SAY HOW, AND THE ARMIES OF {LAND} MARCHED ON {RIVAL} SINGING. YOU WERE NOT INVITED TO THE CROWNING."
  journal "{SWORN} WEARS THE [[=adj]] [[=art]] AND THE CROWN OF {LAND}, AND {LAND} MAKES WAR ON {RIVAL}."
  do take "THE [[=adj]] [[=art]]"
  do realm succession land sworn
  do realm war land rival
  do reward fair
  do mark burden_crowned
  do remember scholar "YOU GAVE IT TO {SWORN.HIM}. I WROTE THAT DOWN TOO. I WRITE EVERYTHING DOWN."
  do fact "{SWORN} OF {TOWN} TOOK THE THRONE OF {LAND} WEARING A [[=art]] FROM {FORGE}. THE WAR WITH {RIVAL} BEGAN THE SAME WEEK."

stage buried
  end success
  say "YOU LAID IT UNDER THE FIRE-PIT AND PILED THE STONES HIGH. {SCHOLAR} FILLED NINE PAGES WITH THE SMITHS' MARKS AND GAVE YOU A COPY. IT IS STILL DOWN THERE, AND STILL AWAKE. BUT IT IS VERY FAR FROM ANYONE NOW."
  journal "THE [[=art]] LIES BURIED UNBROKEN BENEATH {FORGE}. THE CRAFT OF ITS SMITHS IS SAVED, AND SO, PERHAPS, IS IT."
  do take "THE [[=adj]] [[=art]]"
  do secret forge
  do reward fair
  do mark burden_buried
  do remember scholar "SOMETIMES I READ THE MARKS AND I THINK I UNDERSTAND THEM. THEN I PUT THE BOOK AWAY FOR A WEEK."
  do fact "THEY SAY SOMETHING OLD IS BURIED UNBROKEN UNDER {FORGE}, AND THAT THE SMITHS OF {LAND} HAVE NEW SECRETS."

stage worn
  end fail
  say "YOU PUT IT ON. THE WORLD WENT VERY CLEAR AND VERY SMALL. {SCHOLAR} WAS SAYING SOMETHING; IT DID NOT MATTER. THE ROAD BACK WAS LONG, AND YOU DO NOT REMEMBER IT, AND SOMEWHERE ON IT THE THING CAME OFF AND WAS GONE."
  journal "YOU PUT ON THE [[=adj]] [[=art]] AT {FORGE}. NOW IT IS GONE, AND {LAND} MARCHES ON {RIVAL}."
  do take "THE [[=adj]] [[=art]]"
  do realm war land rival
  do mark burden_worn
  do remember scholar "...I DON'T KNOW YOU. I KNEW SOMEONE WHO LOOKED LIKE YOU, ONCE. AT {FORGE}."
  do fact "A STRANGER WALKED OUT OF {FORGE} WEARING A [[=art]], AND WAR CAME BETWEEN {LAND} AND {RIVAL} BEHIND THEM."
)SAGA";

// ---- arc 1: the first night
const char* const kDeadFollow = R"SAGA(
stage %rise
  goal kill 4 undead in home
  then %offer
  say "BEHIND YOU, IN THE DARK OF {HOME}, SOMETHING DRAGS ITS FEET. THEN SOMETHING ELSE. THEN MANY THINGS."
  journal "THE DEAD OF {HOME} HAVE RISEN TO TAKE BACK WHAT THEIR KING HELD. FIGHT YOUR WAY OUT."
stage %offer
  talk scholar
  say "THERE ARE MORE OF THEM. I CAN HEAR IT OFFERING, {PLAYER}. PUT ME ON, IT SAYS, AND THEY KNEEL. THEY WERE ITS SOLDIERS ONCE, AND SOLDIERS REMEMBER WHO THEY OBEY."
  opt "PUT IT ON. JUST ONCE." -> %command
  opt "NO. WE CUT OUR WAY OUT." -> %cut
stage %command
  talk scholar
  say "THEY KNELT. EVERY ONE OF THEM, IN THE DARK, LIKE A FIELD OF BARLEY IN THE WIND. AND YOU SMILED. I SAW YOU. TAKE IT OFF. TAKE IT OFF NOW, AND DON'T DO THAT AGAIN."
  do add burden 1
  do remember scholar "YOU SMILED, IN THE BARROW. I KEEP THINKING ABOUT THAT."
  opt "IT WORKED, DIDN'T IT?" -> @next
  opt "I WON'T. I SWEAR IT." -> @next
stage %cut
  talk scholar
  say "...YOU SAID IT OUT LOUD. GOOD. THAT'S GOOD. I SAW ONE OF THEM STOP AND LOOK AT YOU, LIKE IT WAS DISAPPOINTED. <<relief>> COME ON. THE STARS ARE OUT. I HAVE NEVER BEEN SO GLAD OF STARS."
  do fame 1
  opt "WHICH WAY TO {FORGE}?" -> @next
)SAGA";

const char* const kKnifeVillage = R"SAGA(
role %hamlet site village near
role %host resident any at %hamlet
stage %road
  goal goto %hamlet
  then %host1
  do moves scholar %hamlet
  say "{SCHOLAR} WANTS A ROOF FOR THE NIGHT. THE THING IN YOUR PACK WANTS, TOO, BUT IT WILL NOT SAY WHAT."
  journal "THE FIRST NIGHT ON THE ROAD: SHELTER AT {%hamlet}, {%hamlet.DIR}, BEFORE THE DARK."
stage %host1
  talk %host
  say "THE LOFT'S YOURS. MIND THE DOGS, THEY'VE BEEN STRANGE ALL WEEK. EVERYONE HAS. I DREAMED LAST NIGHT I CUT MY NEIGHBOUR'S THROAT OVER A DEAD KING'S [[=art]]. FUNNY THING TO DREAM. WHAT'S IN THE PACK?"
  opt "BREAD. NOTHING ELSE." -> %night
  opt "SOMETHING THAT GIVES BAD DREAMS." -> %truth
stage %truth
  talk %host
  say "...THEN TAKE IT AND GO, FRIEND. NOW, BEFORE DARK. I'D RATHER NOT FIND OUT WHAT I'D DO WITH A KNIFE IN THE NIGHT. THERE'S A BARN ON THE HILL ROAD. NOBODY SLEEPS THERE BUT OWLS."
  do remember %host "YOU WERE HONEST WITH ME, THAT NIGHT. I SLEPT LIKE A STONE AFTER YOU WENT. FIRST TIME IN A WEEK."
  do befriend %host
  opt "THANK YOU. TRULY." -> %flee
stage %night
  talk scholar
  say "THEY'RE AT THE DOOR. HALF {%hamlet}, WITH LAMPS AND HAYFORKS, AND NOT ONE OF THEM KNOWS WHY. IT CALLED THEM. WE CAN GO OUT THE BACK, OR YOU CAN SHOW THEM WHAT IT IS AND ORDER THEM HOME. IT WOULD WORK. THAT'S WHAT FRIGHTENS ME."
  opt "OUT THE BACK. NOW." -> %flee
  opt "GO HOME, ALL OF YOU!" -> %order
stage %order
  talk %host
  say "WE WENT HOME. ALL OF US, LIKE SHEEP DOWN A LANE. I DON'T REMEMBER DECIDING TO. I DON'T REMEMBER ANYTHING BUT YOUR VOICE. DON'T COME BACK TO {%hamlet}. PLEASE."
  do add burden 1
  do remember %host "YOU. YOU TOLD US TO GO HOME AND WE WENT. GET AWAY FROM MY DOOR."
  opt "I'M SORRY." -> @next
  opt "IT KEPT YOU ALIVE." -> @next
stage %flee
  goal wait 1
  then @next
  say "YOU SLEEP BADLY AND MOVE ON BEFORE DAWN. BEHIND YOU, THE DOGS OF {%hamlet} HOWL UNTIL YOU ARE OVER THE HILL."
  journal "OUT OF {%hamlet} BEFORE DAWN. THE THING SULKS IN YOUR PACK LIKE A DOG DENIED A BONE."
)SAGA";

// ---- arc 2: the sworn
const char* const kSwornFord = R"SAGA(
stage %go
  goal goto town
  then %meet
  journal "THE ROAD TO {FORGE} RUNS THROUGH {TOWN}. {SCHOLAR} SAYS A CAPTAIN THERE, {SWORN}, OWES {SCHOLAR.HIM} A LIFE."
stage %meet
  talk sworn
  say "{SCHOLAR}'S LETTER SAID YOU CARRY SOMETHING OUT OF {HOME}. I'VE BURIED TOO MANY AT THE {RIVAL} BORDER, MY BROTHER FIRST. IF HALF WHAT THEY SAY IS TRUE, IT COULD END THAT WAR IN A DAY. I'LL WALK WITH YOU. I'LL ASK FOR NOTHING. YET."
  opt "WALK WITH US, THEN." -> %oath
  opt "NO. YOU WANT IT TOO MUCH." -> %alone
stage %oath
  talk sworn
  say "THEN HEAR ME SWEAR IT, BY {SWORN.GOD} AND MY BROTHER'S GRAVE: I GUARD THE BEARER, NOT THE BURDEN. IF I EVER REACH FOR IT, CUT MY HAND OFF. <<oath>> NOW. THE FORD AT DUSK. SOMETHING HAS BEEN FOLLOWING YOU, AND IT ISN'T ALIVE."
  do set sworn 1
  opt "THEN WE CROSS AT DUSK." -> %ford
stage %alone
  talk sworn
  say "YOU'RE A FOOL. AN HONEST ONE, BUT A FOOL. I'LL BE ON THE ROAD BEHIND YOU ANYWAY. NOT FOR IT: FOR YOU. YOU'LL WANT A SWORD AT THE FORD TONIGHT, AND I'LL BE THERE WHETHER YOU WANT ME OR NOT."
  do set sworn 0
  opt "...SUIT YOURSELF." -> %ford
stage %ford
  goal kill 3 undead
  then %after
  say "THE DEAD OF {HOME} HAVE KEPT PACE WITH YOU FOR THREE DAYS. AT THE FORD BELOW {TOWN} THEY STAND IN THE WATER AND WAIT."
  journal "THE DEAD FOLLOW THE [[=art]]. THEY WAIT AT THE FORD BELOW {TOWN}. CROSS IT."
stage %after
  talk sworn
  say "...YOU FIGHT LIKE SOMEONE WITH NOTHING TO LOSE. OR LIKE SOMEONE CARRYING A THING THAT WON'T LET THEM DIE. I HOPE IT'S THE FIRST. YOU SHAKE AFTER, THOUGH. GOOD. THE ONES WHO DON'T SHAKE WORRY ME."
  opt "IT'S THE FIRST." -> @next
  opt "I DON'T KNOW ANY MORE." -> @next
)SAGA";

const char* const kSwornGate = R"SAGA(
stage %go
  goal goto town
  then %stop
  journal "THE ROAD TO {FORGE} RUNS THROUGH {TOWN}, AND THE {LORD.TITLE}'S MEN ARE SEARCHING EVERYONE AT ITS GATE."
stage %stop
  talk sworn
  say "HALT. BY ORDER OF THE {LORD.TITLE}, NOTHING LEAVES {TOWN} UNSEARCHED. ...THAT. NO, DON'T SHOW ME. I CAN FEEL IT FROM HERE, LIKE HEAT OFF A FORGE. THE {LORD.TITLE}'S MEN WOULD HANG YOU FOR IT AND KEEP IT. I'M MEANT TO HAND YOU TO THEM."
  opt "THEN HAND ME OVER." -> %cells
  opt "COME WITH US INSTEAD." -> %turn
  opt "LET US PASS." check level 6 -> %pass else %cells
stage %cells
  talk sworn
  say "I COULDN'T. I WALKED YOU TO THE CELLS AND I WALKED YOU OUT THE BACK, AND I DON'T EVEN KNOW WHY. {TOWN} WILL CALL ME TRAITOR BY MORNING. SO. WHERE ARE WE GOING?"
  do set sworn 1
  do remember sworn "I WAS A CAPTAIN OF {TOWN} ONCE. THEN YOU WALKED THROUGH MY GATE."
  opt "TO {FORGE}. TO BREAK IT." -> %flight
stage %turn
  talk sworn
  say "WITH YOU? ...MY BROTHER DIED ON THE {RIVAL} BORDER. IF THAT THING CAN END THEIR KINGDOM, I WANT TO BE THERE WHEN IT DOES. I'LL COME. FOR MY OWN REASONS. YOU SHOULD KNOW THAT ABOUT ME BEFORE WE START."
  do set sworn 1
  opt "I'D RATHER KNOW THAN NOT." -> %flight
stage %pass
  talk sworn
  say "YOU'VE THE LOOK OF SOMEONE WHO'D WIN THAT FIGHT. GO, THEN. I'LL SAY I NEVER SAW YOU. BUT I'LL FOLLOW, AND NOT TO ARREST YOU. I WANT TO SEE WHAT YOU DO WITH IT."
  do set sworn 0
  opt "FOLLOW, THEN." -> %flight
stage %flight
  goal wait 1
  then @next
  say "THE {LORD.TITLE}'S RIDERS QUARTER THE ROAD BEHIND YOU. YOU KEEP TO THE HEDGES AND THE DRY DITCHES, AND {SCHOLAR} DOES NOT COMPLAIN ONCE."
  journal "OUT OF {TOWN} BY NIGHT, WITH THE {LORD.TITLE}'S RIDERS ON THE ROAD. {FORGE} IS STILL FAR."
)SAGA";

// ---- arc 3: the temptation
const char* const kOldBearer = R"SAGA(
role %cave site cave near
role %ancient person male at %cave
stage %go
  goal goto %cave
  then %meet
  do moves scholar %cave
  journal "{SCHOLAR}'S MAPS SAY THE SAFE ROAD TO {FORGE} PASSES {%cave}, WHERE A HERMIT HAS KEPT A FIRE LONGER THAN ANYONE REMEMBERS."
stage %meet
  talk %ancient
  say "I FELT IT WAKE. I CARRIED IT ONCE, FROM {FORGE.OLD} WHEN IT BURNED, TO THE TOMB AT {HOME}. {FORGE.YEARS} YEARS AGO. IT DOES NOT LET ITS BEARERS DIE. GIVE IT TO ME. I KNOW ITS WAYS. I WILL KEEP IT SAFE."
  opt "HOW DID YOU CARRY IT SO LONG?" -> %how
  opt "TAKE IT, THEN." -> %give
  opt "NO. IT GOES TO THE FORGE." -> %refuse
stage %how
  talk %ancient
  say "BY NEVER PUTTING IT ON. NOT ONCE IN ALL THOSE MILES. I WAS PROUD OF THAT. THEN I LAID IT IN THE DEAD KING'S HANDS, AND I FOUND I COULDN'T WALK AWAY. I HAVE LIVED A DAY'S WALK FROM IT EVER SINCE. LISTENING."
  opt "THEN YOU KNOW I CAN'T GIVE IT UP." -> %refuse
  opt "TAKE IT. YOU'VE EARNED REST." -> %give
stage %give
  talk scholar
  say "NO! LOOK AT HIS HANDS, HOW THEY SHAKE. HE DOESN'T WANT TO KEEP IT SAFE. HE WANTS TO KEEP IT. {PLAYER}, IF YOU HAND IT OVER WE NEVER SEE IT AGAIN, AND NEITHER DOES THE FORGE."
  opt "...YOU'RE RIGHT. I'M SORRY, OLD ONE." -> %refuse
  opt "HE'S EARNED IT." -> %kept
stage %kept
  talk %ancient
  say "MINE. MINE AGAIN, AFTER ALL THIS... NO. NO, WAIT. TAKE IT BACK. I CAN HEAR WHAT IT WANTS ME TO DO WITH IT, AND I AM TOO OLD TO SAY NO TWICE. TAKE IT, AND DON'T COME THIS WAY AGAIN."
  do add burden 1
  do remember %ancient "YOU OFFERED IT TO ME. NOBODY HAD EVER OFFERED IT TO ME. I DREAM OF IT AGAIN NOW."
  opt "REST, OLD ONE." -> %rest
stage %refuse
  talk %ancient
  say "...YES. THAT IS WHAT I WOULD HAVE SAID, ONCE. GO, THEN. THE SAFE ROAD IS THE LONG ONE, BY THE RIVER. AND WHEN IT IS BROKEN... I THINK I WILL FINALLY BE TIRED. THANK YOU FOR THAT. <<blessing>>"
  do remember %ancient "IS IT DONE YET? NO. I WOULD KNOW. I WOULD BE TIRED."
  opt "REST, OLD ONE." -> %rest
stage %rest
  goal wait 1
  then @next
  say "YOU LOOK BACK ONCE FROM THE RIVER ROAD. THE HERMIT'S SMOKE IS A THIN GREY THREAD, AND THEN IT IS GONE."
  journal "{%ancient}'S FIRE IS BEHIND YOU. THE LONG ROAD BY THE RIVER LEADS ON TOWARD {FORGE}."
  do fact "A HERMIT OF {%cave} IS SAID TO BE OLDER THAN THE FALL OF {FORGE.OLD}, AND TO BE WAITING FOR SOMETHING."
)SAGA";

const char* const kCouncil = R"SAGA(
stage %summons
  goal goto seat
  then %court
  do moves sworn seat
  journal "RIDERS OF THE {LORD.TITLE} FOUND YOU ON THE ROAD. {LORD} WILL SEE THE BEARER OF THE [[=art]] AT {SEAT}. IT IS NOT A REQUEST."
stage %court
  talk lord
  say "SO THIS IS WHAT KEEPS MY SPIES AWAKE. {RIVAL} MASSES ON MY BORDER, AND YOU WALK PAST MY THRONE WITH A WEAPON THAT COULD END THEM, TO DROP IT IN A FIRE? GIVE IT TO THE CROWN. I WILL USE IT ONCE, AND WELL, AND THEN I WILL BREAK IT MYSELF."
  opt "IT USES YOU. NOT THE OTHER WAY." -> %argue
  opt "TAKE IT, {LORD.TITLE}." -> %yield
  opt "NO. IT GOES TO THE FORGE." check level 8 -> %leave else %argue
stage %argue
  talk lord
  say "PRETTY. DID THE SCHOLAR TEACH YOU THAT? ...AND YET I HAVE HEARD A VOICE IN MY HEAD SINCE YOU CAME THROUGH MY GATE, AND IT DOES NOT SOUND LIKE MINE. GO. QUICKLY, BEFORE I HEAR IT BETTER."
  opt "THANK YOU, {LORD.TITLE}." -> @next
  opt "BREAK IT YOURSELF? NO ONE COULD." -> @next
stage %leave
  talk lord
  say "...GO, THEN. I WILL NOT HAVE IT SAID I TOOK IT BY FORCE FROM SOMEONE THE STREETS ALREADY SING ABOUT. BUT IF {RIVAL} CROSSES MY BORDER WHILE YOU DAWDLE, I WILL REMEMBER THIS DAY."
  do rep land 2
  opt "BY THEN IT WILL BE ASH." -> @next
stage %yield
  talk lord
  say "WISE. ...IT'S LIGHTER THAN I THOUGHT. AND WARM. YOU MAY GO. THE CROWN THANKS YOU AND WILL REMEMBER WHO BROUGHT IT. GUARDS, SEE THE SCHOLAR OUT. GENTLY. FOR NOW."
  do take "THE [[=adj]] [[=art]]"
  do rep land 4
  opt "...WHAT HAVE I DONE?" -> %regret
stage %regret
  talk sworn
  say "YOU GAVE IT A THRONE. THAT'S ALL IT EVER WANTED. ...SO I TOOK IT BACK. OFF THE {LORD.TITLE}'S OWN PILLOW, WHILE {LORD.HE} SLEPT. IT WAS EASY. TOO EASY. TAKE IT, QUICK, BEFORE I DECIDE NOT TO. AND RUN."
  do give "THE [[=adj]] [[=art]]" [[=art]]
  do set angered 1
  do rep land -8
  do remember sworn "I STOLE FROM A KING FOR YOU. I'D DO IT AGAIN. I THINK I'D DO WORSE."
  opt "RUN." -> @next
)SAGA";

// ---- arc 4: the breaking
const char* const kOathBroke = R"SAGA(
stage %camp
  goal enter forge
  then %wake
  do moves sworn forge
  do moves scholar forge
  say "{SWORN} TAKES THE LAST WATCH AGAIN. {SWORN.HE} ALWAYS TAKES THE LAST WATCH NOW, AND {SWORN.HIS} EYES ARE RED IN THE MORNINGS."
  journal "{FORGE}, {FORGE.DIR}: THE RUIN OF {FORGE.OLD}, WHERE THE [[=art]] WAS MADE. {SWORN} HAS STOPPED SLEEPING."
stage %wake
  talk sworn
  say "THE LAST NIGHT, IN THE GATEHOUSE OF {FORGE.OLD}. DON'T REACH FOR YOUR BLADE. I HAVE IT, LOOK, AND IT'S SO QUIET NOW. IT ISN'T SHOUTING. IT'S TELLING ME ABOUT MY BROTHER. HE'D BE THIRTY THIS SPRING. IT SAYS IT CAN MAKE {RIVAL} PAY. IT SAYS YOU'D ONLY BREAK IT."
  opt "GIVE IT BACK. YOU SWORE." -> %oath
  opt "YOUR BROTHER WOULD BE ASHAMED." -> %shame
  opt "THEN WE FIGHT FOR IT." -> %fight
stage %oath
  talk sworn
  say "...I DID. I SWORE. I CAN'T REMEMBER WHAT BY. HERE. TAKE IT, TAKE IT BEFORE I... TIE MY HANDS AT NIGHT FROM NOW ON. I MEAN IT. I'LL HOLD THE ROPE OUT TO YOU MYSELF."
  do set sworn 1
  do remember sworn "YOU REMINDED ME WHAT I SWORE BY. I'D FORGOTTEN. I'M NOT PROUD OF THAT NIGHT."
  opt "WE FINISH THIS TOGETHER." -> @next
stage %shame
  talk sworn
  say "...ASHAMED. YES. HE WAS ALWAYS THE BETTER OF US. HERE. I'M GOING. DON'T FOLLOW. WHEN IT BREAKS I'LL KNOW: I'LL FEEL IT GO OUT OF THE WORLD. <<farewell>>"
  do set sworn 0
  do remember sworn "I WALKED HOME FROM THAT CAMP ALONE. IT WAS THE LONGEST WALK OF MY LIFE, AND THE BEST."
  opt "GO WELL, {SWORN}." -> @next
stage %fight
  talk scholar
  say "STOP! BOTH OF YOU! ...THERE. I HIT {SWORN.HIM} WITH THE LAMP. {SWORN.HE}'LL LIVE. TAKE IT. AND DON'T LOOK AT ME LIKE THAT, YOU HAD YOUR HAND ON YOUR KNIFE. IT WANTED THAT, TOO. IT WANTS ALL OF IT."
  do set sworn 0
  do add burden 1
  opt "...YOU'RE RIGHT. IT DID." -> @next
)SAGA";

const char* const kScholarPages = R"SAGA(
stage %camp
  goal enter forge
  then %pages
  do moves sworn forge
  do moves scholar forge
  say "{SCHOLAR} WRITES BY THE LAST OF THE FIRE. {SCHOLAR.HE} COVERS THE PAGE WHEN YOU STIR, THE WAY A CHILD HIDES A STOLEN SWEET."
  journal "{FORGE}, {FORGE.DIR}: THE RUIN OF {FORGE.OLD}, WHERE THE [[=art]] WAS MADE. {SCHOLAR} WRITES WHILE YOU SLEEP."
stage %pages
  talk scholar
  say "YOU READ IT. ...FINE. YES. IT'S BEEN TELLING ME HOW IT WAS MADE. EVERY STEP: THE ALLOY, THE WORDS. NOBODY HAS KNOWN THAT FOR {FORGE.YEARS} YEARS. IF WE BREAK IT, THAT DIES TOO. ISN'T THAT A KIND OF MURDER?"
  opt "WHAT ELSE DID IT TELL YOU?" -> %more
  opt "BURN THE NOTEBOOK." -> %burn
  opt "KEEP THE NOTES. BREAK THE THING." -> %keep
stage %more
  talk scholar
  say "THAT I COULD MAKE ANOTHER. A BETTER ONE, THAT WOULD ANSWER ONLY TO ME. I LAUGHED. THEN I STOPPED LAUGHING, BECAUSE I'D ALREADY WORKED OUT WHERE I'D GET THE IRON."
  opt "BURN THE NOTEBOOK." -> %burn
  opt "KEEP THE NOTES. BREAK THE THING." -> %keep
stage %burn
  talk scholar
  say "THERE. ASH. {FORGE.YEARS} YEARS OF SILENCE, AND I HAD IT FOR A FORTNIGHT. ...THANK YOU. I THINK I WOULD HAVE DONE IT, YOU KNOW. MADE ANOTHER. I THINK I'D HAVE BEEN GOOD AT IT."
  do set lore 0
  do remember scholar "I STILL REMEMBER THE FIRST LINE OF WHAT I BURNED. I SAY SOMETHING ELSE OUT LOUD WHEN IT COMES."
  opt "YOU'D HAVE BEEN TERRIBLE AT IT." -> @next
  opt "SLEEP. I'LL WATCH." -> @next
stage %keep
  talk scholar
  say "THE NOTES AND NOT THE THING. KNOWLEDGE WITHOUT THE WANTING. ...IT'S QUIETER IN MY HEAD ALREADY. OR I'VE STOPPED LISTENING. I'M NOT SURE THOSE ARE DIFFERENT. <<doubt>>"
  do set lore 1
  do add burden 1
  opt "WE'LL SEE AT THE FORGE." -> @next
)SAGA";

}  // namespace

void addBurdenArcs(std::vector<Archetype>& v) {
  Archetype a;
  a.source = Source::Tolkien;
  a.tier = 3;
  a.needs = N_RUIN | N_KINGDOM;

  a.id = "bu_dead_follow"; a.name = "THE DEAD WHO FOLLOW";
  a.themes = TH_TEMPTATION | TH_BURDEN | TH_COURAGE; a.body = kDeadFollow; v.push_back(a);
  a.id = "bu_knife_village"; a.name = "THE VILLAGE THAT DREAMED OF KNIVES";
  a.themes = TH_TEMPTATION | TH_HOSPITALITY | TH_BURDEN; a.body = kKnifeVillage; v.push_back(a);
  a.id = "bu_sworn_ford"; a.name = "THE OATH AT THE FORD";
  a.themes = TH_LOYALTY | TH_GRIEF | TH_FRIENDSHIP; a.body = kSwornFord; v.push_back(a);
  a.id = "bu_sworn_gate"; a.name = "THE CAPTAIN AT THE GATE";
  a.themes = TH_LOYALTY | TH_BETRAYAL | TH_FRIENDSHIP; a.body = kSwornGate; v.push_back(a);
  a.id = "bu_old_bearer"; a.name = "THE BEARER BEFORE YOU";
  a.themes = TH_MERCY | TH_TEMPTATION | TH_BURDEN; a.body = kOldBearer; v.push_back(a);
  a.id = "bu_council"; a.name = "THE COUNCIL OF THE CROWN";
  a.themes = TH_TEMPTATION | TH_KINGSHIP | TH_PRIDE; a.body = kCouncil; v.push_back(a);
  a.id = "bu_oath_broke"; a.name = "THE NIGHT THE OATH BROKE";
  a.themes = TH_BETRAYAL | TH_REDEMPTION | TH_LOYALTY; a.body = kOathBroke; v.push_back(a);
  a.id = "bu_scholar_pages"; a.name = "WHAT THE SCHOLAR WROTE DOWN";
  a.themes = TH_TEMPTATION | TH_PRIDE | TH_FRIENDSHIP; a.body = kScholarPages; v.push_back(a);
}

CampaignPlan burdenPlan() {
  CampaignPlan p;
  p.id = "burden";
  p.name = "THE BURDEN ACROSS KINGDOMS";
  p.source = Source::Tolkien;
  p.themes = TH_BURDEN | TH_TEMPTATION | TH_FRIENDSHIP | TH_SACRIFICE | TH_MERCY;
  p.needs = N_RUIN | N_KINGDOM | N_RIVAL | N_CAPITAL;
  p.head = kHead;
  p.arcs = {ArcSlot{{"bu_dead_follow", "bu_knife_village"}}, ArcSlot{{"bu_sworn_ford", "bu_sworn_gate"}},
            ArcSlot{{"bu_old_bearer", "bu_council"}}, ArcSlot{{"bu_oath_broke", "bu_scholar_pages"}}};
  p.finale = kFinale;
  return p;
}

}  // namespace camp
}  // namespace saga
}  // namespace story
