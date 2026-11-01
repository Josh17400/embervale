// input.cpp: all input: the held keys (View::input), menu keys (menuKey), touch and mouse taps (tap) and the SDL
// event pump (View::event: keyboard, mouse, fingers). Layout shared with drawing is in hud_layout.h.
// Moved from hud.cpp (SOLID 1.4).
#include "rpg/view/hud_layout.h"
#include <SDL3/SDL.h>

using namespace hudlay;

// ------------------------------------------------------------------ input
Input View::input(Game& g) {
  Input in;
  const bool* ks = SDL_GetKeyboardState(nullptr);
  auto k = [&](SDL_Scancode sc) { return ks[sc] || scriptKeys_[sc]; };   // real keyboard or a test script's held key
  if (g.mode == Mode::Play) {
    if (k(SDL_SCANCODE_A) || k(SDL_SCANCODE_LEFT)) in.move.x -= 1;
    if (k(SDL_SCANCODE_D) || k(SDL_SCANCODE_RIGHT)) in.move.x += 1;
    if (k(SDL_SCANCODE_W) || k(SDL_SCANCODE_UP)) in.move.y -= 1;
    if (k(SDL_SCANCODE_S) || k(SDL_SCANCODE_DOWN)) in.move.y += 1;
    if (stick_.on) {
      Vec2 d = stick_.cur - stick_.start;
      float l = len(d);
      if (l > 4) in.move = d * (1.0f / l) * std::min(1.0f, l / 18.0f);
      if (l > 34) stick_.start += d * ((l - 34) / l);
    }
    // gamepad-like hold: attack repeats while held on keyboard
    if (k(SDL_SCANCODE_J) || k(SDL_SCANCODE_SPACE)) in.attack = true;
    // a finger held on the button swings again only while it reads ATTACK: when it shows a use verb (TALK, SLEEP,
    // OPEN, PRAY...) the tap is a use, and a held finger must not turn the next frame back into a swing
    for (auto& f : fingers_) {
      int hx = 0, hy = 0;
      int pidx = -1;
      if (f.on && f.button == B_ATTACK && !f.use && g.interactTarget() < 0 && !usablePropAt(g, hx, hy) && useVerbHere(g).empty() && !partyUseHere(g, pidx)) in.attack = true;
    }
  }
  if (kAttack_) { in.attack = true; }
  in.bow = kBow_; in.spell = kSpell_; in.roll = kRoll_; in.interact = kUse_; in.potion = kPotion_; in.swapSpell = kSwap_;
  int utx = 0, uty = 0, pidx = -1;
  // the attack key/button talks when someone is in reach; a tap on the world far from them stays an attack
  // (M8) and binds a downed companion's wounds or visits a grave
  if (in.attack && g.mode == Mode::Play && kAttack_ && !kTapAttack_ &&
      (partyUseHere(g, pidx) || g.interactTarget() >= 0 || usablePropAt(g, utx, uty) || !useVerbHere(g).empty())) { in.attack = false; in.interact = true; }
  kAttack_ = kBow_ = kSpell_ = kRoll_ = kUse_ = kPotion_ = kSwap_ = kTapAttack_ = false;
  // (M9) the quick-cast slots and the sneak toggle (skills_hud.cpp): castSlot for the press, castHold while held,
  // castDual for a swipe off the button or the modifier
  {
    View::SkillsUiState& Q = SkillsUiOps::st(*this);
    if (g.mode == Mode::Play) {
      const bool held[4] = {k(SDL_SCANCODE_1), k(SDL_SCANCODE_2), k(SDL_SCANCODE_3), k(SDL_SCANCODE_4)};
      const bool dualMod = k(SDL_SCANCODE_LALT) || k(SDL_SCANCODE_RALT) || k(SDL_SCANCODE_Z);
      SkillsUiOps::fillInput(*this, g, in, held, dualMod, 0);
      seaFillInput(g, in);   // (M10) the fishing reel (render_sea.cpp)
    } else {
      Q.pressSlot = -1; Q.holdSlot = -1; Q.holdDual = false; Q.sneakPress = false;
    }
  }
  // (M8) while the companions' radial is open the keyboard is its own (the stick still steers)
  if (radial_.open && g.mode == Mode::Play) {
    Input keep;
    if (stick_.on) keep.move = in.move;
    in = keep;
  }
  return in;
}

