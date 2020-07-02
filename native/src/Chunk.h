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

#include <SpatialMaterial.hpp>

namespace godot {
class Chunk {

  struct MeshData {
    PoolVector3Array vertices;
    PoolVector3Array normals;
    PoolVector2Array uvs;
    PoolIntArray indices;

    PoolVector3Array collision_faces;

    size_t data_index;
    size_t indices_index;
  };

 public:
  enum class State { UNUSED, BUILDING, ACTIVE };

  Chunk();
  virtual ~Chunk();

  void set_noise(Ref<OpenSimplexNoise> noise);

  void build_terrain();
  void update_tree();
  void unload();

  void lock();
  void unlock();

  Vector3 position;
  bool empty;

  void set_size(size_t size);
  void set_world_size(double world_size);

  State get_state();
  void set_state(State s);

  void set_space_rid(RID space_rid);
  void set_scenario_rid(RID scenario_rid);


  /**
   * @brief Uses the physics server to create a static body and shape for the
   * chunk.
   */
  void init_physics_body();
  void clear_physics_body();

  /**
   * @brief Uses the VisualServer to render the mesh in the world.
   */
  void init_visual_instance();

  void clear_visual_instance();

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

  MeshData _mesh_data;

  Mutex *_lock;
  Mutex *_state_lock;
  State _state;

  Ref<SpatialMaterial> _spatial_material;

  RID _shape_rid;
  RID _body_rid;

  RID _visual_instance;
  RID _mesh_rid;

  RID _space_rid;
  RID _scenario_rid;
};
}  // namespace godot
