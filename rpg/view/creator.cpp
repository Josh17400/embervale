// The character creator (Mode::Creator): looks, name and background, then Game::finishCreator(). M0 hero lane.
// Everyone starts with just the shirt on their back (VISION_PLAN 15.1): the preview is the look the game starts
// with, and a background grants a trait, never gear.
//
// Layout (480x270 logical): the animated preview on the left, three tabs on the right (LOOKS, NAME, ORIGIN),
// RANDOMIZE and BEGIN along the bottom. Every touch target is at least 24 px tall.
// Keys: Tab / 1-3 switch tabs (1-3 not on NAME), Up/Down pick a row, Left/Right change it, R randomizes,
// [ and ] turn the preview, Enter goes to the next tab and BEGINs on ORIGIN, Escape returns to the title.
// On NAME the letters type, Backspace deletes and Up/Down suggest a name.
#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>
#include "rpg/view/view.h"

namespace {
const Color kGoldC(0.98f, 0.82f, 0.42f), kTextC(0.93f, 0.9f, 0.82f), kDimC(0.62f, 0.58f, 0.52f);

const uint32_t kSkins[8] = {rgba(250, 220, 190), rgba(242, 204, 166), rgba(236, 188, 146), rgba(214, 160, 116),
                            rgba(190, 134, 94),  rgba(160, 104, 70),  rgba(124, 80, 56),   rgba(92, 60, 46)};
const uint32_t kHairCols[10] = {rgba(36, 30, 34),  rgba(70, 44, 30),   rgba(120, 70, 36),   rgba(150, 64, 36),  rgba(200, 104, 44),
                                rgba(220, 180, 100), rgba(232, 220, 182), rgba(150, 148, 150), rgba(228, 226, 218), rgba(120, 50, 110)};
const uint32_t kEyes[7] = {0, rgba(96, 58, 32), rgba(60, 112, 196), rgba(56, 140, 80), rgba(120, 132, 146), rgba(206, 146, 40), rgba(136, 80, 176)};
const uint32_t kShirts[12] = {rgba(70, 110, 150), rgba(150, 50, 44), rgba(60, 110, 64), rgba(196, 170, 120), rgba(110, 70, 140), rgba(200, 120, 40),
                              rgba(80, 80, 88),   rgba(226, 220, 200), rgba(40, 60, 100), rgba(130, 40, 70),  rgba(70, 130, 130), rgba(140, 100, 60)};
const uint32_t kTrousers[8] = {rgba(78, 60, 44), rgba(52, 46, 44), rgba(60, 70, 96), rgba(110, 90, 60),
                               rgba(70, 84, 56), rgba(120, 50, 44), rgba(150, 140, 120), rgba(44, 40, 60)};
const char* const kBuildN[3] = {"AVERAGE", "SLIM", "BROAD"};
const char* const kHairN[8] = {"BALD", "SHORT", "LONG", "PONYTAIL", "MOHAWK", "BRAIDS", "BUN", "CURLS"};
const char* const kRowN[8] = {"BUILD", "SKIN", "HAIR", "HAIR COLOR", "BEARD", "EYES", "SHIRT", "TROUSERS"};
const char* const kTabN[3] = {"LOOKS", "NAME", "ORIGIN"};
const char* const kNames[] = {"AELA",   "BRAND",  "CORWIN", "DAGNY", "EIRIK",  "FREYA", "GARRICK", "HILDE", "IVOR",    "JORUNN",
                              "KAEL",   "LIV",    "MAGNUS", "NESSA", "ORRIN",  "PIPER", "RAGNA",   "SIGRUN", "TORVALD", "VALKA",
                              "WYNN",   "YRSA",   "ASTRID", "BJORN", "CAEDMON", "EDDA", "FENRIK",  "GUNNAR", "HALVARD", "INGRID",
                              "KETIL",  "LEIF",   "MARA",   "OSWIN", "ROWAN",  "SOLVEIG", "THEA",  "VIGGO", "ELSPETH", "TAMSIN"};
constexpr int kNameCount = (int)(sizeof kNames / sizeof kNames[0]);
constexpr int kNameMax = 12;
constexpr int kNBg = (int)Background::COUNT - 1;   // the creator offers every background but None

// ---- layout
constexpr float kRX = 164, kRW = 304;                    // right column
constexpr float kTabY = 22, kTabH = 24;
constexpr float kRowY = 50, kRowH = 24;
constexpr float kBotY = 244, kBotH = 24;
constexpr float kLX = 10, kLW = 146;                     // preview column
constexpr float kArrowW = 26, kValX = kRX + 126, kValW = kRW - 126 - 2 * kArrowW - 8;

// ---- state (one View, one creator)
struct CState {
  int tab = 0, row = 0, nameHint = 0;
  int face = -1;            // -1 auto-turn, else 0 down, 1 up, 2 right, 3 left
  float faceHold = 0;       // seconds of manual facing left
  uint64_t seed = 0;
  bool ready = false;
} S;
Rng g_rng(0x51ED5EEDull);

template <size_t N>
int indexOf(const uint32_t (&a)[N], uint32_t c) {
  for (size_t i = 0; i < N; i++) if (a[i] == c) return (int)i;
  return 0;
}
int optionCount(int row) {
  switch (row) {
    case 0: return 3;
    case 1: return 8;
    case 2: return (int)art::Hair::COUNT;
    case 3: return 10;
    case 4: return 2;
    case 5: return 7;
    case 6: return 12;
    default: return 8;
  }
}
int optionIndex(const Appearance& a, int row) {
  switch (row) {
    case 0: return std::min<int>(a.build, 2);
    case 1: return std::min<int>(a.skinTone, 7);
    case 2: return std::min<int>(a.hair, (int)art::Hair::COUNT - 1);
    case 3: return indexOf(kHairCols, a.hairColor);
    case 4: return a.beard ? 1 : 0;
    case 5: return indexOf(kEyes, a.eyeColor);
    case 6: return indexOf(kShirts, a.topColor);
    default: return indexOf(kTrousers, a.bottomColor);
  }
}
void setOption(Appearance& a, int row, int v) {
  int n = optionCount(row);
  v = ((v % n) + n) % n;
  switch (row) {
    case 0: a.build = (uint8_t)v; break;
    case 1: a.skinTone = (uint8_t)v; a.skin = kSkins[v]; break;
    case 2: a.hair = (uint8_t)v; break;
    case 3: a.hairColor = kHairCols[v]; break;
    case 4: a.beard = v == 1; break;
    case 5: a.eyeColor = kEyes[v]; break;
    case 6: a.topColor = kShirts[v]; break;
    default: a.bottomColor = kTrousers[v]; break;
  }
}
// swatch colour for a colour row option (0 = not a swatch row)
uint32_t swatch(int row, int i) {
  switch (row) {
    case 1: return kSkins[i];
    case 3: return kHairCols[i];
    case 5: return kEyes[i] ? kEyes[i] : rgba(36, 26, 48);
    case 6: return kShirts[i];
    case 7: return kTrousers[i];
    default: return 0;
  }
}
std::string optionText(int row, int i) {
  switch (row) {
    case 0: return kBuildN[i];
    case 2: return kHairN[std::min(i, 7)];
    case 4: return i ? "FULL BEARD" : "CLEAN SHAVEN";
    default: return "";
  }
}

void randomizeLooks(Appearance& a) {
  for (int row = 0; row < 8; row++) setOption(a, row, g_rng.irange(optionCount(row)));
  if (g_rng.f() < 0.55f) a.beard = false;   // fewer beards than a coin flip
}

// the creator's preview look: the appearance in a shirt and trousers (rpg/sim/player.cpp dresses the same way)
art::HumanLook lookOf(const Appearance& a) {
  art::HumanLook L;
  L.skin = a.skin; L.hairColor = a.hairColor;
  L.hair = (art::Hair)std::min<int>(a.hair, (int)art::Hair::COUNT - 1);
  L.beard = a.beard; L.eyeColor = a.eyeColor; L.build = a.build;
  L.topColor = a.topColor; L.bottomColor = a.bottomColor;
  L.tabardColor = rgba(170, 50, 40);
  L.weapon = 0;
  return L;
}

void ensureInit(Game& g) {
  if (S.ready && S.seed == g.seed && g.background != Background::None) return;
  S = CState();
  S.ready = true;
  S.seed = g.seed;
  if (g.background == Background::None) g.background = Background::Blacksmith;
  Rng r(g.seed * 7919u + 17);
  S.nameHint = r.irange(kNameCount);
  if (g.app.name.empty() || g.app.name == "YOU") g.app.name = kNames[S.nameHint];
}

bool inR(Vec2 p, float x, float y, float w, float h) { return p.x >= x && p.y >= y && p.x < x + w && p.y < y + h; }

}  // namespace

