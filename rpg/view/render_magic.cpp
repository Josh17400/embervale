// M9 "Arcana & Mastery" (VISION_PLAN 15.29.7, 15.30): magic on screen and in the ear. SPELLFX lane.
// The View's entry points for the spells; the look itself lives View-free in magic_fx.h / magic_draw.h /
// magic_surface.h (so the gallery, tools/preview/magic_gallery.cpp, renders the same pixels):
//   magicEvent       Ev::Spell -> the World (effects, particles, decals, sounds, kicks, flashes)
//   magicUpdate      staged script events, synthetic missiles' ends, the missiles' trails, the statuses' sheddings,
//                    the World's ageing, its requests (sounds through engine/audio, the screen kick, flash, lift),
//                    the surfaces' atlas following magic::Field
//   drawMagicGround  under the bodies: the surfaces, decals, sigils, rifts, ground rings
//   drawSpellProj    the sorted pass: a missile's shadow and its light on the ground (its lit body is drawn after the
//                    lighting, so it burns through the night)
//   magicLights      the light map: missiles, wind-ups, impacts, beams, burning and shocked ground, burning bodies;
//                    lightning lifts the whole night for a frame
//   drawMagicFx      after the lighting: missiles, every phase, the summoned forms climbing out, particles, the status
//                    overlays, flames / clouds / arcs over the surfaces, the scene flash
// Budget: pooled particles (900 desktop / 560 phone), effects and decals capped, sheets baked once, at most 48 (24 on a
// phone) surface tiles baked a frame; magicFxStats().ms is the magic's own draw time.
#include <algorithm>
#include <chrono>
#include <cmath>
#include <vector>
#include "engine/music_style.h"
#include "rpg/art.h"
#include "rpg/sim/game.h"
#include "rpg/sim/magic.h"
#include "rpg/sim/magic_ops.h"
#include "rpg/view/magic_state.h"
#include "rpg/view/prop_traits.h"
#include "rpg/view/view.h"

