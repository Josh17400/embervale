// EMBERVALE art: the 1x outline masks (P2c forge round 2, FORGE-DOLL; docs/P2C_CONTRACTS.md 3c.1, PLAN gates C16 and C18).
// A figure composed as the game draws it (LOCO, no scaling), with the colour removed and nothing held (ComposeOpts::grey +
// noHeld), at idle D, idle R and the walk's passing frame R; each frame's opaque pixels aligned on the pivot and split at the
// neck (the head box's bottom row: the HEAD above, the BODY below). OD = the XOR over the union of two masks, per part,
// averaged over the three frames. The shape table's solver, the gates C16o and C18 and the boards read these.
#include <algorithm>
#include <cstdlib>
#include "rpg/art/doll/doll.h"

namespace art {
namespace doll {

namespace {
const struct { int facing; Clip clip; int i; } kOutlineFrames[3] = {{0, Clip::Idle, 0}, {2, Clip::Idle, 0}, {2, Clip::Walk, 1}};
}  // namespace

Outline outlineOf(const DollRecipe& r) {
  Outline o;
  ComposeOpts op;
  op.grey = true;
  op.noHeld = true;
  RigSheet sheets[2];
  for (int k = 0; k < 2; k++) {
    op.onlyFacing = k == 0 ? 0 : 2;
    sheets[k] = compose(r, RigPart::Loco, &op);
  }
  o.ok = true;
  for (int f = 0; f < 3; f++) {
    const RigSheet& s = sheets[kOutlineFrames[f].facing == 0 ? 0 : 1];
    const FrameRect* fr = s.frame(kOutlineFrames[f].facing, kOutlineFrames[f].clip, 0, kOutlineFrames[f].i);
    OutlineFrame& of = o.f[f];
    if (!fr || fr->w <= 0) { o.ok = false; continue; }
    of.x0 = -fr->px; of.y0 = -fr->py; of.w = fr->w; of.h = fr->h;
    of.m.assign((size_t)fr->w * fr->h, 0);
    for (int y = 0; y < fr->h; y++)
      for (int x = 0; x < fr->w; x++)
        if ((s.atlas.get(fr->x + x, fr->y + y) >> 24) != 0) of.m[(size_t)y * fr->w + x] = 1;
    const RigAnchors* an = s.anchorsOf(kOutlineFrames[f].facing, kOutlineFrames[f].clip, 0, kOutlineFrames[f].i);
    of.neck = an && an->headY != kNoAnchor ? an->headY + an->headH : -19;
  }
  return o;
}

namespace {
// the XOR over the union of the two masks' pixels above (head) or from (body) each one's neck row
double partOD(const OutlineFrame& a, const OutlineFrame& b, bool head) {
  if (a.m.empty() || b.m.empty()) return 1.0;
  const int x0 = std::min(a.x0, b.x0), y0 = std::min(a.y0, b.y0), x1 = std::max(a.x0 + a.w, b.x0 + b.w), y1 = std::max(a.y0 + a.h, b.y0 + b.h);
  long uni = 0, xr = 0;
  for (int y = y0; y < y1; y++)
    for (int x = x0; x < x1; x++) {
      auto at = [&](const OutlineFrame& f) {
        const int fx = x - f.x0, fy = y - f.y0;
        if (fx < 0 || fy < 0 || fx >= f.w || fy >= f.h) return false;
        if (head ? y >= f.neck : y < f.neck) return false;
        return f.m[(size_t)fy * f.w + fx] != 0;
      };
      const bool pa = at(a), pb = at(b);
      if (pa || pb) uni++;
      if (pa != pb) xr++;
    }
  return uni ? (double)xr / (double)uni : 0.0;
}
}  // namespace

OutlineOD outlineDistance(const Outline& a, const Outline& b) {
  OutlineOD d;
  d.bodyMin = d.headMin = 1.0;
  for (int f = 0; f < 3; f++) {
    const double h = partOD(a.f[f], b.f[f], true), bo = partOD(a.f[f], b.f[f], false);
    d.head += h / 3.0;
    d.body += bo / 3.0;
    d.bodyMin = std::min(d.bodyMin, bo);
    d.headMin = std::min(d.headMin, h);
  }
  return d;
}

int outlinePixels(const Outline& o, bool head) {
  int n = 0;
  for (int f = 0; f < 3; f++) {
    const OutlineFrame& of = o.f[f];
    for (int y = 0; y < of.h; y++)
      for (int x = 0; x < of.w; x++)
        if (of.m[(size_t)y * of.w + x] && (head ? of.y0 + y < of.neck : of.y0 + y >= of.neck)) n++;
  }
  return n;
}

}  // namespace doll
}  // namespace art
