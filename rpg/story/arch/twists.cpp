// M6b "Sagas": the twists library (rpg/story/saga.h Twist). ARCHETYPES lane. A twist is a template fragment: its own
// `role` / `var` lines, then `stage %x` stages (the first is its entry), leaving through @out to the slot's
// continuation. It may name only the roles its `roles` lists (and its own roles). Archetypes may acknowledge a twist
// in their own lines with ?tw_<id> / !tw_<id>.
//
// Conventions of this file (so any twist reads true in any archetype that takes its family):
//   - a twist's own people are roles named tw<word> (a twist appears at most once in a story, and no archetype names
//     a role tw...): the validator reads placeholders upper case, so a %local role cannot be named in a {placeholder}
//     yet; the fixed names can ({TWSEER}, {TWSEER.HE}). Stage names stay %local.
//   - people of a twist live in {HOME} (it happens before the player sets out or on the way back: someone in {HOME}
//     seeks the player out), or at the twist's own place (`role twford site village near`: a detour the journal
//     names). Neither knows where the archetype's road leads, so neither claims to.
//   - only the DECEIT family's giver_lied speaks of {GIVER} as a person ({GIVER.HE}): archetypes whose giver is a
//     place (board, ruin, event hooks) do not list deceit. Twists of the families identity, mercy and price never name
//     {HOME} or {GIVER}, so a ruin-hooked archetype (whose home is the ruin itself) may take them.
//   - a twist that names {FOE} happens BEFORE the kill (the foe is still out there): an archetype's slots after its
//     slay goal take no identity, mercy or rival family.
//   - a twist never decides what the archetype decides (who lives, who rules, the reward): it adds a scene, a choice
//     with a lasting consequence (remember / mark / fact / rep / befriend), and goes on through @out.
//
//   DECEIT    giver_lied     THE HALF-TOLD TALE        {GIVER} hid that others went before you, and what became of them
//             false_message  THE FALSE MESSENGER       someone "from {GIVER}" calls the errand off, with silver
//             kind_host      THE KIND HOST             a free supper at {TWINN}, and a bitter taste in the stew
//   IDENTITY  cursed_one     THE NAME UNDER THE CURSE  the foe is someone's child, cursed (before the kill)
//             foe_child      THE FOE'S OWN CHILD       the foe's child knows the safe way in, and why
//             heir_in_rags   THE CREST IN THE DUST     a beggar wears the old royal crest of the kingdom
//   RIVAL     rival_claim    THE SECOND SEEKER         (phase A) the same promise made to someone poorer
//             rival_hunter   THE BOUNTY HUNTER         a hunter with hungry kin wants the same kill
//             rival_crown    THE CROWN'S CLAIM         the lord's reeve claims the errand's fruits for the crown
//   PROPHECY  omen_other     THE OMEN MEANT ANOTHER    a seer: the one who ends this is a child not yet born
//             sign_misread   THE TAILED STAR           the doom-star over {HOME} was read backwards
//             false_chosen   THE WRONG CHOSEN ONE      a youth sure the prophecy named them
//   MERCY     wronged_first  THE TANNER'S GRIEVANCE    the foe was robbed by a reeve before {FOE.HE} robbed anyone
//             beast_wronged  THE EMPTY DEN             the beast's young were sold for pelts
//             thief_caught   THE HAND IN YOUR PACK     a hungry child caught stealing from you
//             deserter       THE DESERTER IN THE BYRE  a levy-runner of the kingdom hides in a byre
//   PRICE     ferry_price    THE FERRYMAN'S FARE       ten silver, or a true thing spoken into the river
//             healer_fee     THE HEALER'S PRICE        a cut gone green; twenty silver, a spring of digging, or fever
//             long_way       THE TOLL ROAD             a toll, the long way round, or a fight
//   BETRAYAL  guide_ambush   THE SHORT WAY             a guide leads you into a gully where four men wait
//             informer       THE EAR AT THE WINDOW     someone in {HOME} has been selling word of your errand
//             turned_friend  THE PAID COMPANION        a companion confesses they were paid to see you fail
//   RETURN    back_from_dead THE DROWNED WHO WALKED    one mourned for six winters wants to go home
//             soldier_home   THE LEVY'S LAST MAN       a soldier of the kingdom, eleven years gone
//   WONDER    road_omen      THE THREAD AND THE SEER   (phase A) a seer reads a thread behind the player
//             fox_eyes       THE WOMAN WITH FOX EYES   share your bread with a stranger whose shadow is a fox
//             road_ghost     THE COLD MAN AT THE STONE a ghost who wants its name said once, aloud
//             white_hart     THE WHITE HART            a huntsman draws on a white hart
//   WORLD     raid_home      THE RAID ON {HOME}        bandits on the home road: turn back or press on
//             levy_press     THE PRESS-GANG            the kingdom's sergeant needs one more for the war
//             hungry_road    THE BURNED-OUT FAMILY     a family from a burned village on the road, and your bread
#include <vector>
#include "rpg/story/arch/arch_tables.h"

namespace story {
namespace saga {
namespace arch {

namespace {

// ================================================================ DECEIT
const char* const kGiverLied = R"SAGA(
role twgossip resident any
stage %word
  talk twgossip
  journal "{TWGOSSIP}, A NEIGHBOUR OF {GIVER} IN {HOME}, WANTS A WORD BEFORE YOU GO FURTHER."
  say "YOU'RE ON {GIVER}'S ERRAND? THEN SOMEONE OUGHT TO TELL YOU. {GIVER.HE} ASKED [[asked=TWO|THREE]] OTHERS BEFORE YOU. ONE CAME BACK WITH A BROKEN ARM. ONE DIDN'T COME BACK AT ALL. {GIVER.HE} LEFT THAT OUT, I'D WAGER."
  opt "WHY WOULD {GIVER.HE} HIDE IT?" -> %why
  opt "IT CHANGES NOTHING." -> %steady
stage %why
  talk twgossip
  say "SHAME, MOSTLY. AND FEAR YOU'D SAY NO. {GIVER.HE} IS NOT A BAD SOUL, {PLAYER}. A FRIGHTENED ONE. <<proverb>>"
  opt "THEN I GO ON WITH MY EYES OPEN." -> %steady
  opt "I'LL HAVE WORDS WITH {GIVER.HIM}." -> %words
stage %steady
  talk twgossip
  say "[[GO CAREFULLY, THEN.|THEN WATCH YOUR BACK.]] AND IF YOU LEARN WHAT BECAME OF THE ONE WHO NEVER CAME BACK, TELL ME. [[HE|SHE]] OWED ME A SHILLING AND A GOODBYE, AND I'D TAKE EITHER."
  do remember twgossip "YOU WENT ON KNOWING THE WORST. THAT'S EITHER BRAVE OR DAFT. I HAVEN'T DECIDED WHICH."
  do mark warned_by_neighbour
  opt "I'LL KEEP AN EAR OUT." -> @out
stage %words
  talk twgossip
  say "GOOD. SOMEBODY SHOULD. BUT GO GENTLY. {GIVER.HE} HASN'T SLEPT A NIGHT THROUGH SINCE THE LAST ONE WENT. <<comfort>>"
  do remember giver "SO YOU KNOW ABOUT THE [[=asked]] BEFORE YOU. I SHOULD HAVE SAID. I WAS AFRAID YOU'D WALK AWAY TOO."
  do remember twgossip "YOU SPOKE TO {GIVER} STRAIGHT AND KIND. NOT MANY MANAGE BOTH."
  opt "I'LL BE GENTLE." -> @out
)SAGA";

const char* const kFalseMessage = R"SAGA(
role twmsgr person [[male|female]] at home
stage %stop
  talk twmsgr
  journal "SOMEONE IN {HOME} IS ASKING AFTER YOU BY NAME: {TWMSGR}, OUT OF BREATH."
  say "<<urgency>> YOU'RE THE ONE ON THE ERRAND FOR {GIVER}? IT'S OFF. ALL OF IT. I'M SENT TO CALL YOU BACK, AND WITH FIVE SILVER FOR YOUR TROUBLE. GO HOME, THERE'S A GOOD SOUL."
  opt "TAKE THE SILVER AND GO ON." -> %pocket
  opt "{GIVER} WOULD HAVE WRITTEN." -> %caught
  opt "WHO REALLY SENT YOU?" check level 3 -> %caught else %slipped
stage %pocket
  talk twmsgr
  do gold 5
  say "THERE. GOOD. YOU'LL BE TURNING BACK NOW? ...YOU'RE NOT TURNING BACK. WHY AREN'T YOU TURNING BACK? THAT WAS FOR TURNING BACK!"
  do remember twmsgr "YOU TOOK MY SILVER AND KEPT WALKING. I HAD TO PAY IT BACK OUT OF MY OWN PURSE."
  opt "THANK YOU FOR THE SILVER." -> @out
stage %caught
  talk twmsgr
  say "...ALL RIGHT. ALL RIGHT! NOBODY SENT ME FROM {GIVER}. A [[MAN IN A GREEN HOOD|WOMAN WITH INK ON HER FINGERS|PEDLAR WITH A LAME MULE]] PAID ME TEN TO TURN YOU ROUND. SOMEBODY DOESN'T WANT THIS DONE, {PLAYER}. THAT'S ALL I KNOW."
  do remember twmsgr "I TOLD YOU THE TRUTH IN THE END. COUNT THAT FOR SOMETHING, WON'T YOU?"
  do mark saw_through_messenger
  opt "THEN SOMEBODY IS AFRAID OF ME." -> @out
  opt "GO. DON'T LIE FOR HIRE AGAIN." -> @out
stage %slipped
  talk twmsgr
  say "REALLY SENT ME? YOU CALL ME A LIAR IN THE STREET? <<insult>> KEEP YOUR ERRAND, THEN, AND MUCH GOOD MAY IT DO YOU."
  do remember twmsgr "THE SUSPICIOUS ONE. WAS YOUR ERRAND WORTH ALL THAT SCOWLING?"
  opt "I'LL TAKE MY CHANCES." -> @out
)SAGA";

const char* const kKindHost = R"SAGA(
role twinn site village near
role twhost person [[male|female]] at twinn
stage %door
  talk twhost
  journal "A HOUSE AT {TWINN} KEEPS A LAMP IN THE WINDOW FOR TRAVELLERS. ITS KEEPER, {TWHOST}, WAVED YOU IN."
  say "RAIN'S COMING, TRAVELLER. THERE'S A FIRE AND A POT ON IT, AND I'LL NOT TAKE A COIN FOR EITHER. SIT. EAT. YOU LOOK HALF DEAD ON YOUR FEET."
  opt "THANK YOU. I WILL." -> %drugged
  opt "I'LL EAT WHAT I CARRY." -> %wary
stage %drugged
  goal wait 1
  then %woken
  do gold -15
  journal "THE STEW TASTED OF [[BITTER HERBS|POPPY AND CLOVES]]. YOU WOKE BEFORE DAWN WITH A SPLITTING HEAD AND A LIGHTER PURSE. {TWHOST} IS STILL AT THE TABLE."
stage %woken
  talk twhost
  say "YOU'RE AWAKE? YOU'RE NOT MEANT TO WAKE TILL NOON. ...YOUR PURSE IS ON THE TABLE. I'VE [[mouths=FOUR|FIVE|THREE]] MOUTHS, AND THE LANDLORD'S MAN COMES ON MARKET DAY. <<apology>>"
  opt "GIVE ME BACK WHAT'S MINE." -> %back
  opt "KEEP IT. FEED YOUR [[=mouths]]." -> %keep
stage %back
  talk twhost
  do gold 15
  say "HERE. ALL OF IT. ...YOU COULD HAVE CALLED THE WARDEN. YOU DIDN'T. I'LL REMEMBER THAT, EVEN IF YOU'D RATHER FORGET THE STEW."
  do remember twhost "YOU AGAIN. NO STEW FOR YOU, I THINK. BUT YOU'LL ALWAYS HAVE A DRY CORNER BY MY FIRE."
  opt "COOK HONEST NEXT TIME." -> @out
stage %keep
  talk twhost
  say "YOU... I'D HAVE TAKEN A KNIFE TO YOU IF YOU'D WOKEN SOONER. I WANT YOU TO KNOW I'M ASHAMED OF THAT. <<thanks>>"
  do remember twhost "THE TRAVELLER WHO LEFT ME THEIR PURSE. THE CHILDREN HAD MEAT. I HAVEN'T DRUGGED A SOUL SINCE."
  do fame 1
  opt "BE BETTER THAN YOU WERE." -> @out
stage %wary
  talk twhost
  say "SUIT YOURSELF. <<doubt>> ...THERE'S A LOFT IF YOU WANT IT. I'LL NOT COME UP, AND YOU'LL NOT COME DOWN. FAIR?"
  do remember twhost "THE ONE WHO WOULDN'T TOUCH MY STEW. CLEVER. TOO CLEVER FOR A POOR HOUSE LIKE MINE."
  do mark refused_the_stew
  opt "FAIR." -> @out
)SAGA";

// ================================================================ IDENTITY
const char* const kCursedOne = R"SAGA(
role twmourner person [[male|female]] at home
stage %grief
  talk twmourner
  journal "{TWMOURNER} OF {HOME} HAS HEARD YOU ARE GOING AFTER {FOE}, AND CAME TO FIND YOU, PALE AS ASH."
  say "YOU'RE GOING AFTER {FOE}? THEN HEAR ME. ON A CORD AT ITS THROAT THERE'S A COPPER RING WITH A CRACKED BLUE STONE. I PUT THAT RING ON MY ELDEST'S HAND [[cursed=SEVEN|NINE|FIVE]] YEARS AGO, THE WEEK BEFORE A CURSE WAS SPOKEN AT OUR DOOR."
  opt "YOUR CHILD... IS {FOE}?" -> %know
  opt "WHAT WOULD YOU HAVE ME DO?" -> %ask
stage %know
  talk twmourner
  say "WHAT'S LEFT OF MY CHILD. THAT WAS THE CURSE: THE THING HAS MY CHILD'S EYES AND NONE OF THE REST. I DON'T ASK YOU TO SPARE IT. THERE'S NOTHING LEFT TO SPARE. <<grief>>"
  opt "I'LL BRING BACK THE RING." -> %ring
  opt "I'M SORRY. IT MUST BE DONE." -> %leave
stage %ask
  talk twmourner
  say "END IT. THEN BURY IT UNDER ITS OWN NAME, NOT AS A MONSTER. ITS NAME WAS NEVER {FOE}, WHATEVER THE ROADS CALL IT. IT WAS SOMETHING SOFTER. AND BRING ME THE RING."
  opt "THE RING, AND A TRUE GRAVE." -> %ring
  opt "THE GRAVE IS YOURS TO DIG." -> %leave
stage %ring
  talk twmourner
  say "THANK YOU. I'LL CARVE THE STONE MYSELF. I'VE HAD [[=cursed]] YEARS TO THINK WHAT TO PUT ON IT. <<thanks>>"
  do mark promised_the_cursed_a_grave
  do remember twmourner "YOU BROUGHT ME THE RING. MY CHILD HAS A STONE WITH A TRUE NAME ON IT NOW. I SIT THERE MOST EVENINGS."
  do fact "THEY SAY {FOE} WAS ONCE A CHILD OF {HOME}, CURSED [[=cursed]] YEARS AGO, AND HAS A GRAVE NOW UNDER ITS TRUE NAME."
  opt "UNDER ITS TRUE NAME." -> @out
stage %leave
  talk twmourner
  say "THEN GO. END IT, AT LEAST. THAT'S MORE THAN I MANAGED IN [[=cursed]] YEARS. <<farewell>>"
  do remember twmourner "YOU'RE THE ONE WHO ENDED MY CHILD. NO, I DON'T BLAME YOU. I JUST CAN'T LOOK AT YOU FOR LONG."
  opt "FORGIVE ME." -> @out
)SAGA";