// A lit stage for a large hero preview (paperdoll.cpp uses it too): a warm glow behind, a stone floor disc with a
// lit front rim, and the hero's soft contact shadow. footY is the row the feet stand on.
void heroStage(Pix& P, const Tex& light, float cx, float footY, float size) {
  P.blitEx(light, 0, 0, 64, 64, cx - 70 * size, footY - 150 * size, 140 * size, 160 * size, false, Color(1, 0.74f, 0.42f, 0.30f), 1);
  const float R = 34 * size, H = 7 * size;
  for (int k = 0; k <= (int)(H * 2); k++) {   // the floor disc, one row at a time: lit far half, darker near half
    float dy = k - H, t = dy / H;
    float hw = R * std::sqrt(std::max(0.0f, 1 - t * t));
    Color c = t < 0 ? Color(0.30f, 0.26f, 0.23f, 0.85f) : Color(0.21f, 0.18f, 0.17f, 0.9f);
    P.rect(std::floor(cx - hw), std::floor(footY + dy), std::floor(hw * 2), 1, c);
  }
  P.rect(std::floor(cx - R * 0.55f), std::floor(footY - H), std::floor(R * 1.1f), 1, Color(0.45f, 0.39f, 0.32f, 0.9f));
  for (int k = 0; k < 4; k++) {   // contact shadow under the feet
    float hw = (17 - k * 3.5f) * size;
    P.rect(std::floor(cx - hw), std::floor(footY - 1 + k * 0.5f), std::floor(hw * 2), 2, Color(0.05f, 0.03f, 0.05f, 0.22f));
  }
}

