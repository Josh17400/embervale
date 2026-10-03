// HUD, menus, dialogue, shop, title/death screens, and all input (keyboard, mouse, touch).
#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>
#include "rpg/view/view.h"

namespace {
Color col(uint32_t c, float a = 1) { return Color((c & 255) / 255.0f, ((c >> 8) & 255) / 255.0f, ((c >> 16) & 255) / 255.0f, a); }
const Color kGold(0.98f, 0.82f, 0.42f), kText(0.93f, 0.9f, 0.82f), kDim(0.62f, 0.58f, 0.52f), kPanel(0.07f, 0.06f, 0.08f);

// on-screen touch buttons
enum Btn { B_ATTACK, B_BOW, B_SPELL, B_ROLL, B_POTION, B_MENU, B_COUNT };
struct BtnDef { float x, y, r; };
const BtnDef kBtn[B_COUNT] = {{424, 222, 23}, {374, 240, 14}, {380, 196, 14}, {424, 172, 14}, {464, 176, 11}, {466, 12, 10}};

const char* kTabs[] = {"ITEMS", "QUESTS", "MAP", "HERO", "SYSTEM"};
constexpr int NTABS = 5;

std::vector<std::string> wrap(const std::string& s, int maxChars) {
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

std::string itemStats(const Game& g, const Item& it) {
  std::string s;
  switch (it.kind) {
    case ItemKind::Weapon: s = "DAMAGE " + std::to_string(it.power); break;
    case ItemKind::Bow: s = "ARROW DAMAGE " + std::to_string(it.power); break;
    case ItemKind::Staff: s = "SPELL POWER +" + std::to_string(it.power * 2) + "%"; break;
    case ItemKind::Armor: case ItemKind::Helmet: case ItemKind::Shield: s = "ARMOR " + std::to_string(it.power); break;
    case ItemKind::Potion: s = "RESTORES " + std::to_string(it.power); break;
    case ItemKind::Food: s = "HEALS " + std::to_string(it.power); break;
    case ItemKind::Arrows: s = "AMMUNITION"; break;
    case ItemKind::Quest: s = "QUEST ITEM"; break;
    default: s = "VALUE " + std::to_string(it.value); break;
  }
  (void)g;
  if (it.ench != Ench::None) {
    switch (it.ench) {
      case Ench::Fire: s += "\nBURNS FOR " + std::to_string(it.enchPow) + " FIRE"; break;
      case Ench::Frost: s += "\nFREEZES FOR " + std::to_string(it.enchPow) + " FROST"; break;
      case Ench::Drain: s += "\nDRAINS " + std::to_string(it.enchPow / 2) + " HEALTH"; break;
      case Ench::Health: s += "\n+" + std::to_string(it.enchPow) + " MAX HEALTH"; break;
      case Ench::Magicka: s += "\n+" + std::to_string(it.enchPow) + " MAX MAGICKA"; break;
      case Ench::Stamina: s += "\n+" + std::to_string(it.enchPow) + " MAX STAMINA"; break;
      case Ench::Fortify: s += "\n+" + std::to_string(it.enchPow / 2) + " ARMOR"; break;
      default: break;
    }
  }
  if (it.kind != ItemKind::Quest) s += "\nVALUE " + std::to_string(it.value);
  return s;
}
// a usable prop (chest, shrine, bed...) the attack button may "use" instead: only when no enemy is close,
// so mashing attack beside a shrine in a fight still swings the sword
int usablePropAt(const Game& g, int& tx, int& ty) {
  int pr = g.interactProp(tx, ty);
  if (!pr) return 0;
  for (size_t i = 1; i < g.actors.size(); i++) {
    const Actor& a = g.actors[i];
    if (a.hostile && a.st != AState::Dead && len2(a.p - g.pl().p) < 70 * 70) return 0;
  }
  return pr;
}
const char* useVerb(int pr) {
  switch ((art::Prop)(pr - 1)) {
    case art::Prop::Chest: return "OPEN";
    case art::Prop::Shrine: case art::Prop::Altar: return "PRAY";
    case art::Prop::BerryBush: return "PICK";
    case art::Prop::Signpost: return "READ";
    case art::Prop::Bed: return "SLEEP";
    default: return "USE";
  }
}
bool equipped(const Game& g, int i) {
  return i == g.eqWeapon || i == g.eqBow || i == g.eqStaff || i == g.eqArmor || i == g.eqHelmet || i == g.eqShield || i == g.eqRing || i == g.eqAmulet;
}
int equippedOf(const Game& g, ItemKind k) {
  switch (k) {
    case ItemKind::Weapon: return g.eqWeapon; case ItemKind::Bow: return g.eqBow; case ItemKind::Staff: return g.eqStaff;
    case ItemKind::Armor: return g.eqArmor; case ItemKind::Helmet: return g.eqHelmet; case ItemKind::Shield: return g.eqShield;
    case ItemKind::Ring: return g.eqRing; case ItemKind::Amulet: return g.eqAmulet; default: return -1;
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
  P.begin(Color(0, 0, 0));
  drawWorld(g);
  drawLighting(g);
  drawWeather(g, 0);
  // floating combat text (screen)
  Vec2 cam(std::floor(cam_.x), std::floor(cam_.y));
  for (const FloatText& f : texts_) {
    float a = clampf(1.2f - f.t, 0, 1);
    P.text(f.p.x - cam.x + 1, f.p.y - cam.y + 1, f.s, 1, Color(0, 0, 0, a * 0.7f), 1);
    P.text(f.p.x - cam.x, f.p.y - cam.y, f.s, 1, Color(f.c.r, f.c.g, f.c.b, a), 1);
  }
  if (g.mode == Mode::Title) { drawTitle(g, hasSave); }
  else {
    drawHud(g);
    if (touchUI && (g.mode == Mode::Play)) drawTouch(g);
    if (g.mode == Mode::Dialogue) drawDialogue(g);
    if (g.mode == Mode::Shop) drawShop(g);
    if (g.mode == Mode::Menu) drawMenu(g);
    if (g.mode == Mode::LevelUp) drawLevelUp(g);
    if (g.mode == Mode::Dead) drawDead(g);
    if (g.mode == Mode::Paused) {
      P.rect(0, 0, Pix::W, Pix::H, Color(0, 0, 0, 0.5f));
      P.text(Pix::W / 2, 120, "PAUSED", 3, kGold, 1);
      P.text(Pix::W / 2, 150, "ESC / TAP TO RESUME", 1, kText, 1);
    }
  }
  float f = std::max(fade_, g.sleepFade > 0 ? std::min(1.0f, g.sleepFade) : 0.0f);
  if (f > 0) P.rect(0, 0, Pix::W, Pix::H, Color(0, 0, 0, f));
}

void View::drawHud(Game& g) {
  Pix& P = *pix_;
  const Actor& p = g.pl();
  // vitals
  float mpMax = g.maxMp, stMax = g.maxSt;
  for (int idx : {g.eqArmor, g.eqHelmet, g.eqShield, g.eqRing, g.eqAmulet}) {
    if (idx < 0) continue;
    if (g.inv[idx].ench == Ench::Magicka) mpMax += g.inv[idx].enchPow;
    if (g.inv[idx].ench == Ench::Stamina) stMax += g.inv[idx].enchPow;
  }
  auto bar = [&](float x, float y, float w, float v, float mx, Color c, Color dark) {
    P.rect(x - 1, y - 1, w + 2, 6, Color(0.05f, 0.04f, 0.06f, 0.85f));
    P.rect(x, y, w, 4, Color(dark.r, dark.g, dark.b, 0.9f));
    float k = clampf(v / std::max(1.0f, mx), 0, 1);
    P.rect(x, y, w * k, 4, c);
    P.rect(x, y, w * k, 1, Color(std::min(1.0f, c.r + 0.3f), std::min(1.0f, c.g + 0.3f), std::min(1.0f, c.b + 0.3f)));
  };
  P.textS(6, 5, "LV " + std::to_string(g.plLevel), 1, kGold);
  bar(36, 6, 90, p.hp, p.maxHp, Color(0.85f, 0.18f, 0.16f), Color(0.3f, 0.05f, 0.05f));
  bar(36, 14, 70, g.mp, mpMax, Color(0.25f, 0.45f, 0.95f), Color(0.06f, 0.1f, 0.3f));
  bar(36, 22, 70, g.stamina, stMax, Color(0.3f, 0.8f, 0.35f), Color(0.06f, 0.22f, 0.08f));
  // xp sliver
  P.rect(6, 14, 26, 2, Color(0.1f, 0.1f, 0.1f, 0.8f));
  P.rect(6, 14, 26 * clampf(g.plXp / (float)g.xpForNext(), 0, 1), 2, kGold);
  // gold, arrows, spell
  int arrows = 0;
  for (auto& it : g.inv) if (it.kind == ItemKind::Arrows) arrows += it.count;
  P.blit(iconTex(art::Icon::Gold, 0), 4, 28);
  P.textS(20, 33, std::to_string(g.gold), 1, kGold);
  P.blit(iconTex(art::Icon::Arrows, 0), 56, 28);
  P.textS(72, 33, std::to_string(arrows), 1, kText);
  P.textS(100, 33, spellName(g.spell), 1, Color(0.6f, 0.75f, 1.0f));
  if (g.perkPts > 0 && ((int)(t_ * 2) & 1)) P.textS(6, 44, "LEVEL UP! OPEN MENU", 1, kGold);
  if (g.blessT > 0) P.textS(6, g.perkPts > 0 ? 54 : 44, g.blessName, 1, Color(0.7f, 0.85f, 1.0f, 0.8f));

  // minimap + location
  drawMinimap(g, Pix::W - 70, 24, 64);
  std::string loc = g.locName;
  if (loc.size() > 26) loc = loc.substr(0, 26);
  // clock
  int hh = (int)g.hour, mm = (int)((g.hour - hh) * 60);
  char clock[16];
  std::snprintf(clock, sizeof clock, "%02d:%02d", hh, mm);
  std::string dayStr = std::string("DAY ") + std::to_string(g.day) + " " + clock;
  {
    // a soft backing so the location / clock / quest column stays readable over busy roofs and snow
    const Quest* tq = g.trackedQuest >= 0 ? g.questById(g.trackedQuest) : nullptr;
    bool hasQ = tq && tq->state != QState::Done;
    int wmax = std::max(P.textW(loc, 1), P.textW(dayStr, 1));
    if (hasQ) wmax = std::max(wmax, P.textW(tq->title.size() > 22 ? tq->title.substr(0, 22) : tq->title, 1));
    if (hasQ && tq->state == QState::Complete) wmax = std::max(wmax, P.textW("RETURN TO " + tq->giverName, 1));
    float bh = hasQ ? 42.0f : 21.0f;
    P.rect(Pix::W - wmax - 10, 88, (float)wmax + 8, bh, Color(0.03f, 0.02f, 0.05f, 0.38f));
  }
  P.textS(Pix::W - 5, 91, loc, 1, kText, 2);
  P.textS(Pix::W - 5, 100, dayStr, 1, kDim, 2);
  if (!touchUI) { P.textS(Pix::W - 24, 7, "TAB", 1, kDim, 1); }
  else {
    const BtnDef& b = kBtn[B_MENU];
    P.rect(b.x - 9, b.y - 8, 18, 16, Color(0.1f, 0.08f, 0.08f, 0.8f));
    P.frame(b.x - 9, b.y - 8, 18, 16, Color(0.6f, 0.5f, 0.3f));
    for (int k = 0; k < 3; k++) P.rect(b.x - 5, b.y - 4 + k * 4, 10, 2, kText);
  }

  // tracked quest
  if (g.trackedQuest >= 0) {
    const Quest* q = g.questById(g.trackedQuest);
    if (q && q->state != QState::Done) {
      std::string obj;
      if (q->type == QType::Hunt && q->state == QState::Active) obj = std::to_string(q->have) + "/" + std::to_string(q->need);
      else if (q->type == QType::Main && q->stage == 1) obj = "SHARDS " + std::to_string(q->have) + "/3";
      else if (q->state == QState::Complete) obj = "RETURN TO " + q->giverName;
      std::string t = q->title.size() > 22 ? q->title.substr(0, 22) : q->title;
      P.textS(Pix::W - 6, 112, t, 1, kGold, 2);
      if (!obj.empty()) P.textS(Pix::W - 6, 121, obj, 1, kText, 2);
      // off-screen arrow toward the objective
      int tx, ty;
      if (!g.inside && g.questTarget(q->id, tx, ty)) {
        Vec2 tgt(tx * 16 + 8.0f, ty * 16 + 8.0f);
        Vec2 sc = tgt - Vec2(std::floor(cam_.x), std::floor(cam_.y));
        bool on = sc.x > 10 && sc.y > 10 && sc.x < Pix::W - 10 && sc.y < Pix::H - 10;
        float dist = len(tgt - p.p) / 16;
        if (!on) {
          Vec2 c(Pix::W / 2.0f, Pix::H / 2.0f);
          Vec2 d = norm(sc - c);
          float k = std::min((Pix::W / 2.0f - 18) / std::max(0.01f, std::fabs(d.x)), (Pix::H / 2.0f - 18) / std::max(0.01f, std::fabs(d.y)));
          Vec2 a = c + d * k;
          // slide along the screen edge out of the HUD corners (vitals top-left, minimap + quest column top-right)
          if (a.x < 150 && a.y < 58) { if (a.y <= 20) a.x = 150; else a.y = 58; }
          if (a.x > Pix::W - 160 && a.y < 140) { if (a.y <= 20) a.x = Pix::W - 160; else a.y = 140; }
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
          P.textS(a.x - d.x * back, a.y - d.y * back - 3, std::to_string((int)dist), 1, kGold, 1);
        } else {
          float bob = std::sin(t_ * 5) * 2;
          P.rect(sc.x - 1, sc.y - 26 + bob, 3, 7, kGold);
          P.rect(sc.x - 1, sc.y - 17 + bob, 3, 2, kGold);
        }
      }
    }
  }

  // boss bar
  for (const Actor& a : g.actors) {
    if (!a.boss || a.st == AState::Dead || !a.aggro) continue;
    if (len2(a.p - p.p) > 260 * 260) continue;
    float w = 200, x = (Pix::W - w) / 2, y = Pix::H - 22;
    P.textS(Pix::W / 2, y - 10, a.name, 1, Color(1, 0.75f, 0.6f), 1);
    P.rect(x - 1, y - 1, w + 2, 6, Color(0, 0, 0, 0.85f));
    P.rect(x, y, w * clampf(a.hp / a.maxHp, 0, 1), 4, Color(0.75f, 0.12f, 0.1f));
    break;
  }

  // NPC name / interact prompt
  int it = g.mode == Mode::Play ? g.interactTarget() : -1;
  if (it >= 0) {
    for (const Actor& a : g.actors)
      if (a.id == it) {
        Vec2 s = a.p - Vec2(std::floor(cam_.x), std::floor(cam_.y));
        P.textS(s.x, s.y - 34, a.name, 1, kText, 1);
        if (!touchUI) P.textS(s.x, s.y - 43, "E: TALK", 1, kGold, 1);
      }
  } else if (g.mode == Mode::Play) {
    int ptx = 0, pty = 0, pr = usablePropAt(g, ptx, pty);
    if (pr) {
      Vec2 s = Vec2(ptx * 16 + 8.0f, pty * 16 - 4.0f) - Vec2(std::floor(cam_.x), std::floor(cam_.y));
      P.textS(s.x, s.y - 10, std::string(touchUI ? "TAP: " : "E: ") + useVerb(pr), 1, kGold, 1);
    }
  }

  // toasts (bottom-left)
  float ty = Pix::H - 14;
  for (int i = (int)toasts_.size() - 1; i >= 0; i--) {
    const Toast& t = toasts_[i];
    float a = clampf(4.0f - t.t, 0, 1);
    P.textS(7, ty + 1, t.s, 1, Color(0, 0, 0, a * 0.6f));
    P.textS(6, ty, t.s, 1, Color(t.c.r, t.c.g, t.c.b, a));
    ty -= 10;
    if (ty < Pix::H - 70) break;
  }
  if (g.noticeT > 0) {
    float a = clampf(g.noticeT, 0, 1);
    P.textS(Pix::W / 2 + 1, Pix::H - 41, g.notice, 1, Color(0, 0, 0, a * 0.6f), 1);
    P.textS(Pix::W / 2, Pix::H - 42, g.notice, 1, Color(1, 0.95f, 0.85f, a), 1);
  }
  // banner
  if (bannerT_ > 0) {
    float a = clampf(std::min(bannerT_, 4.0f - bannerT_) * 2.5f, 0, 1);
    float y = 56;
    P.rect(0, y - 6, Pix::W, 34, Color(0, 0, 0, 0.45f * a));
    P.rect(Pix::W / 2 - 90, y - 6, 180, 1, Color(kGold.r, kGold.g, kGold.b, a * 0.8f));
    P.rect(Pix::W / 2 - 90, y + 27, 180, 1, Color(kGold.r, kGold.g, kGold.b, a * 0.8f));
    P.textS(Pix::W / 2, y, banner_, 1, Color(kGold.r, kGold.g, kGold.b, a), 1);
    P.textS(Pix::W / 2 + 1, y + 11, bannerSub_, 2, Color(0, 0, 0, a * 0.5f), 1);
    P.textS(Pix::W / 2, y + 10, bannerSub_, 2, Color(1, 0.97f, 0.9f, a), 1);
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
    P.blitEx(light_, 0, 0, 64, 64, stick_.start.x - 30, stick_.start.y - 30, 60, 60, false, Color(0.9f, 0.9f, 1, 0.25f));
    P.blitEx(light_, 0, 0, 64, 64, stick_.start.x + d.x - 14, stick_.start.y + d.y - 14, 28, 28, false, Color(1, 1, 1, 0.6f));
  } else {
    P.blitEx(light_, 0, 0, 64, 64, 60 - 26, 215 - 26, 52, 52, false, Color(0.9f, 0.9f, 1, 0.12f));
  }
  int target = g.interactTarget();
  int utx = 0, uty = 0, usePr = target < 0 ? usablePropAt(g, utx, uty) : 0;
  for (int b = 0; b < B_MENU; b++) {
    const BtnDef& d = kBtn[b];
    bool held = false;
    for (auto& f : fingers_) if (f.on && f.button == b) held = true;
    Color ring = held ? Color(1, 0.9f, 0.6f, 0.9f) : Color(0.9f, 0.85f, 0.75f, 0.45f);
    P.blitEx(light_, 0, 0, 64, 64, d.x - d.r * 1.3f, d.y - d.r * 1.3f, d.r * 2.6f, d.r * 2.6f, false, Color(0.05f, 0.04f, 0.06f, 0.0f));
    // disc
    for (int yy = (int)-d.r; yy <= (int)d.r; yy++) {
      float hw = std::sqrt(std::max(0.0f, d.r * d.r - yy * yy));
      P.rect(d.x - hw, d.y + yy, hw * 2, 1, Color(0.08f, 0.07f, 0.09f, held ? 0.65f : 0.42f));
    }
    for (int k = 0; k < 40; k++) { float a = k / 40.0f * TAU; P.rect(d.x + std::cos(a) * d.r, d.y + std::sin(a) * d.r, 1, 1, ring); }
    switch (b) {
      case B_ATTACK:
        if (target >= 0) P.text(d.x, d.y - 3, "TALK", 1, kGold, 1);
        else if (usePr) P.text(d.x, d.y - 3, useVerb(usePr), 1, kGold, 1);
        else P.blitEx(iconTex(art::Icon::Sword, g.eqWeapon >= 0 ? g.inv[g.eqWeapon].tint : 0), 0, 0, 16, 16, d.x - 12, d.y - 12, 24, 24);
        break;
      case B_BOW: P.blit(iconTex(art::Icon::Bow, 0), d.x - 8, d.y - 8); break;
      case B_SPELL: P.blit(iconTex(g.spell == Spell::Heal ? art::Icon::PotionGreen : (g.spell == Spell::IceSpike ? art::Icon::Gem : art::Icon::Staff), g.spell == Spell::Flames ? rgba(255, 120, 40) : rgba(120, 200, 255)), d.x - 8, d.y - 8); break;
      case B_ROLL: P.text(d.x, d.y - 3, "ROLL", 1, kText, 1); break;
      case B_POTION: P.blit(iconTex(art::Icon::PotionRed, 0), d.x - 8, d.y - 8); break;
      default: break;
    }
  }
}

void View::bakeWorldMap(Game& g) {
  const Map& m = g.world.over;
  int W = m.w / 2, H = m.h / 2;
  Canvas c(W, H);
  for (int y = 0; y < H; y++)
    for (int x = 0; x < W; x++) {
      Ground gr = m.at(x * 2, y * 2);
      uint32_t k;
      switch (gr) {
        case Ground::DeepWater: k = rgba(56, 84, 120); break;
        case Ground::Water: k = rgba(80, 116, 150); break;
        case Ground::Sand: k = rgba(214, 196, 150); break;
        case Ground::Snow: k = rgba(232, 232, 228); break;
        case Ground::Rock: k = rgba(128, 116, 100); break;
        case Ground::Road: case Ground::Bridge: k = rgba(120, 92, 60); break;
        case Ground::Plaza: k = rgba(150, 120, 90); break;
        case Ground::Swamp: k = rgba(118, 124, 88); break;
        case Ground::ForestFloor: k = rgba(108, 132, 84); break;
        case Ground::Autumn: k = rgba(176, 136, 84); break;
        case Ground::Tundra: k = rgba(150, 156, 128); break;
        default: k = rgba(150, 168, 110); break;
      }
      if (m.prop[(size_t)y * 2 * m.w + x * 2] && gr != Ground::Road) k = art::shade(k, 0.88f);
      if (gr == Ground::Rock) { float l = vnoise(x / 3.0f, y / 3.0f, 9); k = art::shade(k, 0.85f + l * 0.35f); }
      // parchment grain
      float n = hashf(x, y, 77);
      k = art::mix(k, rgba(222, 202, 160), 0.18f + n * 0.06f);
      c.set(x, y, k);
    }
  worldMap_ = pix_->bake(c);
  worldMapBaked_ = true;
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
          size_t i = (size_t)ty * m.w + tx;
          if (m.bldgAt[i] >= 0) k = rgba(150, 70, 56);
          else if (m.wall[i]) k = rgba(70, 66, 66);
          else if (m.solid[i]) k = art::shade(k, 0.7f);
        }
        miniPx_[(size_t)yy * 64 + xx] = k;
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
  P.rect(x + (ptx - ox) * size / 64.0f - 1, y + (pty - oy) * size / 64.0f - 1, 3, 3, Color(1, 1, 1));
  int tx, ty;
  if (!g.inside && g.trackedQuest >= 0 && g.questTarget(g.trackedQuest, tx, ty)) {
    int ax = std::clamp(tx - ox, 1, 62), ay = std::clamp(ty - oy, 1, 62);
    if (((int)(t_ * 3)) & 1) P.rect(x + ax - 1, y + ay - 1, 3, 3, kGold);
  }
}

void View::drawWorldMap(Game& g, float x, float y, float w, float h) {
  Pix& P = *pix_;
  if (!worldMapBaked_) bakeWorldMap(g);
  float sc = std::min(w / worldMap_.w, h / worldMap_.h);
  float mw = worldMap_.w * sc, mh = worldMap_.h * sc;
  float ox = x + (w - mw) / 2, oy = y + (h - mh) / 2;
  P.blitEx(worldMap_, 0, 0, worldMap_.w, worldMap_.h, ox, oy, mw, mh);
  P.frame(ox - 1, oy - 1, mw + 2, mh + 2, Color(0.4f, 0.3f, 0.2f));
  float k = sc / 2;   // tiles -> map px
  for (int i = 0; i < (int)g.world.sites.size(); i++) {
    const Site& s = g.world.sites[i];
    if (!s.discovered) continue;
    float sx = ox + s.ex * k, sy = oy + s.ey * k;
    Color c;
    float r = 1;
    switch (s.type) {
      case SiteType::City: c = Color(0.95f, 0.85f, 0.4f); r = 3; break;
      case SiteType::Town: c = Color(0.95f, 0.85f, 0.55f); r = 2; break;
      case SiteType::Village: c = Color(0.9f, 0.85f, 0.7f); r = 1.5f; break;
      case SiteType::Cave: c = Color(0.2f, 0.15f, 0.1f); r = 1.5f; break;
      case SiteType::Ruin: c = s.mainQuest ? Color(1, 0.45f, 0.1f) : Color(0.45f, 0.3f, 0.55f); r = 1.5f; break;
      case SiteType::BanditCamp: c = Color(0.75f, 0.15f, 0.12f); r = 1.5f; break;
      case SiteType::Shrine: c = Color(0.4f, 0.7f, 1); r = 1.5f; break;
      case SiteType::DragonLair: c = Color(1, 0.3f, 0.1f); r = 2.5f; break;
      default: break;
    }
    P.rect(sx - r - 1, sy - r - 1, r * 2 + 2, r * 2 + 2, Color(0.1f, 0.06f, 0.04f));
    P.rect(sx - r, sy - r, r * 2, r * 2, c);
    if (s.cleared && (s.type == SiteType::Cave || s.type == SiteType::Ruin || s.type == SiteType::BanditCamp)) P.rect(sx - 0.5f, sy - 0.5f, 1, 1, Color(1, 1, 1));
    if (i == mapSel_) P.frame(sx - r - 3, sy - r - 3, r * 2 + 6, r * 2 + 6, Color(1, 1, 1));
    if (s.type == SiteType::City) P.text(sx, sy + 5, s.name, 1, Color(0.18f, 0.12f, 0.08f), 1);
  }
  // player
  Vec2 pp = g.pl().p;
  if (g.inside) {
    if (g.subSite >= 0) pp = Vec2(g.world.sites[g.subSite].ex * 16.0f, g.world.sites[g.subSite].ey * 16.0f);
    else if (g.subBldg >= 0) pp = Vec2(g.world.over.bldgs[g.subBldg].doorX() * 16.0f, g.world.over.bldgs[g.subBldg].doorY() * 16.0f);
  }
  float px = ox + pp.x / 16 * k, py = oy + pp.y / 16 * k;
  if (((int)(t_ * 3)) & 1) { P.rect(px - 2, py, 5, 1, Color(1, 1, 1)); P.rect(px, py - 2, 1, 5, Color(1, 1, 1)); }
  int tx, ty;
  if (g.trackedQuest >= 0 && g.questTarget(g.trackedQuest, tx, ty)) {
    float qx = ox + tx * k, qy = oy + ty * k;
    P.rect(qx - 1, qy - 7, 3, 4, kGold);
    P.rect(qx, qy - 3, 1, 2, kGold);
  }
}

// ------------------------------------------------------------------ menus
void View::drawMenu(Game& g) {
  Pix& P = *pix_;
  P.rect(0, 0, Pix::W, Pix::H, Color(0, 0, 0, 0.55f));
  panel(8, 6, Pix::W - 16, Pix::H - 12);
  // tabs
  float tw = (Pix::W - 40) / (float)NTABS;
  for (int i = 0; i < NTABS; i++) button(20 + i * tw, 12, tw - 4, 14, kTabs[i], i == menuTab_);
  button(Pix::W - 36, 30, 20, 12, "X", false);
  float top = 34;
  switch (menuTab_) {
    case 0: {   // items
      int n = (int)g.inv.size();
      menuSel_ = std::clamp(menuSel_, 0, std::max(0, n - 1));
      int rows = 17;
      if (menuSel_ < menuScroll_) menuScroll_ = menuSel_;
      if (menuSel_ >= menuScroll_ + rows) menuScroll_ = menuSel_ - rows + 1;
      for (int r = 0; r < rows && menuScroll_ + r < n; r++) {
        int i = menuScroll_ + r;
        const Item& it = g.inv[i];
        float y = top + 12 + r * 12;
        if (i == menuSel_) P.rect(18, y - 2, 236, 12, Color(0.3f, 0.22f, 0.12f, 0.8f));
        P.blitEx(iconTex(it.icon, it.tint), 0, 0, 16, 16, 20, y - 2, 12, 12);
        std::string name = it.name;
        if (name.size() > 30) name = name.substr(0, 30);
        if (it.count > 1) name += " (" + std::to_string(it.count) + ")";
        P.text(36, y, name, 1, col(rarityColor(it.rarity)));
        if (equipped(g, i)) P.text(246, y, "E", 1, kGold, 2);
      }
      if (n == 0) P.text(30, top + 14, "YOUR PACK IS EMPTY", 1, kDim);
      if (n > rows) P.text(136, Pix::H - 18, std::to_string(menuScroll_ + 1) + "-" + std::to_string(std::min(n, menuScroll_ + rows)) + " OF " + std::to_string(n), 1, kDim, 1);
      // detail
      P.rect(262, top + 8, 1, Pix::H - top - 24, Color(0.4f, 0.32f, 0.2f));
      if (n > 0) {
        const Item& it = g.inv[menuSel_];
        P.blitEx(iconTex(it.icon, it.tint), 0, 0, 16, 16, 272, top + 12, 32, 32);
        wrapText(310, top + 14, 150, it.name, col(rarityColor(it.rarity)));
        const char* rn[] = {"COMMON", "UNCOMMON", "RARE", "EPIC", "LEGENDARY"};
        P.text(310, top + 36, rn[(int)it.rarity], 1, kDim);
        auto lines = wrap(itemStats(g, it), 30);
        for (size_t k = 0; k < lines.size(); k++) P.text(272, top + 54 + k * 10, lines[k], 1, kText);
        int eq = equippedOf(g, it.kind);
        if (eq >= 0 && eq != menuSel_ && eq < (int)g.inv.size()) {
          int diff = it.power - g.inv[eq].power;
          P.text(272, top + 104, std::string("VS EQUIPPED: ") + (diff >= 0 ? "+" : "") + std::to_string(diff), 1, diff >= 0 ? Color(0.5f, 1, 0.5f) : Color(1, 0.5f, 0.45f));
        }
        bool canEquip = it.kind <= ItemKind::Amulet;
        std::string use = canEquip ? (equipped(g, menuSel_) ? "UNEQUIP" : "EQUIP") : (it.kind == ItemKind::Potion || it.kind == ItemKind::Food || it.name.rfind("SPELL TOME", 0) == 0 ? "USE" : "");
        if (!use.empty()) button(272, Pix::H - 44, 84, 16, use + (touchUI ? "" : " (ENT)"), true);
        if (it.kind != ItemKind::Quest) button(364, Pix::H - 44, 84, 16, touchUI ? "DROP" : "DROP (X)", false);
      }
      break;
    }
    case 1: {   // quests
      std::vector<int> order;
      for (int i = 0; i < (int)g.quests.size(); i++) if (g.quests[i].state != QState::Done) order.push_back(i);
      for (int i = 0; i < (int)g.quests.size(); i++) if (g.quests[i].state == QState::Done) order.push_back(i);
      menuSel_ = std::clamp(menuSel_, 0, std::max(0, (int)order.size() - 1));
      for (int r = 0; r < (int)order.size() && r < 17; r++) {
        const Quest& q = g.quests[order[r]];
        float y = top + 12 + r * 12;
        if (r == menuSel_) P.rect(18, y - 2, 200, 12, Color(0.3f, 0.22f, 0.12f, 0.8f));
        Color c = q.state == QState::Done ? kDim : (q.type == QType::Main ? kGold : kText);
        std::string t = q.title.size() > 28 ? q.title.substr(0, 28) : q.title;
        P.text(22, y, (q.id == g.trackedQuest ? "> " : "  ") + t, 1, c);
      }
      P.rect(226, top + 8, 1, Pix::H - top - 24, Color(0.4f, 0.32f, 0.2f));
      if (!order.empty()) {
        const Quest& q = g.quests[order[menuSel_]];
        wrapText(236, top + 12, 220, q.title, q.type == QType::Main ? kGold : kText);
        std::string st = q.state == QState::Done ? "COMPLETED" : q.state == QState::Complete ? "READY TO TURN IN" : "IN PROGRESS";
        P.text(236, top + 24, st, 1, q.state == QState::Complete ? Color(0.5f, 1, 0.5f) : kDim);
        wrapText(236, top + 40, 220, q.desc, kText, -1, 10);
        if (q.type == QType::Hunt) P.text(236, Pix::H - 62, "PROGRESS " + std::to_string(q.have) + "/" + std::to_string(q.need), 1, kGold);
        if (q.type == QType::Main && q.stage == 1) P.text(236, Pix::H - 62, "EMBER SHARDS " + std::to_string(q.have) + "/3", 1, kGold);
        if (q.gold > 0 && q.state != QState::Done) P.text(236, Pix::H - 50, "REWARD " + std::to_string(q.gold) + " GOLD", 1, kGold);
        if (q.state != QState::Done) button(236, Pix::H - 38, 100, 16, q.id == g.trackedQuest ? "TRACKED" : "TRACK", q.id == g.trackedQuest);
      } else P.text(236, top + 14, "NO QUESTS YET", 1, kDim);
      break;
    }
    case 2: {   // map
      drawWorldMap(g, 16, top + 6, 300, Pix::H - top - 18);
      float x = 324;
      P.text(x, top + 10, "WORLD MAP", 1, kGold);
      if (mapSel_ >= 0 && mapSel_ < (int)g.world.sites.size()) {
        const Site& s = g.world.sites[mapSel_];
        wrapText(x, top + 26, 136, s.name, kText);
        P.text(x, top + 46, siteTypeName(s.type), 1, kDim);
        P.text(x, top + 56, "LEVEL " + std::to_string(s.level), 1, kDim);
        if (s.cleared) P.text(x, top + 66, "CLEARED", 1, Color(0.5f, 1, 0.5f));
        button(x, top + 84, 130, 18, "FAST TRAVEL", true);
      } else {
        wrapText(x, top + 26, 136, touchUI ? "TAP A DISCOVERED PLACE TO FAST TRAVEL." : "CLICK A PLACE, OR W/S TO PICK AND ENTER TO TRAVEL.", kDim);
      }
      // legend
      float ly = Pix::H - 74;
      struct L { Color c; const char* n; } leg[] = {{Color(0.95f, 0.85f, 0.4f), "CITY / TOWN"}, {Color(0.2f, 0.15f, 0.1f), "CAVE"}, {Color(0.45f, 0.3f, 0.55f), "RUIN"},
                                                    {Color(1, 0.45f, 0.1f), "EMBER SHARD"}, {Color(0.75f, 0.15f, 0.12f), "BANDITS"}, {Color(0.4f, 0.7f, 1), "SHRINE"}};
      for (int i = 0; i < 6; i++) { P.rect(x, ly + i * 9, 5, 5, leg[i].c); P.text(x + 9, ly + i * 9 - 1, leg[i].n, 1, kDim); }
      break;
    }
    case 3: {   // hero
      const Actor& p = g.pl();
      float x = 26, y = top + 14;
      P.text(x, y, "LEVEL " + std::to_string(g.plLevel), 2, kGold);
      P.text(x, y + 20, "XP " + std::to_string(g.plXp) + " / " + std::to_string(g.xpForNext()), 1, kText);
      auto stat = [&](float yy, const std::string& n, const std::string& v) { P.text(x, yy, n, 1, kDim); P.text(x + 110, yy, v, 1, kText); };
      stat(y + 36, "HEALTH", std::to_string((int)p.hp) + " / " + std::to_string((int)p.maxHp));
      float mpMax = g.maxMp, stMax = g.maxSt;   // include enchantment bonuses, same as the HUD bars
      for (int idx : {g.eqArmor, g.eqHelmet, g.eqShield, g.eqRing, g.eqAmulet}) {
        if (idx < 0 || idx >= (int)g.inv.size()) continue;
        if (g.inv[idx].ench == Ench::Magicka) mpMax += g.inv[idx].enchPow;
        if (g.inv[idx].ench == Ench::Stamina) stMax += g.inv[idx].enchPow;
      }
      stat(y + 46, "MAGICKA", std::to_string((int)g.mp) + " / " + std::to_string((int)mpMax));
      stat(y + 56, "STAMINA", std::to_string((int)g.stamina) + " / " + std::to_string((int)stMax));
      stat(y + 70, "WEAPON DAMAGE", std::to_string((int)g.weaponDamage()));
      stat(y + 80, "ARMOR", std::to_string((int)g.armorRating()));
      stat(y + 94, "GOLD", std::to_string(g.gold));
      stat(y + 104, "KILLS", std::to_string(g.kills));
      stat(y + 114, "DUNGEONS CLEARED", std::to_string(g.dungeonsCleared));
      std::string sp;
      for (int s = 0; s < (int)Spell::COUNT; s++) if (g.spellsKnown & (1 << s)) { if (!sp.empty()) sp += ", "; sp += spellName((Spell)s); }
      stat(y + 128, "SPELLS", sp);
      // portrait
      const Tex& t = humanTex(p.look);
      P.blitEx(t, 0, 0, art::HUMAN_W, art::HUMAN_H, 300, top + 20, art::HUMAN_W * 4.0f, art::HUMAN_H * 4.0f);
      if (g.perkPts > 0) {
        P.text(360, top + 20, "LEVEL UP!", 1, kGold, 1);
        P.text(360, top + 32, "CHOOSE A STAT", 1, kText, 1);
        button(310, top + 120, 100, 18, "+12 HEALTH", levelSel_ == 0);
        button(310, top + 142, 100, 18, "+12 MAGICKA", levelSel_ == 1);
        button(310, top + 164, 100, 18, "+12 STAMINA", levelSel_ == 2);
      }
      break;
    }
    case 4: {   // system
      button(Pix::W / 2 - 70, top + 40, 140, 20, "SAVE GAME", menuSel_ == 0);
      button(Pix::W / 2 - 70, top + 68, 140, 20, touchUI ? "TOUCH CONTROLS: ON" : "TOUCH CONTROLS: OFF", menuSel_ == 1);
      button(Pix::W / 2 - 70, top + 96, 140, 20, "SAVE AND QUIT", menuSel_ == 2);
      P.text(Pix::W / 2, top + 140, "WORLD SEED " + std::to_string(g.seed), 1, kDim, 1);
      if (!touchUI) {
        const char* keys[] = {"WASD MOVE   J/SPACE ATTACK   K BOW   L SPELL", "SHIFT ROLL   E TALK/OPEN   Q POTION   R SWAP SPELL", "TAB MENU   M MAP   ESC PAUSE   F11 FULLSCREEN"};
        for (int i = 0; i < 3; i++) P.text(Pix::W / 2, top + 160 + i * 11, keys[i], 1, kDim, 1);
      }
      break;
    }
  }
}

void View::drawDialogue(Game& g) {
  Pix& P = *pix_;
  const Dialogue& d = g.dlg;
  float x = 20, w = Pix::W - 40;
  int nOpt = (int)d.opts.size();
  float h = 58 + nOpt * 13;
  float y = Pix::H - h - 8;
  panel(x, y, w, h, 0.94f);
  P.text(x + 10, y + 8, d.speaker, 1, kGold);
  wrapText(x + 10, y + 20, w - 20, d.text, kText, (int)dlgChars_, 9);
  dlgSel_ = std::clamp(dlgSel_, 0, std::max(0, nOpt - 1));
  for (int i = 0; i < nOpt; i++) {
    float oy = y + h - 8 - (nOpt - i) * 13;
    bool hot = i == dlgSel_;
    if (hot) P.rect(x + 8, oy - 2, w - 16, 12, Color(0.32f, 0.24f, 0.12f, 0.85f));
    P.text(x + 14, oy, (hot ? "> " : "  ") + d.opts[i].label, 1, hot ? Color(1, 0.95f, 0.8f) : Color(0.8f, 0.75f, 0.65f));
  }
}

void View::drawShop(Game& g) {
  Pix& P = *pix_;
  P.rect(0, 0, Pix::W, Pix::H, Color(0, 0, 0, 0.5f));
  panel(8, 6, Pix::W - 16, Pix::H - 12);
  P.text(18, 14, g.shop.title, 1, kGold);
  P.blit(iconTex(art::Icon::Gold, 0), Pix::W - 100, 9);
  P.text(Pix::W - 82, 14, std::to_string(g.gold), 1, kGold);
  button(Pix::W - 36, 10, 20, 12, "X", false);
  P.text(20, 28, "BUY", 1, shopSide_ == 0 ? kGold : kDim);
  P.text(Pix::W / 2 + 6, 28, "SELL", 1, shopSide_ == 1 ? kGold : kDim);
  P.rect(Pix::W / 2, 26, 1, Pix::H - 44, Color(0.4f, 0.32f, 0.2f));
  auto list = [&](float x0, const std::vector<Item>& items, bool buying, int side) {
    int n = (int)items.size();
    int rows = 16;
    int sel = shopSide_ == side ? shopSel_ : -1;
    int scroll = sel >= rows ? sel - rows + 1 : 0;
    for (int r = 0; r < rows && scroll + r < n; r++) {
      int i = scroll + r;
      const Item& it = items[i];
      float y = 40 + r * 12;
      if (i == sel) P.rect(x0 - 2, y - 2, Pix::W / 2 - 22, 12, Color(0.3f, 0.22f, 0.12f, 0.8f));
      P.blitEx(iconTex(it.icon, it.tint), 0, 0, 16, 16, x0, y - 2, 12, 12);
      std::string nm = it.name.size() > 24 ? it.name.substr(0, 24) : it.name;
      if (it.count > 1 && it.kind != ItemKind::Arrows) nm += " x" + std::to_string(it.count);
      if (it.kind == ItemKind::Arrows) nm += " x" + std::to_string(it.count);
      P.text(x0 + 15, y, nm, 1, col(rarityColor(it.rarity)));
      int price = buying ? (it.kind == ItemKind::Arrows ? it.count : it.value) : (it.kind == ItemKind::Arrows ? std::max(1, it.count / 3) : std::max(1, it.value * 2 / 5));
      bool afford = !buying || g.gold >= price;
      P.text(x0 + Pix::W / 2 - 30, y, std::to_string(price), 1, afford ? kGold : Color(0.7f, 0.3f, 0.3f), 2);
    }
  };
  list(18, g.shop.stock, true, 0);
  list(Pix::W / 2 + 8, g.inv, false, 1);
  // detail line
  const std::vector<Item>& src = shopSide_ == 0 ? g.shop.stock : g.inv;
  if (shopSel_ >= 0 && shopSel_ < (int)src.size()) {
    std::string s = itemStats(g, src[shopSel_]);
    std::replace(s.begin(), s.end(), '\n', ' ');
    if (s.size() > 74) s = s.substr(0, 74);
    P.text(Pix::W / 2, Pix::H - 30, s, 1, kText, 1);
  }
  P.text(Pix::W / 2, Pix::H - 19, touchUI ? "TAP AN ITEM TO BUY OR SELL" : "ARROWS SELECT  ENTER BUY/SELL  LEFT/RIGHT SWITCH  ESC CLOSE", 1, kDim, 1);
}

void View::drawLevelUp(Game& g) { menuTab_ = 3; drawMenu(g); }

void View::drawTitle(Game& g, bool hasSave) {
  Pix& P = *pix_;
  (void)g;
  P.rect(0, 0, Pix::W, Pix::H, Color(0.02f, 0.02f, 0.05f, 0.35f));
  float y = 58;
  // ember glow behind the logo
  P.blitEx(light_, 0, 0, 64, 64, Pix::W / 2 - 160, y - 50, 320, 120, false, Color(1, 0.45f, 0.15f, 0.35f + 0.1f * std::sin(t_ * 2)), 1);
  const std::string title = "EMBERVALE";
  for (int k = 3; k >= 1; k--) P.text(Pix::W / 2 + k, y + k, title, 5, Color(0.12f, 0.04f, 0.02f, 0.5f), 1);
  P.text(Pix::W / 2, y, title, 5, Color(1, 0.86f, 0.55f), 1);
  P.text(Pix::W / 2, y + 2, title, 5, Color(1, 0.7f, 0.35f, 0.35f), 1);
  P.text(Pix::W / 2, y + 44, "A SAGA OF STEEL, SORCERY AND DRAGONFIRE", 1, Color(0.9f, 0.85f, 0.75f), 1);
  float by = 150;
  int n = hasSave ? 2 : 1;
  titleSel_ = std::clamp(titleSel_, 0, n - 1);
  if (hasSave) { button(Pix::W / 2 - 70, by, 140, 22, "CONTINUE", titleSel_ == 0); by += 30; }
  button(Pix::W / 2 - 70, by, 140, 22, "NEW ADVENTURE", titleSel_ == (hasSave ? 1 : 0));
  P.text(Pix::W / 2, Pix::H - 14, "EVERY WORLD IS UNIQUE  -  PROCEDURALLY GENERATED", 1, Color(0.7f, 0.65f, 0.6f, 0.8f), 1);
  // embers rising
  for (int i = 0; i < 30; i++) {
    float ex = std::fmod(hashf(i, 0, 3) * 480 + std::sin(t_ + i) * 8, 480.0f);
    float ey = Pix::H - std::fmod(t_ * (12 + hashf(i, 1, 3) * 20) + hashf(i, 2, 3) * 270, 270.0f);
    P.rectAdd(ex, ey, 1, 1, Color(1, 0.5f, 0.2f, 0.6f));
  }
}

void View::drawDead(Game& g) {
  Pix& P = *pix_;
  (void)g;
  P.rect(0, 0, Pix::W, Pix::H, Color(0.15f, 0, 0, 0.55f));
  P.text(Pix::W / 2 + 2, 102, "YOU HAVE FALLEN", 3, Color(0, 0, 0, 0.6f), 1);
  P.text(Pix::W / 2, 100, "YOU HAVE FALLEN", 3, Color(0.9f, 0.25f, 0.2f), 1);
  // the prompt (and input) waits a beat, so mashing attack as you fall doesn't skip the moment
  if (modeT_ > 1.2f) P.text(Pix::W / 2, 140, touchUI ? "TAP TO RISE AGAIN" : "PRESS ENTER TO RISE AGAIN", 1, Color(kText.r, kText.g, kText.b, clampf((modeT_ - 1.2f) * 2, 0, 1)), 1);
}

// ------------------------------------------------------------------ input
void View::titleTap(Vec2 p) {
  float by = 150;
  if (std::fabs(p.x - Pix::W / 2) > 80) return;
  bool first = p.y >= by - 2 && p.y < by + 24, second = p.y >= by + 28 && p.y < by + 54;
  if (hasSave_ && first) wantContinue = true;
  else if ((!hasSave_ && first) || (hasSave_ && second)) wantNewGame = true;
}

int View::buttonAt(Vec2 p) const {
  for (int b = 0; b < B_COUNT; b++) {
    float r = kBtn[b].r + 6;
    if (len2(p - Vec2(kBtn[b].x, kBtn[b].y)) < r * r) return b;
  }
  return -1;
}

Input View::input(Game& g) {
  Input in;
  const bool* k = SDL_GetKeyboardState(nullptr);
  if (g.mode == Mode::Play) {
    if (k[SDL_SCANCODE_A] || k[SDL_SCANCODE_LEFT]) in.move.x -= 1;
    if (k[SDL_SCANCODE_D] || k[SDL_SCANCODE_RIGHT]) in.move.x += 1;
    if (k[SDL_SCANCODE_W] || k[SDL_SCANCODE_UP]) in.move.y -= 1;
    if (k[SDL_SCANCODE_S] || k[SDL_SCANCODE_DOWN]) in.move.y += 1;
    if (stick_.on) {
      Vec2 d = stick_.cur - stick_.start;
      float l = len(d);
      if (l > 4) in.move = d * (1.0f / l) * std::min(1.0f, l / 18.0f);
      if (l > 34) stick_.start += d * ((l - 34) / l);
    }
    // gamepad-like hold: attack repeats while held on keyboard
    if (k[SDL_SCANCODE_J] || k[SDL_SCANCODE_SPACE]) in.attack = true;
    for (auto& f : fingers_) if (f.on && f.button == B_ATTACK && g.interactTarget() < 0) in.attack = true;
  }
  if (kAttack_) { in.attack = true; }
  in.bow = kBow_; in.spell = kSpell_; in.roll = kRoll_; in.interact = kUse_; in.potion = kPotion_; in.swapSpell = kSwap_;
  int utx = 0, uty = 0;
  if (in.attack && g.mode == Mode::Play && kAttack_ && (g.interactTarget() >= 0 || usablePropAt(g, utx, uty))) { in.attack = false; in.interact = true; }
  kAttack_ = kBow_ = kSpell_ = kRoll_ = kUse_ = kPotion_ = kSwap_ = false;
  return in;
}

void View::menuKey(Game& g, int key) {
  auto back = [&]() { g.mode = Mode::Play; audio_->play(Sfx::MenuBack); };
  if (g.mode == Mode::Menu || g.mode == Mode::LevelUp) {
    if (key == SDLK_ESCAPE || key == SDLK_TAB || key == SDLK_I) { back(); return; }
    if (key == SDLK_Q || key == SDLK_PAGEUP) { menuTab_ = (menuTab_ + NTABS - 1) % NTABS; menuSel_ = 0; audio_->play(Sfx::MenuMove); return; }
    if (key == SDLK_E || key == SDLK_PAGEDOWN) { menuTab_ = (menuTab_ + 1) % NTABS; menuSel_ = 0; audio_->play(Sfx::MenuMove); return; }
    if (menuTab_ == 3 && g.perkPts > 0) {
      if (key == SDLK_UP || key == SDLK_W) levelSel_ = (levelSel_ + 2) % 3;
      if (key == SDLK_DOWN || key == SDLK_S) levelSel_ = (levelSel_ + 1) % 3;
      if (key == SDLK_RETURN || key == SDLK_SPACE) g.chooseLevelUp(levelSel_);
      return;
    }
    if (menuTab_ == 2) {
      // keyboard fast travel: up/down cycle the discovered places (nearest first), enter travels
      if (key == SDLK_UP || key == SDLK_W || key == SDLK_DOWN || key == SDLK_S) {
        std::vector<int> list;
        for (int i = 0; i < (int)g.world.sites.size(); i++) if (g.world.sites[i].discovered) list.push_back(i);
        const Vec2 pp = g.pl().p;
        std::sort(list.begin(), list.end(), [&](int a, int b) {
          const Site& A = g.world.sites[a]; const Site& B = g.world.sites[b];
          return len2(Vec2(A.ex * 16.0f, A.ey * 16.0f) - pp) < len2(Vec2(B.ex * 16.0f, B.ey * 16.0f) - pp);
        });
        if (!list.empty()) {
          int at = (int)(std::find(list.begin(), list.end(), mapSel_) - list.begin());
          int n = (int)list.size();
          if (at >= n) at = (key == SDLK_UP || key == SDLK_W) ? 0 : -1;
          at = (key == SDLK_UP || key == SDLK_W) ? (at + n - 1) % n : (at + 1) % n;
          mapSel_ = list[at];
          audio_->play(Sfx::MenuMove);
        }
        return;
      }
      if ((key == SDLK_RETURN || key == SDLK_SPACE) && mapSel_ >= 0) {
        if (g.fastTravel(mapSel_)) { snap(g); mapSel_ = -1; } else g.mode = Mode::Play;
        return;
      }
    }
    if (key == SDLK_UP || key == SDLK_W) { menuSel_ = std::max(0, menuSel_ - 1); audio_->play(Sfx::MenuMove); }
    if (key == SDLK_DOWN || key == SDLK_S) { menuSel_++; audio_->play(Sfx::MenuMove); }
    if (key == SDLK_LEFT || key == SDLK_A) { menuTab_ = (menuTab_ + NTABS - 1) % NTABS; menuSel_ = 0; }
    if (key == SDLK_RIGHT || key == SDLK_D) { menuTab_ = (menuTab_ + 1) % NTABS; menuSel_ = 0; }
    if (key == SDLK_RETURN || key == SDLK_SPACE) {
      if (menuTab_ == 0 && menuSel_ < (int)g.inv.size()) g.useItem(menuSel_);
      if (menuTab_ == 1) {
        std::vector<int> order;
        for (int i = 0; i < (int)g.quests.size(); i++) if (g.quests[i].state != QState::Done) order.push_back(i);
        if (menuSel_ < (int)order.size()) g.trackedQuest = g.quests[order[menuSel_]].id;
      }
      if (menuTab_ == 4) {
        if (menuSel_ == 0) { wantSave = true; banner_ = "GAME SAVED"; bannerSub_ = ""; bannerT_ = 2; }
        if (menuSel_ == 1) touchUI = !touchUI;
        if (menuSel_ == 2) { wantSave = true; wantsQuit = true; }
      }
    }
    if (key == SDLK_X && menuTab_ == 0 && menuSel_ < (int)g.inv.size() && g.inv[menuSel_].kind != ItemKind::Quest) g.dropItem(menuSel_);
    return;
  }
  if (g.mode == Mode::Dialogue) {
    if (key == SDLK_UP || key == SDLK_W) { dlgSel_ = std::max(0, dlgSel_ - 1); audio_->play(Sfx::MenuMove); }
    if (key == SDLK_DOWN || key == SDLK_S) { dlgSel_++; audio_->play(Sfx::MenuMove); }
    if (key == SDLK_RETURN || key == SDLK_SPACE || key == SDLK_E || key == SDLK_J) {
      auto lines = wrap(g.dlg.text, (Pix::W - 60) / 6);
      int total = 0; for (auto& l : lines) total += (int)l.size();
      if (dlgChars_ < total) { dlgChars_ = 9999; return; }
      int sel = dlgSel_;
      std::string before = g.dlg.text;
      g.dialogueChoose(sel);
      if (g.dlg.text != before) dlgChars_ = 0;
      dlgSel_ = 0;
    }
    if (key == SDLK_ESCAPE) g.closeDialogue();
    return;
  }
  if (g.mode == Mode::Shop) {
    const int n = shopSide_ == 0 ? (int)g.shop.stock.size() : (int)g.inv.size();
    if (key == SDLK_UP || key == SDLK_W) shopSel_ = std::max(0, shopSel_ - 1);
    if (key == SDLK_DOWN || key == SDLK_S) shopSel_ = std::min(std::max(0, n - 1), shopSel_ + 1);
    if (key == SDLK_LEFT || key == SDLK_A || key == SDLK_RIGHT || key == SDLK_D) { shopSide_ ^= 1; shopSel_ = 0; }
    if (key == SDLK_RETURN || key == SDLK_SPACE) {
      if (shopSide_ == 0) g.buy(shopSel_); else g.sell(shopSel_);
      int n2 = shopSide_ == 0 ? (int)g.shop.stock.size() : (int)g.inv.size();
      shopSel_ = std::min(shopSel_, std::max(0, n2 - 1));
    }
    if (key == SDLK_ESCAPE || key == SDLK_TAB) { g.mode = Mode::Play; audio_->play(Sfx::MenuBack); }
  }
}

void View::tap(Game& g, Vec2 p) {
  auto inR = [&](float x, float y, float w, float h) { return p.x >= x && p.y >= y && p.x < x + w && p.y < y + h; };
  if (g.mode == Mode::Title) { titleTap(p); return; }
  if (g.mode == Mode::Dead) { if (modeT_ > 1.2f) g.respawn(); return; }
  if (g.mode == Mode::Paused) { g.mode = Mode::Play; return; }
  if (g.mode == Mode::Dialogue) {
    const Dialogue& d = g.dlg;
    int nOpt = (int)d.opts.size();
    float h = 58 + nOpt * 13, y = Pix::H - h - 8;
    auto lines = wrap(g.dlg.text, (Pix::W - 60) / 6);
    int total = 0; for (auto& l : lines) total += (int)l.size();
    for (int i = 0; i < nOpt; i++) {
      float oy = y + h - 8 - (nOpt - i) * 13;
      if (inR(28, oy - 3, Pix::W - 56, 13)) {
        std::string before = g.dlg.text;
        g.dialogueChoose(i);
        if (g.dlg.text != before) dlgChars_ = 0;
        dlgSel_ = 0;
        return;
      }
    }
    if (dlgChars_ < total) dlgChars_ = 9999;
    return;
  }
  if (g.mode == Mode::Shop) {
    if (inR(Pix::W - 40, 6, 30, 20)) { g.mode = Mode::Play; return; }
    for (int side = 0; side < 2; side++) {
      float x0 = side == 0 ? 18 : Pix::W / 2 + 8;
      int n = side == 0 ? (int)g.shop.stock.size() : (int)g.inv.size();
      int sel = shopSide_ == side ? shopSel_ : -1;
      int scroll = sel >= 16 ? sel - 15 : 0;
      for (int r = 0; r < 16 && scroll + r < n; r++) {
        float y = 40 + r * 12;
        if (inR(x0 - 2, y - 2, Pix::W / 2 - 22, 12)) {
          int i = scroll + r;
          if (shopSide_ == side && shopSel_ == i) { if (side == 0) g.buy(i); else g.sell(i); }
          shopSide_ = side; shopSel_ = i;
          int n2 = side == 0 ? (int)g.shop.stock.size() : (int)g.inv.size();
          shopSel_ = std::min(shopSel_, std::max(0, n2 - 1));
          return;
        }
      }
    }
    return;
  }
  if (g.mode == Mode::Menu || g.mode == Mode::LevelUp) {
    float tw = (Pix::W - 40) / (float)NTABS;
    for (int i = 0; i < NTABS; i++) if (inR(20 + i * tw, 12, tw - 4, 14)) { menuTab_ = i; menuSel_ = 0; mapSel_ = -1; audio_->play(Sfx::MenuMove); return; }
    if (inR(Pix::W - 40, 26, 30, 20)) { g.mode = Mode::Play; return; }
    float top = 34;
    switch (menuTab_) {
      case 0: {
        int n = (int)g.inv.size();
        for (int r = 0; r < 17 && menuScroll_ + r < n; r++) {
          float y = top + 12 + r * 12;
          if (inR(18, y - 2, 236, 12)) {
            int i = menuScroll_ + r;
            if (i == menuSel_) g.useItem(i);
            menuSel_ = i;
            return;
          }
        }
        if (n > 0 && inR(272, Pix::H - 44, 84, 16)) g.useItem(menuSel_);
        if (n > 0 && inR(364, Pix::H - 44, 84, 16) && g.inv[menuSel_].kind != ItemKind::Quest) g.dropItem(menuSel_);
        // scroll by tapping the list edges
        if (inR(18, top + 4, 236, 10)) menuScroll_ = std::max(0, menuScroll_ - 5), menuSel_ = std::max(0, menuSel_ - 5);
        if (inR(18, Pix::H - 22, 236, 12)) menuSel_ = std::min(n - 1, menuSel_ + 5);
        break;
      }
      case 1: {
        std::vector<int> order;
        for (int i = 0; i < (int)g.quests.size(); i++) if (g.quests[i].state != QState::Done) order.push_back(i);
        for (int i = 0; i < (int)g.quests.size(); i++) if (g.quests[i].state == QState::Done) order.push_back(i);
        for (int r = 0; r < (int)order.size() && r < 17; r++) if (inR(18, top + 10 + r * 12, 200, 12)) { menuSel_ = r; return; }
        if (menuSel_ < (int)order.size() && inR(236, Pix::H - 38, 100, 16)) g.trackedQuest = g.quests[order[menuSel_]].id;
        break;
      }
      case 2: {
        if (mapSel_ >= 0 && inR(324, top + 84, 130, 18)) {
          if (g.fastTravel(mapSel_)) { snap(g); mapSel_ = -1; }
          else { g.mode = Mode::Play; }
          return;
        }
        if (!worldMapBaked_) bakeWorldMap(g);
        float w = 300, h = Pix::H - top - 18, x = 16, y = top + 6;
        float sc = std::min(w / worldMap_.w, h / worldMap_.h);
        float ox = x + (w - worldMap_.w * sc) / 2, oy = y + (h - worldMap_.h * sc) / 2;
        float k = sc / 2;
        int best = -1; float bd = 10 * 10;
        for (int i = 0; i < (int)g.world.sites.size(); i++) {
          const Site& s = g.world.sites[i];
          if (!s.discovered) continue;
          float d = len2(Vec2(ox + s.ex * k, oy + s.ey * k) - p);
          if (d < bd) { bd = d; best = i; }
        }
        if (best >= 0 || inR(x, y, w, h)) mapSel_ = best;
        break;
      }
      case 3:
        if (g.perkPts > 0) {
          for (int i = 0; i < 3; i++) if (inR(310, top + 120 + i * 22, 100, 18)) { g.chooseLevelUp(i); levelSel_ = i; }
        }
        break;
      case 4:
        if (inR(Pix::W / 2 - 70, top + 40, 140, 20)) { wantSave = true; banner_ = "GAME SAVED"; bannerSub_ = ""; bannerT_ = 2; }
        if (inR(Pix::W / 2 - 70, top + 68, 140, 20)) touchUI = !touchUI;
        if (inR(Pix::W / 2 - 70, top + 96, 140, 20)) { wantSave = true; wantsQuit = true; }
        break;
    }
    return;
  }
}

void View::event(const SDL_Event& e, Game& g) {
  auto logical = [&](float wx, float wy) { float lx, ly; pix_->windowToLogical(wx, wy, lx, ly); return Vec2(lx, ly); };
  switch (e.type) {
    case SDL_EVENT_KEY_DOWN: {
      if (e.key.repeat && g.mode == Mode::Play) break;
      SDL_Keycode k = e.key.key;
      touchUI = false;
      if (g.mode == Mode::Title) {
        if (k == SDLK_UP || k == SDLK_W) titleSel_ = std::max(0, titleSel_ - 1);
        if (k == SDLK_DOWN || k == SDLK_S) titleSel_ = std::min(hasSave_ ? 1 : 0, titleSel_ + 1);
        if (k == SDLK_RETURN || k == SDLK_SPACE) { if (hasSave_ && titleSel_ == 0) wantContinue = true; else wantNewGame = true; }
        if (k == SDLK_N) wantNewGame = true;
        if (k == SDLK_ESCAPE) wantsQuit = true;
        break;
      }
      if (g.mode == Mode::Dead) { if ((k == SDLK_RETURN || k == SDLK_SPACE || k == SDLK_E) && modeT_ > 1.2f) g.respawn(); break; }
      if (g.mode == Mode::Paused) { if (k == SDLK_ESCAPE || k == SDLK_RETURN || k == SDLK_SPACE) g.mode = Mode::Play; break; }
      if (g.mode != Mode::Play) { menuKey(g, k); break; }
      switch (k) {
        case SDLK_ESCAPE: g.mode = Mode::Paused; break;
        case SDLK_TAB: case SDLK_I: g.mode = Mode::Menu; menuTab_ = g.perkPts > 0 ? 3 : 0; menuSel_ = 0; audio_->play(Sfx::MenuSelect); break;
        case SDLK_M: g.mode = Mode::Menu; menuTab_ = 2; mapSel_ = -1; audio_->play(Sfx::MenuSelect); break;
        case SDLK_J: case SDLK_SPACE: kAttack_ = true; break;
        case SDLK_K: kBow_ = true; break;
        case SDLK_L: kSpell_ = true; break;
        case SDLK_LSHIFT: case SDLK_RSHIFT: kRoll_ = true; break;
        case SDLK_E: case SDLK_RETURN: kUse_ = true; break;
        case SDLK_Q: kPotion_ = true; break;
        case SDLK_R: kSwap_ = true; break;
        default: break;
      }
      break;
    }
    case SDL_EVENT_MOUSE_MOTION: mouse_ = logical(e.motion.x, e.motion.y); break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN: {
      if (e.button.which == SDL_TOUCH_MOUSEID) break;
      Vec2 p = logical(e.button.x, e.button.y);
      mouse_ = p;
      if (g.mode == Mode::Title) {
        titleTap(p);
        break;
      }
      if (g.mode == Mode::Play) {
        if (e.button.button == SDL_BUTTON_LEFT) kAttack_ = true;
        else if (e.button.button == SDL_BUTTON_RIGHT) kBow_ = true;
        break;
      }
      tap(g, p);
      break;
    }
    case SDL_EVENT_FINGER_DOWN: {
      touchUI = true;
      int ww, wh;
      SDL_GetWindowSize(pix_->window(), &ww, &wh);
      Vec2 p = logical(e.tfinger.x * ww, e.tfinger.y * wh);
      if (g.mode == Mode::Title) {
        titleTap(p);
        break;
      }
      if (g.mode != Mode::Play) { tap(g, p); break; }
      int b = buttonAt(p);
      Finger f; f.id = e.tfinger.fingerID; f.on = true; f.start = f.cur = p; f.button = b;
      if (b == B_MENU) { g.mode = Mode::Menu; menuTab_ = g.perkPts > 0 ? 3 : 0; menuSel_ = 0; audio_->play(Sfx::MenuSelect); break; }
      if (b == B_ATTACK) kAttack_ = true;
      else if (b == B_BOW) kBow_ = true;
      else if (b == B_SPELL) kSpell_ = true;
      else if (b == B_ROLL) kRoll_ = true;
      else if (b == B_POTION) kPotion_ = true;
      else if (p.x < Pix::W * 0.55f && !stick_.on) { stick_ = f; stick_.button = -2; }
      else if (b < 0) {
        // tap on the world: talk to / open what's there, otherwise attack
        int utx = 0, uty = 0;
        if (g.interactTarget() >= 0 || usablePropAt(g, utx, uty)) kUse_ = true; else kAttack_ = true;
      }
      fingers_.push_back(f);
      break;
    }
    case SDL_EVENT_FINGER_MOTION: {
      int ww, wh;
      SDL_GetWindowSize(pix_->window(), &ww, &wh);
      Vec2 p = logical(e.tfinger.x * ww, e.tfinger.y * wh);
      if (stick_.on && stick_.id == e.tfinger.fingerID) stick_.cur = p;
      for (auto& f : fingers_) if (f.id == e.tfinger.fingerID) f.cur = p;
      break;
    }
    case SDL_EVENT_FINGER_UP: case SDL_EVENT_FINGER_CANCELED: {
      if (stick_.on && stick_.id == e.tfinger.fingerID) stick_.on = false;
      for (size_t i = 0; i < fingers_.size();) if (fingers_[i].id == e.tfinger.fingerID) fingers_.erase(fingers_.begin() + i); else i++;
      break;
    }
    default: break;
  }
}
