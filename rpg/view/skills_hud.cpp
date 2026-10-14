// M9 "Arcana & Mastery" (VISION_PLAN 15.5, 15.29.3): the quick-cast controls. UI lane.
//
// Touch (15.5: "tap to cast, hold to upcast, swipe gestures for dual-cast"; "3-4 quick-cast buttons"): four buttons on
// an arc round the attack button's upper left, where the right thumb reaches, each showing its spell's orb, a
// clockwise cooldown sweep (Live::cd) and greyed while the magicka is short; an empty one shows a socket and opens the
// SPELLS page. A press sends Input::castSlot for one step and Input::castHold every frame while the finger stays down
// (game.h: "hold to upcast; letting go casts"): a ring fills round the button while held. A swipe off the button
// before letting go makes it a dual-cast (Input::castDual from the swipe through the release): a second ring and X2.
// The sneak toggle (Input::sneak) is a small eye button by the movement stick.
// Desktop: keys 1-4 cast the slots the same way (hold the key to upcast; Alt or Z held: dual-cast), Ctrl or V toggles
// sneaking; the HUD shows the four slots as a small bar (number, orb, cooldown) under the vitals. While sneaking a
// lidded eye shows beside the health bar.
#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>
#include <string>
#include "rpg/sim/magic.h"
#include "rpg/view/skills_paint.h"
#include "rpg/view/skills_ui.h"

namespace {
const Color kGoldQ(0.98f, 0.82f, 0.42f), kTextQ(0.93f, 0.9f, 0.82f), kDimQ(0.62f, 0.58f, 0.52f);
Color qCol(uint32_t c, float a = 1) { return Color((c & 255) / 255.0f, ((c >> 8) & 255) / 255.0f, ((c >> 16) & 255) / 255.0f, a); }
void disc(Pix& P, float cx, float cy, float r, Color c) {
  for (int yy = (int)-r; yy <= (int)r; yy++) {
    const float hw = std::floor(std::sqrt(std::max(0.0f, r * r - yy * yy)));
    P.rect(cx - hw, cy + yy, hw * 2, 1, c);
  }
}
void ring(Pix& P, float cx, float cy, float r, Color c, int n = 48, float from = 0, float upto = 1) {
  for (int k = 0; k < n; k++) {
    const float u = (float)k / n;
    if (u < from || u >= upto) continue;
    const float a = -skp::kPi / 2 + 2 * skp::kPi * u;
    P.rect(std::floor(cx + std::cos(a) * r), std::floor(cy + std::sin(a) * r), 1, 1, c);
  }
}
// the cooldown as a clockwise sweep from the top over a disc (k: the share still to run), row spans
void sweepDisc(Pix& P, float cx, float cy, int R, float k) {
  if (k <= 0) return;
  for (int yy = -R; yy <= R; yy++) {
    int run0 = 99999;
    for (int xx = -R; xx <= R + 1; xx++) {
      bool in = false;
      if (xx <= R && xx * xx + yy * yy <= R * R) {
        float a = std::atan2((float)xx + 0.5f, -(float)yy - 0.5f);
        if (a < 0) a += 2 * skp::kPi;
        in = a / (2 * skp::kPi) >= 1.0f - k;
      }
      if (in && run0 == 99999) run0 = xx;
      if (!in && run0 != 99999) { P.rect(cx + run0, cy + yy, (float)(xx - run0), 1, Color(0.02f, 0.02f, 0.05f, 0.62f)); run0 = 99999; }
    }
  }
}
const magic::SpellDef* slotSpell(const Game& g, int i) {
  const uint32_t id = i >= 0 && i < magic::QUICK_SLOTS ? g.arcana.book.slot[i] : 0;
  return id ? magic::spellById(id) : nullptr;
}
}  // namespace

