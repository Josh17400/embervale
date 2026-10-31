// SOLID 1.3 STORY: the story block of the save (Engine::serialize / deserialize, its versions v1-v4 and the v3
// repetition guard). Moved byte for byte from story.cpp; the game's save is rpg/sim/game_save.cpp (SAVE_VER).
#include "rpg/story/story.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include "engine/audio.h"
#include "rpg/culture/culture.h"
#include "rpg/sim/craft.h"
#include "rpg/sim/foes.h"
#include "rpg/sim/gear.h"
#include "rpg/sim/life.h"
#include "rpg/sim/party.h"
#include "rpg/sim/party_voice.h"
#include "rpg/sim/talk.h"
#include "rpg/story/saga.h"
#include "rpg/sim/common.h"
#include "rpg/sim/game.h"
#include "rpg/story/dsl.h"
#include "rpg/story/radiant.h"
#include "rpg/story/story_internal.h"
#include "rpg/story/story_detail.h"
#include "rpg/sim/home.h"
#include "rpg/world/economy.h"
#include "rpg/world/source.h"

namespace story {
namespace {
constexpr uint8_t STORY_BLOCK_V1 = 1;
constexpr uint8_t STORY_BLOCK_V2 = 2;    // (STORY lane) v2: bindings' places, stage day / counter, lore masks, memories,
                                          // facts, successions, plays, the heralds' proclaimed serial. v1 still loads.
constexpr uint8_t STORY_BLOCK_V3 = 3;    // (M6b, SAVE_VER 13) v3: v2 + the sagas' repetition guard
constexpr uint8_t STORY_BLOCK_VER = 4;   // (M6b COMPOSER) v4: the guard's records carry their hook and the offered flag
}  // namespace

// ---------------------------------------------------------------- persistence
// Layout v2: ver u8, nextId u32, running (u32 n; each: id u32, script str, stage i32, questId i32, done u8, failed u8,
// stageDay i32, count i32, cast (u32 n: role str, id u64, name str, kind u8, site u64, gx i32, gy i32, hasPos u8,
// female u8, trade u8, extra str), vars (u32 n: name str, value i32)), lore (u32 n; each: key u64, title str, found i32,
// total i32, foundMask u32, complete u8, lines (u32 n: str)), memories (u32 n: who u64, line str, day i32), facts
// (u32 n: site u64, day i32, text str), rulers (u32 n: kingdom u64, name str, title str, day i32), played (u32 n:
// script str, count i32), proclaimed u32.
// v3 (M6b) appends the repetition guard: seen (u32 n: arch u32, twists u32, shape u32, day i32, gx i32, gy i32 [v4: hook
// u32, offered u8]), phrases (u32 n: family u32, count u16). v1, v2 and v3 blocks still load.
// Layout v1 (phase A; the save fixture's empty block): ver u8, nextId u32, running (as v2 without stageDay, count and
// the bindings' places), lore (as v2 without foundMask and complete). It loads as the same state with the new fields
// at their defaults.
void Engine::serialize(std::vector<uint8_t>& out) const {
  out.clear();
  BinW w(out);
  w.u8(STORY_BLOCK_VER);
  w.u32(nextId_);
  w.u32((uint32_t)running_.size());
  for (const Instance& in : running_) {
    w.u32(in.id); w.str(in.script); w.i32(in.stage); w.i32(in.questId); w.u8(in.done ? 1 : 0); w.u8(in.failed ? 1 : 0);
    w.i32(in.stageDay); w.i32(in.count);
    w.u32((uint32_t)in.cast.size());
    for (const Binding& b : in.cast) {
      w.str(b.role); w.u64(b.id); w.str(b.name);
      w.u8(b.kind); w.u64(b.site); w.i32(b.gx); w.i32(b.gy); w.u8(b.hasPos ? 1 : 0); w.u8(b.female ? 1 : 0); w.u8(b.trade); w.str(b.extra);
    }
    w.u32((uint32_t)in.vars.size());
    for (const auto& kv : in.vars) { w.str(kv.first); w.i32(kv.second); }
  }
  w.u32((uint32_t)lore_.size());
  for (const LoreEntry& e : lore_) {
    w.u64(e.key); w.str(e.title); w.i32(e.found); w.i32(e.total); w.u32(e.foundMask); w.u8(e.complete ? 1 : 0);
    w.u32((uint32_t)e.lines.size());
    for (const std::string& l : e.lines) w.str(l);
  }
  w.u32((uint32_t)memories_.size());
  for (const Memory& m : memories_) { w.u64(m.who); w.str(m.line); w.i32(m.day); }
  w.u32((uint32_t)facts_.size());
  for (const Fact& f : facts_) { w.u64(f.site); w.i32(f.day); w.str(f.text); }
  w.u32((uint32_t)rulers_.size());
  for (const RulerOverride& r : rulers_) { w.u64(r.kingdom); w.str(r.name); w.str(r.title); w.i32(r.day); }
  w.u32((uint32_t)played_.size());
  for (const auto& kv : played_) { w.str(kv.first); w.i32(kv.second); }
  w.u32(proclaimed_);
  // v3: the repetition guard (v4: + hook, offered)
  w.u32((uint32_t)guard_.seen.size());
  for (const saga::Seen& x : guard_.seen) { w.u32(x.arch); w.u32(x.twists); w.u32(x.shape); w.i32(x.day); w.i32(x.gx); w.i32(x.gy); w.u32(x.hook); w.u8(x.offered); }
  w.u32((uint32_t)guard_.phrases.size());
  for (const auto& kv : guard_.phrases) { w.u32(kv.first); w.u16(kv.second); }
}

bool Engine::deserialize(const std::vector<uint8_t>& data) {
  BinR r(data);
  const uint8_t ver = r.u8();
  if (r.bad || (ver != STORY_BLOCK_V1 && ver != STORY_BLOCK_V2 && ver != STORY_BLOCK_V3 && ver != STORY_BLOCK_VER)) return false;
  const bool v2 = ver >= STORY_BLOCK_V2, v3 = ver >= STORY_BLOCK_V3, v4 = ver >= STORY_BLOCK_VER;
  Engine E;
  E.seed_ = seed_;
  E.nextId_ = r.u32();
  const uint32_t n = r.u32();
  if (n > 10000 || n > data.size()) return false;
  for (uint32_t i = 0; i < n && !r.bad; i++) {
    Instance in;
    in.id = r.u32(); in.script = r.str(); in.stage = r.i32(); in.questId = r.i32(); in.done = r.u8() != 0; in.failed = r.u8() != 0;
    if (v2) { in.stageDay = r.i32(); in.count = r.i32(); }
    const uint32_t nc = r.u32();
    if (nc > 1000) return false;
    for (uint32_t k = 0; k < nc && !r.bad; k++) {
      Binding b;
      b.role = r.str(); b.id = r.u64(); b.name = r.str();
      if (v2) {
        b.kind = r.u8(); b.site = r.u64(); b.gx = r.i32(); b.gy = r.i32(); b.hasPos = r.u8() != 0; b.female = r.u8() != 0;
        b.trade = r.u8(); b.extra = r.str();
      }
      in.cast.push_back(b);
    }
    const uint32_t nv = r.u32();
    if (nv > 10000) return false;
    for (uint32_t k = 0; k < nv && !r.bad; k++) { std::string key = r.str(); in.vars[key] = r.i32(); }
    // (a damaged save: a stage past the script's loads as its end, never as an index past the table)
    if (const dsl::Script* s = scriptOf(in)) { if (in.stage < 0 || in.stage >= (int)s->stages.size()) { in.stage = 0; in.done = true; in.failed = true; } }
    else { in.done = true; in.failed = true; in.vars["_forgotten"] = 1; }   // (M6b: closed by the next step)
    E.running_.push_back(std::move(in));
  }
  const uint32_t nl = r.u32();
  if (nl > 100000 || nl > data.size()) return false;
  for (uint32_t i = 0; i < nl && !r.bad; i++) {
    LoreEntry e;
    e.key = r.u64(); e.title = r.str(); e.found = r.i32(); e.total = r.i32();
    if (v2) { e.foundMask = r.u32(); e.complete = r.u8() != 0; }
    const uint32_t k = r.u32();
    if (k > 1000) return false;
    for (uint32_t j = 0; j < k && !r.bad; j++) e.lines.push_back(r.str());
    E.lore_.push_back(std::move(e));
  }
  if (v2) {
    uint32_t c = r.u32();
    if (c > 100000 || c > data.size()) return false;
    for (uint32_t i = 0; i < c && !r.bad; i++) { Memory m; m.who = r.u64(); m.line = r.str(); m.day = r.i32(); E.memories_.push_back(m); }
    c = r.u32();
    if (c > 100000 || c > data.size()) return false;
    for (uint32_t i = 0; i < c && !r.bad; i++) { Fact f; f.site = r.u64(); f.day = r.i32(); f.text = r.str(); E.facts_.push_back(f); }
    c = r.u32();
    if (c > 100000 || c > data.size()) return false;
    for (uint32_t i = 0; i < c && !r.bad; i++) { RulerOverride o; o.kingdom = r.u64(); o.name = r.str(); o.title = r.str(); o.day = r.i32(); E.rulers_.push_back(o); }
    c = r.u32();
    if (c > 100000 || c > data.size()) return false;
    for (uint32_t i = 0; i < c && !r.bad; i++) { std::string s = r.str(); E.played_[s] = r.i32(); }
    E.proclaimed_ = r.u32();
  }
  if (v3) {
    uint32_t c = r.u32();
    if (c > saga::Guard::GUARD_SEEN || c > data.size()) return false;
    for (uint32_t i = 0; i < c && !r.bad; i++) {
      saga::Seen x;
      x.arch = r.u32(); x.twists = r.u32(); x.shape = r.u32(); x.day = r.i32(); x.gx = r.i32(); x.gy = r.i32();
      if (v4) { x.hook = r.u32(); x.offered = r.u8(); }
      E.guard_.seen.push_back(x);
    }
    c = r.u32();
    if (c > saga::Guard::GUARD_PHRASES || c > data.size()) return false;
    for (uint32_t i = 0; i < c && !r.bad; i++) { const uint32_t f = r.u32(); E.guard_.phrases[f] = r.u16(); }
  }
  if (r.bad || r.p != data.size()) return false;
  *this = std::move(E);
  return true;
}

}  // namespace story
