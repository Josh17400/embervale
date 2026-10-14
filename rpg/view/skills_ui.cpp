// M9 "Arcana & Mastery" (VISION_PLAN 15.29.2, 15.30): the SKILLS page and the skill toasts. UI lane.
//
// The page (menu tab View::kSkillsTab), laid out from the menu box (skLay, shared by the draw and the taps):
//   - the group strip (COMBAT ARMOUR BODY CRAFT SOCIAL MAGIC), a gold pip on a group with a perk ready to take;
//   - the group's skills: a star in the skill's colour, its name, its level 1..100 and the bar toward the next level
//     (Track::xp / skills::xpToNext), a pip when one of its perks can be taken now;
//   - the skill's perk tree as a constellation in its own night sky (skills_paint.h: baked once per state), the stars
//     at PerkDef x / y, the lines to their parents, the figure of the skill behind them; live: the ready stars pulse,
//     a few stars twinkle, the selected star carries a gold reticle, a perk just taken bursts;
//   - beside it the skill's card: level and progress, what raises it, the weapon family's wins and losses against foe
//     tags (green / red), the armour class's trade-offs, perk points, the respec cost (respec itself is the SKILLS
//     lane's shrine and trainer dialogue);
//   - under the sky the selected perk: its name, what it needs, its plain-word line and TAKE (SkillOps::takePerk; a
//     blocked perk says why: skills::perkBlocked).
// Keys: W / S (up / down) the skill, A / D (left / right) the perk, Enter / Space take it; Q / E change tabs.
#include "rpg/view/skills_ui.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>
#include "rpg/sim/magic.h"
#include "rpg/sim/skill_ops.h"
#include "rpg/sim/skills.h"
#include "rpg/view/skills_paint.h"

namespace {
using skills::Skill;
const Color kGoldS(0.98f, 0.82f, 0.42f), kTextS(0.93f, 0.9f, 0.82f), kDimS(0.62f, 0.58f, 0.52f), kFrameS(0.55f, 0.45f, 0.28f);
const Color kUpS(0.46f, 0.96f, 0.46f), kDownS(1.0f, 0.46f, 0.38f), kWarnS(0.98f, 0.74f, 0.36f);
Color skCol(uint32_t c, float a = 1) { return Color((c & 255) / 255.0f, ((c >> 8) & 255) / 255.0f, ((c >> 16) & 255) / 255.0f, a); }
Color lift(Color c, float k) { return Color(c.r + (1 - c.r) * k, c.g + (1 - c.g) * k, c.b + (1 - c.b) * k, c.a); }

// the page's layout in the menu box (Pix::W / Pix::H are the box's while drawMenu / tap run)
struct SkLay {
  float gx, gy, gp, gh;          // the group strip: first chip x, y, pitch, height
  float lx, ly, lw, rowH;        // the skill list
  float sx, sy, sw, sh;          // the sky
  float ix, iy, iw, ih;          // the skill's card
  float cx, cy, cw, ch;          // the perk card
  float tbx, tby, tbw, tbh;      // TAKE
};
SkLay skLay(bool touch) {
  SkLay L;
  const float W = (float)Pix::W, H = (float)Pix::H;
  L.gx = 18; L.gy = 36; L.gh = touch ? 22.0f : 14.0f;
  L.gp = std::floor((W - 36 + 4) / 6.0f);
  L.lx = 18; L.ly = L.gy + L.gh + 6;
  L.lw = std::floor(std::clamp(W * 0.30f, 132.0f, 200.0f));
  L.rowH = touch ? 24.0f : 20.0f;
  L.iw = std::floor(std::clamp(W * 0.27f, 122.0f, 190.0f));
  L.ix = W - 18 - L.iw;
  L.sx = L.lx + L.lw + 8;
  L.sw = L.ix - 6 - L.sx;
  L.ch = touch ? 62.0f : 54.0f;
  L.sy = L.ly;
  L.sh = std::floor(H - 10 - L.ch - 4 - L.sy);
  L.iy = L.sy; L.ih = L.sh;
  L.cx = L.sx; L.cy = L.sy + L.sh + 4; L.cw = W - 18 - L.sx;
  L.tbw = touch ? 66.0f : 50.0f; L.tbh = touch ? 24.0f : 16.0f;
  L.tbx = L.cx + L.cw - L.tbw - 6; L.tby = L.cy + 6;
  return L;
}

std::vector<int> groupSkills(int grp) {
  std::vector<int> v;
  for (int i = 0; i < skills::SKILLS; i++) {
    if ((Skill)i == Skill::Lockpicking) continue;   // (M9 fixer) reserved: the world has no locks yet, so the page hides it
    if ((int)skills::skillInfo((Skill)i).group == grp) v.push_back(i);
  }
  return v;
}
// every skill in page order (by group, then the enum)
std::vector<int> pageOrder() {
  std::vector<int> v;
  for (int g = 0; g < (int)skills::Group::COUNT; g++) for (int i : groupSkills(g)) v.push_back(i);
  return v;
}
bool anyReady(const Game& g, Skill s) {
  int n = 0;
  skills::perkTable(s, n);
  if (g.skills.perkPts == 0) return false;
  for (int i = 0; i < n; i++) if (skp::perkState(g.skills, s, i) == skp::PS_READY) return true;
  return false;
}
bool groupReady(const Game& g, int grp) {
  for (int i : groupSkills(grp)) if (anyReady(g, (Skill)i)) return true;
  return false;
}
// the perk to show first: a ready one, else an open one, else the first
int defaultPerk(const Game& g, Skill s) {
  int n = 0;
  skills::perkTable(s, n);
  for (int want : {skp::PS_READY, skp::PS_OPEN})
    for (int i = 0; i < n; i++) if (skp::perkState(g.skills, s, i) == want) return i;
  return n > 0 ? 0 : -1;
}
void fixSel(const Game& g, View::SkillsUiState& S) {
  S.sel = std::clamp(S.sel, 0, skills::SKILLS - 1);
  S.group = (int)skills::skillInfo((Skill)S.sel).group;
  S.groupSel[S.group] = S.sel;
  int n = 0;
  skills::perkTable((Skill)S.sel, n);
  if (S.perkFor != S.sel || S.perkSel >= n) { S.perkSel = defaultPerk(g, (Skill)S.sel); S.perkFor = S.sel; }
}
uint64_t skyKey(Skill s, int w, int h, const std::vector<int>& st) {
  uint64_t k = 1469598103934665603ull;
  auto mix = [&](uint64_t v) { k ^= v; k *= 1099511628211ull; };
  mix((uint64_t)s); mix((uint64_t)w); mix((uint64_t)h);
  for (int v : st) mix((uint64_t)v + 7);
  return (0xA1ull << 56) | (k & 0x00FFFFFFFFFFFFFFull);
}
// a small diamond star (the list's gem, the toasts' star)
void gem(Pix& P, float x, float y, Color c) {
  x = std::floor(x); y = std::floor(y);
  P.rect(x + 2, y, 1, 5, c); P.rect(x, y + 2, 5, 1, c);
  P.rect(x + 1, y + 1, 3, 3, Color(c.r, c.g, c.b, c.a * 0.85f));
  P.rect(x + 2, y + 2, 1, 1, lift(c, 0.6f));
}
}  // namespace

