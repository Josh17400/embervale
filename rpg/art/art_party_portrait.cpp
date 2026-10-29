// M8 Fellowship art: the party portraits (rpg/art/art_party.h portraitSprite). ART lane, phase B.
//
// A 24x24 head-and-shoulders bust painted at its own scale (not a crop of the 16x24 sheet): the head turned a quarter
// toward screen right, so the near cheek and the ear face the top-left light and the far cheek turns into shade; the
// M3 people's ears (a half-breed's short point, an elf's long swept ear), the rig's own colours (art_human.cpp
// lookKit: skin, hair, garment, cloak, the helm's metal), every hair style, the beard, the culture headwear, the worn
// helmet in its form, the armour's collar (a mail coif, a plate gorget and pauldrons, a jerkin's laces, a tabard), the
// cloak and its clasp, a fur mantle, jewellery, face paint and what is carried on the back. Moods: 0 calm, 1 hurt (pale,
// a cut on the cheek, the brows drawn), 2 downed (eyes shut, the colour drained), 3 glad (a smile, the cheeks warm).
// Same conventions as every sprite: top-left light, 3-5 tone ramps, a 1 px darkened-neighbour outline, transparent
// ground, deterministic.
#include "rpg/art/art_internal.h"
#include "rpg/art/art_fx.h"
#include "rpg/art/art_rig.h"
#include "rpg/art/doll/doll.h"

namespace art {

namespace {

Ramp fromKit(const uint32_t* k) { return ramp5(k[0], k[1], k[2], k[3], k[4]); }
inline bool inEl(float px, float py, float cx, float cy, float rx, float ry) {
  const float dx = (px - cx) / rx, dy = (py - cy) / ry;
  return dx * dx + dy * dy <= 1.0f;
}

// a whole bust: geometry in canvas px (pixel centres at +0.5)
struct Bust {
  Canvas& c;
  const HumanLook& L;
  int mood;
  LookKit K;
  Ramp skin, hair, top, sleeve, trim, cloak, head, plume, helm, gilt, hood, pat;
  // the cranium and the jaw (their union is the head), the ear's anchor
  float hx = 11.5f, hcy = 8.4f, hrx = 4.8f, hry = 5.3f;
  float jx = 12.1f, jy = 11.0f, jrx = 3.8f, jry = 3.6f;
  int bw = 0;                    // build: shoulders one px wider (broad) or narrower (slim, elves)
  bool elf = false, half = false;

  Bust(Canvas& c_, const HumanLook& l, int m) : c(c_), L(l), mood(m) {
    lookKit(L, K);
    skin = fromKit(K.skin); hair = fromKit(K.hair); top = fromKit(K.top); sleeve = fromKit(K.sleeve); trim = fromKit(K.trim);
    cloak = fromKit(K.cloak); head = fromKit(K.head); plume = fromKit(K.plume); helm = fromKit(K.helm); gilt = fromKit(K.gilt);
    hood = fromKit(K.hood); pat = fromKit(K.pat);
    elf = L.people == 2; half = L.people == 1;
    if (elf) { hrx = 4.5f; jrx = 3.3f; jry = 3.9f; jy = 11.1f; }
    if (half) { hrx = 4.7f; jrx = 3.6f; }
    bw = L.build == 2 ? 1 : ((L.build == 1 || elf) ? -1 : 0);
    if (mood == 1) {   // hurt: the blood leaves the face
      for (int i = 0; i < 5; i++) skin.c[i] = mix(skin.c[i], rgba(214, 206, 204), 0.28f);
    }
  }

  // ---------------------------------------------------------------- shapes
  bool inHead(int x, int y) const {
    const float px = x + 0.5f, py = y + 0.5f;
    return inEl(px, py, hx, hcy, hrx, hry) || inEl(px, py, jx, jy, jrx, jry);
  }
  // the bust: the trapezius from the neck, the round shoulders, the chest to the cell's bottom row
  void bustSpan(int y, int& x0, int& x1) const {
    static const int L0[9] = {8, 6, 4, 3, 2, 2, 2, 2, 2};     // rows 15..23
    static const int R0[9] = {15, 17, 19, 20, 21, 21, 21, 21, 21};
    if (y < 15 || y > 23) { x0 = 1; x1 = 0; return; }
    x0 = L0[y - 15] - bw; x1 = R0[y - 15] + bw;
    x0 = std::max(1, x0); x1 = std::min(22, x1);
  }
  bool inBust(int x, int y) const { int a, b; bustSpan(y, a, b); return x >= a && x <= b; }

  // ---------------------------------------------------------------- the body
  // 0 tunic-like cloth, 1 leather jerkin, 2 mail, 3 plate, 4 rags
  int bodyKind() const {
    if (K.form) {
      if (K.form == 6 || K.form == 7 || K.form == 8 || K.form == 5) return 3;   // brigandine, plate, leaf
      if (K.form == 3 || K.form == 4) return 2;   // mail, scale
      if (K.form == 2) return 1;
      return 0;
    }
    switch ((Outfit)K.outfit) {
      case Outfit::Leather: return 1;
      case Outfit::Chain: case Outfit::Guard: return 2;
      case Outfit::Plate: case Outfit::Elven: case Outfit::Ebony: return 3;
      case Outfit::Rags: return 4;
      default: return 0;
    }
  }
  // the bust's top edge in column x (the first row inside it)
  int bustTop(int x) const { for (int y = 15; y <= 23; y++) if (inBust(x, y)) return y; return 24; }
  int shadeK(int x, int y) const {   // the cloth's light over the round shoulders and the chest: lit from the top left
    const float nx = std::clamp((x + 0.5f - 12.0f) / 10.5f, -0.95f, 0.95f);
    const float up = std::clamp(1.0f - (y - bustTop(x)) / 3.2f, 0.0f, 1.0f);
    const float ny = -up * 0.8f + (y > 20 ? 0.15f : 0.0f);
    return std::clamp(lightIndex(lightAt(nx, ny), x, y, 0.14f), 0, 4);
  }
  void paintBody() {
    const int kind = bodyKind();
    const Ramp& T = top;
    for (int y = 15; y <= 23; y++) {
      int x0, x1;
      bustSpan(y, x0, x1);
      for (int x = x0; x <= x1; x++) {
        int k = shadeK(x, y);
        uint32_t col = T[k];
        if (kind == 2) {   // mail rings: rows of rings, each with its lit upper edge
          if ((y & 1) == 0 && ((x + (y >> 1)) & 1)) col = T[std::max(0, k - 1)];
          else if ((y & 1) == 1 && ((x + (y >> 1)) & 1) == 0 && k < 4) col = mix(T[k], T[k + 1], 0.5f);
        } else if (kind == 4) {
          if (hash3(x, y, 77) % 9 == 0) col = T[std::max(0, k - 1)];   // the rags' patches and stains
        } else if (kind == 0 && (x + y * 3) % 11 == 0 && k >= 2 && y > 18) {
          col = T[k - 1];   // a fold now and then
        }
        c.set(x, y, col);
      }
    }
    // the arms' seams: the sleeve meets the body below the shoulders (a darker crease), the sleeve colour outside it
    for (int y = 18; y <= 23; y++) {
      int x0, x1;
      bustSpan(y, x0, x1);
      const int sl = x0 + 2, sr = x1 - 2;
      if (!sameRamp(sleeve, top) || kind == 1) {
        for (int x = x0; x < sl; x++) c.set(x, y, sleeve[std::min(4, shadeK(x, y))]);
        for (int x = sr + 1; x <= x1; x++) c.set(x, y, sleeve[std::max(0, shadeK(x, y))]);
      }
      c.set(sl, y, (kind == 2 ? T : top)[0]);
      c.set(sr, y, (kind == 2 ? T : top)[0]);
    }
    garment(kind);
  }
  static bool sameRamp(const Ramp& a, const Ramp& b) { for (int i = 0; i < 5; i++) if (a.c[i] != b.c[i]) return false; return true; }

