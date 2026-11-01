// HUD, menus, toasts, touch controls, minimap (input: input.cpp; dialogue/shop/title/death: *_ui.cpp; layout: hud_layout.h).
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <string>
#include <vector>
#include "rpg/story/story.h"
#include "rpg/culture/culture.h"
#include "rpg/sim/craft.h"
#include "rpg/sim/gear.h"
#include "rpg/sim/home.h"
#include "rpg/sim/magic.h"
#include "rpg/sim/sea_ops.h"
#include "rpg/sim/skill_ops.h"
#include "rpg/sim/skills.h"
#include "rpg/view/hud_layout.h"
#include "rpg/view/realm_ui.h"
#include "rpg/view/skills_ui.h"
#include "rpg/view/view.h"
#include "rpg/world/poi.h"
#include "rpg/world/source.h"

void heroStage(Pix& P, const Tex& light, float cx, float footY, float size);   // creator.cpp
bool seaHudRect(View& v, float& x0, float& y0, float& x1, float& y1);   // (M10) render_sea.cpp: the sail panel's place

using namespace hudlay;

namespace {
Color col(uint32_t c, float a = 1) { return Color((c & 255) / 255.0f, ((c >> 8) & 255) / 255.0f, ((c >> 16) & 255) / 255.0f, a); }
// a kingdom's banner colour lifted toward parchment so a dark field still reads as text over the world
// (M2 fixer) the kingdom's colour lifted well toward a light parchment, so it reads over grass, roofs and snow
Color mixKing(Color c) { return Color(c.r * 0.40f + 0.58f, c.g * 0.40f + 0.55f, c.b * 0.40f + 0.48f, c.a); }
// (M7 fixer r2, review: "a villager's bark was drawn over the touch combat buttons") a label's plate (centre cx, top y0,
// w x h) moved clear of the bottom-right touch cluster (attack, bow, spell, roll, potion with a little margin): slid
// left of it or lifted above it, whichever moves it less
void clearOfTouch(float& cx, float& y0, float w, float h) {
  float x0 = 1e9f, yt = 1e9f;
  for (int b = 0; b < B_COUNT; b++) {
    if (b == B_MENU) continue;
    const BtnDef d = btnPos(b);
    x0 = std::min(x0, d.x - d.r - 4);
    yt = std::min(yt, d.y - d.r - 4);
  }
  for (int q = 0; q < 4; q++) {   // (M9) and the quick-cast arc round the attack button
    const QuickBtn d = quickBtnPos(q);
    x0 = std::min(x0, d.x - d.r - 4);
    yt = std::min(yt, d.y - d.r - 4);
  }
  if (cx + w / 2 + 3 <= x0 || y0 + h + 1 <= yt) return;
  const float left = x0 - w / 2 - 4, up = yt - h - 2;
  if (cx - left <= y0 - up) cx = std::floor(left);
  else y0 = std::floor(up);
}
// the resting place of the movement stick (bottom-left of the safe area)
Vec2 stickRest() { return Vec2((float)Pix::SL + 60, (float)(Pix::H - Pix::SB) - 55); }

const char* kTabs[] = {"ITEMS", "QUESTS", "MAP", "HERO", "SYSTEM", "EQUIP", "PARTY", "SKILLS", "SPELLS"};
float g_quickBarR = 0;   // (M9) the desktop quick bar's right end this frame (the NPC labels keep clear of the HUD block)
// a lead's source word and its colour (the journal's palette: amber, sage, rose, parchment, lilac)
Color leadColour(jobs::LeadFrom f) {
  switch (f) {
    case jobs::LeadFrom::Rumour: return Color(0.96f, 0.72f, 0.38f);
    case jobs::LeadFrom::Gossip: return Color(0.62f, 0.84f, 0.66f);
    case jobs::LeadFrom::Incident: return Color(0.95f, 0.56f, 0.48f);
    case jobs::LeadFrom::Board: return Color(0.90f, 0.84f, 0.66f);
    default: return Color(0.80f, 0.72f, 1.0f);
  }
}
// where a lead points from the hero ("NE" / "NORTH-EAST"; "" without a place); indoors, from the door
std::string leadDir(const Game& g, const jobs::Lead& l, bool longForm) {
  if (!l.hasPos) return "";
  int px = (int)std::floor(g.pl().p.x / TILE), py = (int)std::floor(g.pl().p.y / TILE);
  if (g.inside && g.subBldg >= 0 && g.subBldg < (int)g.world.over.bldgs.size()) { px = g.world.over.bldgs[(size_t)g.subBldg].doorX(); py = g.world.over.bldgs[(size_t)g.subBldg].doorY(); }
  else if (g.inside && g.subSite >= 0 && g.subSite < (int)g.world.sites.size()) { px = g.world.sites[(size_t)g.subSite].ex; py = g.world.sites[(size_t)g.subSite].ey; }
  const int dx = l.gx - (px + g.world.ox), dy = l.gy - (py + g.world.oy);
  if (std::abs(dx) + std::abs(dy) < 24) return "HERE";
  static const char* lf[8] = {"EAST", "NORTH-EAST", "NORTH", "NORTH-WEST", "WEST", "SOUTH-WEST", "SOUTH", "SOUTH-EAST"};
  static const char* sf[8] = {"E", "NE", "N", "NW", "W", "SW", "S", "SE"};
  const float a = std::atan2((float)-dy, (float)dx);
  const int k = ((int)std::lround(a / (3.14159265f / 4)) % 8 + 8) % 8;
  return longForm ? lf[k] : sf[k];
}
// how far, in walking words ("HALF A DAY'S WALK")
std::string leadWalk(const Game& g, const jobs::Lead& l) {
  if (!l.hasPos) return "";
  int px = (int)std::floor(g.pl().p.x / TILE), py = (int)std::floor(g.pl().p.y / TILE);
  if (g.inside && g.subBldg >= 0 && g.subBldg < (int)g.world.over.bldgs.size()) { px = g.world.over.bldgs[(size_t)g.subBldg].doorX(); py = g.world.over.bldgs[(size_t)g.subBldg].doorY(); }
  const float d = std::hypot((float)(l.gx - (px + g.world.ox)), (float)(l.gy - (py + g.world.oy)));
  const float h = d / 30.0f;
  return h < 0.75f ? "CLOSE BY" : h < 1.5f ? "AN HOUR'S WALK" : h < 4.5f ? "A FEW HOURS' WALK" : h < 9.0f ? "HALF A DAY'S WALK" : h < 18.0f ? "A DAY'S WALK" : "DAYS AWAY";
}

std::string trackedStep(const Game& g, const Quest& q) {
  std::string s = g.questStatus(q);
  if (s.empty() && q.state == QState::Complete) s = "RETURN TO " + q.giverName;
  // (P1b ENGINE lane) a story stage's clock ("6 DAYS LEFT", "ONE DAY LEFT", "LAST DAY") is the line that must never be
  // cut: it is read from the engine (the stage's deadline) and takes the second line of its own; the step above it, its
  // copy of the clock dropped (the status line's fit may have cut it anywhere), is trimmed at a word to the column
  if (q.type == QType::Story) {
    std::string clock;
    for (const story::Instance& in : g.story.running())
      if (!in.done && in.questId == q.id) {
        const int d = g.story.stageDeadline(in);
        if (d > 0) {
          const int left = std::max(0, in.stageDay + d - g.day);
          clock = left > 1 ? std::to_string(left) + " DAYS LEFT" : left == 1 ? "ONE DAY LEFT" : "LAST DAY";
        }
      }
    if (!clock.empty()) {
      std::string step = s;
      for (size_t cp = step.find(" ("); cp != std::string::npos; cp = step.find(" (", cp + 1)) {
        const std::string rest = step.substr(cp + 2);
        if (!rest.empty() && (std::isdigit((unsigned char)rest[0]) || rest.rfind("ONE D", 0) == 0 || rest.rfind("LAST D", 0) == 0 || rest.rfind("ONE", 0) == 0 ||
                              rest.rfind("LAST", 0) == 0)) {
          // keep a way word after it ("(EAST)") when the fit kept one
          const size_t close = step.find(')', cp);
          std::string way;
          if (close != std::string::npos) { const size_t w = step.find(" (", close); if (w != std::string::npos) way = step.substr(w); }
          step = step.substr(0, cp) + way;
          break;
        }
      }
      if (step.size() > 30) {
        step = step.substr(0, 30);
        const size_t sp = step.find_last_of(' ');
        if (sp != std::string::npos && sp > 14) step = step.substr(0, sp);
        if (!step.empty() && step.back() == '(') step.pop_back();
        while (!step.empty() && step.back() == ' ') step.pop_back();
      }
      return step + "\n" + clock;
    }
  }
  if (s.size() <= 30) return s;
  // (P1) a hunt keeps its count and its way on the first line, the ground on the second ("THIN THE WOLVES 2/5 (NE)"
  // over "BY THE GREY TEETH")
  if (q.type == QType::Hunt && q.state == QState::Active && s.rfind("THIN THE ", 0) == 0) {
    const size_t by = s.find(" BY "), at = by != std::string::npos ? by : s.find(" AT THEIR DEN") != std::string::npos ? s.find(" AT THEIR DEN") : s.find(" IN THE WILDS");
    const std::string prog = std::to_string(q.have) + "/" + std::to_string(q.need);
    const size_t pp = s.rfind(" " + prog);
    if (at != std::string::npos && pp != std::string::npos && pp > at) {
      std::string first = s.substr(0, at) + s.substr(pp), second = s.substr(at + 1, pp - at - 1);
      if (first.size() > 30) first = s.substr(0, at) + " " + prog;
      if (second.size() > 30) second = second.substr(0, 30);
      return first + "\n" + second;
    }
  }
  // drop a trailing clause (" IN <TOWN>", " (NORTH-EAST)", " NEAR <TOWN>") rather than cutting a word in half
  size_t cut = std::string::npos;
  for (const char* sep : {" IN ", " NEAR ", " AT ", " (", ": "}) {
    size_t p = s.find(sep);
    while (p != std::string::npos) {
      if (p >= 8 && p <= 30 && (cut == std::string::npos || p > cut)) cut = p;
      p = s.find(sep, p + 1);
    }
  }
  if (cut != std::string::npos) {
    // (M1) a place clause ("IN BRIGHTFELL") is where to go: it moves to a second line instead of being dropped
    std::string rest = s.substr(cut + 1);
    if (rest.size() > 30) rest = rest.substr(0, 30);
    if (rest.rfind("IN ", 0) == 0 || rest.rfind("NEAR ", 0) == 0 || rest.rfind("AT ", 0) == 0) return s.substr(0, cut) + "\n" + rest;
    return s.substr(0, cut);
  }
  s = s.substr(0, 30);
  size_t sp = s.find_last_of(' ');
  return sp != std::string::npos && sp > 18 ? s.substr(0, sp) : s;
}
// (P1 fixer) a journal LEAD row: the need first, so a row cut to the column still says something. The source word is
// the bullet's colour and the place is the row's direction, so they go: "SEEN: " / "OVERHEARD: " off the front, the
// "... OF DERGZALT" / "... IN BEDORP" filler out of the middle (the town goes last, in brackets), a leading "THE " off.
// "DAKA OF DERGZALT WANTS SPIDERS CULLED" -> "DAKA WANTS SPIDERS CULLED (DERGZALT)"; a row still too long ends "...".
std::string leadRow(const Game& g, const jobs::Lead& l, float w) {
  std::string t = l.text;
  // (P1 fixer r2) a teller's lead is "NAME OF TOWN: THE TALE'S TITLE - "the words overheard"": the row is the name and
  // the title (the words are on the lead's page)
  if (l.from == jobs::LeadFrom::Teller) {
    const size_t q = t.find(" - \"");
    if (q != std::string::npos) t.resize(q);
  }
  for (const char* pre : {"SEEN: ", "OVERHEARD: ", "RUMOUR: "})
    if (t.rfind(pre, 0) == 0) t = t.substr(std::strlen(pre));
  std::string town;
  const int si = l.site ? g.world.siteHandle(l.site) : -1;
  if (si >= 0 && si < (int)g.world.sites.size()) town = g.world.sites[(size_t)si].name;
  bool moved = false;
  if (!town.empty())
    for (const std::string& pat : {" OF " + town, " IN " + town, " AT " + town}) {
      const size_t at = t.find(pat);
      if (at == std::string::npos) continue;
      t.erase(at, pat.size());
      moved = true;
      break;
    }
  for (const char* mid : {", OVERHEARD: "}) {
    const size_t at = t.find(mid);
    if (at != std::string::npos) t.replace(at, std::strlen(mid), ": ");
  }
  if (t.rfind("THE ", 0) == 0) t = t.substr(4);
  const int n = std::max(1, (int)((w + 1) / 6));
  // whole with its town; else without it; else (a person's need) without their name, the need alone ("NEEDS A
  // COURIER": the name heads the picked lead's page); else the need cut at a word with "..."
  if (moved && (int)(t.size() + town.size() + 3) <= n) return t + " (" + town + ")";
  if ((int)t.size() <= n) return t;
  if (l.who && l.from == jobs::LeadFrom::Gossip) {   // (a teller's row keeps the name: it is who to find)
    const size_t sp = t.find(' ');
    if (sp != std::string::npos && sp + 1 < t.size()) {
      t = t.substr(sp + 1);
      if ((int)t.size() <= n) return t;
    }
  }
  return fitText(t, w - 18) + "...";
}
// (fixer M4 r1) the tracked quest's title on the HUD column: whole up to 30 characters (M4's titles run long: "BREAK
// THE SIEGE OF JARFAADARA"), longer ones cut at a word with an ellipsis (never a hard cut mid-word)
std::string hudQuestTitle(const std::string& s) {
  if (s.size() <= 30) return s;
  return fitText(s, 27 * 6 - 1) + "...";
}

// (M2) the tracked objective's tile in window coordinates: Game::questTarget, else the quest's own global tile (a far
// objective in the endless world: it may lie well outside the loaded window; the arrow and the minimap pin still point)
bool questTile(const Game& g, const Quest& q, int& tx, int& ty) {
  if (g.questTarget(q.id, tx, ty)) return true;
  if (q.state == QState::Done || !q.hasPos) return false;
  const int64_t lx = (int64_t)q.tgx - g.world.ox, ly = (int64_t)q.tgy - g.world.oy;
  tx = (int)std::clamp<int64_t>(lx, -1000000, 1000000);
  ty = (int)std::clamp<int64_t>(ly, -1000000, 1000000);
  return true;
}
// (M4, VISION_PLAN 7.1; M6 the full bands) the danger skull beside the place's name: -1 none (indoors, in a settlement),
// else by D - the hero's level: 0 green (<= 0), 1 yellow (<= 4), 2 red (<= 10), 3 purple (beyond)
int dangerSkull(const Game& g) {
  if (g.inside || g.mode == Mode::Title || !g.world.endless) return -1;
  if (g.curSite >= 0 && g.curSite < (int)g.world.sites.size() && g.world.sites[(size_t)g.curSite].settlement()) return -1;
  const int d = g.world.zoneLevel((int)std::floor(g.pl().p.x / TILE), (int)std::floor(g.pl().p.y / TILE)) - g.plLevel;
  return d <= 0 ? 0 : d <= 4 ? 1 : d <= 10 ? 2 : 3;
}
// a distance in tiles for the edge arrow: "87", "1.4K", "23K"
std::string distText(float d) {
  const int n = (int)d;
  if (n < 1000) return std::to_string(n);
  char b[16];
  if (n < 10000) std::snprintf(b, sizeof b, "%d.%dK", n / 1000, (n % 1000) / 100);
  else std::snprintf(b, sizeof b, "%dK", n / 1000);
  return b;
}
const char* useVerb(int pr);
// the verb for the prop at (tx, ty): a bed that is not yours to sleep in is only looked at
const char* useVerbAt(const Game& g, int pr, int tx, int ty) {
  if (pr == (int)art::Prop::Bed + 1 && !g.bedIsYours(tx, ty)) return "LOOK";
  {   // (P1b) the story's own word for it: LIGHT, SEARCH, OPEN, DRAW
    int wx = 0, wy = 0;
    if (story::wantedProp(g, wx, wy) == pr && wx == tx && wy == ty) return story::wantedVerb(pr - 1);
  }
  return useVerb(pr);
}
const char* useVerb(int pr) {
  switch ((art::Prop)(pr - 1)) {
    case art::Prop::Chest: return "OPEN";
    case art::Prop::Shrine: case art::Prop::Altar: case art::Prop::SummitShrine: return "PRAY";
    case art::Prop::BerryBush: return "PICK";
    case art::Prop::Signpost: return "READ";
    case art::Prop::Bed: case art::Prop::Hammock: case art::Prop::SleepingMat: case art::Prop::Bedroll: return "SLEEP";
    default: return "USE";
  }
}
int equippedOf(const Game& g, ItemKind k) {
  switch (k) {
    case ItemKind::Weapon: return g.eqWeapon; case ItemKind::Bow: return g.eqBow; case ItemKind::Staff: return g.eqStaff;
    case ItemKind::Armor: return g.eqArmor; case ItemKind::Helmet: return g.eqHelmet; case ItemKind::Shield: return g.eqShield;
    case ItemKind::Ring: return g.eqRing; case ItemKind::Amulet: return g.eqAmulet;
    case ItemKind::Gloves: return g.eqGloves; case ItemKind::Boots: return g.eqBoots; case ItemKind::Cloak: return g.eqCloak; default: return -1;
  }
}
}  // namespace


// ------------------------------------------------------------------ drawing helpers
void View::panel(float x, float y, float w, float h, float alpha) {
  Pix& P = *pix_;
  P.rect(x + 2, y + 2, w, h, Color(0, 0, 0, 0.35f * alpha));
  P.rect(x, y, w, h, Color(kPanel.r, kPanel.g, kPanel.b, alpha));
  P.frame(x, y, w, h, Color(0.55f, 0.45f, 0.28f, alpha));
  P.frame(x + 1, y + 1, w - 2, h - 2, Color(0.22f, 0.18f, 0.12f, alpha));
  // corner studs
  for (float cx : {x + 1, x + w - 3}) for (float cy : {y + 1, y + h - 3}) P.rect(cx, cy, 2, 2, Color(0.9f, 0.75f, 0.4f, alpha));
}
void View::button(float x, float y, float w, float h, const std::string& label, bool hot) {
  Pix& P = *pix_;
  P.rect(x, y, w, h, hot ? Color(0.36f, 0.27f, 0.14f, 0.95f) : Color(0.16f, 0.13f, 0.11f, 0.95f));
  P.frame(x, y, w, h, hot ? kGold : Color(0.45f, 0.37f, 0.24f));
  P.text(x + w / 2, y + (h - 7) / 2, label, 1, hot ? Color(1, 0.95f, 0.8f) : kText, 1);
}
void View::wrapText(float x, float y, float w, const std::string& s, Color c, int maxChars, int lineH) {
  int per = std::max(4, (int)(w / 6));
  auto lines = wrap(s, per);
  int shown = 0;
  for (size_t i = 0; i < lines.size(); i++) {
    std::string L = lines[i];
    if (maxChars >= 0) {
      if (shown >= maxChars) break;
      if (shown + (int)L.size() > maxChars) L = L.substr(0, maxChars - shown);
      shown += (int)lines[i].size();
    }
    pix_->text(x, y + i * lineH, L, 1, c);
  }
}