namespace {
using Clock = std::chrono::steady_clock;
double msSince(Clock::time_point t0) { return std::chrono::duration<double, std::milli>(Clock::now() - t0).count(); }

// the game's painter (magic_fx.h): Pix rects, blends, the sheets baked into textures the first time they are drawn
struct GamePt {
  Pix& P;
  const std::vector<Tex>& monsters;
  void rect(float x, float y, float w, float h, Color c) { if (c.a > 0.003f) P.rect(x, y, w, h, c); }
  void add(float x, float y, float w, float h, Color c) { if (c.a > 0.003f) P.rectAdd(x, y, w, h, c); }
  void blit(mfx::Sheet& s, int sx, int sy, int sw, int sh, float dx, float dy, float dw, float dh, Color tint, int blend, bool flip) {
    if (tint.a <= 0.003f) return;
    if (!s.tex.t) {
      if (s.stream) { s.tex = P.makeStream(s.c.w, s.c.h); s.dirty = true; }
      else s.tex = P.bake(s.c);
    }
    if (s.stream && s.dirty) { P.updateStream(s.tex, s.c.px.data()); s.dirty = false; }
    P.blitEx(s.tex, sx, sy, sw, sh, dx, dy, dw, dh, flip, tint, blend);
  }
  void creature(int mon, int frame, float feetX, float feetY, int rows, float sc, Color tint, int blend, bool flip) {
    if (mon < 0 || mon >= (int)monsters.size() || tint.a <= 0.003f) return;
    const art::Monster m = (art::Monster)mon;
    const int cw = art::monsterCellW(m), ch = art::monsterCellH(m);
    const int r = rows < 0 ? ch : std::min(rows, ch);
    if (r <= 0) return;
    const float x = std::floor(feetX - (float)cw * sc * 0.5f), y = std::floor(feetY - (float)ch * sc);
    P.blitEx(monsters[(size_t)mon], (frame & 7) * cw, 0, cw, r, x, y, (float)cw * sc, (float)r * sc, flip, tint, blend);
  }
};

// (M9 fixer) which surface tiles stand behind a tall prop (a tree, a stall, a standing stone): a non-flat prop at or
// below the tile's row whose sprite covers the tile's flames (the tile and the 12 px they rise). Those tiles draw their
// flames, arcs and clouds in the ground pass, under the y-sorted scene, so the tree in front hides them; the rest draw
// over the bodies as before.
std::vector<uint8_t> surfaceBehindProps(const Game& g) {
  const magic::Field& F = g.arcana.live.field;
  std::vector<uint8_t> hide(F.tiles.size(), 0);
  const Map& m = g.map();
  if (m.prop.empty()) return hide;
  for (size_t i = 0; i < F.tiles.size(); i++) {
    const int tx = F.tiles[i].tx, ty = F.tiles[i].ty;
    const float fx0 = (float)tx * 16, fx1 = fx0 + 16, fy0 = (float)ty * 16 - 12, fy1 = (float)ty * 16 + 16;
    bool behind = false;
    for (int py = ty; py <= ty + 6 && !behind; py++)
      for (int px = tx - 3; px <= tx + 3 && !behind; px++) {
        if (px < 0 || py < 0 || px >= m.w || py >= m.h || (px == tx && py == ty)) continue;
        const int pr = m.prop[(size_t)py * m.w + px];
        if (!pr) continue;
        const art::Prop p = (art::Prop)(pr - 1);
        if (flatProp(p)) continue;
        const int w = art::propW(p), h = art::propH(p);
        if (h < 20) continue;   // low cover (tufts, flowers) is not a wall to hide fire behind
        const float x0 = (float)px * 16 + 8 - (float)w * 0.5f + 2, x1 = x0 + (float)w - 4;
        const float y1 = (float)py * 16 + 16, y0 = y1 - (float)h + 2;
        if (x0 < fx1 && x1 > fx0 && y0 < fy1 && y1 > fy0 && (py > ty || y1 >= fy1)) behind = true;
      }
    hide[i] = behind ? 1 : 0;
  }
  return hide;
}

const Actor* actorById(const Game& g, uint32_t id) {
  if (!id) return nullptr;
  for (const Actor& a : g.actors) if ((uint32_t)a.id == id) return &a;
  return nullptr;
}
mfx::Look lookFor(uint32_t spell) { return mfx::lookOf(mfx::defOf(spell)); }
bool isMagicMissile(const Projectile& pr) {
  return pr.kind == ProjKind::Spell || (pr.spell && (pr.kind == ProjKind::Fireball || pr.kind == ProjKind::IceSpike));
}
// the night's weight for what the magic draws after the lighting (as the beasts' glows: render.cpp drawWeather)
void nightOf(const Game& g, float& dark, float& lightK) {
  const bool dungeon = g.inside && g.subSite >= 0;
  dark = dungeon ? 0.7f : g.inside ? clampf(1.0f - g.daylight(), 0, 1) * 0.6f : clampf(1.0f - g.daylight() * 1.4f, 0, 1);
  lightK = 1.0f - 0.72f * dark;
}
// a body's size for its overlays
void bodyOf(const Actor& a, float& bw, float& bh) {
  if (a.human || a.player || a.npc || a.comp) { bw = 12; bh = 22; return; }
  const float sc = a.scalePct ? (float)a.scalePct / 100.0f : 1.0f;
  bw = std::max(10.0f, (float)art::monsterCellW(a.mon) * 0.62f * sc);
  bh = std::max(10.0f, (float)art::monsterCellH(a.mon) * 0.78f * sc);
}
Vec2 feetOf(const Actor& a) { return a.p + Vec2(0, a.fly ? -20.0f : 0.0f); }
}  // namespace

