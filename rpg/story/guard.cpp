// M6b "Sagas": the repetition guard (rpg/story/saga.h Guard). COMPOSER lane.
//
// What the player was told (and offered), so the composer steers away from the same archetype, twists and phrase
// families in the same region and the recent days (VISION_PLAN 15.20: "no two stories within 30 km share archetype +
// twist"). Saved in the story block (story.cpp, block v4).
//
// THE RULES (penalty, 0 .. 1000; pick() never offers at 1000 and weighs the rest down):
//   told stories (Seen::offered 0):
//     the same archetype + twists within GUARD_RADIUS                    1000, for ever (as long as the record lasts)
//     the same archetype within GUARD_RADIUS, fewer than GUARD_DAYS ago  700
//     the same archetype within GUARD_RADIUS, longer ago                 300
//     the same archetype further away, fewer than GUARD_DAYS ago         150
//   offers shown but not taken (Seen::offered 1), from ANOTHER hook, fewer than GUARD_DAYS ago:
//     the same archetype + twists within GUARD_RADIUS                    1000
//     the same archetype within GUARD_RADIUS                             400
//   (the hook that made an offer may make it again: talking twice to one innkeeper gives the same pitch)
//   phrase families: 120 per time heard, at most 1000 (pick() compares a few seeds of one story by them)
#include "rpg/story/saga.h"
#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

namespace story {
namespace saga {

namespace {
uint32_t fnv(const std::string& s) {
  uint32_t h = 2166136261u;
  for (char c : s) { h ^= (uint8_t)c; h *= 16777619u; }
  return h;
}
uint32_t archKeyOf(const std::string& arch, bool campaign) { return fnv((campaign ? "@" : "") + arch); }
uint32_t archKey(const Spec& s) { return archKeyOf(s.arch, s.campaign); }
uint32_t twistKey(const Spec& s) {
  std::string j;
  for (int k = 0; k < 3; k++) { j += s.twist[k]; j += '|'; }
  for (const std::string& a : s.arcs) { j += a; j += '.'; }
  return fnv(j);
}
bool nearOf(const Seen& x, int32_t gx, int32_t gy) {
  const int64_t dx = (int64_t)x.gx - gx, dy = (int64_t)x.gy - gy;
  return dx * dx + dy * dy <= (int64_t)Guard::GUARD_RADIUS * Guard::GUARD_RADIUS;
}
void push(Guard& G, const Seen& x) {
  G.seen.push_back(x);
  if (G.seen.size() > Guard::GUARD_SEEN) {
    // forget offers first (the oldest), then the oldest told story
    size_t drop = G.seen.size();
    for (size_t i = 0; i < G.seen.size(); i++) if (G.seen[i].offered) { drop = i; break; }
    if (drop == G.seen.size()) drop = 0;
    G.seen.erase(G.seen.begin() + (std::ptrdiff_t)drop);
  }
}
}  // namespace

void Guard::note(const Spec& s, uint32_t shape, int day, int32_t gx, int32_t gy, const std::vector<uint32_t>& families) {
  Seen x;
  x.arch = archKey(s);
  x.twists = twistKey(s);
  x.shape = shape;
  x.day = day;
  x.gx = gx;
  x.gy = gy;
  push(*this, x);
  for (uint32_t f : families) {
    if (!f) continue;
    auto it = phrases.find(f);
    if (it != phrases.end()) { if (it->second < 0xFFFF) it->second++; continue; }
    if (phrases.size() >= GUARD_PHRASES) {
      // forget the least heard family to make room
      auto low = phrases.begin();
      for (auto p = phrases.begin(); p != phrases.end(); ++p) if (p->second < low->second) low = p;
      phrases.erase(low);
    }
    phrases[f] = 1;
  }
}

void Guard::noteOffer(const Spec& s, uint32_t hook, int day, int32_t gx, int32_t gy) {
  const uint32_t a = archKey(s), t = twistKey(s);
  for (Seen& x : seen)
    if (x.offered && x.hook == hook && x.arch == a && x.twists == t) { x.day = day; return; }   // (the same pitch again)
  Seen x;
  x.arch = a;
  x.twists = t;
  x.day = day;
  x.gx = gx;
  x.gy = gy;
  x.hook = hook;
  x.offered = 1;
  push(*this, x);
}

int Guard::penalty(const Spec& s, int day, int32_t gx, int32_t gy, uint32_t hook) const {
  const uint32_t a = archKey(s), t = twistKey(s);
  int worst = 0;
  for (const Seen& x : seen) {
    if (x.arch != a) continue;
    const bool near = nearOf(x, gx, gy);
    const int age = day - x.day;
    if (x.offered) {
      if ((hook && x.hook == hook) || !near || age >= GUARD_DAYS) continue;
      if (x.twists == t) return 1000;
      worst = std::max(worst, 400);
      continue;
    }
    if (near && x.twists == t) return 1000;
    if (near && age < GUARD_DAYS) worst = std::max(worst, 700);
    else if (near) worst = std::max(worst, 300);
    else if (age < GUARD_DAYS) worst = std::max(worst, 150);
  }
  return worst;
}

int Guard::penalty(const Spec& s, int day, int32_t gx, int32_t gy) const { return penalty(s, day, gx, gy, 0); }

int Guard::archPenalty(const std::string& arch, bool campaign, int day, int32_t gx, int32_t gy, uint32_t hook) const {
  const uint32_t a = archKeyOf(arch, campaign);
  int worst = 0;
  for (const Seen& x : seen) {
    if (x.arch != a) continue;
    const bool near = nearOf(x, gx, gy);
    const int age = day - x.day;
    if (x.offered) {
      if ((hook && x.hook == hook) || !near || age >= GUARD_DAYS) continue;
      worst = std::max(worst, 400);
      continue;
    }
    if (near && age < GUARD_DAYS) worst = std::max(worst, 700);
    else if (near) worst = std::max(worst, 300);
    else if (age < GUARD_DAYS) worst = std::max(worst, 150);
  }
  return worst;
}

int Guard::phrasePenalty(uint32_t family) const {
  auto it = phrases.find(family);
  return it == phrases.end() ? 0 : std::min(1000, (int)it->second * 120);
}

}  // namespace saga
}  // namespace story
