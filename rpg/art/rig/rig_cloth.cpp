// EMBERVALE art, v2 rig (RIG-BODY, P2a): Tunic, Dress, Robe (hood), Rags, the 8 culture cuts and patterns, the 8 culture
// headwear kinds and the legacy hood, the torc (STYLE_P2 2, 7). The torso is a box lit on its left plane; clothes hang:
// hems are lit, folds are single-pixel shadow lines at the waist and the knees.
//   cuts (cult::Cut + 1)  1 tunic: a belted tunic with a short skirt over the thighs and a slit neck; 2 robe: a crossed
//                         collar, a broad sash, to the ankles; 3 kaftan: open down the front over the inner garment,
//                         its edges trimmed, a sash, to the ankles; 4 wrap: over one shoulder, the other bare, a
//                         diagonal fold; 5 coat: lapels, a button line, to the knee and parted; 6 kilt: a plaid sash
//                         over the shoulder, a buckled belt, to above the knee (the knees bare); 7 poncho: a wide slab
//                         over the shoulders and arms with a fringe, the hands under its edge; 8 gown: a fitted bodice
//                         under a high waist band, a low neckline, to the ankles
//   outfits               Robe: a long gown with a stole in the trim colour down both sides, a rope sash and a trimmed
//                         hem; Rags: sacking with a torn hem, a patch and a tear, bare arms, torn shins, bare feet
//   patterns              1 stripes, 2 checks, 4 dots over the cloth; 3 border trim, 5 embroidery, 6 vines along the
//                         garment's edges (hem and collar)
#include <algorithm>
#include <cmath>
#include "rpg/art/rig/rig_internal.h"

