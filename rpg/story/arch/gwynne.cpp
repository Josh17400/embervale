// M6b "Sagas": archetypes, John Gwynne-like themes (never names, characters, places, invented terms or plots). VOICE
// lane. The template markup and role conventions: rpg/story/saga.h.
//
// Only the shapes and feelings are borrowed: a war where both sides believe they serve the light; truth and courage in
// an oath-sworn band and the shield wall that holds; the bond of a hero and a great beast raised from a pup; the grudge
// of a dying elder race whose stones the kingdoms quarry; the mentor who betrays, or the betrayer who was right; two old
// hosts fighting their war through mortals; the enemy's own reasons; the hunted band that gathers the broken; and the
// vengeance that costs everything. Every name, band and host here is our own.
//
//   false_light      TWO BANNERS OF LIGHT        two champions of the same god; one heals the sick for coin
//   shield_wall      THE SHIELD WALL AT THE FORD nine sworn shields, forty raiders, and the youngest wants to run home
//   pit_hound        THE BEAST RAISED FROM A PUP a hound (or a bear) raised by hand, taken to the fighting pits
//   giant_stones     THE GIANT'S GRUDGE          the last of the tall folk throws boulders at the quarry on his kin's graves
//   master_traitor   THE MASTER WHO TURNED       the master who stole the seal, and why
//   two_hosts        THE WINGED AND THE SCORCHED two strangers want the relic a child found; each names the other enemy
//   enemy_reasons    THE RAIDER'S REASONS        the raider chief who burned the mill, and the night the levy burned his
//   hunted_band      THE HUNTED BAND             a band of the broken on the run, and a stray who asks to join
//   last_vengeance   VENGEANCE AT ANY PRICE      a giver who has sold everything for one death
#include <vector>
#include "rpg/story/arch/arch_tables.h"

