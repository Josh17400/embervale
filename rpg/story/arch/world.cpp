// M6b "Sagas": archetypes native to the simulated world (famine, deserters, refugees, guilds, alloys, beasts). VOICE
// lane. The template markup and role conventions: rpg/story/saga.h.
//
//   hoarding_steward  THE STEWARD'S BARNS        a hungry year, a lord's steward sitting on nine carts of grain
//   deserter          THE DESERTER IN THE LOFT   a soldier of the levy hiding at home, and the sergeant asking
//   refugees          THE FAMILY BY THE WELL     strangers from a lost town camp by the well; something goes missing
//   guild_sold        THE GUILD'S BETRAYAL       the guildmaster sold the guild's secret to a rival town
//   stolen_temper     THE STOLEN TEMPER          a smith's book of the people's own steel, carried off by an unpaid
//                                                journeyman (the reward of the best endings: a smithing secret)
//   beast_children    THE BEAST THAT TOOK THE CHILDREN   the region's named beast took children; the den, and the cubs
#include <vector>
#include "rpg/story/arch/arch_tables.h"

namespace story {
namespace saga {
namespace arch {

namespace {

const char* const kHoardingSteward = R"SAGA(
title [[THE STEWARD'S BARNS|NINE CARTS OF GRAIN|THE HUNGRY YEAR]]
hook npc any
pitch "[[pitch=WHY ARE THE CHILDREN SO THIN?|WHAT'S IN THE STEWARD'S BARN?|IS THERE BREAD ANYWHERE?]]"
hint "[[~pitch|IT'S A HUNGRY YEAR FOR SOME OF US. NOT FOR ALL OF US.|MY YOUNGEST CHEWS BARK. BARK. AND THE STEWARD'S BARN IS SO FULL THE DOORS BULGE.|IT'S A HUNGRY YEAR FOR SOME OF US. NOT FOR ALL OF US.]]"
role giver giver
role home site home
role steward person male at home
role court site town near
role justice person [[male|female]] at court
slot t1 -> court_go world rival deceit
slot t2 -> raid_talk betrayal price mercy

stage start
  talk giver
  say "THE HARVEST FAILED AND {HOME} IS STARVING BY INCHES. THE LORD'S STEWARD, {STEWARD}, HOLDS [[carts=NINE|TWELVE|SEVEN]] CARTS OF GRAIN FOR THE LORD'S TABLE AND THE DEAR MARKET IN {COURT}. MY CHILDREN CHEW BARK. <<plea:fear>>"
  opt "I'LL TALK TO {STEWARD}." -> steward_talk
  opt "I'LL TAKE IT TO THE LORD'S JUSTICE." -> @t1
  opt "IT'S THE LORD'S GRAIN." -> refused

stage refused
  end fail
  journal "YOU TOLD {GIVER} THAT THE STEWARD'S GRAIN IS THE LORD'S GRAIN."
  do remember giver "THE GRAIN WENT TO {COURT} ON NINE CARTS. WE WATCHED IT GO. NOBODY SAID ANYTHING. <<curse>>"

stage steward_talk
  talk steward
  say "THE LORD'S GRAIN IS THE LORD'S GRAIN. IF I OPEN ONE CART, THEY'LL HAVE ALL NINE BY NIGHTFALL, AND THE LORD WILL HAVE MY HANDS. <<refusal:fear>>"
  opt "ONE CART. FOR THE CHILDREN." check fame 1 -> one_cart_end else steward_no
  opt "THEN THE LORD'S JUSTICE HEARS IT." -> @t1
  opt "THEN IT WILL BE TAKEN." -> raid

stage steward_no
  talk steward
  say "WHO ARE YOU TO ASK? I DON'T KNOW YOUR FACE. NOBODY IN {COURT} KNOWS YOUR FACE. GO AWAY. <<insult>>"
  opt "THEN THE LORD'S JUSTICE HEARS IT." -> @t1
  opt "THEN IT WILL BE TAKEN." -> raid

stage raid
  goal wait 1
  then @t2
  journal "{HOME} WILL TAKE THE STEWARD'S GRAIN BY NIGHT. YOU WILL BE AT THE FRONT OF THEM."

stage raid_talk
  talk giver
  say "WE TOOK IT. ALL [[=carts]] CARTS, AND NOBODY HURT BUT THE STEWARD'S PRIDE. BUT HE SAW YOUR FACE, AND HE'LL SEND FOR THE LORD'S RIDERS. WHAT NOW?"
  opt "SHARE IT FAIRLY, OPENLY." -> raided_end
  opt "HIDE IT UNDER EVERY FLOOR." -> hidden_end

stage court_go
  goal goto court
  then justice_talk
  journal "{STEWARD}, STEWARD OF {HOME}, HOARDS GRAIN IN A HUNGRY YEAR. TAKE IT TO THE LORD'S JUSTICE IN {COURT}."

stage justice_talk
  talk justice
  say "A STEWARD SITTING ON GRAIN IN A HUNGRY YEAR? THE LORD WOULDN'T LIKE IT KNOWN. I CAN ORDER IT SOLD AT LAST YEAR'S PRICE. OR I CAN ORDER IT GIVEN OUT, AND THE LORD WILL WANT TO KNOW WHY. CHOOSE."
  opt "SOLD FAIR. NOBODY LOSES A HEAD." -> fair_end
  opt "GIVEN OUT. LET THE LORD ASK." -> given_end

stage one_cart_end
  end success
  say "{STEWARD} OPENED ONE CART, AND WATCHED IT EMPTIED, AND WEPT, AND OPENED A SECOND ONE WITHOUT BEING ASKED. NOBODY IN {HOME} DIED THAT WINTER."
  journal "THE STEWARD OF {HOME} OPENED HIS CARTS TO THE HUNGRY. NOBODY DIED THAT WINTER."
  do reward fair
  do remember steward "TWO CARTS SHORT. THE LORD FINED ME HALF A YEAR'S PAY. THE CHILDREN WAVE AT ME NOW. CHEAP AT THE PRICE."
  do fact "IN THE HUNGRY YEAR THE STEWARD OF {HOME} OPENED THE LORD'S GRAIN TO THE TOWN. HE WAS FINED. HE WAS ALSO FORGIVEN."

stage raided_end
  end success
  say "THE GRAIN WAS PILED IN THE SQUARE AND MEASURED OUT BY THE BOWL, AND EVERY FAMILY SIGNED FOR WHAT IT TOOK. WHEN THE LORD'S RIDERS CAME, THEY FOUND A LEDGER, AND A TOWN THAT WOULD NOT HAND ANYONE OVER."
  journal "{HOME} TOOK THE STEWARD'S GRAIN AND SHARED IT OPENLY. THE LORD'S RIDERS FOUND NOBODY TO HANG."
  do reward fair
  do mark took_the_lords_grain
  do remember giver "THEY ASKED WHO LED IT. THE WHOLE SQUARE SAID ME. EVERY ONE OF US. <<boast:pride>>"
  do fact "IN THE HUNGRY YEAR {HOME} TOOK THE LORD'S GRAIN AND SIGNED FOR EVERY BOWL. THE LEDGER HANGS IN THE INN."

stage hidden_end
  end success
  say "THE GRAIN VANISHED UNDER FLOORBOARDS AND INTO WELLS. THE LORD'S RIDERS TURNED {HOME} INSIDE OUT AND FOUND NOTHING, AND TOOK THE STEWARD AWAY INSTEAD, FOR CARELESSNESS."
  journal "{HOME} HID THE STOLEN GRAIN. THE LORD'S RIDERS TOOK THE STEWARD AWAY FOR LOSING IT."
  do reward small
  do hide steward
  do mark outlaw_of_the_hungry_year
  do remember giver "WE EAT. QUIETLY. NOBODY TALKS ABOUT THE STEWARD. NOBODY TALKS ABOUT YOU, EITHER, IF ANYONE ASKS."

stage fair_end
  end success
  say "THE GRAIN WAS SOLD IN {HOME}'S SQUARE AT LAST YEAR'S PRICE, AND THE STEWARD STOOD BY AND COUNTED EVERY COIN, AND HATED YOU WITH HIS WHOLE FACE."
  journal "THE LORD'S JUSTICE ORDERED THE STEWARD'S GRAIN SOLD FAIRLY IN {HOME}."
  do reward fair
  do remember giver "LAST YEAR'S PRICE. WE CAN PAY THAT, JUST. <<relief:hope>>"
  do fact "THE LORD'S JUSTICE IN {COURT} ORDERED A STEWARD'S HOARD SOLD FAIR IN {HOME} DURING THE HUNGRY YEAR."

stage given_end
  end success
  say "THE GRAIN WAS GIVEN OUT ON THE JUSTICE'S SEAL. THE LORD WROTE, ANGRILY, AND THE JUSTICE WROTE BACK, POLITELY, AND THAT WAS THE LAST ANYONE HEARD OF THE JUSTICE IN {COURT}."
  journal "THE LORD'S JUSTICE GAVE AWAY THE STEWARD'S GRAIN. {HOME} EATS. THE JUSTICE HAS NOT BEEN SEEN SINCE."
  do reward rich
  do hide justice
  do remember giver "THEY SAY THE JUSTICE WAS SENT TO A FORT ON THE BORDER. WE SEND BREAD THERE. A LOT OF BREAD. <<vow>>"
  do fact "A JUSTICE OF {COURT} GAVE AWAY A LORD'S GRAIN TO {HOME} IN A HUNGRY YEAR, AND WAS SENT AWAY FOR IT."
)SAGA";

const char* const kDeserter = R"SAGA(
title [[THE DESERTER IN THE LOFT|THE SERGEANT AT THE DOOR|HOME FROM THE LEVY]]
hook npc any
pitch "[[pitch=WHY DO YOU KEEP THE LOFT LOCKED?|WHO'S THE SERGEANT ASKING ABOUT?|YOU'RE COOKING FOR TWO.]]"
hint "[[~pitch|THEY CALL IT DESERTING. I CALL IT COMING HOME.|THE SERGEANT CAME BY AGAIN. SMILED. ASKED ABOUT MY HEALTH. HE KNOWS. HE MUST KNOW.|THEY CALL IT DESERTING. I CALL IT COMING HOME.]]"
role giver giver
role home site home
role kin resident kin of giver
role land kingdom home
role sergeant person male at home
role far site village far
slot t1 -> smuggle world rival deceit
slot t2 -> sergeant_talk betrayal mercy price

stage start
  talk giver
  say "MY {KIN.KIN} {KIN} MARCHED WITH THE LEVY OF {LAND} IN SPRING AND WALKED HOME IN SUMMER, THIN AS A RAKE AND NOT SPEAKING. {KIN.HE}'S IN MY LOFT. THE SERGEANT, {SERGEANT}, IS HUNTING DESERTERS, AND DESERTERS HANG. <<plea:fear>>"
  opt "LET ME TALK TO {KIN}." -> kin_talk
  opt "I'LL GET {KIN.HIM} AWAY." -> @t1
  opt "THE LAW IS THE LAW." -> turned

stage kin_talk
  talk kin
  say "I DIDN'T RUN FROM THE ENEMY. I RAN FROM US. FROM WHAT THE CAPTAIN HAD US DO TO A VILLAGE THAT COULDN'T PAY. I'LL GO BACK IF YOU MAKE ME. I WON'T DO IT AGAIN. <<grief:shame>>"
  opt "THEN WE GET YOU TO {FAR}." -> @t1
  opt "GO BACK AND SAY WHY YOU LEFT." -> @t2
  opt "STAY HIDDEN. I'LL HANDLE THE SERGEANT." -> @t2

stage turned
  talk sergeant
  say "YOU'RE TURNING IN SOMEONE ELSE'S {KIN.SON}? HM. MOST FOLK HIDE THEM. I'LL TAKE {KIN.HIM} BACK TO THE LINE, NOT THE ROPE, IF {KIN.HE} COMES QUIET. THE ROPE IS FOR THE SECOND TIME. <<warning:duty>>"
  opt "THEN TAKE {KIN.HIM} BACK." -> turned_end

stage smuggle
  goal goto far
  then smuggled_end
  do moves kin far
  journal "{KIN} OF {HOME} DESERTED THE LEVY OF {LAND}. GET {KIN.HIM} TO {FAR}, WHERE NOBODY KNOWS {KIN.HIS} FACE."

stage sergeant_talk
  talk sergeant
  say "AH. YOU'RE THE ONE STAYING AT {GIVER}'S. YOU'LL HAVE SEEN {KIN}, THEN. THIN. QUIET. KEEPS LOOKING AT {KIN.HIS} OWN HANDS. TELL ME THE TRUTH, AND I'LL BE FAIR. LIE, AND I'LL BE THOROUGH."
  opt "{KIN.HE} LEFT FOR SHAME. HEAR WHY." -> witness
  opt "NEVER SAW {KIN.HIM}." check level 3 -> hidden_end else caught_end

stage witness
  talk kin
  say "I'LL SAY IT. TO HIM. ...SERGEANT. AT THE FORD VILLAGE, THE CAPTAIN HAD US BAR THE DOORS. I UNBARRED ONE. THEN I RAN. HANG ME IF YOU LIKE. I'D UNBAR IT AGAIN."
  opt "WELL, SERGEANT?" -> returned_end

stage turned_end
  end success
  say "{KIN} WENT BACK TO THE LINE BETWEEN TWO SOLDIERS, NOT LOOKING AT ANYONE. {GIVER} STOOD IN THE DOOR AND DID NOT WAVE."
  journal "YOU HANDED {KIN} OF {HOME} BACK TO THE LEVY OF {LAND}. {GIVER} WILL NOT FORGET IT."
  do reward small
  do rep land 3
  do remember giver "THE LOFT'S EMPTY. YOU DID THAT. I HOPE THE LAW KEEPS YOU WARM. <<curse>>"

stage smuggled_end
  end success
  say "{KIN} STOOD IN THE ROAD AT {FAR} AND LOOKED BACK THE WAY YOU CAME FOR A LONG TIME. THEN {KIN.HE} TOOK WORK AT A FARM UNDER ANOTHER NAME, AND SENDS {GIVER} AN EGG EVERY MARKET DAY. JUST AN EGG."
  journal "{KIN} DESERTED THE LEVY OF {LAND} AND LIVES IN {FAR} UNDER ANOTHER NAME."
  do reward fair
  do rep land -3
  do remember giver "AN EGG EVERY MARKET DAY. NO NOTE. I KNOW WHO IT'S FROM. <<relief:love>>"
  do fact "A DESERTER OF THE LEVY OF {LAND} LIVES IN {FAR} UNDER ANOTHER NAME. THE SERGEANT HAS STOPPED ASKING."

stage hidden_end
  end success
  say "THE SERGEANT LOOKED AT YOU FOR A LONG MOMENT, THEN AT THE LOFT, THEN BACK AT YOU. NEVER SAW {KIN.HIM}, HE SAID. NOR I. AND HE WALKED AWAY WHISTLING SOMETHING FROM THE LEVY."
  journal "THE SERGEANT CHOSE NOT TO FIND {KIN} IN {GIVER}'S LOFT. {KIN} IS HOME."
  do reward fair
  do befriend kin
  do remember kin "I COME DOWN FROM THE LOFT AT NIGHT NOW. I SIT BY THE FIRE. THE SERGEANT WAVES WHEN HE PASSES. <<thanks:fear>>"

stage caught_end
  end fail
  say "THE SERGEANT SMILED AND SENT TWO MEN UP THE LADDER. {KIN} CAME DOWN WITHOUT A FIGHT. THE SERGEANT LOOKED AT YOU AND SAID: THOROUGH, AS PROMISED."
  journal "YOU LIED TO THE SERGEANT AND {KIN} WAS FOUND IN {GIVER}'S LOFT ANYWAY."
  do rep land -5
  do remember giver "THEY TOOK {KIN.HIM} AT DAWN. YOU TRIED. I KNOW YOU TRIED. <<grief>>"

stage returned_end
  end success
  say "THE SERGEANT TOOK OFF HIS HELMET AND SAT DOWN ON THE STEP. I UNBARRED NONE, HE SAID. I'VE DREAMED ABOUT IT EVERY NIGHT. {KIN} WAS DISCHARGED SICK, AND THE CAPTAIN OF THE FORD VILLAGE HAS SOME QUESTIONS TO ANSWER."
  journal "{KIN} TOLD THE SERGEANT WHY {KIN.HE} LEFT THE LEVY OF {LAND}. {KIN.HE} IS DISCHARGED, AND A CAPTAIN IS NOT."
  do reward rich
  do befriend kin
  do rep land 3
  do fact "A CAPTAIN OF THE LEVY OF {LAND} ANSWERS FOR A BARRED VILLAGE. A DESERTER OF {HOME} TOLD THE TRUTH."
)SAGA";

const char* const kRefugees = R"SAGA(
title [[THE FAMILY BY THE WELL|STRANGERS FROM A LOST TOWN|NOT OUR KIND]]
hook npc any
pitch "[[pitch=WHO'S CAMPED BY THE WELL?|WHY IS EVERYONE WHISPERING?|WHERE DID THOSE CHILDREN COME FROM?]]"
hint "[[~pitch|NOBODY KNOWS THEIR NAMES. NOBODY HAS ASKED.|NOBODY KNOWS THEIR NAMES. NOBODY HAS ASKED.|THEY CAME IN ON FOOT WITH WHAT THEY COULD CARRY. A MOTHER AND HER LITTLE ONES. HALF THE STREET WANTS THEM GONE BY SUNDAY.]]"
role giver giver
role home site home
role mother person female at home
role neighbour resident any
role town site town near
role abbess person female at town
slot t1 -> wait_truth deceit rival world
slot t2 -> abbey mercy price

stage start
  talk giver
  say "A FAMILY CAME IN FROM A TOWN THAT [[lost=BURNED|DROWNED IN THE FLOOD|WAS TAKEN BY THE ENEMY]]. A MOTHER, {MOTHER}, AND HER LITTLE ONES, CAMPED BY THE WELL. NOW {NEIGHBOUR} SAYS [[thing=A HAM|A SILVER SPOON|A GOOD KNIFE]] WENT MISSING, AND WANTS THEM OUT. <<doubt>>"
  opt "LET ME TALK TO {MOTHER}." -> mother_talk
  opt "LET ME TALK TO {NEIGHBOUR}." -> neighbour_talk
  opt "THEY'RE NOT MY CONCERN." -> refused

stage refused
  end fail
  journal "YOU LEFT THE STRANGERS BY THE WELL OF {HOME} TO WHATEVER {HOME} DECIDES."
  do remember giver "THEY WENT ON SUNDAY. THE LITTLEST WAVED. NOBODY WAVED BACK. I DIDN'T EITHER. <<grief:shame>>"
  do fact "A FAMILY FROM A TOWN THAT [[=lost]] WAS TURNED AWAY FROM THE WELL OF {HOME}."

stage mother_talk
  talk mother
  say "WE TOOK NOTHING. I KNOW HOW WE LOOK. I KNOW WHAT I'D THINK. WE HAD A HOUSE ONCE, WITH A BLUE DOOR. NOW WE HAVE A BLANKET AND THE WELL. GIVE ME WORK. ANY WORK. <<plea:hope>>"
  opt "I'LL FIND OUT WHAT HAPPENED." -> @t1
  opt "THERE'S AN ABBEY IN {TOWN}." -> @t2
  opt "{GIVER} HAS A BARN." -> barn_ask

stage neighbour_talk
  talk neighbour
  say "THEY TOOK [[=thing]]. WHO ELSE? NOBODY STOLE IN {HOME} BEFORE THEY CAME. I'VE GOT CHILDREN TOO, YOU KNOW. <<warning:fear>>"
  opt "I'LL FIND OUT WHO TOOK IT." -> @t1
  opt "AND IF THEY DIDN'T?" -> @t1

stage wait_truth
  goal wait 1
  then truth
  journal "{NEIGHBOUR} OF {HOME} SAYS THE STRANGERS BY THE WELL STOLE [[=thing]]. WATCH, AND FIND OUT."

stage truth
  talk neighbour
  say "...IT WAS MY OWN BOY. [[=thing]] UNDER HIS BED, AND HALF THE PANTRY. HE SAID HE WAS FEEDING THE STRANGERS' CHILDREN. HE'S EIGHT. HE SAID THEY WERE HUNGRY. <<apology:shame>>"
  opt "SAY THAT AT THE WELL." -> cleared_end
  opt "KEEP IT QUIET. FEED THEM TOO." -> quiet_end

stage barn_ask
  talk giver
  say "MY BARN? ...IT LEAKS AT ONE END AND THE GOAT IS RUDE. BUT IT'S DRY AT THE OTHER. THE NEIGHBOURS WILL TALK. LET THEM. <<vow>>"
  opt "THEN THE BARN." -> barn_end

stage abbey
  goal goto town
  then abbess_talk
  do moves mother town
  journal "TAKE {MOTHER} AND HER CHILDREN FROM {HOME} TO THE ABBEY IN {TOWN}."

stage abbess_talk
  talk abbess
  say "FOUR MORE MOUTHS? WE HAVE FORTY ALREADY FROM THAT SAME TOWN. ...OF COURSE THEY'LL STAY. WE'LL WATER THE SOUP. WE ALWAYS DO. <<saying>>"
  opt "THANK YOU." -> abbey_end

stage cleared_end
  end success
  say "{NEIGHBOUR} SAID IT AT THE WELL, RED TO THE EARS, WITH THE BOY BESIDE {NEIGHBOUR.HIM}. NOBODY SPOKE. THEN SOMEONE BROUGHT BREAD, AND SOMEONE ELSE BROUGHT A BLANKET, AND THE WELL WAS A KITCHEN BY DARK."
  journal "THE STRANGERS BY THE WELL OF {HOME} STOLE NOTHING. {NEIGHBOUR} SAID SO, PUBLICLY."
  do reward fair
  do befriend neighbour
  do remember mother "{NEIGHBOUR}'S BOY BRINGS US APPLES. HE STEALS THEM. WE'RE WORKING ON THAT. <<thanks:hope>>"
  do fact "A FAMILY FROM A TOWN THAT [[=lost]] LIVES IN {HOME} NOW. THEIR DOOR IS PAINTED BLUE."

stage quiet_end
  end success
  say "{NEIGHBOUR} SAID NOTHING AT THE WELL, BUT A BASKET WENT TO THE STRANGERS EVERY EVENING. IN TIME THE WHISPERS STOPPED. NOBODY EVER SAID SORRY. NOBODY EVER SAID THIEF AGAIN, EITHER."
  journal "{NEIGHBOUR} QUIETLY FEEDS THE STRANGERS {NEIGHBOUR.HE} ACCUSED. {HOME} HAS STOPPED WHISPERING."
  do reward fair
  do befriend neighbour
  do remember neighbour "DON'T LOOK AT ME LIKE THAT. IT'S ONLY A BASKET. ...IT'S TWO BASKETS NOW."

stage barn_end
  end success
  say "{MOTHER} SWEPT THE BARN BEFORE SHE SLEPT IN IT. BY SPRING SHE WAS SPINNING FOR HALF {HOME}, AND THE RUDE GOAT FOLLOWED HER CHILDREN EVERYWHERE."
  journal "{MOTHER} AND HER CHILDREN LIVE IN {GIVER}'S BARN IN {HOME}. SHE SPINS FOR HALF THE TOWN."
  do reward fair
  do remember giver "BEST SPINNER IN {HOME}, AND SHE SLEEPS NEXT TO MY GOAT. LIFE IS VERY STRANGE. <<proverb>>"
  do fact "A WOMAN FROM A TOWN THAT [[=lost]] SPINS THE FINEST THREAD IN {HOME}, AND LIVES IN A BARN BY CHOICE."

stage abbey_end
  end success
  say "THE ABBEY TOOK THEM IN. {MOTHER} WAS SCRUBBING POTS BEFORE YOU LEFT THE GATE, AND THE CHILDREN WERE ALREADY BEING TAUGHT THEIR LETTERS, LOUDLY, AGAINST THEIR WILL."
  journal "{MOTHER} AND HER CHILDREN ARE SAFE IN THE ABBEY OF {TOWN}, AWAY FROM {HOME}'S WHISPERS."
  do reward fair
  do remember abbess "THE ELDEST CAN READ NOW. SHE READS EVERYTHING. THE WALLS. MY LETTERS. I MAY REGRET IT."
)SAGA";

const char* const kGuildSold = R"SAGA(
title [[THE GUILD'S BETRAYAL|SOLD DOWN THE ROAD|THE GUILDMASTER'S NEW HOUSE]]
hook npc any
pitch "[[pitch=WHY IS THE GUILD HALL SHUT?|WHO BUILT THAT NEW HOUSE?|YOU'RE SELLING YOUR TOOLS?]]"
hint "[[~pitch|TWENTY YEARS IN THE GUILD. NOW ANOTHER TOWN SELLS OUR WORK CHEAPER THAN WE CAN BUY THE THREAD. HOW?|THE GUILDMASTER BUILT A NEW HOUSE THIS SPRING. WITH WHAT? THAT'S WHAT I'D LIKE TO KNOW.|TWENTY YEARS IN THE GUILD. NOW ANOTHER TOWN SELLS OUR WORK CHEAPER THAN WE CAN BUY THE THREAD. HOW?]]"
role giver giver
role home site home
role master person male at home
role town site town near
role buyer person [[male|female]] at town
slot t1 -> town_go deceit rival world
slot t2 -> reckoning betrayal price mercy

stage start
  talk giver
  say "OUR GUILD'S [[craft=MADDER-RED DYE|PATTERN BOOK|SECRET GLAZE]] WAS OURS ALONE FOR [[A HUNDRED|SIXTY|NINETY]] YEARS. NOW A MERCHANT OF {TOWN} SELLS IT CHEAPER THAN WE CAN MAKE IT. AND OUR GUILDMASTER, {MASTER}, HAS A NEW HOUSE. <<doubt>>"
  opt "I'LL FIND OUT IN {TOWN}." -> @t1
  opt "ASK {MASTER} STRAIGHT OUT." -> master_talk
  opt "TRADE IS TRADE." -> refused

stage master_talk
  talk master
  say "SOLD IT? I? I'VE GIVEN THIS GUILD THIRTY YEARS. THE {TOWN} MERCHANTS STOLE IT, PLAIN AS DAY. MY HOUSE IS MY WIFE'S INHERITANCE. GO AND ASK THEM IF YOU DOUBT ME. <<oath>>"
  opt "I WILL." -> @t1

stage refused
  end fail
  journal "YOU TOLD {GIVER} THAT TRADE IS TRADE."
  do remember giver "I SOLD MY LOOM. THE GUILD HALL IS A GRAIN STORE NOW. {MASTER} HAS A NEW CARRIAGE."

stage town_go
  goal goto town
  then buyer_talk
  journal "THE GUILD OF {HOME} HAS LOST ITS [[=craft]] TO A MERCHANT OF {TOWN}. FIND OUT HOW."

stage buyer_talk
  talk buyer
  say "STOLE IT? I PAID FOR IT. [[FORTY GOLD|A HOUSE'S WORTH|TWO HUNDRED SILVER]], TO YOUR GUILDMASTER, {MASTER}, IN HIS OWN KITCHEN. I HAVE HIS LETTER, IF YOU LIKE. I'M A MERCHANT, NOT A SAINT. <<boast:greed>>"
  opt "GIVE ME THE LETTER." check level 2 -> letter else no_letter
  opt "SELL THE SECRET BACK TO {HOME}." -> buyback

stage letter
  talk buyer
  say "HERE. TAKE IT. I DON'T LIKE A MAN WHO SELLS HIS OWN, EVEN WHEN I'M BUYING. TELL HIM I SAID SO."
  do give "THE GUILDMASTER'S LETTER" letter
  opt "I'LL TELL HIM." -> @t2

stage no_letter
  talk buyer
  say "THE LETTER? WHY WOULD I GIVE YOU THAT? IT'S MY RECEIPT. ...BUT I'LL TELL ANY JUDGE WHAT I TOLD YOU. FOR A FEE. <<refusal:greed>>"
  opt "THEN I'LL GO BACK WITH YOUR WORD." -> @t2

stage buyback
  talk buyer
  say "SELL IT BACK? IT'S MAKING ME RICH. ...BUT I COULD SHARE IT. YOUR GUILD WORKS IT TOO, AND WE SPLIT THE NORTHERN MARKETS. EVERYONE EATS. EXCEPT YOUR GUILDMASTER, WHO HAS EATEN ALREADY."
  opt "SHARED. I'LL TAKE IT HOME." -> shared_end
  opt "NO. I'LL GO BACK FOR THE TRUTH." -> @t2

stage reckoning
  talk giver
  say "{MASTER} SOLD US. FOR A HOUSE. I SAT AT HIS TABLE AT EVERY GUILD FEAST FOR TWENTY YEARS. ...THE GUILD MEETS TONIGHT. WHAT DO WE DO WITH HIM?"
  opt "SHOW THE GUILD THE LETTER." if have "THE GUILDMASTER'S LETTER" -> expelled_end
  opt "MAKE HIM PAY EVERY FAMILY BACK." -> restitution_end
  opt "SAY NOTHING. START AGAIN." -> silent_end

stage shared_end
  end success
  say "THE GUILD OF {HOME} AND THE MERCHANT OF {TOWN} SIGNED A SHARING OVER A JUG OF WINE. {MASTER} WAS NOT INVITED. NOBODY EXPLAINED WHY, AND NOBODY HAD TO."
  journal "THE GUILD OF {HOME} SHARES ITS [[=craft]] WITH {TOWN} NOW. THE GUILDMASTER IS LEFT OUT."
  do reward fair
  do remember giver "HALF THE TRADE IS BETTER THAN NONE. AND {MASTER} SITS ALONE IN HIS NEW HOUSE. <<proverb>>"
  do fact "THE GUILD OF {HOME} AND A MERCHANT OF {TOWN} SHARE THE [[=craft]] NOW, AFTER A GUILDMASTER SOLD IT."

stage expelled_end
  end success
  say "THE LETTER WENT ROUND THE GUILD TABLE HAND TO HAND. {MASTER} LEFT BEFORE IT CAME BACK. HIS NEW HOUSE WAS SOLD TO PAY THE GUILD'S DEBTS BY MIDSUMMER."
  journal "THE GUILD OF {HOME} READ {MASTER}'S LETTER AND CAST HIM OUT. HIS HOUSE PAID THE GUILD'S DEBTS."
  do take "THE GUILDMASTER'S LETTER"
  do reward rich
  do hide master
  do remember giver "WE MEET IN HIS OLD HOUSE NOW. THE GUILD HALL IS BACK. THE CHAIRS ARE TOO NICE. <<relief>>"
  do fact "{MASTER} OF {HOME} SOLD HIS GUILD'S [[=craft]] TO {TOWN} AND WAS CAST OUT WHEN A LETTER CAME HOME."

stage restitution_end
  end success
  say "{MASTER} PAID. EVERY FAMILY, EVERY COIN, OUT OF THE NEW HOUSE, WHICH IS NOW AN OLD HOUSE WITH NO FURNITURE. HE STILL COMES TO GUILD FEASTS. HE SITS AT THE END. NOBODY PASSES HIM THE SALT."
  journal "{MASTER} PAID BACK EVERY FAMILY OF THE GUILD OF {HOME}. HE STAYS, AT THE END OF THE TABLE."
  do reward fair
  do remember master "I PAID. I'D PAY AGAIN TO HAVE THE SALT PASSED. NOBODY DOES. I DON'T BLAME THEM. <<apology:shame>>"

stage silent_end
  end success
  say "THE GUILD MET AND TALKED ABOUT NEW PATTERNS AND NEW DYES AND OLD FRIENDSHIPS. NOBODY SAID {MASTER}'S NAME. HE RESIGNED A MONTH LATER, FOR HIS HEALTH."
  journal "THE GUILD OF {HOME} STARTED AGAIN WITHOUT NAMING THE ONE WHO SOLD IT. {MASTER} RESIGNED."
  do reward fair
  do remember giver "WE'RE MAKING A NEW RED. IT'S BETTER THAN THE OLD ONE. THAT'S THE ONLY REVENGE I WANT. <<vow:pride>>"
)SAGA";

const char* const kStolenTemper = R"SAGA(
title [[THE STOLEN TEMPER|THE SMITH'S LOST BOOK|STEEL AND WAGES]]
hook npc any
pitch "[[pitch=WHY IS THE FORGE COLD?|WHAT DID THE JOURNEYMAN TAKE?|THE SMITH LOOKS LIKE DEATH.]]"
hint "[[~pitch|THE OLD SMITH'S BOOK IS GONE. SIX GENERATIONS OF IT. HOW TO MAKE OUR STEEL, AND NOBODY ELSE'S.|THE OLD SMITH'S BOOK IS GONE. SIX GENERATIONS OF IT. HOW TO MAKE OUR STEEL, AND NOBODY ELSE'S.|A FORGE WITHOUT ITS BOOK IS JUST A HOT ROOM.]]"
role giver giver
role home site home
role forge npc smith at home
role camp site camp near
role thief person male at camp
slot t1 -> camp_go rival deceit world
slot t2 -> back mercy betrayal price

stage start
  talk giver
  say "OUR SMITH'S BOOK IS GONE: SIX GENERATIONS OF HOW THE {GIVER.PEOPLE} [[QUENCH|FOLD|SMELT]] OUR OWN STEEL. THE JOURNEYMAN, {THIEF}, TOOK IT AND RAN TO {CAMP} TO SELL IT TO WHOEVER PAYS. <<urgency:fear>>"
  opt "I'LL GET THE BOOK BACK." -> @t1
  opt "WHY DID {THIEF} RUN?" -> why
  opt "STEEL IS STEEL." -> refused

stage why
  talk giver
  say "HE SAYS HE WAS NEVER PAID. FIVE YEARS AT THE BELLOWS. ...THE SMITH IS A HARD MAN. I WON'T SWEAR HE WAS PAID. <<doubt>>"
  opt "THEN I'LL HEAR {THIEF} OUT." -> @t1

stage refused
  end fail
  journal "YOU TOLD {GIVER} THAT STEEL IS STEEL."
  do remember giver "THE BOOK WENT TO A RIVAL FORGE. THEIR BLADES ARE GOOD NOW. OURS ARE THE SAME. THAT'S THE PROBLEM."

stage camp_go
  goal goto camp
  then thief_talk
  journal "{THIEF}, A JOURNEYMAN OF {HOME}, TOOK THE SMITH'S BOOK OF {GIVER.PEOPLE} STEEL TO {CAMP} TO SELL. GET IT BACK."

stage thief_talk
  talk thief
  say "FIVE YEARS AT HIS BELLOWS. FIVE YEARS, AND NOT A COIN, BECAUSE I WAS LEARNING. I LEARNED. I LEARNED THE BOOK BY HEART. NOW IT'S MY WAGES, AND A BUYER COMES AT DUSK. <<threat:vengeance>>"
  opt "YOUR WAGES. FROM ME. [20 GOLD]" check gold 20 -> wages else short_purse
  opt "COME BACK. MAKE HIM PAY YOU." -> @t2
  opt "THE BOOK. NOW." -> fight
  opt "SELL IT, THEN. YOU EARNED IT." -> sold_end

stage wages
  talk thief
  say "FROM YOU? WHY WOULD YOU... FINE. FINE. HERE. TAKE IT BACK TO HIM. AND TELL HIM I KNOW EVERY PAGE, AND I'LL OPEN MY OWN FORGE SOMEDAY, AND IT WILL BE BETTER. <<vow:pride>>"
  do gold -20
  do give "THE SMITH'S BOOK" book
  opt "TELL HIM YOURSELF, SOMEDAY." -> wages_end

stage short_purse
  talk thief
  say "THAT'S NOT FIVE YEARS. THAT'S NOT FIVE WEEKS. KEEP IT. YOU MEANT WELL, AND MEANING WELL NEVER PAID A JOURNEYMAN YET. <<refusal:pride>>"
  opt "THEN COME BACK. MAKE HIM PAY." -> @t2
  opt "THEN I TAKE THE BOOK." -> fight

stage fight
  goal kill 2 bandit in camp
  then taken
  journal "{THIEF}'S HIRED MEN AT {CAMP} STAND BETWEEN YOU AND THE SMITH'S BOOK."

stage taken
  talk thief
  say "TAKE IT, THEN. IT'S ONLY PAPER. IT'S IN MY HEAD ANYWAY. <<curse>>"
  do give "THE SMITH'S BOOK" book
  opt "IT'S GOING HOME." -> returned_end

stage back
  goal goto home
  then reckoning
  do moves thief home
  journal "{THIEF} WALKS BACK TO {HOME} WITH THE SMITH'S BOOK, TO ASK FOR FIVE YEARS OF WAGES."

stage reckoning
  talk giver
  say "{THIEF} CAME BACK, WITH THE BOOK UNDER HIS ARM, AND STOOD IN THE FORGE AND ASKED FOR FIVE YEARS' WAGES TO THE SMITH'S FACE. THE SMITH WENT VERY RED. AND THEN VERY QUIET. YOU SHOULD SAY SOMETHING. NOW."
  opt "PAY HIM, AND TAKE HIM BACK." -> paid_end
  opt "PAY HIM, AND LET HIM GO." -> parted_end

stage sold_end
  end success
  say "THE BUYER CAME AT DUSK. {THIEF} WALKED AWAY RICH, AND THE BOOK OF {GIVER.PEOPLE} STEEL WENT SOUTH IN A SADDLEBAG. {HOME}'S FORGE COOLED. ITS RIVALS' FORGES DID NOT."
  journal "YOU LET {THIEF} SELL THE SMITH'S BOOK. THE SECRET OF {HOME}'S STEEL IS GONE."
  do reward small
  do mark let_the_book_go
  do shop forge "I'M SELLING NAILS NOW. NAILS. SIX GENERATIONS, AND I SELL NAILS."
  do fact "THE OLD STEEL OF {HOME} WAS SOLD BY AN UNPAID JOURNEYMAN. ANOTHER TOWN MAKES IT NOW."

stage wages_end
  end success
  say "THE SMITH TOOK THE BOOK, AND READ THE FIRST PAGE, AND CRIED, AND THEN SHOWED YOU THE SECOND PAGE BECAUSE NOBODY ELSE HAD EARNED IT."
  journal "YOU PAID {THIEF}'S WAGES YOURSELF AND BROUGHT THE SMITH'S BOOK HOME. THE SMITH SHOWED YOU A PAGE."
  do take "THE SMITH'S BOOK"
  do reward fair
  do secret home
  do shop forge "THE BOOK'S HOME. AND SO ARE YOU, AS FAR AS THIS FORGE IS CONCERNED."
  do fact "THE SMITH OF {HOME} HAS HIS BOOK BACK. HE PAYS HIS JOURNEYMEN NOW. ON TIME."

stage returned_end
  end success
  say "THE SMITH TOOK THE BOOK WITHOUT A WORD AND LOCKED IT AWAY, AND THEN, LATER, SHOWED YOU ONE PAGE, BECAUSE YOU HAD BLED FOR IT. THE BELLOWS ARE WORKED BY A NEW BOY NOW."
  journal "YOU TOOK THE SMITH'S BOOK BACK FROM {THIEF} BY FORCE. THE FORGE OF {HOME} IS HOT AGAIN."
  do take "THE SMITH'S BOOK"
  do reward fair
  do secret home
  do remember giver "THE FORGE IS HOT. THE NEW BOY IS SLOW. I HOPE SOMEONE PAYS HIM. <<doubt>>"

stage paid_end
  end success
  say "THE SMITH PAID, COIN BY COIN, AND THEN HELD OUT HIS HAND. {THIEF} TOOK IT. THEY ARGUE OVER EVERY BLADE NOW, AND THE BLADES ARE BETTER FOR IT, AND THE SMITH SHOWED YOU A PAGE FOR YOUR TROUBLE."
  journal "THE SMITH OF {HOME} PAID {THIEF} FIVE YEARS' WAGES AND TOOK HIM BACK. THE BOOK IS HOME."
  do reward rich
  do secret home
  do shop forge "TWO OF US AT THE ANVIL NOW. HE'S BETTER THAN ME AT THE QUENCH. DON'T TELL HIM."
  do fact "A JOURNEYMAN OF {HOME} STOLE HIS MASTER'S BOOK FOR HIS WAGES, GOT THEM, AND CAME BACK. THEY FORGE TOGETHER."

stage parted_end
  end success
  say "THE SMITH PAID, AND {THIEF} PUT THE BOOK ON THE ANVIL AND WALKED OUT INTO THE STREET A FREE SMITH. THEY DO NOT SPEAK. THEIR BLADES, PEOPLE SAY, ARE ALMOST THE SAME."
  journal "THE SMITH PAID {THIEF}, AND THE BOOK STAYS IN {HOME}. {THIEF} FORGES ON HIS OWN NOW."
  do reward fair
  do secret home
  do remember giver "TWO FORGES IN {HOME} NOW, AND THEY HATE EACH OTHER, AND THE PRICES HAVE NEVER BEEN BETTER. <<proverb>>"
)SAGA";

const char* const kBeastChildren = R"SAGA(
title [[THE BEAST THAT TOOK THE CHILDREN|THE DEN AND THE SMALL TRACKS|WHAT THE BEAST CARRIED]]
hook npc any
pitch "[[pitch=WHY ARE THE CHILDREN KEPT INSIDE?|WHAT TOOK THE LITTLE ONES?|THERE'S A BOUNTY NOTICE. WHO FOR?]]"
hint "[[~pitch|TWO CHILDREN SINCE THE THAW. GONE FROM THE EDGE OF THE FIELDS IN BROAD DAYLIGHT. THE MOTHERS DON'T SLEEP.|TWO CHILDREN SINCE THE THAW. GONE FROM THE EDGE OF THE FIELDS IN BROAD DAYLIGHT. THE MOTHERS DON'T SLEEP.|IT DOESN'T EAT THEM. THAT'S WHAT THE HUNTRESS SAYS. IT CARRIES THEM. LIKE IT'S TAKING THEM HOME.]]"
role giver giver
role home site home
role beast beast near
role kin resident kin of giver
role huntress person female at home
var waited 0
slot t1 -> hunt rival world wonder
slot t2 -> cubs mercy price identity

stage start
  talk giver
  say "{BEAST}, THE WORST OF THE {BEAST.KIND} {BEAST.DIR} OF HERE, HAS TAKEN [[count=TWO|THREE]] CHILDREN FROM THE EDGE OF THE FIELDS SINCE THE THAW. MY {KIN.KIN} {KIN} WENT AFTER IT WITH A SPEAR AND CAME BACK WITHOUT THE SPEAR OR A WORD. <<plea:fear>>"
  opt "I'LL HUNT {BEAST}." -> @t1
  opt "WHAT DOES THE HUNTRESS SAY?" -> huntress_talk
  opt "LET {KIN} TRY AGAIN." -> refused

stage refused
  end fail
  journal "YOU WOULD NOT HUNT {BEAST} FOR {HOME}."
  do remember giver "IT TOOK ANOTHER ONE TODAY. THE MILLER'S. {KIN} HASN'T COME OUT OF THE HOUSE. <<grief:fear>>"

stage huntress_talk
  talk huntress
  say "IT DOESN'T EAT THEM. THE TRACKS GO INTO ITS DEN AND THERE ARE SMALL TRACKS COMING OUT AGAIN, AND GOING BACK IN. THE CHILDREN ARE ALIVE IN THERE. I'D SWEAR IT. WAIT A DAY AND I'LL COME WITH YOU. <<oath>>"
  opt "THEN WE WAIT, AND GO TOGETHER." -> wait_day
  opt "NO TIME. I GO NOW." -> @t1

stage wait_day
  goal wait 1
  then @t1
  do set waited 1
  journal "THE HUNTRESS OF {HOME} BELIEVES {BEAST}'S STOLEN CHILDREN ARE ALIVE IN ITS DEN. SHE WILL HUNT WITH YOU TOMORROW."

stage hunt
  goal slay beast
  then home_again
  say "<<warning>> AND BRING THEM BACK, IF THEY'RE THERE TO BRING."
  journal "{BEAST} HAS TAKEN [[=count]] CHILDREN OF {HOME}. HUNT IT DOWN, {BEAST.DIR}, AND SEE WHAT IS IN ITS DEN."

stage home_again
  goal goto home
  then @t2
  journal "{BEAST} IS DEAD. IN ITS DEN: [[=count]] CHILDREN, FILTHY AND ALIVE, AND ITS YOUNG. BRING THEM ALL HOME TO {HOME}."

stage cubs
  talk huntress
  say "THE CHILDREN ARE HOME, ALL [[=count]], AND THEIR MOTHERS WON'T LET GO OF THEM. AND THESE? THE BEAST'S YOUNG, IN MY SACK. IT WAS RAISING OUR CHILDREN WITH THEM. {HOME} WANTS THEM DROWNED."
  opt "DROWN THEM. NO MORE {BEAST.KIND}." -> drowned_end
  opt "RAISE THEM. AS IT RAISED OURS." -> raised_end
  opt "CARRY THEM FAR AND LET THEM GO." -> freed_end

stage drowned_end
  end success
  say "THE HUNTRESS DID IT AT THE MILLPOND AND CAME BACK WITH WET SLEEVES AND DID NOT SPEAK AT SUPPER. THE CHILDREN OF {HOME} PLAY AT THE EDGE OF THE FIELDS AGAIN. ONE OF THEM STILL HOWLS AT NIGHT."
  journal "YOU KILLED {BEAST} AND BROUGHT {HOME}'S CHILDREN HOME. ITS YOUNG WERE DROWNED."
  do reward rich
  do befriend kin
  do remember giver "THE CHILDREN ARE HOME. THE SMALLEST ONE GROWLS WHEN SHE'S CROSS. WE'RE WORKING ON IT. <<relief:fear>>"
  do fact "{BEAST} TOOK [[=count]] CHILDREN OF {HOME} AND RAISED THEM IN ITS DEN UNTIL A HUNTER CAME. THE CHILDREN LIVED."

stage raised_end
  end success
  say "THE HUNTRESS RAISES THEM IN HER YARD, AND THE CHILDREN WHO WERE TAKEN VISIT THEM EVERY DAY AND SPEAK TO THEM IN A LANGUAGE NOBODY ELSE KNOWS. {HOME} IS NERVOUS. {HOME} IS ALSO VERY WELL GUARDED."
  journal "{BEAST} IS DEAD. ITS YOUNG ARE RAISED IN {HOME} BY THE HUNTRESS, WITH THE CHILDREN IT ONCE TOOK."
  do reward rich
  do befriend kin
  do remember huntress "THEY SIT WHEN I SAY SIT. MOSTLY. THE CHILDREN SAY SIT BETTER THAN I DO. <<boast:pride>>"
  do fact "{HOME} KEEPS THE YOUNG OF {BEAST} IN A HUNTRESS'S YARD. NO WOLF COMES WITHIN A MILE OF THE FIELDS NOW."

stage freed_end
  end success
  say "YOU CARRIED THEM THREE DAYS INTO THE WILD AND OPENED THE SACK. THEY LOOKED BACK AT YOU ONCE, LIKE THEIR MOTHER DID AT THE END, AND WERE GONE INTO THE TREES."
  journal "{BEAST} IS DEAD AND {HOME}'S CHILDREN ARE HOME. YOU SET ITS YOUNG FREE FAR AWAY."
  do reward fair
  do befriend kin
  do mark freed_the_beasts_young
  do remember kin "YOU LET THEM GO. GOOD. I COULDN'T HAVE KILLED THEM EITHER. THAT'S WHY I CAME BACK WITHOUT THE SPEAR."
  do fact "THE YOUNG OF {BEAST} RUN WILD SOMEWHERE FAR FROM {HOME}. ONE DAY ONE OF THEM WILL BE AS GREAT AS ITS MOTHER."
)SAGA";

void add(std::vector<Archetype>& v, const char* id, const char* name, uint32_t themes, uint32_t needs, uint16_t motives,
         uint32_t twists, const char* body) {
  Archetype a;
  a.id = id;
  a.name = name;
  a.source = Source::World;
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

void addWorld(std::vector<Archetype>& v) {
  using M = Motive;
  add(v, "hoarding_steward", "THE STEWARD'S BARNS", TH_HUNGER | TH_GREED | TH_JUDGEMENT | TH_COURAGE | TH_MERCY,
      N_FAMINE | N_TOWN, mb(M::Fear, M::Love, M::Vengeance, M::Duty), TF_WORLD | TF_RIVAL | TF_DECEIT | TF_BETRAYAL | TF_PRICE | TF_MERCY,
      kHoardingSteward);
  add(v, "deserter", "THE DESERTER IN THE LOFT", TH_WAR | TH_MERCY | TH_KINSHIP | TH_JUDGEMENT | TH_LOYALTY,
      N_KINGDOM | N_WAR | N_VILLAGE, mb(M::Fear, M::Love, M::Shame, M::Grief), TF_WORLD | TF_RIVAL | TF_DECEIT | TF_BETRAYAL | TF_MERCY | TF_PRICE,
      kDeserter);
  add(v, "refugees", "THE FAMILY BY THE WELL", TH_HOSPITALITY | TH_MERCY | TH_EXILE | TH_JUDGEMENT | TH_HUNGER, N_TOWN,
      mb(M::Hope, M::Fear, M::Shame, M::Duty), TF_DECEIT | TF_RIVAL | TF_WORLD | TF_MERCY | TF_PRICE, kRefugees);
  add(v, "guild_sold", "THE GUILD'S BETRAYAL", TH_BETRAYAL | TH_GREED | TH_JUDGEMENT | TH_LOYALTY | TH_FRIENDSHIP, N_TOWN,
      mb(M::Greed, M::Pride, M::Vengeance, M::Grief), TF_DECEIT | TF_RIVAL | TF_WORLD | TF_BETRAYAL | TF_PRICE | TF_MERCY, kGuildSold);
  add(v, "stolen_temper", "THE STOLEN TEMPER", TH_GREED | TH_JUDGEMENT | TH_MERCY | TH_PRIDE | TH_BETRAYAL, N_SMITH | N_CAMP,
      mb(M::Pride, M::Fear, M::Duty, M::Greed), TF_RIVAL | TF_DECEIT | TF_WORLD | TF_MERCY | TF_BETRAYAL | TF_PRICE, kStolenTemper);
  add(v, "beast_children", "THE BEAST THAT TOOK THE CHILDREN", TH_COURAGE | TH_MERCY | TH_KINSHIP | TH_WONDER | TH_SACRIFICE,
      N_UNIQUE, mb(M::Fear, M::Love, M::Grief, M::Vengeance), TF_RIVAL | TF_WORLD | TF_WONDER | TF_MERCY | TF_PRICE | TF_IDENTITY,
      kBeastChildren);
}

}  // namespace arch
}  // namespace saga
}  // namespace story