// ------------------------------------------------------------------------------------------- shared helpers
const Tex& SkillsUiOps::spellIcon(View& v, uint32_t id) {
  return tex(v, (0xA2ull << 56) | id, [&]() {
    const magic::SpellDef* d = magic::spellById(id);
    if (d) return skp::paintSpellIcon(*d);
    magic::SpellDef none;
    return skp::paintSpellIcon(none);
  });
}
void SkillsUiOps::sayWhy(View& v, int page, const std::string& why) {
  View::SkillsUiState& S = st(v);
  S.why = why; S.whyT = v.t_; S.whyPage = page;
  v.audio_->play(Sfx::MenuBack);
}
void SkillsUiOps::drawBadge(View& v, float x, float y, int n) {
  if (n <= 0) return;
  Pix& P = *v.pix_;
  const std::string s = n > 9 ? "9" : std::to_string(n);
  x = std::floor(x); y = std::floor(y);
  const float pulse = 0.5f + 0.5f * std::sin(v.t_ * 4.0f);
  P.blitEx(v.light_, 0, 0, 64, 64, x - 8, y - 8, 20, 20, false, Color(1.0f, 0.75f, 0.3f, 0.35f + 0.25f * pulse), 1);
  P.rect(x - 1, y, 9, 7, Color(0.10f, 0.06f, 0.02f));   // a round-cornered gold disc 9 x 9 with the count
  P.rect(x, y - 1, 7, 9, Color(0.10f, 0.06f, 0.02f));
  P.rect(x, y, 7, 7, Color(0.95f, 0.72f, 0.28f));
  P.rect(x, y, 7, 1, Color(1.0f, 0.92f, 0.62f));
  P.rect(x, y, 1, 7, Color(1.0f, 0.86f, 0.5f));
  P.text(x + 1, y, s, 1, Color(0.18f, 0.08f, 0.02f));
}
void SkillsUiOps::toastDecor(View& v, const Toast& t, float x, float y, float w, float a) {
  Pix& P = *v.pix_;
  const int code = -t.rarity - 100, kind = code / 32;
  const bool bright = kind == 1 || kind == 6;
  gem(P, x - 8, y + 1, Color(t.c.r, t.c.g, t.c.b, a));
  if (bright && t.t < 1.1f) {   // the level / mastery glow: a soft bloom behind the star and a light sweeping the plate
    const float u = t.t / 1.1f, k = (1 - u) * a;
    P.blitEx(v.light_, 0, 0, 64, 64, x - 18, y - 8, 22, 22, false, Color(t.c.r, t.c.g, t.c.b, 0.8f * k), 1);
    const float sx = std::floor(x - 6 + (w + 16) * u);
    P.rectAdd(sx, y - 2, 3, 10, Color(t.c.r * 0.5f * k, t.c.g * 0.5f * k, t.c.b * 0.5f * k, 1));
    P.rectAdd(sx - 3, y - 2, 3, 10, Color(t.c.r * 0.22f * k, t.c.g * 0.22f * k, t.c.b * 0.22f * k, 1));
    if (kind == 6) P.blitEx(v.light_, 0, 0, 64, 64, x - 30, y - 16, w + 50, 38, false, Color(1.0f, 0.8f, 0.4f, 0.35f * k), 1);
  }
}

