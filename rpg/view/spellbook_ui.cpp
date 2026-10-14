// M9 "Arcana & Mastery" (VISION_PLAN 15.5, 15.29.3): the SPELLS page (menu tab View::kSpellsTab). UI lane.
//
//   - the four quick slots across the top (15.5: "a limited prepared-spell loadout ... 3-4 quick-cast buttons"): each
//     its spell's orb, name, cost and cooldown, the slot in focus framed in gold;
//   - the spells known, by school (the school's star colour), each with its orb, name, tier and cost, the slot it is
//     prepared in;
//   - the spell in focus: its orb at 2x, name, tier and school, what it costs and does, its flags (upcast by holding,
//     dual-cast by a swipe or the modifier, concentration), what it inflicts and leaves; PREPARE IN SLOT n and CLEAR
//     SLOT n through MagicOps::prepare (the loadout rule is magic's: a refusal is said on the card).
// Tomes are read from the pack (useItem); the page says so while the book is thin.
// Keys: 1-4 or A / D (left / right) the slot, W / S (up / down) the spell, Enter / Space prepare it in the slot,
// X / Backspace / Delete clear the slot; Q / E change tabs.
#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>
#include "rpg/sim/magic.h"
#include "rpg/sim/magic_ops.h"
#include "rpg/sim/skills.h"
#include "rpg/view/skills_paint.h"
#include "rpg/view/skills_ui.h"

namespace {
const Color kGoldB(0.98f, 0.82f, 0.42f), kTextB(0.93f, 0.9f, 0.82f), kDimB(0.62f, 0.58f, 0.52f), kWarnB(0.98f, 0.74f, 0.36f);
const Color kMana(0.55f, 0.72f, 1.0f), kUpB(0.46f, 0.96f, 0.46f), kDownB(1.0f, 0.46f, 0.38f);
Color bCol(uint32_t c, float a = 1) { return Color((c & 255) / 255.0f, ((c >> 8) & 255) / 255.0f, ((c >> 16) & 255) / 255.0f, a); }
Color liftB(Color c, float k) { return Color(c.r + (1 - c.r) * k, c.g + (1 - c.g) * k, c.b + (1 - c.b) * k, c.a); }

struct SpLay {
  float hy;                      // the heading row
  float qx, qy, qw, qh, qp;      // the quick slots: first x, y, width, height, pitch
  float lx, ly, lw, rowH;        // the list
  int rows;
  float dx, dy, dw, dh;          // the spell card
  float bx, by, bw, bh, bgap;    // PREPARE / CLEAR
};
SpLay spLay(bool touch) {
  SpLay L;
  const float W = (float)Pix::W, H = (float)Pix::H;
  L.hy = 37;
  L.qy = 48; L.qh = touch ? 44.0f : 36.0f;
  L.qx = 18; L.qp = std::floor((W - 36 + 6) / 4.0f); L.qw = L.qp - 6;
  L.lx = 18; L.ly = L.qy + L.qh + 8;
  L.lw = std::floor(std::clamp(W * 0.40f, 170.0f, 250.0f));
  L.rowH = touch ? 22.0f : 18.0f;
  L.rows = std::max(3, (int)((H - 10 - L.ly) / L.rowH));
  L.dx = L.lx + L.lw + 10; L.dy = L.ly; L.dw = W - 18 - L.dx; L.dh = H - 10 - L.dy;
  L.bh = touch ? 24.0f : 16.0f; L.bgap = 6;
  L.bw = std::floor((L.dw - L.bgap) / 2);
  L.bx = L.dx; L.by = L.dy + L.dh - L.bh;
  return L;
}
// the list's rows: a school heading (spell 0, school s) or a spell
struct LRow { uint32_t spell = 0; int school = -1; };
std::vector<LRow> listRows(const Game& g) {
  std::vector<LRow> v;
  for (int s = 0; s < magic::SCHOOLS; s++) {
    std::vector<const magic::SpellDef*> in;
    for (uint32_t id : g.arcana.book.known)
      if (const magic::SpellDef* d = magic::spellById(id)) if ((int)d->school == s) in.push_back(d);
    if (in.empty()) continue;
    std::sort(in.begin(), in.end(), [](const magic::SpellDef* a, const magic::SpellDef* b) { return a->tier != b->tier ? a->tier < b->tier : a->id < b->id; });
    v.push_back({0, s});
    for (const magic::SpellDef* d : in) v.push_back({d->id, s});
  }
  return v;
}
std::vector<uint32_t> spellOrder(const Game& g) {
  std::vector<uint32_t> v;
  for (const LRow& r : listRows(g)) if (r.spell) v.push_back(r.spell);
  return v;
}
Color schoolCol(int s) { return bCol(skills::skillInfo(magic::schoolSkill((magic::School)s)).color); }
std::string secs(float s) {
  if (s <= 0.001f) return "NONE";
  char b[24];
  if (s < 10 && std::fabs(s - std::round(s)) > 0.05f) std::snprintf(b, sizeof b, "%.1fS", (double)s);
  else std::snprintf(b, sizeof b, "%dS", (int)std::lround(s));
  return b;
}
void fixSpell(const Game& g, View::SkillsUiState& S) {
  S.slotSel = std::clamp(S.slotSel, 0, magic::QUICK_SLOTS - 1);
  const std::vector<uint32_t> o = spellOrder(g);
  if (o.empty()) { S.spellSel = 0; return; }
  if (std::find(o.begin(), o.end(), S.spellSel) == o.end()) {
    const uint32_t inSlot = g.arcana.book.slot[S.slotSel];
    S.spellSel = std::find(o.begin(), o.end(), inSlot) != o.end() ? inSlot : o.front();
  }
}
// the cooldown as a dark sweep over a w x h cell (clockwise from the top, k of it still to run)
void sweepRect(Pix& P, float x, float y, float w, float h, float k) {
  if (k <= 0) return;
  const float cx = x + w / 2, cy = y + h / 2;
  for (int yy = 0; yy < (int)h; yy++) {
    int run0 = -1;
    for (int xx = 0; xx <= (int)w; xx++) {
      bool in = false;
      if (xx < (int)w) {
        float a = std::atan2((x + xx + 0.5f) - cx, -((y + yy + 0.5f) - cy));
        if (a < 0) a += 2 * skp::kPi;
        in = a / (2 * skp::kPi) >= 1.0f - k;
      }
      if (in && run0 < 0) run0 = xx;
      if (!in && run0 >= 0) { P.rect(x + run0, y + yy, (float)(xx - run0), 1, Color(0.02f, 0.02f, 0.05f, 0.62f)); run0 = -1; }
    }
  }
}
}  // namespace

