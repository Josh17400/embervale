// screens_ui.cpp: the full-screen moments: level-up, the title screen, the death screen, the title's taps and the
// touch buttons' hit test (View::buttonAt). Moved from hud.cpp (SOLID 1.4).
#include "rpg/view/hud_layout.h"

using namespace hudlay;

void View::drawLevelUp(Game& g) { menuTab_ = 3; drawMenu(g); }

void View::drawTitle(Game& g, bool hasSave) {
  Pix& P = *pix_;
  (void)g;
  P.rect(0, 0, Pix::W, Pix::H, Color(0.02f, 0.02f, 0.05f, 0.35f));
  // embers rising over the whole screen
  for (int i = 0; i < 30 + Pix::W / 16; i++) {
    float ex = std::fmod(hashf(i, 0, 3) * Pix::W + std::sin(t_ + i) * 8 + Pix::W, (float)Pix::W);
    float ey = Pix::H - std::fmod(t_ * (12 + hashf(i, 1, 3) * 20) + hashf(i, 2, 3) * Pix::H, (float)Pix::H);
    P.rectAdd(ex, ey, 1, 1, Color(1, 0.5f, 0.2f, 0.6f));
  }
  if (settingsOpen_) return;   // the settings screen is drawn over the drifting world instead of the title
  const UiBox box = uiBox(270);
  P.pushBox(box.x, box.y, box.w, box.h);
  const float oy = std::floor((Pix::H - 270) / 2.0f);   // (M2) the box is the whole safe area: centre the 270-tall layout
  float y = 58 + oy;
  P.popBox();
  // soft dark bands behind the logo block and the foot line, so the words read over any village (screen-wide, eased)
  auto band = [&](float cy, float hh, float a) {
    for (int k = 0; k < (int)hh; k++) {
      const float d = std::fabs(k - hh / 2) / (hh / 2);
      P.rect(0, box.y + cy - hh / 2 + k, Pix::W, 1, Color(0.02f, 0.015f, 0.03f, a * (1 - d * d)));
    }
  };
  band(y + 26, 100, 0.55f);
  band((float)box.h - 11, 26, 0.6f);
  P.pushBox(box.x, box.y, box.w, box.h);
  // ember glow behind the logo
  P.blitEx(sprites_.light_, 0, 0, 64, 64, Pix::W / 2 - 160, y - 50, 320, 120, false, Color(1, 0.45f, 0.15f, 0.35f + 0.1f * std::sin(t_ * 2)), 1);
  const std::string title = "EMBERVALE";
  for (int k = 3; k >= 1; k--) P.text(Pix::W / 2 + k, y + k, title, 5, Color(0.12f, 0.04f, 0.02f, 0.5f), 1);
  P.text(Pix::W / 2, y, title, 5, Color(1, 0.86f, 0.55f), 1);
  P.text(Pix::W / 2, y + 2, title, 5, Color(1, 0.7f, 0.35f, 0.35f), 1);
  P.textS(Pix::W / 2, y + 44, "A SAGA OF STEEL, SORCERY AND DRAGONFIRE", 1, Color(0.95f, 0.9f, 0.8f), 1);
  // CONTINUE (with a save), NEW ADVENTURE, SETTINGS: the same rows titleTap() and the keys use
  const int n = titleItems();
  titleSel_ = std::clamp(titleSel_, 0, n - 1);
  float by = (hasSave ? 138.0f : 150.0f) + oy;
  if (hasSave) { button(Pix::W / 2 - 70, by, 140, 22, "CONTINUE", titleSel_ == 0); by += 28; }
  button(Pix::W / 2 - 70, by, 140, 22, "NEW ADVENTURE", titleSel_ == (hasSave ? 1 : 0));
  by += 28;
  button(Pix::W / 2 - 50, by, 100, 20, "SETTINGS", titleSel_ == n - 1);
  if (!titleNote.empty()) P.textS(Pix::W / 2, by + 28, titleNote, 1, Color(1, 0.8f, 0.45f), 1);
  P.textS(Pix::W / 2, Pix::H - 14, "EVERY WORLD IS UNIQUE  -  PROCEDURALLY GENERATED", 1, Color(0.8f, 0.75f, 0.68f, 0.9f), 1);
  P.popBox();
}