namespace {

void finish(Game& g) {
  std::string& n = g.app.name;
  while (!n.empty() && n.back() == ' ') n.pop_back();
  while (!n.empty() && n.front() == ' ') n.erase(n.begin());
  if (n.empty()) n = "HERO";
  g.pl().name = n;
  S.ready = false;
  g.finishCreator();
}
}  // namespace

// ------------------------------------------------------------------ draw
void View::drawCreator(Game& g) {
  ensureInit(g);
  Pix& P = *pix_;
  const Appearance& a = g.app;
  S.faceHold = std::max(0.0f, S.faceHold - 1.0f / 60.0f);
  P.rect(0, 0, Pix::W, Pix::H, Color(0.04f, 0.03f, 0.06f, 0.82f));
  if (settingsOpen_) return;
  // (M1) laid out for 480 x 270: a centred box on a wider or taller screen
  const UiBox box = uiBox(270);
  P.pushBox(box.x, box.y, box.w, box.h);
  struct Pop { Pix& p; ~Pop() { p.popBox(); } } pop{P};
  P.textS(Pix::W / 2.0f, 7, "WHO ARE YOU?", 1, kGoldC, 1);

  // ---- preview: the hero walks on the spot and turns; a warm light behind, a shadow below
  panel(kLX, kTabY, kLW, kBotY - kTabY - 4);
  std::string nm = a.name.empty() ? std::string("_") : a.name;
  P.textS(kLX + kLW / 2, kTabY + 8, nm, 1, kGoldC, 1);
  P.text(kLX + kLW / 2, kTabY + 19, backgroundInfo(g.background).name, 1, kDimC, 1);
  float cx = kLX + kLW / 2, footY = kTabY + 168;
  heroStage(P, light_, cx, footY, 1.0f);
  static const int order[4] = {0, 2, 1, 3};
  int face = S.face >= 0 && S.faceHold > 0 ? S.face : order[(int)(t_ / 2.2f) & 3];
  int row = face == 3 ? 2 : face;
  int frame = 1 + ((int)(t_ * 7.0f) & 3);
  const Tex& tex = humanTex(lookOf(a));
  const float sc = 5;
  P.blitEx(tex, frame * art::HUMAN_W, row * art::HUMAN_H, art::HUMAN_W, art::HUMAN_H, cx - art::HUMAN_W * sc / 2,
           footY - (art::HUMAN_H - 1) * sc, art::HUMAN_W * sc, art::HUMAN_H * sc, face == 3);
  button(kLX + 6, kBotY - 32, 30, 24, "<", false);
  button(kLX + kLW - 36, kBotY - 32, 30, 24, ">", false);
  P.text(cx, kBotY - 24, "TURN", 1, kDimC, 1);

  // ---- tabs
  float tw = kRW / 3;
  for (int i = 0; i < 3; i++) button(kRX + i * tw, kTabY, tw - 4, kTabH, kTabN[i], i == S.tab);
  panel(kRX, kRowY - 2, kRW, kBotY - kRowY - 2);

  if (S.tab == 0) {   // ---- LOOKS
    for (int r = 0; r < 8; r++) {
      float y = kRowY + r * kRowH;
      bool sel = r == S.row;
      if (sel) P.rect(kRX + 3, y + 1, kRW - 6, kRowH - 2, Color(0.3f, 0.22f, 0.12f, 0.75f));
      P.text(kRX + 10, y + 8, kRowN[r], 1, sel ? kGoldC : kTextC);
      button(kValX - kArrowW - 4, y + 1, kArrowW, kRowH - 2, "<", false);
      button(kValX + kValW + 4, y + 1, kArrowW, kRowH - 2, ">", false);
      int n = optionCount(r), cur = optionIndex(a, r);
      if (uint32_t s0 = swatch(r, 0)) {
        (void)s0;
        float sw = kValW / n;
        for (int i = 0; i < n; i++) {
          float x = kValX + i * sw;
          uint32_t c = swatch(r, i);
          Color cc((c & 255) / 255.0f, ((c >> 8) & 255) / 255.0f, ((c >> 16) & 255) / 255.0f);
          bool on = i == cur;
          float inset = on ? 1 : 4;
          P.rect(x + 1, y + inset + 1, sw - 2, kRowH - 2 - inset * 2, Color(0.02f, 0.02f, 0.03f, 0.9f));
          P.rect(x + 2, y + inset + 2, sw - 4, kRowH - 4 - inset * 2, cc);
          P.rect(x + 2, y + inset + 2, sw - 4, 1, Color(1, 1, 1, 0.25f));   // a lit top edge
          if (r == 5 && i == 0) P.text(x + sw / 2, y + 9, "-", 1, kTextC, 1);
          if (on) P.frame(x, y + 1, sw, kRowH - 2, kGoldC);
        }
      } else {
        P.text(kValX + kValW / 2, y + 8, optionText(r, cur), 1, sel ? Color(1, 0.95f, 0.8f) : kTextC, 1);
        for (int i = 0; i < n; i++) {   // pips: where this option sits in the list
          float px = kValX + kValW / 2 - n * 3 + i * 6 + 1;
          P.rect(px, y + 18, 4, 2, i == cur ? kGoldC : Color(0.4f, 0.34f, 0.26f));
        }
      }
    }
  } else if (S.tab == 1) {   // ---- NAME
    P.text(kRX + 10, kRowY + 6, "YOUR NAME", 1, kDimC);
    P.rect(kRX + 10, kRowY + 16, kRW - 20, 24, Color(0.02f, 0.02f, 0.03f, 0.9f));
    P.frame(kRX + 10, kRowY + 16, kRW - 20, 24, Color(0.55f, 0.45f, 0.28f));
    std::string shown = a.name;
    if (((int)(t_ * 2.5f) & 1) && (int)a.name.size() < kNameMax) shown += "_";
    P.textS(kRX + kRW / 2, kRowY + 21, shown, 2, Color(1, 0.95f, 0.82f), 1);
    button(kRX + 10, kRowY + 46, 140, 24, "RANDOM NAME", false);
    button(kRX + kRW - 150, kRowY + 46, 140, 24, "DELETE", false);
    const float kw = 30, kh = 24, gap = 3, x0 = kRX + (kRW - (9 * kw + 8 * gap)) / 2, y0 = kRowY + 78;
    for (int i = 0; i < 27; i++) {
      float x = x0 + (i % 9) * (kw + gap), y = y0 + (i / 9) * (kh + gap);
      std::string k = i < 26 ? std::string(1, (char)('A' + i)) : std::string("SPC");
      button(x, y, kw, kh, k, false);
    }
    P.text(kRX + kRW / 2, kRowY + 166, touchUI ? "TAP THE LETTERS, OR RANDOM NAME" : "TYPE A NAME    UP/DOWN: SUGGEST ONE", 1, kDimC, 1);
  } else {   // ---- ORIGIN
    int sel = std::clamp((int)g.background - 1, 0, kNBg - 1);
    for (int i = 0; i < kNBg; i++) {
      float x = kRX + 6 + (i % 2) * (kRW / 2 - 3), y = kRowY + 2 + (i / 2) * 26;
      button(x, y, kRW / 2 - 9, 24, backgroundInfo((Background)(i + 1)).name, i == sel);
    }
    const BackgroundInfo& bi = backgroundInfo(g.background);
    float y = kRowY + 112;
    P.text(kRX + 10, y, "TRAIT", 1, kGoldC);
    wrapText(kRX + 52, y, kRW - 62, bi.perk, kTextC, -1, 10);
    P.text(kRX + 10, y + 34, "STORY", 1, kGoldC);
    wrapText(kRX + 52, y + 34, kRW - 62, bi.hook, Color(0.82f, 0.78f, 0.7f), -1, 10);
    P.text(kRX + kRW / 2, kBotY - 22, "EVERYONE STARTS WITH THE SHIRT ON THEIR BACK", 1, kDimC, 1);
  }

  // ---- bottom bar
  button(kRX, kBotY, 146, kBotH, "RANDOMIZE", false);
  button(kRX + kRW - 146, kBotY, 146, kBotH, S.tab == 2 ? "BEGIN" : "NEXT >", true);
  button(kLX, kBotY, kLW, kBotH, "BACK TO TITLE", false);
}

