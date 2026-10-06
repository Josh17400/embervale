// The character creator (Mode::Creator): looks, name and background, then Game::finishCreator(). M0 hero lane.
// Everyone starts with just the shirt on their back (VISION_PLAN 15.1): the preview is the look the game starts
// with, and a background grants a trait, never gear.
//
// M3 (people lane): a fourth tab, HOMELAND: the people (human, half-breed, elf; looks only), the homeland (the culture
// families nearest the start; its dress cut and palette dress the starting shirt) and the personal arms (division, two
// tinctures, a charge, the banner shape; RANDOMIZE on this tab rolls new arms), previewed on a shield.
// Layout (480x270 logical): the animated preview on the left, four tabs on the right (LOOKS, NAME, ORIGIN, HOMELAND),
// RANDOMIZE and BEGIN along the bottom. Every touch target is at least 24 px tall.
// Keys: Tab / 1-3 switch tabs (1-3 not on NAME), Up/Down pick a row, Left/Right change it, R randomizes,
// [ and ] turn the preview, Enter goes to the next tab and BEGINs on HOMELAND, Escape returns to the title.
// On NAME the letters type, Backspace deletes and Up/Down suggest a name.
#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>
#include <unordered_map>
#include "rpg/culture/culture.h"
#include "rpg/view/view.h"
#include "rpg/world/coords.h"
#include "rpg/world/source.h"

art::HumanLook appearanceLook(const Appearance& a, const cult::Culture* home);   // rpg/sim/player.cpp

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
const char* const kTabN[4] = {"LOOKS", "NAME", "ORIGIN", "HOMELAND"};
constexpr int kTabs = 4;
// ---- M3 homeland and arms
const char* const kPeopleN[3] = {"HUMAN", "HALF-BREED", "ELF"};
const uint32_t kTinct[8] = {rgba(176, 36, 40), rgba(40, 72, 160), rgba(36, 112, 64), rgba(112, 44, 120),
                            rgba(36, 32, 40), rgba(220, 172, 52), rgba(232, 232, 224), rgba(196, 98, 36)};
const char* const kTinctN[8] = {"GULES", "AZURE", "VERT", "PURPURE", "SABLE", "OR", "ARGENT", "TENNE"};
const char* const kDivN[8] = {"PLAIN", "PER PALE", "PER FESS", "QUARTERLY", "CHEVRON", "BEND", "SALTIRE", "BORDURE"};
const char* const kChargeN[9] = {"CROWN", "SWORD", "TOWER", "STAR", "TREE", "SUN", "MOON", "EAGLE", "RUNE"};
const char* const kShapeN[4] = {"SQUARE", "SWALLOW-TAIL", "PENNANT", "GONFALON"};
const char* const kHomeRowN[7] = {"PEOPLE", "HOMELAND", "DIVISION", "FIELD", "SECOND", "CHARGE", "BANNER"};
constexpr int kHomeRows = 7;
const char* const kNames[] = {"AELA",   "BRAND",  "CORWIN", "DAGNY", "EIRIK",  "FREYA", "GARRICK", "HILDE", "IVOR",    "JORUNN",
                              "KAEL",   "LIV",    "MAGNUS", "NESSA", "ORRIN",  "PIPER", "RAGNA",   "SIGRUN", "TORVALD", "VALKA",
                              "WYNN",   "YRSA",   "ASTRID", "BJORN", "CAEDMON", "EDDA", "FENRIK",  "GUNNAR", "HALVARD", "INGRID",
                              "KETIL",  "LEIF",   "MARA",   "OSWIN", "ROWAN",  "SOLVEIG", "THEA",  "VIGGO", "ELSPETH", "TAMSIN"};
constexpr int kNameCount = (int)(sizeof kNames / sizeof kNames[0]);
constexpr int kNameMax = 12;
constexpr int kNBg = (int)Background::COUNT - 1;   // the creator offers every background but None

