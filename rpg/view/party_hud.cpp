// M8 Fellowship, the party on the HUD (VIEW lane): the column of portraits under the vitals, the command radial, the
// alert ribbons and the banter plates.
//
//   - The column: one framed bust per companion travelling with the player (State::With or Downed), in members order,
//     under the vitals at the safe area's left edge (clear of the toasts, which step right of it, the minimap and the
//     quest column on the right, and the touch stick at the bottom). Each card: the portrait on a backdrop in the
//     vocation's colour, lit from the top left, in the HUD's gold double frame; a health bar under it with the
//     low-health pulse below LOW_HP_PCT; the stance badge in its corner. Downed: the bust greys, a red fuse round the
//     frame burns down over DOWNED_SECS with the seconds left, and the health bar becomes the binding's gold progress
//     (Member::reviveT / REVIVE_SECS). Waiting and at-home members follow as small dimmed busts (two to a row).
//   - Touch: a tap selects a card (its name shows beside it), a tap on the selected card or a HOLD opens the radial
//     (held: slide to a wedge and let go; tapped: tap a wedge). Keyboard: C opens it on the selected member (A / D or
//     the arrows turn it, 1-7 or Enter give the order, C the next companion, Esc closes); the mouse clicks a card, a
//     right click opens the radial, a click on a wedge orders.
//   - The radial: seven wedges in party::Cmd order (FOLLOW at the top, clockwise), each with its pixel icon; the Skill
//     wedge names the best ready bond skill, its cooldown or the bond that unlocks it; an order that cannot be given
//     (rules mirrored from Game::partyCommand, and its own refusal) flashes red and says why, by the card too.
//   - Ev::PartyAlert: a short ribbon at the top centre with the member's face, in the kind's colour (one at a time).
//   - Ev::Banter: a plate over the speaker, wrapped, kept off the HUD column, the quest column, the touch cluster and
//     the name tag; by their card when they are out of sight; never while a dialogue or a menu is open.
#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <unordered_map>
#include <vector>
#include "rpg/sim/game.h"
#include "rpg/sim/party.h"
#include "rpg/view/view.h"

namespace {
const Color kGoldP(0.98f, 0.82f, 0.42f), kTextP(0.93f, 0.9f, 0.82f), kDimP(0.62f, 0.58f, 0.52f);
constexpr float kRO = 56, kRI = 19;                    // the radial's outer and inner radius
constexpr float kRLabel = 58;                          // (fixer M8 r2) room beside the rim for the wedges' words (touch)
// (fixer M8 r2) each wedge's word round the rim on touch (by Cmd: Follow Wait Attack Defend Skill Passive Home)
const char* const kRWord[7] = {"FOLLOW", "WAIT", "ATTACK", "DEFEND", "SKILL", "HOLD BACK", "HOME"};
constexpr int kWedges = (int)party::Cmd::COUNT;
constexpr int kRC = 59, kRN = 2 * kRC + 1;             // the wedge canvas: kRN square, centre kRC
constexpr float kCard = 30, kCardH = 35, kPitch = 37;  // a big card: frame, frame + health bar, row pitch
constexpr float kSmall = 18, kSmallPitch = 21;
// (fixer M8 r3, review: "small busts are ~28x32 pt hit targets, their areas overlap") on touch each small bust owns a
// 33 x 33 logical cell (about 44 pt on the iPhone canvas): the busts stand that far apart and the cells never overlap
constexpr float kSmallCellT = 33;
constexpr float kHoldSecs = 0.32f;
// (fixer M8 r3, review: "the radial on a WAITING companion lights FOLLOW") the wedge lit when the ring opens: the stance
// of one travelling with the player; none for one waiting or at home (the caption says where they are), so no wedge
// reads as what they are doing when it is not, and every first tap only lights its wedge
int openHot(const party::Member& m) { return m.state == party::State::With ? (int)m.stance : -1; }
struct LookMemo { art::HumanLook look; float t = -1; };
std::unordered_map<uint32_t, LookMemo> g_lookMemo;   // Member::id -> the look last dressed (members not in play)

uint64_t ptag(uint64_t tag, uint64_t v) { return (tag << 56) ^ (v & 0x00FFFFFFFFFFFFFFull); }
Color colU(uint32_t c, float a = 1) { return Color((c & 255) / 255.0f, ((c >> 8) & 255) / 255.0f, ((c >> 16) & 255) / 255.0f, a); }
uint32_t mulc(uint32_t c, float k) {
  auto ch = [&](int s) { return (uint32_t)std::clamp((int)std::lround(((c >> s) & 255) * k), 0, 255) << s; };
  return ch(0) | ch(8) | ch(16) | (c & 0xFF000000u);
}
uint32_t mixc(uint32_t a, uint32_t b, float t) {
  auto ch = [&](int s) { return (uint32_t)std::clamp((int)std::lround(((a >> s) & 255) * (1 - t) + ((b >> s) & 255) * t), 0, 255) << s; };
  return ch(0) | ch(8) | ch(16) | (a & 0xFF000000u);
}
std::vector<std::string> wrapP(const std::string& s, int maxChars) {
  std::vector<std::string> lines;
  std::string line, word;
  auto flush = [&]() {
    if (word.empty()) return;
    if (!line.empty() && (int)(line.size() + 1 + word.size()) > maxChars) { lines.push_back(line); line.clear(); }
    if (!line.empty()) line += ' ';
    line += word;
    word.clear();
  };
  for (char c : s) {
    if (c == ' ') flush();
    else if (c == '\n') { flush(); lines.push_back(line); line.clear(); }
    else word += c;
  }
  flush();
  if (!line.empty()) lines.push_back(line);
  return lines;
}
// the vocation's colour (the card's backdrop, the radial's hub)
uint32_t vocColour(party::Vocation v) {
  switch (v) {
    case party::Vocation::Warrior: return rgba(150, 62, 48);
    case party::Vocation::Archer: return rgba(72, 116, 60);
    case party::Vocation::Mage: return rgba(66, 82, 160);
    case party::Vocation::Healer: return rgba(170, 146, 82);
    case party::Vocation::Rogue: return rgba(104, 66, 120);
    default: return rgba(124, 92, 60);
  }
}
// an alert's colour by kind (Ev::PartyAlert f: 1 low health ... 8 a skill learned)
Color alertColour(int kind) {
  switch (kind) {
    case 1: return Color(1.0f, 0.70f, 0.32f);
    case 2: return Color(1.0f, 0.36f, 0.30f);
    case 3: return Color(0.55f, 0.95f, 0.55f);
    case 4: return Color(0.80f, 0.78f, 0.86f);
    case 5: return kGoldP;
    case 6: return Color(0.62f, 0.70f, 0.86f);
    case 7: return Color(0.92f, 0.62f, 0.40f);
    case 8: return Color(0.62f, 0.82f, 1.0f);
    default: return kTextP;
  }
}

// ---------------------------------------------------------------- pixel glyphs (the radial's icons, the stance badges)
// 'o' outline, 'H' light, 'M' mid, 'S' shade, 'W' glint, 'D' dark detail; lit from the top left
struct GlyphPal { uint32_t H, M, S, W, D; };
Canvas paintGlyph(const char* const* rows, int n, const GlyphPal& pal) {
  Canvas c(n, n);
  for (int y = 0; y < n; y++)
    for (int x = 0; x < n; x++) {
      uint32_t v = 0;
      switch (rows[y][x]) {
        case 'o': v = rgba(14, 10, 14); break;
        case 'H': v = pal.H; break;
        case 'M': v = pal.M; break;
        case 'S': v = pal.S; break;
        case 'W': v = pal.W; break;
        case 'D': v = pal.D; break;
        default: break;
      }
      c.set(x, y, v);
    }
  return c;
}
// the seven orders' icons (11 x 11), in party::Cmd order
const char* const kIcoFollow[11] = {"....ooo....", "...oHHMo...", "..oHMoMSo..", ".oHMo.oMSo.", "oHMo...oMSo", "ooo.ooo.ooo",
                                    "...oHHMo...", "..oHMoMSo..", ".oHMo.oMSo.", "oHMo...oMSo", "ooo.....ooo"};
const char* const kIcoWait[11] = {"ooooooooooo", "oHHHHHHHMMo", ".oWHHHMMSo.", "..oDDDDDo..", "...oDDDo...", "....oDo....",
                                  "...oSHSo...", "..oSHDHSo..", ".oSDDDDDSo.", "oHHHHHHHMMo", "ooooooooooo"};
const char* const kIcoAttack[11] = {"........ooo", ".......oWHo", "......oWHMo", ".....oWHMo.", "oo..oWHMo..", "oDooWHMo...",
                                    ".oDDHMo....", "..oDDo.....", ".oDooDo....", "oDo..oDo...", "oo....oo..."};
const char* const kIcoDefend[11] = {"ooooooooooo", "oHHHHDMMMSo", "oHWHHDMMMSo", "oHHHHDMMSSo", "oDDDDDDDDDo", ".oHHHDMMSo.",
                                    ".oHHHDMSSo.", "..oHHDMSo..", "...oHDSo...", "....oDo....", ".....o....."};
const char* const kIcoSkill[11] = {".....o.....", "....oWo....", "....oHo....", "...oHWHo...", "oooHHWHMooo", "oWHHWWWHMSo",
                                   "oooMHWHSooo", "...oMHSo...", "....oMo....", "....oSo....", ".....o....."};
const char* const kIcoPassive[11] = {"..o.o.o....", ".oHoHoHo...", ".oHoHoHoo..", ".oHoHoHoHo.", ".oHHHHHoHo.", ".oHHHHHHMo.",
                                     "ooHHHHHHMo.", "oHoHHHHHMo.", ".oHHHHHMSo.", "..oHHHMSo..", "...ooooo..."};
const char* const kIcoHome[11] = {".....o.....", "....oHo....", "...oHHMo...", "..oHHHMMo..", ".oHHHHMMSo.", "ooooooooooo",
                                  ".oWWWWWWSo.", ".oWWoooWSo.", ".oWWoDoWSo.", ".oWWoDoWSo.", ".ooooooooo."};
const char* const* kIcons[kWedges] = {kIcoFollow, kIcoWait, kIcoAttack, kIcoDefend, kIcoSkill, kIcoPassive, kIcoHome};
const GlyphPal kIconPal[kWedges] = {
    {rgba(250, 222, 140), rgba(214, 170, 82), rgba(150, 104, 44), rgba(255, 246, 210), rgba(90, 60, 30)},     // follow: gold
    {rgba(222, 196, 140), rgba(176, 140, 86), rgba(118, 88, 52), rgba(255, 244, 214), rgba(232, 200, 120)},   // wait: sand glass
    {rgba(226, 232, 240), rgba(170, 178, 192), rgba(104, 110, 128), rgba(255, 255, 255), rgba(150, 52, 40)},  // attack: steel, red hilt
    {rgba(120, 156, 214), rgba(76, 106, 170), rgba(46, 64, 116), rgba(220, 236, 255), rgba(214, 180, 96)},    // defend: blue, gold cross
    {rgba(170, 214, 255), rgba(112, 160, 236), rgba(64, 96, 176), rgba(255, 255, 255), rgba(40, 60, 120)},    // skill: star
    {rgba(242, 204, 168), rgba(214, 164, 124), rgba(160, 112, 82), rgba(255, 236, 214), rgba(110, 70, 50)},   // hold back: palm
    {rgba(222, 104, 80), rgba(176, 70, 54), rgba(120, 44, 36), rgba(244, 232, 204), rgba(96, 62, 38)},       // home: red roof
};
// the stance badges (7 x 7): follow, attack, defend, hold back, waiting, at home
const char* const kBadgeFollow[7] = {"...o...", "..oHo..", ".oHoSo.", "oHo.oSo", "..oHo..", ".oHoSo.", "oHo.oSo"};
const char* const kBadgeAttack[7] = {".....oo", "....oWo", "...oWo.", "o.oWo..", ".oDo...", ".oDDo..", "o...o.."};
const char* const kBadgeDefend[7] = {"ooooooo", "oHHDMSo", "oDDDDDo", "oHHDMSo", ".oHDSo.", "..oDo..", "...o..."};
const char* const kBadgePassive[7] = {".o.o.o.", ".oHoHoo", ".oHoHoH", "oHHHHHo", "oHHHHMo", ".oHHMo.", "..ooo.."};
const char* const kBadgeWait[7] = {"ooooooo", "oHHHHMo", ".oDDDo.", "..oDo..", ".oHDSo.", "oHHHHMo", "ooooooo"};
const char* const kBadgeHome[7] = {"...o...", "..oHo..", ".oHHMo.", "ooooooo", ".oWWWo.", ".oWDWo.", ".ooooo."};

// one wedge of the radial (state 0 normal, 1 chosen, 2 cannot be given now), on a kRN square canvas
Canvas paintWedge(int k, int state) {
  Canvas c(kRN, kRN);
  const float step = 6.2831853f / kWedges, half = step / 2, mid = -1.5707963f + k * step;
  const uint32_t fill = state == 1 ? rgba(110, 80, 40, 240) : state == 2 ? rgba(30, 27, 30, 228) : rgba(40, 32, 36, 236);
  const uint32_t rim = state == 1 ? rgba(252, 216, 112) : state == 2 ? rgba(92, 82, 70) : rgba(178, 140, 78);
  const uint32_t side = state == 1 ? rgba(200, 156, 80) : state == 2 ? rgba(62, 56, 52) : rgba(110, 86, 52);
  for (int y = 0; y < kRN; y++)
    for (int x = 0; x < kRN; x++) {
      const float dx = (float)(x - kRC), dy = (float)(y - kRC);
      const float r = std::sqrt(dx * dx + dy * dy);
      if (r < kRI || r > kRO) continue;
      float d = std::atan2(dy, dx) - mid;
      while (d > 3.14159265f) d -= 6.2831853f;
      while (d < -3.14159265f) d += 6.2831853f;
      const float ad = std::fabs(d);
      if (ad > half) continue;
      const float es = (half - ad) * r;   // px to the wedge's side
      if (es < 1.6f) continue;
      const float eo = kRO - r, ei = r - kRI, t = (r - kRI) / (kRO - kRI);
      const float lit = -(dx * 0.7071f + dy * 0.7071f) / std::max(1.0f, r);   // > 0 toward the top left
      uint32_t v = mulc(fill, 0.86f + 0.26f * t);
      if (eo < 1.0f) v = mulc(rim, 1.0f + 0.22f * lit);
      else if (ei < 1.0f) v = mulc(rim, 0.62f + 0.12f * lit);
      else if (es < 2.6f) v = mulc(side, 1.0f + 0.15f * lit);
      else if (eo < 2.0f) v = lit > 0.15f ? mixc(v, rgba(255, 236, 190), 0.30f) : mulc(v, 0.72f);   // the bevel: lit up-left
      else if (ei < 2.0f) v = lit < -0.15f ? mixc(v, rgba(255, 236, 190), 0.16f) : mulc(v, 0.70f);
      c.set(x, y, v | 0xFF000000u);
      if (state != 1 && eo >= 1.0f) c.set(x, y, (v & 0x00FFFFFFu) | (state == 2 ? 0xE4000000u : 0xEC000000u));
    }
  // the sprite convention: a dark outline round it
  Canvas o = c;
  for (int y = 0; y < kRN; y++)
    for (int x = 0; x < kRN; x++) {
      if (c.get(x, y) >> 24) continue;
      bool n = false;
      for (int q = 0; q < 4 && !n; q++) n = (c.get(x + (q == 0) - (q == 1), y + (q == 2) - (q == 3)) >> 24) != 0;
      if (n) o.set(x, y, rgba(10, 7, 10, 235));
    }
  return o;
}
// the hub: a dark disc in a gold ring (the member's face goes in it)
Canvas paintHub(uint32_t voc) {
  const int R = (int)kRI - 3, N = 2 * R + 3;
  Canvas c(N, N);
  for (int y = 0; y < N; y++)
    for (int x = 0; x < N; x++) {
      const float dx = (float)(x - N / 2), dy = (float)(y - N / 2), r = std::sqrt(dx * dx + dy * dy);
      if (r > R + 0.5f) continue;
      const float lit = -(dx + dy) * 0.7071f / std::max(1.0f, r);
      uint32_t v = mixc(rgba(26, 20, 24), voc, 0.28f + 0.18f * (1 - r / R));
      if (r > R - 1.0f) v = mulc(rgba(200, 160, 88), 1.0f + 0.25f * lit);
      else if (r > R - 2.0f) v = rgba(30, 22, 18);
      c.set(x, y, v | 0xFF000000u);
    }
  return c;
}
// a bust's face, cropped for the small cards and the alert chips (14 x 14 of the 24 x 24 portrait: the head)
constexpr int kFace = 14, kFaceX = (art::PORTRAIT_W - kFace) / 2, kFaceY = 2;

// a point on the perimeter of a w x h rectangle, clockwise from its top centre (i in 0 .. 2(w + h) - 4)
void perimPoint(int i, int w, int h, int& px, int& py) {
  const int top = w / 2;
  const int per = 2 * (w + h) - 4;
  i = ((i % per) + per) % per;
  int s = (i + top) % per;   // from the top-left corner going right
  if (s < w) { px = s; py = 0; return; }
  s -= w - 1;
  if (s < h) { px = w - 1; py = s; return; }
  s -= h - 1;
  if (s < w) { px = w - 1 - s; py = h - 1; return; }
  s -= w - 1;
  px = 0; py = h - 1 - s;
}
}  // namespace

