// Magic gallery (M9 "Arcana & Mastery", SPELLFX lane; VISION_PLAN 15.29.7): the spells as the screen draws them, through
// the same World and painter templates the game uses (rpg/view/magic_fx.h, magic_draw.h, magic_surface.h) on a
// software canvas, so the contact sheets the art is judged on are the screen's own pixels.
//   magic_gallery <outDir>   writes (each at 3x, and _1x):
//     magic_schools.png   every school's and essence's spell through its life: wind-up, launch, travel, impact (three
//                         moments), aftermath; plain, upcast and dual-cast; the spectacles (meteor, storm cell, great
//                         summon, starfall)
//     magic_surfaces.png  every surface as an organic pool and the combos' meeting kinds, four animation frames
//     magic_status.png    the status overlays on a person and on a wolf
//   magic_gallery --check   every phase of every school draws something; bigger magnitudes draw bigger; surfaces join
//                           (a 2 x 2 pool has no gap down its middle); two runs are pixel-identical; non-zero on failure
#include <cstring>
#include <map>

#include "rpg/art.h"
#include "rpg/view/magic_surface.h"
#include "tools/preview/preview_util.h"

namespace {

Color fromU(uint32_t u) { return Color((u & 255) / 255.0f, ((u >> 8) & 255) / 255.0f, ((u >> 16) & 255) / 255.0f, (u >> 24) / 255.0f); }
uint32_t toU(Color c) {
  auto q = [](float v) { return (int)std::lround(std::clamp(v, 0.0f, 1.0f) * 255.0f); };
  return rgba(q(c.r), q(c.g), q(c.b), 255);
}

// the software painter (magic_fx.h): alpha blending, additive fills, nearest-scaled tinted blits of the baked sheets
struct SoftPt {
  Canvas& c;
  std::map<int, Canvas>* monsters = nullptr;
  long ops = 0;
  void px(int x, int y, Color k, bool add) {
    if (x < 0 || y < 0 || x >= c.w || y >= c.h || k.a <= 0.002f) return;
    const Color d = fromU(c.get(x, y));
    const float a = std::min(1.0f, k.a);
    const Color o = add ? Color(d.r + k.r * a, d.g + k.g * a, d.b + k.b * a) : Color(d.r + (k.r - d.r) * a, d.g + (k.g - d.g) * a, d.b + (k.b - d.b) * a);
    c.set(x, y, toU(o));
  }
  void fill(float x, float y, float w, float h, Color k, bool add) {
    ops++;
    const int x0 = (int)std::floor(x), y0 = (int)std::floor(y), x1 = (int)std::floor(x + w), y1 = (int)std::floor(y + h);
    for (int yy = y0; yy < y1; yy++)
      for (int xx = x0; xx < x1; xx++) px(xx, yy, k, add);
  }
  void rect(float x, float y, float w, float h, Color k) { fill(x, y, w, h, k, false); }
  void add(float x, float y, float w, float h, Color k) { fill(x, y, w, h, k, true); }
  void blitC(const Canvas& s, int sx, int sy, int sw, int sh, float dx, float dy, float dw, float dh, Color tint, int blend, bool flip) {
    ops++;
    const int X0 = (int)std::floor(dx), Y0 = (int)std::floor(dy), W = (int)std::lround(dw), H = (int)std::lround(dh);
    if (W <= 0 || H <= 0) return;
    for (int y = 0; y < H; y++)
      for (int x = 0; x < W; x++) {
        int u = sx + x * sw / W;
        if (flip) u = sx + sw - 1 - x * sw / W;
        const int v = sy + y * sh / H;
        const Color t = fromU(s.get(u, v));
        if (t.a <= 0) continue;
        px(X0 + x, Y0 + y, Color(t.r * tint.r, t.g * tint.g, t.b * tint.b, t.a * tint.a), blend == 1);
      }
  }
  void blit(mfx::Sheet& s, int sx, int sy, int sw, int sh, float dx, float dy, float dw, float dh, Color tint, int blend, bool flip) {
    blitC(s.c, sx, sy, sw, sh, dx, dy, dw, dh, tint, blend, flip);
  }
  void creature(int mon, int frame, float feetX, float feetY, int rows, float sc, Color tint, int blend, bool flip) {
    if (!monsters) return;
    auto it = monsters->find(mon);
    if (it == monsters->end()) it = monsters->emplace(mon, art::monsterSheet((art::Monster)mon)).first;
    const art::Monster m = (art::Monster)mon;
    const int cw = art::monsterCellW(m), ch = art::monsterCellH(m);
    const int r = rows < 0 ? ch : std::min(rows, ch);
    if (r <= 0) return;
    blitC(it->second, (frame & 7) * cw, 0, cw, r, std::floor(feetX - (float)cw * sc * 0.5f), std::floor(feetY - (float)ch * sc), (float)cw * sc, (float)r * sc, tint, blend, flip);
  }
};

const Canvas& hero() {
  static Canvas c = art::humanSheet(art::HumanLook());
  return c;
}
const Canvas& wolf() {
  static Canvas c = art::monsterSheet(art::Monster::Wolf);
  return c;
}
void putSprite(Board& b, const Canvas& s, int sx, int sy, int w, int h, int x, int y, bool flip = false) {
  for (int j = 0; j < h; j++)
    for (int i = 0; i < w; i++) {
      const uint32_t p = s.get(sx + (flip ? w - 1 - i : i), sy + j);
      if (p >> 24) b.c.set(x + i, y + j, p | 0xFF000000u);
    }
}

// one spell's moment in a cell: a fresh World plays the staged life from 0 to `at` seconds, then draws it
struct Cell { const char* label; uint32_t spell; int upcast; bool dual; float at; };
constexpr int CW = 128, CH = 96;
struct Stage {
  Vec2 hands{44, 50}, target{98, 60};
};
long drawCellIn(Board& B, int ox, int oy, const Cell& cell, std::map<int, Canvas>& mons) {
  const magic::SpellDef* d = mfx::defOf(cell.spell);
  const mfx::Look L = mfx::lookOf(d);
  const float M = magic::magnitudeOf(cell.upcast, cell.dual);
  mfx::World W;
  W.rnd.s = 0x1234567u ^ cell.spell;
  Stage st;
  const Vec2 hands = st.hands + Vec2((float)ox, (float)oy), feet = hands + Vec2(0, 8), tgt = st.target + Vec2((float)ox, (float)oy);
  // the hero and a wolf on the cell's grass
  putSprite(B, hero(), 0, 0, art::HUMAN_W, art::HUMAN_H, (int)feet.x - 8, (int)feet.y - 22);
  if (d && d->form != magic::Form::Summon) putSprite(B, wolf(), 0, 0, art::monsterCellW(art::Monster::Wolf), art::monsterCellH(art::Monster::Wolf), (int)tgt.x - 10, (int)tgt.y - 14, true);
  const float castT = std::max(0.35f, d ? d->castTime : 0.3f) + 0.28f * (float)cell.upcast;
  const bool missile = d && (d->form == magic::Form::Bolt || d->form == magic::Form::Lance || d->form == magic::Form::Orb || d->form == magic::Form::Mark);
  const float speed = d && d->speed > 0 ? d->speed : 220.0f;
  const Vec2 dir = norm(tgt + Vec2(0, -6) - hands);
  const float flight = len(tgt + Vec2(0, -6) - hands) / speed;
  bool launched = false, impacted = false, charged[4] = {};
  const float dt = 1.0f / 60.0f;
  Vec2 mp;
  bool mFly = false;
  for (float t = 0; t <= cell.at + 1e-4f; t += dt) {
    if (t == 0) W.event(magic::Phase::WindUp, L, hands, hands, 1, 1, "");
    for (int i = 1; i <= cell.upcast; i++)
      if (!charged[i] && t >= 0.28f * (float)i) { charged[i] = true; W.event(magic::Phase::Charge, L, hands, hands, magic::magnitudeOf(i, cell.dual), 1, ""); }
    if (!launched && t >= castT) {
      launched = true;
      W.event(magic::Phase::Launch, L, hands, d && (d->form == magic::Form::Meteor || d->form == magic::Form::Storm) ? tgt : hands + dir * 24.0f, M, 1, "");
      if (!missile && d) {
        const Vec2 at = d->form == magic::Form::Nova || d->form == magic::Form::Mend ? feet : tgt;
        if (d->form == magic::Form::Summon) W.event(magic::Phase::Summon, L, tgt, feet, M, 0, "");
        else if (d->form == magic::Form::Ward || d->form == magic::Form::Aura) W.event(magic::Phase::Ward, L, feet, feet, M, 1, "");
        else if (d->form == magic::Form::Blink) W.event(magic::Phase::Blink, L, feet, tgt, M, 1, "");
        else if (d->form == magic::Form::Beam) W.event(magic::Phase::Channel, L, hands, tgt + Vec2(0, -8), M, 1, "");
        else { W.event(magic::Phase::Impact, L, at, at, M, 1, ""); if (d->form != magic::Form::Meteor) W.event(magic::Phase::Aftermath, L, at + Vec2(0, 4), at, M, 0, ""); }
        impacted = true;
      }
    }
    if (launched && missile && !impacted) {
      const float ft = t - castT;
      mp = hands + dir * (6.0f + speed * ft);
      mFly = true;
      if (ft >= flight) { impacted = true; mFly = false; W.missileEnd(L, tgt + Vec2(0, -6), dir * speed, M); }
      else W.trail(L, mp, dir * speed, M, dt, 7);
    }
    if (d && d->form == magic::Form::Beam && launched && t - castT < 0.9f) W.event(magic::Phase::Channel, L, hands, tgt + Vec2(0, -8), M, 1, "");
    W.update(dt, [&](uint32_t id, Vec2& f) { if (id != 1) return false; f = feet; return true; });
    W.sounds.clear();
  }
  SoftPt P{B.c, &mons};
  mfx::Ctx C;
  C.cam = Vec2(0, 0); C.W = (float)B.c.w; C.H = (float)B.c.h; C.t = cell.at; C.dark = 0; C.lightK = 1;
  mfx::drawGround(P, W, C);
  for (const mfx::Fx& f : W.fx)   // the summoned form climbing out (as the game's host draws it)
    if (f.kind == (uint8_t)magic::Phase::Summon) mfx::drawSummonForm(P, W, C, f, (int)art::Monster::Wolf, art::monsterCellH(art::Monster::Wolf), 0);
  if (mFly) { mfx::drawMissileShadow(P, W, C, L, mp, M); mfx::drawMissile(P, W, C, L, mp, dir * speed, M, 0, 7); }
  for (const mfx::Fx& f : W.fx) if (f.delay <= 0) mfx::drawPhaseFx(P, W, C, f);
  mfx::drawParts(P, W, C);
  return P.ops;
}

// a cell drawn on its own canvas (the sky's spectacles stay inside it), then pasted
long drawCell(Board& B, int ox, int oy, const Cell& cell, std::map<int, Canvas>& mons) {
  Board cb(CW, CH);
  const long n = drawCellIn(cb, 0, 0, cell, mons);
  for (int y = 0; y < CH; y++) for (int x = 0; x < CW; x++) B.c.set(ox + x, oy + y, cb.c.get(x, y));
  return n;
}

struct Row { const char* name; uint32_t spell; int upcast; bool dual; };
std::vector<Row> rows() {
  using F = magic::Form;
  using E = magic::Essence;
  using S = magic::School;
  auto sy = [](F f, E e, S s, int size) { return mfx::makeSynth(f, e, s, size, std::string(magic::essenceName(e)) + " " + magic::formName(f)); };
  return {
    {"FIREBOLT", magic::SPELL_FIREBOLT, 0, false}, {"FIREBOLT UPCAST 3", magic::SPELL_FIREBOLT, 3, false}, {"FIREBOLT DUAL", magic::SPELL_FIREBOLT, 0, true},
    {"FROST LANCE", magic::SPELL_FROST_LANCE, 0, false}, {"SPARK", 4, 0, false}, {"STORM NOVA", sy(F::Nova, E::Storm, S::Destruction, 2), 1, false},
    {"MEND", magic::SPELL_MEND, 0, false}, {"STONE WARD", 5, 0, false}, {"LIGHT NOVA", sy(F::Nova, E::Light, S::Restoration, 2), 0, false},
    {"SPIRIT WOLF", 6, 0, false}, {"SHADOW BOLT", sy(F::Bolt, E::Shadow, S::Conjuration, 1), 0, false}, {"FEAR", 7, 0, false},
    {"DREAM BLINK", sy(F::Blink, E::Dream, S::Illusion, 1), 0, false}, {"OAKFLESH", 8, 0, false}, {"STONE NOVA", sy(F::Nova, E::Stone, S::Alteration, 2), 0, false},
    {"TIDE NOVA", sy(F::Nova, E::Tide, S::Alteration, 2), 0, false}, {"VENOM ORB", sy(F::Orb, E::Venom, S::Destruction, 1), 0, false},
    {"FIRE BEAM", sy(F::Beam, E::Fire, S::Destruction, 1), 0, false}, {"BLOOD BOLT", sy(F::Bolt, E::Blood, S::Destruction, 1), 0, false},
    {"STAR MARK", sy(F::Mark, E::Star, S::Destruction, 2), 0, false}, {"SONG BOLT", sy(F::Bolt, E::Song, S::Illusion, 1), 0, false},
    {"VERDANT NOVA", sy(F::Nova, E::Verdant, S::Restoration, 2), 0, false}, {"BONE LANCE", sy(F::Lance, E::Bone, S::Conjuration, 1), 0, false},
    {"WIND BOLT", sy(F::Bolt, E::Wind, S::Alteration, 1), 0, false}, {"RUST NOVA", sy(F::Nova, E::Rust, S::Alteration, 1), 0, false},
    {"ARCANE MARK", sy(F::Mark, E::Arcane, S::Illusion, 1), 0, false},
    {"METEOR (4)", sy(F::Meteor, E::Fire, S::Destruction, 4), 0, false}, {"STORM CELL (4)", sy(F::Storm, E::Storm, S::Destruction, 4), 0, false},
    {"GREAT SUMMON (4)", sy(F::Summon, E::Shadow, S::Conjuration, 4), 0, false}, {"STARFALL (3)", sy(F::Meteor, E::Star, S::Destruction, 3), 0, false},
  };
}

void label(Board& b, int x, int y, const std::string& s, uint32_t col = rgba(255, 250, 230)) {
  for (int j = -1; j < 8; j++) for (int i = -1; i < (int)s.size() * 6; i++) b.c.set(x + i, y + j, rgba(24, 22, 30));
  b.text(x, y, s, col);
}

Canvas schoolsSheet(std::map<int, Canvas>& mons, long* opsOut = nullptr) {
  const std::vector<Row> R = rows();
  struct Col { const char* name; float at; bool rel; };   // rel: seconds after the launch
  const Col cols[7] = {{"WIND-UP", 0.3f, false}, {"LAUNCH", 0.04f, true}, {"TRAVEL", 0.15f, true}, {"IMPACT", 0.27f, true},
                       {"IMPACT +", 0.4f, true}, {"IMPACT ++", 0.62f, true}, {"AFTERMATH", 1.9f, true}};
  Board B(130 + 7 * CW, 14 + (int)R.size() * CH);
  long ops = 0;
  for (int c = 0; c < 7; c++) label(B, 130 + c * CW + 4, 3, cols[c].name);
  for (size_t r = 0; r < R.size(); r++) {
    const magic::SpellDef* d = mfx::defOf(R[r].spell);
    const float castT = std::max(0.35f, d ? d->castTime : 0.3f) + 0.28f * (float)R[r].upcast;
    const bool slow = d && (d->form == magic::Form::Meteor || d->form == magic::Form::Storm || d->form == magic::Form::Summon);
    label(B, 3, 14 + (int)r * CH + 40, R[r].name);
    for (int c = 0; c < 7; c++) {
      float at = cols[c].rel ? castT + cols[c].at * (slow && c >= 2 ? 2.2f : 1.0f) : std::min(cols[c].at, castT - 0.02f);
      ops += drawCell(B, 130 + c * CW, 14 + (int)r * CH, Cell{R[r].name, R[r].spell, R[r].upcast, R[r].dual, at}, mons);
      B.frame(130 + c * CW, 14 + (int)r * CH, CW, CH);
    }
  }
  if (opsOut) *opsOut = ops;
  return B.c;
}

Canvas surfacesSheet() {
  using S = magic::Surface;
  struct Pool { const char* name; S a, b; };
  const Pool pools[] = {{"WATER", S::Water, S::None}, {"ICE", S::Ice, S::None}, {"OIL", S::Oil, S::None}, {"FIRE", S::Fire, S::None},
                        {"POISON", S::Poison, S::None}, {"SHOCKED WATER", S::Shock, S::None}, {"STEAM", S::Steam, S::None}, {"BLOOD", S::Blood, S::None},
                        {"WATER + ICE", S::Water, S::Ice}, {"OIL + FIRE", S::Oil, S::Fire}, {"WATER + SHOCK", S::Water, S::Shock}, {"WATER + STEAM", S::Water, S::Steam}};
  const int PW = 8 * 16, PH = 6 * 16;
  const int n = (int)(sizeof(pools) / sizeof(pools[0]));
  Board B(150 + 4 * (PW + 8), 14 + n * (PH + 8));
  for (int f = 0; f < 4; f++) label(B, 150 + f * (PW + 8) + 4, 3, f < 3 ? ("FRAME " + std::to_string(f)) : std::string("FRAME 3 + OVER"));
  for (int i = 0; i < n; i++) {
    magic::Field F;
    // an organic pool: a ragged ellipse of tiles, a lone tile beside it, the second kind on its right half
    for (int y = 0; y < 6; y++)
      for (int x = 0; x < 8; x++) {
        const float u = ((float)x + 0.5f) / 7.0f * 2 - 1, v = ((float)y + 0.5f) / 5.0f * 2 - 1;
        if (u * u + v * v > 0.85f + 0.5f * mfx::hf(::hash2(x, y, (uint32_t)i))) continue;
        if (x == 7 && y == 0) continue;
        magic::SurfaceTile t;
        t.tx = (int16_t)x; t.ty = (int16_t)y; t.kind = (pools[i].b != S::None && x >= 4) ? pools[i].b : pools[i].a; t.life = 20; t.age = 5; t.power = 2;
        t.seed = ::hash2(x, y, 99u);
        F.tiles.push_back(t);
      }
    magic::SurfaceTile lone;
    lone.tx = 7; lone.ty = 0; lone.kind = pools[i].a; lone.life = 20; lone.age = 5; lone.seed = 5;
    F.tiles.push_back(lone);
    F.serial = 1;
    mfx::SurfLayer L;
    L.sync(F, 0, 0, 1 << 20);
    mfx::World W;
    label(B, 3, 14 + i * (PH + 8) + PH / 2, pools[i].name);
    for (int f = 0; f < 4; f++) {
      const int ox = 150 + f * (PW + 8), oy = 14 + i * (PH + 8);
      SoftPt P{B.c, nullptr};
      mfx::Ctx C;
      C.cam = Vec2(-(float)ox, -(float)oy); C.W = (float)B.c.w; C.H = (float)B.c.h; C.t = (float)f / 6.0f + 0.01f; C.dark = 0; C.lightK = 1;
      L.draw(P, C, F);
      if (f == 3) L.drawOver(P, W, C, F);
      B.frame(ox, oy, PW, PH);
    }
  }
  return B.c;
}

Canvas statusSheet() {
  Board B(10 + 9 * 56, 14 + 2 * 60);
  mfx::World W;
  for (int s = 0; s < magic::STATUSES; s++) {
    label(B, 10 + s * 56, 3, std::string(magic::statusName((magic::Status)s)).substr(0, 8));
    for (int row = 0; row < 2; row++) {
      const int ox = 10 + s * 56, oy = 14 + row * 60;
      const Vec2 feet((float)ox + 28, (float)oy + 46);
      if (row == 0) putSprite(B, hero(), 0, 0, art::HUMAN_W, art::HUMAN_H, (int)feet.x - 8, (int)feet.y - 22);
      else putSprite(B, wolf(), 0, 0, art::monsterCellW(art::Monster::Wolf), art::monsterCellH(art::Monster::Wolf), (int)feet.x - 10, (int)feet.y - 14);
      const float bw = row == 0 ? 12.0f : 14.0f, bh = row == 0 ? 22.0f : 13.0f;
      const uint16_t bits = magic::statusBit((magic::Status)s);
      for (int k = 0; k < 40; k++) { mfx::statusTick(W, feet, bits, bw, bh, 1.0f / 30); W.update(1.0f / 30, nullptr); }
      SoftPt P{B.c, nullptr};
      mfx::Ctx C;
      C.cam = Vec2(0, 0); C.W = (float)B.c.w; C.H = (float)B.c.h; C.t = 1.3f; C.dark = 0; C.lightK = 1;
      mfx::drawStatus(P, W, C, feet, bw, bh, bits, (uint32_t)(s * 7 + row));
      mfx::drawParts(P, W, C);
      W.parts.clear();
      B.frame(ox, oy, 56, 60);
    }
  }
  return B.c;
}

int check() {
  int fails = 0;
  std::map<int, Canvas> mons;
  // every phase of every school draws something; a dual-cast draws more than a plain cast
  for (const Row& r : rows()) {
    const magic::SpellDef* d = mfx::defOf(r.spell);
    const float castT = std::max(0.35f, d ? d->castTime : 0.3f);
    for (float at : {0.25f, castT + 0.05f, castT + 0.3f}) {
      Board a(CW, CH), b(CW, CH);
      drawCell(a, 0, 0, Cell{r.name, r.spell, r.upcast, r.dual, at}, mons);
      drawCell(b, 0, 0, Cell{r.name, r.spell, r.upcast, r.dual, at}, mons);
      Board base(CW, CH);
      int diff = 0, nd = 0;
      for (size_t i = 0; i < a.c.px.size(); i++) { diff += a.c.px[i] != base.c.px[i]; nd += a.c.px[i] != b.c.px[i]; }
      if (diff < 30) { std::printf("FAIL %s at %.2f s: only %d pixels drawn\n", r.name, at, diff); fails++; }
      if (nd) { std::printf("FAIL %s at %.2f s: two runs differ by %d pixels\n", r.name, at, nd); fails++; }
    }
  }
  {
    auto drawn = [&](bool dual) {
      Board a(CW, CH), base(CW, CH);
      drawCell(a, 0, 0, Cell{"F", magic::SPELL_FIREBOLT, dual ? 2 : 0, dual, 0.35f + (dual ? 0.56f : 0.0f) + 0.3f}, mons);
      int n = 0;
      for (size_t i = 0; i < a.c.px.size(); i++) n += a.c.px[i] != base.c.px[i];
      return n;
    };
    const int plain = drawn(false), big = drawn(true);
    std::printf("firebolt impact: plain %d px, upcast 2 + dual %d px\n", plain, big);
    if (big < plain * 1.3f) { std::printf("FAIL the upcast dual-cast is not visibly bigger\n"); fails++; }
  }
  {   // a 2 x 2 pool of water: every pixel down its middle is covered (the tiles join with no seam)
    magic::Field F;
    for (int y = 0; y < 2; y++)
      for (int x = 0; x < 2; x++) { magic::SurfaceTile t; t.tx = (int16_t)x; t.ty = (int16_t)y; t.kind = magic::Surface::Water; t.life = 9; t.age = 9; F.tiles.push_back(t); }
    F.serial = 1;
    mfx::SurfLayer L;
    L.sync(F, 0, 0, 99);
    int gaps = 0;
    for (const magic::SurfaceTile& t : F.tiles) {
      const int s = L.at[mfx::SurfLayer::key(t.tx, t.ty)];
      const int ax = (s % mfx::SURF_COLS) * 64, ay = (s / mfx::SURF_COLS) * 16;
      for (int k = 4; k < 12; k++) {   // the edge it shares with its neighbours
        const int ex = t.tx == 0 ? 15 : 0, ey = t.ty == 0 ? 15 : 0;
        gaps += (L.atlas.c.get(ax + ex, ay + k) >> 24) == 0;
        gaps += (L.atlas.c.get(ax + k, ay + ey) >> 24) == 0;
      }
    }
    std::printf("2x2 water pool: %d uncovered pixels on the shared edges\n", gaps);
    if (gaps) { std::printf("FAIL the surface tiles do not join\n"); fails++; }
  }
  std::printf("%s (%d failure%s)\n", fails ? "magic gallery CHECK FAILED" : "magic gallery: all checks passed", fails, fails == 1 ? "" : "s");
  return fails ? 1 : 0;
}
}  // namespace

int main(int argc, char** argv) {
  if (argc > 1 && !std::strcmp(argv[1], "--check")) return check();
  const std::string dir = argc > 1 ? argv[1] : ".";
  std::map<int, Canvas> mons;
  long ops = 0;
  const Canvas a = schoolsSheet(mons, &ops);
  savePng(a, dir + "/magic_schools.png", 3);
  savePng(a, dir + "/magic_schools_1x.png", 1);
  const Canvas b = surfacesSheet();
  savePng(b, dir + "/magic_surfaces.png", 3);
  savePng(b, dir + "/magic_surfaces_1x.png", 1);
  const Canvas c = statusSheet();
  savePng(c, dir + "/magic_status.png", 4);
  std::printf("painter ops for the schools sheet: %ld\n", ops);
  return 0;
}