const char* const kFoeChild = R"SAGA(
role twchild person [[male|female]] at home
stage %plea
  talk twchild
  journal "{TWCHILD} OF {HOME} HAS BEEN WAITING FOR YOU, AND KNOWS WHO YOU ARE GOING AFTER."
  say "YOU'RE GOING AFTER {FOE}. I KNOW THE WAY IN: THE PATH WITHOUT THE SNARES, THE STEP THAT CREAKS. I'LL DRAW IT FOR YOU. ...WHY? BECAUSE {FOE.HE} IS MY {FOE.FATHER}, AND I WOULD RATHER IT WAS YOU THAN THE ROPE."
  opt "WHY NOT THE ROPE?" -> %why
  opt "DRAW ME THE PATH." -> %path
  opt "GO HOME. THIS ISN'T FOR YOU." -> %home
stage %why
  talk twchild
  say "BECAUSE THE ROPE IS SLOW AND THE WHOLE TOWN COMES TO WATCH, AND THEY'D BRING THEIR CHILDREN. AND BECAUSE {FOE.HE} TAUGHT ME TO [[SWIM|WHITTLE|READ]], ONCE, BEFORE. NOBODY ELSE REMEMBERS BEFORE. I DO."
  opt "DRAW ME THE PATH, THEN." -> %path
  opt "I'LL FIND MY OWN WAY." -> %home
stage %path
  talk twchild
  say "THERE. MIND THE THIRD STONE; IT ROLLS. ...IF {FOE.HE} SAYS ANYTHING AT THE END, I DON'T WANT TO KNOW WHAT IT WAS. <<blessing:grief>>"
  do mark knew_the_safe_path
  do remember twchild "YOU WENT BY MY PATH. I DIDN'T ASK HOW IT ENDED. DON'T TELL ME. ...WAS IT QUICK? NO. DON'T TELL ME."
  opt "I WON'T TELL YOU." -> @out
stage %home
  talk twchild
  say "...YES. ALL RIGHT. I WAS NEVER HERE. <<refusal>>"
  do remember twchild "YOU SENT ME HOME LIKE A CHILD. I SUPPOSE I AM ONE. I SUPPOSE I'M ALLOWED TO BE, NOW."
  opt "GO ON HOME." -> @out
)SAGA";

const char* const kHeirInRags = R"SAGA(
role twheir person [[male|female]] at home
stage %rags
  talk twheir
  journal "A BEGGAR IN {HOME}, {TWHEIR}, WEARS A RING TOO FINE FOR {TWHEIR.HIS} RAGS."
  say "A COPPER, FRIEND? ...YOU'RE STARING AT THE RING. THEY ALL DO, THEN THEY LAUGH. IT BEARS THE OLD CREST OF {KINGDOM}, FROM BEFORE THE HOUSE OF {KINGDOM.LORD} TOOK THE THRONE. GRANDMOTHER SAID IT MADE US KINGS. IT MADE US BEGGARS."
  opt "THEN WEAR IT PROUDLY." -> %proud
  opt "SELL IT. EAT." -> %sell
  opt "HIDE IT. KINGS KILL CLAIMANTS." -> %hide
stage %proud
  talk twheir
  say "PROUDLY. IN THE GUTTER. ...YOU KNOW, NOBODY'S EVER SAID THAT. THEY SAY STEAL IT, OR SELL IT, OR SHUT UP. <<vow>>"
  do remember twheir "I WEAR IT OUTSIDE MY SLEEVE NOW. THE GUARDS STARE. LET THEM. A STRANGER TOLD ME TO, AND THE STRANGER WAS RIGHT."
  do fact "A BEGGAR ON THE ROADS OF {KINGDOM} WEARS THE OLD ROYAL CREST IN PLAIN SIGHT. SOME SAY THE BLOOD IS TRUE."
  opt "LET THEM STARE." -> @out
stage %sell
  talk twheir
  say "SELL GRANDMOTHER'S RING. ...SHE'D SAY THE SAME, YOU KNOW. SHE ATE THE SILVER SPOONS IN THE BAD WINTER, ONE A WEEK. <<thanks>>"
  do remember twheir "I SOLD IT FOR A WINTER COAT AND A YEAR OF BREAD. CRESTS DON'T FILL BELLIES. YOU WERE RIGHT, BLAST YOU."
  opt "EAT WELL." -> @out
stage %hide
  talk twheir
  say "KILL CLAIMANTS? I'M NOT CLAIMING ANYTHING BUT A COPPER. ...BUT YOU'RE RIGHT. THEY WOULDN'T ASK FIRST. <<warning>>"
  do remember twheir "THE RING'S SEWN INTO MY HEM NOW. NOBODY STARES. I MISS IT, A LITTLE. BEING STARED AT."
  do mark hid_the_crest
  opt "STAY ALIVE." -> @out
)SAGA";