  void garment(int kind) {
    const int cx = 12;   // the chest's centre line (the body turned a little toward screen right)
    switch (kind) {
      case 1: {   // a leather jerkin: laced at the chest, brass studs, the shirt's collar showing
        for (int y = 17; y <= 23; y++) { c.set(cx, y, kLeather[0]); if (y & 1) { c.set(cx - 1, y, kLeather[3]); c.set(cx + 1, y, kLeather[1]); } }
        for (int y = 16; y <= 23; y += 3) { c.set(7, y + 1, kBrass[4]); c.set(17, y, kBrass[2]); }
        neckOpening(1, sleeve);
        break;
      }
      case 2: {   // mail: a coif-like collar, the guard's tabard over it
        if ((Outfit)K.outfit == Outfit::Guard) {
          for (int y = 18; y <= 23; y++)
            for (int x = cx - 4 + (y < 19 ? 1 : 0); x <= cx + 4 - (y < 19 ? 1 : 0); x++) c.set(x, y, trim[x < cx - 1 ? 3 : (x > cx + 2 ? 1 : 2)]);
          c.set(cx - 1, 20, kGold[4]); c.set(cx, 20, kGold[3]); c.set(cx, 21, kGold[2]); c.set(cx - 1, 21, kGold[3]); c.set(cx + 1, 20, kGold[2]);
          for (int x = cx - 4; x <= cx + 4; x++) c.set(x, 23, trim[x < cx ? 2 : 1]);
        }
        break;
      }
      case 3: {   // plate: a gorget round the neck, a ridge down the breastplate, the pauldrons over the shoulders
        for (int y = 18; y <= 23; y++) { c.set(cx - 1, y, top[4]); c.set(cx, y, top[3]); c.set(cx + 1, y, top[1]); }
        pauldron(5 - bw, 18, true);
        pauldron(19 + bw, 18, false);
        if (K.accent) { c.set(cx, 21, K.accent); c.set(cx + 1, 21, shade(K.accent, 0.7f)); }
        break;
      }
      case 4: {   // rags: a torn, gaping neckline
        neckOpening(2, top);
        c.set(cx + 2, 18, top[0]); c.set(cx - 3, 19, top[0]);
        break;
      }
      default: {
        // the garment's cut: robes and kaftans close down the front with a band of trim, a coat has lapels, a wrap crosses
        // over, a poncho's yoke and stripes, a gown's low neckline, the dress's round one, the plain tunic's slit
        const int cut = K.cut;
        const bool robe = cut == 2 || (!cut && (Outfit)K.outfit == Outfit::Robe);
        const bool dress = !cut && (Outfit)K.outfit == Outfit::Dress;
        if (robe || cut == 3) {
          for (int y = 16; y <= 23; y++) { c.set(cx, y, trim[3]); c.set(cx + 1, y, trim[2]); }
          neckOpening(0, trim);
        } else if (cut == 4) {   // a wrap: the near panel crosses over to the far side
          for (int y = 16; y <= 23; y++) { const int x = cx - 2 + (y - 16) / 2; c.set(x, y, top[0]); c.set(x + 1, y, top[4]); }
          neckOpening(0, top);
        } else if (cut == 5) {   // a coat: lapels open over a shirt
          for (int y = 16; y <= 21; y++) { c.set(cx - 2 + (y - 16) / 3, y, top[4]); c.set(cx + 2 - (y - 16) / 3, y, top[1]); }
          for (int y = 17; y <= 20; y++) for (int x = cx - 1; x <= cx + 1; x++) c.set(x, y, kCloth[x == cx - 1 ? 4 : 3]);
          c.set(cx, 22, kBrass[4]); c.set(cx, 20, kBrass[3]);
        } else if (cut == 6) {   // a kilt's plaid sash over the near shoulder
          for (int y = 16; y <= 23; y++)
            for (int k = 0; k < 3; k++) {
              const int x = 5 + (y - 16) + k;
              c.set(x, y, ((x + y) % 3 == 0) ? pat[2] : (k == 0 ? top[4] : top[2]));
            }
          c.set(7, 17, kBrass[4]); c.set(8, 17, kBrass[2]);   // the brooch pinning it
          neckOpening(1, top);
        } else if (cut == 7) {   // a poncho: the yoke round the neck, broad stripes across
          for (int y = 18; y <= 23; y += 3) {
            int x0, x1;
            bustSpan(y, x0, x1);
            for (int x = x0; x <= x1; x++) c.set(x, y, pat[std::max(0, shadeK(x, y) - (y == 23 ? 1 : 0))]);
          }
          neckOpening(0, pat);
        } else if (cut == 8 || dress) {   // a gown or a dress: a wide, low neckline with a band of trim
          for (int x = cx - 3; x <= cx + 3; x++) c.set(x, 16, skin[x < cx ? 3 : 2]);
          for (int x = cx - 2; x <= cx + 2; x++) c.set(x, 17, skin[x < cx ? 2 : 1]);
          for (int x = cx - 4; x <= cx + 4; x++) c.set(x, x == cx - 4 || x == cx + 4 ? 16 : 18, trim[x < cx ? 3 : 1]);
          c.set(cx - 3, 17, trim[3]); c.set(cx + 3, 17, trim[1]);
        } else {
          neckOpening(1, top);
        }
        if (L.pattern && cut != 7) clothPattern();
        break;
      }
    }
  }
  // a little opening at the throat: style 0 a closed collar (trim ramp), 1 the shirt's slit, 2 a gaping tear
  void neckOpening(int style, const Ramp& col) {
    const int cx = 12;
    if (style == 0) { for (int x = cx - 2; x <= cx + 2; x++) c.set(x, 15, col[x < cx ? 3 : (x > cx ? 1 : 2)]); return; }
    c.set(cx, 16, skin[1]); c.set(cx + 1, 16, skin[0]); c.set(cx, 17, skin[0]);
    if (style == 2) { c.set(cx - 1, 16, skin[2]); c.set(cx + 1, 17, skin[0]); c.set(cx, 18, skin[0]); }
    c.set(cx - 1, 15, col[4]); c.set(cx + 2, 15, col[2]);
  }
  void clothPattern() {
    for (int y = 17; y <= 23; y++) {
      int x0, x1;
      bustSpan(y, x0, x1);
      for (int x = x0 + 1; x < x1; x++) {
        bool m = false;
        switch (L.pattern) {
          case 1: m = y % 3 == 0; break;                                   // stripes
          case 2: m = ((x / 2) + (y / 2)) % 2 == 0 && ((x + y) & 1); break;   // checks
          case 3: m = y == 23 || y == 22; break;                            // a border
          case 4: m = (x % 3 == 0) && (y % 3 == 1); break;                  // dots
          case 5: m = y == 19 && (x & 1); break;                            // embroidery
          case 6: m = ((x + y * 2) % 7 == 0); break;                        // vines
          default: break;
        }
        if (m) c.set(x, y, pat[std::clamp(shadeK(x, y), 1, 4)]);
      }
    }
  }
  void pauldron(int px, int py, bool lit) {
    const Ramp& M = top;
    for (int y = py - 3; y <= py + 2; y++)
      for (int x = px - 3; x <= px + 3; x++) {
        const float dx = (x + 0.5f - px) / 3.4f, dy = (y + 0.5f - py) / 2.9f;
        if (dx * dx + dy * dy > 1.0f) continue;
        int k = lightIndex(lightAt(dx * 0.9f, dy * 0.9f), x, y, 0.0f);
        if (!lit) k = std::max(0, k - 1);
        c.set(x, y, M[k]);
      }
    for (int x = px - 3; x <= px + 3; x++) if (solid(c, x, py + 2)) c.set(x, py + 2, M[0]);   // the lame's edge
  }
  // the gorget / the mail coif / the tunic's collar round the neck
  void collar() {
    const int kind = bodyKind();
    if (kind == 3) {
      for (int x = 9; x <= 15; x++) { c.set(x, 15, top[x < 11 ? 4 : (x > 13 ? 1 : 3)]); c.set(x, 16, top[x < 11 ? 2 : (x > 13 ? 0 : 1)]); }
    } else if (kind == 2 && !K.worn) {
      for (int x = 9; x <= 15; x++) for (int y = 15; y <= 16; y++) { const int q = (x + 2 * y) & 3; c.set(x, y, top[q == 0 ? 1 : (q == 2 ? 3 : 2)]); }
    }
  }

