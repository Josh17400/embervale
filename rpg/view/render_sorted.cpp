// render_sorted.cpp - the world's sorted pass (View::drawWorld's middle, SOLID 3.4a): everything that stands is
// collected as a Drawable (props, buildings, wall tiles, actors, pickups, projectiles, gates, festival poles, the home's
// draws, graves, the sea's, the relief's and the rise's pieces, a story's hand carts), sorted by its foot line, then
// drawn in two passes: the ground shadows, then the bodies, one member per kind. The hidden hero's silhouette closes it.
// Neighbours: render.cpp (drawWorld: the frame, the terrain, the wall layout, the festival and the particles around
// this pass; the shared paint helpers in render_internal.h), render_anim.cpp (the human body), party_view.cpp,
// home_view.cpp, render_sea.cpp, render_relief.cpp, rise_world.cpp (their own sorted pieces).
// Determinism contract (SOLID_PLAN section 8): the push order, the one std::sort call and its ties, and the order of
// the recorded blits (g_occRecs) are exactly drawWorld's as they were before the split.
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <unordered_map>
#include <vector>
#include "rpg/art/art_city.h"
#include "rpg/art/art_parts.h"
#include "rpg/art/doll/doll.h"
#include "rpg/art/doll/doll_envelope.h"
#include "rpg/art/art_party.h"
#include "rpg/sim/anim.h"
#include "rpg/sim/deco.h"
#include "rpg/sim/foes.h"
#include "rpg/sim/gear_look.h"
#include "rpg/sim/life.h"
#include "rpg/sim/monster_stats.h"   // heavyWindup
#include "rpg/story/story_api.h"
#include "rpg/view/prop_traits.h"
#include "rpg/view/magic_state.h"
#include "rpg/view/realm_ui.h"
#include "rpg/view/rise_view.h"
#include "rpg/view/view.h"
#include "rpg/view/render_internal.h"
#include "rpg/world/biomes.h"
#include "rpg/world/economy.h"
#include "rpg/world/ids.h"
#include "rpg/world/source.h"
#include "rpg/world/urban.h"

using art::Prop;
using art::Monster;
using namespace rview;

// what a Drawable is (its values are the old literals 0..13: append only)
enum class DrawKind : int {
  Prop = 0,         // idx: the Prop; tx, ty its tile
  Building = 1,     // idx: Map::bldgs index
  Wall = 2,         // idx: the wall key; tx, ty its tile
  Actor = 3,        // idx: Game::actors index
  Pickup = 4,       // idx: Game::pickups index
  Projectile = 5,   // idx: Game::projs index
  Gate = 6,         // tx, ty: the gate's tile
  FestPole = 7,     // (M5) idx: the festival span, tx: 0 west / 1 east pole
  Home = 8,         // (M7) idx: View::homeDraws_ index (a yard object, a crop, the building site, a ground edit)
  Grave = 9,        // (M8) idx: Party::fallen index
  Sea = 10,         // (M10) View::seaSorted's idx: a boat
  Relief = 11,      // (M11) View::reliefSorted's idx
  Rise = 12,        // (M12) View::riseSorted's idx: a work, a warlord's standard, resettlers' stores
  Handcart = 13,    // (P1b) idx: story::Engine::walkers_ index
};
struct View::Drawable {
  float y;
  DrawKind kind;
  int idx;
  int tx, ty;
};

// (P2b VIEW-B fix r3, RIG2-ROOF-OVER-YARD) rig 2's taller walls carry roofs up over the ground behind them: a building in
// view, its sprite's top-left, its bottom and its column profile (g_bldgColTop)
struct RoofOcc { float x0, y0, bottom; const std::vector<int16_t>* cols; };
// (M6 fixer r4) a ranked beast on screen: its feet (x, y) and half-width / body height
struct Reveal { float x, y, hw, h; };

// one frame of the sorted pass: what every kind's member reads, and the hidden hero they find
struct View::FrameCtx {
  Pix& P;
  const Map& m;
  Vec2 cam;
  const Actor& pl;
  std::vector<RoofOcc>* roofOcc;   // the buildings in view (rig 2's behind-a-roof test)
  bool rig2Roofs;
  bool recOcc = false;             // (P2a LENS fix r3) the sorted pass's plain blits are recorded (g_occRecs)
  std::vector<Reveal> reveals;
  // the hero is hidden behind a building or a tree crown: show a silhouette over it
  bool ghost = false;
  const Tex* ghostTex = nullptr;
  int ghostFr = 0, ghostRow = 0;
  bool ghostFlip = false;
  float ghostX = 0, ghostY = 0;
  int ghostSx = 0, ghostSy = 0, ghostW = 0, ghostH = 0;   // (P2a) a v2 frame's rect (w 0: the v1 cell)
  // (P2a LENS fix r1, RIG2-HIDDEN-HERO) a v2 hero on foot: the atlas frame's blit as recorded (its texture, source rect
  // and place), so the silhouette is the frame he was drawn with
  Pix::BlitRec ghostRec;
};

// (P2b VIEW-B fix r3, RIG2-ROOF-OVER-YARD) how much of a figure standing at a.p shows past the buildings in view: 1 in
// the open (or his feet only just behind a roof's edge), 0 once his feet are 12 px or more under the top of the sprite
// columns he stands in and inside their opaque run (that building sorts after him); smoothsteps between
static float behindRoofAlpha(const std::vector<RoofOcc>& roofOcc, const Actor& a) {
  float k = 1.0f;
  for (const RoofOcc& o : roofOcc) {
    if (a.p.y >= o.bottom - 4.0f || a.p.y < o.y0) continue;   // in front of (or level with) its walls: drawn after it
    // the columns under the body's middle (9 px): his feet hidden in a column (under its top, inside its opaque run)
    // count by how deep; walking out past a gable's end or from under an eave he comes back column by column
    const std::vector<int16_t>& cv = *o.cols;
    const int w = (int)cv.size() / 2;
    const int c0 = (int)std::floor(a.p.x - o.x0);
    if (c0 + 4 < 0 || c0 - 4 >= w) continue;
    const float y0 = o.y0;
    auto smooth = [](float v) { v = std::clamp(v, 0.0f, 1.0f); return v * v * (3.0f - 2.0f * v); };
    float hid = 0;
    for (int c = c0 - 4; c <= c0 + 4; c++) {
      if (c < 0 || c >= w || cv[(size_t)c * 2] < 0) continue;
      const float top = y0 + cv[(size_t)c * 2], end = y0 + cv[(size_t)c * 2 + 1];
      hid += smooth((a.p.y - top - 2.0f) / 10.0f) * smooth((end + 2.0f - a.p.y) / 6.0f);
    }
    k = std::min(k, 1.0f - hid / 9.0f);
  }
  return k;
}

void View::collectTiles(Game& g, FrameCtx& F, std::vector<Drawable>& list) {
  Pix& P = F.P;
  const Map& m = F.m;
  Vec2 cam = F.cam;
  // (M2: the wonders are up to 7 tiles wide and 7 tall above their anchor tile: a wider margin below and aside)
  int tx0 = (int)std::floor(cam.x / 16) - 5, ty0 = (int)std::floor(cam.y / 16) - 2;
  int tx1 = tx0 + Pix::W / 16 + 10, ty1 = ty0 + Pix::H / 16 + 10;
  drawDeco(m, cam, std::max(0, tx0), std::max(0, ty0), std::min(m.w, tx1), std::min(m.h, ty1));
  for (int ty = std::max(0, ty0); ty < std::min(m.h, ty1); ty++)
    for (int tx = std::max(0, tx0); tx < std::min(m.w, tx1); tx++) {
      int pr = m.prop[(size_t)ty * m.w + tx];
      // (M4) a walk-through cover (flowers, grass) under a diagonal boardwalk's deck is not drawn over the planks
      if (pr && m.kind == MapKind::Overworld && !propSolid((Prop)(pr - 1)) && boardwalkCoversTile(m, tx, ty)) pr = 0;
      // (M9 fixer) the grass, flowers and brush inside a fresh spell crater burned away (trees, stones and solid bushes stand)
      if (pr && !propSolid((Prop)(pr - 1))) {
        const Prop q = (Prop)(pr - 1);
        const bool burns = (q >= Prop::Flowers1 && q <= Prop::Fern && q != Prop::Cactus && q != Prop::LilyPad) || (q >= Prop::Heather && q <= Prop::Petals);
        if (burns && mfxScorched(*this, tx * 16.0f + 8.0f, ty * 16.0f + 10.0f)) pr = 0;
      }
      if (pr) {
        Prop p = (Prop)(pr - 1);
        if (flatProp(p)) {
          if (uint32_t ik = interiorPropTexKey(g, m, tx, ty, p, (int)(t_ * 8 + tx * 3))) {
            const Tex& t = sprites_.cachedTex(*this, 0x01ull << 56 | ik, paintInteriorPiece);
            P.blit(t, tx * 16 + 8 - t.w / 2 - cam.x, ty * 16 + 16 - t.h - cam.y);
          } else {
            const Tex* tp = &sprites_.props_[(int)p];
            int fw = art::propW(p);
            int frames = std::max(1, art::propFrames(p));
            int fr = frames > 1 ? (int)(t_ * 8 + tx * 3) % frames : 0;
            // (M3c fixer) the ground covers of the wild sat at one offset in every tile, so a meadow read as planted rows:
            // each patch takes a sub-tile offset by its global tile (up to +-5 px across, +-4 down) and the Wildlands
            // covers their biome's variant
            float jx = 0, jy = 0;
            if (m.kind == MapKind::Overworld && natureProp(p)) {
              const uint32_t h = hash2(tx + g.world.ox, ty + g.world.oy, 6287);
              jx = (float)((int)(h % 11u) - 5);
              jy = (float)((int)((h >> 8) % 9u) - 4);
              if (art::isWildlandsFlora(p)) {
                const int nv = std::max(1, art::floraVariants(p));
                tp = &sprites_.cachedTex(*this, 0x5Eull << 56 | (uint64_t)p << 16 | (uint64_t)m.ecoAt(tx, ty) << 8 | (uint64_t)((h >> 16) % (uint32_t)nv), paintFlora);
                fw = tp->w / frames;
              }
            }
            P.blitRegion(*tp, fr * fw, 0, fw, tp->h, tx * 16 + 8 - fw / 2 + jx - cam.x, ty * 16 + 16 - tp->h + jy - cam.y);
          }
        } else {
          // (stalls fixer round 3) a stall sorts just ahead of its keeper (art::stallSortY), whichever way it faces
          // (M7b CITYART) the souk's canopy hangs over whoever stands on its tile: it sorts after them (and before the
          // people south of it, who stand in front of its hem)
          const float sy = art::isStall(p) && m.kind == MapKind::Overworld ? art::stallSortY(stallFacingOn(g, m, tx, ty))
                                                                            : (p == Prop::Canopy ? 15.99f : 15.0f);
          list.push_back({ty * 16.0f + sy, DrawKind::Prop, pr - 1, tx, ty});
          // (M7b CITYART) a street tree on a city walk or square stands in a tree pit: a kerbed square of soil under
          // an iron grate, laid flat on the paving before anything stands on it
          if (treeProp(p) && m.kind == MapKind::Overworld && g.mode != Mode::Title) {
            const ew::Urb u = ew::urbClass(m.urbAt(tx, ty));
            if (u == ew::Urb::Sidewalk || u == ew::Urb::Avenue || u == ew::Urb::Street || u == ew::Urb::Square || u == ew::Urb::Court) {
              const art::PropStyle* ps = sprites_.propStyleAt(*this, g, tx, ty);
              const uint64_t k = ew::mix64(ps->key() ^ 0x7EE917ull);
              auto it = sprites_.styledProps_.find(k);
              const Tex& gt = it != sprites_.styledProps_.end() ? it->second : (sprites_.styledProps_[k] = pix_->bake(art::cityTreePit(*ps)));
              P.blit(gt, tx * 16.0f + 8.0f - gt.w / 2.0f - cam.x, ty * 16.0f + 16.0f - gt.h + 1.0f - cam.y);
            }
          }
        }
      }
      uint32_t wk = wallKeys_.empty() ? 0u : wallKeys_[(size_t)ty * m.w + tx];
      if (wk) list.push_back({ty * 16.0f + ((wk & art::WALL_BIT_TOWER) ? 15.3f : 15.0f), DrawKind::Wall, (int)wk, tx, ty});
    }
}

void View::collectBuildings(Game& g, FrameCtx& F, std::vector<Drawable>& list) {
  const Map& m = F.m;
  Vec2 cam = F.cam;
  const Actor& pl = F.pl;
  std::vector<RoofOcc>& roofOcc = *F.roofOcc;
  const bool rig2Roofs = F.rig2Roofs;
  // (P2b VIEW-B fix r3, RIG2-ROOF-OVER-YARD) rig 2's taller walls carry roofs up over the ground behind them, so a villager
  // working a step or two behind a house showed his head over its ridge (a hat read as a blob under the next worker's
  // boots): the buildings in view, for the sorted pass's behind-a-roof test (rig 2 only; rig 1 draws as before)
  roofOcc.clear();
  {
    static std::vector<int> vis;
    bldgsIn(g, m, cam.x, cam.y, cam.x + Pix::W, cam.y + Pix::H, vis);
    for (int bi : vis)   // (M7) a player's house under construction is drawn as its building site (homeCollect)
      if (m.bldgs[bi].home != 3) {
        list.push_back({(m.bldgs[bi].r.y + m.bldgs[bi].r.h) * 16.0f - 1.0f, DrawKind::Building, bi, 0, 0});
        if (rig2Roofs) {   // its sprite's place and column profile (painted now if not yet, as its draw below would)
          const Bldg& b = m.bldgs[(size_t)bi];
          const uint64_t key = sprites_.bldgKey(m, b, bi);
          auto tt = sprites_.bldgTex_.find(key);
          if (tt == sprites_.bldgTex_.end() && (witness || !(g.sleepFade > 0.5f || fade_ > 0.5f))) { sprites_.bldgTex(*this, b, bi); tt = sprites_.bldgTex_.find(key); }
          auto ct = g_bldgColTop.find(key);
          if (tt != sprites_.bldgTex_.end() && tt->second.t && ct != g_bldgColTop.end()) {
            const float bottom = (b.r.y + b.r.h) * 16.0f;
            roofOcc.push_back({b.r.x * 16.0f - art::BLDG_PAD_X, bottom + art::BLDG_PAD_B - tt->second.h, bottom, &ct->second});
          }
        }
      }
  }
  // paint the sprites of buildings near the player ahead of time, at most one per frame, so walking into a town
  // never stalls on a burst of building paints
  // (M3, owner carry-over 1) in steps of about 3 ms: a palace is painted over a dozen frames instead of stalling one
  const double drawnMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - g_drawT0).count();
  // (a heavy frame skips it, the next one paints; never more than two frames in a row, then a small step anyway, so the
  // streets ahead are still painted before they come into view)
  static int paintSkipped = 0;
  double paintBudget = std::min(3.0, 10.0 - drawnMs);
  if (paintBudget < 0.75 && ++paintSkipped > 2) paintBudget = 1.0;
  if (paintBudget >= 0.75) paintSkipped = 0;
  if (!m.bldgs.empty() && g.mode != Mode::Title && !g.travelling() && paintBudget >= 0.75) {   // (M2: a journey paints its arrival in travelArrive)
    bldgPrefetch_ %= (int)m.bldgs.size();
    const Bldg& cur = m.bldgs[(size_t)bldgPrefetch_];
    const uint64_t ck = sprites_.bldgKey(m, cur, bldgPrefetch_);
    bool busy = false;
    if (!sprites_.bldgTex_.count(ck))
      for (const SpriteCache::BldgJob& j : sprites_.bldgJobs_) if (j.key == ck) { busy = true; break; }
    if (busy) sprites_.bldgPaintStep(*this, cur, bldgPrefetch_, paintBudget);
    else {
      // (M3 fixer round 2) the nearest unpainted building to where the hero is heading, not the next index round the
      // list: in a capital of 200+ buildings the round-robin looked at 12 a frame and the walk reached unpainted streets
      // first (a 120-140 ms frame of on-demand paints on the first walk after arriving)
      const Vec2 ahead = pl.p + pl.vel * 0.6f;
      int best = -1;
      float bestD = 1e30f;
      for (int i = 0; i < (int)m.bldgs.size(); i++) {
        const Bldg& b = m.bldgs[(size_t)i];
        const float dx = b.r.cx() * 16.0f - ahead.x, dy = b.r.cy() * 16.0f - ahead.y;
        if (std::fabs(dx) > 720 || std::fabs(dy) > 520) continue;
        const float d = dx * dx + dy * dy * 1.6f;
        if (d >= bestD || sprites_.bldgTex_.count(sprites_.bldgKey(m, b, i))) continue;
        best = i; bestD = d;
      }
      if (best >= 0) { bldgPrefetch_ = best; sprites_.bldgPaintStep(*this, m.bldgs[(size_t)best], best, paintBudget); }
    }
  }
}

