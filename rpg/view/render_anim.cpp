// Human animation in the world (P2a phase A, VISION_PLAN 15.36 item 7): the human frame pick, the body draw, the melee
// swing effect and the dead's offset, moved out of render.cpp's sorted pass. Pixel-identical to the pre-P2a draw at rig 1
// (the frame pick is rpg/sim/anim.cpp's animPick, which reproduces render.cpp's old switch exactly: rpg_test --anim).
// The ANIM lane owns this file from here: at rig 2, where art::rig2Covers(look), it draws the v2 atlas frame by its pivot
// (View::rigTex; never flipped) and the family effects (art::Fx::SwingArc...) timed by the family's pace.
#include <algorithm>
#include <chrono>
#include <cmath>
#include "rpg/art/art_holders2.h"
#include "rpg/art/art_sea.h"
#include "rpg/sim/anim.h"
#include "rpg/sim/gear_look.h"
#include "rpg/story/story_internal.h"
#include "rpg/view/rise_view.h"
#include "rpg/view/view.h"

// render.cpp's helpers this draw shares (defined there, beside the rest of the sorted pass)
float seaHeroLift(View& v, const Game& g);
float seaOarPhase(View& v, const Game& g);                                        // (P2b) the rowboat's stroke, [0, 2)
bool rvBathing(const Game& g, const Map& m, const Actor& a);                      // a townsperson in a bath's pool
art::Berth rvBerthAt(const Map& m, int tx, int ty, int& kit);                     // the berth a bed prop is
Canvas rvFloorCushion(uint64_t key);                                              // the cushion under a floor sitter
void rvPushGlow(const Tex* t, int sx, int sw, int sh, float x, float y, bool flip, float a);   // the after-lighting list

namespace {
// the weapon art id that swings as a family (the `pose` hook's family on a look of another)
uint8_t weaponOfFamily(art::Family f) {
  switch (f) {
    case art::Family::Dagger: return 5;
    case art::Family::Sword: return 1;
    case art::Family::Greatsword: return art::WEAPON_GREATSWORD;
    case art::Family::Axe: return 2;
    case art::Family::Mace: return 6;
    case art::Family::Spear: return 8;
    case art::Family::Staff: return 4;
    case art::Family::Bow: return 3;
    default: return 0;
  }
}
art::RigPart partOf(art::Clip c) {
  switch (c) {
    case art::Clip::Idle: case art::Clip::Walk: case art::Clip::Hurt: return art::RigPart::Loco;
    case art::Clip::Fall: case art::Clip::Lying: return art::RigPart::Death;
    default: return art::RigPart::Action;
  }
}
// the family effects painted facing right that turn with the swing (the ring, the star and the twang do not)
bool fxTurns(art::Fx f) {
  return f == art::Fx::SwingArc || f == art::Fx::HeavyArc || f == art::Fx::ChopArc || f == art::Fx::StabGlint ||
         f == art::Fx::ThrustStreak || f == art::Fx::BluntSwish;
}
// (an effect's frames turned a quarter: rvSizedFx below, an exact pixel transpose, never a resampled rotation)

// (P2a style slice) the Steel Arc's frames for a v2 draw. key: fx id << 8 | rot (0 as painted, facing right; 1 turned
// clockwise = facing down; 3 counter-clockwise = facing up) | back << 2 (the source mirrored top to bottom first: the
// backhand's crescent sweeps the other way) | cell << 16 (painted natively in cells of that size: art::fxSpriteSized, so
// the finisher's 1.2x keeps one pixel size). Exact pixel transposes and mirrors, never a resampled rotation.
Canvas rvArcCells(uint64_t key) {
  const art::Fx f = (art::Fx)((key >> 8) & 0xFF);
  const int rot = (int)(key & 3), sz = (int)((key >> 16) & 0xFF);
  const bool back = ((key >> 2) & 1) != 0;
  Canvas s = art::fxSpriteSized(f, sz, sz);
  if (s.w <= 0) return Canvas(1, 1);
  const int n = std::max(1, art::fxFrames(f));
  Canvas o(n * sz, sz);
  for (int k = 0; k < n; k++)
    for (int y = 0; y < sz; y++)
      for (int x = 0; x < sz; x++) {
        const uint32_t c = s.get(k * sz + x, back ? sz - 1 - y : y);
        if (!c) continue;
        const int nx = rot == 1 ? sz - 1 - y : rot == 3 ? y : x, ny = rot == 1 ? x : rot == 3 ? sz - 1 - x : y;
        o.set(k * sz + nx, ny, c);
      }
  return o;
}

// (P2a `pose`) a frozen clip's v1 sheet column at rig 1 (the v1 sheet has one column per clip but the walk and roll)
int v1ColumnOf(art::Clip c, int frame) {
  switch (c) {
    case art::Clip::Walk: case art::Clip::Roll: return 1 + ((frame % 4) + 4) % 4;
    case art::Clip::Windup: case art::Clip::Cast: case art::Clip::Draw: return 5;
    case art::Clip::Strike: case art::Clip::Follow: case art::Clip::Release: case art::Clip::Block: return 6;
    case art::Clip::Hurt: case art::Clip::Fall: case art::Clip::Lying: return 7;
    default: return 0;
  }
}
}  // namespace