// ------------------------------------------------------------------ the Ops
View::MagicFxState& MagicFxOps::st(View& v) {
  if (!v.magicFx_) {
    v.magicFx_ = std::make_shared<View::MagicFxState>();
    v.magicFx_->w.caps = mfx::capsFor(v.touchUI);   // (the phone: touch controls on)
  }
  return *v.magicFx_;
}
Pix& MagicFxOps::pix(View& v) { return *v.pix_; }
Audio& MagicFxOps::audio(View& v) { return *v.audio_; }
uint64_t MagicFxOps::musicStyle(View& v) { return v.musicStyle_; }
void MagicFxOps::stage(View& v, float delay, const Event& e) {
  View::MagicFxState& S = st(v);
  View::MagicFxState::Staged s;
  s.at = S.clock + std::max(0.0f, delay); s.e = e;
  S.staged.push_back(s);
}
void MagicFxOps::stageMissile(View& v, float delay, const Projectile& pr) {
  View::MagicFxState& S = st(v);
  View::MagicFxState::Staged s;
  s.at = S.clock + std::max(0.0f, delay); s.missile = true; s.pr = pr;
  S.staged.push_back(s);
}
void MagicFxOps::resetPeaks(View& v) {
  View::MagicFxState& S = st(v);
  S.peak = View::MagicFxStats(); S.msWorst = 0; S.msSum = 0; S.msFrames = 0; S.updWorst = 0;
}
bool mfxScorched(View& v, float x, float y) {
  const mfx::World& W = MagicFxOps::st(v).w;
  for (const mfx::Decal& d : W.decals) {
    if (d.kind != mfx::D_CRATER || d.life - d.t < 1.5f) continue;   // (its last fading seconds: the ground grows back)
    const float rx = (float)mfx::decalW(d.kind) * d.size * 0.5f * 0.62f, ry = (float)mfx::decalH(d.kind) * d.size * 0.5f * 0.62f;
    const float dx = (x - d.p.x) / rx, dy = (y - d.p.y) / ry;
    if (dx * dx + dy * dy < 1.0f) return true;
  }
  return false;
}

bool mfxClaimExplode(View& v, Vec2 p) {
  View::MagicFxState& S = MagicFxOps::st(v);
  for (const mfx::Fx& f : S.w.fx)
    if (f.kind == (uint8_t)magic::Phase::Impact && f.t < 0.05f && len2(f.p - p) < 14 * 14) return true;
  return false;
}
void mfxNoteSfx(View& v, int sfx) {
  View::MagicFxState& S = MagicFxOps::st(v);
  switch ((Sfx)sfx) {
    case Sfx::Fireball: S.classicSfx[0] = true; break;
    case Sfx::Frost: S.classicSfx[1] = true; break;
    case Sfx::Heal: S.classicSfx[2] = true; break;
    case Sfx::Explode: S.classicSfx[3] = true; break;
    default: break;
  }
}

View::MagicFxStats View::magicFxStats() const { return magicFx_ ? magicFx_->last : MagicFxStats(); }

// ------------------------------------------------------------------ events
void View::magicEvent(Game& g, const Event& e) {
  View::MagicFxState& S = MagicFxOps::st(*this);
  const mfx::Look L = lookFor(e.spell);
  const magic::Phase ph = (magic::Phase)std::clamp(e.a, 0, (int)magic::Phase::COUNT - 1);
  const size_t before = S.w.fx.size();
  S.w.event(ph, L, e.p, e.q, e.f, e.id, e.s);
  if (ph == magic::Phase::Summon)   // the form that climbs out: the summoned actor's own, else a spirit wolf
    for (size_t i = before; i < S.w.fx.size(); i++) {
      mfx::Fx& f = S.w.fx[i];
      if (f.kind != (uint8_t)magic::Phase::Summon) continue;
      const Actor* a = actorById(g, e.id);
      f.sub = (uint8_t)(a && !a->human ? (int)a->mon : (int)art::Monster::Wolf);
    }
}