void View::collectActors(Game& g, FrameCtx& F, std::vector<Drawable>& list) {
  const Map& m = F.m;
  Vec2 cam = F.cam;
  for (int i = 0; i < (int)g.actors.size(); i++) {
    const Actor& a = g.actors[i];
    if (a.p.x < cam.x - 80 || a.p.x > cam.x + Pix::W + 80 || a.p.y < cam.y - 40 || a.p.y > cam.y + Pix::H + 80) continue;
    float sy = a.p.y + (a.fly ? 60.0f : 0.0f) - (a.st == AState::Dead ? 8.0f : 0.0f);
    // (P2c SWITCH) rig 2: a fallen body on the doll sorts by its own baseline (the lying frame's lowest row, the fall's
    // frame while it topples), so tall grass and a loose horse standing in front of it draw over it and those behind under
    // it (v1's fixed 8 px lift was the v1 corpse cell's). From the cached DEATH atlas only: nothing is baked for a sort.
    if (a.human && a.st == AState::Dead && art::rigVersion() >= 2 && art::rig2Covers(a.look)) {
      auto dt = sprites_.rigTex_.find(rigKey(a.look, art::RigPart::Death, a.role == Role::Child));
      if (dt != sprites_.rigTex_.end() && !dt->second.sheet.empty()) {
        const AnimPick k = animPick(g, a, 2);
        const art::RigSheet& S = dt->second.sheet;
        const art::FrameRect* f = S.frame(std::clamp(a.face, 0, 3), k.clip, k.variant, k.frame);
        if (!f) f = S.frame(0, k.clip, k.variant, k.frame);
        if (f) sy = a.p.y + (float)(f->h - f->py) - 1.0f;
      }
    }
    // (M6 fixer r3, review: "flying harpies draw under tree canopies and bushes") a hovering flyer is over the
    // undergrowth and the near crowns: it sorts as if it stood almost two tiles further down
    if (a.flying && !a.fly && a.st != AState::Dead) sy += 28.0f;
    // (M5) a body on furniture sorts by the furniture's tile (art::PostureInfo): a sleeper after its bed (the blanket over
    // it); a sitter facing down after its chair (in front of the chair back), facing up before it (the chair back hides
    // the small of the back; the table it faces, a row up, is drawn before it either way); side-on after it
    if (a.human && a.posture != art::Posture::None && a.useX >= 0 && a.useY >= 0 && a.st != AState::Dead && m.in(a.useX, a.useY)) {
      const art::PostureInfo pi = art::postureInfo(a.posture);
      if (pi.lying) sy = a.useY * 16.0f + 15.7f;
      else if (pi.seated) {   // (ART seatFit: front = after the seat)
        const int sp = m.propAt(a.useX, a.useY);
        const art::SeatFit f = art::seatFit(sp ? (art::Prop)(sp - 1) : art::Prop::COUNT, -1, a.face);
        sy = a.useY * 16.0f + (!f.ok || f.front ? 15.6f : 14.4f);
      }
    }
    // (P2c envelope, contracts 3d: the doorway draw order) a figure whose feet stand in an interior door's tile (DoorH: the
    // frame, its jambs and the lintel over the cap row) sorts before that door for the whole tile, so the jambs draw over a
    // top tier's pauldrons and cloak (a T6 is 28 px across, the opening 16) until the feet leave the tile (rig 2 only)
    if (art::rigVersion() >= 2 && m.kind == MapKind::Interior && a.human && a.st != AState::Dead && a.posture == art::Posture::None) {
      const int ftx = (int)std::floor(a.p.x / 16.0f), fty = (int)std::floor(a.p.y / 16.0f);
      if (m.in(ftx, fty)) {
        const int dp = m.propAt(ftx, fty);
        if (dp && (Prop)(dp - 1) == Prop::DoorH) sy = std::min(sy, fty * 16.0f + 14.9f);
      }
    }
    list.push_back({sy, DrawKind::Actor, i, 0, 0});
  }
}

void View::collectOthers(Game& g, FrameCtx& F, std::vector<Drawable>& list) {
  const Map& m = F.m;
  Vec2 cam = F.cam;
  // (P1b ENGINE lane) a wagon escort's hand cart (kind 13: idx = story::Engine::walkers_ index), sorted by its own
  // centre (side-on: a breath before its driver, who stands between the shafts)
  if (g.mode != Mode::Title)
    for (int i = 0; i < (int)g.story.walkers_.size(); i++) {
      const story::Engine::Walker& w = g.story.walkers_[(size_t)i];
      if (w.skin != 1 || !w.cartSet || w.actor < 0) continue;
      if (w.cartX < cam.x - 60 || w.cartX > cam.x + Pix::W + 60 || w.cartY < cam.y - 40 || w.cartY > cam.y + Pix::H + 60) continue;
      const int dir = handcartDir(w.cartAng);
      list.push_back({w.cartY - (dir < 2 ? 0.5f : 0.0f), DrawKind::Handcart, i, 0, 0});
    }
  // (M8) the graves of the fallen companions in view (kind 9: idx = Party::fallen index; party_view.cpp)
  {
    std::vector<int> graves;
    party_.graveDraws(g, m, cam, graves);
    for (int fi : graves)
      list.push_back({(float)(g.party.fallen[(size_t)fi].gy - g.world.oy) * 16.0f + 15.5f, DrawKind::Grave, fi, 0, 0});
  }
  for (int i = 0; i < (int)g.pickups.size(); i++) list.push_back({g.pickups[i].p.y - 4, DrawKind::Pickup, i, 0, 0});
  // projectiles fly at chest height: sort them by the ground point beneath (arrows, spells, spit, dragon fire)
  for (int i = 0; i < (int)g.projs.size(); i++) {
    const Projectile& pr = g.projs[i];
    if (pr.p.x < cam.x - 24 || pr.p.x > cam.x + Pix::W + 24 || pr.p.y < cam.y - 24 || pr.p.y > cam.y + Pix::H + 40) continue;
    list.push_back({pr.p.y + 8.0f, DrawKind::Projectile, i, 0, 0});
  }
  if (m.kind == MapKind::Overworld)
    for (auto& gt : g.world.gates) {
      if ((gt.first + 5) * 16 < cam.x || (gt.first - 3) * 16 > cam.x + Pix::W || gt.second * 16 + 24 < cam.y || gt.second * 16 - 60 > cam.y + Pix::H) continue;
      list.push_back({gt.second * 16.0f + 15.6f, DrawKind::Gate, 0, gt.first, gt.second});
    }
  // (M7) the player's property: yard objects, crops, ground edits, the building site (kind 8: idx into homeDraws_;
  //      collected by the VIEW lane's homeCollect, rpg/view/home_view.cpp)
  home_.homeDraws_.clear();
  if (g.mode != Mode::Title) homeCollect(g, m, cam);
  else home_.homeLights_.clear();
  home_.drawHomeFlats(*this, cam);   // the farmland and the paths lie under every shadow (the shadow pass comes next)
  for (int i = 0; i < (int)home_.homeDraws_.size(); i++)
    if (!home_.homeDraws_[(size_t)i].flat) list.push_back({home_.homeDraws_[(size_t)i].sortY, DrawKind::Home, i, 0, 0});
  // (M5) a festival's poles across the plaza (kind 7: idx = span, tx = 0 west / 1 east pole)
  life_.festivalPlan(g, m, cam);
  for (int i = 0; i < (int)life_.festSpans_.size(); i++) {
    const LifeView::FestSpan& s = life_.festSpans_[(size_t)i];
    if (s.y * 16 < cam.y - 40 || s.y * 16 > cam.y + Pix::H + 60 || s.x1 * 16 < cam.x - 40 || s.x0 * 16 > cam.x + Pix::W + 40) continue;
    list.push_back({s.y * 16.0f + 13.0f, DrawKind::FestPole, i, 0, 0});
    list.push_back({s.y * 16.0f + 13.0f, DrawKind::FestPole, i, 1, 0});
  }
  // (M10) the sea's standing things: the boats (render_sea.cpp; Drawable kind 10)
  if (g.mode != Mode::Title) SeaLane::seaSorted(*this, g, cam, [&](float y, int i) { list.push_back({y, DrawKind::Sea, i, 0, 0}); });
  // (M11) the relief's standing things: a fall's spray column (render_relief.cpp; Drawable kind 11)
  if (g.mode != Mode::Title) ReliefLane::reliefSorted(*this, g, cam, [&](float y, int i) { list.push_back({y, DrawKind::Relief, i, 0, 0}); });
  // (M12) the rise's standing pieces: the holdings' works, warlords' standards and stockades, resettlers' stores
  //       (render_rise.cpp's rise_world.cpp; Drawable kind 12), and its flat ones (a beast's claw scars) under them
  if (g.mode != Mode::Title) {
    RiseLane::riseSorted(*this, g, cam, [&](float y, int i) { list.push_back({y, DrawKind::Rise, i, 0, 0}); });
    RiseLane::drawRiseGround(*this, g, cam);
  }
  // (M6 fixer r3, review: "a diagonal curtain wall's face runs over the gate tower's base") a diagonal run leaving a
  // gatehouse starts a row or two below the gate's own row, so it sorted after the gatehouse and its face crossed the
  // round flanking tower: the first diagonal wall tiles beside a gate go down before the gatehouse (the tower stands in
  // front of the wall it closes)
  if (m.kind == MapKind::Overworld && !g.world.gates.empty())
    for (Drawable& d : list) {
      if (d.kind != DrawKind::Wall) continue;
      const uint32_t wk = (uint32_t)d.idx;
      const bool diag = (wk & (art::WALL_BIT_NE | art::WALL_BIT_SE | art::WALL_BIT_SW | art::WALL_BIT_NW)) != 0 &&
                        !((wk & art::WALL_BIT_E) && (wk & art::WALL_BIT_W));
      if (!diag) continue;
      for (const auto& gt : g.world.gates)
        if (d.tx >= gt.first - 2 && d.tx <= gt.first + 4 && d.ty >= gt.second + 1 && d.ty <= gt.second + 2) {
          d.y = std::min(d.y, gt.second * 16.0f + 15.5f);
          break;
        }
    }
}

void View::drawShadowSorted(Game& g, FrameCtx& F, const Drawable& d) {
  Pix& P = F.P;
  Vec2 cam = F.cam;
  if (d.kind == DrawKind::Home) {   // (M7) a yard object's or a crop's ground shadow
    if (d.idx >= 0 && d.idx < (int)home_.homeDraws_.size()) home_.drawHomeShadow(*this, home_.homeDraws_[(size_t)d.idx], cam);
    return;
  }
  if (d.kind == DrawKind::Grave) { party_.drawGrave(*this, g, d.idx, cam, true); return; }   // (M8) a grave's shadow
  if (d.kind == DrawKind::Handcart) {   // (P1b) a story's hand cart: the shadow its own model casts, to the lower right
    if (d.idx >= 0 && d.idx < (int)g.story.walkers_.size()) {
      const story::Engine::Walker& w = g.story.walkers_[(size_t)d.idx];
      const int dir = handcartDir(w.cartAng);
      const Tex& st = sprites_.cachedTex(*this, 0x52ull << 56 | (uint64_t)dir | 1ull << 6, paintHandcart);
      P.blitEx(st, 0, 0, st.w, st.h, std::floor(w.cartX - HC_OX - cam.x + 0.5f), std::floor(w.cartY - HC_OY - cam.y + 0.5f), (float)st.w, (float)st.h, false,
               Color(1, 1, 1, 0.32f));
    }
    return;
  }
  if (d.kind == DrawKind::Actor) drawActorShadow(g, F, d);
  else if (d.kind == DrawKind::Prop) drawPropShadow(g, F, d);
  else if (d.kind == DrawKind::Gate) drawGateShadow(g, F, d);
}

void View::drawActorShadow(Game& g, FrameCtx& F, const Drawable& d) {
  Pix& P = F.P;
  const Map& m = F.m;
  Vec2 cam = F.cam;
  const Actor& a = g.actors[d.idx];
  if (a.st == AState::Dead) {
    // (P2b VIEW-B) rig 2: the contact shadow under a corpse drawn on the doll: as long as the lying figure (its own
    // frame's width), its middle under the body's back row, grown from the standing shadow over the fall (eased on
    // the clip's time, so it spreads as the body goes down instead of stepping per frame) and fading with the corpse;
    // under it the body pass lays STYLE_P2 13's stain. v1's dead keep none (rig 1 unchanged).
    if (a.human && !a.comp && art::rigVersion() >= 2 && art::rig2Covers(a.look)) {
      const AnimPick k = animPick(g, a, 2);
      const RigTex rt = k.clip == art::Clip::Fall || k.clip == art::Clip::Lying ? sprites_.rigTex(*this, a.look, art::RigPart::Death, a.role == Role::Child) : RigTex();
      if (rt.ready()) {
        int fc = std::clamp(a.face, 0, 3);
        const art::FrameRect* lie = rt.sheet->frame(fc, art::Clip::Lying, k.clip == art::Clip::Lying ? k.variant : 0, 0);
        if (!lie) { fc = 0; lie = rt.sheet->frame(0, art::Clip::Lying, k.clip == art::Clip::Lying ? k.variant : 0, 0); }
        if (lie) {
          float u = k.clip == art::Clip::Lying ? 1.0f : clampf(k.t / 0.34f, 0, 1);
          u = u * u * (3.0f - 2.0f * u);
          const float lw = (float)lie->w + 2.0f, lcx = a.p.x - (float)lie->px + lie->w * 0.5f;
          // (3/4 view: the lying body's shadow peeks 2-3 rows out under its back, as a standing body's under its soles)
          const float w = std::round(14.0f + (lw - 14.0f) * u), h = std::round(6.0f + u);
          const float cx = a.p.x + (lcx - a.p.x) * u, cy = a.p.y + u;
          const float ca = animCorpseAlpha(a, 2);
          if (ca > 0.01f)
            P.blitEx(sprites_.shadowBig_, 0, 0, sprites_.shadowBig_.w, sprites_.shadowBig_.h, std::floor(cx - w * 0.5f - cam.x + 0.5f), std::floor(cy - h * 0.5f - cam.y + 0.5f), w, h,
                     false, Color(1, 1, 1, 0.9f * ca));
        }
      }
    }
    return;
  }
#ifdef EMB_WITNESS
  // (Witness reset param hide) no contact shadow or ring. The actor stays in the sorted list (the passes skip it), so
  // the order of every other drawable, ties included, is the same as without the param: only its own pixels go
  if (!wHide.empty() && witnessHides(g, a)) return;
#endif
  if (a.player && g.homeRiding()) { home_.drawMountShadow(*this, g, a, cam); return; }   // (M7) the horse's shadow
  if (a.comp) {   // (M8) the hound's, a rider's horse's, the blood under the downed (party_view.cpp)
    bool done = false;
    party_.drawCompanionShadow(*this, g, a, cam, done);
    if (done) return;
  }
  // (M6 BEASTS) an elite's, a champion's or a named unique's aura: its ground glow round the feet, added under the
  // body (the shadow then darkens its middle where the feet stand); a few fixed widths keep the cache small
  if (const uint32_t ac = beasts::auraOf(a)) {
    const auto t0 = beasts::Clock::now();
    const int sc = beasts::drawScale(a);
    const int aw = std::clamp((int)std::lround((a.human ? 18.0f : art::lookCellW(beasts::lookOf(a)) * 0.7f) * sc / 6.0f) * 6, 18, 72);
    const int ah = art::auraH(aw);
    const Tex& at = sprites_.auraTex(*this, ac, aw);
    const int fr = (int)(t_ * 7.0f + a.id * 0.37f) % art::AURA_FRAMES;
    const float ax = std::floor(a.p.x - aw * 0.5f - cam.x + 0.5f), ay = std::floor(a.p.y - (ah - aw * 0.25f - 1.0f) - cam.y + 0.5f);
    // a faint normal pass lays the affix's hue on the ground (an additive red on grass alone reads yellow), the
    // additive pass makes it light
    P.blitEx(at, fr * aw, 0, aw, ah, ax, ay, (float)aw, (float)ah, false, Color(1, 1, 1, 0.45f));
    P.blitEx(at, fr * aw, 0, aw, ah, ax, ay, (float)aw, (float)ah, false, Color(1, 1, 1, 0.7f), 1);
    beasts::cur.auras++;
    beasts::cur.ms += beasts::msSince(t0);
  }
  if (a.critter > 0) {   // (M5) a village animal: a shadow as long as its body (wider side-on)
    const int ck = a.critter - 1;
    const float w = art::critterCellW((art::Critter)ck) * (a.face >= 2 ? 0.8f : 0.55f), h = std::max(3.0f, w * 0.32f);
    P.blitEx(sprites_.shadow_, 0, 0, sprites_.shadow_.w, sprites_.shadow_.h, a.p.x - w / 2 - cam.x, a.p.y - h / 2 - cam.y, w, h, false, Color(1, 1, 1, 0.85f));
    return;
  }
  if (bathing(g, m, a)) return;   // (M5 fixer r2) in the water: no shadow on the floor
  if (a.human && a.posture != art::Posture::None) {   // (M5) on a chair or in bed the furniture throws the shadow
    const art::PostureInfo pi = art::postureInfo(a.posture);
    if (pi.lying || (pi.seated && a.useY >= 0)) return;
  }
  bool big = a.radius > 7;
  const Tex& s = big ? sprites_.shadowBig_ : sprites_.shadow_;
  float sc = big ? std::min(1.6f, a.radius / 10.0f) : 1.0f;
  if (a.mon == Monster::Dragon) sc = a.fly ? 1.8f : 2.0f;
  if (!a.human) {   // (M6 BEASTS) a world boss throws a heavy shadow; the golem's broad feet a wider one
    if (beasts::isRank(a, foes::Rank::WorldBoss) && a.mon != Monster::Dragon) sc = std::max(sc, 1.6f);
    else if (a.mon == Monster::Golem) sc = std::max(sc, 1.25f);
  }
  // (M6 BEASTS) a flyer high off the ground (a harpy on the wing) throws a smaller, fainter shadow
  const bool airborne = !a.human && a.flying && a.mon == Monster::Harpy;
  if (airborne) sc *= 0.8f;
  P.blitEx(s, 0, 0, s.w, s.h, a.p.x - s.w * sc / 2 - cam.x, a.p.y - s.h * sc / 2 - cam.y, s.w * sc, s.h * sc, false, Color(1, 1, 1, a.fly ? 0.6f : airborne ? 0.75f : 1));
  if (a.player) lens_.drawRing(*this, g, a, cam);   // (P2a) the hero's ring: after the contact shadow, before the body
}

