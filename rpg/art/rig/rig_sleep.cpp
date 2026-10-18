// EMBERVALE art, v2 rig (POSES, P2a): the sleeper fitted to each of the 10 berths (STYLE_P2 15, 16): the head on the
// pillow (hats off, eyes shut), the shoulders and hands over the turned-down sheet, the body a soft mound under the
// berth's own blanket (shading only, so every quilt colour shows through), drawn at 0.8 of its length north-south.
// The berth's geometry is HOLDERS' (art_holders2.h berth2: the canvas, the face's centre, the blanket's rows), so the
// sleeper lands on the pillow HOLDERS painted.
//   north-south berths  the head is the facing-down head (the face up, the crown to the headboard); the shoulders and
//                       the chest's top above the blanket, the hands resting on its edge
//   across berths       (hammock, mat, bedroll, the bare ground: head west) the left-facing side figure laid on its back
//                       (its exact transpose, as the corpse: the face to the sky, the light still top-left), the blanket
//                       from the shoulders; a hammock's sag bends the body; the ground brings its own blanket
#include <algorithm>
#include <cmath>
#include "rpg/art/art_holders2.h"
#include "rpg/art/rig/rig_internal.h"

namespace art {
namespace rig2 {
namespace {

// the soft mound of a body under a blanket (alpha-only light and shade over whatever cloth is below), along an axis:
// from u0 (the shoulders) to u1 (the feet), centred on c across it, half-width hw tapering to the feet; across: the axis
// runs along x (the body lies west-east), else along y
void mound(Canvas& c, float cx, int u0, int u1, float hw, int frame, bool across) {
  const uint32_t lit = withA(kLightHue, 0), dark = withA(kShadowHue, 0);
  const int n = std::max(3, u1 - u0 + 1);
  for (int u = u0; u <= u1; u++) {
    const float t = (u - u0 + 0.5f) / n;
    float w = hw * (1.0f - 0.30f * t);
    if (t < 0.10f) w *= 0.88f + t;
    const bool chest = t > 0.06f && t < 0.36f;
    for (int v = (int)std::floor(cx - w - 1); v <= (int)std::ceil(cx + w + 1); v++) {
      const float q = (v + 0.5f - cx) / w;   // -1 the lit (west / north) flank .. 1 the shaded one
      int a = 0;
      uint32_t col = 0;
      if (q < -1.0f || q > 1.25f) continue;
      if (q > 1.0f) { col = dark; a = 104; }
      else if (q > 0.55f) { col = dark; a = 60 + (int)((q - 0.55f) * 90); }
      else if (q < -0.62f) { col = lit; a = 80; }
      else if (q < -0.2f && chest) { col = lit; a = (frame & 1) ? 76 : 46; }   // the chest rises and falls
      if (t > 0.86f && std::fabs(q) < 0.95f) {                                 // the feet: two humps, their tips lit
        const bool tip = u == u1 - 1 && (std::fabs(q + 0.45f) < 0.25f || std::fabs(q - 0.35f) < 0.25f);
        if (tip) { col = lit; a = 92; }
        else if (u == u1) { col = dark; a = 92; }
      }
      if (!a) continue;
      const int x = across ? u : v, y = across ? v : u;
      const uint32_t under = c.get(x, y);
      if (!chA(under)) c.set(x, y, withA(col, a));
    }
  }
}

// the berth's geometry at rig 2: HOLDERS' numbers (art_holders2.h berth2, from the painters' own constants)
holders::Berth2 geo(Berth b, int kit) { return holders::berth2(b, kit); }

// the sleeping head and shoulders, facing up out of the bed (north-south berths): the facing-down figure with its eyes
// shut and its hands on the chest, cut at the blanket's edge; outlined in ink round its own silhouette
Canvas headAndShoulders(const HumanLook& look, const Body& B, const Kit& K, int frame, int& pivX, int& pivY, int& chinRow, int& handRow) {
  PoseX X;
  X.p = restPose(B, FDown, false);
  X.p.expr = ExClosed;
  X.p.chest = (int8_t)(frame & 1 ? -1 : 0);   // the breath
  X.legs = LegNone;
  X.p.hRx = (int8_t)(-B.torsoHalf + 1); X.p.hLx = (int8_t)(B.torsoHalf - 3);
  X.p.hRy = X.p.hLy = (int8_t)(B.shoulder + 5);
  RigAnchors an;
  const int W = LOCO_W, H = LOCO_H, feet = LOCO_FEET;
  Canvas raw = paintPoseRaw(look, B, K, X, W, H, feet, an);
  pivX = W / 2; pivY = feet;
  chinRow = an.headY + B.headRows - 1;     // pivot-relative
  handRow = B.shoulder + 5;
  return raw;
}

}  // namespace
}  // namespace rig2

BedFit bedFit2(Berth b, int kit) {
  using namespace rig2;
  const holders::Berth2 g = geo(b, kit);
  BedFit f;
  f.w = (int8_t)g.w; f.h = (int8_t)g.h;
  f.headX = (int8_t)g.headX; f.headY = (int8_t)g.headY;
  f.across = g.across;
  f.len = (int8_t)std::max(0, g.quiltEnd - g.quiltTop);
  if (!g.across) {
    // the generic Sleep posture (humanPosture2(look, Sleep): the classic bed's sleeper, its pivot at its canvas's
    // bottom-centre): the actor's p so that its head lies on this berth's pillow
    const holders::Berth2 bed = geo(Berth::Bed, -1);
    const int relX = bed.headX - bed.w / 2, relY = bed.headY - (bed.h - 1);
    f.ax = (int8_t)(-g.w / 2 + g.headX - relX);
    f.ay = (int8_t)(-g.h + g.headY - relY);
  }
  return f;
}

Canvas sleeper2(const HumanLook& look, Berth b, int kit, int frame) {
  using namespace rig2;
  const holders::Berth2 g = geo(b, kit);
  if (g.w <= 0 || g.h <= 0) return Canvas();
  HumanLook L = look;
  L.helmet = false; L.helmStyle = 0; L.hood = false; L.headwear = 0;   // hats and helmets off
  const Body B = bodyOf(L);
  Kit K;
  makeKit(L, K);
  Canvas out(g.w, g.h);
  if (!g.across) {
    int pivX, pivY, chin, handRow;
    const Canvas raw = headAndShoulders(L, B, K, frame, pivX, pivY, chin, handRow);
    // place the raw figure so the face's centre lands on (headX, headY): the face's centre is the head box's middle
    const int headTop = B.headTop - 1;                    // the crown's outline row (pivot-relative)
    const int faceMidY = headTop + B.headRows / 2;        // the eyes' row
    const int dx = g.headX - pivX, dy = g.headY - (pivY + faceMidY);
    // the cut: the head, the shoulders and the chest's top above the blanket (its first row = quiltTop), the hands on it
    const int cut = g.quiltTop + 1;                       // canvas row: the last row of body shown
    Canvas part(g.w, g.h);
    for (int y = 0; y < raw.h; y++)
      for (int x = 0; x < raw.w; x++) {
        const uint32_t p = raw.get(x, y);
        if (!chA(p)) continue;
        const int cx = x + dx, cy = y + dy;
        if (cy > cut) continue;
        part.set(cx, cy, p);
      }
    // the lower bunk's sleeper lies in the upper deck's shade
    if (b == Berth::BunkLow)
      for (int y = 0; y < part.h; y++)
        for (int x = 0; x < part.w; x++) {
          const uint32_t p = part.get(x, y);
          if (chA(p)) part.set(x, y, darken(p, 0.42f));
        }
    outlineInk(part);
    // the blanket's mound from the chest to the feet
    mound(out, g.w * 0.5f, g.quiltTop, g.quiltEnd, (float)B.torsoHalf + 1.0f, frame, false);
    for (int y = 0; y < out.h; y++)
      for (int x = 0; x < out.w; x++)
        if (chA(part.get(x, y))) out.set(x, y, part.get(x, y));
    return out;
  }
  // across: the left-facing side figure laid on its back (transposed: the head west, the face to the sky)
  PoseX X;
  X.p = restPose(B, FLeft, false);
  X.p.expr = ExClosed;
  X.legs = LegNone;
  X.p.hLx = -3; X.p.hLy = (int8_t)(B.shoulder + 6);   // the near hand on the chest
  X.p.hRx = -1; X.p.hRy = (int8_t)(B.shoulder + 6); X.p.hRfront = 0;
  X.p.chest = (int8_t)(frame & 1 ? -1 : 0);
  RigAnchors an;
  const int W = LOCO_W, H = LOCO_H, feet = LOCO_FEET;
  const Canvas raw = paintPoseRaw(L, B, K, X, W, H, feet, an);
  // the transposed figure: (x, y) -> (y, x); the face's centre (the side head's eye) lands on (headX, headY)
  const int eyeX = an.faceX != kNoAnchor ? an.faceX + 3 : -4, eyeY = an.faceY != kNoAnchor ? an.faceY + 2 : B.headTop + 6;
  const int ox = g.headX - (feet + eyeY), oy = g.headY - (W / 2 + eyeX);
  const int cutU = feet + B.shoulder + 7;
  const int bodyAxis = W / 2 + oy;            // the body's middle (the standing figure's centre column) once laid down   // standing rows below this (the chest down) are under the blanket
  Canvas part(g.w, g.h);
  auto sag = [&](int x) {
    if (b != Berth::Hammock) return 0;
    const float t = std::clamp((x - 2) / (float)std::max(1, g.w - 5), 0.0f, 1.0f);
    return (int)std::lround(std::sin(t * 3.14159f) * 6.0f) - 3;
  };
  for (int y = 0; y < raw.h && y <= cutU; y++)
    for (int x = 0; x < raw.w; x++) {
      const uint32_t p = raw.get(x, y);
      if (!chA(p)) continue;
      const int cx = y + ox;
      part.set(cx, x + oy + sag(cx), p);
    }
  // the ground's own blanket (the cloak, or a drab wool) over the body; the berths bring theirs
  if (b == Berth::Ground) {
    const Ramp Bl = ramp(L.cloak ? L.cloakColor : rgba(112, 96, 78));
    const int x0 = g.quiltTop, x1 = g.quiltEnd, cy = bodyAxis;
    Canvas bl(g.w, g.h);
    for (int x = x0; x <= x1; x++) {
      const float t = (x - x0) / (float)std::max(1, x1 - x0);
      float hw = (B.torsoHalf + 1.5f) * (1.0f - 0.22f * t);
      if (x - x0 < 2) hw *= x == x0 ? 0.62f : 0.86f;   // the blanket's ends round off (the folded top, the feet)
      if (x1 - x < 2) hw *= x == x1 ? 0.55f : 0.82f;
      for (int y = (int)std::floor(cy - hw); y <= (int)std::ceil(cy + hw); y++) {
        const float q = (y + 0.5f - cy) / hw;
        if (std::fabs(q) > 1.0f) continue;
        int k = q < -0.5f ? 3 : (q > 0.45f ? 1 : 2);
        if (x == x0) k = 4;
        if (x == x1) k = std::max(0, k - 1);
        if (((x + y) % 5) == 0 && k > 1) k--;   // a coarse weave
        bl.set(x, y, Bl[k]);
      }
    }
    outlineInk(bl);
    for (size_t i = 0; i < out.px.size(); i++) if (chA(bl.px[i])) out.px[i] = bl.px[i];
  }
  outlineInk(part);
  mound(out, (float)bodyAxis, g.quiltTop, g.quiltEnd, (float)B.torsoHalf + 0.5f, frame, true);
  for (int y = 0; y < out.h; y++)
    for (int x = 0; x < out.w; x++)
      if (chA(part.get(x, y))) out.set(x, y, part.get(x, y));
  return out;
}

}  // namespace art