// ------------------------------------------------------------------ main draw
void View::draw(Game& g, bool hasSave) {
  Pix& P = *pix_;
  hasSave_ = hasSave;
  v1DrawsLast = v1Draws;   // (P2a) people frames drawn from v1 sheets last frame
  if (v1DrawsRig >= 2) v1DrawsRun2 += v1Draws;   // (P2c flip, D103) the run's sums, by the rig the frame was drawn at
  else if (v1DrawsRig == 1) v1DrawsRun1 += v1Draws;
  if (v1Draws > 0 && v1DrawsRig >= 2 && std::getenv("EMB_V1TRACE")) std::printf("v1trace t %.3f mode %d n %d\n", t_, (int)g.mode, v1Draws);
  v1Draws = 0;
  v1DrawsRig = art::rigVersion();
  menuFigs.clear();        // (P2c SWITCH) the menu figures drawn this frame (Witness state.menus)
  P.begin(Color(0, 0, 0));
  // (P2a) the WORLD layer (engine/pix.h): every world pixel a whole D x D block of device px; the world view is Pix::W x
  // Pix::H in here (View::viewW / viewH outside)
  ringOn_ = false;
  P.beginWorld();
  drawWorld(g);
  drawLighting(g);
  drawWeather(g, 0);
  // quest-giver markers after the night/weather pass so the "!" stays readable in the dark
  if (g.mode != Mode::Title && g.mode != Mode::Creator) {
    drawMarkers(g, Vec2(std::floor(cam_.x + shakeOff_.x), std::floor(cam_.y + shakeOff_.y)));
    companionOverlays(g, Vec2(std::floor(cam_.x + shakeOff_.x), std::floor(cam_.y + shakeOff_.y)));   // (M8) the wound, the binding
    drawPowerFx(g, Vec2(std::floor(cam_.x + shakeOff_.x), std::floor(cam_.y + shakeOff_.y)));   // (M8b) the worn powers' effects
    drawMagicFx(g, Vec2(std::floor(cam_.x + shakeOff_.x), std::floor(cam_.y + shakeOff_.y)));   // (M9) the spells (render_magic.cpp)
    drawReliefOver(g, Vec2(std::floor(cam_.x + shakeOff_.x), std::floor(cam_.y + shakeOff_.y)));   // (M11) cloud shadows, haze (render_relief.cpp)
    drawSeaOver(g, Vec2(std::floor(cam_.x + shakeOff_.x), std::floor(cam_.y + shakeOff_.y)));   // (M10) the Mist, storms (render_sea.cpp)
  }
  P.endWorld();
  // (P2a `ui 0`) the world alone: the device shots p2_measure.mjs checks for whole D x D blocks
  if (!p2a.ui) return;
  // ---- the UI layer from here: text anchored on the world goes through toUi (STYLE_P2 18: its glyphs the same size
  // at every zoom), world sizes through uiPerWorld
  drawBeastPlates(g);   // the elites' plates, a world boss's bar (render.cpp)
  // (fixer M8 r1) every spoken plate this frame keeps clear of the HUD's bands and of the plates placed before it
  plateBoxes_.clear();
  std::vector<PlateBox> speechBands;
  if (g.mode == Mode::Play) hudBands(g, speechBands);
  for (const FloatText& f : texts_) {
    if (f.speech) {
      // (M5) a spoken line: wrapped to ~22 characters, centred over the speaker on a dark plate with a 1 px rim, in the
      // speaker's colour; fades in over a tenth of a second and out over its last half second; whole pixels
      const float a = clampf(f.t / 0.1f, 0, 1) * clampf((f.life - f.t) / 0.5f, 0, 1);
      const std::vector<std::string> ls = wrap(f.s, 22);
      int w = 0;
      for (const std::string& l : ls) w = std::max(w, P.textW(l, 1));
      const float lh = 9.0f, h = ls.size() * lh + 3.0f;
      const Vec2 fu = toUi(f.p);   // (P2a) the speaker's anchor, projected from the world layer
      float cx = std::floor(fu.x), y0 = std::floor(fu.y - h);
      cx = clampf(cx, (float)Pix::SL + w / 2.0f + 4, (float)(Pix::W - Pix::SR) - w / 2.0f - 4);
      // (M5 fixer) never under the location / quest column: slide left of it (or below it when that is nearer)
      if (y0 < hudColBottom_ && cx + w / 2.0f + 3 > hudColLeft_) {
        const float left = hudColLeft_ - w / 2.0f - 4;
        if (left >= (float)Pix::SL + w / 2.0f + 4 && hudColLeft_ - (cx - w / 2.0f) < hudColBottom_ - y0 + 40) cx = left;
        else y0 = hudColBottom_ + 2;
      }
      // (M5) and never over the name tag / E: TALK of the person the player faces (often the speaker): it stacks above
      if (tagOn_ && cx + w / 2.0f + 3 > tagX0_ && cx - w / 2.0f - 3 < tagX1_ && y0 - 1 < tagY1_ && y0 + h + 1 > tagY0_)
        y0 = std::floor(tagY0_ - 2 - h);
      if (touchUI && g.mode == Mode::Play) clearOfTouch(cx, y0, (float)w + 6, h + 2);
      // (M8) and never over the party's portraits at the left edge
      if (partyColR_ > 0 && cx - w / 2.0f - 3 < partyColR_ && y0 < partyColB_) cx = std::floor(partyColR_ + w / 2.0f + 5);
      if (g.mode == Mode::Play) {   // (fixer M8 r1) never over another plate or the banner ("EVEVENING, XOTZIXA")
        std::vector<PlateBox> avoid = speechBands;
        avoid.insert(avoid.end(), plateBoxes_.begin(), plateBoxes_.end());
        float px0 = cx - w / 2.0f - 3, py0 = y0 - 1;
        clearPlate(px0, py0, (float)w + 6, h + 2, avoid, (float)Pix::SL, (float)Pix::ST, (float)(Pix::W - Pix::SR), (float)(Pix::H - Pix::SB));
        cx = std::floor(px0 + w / 2.0f + 3); y0 = std::floor(py0 + 1);
        PlateBox pb; pb.x0 = cx - w / 2.0f - 3; pb.y0 = y0 - 1; pb.x1 = pb.x0 + (float)w + 6; pb.y1 = pb.y0 + h + 2;
        plateBoxes_.push_back(pb);
      }
      P.rect(cx - w / 2.0f - 3, y0 - 1, (float)w + 6, h + 2, Color(0.02f, 0.02f, 0.04f, 0.45f * a));
      P.rect(cx - w / 2.0f - 2, y0, (float)w + 4, h, Color(0.06f, 0.05f, 0.08f, 0.72f * a));
      for (size_t k = 0; k < ls.size(); k++) {
        P.text(cx + 1, y0 + 2 + k * lh + 1, ls[k], 1, Color(0, 0, 0, a * 0.8f), 1);
        P.text(cx, y0 + 2 + k * lh, ls[k], 1, Color(f.c.r, f.c.g, f.c.b, a), 1);
      }
      m5Count_.speech++;
      continue;
    }
    float a = clampf(1.2f - f.t, 0, 1);
    const Vec2 fu = toUi(f.p);   // (P2a) a combat number: anchored on the world, its glyphs the UI's size
    P.text(fu.x + 1, fu.y + 1, f.s, 1, Color(0, 0, 0, a * 0.7f), 1);
    P.text(fu.x, fu.y, f.s, 1, Color(f.c.r, f.c.g, f.c.b, a), 1);
  }
  if (g.mode == Mode::Play) drawBanter(g, Vec2(std::floor(cam_.x + shakeOff_.x), std::floor(cam_.y + shakeOff_.y)));   // (M8) party_hud.cpp
  if (g.mode == Mode::Title) { drawTitle(g, hasSave); }
  else if (g.mode == Mode::Creator) { drawCreator(g); }
  else {
    // (M1 round 3) the HUD steps away under the menu and the shop (its bars, gold, minimap and the hamburger showed
    // round the panel's edges on a wide phone)
    if (g.mode != Mode::Menu && g.mode != Mode::Shop && g.mode != Mode::LevelUp && g.mode != Mode::Forge && g.mode != Mode::Build) drawHud(g);
    partyWorldText(g);   // (P2a) the party's names over the world (party_view.cpp; after the HUD's name tag it keeps clear of)
    drawPips(g);         // (P2a) the off-screen danger pips at the safe area's edge, over the HUD's banners (render.cpp)
    if (touchUI && (g.mode == Mode::Play)) drawTouch(g);
    if (g.mode == Mode::Play) drawPartyRadial(g);   // (M8) the companions' orders, over the whole HUD (party_hud.cpp)
    if (g.mode == Mode::Dialogue) drawDialogue(g);
    // toasts and the banner wait while a dialogue is open (render.cpp holds their timers): on a phone the touch
    // panel is tall and anything drawn over it hides the speaker and the first line
    if (g.mode != Mode::Shop && g.mode != Mode::Menu && g.mode != Mode::Dialogue && g.mode != Mode::Forge && g.mode != Mode::Build && !(radial_.open && g.mode == Mode::Play)) drawToasts();
    if (g.mode == Mode::Shop) drawShop(g);
    if (g.mode == Mode::Forge) drawForge(g);   // M6 (craft_ui.cpp)
    if (g.mode == Mode::Build) drawBuild(g);   // M7 (buildmode.cpp)
    if (g.mode == Mode::Menu) drawMenu(g);
    if (g.mode == Mode::LevelUp) drawLevelUp(g);
    if (g.mode == Mode::Dead) drawDead(g);
  }
  if (settingsOpen_) drawSettings();
  float f = std::max(fade_, g.sleepFade > 0 ? std::min(1.0f, g.sleepFade) : 0.0f);
  if (f > 0) P.rect(0, 0, Pix::W, Pix::H, Color(0, 0, 0, f));
  // (M2) travel behind the fade: where to and how long, on the black (a respawn says nothing: the death screen did)
  // (M2 fixer round 2: not while paused: the PAUSED box below says what is going on)
  if (g.travelling() && g.travel.kind != 2 && f > 0.05f && g.mode != Mode::Paused) {
    std::string dest = g.travel.site >= 0 && g.travel.site < (int)g.world.sites.size() ? g.world.sites[(size_t)g.travel.site].name : std::string("THE ROAD");
    const int hrs = std::max(1, (int)std::lround(g.travel.hours));
    const std::string l1 = (g.travel.kind == 1 ? "BY CARRIAGE TO " : "TRAVELLING TO ") + dest;
    const std::string l2 = std::to_string(hrs) + (hrs == 1 ? " HOUR" : " HOURS") + " ON THE ROAD";
    const UiBox b = uiBox(270);
    const float cx = b.x + b.w / 2.0f, cy = b.y + b.h / 2.0f, a = clampf((f - 0.05f) * 3, 0, 1);
    const int sc = P.textW(l1, 2) <= b.w - 24 ? 2 : 1;
    P.textS(cx, cy - 6 - 7 * sc, l1, sc, Color(kGold.r, kGold.g, kGold.b, a), 1);
    P.rect(cx - 60, cy + 2, 120, 1, Color(kGold.r, kGold.g, kGold.b, 0.5f * a));
    P.text(cx, cy + 9, l2, 1, Color(kText.r, kText.g, kText.b, a), 1);
    // a dotted road that fills while the journey is under way
    const int dots = 9, lit = std::min(dots, (int)(t_ * 4) % (dots + 1));
    for (int k = 0; k < dots; k++) P.rect(cx - 40 + k * 10, cy + 24, 2, 2, k < lit ? Color(kGold.r, kGold.g, kGold.b, a) : Color(0.35f, 0.3f, 0.25f, a));
  }
  // (M2 fixer round 2) the pause box over everything, the fade too: a pause on the road (a key, or the phone switching
  // apps) used to leave a black screen that looked hung
  if (g.mode == Mode::Paused) {
    P.rect(0, 0, Pix::W, Pix::H, Color(0, 0, 0, 0.5f));
    const UiBox b = uiBox(270);
    P.pushBox(b.x, b.y, b.w, b.h);
    P.text(Pix::W / 2, Pix::H / 2 - 15, "PAUSED", 3, kGold, 1);
    P.text(Pix::W / 2, Pix::H / 2 + 15, "ESC / TAP TO RESUME", 1, kText, 1);
    if (g.travelling() && g.travel.kind != 2) {
      const std::string dest = g.travel.site >= 0 && g.travel.site < (int)g.world.sites.size() ? g.world.sites[(size_t)g.travel.site].name : std::string("THE ROAD");
      P.text(Pix::W / 2, Pix::H / 2 + 30, "ON THE ROAD TO " + dest, 1, Color(kText.r, kText.g, kText.b, 0.7f), 1);
    }
    P.popBox();
  }
  if (!loadingCard.empty()) {   // (M1) the world is being made: say so instead of a frozen title
    P.rect(0, 0, Pix::W, Pix::H, Color(0.03f, 0.025f, 0.04f));
    P.text(Pix::W / 2, Pix::H / 2 - 12, loadingCard, 2, kGold, 1);
    P.text(Pix::W / 2, Pix::H / 2 + 14, "...", 2, kText, 1);
  }
}

// (M2, owner carry-over 1) The panels' box: the whole safe area (at least 480 x 270: screen.cpp guarantees that much),
// so on a phone the menus, the shop, the dialogue and the creator use its real width and height instead of a centred
// 480-wide box with dead bands either side. Every panel lays itself out from Pix::W / Pix::H inside the box. Only a
// very large logical canvas (a big window in PIXEL PERFECT at 1x) is capped, so text never strings out across a
// whole monitor: at most kMaxBoxW x max(maxH, kMaxBoxH), centred.
namespace { constexpr int kMaxBoxW = 760, kMaxBoxH = 400; }
View::UiBox View::uiBox(int maxH) const {
  const int aw = Pix::W - Pix::SL - Pix::SR, ah = Pix::H - Pix::ST - Pix::SB;
  UiBox b;
  b.w = std::max(1, std::min(kMaxBoxW, aw));
  b.h = std::max(1, std::min(std::max(maxH, kMaxBoxH), ah));
  b.x = Pix::SL + (aw - b.w) / 2;
  b.y = Pix::ST + (ah - b.h) / 2;
  return b;
}
// the modal panels' box (menu, shop, dialogue, level-up): the same safe-area box; the MAP tab gets all of it
View::UiBox View::modalBox(const Game& g) const {
  UiBox b = uiBox(300);
  if (g.mode == Mode::Menu && menuTab_ == 2) {
    const int aw = Pix::W - Pix::SL - Pix::SR, ah = Pix::H - Pix::ST - Pix::SB;
    b.w = std::max(1, aw); b.h = std::max(1, ah);
    b.x = Pix::SL; b.y = Pix::ST;
  }
  return b;
}
int View::titleItems() const { return hasSave_ ? 3 : 2; }

// (M9 fixer, review: "the quick bar runs under the region banner") the first-visit region banner's text line: below a
// world boss's bar, and dropped under the desktop quick-cast bar (T+26..T+44) when its plate would reach the bar
float View::biomeBannerY(float half) const {
  const float T = (float)Pix::ST;
  float y = std::max(T + 30, bossBarBottom_ + 10);
  if (quickBarR_ > 0 && quickBarR_ + 6 > Pix::W / 2.0f - half && y - 4 < T + 47) y = T + 51;
  return y;
}

// (SOLID 3.4c) one HUD frame's shared values: the safe area, the world-to-UI scale, the hero's rig, and the location
// column's fade, line offset and extent (set by hudLocationColumn, read by hudQuest)
struct View::HudFrame {
  Pix& P;
  const Actor& p;                 // the hero
  float L, T, R, B;               // M1: the safe area every HUD piece is anchored to
  float kw;                       // (P2a) UI px per world px
  const art::RigMetrics& RM;      // the hero's rig (as drawn)
  float hudA = 1.0f, backA = 0.58f;   // the column's text alpha and its backing's
  float kdy = 0;                  // the realm / state lines' height under the place's name
  float colBottom = 0, colLeft = 0;   // the foot and the left edge of the location / quest column
  float clipY = 1e9f;             // (M2) the column's lines stop above a dialogue panel
  Color fa(Color c) const { c.a *= hudA; return c; }
  void colText(float y, const std::string& s, Color c) const { if (y + 8 <= clipY) P.textS(R - 5, y, s, 1, c, 2); }
};

// (M2 integration) the place's name as the column shows it: long wayside names cut between words (hudLocationColumn)
static std::string hudPlaceName(const Game& g) {
  std::string loc = g.locName;
  {   // (M2 integration) long wayside names ("UNDERBRIDGE ON THE COLDBROOK"): more room on a wide screen, and a cut
      // falls between words, never mid-word
    const size_t maxc = Pix::W >= 560 ? 32 : 26;
    // (fixer M6b r3) a building's name too long for the line drops its article first ("BAB AA - THE PROVOST'S
    // GUILDHALL" -> "BAB AA - PROVOST'S GUILDHALL"), so the noun is not what the cut loses
    if (loc.size() > maxc) {
      const size_t th = loc.find(" - THE ");
      if (th != std::string::npos) loc.erase(th + 3, 4);
    }
    // ... then the owner's possessive ("BAB AA - PROVOST'S GUILDHALL" -> "BAB AA - GUILDHALL")
    if (loc.size() > maxc) {
      const size_t dash = loc.find(" - "), ps = dash == std::string::npos ? dash : loc.find("'S ", dash);
      if (ps != std::string::npos) {
        const size_t w0 = loc.rfind(' ', ps);
        if (w0 != std::string::npos && w0 >= dash + 2) loc.erase(w0 + 1, ps + 3 - (w0 + 1));
      }
    }
    if (loc.size() > maxc) {
      const size_t sp = loc.find_last_of(' ', maxc);
      loc.resize(sp != std::string::npos && sp >= maxc / 2 ? sp : maxc);
      // never end on a dangling little word ("UNDERBRIDGE ON THE"), a possessive with nothing after it ("THE
      // PROVOST'S") or a bare dash
      for (int pass = 0; pass < 3; pass++) {
        for (const char* w : {" THE", " ON", " OF", " IN", " AT", " BY", " AND", " -"}) {
          const size_t n = std::strlen(w);
          if (loc.size() > n + 4 && loc.compare(loc.size() - n, n, w) == 0) loc.resize(loc.size() - n);
        }
        if (loc.size() > 6 && loc.compare(loc.size() - 2, 2, "'S") == 0) {
          const size_t ws = loc.find_last_of(' ');
          if (ws != std::string::npos && ws >= 4) loc.resize(ws);
        }
      }
    }
  }
  return loc;
}

// the column's clock line: "DAY n HH:MM" (hudLocationColumn)
static std::string hudClock(const Game& g) {
  int hh = (int)g.hour, mm = (int)((g.hour - hh) * 60);
  char clock[16];
  std::snprintf(clock, sizeof clock, "%02d:%02d", hh, mm);
  std::string dayStr = std::string("DAY ") + std::to_string(g.day) + " " + clock;
  return dayStr;
}

void View::drawHud(Game& g) {
  Pix& P = *pix_;
  const Actor& p = g.pl();
  // M1: every HUD piece is anchored to the safe area (the notch side of a phone, the HUD MARGIN setting)
  const float L = (float)Pix::SL, T = (float)Pix::ST, R = (float)(Pix::W - Pix::SR), B = (float)(Pix::H - Pix::SB);
  // (P2a) what the HUD places over the world is projected through toUi; a world size is kw UI px per world px and the
  // heights over a body come from the rig's metrics
  const float kw = uiPerWorld();
  const art::RigMetrics& RM = rigFor(g.pl().look);   // the hero's (as drawn)
  HudFrame H{P, p, L, T, R, B, kw, RM};
  const float spellX = hudVitals(g, H);
  hudQuickSlots(g, H, spellX);
  hudBurden(g, H);

  hudLocationColumn(g, H);

  hudQuest(g, H);

  hudBossBar(g, H);

  hudPrompt(g, H);

  const bool ringOpen = hudNotices(g, H);   // (the companions' order ring is open: the title cards step away)
  if (!ringOpen) drawHerald(g);
  drawHomeHud(g);   // (M7) YARD / DECORATE, DISMOUNT (buildmode.cpp)
  drawPartyHud(g);  // (M8) the companions' portraits and alerts (party_hud.cpp)
}

