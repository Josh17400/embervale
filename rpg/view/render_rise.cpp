// M12 "Rise and Fall": the player's place in history on the screen (View's RISE block, rpg/view/view.h; Ev::Rise,
// rpg/sim/rise.h). VIEW lane.
//   - the herald's banners of the player's rise: every RiseEv that matters (sworn, promoted, orders, the court's
//     answer, a lordship, independence, a crown, a warlord or a world boss near) becomes one banner at the top centre:
//     a cloth hanging from a gilded rod in the concerned realm's colours, its arms on the left, the rank's badge (or the
//     realm's arms again) on the right, a small line over the moment's words. Banners queue: one at a time, eased in
//     (a drop with a little overshoot) and out (lifted and faded), never stacked; each kind has its sting (engine/audio:
//     a drum roll for an oath, a fanfare for a rank, a horn call for independence, an alarm for a warlord or a beast).
//     The herald's border ribbon and the first-visit biome banner share the top centre: a banner waits for them.
//   - quiet moments (the week's pay, a fief's taxes, a tribute paid) are a small coin chip by the purse, not a banner
//   - while enlisted, the rank's badge in the sworn army's colours sits under the level, beside the vitals
// The STANDING page is rise_standing.cpp, the world's rise pieces rise_world.cpp, the world map's realms worldmap.cpp.
#include <algorithm>
#include <cmath>
#include <deque>
#include <string>
#include <unordered_map>
#include <vector>
#include "rpg/art/art_rise.h"
#include "rpg/sim/service.h"
#include "rpg/view/realm_ui.h"
#include "rpg/view/rise_view.h"

namespace {
Color colOf(uint32_t c, float a = 1) { return Color((c & 255) / 255.0f, ((c >> 8) & 255) / 255.0f, ((c >> 16) & 255) / 255.0f, a); }
const Color kGoldC(0.98f, 0.82f, 0.42f);

// how a moment is shown: a banner (with its small top line and sting) or a quiet coin chip
struct Kind { const char* top; bool banner; bool bad; bool major; float dur; };
Kind kindOf(RiseEv k, int f) {
  switch (k) {
    case RiseEv::Enlisted: return {"SWORN TO THE ARMY", true, false, false, 3.4f};
    case RiseEv::Promoted: return {"PROMOTED", true, false, false, 3.4f};
    case RiseEv::Order: return {"ORDERS", true, false, false, 2.9f};
    case RiseEv::OrderDone: return {"ORDERS CARRIED OUT", true, false, false, 2.8f};
    case RiseEv::OrderFailed: return {"THE ORDER FAILED", true, true, false, 2.8f};
    case RiseEv::Discharged: return {"HONOURABLY DISCHARGED", true, false, false, 3.0f};
    case RiseEv::Deserted: return {"OATH BROKEN", true, true, false, 3.0f};
    case RiseEv::Mission: return {"AN ENVOY'S ERRAND", true, false, false, 2.9f};
    case RiseEv::MissionDone:
      return {f == 1 ? "PEACE SIGNED" : f == 2 ? "ALLIANCE SWORN" : f == 3 ? "TRIBUTE AGREED" : "THE TERMS REFUSED", true, f == 0, false, 3.0f};
    case RiseEv::FiefGranted: return {f == 2 ? "A CHARTER BOUGHT" : f == 3 ? "A RUIN RESETTLED" : f == 4 ? "TAKEN BY YOUR BANNER" : "A LORDSHIP", true, false, true, 3.8f};
    case RiseEv::TributeRefused: return {"TRIBUTE WITHHELD", true, true, false, 3.0f};
    case RiseEv::Independence: return {"YOUR BANNER FLIES FREE", true, false, true, 4.4f};
    case RiseEv::Crowned: return {"A CROWN OF YOUR OWN", true, false, true, 4.6f};
    case RiseEv::WarlordNear: return {f == 2 ? "A WARBAND RAIDS" : "A WARLORD RISES", true, true, false, 3.4f};
    case RiseEv::BossRaid: return {"A BEAST RAIDS", true, true, false, 3.4f};
    case RiseEv::Wage: return {"PAY", false, false, false, 2.4f};
    case RiseEv::Tax: return {"TAX", false, false, false, 2.4f};
    case RiseEv::Tribute: return {"TRIBUTE", false, false, false, 2.4f};
    default: return {"", false, false, false, 0};
  }
}
// ease-out with a little overshoot (the banner drops and settles)
float easeOutBack(float t) {
  t = std::clamp(t, 0.0f, 1.0f);
  const float c1 = 1.70158f, c3 = c1 + 1;
  return 1 + c3 * std::pow(t - 1, 3.0f) + c1 * std::pow(t - 1, 2.0f);
}
std::vector<std::string> wrapTo(const std::string& s, int maxChars) {
  std::vector<std::string> out;
  std::string line, word;
  auto flush = [&]() {
    if (word.empty()) return;
    if (!line.empty() && (int)(line.size() + 1 + word.size()) > maxChars) { out.push_back(line); line.clear(); }
    if (!line.empty()) line += ' ';
    line += word;
    word.clear();
  };
  for (char c : s) { if (c == ' ') flush(); else word += c; }
  flush();
  if (!line.empty()) out.push_back(line);
  return out;
}
}  // namespace

