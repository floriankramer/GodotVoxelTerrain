#include "Terrain.h"

#include <CSGBox.hpp>
#include <Engine.hpp>
#include <Mesh.hpp>
#include <OpenSimplexNoise.hpp>
#include <RandomNumberGenerator.hpp>
#include <Shape.hpp>
#include <SpatialMaterial.hpp>
#include <SurfaceTool.hpp>
#include <World.hpp>
#include <chrono>
#include <thread>

#include "HeightMap.h"
#include "Utils.h"

namespace godot {

void Terrain::_register_methods() {
  register_method("_ready", &Terrain::_ready);
  register_method("_process", &Terrain::_process);
  register_method("process_chunks", &Terrain::process_chunks);

  register_property<Terrain, NodePath>("Player Path", &Terrain::_player_path,
                                       "Player");
  register_property<Terrain, int64_t>("Load Distance", &Terrain::_loaded_radius,
                                      4);
  register_property<Terrain, double>("Chunk Size", &Terrain::_chunk_size, 16);
  register_property<Terrain, size_t>("Chunk Divisions",
                                     &Terrain::_chunk_num_blocks, 16);

  register_property<Terrain, int64_t>("World Floor", &Terrain::_floor, -3);
  register_property<Terrain, int64_t>("World Ceiling", &Terrain::_ceiling, 3);
}

Terrain::Terrain() : Spatial(), _floor(-3), _ceiling(3) {
  _available_chunks = Semaphore::_new();
  _chunks_to_load_mutex = Mutex::_new();
  _loaded_chunks_mutex = Mutex::_new();
  _chunk_pool_mutex = Mutex::_new();
  _noise = Ref<OpenSimplexNoise>(OpenSimplexNoise::_new());

  Ref<RandomNumberGenerator> rng(RandomNumberGenerator::_new());
  rng->randomize();
  _noise->set_seed(rng->randi());
}

Terrain::~Terrain() {
  _available_chunks->free();
  _chunks_to_load_mutex->free();
  _loaded_chunks_mutex->free();
  _chunk_pool_mutex->free();
}

void Terrain::_init() {}

void Terrain::_ready() {
  constexpr int64_t INIT_LOADED_RADIUS = 2;
  for (size_t i = 0; i < std::thread::hardware_concurrency(); ++i) {
    Thread *thread = Thread::_new();
    thread->start(this, "process_chunks");

    _worker_threads.push_back(thread);
  }

  _player = (Spatial *)get_node_or_null(_player_path);
  if (_player == nullptr) {
    Godot::print("Unable to locate the player at " + _player_path);
  }

  // Initialize the terrain
  Vector3 player_pos = _player->get_global_transform().origin;
  int64_t co_x = player_pos.x / _chunk_size;
  int64_t co_z = player_pos.z / _chunk_size;
  for (int64_t y = _floor; y <= _ceiling; y++) {
    for (int64_t x = co_x - INIT_LOADED_RADIUS; x <= co_x + INIT_LOADED_RADIUS;
         x++) {
      for (int64_t z = co_z - INIT_LOADED_RADIUS;
           z <= co_z + INIT_LOADED_RADIUS; z++) {
        ChunkCoord cc{x, y, z};
        if (_chunks.count(cc) == 0) {
          load_chunk_sequential(x, y, z);
        }
      }
    }
  }
}

void Terrain::_process(float delta) {
  using namespace std::chrono;

  //  time_point start_time = high_resolution_clock::now();

  _loaded_chunks_mutex->lock();
  for (size_t i = 0; i < 1 && !_loaded_chunks.empty(); ++i) {
    Chunk *c = _loaded_chunks.back();
    _loaded_chunks.pop_back();

    c->lock();
    ChunkCoord cc{int64_t(std::round(c->position.x / _chunk_size)),
                  int64_t(std::round(c->position.y / _chunk_size)),
                  int64_t(std::round(c->position.z / _chunk_size))};
    if (_chunks.count(cc) > 0 && _chunks[cc] == c) {
      if (!c->empty) {
        c->update_tree();
        c->set_state(Chunk::State::ACTIVE);
      }
    } else {
      c->set_state(Chunk::State::UNUSED);
      // Remove the chunk from the scene
      c->unload();
      // Then readd it to the chunk pool
      _chunk_pool_mutex->lock();
      _chunk_pool.push_back(c);
      _chunk_pool_mutex->unlock();
    }
    c->unlock();
  }
  _loaded_chunks_mutex->unlock();

  Vector3 player_pos = _player->get_global_transform().origin;
  int64_t co_x = player_pos.x / _chunk_size;
  int64_t co_y = player_pos.y / _chunk_size;
  int64_t co_z = player_pos.z / _chunk_size;

  {
    std::vector<ChunkCoord> to_remove;
    for (const std::pair<ChunkCoord, Chunk *> &p : _chunks) {
      if (std::abs(p.first.x - co_x) > 1.5 * _loaded_radius ||
          std::abs(p.first.y - co_y) > 1.5 * _loaded_radius ||
          std::abs(p.first.z - co_z) > 1.5 * _loaded_radius) {
        to_remove.push_back(p.first);
      }
    }
    for (ChunkCoord &c : to_remove) {
      unload_chunk(c.x, c.y, c.z);
    }
  }

  ChunkCoord player_cc{co_x, co_y, co_z};
  if (_chunks.count(player_cc) == 0 && co_y >= _floor && co_y <= _ceiling) {
    load_chunk_sequential(co_x, co_y, co_z);
  }

  for (int64_t y = co_y - _loaded_radius; y <= co_y + _loaded_radius; y++) {
    if (y < _floor || y > _ceiling) {
      continue;
    }
    for (int64_t x = co_x - _loaded_radius; x <= co_x + _loaded_radius; x++) {
      for (int64_t z = co_z - _loaded_radius; z <= co_z + _loaded_radius; z++) {
        ChunkCoord cc{x, y, z};
        if (_chunks.count(cc) == 0) {
          load_chunk_sequential(x, y, z);
        }
      }
    }
  }

  //  time_point end_time = high_resolution_clock::now();
  //  double time_ms = duration_cast<microseconds>(end_time -
  //  start_time).count() / 1000.0; Godot::print("Update time: " +
  //  String::num(time_ms, 3));
}

void Terrain::process_chunks() {
  using namespace std::chrono;
  while (true) {
    _available_chunks->wait();
    _chunks_to_load_mutex->lock();
    if (_chunks_to_load.empty()) {
      _chunks_to_load_mutex->unlock();
      continue;
    }
    Chunk *to_load = _chunks_to_load.back();
    _chunks_to_load.pop_back();
    to_load->set_state(Chunk::State::BUILDING);
    _chunks_to_load_mutex->unlock();

    to_load->lock();
    to_load->build_terrain();
    to_load->unlock();

    _loaded_chunks_mutex->lock();
    _loaded_chunks.push_back(to_load);
    _loaded_chunks_mutex->unlock();
  }
}

void Terrain::load_chunk(int64_t x, int64_t y, int64_t z) {
  ChunkCoord cc{x, y, z};
  Chunk *chunk = acquire_chunk();
  chunk->lock();
  _chunks[cc] = chunk;

  chunk->position = Vector3(x * _chunk_size, y * _chunk_size, z * _chunk_size);
  Transform t;
  t.origin = chunk->position;
  chunk->unlock();

  _chunks_to_load_mutex->lock();
  _chunks_to_load.push_back(chunk);
  _chunks_to_load_mutex->unlock();
  _available_chunks->post();
}

void Terrain::load_chunk_sequential(int64_t x, int64_t y, int64_t z) {
  ChunkCoord cc{x, y, z};
  Chunk *chunk = acquire_chunk();
  chunk->set_state(Chunk::State::BUILDING);
  chunk->lock();
  _chunks[cc] = chunk;

  chunk->position = Vector3(x * _chunk_size, y * _chunk_size, z * _chunk_size);
  Transform t;
  t.origin = chunk->position;

  chunk->build_terrain();

  chunk->update_tree();
  chunk->set_state(Chunk::State::ACTIVE);
  chunk->unlock();
}

void Terrain::unload_chunk(int64_t x, int64_t y, int64_t z) {
  ChunkCoord cc{x, y, z};
  auto it = _chunks.find(cc);
  _chunks.erase(it);

  // Check if the chunk was scheduled for loading and unschedule it
  _chunks_to_load_mutex->lock();
  for (size_t i = 0; i < _chunks_to_load.size(); ++i) {
    if (_chunks_to_load[i] == it->second) {
      std::iter_swap(_chunks_to_load.begin() + i, _chunks_to_load.end() - 1);
      _chunks_to_load.pop_back();
    }
  }
  Chunk::State s = it->second->get_state();
  _chunks_to_load_mutex->unlock();

  if (s == Chunk::State::BUILDING) {
    // The chunk is still being constructed from the time it was loaded
    // abort. The chunk will be readded to the pool once it was fully loaded.
    return;
  }

  // Remove the chunk from the scene
  it->second->unload();

  // The chunk can now be reused

  _chunk_pool_mutex->lock();
  _chunk_pool.push_back(it->second);
  _chunk_pool_mutex->unlock();
}

Chunk *Terrain::acquire_chunk() {
  _chunk_pool_mutex->lock();
  Chunk *chunk = nullptr;
  if (_chunk_pool.size() > 0) {
    chunk = _chunk_pool.back();
    _chunk_pool.pop_back();
  }
  _chunk_pool_mutex->unlock();
  if (chunk == nullptr) {
    RID space_rid = get_world()->get_space();
    RID scenario_rid = get_world()->get_scenario();
    chunk = new Chunk();
    chunk->set_size(_chunk_num_blocks);
    chunk->set_world_size(_chunk_size);
    chunk->set_noise(_noise);
    chunk->set_space_rid(space_rid);
    chunk->set_scenario_rid(scenario_rid);
  }
  return chunk;
}

}  // namespace godot
