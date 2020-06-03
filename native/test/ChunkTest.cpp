#include <gtest/gtest.h>

#include "Chunk.h"

TEST(ChunkTest, generateTerrain) {
  godot::Chunk chunk;
  chunk.build_terrain();
}