void View::drawDead(Game& g) {
  Pix& P = *pix_;
  // (P2a, STYLE_P2 13) the hero's fall plays UNDER the death screen's fade, on view time (modeT_ = seconds since the
  // death): the tint deepens over 0.8 s, the band and the words come up only as he lands (full at 0.55 s, after the
  // lying frame at about 0.35 s), and the words sit ABOVE his head with the prompt below his feet, so the body stays in
  // view between them, never under the band.
  auto ease = [](float x) { x = clampf(x, 0, 1); return x * x * (3 - 2 * x); };
  const float t = modeT_;
  const float tintA = ease(t / 0.8f), bandA = ease((t - 0.15f) / 0.40f), wordA = ease((t - 0.25f) / 0.30f);
  P.rect(0, 0, Pix::W, Pix::H, Color(0.15f, 0, 0, 0.5f * tintA));
  // the hero on the UI layer: his feet (the pivot) and the top of a standing figure (rig 2 stands about 40 world px)
  const float upw = uiPerWorld();
  const Vec2 hp = toUi(g.pl().p);
  const float headY = hp.y - 42.0f * upw, feetY = hp.y + 8.0f * upw;
  const float top = (float)Pix::ST, bot = (float)(Pix::H - Pix::SB);
  // the words' centre line: above the head with the band's soft lower edge just clearing it; on a screen too short for
  // that (or the hero pinned high by the map's edge) they go below his feet instead
  constexpr int kBand = 56;   // the band's rows (the words are 21 rows tall at size 3)
  float wy = std::floor(std::min(headY - kBand / 2.0f + 4.0f, (top + bot) / 2.0f - 40.0f));
  bool below = false;
  if (wy - kBand / 2.0f < top) { wy = std::floor(feetY + kBand / 2.0f + 2.0f); below = true; }
  if (bandA > 0)
    for (int k = 0; k < kBand; k++) {   // a dark band behind the words, eased at its edges
      const float d = std::fabs(k - kBand / 2.0f) / (kBand / 2.0f);
      P.rect(0, wy - kBand / 2.0f + k, Pix::W, 1, Color(0.06f, 0, 0, 0.6f * bandA * (1 - d * d)));
    }
  if (wordA > 0) {
    const float rise = std::floor((1 - wordA) * 4.0f);   // the words settle 4 px upward as they come up
    const float ty = wy - 10 + rise;
    P.text(Pix::W / 2 + 2, ty + 2, "YOU HAVE FALLEN", 3, Color(0, 0, 0, 0.6f * wordA), 1);
    P.text(Pix::W / 2, ty, "YOU HAVE FALLEN", 3, Color(0.9f, 0.25f, 0.2f, wordA), 1);
  }
  // the prompt (and input) waits a beat, so mashing attack as you fall doesn't skip the moment; it sits under the body
  // (or under the words when they had to go below)
  if (modeT_ > 1.2f) {
    const float py = std::floor(std::min(below ? wy + kBand / 2.0f + 4.0f : feetY + 10.0f, bot - 14.0f));
    P.textS(Pix::W / 2, py, touchUI ? "TAP TO RISE AGAIN" : "PRESS ENTER TO RISE AGAIN", 1,
            Color(kText.r, kText.g, kText.b, clampf((modeT_ - 1.2f) * 2, 0, 1)), 1);
  }
}

void View::titleTap(Vec2 p) {
  if (settingsOpen_) { settingsTap(p); return; }
  const UiBox box = uiBox(270);
  p = inBox(box, p);
  if (std::fabs(p.x - box.w / 2.0f) > 80) return;
  // the rows drawTitle lays out: 28 px apart from 138 (with a save) or 150; each owns its whole pitch
  const float by = (hasSave_ ? 138.0f : 150.0f) + std::floor((box.h - 270) / 2.0f);
  const int row = (int)std::floor((p.y - (by - 3)) / 28.0f);
  if (row < 0 || row >= titleItems()) return;
  titleSel_ = row;
  if (row == titleItems() - 1) { openSettings(); audio_->play(Sfx::MenuSelect); }
  else if (hasSave_ && row == 0) wantContinue = true;
  else wantNewGame = true;
}

int View::buttonAt(Vec2 p) const {
  for (int b = 0; b < B_COUNT; b++) {
    if (b == B_SPELL) continue;   // (M9) replaced by the quick-cast buttons
    const BtnDef d = btnPos(b);
    float r = d.r + 6;
    if (len2(p - Vec2(d.x, d.y)) < r * r) return b;
  }
  return -1;
}
