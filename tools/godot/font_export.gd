# Exports a Godot font as the engine needs it: every glyph of its cmap as
# MSDF atlas pages (rendered by Godot itself, so identical to the game's
# own glyph cache), metrics, and the kerning and ligatures Godot's shaper
# (HarfBuzz) applies. Run by converter/convert.py:
#
#   godot --headless --path <empty project> --script font_export.gd -- <font> <out_dir> [<chars file>]
#
# <chars file>: UTF-8 text; only these characters' glyphs are exported (the
# converter passes what the chosen languages' text uses). Without it, the
# whole cmap (gigabytes for the game's CJK fonts).
# <font>: a .fontdata (imported FontFile) path, or "default" for Godot's
# default theme font. Fonts imported without MSDF are exported as MSDF
# (msdf_size 48, pixel range 16) and marked "msdf_substitute".
# Output: <out_dir>/font.json and <out_dir>/page_<n>.png (RGBA8).
extends SceneTree

const PAIR_SCRIPTS = [[0x20, 0x24F], [0x370, 0x3FF], [0x400, 0x52F], [0x1E00, 0x1EFF], [0x2000, 0x206F], [0x20A0, 0x20CF]]

func in_pair_set(c):
	if (c >= 0x300 and c <= 0x36F) or (c >= 0x483 and c <= 0x489):
		return false # combining marks: shaped as clusters, not pairs
	for r in PAIR_SCRIPTS:
		if c >= r[0] and c <= r[1]:
			return true
	return false

func _init():
	var args = OS.get_cmdline_user_args()
	var src = args[0]
	var out_dir = args[1]
	var keep = null
	if args.size() > 2:
		keep = {}
		var text = FileAccess.get_file_as_string(args[2])
		for i in text.length():
			keep[text.unicode_at(i)] = true
	var orig
	if src == "default":
		orig = ThemeDB.get_default_theme().default_font
	else:
		orig = load(src)
	if orig == null:
		printerr("cannot load ", src)
		quit(1)
		return
	var f = orig
	var substitute = false
	if not orig.multichannel_signed_distance_field:
		f = FontFile.new()
		f.data = orig.data
		f.multichannel_signed_distance_field = true
		f.msdf_pixel_range = 16
		f.msdf_size = 48
		substitute = true
	var base = f.msdf_size
	var sz = Vector2i(base, 0)
	var chars = f.get_supported_chars()
	var cmap = []
	var gids = {}
	for i in chars.length():
		var cp = chars.unicode_at(i)
		if keep != null and not keep.has(cp):
			continue
		var g = f.get_glyph_index(base, cp, 0)
		if g != 0:
			cmap.append([cp, g])
			gids[g] = true

	# Shaping: base advances, kerning and ligatures, measured at twice the
	# base size (subpixel positions, no rounding) and halved.
	var ts = TextServerManager.get_primary_interface()
	var rids = f.get_rids()
	var probe = 2 * base
	var adv = {}
	var pair_chars = []
	for e in cmap:
		if in_pair_set(e[0]):
			pair_chars.append(e[0])
	for cp in pair_chars:
		var r = ts.create_shaped_text()
		ts.shaped_text_add_string(r, String.chr(cp), rids, probe)
		var gl = ts.shaped_text_get_glyphs(r)
		if gl.size() == 1:
			adv[cp] = gl[0]["advance"] / 2.0
		ts.free_rid(r)
	var kern = []
	var lig = []
	for c1 in pair_chars:
		if not adv.has(c1):
			continue
		# c1 c2 c1 c3 ... : each c1 carries kern(c1, next), each ck kern(ck, c1).
		var s = ""
		var order = []
		for c2 in pair_chars:
			if adv.has(c2):
				s += String.chr(c1) + String.chr(c2)
				order.append(c2)
		var r = ts.create_shaped_text()
		ts.shaped_text_add_string(r, s, rids, probe)
		for g in ts.shaped_text_get_glyphs(r):
			var st = g["start"]
			var cp = s.unicode_at(st)
			if g["end"] - st == 2 and g["count"] == 1 and st + 1 < s.length():
				lig.append([cp, s.unicode_at(st + 1), g["index"]])
				gids[g["index"]] = true
				continue
			if g["end"] - st != 1 or st + 1 >= s.length():
				continue
			var nxt = s.unicode_at(st + 1)
			var k = g["advance"] / 2.0 - adv[cp]
			if abs(k) > 0.0001:
				kern.append([cp, nxt, k])
		ts.free_rid(r)
	# The same pair can appear twice (c1 c1); keep one.
	var seen = {}
	var kern_u = []
	for k in kern:
		var key = str(k[0]) + "," + str(k[1])
		if not seen.has(key):
			seen[key] = true
			kern_u.append(k)
	var lig_u = []
	seen = {}
	for l in lig:
		var key = str(l[0]) + "," + str(l[1])
		if not seen.has(key):
			seen[key] = true
			lig_u.append(l)

	# Glyphs: render everything the cmap and the ligatures reach.
	var glyphs = []
	for g in gids.keys():
		f.render_glyph(0, sz, g)
	for g in gids.keys():
		var a = f.get_glyph_advance(0, base, g)
		var o = f.get_glyph_offset(0, sz, g)
		var s2 = f.get_glyph_size(0, sz, g)
		var uv = f.get_glyph_uv_rect(0, sz, g)
		glyphs.append([g, f.get_glyph_texture_idx(0, sz, g), uv.position.x, uv.position.y, uv.size.x, uv.size.y, o.x, o.y, s2.x, s2.y, a.x])
	var pages = f.get_texture_count(0, sz)
	for i in pages:
		f.get_texture_image(0, sz, i).save_png(out_dir.path_join("page_%d.png" % i))
	var info = {
		"name": f.get_font_name(), "style": f.get_font_style_name(),
		"msdf": true, "msdf_substitute": substitute, "px_range": f.msdf_pixel_range, "base_size": base,
		"ascent": f.get_cache_ascent(0, base), "descent": f.get_cache_descent(0, base),
		"underline_position": f.get_cache_underline_position(0, base),
		"underline_thickness": f.get_cache_underline_thickness(0, base),
		"pages": pages, "glyphs": glyphs, "cmap": cmap, "kern": kern_u, "lig": lig_u,
		"advance": adv.keys().map(func(c): return [c, adv[c]]),
	}
	var fa = FileAccess.open(out_dir.path_join("font.json"), FileAccess.WRITE)
	fa.store_string(JSON.stringify(info))
	fa.close()
	print("font ", f.get_font_name(), " ", f.get_font_style_name(), ": ", glyphs.size(), " glyphs, ", pages, " pages, ", kern_u.size(), " kerning pairs, ", lig_u.size(), " ligatures")
	quit()
