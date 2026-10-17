// M4 "Banners": the realm in the game (Game::realmStep, Game::realmSync; rpg/sim/realm.h). REALM lane.
// PHASE A (lead), PHASE B (REALM lane): the realm follows the player (focus: time-sliced instantiation), ticks at each
// day change, learns every settlement the window loads (with its true specialisation for the food economy), gives a settlement to its owner when the realm moved it (banners follow: World::setSiteOwner), adds the
// realm-made kingdoms the generator never planned (rebels, warlords) to World::kingdoms, and tells the view when the
// player crosses into another kingdom's land (Ev::Border).
#include <algorithm>
#include <cmath>
#include "rpg/sim/game.h"
#include "rpg/sim/game_internal.h"
#include "rpg/world/source.h"

void Game::realmSync() {
  if (!world.endless || !world.src) return;
  for (size_t h = 0; h < world.sites.size(); h++) {
    const Site& s = world.sites[h];
    if (!s.settlement()) continue;
    const realm::SettlementState* st = realm.settlement(s.id);
    if (!st) continue;
    const ew::Gid cur = s.kingdom >= 0 ? world.kingdoms[(size_t)s.kingdom].id : 0;
    if (st->owner == cur) continue;
    if (st->owner && !world.kingdomById.count(st->owner)) {
      // a realm-made kingdom (no generator plan): its record comes from the realm
      if (const realm::KingdomState* K = realm.kingdom(st->owner)) {
        Kingdom k;
        k.id = K->id; k.name = K->name; k.color = K->color; k.color2 = K->color2; k.emblem = K->emblem;
        k.capitalId = K->capital; k.culture = K->culture;
        if (world.src->kingdom(K->id) == nullptr) world.addKingdomRecord(k);
      }
    }
    world.setSiteOwner((int)h, st->owner);
  }
  realmSeen_ = realm.eventSerial();
}

bool Game::realmNoteSites() {
  if (!world.endless || !world.src) return false;
  bool fresh = false;
  for (; realmSites_ < world.sites.size(); realmSites_++) {
    const Site& s = world.sites[realmSites_];
    if (s.type == SiteType::BanditCamp) {   // (M12) a warlord's seed: the realm watches every camp the window loads
      const int32_t cx = world.ox + s.ex, cy = world.oy + s.ey;
      realm.noteCamp(s.id, cx, cy, world.src->kingdomAt(cx, cy), day);
      if (s.cleared) realm.campCleared(s.id, day);
    }
    if (!s.settlement()) continue;
    const ew::Gid home = s.homeKingdom >= 0 ? world.kingdoms[(size_t)s.homeKingdom].id : 0;
    realm.noteSite(s.id, home, (uint8_t)s.type, world.ox + s.ex, world.oy + s.ey);
    realm.noteEconomy(s.id, s.special);   // 15.11: its true trade (the lattice only estimated it)
    if (const realm::SettlementState* st = realm.settlement(s.id)) if (st->owner != home) fresh = true;
  }
  return fresh;
}

void Game::realmStep(float dt) {
  (void)dt;
  if (!world.endless || !world.src || inside) return;
  ew::EndlessSource& src = *world.src;
  const int32_t gx = world.ox + (int32_t)std::floor(pl().p.x / TILE), gy = world.oy + (int32_t)std::floor(pl().p.y / TILE);
  realm.focus(src, gx, gy, day);
  // every settlement the window loaded is known to the realm (its genesis owner, type and heart)
  const bool fresh = realmNoteSites();
  if (realmDay_ != day) {
    realm.advanceTo(src, day);
    realmDay_ = day;
  }
  if (fresh || realm.eventSerial() != realmSeen_) realmSync();
  // (M12) the world's rise and fall near the player: a warlord rising, a warband or a world boss raiding (a herald's
  // banner; Event::id = the low bits of the realm it concerns). Only news the player could hear: within 160 tiles
  // (a hidden land is far across the sea), and only events made since the last look (never the history on a load).
  if (riseSeen_ == 0 || riseSeen_ > realm.eventSerial()) riseSeen_ = realm.eventSerial();
  else if (riseSeen_ != realm.eventSerial()) {
    for (const realm::WorldEvent& e : realm.events()) {
      if (e.id < riseSeen_) continue;
      RiseEv k = RiseEv::None;
      float f = 1;
      ew::Gid who = e.a;
      std::string line;
      const realm::KingdomState* A = e.a ? realm.kingdom(e.a) : nullptr;
      if (e.type == realm::EvType::WarlordRose) { k = RiseEv::WarlordNear; f = 1; line = A ? A->name : "A WARLORD"; }
      else if (e.type == realm::EvType::WarbandRaid) { k = RiseEv::WarlordNear; f = 2; who = e.b; line = "SMOKE ON THE HORIZON"; }
      else if (e.type == realm::EvType::BeastRaid) { k = RiseEv::BossRaid; line = "A TOWN NEARBY BURNS"; }
      if (k == RiseEv::None) continue;
      const int64_t dx = (int64_t)e.gx - gx, dy = (int64_t)e.gy - gy;
      if (dx * dx + dy * dy > 160ll * 160ll) continue;
      if (e.site) {   // the place by name when the window holds it (the banner's top line already names the deed)
        const int h = world.siteHandle(e.site);
        if (h >= 0) {
          const std::string& nm = world.sites[(size_t)h].name;
          line = k == RiseEv::WarlordNear && f == 1 ? line + " TAKES " + nm : k == RiseEv::WarlordNear ? "SMOKE OVER " + nm : nm + " BURNS";
        }
      }
      Event ev{Ev::Rise, pl().p, (int)k, f, line};
      ev.id = (uint32_t)who;
      events.push_back(ev);
    }
    riseSeen_ = realm.eventSerial();
  }
  // crossing into another kingdom's land (VISION_PLAN M4: "a herald's banner at every border")
  const ew::Gid land = realm.landOwner(src.kingdomAt(gx, gy), gx, gy);
  if (!realmLandKnown_) { realmLand_ = land; realmLandKnown_ = true; return; }
  if (land != realmLand_) {
    realmLand_ = land;
    std::string name = "THE WILDLANDS";
    uint32_t col = rgba(200, 180, 140);
    if (land) {
      if (const realm::KingdomState* K = realm.kingdom(land)) { name = K->name; col = K->color; }
      else if (const ew::KingdomPlan* kp = src.kingdom(land)) { name = kp->name; col = kp->color; }
    }
    emit(Ev::Border, pl().p, (int)col, land ? 1.0f : 0.0f, name);
  }
}