// (SOLID 3.4c) the vitals: level, health / magicka / stamina bars, buffs, xp, gold, arrows, the potion belt (drawHud)
float View::hudVitals(Game& g, const HudFrame& H) {
  Pix& P = H.P;
  const Actor& p = H.p;
  const float L = H.L, T = H.T;
  // vitals
  float mpMax = g.maxMp + (float)g.craft.live.stats.get(Affix::Magicka), stMax = g.maxSt;
  for (int idx : g.worn()) {
    if (idx < 0) continue;
    if (g.inv[idx].ench == Ench::Magicka) mpMax += g.inv[idx].enchPow;
    if (g.inv[idx].ench == Ench::Stamina) stMax += g.inv[idx].enchPow;
  }
  if (g.craft.live.stats.has(Unique::Wayfarer)) stMax *= 1.2f;
  auto bar = [&](float x, float y, float w, float v, float mx, Color c, Color dark) {
    P.rect(x - 1, y - 1, w + 2, 6, Color(0.05f, 0.04f, 0.06f, 0.85f));
    P.rect(x, y, w, 4, Color(dark.r, dark.g, dark.b, 0.9f));
    float k = clampf(v / std::max(1.0f, mx), 0, 1);
    P.rect(x, y, w * k, 4, c);
    P.rect(x, y, w * k, 1, Color(std::min(1.0f, c.r + 0.3f), std::min(1.0f, c.g + 0.3f), std::min(1.0f, c.b + 0.3f)));
  };
  drawSeaHud(g);   // (M10) the sail and fishing panels (render_sea.cpp)
  drawRiseHud(g);  // (M12) the herald's banner of a rise, the rank's insignia (render_rise.cpp)
  P.textS(L + 6, T + 5, "LV " + std::to_string(g.plLevel), 1, kGold);
  bar(L + 36, T + 6, 90, p.hp, p.maxHp, Color(0.85f, 0.18f, 0.16f), Color(0.3f, 0.05f, 0.05f));
  bar(L + 36, T + 14, 70, g.mp, mpMax, Color(0.25f, 0.45f, 0.95f), Color(0.06f, 0.1f, 0.3f));
  bar(L + 36, T + 22, 70, g.stamina, stMax, Color(0.3f, 0.8f, 0.35f), Color(0.06f, 0.22f, 0.08f));
  if (g.stFlash > 0) P.rect(L + 35, T + 21, 72, 6, Color(1, 0.25f, 0.2f, ((int)(g.stFlash * 16) & 1) ? 0.75f : 0.3f));   // too winded to act
  drawBuffs(g, L + 110, T + 13);   // (M5, 15.2) Well Fed, Rested, Hungry, Weary beside the shorter bars
  // xp sliver
  P.rect(L + 6, T + 14, 26, 2, Color(0.1f, 0.1f, 0.1f, 0.8f));
  P.rect(L + 6, T + 14, 26 * clampf(g.plXp / (float)gear::xpForNext(g.plLevel), 0, 1), 2, kGold);
  // gold, arrows, spell
  int arrows = 0;
  for (auto& it : g.inv) if (it.kind == ItemKind::Arrows) arrows += it.count;
  P.blit(sprites_.iconTex(*this, art::Icon::Gold, 0), L + 4, T + 28);
  P.textS(L + 20, T + 33, std::to_string(g.gold), 1, kGold);
  P.blit(sprites_.iconTex(*this, art::Icon::Arrows, 0), L + 56, T + 28);
  P.textS(L + 72, T + 33, std::to_string(arrows), 1, kText);
  float spellX = L + 100;
  if (!touchUI) {   // (M6) the potion belt and its cooldown (touch: on the potion button)
    int pots = 0;
    for (const Item& it : g.inv) if (it.kind == ItemKind::Potion && it.sub == (uint8_t)PotionType::Health) pots += it.count;
    if (pots > 0 || g.craft.live.potionCd > 0) {
      P.blit(sprites_.iconTex(*this, art::Icon::PotionRed, 0), L + 88, T + 28);
      const float cd = g.craft.live.potionCd, k = cd > 0 ? clampf(cd / std::max(0.1f, g.craft.live.potionCdMax), 0, 1) : 0.0f;
      if (k > 0) P.rect(L + 90, T + 30 + 12 * (1 - k), 12, 12 * k, Color(0.02f, 0.02f, 0.05f, 0.7f));
      P.textS(L + 104, T + 33, cd > 0 ? std::to_string((int)std::ceil(cd)) + "S" : std::to_string(pots), 1, cd > 0 ? kDim : kText);
      spellX = L + 124;
    }
  }
  return spellX;
}

// (M9) the quick slots, the sneak eye, LEVEL UP and the blessing (drawHud)
void View::hudQuickSlots(Game& g, const HudFrame& H, float spellX) {
  Pix& P = H.P;
  const float L = H.L, T = H.T;
  // (M9) the prepared spells from the book: on the desktop a bar of the four quick slots (number, orb, cooldown; keys
  // 1-4); on touch the quick-cast buttons round the attack button show them (drawTouch)
  g_quickBarR = spellX;
  quickBarR_ = 0;
  if (!touchUI) {
    bool any = false;
    for (uint32_t id : g.arcana.book.slot) if (id) any = true;
    if (any) quickBarR_ = g_quickBarR = SkillsUiOps::drawQuickBar(*this, g, spellX, T + 26);
  }
  SkillsUiOps::drawSneakEye(*this, g, L + 130, T + 4);   // (M9) sneaking: a lidded eye beside the health bar
  if (g.perkPts > 0 && ((int)(t_ * 2) & 1)) P.textS(L + 6, T + 44, "LEVEL UP! OPEN MENU", 1, kGold);
  if (g.blessT > 0) P.textS(L + 6, T + (g.perkPts > 0 ? 54 : 44), g.blessName, 1, Color(0.7f, 0.85f, 1.0f, 0.8f));
}

// (P1b) a story's burden: the HEAVY LOAD chip (drawHud)
void View::hudBurden(Game& g, const HudFrame& H) {
  Pix& P = H.P;
  const float L = H.L, T = H.T;
  // (P1b ENGINE lane) a story's burden (a body carried home, a cask, a sack): the HEAVY LOAD chip, a small iron weight
  // and the words on a dark plate with an amber rim, so the slower step is never a mystery
  if (g.story.burdened()) {
    const float cy = T + (g.perkPts > 0 ? 10.0f : 0.0f) + (g.blessT > 0 ? 10.0f : 0.0f) + 43.0f;
    const char* lab = "HEAVY LOAD";
    const float w = (float)P.textW(lab, 1) + 20.0f, x0 = L + 4;
    P.rect(x0, cy, w, 11, Color(0.06f, 0.05f, 0.05f, 0.78f));
    P.frame(x0, cy, w, 11, Color(0.78f, 0.55f, 0.24f, 0.95f));
    // the weight: a trapezoid of iron, its ring on top, lit from the top left
    const float wx = x0 + 3, wy = cy + 2;
    P.rect(wx + 3, wy, 3, 2, Color(0.55f, 0.55f, 0.58f));
    P.rect(wx + 4, wy + 1, 1, 1, Color(0.06f, 0.05f, 0.05f));
    P.rect(wx + 1, wy + 2, 7, 5, Color(0.38f, 0.38f, 0.42f));
    P.rect(wx + 1, wy + 2, 7, 1, Color(0.62f, 0.62f, 0.66f));
    P.rect(wx + 1, wy + 2, 1, 5, Color(0.56f, 0.56f, 0.6f));
    P.rect(wx, wy + 6, 9, 1, Color(0.22f, 0.22f, 0.26f));
    P.textS(x0 + 14, cy + 2, lab, 1, Color(1.0f, 0.78f, 0.45f));
  }
}

// the location / quest column's fade over a hall's rooms, a boss or a foe under it; whether the minimap shows (hudLocationColumn)
bool View::hudColumnFade(Game& g, HudFrame& H) {
  const float T = H.T, R = H.R;
  const float kw = H.kw;
  float& hudA = H.hudA;
  float& backA = H.backA;
  // M0b fix round 3: inside a building the rooms fill the screen and a wide one runs on under the minimap and the
  // location / quest column. The building is in full view, so its minimap is dropped there, and the column fades to
  // a faint overlay (fainter still with the hero right under it) so the hearth corner and the shelves show through.
  hudA = 1.0f; backA = 0.58f;   // (M2 fixer: 0.38 let bright paving, snow and roofs wash the lines out)
  bool showMini = true;
  // (fixer M7b r2, review: "a tall touch-mode dialogue panel leaves the minimap's empty frame sticking out beside it")
  // while the phone's tall talk panel is up the minimap stands down (the panel reaches the top of the screen)
  if (g.mode == Mode::Dialogue && touchUI) showMini = false;
  if (g.inside && g.subBldg >= 0) {
    // (M1) a big hall (the palace, a keep) runs on under the minimap and the column: keep both readable over the
    // shelves and beds with a firmer backing, and step aside only while the hero is right under them
    const Map& m = g.map();
    // (P2a) the room's right and top edges and the hero, projected from the world layer into the HUD's UI px
    const Vec2 mc = toUi(Vec2(m.w * 16.0f, -16.0f));
    float mx1 = mc.x, my0 = mc.y;
    const Vec2 ps = toUi(g.pl().p);
    if (mx1 > R - 74 && my0 < T + 90 && ps.x > R - 84 && ps.y < T + 100) showMini = false;
    if (mx1 > R - 160 && my0 < T + 130) {   // the rooms run on under the location / quest column
      backA = 0.74f;
      if (ps.x > R - 170 && ps.y < T + 160) { hudA = 0.35f; backA = 0.3f; }
    }
  }
  // (M6 fixer r2, review: "the info panel is drawn over the STONEFATHER world boss") a boss standing under the location
  // / quest column fades the column the way a hall's rooms do: the fight is what matters
  if (!g.inside && g.mode == Mode::Play) {
    for (const Actor& a : g.actors) {
      if (!a.hostile || a.st == AState::Dead || !(a.boss || a.rank >= 4)) continue;
      const Vec2 s = toUi(a.p);
      const float half = (a.scalePct > 100 ? 8.0f * a.scalePct / 100.0f + 6.0f : 14.0f) * kw;
      if (s.x + half > R - 180 && s.x - half < R && s.y - 3.0f * half < T + 165 && s.y > T + 60) { hudA = 0.3f; backA = 0.22f; break; }
    }
  }
  // (fix, C10 T4 playtest: "the bandit stands under the goal panel") any live foe in the hero's own fight (within 10
  // tiles of him) standing under the column fades it the same way, eased in and out over a fraction of a second so
  // the column never blinks as the foe steps in and out from under it
  {
    float want = 0.0f;
    if (!g.inside && g.mode == Mode::Play) {
      const float cl = hudColLeft_ < 1e8f ? std::min(hudColLeft_ - 4.0f, R - 60.0f) : R - 180.0f;
      const float cb = hudColBottom_ > 0 ? hudColBottom_ + 4.0f : T + 165.0f;
      for (const Actor& a : g.actors) {
        if (!a.hostile || a.st == AState::Dead || a.id == g.pl().id) continue;
        if (len2(a.p - g.pl().p) > (10.0f * TILE) * (10.0f * TILE)) continue;
        const Vec2 s = toUi(a.p);
        const float half = (a.scalePct > 100 ? 8.0f * a.scalePct / 100.0f + 6.0f : 8.0f) * kw;
        if (s.x + half > cl && s.x - half < R && s.y - 3.0f * half < cb && s.y > T + 60) { want = 1.0f; break; }
      }
    }
    const float dt = colFoeT_ < 0 ? 1.0f : std::clamp(t_ - colFoeT_, 0.0f, 0.1f);
    colFoeT_ = t_;
    colFoeFade_ += (want - colFoeFade_) * std::min(1.0f, dt * 9.0f);
    const float e = colFoeFade_ * colFoeFade_ * (3.0f - 2.0f * colFoeFade_);
    hudA = std::min(hudA, 1.0f - 0.7f * e);
    backA = std::min(backA, 0.58f - 0.36f * e);
  }
  return showMini;
}

// the minimap and the location / clock column: the place, its mood, its realm and state, the danger skull, the day (drawHud)
void View::hudLocationColumn(Game& g, HudFrame& H) {
  Pix& P = H.P;
  const float T = H.T, R = H.R;
  float& hudA = H.hudA;
  float& backA = H.backA;
  float& colBottom = H.colBottom;
  float& colLeft = H.colLeft;
  float& clipY = H.clipY;
  const bool showMini = hudColumnFade(g, H);
  // minimap + location
  if (showMini) drawMinimap(g, R - 70, T + 24, 64);
  std::string loc = hudPlaceName(g);
  // (M5, 15.12) a settlement's mood after its name ("BJALFVA - CONTENT", "- HUNGRY", "- FESTIVAL", "- WAR-TORN"), in its
  // colour, while its census is known (outdoors there)
  std::string moodS;
  Color moodC = kDim;
  if (!g.inside && g.curSite >= 0 && g.curSite < (int)g.world.sites.size() && g.world.sites[(size_t)g.curSite].settlement()) {
    uint32_t mc = 0;
    moodS = moodWord(g, g.world.sites[(size_t)g.curSite].id, &mc);
    if (!moodS.empty()) {
      moodC = col(mc);
      const size_t maxc = Pix::W >= 560 ? 34 : 28;
      if (loc.size() + 3 + moodS.size() > maxc) moodS.clear();   // a long name keeps its room: the mood waits for the map
    }
  }
  const std::string locSuffix = moodS.empty() ? std::string() : "  " + moodS;
  // clock
  std::string dayStr = hudClock(g);
  // (M1) kingdom identity: in a settlement, the line under its name says whose it is (and marks a capital)
  std::string kingLine;
  Color kingCol = kDim;
  // (M4) the owner NOW, in its society's word ("KHAGANATE OF QIBA"), and the settlement's state under it in its colour
  std::string stateLine;
  Color stateCol = kDim;
  hudKingLines(g, loc, kingLine, kingCol, stateLine, stateCol);
  if (kingLine.empty() && !stateLine.empty()) { kingLine = stateLine; kingCol = stateCol; stateLine.clear(); }
  H.kdy = (kingLine.empty() ? 0.0f : 9.0f) + (stateLine.empty() ? 0.0f : 9.0f);
  const float kdy = H.kdy;
  colBottom = T + 150;   // the foot of the location / quest column (the quest arrow keeps below it)
  colLeft = R - 160;     // and its left edge (a long quest line on a phone runs further left than 160 px)
  // (M2) a dialogue panel runs under the column on a wide screen: the column's lines stop above the panel's top
  clipY = 1e9f;
  if (g.mode == Mode::Dialogue) {
    const UiBox db = uiBox(300);
    P.pushBox(db.x, db.y, db.w, db.h);
    clipY = db.y + dlgLay(g, dlgRowH(), dlgFirst_).y - 2;
    P.popBox();
  }
  {
    // a soft backing so the location / clock / quest column stays readable over busy roofs and snow
    const Quest* tq = g.trackedQuest >= 0 ? g.questById(g.trackedQuest) : nullptr;
    bool hasQ = tq && tq->state != QState::Done;
    int wmax = std::max(std::max(P.textW(loc + locSuffix, 1), P.textW(dayStr, 1)), std::max(P.textW(kingLine, 1), P.textW(stateLine, 1)));
    if (dangerSkull(g) >= 0) wmax = std::max(wmax, P.textW(loc + locSuffix, 1) + 11);
    if (hasQ) wmax = std::max(wmax, P.textW(hudQuestTitle(tq->title), 1));
    float extra = 0;
    if (hasQ) {
      const std::string st = trackedStep(g, *tq);
      const size_t nl = st.find('\n');
      wmax = std::max(wmax, P.textW(st.substr(0, nl), 1));
      if (nl != std::string::npos) { wmax = std::max(wmax, P.textW(st.substr(nl + 1), 1)); extra = 9.0f; }
    }
    float bh = (hasQ ? 42.0f : 21.0f) + kdy + extra;
    // (fixer M7b r3, review: "a sliver of the region box and half its skull poke out above a tall dialogue panel") the
    // column shows whole above an open dialogue or not at all
    if (g.mode == Mode::Dialogue && T + 88 + bh > clipY) clipY = -1e9f;
    P.rect(R - wmax - 10, T + 88, (float)wmax + 8, std::max(0.0f, std::min(bh, clipY - (T + 88))), Color(0.03f, 0.02f, 0.05f, backA));
    colBottom = T + 88 + bh + 12;
    colLeft = std::min(colLeft, R - wmax - 16);
    hudColLeft_ = R - wmax - 12; hudColBottom_ = T + 88 + bh + 2;
  }
  if (locSuffix.empty()) H.colText(T + 91, loc, H.fa(kText));
  else if (T + 99 <= clipY) {   // the name, then the mood in its colour (a dot between)
    const float mw = (float)P.textW(locSuffix, 1);
    P.textS(R - 5, T + 91, moodS, 1, H.fa(moodC), 2);
    P.rect(std::floor(R - 5 - mw + 3), T + 94, 2, 2, H.fa(Color(kDim.r, kDim.g, kDim.b, 0.9f)));
    P.textS(R - 5 - mw, T + 91, loc, 1, H.fa(kText), 2);
  }
  if (!kingLine.empty() && T + 108 <= clipY) {
    // the kingdom line in its colour, with a full dark outline (its tint sits close to the land's own colours)
    const Color o(0.02f, 0.02f, 0.04f, 0.9f * hudA);
    for (int d = 0; d < 4; d++) P.text(R - 5 + (d == 0 ? -1.0f : d == 1 ? 1.0f : 0.0f), T + 100 + (d == 2 ? -1.0f : d == 3 ? 1.0f : 0.0f), kingLine, 1, o, 2);
    P.text(R - 4, T + 101, kingLine, 1, o, 2);
    P.text(R - 5, T + 100, kingLine, 1, H.fa(kingCol), 2);
    if (!stateLine.empty() && T + 117 <= clipY) {
      for (int d = 0; d < 4; d++) P.text(R - 5 + (d == 0 ? -1.0f : d == 1 ? 1.0f : 0.0f), T + 109 + (d == 2 ? -1.0f : d == 3 ? 1.0f : 0.0f), stateLine, 1, o, 2);
      P.text(R - 5, T + 109, stateLine, 1, H.fa(stateCol), 2);
    }
  }
  // (M4, VISION_PLAN 7.1) dangerous country: a skull before the place's name, coloured by how far the land's danger
  // runs past the hero's level (yellow, red, purple); none at home or in safe country
  {
    const int dk = dangerSkull(g);
    if (dk >= 0 && T + 99 <= clipY) {
      static const char* kSkull[7] = {".xxxxx.", "xxxxxxx", "x..x..x", "x..x..x", "xxx.xxx", ".xxxxx.", ".x.x.x."};
      const Color sc = dk == 0 ? Color(0.45f, 0.90f, 0.42f) : dk == 1 ? Color(0.98f, 0.86f, 0.30f) : dk == 2 ? Color(0.96f, 0.36f, 0.26f) : Color(0.80f, 0.45f, 1.0f);
      const float sx = R - 5 - P.textW(loc + locSuffix, 1) - 10, sy = T + 91;
      for (int j = 0; j < 7; j++)
        for (int i = 0; i < 7; i++) {
          if (kSkull[j][i] != 'x') continue;
          P.rect(sx + i + 1, sy + j + 1, 1, 1, Color(0, 0, 0, 0.7f * hudA));
          P.rect(sx + i, sy + j, 1, 1, H.fa(sc));
        }
    }
  }
  H.colText(T + 100 + kdy, dayStr, H.fa(kDim));
  hudRaidPointer(g, H);
  hudMenuBadge(g, H);
}

// (M1, M4, M2) the line under the place's name: whose it is and its state, or what a wayside place is (hudLocationColumn)
void View::hudKingLines(Game& g, const std::string& loc, std::string& kingLine, Color& kingCol, std::string& stateLine, Color& stateCol) {
  // (M1) kingdom identity: in a settlement, the line under its name says whose it is (and marks a capital)
  // (M4) the owner NOW, in its society's word ("KHAGANATE OF QIBA"), and the settlement's state under it in its colour
  if (g.curSite >= 0 && g.curSite < (int)g.world.sites.size() && g.world.sites[g.curSite].settlement()) {
    const Site& cs = g.world.sites[(size_t)g.curSite];
    if (const Kingdom* k = g.world.kingdomOf(g.curSite)) {
      const rui::Look lk = rui::look(g, k->id);
      kingLine = cs.capital && cs.homeKingdom == cs.kingdom ? "CAPITAL OF " + lk.name : rui::realmTitle(lk, false);
      // (M12 fixer) a long realm name is shortened by words, never clipped mid-word ("LORDSHIP OF THE EMBER MARCH"
      // drops its THE; a name still too long stands alone, then loses whole trailing words)
      if (kingLine.size() > 26) {
        const size_t ot = kingLine.find(" OF THE ");
        if (ot != std::string::npos) kingLine.erase(ot + 4, 4);
      }
      if (kingLine.size() > 26) kingLine = lk.name;
      while (kingLine.size() > 26) {
        const size_t ws = kingLine.find_last_of(' ', 26);
        if (ws == std::string::npos || ws < 4) { kingLine.resize(26); break; }
        kingLine.resize(ws);
      }
      if (k->color) kingCol = mixKing(col(k->color));
    }
    uint32_t sc = 0;
    stateLine = rui::stateLine(g, cs.id, &sc);
    if (stateLine.size() > 26) stateLine = stateLine.substr(0, 26);
    if (!stateLine.empty()) stateCol = col(sc);
  }
  // (M2) a wayside place or a wonder: the line under its name says what it is ("HUNTER'S CAMP", "ELDER TREE"), unless
  // its name already does
  if (!g.inside && g.curSite >= 0 && g.curSite < (int)g.world.sites.size()) {
    const Site& cs = g.world.sites[(size_t)g.curSite];
    if (cs.type == SiteType::Vignette || cs.type == SiteType::Wonder) {
      const std::string kn = ew::poiKindName(cs.type, cs.kind);
      if (loc.find(kn) == std::string::npos) {
        kingLine = cs.type == SiteType::Wonder ? "WONDER - " + kn : kn;
        if (kingLine.size() > 26) kingLine = kn.substr(0, 26);
        kingCol = cs.type == SiteType::Wonder ? Color(0.75f, 0.85f, 1.0f) : Color(0.85f, 0.78f, 0.58f);
      }
    }
  }
}

