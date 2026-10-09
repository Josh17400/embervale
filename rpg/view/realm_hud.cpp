// M4 "Banners" VIEW lane: the realm on the HUD and in the journal (VISION_PLAN 4.5, 4.6, 7.1, 15.8).
//   - the border herald: crossing into another kingdom's land (Ev::Border) shows a herald's ribbon at the top centre,
//     "ENTERING" over "THE KHAGANATE OF ASHMARK" (the society's word: rui::realmWord) between two shields of its arms,
//     for about 2.6 s, clear of the vitals, the minimap and the thumbs; "THE WILDLANDS" when leaving every realm
//   - Ev::News: a toast in the left column ("NEWS: ...")
//   - the arrival banner names the settlement's CURRENT owner and its state ("OCCUPIED BY QIBA"), and is shown again
//     when either changes while the player stands there (a conquest, a siege laid or broken)
//   - the journal (QUESTS tab): a section strip QUESTS | NEWS | HISTORY at the top of the list; NEWS lists the heard
//     events newest first (story::newsLine, how many days ago), HISTORY the Lost History entries (story.lore())
#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>
#include "rpg/art/art_culture.h"
#include "rpg/view/realm_ui.h"
#include "rpg/view/view.h"
#include "rpg/world/economy.h"
#include "rpg/world/poi.h"
#include "rpg/world/source.h"

namespace {
Color col(uint32_t c, float a = 1) { return Color((c & 255) / 255.0f, ((c >> 8) & 255) / 255.0f, ((c >> 16) & 255) / 255.0f, a); }
Color lift(Color c) { return Color(c.r * 0.40f + 0.58f, c.g * 0.40f + 0.55f, c.b * 0.40f + 0.48f, c.a); }
const Color kGold(0.98f, 0.82f, 0.42f), kText(0.93f, 0.9f, 0.82f), kDim(0.62f, 0.58f, 0.52f);

std::vector<std::string> wrapW(const std::string& s, int maxChars) {
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
    else word += c;
  }
  flush();
  if (!line.empty()) lines.push_back(line);
  return lines;
}
std::string cut(const std::string& s, int chars) {
  if ((int)s.size() <= chars) return s;
  std::string t = s.substr(0, (size_t)std::max(1, chars));
  const size_t sp = t.find_last_of(' ');
  if (sp != std::string::npos && (int)sp >= chars - 8) t.resize(sp);
  return t;
}
// the heard events, newest first (the journal's NEWS)
std::vector<const realm::WorldEvent*> heardNews(const Game& g) {
  std::vector<const realm::WorldEvent*> v;
  for (const realm::WorldEvent& e : g.realm.events()) if (e.heard) v.push_back(&e);
  std::stable_sort(v.begin(), v.end(), [](const realm::WorldEvent* a, const realm::WorldEvent* b) { return a->id > b->id; });
  return v;
}
// a short headline for the NEWS list ("SIEGE - HRAFNSTAD")
std::string headline(const Game& g, const realm::WorldEvent& e) {
  std::string place;
  if (e.site) {
    const int h = g.world.siteHandle(e.site);
    if (h >= 0) place = g.world.sites[(size_t)h].name;
  }
  if (place.empty() && e.a) { const rui::Look k = rui::look(g, e.a); if (k.ok) place = k.name; }
  std::string t = realm::evTypeName(e.type);
  return place.empty() ? t : t + " - " + place;
}
Color evColour(realm::EvType t) {
  switch (t) {
    case realm::EvType::WarDeclared: case realm::EvType::SiegeBegun: case realm::EvType::TownTaken: case realm::EvType::TownBurned:
    case realm::EvType::Skirmish: case realm::EvType::KingdomFell: case realm::EvType::CivilWar: return Color(1.0f, 0.55f, 0.42f);
    case realm::EvType::Famine: case realm::EvType::HarvestFailed: case realm::EvType::PricesRising: case realm::EvType::TradeBroken:
    case realm::EvType::Refugees: case realm::EvType::BorderIncident: case realm::EvType::TroopsMarching: return Color(0.98f, 0.80f, 0.45f);
    case realm::EvType::Peace: case realm::EvType::SiegeBroken: case realm::EvType::TradeDeal: case realm::EvType::Festival:
    case realm::EvType::Resettled: return Color(0.62f, 0.92f, 0.6f);
    default: return kText;
  }
}
}  // namespace

