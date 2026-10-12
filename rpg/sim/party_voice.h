// M8 Fellowship, the VOICE lane's own declarations (rpg/sim/party.h holds the shared contract; this header is VOICE's
// alone). What companions and the people round them say is built from the living world: the census (their trade, home,
// household, friends), the culture engine (their people's ways, faith and rites), the realm (their homeland's wars) and
// the deeds they remember. Every line meets the M7b speech bar (rpg/sim/talk.h lint): first person, names introduced,
// whole sentences, the phone panel's fit.
//
// Files: party_voice.cpp (the facts and the word tables), banter.cpp (party::banterStep / deedLine / cryLine / epitaph /
// aboutLine), party_talk.cpp (the conversations: recruiting, a companion's own talk, the kin, the grave),
// rpg/story/arch/companion.cpp (the personal quests' archetypes) and rpg/story/companion_quest.cpp (choosing one).
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "rpg/sim/party.h"
#include "rpg/world/ids.h"

class Game;

struct Actor;
struct Dialogue;

// Game's friend for the companions' talk (game.h: friend struct PartyTalkOps). Defined in rpg/sim/party_talk.cpp.
struct PartyTalkOps {
  // the member a census person's street body is (at home, waiting, left), -1 none
  static int memberOfResident(const Game& g, const Actor& a);
  // a companion's first page (opening: the conversation begins; else back from a page) and the orders page
  static void front(Game& g, int mi, bool opening);
  static void orders(Game& g, int mi);
  // their personal quest ready to offer (spec: its saga id)
  static bool questReady(Game& g, int mi, std::string& spec);
  // (tests, scripts) the grave of party.fallen[fi] spoken to, as partyInteract opens it at the stone
  static void graveTalk(Game& g, int fi);
};

