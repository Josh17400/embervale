// M6b "Sagas": archetypes from epic and Arthurian legend (close retellings allowed). ARCHETYPES lane. The template
// markup and role conventions: rpg/story/saga.h; twists: twists.cpp.
//
//   stone_and_anvil THE SWORD IN THE STONE          the stone in the ruin that no lord can move, and the squire who
//                                                   pulled it out looking for a sword for someone else
//   unasked_question THE CUP AND THE UNASKED QUESTION the wounded lord, the waste land, the cup carried past, and the
//                                                   knight who was too polite to ask
//   green_wager     THE GREEN STRANGER'S GAME       a blow for a blow in a year and a day; a host's exchange of winnings
//                                                   and a green girdle
#include <vector>
#include "rpg/story/arch/arch_tables.h"

namespace story {
namespace saga {
namespace arch {

namespace {

// ---------------------------------------------------------------- the sword in the stone
const char* const kStoneAndAnvil = R"SAGA(
title [[THE SWORD IN THE STONE|WHOSO PULLETH|THE SQUIRE AND THE ANVIL]]
hook herald
pitch "[[pitch=WHAT NEWS FROM THE OLD RUIN?|WHAT'S THIS ABOUT A SWORD?|WHO HAS TRIED THE STONE?]]"
hint "[[~pitch|HEAR THIS! A SWORD STANDS IN A STONE IN THE OLD RUIN, AND LETTERS OF GOLD ON THE STONE. EVERY LORD HAS TRIED IT. NONE HAS MOVED IT.|THE LORDS PULLED AT IT TILL THEIR FACES WENT PURPLE. IT DIDN'T NOTICE.|HEAR THIS! A SWORD STANDS IN A STONE IN THE OLD RUIN, AND LETTERS OF GOLD ON THE STONE. EVERY LORD HAS TRIED IT. NONE HAS MOVED IT.]]"
role giver giver
role home site home
role kingdom kingdom home
role ruin ruin near
role squire person [[male|female]] at home
role knight person male at home
var proof 0
slot t1 -> pulled rival deceit prophecy wonder
slot t2 -> council betrayal mercy price world

stage start
  talk giver
  say "IN {RUIN}, WHERE {RUIN.LORD} ONCE HELD COURT, A SWORD STANDS IN AN ANVIL ON A STONE. THE LETTERS SAY: WHOSO PULLS THIS SWORD IS RIGHTWISE BORN TO HOLD {KINGDOM} WHEN ITS CROWN FALLS VACANT. [[lords=FORTY|THIRTY|SIXTY]] LORDS HAVE TRIED."
  opt "I'LL TRY THE STONE." -> try
  opt "WHO SET IT THERE?" -> who
  opt "LORDS AND THEIR TOYS." -> refused

stage refused
  end fail
  say "AS YOU LIKE. THE STONE WILL WAIT. IT HAS WAITED [[=lords]] LORDS ALREADY. <<farewell>>"
  journal "YOU PAID NO HEED TO THE SWORD IN THE STONE AT {RUIN}."
  do remember giver "THE SWORD'S STILL IN THE STONE. A GOAT PULLED AT IT YESTERDAY. NO LUCK EITHER."

stage who
  talk giver
  say "NOBODY KNOWS. IT WAS THERE ONE MORNING AFTER A STORM, WITH SNOW ON THE HILT AND NONE ON THE STONE. SOME SAY {RUIN.LORD} LEFT IT FOR A TRUE HEIR. SOME SAY A WIZARD WITH A SENSE OF HUMOUR. <<omen>>"
  opt "I'LL TRY IT." -> try
  opt "WHO ELSE IS GOING?" -> knight_talk

stage try
  goal goto ruin
  then stuck
  journal "A SWORD STANDS IN A STONE AT {RUIN}. WHOEVER PULLS IT IS BORN TO HOLD {KINGDOM}. TRY IT."

stage stuck
  talk squire
  say "YOU TRIED IT TOO? IT'S LIKE PULLING ON A MOUNTAIN. ...I'M NOT HERE FOR THE STONE. MY MASTER {KNIGHT} BROKE HIS SWORD AT THE TOURNEY AND SENT ME TO FIND ANOTHER, AND THE ARMOURERS ARE ALL SHUT FOR THE FEAST. SO I THOUGHT..."
  opt "TRY IT, THEN." -> @t1
  opt "IT'S NOT FOR SQUIRES." -> knight_talk

stage knight_talk
  talk knight
  say "MY SQUIRE? {SQUIRE} IS A GOOD LAD... A GOOD SQUIRE. FOUNDLING. WE TOOK {SQUIRE.HIM} IN THE WINTER {RUIN.LORD}'S LINE WAS SAID TO HAVE ENDED, WRAPPED IN A CLOAK TOO FINE FOR ANYONE WHO'D LEAVE A CHILD. I NEVER ASKED. YOU DON'T, WITH CLOAKS LIKE THAT."
  do set proof 1
  opt "BRING {SQUIRE} TO THE STONE." -> @t1

stage pulled
  talk squire
  say "IT CAME OUT. IT JUST... CAME OUT, LIKE A SPOON FROM PORRIDGE. {PLAYER}, WHY IS EVERYONE KNEELING? GET UP. PLEASE GET UP. I ONLY WANTED A SWORD FOR {KNIGHT}. <<surprise>>"
  opt "PUT IT BACK AND PULL AGAIN." -> again
  opt "THEY'RE KNEELING TO YOU." -> @t2

stage again
  talk squire
  say "IN. OUT. IN. OUT. IT GOES BACK LIKE A KNIFE INTO BUTTER AND COMES OUT FOR NOBODY BUT ME. THE LORDS HAVE ALL TRIED AGAIN. ONE OF THEM IS CRYING. I FEEL TERRIBLE."
  opt "THE LORDS WILL HAVE TO ANSWER." -> @t2

stage council
  talk knight
  say "THE LORDS SAY A FOUNDLING SQUIRE CAN'T BE BORN TO ANYTHING BUT MUCKING OUT. THEY WANT A TOURNEY, A VOTE, ANYTHING BUT THIS. AND {SQUIRE} WANTS TO GIVE THE SWORD BACK. WHAT DO WE DO, {PLAYER}?"
  opt "PULL IT IN FRONT OF THEM ALL." -> proclaim_end
  opt "HIDE {SQUIRE.HIM} AWAY TO GROW." -> hidden_end
  opt "LET {SQUIRE.HIM} REFUSE IT." -> refuse_end

stage proclaim_end
  end success
  say "AT EVERY FEAST THAT YEAR, BEFORE EVERY LORD WHO DOUBTED, {SQUIRE} PUT THE SWORD BACK IN THE STONE AND DREW IT OUT AGAIN. BY THE LAST FEAST NOBODY ASKED FOR A VOTE. {KNIGHT} WAS FIRST TO KNEEL."
  journal "{SQUIRE}, A FOUNDLING SQUIRE OF {HOME}, DREW THE SWORD FROM THE STONE AT {RUIN} AND IS HAILED AS HEIR TO {KINGDOM}."
  do reward rich
  do rep kingdom 3
  do remember squire "THEY CALL ME HEIR NOW. I STILL MUCK OUT {KNIGHT}'S HORSE EVERY MORNING. SOMEONE HAS TO. IT'S A GOOD HORSE."
  do fact "{SQUIRE}, A FOUNDLING SQUIRE OF {HOME}, DREW THE SWORD FROM THE STONE AT {RUIN} AFTER [[=lords]] LORDS FAILED. {KINGDOM} HAILS {SQUIRE.HIM} HEIR."
  do mark the_sword_drawn

stage hidden_end
  end success
  say "{KNIGHT} TOOK {SQUIRE} NORTH TO AN OLD MANOR WITH THICK WALLS AND A GOOD LIBRARY. THE SWORD IS BACK IN THE STONE. ONLY THREE PEOPLE KNOW WHO CAN DRAW IT, AND YOU ARE ONE OF THEM."
  journal "THE SWORD IS BACK IN THE STONE AT {RUIN}. {SQUIRE}, WHO DREW IT, IS HIDDEN AWAY TO GROW INTO IT."
  do reward fair
  do remember knight "{SQUIRE} READS ALL DAY NOW AND FIGHTS ALL EVENING. ASKS ME ABOUT THE CLOAK. I TELL {SQUIRE.HIM} WHAT I KNOW. IT ISN'T MUCH."
  do fact "THE SWORD STANDS IN THE STONE AT {RUIN} STILL. THEY SAY SOMEONE DREW IT ONCE, AND PUT IT BACK, AND WENT AWAY TO GROW UP."
  do mark heir_in_hiding

stage refuse_end
  end success
  say "{SQUIRE} WALKED TO THE STONE, PUT THE SWORD BACK, AND SAID IN FRONT OF EVERYONE: I'D SOONER BE A GOOD SQUIRE THAN A BAD KING. THE LORDS LAUGHED. {KNIGHT} DID NOT. NEITHER DID THE STONE."
  journal "{SQUIRE} DREW THE SWORD FROM THE STONE AT {RUIN}, AND PUT IT BACK, AND WILL NOT BE KING. THE LORDS ARE RELIEVED. FOR NOW."
  do reward fair
  do remember squire "I CAN STILL DRAW IT. I CHECKED, ONCE, AT NIGHT. DON'T TELL ANYONE. I DON'T WANT IT. I JUST WANTED TO KNOW."
  do fact "THEY SAY A SQUIRE OF {HOME} DREW THE SWORD AT {RUIN} AND PUT IT BACK, SAYING A GOOD SQUIRE BEATS A BAD KING."
)SAGA";

// ---------------------------------------------------------------- the grail and the unasked question
const char* const kUnaskedQuestion = R"SAGA(
title [[THE CUP AND THE UNASKED QUESTION|THE WOUNDED LORD|THE WASTE LAND]]
hook npc farmer
pitch "[[pitch=WHY IS YOUR FIELD ALL DUST?|WHAT'S WRONG WITH THE SOIL?|WHEN DID IT LAST RAIN HERE?]]"
hint "[[~pitch|NOTHING GROWS. NOT SINCE THE LORD UP AT THE OLD HALL TOOK HIS WOUND. THE LAND KNOWS, THEY SAY.|NOTHING GROWS. NOT SINCE THE LORD UP AT THE OLD HALL TOOK HIS WOUND. THE LAND KNOWS, THEY SAY.|SEVEN YEARS OF DUST. THE OLD ONES SAY THE LAND IS SICK BECAUSE ITS LORD IS. I JUST FARM IT.]]"
role giver giver
role home site home
role ruin ruin near
role lord person male at ruin
role knight person [[male|female]] at home
var question 0
slot t1 -> go price rival deceit wonder
slot t2 -> second mercy prophecy identity world

stage start
  talk giver
  say "THE LORD OF THE OLD HALL AT {RUIN} TOOK A WOUND IN THE THIGH [[years=SEVEN|NINE|TWELVE]] YEARS AGO THAT WON'T CLOSE. AND SINCE THEN NOTHING GROWS FOR TEN MILES. HE FISHES ALL DAY IN THE MOAT BECAUSE IT'S ALL HE CAN DO. <<grief>>"
  opt "WHAT WOULD HEAL HIM?" -> heal
  opt "I'LL GO TO THE HALL." -> @t1
  opt "DROUGHTS END ON THEIR OWN." -> refused

stage refused
  end fail
  say "THIS ONE HASN'T. <<farewell>>"
  journal "YOU LEFT THE WASTE LAND AROUND {RUIN} TO ITS DUST."
  do remember giver "STILL DUST. I'VE SOLD THE PLOUGH. I SELL ROPE NOW. PEOPLE ALWAYS NEED ROPE."

stage heal
  talk knight
  say "I WAS THERE. I WAS GUESTED IN THAT HALL. A PROCESSION PASSED: A BLEEDING SPEAR, A SILVER DISH, AND A CUP THAT SHONE SO YOU COULDN'T LOOK AT IT. I SAW IT ALL AND SAID NOTHING. MY TEACHER TOLD ME A GUEST SHOULD NOT ASK QUESTIONS. <<apology>>"
  opt "WHAT SHOULD YOU HAVE ASKED?" -> what
  opt "COME BACK WITH ME." -> @t1

stage what
  talk knight
  say "I DON'T KNOW. THAT'S THE WORST OF IT. I WOKE IN AN EMPTY HALL AND RODE OUT INTO THE DUST, AND A WOMAN AT THE CROSSROADS CURSED ME FOR A FOOL. SHE SAID ONE QUESTION WOULD HAVE HEALED HIM. SHE DIDN'T SAY WHICH."
  opt "THEN WE'LL FIND IT TOGETHER." -> @t1

stage go
  goal goto ruin
  then hall
  journal "THE LORD OF THE OLD HALL AT {RUIN} BEARS A WOUND THAT WILL NOT HEAL, AND THE LAND DIES WITH HIM. GO TO HIS HALL."

stage hall
  talk lord
  say "A GUEST. SIT. THE FISH ARE NOT BITING; THEY HAVEN'T BITTEN IN [[=years]] YEARS. ...HUSH, NOW. HERE IT COMES. THE PROCESSION. THE SPEAR, THE DISH, THE CUP. EVERY NIGHT. NOBODY EVER SAYS ANYTHING."
  opt "WHOM DOES THE CUP SERVE?" -> asked
  opt "WHAT AILS YOU, LORD?" -> asked
  opt "SAY NOTHING. WATCH." -> silent

stage silent
  talk lord
  say "...AND IT'S GONE. LIKE EVERY NIGHT. YOU'RE POLITE, STRANGER. EVERYONE IS SO POLITE. <<grief>>"
  opt "TAKE THE CUP BY FORCE." -> grab
  opt "I'LL COME BACK TOMORROW." -> @t2

stage grab
  talk lord
  say "YOU REACHED FOR IT AND IT WAS NOT THERE, AND YOUR HAND IS COLD TO THE ELBOW. THE CUP DOES NOT GO TO THOSE WHO GRAB. YOU KNOW THAT. EVERYONE KNOWS THAT. <<warning>>"
  opt "THEN I FAILED." -> unworthy_end

stage second
  talk knight
  say "A SECOND NIGHT. NOBODY GETS A SECOND NIGHT, THE STORIES SAY. BUT HERE WE ARE, AND HERE IT COMES AGAIN. {PLAYER}, LET ME. PLEASE. I'VE HAD [[=years]] YEARS TO THINK OF WHAT I SHOULD HAVE SAID."
  opt "ASK IT, {KNIGHT}." -> knight_end
  opt "NO. I'LL ASK." -> asked

stage asked
  talk lord
  say "...ASKED. SOMEBODY ASKED. [[=years]] YEARS AND NOBODY ASKED. THE CUP SERVES THE ONE WHO NEEDS IT AND DOES NOT KNOW TO ASK. IT SERVES ME. I AM SO TIRED, {PLAYER}. LOOK: MY WOUND HAS CLOSED. <<relief>>"
  do set question 1
  opt "AND THE LAND?" -> healed_end

stage healed_end
  end success
  say "IT RAINED THAT NIGHT FOR THE FIRST TIME IN [[=years]] YEARS. {GIVER} STOOD IN THE FIELD AND LET IT SOAK HIM TO THE SKIN. BY SPRING THE DUST AROUND {RUIN} WAS GREEN."
  journal "YOU ASKED THE QUESTION IN THE HALL AT {RUIN}. THE LORD'S WOUND HAS CLOSED, AND THE WASTE LAND IS GREEN AGAIN."
  do reward rich
  do remember giver "BARLEY! I BOUGHT A PLOUGH BACK FROM THE MAN I SOLD MINE TO. HE CHARGED ME DOUBLE. I DIDN'T EVEN ARGUE."
  do fact "THE WASTE LAND ROUND {RUIN} TURNED GREEN AFTER [[=years]] YEARS WHEN A STRANGER ASKED THE WOUNDED LORD THE QUESTION NOBODY HAD."
  do mark the_question_asked

stage knight_end
  end success
  say "{KNIGHT} ASKED, VOICE SHAKING, AND THE LORD WEPT AND THE WOUND CLOSED, AND THE RAIN CAME. {KNIGHT} STAYED ON AT THE HALL TO SERVE HIM. THEY FISH TOGETHER. THE FISH BITE NOW."
  journal "{KNIGHT} ASKED THE QUESTION AT LAST, AND THE LAND AROUND {RUIN} IS HEALED. {KNIGHT} SERVES THE LORD THERE NOW."
  do reward rich
  do remember knight "I ASKED. A SECOND CHANCE. NOBODY GETS A SECOND CHANCE AND I GOT ONE. I DON'T KNOW WHAT TO DO WITH THAT EXCEPT SPEND IT WELL."
  do fact "THE KNIGHT WHO FAILED TO ASK THE QUESTION IN THE HALL AT {RUIN} CAME BACK AND ASKED IT, AND THE WASTE LAND HEALED."
  do mark the_question_asked

stage unworthy_end
  end fail
  say "THE HALL WAS EMPTY IN THE MORNING. NO LORD, NO PROCESSION, NO CUP, ONLY DUST ON THE TABLE THICK AS FELT. YOUR HAND IS STILL COLD."
  journal "YOU GRASPED AT THE CUP IN THE HALL AT {RUIN} AND IT WAS NOT THERE. THE WASTE LAND STAYS WASTE."
  do remember giver "YOU WENT TO THE HALL AND CAME BACK WITH A COLD HAND. THAT'S WHAT THEY ALL COME BACK WITH. DUST AND A COLD HAND."
  do fact "SOMEONE REACHED FOR THE SHINING CUP IN THE HALL AT {RUIN} WITHOUT ASKING, AND THE HALL WAS EMPTY BY MORNING."
)SAGA";

// ---------------------------------------------------------------- the green knight
const char* const kGreenWager = R"SAGA(
title [[THE GREEN STRANGER'S GAME|A BLOW FOR A BLOW|THE GREEN GIRDLE]]
hook npc innkeeper
pitch "[[pitch=WHO'S THE BIG MAN IN GREEN?|WHY IS THE HALL SO QUIET?|WHAT'S THE STRANGER WAITING FOR?]]"
hint "[[~pitch|A MAN CAME IN AT SUPPER, GREEN FROM HIS BOOTS TO HIS BEARD, WITH AN AXE IN ONE HAND AND A HOLLY BRANCH IN THE OTHER.|A MAN CAME IN AT SUPPER, GREEN FROM HIS BOOTS TO HIS BEARD, WITH AN AXE IN ONE HAND AND A HOLLY BRANCH IN THE OTHER.|NOBODY'S TOUCHED THEIR SUPPER SINCE THE GREEN ONE WALKED IN. HE'S ASKING FOR A GAME.]]"
role giver giver
role home site home
role green person male at home
role chapel ruin near
role far site village near
role host person male at far
role lady person female at far
var girdle 0
var told 0
var hid 0
slot t1 -> journey price rival wonder world
slot t2 -> chapel_gate mercy prophecy betrayal

stage start
  talk giver
  say "<<warning>> THE GREEN ONE WANTS A GAME. ANYONE MAY STRIKE HIM ONE BLOW WITH HIS OWN AXE, AND IN A YEAR AND A DAY, HE'LL RETURN THE BLOW AT {CHAPEL}. NOBODY'S MOVED. HE'S STARTED LAUGHING AT US."
  opt "I'LL PLAY HIS GAME." -> strike
  opt "IT'S A TRICK. ASK HIM WHAT KIND." -> green_talk
  opt "LET HIM LAUGH." -> refused

stage refused
  end fail
  say "HE LAUGHED ALL THE WAY OUT THE DOOR. HE'LL TELL THE STORY OF US FOR A HUNDRED YEARS. <<insult>>"
  journal "NOBODY IN {HOME} PLAYED THE GREEN STRANGER'S GAME. NOT EVEN YOU."
  do remember giver "HE LAUGHED ALL THE WAY TO THE ROAD. I STILL HEAR IT WHEN I LOCK UP."

stage green_talk
  talk green
  say "A TRICK? THE GAME IS EXACTLY WHAT I SAID: ONE BLOW FOR ONE BLOW. NO TRICK BUT COURAGE, AND KEEPING YOUR WORD WHEN IT'S COLD AND YOU'D RATHER NOT. IS THAT A TRICK, IN {HOME}? <<boast>>"
  opt "THEN BOW YOUR HEAD." -> strike
  opt "NOBODY HERE IS THAT FOOLISH." -> refused

stage strike
  talk green
  say "A CLEAN BLOW. MY COMPLIMENTS. ...WHAT ARE YOU STARING AT? HAVE YOU NEVER SEEN A MAN PICK UP HIS OWN HEAD? A YEAR AND A DAY, {PLAYER}. {CHAPEL}. DON'T BE LATE. I HATE TO BE KEPT WAITING. HA!"
  opt "I'LL BE THERE." -> wait_year
  opt "I'LL RUN. NOBODY WOULD BLAME ME." -> coward_end

stage wait_year
  goal wait 3
  then @t1
  journal "YOU STRUCK THE GREEN STRANGER'S HEAD OFF, AND HE PICKED IT UP. HE WILL RETURN THE BLOW AT {CHAPEL}. THE DAYS PASS QUICKER THAN THEY SHOULD."

stage journey
  goal goto far
  then castle
  journal "ON THE WAY TO {CHAPEL}, A HOUSE AT {FAR} OFFERS SHELTER FOR THE LAST NIGHTS BEFORE THE GREEN STRANGER'S BLOW."

stage castle
  talk host
  say "STAY! THE CHAPEL IS A SHORT RIDE, AND YOU'LL WANT TO BE RESTED. I'LL HUNT TOMORROW. LET'S PLAY A GAME TOO, A GENTLER ONE: WHATEVER I WIN IN THE WOODS IS YOURS, AND WHATEVER YOU WIN IN MY HOUSE IS MINE. AGREED?"
  opt "AGREED." -> lady_talk

stage lady_talk
  talk lady
  say "MY HUSBAND IS HUNTING. YOU'RE GOING TO MEET THE GREEN ONE TOMORROW. ...TAKE THIS GIRDLE OF GREEN SILK. WEAR IT AND NO BLADE CAN CUT YOU. TELL NOBODY. NOT EVEN HIM. <<plea:love>>"
  opt "I'LL TAKE IT." -> took
  opt "NO. I'LL FACE IT AS I AM." -> declined

stage took
  talk host
  do set girdle 1
  say "BACK FROM THE HUNT! A FINE BOAR, AND IT'S YOURS, BY OUR GAME. AND WHAT DID YOU WIN IN MY HOUSE TODAY, GUEST? FAIR'S FAIR. HAND IT OVER."
  opt "THIS GREEN GIRDLE." -> honest
  opt "NOTHING. A QUIET DAY." -> hid

stage honest
  talk host
  do set told 1
  say "A GIRDLE? FROM MY WIFE? ...KEEP IT, THEN. I DON'T WEAR GREEN, MUCH. ANY MORE. <<surprise>>"
  opt "TO THE CHAPEL, THEN." -> @t2

stage hid
  talk host
  do set hid 1
  say "NOTHING? A QUIET DAY. ...GOOD. SOMETIMES THE QUIET DAYS ARE THE ONES THAT MATTER. SLEEP WELL, GUEST. YOU'LL WANT A STEADY NECK IN THE MORNING. <<doubt>>"
  opt "GOOD NIGHT." -> @t2

stage declined
  talk host
  say "BACK FROM THE HUNT! A FINE BOAR, AND YOURS. AND YOU, GUEST? WHAT DID YOU WIN TODAY? NOTHING? A QUIET DAY. GOOD. SOMETIMES THE QUIET DAYS ARE THE ONES THAT MATTER."
  opt "TO THE CHAPEL, THEN." -> @t2

stage chapel_gate
  talk green
  say "YOU CAME! THEY NEVER COME. KNEEL, THEN, AND BARE YOUR NECK, AND KEEP STILL. IF YOU FLINCH, I'LL CALL YOU A COWARD IN EVERY HALL FROM HERE TO THE SEA."
  opt "[[STRIKE.|I'M READY. STRIKE.]]" -> blow
  opt "WAIT. ONE MOMENT." -> flinch

stage flinch
  talk green
  say "YOU FLINCHED! THE BRAVE ONE FROM {HOME} FLINCHED AT THE WIND OF MY AXE. ...NO SHAME IN IT. EVERYONE DOES. ONCE. KNEEL AGAIN. <<insult>>"
  opt "I WON'T FLINCH AGAIN." -> blow

stage blow
  talk green
  say "ONE FEINT FOR THE FIRST DAY YOU KEPT FAITH. ONE FEINT FOR THE SECOND. AND A NICK FOR THE THIRD. ...YOU KNOW MY FACE NOW, DON'T YOU? I WAS YOUR HOST. I SENT MY WIFE TO TEST YOU. SO, {PLAYER}: THE GIRDLE?"
  opt "I TOLD YOU OF IT." if var told 1 -> clean_end
  opt "I HID IT. I WAS AFRAID." if var girdle 1 -> scar_end
  opt "THERE WAS NO GIRDLE." if novar girdle 1 -> clean_end

stage clean_end
  end success
  say "THE GREEN ONE LAUGHED UNTIL THE CHAPEL STONES SHOOK, AND BOUND THE TINY CUT ON YOUR NECK WITH HIS OWN HOLLY-GREEN SLEEVE. YOU KEPT YOUR WORD WHEN IT WAS COLD AND YOU'D RATHER NOT. THAT WAS ALL THE GAME EVER WAS."
  journal "YOU KEPT FAITH WITH THE GREEN STRANGER AT {CHAPEL} AND WALKED AWAY WITH A SCRATCH AND HIS RESPECT."
  do reward rich
  do remember giver "THEY SAY YOU WENT TO {CHAPEL} AND CAME BACK. WITH YOUR HEAD! I PUT A GREEN CANDLE IN THE WINDOW FOR IT. EVERY YEAR."
  do fact "A TRAVELLER FROM {HOME} STRUCK THE GREEN STRANGER'S HEAD OFF, AND A YEAR LATER KNELT AT {CHAPEL} FOR THE RETURN BLOW, AND KEPT FAITH."
  do mark green_game_kept

stage scar_end
  end success
  say "HE NICKED YOUR NECK, A HAIR'S DEPTH, AND LET IT SCAR. YOU'RE ALIVE, HE SAID, BECAUSE YOU LOVED YOUR LIFE, AND THAT'S NO GREAT SIN. YOU WEAR THE GIRDLE STILL. IT IS SUPPOSED TO REMIND YOU OF SOMETHING."
  journal "YOU SURVIVED THE GREEN STRANGER'S BLOW AT {CHAPEL}, WITH A SCAR FOR THE GIRDLE YOU HID. YOU KNOW WHAT IT MEANS."
  do reward fair
  do remember giver "A SCAR ON YOUR NECK? FROM THE GREEN ONE? AND A GREEN SASH. I WON'T ASK. YOU LOOK LIKE YOU DON'T WANT ME TO."
  do fact "A TRAVELLER FROM {HOME} KEPT THE GREEN STRANGER'S GAME, NEARLY, AND WEARS A GREEN GIRDLE AND A THIN SCAR FOR THE NEARLY."
  do mark wears_the_girdle

stage coward_end
  end fail
  say "THE GREEN ONE PUT HIS HEAD BACK ON HIS SHOULDERS, LIKE A HAT, AND LOOKED AT YOU A LONG TIME, AND THEN HE LEFT WITHOUT SAYING ANYTHING. THAT WAS WORSE THAN LAUGHING."
  journal "YOU STRUCK THE GREEN STRANGER'S BLOW AND WOULD NOT TAKE HIS. {CHAPEL} WAITS FOR A GUEST WHO WILL NOT COME."
  do remember giver "THE ONE WHO STRUCK AND RAN. THEY SAY IT IN EVERY HALL NOW. I DON'T SAY IT. I DON'T NEED TO."
  do fact "SOMEONE OF {HOME} STRUCK THE GREEN STRANGER'S HEAD OFF AND THEN NEVER WENT TO {CHAPEL} FOR THE RETURN BLOW."
)SAGA";

struct Def {
  const char* id;
  const char* name;
  uint32_t themes;
  uint32_t needs;
  uint16_t motives;
  uint32_t twists;
  const char* body;
};

uint16_t M(Motive a) { return motiveBit(a); }

}  // namespace

void addLegend(std::vector<Archetype>& v) {
  const Def defs[] = {
      {"stone_and_anvil", "THE SWORD IN THE STONE", TH_KINGSHIP | TH_PROPHECY | TH_WONDER | TH_PRIDE, N_RUIN | N_KINGDOM | N_CAPITAL,
       (uint16_t)(M(Motive::Duty) | M(Motive::Hope) | M(Motive::Pride)),
       TF_RIVAL | TF_DECEIT | TF_PROPHECY | TF_WONDER | TF_BETRAYAL | TF_MERCY | TF_PRICE | TF_WORLD, kStoneAndAnvil},
      {"unasked_question", "THE CUP AND THE UNASKED QUESTION", TH_FAITH | TH_HUNGER | TH_MERCY | TH_WONDER | TH_REDEMPTION, N_RUIN,
       (uint16_t)(M(Motive::Hope) | M(Motive::Grief) | M(Motive::Devotion) | M(Motive::Shame)),
       TF_PRICE | TF_RIVAL | TF_DECEIT | TF_WONDER | TF_MERCY | TF_PROPHECY | TF_IDENTITY | TF_WORLD, kUnaskedQuestion},
      {"green_wager", "THE GREEN STRANGER'S GAME", TH_COURAGE | TH_TEMPTATION | TH_TRICKERY | TH_WONDER | TH_LOYALTY, N_RUIN | N_VILLAGE,
       (uint16_t)(M(Motive::Pride) | M(Motive::Fear) | M(Motive::Shame) | M(Motive::Duty)),
       TF_PRICE | TF_RIVAL | TF_WONDER | TF_WORLD | TF_MERCY | TF_PROPHECY | TF_BETRAYAL, kGreenWager},
  };
  for (const Def& d : defs) {
    Archetype a;
    a.id = d.id;
    a.name = d.name;
    a.source = Source::Legend;
    a.themes = d.themes;
    a.needs = d.needs;
    a.tier = 2;
    a.motives = d.motives;
    a.twists = d.twists;
    a.body = d.body;
    v.push_back(a);
  }
}

}  // namespace arch
}  // namespace saga
}  // namespace story