void View::drawHumanBody(Game& g, const Map& m, const Actor& a, Vec2 cam, float alpha, float flash, HumanGhost& ghost) {
  Pix& P = *pix_;
  if (a.player) { heroSprite.on = false; heroSprite.part = -1; }
  if (a.player && p2a.hidePlayer) return;   // (P2a `hideplayer`) the shot pair's second shot: no body, no silhouette
  const bool child = a.role == Role::Child;   // (M5) a shorter body (childBody)
  // (M12) the enlisted hero wears the sworn realm's tabard and the rank's mark (render_rise.cpp)
  art::HumanLook heroLook;
  if (a.player && g.service.enlisted()) { heroLook = a.look; RiseViewOps::dressHero(g, heroLook); }
  const art::HumanLook& look = a.player && g.service.enlisted() ? heroLook : a.look;
  const Tex* tp = &humanTex(look, child);
  int row = a.face == 0 ? 0 : a.face == 1 ? 1 : 2;
  bool flip = a.face == 3;
  // the frame: rpg/sim/anim.cpp (at rig 1 the v1 column render.cpp picked before P2a); `pose` freezes the hero's
  int fr = animPick(g, a, 1).frame;
  if (a.player && p2a.poseOn) fr = v1ColumnOf(p2a.poseClip, p2a.poseFrame);
  float bob = a.st == AState::Roll ? 3.0f : 0;
  if (a.player && g.sea.atSea()) bob -= seaHeroLift(*this, g);   // (M10) standing on the deck, riding its bob
  // (M5) what the body is doing (art::Posture): its pose sheet, frames looped at the pose's pace (each actor a
  // little out of step with the next, so a tavern does not lift its mugs as one), sunk onto the seat by dy
  // (seated, begging), laid over the bed (one row: the bed's way); combat states keep the standing sheet
  Vec2 at = a.p;   // where the figure's feet are drawn (a seat's fit moves it onto the seat)
  bool drawn = false;
  bool floorSeat = false;   // (fixer M5 r3) sat on the bare floor: a cushion goes under them
  // (M8) a downed companion lies in Posture::Downed, the player binding them kneels (party_view.cpp partyPosture)
  const art::Posture pzA = (a.comp || a.player) ? partyPosture(g, a) : a.posture;
  const bool posing = pzA != art::Posture::None && (a.st == AState::Idle || a.st == AState::Walk || ((a.st == AState::Down || a.st == AState::Dead) && pzA == art::Posture::Downed));
  // (P2b B11) the hero's fall under the death screen runs on the sim's clock (Game::update steps the body's state time
  // in Mode::Dead every step, drawn or not); the draw keeps no state of its own, so nothing leaks across resets
  // ---- (P2a ANIM) rig 2: a look the v2 painters cover is drawn from its atlas frame, the pivot on the feet, never
  // flipped (STYLE_P2 9, 10). A posture draws from its v2 posture atlas (POSES: humanPosture2, seated by seatFit2, the
  // sleeper by sleeper2 / bedFit2); a frame not painted or not baked yet falls back to v1 whole.
  const bool v2Look = art::rigVersion() >= 2 && art::rig2Covers(look);
  // (P2a fix H2-BOAT-MEASURE, STYLE_P2 16 "a rower's hips on the thwart") rig 2, the hero in a rowboat: whatever he does,
  // nothing of him is drawn on or below the thwart's seat line (art::shipThwartY, HOLDERS): the plank's own row shows
  // under his hips. The thwart lies 2 px under the gunwale amidships and the near gunwale is never higher on screen than
  // that, so the cut never shows what the hull hides; his legs are in the boat, under the thwart, where the 3/4 view never
  // sees them. The cut is in the blit itself, so the night lift and any replay of the recorded blit carry it too (and the
  // render's ghost, which looks for the frame's whole rect, never tints him: nothing of him is hidden). Idle or under way
  // he sits at the oars (below).
  float clipRow = 1e9f;   // the first screen row (world view px) not drawn: the thwart's seat line
  bool rower = false;     // seated on the middle thwart
  float rowerDy = 0;      // the thwart's seat line over the floor a standing figure's soles rest on (px)
  if (v2Look && a.player && g.sea.atSea() && !g.sea.live.swimming && g.sea.aboard >= 0 && g.sea.aboard < (int)g.sea.boats.size() &&
      g.sea.boats[(size_t)g.sea.aboard].hull == sea::Hull::Rowboat) {
    const int hull = (int)sea::Hull::Rowboat;
    rowerDy = (float)(art::shipThwartY(hull) - art::shipDeckY(hull));
    // the same rounding as the boat's own cell (render_sea.cpp drawCell: floor of its screen point), so the line is
    // the thwart's row to the pixel; seaHeroLift carries the bob both share
    clipRow = std::floor(a.p.y - cam.y) - seaHeroLift(*this, g) - rowerDy;
    rower = !posing && (a.st == AState::Idle || a.st == AState::Walk) && !p2a.poseOn;
  }
  // one v2 frame by its pivot (feet: the pivot's world point): the bather's waterline crop, the corpse's stain, the hit
  // flash and the burn flicker; the head's top for bubbles and plates; the hero's sprite for Witness
  // (P2b VIEW-B) the look's rest frame (LOCO idle frame 0 of a facing): its top (hair, a hat, a crest) is where markers,
  // plates and bubbles float, steady through the walk's bob and a swing's raised blade; a posture moves it with the head
  auto restAnchors = [&](int facing) -> const art::RigAnchors* {
    const RigTex lt = rigTex(look, art::RigPart::Loco, child);
    return lt.ready() ? lt.sheet->anchorsOf(facing, art::Clip::Idle, 0, 0) : nullptr;
  };
  auto drawV2 = [&](const Tex& tx, const art::FrameRect& f2, const art::RigAnchors* an, Vec2 feet, float alpha2, bool stain, int frameNo, int facing,
                    bool follow = false) {
    const float bob2 = a.player && g.sea.atSea() ? -seaHeroLift(*this, g) : 0.0f;
    const float pivX = std::floor(feet.x - cam.x + 0.5f), pivY = std::floor(feet.y + bob2 - cam.y + 0.5f);
    const float x2 = pivX - f2.px, y2 = pivY - f2.py;
    // the rows drawn (all but in a rowboat: down to the thwart's seat line)
    const int hv = std::clamp((int)(clipRow - y2), 0, (int)f2.h);
    bool done = false;
    // a bather or a swimmer: head and shoulders over the water, the chest a blur under it, ripples at the waterline
    if (rvBathing(g, m, a) || (a.player && g.sea.live.swimming && !g.sea.atSea())) {
      const art::RigMetrics& M2 = art::rig(2);
      const int vis = std::min<int>(M2.bathVis, f2.h), under = std::max(0, std::min<int>(M2.bathUnder, f2.h - vis));
      const float wy = std::floor(a.p.y - 3 - cam.y + 0.5f);
      const float bobW = (float)((int)(t_ * 1.3f + a.id * 0.7f) & 1);
      P.blitEx(tx, f2.x, f2.y, f2.w, vis, x2, wy - vis + bobW, (float)f2.w, (float)vis, false, Color(1, 1, 1, alpha2));
      if (under > 0)
        P.blitEx(tx, f2.x, f2.y + vis, f2.w, under, x2, wy + bobW, (float)f2.w, (float)under, false, Color(0.45f, 0.75f, 0.85f, 0.38f * alpha2));
      P.rect(x2 + 2, wy, (float)std::max(2, f2.w - 4), 1, Color(0.78f, 0.92f, 0.95f, 0.55f * alpha2));
      P.rect(x2 + 1, wy + 1, (float)std::max(2, f2.w - 2), 1, Color(0.05f, 0.18f, 0.26f, 0.35f * alpha2));
      P.rect(x2 + 5, wy - 1, 3, 1, Color(0.9f, 0.97f, 1.0f, 0.4f * alpha2));
      done = true;
    }
    if (!done) {
      // the dead: a small dark stain on the ground under the body (Ink at 25 %, STYLE_P2 13)
      if (stain) {
        const Color ink(30 / 255.0f, 22 / 255.0f, 42 / 255.0f, 0.25f * alpha2);
        const float sw = (float)std::max(8, f2.w - 6), sx = std::floor(x2 + (f2.w - sw) / 2.0f);
        P.rect(sx + 2, pivY - 1, sw - 4, 1, ink);
        P.rect(sx, pivY, sw, 1, ink);
        P.rect(sx + 2, pivY + 1, sw - 4, 1, ink);
      }
      if (hv > 0) {
        P.blitEx(tx, f2.x, f2.y, f2.w, hv, x2, y2, (float)f2.w, (float)hv, false, Color(1, 1, 1, alpha2));
        if (flash > 0) P.blitEx(tx, f2.x, f2.y, f2.w, hv, x2, y2, (float)f2.w, (float)hv, false, Color(1, 1, 1, flash), 1);
      }
      if (a.burnT > 0 && ((int)(t_ * 10) & 1) && hv == f2.h)
        P.rectAdd(x2 + f2.w / 2 - 3, y2 + f2.h * 0.3f, 6, std::max(4.0f, f2.h * 0.4f), Color(0.6f, 0.25f, 0.05f, 0.6f));
    }
    // (P2b VIEW-B) the frame's top for markers, plates and bubbles (RigAnchors::topY, STYLE_P2 17): the rest frame's top
    // (steady while walking, running or swinging), moved by the head's drop in a posture (seated, kneeling, a rower);
    // a lying body its own frame's top; else the head box, else the frame's rect
    const art::RigAnchors* ra = stain ? nullptr : restAnchors(facing);
    float topRel = (float)(y2 - pivY);
    if (ra && ra->topY != art::kNoAnchor) {
      topRel = (float)ra->topY;
      if (follow && an && an->headY != art::kNoAnchor && ra->headY != art::kNoAnchor) topRel += (float)(an->headY - ra->headY);
    } else if (an && an->topY != art::kNoAnchor) {
      topRel = (float)an->topY;
    } else if (an && an->headY != art::kNoAnchor) {
      topRel = (float)an->headY;
    }
    // world y, exact against the feet (no camera rounding: View::topLift reads feet minus top as whole px)
    drawnHead_[a.id] = DrawnHead{feet.x, feet.y + bob2 + topRel, t_, true};
    if (a.player) {
      HeroSprite& hs = heroSprite;
      hs.on = true; hs.rig = 2; hs.x = x2; hs.y = y2; hs.w = f2.w; hs.h = f2.h; hs.fr = frameNo; hs.row = facing; hs.flip = false;
      hs.bx0 = 0; hs.by0 = 0; hs.bx1 = f2.w - 1; hs.by1 = f2.h - 1;   // a trimmed atlas frame: its rect is its box
      if (hv < f2.h) hs.by1 = hv - 1;   // in a rowboat: the box is what is drawn (the frame's rect stays w x h)
      hs.pivX = pivX; hs.pivY = pivY;
      // (the roof silhouette, HumanGhost, is cell-based: v2's needs a source rect, LENS)
    }
  };
  // (P2b `dollpart`) the hero drawn as one doll part alone at its anchor, for an artist's review in the game
  if (a.player && !p2b.dollPart.empty() && art::rigVersion() >= 2) {
    const RigTex dt2 = dollPartTex(look);
    if (dt2.ready()) {
      const art::FrameRect* f2 = dt2.sheet->frame(p2b.dollFacing, art::Clip::Idle, 0, 0);
      if (f2) { drawV2(*dt2.tex, *f2, dt2.sheet->anchorsOf(p2b.dollFacing, art::Clip::Idle, 0, 0), at, alpha, false, 0, p2b.dollFacing); return; }
    }
  }
  // the rower: the v2 Sit posture (POSES, the chair-height sitter) in his facing, its seat point (the hip anchor) on the
  // thwart's seat line and the body cut there; the column on the boat's own (its anchor, floored as its cell is). He
  // keeps the posture's own slow breath; the boat's bob and its oars' stroke (art::shipCell frames) move him with it.
  // (P2b VIEW-B) Row (STYLE_P2 15: reach, catch, pull, recover) where the doll covers it, its keys on the oars' stroke
  // (seaOarPhase: the boat cell's own clock, so the hands reach as the blades swing forward and pull as they go aft);
  // the Sit posture until then
  if (rower) {
    RigTex rt = rigPoseTex(look, art::Posture::Row, 0);
    const bool rowing = rt.ready() && rt.sheet->count(std::clamp(a.face, 0, 3), art::Clip::Idle, 0) > 0;
    if (!rowing) rt = rigPoseTex(look, art::Posture::Sit, 0);
    if (rt.ready()) {
      const art::PostureInfo pi = art::postureInfo(rowing ? art::Posture::Row : art::Posture::Sit);
      const float clock = t_ + (float)(hash32((uint32_t)a.id * 2654435761u) % 997u) * 0.0131f;
      const int nfr = std::clamp((int)pi.frames, 1, art::POSTURE_FRAMES);
      const int pf = rowing ? std::clamp((int)(seaOarPhase(*this, g) * 2.0f), 0, 3) * nfr / 4
                            : (int)(clock * std::max(0.1f, pi.fps)) % nfr;
      int fc = std::clamp(a.face, 0, 3);
      const art::FrameRect* f2 = rt.sheet->frame(fc, art::Clip::Idle, 0, pf);
      if (!f2) { fc = 0; f2 = rt.sheet->frame(0, art::Clip::Idle, 0, pf); }
      if (f2) {
        const int n = std::max(1, rt.sheet->count(fc, art::Clip::Idle, 0));
        const int fi = ((pf % n) + n) % n;
        const art::RigAnchors* an = rt.sheet->anchorsOf(fc, art::Clip::Idle, 0, fi);
        const int hipY = an && an->hipY != art::kNoAnchor ? an->hipY : -(art::holders::kSeat + 2);
        // the pivot (the floor under the hips) hipY rows under the seat line: whole rows, so drawV2's rounding is exact
        const Vec2 feet(std::floor(a.p.x - cam.x) + cam.x, std::floor(a.p.y - cam.y) + cam.y - rowerDy - (float)hipY);
        drawV2(*rt.tex, *f2, an, feet, alpha, false, fi, fc, true);
        if (a.player) heroSprite.part = 4;   // (a posture atlas)
        m5Count_.posed++;
        return;
      }
    }
  }
  if (v2Look && !posing) {
    AnimPick k = animPick(g, a, 2);
    art::HumanLook lookP = look;
    if (a.player && p2a.poseOn) {
      k.clip = p2a.poseClip; k.variant = (uint8_t)p2a.poseVariant; k.frame = (uint8_t)p2a.poseFrame;
      if (p2a.poseFamily != art::familyOf(look)) lookP.weapon = weaponOfFamily(p2a.poseFamily);
    }
    const int facing = std::clamp(a.face, 0, 3);
    RigTex rt;
    const art::FrameRect* fr2 = nullptr;
    const art::RigAnchors* an = nullptr;
    // (P2b slice fix, SLICE-RUN-INGAME) the part the anim key picked first (the RUN part above the gait switch: its
    // own drawings on its own table, so the soles register; the ACTION part's drawn walk / idle), then the clip's
    // usual part (a part whose atlas lacks the clip, is not baked yet or is empty for this look)
    int partDrawn = -1;
    auto tryPart = [&](art::RigPart part, art::Clip c, int variant, int frame) {
      if (fr2) return;
      RigTex r = rigTex(lookP, part, child);
      if (!r.ready()) return;
      const art::FrameRect* f = r.sheet->frame(facing, c, variant, frame);
      int fc = facing;
      if (!f && (c == art::Clip::Lying || c == art::Clip::Fall)) { f = r.sheet->frame(0, c, variant, frame); fc = 0; }
      if (!f) return;
      rt = r; fr2 = f; an = r.sheet->anchorsOf(fc, c, variant, frame); partDrawn = (int)part;
    };
    auto tryClip = [&](art::Clip c, int variant, int frame) { tryPart(partOf(c), c, variant, frame); };
    if (k.part != partOf(k.clip) && !(a.player && p2a.poseOn)) tryPart(k.part, k.clip, k.variant, k.frame);
    // (P2c) a frozen drawn idle or walk (`pose idle|walk <family> 1|2 <frame>`): the DRAWN part's frame
    if (a.player && p2a.poseOn && (k.clip == art::Clip::Idle || k.clip == art::Clip::Walk) && k.variant >= 1) tryPart(art::RigPart::Drawn, k.clip, k.variant, k.frame);
    // (P2b fix r1) a drawn run (ACTION walk variant 2) whose ACTION atlas lacks it or is not baked yet: the sheathed run
    if (k.gait && (k.part == art::RigPart::Action || k.part == art::RigPart::Drawn)) tryPart(art::RigPart::Run, k.clip, 0, k.frame);
    // (P2c) a drawn idle or walk whose DRAWN atlas is not baked yet: the sheathed LOCO frame (never a hole)
    if (k.part == art::RigPart::Drawn) tryPart(art::RigPart::Loco, k.clip, 0, k.frame);
    tryClip(k.clip, k.variant, k.frame);
    switch (k.clip) {   // a clip this look's atlas lacks: the nearest it has (never a v1 frame mid-swing)
      case art::Clip::Draw: case art::Clip::Release: case art::Clip::Block: tryClip(art::Clip::Cast, 0, k.clip == art::Clip::Draw ? 0 : 1); [[fallthrough]];
      case art::Clip::Windup: case art::Clip::Strike: case art::Clip::Follow: case art::Clip::Cast: case art::Clip::Roll: case art::Clip::Hurt:
        tryClip(art::Clip::Idle, 0, 0);
        break;
      default: break;
    }
    if (fr2 && rt.ready()) {
      const float alpha2 = a.st == AState::Dead ? animCorpseAlpha(a, 2) : alpha;
      drawV2(*rt.tex, *fr2, an, at, alpha2, k.clip == art::Clip::Lying, k.frame, facing);
      // (P2c ladder) the living finish over it (T5-T6; the dead lie dark)
      if (a.st != AState::Dead && lookP.topTier() >= 5) {
        const float bob2 = a.player && g.sea.atSea() ? -seaHeroLift(*this, g) : 0.0f;
        const float pivX = std::floor(at.x - cam.x + 0.5f), pivY = std::floor(at.y + bob2 - cam.y + 0.5f);
        queueLivingFinish(a, lookP, rt, *fr2, an, pivX - fr2->px, pivY - fr2->py, pivX, pivY, alpha);
      }
      // (P2c) HeroSprite::part: 0 loco 1 action 2 death 3 run, 4 a posture atlas (home_view's too), 5 the DRAWN part
      if (a.player) heroSprite.part = partDrawn == (int)art::RigPart::Drawn ? 5 : partDrawn;
      return;
    }
  }
  if (posing) {
    art::Posture pz = pzA;
    // (P2b fix r2, B6: the POSES instrument variants no draw asked for) rig 2 only, rig 1 unchanged: between tunes the
    // musician takes up his people's drum (MusicStyle::perc: the drum's variant below) one tune in three, each player on
    // his own phase of the sim's 30 s tune
    if (v2Look && (pz == art::Posture::Lute || pz == art::Posture::Flute) && a.st == AState::Idle) {
      const int cs = g.inside && g.subBldg >= 0 && g.subBldg < (int)g.world.over.bldgs.size() ? g.world.over.bldgs[(size_t)g.subBldg].site : a.site;
      const cult::Culture* C = cs >= 0 ? g.world.cultureOf(cs) : nullptr;
      if (C && C->music.perc != PercKind::None) {
        const uint32_t ph = hash32((uint32_t)a.id * 0x9E3779B1u ^ 0xD2D2u) % 30u;
        const int tune = (int)std::floor(((float)g.time + (float)ph) / 30.0f);
        if (((tune % 3) + 3) % 3 == 2) pz = art::Posture::Drum;
      }
    }
    const art::PostureInfo pi = art::postureInfo(pz);
    const int nf = std::clamp((int)pi.frames, 1, art::POSTURE_FRAMES);
    const bool stride = a.st == AState::Walk && (pz == art::Posture::Carry || pz == art::Posture::Play || pz == art::Posture::Dance);
    const float clock = (a.postureT > 0 ? a.postureT : t_) + (float)(hash32((uint32_t)a.id * 2654435761u) % 997u) * 0.0131f;
    const int pf = stride ? (int)(a.animT * 9) % nf : (int)(clock * std::max(0.1f, pi.fps)) % nf;
    uint8_t variant = 0;
    if (pz == art::Posture::Kneel && story::isStoryPerson(a)) variant = 1;   // (P1b) a bout's yield: the head bowed
    // (P2b fix r2, B6) rig 2 only: a carrier's load is his own (POSES "carry loads (sack, basket, crate)"), by his id
    if (v2Look && pz == art::Posture::Carry) variant = (uint8_t)(hash32((uint32_t)a.id * 2246822519u ^ 0xCA44u) % 3u);
    // (ART seatFit) on a seat: the posture for its kind (chair height or cross-legged) and the sitter's feet
    // where its seat line lands on the seat's surface; a facing the seat does not take keeps the figure where it is
    // (P2a ANIM) at rig 2 the v2 sitter's pivot by seatFit2 (POSES: the floor under the hips), kept beside v1's so a
    // frame whose v2 atlas is not baked yet still draws v1 where v1 sits
    Vec2 at2 = at;
    if (pi.seated && a.useX >= 0 && a.useY >= 0 && m.in(a.useX, a.useY)) {
      const int sp = m.propAt(a.useX, a.useY);
      const art::Prop seat = sp ? (art::Prop)(sp - 1) : art::Prop::COUNT;
      const art::SeatFit f = art::seatFit(seat, -1, a.face);
      // (M5 fixer r2) on the bench before a long hearth: forward on its edge, so the embers and logs show over
      // the shoulders (life_game.cpp sitDown places the body the same)
      bool hearth = false;
      if (seat == art::Prop::Bench && a.face == 1 && m.in(a.useX, a.useY - 1)) {
        const int up = m.propAt(a.useX, a.useY - 1);
        hearth = up == (int)art::Prop::FirePitL + 1 || up == (int)art::Prop::FirePitM + 1 || up == (int)art::Prop::FirePitR + 1;
      }
      if (f.ok) {
        pz = art::seatedPosture(seat, pz);
        floorSeat = seat == art::Prop::COUNT;
        at = Vec2(a.useX * 16.0f + 8.0f + (a.face == 3 ? -f.ax : f.ax), a.useY * 16.0f + 16.0f + f.ay);
        if (hearth) at.y += 5;
      }
      if (v2Look) {
        const art::SeatFit f2 = art::seatFit2(seat, -1, a.face);
        if (f2.ok) {
          pz = art::seatedPosture(seat, pzA);
          floorSeat = seat == art::Prop::COUNT;
          at2 = Vec2(a.useX * 16.0f + 8.0f + (a.face == 3 ? -f2.ax : f2.ax), a.useY * 16.0f + 16.0f + f2.ay);
          if (hearth) at2.y += 5;
        } else {
          at2 = at;   // a facing the v2 seat does not take: where v1 put the figure
        }
      }
    }
    // (ART bedFit) in a bed: the sleeper fitted to the berth, drawn right after it at the berth's own anchor
    if (pi.lying && a.useX >= 0 && a.useY >= 0 && m.in(a.useX, a.useY)) {
      int kit = -1;
      const art::Berth bk = rvBerthAt(m, a.useX, a.useY, kit);
      const int sf = (int)(clock * std::max(0.1f, pi.fps)) & 1;
      // (P2a ANIM) rig 2: sleeper2 fitted by bedFit2, its texture cached with the v2 atlases (baked within their budget)
      if (v2Look) {
        const uint64_t sk = ew::mix64(look.key() ^ 0x51EE92ull ^ ((uint64_t)bk << 40) ^ ((uint64_t)(kit + 1) << 48) ^ ((uint64_t)(sf & 1) << 60));
        auto it = rigTex_.find(sk);
        if (it == rigTex_.end() && (witness || rigBakeBudget())) {
          const auto t0 = std::chrono::steady_clock::now();
          RigEntry e;
          const Canvas sc = art::sleeper2(look, bk, kit, sf & 1);
          if (sc.w > 0 && sc.h > 0) { e.tex = pix_->bake(sc); liftMaskFor(sc, e.tex); }
          it = rigTex_.emplace(sk, std::move(e)).first;
          rigBakeFrameMs_ += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
        }
        if (it != rigTex_.end() && it->second.tex.t) {
          it->second.used = t_;
          const Tex& st = it->second.tex;
          const art::BedFit bf = art::bedFit2(bk, kit);
          const float sx = std::floor(a.useX * 16.0f + 8.0f - bf.w / 2.0f - cam.x), sy = std::floor(a.useY * 16.0f + 16.0f - bf.h - cam.y);
          P.blitEx(st, 0, 0, st.w, st.h, sx, sy, (float)st.w, (float)st.h, false, Color(1, 1, 1, alpha));
          drawn = true;
          m5Count_.posed++;
        }
      }
      if (drawn) {
      } else if (const Tex* st = sleeperTex(look, bk, kit, sf)) {
        const art::BedFit bf = art::bedFit(bk, kit);
        const float sx = std::floor(a.useX * 16.0f + 8.0f - bf.w / 2.0f - cam.x), sy = std::floor(a.useY * 16.0f + 16.0f - bf.h - cam.y);
        P.blitEx(*st, 0, 0, st->w, st->h, sx, sy, (float)st->w, (float)st->h, false, Color(1, 1, 1, alpha));
        drawn = true;
        m5Count_.posed++;
        v1Draws++;
      }
    }
    // the bard's instrument in his people's style (ART: humanPostureSheet variants by MusicStyle)
    if (pz == art::Posture::Lute || pz == art::Posture::Flute || pz == art::Posture::Drum) {
      const int cs = g.inside && g.subBldg >= 0 && g.subBldg < (int)g.world.over.bldgs.size() ? g.world.over.bldgs[(size_t)g.subBldg].site : a.site;
      if (const cult::Culture* C = cs >= 0 ? g.world.cultureOf(cs) : nullptr) {
        if (pz == art::Posture::Drum) variant = (uint8_t)C->music.perc;
        else if (art::bardPosture(C->music) == pz) variant = art::bardVariant(C->music);
      }
    }
    // (P2a ANIM) rig 2: the posture's v2 frame (humanPosture2: clip Idle variant 0, four native facings; the generic
    // sleeper one), its pivot on at2 (seatFit2's floor under the hips; standing postures the feet), never flipped
    if (!drawn && v2Look) {
      const RigTex rt = rigPoseTex(look, pz, variant);
      if (rt.ready()) {
        int fc = std::clamp(a.face, 0, 3);
        const art::FrameRect* f2 = rt.sheet->frame(fc, art::Clip::Idle, 0, pf);
        if (!f2) { fc = 0; f2 = rt.sheet->frame(0, art::Clip::Idle, 0, pf); }
        if (f2) {
          const int fi = ((pf % std::max(1, rt.sheet->count(fc, art::Clip::Idle, 0))) + 64) % std::max(1, rt.sheet->count(fc, art::Clip::Idle, 0));
          if (floorSeat) {   // (fixer M5 r3) a floor sitter's cushion, under the v2 hips
            const Tex& ct = cachedTex(0x6Aull << 56 | (uint64_t)(hash32((uint32_t)(a.useX * 73856093) ^ (uint32_t)(a.useY * 19349663)) & 3u),
                                      rvFloorCushion);
            P.blitEx(ct, 0, 0, ct.w, ct.h, std::floor(at2.x - 9.0f - cam.x + 0.5f), std::floor(at2.y - 5.0f - cam.y + 0.5f), (float)ct.w, (float)ct.h,
                     false, Color(1, 1, 1, alpha));
          }
          drawV2(*rt.tex, *f2, rt.sheet->anchorsOf(fc, art::Clip::Idle, 0, fi), at2, alpha, false, fi, fc, true);
          if (a.player) heroSprite.part = 4;
          m5Count_.posed++;
          return;
        }
      }
    }
    if (!drawn) {
      if (const Tex* pt = poseTex(look, pz, variant, child)) {
        tp = pt;
        fr = pf;
        if (pi.oneRow || pi.lying) { row = 0; flip = false; }
        m5Count_.posed++;
      } else {
        fr = stride ? 1 + (int)(a.animT * 9) % 4 : 0;   // the stand-in this frame: its own standing sheet
        m5Count_.poseFallback++;
      }
      bob += (float)art::postureInfo(pz).dy;
    }
  }
  const bool standSheet = !child && tp == &humanTex(look, child);   // (M8b fixer r3) the aura fits this sheet only
  const Tex& t = *tp;
  float x = at.x - art::HUMAN_W / 2.0f - cam.x, y = at.y - art::HUMAN_H + 2 + bob - cam.y;
  if (a.st == AState::Dead) y += 3;   // the dead's offset (v1: the hurt frame, 3 px lower)
  x = std::floor(x + 0.5f);   // (M5) crisp at 1x: whole pixels (a seated or sleeping body never shimmers)
  y = std::floor(y + 0.5f);
  // (M5 fixer r2) a bather in a bath's pool: head and shoulders above the water, a ring of ripples at the waterline
  if (!drawn && (rvBathing(g, m, a) || (a.player && g.sea.live.swimming && !g.sea.atSea()))) {   // (M10) and a swimmer in the sea
    const int vis = art::rig(1).bathVis, under = art::rig(1).bathUnder;   // head and shoulders in the air, the chest a blur under the water
    const float wy = std::floor(a.p.y - 3 - cam.y + 0.5f);
    const float bobW = (float)((int)(t_ * 1.3f + a.id * 0.7f) & 1);
    P.blitEx(t, fr * art::HUMAN_W, row * art::HUMAN_H, art::HUMAN_W, (float)vis, x, wy - vis + bobW, (float)art::HUMAN_W, (float)vis, flip, Color(1, 1, 1, alpha));
    P.blitEx(t, fr * art::HUMAN_W, row * art::HUMAN_H + vis, art::HUMAN_W, (float)under, x, wy + bobW, (float)art::HUMAN_W, (float)under, flip,
             Color(0.45f, 0.75f, 0.85f, 0.38f * alpha));
    P.rect(x + 2, wy, 12, 1, Color(0.78f, 0.92f, 0.95f, 0.55f * alpha));
    P.rect(x + 1, wy + 1, 14, 1, Color(0.05f, 0.18f, 0.26f, 0.35f * alpha));
    P.rect(x + 4, wy - 1, 2, 1, Color(0.9f, 0.97f, 1.0f, 0.4f * alpha));
    drawn = true;
    m5Count_.posed++;
    v1Draws++;
  }
  if (floorSeat && !drawn) {   // (fixer M5 r3) "seated patrons hover with no seat": a cushion under them
    const Tex& ct = cachedTex(0x6Aull << 56 | (uint64_t)(hash32((uint32_t)(a.useX * 73856093) ^ (uint32_t)(a.useY * 19349663)) & 3u),
                              rvFloorCushion);
    P.blitEx(ct, 0, 0, ct.w, ct.h, std::floor(at.x - 9.0f - cam.x + 0.5f), std::floor(at.y - 5.0f - cam.y + 0.5f), (float)ct.w, (float)ct.h,
             false, Color(1, 1, 1, alpha));
  }
  // (M8b fixer r3, review: "a Mythic-geared hero looks plain in the world") a Mythic wearer stands in a flickering
  // flame that hugs the whole silhouette and licks upward (loot_ui.cpp mythicAuraTex): behind the body here, and
  // again additively after the lighting (beasts::glows), so it burns by night as well as reading by day
  if (!drawn && standSheet && a.look.glow && a.look.glowColor == kMythicGlow && a.st != AState::Dead) {
    if (const Tex* au = mythicAuraTex(a.look)) {
      const int v = (int)(t_ * 11.0f + (float)(a.id % 7) * 0.37f) % AURA_V;
      const int sx = ((row * art::HUMAN_FRAMES + fr) * AURA_V + v) * AURA_CW;
      const float ax = x - AURA_PAD, ay = y - AURA_PAD;
      P.blitEx(*au, sx, 0, AURA_CW, AURA_CH, ax, ay, (float)AURA_CW, (float)AURA_CH, flip, Color(1, 1, 1, 0.92f * alpha));
      rvPushGlow(au, sx, AURA_CW, AURA_CH, ax, ay, flip, 0.7f * alpha);
    }
  }
  if (!drawn) { P.blitEx(t, fr * art::HUMAN_W, row * art::HUMAN_H, art::HUMAN_W, art::HUMAN_H, x, y, (float)art::HUMAN_W, (float)art::HUMAN_H, flip, Color(1, 1, 1, alpha)); v1Draws++; }
  if (!drawn) drawnHead_[a.id] = DrawnHead{at.x, y + cam.y, t_};
  if (a.player) {
    ghost.t = &t; ghost.fr = fr; ghost.row = row; ghost.flip = flip; ghost.x = x; ghost.y = y;
    // (P2a, Witness state.player.sprite) the hero's cell as drawn, its opaque box from the baked sheet, the feet point
    HeroSprite& hs = heroSprite;
    hs.on = !drawn; hs.rig = 1; hs.x = x; hs.y = y; hs.w = art::HUMAN_W; hs.h = art::HUMAN_H; hs.fr = fr; hs.row = row; hs.flip = flip;
    hs.pivX = std::floor(at.x - cam.x + 0.5f); hs.pivY = std::floor(at.y + bob - cam.y + 0.5f);
    hs.bx0 = 0; hs.by0 = 0; hs.bx1 = -1; hs.by1 = -1;
    if (standSheet) {
      auto bi = humanBoxes_.find(look.key());
      const int cell = row * art::HUMAN_FRAMES + fr;
      if (bi != humanBoxes_.end() && cell >= 0 && cell < (int)bi->second.size()) {
        const CellBox& cb = bi->second[(size_t)cell];
        if (cb.x1 >= cb.x0) {   // a flipped cell mirrors its box
          hs.bx0 = flip ? art::HUMAN_W - 1 - cb.x1 : cb.x0; hs.bx1 = flip ? art::HUMAN_W - 1 - cb.x0 : cb.x1;
          hs.by0 = cb.y0; hs.by1 = cb.y1;
        }
      }
    }
  }
  if (flash > 0 && !drawn) P.blitEx(t, fr * art::HUMAN_W, row * art::HUMAN_H, art::HUMAN_W, art::HUMAN_H, x, y, (float)art::HUMAN_W, (float)art::HUMAN_H, flip, Color(1, 1, 1, flash), 1);
  if (a.burnT > 0 && ((int)(t_ * 10) & 1)) P.rectAdd(x + 5, y + 6, 6, 10, Color(0.6f, 0.25f, 0.05f, 0.6f));
}