// ------------------------------------------------------------------ input
void View::creatorKey(Game& g, int key) {
  ensureInit(g);
  Appearance& a = g.app;
  auto move = [&]() { audio_->play(Sfx::MenuMove); };
  auto next = [&]() {
    if (S.tab < 2) { S.tab++; audio_->play(Sfx::MenuSelect); }
    else { audio_->play(Sfx::QuestStart); finish(g); }
  };
  if (key == SDLK_ESCAPE) { S.ready = false; g.mode = Mode::Title; audio_->play(Sfx::MenuBack); return; }
  if (key == SDLK_TAB) { S.tab = (S.tab + 1) % 3; move(); return; }
  if (key == SDLK_RETURN || key == SDLK_KP_ENTER) { next(); return; }
  if (key == SDLK_LEFTBRACKET || key == SDLK_RIGHTBRACKET) {
    static const int ring[4] = {0, 2, 1, 3};
    int at = 0;
    int cur = S.face >= 0 && S.faceHold > 0 ? S.face : 0;
    for (int i = 0; i < 4; i++) if (ring[i] == cur) at = i;
    S.face = ring[(at + (key == SDLK_RIGHTBRACKET ? 1 : 3)) % 4];
    S.faceHold = 4;
    return;
  }
  if (S.tab == 1) {   // NAME: type
    if (key >= SDLK_A && key <= SDLK_Z) { if ((int)a.name.size() < kNameMax) a.name += (char)('A' + (key - SDLK_A)); move(); return; }
    if (key == SDLK_SPACE) { if (!a.name.empty() && (int)a.name.size() < kNameMax) a.name += ' '; return; }
    if (key == SDLK_BACKSPACE || key == SDLK_DELETE) { if (!a.name.empty()) a.name.pop_back(); move(); return; }
    if (key == SDLK_UP || key == SDLK_DOWN) {
      S.nameHint = (S.nameHint + (key == SDLK_UP ? kNameCount - 1 : 1)) % kNameCount;
      a.name = kNames[S.nameHint];
      move();
    }
    return;
  }
  if (key >= SDLK_1 && key <= SDLK_3) { S.tab = (int)(key - SDLK_1); move(); return; }
  if (key == SDLK_R) { if (S.tab == 2) g.background = (Background)(1 + g_rng.irange(kNBg)); else randomizeLooks(a); move(); return; }
  bool up = key == SDLK_UP || key == SDLK_W, down = key == SDLK_DOWN || key == SDLK_S;
  bool left = key == SDLK_LEFT || key == SDLK_A, right = key == SDLK_RIGHT || key == SDLK_D;
  if (S.tab == 0) {
    if (up) { S.row = (S.row + 7) % 8; move(); }
    if (down) { S.row = (S.row + 1) % 8; move(); }
    if (left || right) { setOption(a, S.row, optionIndex(a, S.row) + (right ? 1 : -1)); move(); }
    if (key == SDLK_SPACE) next();
  } else {
    int i = std::clamp((int)g.background - 1, 0, kNBg - 1);
    if (left || right) i = (i ^ 1);
    if (up) i = (i + kNBg - 2) % kNBg;
    if (down) i = (i + 2) % kNBg;
    if (up || down || left || right) { g.background = (Background)(i + 1); move(); }
    if (key == SDLK_SPACE) next();
  }
}