// (M5 fixer) a live night raid: the chevron toward the nearest raider out of sight (hudLocationColumn)
void View::hudRaidPointer(Game& g, const HudFrame& H) {
  Pix& P = H.P;
  const float L = H.L, T = H.T, R = H.R, B = H.B;
  const float colBottom = H.colBottom;
  const float colLeft = H.colLeft;
  // (M5 fixer, review: "nothing points to the fight") a live night raid: a red chevron at the screen's edge toward the
  // nearest raider out of sight, pulsing, so a phone player can find (and join) the town's defence
  if (g.life.raid.site && g.life.raid.outcome == 0 && !g.inside && g.mode == Mode::Play) {
    float bd = 1e30f;
    Vec2 best;
    bool any = false, seen = false;
    for (const Actor& ra : g.actors) {
      if (std::find(g.life.raid.ids.begin(), g.life.raid.ids.end(), ra.id) == g.life.raid.ids.end()) continue;
      if (ra.st == AState::Dead || !ra.hostile) continue;
      const Vec2 sc = toUi(ra.p);   // (P2a) projected from the world layer
      if (sc.x > L && sc.y > T && sc.x < R && sc.y < B) { seen = true; break; }
      const float d2 = len2(ra.p - g.pl().p);
      if (d2 < bd) { bd = d2; best = sc; any = true; }
    }
    if (any && !seen) {
      const Vec2 c((L + R) / 2.0f, (T + B) / 2.0f);
      const Vec2 d = norm(best - c);
      const float k = std::min(((R - L) / 2.0f - 26) / std::max(0.01f, std::fabs(d.x)), ((B - T) / 2.0f - 26) / std::max(0.01f, std::fabs(d.y)));
      Vec2 a = c + d * k;
      if (a.x > colLeft && a.y < colBottom) a.x = colLeft - 4;
      if (a.x < L + 150 && a.y < T + 58) a.y = T + 58;
      a = a + d * (std::sin(t_ * 8) * 2.0f);
      const Vec2 side(-d.y, d.x);
      for (int pass = 0; pass < 2; pass++) {
        const Color cc = pass == 0 ? Color(0.08f, 0.02f, 0.02f, 0.9f) : Color(1.0f, 0.30f, 0.22f);
        const float grow = pass == 0 ? 1.0f : 0.0f;
        for (float t = 0; t <= 8.0f; t += 0.5f)
          for (int arm = -1; arm <= 1; arm += 2) {
            const Vec2 q2 = a - d * t + side * (t * (float)arm);
            P.rect(std::floor(q2.x) - grow, std::floor(q2.y) - grow, 2 + grow * 2, 2 + grow * 2, cc);
          }
      }
      P.textS(a.x - d.x * 22, a.y - d.y * 22 - 3, "RAID", 1, Color(1.0f, 0.36f, 0.28f), 1);
    }
  }
}

// the menu key's label (desktop) or the hamburger button (touch), with the perk badge (hudLocationColumn)
void View::hudMenuBadge(Game& g, const HudFrame& H) {
  Pix& P = H.P;
  const float T = H.T, R = H.R;
  if (!touchUI) {
    P.textS(R - 24, T + 7, "TAB", 1, kDim, 1);
    SkillsUiOps::drawBadge(*this, R - 46, T + 7, g.skills.perkPts);   // (M9) perk points to spend
  } else {
    const BtnDef b = btnPos(B_MENU);
    P.rect(b.x - 9, b.y - 8, 18, 16, Color(0.1f, 0.08f, 0.08f, 0.8f));
    P.frame(b.x - 9, b.y - 8, 18, 16, Color(0.6f, 0.5f, 0.3f));
    for (int k = 0; k < 3; k++) P.rect(b.x - 5, b.y - 4 + k * 4, 10, 2, kText);
    SkillsUiOps::drawBadge(*this, b.x - 15, b.y + 4, g.skills.perkPts);   // (M9) on the hamburger's lower-left corner
  }
}

// the tracked quest under the column: its title, its step, the compass caret and the map pin (drawHud)
void View::hudQuest(Game& g, HudFrame& H) {
  Pix& P = H.P;
  const Actor& p = H.p;
  const float L = H.L, T = H.T, R = H.R, B = H.B;
  const float kw = H.kw;
  float& colBottom = H.colBottom;
  float& colLeft = H.colLeft;
  const float kdy = H.kdy;
  // tracked quest
  if (g.trackedQuest >= 0) {
    const Quest* q = g.questById(g.trackedQuest);
    if (q && q->state != QState::Done) {
      const std::string obj = trackedStep(g, *q);
      const std::string t = hudQuestTitle(q->title);
      H.colText(T + 112 + kdy, t, H.fa(kGold));
      if (!obj.empty()) {
        const size_t nl = obj.find('\n');
        H.colText(T + 121 + kdy, obj.substr(0, nl), H.fa(kText));
        if (nl != std::string::npos) {
          // (P1b ENGINE lane) a story's clock reads at a glance: gold while days remain, the last day a warning red
          const std::string l2 = obj.substr(nl + 1);
          Color c2 = kText;
          if (q->type == QType::Story && l2 == "LAST DAY") c2 = ((int)(t_ * 2.5f) & 1) ? Color(1.0f, 0.42f, 0.32f) : Color(1.0f, 0.62f, 0.45f);
          else if (q->type == QType::Story && l2 == "ONE DAY LEFT") c2 = Color(1.0f, 0.62f, 0.38f);
          else if (q->type == QType::Story && l2.find("DAYS LEFT") != std::string::npos) c2 = kGold;
          H.colText(T + 130 + kdy, l2, H.fa(c2));
        }
      }
      // off-screen arrow toward the objective
      int tx, ty;
      if (!g.inside && questTile(g, *q, tx, ty)) {
        Vec2 tgt(tx * 16 + 8.0f, ty * 16 + 8.0f);
        Vec2 sc = toUi(tgt);   // (P2a) projected from the world layer
        bool on = sc.x > L + 10 && sc.y > T + 10 && sc.x < R - 10 && sc.y < B - 10;
        float dist = len(tgt - p.p) / 16;
        // (fixer M4 r1, review: "the compass chevron draws over the dialogue panel's border") no compass while a
        // dialogue panel is open: the panel is modal and sits above the world's markers
        if (!on && g.mode == Mode::Dialogue) {
        } else if (!on) {
          // the arrow rides the edge of the safe area, around its centre
          Vec2 c((L + R) / 2.0f, (T + B) / 2.0f);
          Vec2 d = norm(sc - c);
          float k = std::min(((R - L) / 2.0f - 18) / std::max(0.01f, std::fabs(d.x)), ((B - T) / 2.0f - 18) / std::max(0.01f, std::fabs(d.y)));
          Vec2 a = c + d * k;
          // slide along the screen edge out of the HUD corners (vitals top-left, minimap + quest column top-right)
          // (P1b fixer) the corner's foot follows its lines: LEVEL UP and a blessing stacked, and the HEAVY LOAD chip
          // under them (the arrow and its distance sat on the chip: "[arrow]AVY LOAD")
          float vitB = T + ((g.perkPts > 0 && g.blessT > 0) ? 66.0f : 58.0f);
          if (g.story.burdened()) vitB = std::max(vitB, T + (g.perkPts > 0 ? 10.0f : 0.0f) + (g.blessT > 0 ? 10.0f : 0.0f) + 43.0f + 11.0f + 6.0f);
          if (a.x < L + 150 && a.y < vitB) { if (a.y <= T + 20) a.x = L + 150; else a.y = vitB; }
          // (M1 round 3) below the column's real foot (a two-line step and a kingdom line made it taller than the old
          // fixed limit, and the arrow and its distance sat on the quest's last line)
          if (a.x > colLeft && a.y < colBottom) { if (a.y <= T + 20) a.x = colLeft; else a.y = colBottom; }
          // (M3c fixer, review: "the compass distance strikes through the biome banner") while the first-visit biome
          // banner shows at the top centre, the arrow and its distance keep out of its strip: along the top edge they
          // slide out past its ends, lower down they drop below it
          if (bossBarBottom_ > 0) {   // (M6 fixer) and out of a world boss's top bar (its distance too)
            // (M6 fixer r2, review: "the compass caret and its distance sit on the bar's right end" with the real
            // iPhone insets) it drops below the bar's strip instead of squeezing past its ends into the HUD labels
            const float x0 = bossBarX0_ - 48, x1 = bossBarX1_ + 48;
            if (a.y < bossBarBottom_ + 14 && a.x > x0 && a.x < x1) a.y = bossBarBottom_ + 14;
          }
          if (herald_.t > 0 && g.mode == Mode::Play) {   // (M4) and out of the herald's ribbon
            const float half = (float)P.textW(herald_.title, 1) / 2 + 40, by1 = std::max(T + 40, bossBarBottom_ + 38), mx = Pix::W / 2.0f;
            if (a.y < by1 && std::fabs(a.x - mx) < half + 30) {
              if (a.y <= T + 20) a.x = a.x < mx ? mx - half - 30 : mx + half + 30;
              else a.y = std::max(a.y, by1);
            }
          }
          if (riseBanBottom_ > 0 && g.mode == Mode::Play) {   // (M12 integrator) and out of a rise's herald banner
            const float x0 = riseBanX0_ - 40, x1 = riseBanX1_ + 40, mx = (riseBanX0_ + riseBanX1_) / 2;
            if (a.y < riseBanBottom_ + 14 && a.x > x0 && a.x < x1) {
              if (a.y <= T + 20) a.x = a.x < mx ? x0 : x1;   // along the top edge: past the banner's ends
              else a.y = riseBanBottom_ + 14;
            }
          }
          if (biomeBannerT_ > 0 && !biomeBanner_.empty() && bannerT_ <= 0 && g.mode == Mode::Play) {
            const float half = std::min((float)P.textW(biomeBanner_, 1) / 2 + 26, Pix::W / 2.0f - 70);
            const float by1 = biomeBannerY(half) + 11 + 14;
            const float mx = Pix::W / 2.0f;
            if (a.y < by1 && std::fabs(a.x - mx) < half + 40) {
              if (a.y <= T + 20) a.x = a.x < mx ? mx - half - 40 : mx + half + 40;
              else a.y = std::max(a.y, by1);
            }
          }
          // (M3 fixer round 3, review: "on the phone the quest compass draws inside the TALK button") with the touch
          // layout on, it also slides out of the action buttons (bottom right) and the movement stick (bottom left)
          if (touchUI) {
            const BtnDef bw = btnPos(B_BOW), rl = btnPos(B_ROLL), at = btnPos(B_ATTACK);
            // (M3c fixer round 3, review: "the quest pointer's distance sits inside the right-thumb touch cluster ... reads
            // like another control") a wider berth round the cluster: the arrow and the distance behind it stay a
            // thumb's width clear of ROLL, the bow and the attack button
            const QuickBtn q0 = quickBtnPos(0), q3 = quickBtnPos(3);   // (M9) the quick-cast arc reaches further out
            const float cx0 = std::min(bw.x - bw.r - 40, q0.x - q0.r - 24), cy0 = std::min(rl.y - rl.r - 34, q3.y - q3.r - 20);
            const float cyB = at.y + at.r;
            if (a.x > cx0 && a.y > cy0) { if (a.y >= cyB - 4 || a.y > B - 30) a.x = cx0; else a.y = cy0; }
            const Vec2 st = stickRest();
            const float sx1 = st.x + 64, sy0 = st.y - 74;   // (M3c fixer: clear of the thumb on the stick, not on its ring)
            if (a.x < sx1 && a.y > sy0) { if (a.y > B - 30) a.x = sx1; else a.y = sy0; }
          }
          // (M5 fixer, review: "the arrow and its distance overprint the quest line on the phone") the touch cluster's
          // slide can push the arrow back up the right edge into the column: then it goes left of the column instead,
          // with room for the distance behind it
          if (a.x > colLeft - 4 && a.y < colBottom) a.x = colLeft - 4;
          // (M8) and off the party's portraits down the left edge (the distance sits behind it: room for it too)
          if (partyColR_ > 0 && a.x < partyColR_ + 30 && a.y < partyColB_ + 6 && a.y > T + 40) a.x = partyColR_ + 30;
          // (M10) and off the sail panel (its distance label too): above it when it sits low, left of it when high
          {
            float sx0, sy0, sx1, sy1;
            if (seaHudRect(*this, sx0, sy0, sx1, sy1) && a.x > sx0 - 30 && a.x < sx1 + 30 && a.y > sy0 - 14 && a.y < sy1 + 14) {
              if (sy1 > B - 50) a.y = sy0 - 14;
              else if (sx0 < (L + R) / 2) a.x = sx1 + 30;
              else a.x = sx0 - 30;
            }
          }
          // a filled arrowhead (dark outline first so it reads on snow and sand), gently pulsing toward the goal
          a = a + d * (std::sin(t_ * 6) * 1.5f);
          Vec2 side(-d.y, d.x);
          // a bold ">" chevron: two 2px arms meeting at the tip
          for (int pass = 0; pass < 2; pass++) {
            Color cc = pass == 0 ? Color(0.08f, 0.05f, 0.03f, 0.85f) : kGold;
            float grow = pass == 0 ? 1.0f : 0.0f;
            for (float t = 0; t <= 6.0f; t += 0.5f)
              for (int arm = -1; arm <= 1; arm += 2) {
                Vec2 q2 = a - d * t + side * (t * (float)arm);
                P.rect(std::floor(q2.x) - grow, std::floor(q2.y) - grow, 2 + grow * 2, 2 + grow * 2, cc);
              }
          }
          // the distance sits behind the arrowhead (further back on diagonals, where the label is widest)
          float back = 15 + 8 * std::fabs(d.x);
          const std::string dtx = distText(dist);
          const float lx = a.x - d.x * back, ly = a.y - d.y * back - 3;
          P.textS(lx, ly, dtx, 1, kGold, 1);
          // (M3c fixer round 3) a small quest diamond before the number says what it counts (tiles to the tracked quest)
          {
            const float dx0 = lx - (float)P.textW(dtx, 1) / 2.0f - 6, dy0 = ly + 3;
            for (int pass = 0; pass < 2; pass++)
              for (int j = -2; j <= 2; j++) {
                const int hw = 2 - std::abs(j) + (pass == 0 ? 1 : 0);
                P.rect(std::floor(dx0) - hw, std::floor(dy0) + j, hw * 2 + 1, 1, pass == 0 ? Color(0.08f, 0.05f, 0.03f, 0.85f) : kGold);
              }
          }
        } else if (!(q->state == QState::Complete && [&] {
                     // the giver already wears the world "!" bubble (drawMarkers): don't stack a second pin on it
                     for (const Actor& a2 : g.actors)
                       if (a2.npc && a2.st != AState::Dead && len2(a2.p - tgt) < 48.0f * 48.0f && g.rewardWaiting(a2)) return true;
                     return false;
                   }())) {
          // an outlined quest pin in the "!" bubble's style (render_markers.cpp): a gold diamond with a dark outline
          // and a tail pointing down. A door target (a building) carries it above the roof line, not on the facade.
          float pinY = sc.y - 26 * kw;   // tail tip
          const Map& om = g.map();
          for (int bi = 0; bi < (int)om.bldgs.size(); bi++) {
            const Bldg& b = om.bldgs[bi];
            if (b.doorX() != tx || b.doorY() != ty) continue;
            const uint64_t k = sprites_.bldgKey(om, b, bi);
            auto bt = sprites_.bldgTex_.find(k);
            auto tr = sprites_.bldgTopRow_.find(k);
            if (bt != sprites_.bldgTex_.end() && tr != sprites_.bldgTopRow_.end()) {
              const float top = (b.r.y + b.r.h) * 16.0f + art::BLDG_PAD_B - bt->second.h + tr->second;
              pinY = std::min(pinY, toUi(Vec2(0, top)).y - 3);
            }
            break;
          }
          // never into the HUD: above the screen top or behind the vitals block, it sits just over the door instead
          if (pinY - 11 < T + 6 || (sc.x < L + 160 && pinY - 11 < T + 46)) pinY = sc.y - 22 * kw;
          // (fixer M4 r1, review: "the quest diamond draws over the arrow counter") still under the HUD's top-left block
          // (vitals, gold, arrows) or the top edge: the pin is not drawn (the target is right there on screen)
          const bool underHud = pinY - 12 < T + 4 || (sc.x - 6 < L + 160 && pinY - 12 < T + 52) || (g.mode == Mode::Dialogue);
          if (!underHud) {
          const int bob = (int)std::lround(std::sin(t_ * 4) * 1.2f);
          static const char* kPin[12] = {
              "....ooo....", "...oHYYo...", "..oHYYYSo..", ".oHYYYYYSo.", "oHYYYWYYYSo", "oYYYWWWYYSo",
              ".oYYYWYYSo.", "..oYYYYSo..", "...oYSSo...", "....oSo....", "....oSo....", ".....o.....",
          };
          const Color outline(0.09f, 0.06f, 0.08f), hi(1.0f, 0.9f, 0.5f), gold(1.0f, 0.7f, 0.16f), goldShade(0.72f, 0.36f, 0.06f),
              core(1.0f, 0.98f, 0.9f);
          const int x0 = (int)std::floor(sc.x) - 5, y0 = (int)std::floor(pinY) - 11 + bob;
          for (int pass = 0; pass < 2; pass++)
            for (int r = 0; r < 12; r++)
              for (int c = 0; c < 11; c++) {
                char ch = kPin[r][c];
                if (ch == '.') continue;
                if (pass == 0) { P.rect((float)(x0 + c + 1), (float)(y0 + r + 1), 1, 1, Color(0, 0, 0, 0.35f)); continue; }
                P.rect((float)(x0 + c), (float)(y0 + r), 1, 1,
                       ch == 'o' ? outline : ch == 'H' ? hi : ch == 'S' ? goldShade : ch == 'W' ? core : gold);
              }
          }
        }
      }
    }
  }
}

// the bosses' bars over their heads (drawHud)
void View::hudBossBar(Game& g, const HudFrame& H) {
  Pix& P = H.P;
  const Actor& p = H.p;
  const float B = H.B;
  // boss bar
  for (const Actor& a : g.actors) {
    if (!a.boss || a.st == AState::Dead || !a.aggro) continue;
    if (a.rank == 5) continue;   // (M6 integration) a world boss (foes::Rank::WorldBoss) has its bar at the top (render.cpp)
    if (len2(a.p - p.p) > 260 * 260) continue;
    float w = 200, x = (Pix::W - w) / 2, y = B - 22;
    P.textS(Pix::W / 2, y - 10, a.name, 1, Color(1, 0.75f, 0.6f), 1);
    P.rect(x - 1, y - 1, w + 2, 6, Color(0, 0, 0, 0.85f));
    P.rect(x, y, w * clampf(a.hp / a.maxHp, 0, 1), 4, Color(0.75f, 0.12f, 0.1f));
    break;
  }
}