bool spellsUiPrepare(View& v, Game& g, int slot, uint32_t id) {
  View::SkillsUiState& S = SkillsUiOps::st(v);
  std::string why;
  if (!MagicOps::prepare(g, slot, id, &why)) { SkillsUiOps::sayWhy(v, 2, why.empty() ? "THE LOADOUT CANNOT CHANGE NOW" : why); return false; }
  S.preparedT = SkillsUiOps::now(v); S.preparedSlot = slot;
  S.why.clear(); S.whyT = -100;
  SkillsUiOps::audio(v).play(id ? Sfx::MenuSelect : Sfx::MenuBack);
  return true;
}

void View::drawSpellsPage(Game& g) {
  Pix& P = *pix_;
  View::SkillsUiState& S = SkillsUiOps::st(*this);
  fixSpell(g, S);
  const SpLay L = spLay(touchUI);
  const magic::Book& B = g.arcana.book;

  // ---- the heading: what the slots are, how to cast them
  P.text(L.qx, L.hy, "PREPARED SPELLS", 1, kGoldB);
  const std::string how = touchUI ? "TAP CAST  HOLD UPCAST  SWIPE OFF: DUAL" : "1-4 CAST  HOLD: UPCAST  ALT+KEY: DUAL";
  P.text((float)Pix::W - 18, L.hy, how, 1, kDimB, 2);

  // ---- the quick slots
  S.slotsDrawn = 0;
  for (int i = 0; i < magic::QUICK_SLOTS; i++) {
    const float x = L.qx + i * L.qp, y = L.qy;
    const bool on = i == S.slotSel;
    const uint32_t id = B.slot[i];
    const magic::SpellDef* d = id ? magic::spellById(id) : nullptr;
    const float flash = S.preparedSlot == i ? clampf(1 - (t_ - S.preparedT) / 0.6f, 0, 1) : 0.0f;
    P.rect(x, y, L.qw, L.qh, on ? Color(0.22f, 0.16f, 0.09f, 0.95f) : Color(0.08f, 0.07f, 0.08f, 0.95f));
    if (d) {   // a wash of the school's colour from the left
      const Color c = schoolCol((int)d->school);
      P.rect(x + 1, y + 1, 26, L.qh - 2, Color(c.r, c.g, c.b, 0.10f));
    }
    P.frame(x, y, L.qw, L.qh, on ? kGoldB : Color(0.40f, 0.33f, 0.22f));
    if (flash > 0) P.blitEx(light_, 0, 0, 64, 64, x - 8, y - 8, L.qw + 16, L.qh + 16, false, Color(1.0f, 0.8f, 0.4f, 0.5f * flash), 1);
    // the slot's number in a small plate on the top-left corner
    P.rect(x + 2, y + 2, 9, 9, Color(0.03f, 0.02f, 0.04f, 0.9f));
    P.text(x + 4, y + 3, std::to_string(i + 1), 1, on ? kGoldB : kDimB);
    const float iy = y + std::floor((L.qh - 16) / 2);
    if (d) {
      const Tex& ic = SkillsUiOps::spellIcon(*this, id);
      P.blit(ic, x + 14, iy);
      const float cd = g.arcana.live.cd[i];
      if (cd > 0 && d->cooldown > 0) sweepRect(P, x + 14, iy, 16, 16, clampf(cd / d->cooldown, 0, 1));
      const float tx = x + 34, tw = L.qw - 36;
      P.text(tx, y + std::floor(L.qh / 2) - 9, lootp::fit(d->name, tw), 1, on ? liftB(schoolCol((int)d->school), 0.55f) : kTextB);
      const bool short_ = g.mp < d->cost;
      P.text(tx, y + std::floor(L.qh / 2) + 2, std::to_string((int)std::lround(d->cost)) + " MP", 1, short_ ? kDownB : kMana);
      if (cd > 0) P.text(x + L.qw - 4, y + std::floor(L.qh / 2) + 2, secs(cd), 1, kDimB, 2);
      S.slotsDrawn++;
    } else {
      for (int k = 0; k < 16; k += 2) {   // an empty socket: a dotted ring where the orb would sit
        const float a = 2 * skp::kPi * k / 16;
        P.rect(std::floor(x + 22 + std::cos(a) * 7), std::floor(iy + 8 + std::sin(a) * 7), 1, 1, Color(0.5f, 0.45f, 0.36f, 0.7f));
      }
      P.text(x + 34, y + std::floor((L.qh - 7) / 2), "EMPTY", 1, Color(0.5f, 0.46f, 0.4f));
    }
  }

  // ---- the spells known
  const std::vector<LRow> rows = listRows(g);
  if (rows.empty()) {
    const float cy = L.ly + 30;
    P.textS((float)Pix::W / 2, cy, "YOU KNOW NO SPELLS YET", 1, kGoldB, 1);
    P.text((float)Pix::W / 2, cy + 14, "READ A SPELL TOME FROM YOUR PACK,", 1, kTextB, 1);
    P.text((float)Pix::W / 2, cy + 24, "OR FIND A TEACHER IN A TOWN", 1, kTextB, 1);
    return;
  }
  int selRow = 0;
  for (size_t r = 0; r < rows.size(); r++) if (rows[r].spell == S.spellSel) selRow = (int)r;
  if (selRow < S.spellScroll + 1) S.spellScroll = std::max(0, selRow - 1);
  if (selRow >= S.spellScroll + L.rows) S.spellScroll = selRow - L.rows + 1;
  S.spellScroll = std::clamp(S.spellScroll, 0, std::max(0, (int)rows.size() - L.rows));
  for (int r = 0; r < L.rows && S.spellScroll + r < (int)rows.size(); r++) {
    const LRow& row = rows[(size_t)(S.spellScroll + r)];
    const float y = L.ly + r * L.rowH;
    const Color c = schoolCol(row.school);
    if (!row.spell) {   // the school's heading: its name in its colour on a thin rule
      const std::string nm = magic::schoolName((magic::School)row.school);
      const float ty = y + L.rowH - 10;
      P.text(L.lx + 2, ty, nm, 1, liftB(c, 0.2f));
      P.rect(L.lx + 2 + P.textW(nm, 1) + 5, ty + 3, L.lw - P.textW(nm, 1) - 9, 1, Color(c.r, c.g, c.b, 0.4f));
      continue;
    }
    const magic::SpellDef* d = magic::spellById(row.spell);
    if (!d) continue;
    const bool on = row.spell == S.spellSel;
    if (on) { P.rect(L.lx, y, L.lw, L.rowH - 1, Color(0.30f, 0.22f, 0.12f, 0.85f)); P.frame(L.lx, y, L.lw, L.rowH - 1, kGoldB); }
    const float iy = y + std::floor((L.rowH - 1 - 16) / 2);
    P.blit(SkillsUiOps::spellIcon(*this, row.spell), L.lx + 3, iy);
    const int inSlot = B.slotOf(row.spell);
    if (inSlot >= 0) {   // prepared: its slot's number on the orb's corner
      P.rect(L.lx + 13, iy + 9, 8, 9, Color(0.10f, 0.07f, 0.02f, 0.95f));
      P.text(L.lx + 14, iy + 10, std::to_string(inSlot + 1), 1, kGoldB);
    }
    const float ty = y + std::floor((L.rowH - 1 - 7) / 2);
    P.text(L.lx + 25, ty, lootp::fit(d->name, L.lw - 25 - 64), 1, on ? liftB(c, 0.55f) : kTextB);
    P.text(L.lx + L.lw - 30, ty, magic::tierName(d->tier), 1, kDimB, 2);
    P.text(L.lx + L.lw - 4, ty, std::to_string((int)std::lround(d->cost)), 1, g.mp < d->cost ? kDownB : kMana, 2);
  }
  if ((int)rows.size() > L.rows) {   // more below / above: small chevrons at the list's right edge
    if (S.spellScroll > 0) P.text(L.lx + L.lw / 2, L.ly - 8, "...", 1, kDimB, 1);
    if (S.spellScroll + L.rows < (int)rows.size()) P.text(L.lx + L.lw / 2, L.ly + L.rows * L.rowH - 4, "...", 1, kDimB, 1);
  }

  // ---- the spell in focus
  const magic::SpellDef* d = magic::spellById(S.spellSel);
  if (!d) return;
  const Color c = schoolCol((int)d->school);
  P.rect(L.dx - 4, L.dy - 2, L.dw + 6, L.dh + 2, Color(0.04f, 0.035f, 0.05f, 0.6f));
  P.frame(L.dx - 4, L.dy - 2, L.dw + 6, L.dh + 2, Color(0.36f, 0.30f, 0.20f));
  float y = L.dy + 2;
  const float x = L.dx + 2, w = L.dw - 4;
  P.blitEx(light_, 0, 0, 64, 64, x - 10, y - 10, 52, 52, false, Color(c.r, c.g, c.b, 0.22f + 0.06f * std::sin(t_ * 2.0f)), 1);
  P.blitEx(SkillsUiOps::spellIcon(*this, d->id), 0, 0, 16, 16, x, y, 32, 32);
  const float tx = x + 38, tw = w - 38;
  const int nsc = P.textW(d->name, 2) <= tw ? 2 : 1;
  P.textS(tx, y + (nsc == 2 ? 1.0f : 4.0f), d->name, nsc, liftB(c, 0.45f));
  P.text(tx, y + 18, lootp::fit(std::string(magic::tierName(d->tier)) + " " + magic::schoolName(d->school), tw), 1, liftB(c, 0.15f));
  const skills::Skill ss = magic::schoolSkill(d->school);
  const int need = 25 * d->tier, have = g.skills.level(ss);
  P.text(tx, y + 27, lootp::fit(std::string(skills::skillInfo(ss).name) + " " + std::to_string(have) + (need > 0 ? "  NEEDS " + std::to_string(need) : ""), tw), 1,
         have >= need ? kDimB : kDownB);
  y += 40;
  // the numbers, two to a row
  std::vector<std::pair<std::string, std::string>> st;
  st.push_back({"COST", std::to_string((int)std::lround(d->cost)) + " MP"});
  st.push_back({"COOLDOWN", secs(d->cooldown)});
  st.push_back({"CAST", d->castTime > 0 ? secs(d->castTime) : std::string("INSTANT")});
  if (d->power > 0) st.push_back({d->form == magic::Form::Mend ? "HEALS" : d->form == magic::Form::Ward ? "ABSORBS" : "POWER", std::to_string((int)std::lround(d->power))});
  if (d->range > 0) st.push_back({"RANGE", std::to_string((int)std::lround(d->range / 16.0f)) + " TILES"});
  if (d->radius > 0) st.push_back({"RADIUS", std::to_string(std::max(1, (int)std::lround(d->radius / 16.0f))) + " TILES"});
  if (d->duration > 0) st.push_back({"LASTS", secs(d->duration)});
  // (M9 fixer, review: "the stats run together on phone: INSTANT POWER") each column's value ends a clear gutter short
  // of the next column, with a faint rule between the two columns and a dot leader from each label to its value
  const float colW = std::floor(w / 2);
  if (st.size() > 1) P.rect(x + colW - 5, y - 1, 1, (float)((st.size() + 1) / 2) * 9 - 1, Color(0.36f, 0.30f, 0.20f, 0.55f));
  for (size_t i = 0; i < st.size(); i++) {
    const float cx = x + (i & 1) * colW, cy = y + (i / 2) * 9;
    const float vr = (i & 1) ? x + w : cx + colW - 10;   // the value's right edge
    P.text(cx, cy, st[i].first, 1, kDimB);
    const float lx = cx + (float)P.textW(st[i].first, 1) + 3, rx = vr - (float)P.textW(st[i].second, 1) - 3;
    for (float dx = std::ceil(lx); dx < rx; dx += 3) P.rect(dx, cy + 5, 1, 1, Color(0.42f, 0.36f, 0.26f, 0.6f));
    P.text(vr, cy, st[i].second, 1, st[i].first == "COST" ? kMana : kTextB, 2);
  }
  y += ((st.size() + 1) / 2) * 9 + 3;
  // what it does besides: statuses, the surface it leaves, its flags
  std::vector<std::pair<std::string, Color>> fl;
  for (int s = 0; s < magic::STATUSES; s++)
    if (d->statuses & magic::statusBit((magic::Status)s)) fl.push_back({std::string("INFLICTS ") + magic::statusName((magic::Status)s), bCol(magic::statusColor((magic::Status)s))});
  if (d->leaves != magic::Surface::None) fl.push_back({std::string("LEAVES ") + magic::surfaceName(d->leaves), kTextB});
  if (d->upcast) fl.push_back({touchUI ? "UPCAST: HOLD ITS BUTTON" : "UPCAST: HOLD ITS KEY", kGoldB});
  if (d->dual) fl.push_back({touchUI ? "DUAL-CAST: SWIPE OFF IT" : "DUAL-CAST: ALT + ITS KEY", kGoldB});
  if (d->concentration) fl.push_back({"CONCENTRATION: BREAKS ON A HEAVY HIT", kWarnB});
  if (d->reaction) fl.push_back({"REACTION: ANSWERS A FOE'S CAST", kTextB});
  const float footY = L.by - 12;
  for (const auto& f : fl) {
    if (y + 8 > footY) break;
    P.rect(x + 1, y + 2, 3, 3, Color(f.second.r, f.second.g, f.second.b, 0.9f));
    P.text(x + 8, y, lootp::fit(f.first, w - 8), 1, f.second);
    y += 9;
  }
  // the refusal, or what the buttons will do
  const int slot = S.slotSel;
  const bool whyOn = S.whyPage == 2 && t_ - S.whyT < 2.6f;
  if (whyOn) {
    const bool blink = t_ - S.whyT < 0.7f && std::fmod(t_ - S.whyT, 0.24f) < 0.12f;
    P.text(x, footY, lootp::fit(S.why, w), 1, blink ? kGoldB : kWarnB);
  } else if (B.slot[slot] == d->id) {
    P.text(x, footY, "PREPARED IN SLOT " + std::to_string(slot + 1), 1, kUpB);
  }
  const bool already = B.slot[slot] == d->id;
  button(L.bx, L.by, L.bw, L.bh, already ? "PREPARED" : "PREPARE IN " + std::to_string(slot + 1), !already);
  if (B.slot[slot]) button(L.bx + L.bw + L.bgap, L.by, L.bw, L.bh, "CLEAR SLOT " + std::to_string(slot + 1), false);
}