// ---------------------------------------------------------------- look-ups
int View::memberOf(const Game& g, uint32_t id) const {
  for (int i = 0; i < (int)g.party.members.size(); i++) if (g.party.members[(size_t)i].id == id) return i;
  return -1;
}
int View::companionIndex(const Game& g, const Actor& a) const {
  if (!a.comp) return -1;
  const int mi = a.comp - 1;
  return mi < (int)g.party.members.size() ? mi : -1;
}

// the sim's look for the animal (Game::partyHoundLook: age from the days since it was taken in, collar by the bond)
art::HoundLook View::houndLookOf(const party::Member& m, const Game& g) const {
  return g.partyHoundLook(memberOf(g, m.id));
}
const Tex& View::houndTex(const party::Member& m, const Game& g) {
  const art::HoundLook L = houndLookOf(m, g);
  const uint64_t k = ptag(0x21, L.key());
  auto it = partyTex_.find(k);
  if (it == partyTex_.end()) it = partyTex_.emplace(k, pix_->bake(art::houndSheet(L))).first;
  return it->second;
}
const Tex& View::portraitTex(const Game& g, const party::Member& m, int mood) {
  const int mi = memberOf(g, m.id);
  const bool hound = m.voc == party::Vocation::Hound;
  uint64_t lk = 0;
  art::HumanLook look;
  if (hound) lk = houndLookOf(m, g).key();
  else {
    const int ai = mi >= 0 && mi < (int)partyActor_.size() ? partyActor_[(size_t)mi] : -1;
    if (ai > 0) look = g.actors[(size_t)ai].look;
    else {   // not in play (waiting, at home): their look is dressed afresh now and then, not every frame
      if (g_lookMemo.size() > 32) g_lookMemo.clear();
      LookMemo& lm = g_lookMemo[m.id];
      if (lm.t < 0 || t_ - lm.t > 1.5f || lm.t > t_) { lm.look = g.partyLook(m); lm.t = t_; }
      look = lm.look;
    }
    lk = look.key();
  }
  const uint64_t k = ptag(hound ? 0x22 : 0x23, lk * 31u + (uint64_t)mood);
  auto it = partyTex_.find(k);
  if (it != partyTex_.end()) return it->second;
  const Canvas c = hound ? art::houndPortrait(houndLookOf(m, g), mood) : art::portraitSprite(look, mood);
  return partyTex_.emplace(k, pix_->bake(c)).first->second;
}

int View::bestSkill(const Game& g, int mi, float& cd, bool& locked) const {
  cd = 0; locked = true;
  if (mi < 0 || mi >= (int)g.party.members.size()) return -1;
  const party::Member& m = g.party.members[(size_t)mi];
  const uint8_t un = party::unlockedSkills(m);
  if (!un) return 0;   // the first, locked
  locked = false;
  int best = -1;
  float bcd = 1e9f;
  for (int k = 2; k >= 0; k--) {
    if (!(un & (1u << k))) continue;
    if (m.skillCd[k] <= 0) { cd = 0; return k; }
    if (m.skillCd[k] < bcd) { bcd = m.skillCd[k]; best = k; }
  }
  cd = bcd;
  return best;
}