// ------------------------------------------------------------------------------------------- the toasts
// Ev::Skill: a = skill (COUNT: none), f = kind (1 level, 2 perk point, 3 perk taken, 4 book, 5 trained, 6 mastered,
// 7 respec), Event::spell = the level or the perk, s = the line. A toast in the skill's colour, marked by
// Toast::rarity = -(100 + kind * 32 + skill) (hud.cpp's drawToasts draws its star and glow: SkillsUiOps::toastDecor).
// A run of rises of the same skill updates one toast instead of stacking a column of them.
void View::skillEvent(Game& g, const Event& e) {
  if (e.s.empty()) return;
  View::SkillsUiState& S = SkillsUiOps::st(*this);
  const int kind = std::clamp((int)std::lround(e.f), 0, 7);
  const int sk = e.a >= 0 && e.a < skills::SKILLS ? e.a : skills::SKILLS;
  Toast t;
  t.s = e.s;
  if (kind == 2) t.s = g.skills.perkPts > 1 ? "PERK POINT TO SPEND (" + std::to_string((int)g.skills.perkPts) + ")" : "A PERK POINT TO SPEND";
  if (kind == 3) t.s = "PERK: " + e.s;
  const Color base = sk < skills::SKILLS ? skCol(skills::skillInfo((Skill)sk).color) : kGoldS;
  t.c = kind == 6 ? kGoldS : kind == 2 ? kGoldS : lift(base, 0.25f);
  t.rarity = -(100 + kind * 32 + sk);
  if (!toasts_.empty()) {
    Toast& last = toasts_.back();
    if (last.rarity == t.rarity && last.t < 2.5f) { last.s = t.s; last.t = std::min(last.t, 0.25f); t.rarity = 0; }
  }
  S.lastToast = t.rarity != 0 ? t.s : toasts_.back().s;
  if (t.rarity != 0) toasts_.push_back(t);
  S.toastsSkill++;
  if (kind == 1 || kind == 6) {
    S.glowT = t_; S.glowSkill = sk; S.glowMaster = kind == 6;
    audio_->play(kind == 6 ? Sfx::LevelUp : Sfx::Discover, kind == 6 ? 1.0f : 1.35f, kind == 6 ? 0.8f : 0.32f);
  }
  if (kind == 2) audio_->play(Sfx::Discover, 0.9f, 0.45f);
}

