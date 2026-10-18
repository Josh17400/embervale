// EMBERVALE art, v2 rig (RIG-BODY, P2a): faces (brows, 1x2 eyes, nose, mouth, ears, beard, expressions) and hands
// (STYLE_P2 4, 5). The head is a 12 x 12 box (incl. the outline) with a 10 px face; its maps are painted per facing
// (the left facing is its own map, never the right one mirrored: the light stays top-left).
#include "rpg/art/rig/rig_internal.h"

namespace art {
namespace rig2 {

namespace {

// Face maps: 11 rows (the head's top painted row down to the jaw shadow on the neck) x 12 columns (x = head.x + i,
// i.e. the head box incl. its outline columns). Codes:
//   . none          0..4 skin ramp         b brow       L lash (dark upper eye)     I iris (lower eye)
//   n nose shadow   N nose (side, proud)   m mouth      r Rosehip (women's lip)     k blush
//   E ear           e ear's inner shadow    f ear's deep shadow
// Rows: 0 = F-30 (man) .. 4 = brows F-26, 5-6 eyes F-25..F-24, 7 nose F-23, 8 mouth F-22, 9 chin F-21, 10 F-20.
const char* const kFaceManDown[11] = {
    "...222222...",
    "..33222222..",
    ".3332222221.",
    ".3332222221.",
    ".33bb32bb21.",
    ".E33L22L22e.",
    ".e23I22I21e.",
    ".22222n2211.",
    "..222mm221..",
    "...111111...",
    "....0000....",
};
const char* const kFaceWomanDown[11] = {
    "...222222...",
    "..33222222..",
    ".3332222221.",
    ".33c3222c21.",
    ".333b22b221.",
    ".E3LL22LL2e.",
    ".e23I22I21e.",
    ".22k22n2k11.",
    "..222mm221..",
    "....1r11....",
    "....0000....",
};
const char* const kFaceUp[11] = {
    "...222222...",
    "..22222222..",
    ".2222222222.",
    ".2222222222.",
    ".2222222222.",
    ".E22222222e.",
    ".e22222222f.",
    ".2222222221.",
    "..22222221..",
    "...111111...",
    "....1111....",
};
// side views: the face in the front third, the nose 1 px proud, the ear 2 x 2 at the middle, the jaw line
const char* const kFaceManRight[11] = {
    "...222222...",
    "..22222222..",
    ".2222222232.",
    ".2222222332.",
    ".2222223bb1.",
    ".2222Ee2L21.",
    ".2222ee2I21.",
    ".222222222nN",
    "..2222222m1.",
    ".....11111..",
    ".....000....",
};
const char* const kFaceManLeft[11] = {
    "...222222...",
    "..22222222..",
    ".3322222222.",
    ".3332222222.",
    ".3bb3222222.",
    ".32L2eE2222.",
    ".33I2ee2222.",
    "N3322222222.",
    ".3m2222221..",
    "..22111.....",
    "....000.....",
};
const char* const kFaceWomanRight[11] = {
    "...222222...",
    "..22222222..",
    ".2222222232.",
    ".22222223b2.",
    ".2222223b21.",
    ".2222Ee2LL1.",
    ".2222ee2I21.",
    ".22222222knN",
    "..2222222m1.",
    "......1r1...",
    ".....00.....",
};
const char* const kFaceWomanLeft[11] = {
    "...222222...",
    "..22222222..",
    ".3322222222.",
    ".3b32222222.",
    ".33b3222222.",
    ".LL2eE22222.",
    ".33I2ee2222.",
    "Nk332222222.",
    ".3m2222221..",
    "...1r1......",
    ".....00.....",
};

}  // namespace

void paintFace(Fig& f, const Body& B, const Kit& K, const Pose& P, HeadAt h, RigAnchors& an) {
  const int fc = P.facing;
  const bool w = B.woman != 0;
  const char* const* map = fc == FDown ? (w ? kFaceWomanDown : kFaceManDown)
                         : fc == FUp ? kFaceUp
                         : fc == FRight ? (w ? kFaceWomanRight : kFaceManRight)
                                        : (w ? kFaceWomanLeft : kFaceManLeft);
  const int top = h.y + 1;   // the head's top painted row (the crown's outline is h.y)
  // a map row's painted row (a child's 11 px head drops the forehead row; -1 = not painted)
  auto row = [&](int r) { const int p = headRow(B, r); return p < 0 ? -1000 : top + p; };
  // (RIG-BODY, after the slice) the finer features: an elf's brows and nose a step softer, a 4 px chin on an elf man and
  // a child; a child's cheeks blush like a woman's (STYLE_P2 4). None of it touches a human adult's face.
  const bool elf = B.people == 2, kid = B.age != 0;
  const bool narrowChin = !w && B.chinW <= 4;
  for (int r = 0; r < 11; r++) {
    const int y = row(r);
    if (y < -900) continue;
    for (int i = 0; i < 12; i++) {
      char ch = map[r][i];
      if (narrowChin && fc == FDown && r == 9) ch = (i >= 4 && i <= 7) ? '1' : '.';
      if (narrowChin && (fc == FRight || fc == FLeft) && r == 9) {   // the side jaw a px shorter toward the throat
        if (fc == FRight && i == 5) ch = '.';
        if (fc == FLeft && i == 6) ch = '.';
      }
      if (ch == '.') continue;
      uint32_t col;
      switch (ch) {
        case 'b': col = elf ? mix(K.brow, K.skin[2], 0.30f) : K.brow; break;
        case 'c': col = mix(K.brow, K.skin[2], 0.40f); break;   // a brow's soft tail (women's arch)
        case 'L': col = K.lash; break;
        case 'I': col = K.iris; break;
        case 'n': col = elf ? mix(K.skin[1], K.skin[2], 0.35f) : K.skin[1]; break;
        case 'N': col = fc == FLeft ? K.skin[3] : K.skin[2]; break;
        case 'm': col = K.mouth; break;
        case 'r': col = K.rose; break;
        case 'k': col = K.blush; break;
        case 'E': col = K.skin[2]; break;
        case 'e': col = K.skin[1]; break;
        case 'f': col = K.skin[0]; break;
        default: col = K.skin[ch - '0']; break;
      }
      f.set(h.x + i, y, col);
    }
  }
  if (kid && !w && fc == FDown) { f.set(h.x + 3, row(7), K.blush); f.set(h.x + 8, row(7), K.blush); }
  if (kid && !w && fc == FRight) f.set(h.x + 9, row(7), K.blush);
  if (kid && !w && fc == FLeft) f.set(h.x + 2, row(7), K.blush);
  // expressions (STYLE_P2 4): the only face changes
  auto eyeAt = [&](int i, int r) { f.set(h.x + i, row(r), K.skin[2]); };
  if (fc == FDown) {
    const int eyes[2] = {4, 7};
    if (P.expr == ExBlink || P.expr == ExClosed) {
      for (int e : eyes) { eyeAt(e, 5); f.set(h.x + e, row(6), K.lash); }
      if (w) { eyeAt(3, 5); eyeAt(8, 5); f.set(h.x + 3, row(6), K.lash); f.set(h.x + 8, row(6), K.lash); }
    } else if (P.expr == ExHurt) {
      for (int e : eyes) { f.set(h.x + e, row(6), K.skin[2]); }
      f.set(h.x + 3, row(5), K.lash); f.set(h.x + 4, row(5), K.lash);
      f.set(h.x + 7, row(5), K.lash); f.set(h.x + 8, row(5), K.lash);
      f.set(h.x + 5, row(8), mix(K.mouth, kInk, 0.45f)); f.set(h.x + 6, row(8), mix(K.mouth, kInk, 0.45f));
      f.set(h.x + 5, row(9), mix(K.mouth, kInk, 0.25f)); f.set(h.x + 6, row(9), mix(K.mouth, kInk, 0.25f));
    } else if (P.expr == ExEffort) {
      f.set(h.x + 4, row(8), K.mouth); f.set(h.x + 7, row(8), K.mouth);
    }
  } else if (fc == FRight || fc == FLeft) {
    const int e = fc == FRight ? 8 : 3;
    const int m = fc == FRight ? 9 : 2;
    if (P.expr == ExBlink || P.expr == ExClosed) {
      eyeAt(e, 5); f.set(h.x + e, row(6), K.lash);
      if (w) { const int o = fc == FRight ? 9 : 2; eyeAt(o, 5); }
    } else if (P.expr == ExHurt) {
      f.set(h.x + e, row(6), K.skin[2]);
      f.set(h.x + e, row(5), K.lash); f.set(h.x + e + (fc == FRight ? 1 : -1), row(5), K.lash);
      f.set(h.x + m, row(8), mix(K.mouth, kInk, 0.45f)); f.set(h.x + m, row(9), mix(K.mouth, kInk, 0.3f));
    } else if (P.expr == ExEffort) {
      f.set(h.x + m + (fc == FRight ? -1 : 1), row(8), K.mouth);
    }
  }
  if (fc == FDown) { an.faceX = (int16_t)(h.x + 1); an.faceY = (int16_t)row(4); }
  else if (fc == FRight) { an.faceX = (int16_t)(h.x + 5); an.faceY = (int16_t)row(4); }
  else if (fc == FLeft) { an.faceX = (int16_t)(h.x); an.faceY = (int16_t)row(4); }
}

// the peoples' ears over the hair (they poke through it: a half-breed's 1 px tip up, an elf's swept up and back past the
// head's outline), face paint (1 stripes, 2 dots, 3 a mask band, 4 a sun mark) and earrings; after the hair, before the
// headwear (STYLE_P2 4: ears; v1's marks at the v2 scale; nothing outlined inside the face)
void paintFaceMarks(Fig& f, const Body& B, const Kit& K, const Pose& P, HeadAt h) {
  const int fc = P.facing;
  const int top = h.y + 1;
  auto row = [&](int r) { const int p = headRow(B, r); return p < 0 ? -1000 : top + p; };
  auto at = [&](int i, int r, uint32_t c) { if (row(r) > -900) f.set(h.x + i, row(r), c); };
  auto onSkin = [&](int i, int r, uint32_t c) {   // only over the face's skin (never over the hair or an eye)
    const int y = row(r);
    if (y < -900) return;
    const uint32_t u = f.get(h.x + i, y);
    for (int k = 0; k < 5; k++) if (u == K.skin[k] || u == K.blush) { f.set(h.x + i, y, c); return; }
  };
  const bool hooded = K.hood || K.headwear == 1 || K.headwear == 5 || K.headwear == 8;   // the ears are under the cloth
  // ---- ears
  if (B.ear && !hooded) {
    const uint32_t e3 = K.skin[3], e2 = K.skin[2], e1 = K.skin[1];
    if (fc == FDown || fc == FUp) {
      // the ear's own pixels (rows 5-6 at columns 1 and 10) and its tip: half-breed one px up, elf two more out and up
      at(1, 5, e2); at(1, 6, e1); at(10, 5, fc == FDown ? e1 : e2); at(10, 6, e1);
      if (B.ear == 1) { at(0, 5, e2); at(0, 4, e3); at(11, 5, e1); at(11, 4, e2); }
      else { at(0, 5, e2); at(0, 4, e3); at(-1, 3, e3); at(11, 5, e1); at(11, 4, e2); at(12, 3, e2); }
    } else {
      const int s = fc == FRight ? 1 : -1;
      const int ex = fc == FRight ? 5 : 6;   // the ear's front column (the 2 x 2 at the middle of the head)
      at(ex, 5, e2); at(ex + s, 5, e1); at(ex, 6, e1); at(ex + s, 6, K.skin[0]);
      if (B.ear == 1) at(ex, 4, e3);
      else { at(ex, 4, e3); at(ex - s, 3, e3); at(ex - 2 * s, 2, e2); at(ex - s, 4, e2); }
    }
  }
  // ---- face paint (never in the back view)
  if (K.facePaint && fc != FUp) {
    const uint32_t ochre = rgba(176, 58, 40), chalk = rgba(240, 236, 220), woad = rgba(44, 60, 120);
    const bool side = fc != FDown;
    const int s = fc == FLeft ? -1 : 1;
    const int fx = fc == FRight ? 8 : 3;   // the side view's eye column
    switch (K.facePaint) {
      case 1:   // war stripes: two short slanting strokes under each eye
        if (!side) { onSkin(3, 7, ochre); onSkin(4, 7, shade(ochre, 0.9f)); onSkin(7, 7, shade(ochre, 0.9f)); onSkin(8, 7, shade(ochre, 0.8f));
                     onSkin(3, 8, shade(ochre, 0.85f)); onSkin(8, 8, shade(ochre, 0.7f)); }
        else { onSkin(fx, 7, ochre); onSkin(fx - s, 7, shade(ochre, 0.85f)); onSkin(fx - s, 8, shade(ochre, 0.8f)); }
        break;
      case 2:   // chalk dots on the cheeks and the brow
        if (!side) { onSkin(3, 7, chalk); onSkin(8, 7, shade(chalk, 0.85f)); onSkin(5, 3, chalk); onSkin(6, 3, shade(chalk, 0.9f)); }
        else { onSkin(fx - s, 7, chalk); onSkin(fx, 3, chalk); }
        break;
      case 3: {   // a woad band across the eyes: the lash row from temple to temple, a point under each outer corner (the
                  // eyes and the irises stay: a mid blue, never a dark visor)
        const uint32_t wl = mix(woad, rgba(90, 120, 190), 0.55f), wd = mix(woad, rgba(70, 96, 160), 0.4f);
        if (!side) { for (int i = 1; i <= 10; i++) onSkin(i, 5, i < 6 ? wl : wd); onSkin(2, 6, wl); onSkin(9, 6, wd); }
        else { for (int k = -2; k <= 1; k++) onSkin(fx + k * s, 5, k * s < 0 ? wl : wd); onSkin(fx - s, 6, wd); }
        break;
      }
      default:  // a sun mark on the brow: a gold lozenge with four rays
        if (!side) { onSkin(5, 3, K.gold[4]); onSkin(6, 3, K.gold[3]); onSkin(5, 2, K.gold[2]); onSkin(6, 2, K.gold[2]); onSkin(4, 3, K.gold[2]); onSkin(7, 3, K.gold[1]); }
        else { onSkin(fx, 3, K.gold[4]); onSkin(fx + s, 3, K.gold[2]); onSkin(fx, 2, K.gold[2]); }
        break;
    }
  }
  // ---- earrings: a gold drop under each ear lobe (front and back), under the near ear (side)
  if ((K.jewellery & 1) && !hooded) {
    if (fc == FDown || fc == FUp) { at(1, 7, K.gold[fc == FDown ? 4 : 3]); at(10, 7, K.gold[2]); at(0, 8, K.gold[fc == FDown ? 3 : 2]); at(11, 8, K.gold[1]); }
    else { const int ex = fc == FRight ? 5 : 6; at(ex, 7, K.gold[4]); at(ex, 8, K.gold[2]); }
  }
}

// the idle blink from the neutral frame: exactly what paintFace's ExBlink paints, without repainting the frame
Cut blinkOf(const Cut& n, const Body& B, const Kit& K, int facing) {
  Cut b = n;
  if (n.an.faceX == kNoAnchor || facing == FUp) return b;
  const bool w = B.woman != 0;
  auto at = [&](int fx, int fy, uint32_t col) { b.c.set(n.px + n.an.faceX + fx, n.py + n.an.faceY + fy, col); };
  if (facing == FDown) {
    for (int e : {3, 6}) { at(e, 1, K.skin[2]); at(e, 2, K.lash); }
    if (w) { at(2, 1, K.skin[2]); at(7, 1, K.skin[2]); at(2, 2, K.lash); at(7, 2, K.lash); }
  } else {
    at(3, 1, K.skin[2]); at(3, 2, K.lash);
    if (w) at(facing == FRight ? 4 : 2, 1, K.skin[2]);
  }
  return b;
}

// a 2 x 2 fist (STYLE_P2 5): the top-left pixel ramp 3, the rest ramp 2, the bottom-right ramp 1
void paintHand(Fig& f, const Kit& K, int x, int y, bool open) {
  f.set(x, y, K.skin[3]);
  f.set(x + 1, y, K.skin[2]);
  f.set(x, y + 1, K.skin[2]);
  f.set(x + 1, y + 1, K.skin[1]);
  if (open) { f.set(x, y + 2, K.skin[2]); f.set(x + 1, y + 2, K.skin[1]); }
}

}  // namespace rig2
}  // namespace art