void View::drawPropShadow(Game& g, FrameCtx& F, const Drawable& d) {
  Pix& P = F.P;
  const Map& m = F.m;
  Vec2 cam = F.cam;
  Prop p = (Prop)d.idx;
  // (stall facings) a stall, table, cloth or cart throws its own model's shadow (soft, to the lower right)
  if (m.kind == MapKind::Overworld && (art::isVendorProp(p) || p == Prop::Cart)) {
    const Tex* st;
    if (art::isStall(p)) {
      const int fc = stallFacingOn(g, m, d.tx, d.ty);
      st = &sprites_.cachedTex(*this, 0x3Aull << 56 | (uint64_t)fc << 4 | (uint64_t)stallFormOn(g, d.tx, d.ty, fc), paintStallShadow);
    } else if (p == Prop::MarketTable) {   // (stalls fixer round 1) a packed-up table's shade is furled: no shade's shadow
      const bool open = g.mode == Mode::Title || ew::stallOpen(d.tx + g.world.ox, d.ty + g.world.oy, g.hour);
      st = &sprites_.cachedTex(*this, 0x3Bull << 56 | (uint64_t)(open ? ew::tableShadeAt(d.tx + g.world.ox, d.ty + g.world.oy) : 0), paintTableShadow);
    }
    else if (p == Prop::GroundCloth) st = &sprites_.cachedTex(*this, 0x3Cull << 56, paintClothShadow);
    else st = &sprites_.cachedTex(*this, 0x3Eull << 56 | (uint64_t)cartLook(g, d.tx, d.ty), paintCartShadow);
    P.blit(*st, d.tx * 16 + art::kVendorShadowX - cam.x, d.ty * 16 + art::kVendorShadowY - cam.y);
    return;
  }
  int sh = propShadow(p);
  if (sh == 1) P.blitEx(sprites_.shadowBig_, 0, 0, 40, 12, d.tx * 16 + 8 - 12 - cam.x, d.ty * 16 + 11 - cam.y, 24, 8, false, Color(1, 1, 1, 0.8f));
  else if (sh == 2) P.blitEx(sprites_.shadowBig_, 0, 0, 40, 12, d.tx * 16 + 8 - 11 - cam.x, d.ty * 16 + 12 - cam.y, 22, 7, false, Color(1, 1, 1, 0.7f));
  else if (sh == 3) {   // (M1 economy) the stall's awning shades the ground a little east of it (the sun is up-left)
    P.blitEx(sprites_.shadowBig_, 0, 0, 40, 12, d.tx * 16 + 8 - 22 - cam.x, d.ty * 16 + 9 - cam.y, 50, 11, false, Color(1, 1, 1, 0.75f));
  } else if (sh == 4) {   // (M1 fixer round 2) a two-tile table or cloth: its shade under both its tiles
    P.blitEx(sprites_.shadowBig_, 0, 0, 40, 12, d.tx * 16 + 1 - cam.x, d.ty * 16 + 10 - cam.y, 32, 8, false, Color(1, 1, 1, 0.6f));
  } else if (sh == 5) {   // a beast's own small shadow
    P.blitEx(sprites_.shadowBig_, 0, 0, 40, 12, d.tx * 16 + 8 - 9 - cam.x, d.ty * 16 + 12 - cam.y, 18, 5, false, Color(1, 1, 1, 0.6f));
  } else if (sh >= 6) {   // (M2) a wild prop's ground shadow, as wide as its footprint, thrown a little down-right
    int fw = 1, fh = 1;
    art::wildFootprint(p, fw, fh);
    const float w = fw * 16.0f * (sh == 7 ? 1.05f : 0.82f) + 6, h = std::max(6.0f, w * 0.22f);
    P.blitEx(sprites_.shadowBig_, 0, 0, 40, 12, d.tx * 16 + 8 - w / 2 + 3 - cam.x, d.ty * 16 + 15 - h * 0.6f - cam.y, w, h, false, Color(1, 1, 1, 0.75f));
  }
}

void View::drawGateShadow(Game& g, FrameCtx& F, const Drawable& d) {
  Pix& P = F.P;
  Vec2 cam = F.cam;
  // the gate passage lies in the gatehouse's shade, deepest under the vault
  if (art::rigVersion() >= 2) {
    // (P2b HOLDERS-B) rig 2: the 41 px passages show their whole floor and the light beyond, so the shade is shaped
    // by the form and feathered (no hard box): a vault's shade deep inside and thinning to the sunlit far mouth and
    // the jambs; a moon gate's thin wall a light one; the living arch's canopy dappled; the hedge gap's open sky none
    bld::FortParts fp;
    const bld::GateForm gf = sprites_.fortPartsOfSite(g, g.world.siteAt(d.tx + 1, d.ty, 3), fp) ? fp.gate : bld::GateForm::DrumTowers;
    const float k = gf == bld::GateForm::HedgeGap ? 0.0f : gf == bld::GateForm::MoonGate ? 0.45f : gf == bld::GateForm::LivingArch ? 0.6f : 1.0f;
    const float gx0 = d.tx * 16 + 3 - cam.x, gy0 = d.ty * 16 - cam.y;
    for (int r = -3; k > 0 && r < 16; r++) {
      static const float kRow[19] = {0.05f, 0.10f, 0.17f, 0.30f, 0.38f, 0.42f, 0.44f, 0.44f, 0.44f, 0.44f, 0.42f,
                                     0.40f, 0.36f, 0.33f, 0.30f, 0.27f, 0.24f, 0.21f, 0.18f};
      const float a = kRow[r + 3] * k;
      const Color sh(0.10f, 0.07f, 0.20f, a * 0.5f);
      if (gf == bld::GateForm::LivingArch) {   // light through the leaves: each row in three or four pieces
        int x = (int)(ew::mix64((uint64_t)(d.tx * 31 + r) * 0x9E37ull) % 5);
        for (int s = 0; x < 42 && s < 5; s++) {
          const int w = 6 + (int)(ew::mix64((uint64_t)(d.ty * 17 + r * 5 + s)) % 9);
          P.rect(gx0 + x, gy0 + r, (float)std::min(w, 42 - x), 1, Color(0.10f, 0.07f, 0.20f, a));
          x += w + 2 + (int)(ew::mix64((uint64_t)(r * 7 + s * 13 + d.tx)) % 4);
        }
        continue;
      }
      P.rect(gx0, gy0 + r, 42, 1, sh);           // the jambs' columns at half strength,
      P.rect(gx0 + 1, gy0 + r, 40, 1, sh);       // the middle at full
    }
  } else {
    P.rect(d.tx * 16 + 3 - cam.x, d.ty * 16 - cam.y, 42, 16, Color(0.10f, 0.07f, 0.20f, 0.28f));
    P.rect(d.tx * 16 + 3 - cam.x, d.ty * 16 - cam.y, 42, 10, Color(0.10f, 0.07f, 0.20f, 0.22f));
  }
}

void View::drawPropSorted(Game& g, FrameCtx& F, const Drawable& d) {
  Pix& P = F.P;
  const Map& m = F.m;
  Vec2 cam = F.cam;
  const Actor& pl = F.pl;
  bool& ghost = F.ghost;
  const std::vector<Reveal>& reveals = F.reveals;
  Prop p = (Prop)d.idx;
  if (m.kind == MapKind::Interior)
    if (uint32_t ik = interiorPropTexKey(g, m, d.tx, d.ty, p, (int)(t_ * 8 + d.tx * 3))) {
      const Tex& t = sprites_.cachedTex(*this, 0x01ull << 56 | ik, paintInteriorPiece);
      P.blit(t, d.tx * 16 + 8 - t.w / 2 - cam.x, d.ty * 16 + 16 - t.h - cam.y);
      return;
    }
  const Tex* tp = &sprites_.props_[d.idx];
  // (M1) every market stall on the overworld has its own awning and goods (by its tile: stable, never repeats
  // on one square)
  if (p == Prop::MarketStall && m.kind == MapKind::Overworld) {
    const int32_t gx = d.tx + (m.kind == MapKind::Overworld ? g.world.ox : 0), gy = d.ty + (m.kind == MapKind::Overworld ? g.world.oy : 0);
    const uint32_t h = hash2(gx, gy, 6151);
    const art::PropStyle* ps = g.mode == Mode::Title ? nullptr : sprites_.propStyleAt(*this, g, d.tx, d.ty);
    tp = ps && !ps->classic() ? &sprites_.styledPropTex(*this, p, *ps) : &sprites_.cachedTex(*this, 0x02ull << 56 | (uint64_t)(h % 36), paintStall);   // (M3b) the culture's
  }
  // (M1 economy) a trade's stall: the awning cloth steps along a row (a stall three tiles on wears the next
  // cloth, so neighbours never match), and differs between rows and squares
  // (M1 fixer round 2) each row of a market keeps one stall form (cloth booths, canvas tents or shingled timber
  // booths; by the row, so a row reads as one covered run and the next row may differ); after its closing hour
  // a stall is packed up (its stock under a cover, a curtain or shutter down): the hour its keeper leaves
  bool vendorOpen = g.mode == Mode::Title || m.kind != MapKind::Overworld || ew::stallOpen(d.tx + g.world.ox, d.ty + g.world.oy, g.hour);
  // (M5, 15.12) a hungry or famished town's stalls stay shuttered all day (nothing to sell): packed up like after hours
  if (vendorOpen && g.mode != Mode::Title && m.kind == MapKind::Overworld && (art::isVendorProp(p) || p == Prop::MarketStall) &&
      (life_.lifeFlagsAt(*this, g, d.tx, d.ty) & (life::MF_SHUTTERED | life::MF_FAMINE))) {
    vendorOpen = false;
    life_.m5Count_.stallsShut++;
  }
  // (stall facings) a row running north-south (side profiles) steps its cloth along the column instead
  int stallFacing = art::StallS;
  if (art::isStall(p) && m.kind == MapKind::Overworld) {
    stallFacing = stallFacingOn(g, m, d.tx, d.ty);
    const bool ns = stallFacing >= art::StallE;
    const int32_t gx = d.tx + g.world.ox, gy = d.ty + g.world.oy;
    const int32_t along = ns ? gy : gx, across = ns ? gx : gy;
    const uint32_t row = hash2(ns ? 1 : 0, across, 6163) % (uint32_t)art::kStallAwnings;
    const int64_t col3 = along >= 0 ? along / 3 : -((-(int64_t)along + 2) / 3);   // floor(along / 3)
    const uint32_t aw = (uint32_t)(((col3 * 5 + (int64_t)row) % art::kStallAwnings + art::kStallAwnings) % art::kStallAwnings);
    const uint32_t form = (uint32_t)stallFormOn(g, d.tx, d.ty, stallFacing);
    const bool snowy = g.mode != Mode::Title && snowyTile(m, d.tx, d.ty);
    const uint64_t sk = 0x04ull << 56 | (uint64_t)(snowy ? 1 : 0) << 16 | (uint64_t)stallFacing << 13 | (uint64_t)(vendorOpen ? 0 : 1) << 12 |
                        (uint64_t)form << 8 | (uint64_t)aw << 4 | (uint64_t)art::stallTrade(p);
    const art::PropStyle* ps = g.mode == Mode::Title ? nullptr : sprites_.propStyleAt(*this, g, d.tx, d.ty);
    if (ps && !ps->classic() && (ps->awning || ps->awningA || ps->cloth)) {   // (M3) the culture's awnings, the layout untouched
      const uint64_t k2 = ew::mix64(sk ^ ps->key() ^ 0x57A11ull);
      auto it = sprites_.styledProps_.find(k2);
      if (it != sprites_.styledProps_.end()) tp = &it->second;
      else {
        art::MarketSnow snow;   // (P3) scoped: the snow is put back to none when the bake ends
        if (snowy) { snow.z = 13.0f; snow.seed = (uint32_t)k2; }
        const art::MarketSnowScope snowScope(snow);
        tp = &(sprites_.styledProps_[k2] = pix_->bake(art::marketStallStyled(art::stallTrade(p), (int)aw, (int)form, !vendorOpen, stallFacing, *ps)));
      }
    } else
      tp = &sprites_.cachedTex(*this, sk, paintTradeStall);
  }
  propMarketTex(g, F, d, p, vendorOpen, tp);
  if (propCultureStyled(g, F, d, p, tp)) return;
  if (drawCityPropSorted(g, F, d, p, tp)) return;
  bool flipVar = false;
  propVariantTex(g, F, d, p, tp, flipVar);
  // (stall facings) the market's stalls, tables, cloths and carts are 3/4 models on canvases of their own size,
  // placed by their origin from the prop tile; an open stall or table hangs its lantern at dusk (the light pass
  // adds its glow); after the stall's closing hour it is dark and its keeper has gone
  if (m.kind == MapKind::Overworld && (art::isVendorProp(p) || p == Prop::Cart)) {
    drawVendorSorted(g, F, d, p, tp, stallFacing, vendorOpen);
    return;
  }
  // (M12 fixer) a palisade joins its run (east-west stakes, a north-south file, corners round one stake) and where
  // a road cuts it, the run's ends are squared gateposts under a timber lintel (a lord's ring, a war camp's fence)
  if (p == Prop::Palisade && m.kind == MapKind::Overworld && g.mode != Mode::Title) {
    drawPalisadePiece(g, F, d);
    return;
  }
  // (M4) a war prop in the colours of the kingdom whose camp it belongs to (realm_render.cpp)
  bool warSwap = false;
  if (art::isWarProp(p) && m.kind == MapKind::Overworld && g.mode != Mode::Title)
    if (const Tex* wt = sprites_.warPropTex(*this, g, p, d.tx, d.ty)) { tp = wt; warSwap = true; }
  // (M12) the lord's standard in its owner's arms (rise_world.cpp; the same frames as the plain prop)
  if (p == Prop::LordBanner && m.kind == MapKind::Overworld && g.mode != Mode::Title)
    if (const Tex* bt = RiseLane::riseBannerTex(*this, g, d.tx, d.ty)) tp = bt;
  // (M5, VISION_PLAN 10.3) a street lamp burns only from the lamplighter's visit at dusk (19:00..20:30 along his
  // round) until it is put out at dawn; by day and before he comes its glass is dark
  bool lampFlameOn = false;
  if (p == Prop::Lamppost && m.kind == MapKind::Overworld && g.mode != Mode::Title) {
    if (life::lampLit(d.tx + g.world.ox, d.ty + g.world.oy, g.hour)) {
      life_.m5Count_.lampsLit++;
      lampFlameOn = tp == &sprites_.props_[(int)p];   // the classic lantern: its flame drawn live over the glass (ART lampHead)
    } else {
      tp = &sprites_.lampDarkTex(*this, sprites_.propStyleAt(*this, g, d.tx, d.ty));
      life_.m5Count_.lampsDark++;
    }
  }
  const Tex& t = *tp;
  int fw = art::propW(p), fh = art::propH(p);
  int frames = warSwap ? 1 : std::max(1, art::propFrames(p));
  int fr = frames > 1 ? (int)(t_ * 8 + d.tx * 3 + d.ty) % frames : 0;
  if (p == Prop::LordBanner) fr = (int)(t_ * 5 + d.tx * 3 + d.ty) % frames;   // (M12) the heavy cloth waves slower
  bool flipP = flipVar;
  if (p == Prop::Sheep || p == Prop::Cow) {   // (M1 fixer round 2) the beasts graze at their own slow pace, either way round
    const uint32_t h = hash2(d.tx + g.world.ox, d.ty + g.world.oy, 6197);
    fr = (int)(t_ * 1.6f + (float)(h % 97u) * 0.37f) % frames;
    flipP = ((h >> 8) & 1) != 0;
  }
  float jx = 0, jy = 0;
  if (natureProp(p) && m.kind == MapKind::Overworld) {
    // (M3c fixer) by the global tile (the local one moved every patch when the window recentred); the ground
    // covers walked through (grasses, heather, ferns, brush) take a wider sub-tile offset so drifts of them never
    // line up in rows; the solids stay close to their tile (the forest rule's spacing)
    const uint32_t h = hash2(d.tx + g.world.ox, d.ty + g.world.oy, 55);
    if (art::isWildSolidProp(p)) { jx = (float)((int)(h % 7) - 3); jy = (float)((int)((h >> 4) % 3) - 1); }
    else { jx = (float)((int)(h % 11) - 5); jy = (float)((int)((h >> 4) % 7) - 3); }
  }
  float x = d.tx * 16 + 8 - fw / 2 + jx - cam.x, y = d.ty * 16 + 16 - fh + jy - cam.y;
  float alpha = 1;
  // a tree crown over the hero (drawn before it): the crown thins out and the hero shows through (ghost below)
  if ((treeProp(p) || tallWild(p)) && pl.p.y < d.ty * 16 + 15 && pl.p.y > d.ty * 16 + 16 - fh + 8 && std::fabs(pl.p.x - (d.tx * 16 + 8 + jx)) < fw * 0.5f) {
    alpha = tallWild(p) ? 0.55f : 0.6f;
    ghost = true;
  }
  if (treeProp(p) || tallWild(p))
    for (const Reveal& rv : reveals) {
      const float base = d.ty * 16.0f + 16.0f, top = base - fh + 6.0f, tx = d.tx * 16 + 8 + jx;
      // the crown stands in front (its base below the feet) and covers some of the body
      if (rv.y < base - 1.0f && rv.y > top && rv.y - rv.h < base - 8.0f && std::fabs(rv.x - tx) < fw * 0.5f + rv.hw) {
        alpha = std::min(alpha, 0.4f);
        break;
      }
    }
  P.blitEx(t, fr * fw, 0, fw, fh, x, y, (float)fw, (float)fh, flipP, Color(1, 1, 1, alpha));
  if (lampFlameOn) {   // (M5) the lit lamp's flame, flickering on its own clock
    const Tex& ft = sprites_.cachedTex(*this, 0x4Full << 56, [](uint64_t) { return art::lampFlame(0); });
    int hx = 0, hy = 0;
    art::lampHead(hx, hy);
    const int ff = (int)(t_ * 7.0f + (float)(hash2(d.tx + g.world.ox, d.ty + g.world.oy, 6301) % 97u) * 0.31f) % art::LAMP_FLAME_FRAMES;
    P.blitRegion(ft, ff * art::LAMP_FLAME_W, 0, art::LAMP_FLAME_W, art::LAMP_FLAME_H, std::floor(x + hx - art::LAMP_FLAME_W / 2.0f), std::floor(y + hy - art::LAMP_FLAME_H + 1.0f));
  }
  if (p == Prop::WatchtowerRuin && m.kind == MapKind::Overworld && g.mode != Mode::Title && story::beaconLit(g, d.tx, d.ty)) {
    // (P1b) the beacon a story lit: the fire in the broken drum, its glow on the night, sparks going up
    const int ff = (int)(t_ * 9.0f + (float)(hash2(d.tx + g.world.ox, d.ty + g.world.oy, 911) % 13u)) % 4;
    const Tex& ft = sprites_.cachedTex(*this, 0x53ull << 56 | (uint64_t)ff, paintBeaconFlame);
    const float fx = std::floor(x + fw * 0.5f - BF_W * 0.5f - 1.0f), fy = std::floor(y + 22.0f - BF_H);
    const float nk = clampf(1.0f - g.daylight() * 1.4f, 0, 1);
    P.blit(ft, fx, fy);
    // by night the fire burns over the dark (drawn again additively after the lighting) and lights the hilltop
    if (nk > 0.02f) beasts::glows.push_back({&ft, 0, ft.w, ft.h, fx, fy, false, 0.85f * nk});
    g_beaconFires.push_back(Vec2(fx + cam.x + BF_W * 0.5f, fy + cam.y + BF_H - 4.0f));
    Rng r((uint32_t)(t_ * 30) + d.tx * 13);
    if (r.f() < 0.45f) {
      Particle q;
      q.p = Vec2(fx + cam.x + BF_W * 0.5f + r.range(-4, 4), fy + cam.y + 8.0f);
      q.v = Vec2(r.range(-8, 8), r.range(-46, -24));
      q.life = q.max = 1.1f;
      q.c = Color(1, 0.62f, 0.22f);
      q.size = 1;
      fx_.parts_.push_back(q);
    }
  }
  if (p == Prop::Campfire || p == Prop::Brazier) {
    Rng r((uint32_t)(t_ * 30) + d.tx * 7);
    if (r.f() < 0.3f) { Particle q; q.p = Vec2(d.tx * 16 + 8 + r.range(-3, 3), d.ty * 16 + 6.0f); q.v = Vec2(r.range(-5, 5), r.range(-30, -15)); q.life = q.max = 0.8f; q.c = Color(1, 0.6f, 0.2f); q.size = 1; fx_.parts_.push_back(q); }
  }
}