// ================================================================ RIVAL
const char* const kRivalHunter = R"SAGA(
role twhunter person [[male|female]] at home
stage %race
  talk twhunter
  journal "ANOTHER HUNTER IN {HOME}, {TWHUNTER}, HAS HEARD YOU ARE AFTER {FOE}."
  say "<<surprise>> YOU'RE AFTER {FOE} TOO? THERE'S A PRICE ON THAT HEAD IN [[THREE|FOUR]] VILLAGES AND I MEAN TO COLLECT IN ALL OF THEM. MY {TWHUNTER.FATHER}'S HUNGRY AND THE ROOF'S OPEN TO THE SKY. WHAT ARE YOU AFTER IT FOR?"
  opt "TAKE THE BOUNTIES. I'LL DO IT." -> %gift
  opt "TOGETHER, THEN. WE SPLIT IT." -> %pair
  opt "STAY OUT OF MY WAY." check level 4 -> %cowed else %racing
stage %gift
  talk twhunter
  say "YOU'D DO THE BLEEDING AND LET ME DO THE COLLECTING? ...EITHER YOU'RE A SAINT OR YOU'RE A FOOL, AND EITHER WAY I'LL BUY YOU A DRINK. <<oath>>"
  do remember twhunter "THE ONE WHO GAVE ME THE BOUNTY ON {FOE}! THE ROOF'S MENDED. SIT. YOU DON'T PAY HERE."
  do fame 1
  opt "MEND THE ROOF." -> @out
stage %pair
  talk twhunter
  say "SPLIT. FAIR ENOUGH. I'LL WATCH YOUR LEFT; I'M BETTER ON THE LEFT. AND IF IT GOES BADLY, I'M RUNNING. I'M TELLING YOU NOW SO IT'S NOT A BETRAYAL LATER."
  do remember twhunter "HALF A BOUNTY AND A WHOLE STORY. YOU WERE SLOWER THAN ME ON THE LEFT. BUT YOU DIDN'T RUN."
  opt "HONEST. I LIKE THAT." -> @out
stage %cowed
  talk twhunter
  say "...ALL RIGHT. I'VE SEEN THAT LOOK ON MEN WHO CAME BACK. AND ON SOME WHO DIDN'T. I'LL WAIT A WEEK. IF YOU'RE NOT BACK, IT'S MINE."
  do remember twhunter "YOU WARNED ME OFF {FOE} WITH A LOOK. I'M STILL NOT SURE IF THAT WAS KINDNESS."
  opt "A WEEK, THEN." -> @out
stage %racing
  talk twhunter
  say "OUT OF YOUR WAY? <<insult>> THEN WE'LL SEE WHO'S FASTER, AND THE LOSER BUYS THE ALE."
  do remember twhunter "STILL SORE YOU BEAT ME TO {FOE}. THE ALE WAS GOOD, THOUGH. I'LL GIVE YOU THAT."
  do mark raced_a_hunter
  opt "THE LOSER BUYS." -> @out
)SAGA";

const char* const kRivalCrown = R"SAGA(
role twreeve person [[male|female]] at home
stage %claim
  talk twreeve
  journal "A REEVE OF {KINGDOM}, {TWREEVE}, IS WAITING FOR YOU IN {HOME} WITH A SEALED WRIT."
  say "IN THE NAME OF {KINGDOM.LORD}: WHATEVER COMES OF YOUR ERRAND, COIN, GOODS OR THANKS, ONE PART IN THREE IS THE CROWN'S. IT'S WRITTEN. SEE THE SEAL? THE CROWN HAS WARS TO PAY FOR, AND YOU WALK ITS ROADS."
  opt "THE CROWN SHALL HAVE ITS THIRD." -> %yield
  opt "THE CROWN DIDN'T WALK THE ROAD." -> %defy
  opt "A TENTH, AND I'LL SING ITS PRAISES." check level 3 -> %bargain else %defy
stage %yield
  talk twreeve
  say "A LOYAL SUBJECT. RARER THAN YOU'D THINK, ON THESE ROADS. I'LL WRITE YOUR NAME IN THE LEDGER, ON THE GOOD SIDE. <<blessing>>"
  do rep kingdom 3
  do gold -10
  do remember twreeve "YOUR NAME IS ON THE GOOD SIDE OF MY LEDGER. NOT MANY ARE. FEWER STAY THERE."
  opt "WRITE IT NEATLY." -> @out
stage %defy
  talk twreeve
  say "THEN I'LL WRITE YOUR NAME ON THE OTHER PAGE. THE CROWN HAS A LONG MEMORY AND A SHORT TEMPER. <<threat>>"
  do rep kingdom -3
  do remember twreeve "YOU. THE ONE WHO TOLD THE CROWN TO WALK ITS OWN ROADS. IT'S IN THE LEDGER. IN RED."
  do mark defied_the_reeve
  opt "WRITE IT IN RED." -> @out
stage %bargain
  talk twreeve
  say "...A TENTH. AND THE SINGING. THE CROWN IS NOT PROUD, ONLY POOR. DONE, AND DON'T TELL THE NEXT REEVE I SAID THAT."
  do rep kingdom 1
  do gold -3
  do remember twreeve "A TENTH AND A SONG. THE SONG WAS TERRIBLE. THE TENTH WAS FAIR. GO WELL."
  opt "LONG LIVE THE CROWN." -> @out
)SAGA";

// ================================================================ PROPHECY
const char* const kOmenOther = R"SAGA(
role twseer person female at home
stage %dream
  talk twseer
  journal "{TWSEER}, THE OLD DREAMER OF {HOME}, HAS SENT FOR YOU."
  say "I DREAMED OF YOU [[nights=THREE|FOUR|SEVEN]] NIGHTS RUNNING, {PLAYER}, AND I READ THE DREAM WRONG. THE ONE WHO ENDS THIS IS NOT YOU. IT IS A CHILD OF {HOME} NOT YET BORN. YOU ARE ONLY THE ONE WHO KEEPS THE ROAD OPEN FOR THEM."
  opt "THEN I'LL KEEP IT OPEN." -> %humble
  opt "I MAKE MY OWN FATE." -> %proud
  opt "WHOSE CHILD?" -> %whose
stage %whose
  talk twseer
  say "IF I TOLD YOU, YOU'D WATCH THEM ALL THEIR LIFE, AND WATCHED CHILDREN GROW UP CROOKED. <<proverb>>"
  opt "THEN I'LL KEEP THE ROAD OPEN." -> %humble
  opt "DREAMS ARE ONLY DREAMS." -> %proud
stage %humble
  talk twseer
  say "MOST WHO HEAR THAT GO HOME SULKING. YOU DIDN'T. MAYBE I SHOULD HAVE DREAMED ABOUT YOU A FIFTH NIGHT. <<blessing>>"
  do remember twseer "THE ROAD-KEEPER! I STILL DREAM OF YOU SOME NIGHTS. YOU'RE ALWAYS WALKING, AND IT'S ALWAYS DAWN."
  do mark keeps_the_road
  opt "DREAM KINDLY." -> @out
stage %proud
  talk twseer
  say "MAYBE YOU DO. MAYBE THE DREAM IS WRONG TWICE. I'VE BEEN WRONG TWICE BEFORE: ONCE ABOUT A HORSE, ONCE ABOUT A HUSBAND. <<doubt>>"
  do remember twseer "STILL MAKING YOUR OWN FATE? I DREAMED OF YOU AGAIN. YOU WERE ARGUING WITH THE MOON."
  opt "I'LL PROVE IT WRONG." -> @out
)SAGA";

const char* const kSignMisread = R"SAGA(
role twscribe person [[male|female]] at home
stage %rolls
  talk twscribe
  journal "{TWSCRIBE}, WHO KEEPS THE OLD ROLLS OF {HOME}, HAS FOUND SOMETHING ABOUT THE STAR."
  say "YOU'VE SEEN THE TAILED STAR OVER {HOME}? THEY'RE CALLING IT DOOM ON YOUR ERRAND. I'VE READ THE OLD ROLLS. IT CAME THE YEAR THE TOWN WAS FOUNDED AND THE YEAR THE WELL WAS DUG. IT MEANS BEGINNINGS, NOT ENDINGS."
  opt "TELL THE TOWN, THEN." -> %tell
  opt "LET THEM FEAR. FEAR KEEPS THEM IN." -> %quiet
  opt "SIGNS MEAN WHAT WE MAKE THEM." -> %make
stage %tell
  talk twscribe
  say "THEY WON'T LISTEN TO ME. I'M THE ODD ONE WITH THE ROLLS. BUT THEY MIGHT LISTEN TO YOU. COME WITH ME TO THE WELL AT NOON. <<vow>>"
  do fact "THE TAILED STAR OVER {HOME} WAS READ AGAIN FROM THE OLD ROLLS: A SIGN OF BEGINNINGS, NOT OF DOOM."
  do remember twscribe "THEY LISTENED! WELL. HALF OF THEM. THE OTHER HALF STILL BAR THEIR DOORS AT DUSK. IT'S A START."
  opt "AT NOON, THEN." -> @out
stage %quiet
  talk twscribe
  say "THAT'S A COLD THING TO SAY. TRUE, MAYBE. COLD. <<doubt>>"
  do remember twscribe "THEY STILL THINK THE STAR WAS DOOM. YOU COULD HAVE HELPED ME TELL THEM. YOU CHOSE NOT TO."
  do mark kept_the_town_afraid
  opt "COLD THINGS KEEP." -> @out
stage %make
  talk twscribe
  say "SPOKEN LIKE SOMEONE WHO HAS NEVER HAD TO COPY OUT A HUNDRED YEARS OF SIGNS BY CANDLE. ...BUT YES. MAYBE. <<proverb>>"
  do remember twscribe "SIGNS MEAN WHAT WE MAKE THEM. I WROTE THAT IN THE MARGIN OF THE ROLL, YOU KNOW. UNDER YOUR NAME."
  opt "WRITE IT LARGE." -> @out
)SAGA";

const char* const kFalseChosen = R"SAGA(
role twchosen person [[male|female]] at home
stage %sure
  talk twchosen
  journal "{TWCHOSEN}, A YOUTH OF {HOME}, HAS PACKED A BAG AND WANTS TO COME WITH YOU."
  say "THE WISE WOMAN SAID ONE WOULD COME FROM THE ROAD AND ONE WOULD GO BACK DOWN IT, AND THE ONE WHO GOES IS CHOSEN. THAT'S ME! I'VE PACKED. I'VE GOT [[A KNIFE AND A LOAF|MY FATHER'S BOW|A SLING AND NINE STONES]]."
  opt "IT WAS NEVER ABOUT YOU." -> %truth
  opt "THEN COME. STAY BEHIND ME." -> %come
  opt "YOU'RE CHOSEN TO GUARD {HOME}." -> %lie
