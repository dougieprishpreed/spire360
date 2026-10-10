extends SceneTree
var frames = 0
var scene_path = ""
var out = ""
func _init():
	var args = OS.get_cmdline_user_args()
	scene_path = args[0]
	out = args[1]
	root.size = Vector2i(1920, 1080)
	var ps = load(scene_path)
	if ps == null:
		print("LOAD FAILED ", scene_path)
		quit(1)
		return
	var inst = ps.instantiate()
	root.add_child(inst)
func _process(_delta):
	frames += 1
	if frames == 5:
		var img = root.get_texture().get_image()
		img.save_png(out)
		print("SAVED ", out, " ", img.get_size())
		quit()
	return false