void View::propMarketTex(Game& g, FrameCtx& F, const Drawable& d, Prop p, bool vendorOpen, const Tex*& tp) {
  const Map& m = F.m;
  if ((p == Prop::MarketTable || p == Prop::GroundCloth) && m.kind == MapKind::Overworld) {
    const int32_t gx = d.tx + g.world.ox, gy = d.ty + g.world.oy;
    const uint64_t k = (uint64_t)(vendorOpen ? 0 : 1) << 8;
    const art::PropStyle* ps = g.mode == Mode::Title ? nullptr : sprites_.propStyleAt(*this, g, d.tx, d.ty);
    if (ps && !ps->classic()) {   // (M3b) the culture's cloths and timber, the layout and goods untouched
      const bool table = p == Prop::MarketTable;
      const int a1 = table ? ew::tableGoodsAt(gx, gy) : ew::clothGoodsAt(gx, gy), a2 = table ? ew::tableShadeAt(gx, gy) : ew::clothColourAt(gx, gy);
      const uint64_t k2 = ew::mix64(ps->key() ^ ((uint64_t)(table ? 1 : 2) << 56) ^ k ^ ((uint64_t)a2 << 4) ^ (uint64_t)a1 ^ 0x7AB1Eull);
      auto it = sprites_.styledProps_.find(k2);
      tp = it != sprites_.styledProps_.end() ? &it->second
                                    : &(sprites_.styledProps_[k2] = pix_->bake(table ? art::marketTableStyled(a1, a2, !vendorOpen, *ps) : art::groundClothStyled(a1, a2, !vendorOpen, *ps)));
    } else if (p == Prop::MarketTable) tp = &sprites_.cachedTex(*this, 0x05ull << 56 | k | (uint64_t)ew::tableShadeAt(gx, gy) << 4 | (uint64_t)ew::tableGoodsAt(gx, gy), paintMarketTable);
    else tp = &sprites_.cachedTex(*this, 0x06ull << 56 | k | (uint64_t)ew::clothColourAt(gx, gy) << 4 | (uint64_t)ew::clothGoodsAt(gx, gy), paintGroundCloth);
  }
  // (M1 fixer round 2) the mine hill: its shape by its tile, its top by its land (snow, dry grass or green)
  if (p == Prop::MineHill && m.kind == MapKind::Overworld) {
    const Biome bb = m.biomeAt(d.tx, d.ty);
    const int land = bb == Biome::Snow || bb == Biome::Taiga || bb == Biome::Mountain ? 1 : (bb == Biome::Desert ? 2 : 0);
    const uint32_t mv = hash2(d.tx + g.world.ox, d.ty + g.world.oy, 6211) & 3u;
    const art::PropStyle* ps = g.mode == Mode::Title ? nullptr : sprites_.propStyleAt(*this, g, d.tx, d.ty);
    if (ps && !ps->classic()) {   // (M3b) its adit timbered in the culture's wood
      const uint64_t k2 = ew::mix64(ps->key() ^ (0x08ull << 56) ^ ((uint64_t)land << 2) ^ mv ^ 0x3111ull);
      auto it = sprites_.styledProps_.find(k2);
      tp = it != sprites_.styledProps_.end() ? &it->second : &(sprites_.styledProps_[k2] = pix_->bake(art::mineHillStyled((int)mv, land, *ps)));
    } else tp = &sprites_.cachedTex(*this, 0x08ull << 56 | (uint64_t)land << 2 | (uint64_t)mv, paintMineHill);
  }
  // (M1 fixer round 2) the mine's track joins its neighbours (and runs in under the adit's frame)
  if (p == Prop::MineRail) {
    auto railAt = [&](int x, int y) {
      const int q = m.propAt(x, y);
      return q == (int)Prop::MineRail + 1 || q == (int)Prop::OreCart + 1;
    };
    int j = 0;
    if (railAt(d.tx, d.ty - 1) || m.propAt(d.tx, d.ty - 1) == (int)Prop::MineEntrance + 1 || m.propAt(d.tx, d.ty - 1) == (int)Prop::MineHill + 1) j |= 1;
    if (railAt(d.tx + 1, d.ty)) j |= 2;
    if (railAt(d.tx, d.ty + 1)) j |= 4;
    if (railAt(d.tx - 1, d.ty)) j |= 8;
    tp = &sprites_.cachedTex(*this, 0x07ull << 56 | (uint64_t)j, paintMineRail);
  }
  // (M1) every piece of a ruin's walls and columns has its own broken top (by its global tile)
  if ((p == Prop::RuinWall || p == Prop::RuinColumn) && m.kind == MapKind::Overworld) {
    const uint32_t h = hash2(d.tx + g.world.ox, d.ty + g.world.oy, 6173);
    int joins = 0;   // (M8) the wall's run: a north-south run shows one continuous wall top
    if (p == Prop::RuinWall) {
      const int rw = (int)Prop::RuinWall + 1;
      if (m.propAt(d.tx, d.ty - 1) == rw) joins |= 1;
      if (m.propAt(d.tx + 1, d.ty) == rw) joins |= 2;
      if (m.propAt(d.tx, d.ty + 1) == rw) joins |= 4;
      if (m.propAt(d.tx - 1, d.ty) == rw) joins |= 8;
    }
    tp = &sprites_.cachedTex(*this, 0x03ull << 56 | (uint64_t)p << 8 | (uint64_t)(h % 8) | (uint64_t)joins << 4, paintRuin);
  }
  // (M1) a banner inside a kingdom's settlement flies that kingdom's colours
  if (p == Prop::Banner && m.kind == MapKind::Overworld && g.mode != Mode::Title) {
    const int bs = g.world.siteAt(d.tx, d.ty, 2);
    if (const Kingdom* k = g.world.kingdomOf(bs)) {
      // (M3) the kingdom's full arms where its culture gives them, else its two colours and emblem
      const cult::Culture* kc = g.world.cultureOfKingdom(g.world.sites[(size_t)bs].kingdom);
      tp = (kc && !kc->heraldry.empty()) ? &sprites_.heraldryTex(*this, 0, kc->heraldry, 0, 0) : &sprites_.kingdomTex(*this, 0, *k, 0);
      // (M12) a realm the generator never planned (a warlord's, a successor's, the player's own) flies its own arms
      const rui::Look RL = rui::look(g, k->id);
      if (RL.made || RL.own) { cult::Heraldry hr; rui::arms(RL, hr); tp = &sprites_.heraldryTex(*this, 0, hr, 0, 0); }
    } else {   // (M3b) no kingdom: the culture's own banner
      const art::PropStyle* ps = sprites_.propStyleAt(*this, g, d.tx, d.ty);
      if (!ps->classic()) tp = &sprites_.styledPropTex(*this, p, *ps);
    }
  }
}

bool View::propCultureStyled(Game& g, FrameCtx& F, const Drawable& d, Prop p, const Tex*& tp) {
  Pix& P = F.P;
  const Map& m = F.m;
  Vec2 cam = F.cam;
  // (M3) the settlement's culture styles its fences, wells, lamps, benches, statues, shrines and fire bowls
  if (m.kind == MapKind::Overworld && g.mode != Mode::Title && p != Prop::Banner && !art::isStall(p) && p != Prop::MarketStall &&
      p != Prop::MarketTable && p != Prop::GroundCloth && p != Prop::MineHill && p != Prop::StandingStone && art::propStyled(p)) {
    // (M3b) the wayside's built props (signposts, toll posts, graves) in the land's culture, the rest in their settlement's
    const bool wayside = p == Prop::Signpost || p == Prop::TollPost || p == Prop::GraveCairn || p == Prop::Gravestone || p == Prop::Shrine;
    const art::PropStyle* ps = wayside ? sprites_.landStyleAt(*this, g, d.tx, d.ty) : sprites_.propStyleAt(*this, g, d.tx, d.ty);
    if (!ps->classic()) tp = &sprites_.styledPropTex(*this, p, *ps);
    // (M3 fixer round 2) a dry-stone dyke joins its neighbours: one wall of one height, closed L / T corners
    // (M3b round 3) and so does a clipped hedge (a broken corner and a flat summer-green strip before), snow on
    // its top in the cold where the trees carry snow
    const bool hedge = !ps->classic() && ps->fence == art::Fence::Hedge;
    // (M7 fix) the wattle hurdles, bamboo palisades and rope fences join the same way (a lone cane per tile before,
    // stopping short of the corner and drawn over the next run's rails), and every run's end facing a one-tile
    // gateway gets its gatepost or pier (a bare cut before), a run that simply stops a capping post
    const bool joined = ps->fence == art::Fence::StoneDyke || hedge || ps->fence == art::Fence::Wattle || ps->fence == art::Fence::Bamboo ||
                        ps->fence == art::Fence::Rope;
    if (!ps->classic() && joined && (p == Prop::FenceH || p == Prop::FenceV)) {
      auto fz = [&](int x, int y) { const int q = m.propAt(x, y); return q == (int)Prop::FenceH + 1 || q == (int)Prop::FenceV + 1; };
      int mask = (fz(d.tx, d.ty - 1) ? 1 : 0) | (fz(d.tx + 1, d.ty) ? 2 : 0) | (fz(d.tx, d.ty + 1) ? 4 : 0) | (fz(d.tx - 1, d.ty) ? 8 : 0);
      if (!(mask & 10) && !(mask & 5)) mask = p == Prop::FenceH ? 10 : 5;   // a lone piece keeps its run's axis
      {
        static const int DX[4] = {0, 1, 0, -1}, DY[4] = {-1, 0, 1, 0};
        int gates = 0, ends = 0;
        for (int k = 0; k < 4; k++) {
          const int bit = 1 << k, back = 1 << ((k + 2) & 3);
          const int nx = d.tx + DX[k], ny = d.ty + DY[k];
          const int perp = (1 << ((k + 1) & 3)) | (1 << ((k + 3) & 3));
          if (fz(nx, ny) || !(mask & back) || (mask & bit) || (mask & perp)) continue;   // joined that way, a corner, or no run that way
          const bool open = m.in(nx, ny) && !m.propAt(nx, ny) && !m.blocked(nx, ny);
          if (open && fz(nx + DX[k], ny + DY[k])) gates |= bit;      // a one-tile gateway: the jamb at the tile edge
          else ends |= bit;                                           // the run stops: a post caps it
        }
        mask |= gates << 4 | ends << 8;
      }
      bool snow = false;
      if (hedge) {
        const Biome hb = m.biomeAt(d.tx, d.ty);
        snow = hb == Biome::Snow || m.at(d.tx, d.ty) == Ground::Snow || (hb == Biome::Taiga && m.heightAt(d.tx, d.ty) >= 4);
      }
      const uint64_t k = ew::mix64(ps->key() ^ ((uint64_t)mask << 40) ^ ((uint64_t)(0x40 + (hedge ? 16 : 0) + (snow ? 32 : 0)) << 56) ^ 0xD7CF1ull);
      auto it = sprites_.styledProps_.find(k);
      const Tex& dt = it != sprites_.styledProps_.end() ? it->second
                                               : (sprites_.styledProps_[k] = pix_->bake(hedge ? art::hedgePiece(mask, *ps, snow)
                                                                                     : ps->fence == art::Fence::StoneDyke ? art::dykePiece(mask, *ps)
                                                                                                                           : art::fenceJoinPiece(mask, *ps)));
      P.blit(dt, d.tx * 16.0f - cam.x, d.ty * 16.0f + 16.0f - (float)dt.h - cam.y);
      // (M7 fixer r2, review: "the hedge lot's gate is two loose posts, no gate leaf") a one-tile gateway east or
      // south of this piece hangs a field gate, half open (hedges, dykes and hurdles; the piece west of / north of
      // the gateway draws it, once)
      const int gw = (mask >> 4) & 15;
      if ((gw & 6) && (hedge || ps->fence == art::Fence::StoneDyke || ps->fence == art::Fence::Wattle)) {
        const bool ns = (gw & 4) != 0;
        const uint64_t gk = ew::mix64(0x6A7E1EAFull ^ (ns ? 1ull : 2ull));
        auto gi = sprites_.styledProps_.find(gk);
        const Tex& gt = gi != sprites_.styledProps_.end() ? gi->second : (sprites_.styledProps_[gk] = pix_->bake(art::fieldGatePiece(ns)));
        const int gx = d.tx + (ns ? 0 : 1), gy = d.ty + (ns ? 1 : 0);
        P.blit(gt, gx * 16.0f - cam.x, gy * 16.0f + 16.0f - (float)gt.h - cam.y);
      }
      return true;
    }
  }
  return false;
}