stage %truth
  talk twchosen
  say "...NEVER ABOUT ME. NO. I SUPPOSE IT WOULDN'T BE. IT NEVER IS. <<grief:shame>>"
  do remember twchosen "YOU TOLD ME TRUE AND IT STUNG. I'VE STOPPED WAITING TO BE CHOSEN. I'M APPRENTICED TO THE COOPER NOW. I'M GOOD AT IT."
  opt "YOU'LL FIND YOUR OWN ROAD." -> @out
stage %come
  talk twchosen
  say "I KNEW IT! I KNEW IT. I'LL STAY BEHIND YOU. FAR BEHIND. WELL BEHIND. <<vow>>"
  do remember twchosen "I WENT WITH YOU AND SAW IT ALL. NOBODY BELIEVES ME. I DON'T CARE. I WAS THERE."
  do mark took_a_youth_along
  opt "WELL BEHIND." -> @out
stage %lie
  talk twchosen
  say "GUARD {HOME}? ...THAT'S EVEN BETTER. THAT'S MORE IMPORTANT, ISN'T IT? NOBODY GETS PAST ME. <<oath>>"
  do remember twchosen "STILL GUARDING {HOME}! NOBODY'S GOT PAST ME. WELL, A GOAT. ONCE. IT DOESN'T COUNT."
  do mark a_kind_lie
  opt "NOBODY GETS PAST YOU." -> @out
)SAGA";

// ================================================================ MERCY
const char* const kWrongedFirst = R"SAGA(
role tweld person male at home
stage %hear
  talk tweld
  journal "AN OLD MAN, {TWELD}, WANTS YOU TO HEAR WHO {FOE} WAS BEFORE."
  say "BEFORE YOU GO AFTER {FOE}, HEAR AN OLD MAN OUT. {FOE.HE} WAS A TANNER IN THIS VALLEY. A REEVE TOOK THE LAND FOR A DEBT NEVER OWED, AND {FOE.HIS} [[SON|DAUGHTER]] DIED IN THE WINTER AFTER. NOBODY HANGED THE REEVE."
  opt "THAT DOESN'T EXCUSE {FOE.HIM}." -> %hard
  opt "IS THE REEVE STILL ALIVE?" -> %reeve
  opt "I'LL CARRY THE TRUTH OF IT." -> %carry
stage %reeve
  talk tweld
  say "DEAD THESE TWO YEARS, OLD AND FAT, IN HIS OWN BED, WITH A PRIEST HOLDING HIS HAND. THE WORLD IS NOT ALWAYS FAIR, {PLAYER}. YOU'LL HAVE NOTICED. <<proverb>>"
  opt "THEN I'LL CARRY THE TRUTH." -> %carry
  opt "IT STILL DOESN'T EXCUSE {FOE.HIM}." -> %hard
stage %hard
  talk tweld
  say "NO. IT DOESN'T. I ONLY WANTED SOMEONE TO KNOW, BEFORE THE END, THAT {FOE.HE} HAD A BEGINNING. <<farewell>>"
  do remember tweld "YOU LISTENED, AT LEAST. MOST DON'T EVEN DO THAT FOR AN OLD MAN."
  opt "I KNOW IT NOW." -> @out
stage %carry
  talk tweld
  say "THEN TELL IT RIGHT: NOT THAT {FOE.HE} WAS GOOD, BUT THAT {FOE.HE} WAS WRONGED FIRST. THEY'RE NOT THE SAME THING. <<thanks>>"
  do fact "THEY SAY {FOE} WAS A TANNER WHOSE LAND A CROOKED REEVE STOLE, LONG BEFORE {FOE.HE} TOOK ANYTHING FROM ANYONE."
  do remember tweld "YOU TOLD IT RIGHT. I HEARD THEM SAY IT AT THE WELL: WRONGED FIRST. IT'S SOMETHING."
  opt "NOT THE SAME THING." -> @out
)SAGA";

const char* const kBeastWronged = R"SAGA(
role twtrapper person [[male|female]] at home
stage %den
  talk twtrapper
  journal "{TWTRAPPER}, A TRAPPER OF {HOME}, HAS SOMETHING TO CONFESS ABOUT {FOE}."
  say "YOU'RE GOING AFTER {FOE}. I'LL TELL YOU WHAT NOBODY ELSE WILL. LAST SPRING WE DUG OUT ITS DEN AND SOLD THE YOUNG AT THE FAIR. I SPENT MY SHARE ON BOOTS. IT HAS BEEN KILLING EVER SINCE. IT IS NOT MAD. IT IS GRIEVING."
  opt "IT STILL KILLS." -> %kills
  opt "WHERE ARE THE YOUNG NOW?" -> %young
  opt "THEN THE BLOOD IS ON YOU." -> %blame
stage %young
  talk twtrapper
  say "TWO IN A LORD'S MENAGERIE, BEHIND BARS. ONE DIED IN THE CART. ...YOU'D LIKE ME TO SAY I'M SORRY. I AM. IT DOESN'T MEND ANYTHING. <<apology>>"
  opt "IT STILL KILLS." -> %kills
  opt "THEN THE BLOOD IS ON YOU." -> %blame
stage %kills
  talk twtrapper
  say "IT DOES. SO END IT, AND END IT CLEAN. NOT FOR ME. FOR IT. <<plea:shame>>"
  do remember twtrapper "YOU ENDED IT CLEAN, THEY SAY. I'VE THROWN THE BOOTS IN THE RIVER. DON'T LAUGH."
  opt "CLEAN. I PROMISE." -> @out
stage %blame
  talk twtrapper
  say "ON ME AND [[four=FOUR|FIVE|THREE]] OTHERS. I'M THE ONLY ONE WHO'LL SAY SO. <<grief:shame>>"
  do fact "THE KILLINGS BY {FOE} BEGAN THE SPRING [[=four]] TRAPPERS OF {HOME} DUG OUT ITS DEN AND SOLD ITS YOUNG."
  do remember twtrapper "EVERYONE KNOWS NOW, ABOUT THE DEN. THEY SPIT WHEN I PASS. FAIR. I'D SPIT TOO."
  do mark named_the_trappers
  opt "SAY IT LOUDER." -> @out
)SAGA";

const char* const kThiefCaught = R"SAGA(
role twthief person [[male|female]] at home
stage %caught
  talk twthief
  journal "YOU CAUGHT A THIN HAND IN YOUR PACK. IT BELONGS TO {TWTHIEF}, WHO IS PERHAPS TWELVE."
  say "LET GO! LET GO, I'LL... ALL RIGHT. ALL RIGHT. I WASN'T TAKING MUCH. BREAD AND THE SMALL KNIFE. MY [[BROTHER|SISTER|MOTHER]] HASN'T EATEN SINCE THE FEAST OF LAMPS. DON'T CALL THE WARDEN. THEY TAKE A HAND FOR THE SECOND TIME."
  opt "KEEP THE BREAD. GO." -> %free
  opt "THIS IS FOR THE WARDEN." -> %warden
  opt "WORK FOR IT INSTEAD." -> %work
stage %free
  talk twthief
  say "...JUST GO? YOU'RE NOT EVEN GOING TO SHOUT? <<thanks>>"
  do remember twthief "YOU LET ME GO WITH THE BREAD. I DON'T STEAL NOW. MOSTLY. NOT FROM YOU, EVER."
  do fame 1
  opt "GO, BEFORE I CHANGE MY MIND." -> @out
stage %warden
  talk twthief
  say "THE SECOND TIME. IT'S THE SECOND TIME. THEY'LL TAKE THE HAND. <<curse>>"
  do remember twthief "ONE HAND. I'M QUICK WITH THE OTHER. YOU SHOULD KNOW THAT, IF YOU EVER SLEEP NEAR ME."
  do mark gave_thief_to_warden
  opt "THE LAW IS THE LAW." -> @out
stage %work
  talk twthief
  say "WORK? FOR WHO? NOBODY TAKES ME ON. ...FOR YOU? CARRYING? I CAN CARRY. I'M STRONGER THAN I LOOK. <<vow>>"
  opt "THEN CARRY." -> %carry
stage %carry
  goal wait 1
  then @out
  journal "{TWTHIEF} CARRIED YOUR PACK A WHOLE DAY WITHOUT A WORD OF COMPLAINT AND ATE LIKE A WOLF AT NIGHT."
  do gold -3
  do remember twthief "I CARRIED YOUR PACK A WHOLE DAY AND YOU PAID ME LIKE A GROWN PERSON. I'M A CARRIER NOW. FOR THE CARAVANS."
)SAGA";

const char* const kDeserter = R"SAGA(
role twrunner person male at home
stage %byre
  talk twrunner
  journal "SOMEONE IS HIDING IN A BYRE AT THE EDGE OF {HOME}: {TWRUNNER}, IN THE COLOURS OF {KINGDOM}'S LEVY."
  say "DON'T SHOUT. PLEASE. I RAN FROM {KINGDOM.LORD}'S LEVY AFTER THE THIRD MARCH. THEY HANG RUNNERS AT THE CROSSROADS AND LEAVE THEM FOR THE CROWS. I HAVE A WIFE IN THE HILLS I HAVEN'T SEEN IN [[TWO|THREE]] YEARS."
  opt "I WON'T SHOUT. GO HOME." -> %home
  opt "YOUR OATH WAS TO THE CROWN." -> %oath
  opt "HERE. FOR THE ROAD." check gold 10 -> %coin else %home
stage %home
  talk twrunner
  say "<<thanks>> IF ANYONE ASKS, YOU SAW A MAN IN A BYRE AND HE WAS A SHEPHERD. I WAS A SHEPHERD ONCE. IT'S NEARLY TRUE."
  do remember twrunner "THE SHEPHERD FROM THE BYRE. I'M HOME. THE CHILD DIDN'T KNOW ME. SHE DOES NOW."
  do mark spared_a_deserter
  opt "A SHEPHERD. YES." -> @out
stage %oath
  talk twrunner
  say "MY OATH. I SWORE IT AT FIFTEEN WITH A SERGEANT'S HAND ON MY NECK. ...DO WHAT YOU MUST. I'M TOO TIRED TO RUN AGAIN."
  opt "THEN I MUST." -> %taken
  opt "GO. BEFORE I THINK BETTER." -> %home