std::string View::radialWhy(const Game& g, int mi, int cmd) const {
  if (mi < 0 || mi >= (int)g.party.members.size()) return "THEY ARE GONE";
  const party::Member& m = g.party.members[(size_t)mi];
  using party::Cmd;
  using party::State;
  if (m.state == State::Downed) return "BIND THEIR WOUNDS FIRST";
  if (m.state == State::Dead) return "THEY ARE GONE";
  const Cmd c = (Cmd)cmd;
  switch (c) {
    case Cmd::Follow: case Cmd::Attack: case Cmd::Defend: case Cmd::Passive:
      if (m.state == State::Left) return "THEY HAVE LEFT YOU";
      if ((m.state == State::Waiting || m.state == State::Home) && !g.partyCanCall(mi)) return "TOO FAR: GO AND FETCH THEM";
      if (m.state != State::With && m.voc != party::Vocation::Hound && g.party.people() >= party::capacity(g.plLevel))
        return "NO ROOM IN THE PARTY";
      return "";
    case Cmd::Wait: return m.state == State::With ? "" : m.state == State::Waiting ? "ALREADY WAITING" : "NOT WITH YOU";
    case Cmd::Home: return m.state == State::Left ? "THEY HAVE LEFT YOU" : m.state == State::Home ? "ALREADY HOME" : "";
    case Cmd::Skill: {
      if (m.state != State::With) return "NOT WITH YOU";
      float cd = 0;
      bool locked = true;
      const int k = bestSkill(g, mi, cd, locked);
      if (locked) return "UNLOCKS AT BOND " + std::to_string((int)party::SKILL_BOND[0]);
      if (k < 0) return "NO SKILL YET";
      if (cd > 0) return "READY IN " + std::to_string((int)std::ceil(cd)) + "S";
      return "";
    }
    default: return "";
  }
}

// ---------------------------------------------------------------- events and timers
void View::partyEvent(Game& g, const Event& e) {
  // the speaker by Member::id (Event::id): e.a, their index when it was said, may name someone else by now (a member
  // who died that step is already gone from the list); staged events without an id use the index
  uint32_t id = e.id;
  if (!id) {
    if (e.a < 0 || e.a >= (int)g.party.members.size()) return;
    id = g.party.members[(size_t)e.a].id;
  }
  const int mi = memberOf(g, id);
  if (e.type == Ev::Banter) {
    if (e.s.empty() || mi < 0) return;   // (the dead say nothing more)
    for (size_t i = 0; i < banterShow_.size();)   // their new line replaces the old one
      if (banterShow_[i].id == id) banterShow_.erase(banterShow_.begin() + (std::ptrdiff_t)i);
      else i++;
    // (fixer M8 r1) two companions never say the very same words at the same moment: the second keeps quiet
    for (const PartyNote& o : banterShow_) if (o.s == e.s) return;
    PartyNote n;
    n.id = id; n.s = e.s;
    int words = 1;
    for (char ch : e.s) if (ch == ' ') words++;
    n.life = std::clamp(std::max(e.f, 1.6f + 0.3f * (float)words), 2.0f, 7.0f);
    banterShow_.push_back(n);
    if (banterShow_.size() > 3) banterShow_.erase(banterShow_.begin());
    return;
  }
  // a PartyAlert: queued, one ribbon at a time (the same line twice in a row is said once)
  PartyNote n;
  n.id = id; n.kind = (int)std::lround(e.f); n.s = e.s;
  if (n.s.empty()) return;
  n.life = n.kind == 4 ? 5.0f : n.kind == 2 ? 4.0f : n.kind == 5 ? 2.4f : 3.2f;
  if (!alertShow_.empty() && alertShow_.back().s == n.s) return;
  // (fixer M8 r2, review: "join toasts lag several seconds behind the event; JOINS YOU still up during I'M DOWN")
  // danger (downed, badly hurt) goes to the head of the line and cuts the ribbon on show short
  if ((n.kind == 2 || n.kind == 1) && !alertShow_.empty()) {
    PartyNote& f = alertShow_.front();
    if (f.kind != 2 && f.kind != 1) f.life = std::min(f.life, f.t + 0.35f);
    size_t at = 1;
    while (at < alertShow_.size() && (alertShow_[at].kind == 2 || alertShow_[at].kind == 1)) at++;
    alertShow_.insert(alertShow_.begin() + (std::ptrdiff_t)at, n);
  } else {
    alertShow_.push_back(n);
  }
  if (alertShow_.size() > 5) alertShow_.erase(alertShow_.begin() + 1);
  if ((n.kind == 2 || n.kind == 1) && mi >= 0) partySel_ = mi;   // the one in trouble is the one the radial opens on
}

void View::partyUpdate(Game& g, float dt) {
  // the members' actors this frame
  partyActor_.assign(g.party.members.size(), -1);
  for (size_t i = 1; i < g.actors.size(); i++) {
    const Actor& a = g.actors[i];
    if (a.comp && a.comp <= g.party.members.size() && a.st != AState::Dead) partyActor_[(size_t)a.comp - 1] = (int)i;
  }
  if (g.party.members.empty()) { radial_ = Radial(); partyPress_ = PartyPress(); }
  partySel_ = std::clamp(partySel_, 0, std::max(0, (int)g.party.members.size() - 1));
  const bool play = g.mode == Mode::Play;
  // the radial closes when the game leaves play or its member is gone
  if (radial_.open && (!play || radial_.member < 0 || radial_.member >= (int)g.party.members.size())) radial_.open = false;
  if (radial_.open) radial_.t += dt;
  radial_.denyT = std::max(0.0f, radial_.denyT - dt);
  partyDenyT_ = std::max(0.0f, partyDenyT_ - dt);
  partyTagT_ = std::max(0.0f, partyTagT_ - dt);
  // a hold on a card opens the radial round it
  if (partyPress_.on && play && !radial_.open) {
    partyPress_.t += dt;
    if (partyPress_.t >= kHoldSecs && len2(partyPress_.cur - partyPress_.start) < 10.0f * 10.0f) {
      radialOpen(g, partyPress_.member, true, partyPress_.id);
      radial_.from = partyPress_.cur;
      partyPress_.on = false;
    }
  }
  // (fixer M8 r1) a new place is a new scene: the lines said in the last one go with it
  if (g.party.mapChanges != partyMapSeen_) {
    if (partyMapSeen_ >= 0) banterShow_.clear();
    partyMapSeen_ = g.party.mapChanges;
  }
  // banter and alerts run on the play clock (a dialogue or a menu holds them)
  if (play) {
    // (fixer M8 r3, review: "plates pinned top-centre above the title card, away from the speaker; two 3-line plates
    // bracket the player on a phone") a line runs its clock only while it is on show over its speaker: it waits out
    // the arrival card and the herald, and for a free place by its speaker (drawBanter marks it shown); a line kept
    // waiting too long is let go unsaid
    const bool cards = (bannerT_ > 0 && !bannerSub_.empty()) || herald_.t > 0;
    for (size_t i = 0; i < banterShow_.size();) {
      PartyNote& n = banterShow_[i];
      if (n.shown && !cards) n.t += dt;
      else n.held += dt;
      n.shown = false;
      if (n.t > n.life || n.held > 6.0f || memberOf(g, n.id) < 0) banterShow_.erase(banterShow_.begin() + (std::ptrdiff_t)i);
      else i++;
    }
    // (fixer M8 r1, review: "the join ribbon still shows several scenes later") the ribbon ages on the play clock even
    // under the arrival banner (it stands below the banner then: partyRibbonBox), so it never waits out a string of
    // banners and follows the player through doors and travel
    if (!alertShow_.empty()) {
      // (fixer M8 r2) with others waiting, the ribbon on show hurries once it has been read (a second): a string of
      // joins is told while it is still news, not eight seconds on
      const float hurry = alertShow_.front().t > 1.0f ? 1.0f + 1.6f * (float)(alertShow_.size() - 1) : 1.0f;
      alertShow_.front().t += dt * hurry;
      if (alertShow_.front().t > alertShow_.front().life) alertShow_.erase(alertShow_.begin());
    }
  }
  // the bonded hound howls at the night now and then (outdoors, travelling with the player)
  if (play && !g.inside && g.daylight() < 0.25f) {
    bool hound = false;
    Vec2 at;
    for (int mi = 0; mi < (int)g.party.members.size(); mi++) {
      const party::Member& m = g.party.members[(size_t)mi];
      if (m.voc == party::Vocation::Hound && m.state == party::State::With && partyActor_[(size_t)mi] > 0) { hound = true; at = g.actors[(size_t)partyActor_[(size_t)mi]].p; }
    }
    if (hound) {
      howlT_ -= dt;
      if (howlT_ <= 0) {
        const float d = len(at - g.pl().p);
        audio_->play(Sfx::Howl, 0.94f + 0.12f * (float)(hash32((uint32_t)(t_ * 1000)) % 100u) / 100.0f, clampf(1.0f - d / 400.0f, 0.35f, 0.9f));
        howlT_ = 45.0f + (float)(hash32((uint32_t)(t_ * 977) ^ 0x4F1Bu) % 60u);
      }
    }
  }
}

