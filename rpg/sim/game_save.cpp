// EMBERVALE RPG: the save file: SAVE_VER, serialize/deserialize byte for byte (SOLID 1.1). The save blocks, in
// write order with the SAVE_VER that added each, are listed in the comment block below.
// Public: Game::currentSaveVersion, Game::saveVersion, Game::saveFromOlderGenerator, Game::serialize,
//   Game::deserialize
#include "rpg/sim/game_rpg_detail.h"

// ------------------------------------------------------------------ save / load
// SAVE_VER 12 (M6: items gain item level, material, alloy, maker culture, form, seed, affixes, unique power and flags
// (items.cpp writeItem); the craft block appended after the life block). SAVE_VER 11 (M5: the life block appended after the story block). SAVE_VER 10 (M4: the realm and story blocks appended after marks). SAVE_VER 9 (M3c: the layout of 8, bumped with the Wildlands world generation). SAVE_VER 8 (M3b: the layout of 7, bumped with the builder's world generation). SAVE_VER 7 (M3: the appearance block gains people, homeland and personal heraldry; nothing else moved).
// SAVE_VER 6 (M2). Owner, 2026-10-04: old saves are not a concern, so only this version loads; an older file is refused
// and the title offers a new game ("this save is from an older version"). The layout is frozen for M2 after phase A
// (the lanes fill the new fields, they do not move them); any later change bumps SAVE_VER and regenerates
// tests/fixtures/save_v6.bin with `save_test --make-fixture`.
//
// Everything that points into the world is saved by stable id (Gid), never by handle (a vector index), because an
// endless world loads its sites and buildings in whatever order the player walked: sites by Site::id, buildings by
// (Bldg::id, owner Site::id), map keys (looted chests, killed spawns) as (kind, id, floor). Overworld chests of an
// endless world are keyed by global tile (Game::lootKey). M2 retired the classic island: worldKind is always 1.
//
//   header   magic 'EMBV', ver 7, worldKind u8 (1 endless; anything else is refused), genVer u32 (ew::ENDLESS_GEN_VER),
//            seed u64, time f32, hour f32, day i32
//   window   origin ox i32, oy i32 (positions below are window-local pixels)
//   where    inside u8, subSite (site ref), subBldg (bldg ref), subFloor i32, x f32, y f32
//   player   hp, mp, stamina f32; level, xp, gold, perk points i32; baseHp, maxMp, maxSt f32; inventory; 11 equip
//            slots i32; spellsKnown u8, spell u8; kills, dungeons i32; blessT f32, blessName str; lastTown (site ref)
//   quests   count, then id, type, state, title, desc, giverName, giverSite (site ref), giverBldg (bldg ref),
//            giverSlot i32, target (site ref), mon u8, need, have, gold, xp, stage i32,
//            (v6) targetId u64, hasPos u8, tgx i32, tgy i32, giverId u64, subject str, destBldg (bldg ref),
//            deadlineDay i32, flags u32; then nextQuestId, trackedQuest i32
//   npcs     count, then npcKey u64 (already id-based) + done i32
//   looted   count, then (map ref, tile u32) or an endless overworld key (map kind 0xE + the raw key)
//   killed   count, then (map ref, n, n values: site ref + slot for overworld spawns (slot < 4096), den id + day for
//            dens, slot otherwise)
//   sites    count, then site id u64 + flags u8 (1 discovered, 2 cleared, 4 rumoured)
//   char     background u8, storyFlags u32, appearance (u16 length + fields; v7 appends people u8, homeland u64,
//            heraldry: field u32, field2 u32, charge u32, division u8, chargeKind u8, emblem u8, glyphSeed u32, shape u8)
//   lodging  bldg ref, floor i32, room i32, untilDay i32
//   explored count, then rx i32, ry i32, 128 bytes (the fog-of-war bits of one region)
//   marks    (v6) count, then key u64 + value i32, in key order
//   realm    (v10) u32 byte length + the realm block (realm::Realm::serialize: its own version byte first)
//   story    (v10) u32 byte length + the story block (story::Engine::serialize: its own version byte first; v13: block v3)
//   life     (v11) u32 byte length + the life block (life::Life::serialize: its own version byte first)
//   craft    (v12) u32 byte length + the craft block (craft::Knowledge::serialize: its own version byte first)
//   home     (v14) u32 byte length + the home block (home::Homes::serialize: its own version byte first)
//   talk     (v15) u32 byte length + the talk block (speech::Memory::serialize: its own version byte first)
//   party    (v16) u32 byte length + the party block (party::Party::serialize: its own version byte first)
//   skills   (v20) u32 byte length + the skills block (skills::Skills::serialize: its own version byte first)
//   magic    (v20) u32 byte length + the magic block (magic::Book::serialize: its own version byte first)
//   sea      (v21) u32 byte length + the sea block (sea::Sea::serialize: its own version byte first)
//   service  (v22) u32 byte length + the service block (service::Service::serialize: its own version byte first)
//   lordship (v22) u32 byte length + the lordship block (lord::Lordship::serialize: its own version byte first)
//   jobs     (v23) u32 byte length + the jobs block (jobs::Book::serialize: its own version byte first: the weekly
//                  job pools, the journal's leads)
// refs: site ref = u64 id (0 none); bldg ref = u64 id + u64 owner site id (0 none); map ref = u8 kind (0 overworld,
// 1 cave/ruin, 2 building, 3 dens) + u64 id + u64 owner + u8 floor.
static constexpr uint32_t SAVE_MAGIC = 0x454D4256;   // EMBV
static constexpr uint32_t SAVE_VER = 23;  // 23: P1 Fresh Roads (the jobs block after the lordship block: the weekly job
                                          //    pools and the journal's leads; QType::Errand; ENDLESS_GEN_VER 27, saga
                                          //    SAGA_GEN_VER 2). Old saves are refused ("start a new adventure").
                                          // 22: M12 Rise and Fall (the service and lordship blocks after the sea block;
                                          //    the realm block v4: camps, lairs, realm origins, vassals, the player's
                                          //    realm; ENDLESS_GEN_VER unchanged). Old saves are refused.
                                          // 21: M10 Beyond the Mist (the sea block after the magic block: boats, the one
                                          //    aboard, ports known, the lands' chart chains and contact, catches;
                                          //    ENDLESS_GEN_VER 23). Old saves are refused ("start a new adventure").
                                          // 20: M9 Arcana & Mastery (the skills and magic blocks after the party block;
                                          //    ENDLESS_GEN_VER unchanged). A SAVE_VER 19 save still loads (owner
                                          //    15.29.5: "give old saves default skills based on their level"): the
                                          //    same layout without the two blocks; skills::defaultsForLevel fills them;
                                          // 19: M8b Treasure (items carry 4 affix slots, the Mythic rarity and
                                          //    powers; ENDLESS_GEN_VER unchanged);
                                          // 18: fixer M8 r3 (party block v4: the fallen's gender);
                                          // 17: fixer M8 r1 (party block v3: a companion's look fixed when they join);
                                          // 16: M8 Fellowship (the party block after the talk block: the companions,
                                          //    their approval, bond, gear and memories, the fallen and their graves;
                                          //    ENDLESS_GEN_VER 21);
                                          // 15: M7b Cities & Voice (the talk block after the home block: the people
                                          //    met, introductions, the lines said lately; ENDLESS_GEN_VER 17: the
                                          //    cities' plan);
                                          // 14: M7 Home (the home block after the craft block: the player's houses, lots,
                                          //    yards, crops, animals, stores, discovered styles, the horse ridden;
                                          //    ENDLESS_GEN_VER 16);
                                          // 13: M6b Sagas (the story block's own version 3: the generated stories'
                                          //    repetition guard; running sagas are saved by their spec id);
                                          // 12: M6 Steel (the item layout grows: ilvl, material, alloy, culture, form,
                                          //    seed, 3 affixes, unique power, flags; the craft block after the life
                                          //    block: smithing skill, secrets, trust; ENDLESS_GEN_VER 15);
                                          // 11: M5 Hearth and Hall (the life block after the story block: the census
                                          //    deltas, settlement stock and moods, the player's buffs; ENDLESS_GEN_VER 14);
                                          // 10: M4 Banners (the realm and story blocks after marks; ENDLESS_GEN_VER 13);
                                          // 9: M3c Wildlands (the layout of 8; the world's biomes, flora and wildlife
                                          //    are new, ENDLESS_GEN_VER 12, so older adventures start anew);
                                          // 8: M3b Builders & Societies (the world is built by the builder: a new
                                          //    generation, so older adventures start anew); 7: M3 (appearance: people,
                                          //    homeland, personal heraldry)

