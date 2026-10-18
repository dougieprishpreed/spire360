# Bake NoiseTexture2D after Godot's generation thread has finished.
# Inputs contain texture resources and their dependencies, without scripts.
extends SceneTree

func _initialize():
	call_deferred("export_textures")

func export_textures():
	var args = OS.get_cmdline_user_args()
	var jobs = JSON.parse_string(FileAccess.get_file_as_string(args[0]))
	for job in jobs:
		var texture = load(job["source"])
		if texture == null:
			printerr("cannot load procedural texture: ", job["source"])
			quit(1)
			return
		var image = texture.get_image()
		if image == null:
			await texture.changed
			image = texture.get_image()
		if image == null or image.save_png(job["output"]) != OK:
			printerr("cannot save procedural texture: ", job["output"])
			quit(1)
			return
	quit()