// ---------------------------------------------------------------- the column
void View::drawPortraitCard(Game& g, int mi, float x, float y, bool small) {
  Pix& P = *pix_;
  const party::Member& m = g.party.members[(size_t)mi];
  const int ai = mi < (int)partyActor_.size() ? partyActor_[(size_t)mi] : -1;
  float hp = 1;
  if (ai > 0 && g.actors[(size_t)ai].maxHp > 0) hp = clampf(g.actors[(size_t)ai].hp / g.actors[(size_t)ai].maxHp, 0, 1);
  else if (m.hp >= 0) hp = clampf(m.hp / std::max(1.0f, party::bodyFor(m, g.plLevel).maxHp), 0, 1);
  const bool downed = m.state == party::State::Downed;
  const bool low = !downed && m.state == party::State::With && hp * 100 < party::LOW_HP_PCT;
  const int mood = downed ? 2 : low ? 1 : 0;
  const Tex& pt = portraitTex(g, m, mood);
  const bool sel = mi == partySel_;
  const uint32_t vc = vocColour(m.voc);
  if (small) {
    // a waiting or at-home member: the face alone, small and dimmed, with what they are doing in the corner
    const float s = kSmall;
    P.rect(x + 1, y + 1, s, s, Color(0, 0, 0, 0.35f));
    P.rect(x, y, s, s, Color(0.06f, 0.05f, 0.07f, 0.85f));
    P.rect(x + 2, y + 2, s - 4, s - 4, colU(mulc(vc, 0.35f), 0.85f));
    P.blitEx(pt, kFaceX, kFaceY, kFace, kFace, x + 2, y + 2, (float)kFace, (float)kFace, false, Color(0.62f, 0.62f, 0.68f, 0.85f));
    P.frame(x, y, s, s, sel ? kGoldP : Color(0.40f, 0.33f, 0.22f, 0.9f));
    const bool home = m.state == party::State::Home;
    const char* const* bg = home ? kBadgeHome : kBadgeWait;
    const GlyphPal pal = home ? kIconPal[6] : kIconPal[1];
    const uint64_t bk = ptag(0x31, home ? 6 : 1);
    auto it = partyTex_.find(bk);
    if (it == partyTex_.end()) it = partyTex_.emplace(bk, pix_->bake(paintGlyph(bg, 7, pal))).first;
    P.blit(it->second, x + s - 6, y + s - 6);
    return;
  }
  // the card: a drop shadow, the vocation's backdrop lit from the top left, the bust, the gold double frame
  const float w = kCard;
  P.rect(x + 2, y + 2, w, kCardH, Color(0, 0, 0, 0.32f));
  P.rect(x, y, w, w, Color(0.05f, 0.04f, 0.06f, 0.92f));
  for (int r = 0; r < 24; r++) {   // the backdrop: lighter up top and to the left
    const float k = 0.62f - 0.30f * r / 23.0f;
    P.rect(x + 3, y + 3 + r, 24, 1, colU(mulc(vc, downed ? k * 0.45f : k), 0.95f));
  }
  P.rect(x + 3, y + 3, 9, 6, Color(1, 0.95f, 0.85f, downed ? 0.03f : 0.07f));
  P.blit(pt, x + 3, y + 3);
  if (low) {   // the low-health pulse: a red wash over the bust and a beating frame
    const float k = 0.5f + 0.5f * std::sin(t_ * 9);
    P.rect(x + 3, y + 3, 24, 24, Color(0.9f, 0.1f, 0.08f, 0.10f + 0.12f * k));
  }
  // the frame (outer lit top-left, shaded bottom-right; inner dark), corner studs
  const Color fo = sel ? kGoldP : low ? Color(0.85f + 0.15f * std::sin(t_ * 9), 0.28f, 0.2f) : Color(0.62f, 0.50f, 0.30f);
  P.rect(x, y, w, 1, fo);
  P.rect(x, y, 1, w, fo);
  P.rect(x, y + w - 1, w, 1, Color(fo.r * 0.62f, fo.g * 0.62f, fo.b * 0.62f));
  P.rect(x + w - 1, y, 1, w, Color(fo.r * 0.62f, fo.g * 0.62f, fo.b * 0.62f));
  P.frame(x + 1, y + 1, w - 2, w - 2, Color(0.20f, 0.15f, 0.10f));
  P.frame(x + 2, y + 2, w - 4, w - 4, Color(0.05f, 0.04f, 0.05f, 0.8f));
  for (float cx : {x, x + w - 2}) for (float cy : {y, y + w - 2}) P.rect(cx, cy, 2, 2, sel ? Color(1, 0.95f, 0.7f) : Color(0.9f, 0.75f, 0.4f));
  if (sel) P.frame(x - 1, y - 1, w + 2, w + 2, Color(kGoldP.r, kGoldP.g, kGoldP.b, 0.35f + 0.2f * std::sin(t_ * 3)));
  // the health bar (or the binding's progress while they are down)
  const float by = y + w + 1;
  P.rect(x, by, w, 4, Color(0.04f, 0.03f, 0.05f, 0.9f));
  if (downed) {
    const float k = clampf(m.reviveT / party::REVIVE_SECS, 0, 1);
    P.rect(x + 1, by + 1, w - 2, 2, Color(0.22f, 0.05f, 0.05f, 0.9f));
    if (k > 0) {
      P.rect(x + 1, by + 1, (w - 2) * k, 2, kGoldP);
      P.rect(x + 1, by + 1, (w - 2) * k, 1, Color(1, 0.96f, 0.8f));
    }
  } else {
    P.rect(x + 1, by + 1, w - 2, 2, Color(0.30f, 0.05f, 0.05f, 0.9f));
    const Color hc = hp > 0.6f ? Color(0.36f, 0.82f, 0.36f) : hp > 0.3f ? Color(0.92f, 0.74f, 0.26f) : Color(0.92f, 0.22f, 0.18f);
    P.rect(x + 1, by + 1, (w - 2) * hp, 2, hc);
    P.rect(x + 1, by + 1, (w - 2) * hp, 1, Color(std::min(1.0f, hc.r + 0.25f), std::min(1.0f, hc.g + 0.25f), std::min(1.0f, hc.b + 0.25f)));
  }
  if (downed) {
    // the fuse: a red line round the frame that burns down clockwise over DOWNED_SECS, its head a spark
    const int fw = (int)w + 2, fh = (int)w + 2, per = 2 * (fw + fh) - 4;
    const float k = clampf(m.downT / party::DOWNED_SECS, 0, 1);
    const int lit = (int)std::lround(per * k);
    const float pulse = 0.65f + 0.35f * std::sin(t_ * (k < 0.33f ? 14.0f : 7.0f));
    for (int i = 0; i < per; i++) {
      int px = 0, py = 0;
      perimPoint(i, fw, fh, px, py);
      const bool on = i < lit;
      const Color c = on ? Color(0.95f, 0.18f + 0.1f * pulse, 0.12f, 0.95f) : Color(0.18f, 0.06f, 0.06f, 0.75f);
      P.rect(x - 1 + px, y - 1 + py, 1, 1, c);
      // a second, outer line so the fuse reads at phone size
      const int ox = px == 0 ? -1 : px == fw - 1 ? 1 : 0, oy = py == 0 ? -1 : py == fh - 1 ? 1 : 0;
      P.rect(x - 1 + px + ox, y - 1 + py + oy, 1, 1, on ? Color(0.75f, 0.10f, 0.08f, 0.8f * pulse) : Color(0, 0, 0, 0.4f));
    }
    if (lit > 0) {   // the burning head
      int px = 0, py = 0;
      perimPoint(lit - 1, fw, fh, px, py);
      P.rect(x - 2 + px, y - 2 + py, 3, 3, Color(1.0f, 0.75f, 0.35f, pulse));
      P.rect(x - 1 + px, y - 1 + py, 1, 1, Color(1, 1, 0.9f));
    }
    // the seconds left, on a dark plate over the bust's chest
    const std::string sec = std::to_string((int)std::ceil(std::max(0.0f, m.downT)));
    const float tw = (float)P.textW(sec, 1);
    P.rect(x + w / 2 - tw / 2 - 2, y + 18, tw + 4, 9, Color(0.05f, 0.02f, 0.02f, 0.78f));
    P.textS(x + w / 2, y + 19, sec, 1, Color(1.0f, 0.42f, 0.34f), 1);
  } else {
    // the stance badge in the bottom-right corner
    int bi = 0;
    switch (m.stance) {
      case party::Cmd::Attack: bi = 1; break;
      case party::Cmd::Defend: bi = 2; break;
      case party::Cmd::Passive: bi = 3; break;
      default: bi = 0; break;
    }
    static const char* const* const kBadges[4] = {kBadgeFollow, kBadgeAttack, kBadgeDefend, kBadgePassive};
    static const int kBadgePal[4] = {0, 2, 3, 5};
    const uint64_t bk = ptag(0x31, 10 + bi);
    auto it = partyTex_.find(bk);
    if (it == partyTex_.end()) it = partyTex_.emplace(bk, pix_->bake(paintGlyph(kBadges[bi], 7, kIconPal[kBadgePal[bi]]))).first;
    P.rect(x + w - 11, y + w - 11, 11, 11, Color(0.05f, 0.04f, 0.06f, 0.92f));
    P.frame(x + w - 11, y + w - 11, 11, 11, Color(0.62f, 0.50f, 0.30f));
    P.blit(it->second, x + w - 9, y + w - 9);
  }
}