  // ---------------------------------------------------------------- cloaks, capes, things on the back
  void backItems() {
    if (L.backItem & 2) {   // a staff over the far shoulder
      for (int y = 2; y <= 23; y++) { c.set(19 + (y < 8 ? 1 : 0), y, kWood[y < 6 ? 3 : 2]); c.set(20 + (y < 8 ? 1 : 0), y, kWood[1]); }
      c.set(20, 1, kGold[4]); c.set(21, 2, kGold[2]);
    }
    if (L.backItem & 1) {   // the bow's limb over the near shoulder, the quiver's fletchings over the far one
      line(c, 2, 19, 5, 13, kWood[3]);
      line(c, 3, 19, 6, 13, kWood[1]);
      c.set(5, 12, kWood[4]);
      const uint32_t fl[3] = {rgba(232, 228, 216), rgba(176, 64, 52), rgba(232, 228, 216)};
      for (int k = 0; k < 3; k++) { c.set(18 + k, 15 - k, fl[k]); c.set(19 + k, 16 - k, shade(fl[k], 0.7f)); }
      for (int y = 16; y <= 17; y++) { c.set(18, y, kLeather[2]); c.set(19, y, kLeather[1]); }
    }
  }
  bool cloaked() const { return L.cloak || L.cape; }
  void cloakBack() {   // the cloak's hood fallen behind the neck (travel cloak) or the cape's upper edge
    if (L.cloak == 1) {
      for (int y = 11; y <= 15; y++)
        for (int x = 6; x <= 18; x++)
          if (inEl(x + 0.5f, y + 0.5f, 12.0f, 15.0f, 6.6f, 3.8f) && !inEl(x + 0.5f, y + 0.5f, 12.0f, 14.2f, 3.4f, 2.4f))
            c.set(x, y, cloak[x < 10 ? 3 : (x > 15 ? 1 : 2)]);
    }
  }
  void cloakFront() {
    if (!cloaked()) return;
    const Ramp& C = L.cloak ? cloak : trim;   // the legacy cape is in the tabard colour
    const int style = L.cloak ? L.cloak : 1;
    // the cloak over both shoulders, open down the chest
    for (int y = 15; y <= 23; y++) {
      int x0, x1;
      bustSpan(y, x0, x1);
      const int openL = 10 - (y - 16) / 3, openR = 14 + (y - 16) / 3;
      for (int x = x0; x <= x1; x++) {
        if (x > openL && x < openR && y >= 16) continue;
        int k = shadeK(x, y);
        if (x == openL || x == openR) k = x == openL ? std::min(4, k + 1) : 0;   // the edges: the near lit, the far turned
        if (style == 3 && (x == openL || x == openR)) { c.set(x, y, gilt[x == openL ? 4 : 2]); continue; }
        if ((x + y) % 5 == 0 && k > 1 && y > 18) k--;   // the drape's folds
        c.set(x, y, C[k]);
      }
    }
    if (style == 2) {   // a fur mantle: a thick ruff over the shoulders
      const Ramp F = ramp(L.cloakColor ? mix(L.cloakColor, rgba(150, 120, 90), 0.5f) : rgba(140, 110, 80));
      for (int y = 14; y <= 19; y++)
        for (int x = 2; x <= 21; x++) {
          const float d = std::fabs((x + 0.5f) - 12.0f);
          const float top0 = 15.0f + d * 0.22f, bot = 17.6f + d * 0.18f + (hash3(x, 0, 41) % 3) * 0.5f;
          if (y + 0.5f < top0 || y + 0.5f > bot || !inBust(x, y) || (d < 3.2f && y < 16)) continue;
          int k = x < 9 ? 3 : (x > 16 ? 1 : 2);
          if (y + 0.5f < top0 + 1.0f) k = std::min(4, k + 1);
          if (hash3(x, y, 43) % 4 == 0) k = std::max(0, k - 1);
          c.set(x, y, F[k]);
        }
    }
    // the clasp at the throat
    const Ramp& M = style == 3 ? kGold : kBrass;
    c.set(11, 16, M[4]); c.set(12, 16, M[3]); c.set(11, 17, M[2]); c.set(12, 17, M[1]);
    c.set(14, 16, M[3]); c.set(13, 16, kLeather[1]);
  }