bool View::drawCityPropSorted(Game& g, FrameCtx& F, const Drawable& d, Prop p, const Tex*& tp) {
  Pix& P = F.P;
  const Map& m = F.m;
  Vec2 cam = F.cam;
  const Actor& pl = F.pl;
  // (M7b CITYART) the city props in their settlement's culture and a variant by their global tile (the fruit by the
  // climate); garden walls and hedges joined into runs (as the fences: straight, corner, T, the piers at a gateway
  // or a run's end), the souk's canopy joined over its street (a hem and poles where it stops)
  if (art::isCityProp(p) && m.kind == MapKind::Overworld && g.mode != Mode::Title) {
    const art::PropStyle* ps = sprites_.propStyleAt(*this, g, d.tx, d.ty);
    // (M12 fixer) a wall ringing a village just outside its bounds (a lord's stone walls) is built in the village's
    // style all the way round, never half in the land's plain stone
    if (p == Prop::GardenWall && ps->classic() && g.world.endless) {
      const int ns = g.world.siteAt(d.tx, d.ty, 7);
      if (ns >= 0 && g.world.sites[(size_t)ns].settlement()) ps = sprites_.propStyleAt(*this, g, g.world.sites[(size_t)ns].ex, g.world.sites[(size_t)ns].ey);
    }
    const uint32_t h = hash2(d.tx + g.world.ox, d.ty + g.world.oy, 6311);
    int variant = 0, joins = 0;
    const bool snow = snowyTile(m, d.tx, d.ty);
    if (p == Prop::FruitTree) variant = art::fruitTreeKind((int)m.biomeAt(d.tx, d.ty), h);
    else if (art::cityPropVariants(p) > 1) variant = (int)(h % (uint32_t)art::cityPropVariants(p));
    auto same = [&](int x, int y) { return m.propAt(x, y) == (int)p + 1; };
    // (fixer M7b r1, review: "the souk canopies are a jumbled stripy mass with a rectangular fade patch") a souk's
    // bay is one cloth: its variant from the bay (the run along the street from its open end, across the street
    // from its edge), not the tile; it fades whole while the hero stands under it
    bool bayFade = false;
    int gwSpan[4] = {0, 0, 0, 0};
    if (p == Prop::Canopy) {
      joins = (same(d.tx, d.ty - 1) ? 1 : 0) | (same(d.tx + 1, d.ty) ? 2 : 0) | (same(d.tx, d.ty + 1) ? 4 : 0) | (same(d.tx - 1, d.ty) ? 8 : 0);
      auto runLen = [&](int dx, int dy) { int n = 0; while (n < 10 && same(d.tx + dx * (n + 1), d.ty + dy * (n + 1))) n++; return n; };
      const int rW = runLen(-1, 0), rE = runLen(1, 0), rN = runLen(0, -1), rS = runLen(0, 1);
      const bool alongX = rW + rE >= rN + rS;
      const int bx0 = d.tx - rW, bx1 = d.tx + rE, by0 = d.ty - rN, by1 = d.ty + rS;
      const uint32_t bh = alongX ? hash2(bx0 + g.world.ox, by0 + g.world.oy, 6313) : hash2(bx0 + g.world.ox, by0 + g.world.oy, 6317);
      variant = (int)(bh % 4u);
      const float hx = pl.p.x / 16.0f, hy = pl.p.y / 16.0f;
      bayFade = hx > bx0 - 0.3f && hx < bx1 + 1.3f && hy > by0 - 0.6f && hy < by1 + 1.9f;
    }
    if (p == Prop::GardenWall || p == Prop::Hedge) {
      int mask = (same(d.tx, d.ty - 1) ? 1 : 0) | (same(d.tx + 1, d.ty) ? 2 : 0) | (same(d.tx, d.ty + 1) ? 4 : 0) | (same(d.tx - 1, d.ty) ? 8 : 0);
      if (!mask) {   // a lone piece runs the way its neighbours' walls or fences do, else east-west
        auto wallish = [&](int x, int y) { const int q = m.propAt(x, y); return q == (int)Prop::FenceH + 1 || q == (int)Prop::GardenWall + 1 || q == (int)Prop::Hedge + 1; };
        mask = (wallish(d.tx, d.ty - 1) || wallish(d.tx, d.ty + 1)) && !(wallish(d.tx - 1, d.ty) || wallish(d.tx + 1, d.ty)) ? 5 : 10;
      }
      static const int DX[4] = {0, 1, 0, -1}, DY[4] = {-1, 0, 1, 0};
      int gates = 0, ends = 0;
      for (int k = 0; k < 4; k++) {
        const int bit = 1 << k, back = 1 << ((k + 2) & 3);
        const int nx = d.tx + DX[k], ny = d.ty + DY[k];
        const int perp = (1 << ((k + 1) & 3)) | (1 << ((k + 3) & 3));
        if (same(nx, ny) || !(mask & back) || (mask & bit) || (mask & perp)) continue;
        const bool open = m.in(nx, ny) && !m.propAt(nx, ny) && !m.blocked(nx, ny);
        if (open && same(nx + DX[k], ny + DY[k])) gates |= bit;
        else ends |= bit;
      }
      joins = mask | gates << 4 | ends << 8;
      // (M12 fixer r2) a lord's stone ring where a road cuts it: the posts across the road (1..5 open tiles, nothing
      // standing in the way) become a gateway (gwSpan: the posts' distance in tiles, by side N E S W)
      bool lordRing = false;
      if (p == Prop::GardenWall && g.world.endless && !g.lordship.holdings.empty()) {
        const int ns = g.world.siteAt(d.tx, d.ty, 7);
        if (ns >= 0)
          for (const lord::Holding& lh : g.lordship.holdings)
            if ((lh.works & lord::LW_WALLS) && g.world.siteHandle(lh.site) == ns) lordRing = true;
      }
      // the gap must be the road's cut: every open tile road, plaza, bridge or a trodden path (the ring leaves out
      // exactly those), and not the ground beside a run that steps aside and back (a stair of the ring's curve, where
      // the gap's tiles lie along the wall: no gateway there)
      for (int k = 0; k < 4 && lordRing; k++) {
        if (mask & (1 << k)) continue;
        const int pk = (k + 1) & 3;
        int along = 0;
        for (int n = 1; n <= 6; n++) {
          const int nx = d.tx + DX[k] * n, ny = d.ty + DY[k] * n;
          if (!m.in(nx, ny)) break;
          if (same(nx, ny)) { if (n >= 2 && along * 2 <= n - 1) gwSpan[k] = n; break; }
          if (m.propAt(nx, ny) || m.blocked(nx, ny)) break;
          const Ground gr = m.at(nx, ny);
          if (gr != Ground::Road && gr != Ground::Plaza && gr != Ground::Bridge && gr != Ground::Dirt) break;
          if (same(nx + DX[pk], ny + DY[pk]) || same(nx - DX[pk], ny - DY[pk])) along++;
        }
      }
    }
    const uint64_t sk = art::cityPropStyledByCulture(p) || p == Prop::Hedge ? ps->key() : 0;
    const uint64_t k = ew::mix64(sk ^ ((uint64_t)p << 48) ^ ((uint64_t)variant << 40) ^ ((uint64_t)joins << 20) ^ (snow ? 0x51u : 0u) ^ 0xC17Aull);
    auto it = sprites_.styledProps_.find(k);
    const Tex& ct = it != sprites_.styledProps_.end() ? it->second : (sprites_.styledProps_[k] = pix_->bake(art::cityPropStyled(p, *ps, variant, joins, snow)));
    if (p == Prop::GardenWall || p == Prop::Hedge) {   // (16 wide, its bottom on the tile's bottom edge, as the fences)
      P.blit(ct, d.tx * 16.0f - cam.x, d.ty * 16.0f + 16.0f - (float)ct.h - cam.y);
      // (M12 fixer r2) the gateway: a gate tower over each post, the arch (or the way) drawn by the west (north) post
      if (gwSpan[0] | gwSpan[1] | gwSpan[2] | gwSpan[3]) {
        const uint32_t gs = hash2(d.tx + g.world.ox, d.ty + g.world.oy, 6343);
        auto gateTex = [&](uint64_t tag, auto&& paint) -> const Tex& {
          const uint64_t gk = ew::mix64(sk ^ tag ^ (snow ? 0x51ull : 0ull) ^ 0x6A7E6A7Eull);
          auto gi = sprites_.styledProps_.find(gk);
          return gi != sprites_.styledProps_.end() ? gi->second : (sprites_.styledProps_[gk] = pix_->bake(paint()));
        };
        const Tex& tt = gateTex(0x7011ull << 32 | (gs & 7), [&] { return art::stoneGateTower(ps->culture, ps->stone, snow, gs & 7); });
        P.blit(tt, d.tx * 16.0f - cam.x, d.ty * 16.0f + 16.0f - (float)tt.h - cam.y);
        if (gwSpan[1]) {
          const int span = gwSpan[1] * 16 - 11;
          const Tex& at = gateTex(0xA4Cull << 32 | (uint64_t)span << 8 | (gs >> 8 & 7), [&] { return art::stoneGateArch(span, ps->culture, ps->stone, snow, gs >> 8 & 7); });
          P.blit(at, d.tx * 16.0f + 13.0f - cam.x, d.ty * 16.0f + 16.0f - (float)at.h - cam.y);
        }
        if (gwSpan[2]) {
          const Tex& wt = gateTex(0x3A7ull << 32 | (uint64_t)gwSpan[2] << 8 | (gs >> 8 & 7), [&] { return art::stoneGateWay(gwSpan[2], ps->culture, ps->stone, snow, gs >> 8 & 7); });
          P.blit(wt, d.tx * 16.0f - cam.x, d.ty * 16.0f - 28.0f - cam.y);
        }
      }
      return true;
    }
    if (p == Prop::Canopy) {   // drawn over the walkers under it; its bay thins while the hero is under it
      P.blitEx(ct, 0, 0, ct.w, ct.h, d.tx * 16.0f - cam.x, d.ty * 16.0f + 16.0f - (float)ct.h - cam.y, (float)ct.w, (float)ct.h, false, Color(1, 1, 1, bayFade ? 0.5f : 1.0f));
      return true;
    }
    tp = &ct;
  }
  return false;
}

void View::propVariantTex(Game& g, FrameCtx& F, const Drawable& d, Prop p, const Tex*& tp, bool& flipVar) {
  const Map& m = F.m;
  // (M2) every peak of a range is one of eight shapes in its land's rock (by its global tile: never one peak cloned)
  if ((p == Prop::Peak || p == Prop::GreatPeak) && m.kind == MapKind::Overworld) {
    const int land = peakLand(m, d.tx, d.ty);
    tp = &sprites_.cachedTex(*this, 0x09ull << 56 | (p == Prop::GreatPeak ? 128ull : 0ull) | (uint64_t)(land & 15) << 3 | (uint64_t)(hash2(d.tx + g.world.ox, d.ty + g.world.oy, 6229) & 7u), paintPeak);
  }
  // (M2 fixer round 2) the wild's commonest props in variants by their global tile, some flipped
  // (M3b) every standing stone of a ring is its own stone (height, lean, top, lichen), in its land's culture
  if (p == Prop::StandingStone && m.kind == MapKind::Overworld) {
    const uint32_t h = hash2(d.tx + g.world.ox, d.ty + g.world.oy, 6257);
    const art::PropStyle* ps = g.mode == Mode::Title ? nullptr : sprites_.landStyleAt(*this, g, d.tx, d.ty);
    const int cul = ps ? (int)ps->culture : 0;
    tp = &sprites_.cachedTex(*this, 0x0Cull << 56 | (uint64_t)cul << 8 | (uint64_t)(h & 15u), paintStandingStone);   // (never flipped: the light)
  }
  if ((p == Prop::TallGrass || p == Prop::Boulder) && m.kind == MapKind::Overworld) {
    const uint32_t h = hash2(d.tx + g.world.ox, d.ty + g.world.oy, 6241);
    tp = &sprites_.cachedTex(*this, (p == Prop::TallGrass ? 0x0Aull : 0x0Bull) << 56 | (uint64_t)(h & 15u), p == Prop::TallGrass ? paintTallGrass : paintBoulder);
    flipVar = ((h >> 5) & 1) != 0;
  }
  // (M3c) the Wildlands flora: a variant by its global tile, as its biome grows it (art_flora.cpp floraVariant;
  // the variants carry their own mirroring, so never flipped here: the light stays top-left)
  if (art::isWildlandsFlora(p) && m.kind == MapKind::Overworld) {
    const uint32_t h = hash2(d.tx + g.world.ox, d.ty + g.world.oy, 6271);
    const int nv = std::max(1, art::floraVariants(p));
    tp = &sprites_.cachedTex(*this, 0x5Eull << 56 | (uint64_t)p << 16 | (uint64_t)m.ecoAt(d.tx, d.ty) << 8 | (uint64_t)(h % (uint32_t)nv), paintFlora);
  }
  // (M3c fixer round 2) a leafy classic tree standing in the snow (a town's oaks and willows, an elven heart tree)
  // carries snow on its crown like the pines beside it
  if (m.kind == MapKind::Overworld && (p == Prop::OakTree || p == Prop::OakTree2 || p == Prop::WillowTree || p == Prop::BirchTree ||
                                       p == Prop::AutumnTree || p == Prop::ElderTree)) {
    const Biome hb = m.biomeAt(d.tx, d.ty);
    const int lv = m.heightAt(d.tx, d.ty);
    if (m.at(d.tx, d.ty) == Ground::Snow || hb == Biome::Snow || (hb == Biome::Taiga && lv >= 4) || (hb == Biome::Mountain && lv >= 5))
      tp = &sprites_.cachedTex(*this, 0x5Full << 56 | (uint64_t)p, paintWinterTree);
  }
}