stage %taken
  talk twrunner
  say "...AYE. AT LEAST IT'S A FACE AND NOT A DOG THAT FOUND ME. <<grief>>"
  do rep kingdom 2
  do fact "A DESERTER FROM {KINGDOM.LORD}'S LEVY WAS TAKEN IN A BYRE NEAR {HOME}, AND THE CROSSROADS HAD ANOTHER CROW-FEAST."
  do mark gave_up_a_deserter
  opt "I'M SORRY." -> @out
stage %coin
  talk twrunner
  do gold -10
  say "SILVER? FOR A RUNNER? ...YOU'RE A STRANGE ONE. I'LL BUY A BOAT-PASSAGE AND NEVER SEE A SERGEANT AGAIN. <<blessing>>"
  do remember twrunner "THE SILVER FROM THE BYRE BOUGHT A BOAT AND A NEW NAME. I FISH NOW. I'M A BAD FISHER. I'M ALIVE."
  do mark spared_a_deserter
  opt "FISH WELL." -> @out
)SAGA";

// ================================================================ PRICE
const char* const kFerryPrice = R"SAGA(
role twford site village near
role twferry person [[male|female]] at twford
stage %bank
  talk twferry
  journal "THE RIVER IS UP AT {TWFORD}. {TWFERRY} HAS THE ONLY BOAT, AND NAMES A STRANGE FARE."
  say "RIVER'S UP AND MINE'S THE ONLY BOAT. THE FARE IS TEN SILVER, OR A TRUE THING YOU'VE NEVER TOLD ANYONE, SPOKEN INTO THE WATER. THE RIVER IS FUSSY ABOUT ITS FARES. I DON'T ASK WHY. <<proverb>>"
  opt "TEN SILVER." check gold 10 -> %paid else %poor
  opt "A TRUE THING, THEN." -> %secret
  opt "I'LL FORD IT MYSELF." -> %ford
stage %paid
  talk twferry
  do gold -10
  say "SILVER'S EASY. SILVER'S FORGOTTEN BY THE OTHER BANK. SIT IN THE MIDDLE AND DON'T TRAIL YOUR HAND."
  do remember twferry "YOU PAID IN SILVER. MOST DO. THE RIVER DOESN'T REMEMBER YOU. LUCKY."
  opt "THE MIDDLE. YES." -> @out
stage %poor
  talk twferry
  say "NO SILVER? THEN IT'S THE TRUE THING, OR THE COLD WATER. THE WATER IS VERY COLD. <<warning>>"
  opt "A TRUE THING, THEN." -> %secret
  opt "THE COLD WATER." -> %ford
stage %secret
  talk twferry
  say "...THE RIVER HEARD. DID YOU FEEL IT GO QUIET? DON'T TELL ME WHAT YOU SAID. I CARRY ENOUGH ACROSS THIS WATER. <<omen>>"
  do mark spoke_to_the_river
  do remember twferry "THE RIVER STILL GOES QUIET WHEN YOU CROSS. I'VE NEVER SEEN IT DO THAT FOR ANYONE ELSE."
  opt "IT WENT QUIET." -> @out
stage %ford
  goal wait 1
  then @out
  journal "YOU FORDED THE RIVER AT {TWFORD} CHEST-DEEP AND LOST A DAY DRYING BY A FIRE THAT WOULD NOT CATCH."
  do remember twferry "THE ONE WHO SWAM IT RATHER THAN PAY. THE RIVER WAS ANGRY A WEEK. I HOPE IT WAS WORTH IT."
)SAGA";

const char* const kHealerFee = R"SAGA(
role twhealer person [[male|female]] at home
stage %wound
  talk twhealer
  journal "THE SCRATCH ON YOUR ARM HAS GONE HOT AND GREEN AT THE EDGES. {TWHEALER} OF {HOME} KNOWS THE CURE."
  say "THAT SCRATCH IS GOING GREEN, AND GREEN GOES TO BLACK BY THE WEEK'S END. I CAN DRAW IT. TWENTY SILVER, OR SWEAR TO COME BACK AND DIG MY BEDS IN SPRING. OR LEAVE IT AND PRAY. <<warning>>"
  opt "TWENTY SILVER." check gold 20 -> %paid else %swear
  opt "I SWEAR IT. COME SPRING." -> %swear
  opt "IT WILL HEAL ON ITS OWN." -> %fever
stage %paid
  talk twhealer
  do gold -20
  say "HOLD STILL. THIS IS THE PART WHERE GROWN SOLDIERS CALL FOR THEIR MOTHERS. ...THERE. YOU DIDN'T. I'M ALMOST DISAPPOINTED."
  do remember twhealer "HOW'S THE ARM? LET ME SEE. ...GOOD. CLEAN AS A NEW KNIFE."
  opt "IT'S BETTER ALREADY." -> @out
stage %swear
  talk twhealer
  say "SWORN, THEN. I'LL HOLD YOU TO IT. THE BEDS ARE CLAY AND THE CLAY IS STUBBORN. ...HOLD STILL."
  do mark owes_the_healer_a_spring
  do remember twhealer "YOU OWE ME A SPRING OF DIGGING. I HAVEN'T FORGOTTEN. THE CLAY HASN'T EITHER."
  opt "A SPRING OF DIGGING." -> @out
stage %fever
  goal wait 2
  then @out
  journal "THE FEVER CAME ON THE SECOND NIGHT. YOU LOST TWO DAYS TO SWEAT AND DREAMS OF {HOME}, AND WOKE WEAKER, BUT WHOLE."
  do remember twhealer "STILL ALIVE? STUBBORN AND LUCKY. THE TWO DON'T OFTEN KEEP EACH OTHER COMPANY FOR LONG."
)SAGA";

const char* const kLongWay = R"SAGA(
role twtoll site camp near
role twtoller person [[male|female]] at twtoll
stage %toll
  talk twtoller
  journal "MEN FROM {TWTOLL} HAVE CHAINED THE ROAD AND WANT A TOLL. {TWTOLLER} SPEAKS FOR THEM."
  say "THE ROAD'S CLOSED. UNLESS IT ISN'T. FIFTEEN SILVER AND IT ISN'T. OR GO ROUND BY THE MARSH: TWO DAYS, AND THE MIDGES WILL HAVE YOUR EYES. OR TRY US. WE'D ENJOY THAT. <<threat>>"
  opt "FIFTEEN SILVER." check gold 15 -> %paid else %round
  opt "THE LONG WAY ROUND." -> %round
  opt "I'LL TRY YOU." -> %fight
stage %paid
  talk twtoller
  do gold -15
  say "PLEASURE DOING BUSINESS. TELL YOUR FRIENDS. ACTUALLY, DON'T. WE'LL RAISE THE PRICE."
  do remember twtoller "THE ONE WHO PAID THE TOLL WITHOUT A FUSS. WE DRANK YOUR HEALTH. IT WAS CHEAP ALE."
  opt "ENJOY IT WHILE YOU CAN." -> @out
stage %round
  goal wait 2
  then @out
  journal "TWO DAYS THROUGH THE MARSH ROUND {TWTOLL}. THE MIDGES DID NOT HAVE YOUR EYES. THEY HAD MOST OF THE REST."
  do mark took_the_marsh_road
stage %fight
  goal kill 4 bandit
  then %cleared
  journal "THE TOLL-MEN OF {TWTOLL} CHOSE TO BE TRIED. FOUR OF THEM HOLD THE CHAIN."
stage %cleared
  talk twtoller
  say "ENOUGH! ENOUGH. THE ROAD'S OPEN. IT WAS ALWAYS OPEN. WE WERE ONLY... ADVISING. <<apology>>"
  do fact "THE CHAIN ACROSS THE ROAD BY {TWTOLL} IS DOWN. A TRAVELLER TRIED THE TOLL-MEN, AND THEY CAME UP SHORT."
  do remember twtoller "DON'T HIT ME. I'M AN HONEST {TWTOLLER.MAN} NOW. I KEEP GOATS. THE GOATS ARE HONEST TOO."
  opt "STAY HONEST." -> @out
)SAGA";

// ================================================================ BETRAYAL
const char* const kGuideAmbush = R"SAGA(
role twguide person [[male|female]] at home
stage %offer
  talk twguide
  journal "{TWGUIDE} OF {HOME} OFFERS TO SHOW YOU A SHORTER WAY, OVER THE RIDGE."
  say "GOING THE LONG WAY? THERE'S A SHORTER ONE OVER THE RIDGE, BY THE DRY GULLY. HALF A DAY SAVED. I'LL SHOW YOU FOR A COPPER. I KNOW EVERY STONE ON IT. <<greet>>"
  opt "LEAD ON." -> %gully
  opt "I'LL KEEP TO THE ROAD." -> %declined
stage %gully
  goal kill 4 bandit
  then %after
  journal "THE GULLY WAS DRY AND NARROW, AND FOUR MEN WERE WAITING IN IT. {TWGUIDE} STEPPED BACK BEHIND A ROCK."
stage %after
  talk twguide
  say "THEY HAVE MY [[BROTHER|SISTER]]. THEY SAID THEY'D LET [[HIM|HER]] GO FOR ONE TRAVELLER WITH A FULL PACK. I PICKED YOU BECAUSE YOU LOOKED LIKE YOU COULD FIGHT. ...THAT'S NOT AN EXCUSE. I KNOW."
  opt "GO AND FIND YOUR KIN." -> %forgive
  opt "THE WARDEN WILL HEAR OF THIS." -> %warden
stage %forgive
  talk twguide
  say "YOU'D... I'LL FIND THEM. AND IF I EVER LEAD ANYONE ANYWHERE AGAIN, IT'LL BE THE LONG WAY. <<vow>>"
  do remember twguide "I FOUND THEM. ALIVE. I DON'T GUIDE NOW. I CAN'T LOOK AT A GULLY WITHOUT SWEATING."
  opt "THE LONG WAY." -> @out
stage %warden
  talk twguide
  say "...YES. I'D DO THE SAME. TELL IT TRUE, AT LEAST: THAT THEY HAD MY KIN. <<grief>>"
  do fact "{TWGUIDE} OF {HOME} LED A TRAVELLER INTO AN AMBUSH FOR A HOSTAGE'S SAKE, AND ANSWERED FOR IT."
  do mark gave_up_the_guide
  opt "I'LL TELL IT TRUE." -> @out
stage %declined
  talk twguide
  say "SUIT YOURSELF. THE ROAD'S SAFE ENOUGH, I SUPPOSE. FOR SOME. <<doubt>>"
  do remember twguide "YOU DIDN'T TAKE THE SHORT WAY. CLEVER. I WISH YOU HAD. NO. I DON'T. I DON'T KNOW WHAT I WISH."
  opt "SAFE ENOUGH." -> @out
)SAGA";