namespace story {
namespace saga {
namespace arch {

namespace {

const char* const kFalseLight = R"SAGA(
title [[TWO BANNERS OF LIGHT|THE HEALER AND THE BEGGAR-PROPHET|WHICH OF THEM IS FALSE]]
hook npc any
pitch "[[pitch=WHY IS THE SQUARE SO CROWDED?|WHO IS THE ONE IN WHITE?|WHAT ARE THEY ARGUING ABOUT?]]"
hint "[[~pitch|TWO OF THEM, BOTH SWEARING BY THE SAME GOD, BOTH SAYING THE OTHER WILL BURN THE WORLD. ONE OF THEM IS RIGHT.|ONE HEALS THE SICK IN THE SQUARE. THE OTHER SAYS THE SICK WERE NEVER SICK.|TWO OF THEM, BOTH SWEARING BY THE SAME GOD, BOTH SAYING THE OTHER WILL BURN THE WORLD. ONE OF THEM IS RIGHT.]]"
role giver giver
role home site home
role bright person male at home
role grey person female at home
role camp site camp near
role survivor person [[male|female]] at camp
var raided 0
slot t1 -> raid rival betrayal world
slot t2 -> unmask prophecy mercy betrayal

stage start
  talk giver
  say "TWO HAVE COME TO {HOME} RAISING SWORDS FOR {GIVER.GOD}. {BRIGHT}, IN [[WHITE ENAMEL|GILT MAIL|A SUN-BRIGHT CLOAK]], HEALS THE SICK IN THE SQUARE. {GREY}, IN RAGS, SAYS HE IS A LIAR AND WILL DROWN US ALL IN BLOOD. <<doubt>>"
  opt "I'LL HEAR {BRIGHT} OUT." -> bright_talk
  opt "I'LL HEAR {GREY} OUT." -> grey_talk
  opt "LET THE PRIESTS SORT IT OUT." -> refused

stage refused
  end fail
  journal "YOU WOULD NOT JUDGE BETWEEN {BRIGHT} AND {GREY} IN {HOME}."
  do remember giver "HALF THE YOUNG ONES RODE OFF WITH {BRIGHT}. THE OTHER HALF HIDE {GREY}. NOBODY EATS TOGETHER NOW."

stage bright_talk
  talk bright
  say "{GREY} HIDES A WARBAND OF HERETICS AT {CAMP}. THEY HAVE BURNED TWO SHRINES. RIDE WITH MY MEN AND END THEM, AND {GIVER.GOD} WILL KNOW YOUR NAME. <<oath:devotion>>"
  opt "I RIDE WITH YOU." -> @t1
  opt "FIRST I'LL HEAR {GREY}." -> grey_talk

stage grey_talk
  talk grey
  say "HE HEALS? HE PAYS BEGGARS TO LIMP IN AND WALK OUT. AT {CAMP} I HIDE THE ONES HE CALLS HERETICS: THE SHRINE-KEEPERS WHO WOULD NOT KNEEL TO HIM. I CANNOT PROVE IT. GO AND LOOK. <<plea:devotion>>"
  opt "I'LL GO AND LOOK." -> look
  opt "I'LL RIDE WITH {BRIGHT}." -> @t1

stage look
  goal goto camp
  then camp_talk
  journal "{GREY} SAYS THE HERETICS AT {CAMP} ARE SHRINE-KEEPERS HUNTED BY {BRIGHT}. GO AND SEE."

stage camp_talk
  talk survivor
  say "YOU'RE NOT ONE OF HIS. GOOD. WE KEPT THE SHRINES. HE BURNED THEM HIMSELF AND SAID WE DID IT. HE WANTS THE TEMPLE GOLD AND {HOME}'S SWORDS, AND HE WILL HAVE BOTH BY MIDSUMMER. <<grief:fear>>"
  opt "THEN I'LL UNMASK HIM." -> @t2
  opt "I'LL HIDE YOU DEEPER." -> hidden_end

stage raid
  goal kill 3 bandit in camp
  then raid_after
  do set raided 1
  journal "{BRIGHT} SENDS YOU WITH HIS RIDERS AGAINST THE HERETICS OF {CAMP}."

stage raid_after
  talk survivor
  say "WHY? WE KEPT THE SHRINES. HE BURNED THEM HIMSELF, AND NOW YOU HAVE KILLED THE LAST WHO KNEW IT. HE LAUGHED WHEN HE SENT YOU, DIDN'T HE? HE ALWAYS LAUGHS. <<curse>>"
  opt "THEN I'LL UNMASK HIM." -> @t2
  opt "I CHOSE MY SIDE." -> blind_end

stage unmask
  goal goto home
  then confront
  journal "{BRIGHT} IS NO CHAMPION OF {GIVER.GOD}. GO BACK TO {HOME} AND SAY SO."

stage confront
  talk bright
  say "BACK FROM {CAMP}? THEN YOU KNOW. AND WHO WILL {HOME} BELIEVE: MY MIRACLES, OR YOUR MUD? THINK CAREFULLY. I AM VERY GENEROUS TO PEOPLE WHO THINK CAREFULLY. <<threat:pride>>"
  opt "THE TRUTH. IN THE SQUARE." check fame 1 -> exposed_end else scorned_end
  opt "LEAVE TONIGHT, OR I TALK." -> banished_end

stage hidden_end
  end success
  say "YOU MOVED THE SHRINE-KEEPERS DEEPER INTO THE HILLS BY NIGHT. {BRIGHT} STILL HEALS IN THE SQUARE. {GREY} STILL SHOUTS. NOBODY HAS BURNED ANYONE YET."
  journal "YOU HID THE SHRINE-KEEPERS OF {CAMP} FROM {BRIGHT}. THE LIE STILL WALKS IN {HOME}."
  do reward fair
  do remember grey "THEY'RE SAFE. HE'S STILL HERE. BOTH OF THOSE ARE TRUE, AND I HATE THE SECOND ONE. <<thanks:devotion>>"

stage blind_end
  end success
  say "{BRIGHT} PRAISED YOU IN THE SQUARE AND THE CROWD CHEERED YOUR NAME. THAT NIGHT {GREY} WAS GONE, AND SO WAS THE TEMPLE GOLD, AND {BRIGHT} WAS FIRST TO SAY WHO TOOK IT."
  journal "YOU RODE FOR {BRIGHT} AGAINST {CAMP}. YOU KNOW NOW WHAT HE IS. YOU SAID NOTHING."
  do reward fair
  do mark served_the_false_light
  do remember giver "{BRIGHT} SAYS YOU'RE A HERO. YOU DON'T LOOK LIKE ONE. YOU LOOK LIKE ME, WHEN I DON'T SLEEP."
  do fact "THEY SAY THE SHRINES NEAR {HOME} WERE BURNED BY HERETICS. THE ONES WHO KNOW BETTER ARE DEAD."

stage exposed_end
  end success
  say "YOU SAID IT ALL IN THE SQUARE, AND {GREY} STOOD BESIDE YOU, AND {BRIGHT} SMILED UNTIL THE PAID BEGGARS STARTED TALKING. HE LEFT {HOME} AT A RUN, WITHOUT HIS WHITE CLOAK."
  journal "YOU UNMASKED {BRIGHT} IN THE SQUARE OF {HOME}. THE FALSE CHAMPION FLED."
  do reward rich
  do hide bright
  do remember grey "THEY CALL ME PROPHET NOW. I LIKED IT BETTER WHEN THEY THREW CABBAGES. <<saying>>"
  do fact "A FALSE CHAMPION OF {GIVER.GOD} WAS UNMASKED IN {HOME} AND FLED WITHOUT HIS CLOAK. IT HANGS IN THE TEMPLE NOW."

stage scorned_end
  end fail
  say "THEY LAUGHED. {BRIGHT} LAID HIS HANDS ON A LIMPING MAN AND THE MAN DANCED, AND THEY LAUGHED HARDER, AND SOMEONE THREW A STONE. AT YOU."
  journal "{HOME} DID NOT BELIEVE YOU ABOUT {BRIGHT}. NOT YET."
  do mark scorned_in_the_square
  do remember giver "I BELIEVE YOU. I'M THE ONLY ONE. FOR WHAT THAT'S WORTH, WHICH IS NOTHING."

stage banished_end
  end success
  say "{BRIGHT} LOOKED AT YOU A LONG TIME, THEN LAUGHED, AND WAS GONE BEFORE DAWN WITH TEN OF {HOME}'S YOUNG ONES BEHIND HIM. HE IS SOMEONE ELSE'S CHAMPION NOW."
  journal "{BRIGHT} LEFT {HOME} RATHER THAN FACE THE TRUTH. HE TOOK FOLLOWERS WITH HIM."
  do reward fair
  do hide bright
  do remember grey "YOU SENT HIM DOWN THE ROAD. THE ROAD HAS OTHER TOWNS ON IT. <<warning:devotion>>"
  do fact "A CHAMPION IN WHITE LEFT {HOME} IN THE NIGHT WITH TEN OF ITS YOUNG. HE PREACHES SOMEWHERE ELSE NOW."
)SAGA";

const char* const kShieldWall = R"SAGA(
title [[THE SHIELD WALL AT THE FORD|TRUTH AND COURAGE|NINE SHIELDS]]
hook npc any
pitch "[[pitch=WHY ARE YOU PAINTING SHIELDS?|WHO ARE THE NINE?|YOU LOOK LIKE A SWORN MAN.]]"
hint "[[~pitch|NINE SHIELDS TO REPAINT BEFORE THE FORD, AND MY HAND WON'T HOLD THE BRUSH STILL.|NINE OF US. TRUTH ON OUR LIPS, COURAGE IN OUR HANDS. THAT'S THE OATH. IT SOUNDS BETTER WHEN THERE ARE MORE THAN NINE.|THE FORD HOLDS IF THE WALL HOLDS. THE WALL HOLDS IF NOBODY RUNS.]]"
role giver giver
role home site home
role ford site camp near
role youngest person [[male|female]] at home
role raider foe male at ford
var stood 0
slot t1 -> wall betrayal price world rival
slot t2 -> captives mercy betrayal

stage start
  talk giver
  say "MY SWORN BAND, THE [[band=ASH SHIELDS|GREY OATH|HOLLOW SPEARS]], HOLDS THE FORD AT {FORD} AGAINST {RAIDER}'S REAVERS. WE ARE NINE. THEY ARE FORTY. AND {YOUNGEST}, MY YOUNGEST SHIELD, WANTS TO RUN HOME TO A DYING MOTHER. <<proverb>>"
  opt "LET {YOUNGEST.HIM} GO." -> release
  opt "AN OATH IS AN OATH." -> hold_talk
  opt "I'LL TAKE {YOUNGEST.HIS} PLACE." -> replace
  opt "NOT MY FORD." -> refused

stage refused
  end fail
  journal "YOU WOULD NOT STAND WITH THE [[=band]] AT {FORD}."
  do remember giver "WE HELD WITH EIGHT. WE HELD. TWO OF US WILL NOT HOLD ANYTHING AGAIN."

stage release
  talk youngest
  say "{GIVER} WOULD LET ME GO? ...THEN I'M GOING. NO. WAIT. IF I GO, THE WALL HAS A HOLE, AND THE HOLE IS NEXT TO MY SHIELD-BROTHER. <<grief:duty>>"
  opt "GO. I'LL STAND IN THE HOLE." -> replace
  opt "STAY, THEN. BY YOUR OWN CHOICE." -> @t1

stage hold_talk
  talk youngest
  say "AN OATH. YES. TRUTH AND COURAGE. AND MY MOTHER WILL DIE WITH NOBODY HOLDING HER HAND, AND THAT WILL BE THE TRUTH, AND I WILL NEED THE COURAGE. <<grief:duty>>"
  opt "HOLD THE WALL. THEN RIDE HOME." -> @t1
  opt "GO TO HER. I'LL STAND FOR YOU." -> replace

stage replace
  talk youngest
  say "YOU'D STAND IN MY PLACE? WITH STRANGERS? FOR A STRANGER'S MOTHER? ...I'LL BE BACK FOR THE NEXT ONE. I SWEAR IT. <<thanks:love>>"
  do set stood 1
  do remember youngest "YOU STOOD IN MY PLACE. SHE DIED WITH MY HAND IN HERS. I'LL STAND IN YOURS, ANY DAY YOU NAME."
  do mark stood_for_a_shield
  opt "GO. RUN." -> @t1

stage wall
  goal slay raider
  then captives_go
  say "SHIELDS LOCKED. TRUTH ON OUR LIPS, COURAGE IN OUR HANDS. <<oath>>"
  journal "THE [[=band]] HOLD THE FORD AT {FORD} AGAINST {RAIDER} AND FORTY REAVERS. STAND IN THE WALL. KILL {RAIDER}, AND THEY BREAK."

stage captives_go
  goal goto home
  then @t2
  journal "THE WALL HELD AT {FORD}. {RAIDER} IS DEAD. THE [[=band]] BRING PRISONERS HOME TO {HOME}."

stage captives
  talk giver
  say "WE HELD. GODS, WE HELD. AND WE TOOK [[SIX|SEVEN|FIVE]] OF THEM ALIVE, MOST OF THEM BOYS. THE BAND WANTS THEM HANGED ON THE FORD POSTS AS A WARNING. YOU STOOD IN THE WALL. YOU GET A VOICE."
  opt "HANG THEM. LET THE FORD WARN." -> hanged_end
  opt "SEND THEM HOME WITHOUT BOOTS." -> boots_end
  opt "TAKE THEM INTO THE BAND." -> sworn_end

stage hanged_end
  end success
  say "THE POSTS AT THE FORD CREAK IN THE WIND NOW. NO RAIDER HAS CROSSED SINCE. THE [[=band]] DRINK TO YOU, AND DO NOT SING."
  journal "THE [[=band]] HELD THE FORD AT {FORD}, AND HANGED THEIR PRISONERS THERE AS A WARNING."
  do reward fair
  do remember giver "TRUTH AND COURAGE. THE TRUTH IS WE HANGED BOYS. I'M STILL LOOKING FOR THE COURAGE TO SAY IT."
  do fact "THE FORD AT {FORD} HAS GALLOWS POSTS NOW. NO REAVER HAS CROSSED IT SINCE THE NINE HELD IT."

stage boots_end
  end success
  say "THEY WENT BACK OVER THE HILLS BAREFOOT, CURSING, AND EVERY REAVER CAMP WILL HEAR WHO SENT THEM. THE [[=band]] LAUGHED FOR A WEEK."
  journal "THE [[=band]] HELD THE FORD AT {FORD} AND SENT THEIR PRISONERS HOME BAREFOOT."
  do reward fair
  do remember giver "BAREFOOT! OVER THE HILLS! WE'VE A SONG ABOUT IT NOW. IT HAS YOU IN IT. <<praise>>"
  do fact "THE NINE SHIELDS OF {HOME} HELD THE FORD AT {FORD} AGAINST FORTY, AND SENT THE LIVING HOME WITHOUT THEIR BOOTS."

stage sworn_end
  end success
  say "TWO OF THEM SPAT. FOUR KNELT. THE [[=band]] ARE THIRTEEN NOW, AND THE NEWEST SHIELDS CARRY THE OATH LIKE A HOT COAL, CAREFULLY, AND WITH BOTH HANDS."
  journal "THE [[=band]] TOOK THEIR PRISONERS INTO THE OATH. THEY ARE THIRTEEN NOW."
  do reward rich
  do remember giver "THIRTEEN. ONE OF THE NEW ONES SAVED MY LIFE AT THE SECOND FORD. HE WAS A REAVER A MONTH AGO. <<saying>>"
  do fact "THE SWORN BAND OF {HOME} TOOK DEFEATED REAVERS INTO ITS OATH. TRUTH AND COURAGE, THEY SAY. THEY MEAN IT."
)SAGA";

const char* const kPitHound = R"SAGA(
title [[THE BEAST RAISED FROM A PUP|THE PIT AT THE CAMP|COME HOME, OLD FRIEND]]
hook npc any
pitch "[[pitch=WHOSE IS THAT EMPTY COLLAR?|WHY IS THERE A BOWL BY YOUR DOOR?|YOU LOOK LIKE YOU'VE LOST A FRIEND.]]"
hint "[[~pitch|I RAISED HIM ON GOAT'S MILK AND STORIES. HE SLEPT ACROSS MY DOOR FOR SIX YEARS. NOW THE DOOR IS JUST A DOOR.|I RAISED HIM ON GOAT'S MILK AND STORIES. HE SLEPT ACROSS MY DOOR FOR SIX YEARS. NOW THE DOOR IS JUST A DOOR.|THERE ARE MEN WHO MAKE GREAT BEASTS FIGHT IN PITS FOR WAGERS. I DIDN'T BELIEVE IT. NOW I DO.]]"
role giver giver
role home site home
role camp site camp near
role master person male at camp
var paid 0
slot t1 -> homeward betrayal rival world
slot t2 -> reunion mercy price return

stage start
  talk giver
  say "I RAISED HIM FROM [[beast=A WOLFHOUND PUP|A BEAR CUB|A BOARHOUND PUP]], AND CALLED HIM [[hname=BRAN|OLD GREY|THUNDER|ASHES]]. NOW PIT-MEN AT {CAMP} HAVE TAKEN HIM TO FIGHT FOR WAGERS, AND HE WON'T EAT FROM ANY HAND BUT MINE. <<plea:love>>"
  opt "I'LL BRING [[=hname]] HOME." -> seek
  opt "WHAT WILL THE PIT-MEN WANT?" -> price
  opt "A BEAST IS A BEAST." -> refused

stage price
  talk giver
  say "COIN, OR BLOOD. THEY'RE THAT SORT. I HAVE NO COIN. I'D GIVE THEM BLOOD, BUT I'M OLD AND I'D GIVE IT BADLY. <<grief:shame>>"
  opt "THEN I'LL GIVE IT WELL." -> seek

stage refused
  end fail
  say "<<refusal:grief>> A BEAST IS A BEAST. AND A FRIEND IS A FRIEND. YOU DON'T KNOW THE DIFFERENCE, AND I PITY YOU."
  journal "YOU WOULD NOT GO TO {CAMP} FOR {GIVER}'S [[=hname]]."
  do remember giver "THEY SAY HE KILLED THREE IN THE PIT LAST WEEK. HE NEVER KILLED A HEN FOR ME."

stage seek
  goal goto camp
  then master_talk
  journal "{GIVER} OF {HOME} RAISED [[=hname]] FROM [[=beast]]. PIT-MEN AT {CAMP} HAVE TAKEN HIM TO FIGHT. BRING HIM HOME."

stage master_talk
  talk master
  say "THE BEAST CALLED [[=hname]]? BEST FIGHTER I'VE OWNED. WON'T EAT FROM ANY HAND, WON'T BREAK, KEEPS LOOKING DOWN THE ROAD. SELL HIM? FORTY SILVER. OR STEP IN THE PIT YOURSELF AGAINST MY LADS. <<boast:greed>>"
  opt "FORTY, THEN." check gold 40 -> bought else short
  opt "I'LL TAKE THE PIT." -> pit
  opt "OPEN THE CAGE. HE'LL CHOOSE." -> choose

stage short
  talk master
  say "THAT'S NOT FORTY. THAT'S AN INSULT WITH A PURSE ROUND IT. THE PIT, THEN, OR THE ROAD. <<insult>>"
  opt "THE PIT." -> pit
  opt "OPEN THE CAGE. HE'LL CHOOSE." -> choose

stage bought
  talk master
  say "A PLEASURE. HE'LL TEAR YOUR THROAT OUT ON THE ROAD, MIND. NO REFUNDS."
  do gold -40
  do set paid 1
  opt "WE'LL SEE." -> @t1

stage pit
  goal kill 3 bandit in camp
  then pit_won
  journal "YOU STEPPED INTO THE PIT AT {CAMP} AGAINST THE PIT-MASTER'S LADS, FOR [[=hname]]."

stage pit_won
  talk master
  say "...THREE OF MY BEST. YOU'RE WORSE THAN THE BEAST. TAKE HIM AND GET OUT BEFORE I FIND MORE LADS."
  opt "GLADLY." -> @t1

stage choose
  talk master
  say "OPEN IT? HE'LL HAVE YOUR FACE OFF. ...FINE. YOUR FACE. ...LOOK AT THAT. HE WALKS PAST YOU, SNIFFS THE ROAD, AND JUST... GOES. TOWARD {HOME}. LIKE HE KNEW."
  opt "FOLLOW HIM HOME." -> @t1

stage homeward
  goal goto home
  then @t2
  journal "[[=hname]] IS OUT OF THE PIT AT {CAMP}. TAKE HIM HOME TO {GIVER} IN {HOME}."

stage reunion
  talk giver
  say "[[=hname]]! ...HE'S THIN, AND SCARRED, AND HE PUT HIS GREAT HEAD IN MY LAP LIKE HE WAS SMALL AGAIN. THE PIT-MEN WILL COME FOR HIM. OR FOR ME. WHAT DO WE DO?"
  opt "LET THEM COME. WE STAND." -> stand_end
  opt "TAKE HIM DEEP INTO THE WILD." -> wild_end
  opt "LET HIM GUARD {HOME}." -> guard_end

stage stand_end
  end success
  say "THE PIT-MEN CAME ONCE, AT DUSK. THEY SAW [[=hname]] IN THE DOORWAY, AND {GIVER} BESIDE HIM WITH A PITCHFORK, AND YOU, AND THEY REMEMBERED SOMEWHERE ELSE TO BE."
  journal "[[=hname]] IS HOME WITH {GIVER}. THE PIT-MEN OF {CAMP} CAME ONCE, AND THOUGHT BETTER OF IT."
  do reward fair
  do remember giver "HE SLEEPS ACROSS MY DOOR AGAIN. I STEP OVER HIM EVERY MORNING. BEST THING IN THE WORLD. <<thanks:love>>"
  do fact "A GREAT BEAST CALLED [[=hname]] SLEEPS ACROSS A DOOR IN {HOME}. IT CAME HOME FROM THE FIGHTING PITS ON ITS OWN FEET."

stage wild_end
  end success
  say "{GIVER} WALKED [[=hname]] THREE DAYS INTO THE WILD AND CAME BACK ALONE, AND DID NOT SAY WHAT WAS SAID AT THE PARTING. SOMETIMES, AT NIGHT, SOMETHING HUGE WATCHES THE HOUSE FROM THE TREES."
  journal "{GIVER} SET [[=hname]] FREE IN THE DEEP WILD, BEYOND THE PIT-MEN'S REACH."
  do reward fair
  do remember giver "HE COMES TO THE TREE LINE ON MY NAME DAY. JUST STANDS THERE. I WAVE. <<grief:hope>>"

stage guard_end
  end success
  say "[[=hname]] WALKS THE WALLS OF {HOME} NOW. CHILDREN RIDE ON HIS BACK IN DAYLIGHT, AND AT NIGHT NOTHING COMES NEAR THE GATES. THE PIT-MEN MOVED THEIR CAMP."
  journal "[[=hname]] GUARDS {HOME} NOW, AND THE PIT-MEN OF {CAMP} HAVE MOVED ON."
  do reward rich
  do remember giver "THE WHOLE TOWN FEEDS HIM NOW. HE'S FAT. HE'S STILL MINE. <<boast:pride>>"
  do fact "{HOME} IS GUARDED BY A GREAT BEAST CALLED [[=hname]], RAISED BY HAND AND STOLEN BACK FROM A PIT."
)SAGA";

const char* const kGiantStones = R"SAGA(
title [[THE GIANT'S GRUDGE|THE STONES OF THE TALL FOLK|WHAT THE QUARRY DIGS UP]]
hook npc any
pitch "[[pitch=WHAT'S BREAKING ROCKS IN THE HILLS?|WHY DID THE QUARRY STOP?|WAS THAT THUNDER?]]"
hint "[[~pitch|SOMETHING THROWS BOULDERS AT THE QUARRY. BOULDERS THE SIZE OF A COW. FROM THE RIDGE. HALF A MILE.|SOMETHING THROWS BOULDERS AT THE QUARRY. BOULDERS THE SIZE OF A COW. FROM THE RIDGE. HALF A MILE.|MY GRANDMOTHER SAID THE TALL FOLK WERE ALL DEAD. MY GRANDMOTHER WAS WRONG ABOUT A LOT OF THINGS.]]"
role giver giver
role home site home
role ruin ruin near
role giant person male at ruin
var offered 0
slot t1 -> ridge deceit identity wonder
slot t2 -> village_answer mercy price world

stage start
  talk giver
  say "WE QUARRY THE OLD STONES AT {RUIN} FOR OUR WALLS. ALWAYS HAVE. NOW SOMETHING ON THE RIDGE THROWS BOULDERS AT THE QUARRY. [[TWO MEN|THREE OXEN|THE FOREMAN]] DEAD. THE OLD WIVES SAY IT'S THE LAST OF THE TALL FOLK. <<warning:fear>>"
  opt "I'LL CLIMB THE RIDGE." -> @t1
  opt "WHO WERE THE TALL FOLK?" -> lore
  opt "STOP QUARRYING, THEN." -> refused

stage lore
  talk giver
  say "GIANTS. THEY RULED HERE BEFORE {RUIN.OLD}, BEFORE ANY OF US. OUR FOREFATHERS DROVE THEM OUT. THE SONGS SAY IT WAS A GLORIOUS WAR. THE SONGS ARE VERY SHORT. <<doubt>>"
  opt "THEN I'LL ASK THE GIANT." -> @t1

stage refused
  end fail
  journal "YOU WOULD NOT CLIMB TO WHATEVER THROWS BOULDERS AT THE QUARRY OF {RUIN}."
  do remember giver "THE QUARRY'S SHUT. THE MEN SIT IN THE TAVERN AND DRINK THEIR WAGES THAT AREN'T COMING. <<grief>>"

stage ridge
  goal goto ruin
  then giant_talk
  journal "SOMETHING ON THE RIDGE ABOVE {RUIN} KILLS THE QUARRYMEN OF {HOME}. CLIMB UP AND FACE IT."

stage giant_talk
  talk giant
  say "SMALL ONE. YOU CLIMB WELL FOR A SMALL ONE. YOUR PEOPLE BREAK MY PEOPLE'S BONES. THOSE STONES ARE OUR GRAVES. YOUR FOREFATHERS ASKED MY KIN TO A FEAST UNDER THIS HILL, BARRED THE DOORS, AND BURNED IT. I AM THE LAST. I AM VERY OLD. I AM VERY TIRED."
  opt "I'LL MAKE THEM STOP." -> stop
  opt "COME DOWN. EAT WITH THEM." -> feast
  opt "THE PAST IS DEAD. GO." check level 5 -> driven_end else crushed

stage stop
  talk giant
  say "STOP? SMALL ONES DO NOT STOP. BUT IF YOU CAN MAKE THEM, I WILL PUT DOWN THE STONES. GO AND SAY IT. I WILL WAIT. I AM VERY GOOD AT WAITING NOW."
  opt "I'LL TELL THEM." -> @t2

stage feast
  talk giant
  say "EAT? WITH THE CHILDREN OF THE ONES WHO BURNED THE LAST FEAST? ...YOUR FACE IS SERIOUS. HM. ASK THEM, THEN. IF THEY SAY YES, I WILL COME, AND I WILL SIT NEAR THE DOOR."
  do set offered 1
  opt "I'LL ASK THEM." -> @t2

stage crushed
  end fail
  say "HE DID NOT EVEN STAND. HE FLICKED A STONE THE SIZE OF A BARREL, ALMOST GENTLY, AND YOU WOKE AT THE BOTTOM OF THE RIDGE WITH THE STARS OUT."
  journal "THE LAST OF THE TALL FOLK THREW YOU OFF THE RIDGE ABOVE {RUIN}. THE QUARRY STAYS SHUT."
  do remember giver "YOU'RE ALIVE? HE COULD HAVE KILLED YOU. HE DIDN'T. WHAT DOES THAT MEAN?"

stage driven_end
  end success
  say "HE LOOKED AT YOUR BLADE FOR A LONG TIME, THEN STOOD, AND STOOD, AND KEPT STANDING. THEN HE WALKED AWAY OVER THE RIDGE, AND THE GROUND SHOOK FOR AN HOUR, AND THEN IT DIDN'T."
  journal "YOU DROVE THE LAST OF THE TALL FOLK FROM THE RIDGE ABOVE {RUIN}. THE QUARRY OPENS AGAIN."
  do reward fair
  do mark drove_the_last_giant
  do remember giver "THE QUARRY'S OPEN. WE FOUND A SKULL TODAY THE SIZE OF A CART. NOBODY WANTS TO TOUCH IT."
  do fact "THE LAST OF THE TALL FOLK WAS DRIVEN FROM THE RIDGE ABOVE {RUIN}. HE WENT EAST, SLOWLY, AND DID NOT LOOK BACK."

stage village_answer
  talk giver
  say "STOP THE QUARRY? HALF OF {HOME} EATS FROM IT. AND YOU WANT US TO SIT AT TABLE WITH THE THING THAT KILLED OUR MEN? ...BUT IF THE SONGS LIED. IF THE FEAST WAS OURS. <<grief:shame>>"
  opt "SET A TABLE. SAY THE TRUTH." if var offered 1 -> feast_end
  opt "QUARRY ONLY THE NORTH FACE." -> north_end
  opt "SHUT THE QUARRY. FOR GOOD." -> shut_end

stage feast_end
  end success
  say "THEY SET THE TABLE IN THE OPEN, BECAUSE NO HALL WOULD HOLD HIM. HE SAT NEAR THE DOOR THAT WASN'T THERE. THE ELDEST OF {HOME} SAID THE TRUTH ALOUD, AND THE GIANT WEPT, AND IT RAINED."
  journal "{HOME} SET A TABLE FOR THE LAST OF THE TALL FOLK AND SAID ALOUD WHAT THEIR FOREFATHERS DID."
  do reward rich
  do remember giver "HE COMES DOWN FOR THE HARVEST FEAST. HE CARRIES THE BEER. NOBODY ELSE CAN LIFT THE BARREL. <<saying>>"
  do fact "THE LAST GIANT EATS AT THE HARVEST TABLE OF {HOME}. THE TOWN SINGS A LONGER SONG ABOUT THE OLD WAR NOW."

stage north_end
  end success
  say "THE QUARRYMEN CUT ONLY THE NORTH FACE NOW, WHERE NO ONE IS BURIED. THE GIANT WATCHES FROM THE RIDGE AND THROWS NOTHING. NEITHER SIDE CALLS IT PEACE."
  journal "{HOME} QUARRIES ONLY THE NORTH FACE OF {RUIN} NOW. THE GIANT WATCHES, AND WAITS."
  do reward fair
  do remember giver "HALF THE STONE, HALF THE WAGES. THE MEN GRUMBLE. NOBODY'S DEAD. I'LL TAKE IT. <<proverb>>"
  do fact "THE QUARRY AT {RUIN} CUTS ONLY THE NORTH FACE. THE SOUTH FACE IS A GRAVE, AND A GIANT GUARDS IT."

stage shut_end
  end success
  say "THE QUARRY WAS SHUT AND THE TOOLS SOLD. IN SPRING THE GIANT CAME DOWN IN THE NIGHT AND RAISED THE FALLEN STONES AGAIN, ONE BY ONE, AND THEN HE LAY DOWN AMONG THEM, AND HAS NOT MOVED SINCE."
  journal "{HOME} SHUT THE QUARRY OF {RUIN}. THE LAST GIANT RAISED HIS KIN'S STONES AND LAY DOWN AMONG THEM."
  do reward fair
  do remember giver "WE CAN'T BUILD THE NEW WALL NOW. I GO UP SOMETIMES AND SIT WITH HIM. HE DOESN'T SAY ANYTHING. NEITHER DO I."
  do fact "A CIRCLE OF STANDING STONES RISES AGAIN AT {RUIN}. IN ITS HEART LIES SOMETHING VERY LARGE THAT DOES NOT MOVE."
)SAGA";

const char* const kMasterTraitor = R"SAGA(
title [[THE MASTER WHO TURNED|THE STOLEN SEAL|TRAITOR, OR THE ONLY HONEST ONE]]
hook npc any
pitch "[[pitch=WHY DO THEY SPIT AT YOUR MASTER'S NAME?|WHAT DID YOUR MASTER STEAL?|YOU LOOK LIKE AN APPRENTICE.]]"
hint "[[~pitch|THEY CALL MY MASTER A TRAITOR. THEY'RE RIGHT. I THINK. I WANT TO KNOW.|TWENTY YEARS MY MASTER TAUGHT ME TO KEEP MY WORD. THEN MY MASTER BROKE EVERY WORD IN ONE NIGHT AND RAN.|TWENTY YEARS MY MASTER TAUGHT ME TO KEEP MY WORD. THEN MY MASTER BROKE EVERY WORD IN ONE NIGHT AND RAN.]]"
role giver giver
role home site home
role town site town near
role master person [[male|female]] at town
role reeve person male at home
var proof 0
slot t1 -> town_go deceit rival betrayal
slot t2 -> return_home mercy price betrayal

stage start
  talk giver
  say "MY MASTER, {MASTER}, TAUGHT ME EVERYTHING I KNOW, THEN STOLE THE [[seal=TOWN SEAL|GUILD SEAL|TEMPLE SEAL]] OF {HOME} ONE NIGHT AND RAN TO {TOWN}. WITHOUT THE [[=seal]] THE REEVE CANNOT SIGN THE [[deal=SALE OF THE WELLS|NEW TOLL|LAND GRANT]]. <<grief:shame>>"
  opt "I'LL FIND {MASTER}." -> @t1
  opt "WHAT DOES THE REEVE SAY?" -> reeve_talk
  opt "A THIEF IS A THIEF." -> refused

stage reeve_talk
  talk reeve
  say "A TRAITOR. BRING BACK THE [[=seal]], AND {MASTER.HIM} IN CHAINS IF YOU CAN. THE [[=deal]] WILL MAKE {HOME} RICH. RICHER. SOME OF US, AT LEAST. <<threat:greed>>"
  opt "SOME OF YOU?" -> @t1

stage refused
  end fail
  journal "YOU WOULD NOT LOOK FOR {MASTER}, {GIVER}'S MASTER."
  do remember giver "THEY BURNED {MASTER}'S WORKSHOP. I SAVED ONE TOOL. I DON'T KNOW WHY I'M TELLING YOU."

stage town_go
  goal goto town
  then master_talk
  journal "{MASTER}, {GIVER}'S MASTER, STOLE THE [[=seal]] OF {HOME} AND FLED TO {TOWN}. FIND OUT WHY."

stage master_talk
  talk master
  say "THEY SENT YOU? GOOD. SOMEONE SHOULD HEAR IT. THE REEVE HAS SOLD THE [[=deal]] TO A LORD WHO WILL TAX THE WATER UNTIL CHILDREN DIE OF IT. I COULDN'T STOP HIM. I COULD STEAL HIS SEAL. <<oath>>"
  opt "PROVE IT." check level 3 -> ledger else no_proof
  opt "COME BACK AND SAY IT ALOUD." -> @t2
  opt "GIVE ME THE [[=seal]]." -> seized

stage ledger
  talk master
  say "HERE: THE REEVE'S OWN LETTER TO THE LORD. HIS HAND, HIS PRICE, HIS SHARE. I TOOK IT WITH THE [[=seal]]. I'M A THIEF TWICE OVER. <<apology:duty>>"
  do give "THE REEVE'S LETTER" letter
  do set proof 1
  opt "THEN COME HOME WITH IT." -> @t2

stage no_proof
  talk master
  say "PROVE IT? I HAVE MY WORD AND TWENTY YEARS OF KEEPING IT. ASK {GIVER}. THAT'S THE ONLY PROOF I EVER HAD. <<doubt>>"
  opt "COME BACK AND SAY IT ALOUD." -> @t2
  opt "GIVE ME THE [[=seal]]." -> seized

stage seized
  talk master
  say "TAKE IT, THEN. I WON'T FIGHT YOU. I'M TOO OLD, AND YOU'RE TOO SURE. TELL {GIVER} I'M SORRY I WASN'T WHAT {GIVER.HE} THOUGHT. OR THAT I WAS. YOU DECIDE. <<grief:shame>>"
  do give "THE [[=seal]]" sigil
  opt "I'LL TELL {GIVER.HIM}." -> sealed_end

stage return_home
  goal goto home
  then trial
  do moves master home
  journal "{MASTER} COMES BACK TO {HOME} TO FACE THE REEVE, AND THE TOWN, AND {GIVER}."

stage trial
  talk giver
  say "THE WHOLE SQUARE IS HERE. THE REEVE, IN HIS BEST COAT. {MASTER}, IN CHAINS {MASTER.HE} ASKED TO WEAR. THEY'RE ALL WAITING FOR SOMEONE TO SPEAK FIRST. WHAT DO WE SAY?"
  opt "READ THE REEVE'S LETTER ALOUD." if have "THE REEVE'S LETTER" -> exposed_end
  opt "{MASTER} WAS RIGHT. SAY IT." check fame 1 -> believed_end else doubted_end
  opt "SAY NOTHING. RETURN THE SEAL." -> quiet_end

stage sealed_end
  end success
  say "THE REEVE SIGNED THE [[=deal]] WITH THE [[=seal]] THE NEXT MORNING AND PAID YOU WELL. {GIVER} TOOK THE MESSAGE WITHOUT A WORD. THE WELLS HAVE A TOLL NOW."
  journal "YOU BROUGHT BACK THE [[=seal]] OF {HOME}. THE [[=deal]] IS SIGNED. {MASTER} STAYS IN {TOWN}."
  do take "THE [[=seal]]"
  do reward fair
  do mark sealed_the_deal
  do remember giver "THE WELL TOLL IS TWO COPPERS. MY MASTER SAID IT WOULD BE. MY MASTER WAS RIGHT ABOUT EVERYTHING EXCEPT ME."

stage exposed_end
  end success
  say "YOU READ IT IN THE REEVE'S OWN WORDS, AND THE SQUARE WENT SILENT, AND THEN VERY LOUD. THE REEVE LEFT {HOME} WITH ONE BOOT ON. {MASTER} TOOK OFF THE CHAINS AND HANDED THEM TO {GIVER}."
  journal "THE REEVE'S LETTER WAS READ IN THE SQUARE OF {HOME}. {MASTER} WAS RIGHT. THE WELLS STAY FREE."
  do take "THE REEVE'S LETTER"
  do reward rich
  do hide reeve
  do remember master "TRAITOR, THEY CALLED ME. NOW THEY CALL ME COUNCILLOR. I LIKED TRAITOR BETTER. FEWER MEETINGS."
  do fact "THE REEVE OF {HOME} TRIED TO SELL THE WELLS AND WAS STOPPED BY A THIEF, A LETTER AND A STRANGER."

stage believed_end
  end success
  say "THEY BELIEVED YOU, BECAUSE YOU HAD NO REASON TO LIE AND THE REEVE HAD EVERY REASON. THE [[=deal]] WAS TORN UP. {MASTER} WENT BACK TO WORK THE NEXT DAY, AS IF NOTHING HAD HAPPENED."
  journal "{HOME} BELIEVED THAT {MASTER} STOLE THE [[=seal]] TO STOP THE [[=deal]]. THE DEAL IS DEAD."
  do reward fair
  do remember giver "MY MASTER IS HOME. WE DON'T TALK ABOUT IT. WE TALK ABOUT JOINTS AND GRAIN AND GLUE. IT'S PERFECT."

stage doubted_end
  end fail
  say "THEY DID NOT BELIEVE YOU. THE REEVE SMILED AND SIGNED. {MASTER} WAS SENT TO THE STOCKS FOR A WEEK, AND {GIVER} BROUGHT {MASTER.HIM} WATER EVERY DAY, AND NOBODY STOPPED {GIVER.HIM}."
  journal "{HOME} DID NOT BELIEVE {MASTER}. THE [[=deal]] IS SIGNED. THE STOCKS ARE FULL."
  do mark doubted_in_the_square
  do remember master "YOU TRIED. THAT'S THE WHOLE OF IT, MOST DAYS. TRYING. <<comfort:hope>>"

stage quiet_end
  end success
  say "YOU GAVE BACK THE SEAL AND SAID NOTHING. {MASTER} WAS FINED AND FORGIVEN, AS A FOOL IS FORGIVEN. THE [[=deal]] WAS SIGNED. {MASTER} LOOKED AT YOU ONCE, AND THEN NEVER AGAIN."
  journal "YOU RETURNED THE [[=seal]] OF {HOME} AND KEPT QUIET. {MASTER} IS FORGIVEN, AND SILENT."
  do reward fair
  do remember master "YOU HAD A CHOICE. I KNOW WHAT IT COST YOU TO MAKE IT. I KNOW WHAT IT COST THE REST OF US MORE."
)SAGA";

const char* const kTwoHosts = R"SAGA(
title [[THE WINGED AND THE SCORCHED|TWO STRANGERS AND A RELIC|AN OLD WAR IN A SMALL TOWN]]
hook npc any
pitch "[[pitch=WHO ARE THE TWO STRANGERS?|WHY IS YOUR KIN HIDING?|WHAT DID THE CHILD FIND?]]"
hint "[[~pitch|ONE HAS FEATHERS SEWN INTO HER CLOAK AND NEVER BLINKS. THE OTHER HAS BURNED HANDS AND LAUGHS TOO MUCH. BOTH WANT WHAT MY KIN FOUND.|ONE HAS FEATHERS SEWN INTO HER CLOAK AND NEVER BLINKS. THE OTHER HAS BURNED HANDS AND LAUGHS TOO MUCH. BOTH WANT WHAT MY KIN FOUND.|THERE IS AN OLD WAR, OLDER THAN KINGS, AND IT HAS COME TO OUR TOWN LOOKING FOR A TRINKET.]]"
role giver giver
role home site home
role kin resident kin of giver
role winged person female at home
role scorched person male at home
role cave site cave near
slot t1 -> listen deceit betrayal prophecy
slot t2 -> decide price mercy wonder

stage start
  talk giver
  say "MY {KIN.KIN} {KIN} FOUND [[relic=A FEATHER OF WHITE BRONZE|A BLACK STONE THAT HUMS|A SPEARHEAD OF STAR-IRON]] IN THE STREAM. NOW TWO STRANGERS WANT IT: {WINGED}, WHO NEVER BLINKS, AND {SCORCHED}, WHOSE HANDS ARE BURNED. EACH SAYS THE OTHER WILL DROWN THE WORLD IN FIRE. <<warning:fear>>"
  opt "LET ME HEAR THEM BOTH." -> @t1
  opt "WHERE IS THE THING NOW?" -> kin_talk
  opt "GIVE IT TO EITHER. BE RID OF IT." -> refused

stage kin_talk
  talk kin
  say "UNDER MY BED. IT'S WARM. IT'S ALWAYS WARM. AT NIGHT I DREAM OF A WAR IN THE SKY THAT NEVER ENDS, AND WE'RE ALL ON THE GROUND UNDER IT, LIKE ANTS UNDER BOOTS. <<omen>>"
  opt "LET ME HEAR THE STRANGERS." -> @t1

stage refused
  end fail
  journal "YOU TOLD {GIVER} TO GIVE AWAY [[=relic]] TO EITHER STRANGER."
  do remember giver "{KIN} GAVE IT TO THE ONE WHO SMILED. THE OTHER ONE STOOD IN THE STREET ALL NIGHT, LOOKING AT OUR DOOR."

stage listen
  goal wait 1
  then winged_talk
  journal "TWO STRANGERS IN {HOME} WANT [[=relic]] THAT {KIN} FOUND. EACH NAMES THE OTHER THE ENEMY. HEAR THEM."

stage winged_talk
  talk winged
  say "THE RELIC IS OURS. IT FELL FROM US WHEN THEY FELL FROM US. THE BURNED ONE WILL USE IT TO OPEN A DOOR THAT SHOULD STAY SHUT. YOUR KIN WILL BE THE FIRST TO BURN. I DO NOT LIE. I DO NOT KNOW HOW."
  opt "AND WHAT WILL YOU DO WITH IT?" -> winged_why
  opt "NOW I'LL HEAR {SCORCHED}." -> scorched_talk

stage winged_why
  talk winged
  say "KEEP IT. FOREVER. WHERE NO MORTAL TOUCHES IT. AND IF YOUR TOWN STANDS IN THE WAY OF FOREVER, YOUR TOWN WILL BE A SMALL PRICE. <<saying>>"
  opt "NOW I'LL HEAR {SCORCHED}." -> scorched_talk

stage scorched_talk
  talk scorched
  say "SHE TOLD YOU I'M THE MONSTER? OF COURSE SHE DID. WE WERE THE SAME ONCE. WE FELL BECAUSE WE ASKED WHY. THAT RELIC CAN OPEN A WAY HOME FOR US. AND YES, HOME IS ON THE OTHER SIDE OF YOUR SKY. YOUR WEATHER MAY SUFFER. <<boast:hope>>"
  opt "I'VE HEARD ENOUGH." -> @t2

stage decide
  talk kin
  say "YOU'VE HEARD THEM BOTH. ONE IS COLD AND SURE. ONE IS WARM AND LYING ABOUT SOMETHING. NEITHER OF THEM ONCE ASKED MY NAME. WHAT DO I DO WITH IT?"
  opt "GIVE IT TO {WINGED}." -> winged_end
  opt "GIVE IT TO {SCORCHED}." -> scorched_end
  opt "SINK IT IN {CAVE}, DEEP." -> sunk
  opt "KEEP IT. TELL NOBODY." -> kept_end

stage sunk
  goal goto cave
  then sunk_end
  do give "[[=relic]]" gem
  journal "CARRY [[=relic]] TO {CAVE} AND DROP IT IN THE DEEPEST WATER YOU CAN FIND, WHERE NEITHER HOST CAN FOLLOW."

stage winged_end
  end success
  say "{WINGED} TOOK IT WITHOUT THANKS AND WAS GONE BEFORE DAWN. THAT SUMMER WAS COLD, AND THE NEXT, AND THE PRIESTS SAID IT WAS A BLESSING. {KIN} STOPPED DREAMING."
  journal "YOU GAVE [[=relic]] TO {WINGED}. THE WINGED HOST HAS IT BACK. THE SUMMERS ARE COLD."
  do reward fair
  do hide winged
  do hide scorched
  do remember kin "NO MORE DREAMS. I MISS THEM, A LITTLE. ISN'T THAT STRANGE? <<doubt>>"
  do fact "A COLD SUMMER CAME TO {HOME} THE YEAR A STRANGER WITH FEATHERS IN HER CLOAK TOOK SOMETHING AWAY."

stage scorched_end
  end success
  say "{SCORCHED} LAUGHED, AND KISSED {KIN}'S FOREHEAD, AND WAS GONE. THE SKY OVER {HOME} WAS GREEN FOR THREE NIGHTS. THE WINGED STRANGER STOOD IN THE SQUARE AND LOOKED AT YOU UNTIL YOU LOOKED AWAY."
  journal "YOU GAVE [[=relic]] TO {SCORCHED}. THE SKY WAS GREEN FOR THREE NIGHTS. THE WINGED HOST REMEMBERS YOUR FACE."
  do reward fair
  do hide scorched
  do hide winged
  do mark marked_by_the_winged
  do fact "THE SKY ABOVE {HOME} BURNED GREEN FOR THREE NIGHTS. A STRANGER WITH BURNED HANDS WAS SEEN LAUGHING IN IT."

stage sunk_end
  end success
  say "IT SANK WITHOUT A SOUND AND THE WATER CLOSED OVER IT LIKE A HAND. WHEN YOU CAME BACK TO {HOME}, BOTH STRANGERS WERE GONE. EACH HAD LEFT A MARK ON {GIVER}'S DOOR: A WHITE FEATHER, AND A SCORCH."
  journal "YOU SANK [[=relic]] IN {CAVE}. NEITHER HOST HAS IT. BOTH KNOW WHO TOOK IT FROM THEM."
  do take "[[=relic]]"
  do reward rich
  do hide winged
  do hide scorched
  do mark hunted_by_both_hosts
  do remember giver "A FEATHER AND A BURN ON MY DOOR. I PAINTED OVER BOTH. THEY CAME BACK BY MORNING. <<omen>>"

stage kept_end
  end success
  say "{KIN} WRAPPED IT IN WOOL AND PUT IT UNDER THE HEARTHSTONE. BOTH STRANGERS STAYED IN {HOME} ALL WINTER, POLITE AS CATS, WATCHING THE HOUSE. IN SPRING, THEY WERE STILL THERE."
  journal "{KIN} KEEPS [[=relic]] HIDDEN UNDER THE HEARTH. TWO OLD HOSTS WATCH THE HOUSE IN {HOME}."
  do reward small
  do mark kept_the_relic
  do remember kin "THEY BOTH SAY GOOD MORNING TO ME NOW. EVERY MORNING. I SAY IT BACK. I'M NOT AFRAID. I'M VERY AFRAID."
)SAGA";

