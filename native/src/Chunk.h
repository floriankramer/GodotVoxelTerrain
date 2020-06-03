#pragma once

#include <ArrayMesh.hpp>
#include <CollisionShape.hpp>
#include <Godot.hpp>
#include <Material.hpp>
#include <MeshInstance.hpp>
#include <Mutex.hpp>
#include <OpenSimplexNoise.hpp>
#include <StaticBody.hpp>
#include <vector>

namespace godot {
class Chunk : public StaticBody {
  GODOT_CLASS(Chunk, StaticBody)

  struct MeshData {
    PoolVector3Array vertices;
    PoolVector3Array normals;
    PoolVector2Array uvs;
    PoolIntArray indices;

    size_t data_index;
    size_t indices_index;
  };

 public:
  enum class State { UNUSED, BUILDING, ACTIVE };

  static void _register_methods();

  Chunk();
  virtual ~Chunk();

  void _init();
  void _ready();
  void _enter_tree();
  void _process(float delta);

  void set_rebuild_terrain(bool b);
  bool get_rebuild_terrain() const;

  void set_noise(Ref<OpenSimplexNoise> noise);

  void build_terrain();
  void update_tree();

  void lock();
  void unlock();

  Vector3 position;
  bool empty;

  void set_size(size_t size);
  void set_world_size(double world_size);

  State get_state();
  void set_state(State s);

 private:
  std::vector<bool>::reference voxel(size_t x, size_t y, size_t z);

  static void create_top_face(double x, double y, double z, double size,
                              MeshData *data);

  static void create_bottom_face(double x, double y, double z, double size,
                                 MeshData *data);

  static void create_left_face(double x, double y, double z, double size,
                               MeshData *data);

  static void create_right_face(double x, double y, double z, double size,
                                MeshData *data);

  static void create_front_face(double x, double y, double z, double size,
                                MeshData *data);

  static void create_back_face(double x, double y, double z, double size,
                               MeshData *data);

  /**
   * @brief Returns the voxel in this chunk or false if the coordinates are
   * outside the chunk
   */
  bool voxel_or_false(int64_t x, int64_t y, int64_t z);

  Ref<OpenSimplexNoise> _noise;

  /**
   * @brief The extend of the chunk in world space. Chunks are cubes.
   */
  double _world_size;

  /**
   * @brief How many additional vertecies are added on each axis.
   */
  size_t _size;

  std::vector<bool> _voxels;

  MeshInstance *_mesh_instance;
  Ref<ArrayMesh> _mesh;
  Ref<Shape> _collision_shape;
  int _collision_shape_owner;
  int _shape_owner;
  Object *_shape_owner_dummy;

  MeshData _mesh_data;

  Mutex *_lock;
  Mutex *_state_lock;
  State _state;
};
}  // namespace godot