// ---- layout (M2: from the box, which is the whole safe area; at 480 x 270 exactly the M0 layout the scripts tap)
float kRX = 164, kRW = 304;                    // right column
constexpr float kTabY = 22, kTabH = 24;
constexpr float kRowY = 50;
float kRowH = 24;
float kBotY = 244;
constexpr float kBotH = 24;
constexpr float kLX = 10;
float kLW = 146;                               // preview column
float kArrowW = 26, kValX = kRX + 126, kValW = kRW - 126 - 2 * kArrowW - 8;
float kKeyW = 30;                              // the NAME tab's letter keys
void relayout(int W, int H) {
  kLW = 146 + std::floor(std::max(0, W - 480) * 0.3f);
  kRX = kLX + kLW + 8;
  kRW = (float)W - 12 - kRX;
  kBotY = (float)H - 26;
  kRowH = std::max(24.0f, std::floor((kBotY - 6 - kRowY) / 8));
  kArrowW = W >= 560 ? 30.0f : 26.0f;
  kValX = kRX + 126 + std::floor(std::max(0.0f, kRW - 304) * 0.2f);
  kValW = kRX + kRW - kValX - 2 * kArrowW - 8;
  kKeyW = 30 + std::floor(std::max(0.0f, kRW - 304) / 9 * 0.6f);
}

