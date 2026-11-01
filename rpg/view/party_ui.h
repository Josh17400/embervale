// PartyUi (SOLID 4.2, View part: the M8 Fellowship lane), a value member of View (View::party_). The party column
// under the vitals, the command radial and its refusals, the alert ribbons and banter plates (and the boxes a spoken
// line keeps clear of: hudBands), the companions in the world (the hound, the downed and their binder, a mounted
// companion, the graves of the fallen and their names), the party on the minimap and the world map, and the menu's
// PARTY page. Bodies: rpg/view/party_hud.cpp, party_view.cpp, paperdoll.cpp (the PARTY page), hud.cpp (hudBands).
#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>
#include "engine/pix.h"
#include "rpg/art.h"
#include "rpg/sim/game.h"

class View;

struct PartyUi {
  void partyWorldText(View& v, Game& g);    // party_view.cpp: the tapped companion's name, the graves' names (the UI layer)
  struct GraveTop { int fi = -1; float x = 0, feet = 0, top = 0; };   // a grave drawn this frame (map px)
  std::vector<GraveTop> graveTops_;
  struct PartyCard { int member = -1; float x = 0, y = 0, w = 0, h = 0; bool small = false; };
  std::vector<PartyCard> partyCards_;          // laid out by drawPartyHud this frame (the taps' hit tests)
  std::vector<int> partyActor_;                // members index -> g.actors index this frame (-1 not in play)
  float partyColR_ = 0, partyColB_ = 0;        // the column's right edge and foot this frame (0 none: toasts keep clear)
  int partySel_ = 0;                           // the selected member (the radial's and the PARTY page's)
  float partyTagT_ = 0;                        // the selected card's name tag, shown for a moment after a tap
  struct PartyPress { bool on = false; uint64_t id = 0; int member = -1; Vec2 start, cur; float t = 0; bool mouse = false; };
  PartyPress partyPress_;                      // a finger (or the mouse) held on a card: a hold opens the radial
  struct Radial {
    bool open = false, drag = false, armed = false;   // drag: opened by a hold (release picks); armed: the finger moved
    int member = -1, hot = -1;
    Vec2 c, from;
    float t = 0;
    uint64_t finger = 0;
    float denyT = 0;                           // a refused order: the wedge flashes and the reason shows
    int denyWedge = -1;
    std::string deny;
  };
  Radial radial_;
  struct PartyNote { uint32_t id = 0; int kind = 0; std::string s; float t = 0, life = 3;   // id: Member::id
                     float held = 0; bool shown = false; };   // (fixer M8 r3) banter: seconds kept waiting; drawn this frame
  std::vector<PartyNote> banterShow_, alertShow_;
  // (fixer M8 r1, review: "speech plates and ribbons draw on top of each other and over the town banner") the boxes a
  // spoken line keeps clear of this frame (screen px): the HUD's bands (hudBands: the arrival banner, the herald, the
  // biome name, the party ribbon) and every plate already placed (the M5 speech plates, then the banter)
  struct PlateBox { float x0 = 0, y0 = 0, x1 = 0, y1 = 0; };
  std::vector<PlateBox> plateBoxes_;
  void hudBands(View& v, const Game& g, std::vector<PlateBox>& out);
  bool partyRibbonBox(View& v, const Game& g, PlateBox& b, float& textW);   // where the party ribbon stands this frame (false none)
  // moves a plate (top-left x0, y0; w x h) to the nearest place inside L..R x T..B that overlaps none of `avoid`
  static void clearPlate(float& x0, float& y0, float w, float h, const std::vector<PlateBox>& avoid, float L, float T, float R, float B);
  int partyMapSeen_ = -1;                      // Party::mapChanges last seen: a new place clears the old scene's plates
  std::string partyDeny_;                      // the last refused order's reason by its card (scripts read it)
  float partyDenyT_ = 0;
  uint32_t partyDenyId_ = 0;
  float howlT_ = 30;
  int partyPage_ = 0, partySlot_ = 0, partyPack_ = 0, partyScroll_ = 0, partyFocus_ = 0;   // the PARTY page's state
  int partyDollTurn_ = 0;                      // (fixer M8 r2) the companion doll's facing: 0 front, 1 right, 2 back, 3 left (< TURN >)
  int memberOf(const Game& g, uint32_t id) const;          // members index of a Member::id (-1 gone)
  int companionIndex(const Game& g, const Actor& a) const; // members index an actor embodies (-1 none)
  void partyEvent(Game& g, const Event& e);    // Ev::Banter, Ev::PartyAlert (update's event pass)
  void partyUpdate(View& v, Game& g, float dt);         // timers, the hold, the howl
  void drawPartyHud(View& v, Game& g);                  // the column, the alerts (drawHud)
  void drawPartyRadial(View& v, Game& g);               // the radial and the refusals (over the whole HUD)
  void drawBanter(View& v, Game& g, Vec2 cam);          // the banter plates over the speakers (after the lighting)
  bool partyPointer(View& v, Game& g, int phase, uint64_t id, Vec2 p, bool mouse);   // 0 down 1 move 2 up; true: taken
  bool partyKey(View& v, Game& g, int key);             // C: the radial; while it is open every key is its
  void radialOpen(View& v, Game& g, int member, bool drag, uint64_t finger);
  int radialWedgeAt(const View& v, Vec2 p) const;             // -1 the hub, -2 outside, else the Cmd
  void radialChoose(View& v, Game& g, int cmd);
  std::string radialWhy(const Game& g, int member, int cmd) const;   // why an order cannot be given now ("" it can)
  int bestSkill(const Game& g, int member, float& cd, bool& locked) const;   // the Skill wedge's skill (index 0..2, -1 none)
  art::HoundLook houndLookOf(const party::Member& m, const Game& g) const;
  void drawPortraitCard(View& v, Game& g, int mi, float x, float y, bool small);
  // the world (party_view.cpp)
  bool drawCompanionBody(View& v, Game& g, const Actor& a, Vec2 cam, float alpha, float flash);   // true: drawn (hound, rider)
  void drawCompanionShadow(View& v, Game& g, const Actor& a, Vec2 cam, bool& done);
  void companionOverlays(View& v, Game& g, Vec2 cam);   // the wound mark and the binding ring over the downed (after lighting)
  art::Posture partyPosture(const Game& g, const Actor& a) const;    // Downed / Kneel the sim leaves as None
  void graveDraws(Game& g, const Map& m, Vec2 cam, std::vector<int>& out);   // the fallen whose graves are in view
  void drawGrave(View& v, Game& g, int fallen, Vec2 cam, bool shadow);
  void drawPartyMinimap(View& v, Game& g, float x, float y, int size, int ox, int oy);
  void drawPartyWorldMap(View& v, Game& g, Vec2 (*toScr)(void*, double, double), void* ctx, float w, float h);
  void drawPartyPage(View& v, Game& g);
  void partyPageKey(View& v, Game& g, int key);
  void partyPageTap(View& v, Game& g, Vec2 p);
};
