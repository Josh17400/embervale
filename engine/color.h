// Float RGBA color shared by the renderers.
#pragma once

struct Color {
  float r = 1, g = 1, b = 1, a = 1;
  Color() = default;
  Color(float r_, float g_, float b_, float a_ = 1) : r(r_), g(g_), b(b_), a(a_) {}
  Color withA(float na) const { return {r, g, b, na}; }
  Color scaled(float s) const { return {r * s, g * s, b * s, a}; }
};