// the arc on the 480 x 270 design screen (attack at 424, 222): four slots at a thumb's reach, clear of the bow (374,
// 240) and the roll (424, 172) buttons; the sneak toggle by the movement stick (bottom-left of the safe area)
QuickBtn quickBtnPos(int i) {
  static const float kQ[4][2] = {{342, 226}, {350, 199}, {366, 176}, {390, 160}};
  QuickBtn b;
  if (i >= 0 && i < 4) {
    b.r = 12;
    b.x = (float)(Pix::W - Pix::SR) - (480 - kQ[i][0]);
    b.y = (float)(Pix::H - Pix::SB) - (270 - kQ[i][1]);
  } else {
    b.r = 10;
    b.x = (float)Pix::SL + 118;
    b.y = (float)(Pix::H - Pix::SB) - 20;
  }
  return b;
}

int SkillsUiOps::quickAt(View& v, const Game& g, Vec2 p) {
  (void)v; (void)g;
  int best = -1;
  float bd = 1e9f;
  for (int i = 0; i < 5; i++) {
    const QuickBtn b = quickBtnPos(i);
    const float r = b.r + 5, d = (p.x - b.x) * (p.x - b.x) + (p.y - b.y) * (p.y - b.y);
    if (d < r * r && d < bd) { bd = d; best = i; }
  }
  return best;
}

bool SkillsUiOps::quickDown(View& v, Game& g, int b, uint64_t finger, Vec2 p) {
  View::SkillsUiState& S = st(v);
  if (b == 4) { sneakKey(v, g); return true; }
  if (b < 0 || b >= magic::QUICK_SLOTS) return false;
  if (!g.arcana.book.slot[b]) {   // an empty slot: the spellbook, on that slot
    if (g.travelling()) return true;
    g.mode = Mode::Menu; v.menuTab_ = View::kSpellsTab; v.menuSel_ = 0; S.slotSel = b;
    v.audio_->play(Sfx::MenuSelect);
    return true;
  }
  S.pressSlot = (int8_t)b; S.holdSlot = b; S.holdTouch = true; S.holdFinger = finger; S.holdDual = false; S.holdT = v.t_;
  (void)p;
  return true;
}
void SkillsUiOps::quickMove(View& v, uint64_t finger, Vec2 p) {
  View::SkillsUiState& S = st(v);
  if (S.holdSlot < 0 || !S.holdTouch || S.holdFinger != finger || S.holdDual) return;
  const QuickBtn b = quickBtnPos(S.holdSlot);
  const float dx = p.x - b.x, dy = p.y - b.y;
  if (dx * dx + dy * dy > (b.r + 12) * (b.r + 12)) { S.holdDual = true; v.audio_->play(Sfx::MenuMove, 1.4f, 0.6f); }
}
void SkillsUiOps::slotKey(View& v, Game& g, int slot, bool dual) {
  View::SkillsUiState& S = st(v);
  if (slot < 0 || slot >= magic::QUICK_SLOTS) return;
  if (!g.arcana.book.slot[slot]) { g.notice = "SLOT " + std::to_string(slot + 1) + " IS EMPTY: PREPARE A SPELL ON THE SPELLS PAGE"; g.noticeT = 3.5f; return; }
  S.pressSlot = (int8_t)slot; S.holdSlot = slot; S.holdTouch = false; S.holdFinger = 0; S.holdDual = dual; S.holdT = v.t_;
}
void SkillsUiOps::fillInput(View& v, Game& g, Input& in, const bool keyHeld[4], bool dualMod, float dt) {
  (void)g; (void)dt;
  View::SkillsUiState& S = st(v);
  in.castSlot = S.pressSlot;
  if (S.pressSlot >= 0 && S.pressSlot < 4) S.sentPress[S.pressSlot]++;
  S.pressSlot = -1;
  in.sneak = S.sneakPress;
  if (S.sneakPress) S.sentSneak++;
  S.sneakPress = false;
  // a toggle not taken yet (the step went to a cast): ask again while it is fresh
  if (!in.sneak && v.t_ - S.sneakT < 0.5f && g.skills.live.sneaking != S.sneakWant) in.sneak = true;
  if (g.skills.live.sneaking == S.sneakWant) S.sneakT = -100;
  if (S.holdSlot >= 0) {
    bool held = false;
    if (S.holdTouch) { for (const View::Finger& f : v.fingers_) if (f.on && f.id == S.holdFinger) held = true; }
    else held = S.holdSlot < 4 && keyHeld[S.holdSlot];
    if (held) {
      if (!S.holdTouch && dualMod) S.holdDual = true;
      in.castHold = true;
      in.castDual = S.holdDual;
      S.sentHoldMax = std::max(S.sentHoldMax, v.t_ - S.holdT);
    } else {   // let go: this step carries the release (castHold off) and whether it was a dual-cast
      in.castHold = false;
      in.castDual = S.holdDual;
      if (S.holdDual) S.sentDual++;
      S.sentRelease++;
      S.holdSlot = -1; S.holdDual = false;
    }
  }
}

