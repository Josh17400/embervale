// M6b "Sagas": the story, mission and campaign GENERATOR (VISION_PLAN 15.20; phase A contracts: 15.21). Lead, phase A.
//
// M4 built the engine (rpg/story/dsl.h: the language; story.h: instances, the caster, the save block, path walking).
// M6b composes NEW scripts for it from five layers, so the world tells thousands of different, well-written tales:
//   1. ARCHETYPES  plot skeletons written as DSL TEMPLATES (the markup below): roles, beats, at least one moral choice
//                  with a lasting consequence, 1-3 twist slots, tags. ARCHETYPES and VOICE lanes: rpg/story/arch/*.cpp.
//   2. CAST        the M4 caster, extended (dsl.h role kinds resident / beast / boss): real residents of the M5 census
//                  (households, friendships, grief, needs), M4 realm rulers / wars / ruins, M6 named uniques and world
//                  bosses, M6 rewards (dsl.h effects reward / secret). COMPOSER lane: caster.cpp, story.cpp.
//   3. TWISTS      reusable turns slotted where an archetype allows (rpg/story/arch/twists.cpp: ARCHETYPES lane).
//   4. VOICE       phrase grammars per culture voice and speaker motive (<<key>> slots), proverbs, oaths, sayings of each
//                  people's faith (rpg/story/voice*.cpp: VOICE lane).
//   5. COMPOSER    archetype + twists + voice -> DSL text -> the existing parser and validator -> a dsl::Script the engine
//                  runs like any other (rpg/story/compose.cpp; repetition guard: guard.cpp. COMPOSER lane). Campaigns
//                  are chained from arcs by rpg/story/chain.cpp (CAMPAIGNS lane).
//
// A composed story is named by its SPEC ID ("saga1~prodigal~giver_lied~~~v2~m3~1a2b3c4d"): the id holds every choice the
// composer made, so the script is regenerated from it (never saved); a running saga's save record is the M4 Instance
// (cast, stage, vars) like any other story. scriptOf() resolves spec ids through script() below.
//
// ---------------------------------------------------------------------------------------------------------------------
// THE TEMPLATE MARKUP (expand(); the composer then prepends `script <id>`, `archetype <name>` and `tier <n>`):
//   A template is DSL text (dsl.h) from `title` on: title, hook, pitch, hint, roles, vars, stages. Additions:
//   [[A|B|C]]      one alternative, chosen by the story's seed. One line only; no nesting; an alternative may hold
//                  placeholders, <<voice>> slots and DSL words ("opt [[\"AYE.\"|\"I WILL.\"]] -> seek" is fine).
//   [[name=A|B]]   the same, remembered as `name`; [[=name]] repeats that choice later (another line, a twist): "NINE
//                  YEARS" in the opening and in the journal stay the same number.
//   <<key>>        a voice phrase (VOICE lane: phrase(); keys: voiceKeys()) in the story's voice and the giver's motive;
//   <<key:motive>> ... with another motive (fear greed grief pride devotion love duty vengeance shame hope). A phrase is
//                  upper case and may only use the placeholders {PLAYER}, {GIVER}, {GIVER.GOD}, {GIVER.PEOPLE}, {HOME}
//                  (every archetype declares the roles giver and home: see the role conventions).
//   %name          a LOCAL name (twists and campaign arcs): becomes <prefix>name (stage, role and var names, placeholders
//                  {%name} / {%name.he}): "t1_" in twist slot 1, "a3_" in campaign arc 3. Archetype bodies have none.
//   @t1 @t2 @t3    (archetypes) a twist slot's entry: where an option or `then` may lead. Declared by a line
//                  `slot t1 -> <stage> [family words]` (the continuation, and optionally the twist families that make
//                  sense at that point of the story: deceit identity rival prophecy mercy price betrayal return wonder
//                  world). A filled slot leads into the twist; an empty one straight to the continuation. The slot line
//                  itself is removed.
//   @out           (twists) back to the slot's continuation.
//   @next          (campaign arcs) on to the next arc's first stage (the finale after the last arc).
//   ?flag <line>   keep the line only when the flag is set; !flag <line> only when it is not. Flags: t1 t2 t3 (slot
//                  filled), the twist ids chosen ("tw_giver_lied"), the motive ("m_grief"), the voice ("v3"), and in a
//                  campaign the arcs' archetype ids ("arc_<id>").
//   Everything else passes through unchanged ({ROLE} placeholders, quotes, comments).
//
// ROLE CONVENTIONS (so twists and voice phrases can rely on them):
//   giver  the first role (always `role giver giver`)          home   the hook settlement (`role home site home`)
//   Twists name what else they need in Twist::roles ("foe:foe/beast victim:person/resident": a role named foe of kind
//   foe or beast, a role named victim of kind person or resident; kind * = any kind).
//   Standard names twists may require: foe (foe or beast or boss), victim (person or resident), kin (resident or person),
//   rival (person or npc), place (a site), ruin (ruin), kingdom (kingdom home), lord (ruler), item (a quest item the
//   archetype gives with `do give "{...}"`: twists that need one say "item:*").
//
// REWARDS stay inside the M6 bands (slow rags to riches): composed stories never write `do gold` above 40 or `do loot`;
// they end with `do reward small|fair|rich|great` (dsl.h): gold and XP from the hook's danger band, and for rich / great a
// piece of gear rolled in the band (great: rare at most). Secrets (`do secret <place>`) are the treasure of the best
// endings.
#pragma once
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>
#include "rpg/story/dsl.h"