// the NPC's name and the interact prompt (drawHud)
void View::hudPrompt(Game& g, const HudFrame& H) {
  Pix& P = H.P;
  const float L = H.L, T = H.T;
  const float kw = H.kw;
  const art::RigMetrics& RM = H.RM;
  // NPC name / interact prompt
  tagOn_ = false;
  int it = g.mode == Mode::Play ? g.interactTarget() : -1;
  // (M8) the party's use comes first, as Game::partyInteract takes it: a grave of the fallen in reach (VISIT), a downed
  // companion (BIND WOUNDS instead of TALK; also mid-fight, when no one else may be talked to)
  int partyIdx = -1;
  const int partyUse = g.mode == Mode::Play ? partyUseHere(g, partyIdx) : 0;
  if (partyUse == 1) it = -1;
  if (partyUse == 2) {
    const int mi = partyIdx;
    it = -1;
    for (const Actor& a : g.actors)
      if (a.comp == mi + 1 && a.st != AState::Dead) { it = a.id; break; }
  }
  const std::string homeVerb = it < 0 && !partyUse ? useVerbHere(g) : std::string();   // (M7) the yard's use
  if (partyUse == 1 && partyIdx >= 0 && partyIdx < (int)g.party.fallen.size()) {
    // the grave: "E: VISIT" over the stone (its name shows by the stone itself, party_view.cpp)
    const party::Fallen& f = g.party.fallen[(size_t)partyIdx];
    Vec2 s = toUi(Vec2((f.gx - g.world.ox) * 16.0f + 8.0f, (f.gy - g.world.oy) * 16.0f - 24.0f));
    std::string lab = std::string(touchUI ? "TAP: " : "E: ") + "VISIT " + f.name + "'S GRAVE";
    if (lab.size() > 30) lab = std::string(touchUI ? "TAP: " : "E: ") + "VISIT THE GRAVE";
    const float w = (float)P.textW(lab, 1) + 8;
    if (touchUI) { float ty = s.y - 13; clearOfTouch(s.x, ty, w, 11); s.y = ty + 13; }
    P.rect(s.x - w / 2, s.y - 13, w, 11, Color(0.04f, 0.03f, 0.05f, 0.55f));
    P.textS(s.x, s.y - 11, lab, 1, kGold, 1);
    tagOn_ = true;
    tagX0_ = s.x - w / 2; tagX1_ = s.x + w / 2; tagY0_ = s.y - 13; tagY1_ = s.y - 2;
  } else if (it >= 0) {
    for (const Actor& a : g.actors)
      if (a.id == it) {
        // (fixer M8 r1, review: "a pup always at heel puts a permanent E: TALK prompt over the player's head") a
        // companion at heel is always the nearest person: their tag shows only while the player stands still (the
        // bonded animal's never; the touch button still says TALK, the portrait card opens their orders)
        if (a.comp && partyUse != 2 && a.comp <= g.party.members.size() && g.party.members[(size_t)a.comp - 1].state == party::State::With &&
            (!a.human || g.pl().st != AState::Idle || len2(g.pl().vel) > 4.0f))
          break;
        // (fixer M8 r2) while the wounds are being bound the ring over them says it all: no name or verb over it
        if (partyUse == 2 && partyIdx >= 0 && partyIdx < (int)g.party.members.size() && g.party.members[(size_t)partyIdx].reviveT > 0) break;
        Vec2 s = toUi(a.p);   // (P2a) the feet, projected; heights over them are the rig's, kw UI px per world px
        // a giver with a reward waiting wears the "!" bubble (drawMarkers) just above the head: the label goes over it
        // (fixer M6b r3) so do the "has a tale" scroll and the wayside "!" (drawMarkers): the label covered the scroll
        // of the very person the player had walked up to
        const bool wayside = a.role == Role::Hunter || a.role == Role::Fisher || a.role == Role::Herbalist || a.role == Role::Traveller;
        const bool marked = g.rewardWaiting(a) || (wayside && g.offersWork(a)) ||
                            (g.story.hasTale(a.id) && !(a.bubble != art::Bubble::None && a.bubbleT > 0));
        // (v1: 48 / 34 px over the feet: the marker line plus the "!" bubble, or the bubble line)
        const art::RigMetrics& AM = rigFor(a.look);
        float ly = marked ? s.y - (AM.markerY + 18) * kw : s.y - AM.bubbleY * kw;
        if (partyUse == 2) ly -= 16.0f * kw;   // (M8) over the wound mark and the binding ring (fixer M8 r2: clear of it)
        // keep the label off the hero: when the hero's head is where the label would go, lift it clear
        const Vec2 hs = toUi(g.pl().p);
        // (M5) village animals are petted, not talked to; (M7) the player's own animals say what the button does
        // (RIDE <NAME> for the waiting horse, GROOM with a brush): home::interactLabel shares homeInteract's decision
        std::string critterUse = a.critter ? useVerbHere(g) : std::string();
        std::string verb = a.critter ? "E: " + (critterUse.empty() ? std::string("PET") : critterUse) : std::string("E: TALK");
        if (partyUse == 2) verb = "E: BIND WOUNDS";   // (M8) a downed companion
        const float labelW = (float)std::max(P.textW(a.name, 1), touchUI ? 0 : P.textW(verb, 1)) * 0.5f + 6;
        const float labelTop = ly - (touchUI ? 2.0f : 11.0f);
        if (std::fabs(hs.x - s.x) < labelW + 6 && hs.y - RM.markerY * kw < ly + 8 && hs.y > labelTop - 4)
          ly = std::min(ly, hs.y - RM.markerY * kw - 8.0f);
        // and off the HUD's top-left block (vitals, gold, arrows, spell name): an NPC near the top of a room had its
        // name run into the spell text ("FIREBOLTULONMAR"). There the label goes under the NPC's feet instead (the
        // floor below a person at the top of a room is open; the "!" bubble stays over the head).
        float lx = s.x;
        {
          const float hudR = std::max(L + 130, g_quickBarR) + 12;
          const float hudB = T + ((g.perkPts > 0 || g.blessT > 0) ? (g.perkPts > 0 && g.blessT > 0 ? 64.0f : 54.0f) : 46.0f) + (g.story.burdened() ? 12.0f : 0.0f);
          const float half = labelW - 6;
          const float topY = ly - (touchUI ? 0.0f : 9.0f);
          if (lx - half < hudR && topY < hudB) ly = s.y + (touchUI ? 4.0f : 4.0f * kw + 9.0f);
          // (fixer M8 r3, review: "the HUNGRY hint box overlaps E: TALK / A GUARD") clear of the toast column on the
          // left, as the yard's and the props' prompts keep (M7 fix): the label steps right of the notices
          const float ly0 = ly - (touchUI ? 2.0f : 11.0f), ly1 = ly + 9.0f;
          if (toastX1_ > 0 && lx - labelW < toastX1_ + 4 && ly0 < toastY1_ + 2 && ly1 > toastY0_ - 2) lx = toastX1_ + 4 + labelW;
        }
        // (P1 fixer) the centre banner ("QUEST COMPLETE / STOP, THIEF!") is drawn after the tag, its plate over the
        // label ("STOP, TALK THIEF!"): while it shows, a label in its band waits for it to fade (moved under it, it
        // landed on the "+16 GOLD" rising off the hero)
        if (bannerT_ > 0 && g.mode != Mode::Dialogue) {
          const float b0 = T + 50.0f, b1 = T + 56.0f + (bannerState_.empty() ? 30.0f : 42.0f);
          const float l0 = ly - (touchUI ? 2.0f : 11.0f), l1 = ly + 9.0f;
          if (l1 > b0 - 2 && l0 < b1 + 2) break;
        }
        // (M7 fix) "E: RIDE DUSTY" already names her: one label, not the verb stacked over the name
        const bool verbNames = !touchUI && !a.name.empty() && verb.size() > a.name.size() && verb.compare(verb.size() - a.name.size(), a.name.size(), a.name) == 0;
        if (verbNames) P.textS(lx, ly, verb, 1, kGold, 1);
        else {
          P.textS(lx, ly, a.name, 1, kText, 1);
          if (!touchUI) P.textS(lx, ly - 9, verb, 1, kGold, 1);
        }
        tagOn_ = true;
        tagX0_ = lx - labelW; tagX1_ = lx + labelW;
        tagY0_ = ly - (touchUI ? 2.0f : 11.0f); tagY1_ = ly + 9.0f;
      }
  } else if (g.mode == Mode::Play && !homeVerb.empty()) {
    // (M7) the player's own yard: exactly what the use button will do on the tile ahead ("E: PLANT BARLEY"), over it
    const Actor& hp = g.pl();
    const Vec2 probe = hp.p + hp.aim * 12.0f + Vec2(0, -4);
    const float px = std::floor(probe.x / TILE) * TILE + 8.0f, py = std::floor(probe.y / TILE) * TILE;
    Vec2 s = toUi(Vec2(px, py - 4.0f));
    const Vec2 hs = toUi(hp.p);
    if (std::fabs(hs.x - s.x) < 30 * kw && s.y - 10 > hs.y - (RM.markerY + 10) * kw && s.y - 10 < hs.y + 4 * kw) s.y = hs.y - RM.markerY * kw;   // off the hero's head
    // (fixer M10 r1) a held verb ("HOLD: REEL") says HOLD, not TAP
    const bool held = homeVerb.rfind("HOLD: ", 0) == 0;
    const std::string lab = held ? std::string(touchUI ? "HOLD: " : "HOLD E: ") + homeVerb.substr(6)
                                 : std::string(touchUI ? "TAP: " : "E: ") + homeVerb;
    const float w = (float)P.textW(lab, 1) + 8;
    // (M7 fix) clear of the toast column on the left (a need's notice ran into the prompt: "...OR AE: LOOK AT")
    if (toastX1_ > 0 && s.x - w / 2 < toastX1_ + 4 && s.y - 13 < toastY1_ + 2 && s.y - 2 > toastY0_ - 2) s.x = toastX1_ + 4 + w / 2;
    if (touchUI) { float ty = s.y - 13; clearOfTouch(s.x, ty, w, 11); s.y = ty + 13; }   // (M7 fixer r2)
    P.rect(s.x - w / 2, s.y - 13, w, 11, Color(0.04f, 0.03f, 0.05f, 0.55f));
    P.textS(s.x, s.y - 11, lab, 1, kGold, 1);
    if (!touchUI && !home::interactLabel(g).empty()) {   // the tile it acts on: four corner ticks (the yard's; the sea's verbs act on the boat)
      const Vec2 tl = toUi(Vec2(px - 8, py));   // (P2a) the tile's corners, projected: the ticks frame it at any zoom
      const float tx = std::floor(tl.x), ty = std::floor(tl.y);
      const Color tc(kGold.r, kGold.g, kGold.b, 0.75f);
      for (int k = 0; k < 4; k++) {
        const float cx = tx + ((k & 1) ? std::floor(16.0f * kw) - 3.0f : 0.0f), cy = ty + ((k & 2) ? std::floor(16.0f * kw) - 1.0f : 0.0f);
        P.rect(cx, cy, 3, 1, tc);
        P.rect(cx + ((k & 1) ? 2.0f : 0.0f), cy + ((k & 2) ? -2.0f : 0.0f), 1, 3, tc);
      }
    }
  } else if (g.mode == Mode::Play) {
    int ptx = 0, pty = 0, pr = usablePropAt(g, ptx, pty);
    if (pr) {
      Vec2 s = toUi(Vec2(ptx * 16 + 8.0f, pty * 16 - 4.0f));
      const std::string lab = std::string(touchUI ? "TAP: " : "E: ") + useVerbAt(g, pr, ptx, pty);
      const float w = (float)P.textW(lab, 1);
      if (toastX1_ > 0 && s.x - w / 2 < toastX1_ + 4 && s.y - 11 < toastY1_ + 2 && s.y - 2 > toastY0_ - 2) s.x = toastX1_ + 4 + w / 2;   // (M7 fix)
      if (touchUI) { float ty = s.y - 11; clearOfTouch(s.x, ty, w + 4, 10); s.y = ty + 11; }   // (M7 fixer r2)
      P.textS(s.x, s.y - 10, lab, 1, kGold, 1);
    }
  }
}

// the notice line, the biome banner and the arrival banner; true while the companions' order ring is open (drawHud)
bool View::hudNotices(Game& g, const HudFrame& H) {
  Pix& P = H.P;
  const float L = H.L, T = H.T, B = H.B;
  // (toasts are drawn by drawToasts, above the dialogue panel)
  // (M2 fixer round 2) not under a dialogue (it showed through the panel as ghost text), on a backing band (a 1-px
  // drop shadow was lost over foliage and birch trunks), and clear of the quest pointer's distance at the bottom
  if (g.noticeT > 0 && g.mode != Mode::Dialogue) {
    float a = clampf(g.noticeT, 0, 1);
    // M0b fix round 2: with the touch buttons on, a notice too wide to clear the bow button (x >= 360) breaks into two
    // centred lines at the space nearest its middle
    std::string l1 = g.notice, l2;
    if (touchUI && P.textW(g.notice, 1) > 2 * (btnPos(B_BOW).x - btnPos(B_BOW).r - 6 - Pix::W / 2)) {
      size_t mid = g.notice.size() / 2, best = std::string::npos;
      for (size_t i = 0; i < g.notice.size(); i++)
        if (g.notice[i] == ' ' && (best == std::string::npos || (i > mid ? i - mid : mid - i) < (best > mid ? best - mid : mid - best))) best = i;
      if (best != std::string::npos) { l1 = g.notice.substr(0, best); l2 = g.notice.substr(best + 1); }
    }
    float ny = B - 64 - (l2.empty() ? 0.0f : 9.0f);
    {
      const float bw = (float)std::max(P.textW(l1, 1), l2.empty() ? 0 : P.textW(l2, 1)) + 16;
      const float bh = l2.empty() ? 13.0f : 22.0f;
      P.rect(Pix::W / 2 - bw / 2, ny - 3, bw, bh, Color(0.04f, 0.03f, 0.05f, 0.62f * a));
      P.rect(Pix::W / 2 - bw / 2 + 4, ny - 3, bw - 8, 1, Color(kGold.r, kGold.g, kGold.b, 0.35f * a));
      P.rect(Pix::W / 2 - bw / 2 + 4, ny - 4 + bh, bw - 8, 1, Color(kGold.r, kGold.g, kGold.b, 0.35f * a));
    }
    for (const std::string* L : {&l1, &l2}) {
      if (L->empty()) continue;
      P.textS(Pix::W / 2 + 1, ny + 1, *L, 1, Color(0, 0, 0, a * 0.6f), 1);
      P.textS(Pix::W / 2, ny, *L, 1, Color(1, 0.95f, 0.85f, a), 1);
      ny += 9;
    }
  }
  // (M3c LIFE) the first visit to a biome: its name, quietly, under the location column's height at the top centre
  // (small type between two short gold rules; it fades in and out and waits for a big banner to finish)
  // (fixer M8 r3, review: "radial labels run into the region banner: WAIT LLAGE") the title cards, the herald and the
  // toasts step away while the companions' order ring is open (their clocks run on; the ring is a moment's choice)
  const bool ringOpen = radial_.open && g.mode == Mode::Play;
  if (!ringOpen && biomeBannerT_ > 0 && !biomeBanner_.empty() && (bannerT_ <= 0 || g.mode == Mode::Dialogue) && herald_.t <= 0 && g.mode == Mode::Play && !g.inside) {
    const float a = clampf(std::min(biomeBannerT_, 4.0f - biomeBannerT_) * 1.6f, 0, 1);
    const float tw = (float)P.textW(biomeBanner_, 1);
    const float half = std::min(tw / 2 + 26, Pix::W / 2.0f - 70);
    const float y = biomeBannerY(half);   // (M6 fixer) clear of a world boss's bar; (M9 fixer) and of the quick bar
    P.rect(Pix::W / 2 - half, y - 4, half * 2, 15, Color(0.03f, 0.03f, 0.05f, 0.42f * a));
    P.rect(Pix::W / 2 - half + 6, y + 3, 12, 1, Color(kGold.r, kGold.g, kGold.b, 0.7f * a));
    P.rect(Pix::W / 2 + half - 18, y + 3, 12, 1, Color(kGold.r, kGold.g, kGold.b, 0.7f * a));
    P.textS(Pix::W / 2 + 1, y + 1, biomeBanner_, 1, Color(0, 0, 0, a * 0.6f), 1);
    P.textS(Pix::W / 2, y, biomeBanner_, 1, Color(0.98f, 0.92f, 0.74f, a), 1);
  }
  // banner (not on the death screen: its words own the centre, and an arrival card under them read "YOU HAVE FALLARS")
  if (bannerT_ > 0 && g.mode != Mode::Dialogue && g.mode != Mode::Dead && !ringOpen) {
    const float a0 = clampf(std::min(bannerT_, 4.0f - bannerT_) * 2.5f, 0, 1);
    // (fixer M4 r3, review: "a ghost of the town name stays on the ground after the plate fades") the plate fades last:
    // the words go first (a squared), so no name ever floats over the street without its dark plate under it
    const float plateA = std::min(1.0f, a0 * 1.6f), a = a0 * a0;
    float y = T + 56;
    P.rect(0, y - 6, Pix::W, 34, Color(0, 0, 0, 0.45f * plateA));
    P.rect(Pix::W / 2 - 90, y - 6, 180, 1, Color(kGold.r, kGold.g, kGold.b, a * 0.8f));
    P.rect(Pix::W / 2 - 90, y + 27, 180, 1, Color(kGold.r, kGold.g, kGold.b, a * 0.8f));
    P.textS(Pix::W / 2, y, banner_, 1, Color(kGold.r, kGold.g, kGold.b, a), 1);
    P.textS(Pix::W / 2 + 1, y + 11, bannerSub_, 2, Color(0, 0, 0, a * 0.5f), 1);
    P.textS(Pix::W / 2, y + 10, bannerSub_, 2, Color(1, 0.97f, 0.9f, a), 1);
    if (!bannerState_.empty()) {   // (M4) the settlement's state under its name, in its colour
      const Color sc = col(bannerStateCol_ ? bannerStateCol_ : rgba(236, 120, 90));
      P.rect(0, y + 28, Pix::W, 11, Color(0, 0, 0, 0.45f * plateA));
      P.textS(Pix::W / 2, y + 30, bannerState_, 1, Color(sc.r, sc.g, sc.b, a), 1);
    }
  }
  return ringOpen;
}

