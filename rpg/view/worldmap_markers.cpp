// worldmap_markers.cpp - the world map's markers that are not the places' own icons (SOLID 3.4d, from
// View::drawWorldMap): the war (sieges, burned and hungry towns, refugees, occupiers' banners, the news scrolls), the
// player's property, the party (party_view.cpp draws it) and the sea (ports, ferry routes, our boats). Each takes the
// frame's MapFrame (worldmap_internal.h) and is called by drawWorldMap in this order: war, home, party, sea.
#include <SDL3/SDL.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <vector>
#include "rpg/sim/home.h"
#include "rpg/sim/sea.h"
#include "rpg/story/story.h"
#include "rpg/view/realm_ui.h"
#include "rpg/view/rise_view.h"
#include "rpg/view/view.h"
#include "rpg/world/biomes.h"
#include "rpg/world/coords.h"
#include "rpg/world/settlement.h"
#include "rpg/world/source.h"
#include "rpg/world/town_plan.h"
#include "rpg/world/urban.h"
#include "rpg/view/worldmap_internal.h"

using namespace wmap;

// (M4) the war on the map: the troubled settlements' markers and the heard news scrolls (drawWorldMap)
void View::mapMarkersWar(Game& g, MapFrame& M) {
  Pix& P = M.P;
  const float x = M.x, y = M.y, w = M.w, h = M.h;
  const double top = M.top;
  const Vec2 hp = M.hp;
  auto& taken = M.taken;
  auto& warBoxes = M.warBoxes;
  auto& marks = M.marks;
  // (M4) the war on the map: a marker at each troubled settlement the hero has seen or heard of (beside its icon, up and
  // right), a scroll at every other heard event; the side column names the state of the selected place
  S.markers = 0;
  if (g.world.endless && !S.geo) {
    std::unordered_set<ew::Gid> marked;
    auto inView = [&](const Vec2& q) { return q.x >= -12 && q.y >= -12 && q.x <= w + 12 && q.y <= h + 12; };
    std::unordered_set<ew::Gid> heardAt;
    for (const realm::WorldEvent& e : g.realm.events()) if (e.heard && e.site) heardAt.insert(e.site);
    for (const realm::SettlementState* st : rui::knownStates(g)) {
      const uint16_t f = st->flags;
      int wi = -1;
      bool occupied = false;
      if (f & realm::SS_BESIEGED) wi = WI_SIEGE;
      else if (f & (realm::SS_BURNED | realm::SS_RUINED)) wi = WI_BURNED;
      else if (f & realm::SS_OCCUPIED) occupied = true;
      else if (f & realm::SS_REFUGEES) wi = WI_REFUGEES;
      else if (f & realm::SS_FAMINE) wi = WI_HUNGRY;
      if (wi < 0 && !occupied) continue;
      if (!g.explored.seen(st->gx, st->gy) && !heardAt.count(st->site)) continue;
      const Vec2 q = M.toScr(st->gx + 0.5, st->gy + 0.5);
      if (!inView(q)) continue;
      // beside the settlement's icon (up and to the right) so both read
      const float mx = std::floor(q.x + 6), my = std::floor(q.y - 9);
      if (occupied) {
        // a little banner on a pole in the occupier's colours
        const rui::Look L = rui::look(g, st->owner);
        const Color fc = colOf(L.color ? L.color : C(160, 40, 44)), tc = colOf(L.color2 ? L.color2 : C(236, 210, 120));
        P.rect(mx - 1, my - 1, 9, 11, Color(0.94f, 0.88f, 0.74f, 0.8f));
        P.rect(mx, my, 1, 10, kInk);
        P.rect(mx + 1, my, 6, 6, kInk);
        P.rect(mx + 1, my + 1, 5, 4, fc);
        P.rect(mx + 3, my + 2, 1, 2, tc);
        P.rect(mx + 1, my + 5, 2, 1, fc);
        P.rect(mx + 4, my + 5, 2, 1, fc);
        M.legendAdd(-5);
      } else {
        const Tex& t = S.warIcons[(size_t)wi];
        P.blit(t, mx, my);
        if (wi == WI_SIEGE) {   // a slow pulse round a siege still being fought
          const float pr = 7 + 1.5f * (0.5f + 0.5f * std::sin(t_ * 3));
          for (int k = 0; k < 18; k++) {
            const float a = k / 18.0f * 6.2831853f;
            P.rect(std::floor(mx + t.w / 2.0f + std::cos(a) * pr), std::floor(my + t.h / 2.0f + std::sin(a) * pr), 1, 1, Color(0.75f, 0.12f, 0.1f, 0.7f));
          }
        }
        M.legendAdd(wi == WI_SIEGE ? -3 : wi == WI_BURNED ? -4 : wi == WI_REFUGEES ? -6 : -9);
      }
      taken.push_back({mx - 1, my - 1, mx + 11, my + 11});
      warBoxes.push_back({mx - 1, my - 1, mx + 11, my + 11});
      marked.insert(st->site);
      S.markers++;
    }
    // (M5, 15.12) the settlements whose people the hero has seen (a census is known): a hungry one shows the bowl, a
    // festival a little string of pennants, beside its icon like the realm's markers
    for (int si = 0; si < (int)g.world.sites.size(); si++) {
      const Site& s = g.world.sites[(size_t)si];
      if (!s.settlement() || !s.discovered || marked.count(s.id) || !g.life.find(s.id)) continue;
      const uint16_t f = g.life.moodFlags(s.id);
      const bool fest = g.life.festival(s.id, g.day) || (f & life::MF_FESTIVAL);
      const bool hungry = (f & (life::MF_HUNGRY | life::MF_FAMINE)) != 0;
      if (!fest && !hungry) continue;
      const Vec2 q = M.toScr(s.ex + g.world.ox + 0.5, s.ey + g.world.oy + 0.5);
      if (!inView(q)) continue;
      const float mx = std::floor(q.x + 6), my = std::floor(q.y - 9);
      if (hungry) {
        P.blit(S.warIcons[WI_HUNGRY], mx, my);
        M.legendAdd(-9);
      } else {   // the pennants: a sagging cord with three flags in red and gold
        P.rect(mx - 1, my + 1, 11, 7, Color(0.94f, 0.88f, 0.74f, 0.8f));
        const int sag[9] = {0, 1, 1, 2, 2, 2, 1, 1, 0};
        for (int k = 0; k < 9; k++) P.rect(mx + k, my + 2 + sag[k], 1, 1, kInk);
        for (int k = 0; k < 3; k++) {
          const Color fc = (k & 1) ? Color(0.95f, 0.78f, 0.25f) : Color(0.80f, 0.20f, 0.17f);
          const float fx = mx + 1 + k * 3, fy = my + 3 + sag[1 + k * 3];
          P.rect(fx, fy, 2, 2, fc);
          P.rect(fx, fy + 2, 1, 1, fc);
        }
      }
      taken.push_back({mx - 1, my - 1, mx + 11, my + 11});
      marked.insert(s.id);
      S.markers++;
    }
    // the other heard events (a war declared, a famine, prices rising...): a scroll where it happened
    // (M4 integration) newest first, only the last 90 days' news, at most 12 scrolls and never on top of another
    // marker: after a long game the whole continent used to be papered with scrolls (unreadable on a phone)
    {
      const auto& evs = g.realm.events();
      int scrolls = 0;
      S.news.clear();
      std::vector<Box> drawn;   // the scrolls keep off the markers and each other; labels still win over them
      for (size_t ei = evs.size(); ei-- > 0 && scrolls < 12;) {
        const realm::WorldEvent& e = evs[ei];
        if (!e.heard || (e.site && marked.count(e.site))) continue;
        if (!e.gx && !e.gy) continue;
        if ((uint16_t)((uint16_t)g.day - e.day) > 90) continue;
        // (fixer M4 r2) only where the map knows the land: a scroll on blank fogged parchment said nothing
        if (!g.explored.seen(e.gx, e.gy)) continue;
        const Vec2 q = M.toScr(e.gx + 0.5, e.gy + 0.5);
        if (!inView(q)) continue;
        const Tex& t = S.warIcons[WI_NEWS];
        const float mx = std::floor(q.x - t.w / 2.0f), my = std::floor(q.y - t.h / 2.0f);
        bool clash = false;
        for (const std::vector<Box>* L : {&taken, &drawn})
          for (const Box& b : *L)
            if (mx < b.x1 && mx + t.w > b.x0 && my < b.y1 && my + t.h > b.y0) { clash = true; break; }
        if (clash) continue;
        P.blit(t, mx, my);
        if (e.id == S.newsSel) P.frame(mx - 2, my - 2, t.w + 4, t.h + 4, Color(1, 1, 1));
        S.news.push_back({e.id, Vec2(q.x + x, q.y + y)});
        drawn.push_back({mx - 1, my - 1, mx + t.w + 1, my + t.h + 1});
        warBoxes.push_back({mx - 1, my - 1, mx + t.w + 1, my + t.h + 1});
        M.legendAdd(-7);
        S.markers++;
        scrolls++;
        if (e.site) marked.insert(e.site);
      }
    }
    // (M12) the rise on the map: warbands' and warlords' camps, beasts' lairs that raid, resettled places, the player's
    // holdings and the crown at the player's seat
    {
      RiseViewState& RS = RiseViewOps::st(*this);
      RS.stats.mapMarks = 0;
      auto mark = [&](int kind, double gx, double gy, uint32_t c1, uint32_t c2) {
        const Vec2 q = M.toScr(gx + 0.5, gy + 0.5);
        if (!inView(q)) return;
        float mx = std::floor(q.x - 4), my = std::floor(q.y - (kind == RM_SEAT || kind == RM_HOLDING ? 16 : 4));
        // (M12 fixer r2) the glyph's whole box (the seat's and a holding's pennant stand 14 px tall, from my - 2)
        const float gT = kind == RM_SEAT || kind == RM_HOLDING ? 3.0f : 1.0f, gB = kind == RM_SEAT || kind == RM_HOLDING ? 13.0f : 11.0f;
        // (M12 fixer) marks that fall on one another (the seat, a lair, a camp and the hero at a far zoom) step aside
        // to the first clear place round their spot, a thin leader line back to it
        {
          auto hit = [&](float x0, float y0) {
            const Box b{x0 - 1, y0 - gT, x0 + 10, y0 + gB};
            if (b.x0 < hp.x + 9 && b.x1 > hp.x - 9 && b.y0 < hp.y + 8 && b.y1 > hp.y - 10) return true;
            for (const Box& o : warBoxes) if (b.x0 < o.x1 && b.x1 > o.x0 && b.y0 < o.y1 && b.y1 > o.y0) return true;
            return false;
          };
          if (hit(mx, my)) {
            static const float off[12][2] = {{12, 0}, {-12, 0}, {0, -12}, {0, 12}, {12, -12}, {-12, -12}, {12, 12}, {-12, 12},
                                             {24, 0}, {-24, 0}, {0, -24}, {0, 24}};
            for (const auto& o : off)
              if (!hit(mx + o[0], my + o[1])) {
                const float nx = mx + o[0], ny = my + o[1];
                const float ax = q.x, ay = q.y, bx = nx + 4.5f, by = ny + 4.5f;
                const int steps = (int)std::max(std::fabs(bx - ax), std::fabs(by - ay));
                for (int i = 2; i < steps - 3; i++) {
                  const float t = (float)i / (float)std::max(1, steps);
                  P.rect(std::floor(ax + (bx - ax) * t), std::floor(ay + (by - ay) * t), 1, 1, Color(0.30f, 0.20f, 0.12f, 0.75f));
                }
                mx = nx; my = ny;
                break;
              }
          }
        }
        riseMapGlyph(P, kind, mx, my, c1, c2);
        taken.push_back({mx - 1, my - gT, mx + 10, my + gB});
        warBoxes.push_back({mx - 1, my - gT, mx + 10, my + gB});
        M.legendAdd(-10 - kind);
        RS.stats.mapMarks++;
      };
      for (const realm::Camp& cp : g.realm.camps()) {
        if ((cp.flags & realm::CF_CLEARED) || !(cp.flags & (realm::CF_WARBAND | realm::CF_WARLORD))) continue;
        if (!g.explored.seen(cp.gx, cp.gy) && !heardAt.count(cp.site)) continue;
        const bool lord = (cp.flags & realm::CF_WARLORD) && cp.realm;
        const rui::Look CL = lord ? rui::look(g, cp.realm) : rui::Look();
        mark(lord ? RM_WARLORD : RM_WARBAND, cp.gx, cp.gy, CL.color ? CL.color : C(120, 30, 34), CL.color2 ? CL.color2 : C(30, 26, 30));
      }
      for (const realm::Lair& lr : g.realm.lairs())
        if (!lr.slain && (lr.raids > 0 || lr.wokeDay > 0) && (g.explored.seen(lr.gx, lr.gy) || lr.raids > 0)) mark(RM_LAIR, lr.gx, lr.gy, C(150, 40, 30), 0);
      for (const realm::SettlementState* st : rui::knownStates(g))
        if ((st->flags & realm::SS_REBUILDING) && !marked.count(st->site) && (g.explored.seen(st->gx, st->gy) || heardAt.count(st->site)))
          mark(RM_REBUILT, st->gx + 1.5, st->gy - 0.5, C(176, 132, 70), 0);
      const ew::Gid mine = g.realm.playerRealm();
      cult::Heraldry mh;
      if (mine) rui::arms(rui::look(g, mine), mh);
      for (size_t hi = 0; hi < g.lordship.holdings.size(); hi++) {
        const realm::SettlementState* st = g.realm.settlement(g.lordship.holdings[hi].site);
        if (!st) continue;
        const bool seat = mine && hi == 0;
        const uint32_t fc = mine ? mh.field : C(200, 170, 90), tc = mine ? (mh.charge ? mh.charge : C(236, 200, 90)) : C(120, 80, 30);
        mark(seat ? RM_SEAT : RM_HOLDING, st->gx, st->gy, fc, tc);
      }
    }
    // a border is on screen: say what the dashed lines are
    M.legendAdd(-8);
  }
}