const char* const kInformer = R"SAGA(
role twear resident any at home
stage %window
  talk twear
  journal "SOMEONE IN {HOME} HAS BEEN SELLING WORD OF YOUR ERRAND. YOU CAUGHT {TWEAR} AT IT."
  say "ALL RIGHT, YES. I LISTENED AT THE WINDOW AND SOLD WHAT I HEARD, WHERE YOU WERE GOING AND WHEN. [[SIX|EIGHT]] SILVER. MY {TWEAR.SON}'S DEBTS ARE OWED TO PEOPLE WHO BREAK FINGERS. <<apology>>"
  opt "EVERYONE WILL KNOW WHAT YOU DID." -> %expose
  opt "HOW MUCH IS THE DEBT?" check gold 15 -> %pay else %warn
  opt "NEVER AGAIN. GO." -> %warn
stage %expose
  talk twear
  say "THEY'LL KNOW. THEN THEY'LL NEVER TALK NEAR ME AGAIN, AND I'LL HAVE NOTHING TO SELL, AND THEN THE FINGERS. <<grief:fear>>"
  do fact "{TWEAR} OF {HOME} SOLD A TRAVELLER'S ROAD TO STRANGERS FOR SILVER, AND THE WHOLE STREET KNOWS IT NOW."
  do remember twear "YOU TOLD THEM ALL. NOBODY SPEAKS NEAR ME NOW. I SUPPOSE THAT'S FAIR. IT'S ALSO VERY QUIET."
  opt "YOU CHOSE THIS." -> @out
stage %pay
  talk twear
  do gold -15
  say "YOU'D PAY IT? THE PERSON I SOLD? ...I DON'T UNDERSTAND YOU. I DON'T THINK I WANT TO. THANK YOU. <<thanks>>"
  do befriend twear
  do remember twear "THE DEBT'S PAID. THE BOY'S HOME. I LISTEN AT NO WINDOWS NOW, ON MY LIFE."
  opt "NO MORE WINDOWS." -> @out
stage %warn
  talk twear
  say "NEVER AGAIN. NEVER. <<oath>>"
  do remember twear "I HAVEN'T SOLD A WORD SINCE YOU CAUGHT ME. I'M POORER. I SLEEP BETTER. MOSTLY."
  do mark caught_the_informer
  opt "I'LL BE LISTENING." -> @out
)SAGA";

const char* const kTurnedFriend = R"SAGA(
role twfriend person [[male|female]] at home
stage %confess
  talk twfriend
  journal "{TWFRIEND}, WHO HAS BEEN SO HELPFUL SINCE YOU CAME TO {HOME}, HAS SOMETHING TO TELL YOU."
  say "YOU'VE BEEN KIND TO ME, SO HERE IT IS. I WAS PAID TO KEEP NEAR YOU AND SEE YOUR ERRAND FAIL. A WRONG TURN, A LAME HORSE, A WORD IN A WRONG EAR. I'VE DONE NONE OF IT. I'M GIVING THE MONEY BACK."
  opt "WHO PAID YOU?" -> %who
  opt "THEN WALK WITH ME FOR REAL." -> %walk
  opt "GET AWAY FROM ME." -> %away
stage %who
  talk twfriend
  say "I NEVER SAW A FACE. A PURSE LEFT UNDER A STONE AND A NOTE IN A GOOD HAND. SOMEONE WHO CAN WRITE AND CAN PAY. THAT'S NOT MANY PEOPLE. <<warning>>"
  opt "THEN WALK WITH ME FOR REAL." -> %walk
  opt "GET AWAY FROM ME." -> %away
stage %walk
  talk twfriend
  say "FOR REAL. YES. I'D LIKE THAT. NOBODY'S EVER HIRED ME TO BE A FRIEND BEFORE. ...THAT CAME OUT WRONG. <<vow>>"
  do remember twfriend "I PUT THE PURSE BACK UNDER THE STONE. NOBODY CAME FOR IT. IT'S STILL THERE. LET IT ROT."
  do mark a_true_companion
  opt "IT CAME OUT FINE." -> @out
stage %away
  talk twfriend
  say "...FAIR. I'D DO THE SAME. BUT I TOLD YOU. REMEMBER THAT I TOLD YOU. <<grief:shame>>"
  do remember twfriend "YOU SENT ME AWAY. I DESERVED IT. I'D STILL TAKE A KNIFE FOR YOU, IF IT CAME TO IT."
  opt "I'LL REMEMBER." -> @out
)SAGA";

// ================================================================ RETURN
const char* const kBackFromDead = R"SAGA(
role twdrowned person [[male|female]] at home
stage %hood
  talk twdrowned
  journal "A HOODED STRANGER AT THE EDGE OF {HOME} ASKED YOU IF THE MILL STILL TURNS."
  say "DOES THE MILL STILL TURN? ...YOU DON'T KNOW ME. YOU WOULDN'T. I'M {TWDROWNED}. THEY SAY I DROWNED AT THE FORD [[six=SIX|EIGHT|FIVE]] WINTERS AGO. I DIDN'T. I WALKED AWAY AND LET THEM THINK IT. NOW I WANT TO COME HOME, AND I'M AFRAID."
  opt "GO HOME. THEY'LL FORGIVE YOU." -> %home
  opt "WHY DID YOU WALK AWAY?" -> %why
  opt "SOME DOORS STAY SHUT." -> %shut
stage %why
  talk twdrowned
  say "DEBTS. A TEMPER. A NIGHT I'M NOT PROUD OF. IT SEEMED KINDER TO BE DEAD THAN TO BE ME. IT WASN'T. THEY GRIEVED, AND I DRANK, AND [[=six]] YEARS WENT. <<apology>>"
  opt "GO HOME. TELL THEM THAT." -> %home
  opt "SOME DOORS STAY SHUT." -> %shut
stage %home
  talk twdrowned
  say "TODAY. BEFORE I LOSE MY NERVE. WILL YOU... NO. I'LL GO ALONE. I WALKED OUT ALONE. <<vow>>"
  do moves twdrowned home
  do fact "{TWDROWNED}, MOURNED IN {HOME} AS DROWNED [[=six]] WINTERS AGO, WALKED BACK IN ONE EVENING AND KNOCKED AT THE OLD DOOR."
  do remember twdrowned "THEY OPENED THE DOOR. MY [[SISTER|MOTHER|BROTHER]] HIT ME, THEN HELD ME. BOTH WERE FAIR. I'M HOME."
  opt "GO NOW." -> @out
stage %shut
  talk twdrowned
  say "MAYBE. MAYBE THEY'VE MENDED WHERE I WAS. YOU'RE RIGHT. I'LL... I'LL GO ON BEING DROWNED. <<grief>>"
  do remember twdrowned "STILL DROWNED. I WATCH THE MILL FROM THE HILL SOMETIMES. IT STILL TURNS. THEY'RE ALL RIGHT WITHOUT ME."
  do mark left_the_drowned_drowned
  opt "THEY'RE ALL RIGHT." -> @out
)SAGA";

const char* const kSoldierHome = R"SAGA(
role twvet person male at home
stage %back
  talk twvet
  journal "A GREY SOLDIER IN THE OLD COLOURS OF {KINGDOM}, {TWVET}, IS STANDING OUTSIDE A HOUSE IN {HOME} AND NOT KNOCKING."
  say "[[ELEVEN|TWELVE|NINE]] YEARS SINCE {KINGDOM} MARCHED ME OFF TO A WAR NOBODY REMEMBERS. TAKEN, SOLD, ESCAPED, WALKED HOME. THAT'S MY HOUSE. THERE'S A MAN SPLITTING WOOD IN MY YARD, AND MY WIFE BROUGHT HIM WATER."
  opt "KNOCK. SHE SHOULD KNOW." -> %knock
  opt "LET HER KEEP HER NEW LIFE." -> %leave
  opt "I'LL CARRY WORD FIRST." -> %word
stage %knock
  talk twvet
  say "SHE SHOULD KNOW. YES. WHATEVER SHE CHOOSES, SHE SHOULD GET TO CHOOSE IT. HOLD MY PACK. MY HANDS ARE SHAKING. <<oath>>"
  do remember twvet "SHE KNEW ME BY MY WALK BEFORE SHE SAW MY FACE. WE'VE... TALKED. ALL THREE OF US. IT'S HARD. IT'S HONEST."
  do fact "A SOLDIER OF {KINGDOM}, GIVEN UP FOR DEAD, CAME HOME TO {HOME} AFTER YEARS AWAY AND KNOCKED AT HIS OWN DOOR."
  opt "GO ON. KNOCK." -> @out
stage %leave
  talk twvet
  say "...SHE LOOKS HAPPY. I CAN'T TAKE THAT. I'LL GO ON TO THE COAST. A DEAD MAN CAN BE ANYONE. <<farewell>>"
  do remember twvet "DID YOU EVER TELL HER? NO. GOOD. DON'T. I SEND A COIN SOMETIMES, WITH NO NAME. THAT'S ENOUGH."
  do mark a_soldier_stayed_dead
  opt "GO WELL, SOLDIER." -> @out
stage %word
  talk twvet
  say "WOULD YOU? TELL HER GENTLY. TELL HER I'M NOT ANGRY. TELL HER I'LL WAIT BY THE WELL TILL DARK, AND IF SHE DOESN'T COME, I'LL UNDERSTAND."
  do remember twvet "SHE CAME TO THE WELL AT DUSK. WE TALKED TILL THE STARS WERE OUT. I'M SLEEPING IN THE BARN. IT'S A START."
  do mark carried_the_soldiers_word
  opt "BY THE WELL. TILL DARK." -> @out
)SAGA";

// ================================================================ WONDER
const char* const kFoxEyes = R"SAGA(
role twfox person female at home
stage %crust
  talk twfox
  journal "AN OLD WOMAN WITH AMBER EYES SITS BY THE WELL OF {HOME}. HER SHADOW IS THE WRONG SHAPE."
  say "A CRUST, TRAVELLER? JUST THE END OF THE LOAF. I'M NOT PROUD. ...NO, DON'T LOOK AT MY SHADOW. LOOK AT ME. A CRUST, AND I'LL TELL YOU A THING WORTH MORE THAN BREAD."
  opt "HERE. TAKE THE WHOLE LOAF." -> %shared
  opt "YOUR SHADOW HAS A TAIL." -> %seen
  opt "I'VE NOTHING TO SPARE." -> %refused
