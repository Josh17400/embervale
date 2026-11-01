// WeatherView (SOLID 4.2, View part: M3c LIFE weather), a value member of View (View::weather_): the land's weather
// round the player, the scripts' forced sky, the ambient bed's level, the eco underfoot and the first-visit biome
// banner. Bodies: rpg/view/render.cpp (update and draw), hud.cpp (the banner's line).
#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>
#include "engine/pix.h"
#include "rpg/art.h"
#include "rpg/sim/game.h"

class View;

struct WeatherView {
  // (M3c LIFE) the weather of the land round the player (render.cpp drawWeather: what the eco's Sky makes of the day
  // and hour, averaged over a few tiles round the player and eased, so it fades across a biome border), the land's
  // ambient bed and music mood (update), and the first-visit biome banner (hud.cpp)
  struct Wx {
    float rain = 0, pour = 0, drizzle = 0, fog = 0, snow = 0, blizz = 0, sand = 0, shimmer = 0, ash = 0, eerie = 0;
    float er = 0.7f, eg = 0.9f, eb = 1.0f;   // the eerie motes' and haze's colour
  };
  Wx wx_;
  bool wxInit_ = false;
  int skyOverride_ = -1;
  float wxSampleT_ = 0;
  Wx wxWant_;
  float ambLevel_ = 0;
  int hereEco_ = -1;            // the eco under the player (outdoors), -1 unknown
  float ecoT_ = 0;
  std::string biomeBanner_;     // "THE SAVANNA": shown once per biome per adventure (Game::marks, Mk::Biome)
  float biomeBannerT_ = 0;
  int biomeBannerEco_ = -1;     // (M3c fixer) the eco the banner names (-1: none)
  void updateBiomeBanner(View& v, Game& g, float dt, bool outdoors);
  void updateWeather(Game& g, float dt, bool outdoors);
  void updateAmbience(View& v, Game& g, bool outdoors);
  void drawWeather(View& v, Game& g, float dt);
  float biomeBannerY(const View& v, float half) const;   // (M9 fixer) the region banner's text line: below a boss bar, clear of the quick bar
  void drawWeatherSky(View& v, Game& g);    // drawWeather's sky (the world layer)
};