void SkillsUiOps::drawQuickButtons(View& v, Game& g) {
  Pix& P = *v.pix_;
  View::SkillsUiState& S = st(v);
  S.quickDrawn = 0;
  for (int i = 0; i < magic::QUICK_SLOTS; i++) {
    const QuickBtn b = quickBtnPos(i);
    const magic::SpellDef* d = slotSpell(g, i);
    const bool held = S.holdSlot == i && S.holdTouch;
    const Color sc = d ? qCol(d->fx.ramp[2] ? d->fx.ramp[2] : 0xFFC0C0C0u) : Color(0.6f, 0.55f, 0.45f);
    // (M9 fixer, review: "the quick buttons are hard to read over foliage; an empty slot is almost invisible") a
    // solid dark plate with a dark outer edge and a lit top-left rim, so every button reads over any ground
    disc(P, b.x, b.y, b.r + 1, Color(0.02f, 0.02f, 0.03f, 0.55f));
    disc(P, b.x, b.y, b.r, Color(0.08f, 0.07f, 0.09f, held ? 0.82f : 0.68f));
    ring(P, b.x, b.y, b.r - 1, Color(1.0f, 0.95f, 0.85f, 0.22f), 40, 0.64f, 1.0f);   // (the arc from left to top)
    if (!d) {   // an empty socket: a gold ring and a bright plus (a tap opens the spellbook)
      const float pu = 0.75f + 0.25f * std::sin(v.t_ * 3.0f);
      ring(P, b.x, b.y, b.r, Color(0.95f, 0.82f, 0.5f, 0.7f), 40);
      ring(P, b.x, b.y, 7, Color(0.95f, 0.85f, 0.6f, 0.45f * pu), 20);
      P.rect(b.x - 3, b.y - 1, 7, 3, Color(0.05f, 0.04f, 0.04f, 0.6f));
      P.rect(b.x - 1, b.y - 3, 3, 7, Color(0.05f, 0.04f, 0.04f, 0.6f));
      P.rect(b.x - 3, b.y, 7, 1, Color(1.0f, 0.93f, 0.75f, 0.95f * pu));
      P.rect(b.x, b.y - 3, 1, 7, Color(1.0f, 0.93f, 0.75f, 0.95f * pu));
      continue;
    }
    S.quickDrawn++;
    const bool shortMp = g.mp < d->cost;
    const float cd = g.arcana.live.cd[i];
    // the orb, its school's glow behind it (dim while it cannot be cast)
    if (!shortMp && cd <= 0) P.blitEx(v.light_, 0, 0, 64, 64, b.x - 15, b.y - 15, 30, 30, false, Color(sc.r, sc.g, sc.b, held ? 0.55f : 0.22f), 1);
    const Tex& ic = spellIcon(v, d->id);
    P.blitEx(ic, 0, 0, 16, 16, b.x - 8, b.y - 8, 16, 16, false, shortMp ? Color(0.45f, 0.45f, 0.5f, 0.9f) : Color(1, 1, 1, 1));
    if (shortMp) disc(P, b.x, b.y, b.r - 1, Color(0.05f, 0.05f, 0.12f, 0.45f));
    const float k = cd > 0 && d->cooldown > 0 ? clampf(cd / d->cooldown, 0, 1) : 0.0f;
    if (k > 0) {
      sweepDisc(P, b.x, b.y, (int)b.r - 1, k);
      P.textS(b.x, b.y - 3, std::to_string((int)std::ceil(cd)), 1, kTextQ, 1);
    }
    const Color rc = held ? Color(1, 0.9f, 0.6f, 0.95f) : shortMp ? Color(0.5f, 0.55f, 0.85f, 0.7f) : Color(sc.r * 0.6f + 0.35f, sc.g * 0.6f + 0.35f, sc.b * 0.6f + 0.3f, 0.9f);
    ring(P, b.x, b.y, b.r, rc, 44);
    if (held) {   // the charge: a ring filling clockwise round the button, a step every 0.4 s (three steps: upcast 3)
      const float ht = v.t_ - S.holdT;
      const float u = clampf(ht / 1.2f, 0, 1);
      const Color hot = qCol(d->fx.ramp[3] ? d->fx.ramp[3] : 0xFFFFFFFFu);
      // (M9 fixer, review: "a 1-px dotted arc under the thumb") a thick band: a dark track all round, then the charge
      // filling it 3 px deep, solid, with a white leading edge; the steps reached as bright nodes
      const int nA = 120;
      for (int rr = 0; rr < 4; rr++) ring(P, b.x, b.y, b.r + 2 + rr, Color(0.02f, 0.02f, 0.04f, 0.6f), nA);
      for (int rr = 0; rr < 3; rr++) ring(P, b.x, b.y, b.r + 2.5f + rr, Color(hot.r, hot.g, hot.b, rr == 1 ? 1.0f : 0.85f), nA, 0, u);
      if (u > 0.02f && u < 0.999f) {
        const float a = -skp::kPi / 2 + 2 * skp::kPi * u;
        for (int rr = 1; rr <= 5; rr++) P.rect(std::floor(b.x + std::cos(a) * (b.r + rr)), std::floor(b.y + std::sin(a) * (b.r + rr)), 1, 1, Color(1, 1, 1, 0.95f));
      }
      for (int s = 1; s <= 3; s++)
        if (u >= s / 3.0f - 0.001f) {   // the steps reached: a bright node on the ring
          const float a = -skp::kPi / 2 + 2 * skp::kPi * (s / 3.0f) - 0.001f;
          P.rect(std::floor(b.x + std::cos(a) * (b.r + 3.5f)) - 1, std::floor(b.y + std::sin(a) * (b.r + 3.5f)) - 1, 3, 3, Color(1, 1, 1, 0.95f));
        }
      P.blitEx(v.light_, 0, 0, 64, 64, b.x - 20 - 6 * u, b.y - 20 - 6 * u, 40 + 12 * u, 40 + 12 * u, false, Color(hot.r, hot.g, hot.b, 0.18f + 0.3f * u), 1);
      // the level reached and the dual-cast, as badges up and to the left: clear of the thumb, which covers the button
      // and the screen below and right of it
      const int lvl = std::min(3, (int)std::floor(u * 3.0f + 0.001f));
      const float bx = std::floor(b.x - b.r - 26), by = std::floor(b.y - b.r - 16);
      auto badge = [&](float x, float y, const std::string& s, Color c) {
        const float w = (float)s.size() * 6 + 5;
        P.rect(x - 1, y - 1, w + 2, 13, Color(0.0f, 0.0f, 0.0f, 0.55f));
        P.rect(x, y, w, 11, Color(0.07f, 0.06f, 0.08f, 0.92f));
        P.rect(x, y, w, 1, Color(c.r, c.g, c.b, 0.9f));
        P.rect(x, y + 10, w, 1, Color(c.r * 0.5f, c.g * 0.5f, c.b * 0.5f, 0.9f));
        P.textS(x + 3, y + 2, s, 1, c);
      };
      if (lvl > 0) {
        for (int s = 0; s < 3; s++) P.rect(bx + 2 + s * 5, by + 13, 4, 3, s < lvl ? Color(hot.r, hot.g, hot.b, 1) : Color(0.15f, 0.14f, 0.18f, 0.85f));
        badge(bx, by, "+" + std::to_string(lvl), Color(hot.r, hot.g, hot.b));
      }
      if (S.holdDual) {   // dual: a second ring outside, and an X2 badge
        const Color hh = qCol(d->fx.ramp[4] ? d->fx.ramp[4] : 0xFFFFFFFFu);
        for (int rr = 0; rr < 2; rr++) ring(P, b.x, b.y, b.r + 7 + rr, Color(hh.r, hh.g, hh.b, rr ? 0.5f : 0.95f), 96);
        badge(bx + (lvl > 0 ? 18.0f : 0.0f), by, "X2", Color(1.0f, 0.95f, 0.8f));
      }
    }
  }
  // the sneak toggle: a lidded eye; lit violet while sneaking
  {
    const QuickBtn b = quickBtnPos(4);
    const bool on = g.skills.live.sneaking;
    const Color vc = qCol(skills::skillInfo(skills::Skill::Stealth).color);
    disc(P, b.x, b.y, b.r, Color(0.08f, 0.07f, 0.10f, on ? 0.7f : 0.42f));
    if (on) P.blitEx(v.light_, 0, 0, 64, 64, b.x - 14, b.y - 14, 28, 28, false, Color(vc.r, vc.g, vc.b, 0.35f), 1);
    ring(P, b.x, b.y, b.r, on ? Color(vc.r * 0.5f + 0.5f, vc.g * 0.5f + 0.5f, vc.b * 0.5f + 0.5f, 0.95f) : Color(0.9f, 0.85f, 0.75f, 0.45f), 40);
    const Tex& eye = tex(v, (0xA4ull << 56) | 1, [&]() { return skp::paintEye(true, skills::skillInfo(skills::Skill::Stealth).color); });
    P.blitEx(eye, 0, 0, 11, 7, b.x - 5, b.y - 3, 11, 7, false, on ? Color(1, 1, 1, 1) : Color(0.95f, 0.95f, 1.0f, 0.95f));
  }
}