stage %shared
  talk twfox
  say "THE WHOLE LOAF. HOW LONG SINCE ANYONE... LISTEN: WHERE YOU'RE GOING, THE ONE WHO SMILES FIRST IS NOT YOUR FRIEND. AND WHEN THE WIND DROPS, RUN. <<blessing>>"
  do mark fed_the_fox_woman
  do remember twfox "THE ONE WITH THE WHOLE LOAF! I REMEMBER. FOXES ALWAYS REMEMBER. WE'RE TERRIBLE AT FORGIVING, AND WONDERFUL AT THANKS."
  opt "THE ONE WHO SMILES FIRST." -> @out
stage %seen
  talk twfox
  say "SO IT HAS. MOST PEOPLE DON'T LOOK DOWN. ...YOU'RE A NOTICING SORT. THAT WILL KEEP YOU ALIVE OR GET YOU KILLED. <<omen>>"
  opt "HERE. THE LOAF." -> %shared
  opt "GOOD DAY, GRANDMOTHER." -> %refused
stage %refused
  talk twfox
  say "NOTHING TO SPARE. NO. NOBODY EVER HAS. <<curse>>"
  do remember twfox "STILL NOTHING TO SPARE? I'VE STOPPED ASKING. THE FOXES IN THE HEDGE HAVE STOPPED TOO. THEY WATCH YOU, THOUGH."
  do mark refused_the_fox_woman
  opt "...GOOD DAY." -> @out
)SAGA";

const char* const kRoadGhost = R"SAGA(
role twstone site village near
role twghost person [[male|female]] at twstone
stage %stone
  talk twghost
  journal "BY THE MILESTONE OUTSIDE {TWSTONE} STANDS SOMEONE WHO CASTS NO SHADOW AT NOON."
  say "YOU CAN SEE ME. NOBODY SEES ME. I'VE STOOD BY THIS STONE [[hundred=A HUNDRED|NINETY|SIXTY]] WINTERS. MY BONES ARE UNDER IT. NOBODY KNOWS MY NAME TO SAY IT. SAY IT ONCE, ALOUD. IT'S {TWGHOST}. THAT'S ALL I WANT."
  opt "{TWGHOST}." -> %said
  opt "WHO PUT YOU UNDER THE STONE?" -> %who
  opt "I DON'T SPEAK TO THE DEAD." -> %walkon
stage %who
  talk twghost
  say "A FRIEND, FOR A HORSE. A GOOD HORSE. I'D HAVE GIVEN HIM THE HORSE IF HE'D ASKED. THAT'S THE PART THAT KEEPS ME HERE, I THINK. <<grief>>"
  opt "{TWGHOST}. REST NOW." -> %said
  opt "I CAN'T HELP YOU." -> %walkon
stage %said
  talk twghost
  say "AH. THAT'S IT. THAT'S WHAT IT SOUNDS LIKE. ...LISTEN, BEFORE I GO: THE ROAD AHEAD HAS TEETH. KEEP YOUR BACK TO A WALL. <<farewell>>"
  do mark said_the_ghosts_name
  do fact "THE COLD FIGURE BY THE MILESTONE AT {TWSTONE} IS GONE. A TRAVELLER SAID ITS NAME, {TWGHOST}, AND IT WENT."
  opt "REST, {TWGHOST}." -> @out
stage %walkon
  talk twghost
  say "NO. NOBODY DOES. ANOTHER [[=hundred]] WINTERS, THEN. <<grief>>"
  do mark walked_past_a_ghost
  opt "..." -> @out
)SAGA";

const char* const kWhiteHart = R"SAGA(
role twhuntsman person male at home
stage %hart
  talk twhuntsman
  journal "OUTSIDE {HOME}, A HUNTSMAN, {TWHUNTSMAN}, HAS AN ARROW ON THE STRING AND A WHITE HART IN HIS SIGHTS."
  say "QUIET! THERE, IN THE BIRCHES. WHITE AS MILK, ANTLERS LIKE A CROWN. A WHITE HART. THE HIDE ALONE IS A YEAR'S BREAD, AND THE LORDS PAY MORE FOR THE HEAD. ONE ARROW, {PLAYER}. ONE."
  opt "SPOIL THE SHOT." -> %spoil
  opt "LET HIM LOOSE." -> %loose
  opt "HERE. TEN SILVER. LET IT GO." check gold 10 -> %bought else %spoil
stage %spoil
  talk twhuntsman
  say "YOU... YOU STEPPED ON A STICK ON PURPOSE. I SAW YOU. A YEAR'S BREAD, GONE INTO THE BIRCHES. <<curse>> ...AND YET. DID YOU SEE IT LOOK BACK AT YOU?"
  do mark spared_the_white_hart
  do remember twhuntsman "THE STICK-STEPPER. I'VE NOT SEEN THE HART SINCE. I'VE HAD GOOD HUNTING, THOUGH. STRANGE, THAT."
  opt "IT LOOKED BACK." -> @out
stage %loose
  talk twhuntsman
  say "...IT'S DOWN. IT'S... WHY IS IT SO QUIET? EVEN THE ROOKS HAVE STOPPED. <<omen>>"
  do fact "A WHITE HART WAS SHOT IN THE BIRCHES OUTSIDE {HOME}. THE OLD ONES SAY NO GOOD COMES OF THAT. THE HUNTSMAN IS RICH."
  do remember twhuntsman "I SOLD THE HEAD TO A LORD. I'VE NOT SLEPT A NIGHT THROUGH SINCE. I DREAM OF BIRCHES."
  do mark saw_the_white_hart_fall
  opt "IT WAS ONLY A DEER." -> @out
stage %bought
  talk twhuntsman
  do gold -10
  say "TEN SILVER FOR A DEER YOU DON'T EVEN WANT? ...TAKE YOUR HART, THEN. IT'S ALREADY GONE. <<doubt>>"
  do mark spared_the_white_hart
  do remember twhuntsman "THE ONE WHO BOUGHT A WHITE HART AND LET IT RUN. I TELL THAT IN EVERY ALEHOUSE. NOBODY BELIEVES ME."
  opt "IT'S ALREADY GONE." -> @out
)SAGA";

// ================================================================ WORLD
const char* const kRaidHome = R"SAGA(
role twrunner2 person [[male|female]] at home
stage %news
  talk twrunner2
  journal "{TWRUNNER2} CAME RUNNING DOWN THE ROAD FROM {HOME} WITH BLOOD ON {TWRUNNER2.HIS} SLEEVE."
  say "<<urgency>> RAIDERS ON THE {HOME} ROAD! THEY TOOK THE MILL CART AND THEY'RE IN THE BARNS NOW. FOUR OF THEM, MAYBE MORE. THE MEN ARE IN THE FIELDS. YOU'VE A SWORD. PLEASE."
  opt "SHOW ME WHERE." -> %fight
  opt "MY ERRAND CAN'T WAIT." -> %press
stage %fight
  goal kill 4 bandit
  then %saved
  journal "RAIDERS ARE IN THE BARNS OF {HOME}. DRIVE THEM OFF BEFORE YOUR ERRAND GOES ANY FURTHER."
stage %saved
  talk twrunner2
  say "THEY'RE GONE. THE CART'S BACK, MOSTLY. ONE WHEEL. ...NOBODY DIED. DO YOU KNOW HOW RARE THAT IS? <<thanks>>"
  do fame 1
  do fact "RAIDERS HIT {HOME} AND WERE DRIVEN OFF BY A TRAVELLER WHO TURNED BACK FROM THEIR OWN ROAD TO DO IT."
  do remember twrunner2 "YOU TURNED BACK FOR US. NOBODY ELSE ON THAT ROAD DID. THE MILLER'S NAMED A HEN AFTER YOU."
  opt "ONE WHEEL'S A START." -> @out
stage %press
  talk twrunner2
  say "CAN'T WAIT. NO. NOTHING EVER CAN. <<grief>>"
  do remember twrunner2 "WE LOST THE CART AND THE BARLEY. NOBODY DIED. NO THANKS TO YOU. YOUR ERRAND WAS IMPORTANT, I'M SURE."
  do mark passed_the_raid_by
  opt "I'M SORRY." -> @out
)SAGA";

const char* const kLevyPress = R"SAGA(
role twsarge person [[male|female]] at home
stage %press
  talk twsarge
  journal "A SERGEANT OF {KINGDOM}, {TWSARGE}, IS PRESSING MEN INTO THE LEVY IN {HOME}, AND HAS SEEN YOUR SWORD."
  say "YOU. WITH THE SWORD. {KINGDOM.LORD} HAS A WAR AND I HAVE A QUOTA, AND I'M ONE SHORT. A DAY'S DRILL AND A WEEK ON THE WALL. OR SHOW ME A REASON WHY NOT. <<threat>>"
  opt "A DAY'S DRILL, THEN." -> %drill
  opt "HERE. FOR THE QUOTA BOOK." check gold 10 -> %bribe else %errand
  opt "I'M ON AN ERRAND. HEAR IT." check level 3 -> %excused else %drill
stage %drill
  goal wait 1
  then %released
  journal "A DAY OF DRILL WITH {KINGDOM}'S LEVY: SPEAR, SHIELD, SHIELD, SPEAR, AND A SERGEANT WHO NEVER TIRES."
stage %released
  talk twsarge
  say "YOU'VE DONE THIS BEFORE. TOO WELL. I'M NOT SENDING YOU TO THE WALL; YOU'D ONLY MAKE THE OTHERS LOOK BAD. GO ON. FINISH YOUR ERRAND."
  do rep kingdom 2
  do remember twsarge "THE ONE WHO DRILLED A DAY WITHOUT COMPLAINING. IF YOU EVER WANT A CORPORAL'S BADGE, IT'S YOURS."
  opt "KEEP THE BADGE WARM." -> @out
stage %bribe
  talk twsarge
  do gold -10
  say "THE QUOTA BOOK THANKS YOU. THE QUOTA BOOK HAS NEVER SEEN YOU. <<doubt>>"
  do remember twsarge "I NEVER SAW YOU. I'VE A SHARP MEMORY FOR THE FACES I NEVER SAW."
  do mark bought_off_the_levy
  opt "NEVER SEEN ME." -> @out
stage %errand
  talk twsarge
  say "NO SILVER? THEN IT'S THE DRILL. EVERYONE DRILLS. EVEN SAINTS. ESPECIALLY SAINTS."
  opt "THE DRILL, THEN." -> %drill