const char* const kEnemyReasons = R"SAGA(
title [[THE RAIDER'S REASONS|THE OTHER SIDE OF THE FIRE|WHY THE MILL BURNED]]
hook npc any
pitch "[[pitch=WHAT HAPPENED TO THE MILL?|WHO BURNED IT?|YOU'RE SHARPENING THAT FOR SOMEONE.]]"
hint "[[~pitch|I KNOW THE NAME OF THE ONE WHO LED THEM. I SAY IT EVERY NIGHT SO I DON'T FORGET.|RAIDERS. THEY CAME OUT OF THE DARK WITH TORCHES AND KNEW EXACTLY WHICH BUILDING TO BURN. EXACTLY.|I KNOW THE NAME OF THE ONE WHO LED THEM. I SAY IT EVERY NIGHT SO I DON'T FORGET.]]"
role giver giver
role home site home
role camp site camp near
role chief person male at camp
slot t1 -> camp_go rival world betrayal
slot t2 -> back_home mercy price

stage start
  talk giver
  say "RAIDERS UNDER {CHIEF} BURNED [[place=THE MILL|THE GRAIN STORE|THE BRIDGE HOUSE]] AND TOOK [[TWO|FOUR]] HORSES. MY [[lost=HUSBAND|BROTHER|ELDEST]] DIED IN THE SMOKE TRYING TO SAVE THE HORSES. {CHIEF} CAMPS AT {CAMP}. I WANT {CHIEF}'S HEAD. <<vow:vengeance>>"
  opt "I'LL GO TO {CAMP}." -> @t1
  opt "WHY DID THEY BURN [[=place]]?" -> why
  opt "VENGEANCE ISN'T MY TRADE." -> refused

stage why
  talk giver
  say "WHY? BECAUSE THEY'RE ANIMALS. ...THEY KNEW WHICH DOOR. THEY DIDN'T TOUCH ANOTHER HOUSE. I DON'T WANT TO THINK ABOUT THAT. <<doubt>>"
  opt "I'LL GO TO {CAMP}." -> @t1

stage refused
  end fail
  journal "YOU WOULD NOT HUNT {CHIEF} FOR {GIVER} OF {HOME}."
  do remember giver "I BOUGHT A SPEAR. I'M LEARNING. SLOWLY. <<threat:vengeance>>"

stage camp_go
  goal goto camp
  then chief_talk
  journal "{CHIEF} LED THE RAIDERS WHO BURNED [[=place]] IN {HOME} AND KILLED {GIVER}'S [[=lost]]. HE CAMPS AT {CAMP}."

stage chief_talk
  talk chief
  say "{HOME} SENT YOU. SIT. EAT FIRST, THEN KILL ME, IF YOU STILL WANT TO. [[TWELVE|FIFTEEN|TWENTY]] YEARS AGO {HOME}'S LEVY BURNED MY VILLAGE FOR A TAX WE COULDN'T PAY. THE ONE WHO HELD THE TORCH TO MY FATHER'S HOUSE WAS {GIVER}'S [[=lost]]. ASK {GIVER.HIM}."
  opt "AND THE HORSES? THE DEAD?" -> horses
  opt "COME BACK AND SAY IT TO {GIVER}." -> @t2
  opt "YOUR BAND ENDS TONIGHT." -> fight

stage horses
  talk chief
  say "THE HORSES PULL OUR PLOUGHS NOW. THE DEAD... I DID NOT WANT HIM DEAD. I WANTED HIM TO STAND IN THE SMOKE AND KNOW WHAT IT WAS LIKE. HE RAN INTO IT FOR THE HORSES. HE WAS BRAVER THAN I WANTED HIM TO BE. <<grief:shame>>"
  opt "COME BACK AND SAY IT TO {GIVER}." -> @t2
  opt "YOUR BAND ENDS TONIGHT." -> fight

stage fight
  goal kill 3 bandit in camp
  then fought_end
  journal "YOU TURNED ON {CHIEF}'S BAND AT {CAMP}."

stage back_home
  goal goto home
  then face
  do moves chief home
  journal "{CHIEF} COMES TO {HOME} UNARMED, TO SAY TO {GIVER}'S FACE WHY [[=place]] BURNED."

stage face
  talk giver
  say "HE'S HERE. HE'S STANDING IN MY YARD WITH HIS HANDS EMPTY. HE SAYS MY [[=lost]] BURNED HIS FATHER'S HOUSE. ...I KNEW. I ALWAYS KNEW SOMETHING HAPPENED ON THAT LEVY. NOBODY WOULD SAY WHAT. WHAT DO I DO NOW?"
  opt "GIVE HIM TO THE REEVE'S ROPE." -> hanged_end
  opt "LET HIM GO HOME." -> freed_end
  opt "YOU BOTH LOST SOMEONE. START THERE." -> mourned_end

stage fought_end
  end success
  say "THE BAND BROKE. {CHIEF} WAS NOT AMONG THE DEAD; HE WAS SEEN AT DAWN WALKING EAST WITH A CHILD ON HIS SHOULDERS. {GIVER} HAD THE HORSES BACK, AND COUNTED THEM TWICE."
  journal "YOU BROKE {CHIEF}'S BAND AT {CAMP}. {CHIEF} ESCAPED. {GIVER} HAS THE HORSES, AND NO ANSWER."
  do reward fair
  do remember giver "THE HORSES ARE HOME. I KEEP WAITING TO FEEL BETTER. <<grief:vengeance>>"

stage hanged_end
  end success
  say "THEY HANGED HIM AT THE CROSSROADS. HE DID NOT STRUGGLE. {GIVER} WATCHED ALL OF IT, AND WENT HOME, AND SLEPT FOR THE FIRST TIME IN A MONTH, AND WOKE UP SCREAMING."
  journal "{CHIEF} WAS HANGED AT THE CROSSROADS OF {HOME}. {GIVER} HAS {GIVER.HIS} VENGEANCE."
  do reward fair
  do hide chief
  do mark hanged_the_raider
  do remember giver "IT'S DONE. IT'S DONE. I SAY IT EVERY NIGHT NOW INSTEAD OF HIS NAME."
  do fact "A RAIDER WAS HANGED AT THE CROSSROADS OF {HOME}. HIS PEOPLE SAY {HOME}'S LEVY BURNED THEM FIRST."

stage freed_end
  end success
  say "{GIVER} OPENED THE GATE. {CHIEF} BOWED, AND WENT, AND THE HORSES CAME BACK A WEEK LATER WITH NOBODY LEADING THEM, AND A SACK OF SEED CORN TIED TO EACH SADDLE."
  journal "{GIVER} LET {CHIEF} GO HOME. THE HORSES CAME BACK WITH SEED CORN."
  do reward fair
  do hide chief
  do remember giver "SEED CORN. FROM THEM. I PLANTED IT. I DON'T KNOW WHAT ELSE TO DO WITH IT. <<doubt>>"

stage mourned_end
  end success
  say "THEY SAT IN THE YARD UNTIL DARK, NOT TOUCHING, TALKING ABOUT FATHERS. IN SPRING, MEN FROM {CHIEF}'S CAMP CAME TO REBUILD [[=place]], AND NOBODY IN {HOME} THREW A STONE."
  journal "{GIVER} AND {CHIEF} MOURNED TOGETHER. [[=place]] OF {HOME} IS BEING REBUILT BY BOTH SIDES."
  do reward rich
  do hide chief
  do remember giver "THEY'RE GOOD WITH TIMBER, HIS PEOPLE. I HATE HOW GOOD THEY ARE. I BRING THEM BEER ANYWAY. <<saying>>"
  do fact "{HOME} AND THE RAIDERS OF {CAMP} REBUILT [[=place]] TOGETHER, AND SPEAK OF AN OLD LEVY NOBODY WILL DEFEND."
)SAGA";

const char* const kHuntedBand = R"SAGA(
title [[THE HUNTED BAND|THE BROKEN AND THE OUTCAST|ONE MORE AT THE FIRE]]
hook npc any
pitch "[[pitch=WHO ARE YOU HIDING FROM?|WHY DO YOU WATCH THE GATE?|YOU HAVE A BRANDED HAND.]]"
hint "[[~pitch|THE LORD'S HUNTERS ARE TWO DAYS BEHIND US. MAYBE ONE.|THE LORD'S HUNTERS ARE TWO DAYS BEHIND US. MAYBE ONE.|A THIEF WITH A BRAND, A BOY WHO CAN'T SPEAK, A SHIELD-MAIDEN WITHOUT A SHIELD, AND AN OLD PRIEST NOBODY WANTS. AND ME. THAT'S MY WARBAND.]]"
role giver giver
role home site home
role camp site camp near
role far site village far
role stray person female at camp
role hunter foe male at far
var took 0
slot t1 -> road world rival betrayal
slot t2 -> last_stand mercy betrayal price

stage start
  talk giver
  say "MY BAND IS HIDING AT {CAMP}: [[A BRANDED THIEF|A ONE-HANDED SMITH|A RUNAWAY SQUIRE]], A BOY WHO CAN'T SPEAK, AN OLD PRIEST, AND ME. THE LORD'S HUNTERS ARE BEHIND US. THERE'S SAFE LAND PAST {FAR}. WILL YOU WALK WITH US? <<plea:hope>>"
  opt "I'LL WALK WITH YOU." -> camp_go
  opt "WHAT DID YOU DO?" -> crime
  opt "OUTLAWS ARE OUTLAWS." -> refused

stage crime
  talk giver
  say "I SAID NO TO A LORD WHO WANTED MY SISTER. THE THIEF STOLE BREAD. THE BOY SAW SOMETHING. THE PRIEST TOLD THE TRUTH IN A SERMON. NONE OF US IS SORRY. <<oath>>"
  opt "THEN I'LL WALK WITH YOU." -> camp_go

stage refused
  end fail
  journal "YOU WOULD NOT WALK WITH {GIVER}'S HUNTED BAND."
  do remember giver "WE LOST THE PRIEST AT THE RIVER. HE SAID GO ON. WE WENT ON. <<grief>>"

stage camp_go
  goal goto camp
  then stray_talk
  journal "{GIVER}'S BAND OF THE BROKEN HIDES AT {CAMP}. MEET THEM AND LEAD THEM TOWARD {FAR}."

stage stray_talk
  talk stray
  say "PLEASE. I SAW YOUR FIRE. I'M HURT, AND ALONE, AND I CAN COOK, AND I CAN SHOOT. TAKE ME WITH YOU. ...YOUR BAND IS LOOKING AT ME LIKE I'M A SPY. I'M NOT. I'M RUNNING TOO."
  opt "COME WITH US." -> took_her
  opt "WE CAN'T RISK IT." -> left_her

stage took_her
  talk giver
  say "ONE MORE AT THE FIRE. THAT'S HOW WE ALL CAME. ...BUT IF SHE SELLS US, IT'S ON YOUR HEAD. <<warning>>"
  do set took 1
  opt "ON MY HEAD, THEN." -> @t1

stage left_her
  talk giver
  say "AYE. WE CAN'T. ...THE BOY LEFT HER HIS BLANKET. HE DOESN'T SPEAK, BUT HE DOES THAT. <<grief:shame>>"
  opt "WE MOVE." -> @t1

stage road
  goal goto far
  then @t2
  journal "LEAD {GIVER}'S BAND FROM {CAMP} TOWARD THE SAFE LAND PAST {FAR}. THE HUNTERS ARE CLOSE."

stage last_stand
  talk giver
  say "{HUNTER} AND HIS DOGS, AT THE LAST BRIDGE BEFORE {FAR}. WE CAN'T OUTRUN THEM. THE THIEF WANTS TO FIGHT. THE PRIEST WANTS TO PRAY. WHAT DO YOU WANT?"
  opt "WE FIGHT. TOGETHER." -> fight
  opt "SHE KNOWS HIM. LET HER TALK." if var took 1 -> stray_talks
  opt "I'LL HOLD THE BRIDGE. YOU RUN." -> fight

stage stray_talks
  talk stray
  say "HE'S MY FATHER'S HUNTSMAN. I RAN FROM THE SAME LORD YOU DID. IF I GO BACK WITH HIM, HE'LL TURN AROUND. HE WANTS ME MORE THAN ALL OF YOU. ...I'D DO IT. YOU TOOK ME IN."
  opt "NO. WE FIGHT FOR YOU TOO." -> fight
  opt "...THANK YOU." -> traded_end

stage fight
  goal slay hunter
  then free_end
  journal "{HUNTER}, THE LORD'S HUNTSMAN, HAS CAUGHT {GIVER}'S BAND AT THE LAST BRIDGE BEFORE {FAR}. STAND."

stage free_end
  end success
  say "THE DOGS SCATTERED WHEN {HUNTER} FELL. THE BAND CROSSED THE BRIDGE AT DAWN, AND THE BOY WHO DOESN'T SPEAK LAUGHED OUT LOUD, ONCE, AND NOBODY SAID A WORD ABOUT IT."
  journal "{GIVER}'S BAND OF THE BROKEN CROSSED INTO SAFE LAND PAST {FAR}."
  do reward rich
  do remember giver "WE'RE BUILDING A HALL. CROOKED, LEAKY, OURS. THERE'S A SEAT FOR YOU BY THE FIRE. <<thanks>>"
  do fact "A BAND OF THE BROKEN AND THE OUTCAST CROSSED INTO THE WILD PAST {FAR} AND BUILT A CROOKED HALL. THEY TAKE IN ANYONE."

stage traded_end
  end success
  say "SHE WALKED OUT ONTO THE BRIDGE ALONE. {HUNTER} PUT HIS CLOAK ROUND HER, AND LOOKED ACROSS AT THE BAND A LONG TIME, AND TURNED HIS DOGS FOR HOME."
  journal "THE STRAY GAVE HERSELF UP TO {HUNTER} SO {GIVER}'S BAND COULD CROSS INTO SAFETY."
  do reward fair
  do hide stray
  do hide hunter
  do remember giver "WE KEEP A PLACE FOR HER. THE BOY LAYS A BLANKET ON IT. EVERY NIGHT. <<vow>>"
)SAGA";