void View::drawVendorSorted(Game& g, FrameCtx& F, const Drawable& d, Prop p, const Tex* tp, int stallFacing, bool vendorOpen) {
  Pix& P = F.P;
  const Map& m = F.m;
  Vec2 cam = F.cam;
  const Actor& pl = F.pl;
  bool& ghost = F.ghost;
  int ox = 0, oy = 0;
  if (art::isStall(p)) art::stallOrigin(stallFacing, ox, oy);
  else if (p == Prop::MarketTable) art::marketTableOrigin(ox, oy);
  else if (p == Prop::GroundCloth) art::groundClothOrigin(ox, oy);
  else {
    // (P1 fixer r2) the street incident's cart stands with its near wheel off until the wheel is going back on
    // (the wheel lies in the road meanwhile: drawIncidentGround, render_markers.cpp)
    const life::IncidentView iv = life::incidentNow(g);
    const bool brokenCart = iv.kind == life::INC_CART && iv.holdT < 3.0f && iv.gx - g.world.ox == d.tx && iv.gy - g.world.oy == d.ty;
    tp = &sprites_.cachedTex(*this, 0x3Dull << 56 | (uint64_t)(brokenCart ? 1 : 0) << 9 | (uint64_t)(g.mode != Mode::Title && snowyTile(m, d.tx, d.ty) ? 1 : 0) << 8 | (uint64_t)cartLook(g, d.tx, d.ty), paintCart);
    art::marketCartOrigin(ox, oy);
  }
  const float x = d.tx * 16.0f + ox - cam.x, y = d.ty * 16.0f + oy - cam.y;
  P.blit(*tp, x, y);
  // (fixer M5 r3, review: "no silhouette when the hero is behind a stall") a stall's roof and counter drawn over
  // the hero (who sorts before it) hide them whole: the hero shows through as the buildings' silhouette
  if (art::isStall(p) && g.mode != Mode::Title && pl.p.y < d.y) {
    const bool ns = stallFacing >= art::StallE;
    const float hx0 = ns ? d.tx * 16.0f + ox + 8.0f : d.tx * 16.0f - 14.0f, hx1 = ns ? d.tx * 16.0f + ox + tp->w - 8.0f : d.tx * 16.0f + 30.0f;
    if (pl.p.x > hx0 && pl.p.x < hx1 && pl.p.y > d.ty * 16.0f + oy + 18.0f) ghost = true;
  }
  const bool lamp = g.mode != Mode::Title && g.daylight() < 0.55f && vendorOpen;
  if (lamp && p == Prop::MarketTable) {   // a candle lantern standing at the table's far end
    const float lx = d.tx * 16.0f + 27.0f - cam.x, ly = d.ty * 16.0f - 6.0f - cam.y;
    P.rect(lx - 1, ly - 3, 3, 1, Color(0.32f, 0.24f, 0.16f));
    P.rect(lx - 1, ly - 2, 3, 4, Color(1.0f, 0.78f, 0.38f));
    P.rect(lx, ly - 1, 1, 2, Color(1.0f, 0.97f, 0.78f));
    P.rect(lx - 1, ly + 2, 3, 1, Color(0.32f, 0.24f, 0.16f));
  }
  if (lamp && art::isStall(p)) {
    int lxi, lyi;
    art::stallLantern(stallFacing, lxi, lyi);
    const float lx = d.tx * 16.0f + lxi - cam.x, ly = d.ty * 16.0f + lyi - cam.y;
    P.rect(lx, ly - 5, 1, 2, Color(0.22f, 0.18f, 0.14f));       // the hook
    P.rect(lx - 2, ly - 3, 5, 1, Color(0.32f, 0.24f, 0.16f));   // the cap
    P.rect(lx - 2, ly - 2, 5, 4, Color(1.0f, 0.78f, 0.38f));    // the glass
    P.rect(lx - 1, ly - 1, 3, 2, Color(1.0f, 0.97f, 0.78f));    // the flame
    P.rect(lx - 2, ly + 2, 5, 1, Color(0.32f, 0.24f, 0.16f));   // the base
  }
}

void View::drawPalisadePiece(Game& g, FrameCtx& F, const Drawable& d) {
  Pix& P = F.P;
  const Map& m = F.m;
  Vec2 cam = F.cam;
  const Prop p = Prop::Palisade;
  (void)sprites_.warPropTex(*this, g, p, d.tx, d.ty);   // the war overlay's counts (the camp's owner) as before
  auto pz =[&](int x, int y) { return m.propAt(x, y) == (int)Prop::Palisade + 1; };
  int mask = (pz(d.tx, d.ty - 1) ? 1 : 0) | (pz(d.tx + 1, d.ty) ? 2 : 0) | (pz(d.tx, d.ty + 1) ? 4 : 0) | (pz(d.tx - 1, d.ty) ? 8 : 0);
  static const int DX[4] = {0, 1, 0, -1}, DY[4] = {-1, 0, 1, 0};
  int gates = 0, span[4] = {0, 0, 0, 0};
  for (int k = 0; k < 4; k++) {
    if (mask & (1 << k)) continue;
    int n = 1;
    for (; n <= 5; n++) {
      const int nx = d.tx + DX[k] * n, ny = d.ty + DY[k] * n;
      if (!m.in(nx, ny)) { n = 99; break; }
      if (pz(nx, ny)) break;
      if (m.propAt(nx, ny) || m.blocked(nx, ny)) { n = 99; break; }
    }
    if (n >= 2 && n <= 5) { gates |= 1 << k; span[k] = n; }
  }
  // a one-run gateway: the side across the road joins the run's axis (a lone stake by a road is the run's end)
  if (!mask) mask = (gates & 5) && !(gates & 10) ? 5 : 10;
  const uint32_t ph = hash2(d.tx + g.world.ox, d.ty + g.world.oy, 6337);
  const uint64_t pk = ew::mix64(0x9A115ADEull ^ ((uint64_t)mask << 8) ^ ((uint64_t)gates << 12) ^ ((uint64_t)(ph & 7) << 20));
  auto pit = sprites_.styledProps_.find(pk);
  const Tex& pt = pit != sprites_.styledProps_.end() ? pit->second : (sprites_.styledProps_[pk] = pix_->bake(art::palisadePiece(mask, gates, ph & 7)));
  P.blit(pt, d.tx * 16.0f - cam.x, d.ty * 16.0f + 16.0f - (float)pt.h - cam.y);
  // the lintel over the gateway, drawn once by the west / north post (a beam lit on top, its shadow on the road)
  const Color b0(0.66f, 0.50f, 0.33f), b1(0.50f, 0.35f, 0.22f), b2(0.36f, 0.24f, 0.16f), b3(0.22f, 0.15f, 0.11f);
  if (gates & 2) {
    const float x0 = d.tx * 16.0f + 11 - cam.x, x1 = (d.tx + span[1]) * 16.0f + 5 - cam.x;
    const float y0 = d.ty * 16.0f + 16 - 4 - 27 - cam.y;
    P.rect(x0 + 3, d.ty * 16.0f + 16 - cam.y, x1 - x0, 2, Color(0.05f, 0.03f, 0.06f, 0.16f));
    P.rect(x0, y0, x1 - x0, 1, b0);
    P.rect(x0, y0 + 1, x1 - x0, 2, b1);
    P.rect(x0, y0 + 3, x1 - x0, 1, b2);
    P.rect(x0, y0 + 4, x1 - x0, 1, b3);
    for (float bx = x0 + 5; bx < x1 - 3; bx += 7) P.rect(bx, y0 + 1, 1, 2, b2);   // the adze marks
    // a pair of braces from the posts to the beam
    for (int j = 0; j < 4; j++) { P.rect(x0 + 1 + j, y0 + 5 + j, 1, 1, b2); P.rect(x1 - 2 - j, y0 + 5 + j, 1, 1, b2); }
  }
  if (gates & 4) {
    const float xx = d.tx * 16.0f + 5 - cam.x;
    const float y0 = d.ty * 16.0f + 16 - 4 - 29 - cam.y, y1 = (d.ty + span[2]) * 16.0f + 16 - 4 - 12 - 29 - cam.y;
    if (y1 > y0) {
      P.rect(xx + 6, y0 + 30, 2, y1 - y0, Color(0.05f, 0.03f, 0.06f, 0.16f));
      P.rect(xx, y0, 1, y1 - y0 + 4, b0);
      P.rect(xx + 1, y0, 3, y1 - y0 + 4, b1);
      P.rect(xx + 4, y0, 1, y1 - y0 + 4, b2);
      P.rect(xx + 5, y0, 1, y1 - y0 + 4, b3);
    }
  }
}

void View::drawBuildingSorted(Game& g, FrameCtx& F, const Drawable& d) {
  Pix& P = F.P;
  const Map& m = F.m;
  Vec2 cam = F.cam;
  const Actor& pl = F.pl;
  bool& ghost = F.ghost;
  const Bldg& b = m.bldgs[d.idx];
  // (M1 round 3) behind a fade (a fast travel's arrival, a new game) the buildings not painted yet are painted
  // a few per frame within a budget instead of all in the arrival frame (a capital's 70-90 sprites took ~150 ms
  // on the desktop, more on a phone's wasm); the fade hides the ones still waiting
  // ((Witness) never: a building in view is painted inline, so a shot through the fade never misses one)
  if (!witness && (g.sleepFade > 0.5f || fade_ > 0.5f) && !sprites_.bldgTex_.count(sprites_.bldgKey(m, b, d.idx))) {
    if (fadePaintMs_ >= 10.0) return;
    const auto t0 = std::chrono::steady_clock::now();
    const bool done = sprites_.bldgPaintStep(*this, b, d.idx, std::max(0.5, std::min(4.0, 10.0 - fadePaintMs_)));   // (M3: in steps)
    fadePaintMs_ += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    if (!done) return;
  }
  const Tex* tp = &sprites_.bldgTex(*this, b, d.idx);
  if (m.kind == MapKind::Overworld && g.mode != Mode::Title && windowsLit(g, b)) {
    auto nt = sprites_.bldgNight_.find(sprites_.bldgKey(m, b, d.idx));
    if (nt != sprites_.bldgNight_.end()) tp = &nt->second;
    life_.m5Count_.winLit++;
  } else if (m.kind == MapKind::Overworld && g.mode != Mode::Title && !g.inside && g.daylight() <= 0.55f) life_.m5Count_.winDark++;
  const Tex& t = *tp;
  const float bottom = (b.r.y + b.r.h) * 16.0f, top = bottom + art::BLDG_PAD_B - t.h;
  float x = b.r.x * 16.0f - art::BLDG_PAD_X - cam.x, y = top - cam.y;
  // the hero behind the building (feet hidden under its walls or roof): the building stays solid and the hero
  // shows through it as a silhouette (drawn after everything, below). Fading the whole house - door, windows and
  // all - turned buildings into ghosts whenever the hero passed behind a roof.
  auto tr = sprites_.bldgTopRow_.find(sprites_.bldgKey(m, b, d.idx));
  float vis = top + (tr != sprites_.bldgTopRow_.end() ? (float)tr->second : 0.0f);
  if (pl.p.y < bottom - 4 && pl.p.y > vis + 6 && pl.p.x > b.r.x * 16 - 2 && pl.p.x < (b.r.x + b.r.w) * 16 + 2) ghost = true;
  P.blitEx(t, 0, 0, t.w, t.h, x, y, (float)t.w, (float)t.h, false, Color(1, 1, 1, 1));
  // chimney smoke: soft puffs that rise, drift east with the wind and spread
  auto sm = sprites_.bldgSmoke_.find(sprites_.bldgKey(m, b, d.idx));
  if (sm != sprites_.bldgSmoke_.end() && g.mode != Mode::Title)
    for (size_t si = 0; si < sm->second.size(); si++) {
      Rng r((uint32_t)(t_ * 40) * 31u + (uint32_t)d.idx * 977u + (uint32_t)si * 13u);
      if (r.f() > 0.10f) continue;
      Particle q;
      q.p = Vec2(b.r.x * 16.0f - art::BLDG_PAD_X + sm->second[si].x + r.range(-1, 1), top + sm->second[si].y);
      q.v = Vec2(r.range(2, 7), r.range(-13, -8));
      q.life = q.max = r.range(2.2f, 3.2f);
      float gr = r.range(0.72f, 0.86f);
      q.c = Color(gr, gr, gr + 0.04f);
      q.size = r.f() < 0.5f ? 2.0f : 1.0f;
      q.grav = -1.5f;
      q.kind = 2;
      fx_.parts_.push_back(q);
    }
  if (b.charred && m.kind == MapKind::Overworld) realm_.burnedFx(*this, g, m, b, d.idx, 0);   // (M4) a burned building smoulders
}

void View::drawWallSorted(FrameCtx& F, const Drawable& d) {
  Pix& P = F.P;
  Vec2 cam = F.cam;
  const Tex& t = sprites_.wallTileTex(*this, (uint32_t)d.idx);
  P.blit(t, d.tx * 16.0f - art::WALL_OX - cam.x, d.ty * 16.0f - art::WALL_OY - cam.y);
}

void View::drawGateSorted(Game& g, FrameCtx& F, const Drawable& d) {
  Pix& P = F.P;
  const Map& m = F.m;
  Vec2 cam = F.cam;
  const Actor& pl = F.pl;
  bool& ghost = F.ghost;
  // (M1) the gatehouse flies the banner of the kingdom whose town it guards
  const Tex* gt = &sprites_.gateTex_;
  // (M3) the gate in its wall's culture style (Map::wall on the flanking tile is 1 + CityWall) and the kingdom's arms
  int ws = 0;
  if (m.in(d.tx - 1, d.ty) && m.wall[(size_t)d.ty * m.w + d.tx - 1] > 1) ws = m.wall[(size_t)d.ty * m.w + d.tx - 1] - 1;
  else if (m.in(d.tx + 3, d.ty) && m.wall[(size_t)d.ty * m.w + d.tx + 3] > 1) ws = m.wall[(size_t)d.ty * m.w + d.tx + 3] - 1;
  const int gs = g.world.siteAt(d.tx + 1, d.ty, 3);
  bld::FortParts fp;
  if (sprites_.fortPartsOfSite(g, gs, fp)) {   // (M3b) the gate in its culture's form, flying its kingdom's arms
    cult::Heraldry arms;
    if (const Kingdom* k = g.world.kingdomOf(gs)) {
      const cult::Culture* kc = g.world.cultureOfKingdom(g.world.sites[(size_t)gs].kingdom);
      if (kc && !kc->heraldry.empty()) arms = kc->heraldry;
      else { arms.field = k->color; arms.charge = k->color2; arms.emblem = k->emblem; }
    }
    gt = &sprites_.fortGateTex(*this, arms, fp);
  } else if (const Kingdom* k = g.world.kingdomOf(gs)) {
    const cult::Culture* kc = g.world.cultureOfKingdom(g.world.sites[(size_t)gs].kingdom);
    if (kc && !kc->heraldry.empty()) gt = &sprites_.heraldryTex(*this, 1, kc->heraldry, 7, ws);
    else if (ws) { cult::Heraldry h; h.field = k->color; h.charge = k->color2; h.emblem = k->emblem; gt = &sprites_.heraldryTex(*this, 1, h, 7, ws); }
    else gt = &sprites_.kingdomTex(*this, 1, *k, 7);
  } else if (ws) {
    cult::Heraldry h;
    gt = &sprites_.heraldryTex(*this, 1, h, 7, ws);
  }
  P.blit(*gt, d.tx * 16.0f - art::GATE_OX - cam.x, d.ty * 16.0f - art::GATE_OY - cam.y);
  // (M7c fixer r3, review: "the hero almost disappears inside a gatehouse passage": every city entry on foot
  // passes through this frame) a hero under the gatehouse's vault (sorted before it, inside its span) shows
  // through as the same clear silhouette a roof or a crown gives
  if (!g.inside && pl.p.x > d.tx * 16.0f - 4.0f && pl.p.x < d.tx * 16.0f + 52.0f && pl.p.y <= d.ty * 16.0f + 15.6f && pl.p.y > d.ty * 16.0f - 34.0f)
    ghost = true;
}

