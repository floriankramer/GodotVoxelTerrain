#ifndef HEIGHTMAP_H
#define HEIGHTMAP_H

#include <Godot.hpp>

namespace godot {
class HeightMap {
 public:
  HeightMap(size_t width, size_t height, double cell_size, double depth);

  double height(size_t x, size_t y);
  Vector2 derivative(size_t x, size_t y);

 private:
  void genIsland();
  void computeDerivative();

  size_t _width;
  size_t _height;
  double _cell_size;
  double _depth;

  PoolRealArray _heights;
  PoolVector2Array _derivatives;
};
}  // namespace godot

#endif  // HEIGHTMAP_H
