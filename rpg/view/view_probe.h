// ViewProbe (SOLID 4.2, View part 5): the test and Witness hooks that only the script commands (rpg/view/scripts/) and
// the Witness state (witness_state.cpp) use: the camera and open menu page they report, the forced weather and town
// look, the presented layers, the herald and journal state, the M4 / M5 / loot counters and the last item card, and the
// `partyview` commands. A friend of View with static functions only: the state stays where the draws keep it. Include
// it ONLY from rpg/view/scripts/*.cpp and rpg/view/witness_state.cpp (view.h never does), and from paperdoll.cpp, which
// defines equipPage beside the EQUIP page's state (P4 WITNESS).
#pragma once
#include <string>
#include <vector>
#include "rpg/view/view.h"

struct ViewProbe {
  static Vec2 camera(const View& v) { return v.camera_.cam_; }   // M2: top-left of the view in world pixels (scripts tap world things)
  static int menuTab(const View& v) { return v.menuTab_; }   // the open menu's page (Mode::Menu; the Witness harness reports it)
  // (M3c) test scripts: force a weather kind (rpg/world/biomes.h Sky value, at full strength; -1 back to the land's own)
  static void scriptSky(View& v, int sky) { v.weather_.skyOverride_ = sky; v.weather_.wxInit_ = false; v.weather_.wxSampleT_ = 0; }
  static int scriptSkyNow(const View& v) { return v.weather_.skyOverride_; }
  // (Witness state.screen) the layers and the window as presented
  static const Pix::Layer& worldLayerForTests(const View& v) { return v.pix_->worldLayer(); }
  static const Pix::Layer& uiLayerForTests(const View& v) { return v.pix_->uiLayer(); }
  static void outputSizeForTests(const View& v, int& w, int& h) { w = h = 0; if (v.pix_) v.pix_->outputSize(w, h); }
  static int journalSection(const View& v) { return v.realm_.journalSec_; }
  static bool heraldShowing(const View& v) { return v.realm_.herald_.t > 0; }
  static const std::string& heraldTitle(const View& v) { return v.realm_.herald_.title; }
  static View::M4Stats m4Stats(const View& v) { return v.realm_.m4Stats_; }
  static View::M5Stats m5Stats(const View& v) { return v.life_.m5Stats_; }
  // (scripts) force a settlement's festival dressing / shutters on or off whatever its census says (-1: its own)
  static void scriptLifeLook(View& v, int festival, int shuttered) { v.life_.forceFest_ = festival; v.life_.forceShut_ = shuttered; }
  // (scripts) `partyview <words...>` / `expect partyview <words...>` (script_partyview.cpp); false with err when refused
  static bool partyViewCommand(View& v, Game& g, const std::vector<std::string>& a, std::string& err);
  static bool partyViewExpect(View& v, Game& g, const std::vector<std::string>& a, std::string& err);
  static View::LootStats lootStats(const View& v) { return v.loot_.lootShown_; }
  static View::CardInfo lastCard(const View& v) { return v.loot_.lastCard_; }
  // (P4 WITNESS) the menu's row cursor and the shop's side, row and armed sale (a worn piece's first Enter), as drawn
  static int menuCursor(const View& v) { return v.menuSel_; }
  static int shopSide(const View& v) { return v.shopSide_; }
  static int shopCursor(const View& v) { return v.shopSel_; }
  static int shopArmed(const View& v) { return v.shopArm_; }
  // (P4 WITNESS) the EQUIP page: each slot's name and what it wears, the slot under the cursor and the pack piece picked
  // for it (cand -1: none to pick). Defined in paperdoll.cpp, beside the page's own state; read-only.
  struct EquipRow { std::string slot, item; };
  struct EquipPage { std::vector<EquipRow> slots; int slot = 0, cand = -1, candidates = 0; std::string candidate; bool candidateWorn = false; };
  static EquipPage equipPage(const View& v, const Game& g);
};