float SkillsUiOps::drawQuickBar(View& v, Game& g, float x, float y) {
  Pix& P = *v.pix_;
  const magic::Book& B = g.arcana.book;
  for (int i = 0; i < magic::QUICK_SLOTS; i++) {
    const magic::SpellDef* d = slotSpell(g, i);
    const bool sel = i == B.sel && d;
    P.textS(x, y + 6, std::to_string(i + 1), 1, sel ? kGoldQ : kDimQ);
    const float cx = x + 6;
    P.rect(cx, y, 18, 18, Color(0.05f, 0.04f, 0.06f, 0.8f));
    P.frame(cx, y, 18, 18, sel ? kGoldQ : Color(0.42f, 0.34f, 0.22f, 0.9f));
    if (d) {
      const bool shortMp = g.mp < d->cost;
      P.blitEx(spellIcon(v, d->id), 0, 0, 16, 16, cx + 1, y + 1, 16, 16, false, shortMp ? Color(0.45f, 0.45f, 0.5f, 0.9f) : Color(1, 1, 1, 1));
      const float cd = g.arcana.live.cd[i];
      if (cd > 0 && d->cooldown > 0) {
        const float k = clampf(cd / d->cooldown, 0, 1);
        P.rect(cx + 1, y + 1 + 16 * (1 - k), 16, 16 * k, Color(0.02f, 0.02f, 0.05f, 0.65f));
      }
      if (shortMp) P.rect(cx + 1, y + 1, 16, 16, Color(0.05f, 0.05f, 0.14f, 0.35f));
    } else {
      P.rect(cx + 8, y + 9, 3, 1, Color(0.5f, 0.45f, 0.36f, 0.7f));
      P.rect(cx + 9, y + 8, 1, 3, Color(0.5f, 0.45f, 0.36f, 0.7f));
    }
    x += 27;
  }
  return x;
}

void SkillsUiOps::drawSneakEye(View& v, Game& g, float x, float y) {
  if (!g.skills.live.sneaking) return;
  Pix& P = *v.pix_;
  const Color vc = qCol(skills::skillInfo(skills::Skill::Stealth).color);
  const float k = 0.5f + 0.5f * std::sin(v.t_ * 2.5f);
  P.blitEx(v.light_, 0, 0, 64, 64, x - 6, y - 8, 23, 23, false, Color(vc.r, vc.g, vc.b, 0.18f + 0.14f * k), 1);
  const Tex& eye = tex(v, (0xA4ull << 56) | 1, [&]() { return skp::paintEye(true, skills::skillInfo(skills::Skill::Stealth).color); });
  P.blit(eye, std::floor(x), std::floor(y));
}