// ------------------------------------------------------------------------------------------- the page
void View::drawSkillsPage(Game& g) {
  Pix& P = *pix_;
  View::SkillsUiState& S = SkillsUiOps::st(*this);
  fixSel(g, S);
  const SkLay L = skLay(touchUI);
  const Skill sk = (Skill)S.sel;
  const skills::SkillInfo& inf = skills::skillInfo(sk);
  const Color sc = skCol(inf.color);

  // ---- the group strip
  for (int gi = 0; gi < (int)skills::Group::COUNT; gi++) {
    const float x = L.gx + gi * L.gp, w = L.gp - 4;
    const bool on = gi == S.group;
    P.rect(x, L.gy, w, L.gh, on ? Color(0.36f, 0.27f, 0.14f, 0.95f) : Color(0.13f, 0.11f, 0.10f, 0.95f));
    P.frame(x, L.gy, w, L.gh, on ? kGoldS : Color(0.40f, 0.33f, 0.22f));
    // the group's colour as a thread along the chip's foot (its first skill's star colour)
    const Color gc = skCol(skills::skillInfo((Skill)groupSkills(gi).front()).color);
    P.rect(x + 2, L.gy + L.gh - 3, w - 4, 1, Color(gc.r, gc.g, gc.b, on ? 0.9f : 0.45f));
    P.text(x + w / 2, L.gy + std::floor((L.gh - 8) / 2), skills::groupName((skills::Group)gi), 1, on ? Color(1, 0.95f, 0.8f) : kTextS, 1);
    if (groupReady(g, gi)) {
      const float k = 0.6f + 0.4f * std::sin(t_ * 5.0f);
      P.rect(x + w - 6, L.gy + 2, 3, 3, Color(1.0f, 0.82f, 0.36f, k));
    }
  }

  // ---- the skills of the group
  const std::vector<int> rows = groupSkills(S.group);
  for (size_t r = 0; r < rows.size(); r++) {
    const Skill s = (Skill)rows[r];
    const skills::SkillInfo& si = skills::skillInfo(s);
    const Color c = skCol(si.color);
    const float y = L.ly + r * L.rowH;
    const bool on = rows[r] == S.sel;
    const int lv = g.skills.level(s);
    if (on) {
      P.rect(L.lx, y, L.lw, L.rowH - 2, Color(0.30f, 0.22f, 0.12f, 0.85f));
      P.frame(L.lx, y, L.lw, L.rowH - 2, kGoldS);
    } else if (r & 1) {
      P.rect(L.lx, y, L.lw, L.rowH - 2, Color(0.16f, 0.12f, 0.08f, 0.35f));
    }
    const float ty = y + (touchUI ? 5.0f : 3.0f);
    gem(P, L.lx + 4, ty + 1, c);
    P.text(L.lx + 13, ty, si.name, 1, on ? lift(c, 0.55f) : kTextS);
    P.text(L.lx + L.lw - 5, ty, std::to_string(lv), 1, lv >= skills::MAX_LEVEL ? kGoldS : lift(c, 0.35f), 2);
    if (anyReady(g, s)) {   // a perk can be taken now: a small gold diamond before the level
      const float k = 0.55f + 0.45f * std::sin(t_ * 5.0f + (float)r);
      const float px = std::floor(L.lx + L.lw - 5 - 17 - 7), py = std::floor(ty + 1);
      P.rect(px + 1, py, 1, 5, Color(1, 0.84f, 0.4f, k)); P.rect(px, py + 1, 3, 3, Color(1, 0.84f, 0.4f, k));
    }
    // the bar toward the next level
    const float bx = L.lx + 13, bw = L.lw - 18, by = y + L.rowH - (touchUI ? 9.0f : 7.0f), bh = touchUI ? 3.0f : 2.0f;
    const float k = lv >= skills::MAX_LEVEL ? 1.0f : clampf(g.skills[s].xp / std::max(0.01f, skills::xpToNext(lv)), 0, 1);
    P.rect(bx, by, bw, bh, Color(0.05f, 0.04f, 0.06f, 0.9f));
    const Color fc = lv >= skills::MAX_LEVEL ? kGoldS : Color(c.r * 0.85f, c.g * 0.85f, c.b * 0.85f);
    P.rect(bx, by, std::floor(bw * k), bh, fc);
    P.rect(bx, by, std::floor(bw * k), 1, lift(fc, 0.45f));
  }
  if (!touchUI) P.text(L.lx, L.ly + rows.size() * L.rowH + 4, "W S SKILL  A D PERK", 1, Color(kDimS.r, kDimS.g, kDimS.b, 0.75f));

  // ---- the sky
  int n = 0;
  const skills::PerkDef* PD = skills::perkTable(sk, n);
  std::vector<int> state((size_t)std::max(1, n));
  for (int i = 0; i < n; i++) state[(size_t)i] = skp::perkState(g.skills, sk, i);
  if (n == 0) state.clear();
  const int sw = (int)L.sw, sh = (int)L.sh;
  const uint64_t key = skyKey(sk, sw, sh, state);
  // keep the cache small: skies unseen for a while go once there are many
  if (S.tex.size() > 40) {
    for (auto it = S.skyUsed.begin(); it != S.skyUsed.end();) {
      if (t_ - it->second > 4.0f && it->first != key) {
        auto tt = S.tex.find(it->first);
        if (tt != S.tex.end()) { P.destroy(tt->second); S.tex.erase(tt); }
        it = S.skyUsed.erase(it);
      } else ++it;
    }
  }
  S.skyUsed[key] = t_;
  const Tex& sky = SkillsUiOps::tex(*this, key, [&]() { return skp::paintSky(sw, sh, sk, PD, n, state.empty() ? nullptr : state.data()); });
  P.rect(L.sx - 2, L.sy - 2, L.sw + 4, L.sh + 4, Color(0.03f, 0.02f, 0.04f, 0.9f));
  P.blit(sky, L.sx, L.sy);
  P.frame(L.sx - 1, L.sy - 1, L.sw + 2, L.sh + 2, kFrameS);
  for (float cxx : {L.sx - 1, L.sx + L.sw - 1}) for (float cyy : {L.sy - 1, L.sy + L.sh - 1}) P.rect(cxx, cyy, 2, 2, Color(0.9f, 0.75f, 0.4f));
  // live: twinkles, the ready stars' pulse, the burst of a perk just taken, the reticle on the selected star
  const Tex& spark = SkillsUiOps::tex(*this, 0xA3ull << 56, []() { return skp::paintSpark(); });
  for (int k = 0; k < 6; k++) {
    const uint32_t h = skp::hsh(inf.glyphSeed, (uint32_t)k * 31u + 5u);
    const float x = L.sx + 6 + (float)(h % (uint32_t)std::max(1, sw - 12)), y = L.sy + 6 + (float)((h >> 11) % (uint32_t)std::max(1, sh - 12));
    const float a = std::pow(std::max(0.0f, std::sin(t_ * (0.9f + 0.15f * k) + (float)(h & 63))), 6.0f);
    if (a > 0.02f) P.blitEx(spark, 0, 0, 7, 7, std::floor(x - 3), std::floor(y - 3), 7, 7, false, Color(lift(sc, 0.6f).r, lift(sc, 0.6f).g, lift(sc, 0.6f).b, a * 0.9f), 1);
  }
  const std::vector<skp::PerkVis> pv = skp::perkLayout(PD, n, sw, sh);
  S.perksDrawn = n; S.starsReady = 0;
  for (int i = 0; i < n; i++) {
    const float x = L.sx + pv[(size_t)i].x, y = L.sy + pv[(size_t)i].y;
    if (state[(size_t)i] == skp::PS_READY) {
      S.starsReady++;
      const float k = 0.5f + 0.5f * std::sin(t_ * 4.0f + i);
      const float r = 9 + 4 * k;
      P.blitEx(light_, 0, 0, 64, 64, x + 0.5f - r, y + 0.5f - r, r * 2, r * 2, false, Color(sc.r, sc.g, sc.b, 0.30f + 0.35f * k), 1);
    }
    if (state[(size_t)i] == skp::PS_LOCKED && g.skills.level(sk) < PD[i].need) {
      // the level it needs under it, when nothing else stands there
      bool clear = true;
      for (int j = 0; j < n; j++)
        if (j != i && std::fabs((float)pv[(size_t)j].x - pv[(size_t)i].x) < 12 && pv[(size_t)j].y - pv[(size_t)i].y > 0 && pv[(size_t)j].y - pv[(size_t)i].y < 20) clear = false;
      if (clear && pv[(size_t)i].y + 16 < sh) P.text(x + 1, y + 8, std::to_string((int)PD[i].need), 1, Color(0.55f, 0.58f, 0.72f, 0.85f), 1);
    }
  }
  if (S.takenSkill == S.sel && S.takenIdx >= 0 && S.takenIdx < n && t_ - S.takenT < 0.8f) {
    const float u = (t_ - S.takenT) / 0.8f;
    const float x = L.sx + pv[(size_t)S.takenIdx].x + 0.5f, y = L.sy + pv[(size_t)S.takenIdx].y + 0.5f;
    const float r = 4 + 26 * u;
    for (int k = 0; k < 48; k++) {
      const float a = 2 * skp::kPi * k / 48;
      P.rect(std::floor(x + std::cos(a) * r), std::floor(y + std::sin(a) * r), 1, 1, Color(lift(sc, 0.5f).r, lift(sc, 0.5f).g, lift(sc, 0.5f).b, 1 - u));
    }
    P.blitEx(light_, 0, 0, 64, 64, x - 24, y - 24, 48, 48, false, Color(sc.r, sc.g, sc.b, 0.9f * (1 - u)), 1);
  }
  if (S.perkSel >= 0 && S.perkSel < n) {
    const float x = std::floor(L.sx + pv[(size_t)S.perkSel].x), y = std::floor(L.sy + pv[(size_t)S.perkSel].y);
    const float d = 7 + ((std::sin(t_ * 5.0f) > 0) ? 1.0f : 0.0f);
    for (int sx2 = -1; sx2 <= 1; sx2 += 2)
      for (int sy2 = -1; sy2 <= 1; sy2 += 2) {
        const float ex = x + sx2 * d, ey = y + sy2 * d;
        // an L-bracket: 3 px each way toward the star, with a dark shadow under it
        P.rect(ex - (sx2 > 0 ? 2 : 0) + 1, ey + 1, 3, 1, Color(0, 0, 0, 0.6f));
        P.rect(ex + 1, ey - (sy2 > 0 ? 2 : 0) + 1, 1, 3, Color(0, 0, 0, 0.6f));
        P.rect(ex - (sx2 > 0 ? 2 : 0), ey, 3, 1, kGoldS);
        P.rect(ex, ey - (sy2 > 0 ? 2 : 0), 1, 3, kGoldS);
      }
  }

  // ---- the skill's card
  {
    float y = L.iy;
    const float x = L.ix, w = L.iw;
    P.textS(x, y, inf.name, 1, lift(sc, 0.3f));
    y += 11;
    const int lv = g.skills.level(sk);
    const bool mastered = lv >= skills::MAX_LEVEL;
    P.text(x, y, "LEVEL " + std::to_string(lv), 1, kTextS);
    const float need = skills::xpToNext(lv);
    if (mastered) P.text(x + w, y, "MASTERED", 1, kGoldS, 2);
    else P.text(x + w, y, std::to_string((int)std::floor(g.skills[sk].xp)) + "/" + std::to_string((int)std::ceil(need)), 1, kDimS, 2);
    y += 10;
    const float k = mastered ? 1.0f : clampf(g.skills[sk].xp / std::max(0.01f, need), 0, 1);
    P.rect(x, y, w, 4, Color(0.05f, 0.04f, 0.06f, 0.9f));
    P.rect(x, y, std::floor(w * k), 4, mastered ? kGoldS : sc);
    P.rect(x, y, std::floor(w * k), 1, lift(mastered ? kGoldS : sc, 0.5f));
    y += 9;
    // the lines (tone 1 green, -1 red, 0 plain, 2 quiet; prio: the first to go when short of room is the highest)
    struct Row { std::string s; int tone, prio; bool gut; };
    std::vector<Row> rws;
    rws.push_back({std::string("RAISED BY: ") + inf.line, 2, 3, false});
    const int fam = (int)sk;
    if (fam <= (int)Skill::Unarmed) {
      rws.push_back({skills::familyTraits(sk).line, 0, 1, false});
      for (const skp::Line& l : skp::familyLines(sk)) {
        const size_t c = l.s.find(": ");
        rws.push_back({c == std::string::npos ? l.s : l.s.substr(0, c), l.tone, 0, true});
        if (c != std::string::npos) rws.push_back({l.s.substr(c + 2), 2, 2, true});
      }
    } else if (sk == Skill::LightArmour || sk == Skill::MediumArmour || sk == Skill::HeavyArmour) {
      const skills::Weight wc = sk == Skill::LightArmour ? skills::Weight::Light : sk == Skill::MediumArmour ? skills::Weight::Medium : skills::Weight::Heavy;
      rws.push_back({std::string("IN ") + skills::weightName(wc) + " ARMOUR:", 2, 1, false});
      for (const skp::Line& l : skp::tradeLines(wc, lv)) rws.push_back({l.s, l.tone, 0, true});
    } else if (sk == Skill::Heritage) {
      std::vector<std::pair<int, int>> mats;
      for (int m = 1; m < (int)Mat::COUNT; m++) if (g.skills.matFam[m] > 0) mats.push_back({g.skills.matFam[m], m});
      std::sort(mats.rbegin(), mats.rend());
      rws.push_back({mats.empty() ? "NO MAKE KNOWN WELL YET" : "FAMILIAR MAKES:", 2, 1, false});
      for (size_t i = 0; i < mats.size() && i < 4; i++) rws.push_back({std::string(matName((Mat)mats[i].second)) + " " + std::to_string(mats[i].first), 0, 0, false});
      if (!g.skills.cultFam.empty()) rws.push_back({"PEOPLES' WORK KNOWN: " + std::to_string((int)g.skills.cultFam.size()), 0, 1, false});
    } else if (skills::isMagic(sk)) {
      int known = 0;
      for (uint32_t id : g.arcana.book.known) if (const magic::SpellDef* d = magic::spellById(id)) if (magic::schoolSkill(d->school) == sk) known++;
      rws.push_back({"SPELLS KNOWN: " + std::to_string(known), 0, 0, false});
      const int tier = std::min(magic::TIERS - 1, lv / 25);
      rws.push_back({std::string("CASTS UP TO ") + magic::tierName(tier), 1, 0, false});
      if (tier < magic::TIERS - 1) rws.push_back({std::string(magic::tierName(tier + 1)) + " AT " + std::to_string((tier + 1) * 25), 2, 1, false});
    } else {
      rws.push_back({"DONE " + std::to_string((unsigned)g.skills.uses[(int)sk]) + " TIMES", 2, 1, false});
    }
    const float foot = L.iy + L.ih - 21;
    const int perFull = std::max(6, (int)((w + 1) / 6)), perGut = std::max(6, (int)((w - 10 + 1) / 6));
    auto linesOf = [&](const Row& r) { return lootp::wrap(r.s, r.gut ? perGut : perFull); };
    std::vector<char> on(rws.size(), 1);
    auto height = [&]() { float hh = 0; for (size_t i = 0; i < rws.size(); i++) if (on[i]) hh += 9.0f * linesOf(rws[i]).size(); return hh; };
    for (int pr = 3; pr >= 1 && height() > foot - y - 3; pr--)
      for (size_t i = 0; i < rws.size(); i++) if (rws[i].prio == pr) on[i] = 0;
    P.rect(x, y, w, 1, Color(0.4f, 0.32f, 0.2f, 0.7f));
    y += 4;
    for (size_t i = 0; i < rws.size(); i++) {
      if (!on[i]) continue;
      const Row& r = rws[i];
      const Color c = r.tone > 0 && r.tone != 2 ? kUpS : r.tone < 0 ? kDownS : r.tone == 2 ? kDimS : kTextS;
      const auto ls = linesOf(r);
      for (size_t k2 = 0; k2 < ls.size(); k2++) {
        if (y + 8 > foot) break;
        if (k2 == 0 && r.gut && (r.tone == 1 || r.tone == -1)) SkillsUiOps::arrow(*this, x, y, r.tone);
        P.text(x + (r.gut ? 10.0f : 0.0f), y, ls[k2], 1, c);
        y += 9;
      }
    }
    // the foot: the perk points (a gold count) and the respec's price
    P.rect(x, foot + 1, w, 1, Color(0.4f, 0.32f, 0.2f, 0.7f));
    const int pts = g.skills.perkPts;
    P.text(x, foot + 5, "PERK POINTS", 1, kDimS);
    P.textS(x + w, foot + 5, std::to_string(pts), 1, pts > 0 ? kGoldS : kDimS, 2);
    const int rc = SkillOps::respecCost(g, sk);
    const std::string rs = rc > 0 ? "RESPEC " + std::to_string(rc) + "G: PRIEST" : "RESPEC: PRIEST, TRAINER";
    P.text(x, foot + 14, lootp::fit(rs, w), 1, Color(kDimS.r, kDimS.g, kDimS.b, 0.8f));
  }

  // ---- the perk card
  P.rect(L.cx, L.cy, L.cw, L.ch, Color(0.05f, 0.045f, 0.06f, 0.92f));
  P.frame(L.cx, L.cy, L.cw, L.ch, Color(0.40f, 0.33f, 0.22f));
  if (n == 0 || S.perkSel < 0) {
    P.text(L.cx + 8, L.cy + 8, "NO PERKS IN THIS TREE YET", 1, kDimS);
  } else {
    const skills::PerkDef& d = PD[S.perkSel];
    const int ps = state[(size_t)S.perkSel];
    const float x = L.cx + 8, tw = L.cw - 16 - L.tbw - 8;
    P.textS(x, L.cy + 6, d.name, 1, lift(sc, 0.35f));
    // what it needs: the level (green met / red short), its parents, its cost
    std::string need = "NEEDS " + std::string(inf.name) + " " + std::to_string((int)d.need);
    const bool lvOk = g.skills.level(sk) >= d.need;
    std::string after;
    for (int pp : {(int)d.parent, (int)d.parent2})
      if (pp >= 0 && pp < n) after += (after.empty() ? "AFTER " : " + ") + std::string(PD[pp].name);
    float ny = L.cy + 17;
    const bool whyOn = S.whyPage == 1 && t_ - S.whyT < 2.6f && S.whySkill == S.sel && S.whyPerk == S.perkSel;
    if (whyOn) {
      const bool blink = t_ - S.whyT < 0.7f && std::fmod(t_ - S.whyT, 0.24f) < 0.12f;
      P.text(x, ny, lootp::fit(S.why, tw), 1, blink ? kGoldS : kWarnS);
    } else {
      P.text(x, ny, need, 1, ps == skp::PS_TAKEN || lvOk ? kUpS : kDownS);
      if (!after.empty()) {
        const float ax = x + P.textW(need, 1) + 8;
        bool parentsOk = (d.parent < 0 || g.skills.has(sk, d.parent)) && (d.parent2 < 0 || g.skills.has(sk, d.parent2));
        if (ax + P.textW(after, 1) <= x + tw) P.text(ax, ny, after, 1, parentsOk ? kUpS : kDownS);
      }
    }
    float ly = L.cy + 28;
    for (const std::string& l : lootp::wrap(d.line, std::max(8, (int)((tw + 1) / 6)))) {
      if (ly + 8 > L.cy + L.ch - 2) break;
      P.text(x, ly, l, 1, kTextS);
      ly += 9;
    }
    // TAKE (or TAKEN), and the cost under it
    if (ps == skp::PS_TAKEN) {
      const float bx = L.tbx, by = L.tby;
      P.rect(bx, by, L.tbw, L.tbh, Color(0.12f, 0.10f, 0.06f, 0.95f));
      P.frame(bx, by, L.tbw, L.tbh, Color(0.6f, 0.5f, 0.28f));
      P.text(bx + L.tbw / 2, by + std::floor((L.tbh - 7) / 2), "TAKEN", 1, kGoldS, 1);
    } else {
      const bool ready = ps == skp::PS_READY;
      if (ready) {
        const float k = 0.5f + 0.5f * std::sin(t_ * 4.0f);
        P.blitEx(light_, 0, 0, 64, 64, L.tbx - 10, L.tby - 10, L.tbw + 20, L.tbh + 20, false, Color(1.0f, 0.75f, 0.3f, 0.10f + 0.12f * k), 1);
        button(L.tbx, L.tby, L.tbw, L.tbh, "TAKE", true);
      } else {
        P.rect(L.tbx, L.tby, L.tbw, L.tbh, Color(0.12f, 0.10f, 0.09f, 0.9f));
        P.frame(L.tbx, L.tby, L.tbw, L.tbh, Color(0.32f, 0.27f, 0.2f));
        P.text(L.tbx + L.tbw / 2, L.tby + std::floor((L.tbh - 7) / 2), "TAKE", 1, Color(0.45f, 0.42f, 0.38f), 1);
      }
      const std::string cost = d.cost == 1 ? "1 POINT" : std::to_string((int)d.cost) + " POINTS";
      P.text(L.tbx + L.tbw / 2, L.tby + L.tbh + 4, cost, 1, g.skills.perkPts >= d.cost ? kDimS : kDownS, 1);
    }
  }
}

