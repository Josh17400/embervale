// M12 "Rise and Fall": the service block's data, tables and persistence (rpg/sim/service.h). SERVICE lane (phase A by
// the lead).
#include "rpg/sim/service.h"
#include <algorithm>
#include "rpg/sim/common.h"

namespace service {
namespace {
constexpr uint8_t SERVICE_BLOCK_VER = 2;   // v2 (SERVICE lane, phase B): the orders' play state, the hunt, siegeDay (v1 still read)
}  // namespace

const char* rankName(Rank r) {
  static const char* n[] = {"", "RECRUIT", "SERGEANT", "CAPTAIN", "MARSHAL"};
  static_assert(sizeof(n) / sizeof(n[0]) == (size_t)Rank::COUNT, "a name for every rank");
  return (size_t)r < (size_t)Rank::COUNT ? n[(size_t)r] : "";
}
int wageOf(Rank r) {
  static const int w[] = {0, 12, 30, 70, 150};
  return (size_t)r < (size_t)Rank::COUNT ? w[(size_t)r] : 0;
}
int meritToRise(Rank r) {
  static const int m[] = {0, 30, 80, 180, 0};
  return (size_t)r < (size_t)Rank::COUNT ? m[(size_t)r] : 0;
}
int squadOf(Rank r) {
  static const int s[] = {0, 0, 2, 4, 6};
  return (size_t)r < (size_t)Rank::COUNT ? s[(size_t)r] : 0;
}
const char* orderName(OrderKind k) {
  static const char* n[] = {"", "BREAK THE SIEGE OF", "TAKE", "SUPPLY", "SCOUT", "KILL THE CAPTAIN AT", "ESCORT THE PEOPLE OF", "HOLD"};
  static_assert(sizeof(n) / sizeof(n[0]) == (size_t)OrderKind::COUNT, "a name for every order");
  return (size_t)k < (size_t)OrderKind::COUNT ? n[(size_t)k] : "";
}
const char* missionName(MissionKind k) {
  static const char* n[] = {"", "CARRY THE TERMS TO", "SEEK AN ALLIANCE WITH", "DEMAND TRIBUTE OF", "GREET THE COURT OF"};
  static_assert(sizeof(n) / sizeof(n[0]) == (size_t)MissionKind::COUNT, "a name for every mission");
  return (size_t)k < (size_t)MissionKind::COUNT ? n[(size_t)k] : "";
}

bool Service::deserterOf(Gid k) const {
  for (const Past& p : past) if (p.kingdom == k && p.how == PAST_DESERTED) return true;
  return false;
}
Order* Service::order(uint32_t id) {
  for (Order& o : orders) if (o.id == id) return &o;
  return nullptr;
}
Mission* Service::mission(uint32_t id) {
  for (Mission& m : missions) if (m.id == id) return &m;
  return nullptr;
}

// Layout (SERVICE_BLOCK_VER 1): ver u8, sworn u64, rank u8, merit i32, enlistDay u16, paidDay u16, lastOrderDay u16,
// rankDay u16, battles u16, ordersDone u16, ordersFailed u16, missionsDone u16, nextId u32;
// orders (u32 n: id u32, kind u8, site u64, war u32, siege u32, gx i32, gy i32, given u16, due u16, quest i32, state u8,
// merit u8); missions (u32 n: id u32, kind u8, from u64, to u64, toSite u64, gx i32, gy i32, given u16, due u16,
// quest i32, state u8, outcome u8); past (u32 n: kingdom u64, rank u8, how u8, day u16).
// v2 adds, after each order's merit: flags u8, squad u8, foe u64, from u64, aux u16, untilDay u16; and at the end:
// huntedBy u64, huntedUntil u16, siegeDay u16.
void Service::serialize(std::vector<uint8_t>& out) const {
  out.clear();
  BinW w(out);
  w.u8(SERVICE_BLOCK_VER);
  w.u64(sworn); w.u8((uint8_t)rank); w.i32(merit);
  w.u16(enlistDay); w.u16(paidDay); w.u16(lastOrderDay); w.u16(rankDay);
  w.u16(battles); w.u16(ordersDone); w.u16(ordersFailed); w.u16(missionsDone); w.u32(nextId);
  w.u32((uint32_t)orders.size());
  for (const Order& o : orders) {
    w.u32(o.id); w.u8((uint8_t)o.kind); w.u64(o.site); w.u32(o.war); w.u32(o.siege); w.i32(o.gx); w.i32(o.gy);
    w.u16(o.givenDay); w.u16(o.dueDay); w.i32(o.quest); w.u8(o.state); w.u8(o.merit);
    w.u8(o.flags); w.u8(o.squad); w.u64(o.foe); w.u64(o.from); w.u16(o.aux); w.u16(o.untilDay);
  }
  w.u32((uint32_t)missions.size());
  for (const Mission& m : missions) {
    w.u32(m.id); w.u8((uint8_t)m.kind); w.u64(m.from); w.u64(m.to); w.u64(m.toSite); w.i32(m.gx); w.i32(m.gy);
    w.u16(m.givenDay); w.u16(m.dueDay); w.i32(m.quest); w.u8(m.state); w.u8(m.outcome);
  }
  w.u32((uint32_t)past.size());
  for (const Past& p : past) { w.u64(p.kingdom); w.u8((uint8_t)p.rank); w.u8(p.how); w.u16(p.day); }
  w.u64(huntedBy); w.u16(huntedUntil); w.u16(siegeDay);
}

bool Service::deserialize(const std::vector<uint8_t>& in) {
  BinR r(in);
  const uint8_t ver = r.u8();
  if (r.bad || ver < 1 || ver > SERVICE_BLOCK_VER) return false;
  auto fits = [&](uint32_t n, size_t minBytes) { return !r.bad && (uint64_t)n * minBytes <= (uint64_t)(in.size() - std::min(in.size(), r.p)); };
  Service S;
  S.sworn = r.u64(); S.rank = (Rank)std::min<uint8_t>(r.u8(), (uint8_t)Rank::COUNT - 1); S.merit = r.i32();
  S.enlistDay = r.u16(); S.paidDay = r.u16(); S.lastOrderDay = r.u16(); S.rankDay = r.u16();
  S.battles = r.u16(); S.ordersDone = r.u16(); S.ordersFailed = r.u16(); S.missionsDone = r.u16(); S.nextId = r.u32();
  const uint32_t no = r.u32();
  if (!fits(no, ver >= 2 ? 61 : 39)) return false;
  for (uint32_t i = 0; i < no && !r.bad; i++) {
    Order o;
    o.id = r.u32(); o.kind = (OrderKind)std::min<uint8_t>(r.u8(), (uint8_t)OrderKind::COUNT - 1); o.site = r.u64();
    o.war = r.u32(); o.siege = r.u32(); o.gx = r.i32(); o.gy = r.i32(); o.givenDay = r.u16(); o.dueDay = r.u16();
    o.quest = r.i32(); o.state = std::min<uint8_t>(r.u8(), OS_FAILED); o.merit = r.u8();
    if (ver >= 2) { o.flags = r.u8(); o.squad = r.u8(); o.foe = r.u64(); o.from = r.u64(); o.aux = r.u16(); o.untilDay = r.u16(); }
    S.orders.push_back(o);
  }
  const uint32_t nm = r.u32();
  if (!fits(nm, 47)) return false;
  for (uint32_t i = 0; i < nm && !r.bad; i++) {
    Mission m;
    m.id = r.u32(); m.kind = (MissionKind)std::min<uint8_t>(r.u8(), (uint8_t)MissionKind::COUNT - 1); m.from = r.u64();
    m.to = r.u64(); m.toSite = r.u64(); m.gx = r.i32(); m.gy = r.i32(); m.givenDay = r.u16(); m.dueDay = r.u16();
    m.quest = r.i32(); m.state = std::min<uint8_t>(r.u8(), OS_FAILED); m.outcome = r.u8();
    S.missions.push_back(m);
  }
  const uint32_t np = r.u32();
  if (!fits(np, 12)) return false;
  for (uint32_t i = 0; i < np && !r.bad; i++) {
    Past p;
    p.kingdom = r.u64(); p.rank = (Rank)std::min<uint8_t>(r.u8(), (uint8_t)Rank::COUNT - 1); p.how = r.u8(); p.day = r.u16();
    S.past.push_back(p);
  }
  if (ver >= 2) { S.huntedBy = r.u64(); S.huntedUntil = r.u16(); S.siegeDay = r.u16(); }
  if (r.bad || r.p != in.size()) return false;
  if (!S.sworn) S.rank = Rank::None;
  *this = std::move(S);
  return true;
}

}  // namespace service
