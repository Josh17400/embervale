// M8b Treasure (VIEW lane; VISION_PLAN 15.27): the rarity ladder's drawing, written once for two painters: the game's
// (rpg/view/loot_ui.cpp: Pix rects, blends and the baked glow textures) and the loot gallery's (tools/preview/
// loot_gallery.cpp: a software Canvas), so the sheet the art is judged on is the screen's own pixels.
//
// A painter Pt offers:
//   rect(x, y, w, h, Color)           alpha-blended fill          add(x, y, w, h, Color)   additive fill
//   frame(x, y, w, h, Color)          1 px outline                text(x, y, s, Color, align)   the 5x7 font
//   int textW(s)                      6 px a letter - 1
//   beam(x, y, w, h, Color, sy, sh, add)   the beam glow: rows sy..sy+sh of the 16 x 64 column profile below, blended
//                                     over (add false: the halo keeps its true hue on any ground) or added (the core)
//   pool(x, y, w, h, Color)           an additive radial glow ((1 - d)^2, the game's light_ blob)
//   shadow(x, y, w, h, a)             the pickup's ground shadow
//   icon(const Item&, x, y, size)     the item's icon (itemTex / art::itemIconLook)
//   noted(Rarity)                     a rarity frame was drawn (the game's script counters; the gallery ignores it)
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>
#include "engine/color.h"
#include "rpg/culture/culture.h"
#include "rpg/sim/gear.h"
#include "rpg/sim/items.h"

