// M12 VIEW lane script commands (rpg/view/script_api.h; `embervale --script-help` lists them):
//   riseview fire <kind> [f] [words...]   test only: an Ev::Rise moment as the SERVICE / LORD lanes send it (kind:
//                         enlisted promoted wage order orderdone orderfailed discharged deserted mission missiondone
//                         fief tax tribute tributerefused independence crowned warlord bossraid; f its number; the words
//                         its line; it names the sworn army, else the player's realm, else the land's kingdom)
//   riseview stage village|camp|rebuild|raid   test only: lay out what the rise looks like round the hero for a review
//                         (village: a lord's banner and a watchtower stamped in the nearest settlement; camp: the
//                         nearest bandit camp noted and stood by; rebuild / raid: stand at the nearest settlement)
//   riseview works <work...> [raising <work>]  test only: the first holding's works (banner watchtower market shrine
//                         granary palisade hall militia walls) and the one being raised
//   riseview row <n>      pick row n of the open STANDING page (as a tap on it)
//   riseview spawn steward|warlord|envoy [n]  test only: n people of that role (default 1) beside the hero
//   expect riseview banners|toasts|shown <n>   at least n banners queued / coin chips / banners shown so far
//   expect riseview showing 0|1           a banner is on screen now
//   expect riseview last <kind>           the last moment's kind
//   expect riseview insignia 0|1          the rank's badge was drawn on the HUD last frame
//   expect riseview standing <n>          the STANDING page listed at least n rows last frame
//   expect riseview map <n>               the world map drew at least n rise markers last frame (camps, lairs, holdings)
//   expect riseview world banners|works|warlord|rebuild|scars <n>   the last frame drew at least n of those
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include "rpg/art/art_props.h"
#include "rpg/sim/game.h"
#include "rpg/sim/lordship.h"
#include "rpg/story/story.h"
#include "rpg/view/realm_ui.h"
#include "rpg/view/rise_view.h"
#include "rpg/view/script_api.h"
#include "rpg/view/view.h"
#include "rpg/world/source.h"

