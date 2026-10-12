// M7b "Cities & Voice", the VOICE half (VISION_PLAN 15.23, 15.24): the shared contract for natural dialogue.
// TALK lane is its only editor after phase A (it may ADD; never rename, remove or change the meaning of what is listed);
// the SAGA lane (rpg/story/**) uses it read-only.
//
//  1. The phone panel: how much a spoken line may hold (the narrowest phone layout of rpg/view/hud.cpp dlgLay).
//  2. One line at a time: every talk hook PROPOSES its line with a priority and a topic (propose); the line with the
//     highest priority wins, so a conversation opens on the one thing that matters most to this speaker now instead of
//     whichever hook ran last overwriting (or stitching onto) the others. Direct writes to Game::dlg.text still work.
//  3. Memory (saved, the talk block, SAVE_VER 15): who the player has met and when, whether they have introduced
//     themselves (a name is introduced before it is relied on), the last thing each said, and the lines said lately
//     (no one repeats the same bark to the player day after day).
//  4. The recorder (tests and the dialogue review only, never saved): every conversation as the player sees it, turn
//     by turn (the speaker's line, the options, the option chosen).
//  5. The lint: coherence checks on a recorded conversation (placeholders, case, panel fit, repeated phrases, doubled
//     words, stitched sentences, fragments, unintroduced names, an answer that ignores the option chosen). rpg_test
//     --dialogue, --sagas and --voice run it; the dialogue-quality review reads formatPhone() dumps.
#pragma once
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

struct Dialogue;   // rpg/sim/game.h