void View::drawPartyHud(Game& g) {
  Pix& P = *pix_;
  partyCards_.clear();
  partyColR_ = 0; partyColB_ = 0;
  if (g.party.members.empty()) return;
  if (partyActor_.size() != g.party.members.size()) partyActor_.assign(g.party.members.size(), -1);
  if (g.mode == Mode::Dialogue) return;   // the conversation is modal: the column steps away under its panel
  const float L = (float)Pix::SL, T = (float)Pix::ST;
  float y = T + 46 + (g.perkPts > 0 ? 10.0f : 0.0f) + (g.blessT > 0 ? 10.0f : 0.0f);
  const float x = L + 5;
  // the big cards: travelling with the player
  for (int mi = 0; mi < (int)g.party.members.size(); mi++) {
    const party::State s = g.party.members[(size_t)mi].state;
    if (s != party::State::With && s != party::State::Downed) continue;
    drawPortraitCard(g, mi, x, y, false);
    partyCards_.push_back({mi, x - 2, y - 2, kCard + 6, kPitch, false});
    y += kPitch;
  }
  // the small ones: waiting and at home (two to a row, at most four; "+N" past them)
  int shown = 0, extra = 0;
  float smallR = 0;
  for (int mi = 0; mi < (int)g.party.members.size(); mi++) {
    const party::State s = g.party.members[(size_t)mi].state;
    if (s != party::State::Waiting && s != party::State::Home) continue;
    if (shown >= 4) { extra++; continue; }
    const float pitch = touchUI ? kSmallCellT : kSmallPitch;
    const float sx = x + (shown & 1) * pitch, sy = y + (shown / 2) * pitch;
    drawPortraitCard(g, mi, sx, sy, true);
    if (touchUI) {   // the bust's whole cell, centred on it
      const float pad = (kSmallCellT - kSmall) / 2;
      partyCards_.push_back({mi, sx - pad, sy - pad + 2, kSmallCellT, kSmallCellT, true});
      smallR = std::max(smallR, sx + kSmall + 4);
    } else {
      partyCards_.push_back({mi, sx - 2, sy - 2, kSmall + 3, kSmall + 3, true});
    }
    shown++;
  }
  if (shown) y += ((shown + 1) / 2) * (touchUI ? kSmallCellT : kSmallPitch);
  if (extra) { P.textS(x + 2, y, "+" + std::to_string(extra), 1, kDimP); y += 10; }
  partyColR_ = std::max(x + kCard + 4, smallR);
  partyColB_ = y;
  // the selected card's name (a moment after a tap), with how to give orders
  if (partyTagT_ > 0 && !radial_.open) {
    for (const PartyCard& c : partyCards_) {
      if (c.member != partySel_) continue;
      const party::Member& m = g.party.members[(size_t)c.member];
      const float a = clampf(partyTagT_ / 0.3f, 0, 1);
      const std::string l1 = m.name, l2 = std::string(party::vocationName(m.voc)) + " - " + party::stateName(m.state);
      const std::string l3 = touchUI ? "HOLD FOR ORDERS" : "C: ORDERS";
      const float tw = (float)std::max(P.textW(l1, 1), std::max(P.textW(l2, 1), P.textW(l3, 1))) + 8;
      const float tx = c.x + c.w + 3, ty = c.y + 2;
      P.rect(tx, ty, tw, 31, Color(0.05f, 0.04f, 0.06f, 0.82f * a));
      P.frame(tx, ty, tw, 31, Color(0.55f, 0.45f, 0.28f, a));
      P.rect(tx - 2, ty + 12, 2, 5, Color(0.55f, 0.45f, 0.28f, a));   // a little tab toward the card
      P.textS(tx + 4, ty + 3, l1, 1, Color(kGoldP.r, kGoldP.g, kGoldP.b, a));
      P.text(tx + 4, ty + 12, l2, 1, Color(kTextP.r, kTextP.g, kTextP.b, a));
      P.text(tx + 4, ty + 21, l3, 1, Color(kDimP.r, kDimP.g, kDimP.b, a));
    }
  }
  // a refused order's reason beside its card
  if (partyDenyT_ > 0 && !partyDeny_.empty() && !radial_.open) {
    const int mi = memberOf(g, partyDenyId_);
    for (const PartyCard& c : partyCards_) {
      if (c.member != mi) continue;
      const float a = clampf(partyDenyT_ / 0.3f, 0, 1);
      const float tw = (float)P.textW(partyDeny_, 1) + 8, tx = c.x + c.w + 3, ty = c.y + (c.small ? 2.0f : 9.0f);
      P.rect(tx, ty, tw, 12, Color(0.12f, 0.03f, 0.03f, 0.85f * a));
      P.frame(tx, ty, tw, 12, Color(0.9f, 0.3f, 0.24f, a));
      P.text(tx + 4, ty + 3, partyDeny_, 1, Color(1.0f, 0.62f, 0.55f, a));
    }
  }
  // the alert ribbon (the oldest waiting one): the member's face and the line in the kind's colour, top centre
  PlateBox rb;
  float tw = 0;
  if (!radial_.open && partyRibbonBox(g, rb, tw)) {   // (r3: it steps away while the order ring is open)
    const PartyNote& n = alertShow_.front();
    const int mi = memberOf(g, n.id);
    // (fixer M8 r3) the words and the face fade first (a squared), the dark plate last: no half-faded glyphs ever
    // float over the street or a prompt without their plate under them
    const float a0 = clampf(std::min(n.t / 0.15f, (n.life - n.t) / 0.4f), 0, 1);
    const float a = a0 * a0, plateA = std::min(1.0f, a0 * 1.6f);
    const Color kc = alertColour(n.kind);
    const float ay = rb.y0, ax = rb.x0, w = rb.x1 - rb.x0, h = rb.y1 - rb.y0;
    P.rect(ax + 2, ay + 2, w, h, Color(0, 0, 0, 0.3f * plateA));
    P.rect(ax, ay, w, h, Color(0.05f, 0.04f, 0.06f, 0.86f * plateA));
    P.rect(ax + 3, ay, w - 6, 1, Color(kc.r, kc.g, kc.b, 0.85f * a));
    P.rect(ax + 3, ay + h - 1, w - 6, 1, Color(kc.r * 0.6f, kc.g * 0.6f, kc.b * 0.6f, 0.85f * a));
    P.rect(ax, ay + 3, 1, h - 6, Color(kc.r * 0.8f, kc.g * 0.8f, kc.b * 0.8f, 0.7f * a));
    P.rect(ax + w - 1, ay + 3, 1, h - 6, Color(kc.r * 0.5f, kc.g * 0.5f, kc.b * 0.5f, 0.7f * a));
    float tx = ax + 7;
    if (mi >= 0) {
      const party::Member& m = g.party.members[(size_t)mi];
      const Tex& pt = portraitTex(g, m, n.kind == 2 || n.kind == 4 ? 2 : n.kind == 1 ? 1 : n.kind == 3 || n.kind == 5 || n.kind == 8 ? 3 : 0);
      P.rect(ax + 3, ay + 2, 16, 16, Color(0.02f, 0.02f, 0.03f, a));
      P.rect(ax + 4, ay + 3, 14, 14, colU(mulc(vocColour(m.voc), 0.5f), a));
      P.blitEx(pt, kFaceX, kFaceY, kFace, kFace, ax + 4, ay + 3, (float)kFace, (float)kFace, false, Color(1, 1, 1, a));
      P.frame(ax + 3, ay + 2, 16, 16, Color(kc.r, kc.g, kc.b, 0.9f * a));
      tx = ax + 24;
    }
    P.textS(tx, ay + 7, n.s, 1, Color(kc.r, kc.g, kc.b, a));
  }
}

// ---------------------------------------------------------------- the radial
void View::radialOpen(Game& g, int mi, bool drag, uint64_t finger) {
  if (mi < 0 || mi >= (int)g.party.members.size()) return;
  radial_ = Radial();
  radial_.open = true; radial_.drag = drag; radial_.member = mi; radial_.finger = finger;
  partySel_ = mi;
  partyTagT_ = 0;
  // round the member's card, as far in from the safe area's edges as the ring needs
  float cy = (float)Pix::ST + 46 + kCard / 2;
  for (const PartyCard& c : partyCards_) if (c.member == mi) cy = c.y + c.h / 2;
  const float L = (float)Pix::SL, T = (float)Pix::ST, B = (float)(Pix::H - Pix::SB);
  // (fixer M8 r2, review: "the wheel opens over the portraits, HP bars and downed timers") right of the portrait
  // column, with room for the wedges' words on its left (touch); a held finger picks by the way it slides (phase 1)
  const float colR = partyColR_ > 0 ? partyColR_ : L + kCard + 8;
  radial_.c = Vec2(std::floor(colR + kRO + (touchUI ? kRLabel : 8.0f)), std::floor(clampf(cy, T + 44 + kRO, B - kRO - 6)));
  radial_.from = radial_.c;
  radial_.hot = drag ? -1 : openHot(g.party.members[(size_t)mi]);
  audio_->play(Sfx::MenuSelect, 1.15f, 0.8f);
}

int View::radialWedgeAt(Vec2 p) const {
  const Vec2 d = p - radial_.c;
  const float r = len(d);
  if (r < kRI - 2) return -1;
  if (r > kRO + (touchUI ? kRLabel + 6 : 16.0f)) return -2;   // (fixer M8 r2) a tap on a wedge's word counts
  float a = std::atan2(d.y, d.x) + 1.5707963f;   // 0 at the top, clockwise
  const float step = 6.2831853f / kWedges;
  a += step / 2;
  while (a < 0) a += 6.2831853f;
  while (a >= 6.2831853f) a -= 6.2831853f;
  return std::clamp((int)(a / step), 0, kWedges - 1);
}

void View::radialChoose(Game& g, int cmd) {
  const int mi = radial_.member;
  if (cmd < 0 || cmd >= kWedges || mi < 0 || mi >= (int)g.party.members.size()) return;
  std::string why = radialWhy(g, mi, cmd);
  const uint32_t id = g.party.members[(size_t)mi].id;
  if (why.empty() && !g.partyCommand(mi, (party::Cmd)cmd)) why = cmd == (int)party::Cmd::Attack ? "NOTHING TO ATTACK" : "THEY CANNOT DO THAT NOW";
  if (!why.empty()) {
    radial_.deny = why; radial_.denyT = 1.4f; radial_.denyWedge = cmd;
    partyDeny_ = why; partyDenyT_ = 2.2f; partyDenyId_ = id;
    audio_->play(Sfx::MenuBack, 0.8f, 0.9f);
    if (radial_.drag) radial_.open = false;   // a held radial closes on the release; the reason stays by the card
    return;
  }
  audio_->play(Sfx::MenuSelect, 1.0f, 0.9f);
  partyDeny_.clear(); partyDenyT_ = 0;
  radial_.open = false;
  const int now = memberOf(g, id);
  if (now >= 0) partySel_ = now;
}

