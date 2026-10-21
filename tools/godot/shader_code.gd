# Prints the shader code Godot generates for a VisualShader resource (the
# game keeps some of its canvas shaders as node graphs). Run by
# tools/visual_shader_code.py:
#
#   godot --headless --path <project> --script shader_code.gd -- <res path>
extends SceneTree

func _init():
	var path = OS.get_cmdline_user_args()[0]
	var s = load(path)
	if s == null:
		printerr("cannot load ", path)
		quit(1)
		return
	print("----8<----")
	print(s.get_code())
	quit()
