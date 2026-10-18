// EMBERVALE art, v2 rig (POSES, P2a): skeleton, draugr and goblin on the v2 skeleton in the monster sheet layout
// (STYLE_P2 15): MONSTER_FRAMES cells of humanoidCellW2 x humanoidCellH2 facing RIGHT (the renderer flips them for left,
// as every monster), columns 0-3 the walk, 4 the wind-up, 5 the strike, 6 hurt, 7 dead on the ground. The feet's sole
// row lands on the cell's row H - 2 (the actor's p.y, as v2 people). The cells come back raw: the monster sheet
// (art_monsters.cpp lookSheet) tints them, lays the variant overlays and adds the ink outer outline, as every monster.
//   skeleton  the adult man's 32 px skeleton in bone: a 12 px skull with an ember in its socket, the spine, the ribs,
//             the pelvis, thin limb bones; a pitted sword and a round shield on the far arm; dead: a heap of bones
//   draugr    a 32 px dead warrior: grey-green skin, long silver hair and beard, mail, a horned helm, cold glowing eyes,
//             a bearded axe; dead: laid on its back, the head east
//   goblin    25 px (0.78 of the man): a youth's big head on short legs, green skin, ears swept back, a mohawk, yellow
//             eyes, rags, a dagger; dead: laid on its back
#include <algorithm>
#include <cmath>
#include "rpg/art/rig/rig_internal.h"

