// EMBERVALE art: the doll's body set and walk tables (DOLL; P2b phase A, split out of doll_resolve.cpp). The simulation
// reads these (rpg/sim/anim.cpp: the gait, the walk clock), so they build with the pool (doll_pool.cpp, gen/doll_pool.cpp)
// into rpg_doll, the doll's data library below both rpg_sim and rpg_art: no painter, no SDL. Nothing here draws.
#include <algorithm>
#include <string>
#include <vector>
#include "rpg/art/doll/doll_internal.h"

namespace art {
namespace doll {

BodySet bodySetOf(const HumanLook& L) {
  if (L.age == 1) return BodySet::Child;
  if (L.age == 2) return BodySet::Youth;
  return L.body == 1 ? BodySet::Woman : BodySet::Man;
}

// ---------------------------------------------------------------- the walk table
int WalkTable::frameAt(float px) const {
  if (frames <= 0 || cycle <= 0) return 0;
  float d = px - cycle * (float)(int)(px / cycle);
  if (d < 0) d += cycle;
  int f = 0;
  while (f + 1 < frames && d >= start[f + 1]) f++;
  return f;
}
// (P2b integration) the hair family table lives in the data library, so the simulation (gear_look.cpp: the created
// hero's style within the family) links without the painters (save_test)
namespace {
// family x variant -> style (art::Hair order: Bald, Short, Long, Ponytail, Mohawk, Braids, Bun, Curls)
const int kFamilyStyles[8][5] = {{0, 1, -1, -1, -1}, {3, 2, 4, 5, 7}, {12, 13, 11, 10, -1}, {14, 15, 22, -1, -1},
                                 {8, 9, -1, -1, -1}, {16, 17, 19, 20, -1}, {18, 21, -1, -1, -1}, {6, 23, -1, -1, -1}};
}  // namespace
int hairVariants(Hair f) {
  const int fi = std::clamp((int)f, 0, 7);
  int n = 0;
  while (n < 5 && kFamilyStyles[fi][n] >= 0) n++;
  return n;
}
int hairStyleOf(Hair f, uint8_t v) {
  const int fi = std::clamp((int)f, 0, 7);
  const int n = hairVariants(f);
  return kFamilyStyles[fi][std::min((int)v, n - 1)];
}
namespace {
WalkTable walkTableOf(BodySet set, const std::string& bodyKey, Gait gait, int facing) {
  using namespace pool;
  WalkTable t;
  static const float kStride[4] = {19.0f, 20.0f, 16.0f, 13.0f};
  t.cycle = gait == Gait::Run ? 36.0f : kStride[std::min((int)set, 3)];
  const Pool& P = get();
  const std::vector<const ClipRec*> cs = clipsOf(bodyKey, gait == Gait::Run ? (uint8_t)RigPart::Run : (uint8_t)RigPart::Loco, -1, -1);
  const ClipRec* c = nullptr;
  for (const ClipRec* x : cs) if (x->clip == (uint8_t)Clip::Walk && x->facing == facing) c = x;
  if (!c) return t;   // frames 0: no clip (the caller keeps its own table)
  t.frames = std::min<int>(c->nFrames, 16);
  float adv[16] = {};
  float total = 0;
  bool planted = true;
  for (int i = 0; i < t.frames; i++) {
    const FrameRec& f = P.frames[c->firstFrame + i];
    if (f.plant0 == kNoPlant || f.plant1 == kNoPlant) { planted = false; break; }
    adv[i] = std::max(0.0f, (f.plant0 - f.plant1) / 16.0f);   // the planted foot slides back as the body moves on
    total += adv[i];
  }
  if (!planted || total <= 0) {
    if (c->cycle16) t.cycle = c->cycle16 / 16.0f;
    for (int i = 0; i < t.frames; i++) t.start[i] = t.cycle * i / t.frames;
    return t;
  }
  t.cycle = total;
  float acc = 0;
  for (int i = 0; i < t.frames; i++) { t.start[i] = acc; acc += adv[i]; }
  return t;
}
}  // namespace
WalkTable walkTable(BodySet set, Gait gait, int facing) { return walkTableOf(set, bodySetName(set), gait, facing); }
WalkTable walkTable(const HumanLook& look, Gait gait, int facing) {
  const BodySet set = bodySetOf(look);
  std::string key = bodySetName(set);
  if (look.age == 3 && (set == BodySet::Man || set == BodySet::Woman)) key += ".elder";
  return walkTableOf(set, key, gait, facing);
}

}  // namespace doll
}  // namespace art