int Game::currentSaveVersion() { return (int)SAVE_VER; }
// the oldest SAVE_VER that still loads (M10: only the current one; M9's SAVE_VER 19 exception ended with the generator
// change, owner: old saves are not a concern)
static constexpr uint32_t SAVE_VER_MIN = 23;   // (P1) only the current one
int Game::saveVersion(const std::vector<uint8_t>& in) {
  BinR r(in);
  if (r.u32() != SAVE_MAGIC || r.bad) return 0;
  uint32_t v = r.u32();
  return r.bad ? 0 : (int)v;
}
bool Game::saveFromOlderGenerator(const std::vector<uint8_t>& in) {
  BinR r(in);
  if (r.u32() != SAVE_MAGIC || r.bad) return false;
  { const uint32_t v = r.u32(); if (v < SAVE_VER_MIN || v > SAVE_VER || r.bad) return false; }
  const bool endless = r.u8() == 1;
  const int genVer = (int)r.u32();
  if (r.bad) return false;
  return !endless || genVer < ew::ENDLESS_GEN_VER;
}

namespace {
// The appearance block: u16 byte length, then the fields in this order.
void writeAppearance(BinW& w, const Appearance& a) {
  std::vector<uint8_t> blk;
  BinW b(blk);
  b.str(a.name); b.u8(a.female ? 1 : 0); b.u8(a.build); b.u8(a.skinTone); b.u32(a.skin); b.u8(a.hair); b.u32(a.hairColor);
  b.u8(a.beard ? 1 : 0); b.u32(a.eyeColor); b.u32(a.topColor); b.u32(a.bottomColor); b.u8(a.created ? 1 : 0);
  // v7 (M3): people, homeland, personal heraldry
  b.u8(a.people); b.u64(a.homeland);
  b.u32(a.heraldry.field); b.u32(a.heraldry.field2); b.u32(a.heraldry.charge); b.u8(a.heraldry.division);
  b.u8(a.heraldry.chargeKind); b.u8(a.heraldry.emblem); b.u32(a.heraldry.glyphSeed); b.u8(a.heraldry.shape);
  w.u16((uint16_t)blk.size());
  for (uint8_t c : blk) w.u8(c);
}
bool readAppearance(BinR& r, Appearance& a) {
  int n = r.u16();
  std::vector<uint8_t> blk;
  for (int i = 0; i < n && !r.bad; i++) blk.push_back(r.u8());
  if (r.bad) return false;
  BinR b(blk);
  Appearance d;
  auto more = [&] { return b.p < blk.size() && !b.bad; };
  if (more()) d.name = b.str();
  if (more()) d.female = b.u8() != 0;
  if (more()) d.build = b.u8();
  if (more()) d.skinTone = b.u8();
  if (more()) d.skin = b.u32();
  if (more()) d.hair = b.u8();
  if (more()) d.hairColor = b.u32();
  if (more()) d.beard = b.u8() != 0;
  if (more()) d.eyeColor = b.u32();
  if (more()) d.topColor = b.u32();
  if (more()) d.bottomColor = b.u32();
  if (more()) d.created = b.u8() != 0;
  if (more()) {   // v7 (M3)
    d.people = b.u8(); d.homeland = b.u64();
    d.heraldry.field = b.u32(); d.heraldry.field2 = b.u32(); d.heraldry.charge = b.u32(); d.heraldry.division = b.u8();
    d.heraldry.chargeKind = b.u8(); d.heraldry.emblem = b.u8(); d.heraldry.glyphSeed = b.u32(); d.heraldry.shape = b.u8();
  }
  if (b.bad) return false;
  a = d;
  return true;
}

// handle <-> stable id
struct Refs {
  World& w;
  ew::Gid site(int h) const { return h >= 0 && h < (int)w.sites.size() ? w.sites[(size_t)h].id : 0; }
  // an id that is not a site's (a damaged or foreign save) loads as "none", never as some other record
  int siteFrom(ew::Gid id) const {
    if (!id) return -1;
    return w.ensureSite(id);
  }
  void bldg(BinW& o, int h) const {
    if (h < 0 || h >= (int)w.over.bldgs.size()) { o.u64(0); o.u64(0); return; }
    const Bldg& b = w.over.bldgs[(size_t)h];
    o.u64(b.id); o.u64(site(b.site));
  }
  int bldgFrom(BinR& r) const {
    ew::Gid id = r.u64(), owner = r.u64();
    if (!id || r.bad) return -1;
    int sh = owner ? siteFrom(owner) : -1;
    if (sh >= 0) w.ensureSiteRecords(sh);
    return w.bldgHandle(id);
  }
  // Game::mapKey(): 0 overworld, 1 + site (cave/ruin), 100000 + bldg (ground floor), 10000000 + bldg * 16 + floor
  // (upper floors); -1 the dens' record in killedSlots
  void map(BinW& o, int key) const {
    if (key == 0) { o.u8(0); o.u64(0); o.u64(0); o.u8(0); return; }
    if (key == -1) { o.u8(3); o.u64(0); o.u64(0); o.u8(0); return; }
    if (key < 100000) { o.u8(1); o.u64(site(key - 1)); o.u64(0); o.u8(0); return; }
    int b = key < 10000000 ? key - 100000 : (key - 10000000) / 16, f = key < 10000000 ? 0 : (key - 10000000) % 16;
    o.u8(2); bldg(o, b); o.u8((uint8_t)f);
  }
  bool mapFrom(BinR& r, int& key) const {
    uint8_t kind = r.u8();
    if (kind == 2) {
      int b = bldgFrom(r);
      int f = r.u8();
      if (b < 0) return false;
      key = f > 0 ? 10000000 + b * 16 + f : 100000 + b;
      return true;
    }
    ew::Gid id = r.u64(); r.u64(); r.u8();
    if (kind == 0) { key = 0; return true; }
    if (kind == 3) { key = -1; return true; }
    int s = siteFrom(id);
    if (s < 0) return false;
    key = 1 + s;
    return true;
  }
};
}  // namespace