// ---- state (one View, one creator)
struct CState {
  int tab = 0, row = 0, nameHint = 0;
  int face = -1;            // -1 auto-turn, else 0 down, 1 up, 2 right, 3 left
  float faceHold = 0;       // seconds of manual facing left
  uint64_t seed = 0;
  bool ready = false;
  std::vector<uint64_t> homes;   // M3: homeland choices (cult::CultureId), [0] = the start's own culture
  int homeRow = 0;
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
// (M3: the shared builder in player.cpp, with the people, the homeland's cut and the personal arms' field)
art::HumanLook lookOf(const Game& g) {
  const cult::Culture* home = g.app.homeland && g.world.src ? &g.world.src->culture(g.app.homeland) : nullptr;
  return appearanceLook(g.app, home);
}

// ---- M3: homeland and personal arms
int tinctIndex(uint32_t c) {
  for (int i = 0; i < 8; i++) if (kTinct[i] == c) return i;
  return 0;
}
bool lightTinct(uint32_t c) { return c == kTinct[5] || c == kTinct[6]; }
// a charge that reads on its field: a metal (or, argent) on a colour, sable on a metal
void fixCharge(cult::Heraldry& h) {
  if (lightTinct(h.field)) h.charge = kTinct[4];
  else h.charge = lightTinct(h.field2) ? h.field2 : kTinct[5];
}
void randomArms(cult::Heraldry& h, Rng& r) {
  const int a = r.irange(8);
  int b = r.irange(8);
  if (b == a) b = (a + 3) % 8;
  if (!lightTinct(kTinct[a]) && !lightTinct(kTinct[b])) b = 5 + r.irange(2);   // the rule of tincture
  h.field = kTinct[a]; h.field2 = kTinct[b];
  h.division = (uint8_t)r.irange(8);
  h.chargeKind = r.irange(6) == 0 ? 1 : 0;
  h.emblem = (uint8_t)r.irange(8);
  h.glyphSeed = r.next() | 1u;
  h.shape = (uint8_t)r.irange(4);
  fixCharge(h);
}
int32_t playerGx(const Game& g) { return g.world.ox + (int32_t)std::floor(g.pl().p.x / TILE); }
int32_t playerGy(const Game& g) { return g.world.oy + (int32_t)std::floor(g.pl().p.y / TILE); }
// the start's own culture first, then the nearest other culture families (distinct archetypes first): 3 or 4 choices
void homelandOptions(Game& g) {
  S.homes.clear();
  if (!g.world.src || g.actors.empty()) return;
  ew::EndlessSource& src = *g.world.src;
  const int32_t gx = playerGx(g), gy = playerGy(g);
  const uint64_t here = src.cultureAt(gx, gy);
  S.homes.push_back(here);
  std::vector<cult::Archetype> seen = {src.culture(here).archetype};
  const int32_t ci = ew::floorDiv(gx, ew::CCELL), cj = ew::floorDiv(gy, ew::CCELL);
  struct Cand { int64_t d; uint64_t id; cult::Archetype a; };
  std::vector<Cand> c;
  for (int dj = -2; dj <= 2; dj++)
    for (int di = -2; di <= 2; di++) {
      const uint64_t id = cult::familyId(ci + di, cj + dj);
      if (id == cult::familyOf(here)) continue;
      const int64_t cx = (int64_t)(ci + di) * ew::CCELL + ew::CCELL / 2 - gx, cy = (int64_t)(cj + dj) * ew::CCELL + ew::CCELL / 2 - gy;
      c.push_back({cx * cx + cy * cy, id, src.culture(id).archetype});
    }
  std::sort(c.begin(), c.end(), [](const Cand& a, const Cand& b) { return a.d < b.d || (a.d == b.d && a.id < b.id); });
  for (int pass = 0; pass < 2 && S.homes.size() < 4; pass++)
    for (const Cand& k : c) {
      if (S.homes.size() >= 4) break;
      if (std::find(S.homes.begin(), S.homes.end(), k.id) != S.homes.end()) continue;
      if (pass == 0 && std::find(seen.begin(), seen.end(), k.a) != seen.end()) continue;
      S.homes.push_back(k.id);
      seen.push_back(k.a);
    }
}
int homeIndex(const Game& g) {
  for (size_t i = 0; i < S.homes.size(); i++) if (S.homes[i] == g.app.homeland) return (int)i;
  return 0;
}
// choosing a homeland dresses the starting shirt and trousers in its palette (the cut follows in recalcPlayer)
void setHomeland(Game& g, int i) {
  if (S.homes.empty() || !g.world.src) return;
  i = ((i % (int)S.homes.size()) + (int)S.homes.size()) % (int)S.homes.size();
  g.app.homeland = S.homes[(size_t)i];
  const cult::Culture& C = g.world.src->culture(g.app.homeland);
  int n = 0;
  for (uint32_t c : C.dress.cloth) n += c != 0;
  if (n) {
    g.app.topColor = C.dress.cloth[0] | 0xFF000000u;
    const uint32_t b = C.dress.cloth[n > 1 ? 1 : 0];
    g.app.bottomColor = rgba((int)((b & 255) * 0.62f), (int)(((b >> 8) & 255) * 0.62f), (int)(((b >> 16) & 255) * 0.62f));
  }

}
std::string homeText(const Game& g, int i) {
  if (S.homes.empty() || !g.world.src) return "-";
  const cult::Culture& C = g.world.src->culture(S.homes[(size_t)std::clamp(i, 0, (int)S.homes.size() - 1)]);
  std::string n = C.name.empty() ? std::string("UNKNOWN") : C.name;
  if (n.size() > 10) n = n.substr(0, 10);
  return n + " (" + cult::archetypeName(C.archetype) + ")";
}
int homeOptionCount(int row) {
  switch (row) {
    case 0: return 3;
    case 1: return std::max<int>(1, (int)S.homes.size());
    case 5: return 9;
    case 6: return 4;
    default: return 8;
  }
}
int homeOptionIndex(const Game& g, int row) {
  const cult::Heraldry& h = g.app.heraldry;
  switch (row) {
    case 0: return std::min<int>(g.app.people, 2);
    case 1: return homeIndex(g);
    case 2: return h.division & 7;
    case 3: return tinctIndex(h.field);
    case 4: return tinctIndex(h.field2);
    case 5: return h.chargeKind ? 8 : (h.emblem & 7);
    default: return h.shape & 3;
  }
}
void setHomeOption(Game& g, int row, int v) {
  const int n = homeOptionCount(row);
  v = ((v % n) + n) % n;
  cult::Heraldry& h = g.app.heraldry;
  switch (row) {
    case 0: g.app.people = (uint8_t)v; if (v == 2) g.app.beard = false; return;
    case 1: setHomeland(g, v); return;
    case 2: h.division = (uint8_t)v; break;
    case 3: h.field = kTinct[v]; break;
    case 4: h.field2 = kTinct[v]; break;
    case 5:
      if (v == 8) { h.chargeKind = 1; if (!h.glyphSeed) h.glyphSeed = 0x5EEDu; }
      else { h.chargeKind = 0; h.emblem = (uint8_t)v; }
      break;
    default: h.shape = (uint8_t)v; break;
  }
  fixCharge(h);

}
std::string homeOptionText(const Game& g, int row, int i) {
  switch (row) {
    case 0: return kPeopleN[std::clamp(i, 0, 2)];
    case 1: return homeText(g, i);
    case 2: return kDivN[i & 7];
    case 5: return kChargeN[std::clamp(i, 0, 8)];
    case 6: return kShapeN[i & 3];
    default: return kTinctN[i & 7];
  }
}
bool homeSwatchRow(int row) { return row == 3 || row == 4; }
// HOMELAND rows: 24 px each, with a gap before the arms
float homeRowY(int r) { return kRowY + r * 24.0f + (r >= 2 ? 6.0f : 0.0f); }
// the arms rows leave room on the right for the shield preview
// (M3 fixer round 3, review: "the PEOPLE and HOMELAND '>' arrows sit at another x than the rows below") every row
// keeps the same box, so the arrows form one column
float homeValW(int r) { (void)r; return kValW - 54; }
// the arms preview, painted by art::shieldArms and cached per arms (the cache key's top byte 0x03 is this lane's)
std::unordered_map<uint64_t, std::pair<cult::Heraldry, int>>& armsPending() {
  static std::unordered_map<uint64_t, std::pair<cult::Heraldry, int>> m;
  return m;
}
}  // namespace
Canvas creatorPaintArms(uint64_t key) {
  auto it = armsPending().find(key);
  if (it == armsPending().end()) return Canvas(1, 1);
  return art::shieldArms(it->second.first, it->second.second);
}
uint64_t creatorArmsKey(const cult::Heraldry& h, int size) {
  const uint64_t k = 0x03ull << 56 | ((h.key() ^ (uint64_t)size * 0x9E3779B97F4A7C15ull) & 0x00FFFFFFFFFFFFFFull);
  armsPending()[k] = {h, size};
  return k;
}
namespace {

void ensureInit(Game& g) {
  if (S.ready && S.seed == g.seed && g.background != Background::None) return;
  S = CState();
  S.ready = true;
  S.seed = g.seed;
  if (g.background == Background::None) g.background = Background::Blacksmith;
  Rng r(g.seed * 7919u + 17);
  S.nameHint = r.irange(kNameCount);
  if (g.app.name.empty() || g.app.name == "YOU") g.app.name = kNames[S.nameHint];
  homelandOptions(g);
  if (g.app.heraldry.empty()) randomArms(g.app.heraldry, r);
  if (!g.app.homeland && !S.homes.empty()) setHomeland(g, 0);
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
  relayout(Pix::W, Pix::H);
  P.textS(Pix::W / 2.0f, 7, "WHO ARE YOU?", 1, kGoldC, 1);

  // ---- preview: the hero walks on the spot and turns; a warm light behind, a shadow below
  panel(kLX, kTabY, kLW, kBotY - kTabY - 4);
  std::string nm = a.name.empty() ? std::string("_") : a.name;
  P.textS(kLX + kLW / 2, kTabY + 8, nm, 1, kGoldC, 1);
  P.text(kLX + kLW / 2, kTabY + 19, backgroundInfo(g.background).name, 1, kDimC, 1);
  float cx = kLX + kLW / 2, footY = kBotY - 54;
  heroStage(P, light_, cx, footY, 1.0f);
  static const int order[4] = {0, 2, 1, 3};
  int face = S.face >= 0 && S.faceHold > 0 ? S.face : order[(int)(t_ / 2.2f) & 3];
  int row = face == 3 ? 2 : face;
  int frame = 1 + ((int)(t_ * 7.0f) & 3);
  const Tex& tex = humanTex(lookOf(g));
  const float sc = 5;
  P.blitEx(tex, frame * art::HUMAN_W, row * art::HUMAN_H, art::HUMAN_W, art::HUMAN_H, cx - art::HUMAN_W * sc / 2,
           footY - (art::HUMAN_H - 1) * sc, art::HUMAN_W * sc, art::HUMAN_H * sc, face == 3);
  button(kLX + 6, kBotY - 32, 30, 24, "<", false);
  button(kLX + kLW - 36, kBotY - 32, 30, 24, ">", false);
  P.text(cx, kBotY - 24, "TURN", 1, kDimC, 1);

  // ---- tabs
  float tw = kRW / kTabs;
  for (int i = 0; i < kTabs; i++) button(kRX + i * tw, kTabY, tw - 4, kTabH, kTabN[i], i == S.tab);
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
    const float kw = kKeyW, kh = 24, gap = 3, x0 = kRX + (kRW - (9 * kw + 8 * gap)) / 2, y0 = kRowY + 78;
    for (int i = 0; i < 27; i++) {
      float x = x0 + (i % 9) * (kw + gap), y = y0 + (i / 9) * (kh + gap);
      std::string k = i < 26 ? std::string(1, (char)('A' + i)) : std::string("SPC");
      button(x, y, kw, kh, k, false);
    }
    P.text(kRX + kRW / 2, kRowY + 166, touchUI ? "TAP THE LETTERS, OR RANDOM NAME" : "TYPE A NAME    UP/DOWN: SUGGEST ONE", 1, kDimC, 1);
  } else if (S.tab == 2) {   // ---- ORIGIN
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
  } else {   // ---- HOMELAND (M3): people, homeland, personal arms
    for (int r = 0; r < kHomeRows; r++) {
      const float y = homeRowY(r), vw = homeValW(r);
      const bool sel = r == S.homeRow;
      if (sel) P.rect(kRX + 3, y + 1, kRW - 60, 22, Color(0.3f, 0.22f, 0.12f, 0.75f));
      P.text(kRX + 10, y + 8, kHomeRowN[r], 1, sel ? kGoldC : kTextC);
      button(kValX - kArrowW - 4, y + 1, kArrowW, 22, "<", false);
      button(kValX + vw + 4, y + 1, kArrowW, 22, ">", false);
      const int n = homeOptionCount(r), cur = homeOptionIndex(g, r);
      if (homeSwatchRow(r)) {
        const float sw = vw / n;
        for (int i = 0; i < n; i++) {
          const float x = kValX + i * sw;
          const uint32_t c = kTinct[i];
          const bool on = i == cur;
          // (M3 fixer round 3: finger-sized like the LOOKS swatches: the whole row's height, a 1 px gap between)
          const float inset = on ? 1 : 2;
          P.rect(x, y + inset, sw - 1, 22 - inset * 2 + 1, Color(0.02f, 0.02f, 0.03f, 0.9f));
          P.rect(x + 1, y + inset + 1, sw - 3, 20 - inset * 2 + 1, Color((c & 255) / 255.0f, ((c >> 8) & 255) / 255.0f, ((c >> 16) & 255) / 255.0f));
          if (on) P.frame(x, y + 1, sw, 22, kGoldC);
        }
      } else {
        const std::string t = homeOptionText(g, r, cur);
        const Color tc = sel ? Color(1, 0.95f, 0.8f) : kTextC;
        const size_t paren = r == 1 ? t.find(" (") : std::string::npos;
        if (paren != std::string::npos && P.textW(t, 1) > (int)vw - 4) {
          // (M3 fixer) "NAME (ARCHETYPE)" too wide for the box between the arrows (the 480 x 270 desktop layout): the
          // homeland's name over its people's kind, each cut to the box
          const size_t maxC = (size_t)std::max(1.0f, (vw - 4 + 1) / 6.0f);
          std::string a1 = t.substr(0, paren), a2 = t.substr(paren + 1);
          if (a1.size() > maxC) a1 = a1.substr(0, maxC);
          if (a2.size() > maxC) a2 = a2.substr(0, maxC);
          P.text(kValX + vw / 2, y + 3, a1, 1, tc, 1);
          P.text(kValX + vw / 2, y + 13, a2, 1, kDimC, 1);
        } else
          P.text(kValX + vw / 2, y + 8, t, 1, tc, 1);
      }
    }
    // (M3 fixer) what the choices do, as ORIGIN says what a background gives
    // (M3 fixer round 2) centred in the free band between the last row and the panel's bottom frame (at kBotY - 13
    // it sat on the frame at the phone size)
    {
      const float rowsEnd = homeRowY(kHomeRows - 1) + 24, frameY = kBotY - 4;
      const float ty = std::floor(std::max(rowsEnd + 1, (rowsEnd + frameY) * 0.5f - 4));
      P.text(kRX + kRW / 2, ty, "PEOPLE SHAPES YOUR LOOKS, HOMELAND YOUR DRESS", 1, kDimC, 1);
    }
    // the arms on a shield, beside the arms rows
    const float shX = kRX + kRW - 52, shY = homeRowY(2) + 4;
    const Tex& arms = cachedTex(creatorArmsKey(a.heraldry, 32), creatorPaintArms);
    P.blitEx(arms, 0, 0, arms.w, arms.h, shX, shY, 44, 44 * (float)arms.h / std::max(1, arms.w));
    P.text(shX + 22, shY + 52, "ARMS", 1, kDimC, 1);
  }

