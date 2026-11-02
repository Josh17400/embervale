// dialogue_ui.cpp: the dialogue panel (View::drawDialogue): the speech, the speaker's portrait and the paged
// options. Its layout (dlgLay) is in hud_layout.h, shared with input.cpp's keys and taps. Moved from hud.cpp (SOLID 1.4).
// (P4) also the small HUD notes the over-long hud.cpp and render.cpp call (FxState's statics, fx_state.h): the gift strip
// under the speaker's name (item 3), the danger banner on entering a dungeon (6a) and the menu page's key hint (6b).
#include <cctype>
#include "rpg/view/hud_layout.h"

using namespace hudlay;

namespace {
// (P4 item 3) the gift strip's words: "+ OLD BLADE", "+ MINOR HEALTH POTION" (the toasts' own words), flowed into lines
// of the speech column's width, two spaces apart; at most two lines (the toast list keeps five)
struct GiftWord { const Toast* t; float x; int line; };
std::vector<GiftWord> giftFlow(const View& v, float width) {
  std::vector<GiftWord> out;
  float x = 0;
  int line = 0;
  for (const Toast* t : FxState::dlgGifts(v)) {
    const float w = (float)t->s.size() * 6;
    if (x > 0 && x + w > width) { x = 0; line++; }
    if (line > 1) break;
    out.push_back({t, x, line});
    x += w + 12;
  }
  return out;
}
float giftStripH(const View& v, float width) {
  const std::vector<GiftWord> f = giftFlow(v, width);
  return f.empty() ? 0.0f : (float)(f.back().line + 1) * 9 + 1;
}
}  // namespace

