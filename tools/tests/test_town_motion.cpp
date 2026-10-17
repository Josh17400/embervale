// rpg_test --town-motion [--seeds A..B] [--from 8] [--to 18] [--radius 24] [--city] [--gates] [--report]
// P1 "Fresh Roads" (lead; VISION_PLAN 15.34): do the townsfolk go about their lives? The owner's playtest: "in towns and
// cities lots of NPCs stand still". Per seed, headless: the player arrives at the heart of the town nearest the start
// (--city: the nearest city too) at --from o'clock, as a player walking in does (a fresh reconcile), and stands there
// while the day plays on, continuously, to --to o'clock. Every 5 game minutes every person outdoors within --radius
// tiles of the heart is looked at (no animals, no companions):
//   walking share   people in AState::Walk / people present, averaged over the samples        gate >= 15 %
//   arrival share   the same over the first 15 game minutes (the state a player walks into)     reported
//   static          a person who stays within 1.5 tiles of one spot for 2 game hours, unless   gate 0
//                   they are a sentry (guard, soldier, captain, herald at their post) or a
//                   posted worker in an animated work pose the whole time (a stall keeper, a
//                   resident at their own workplace, a field / yard trade: farmer, fisher,
//                   miner, woodcutter, herder, labourer, a bard playing, a beggar begging)
//   life cost       lifeFolkStats: average and worst ms of a life step (the perf must stay flat)  reported
// The P1 survey (seed 7): start village 14 of 16 never moved, TOWN 0 of 16 walking, CITY 5 of 27 walking; 34-36 % of
// town folk parked WORK@STREET for the whole work block.
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>
#include "rpg/sim/game_internal.h"
#include "rpg/sim/life.h"
#include "tools/tests/tests.h"