namespace {
void takeSelected(View& v, Game& g) {
  View::SkillsUiState& S = SkillsUiOps::st(v);
  if (S.perkSel < 0) return;
  std::string why;
  if (SkillOps::takePerk(g, (Skill)S.sel, S.perkSel, &why)) {
    S.takenT = SkillsUiOps::now(v); S.takenSkill = S.sel; S.takenIdx = S.perkSel;
    S.why.clear(); S.whyT = -100;
    SkillsUiOps::audio(v).play(Sfx::Discover, 0.8f, 0.8f);
  } else {
    SkillsUiOps::sayWhy(v, 1, why.empty() ? "CANNOT TAKE IT NOW" : why);
    S.whySkill = S.sel; S.whyPerk = S.perkSel;
  }
}
}  // namespace

void View::skillsPageKey(Game& g, int key) {
  View::SkillsUiState& S = SkillsUiOps::st(*this);
  fixSel(g, S);
  const std::vector<int> order = pageOrder();
  int at = 0;
  for (size_t i = 0; i < order.size(); i++) if (order[i] == S.sel) at = (int)i;
  int n = 0;
  skills::perkTable((Skill)S.sel, n);
  if (key == SDLK_UP || key == SDLK_W) { S.sel = order[(size_t)((at + (int)order.size() - 1) % (int)order.size())]; audio_->play(Sfx::MenuMove); }
  else if (key == SDLK_DOWN || key == SDLK_S) { S.sel = order[(size_t)((at + 1) % (int)order.size())]; audio_->play(Sfx::MenuMove); }
  else if ((key == SDLK_LEFT || key == SDLK_A) && n > 0) { S.perkSel = (S.perkSel - 1 + n) % n; audio_->play(Sfx::MenuMove); }
  else if ((key == SDLK_RIGHT || key == SDLK_D) && n > 0) { S.perkSel = (S.perkSel + 1) % n; audio_->play(Sfx::MenuMove); }
  else if (key == SDLK_RETURN || key == SDLK_SPACE) takeSelected(*this, g);
  fixSel(g, S);
}