View::RiseViewState& RiseViewOps::st(View& v) {
  if (!v.riseView_) v.riseView_ = std::make_shared<View::RiseViewState>();
  return *v.riseView_;
}

ew::Gid RiseViewOps::kingdomByLow(const Game& g, uint32_t low) {
  if (!low) return 0;
  for (const realm::KingdomState& k : g.realm.kingdoms()) if ((uint32_t)k.id == low) return k.id;
  for (const Kingdom& k : g.world.kingdoms) if ((uint32_t)k.id == low) return k.id;
  return 0;
}

void RiseViewOps::armsOf(const Game& g, ew::Gid k, cult::Heraldry& h, uint32_t* color) {
  h = cult::Heraldry();
  const rui::Look L = rui::look(g, k);
  if (L.ok) rui::arms(L, h);
  if (!h.field) h.field = L.color ? L.color : rgba(150, 40, 40);
  if (color) *color = L.color ? L.color : h.field;
}

const Tex& RiseViewOps::badgeTex(View& v, int rank, uint32_t field, uint32_t trim, int size) {
  View::RiseViewState& S = st(v);
  const uint64_t key = ew::mix64(((uint64_t)field << 32 | trim) ^ ((uint64_t)rank << 8 | (uint64_t)size) ^ 0xBAD6Eull);
  auto it = S.badges.find(key);
  if (it != S.badges.end()) return it->second;
  return S.badges[key] = v.pix_->bake(art::rankBadge(rank, field, trim, size));
}

const Tex& RiseViewOps::armsTex(View& v, const cult::Heraldry& h, int size) { return v.armsTex(h, size); }

void RiseViewOps::dressHero(const Game& g, art::HumanLook& L) {
  L.armsOrnament = (uint16_t)(L.armsOrnament & ~art::RISE_MASK);
  if (!g.service.enlisted()) return;
  cult::Heraldry h;
  armsOf(g, g.service.sworn, h, nullptr);
  const int rank = std::clamp((int)g.service.rank, 1, 4);
  L.armsOrnament = (uint16_t)(L.armsOrnament | art::RISE_TABARD | art::riseRankBits(rank));
  L.tabardColor = h.field | 0xFF000000u;
  if (!L.pattern) L.patternColor = (h.charge ? h.charge : rgba(232, 200, 90)) | 0xFF000000u;
  if (rank == 4) { L.cloak = 3; L.cloakColor = h.field | 0xFF000000u; }
}

View::RiseViewStats View::riseViewStats() const { return riseView_ ? riseView_->stats : RiseViewStats(); }