namespace speech {

// ---------------------------------------------------------------- 1. the phone panel
// rpg/view/hud.cpp dlgLay on the 480-px design width without the portrait: (480 - 40 - 20) / 6 = 70 characters a speech
// line, (480 - 40 - 40) / 6 = 66 an option row. A line should read in at most PANEL_LINES rows (about 300 characters:
// two or three full sentences); an option should fit one row (two at most).
constexpr int PANEL_CHARS = 70;
constexpr int PANEL_LINES = 5;
constexpr int OPTION_CHARS = 66;
std::vector<std::string> wrapLines(const std::string& s, int perLine);   // the HUD's greedy word wrap ('\n' breaks)
inline bool fitsPanel(const std::string& text) { return wrapLines(text, PANEL_CHARS).size() <= (size_t)PANEL_LINES; }
inline bool fitsOption(const std::string& label) { return wrapLines(label, OPTION_CHARS).size() <= 2; }
// (TALK lane) the first page of a conversation shows at most FIRST_PAGE options before the services are grouped under
// one header ("WHAT CAN YOU OFFER?")
constexpr int FIRST_PAGE = 6;
// a + " " + b when that still fits the panel, else b (a lead-in that gives way to the line that matters)
std::string joinFit(const std::string& a, const std::string& b);
// small counts in words ("SIX CUTS OF MEAT"): ONE..TWELVE, then digits
std::string countWord(int n);
// a speaker's own name without the title it is shown with ("FIRST SPEAKER QUAVION" -> "QUAVION")
std::string givenName(const std::string& shownName);

// ---------------------------------------------------------------- 2. one line at a time
enum class Topic : uint8_t {
  None, Greeting, Intro, Smalltalk, Bark, Trade, Service, Need, Rumour, News, War, Story, QuestOffer, QuestActive,
  QuestDone, Reward, Warning, Home, Craft, Foe,
  Companion,   // (M8) a companion speaks, or someone speaks of the party (recruiting, kin, the fallen)
  COUNT
};
const char* topicName(Topic t);
// priorities (higher wins; ties: the later proposal wins, like the old overwrite order)
enum Pri : int {
  P_NONE = 0,
  P_GREETING = 10,   // "GOOD MORNING." and the like
  P_SMALLTALK = 20,  // weather, the land, the realm in general
  P_BARK = 25,       // a mood or a need said in passing
  P_SERVICE = 30,    // what a shopkeeper / innkeeper / smith / stablemaster offers
  P_NEWS = 40,       // the realm's news, rumours, gossip
  P_NEED = 45,       // the speaker's own pressing need (hunger, a debt)
  P_STORY = 60,      // a story's hook or its next step from this speaker
  P_QUEST = 70,      // the speaker's own job: offered, under way
  P_REWARD = 90,     // a finished job's reward waits with them
  P_URGENT = 100     // danger now (a raid, a siege, the speaker is fleeing)
};
// The line replaces Dialogue::text when pri >= Dialogue::pri (then Dialogue::pri / topic follow); false: kept the old.
bool propose(Dialogue& d, int pri, Topic t, const std::string& text);

// ---------------------------------------------------------------- 3. memory (saved: the talk block)
constexpr uint8_t TM_INTRODUCED = 1;    // they told the player who they are (name, trade), or the player was told
constexpr uint8_t TM_KNOWS_PLAYER = 2;  // they know the player's name
constexpr uint8_t TM_THANKED = 4;       // they owe the player thanks (a job done, a gift)
constexpr uint8_t TM_SLIGHTED = 8;      // the player wronged them (theft, a broken promise)
struct Met {
  uint64_t key = 0;                     // Game::npcKey (stable across sessions)
  int32_t first = 0, last = 0;          // the days of the first and the latest conversation
  uint16_t times = 0;                   // conversations so far (saturates)
  uint8_t flags = 0;                    // TM_*
  uint8_t lastTopic = 0;                // Topic of the last line they opened with
  uint32_t lastLine = 0;                // lineHash of that line
};
struct Memory {
  static constexpr int MAX_MET = 1024;  // the least recently met are forgotten first
  static constexpr int RECENT = 64;     // lines remembered for saidRecently
  static constexpr uint8_t VERSION = 1;
  std::vector<Met> met;                 // sorted by key
  std::vector<std::pair<uint32_t, int32_t>> recent;   // (lineHash, day), oldest first
  const Met* find(uint64_t key) const;
  Met& meet(uint64_t key, int day);     // add (or touch) a record: last = day, times + 1
  bool introduced(uint64_t key) const { const Met* m = find(key); return m && (m->flags & TM_INTRODUCED); }
  void flag(uint64_t key, uint8_t f, int day);        // set flags on (adding the record if needed)
  bool saidRecently(uint32_t lineH, int day, int withinDays) const;
  void said(uint32_t lineH, int day);
  void clear() { met.clear(); recent.clear(); }
  void serialize(std::vector<uint8_t>& out) const;    // its own version byte first
  bool deserialize(const std::vector<uint8_t>& in);   // false: damaged or another version (the save is refused)
  bool operator==(const Memory& o) const;
};
uint32_t lineHash(const std::string& s);

// ---------------------------------------------------------------- 4. the recorder
struct Turn {
  std::string speaker, text;            // who speaks and what the panel shows
  std::vector<std::string> opts;        // the options offered with it
  int chosen = -1;                      // the option the player took next (-1: none, the conversation ended here)
};
struct Conversation {
  std::string who, role, where;         // the speaker, their trade (roleName), the place (site name or "")
  uint64_t key = 0;                     // npcKey (0: a board, a grave, a herald's proclamation...)
  std::vector<Turn> turns;
};
struct Recorder {
  std::vector<Conversation> convs;
  size_t maxConvs = 20000;
  // talkTo opened a conversation (a new Conversation with its first turn)
  void begin(const Dialogue& d, const std::string& who, const std::string& role, const std::string& where, uint64_t key);
  // the player chose option `chosen` of the last turn; `open`: the dialogue is still up, d its new state (a new turn)
  void reply(int chosen, const Dialogue& d, bool open);
  // (TALK lane) the last turn's shown options as indices into Dialogue::opts, and every label it had
  std::vector<int> shownIdx_;
  std::vector<std::string> labels_;
};

// ---------------------------------------------------------------- 5. the lint
enum Check : uint32_t {
  C_EMPTY = 1u << 0,        // an empty line or option
  C_PLACEHOLDER = 1u << 1,  // a template left unfilled: { } % < > _ # @ "NULL" "(NONE)"
  C_CASE = 1u << 2,         // lower-case letters (the HUD font is upper case)
  C_PANEL = 1u << 3,        // the line runs past PANEL_LINES rows of PANEL_CHARS
  C_OPTION = 1u << 4,       // an option longer than two rows
  C_REPEAT = 1u << 5,       // the same 4-word phrase twice in one conversation (the speaker's lines)
  C_DOUBLE = 1u << 6,       // a doubled word ("THE THE") or broken punctuation (" ,", "..", "?.", ",.")
  C_STITCH = 1u << 7,       // fragments glued together: a sentence starting AND / OR after a full stop, a
                            // bracketed journal note inside speech, a dangling ':' at the end
  C_FRAGMENT = 1u << 8,     // a one-word sentence that is not an interjection
  C_NAME = 1u << 9,         // a name (LintCtx::names) used before it was introduced
  C_ECHO = 1u << 10,        // the answer to a chosen option is the line it answered (the choice changed nothing)
  C_PUNCT = 1u << 11,       // a line that does not end its last sentence (. ! ? ... or a closing quote)
  C_SELF = 1u << 12,        // the speaker named in the third person in their own line ("KEEP EVIELIEL ALIVE" said by
                            // EVIELIEL), unless they say who they are ("I'M EVIELIEL", "MY NAME IS ...", "CALL ME ...")
  C_ALL = 0x1FFFu,
  // (TALK lane additions; not in C_ALL, so a LintCtx left at its default runs the phase A set)
  C_ANSWER = 1u << 13,      // the answer to a chosen option has nothing to do with it (no content word, synonym or
                            // expected kind of reply in common: "BUY A RUMOUR" answered with a recipe)
  C_EVERY = 0x3FFFu         // every check, the additions included
};
const char* checkName(uint32_t c);
struct Issue {
  uint32_t check = 0;
  int turn = -1;            // the turn index (-1: the conversation as a whole)
  std::string what;         // the offending text, short
};
struct LintCtx {
  // proper names the conversation may rely on only once introduced (a name is introduced by its first mention when
  // that mention is the speaker's own name, or comes with who they are: "MY BROTHER ARN", "ARN THE SMITH", "ARN, MY
  // NEIGHBOUR", "A MAN CALLED ARN", "I'M ARN" ... see talk.cpp introducedAt)
  std::vector<std::string> names;
  std::vector<std::string> known;       // names the player already knows (met, or introduced earlier)
  uint32_t checks = C_ALL;              // which checks run
};
// (TALK lane) does `reply` answer `option`? (the C_ANSWER relation: a shared content word or synonym, or an option that
// asks for no particular answer: a farewell, a yes, a commitment)
bool answers(const std::string& option, const std::string& reply);
std::vector<Issue> lint(const Conversation& c, const LintCtx& ctx = LintCtx());
// the conversation as the phone panel shows it (the speaker, each line wrapped to PANEL_CHARS, the options, "> " on the
// one chosen), for review dumps
std::string formatPhone(const Conversation& c);

}  // namespace speech