namespace party {

// a companion as the giver of their own personal quest (story::Binding::id of its giver role; stable across saves)
inline uint64_t companionKey(uint32_t memberId) { return ew::mix64(0xC0A1D5EE00000000ull ^ ((uint64_t)memberId * 0x9E3779B97F4A7C15ull)); }

namespace voice {

// ---------------------------------------------------------------- who they are (read fresh from the world)
struct Kin {
  std::string name;            // "ASTRID" ("" none)
  std::string word;            // what they are to the member: "WIFE", "SON", "MOTHER", "BROTHER"
  bool female = false, dead = false, away = false;
  int idx = -1;                // census index (-1 none)
};
struct Bio {
  std::string name;            // the member's own name
  bool female = false;
  int age = 30;
  int job = 0;                 // life::Job
  std::string trade;           // "SMITH" (the job's word; "" none)
  std::string home;            // their home settlement's name ("" unknown)
  int homeType = -1;           // SiteType of home (-1 unknown)
  std::string people;          // their culture's adjective ("VETHMARKI"; "" none)
  std::string land;            // their culture's name ("VETHMARK")
  int arch = 2;                // cult::Archetype
  std::string god;             // the first god of their faith ("" none)
  int faith = 0;               // Religion::kind
  int burial = 0;              // Religion::burial
  int drink = 0;               // Customs::drink
  int staple = 0;              // Customs::staple[0]
  std::string homeland;        // the kingdom of their home ("" none)
  ew::Gid homelandId = 0;
  std::vector<Kin> household;  // spouse, children, parents, siblings (census order: spouse first)
  Kin friendOf;                // their closest friend in town
  Kin stakeKin;                // the stake's kin (Stake::kin), when a census person
  std::string stakeSite;       // the stake's place (Stake::site) by name
  std::string stakeRuin;       // the stake's ruin by name
  std::string stakeKingdom;    // the stake's kingdom by name
};
Bio bio(const Game& g, const Member& m);

// ---------------------------------------------------------------- words
const char* jobWord(int job);                       // "SMITH", "FARMER" ("" for none)
std::string article(const std::string& noun);       // "A SMITH", "AN INNKEEPER"
const char* drinkWord(int drink);                   // "ALE", "MEAD", "WINE", "TEA", "KUMIS", "NECTAR"
const char* stapleWord(int staple);                 // "BREAD", "FISH", "RICE"...
const char* houseWords(int arch);                   // how a people builds ("TURF ROOFS AND LONGHOUSES")
const char* landWords(int arch);                    // the country they come from ("THE FJORDS")
const char* riteWords(int burial);                  // the rite ("BURIED", "UNDER A CAIRN", "ON A PYRE"...)
std::string ago(int days);                          // "TWO DAYS AGO", "LAST WEEK", "A MONTH AGO"
// a member's name as others say it (their first word)
std::string given(const std::string& name);

// ---------------------------------------------------------------- lines the FELLOWS lane's sellers and hirelings say
// (species 0 hound, 1 wolf) a farmer selling a pup, at `price` gold; a den keeper's wolf pup (price 0: found, not sold)
std::string pupPitch(const Game& g, int species, int price);
// the pup taken: what the seller says as it goes with the player
std::string pupTaken(const Game& g, int species, const std::string& pupName);
// a mercenary's terms in their own voice (no arc): wage gold a day
std::string mercTerms(const Game& g, const std::string& who, int wage);

// ---------------------------------------------------------------- the personal quest (rpg/story/companion_quest.cpp)
// the saga spec id of member m's personal quest, composed from their stake through the M6b composer and cast from their
// real history (the companion as giver, their home, their kin, foe, kingdom, ruin); "" when none fits. Deterministic.
std::string questFor(Game& g, const Member& m);
// begin it (the member must be in play: their actor is the giver). Returns the story instance id (0: it could not be
// cast); sets Member::quest / questInst / questState / MF_QUEST_OFFERED
uint32_t questBegin(Game& g, int member);
// Member::questState (VOICE): 0 not offered, 1 offered and put off, 2 running, 3 ended well, 4 ended badly. questStatus
// brings it up to date from the story engine and returns it
int questStatus(Game& g, Member& m);

// ---------------------------------------------------------------- banter (rpg/sim/banter.cpp)
// The triggers of party::banterStep (VISION_PLAN 9), in the order it weighs them
enum class Trig : uint8_t {
  Border,      // into the land of another people (arg: that culture id)
  BorderHome,  // back into their own people's land
  Home,        // into their home settlement
  StakeSite,   // into the place their stake happened (a burned village, where the kin was last seen)
  StakeRuin,   // near the ruin of their stake
  Wonder,      // a wonder found (name: its name)
  HardFight,   // after a fight that nearly went wrong (arg: 1 they were downed in it)
  LowHealth,   // the player is badly hurt in a fight
  War,         // news of a war involving their homeland (arg: the other kingdom's id)
  Praise,      // approval rose past APPROVAL_QUEST
  Trust,       // bond rose past SKILL_BOND[1]
  Rain, Snow, Heat, Sand, Fog, Ash, Eerie,   // the weather on screen (the view's own rule: render.cpp)
  Night,       // nightfall on the road
  Musing,      // a quiet moment: home, the ones left behind, their trade, their stake
  Pair,        // two people with the player talk to each other (from LEADERSHIP_LEVEL); see exchange()
  Hound,       // someone speaks of (or to) the bonded animal
  COUNT
};
const char* trigName(Trig t);
// the line member mi would say for this trigger now (arg / name: see Trig; salt: the variety): "" when the trigger has
// nothing for them. Pure: no state is changed (tests call it for every culture pair)
std::string banterLine(const Game& g, int mi, Trig t, uint64_t arg, const std::string& name, uint32_t salt);
// two people with the player: a's line to b and b's answer (false: nothing to say)
bool exchange(const Game& g, int a, int b, uint32_t salt, std::string& lineA, std::string& lineB);
// what the weather on screen is now at the player (the view's rule): a Trig of the weather kinds, or Trig::COUNT
Trig weatherNow(const Game& g);
// (tests) how often banterStep said a line of each trigger since the counters were reset
int banterCount(Trig t);
void banterResetCounts();

// the talk's shared sentences (banter.cpp): their trade at home, who they left behind (names introduced by what they
// are: "MY WIFE ASTRID"), what drives them (the stake)
std::string tradeLine(const Bio& b);
std::string kinLine(const Bio& b);
std::string stakeLine(const Game& g, const Member& m, const Bio& b);

// Member::talkBits (VOICE): what has been talked through
constexpr uint32_t TB_ABOUT = 1u << 0;        // told their story
constexpr uint32_t TB_HOME = 1u << 1;         // talked of home
constexpr uint32_t TB_KIN = 1u << 2;          // talked of the ones left behind
constexpr uint32_t TB_STAKE = 1u << 3;        // talked of what drives them
constexpr uint32_t TB_REBUFF = 1u << 4;       // (Left) turned the player away once: an apology may follow another day
constexpr uint32_t TB_QUEST_ASKED = 1u << 5;  // the quest's offer was put off once
constexpr uint32_t TB_PRAISE = 1u << 6;       // the approval threshold lines said (50)
constexpr uint32_t TB_DOUBT = 1u << 7;        // ... (-40)
constexpr uint32_t TB_INTRO = 1u << 8;        // introduced themselves (the recruit's first words)
constexpr uint32_t TB_KINASK = 1u << 9;       // their kin asked the player to look after them
constexpr uint32_t TB_WAR0 = 1u << 10;        // bits 10..12: their homeland's wars already remarked (by war id)
constexpr uint32_t TB_TRUST = 1u << 13;       // the trust line said (bond SKILL_BOND[1])
constexpr uint32_t TB_AMENDS = 1u << 14;      // (Left) the apology was made and taken
constexpr uint32_t TB_RUIN = 1u << 15;        // the stake's ruin seen and remarked
constexpr uint32_t TB_BORDER0 = 1u << 16;     // bits 16..31: culture borders already remarked (by culture hash)

}  // namespace voice
}  // namespace party
