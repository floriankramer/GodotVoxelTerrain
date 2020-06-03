#include "HeightMap.h"

#include <OpenSimplexNoise.hpp>

#include "Utils.h"

namespace godot {
HeightMap::HeightMap(size_t width, size_t height, double cell_size,
                     double depth)
    : _width(width + 2),
      _height(height + 2),
      _cell_size(cell_size),
      _depth(depth) {
  genIsland();
  computeDerivative();
}

double HeightMap::height(size_t x, size_t y) {
  // The grid of heights has a border of size 1 to allow for computing
  // well defined derivatives for border points
  return _heights[x + 1 + (y + 1) * _width];
}

Vector2 HeightMap::derivative(size_t x, size_t y) {
  return _derivatives[x + y * (_width - 2)];
}

void HeightMap::genIsland() {
  Godot::print(("Generating an island map of size " + std::to_string(_width) +
                " " + std::to_string(_height))
                   .c_str());
  Ref<OpenSimplexNoise> noise = OpenSimplexNoise::_new();

  double half_size_h = _width * _cell_size * 0.5;
  double half_size_w = _height * _cell_size * 0.5;
  _heights.resize(_width * _height);
  for (size_t y = 0; y < _height; ++y) {
    double y_world = y * _cell_size - half_size_h;
    for (size_t x = 0; x < _width; ++x) {
      double x_world = x * _cell_size - half_size_w;
      float falloff =
          1 - min(Vector2(x_world, y_world).length() / half_size_h, 1.0);
      // mix two levels of noise
      float height =
          falloff * (_depth / 2 + noise->get_noise_2d(x, y) * _depth +
                     noise->get_noise_2d(x * 2, y * 2) * _depth / 2);
      _heights.set(y * _width + x, height);
    }
  }
}

void HeightMap::computeDerivative() {
  Godot::print("Computing the heightmaps derivative.");
  _derivatives.resize((_width - 2) * (_height - 2));
  for (size_t y = 0; y < _height - 2; ++y) {
    for (size_t x = 0; x < _width - 2; ++x) {
      // partial derivate w.r.p. x
      double dx_left = (_heights[(x + 1) + (y + 1) * _width] -
                        _heights[(x + 0) + (y + 1) * _width]) /
                       _cell_size;
      double dx_right = (_heights[(x + 2) + (y + 1) * _width] -
                         _heights[(x + 1) + (y + 1) * _width]) /
                        _cell_size;
      double dx = (dx_left + dx_right) * 0.5;

      // partial derivate w.r.p. y
      double dy_top = (_heights[(x + 1) + (y + 1) * _width] -
                       _heights[(x + 1) + (y + 0) * _width]) /
                      _cell_size;
      double dy_bottom = (_heights[(x + 1) + (y + 2) * _width] -
                          _heights[(x + 1) + (y + 1) * _width]) /
                         _cell_size;
      double dy = (dy_top + dy_bottom) * 0.5;
      _derivatives.set(x + y * (_width - 2), Vector2(dx, dy));
    }
  }
}
}  // namespace godot
