#ifndef VOXEL_H_
#define VOXEL_H_

#include <Godot.hpp>
#include <Material.hpp>
#include <MeshInstance.hpp>
#include <StaticBody.hpp>
#include <unordered_map>

#include <Semaphore.hpp>
#include <Mutex.hpp>
#include <Thread.hpp>

#include "Chunk.h"

#include <vector>

namespace godot {

class Terrain : public Spatial {
  GODOT_CLASS(Terrain, Spatial)

  struct ChunkCoord {
    int64_t x, y, z;

    bool operator==(const ChunkCoord &other) const {
      return x == other.x && y == other.y && z == other.z;
    }
  };

  struct ChunkCoordHash {
    size_t operator()(const ChunkCoord &c) const { return c.x ^ c.y ^ c.z; }
  };

 public:
  static void _register_methods();

  Terrain();
  virtual ~Terrain();

  void _init();
  void _ready();
  void _process(float delta);

 private:

  std::unordered_map<ChunkCoord, Chunk *, ChunkCoordHash> _chunks;

  void unload_chunk(int64_t x, int64_t y, int64_t z);

  void load_chunk(int64_t x, int64_t y, int64_t z);
  void load_chunk_sequential(int64_t x, int64_t y, int64_t z);

  void process_chunks();

  /**
   * @brief Grabs a chunk from the chunk pool if one is available. Otherwise
   * generates a new chunk
   */
  Chunk *acquire_chunk();

  NodePath _player_path;
  Spatial *_player;

  std::vector<Chunk*> _chunks_to_load;
  std::vector<Thread*> _worker_threads;

  std::vector<Chunk*> _loaded_chunks;

  Semaphore *_available_chunks;
  Mutex *_chunks_to_load_mutex;
  Mutex *_loaded_chunks_mutex;

  std::vector<Chunk*> _chunk_pool;
  Mutex *_chunk_pool_mutex;

  Ref<OpenSimplexNoise> _noise;

  double _chunk_size = 16;
  int64_t _loaded_radius = 4;
  size_t _chunk_num_blocks = 16;

  int64_t _floor;
  int64_t _ceiling;
};
}  // namespace godot

#endif