namespace {

using art::Posture;

struct Track {
  Vec2 anchor;
  float since = 0;        // game hours (day * 24 + hour) the person has been within 1.5 tiles of anchor
  bool allPosted = true;  // a sentry or a posted worker in a work pose at every look since then
  bool reported = false;
  std::string what;
};

bool workPose(Posture p) {
  switch (p) {
    case Posture::Hammer: case Posture::Hoe: case Posture::Chop: case Posture::Stir: case Posture::Fish: case Posture::Sweep:
    case Posture::Carry: case Posture::Lamp: case Posture::Lute: case Posture::Drum: case Posture::Flute: case Posture::Read: case Posture::Beg:
      return true;
    default: return false;
  }
}
const char* postureName(Posture p) {
  static const char* n[] = {"none", "sit", "siteat", "sitdrink", "eat", "drink", "cheer", "hammer", "hoe", "sweep", "chop", "stir", "carry", "fish",
                            "sleep", "wave", "play", "dance", "lute", "drum", "flute", "pray", "beg", "lamp", "read", "sitfloor", "sitfloor",
                            "sitfloor", "ride", "downed", "kneel"};
  return (int)p < (int)(sizeof(n) / sizeof(n[0])) ? n[(int)p] : "?";
}

// a sentry, or a posted worker in an animated work pose (see the header)
bool posted(Game& g, const Actor& a, std::string& what) {
  if (a.role == Role::Guard || a.role == Role::Soldier || a.role == Role::Captain || a.role == Role::Herald) { what = "sentry"; return true; }
  const life::Resident* r = nullptr;
  if (a.resident >= 0 && a.site >= 0 && a.site < (int)g.world.sites.size())
    if (life::Census* c = g.life.census(g.world, a.site))
      if (a.resident < (int)c->res.size()) r = &c->res[(size_t)a.resident];
  what = std::string(r ? life::jobName(r->job) : "SPAWN") + " " + (r ? life::actName(r->act) : "-") + (r && r->work < 0 ? "@NOWORK" : "") + " pose " +
         postureName(a.posture);
  if (!workPose(a.posture)) return false;
  if (a.stallKeeper) return true;
  if (!r) return false;
  if (r->work >= 0 && r->act == life::Act::Work) return true;
  switch (r->job) {
    case life::Job::Farmer: case life::Job::Fisher: case life::Job::Miner: case life::Job::Woodcutter: case life::Job::Herder: case life::Job::Labourer:
    case life::Job::Bard: case life::Job::Beggar: case life::Job::Lamplighter:
      return true;
    default: return false;
  }
}

void tick(Game& g, int frames) {
  for (int f = 0; f < frames; f++) {
    g.update(SIM_DT, Input());
    g.events.clear();
    if (g.mode != Mode::Play) g.mode = Mode::Play;
  }
}

struct Result {
  double walkShare = 0, arrivalShare = 0;
  int samples = 0, statics = 0, people = 0;
  double avgMs = 0, worstMs = 0;
};

// one settlement: arrive at its heart at `from`, stand until `to`
Result watch(Game& g, int site, float from, float to, float radius, bool report, const char* label) {
  Result R;
  const ew::Gid sid = g.world.sites[(size_t)site].id;
  const std::string name = g.world.sites[(size_t)site].name;
  if (g.inside) g.debugLeave();
  {
    const Site s = g.world.sites[(size_t)site];
    g.teleportGlobal(g.world.ox + s.ex, g.world.oy + s.ey + 1);
  }
  g.hour = from;
  tick(g, 2);
  const float dayH = (float)g.day * 24.0f;
  std::map<int, Track> tr;
  double shareSum = 0, arrSum = 0;
  int arrN = 0;
  const int stepsBefore = g.lifeFolkStats().steps;
  const double sumBefore = g.lifeFolkStats().sumMs;
  double worst = 0;
  float nextLook = from;
  int maxPresent = 0;
  while (true) {
    const float now = (float)g.day * 24.0f + g.hour - dayH;   // hours since midnight of the arrival day
    if (now >= to) break;
    if (now + 1e-4f >= nextLook) {
      nextLook = now + 5.0f / 60.0f;
      const int here = g.world.siteHandle(sid);
      if (here < 0 || g.inside) break;
      const Site& S = g.world.sites[(size_t)here];
      const Vec2 heart((S.ex + 0.5f) * TILE, (S.ey + 0.5f) * TILE);
      int present = 0, walking = 0;
      std::map<int, bool> seen;
      for (size_t k = 1; k < g.actors.size(); k++) {
        const Actor& a = g.actors[k];
        if (!a.npc || !a.human || a.critter || a.comp || a.st == AState::Dead || a.hostile) continue;
        if (len(a.p - heart) > radius * TILE) continue;
        present++;
        if (a.st == AState::Walk) walking++;
        seen[a.id] = true;
        std::string what;
        const bool ok = posted(g, a, what);
        auto it = tr.find(a.id);
        if (it == tr.end() || len(a.p - it->second.anchor) > 1.5f * TILE) {
          Track t;
          t.anchor = a.p; t.since = now; t.allPosted = ok; t.what = what;
          tr[a.id] = t;
          continue;
        }
        Track& t = it->second;
        t.allPosted = t.allPosted && ok;
        t.what = what;
        if (!t.reported && now - t.since >= 2.0f && !t.allPosted) {
          t.reported = true;
          R.statics++;
          if (report || R.statics <= 3)
            out("  town-motion %s %s: %s stood still from %.2f to %.2f (%s) at %d,%d\n", label, name.c_str(), a.name.c_str(), t.since, now, t.what.c_str(),
                (int)std::floor(a.p.x / TILE), (int)std::floor(a.p.y / TILE));
        }
      }
      for (auto it = tr.begin(); it != tr.end();) it = seen.count(it->first) ? std::next(it) : tr.erase(it);
      if (present > 0) {
        const double sh = (double)walking / present;
        shareSum += sh;
        R.samples++;
        if (now - from < 0.25f + 1e-3f) { arrSum += sh; arrN++; }
      }
      maxPresent = std::max(maxPresent, present);
    }
    tick(g, 30);
    worst = std::max(worst, g.lifeFolkStats().lastMs);
  }
  const int steps = g.lifeFolkStats().steps - stepsBefore;
  R.avgMs = steps > 0 ? (g.lifeFolkStats().sumMs - sumBefore) / steps : 0;
  R.worstMs = worst;
  R.walkShare = R.samples ? 100.0 * shareSum / R.samples : 0;
  R.arrivalShare = arrN ? 100.0 * arrSum / arrN : 0;
  R.people = maxPresent;
  out("  town-motion %s %s (seed %llu): %d samples, up to %d people within %.0f tiles; walking %.1f %% (arrival %.1f %%), static for 2 h: %d; "
      "life step avg %.3f ms, worst %.3f ms\n",
      label, name.c_str(), (unsigned long long)g_curSeed, R.samples, maxPresent, (double)radius, R.walkShare, R.arrivalShare, R.statics, R.avgMs, R.worstMs);
  return R;
}

int townMotion(int argc, char** argv) {
  uint64_t A = 1, B = 3;
  float from = 8, to = 18, radius = 24;
  bool city = false, gates = false, report = false;
  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "--seeds") && i + 1 < argc) {
      if (!parseSeedRange(argv[++i], A, B)) { printf("bad --seeds range '%s'\n", argv[i]); return 2; }
    } else if (!strcmp(argv[i], "--from") && i + 1 < argc) from = (float)atof(argv[++i]);
    else if (!strcmp(argv[i], "--to") && i + 1 < argc) to = (float)atof(argv[++i]);
    else if (!strcmp(argv[i], "--radius") && i + 1 < argc) radius = (float)atof(argv[++i]);
    else if (!strcmp(argv[i], "--city")) city = true;
    else if (!strcmp(argv[i], "--gates")) gates = true;
    else if (!strcmp(argv[i], "--report")) report = true;
  }
  const auto t0 = std::chrono::steady_clock::now();
  double shareSum = 0, arrSum = 0, msSum = 0, worst = 0;
  int n = 0, statics = 0, bad = 0;
  for (uint64_t seed = A; seed <= B; seed++) {
    g_curSeed = seed;
    for (int pass = 0; pass < (city ? 2 : 1); pass++) {
      Game g(seed);
      g.newEndlessGame(seed);
      g.mode = Mode::Play; g.godMode = true; g.noWildSpawns = true;
      tick(g, 10);
      const Site& h = g.world.sites[(size_t)g.world.startSite];
      const int si = g.world.findSiteNear(g.world.ox + h.ex, g.world.oy + h.ey, pass == 0 ? SiteType::Town : SiteType::City, 8);
      if (si < 0) { out("WARN: town-motion seed %llu: no %s near the start\n", (unsigned long long)seed, pass == 0 ? "town" : "city"); continue; }
      const Result R = watch(g, si, from, to, radius, report, pass == 0 ? "town" : "city");
      if (!R.samples) { out("FAIL: town-motion seed %llu: nobody seen at the %s's heart\n", (unsigned long long)seed, pass == 0 ? "town" : "city"); bad++; continue; }
      shareSum += R.walkShare; arrSum += R.arrivalShare; msSum += R.avgMs; worst = std::max(worst, R.worstMs);
      statics += R.statics;
      n++;
    }
  }
  const double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
  const double share = n ? shareSum / n : 0;
  printf("town-motion: seeds %llu..%llu%s, %.0f:00-%.0f:00, radius %.0f: walking %.1f %% (arrival %.1f %%), static for 2 h %d; life step avg %.3f ms, worst "
         "%.3f ms (%.0f s)\n",
         (unsigned long long)A, (unsigned long long)B, city ? " (towns and cities)" : " (towns)", (double)from, (double)to, (double)radius, share,
         n ? arrSum / n : 0.0, statics, n ? msSum / n : 0.0, worst, secs);
  if (gates) {
    if (share < 15.0) { printf("FAIL: town-motion walking %.1f %% (want >= 15)\n", share); bad++; }
    if (statics > 0) { printf("FAIL: town-motion %d people static for 2 game hours (want 0)\n", statics); bad++; }
    printf("town-motion gates: %s\n", bad ? "FAIL" : "ok");
  }
  return bad ? 1 : 0;
}

}  // namespace

RPG_TEST_CMD("--town-motion", "P1: townsfolk walking share and nobody static [--seeds A..B --from 8 --to 18 --radius 24 --city --gates --report]", townMotion);