const char* const kLastVengeance = R"SAGA(
title [[VENGEANCE AT ANY PRICE|WHAT IS LEFT AFTER|THE LAST COIN FOR ONE DEATH]]
hook npc any
pitch "[[pitch=WHY IS YOUR HOUSE EMPTY?|YOU SOLD YOUR PLOUGH?|WHO ARE YOU HIRING SWORDS FOR?]]"
hint "[[~pitch|THERE'S NOTHING IN MY HOUSE BUT A NAME, AND I'M GOING TO SPEND IT.|I SOLD THE COW. THEN THE PLOUGH. THEN THE ROOF TILES. I HAVE FORTY SILVER AND ONE NAME. THAT'S ALL I NEED.|I SOLD THE COW. THEN THE PLOUGH. THEN THE ROOF TILES. I HAVE FORTY SILVER AND ONE NAME. THAT'S ALL I NEED.]]"
role giver giver
role home site home
role camp site camp near
role foe foe male at camp
role village site village near
role sister person female at village
var burned 0
slot t1 -> hunt rival world price
slot t2 -> after mercy betrayal price

stage start
  talk giver
  say "{FOE} AND HIS RIDERS KILLED MY [[WHOLE HOUSE|WIFE AND SONS|FATHER AND BROTHERS]] OVER A [[DEBT|BOUNDARY|WORD SAID AT MARKET]]. I'VE SOLD EVERYTHING. EVERYTHING. FORTY SILVER, AND IT'S YOURS IF HE DIES. <<vow:vengeance>>"
  opt "WHERE IS HE?" -> where
  opt "AND AFTER? WHAT WILL YOU HAVE?" -> after_ask
  opt "KEEP YOUR SILVER. AND YOUR HOUSE." -> refused

