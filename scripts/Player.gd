extends KinematicBody

# Declare member variables here. Examples:
# var a = 2
# var b = "text"
var velocity: Vector3 = Vector3(0, 0, 0)
var motion: Vector3 = Vector3(0, 0, 0)

onready var camera: Camera = get_node("Camera")

var yaw = 0
var pitch = 0

export var walking_speed: float = 8
export var walking_force: float = 20
export var camera_sensitivity: float = 1
export var jump_power: float = 5

# Called when the node enters the scene tree for the first time.
func _ready():
	Input.set_mouse_mode(Input.MOUSE_MODE_CAPTURED)
	
func _input(event):
	if event is InputEventMouseMotion:
		yaw -= event.relative.x / 500 * camera_sensitivity
		pitch -= event.relative.y / 500 * camera_sensitivity
		pitch = clamp(pitch, -1.3, 1.3)
		rotation.y = yaw
		camera.rotation.x = pitch

# Called every frame. 'delta' is the elapsed time since the previous frame.
#func _process(delta):
	#pass


func _physics_process(delta):
	# compute the players target motion
	motion.x = 0
	motion.z = 0
	motion.y = 0
	if is_on_floor():
		if Input.is_action_pressed("move_forward"):
			motion += transform.basis[0]
		if Input.is_action_pressed("move_backward"):
			motion -= transform.basis[0]
		if Input.is_action_pressed("move_right"):
			motion += transform.basis[2]
		if Input.is_action_pressed("move_left"):
			motion -= transform.basis[2]
		
		motion = motion.normalized()
		motion.y = 0
		motion *= walking_speed
		
		# compute the correcting force the player wants to apply
		var horizontal_vel = velocity
		horizontal_vel.y = 0
		motion = (motion - horizontal_vel) * walking_force / 2
		# cap the players horizontal control
		var mlen = motion.length()
		if mlen > walking_force:
			motion *= walking_force / mlen
	
	# jumping
	if Input.is_action_just_pressed("jump"):
		velocity.y += jump_power
	velocity.y -= 9.81 * delta
	velocity += motion * delta
	velocity = move_and_slide(velocity, Vector3.UP, true)