void View::drawActorSorted(Game& g, FrameCtx& F, const Drawable& d) {
  Pix& P = F.P;
  Vec2 cam = F.cam;
  auto& ghostTex = F.ghostTex;
  auto& ghostFr = F.ghostFr;
  auto& ghostRow = F.ghostRow;
  auto& ghostFlip = F.ghostFlip;
  auto& ghostX = F.ghostX;
  auto& ghostY = F.ghostY;
  auto& ghostSx = F.ghostSx;
  auto& ghostSy = F.ghostSy;
  auto& ghostW = F.ghostW;
  auto& ghostH = F.ghostH;
  const Actor& a = g.actors[d.idx];
#ifdef EMB_WITNESS
  if (!wHide.empty() && witnessHides(g, a)) return;   // (Witness reset param hide) no body, no silhouette through a roof
#endif
  if (a.player && g.homeRiding() && home_.drawMounted(*this, g, a, cam)) {   // (M7) the rider on the horse (home_view.cpp)
    if (rideGhost_.t) {   // hidden behind a roof or a crown: the rider's silhouette shows through (as on foot)
      ghostTex = rideGhost_.t; ghostFr = rideGhost_.fr; ghostRow = rideGhost_.row; ghostFlip = rideGhost_.flip;
      ghostSx = rideGhost_.sx; ghostSy = rideGhost_.sy; ghostW = rideGhost_.w; ghostH = rideGhost_.h;
      ghostX = rideGhost_.x; ghostY = rideGhost_.y;
    }
    return;
  }
  float flash = a.flash > 0 ? a.flash / 0.12f : 0;
  float alpha = a.st == AState::Dead ? clampf(1.0f - (a.stT - 4.0f) / 2.0f, 0, 1) : 1.0f;
  if (a.player && a.iframes > 0 && a.st != AState::Roll && ((int)(t_ * 20) & 1)) alpha *= 0.5f;
  // (P2b VIEW-B fix r3, RIG2-ROOF-OVER-YARD) rig 2: a villager whose feet are behind a building's sprite (it is drawn
  // after him) is hidden by it whole, not cut to a head over the ridge; he fades over 10 px of depth as he steps in
  // or out behind the roof's edge, so nothing pops
  if (!F.roofOcc->empty() && a.human && !a.player && !a.comp && a.st != AState::Dead) alpha *= behindRoofAlpha(*F.roofOcc, a);
  // (P2a slice) facing up, the Steel Arc of hits 1 and 2 sweeps beyond the body: drawn first, the body hides its middle
  const bool fxBehind = rvSwingFxBehind(g, a);
  if (fxBehind) drawSwingFx(g, a, cam);
  // (P2a LENS) a human's body blits are recorded: the night lift adds them again after the light map (a mounted
  // companion's rider records itself, not the horse: party_view.cpp)
  if (a.comp && party_.drawCompanionBody(*this, g, a, cam, alpha, flash)) {
    // (M8) the bonded hound, a companion on their horse (party_view.cpp)
  } else if (a.human) {
    drawHumanSorted(g, F, a, alpha, flash);
  } else if (a.critter > 0) {
    drawCritterSorted(F, a, alpha, flash);
  } else {
    if (drawMonsterSorted(g, F, a, alpha, flash)) return;   // (a sea creature drawn by the swell: nothing more)
  }
  // melee slash arc (P2a: rpg/view/render_anim.cpp; drawn before the body above when it sweeps beyond it)
  if (!fxBehind) drawSwingFx(g, a, cam);
  // attack telegraph
  drawTelegraph(F, a);
  // (M6 BEASTS) an elite's, a champion's or a named unique's name plate (drawn after the lighting, nearest first)
  bool plated = false;   // (its health shows on the plate)
  if (a.hostile && a.st != AState::Dead && a.st != AState::Down && beasts::marked(a)) {
    const float d2 = len2(a.p - g.pl().p);
    if (a.aggro || d2 < 90.0f * 90.0f) {
      const float top = a.human ? (float)rigFor(a.look).plateY + topLift(a) : art::lookCellH(beasts::lookOf(a)) * beasts::drawScale(a) + 2.0f;
      beasts::plates.push_back({d.idx, a.p.x, a.p.y - top - (a.fly ? 34.0f : a.flying ? 7.0f : 0.0f), d2, a.p.y});
      plated = true;
    }
  }
  // health bar for damaged enemies
  if (!plated && a.hostile && a.st != AState::Dead && a.st != AState::Down && a.hp < a.maxHp && !a.boss && a.rank != (uint8_t)foes::Rank::WorldBoss) {
    float lift = a.fly ? 34 : 0;
    float bw = 16, x = a.p.x - bw / 2 - cam.x, y = a.p.y - (a.human ? (float)rigFor(a.look).barY + topLift(a) : art::lookCellH(beasts::lookOf(a)) * beasts::drawScale(a) + 2.0f) - lift - cam.y;
    P.rect(x - 1, y - 1, bw + 2, 4, Color(0.05f, 0.03f, 0.05f, 0.8f));
    P.rect(x, y, bw * clampf(a.hp / a.maxHp, 0, 1), 2, Color(0.85f, 0.2f, 0.18f));
  }
}

void View::drawHumanSorted(Game& g, FrameCtx& F, const Actor& a, float alpha, float flash) {
  Pix& P = F.P;
  const Map& m = F.m;
  Vec2 cam = F.cam;
  auto& ghostTex = F.ghostTex;
  auto& ghostFr = F.ghostFr;
  auto& ghostRow = F.ghostRow;
  auto& ghostFlip = F.ghostFlip;
  auto& ghostX = F.ghostX;
  auto& ghostY = F.ghostY;
  auto& ghostRec = F.ghostRec;
  // (P2a) the human frame pick and body draw: rpg/view/render_anim.cpp
  HumanGhost hg;
  const size_t rec0 = lens_.liftRecs_.size();
  if (g.mode != Mode::Title) P.recordBlits(&lens_.liftRecs_);
  drawHumanBody(g, m, a, cam, alpha, flash, hg);
  P.recordBlits(nullptr);
  if (a.player && hg.t) { ghostTex = hg.t; ghostFr = hg.fr; ghostRow = hg.row; ghostFlip = hg.flip; ghostX = hg.x; ghostY = hg.y; }
  if (a.player && !hg.t && heroSprite.on && heroSprite.rig >= 2) {
    // rig 2 leaves HumanGhost empty (render_anim.cpp): take the body's own blit (the one at the frame's rect)
    const HeroSprite& hs = heroSprite;
    for (size_t i = rec0; i < lens_.liftRecs_.size(); i++) {
      const Pix::BlitRec& r = lens_.liftRecs_[i];
      // (P2b VIEW-B) the frame's rows may be cut (a rower at the thwart's line): its width and top-left decide
      if (r.sw == hs.w && r.sh <= hs.h && r.sh > 0 && std::fabs(r.dx - hs.x) < 0.5f && std::fabs(r.dy - hs.y) < 0.5f) {
        ghostRec = r; ghostTex = nullptr; break;
      }
    }
  }
}

void View::drawCritterSorted(FrameCtx& F, const Actor& a, float alpha, float flash) {
  Pix& P = F.P;
  Vec2 cam = F.cam;
  // (M5) a village animal (art::critterSheet): columns 0 idle, 1-4 walk, 5-6 its own action, 7 lying down; rows
  // down / up / right (left flipped). Standing about it now and then does its thing (pecks, grooms, wags), each
  // on its own clock; a posture from the sim (Sleep: lying down; any other: the action) wins.
  const int ck = a.critter - 1;
  const Tex& t = sprites_.critterTex(*this, ck, a.critterVar);
  const int cw = art::critterCellW((art::Critter)ck), chh = art::critterCellH((art::Critter)ck);
  const int row = a.face == 0 ? 0 : a.face == 1 ? 1 : 2;
  const bool flip = a.face == 3;
  int fr = 0;
  const float own = (float)(hash32((uint32_t)a.id * 0x9E3779B1u) % 1000u) * 0.001f;
  if (a.st == AState::Walk) fr = 1 + (int)(a.animT * 10) % 4;
  else if (a.posture == art::Posture::Sleep) fr = 7;
  else if (a.posture != art::Posture::None) fr = 5 + (int)((a.postureT > 0 ? a.postureT : t_) * 3 + own * 7) % 2;
  else {
    const float ph = std::fmod(t_ * 0.21f + own * 3.7f, 1.0f);
    if (ph < 0.32f) fr = 5 + (int)(t_ * (ck == (int)art::Critter::Cat ? 2.5f : 5.0f) + own * 9) % 2;
  }
  const float x = std::floor(a.p.x - cw / 2.0f - cam.x + 0.5f), y = std::floor(a.p.y - chh + 2 - cam.y + 0.5f);
  P.blitEx(t, fr * cw, row * chh, cw, chh, x, y, (float)cw, (float)chh, flip, Color(1, 1, 1, alpha));
  if (flash > 0) P.blitEx(t, fr * cw, row * chh, cw, chh, x, y, (float)cw, (float)chh, flip, Color(1, 1, 1, flash), 1);
  life_.m5Count_.critters++;
}

bool View::drawMonsterSorted(Game& g, FrameCtx& F, const Actor& a, float alpha, float flash) {
  Pix& P = F.P;
  const Map& m = F.m;
  Vec2 cam = F.cam;
  bool& ghost = F.ghost;
  const Tex& t = sprites_.monsterTex(*this, a);   // (M6: the variant look)
  const int bsc = beasts::drawScale(a);   // (always 1: one pixel grid)
  int cw = art::lookCellW(beasts::lookOf(a)) * bsc, chh = art::lookCellH(beasts::lookOf(a)) * bsc;
  int fr = 0;
  switch (a.st) {
    case AState::Walk: fr = (int)(a.animT * (a.mon == Monster::Bat ? 14 : 8)) % 4; break;
    case AState::Windup: fr = 4; break;
    case AState::Strike: fr = 5; break;
    case AState::Hurt: fr = 6; break;
    case AState::Dead: case AState::Down: fr = 7; break;
    default: fr = (int)(a.animT * 3) % 2; break;
  }
  if (a.fly && a.mon == Monster::Dragon) fr = (int)(a.animT * 6) % 4;
  bool flip = a.aim.x < 0;
  float lift = a.fly ? 34 + std::sin(a.animT * 3) * 3 : (a.flying && a.st != AState::Dead ? 6 + std::sin(a.animT * 6) * 2 : 0);
  float x = a.p.x - cw / 2.0f - cam.x, y = a.p.y - chh + 2 * bsc - lift - cam.y;
  // collapsed bones rattle harder as they are about to stand back up
  if (a.st == AState::Down && a.stT > 1.2f) x += std::sin(t_ * 60) * std::min(2.0f, (a.stT - 1.2f) * 1.5f);
  // a heavy windup rears back; a wolf crouches before the lunge
  if (a.st == AState::Windup && a.heavy) y -= std::min(3.0f, a.stT * 6);
  if (a.st == AState::Windup && a.lunge) y += 1;
  // (M3c) a lurker that hasn't seen you lies low in the shallows or the mud: only its back, eyes and snout show,
  // (M6 fixer r4) on whole pixels, like the critters: the body, its x-ray ghost, its rim and glow sheets (all
  // floored) then land on the same pixels instead of a pixel apart (a doubled, smeared beast)
  x = std::floor(x + 0.5f); y = std::floor(y + 0.5f);
  // (M10) a sea creature on the water shows only what breaks the swell (render_sea.cpp)
  if (a.swim && seaDrawSwimmer(*this, g, a, t, fr, cw, chh, flip, alpha, flash, cam)) return true;
  // with a ripple round it. (M6 fixer r4) Never a world boss: the land's headline beast shows its whole body.
  int srcH = chh;
  if (a.mon == Monster::Lurker && !a.aggro && !beasts::isRank(a, foes::Rank::WorldBoss) && a.st != AState::Dead && (a.st == AState::Idle || a.st == AState::Walk) && m.kind == MapKind::Overworld) {
    const int ltx = (int)std::floor(a.p.x / 16), lty = (int)std::floor((a.p.y - 2) / 16);
    bool wet = m.at(ltx, lty) == Ground::Swamp;
    for (int k = 0; k < 4 && !wet; k++) wet = groundWater(m.at(ltx + (k == 0) - (k == 1), lty + (k == 2) - (k == 3)));
    if (wet) {
      srcH = chh - 6;
      y += 6;
      const float rp = 0.5f + 0.5f * std::sin(t_ * 2.2f + a.id);
      P.rect(x + 4, y + srcH - 1, (float)cw - 8, 1, Color(0.75f, 0.85f, 0.85f, 0.35f + 0.2f * rp));
      P.rect(x + 8 - rp * 2, y + srcH + 1, (float)cw - 16 + rp * 4, 1, Color(0.75f, 0.85f, 0.85f, 0.18f));
    }
  }
  // (M6 fixer, review: "the named unique is an unreadable dark-green blob in its lair") a named unique wears a
  // crisp gold rim (its silhouette one pixel round it, pulsing softly): it reads against any undergrowth and marks
  // it as the land's named beast before its plate is read
  if (beasts::isRank(a, foes::Rank::Named) && a.st != AState::Dead) {
    const art::MonsterLook SL = beasts::lookOf(a);
    const uint64_t sk = (SL.key() ^ 0x5A11E77E5A11E77Eull) | (1ull << 61);
    auto si = sprites_.monsterLooks_.find(sk);
    if (si == sprites_.monsterLooks_.end()) {
      Canvas sil = art::monsterSheetLook(SL);
      for (int yy = 0; yy < sil.h; yy++)
        for (int xx = 0; xx < sil.w; xx++) sil.set(xx, yy, (sil.get(xx, yy) >> 24) ? rgba(255, 255, 255) : 0u);
      si = sprites_.monsterLooks_.emplace(sk, pix_->bake(sil)).first;
    }
    const float pu = 0.75f + 0.25f * std::sin(t_ * 3.0f + (float)a.id);
    const Color rc(1.0f, 0.80f, 0.32f, alpha * pu);
    for (int d = 0; d < 4; d++)
      P.blitEx(si->second, fr * cw, 0, cw, srcH, x + (float)((d == 0) - (d == 1)), y + (float)((d == 2) - (d == 3)), (float)cw, (float)srcH, flip, rc);
  }
  // (M6 fixer r3, review: "the world boss is nearly invisible at night") a world boss carries its own menace
  // light: an ember-red rim round its silhouette and a faint lift of its body, both on the after-lighting list
  // (additive, scaled by how dark the night is), so the biggest beast in the land reads as a threat in the dark
  if (beasts::isRank(a, foes::Rank::WorldBoss) && a.st != AState::Dead && alpha > 0.05f) {
    const float nightK = g.inside ? 0.7f : clampf(1.0f - g.daylight() * 1.4f, 0, 1);
    if (nightK > 0.02f) {
      const art::MonsterLook SL = beasts::lookOf(a);
      const uint64_t sk = (SL.key() ^ 0x0B055B055B055B05ull) | (1ull << 60);
      auto si = sprites_.monsterLooks_.find(sk);
      if (si == sprites_.monsterLooks_.end()) {
        Canvas sh = art::monsterSheetLook(SL);
        // the rim: the clear pixels touching the beast (never its own body), within its frame
        const int cwf = art::lookCellW(beasts::lookOf(a)) * bsc;
        Canvas rim(sh.w, sh.h);
        auto op = [&](int xx, int yy) { return xx >= 0 && yy >= 0 && xx < sh.w && yy < sh.h && (sh.get(xx, yy) >> 24) != 0; };
        for (int yy = 0; yy < sh.h; yy++)
          for (int xx = 0; xx < sh.w; xx++) {
            if (op(xx, yy)) continue;
            const int f0 = xx / cwf * cwf;
            auto opf = [&](int qx, int qy) { return qx >= f0 && qx < f0 + cwf && op(qx, qy); };
            if (opf(xx - 1, yy) || opf(xx + 1, yy) || opf(xx, yy - 1) || opf(xx, yy + 1)) rim.set(xx, yy, rgba(255, 96, 52));
          }
        si = sprites_.monsterLooks_.emplace(sk, pix_->bake(rim)).first;
      }
      const float pu = 0.8f + 0.2f * std::sin(t_ * 2.2f + (float)a.id);
      beasts::glows.push_back({&si->second, fr * cw, cw, srcH, std::floor(x + 0.5f), std::floor(y + 0.5f), flip, alpha * nightK * pu * 0.9f});
      beasts::glows.push_back({&t, fr * cw, cw, srcH, std::floor(x + 0.5f), std::floor(y + 0.5f), flip, alpha * nightK * 0.32f});
    }
  }
  P.blitEx(t, fr * cw, 0, cw, srcH, x, y, (float)cw, (float)srcH, flip, Color(1, a.slowT > 0 ? 0.85f : 1, a.slowT > 0 ? 1 : 1, alpha));
  if (a.st != AState::Dead && alpha > 0.5f && !g.inside &&
      (a.flying || a.fly || (a.rank >= (uint8_t)foes::Rank::Champion && a.rank <= (uint8_t)foes::Rank::WorldBoss)))
    beasts::xray.push_back({&t, fr * cw, cw, srcH, std::floor(x + 0.5f), std::floor(y + 0.5f), flip, 1.0f});
  if (a.mon == Monster::Wisp && a.st != AState::Dead)   // light: an additive pass makes it glow on any ground
    P.blitEx(t, fr * cw, 0, cw, chh, x, y, (float)cw, (float)chh, flip, Color(0.6f, 1.0f, 1.0f, 0.55f + 0.25f * std::sin(t_ * 11 + a.id)), 1);
  if (a.slowT > 0) P.blitEx(t, fr * cw, 0, cw, chh, x, y, (float)cw, (float)chh, flip, Color(0.2f, 0.4f, 0.7f, 0.5f), 1);
  if (flash > 0) P.blitEx(t, fr * cw, 0, cw, chh, x, y, (float)cw, (float)chh, flip, Color(1, 1, 1, flash), 1);
  if (a.burnT > 0 && ((int)(t_ * 10) & 1)) P.rectAdd(x + cw * 0.3f, y + chh * 0.3f, cw * 0.4f, chh * 0.5f, Color(0.6f, 0.25f, 0.05f, 0.5f));
  // (M6 BEASTS) a boss passing a phase (66 %, 33 %) flashes white-hot for a moment (update() shakes the screen
  // and plays the roar); the glowing parts of a variant look (eyes, ember cracks, crystal glints, a golem's
  // rune) go on the after-lighting list so they burn through the night
  {
    auto mi = beasts::memo.find(a.id);
    if (mi != beasts::memo.end() && mi->second.flashT > 0) {
      const float k = clampf(mi->second.flashT / 0.45f, 0, 1) * (((int)(t_ * 24) & 1) ? 1.0f : 0.6f);
      P.blitEx(t, fr * cw, 0, cw, chh, x, y, (float)cw, (float)chh, flip, Color(1, 0.95f, 0.8f, k), 1);
    }
    const art::MonsterLook L = beasts::lookOf(a);
    if (art::monsterLookGlows(L) && alpha > 0.05f) {
      const uint64_t gk = L.key() | (1ull << 63);
      auto gi = sprites_.monsterLooks_.find(gk);
      if (gi == sprites_.monsterLooks_.end()) {
        Canvas gs = art::monsterGlowLook(L);
        gi = sprites_.monsterLooks_.emplace(gk, pix_->bake(gs)).first;
      }
      beasts::glows.push_back({&gi->second, fr * cw, cw, srcH, std::floor(x + 0.5f), std::floor(y + 0.5f), flip, alpha});
    }
  }
  return false;
}

