// rpg/art/sig_icons_B.inc (P5b production, area B: Dune, Steppe, Marsh at T1-T4; docs/STYLE_P5B.md 4-5 and 7, 9.3 S5)
// Included by rpg/art/art_items.cpp after the style slice's signature painters (sigOf, sigGrid, SigPal), inside its namespace:
// the area's own grids and painters, so the slice's tables and the other areas' stay untouched. The bag says what the doll
// says: each helm icon is its worn head's class at its tier (tools/doll/p5b_sig_B.py draws the worn heads), each shield the
// people's form in its dominant, each cloak the dominant, each coat its layer (the sash, the scarf). T5-T6 (the budget ruling
// pending) keep the classic painters and P5's band in the people's pair. Grids by character as the slice's:
//   k ink, 0-4 the piece's metal, a-e the dominant, f-j the accent, p-t the leather, u-y the gilt, # the face
const char* const kSigHelmB[3][4][16] = {
  {  // Dune: the cream turban wound low, its teal wraps, the brass brooch and the tail; the same over the spiked cap's steel
     // crown; the kulah: the needle over the steel bowl, the teal turban band and its brooch, the mail aventail beside the face
    {"................", "................", "................", "................",
     ".....kkkkkk.....", "...kkjjiihhgk...", "..kjiiihhhhggk..", ".kjiihhgghhhggk.",
     ".kiihhhhhgghggk.", ".kddcccvucbbbbk.", ".kdcccbbbbbbaak.", "..kkkkkkkkkkkhgk",
     "............khgk", "............kgfk", "............kcbk", ".............kk."},
    {"................", "................", "........k.......", ".......k42k.....",
     ".....kkkkkk.....", "...kkjjiihhgk...", "..kjiiihhhhggk..", ".kjiihhgghhhggk.",
     ".kiihhhhhgghggk.", ".kddcccvucbbbbk.", ".kdcccbbbbbbaak.", "..kkkkkkkkkkkhgk",
     "............khgk", "............kgfk", "............kcbk", ".............kk."},
    {"........k.......", ".......k4k......", ".......k3k......", "......k432k.....",
     ".....k44321k....", "....k4433221k...", "...kdeddccbbak..", "..kdddcvucbbbak.",
     "..kddccbbbbbaak.", "..k43k####k21k..", "..k32k####k21k..", "..k43k####k21k..",
     "..k3321####221k.", ".k432211111111k.", ".kkkkkkkkkkkkkk.", "................"},
    {nullptr},
  },
  {  // Steppe: the felt hat (a sky crown on a fur brim, its flaps to the jaw); the peaked felt cap and its rust knot; the
     // pointed helm under the rust horsetail, its guards in courses of laced plates
    {"................", "................", "................", "................",
     "................", ".....kkkkkk.....", "....kgjiihgk....", "...kiihhhggfk...",
     "..krssrsrrqrqk..", ".krsrrqrqqqpqpk.", ".kqrk######kqpk.", ".krqk######kqpk.",
     ".kqqk######kppk.", "..kk........kk..", "................", "................"},
    {"................", "........k.......", ".......kck......", "......kjik......",
     ".....kjihgk.....", "....kjiihggk....", "...kiihhhggfk...", "..krssrsrrqrqk..",
     ".krsrrqrqqqpqpk.", ".kqrk######kqpk.", ".krqk######kqpk.", ".kqqk######kppk.",
     "..kk........kk..", "................", "................", "................"},
    {"...k.......k....", "...kck...kbk....", "....kdc.kck.....", ".....kdcck......",
     "......k3k.......", ".....k432k......", "....k44321k.....", "...k4433221k....",
     "..kkkkkkkkkkk...", "..k3k#####k1k...", ".k43k#####k21k..", ".kkkk#####kkkk..",
     ".k32k#####k21k..", "k4321k...k2110k.", "kkkkk.....kkkkk.", "................"},
    {nullptr},
  },
  {  // Marsh: the bog cap (the hide kerchief) fringed in moss; the rawhide kettle, its bone crest and two moss feathers; the
     // tasselled reed war hat, its moss wrap; the wide war hat drooping, its moss wrap, bulrushes and bone-tipped cords
    {"................", "................", "................", "................",
     ".....kkkkkk.....", "...kkjjiihhgk...", "..kjiiihhhhggk..", ".kjiihhhhhhhggk.",
     ".kiihhhhhhhhgfk.", ".kkdcdcdcbcbkkk.", ".kck.d.c.b.b.kbk", ".kbk.........kak",
     ".kck.........kbk", "..j...........h.", "................", "................"},
    {"...........c....", "......k...kdc...", ".....kjk.kdc....", "....kpjhkkck....",
     "...kssrhrqkk....", "..kssrrrqqqqk...", "..ksrrrrqqqqk...", ".kkkkkkkkkkkkk..",
     "kssrrrrrrqqqqpk.", ".kkkkkkkkkkkkkk.", "...k########k...", "...k########k...",
     "................", "................", "................", "................"},
    {"................", "................", "................", "......kkkk......",
     "....kk4432kk....", "...k44433221k...", "...k43332221k...", "..kddcccbbbbak..",
     "k443333322221110", ".k3222221111110k", ".kek.........kak", ".kdk.........kbk",
     "..k...........k.", "................", "................", "................"},
    {"......r..r......", "......q.rq......", ".......c.d......", "........dc......",
     ".......kk.......", ".....kk43kk.....", "...kk4433322kk..", "..kddddcccbbbak.",
     ".kdddddccccbbbak", "k44443333222211k", "kkkkkkkkkkkkkkkk", "k..k.......k...k",
     "...k.......k....", "...f.......f....", "...g.......g....", "................"},
  },
};
// the coats (over the classic body icon): the Dune kaftan (a teal hem band; from T2 its skirts flared and split for the saddle),
// the Steppe riding coat (its rust hem; T2 the fur collar; T3-T4 the lamellar courses and the strip skirt), the Marsh jerkin
// (bare arms: the sleeves cut away; from T2 the fringed strip skirt, bone-tipped). Rows by tier band; nullptr keeps the row
struct BodyRowsB { int y; const char* r; };
const BodyRowsB kSigBodyB[7][8] = {
  {{12, "...dccccbbbbb..."}, {13, "...dcccbbbbab..."}, {-1, nullptr}, {-1, nullptr}, {-1, nullptr}, {-1, nullptr}, {-1, nullptr}, {-1, nullptr}},   // dune T1 band
  {{12, "...dccccbbbbb..."}, {13, "..kdccccbbbbak.."}, {14, "..kdccck--kbbak."}, {15, "..kkkkk--kkkkk.."}, {-1, nullptr}, {-1, nullptr}, {-1, nullptr}, {-1, nullptr}},   // dune T2 band
  {{13, "...dccccbbbba..."}, {14, "...dcccbbbbba..."}, {-1, nullptr}, {-1, nullptr}, {-1, nullptr}, {-1, nullptr}, {-1, nullptr}, {-1, nullptr}},   // steppe T1 band
  {{1, "..ppp.rqqr.ppp.."}, {2, ".pqqqrrssrrqqqp."}, {3, "...pqrsttsrqp..."}, {13, "...dccccbbbba..."}, {14, "...dcccbbbbba..."}, {-1, nullptr}, {-1, nullptr}, {-1, nullptr}},   // steppe T2 band
  {{7, "...kkkkkkkkkk..."}, {9, "...kkkkkkkkkk..."}, {11, "...kkkkkkkkkk..."}, {13, "...0k1k0k1k0k..."}, {14, "...dk-k-k-k-k..."}, {15, "...d.-.-.-.-...."}, {-1, nullptr}, {-1, nullptr}},   // steppe T3 band
  {{2, "--...........--."}, {3, "--............--"}, {4, "-..............-"}, {13, "...dddcdcdcbb..."}, {14, "...cdcdcbcbba..."}, {-1, nullptr}, {-1, nullptr}, {-1, nullptr}},   // marsh T1 band
  {{2, "--...........--."}, {3, "--............--"}, {4, "-..............-"}, {12, "...dddcdcdcbb..."}, {13, "...d-c-d-c-b-..."}, {14, "...c-d-c-b-a-..."}, {15, "...g.h.h.g.f...."}, {-1, nullptr}},   // marsh T2 band
};
// the cloaks (T4 kits): the Dune burnous (its hood, the teal body, cream edges down the front), the Steppe coat-cloak (a
// fur collar, the sky lining down the opening), the Marsh reed cape (its hem cut into bone-tipped strands)
const char* const kSigCloakB[3][16] = {
  {"................", ".......kk.......", "......kcck......", ".....kdccbk.....",
   "....kdc##cbk....", "....kdc##cbk....", "...kidcccbbhk...", "...kidcccbbhk...",
   "..kidccccbbbhk..", "..kidccccbbbhk..", "..kidccccbbbhk..", ".kiddccccbbbbhk.",
   ".kiddccccbbbbhk.", ".kiddcccbbbbbhk.", ".kkkkkkkkkkkkkk.", "................"},
  {"................", "..kssrrrrrqqqk..", ".ksttsrrrrqqqpk.", ".kstsrqqqqrqqpk.",
   "..kdcccicbbbak..", "..kdcccjhbbbak..", "..kdcccihbbbak..", "..kdccciibbbak..",
   ".kddcccihbbbbak.", ".kddcccihbbbbak.", ".kddcccihbbbbak.", ".kddcccihbbbbak.",
   "kdddccccibbbbaak", "kddcccciibbbbbak", "kkkkkkkkkkkkkkkk", "................"},
  {"................", "....kkkkkkkk....", "...kdddccccbk...", "..kddcdccbcbbk..",
   "..kdcdcccbcbbk..", ".kddcdccbcbbbak.", ".kdcdcccbcbbbak.", ".kddcdccbcbbbak.",
   ".kdcdcccbcbbbak.", "kddcdccbcbbbbaak", "kdcdcccbcbbbbaak", "kd.dc.cb.bb.ba.k",
   "d..c..b..b..b..a", "i..h..i..h..h..h", "................", "................"},
};
int sigIndexB(int people) {   // the area's three peoples (Dune, Steppe, Marsh); -1 every other
  const char* n = fp::row(people).name;
  return !std::strcmp(n, "dune") ? 0 : !std::strcmp(n, "steppe") ? 1 : !std::strcmp(n, "marsh") ? 2 : -1;
}
// the helm grid of a tier: Dune and Steppe T1, T2, T3-T4; Marsh T1, T2, T3, T4 (its war hat widens at T4)
const char* const* sigHelmGridB(int si, int t) {
  const int band = t <= 1 ? 0 : t == 2 ? 1 : (si == 2 && t >= 4) ? 3 : 2;
  return kSigHelmB[si][band];
}
// a signed area-B helm, shield or cloak painted whole (true); false: the classic painter (and P5's band in the pair)
bool sigIconPieceB(Canvas& c, const IconLook& l, const Ramp& M) {
  const fp::Sig* g = sigOf(l);
  const int si = sigIndexB(l.people);
  if (!g || si < 0 || l.tier < 1 || l.tier > 4) return false;   // (T5-T6: the classic painters, the budget ruling pending)
  const Ramp dom = fpRamp(g->dom), acc = fpRamp(g->acc);
  const int t = l.tier;
  switch (l.icon) {
    case Icon::Helmet: {
      const SigPal P{&M, &dom, &acc};
      sigGrid(c, sigHelmGridB(si, t), 16, P);
      return true;
    }
    case Icon::Shield: {
      static const char* const kForm[8] = {"round", "kite", "heater", "tower", "crescent", "oval", "buckler", "leaf"};
      int form = l.form;
      for (int i = 0; i < 8; i++) if (g->shield && !std::strcmp(g->shield, kForm[i])) form = i + 1;
      // T1 plain boards bound in the dominant; T2-T4 the field in the dominant, the rim the piece's own (STYLE_P5B 3)
      shieldIcon(c, form, t <= 1 ? dom : (l.mat == 1 ? kLeather : M), t <= 1 ? kLeather : dom, false);
      {   // (each people's shield its own at every tier, T1's plain boards too: the bosses, the studs, the pales)
        auto on = [&](int x, int y) { return x >= 0 && y >= 0 && x < ICON && y < ICON && (c.get(x, y) >> 24) != 0; };
        if (si == 0) {   // the dhal: small (a ring in from the round's), its rim, four brass bosses round a centre boss
          const Ramp& rim = t <= 1 ? dom : (l.mat == 1 ? kLeather : M);
          for (int y = 0; y < ICON; y++)
            for (int x = 0; x < ICON; x++) {
              if (!on(x, y)) continue;
              const float dx = x + 0.5f - 8.0f, dy = y + 0.5f - 8.0f, r = std::sqrt(dx * dx + dy * dy);
              if (r > 5.7f) c.set(x, y, 0);
              else if (r > 4.7f) c.set(x, y, r > 5.3f ? kInk : rim[dx + dy < -1.0f ? 3 : dx + dy > 1.0f ? 1 : 2]);
            }
          for (const auto& b : {std::pair<int, int>{6, 6}, {9, 6}, {6, 9}, {9, 9}}) c.set(b.first, b.second, kBrass[b.first < 8 ? 4 : 2]);
          c.set(7, 7, kBrass[3]); c.set(8, 7, kBrass[2]); c.set(7, 8, kBrass[2]); c.set(8, 8, kBrass[1]);
        }
        if (si == 1)   // the Steppe's black studded rim: every other rim pixel a dark stud
          for (int y = 0; y < ICON; y++)
            for (int x = 0; x < ICON; x++)
              if (on(x, y) && (!on(x - 1, y) || !on(x + 1, y) || !on(x, y - 1) || !on(x, y + 1)) && ((x + y) & 1)) c.set(x, y, kInk);
        if (si == 2)   // the long oval painted in moss and bone stripes (three bone pales, the light on the left)
          for (int y = 0; y < ICON; y++)
            for (int x : {5, 8, 11})
              if (on(x, y) && on(x - 1, y) && on(x + 1, y) && on(x, y - 1) && on(x, y + 1)) c.set(x, y, acc[x < 7 ? 4 : x < 10 ? 3 : 2]);
      }
      return true;
    }
    case Icon::Cloak: {
      const SigPal P{&M, &dom, &acc};
      sigGrid(c, kSigCloakB[si], 16, P);
      return true;
    }
    default: return false;
  }
}
// an area-B coat's cut and layer (the worn ones, as the slice's sigBodyLayer draws the Jade obi and the Imperial focale): the
// rows of kSigBodyB, then the Dune T1 sash and its T2 and T4 scarf, the Steppe T3-T4 scarf, in the people's dominant, each
// cloth pixel parted from the coat under it by value; the brown stays the coat's (D117). True: it drew (P5's marks step aside)
bool sigBodyLayerB(Canvas& c, const IconLook& l, const Ramp& M) {
  const fp::Sig* g = sigOf(l);
  const int si = sigIndexB(l.people);
  if (!g || si < 0 || l.icon != Icon::Armor || l.tier < 1 || l.tier > 4) return false;
  const fp::SigTier& st = g->t[l.tier];
  const Ramp dom = fpRamp(g->dom);
  auto op = [&](int x, int y) { return x >= 0 && y >= 0 && x < ICON && y < ICON && (c.get(x, y) >> 24) != 0; };
  auto lay = [&](int x, int y, const Ramp& R, int k) {
    if (!op(x, y)) return;
    const float b = luma(c.get(x, y));
    int best = k;
    if (std::fabs(luma(R[k]) - b) < 0.12f)
      for (int d : {-1, 1, -2, 2}) {
        const int q = k + d;
        if (q >= 0 && q <= 4 && std::fabs(luma(R[q]) - b) >= 0.12f) { best = q; break; }
      }
    c.set(x, y, R[best]);
  };
  // the coat's own cut first (the band's rows), then the layer over it
  const Ramp acc = fpRamp(g->acc);
  const int t = l.tier, band = si == 0 ? (t <= 1 ? 0 : 1) : si == 1 ? (t <= 1 ? 2 : t == 2 ? 3 : 4) : (t <= 1 ? 5 : 6);
  for (const BodyRowsB& br : kSigBodyB[band]) {
    if (!br.r) continue;
    for (int x = 0; x < ICON && br.r[x]; x++) {
      const char ch = br.r[x];
      const int y = br.y;
      if (ch == '.') continue;
      if (ch == '-') { c.set(x, y, 0); continue; }
      if (ch == 'k') { c.set(x, y, kInk); continue; }
      if (ch >= '0' && ch <= '4') { c.set(x, y, M[ch - '0']); continue; }
      if (ch >= 'p' && ch <= 't') { c.set(x, y, kLeather[ch - 'p']); continue; }
      const bool d = ch >= 'a' && ch <= 'e';
      if (!op(x, y)) c.set(x, y, (d ? dom : acc)[d ? ch - 'a' : ch - 'f']);   // a pixel past the coat: the cloth itself
      else lay(x, y, d ? dom : acc, d ? ch - 'a' : ch - 'f');
    }
  }
  if (st.layer && !std::strcmp(st.layer, "sash")) {   // the Dune sash: from the right shoulder to the left hip, its knot
    for (int i = 0; i <= 6; i++) { lay(10 - i, 5 + i, dom, i <= 2 ? 1 : 2); lay(9 - i, 5 + i, dom, i <= 2 ? 2 : 3); }
    lay(4, 12, dom, 2);
  }
  if (st.layer && !std::strcmp(st.layer, "scarf")) {   // the scarf round the neck, its end over the chest
    lay(5, 4, dom, 3); lay(10, 4, dom, 1);
    for (int x = 5; x <= 10; x++) lay(x, 5, dom, x <= 6 ? 3 : x >= 9 ? 1 : 2);
    lay(9, 6, dom, 2); lay(9, 7, dom, 1); lay(10, 8, dom, 1);
  }
  return true;
}