bool View::partyKey(Game& g, int key) {
  if (g.mode != Mode::Play || g.party.members.empty()) return false;
  if (!radial_.open) {
    if (key != SDLK_C) return false;
    partySel_ = std::clamp(partySel_, 0, (int)g.party.members.size() - 1);
    // the selected one, else the first one travelling with the player
    if (g.party.members[(size_t)partySel_].state == party::State::Left)
      for (int i = 0; i < (int)g.party.members.size(); i++) if (g.party.members[(size_t)i].state == party::State::With) { partySel_ = i; break; }
    radialOpen(g, partySel_, false, 0);
    return true;
  }
  switch (key) {
    case SDLK_ESCAPE: radial_.open = false; audio_->play(Sfx::MenuBack); return true;
    case SDLK_C: case SDLK_TAB: {   // the next companion (not one who left)
      const int n = (int)g.party.members.size();
      for (int k = 1; k <= n; k++) {
        const int mi = (radial_.member + k) % n;
        if (g.party.members[(size_t)mi].state == party::State::Left) continue;
        if (mi == radial_.member) { radial_.open = false; return true; }
        const int hot = radial_.hot;
        radialOpen(g, mi, false, 0);
        radial_.hot = hot;
        return true;
      }
      radial_.open = false;
      return true;
    }
    case SDLK_LEFT: case SDLK_A: case SDLK_UP: case SDLK_W:
      radial_.hot = radial_.hot < 0 ? 0 : (radial_.hot + kWedges - 1) % kWedges; audio_->play(Sfx::MenuMove); return true;
    case SDLK_RIGHT: case SDLK_D: case SDLK_DOWN: case SDLK_S:
      radial_.hot = radial_.hot < 0 ? 0 : (radial_.hot + 1) % kWedges; audio_->play(Sfx::MenuMove); return true;
    case SDLK_RETURN: case SDLK_SPACE: case SDLK_E: case SDLK_J: case SDLK_KP_ENTER:
      if (radial_.hot >= 0) radialChoose(g, radial_.hot);
      return true;
    default:
      if (key >= SDLK_1 && key < SDLK_1 + kWedges) { radial_.hot = key - SDLK_1; radialChoose(g, radial_.hot); }
      return true;   // while it is open every key is the radial's
  }
}

bool View::partyPointer(Game& g, int phase, uint64_t id, Vec2 p, bool mouse) {
  if (g.mode != Mode::Play || g.party.members.empty()) return false;
  // (fixer M8 r1, review: "cards are under the 44 pt touch minimum") a finger lands on a card within a few pixels of
  // its frame (the column has open ground to its right and below); the nearest card's centre wins between two
  auto cardAt = [&](Vec2 q) -> const PartyCard* {
    const PartyCard* best = nullptr;
    float bd = 1e30f;
    for (const PartyCard& c : partyCards_) {
      const float m = c.small ? (touchUI ? 0.0f : 3.0f) : 5.0f;   // (r3: a touch bust's box is already its whole cell)
      const float mr = c.small && touchUI ? 0.0f : m + 3;
      if (q.x < c.x - m || q.y < c.y - m || q.x >= c.x + c.w + mr || q.y >= c.y + c.h + m) continue;
      const float d2 = len2(q - Vec2(c.x + c.w / 2, c.y + c.h / 2));
      if (d2 < bd) { bd = d2; best = &c; }
    }
    return best;
  };
  if (phase == 0) {
    if (radial_.open) {
      if (radial_.drag && id != radial_.finger) return false;   // another finger: the stick, the buttons
      const int w = radialWedgeAt(p);
      // (fixer M8 r2, review: "a tap gives the order at once, before a phone player can see what the icon means")
      // on touch the first tap on a wedge lights it and the caption says what it does; a tap on the lit one gives it
      if (w >= 0 && !mouse && touchUI && w != radial_.hot) { radial_.hot = w; radial_.denyT = 0; audio_->play(Sfx::MenuMove, 1.0f, 0.6f); return true; }
      if (w >= 0) { radial_.hot = w; radialChoose(g, w); return true; }
      if (w == -1) { radial_.open = false; audio_->play(Sfx::MenuBack); return true; }
      if (const PartyCard* c = cardAt(p)) { radialOpen(g, c->member, false, 0); return true; }   // another card: the radial moves to them
      radial_.open = false;
      return true;   // a tap outside only closes it
    }
    if (const PartyCard* c = cardAt(p)) {
      partyPress_ = PartyPress();
      partyPress_.on = true; partyPress_.id = id; partyPress_.member = c->member; partyPress_.start = partyPress_.cur = p; partyPress_.mouse = mouse;
      return true;
    }
    return false;
  }
  if (phase == 1) {
    if (radial_.open && radial_.drag && id == radial_.finger) {
      if (!radial_.armed && len2(p - radial_.from) > 9.0f * 9.0f) radial_.armed = true;
      if (radial_.armed) {
        // (fixer M8 r2) the ring stands beside the card, so the finger's slide from where it pressed picks the wedge
        // that way (a marking menu); back near the press point is no order
        int hot = -1;
        const Vec2 d = p - radial_.from;
        if (len2(d) > 9.0f * 9.0f) {
          const float step = 6.2831853f / kWedges;
          float an = std::atan2(d.y, d.x) + 1.5707963f + step / 2;
          while (an < 0) an += 6.2831853f;
          while (an >= 6.2831853f) an -= 6.2831853f;
          hot = std::clamp((int)(an / step), 0, kWedges - 1);
        }
        if (hot != radial_.hot && hot >= 0) audio_->play(Sfx::MenuMove, 1.0f, 0.6f);
        radial_.hot = hot;
      }
      return true;
    }
    if (radial_.open && !radial_.drag && mouse) { const int w = radialWedgeAt(p); if (w >= 0) radial_.hot = w; return false; }
    if (partyPress_.on && partyPress_.id == id) { partyPress_.cur = p; return true; }
    return false;
  }
  // phase 2: up
  if (radial_.open && radial_.drag && id == radial_.finger) {
    if (radial_.armed && radial_.hot >= 0) radialChoose(g, radial_.hot);
    else if (!radial_.armed) { radial_.drag = false; radial_.hot = openHot(g.party.members[(size_t)radial_.member]); }   // held still: it stays open for taps
    else radial_.open = false;   // let go over the hub or outside: nothing
    return true;
  }
  if (partyPress_.on && partyPress_.id == id) {
    partyPress_.on = false;
    const int mi = partyPress_.member;
    if (mi < 0 || mi >= (int)g.party.members.size()) return true;
    if (mi == partySel_ && partyTagT_ > 0) radialOpen(g, mi, false, 0);   // a tap on the selected card: the orders
    else { partySel_ = mi; partyTagT_ = 3.0f; audio_->play(Sfx::MenuMove); }
    return true;
  }
  return false;
}