namespace {

struct KindName { const char* n; RiseEv k; };
const KindName kKinds[] = {{"enlisted", RiseEv::Enlisted}, {"promoted", RiseEv::Promoted}, {"wage", RiseEv::Wage}, {"order", RiseEv::Order},
                           {"orderdone", RiseEv::OrderDone}, {"orderfailed", RiseEv::OrderFailed}, {"discharged", RiseEv::Discharged},
                           {"deserted", RiseEv::Deserted}, {"mission", RiseEv::Mission}, {"missiondone", RiseEv::MissionDone},
                           {"fief", RiseEv::FiefGranted}, {"tax", RiseEv::Tax}, {"tribute", RiseEv::Tribute},
                           {"tributerefused", RiseEv::TributeRefused}, {"independence", RiseEv::Independence}, {"crowned", RiseEv::Crowned},
                           {"warlord", RiseEv::WarlordNear}, {"bossraid", RiseEv::BossRaid}, {"none", RiseEv::None}};
RiseEv kindByName(const std::string& s, bool& ok) {
  for (const KindName& k : kKinds) if (s == k.n) { ok = true; return k.k; }
  ok = false;
  return RiseEv::None;
}
const char* nameOf(RiseEv k) {
  for (const KindName& n : kKinds) if (n.k == k) return n.n;
  return "?";
}

ew::Gid landHere(Game& g) {
  if (!g.world.src) return 0;
  const int32_t gx = g.world.ox + (int32_t)std::floor(g.pl().p.x / TILE), gy = g.world.oy + (int32_t)std::floor(g.pl().p.y / TILE);
  return rui::landOwnerAt(g, gx, gy);
}

std::vector<int> nearest(Game& g, bool settlements, SiteType only = SiteType::COUNT) {
  std::vector<std::pair<float, int>> d;
  const float px = g.pl().p.x / TILE, py = g.pl().p.y / TILE;
  for (size_t i = 0; i < g.world.sites.size(); i++) {
    const Site& s = g.world.sites[i];
    if (settlements ? !s.settlement() : s.type != only) continue;
    d.push_back({(s.ex - px) * (s.ex - px) + (s.ey - py) * (s.ey - py), (int)i});
  }
  std::sort(d.begin(), d.end());
  std::vector<int> out;
  for (auto& e : d) out.push_back(e.second);
  return out;
}

bool cmdRiseView(ScriptCtx& c) {
  Game& g = c.game;
  const std::string what = c.arg(1);
  if (what == "fire") {
    bool ok = false;
    const RiseEv k = kindByName(c.arg(2), ok);
    if (!ok) { c.fail("riseview fire: unknown kind " + c.arg(2)); return true; }
    const float f = c.arg(3).empty() ? 1.0f : (float)std::atof(c.arg(3).c_str());
    std::string line = c.rest(4);
    ew::Gid kid = g.service.sworn ? g.service.sworn : g.realm.playerRealm() ? g.realm.playerRealm() : landHere(g);
    if (k == RiseEv::Independence || k == RiseEv::Crowned) kid = g.realm.playerRealm() ? g.realm.playerRealm() : kid;
    Event e{Ev::Rise, g.pl().p, (int)k, f, line};
    e.id = (uint32_t)kid;
    g.events.push_back(e);
    return true;
  }
  if (what == "stage") {
    const std::string where = c.arg(2);
    Map& m = g.world.over;
    if (where == "village") {
      // a lord's banner before the elder's door (or the heart) and a watchtower at the village's edge, as the LORD
      // lane's works will stand them; trees and clutter under them cleared
      const std::vector<int> near = nearest(g, true);
      if (near.empty()) { c.fail("riseview stage village: no settlement loaded"); return true; }
      const Site& s = g.world.sites[(size_t)near[0]];
      auto freeAt = [&](int x, int y) {
        if (!m.in(x, y) || m.blocked(x, y)) return false;
        if (m.bldgAt[(size_t)y * m.w + x] >= 0 || (!m.wall.empty() && m.wall[(size_t)y * m.w + x])) return false;
        const int pr = m.prop[(size_t)y * m.w + x];
        return pr == 0 || !propSolid((art::Prop)(pr - 1));
      };
      auto placeNear = [&](art::Prop p, int cx, int cy, int r0, int r1) {
        for (int r = r0; r <= r1; r++)
          for (int dy = -r; dy <= r; dy++)
            for (int dx = -r; dx <= r; dx++) {
              if (std::max(std::abs(dx), std::abs(dy)) != r) continue;
              const int x = cx + dx, y = cy + dy;
              if (!freeAt(x, y) || !freeAt(x, y - 1) || !freeAt(x + 1, y) || !freeAt(x - 1, y) || !freeAt(x, y + 1)) continue;
              m.setP(x, y, (int)p + 1);
              return Vec2(x, y);
            }
        return Vec2(-1, -1);
      };
      const Vec2 b = placeNear(art::Prop::LordBanner, s.ex, s.ey, 1, 8);
      // the tower on open ground west of the standard (clear of the HUD's right column in a review shot): the nearest
      // spot with two free tiles round it
      Vec2 t(-1, -1);
      for (int r = 3; r <= 12 && t.x < 0; r++)
        for (int dx = -r; dx <= -2 && t.x < 0; dx++)
          for (int dy = -r / 2; dy <= r / 2 && t.x < 0; dy++) {
            const int x = (int)b.x + dx, y = (int)b.y + dy;
            bool ok = b.x >= 0;
            for (int qy = y - 3; qy <= y + 1 && ok; qy++) for (int qx = x - 2; qx <= x + 2 && ok; qx++) ok = freeAt(qx, qy);
            // no house in front of it either (its roof would hide the tower's legs)
            for (int qy = y + 2; qy <= y + 5 && ok; qy++)
              for (int qx = x - 2; qx <= x + 2 && ok; qx++) ok = !m.in(qx, qy) || m.bldgAt[(size_t)qy * m.w + qx] < 0;
            if (ok) { m.setP(x, y, (int)art::Prop::Watchtower + 1); t = Vec2((float)x, (float)y); }
          }
      m.rebuildSolid();
      if (b.x >= 0) {
        const float mx = t.x >= 0 ? (b.x + t.x) * 0.5f : b.x;
        g.pl().p = Vec2(mx * (float)TILE + 8, (std::max(b.y, t.y) + 3) * (float)TILE + 8);
      }
      c.view.snap(g);
      std::printf("script: stage village %s: banner %d,%d tower %d,%d\n", s.name.c_str(), (int)b.x, (int)b.y, (int)t.x, (int)t.y);
      return true;
    }
    if (where == "camp") {
      const std::vector<int> camps = nearest(g, false, SiteType::BanditCamp);
      if (camps.empty()) { c.fail("riseview stage camp: no bandit camp loaded"); return true; }
      const Site& s = g.world.sites[(size_t)camps[0]];
      g.pl().p = Vec2((s.ex - 1) * (float)TILE + 8, (s.ey + 3) * (float)TILE + 8);
      c.view.snap(g);
      return true;
    }
    if (where == "rebuild" || where == "raid") {
      const std::vector<int> near = nearest(g, true);
      const int n = std::max(0, std::atoi(c.arg(3).c_str()));
      if ((size_t)n >= near.size()) { c.fail("riseview stage: no settlement " + std::to_string(n)); return true; }
      const Site& s = g.world.sites[(size_t)near[(size_t)n]];
      g.pl().p = Vec2(s.ex * (float)TILE + 8, (s.ey + 3) * (float)TILE + 8);
      c.view.snap(g);
      return true;
    }
    c.fail("riseview stage village|camp|rebuild|raid");
    return true;
  }
  if (what == "row") {   // pick a row of the open STANDING page (a tap on it, without switching the touch layout off)
    RiseViewOps::standingSelect(c.view, std::atoi(c.arg(2).c_str()));
    return true;
  }
  if (what == "works") {
    // test only: the first holding's works (banner watchtower market shrine granary palisade hall militia walls) and
    // `raising <work>`, as the LORD lane's steward raises them (the view draws them on the village)
    if (g.lordship.holdings.empty()) { c.fail("riseview works: no holding (lord grant first)"); return true; }
    lord::Holding& H = g.lordship.holdings[0];
    auto bitOf = [](const std::string& w) -> uint16_t {
      return w == "banner" ? lord::LW_BANNER : w == "palisade" ? lord::LW_PALISADE : w == "hall" ? lord::LW_HALL : w == "granary" ? lord::LW_GRANARY
           : w == "militia" ? lord::LW_MILITIA : w == "market" ? lord::LW_MARKET : w == "watchtower" ? lord::LW_WATCHTOWER
           : w == "walls" ? lord::LW_WALLS : w == "shrine" ? lord::LW_SHRINE : 0;
    };
    for (size_t i = 2; i < c.a.size(); i++) {
      if (c.a[i] == "raising" && i + 1 < c.a.size()) { H.building = bitOf(c.a[i + 1]); H.buildDone = (uint16_t)(g.day + 6); i++; continue; }
      const uint16_t b = bitOf(c.a[i]);
      if (!b) { c.fail("riseview works: unknown work " + c.a[i]); return true; }
      H.works |= b;
    }
    return true;
  }
  if (what == "spawn") {
    const std::string r = c.arg(2);
    const Role role = r == "steward" ? Role::Steward : r == "warlord" ? Role::Warlord : r == "envoy" ? Role::Envoy : Role::COUNT;
    if (role == Role::COUNT) { c.fail("riseview spawn steward|warlord|envoy [n]"); return true; }
    const int n = std::max(1, std::atoi(c.arg(3).c_str()));
    if (!story::host().spawnHuman) { c.fail("riseview spawn: no spawner"); return true; }
    for (int i = 0; i < n; i++) {
      Spawn sp;
      sp.npc = true;
      sp.role = role;
      sp.site = g.settlementAt(g.pl().p);
      sp.slot = 7000 + i * 13 + (int)role;
      const float row = role == Role::Steward ? -26.0f : role == Role::Warlord ? -54.0f : 28.0f;   // a row each, clear of the hero
      story::host().spawnHuman(g, sp, g.pl().p.x - 30.0f + 22.0f * (float)i, g.pl().p.y + row);
    }
    return true;
  }
  c.fail("riseview fire|stage|spawn");
  return true;
}
EMB_SCRIPT_CMD("riseview", "riseview fire <kind> [f] [words]|stage village|camp|rebuild|raid|spawn steward|warlord|envoy [n]: test only (M12 VIEW)", cmdRiseView);

bool expRiseView(ScriptCtx& c) {
  const View::RiseViewStats s = c.view.riseViewStats();
  const std::string what = c.arg(2);
  const std::string want = c.arg(3);
  const int n = std::atoi(want.c_str());
  auto atLeast = [&](const char* name, int have) {
    if (have < n) c.fail(std::string("expect riseview ") + name + " " + want + ": got " + std::to_string(have));
  };
  if (what == "banners") atLeast("banners", s.banners);
  else if (what == "toasts") atLeast("toasts", s.toasts);
  else if (what == "shown") atLeast("shown", s.shown);
  else if (what == "standing") atLeast("standing", s.standingRows);
  else if (what == "map") atLeast("map", s.mapMarks);
  else if (what == "showing") {
    const int have = s.bannerW > 0 ? 1 : 0;
    if (have != n) c.fail("expect riseview showing " + want + ": got " + std::to_string(have));
  } else if (what == "insignia") {
    if ((s.insignia ? 1 : 0) != n) c.fail("expect riseview insignia " + want + ": got " + std::to_string(s.insignia ? 1 : 0));
  } else if (what == "last") {
    bool ok = false;
    const RiseEv k = kindByName(want, ok);
    if (!ok || k != s.last) c.fail("expect riseview last " + want + ": got " + nameOf(s.last));
  } else if (what == "world") {
    const std::string w = c.arg(3);
    const int m = std::atoi(c.arg(4).c_str());
    const int have = w == "banners" ? s.lordBanners : w == "works" ? s.works : w == "warlord" ? s.warlordPieces : w == "rebuild" ? s.rebuildPieces
                   : w == "scars" ? s.raidScars : -1;
    if (have < 0) c.fail("expect riseview world banners|works|warlord|rebuild|scars <n>");
    else if (have < m) c.fail("expect riseview world " + w + " " + c.arg(4) + ": got " + std::to_string(have));
  } else c.fail("expect riseview banners|toasts|shown|showing|last|insignia|standing|world ...");
  return true;
}
EMB_SCRIPT_CMD("expect:riseview", "expect riseview banners|toasts|shown <n> | showing 0|1 | last <kind> | insignia 0|1 | standing <n> | world <what> <n> (M12 VIEW)", expRiseView);

}  // namespace
