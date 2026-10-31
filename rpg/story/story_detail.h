// SOLID 1.3 STORY: the private seam of the split story engine (story.cpp + story_effects/text/save.cpp) and of
// the story hooks (story_game.cpp + story_actors/ai/combat/debug.cpp): their shared inline helpers and the few
// functions one file defines for another. Included only by those files; the lane-wide words are story_internal.h.
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "engine/audio.h"
#include "rpg/sim/game.h"
#include "rpg/sim/game_internal.h"
#include "rpg/sim/party.h"
#include "rpg/sim/party_voice.h"
#include "rpg/story/dsl.h"
#include "rpg/story/story_internal.h"

namespace story {
namespace {
using dsl::CondType;
using dsl::EffType;
using dsl::ObjType;
using dsl::RoleKind;
using dsl::StageKind;

inline uint64_t strHash(const std::string& s) {
  uint64_t h = 1469598103934665603ull;
  for (char c : s) { h ^= (uint8_t)c; h *= 1099511628211ull; }
  return h;
}

inline bool hasItem(const Game& g, const std::string& name) {
  for (const Item& it : g.inv) if (it.kind == ItemKind::Quest && it.name == name) return true;
  return false;
}

inline void sound(Game& g, Sfx s, float pitch = 1.0f, float vol = 1.0f) {
  if (host().sound) host().sound(g, (int)s, pitch, vol);
}

// (M8) the party member whose own story this is (its giver: party::companionKey), -1 none
inline int companionOf(const Game& g, const Instance& in) {
  if (in.cast.empty()) return -1;
  for (size_t i = 0; i < g.party.members.size(); i++) if (party::companionKey(g.party.members[i].id) == in.cast[0].id) return (int)i;
  return -1;
}
}  // namespace

// defined in story_effects.cpp, used by story.cpp / story_text.cpp
bool evalCond(const Game& g, const Instance& in, const dsl::CondDef& c);
bool noteRealmSite(Game& g, ew::Gid site);
void runEffect(Game& g, Engine& E, Instance& in, const dsl::EffDef& e);
}  // namespace story

// story_game.cpp's helpers, shared by the hook files
namespace storyg {
// the engine's own record of the player's footsteps (Engine::trail_; the companions' trail is kept only with a party)
inline std::vector<Vec2> trailOf(const Game& g) {
  std::vector<Vec2> v;
  v.reserve(g.story.trail_.size());
  for (const story::Engine::Crumb& c : g.story.trail_) v.push_back(Vec2(c.x, c.y));
  return v;
}

inline int findA(const Game& g, int id) {
  if (story::host().findActor) return story::host().findActor(g, id);
  for (size_t k = 0; k < g.actors.size(); k++) if (g.actors[k].id == id) return (int)k;
  return -1;
}

inline void spoken(Game& g, const Actor& a, const std::string& line, uint32_t col) {
  Event e;
  e.type = Ev::Text; e.p = a.p + Vec2(0, -30); e.a = (int)col; e.f = 0; e.s = line;
  g.events.push_back(e);
  if (story::host().say) story::host().say(g, a.name.substr(0, a.name.find(' ')) + ": " + line);
}

// a bout opponent squares up: stats matched to the player (a fair fight, not a slaughter either way), hostile to the
// player only (game.cpp collectHostiles leaves the town out of it), not to be talked to while it lasts
inline void armForBout(Game& g, Actor& a) {
  const Actor& p = g.pl();
  if (!a.hostile || a.npc) {   // (a foe of the story is already armed: its own stats and colours stand)
    a.level = std::max(1, g.plLevel);
    a.maxHp = std::max(40.0f, p.maxHp * 0.9f);
    a.hp = a.maxHp;
    a.dmg = std::max(3.0f, p.maxHp * 0.085f);
    a.windup = 0.42f; a.range = 15; a.speed = 52; a.aggroR = 220; a.armor = 0;
    // the town's own colours: hostile to the player alone (warFoes: whoever is hostile fights the player), so neither
    // the bystanders nor the watch are any part of it, and its blows land on nobody else
    a.faction = Faction::Town;
  }
  a.hostile = true; a.npc = false; a.aggro = true; a.target = p.id;
  a.posture = art::Posture::None; a.fleeing = false;
}
// they stand down: friendly again (the yield, the player beaten, the player gone)
inline void standDown(Actor& a) {
  a.hostile = false; a.npc = true; a.faction = Faction::Town; a.aggro = false; a.target = -1;
  a.st = AState::Idle; a.stT = 0; a.heavy = a.lunge = false; a.knock = Vec2();
}

// defined in story_actors.cpp
bool besidePlayer(const Game& g, float back, int& ox, int& oy, uint32_t salt);
void syncPersons(Game& g);
void placeWantedProps(Game& g);
void placeInsiders(Game& g, int site);
void engineSync(Game& g);
// defined in story_game.cpp
void engineTick(Game& g, float dt);
}  // namespace storyg
using namespace storyg;
