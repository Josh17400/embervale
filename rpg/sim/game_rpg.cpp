// EMBERVALE RPG: interaction (the action button, beds, the inn's room), the blessing time and resting. The rest of
// the RPG systems moved out in SOLID 1.1: game_items, game_quests, game_dialogue, game_shop, game_save.cpp.
// Public: Game::foeInReach, Game::interactTarget, Game::nearDoorOrExit, Game::bedIsYours, Game::interactProp,
//   Game::interact, Game::npcKeyOf, Game::npcKey, Game::blessingSecs, Game::rest
#include "rpg/sim/game_rpg_detail.h"
#include "rpg/sim/life_hooks.h"
#include "rpg/sim/game_internal.h"

// ------------------------------------------------------------------ interaction
// (fixer M4 r3) a fight comes first: with an awake foe (a raiding wolf, a soldier of the enemy) in striking reach the
// button is ATTACK, never TALK to the guard who came running to stand beside the hero, nor a use of a prop (phone
// players have no other way to swing)
bool Game::foeInReach() const {
  const Actor& p = pl();
  for (size_t i = 1; i < actors.size(); i++) {
    const Actor& a = actors[i];
    if (a.st == AState::Dead || a.asleep || a.player) continue;
    // (a beast that is fighting: a raider at a villager, a guard's foe; an idle cave dweller beside a captive does not
    // turn the captive's TALK into a swing)
    const bool foe = (a.hostile && a.aggro) || (a.aggro && a.target == p.id) || (a.npc && warFoes(*this, a, p));
    if (foe && len2(a.p - p.p) < 34.0f * 34.0f) return true;
  }
  return false;
}

int Game::interactTarget() const {
  const Actor& p = pl();
  if (foeInReach()) return -1;
  // (fixer M10 r1) aboard, the deck's own verb (DISEMBARK, MOOR, FISH) outranks the crowd on the pier: no TALK flicker
  if (sea.atSea() && !SeaOps::interactLabel(*this).empty()) return -1;
  int best = -1; float bd = 24 * 24;
  // (M7) in the player's own yard, a crop / coop / stable in front wins over a passer-by (the farm's own actors stay)
  const bool yard = !inside && !home.plots.empty() && home::yardUseAhead(*this);
  for (size_t i = 1; i < actors.size(); i++) {
    const Actor& a = actors[i];
    if (!a.npc || a.st == AState::Dead) continue;
    if (yard && !(a.lifeBits & LB_HOMESTEAD)) continue;
    // (M4 integration) a soldier or guard who is the player's foe, or is fighting the player, is not talked to: the
    // tap and E attack them instead of offering TALK mid-fight
    if ((a.aggro && a.target == p.id) || warFoes(*this, a, p)) continue;
    float d = len2(a.p - p.p);
    // shopkeepers behind counters can be reached across them
    float reach = (a.role == Role::Merchant || a.role == Role::Innkeeper || a.role == Role::Smith) ? 40.0f : 24.0f;
    if (d >= reach * reach) continue;
    // (stall facings) a keeper at their counter is talked to across it, from the customers' side (not through its back)
    if (a.stallKeeper && len2(a.p - a.home) < 4.0f) {
      const Vec2 fv = faceVec(a.face);
      if ((p.p.x - a.p.x) * fv.x + (p.p.y - a.p.y) * fv.y < 2.0f) continue;
    }
    // someone with a reward waiting or work to offer wins over a bystander about as close (two villagers side by
    // side: the prompt and the tap go to the one with something to say)
    if (rewardWaiting(a) || hasOffer(a)) d *= 0.45f;
    // (M7c merge, m7_homestead_ride) the player's own horse waiting beside them, turned to, takes the prompt from a
    // passer-by about as near: in the M7c villages a street's foot traffic brushes past the spot where the rider got
    // off, and E: TALK <stranger> stole the RIDE the player had turned round for
    if (a.slot == HOME_SLOT_HORSE && a.critter && (a.lifeBits & LB_HOMESTEAD) && home.riding < 0) {
      const Vec2 to = a.p - p.p;
      if (to.x * p.aim.x + to.y * p.aim.y > 0.5f * std::sqrt(std::max(1.0f, d))) d *= 0.45f;
    }
    // (fixer M8 r2, review: "E: TALK <companion> follows them everywhere") a companion at heel is always the nearest
    // person: they take the prompt only when the player turns to them, and anyone else about as near comes first
    if (a.comp && a.comp <= party.members.size() && party.members[(size_t)a.comp - 1].state == party::State::With) {
      const Vec2 to = a.p - p.p;
      if (to.x * p.aim.x + to.y * p.aim.y < 0.35f * std::sqrt(std::max(1.0f, d))) continue;
      d = std::min(d * 2.5f, 24.0f * 24.0f - 1.0f);
    }
    if (d < bd + (reach * reach - 24 * 24)) { bd = d; best = a.id; }
  }
  return best;
}

bool Game::nearDoorOrExit() const { return false; }