  // ---------------------------------------------------------------- the neck, the head
  void neck() {
    for (int y = 12; y <= 16; y++)
      for (int x = 10; x <= 14; x++) {
        int k = x == 10 ? 2 : (x >= 13 ? 1 : 2);
        if (y >= 14 && x >= 12) k = 1;
        c.set(x, y, skin[k]);
      }
    if (elf) c.set(10, 16, 0);
  }
  void ear() {
    if (K.worn && K.helmShape != 1 && K.helmShape != 2 && K.helmShape != 3 && K.helmShape != 7 && K.helmShape != 10 &&
        K.helmShape != 11 && K.helmShape != 12) return;   // covered by the helm's cheek pieces
    if (L.hood || K.hw == 1 || K.hw == 5 || K.hw == 8) return;   // hood, veil, headscarf
    const int ex = 6, ey = 8;
    c.set(ex, ey, skin[3]); c.set(ex + 1, ey, skin[2]);
    c.set(ex, ey + 1, skin[2]); c.set(ex + 1, ey + 1, skin[1]);
    c.set(ex, ey + 2, skin[2]); c.set(ex + 1, ey + 2, skin[1]);
    c.set(ex + 1, ey + 3, skin[1]);
    if (half) { c.set(ex - 1, ey - 1, skin[3]); c.set(ex, ey - 1, skin[2]); c.set(ex - 1, ey - 2, skin[3]); }
    if (elf) {   // the long ear swept up and back, its inner fold in shade
      const int pts[6][2] = {{5, 8}, {4, 7}, {4, 6}, {3, 5}, {2, 4}, {2, 3}};
      for (int i = 0; i < 6; i++) { c.set(pts[i][0], pts[i][1], skin[i < 2 ? 3 : 4]); c.set(pts[i][0] + 1, pts[i][1], skin[i < 4 ? 2 : 3]); }
      c.set(5, 7, skin[1]); c.set(6, 8, skin[1]); c.set(5, 6, skin[2]); c.set(6, 7, skin[2]);
    }
    if (L.jewellery & 1) { c.set(ex + 1, ey + 4, kGold[4]); c.set(ex + 1, ey + 5, kGold[2]); }
  }
  void face() {
    // the head as one shaded volume: lit on the forehead and the near cheek, the far cheek and under the jaw in shade
    for (int y = 1; y <= 15; y++)
      for (int x = 4; x <= 19; x++) {
        if (!inHead(x, y)) continue;
        const float nx = (x + 0.5f - 11.8f) / 5.0f, ny = (y + 0.5f - 9.2f) / 6.2f;
        int k = lightIndex(lightAt(nx * 0.95f, ny * 0.95f), x, y, 0.06f);
        k = std::clamp(k, 1, 4);
        if (y >= 13 && x >= 13) k = std::min(k, 1);   // under the jaw
        if (x >= 16) k = std::min(k, 1);
        c.set(x, y, skin[k]);
      }
    // the far cheek's turned edge and the jaw line darker against the neck
    for (int y = 12; y <= 15; y++) for (int x = 9; x <= 16; x++) if (inHead(x, y) && !inHead(x, y + 1)) c.set(x, y, skin[x < 11 ? 2 : 1]);
  }
  bool eyesOpen() const { return mood != 2; }
  void features() {
    const uint32_t eye = K.eye;
    const uint32_t lash = mix(skin[0], kInk, 0.5f);
    const uint32_t white = mix(kWhite, skin[3], 0.25f), whiteDim = mix(kWhite, skin[1], 0.45f);
    const uint32_t brow = L.beard || L.hair != Hair::Bald ? hair[1] : skin[0];
    // brows (hurt: the inner ends drawn down; glad: raised)
    const int bL = mood == 3 ? 5 : 6;
    c.set(8, bL, brow); c.set(9, bL, brow); c.set(10, mood == 1 ? bL + 1 : bL, brow);
    c.set(13, mood == 1 ? bL + 1 : bL, brow); c.set(14, bL, brow); c.set(15, bL + 1, mix(brow, skin[1], 0.4f));
    if (eyesOpen()) {
      c.set(9, 7, lash); c.set(10, 7, lash);
      c.set(13, 7, lash); c.set(14, 7, lash);
      c.set(9, 8, white); c.set(10, 8, eye);
      c.set(13, 8, whiteDim); c.set(14, 8, eye);
      if (mood == 3) { c.set(9, 9, skin[3]); c.set(10, 9, skin[3]); c.set(9, 8, mix(eye, skin[1], 0.4f)); c.set(13, 8, mix(eye, skin[1], 0.4f)); }   // smiling eyes
      else { c.set(10, 9, skin[1]); }   // the lower lid's shade
    } else {   // shut: the lids closed in a soft curve
      c.set(9, 8, lash); c.set(10, 8, lash); c.set(13, 8, lash); c.set(14, 8, lash);
      c.set(9, 7, skin[2]); c.set(10, 7, skin[2]);
    }
    // the nose: the lit ridge, its shade cast toward the lower right, the nostril
    c.set(12, 8, skin[3]); c.set(12, 9, skin[3]); c.set(13, 9, skin[1]);
    c.set(12, 10, skin[2]); c.set(13, 10, skin[0]); c.set(11, 10, skin[2]);
    c.set(12, 11, skin[1]);
    // the mouth
    const uint32_t lip = mix(skin[0], rgba(150, 56, 64), 0.4f), lipLit = mix(skin[3], rgba(220, 120, 120), 0.3f);
    switch (mood) {
      case 1:   // hurt: the jaw clenched, a corner down
        c.set(11, 12, lip); c.set(12, 12, lip); c.set(13, 12, lip); c.set(14, 13, skin[0]);
        break;
      case 2:   // downed: the mouth slack
        c.set(12, 12, mix(lip, kInk, 0.4f)); c.set(13, 12, lip); c.set(12, 13, skin[1]);
        break;
      case 3:   // glad: a smile, its corners lifted
        c.set(10, 11, skin[1]); c.set(11, 12, lip); c.set(12, 12, lip); c.set(13, 12, lip); c.set(14, 11, lip);
        c.set(12, 13, lipLit);
        c.set(9, 10, mix(skin[3], rgba(236, 120, 110), 0.35f)); c.set(15, 10, mix(skin[2], rgba(220, 110, 100), 0.3f));   // warm cheeks
        break;
      default:   // calm: the lips closed
        c.set(11, 12, mix(lip, skin[2], 0.4f)); c.set(12, 12, lip); c.set(13, 12, mix(lip, skin[1], 0.5f));
        break;
    }
    facePaint();
    if (mood == 1) {   // a fresh cut across the near cheek
      c.set(8, 10, kRed[1]); c.set(9, 11, kRed[2]); c.set(10, 12, kRed[1]); c.set(9, 10, kRed[3]);
      c.set(8, 11, kRed[0]);
    }
  }
  void facePaint() {
    const uint32_t col = L.tabardColor ? opaque(L.tabardColor) : kRed[2];
    switch (L.facePaint) {
      case 1: c.set(8, 10, col); c.set(9, 10, col); c.set(14, 10, shade(col, 0.8f)); c.set(15, 10, shade(col, 0.7f)); break;   // stripes
      case 2: c.set(8, 10, col); c.set(15, 10, shade(col, 0.75f)); c.set(12, 6, col); break;                                    // dots
      case 3: for (int x = 8; x <= 16; x++) if (inHead(x, 8)) c.set(x, x == 9 || x == 10 || x == 13 || x == 14 ? 8 : 8, x == 10 || x == 14 ? K.eye : (x < 12 ? col : shade(col, 0.75f))); break;   // the mask band
      case 4: c.set(12, 5, kGold[4]); c.set(11, 5, kGold[3]); c.set(12, 4, kGold[2]); c.set(13, 5, kGold[2]); break;          // the sun mark
      default: break;
    }
  }