class Game;
struct Actor;

namespace story {
class Engine;
namespace saga {

// the composer's output version: inside every spec id. A saved saga of another version is dropped as "forgotten" (its
// journal entry closes, nothing crashes); bump it whenever the same spec would compose a different script.
constexpr int SAGA_GEN_VER = 1;

// ---------------------------------------------------------------- 1. archetypes
enum class Source : uint8_t {
  Scripture,   // the Bible and other scripture (close retellings allowed)
  Myth,        // Greek, Norse, Celtic, Mesopotamian, Persian, Indian, East Asian myth (close retellings allowed)
  Folk,        // folk and fairy tale (close retellings allowed)
  Legend,      // epics and Arthurian legend (close retellings allowed)
  Lewis,       // C.S. Lewis-like: themes and feelings only
  Tolkien,     // J.R.R. Tolkien-like: themes and feelings only
  Maas,        // Sarah J. Maas-like: themes and feelings only
  Gwynne,      // John Gwynne-like: themes and feelings only
  World,       // native to the simulated world (famine, deserters, refugees, guilds, alloys, beasts)
  COUNT
};
const char* sourceName(Source s);   // "SCRIPTURE"...

enum Theme : uint32_t {
  TH_REDEMPTION = 1u << 0, TH_BETRAYAL = 1u << 1, TH_BURDEN = 1u << 2, TH_BARGAIN = 1u << 3, TH_EXILE = 1u << 4,
  TH_JUDGEMENT = 1u << 5, TH_MERCY = 1u << 6, TH_PRIDE = 1u << 7, TH_LOVE = 1u << 8, TH_KINSHIP = 1u << 9,
  TH_KINGSHIP = 1u << 10,   // the return of the king, rightful rule
  TH_SACRIFICE = 1u << 11, TH_TRICKERY = 1u << 12, TH_COURAGE = 1u << 13, TH_VENGEANCE = 1u << 14, TH_FAITH = 1u << 15,
  TH_GREED = 1u << 16, TH_HOMECOMING = 1u << 17, TH_CURSE = 1u << 18, TH_PROPHECY = 1u << 19, TH_WAR = 1u << 20,
  TH_HUNGER = 1u << 21, TH_LOYALTY = 1u << 22, TH_WONDER = 1u << 23, TH_GRIEF = 1u << 24, TH_FREEDOM = 1u << 25,
  TH_TEMPTATION = 1u << 26, TH_HOSPITALITY = 1u << 27, TH_FRIENDSHIP = 1u << 28, TH_DOOM = 1u << 29
};

// world facts an archetype needs (the composer offers it only where the world has them; the caster still decides)
enum Need : uint32_t {
  N_CAVE = 1u << 0, N_CAMP = 1u << 1, N_RUIN = 1u << 2, N_VILLAGE = 1u << 3, N_TOWN = 1u << 4, N_CITY = 1u << 5,
  N_KINGDOM = 1u << 6,      // the hook lies in a kingdom (not the wildlands)
  N_RIVAL = 1u << 7,        // a rival kingdom exists
  N_WAR = 1u << 8,          // the hook's kingdom is at war
  N_FAMINE = 1u << 9,       // famine in or near the hook settlement
  N_GRIEF = 1u << 10,       // a grieving household in the hook settlement (M5 census RF_GRIEVING)
  N_FRIENDS = 1u << 11,     // residents with friendships (M5 ties)
  N_NEEDY = 1u << 12,       // a resident in need (hunger / money)
  N_UNIQUE = 1u << 13,      // an M6 named unique in the region (role beast)
  N_BOSS = 1u << 14,        // the kingdom cell's M6 world boss alive (role boss)
  N_SMITH = 1u << 15,       // a smith in the hook settlement (secrets)
  N_CAPITAL = 1u << 16,     // the hook's kingdom has a capital and a ruler
  N_FALLEN = 1u << 17,      // a ruin with a record of a fallen house / realm
  N_COAST = 1u << 18,       // the sea within reach (M10 deepens it)
  N_EVENT = 1u << 19        // a fresh realm event reached the hook (event hooks)
};

// the speaker's motive (the voice grammar's second axis; also the composer's pick from the giver's life)
enum class Motive : uint8_t { Fear, Greed, Grief, Pride, Devotion, Love, Duty, Vengeance, Shame, Hope, COUNT };
const char* motiveName(Motive m);          // "fear"...
int motiveWord(const std::string& w);      // Motive or -1
inline uint16_t motiveBit(Motive m) { return (uint16_t)(1u << (unsigned)m); }

// the twist families (an archetype lists those that fit its slots)
enum TwistFamily : uint32_t {
  TF_DECEIT = 1u << 0,      // the giver lied; the reward is a trap
  TF_IDENTITY = 1u << 1,    // the monster was a cursed person; the heir is someone you met; the stranger is kin
  TF_RIVAL = 1u << 2,       // a rival wants the same thing (and maybe the same good)
  TF_PROPHECY = 1u << 3,    // the prophecy meant someone else; the omen misread
  TF_MERCY = 1u << 4,       // the foe begs, explains, or was wronged first
  TF_PRICE = 1u << 5,       // success costs something unlooked for
  TF_BETRAYAL = 1u << 6,    // an ally turns; a friend sells you
  TF_RETURN = 1u << 7,      // someone thought lost comes back
  TF_WONDER = 1u << 8,      // the uncanny intrudes (a sign, a ghost, a talking beast)
  TF_WORLD = 1u << 9        // the living world intervenes (a war, a famine, a raid changes the errand)
};

struct Archetype {
  const char* id = "";           // unique, lower case [a-z0-9_] ("prodigal")
  const char* name = "";         // the archetype's name ("THE PRODIGAL'S RETURN"; reports, the DSL `archetype` line)
  Source source = Source::Folk;
  uint32_t themes = 0;           // Theme bits
  uint32_t needs = 0;            // Need bits
  uint8_t tier = 2;              // 2: a story quest; 3: a campaign arc (chain.cpp uses it; never offered alone)
  uint16_t motives = 0;          // the giver's motives that fit (motiveBit; 0: any)
  uint32_t twists = 0;           // TwistFamily bits its slots accept (0: it has no slots)
  const char* body = "";         // the template (the markup above)
};
// (ARCHETYPES + VOICE lanes: rpg/story/arch/index.cpp gathers every file's table) every archetype, library order
const std::vector<Archetype>& archetypes();
const Archetype* archetype(const std::string& id);

// ---------------------------------------------------------------- 3. twists
struct Twist {
  const char* id = "";           // unique, lower case ("giver_lied")
  const char* name = "";         // "THE GIVER LIED"
  uint32_t family = 0;           // one TwistFamily bit
  uint32_t needs = 0;            // Need bits (beyond the archetype's)
  const char* roles = "";        // roles the archetype must declare: "name:kind" words, kind a DSL role kind or * (any)
  const char* excludes = "";     // twist ids it never shares a story with (space separated)
  const char* body = "";         // template: its own `role %x` / `var %x` lines, then `%stage`s; exits through @out
};
// (ARCHETYPES lane: rpg/story/arch/twists.cpp)
const std::vector<Twist>& twists();
const Twist* twist(const std::string& id);
// may this twist fill a slot of this archetype? (family, roles, needs, excludes against the others chosen)
bool twistFits(const Archetype& a, const Twist& t, const std::vector<std::string>& chosen);
// ... and in slot `slot` (0..2: t1..t3) in particular: a slot line may name the families it takes
// (`slot t2 -> confront mercy price`); a slot that names none takes any of the archetype's families
bool twistFitsSlot(const Archetype& a, const Twist& t, int slot, const std::vector<std::string>& chosen);
uint32_t slotFamilies(const std::string& body, int slot);       // the TwistFamily bits a slot line names (0 none)
uint32_t twistFamilyWord(const std::string& w);                 // "deceit" .. "world" -> its bit (0 unknown)

// ---------------------------------------------------------------- 4. voice
struct VoiceCtx {
  int voice = 2;                 // the culture's voice family 0..7 (rumours.cpp voiceOf: 0 plain, 1 fjord/highland,
                                 // 2 heartland/imperial, 3 dune/sun-temple, 4 steppe, 5 marsh/river, 6 jade, 7 sylvan/star)
  Motive motive = Motive::Duty;  // the speaker's
  uint64_t seed = 0;             // the choice within the grammar (the composer advances it per slot)
};
struct Phrase {
  std::string text;              // upper case, at most 60 characters (the 300-character line budget); "" unknown key
  uint32_t family = 0;           // the phrase family (the repetition guard counts them); 0 none
};
// (VOICE lane: rpg/story/voice*.cpp) a phrase for a key. THE KEY VOCABULARY (fixed in phase A so templates can be
// written against it; the VOICE lane may add keys, never remove these 24):
//   greet     to a stranger, opening a talk         welcome   to someone known, returning     plea      asking for help
//   urgency   time is short                         thanks    gratitude                        farewell  parting words
//   blessing  wishing well (may name the god)       oath      swearing by something sacred    vow       promising to act
//   proverb   folk wisdom of this people            saying    scripture-like line of the faith curse     wishing ill
//   threat    menace                                insult    contempt                         comfort   consoling
//   grief     mourning                              doubt     suspicion / disbelief            warning   caution
//   apology   contrition                            boast     pride                            refusal   saying no
//   surprise  astonishment                          relief    a burden lifted                  omen      a sign seen
Phrase phrase(const std::string& key, const VoiceCtx& v);
// every key the voice knows (the library lint checks every <<key>> of every template against it)
const std::vector<std::string>& voiceKeys();

// ---------------------------------------------------------------- 5. the composer
// Everything the composer chose for one story. Its spec id (specId) is the story's script id.
struct Spec {
  bool campaign = false;         // a campaign (chain.cpp: `arch` is the campaign id, `arcs` its arc choices)
  std::string arch;              // archetype id (or campaign id)
  std::string twist[3];          // the twist in each slot ("" empty)
  std::vector<std::string> arcs; // (campaigns) the archetype chosen for each arc slot
  int voice = 2;                 // 0..7
  Motive motive = Motive::Duty;
  uint32_t seed = 0;             // alternatives, phrases
  int ver = SAGA_GEN_VER;
};
// "saga<ver>~<arch>~<t1>~<t2>~<t3>~v<voice>~m<motive>~<seed hex>"; campaigns "saga<ver>~@<id>~<arc>.<arc>...~v..~m..~.."
std::string specId(const Spec& s);
bool parseSpecId(const std::string& id, Spec& out);   // false: not a saga id, or malformed
inline bool isSagaId(const std::string& id) { return id.compare(0, 4, "saga") == 0; }

// The template expander (compose.cpp; chain.cpp uses it for arcs).
struct Expand {
  uint64_t rng = 0;                              // advanced once per [[...]] and <<...>> (explicitly sequenced)
  int voice = 2;
  Motive motive = Motive::Duty;
  std::string prefix;                            // what %name becomes: prefix + name
  std::map<std::string, std::string> targets;    // "@t1" / "@out" / "@next" -> stage name
  std::set<std::string> flags;                   // ?flag / !flag
  std::map<std::string, std::string> picks;      // [[name=...]] choices, for [[=name]] (twists inherit the archetype's)
  std::vector<uint32_t> families;                // (out) phrase families used
  std::vector<std::string> errors;               // (out) unknown voice keys, unclosed markup, unknown @targets
};
std::string expand(const std::string& body, Expand& x);
// `slot t1 -> <stage>` lines of a template: slot name -> continuation stage
std::map<std::string, std::string> slotsOf(const std::string& body);

struct Composed {
  std::string text;              // the complete DSL text (script line first)
  std::vector<std::string> errors;   // expansion, parse and validation problems (empty: a valid script)
  std::vector<uint32_t> families;    // phrase families used
  int words = 0;                 // words the player can read
};
// compose a tier-2 story (compose.cpp) or a campaign (chain.cpp: composeCampaign). Pure functions of the spec.
bool compose(const Spec& s, Composed& out);
bool composeCampaign(const Spec& s, Composed& out);
// the parsed, linked and validated script of a spec id (composed on first use and cached); nullptr: not a saga id, an
// unknown archetype, another SAGA_GEN_VER or an invalid composition. Pointers stay valid until trimCache drops the entry.
const dsl::Script* script(const std::string& id);
// drop cached scripts no running instance of `e` uses (Engine::step calls it now and then; tests between stories)
void trimCache(const Engine& e);
size_t cacheSize();

// ---------------------------------------------------------------- choosing stories from the world (COMPOSER lane)
// Where a story is being offered: an NPC (hookActor: actors id) or a place (board, herald, ruin, event) in settlement /
// ruin handle hookSite.
struct Hook {
  int kind = 0;                  // dsl::HookKind
  int actor = -1;                // the speaker (npc, herald), actors id
  int site = -1;                 // the hook place's handle
  int ev = -1;                   // event hooks: realm::EvType
};
// pick a spec for this hook from the world (needs, the giver's life and motive, the repetition guard); deterministic
// in (world seed, the hook entity, the day bucket, the guard). false: nothing fits here.
bool pick(Game& g, const Hook& h, Spec& out);
// (CAMPAIGNS lane: chain.cpp) a campaign this hook may begin (boards, heralds, ruins, realm events); false none
bool pickCampaign(Game& g, const Hook& h, Spec& out);
// the generated story an NPC would offer now ("" none) and the one a place would (story_game.cpp / places.cpp ask these
// after the library's own scripts)
std::string offerFor(Game& g, const Actor& a);
std::string offerForPlace(Game& g, int hookKind, int site);
// (COMPOSER lane) the world facts at a hook: which of `needs` (Need bits) do NOT hold there (0: all hold). pick()'s own
// cheap look (the caster still decides); tests choose hook places by it. needName: "CAVE", "WAR"... (reports)
uint32_t needsMissing(Game& g, const Hook& h, uint32_t needs);
// (fixer M6b r2) the world facts of hooks at settlement `site` worked out a little at a time (one fact a
// call) so the frame that composes the first offer there does not pay for them all; true: all known today
bool warmHook(Game& g, int site);
// (fixer M6b r2) the composer's one-time tables (archetype and twist bodies, voice grammars) built ahead of play
void warmComposer();
const char* needName(uint32_t bit);
// (COMPOSER lane) the repetition guard's bookkeeping, called where the game shows and starts generated stories:
// the hook's guard hash and global tile (0: not a hook pick() could use)
uint32_t hookHashOf(Game& g, const Hook& h, int32_t& gx, int32_t& gy);
// an offer was shown at this hook (story_game.cpp storyTalk, places.cpp boards and heralds)
void noteOffered(Game& g, const Hook& h, const std::string& specId);
// a generated story was begun (instance `inst`) from this hook: the guard notes archetype, twists, cast shape and phrase
// families; a place (board, herald, ruin, event) tells no other generated story for a fortnight
void noteTold(Game& g, const std::string& specId, uint32_t inst, const Hook& h);

// ---------------------------------------------------------------- the repetition guard (saved in the story block)
// What the player has been offered and played, so the composer steers away from the same archetype, twist and phrase
// families in the same region and recent days (15.20: "no two stories within 30 km share archetype + twist").
struct Seen {
  uint32_t arch = 0;             // strHash of the archetype (or campaign) id
  uint32_t twists = 0;           // strHash of the twist ids, joined
  uint32_t shape = 0;            // the cast shape (role kinds and trades) hash
  int32_t day = 0;
  int32_t gx = 0, gy = 0;        // the hook's global tile
  // (COMPOSER lane, story block v4) who offered it (a hash of the hook: the giver's npcKey, the place's id) and whether
  // it was only offered (shown, not taken: steers other hooks away for GUARD_DAYS; the same hook may offer it again)
  uint32_t hook = 0;
  uint8_t offered = 0;
};
struct Guard {
  std::vector<Seen> seen;                    // oldest first; at most GUARD_SEEN
  std::map<uint32_t, uint16_t> phrases;      // phrase family -> times heard (at most GUARD_PHRASES families)
  static constexpr size_t GUARD_SEEN = 512, GUARD_PHRASES = 2048;
  // "30 km" of 15.20 is one kingdom cell (VISION_PLAN 2.1: 1024 tiles, ~3.7 minutes' walk); "recent" is 60 days
  static constexpr int32_t GUARD_RADIUS = 1024;
  static constexpr int GUARD_DAYS = 60;
  // (COMPOSER lane: guard.cpp)
  void note(const Spec& s, uint32_t shape, int day, int32_t gx, int32_t gy, const std::vector<uint32_t>& families);
  // how strongly to avoid this choice here and now (0 none .. 1000 forbidden: the same archetype + twists within
  // GUARD_RADIUS ever; the same archetype within GUARD_RADIUS and GUARD_DAYS strongly)
  int penalty(const Spec& s, int day, int32_t gx, int32_t gy) const;
  int phrasePenalty(uint32_t family) const;
  // (COMPOSER lane) an offer shown to the player (not yet taken): other hooks within GUARD_RADIUS never offer the same
  // archetype + twists for GUARD_DAYS; the same hook keeps its offer (penalty(..., hook) ignores its own offers)
  void noteOffer(const Spec& s, uint32_t hook, int day, int32_t gx, int32_t gy);
  int penalty(const Spec& s, int day, int32_t gx, int32_t gy, uint32_t hook) const;
  // the archetype alone (any twists): how strongly to avoid it here and now (0 .. 700; offers count too)
  int archPenalty(const std::string& arch, bool campaign, int day, int32_t gx, int32_t gy, uint32_t hook) const;
  void clear() { seen.clear(); phrases.clear(); }
};

// ---------------------------------------------------------------- reports (tests, the writing review)
struct LibraryReport {
  int archetypes = 0, tier2 = 0, arcs = 0, twists = 0, voiceKeys = 0, campaigns = 0;
  int bySource[(int)Source::COUNT] = {};
};
LibraryReport report();

// ---------------------------------------------------------------- campaigns (CAMPAIGNS lane: chain.cpp, campaigns/*.cpp)
struct ArcSlot {
  std::vector<std::string> choices;   // tier-3 archetype ids (or "" for an optional arc) the chainer picks from
};
struct CampaignPlan {
  const char* id = "";           // "burden"
  const char* name = "";         // "THE BURDEN ACROSS KINGDOMS"
  Source source = Source::World;
  uint32_t themes = 0;
  uint32_t needs = 0;
  const char* head = "";         // template: title, hook, pitch, hint, the recurring roles and vars, the opening stages
                                 // (ending in @next)
  std::vector<ArcSlot> arcs;     // 4..8 arcs (each arc's stages end in @next)
  const char* finale = "";       // template: the world-changing endings
};
const std::vector<CampaignPlan>& campaignPlans();
const CampaignPlan* campaignPlan(const std::string& id);

}  // namespace saga
}  // namespace story