void View::spellsPageKey(Game& g, int key) {
  View::SkillsUiState& S = SkillsUiOps::st(*this);
  fixSpell(g, S);
  const std::vector<uint32_t> o = spellOrder(g);
  int at = 0;
  for (size_t i = 0; i < o.size(); i++) if (o[i] == S.spellSel) at = (int)i;
  if (key >= SDLK_1 && key <= SDLK_4) { S.slotSel = key - SDLK_1; audio_->play(Sfx::MenuMove); return; }
  if (key == SDLK_LEFT || key == SDLK_A) { S.slotSel = (S.slotSel + magic::QUICK_SLOTS - 1) % magic::QUICK_SLOTS; audio_->play(Sfx::MenuMove); return; }
  if (key == SDLK_RIGHT || key == SDLK_D) { S.slotSel = (S.slotSel + 1) % magic::QUICK_SLOTS; audio_->play(Sfx::MenuMove); return; }
  if (o.empty()) return;
  if (key == SDLK_UP || key == SDLK_W) { S.spellSel = o[(size_t)((at + (int)o.size() - 1) % (int)o.size())]; audio_->play(Sfx::MenuMove); return; }
  if (key == SDLK_DOWN || key == SDLK_S) { S.spellSel = o[(size_t)((at + 1) % (int)o.size())]; audio_->play(Sfx::MenuMove); return; }
  if (key == SDLK_RETURN || key == SDLK_SPACE) { if (S.spellSel) spellsUiPrepare(*this, g, S.slotSel, S.spellSel); return; }
  if (key == SDLK_X || key == SDLK_BACKSPACE || key == SDLK_DELETE) { if (g.arcana.book.slot[S.slotSel]) spellsUiPrepare(*this, g, S.slotSel, 0); return; }
}