stage %excused
  talk twsarge
  say "...HM. THAT'S EITHER TRUE OR THE BEST LIE I'VE HEARD THIS MONTH. GO. AND IF YOU'RE LYING, DON'T COME BACK THIS WAY."
  do remember twsarge "YOU. THE ERRAND. WAS IT TRUE? DON'T TELL ME. I LIKE NOT KNOWING."
  opt "IT'S TRUE." -> @out
)SAGA";

const char* const kHungryRoad = R"SAGA(
role twwidow person female at home
stage %road
  talk twwidow
  journal "A FAMILY FROM A BURNED VILLAGE HAS STOPPED IN {HOME}: {TWWIDOW} AND [[kids=THREE|FOUR|TWO]] CHILDREN, NONE WITH SHOES."
  say "WE'RE FROM UP THE VALLEY. WAS UP THE VALLEY. RAIDERS, THEN FIRE, THEN NOTHING. WE'VE WALKED [[days=FIVE|SIX|FOUR]] DAYS. I DON'T ASK FOR MYSELF. THE SMALLEST HASN'T CRIED SINCE YESTERDAY, AND THAT FRIGHTENS ME MORE THAN CRYING."
  opt "HERE. ALL MY FOOD." -> %fed
  opt "TAKE THIS SILVER." check gold 10 -> %silver else %fed
  opt "I CAN'T FEED EVERYONE." -> %nothing
stage %fed
  talk twwidow
  say "ALL OF IT? ...LOOK. SHE'S EATING. SHE'S CRYING, TOO. THAT'S GOOD. THAT'S GOOD. <<thanks>>"
  do fame 1
  do remember twwidow "WE'VE A ROOF NOW, IN {HOME}. THE SMALLEST ASKS AFTER YOU. SHE CALLS YOU THE BREAD ONE."
  do fact "A FAMILY BURNED OUT OF THE VALLEY CAME TO {HOME} ON THE ROAD, AND A TRAVELLER GAVE THEM EVERY CRUMB THEY CARRIED."
  opt "LOOK AFTER THEM." -> @out
stage %silver
  talk twwidow
  do gold -10
  say "SILVER. THAT'S A WEEK. THAT'S A WEEK OF NOT BEING AFRAID. I'LL PAY YOU BACK. I WON'T, I KNOW. BUT I'LL MEAN TO. <<blessing>>"
  do remember twwidow "I STILL MEAN TO PAY YOU BACK. THE CHILDREN HAVE SHOES NOW. I'LL PAY YOU IN SHOES, MAYBE."
  opt "NO NEED." -> @out
stage %nothing
  talk twwidow
  say "NO. NOBODY CAN. I KNOW. <<farewell>>"
  do remember twwidow "WE GOT BY. OTHERS HELPED. YOU WEREN'T ONE OF THEM. I DON'T HOLD IT AGAINST YOU. MUCH."
  do mark passed_the_hungry
  opt "I'M SORRY." -> @out
)SAGA";

// ================================================================ the phase A worked examples
const char* const kRivalClaim = R"SAGA(
role %rival person [[male|female]] at home
stage %claim
  talk %rival
  say "<<surprise>> YOU'RE THE ONE {GIVER} SENT? SO AM I. THE SAME PROMISE, THE SAME WEEK. AND I HAVE TWO CHILDREN AND A ROOF THAT LEAKS. SO: DO WE SHARE IT, OR FIGHT OVER IT LIKE DOGS?"
  opt "WE SHARE IT." -> %share
  opt "TAKE IT ALL. I DON'T NEED IT." -> %yield
  opt "GO HOME. THIS ONE IS MINE." -> %refuse
stage %share
  talk %rival
  say "...HUH. FAIR, THEN. <<oath>>"
  do remember %rival "HALF A REWARD AND A WHOLE FRIEND. NOT A BAD TRADE, THAT."
  opt "LET'S BE ON WITH IT." -> @out
stage %yield
  talk %rival
  say "YOU'D... THANK YOU. I WON'T FORGET IT. <<blessing>>"
  do remember %rival "THERE'S THE ONE WHO GAVE ME THEIR SHARE. MY ROOF IS DRY BECAUSE OF YOU."
  do fame 1
  opt "MEND THE ROOF." -> @out
stage %refuse
  talk %rival
  say "[[THEN I HOPE IT CHOKES YOU.|SO THAT'S THE SORT YOU ARE.]] <<curse>>"
  do remember %rival "OH, IT'S YOU. ENJOYING MY SHARE, ARE YOU?"
  opt "[[MAYBE IT WILL.|GOOD DAY TO YOU TOO.]]" -> @out
)SAGA";

const char* const kRoadOmen = R"SAGA(
role %seer person female at home
stage %sign
  talk %seer
  say "STOP. YOU WALK WITH A THREAD BEHIND YOU, {PLAYER}, AND SOMEONE IN {HOME} IS HOLDING THE OTHER END. <<proverb>> A COIN, AND I'LL TELL YOU IF IT FRAYS."
  opt "HERE. TELL ME." check gold 5 -> %told else %poor
  opt "I DON'T BUY FORTUNES." -> @out
stage %told
  talk %seer
  do gold -5
  say "IT HOLDS. BUT IT HOLDS BECAUSE SOMEONE IS PULLING. WHEN YOU GET WHERE YOU'RE GOING, WATCH THE ONE WHO WILL NOT MEET YOUR EYES."
  do remember %seer "THE THREAD HELD, DIDN'T IT? IT USUALLY DOES, FOR THOSE WHO PAY."
  opt "I'LL REMEMBER." -> @out
stage %poor
  talk %seer
  say "NO COIN? THEN HAVE IT FREE, SINCE YOU'LL NEED IT. <<doubt>>"
  do remember %seer "THE ONE WITH EMPTY POCKETS AND A THREAD BEHIND THEM. DID IT HOLD?"
  opt "...THANK YOU, I THINK." -> @out
)SAGA";

struct Def {
  const char* id;
  const char* name;
  uint32_t family;
  const char* roles;
  const char* excludes;
  const char* body;
};

const Def kDefs[] = {
    // deceit
    {"giver_lied", "THE HALF-TOLD TALE", TF_DECEIT, "giver:giver home:site", "", kGiverLied},
    {"false_message", "THE FALSE MESSENGER", TF_DECEIT, "giver:giver home:site", "", kFalseMessage},
    {"kind_host", "THE KIND HOST", TF_DECEIT, "giver:giver home:site", "", kKindHost},
    // identity
    {"cursed_one", "THE NAME UNDER THE CURSE", TF_IDENTITY, "foe:beast/boss", "beast_wronged wronged_first", kCursedOne},
    {"foe_child", "THE FOE'S OWN CHILD", TF_IDENTITY, "foe:foe", "cursed_one", kFoeChild},
    {"heir_in_rags", "THE CREST IN THE DUST", TF_IDENTITY, "kingdom:kingdom", "", kHeirInRags},
    // rival
    {"rival_claim", "THE SECOND SEEKER", TF_RIVAL, "giver:giver home:site", "", kRivalClaim},
    {"rival_hunter", "THE BOUNTY HUNTER", TF_RIVAL, "foe:foe/beast/boss", "", kRivalHunter},
    {"rival_crown", "THE CROWN'S CLAIM", TF_RIVAL, "kingdom:kingdom", "levy_press", kRivalCrown},
    // prophecy
    {"omen_other", "THE OMEN MEANT ANOTHER", TF_PROPHECY, "home:site", "false_chosen road_omen", kOmenOther},
    {"sign_misread", "THE TAILED STAR", TF_PROPHECY, "home:site", "", kSignMisread},
    {"false_chosen", "THE WRONG CHOSEN ONE", TF_PROPHECY, "home:site", "", kFalseChosen},
    // mercy
    {"wronged_first", "THE TANNER'S GRIEVANCE", TF_MERCY, "foe:foe", "foe_child", kWrongedFirst},
    {"beast_wronged", "THE EMPTY DEN", TF_MERCY, "foe:beast/boss", "", kBeastWronged},
    {"thief_caught", "THE HAND IN YOUR PACK", TF_MERCY, "giver:giver", "", kThiefCaught},
    {"deserter", "THE DESERTER IN THE BYRE", TF_MERCY, "kingdom:kingdom home:site", "levy_press soldier_home", kDeserter},
    // price
    {"ferry_price", "THE FERRYMAN'S FARE", TF_PRICE, "giver:giver", "", kFerryPrice},
    {"healer_fee", "THE HEALER'S PRICE", TF_PRICE, "home:site", "", kHealerFee},
    {"long_way", "THE TOLL ROAD", TF_PRICE, "giver:giver", "", kLongWay},
    // betrayal
    {"guide_ambush", "THE SHORT WAY", TF_BETRAYAL, "home:site", "long_way", kGuideAmbush},
    {"informer", "THE EAR AT THE WINDOW", TF_BETRAYAL, "home:site", "", kInformer},
    {"turned_friend", "THE PAID COMPANION", TF_BETRAYAL, "home:site", "false_message", kTurnedFriend},
    // return
    {"back_from_dead", "THE DROWNED WHO WALKED", TF_RETURN, "home:site", "", kBackFromDead},
    {"soldier_home", "THE LEVY'S LAST MAN", TF_RETURN, "kingdom:kingdom home:site", "", kSoldierHome},
    // wonder
    {"road_omen", "THE THREAD AND THE SEER", TF_WONDER, "giver:giver home:site", "", kRoadOmen},
    {"fox_eyes", "THE WOMAN WITH FOX EYES", TF_WONDER, "home:site", "", kFoxEyes},
    {"road_ghost", "THE COLD MAN AT THE STONE", TF_WONDER, "giver:giver", "", kRoadGhost},
    {"white_hart", "THE WHITE HART", TF_WONDER, "home:site", "", kWhiteHart},
    // world
    {"raid_home", "THE RAID ON THE HOME ROAD", TF_WORLD, "home:site", "", kRaidHome},
    {"levy_press", "THE PRESS-GANG", TF_WORLD, "kingdom:kingdom home:site", "", kLevyPress},
    {"hungry_road", "THE BURNED-OUT FAMILY", TF_WORLD, "home:site", "", kHungryRoad},
};

}  // namespace

void addTwists(std::vector<Twist>& v) {
  for (const Def& d : kDefs) {
    Twist t;
    t.id = d.id;
    t.name = d.name;
    t.family = d.family;
    t.roles = d.roles;
    t.excludes = d.excludes;
    t.body = d.body;
    v.push_back(t);
  }
}

}  // namespace arch
}  // namespace saga
}  // namespace story