stage after_ask
  talk giver
  say "AFTER? THERE IS NO AFTER. THERE'S HIM, AND THEN THERE'S NOTHING, AND NOTHING WILL BE QUIET. <<grief:vengeance>>"
  opt "THEN WHERE IS HE?" -> where

stage refused
  end fail
  journal "YOU WOULD NOT TAKE {GIVER}'S LAST SILVER FOR {FOE}'S LIFE."
  do remember giver "SOMEONE ELSE WILL TAKE IT. SOMEONE ALWAYS DOES. <<curse>>"

stage where
  talk giver
  say "HE HOLDS UP AT {CAMP}. BUT HE NEVER LEAVES IT. SO: HIS SISTER {SISTER} LIVES IN {VILLAGE}. BURN HER BARN AND HE'LL COME RUNNING OUT. SHE'S NOTHING TO ME. NOTHING IS ANYTHING TO ME NOW."
  opt "I'LL BURN THE BARN." -> burn
  opt "NO. I'LL GO STRAIGHT IN." -> @t1
  opt "THIS ISN'T YOU. IT'S HIM, IN YOU." -> talk_down

stage talk_down
  talk giver
  say "NOT ME? WHAT IS ME, THEN? A MAN WHO PLOUGHED. A HOUSE THAT LAUGHED. ...STOP. STOP IT. I CAN'T AFFORD TO HEAR YOU. I'VE NOTHING LEFT TO PAY FOR IT WITH. <<grief>>"
  opt "THEN I'LL GO STRAIGHT IN." -> @t1
  opt "COME AWAY. LET HIM ROT." check level 4 -> walked_end else hunt_alone