namespace art {
namespace rig2 {

const Ramp5& sleeveRamp(const Kit& K) { return K.sleeve; }
const Ramp5& legRamp(const Kit& K) { return K.leg; }
bool longSleeved(const Kit& K) {
  return (K.skirt && K.cut != 0) || K.outfit == (int)Outfit::Robe || K.cut == 5;
}

// the garment's pattern (cult::Pattern) over a cloth pixel of ramp index idx
uint32_t patternAt(const Kit& K, int x, int y, uint32_t base, int idx) {
  switch (K.pattern) {
    case 1: return ((x & 3) == 0) ? mix(base, K.pat[idx], 0.55f) : base;                        // stripes
    case 2: return ((x & 3) == 1 || (y & 3) == 1) ? mix(base, K.pat[idx], ((x & 3) == 1 && (y & 3) == 1) ? 0.42f : 0.24f) : base;   // checks (a gingham grid)
    case 4: return ((x % 3) == 0 && (y % 3) == 1) ? mix(base, K.pat[std::min(4, idx + 1)], 0.6f) : base;   // dots
    default: return base;
  }
}

namespace {

// the edge patterns along a garment's lowest rows (fromHem: rows above the hem row, 0 = the hem): border trim, embroidery,
// vines (v1's design at the v2 scale); 0 = no change
uint32_t hemPattern(const Kit& K, int x, int fromHem, int idx) {
  const Ramp5& Pt = K.pat;
  switch (K.pattern) {
    case 3: if (fromHem == 0) return Pt[idx]; if (fromHem == 1) return Pt[std::min(4, idx + 1)]; break;   // a 2-row band
    case 5:   // a solid band, a stitched row of dots over it, a third row of crosses every 4 px
      if (fromHem == 0) return Pt[idx];
      if (fromHem == 1 && (x & 1)) return Pt[std::min(4, idx + 1)];
      if (fromHem == 2 && (x & 3) == 1) return mix(Pt[std::min(4, idx + 1)], K.gold[3], 0.35f);
      break;
    case 6: {   // a vine waving over two rows with a leaf every 4 px
      const int wave = (x >> 1) & 1;
      if (fromHem == 1 + wave) return Pt[std::min(4, idx + 1)];
      if (fromHem == 2 + wave && (x & 3) == 1) return mix(Pt[idx], rgba(110, 170, 90), 0.5f);
      break;
    }
    default: break;
  }
  return 0;
}

// the torso's bands (rows) for a pose: collar S-1, shoulders S, chest S+1..H-4, waist H-3..H-2, belt H-1, hips H,
// hem H+1 (tunic)
enum Band { kCollar, kShoulder, kChest, kUnderBust, kWaist, kBelt, kHips, kHem };
Band bandOf(int y, int S, int H, bool woman) {
  if (y <= S - 1) return kCollar;
  if (y == S) return kShoulder;
  if (y >= H + 1) return kHem;
  if (y == H) return kHips;
  if (y == H - 1) return kBelt;
  if (y >= H - 3) return kWaist;
  if (woman && y == H - 4) return kUnderBust;
  return kChest;
}

// the robe's hemp cord (a pale rope, so the trim-coloured stole stays the robe's one bright line)
const Ramp kRopeCord = ramp5(rgba(98, 76, 68), rgba(150, 126, 104), rgba(196, 174, 140), rgba(224, 208, 172), rgba(244, 234, 206));

// a sash / band row in the pattern colour (the robe's, the kaftan's, the wrap's), lit on the left
void sashRow(Fig& f, const Kit& K, int l, int r, int y, const Ramp5& c, int lift = 0) {
  for (int x = l; x <= r; x++) f.set(x, y, c[std::min(4, (x == l ? 3 : x == r ? 1 : 2) + lift)]);
}

// the poncho: a wide slab from the neck over the shoulders and arms to 2 rows under the hips, a fringe on its hem,
// two bands woven across (front / back views); side views: the slab's depth from the back to the chest
void paintPoncho(Fig& f, const Body& B, const Kit& K, const Pose& P, TorsoAt t, int H) {
  const Ramp5& C = K.top;
  const int S = t.y, fc = P.facing;
  const int bottom = H + 2;
  if (fc == FDown || fc == FUp) {
    const int cx = t.x;
    for (int y = S - 1; y <= bottom + 1; y++) {
      // the neck hole at the top, then an A-line drape widening a px every two rows past the arms, its hem dipping to a
      // shallow point at the middle (a square of cloth worn corner-down) with a woven band over the fringe
      const int half = y == S - 1 ? B.torsoHalf - 1 : y == S ? B.torsoHalf + 1 : std::min(B.shoulderHalf + 2, B.torsoHalf + 1 + (y - S + 1) / 2);
      const int l = cx - half, r = cx + half - 1 - B.trimR;
      for (int x = l; x <= r; x++) {
        const int d = x < cx ? cx - 1 - x : x - cx;                       // columns from the middle
        const int colBottom = bottom + (d <= 2 ? 1 : 0) - (d >= half - 1 ? 1 : 0);
        if (y > colBottom) continue;
        if (y == S - 1 && x >= cx - 1 && x <= cx) { f.set(x, y, K.skin[1]); continue; }   // the neck in its hole
        int idx = x == l ? 3 : x == r ? 1 : 2;
        if (y <= S && x > l && x <= cx) idx = 3;                 // the shoulders' top catches the light
        if (x == r - 1 && y > S) idx = std::min(idx, 1);         // the drape's shadowed side
        if ((x == cx - 4 || x == cx + 3) && y > S + 1 && y < colBottom - 1) idx = std::max(0, idx - 1);   // folds over the arms
        uint32_t c = C[idx];
        if (y == colBottom - 2) c = K.pat[std::min(4, idx + 1)];   // the woven band
        if (y == colBottom) c = ((x + (fc == FUp)) & 1) ? C[std::max(0, idx - 1)] : K.pat[std::min(4, idx)];   // the fringe
        f.set(x, y, c);
      }
    }
    return;
  }
  const int s = fc == FRight ? 1 : -1;
  for (int y = S - 1; y <= bottom; y++) {
    const int cx = t.x;
    const int grow = std::min(2, (y - S + 1) / 2);
    const int back = y == S - 1 ? 1 : 3 + grow, front = y == S - 1 ? 1 : (y == S ? 3 : 3 + grow);
    for (int d = -back; d <= front; d++) {
      const int x = s > 0 ? cx + d : cx - 1 - d;
      const bool lit = s > 0 ? d == -back : d == front;
      const bool dark = s > 0 ? d == front : d == -back;
      int idx = lit ? 3 : dark ? 1 : 2;
      if (y == S - 1 && d >= 0) { f.set(x, y, K.skin[s > 0 ? 1 : 2]); continue; }
      uint32_t c = C[idx];
      if (y == bottom - 2) c = K.pat[std::min(4, idx + 1)];
      if (y == bottom) c = ((x & 1) ? C[std::max(0, idx - 1)] : K.pat[std::min(4, idx)]);
      f.set(x, y, c);
    }
  }
}

}  // namespace

void paintTorso(Fig& f, const Body& B, const Kit& K, const Pose& P, TorsoAt t) {
  const int S = t.y, H = B.hip + P.dy;
  const int fc = P.facing;
  const bool woman = B.woman != 0;
  const bool gown = K.cut == 8;
  if (K.cut == 7) { paintPoncho(f, B, K, P, t, H); return; }
  const Ramp5& C = K.top;
  auto cxAt = [&](int y) {
    const float k = H > S ? clampf((float)(y - S) / (float)(H - S), 0.0f, 1.0f) : 0.0f;
    return (int)std::lround(t.x - P.lean * k);
  };
  // the tunic ends at the hips (its lit hem; a woman's falls two rows further, flared); a skirt carries on below
  const bool ownHem = K.hang == HangNone;
  const int bottom = H + (woman && ownHem ? 2 : 0);
  auto cloth = [&](int x, int y, int idx) {
    uint32_t c = patternAt(K, x, y, C[idx], idx);
    if (ownHem && K.pattern >= 3) { const uint32_t e = hemPattern(K, x, bottom - y, idx); if (e) c = e; }
    if (K.pattern == 3 && y == S - 1) c = K.pat[std::min(4, idx + 1)];   // border trim round the collar too (side views)
    f.set(x, y, c);
  };
  const bool robe = K.outfit == (int)Outfit::Robe && K.cut == 0;
  if (fc == FDown || fc == FUp) {
    const bool dn = fc == FDown;
    for (int y = S - 1; y <= bottom; y++) {
      const int cx = cxAt(y);
      const Band b = bandOf(y, S, H, woman);
      int half = B.torsoHalf;
      if (b == kShoulder) half = B.torsoHalf + 1;   // the shoulder line rounds over the tops of the arms
      if (b == kWaist) half = B.waistHalf;
      if (b == kBelt) half = B.waistHalf - (woman ? 1 : 0);
      if (b == kHips) half = B.hipHalf;
      if (b == kHem) half = B.hipHalf + (y == bottom ? 1 : 0);
      if (b == kCollar) {
        // the neck (skin, in the chin's shadow) between the shoulder tops; the collar's edge catches the light
        for (int x = cx - 1; x <= cx; x++) f.set(x, y, K.skin[fc == FDown ? 1 : 1]);
        auto collar = [&](int x, int idx) { f.set(x, y, K.pattern == 3 ? K.pat[std::min(4, idx + 1)] : C[idx]); };   // (border trim)
        for (int x = cx - B.torsoHalf + 1; x <= cx - 2; x++) collar(x, x == cx - 2 ? 4 : 3);
        for (int x = cx + 1; x <= cx + B.torsoHalf - 2 - B.trimR; x++) collar(x, x == cx + 1 ? 3 : 2);
        continue;
      }
      const int l = cx - half, r = cx + half - 1 - B.trimR;
      const bool lastRow = y == bottom && ownHem;
      for (int x = l; x <= r; x++) {
        int idx = 2;
        // the box's lit left plane (two columns on the shoulders and chest), its shadowed right side
        if (x == l) idx = 3;
        else if (x == l + 1 && (b == kShoulder || b == kChest || b == kUnderBust)) idx = 3;
        else if (x == r) idx = 1;
        else if (x == r - 1 && (b == kWaist || b == kHips)) idx = 1;
        if (b == kShoulder && x > l + 1 && x <= cx) idx = 3;    // the shoulders' top plane catches the light on the lit half
        if (lastRow) idx = x <= cx ? 3 : 2;                     // the lit hem
        if (b == kUnderBust && x > l && x < r) idx = 1;         // ONE shadow row under the bust
        if (b == kBelt && !gown) {
          if (robe) { f.set(x, y, kRopeCord[x == l ? 3 : x == r ? 1 : 2]); continue; }   // the robe's hemp cord
          f.set(x, y, K.belt[x == l ? 3 : x == r ? 1 : 2]);
          continue;
        }
        cloth(x, y, idx);
      }
      if (b == kBelt && !gown && !robe && fc == FDown) { f.set(cx - 1, y, K.guard[3]); f.set(cx, y, K.guard[1]); }   // the buckle
      if (fc == FDown && b == kShoulder && !gown) { f.set(cx - 1, y, C[1]); f.set(cx, y, C[0]); }   // the neck's notch
      if (fc == FDown && gown && b == kShoulder) { for (int x = cx - 2; x <= cx + 1; x++) f.set(x, y, K.skin[x == cx - 2 ? 2 : 1]); }
      if (fc == FDown && gown && b == kChest && y == S + 1) { f.set(cx - 1, y, K.skin[1]); f.set(cx, y, K.skin[0]); }
      // rags: the hem torn into a ragged edge (some columns end a row early), the row above shadowed where it frays
      if (K.rags && lastRow)
        for (int x = l; x <= r; x++)
          if (((x * 7 + 3) % 5) < 2) { f.set(x, y, 0u); f.set(x, y - 1, C[std::max(0, (x == r ? 1 : 2) - 1)]); }
    }
    // ---- the cut's detail over the torso (front / back): collars, sashes, openings, the bare shoulder
    const int cS = cxAt(S), cH = cxAt(H - 1);
    const Ramp5& Pt = K.pat;
    auto bandX = [&](int y) { return cxAt(y); };
    switch (K.cut) {
      case 1:   // the tunic: a slit at the neck, the belt (the torso's own)
        if (dn) { f.set(cS - 1, S, K.skin[1]); f.set(cS, S, K.skin[0]); f.set(cS - 1, S + 1, C[1]); f.set(cS, S + 1, C[0]); }
        break;
      case 2: {   // the robe: a crossed collar (the left side over the right) and a broad sash
        if (dn) {
          f.set(cS - 1, S - 1, K.skin[1]); f.set(cS, S - 1, K.skin[0]);
          for (int i = 0; i < 4; i++) f.set(cS - 3 + i, S - 1 + i, Pt[i < 2 ? 3 : 2]);              // the overlapping edge
          for (int i = 0; i < 2; i++) f.set(cS + 2 - i, S - 1 + i, Pt[1]);                         // the edge it covers
          f.set(cS - 1, S, K.skin[1]); f.set(cS, S, K.skin[0]);
        } else {
          for (int x = cS - 2; x <= cS + 1; x++) f.set(x, S - 1, Pt[x < cS ? 3 : 2]);              // the collar round the nape
        }
        const int half = B.waistHalf;
        for (int y = H - 2; y <= H - 1; y++) sashRow(f, K, bandX(y) - half - (y == H - 1 ? 0 : 0), bandX(y) + half - 1 - B.trimR, y, Pt, y == H - 2 ? 1 : 0);
        if (dn) { f.set(cH + 1, H - 1, Pt[0]); f.set(cH + 1, H, Pt[1]); f.set(cH + 2, H + 1, Pt[1]); }   // the sash's knot and its tail
        break;
      }
      case 3: {   // the kaftan: open down the front over the inner garment (the trousers' cloth), trimmed edges, a sash
        if (dn) {
          f.set(cS - 1, S - 1, K.skin[1]); f.set(cS, S - 1, K.skin[0]);
          for (int y = S; y <= H; y++) {
            const int c = bandX(y);
            f.set(c - 1, y, K.leg[2]); f.set(c, y, K.leg[1]);
            f.set(c - 2, y, Pt[3]); f.set(c + 1, y, Pt[2]);
          }
        }
        const int half = B.waistHalf;
        sashRow(f, K, cH - half, cH + half - 1 - B.trimR, H - 1, Pt);
        break;
      }
      case 4: {   // the wrap: over one shoulder, the other bare; the fold runs diagonally down to the opposite hip
        const int bare = dn ? -1 : 1;   // the screen-left shoulder bare facing down (the figure's right), the screen-right facing up
        const int sl = cS - B.torsoHalf - 1, sr = cS + B.torsoHalf - B.trimR;
        for (int y = S - 1; y <= S; y++)
          for (int x = (bare < 0 ? sl : cS + 2); x <= (bare < 0 ? cS - 3 : sr); x++)
            if (f.solid(x, y)) f.set(x, y, K.skin[x == sl ? 3 : (x == sr ? 1 : 2)]);
        for (int i = 0; i < 6; i++) {
          const int y = S + i;
          const int x = bare < 0 ? cS - 2 + i : cS + 1 - i;
          f.set(x, y, Pt[i < 3 ? 3 : 2]);
          f.set(x + (bare < 0 ? -1 : 1), y, Pt[1]);
        }
        const int half = B.waistHalf;
        sashRow(f, K, cH - half, cH + half - 1 - B.trimR, H - 1, Pt);
        break;
      }
      case 5: {   // the coat: a raised collar, lapels over a shirt, a button line down the opening
        if (dn) {
          for (int x = cS - 2; x <= cS + 1; x++) f.set(x, S - 1, x < cS ? C[4] : C[2]);
          f.set(cS - 1, S, K.leg[2]); f.set(cS, S, K.leg[1]);           // the shirt in the lapels' V
          f.set(cS - 2, S, C[4]); f.set(cS + 1, S, C[1]);
          f.set(cS - 1, S + 1, C[4]); f.set(cS, S + 1, C[1]);
          for (int y = S + 2; y <= H; y++) {
            const int c = bandX(y);
            f.set(c - 1, y, C[3]); f.set(c, y, C[1]);                    // the opening's edge over the other side
            if (((y - S) & 1) == 0 && y < H - 1) f.set(c, y, K.guard[3]);   // brass buttons
          }
        } else {
          for (int x = cS - 2; x <= cS + 1; x++) f.set(x, S - 1, x < cS ? C[4] : C[2]);
          for (int y = H - 2; y <= H; y++) f.set(bandX(y), y, C[1]);    // the back seam to the vent
        }
        break;
      }
      case 6: {   // the kilt: a plaid sash from one shoulder to the other hip (the belt and buckle are the torso's own)
        const int dir = dn ? 1 : -1;
        const int x0 = dn ? cS - B.torsoHalf : cS + B.torsoHalf - 1 - B.trimR;
        for (int i = 0; i <= H - 1 - S; i++) {
          const int y = S + i;
          const int x = x0 + dir * (i * 2 * B.torsoHalf / std::max(1, H - 1 - S));
          f.set(x, y, Pt[3]);
          f.set(x + dir, y, ((x + y) & 1) ? Pt[2] : mix(Pt[2], K.top[1], 0.5f));
        }
        break;
      }
      default: break;
    }
    // the robe (outfit): a stole in the trim colour hanging from the collar down both sides of the front, over the cord
    // (it carries on down the skirt: paintSkirt), a trimmed collar; from behind, the stole's band round the nape
    if (robe && dn) {
      for (int y = S; y <= H; y++) {
        const int c = bandX(y);
        f.set(c - 3, y, K.trim[3]); f.set(c + 2, y, K.trim[1]);
      }
      f.set(cS - 2, S - 1, K.trim[4]); f.set(cS + 1, S - 1, K.trim[2]);
    } else if (robe) {
      for (int x = cS - 2; x <= cS + 1; x++) f.set(x, S - 1, K.trim[x < cS ? 3 : 2]);
    }
    // rags: a patch of other sacking on the chest and a tear
    if (K.rags) {
      const int c = cxAt(S + 3);
      const uint32_t p0 = mix(C[2], rgba(110, 96, 74), 0.6f), p1 = mix(C[1], rgba(90, 76, 60), 0.6f);
      if (dn) { f.set(c + 1, S + 2, p0); f.set(c + 2, S + 2, p1); f.set(c + 1, S + 3, p1); f.set(c + 2, S + 3, p1); f.set(c - 3, S + 4, C[0]); f.set(c - 2, S + 5, C[0]); }
      else { f.set(c - 2, S + 3, p0); f.set(c - 1, S + 3, p1); f.set(c + 2, S + 2, C[0]); }
    }
    // embroidery: a small motif under the collar (front)
    if (K.pattern == 5 && dn && K.cut != 3 && K.cut != 5) {
      f.set(cS - 1, S + 1, K.pat[3]); f.set(cS, S + 1, K.pat[2]); f.set(cS - 1, S + 2, K.pat[2]); f.set(cS, S + 2, K.pat[1]);
    }
    // the gown's high waist band under the bust
    if (gown) {
      const int y = S + 3, cx = cxAt(y);
      for (int x = cx - B.waistHalf; x <= cx + B.waistHalf - 1; x++) f.set(x, y, K.trim[x == cx - B.waistHalf ? 3 : x == cx + B.waistHalf - 1 ? 1 : 2]);
    }
    return;
  }
  // side views: the back is lit facing right, the chest facing left
  const int s = fc == FRight ? 1 : -1;
  for (int y = S - 1; y <= bottom; y++) {
    const int cx = cxAt(y);
    const Band b = bandOf(y, S, H, woman);
    // depth extents (facing right: back = cx - 3, front = cx + front)
    int back = 3, frontX = 2;
    switch (b) {
      case kCollar: back = 1; frontX = woman ? 0 : 1; break;
      case kShoulder: back = 3; frontX = 2; break;
      case kChest: back = 3; frontX = 3; break;
      case kUnderBust: back = 3; frontX = 3; break;
      case kWaist: back = 3; frontX = 2; break;
      case kBelt: back = 3; frontX = woman ? 1 : 2; break;
      case kHips: case kHem: back = 3; frontX = 3; break;
    }
    if (woman && b == kChest && y == H - 5) frontX = 3;
    if (!woman && b == kChest) frontX = 3;
    if (b == kChest || b == kUnderBust) frontX += B.chestDepth;   // a broad build's deeper chest
    if (B.age && (b == kChest || b == kUnderBust || b == kHips || b == kHem)) frontX = 2;   // kids: a slighter body
    if (B.age == 1 && b != kCollar) back = 2;
    const int x0 = cx - back, x1 = cx + frontX;   // facing right
    const bool lastRow = y == bottom && ownHem;
    for (int k = x0; k <= x1; k++) {
      const int x = s > 0 ? k : 2 * cx - 1 - k + 1;   // facing left: mirrored about the centre column
      const bool backEdge = k == x0, frontEdge = k == x1;
      const bool litEdge = s > 0 ? backEdge : frontEdge;
      const bool darkEdge = s > 0 ? frontEdge : backEdge;
      int idx = litEdge ? 3 : darkEdge ? 1 : 2;
      if (b == kCollar) {
        if (k >= cx - 1) { f.set(x, y, K.skin[s > 0 ? (k == x1 ? 1 : 2) : (k == x1 ? 3 : 2)]); continue; }
      }
      if (lastRow) idx = darkEdge ? 2 : 3;   // the lit hem
      if (b == kUnderBust && k > cx) idx = 1;
      if (b == kBelt && !gown) {
        const int bi = litEdge ? 3 : darkEdge ? 1 : 2;
        f.set(x, y, robe ? kRopeCord[bi] : (K.cut == 2 || K.cut == 3 || K.cut == 4 ? K.pat[bi] : K.belt[bi]));
        continue;
      }
      if (K.rags && lastRow && ((x * 7 + 3) % 5) < 2) continue;   // the torn hem
      cloth(x, y, idx);
    }
    // the cut's side detail
    if (b != kCollar) {
      auto at = [&](int k, uint32_t c) { f.set(s > 0 ? k : 2 * cx - k, y, c); };
      if (K.openFront && y >= S) at(x1, K.pat[s > 0 ? 2 : 3]);                       // the kaftan's / coat's front edge
      if (K.cut == 5 && y >= S + 2 && y < H - 1 && ((y - S) & 1) == 0) at(x1 - 1, K.guard[3]);   // the coat's buttons
      if (K.cut == 2 && y == H - 2) for (int k = x0; k <= x1; k++) at(k, K.pat[k == x0 ? (s > 0 ? 4 : 2) : 3]);   // the robe's broad sash
      if (K.cut == 6 && y >= S && y < H - 1) {                                       // the kilt's sash crossing the chest
        const int k = x0 + (y - S) * (x1 - x0) / std::max(1, H - 2 - S);
        at(k, K.pat[3]);
      }
      if (K.cut == 4 && y >= S && y <= S + 1 && s > 0) at(x1, K.skin[2]);            // the wrap's bare shoulder (facing right: the near one)
      if (robe && y >= S && y <= H - 2) at(x1 - 1, K.trim[s > 0 ? 2 : 3]);           // the stole
      if (K.rags && y == S + 3) at(cx, mix(K.top[1], rgba(90, 76, 60), 0.6f));
    }
    if (gown && y == S + 3)
      for (int k = x0; k <= x1; k++) f.set(s > 0 ? k : 2 * cx - k, y, K.trim[k == x0 ? (s > 0 ? 3 : 1) : 2]);
  }
}

// a skirt from the hips to its hem (long: just above the shoes; knee: the coat and the kilt; short: two rows, the culture
// tunic and the poncho's own hem), flaring, its hem following the feet (STYLE_P2 2: clothes hang)
void paintSkirt(Fig& f, const Body& B, const Kit& K, const Pose& P, TorsoAt t, int footRx, int footLx) {
  if (K.cut == 7) return;   // (the poncho's slab is its own hem)
  const int H = B.hip + P.dy;
  int hem = -3;
  if (K.hang == HangKnee) hem = H + (B.legs * 6) / 11 + (K.cut == 6 ? -1 : 0) + P.dy * 0;
  else if (K.hang == HangShort) hem = H + 2;
  if (K.hang != HangLong) hem = std::min(hem, -3);
  const int fc = P.facing;
  const int cx = (int)std::lround(t.x - P.lean);
  const Ramp5& C = K.top;
  const int rows = std::max(1, hem - H);
  const bool split = K.openFront && fc == FDown;   // the kaftan and the coat open at the front: the legs show between
  const bool vent = K.openFront && fc == FUp;
  for (int y = H; y <= hem; y++) {
    const float k = (float)(y - H) / (float)rows;
    int l, r;
    if (fc == FDown || fc == FUp) {
      const float mid = (footRx + footLx) * 0.5f - (cx - 0.5f);
      const int flare = (y - H) / 3;
      const int dx = (int)std::lround(mid * k * 0.5f);
      l = cx - B.hipHalf - flare + dx;
      r = cx + B.hipHalf - 1 + flare + dx - B.trimR;
    } else {
      const int s = fc == FRight ? 1 : -1;
      const int fmin = std::min(footRx, footLx), fmax = std::max(footRx, footLx);
      // facing right the back is at cx - 3, the front at cx + 3; the hem reaches over both feet
      const int backAt = s > 0 ? cx - 3 : cx - 3;
      const int frontAt = s > 0 ? cx + 3 : cx + 3;
      // the hem swings with the legs but the shoes peek out at the front and the back (no tent)
      l = (int)std::lround(lerpf((float)backAt, (float)std::min(backAt - 1, fmin + 1), k));
      r = (int)std::lround(lerpf((float)frontAt, (float)std::max(frontAt + 1, fmax - 1), k));
      if (K.hang != HangLong) { l = std::max(l, backAt - 1); r = std::min(r, frontAt + 1); }
    }
    // the cloth hangs over the legs: no leg pixel above the hem shows past the skirt's edge (a striding leg pushes the
    // cloth out; a lagging hem trails only where it still covers them)
    if (f.legLay[0]) {
      int legL = 1 << 20, legR = -(1 << 20);
      for (int x = l - 8; x <= r + 8; x++) {
        const uint8_t la = f.layerAt(x, y);
        if (la && (la == f.legLay[0] || la == f.legLay[1])) { legL = std::min(legL, x); legR = std::max(legR, x); }
      }
      if (legR >= legL) { l = std::min(l, legL); r = std::max(r, legR); }
    }
    for (int x = l; x <= r; x++) {
      const int rel = x - cx;
      if (split && y > H && (rel == -1 || rel == 0)) continue;   // the opening (the trousers under it)
      int idx = 2;
      if (x == l) idx = 3;
      else if (x == r) idx = 1;
      // folds: single ramp-1 lines falling from the hips
      if ((fc == FDown || fc == FUp) && y > H + 1 && (rel == -2 || rel == 2) && y < hem) idx = 1;
      if (y == hem) idx = x == r ? 2 : 3;                    // the lit hem
      if (vent && y > H + 1 && rel == 0) idx = 0;            // the back vent
      uint32_t c = patternAt(K, x, y, C[idx], idx);
      if (K.pattern >= 3) { const uint32_t e = hemPattern(K, x, hem - y, idx); if (e) c = e; }
      if (split && y > H && (rel == -2 || rel == 1)) c = K.pat[rel < 0 ? 3 : 2];   // the opening's trimmed edges
      if (K.outfit == (int)Outfit::Robe && K.cut == 0 && hem - y <= 1) c = K.trim[std::min(4, idx + (hem - y))];   // the robe's trimmed hem
      if (K.rags && y == hem && ((x * 7 + 3) % 5) < 2) continue;   // a torn hem
      f.set(x, y, c);
    }
    // the robe's stole carries on down the front to just over the trimmed hem
    if (K.outfit == (int)Outfit::Robe && K.cut == 0 && fc == FDown && y < hem - 1) { f.set(cx - 3, y, K.trim[3]); f.set(cx + 2, y, K.trim[1]); }
  }
}

// the kilt's bare knees (the row under its hem and the knee row) and the rags' torn shins and bare feet: only pixels of
// the two leg layers change, re-coloured by their ramp index (so the legs keep their light)
void paintLegWear(Fig& f, const Body& B, const Kit& K, const Pose& P, uint8_t la, uint8_t lb) {
  const Ramp5& L = K.leg;
  auto idxOf = [&](uint32_t c) { for (int i = 0; i < 5; i++) if (L.c[i] == c) return i; return -1; };
  const int H = B.hip + P.dy;
  int y0, y1;
  if (K.cut == 6) { y0 = H + (B.legs * 6) / 11; y1 = y0 + 2; }   // under the kilt: the knees
  else { y0 = -4; y1 = -3; }                                    // rags: the shins below the frayed trouser hems
  for (int y = y0; y <= y1; y++)
    for (int x = -24; x < 24; x++) {
      const uint8_t l = f.layerAt(x, y);
      if (l != la && l != lb) continue;
      const int i = idxOf(f.get(x, y));
      if (i < 0) continue;
      if (K.rags && y == y0 && ((x * 5 + 1) % 3) == 0) { f.set(x, y, L[std::max(0, i - 1)]); continue; }   // a frayed thread
      f.set(x, y, K.skin[std::min(4, i + 0)]);
    }
}

// the torc (jewellery bit 2): a gold band round the base of the neck, lit on the left
void paintTorc(Fig& f, const Body& B, const Kit& K, const Pose& P, TorsoAt t) {
  const int y = t.y - 1;
  const Ramp5& g = K.gold;
  if (P.facing == FDown) {
    f.set(t.x - 2, y, g[3]); f.set(t.x - 1, y, g[4]); f.set(t.x, y, g[3]); f.set(t.x + 1, y, g[2]);
    f.set(t.x - 1, y + 1, g[2]);
  } else if (P.facing == FUp) {
    f.set(t.x - 2, y, g[2]); f.set(t.x - 1, y, g[3]); f.set(t.x, y, g[2]); f.set(t.x + 1, y, g[1]);
  } else {
    const int s = P.facing == FRight ? 1 : -1;
    f.set(t.x + s * 0, y, g[3]); f.set(t.x + s * 1, y, g[s > 0 ? 2 : 4]);
  }
  (void)B;
}

// ---------------------------------------------------------------- headwear (cult::Headwear + the legacy hood)
// Maps over the hair maps' grid (16 columns: x = head.x - 2 + i; row 0 = the head's top painted row), a few rows above
// the head allowed (rise). Codes: '.' none, 0-4 the headwear's ramp, 's' the face in the brim's shadow (skin ramp 0, only
// over skin), 'y' 'g' 'G' 'h' gold 4 / 3 / 2 / 1, 'j' a jewel, 'f' 'F' fur light / dark. The left facing is the right
// map mirrored with every run re-lit left to right (rig_hair.cpp's rule), so the light stays top-left.
namespace {

struct HwMap { int rise = 0, rows = 0; const char* const* m = nullptr; };
template <size_t N>
HwMap hw(int rise, const char* const (&a)[N]) { return HwMap{rise, (int)N, a}; }

// ---- the hood (headwear 1 and the legacy hood): cloth over the head, framing the face, the cowl on the shoulders
const char* const kHoodDown[] = {
    ".....233332.....",
    "....23443322....",
    "...2344333221...",
    "..233333332221..",
    "..2343ssss3421..",
    "..23........21..",
    "..23........21..",
    "..23........11..",
    "..232......121..",
    "..2321....1221..",
    "..23321..12221..",
    ".2333221122211..",
    ".23332222222211.",
};
const char* const kHoodUp[] = {
    ".....233332.....",
    "....23443322....",
    "...2344333221...",
    "..233433322221..",
    "..233332222211..",
    "..233322222111..",
    "..223322221111..",
    "..232222221111..",
    "..223222211111..",
    "..222322211111..",
    "..222222211111..",
    ".2333222221111..",
    ".23332222221111.",
};
const char* const kHoodRight[] = {
    ".....233332.....",
    "....23443322....",
    "...2344333221...",
    "..23443332221...",
    "..2333332s221...",
    "..233322.1......",
    "..23322.........",
    "..2322..........",
    "..23221.........",
    "..232221........",
    "..2322221.......",
    "..23322221......",
    ".233322211......",
};
// ---- the cap (headwear 2): a soft flat beret over the hair, wider than the head and tipped down on the right, its band
// at the crown (the hair shows from the temples down)
const char* const kCapDown[] = {
    ".....233332.....",
    "..2344433332....",
    "...2333332221l..",
    "....1222221.....",
};
const char* const kCapUp[] = {
    ".....233332.....",
    "..2334333332....",
    "...2333322221l..",
    "....1222221.....",
};
const char* const kCapRight[] = {
    "....233332......",
    "..234443332.....",
    "...23333221l....",
    "....122221......",
};
// ---- the turban (headwear 3): wraps rising over the crown, the folds running diagonally, a jewel at the front
const char* const kTurbanDown[] = {
    ".....233332.....",
    "....2344332.....",
    "...234332321....",
    "..23343j3322l...",
    "..2343gyg32211..",
    "..2332343322111.",
    "..233322232211..",
};
const char* const kTurbanUp[] = {
    ".....233332.....",
    "....2344332.....",
    "...234332321....",
    "..2334332322l...",
    "..234332332211..",
    "..233233222111..",
    "..232322232211..",
};
const char* const kTurbanRight[] = {
    ".....23333......",
    "....234433......",
    "...2343323......",
    "..234332332.....",
    "..2343323gj.....",
    "..23323323y.....",
    "..2332223221....",
};
// ---- the fur hat (headwear 4): a cloth crown domed over the head, a thick pelt band round it at the brow, its tufts
// catching the light on top ('f' 'u' 'F' 'v': the pelt's ramp 4 / 3 / 2 / 1, in irregular clumps, never a checker)
const char* const kFurDown[] = {
    ".....233332.....",
    "....23443322....",
    "...2344333221...",
    "..uffufffuffuv..",
    "..vuFvuuFvuFvv..",
};
const char* const kFurUp[] = {
    ".....233332.....",
    "....23433322....",
    "...2343332221...",
    "..ufuffuffufuv..",
    "..vuFvuFvuuFvv..",
};
const char* const kFurRight[] = {
    "....233332......",
    "...23443322.....",
    "..2344333221....",
    "..uffufffufv....",
    "..vuFvuuFvvv....",
};
// ---- the veil (headwear 5): a light cloth over the head falling past the shoulders, a gold band at the brow
const char* const kVeilDown[] = {
    ".....233332.....",
    "....23443322....",
    "...2344333221...",
    "..2gyggggGGh21..",
    "..23........21..",
    "..32........21..",
    "..32........11..",
    "..32........21..",
    "..32........11..",
    "..321......121..",
    "..3221....1221..",
    "..32221..12221..",
    "..322221122221..",
    "..32222..12211..",
    "..3222....1221..",
};
const char* const kVeilUp[] = {
    ".....233332.....",
    "....23443322....",
    "...2344333221...",
    "..2gyggggGGh21..",
    "..233332222211..",
    "..233322222111..",
    "..323222221121..",
    "..322322221211..",
    "..323222212111..",
    "..322322221211..",
    "..323222212111..",
    "..322322221211..",
    "..323222212111..",
    "..322322221211..",
    "...2322221111...",
};
const char* const kVeilRight[] = {
    ".....233332.....",
    "....23443322....",
    "...2344333221...",
    "..2ggggggGh.....",
    "..23333.........",
    "..2333..........",
    "..2332..........",
    "..3232..........",
    "..2322..........",
    "..3232..........",
    "..23221.........",
    "..322221........",
    "..2322221.......",
    "..3222221.......",
    "...22211........",
};
// ---- the circlet (headwear 6): a thin gold band across the brow, a gem at the front; the hair shows
const char* const kCircletDown[] = {
    "",
    "",
    "",
    "...gyggjgggGh...",
};
const char* const kCircletUp[] = {
    "",
    "",
    "",
    "...gggggGGGGh...",
};
const char* const kCircletRight[] = {
    "",
    "",
    "",
    "...gggggggyj....",
};
// ---- the conical hat (headwear 7): a straw cone over the crown, a wide brim that shades the brow
const char* const kConeDown[] = {
    ".......32.......",
    "......3432......",
    ".....344321.....",
    "....34433221....",
    ".23444333332221.",
    "2334444333332221",
    ".1222222222221..",
};
const char* const kConeUp[] = {
    ".......32.......",
    "......3332......",
    ".....343321.....",
    "....33433221....",
    ".23343333332221.",
    "2333443333322221",
    ".1222222222221..",
};
const char* const kConeRight[] = {
    "......32........",
    ".....3432.......",
    "....344321......",
    "...34433221.....",
    ".234443333222...",
    "23344433333221..",
    ".12222222222....",
};
// ---- the headscarf (headwear 8): cloth snug over the head and down both cheeks, tied in a knot under the chin, its lit
// fold framing the face; from behind, its corner hangs to the nape
const char* const kScarfDown[] = {
    ".....233332.....",
    "....23443322....",
    "...2344333221...",
    "...2333333221...",
    "...23ssssss21...",
    "...23......21...",
    "...23......21...",
    "...32......11...",
    "...321....121...",
    "....321..121....",
    ".....32l121.....",
    "......3l2.......",
};
const char* const kScarfUp[] = {
    ".....233332.....",
    "....23443322....",
    "...2344333221...",
    "...2334332221...",
    "...2333322211...",
    "...2233222111...",
    "...2322221111...",
    "...2232221111...",
    "....23222111....",
    ".....232211.....",
    "......2211......",
    ".......21.......",
};
const char* const kScarfRight[] = {
    ".....233332.....",
    "....23443322....",
    "...2344333221...",
    "...234433321....",
    "...2343331......",
    "...234331.......",
    "...23431........",
    "...2333l........",
    "...23221........",
    "....2322221.....",
    ".....223321l....",
    ".........2l.....",
};

bool hwMap(const Kit& K, int facing, HwMap& out) {
  const int kind = K.hood ? 1 : K.headwear;
  const bool dn = facing == FDown, up = facing == FUp;
  switch (kind) {
    case 1: out = dn ? hw(1, kHoodDown) : up ? hw(1, kHoodUp) : hw(1, kHoodRight); break;
    case 2: out = dn ? hw(1, kCapDown) : up ? hw(1, kCapUp) : hw(1, kCapRight); break;
    case 3: out = dn ? hw(3, kTurbanDown) : up ? hw(3, kTurbanUp) : hw(3, kTurbanRight); break;
    case 4: out = dn ? hw(2, kFurDown) : up ? hw(2, kFurUp) : hw(2, kFurRight); break;
    case 5: out = dn ? hw(1, kVeilDown) : up ? hw(1, kVeilUp) : hw(1, kVeilRight); break;
    case 6: out = dn ? hw(0, kCircletDown) : up ? hw(0, kCircletUp) : hw(0, kCircletRight); break;
    case 7: out = dn ? hw(4, kConeDown) : up ? hw(4, kConeUp) : hw(4, kConeRight); break;
    case 8: out = dn ? hw(1, kScarfDown) : up ? hw(1, kScarfUp) : hw(1, kScarfRight); break;
    default: return false;
  }
  return true;
}

}  // namespace

bool headwearHides(const Kit& K) {
  const int kind = K.hood ? 1 : K.headwear;
  // the cap, the circlet and the conical hat sit over the hair (it shows under their brims); the hood, the turban, the
  // fur hat, the veil and the scarf cover it (only long lengths fall out below the ears)
  return kind != 0 && kind != 2 && kind != 6 && kind != 7;
}

void paintHeadwear(Fig& f, const Body& B, const Kit& K, const Pose& P, HeadAt h, bool backLayer) {
  HwMap m;
  if (!hwMap(K, P.facing, m)) return;
  const int kind = K.hood ? 1 : K.headwear;
  if (P.hatOff && (kind == 2 || kind == 7)) return;   // (a lying body's cap or straw hat has fallen off: rig_death.cpp)
  // the back layer: the hood's and the veil's drape and the scarf's tails behind the body (front views only: from the
  // back they hang over the body and are painted with the front layer)
  if (backLayer) return;
  const Ramp5& R = kind == 1 ? (K.hood ? K.hoodR : K.head) : kind == 7 ? K.straw : K.head;
  const int top = h.y + 1;
  const bool left = P.facing == FLeft;
  auto colourOf = [&](char ch, int x, int y, uint32_t& c) {
    switch (ch) {
      case 's': { const uint32_t u = f.get(x, y); if (!(u >> 24)) return false; c = K.skin[0]; return u == K.skin[1] || u == K.skin[2] || u == K.skin[3] || u == K.skin[4]; }
      case 'y': c = K.gold[4]; return true;
      case 'g': c = K.gold[3]; return true;
      case 'G': c = K.gold[2]; return true;
      case 'h': c = K.gold[1]; return true;
      case 'j': c = kind == 6 ? rgba(120, 210, 230) : rgba(176, 46, 58); return true;   // the circlet's blue stone, the turban's ruby
      case 'l': c = R[1]; return true;
      case 'f': c = K.fur[4]; return true;   // the fur band: pale tufts over its shaded base
      case 'u': c = K.fur[3]; return true;
      case 'F': c = K.fur[2]; return true;
      case 'v': c = K.fur[1]; return true;
      default:
        if (ch >= '0' && ch <= '4') { c = R[ch - '0']; return true; }
        return false;
    }
  };
  for (int r = 0; r < m.rows; r++) {
    const int hr = r - m.rise;   // the row on the head map (negative: above the head)
    const int pr = hr < 0 ? hr : headRow(B, hr);
    if (pr == -1 && hr >= 0) continue;
    const char* row = m.m[r];
    int n = 0;
    while (row[n]) n++;
    auto put = [&](int i, char ch) {
      const int x = h.x - 2 + i, y = top + pr;
      uint32_t c;
      if (ch == '.' || !colourOf(ch, x, y, c)) return;
      f.set(x, y, c);
    };
    if (!left) { for (int i = 0; i < n; i++) put(i, row[i]); continue; }
    char mir[16];
    mirrorRelit(row, n, 16, mir);
    for (int i = 0; i < 16; i++) put(i, mir[i]);
  }
  // the conical hat's brim shades the brow below it (the skin of the two rows under the brim one step darker)
  if (kind == 7 && P.facing != FUp)
    for (int hr = 3; hr <= 4; hr++) {
      const int pr = headRow(B, hr);
      if (pr < 0) continue;
      for (int x = h.x; x < h.x + 12; x++) {
        const uint32_t u = f.get(x, top + pr);
        for (int i = 2; i <= 4; i++) if (u == K.skin[i]) { f.set(x, top + pr, K.skin[i - 1]); break; }
      }
    }
}

}  // namespace rig2
}  // namespace art
