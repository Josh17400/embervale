// EMBERVALE art: the Clear Sight rig contract's shared pieces (art_rig.h; P2a phase A, lead) and, from P2b phase A, the
// dispatch to the DOLL (rpg/art/doll/doll.h): rig 2 draws hand-placed part maps composed by the doll, never the rejected
// procedural painters (rpg/art/rig, detached: their entry points are legacy*). A look the doll does not cover is drawn
// whole on v1 (rig2Covers), so `rig 2` never shows a half-made person.
#include "rpg/art/art_rig.h"
#include "rpg/art/doll/doll.h"

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

RigSheet humanRig(const HumanLook& look, RigPart part) { return doll::compose(look, part); }
RigSheet humanPosture2(const HumanLook& look, Posture p, uint8_t variant) { return doll::composePosture(look, p, variant); }
Canvas sleeper2(const HumanLook& look, Berth b, int kit, int frame) { return doll::sleeper(look, b, kit, frame); }
Canvas humanFigureStill2(const HumanLook& look, int facing) { return doll::figureStill(look, facing); }
// the humanoid monsters on the doll are P2c's (HUMANOIDS): v1's cells until then
Canvas humanoidCell2(int species, int frame) { (void)species; (void)frame; return Canvas(); }
int humanoidCellW2(int species) { (void)species; return 0; }
int humanoidCellH2(int species) { (void)species; return 0; }

// the doll covers what this look needs: LOCO and DEATH always, ACTION when it carries a weapon (the census's fighters)
bool rig2Covers(const HumanLook& look) {
  if (!doll::dollCovers(look, RigPart::Loco) || !doll::dollCovers(look, RigPart::Death)) return false;
  const bool armed = look.weapon >= 1 && look.weapon <= 8;
  return !armed || doll::dollCovers(look, RigPart::Action);
}

}  // namespace art