stage hunt_alone
  talk giver
  say "NO. I'M GOING MYSELF. TONIGHT. WITH A PITCHFORK. YOU CAN COME OR NOT. <<threat:vengeance>>"
  opt "THEN I COME." -> @t1

stage burn
  goal goto village
  then burned_talk
  do set burned 1
  journal "{GIVER} WANTS {SISTER}'S BARN IN {VILLAGE} BURNED TO DRAW {FOE} OUT OF {CAMP}."

stage burned_talk
  talk sister
  say "MY BARN. MY BARN! THE GOATS WERE IN IT. ...YOU. YOU DID THIS FOR MY BROTHER? GOOD. KILL HIM. HE KILLED MY HUSBAND TOO, YEARS AGO. NOBODY BURNED A BARN FOR ME. <<curse>>"
  opt "...I'M SORRY." -> @t1

stage hunt
  goal slay foe
  then @t2
  journal "{FOE}, WHO KILLED {GIVER}'S HOUSEHOLD, HOLDS {CAMP}. END HIM."

stage after
  talk giver
  say "HE'S DEAD? HE'S DEAD. ...I THOUGHT IT WOULD BE LOUDER. I HAVE NOTHING. NO COW, NO ROOF, NO ONE. THE SILVER'S YOURS. AND THEN I DON'T KNOW WHAT."
  opt "KEEP THE SILVER. REBUILD." -> rebuild_end
  opt "I'LL TAKE IT. IT WAS THE DEAL." -> paid_end