// Toasts (items received, quest steps, notices): a column on the left under the vitals, clear of the thumbs (the
// touch stick and buttons sit at the bottom), the centre notice and the quest column; newest at the top.
// (M2) a toast never runs under the minimap or the quest column (the right 160 px of the safe area): a long one wraps
// onto a second line, and the column stops above the touch stick.
void View::drawToasts() {
  Pix& P = *pix_;
  // (M8) with a party the column of portraits stands at the left edge under the vitals: the toasts step right of it
  // (fixer M8 r1, review: "WEARY: FIND A BED reads as the hound's label") a clear gap from the cards and a firmer plate
  const float L = partyColR_ > 0 ? std::max((float)Pix::SL, partyColR_ + 6) : (float)Pix::SL, R = (float)(Pix::W - Pix::SR), B = (float)(Pix::H - Pix::SB);
  // (M8b fixer r3, review: "IRON MAIL floats at the left edge with no backing panel") a firm plate and a rule for every
  // toast, so a find's name reads as a notice on its strip, not a stray label over a dark floor
  const float plateA = partyColR_ > 0 ? 0.7f : 0.66f;
  // (M6 fixer r4, review: "event text is drawn right over the world boss") at most 30 characters a line, so a long
  // notice wraps in the left column instead of running across the middle of the screen, where the hero and his foe stand
  const int per = std::clamp((int)((R - 166 - (L + 6)) / 6), 16, 30);
  const float bottom = B - (touchUI ? 86.0f : 30.0f);
  float ty = (float)Pix::ST + 92;
  // (fixer M4 r2) the full-width banner (T+50 .. T+84, its state line to T+95) is never written over: the column
  // starts under it while it shows
  if (bannerT_ > 0) ty = std::max(ty, (float)Pix::ST + 56 + (bannerState_.empty() ? 30.0f : 42.0f));
  int shown = 0;
  toastX1_ = 0; toastY0_ = ty - 2; toastY1_ = ty - 2;
  for (int i = (int)toasts_.size() - 1; i >= 0 && shown < 5; i--, shown++) {
    const Toast& t = toasts_[i];
    float a = clampf(4.0f - t.t, 0, 1);
    auto lines = wrap(t.s, per);
    if (ty + lines.size() * 10.0f > bottom) break;
    // (M9) a skill toast (Toast::rarity <= -100): a star in the skill's colour before the words, a glow on a level / mastery
    const bool skillT = SkillsUiOps::isSkillToast(t);
    const float ind = skillT ? 8.0f : 0.0f;
    for (size_t k = 0; k < lines.size(); k++) {
      const std::string& s = lines[k];
      float w = (float)P.textW(s, 1) + ind;
      P.rect(L + 3, ty - 2, w + 6, k + 1 < lines.size() ? 10.0f : 10.0f, Color(0.03f, 0.02f, 0.05f, plateA * a));
      P.rect(L + 3, ty - 2, 1, 10.0f, Color(t.c.r * 0.7f, t.c.g * 0.7f, t.c.b * 0.7f, 0.8f * a));   // (its own rule: no card's)
      P.rect(L + 4, ty + 8, w + 5, 1, Color(0, 0, 0, 0.35f * a));   // a hairline under the plate: it sits on the screen
      if (a > 0.05f) { toastX1_ = std::max(toastX1_, L + 9 + w); toastY1_ = ty + 8; }
      if (skillT) {
        if (k == 0) SkillsUiOps::toastDecor(*this, t, L + 6 + ind, ty, w - ind, a);
        P.textS(L + 6 + ind, ty, s, 1, Color(t.c.r, t.c.g, t.c.b, a));
        ty += k + 1 < lines.size() ? 10.0f : 11.0f;
        continue;
      }
      P.textS(L + 7, ty + 1, s, 1, Color(0, 0, 0, a * 0.6f));
      if (t.rarity > 0) {   // (M8b) a find: its name in the ladder colour (a Mythic's in the flame), a rule in it
        P.rect(L + 3, ty - 2, 1, 10.0f, Color(t.c.r, t.c.g, t.c.b, 0.9f * a));
        rarityText(L + 6, ty, s, (Rarity)t.rarity, a, 0, true);
      } else {
        P.textS(L + 6, ty, s, 1, Color(t.c.r, t.c.g, t.c.b, a));
      }
      ty += k + 1 < lines.size() ? 10.0f : 11.0f;
    }
  }
}

void View::drawTouch(Game& g) {
  Pix& P = *pix_;
  // joystick
  if (stick_.on) {
    Vec2 d = stick_.cur - stick_.start;
    float l = len(d);
    if (l > 26) d = d * (26 / l);
    for (int r = 26; r > 0; r -= 2) P.rect(stick_.start.x - r, stick_.start.y - 1, r * 2, 2, Color(1, 1, 1, 0.0f));
    P.blitEx(sprites_.light_, 0, 0, 64, 64, stick_.start.x - 30, stick_.start.y - 30, 60, 60, false, Color(0.9f, 0.9f, 1, 0.25f));
    P.blitEx(sprites_.light_, 0, 0, 64, 64, stick_.start.x + d.x - 14, stick_.start.y + d.y - 14, 28, 28, false, Color(1, 1, 1, 0.6f));
  } else {
    // (M1 round 3) the stick at rest is drawn like the buttons (a dark disc in a ring, with its thumb and four
    // direction ticks), so a new phone player sees where to drag: the old faint glow vanished on grass and cobbles
    const Vec2 rest = stickRest();
    const float R = 26, r2 = 11;
    for (int yy = (int)-R; yy <= (int)R; yy++) {
      const float hw = std::sqrt(std::max(0.0f, R * R - yy * yy));
      P.rect(rest.x - hw, rest.y + yy, hw * 2, 1, Color(0.08f, 0.07f, 0.09f, 0.32f));
    }
    for (int k = 0; k < 64; k++) {
      const float a = k / 64.0f * TAU;
      P.rect(rest.x + std::cos(a) * R, rest.y + std::sin(a) * R, 1, 1, Color(0.9f, 0.85f, 0.75f, 0.5f));
    }
    for (int k = 0; k < 4; k++) {   // direction ticks
      const float a = k * TAU / 4;
      const float cx = rest.x + std::cos(a) * (R - 6), cy = rest.y + std::sin(a) * (R - 6);
      P.rect(cx - 1, cy - 1, 2, 2, Color(0.95f, 0.9f, 0.8f, 0.45f));
    }
    for (int yy = (int)-r2; yy <= (int)r2; yy++) {
      const float hw = std::sqrt(std::max(0.0f, r2 * r2 - yy * yy));
      P.rect(rest.x - hw, rest.y + yy, hw * 2, 1, Color(0.85f, 0.82f, 0.78f, 0.38f));
    }
    for (int k = 0; k < 32; k++) {
      const float a = k / 32.0f * TAU;
      P.rect(rest.x + std::cos(a) * r2, rest.y + std::sin(a) * r2, 1, 1, Color(0.12f, 0.1f, 0.14f, 0.6f));
    }
  }
  int target = g.interactTarget();
  const std::string homeUse = target < 0 ? useVerbHere(g) : std::string();   // (M7) the yard's use
  int utx = 0, uty = 0, usePr = target < 0 && homeUse.empty() ? usablePropAt(g, utx, uty) : 0;
  for (int b = 0; b < B_MENU; b++) {
    if (b == B_SPELL) continue;   // (M9) the four quick-cast buttons replace it (SkillsUiOps::drawQuickButtons)
    const BtnDef d = btnPos(b);
    bool held = false;
    for (auto& f : fingers_) if (f.on && f.button == b) held = true;
    Color ring = held ? Color(1, 0.9f, 0.6f, 0.9f) : Color(0.9f, 0.85f, 0.75f, 0.45f);
    P.blitEx(sprites_.light_, 0, 0, 64, 64, d.x - d.r * 1.3f, d.y - d.r * 1.3f, d.r * 2.6f, d.r * 2.6f, false, Color(0.05f, 0.04f, 0.06f, 0.0f));
    // disc
    for (int yy = (int)-d.r; yy <= (int)d.r; yy++) {
      float hw = std::sqrt(std::max(0.0f, d.r * d.r - yy * yy));
      P.rect(d.x - hw, d.y + yy, hw * 2, 1, Color(0.08f, 0.07f, 0.09f, held ? 0.65f : 0.42f));
    }
    for (int k = 0; k < 40; k++) { float a = k / 40.0f * TAU; P.rect(d.x + std::cos(a) * d.r, d.y + std::sin(a) * d.r, 1, 1, ring); }
    switch (b) {
      case B_ATTACK:
        if (int pidx = -1, pu = partyUseHere(g, pidx); pu) {   // (M8) BIND a downed companion, VISIT a grave
          P.text(d.x, d.y - 3, pu == 2 ? "BIND" : "VISIT", 1, kGold, 1);
        }
        else if (target >= 0) {
          const Actor* ta = nullptr;
          for (const Actor& x : g.actors) if (x.id == target) { ta = &x; break; }
          std::string tv = "TALK";
          if (ta && ta->critter) {   // (M7) RIDE / GROOM for the player's own animals, else PET
            tv = useVerbHere(g);
            if (tv.empty()) tv = "PET";
            const size_t sp = tv.find(' ');
            if (sp != std::string::npos && P.textW(tv, 1) > (int)(d.r * 2 - 4)) tv = tv.substr(0, sp);
          }
          P.text(d.x, d.y - 3, tv, 1, kGold, 1);
        }
        else if (!homeUse.empty()) {   // (M7) PLANT / WATER / HARVEST / TILL: the verb, its crop under it when it fits
          const size_t sp = homeUse.find(' ');
          if (sp == std::string::npos || P.textW(homeUse, 1) <= (int)(d.r * 2 - 4)) P.text(d.x, d.y - 3, homeUse, 1, kGold, 1);
          else {
            P.text(d.x, d.y - 7, homeUse.substr(0, sp), 1, kGold, 1);
            P.text(d.x, d.y + 2, homeUse.substr(sp + 1, (size_t)std::max(1.0f, (d.r * 2 - 4) / 6)), 1, kText, 1);
          }
        }
        else if (usePr) P.text(d.x, d.y - 3, useVerbAt(g, usePr, utx, uty), 1, kGold, 1);
        else if (g.eqWeapon >= 0) P.blitEx(sprites_.itemTex(*this, g, g.inv[g.eqWeapon]), 0, 0, 16, 16, d.x - 12, d.y - 12, 24, 24);
        else {
          // unarmed (the shirt-only start): a clenched fist, knuckles up, lit from the top-left
          static const char* kFist[] = {
              "..####.....",
              ".#aab#####.",
              "#aabb#aab#.",
              "#abbb#abb##",
              "#abbb#abbc#",
              "#abbbbbbbc#",
              "#bbbbbbbcc#",
              ".#bbbbbbc#.",
              "..#bbbbcc#.",
              "...#bbbc#..",
              "...#cccc#..",
              "....####...",
          };
          const Color ink(0.12f, 0.07f, 0.08f), la(1.0f, 0.86f, 0.70f), mb(0.88f, 0.66f, 0.50f), dc(0.62f, 0.40f, 0.33f);
          for (int yy = 0; yy < 12; yy++)
            for (int xx = 0; xx < 11; xx++) {
              char ch = kFist[yy][xx];
              if (ch == '.') continue;
              Color c = ch == '#' ? ink : ch == 'a' ? la : ch == 'b' ? mb : dc;
              P.rect(d.x - 11 + xx * 2, d.y - 12 + yy * 2, 2, 2, c);
            }
        }
        break;
      case B_BOW: P.blit(sprites_.iconTex(*this, art::Icon::Bow, 0), d.x - 8, d.y - 8); break;
      case B_ROLL: P.text(d.x, d.y - 3, "ROLL", 1, kText, 1); break;
      case B_POTION: {
        P.blit(sprites_.iconTex(*this, art::Icon::PotionRed, 0), d.x - 8, d.y - 8);
        // (M6, 7.5 rule 7) the shared cooldown as a clockwise sweep from the top, the seconds left over it
        const float cd = g.craft.live.potionCd;
        if (cd > 0) {
          const float k = clampf(cd / std::max(0.1f, g.craft.live.potionCdMax), 0, 1);
          const int R = (int)d.r - 1;
          for (int yy = -R; yy <= R; yy++)
            for (int xx = -R; xx <= R; xx++) {
              if (xx * xx + yy * yy > R * R) continue;
              float a = std::atan2((float)xx + 0.5f, -(float)yy - 0.5f);
              if (a < 0) a += TAU;
              if (a / TAU < 1.0f - k) continue;
              P.rect(d.x + xx, d.y + yy, 1, 1, Color(0.02f, 0.02f, 0.05f, 0.62f));
            }
          P.textS(d.x, d.y - 3, std::to_string((int)std::ceil(cd)), 1, kText, 1);
        } else {
          int pots = 0;
          for (const Item& it : g.inv) if (it.kind == ItemKind::Potion && it.sub == (uint8_t)PotionType::Health) pots += it.count;
          if (pots > 0) P.textS(d.x + d.r - 2, d.y + d.r - 7, std::to_string(pots), 1, kText, 1);
        }
        break;
      }
      default: break;
    }
  }
  SkillsUiOps::drawQuickButtons(*this, g);   // (M9) the quick-cast arc and the sneak toggle (skills_hud.cpp)
}

void View::drawMinimap(Game& g, float x, float y, int size) {
  Pix& P = *pix_;
  miniT_ -= 0.016f;
  const Map& m = g.map();
  const Actor& p = g.pl();
  int ptx = (int)(p.p.x / 16), pty = (int)(p.p.y / 16);
  // window origin: centred on the player outdoors; inside, clamped to the map so a dungeon fills the box
  int ox = ptx - 32, oy = pty - 32;
  if (g.inside) {
    ox = m.w <= 64 ? (m.w - 64) / 2 : std::clamp(ox, 0, m.w - 64);
    oy = m.h <= 64 ? (m.h - 64) / 2 : std::clamp(oy, 0, m.h - 64);
  }
  if (miniT_ <= 0) {
    miniT_ = 0.2f;
    for (int yy = 0; yy < 64; yy++)
      for (int xx = 0; xx < 64; xx++) {
        int tx = ox + xx, ty = oy + yy;
        uint32_t k = rgba(10, 10, 14);
        if (m.in(tx, ty)) {
          Ground gr = m.at(tx, ty);
          switch (gr) {
            case Ground::DeepWater: k = rgba(30, 56, 104); break;
            case Ground::Water: k = rgba(52, 96, 150); break;
            case Ground::Sand: k = rgba(206, 188, 130); break;
            case Ground::Snow: k = rgba(220, 228, 236); break;
            case Ground::Rock: k = rgba(104, 98, 94); break;
            case Ground::CaveWall: case Ground::InteriorWall: k = rgba(30, 26, 26); break;
            case Ground::CaveFloor: k = rgba(110, 98, 88); break;
            case Ground::Road: case Ground::Bridge: case Ground::Plaza: k = rgba(176, 156, 118); break;
            case Ground::StoneFloor: k = rgba(130, 128, 136); break;
            case Ground::WoodFloor: k = rgba(140, 100, 66); break;
            case Ground::Dirt: case Ground::Farmland: k = rgba(134, 104, 70); break;
            case Ground::ForestFloor: k = rgba(56, 100, 52); break;
            case Ground::Autumn: k = rgba(160, 104, 52); break;
            case Ground::Tundra: k = rgba(104, 122, 96); break;
            case Ground::Swamp: k = rgba(70, 86, 56); break;
            default: k = rgba(86, 150, 66); break;
          }
          // (M3c LIFE) the natural ground takes its biome's colour (biomes.h ecoInfo rgb): the savanna gold, the heath
          // mauve, the badlands rust, the blight sick violet; roads, water, rock and buildings keep theirs
          if (m.kind == MapKind::Overworld && !m.eco.empty() &&
              (gr == Ground::Grass || gr == Ground::Meadow || gr == Ground::ForestFloor || gr == Ground::Tundra || gr == Ground::Autumn ||
               gr == Ground::Swamp || gr == Ground::Sand || gr == Ground::Snow)) {
            const Eco te = m.ecoAt(tx, ty);
            const EcoInfo& ei = ecoInfo(te);
            k = art::mix(k, rgba(ei.r, ei.g, ei.b), 0.7f);
            // (M4) a flower meadow is flecked with blooms (it read as the plain meadow's green)
            if (te == Eco::FlowerMeadow && gr != Ground::Sand && gr != Ground::Snow) {
              const uint32_t fh = hash32((uint32_t)(tx + g.world.ox) * 73856093u ^ (uint32_t)(ty + g.world.oy) * 19349663u);
              if (fh % 5u == 0) k = (fh >> 8) % 2u ? rgba(236, 150, 172) : rgba(244, 216, 100);
            }
          }
          // (M2 fixer) the view's snowline (terrain.cpp reliefPixel): taiga and snowfields from level 4, mountains from
          // 5 lie under snow, so the minimap shows them white as the world does
          if (m.kind == MapKind::Overworld && !m.biome.empty() &&
              (gr == Ground::Grass || gr == Ground::Meadow || gr == Ground::ForestFloor || gr == Ground::Tundra || gr == Ground::Autumn)) {
            const Biome bb = m.biomeAt(tx, ty);
            const int lv = m.heightAt(tx, ty);
            if (((bb == Biome::Snow || bb == Biome::Taiga) && lv >= 4) || (bb == Biome::Mountain && lv >= 5)) k = rgba(214, 222, 232);
          }
          size_t i = (size_t)ty * m.w + tx;
          if (m.bldgAt[i] >= 0) k = rgba(150, 70, 56);
          else if (m.wall[i]) k = rgba(70, 66, 66);
          else if (m.solid[i]) k = art::shade(k, 0.7f);
        }
        miniPx_[(size_t)yy * 64 + xx] = k;
      }
    // (M11 CLIMB) the relief (VISION_PLAN 11.3, 11.5): higher ground a little paler (the aerial grade), a cliff a dark
    // line (a stacked face darker still), a hillside softly hatched, a ramp or stone stairs a pale tongue up it, a
    // waterfall white water; a crest whose south or east falls away catches the north-west light
    if (m.kind == MapKind::Overworld && !m.height.empty())
      for (int yy = 0; yy < 64; yy++)
        for (int xx = 0; xx < 64; xx++) {
          const int tx = ox + xx, ty = oy + yy;
          if (!m.in(tx, ty)) continue;
          const size_t i = (size_t)ty * m.w + tx;
          const uint8_t b = m.height[i];
          uint32_t& k = miniPx_[(size_t)yy * 64 + xx];
          if (b & Map::HEIGHT_FALL) {   // white water, and the pool round its foot churned pale
            k = rgba(214, 238, 252);
            static const int fx4[4] = {1, -1, 0, 0}, fy4[4] = {0, 0, 1, -1};
            for (int q = 0; q < 4; q++) {
              const int qx = xx + fx4[q], qy = yy + fy4[q];
              if (qx < 0 || qy < 0 || qx >= 64 || qy >= 64 || !m.in(tx + fx4[q], ty + fy4[q])) continue;
              if (groundWater(m.at(tx + fx4[q], ty + fy4[q])) && !m.fallAt(tx + fx4[q], ty + fy4[q]))
                miniPx_[(size_t)qy * 64 + qx] = art::mix(miniPx_[(size_t)qy * 64 + qx], rgba(206, 228, 244), 0.6f);
            }
            continue;
          }
          if (groundWater(m.at(tx, ty)) || m.bldgAt[i] >= 0 || m.wall[i]) continue;
          const int lv = b & Map::HEIGHT_LEVEL;
          k = art::mix(k, rgba(232, 228, 214), (float)lv * 0.035f);
          if (b & Map::HEIGHT_CLIFF) {
            const int rise = std::max({m.heightAt(tx, ty - 1), m.heightAt(tx + 1, ty), m.heightAt(tx - 1, ty), m.heightAt(tx, ty + 1)}) - lv;
            k = art::mix(art::shade(k, rise >= 2 ? 0.40f : 0.56f), rgba(60, 46, 40), rise >= 2 ? 0.35f : 0.18f);
          } else if (b & Map::HEIGHT_SLOPE)
            k = art::shade(k, ((tx + ty) & 1) ? 0.84f : 0.94f);
          else if (b & Map::HEIGHT_RAMP)
            k = art::mix(k, (b & Map::HEIGHT_STAIR) ? rgba(214, 204, 182) : rgba(200, 174, 128), 0.5f);
          else if (m.heightAt(tx, ty + 1) < lv || m.heightAt(tx + 1, ty) < lv)
            k = art::mix(k, rgba(250, 244, 226), 0.22f);
        }
    // (M2 fixer round 2) the mountains: a peak or a massif shows as a little dark mountain over its footprint (a lit
    // west flank, a shaded east one), so a summit the map names is on the minimap too
    if (m.kind == MapKind::Overworld)
      for (int yy = 0; yy < 64 + 3; yy++)
        for (int xx = -4; xx < 64 + 4; xx++) {
          const int tx = ox + xx, ty = oy + yy;
          if (!m.in(tx, ty)) continue;
          const int pp = m.propAt(tx, ty);
          if (pp != (int)art::Prop::Peak + 1 && pp != (int)art::Prop::GreatPeak + 1) continue;
          int fw = 1, fh = 1;
          art::wildFootprint((art::Prop)(pp - 1), fw, fh);
          const int hh = fh + 1;   // a row taller than the footprint: the summit stands above its foot
          for (int j = 0; j < hh; j++) {
            const int half = std::max(0, (fw / 2) * (j + 1) / hh);
            for (int i = -half; i <= half; i++) {
              const int qx = xx + i, qy = yy - hh + 1 + j;
              if (qx < 0 || qy < 0 || qx >= 64 || qy >= 64) continue;
              miniPx_[(size_t)qy * 64 + qx] = j == 0 ? rgba(236, 240, 246) : i < 0 ? rgba(120, 114, 124) : rgba(78, 74, 90);
            }
          }
        }
    pix_->miniUpdate(miniPx_.data());
  }
  P.rect(x - 2, y - 2, size + 4, size + 4, Color(0.05f, 0.04f, 0.05f, 0.9f));
  P.frame(x - 2, y - 2, size + 4, size + 4, Color(0.55f, 0.45f, 0.28f));
  P.miniDraw(x, y, size / 64.0f);
  // hostiles + npcs
  for (const Actor& a : g.actors) {
    if (a.player || a.st == AState::Dead) continue;
    int ax = (int)(a.p.x / 16) - ox, ay = (int)(a.p.y / 16) - oy;
    if (ax < 0 || ay < 0 || ax >= 64 || ay >= 64) continue;
    P.rect(x + ax * size / 64.0f, y + ay * size / 64.0f, 1, 1, a.hostile ? Color(1, 0.25f, 0.2f) : Color(0.9f, 0.9f, 0.5f));
  }
  if (!g.inside)
    for (const Site& s : g.world.sites) {
      if (!s.discovered) continue;
      int ax = s.ex - ox, ay = s.ey - oy;
      if (ax < 1 || ay < 1 || ax >= 63 || ay >= 63) continue;
      Color c = s.type == SiteType::Cave || s.type == SiteType::Ruin ? Color(0.75f, 0.6f, 1) : s.type == SiteType::BanditCamp ? Color(1, 0.4f, 0.3f) : Color(1, 0.9f, 0.6f);
      P.rect(x + ax - 1, y + ay - 1, 3, 3, Color(0, 0, 0, 0.8f));
      P.rect(x + ax, y + ay, 1, 1, c);
    }
  // (M7) the player's property: its ground outlined in warm gold and a small roof glyph over its middle
  if (!g.inside && g.world.endless)
    for (const home::Plot& hp : g.home.plots) {
      const int px0 = hp.gx - g.world.ox - ox, py0 = hp.gy - g.world.oy - oy;
      if (px0 + hp.w < 0 || py0 + hp.h < 0 || px0 >= 64 || py0 >= 64) continue;
      const float k = size / 64.0f;
      const float rx0 = x + std::max(0, px0) * k, ry0 = y + std::max(0, py0) * k;
      const float rx1 = x + std::min(64, px0 + (int)hp.w) * k, ry1 = y + std::min(64, py0 + (int)hp.h) * k;
      P.rect(rx0, ry0, rx1 - rx0, ry1 - ry0, Color(0.95f, 0.78f, 0.42f, 0.28f));
      P.frame(rx0, ry0, rx1 - rx0, ry1 - ry0, Color(0.98f, 0.82f, 0.42f, 0.9f));
      const float cx = std::floor(x + (px0 + hp.w * 0.5f) * k), cy = std::floor(y + (py0 + hp.h * 0.5f) * k);
      if (cx < x + 3 || cy < y + 4 || cx > x + size - 4 || cy > y + size - 3) continue;
      // a house: a dark outline, a red roof, a pale wall
      P.rect(cx - 3, cy - 3, 7, 7, Color(0.08f, 0.05f, 0.04f, 0.9f));
      P.rect(cx - 2, cy - 2, 5, 1, Color(0.86f, 0.32f, 0.24f));
      P.rect(cx - 1, cy - 3, 3, 1, Color(0.86f, 0.32f, 0.24f));
      P.rect(cx - 2, cy - 1, 5, 3, Color(0.96f, 0.90f, 0.74f));
      P.rect(cx, cy + 1, 1, 2, Color(0.40f, 0.26f, 0.16f));
    }
  drawPartyMinimap(g, x, y, size, ox, oy);   // (M8) companions, the waiting ones, graves (party_view.cpp)
  P.rect(x + (ptx - ox) * size / 64.0f - 1, y + (pty - oy) * size / 64.0f - 1, 3, 3, Color(1, 1, 1));
  int tx, ty;
  const Quest* mq = g.trackedQuest >= 0 ? g.questById(g.trackedQuest) : nullptr;
  if (!g.inside && mq && questTile(g, *mq, tx, ty)) {
    int ax = std::clamp(tx - ox, 1, 62), ay = std::clamp(ty - oy, 1, 62);
    if (((int)(t_ * 3)) & 1) P.rect(x + ax - 1, y + ay - 1, 3, 3, kGold);
  }
}

