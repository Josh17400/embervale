// Shared helpers for the P2a rig galleries (rig_gallery, rigarms_gallery, rigposes_gallery, holders_gallery): a v2
// atlas's clips laid out frame by frame on a board, each frame stood on a common ground line by its pivot (a 1 px gold
// tick marks the pivot), labelled; boards are written at 1x and 4x (STYLE_P2: judge at 1x and zoomed). Lead-owned (P2a
// phase A): areas ADD helpers, never change these.
#pragma once
#include <string>
#include <vector>
#include "tools/preview/preview_util.h"

namespace rigpv {

// the slice's people (STYLE_P2 19): the hero as a man (Chestnut short hair) and as a woman (Chestnut braids), S3 skin,
// the default blue tunic and brown trousers, plain shoes, a sword; the village woman in a Dress with a culture cut and a
// pattern, Wheat hair, S6 skin
inline art::HumanLook sliceHero(bool woman) {
  art::HumanLook L;
  L.skin = rgba(236, 188, 146);         // S3 #ECBC92
  L.hairColor = rgba(120, 70, 36);      // Chestnut #784624
  L.hair = woman ? art::Hair::Braids : art::Hair::Short;
  L.topColor = rgba(60, 100, 160);
  L.bottomColor = rgba(80, 64, 50);
  L.weapon = 1;
  L.body = woman ? 1 : 0;
  return L;
}
inline art::HumanLook sliceVillager() {
  art::HumanLook L;
  L.skin = rgba(160, 104, 70);          // S6 #A06846
  L.hairColor = rgba(220, 180, 100);    // Wheat #DCB464
  L.hair = art::Hair::Bun;
  L.outfit = art::Outfit::Dress;
  L.cut = 8;                            // gown (cult::Cut + 1)
  L.pattern = 2;
  L.patternColor = rgba(230, 200, 120);
  L.topColor = rgba(150, 70, 60);
  L.body = 1;
  return L;
}

// a frame of a sheet as a canvas (empty when the clip has none)
inline Canvas frameCanvas(const art::RigSheet& s, int facing, art::Clip c, int variant, int i, int& px, int& py) {
  const art::FrameRect* r = s.frame(facing, c, variant, i);
  px = py = 0;
  if (!r || r->w <= 0 || s.atlas.w <= 0) return Canvas();
  Canvas f(r->w, r->h);
  for (int y = 0; y < r->h; y++) for (int x = 0; x < r->w; x++) f.set(x, y, s.atlas.get(r->x + x, r->y + y));
  px = r->px; py = r->py;
  return f;
}

// one row: every frame of (clip, variant) in the four facings, stood on y = groundY, `pitch` px apart; returns the x
// after the last frame. A clip with no frames writes "NOT PAINTED".
inline int clipRow(Board& b, const art::RigSheet& s, art::Clip c, int variant, int x, int groundY, int pitch, const std::string& label) {
  b.text(4, groundY - 30, label);
  bool any = false;
  for (int facing = 0; facing < art::RIG_FACINGS; facing++) {
    const int n = s.count(facing, c, variant);
    for (int i = 0; i < n; i++) {
      int px, py;
      const Canvas f = frameCanvas(s, facing, c, variant, i, px, py);
      if (f.w <= 0) continue;
      b.put(f, x + pitch / 2 - px, groundY - py);
      b.c.set(x + pitch / 2, groundY + 1, rgba(242, 193, 78));   // the pivot (Ember Gold)
      x += pitch;
      any = true;
    }
    x += pitch / 3;
  }
  if (!any) b.text(x, groundY - 10, "NOT PAINTED", rgba(232, 96, 60));
  return x;
}

inline void saveBoth(const Board& b, const std::string& dir, const std::string& name) {
  savePng(b.c, dir + "/" + name + "_1x.png", 1);
  savePng(b.c, dir + "/" + name + ".png", 4);
}

}  // namespace rigpv
