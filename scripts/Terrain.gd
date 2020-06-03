tool
extends MeshInstance

var noise: OpenSimplexNoise = OpenSimplexNoise.new()
var surface: SurfaceTool = SurfaceTool.new()
var heightmap: Array = []

var static_body: StaticBody = StaticBody.new()
var shape_owner_id: int = 0

export var cell_size: float = 1
export var cells: int = 64
export var height: float = 16

export var terrain_seed: int = 0
export var random_seed: bool = false

export var regenerate: bool = false setget set_regenerate
export var clear: bool = false setget set_clear

export var uv_scale: float = 1
export var mat_ground: Material = SpatialMaterial.new()

export var generate_on_startup = true

var cell_size_h: float = cell_size / 2.0
var size_h = cell_size * cells / 2
var size = cell_size * cells

# Called when the node enters the scene tree for the first time.
func _ready():
	var mgs = mat_ground as SpatialMaterial
	mgs.albedo_color = Color(0.5, 0.3, 0, 1)
	if not Engine.editor_hint:
		add_child(static_body)
		shape_owner_id = static_body.create_shape_owner(self)
		if generate_on_startup:
			generate()
	
func generate():
	# update cached properties
	cell_size_h = cell_size / 2.0
	size_h = cell_size * cells / 2
	size = cell_size * cells
	
	if random_seed:
		terrain_seed = randi()
	noise.seed = terrain_seed
	
	# compute the heightmap
	heightmap = []
	for x in range(cells + 1):
		heightmap.append([])
		for z in range(cells + 1):
			heightmap[x].append(compute_height(x * cell_size, z * cell_size))
	
	surface.clear()
	surface.begin(Mesh.PRIMITIVE_TRIANGLES)
	for x in range(cells):
		for z in range(cells):
			generate_quad(x * cell_size, z * cell_size)
	
	surface.generate_normals()
	surface.index()
	mesh = surface.commit()
	if not Engine.editor_hint:
		generate_collision()

func generate_quad(x: float, z: float):
	surface.add_normal(Vector3(0, 1, 0))
	surface.set_material(mat_ground)
	
	surface.add_uv(Vector2((x + 1) / size * uv_scale, (z + 1) / size * uv_scale))
	surface.add_vertex(Vector3(x + cell_size_h - size_h, heightmap[x + 1][z + 1], z + cell_size_h - size_h))
	
	surface.add_uv(Vector2(x / size * uv_scale, z / size * uv_scale))
	surface.add_vertex(Vector3(x - cell_size_h - size_h, heightmap[x][z], z - cell_size_h - size_h))
	
	surface.add_uv(Vector2((x + 1) / size * uv_scale, z / size * uv_scale))
	surface.add_vertex(Vector3(x + cell_size_h - size_h, heightmap[x + 1][z], z - cell_size_h - size_h))
	
	
	surface.add_uv(Vector2(x / size * uv_scale, z / size * uv_scale))
	surface.add_vertex(Vector3(x - cell_size_h - size_h, heightmap[x][z], z - cell_size_h - size_h))
	
	surface.add_uv(Vector2((x + 1) / size * uv_scale, (z + 1) / size * uv_scale))
	surface.add_vertex(Vector3(x + cell_size_h - size_h, heightmap[x + 1][z + 1], z + cell_size_h - size_h))
	
	surface.add_uv(Vector2(x / size * uv_scale, (z + 1) / size * uv_scale))
	surface.add_vertex(Vector3(x - cell_size_h - size_h, heightmap[x][z + 1], z + cell_size_h - size_h))

func compute_height(x:float, z:float):
	var d = 1 - min(Vector2(x - size_h, z - size_h).length() / size_h, 1.0)
	return d * (height / 2 + noise.get_noise_2d(x, z) * height + noise.get_noise_2d(x * 2, z * 2) * height / 2)
	
func generate_collision():
	var collision = mesh.create_trimesh_shape()
	static_body.shape_owner_clear_shapes(shape_owner_id)
	static_body.shape_owner_add_shape(shape_owner_id, collision)
	
func set_regenerate(val: bool):
	generate()

func set_clear(val: bool):
	surface.clear()
	mesh = surface.commit()
