// Kingdom heraldry (M1, VISION_PLAN 15.8): the eight charges a kingdom's banner can carry, as small pixel bitmaps the
// banner, gatehouse and building painters share. Include after rpg/art/art_internal.h (it uses Ramp and Canvas).
//   emblem: 0 crown, 1 sword, 2 tower, 3 star, 4 tree, 5 sun, 6 crescent moon, 7 eagle
#pragma once
#include <cstdint>
#include "engine/pix.h"

namespace art {
namespace heraldry {

// 7 x 7 charges for standing banners and the gatehouse's arms ('x' = charge)
inline const char* const* charge7(int emblem) {
  static const char* const k[8][7] = {
      {".......", "x..x..x", "xx.x.xx", "xxxxxxx", "xxxxxxx", ".xxxxx.", "......."},   // crown
      {"...x...", "...x...", "...x...", "...x...", ".xxxxx.", "...x...", "..xxx.."},   // sword
      {"x.x.x.x", "xxxxxxx", ".xxxxx.", ".xx.xx.", ".xxxxx.", ".xx.xx.", "xxxxxxx"},   // tower
      {"...x...", ".x.x.x.", "..xxx..", "xxxxxxx", "..xxx..", ".x.x.x.", "...x..."},   // star
      {"..xxx..", ".xxxxx.", "xxxxxxx", ".xxxxx.", "...x...", "...x...", "..xxx.."},   // tree
      {"x..x..x", ".xxxxx.", ".xxxxx.", "xxxxxxx", ".xxxxx.", ".xxxxx.", "x..x..x"},   // sun
      {"..xxx..", ".xx....", "xx.....", "xx.....", "xx.....", ".xx....", "..xxx.."},   // crescent
      {"x.....x", "xx.x.xx", ".xxxxx.", "..xxx..", "...x...", "..x.x..", "......."},   // eagle
  };
  return k[((emblem % 8) + 8) % 8];
}
// 5 x 5 charges for the small banners on towers and facades
inline const char* const* charge5(int emblem) {
  static const char* const k[8][5] = {
      {".....", "x.x.x", "xxxxx", "xxxxx", "....."},   // crown
      {"..x..", "..x..", ".xxx.", "..x..", "..x.."},   // sword
      {"x.x.x", "xxxxx", ".xxx.", ".x.x.", ".xxx."},   // tower
      {"..x..", ".xxx.", "xxxxx", ".xxx.", "..x.."},   // star
      {".xxx.", "xxxxx", ".xxx.", "..x..", ".xxx."},   // tree
      {"x.x.x", ".xxx.", "xxxxx", ".xxx.", "x.x.x"},   // sun
      {".xxx.", "xx...", "x....", "xx...", ".xxx."},   // crescent
      {"x...x", "xx.xx", ".xxx.", "..x..", ".x.x."},   // eagle
  };
  return k[((emblem % 8) + 8) % 8];
}
inline bool chargeAt(int emblem, int size, int i, int j) {
  if (i < 0 || j < 0 || i >= size || j >= size) return false;
  return (size == 7 ? charge7(emblem) : charge5(emblem))[j][i] == 'x';
}
// the shade of a charge pixel: lit on its top-left edges, shaded on its bottom-right (ramp index)
inline int chargeShade(int emblem, int size, int i, int j) {
  bool up = !chargeAt(emblem, size, i, j - 1), left = !chargeAt(emblem, size, i - 1, j);
  bool down = !chargeAt(emblem, size, i, j + 1), right = !chargeAt(emblem, size, i + 1, j);
  if (up || left) return 4;
  if (down || right) return 2;
  return 3;
}

}  // namespace heraldry
}  // namespace art