void View::skillsPageTap(Game& g, Vec2 p) {
  View::SkillsUiState& S = SkillsUiOps::st(*this);
  fixSel(g, S);
  const SkLay L = skLay(touchUI);
  auto inR = [&](float x, float y, float w, float h) { return p.x >= x && p.y >= y && p.x < x + w && p.y < y + h; };
  for (int gi = 0; gi < (int)skills::Group::COUNT; gi++)
    if (inR(L.gx + gi * L.gp - 2, L.gy - 3, L.gp, L.gh + 5)) {
      const std::vector<int> v = groupSkills(gi);
      S.sel = S.groupSel[gi] >= 0 ? S.groupSel[gi] : v.front();
      fixSel(g, S);
      audio_->play(Sfx::MenuMove);
      return;
    }
  const std::vector<int> rows = groupSkills(S.group);
  for (size_t r = 0; r < rows.size(); r++)
    if (inR(L.lx, L.ly + r * L.rowH, L.lw, L.rowH)) { S.sel = rows[r]; fixSel(g, S); audio_->play(Sfx::MenuMove); return; }
  int n = 0;
  const skills::PerkDef* PD = skills::perkTable((Skill)S.sel, n);
  if (inR(L.tbx - 4, L.tby - 4, L.tbw + 8, L.tbh + 8) && S.perkSel >= 0) { takeSelected(*this, g); return; }
  if (inR(L.sx, L.sy, L.sw, L.sh)) {
    const std::vector<skp::PerkVis> pv = skp::perkLayout(PD, n, (int)L.sw, (int)L.sh);
    const float reach = touchUI ? 20.0f : 12.0f;
    int best = -1;
    float bd = reach * reach;
    for (int i = 0; i < n; i++) {
      const float dx = p.x - (L.sx + pv[(size_t)i].x), dy = p.y - (L.sy + pv[(size_t)i].y);
      if (dx * dx + dy * dy < bd) { bd = dx * dx + dy * dy; best = i; }
    }
    if (best >= 0) { S.perkSel = best; S.perkFor = S.sel; audio_->play(Sfx::MenuMove); }
    return;
  }
}