void View::drawDialogue(Game& g) {
  Pix& P = *pix_;
  if (settingsOpen_) return;
  const UiBox box = uiBox(300);
  P.pushBox(box.x, box.y, box.w, box.h);
  struct Pop { Pix& p; ~Pop() { p.popBox(); } } pop{P};
  const Dialogue& d = g.dlg;
  const float rowH = dlgRowH();
  g_dlgGiftH = giftStripH(*this, dlgLay(g, rowH, dlgFirst_).tw);   // (P4) before every layout below (and the taps')
  int nOpt = (int)dlgShown(d).size();
  dlgSel_ = std::clamp(dlgSel_, 0, std::max(0, nOpt - 1));
  {   // the page follows the selection (keyboard), and a page past the list's end starts over
    const DlgLay L0 = dlgLay(g, rowH, dlgFirst_);
    if (!L0.more) dlgFirst_ = 0;
    else if (dlgFirst_ >= nOpt) dlgFirst_ = 0;
  }
  const DlgLay L = dlgLay(g, rowH, dlgFirst_);
  const float x = L.x, w = L.w, y = L.y;
  const float sy = y + 20 + L.giftH;   // (P4) the speech's first line, under the gift strip
  panel(x, y, w, L.h, 0.94f);
  if (L.portrait) {
    // the speaker on a small lit stage, framed like the inventory's icons
    const float px = x + 8, py = y + 8, pw = 48, ph = std::min(58.0f, L.optTop.empty() ? L.h - 16 : L.optsTop - py - 6);
    P.rect(px, py, pw, ph, Color(0.03f, 0.025f, 0.04f, 0.9f));
    P.blitEx(sprites_.light_, 0, 0, 64, 64, px - 8, py - 6, pw + 16, ph + 10, false, Color(1, 0.72f, 0.4f, 0.22f), 1);
    for (const Actor& a : g.actors)
      if (a.id == d.actor) {
        // (P2c SWITCH) rig 2: the speaker's doll frame (idle, facing down; the frame centred, a shield or a blade inside the stage) at 1x or 2x: the whole figure when the stage
        // holds it at that multiple, else from the head down (never past the frame), its living finish on it
        const MenuStage dst{px + 1, py + 1, pw - 2, ph - 2};
        bool v2 = false;
        if (art::rigVersion() >= 2 && art::rig2Covers(a.look)) {
          sprites_.rigBakeNow_ = true;
          const RigTex lt = sprites_.rigTex(*this, a.look, art::RigPart::Loco);
          sprites_.rigBakeNow_ = false;
          const art::FrameRect* f0 = lt.ready() ? lt.sheet->frame(0, art::Clip::Idle, 0, 0) : nullptr;
          if (f0) {
            const int dsc = f0->w * 2 <= (int)pw - 4 ? 2 : 1;
            const int rows = std::min<int>(f0->h, (int)std::floor((ph - 4) / dsc));
            // the frame's top on the stage's top edge + 1 when cut (drawMenuDoll cuts it at the stage), else the feet
            // 1 px over its floor
            const float feetY = rows < f0->h ? py + 2 + (float)f0->py * dsc : py + ph - 2 - (float)(f0->h - f0->py) * dsc;
            if (rows > 4) v2 = drawMenuDoll(a.look, kMenuDialogue, 0, MenuGait::Idle, std::floor(px + pw / 2 - (f0->w / 2.0f - f0->px) * dsc), feetY, dsc, dst, 1.0f, 1, a.id);
          }
        }
        if (!v2) {
        const Tex& t = sprites_.humanTex(*this, a.look);
        const float sc = 2.0f, hw = art::HUMAN_W * sc;
        // (M5) a short stage (a long option list) shows the speaker from the head down, never past the frame
        const float srcH = std::min((float)art::HUMAN_H, std::floor((ph - 4) / sc)), hh = srcH * sc;
        if (srcH > 4) P.blitEx(t, art::HUMAN_W, 0, art::HUMAN_W, srcH, px + (pw - hw) / 2, py + ph - hh - 2, hw, hh);
        if (srcH > 4) noteMenuV1(kMenuDialogue, px + (pw - hw) / 2, py + ph - hh - 2, (int)hw, (int)hh, dst);
        }
      }
    P.frame(px, py, pw, ph, Color(0.55f, 0.45f, 0.28f));
    P.frame(px + 1, py + 1, pw - 2, ph - 2, Color(0.22f, 0.18f, 0.12f));
  }
  P.text(L.tx, y + 8, g.speakerTitle(), 1, kGold);   // (M7b) "SUSUT, INNKEEPER" once introduced, "THE INNKEEPER" before
  for (const GiftWord& gw : giftFlow(*this, L.tw)) {   // (P4 item 3) the gifts said here, each in its rarity's colour
    const float gx = L.tx + gw.x, gy = y + 18 + gw.line * 9;
    if (gw.t->rarity > 0) loot_.rarityText(*this, gx, gy, gw.t->s, (Rarity)gw.t->rarity, 1, 0, true);
    else { P.text(gx + 1, gy + 1, gw.t->s, 1, Color(0, 0, 0, 0.6f)); P.text(gx, gy, gw.t->s, 1, gw.t->c); }
  }
  {   // the speech, typed out
    int shown = 0;
    const int maxLines = std::max(2, (int)((L.optTop.empty() ? L.h - 30 : L.optsTop - 10 - sy) / 9));
    for (size_t i = 0; i < L.lines.size() && (int)i < maxLines; i++) {
      if (shown >= (int)dlgChars_) break;
      std::string s = L.lines[i];
      if (shown + (int)s.size() > (int)dlgChars_) s = s.substr(0, (size_t)std::max(0, (int)dlgChars_ - shown));
      shown += (int)L.lines[i].size();
      P.text(L.tx, sy + i * 9, s, 1, kText);
    }
  }
  // the options stay dimmed until the line has finished typing (a tap meanwhile only skips the text, see tap())
  const bool typing = dlgChars_ < L.total;
  P.rect(x + 8, L.optTop.empty() ? y : L.optsTop - 5, w - 16, 1, Color(0.4f, 0.32f, 0.2f, 0.6f));
  for (int i = L.first; i < L.first + L.count && i < nOpt; i++) {
    const float top = L.optTop[(size_t)i], oh = L.optH[(size_t)i];
    const bool hot = i == dlgSel_ && !typing;
    Color tc = typing ? Color(0.45f, 0.42f, 0.37f) : hot ? Color(1, 0.95f, 0.8f) : Color(0.8f, 0.75f, 0.65f);
    if (typing) { if (touchUI) P.rect(x + 8, top + 1, w - 16, oh - 2, Color(0.12f, 0.09f, 0.06f, 0.4f)); }
    else if (hot) P.rect(x + 8, top + 1, w - 16, oh - 2, Color(0.32f, 0.24f, 0.12f, 0.85f));
    else if (touchUI) P.rect(x + 8, top + 1, w - 16, oh - 2, Color(0.16f, 0.12f, 0.08f, 0.55f));   // a full-width band
    const auto& ol = L.optLines[(size_t)i];
    const float ty = top + std::floor((oh - 7 - (ol.size() - 1) * 9.0f) / 2);
    for (size_t k = 0; k < ol.size(); k++) P.text(x + 26, ty + k * 9, ol[k], 1, tc);
    if (hot) P.text(x + 14, ty, ">", 1, kGold);
  }
  if (L.more) {   // (M5) the page turn: where the list goes on, and how far along it this page is
    const float top = L.moreTop, oh = L.moreH;
    const Color mc = typing ? Color(0.45f, 0.42f, 0.37f) : kGold;
    if (touchUI) P.rect(x + 8, top + 1, w - 16, oh - 2, Color(0.10f, 0.08f, 0.05f, typing ? 0.4f : 0.7f));
    const float ty = top + std::floor((oh - 7) / 2);
    const bool last = L.first + L.count >= nOpt;
    P.text(x + 26, ty, last ? "BACK TO THE START..." : "MORE...", 1, mc);
    const std::string pg = std::to_string(L.first + 1) + "-" + std::to_string(L.first + L.count) + " OF " + std::to_string(nOpt);
    P.text(x + w - 14, ty, pg, 1, Color(0.6f, 0.55f, 0.45f), 2);
  }
}