const Tex& View::armsTex(const cult::Heraldry& h, int size) {
  const uint64_t k = ew::mix64(h.key() ^ ((uint64_t)size << 56) ^ 0xA2A5ull);
  auto it = armsTex_.find(k);
  if (it != armsTex_.end()) return it->second;
  return armsTex_[k] = pix_->bake(art::shieldArms(h, size));
}

// ---------------------------------------------------------------- events
void View::heraldEvent(Game& g, const Event& e) {
  // whose land the player now stands on (the event carries the name and the colour; the arms and the society's word
  // come from the realm)
  ew::Gid land = 0;
  if (g.world.src && e.f > 0.5f) {
    const int32_t gx = g.world.ox + (int32_t)std::floor(g.pl().p.x / TILE), gy = g.world.oy + (int32_t)std::floor(g.pl().p.y / TILE);
    land = rui::landOwnerAt(g, gx, gy);
  }
  const ew::Gid was = herald_.kingdom;
  // (fixer M4 r1) crossing into a kingdom by arriving in one of its settlements (or the settlement changing hands under
  // the player): the arrival banner already says whose it is, so the herald's plaque does not stack over it
  const bool quiet = bannerT_ > 0 && !g.inside && g.settlementAt(g.pl().p) >= 0;
  herald_ = Herald();
  herald_.kingdom = land;
  herald_.wild = land == 0;
  herald_.t = quiet ? 0.0f : 2.8f;
  if (land) {
    const rui::Look k = rui::look(g, land);
    herald_.title = rui::realmTitle(k, true);
    if (!k.ok) herald_.title = "THE LANDS OF " + e.s;
    herald_.color = k.color ? k.color : (uint32_t)e.a;
    herald_.top = "ENTERING";
  } else {
    herald_.title = "THE WILDLANDS";
    herald_.color = rgba(214, 196, 150);
    herald_.top = was ? "LEAVING THE REALM" : "ENTERING";
  }
  // a herald's two notes (quiet: it is a crossing, not a fanfare)
  audio_->play(Sfx::Discover, land ? 0.9f : 0.75f, 0.45f);
  // the first-visit biome banner waits behind the herald (it shares the top centre)
  if (biomeBannerT_ > 0) biomeBannerT_ = std::min(biomeBannerT_ + 2.8f, 4.0f);
}

void View::newsEvent(Game& g, const Event& e) {
  (void)g;
  if (e.s.empty()) return;
  Toast t;
  t.s = "NEWS: " + e.s;
  t.c = Color(0.98f, 0.86f, 0.62f);
  toasts_.push_back(t);
}

// ---------------------------------------------------------------- the arrival banner's words
void View::arrivalText(Game& g, int cs, std::string& line, std::string& state, uint32_t& stateCol) {
  const Site& S = g.world.sites[(size_t)cs];
  std::string sp = S.special ? std::string(ew::specialtyName((ew::Specialty)S.special)) + " " + siteTypeName(S.type) : std::string();
  if (S.archetype == (uint8_t)ew::Archetype::Market && S.type != SiteType::Village)
    sp = std::string("MARKET ") + siteTypeName(S.type) + (S.special ? std::string(" - ") + ew::specialtyName((ew::Specialty)S.special) : std::string());
  const Kingdom* k = g.world.kingdomOf(cs);
  std::string kl;
  if (k) {
    const rui::Look L = rui::look(g, k->id);
    kl = (S.capital && S.homeKingdom == S.kingdom ? "CAPITAL OF THE " : "") + rui::realmTitle(L, false);
  }
  line = sp.empty() ? kl : (kl.empty() ? sp : sp + "  -  " + kl);
  stateCol = rgba(236, 120, 90);
  state = rui::stateLine(g, S.id, &stateCol);
}