void View::menuKey(Game& g, int key) {
  auto back = [&]() { g.mode = Mode::Play; audio_->play(Sfx::MenuBack); };
  if (g.mode == Mode::Forge) { forgeKey(g, key); return; }   // M6 (craft_ui.cpp)
  if (g.mode == Mode::Build) { buildKey(g, key); return; }   // M7 (buildmode.cpp)
  if (g.mode == Mode::Menu || g.mode == Mode::LevelUp) {
    g_partyTab = partyTabShown(g);
    if (key == SDLK_ESCAPE || key == SDLK_TAB || key == SDLK_I) { back(); return; }
    // (M1) on the MAP tab Q / E zoom (worldMapKey); PageUp / PageDown and A / D still switch tabs there
    const bool mapQE = menuTab_ == 2 && g.mode == Mode::Menu && (key == SDLK_Q || key == SDLK_E);
    if (!mapQE && (key == SDLK_Q || key == SDLK_PAGEUP)) { menuTab_ = tabStep(menuTab_, -1); menuSel_ = menuTab_ == 1 ? journalRowOf(g) : 0; audio_->play(Sfx::MenuMove); return; }
    if (!mapQE && (key == SDLK_E || key == SDLK_PAGEDOWN)) { menuTab_ = tabStep(menuTab_, 1); menuSel_ = menuTab_ == 1 ? journalRowOf(g) : 0; audio_->play(Sfx::MenuMove); return; }
    if (menuTab_ == 5 && g.mode == Mode::Menu) { paperdollKey(g, key); return; }
    if (menuTab_ == kPartyTab && g.mode == Mode::Menu) { partyPageKey(g, key); return; }   // (M8) paperdoll.cpp
    if (menuTab_ == kSkillsTab && g.mode == Mode::Menu) { skillsPageKey(g, key); return; }   // (M9) skills_ui.cpp
    if (menuTab_ == kSpellsTab && g.mode == Mode::Menu) { spellsPageKey(g, key); return; }   // (M9) spellbook_ui.cpp
    if (menuTab_ == 3 && g.perkPts > 0) {
      if (key == SDLK_UP || key == SDLK_W) levelSel_ = (levelSel_ + 2) % 3;
      if (key == SDLK_DOWN || key == SDLK_S) levelSel_ = (levelSel_ + 1) % 3;
      if (key == SDLK_RETURN || key == SDLK_SPACE) g.chooseLevelUp(levelSel_);
      return;
    }
    if (menuTab_ == 2 && mapTabKey(g, key)) return;   // worldmap.cpp
    if (menuTab_ == 1 && key == SDLK_N) { setJournalSection((journalSec_ + 1) % 4); audio_->play(Sfx::MenuMove); return; }   // (M4) the journal's sections
    if (menuTab_ == 1 && journalSec_ != 0 && (key == SDLK_RETURN || key == SDLK_SPACE)) return;
    if (key == SDLK_UP || key == SDLK_W) { menuSel_ = std::max(0, menuSel_ - 1); audio_->play(Sfx::MenuMove); }
    if (key == SDLK_DOWN || key == SDLK_S) { menuSel_++; audio_->play(Sfx::MenuMove); }
    if (key == SDLK_LEFT || key == SDLK_A) { menuTab_ = tabStep(menuTab_, -1); menuSel_ = menuTab_ == 1 ? journalRowOf(g) : 0; }
    if (key == SDLK_RIGHT || key == SDLK_D) { menuTab_ = tabStep(menuTab_, 1); menuSel_ = menuTab_ == 1 ? journalRowOf(g) : 0; }
    if (key == SDLK_RETURN || key == SDLK_SPACE) {
      if (menuTab_ == 0 && menuSel_ < (int)g.inv.size()) packUse(g, menuSel_);
      if (menuTab_ == 1) {   // (P1) track the picked quest (a lead row has nothing to track)
        const std::vector<JRow> rows = journalRows(g);
        if (menuSel_ < (int)rows.size() && rows[(size_t)menuSel_].q >= 0 && g.quests[(size_t)rows[(size_t)menuSel_].q].state != QState::Done)
          g.trackedQuest = g.quests[(size_t)rows[(size_t)menuSel_].q].id;
      }
      if (menuTab_ == 4) {
        if (menuSel_ == 0) { wantSave = true; banner_ = "GAME SAVED"; bannerSub_ = ""; bannerState_.clear(); bannerT_ = 2; }
        if (menuSel_ == 1) { openSettings(); audio_->play(Sfx::MenuSelect); }
        if (menuSel_ == 2) touchUI = !touchUI;
        if (menuSel_ == 3) { wantSave = true; wantsQuit = true; }
      }
    }
    if (key == SDLK_X && menuTab_ == 0 && menuSel_ < (int)g.inv.size() && g.inv[menuSel_].kind != ItemKind::Quest) g.dropItem(menuSel_);
    return;
  }
  if (g.mode == Mode::Dialogue) {
    const std::vector<int> shown = dlgShown(g.dlg);
    const int nOpt = (int)shown.size();
    if (key == SDLK_UP || key == SDLK_W) { dlgSel_ = std::max(0, dlgSel_ - 1); audio_->play(Sfx::MenuMove); }
    if (key == SDLK_DOWN || key == SDLK_S) { dlgSel_ = std::min(std::max(0, nOpt - 1), dlgSel_ + 1); audio_->play(Sfx::MenuMove); }
    if (key == SDLK_UP || key == SDLK_W || key == SDLK_DOWN || key == SDLK_S) {   // (M5) the page follows the selection
      const UiBox box = uiBox(300);
      pix_->pushBox(box.x, box.y, box.w, box.h);
      dlgFirst_ = dlgPageFirst(g, dlgRowH(), dlgSel_);
      pix_->popBox();
    }
    if (key == SDLK_RETURN || key == SDLK_SPACE || key == SDLK_E || key == SDLK_J) {
      int total = 0;
      {   // the same wrap as drawDialogue (its box)
        const UiBox box = uiBox(300);
        pix_->pushBox(box.x, box.y, box.w, box.h);
        total = dlgLay(g, dlgRowH()).total;
        pix_->popBox();
      }
      if (dlgChars_ < total) { dlgChars_ = 9999; return; }
      const int sel = dlgSel_ >= 0 && dlgSel_ < nOpt ? shown[(size_t)dlgSel_] : -1;
      std::string before = g.dlg.text;
      g.dialogueChoose(sel);
      if (g.dlg.text != before) dlgChars_ = 0;
      dlgSel_ = 0; dlgFirst_ = 0;
    }
    if (key == SDLK_ESCAPE) g.closeDialogue();
    return;
  }
  if (g.mode == Mode::Shop) {
    const int n = shopSide_ == 0 ? (int)g.shop.stock.size() : (int)g.inv.size();
    if (key == SDLK_UP || key == SDLK_W) { shopSel_ = std::max(0, shopSel_ - 1); shopArm_ = -1; }
    if (key == SDLK_DOWN || key == SDLK_S) { shopSel_ = std::min(std::max(0, n - 1), shopSel_ + 1); shopArm_ = -1; }
    if (key == SDLK_LEFT || key == SDLK_A || key == SDLK_RIGHT || key == SDLK_D) { shopSide_ ^= 1; shopSel_ = 0; shopArm_ = -1; }
    if (key == SDLK_RETURN || key == SDLK_SPACE) shopDeal(g);
    if (key == SDLK_ESCAPE || key == SDLK_TAB) { g.mode = Mode::Play; audio_->play(Sfx::MenuBack); }
  }
}