void View::magicUpdate(Game& g, float dt) {
  View::MagicFxState& S = MagicFxOps::st(*this);
  mfx::World& W = S.w;
  const auto tu = Clock::now();
  S.clock += dt;
  // staged script events and missiles that are due
  for (size_t i = 0; i < S.staged.size();) {
    if (S.staged[i].at > S.clock) { i++; continue; }
    const View::MagicFxState::Staged st = S.staged[i];
    S.staged.erase(S.staged.begin() + (std::ptrdiff_t)i);
    if (st.missile) g.projs.push_back(st.pr);
    else magicEvent(g, st.e);
  }
  // synthetic missiles: the sim has no def for them, so the view gives them their impact and aftermath when they end
  for (View::MagicFxState::Flying& f : S.flying) f.seen = false;
  for (const Projectile& pr : g.projs) {
    if (pr.kind != ProjKind::Spell || !mfx::isSynth(pr.spell)) continue;
    View::MagicFxState::Flying* best = nullptr;
    float bd = 24 * 24;
    for (View::MagicFxState::Flying& f : S.flying) {
      if (f.seen || f.spell != pr.spell) continue;
      const float d = len2(f.p + f.v * dt - pr.p);
      if (d < bd) { bd = d; best = &f; }
    }
    if (!best) { S.flying.push_back(View::MagicFxState::Flying{pr.spell, pr.p, pr.v, pr.mag, true}); continue; }
    best->p = pr.p; best->v = pr.v; best->mag = pr.mag; best->seen = true;
  }
  for (size_t i = 0; i < S.flying.size();) {
    const View::MagicFxState::Flying f = S.flying[i];
    if (f.seen) { i++; continue; }
    S.flying.erase(S.flying.begin() + (std::ptrdiff_t)i);
    const mfx::Look L = lookFor(f.spell);
    const Vec2 end = f.p + f.v * dt;
    W.missileEnd(L, end, f.v, f.mag);
    if (L.leaves != magic::Surface::None) {
      const int tx = (int)std::floor(end.x / TILE), ty = (int)std::floor((end.y + 8) / TILE);
      const int r = L.size >= 3 ? 2 : L.size >= 2 || f.mag >= 1.5f ? 1 : 0;
      for (int j = -r; j <= r; j++)
        for (int k = -r; k <= r; k++)
          if (k * k + j * j <= r * r + r / 2) MagicOps::laySurface(g, tx + k, ty + j, L.leaves, 8.0f + 2.0f * (float)L.size, (uint8_t)std::min(3, 1 + (int)L.size / 2), g.pl().id);
    }
  }
  // the missiles' sheddings (embers, shards, sparks, motes)
  for (const Projectile& pr : g.projs)
    if (isMagicMissile(pr)) W.trail(lookFor(pr.spell), pr.p, pr.v, pr.mag, dt, (uint32_t)(pr.from.x * 7 + pr.from.y * 13));
  // the statuses' sheddings
  if (g.mode != Mode::Title)
    for (const Actor& a : g.actors) {
      if (!a.status || a.st == AState::Dead) continue;
      if (len2(a.p - g.pl().p) > 420.0f * 420.0f) continue;
      float bw, bh;
      bodyOf(a, bw, bh);
      mfx::statusTick(W, feetOf(a), a.status, bw, bh, dt);
    }
  W.update(dt, [&](uint32_t id, Vec2& feet) {
    const Actor* a = actorById(g, id);
    if (!a) return false;
    feet = feetOf(*a);
    return true;
  });
  // the requests
  {
    const float hear = 1.0f;
    for (const mfx::SoundReq& r : W.sounds) {
      const int si = (int)r.s;
      if (!r.motif && S.clock - S.sfxT[si] < 0.05f) continue;   // a burst of events: one voice
      if ((S.classicSfx[0] && r.s == Sfx::FireCast) || (S.classicSfx[1] && r.s == Sfx::FrostCast) ||
          (S.classicSfx[2] && (r.s == Sfx::HolyCast || r.s == Sfx::HolyBloom)) || (S.classicSfx[3] && r.s == Sfx::FireBurst))
        continue;   // the classic spell already sounds there
      S.sfxT[si] = S.clock;
      if (r.motif) {
        const MusicStyle ms = MusicStyle::unpack(MagicFxOps::musicStyle(*this));
        static const int roots[7] = {62, 74, 57, 76, 67, 69, 72};
        audio_->playMotif(r.motifId, (uint8_t)ms.scale, r.timbre, roots[r.timbre % 7], r.vol * hear);
      } else audio_->play(r.s, r.pitch, r.vol * hear);
    }
    W.sounds.clear();
    for (bool& b : S.classicSfx) b = false;
    if (W.shake > 0) { shake_ = std::max(shake_, W.shake); W.shake = 0; }
    if (W.flashA > S.flashA) { S.flashA = W.flashA; S.flashC = W.flashC; }
    if (W.lift > S.liftA) { S.liftA = W.lift; S.liftC = W.liftC; }
    W.flashA = 0; W.lift = 0;
  }
  S.surf.sync(g.arcana.live.field, g.world.ox, g.world.oy, touchUI ? 16 : 32);
  S.updMs = msSince(tu);
  S.updWorst = std::max(S.updWorst, S.updMs);
}