// ---------------------------------------------------------------- events
void View::riseEvent(Game& g, const Event& e) {
  RiseViewState& S = RiseViewOps::st(*this);
  const RiseEv k = (RiseEv)std::min<int>(std::max(0, e.a), (int)RiseEv::COUNT - 1);
  S.stats.last = k;
  const Kind K = kindOf(k, (int)std::lround(e.f));
  if (!K.banner) {
    // the quiet ones: a coin chip by the purse (+ pay or tax, - tribute), a soft coin
    if (k == RiseEv::None) return;
    RiseViewState::Chip c;
    c.label = K.top;
    c.gold = (int)std::lround(e.f);
    c.out = k == RiseEv::Tribute;
    S.chips.push_back(c);
    if (S.chips.size() > 3) S.chips.erase(S.chips.begin());
    S.stats.toasts++;
    audio_->play(Sfx::Coin, c.out ? 0.85f : 1.1f, 0.45f);
    return;
  }
  RiseViewState::Banner b;
  b.kind = k;
  b.f = (int)std::lround(e.f);
  b.top = K.top;
  b.line = e.s;
  b.kingdom = RiseViewOps::kingdomByLow(g, e.id);
  if (!b.kingdom && (k == RiseEv::Enlisted || k == RiseEv::Promoted || k == RiseEv::Order || k == RiseEv::OrderDone || k == RiseEv::Discharged))
    b.kingdom = g.service.sworn;
  if (!b.kingdom && (k == RiseEv::Independence || k == RiseEv::Crowned)) b.kingdom = g.realm.playerRealm();
  if (k == RiseEv::Enlisted || k == RiseEv::Promoted || k == RiseEv::Discharged || k == RiseEv::Deserted)
    b.rank = std::clamp(b.f, 0, 4);
  b.dur = K.dur;
  b.bad = K.bad;
  b.major = K.major;
  // the herald's own words: the realm, the rank, the fief (the sim's notice already reads out the whole sentence)
  {
    const std::string realmT = b.kingdom ? rui::realmTitle(rui::look(g, b.kingdom), true) : std::string();
    const std::string realmN = b.kingdom ? rui::look(g, b.kingdom).name : std::string();
    std::string head;
    switch (k) {
      case RiseEv::Enlisted: b.top = "SWORN TO THE ARMY OF"; head = realmT; break;
      case RiseEv::Promoted: if (!realmN.empty()) head = std::string(service::rankName((service::Rank)b.rank)) + " OF " + realmN; break;
      case RiseEv::Discharged: head = realmT; break;
      case RiseEv::Deserted: if (!realmN.empty()) head = "A DESERTER OF " + realmN; break;
      case RiseEv::FiefGranted:
        if (!g.lordship.holdings.empty()) {
          const std::string fief = RiseViewOps::siteName(g, g.lordship.holdings.back().site);
          if (!fief.empty()) head = "LORD OF " + fief;
        }
        break;
      case RiseEv::Independence: case RiseEv::Crowned: head = realmT; break;
      case RiseEv::Order: if (b.line.compare(0, 8, "ORDERS: ") == 0) head = b.line.substr(8); break;
      case RiseEv::OrderDone: head = "+" + std::to_string(std::max(0, b.f)) + " MERIT" + (realmN.empty() ? std::string() : " IN THE ARMY OF " + realmN); break;
      default: break;
    }
    if (!head.empty()) b.line = head;
  }
  if (b.line.empty()) b.line = K.top;
  // the same moment twice in a row (a sim that reports a rank on load and on change) is one banner
  if (!S.queue.empty() && S.queue.back().kind == b.kind && S.queue.back().line == b.line) return;
  if (S.showing && S.cur.kind == b.kind && S.cur.line == b.line) return;
  S.queue.push_back(b);
  while (S.queue.size() > 6) S.queue.pop_front();   // a burst (a script, a long absence) keeps the latest six
  S.stats.banners++;
}

void View::riseUpdate(Game& g, float dt) {
  RiseViewState& S = RiseViewOps::st(*this);
  // banners age only while the world is on screen (the menu and the map stop the herald)
  const bool live = g.mode == Mode::Play || g.mode == Mode::Dialogue;
  for (size_t i = 0; i < S.chips.size();) {
    if (live) S.chips[i].t += dt;
    if (S.chips[i].t > 2.6f) S.chips.erase(S.chips.begin() + (long)i); else i++;
  }
  if (!live) return;
  if (S.showing) {
    S.cur.t += dt;
    if (S.cur.t >= S.cur.dur) { S.showing = false; S.gap = 0.25f; }
  }
  if (!S.showing) {
    if (S.gap > 0) { S.gap -= dt; return; }
    // the herald's ribbon and the biome banner own the top centre while they show
    if (S.queue.empty() || herald_.t > 0 || biomeBannerT_ > 0) return;
    S.cur = S.queue.front();
    S.queue.pop_front();
    S.cur.t = 0;
    S.showing = true;
    S.stats.shown++;
    // the sting
    switch (S.cur.kind) {
      case RiseEv::Enlisted: audio_->playSting(Audio::Sting::DrumRoll, 1.0f, 0.9f); break;
      case RiseEv::Promoted: audio_->playSting(Audio::Sting::Fanfare, 1.0f, 0.9f); break;
      case RiseEv::Discharged: audio_->playSting(Audio::Sting::Fanfare, 0.84f, 0.6f); break;
      case RiseEv::FiefGranted: audio_->playSting(Audio::Sting::Fanfare, 0.94f, 0.85f); break;
      case RiseEv::Independence: audio_->playSting(Audio::Sting::HornCall, 1.0f, 1.0f); break;
      case RiseEv::Crowned: audio_->playSting(Audio::Sting::HornCall, 1.06f, 1.0f); audio_->playSting(Audio::Sting::Fanfare, 1.06f, 0.7f); break;
      case RiseEv::WarlordNear: audio_->playSting(Audio::Sting::WarAlarm, 1.0f, 0.9f); break;
      case RiseEv::BossRaid: audio_->playSting(Audio::Sting::WarAlarm, 0.82f, 0.95f); break;
      case RiseEv::Mission: case RiseEv::Order: audio_->playSting(Audio::Sting::Seal, 1.0f, 0.75f); break;
      case RiseEv::MissionDone: audio_->playSting(S.cur.bad ? Audio::Sting::Seal : Audio::Sting::Fanfare, S.cur.bad ? 0.8f : 1.12f, 0.6f); break;
      case RiseEv::OrderDone: audio_->playSting(Audio::Sting::DrumRoll, 1.18f, 0.55f); break;
      case RiseEv::OrderFailed: case RiseEv::Deserted: case RiseEv::TributeRefused: audio_->playSting(Audio::Sting::DrumRoll, 0.7f, 0.65f); break;
      default: break;
    }
  }
}