// ---- for the scripts (script_skillsui.cpp): where a perk's star / the TAKE button / a skill row is on screen (box
//      coordinates of the menu: the caller adds the box's origin)
bool skillsUiStarAt(bool touch, skills::Skill s, int idx, float& x, float& y) {
  const SkLay L = skLay(touch);
  int n = 0;
  const skills::PerkDef* PD = skills::perkTable(s, n);
  if (idx < 0 || idx >= n) return false;
  const std::vector<skp::PerkVis> pv = skp::perkLayout(PD, n, (int)L.sw, (int)L.sh);
  x = L.sx + pv[(size_t)idx].x; y = L.sy + pv[(size_t)idx].y;
  return true;
}
void skillsUiTakeAt(bool touch, float& x, float& y) {
  const SkLay L = skLay(touch);
  x = L.tbx + L.tbw / 2; y = L.tby + L.tbh / 2;
}
bool skillsUiRowAt(bool touch, skills::Skill s, float& x, float& y) {
  const SkLay L = skLay(touch);
  const std::vector<int> rows = groupSkills((int)skills::skillInfo(s).group);
  for (size_t r = 0; r < rows.size(); r++)
    if (rows[r] == (int)s) { x = L.lx + L.lw / 2; y = L.ly + r * L.rowH + L.rowH / 2; return true; }
  return false;
}
void skillsUiGroupAt(bool touch, int grp, float& x, float& y) {
  const SkLay L = skLay(touch);
  x = L.gx + grp * L.gp + (L.gp - 4) / 2; y = L.gy + L.gh / 2;
}