void View::magicShift(Vec2 d) {
  if (!magicFx_) return;
  magicFx_->w.shift(d);
  for (auto& s : magicFx_->staged) { s.e.p += d; s.e.q += d; s.pr.p += d; s.pr.from += d; }
  for (auto& f : magicFx_->flying) f.p += d;
}

// ------------------------------------------------------------------ drawing
void View::drawMagicGround(Game& g, Vec2 cam) {
  View::MagicFxState& S = MagicFxOps::st(*this);
  // a new frame's count begins (the ground pass is the frame's first magic draw)
  S.last = S.cur;
  S.peak.effects = std::max(S.peak.effects, S.cur.effects);
  S.peak.particles = std::max(S.peak.particles, S.cur.particles);
  S.peak.lights = std::max(S.peak.lights, S.cur.lights);
  S.peak.decals = std::max(S.peak.decals, S.cur.decals);
  S.peak.surfaceTiles = std::max(S.peak.surfaceTiles, S.cur.surfaceTiles);
  S.peak.missiles = std::max(S.peak.missiles, S.cur.missiles);
  S.peak.statusOverlays = std::max(S.peak.statusOverlays, S.cur.statusOverlays);
  S.peak.ms = std::max(S.peak.ms, S.cur.ms);
  if (S.cur.effects || S.cur.particles || S.cur.surfaceTiles || S.cur.missiles || S.cur.statusOverlays) {
    S.msWorst = std::max(S.msWorst, S.cur.ms);
    S.msSum += S.cur.ms;
    S.msFrames++;
  }
  S.cur = MagicFxStats();
  const auto t0 = Clock::now();
  GamePt P{*pix_, monsters_};
  mfx::Ctx C;
  C.cam = cam; C.W = (float)Pix::W; C.H = (float)Pix::H; C.t = t_;
  nightOf(g, C.dark, C.lightK);
  S.cur.surfaceTiles = S.surf.draw(P, C, g.arcana.live.field);
  {   // (M9 fixer) the flames behind a tree or a stall go down here, under the scene that hides them
    const std::vector<uint8_t> hide = surfaceBehindProps(g);
    S.surf.drawOver(P, S.w, C, g.arcana.live.field, hide.data(), true);
  }
  mfx::drawGround(P, S.w, C);
  S.cur.decals = (int)S.w.decals.size();
  S.cur.ms += msSince(t0);
}

void View::drawSpellProj(Game& g, const Projectile& pr, int idx, Vec2 cam) {
  (void)g; (void)idx;
  View::MagicFxState& S = MagicFxOps::st(*this);
  const auto t0 = Clock::now();
  GamePt P{*pix_, monsters_};
  mfx::Ctx C;
  C.cam = cam; C.W = (float)Pix::W; C.H = (float)Pix::H; C.t = t_;
  mfx::drawMissileShadow(P, S.w, C, lookFor(pr.spell), pr.p, pr.mag);
  S.cur.missiles++;
  S.cur.ms += msSince(t0);
}