void View::drawPartyRadial(Game& g) {
  if (!radial_.open || radial_.member < 0 || radial_.member >= (int)g.party.members.size()) return;
  Pix& P = *pix_;
  const party::Member& m = g.party.members[(size_t)radial_.member];
  const float k = clampf(radial_.t / 0.14f, 0, 1), ease = 1 - (1 - k) * (1 - k);
  // (fixer M8 r3) a firmer veil: the world's name tags and prompts sink under the ring and its words
  P.rect(0, 0, (float)Pix::W, (float)Pix::H, Color(0.02f, 0.01f, 0.03f, 0.48f * ease));
  const Vec2 c = radial_.c;
  const float step = 6.2831853f / kWedges;
  float skCd = 0;
  bool skLocked = true;
  const int sk = bestSkill(g, radial_.member, skCd, skLocked);
  for (int w = 0; w < kWedges; w++) {
    const bool dis = !radialWhy(g, radial_.member, w).empty();
    const int st = w == radial_.hot ? 1 : dis ? 2 : 0;
    const uint64_t key = ptag(0x41, (uint64_t)(w * 4 + st));
    auto it = partyTex_.find(key);
    if (it == partyTex_.end()) it = partyTex_.emplace(key, pix_->bake(paintWedge(w, st))).first;
    const float a = -1.5707963f + w * step;
    const Vec2 dir(std::cos(a), std::sin(a));
    const float slide = (1 - ease) * -10.0f;   // the wedges slide out from the hub as it opens
    const float wx = std::floor(c.x - kRC + dir.x * slide + 0.5f), wy = std::floor(c.y - kRC + dir.y * slide + 0.5f);
    P.blitEx(it->second, 0, 0, kRN, kRN, wx, wy, (float)kRN, (float)kRN, false, Color(1, 1, 1, ease));
    if (radial_.denyWedge == w && radial_.denyT > 0 && ((int)(radial_.denyT * 12) & 1))
      P.blitEx(it->second, 0, 0, kRN, kRN, wx, wy, (float)kRN, (float)kRN, false, Color(1.0f, 0.2f, 0.15f, 0.55f), 1);
    // the icon at the wedge's middle (whole pixels), dimmed when the order cannot be given
    const uint64_t ik = ptag(0x42, (uint64_t)w);
    auto ii = partyTex_.find(ik);
    if (ii == partyTex_.end()) ii = partyTex_.emplace(ik, pix_->bake(paintGlyph(kIcons[w], 11, kIconPal[w]))).first;
    const float mr = (kRO + kRI) / 2 + 1;
    const float ix = std::floor(c.x + dir.x * (mr + slide) - 5.5f + 0.5f), iy = std::floor(c.y + dir.y * (mr + slide) - 5.5f + 0.5f);
    P.blitEx(ii->second, 0, 0, 11, 11, ix + 1, iy + 1, 11, 11, false, Color(0, 0, 0, 0.45f * ease));
    P.blitEx(ii->second, 0, 0, 11, 11, ix, iy, 11, 11, false, dis ? Color(0.5f, 0.48f, 0.5f, 0.75f * ease) : Color(1, 1, 1, ease));
    if (w == (int)party::Cmd::Skill && !skLocked && skCd > 0) {   // the cooldown's seconds over the star
      const std::string s = std::to_string((int)std::ceil(skCd));
      P.textS(ix + 6, iy + 9, s, 1, Color(kTextP.r, kTextP.g, kTextP.b, ease), 1);
    }
    // the stance they hold now: a gold pip at the wedge's outer rim
    const bool current = (m.state == party::State::With && w == (int)m.stance) || (m.state == party::State::Waiting && w == (int)party::Cmd::Wait) ||
                         (m.state == party::State::Home && w == (int)party::Cmd::Home);
    if (current) {
      const float px = std::floor(c.x + dir.x * (kRO - 5 + slide)), py = std::floor(c.y + dir.y * (kRO - 5 + slide));
      P.rect(px - 1, py - 1, 3, 3, Color(0.1f, 0.07f, 0.04f, ease));
      P.rect(px, py, 1, 1, Color(1, 0.92f, 0.6f, ease));
    }
    if (!touchUI) {   // the keyboard's digit by the rim
      const float px = c.x + dir.x * (kRO + 7), py = c.y + dir.y * (kRO + 7);
      P.textS(std::floor(px) + 1, std::floor(py) - 3, std::to_string(w + 1), 1, Color(kDimP.r, kDimP.g, kDimP.b, ease), 1);
    } else {   // (fixer M8 r2) the wedge's word just outside the rim, pushed out along its side
      const char* word = kRWord[w];
      const float half = (float)P.textW(word, 1) / 2.0f;
      const float px = c.x + dir.x * (kRO + 5) + dir.x * (half + 1), py = c.y + dir.y * (kRO + 6) - 3;
      const Color wc = w == radial_.hot ? kGoldP : dis ? Color(0.55f, 0.52f, 0.55f) : kTextP;
      P.rect(std::floor(px - half - 2), std::floor(py - 1), std::floor(half * 2 + 4), 9, Color(0.04f, 0.03f, 0.05f, 0.7f * ease));
      P.textS(std::floor(px), std::floor(py), word, 1, Color(wc.r, wc.g, wc.b, ease), 1);
    }
  }
  // the hub with the member's face
  {
    const uint32_t vc = vocColour(m.voc);
    const uint64_t hk = ptag(0x43, vc);
    auto it = partyTex_.find(hk);
    if (it == partyTex_.end()) it = partyTex_.emplace(hk, pix_->bake(paintHub(vc))).first;
    const Tex& ht = it->second;
    P.blitEx(ht, 0, 0, ht.w, ht.h, std::floor(c.x - ht.w / 2.0f), std::floor(c.y - ht.h / 2.0f), (float)ht.w, (float)ht.h, false, Color(1, 1, 1, ease));
    const Tex& pt = portraitTex(g, m, m.state == party::State::Downed ? 2 : 0);
    P.blitEx(pt, 3, 1, 18, 18, std::floor(c.x - 9), std::floor(c.y - 10), 18, 18, false, Color(1, 1, 1, ease));
  }
  // the caption beside the ring: who, the chosen order, and what it means now
  {
    std::string l1 = m.name, l2, l3;
    Color c3 = kDimP;
    if (radial_.denyT > 0 && !radial_.deny.empty()) { l2 = party::cmdName((party::Cmd)radial_.denyWedge); l3 = radial_.deny; c3 = Color(1.0f, 0.55f, 0.48f); }
    else if (radial_.hot >= 0) {
      l2 = party::cmdName((party::Cmd)radial_.hot);
      const std::string why = radialWhy(g, radial_.member, radial_.hot);
      if (radial_.hot == (int)party::Cmd::Skill && sk >= 0) {
        const party::SkillInfo& si = party::skillInfo(party::skillsOf(m.voc)[(size_t)sk]);
        l2 = si.name;
        l3 = why.empty() ? si.what : why;
        if (!why.empty()) c3 = Color(0.95f, 0.7f, 0.5f);
      } else if (!why.empty()) { l3 = why; c3 = Color(0.95f, 0.7f, 0.5f); }
    } else {
      l2 = touchUI ? (radial_.drag ? "SLIDE TO AN ORDER" : "TAP AN ORDER") : "1-7 OR ARROWS";
      // (r3) one not travelling with the player: where they are now, under the prompt
      if (m.state == party::State::Waiting) l3 = "WAITING FOR YOU";
      else if (m.state == party::State::Home) l3 = "AT HOME";
      else if (m.state == party::State::Downed) l3 = "DOWN AND BLEEDING";
    }
    const int per = 24;
    std::vector<std::string> l3s = l3.empty() ? std::vector<std::string>() : wrapP(l3, per);
    if (l3s.size() > 3) l3s.resize(3);
    int tw = std::max(P.textW(l1, 1), P.textW(l2, 1));
    for (const std::string& s : l3s) tw = std::max(tw, P.textW(s, 1));
    const float bw = (float)tw + 12, bh = 26.0f + 9.0f * l3s.size();
    float bx = c.x + kRO + (touchUI ? kRLabel + 4 : 14), by = std::floor(c.y - bh / 2);
    if (bx + bw > (float)(Pix::W - Pix::SR) - 4) { bx = std::floor(c.x - bw / 2); by = c.y + kRO + 10; }
    {   // (fixer M8 r2, review: "the tooltip overlaps the JOINS YOU ribbon") below the ribbon when they would meet
      PlateBox rb;
      float rtw = 0;
      if (partyRibbonBox(g, rb, rtw) && bx < rb.x1 + 2 && bx + bw > rb.x0 - 2 && by < rb.y1 + 2 && by + bh > rb.y0 - 2) by = rb.y1 + 4;
    }
    P.rect(bx + 2, by + 2, bw, bh, Color(0, 0, 0, 0.3f * ease));
    P.rect(bx, by, bw, bh, Color(0.05f, 0.04f, 0.06f, 0.9f * ease));
    P.frame(bx, by, bw, bh, Color(0.62f, 0.50f, 0.30f, ease));
    P.frame(bx + 1, by + 1, bw - 2, bh - 2, Color(0.20f, 0.15f, 0.10f, ease));
    P.textS(bx + 6, by + 5, l1, 1, Color(kGoldP.r, kGoldP.g, kGoldP.b, ease));
    P.text(bx + 6, by + 15, l2, 1, Color(kTextP.r, kTextP.g, kTextP.b, ease));
    for (size_t i = 0; i < l3s.size(); i++) P.text(bx + 6, by + 25 + 9.0f * i, l3s[i], 1, Color(c3.r, c3.g, c3.b, ease));
  }
}

// ---------------------------------------------------------------- plate layout (fixer M8 r1)
void View::clearPlate(float& x0, float& y0, float w, float h, const std::vector<PlateBox>& avoid, float L, float T, float R, float B) {
  auto hits = [&](float x, float y) {
    for (const PlateBox& b : avoid)
      if (x < b.x1 + 2 && x + w > b.x0 - 2 && y < b.y1 + 2 && y + h > b.y0 - 2) return true;
    return false;
  };
  if (!hits(x0, y0)) return;
  // candidates: where it is, and beside every box (above, below, left, right); the cheapest free one wins (lifting
  // costs least: a line rises over its speaker; sliding sideways or dropping cost more)
  std::vector<float> xs{x0}, ys{y0};
  for (const PlateBox& b : avoid) {
    ys.push_back(b.y0 - h - 3); ys.push_back(b.y1 + 3);
    xs.push_back(b.x0 - w - 3); xs.push_back(b.x1 + 3);
  }
  float bx = x0, by = y0, best = 1e9f;
  for (float x : xs)
    for (float y : ys) {
      if (x < L + 2 || x + w > R - 2 || y < T + 2 || y + h > B - 2 || hits(x, y)) continue;
      const float dy = y - y0, cost = std::fabs(x - x0) * 1.3f + (dy < 0 ? -dy : dy * 1.6f);
      if (cost < best) { best = cost; bx = x; by = y; }
    }
  x0 = bx; y0 = by;
}

bool View::partyRibbonBox(const Game& g, PlateBox& b, float& textW) {
  if (alertShow_.empty() || g.mode != Mode::Play) return false;
  Pix& P = *pix_;
  const float T = (float)Pix::ST;
  const PartyNote& n = alertShow_.front();
  const int mi = memberOf(g, n.id);
  float ay = std::max(T + 30, bossBarBottom_ + 8);
  if (herald_.t > 0 || (biomeBannerT_ > 0 && !biomeBanner_.empty())) ay += 24;
  if (biomeBannerT_ > 0 && !biomeBanner_.empty())   // (M9 fixer) the banner may have dropped under the quick bar
    ay = std::max(ay, biomeBannerY(std::min((float)P.textW(biomeBanner_, 1) / 2 + 26, Pix::W / 2.0f - 70)) + 14);
  if (bannerT_ > 0) ay = std::max(ay, T + (bannerState_.empty() ? 88.0f : 99.0f));   // under the arrival banner
  textW = (float)P.textW(n.s, 1);
  const float w = textW + (mi >= 0 ? 30.0f : 14.0f), h = 20;
  b.x0 = std::floor(Pix::W / 2.0f - w / 2); b.y0 = ay; b.x1 = b.x0 + w; b.y1 = ay + h;
  // (fixer M8 r3, review: "E: TALK is drawn over the half-faded JOINS YOU ribbon: ghost glyphs") never under the
  // faced person's name tag: the ribbon steps above it (else below)
  if (tagOn_ && b.x0 < tagX1_ + 2 && b.x1 > tagX0_ - 2 && b.y0 < tagY1_ + 2 && b.y1 > tagY0_ - 2) {
    const float up = tagY0_ - 3 - h;
    const float floorY = bannerT_ > 0 ? T + (bannerState_.empty() ? 88.0f : 99.0f) : T + 30;   // (never into the banner)
    if (up >= floorY) { b.y0 = up; b.y1 = up + h; }
    else { b.y0 = tagY1_ + 3; b.y1 = b.y0 + h; }
  }
  return true;
}