  // ---------------------------------------------------------------- hair and beard
  bool hairHidden() const { return K.worn || L.hood || (K.hw && K.hw != 6); }
  bool longHair() const { return L.hair == Hair::Long || L.hair == Hair::Braids || L.hair == Hair::Ponytail || L.hair == Hair::Curls; }
  int hairK(int x, int y, float cx, float cy, float rx, float ry) const {
    const float nx = (x + 0.5f - cx) / rx, ny = (y + 0.5f - cy) / ry;
    int k = lightIndex(lightAt(nx * 0.9f, ny * 0.9f), x, y, 0.0f);
    if ((hash3(x, y >> 1, 5) & 3) == 0 && k > 1) k--;   // strands
    return std::clamp(k, 0, 4);
  }
  // behind the head and the shoulders: long hair's mass
  void hairBack() {
    if (!longHair() || (L.hood || K.hw == 1 || K.hw == 5 || K.hw == 8)) return;
    if (K.worn && K.helmShape == 4) return;   // the great helm hides it
    const bool curls = L.hair == Hair::Curls;
    for (int y = 4; y <= 20; y++)
      for (int x = 3; x <= 20; x++) {
        if (L.hair == Hair::Ponytail) {
          if (!inEl(x + 0.5f, y + 0.5f, 5.8f, 11.0f, 1.6f, 4.6f)) continue;
          c.set(x, y, hair[std::clamp(hairK(x, y, 6.0f, 9.0f, 3.0f, 6.0f) - 1, 0, 3)]);
          continue;
        }
        const float ry = curls ? 6.4f : 8.4f;
        if (!inEl(x + 0.5f, y + 0.5f, 11.6f, 10.6f, 6.6f, ry)) continue;
        if (curls && (hash3(x, y, 9) % 5 == 0) && !inEl(x + 0.5f, y + 0.5f, 11.6f, 10.6f, 5.6f, ry - 1)) continue;
        c.set(x, y, hair[std::clamp(hairK(x, y, 11.0f, 7.0f, 6.6f, ry) - 1, 0, 3)]);
      }
    if (L.hair == Hair::Ponytail) { c.set(5, 6, gilt[3]); c.set(6, 6, gilt[2]); }   // the tie
  }
  void hairFront() {
    if (L.hair == Hair::Bald) {
      if (!hairHidden()) { c.set(9, 4, skin[4]); c.set(10, 4, skin[4]); c.set(9, 5, skin[3]); }   // the shine on the pate
      return;
    }
    const bool circlet = K.hw == 6;
    if (hairHidden() && !circlet) return;
    layered(c, [&](Canvas& p) {
      const bool mohawk = L.hair == Hair::Mohawk;
      const bool curls = L.hair == Hair::Curls;
      const float grow = curls ? 1.2f : 0.7f;
      for (int y = 0; y <= 15; y++)
        for (int x = 3; x <= 20; x++) {
          const float px = x + 0.5f, py = y + 0.5f;
          if (!inEl(px, py, hx - 0.2f, hcy - 0.4f, hrx + grow, hry + grow * 0.8f)) continue;
          // the hairline: over the forehead down to the brows' top, the temple to the ear, the far side to the cheek
          bool on = false;
          if (y <= 4) on = true;
          else if (y == 5) on = x <= 9 || x >= 15 || (L.hair == Hair::Short && x == 12) || x == 10;
          else if (y <= 8) on = x <= 7 || x >= 16;
          else if (y <= 10) on = x <= 6 && L.hair != Hair::Short;
          if (curls && y <= 6 && (x == 11 || x == 13)) on = true;
          if (mohawk) {   // the shaved sides (stubble), the crest down the middle
            const bool crest = std::abs(x - 11) <= 1 || (y <= 1 && std::abs(x - 10) <= 1);
            if (!crest) {
              if (on && inHead(x, y) && ((x + y) & 1)) p.set(x, y, mix(skin[1], hair[1], 0.45f));
              continue;
            }
            on = y <= 5;
          }
          if (!on) continue;
          int k = hairK(x, y, hx - 1.6f, hcy - 2.2f, hrx + 1.0f, hry + 1.0f);
          if (curls && ((x * 3 + y * 5) % 4 == 0)) k = std::min(4, k + 1);
          p.set(x, y, hair[k]);
        }
      if (mohawk) for (int y = 0; y <= 2; y++) for (int x = 10; x <= 12; x++) p.set(x, y, hair[x == 10 ? 4 : (x == 11 ? 3 : 1)]);
      if (L.hair == Hair::Bun) {
        for (int y = 0; y <= 4; y++)
          for (int x = 5; x <= 11; x++)
            if (inEl(x + 0.5f, y + 0.5f, 8.4f, 2.4f, 2.6f, 2.2f)) p.set(x, y, hair[std::clamp(hairK(x, y, 7.6f, 1.6f, 2.6f, 2.2f), 1, 4)]);
        p.set(10, 3, gilt[4]);
      }
      if (L.hair == Hair::Long || L.hair == Hair::Curls) {   // the near lock falls past the ear over the shoulder
        for (int y = 9; y <= 19; y++) {
          const int w = y < 16 ? 2 : 1;
          for (int x = 5; x < 5 + w + (y < 12 ? 1 : 0); x++) p.set(x - (y > 15 ? 0 : 0), y, hair[x == 5 ? 3 : 2]);
        }
        for (int y = 10; y <= 17; y++) { p.set(17, y, hair[1]); p.set(16, y, y > 12 ? hair[1] : hair[2]); }
      }
      if (L.hair == Hair::Braids) {   // two plaits down the front
        for (int y = 10; y <= 20; y++) {
          const int k = (y & 1) ? 3 : 1;
          p.set(6, y, hair[k]); p.set(7, y, hair[k - 1]);
          if (y <= 18) { p.set(17, y, hair[k - 1]); p.set(18, y, hair[std::max(0, k - 2)]); }
        }
        p.set(6, 21, gilt[3]); p.set(7, 21, gilt[2]); p.set(17, 19, gilt[2]);
      }
    }, 0.55f);
  }
  void beard() {
    if (!L.beard) return;
    if (K.worn && (K.helmShape == 4 || K.helmShape == 9 || K.helmShape == 13 || K.helmShape == 14)) return;
    layered(c, [&](Canvas& p) {
      for (int y = 9; y <= 17; y++)
        for (int x = 6; x <= 18; x++) {
          const float px = x + 0.5f, py = y + 0.5f;
          const bool jaw = inEl(px, py, jx, jy + 1.0f, jrx + 0.9f, jry + 0.9f) && y >= 10;
          const bool side = x <= 8 && y >= 9 && y <= 12 && inHead(x, y);
          if (!jaw && !side) continue;
          if (y <= 11 && x >= 9 && x <= 16 && !(y == 11 && x >= 10 && x <= 15)) continue;   // the cheeks stay clear
          if (y == 12 && x >= 11 && x <= 13) continue;   // the mouth
          int k = hairK(x, y, 10.5f, 10.0f, 5.0f, 5.0f);
          if (y == 11) k = std::min(4, k + 1);   // the moustache catches the light
          if (y >= 16) k = std::max(0, k - 1);
          p.set(x, y, hair[k]);
        }
    }, 0.5f);
  }