  // ---- bottom bar
  button(kRX, kBotY, 146, kBotH, "RANDOMIZE", false);
  button(kRX + kRW - 146, kBotY, 146, kBotH, S.tab == kTabs - 1 ? "BEGIN" : "NEXT >", true);
  button(kLX, kBotY, kLW, kBotH, "BACK TO TITLE", false);
}

// ------------------------------------------------------------------ input
void View::creatorPrepare(Game& g) { ensureInit(g); }

void View::creatorKey(Game& g, int key) {
  ensureInit(g);
  Appearance& a = g.app;
  auto move = [&]() { audio_->play(Sfx::MenuMove); };
  auto next = [&]() {
    if (S.tab < kTabs - 1) { S.tab++; audio_->play(Sfx::MenuSelect); }
    else { audio_->play(Sfx::QuestStart); finish(g); }
  };
  if (key == SDLK_ESCAPE) { S.ready = false; g.mode = Mode::Title; audio_->play(Sfx::MenuBack); return; }
  if (key == SDLK_TAB) { S.tab = (S.tab + 1) % kTabs; move(); return; }
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
  if (key >= SDLK_1 && key <= SDLK_4) { S.tab = (int)(key - SDLK_1); move(); return; }
  if (key == SDLK_R) {
    if (S.tab == 2) g.background = (Background)(1 + g_rng.irange(kNBg));
    else if (S.tab == 3) { randomArms(a.heraldry, g_rng); }
    else randomizeLooks(a);
    move();
    return;
  }
  bool up = key == SDLK_UP || key == SDLK_W, down = key == SDLK_DOWN || key == SDLK_S;
  bool left = key == SDLK_LEFT || key == SDLK_A, right = key == SDLK_RIGHT || key == SDLK_D;
  if (S.tab == 0) {
    if (up) { S.row = (S.row + 7) % 8; move(); }
    if (down) { S.row = (S.row + 1) % 8; move(); }
    if (left || right) { setOption(a, S.row, optionIndex(a, S.row) + (right ? 1 : -1)); move(); }
    if (key == SDLK_SPACE) next();
  } else if (S.tab == 3) {
    if (up) { S.homeRow = (S.homeRow + kHomeRows - 1) % kHomeRows; move(); }
    if (down) { S.homeRow = (S.homeRow + 1) % kHomeRows; move(); }
    if (left || right) { setHomeOption(g, S.homeRow, homeOptionIndex(g, S.homeRow) + (right ? 1 : -1)); move(); }
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
  const UiBox box = uiBox(270);
  p = inBox(box, p);   // drawCreator's box
  relayout(box.w, box.h);
  Appearance& a = g.app;
  auto move = [&]() { audio_->play(Sfx::MenuMove); };
  // preview turn buttons
  if (inR(p, kLX + 6, kBotY - 32, 30, 24) || inR(p, kLX + kLW - 36, kBotY - 32, 30, 24)) {
    creatorKey(g, p.x < kLX + kLW / 2 ? SDLK_LEFTBRACKET : SDLK_RIGHTBRACKET);
    return;
  }
  if (inR(p, kLX, kBotY, kLW, kBotH)) { creatorKey(g, SDLK_ESCAPE); return; }
  // tabs
  float tw = kRW / kTabs;
  for (int i = 0; i < kTabs; i++)
    if (inR(p, kRX + i * tw, kTabY, tw - 4, kTabH)) { S.tab = i; move(); return; }
  // bottom bar
  if (inR(p, kRX, kBotY, 146, kBotH)) {
    if (S.tab == 2) g.background = (Background)(1 + g_rng.irange(kNBg));
    else if (S.tab == 3) { randomArms(a.heraldry, g_rng); }
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
    const float kw = kKeyW, kh = 24, gap = 3, x0 = kRX + (kRW - (9 * kw + 8 * gap)) / 2, y0 = kRowY + 78;
    for (int i = 0; i < 27; i++) {
      float x = x0 + (i % 9) * (kw + gap), y = y0 + (i / 9) * (kh + gap);
      if (!inR(p, x, y, kw, kh)) continue;
      if ((int)a.name.size() < kNameMax && (i < 26 || !a.name.empty())) a.name += i < 26 ? (char)('A' + i) : ' ';
      move();
      return;
    }
  } else if (S.tab == 3) {
    for (int r = 0; r < kHomeRows; r++) {
      const float y = homeRowY(r), vw = homeValW(r);
      if (!inR(p, kRX, y, kRW, 24)) continue;
      S.homeRow = r;
      const int n = homeOptionCount(r), cur = homeOptionIndex(g, r);
      if (inR(p, kValX - kArrowW - 4, y, kArrowW, 24)) setHomeOption(g, r, cur - 1);
      else if (inR(p, kValX + vw + 4, y, kArrowW, 24)) setHomeOption(g, r, cur + 1);
      else if (homeSwatchRow(r) && inR(p, kValX, y, vw, 24)) setHomeOption(g, r, std::clamp((int)((p.x - kValX) / (vw / n)), 0, n - 1));
      else if (!homeSwatchRow(r) && inR(p, kValX, y, vw, 24)) setHomeOption(g, r, cur + 1);
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