// (P2a style slice) facing up, hits 1 and 2's Steel Arc arch beyond the body (STYLE_P2 12: "facing up, it passes over the
// head and shoulders, visible above them"), so render.cpp's sorted pass draws it before the body instead of after;
// everything else (and hit 3's upright crescent, which starts from the blade hanging down the back) is drawn after.
bool rvSwingFxBehind(const Game& g, const Actor& a) {
  if (a.face != 1 || !(a.player || a.human)) return false;
  if (art::rigVersion() < 2 || !art::rig2Covers(a.look)) return false;
  if (animSwingProgress(g, a) < 0 || animSwingFx(g, a) != art::Fx::SwingArc) return false;
  return animPick(g, a, 2).variant != 2;
}

// (P2a fix FX-RIG1-HEAVY-ARC) the swing effect's native size for the body drawn: v2 at the STYLE_P2 12 size; a v1 body
// at 22 / 32 of it (the figures' ratio), but the point effects (glint, punch star, twang: already a few px, they lose
// their shape smaller) and the legacy Slash (painted for the v1 body); the finisher 1.2x that. The Thrust Streak keeps
// its 5 px thickness on the v1 body (its painter reads the cell: only the length scales). A painter that cannot paint at
// another size (art::fxSpriteSized returns an empty canvas: today the legacy Slash) keeps its own size, so nothing is
// ever stretched at draw time.
struct RvFxSize { int w, h; };
static float rvV1Scale() { return (float)art::rig(1).figH(0, 0, 0) / (float)std::max(1, art::rig(2).figH(0, 0, 0)); }
static bool rvFxSizable(art::Fx f) {
  static int8_t known[(int)art::Fx::COUNT] = {};
  if ((int)f >= (int)art::Fx::COUNT) return false;
  int8_t& k = known[(int)f];
  if (!k) k = art::fxSpriteSized(f, art::fxW(f) + 1, art::fxH(f) + 1).w > 0 ? 1 : -1;
  return k > 0;
}
static RvFxSize rvFxSize(art::Fx f, bool v2, bool fin) {
  const int fw = art::fxW(f), fh = art::fxH(f);
  const bool point = f == art::Fx::StabGlint || f == art::Fx::PunchStar || f == art::Fx::Twang || f == art::Fx::Slash;
  float k = v2 || point ? 1.0f : rvV1Scale();
  if (fin) k *= 1.2f;
  if (k == 1.0f || !rvFxSizable(f)) return RvFxSize{fw, fh};
  if (f == art::Fx::ThrustStreak) return RvFxSize{(int)std::lround(fw * k), fin ? (int)std::lround(fh * 1.2f) : fh};
  const int h = std::max(1, (int)std::lround(fh * k));
  return RvFxSize{std::max(1, (int)std::lround(fw * (float)h / (float)fh)), h};   // the width by the painter's own k
}
// an effect painted natively in cells of w x h (art::fxSpriteSized; fxSprite at its own size), turned a quarter or not.
// key: w << 40 | h << 24 | fx id << 8 | rot (0 as painted, 1 clockwise = facing down, 3 counter-clockwise = facing up)
static Canvas rvSizedFx(uint64_t key) {
  const art::Fx f = (art::Fx)((key >> 8) & 0xFF);
  const int rot = (int)(key & 3), w = (int)((key >> 40) & 0xFFFF), h = (int)((key >> 24) & 0xFFFF);
  const Canvas s = w == art::fxW(f) && h == art::fxH(f) ? art::fxSprite(f) : art::fxSpriteSized(f, w, h);
  if (s.w <= 0) return Canvas(1, 1);
  if (rot == 0) return s;
  const int n = std::max(1, art::fxFrames(f));
  Canvas o(n * h, w);
  for (int k = 0; k < n; k++)
    for (int y = 0; y < h; y++)
      for (int x = 0; x < w; x++) {
        const uint32_t c = s.get(k * w + x, y);
        if (!c) continue;
        const int nx = rot == 1 ? h - 1 - y : y, ny = rot == 1 ? x : w - 1 - x;
        o.set(k * h + nx, ny, c);
      }
  return o;
}

