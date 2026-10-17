// M12 "Rise and Fall" VIEW lane: the RISE block's private state and helpers, shared by its own files only
// (render_rise.cpp: the herald's banners, the insignia, the coin chips; rise_standing.cpp: the journal's STANDING page;
// rise_world.cpp: the lord's banners in their owners' arms, the holdings' works, warlords' camps, resettled and raided
// places). RiseViewOps is View's friend (view.h), so these reach View's internals.
#pragma once
#include <cstdint>
#include <deque>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>
#include "rpg/culture/culture.h"
#include "rpg/view/view.h"

struct View::RiseViewState {
  struct Banner {
    RiseEv kind = RiseEv::None;
    int f = 0;
    std::string top, line;
    ew::Gid kingdom = 0;
    int rank = 0;              // the badge on the right (0: the realm's arms again)
    float t = 0, dur = 3;
    bool bad = false, major = false;
  };
  struct Chip { std::string label; int gold = 0; float t = 0; bool out = false; };
  RiseViewStats stats;
  std::deque<Banner> queue;
  Banner cur;
  bool showing = false;
  float gap = 0;                                // a short breath between two banners
  std::vector<Chip> chips;
  std::unordered_map<uint64_t, Tex> badges;     // rank badges by (rank, field, trim, size)
  // rise_world.cpp
  std::unordered_map<uint64_t, Tex> worldTex;   // lord banners, warlord standards, trophies, stores, claw scars
  struct Piece { int kind = 0; float x = 0, y = 0; int var = 0; uint32_t field = 0, trim = 0; uint64_t key = 0; };
  std::vector<Piece> pieces;                    // this frame's standing pieces (Drawable kind 12: idx into it)
  std::vector<Piece> flats;                     // this frame's ground pieces (drawn under everything)
  float smokeT = 0;
  int bannerCount = 0;                          // lord's banners drawn this frame (props and works)
  // rise_standing.cpp
  int standingScroll = 0;
};

struct RiseViewOps {
  static View::RiseViewState& st(View& v);
  // the kingdom a RiseEv names (Event::id: the low 32 bits of its Gid)
  static ew::Gid kingdomByLow(const Game& g, uint32_t low);
  // a kingdom's arms as the view draws them (its culture's heraldry, else its colours and emblem); color: its banner colour
  static void armsOf(const Game& g, ew::Gid k, cult::Heraldry& h, uint32_t* color);
  static const Tex& badgeTex(View& v, int rank, uint32_t field, uint32_t trim, int size);
  static const Tex& armsTex(View& v, const cult::Heraldry& h, int size);
  // a settlement's name: the loaded site's, else its region plan's (empty: unknown)
  static std::string siteName(const Game& g, ew::Gid id);
  static Pix& pix(View& v) { return *v.pix_; }
  static Audio& audio(View& v) { return *v.audio_; }
  static float now(const View& v) { return v.t_; }
  static bool touch(const View& v) { return v.touchUI; }
  static void button(View& v, float x, float y, float w, float h, const std::string& s, bool sel) { v.button(x, y, w, h, s, sel); }
  // the enlisted hero's look: the sworn realm's tabard over the gear and the rank's mark (a marshal's cloak in its field)
  static void dressHero(const Game& g, art::HumanLook& L);
  // rise_standing.cpp: the STANDING page (realm_hud.cpp's drawJournalSection hands section 3 over)
  static void drawStanding(View& v, Game& g, float top);
  static bool standingTap(View& v, Game& g, Vec2 p, float top);
  static void standingKey(View& v, Game& g, int key);
  static void standingSelect(View& v, int row) { v.menuSel_ = row < 0 ? 0 : row; }   // (scripts: riseview row n)
};