// ------------------------------------------------------------------ menus
void View::drawMenu(Game& g) {
  Pix& P = *pix_;
  P.rect(0, 0, Pix::W, Pix::H, Color(0, 0, 0, 0.55f));
  if (settingsOpen_) return;   // the settings screen replaces the menu while it is open
  const UiBox box = modalBox(g);
  P.pushBox(box.x, box.y, box.w, box.h);
  struct Pop { Pix& p; ~Pop() { p.popBox(); } } pop{P};
  panel(8, 6, Pix::W - 16, Pix::H - 12);
  // tabs, and the close X at the strip's right end (finger-sized on touch)
  g_partyTab = partyTabShown(g);
  if (!g_partyTab && menuTab_ == kPartyTab) menuTab_ = 5;
  const TabLay T = tabLay(touchUI);
  for (int i = 0; i < nTabs(); i++) button(T.x0 + i * T.tw, T.y, T.tw - 4, T.h, kTabs[tabAt(i)], tabAt(i) == menuTab_);
  for (int i = 0; i < nTabs(); i++)   // (M9) perk points to spend: a pulsing gold pip on the SKILLS tab
    if (tabAt(i) == kSkillsTab && g.skills.perkPts > 0) {
      const float k = 0.6f + 0.4f * std::sin(t_ * 5.0f);
      const float px = std::floor(T.x0 + i * T.tw + T.tw - 4 - 6), py = std::floor(T.y + 2);
      P.rect(px + 1, py, 1, 5, Color(1, 0.84f, 0.4f, k)); P.rect(px, py + 1, 3, 3, Color(1, 0.84f, 0.4f, k));
    }
  button(T.xX, T.xY, T.xW, T.xH, "X", false);
  float top = 34;
  // a touch list's page buttons and its "1-8 OF 15" count (keyboard: the count alone, under the list)
  auto pager = [&](const Cols& C, const MenuLay& L, int first, int n) {
    if (n <= L.rows) return;
    const std::string cnt = std::to_string(first + 1) + "-" + std::to_string(std::min(n, first + L.rows)) + " OF " + std::to_string(n);
    if (touchUI) {
      const PageBtns pb = pageBtns(C, L);
      button(pb.upX, pb.y, pb.w, pb.h, "UP", false);
      button(pb.downX, pb.y, pb.w, pb.h, "DOWN", false);
      P.text(C.lx + C.lw / 2, pb.y + std::floor((pb.h - 7) / 2), cnt, 1, kDim, 1);
    } else {
      P.text(C.lx + C.lw / 2, Pix::H - 18, cnt, 1, kDim, 1);
    }
  };
  switch (menuTab_) {
    case 0: {   // items: the pack on the left, the picked item's detail beside it
      int n = (int)g.inv.size();
      menuSel_ = std::clamp(menuSel_, 0, std::max(0, n - 1));
      const MenuLay L = menuLay(touchUI);
      const Cols C = itemCols();
      int rows = L.rows;
      if (menuSel_ < menuScroll_) menuScroll_ = menuSel_;
      if (menuSel_ >= menuScroll_ + rows) menuScroll_ = menuSel_ - rows + 1;
      menuScroll_ = std::clamp(menuScroll_, 0, std::max(0, n - rows));
      for (int r = 0; r < rows && menuScroll_ + r < n; r++) {
        int i = menuScroll_ + r;
        const Item& it = g.inv[i];
        const float rowTop = top + 10 + r * L.pitch, y = rowTop + std::floor((L.pitch - 8) / 2);
        if (i == menuSel_) P.rect(C.lx, rowTop, C.lw, L.pitch - (touchUI ? 1 : 0), Color(0.3f, 0.22f, 0.12f, 0.8f));
        else if (touchUI && (r & 1)) P.rect(C.lx, rowTop, C.lw, L.pitch - 1, Color(0.16f, 0.12f, 0.08f, 0.35f));
        const float isz = touchUI ? 16.0f : 12.0f;
        rarityIcon(g, it, C.lx + 2, rowTop + std::floor((L.pitch - isz) / 2), isz);   // (M8b) in its rarity's cell
        const float nx = C.lx + (touchUI ? 22.0f : 18.0f);
        std::string cnt = it.count > 1 ? " (" + std::to_string(it.count) + ")" : "";
        std::string name = fitText(it.name, C.lx + C.lw - 14 - nx - P.textW(cnt, 1)) + cnt;
        rarityText(nx, y, name, it.rarity);
        if (equipped(g, i)) P.text(C.lx + C.lw - 6, y, "E", 1, kGold, 2);
        else if (itemEquippable(it.kind)) {   // (M8b) an unworn piece: is it better than what that slot wears?
          bool w = false;
          const Item* vs = wornFor(g, it, i, w);
          // (M8b fixer r2) a great piece that cannot be worn now: why, in red, never an upgrade arrow
          const int why = attuneWhy(g, it, vs);
          if (why) P.text(C.lx + C.lw - 4, y, why == 1 ? "LV10" : "FULL", 1, Color(1.0f, 0.42f, 0.36f), 2);
          else cmpArrow(C.lx + C.lw - 11, y, cmpVerdict(g, it, vs));
        }
      }
      if (n == 0) P.text(30, top + 14, "YOUR PACK IS EMPTY", 1, kDim);
      pager(C, L, menuScroll_, n);
      // detail
      P.rect(C.div, top + 8, 1, Pix::H - top - 24, Color(0.4f, 0.32f, 0.2f));
      if (n > 0) {
        const Item& it = g.inv[menuSel_];
        int eq = equippedOf(g, it.kind);
        // (M8b) the item card: its name in the ladder colour, rarity and item level, every affix in plain words, the
        // power, and the compare against what that slot wears (green / red arrows; an empty slot is an upgrade)
        {
          bool isWorn = false;
          const Item* vs = wornFor(g, it, menuSel_, isWorn);
          itemCard(g, it, C.dx + 1, top + 13, C.dw - 1, menuLay(touchUI).btnY - 6, true, vs, isWorn);
        }
        bool canEquip = itemEquippable(it.kind);
        std::string use = canEquip ? (equipped(g, menuSel_) ? "UNEQUIP" : "EQUIP") : (it.kind == ItemKind::Food ? "EAT" : it.kind == ItemKind::Potion || it.name.rfind("SPELL TOME", 0) == 0 ? "USE" : it.kind == ItemKind::Book ? "READ" : "");
        // (M6 fixer r2, review: "tapping EQUIP on a legendary below level 10 does nothing visible") a legendary that cannot
        // be attuned yet shows why on its button, drawn idle (the refusal notice would hide behind the open menu)
        bool idle = false;
        if (canEquip && !equipped(g, menuSel_) && gear::attunes(it)) {
          const int slots = gear::attunementSlots(g.plLevel);
          int worn = g.craft.live.stats.legendaries;
          if (eq >= 0 && eq < (int)g.inv.size() && gear::attunes(g.inv[(size_t)eq])) worn--;
          if (worn >= slots) { use = slots == 0 ? "FROM LEVEL 10" : "ATTUNED FULL"; idle = true; }
        }
        const float bw = std::floor((C.dw - 20) / 2);
        if (!use.empty()) button(C.dx, L.btnY, bw, L.btnH, use + (touchUI || idle ? "" : " (ENT)"), !idle);
        if (it.kind != ItemKind::Quest) button(C.dx + bw + 8, L.btnY, bw, L.btnH, touchUI ? "DROP" : "DROP (X)", false);
      }
      break;
    }
    case 1: {   // quests: the journal on the left, the picked quest beside it
      // (M4) a section strip over the list: QUESTS | NEWS | HISTORY (realm_hud.cpp)
      drawJournalStrip(questCols().lx, questCols().lw, top);
      if (journalSec_ != 0) { drawJournalSection(g, top + journalStripH()); break; }
      top += journalStripH();
      // (P1) the open quests, then the LEADS, then the finished quests (journalRows)
      const std::vector<JRow> rows = journalRows(g);
      menuSel_ = std::clamp(menuSel_, 0, std::max(0, (int)rows.size() - 1));
      MenuLay L = menuLay(touchUI);
      L.rows = std::max(3, L.rows - (int)std::ceil(journalStripH() / L.pitch));
      const Cols C = questCols();
      if (menuSel_ < menuScroll_) menuScroll_ = menuSel_;
      if (menuSel_ >= menuScroll_ + L.rows) menuScroll_ = menuSel_ - L.rows + 1;
      menuScroll_ = std::clamp(menuScroll_, 0, std::max(0, (int)rows.size() - L.rows));
      for (int r = 0; r < L.rows && menuScroll_ + r < (int)rows.size(); r++) {
        const int oi = menuScroll_ + r;
        const JRow& jr = rows[(size_t)oi];
        const float rowTop = top + 10 + r * L.pitch, y = rowTop + std::floor((L.pitch - 8) / 2);
        if (oi == menuSel_) P.rect(C.lx, rowTop, C.lw, L.pitch - (touchUI ? 1 : 0), Color(0.3f, 0.22f, 0.12f, 0.8f));
        else if (touchUI && (r & 1)) P.rect(C.lx, rowTop, C.lw, L.pitch - 1, Color(0.16f, 0.12f, 0.08f, 0.35f));
        if (jr.q >= 0) {
          const Quest& q = g.quests[(size_t)jr.q];
          Color c = q.state == QState::Done ? kDim : (q.type == QType::Main ? kGold : kText);
          P.text(C.lx + 4, y, (q.id == g.trackedQuest ? "> " : "  ") + fitText(q.title, C.lw - 20), 1, c);
          continue;
        }
        // a lead: a hairline over the first one parts them from the quests; the source word in its colour, the line,
        // the way there at the right (touch: the source and the way on a small first line, the words under them)
        const jobs::Lead& l = g.jobBook.leads[(size_t)jr.lead];
        const bool stale = l.state == jobs::LS_STALE;
        if (oi == 0 || rows[(size_t)oi - 1].lead < 0) {
          P.rect(C.lx + 2, rowTop - 1, C.lw - 4, 1, Color(0.55f, 0.43f, 0.26f, 0.9f));
          P.rect(C.lx + 2, rowTop, C.lw - 4, 1, Color(0.04f, 0.03f, 0.03f, 0.5f));
        }
        Color tc = leadColour(l.from), xc = kText;
        if (stale) { tc = Color(tc.r * 0.62f, tc.g * 0.62f, tc.b * 0.62f); xc = kDim; }
        const std::string dir = leadDir(g, l, false);
        const float dirW = dir.empty() ? 0.0f : (float)P.textW(dir, 1) + 4;
        if (touchUI) {
          const float y1 = rowTop + 3, y2 = rowTop + 13;
          P.text(C.lx + 16, y1, jobs::leadFromName(l.from), 1, tc);
          if (!dir.empty()) P.text(C.lx + C.lw - 4 - (float)P.textW(dir, 1), y1, dir, 1, stale ? kDim : Color(0.80f, 0.74f, 0.62f));
          P.rect(C.lx + 7, y1 + 2, 3, 3, tc);
          P.text(C.lx + 16, y2, leadRow(g, l, C.lw - 22), 1, xc);
        } else {
          // (one 12 px line: the source is its bullet's colour here, and its word heads the picked lead's page)
          P.rect(C.lx + 7, y + 2, 3, 3, tc);
          P.text(C.lx + 16, y, leadRow(g, l, C.lw - 22 - dirW), 1, xc);
          if (!dir.empty()) P.text(C.lx + C.lw - 4 - (float)P.textW(dir, 1), y, dir, 1, stale ? kDim : Color(0.80f, 0.74f, 0.62f));
        }
      }
      pager(C, L, menuScroll_, (int)rows.size());
      P.rect(C.div, top - journalStripH() + 8, 1, Pix::H - (top - journalStripH()) - 24, Color(0.4f, 0.32f, 0.2f));
      const int per = std::max(10, (int)(C.dw / 6));
      if (!rows.empty() && rows[(size_t)menuSel_].lead >= 0) {
        // the picked lead: its words, where it came from and when, where it points, and what to do about it
        const jobs::Lead& l = g.jobBook.leads[(size_t)rows[(size_t)menuSel_].lead];
        const bool stale = l.state == jobs::LS_STALE;
        float y = top - journalStripH() + 12;
        for (const std::string& ln : wrap(l.text, per)) { P.text(C.dx, y, ln, 1, stale ? kDim : kText); y += 10; }
        y += 4;
        const Color tc = leadColour(l.from);
        P.rect(C.dx, y + 2, 3, 3, tc);
        // the source word in its colour, then when, dim ("RUMOUR  3 DAYS AGO")
        const std::string src = jobs::leadFromName(l.from);
        P.text(C.dx + 8, y, src, 1, tc);
        P.text(C.dx + 8 + (float)P.textW(src, 1) + 12, y, fitText(rui::daysAgo(g.day, l.day), C.dw - 24 - (float)P.textW(src, 1)), 1, kDim);
        y += 14;
        int si = l.site ? g.world.siteHandle(l.site) : -1;
        std::string where;
        if (si >= 0 && g.world.sites[(size_t)si].settlement()) where = "IN " + g.world.sites[(size_t)si].name;
        if (l.hasPos) {
          const std::string d = leadDir(g, l, true);
          where += (where.empty() ? std::string() : ", ") + (d == "HERE" ? std::string("RIGHT HERE") : leadWalk(g, l) + " " + d);
        }
        if (!where.empty()) { for (const std::string& ln : wrap(where, per)) { P.text(C.dx, y, ln, 1, kText); y += 10; } y += 4; }
        const char* hint = stale ? "AN OLD LEAD. IT MAY HAVE GONE COLD BY NOW."
                           : l.from == jobs::LeadFrom::Teller ? "SOMETHING TROUBLES THEM. FIND THEM AND HEAR THEM OUT."
                           : l.from == jobs::LeadFrom::Incident && l.qtype == (uint8_t)QType::COUNT ? "IT'S OVER NOW. LOOK THEM UP: THEY MAY NEED A FRIEND."
                           : l.who ? "FIND THE ONE IT NAMES AND ASK IF THEY NEED HELP."
                           : l.qtype != (uint8_t)QType::COUNT ? "GO AND SEE. SOMEONE THERE MAY HAVE WORK."
                                                              : "THE PLACE IS ON YOUR MAP. WHAT LIES THERE IS FOR YOU TO FIND.";
        for (const std::string& ln : wrap(hint, per)) { if (y > L.trackY - 10) break; P.text(C.dx, y, ln, 1, kDim); y += 10; }
      } else if (!rows.empty()) {
        const Quest& q = g.quests[(size_t)rows[(size_t)menuSel_].q];
        float y = top - journalStripH() + 12;
        for (const std::string& l : wrap(q.title, per)) { P.text(C.dx, y, l, 1, q.type == QType::Main ? kGold : kText); y += 10; }
        y += 2;
        std::string st = q.state == QState::Done ? ((q.flags & QF_FAILED) ? "ENDED UNFINISHED" : "COMPLETED") : q.state == QState::Complete ? "READY TO TURN IN" : "IN PROGRESS";
        if (q.state != QState::Done) { std::string qs = g.questStatus(q); if (!qs.empty()) st = qs; }
        for (const std::string& l : wrap(st, per)) { P.text(C.dx, y, l, 1, q.state == QState::Complete ? Color(0.5f, 1, 0.5f) : kDim); y += 10; }
        y += 6;
        // the description fills the room down to the progress / reward lines (never under them)
        const bool prog = (q.type == QType::Hunt) || (q.type == QType::Main && q.stage == 1);
        const bool rew = q.gold > 0 && q.state != QState::Done;
        float footY = L.trackY - 8 - (rew ? 12.0f : 0.0f) - (prog ? 12.0f : 0.0f);
        const int maxLines = std::max(1, (int)((footY - y) / 10));
        auto dl = wrap(q.desc, per);
        for (int k = 0; k < (int)dl.size() && k < maxLines; k++) {
          std::string l = dl[(size_t)k];
          if (k == maxLines - 1 && (int)dl.size() > maxLines) l = fitText(l, C.dw - 18) + "...";
          P.text(C.dx, y + k * 10, l, 1, kText);
        }
        float fy = L.trackY - 8 - (rew ? 12.0f : 0.0f) - (prog ? 12.0f : 0.0f) + 2;
        if (q.type == QType::Hunt) { P.text(C.dx, fy, "PROGRESS " + std::to_string(q.have) + "/" + std::to_string(q.need), 1, kGold); fy += 12; }
        if (q.type == QType::Main && q.stage == 1) { P.text(C.dx, fy, "EMBER SHARDS " + std::to_string(q.have) + "/3", 1, kGold); fy += 12; }
        if (rew) P.text(C.dx, fy, "REWARD " + std::to_string(q.gold) + " GOLD", 1, kGold);
        if (q.state != QState::Done) button(C.dx, L.trackY, std::min(C.dw, touchUI ? 140.0f : 110.0f), L.trackH, q.id == g.trackedQuest ? "TRACKED" : "TRACK", q.id == g.trackedQuest);
      } else P.text(C.dx, top - journalStripH() + 14, "NO QUESTS YET", 1, kDim);
      break;
    }
    case 2:   // map (worldmap.cpp, M2: the whole tab - the map, its side column, legend and travel)
      drawMapTab(g, top);
      break;
    case 3: {   // hero: the numbers in a card on the left, the hero on a lit stage on the right (the level-up choice under him)
      const Actor& p = g.pl();
      const HeroLay H = heroLay(touchUI, g.perkPts > 0);
      const float x = H.lx;
      float y = top + 12;
      P.text(x, y, "LEVEL " + std::to_string(g.plLevel), 2, kGold);
      y += 18;
      {   // the XP toward the next level as a bar
        const float bw = H.lw, k = clampf(g.plXp / (float)std::max(1, gear::xpForNext(g.plLevel)), 0, 1);
        P.rect(x, y, bw, 5, Color(0.05f, 0.04f, 0.06f, 0.9f));
        P.rect(x + 1, y + 1, (bw - 2) * k, 3, kGold);
        P.rect(x + 1, y + 1, (bw - 2) * k, 1, Color(1, 0.95f, 0.7f));
        P.text(x + bw, y + 8, "XP " + std::to_string(g.plXp) + " / " + std::to_string(gear::xpForNext(g.plLevel)), 1, kDim, 2);
        y += 22;
      }
      float mpMax = g.maxMp, stMax = g.maxSt;   // include enchantment bonuses, same as the HUD bars
      for (int idx : g.worn()) {
        if (idx < 0 || idx >= (int)g.inv.size()) continue;
        if (g.inv[idx].ench == Ench::Magicka) mpMax += g.inv[idx].enchPow;
        if (g.inv[idx].ench == Ench::Stamina) stMax += g.inv[idx].enchPow;
      }
      mpMax += (float)g.craft.live.stats.get(Affix::Magicka);
      if (g.craft.live.stats.has(Unique::Wayfarer)) stMax *= 1.2f;
      const float sp = Pix::H >= 285 ? 11.0f : 10.0f;
      auto stat = [&](const std::string& n, const std::string& v, Color c) { P.text(x, y, n, 1, kDim); P.text(x + H.lw, y, v, 1, c, 2); y += sp; };
      auto vital = [&](const std::string& n, float v, float mx, Color c) {
        stat(n, std::to_string((int)v) + " / " + std::to_string((int)mx), kText);
        const float k = clampf(v / std::max(1.0f, mx), 0, 1);
        P.rect(x, y - 2, H.lw, 2, Color(0.12f, 0.1f, 0.1f, 0.9f));
        P.rect(x, y - 2, H.lw * k, 2, c);
        y += 3;
      };
      vital("HEALTH", p.hp, p.maxHp, Color(0.85f, 0.18f, 0.16f));
      vital("MAGICKA", g.mp, mpMax, Color(0.25f, 0.45f, 0.95f));
      vital("STAMINA", g.stamina, stMax, Color(0.3f, 0.8f, 0.35f));
      y += 4;
      stat("WEAPON DAMAGE", std::to_string((int)g.weaponDamage()), kText);
      stat("ARMOR", std::to_string((int)g.armorRating()), kText);
      y += 4;
      stat("GOLD", std::to_string(g.gold), kGold);
      stat("KILLS", std::to_string(g.kills), kText);
      stat("DUNGEONS CLEARED", std::to_string(g.dungeonsCleared), kText);
      {   // (M5, 15.2) the light hunger and sleep buffs by name
        const uint8_t bf = g.life.player.buffs();
        std::string cond;
        Color cc = kText;
        for (uint8_t bit : {life::BUFF_WELLFED, life::BUFF_RESTED, life::BUFF_HUNGRY, life::BUFF_WEARY})
          if (bf & bit) { if (!cond.empty()) cond += ", "; cond += life::buffName(bit); }
        if (cond.empty()) { cond = "FINE"; cc = kDim; }
        else if (bf & (life::BUFF_HUNGRY | life::BUFF_WEARY)) cc = Color(0.95f, 0.66f, 0.42f);
        else cc = Color(0.62f, 0.90f, 0.55f);
        stat("CONDITION", cond, cc);
      }
      y += 4;
      std::string spells;   // (M9) from the spellbook (the classic bits are its mirror)
      for (uint32_t id : g.arcana.book.known)
        if (const magic::SpellDef* d = magic::spellById(id)) { if (!spells.empty()) spells += ", "; spells += d->name; }
      P.text(x, y, "SPELLS", 1, kDim);
      auto sl = wrap(spells.empty() ? "NONE" : spells, std::max(8, (int)((H.lw - 48) / 6)));
      for (size_t k = 0; k < sl.size() && y + k * 10 < Pix::H - 20; k++) P.text(x + H.lw, y + k * 10, sl[k], 1, Color(0.6f, 0.75f, 1.0f), 2);
      // the stage
      P.rect(H.sx, H.sy, H.sw, H.sh, Color(0.03f, 0.025f, 0.04f, 0.55f));
      P.frame(H.sx, H.sy, H.sw, H.sh, Color(0.3f, 0.24f, 0.15f));
      heroStage(P, sprites_.light_, H.pcx, H.footY, H.scale / 5.0f);
      // (P2c SWITCH) rig 2: the hero on the doll at the largest whole multiple the stage holds (the T6 envelope's room),
      // walking on the spot facing down as the v1 sheet did, with the living finish
      const MenuStage hst{H.sx + 1, H.sy + 14, H.sw - 2, H.footY + 4 - (H.sy + 14)};   // under the LEVEL UP line, over the buttons
      const int hsc = menuDollScale(p.look, hst, H.pcx, H.footY, std::max(1, (int)H.scale));
      if (!(hsc > 0 && drawMenuDoll(p.look, kMenuHero, 0, MenuGait::Walk, H.pcx, H.footY, hsc, hst))) {
      const Tex& t = sprites_.humanTex(*this, p.look);
      const int frame = 1 + ((int)(t_ * 5.0f) & 3);
      P.blitEx(t, frame * art::HUMAN_W, 0, art::HUMAN_W, art::HUMAN_H, H.pcx - art::HUMAN_W * H.scale / 2, H.footY - (art::HUMAN_H - 1) * H.scale,
               art::HUMAN_W * H.scale, art::HUMAN_H * H.scale);
      noteMenuV1(kMenuHero, H.pcx - art::HUMAN_W * H.scale / 2, H.footY - (art::HUMAN_H - 1) * H.scale, (int)(art::HUMAN_W * H.scale), (int)(art::HUMAN_H * H.scale), hst);
      }
      if (g.perkPts > 0) {
        P.textS(H.pcx, H.sy + 6, "LEVEL UP! CHOOSE A STAT", 1, kGold, 1);
        for (int i = 0; i < 3; i++) {
          static const char* lb[3] = {"+12 HEALTH", "+12 MAGICKA", "+12 STAMINA"};
          button(H.bx, H.by + i * (H.bh + 4), H.bw, H.bh, lb[i], levelSel_ == i);
        }
      } else {
        P.textS(H.pcx, H.footY + 10, g.app.name.empty() ? p.name : g.app.name, 1, kGold, 1);
        P.text(H.pcx, H.footY + 21, backgroundInfo(g.background).name, 1, kDim, 1);
      }
      break;
    }
    case 4: {   // system
      const float sh = menuLay(touchUI).sysH, pitch = sh + 6;
      const float bw = std::clamp(std::floor(Pix::W * 0.36f), 140.0f, 220.0f), bx = std::floor((Pix::W - bw) / 2);
      menuSel_ = std::clamp(menuSel_, 0, 3);
      const float by = sysTop(top, sh, touchUI);
      button(bx, by, bw, sh, "SAVE GAME", menuSel_ == 0);
      button(bx, by + pitch, bw, sh, "SETTINGS", menuSel_ == 1);
      button(bx, by + pitch * 2, bw, sh, touchUI ? "TOUCH CONTROLS: ON" : "TOUCH CONTROLS: OFF", menuSel_ == 2);
      button(bx, by + pitch * 3, bw, sh, "SAVE AND QUIT", menuSel_ == 3);
      const float ty = by + pitch * 4 + 8;
      P.text(Pix::W / 2, ty, "WORLD SEED " + std::to_string(g.seed), 1, kDim, 1);
      if (!touchUI) {
        const char* keys[] = {"WASD MOVE   J/SPACE ATTACK   K BOW   L SPELL", "SHIFT ROLL   E TALK/OPEN   Q POTION   R SWAP SPELL",
                              "1-4 CAST (HOLD: UPCAST, ALT: DUAL)   CTRL SNEAK", "TAB MENU   M MAP   ESC PAUSE   F11 FULLSCREEN"};
        for (int i = 0; i < 4; i++) P.text(Pix::W / 2, ty + 16 + i * 11, keys[i], 1, kDim, 1);
      }
      break;
    }
    case 5:   // equip (paperdoll.cpp)
      drawPaperdoll(g, 8, 30, Pix::W - 16, Pix::H - 36);
      break;
    case kPartyTab:   // (M8) the companions' paper doll and the remembered (paperdoll.cpp)
      drawPartyPage(g);
      break;
    case kSkillsTab:  // (M9) the skills and their perk constellations (skills_ui.cpp)
      drawSkillsPage(g);
      break;
    case kSpellsTab:  // (M9) the spellbook and the quick slots (spellbook_ui.cpp)
      drawSpellsPage(g);
      break;
  }
}