// ---------------------------------------------------------------- the herald's ribbon
void View::drawHerald(Game& g) {
  if (herald_.t <= 0 || g.mode != Mode::Play) return;
  Pix& P = *pix_;
  const float L = (float)Pix::SL, T = (float)Pix::ST, R = (float)(Pix::W - Pix::SR);
  const float total = 2.8f, a = std::clamp(std::min(herald_.t, total - herald_.t) * 3.0f, 0.0f, 1.0f);
  const float slide = std::floor((1 - a) * -8);
  const int tw = std::max(P.textW(herald_.title, 1), P.textW(herald_.top, 1));
  const bool shields = !herald_.wild;
  const float side = shields ? 22.0f : 10.0f;
  float w = tw + side * 2 + 12, h = 27;
  // between the vitals (L .. L + 132) and the minimap (R - 72 .. R): centred, nudged into the free strip
  float x = std::floor(Pix::W / 2.0f - w / 2);
  const float minX = L + 136, maxX = R - 76 - w;
  if (maxX >= minX) x = std::clamp(x, minX, maxX);
  // (M6 fixer) under a world boss's top bar, never over it (the ribbon shows exactly when the hero walks into its land)
  const float y = std::max(T + 5, bossBarBottom_ + 3) + slide;
  const Color ink(0.03f, 0.025f, 0.04f, 0.78f * a);
  const Color kc = col(herald_.color, a);
  // the ribbon: a dark band with swallow-tail ends in the realm's colour, gold rules top and bottom
  P.rect(x + 4, y, w - 8, h, ink);
  for (int j = 0; j < (int)h; j++) {
    const float d = std::fabs(j - (h - 1) / 2.0f), ind = std::floor((h / 2 - d) * 0.35f);   // the notch
    P.rect(x - 4 + ind, y + j, 8 - ind, 1, Color(kc.r * 0.55f, kc.g * 0.55f, kc.b * 0.55f, 0.9f * a));
    P.rect(x + w - 4, y + j, 8 - ind, 1, Color(kc.r * 0.55f, kc.g * 0.55f, kc.b * 0.55f, 0.9f * a));
  }
  P.rect(x + 4, y, w - 8, 1, Color(kGold.r, kGold.g, kGold.b, 0.85f * a));
  P.rect(x + 4, y + h - 1, w - 8, 1, Color(kGold.r, kGold.g, kGold.b, 0.85f * a));
  P.rect(x + 4, y + 1, w - 8, 1, Color(kc.r, kc.g, kc.b, 0.55f * a));
  P.rect(x + 4, y + h - 2, w - 8, 1, Color(kc.r, kc.g, kc.b, 0.55f * a));
  // the shields of its arms at both ends
  if (shields) {
    const rui::Look k = rui::look(g, herald_.kingdom);
    cult::Heraldry hr;
    rui::arms(k, hr);
    const Tex& sh = armsTex(hr, 16);
    const Color tint(1, 1, 1, a);
    P.blit(sh, x + 6, y + std::floor((h - sh.h) / 2), false, tint);
    P.blit(sh, x + w - 6 - sh.w, y + std::floor((h - sh.h) / 2), false, tint);
  }
  // the words: the small line in dim gold, the realm's title in its colour lifted to read on the dark band
  const float cx = std::floor(x + w / 2);
  P.text(cx, y + 4, herald_.top, 1, Color(0.85f, 0.74f, 0.5f, 0.95f * a), 1);
  const Color tc = herald_.wild ? Color(0.96f, 0.90f, 0.74f, a) : Color(kc.r * 0.30f + 0.70f, kc.g * 0.30f + 0.67f, kc.b * 0.30f + 0.60f, a);
  P.text(cx + 1, y + 15, herald_.title, 1, Color(0, 0, 0, 0.8f * a), 1);
  P.text(cx, y + 14, herald_.title, 1, Color(tc.r, tc.g, tc.b, a), 1);
}