void View::drawTelegraph(FrameCtx& F, const Actor& a) {
  Pix& P = F.P;
  Vec2 cam = F.cam;
  if (a.hostile && a.st == AState::Windup && a.mon != Monster::Dragon) {
    float lift = a.human ? (float)rigFor(a.look).markerY + topLift(a) : art::lookCellH(beasts::lookOf(a)) * beasts::drawScale(a) + 6.0f;   // (P2b VIEW-B: over the doll frame's top)
    float pulse = 0.6f + 0.4f * std::sin(t_ * 30);
    if (a.heavy) {
      // heavy slam: a ground ring that fills in as the blow comes, and a double "!!" - roll out (or through)
      // (the sim's heavyWindup, monster_stats.h)
      const float wu = gsim::heavyWindup(a.mon);
      float k = clampf(a.stT / wu, 0, 1);
      Vec2 c = a.p + a.aim * 10.0f;
      float r = 26.0f + a.radius;
      int seg = 44;
      for (int i = 0; i < seg; i++) {   // the danger zone: a solid outline...
        float an = i * TAU / seg;
        float px = c.x + std::cos(an) * r - cam.x, py = c.y + std::sin(an) * r * 0.55f - cam.y;
        P.rect(px - 1, py - 1, 2, 2, Color(1, 0.22f, 0.12f, 0.55f + 0.45f * k));
      }
      for (float ir = 4; ir < r * k; ir += 4) {   // ...that fills from the middle as the blow comes
        int n = std::max(8, (int)(ir * 1.2f));
        for (int i = 0; i < n; i++) {
          float an = i * TAU / n;
          P.rect(c.x + std::cos(an) * ir - cam.x, c.y + std::sin(an) * ir * 0.55f - cam.y, 1, 1, Color(1, 0.35f, 0.2f, 0.25f + 0.3f * k));
        }
      }
      Color rc(1, 0.2f + 0.3f * k, 0.15f, pulse);
      P.rect(a.p.x - 4 - cam.x, a.p.y - lift - 2 - cam.y, 2, 6, rc);
      P.rect(a.p.x - 4 - cam.x, a.p.y - lift + 5 - cam.y, 2, 2, rc);
      P.rect(a.p.x + 2 - cam.x, a.p.y - lift - 2 - cam.y, 2, 6, rc);
      P.rect(a.p.x + 2 - cam.x, a.p.y - lift + 5 - cam.y, 2, 2, rc);
    } else {
      Color tc = a.lunge ? Color(1, 0.65f, 0.2f, pulse) : Color(1, 0.3f, 0.2f, pulse);
      P.rect(a.p.x - 1 - cam.x, a.p.y - lift - cam.y, 2, 5, tc);
      P.rect(a.p.x - 1 - cam.x, a.p.y - lift + 6 - cam.y, 2, 2, tc);
      if (a.lunge) {   // a short streak showing where the lunge will go
        for (int i = 1; i <= 4; i++) {
          Vec2 q = a.p + Vec2(0, -3) + a.aim * (6.0f + i * 5.0f);
          P.rect(q.x - cam.x, q.y - cam.y, 1, 1, Color(1, 0.65f, 0.2f, 0.5f - i * 0.08f));
        }
      }
    }
  }
}

void View::drawProjSorted(Game& g, FrameCtx& F, const Drawable& d) {
  Pix& P = F.P;
  Vec2 cam = F.cam;
  const Projectile& pr = g.projs[d.idx];
  Vec2 dir = norm(pr.v);
  float x = pr.p.x - cam.x, y = pr.p.y - cam.y;
  switch (pr.kind) {
    case ProjKind::Arrow:
      for (int i = 0; i < 8; i++) P.rect(x - dir.x * i, y - dir.y * i, 1, 1, i < 2 ? Color(0.85f, 0.85f, 0.9f) : (i > 5 ? Color(0.95f, 0.95f, 0.95f) : Color(0.55f, 0.38f, 0.2f)));
      break;
    case ProjKind::Fireball: case ProjKind::DragonFire: {
      if (pr.spell) { MagicLane::drawSpellProj(*this, g, pr, d.idx, cam); break; }   // (M9) a classic FIREBOLT: the magic draws it (render_magic.cpp)
      const Tex& t = sprites_.fx_[(int)art::Fx::Fireball];
      int fw = art::fxW(art::Fx::Fireball), fh = art::fxH(art::Fx::Fireball), frames = std::max(1, art::fxFrames(art::Fx::Fireball));
      int fr = (int)(t_ * 14 + d.idx) % frames;
      float sc = pr.kind == ProjKind::DragonFire ? 1.5f : 1.0f;
      P.blitEx(t, fr * fw, 0, fw, fh, x - fw * sc / 2, y - fh * sc / 2, fw * sc, fh * sc, dir.x < 0);
      Rng r((uint32_t)(t_ * 60) + d.idx);
      Particle q; q.p = pr.p; q.v = Vec2(r.range(-10, 10), r.range(-10, 10)); q.life = q.max = 0.3f; q.c = Color(1, r.range(0.3f, 0.7f), 0.1f); q.size = 1; fx_.parts_.push_back(q);
      break;
    }
    case ProjKind::IceSpike: {
      if (pr.spell) { MagicLane::drawSpellProj(*this, g, pr, d.idx, cam); break; }   // (M9) a classic FROST LANCE: the magic draws it
      const Tex& t = sprites_.fx_[(int)art::Fx::Frost];
      int fw = art::fxW(art::Fx::Frost), fh = art::fxH(art::Fx::Frost);
      P.blitEx(t, 0, 0, fw, fh, x - fw / 2.0f, y - fh / 2.0f, (float)fw, (float)fh, dir.x < 0);
      break;
    }
    case ProjKind::Spit:
      if (pr.ench == Ench::Frost && !pr.fromPlayer) {   // (M3c) a blightspawn's spore clot: a soft sickly puff
        const float a = clampf(pr.life / 0.42f, 0, 1);
        P.rect(x - 3, y - 2, 6, 4, Color(0.48f, 0.36f, 0.50f, 0.35f * a));
        P.rect(x - 2, y - 3, 4, 6, Color(0.48f, 0.36f, 0.50f, 0.35f * a));
        P.rect(x - 1, y - 1, 2, 2, Color(0.72f, 0.86f, 0.38f, 0.8f * a));
        break;
      }
      P.rect(x - 2, y - 2, 4, 4, Color(0.55f, 0.85f, 0.3f));
      P.rect(x - 1, y - 1, 2, 2, Color(0.8f, 1, 0.6f));
      break;
    case ProjKind::Spell: MagicLane::drawSpellProj(*this, g, pr, d.idx, cam); break;   // (M9) render_magic.cpp
    case ProjKind::Magic:
      if (pr.ench == Ench::None && !pr.fromPlayer) {   // (M3c) a wisp's bolt of light: cyan, with a short trail
        for (int i = 1; i <= 3; i++) P.rectAdd(x - dir.x * i * 2 - 1, y - dir.y * i * 2 - 1, 2, 2, Color(0.4f, 0.9f, 0.9f, 0.5f - i * 0.12f));
        P.rectAdd(x - 3, y - 3, 6, 6, Color(0.3f, 0.8f, 0.8f, 0.7f));
        P.rect(x - 1, y - 1, 2, 2, Color(0.9f, 1, 1));
        break;
      }
      P.rectAdd(x - 3, y - 3, 6, 6, Color(0.4f, 0.3f, 0.9f, 0.7f));
      P.rect(x - 1, y - 1, 2, 2, Color(0.85f, 0.8f, 1));
      break;
  }
}

void View::drawHandcartSorted(Game& g, FrameCtx& F, const Drawable& d) {
  Pix& P = F.P;
  Vec2 cam = F.cam;
  if (d.idx >= 0 && d.idx < (int)g.story.walkers_.size()) {
    const story::Engine::Walker& w = g.story.walkers_[(size_t)d.idx];
    const int dir = handcartDir(w.cartAng);
    // the load the story names (var cartload 1..4: sacks, crates, barrels, hay), else one by the instance
    int load = (int)(hash32(w.inst * 2654435761u) % 4u);
    if (const story::Instance* li = g.story.find(w.inst)) {
      auto lv = li->vars.find("cartload");
      if (lv != li->vars.end() && lv->second >= 1 && lv->second <= 4) load = lv->second - 1;
    }
    const int turn = ((int)std::floor(w.wheel * 4.0f / 0.7854f) % 4 + 4) % 4;
    const Tex& t = sprites_.cachedTex(*this, 0x52ull << 56 | (uint64_t)dir | (uint64_t)load << 2 | (uint64_t)turn << 4, paintHandcart);
    P.blit(t, std::floor(w.cartX - HC_OX - cam.x + 0.5f), std::floor(w.cartY - HC_OY - cam.y + 0.5f));
  }
}

void View::drawHiddenHero(Game& g, FrameCtx& F) {
  Pix& P = F.P;
  Vec2 cam = F.cam;
  bool& ghost = F.ghost;
  auto& ghostTex = F.ghostTex;
  auto& ghostFr = F.ghostFr;
  auto& ghostRow = F.ghostRow;
  auto& ghostFlip = F.ghostFlip;
  auto& ghostX = F.ghostX;
  auto& ghostY = F.ghostY;
  auto& ghostSx = F.ghostSx;
  auto& ghostSy = F.ghostSy;
  auto& ghostW = F.ghostW;
  auto& ghostH = F.ghostH;
  auto& ghostRec = F.ghostRec;
  // the hidden hero: a cool, see-through silhouette over whatever hides it, so it reads as "behind" the roof or crown
  // rather than standing on it (a faint additive rim keeps it visible on dark slate as well as on pale thatch)
  if (ghost && ghostTex && g.mode != Mode::Title) {
    const float W = (float)art::HUMAN_W, H = (float)art::HUMAN_H;
    const int sx = ghostW > 0 ? ghostSx : ghostFr * art::HUMAN_W, sy = ghostW > 0 ? ghostSy : ghostRow * art::HUMAN_H;
    const int sw = ghostW > 0 ? ghostW : art::HUMAN_W, sh = ghostW > 0 ? ghostH : art::HUMAN_H;
    (void)W; (void)H;
    P.blitEx(*ghostTex, sx, sy, sw, sh, ghostX, ghostY, (float)sw, (float)sh, ghostFlip, Color(0.42f, 0.48f, 0.72f, 0.55f));
    P.blitEx(*ghostTex, sx, sy, sw, sh, ghostX, ghostY, (float)sw, (float)sh, ghostFlip, Color(0.18f, 0.22f, 0.36f, 0.5f), 1);
  } else if (ghost && ghostRec.t && g.mode != Mode::Title) {   // (P2a) the v2 hero's frame, the same two passes
    // (P2b HOLDERS-B) aboard at rig 2 the floor lies inside the hull: the silhouette (a sail before him) stops at the
    // gunwale / rail, so his legs never show through the hull's side
    if (g.sea.atSea() && art::rigVersion() >= 2 && ghostRec.dh > 0) {
      const int hull = (int)g.sea.boats[(size_t)g.sea.aboard].hull;
      const float lim = std::floor(g.pl().p.y - cam.y) - seaHeroLift(*this, g) - (float)art::holderPx(hull <= 0 ? art::Holder::RowboatGunwale : art::Holder::DeckRail);
      const float s = ghostRec.sh > 0 ? ghostRec.dh / (float)ghostRec.sh : 1.0f;   // device rows per source row
      const float keep = std::clamp(lim - ghostRec.dy, 0.0f, ghostRec.dh);
      ghostRec.sh = (int)std::floor(keep / s + 0.001f);
      ghostRec.dh = ghostRec.sh * s;
    }
    if (ghostRec.sh > 0) {
      P.blitRec(ghostRec, Color(0.42f, 0.48f, 0.72f, 0.55f), 0);
      P.blitRec(ghostRec, Color(0.18f, 0.22f, 0.36f, 0.5f), 1);
    }
  }
}

void View::drawSortedPass(Game& g, const Map& m, Vec2 cam) {
  Pix& P = *pix_;
  // collect drawables
  std::vector<Drawable> list;
  // (P2b VIEW-B fix r3, RIG2-ROOF-OVER-YARD) the buildings in view for the behind-a-roof test (collectBuildings)
  static std::vector<RoofOcc> roofOcc;
  FrameCtx F{P, m, cam, g.pl(), &roofOcc,
             art::rigVersion() >= 2 && m.kind == MapKind::Overworld && g.mode != Mode::Title && !g.inside};
  collectTiles(g, F, list);
  roofOcc.clear();
  collectBuildings(g, F, list);
  collectActors(g, F, list);
  collectOthers(g, F, list);
  std::sort(list.begin(), list.end(), [](const Drawable& a, const Drawable& b) { return a.y < b.y; });

  // shadows first (under everything standing)
  for (const Drawable& d : list) drawShadowSorted(g, F, d);
  // (M6 fixer r4, review: "the world boss on the overland is hidden under the jungle canopy") the ranked beasts on screen
  // (elites up to world bosses): a tree crown in front of one thins out round it as it does round the hero, so the
  // land's headline foes read under forest and jungle canopy; their feet (x, y) and half-width / body height
  if (m.kind == MapKind::Overworld && g.mode != Mode::Title)
    for (const Actor& a : g.actors) {
      if (a.human || a.st == AState::Dead || a.rank < (uint8_t)foes::Rank::Elite || a.rank > (uint8_t)foes::Rank::WorldBoss) continue;
      if (a.p.x < cam.x - 80 || a.p.x > cam.x + Pix::W + 80 || a.p.y < cam.y - 40 || a.p.y > cam.y + Pix::H + 120) continue;
      F.reveals.push_back({a.p.x, a.p.y, art::lookCellW(beasts::lookOf(a)) * 0.35f, (float)art::lookCellH(beasts::lookOf(a))});
    }
  // (P2a LENS fix r3) every plain blit of the sorted pass is recorded (g_occRecs), and for each figure blit how many of
  // those came before it: the night lift then lifts only what shows (drawNightLift)
  F.recOcc = g.mode != Mode::Title && !g.inside;
  for (const Drawable& d : list) {
    P.recordOthers(F.recOcc ? &g_occRecs : nullptr);
    switch (d.kind) {
      case DrawKind::Prop: drawPropSorted(g, F, d); break;
      case DrawKind::Building: drawBuildingSorted(g, F, d); break;
      case DrawKind::Wall: drawWallSorted(F, d); break;
      case DrawKind::Gate: drawGateSorted(g, F, d); break;
      case DrawKind::Actor: drawActorSorted(g, F, d); break;
      case DrawKind::Pickup: {
        const Pickup& k = g.pickups[d.idx];
        float bob = std::sin(t_ * 4 + d.idx) * 1.5f;
        loot_.drawLootGround(*this, g, k, d.idx, cam, bob);   // (M8b) its rarity's glint, beam or pillar (loot_ui.cpp)
        break;
      }
      case DrawKind::Projectile: drawProjSorted(g, F, d); break;
      case DrawKind::FestPole:   // (M5) a festival pole
        if (d.idx >= 0 && d.idx < (int)life_.festSpans_.size()) life_.drawFestPole(*this, life_.festSpans_[(size_t)d.idx], d.tx != 0, cam);
        break;
      case DrawKind::Home:   // (M7) a home draw (its ground shadow went down in the shadow pass)
        if (d.idx >= 0 && d.idx < (int)home_.homeDraws_.size()) home_.drawHomeDraw(*this, home_.homeDraws_[(size_t)d.idx], cam);
        break;
      case DrawKind::Grave:   // (M8) a fallen companion's grave
        party_.drawGrave(*this, g, d.idx, cam, false);
        break;
      case DrawKind::Sea:  // (M10) a boat (render_sea.cpp)
        SeaLane::drawSeaSorted(*this, g, d.idx, cam);
        if (seaHeroUnderRig(*this)) F.ghost = true;   // the sail stands before the hero: his silhouette shows through it
        break;
      case DrawKind::Rise:  // (M12) the rise's (rise_world.cpp)
        RiseLane::drawRiseSorted(*this, g, d.idx, cam);
        break;
      case DrawKind::Relief:  // (M11) the relief's (render_relief.cpp)
        ReliefLane::drawReliefSorted(*this, g, d.idx, cam);
        break;
      case DrawKind::Handcart: drawHandcartSorted(g, F, d); break;
      default: break;
    }
  }
  P.recordOthers(nullptr);
  drawHiddenHero(g, F);
}