// the swing effect (P2a ANIM, STYLE_P2 12): each family its own (animSwingFx), running over the strike state and ending
// with it (animSwingProgress: the family's pace); painted facing right, turned a quarter for the down and up facings,
// mirrored for the left; the hero's finisher in a warm tint and 1.2x. Placed by the rig's metrics (the body drawn: v2
// where it covers the look, else v1).
void View::drawSwingFx(const Game& g, const Actor& a, Vec2 cam) {
  Pix& P = *pix_;
  const float prog = animSwingProgress(g, a);   // (rpg/sim/anim.h: the effect and its timing, shared with rpg_test --anim)
  if (prog < 0) return;
  const bool v2 = art::rigVersion() >= 2 && art::rig2Covers(a.look);
  const art::RigMetrics& M = art::rig(v2 ? 2 : 1);
  const art::Fx f = animSwingFx(g, a);
  const art::Family fam = art::familyOf(a.look);
  // (P2a style slice) the Steel Arc on a v2 body (STYLE_P2 12): a 32 x 32 crescent (the finisher painted natively at 38 x
  // 38 in the Finisher tint #FFE6A0), drawn opaque so its core #F4F1E6 and edge #B8C4D8 land exactly. Its circle's centre
  // (1/32 of the cell beyond the cell's body side) sits on the body: facing down at the crown, so the crescent crosses
  // the front of the body at the chest and belly; facing up low on the back, so it arches over the head and shoulders;
  // side-on at the shoulder line, so it sweeps in front of the face. The crescent follows the blade: as painted it sweeps
  // clockwise (side-on top to bottom, facing down screen right to left, facing up left to right), which is hits 2 and 3
  // in sliceSwordAction's keys; hit 1 (and, facing left, whose keys swap hits 1 and 2, hit 2) runs the other way. Hit 3
  // facing down or up is the overhead cut seen end-on: its tip runs straight up or down the screen beside the weapon arm,
  // so its crescent stands upright there (facing down a "(" down the hero's right side, from over the head to the shins;
  // facing up a ")" up the right side from the back to over the head) instead of lying across the body.
  if (v2 && f == art::Fx::SwingArc) {
    const int face = std::clamp(a.face, 0, 3);
    const bool fin = a.player && a.combo == 2;   // the hero's third hit (as the v1 finisher)
    const int base = art::fxH(f), sz = fin ? (int)std::lround(base * 1.2f) : base;
    const int frames = std::max(1, art::fxFrames(f));
    const int fr = std::clamp((int)(prog * frames), 0, frames - 1);
    const int variant = animPick(g, a, 2).variant;
    const bool upright = variant == 2 && face <= 1;
    const bool back = upright ? face == 1 : variant == (face == 3 ? 1 : 0);
    const bool flip = upright ? face == 0 : face == 3;
    const int rot = upright ? 0 : face == 0 ? 1 : face == 1 ? 3 : 0;
    const Tex& ft = cachedTex(0x7Cull << 56 | (uint64_t)sz << 16 | (uint64_t)f << 8 | (back ? 4ull : 0ull) | (uint64_t)rot, rvArcCells);
    // the circle's centre on the body (pivot-relative) and the way the cell lies from it (facing up the arch tops out 3 px
    // over the crown and is drawn behind the body, rvSwingFxBehind: the head and shoulders hide its middle)
    static const int kArcX[4] = {0, 0, -1, 1}, kArcY[4] = {-32, -16, -17, -17};
    static const int kDirX[4] = {0, 0, 1, -1}, kDirY[4] = {1, -1, 0, 0};
    const int ax = upright ? (face == 0 ? 6 : -11) : kArcX[face], ay = upright ? (face == 0 ? -18 : -26) : kArcY[face];
    const int dx = upright ? (face == 0 ? -1 : 1) : kDirX[face], dy = upright ? 0 : kDirY[face];
    const float off = sz * 0.5f + sz / 32.0f;
    const Vec2 c = a.p + Vec2(ax + dx * off, ay + dy * off);
    const Color tint = fin ? Color(1, 0xE6 / 255.0f, 0xA0 / 255.0f, 1) : Color(1, 1, 1, 1);
    P.blitEx(ft, fr * sz, 0, sz, sz, std::floor(c.x - sz * 0.5f - cam.x + 0.5f), std::floor(c.y - sz * 0.5f - cam.y + 0.5f), (float)sz, (float)sz,
             flip, tint);
    return;
  }
  // (P2a fix FX-RIG1-HEAVY-ARC) every other family, and the v1 body's sword (the legacy Slash), drawn 1:1 from a sprite
  // painted natively at the size it shows (rvFxSize; never stretched at draw time, STYLE_P2 1 and 20). On a v1 body (rig
  // 1, or a look v2 does not cover yet) the family effects are painted at the v1 body's scale (22 / 32 of the v2 figure)
  // and placed by the v2 metrics scaled the same way (lift 12 -> 8, the greatsword's reach 22 -> 15), so the Heavy Arc's
  // ellipse sits on the v1 shoulder and tops out at the v1 head as the v2 one does at the v2 head (it was the v2-sized
  // 48 x 40 arc centred on the v1 rig: a flat bar floating over the head). The finisher is the same effect painted
  // natively at 1.2x in the Finisher tint #FFE6A0; an arc hugging the body (Heavy Arc, Chop, Swish) grows away from it
  // from the same body-side edge (side-on, also from the same top edge: it reaches further out and lower, never higher).
  const art::Fx src = f == art::Fx::SwingArc ? art::Fx::Slash : f;
  const bool fin = a.player && a.combo == 2;
  const int frames = std::max(1, art::fxFrames(src));
  int fr = std::clamp((int)(prog * frames), 0, frames - 1);
  // on the v1 body the Heavy Arc plays its three full frames (1-3: the cleave with its ghost blade, the moon, the cut to
  // the ground) over the strike: its first and last are thin slivers along the ellipse's flat top and bottom, which on the
  // short v1 figure (and held through a finisher's hit-stop) read as a plank over the head, not a swing
  if (!v2 && src == art::Fx::HeavyArc && frames == 5) fr = 1 + std::clamp((int)(prog * 3), 0, 2);
  const bool turns = fxTurns(f) && (a.face == 0 || a.face == 1);
  const int rot = turns ? (a.face == 0 ? 1 : 3) : 0;
  const RvFxSize s1 = rvFxSize(src, v2, false), s = rvFxSize(src, v2, fin);
  const Tex& ft = !turns && s.w == art::fxW(src) && s.h == art::fxH(src)
                      ? fx_[(int)src]
                      : cachedTex(0x7Dull << 56 | (uint64_t)s.w << 40 | (uint64_t)s.h << 24 | (uint64_t)src << 8 | (uint64_t)rot, rvSizedFx);
  const int sw = turns ? s.h : s.w, sh = turns ? s.w : s.h;      // the drawn cell (turned a quarter: transposed)
  const int sw1 = turns ? s1.h : s1.w, sh1 = turns ? s1.w : s1.h;   // the hit 1 / 2 cell it grows from
  const art::RigMetrics& M2 = art::rig(2);
  const float k1 = v2 ? 1.0f : rvV1Scale();
  const float lift = v2 ? (float)M.fxLift : (float)std::lround(M2.fxLift * k1);
  const float reach = v2 ? (float)M.reach[(int)fam] : (float)std::lround(M2.reach[(int)fam] * k1);
  Vec2 c;
  if (f == art::Fx::SmashRing) c = a.p + a.aim * reach + Vec2(0, -1);         // the blow lands on the ground
  else if (f == art::Fx::Twang) c = a.p + Vec2(0, -lift) + a.aim * 6.0f;       // at the bow
  else c = a.p + Vec2(0, -lift) + a.aim * reach;
  float x0 = c.x - sw1 * 0.5f, y0 = c.y - sh1 * 0.5f;   // the hit 1 / 2 cell's top-left
  if (sw != sw1 || sh != sh1) {
    const bool hug = src == art::Fx::HeavyArc || src == art::Fx::ChopArc || src == art::Fx::BluntSwish;
    if (!hug) { x0 = c.x - sw * 0.5f; y0 = c.y - sh * 0.5f; }
    else if (a.face == 2) { /* the left (body-side) and top edges stay */ }
    else if (a.face == 3) x0 += (float)(sw1 - sw);                              // mirrored: the right edge stays
    else { x0 = c.x - sw * 0.5f; if (a.face == 1) y0 += (float)(sh1 - sh); }   // down: the top stays; up: the bottom
  }
  // the family effects opaque, so their named colours land exactly (STYLE_P2 6); the legacy Slash keeps its 0.9
  const bool family = (int)src >= (int)art::Fx::SwingArc;
  const Color tint = fin ? Color(1, 0xE6 / 255.0f, 0xA0 / 255.0f, 1) : Color(1, 1, 1, family ? 1.0f : 0.9f);
  P.blitEx(ft, fr * sw, 0, sw, sh, std::floor(x0 - cam.x + 0.5f), std::floor(y0 - cam.y + 0.5f), (float)sw, (float)sh, a.face == 3 && !turns,
           tint);
}
