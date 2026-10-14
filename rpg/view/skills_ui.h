// M9 "Arcana & Mastery" (UI lane): the state and the friend functions shared by the UI lane's files
// (rpg/view/skills_ui.cpp: the SKILLS page and the skill toasts; rpg/view/spellbook_ui.cpp: the SPELLS page;
// rpg/view/skills_hud.cpp: the quick-cast buttons, the desktop quick bar, the sneak eye and the cast input;
// rpg/view/script_skillsui.cpp: the `skillsui` script commands). hud.cpp calls the HUD hooks below.
#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include "rpg/sim/game.h"
#include "rpg/sim/skills.h"
#include "rpg/view/view.h"

struct View::SkillsUiState {
  // ---- the SKILLS page
  int group = 0;                // the group strip's chip (skills::Group): always the group of `sel`
  int sel = 0;                  // the skill in focus (skills::Skill index)
  int groupSel[8] = {-1, -1, -1, -1, -1, -1, -1, -1};   // the skill last in focus in each group
  int perkSel = -1;             // its perk in focus (-1: the first ready one, else the root)
  int perkFor = -1;             // the skill perkSel belongs to
  std::string why;              // the last refusal (TAKE on a blocked perk, PREPARE refused) and when it was said (t)
  float whyT = -100;
  int whyPage = 0;              // 1 skills, 2 spells
  int whySkill = -1, whyPerk = -1;   // (the skills page: the perk it was said of)
  float takenT = -100;          // a perk just taken: its star bursts (t_ when)
  int takenSkill = -1, takenIdx = -1;
  // ---- the SPELLS page
  int slotSel = 0;              // the quick slot in focus
  uint32_t spellSel = 0;        // the spell in focus (0: the first known)
  int spellScroll = 0;
  float preparedT = -100;       // a slot just filled (its card flashes)
  int preparedSlot = -1;
  // ---- the quick-cast input (View::input reads it every frame)
  int8_t pressSlot = -1;        // a slot pressed since the last input (castSlot, one step)
  int holdSlot = -1;            // the slot whose key / button is held (castHold), -1 none
  bool holdTouch = false;       // held by a finger (holdFinger) rather than a key
  uint64_t holdFinger = 0;
  bool holdDual = false;        // a swipe off the button (or the dual modifier) made it a dual-cast
  float holdT = 0;              // t_ when the hold began (the charge ring on the button)
  bool sneakPress = false;      // the sneak toggle pressed (Input::sneak, one step)
  bool sneakWant = false;       // ... and the state it asks for: sent again until a step takes it (a step spent on a
  float sneakT = -100;          //     cast skips the toggle), for half a second at most
  // what the input sent (scripts: `expect skillsui input ...`): presses per slot, the longest hold, dual releases
  int sentPress[4] = {};
  float sentHoldMax = 0;
  int sentDual = 0, sentSneak = 0, sentRelease = 0;
  std::string lastToast;        // the newest skill toast's words
  // ---- toasts: the HUD glow of the latest level / mastery (t_ when, the skill)
  float glowT = -100;
  int glowSkill = -1;
  bool glowMaster = false;
  // ---- baked textures (skies by state, spell icons by id, the eye, the sparkle)
  std::unordered_map<uint64_t, Tex> tex;
  std::unordered_map<uint64_t, float> skyUsed;
  // scripts: what the pages drew last
  int perksDrawn = 0, starsReady = 0, slotsDrawn = 0, quickDrawn = 0, toastsSkill = 0;
};

// touch: the four quick-cast buttons around the attack button and the sneak button (logical screen coordinates,
// anchored to the safe area's bottom-right like hud.cpp's btnPos)
struct QuickBtn { float x, y, r; };
QuickBtn quickBtnPos(int i);    // 0..3 the slots, 4 the sneak toggle

struct SkillsUiOps {
  static View::SkillsUiState& st(View& v) {
    if (!v.skillsUi_) v.skillsUi_ = std::make_shared<View::SkillsUiState>();
    return *v.skillsUi_;
  }
  static Pix& pix(View& v) { return *v.pix_; }
  static float now(const View& v) { return v.t_; }
  static bool touch(const View& v) { return v.touchUI; }
  static Audio& audio(View& v) { return *v.audio_; }
  static const Tex& light(View& v) { return v.light_; }
  static void button(View& v, float x, float y, float w, float h, const std::string& label, bool hot) { v.button(x, y, w, h, label, hot); }
  static void arrow(View& v, float x, float y, int dir) { v.cmpArrow(x, y, dir); }
  static const Tex& itemTex(View& v, const Game& g, const Item& it) { return v.itemTex(g, it); }
  static int& menuTab(View& v) { return v.menuTab_; }
  static std::vector<View::Finger>& fingers(View& v) { return v.fingers_; }
  // a baked texture of the lane's cache (paint once per key)
  template <class F> static const Tex& tex(View& v, uint64_t key, F paint) {
    View::SkillsUiState& S = st(v);
    auto it = S.tex.find(key);
    if (it != S.tex.end()) return it->second;
    return S.tex[key] = v.pix_->bake(paint());
  }
  static const Tex& spellIcon(View& v, uint32_t spellId);
  static void sayWhy(View& v, int page, const std::string& why);

  // ---- the HUD (skills_hud.cpp; hud.cpp calls these)
  static void drawQuickButtons(View& v, Game& g);           // touch: the four slots and the sneak button
  static float drawQuickBar(View& v, Game& g, float x, float y);   // desktop: 1 [icon] 2 [icon] ...; returns its right end
  static void drawSneakEye(View& v, Game& g, float x, float y);
  static int quickAt(View& v, const Game& g, Vec2 p);       // touch: 0..3 a slot, 4 sneak, -1 none
  // a finger went down on quick button b (true: taken); moved; the keys 1-4 (dual: the modifier held)
  static bool quickDown(View& v, Game& g, int b, uint64_t finger, Vec2 p);
  static void quickMove(View& v, uint64_t finger, Vec2 p);
  static void slotKey(View& v, Game& g, int slot, bool dual);
  // View::input: the quick-cast fields of this frame's Input (keyHeld: the slot keys physically or script-held)
  static void fillInput(View& v, Game& g, Input& in, const bool keyHeld[4], bool dualMod, float dt);
  static void sneakKey(View& v, const Game& g) { View::SkillsUiState& S = st(v); S.sneakPress = true; S.sneakWant = !g.skills.live.sneaking; S.sneakT = v.t_; }
  // the menu's perk-point badge (the hamburger, the TAB hint, the SKILLS tab)
  static void drawBadge(View& v, float x, float y, int n);
  // a skill toast's decoration (its star and the level / mastery glow) at the toast plate (x, y: the text's top-left)
  static void toastDecor(View& v, const Toast& t, float x, float y, float w, float a);
  static bool isSkillToast(const Toast& t) { return t.rarity <= -100; }
  // scripts (script_skillsui.cpp): the menu box's origin on screen (the pages lay out inside it), the page keys / taps
  static void boxOf(View& v, const Game& g, int& x, int& y, int& w, int& h) { const View::UiBox b = v.modalBox(g); x = b.x; y = b.y; w = b.w; h = b.h; }
  static void pageKey(View& v, Game& g, int key) { if (v.menuTab_ == View::kSkillsTab) v.skillsPageKey(g, key); else if (v.menuTab_ == View::kSpellsTab) v.spellsPageKey(g, key); }
};