stage walked_end
  end success
  say "{GIVER} SAT DOWN IN THE ROAD AND WEPT LIKE A CHILD, AND YOU SAT WITH {GIVER.HIM}. {FOE} STILL HOLDS {CAMP}. {GIVER} BOUGHT BACK THE COW."
  journal "YOU TALKED {GIVER} OUT OF VENGEANCE. {FOE} LIVES. SO DOES {GIVER}."
  do reward small
  do remember giver "THE COW KNEW ME. STUPID BEAST. I CRIED ON IT. <<comfort:grief>>"

stage rebuild_end
  end success
  say "YOU PUT THE SILVER BACK IN {GIVER}'S HAND AND CLOSED {GIVER.HIS} FINGERS ON IT. IN SPRING {GIVER} BOUGHT A PLOUGH. IT WAS NOT THE SAME PLOUGH. IT PLOUGHED."
  journal "{FOE} IS DEAD. YOU WOULD NOT TAKE {GIVER}'S LAST SILVER. {GIVER} IS PLOUGHING AGAIN."
  do reward fair
  do remember giver "NEW PLOUGH. OLD FIELD. THE FURROWS ARE CROOKED. MY HANDS SHAKE. I'M PLOUGHING. <<thanks:hope>>"
  do fact "{FOE}, WHO HELD {CAMP}, IS DEAD. THE ONE WHO PAID FOR IT PLOUGHS NEAR {HOME} AND DOES NOT SPEAK OF IT."

