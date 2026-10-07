# Dumps Godot's default Theme as JSON (input of tools/gen_default_theme.py).
# godot --headless --path <empty project> --script dump_default_theme.gd -- out.json
extends SceneTree
func sb(s):
	if s == null: return null
	var d = {"class": s.get_class()}
	for p in s.get_property_list():
		if p.usage & PROPERTY_USAGE_STORAGE:
			var v = s.get(p.name)
			if typeof(v) == TYPE_OBJECT: continue
			d[p.name] = var_to_str(v)
	return d
func _init():
	var th = ThemeDB.get_default_theme()
	var out = {"default_font_size": th.default_font_size, "default_base_scale": th.default_base_scale, "types": {}}
	var f = th.default_font
	out["default_font"] = f.get_class() + " " + str(f.get_font_name())
	for t in th.get_type_list():
		var d = {}
		for n in th.get_color_list(t): d["colors/" + n] = var_to_str(th.get_color(n, t))
		for n in th.get_constant_list(t): d["constants/" + n] = th.get_constant(n, t)
		for n in th.get_font_size_list(t): d["font_sizes/" + n] = th.get_font_size(n, t)
		for n in th.get_font_list(t): d["fonts/" + n] = th.get_font(n, t).get_class()
		for n in th.get_stylebox_list(t): d["styles/" + n] = sb(th.get_stylebox(n, t))
		for n in th.get_icon_list(t): d["icons/" + n] = th.get_icon(n, t).get_size()
		var base = th.get_type_variation_base(t)
		if base != &"": d["base"] = base
		out["types"][t] = d
	var fa = FileAccess.open(OS.get_cmdline_user_args()[0], FileAccess.WRITE)
	fa.store_string(JSON.stringify(out, " ", true))
	fa.close()
	quit()