// ------------------------------------------------------------------ (P4) FxState's HUD notes (fx_state.h)
const FxState& FxState::of(const View& v) { return v.fx_; }

std::vector<const Toast*> FxState::dlgGifts(const View& v) {
  std::vector<const Toast*> out;
  for (const Toast& t : v.fx_.toasts_) if (t.dlg) out.push_back(&t);
  return out;
}

// (P4 6a, VISION_PLAN 7.1/7.5: danger by place, never scaled to the hero) entering a cave, ruin or shrine five or more
// levels above the hero: "DANGEROUS GROUND" / "TOMB OF KEE: LEVEL 7 - YOU ARE LEVEL 2" (red, hud.cpp's banner); a name
// too long for the big line goes up beside the heading
void FxState::dangerBanner(View& v, const Game& g) {
  const int dl = dungeonLevel(g);
  if (dl < 0 || dl - g.plLevel < 5) return;
  std::string name = g.world.sites[(size_t)g.subSite].name;
  for (char& ch : name) ch = (char)std::toupper((unsigned char)ch);
  const std::string lv = "LEVEL " + std::to_string(dl) + " - YOU ARE LEVEL " + std::to_string(g.plLevel);
  FxState& f = v.fx_;
  f.banner_ = "DANGEROUS GROUND";
  f.bannerSub_ = name + ": " + lv;
  if (v.pix_->textW(f.bannerSub_, 2) > Pix::W - 16) { f.banner_ += "  -  " + name; f.bannerSub_ = lv; }
  f.bannerT_ = 4.0f;
  v.realm_.bannerState_.clear();
}

// (P4 6b) the page's keys at the panel's foot, keyboard only (touch keeps its buttons): "Q/E TABS" and the page's own
void FxState::drawMenuHint(View& v, int tab) {
  const char* h = menuHint(tab, v.touchUI);
  if (!*h) return;
  Pix& P = *v.pix_;
  const float y = (float)Pix::H - 15, w = (float)P.textW(h, 1);
  P.rect(std::floor(Pix::W / 2.0f - w / 2 - 4), y - 2, w + 8, 10, Color(0.05f, 0.04f, 0.06f, 0.92f));
  P.text(Pix::W / 2.0f, y, h, 1, kDim, 1);
}