void View::tap(Game& g, Vec2 p) {
  auto inR = [&](float x, float y, float w, float h) { return p.x >= x && p.y >= y && p.x < x + w && p.y < y + h; };
  if (g.mode == Mode::Title) { titleTap(p); return; }
  if (settingsOpen_) { settingsTap(p); return; }
  if (g.mode == Mode::Creator) { creatorTap(g, p); return; }
  if (g.mode == Mode::Forge) { forgeTap(g, p); return; }   // M6 (craft_ui.cpp: it maps the tap into its own box)
  if (g.mode == Mode::Build) { buildTap(g, p); return; }   // M7 (buildmode.cpp: screen coordinates; the ghost on the world)
  if (g.mode == Mode::Dead) { if (modeT_ > 1.2f) g.respawn(); return; }
  if (g.mode == Mode::Paused) { g.mode = Mode::Play; return; }
  // the dialogue, the shop and the menu are laid out in a 480-wide box (drawDialogue / drawShop / drawMenu): the tap
  // moves into it, and Pix::W / Pix::H read the box while the hit tests run
  Pix& P = *pix_;
  const UiBox box = modalBox(g);
  p = inBox(box, p);
  P.pushBox(box.x, box.y, box.w, box.h);
  struct Pop { Pix& p; ~Pop() { p.popBox(); } } pop{P};
  if (g.mode == Mode::Dialogue) {
    const DlgLay L = dlgLay(g, dlgRowH(), dlgFirst_);
    const std::vector<int> shown = dlgShown(g.dlg);
    const int nOpt = (int)shown.size();
    // while the line is still typing, a tap anywhere only finishes it (the common "tap to skip" gesture must never
    // pick an option the player has not read yet; the keyboard path in menuKey does the same)
    if (dlgChars_ < L.total) { dlgChars_ = 9999; return; }
    // (M5) the MORE row turns the page (the last row, so it reaches down to the screen edge)
    if (L.more && inR(-1000.0f, L.moreTop, Pix::W + 2000.0f, 1000.0f)) {
      dlgFirst_ = L.first + L.count >= nOpt ? 0 : L.first + L.count;
      dlgSel_ = dlgFirst_;
      audio_->play(Sfx::MenuMove);
      return;
    }
    for (int i = L.first; i < L.first + L.count && i < nOpt; i++) {
      // each option owns its whole row band, the full panel width; the last one reaches down to the screen edge
      const float top = L.optTop[(size_t)i];
      const float bh = (i == nOpt - 1 && !L.more) ? 1000.0f : L.optH[(size_t)i];   // (M1: on past the box, to the screen's real edge)
      if (inR(-1000.0f, top, Pix::W + 2000.0f, bh)) {
        std::string before = g.dlg.text;
        g.dialogueChoose(shown[(size_t)i]);
        if (g.dlg.text != before) dlgChars_ = 0;
        dlgSel_ = 0; dlgFirst_ = 0;
        return;
      }
    }
    // a tap above the panel closes a conversation that only has one way out (FAREWELL)
    if (nOpt == 1 && p.y < L.y) { g.dialogueChoose(shown[0]); dlgSel_ = 0; }
    return;
  }
  if (g.mode == Mode::Shop) {
    const ShopLay L = shopLay(touchUI);
    if (inR(L.xX - 4, 0, L.xW + 14, L.xY + L.xH + 6)) { g.mode = Mode::Play; return; }
    // the deal button (touch): a tap on a row only selects it
    if (touchUI && inR(L.btnX - 4, L.btnY - 3, L.btnW + 8, L.btnH + 6)) { shopDeal(g); return; }
    for (int side = 0; side < 2; side++) {
      float x0 = L.x0[side];
      int n = side == 0 ? (int)g.shop.stock.size() : (int)g.inv.size();
      int sel = shopSide_ == side ? shopSel_ : -1;
      const int rows = shopRows(L, touchUI, n);
      int scroll = shopScroll(sel, rows);
      if (rows < L.rows) {   // the page buttons: the selection moves a page (the list follows it)
        const float py = 40 + rows * L.pitch - 2;
        const bool up = inR(x0 - 4, py - 2, 50, L.pitch + 4), down = inR(x0 + L.lw - 50, py - 2, 50, L.pitch + 4);
        if (up || down) {
          const int base = shopSide_ == side ? shopSel_ : 0;
          shopSide_ = side; shopArm_ = -1;
          shopSel_ = up ? std::max(0, scroll - 1) : std::min(n - 1, std::max(base, scroll + rows - 1) + rows);
          audio_->play(Sfx::MenuMove);
          return;
        }
      }
      for (int r = 0; r < rows && scroll + r < n; r++) {
        float y = 40 + r * L.pitch;
        if (inR(x0 - 2, y - 2, L.lw, L.pitch)) {
          int i = scroll + r;
          if (!(shopSide_ == side && shopSel_ == i)) shopArm_ = -1;
          shopSide_ = side; shopSel_ = i;
          audio_->play(Sfx::MenuMove);
          return;
        }
      }
    }
    return;
  }
  if (g.mode == Mode::Menu || g.mode == Mode::LevelUp) {
    g_partyTab = partyTabShown(g);
    const TabLay T = tabLay(touchUI);
    // the close X first (on touch its tap box reaches a little past it, into the panel's corner)
    if (touchUI ? inR(T.xX - 2, 0, T.xW + 18, T.xY + T.xH + 6) : inR(T.xX, T.xY, T.xW, T.xH)) { g.mode = Mode::Play; return; }
    for (int i = 0; i < nTabs(); i++)
      if (touchUI ? inR(T.x0 - 2 + i * T.tw, 0, T.tw, T.y + T.h + 2) : inR(T.x0 + i * T.tw, T.y, T.tw - 4, T.h)) {
        menuTab_ = tabAt(i); menuSel_ = menuTab_ == 1 ? journalRowOf(g) : 0; menuScroll_ = 0; mapSel_ = -1; audio_->play(Sfx::MenuMove); return;
      }
    float top = 34;
    const MenuLay L = menuLay(touchUI);
    // a touch list's page buttons: a page up / down
    auto paged = [&](const Cols& C, int n) {
      if (!touchUI || n <= L.rows) return false;
      const PageBtns pb = pageBtns(C, L);
      if (inR(pb.upX, pb.y - 2, pb.w, pb.h + 4)) { menuSel_ = std::max(0, menuSel_ - L.rows); menuScroll_ = std::max(0, menuScroll_ - L.rows); audio_->play(Sfx::MenuMove); return true; }
      if (inR(pb.downX, pb.y - 2, pb.w, pb.h + 4)) {
        menuScroll_ = std::min(std::max(0, n - L.rows), menuScroll_ + L.rows);
        menuSel_ = std::min(n - 1, std::max(menuSel_ + L.rows, menuScroll_));
        audio_->play(Sfx::MenuMove);
        return true;
      }
      return false;
    };
    switch (menuTab_) {
      case 0: {
        int n = (int)g.inv.size();
        const Cols C = itemCols();
        for (int r = 0; r < L.rows && menuScroll_ + r < n; r++) {
          if (inR(C.lx, top + 10 + r * L.pitch, C.lw, L.pitch)) {
            int i = menuScroll_ + r;
            if (i == menuSel_) packUse(g, i);
            menuSel_ = i;
            return;
          }
        }
        if (paged(C, n)) return;
        const float bw = std::floor((C.dw - 20) / 2);
        if (n > 0 && inR(C.dx, L.btnY, bw, L.btnH)) packUse(g, menuSel_);   // (M8b fixer r2) an idle button says why
        if (n > 0 && inR(C.dx + bw + 8, L.btnY, bw, L.btnH) && g.inv[menuSel_].kind != ItemKind::Quest) g.dropItem(menuSel_);
        // keyboard / mouse: scroll by clicking the list edges
        if (!touchUI && inR(C.lx, top + 4, C.lw, 6)) menuScroll_ = std::max(0, menuScroll_ - 5), menuSel_ = std::max(0, menuSel_ - 5);
        if (!touchUI && inR(C.lx, top + 10 + L.rows * L.pitch, C.lw, 12)) menuSel_ = std::min(n - 1, menuSel_ + 5);
        break;
      }
      case 1: {
        if (journalStripTap(p, questCols().lx, questCols().lw, top)) return;
        if (journalSec_ != 0) { journalSectionTap(g, p, top + journalStripH()); return; }
        top += journalStripH();
        MenuLay L = menuLay(touchUI);
        L.rows = std::max(3, L.rows - (int)std::ceil(journalStripH() / L.pitch));
        const std::vector<JRow> rows = journalRows(g);   // (P1) quests, leads, finished quests
        const Cols C = questCols();
        for (int r = 0; r < L.rows && menuScroll_ + r < (int)rows.size(); r++)
          if (inR(C.lx, top + 10 + r * L.pitch, C.lw, L.pitch)) { menuSel_ = menuScroll_ + r; return; }
        if (paged(C, (int)rows.size())) return;
        // keyboard / mouse: scroll the journal by clicking just above / below the list
        if (!touchUI && inR(C.lx, top + 2, C.lw, 8)) { menuSel_ = std::max(0, menuSel_ - 5); return; }
        if (!touchUI && inR(C.lx, top + 10 + L.rows * L.pitch, C.lw, 14)) { menuSel_ = std::min((int)rows.size() - 1, menuSel_ + 5); return; }
        if (menuSel_ < (int)rows.size() && rows[(size_t)menuSel_].q >= 0 && g.quests[(size_t)rows[(size_t)menuSel_].q].state != QState::Done &&
            inR(C.dx, L.trackY, std::min(C.dw, touchUI ? 140.0f : 110.0f), L.trackH))
          g.trackedQuest = g.quests[(size_t)rows[(size_t)menuSel_].q].id;
        break;
      }
      case 2:
        mapTabTap(g, p, top);
        break;
      case 3:
        if (g.perkPts > 0) {
          const HeroLay H = heroLay(touchUI, true);
          for (int i = 0; i < 3; i++) if (inR(H.bx, H.by + i * (H.bh + 4) - 2, H.bw, H.bh + 4)) { g.chooseLevelUp(i); levelSel_ = i; }
        }
        break;
      case 5:
        paperdollTap(g, p);
        break;
      case kPartyTab:   // (M8) paperdoll.cpp
        partyPageTap(g, p);
        break;
      case kSkillsTab:  // (M9) skills_ui.cpp
        skillsPageTap(g, p);
        break;
      case kSpellsTab:  // (M9) spellbook_ui.cpp
        spellsPageTap(g, p);
        break;
      case 4: {
        const float sh = L.sysH, pitch = sh + 6;
        const float bw = std::clamp(std::floor(Pix::W * 0.36f), 140.0f, 220.0f), bx = std::floor((Pix::W - bw) / 2), by = sysTop(top, sh, touchUI);
        if (inR(bx, by, bw, sh)) { menuSel_ = 0; wantSave = true; banner_ = "GAME SAVED"; bannerSub_ = ""; bannerState_.clear(); bannerT_ = 2; }
        if (inR(bx, by + pitch, bw, sh)) { menuSel_ = 1; openSettings(); audio_->play(Sfx::MenuSelect); }
        if (inR(bx, by + pitch * 2, bw, sh)) { menuSel_ = 2; touchUI = !touchUI; }
        if (inR(bx, by + pitch * 3, bw, sh)) { wantSave = true; wantsQuit = true; }
        break;
      }
    }
    return;
  }
}