void Game::serialize(std::vector<uint8_t>& out) const {
  out.clear();
  BinW w(out);
  Refs R{const_cast<World&>(world)};
  w.u32(SAVE_MAGIC); w.u32(SAVE_VER);
  w.u8(1);
  w.u32((uint32_t)ew::ENDLESS_GEN_VER);
  w.u64(seed); w.f32(time); w.f32(hour); w.i32(day);
  w.i32(world.ox); w.i32(world.oy);
  const Actor& p = pl();
  w.u8(inside ? 1 : 0);
  w.u64(R.site(inside ? subSite : -1));
  R.bldg(w, inside ? subBldg : -1);
  w.i32(inside && subBldg >= 0 ? subFloor : 0);
  w.f32(p.p.x); w.f32(p.p.y);
  w.f32(p.hp); w.f32(mp); w.f32(stamina);
  w.i32(plLevel); w.i32(plXp); w.i32(gold); w.i32(perkPts);
  w.f32(baseHp); w.f32(maxMp); w.f32(maxSt);
  w.u32((uint32_t)inv.size());
  for (auto& it : inv) writeItem(w, it);
  for (int e : {eqWeapon, eqBow, eqStaff, eqArmor, eqHelmet, eqShield, eqRing, eqAmulet, eqGloves, eqBoots, eqCloak}) w.i32(e);
  // (M9) the classic spell fields as a load will see them (magic::syncLegacy on copies: the book written below agrees)
  magic::Book bookOut = arcana.book;
  uint8_t knownOut = spellsKnown;
  int spellOut = (int)spell;
  int8_t mirrorOut = arcana.live.legacyMirror;
  magic::syncLegacy(bookOut, knownOut, spellOut, mirrorOut);
  w.u8(knownOut); w.u8((uint8_t)spellOut);
  w.i32(kills); w.i32(dungeonsCleared); w.f32(blessT); w.str(blessName);
  w.u64(R.site(lastTown));
  w.u32((uint32_t)quests.size());
  for (auto& q : quests) {
    w.i32(q.id); w.u8((uint8_t)q.type); w.u8((uint8_t)q.state); w.str(q.title); w.str(q.desc); w.str(q.giverName);
    w.u64(R.site(q.giverSite)); R.bldg(w, q.giverBldg); w.i32(q.giverSlot); w.u64(R.site(q.target)); w.u8((uint8_t)q.mon);
    w.i32(q.need); w.i32(q.have); w.i32(q.gold); w.i32(q.xp); w.i32(q.stage);
    w.u64(q.targetId); w.u8(q.hasPos ? 1 : 0); w.i32(q.tgx); w.i32(q.tgy); w.u64(q.giverId); w.str(q.subject);
    R.bldg(w, q.destBldg); w.i32(q.deadlineDay); w.u32(q.flags);
  }
  w.i32(nextQuestId); w.i32(trackedQuest);
  w.u32((uint32_t)npcQuestsDone.size());
  for (auto& kv : npcQuestsDone) { w.u64(kv.first); w.i32(kv.second); }
  // (records keyed by ids are written in the order of their encoded bytes: an endless session's handles depend on the
  // order the player met things, so handle order would make two saves of the same state differ)
  auto writeSorted = [&](std::vector<std::vector<uint8_t>>& recs) {
    std::sort(recs.begin(), recs.end());
    w.u32((uint32_t)recs.size());
    for (const auto& rec : recs) for (uint8_t c : rec) w.u8(c);
  };
  {
    std::vector<std::vector<uint8_t>> recs;
    for (uint64_t k : looted) {
      recs.emplace_back();
      BinW b(recs.back());
      if ((k >> 60) == 0xE) { b.u8(0xE); b.u64(k); continue; }
      b.u8(0); R.map(b, (int)(k >> 32)); b.u32((uint32_t)k);
    }
    writeSorted(recs);
  }
  {
    std::vector<std::vector<uint8_t>> recs;
    for (auto& kv : killedSlots) {
      if (kv.second.empty()) continue;
      std::vector<std::vector<uint8_t>> vals;
      for (int v : kv.second) {
        vals.emplace_back();
        BinW b(vals.back());
        if (kv.first == 0) { b.u64(R.site(v / 4096)); b.i32(v % 4096); }   // overworld spawns: slot + site * 4096
        else if (kv.first == -1) {                                        // dens: den * 4096 + the day it was cleared
          int d = v / 4096;
          b.u64(d >= 0 && d < (int)world.dens.size() ? world.dens[(size_t)d].id : 0); b.i32(v % 4096);
        } else { b.u64(0); b.i32(v); }
      }
      std::sort(vals.begin(), vals.end());
      recs.emplace_back();
      BinW b(recs.back());
      R.map(b, kv.first);
      b.u32((uint32_t)vals.size());
      for (const auto& v : vals) for (uint8_t c : v) b.u8(c);
    }
    writeSorted(recs);
  }
  {
    std::vector<std::pair<uint64_t, uint8_t>> fl;
    for (const Site& s : world.sites)
      if (s.discovered || s.cleared || s.rumoured) fl.push_back({s.id, (uint8_t)((s.discovered ? 1 : 0) | (s.cleared ? 2 : 0) | (s.rumoured ? 4 : 0))});
    std::sort(fl.begin(), fl.end());
    w.u32((uint32_t)fl.size());
    for (auto& f : fl) { w.u64(f.first); w.u8(f.second); }
  }
  w.u8((uint8_t)background); w.u32(storyFlags);
  writeAppearance(w, app);
  R.bldg(w, lodging.bldg); w.i32(lodging.floor); w.i32(lodging.room); w.i32(lodging.untilDay);
  // explored, in key order so the bytes do not depend on hash-map order
  std::vector<uint64_t> keys;
  for (auto& kv : explored.regions) keys.push_back(kv.first);
  std::sort(keys.begin(), keys.end());
  w.u32((uint32_t)keys.size());
  for (uint64_t k : keys) {
    w.i32(ExploredMask::keyRx(k)); w.i32(ExploredMask::keyRy(k));
    for (uint8_t b : explored.regions.at(k)) w.u8(b);
  }
  w.u32((uint32_t)marks.size());   // (std::map: already in key order)
  for (auto& kv : marks) { w.u64(kv.first); w.i32(kv.second); }
  // M4 (v10): the realm and the stories, each a length-prefixed block with its own version byte
  {
    std::vector<uint8_t> blk;
    realm.serialize(blk);
    w.u32((uint32_t)blk.size());
    for (uint8_t c : blk) w.u8(c);
    story.serialize(blk);
    w.u32((uint32_t)blk.size());
    for (uint8_t c : blk) w.u8(c);
    life.serialize(blk);   // M5 (v11)
    w.u32((uint32_t)blk.size());
    for (uint8_t c : blk) w.u8(c);
    craft.serialize(blk);  // M6 (v12)
    w.u32((uint32_t)blk.size());
    for (uint8_t c : blk) w.u8(c);
    home.serialize(blk);   // M7 (v14)
    w.u32((uint32_t)blk.size());
    for (uint8_t c : blk) w.u8(c);
    talkMem.serialize(blk);   // M7b (v15)
    w.u32((uint32_t)blk.size());
    for (uint8_t c : blk) w.u8(c);
    party.serialize(blk);     // M8 (v16)
    w.u32((uint32_t)blk.size());
    for (uint8_t c : blk) w.u8(c);
    skills.serialize(blk);    // M9 (v20)
    w.u32((uint32_t)blk.size());
    for (uint8_t c : blk) w.u8(c);
    bookOut.serialize(blk);   // M9 (v20): the book (synced with the classic fields above)
    w.u32((uint32_t)blk.size());
    for (uint8_t c : blk) w.u8(c);
    sea::Sea seaOut = sea;    // M10 (v21): the boat aboard takes the hero's tile and heading (on a copy)
    SeaOps::saving(*this, seaOut);
    seaOut.serialize(blk);
    w.u32((uint32_t)blk.size());
    for (uint8_t c : blk) w.u8(c);
    service.serialize(blk);   // M12 (v22): the army served, rank, orders, missions
    w.u32((uint32_t)blk.size());
    for (uint8_t c : blk) w.u8(c);
    lordship.serialize(blk);  // M12 (v22): the holdings, the hall's coffers, tribute, the player's realm
    w.u32((uint32_t)blk.size());
    for (uint8_t c : blk) w.u8(c);
    jobBook.serialize(blk);   // P1 (v23): the weekly job pools, the journal's leads
    w.u32((uint32_t)blk.size());
    for (uint8_t c : blk) w.u8(c);
  }
}