  // ---------------------------------------------------------------- headwear (the culture's), the legacy hood
  void hoodLike(const Ramp& H, bool veil) {
    // the cloth wraps the head and falls to the shoulders, the face framed by its shaded inner edge
    for (int y = 1; y <= 20; y++)
      for (int x = 2; x <= 21; x++) {
        const float px = x + 0.5f, py = y + 0.5f;
        const bool shell = inEl(px, py, 11.6f, 8.4f, 6.4f, 7.0f) || (y >= 12 && inEl(px, py, 11.6f, 16.0f, veil ? 9.0f : 7.6f, 5.0f));
        if (!shell) continue;
        const bool faceHole = inEl(px, py, 12.3f, 9.8f, 3.9f, 4.9f) && y >= 5;
        if (faceHole) continue;
        if (y >= 15 && !inBust(x, y) && !inEl(px, py, 11.6f, 8.4f, 6.4f, 7.0f)) continue;
        int k = lightIndex(lightAt((px - 10.5f) / 8.0f, (py - 7.0f) / 9.0f), x, y, 0.05f);
        if (inEl(px, py, 12.3f, 9.8f, 4.9f, 5.9f)) k = std::max(0, std::min(k, 2) - 1);   // the inner edge in shade
        if (!veil && (x + y * 2) % 7 == 0 && k > 1 && y > 12) k--;
        c.set(x, y, H[std::clamp(k, 0, 4)]);
      }
    if (veil) for (int x = 7; x <= 16; x++) if (solid(c, x, 4)) c.set(x, 4, gilt[x < 11 ? 4 : 2]);   // the band holding it
  }
  void headwear() {
    if (L.hood && !K.worn) { hoodLike(hood, false); return; }
    if (K.worn) return;
    const Ramp& H = head;
    switch (K.hw) {
      case 1: hoodLike(H, false); break;
      case 5: hoodLike(H, true); break;
      case 8: {   // a headscarf: over the hair, knotted at the nape, the tails behind the neck
        for (int y = 1; y <= 9; y++)
          for (int x = 4; x <= 19; x++) {
            const float px = x + 0.5f, py = y + 0.5f;
            if (!inEl(px, py, 11.4f, 7.6f, 6.0f, 6.0f) || (y >= 5 && x >= 8) ) continue;
            if (y >= 6 && x >= 7) continue;
            c.set(x, y, H[std::clamp(lightIndex(lightAt((px - 10.0f) / 7.0f, (py - 5.0f) / 6.0f), x, y, 0.05f), 0, 4)]);
          }
        for (int x = 7; x <= 17; x++) c.set(x, 5, H[x < 11 ? 3 : (x > 15 ? 1 : 2)]);
        for (int y = 6; y <= 9; y++) { c.set(16 + (y > 7), y, H[1]); }
        c.set(5, 9, H[3]); c.set(4, 10, H[2]); c.set(5, 10, H[1]); c.set(4, 11, H[1]); c.set(3, 12, H[0]);   // the knot's tails
        break;
      }
      case 2: {   // a soft cap tilted back on the head
        for (int y = 0; y <= 6; y++)
          for (int x = 4; x <= 19; x++) {
            const float px = x + 0.5f, py = y + 0.5f;
            if (!inEl(px, py, 11.0f, 3.8f, 6.2f, 2.9f)) continue;
            c.set(x, y, H[std::clamp(lightIndex(lightAt((px - 10.0f) / 6.0f, (py - 2.5f) / 3.0f), x, y, 0.05f), 0, 4)]);
          }
        for (int x = 6; x <= 17; x++) if (solid(c, x, 5)) c.set(x, 5, H[x < 10 ? 2 : 1]);   // the band
        c.set(11, 0, H[4]);
        break;
      }
      case 3: {   // a turban: wound bands round a dome, the jewel at its front
        for (int y = 0; y <= 7; y++)
          for (int x = 4; x <= 19; x++) {
            const float px = x + 0.5f, py = y + 0.5f;
            if (!inEl(px, py, 11.6f, 4.4f, 6.4f, 4.0f)) continue;
            int k = lightIndex(lightAt((px - 10.5f) / 6.4f, (py - 3.4f) / 4.0f), x, y, 0.0f);
            if ((x - y * 2 + 40) % 4 == 0) k = std::max(0, k - 1);   // the wraps' folds
            c.set(x, y, H[std::clamp(k, 0, 4)]);
          }
        c.set(12, 4, kGold[4]); c.set(12, 5, kGold[2]); c.set(13, 4, kGold[3]);
        if (K.hw == 3) { c.set(12, 3, rgba(200, 60, 80)); }
        break;
      }
      case 4: {   // a fur hat: a deep roll of fur, a cloth crown
        const Ramp F = ramp(L.headColor ? L.headColor : rgba(120, 92, 70));
        for (int y = 0; y <= 6; y++)
          for (int x = 4; x <= 19; x++) {
            const float px = x + 0.5f, py = y + 0.5f;
            if (!inEl(px, py, 11.6f, 3.6f, 6.6f, 3.2f)) continue;
            int k = lightIndex(lightAt((px - 10.5f) / 6.6f, (py - 2.6f) / 3.2f), x, y, 0.0f);
            if (hash3(x, y, 71) % 3 == 0) k = std::max(0, k - 1);
            c.set(x, y, F[std::clamp(k, 0, 4)]);
          }
        for (int x = 7; x <= 16; x++) if (solid(c, x, 6)) c.set(x, 6, F[x < 11 ? 2 : 0]);
        break;
      }
      case 6: {   // a circlet across the brow, a stone at its front
        for (int x = 7; x <= 16; x++) if (inHead(x, 5) || solid(c, x, 5)) c.set(x, 5, kGold[x < 10 ? 4 : (x > 14 ? 1 : 3)]);
        c.set(12, 5, rgba(120, 210, 230)); c.set(12, 4, kGold[3]);
        break;
      }
      case 7: {   // a wide straw hat, its brim shading the brow
        const bool straw = !L.headColor;
        const Ramp S = straw ? ramp5(rgba(96, 70, 44), rgba(150, 112, 60), rgba(196, 160, 90), rgba(226, 198, 126), rgba(244, 228, 170)) : H;
        for (int y = 5; y <= 8; y++) for (int x = 6; x <= 17; x++) if (inHead(x, y) && y <= 6) c.set(x, y, skin[1]);   // the shade
        for (int y = 0; y <= 7; y++)
          for (int x = 1; x <= 22; x++) {
            const float px = x + 0.5f, py = y + 0.5f;
            const bool brim = inEl(px, py, 11.6f, 5.0f, 10.4f, 2.4f);
            const float coneW = (py - 0.6f) * 1.6f;
            const bool cone = py >= 0.6f && py <= 5.2f && std::fabs(px - 11.6f) <= coneW;
            if (!brim && !cone) continue;
            int k = brim && !cone ? (py < 5.0f ? 3 : 1) : (px < 11.6f ? 4 : 2);
            if (cone && py > 4.2f) k = std::max(1, k - 1);
            if ((x + y) % 3 == 0 && k > 1) k--;   // the straw's weave
            c.set(x, y, S[k]);
          }
        break;
      }
      default: break;
    }
  }