bool Game::bedIsYours(int tx, int ty) const {
  if (homeBed(tx, ty)) return true;   // M7: a bed in the player's own house
  if (!inside || subBldg < 0) return true;
  const Bldg& B = world.over.bldgs[subBldg];
  if (B.type != art::Building::Inn || B.genVer < WORLDGEN_V7) return true;
  return lodgingActive() && lodging.bldg == subBldg && lodging.floor == subFloor && lodging.room == sub.roomIndexAt(tx, ty);
}

int Game::interactProp(int& otx, int& oty) const {
  const Actor& p = pl();
  const Map& m = map();
  Vec2 probe = p.p + p.aim * 12.0f + Vec2(0, -4);
  for (int r = 0; r <= 1; r++)
    for (int oy = -r; oy <= r; oy++)
      for (int ox = -r; ox <= r; ox++) {
        int tx = (int)std::floor(probe.x / TILE) + ox, ty = (int)std::floor(probe.y / TILE) + oy;
        int pr = m.propAt(tx, ty);
        if (!pr) continue;
        Prop prop = (Prop)(pr - 1);
        bool usable = prop == Prop::Chest || prop == Prop::Shrine || prop == Prop::Altar || prop == Prop::BerryBush ||
                      (prop == Prop::SummitShrine && !inside) ||   // M11: the shrine at the top of a peak's climb
                      ((prop == Prop::StandingStone || prop == Prop::GraveCairn) && !inside) ||   // M2 wayside (wayside.cpp)
                      (prop == Prop::Signpost && !inside && world.siteAt(tx, ty, 3) >= 0) ||
                      ((prop == Prop::Hammock || prop == Prop::SleepingMat) && inside && subBldg >= 0) ||   // M3: cultures that sleep so
                      (prop == Prop::Bedroll && !inside) ||   // M5: a wayfarer's bedroll (Life: rested, the night passes)
                      (art::isLoreProp(prop) && (inside ? subSite >= 0 : (prop == Prop::NoticeBoard || prop == Prop::ToppledStatue))) ||   // M4 (story_game.cpp)
                      homePropUsable(prop, tx, ty) ||   // M7: cooking places, the player's own furniture (home_game.cpp)
                      (prop == Prop::Bed && inside && subBldg >= 0 &&
                       (world.over.bldgs[subBldg].type != art::Building::Inn || world.over.bldgs[subBldg].genVer >= WORLDGEN_V7));
        if (!usable) continue;
        otx = tx; oty = ty;
        return pr;
      }
  return 0;
}