void View::spellsPageTap(Game& g, Vec2 p) {
  View::SkillsUiState& S = SkillsUiOps::st(*this);
  fixSpell(g, S);
  const SpLay L = spLay(touchUI);
  auto inR = [&](float x, float y, float w, float h) { return p.x >= x && p.y >= y && p.x < x + w && p.y < y + h; };
  for (int i = 0; i < magic::QUICK_SLOTS; i++)
    if (inR(L.qx + i * L.qp - 3, L.qy - 3, L.qp, L.qh + 6)) { S.slotSel = i; audio_->play(Sfx::MenuMove); return; }
  const std::vector<LRow> rows = listRows(g);
  for (int r = 0; r < L.rows && S.spellScroll + r < (int)rows.size(); r++) {
    const LRow& row = rows[(size_t)(S.spellScroll + r)];
    if (!row.spell) continue;
    if (inR(L.lx, L.ly + r * L.rowH, L.lw, L.rowH)) {
      // a second tap on the spell in focus prepares it in the slot in focus
      if (S.spellSel == row.spell) spellsUiPrepare(*this, g, S.slotSel, row.spell);
      else { S.spellSel = row.spell; audio_->play(Sfx::MenuMove); }
      return;
    }
  }
  if (S.spellSel && inR(L.bx, L.by - 3, L.bw, L.bh + 6)) { spellsUiPrepare(*this, g, S.slotSel, S.spellSel); return; }
  if (g.arcana.book.slot[S.slotSel] && inR(L.bx + L.bw + L.bgap, L.by - 3, L.bw, L.bh + 6)) { spellsUiPrepare(*this, g, S.slotSel, 0); return; }
}

// ---- for the scripts: the centre of a slot card, a known spell's row, PREPARE and CLEAR (menu box coordinates)
void spellsUiSlotAt(bool touch, int slot, float& x, float& y) {
  const SpLay L = spLay(touch);
  x = L.qx + slot * L.qp + L.qw / 2; y = L.qy + L.qh / 2;
}
bool spellsUiRowAt(const Game& g, bool touch, int scroll, uint32_t id, float& x, float& y) {
  const SpLay L = spLay(touch);
  const std::vector<LRow> rows = listRows(g);
  for (int r = 0; r < L.rows && scroll + r < (int)rows.size(); r++)
    if (rows[(size_t)(scroll + r)].spell == id) { x = L.lx + L.lw / 2; y = L.ly + r * L.rowH + L.rowH / 2; return true; }
  return false;
}
void spellsUiButtonAt(bool touch, bool clear, float& x, float& y) {
  const SpLay L = spLay(touch);
  x = L.bx + (clear ? L.bw + L.bgap : 0) + L.bw / 2; y = L.by + L.bh / 2;
}