// (M7) the player's property: a little house over each plot (drawWorldMap)
void View::mapMarkersHome(Game& g, MapFrame& M) {
  Pix& P = M.P;
  const float w = M.w, h = M.h;
  const float z = M.z;
  const double left = M.left, top = M.top;
  auto& labs = M.labs;
  auto& taken = M.taken;
  // (M7) the player's property: a little house in the HUD's gold over each plot (its ground outlined when zoomed in
  // close enough to see it), labelled with its name
  if (!S.geo)
    for (const home::Plot& hp : g.home.plots) {
      const Vec2 s = M.toScr(hp.gx + hp.w * 0.5, hp.gy + hp.h * 0.5);
      if (s.x < -10 || s.y < -10 || s.x > w + 10 || s.y > h + 10) continue;
      const float pw = (float)(hp.w / z), ph = (float)(hp.h / z);
      if (pw >= 10) {
        const Vec2 o = M.toScr(hp.gx, hp.gy);
        P.frame(std::floor(o.x), std::floor(o.y), std::floor(pw), std::floor(ph), Color(0.55f, 0.32f, 0.08f, 0.9f));
      }
      const float cx = std::floor(s.x), cy = std::floor(s.y);
      // dark outline, then a gold roof, a cream wall and a door
      P.rect(cx - 6, cy - 6, 13, 3, kInk);
      P.rect(cx - 4, cy - 8, 9, 2, kInk);
      P.rect(cx - 5, cy - 3, 11, 9, kInk);
      P.rect(cx - 2, cy - 7, 5, 1, Color(0.98f, 0.82f, 0.42f));
      P.rect(cx - 4, cy - 6, 9, 1, Color(0.98f, 0.82f, 0.42f));
      P.rect(cx - 5, cy - 5, 11, 1, Color(0.80f, 0.58f, 0.22f));
      P.rect(cx - 4, cy - 2, 9, 7, Color(0.96f, 0.91f, 0.78f));
      P.rect(cx + 2, cy - 2, 2, 7, Color(0.80f, 0.74f, 0.62f));   // the shaded east wall (light from the top-left)
      P.rect(cx - 1, cy + 1, 3, 4, Color(0.45f, 0.27f, 0.14f));
      taken.push_back({cx - 7, cy - 9, cx + 8, cy + 7});
      std::string nm = hp.name;
      if (nm.empty()) nm = "HOME";
      labs.push_back({1, s.x, s.y + 9, s.y - 20, nm, Color(0.45f, 0.24f, 0.04f), Color(1.0f, 0.95f, 0.8f, 0.8f), false});
    }
}