// ---------------------------------------------------------------- the journal's sections
void View::drawJournalStrip(float lx, float lw, float top) {
  Pix& P = *pix_;
  static const char* kSec[3] = {"QUESTS", "NEWS", "HISTORY"};
  const float bw = std::floor((lw - 8) / 3), bh = journalStripH() - 4;
  for (int i = 0; i < 3; i++) button(lx + i * (bw + 4), top + 6, bw, bh, kSec[i], journalSec_ == i);
  (void)P;
}
bool View::journalStripTap(Vec2 p, float lx, float lw, float top) {
  const float bw = std::floor((lw - 8) / 3), bh = journalStripH() - 4;
  for (int i = 0; i < 3; i++)
    if (p.x >= lx + i * (bw + 4) - 1 && p.x < lx + i * (bw + 4) + bw + 1 && p.y >= top + 4 && p.y < top + 8 + bh) {
      if (journalSec_ != i) { setJournalSection(i); audio_->play(Sfx::MenuMove); }
      return true;
    }
  return false;
}

// the NEWS and HISTORY sections (journalSec_ 1, 2): a list on the left (the menu's list column), the picked entry on
// the right; `top` is the list's top (under the strip)
void View::drawJournalSection(Game& g, float top) {
  Pix& P = *pix_;
  const bool touch = touchUI;
  const float pitch = touch ? 24.0f : 12.0f;
  const float lx = 18, lw = std::floor(200 + std::max(0, Pix::W - 480) * 0.4f), div = lx + lw + 8, dx = div + 10, dw = (float)Pix::W - 20 - dx;
  const float listBottom = (float)Pix::H - (touch ? 50.0f : 26.0f);
  const int rows = std::max(3, (int)((listBottom - top - 6) / pitch));
  std::vector<std::string> titles;
  std::vector<Color> cols;
  std::vector<const realm::WorldEvent*> news;
  const std::vector<story::LoreEntry>& lore = g.story.lore();
  if (journalSec_ == 1) {
    news = heardNews(g);
    for (const realm::WorldEvent* e : news) { titles.push_back(headline(g, *e)); cols.push_back(evColour(e->type)); }
  } else {
    for (const story::LoreEntry& l : lore) { titles.push_back(l.title); cols.push_back(Color(0.86f, 0.80f, 1.0f)); }
  }
  const int n = (int)titles.size();
  menuSel_ = std::clamp(menuSel_, 0, std::max(0, n - 1));
  if (menuSel_ < menuScroll_) menuScroll_ = menuSel_;
  if (menuSel_ >= menuScroll_ + rows) menuScroll_ = menuSel_ - rows + 1;
  menuScroll_ = std::clamp(menuScroll_, 0, std::max(0, n - rows));
  for (int r = 0; r < rows && menuScroll_ + r < n; r++) {
    const int i = menuScroll_ + r;
    const float rowTop = top + 4 + r * pitch, yy = rowTop + std::floor((pitch - 8) / 2);
    if (i == menuSel_) P.rect(lx, rowTop, lw, pitch - (touch ? 1 : 0), Color(0.3f, 0.22f, 0.12f, 0.8f));
    else if (touch && (r & 1)) P.rect(lx, rowTop, lw, pitch - 1, Color(0.16f, 0.12f, 0.08f, 0.35f));
    std::string t = titles[(size_t)i];
    const int fit = std::max(4, (int)((lw - 10) / 6));
    if ((int)t.size() > fit) t = t.substr(0, (size_t)fit);
    P.text(lx + 4, yy, t, 1, cols[(size_t)i]);
  }
  if (n > rows) {
    const std::string cnt = std::to_string(menuScroll_ + 1) + "-" + std::to_string(std::min(n, menuScroll_ + rows)) + " OF " + std::to_string(n);
    if (touch) {
      button(lx, (float)Pix::H - 46, 52, 24, "UP", false);
      button(lx + lw - 52, (float)Pix::H - 46, 52, 24, "DOWN", false);
      P.text(lx + lw / 2, (float)Pix::H - 38, cnt, 1, kDim, 1);
    } else P.text(lx + lw / 2, (float)Pix::H - 18, cnt, 1, kDim, 1);
  }
  P.rect(div, top - journalStripH() + 8, 1, Pix::H - (top - journalStripH()) - 24, Color(0.4f, 0.32f, 0.2f));
  const int per = std::max(10, (int)(dw / 6));
  float y = top - journalStripH() + 12;
  if (n == 0) {
    const char* empty = journalSec_ == 1 ? "NO NEWS YET. INNKEEPERS, TRAVELLERS AND GUARDS TELL WHAT THEY HAVE HEARD."
                                         : "NOTHING PIECED TOGETHER YET. RUINS KEEP THEIR STORIES IN INSCRIPTIONS, GRAVES AND JOURNALS.";
    for (const std::string& l : wrapW(empty, per)) { P.text(dx, y, l, 1, kDim); y += 10; }
    return;
  }
  if (journalSec_ == 1) {
    const realm::WorldEvent& e = *news[(size_t)menuSel_];
    for (const std::string& l : wrapW(realm::evTypeName(e.type), per)) { P.text(dx, y, l, 1, evColour(e.type)); y += 10; }
    P.text(dx, y, rui::daysAgo(g.day, e.day), 1, kDim);
    y += 14;
    const std::string line = story::newsLine(g, e, 0);
    for (const std::string& l : wrapW(line, per)) { if (y > Pix::H - 40) break; P.text(dx, y, l, 1, kText); y += 10; }
    y += 6;
    // the sides, each in its colours
    for (ew::Gid kid : {e.a, e.b}) {
      if (!kid || y > Pix::H - 30 || (kid == e.b && e.b == e.a)) continue;
      const rui::Look k = rui::look(g, kid);
      if (!k.ok) continue;
      cult::Heraldry hr;
      rui::arms(k, hr);
      const Tex& sh = armsTex(hr, 12);
      P.blit(sh, dx, y - 2);
      P.text(dx + 16, y + 1, cut(rui::realmTitle(k, false), (int)((dw - 18) / 6)), 1, lift(col(k.color ? k.color : rgba(200, 180, 140))));
      y += 15;
    }
  } else {
    const story::LoreEntry& l = lore[(size_t)menuSel_];
    for (const std::string& s : wrapW(l.title, per)) { P.text(dx, y, s, 1, Color(0.86f, 0.80f, 1.0f)); y += 10; }
    if (l.total > 0) { P.text(dx, y, std::to_string(l.found) + " OF " + std::to_string(l.total) + " CLUES FOUND", 1, kDim); y += 10; }
    y += 4;
    for (const std::string& line : l.lines) {
      for (const std::string& s : wrapW(line, per)) { if (y > Pix::H - 24) break; P.text(dx, y, s, 1, kText); y += 10; }
      y += 3;
    }
  }
}