namespace art {
namespace rig2 {
namespace {

constexpr int kSkel = 1, kDraugr = 2, kGoblin = 3;
const int kCellW[4] = {0, 46, 46, 36}, kCellH[4] = {0, 46, 46, 36};

HumanLook speciesLook(int sp) {
  HumanLook L;
  switch (sp) {
    case kDraugr:
      L.skin = rgba(120, 132, 118); L.hairColor = rgba(128, 130, 122); L.hair = Hair::Long;
      L.topColor = rgba(84, 82, 82); L.bottomColor = rgba(60, 54, 50); L.weapon = 2; L.weaponColor = rgba(118, 140, 120);
      L.eyeColor = rgba(120, 240, 220);
      break;
    case kGoblin:
      L.skin = rgba(124, 160, 72); L.hairColor = rgba(60, 40, 40); L.hair = Hair::Short; L.outfit = Outfit::Rags;
      L.weapon = 5; L.weaponColor = rgba(170, 160, 150); L.age = 2; L.eyeColor = rgba(250, 210, 60);
      break;
    default:
      L.weapon = 1; L.weaponColor = rgba(150, 128, 104); L.tabardColor = rgba(110, 80, 60);
      break;
  }
  return L;
}

Body speciesBody(int sp) {
  HumanLook L = speciesLook(sp);
  Body B = bodyOf(L);
  if (sp == kGoblin) {   // a youth's body a row shorter in the leg: 25 px
    B.legs -= 1; B.hip += 1; B.shoulder += 1; B.neck += 1; B.headTop += 1;
  }
  return B;
}

// the side walk's 4 frames (contact, pass, contact, pass) and the actions: the near hand holds the weapon
struct Key { int8_t fn, ln, ff, lf, dy, hx, hy, ox, oy, dir; uint8_t ofront; int8_t lean, expr; };
//                    near foot / lift, far foot / lift, bob, weapon hand, off hand, the weapon's direction, the off arm
//                    in front, the lean, the face
const Key kKeys[7] = {
    {6, 0, -6, 0, 0, 1, -10, -1, -10, 1, 0, 0, ExNeutral},      // 0 contact: the near foot forward
    {-1, 0, 2, 2, 1, 0, -9, 0, -9, 1, 0, 0, ExNeutral},         // 1 pass
    {-6, 0, 6, 0, 0, -1, -10, 1, -10, 1, 0, 0, ExNeutral},      // 2 contact: the far foot forward
    {2, 2, -1, 0, 1, 0, -9, 0, -9, 1, 0, 0, ExNeutral},         // 3 pass
    {3, 0, -4, 0, 0, -6, -24, 3, -15, 11, 1, -1, ExNeutral},    // 4 wind-up: raised back over the shoulder
    {5, 0, -4, 0, 1, 6, -14, 2, -13, 2, 1, 2, ExEffort},        // 5 strike: brought down in front
    {-1, 0, -4, 0, 0, -3, -15, 3, -16, 13, 0, -1, ExHurt},      // 6 hurt: knocked back
};

PoseX keyPose(const Body& B, int frame) {
  const Key& k = kKeys[std::clamp(frame, 0, 6)];
  PoseX X;
  X.p = restPose(B, FRight, false);
  X.legs = LegStand;
  const float sc = B.stride / 7.0f;
  X.p.fRx = (int8_t)std::lround(k.fn * sc); X.p.fRy = k.ln;
  X.p.fLx = (int8_t)std::lround(k.ff * sc); X.p.fLy = k.lf;
  X.p.dy = k.dy;
  const int wy = B.hip + 11;   // the authoring rows are the man's; a shorter body's hands come down with it
  X.p.hRx = k.hx; X.p.hRy = (int8_t)(k.hy + wy);
  X.p.hLx = k.ox; X.p.hLy = (int8_t)(k.oy + wy); X.p.hLfront = k.ofront;
  X.p.lean = k.lean; X.p.expr = (uint8_t)k.expr;
  X.p.wDir = k.dir;
  if (frame == 4) X.p.elbowOut = 1;
  if (frame == 6) X.p.headX = -1;
  X.fr = (int8_t)frame;
  return X;
}

// the weapon in the near hand (RIG-ARMS' base weapons), the hand drawn over its grip; the draugr's helm, beard and
// eyes, the goblin's ears and mohawk
struct HumCtx { int sp; };
void humProps(Fig& f, const Body& B, const Kit& K, const PoseX& X, int layer, const void* ctx) {
  const int sp = ((const HumCtx*)ctx)->sp;
  const Pose& P = X.p;
  if (layer == PropGrip && P.wDir >= 0) {
    int tx, ty;
    paintArms(f, K, K.weapon, P.hRx + 1.0f - 0.5f, P.hRy + 1.0f - 0.5f, P.wDir, 0, 1, tx, ty);
  }
  if (layer != PropHead) return;
  const int hx = P.hipX + P.lean - 6 + P.headX;            // the head box's left (as the painter)
  const int top = B.headTop + P.dy + P.chest + P.headY;   // the head's top painted row
  if (sp == kDraugr) {
    const Ramp5 I = r5(ramp5(rgba(36, 36, 44), rgba(58, 58, 64), rgba(84, 82, 82), rgba(112, 108, 100), rgba(140, 136, 124)));
    const Ramp5 Bn = r5(kBone);
    // the helm: an iron cap over the crown, a brow band, the nasal down the face's front
    for (int r = -1; r <= 3; r++)
      for (int i = 1; i <= 10; i++) {
        if (r == -1 && (i < 3 || i > 8)) continue;
        if (r == 0 && (i < 2 || i > 9)) continue;
        int k = 2;
        if (r <= 0) k = 3;
        if (i <= 2) k = std::min(4, k + 1);
        if (i >= 9) k = std::max(0, k - 1);
        if (r == 3) k = 1;   // the brow band's shadow line
        f.set(hx + i, top + r, I[k]);
      }
    f.set(hx + 4, top - 1, I[4]);                                       // the helm's lit crown
    f.set(hx + 9, top + 4, I[2]); f.set(hx + 9, top + 5, I[1]); f.set(hx + 9, top + 6, I[1]);   // the nasal
    // the horns: up and back from the helm's sides, bone lit top-left
    // (they curve out from the helm's sides and up: a short hook, so the draugr stands a man's height with its helm)
    f.set(hx + 1, top, Bn[3]); f.set(hx, top - 1, Bn[4]); f.set(hx, top - 2, Bn[3]); f.set(hx + 1, top - 3, Bn[2]);
    f.set(hx + 9, top - 1, Bn[3]); f.set(hx + 10, top - 2, Bn[4]); f.set(hx + 10, top - 3, Bn[2]);
    // the beard: down from the jaw over the collar, silver, in strands
    for (int r = 8; r <= 11; r++)
      for (int i = 6; i <= 10; i++) {
        if (r >= 10 && (i < 7 || i > 9)) continue;
        if (r == 8 && i == 9) continue;   // the mouth's corner shows
        const int k = (i + r) & 1 ? 2 : 3;
        f.set(hx + i, top + r, K.hair[r == 11 ? 1 : k]);
      }
  } else if (sp == kGoblin) {
    const Ramp5& S = K.skin;
    // the big ears swept back from the side of the head
    f.set(hx + 5, top + 4, S[3]); f.set(hx + 4, top + 3, S[3]); f.set(hx + 3, top + 2, S[2]); f.set(hx + 2, top + 1, S[2]);
    f.set(hx + 4, top + 5, S[1]); f.set(hx + 3, top + 4, S[1]); f.set(hx + 2, top + 2, S[1]); f.set(hx + 1, top + 1, S[2]);
    // the mohawk: a ridge of dark hair over the crown
    for (int i = 3; i <= 8; i++) { f.set(hx + i, top - 1, K.hair[i == 3 ? 3 : 2]); f.set(hx + i, top - 2 + ((i & 1) ? 0 : 1), K.hair[i < 6 ? 3 : 1]); }
    // the hooked nose
    f.set(hx + 11, top + 7, S[2]); f.set(hx + 11, top + 8, S[1]);
  }
}

// the skeleton: the posed figure in bone (side view), the ember in its socket, the sword and the round shield
void paintSkeleton(Fig& f, const Body& B, const Kit& K, const PoseX& X, int frame) {
  const Pose& P = X.p;
  const Ramp5 Bn = r5(kBone);
  const uint32_t hollow = mix(Bn[0], kInk, 0.45f);
  const int cx = P.hipX, H = B.hip + P.dy, S = B.shoulder + P.dy + P.chest, sx = cx + P.lean;
  auto legBones = [&](int fx, int lift, int bias) {
    const float hx = cx + 0.5f, hy = H + 1.0f, ax = fx + 0.5f, ay = -2.0f - lift;
    const float kx = (hx + ax) * 0.5f + 1.0f, ky = (hy + ay) * 0.5f;
    f.begin(); limb(f, hx, hy, kx, ky, 2, Bn, bias); limb(f, kx, ky, ax, ay, 1, Bn, bias);
    f.set((int)std::floor(kx), (int)std::floor(ky), Bn[3 + bias]);                // the knee
    const int fy = (int)std::floor(ay) + 1, fxI = (int)std::floor(ax);
    for (int k = -1; k <= 2; k++) f.set(fxI + k, fy, Bn[k == -1 ? 2 + bias : (k == 2 ? 1 + bias : 2 + bias)]);   // the foot
    f.set(fxI, fy - 1, Bn[3 + bias]);
  };
  auto armBones = [&](int hx, int hy, int bias, bool farArm) {
    const float shx = sx - (farArm ? 0.0f : 1.0f), shy = S + 1.5f, wx = hx + 1.0f, wy = hy < S - 1 ? hy + 2.0f : hy - 0.5f;
    const float ex = (shx + wx) * 0.5f - 1.0f, ey = (shy + wy) * 0.5f + 1.0f;
    f.begin(); limb(f, shx, shy, ex, ey, 1, Bn, bias); limb(f, ex, ey, wx, wy, 1, Bn, bias);
    f.set(hx, hy, Bn[3 + bias]); f.set(hx + 1, hy, Bn[2 + bias]); f.set(hx + 1, hy + 1, Bn[1 + bias]); f.set(hx, hy + 1, Bn[2 + bias]);   // the bony fist
  };
  // the far arm, the far leg
  armBones(P.hLx, P.hLy, -1, true);
  legBones(P.fLx, P.fLy, -1);
  legBones(P.fRx, P.fRy, 0);
  f.begin();
  // the pelvis: the hip blades and a dark hollow
  for (int x = cx - 3; x <= cx + 2; x++) { f.set(x, H - 1, Bn[x == cx - 3 ? 4 : 3]); f.set(x, H, Bn[x >= cx + 1 ? 1 : 2]); }
  f.set(cx - 1, H, hollow); f.set(cx, H - 1, Bn[2]);
  // the spine (vertebrae: alternate lit and shaded) from the pelvis to the skull
  for (int y = S - 1; y <= H - 2; y++) { f.set(sx - 2 + (y > S + 6 ? 0 : 0), y, Bn[(y & 1) ? 3 : 1]); }
  // the ribcage: ribs from the spine round to the breastbone, hollows between them
  for (int r = 0; r < 6; r++) {
    const int y = S + 1 + r;
    const int front = r < 4 ? 3 : 2;
    for (int x = sx - 1; x <= sx + front; x++) f.set(x, y, (r & 1) ? hollow : Bn[x == sx - 1 ? 4 : (x == sx + front ? 2 : 3)]);
    f.set(sx + front, y, Bn[(r & 1) ? 1 : 2]);
  }
  // the collarbone and the shoulder
  for (int x = sx - 3; x <= sx + 3; x++) f.set(x, S, Bn[x <= sx - 2 ? 4 : 3]);
  // the skull (side view, facing right): the round cranium lit top-left, the eye socket with its ember, the nose hole,
  // the teeth, the jaw
  static const char* const kSkull[12] = {
      "...23332....",
      "..2334443...",
      ".233344443..",
      ".2333444432.",
      ".23333300032",
      ".23333300E32",
      ".22333332012",
      "..2233333212",
      "...22T.T.T1.",
      "...1222222..",
      "....11111...",
      "............",
  };
  const int hx = sx - 6 + P.headX, ht = B.headTop - 1 + P.dy + P.chest + P.headY;   // the skull's crown a row over a man's hair
  for (int r = 0; r < 12; r++)
    for (int i = 0; i < 12; i++) {
      const char ch = kSkull[r][i];
      if (ch == '.') continue;
      uint32_t col;
      if (ch == 'E') col = frame == 6 || frame == 7 ? hollow : rgba(255, 96, 60);   // the ember (out when struck)
      else if (ch == 'T') col = Bn[4];
      else if (ch == '0') col = hollow;
      else col = Bn[ch - '0'];
      f.set(hx + i, ht + r, col);
    }
  // the round shield on the far arm, held before the body; the sword in the near hand over it
  {
    f.begin();
    const Ramp5 Wd = r5(ramp(rgba(110, 80, 60))), Ir = r5(kIron);
    const float scx = sx + 5.5f, scy = S + 6.0f;
    for (int y = (int)scy - 5; y <= (int)scy + 5; y++)
      for (int x = (int)scx - 3; x <= (int)scx + 2; x++) {
        const float u = (x + 0.5f - scx) / 3.2f, v = (y + 0.5f - scy) / 5.4f;
        const float d = u * u + v * v;
        if (d > 1.0f) continue;
        int k = u + v < -0.5f ? 3 : (u + v > 0.6f ? 1 : 2);
        const bool rim = d > 0.62f;
        f.set(x, y, rim ? Ir[k] : Wd[(x + y) % 4 == 0 ? std::max(0, k - 1) : k]);
      }
    f.set((int)scx, (int)scy, Ir[4]); f.set((int)scx - 1, (int)scy, Ir[3]);   // the boss
    f.contour(0.6f, 0.2f);
  }
  if (P.wDir >= 0) {
    f.begin();
    int tx, ty;
    paintArms(f, K, 1, P.hRx + 1.0f - 0.5f, P.hRy + 1.0f - 0.5f, P.wDir, 0, 1, tx, ty);
    f.contour(0.6f, 0.2f);
  }
  armBones(P.hRx, P.hRy, 0, false);
}

// a heap of bones (the skeleton's dead frame): the skull on top, ribs, long bones, the sword beside it
void boneHeap(Fig& f, const Kit& K) {
  const Ramp5 Bn = r5(kBone);
  const uint32_t hollow = mix(Bn[0], kInk, 0.45f);
  f.begin();
  limb(f, -11.0f, -2.5f, 2.0f, -3.5f, 2, Bn, 0);    // a thigh bone
  limb(f, -4.0f, -1.5f, 9.0f, -1.5f, 2, Bn, -1);     // a shin
  limb(f, 4.0f, -4.5f, 11.0f, -6.5f, 1, Bn, 0);      // an arm bone
  for (int r = 0; r < 4; r++)                         // the ribcage, fallen on its side
    for (int x = -3; x <= 3; x++) f.set(x, -5 - r, (r & 1) ? hollow : Bn[x == -3 ? 4 : (x == 3 ? 1 : 3)]);
  for (int x = -6; x <= -1; x++) f.set(x, -1, Bn[x == -6 ? 3 : 2]);   // the pelvis
  // the skull, tipped back on the heap, its socket dark
  static const char* const kTop[7] = {"..2334..", ".233443.", "23334443", "23300342", ".230023.", "..2TT2..", "...11..."};
  for (int r = 0; r < 7; r++)
    for (int i = 0; i < 8; i++) {
      const char ch = kTop[r][i];
      if (ch == '.') continue;
      f.set(5 + i, -9 + r, ch == '0' ? hollow : (ch == 'T' ? Bn[4] : Bn[ch - '0']));
    }
  f.contour(0.6f, 0.2f);
  f.begin();
  int tx, ty;
  paintArms(f, K, 1, -13.5f, -0.5f, 0, 0, 1, tx, ty);   // the sword on the ground beside it
}

// the cell: the raw figure painted round its pivot, then placed so the sole row lands on row H - 2
Canvas toCell(const Canvas& raw, int pivX, int pivY, int cw, int ch) {
  Canvas cell(cw, ch);
  for (int y = 0; y < raw.h; y++)
    for (int x = 0; x < raw.w; x++) {
      const uint32_t p = raw.get(x, y);
      if (chA(p)) cell.set(x - pivX + cw / 2, y - pivY + ch - 2, p);
    }
  return cell;
}

// a body laid on its back, the head east (a quarter turn clockwise of the right-facing figure lying flat: the monster
// faces right, so its head lands at the right end, as v1's dead frame)
Canvas laidOut(const HumanLook& L, const Body& B, const Kit& K, const HumCtx& ctx, int cw, int ch) {
  PoseX X;
  X.p = restPose(B, FRight, false);
  X.p.expr = ExClosed;
  X.p.hRx = 1; X.p.hRy = (int8_t)(B.hip - 1);
  X.p.hLx = -3; X.p.hLy = (int8_t)(B.hip + 1); X.p.hLfront = 0;
  X.p.fRx = 2; X.p.fLx = -2;
  X.legs = LegStand;
  X.prop = humProps; X.ctx = &ctx;
  RigAnchors an;
  const int W = LOCO_W, H = 56, feet = LOCO_FEET;
  const Canvas raw = paintPoseRaw(L, B, K, X, W, H, feet, an);
  // a quarter turn counter-clockwise: (x, y) -> (y, W - 1 - x): the head (top) goes right... the face (right) goes up
  Canvas t(H, W);
  for (int y = 0; y < H; y++)
    for (int x = 0; x < W; x++) t.set(y, W - 1 - x, raw.get(x, y));
  // its back row (the figure's rear: x small in raw -> large y in t) is the lowest row; centre it in the cell
  int x0, y0, x1, y1;
  bounds(t, x0, y0, x1, y1);
  Canvas cell(cw, ch);
  if (x1 < x0) return cell;
  const int ox = (cw - (x1 - x0 + 1)) / 2 - x0, oy = (ch - 3) - y1;
  for (int y = y0; y <= y1; y++)
    for (int x = x0; x <= x1; x++) {
      const uint32_t p = t.get(x, y);
      if (chA(p)) cell.set(x + ox, y + oy, p);
    }
  return cell;
}

}  // namespace
}  // namespace rig2

int humanoidCellW2(int species) { return species >= 1 && species <= 3 ? rig2::kCellW[species] : 0; }
int humanoidCellH2(int species) { return species >= 1 && species <= 3 ? rig2::kCellH[species] : 0; }

Canvas humanoidCell2(int species, int frame) {
  using namespace rig2;
  if (species < 1 || species > 3) return Canvas();
  const int cw = kCellW[species], ch = kCellH[species];
  const HumanLook L = speciesLook(species);
  const Body B = speciesBody(species);
  Kit K;
  makeKit(L, K);
  if (species == kDraugr) {   // mail over a dark tunic, the sleeves too
    K.top = r5(ramp5(rgba(36, 36, 44), rgba(58, 58, 64), rgba(84, 82, 82), rgba(112, 108, 100), rgba(140, 136, 124)));
    K.sleeve = K.top;
  }
  if (species == kGoblin) { K.top = r5(kLeather); K.leg = r5(kLeather); }
  const int W = LOCO_W, H = LOCO_H, feet = LOCO_FEET;
  const HumCtx ctx{species};
  if (species == kSkel) {
    Fig f(W, H, W / 2, feet);
    if (frame == 7) boneHeap(f, K);
    else {
      PoseX X = keyPose(B, frame);
      paintSkeleton(f, B, K, X, frame);
    }
    Canvas raw(W, H);
    raw.px = f.px;
    return toCell(raw, W / 2, feet, cw, ch);
  }
  if (frame == 7) return laidOut(L, B, K, ctx, cw, ch);
  PoseX X = keyPose(B, frame);
  if (species == kDraugr && frame <= 3) X.p.wDir = 14;   // the axe carried up at the shoulder on the march
  X.prop = humProps;
  X.ctx = &ctx;
  RigAnchors an;
  const Canvas raw = paintPoseRaw(L, B, K, X, W, H, feet, an);
  return toCell(raw, W / 2, feet, cw, ch);
}

}  // namespace art