void Game::interact() {
  if (PartyOps::interact(*this)) return;   // M8: bind a downed companion's wounds, a grave of the fallen (party_game.cpp)
  int t = interactTarget();
  if (t >= 0) { talkTo(actors[findActor(t)]); return; }
  if (homeInteract()) return;   // M7: the player's own yard (till, plant, water, harvest, collect, mount; home_game.cpp)
  if (SeaOps::interact(*this)) return;   // M10: board / disembark, cast a line, a pier's use (sea_game.cpp)
  // chest / shrine / bed / bush in front of the player
  const Actor& p = pl();
  int tx = 0, ty = 0;
  int pr = interactProp(tx, ty);
  if (!pr) return;
  Prop prop = (Prop)(pr - 1);
  if (homeUseProp(prop, tx, ty)) return;   // M7: the player's own stores, beds and cooking places (home_game.cpp)
  if (prop == Prop::Chest) { openChest(tx, ty); return; }
  if (LifeHooks::useProp(*this, prop, tx, ty)) return;   // M5: beds and bedrolls (Rested), the townsfolk's furniture (life_game.cpp)
  // M4: a story may ask for a wayside prop (standing stones, a cairn) before its own use runs
  if (prop == Prop::StandingStone || prop == Prop::GraveCairn) story.onUseProp(*this, (int)prop, tx, ty);
  if (useWaysideProp(prop, tx, ty)) return;
  if (storyUseProp(prop, tx, ty)) return;   // M4: notice boards, inscriptions, graves, journals (story_game.cpp)
  if (prop == Prop::SummitShrine) {
    // M11: the summit shrine pays the climb back: its god's blessing for twice as long, full health and a
    // "summit" word so the player knows this one is stronger than a wayside shrine's
    int si = world.siteAt(tx, ty, 2);
    blessName = "BLESSING OF THE SUMMIT";
    if (si >= 0 && world.sites[si].type == SiteType::Shrine && world.sites[si].name.rfind("SHRINE OF ", 0) == 0)
      blessName = "BLESSING OF " + world.sites[si].name.substr(10);
    deed(party::Deed::Prayed, si >= 0 ? world.sites[(size_t)si].id : 0);
    blessT = blessingSecs() * 2.0f;
    pl().hp = pl().maxHp;
    recalcPlayer();
    say(blessName + " (SUMMIT): +ARMOR, FASTER HEALING, TWICE AS LONG");
    emit(Ev::Heal, p.p);
    sfx((int)Sfx::Heal, p.p, 1.0f);
    return;
  }
  if (prop == Prop::Shrine || prop == Prop::Altar) {
    static const char* bless[] = {"BLESSING OF SOLMIR", "BLESSING OF VEYNA", "BLESSING OF HALDRUN", "BLESSING OF ORISSA"};
    blessName = bless[hash2(tx, ty) % 4];
    int si = inside ? -1 : world.siteAt(tx, ty, 1);   // a wayside shrine blesses in its own god's name
    if (si >= 0 && world.sites[si].type == SiteType::Shrine && world.sites[si].name.rfind("SHRINE OF ", 0) == 0)
      blessName = "BLESSING OF " + world.sites[si].name.substr(10);
    deed(party::Deed::Prayed, si >= 0 ? world.sites[(size_t)si].id : 0);   // M8: the companions see it
    blessT = blessingSecs();
    pl().hp = pl().maxHp;
    recalcPlayer();
    say(blessName + (background == Background::Novice ? ": +ARMOR, FASTER HEALING (LONGER FOR A NOVICE)" : ": +ARMOR, FASTER HEALING"));
    emit(Ev::Heal, p.p);
    sfx((int)Sfx::Heal, p.p, 0.8f);
    return;
  }
  if (prop == Prop::BerryBush) {
    Item a = makeFood(2);
    addItem(a);
    map().setProp(tx, ty, Prop::Bush);
    return;
  }
  if (prop == Prop::Signpost) {
    int si = world.siteAt(tx, ty, 3);
    if (si >= 0) say("WELCOME TO " + world.sites[si].name);
    return;
  }
  if (prop == Prop::Hammock || prop == Prop::SleepingMat) {   // M3: a home's hammock or sleeping mat (inns keep beds)
    rest(hoursToMorning(hour));
    say("YOU SLEEP SOUNDLY.");
    return;
  }
  if (prop == Prop::Bed) {
    // M0b: an inn's beds upstairs are let room by room; only the one you rented is yours to sleep in
    const Bldg& B = world.over.bldgs[subBldg];
    if (B.type == art::Building::Inn && B.genVer >= WORLDGEN_V7) {
      int ri = sub.roomIndexAt(tx, ty);
      RoomKind k = ri >= 0 && ri < (int)sub.rooms.size() ? sub.rooms[(size_t)ri].kind : RoomKind::Common;
      bool mine = lodgingActive() && lodging.bldg == subBldg && lodging.floor == subFloor && lodging.room == ri;
      if (mine) {
        // a lie-in on the morning the room is due back ends before noon (the innkeeper would knock)
        int h = lodgingSleepHours(day, hour, lodging.untilDay);
        if (h <= 0) { say(kRoomDue); return; }
        rest(h);
        say("YOU SLEEP SOUNDLY IN YOUR ROOM.");
        return;
      }
      if (k == RoomKind::OwnerRoom) { say("THAT IS THE INNKEEPER'S OWN BED."); return; }
      say(lodgingActive() && lodging.bldg == subBldg ? "THIS ROOM IS TAKEN. YOURS IS ANOTHER." : "THIS ROOM IS TAKEN. RENT ONE FROM THE INNKEEPER.");
      return;
    }
    rest(hoursToMorning(hour));
    say("YOU SLEEP SOUNDLY.");
    return;
  }
}

// M1: an NPC's identity from stable ids (site, home building, slot), so shop stock, greetings and finished-quest
// counts follow the same person across sessions and window moves
uint64_t Game::npcKeyOf(int site, int bldg, int slot) const {
  // the classic island's handles are its indices, so its NPCs keep the M0 key (and with it their offers and greetings)
  if (!world.endless) return ((uint64_t)(site + 1) << 24) ^ ((uint64_t)(bldg + 1) << 12) ^ (uint64_t)(slot & 0xFFF);
  uint64_t s = site >= 0 && site < (int)world.sites.size() ? world.sites[(size_t)site].id : 0;
  uint64_t b = bldg >= 0 && bldg < (int)world.over.bldgs.size() ? world.over.bldgs[(size_t)bldg].id : 0;
  return ew::mix64(s * 0x9E3779B97F4A7C15ull ^ ew::mix64(b + 0x51ED27ull) ^ (uint64_t)(uint32_t)(slot & 0xFFF));
}
uint64_t Game::npcKey(const Actor& a) const { return npcKeyOf(a.site, a.bldg, a.slot); }

float Game::blessingSecs() const { return background == Background::Novice ? 900.0f : 600.0f; }   // novice: +50 %

void Game::rest(int hours) {
  if (homeRiding()) homeDismount(home::Dismount::Sleep);   // M7
  MagicOps::rested(*this, hours);   // M9: the prepared spells may be swapped at rest
  hour += hours;
  while (hour >= 24) { hour -= 24; day++; }
  Actor& p = pl();
  p.hp = p.maxHp; mp = maxMp; stamina = maxSt;
  sleepFade = 1.5f;
  LifeHooks::slept(*this, hours, true);   // M5 (15.2): every rest is in a bed, a hammock, a mat or a bedroll: Rested
  PartyOps::rested(*this, hours);       // M8: the companions rest too (party_game.cpp)
  sfx((int)Sfx::Heal, p.p, 0.7f);
}

// travel and death: rpg/sim/travel.cpp (M2)