stage paid_end
  end success
  say "{GIVER} COUNTED THE FORTY INTO YOUR HAND ONE COIN AT A TIME, AND THEN WALKED INTO THE EMPTY HOUSE AND SHUT THE DOOR THAT WAS NO LONGER ON ITS HINGES. NOBODY KNOWS WHERE {GIVER.HE} WENT."
  journal "{FOE} IS DEAD. {GIVER} PAID YOU EVERYTHING {GIVER.HE} HAD LEFT, AND IS GONE."
  do gold 40
  do mark took_the_last_silver
  do fact "THE HOUSE OF {GIVER} IN {HOME} STANDS EMPTY. THEY SAY THE OWNER SOLD EVERYTHING FOR ONE DEATH, AND GOT IT."
)SAGA";

void add(std::vector<Archetype>& v, const char* id, const char* name, uint32_t themes, uint32_t needs, uint16_t motives,
         uint32_t twists, const char* body) {
  Archetype a;
  a.id = id;
  a.name = name;
  a.source = Source::Gwynne;
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

void addGwynne(std::vector<Archetype>& v) {
  using M = Motive;
  add(v, "false_light", "TWO BANNERS OF LIGHT", TH_FAITH | TH_BETRAYAL | TH_JUDGEMENT | TH_WAR | TH_PROPHECY, N_CAMP,
      mb(M::Devotion, M::Fear, M::Duty, M::Hope),
      TF_RIVAL | TF_BETRAYAL | TF_WORLD | TF_PROPHECY | TF_MERCY, kFalseLight);
  add(v, "shield_wall", "THE SHIELD WALL AT THE FORD", TH_COURAGE | TH_LOYALTY | TH_SACRIFICE | TH_WAR | TH_MERCY, N_CAMP,
      mb(M::Duty, M::Pride, M::Devotion), TF_BETRAYAL | TF_PRICE | TF_WORLD | TF_RIVAL | TF_MERCY, kShieldWall);
  add(v, "pit_hound", "THE BEAST RAISED FROM A PUP", TH_LOYALTY | TH_LOVE | TH_COURAGE | TH_HOMECOMING | TH_FRIENDSHIP, N_CAMP,
      mb(M::Love, M::Grief, M::Hope), TF_BETRAYAL | TF_RIVAL | TF_WORLD | TF_MERCY | TF_PRICE | TF_RETURN, kPitHound);
  add(v, "giant_stones", "THE GIANT'S GRUDGE", TH_GRIEF | TH_VENGEANCE | TH_JUDGEMENT | TH_MERCY | TH_DOOM, N_RUIN,
      mb(M::Fear, M::Greed, M::Shame, M::Duty), TF_DECEIT | TF_IDENTITY | TF_WONDER | TF_MERCY | TF_PRICE | TF_WORLD, kGiantStones);
  add(v, "master_traitor", "THE MASTER WHO TURNED", TH_BETRAYAL | TH_LOYALTY | TH_JUDGEMENT | TH_GREED | TH_COURAGE, N_TOWN,
      mb(M::Grief, M::Shame, M::Duty, M::Love), TF_DECEIT | TF_RIVAL | TF_BETRAYAL | TF_MERCY | TF_PRICE, kMasterTraitor);
  add(v, "two_hosts", "THE WINGED AND THE SCORCHED", TH_WAR | TH_TEMPTATION | TH_DOOM | TH_WONDER | TH_PROPHECY, N_CAVE,
      mb(M::Fear, M::Love, M::Devotion), TF_DECEIT | TF_BETRAYAL | TF_PROPHECY | TF_PRICE | TF_MERCY | TF_WONDER, kTwoHosts);
  add(v, "enemy_reasons", "THE RAIDER'S REASONS", TH_VENGEANCE | TH_MERCY | TH_GRIEF | TH_JUDGEMENT | TH_REDEMPTION, N_CAMP,
      mb(M::Vengeance, M::Grief, M::Shame), TF_RIVAL | TF_WORLD | TF_BETRAYAL | TF_MERCY | TF_PRICE, kEnemyReasons);
  add(v, "hunted_band", "THE HUNTED BAND", TH_FRIENDSHIP | TH_EXILE | TH_COURAGE | TH_SACRIFICE | TH_FREEDOM, N_CAMP | N_VILLAGE,
      mb(M::Hope, M::Fear, M::Duty, M::Love), TF_WORLD | TF_RIVAL | TF_BETRAYAL | TF_MERCY | TF_PRICE, kHuntedBand);
  add(v, "last_vengeance", "VENGEANCE AT ANY PRICE", TH_VENGEANCE | TH_GRIEF | TH_SACRIFICE | TH_TEMPTATION | TH_DOOM,
      N_CAMP | N_VILLAGE, mb(M::Vengeance, M::Grief, M::Pride), TF_RIVAL | TF_WORLD | TF_PRICE | TF_MERCY | TF_BETRAYAL,
      kLastVengeance);
}

}  // namespace arch
}  // namespace saga
}  // namespace story