bool View::journalSectionTap(Game& g, Vec2 p, float top) {
  const bool touch = touchUI;
  const float pitch = touch ? 24.0f : 12.0f;
  const float lx = 18, lw = std::floor(200 + std::max(0, Pix::W - 480) * 0.4f);
  const float listBottom = (float)Pix::H - (touch ? 50.0f : 26.0f);
  const int rows = std::max(3, (int)((listBottom - top - 6) / pitch));
  const int n = journalSec_ == 1 ? (int)heardNews(g).size() : (int)g.story.lore().size();
  for (int r = 0; r < rows && menuScroll_ + r < n; r++)
    if (p.x >= lx && p.x < lx + lw && p.y >= top + 4 + r * pitch && p.y < top + 4 + (r + 1) * pitch) { menuSel_ = menuScroll_ + r; audio_->play(Sfx::MenuMove); return true; }
  if (touch && n > rows) {
    const float by = (float)Pix::H - 46;
    if (p.y >= by - 2 && p.y < by + 26 && p.x >= lx && p.x < lx + 52) { menuSel_ = std::max(0, menuSel_ - rows); menuScroll_ = std::max(0, menuScroll_ - rows); return true; }
    if (p.y >= by - 2 && p.y < by + 26 && p.x >= lx + lw - 52 && p.x < lx + lw) {
      menuScroll_ = std::min(std::max(0, n - rows), menuScroll_ + rows);
      menuSel_ = std::min(n - 1, std::max(menuSel_ + rows, menuScroll_));
      return true;
    }
  }
  return false;
}