void View::event(const SDL_Event& e, Game& g) {
  auto logical = [&](float wx, float wy) {
    float lx, ly;
    pix_->windowToLogical(wx, wy, lx, ly);
    if (witness) { lx = witnessSnap(lx); ly = witnessSnap(ly); }   // (Witness) replays re-create these floats exactly
    return Vec2(lx, ly);
  };
  // the touch gestures' clock (a pinch's 0.3 s, a short tap's 0.35 s), in ms: the wall clock in normal play; under
  // Witness (a harness or a WITNESS_RECORD session) the view's own clock, which only sim steps advance, so a replay
  // sees the same gesture lengths as the recorded session
  auto gestureMs = [&]() -> uint64_t { return witness ? (uint64_t)((double)t_ * 1000.0) : SDL_GetTicks(); };
  // (M1) the MAP tab: drags pan and pinches zoom the world map; a press that does not move is a tap (picks a place)
  const bool onMap = g.mode == Mode::Menu && menuTab_ == 2 && !settingsOpen_;
  auto mapPtr = [&](int phase, uint64_t id, Vec2 screenP) {
    Pix& P = *pix_;
    const UiBox box = modalBox(g);
    P.pushBox(box.x, box.y, box.w, box.h);   // Pix::H as drawMenu sees it
    const bool tapped = worldMapPointer(phase, id, inBox(box, screenP));
    P.popBox();
    return tapped;
  };
  switch (e.type) {
    case SDL_EVENT_MOUSE_WHEEL:
      if (onMap && e.wheel.y != 0) worldMapZoom(e.wheel.y > 0 ? -1 : 1, inBox(modalBox(g), mouse_));
      else if (g.mode == Mode::Play && !settingsOpen_ && e.wheel.y != 0) {
        // (P2a) ZOOM in play: a notch steps one level (a trackpad's small deltas add up to one; a fling steps once per
        // 0.15 s, so it never races through the levels)
        camera_.wheelAcc_ += e.wheel.y;
        if (std::fabs(camera_.wheelAcc_) >= 1.0f && (t_ - camera_.wheelT_ >= 0.15f || t_ < camera_.wheelT_)) {   // (view time: a harness's steps too)
          camera_.zoomStep(*this, camera_.wheelAcc_ > 0 ? 1 : -1);
          camera_.wheelT_ = t_;
          camera_.wheelAcc_ = 0;
        }
        camera_.wheelAcc_ = std::clamp(camera_.wheelAcc_, -1.5f, 1.5f);
      }
      break;
    case SDL_EVENT_MOUSE_BUTTON_UP:
      if (g.mode == Mode::Play && e.button.which != SDL_TOUCH_MOUSEID && e.button.button == SDL_BUTTON_LEFT)
        partyPointer(g, 2, 1ull << 62, logical(e.button.x, e.button.y), true);   // (M8) a card clicked / the radial's wedge
      if (g.mode == Mode::Build && e.button.which != SDL_TOUCH_MOUSEID && e.button.button == SDL_BUTTON_LEFT) buildPointer(g, 2, logical(e.button.x, e.button.y));
      if (onMap && e.button.which != SDL_TOUCH_MOUSEID && e.button.button == SDL_BUTTON_LEFT && mapPtr(2, 1ull << 62, logical(e.button.x, e.button.y)))
        tap(g, logical(e.button.x, e.button.y));
      break;
    case SDL_EVENT_KEY_DOWN: {
      if (e.key.repeat && g.mode == Mode::Play) break;
      SDL_Keycode k = e.key.key;
      touchUI = false;
      if (settingsOpen_) { settingsKey((int)k); break; }
      if (g.mode == Mode::Title) {
        if (k == SDLK_UP || k == SDLK_W) titleSel_ = std::max(0, titleSel_ - 1);
        if (k == SDLK_DOWN || k == SDLK_S) titleSel_ = std::min(titleItems() - 1, titleSel_ + 1);
        if (k == SDLK_RETURN || k == SDLK_SPACE) {
          if (titleSel_ == titleItems() - 1) { openSettings(); audio_->play(Sfx::MenuSelect); }
          else if (hasSave_ && titleSel_ == 0) wantContinue = true;
          else wantNewGame = true;
        }
        if (k == SDLK_N) wantNewGame = true;
        if (k == SDLK_ESCAPE) wantsQuit = true;
        break;
      }
      if (g.mode == Mode::Dead) { if ((k == SDLK_RETURN || k == SDLK_SPACE || k == SDLK_E) && modeT_ > 1.2f) g.respawn(); break; }
      if (g.mode == Mode::Paused) { if (k == SDLK_ESCAPE || k == SDLK_RETURN || k == SDLK_SPACE) g.mode = Mode::Play; break; }
      if (g.mode == Mode::Creator) { creatorKey(g, (int)k); break; }
      if (g.mode != Mode::Play) { menuKey(g, k); break; }
      if (partyKey(g, (int)k)) break;     // (M8) C: the companions' orders (the radial takes every key while open)
      if (homeHudKey(g, (int)k)) break;   // (M7) B: yard / decorate, F: dismount
      switch (k) {
        case SDLK_ESCAPE: g.mode = Mode::Paused; break;
        // (M2 fixer round 2) no menu on the road: it stopped the journey mid-gather (and its SYSTEM tab saved and quit
        // with the fare paid and the hero still at the start)
        case SDLK_TAB: case SDLK_I: if (g.travelling()) break; g.mode = Mode::Menu; menuTab_ = g.perkPts > 0 ? 3 : 0; menuSel_ = 0; audio_->play(Sfx::MenuSelect); break;
        case SDLK_M: if (g.travelling()) break; g.mode = Mode::Menu; menuTab_ = 2; mapSel_ = -1; audio_->play(Sfx::MenuSelect); break;
        case SDLK_J: case SDLK_SPACE: kAttack_ = true; break;
        case SDLK_K: kBow_ = true; break;
        case SDLK_L: kSpell_ = true; break;
        case SDLK_LSHIFT: case SDLK_RSHIFT: kRoll_ = true; break;
        case SDLK_E: case SDLK_RETURN: kUse_ = true; break;
        case SDLK_Q: kPotion_ = true; break;
        case SDLK_R: kSwap_ = true; break;
        // (M9) the quick slots: 1-4 cast (held: upcast; Alt or Z held: dual-cast); Ctrl or V: sneak on / off
        case SDLK_1: case SDLK_2: case SDLK_3: case SDLK_4: {
          const bool* ks = SDL_GetKeyboardState(nullptr);
          const bool dual = (SDL_GetModState() & SDL_KMOD_ALT) || ks[SDL_SCANCODE_Z] || scriptKeys_[SDL_SCANCODE_Z] || scriptKeys_[SDL_SCANCODE_LALT];
          SkillsUiOps::slotKey(*this, g, (int)(k - SDLK_1), dual);
          break;
        }
        case SDLK_LCTRL: case SDLK_RCTRL: case SDLK_V: SkillsUiOps::sneakKey(*this, g); break;
        // (P2a) ZOOM: - out, = (or +) in, and the keypad's
        case SDLK_MINUS: case SDLK_KP_MINUS: camera_.zoomStep(*this, -1); break;
        case SDLK_EQUALS: case SDLK_PLUS: case SDLK_KP_PLUS: camera_.zoomStep(*this, 1); break;
        default: break;
      }
      break;
    }
    case SDL_EVENT_MOUSE_MOTION:
      mouse_ = logical(e.motion.x, e.motion.y);
      if (g.mode == Mode::Play && e.motion.which != SDL_TOUCH_MOUSEID) partyPointer(g, 1, 1ull << 62, mouse_, true);   // (M8) the radial's hover
      if (g.mode == Mode::Build && e.motion.which != SDL_TOUCH_MOUSEID && (e.motion.state & SDL_BUTTON_LMASK)) buildPointer(g, 1, mouse_);
      if (onMap && e.motion.which != SDL_TOUCH_MOUSEID) mapPtr(1, 1ull << 62, mouse_);
      break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN: {
      if (e.button.which == SDL_TOUCH_MOUSEID) break;
      Vec2 p = logical(e.button.x, e.button.y);
      mouse_ = p;
      if (g.mode == Mode::Title) {
        titleTap(p);
        break;
      }
      if (g.mode == Mode::Play) {
        // (M8) a click on a companion's card selects it (a second click, or a right click, opens the radial)
        if (e.button.button == SDL_BUTTON_LEFT && partyPointer(g, 0, 1ull << 62, p, true)) break;
        if (e.button.button == SDL_BUTTON_RIGHT && !radial_.open && partyPointer(g, 0, 1ull << 62, p, true)) {
          const int mi = partyPress_.member;
          partyPress_.on = false;
          radialOpen(g, mi, false, 0);
          break;
        }
        if (e.button.button == SDL_BUTTON_LEFT && homeHudTap(g, p)) break;   // (M7) the home pills
        if (e.button.button == SDL_BUTTON_LEFT) kAttack_ = true;
        else if (e.button.button == SDL_BUTTON_RIGHT) kBow_ = true;
        break;
      }
      if (onMap && e.button.button == SDL_BUTTON_LEFT) {
        const UiBox box = modalBox(g);
        const Vec2 bp = inBox(box, p);
        // on the map itself: the press may become a drag (the tap is decided on release)
        if (bp.x >= 16 && bp.x < box.w - 164 && bp.y >= 40 && bp.y < box.h - 12) { mapPtr(0, 1ull << 62, p); break; }
      }
      tap(g, p);
      break;
    }
    case SDL_EVENT_FINGER_DOWN: {
      touchUI = true;
      int ww, wh;
      SDL_GetWindowSize(pix_->window(), &ww, &wh);
      Vec2 p = logical(e.tfinger.x * ww, e.tfinger.y * wh);
      if (g.mode == Mode::Title) {
        titleTap(p);
        break;
      }
      if (onMap) {
        const UiBox box = modalBox(g);
        const Vec2 bp = inBox(box, p);
        if (bp.x >= 16 && bp.x < box.w - 164 && bp.y >= 40 && bp.y < box.h - 12) { mapPtr(0, (uint64_t)e.tfinger.fingerID, p); break; }
      }
      if (g.mode != Mode::Play) { tap(g, p); break; }
      {   // (P2a) a two-finger pinch steps ZOOM: a second finger landing within 0.3 s of a first one that has hardly moved,
          // both clear of the buttons (the first one's stick, if it made one, is let go)
        const uint64_t now = gestureMs();
        int others = 0;
        const Finger* first = nullptr;
        for (const Finger& f : fingers_) if (f.on) { others++; first = &f; }
        if (others == 1 && first && first->button == -1 && now - first->t0 < 300 && len2(first->cur - first->start) < 10.0f * 10.0f &&
            buttonAt(p) < 0 && SkillsUiOps::quickAt(*this, g, p) < 0) {
          camera_.pinch_.on = true; camera_.pinch_.a = first->id; camera_.pinch_.b = (uint64_t)e.tfinger.fingerID;
          camera_.pinch_.pa = first->cur; camera_.pinch_.pb = p; camera_.pinch_.d0 = std::max(8.0f, len(p - first->cur)); camera_.pinch_.done = 0;
          if (stick_.on && stick_.id == first->id) stick_.on = false;
          Finger f; f.id = e.tfinger.fingerID; f.on = true; f.start = f.cur = p; f.button = -3; f.t0 = now;   // -3: a pinch finger
          fingers_.push_back(f);
          break;
        }
      }
      if (partyPointer(g, 0, (uint64_t)e.tfinger.fingerID, p, false)) break;   // (M8) a companion's card: tap selects, hold orders
      if (homeHudTap(g, p)) break;   // (M7) YARD / DECORATE, DISMOUNT
      if (const int q = SkillsUiOps::quickAt(*this, g, p); q >= 0 && buttonAt(p) != B_ATTACK) {   // (M9) a quick-cast button
        SkillsUiOps::quickDown(*this, g, q, (uint64_t)e.tfinger.fingerID, p);
        Finger f; f.id = e.tfinger.fingerID; f.on = true; f.start = f.cur = p; f.button = 100 + q; f.t0 = gestureMs();
        fingers_.push_back(f);
        break;
      }
      int b = buttonAt(p);
      // (fixer M5 r3, review: "nothing explains the buff icons when tapped") a tap on the buff icons says what they do
      if (b < 0 && buffTap(g, p)) break;
      Finger f; f.id = e.tfinger.fingerID; f.on = true; f.start = f.cur = p; f.button = b; f.t0 = gestureMs();
      if (b == B_MENU) { if (!g.travelling()) { g.mode = Mode::Menu; menuTab_ = g.perkPts > 0 ? 3 : 0; menuSel_ = 0; audio_->play(Sfx::MenuSelect); } break; }
      if (b == B_ATTACK) {
        kAttack_ = true;
        // (M7 fix r3) a touch that starts while the button reads a use verb (TALK, RIDE, OPEN...) stays a use for its
        // whole hold: the held-finger repeat must not turn it into a swing once the verb is gone (mounting clears it,
        // and the swing would get the rider straight off again)
        int ux = 0, uy = 0;
        if (g.interactTarget() >= 0 || usablePropAt(g, ux, uy) || !useVerbHere(g).empty()) f.use = true;
      }
      else if (b == B_BOW) kBow_ = true;
      else if (b == B_SPELL) kSpell_ = true;
      else if (b == B_ROLL) kRoll_ = true;
      else if (b == B_POTION) kPotion_ = true;
      else if (p.x < Pix::W * 0.55f && !stick_.on) { stick_ = f; stick_.button = -2; }
      else if (b < 0) {
        // tap on the world: talk to / open what's there, otherwise attack. Talking needs the tap on (or near) the
        // person or the hero, so a tap across the screen never opens a conversation by accident
        int utx = 0, uty = 0;
        bool talk = false;
        if (int it = g.interactTarget(); it >= 0) {
          // (P2a) the bodies projected from the world layer; the finger's reach stays in UI px
          for (const Actor& a : g.actors)
            if (a.id == it && (len2(p - camera_.toUi(*this, a.p - Vec2(0, 10))) < 40.0f * 40.0f || len2(p - camera_.toUi(*this, g.pl().p - Vec2(0, 10))) < 40.0f * 40.0f)) talk = true;
        }
        if (talk || usablePropAt(g, utx, uty) || !useVerbHere(g).empty()) kUse_ = true; else { kAttack_ = true; kTapAttack_ = true; }
      }
      fingers_.push_back(f);
      break;
    }
    case SDL_EVENT_FINGER_MOTION: {
      int ww, wh;
      SDL_GetWindowSize(pix_->window(), &ww, &wh);
      Vec2 p = logical(e.tfinger.x * ww, e.tfinger.y * wh);
      if (camera_.pinch_.on && ((uint64_t)e.tfinger.fingerID == camera_.pinch_.a || (uint64_t)e.tfinger.fingerID == camera_.pinch_.b)) {
        // (P2a) the pinch: spread a third wider for a level in, close a quarter for a level out (rebased each step)
        if ((uint64_t)e.tfinger.fingerID == camera_.pinch_.a) camera_.pinch_.pa = p; else camera_.pinch_.pb = p;
        const float d = len(camera_.pinch_.pb - camera_.pinch_.pa), r = d / std::max(1.0f, camera_.pinch_.d0);
        if (g.mode == Mode::Play && (r > 1.35f || r < 0.74f)) { camera_.zoomStep(*this, r > 1 ? 1 : -1); camera_.pinch_.d0 = std::max(8.0f, d); camera_.pinch_.done++; }
      }
      if (onMap) mapPtr(1, (uint64_t)e.tfinger.fingerID, p);
      if (g.mode == Mode::Build) buildPointer(g, 1, p);   // (M7) the ghost or a list dragged
      if (g.mode == Mode::Play) partyPointer(g, 1, (uint64_t)e.tfinger.fingerID, p, false);   // (M8) the held radial
      if (stick_.on && stick_.id == e.tfinger.fingerID) stick_.cur = p;
      for (auto& f : fingers_) if (f.id == e.tfinger.fingerID) f.cur = p;
      SkillsUiOps::quickMove(*this, (uint64_t)e.tfinger.fingerID, p);   // (M9) a swipe off a quick button: dual-cast
      break;
    }
    case SDL_EVENT_FINGER_UP: case SDL_EVENT_FINGER_CANCELED: {
      if (camera_.pinch_.on && ((uint64_t)e.tfinger.fingerID == camera_.pinch_.a || (uint64_t)e.tfinger.fingerID == camera_.pinch_.b)) camera_.pinch_.on = false;   // (P2a)
      if (g.mode == Mode::Play) {   // (M8) a card let go of: a tap selects; the held radial gives the order under the finger
        int ww, wh;
        SDL_GetWindowSize(pix_->window(), &ww, &wh);
        const Vec2 p = logical(e.tfinger.x * ww, e.tfinger.y * wh);
        if (e.type == SDL_EVENT_FINGER_CANCELED) { if (radial_.drag && radial_.finger == (uint64_t)e.tfinger.fingerID) radial_.open = false; partyPress_.on = false; }
        else partyPointer(g, 2, (uint64_t)e.tfinger.fingerID, p, false);
      }
      if (g.mode == Mode::Build) {   // (M7) a drag ends; a still tap on a list row acts now
        int ww, wh;
        SDL_GetWindowSize(pix_->window(), &ww, &wh);
        buildPointer(g, 2, logical(e.tfinger.x * ww, e.tfinger.y * wh));
      }
      if (onMap) {
        int ww, wh;
        SDL_GetWindowSize(pix_->window(), &ww, &wh);
        const Vec2 p = logical(e.tfinger.x * ww, e.tfinger.y * wh);
        if (mapPtr(2, (uint64_t)e.tfinger.fingerID, p) && e.type == SDL_EVENT_FINGER_UP) tap(g, p);
      }
      if (stick_.on && stick_.id == e.tfinger.fingerID) {
        stick_.on = false;
        // M0b fix round 3: a short, still tap on the left half opens the stick, but when it lands on the "TAP: SLEEP"
        // label / the prop it names, or on the person in reach, it is a use: the label says tap here, so it must work
        if (e.type == SDL_EVENT_FINGER_UP && g.mode == Mode::Play && gestureMs() - stick_.t0 < 350 && len2(stick_.cur - stick_.start) < 8.0f * 8.0f) {
          const Vec2 q = stick_.start;
          bool use = false;
          int utx = 0, uty = 0;
          if (usablePropAt(g, utx, uty)) {
            Vec2 s = camera_.toUi(*this, Vec2(utx * 16 + 8.0f, uty * 16 - 4.0f));   // the prop's label sits at s.y - 10 (UI px)
            if (std::fabs(q.x - s.x) < 32 && q.y > s.y - 22 && q.y < s.y + 24) use = true;
          }
          if (int it = g.interactTarget(); it >= 0)
            for (const Actor& a : g.actors)
              if (a.id == it && len2(q - camera_.toUi(*this, a.p - Vec2(0, 10))) < 24.0f * 24.0f) use = true;
          if (use) kUse_ = true;
        }
      }
      for (size_t i = 0; i < fingers_.size();) if (fingers_[i].id == e.tfinger.fingerID) fingers_.erase(fingers_.begin() + i); else i++;
      break;
    }
    default: break;
  }
}