void View::magicLights(Game& g, const std::function<void(Vec2, float, Color, float)>& light) {
  View::MagicFxState& S = MagicFxOps::st(*this);
  const auto t0 = Clock::now();
  int n = 0;
  if (S.liftA > 0.01f) {   // lightning: the whole night lit for a frame
    pix_->rectAdd(0, 0, (float)lightMap_.w, (float)lightMap_.h, S.liftC.withA(S.liftA));
    S.liftA *= 0.35f;
    if (S.liftA < 0.05f) S.liftA = 0;
    n++;
  }
  for (const Projectile& pr : g.projs) {
    if (!isMagicMissile(pr)) continue;
    const mfx::Look L = lookFor(pr.spell);
    const float sc = mfx::baseScale(L) * std::max(0.6f, pr.mag);
    light(pr.p, 56.0f * sc, L.c(2), 0.95f);
    light(pr.p - norm(pr.v) * 14.0f, 34.0f * sc, L.c(1), 0.6f);   // the lit trail behind it
    n += 2;
  }
  n += mfx::lightsOf(S.w, [&](Vec2 p, float r, Color c, float k) { light(p, r, c, k); });
  n += S.surf.lights(g.arcana.live.field, t_, [&](Vec2 p, float r, Color c, float k) { light(p, r, c, k); });
  for (const Actor& a : g.actors) {
    if (!a.status || a.st == AState::Dead) continue;
    if (a.hasStatus(magic::Status::Burning)) { light(a.p + Vec2(0, -10), 44, Color(1.0f, 0.55f, 0.22f), 0.75f); n++; }
    if (a.hasStatus(magic::Status::Shocked)) { light(a.p + Vec2(0, -10), 30, Color(0.55f, 0.6f, 1.0f), 0.4f + 0.3f * std::sin(t_ * 31 + (float)a.id)); n++; }
  }
  S.cur.lights += n;
  S.cur.ms += msSince(t0);
}

void View::drawMagicFx(Game& g, Vec2 cam) {
  View::MagicFxState& S = MagicFxOps::st(*this);
  const auto t0 = Clock::now();
  GamePt P{*pix_, monsters_};
  mfx::Ctx C;
  C.cam = cam; C.W = (float)Pix::W; C.H = (float)Pix::H; C.t = t_;
  nightOf(g, C.dark, C.lightK);
  mfx::World& W = S.w;
  // the surfaces' flames, clouds, arcs and night glow
  {
    const std::vector<uint8_t> hide = surfaceBehindProps(g);
    S.surf.drawOver(P, W, C, g.arcana.live.field, hide.data(), false);
  }
  // the summoned forms climbing out of their rifts: the creature's sheet rising through the rift line, in the
  // school's light, then fading into the real summon (or into motes, a phase-A summon with no actor)
  for (const mfx::Fx& f : W.fx)
    if (f.kind == (uint8_t)magic::Phase::Summon && f.delay <= 0)
      mfx::drawSummonForm(P, W, C, f, (int)f.sub, art::monsterCellH((art::Monster)f.sub), (int)(t_ * 8) % 4);
  // the missiles' lit bodies (after the lighting: they burn through the night)
  for (const Projectile& pr : g.projs) {
    if (!isMagicMissile(pr)) continue;
    mfx::drawMissile(P, W, C, lookFor(pr.spell), pr.p, pr.v, pr.mag, pr.life, (uint32_t)(pr.from.x * 7 + pr.from.y * 13), len(pr.p - pr.from));
  }
  for (const mfx::Fx& f : W.fx) if (f.delay <= 0) mfx::drawPhaseFx(P, W, C, f);
  mfx::drawParts(P, W, C);
  // the status overlays on every body that carries one
  int overlays = 0;
  for (const Actor& a : g.actors) {
    if (!a.status || a.st == AState::Dead || a.indoors != g.pl().indoors) continue;
    const Vec2 s = mfx::scr(C, feetOf(a));
    if (!mfx::onScr(C, s, 40)) continue;
    float bw, bh;
    bodyOf(a, bw, bh);
    overlays += mfx::drawStatus(P, W, C, s, bw, bh, a.status, (uint32_t)a.id * 0x2545F491u);
  }
  // the scene flash (lightning, a meteor's landing): one frame full, the next a fading echo
  if (S.flashA > 0.01f) {
    P.add(0, 0, C.W, C.H, S.flashC.withA(S.flashA));
    S.flashA *= 0.3f;
    if (S.flashA < 0.04f) S.flashA = 0;
  }
  S.cur.effects = (int)W.fx.size();
  S.cur.particles = (int)W.parts.size();
  S.cur.statusOverlays = overlays;
  S.cur.ms += msSince(t0);
}
