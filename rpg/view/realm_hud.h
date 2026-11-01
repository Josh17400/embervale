// RealmHud (SOLID 4.2, View part: the M4 "Banners" lane), a value member of View (View::realm_). The border herald,
// the news, the arrival banner's owner and state, the journal's section strip and its NEWS / LOST HISTORY / STANDING
// sections, the fires and smoke of burned places, the siege camps' light, the M4 counters. Bodies:
// rpg/view/realm_hud.cpp, realm_render.cpp.
#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>
#include "engine/pix.h"
#include "rpg/art.h"
#include "rpg/sim/game.h"
#include "rpg/view/fx_state.h"   // LightPool
class View;

struct RealmHud {
  // (scripts) the last frame's counts of drawn M4 overlays: war props in an owner's colours, burned buildings smoking,
  // siege-camp fires lit
  struct M4Stats { int warProps = 0, warPropsOwned = 0, burned = 0, campFires = 0, mapMarkers = 0, borderPx = 0; };
  int arriveSite_ = -2;                      // the settlement the arrival banner last announced
  struct Herald { float t = 0; std::string top, title; uint32_t color = 0; ew::Gid kingdom = 0; bool wild = false; };
  Herald herald_;
  M4Stats m4Stats_, m4Count_;
  int journalSec_ = 0;                        // the QUESTS tab: 0 quests, 1 news, 2 lost history, 3 standing (M12)
  ew::Gid arriveOwner_ = 0;                   // the owner and state flags the arrival banner last announced
  uint16_t arriveFlags_ = 0;
  std::string bannerState_;                   // a third line under the arrival banner ("OCCUPIED BY QIBA")
  uint32_t bannerStateCol_ = 0;
  void heraldEvent(View& v, Game& g, const Event& e);
  void newsEvent(View& v, Game& g, const Event& e);
  void drawHerald(View& v, Game& g);
  // the arrival banner's text for the settlement the player stands in (its trade, its current owner in its society's
  // word, its state); refreshed when the owner or the state changes while the player is there
  void arrivalText(Game& g, int site, std::string& line, std::string& state, uint32_t& stateCol);
  void drawJournalStrip(View& v, float lx, float lw, float top);
  bool journalStripTap(View& v, Vec2 p, float lx, float lw, float top);
  void drawJournalSection(View& v, Game& g, float top);
  bool journalSectionTap(View& v, Game& g, Vec2 p, float top);
  void burnedFx(View& v, Game& g, const Map& m, const Bldg& b, int index, float dt);
  float burnT_ = 0;
  void m4Lights(View& v, Game& g, const Map& m, Vec2 cam, float dark, std::vector<LightPool>& out);
  void m4Emissive(View& v, Game& g, const Map& m, Vec2 cam, float dark);   // (fixer M4 r3) burned shells glowing over the night
  bool nearSiege(Game& g) const;              // the player is in or near a besieged settlement (the Siege music)
};
