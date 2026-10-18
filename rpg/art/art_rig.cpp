// EMBERVALE art: the Clear Sight rig contract's shared pieces (art_rig.h; P2a phase A, lead). The painters live in
// rpg/art/rig/*.cpp (one owner each: see art_rig.h); this file dispatches to them and holds the atlas helpers.
#include "rpg/art/art_rig.h"
#include "rpg/art/rig/rig_internal.h"

namespace art {

int RigSheet::count(int facing, Clip c, int variant) const {
  if (spans.empty() || (int)c >= CLIP_COUNT) return 0;
  const Span& s = spans[slot(facing, c, variant)];
  if (s.count) return s.count;
  return variant ? spans[slot(facing, c, 0)].count : 0;
}

const FrameRect* RigSheet::frame(int facing, Clip c, int variant, int i) const {
  if (spans.empty() || (int)c >= CLIP_COUNT) return nullptr;
  const Span* s = &spans[slot(facing, c, variant)];
  if (!s->count && variant) s = &spans[slot(facing, c, 0)];
  if (!s->count || s->first < 0) return nullptr;
  const int n = s->count, k = ((i % n) + n) % n;
  const size_t at = (size_t)s->first + (size_t)k;
  return at < frames.size() ? &frames[at] : nullptr;
}

void RigSheet::add(int facing, Clip c, int variant, const FrameRect& r) {
  if ((int)c >= CLIP_COUNT) return;
  Span& s = spans[slot(facing, c, variant)];
  if (s.count == 0) s.first = (int16_t)frames.size();
  frames.push_back(r);
  if (!anchors.empty()) anchors.resize(frames.size());   // a frame without pose data keeps the anchors aligned
  s.count++;
}

void RigSheet::add(int facing, Clip c, int variant, const FrameRect& r, const RigAnchors& an) {
  if ((int)c >= CLIP_COUNT) return;
  anchors.resize(frames.size());   // (the frames before keep empty pose data)
  add(facing, c, variant, r);
  anchors.resize(frames.size());
  anchors.back() = an;
}

const RigAnchors* RigSheet::anchorsOf(int facing, Clip c, int variant, int i) const {
  const FrameRect* f = frame(facing, c, variant, i);
  if (!f) return nullptr;
  const size_t at = (size_t)(f - frames.data());
  return at < anchors.size() ? &anchors[at] : nullptr;
}

const char* holderName(Holder h) {
  static const char* n[] = {"house door h", "house door w", "small door h", "temple door h", "grand door h", "palace door h",
                            "one-storey wall", "chair seat", "stool seat", "bench seat", "throne seat", "table top", "counter top",
                            "horse withers", "rowboat gunwale", "deck rail", "ladder rung step", "stair step", "gate clear",
                            "arch clear", "city gate clear", "fence", "hedge", "garden wall", "cart wheel", "handcart wheel",
                            "grave marker"};
  static_assert(sizeof(n) / sizeof(n[0]) == (size_t)Holder::COUNT, "a name for every holder");
  return (int)h < (int)Holder::COUNT ? n[(int)h] : "?";
}

RigSheet humanRig(const HumanLook& look, RigPart part) {
  switch (part) {
    case RigPart::Loco: return rig2::rigLoco(look);
    case RigPart::Action: return rig2::rigAction(look);
    case RigPart::Death: return rig2::rigDeath(look);
    default: return RigSheet();
  }
}

// (rig2Covers lives in rig/rig_body.cpp: RIG-BODY says which looks the v2 painters cover)

}  // namespace art