bool Game::deserialize(const std::vector<uint8_t>& in) {
  BinR r(in);
  if (r.u32() != SAVE_MAGIC) return false;
  const uint32_t ver = r.u32();
  if (ver < SAVE_VER_MIN || ver > SAVE_VER || r.bad) return false;   // older (or newer) saves are not loaded: the title says so
  const bool endless = r.u8() == 1;
  const int genVer = (int)r.u32();
  const uint64_t sd = r.u64();
  const float t = r.f32(), hr = r.f32();
  const int dy = r.i32();
  if (r.bad || !endless) return false;                 // (M2: the classic island is retired)
  if (genVer != ew::ENDLESS_GEN_VER) return false;     // made by another endless generator
  {
    int32_t wox = r.i32(), woy = r.i32();
    if (r.bad || std::abs(wox) > ew::WORLD_EDGE + 4096 || std::abs(woy) > ew::WORLD_EDGE + 4096) return false;
    // the window is built once, where the save was made (not at the start first): a load behind the fade
    seed = sd;
    rng_ = Rng(sd ^ 0xABCDEF);
    world.generateEndlessAt(sd, wox, woy);
    resetSession();
  }
  Refs R{world};
  time = t; hour = hr; day = dy;
  bool ins = r.u8() != 0;
  int ss = R.siteFrom(r.u64());
  const size_t sbAt = r.p;   // (M7) where the building ref lies: the player's own house is found after the home block
  int sb = R.bldgFrom(r);
  int floorIn = r.i32();
  Vec2 pp; pp.x = r.f32(); pp.y = r.f32();
  float hp = r.f32(); mp = r.f32(); stamina = r.f32();
  plLevel = r.i32(); plXp = r.i32(); gold = r.i32(); perkPts = r.i32();
  baseHp = r.f32(); maxMp = r.f32(); maxSt = r.f32();
  uint32_t n = r.u32();
  if (n > 5000) return false;
  inv.clear();
  for (uint32_t i = 0; i < n && !r.bad; i++) inv.push_back(readItem(r));
  int* eqs[] = {&eqWeapon, &eqBow, &eqStaff, &eqArmor, &eqHelmet, &eqShield, &eqRing, &eqAmulet, &eqGloves, &eqBoots, &eqCloak};
  for (int* e : eqs) { *e = r.i32(); if (*e >= (int)inv.size()) *e = -1; }
  spellsKnown = r.u8(); spell = (Spell)(r.u8() % (int)Spell::COUNT);
  kills = r.i32(); dungeonsCleared = r.i32(); blessT = r.f32(); blessName = r.str();
  lastTown = R.siteFrom(r.u64());
  n = r.u32();
  if (n > 5000) return false;
  quests.clear();
  for (uint32_t i = 0; i < n && !r.bad; i++) {
    Quest q;
    q.id = r.i32(); q.type = (QType)r.u8(); q.state = (QState)r.u8(); q.title = r.str(); q.desc = r.str(); q.giverName = r.str();
    q.giverSite = R.siteFrom(r.u64()); q.giverBldg = R.bldgFrom(r); q.giverSlot = r.i32(); q.target = R.siteFrom(r.u64());
    q.mon = (Monster)r.u8();
    q.need = r.i32(); q.have = r.i32(); q.gold = r.i32(); q.xp = r.i32(); q.stage = r.i32();
    q.targetId = r.u64(); q.hasPos = r.u8() != 0; q.tgx = r.i32(); q.tgy = r.i32(); q.giverId = r.u64(); q.subject = r.str();
    q.destBldg = R.bldgFrom(r); q.deadlineDay = r.i32(); q.flags = r.u32();
    // (a damaged save: out-of-range kinds load as harmless defaults, never as indices past a table)
    if ((int)q.type >= (int)QType::COUNT) q.type = QType::Hunt;
    if ((int)q.state > (int)QState::Done) q.state = QState::Done;
    if ((int)q.mon >= (int)Monster::COUNT) q.mon = Monster::Wolf;
    if (q.type == QType::Main) q.stage = std::clamp(q.stage, 0, 4);
    quests.push_back(q);
  }
  nextQuestId = r.i32(); trackedQuest = r.i32();
  n = r.u32();
  npcQuestsDone.clear();
  for (uint32_t i = 0; i < n && !r.bad; i++) { uint64_t k = r.u64(); npcQuestsDone[k] = r.i32(); }
  n = r.u32();
  looted.clear();
  for (uint32_t i = 0; i < n && !r.bad; i++) {
    if (r.u8() == 0xE) { looted.insert(r.u64()); continue; }
    int key = 0;
    bool ok = R.mapFrom(r, key);
    uint32_t tile = r.u32();
    if (ok) looted.insert(((uint64_t)(uint32_t)key << 32) | tile);
  }
  n = r.u32();
  killedSlots.clear();
  for (uint32_t i = 0; i < n && !r.bad; i++) {
    int key = 0;
    bool ok = R.mapFrom(r, key);
    uint32_t m = r.u32();
    if (r.bad || m > (in.size() - r.p) / 12) return false;   // (M4) more records than bytes left: a damaged save
    for (uint32_t j = 0; j < m && !r.bad; j++) {
      ew::Gid id = r.u64();
      int v = r.i32();
      if (!ok) continue;
      if (key == 0) { int s = R.siteFrom(id); if (s >= 0 && v >= 0 && v < 4096) killedSlots[0].insert(v + s * 4096); }
      else if (key == -1) { int d = world.ensureDen(id); if (d >= 0) killedSlots[-1].insert(d * 4096 + v); }
      else killedSlots[key].insert(v);
    }
  }
  n = r.u32();
  for (uint32_t i = 0; i < n && !r.bad; i++) {
    ew::Gid id = r.u64();
    uint8_t f = r.u8();
    int s = R.siteFrom(id);
    if (s >= 0) {
      world.sites[(size_t)s].discovered = f & 1; world.sites[(size_t)s].cleared = (f & 2) != 0; world.sites[(size_t)s].rumoured = (f & 4) != 0;
    }
  }
  uint8_t bg = r.u8();
  background = bg < (uint8_t)Background::COUNT ? (Background)bg : Background::None;
  storyFlags = r.u32();
  if (!readAppearance(r, app)) return false;
  lodging = Lodging();
  lodging.bldg = R.bldgFrom(r); lodging.floor = r.i32(); lodging.room = r.i32(); lodging.untilDay = r.i32();
  if (lodging.bldg < 0) lodging = Lodging();
  n = r.u32();
  if (n > 1000000) return false;
  explored.clear();
  for (uint32_t i = 0; i < n && !r.bad; i++) {
    int32_t rx = r.i32(), ry = r.i32();
    std::vector<uint8_t> m((size_t)ExploredMask::CELLS * ExploredMask::CELLS / 8);
    for (uint8_t& b : m) b = r.u8();
    explored.regions[ExploredMask::key(rx, ry)] = std::move(m);
  }
  n = r.u32();
  if (n > 1000000) return false;
  marks.clear();
  for (uint32_t i = 0; i < n && !r.bad; i++) { uint64_t k = r.u64(); marks[k] = r.i32(); }
  // M4 (v10): the realm and story blocks, M5 (v11) the life block, M6 (v12) the craft block, M7 (v14) the home block,
  // M7b (v15) the talk block, M8 (v16) the party block (a block its owner cannot read refuses the whole save)
  // M9 (v20) the skills and magic blocks (a SAVE_VER 19 save has neither: defaults from its level below)
  // M10 (v21) the sea block, M12 (v22) the service and lordship blocks, P1 (v23) the jobs block
  const int nBlocks = 13;
  skills = skills::Skills();
  arcana = magic::Arcana();
  sea = sea::Sea();
  service = service::Service();
  lordship = lord::Lordship();
  jobBook.clear();
  for (int blkI = 0; blkI < nBlocks && !r.bad; blkI++) {
    n = r.u32();
    if (r.bad || n > (64u << 20) || r.p + n > in.size()) return false;
    std::vector<uint8_t> blk(in.begin() + (std::ptrdiff_t)r.p, in.begin() + (std::ptrdiff_t)(r.p + n));
    r.p += n;
    const bool ok = blkI == 0 ? realm.deserialize(blk) : blkI == 1 ? story.deserialize(blk) : blkI == 2 ? life.deserialize(blk)
                    : blkI == 3 ? craft.deserialize(blk) : blkI == 4 ? home.deserialize(blk) : blkI == 5 ? talkMem.deserialize(blk)
                    : blkI == 6 ? party.deserialize(blk) : blkI == 7 ? skills.deserialize(blk) : blkI == 8 ? arcana.book.deserialize(blk)
                    : blkI == 9 ? sea.deserialize(blk) : blkI == 10 ? service.deserialize(blk) : blkI == 11 ? lordship.deserialize(blk)
                    : jobBook.deserialize(blk);
    if (!ok) return false;
  }
  arcana.live.legacyMirror = (int8_t)spell;   // (the save wrote them synced: nothing to reselect)
  MagicOps::loaded(*this);   // (M9) the traditions of the lands the book has touched made again (their spells known to spellById)
  MagicOps::syncLegacy(*this);
  if (r.bad) return false;
  homeSerial_ = world.placeSerial + 0x80000000u;   // M7: the player's plots are stamped on the first step
  if (ins && sb < 0 && !home.plots.empty()) {      // M7: saved inside the player's own house (a Bldg the home stamps)
    BinR rb(in);
    rb.p = sbAt;
    const ew::Gid hid = rb.u64();
    home::stampWindow(*this);
    sb = world.bldgHandle(hid);
  }
  // re-apply looted overworld chests (by global tile)
  world.over.rebuildSolid();
  reapplyLooted();
  recalcPlayer();
  // (fixer M4 r1) the realm's owners on the loaded settlements before anyone is spawned: a save made inside a conquered
  // town's building (its palace) loads with the conqueror's guards and ruler, not the generator's (realmStep waits
  // outside). Only the states the realm already holds are applied (noting new sites here would change the realm the
  // save holds: realmStep notes them on the first step outside, as before)
  realmSync();
  MagicOps::trialsLoaded(*this);   // (fixer M10 r1) a save inside a Trial loads back as the trial (genRuin reads the registry)
  if (ins && sb >= 0 && sb < (int)world.over.bldgs.size()) {
    enterBuilding(sb);
    if (floorIn > 0 && floorIn < world.over.bldgs[(size_t)sb].floors()) changeFloor(floorIn);
  } else if (ins && ss >= 0 && ss < (int)world.sites.size()) enterSite(ss);
  else clearNonPlayer();
  pl().p = pp;
  // a position off the map (a damaged save) would leave the player stuck in the void: stand them on a free tile
  if (!(std::isfinite(pp.x) && std::isfinite(pp.y)) || !map().in((int)std::floor(pp.x / TILE), (int)std::floor(pp.y / TILE))) {
    if (inside) placePlayerAt(sub.exitX, sub.exitY - 1);
    else placePlayerAt(world.over.w / 2, world.over.h / 2);
  }
  if (!std::isfinite(hp)) hp = 1;
  pl().hp = hp;
  SeaOps::loaded(*this);    // (M10) the boat aboard back under the hero (sea_game.cpp)
  LordOps::loaded(*this);   // (M12) the renown mirror of the lordship (lord_game.cpp)
  ServiceOps::loaded(*this);   // (M12) the renown mirror of the service (service_game.cpp)
  JobOps::loaded(*this);       // (P1) the job pools and leads (jobs.cpp)
  if (hp <= 0) respawn();   // saved on the death screen: wake in town rather than standing up with 1 HP where you fell
  events.clear();
  updateLocation();
  return true;
}
