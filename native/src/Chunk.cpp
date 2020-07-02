#include "Chunk.h"

#include <ArrayMesh.hpp>
#include <CSGBox.hpp>
#include <Engine.hpp>
#include <PhysicsServer.hpp>
#include <ResourceLoader.hpp>
#include <Shape.hpp>
#include <VisualServer.hpp>
#include <World.hpp>
#include <chrono>

namespace godot {

Chunk::Chunk() : _world_size(16), _size(16), _state(State::UNUSED) {
  set_noise(OpenSimplexNoise::_new());
  _spatial_material = Ref<SpatialMaterial>(SpatialMaterial::_new());
  _lock = Mutex::_new();
  _state_lock = Mutex::_new();
}

Chunk::~Chunk() {
  _lock->free();
  _state_lock->free();
  clear_physics_body();
  clear_visual_instance();
}

Chunk::State Chunk::get_state() {
  State s;
  _state_lock->lock();
  s = _state;
  _state_lock->unlock();
  return s;
}

void Chunk::set_state(State s) {
  _state_lock->lock();
  _state = s;
  _state_lock->unlock();
}

void Chunk::set_noise(Ref<OpenSimplexNoise> noise) { _noise = noise; }

void Chunk::build_terrain() {
  using namespace std::chrono;

  //  time_point start = high_resolution_clock::now();

  constexpr double TERRAIN_SCALE = 20;
  double voxel_size = _world_size / _size;
  double half_size = _world_size / 2;

  size_t num_voxels = _size * _size * _size;
  _voxels.resize(_world_size * _world_size * _world_size);

  // compute the terrain height
  std::vector<double> _heights(_world_size * _world_size);
  for (size_t z = 0; z < _world_size; ++z) {
    for (size_t x = 0; x < _world_size; ++x) {
      _heights[x + z * _size] =
          _noise->get_noise_2d(position.x + x * voxel_size - half_size,
                               position.z + z * voxel_size - half_size);
    }
  }

  // Initialize the voxels
  for (size_t y = 0; y < _size; ++y) {
    double world_y = position.y + y * voxel_size - half_size;
    for (size_t z = 0; z < _size; ++z) {
      for (size_t x = 0; x < _size; ++x) {
        voxel(x, y, z) = world_y < _heights[x + z * _size] * TERRAIN_SCALE;
      }
    }
  }

  // Generate the faces
  _mesh_data.vertices.resize(num_voxels / 2 * 6 * 4);
  _mesh_data.normals.resize(num_voxels / 2 * 6 * 4);
  _mesh_data.uvs.resize(num_voxels / 2 * 6 * 4);

  _mesh_data.indices.resize(num_voxels / 2 * 6 * 6);
  _mesh_data.collision_faces.resize(num_voxels / 2 * 6 * 6);

  _mesh_data.data_index = 0;
  _mesh_data.indices_index = 0;

  for (size_t y = 0; y < _size; ++y) {
    for (size_t z = 0; z < _size; ++z) {
      for (size_t x = 0; x < _size; ++x) {
        if (!voxel(x, y, z)) {
          // Air voxels never need geometry
          continue;
        }
        // the smallest corner of the voxel
        double wx = x * voxel_size - half_size;
        double wy = y * voxel_size - half_size;
        double wz = z * voxel_size - half_size;

        // Check the face above
        if (!voxel_or_false(x, y + 1, z)) {
          create_top_face(wx, wy, wz, voxel_size, &_mesh_data);
        }
        if (!voxel_or_false(x, y - 1, z)) {
          create_bottom_face(wx, wy, wz, voxel_size, &_mesh_data);
        }
        if (!voxel_or_false(x + 1, y, z)) {
          create_right_face(wx, wy, wz, voxel_size, &_mesh_data);
        }
        if (!voxel_or_false(x - 1, y, z)) {
          create_left_face(wx, wy, wz, voxel_size, &_mesh_data);
        }
        if (!voxel_or_false(x, y, z + 1)) {
          create_back_face(wx, wy, wz, voxel_size, &_mesh_data);
        }
        if (!voxel_or_false(x, y, z - 1)) {
          create_front_face(wx, wy, wz, voxel_size, &_mesh_data);
        }
      }
    }
  }

  //  time_point pre_engine_upload = high_resolution_clock::now();
  if (_mesh_data.indices_index > 0) {
    // TODO: This currently requires the majority of the time (about 200 of 206
    // ms). That appears to be connected to the creation of the mesh
    // (potentially uploads to the graphics card?).
    empty = false;

    _mesh_data.vertices.resize(_mesh_data.data_index);
    _mesh_data.normals.resize(_mesh_data.data_index);
    _mesh_data.uvs.resize(_mesh_data.data_index);

    _mesh_data.indices.resize(_mesh_data.indices_index);
    _mesh_data.collision_faces.resize(_mesh_data.indices_index);

  } else {
    empty = true;
  }

  //  time_point end = high_resolution_clock::now();
  //  duration delta = end - start;
  //  double msecs = duration_cast<microseconds>(delta).count() / 1000.0;
  //  double msecs_engine =
  //      duration_cast<microseconds>(pre_engine_upload - start).count() /
  //      1000.0;
  //  Godot::print("Mesh building took " + String::num(msecs, 3) + " ms with " +
  //               String::num(_mesh_data.indices_index) + " indices. " +
  //               String::num(msecs_engine) + " of that was pre mesh upload.");
}

void Chunk::update_tree() {
  //  using namespace std::chrono;
  //  time_point start = high_resolution_clock::now();

  if (_mesh_data.indices_index > 0) {
    empty = false;
    init_physics_body();
    init_visual_instance();
  } else {
    empty = true;
    clear_visual_instance();
    clear_physics_body();
  }

  //  time_point end = high_resolution_clock::now();
  //  duration delta = end - start;
  //  double msecs = duration_cast<microseconds>(delta).count() / 1000.0;
  //  Godot::print("Adding the instances took " + String::num(msecs, 3) +
  //               " ms with " + String::num(_mesh_data.indices_index) +
  //               " indices. ");
}

void Chunk::unload() {
  clear_visual_instance();
  clear_physics_body();
}

void Chunk::create_top_face(double x, double y, double z, double size,
                            MeshData *data) {
  // create a new face above the current voxel
  size_t v_idx = data->data_index;
  size_t i_idx = data->indices_index;

  // bottom left
  data->vertices.set(v_idx + 0, Vector3(x, y + size, z));
  // bottom right
  data->vertices.set(v_idx + 1, Vector3(x + size, y + size, z));
  // top left
  data->vertices.set(v_idx + 2, Vector3(x, y + size, z + size));
  // top right
  data->vertices.set(v_idx + 3, Vector3(x + size, y + size, z + size));

  data->normals.set(v_idx + 0, Vector3(0, 1, 0));
  data->normals.set(v_idx + 1, Vector3(0, 1, 0));
  data->normals.set(v_idx + 2, Vector3(0, 1, 0));
  data->normals.set(v_idx + 3, Vector3(0, 1, 0));

  data->uvs.set(v_idx + 0, Vector2(0, 0));
  data->uvs.set(v_idx + 1, Vector2(1, 0));
  data->uvs.set(v_idx + 2, Vector2(0, 1));
  data->uvs.set(v_idx + 3, Vector2(1, 1));

  data->indices.set(i_idx + 0, v_idx + 1);
  data->indices.set(i_idx + 1, v_idx + 2);
  data->indices.set(i_idx + 2, v_idx + 0);

  data->indices.set(i_idx + 3, v_idx + 1);
  data->indices.set(i_idx + 4, v_idx + 3);
  data->indices.set(i_idx + 5, v_idx + 2);

  data->collision_faces.set(i_idx + 0, Vector3(x + size, y + size, z));
  data->collision_faces.set(i_idx + 1, Vector3(x, y + size, z + size));
  data->collision_faces.set(i_idx + 2, Vector3(x, y + size, z));

  data->collision_faces.set(i_idx + 3, Vector3(x + size, y + size, z));
  data->collision_faces.set(i_idx + 4, Vector3(x + size, y + size, z + size));
  data->collision_faces.set(i_idx + 5, Vector3(x, y + size, z + size));

  data->data_index += 4;
  data->indices_index += 6;
}

void Chunk::create_bottom_face(double x, double y, double z, double size,
                               MeshData *data) {
  // create a new face above the current voxel
  size_t v_idx = data->data_index;
  size_t i_idx = data->indices_index;

  // bottom left
  data->vertices.set(v_idx + 0, Vector3(x, y, z));
  // bottom right
  data->vertices.set(v_idx + 1, Vector3(x + size, y, z));
  // top left
  data->vertices.set(v_idx + 2, Vector3(x, y, z + size));
  // top right
  data->vertices.set(v_idx + 3, Vector3(x + size, y, z + size));

  data->normals.set(v_idx + 0, Vector3(0, -1, 0));
  data->normals.set(v_idx + 1, Vector3(0, -1, 0));
  data->normals.set(v_idx + 2, Vector3(0, -1, 0));
  data->normals.set(v_idx + 3, Vector3(0, -1, 0));

  data->uvs.set(v_idx + 0, Vector2(0, 0));
  data->uvs.set(v_idx + 1, Vector2(1, 0));
  data->uvs.set(v_idx + 2, Vector2(0, 1));
  data->uvs.set(v_idx + 3, Vector2(1, 1));

  data->indices.set(i_idx + 0, v_idx + 0);
  data->indices.set(i_idx + 1, v_idx + 2);
  data->indices.set(i_idx + 2, v_idx + 1);

  data->indices.set(i_idx + 3, v_idx + 2);
  data->indices.set(i_idx + 4, v_idx + 3);
  data->indices.set(i_idx + 5, v_idx + 1);

  data->collision_faces.set(i_idx + 0, Vector3(x, y, z));
  data->collision_faces.set(i_idx + 1, Vector3(x + size, y, z));
  data->collision_faces.set(i_idx + 2, Vector3(x, y, z + size));

  data->collision_faces.set(i_idx + 3, Vector3(x + size, y, z));
  data->collision_faces.set(i_idx + 4, Vector3(x, y, z + size));
  data->collision_faces.set(i_idx + 5, Vector3(x + size, y, z + size));

  data->data_index += 4;
  data->indices_index += 6;
}

void Chunk::create_left_face(double x, double y, double z, double size,
                             MeshData *data) {
  // create a new face above the current voxel
  size_t v_idx = data->data_index;
  size_t i_idx = data->indices_index;

  // bottom left
  data->vertices.set(v_idx + 0, Vector3(x, y, z));
  // bottom right
  data->vertices.set(v_idx + 1, Vector3(x, y, z + size));
  // top left
  data->vertices.set(v_idx + 2, Vector3(x, y + size, z));
  // top right
  data->vertices.set(v_idx + 3, Vector3(x, y + size, z + size));

  data->normals.set(v_idx + 0, Vector3(-1, 0, 0));
  data->normals.set(v_idx + 1, Vector3(-1, 0, 0));
  data->normals.set(v_idx + 2, Vector3(-1, 0, 0));
  data->normals.set(v_idx + 3, Vector3(-1, 0, 0));

  data->uvs.set(v_idx + 0, Vector2(0, 0));
  data->uvs.set(v_idx + 1, Vector2(1, 0));
  data->uvs.set(v_idx + 2, Vector2(0, 1));
  data->uvs.set(v_idx + 3, Vector2(1, 1));

  data->indices.set(i_idx + 0, v_idx + 0);
  data->indices.set(i_idx + 1, v_idx + 2);
  data->indices.set(i_idx + 2, v_idx + 1);

  data->indices.set(i_idx + 3, v_idx + 2);
  data->indices.set(i_idx + 4, v_idx + 3);
  data->indices.set(i_idx + 5, v_idx + 1);

  data->collision_faces.set(i_idx + 0, Vector3(x, y, z));
  data->collision_faces.set(i_idx + 1, Vector3(x, y, z + size));
  data->collision_faces.set(i_idx + 2, Vector3(x, y + size, z));

  data->collision_faces.set(i_idx + 3, Vector3(x, y + size, z));
  data->collision_faces.set(i_idx + 4, Vector3(x, y, z + size));
  data->collision_faces.set(i_idx + 5, Vector3(x, y + size, z + size));

  data->data_index += 4;
  data->indices_index += 6;
}

void Chunk::create_right_face(double x, double y, double z, double size,
                              MeshData *data) {
  // create a new face above the current voxel
  size_t v_idx = data->data_index;
  size_t i_idx = data->indices_index;

  // bottom left
  data->vertices.set(v_idx + 0, Vector3(x + size, y, z));
  // bottom right
  data->vertices.set(v_idx + 1, Vector3(x + size, y, z + size));
  // top left
  data->vertices.set(v_idx + 2, Vector3(x + size, y + size, z));
  // top right
  data->vertices.set(v_idx + 3, Vector3(x + size, y + size, z + size));

  data->normals.set(v_idx + 0, Vector3(1, 0, 0));
  data->normals.set(v_idx + 1, Vector3(1, 0, 0));
  data->normals.set(v_idx + 2, Vector3(1, 0, 0));
  data->normals.set(v_idx + 3, Vector3(1, 0, 0));

  data->uvs.set(v_idx + 0, Vector2(0, 0));
  data->uvs.set(v_idx + 1, Vector2(1, 0));
  data->uvs.set(v_idx + 2, Vector2(0, 1));
  data->uvs.set(v_idx + 3, Vector2(1, 1));

  data->indices.set(i_idx + 0, v_idx + 1);
  data->indices.set(i_idx + 1, v_idx + 2);
  data->indices.set(i_idx + 2, v_idx + 0);

  data->indices.set(i_idx + 3, v_idx + 1);
  data->indices.set(i_idx + 4, v_idx + 3);
  data->indices.set(i_idx + 5, v_idx + 2);

  data->collision_faces.set(i_idx + 0, Vector3(x + size, y, z));
  data->collision_faces.set(i_idx + 1, Vector3(x + size, y, z + size));
  data->collision_faces.set(i_idx + 2, Vector3(x + size, y + size, z));

  data->collision_faces.set(i_idx + 3, Vector3(x + size, y + size, z));
  data->collision_faces.set(i_idx + 4, Vector3(x + size, y, z + size));
  data->collision_faces.set(i_idx + 5, Vector3(x + size, y + size, z + size));

  data->data_index += 4;
  data->indices_index += 6;
}

void Chunk::create_front_face(double x, double y, double z, double size,
                              MeshData *data) {
  // create a new face above the current voxel
  size_t v_idx = data->data_index;
  size_t i_idx = data->indices_index;

  // bottom left
  data->vertices.set(v_idx + 0, Vector3(x, y, z));
  // bottom right
  data->vertices.set(v_idx + 1, Vector3(x + size, y, z));
  // top left
  data->vertices.set(v_idx + 2, Vector3(x, y + size, z));
  // top right
  data->vertices.set(v_idx + 3, Vector3(x + size, y + size, z));

  data->normals.set(v_idx + 0, Vector3(0, 0, -1));
  data->normals.set(v_idx + 1, Vector3(0, 0, -1));
  data->normals.set(v_idx + 2, Vector3(0, 0, -1));
  data->normals.set(v_idx + 3, Vector3(0, 0, -1));

  data->uvs.set(v_idx + 0, Vector2(0, 0));
  data->uvs.set(v_idx + 1, Vector2(1, 0));
  data->uvs.set(v_idx + 2, Vector2(0, 1));
  data->uvs.set(v_idx + 3, Vector2(1, 1));

  data->indices.set(i_idx + 0, v_idx + 1);
  data->indices.set(i_idx + 1, v_idx + 2);
  data->indices.set(i_idx + 2, v_idx + 0);

  data->indices.set(i_idx + 3, v_idx + 1);
  data->indices.set(i_idx + 4, v_idx + 3);
  data->indices.set(i_idx + 5, v_idx + 2);

  data->collision_faces.set(i_idx + 0, Vector3(x, y, z));
  data->collision_faces.set(i_idx + 1, Vector3(x + size, y, z));
  data->collision_faces.set(i_idx + 2, Vector3(x, y + size, z));

  data->collision_faces.set(i_idx + 3, Vector3(x + size, y, z));
  data->collision_faces.set(i_idx + 4, Vector3(x, y + size, z));
  data->collision_faces.set(i_idx + 5, Vector3(x + size, y + size, z));

  data->data_index += 4;
  data->indices_index += 6;
}

void Chunk::create_back_face(double x, double y, double z, double size,
                             MeshData *data) {
  // create a new face above the current voxel
  size_t v_idx = data->data_index;
  size_t i_idx = data->indices_index;

  // bottom left
  data->vertices.set(v_idx + 0, Vector3(x, y, z + size));
  // bottom right
  data->vertices.set(v_idx + 1, Vector3(x + size, y, z + size));
  // top left
  data->vertices.set(v_idx + 2, Vector3(x, y + size, z + size));
  // top right
  data->vertices.set(v_idx + 3, Vector3(x + size, y + size, z + size));

  data->normals.set(v_idx + 0, Vector3(0, 0, 1));
  data->normals.set(v_idx + 1, Vector3(0, 0, 1));
  data->normals.set(v_idx + 2, Vector3(0, 0, 1));
  data->normals.set(v_idx + 3, Vector3(0, 0, 1));

  data->uvs.set(v_idx + 0, Vector2(0, 0));
  data->uvs.set(v_idx + 1, Vector2(1, 0));
  data->uvs.set(v_idx + 2, Vector2(0, 1));
  data->uvs.set(v_idx + 3, Vector2(1, 1));

  data->indices.set(i_idx + 0, v_idx + 0);
  data->indices.set(i_idx + 1, v_idx + 2);
  data->indices.set(i_idx + 2, v_idx + 1);

  data->indices.set(i_idx + 3, v_idx + 2);
  data->indices.set(i_idx + 4, v_idx + 3);
  data->indices.set(i_idx + 5, v_idx + 1);

  data->collision_faces.set(i_idx + 0, Vector3(x, y, z + size));
  data->collision_faces.set(i_idx + 1, Vector3(x + size, y, z + size));
  data->collision_faces.set(i_idx + 2, Vector3(x, y + size, z + size));

  data->collision_faces.set(i_idx + 3, Vector3(x + size, y, z + size));
  data->collision_faces.set(i_idx + 4, Vector3(x, y + size, z + size));
  data->collision_faces.set(i_idx + 5, Vector3(x + size, y + size, z + size));

  data->data_index += 4;
  data->indices_index += 6;
}

std::vector<bool>::reference Chunk::voxel(size_t x, size_t y, size_t z) {
  return _voxels[x + z * _world_size + y * _world_size * _world_size];
}

bool Chunk::voxel_or_false(int64_t x, int64_t y, int64_t z) {
  if (x < 0 || y < 0 || z < 0 || x >= _world_size || y >= _world_size ||
      z >= _world_size) {
    return false;
  }
  return voxel(x, y, z);
}

void Chunk::lock() { _lock->lock(); }

void Chunk::unlock() { _lock->unlock(); }

void Chunk::set_size(size_t size) { _size = size; }
void Chunk::set_world_size(double world_size) { _world_size = world_size; }

void Chunk::set_space_rid(RID space_rid) { _space_rid = space_rid; }

void Chunk::set_scenario_rid(RID scenario_rid) { _scenario_rid = scenario_rid; }

void Chunk::init_physics_body() {
  PhysicsServer *physics = PhysicsServer::get_singleton();
  clear_physics_body();
  _body_rid = physics->body_create(PhysicsServer::BodyMode::BODY_MODE_STATIC);
  physics->body_set_collision_layer(_body_rid, 1);
  physics->body_set_collision_mask(_body_rid, 1);
  physics->body_set_space(_body_rid, _space_rid);

  Transform shape_transform;
  shape_transform.origin = position;

  _shape_rid =
      physics->shape_create(PhysicsServer::ShapeType::SHAPE_CONCAVE_POLYGON);
  physics->shape_set_data(_shape_rid, _mesh_data.collision_faces);
  physics->body_add_shape(_body_rid, _shape_rid, shape_transform);
}

void Chunk::clear_physics_body() {
  PhysicsServer *physics = PhysicsServer::get_singleton();
  if (_body_rid.is_valid()) {
    physics->free_rid(_body_rid);
    _body_rid = RID();
  }
  if (_shape_rid.is_valid()) {
    physics->free_rid(_shape_rid);
    _shape_rid = RID();
  }
}

void Chunk::init_visual_instance() {
  VisualServer *visual = VisualServer::get_singleton();
  clear_visual_instance();

  Array arrays;
  arrays.resize(ArrayMesh::ARRAY_MAX);
  arrays[ArrayMesh::ARRAY_VERTEX] = _mesh_data.vertices;
  arrays[ArrayMesh::ARRAY_NORMAL] = _mesh_data.normals;
  arrays[ArrayMesh::ARRAY_TEX_UV] = _mesh_data.uvs;
  arrays[ArrayMesh::ARRAY_INDEX] = _mesh_data.indices;

  _mesh_rid = visual->mesh_create();
  visual->mesh_add_surface_from_arrays(
      _mesh_rid, VisualServer::PRIMITIVE_TRIANGLES, arrays);
  visual->mesh_surface_set_material(_mesh_rid, 0, _spatial_material->get_rid());

  _visual_instance = visual->instance_create();
  visual->instance_set_scenario(_visual_instance, _scenario_rid);
  visual->instance_set_base(_visual_instance, _mesh_rid);

  Transform visual_transform;
  visual_transform.origin = position;
  visual->instance_set_transform(_visual_instance, visual_transform);
}

void Chunk::clear_visual_instance() {
  VisualServer *visual = VisualServer::get_singleton();
  if (_mesh_rid.is_valid()) {
    visual->free_rid(_mesh_rid);
    _mesh_rid = RID();
  }
  if (_visual_instance.is_valid()) {
    visual->free_rid(_visual_instance);
    _visual_instance = RID();
  }
}
}  // namespace godot