  // ---------------------------------------------------------------- helmets
  void dome(float cy, float ry, int bottom, const Ramp& M, float rx = 5.4f) {
    for (int y = 0; y <= bottom; y++)
      for (int x = 3; x <= 20; x++) {
        const float px = x + 0.5f, py = y + 0.5f;
        if (!inEl(px, py, 11.6f, cy, rx, ry)) continue;
        int k = lightIndex(lightAt((px - 11.6f) / rx * 0.95f, (py - cy) / ry * 0.95f), x, y, 0.04f);
        c.set(x, y, M[std::clamp(k, 0, 4)]);
      }
    // the rim: a band of metal at the brow, lit on its near side
    for (int x = 3; x <= 20; x++) if (solid(c, x, bottom) && inEl(x + 0.5f, bottom + 0.5f, 11.6f, cy, rx + 0.5f, ry + 0.5f))
      c.set(x, bottom, M[x < 9 ? 4 : (x > 15 ? 1 : 3)]);
  }
  void cheekGuards(const Ramp& M) {
    for (int y = 6; y <= 12; y++) {
      for (int x = 6; x <= 8; x++) if (y <= 11 || x == 8) c.set(x, y, M[x == 6 ? 3 : (x == 7 ? 2 : 1)]);
      for (int x = 16; x <= 17; x++) c.set(x, y, M[x == 16 ? 1 : 0]);
    }
  }
  void helmet() {
    if (!K.worn) return;
    const Ramp& M = helm;
    const int s = K.helmShape;
    switch (s) {
      case 1:   // a leather cap with a seam
        dome(5.6f, 4.6f, 5, M, 5.2f);
        for (int y = 1; y <= 4; y++) c.set(11, y, M[1]);
        break;
      case 2:   // an iron helm with a nasal bar
        dome(5.8f, 5.0f, 6, M);
        for (int y = 6; y <= 9; y++) c.set(12, y, M[y == 6 ? 4 : 3]);
        c.set(13, 9, M[1]);
        break;
      case 3:   // a kettle hat: the dome and its brim, the brim's top lit, its edge turned down into shade
        for (int x = 3; x <= 20; x++)
          for (int y = 4; y <= 8; y++) {
            if (!inEl(x + 0.5f, y + 0.5f, 11.8f, 6.2f, 7.4f, 1.9f)) continue;
            const bool under = !inEl(x + 0.5f, y + 0.5f - 1.0f, 11.8f, 6.2f, 7.4f, 1.9f);
            c.set(x, y, under ? M[0] : M[x < 9 ? 3 : (x > 15 ? 1 : 2)]);
          }
        dome(4.6f, 4.0f, 5, M, 4.8f);
        break;
      case 4: {   // a great helm: the whole head in steel, the eye slit, the breaths
        for (int y = 1; y <= 15; y++)
          for (int x = 5; x <= 18; x++) {
            const float px = x + 0.5f, py = y + 0.5f;
            if (!(inEl(px, py, 11.8f, 6.0f, 6.0f, 5.0f) || (y >= 6 && px >= 6.0f && px <= 17.6f))) continue;
            if (y == 15 && (x < 7 || x > 16)) continue;
            int k = lightIndex(lightAt((px - 11.8f) / 6.4f, (py - 6.0f) / 9.0f), x, y, 0.03f);
            if (x == 12) k = std::min(4, k + 1);   // the ridge down the face
            c.set(x, y, M[std::clamp(k, 0, 4)]);
          }
        const uint32_t v = K.helmVisor ? K.helmVisor : kInk;
        for (int x = 8; x <= 16; x++) if (x != 12) c.set(x, 8, v);
        c.set(14, 11, v); c.set(16, 11, v); c.set(14, 13, v); c.set(16, 13, v);
        if (K.helmAccent) { c.set(10, 8, K.helmAccent); c.set(14, 8, K.helmAccent); }
        break;
      }
      case 5:   // a spangenhelm: a pointed crown of plates on gilt ribs, the nasal
        for (int y = 0; y <= 6; y++)
          for (int x = 5; x <= 18; x++) {
            const float px = x + 0.5f, py = y + 0.5f;
            const float half = 1.0f + py * 1.05f;
            if (std::fabs(px - 11.8f) > std::min(half, 6.2f)) continue;
            int k = lightIndex(lightAt((px - 11.8f) / 6.4f, (py - 4.0f) / 6.0f), x, y, 0.04f);
            c.set(x, y, M[std::clamp(k, 0, 4)]);
          }
        for (int y = 1; y <= 6; y++) { c.set(9 - (y < 4 ? 0 : 1) + (y < 2), y, gilt[3]); c.set(14 + (y > 3), y, gilt[2]); }
        for (int x = 6; x <= 17; x++) c.set(x, 6, gilt[x < 10 ? 4 : 2]);
        for (int y = 7; y <= 10; y++) { c.set(12, y, M[3]); c.set(13, y, M[1]); }
        break;
      case 6: case 13: case 14: {   // horned helms: the dome, horns curving up from its sides
        dome(5.8f, 5.0f, s == 6 ? 6 : 7, M);
        const Ramp& Hn = s == 14 ? M : kBone;
        static const int lh[5][2] = {{6, 5}, {5, 4}, {4, 3}, {4, 2}, {3, 1}}, rh[5][2] = {{17, 5}, {18, 4}, {19, 3}, {19, 2}, {20, 1}};
        for (int i = 0; i < 5; i++) {
          c.set(lh[i][0], lh[i][1], Hn[i < 2 ? 3 : 4]); c.set(lh[i][0] + 1, lh[i][1], Hn[2]);
          c.set(rh[i][0], rh[i][1], Hn[i < 2 ? 1 : 2]); c.set(rh[i][0] - 1, rh[i][1], Hn[1]);
        }
        if (s == 13 || s == 14) {   // the dark helms close over the face: a visor with a glowing slit
          for (int y = 7; y <= 11; y++) for (int x = 7; x <= 17; x++) if (inHead(x, y)) c.set(x, y, M[x < 10 ? 2 : (x > 14 ? 0 : 1)]);
          const uint32_t g = K.helmAccent ? K.helmAccent : kInk;
          c.set(9, 8, g); c.set(10, 8, g); c.set(13, 8, g); c.set(14, 8, shade(g, 0.7f));
          if (s == 14) for (int x = 8; x <= 15; x += 2) c.set(x, 0, K.helmAccent ? K.helmAccent : M[4]);   // the crown's points
        }
        break;
      }
      case 7: case 10: {   // a plumed or crested helm: the dome and a plume / brush crest along its crown
        dome(5.8f, 5.0f, 6, M);
        for (int y = 6; y <= 9; y++) { c.set(12, y, M[3]); c.set(13, y, M[1]); }
        if (s == 10) {
          for (int x = 6; x <= 16; x++) {
            const int y0 = x < 9 ? 2 : (x > 13 ? 2 : 1);
            c.set(x, y0 - 1 + (x > 15), plume[x < 10 ? 4 : 2]); c.set(x, y0, plume[x < 10 ? 3 : 1]);
          }
        } else {
          for (int y = 0; y <= 4; y++) for (int x = 4; x <= 10; x++)
            if (inEl(x + 0.5f, y + 0.5f, 7.4f, 2.0f, 3.4f, 2.0f)) c.set(x, y, plume[(x + y) % 3 == 0 ? 2 : (x < 7 ? 4 : 3)]);
        }
        break;
      }
      case 8:   // a pointed helm with a mail curtain over the neck and shoulders
        for (int y = 6; y <= 17; y++)
          for (int x = 4; x <= 19; x++) {
            const float px = x + 0.5f, py = y + 0.5f;
            if (!inEl(px, py, 11.8f, 10.6f, 7.0f, 7.2f) || (inEl(px, py, 12.4f, 9.8f, 3.6f, 4.4f) && y < 14)) continue;
            if (y > 14 && !inBust(x, y)) continue;
            const int b = x < 9 ? 3 : (x > 15 ? 1 : 2);
            const bool ring = (y & 1) == 0 && ((x + (y >> 1)) & 1);
            c.set(x, y, M[std::clamp(ring ? b - 1 : b, 0, 4)]);
          }
        for (int y = 0; y <= 6; y++)
          for (int x = 5; x <= 18; x++) {
            const float px = x + 0.5f, py = y + 0.5f;
            if (std::fabs(px - 11.8f) > std::min(0.8f + py * 1.0f, 6.0f)) continue;
            c.set(x, y, M[std::clamp(lightIndex(lightAt((px - 11.8f) / 6.0f, (py - 4.0f) / 6.0f), x, y, 0.04f), 0, 4)]);
          }
        for (int x = 6; x <= 17; x++) c.set(x, 6, M[x < 10 ? 4 : 2]);
        break;
      case 9: {   // a masked helm: the dome and a face mask with eye holes
        dome(5.8f, 5.0f, 6, M);
        for (int y = 6; y <= 11; y++) for (int x = 7; x <= 17; x++) if (inHead(x, y)) c.set(x, y, M[x < 10 ? 3 : (x > 15 ? 1 : 2)]);
        const uint32_t v = K.helmVisor ? K.helmVisor : kInk;
        c.set(9, 8, v); c.set(10, 8, v); c.set(13, 8, v); c.set(14, 8, v);
        c.set(12, 10, M[1]); c.set(11, 11, M[0]); c.set(13, 11, M[0]);
        break;
      }
      case 11: case 12: {   // a winged (gilded) helm or the elven peaked helm
        dome(5.8f, 5.0f, 6, M);
        if (s == 11) {
          for (int i = 0; i < 4; i++) {
            for (int k = 0; k <= i; k++) { c.set(6 - i, 5 - k, gilt[4 - (k & 1)]); c.set(17 + i, 5 - k, gilt[2 - (k & 1)]); }
          }
        } else {
          for (int y = 0; y <= 2; y++) c.set(11 + (y == 0), y, M[4]);
          for (int y = 6; y <= 11; y++) { c.set(7, y, M[3]); c.set(17, y, M[1]); }
        }
        if (K.helmAccent) { c.set(12, 4, K.helmAccent); c.set(12, 5, shade(K.helmAccent, 0.7f)); }
        break;
      }
      default: dome(5.8f, 5.0f, 6, M); break;
    }
    // the culture's plume tuft and the horse-tail down the back
    if (s != 7 && s != 10 && (L.crest >= 3 || (L.armsOrnament & 1))) {
      c.set(11, 0, plume[4]); c.set(12, 0, plume[3]); c.set(10, 1, plume[3]); c.set(13, 1, plume[2]);
    }
    if ((L.armsOrnament & 8) || L.crest >= 4) for (int y = 3; y <= 14; y++) { c.set(4 - (y > 8), y, plume[y & 1 ? 2 : 3]); c.set(5 - (y > 8), y, plume[1]); }
    if ((L.armsOrnament & 4) && s != 5) for (int x = 6; x <= 17; x++) if (solid(c, x, 6) && x % 3 == 0) c.set(x, 6, kGold[4]);
  }