// ---------------------------------------------------------------- the HUD
void View::drawRiseHud(Game& g) {
  RiseViewState& S = RiseViewOps::st(*this);
  S.stats.insignia = false;
  if (g.mode != Mode::Play && g.mode != Mode::Dialogue) return;
  Pix& P = *pix_;
  const float L = (float)Pix::SL, T = (float)Pix::ST, R = (float)(Pix::W - Pix::SR);
  // ---- the rank's badge under the level, beside the vitals (the sworn army's field and trim)
  if (g.service.enlisted()) {
    cult::Heraldry h;
    RiseViewOps::armsOf(g, g.service.sworn, h, nullptr);
    const Tex& b = RiseViewOps::badgeTex(*this, (int)g.service.rank, h.field, h.charge, 11);
    P.blit(b, L + 10, T + 15);
    S.stats.insignia = true;
  }
  // ---- the coin chips by the purse (they rise a little and fade)
  for (size_t i = 0; i < S.chips.size(); i++) {
    const RiseViewState::Chip& c = S.chips[i];
    const float a = std::clamp(std::min(c.t * 5.0f, (2.6f - c.t) * 2.0f), 0.0f, 1.0f);
    // under the purse (clear of the arrows' count and the level-up line), the newest on top, drifting up a little
    const float x = L + 6, y = T + (g.perkPts > 0 ? 56.0f : 45.0f) + (float)(S.chips.size() - 1 - i) * 10 - std::floor(std::min(c.t, 0.5f) * 6.0f);
    const std::string s = (c.out ? "-" : "+") + std::to_string(c.gold) + " " + c.label;
    P.rect(x - 2, y - 2, (float)P.textW(s, 1) + 4, 10, Color(0.03f, 0.02f, 0.05f, 0.55f * a));
    P.textS(x, y, s, 1, c.out ? Color(0.95f, 0.66f, 0.5f, a) : Color(kGoldC.r, kGoldC.g, kGoldC.b, a));
  }
  // ---- the herald's banner of the moment
  riseBanBottom_ = 0;
  if (!S.showing) return;
  const RiseViewState::Banner& B = S.cur;
  const float tIn = 0.38f, tOut = 0.32f;
  const float u = B.t < tIn ? easeOutBack(B.t / tIn) : 1.0f;
  const float o = B.t > B.dur - tOut ? (B.t - (B.dur - tOut)) / tOut : 0.0f;
  const float a = std::clamp(std::min(B.t / 0.18f, 1.0f - o), 0.0f, 1.0f);
  cult::Heraldry hr;
  uint32_t kcol = 0;
  RiseViewOps::armsOf(g, B.kingdom, hr, &kcol);
  if (!B.kingdom) { hr.field = B.bad ? rgba(120, 36, 34) : rgba(150, 110, 50); kcol = hr.field; }
  // the words: the top line, the moment in up to two lines between the shields
  const float minX = L + 136, maxX = R - 76;
  const float avail = std::max(160.0f, maxX - minX);
  const int perLine = std::max(16, (int)((avail - 64) / 6));
  std::vector<std::string> lines = wrapTo(B.line, std::min(perLine, 40));
  if (lines.size() > 2) { lines.resize(2); lines[1] += "..."; }
  int tw = P.textW(B.top, 1);
  for (const std::string& s : lines) tw = std::max(tw, P.textW(s, 1));
  const float side = 26.0f;
  const float w = std::floor(std::min(avail, (float)tw + side * 2 + 14)), h = 22.0f + 10.0f * (float)lines.size();
  float x = std::floor(Pix::W / 2.0f - w / 2);
  if (maxX - w >= minX) x = std::clamp(x, minX, maxX - w);
  const float y0 = std::max(T + 4, bossBarBottom_ + 3);
  const float y = std::floor(y0 - (1 - u) * (h + 8) - o * 10);
  riseBanX0_ = x; riseBanX1_ = x + w; riseBanBottom_ = y + h + 5;
  const Color field = colOf(kcol);
  const Color dark(field.r * 0.32f + 0.02f, field.g * 0.32f + 0.02f, field.b * 0.32f + 0.03f, 0.92f * a);
  const Color gold(kGoldC.r, kGoldC.g, kGoldC.b, a);
  // the cloth: a dark field-tinted panel, the realm's colour down both edges, a notched hem (two tails and a point)
  const float notch = 5;
  P.rect(x, y + 3, w, h - 3, dark);
  P.rect(x, y + 3, 3, h - 3, Color(field.r, field.g, field.b, 0.95f * a));
  P.rect(x + w - 3, y + 3, 3, h - 3, Color(field.r * 0.75f, field.g * 0.75f, field.b * 0.75f, 0.95f * a));
  // the hem cut in dags (a row of points, the cloth's edge in the field's colour), the side stripes ending in points
  const int dag = 8;
  for (int i = 0; i < (int)w; i++) {
    const int ph = i % dag, d = (int)notch - 1 - std::abs(ph - dag / 2) * 2 / 2;
    if (d <= 0) continue;
    const bool edge = i < 3 || i >= (int)w - 3;
    P.rect(x + i, y + h, 1, (float)d, edge ? Color(field.r * 0.85f, field.g * 0.85f, field.b * 0.85f, 0.95f * a) : dark);
    P.rect(x + i, y + h + d - 1, 1, 1, Color(field.r * 0.7f + 0.05f, field.g * 0.7f + 0.05f, field.b * 0.7f + 0.05f, 0.9f * a));
  }
  P.rect(x + 3, y + h - 2, w - 6, 1, Color(gold.r, gold.g, gold.b, 0.55f * a));   // a gold stripe over the hem
  // the gilded rod across the top with its knobs, the cloth's shade under it
  P.rect(x - 3, y + 1, w + 6, 2, gold);
  P.rect(x - 3, y + 3, w + 6, 1, Color(0.55f, 0.36f, 0.14f, a));
  P.rect(x - 5, y, 3, 4, gold);
  P.rect(x + w + 2, y, 3, 4, Color(0.80f, 0.60f, 0.26f, a));
  P.rect(x + 3, y + 4, w - 6, 1, Color(0, 0, 0, 0.35f * a));
  // a glint running along the rod as it lands (the grand moments glint twice)
  if (B.t < (B.major ? 1.6f : 0.9f)) {
    const float gx = x + std::fmod(B.t * 1.1f, 0.8f) / 0.8f * w;
    P.rect(gx, y + 1, 3, 1, Color(1, 1, 0.9f, 0.9f * a));
  }
  // the shields: the realm's arms on the left, the rank's badge (or the arms again) on the right
  const Color tint(1, 1, 1, a);
  const Tex& arms = armsTex(hr, 20);
  P.blit(arms, x + 6, y + std::floor((h - arms.h) / 2) + 2, false, tint);
  if (B.rank > 0) {
    const Tex& bd = RiseViewOps::badgeTex(*this, B.rank, hr.field, hr.charge, 16);
    P.blit(bd, x + w - 6 - bd.w - 2, y + std::floor((h - bd.h) / 2) + 2, false, tint);
  } else P.blit(arms, x + w - 6 - arms.w, y + std::floor((h - arms.h) / 2) + 2, false, tint);
  // the words
  const float cx = std::floor(x + w / 2);
  const Color topC = B.bad ? Color(1.0f, 0.62f, 0.50f, a) : Color(0.90f, 0.76f, 0.46f, a);
  P.text(cx, y + 7, B.top, 1, topC, 1);
  const Color tc = B.bad ? Color(1.0f, 0.86f, 0.80f, a) : Color(field.r * 0.22f + 0.76f, field.g * 0.22f + 0.73f, field.b * 0.22f + 0.66f, a);
  for (size_t i = 0; i < lines.size(); i++) {
    const float ly = y + 18 + 10.0f * (float)i;
    P.text(cx + 1, ly + 1, lines[i], 1, Color(0, 0, 0, 0.8f * a), 1);
    P.text(cx, ly, lines[i], 1, tc, 1);
  }
  S.stats.bannerX = x; S.stats.bannerW = w; S.stats.bannerY = y; S.stats.bannerH = h + notch;
}
