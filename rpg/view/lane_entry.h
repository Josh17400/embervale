// The opaque-state lanes' entry points (SOLID 4.2, View part: lanes). Each lane keeps its state in its own opaque
// struct (View::MagicFxState, SkillsUiState, SeaViewState, ReliefViewState, RiseViewState, defined in its .cpp) and
// reaches View's internals through its friend Ops struct; the View calls the lane through these statics (formerly View
// members of the same names), each taking the View. Declared after class View (view.h includes this at its end).
#pragma once
#include <functional>
#include "rpg/view/view.h"

// M9 Arcana & Mastery (VISION_PLAN 15.29, 15.30). Two blocks, each owned by one lane; view.h itself is frozen:
//      each lane keeps its state in its own opaque struct (defined in its .cpp) and reaches View's internals through
//      its friend Ops struct (static functions in its own files), so neither needs to edit this header.
// SPELLFX lane (rpg/view/render_magic.cpp + its own new files): every spell's life on screen (wind-up, launch,
//   travel, impact, aftermath: Ev::Spell phases from the sim; ProjKind::Spell missiles), the surfaces' animated pixel
//   art (magic::Field), the statuses' overlays, the spells' light (lit trails, impact flashes, lightning's frame
//   flash), decals, baked effect sheets and pooled particles inside the phone frame budget, the spell sounds.
struct MagicLane {
  using MagicFxState = View::MagicFxState; using MagicFxStats = View::MagicFxStats;   // the lane's own types, as the bodies name them
  static void magicEvent(View& v, Game& g, const Event& e);    // Ev::Spell (the update's event pass, spawnParticles)
  static void magicUpdate(View& v, Game& g, float dt);         // age the effects (update, after powerUpdate)
  static void magicShift(View& v, Vec2 d);                     // Ev::WindowShift: effects and decals move with the window
  static void drawMagicGround(View& v, Game& g, Vec2 cam);     // world space under the bodies: surfaces, decals, ground glyphs (drawWorld)
  static void drawSpellProj(View& v, Game& g, const Projectile& pr, int idx, Vec2 cam);   // a ProjKind::Spell missile in the sorted pass
  static void magicLights(View& v, Game& g, const std::function<void(Vec2, float, Color, float)>& light);   // drawLighting
  static void drawMagicFx(View& v, Game& g, Vec2 cam);         // world space after the lighting: bursts, trails, flashes (View::draw)
};

// UI lane (rpg/view/skills_ui.cpp, rpg/view/spellbook_ui.cpp + hud.cpp, which it owns): the skills page (every skill's
//   level and progress; the perk trees as tappable constellations / culture glyph trees; the weapon families' vs-foe
//   lines; respec), the spellbook page (known spells, the 3-4 quick slots, tomes), the quick-cast buttons on the phone
//   (tap to cast, hold to upcast, swipe to dual-cast), skill-up and perk toasts (Ev::Skill), trainers' screen.
struct SkillsLane {
  using SkillsUiState = View::SkillsUiState;   // the lane's own types, as the bodies name them
  static void skillEvent(View& v, Game& g, const Event& e);    // Ev::Skill (the update's event pass): toasts, the level-up glow
  static void drawSkillsPage(View& v, Game& g);                // menuTab_ == kSkillsTab (hud.cpp's drawMenu calls it)
  static void skillsPageKey(View& v, Game& g, int key);
  static void skillsPageTap(View& v, Game& g, Vec2 p);
  static void drawSpellsPage(View& v, Game& g);                // menuTab_ == kSpellsTab
  static void spellsPageKey(View& v, Game& g, int key);
  static void spellsPageTap(View& v, Game& g, Vec2 p);
};

// M10 Beyond the Mist (VISION_PLAN 2.12, 15.4, 15.31). One block, the ART lane's (rpg/view/render_sea.cpp + its
//      own new files; view.h itself is frozen: the state lives in the opaque SeaViewState, View's internals are
//      reached through the friend SeaViewOps). The sea on screen and in the ear: waves, foam and the shore's surf on
//      the water, wakes, the boats (each culture's ships, the one aboard with the hero on deck, moored ones, ferries,
//      pirates), the Mist's fog, storms (rain, lightning, the darkened sky), the sea creatures' splashes, the
//      sail HUD (wind, heading, hull, speed) and the fishing panel (cast, bite, the reel's tension), the sea's sounds.
struct SeaLane {
  using SeaViewState = View::SeaViewState; using SeaViewStats = View::SeaViewStats;   // the lane's own types, as the bodies name them
  static void seaEvent(View& v, Game& g, const Event& e);      // Ev::Sea (the update's event pass, spawnParticles)
  static void seaUpdate(View& v, Game& g, float dt);           // age wakes, splashes, rain, lightning (update, after magicUpdate)
  static void seaShift(View& v, Vec2 d);                       // Ev::WindowShift: wakes and splashes move with the window
  static void drawSeaWater(View& v, Game& g, Vec2 cam);        // world space over the terrain, under the bodies: waves, foam, wakes
  // the sorted pass: push(y, idx) entries for the boats (and anything else of the sea that stands up: a serpent's
  // coils, a mast); drawSeaSorted draws entry idx when its turn comes (Drawable kind 10)
  static void seaSorted(View& v, Game& g, Vec2 cam, const std::function<void(float, int)>& push);
  static void drawSeaSorted(View& v, Game& g, int idx, Vec2 cam);
  static void seaLights(View& v, Game& g, const std::function<void(Vec2, float, Color, float)>& light);   // drawLighting: lanterns, the lighthouse
  static void drawSeaOver(View& v, Game& g, Vec2 cam);         // world space after the lighting and weather: the Mist, storm rain, lightning
  static void drawSeaHud(View& v, Game& g);                    // screen space (drawHud, under the vitals pass): the sail and fishing panels
  static void seaFillInput(View& v, Game& g, Input& in);       // the HUD's input pass (after the quick-cast slots): Input::reel
};