void View::creatorTap(Game& g, Vec2 p) {
  ensureInit(g);
  p = inBox(uiBox(270), p);   // drawCreator's box
  Appearance& a = g.app;
  auto move = [&]() { audio_->play(Sfx::MenuMove); };
  // preview turn buttons
  if (inR(p, kLX + 6, kBotY - 32, 30, 24) || inR(p, kLX + kLW - 36, kBotY - 32, 30, 24)) {
    creatorKey(g, p.x < kLX + kLW / 2 ? SDLK_LEFTBRACKET : SDLK_RIGHTBRACKET);
    return;
  }
  if (inR(p, kLX, kBotY, kLW, kBotH)) { creatorKey(g, SDLK_ESCAPE); return; }
  // tabs
  float tw = kRW / 3;
  for (int i = 0; i < 3; i++)
    if (inR(p, kRX + i * tw, kTabY, tw - 4, kTabH)) { S.tab = i; move(); return; }
  // bottom bar
  if (inR(p, kRX, kBotY, 146, kBotH)) {
    if (S.tab == 2) g.background = (Background)(1 + g_rng.irange(kNBg));
    else if (S.tab == 1) { S.nameHint = g_rng.irange(kNameCount); a.name = kNames[S.nameHint]; }
    else randomizeLooks(a);
    move();
    return;
  }
  if (inR(p, kRX + kRW - 146, kBotY, 146, kBotH)) { creatorKey(g, SDLK_RETURN); return; }
  if (S.tab == 0) {
    for (int r = 0; r < 8; r++) {
      float y = kRowY + r * kRowH;
      if (!inR(p, kRX, y, kRW, kRowH)) continue;
      S.row = r;
      int n = optionCount(r), cur = optionIndex(a, r);
      if (inR(p, kValX - kArrowW - 4, y, kArrowW, kRowH)) setOption(a, r, cur - 1);
      else if (inR(p, kValX + kValW + 4, y, kArrowW, kRowH)) setOption(a, r, cur + 1);
      else if (swatch(r, 0) && inR(p, kValX, y, kValW, kRowH)) setOption(a, r, std::clamp((int)((p.x - kValX) / (kValW / n)), 0, n - 1));
      else if (!swatch(r, 0) && inR(p, kValX, y, kValW, kRowH)) setOption(a, r, cur + 1);
      move();
      return;
    }
  } else if (S.tab == 1) {
    if (inR(p, kRX + 10, kRowY + 46, 140, 24)) { S.nameHint = g_rng.irange(kNameCount); a.name = kNames[S.nameHint]; move(); return; }
    if (inR(p, kRX + kRW - 150, kRowY + 46, 140, 24)) { if (!a.name.empty()) a.name.pop_back(); move(); return; }
    const float kw = 30, kh = 24, gap = 3, x0 = kRX + (kRW - (9 * kw + 8 * gap)) / 2, y0 = kRowY + 78;
    for (int i = 0; i < 27; i++) {
      float x = x0 + (i % 9) * (kw + gap), y = y0 + (i / 9) * (kh + gap);
      if (!inR(p, x, y, kw, kh)) continue;
      if ((int)a.name.size() < kNameMax && (i < 26 || !a.name.empty())) a.name += i < 26 ? (char)('A' + i) : ' ';
      move();
      return;
    }
  } else {
    for (int i = 0; i < kNBg; i++) {
      float x = kRX + 6 + (i % 2) * (kRW / 2 - 3), y = kRowY + 2 + (i / 2) * 26;
      if (inR(p, x, y, kRW / 2 - 9, 24)) { g.background = (Background)(i + 1); move(); return; }
    }
  }
}