void View::hudBands(const Game& g, std::vector<PlateBox>& out) {
  Pix& P = *pix_;
  const float L = (float)Pix::SL, T = (float)Pix::ST, R = (float)(Pix::W - Pix::SR);
  auto add = [&](float x0, float y0, float x1, float y1) { PlateBox b; b.x0 = x0; b.y0 = y0; b.x1 = x1; b.y1 = y1; out.push_back(b); };
  if (bannerT_ > 0 && g.mode != Mode::Dialogue) add(0, T + 49, (float)Pix::W, T + (bannerState_.empty() ? 85.0f : 96.0f));
  if (herald_.t > 0 && g.mode == Mode::Play) {   // (as drawHerald lays it out)
    const int tw = std::max(P.textW(herald_.title, 1), P.textW(herald_.top, 1));
    const float side = !herald_.wild ? 22.0f : 10.0f, w = tw + side * 2 + 12;
    float x = std::floor(Pix::W / 2.0f - w / 2);
    const float minX = L + 136, maxX = R - 76 - w;
    if (maxX >= minX) x = std::clamp(x, minX, maxX);
    const float y = std::max(T + 5, bossBarBottom_ + 3);
    add(x - 4, y, x + w + 4, y + 27);
  }
  if (biomeBannerT_ > 0 && !biomeBanner_.empty() && bannerT_ <= 0 && herald_.t <= 0 && g.mode == Mode::Play && !g.inside) {
    const float half = std::min((float)P.textW(biomeBanner_, 1) / 2 + 26, Pix::W / 2.0f - 70), y = biomeBannerY(half);
    add(Pix::W / 2.0f - half, y - 4, Pix::W / 2.0f + half, y + 11);
  }
  PlateBox rb;
  float tw = 0;
  if (partyRibbonBox(g, rb, tw)) out.push_back(rb);
  if (radial_.open) {   // the order ring and its caption to its right
    const Vec2 c = radial_.c;
    const float lw = touchUI ? kRLabel : 6.0f;
    add(c.x - kRO - lw, c.y - kRO - 12, c.x + kRO + lw, c.y + kRO + 12);
    add(c.x + kRO + lw + 4, c.y - 34, c.x + kRO + lw + 176, c.y + 34);
  }
  if (tagOn_) add(tagX0_ - 2, tagY0_ - 2, tagX1_ + 2, tagY1_ + 2);   // the faced person's name and E: TALK
  if (g.mode == Mode::Play) {
    const Vec2 cam(std::floor(cam_.x), std::floor(cam_.y));
    // (fixer M8 r2, review: "plates cover the mounted hero's head and the companion's horse") the hero, on foot or
    // in the saddle: no spoken line is laid over them
    const Vec2 hs = g.pl().p - cam;
    if (g.homeRiding()) add(hs.x - 15, hs.y - 40, hs.x + 15, hs.y + 3);
    else add(hs.x - 8, hs.y - 30, hs.x + 8, hs.y + 2);
    // (fixer M8 r2, review: "the binding ring is hidden under the name label and the I'M DOWN plate") the wound mark
    // and the binding ring over a downed companion (companionOverlays) stay in the clear
    for (size_t i = 1; i < g.actors.size(); i++) {
      const Actor& a = g.actors[i];
      const int mi = companionIndex(g, a);
      if (mi < 0 || a.st == AState::Dead || g.party.members[(size_t)mi].state != party::State::Downed) continue;
      const Vec2 s = a.p - cam;
      const float cx = std::floor(s.x), cy = std::floor(s.y - (a.human ? 28.0f : 22.0f));
      add(cx - 11, cy - 11, cx + 11, cy + 11);
    }
  }
}

// ---------------------------------------------------------------- banter
void View::drawBanter(Game& g, Vec2 cam) {
  if (banterShow_.empty() || g.mode != Mode::Play) return;
  Pix& P = *pix_;
  const float L = (float)Pix::SL, T = (float)Pix::ST, R = (float)(Pix::W - Pix::SR), B = (float)(Pix::H - Pix::SB);
  // (fixer M8 r1) clear of the banner, the herald, the ribbon and every plate already drawn (the townsfolk's lines,
  // the other companions'): the nearest free place, over the speaker first
  std::vector<PlateBox> avoid = plateBoxes_;
  hudBands(g, avoid);
  // (fixer M8 r3) the arrival card and the herald first: the lines wait for them (partyUpdate holds their clocks)
  if ((bannerT_ > 0 && !bannerSub_.empty()) || herald_.t > 0) return;
  // one spoken plate at a time on a phone (two on a wide screen): the rest wait their turn, the oldest first
  const int cap = touchUI ? 1 : 2;
  int onShow = 0;
  for (PartyNote& n : banterShow_) {
    if (onShow >= cap) break;
    const int mi = memberOf(g, n.id);
    if (mi < 0) continue;
    // (fixer M8 r2) a downed one whose wounds are being bound has been heard: their cry steps aside for the ring
    if (g.party.members[(size_t)mi].state == party::State::Downed && g.party.members[(size_t)mi].reviveT > 0) continue;
    const int ai = mi < (int)partyActor_.size() ? partyActor_[(size_t)mi] : -1;
    const float a = clampf(n.t / 0.12f, 0, 1) * clampf((n.life - n.t) / 0.45f, 0, 1);
    const std::vector<std::string> ls = wrapP(n.s, 24);
    int w = 0;
    for (const std::string& l : ls) w = std::max(w, P.textW(l, 1));
    // (fixer M8 r2, review: "two plates float stacked with no tail or name: who said which line?") with more than
    // one companion, or the speaker out of sight, the plate is headed with the speaker's name in their party gold
    const std::string& who = g.party.members[(size_t)mi].name;
    bool named = g.party.members.size() > 1 || ai <= 0;
    if (named) w = std::max(w, P.textW(who, 1));
    const float lh = 9.0f;
    float h = (ls.size() + (named ? 1 : 0)) * lh + 4.0f;
    float cx = 0, y0 = 0;
    bool overHead = false, tail = true;
    Vec2 head;
    if (ai > 0) {
      const Actor& sp = g.actors[(size_t)ai];
      head = sp.p - cam;
      const float top = sp.human ? 30.0f : 20.0f;
      if (head.x > L + 4 && head.x < R - 4 && head.y - top > T + 4 && head.y < B - 4) {
        overHead = true;
        cx = std::floor(head.x);
        y0 = std::floor(head.y - top - h - 4);
      }
    }
    if (!overHead) {
      // out of sight: by their card
      tail = false;
      bool found = false;
      for (const PartyCard& c : partyCards_)
        if (c.member == mi) { cx = c.x + c.w + 6 + w / 2.0f; y0 = c.y + 4; found = true; }
      if (!found) { cx = Pix::W / 2.0f; y0 = B - 90; }
    }
    cx = clampf(cx, L + w / 2.0f + 6, R - w / 2.0f - 6);
    if (y0 < T + 4) y0 = T + 4;
    // off the party column, the location / quest column, the name tag, the touch cluster
    if (partyColR_ > 0 && cx - w / 2.0f - 4 < partyColR_ && y0 < partyColB_) cx = partyColR_ + w / 2.0f + 6;
    if (y0 < hudColBottom_ && cx + w / 2.0f + 3 > hudColLeft_) {
      const float left = hudColLeft_ - w / 2.0f - 5;
      if (left >= L + w / 2.0f + 4) cx = left;
      else y0 = hudColBottom_ + 2;
    }
    if (tagOn_ && cx + w / 2.0f + 3 > tagX0_ && cx - w / 2.0f - 3 < tagX1_ && y0 - 1 < tagY1_ && y0 + h + 1 > tagY0_) y0 = std::floor(tagY0_ - 3 - h);
    if (touchUI) {   // the bottom-right buttons (the same berth the speech plates keep, hud.cpp clearOfTouch)
      const float bx0 = R - 128, by0 = B - 116;
      if (cx + w / 2.0f + 3 > bx0 && y0 + h > by0) { if (cx - bx0 < y0 + h - by0) cx = bx0 - w / 2.0f - 4; else y0 = by0 - h - 2; }
      const float sx1 = L + 124, sy0 = B - 84;   // the stick
      if (cx - w / 2.0f < sx1 && y0 + h > sy0) y0 = sy0 - h - 2;
    }
    if (y0 < T + 4) y0 = T + 4;
    float pw = (float)w + 8;
    float x0 = std::floor(cx) - w / 2.0f - 4;
    const float wantX = x0, wantY = y0;
    clearPlate(x0, y0, pw, h, avoid, L, T, R, B);
    // (fixer M8 r3) a speaker in sight is never answered from across the screen: no free place near them, the line
    // waits (its clock held) rather than pin itself over the title card or the far side of the street
    if (overHead && (std::fabs(y0 - wantY) > 48 || std::fabs(x0 - wantX) > 64)) continue;
    if (!named && (std::fabs(y0 - wantY) > 26 || std::fabs(x0 - wantX) > 36)) {
      // pushed well away from the speaker (the hero, a banner, another plate in the way): the plate names them
      named = true;
      w = std::max(w, P.textW(who, 1));
      h += lh;
      pw = (float)w + 8;
      x0 = wantX; y0 = wantY - lh;
      clearPlate(x0, y0, pw, h, avoid, L, T, R, B);
    }
    x0 = std::floor(x0); y0 = std::floor(y0);
    cx = x0 + w / 2.0f + 4;
    PlateBox pb; pb.x0 = x0; pb.y0 = y0; pb.x1 = x0 + pw; pb.y1 = y0 + h;
    avoid.push_back(pb);
    plateBoxes_.push_back(pb);
    n.shown = true;
    onShow++;
    // the plate: dark, a warm rim (a companion's voice, unlike a passer-by's), a tail down to the speaker
    P.rect(x0 + 1, y0 + 1, pw, h, Color(0, 0, 0, 0.35f * a));
    P.rect(x0, y0, pw, h, Color(0.06f, 0.05f, 0.07f, 0.84f * a));
    P.rect(x0 + 2, y0, pw - 4, 1, Color(kGoldP.r, kGoldP.g, kGoldP.b, 0.75f * a));
    P.rect(x0 + 2, y0 + h - 1, pw - 4, 1, Color(0.45f, 0.35f, 0.2f, 0.8f * a));
    P.rect(x0, y0 + 2, 1, h - 4, Color(0.62f, 0.5f, 0.3f, 0.7f * a));
    P.rect(x0 + pw - 1, y0 + 2, 1, h - 4, Color(0.4f, 0.3f, 0.18f, 0.7f * a));
    if (tail && overHead && std::fabs(head.x - cx) < w / 2.0f + 2 && head.y > y0 + h) {
      const float tx = clampf(std::floor(head.x), x0 + 4, x0 + pw - 6);
      P.rect(tx - 1, y0 + h, 3, 1, Color(0.06f, 0.05f, 0.07f, 0.84f * a));
      P.rect(tx, y0 + h + 1, 1, 2, Color(0.45f, 0.35f, 0.2f, 0.8f * a));
    }
    float ty = y0 + 3;
    if (named) { P.textS(cx, ty, who, 1, Color(kGoldP.r, kGoldP.g, kGoldP.b, a), 1); ty += lh; }
    for (size_t k = 0; k < ls.size(); k++) P.textS(cx, ty + k * lh, ls[k], 1, Color(0.97f, 0.92f, 0.80f, a), 1);
  }
}

// (Witness, View::witnessRelease) the members' remembered looks back to none (member ids repeat from run to run)
void witnessResetPartyHud() { g_lookMemo.clear(); }