// M11 High Ground (VISION_PLAN 11.3, 13 M11, 15.32). One block, the FALLS lane's (rpg/view/render_relief.cpp + its
//      own new files; view.h itself is frozen for that lane: the state lives in the opaque ReliefViewState, View's
//      internals are reached through the friend ReliefViewOps). What of the relief MOVES or stands up, drawn live
//      over the baked terrain: waterfalls (Map::HEIGHT_FALL tiles: the falling sheet over the face, 3+ frames, the
//      foam pool and spray at the foot, mist particles), plateau lakes' spills, the falls' roar (Audio::setFalls,
//      by the nearest fall and its side), drifting cloud shadows and high-ground haze. The baked look of the relief
//      (faces, slopes, stairs, grading, cast shadows) stays in terrain.cpp (the TERRAIN lane).
struct ReliefLane {
  using ReliefViewState = View::ReliefViewState; using ReliefViewStats = View::ReliefViewStats;   // the lane's own types, as the bodies name them
  static void reliefUpdate(View& v, Game& g, float dt);        // update, after seaUpdate: mist, spray, the roar's level
  static void reliefShift(View& v, Vec2 d);                    // Ev::WindowShift: particles move with the window
  static void drawReliefWater(View& v, Game& g, Vec2 cam);     // world space over the terrain, before the sea's water: the falls
  // the sorted pass: push(y, idx) for anything of the relief that stands up among the bodies (a fall's spray column,
  // a lip of the face the hero walks behind); drawReliefSorted draws entry idx when its turn comes (Drawable kind 11)
  static void reliefSorted(View& v, Game& g, Vec2 cam, const std::function<void(float, int)>& push);
  static void drawReliefSorted(View& v, Game& g, int idx, Vec2 cam);
  static void drawReliefOver(View& v, Game& g, Vec2 cam);      // world space after the weather, before the sea's Mist: cloud shadows, haze
};

// M12 Rise and Fall (VISION_PLAN 13 M12 "realms that rise and fall around him, and a place in that history",
//      15.33). The VIEW lane's block (rpg/view/render_rise.cpp + its own new files; the state lives in the opaque
//      RiseViewState, View's internals through the friend RiseViewOps). Ev::Rise moments (rpg/sim/rise.h: enlisted,
//      promoted, paid, orders, a lordship, tribute, independence, a warlord or a world boss near) shown as a herald's
//      banner with the realm's arms and a fanfare; the rank's insignia and the fief's state on the HUD. The journal's
//      fourth section, STANDING (journalSection 3; realm_hud.cpp draws it), shows the army served, the rank and merit,
//      orders and missions, the holdings, the coffers, tribute and the player's realm, and the world's history.
struct RiseLane {
  using RiseViewState = View::RiseViewState; using RiseViewStats = View::RiseViewStats;   // the lane's own types, as the bodies name them
  static void riseEvent(View& v, Game& g, const Event& e);     // Ev::Rise (render.cpp's event pass)
  static void riseUpdate(View& v, Game& g, float dt);          // update, after reliefUpdate
  static void drawRiseHud(View& v, Game& g);                   // screen space (drawHud, after drawSeaHud): the herald's banner, insignia
  // (phase B, VIEW lane: rise_world.cpp) the rise in the world: the lord's standard (Prop::LordBanner) in its owner's
  // arms, the holdings' works, warlords' camps and seats, resettled places, boss-raided towns
  static const Tex* riseBannerTex(View& v, Game& g, int tx, int ty);    // a LordBanner's frame sheet in the tile's owner's arms (nullptr: plain)
  static void drawRiseGround(View& v, Game& g, Vec2 cam);      // world space, flat (after the home flats): a beast's claw scars
  // the sorted pass: push(y, idx) for every standing rise piece in view; drawRiseSorted draws one (Drawable kind 12)
  static void riseSorted(View& v, Game& g, Vec2 cam, const std::function<void(float, int)>& push);
  static void drawRiseSorted(View& v, Game& g, int idx, Vec2 cam);
  static void riseLights(View& v, Game& g, const std::function<void(Vec2, float, Color, float)>& light);   // drawLighting: tower lanterns
};