namespace lootp {

constexpr float kPi = 3.14159265f;
inline Color colU(uint32_t c, float a = 1) { return Color((c & 255) / 255.0f, ((c >> 8) & 255) / 255.0f, ((c >> 16) & 255) / 255.0f, a); }
inline Color lerpC(Color a, Color b, float t) { return Color(a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t, a.a + (b.a - a.a) * t); }
inline Color shade(Color c, float k) { return Color(std::min(1.0f, c.r * k), std::min(1.0f, c.g * k), std::min(1.0f, c.b * k), c.a); }
inline Color withA(Color c, float a) { return Color(c.r, c.g, c.b, a); }
const Color kText(0.93f, 0.9f, 0.82f), kDim(0.62f, 0.58f, 0.52f), kGold(0.98f, 0.82f, 0.42f), kQuietFrame(0.42f, 0.34f, 0.22f);
const Color kAffix(0.62f, 0.80f, 1.0f), kWarn(0.98f, 0.78f, 0.36f), kQuiet(0.62f, 0.58f, 0.52f), kLost(0.86f, 0.46f, 0.42f);
const Color kUp(0.46f, 0.96f, 0.46f), kDown(1.0f, 0.42f, 0.36f), kPower(1.0f, 0.70f, 0.28f), kPowerWords(0.95f, 0.86f, 0.70f);

// the column profile of a beam (16 x 64, row 0 the top): gaussian across, full over its lower 30 %, fading toward the top, a short
// fade-in at the very foot. The game bakes it into a texture; the gallery evaluates it per pixel.
inline float beamProfile(float x, float y) {
  const float dx = (x - 7.5f) / 7.5f;
  const float across = std::exp(-dx * dx * 3.2f);
  const float v = std::clamp(y / 63.0f, 0.0f, 1.0f);
  return across * std::pow(std::min(1.0f, v / 0.7f), 1.25f) * std::min(1.0f, (63.0f - y) / 3.0f + 0.25f);
}
inline float poolProfile(float u, float v) {   // u, v in -1..1
  const float d = std::sqrt(u * u + v * v);
  return d < 1 ? (1 - d) * (1 - d) : 0.0f;
}

// the Mythic flame at a place on a frame or a beam (u 0 the root .. 1 the tip): hot yellow, orange, ember red, with the
// item's own shimmer (items.h rarityColorAt) running through it
inline Color flameAt(float t, float phase, float u) {
  const Color sh = colU(rarityColorAt(Rarity::Mythic, t, phase));
  const Color root(1.0f, 0.86f, 0.46f), mid(1.0f, 0.46f, 0.12f), tip(0.80f, 0.14f, 0.08f);
  const Color base = u < 0.5f ? lerpC(root, mid, u * 2) : lerpC(mid, tip, (u - 0.5f) * 2);
  return lerpC(base, sh, 0.45f);
}

// (M8b fixer r3, review: "a lone 'S' or 'YOU' on its own line") a number keeps its unit ("6 S", "+15 %"), and a last line
// of one short word pulls the word before it down with it
inline std::vector<std::string> wrap(const std::string& s, int maxChars) {
  maxChars = std::max(4, maxChars);
  std::vector<std::string> words;
  {
    std::string w;
    for (char c : s) {
      if (c == ' ') { if (!w.empty()) { words.push_back(w); w.clear(); } }
      else w += c;
    }
    if (!w.empty()) words.push_back(w);
  }
  auto isNum = [](const std::string& w) {
    bool dig = false;
    for (char c : w) {
      if (c >= '0' && c <= '9') dig = true;
      else if (c != '+' && c != '-' && c != '.' && c != '%' && c != 'X') return false;
    }
    return dig;
  };
  auto isUnit = [](const std::string& w) {
    std::string u = w;
    while (!u.empty() && (u.back() == '.' || u.back() == ',' || u.back() == ')' || u.back() == ';')) u.pop_back();
    return u == "S" || u == "M" || u == "%" || u == "SEC" || u == "SECS" || u == "SECOND" || u == "SECONDS" || u == "HP" || u == "TILES";
  };
  std::vector<std::string> toks;   // a token may hold a glued "6 S"
  for (size_t i = 0; i < words.size(); i++) {
    if (i + 1 < words.size() && isNum(words[i]) && isUnit(words[i + 1]) && (int)(words[i].size() + 1 + words[i + 1].size()) <= maxChars) {
      toks.push_back(words[i] + " " + words[i + 1]);
      i++;
    } else toks.push_back(words[i]);
  }
  std::vector<std::string> lines;
  std::string line;
  for (std::string word : toks) {
    while ((int)word.size() > maxChars) {   // a word longer than the line is cut (never runs past the column)
      if (!line.empty()) { lines.push_back(line); line.clear(); }
      lines.push_back(word.substr(0, (size_t)maxChars));
      word.erase(0, (size_t)maxChars);
    }
    if (word.empty()) continue;
    if (!line.empty() && (int)(line.size() + 1 + word.size()) > maxChars) { lines.push_back(line); line.clear(); }
    if (!line.empty()) line += ' ';
    line += word;
  }
  if (!line.empty()) lines.push_back(line);
  // no orphan: a short last line (one word, or a number and its unit) takes the previous line's last word too, when that leaves two words above
  if (lines.size() >= 2) {
    std::string& last = lines.back();
    std::string& prev = lines[lines.size() - 2];
    const size_t sp = prev.find_last_of(' ');
    if (last.size() <= 5 && sp != std::string::npos && prev.find(' ') < sp) {
      const std::string mv = prev.substr(sp + 1);
      if ((int)(mv.size() + 1 + last.size()) <= maxChars) { last = mv + " " + last; prev.resize(sp); }
    }
  }
  return lines;
}
// (M8b fixer r3, review: "TAKE OFF BRAMBLEHEART OF FIRST", "SABRE OF PRECISIO") a cut ends on a whole word and says so
// with "..", never on half a word or a dangling OF / THE
inline std::string fit(const std::string& s, float w) {
  const int n = std::max(1, (int)((w + 1) / 6));
  if ((int)s.size() <= n) return s;
  if (n < 4) return s.substr(0, (size_t)n);
  std::string t = s.substr(0, (size_t)(n - 2));
  if (s[(size_t)(n - 2)] != ' ') {   // cut inside a word: back to the last whole one, if any
    const size_t sp = t.find_last_of(' ');
    if (sp != std::string::npos) t.resize(sp);
  }
  for (;;) {   // no trailing space, no dangling joining word
    while (!t.empty() && t.back() == ' ') t.pop_back();
    const size_t sp = t.find_last_of(' ');
    const std::string tail = sp == std::string::npos ? std::string() : t.substr(sp + 1);
    if (tail == "OF" || tail == "THE" || tail == "AND" || tail == "VS") t.resize(sp);
    else break;
  }
  return t + "..";
}

// ---------------------------------------------------------------------------------------------------- text
// a name in its ladder colour; a Mythic one letter by letter in the moving flame over a deep ember shadow
template <class Pt> void rarityText(Pt& P, float t, float x, float y, const std::string& s, Rarity r, float alpha = 1, int align = 0, bool shadow = false) {
  if (r != Rarity::Mythic) {
    if (shadow) P.text(x + 1, y + 1, s, Color(0, 0, 0, 0.6f * alpha), align);
    P.text(x, y, s, colU(rarityColor(r), alpha), align);
    return;
  }
  const float w = (float)P.textW(s);
  const float x0 = align == 1 ? std::floor(x - w / 2) : align == 2 ? x - w : x;
  P.text(x0 + 1, y + 1, s, Color(0.22f, 0.03f, 0.02f, 0.85f * alpha), 0);
  for (size_t i = 0; i < s.size(); i++) {
    if (s[i] == ' ') continue;
    P.text(x0 + (float)i * 6, y, std::string(1, s[i]), colU(rarityColorAt(Rarity::Mythic, t, (float)i * 0.8f), alpha), 0);
  }
}

// ---------------------------------------------------------------------------------------------------- frames
// A rarity's frame round a slot or an icon, lit from the top-left like every sprite: the top and left edges catch the
// light, the bottom and right fall into the colour's shade, an outer rim of its dark keeps it off the panel. Common is
// the quiet panel frame; Epic adds corner ticks; Legendary gold corner gems and a slow glint; Mythic's ring is a flame
// running round the frame with ember licks rising off its top edge (never a plain blink). small: a list's icon cell.
template <class Pt> void rarityFrame(Pt& P, float t, float x, float y, float w, float h, Rarity r, bool hot = false, bool small = false) {
  x = std::floor(x); y = std::floor(y); w = std::floor(w); h = std::floor(h);
  P.noted(r);   // (the game counts the frames it draws: scripts check them)
  if (r == Rarity::Common) {
    P.frame(x, y, w, h, hot ? kGold : kQuietFrame);
    return;
  }
  const Color c = colU(rarityColor(r));
  if (r != Rarity::Mythic) {
    const Color lit = lerpC(c, Color(1, 1, 1), 0.30f), dk = shade(c, 0.55f), rim = shade(c, 0.28f);
    P.rect(x + 1, y + 1, w - 2, h - 2, withA(c, small ? 0.10f : 0.08f));   // the gem's glow behind the icon
    if (!small) P.frame(x - 1, y - 1, w + 2, h + 2, withA(rim, 0.9f));
    P.rect(x, y, w, 1, lit);
    P.rect(x, y + 1, 1, h - 1, lit);
    P.rect(x + 1, y + h - 1, w - 1, 1, dk);
    P.rect(x + w - 1, y + 1, 1, h - 2, dk);
    if (!small) {
      P.rect(x + 1, y + 1, w - 2, 1, withA(c, 0.45f));   // the bevel's inner line
      P.rect(x + 1, y + 2, 1, h - 3, withA(c, 0.45f));
      P.rect(x, y, 1, 1, Color(1, 1, 1, 0.9f));           // the top-left catch-light
    }
    if (hot) P.frame(x - 2, y - 2, w + 4, h + 4, withA(kGold, 0.85f));
    if (!small && r == Rarity::Epic) {   // corner ticks
      P.rect(x + 2, y + 2, 2, 1, lit); P.rect(x + 2, y + 2, 1, 2, lit);
      P.rect(x + w - 4, y + h - 3, 2, 1, c); P.rect(x + w - 3, y + h - 4, 1, 2, c);
    }
    if (!small && r == Rarity::Legendary) {   // gold gems at the corners, lit top-left, and a glint along the top
      const float cx[4] = {x - 1, x + w - 2, x - 1, x + w - 2}, cy[4] = {y - 1, y - 1, y + h - 2, y + h - 2};
      for (int k = 0; k < 4; k++) {
        P.rect(cx[k], cy[k], 3, 3, rim);
        P.rect(cx[k] + 1, cy[k], 1, 3, lit);
        P.rect(cx[k], cy[k] + 1, 3, 1, lit);
        P.rect(cx[k] + 1, cy[k] + 1, 1, 1, Color(1, 0.97f, 0.85f));
      }
      const float gx = std::fmod(t * 18.0f, w + 30) - 15;
      if (gx > 1 && gx < w - 2) P.add(x + std::floor(gx), y, 2, 1, Color(1, 0.95f, 0.75f, 0.8f));
    }
    return;
  }
  // ---- Mythic: a ring of fire. The main ring runs the flame round the frame (hotter along the top, the shimmer moving
  // along it); outside it an ember rim that glows and dims in patches; inside a thin hot line catching the top-left
  // light; ember licks rise off the top edge and flicker up the sides. Never a plain blink: every pixel moves on its own.
  P.rect(x + 1, y + 1, w - 2, h - 2, Color(1.0f, 0.36f, 0.12f, (small ? 0.12f : 0.09f) + 0.04f * std::sin(t * 3.1f)));
  const int W = (int)w, H = (int)h;
  const int per = 2 * (W + H) - 4;
  auto ringAt = [&](int i, int ww, int hh, int& px, int& py) {
    if (i < ww) { px = i; py = 0; }
    else if (i < ww + hh - 1) { px = ww - 1; py = i - ww + 1; }
    else if (i < 2 * ww + hh - 2) { px = ww - 2 - (i - (ww + hh - 1)); py = hh - 1; }
    else { px = 0; py = hh - 2 - (i - (2 * ww + hh - 2)); }
  };
  if (!small) {   // the ember rim, one pixel out: dark red, glowing in patches that drift round
    const int pr = 2 * (W + 2 + H + 2) - 4;
    for (int i = 0; i < pr; i++) {
      int px, py;
      ringAt(i, W + 2, H + 2, px, py);
      const float gl = 0.5f + 0.5f * std::sin(t * 2.3f + (float)i * 0.31f) * std::sin(t * 1.7f - (float)i * 0.17f);
      P.rect(x - 1 + (float)px, y - 1 + (float)py, 1, 1, lerpC(Color(0.24f, 0.04f, 0.03f), Color(0.70f, 0.16f, 0.06f), gl * gl));
    }
  }
  for (int i = 0; i < per; i++) {
    int px, py;
    ringAt(i, W, H, px, py);
    const float u = 1.0f - (float)py / (float)std::max(1, H - 1);   // 0 at the foot, 1 at the top
    Color fc = flameAt(t, (float)i * 0.42f, 0.55f - 0.40f * u);      // the top burns hottest
    if (py == H - 1 || px == W - 1) fc = shade(fc, 0.80f);           // the top-left light still reads
    P.rect(x + (float)px, y + (float)py, 1, 1, fc);
  }
  if (!small) {   // the inner hot line along the top and left, a beat behind the ring
    for (int i = 1; i < W - 1; i++)
      P.rect(x + (float)i, y + 1, 1, 1, Color(1.0f, 0.78f, 0.36f, 0.30f + 0.30f * std::sin(t * 6.0f + (float)i * 0.5f)));
    for (int j = 2; j < H - 1; j++)
      P.rect(x + 1, y + (float)j, 1, 1, Color(1.0f, 0.62f, 0.24f, 0.20f + 0.20f * std::sin(t * 5.0f + (float)j * 0.6f)));
  }
  if (small) {   // a small frame: one tongue off the top edge
    const float cyc = t * 1.1f, life = cyc - std::floor(cyc);
    const float lx = std::floor(x + 2 + std::fmod(std::floor(cyc) * 0.618f, 1.0f) * (w - 5));
    const int lh = 1 + (life < 0.5f ? 1 : 0);
    for (int j = 0; j < lh; j++)
      P.rect(lx, y - 1 - (float)j, 1, 1, withA(j == 0 ? Color(1.0f, 0.84f, 0.42f) : flameAt(t, 0, 0.7f), (1.0f - life * 0.85f) * 0.95f));
  } else {
    // (M8b fix, review: "the ember licks are separate specks floating above the frame") a continuous flame fringe
    // rooted on the ember rim: every column of the top edge burns 0..4 px high (two travelling waves, so tongues rise,
    // merge and part), a hot root cooling to a red tip; the upper sides lick outward the same way, one pixel deep
    for (int i = 0; i < W; i++) {
      const float n = 0.5f + 0.5f * std::sin(t * 7.0f + (float)i * 0.9f) * std::sin(t * 4.3f - (float)i * 0.53f);
      const float n2 = 0.5f + 0.5f * std::sin(t * 11.0f + (float)i * 1.7f);
      const int lh = (int)(3.4f * n * n + 1.3f * n2 * n);
      for (int j = 0; j < lh; j++) {
        const float u = ((float)j + 0.5f) / (float)lh;
        const Color fc = j == 0 ? lerpC(Color(1.0f, 0.86f, 0.46f), flameAt(t, (float)i * 0.3f, 0.2f), 0.4f) : flameAt(t, (float)i * 0.3f, 0.25f + 0.70f * u);
        P.rect(x + (float)i, y - 2 - (float)j, 1, 1, withA(fc, 0.95f * (1.0f - 0.45f * u)));
      }
    }
    const int sideRows = std::max(2, H / 3);
    for (int j = 0; j < sideRows; j++)
      for (int sd = 0; sd < 2; sd++) {
        const float n = 0.5f + 0.5f * std::sin(t * 6.3f + (float)j * 1.1f + (float)sd * 2.0f);
        if (n < 0.55f) continue;
        const Color fc = flameAt(t, (float)j * 0.4f + (float)sd, 0.35f + 0.5f * (float)j / (float)sideRows);
        P.rect(sd ? x + w + 1 : x - 2, y + (float)j, 1, 1, withA(fc, (n - 0.55f) * 2.0f * (1.0f - (float)j / (float)sideRows)));
      }
  }
  if (hot) P.frame(x - 2, y - 2, w + 4, h + 4, withA(kGold, 0.85f));
}

// an item's icon in its rarity cell: the dark socket and the frame (Common pieces and goods: none, as before)
template <class Pt> void rarityIcon(Pt& P, float t, const Item& it, float x, float y, float size) {
  if (itemEquippable(it.kind) && it.rarity > Rarity::Common) {
    P.rect(x - 1, y - 1, size + 2, size + 2, Color(0.04f, 0.03f, 0.05f, 0.55f));
    rarityFrame(P, t, x - 1, y - 1, size + 2, size + 2, it.rarity, false, size <= 16);
  }
  P.icon(it, x, y, size);
}

// a small pixel arrow (7 x 6: a four-row head over a three-wide shaft, lit along its top, with a drop shadow): up green,
// down red; even: a short grey bar
template <class Pt> void arrow(Pt& P, float x, float y, int dir) {
  x = std::floor(x); y = std::floor(y);
  const Color sh(0.02f, 0.01f, 0.03f, 0.8f);
  if (dir == 0) {
    P.rect(x + 2, y + 3, 5, 2, sh);
    P.rect(x + 1, y + 2, 5, 2, Color(0.62f, 0.60f, 0.56f));
    return;
  }
  const Color c = dir > 0 ? kUp : kDown, lit = lerpC(c, Color(1, 1, 1), 0.4f), dk = shade(c, 0.62f);
  static const int rows[6][2] = {{3, 1}, {2, 3}, {1, 5}, {0, 7}, {2, 3}, {2, 3}};   // {x offset, width}, top to bottom (up)
  for (int pass = 0; pass < 2; pass++)
    for (int r = 0; r < 6; r++) {
      const int rr = dir > 0 ? r : 5 - r;
      const float o = pass == 0 ? 1.0f : 0.0f;
      const Color rc = pass == 0 ? sh : (r == 0 || r == 1) ? lit : (r == 3 ? c : (r >= 4 ? dk : c));
      P.rect(x + (float)rows[r][0] + o, y + (float)rr + o, (float)rows[r][1], 1, rc);
    }
}

// ---------------------------------------------------------------------------------------------------- the card
// What the card shows, for the scripts' checks (`expect lootcard ...`)
struct CardInfo {
  int rarity = -1, ilvl = 0, affixLines = 0, lostLines = 0, verdict = -2;   // verdict: 1 / 0 / -1, -2 none (WORN or no compare)
  bool power = false, compare = false, worn = false;
  std::string name;
};
// the world the card reads: the player's level (level sync), the attunement count, the maker cultures
struct CardCtx {
  int level = 1;
  int attuned = 0;
  const cult::Culture* maker = nullptr;
  // (M8b fixer r2) the piece cannot be worn now: 1 a great piece below level 10, 2 every attunement taken (blockName:
  // a worn great piece to take off first); flash: the player just tried (the line lights up)
  int block = 0;
  std::string blockName;
  bool flash = false;
};
// 1 better, -1 worse, 0 about even (gear::powerScore at the player's level, 3 % either way); an empty slot is better
inline int verdict(const Item& it, const Item* vs, int level, int* pct = nullptr) {
  if (pct) *pct = 0;
  if (!vs) return 1;
  const float a = gear::powerScore(it, level), b = gear::powerScore(*vs, level);
  const float d = (a - b) / std::max(1.0f, b);
  if (pct) *pct = (int)std::lround(d * 100.0f);
  return d > 0.03f ? 1 : d < -0.03f ? -1 : 0;
}

struct CardRow {
  std::string s;
  Color c;
  int arrow = -9;        // -9 none; else -1 down, 0 even, 1 up (drawn in the line's gutter)
  bool bullet = false;   // an affix line with no compare: a small diamond in the gutter
  bool flame = false;    // a Mythic power's name (the flame, letter by letter)
  int prio = 0;          // 0 always; 1 compare losses; 2 power words; 3 warnings; 5 maker; 6 lore; 7 value
  int delta = 0;         // the base stat's change (shown after the text when deltaOn)
  bool deltaOn = false;
};

// The item card (tooltip and compare panel), x, y its top-left, w wide, down to maxY: the icon in its rarity cell, the
// name in its colour (two lines at most), RARITY and ITEM LEVEL N; then the base stat (synced to the player's level),
// the level-sync note, every affix in plain words (affixText), the power's name and words, the maker, the lore, the
// attunement, the value. cmp: the compare against vs (nullptr: an empty slot): an arrow in each affix line's gutter (up:
// new or better, down: worse), the worn piece's affixes this one lacks in red, the base stat's change after it, a
// verdict footer (UPGRADE +N % / DOWNGRADE / ABOUT EVEN, gear::powerScore) and the piece it is weighed against. worn:
// the piece is the one worn (a WORN tag). When the room is short the quiet rows go first (value, lore, maker, then the
// warnings and the power's words); the stat, the affixes, the power's name and the losses always show. Returns the y
// under the card.
template <class Pt>
float itemCard(Pt& P, float t, const CardCtx& X, const Item& it, float x, float y, float w, float maxY, bool cmp, const Item* vs, bool worn,
               bool bigIcon, CardInfo* out, const cult::Culture* vsMaker = nullptr) {
  (void)vsMaker;
  x = std::floor(x); y = std::floor(y);
  const bool equip = itemEquippable(it.kind);
  const bool gearItem = equip && it.ilvl > 0;
  cmp = cmp && equip && !worn;
  const int L = X.level;
  const int eff = gear::effLevel(it.ilvl, L);
  CardInfo info;
  info.rarity = (int)it.rarity; info.ilvl = it.ilvl; info.name = it.name; info.compare = cmp; info.worn = worn && equip;

  // ---- the header
  const float isz = bigIcon ? 32.0f : 16.0f;
  if (bigIcon) {
    P.rect(x - 1, y - 1, isz + 2, isz + 2, Color(0.04f, 0.03f, 0.05f, 0.6f));
    if (equip) rarityFrame(P, t, x - 1, y - 1, isz + 2, isz + 2, it.rarity, false, false);
    P.icon(it, x, y, isz);
  } else {
    rarityIcon(P, t, it, x, y, isz);
  }
  const float tx = x + isz + 6, tw = w - isz - 6;
  float hy = y + (bigIcon ? 1.0f : 0.0f);
  const auto nameLines = wrap(it.name, (int)((tw + 1) / 6));
  for (size_t i = 0; i < nameLines.size() && i < 2; i++) {
    std::string l = nameLines[i];
    if (i == 1 && nameLines.size() > 2) l = fit(l, tw - 18) + "...";
    rarityText(P, t, tx, hy, l, it.rarity, 1, 0, true);
    hy += 10;
  }
  if (equip) {
    const std::string rl = rarityName(it.rarity);
    std::string il = gearItem ? "ITEM LEVEL " + std::to_string((int)it.ilvl) : std::string();
    if (gearItem && (float)P.textW(rl + "  " + il) > tw) il = "ILVL " + std::to_string((int)it.ilvl);
    rarityText(P, t, tx, hy, rl, it.rarity, 0.95f);
    if (!il.empty()) P.text(tx + (float)P.textW(rl) + 7, hy, il, kDim, 0);
    hy += 10;
  }
  float cy = std::max(hy + 3, y + isz + 5);

  // ---- the rows
  std::vector<CardRow> rows;
  rows.reserve(24);
  auto add = [&](const std::string& s, Color c, int prio) -> CardRow& { CardRow r; r.s = s; r.c = c; r.prio = prio; rows.push_back(r); return rows.back(); };
  const int pw = (int)std::lround(gear::usePower(it, L));
  const int vpw = vs ? (int)std::lround(gear::usePower(*vs, L)) : 0;
  auto statRow = [&](const std::string& s, int v, int vv) {
    CardRow& r = add(s, kText, 0);
    if (cmp) { r.deltaOn = true; r.delta = v - vv; }
  };
  switch (it.kind) {
    case ItemKind::Weapon: statRow("DAMAGE " + std::to_string(pw), pw, vpw); break;
    case ItemKind::Bow: statRow("ARROW DAMAGE " + std::to_string(pw), pw, vpw); break;
    case ItemKind::Staff: statRow("SPELL POWER +" + std::to_string(pw * 2) + "%", pw * 2, vpw * 2); break;
    case ItemKind::Armor: case ItemKind::Helmet: case ItemKind::Shield: case ItemKind::Gloves: case ItemKind::Boots: case ItemKind::Cloak:
      statRow("ARMOR " + std::to_string(pw), pw, vpw); break;
    case ItemKind::Ring: case ItemKind::Amulet: if (!gearItem) add("JEWELLERY", kText, 0); break;
    case ItemKind::Potion: add("RESTORES " + std::to_string(it.power), kText, 0); add("SHARED COOLDOWN 8 S", kQuiet, 5); add("CARRY 10 AT MOST", kQuiet, 5); break;
    case ItemKind::Food: add("HEALS " + std::to_string(it.power), kText, 0); add("ENDS HUNGER: WELL FED", kText, 2); break;
    case ItemKind::Arrows: add("AMMUNITION", kText, 0); break;
    case ItemKind::Quest: add("QUEST ITEM", kText, 0); break;
    case ItemKind::Material: add("CRAFTING MATERIAL", kText, 0); add("FOR THE SMELTER, FORGE OR TANNERY", kQuiet, 5); break;
    default: break;
  }
  if (gearItem && eff < it.ilvl) add("SYNCED TO " + std::to_string(eff) + " (YOUR LEVEL + 4)", kWarn, 3);
  if (it.ench != Ench::None) {
    std::string e;
    switch (it.ench) {
      case Ench::Fire: e = "BURNS FOR " + std::to_string(it.enchPow) + " FIRE"; break;
      case Ench::Frost: e = "FREEZES FOR " + std::to_string(it.enchPow) + " FROST"; break;
      case Ench::Drain: e = "DRAINS " + std::to_string(it.enchPow / 2) + " HEALTH"; break;
      case Ench::Health: e = "+" + std::to_string(it.enchPow) + " MAX HEALTH"; break;
      case Ench::Magicka: e = "+" + std::to_string(it.enchPow) + " MAX MAGICKA"; break;
      case Ench::Stamina: e = "+" + std::to_string(it.enchPow) + " MAX STAMINA"; break;
      case Ench::Fortify: e = "+" + std::to_string(it.enchPow / 2) + " ARMOR"; break;
      default: break;
    }
    if (!e.empty()) add(e, kAffix, 0).bullet = true;
  }
  for (const ItemAffix& a : it.affix) {   // every affix in plain words, each with its arrow against the worn one's same affix
    if (a.kind == Affix::None) continue;
    const int v = gear::affixUse(it, a.kind, L);
    CardRow& r = add(affixText(a.kind, v), kAffix, 0);
    info.affixLines++;
    if (cmp) {
      const int wv = vs && vs->affixValue(a.kind) ? gear::affixUse(*vs, a.kind, L) : 0;
      r.arrow = wv == 0 || v > wv ? 1 : v < wv ? -1 : 0;
    } else {
      r.bullet = true;
    }
  }
  if (it.unique != Unique::None) {   // the power: its name as a heading (a Mythic's in the flame), its words under it
    const UniqueInfo& u = uniqueInfo(it.unique);
    CardRow& r = add(u.name, kPower, 0);
    r.flame = uniqueIsMythic(it.unique);
    if (cmp) r.arrow = vs && vs->unique == it.unique ? 0 : 1;
    else r.bullet = true;
    add(u.line, kPowerWords, 2);
    info.power = true;
  }
  // what the worn piece has and this one lacks, after the item's own lines and each one named as a loss
  // ("LOSES +6% FROST DAMAGE") so it never reads as a stat this item carries
  if (cmp && vs)
    for (const ItemAffix& a : vs->affix) {
      if (a.kind == Affix::None || it.affixValue(a.kind)) continue;
      add("LOSES " + affixText(a.kind, gear::affixUse(*vs, a.kind, L)), kLost, 1).arrow = -1;
      info.lostLines++;
    }
  if (cmp && vs && vs->unique != Unique::None && vs->unique != it.unique) {
    add(std::string("LOSES ") + uniqueInfo(vs->unique).name, kLost, 1).arrow = -1;
    info.lostLines++;
  }
  if (gearItem) {
    std::string who = X.maker ? X.maker->adjective + " WORK" : std::string("HEARTLAND WORK");
    const std::string mw = gear::matWord(it, X.maker);
    if (!mw.empty()) who += ", " + mw;
    else if (it.mat != Mat::None) who += std::string(", ") + matName(it.mat);
    add(who, kQuiet, 5);
    if (it.flags & IF_CRAFTED) add("CRAFTED", kQuiet, 6);
  }
  if (it.rarity >= Rarity::Legendary && gearItem) {
    add(gear::legendaryLore(it, X.maker), kQuiet, 6);
    const int slots = gear::attunementSlots(L);
    if (slots == 0) add("ATTUNEMENT OPENS AT LEVEL 10", kWarn, 3);
    else add("ATTUNED " + std::to_string(X.attuned) + "/" + std::to_string(slots), kWarn, 3);
  }
  if (it.kind != ItemKind::Quest) add("VALUE " + std::to_string(it.value), kQuiet, 7);

  // ---- fit: the footer's room first, then the lowest-priority rows go until the rest fits
  const float gutter = 10;
  const bool footer = equip && (cmp || worn);
  const float footH = !footer ? 0.0f : worn ? 13.0f : 26.0f;
  auto rowLines = [&](const CardRow& r) {
    const bool gut = r.arrow != -9 || r.bullet;
    return (int)wrap(r.s, (int)((w - (gut ? gutter : 0) + 1) / 6)).size();
  };
  float pitch = 10;
  std::vector<char> on(rows.size(), 1);
  auto height = [&]() {
    float h = 0;
    for (size_t i = 0; i < rows.size(); i++) if (on[i]) h += (float)rowLines(rows[i]) * pitch;
    return h;
  };
  const float room = maxY - cy - footH;
  if (height() > room) pitch = 9;
  for (int pr = 7; pr >= 2 && height() > room; pr--)
    for (int i = (int)rows.size() - 1; i >= 0 && height() > room; i--)
      if (rows[(size_t)i].prio == pr) on[(size_t)i] = 0;

  // ---- draw them
  for (size_t i = 0; i < rows.size(); i++) {
    if (!on[i]) continue;
    const CardRow& r = rows[i];
    const bool gut = r.arrow != -9 || r.bullet;
    const float lx = x + (gut ? gutter : 0);
    const auto lines = wrap(r.s, (int)((w - (gut ? gutter : 0) + 1) / 6));
    for (size_t k = 0; k < lines.size(); k++) {
      if (cy + 8 > maxY - footH) break;
      if (k == 0) {
        if (r.arrow != -9) arrow(P, x, cy, r.arrow);
        else if (r.bullet) {   // a small diamond in the line's colour
          const Color bc = r.flame ? flameAt(t, 0, 0.4f) : r.c;
          P.rect(x + 3, cy + 1, 1, 1, bc); P.rect(x + 2, cy + 2, 3, 1, bc); P.rect(x + 3, cy + 3, 1, 1, bc);
        }
      }
      if (r.flame) rarityText(P, t, lx, cy, lines[k], Rarity::Mythic);
      else P.text(lx, cy, lines[k], r.c, 0);
      if (k == 0 && r.deltaOn) {   // the base stat's change after it
        const float dx = lx + (float)P.textW(lines[k]) + 6;
        const int dir = r.delta > 0 ? 1 : r.delta < 0 ? -1 : 0;
        arrow(P, dx, cy, dir);
        if (dir) P.text(dx + 10, cy, (r.delta > 0 ? "+" : "") + std::to_string(r.delta), dir > 0 ? kUp : kDown, 0);
      }
      cy += pitch;
    }
  }

  // ---- the compare footer: the verdict and the piece it is weighed against
  if (footer) {
    float fy = cy + 2;
    P.rect(x, fy, w, 1, Color(0.4f, 0.32f, 0.2f, 0.8f));
    fy += 4;
    if (worn) {
      P.text(x, fy, "WORN", kGold, 0);
    } else {
      int pct = 0;
      const int v = X.block ? 0 : verdict(it, vs, L, &pct);
      info.verdict = v;
      if (X.block) {   // (M8b fixer r2, review: "UPGRADE on a piece the player cannot wear") why it cannot be worn, never a verdict
        const Color bc = X.flash ? kWarn : kDown;
        std::string why = "CAN'T WEAR YET: LEVEL 10";
        if (X.block == 2) {   // the worn piece by its whole name, else its first name ("BRAMBLEHEART"), else plainly
          const int room = (int)((w - 10 + 1) / 6) - 15;
          std::string nm = X.blockName;
          const size_t of = nm.find(" OF ");
          if ((int)nm.size() > room && of != std::string::npos) nm.resize(of);
          why = nm.empty() || (int)nm.size() > room ? "TAKE A GREAT PIECE OFF FIRST" : "TAKE OFF " + nm + " FIRST";
        }
        P.rect(x + 1, fy + 2, 5, 2, bc);   // a short bar where the arrow would be
        P.text(x + 10, fy, fit(why, w - 10), bc, 0);
      } else {
        arrow(P, x, fy, v);
        std::string vw = v > 0 ? "UPGRADE" : v < 0 ? "DOWNGRADE" : "ABOUT EVEN";
        if (vs && v != 0) vw += std::string(" ") + (pct > 0 ? "+" : "") + std::to_string(pct) + "%";
        P.text(x + 10, fy, vw, v > 0 ? kUp : v < 0 ? kDown : kDim, 0);
      }
      fy += 11;
      if (vs) {
        P.text(x, fy + 1, "VS", kDim, 0);
        rarityIcon(P, t, *vs, x + 15, fy - 1, 10);
        rarityText(P, t, x + 29, fy + 1, fit(vs->name, w - 29), vs->rarity, 0.9f);
      } else {
        P.text(x, fy + 1, "THE SLOT IS EMPTY", kDim, 0);
      }
    }
    fy += 11;
    cy = fy;
  }
  if (out) *out = info;
  return cy;
}

// ---------------------------------------------------------------------------------------------------- the ground
// A pickup on the ground with its rarity's presence, in 3/4: an elliptical pool of light under it, a soft additive
// column rising straight up (Rare a short blue beam, Epic a taller purple one with twinkling sparkles, Legendary a gold
// pillar with motes, Mythic a living flame: its height and core flicker, its halo sways in slices like rising heat,
// embers ride it and burn out from yellow to red), the piece bobbing over it; an Uncommon only glints green now and then.
// bx, by: the pickup's foot on screen; near 0.28..1 (it fades as the hero walks up); idx: a per-pickup phase.
template <class Pt> void lootGround(Pt& P, float t, const Item* it, int rarity, float bx, float by, float bob, float near, int idx) {
  const int r = std::clamp(rarity, 0, 5);
  bx = std::floor(bx); by = std::floor(by);
  const Color rc = colU(rarityColor((Rarity)r));
  const float tt = t + (float)idx * 0.71f;
  auto beam = [&](float h, float wHalo, float wCore, Color halo, Color core, float a) {
    // the halo and the core blended over the ground in the rarity's own hue (an added blue on green grass reads cyan,
    // an added purple white), a soft added bloom round them, and a faint added heat down the middle
    P.beam(bx - wHalo / 2, by - h, wHalo, h, withA(halo, a * 0.66f), 0, 64, false);
    // (M8b fix) a Rare's added bloom is lighter: blue added over green grass turned the beam cyan-teal, so a Rare body
    // piece on grass and a Rare sword on a path read as two hues
    P.beam(bx - wHalo / 2 - 2, by - h, wHalo + 4, h, withA(halo, a * (r == 2 ? 0.08f : 0.20f)), 0, 64, true);
    P.beam(bx - wCore / 2, by - h * 0.92f, wCore, h * 0.92f, withA(core, a * 0.90f), 0, 64, false);
    P.beam(bx - wCore / 2, by - h * 0.85f, wCore, h * 0.85f, withA(Color(1, 1, 1), a * 0.16f), 0, 64, true);
  };
  auto pool = [&](float w, float h, Color c, float a) { P.pool(bx - w / 2, by - h / 2 - 1, w, h, withA(c, a)); };
  P.shadow(bx - 5, by - 2, 10, 4, 0.8f);
  switch (r) {
    case 1:   // (M8b fix) a dark green rim under the pool so it reads on grass, and a short low glimmer column
      pool(17, 7, shade(rc, 0.35f), 0.55f * near);
      pool(14, 5, rc, 0.70f * near);
      beam(15, 7, 1, rc, lerpC(rc, Color(1, 1, 1), 0.45f), 0.62f * near);
      {   // (M8b fixer r3, review: "on grass an Uncommon looks the same as a Common at phone size") a crisp pale-mint
          // ring on the ground with a dark rim under it, always shown (no blinking glint): light on dark reads on grass,
          // snow and sand alike
        const float pa = (0.90f + 0.10f * std::sin(tt * 3.0f)) * std::max(0.7f, near);
        const int rx = 8, ry = 3;
        for (int x = -rx; x <= rx; x++) {
          const float fx = (float)x / (float)rx;
          const int y = (int)std::lround((float)ry * std::sqrt(std::max(0.0f, 1.0f - fx * fx)));
          const int yPrev = (int)std::lround((float)ry * std::sqrt(std::max(0.0f, 1.0f - std::min(1.0f, std::fabs(fx) + 1.0f / (float)rx) * std::min(1.0f, std::fabs(fx) + 1.0f / (float)rx))));
          const float hgt = (float)std::max(1, y - yPrev + 1);   // the ends stand up so the ring stays closed
          for (int sgn = -1; sgn <= 1; sgn += 2) {
            const float py = sgn < 0 ? by - 2 - (float)y : by - 2 + (float)y - hgt + 1;
            // the dark rim a pixel outside the ring (above the far arc, below the near one), the pale ring on it
            P.rect(bx + (float)x, sgn < 0 ? py - 1 : py + 1, 1, hgt, Color(0.02f, 0.12f, 0.03f, 0.80f * pa));
            P.rect(bx + (float)x, py, 1, hgt, Color(0.84f, 1.0f, 0.74f, std::min(1.0f, 1.05f * pa)));
          }
        }
      }
      break;
    case 2: pool(18, 7, rc, 0.62f * near); beam(34, 9, 3, rc, lerpC(rc, Color(1, 1, 1), 0.40f), 0.85f * near); break;
    case 3: pool(22, 8, rc, 0.70f * near); beam(46, 11, 3, rc, lerpC(rc, Color(1, 1, 1), 0.40f), 0.92f * near); break;
    case 4: {
      const float pulse = 0.88f + 0.12f * std::sin(tt * 2.4f);
      pool(28, 10, rc, 0.80f * near * pulse);
      beam(60, 13, 3, rc, Color(1.0f, 0.93f, 0.62f), 1.0f * near * pulse);
      break;
    }
    case 5: {
      // (M8b fixer r2, review: "the Mythic beam is a dim brown-orange with less contrast than Legendary's gold; frames
      // 0.13 s apart barely differ") a hot two-tone flame: a deep red core inside orange-yellow edges, a white-hot
      // root, every slice's width and the whole column's height flickering fast and hard (never a steady glow)
      const float fl = 0.80f + 0.12f * std::sin(tt * 17.0f) * std::sin(tt * 7.3f) + 0.08f * std::sin(tt * 31.0f + 1.3f);
      const float h = 72.0f + 9.0f * std::sin(tt * 9.1f) * std::sin(tt * 3.7f) + 3.0f * std::sin(tt * 23.0f);
      pool(34, 12, Color(1.0f, 0.30f, 0.08f), 0.95f * near * fl);
      pool(18, 7, Color(1.0f, 0.78f, 0.30f), 0.80f * near * fl);
      const int slices = 12;
      for (int s = 0; s < slices; s++) {
        const float u0 = (float)s / (float)slices;   // 0 foot .. 1 top
        const int sy = std::max(0, (int)std::floor(64.0f * (1.0f - (u0 + 1.0f / (float)slices))));
        const int sh = std::max(1, (int)std::ceil(64.0f / (float)slices));
        const float top = std::floor(by - h * (u0 + 1.0f / (float)slices)), bot = std::floor(by - h * u0);
        const float dy0 = by - top, dh = bot - top;   // whole rows: the slices meet with no seam and no overlap
        const float sway = std::sin(tt * 8.0f - u0 * 7.0f) * 2.4f * u0 + std::sin(tt * 15.0f - u0 * 13.0f) * 0.9f * u0;
        const float lick = std::sin(tt * 19.0f + u0 * 9.0f) * std::sin(tt * 7.7f - u0 * 3.0f);
        const float wd = std::max(3.0f, 18.0f - 11.0f * u0 + 3.0f * lick);
        // the edge: orange-yellow at the root cooling to orange at the tips; a soft added bloom round it
        const Color edge = lerpC(Color(1.0f, 0.84f, 0.34f), Color(1.0f, 0.50f, 0.12f), u0);
        P.beam(std::floor(bx - wd / 2 + sway), by - dy0, std::floor(wd), dh, withA(edge, 0.92f * near * fl), sy, sh, false);
        P.beam(std::floor(bx - wd / 2 - 2 + sway), by - dy0, std::floor(wd) + 4, dh, withA(edge, 0.34f * near * fl), sy, sh, true);
        if (u0 < 0.86f) {   // the red core, flickering in width on its own beat
          const float cw = std::max(2.0f, wd * 0.48f + 1.2f * std::sin(tt * 23.0f + u0 * 11.0f));
          const Color core = lerpC(Color(0.96f, 0.16f, 0.06f), Color(0.78f, 0.06f, 0.04f), u0);
          P.beam(std::floor(bx - cw / 2 + sway * 0.8f), by - dy0, std::floor(cw), dh, withA(core, 1.0f * near * fl), sy, sh, false);
        }
        if (u0 < 0.25f) {   // the white-hot root
          P.beam(std::floor(bx - 1.5f + sway * 0.5f), by - dy0, 3, dh, withA(Color(1.0f, 0.95f, 0.70f), 0.95f * near * (1.0f - u0 * 3.0f)), sy, sh, false);
        }
      }
      break;
    }
    default: break;
  }
  const float iy = by - 16 + bob;
  if (it) P.icon(*it, bx - 8, iy, 16);
  switch (r) {
    case 1: {   // a small green glint that twinkles on the piece's top-left (M8b fix: twice as often)
      const float ph = std::fmod(tt * 1.6f, 1.0f);
      if (ph < 0.30f) {
        const float s = std::sin(ph / 0.30f * kPi);
        const float gx = bx - 3, gy = iy + 3;
        const Color gc = lerpC(rc, Color(1, 1, 1), 0.5f);
        P.add(gx, gy, 1, 1, withA(gc, 0.95f * s));
        if (s > 0.45f) {
          P.add(gx - 1, gy, 1, 1, withA(rc, 0.7f * s)); P.add(gx + 1, gy, 1, 1, withA(rc, 0.7f * s));
          P.add(gx, gy - 1, 1, 1, withA(rc, 0.7f * s)); P.add(gx, gy + 1, 1, 1, withA(rc, 0.7f * s));
        }
        if (s > 0.8f) { P.add(gx - 2, gy, 1, 1, withA(rc, 0.35f)); P.add(gx + 2, gy, 1, 1, withA(rc, 0.35f)); }
      }
      break;
    }
    case 2: {   // one glint on the beam's core now and then
      const float ph = std::fmod(tt * 0.7f, 1.0f);
      if (ph < 0.18f) P.add(bx - 1, iy + 2, 2, 1, Color(0.85f, 0.92f, 1.0f, 0.8f * std::sin(ph / 0.18f * kPi) * near + 0.15f));
      break;
    }
    case 3: case 4: case 5: {
      const int n = r == 3 ? 3 : r == 4 ? 5 : 14;
      const float H = r == 3 ? 44.0f : r == 4 ? 58.0f : 66.0f;
      for (int m = 0; m < n; m++) {
        const float sp = r == 5 ? 0.62f + 0.13f * (float)(m % 3) : 0.32f + 0.06f * (float)(m % 2);
        const float cyc = tt * sp + (float)m * 0.618f;
        const float life = cyc - std::floor(cyc);
        const float seedx = std::sin((float)m * 12.9898f + std::floor(cyc) * 4.1f);
        const float spread = r == 5 ? 5.0f : 4.0f;
        const float mx = std::floor(bx + seedx * spread * (r == 5 ? (0.5f + life) : 1.0f) + (r == 5 ? std::sin(tt * 5 + (float)m) * 1.2f * life : 0.0f));
        const float my = std::floor(by - 2 - life * H);
        const float a = std::sin(life * kPi) * near;
        Color mc;
        if (r == 3) mc = lerpC(rc, Color(1, 1, 1), 0.5f + 0.5f * std::sin(tt * 9 + (float)m * 2.0f));
        else if (r == 4) mc = Color(1.0f, 0.90f, 0.55f);
        else mc = flameAt(t, (float)m, life);
        P.add(mx, my, 1, 1, withA(mc, 0.95f * a));
        if (r == 3 && a > 0.75f) {   // the twinkle's cross
          P.add(mx - 1, my, 1, 1, withA(mc, 0.45f * a)); P.add(mx + 1, my, 1, 1, withA(mc, 0.45f * a));
          P.add(mx, my - 1, 1, 1, withA(mc, 0.45f * a)); P.add(mx, my + 1, 1, 1, withA(mc, 0.45f * a));
        }
        if (r == 5 && life < 0.45f) {   // a two-pixel ember near the root, solid so it reads on a sunlit field
          P.rect(mx, my + 1, 1, 1, withA(Color(mc.r, mc.g * 0.7f, mc.b * 0.5f), 0.85f * a));
          P.rect(mx + 1, my, 1, 1, withA(mc, 0.6f * a));
        }
      }
      if (r >= 4) {   // the piece catches the light: a slow glint across the icon
        const float ph = std::fmod(tt * 0.45f, 1.0f);
        if (ph < 0.25f) {
          const float s = std::sin(ph / 0.25f * kPi);
          const Color gc = r == 4 ? Color(1, 0.95f, 0.75f) : Color(1, 0.8f, 0.45f);
          P.add(std::floor(bx - 4 + ph * 32), std::floor(iy + 4 + ph * 12), 1, 1, withA(gc, 0.9f * s));
        }
      }
      break;
    }
    default: break;
  }
}

}  // namespace lootp