// (M8) the party: companions waiting out in the land, the graves of the fallen (drawWorldMap)
void View::mapMarkersParty(Game& g, MapFrame& M) {
  const float x = M.x, y = M.y, w = M.w, h = M.h;
  auto toScr = [&](double gx, double gy) { return M.toScr(gx, gy); };   // (drawPartyWorldMap takes it by address)
  // (M8) the party: companions waiting out in the land, the graves of the fallen (party_view.cpp)
  if (!S.geo && (!g.party.members.empty() || !g.party.fallen.empty())) {
    auto scr = [](void* c, double x, double y) { return (*static_cast<decltype(toScr)*>(c))(x, y); };
    drawPartyWorldMap(g, scr, &toScr, w, h);
  }
}

// (M10) the sea on the map: the known ports, the ferry routes, the boats we own (drawWorldMap)
void View::mapMarkersSea(Game& g, MapFrame& M) {
  Pix& P = M.P;
  const float x = M.x, y = M.y, w = M.w, h = M.h;
  const Vec2 hp = M.hp;
  // (M10) the sea on the map: the ports the hero knows (an anchor by the town's icon), the ferry routes between them
  // (dashed over the water), the boats we own where they lie (a little sail), and under the hero's arrow his boat
  if (!S.geo && g.world.endless) {
    auto boatGlyph = [&](Vec2 s, bool ours) {
      const float x = std::floor(s.x), y = std::floor(s.y);
      P.rect(x - 4, y + 1, 9, 2, Color(0.30f, 0.18f, 0.10f));            // the hull
      P.rect(x - 3, y + 3, 7, 1, Color(0.30f, 0.18f, 0.10f));
      P.rect(x - 3, y + 1, 7, 1, Color(0.62f, 0.40f, 0.22f));
      P.rect(x, y - 5, 1, 6, Color(0.26f, 0.16f, 0.10f));                  // the mast
      P.rect(x + 1, y - 4, 3, 4, ours ? Color(0.96f, 0.92f, 0.80f) : Color(0.78f, 0.74f, 0.66f));   // the sail
      P.rect(x + 1, y - 4, 1, 4, Color(0.80f, 0.74f, 0.60f));
    };
    std::vector<Vec2> ports;
    for (int i = 0; i < (int)g.world.sites.size(); i++) {
      const Site& st = g.world.sites[(size_t)i];
      if (!st.discovered || !g.sea.knowsPort(st.id)) continue;
      const Vec2 s = M.toScr(st.ex + g.world.ox + 0.5, st.ey + g.world.oy + 0.5);
      ports.push_back(s);
    }
    for (size_t a = 0; a < ports.size(); a++) {   // each port to its nearest known neighbour: a dashed passage
      int best = -1;
      float bd = 1e9f;
      for (size_t b = 0; b < ports.size(); b++)
        if (b != a) { const float d = len2(ports[b] - ports[a]); if (d < bd) { bd = d; best = (int)b; } }
      if (best < 0 || best < (int)a) continue;
      const Vec2 p0 = ports[a], d = ports[(size_t)best] - p0;
      const int n = (int)std::max(std::fabs(d.x), std::fabs(d.y));
      for (int k = 0; k < n; k++) {
        if ((k / 3) % 2) continue;
        const Vec2 q = p0 + d * ((float)k / std::max(1, n));
        if (q.x < 0 || q.y < 0 || q.x >= w || q.y >= h) continue;
        P.rect(std::floor(q.x), std::floor(q.y), 1, 1, Color(0.22f, 0.36f, 0.56f, 0.85f));
      }
    }
    for (const Vec2& s : ports) {   // the anchor beside the port's icon
      if (s.x < -8 || s.y < -8 || s.x > w + 8 || s.y > h + 8) continue;
      const float x = std::floor(s.x + 6), y = std::floor(s.y - 9);
      P.rect(x - 2, y - 1, 7, 9, Color(0.93f, 0.88f, 0.76f, 0.8f));
      P.rect(x + 1, y, 1, 6, Color(0.18f, 0.24f, 0.36f));
      P.rect(x, y + 1, 3, 1, Color(0.18f, 0.24f, 0.36f));
      P.rect(x - 1, y + 5, 5, 1, Color(0.18f, 0.24f, 0.36f));
      P.rect(x - 1, y + 4, 1, 1, Color(0.18f, 0.24f, 0.36f));
      P.rect(x + 3, y + 4, 1, 1, Color(0.18f, 0.24f, 0.36f));
    }
    for (int i = 0; i < (int)g.sea.boats.size(); i++) {
      const sea::Boat& b = g.sea.boats[(size_t)i];
      if (b.sunk || i == g.sea.aboard) continue;
      const Vec2 s = M.toScr(b.gx + 0.5, b.gy + 0.5);
      if (s.x < -8 || s.y < -8 || s.x > w + 8 || s.y > h + 8) continue;
      boatGlyph(s, true);
    }
    if (g.sea.atSea()) boatGlyph(hp + Vec2(0, 7), true);
  }
}
