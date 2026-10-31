// rpg_test --doll-clips (SOLID 2a.5): the seams 2a.5 adds for other lanes.
//   - art::doll::clipsOf(bodyKey, part) returns the same records as pool::clipsOf (every field, every frame's plants) for
//     every body key the pool's clips name and every RigPart (doll.h clipsOfCheck walks the pool's own lookup);
//   - the elder sets ("man.elder", "woman.elder") have their own Loco walk clips through it (anim.cpp's elder walk);
//   - rpg/story/story_api.h compiles on its own (it is this file's first include) with the slot ranges as saved.
#include "rpg/story/story_api.h"
#include <string>
#include <vector>
#include "rpg/art/doll/doll.h"
#include "tools/tests/tests.h"

static_assert(story::PERSON_SLOT0 == 50000 && story::PERSON_ROLES == 32 && story::HERALD_SLOT == 950, "story slots as saved");
static_assert(story::DROVE_SLOT0 == story::PERSON_SLOT1 && story::DROVE_MAX == 8, "drove slots as saved");

static int dollClipsCmd(int, char**) {
  int fails = 0;
  std::vector<std::string> bad;
  const int n = art::doll::clipsOfCheck(&bad);
  for (size_t i = 0; i < bad.size() && i < 20; i++) out("FAIL: clipsOf %s\n", bad[i].c_str());
  if (n <= 0) { out("FAIL: clipsOf: no body keys compared\n"); fails++; }
  fails += (int)bad.size();
  out("clipsOf vs pool::clipsOf: %d lookups, %d mismatches\n", n, (int)bad.size());
  for (const char* key : {"man.elder", "woman.elder"}) {
    int own = 0, walk = 0;
    for (const art::doll::ClipInfo& c : art::doll::clipsOf(key, art::RigPart::Loco)) {
      if (!c.own) continue;
      own++;
      if (c.clip == (uint8_t)art::Clip::Walk && c.facing == 2 && c.nFrames > 0 && c.plants.size() == c.nFrames) walk++;
    }
    out("%s: %d own Loco clips, %d walk (facing 2)\n", key, own, walk);
    if (walk < 1) { out("FAIL: %s: expected an own walk clip facing 2\n", key); fails++; }
  }
  if (fails) out("doll-clips: FAIL (%d)\n", fails);
  else out("doll-clips: ALL OK\n");
  return fails ? 1 : 0;
}
RPG_TEST_CMD("--doll-clips", "(SOLID 2a.5) doll::clipsOf equals pool::clipsOf; story_api.h stands alone", dollClipsCmd);