  // ---------------------------------------------------------------- jewellery on the chest
  void chest() {
    if (L.jewellery & 2) { for (int x = 9; x <= 15; x++) c.set(x, 16 + (x == 9 || x == 15 ? 0 : 1) - (x >= 11 && x <= 13 ? 0 : 0), kGold[x < 12 ? 4 : 2]); }
    if (L.amulet) {
      c.set(10, 17, kGold[3]); c.set(11, 18, kGold[3]); c.set(14, 17, kGold[2]); c.set(13, 18, kGold[2]);
      c.set(12, 19, rgba(90, 170, 220)); c.set(12, 20, rgba(40, 90, 150)); c.set(11, 19, kGold[4]); c.set(13, 19, kGold[2]);
    }
  }

  void paint() {
    backItems();
    hairBack();
    cloakBack();
    paintBody();
    collar();
    neck();
    if (!cloaked()) chest();
    cloakFront();
    if (cloaked()) chest();
    if (!L.people) ear();   // human ears sit under the hair; the peoples' poke through it (below)
    face();
    features();
    beard();
    hairFront();
    if (L.people) ear();
    headwear();
    helmet();
    if (K.worn && (K.helmShape == 4 || K.helmShape == 9 || K.helmShape == 13 || K.helmShape == 14)) {
      // a closed helm hides the face: the mood shows on the steel (a fresh dent and blood when hurt, a bright gleam glad)
      if (mood == 1) { c.set(9, 4, helm[0]); c.set(10, 5, helm[0]); c.set(9, 5, kRed[2]); c.set(15, 12, kRed[1]); c.set(15, 13, kRed[0]); }
      if (mood == 3) { c.set(8, 3, kWhite); c.set(9, 3, helm[4]); c.set(8, 4, helm[4]); }
    }
    if (mood == 2) {   // downed: the colour drains away
      for (int y = 0; y < c.h; y++)
        for (int x = 0; x < c.w; x++) {
          const uint32_t p = c.get(x, y);
          if (!chA(p)) continue;
          const int l = (int)(luma(p) * 255);
          c.set(x, y, mix(p, rgba(std::min(255, l * 9 / 10 + 8), std::min(255, l * 9 / 10 + 10), std::min(255, l + 18)), 0.62f));
        }
    }
  }
};

}  // namespace

Canvas portraitRaw(const HumanLook& look, int mood) {
  Canvas c(PORTRAIT_W, PORTRAIT_H);
  Bust b(c, look, std::clamp(mood, 0, 3));
  b.paint();
  // the whole bust a row lower: the tallest crest, plume or hat keeps a pixel above it for the outline
  Canvas d(PORTRAIT_W, PORTRAIT_H);
  for (int y = 0; y + 1 < PORTRAIT_H; y++)
    for (int x = 0; x < PORTRAIT_W; x++) d.set(x, y + 1, c.get(x, y));
  return d;
}
Canvas portraitSprite(const HumanLook& look, int mood) {
  Canvas c = portraitRaw(look, mood);
  outline(c);
  return c;
}

// (P2c SWITCH) rig 2: the bust cut from the doll's own frame (A3's face: LOCO idle facing down, as the game draws it), so
// the portrait wears what the figure wears (every helm family, the shapes, the ladder's finish) in the doll's pixels. The
// head box centred, its crown 3-6 rows from the top (a tall crest or plume above that is cut: the full figure stands on
// the party page), the shoulders and chest under it. Moods on the doll's own pixels: 1 hurt (paler skin, a cut on the
// cheek), 2 downed (the colour drained, as the painted bust), 3 glad (the skin a little warmer). `glow` takes the bust's
// glow-plane pixels (T5-T6: the view breathes them). An empty canvas at rig 1 or when the doll does not cover the look.
Canvas portraitDoll(const HumanLook& look, int mood, std::vector<finish::GlowPx>* glow) {
  if (glow) glow->clear();
  if (rigVersion() < 2 || !rig2Covers(look)) return Canvas();
  doll::ComposeOpts one;
  one.onlyFacing = 0;
  const RigSheet s = doll::compose(doll::resolve(look), RigPart::Loco, &one);
  const FrameRect* f = s.frame(0, Clip::Idle, 0, 0);
  if (!f || s.atlas.w <= 0) return Canvas();
  const RigAnchors* an = s.anchorsOf(0, Clip::Idle, 0, 0);
  int hx = f->w / 2 - 5, hy = 2, hw = 10, hh = 11;
  if (an && an->headX != kNoAnchor && an->headY != kNoAnchor && an->headW > 0) { hx = an->headX + f->px; hy = an->headY + f->py; hw = an->headW; hh = an->headH; }
  // the rows of headwear over the crown (the frame's top is the figure's highest pixel)
  const int over = std::max(0, hy);
  const int crownRow = std::clamp(over, 3, 6);
  const int top = hy - crownRow, left = hx + hw / 2 - PORTRAIT_W / 2;
  Canvas c(PORTRAIT_W, PORTRAIT_H);
  for (int y = 0; y < PORTRAIT_H; y++)
    for (int x = 0; x < PORTRAIT_W; x++) {
      const int fx = left + x, fy = top + y;
      if (fx < 0 || fy < 0 || fx >= f->w || fy >= f->h) continue;
      c.set(x, y, s.atlas.get(f->x + fx, f->y + fy));
    }
  if (glow) {
    const size_t fi = (size_t)(f - s.frames.data());
    s.glow.each(fi, [&](int gx, int gy, uint32_t col) {
      const int bx = gx - left, by = gy - top;
      if (bx >= 0 && by >= 0 && bx < PORTRAIT_W && by < PORTRAIT_H) glow->push_back({bx, by, col});
    });
  }
  // the skin: the head box's pixels warm in hue (r over g over b), neither ink nor metal
  auto skin = [&](uint32_t p) {
    const int r = (int)chR(p), g = (int)chG(p), b = (int)chB(p);
    return chA(p) && r > g && g > b && r - b > 40 && r > 110;
  };
  const int faceX = an && an->faceX != kNoAnchor ? an->faceX + f->px - left : PORTRAIT_W / 2 - 4;
  const int faceY = an && an->faceY != kNoAnchor ? an->faceY + f->py - top : crownRow + 4;
  if (mood == 1) {
    for (int y = 0; y < PORTRAIT_H; y++)
      for (int x = 0; x < PORTRAIT_W; x++) if (skin(c.get(x, y))) c.set(x, y, mix(c.get(x, y), rgba(236, 226, 220), 0.22f));
    const int cx = faceX + 7, cy = faceY + 4;
    if (skin(c.get(cx, cy))) c.set(cx, cy, rgba(150, 30, 26));
    if (skin(c.get(cx + 1, cy + 1))) c.set(cx + 1, cy + 1, rgba(190, 44, 36));
  } else if (mood == 2) {
    for (int y = 0; y < c.h; y++)
      for (int x = 0; x < c.w; x++) {
        const uint32_t p = c.get(x, y);
        if (!chA(p)) continue;
        const int l = (int)(luma(p) * 255);
        c.set(x, y, mix(p, rgba(std::min(255, l * 9 / 10 + 8), std::min(255, l * 9 / 10 + 10), std::min(255, l + 18)), 0.62f));
      }
    if (glow) glow->clear();   // the downed: no living light
  } else if (mood == 3) {
    for (int y = 0; y < PORTRAIT_H; y++)
      for (int x = 0; x < PORTRAIT_W; x++) if (skin(c.get(x, y))) c.set(x, y, mix(c.get(x, y), rgba(250, 170, 130), 0.12f));
  }
  return c;
}

}  // namespace art