// ------------------------------------------------------------------ M5 "Hearth and Hall": the player's buffs, the mood
namespace {
// The buff icons, 11 x 11 pixel art in the HUD's frame style: a dark plate, a 1 px rim green for a boon (Well Fed,
// Rested) and amber for a want (Hungry, Weary), the glyph lit from the top-left. Palette: 'o' outline, 'r' rim,
// 'p' plate; glyph tones 'H' highlight, 'M' mid, 'S' shade, 'W' white glint, 'D' dark detail.
Canvas paintBuffIcon(uint64_t key) {
  const uint8_t bit = (uint8_t)(key & 255);
  static const char* kWellFed[11] = {   // a crusty loaf with a scored top and a drumstick tucked behind
      "ooooooooooo", "orrrrrrrrro", "orppppDDppo", "orppHHMDDro", "orpHWHMMSDo", "orHHMHMMSSo",
      "orHMSMMSSSo", "orMMMMSSSDo", "orpSSSSSDpo", "orrrrrrrrro", "ooooooooooo"};
  static const char* kRested[11] = {   // a crescent moon over a pillow
      "ooooooooooo", "orrrrrrrrro", "orppHHpppro", "orpHWpppMro", "orpHHppppro", "orpHHMppppo",
      "orppHMMpppo", "orpWWWWWppo", "orpMMMMMSpo", "orrrrrrrrro", "ooooooooooo"};
  // (fixer M5 r3, review: "the Hungry icon reads as a box or crate at 1x") an empty plate seen from above between a fork
  // and a knife: the place set and nothing on it
  static const char* kHungry[11] = {
      "ooooooooooo", "orrrrrrrrro", "orWpppppWro", "orDpSMSpWro", "orDSHHMSWro", "orDMHWHSDro",
      "orDSMHMSDro", "orppSSSppro", "orpppppppro", "orrrrrrrrro", "ooooooooooo"};
  static const char* kWeary[11] = {   // a heavy "Z"
      "ooooooooooo", "orrrrrrrrro", "orpppppppro", "orpWHHHHpro", "orppppMSpro", "orpppMSppro",
      "orppMSpppro", "orpHMMMSpro", "orpppppppro", "orrrrrrrrro", "ooooooooooo"};
  const char** pat = bit == life::BUFF_WELLFED ? kWellFed : bit == life::BUFF_RESTED ? kRested : bit == life::BUFF_HUNGRY ? kHungry : kWeary;
  const bool boon = bit == life::BUFF_WELLFED || bit == life::BUFF_RESTED;
  uint32_t H, M, S, W = rgba(255, 250, 230), D;
  if (bit == life::BUFF_WELLFED) { H = rgba(240, 196, 110); M = rgba(206, 140, 62); S = rgba(140, 84, 40); D = rgba(98, 54, 30); }
  else if (bit == life::BUFF_RESTED) { H = rgba(250, 232, 150); M = rgba(150, 168, 214); S = rgba(96, 108, 160); W = rgba(232, 236, 248); D = rgba(60, 60, 100); }
  else if (bit == life::BUFF_HUNGRY) { H = rgba(238, 232, 218); M = rgba(198, 190, 176); S = rgba(126, 118, 112); D = rgba(182, 188, 204); W = rgba(255, 255, 255); }
  else { H = rgba(214, 206, 236); M = rgba(170, 160, 206); S = rgba(98, 88, 140); D = rgba(46, 36, 44); W = rgba(246, 242, 255); }
  const uint32_t rim = boon ? rgba(110, 196, 96) : rgba(230, 150, 60), plate = rgba(28, 22, 32), ol = rgba(10, 8, 12);
  Canvas c(11, 11);
  for (int y = 0; y < 11; y++)
    for (int x = 0; x < 11; x++) {
      const char ch = pat[y][x];
      uint32_t v = 0;
      switch (ch) {
        case 'o': v = ol; break;
        case 'r': v = rim; break;
        case 'p': v = plate; break;
        case 'H': v = H; break;
        case 'M': v = M; break;
        case 'S': v = S; break;
        case 'W': v = W; break;
        case 'D': v = D; break;
        default: break;
      }
      if ((x == 0 || x == 10) && (y == 0 || y == 10)) v = 0;   // rounded corners
      c.set(x, y, v);
    }
  // the rim lit top-left, shaded bottom-right
  auto shade = [](uint32_t v, float k) {   // k > 1 lighter, < 1 darker
    auto ch = [&](int s) { const int x = (int)((v >> s) & 255); return std::min(255, (int)(k > 1 ? x + (255 - x) * (k - 1) : x * k)); };
    return rgba(ch(0), ch(8), ch(16));
  };
  for (int k = 1; k < 10; k++) {
    if (pat[1][k] == 'r') c.set(k, 1, shade(rim, 1.3f));
    if (pat[9][k] == 'r') c.set(k, 9, shade(rim, 0.65f));
  }
  return c;
}
}  // namespace

// (M5, 15.2) the player's buffs as icons in a row beside the magicka and stamina bars (readable on the phone: 11 px
// plates); a boon about to run out (under an in-game hour) blinks
void View::drawBuffs(Game& g, float x, float y) {
  Pix& P = *pix_;
  const life::PlayerNeeds& pn = g.life.player;
  const uint8_t bf = pn.buffs();
  if (!bf) return;
  float cx = x;
  for (uint8_t bit : {life::BUFF_WELLFED, life::BUFF_RESTED, life::BUFF_HUNGRY, life::BUFF_WEARY}) {
    if (!(bf & bit)) continue;
    const float left = bit == life::BUFF_WELLFED ? pn.fedH : bit == life::BUFF_RESTED ? pn.restedH : 99.0f;
    const bool blink = left < 1.0f && ((int)(t_ * 3) & 1);
    const Tex& t = sprites_.cachedTex(*this, 0x4Eull << 56 | bit, paintBuffIcon);
    P.blitEx(t, 0, 0, t.w, t.h, std::floor(cx), std::floor(y), (float)t.w, (float)t.h, false, Color(1, 1, 1, blink ? 0.45f : 1.0f));
    if (bit == life::BUFF_WELLFED && pn.mealQuality > 0) {   // (M7, 15.2) the meal's quality: gold pips under the loaf
      for (int q = 0; q < std::min(3, (int)pn.mealQuality); q++) {
        const float qx = std::floor(cx) + 1 + q * 3.5f, qy = std::floor(y) + t.h + 1;
        P.rect(qx - 0.5f, qy - 0.5f, 3, 3, Color(0.05f, 0.04f, 0.04f, 0.8f));
        P.rect(qx, qy, 2, 2, Color(0.98f, 0.82f, 0.42f, blink ? 0.45f : 1.0f));
      }
    }
    cx += t.w + 2;
  }
}

// (fixer M5 r3) a tap on the buff icons (drawBuffs at the bars' end): a toast per buff says what it does and how it ends
bool View::buffTap(Game& g, Vec2 p) {
  const uint8_t bf = g.life.player.buffs();
  if (!bf) return false;
  const float x0 = (float)Pix::SL + 108, y0 = (float)Pix::ST + 9;
  int n = 0;
  for (uint8_t bit : {life::BUFF_WELLFED, life::BUFF_RESTED, life::BUFF_HUNGRY, life::BUFF_WEARY}) if (bf & bit) n++;
  if (p.x < x0 || p.x >= x0 + n * 13.0f + 6 || p.y < y0 || p.y >= y0 + 19) return false;
  auto say = [&](const char* s, uint32_t c) { Toast t; t.s = s; t.c = col(c); toasts_.push_back(t); };
  if (bf & life::BUFF_WELLFED) {
    static const char* kQ[] = {"WELL FED", "WELL FED (GOOD MEAL)", "WELL FED (FINE MEAL)", "WELL FED (SUPERB MEAL)"};
    const std::string s = std::string(kQ[std::min(3, (int)g.life.player.mealQuality)]) + ": STAMINA AND HEALTH COME BACK FASTER";
    Toast t; t.s = s; t.c = col(rgba(240, 200, 110)); toasts_.push_back(t);
  }
  if (bf & life::BUFF_RESTED) say("RESTED: +10% EXPERIENCE", rgba(150, 200, 255));
  if (bf & life::BUFF_HUNGRY) say("HUNGRY: STAMINA COMES BACK SLOWER. EAT FOOD (ITEMS) OR A MEAL AT AN INN", rgba(220, 160, 110));
  if (bf & life::BUFF_WEARY) say("WEARY: SLEEP IN A BED, A BEDROLL OR AN INN ROOM", rgba(170, 160, 200));
  audio_->play(Sfx::MenuMove);
  return true;
}

// (M5, 15.12) the word for a settlement's mood, as the HUD and the map show it, and its colour (rgba)
std::string View::moodWord(const Game& g, ew::Gid site, uint32_t* color) {
  if (!site || !g.life.find(site)) return std::string();
  const uint16_t f = g.life.moodFlags(site);
  const int mood = g.life.mood(site);
  auto out = [&](const char* w, uint32_t c) { if (color) *color = c; return std::string(w); };
  // (M5 fixer) a raid being fought right now outranks everything (the bell is ringing: never "CONTENT")
  if (g.life.raid.site == site && g.life.raid.outcome == 0) return out("UNDER ATTACK", rgba(255, 84, 64));
  if (g.life.festival(site, g.day) || (f & life::MF_FESTIVAL)) return out("FESTIVAL", rgba(250, 206, 92));
  if (f & life::MF_RAIDED) return out("RAIDED", rgba(240, 96, 70));
  if (f & life::MF_WARTORN) return out("WAR-TORN", rgba(226, 104, 80));
  if (f & life::MF_FAMINE) return out("FAMINE", rgba(236, 120, 70));
  if (f & life::MF_HUNGRY) return out("HUNGRY", rgba(236, 160, 84));
  if (f & life::MF_GRIEF) return out("MOURNING", rgba(170, 180, 214));
  if (f & life::MF_BRAWLS) return out("RESTLESS", rgba(226, 150, 96));
  if (f & life::MF_EMIGRATING) return out("EMPTYING", rgba(200, 170, 140));
  if ((f & life::MF_CONTENT) || mood >= 60) return out(mood >= 78 ? "THRIVING" : "CONTENT", rgba(150, 214, 120));
  return out(mood < 35 ? "MISERABLE" : "UNEASY", rgba(214, 190, 130));
}

// (Witness, View::witnessRelease) the menu strip's PARTY flag and the quick bar's last extent back to their start
void witnessResetHud() { g_partyTab = false; g_quickBarR = 0; }
